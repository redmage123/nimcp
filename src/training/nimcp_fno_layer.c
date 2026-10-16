/**
 * @file nimcp_fno_layer.c
 * @brief Fourier Neural Operator — spectral convolution implementation
 *
 * Implements FFT-based spectral convolution with learned complex weights.
 * Used by both FNO Audio Cortex and FNO Population Dynamics.
 */

#include "training/nimcp_fno_layer.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

/* W5 KG-integration — FNO network-level aggregator event emitter.
 * Tracks spectral mode shift: compares the dominant mode energy between
 * consecutive forward calls and emits "spectral_shift" when the dominant
 * mode's share of total spectral energy changes by > 0.5. Admin-token
 * self-elevation per kg-node-naming-registry.md §7. */
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain_kg.h"

static brain_t s_net_fno_kg_brain = NULL;
static float   s_fno_prev_dom_ratio = -1.0f;  /* <0 = no prior sample */

void net_fno_kg_register_brain(brain_t brain) {
    s_net_fno_kg_brain = brain;
}

static void net_fno_kg_emit_event(brain_t brain, const char* kind,
                                  float magnitude, uint64_t ts_us) {
    if (!brain || !kind) return;
    if (!brain->internal_kg_enabled || !brain->internal_kg) return;

    brain_kg_t* kg = brain->internal_kg;
    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_ADMIN,
                              brain->internal_kg_admin_token);

    char ev_name[160];
    snprintf(ev_name, sizeof(ev_name),
             "net_fno_event_%s_%llu", kind, (unsigned long long)ts_us);
    char desc[240];
    snprintf(desc, sizeof(desc),
             "FNO spectral event: kind=%s delta_ratio=%.4f", kind, magnitude);

    brain_kg_node_id_t ev = brain_kg_add_node(kg, ev_name,
        BRAIN_KG_NODE_CORE, desc);
    brain_kg_node_id_t owner = brain_kg_find_node(kg, "net_fno");
    if (owner != BRAIN_KG_INVALID_NODE && ev != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, owner, ev, BRAIN_KG_EDGE_SENDS_TO,
            "produced_by", magnitude);
    }
    if (ev != BRAIN_KG_INVALID_NODE) {
        char buf[40];
        snprintf(buf, sizeof(buf), "%.6f", magnitude);
        brain_kg_add_metadata(kg, ev, "magnitude", buf);
        snprintf(buf, sizeof(buf), "%llu", (unsigned long long)ts_us);
        brain_kg_add_metadata(kg, ev, "ts_us", buf);
    }

    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_READ, 0);
}

/* Public test-facing trigger — forwards to static emit. */
void net_fno_kg_trigger_event(brain_t brain, const char* kind,
                              float magnitude, uint64_t ts_us) {
    net_fno_kg_emit_event(brain, kind, magnitude, ts_us);
}

/* =========================================================================
 * Helpers
 * ========================================================================= */

static uint32_t next_power_of_2(uint32_t v) {
    v--;
    v |= v >> 1; v |= v >> 2; v |= v >> 4;
    v |= v >> 8; v |= v >> 16;
    v++;
    return v < 4 ? 4 : v;
}

/** Simple radix-2 FFT (real input → complex output) */
static void fft_real(const float* input, uint32_t n,
                     float* out_real, float* out_imag) {
    /* Zero-pad to n if needed, then DFT */
    uint32_t freq_n = n / 2 + 1;
    for (uint32_t k = 0; k < freq_n; k++) {
        double re = 0.0, im = 0.0;
        for (uint32_t j = 0; j < n; j++) {
            double angle = -2.0 * M_PI * k * j / n;
            re += input[j] * cos(angle);
            im += input[j] * sin(angle);
        }
        out_real[k] = (float)re;
        out_imag[k] = (float)im;
    }
}

/** Inverse FFT (complex half-spectrum → real output) using Hermitian symmetry */
static void ifft_real(const float* in_real, const float* in_imag,
                      uint32_t n, float* output) {
    uint32_t freq_n = n / 2 + 1;
    float inv_n = 1.0f / (float)n;
    for (uint32_t j = 0; j < n; j++) {
        double re = in_real[0]; /* DC component (k=0, real only) */
        for (uint32_t k = 1; k < freq_n - 1; k++) {
            double angle = 2.0 * M_PI * k * j / n;
            /* Positive freq + Hermitian conjugate (negative freq) */
            re += 2.0 * (in_real[k] * cos(angle) - in_imag[k] * sin(angle));
        }
        /* Nyquist component (k=n/2, real only if n is even) */
        if (n % 2 == 0 && freq_n > 1) {
            double angle = M_PI * j; /* cos(pi*j) = (-1)^j */
            re += in_real[freq_n - 1] * cos(angle);
        }
        output[j] = (float)(re * inv_n);
    }
}

