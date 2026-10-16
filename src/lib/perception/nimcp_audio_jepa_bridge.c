/**
 * @file nimcp_audio_jepa_bridge.c
 * @brief Audio JEPA bridge implementation.
 *
 * Parallels nimcp_speech_jepa_bridge.c but operates on the environmental-audio
 * training state exposed by audio_cortex_get_training_state(). Self-contained:
 * no brain wiring, no bio-async, no factory integration. See the header for
 * the full design contract.
 *
 * @author NIMCP Development Team
 */

#include "perception/nimcp_audio_jepa_bridge.h"
#include "perception/nimcp_audio_cortex.h"
#include "cognitive/jepa/nimcp_jepa_predictor.h"
#include "cognitive/jepa/nimcp_jepa_latent.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#include <math.h>
#include <string.h>

/* ============================================================================
 * Internal constants
 * ============================================================================ */

#define AUDIO_JEPA_MEL_CHANNELS   16u
#define AUDIO_JEPA_MFCC_CHANNELS  16u
#define AUDIO_JEPA_DEFAULT_EMBED_DIM (AUDIO_JEPA_MEL_CHANNELS + AUDIO_JEPA_MFCC_CHANNELS)

/* ============================================================================
 * Internal struct (opaque to callers)
 * ============================================================================ */

struct audio_jepa_bridge_s {
    audio_cortex_t*    cortex;       /* non-owning */
    jepa_predictor_t*  predictor;    /* owning */
    jepa_latent_t*     ctx_latent;   /* owning */
    jepa_latent_t*     tgt_latent;   /* owning */
    uint32_t           embed_dim;
    float*             prev_embed;   /* owning, size embed_dim */
    bool               has_prev;
    uint32_t           n_steps;
    float              last_loss;
};

/* ============================================================================
 * Helpers
 * ============================================================================ */

/**
 * Pack the current audio training state into an embed_dim-sized vector.
 *
 * Layout:
 *   [0          .. mel_used)              : mel_features[i] / max_abs(mel)
 *   [mel_used   .. mel_used + mfcc_used)  : tanhf(mfcc_features[i])
 *   [mel_used + mfcc_used .. embed_dim)   : zeros (if embed_dim > 32)
 *
 * If embed_dim < 32, the mel block is truncated first, then the MFCC block.
 * Anything NULL or zero-sized contributes zeros for that block.
 */
