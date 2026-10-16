/**
 * @file nimcp_engram_jepa_bridge.c
 * @brief Implementation of the JEPA bridge for the engram memory system.
 *
 * See include/cognitive/memory/nimcp_engram_jepa_bridge.h for the full
 * contract and rationale.
 *
 * Pattern: modeled after Phase 4m of the octopus module
 * (src/cognitive/octopus/nimcp_octopus.c). The bridge owns:
 *   - a jepa_predictor_t (shared, one per bridge)
 *   - two reusable jepa_latent_t buffers (ctx + tgt)
 *   - a single "previous embedding" buffer of length embed_dim
 *
 * On record(): if a prev exists, set ctx = prev, tgt = current, run one
 * jepa_predictor_train_step; then memcpy current → prev regardless.
 * On predict_next(): set ctx = current, call jepa_predictor_predict,
 * copy the resulting latent embedding out.
 */

#include "cognitive/memory/nimcp_engram_jepa_bridge.h"

/* JEPA predictor + latent APIs. */
#include "cognitive/jepa/nimcp_jepa_predictor.h"
#include "cognitive/jepa/nimcp_jepa_latent.h"

/* NIMCP memory + logging — no raw malloc/free allowed here. */
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#include <string.h>

/*============================================================================
 * Private struct
 *
 * Layout follows the contract in the header. Not shared across translation
 * units — stays opaque via the forward declaration there.
 *==========================================================================*/

struct engram_jepa_bridge_s {
    jepa_predictor_t* predictor;     /* Shared MLP predictor (embed → embed). */
    jepa_latent_t*    ctx_latent;    /* Reusable context latent. */
    jepa_latent_t*    tgt_latent;    /* Reusable target  latent. */

    uint32_t          embed_dim;     /* Dimension the bridge was created with. */

    float*            prev_embed;    /* Heap buffer, length embed_dim. */
    bool              has_prev;      /* True once the first record() lands. */

    uint32_t          n_steps;       /* Successful train steps so far. */
    float             last_loss;     /* Most recent train-step loss. */
};

/*============================================================================
 * Helpers
 *==========================================================================*/

/* Destroy whatever the bridge currently owns. Safe to call on a half-built
 * bridge during create() failure cleanup — every field is NULL-checked. */
static void _engram_jepa_bridge_free_internals(engram_jepa_bridge_t* b) {
    if (!b) {
        return;
    }
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
}

/*============================================================================
 * Lifecycle
 *==========================================================================*/

engram_jepa_bridge_t* engram_jepa_bridge_create(uint32_t embed_dim) {
    if (embed_dim == 0) {
        NIMCP_LOGGING_WARN("engram_jepa_bridge_create: embed_dim must be > 0");
        return NULL;
    }

    engram_jepa_bridge_t* b =
        (engram_jepa_bridge_t*)nimcp_calloc(1, sizeof(*b));
    if (!b) {
        NIMCP_LOGGING_ERROR("engram_jepa_bridge_create: allocation failed "
                            "(embed_dim=%u)", embed_dim);
        return NULL;
    }
    b->embed_dim = embed_dim;

    /* JEPA predictor config. Mirrors Phase 4m octopus settings but sized
     * to the caller's embed_dim. Light MLP, single hidden layer, no
     * dropout, layer norm on, FEP off. */
    jepa_predictor_config_t jcfg = {0};
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
        NIMCP_LOGGING_ERROR("engram_jepa_bridge_create: predictor_create "
                            "failed (embed_dim=%u)", embed_dim);
        _engram_jepa_bridge_free_internals(b);
        nimcp_free(b);
        return NULL;
    }
    (void)jepa_predictor_set_training(b->predictor, true);

    b->ctx_latent = jepa_latent_create_dim(embed_dim);
    b->tgt_latent = jepa_latent_create_dim(embed_dim);
    b->prev_embed = (float*)nimcp_calloc(embed_dim, sizeof(float));

    if (!b->ctx_latent || !b->tgt_latent || !b->prev_embed) {
        NIMCP_LOGGING_ERROR("engram_jepa_bridge_create: latent/prev buffer "
                            "alloc failed (embed_dim=%u)", embed_dim);
        _engram_jepa_bridge_free_internals(b);
        nimcp_free(b);
        return NULL;
    }

    b->has_prev  = false;
    b->n_steps   = 0;
    b->last_loss = 0.0f;

    NIMCP_LOGGING_INFO("engram_jepa_bridge_create: embed_dim=%u, "
                       "predictor=live, latents=live",
                       embed_dim);
    return b;
}

void engram_jepa_bridge_destroy(engram_jepa_bridge_t* b) {
    if (!b) {
        return;
    }
    _engram_jepa_bridge_free_internals(b);
    nimcp_free(b);
}