/** GELU activation: x * 0.5 * (1 + tanh(sqrt(2/pi) * (x + 0.044715*x^3))) */
static float gelu(float x) {
    float c = 0.7978845608028654f; /* sqrt(2/pi) */
    float inner = c * (x + 0.044715f * x * x * x);
    return 0.5f * x * (1.0f + tanhf(inner));
}

/** GELU derivative */
static float gelu_grad(float x) {
    float c = 0.7978845608028654f;
    float x3 = x * x * x;
    float inner = c * (x + 0.044715f * x3);
    float tanh_val = tanhf(inner);
    float sech2 = 1.0f - tanh_val * tanh_val;
    float d_inner = c * (1.0f + 3.0f * 0.044715f * x * x);
    return 0.5f * (1.0f + tanh_val) + 0.5f * x * sech2 * d_inner;
}

static float xavier_init(uint32_t fan_in, uint32_t fan_out) {
    float limit = sqrtf(6.0f / (float)(fan_in + fan_out));
    return limit * (2.0f * ((float)rand() / RAND_MAX) - 1.0f);
}

/* =========================================================================
 * Spectral Convolution Layer
 * ========================================================================= */

fno_spectral_conv_t* fno_spectral_conv_create(
    uint32_t in_ch, uint32_t out_ch, uint32_t n_modes, uint32_t spatial_size)
{
    if (in_ch == 0 || out_ch == 0 || n_modes == 0 || spatial_size == 0) {
        NIMCP_LOGGING_ERROR("fno_spectral_conv_create: invalid dimensions");
        return NULL;
    }

    fno_spectral_conv_t* layer = nimcp_calloc(1, sizeof(fno_spectral_conv_t));
    if (!layer) return NULL;

    layer->in_channels = in_ch;
    layer->out_channels = out_ch;
    layer->n_modes = n_modes;
    layer->spatial_size = spatial_size;
    layer->fft_size = next_power_of_2(spatial_size);
    layer->freq_size = layer->fft_size / 2 + 1;

    /* Clamp modes to freq_size */
    if (layer->n_modes > layer->freq_size)
        layer->n_modes = layer->freq_size;

    size_t w_size = (size_t)out_ch * in_ch * n_modes;
    size_t bypass_size = (size_t)out_ch * in_ch;

    /* Spectral weights (complex) */
    layer->W_real = nimcp_calloc(w_size, sizeof(float));
    layer->W_imag = nimcp_calloc(w_size, sizeof(float));
    layer->grad_W_real = nimcp_calloc(w_size, sizeof(float));
    layer->grad_W_imag = nimcp_calloc(w_size, sizeof(float));

    /* Bypass weights */
    layer->W_bypass = nimcp_calloc(bypass_size, sizeof(float));
    layer->grad_W_bypass = nimcp_calloc(bypass_size, sizeof(float));

    /* Bias */
    layer->bias = nimcp_calloc(out_ch, sizeof(float));
    layer->grad_bias = nimcp_calloc(out_ch, sizeof(float));

    /* Cache */
    layer->input_cache = nimcp_calloc((size_t)in_ch * spatial_size, sizeof(float));
    layer->input_hat_real = nimcp_calloc((size_t)in_ch * layer->freq_size, sizeof(float));
    layer->input_hat_imag = nimcp_calloc((size_t)in_ch * layer->freq_size, sizeof(float));
    layer->pre_activation = nimcp_calloc((size_t)out_ch * spatial_size, sizeof(float));

    if (!layer->W_real || !layer->W_imag || !layer->W_bypass ||
        !layer->bias || !layer->input_cache || !layer->pre_activation) {
        fno_spectral_conv_destroy(layer);
        return NULL;
    }

    /* Spectral-aware initialization: scale by 1/sqrt(in_ch * n_modes)
     * to account for FFT amplification. Lower modes get slightly larger
     * weights since they carry more signal energy. */
    float scale = 1.0f / sqrtf((float)(in_ch * n_modes));
    for (size_t i = 0; i < w_size; i++) {
        layer->W_real[i] = scale * (2.0f * ((float)rand() / RAND_MAX) - 1.0f);
        layer->W_imag[i] = scale * (2.0f * ((float)rand() / RAND_MAX) - 1.0f);
    }
    scale = sqrtf(2.0f / (float)(in_ch + out_ch));
    for (size_t i = 0; i < bypass_size; i++) {
        layer->W_bypass[i] = scale * (2.0f * ((float)rand() / RAND_MAX) - 1.0f);
    }

    NIMCP_LOGGING_DEBUG("FNO spectral conv: %u→%u ch, %u modes, fft=%u",
                        in_ch, out_ch, n_modes, layer->fft_size);
    return layer;
}

