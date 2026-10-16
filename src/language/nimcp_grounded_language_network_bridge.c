/**
 * @file nimcp_grounded_language_network_bridge.c
 * @brief Per-network bridges (LNN / cortex-CNN / FNO / ANN).
 * @date 2026-05-05
 *
 * WHAT: Routes the comprehend semantic_vector to each attached
 *       network's forward pass, captures the response magnitude as
 *       a confidence-modulation factor.
 *
 * WHY:  Pre-bridge, only SNN saw language input (via
 *       nimcp_snn_language_bridge). LNN/CNN/FNO/ANN never saw
 *       lexicon vectors and never contributed to comprehend
 *       confidence. This file closes that gap with a single
 *       broadcast point.
 *
 * HOW:  Four attach setters (one per network type), one broadcast
 *       function that calls each connected network's forward, and a
 *       modulation accessor the comprehend hot path consults. All
 *       independently optional — no behavior change when nothing
 *       is wired.
 *
 * SOLID/DRY: Each network gets its own helper that encapsulates the
 *            forward signature differences (LNN takes tensors, CNN
 *            takes raw float arrays, FNO takes mel + writes embedding,
 *            ANN takes a callback). Broadcast iterates them once.
 */

#include "language/nimcp_grounded_language.h"
#include "nimcp_grounded_language_internal.h"

#include "lnn/nimcp_lnn_layer.h"
#include "training/nimcp_cortex_cnn.h"
#include "training/nimcp_fno_layer.h"
#include "utils/tensor/nimcp_tensor.h"

#include "utils/logging/nimcp_logging.h"
#include <math.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

/*=============================================================================
 * Attach setters — symmetric, validate handle, write opaque pointers.
 *===========================================================================*/

void grounded_language_attach_lnn(grounded_language_t* gl, void* lnn_layer) {
    if (!gl) return;
    gl->lnn_layer = lnn_layer;
    if (!lnn_layer) gl->last_lnn_mag = 0.0f;
}

void grounded_language_attach_cortex_cnn(grounded_language_t* gl, void* cnn_proc) {
    if (!gl) return;
    gl->cortex_cnn_proc = cnn_proc;
    if (!cnn_proc) gl->last_cnn_mag = 0.0f;
}

void grounded_language_attach_fno(grounded_language_t* gl, void* fno_proc) {
    if (!gl) return;
    gl->fno_proc = fno_proc;
    if (!fno_proc) gl->last_fno_mag = 0.0f;
}

void grounded_language_attach_ann(grounded_language_t* gl,
                                    gl_ann_predict_fn fn,
                                    void* ctx) {
    if (!gl) return;
    gl->ann_predict_fn = fn;
    gl->ann_ctx        = ctx;
    if (!fn) gl->last_ann_mag = 0.0f;
}

/*=============================================================================
 * Per-network forward helpers — return normalized response magnitude
 * in [0, 1], or -1.0f when not attached / forward failed.
 *
 * Magnitude calc: L2 norm of the output, divided by sqrt(dim) so the
 * scale is dim-independent, then squashed via tanh so values stay
 * bounded. This is a coarse "did the network do anything?" probe —
 * NOT a downstream signal. Real downstream gradients come through
 * the existing brain_learn_vector path.
 *===========================================================================*/

static float _normalize_magnitude(const float* vec, uint32_t dim) {
    if (!vec || dim == 0) return 0.0f;
    double sum_sq = 0.0;
    for (uint32_t i = 0; i < dim; i++) sum_sq += (double)vec[i] * vec[i];
    double norm = sqrt(sum_sq / (double)dim);
    /* tanh squashes to [0, 1) without saturating below ~0.5. */
    return (float)tanh(norm);
}

static float _broadcast_to_lnn(grounded_language_t* gl,
                                 const float* vec, uint32_t dim) {
    if (!gl->lnn_layer) return -1.0f;

    /* LNN takes input tensor of size n_inputs. We don't know the
     * layer's n_inputs from the public API without an accessor —
     * passing the wrong-sized tensor is a bug, not a soft failure.
     * Caller is responsible for matching emb_dim/semantic_dim to the
     * LNN config. We allocate input tensor of [dim], output tensor
     * of [dim] (best-effort — LNN's actual output dim is n_neurons,
     * which may differ; we only read magnitude so size mismatch
     * still gives a usable scalar). */
    uint32_t dims_in[1]  = { dim };
    uint32_t dims_out[1] = { dim };
    nimcp_tensor_t* in  = nimcp_tensor_create(dims_in,  1, NIMCP_DTYPE_F32);
    nimcp_tensor_t* out = nimcp_tensor_create(dims_out, 1, NIMCP_DTYPE_F32);
    if (!in || !out) {
        if (in)  nimcp_tensor_destroy(in);
        if (out) nimcp_tensor_destroy(out);
        return -1.0f;
    }
    memcpy(nimcp_tensor_data(in), vec, dim * sizeof(float));

    int rc = lnn_layer_forward((lnn_layer_t*)gl->lnn_layer, in, out, 0.01f);
    float mag = -1.0f;
    if (rc == 0) {
        mag = _normalize_magnitude((const float*)nimcp_tensor_data_const(out), dim);
    }
    nimcp_tensor_destroy(in);
    nimcp_tensor_destroy(out);
    return mag;
}

static float _broadcast_to_cnn(grounded_language_t* gl,
                                 const float* vec, uint32_t dim) {
    if (!gl->cortex_cnn_proc) return -1.0f;
    /* Use the speech variant — it takes a 1D float buffer + size,
     * which matches our semantic_vector contract exactly. */
    const float* features = cortex_cnn_forward_speech(
        (cortex_cnn_processor_t*)gl->cortex_cnn_proc, vec, dim);
    if (!features) return -1.0f;
    return _normalize_magnitude(features, dim);
}