/*============================================================================
 * Record + train
 *==========================================================================*/

int engram_jepa_bridge_record(engram_jepa_bridge_t* b,
                              const float* embed,
                              uint32_t dim) {
    if (!b || !embed) {
        return -1;
    }
    if (dim != b->embed_dim) {
        NIMCP_LOGGING_WARN("engram_jepa_bridge_record: dim mismatch "
                           "(got %u, expected %u)", dim, b->embed_dim);
        return -1;
    }

    /* If we already have a prev, train on (prev → current). */
    if (b->has_prev) {
        if (jepa_latent_set_embedding(b->ctx_latent,
                                      b->prev_embed,
                                      b->embed_dim) != 0) {
            NIMCP_LOGGING_WARN("engram_jepa_bridge_record: ctx set failed");
            /* Still update prev below — skipping the training step is
             * preferable to dropping the sequence entirely. */
        } else if (jepa_latent_set_embedding(b->tgt_latent,
                                             embed,
                                             b->embed_dim) != 0) {
            NIMCP_LOGGING_WARN("engram_jepa_bridge_record: tgt set failed");
        } else {
            float step_loss = 0.0f;
            int rc = jepa_predictor_train_step(b->predictor,
                                               b->ctx_latent,
                                               b->tgt_latent,
                                               &step_loss);
            if (rc == 0) {
                b->n_steps  += 1u;
                b->last_loss = step_loss;
                NIMCP_LOGGING_TRACE("engram_jepa_bridge_record: "
                                    "step=%u loss=%.6f",
                                    b->n_steps, b->last_loss);
            } else {
                NIMCP_LOGGING_WARN("engram_jepa_bridge_record: "
                                   "train_step rc=%d", rc);
            }
        }
    }

    /* Always save current as the new prev. This also handles the
     * first-call case where there is nothing to train on yet. */
    memcpy(b->prev_embed, embed, sizeof(float) * b->embed_dim);
    b->has_prev = true;
    return 0;
}

/*============================================================================
 * Forward prediction
 *==========================================================================*/

int engram_jepa_bridge_predict_next(engram_jepa_bridge_t* b,
                                    const float* embed,
                                    uint32_t dim,
                                    float* out_pred) {
    if (!b || !embed || !out_pred) {
        return -1;
    }
    if (dim != b->embed_dim) {
        NIMCP_LOGGING_WARN("engram_jepa_bridge_predict_next: dim mismatch "
                           "(got %u, expected %u)", dim, b->embed_dim);
        return -1;
    }

    if (jepa_latent_set_embedding(b->ctx_latent, embed, b->embed_dim) != 0) {
        NIMCP_LOGGING_WARN("engram_jepa_bridge_predict_next: ctx set failed");
        return -1;
    }

    if (jepa_predictor_predict(b->predictor,
                               b->ctx_latent,
                               b->tgt_latent) != 0) {
        NIMCP_LOGGING_WARN("engram_jepa_bridge_predict_next: predict failed");
        return -1;
    }

    int copied = jepa_latent_get_embedding(b->tgt_latent,
                                           out_pred,
                                           b->embed_dim);
    if (copied < 0 || (uint32_t)copied != b->embed_dim) {
        NIMCP_LOGGING_WARN("engram_jepa_bridge_predict_next: "
                           "get_embedding copied=%d (expected %u)",
                           copied, b->embed_dim);
        return -1;
    }
    return 0;
}

/*============================================================================
 * Accessors
 *==========================================================================*/

uint32_t engram_jepa_bridge_n_steps(const engram_jepa_bridge_t* b) {
    return b ? b->n_steps : 0u;
}

float engram_jepa_bridge_last_loss(const engram_jepa_bridge_t* b) {
    return b ? b->last_loss : 0.0f;
}

int engram_jepa_bridge_tick_from_vec(engram_jepa_bridge_t* b,
                                     const float* vec,
                                     uint32_t dim) {
    if (!b || !vec || dim == 0) return -1;
    /* Pack vec into an embed_dim-shaped scratch (truncate or zero-pad),
     * then delegate to record() which handles prev/cur pair tracking. */
    float scratch_stack[1024];
    float* work = (b->embed_dim <= 1024) ? scratch_stack
        : (float*)nimcp_calloc(b->embed_dim, sizeof(float));
    if (!work) return -1;
    uint32_t copy = dim < b->embed_dim ? dim : b->embed_dim;
    if (copy > 0) memcpy(work, vec, sizeof(float) * copy);
    if (copy < b->embed_dim) {
        memset(work + copy, 0, sizeof(float) * (b->embed_dim - copy));
    }
    int rc = engram_jepa_bridge_record(b, work, b->embed_dim);
    if (work != scratch_stack) nimcp_free(work);
    return rc;
}