void fno_spectral_conv_destroy(fno_spectral_conv_t* layer) {
    if (!layer) return;
    nimcp_free(layer->W_real);
    nimcp_free(layer->W_imag);
    nimcp_free(layer->grad_W_real);
    nimcp_free(layer->grad_W_imag);
    nimcp_free(layer->W_bypass);
    nimcp_free(layer->grad_W_bypass);
    nimcp_free(layer->bias);
    nimcp_free(layer->grad_bias);
    nimcp_free(layer->input_cache);
    nimcp_free(layer->input_hat_real);
    nimcp_free(layer->input_hat_imag);
    nimcp_free(layer->pre_activation);
    nimcp_free(layer);
}

int fno_spectral_conv_forward(fno_spectral_conv_t* layer,
                               const float* input, float* output) {
    if (!layer || !input || !output) return -1;

    uint32_t in_ch = layer->in_channels;
    uint32_t out_ch = layer->out_channels;
    uint32_t n_modes = layer->n_modes;
    uint32_t sp = layer->spatial_size;
    uint32_t fft_n = layer->fft_size;
    uint32_t freq_n = layer->freq_size;

    /* Cache input for backward */
    memcpy(layer->input_cache, input, (size_t)in_ch * sp * sizeof(float));

    /* FFT each input channel */
    float* padded = nimcp_calloc(fft_n, sizeof(float));
    if (!padded) return -1;

    for (uint32_t ic = 0; ic < in_ch; ic++) {
        memset(padded, 0, fft_n * sizeof(float));
        memcpy(padded, input + ic * sp, sp * sizeof(float));
        fft_real(padded, fft_n,
                 layer->input_hat_real + ic * freq_n,
                 layer->input_hat_imag + ic * freq_n);
    }

    /* Spectral convolution: for each output channel */
    float* out_hat_real = nimcp_calloc(freq_n, sizeof(float));
    float* out_hat_imag = nimcp_calloc(freq_n, sizeof(float));
    float* ifft_out = nimcp_calloc(fft_n, sizeof(float));

    if (!out_hat_real || !out_hat_imag || !ifft_out) {
        nimcp_free(padded); nimcp_free(out_hat_real);
        nimcp_free(out_hat_imag); nimcp_free(ifft_out);
        return -1;
    }

    for (uint32_t oc = 0; oc < out_ch; oc++) {
        memset(out_hat_real, 0, freq_n * sizeof(float));
        memset(out_hat_imag, 0, freq_n * sizeof(float));

        /* Sum over input channels */
        for (uint32_t ic = 0; ic < in_ch; ic++) {
            size_t w_off = (size_t)oc * in_ch * n_modes + ic * n_modes;
            float* ihr = layer->input_hat_real + ic * freq_n;
            float* ihi = layer->input_hat_imag + ic * freq_n;

            for (uint32_t k = 0; k < n_modes && k < freq_n; k++) {
                float wr = layer->W_real[w_off + k];
                float wi = layer->W_imag[w_off + k];
                /* Complex multiply: (wr + j*wi) * (ihr + j*ihi) */
                out_hat_real[k] += wr * ihr[k] - wi * ihi[k];
                out_hat_imag[k] += wr * ihi[k] + wi * ihr[k];
            }
        }

        /* IFFT */
        ifft_real(out_hat_real, out_hat_imag, fft_n, ifft_out);

        /* Copy to output (truncate to spatial_size) + bypass + bias */
        float* out_ptr = output + oc * sp;
        float* pre_ptr = layer->pre_activation + oc * sp;
        for (uint32_t i = 0; i < sp; i++) {
            float spectral_val = ifft_out[i];

            /* Bypass: 1x1 conv (sum over input channels at position i) */
            float bypass_val = 0.0f;
            for (uint32_t ic = 0; ic < in_ch; ic++) {
                bypass_val += layer->W_bypass[oc * in_ch + ic] * input[ic * sp + i];
            }

            pre_ptr[i] = spectral_val + bypass_val + layer->bias[oc];
            out_ptr[i] = gelu(pre_ptr[i]);
        }
    }

    /* W5 KG anomaly emit — track dominant-mode share of the last oc's
     * spectrum. If the ratio moves by > 0.5 between calls (new dominant
     * mode or collapse onto a single mode), emit "spectral_shift".
     * Only the LAST `oc`'s out_hat_* buffers are still populated — that
     * is the sample we inspect. Single hook per forward call. */
    if (s_net_fno_kg_brain && freq_n > 0) {
        float total = 0.0f, peak = 0.0f;
        for (uint32_t k = 0; k < freq_n; k++) {
            float e = out_hat_real[k] * out_hat_real[k]
                    + out_hat_imag[k] * out_hat_imag[k];
            total += e;
            if (e > peak) peak = e;
        }
        if (total > 1e-12f) {
            float ratio = peak / total;
            if (s_fno_prev_dom_ratio >= 0.0f) {
                float delta = fabsf(ratio - s_fno_prev_dom_ratio);
                if (delta > 0.5f) {
                    uint64_t ts_us = (uint64_t)time(NULL) * 1000000ULL;
                    net_fno_kg_emit_event(s_net_fno_kg_brain,
                        "spectral_shift", delta, ts_us);
                }
            }
            s_fno_prev_dom_ratio = ratio;
        }
    }

    nimcp_free(padded);
    nimcp_free(out_hat_real);
    nimcp_free(out_hat_imag);
    nimcp_free(ifft_out);
    return 0;
}