static int audio_jepa_pack_embedding(const audio_training_state_t* ts,
                                     float* out,
                                     uint32_t embed_dim)
{
    if (!ts || !out || embed_dim == 0) return -1;

    memset(out, 0, sizeof(float) * embed_dim);

    /* --- Mel block: first min(16, embed_dim) channels --- */
    uint32_t mel_slots = (embed_dim < AUDIO_JEPA_MEL_CHANNELS)
                         ? embed_dim : AUDIO_JEPA_MEL_CHANNELS;

    if (ts->mel_features && ts->num_mel_filters > 0 && mel_slots > 0) {
        uint32_t n_mel = ts->num_mel_filters;
        if (n_mel > mel_slots) n_mel = mel_slots;

        /* Max-abs over the mel slice we'll actually copy. Fall back to 1.0
         * if the block is identically zero, so we don't divide by zero. */
        float max_abs = 0.0f;
        for (uint32_t i = 0; i < n_mel; i++) {
            float v = fabsf(ts->mel_features[i]);
            if (v > max_abs) max_abs = v;
        }
        float inv = (max_abs > 1e-12f) ? (1.0f / max_abs) : 0.0f;

        for (uint32_t i = 0; i < n_mel; i++) {
            out[i] = ts->mel_features[i] * inv;
        }
        /* Remaining mel slots stay at 0 (already memset). */
    }

    /* --- MFCC block: next min(16, embed_dim - 16) channels --- */
    if (embed_dim > AUDIO_JEPA_MEL_CHANNELS &&
        ts->mfcc_features && ts->num_mfcc > 0) {
        uint32_t mfcc_slots = embed_dim - AUDIO_JEPA_MEL_CHANNELS;
        if (mfcc_slots > AUDIO_JEPA_MFCC_CHANNELS) mfcc_slots = AUDIO_JEPA_MFCC_CHANNELS;

        uint32_t n_mfcc = ts->num_mfcc;
        if (n_mfcc > mfcc_slots) n_mfcc = mfcc_slots;

        for (uint32_t i = 0; i < n_mfcc; i++) {
            out[AUDIO_JEPA_MEL_CHANNELS + i] = tanhf(ts->mfcc_features[i]);
        }
    }

    /* embed_dim > 32 -> trailing channels remain zero (already memset). */
    return 0;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

audio_jepa_bridge_t* audio_jepa_bridge_create(audio_cortex_t* ac,
                                              uint32_t embed_dim)
{
    if (!ac || embed_dim == 0) {
        NIMCP_LOGGING_WARN("audio_jepa_bridge_create: invalid args "
                           "(ac=%p, embed_dim=%u)", (void*)ac, embed_dim);
        return NULL;
    }

    audio_jepa_bridge_t* b =
        (audio_jepa_bridge_t*)nimcp_calloc(1, sizeof(*b));
    if (!b) {
        NIMCP_LOGGING_ERROR("audio_jepa_bridge_create: calloc(bridge) failed");
        return NULL;
    }

    b->cortex    = ac;
    b->embed_dim = embed_dim;
    b->has_prev  = false;
    b->n_steps   = 0;
    b->last_loss = 0.0f;

    /* JEPA predictor: mirror octopus Phase 4m config. */
    jepa_predictor_config_t jcfg;
    memset(&jcfg, 0, sizeof(jcfg));
    (void)jepa_predictor_default_config(&jcfg);
    jcfg.input_dim         = embed_dim;
    jcfg.output_dim        = embed_dim;
    jcfg.hidden_dim        = embed_dim;
    jcfg.num_layers        = 1;
    jcfg.learning_rate     = 1e-3f;
    jcfg.weight_decay      = 1e-5f;
    jcfg.dropout_rate      = 0.0f;
    jcfg.enable_layer_norm = true;
    jcfg.enable_fep        = false;

    b->predictor = jepa_predictor_create(&jcfg);
    if (!b->predictor) {
        NIMCP_LOGGING_ERROR("audio_jepa_bridge_create: jepa_predictor_create failed");
        goto fail;
    }
    (void)jepa_predictor_set_training(b->predictor, true);

    b->ctx_latent = jepa_latent_create_dim(embed_dim);
    if (!b->ctx_latent) {
        NIMCP_LOGGING_ERROR("audio_jepa_bridge_create: ctx_latent alloc failed");
        goto fail;
    }
    b->tgt_latent = jepa_latent_create_dim(embed_dim);
    if (!b->tgt_latent) {
        NIMCP_LOGGING_ERROR("audio_jepa_bridge_create: tgt_latent alloc failed");
        goto fail;
    }

    b->prev_embed = (float*)nimcp_calloc(embed_dim, sizeof(float));
    if (!b->prev_embed) {
        NIMCP_LOGGING_ERROR("audio_jepa_bridge_create: prev_embed alloc failed");
        goto fail;
    }

    NIMCP_LOGGING_INFO("audio_jepa_bridge_create: embed_dim=%u", embed_dim);
    return b;

fail:
    audio_jepa_bridge_destroy(b);
    return NULL;
}

void audio_jepa_bridge_destroy(audio_jepa_bridge_t* b)
{
    if (!b) return;

    if (b->predictor) {
        jepa_predictor_destroy(b->predictor);
        b->predictor = NULL;
    }
    if (b->ctx_latent) {
        jepa_latent_destroy(b->ctx_latent);
        b->ctx_latent = NULL;
    }
    if (b->tgt_latent) {
        jepa_latent_destroy(b->tgt_latent);
        b->tgt_latent = NULL;
    }
    if (b->prev_embed) {
        nimcp_free(b->prev_embed);
        b->prev_embed = NULL;
    }
    /* cortex is non-owning — do not destroy. */
    nimcp_free(b);
}

int audio_jepa_bridge_train_step(audio_jepa_bridge_t* b, float* loss_out)
{
    if (loss_out) *loss_out = 0.0f;

    if (!b || !b->cortex || !b->predictor ||
        !b->ctx_latent || !b->tgt_latent || !b->prev_embed) {
        return -1;
    }

    /* Pull current training state from the audio cortex. */
    audio_training_state_t ts;
    memset(&ts, 0, sizeof(ts));
    if (audio_cortex_get_training_state(b->cortex, &ts) != 0 || !ts.valid) {
        /* No fresh state yet — caller should retry later. */
        return -1;
    }

    /* Pack current embedding into ctx_latent's embedding buffer via a
     * scratch array, then hand it to the JEPA latent. */
    float cur_embed_stack[64];
    float* cur_embed;
    bool cur_embed_heap = false;

    if (b->embed_dim <= (uint32_t)(sizeof(cur_embed_stack) / sizeof(float))) {
        cur_embed = cur_embed_stack;
    } else {
        cur_embed = (float*)nimcp_calloc(b->embed_dim, sizeof(float));
        if (!cur_embed) {
            NIMCP_LOGGING_ERROR("audio_jepa_bridge_train_step: cur_embed alloc failed");
            return -1;
        }
        cur_embed_heap = true;
    }

    if (audio_jepa_pack_embedding(&ts, cur_embed, b->embed_dim) != 0) {
        if (cur_embed_heap) nimcp_free(cur_embed);
        return -1;
    }

    /* First call: just seed prev_embed and return success-but-no-step. */
    if (!b->has_prev) {
        memcpy(b->prev_embed, cur_embed, sizeof(float) * b->embed_dim);
        b->has_prev = true;
        if (cur_embed_heap) nimcp_free(cur_embed);
        return 0;
    }

    /* Subsequent calls: train on (prev -> cur). */
    int rc = 0;
    if (jepa_latent_set_embedding(b->ctx_latent,
                                   b->prev_embed, b->embed_dim) != 0) {
        rc = -1;
        goto done;
    }
    if (jepa_latent_set_embedding(b->tgt_latent,
                                   cur_embed, b->embed_dim) != 0) {
        rc = -1;
        goto done;
    }

    float step_loss = 0.0f;
    int jr = jepa_predictor_train_step(b->predictor,
                                        b->ctx_latent,
                                        b->tgt_latent,
                                        &step_loss);
    if (jr == 0) {
        b->n_steps++;
        if (isfinite(step_loss)) {
            b->last_loss = step_loss;
        }
        if (loss_out) *loss_out = step_loss;
    } else {
        NIMCP_LOGGING_DEBUG("audio_jepa_bridge_train_step: "
                            "jepa_predictor_train_step rc=%d", jr);
        rc = -1;
    }

    /* Always roll prev forward: even if the JEPA step itself failed, the
     * latest state is still the best 'prev' for the next attempt. */
    memcpy(b->prev_embed, cur_embed, sizeof(float) * b->embed_dim);

done:
    if (cur_embed_heap) nimcp_free(cur_embed);
    return rc;
}

uint32_t audio_jepa_bridge_n_steps(const audio_jepa_bridge_t* b)
{
    return b ? b->n_steps : 0u;
}

float audio_jepa_bridge_last_loss(const audio_jepa_bridge_t* b)
{
    return b ? b->last_loss : 0.0f;
}