static float _broadcast_to_fno(grounded_language_t* gl,
                                 const float* vec, uint32_t dim) {
    if (!gl->fno_proc) return -1.0f;

    /* Audit Cat A #5 — wiring fix.
     *
     * fno_audio_forward expects a MEL SPECTROGRAM of size `mel_size`
     * (the FNO's configured input_size) and writes an `embed_dim`
     * embedding into the output buffer. Pre-fix, this function passed
     * the GL semantic_vector to fno_audio_forward as if it were a mel
     * spec — the FNO interpreted the semantic features as DSP-shaped
     * input and wrote a garbage embedding. The normalize_magnitude
     * call then read `dim` floats out of the scratch buffer, but the
     * actual valid bytes were `embed_dim` floats which could be
     * smaller (silent partial read) or larger (truncation).
     *
     * The right caller for this path is gl_drive_audio_comprehension,
     * which has actual audio-derived mel data. Text comprehend has
     * no mel signal and should NOT exercise the audio FNO here — the
     * skip below returns the "not attached" sentinel so the broadcast
     * caller treats this as "no contribution" rather than mixing in
     * a garbage confidence number.
     *
     * Runtime guard: input dim must match the FNO's mel_size. Output
     * scratch buffer is sized to the FNO's embed_dim. Mismatch on
     * either side skips with a one-shot LOG_DEBUG so a misconfigured
     * call site is visible without spamming. */
    fno_audio_processor_t* fno = (fno_audio_processor_t*)gl->fno_proc;
    if (dim != fno->input_size) {
        static int warned_once = 0;
        if (!warned_once) {
            LOG_DEBUG("GL_NET_BRIDGE",
                      "_broadcast_to_fno: input dim %u != FNO mel_size %u — "
                      "skipping (one-shot warning). This is the expected "
                      "outcome for text comprehend; audio comprehend should "
                      "use gl_drive_audio_comprehension directly.",
                      dim, fno->input_size);
            warned_once = 1;
        }
        return -1.0f;
    }
    if (fno->embed_dim == 0 || fno->embed_dim > 1024) return -1.0f;

    float scratch[1024];
    int rc = fno_audio_forward(fno, vec, dim, scratch);
    if (rc != 0) return -1.0f;
    /* Read EXACTLY embed_dim floats — the FNO's actual output size,
     * not the input dim. Pre-fix bug: passed `dim` to
     * _normalize_magnitude which mis-counted output bytes. */
    return _normalize_magnitude(scratch, fno->embed_dim);
}

static float _broadcast_to_ann(grounded_language_t* gl,
                                 const float* vec, uint32_t dim) {
    if (!gl->ann_predict_fn) return -1.0f;
    if (dim > 1024) return -1.0f;
    float scratch[1024];
    int rc = gl->ann_predict_fn(gl->ann_ctx, vec, dim, scratch, dim);
    if (rc != 0) return -1.0f;
    return _normalize_magnitude(scratch, dim);
}

/*=============================================================================
 * Broadcast — runs each attached forward and stores last magnitudes.
 *===========================================================================*/

int grounded_language_broadcast_to_networks(grounded_language_t* gl,
                                              const float* vec,
                                              uint32_t dim) {
    if (!gl || !vec || dim == 0) return -1;

    int hits = 0;
    float m;

    m = _broadcast_to_lnn(gl, vec, dim);
    if (m >= 0.0f) { gl->last_lnn_mag = m; hits++; }

    m = _broadcast_to_cnn(gl, vec, dim);
    if (m >= 0.0f) { gl->last_cnn_mag = m; hits++; }

    m = _broadcast_to_fno(gl, vec, dim);
    if (m >= 0.0f) { gl->last_fno_mag = m; hits++; }

    m = _broadcast_to_ann(gl, vec, dim);
    if (m >= 0.0f) { gl->last_ann_mag = m; hits++; }

    return hits;
}

/*=============================================================================
 * Modulation accessor — read last broadcast values.
 *===========================================================================*/

int grounded_language_get_network_modulation(grounded_language_t* gl,
                                               gl_network_modulation_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof(*out));
    if (!gl) return -1;
    out->lnn_magnitude = gl->last_lnn_mag;
    out->cnn_magnitude = gl->last_cnn_mag;
    out->fno_magnitude = gl->last_fno_mag;
    out->ann_magnitude = gl->last_ann_mag;
    return 0;
}

/*=============================================================================
 * Public-ish helper for the comprehend hot path: returns a single
 * blended modulation factor in [1.0, 1.0+max_boost] driven by the
 * average of the four magnitudes. Used to bias confidence after the
 * cortex modulation hook. No-op (returns 1.0) when nothing is wired.
 *===========================================================================*/

float gl_network_modulation_factor(grounded_language_t* gl, float max_boost) {
    if (!gl) return 1.0f;
    int active = 0;
    float sum = 0.0f;
    if (gl->lnn_layer)        { sum += gl->last_lnn_mag; active++; }
    if (gl->cortex_cnn_proc)  { sum += gl->last_cnn_mag; active++; }
    if (gl->fno_proc)         { sum += gl->last_fno_mag; active++; }
    if (gl->ann_predict_fn)   { sum += gl->last_ann_mag; active++; }
    if (active == 0) return 1.0f;
    float avg = sum / (float)active;
    if (avg < 0.0f) avg = 0.0f;
    if (avg > 1.0f) avg = 1.0f;
    return 1.0f + max_boost * avg;
}