int fno_spectral_conv_backward(fno_spectral_conv_t* layer,
                                const float* dl_dout, float* dl_din) {
    if (!layer || !dl_dout) return -1;

    uint32_t in_ch = layer->in_channels;
    uint32_t out_ch = layer->out_channels;
    uint32_t n_modes = layer->n_modes;
    uint32_t sp = layer->spatial_size;
    uint32_t fft_n = layer->fft_size;
    uint32_t freq_n = layer->freq_size;

    /* Apply GELU backward */
    float* dl_dpre = nimcp_calloc((size_t)out_ch * sp, sizeof(float));
    if (!dl_dpre) return -1;

    for (uint32_t oc = 0; oc < out_ch; oc++) {
        for (uint32_t i = 0; i < sp; i++) {
            float pre = layer->pre_activation[oc * sp + i];
            dl_dpre[oc * sp + i] = dl_dout[oc * sp + i] * gelu_grad(pre);
        }
    }

    /* Bias gradient */
    for (uint32_t oc = 0; oc < out_ch; oc++) {
        for (uint32_t i = 0; i < sp; i++) {
            layer->grad_bias[oc] += dl_dpre[oc * sp + i];
        }
    }

    /* Bypass gradient: grad_W_bypass[oc*in_ch+ic] += sum_i dl_dpre[oc,i] * input[ic,i] */
    for (uint32_t oc = 0; oc < out_ch; oc++) {
        for (uint32_t ic = 0; ic < in_ch; ic++) {
            float grad = 0.0f;
            for (uint32_t i = 0; i < sp; i++) {
                grad += dl_dpre[oc * sp + i] * layer->input_cache[ic * sp + i];
            }
            layer->grad_W_bypass[oc * in_ch + ic] += grad;
        }
    }

    /* Bypass contribution to dl_din */
    if (dl_din) {
        memset(dl_din, 0, (size_t)in_ch * sp * sizeof(float));
        for (uint32_t ic = 0; ic < in_ch; ic++) {
            for (uint32_t oc = 0; oc < out_ch; oc++) {
                float w = layer->W_bypass[oc * in_ch + ic];
                for (uint32_t i = 0; i < sp; i++) {
                    dl_din[ic * sp + i] += w * dl_dpre[oc * sp + i];
                }
            }
        }
    }

    /* Spectral gradient: FFT dl_dpre, accumulate grad_W, propagate to dl_din */
    float* padded = nimcp_calloc(fft_n, sizeof(float));
    float* dl_hat_real = nimcp_calloc(freq_n, sizeof(float));
    float* dl_hat_imag = nimcp_calloc(freq_n, sizeof(float));

    if (!padded || !dl_hat_real || !dl_hat_imag) {
        nimcp_free(dl_dpre); nimcp_free(padded);
        nimcp_free(dl_hat_real); nimcp_free(dl_hat_imag);
        return -1;
    }

    for (uint32_t oc = 0; oc < out_ch; oc++) {
        /* FFT the upstream gradient for this output channel */
        memset(padded, 0, fft_n * sizeof(float));
        memcpy(padded, dl_dpre + oc * sp, sp * sizeof(float));
        fft_real(padded, fft_n, dl_hat_real, dl_hat_imag);

        for (uint32_t ic = 0; ic < in_ch; ic++) {
            size_t w_off = (size_t)oc * in_ch * n_modes + ic * n_modes;
            float* ihr = layer->input_hat_real + ic * freq_n;
            float* ihi = layer->input_hat_imag + ic * freq_n;

            for (uint32_t k = 0; k < n_modes && k < freq_n; k++) {
                /* grad_W = dl_hat * conj(input_hat) */
                layer->grad_W_real[w_off + k] +=
                    dl_hat_real[k] * ihr[k] + dl_hat_imag[k] * ihi[k];
                layer->grad_W_imag[w_off + k] +=
                    dl_hat_imag[k] * ihr[k] - dl_hat_real[k] * ihi[k];
            }

            /* Spectral contribution to dl_din: IFFT(W^H * dl_hat) */
            if (dl_din) {
                float* din_hat_real = nimcp_calloc(freq_n, sizeof(float));
                float* din_hat_imag = nimcp_calloc(freq_n, sizeof(float));
                float* din_spatial = nimcp_calloc(fft_n, sizeof(float));
                if (!din_hat_real || !din_hat_imag || !din_spatial) {
                    nimcp_free(din_hat_real);
                    nimcp_free(din_hat_imag);
                    nimcp_free(din_spatial);
                    continue;  /* Skip this channel pair */
                }
                {
                    for (uint32_t k = 0; k < n_modes && k < freq_n; k++) {
                        float wr = layer->W_real[w_off + k];
                        float wi = layer->W_imag[w_off + k];
                        /* W^H * dl_hat = conj(W) * dl_hat */
                        din_hat_real[k] += wr * dl_hat_real[k] + wi * dl_hat_imag[k];
                        din_hat_imag[k] += wr * dl_hat_imag[k] - wi * dl_hat_real[k];
                    }
                    ifft_real(din_hat_real, din_hat_imag, fft_n, din_spatial);
                    for (uint32_t i = 0; i < sp; i++) {
                        dl_din[ic * sp + i] += din_spatial[i];
                    }
                }
                nimcp_free(din_hat_real);
                nimcp_free(din_hat_imag);
                nimcp_free(din_spatial);
            }
        }
    }

    nimcp_free(dl_dpre);
    nimcp_free(padded);
    nimcp_free(dl_hat_real);
    nimcp_free(dl_hat_imag);
    return 0;
}

void fno_spectral_conv_zero_grad(fno_spectral_conv_t* layer) {
    if (!layer) return;
    size_t w_size = (size_t)layer->out_channels * layer->in_channels * layer->n_modes;
    size_t bypass_size = (size_t)layer->out_channels * layer->in_channels;
    memset(layer->grad_W_real, 0, w_size * sizeof(float));
    memset(layer->grad_W_imag, 0, w_size * sizeof(float));
    memset(layer->grad_W_bypass, 0, bypass_size * sizeof(float));
    memset(layer->grad_bias, 0, layer->out_channels * sizeof(float));
}

void fno_spectral_conv_step(fno_spectral_conv_t* layer, float lr) {
    if (!layer) return;
    size_t w_size = (size_t)layer->out_channels * layer->in_channels * layer->n_modes;
    size_t bypass_size = (size_t)layer->out_channels * layer->in_channels;

    for (size_t i = 0; i < w_size; i++) {
        layer->W_real[i] -= lr * layer->grad_W_real[i];
        layer->W_imag[i] -= lr * layer->grad_W_imag[i];
    }
    for (size_t i = 0; i < bypass_size; i++) {
        layer->W_bypass[i] -= lr * layer->grad_W_bypass[i];
    }
    for (uint32_t i = 0; i < layer->out_channels; i++) {
        layer->bias[i] -= lr * layer->grad_bias[i];
    }
}

uint32_t fno_spectral_conv_param_count(const fno_spectral_conv_t* layer) {
    if (!layer) return 0;
    uint32_t spectral = layer->out_channels * layer->in_channels * layer->n_modes * 2;
    uint32_t bypass = layer->out_channels * layer->in_channels;
    uint32_t bias = layer->out_channels;
    return spectral + bypass + bias;
}

/* =========================================================================
 * FNO Audio Processor
 * ========================================================================= */

fno_audio_processor_t* fno_audio_create(
    uint32_t mel_size, uint32_t embed_dim,
    uint32_t hidden_ch, uint32_t n_modes, uint32_t n_blocks)
{
    if (mel_size == 0 || embed_dim == 0 || hidden_ch == 0) {
        NIMCP_LOGGING_ERROR("fno_audio_create: invalid dimensions");
        return NULL;
    }
    if (n_modes == 0) n_modes = mel_size / 4;
    if (n_blocks == 0) n_blocks = 4;

    fno_audio_processor_t* proc = nimcp_calloc(1, sizeof(fno_audio_processor_t));
    if (!proc) return NULL;

    proc->input_size = mel_size;
    proc->embed_dim = embed_dim;
    proc->hidden_channels = hidden_ch;
    proc->n_modes = n_modes;
    proc->n_blocks = n_blocks;

    /* Lifting: 1 channel → hidden_ch */
    proc->W_lift = nimcp_calloc(hidden_ch, sizeof(float));
    proc->b_lift = nimcp_calloc(hidden_ch, sizeof(float));
    proc->grad_W_lift = nimcp_calloc(hidden_ch, sizeof(float));
    proc->grad_b_lift = nimcp_calloc(hidden_ch, sizeof(float));

    if (!proc->W_lift || !proc->b_lift) {
        fno_audio_destroy(proc);
        return NULL;
    }

    /* Init lifting weights */
    float scale = sqrtf(2.0f / (float)(1 + hidden_ch));
    for (uint32_t i = 0; i < hidden_ch; i++) {
        proc->W_lift[i] = scale * (2.0f * ((float)rand() / RAND_MAX) - 1.0f);
    }

    /* FNO blocks */
    proc->blocks = nimcp_calloc(n_blocks, sizeof(fno_spectral_conv_t*));
    if (!proc->blocks) { fno_audio_destroy(proc); return NULL; }

    for (uint32_t i = 0; i < n_blocks; i++) {
        proc->blocks[i] = fno_spectral_conv_create(hidden_ch, hidden_ch, n_modes, mel_size);
        if (!proc->blocks[i]) { fno_audio_destroy(proc); return NULL; }
    }

    /* Projection: hidden_ch → embed_dim */
    proc->W_proj = nimcp_calloc((size_t)embed_dim * hidden_ch, sizeof(float));
    proc->b_proj = nimcp_calloc(embed_dim, sizeof(float));
    proc->grad_W_proj = nimcp_calloc((size_t)embed_dim * hidden_ch, sizeof(float));
    proc->grad_b_proj = nimcp_calloc(embed_dim, sizeof(float));

    if (!proc->W_proj || !proc->b_proj) {
        fno_audio_destroy(proc);
        return NULL;
    }

    scale = sqrtf(2.0f / (float)(hidden_ch + embed_dim));
    for (size_t i = 0; i < (size_t)embed_dim * hidden_ch; i++) {
        proc->W_proj[i] = scale * (2.0f * ((float)rand() / RAND_MAX) - 1.0f);
    }

    /* Cache */
    proc->lifted = nimcp_calloc((size_t)hidden_ch * mel_size, sizeof(float));
    proc->block_outputs = nimcp_calloc(n_blocks, sizeof(float*));
    if (proc->block_outputs) {
        for (uint32_t i = 0; i < n_blocks; i++) {
            proc->block_outputs[i] = nimcp_calloc((size_t)hidden_ch * mel_size, sizeof(float));
        }
    }
    proc->pooled = nimcp_calloc(hidden_ch, sizeof(float));

    NIMCP_LOGGING_INFO("FNO audio: mel=%u → %u blocks (hidden=%u, modes=%u) → embed=%u",
                       mel_size, n_blocks, hidden_ch, n_modes, embed_dim);
    return proc;
}

void fno_audio_destroy(fno_audio_processor_t* proc) {
    if (!proc) return;
    nimcp_free(proc->W_lift);
    nimcp_free(proc->b_lift);
    nimcp_free(proc->grad_W_lift);
    nimcp_free(proc->grad_b_lift);

    if (proc->blocks) {
        for (uint32_t i = 0; i < proc->n_blocks; i++) {
            fno_spectral_conv_destroy(proc->blocks[i]);
        }
        nimcp_free(proc->blocks);
    }

    nimcp_free(proc->W_proj);
    nimcp_free(proc->b_proj);
    nimcp_free(proc->grad_W_proj);
    nimcp_free(proc->grad_b_proj);

    nimcp_free(proc->lifted);
    if (proc->block_outputs) {
        for (uint32_t i = 0; i < proc->n_blocks; i++) {
            nimcp_free(proc->block_outputs[i]);
        }
        nimcp_free(proc->block_outputs);
    }
    nimcp_free(proc->pooled);
    nimcp_free(proc);
}

int fno_audio_forward(fno_audio_processor_t* proc,
                       const float* mel, uint32_t mel_size, float* embedding) {
    if (!proc || !mel || !embedding) return -1;
    if (mel_size != proc->input_size) return -1;

    uint32_t hc = proc->hidden_channels;
    uint32_t sp = proc->input_size;

    /* Lifting: 1-channel mel → hidden_ch channels */
    for (uint32_t ch = 0; ch < hc; ch++) {
        for (uint32_t i = 0; i < sp; i++) {
            proc->lifted[ch * sp + i] = proc->W_lift[ch] * mel[i] + proc->b_lift[ch];
        }
    }

    /* FNO blocks */
    float* current = proc->lifted;
    for (uint32_t b = 0; b < proc->n_blocks; b++) {
        int rc = fno_spectral_conv_forward(proc->blocks[b], current, proc->block_outputs[b]);
        if (rc != 0) return rc;
        current = proc->block_outputs[b];
    }

    /* Global average pooling: [hidden_ch, sp] → [hidden_ch] */
    float inv_sp = 1.0f / (float)sp;
    for (uint32_t ch = 0; ch < hc; ch++) {
        float sum = 0.0f;
        for (uint32_t i = 0; i < sp; i++) {
            sum += current[ch * sp + i];
        }
        proc->pooled[ch] = sum * inv_sp;
    }

    /* Projection: hidden_ch → embed_dim */
    for (uint32_t j = 0; j < proc->embed_dim; j++) {
        float sum = proc->b_proj[j];
        for (uint32_t ch = 0; ch < hc; ch++) {
            sum += proc->W_proj[j * hc + ch] * proc->pooled[ch];
        }
        embedding[j] = sum;
    }

    proc->forward_steps++;
    return 0;
}

int fno_audio_backward(fno_audio_processor_t* proc,
                        const float* dl_dembed, float* dl_dinput) {
    if (!proc || !dl_dembed) return -1;

    uint32_t hc = proc->hidden_channels;
    uint32_t sp = proc->input_size;
    uint32_t ed = proc->embed_dim;

    /* Backward through projection */
    float* dl_dpooled = nimcp_calloc(hc, sizeof(float));
    if (!dl_dpooled) return -1;

    for (uint32_t j = 0; j < ed; j++) {
        proc->grad_b_proj[j] += dl_dembed[j];
        for (uint32_t ch = 0; ch < hc; ch++) {
            proc->grad_W_proj[j * hc + ch] += dl_dembed[j] * proc->pooled[ch];
            dl_dpooled[ch] += proc->W_proj[j * hc + ch] * dl_dembed[j];
        }
    }

    /* Backward through global average pool: [hc] → [hc, sp] */
    float* dl_dcurrent = nimcp_calloc((size_t)hc * sp, sizeof(float));
    if (!dl_dcurrent) { nimcp_free(dl_dpooled); return -1; }

    float inv_sp = 1.0f / (float)sp;
    for (uint32_t ch = 0; ch < hc; ch++) {
        float grad = dl_dpooled[ch] * inv_sp;
        for (uint32_t i = 0; i < sp; i++) {
            dl_dcurrent[ch * sp + i] = grad;
        }
    }
    nimcp_free(dl_dpooled);

    /* Backward through FNO blocks (reverse order) */
    float* dl_dblock_in = nimcp_calloc((size_t)hc * sp, sizeof(float));
    if (!dl_dblock_in) { nimcp_free(dl_dcurrent); return -1; }

    for (int b = (int)proc->n_blocks - 1; b >= 0; b--) {
        int rc = fno_spectral_conv_backward(proc->blocks[b], dl_dcurrent, dl_dblock_in);
        if (rc != 0) {
            nimcp_free(dl_dcurrent); nimcp_free(dl_dblock_in);
            return rc;
        }
        /* Swap for next iteration */
        float* tmp = dl_dcurrent;
        dl_dcurrent = dl_dblock_in;
        dl_dblock_in = tmp;
    }

    /* Backward through lifting */
    for (uint32_t ch = 0; ch < hc; ch++) {
        for (uint32_t i = 0; i < sp; i++) {
            /* Lifting gradient: d(W*x+b)/dW = x. Use original mel input, not lifted. */
            proc->grad_W_lift[ch] += dl_dcurrent[ch * sp + i];
            proc->grad_b_lift[ch] += dl_dcurrent[ch * sp + i];
        }
    }

    /* Propagate to input if requested */
    if (dl_dinput) {
        memset(dl_dinput, 0, sp * sizeof(float));
        for (uint32_t ch = 0; ch < hc; ch++) {
            for (uint32_t i = 0; i < sp; i++) {
                dl_dinput[i] += proc->W_lift[ch] * dl_dcurrent[ch * sp + i];
            }
        }
    }

    nimcp_free(dl_dcurrent);
    nimcp_free(dl_dblock_in);
    return 0;
}

void fno_audio_step(fno_audio_processor_t* proc, float lr) {
    if (!proc) return;

    /* Lifting */
    for (uint32_t i = 0; i < proc->hidden_channels; i++) {
        proc->W_lift[i] -= lr * proc->grad_W_lift[i];
        proc->b_lift[i] -= lr * proc->grad_b_lift[i];
    }

    /* Blocks */
    for (uint32_t b = 0; b < proc->n_blocks; b++) {
        fno_spectral_conv_step(proc->blocks[b], lr);
    }

    /* Projection */
    size_t proj_size = (size_t)proc->embed_dim * proc->hidden_channels;
    for (size_t i = 0; i < proj_size; i++) {
        proc->W_proj[i] -= lr * proc->grad_W_proj[i];
    }
    for (uint32_t i = 0; i < proc->embed_dim; i++) {
        proc->b_proj[i] -= lr * proc->grad_b_proj[i];
    }
}

void fno_audio_zero_grad(fno_audio_processor_t* proc) {
    if (!proc) return;

    memset(proc->grad_W_lift, 0, proc->hidden_channels * sizeof(float));
    memset(proc->grad_b_lift, 0, proc->hidden_channels * sizeof(float));

    for (uint32_t b = 0; b < proc->n_blocks; b++) {
        fno_spectral_conv_zero_grad(proc->blocks[b]);
    }

    size_t proj_size = (size_t)proc->embed_dim * proc->hidden_channels;
    memset(proc->grad_W_proj, 0, proj_size * sizeof(float));
    memset(proc->grad_b_proj, 0, proc->embed_dim * sizeof(float));
}

/* Metrics accessors for brain-level reporting */
float fno_audio_get_last_loss(const void* p) {
    const fno_audio_processor_t* proc = (const fno_audio_processor_t*)p;
    return proc ? proc->last_loss : 0.0f;
}

float fno_audio_get_ema_loss(const void* p) {
    const fno_audio_processor_t* proc = (const fno_audio_processor_t*)p;
    return proc ? proc->ema_loss : 0.0f;
}

uint64_t fno_audio_get_steps(const void* p) {
    const fno_audio_processor_t* proc = (const fno_audio_processor_t*)p;
    return proc ? proc->forward_steps : 0;
}

uint32_t fno_audio_get_param_count(const void* p) {
    const fno_audio_processor_t* proc = (const fno_audio_processor_t*)p;
    if (!proc) return 0;
    uint32_t count = proc->hidden_channels * 2; /* W_lift + b_lift */
    for (uint32_t b = 0; b < proc->n_blocks; b++) {
        count += fno_spectral_conv_param_count(proc->blocks[b]);
    }
    count += proc->embed_dim * proc->hidden_channels + proc->embed_dim; /* W_proj + b_proj */
    return count;
}

uint32_t fno_audio_param_count(const fno_audio_processor_t* proc) {
    if (!proc) return 0;
    uint32_t count = proc->hidden_channels * 2; /* lift W + b */
    for (uint32_t b = 0; b < proc->n_blocks; b++) {
        count += fno_spectral_conv_param_count(proc->blocks[b]);
    }
    count += proc->embed_dim * proc->hidden_channels + proc->embed_dim; /* proj W + b */
    return count;
}

void fno_audio_record_loss(fno_audio_processor_t* proc, float loss) {
    if (!proc) return;
    proc->last_loss = loss;
    if (proc->ema_loss == 0.0f && proc->forward_steps <= 1) {
        proc->ema_loss = loss;
    } else {
        proc->ema_loss = 0.99f * proc->ema_loss + 0.01f * loss;
    }
}
