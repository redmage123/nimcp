/**
 * @file nimcp_consolidation_jepa_bridge.c
 * @brief Implementation of the JEPA bridge for the memory consolidation
 *        system.
 *
 * See include/cognitive/memory/nimcp_consolidation_jepa_bridge.h for the
 * full contract and rationale.
 *
 * Pattern: modeled after nimcp_engram_jepa_bridge.c. The bridge owns:
 *   - a jepa_predictor_t (shared, one per bridge)
 *   - two reusable jepa_latent_t buffers (ctx + tgt)
 *
 * Unlike the engram bridge this one does NOT keep a "previous embedding"
 * buffer. Consolidation events are intrinsically pre/post pairs and the
 * caller supplies both endpoints per record() call.
 *
 * On record():        set ctx = pre, tgt = post, run one train_step.
 * On predict_outcome(): set ctx = pre, forward through predictor,
 *                       copy resulting latent out.
 * On quality():       forward-predict from pre, MSE vs observed post,
 *                     return 1 / (1 + mse).
 */

#include "cognitive/memory/nimcp_consolidation_jepa_bridge.h"

/* JEPA predictor + latent APIs. */
#include "cognitive/jepa/nimcp_jepa_predictor.h"
#include "cognitive/jepa/nimcp_jepa_latent.h"

/* NIMCP memory + logging — no raw malloc/free allowed here. */
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#include <string.h>
#include <math.h>

/*============================================================================
 * Stack/heap threshold for the scratch buffer used by quality()
 *==========================================================================*/

/* If embed_dim <= this, quality() uses a stack buffer; otherwise it
 * falls back to nimcp_calloc/nimcp_free. Keeps the common 64 / 128
 * cases allocation-free while still handling arbitrary larger dims. */
#define CONSOLIDATION_JEPA_BRIDGE_STACK_SCRATCH 256u

/*============================================================================
 * Private struct
 *
 * Layout follows the contract in the header. Not shared across translation
 * units — stays opaque via the forward declaration there.
 *==========================================================================*/

struct consolidation_jepa_bridge_s {
    jepa_predictor_t* predictor;     /* Shared MLP predictor (embed → embed). */
    jepa_latent_t*    ctx_latent;    /* Reusable context latent. */
    jepa_latent_t*    tgt_latent;    /* Reusable target  latent. */

    uint32_t          embed_dim;     /* Dimension the bridge was created with. */

    uint32_t          n_steps;       /* Successful train steps so far. */
    float             last_loss;     /* Most recent train-step loss. */

    /* Self-drive prev buffer for tick_from_vec. */
    float*            prev_embed;
    bool              has_prev;
};

/*============================================================================
 * Helpers
 *==========================================================================*/

/* Destroy whatever the bridge currently owns. Safe to call on a half-built
 * bridge during create() failure cleanup — every field is NULL-checked. */
static void _consolidation_jepa_bridge_free_internals(consolidation_jepa_bridge_t* b) {
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

consolidation_jepa_bridge_t* consolidation_jepa_bridge_create(uint32_t embed_dim) {
    if (embed_dim == 0) {
        NIMCP_LOGGING_WARN("consolidation_jepa_bridge_create: embed_dim must be > 0");
        return NULL;
    }

    consolidation_jepa_bridge_t* b =
        (consolidation_jepa_bridge_t*)nimcp_calloc(1, sizeof(*b));
    if (!b) {
        NIMCP_LOGGING_ERROR("consolidation_jepa_bridge_create: allocation failed "
                            "(embed_dim=%u)", embed_dim);
        return NULL;
    }
    b->embed_dim = embed_dim;

    /* JEPA predictor config. Mirrors engram bridge settings but sized
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
        NIMCP_LOGGING_ERROR("consolidation_jepa_bridge_create: predictor_create "
                            "failed (embed_dim=%u)", embed_dim);
        _consolidation_jepa_bridge_free_internals(b);
        nimcp_free(b);
        return NULL;
    }
    (void)jepa_predictor_set_training(b->predictor, true);

    b->ctx_latent = jepa_latent_create_dim(embed_dim);
    b->tgt_latent = jepa_latent_create_dim(embed_dim);
    b->prev_embed = (float*)nimcp_calloc(embed_dim, sizeof(float));

    if (!b->ctx_latent || !b->tgt_latent || !b->prev_embed) {
        NIMCP_LOGGING_ERROR("consolidation_jepa_bridge_create: latent/prev alloc "
                            "failed (embed_dim=%u)", embed_dim);
        _consolidation_jepa_bridge_free_internals(b);
        nimcp_free(b);
        return NULL;
    }

    b->n_steps   = 0;
    b->last_loss = 0.0f;

    NIMCP_LOGGING_INFO("consolidation_jepa_bridge_create: embed_dim=%u, "
                       "predictor=live, latents=live",
                       embed_dim);
    return b;
}

void consolidation_jepa_bridge_destroy(consolidation_jepa_bridge_t* b) {
    if (!b) {
        return;
    }
    _consolidation_jepa_bridge_free_internals(b);
    nimcp_free(b);
}

/*============================================================================
 * Record + train
 *==========================================================================*/

int consolidation_jepa_bridge_record(consolidation_jepa_bridge_t* b,
                                     const float* pre_embed,
                                     const float* post_embed,
                                     uint32_t dim,
                                     float* loss_out) {
    if (!b || !pre_embed || !post_embed) {
        return -1;
    }
    if (dim != b->embed_dim) {
        NIMCP_LOGGING_WARN("consolidation_jepa_bridge_record: dim mismatch "
                           "(got %u, expected %u)", dim, b->embed_dim);
        return -1;
    }

    if (jepa_latent_set_embedding(b->ctx_latent,
                                  pre_embed,
                                  b->embed_dim) != 0) {
        NIMCP_LOGGING_WARN("consolidation_jepa_bridge_record: ctx set failed");
        return -1;
    }
    if (jepa_latent_set_embedding(b->tgt_latent,
                                  post_embed,
                                  b->embed_dim) != 0) {
        NIMCP_LOGGING_WARN("consolidation_jepa_bridge_record: tgt set failed");
        return -1;
    }

    float step_loss = 0.0f;
    int rc = jepa_predictor_train_step(b->predictor,
                                       b->ctx_latent,
                                       b->tgt_latent,
                                       &step_loss);
    if (rc != 0) {
        NIMCP_LOGGING_WARN("consolidation_jepa_bridge_record: "
                           "train_step rc=%d", rc);
        return -1;
    }

    b->n_steps  += 1u;
    b->last_loss = step_loss;
    if (loss_out) {
        *loss_out = step_loss;
    }
    NIMCP_LOGGING_TRACE("consolidation_jepa_bridge_record: "
                        "step=%u loss=%.6f",
                        b->n_steps, b->last_loss);
    return 0;
}

/*============================================================================
 * Forward prediction
 *==========================================================================*/

int consolidation_jepa_bridge_predict_outcome(consolidation_jepa_bridge_t* b,
                                              const float* pre_embed,
                                              uint32_t dim,
                                              float* out_pred) {
    if (!b || !pre_embed || !out_pred) {
        return -1;
    }
    if (dim != b->embed_dim) {
        NIMCP_LOGGING_WARN("consolidation_jepa_bridge_predict_outcome: "
                           "dim mismatch (got %u, expected %u)",
                           dim, b->embed_dim);
        return -1;
    }

    if (jepa_latent_set_embedding(b->ctx_latent,
                                  pre_embed,
                                  b->embed_dim) != 0) {
        NIMCP_LOGGING_WARN("consolidation_jepa_bridge_predict_outcome: "
                           "ctx set failed");
        return -1;
    }

    if (jepa_predictor_predict(b->predictor,
                               b->ctx_latent,
                               b->tgt_latent) != 0) {
        NIMCP_LOGGING_WARN("consolidation_jepa_bridge_predict_outcome: "
                           "predict failed");
        return -1;
    }

    int copied = jepa_latent_get_embedding(b->tgt_latent,
                                           out_pred,
                                           b->embed_dim);
    if (copied < 0 || (uint32_t)copied != b->embed_dim) {
        NIMCP_LOGGING_WARN("consolidation_jepa_bridge_predict_outcome: "
                           "get_embedding copied=%d (expected %u)",
                           copied, b->embed_dim);
        return -1;
    }
    return 0;
}

/*============================================================================
 * Quality scoring
 *==========================================================================*/

float consolidation_jepa_bridge_quality(consolidation_jepa_bridge_t* b,
                                        const float* pre_embed,
                                        const float* observed_post,
                                        uint32_t dim) {
    if (!b || !pre_embed || !observed_post) {
        return 0.0f;
    }
    if (dim != b->embed_dim) {
        NIMCP_LOGGING_WARN("consolidation_jepa_bridge_quality: dim mismatch "
                           "(got %u, expected %u)", dim, b->embed_dim);
        return 0.0f;
    }

    /* Stack-allocated scratch for common embed_dim values; heap fallback
     * for anything larger. */
    float  stack_scratch[CONSOLIDATION_JEPA_BRIDGE_STACK_SCRATCH];
    float* pred = NULL;
    bool   pred_on_heap = false;

    if (b->embed_dim <= CONSOLIDATION_JEPA_BRIDGE_STACK_SCRATCH) {
        pred = stack_scratch;
    } else {
        pred = (float*)nimcp_calloc(b->embed_dim, sizeof(float));
        if (!pred) {
            NIMCP_LOGGING_ERROR("consolidation_jepa_bridge_quality: "
                                "scratch alloc failed (embed_dim=%u)",
                                b->embed_dim);
            return 0.0f;
        }
        pred_on_heap = true;
    }

    if (consolidation_jepa_bridge_predict_outcome(b,
                                                  pre_embed,
                                                  dim,
                                                  pred) != 0) {
        if (pred_on_heap) {
            nimcp_free(pred);
        }
        return 0.0f;
    }

    /* Mean-squared error between JEPA prediction and observed post. */
    double sse = 0.0;
    for (uint32_t i = 0; i < b->embed_dim; ++i) {
        double d = (double)pred[i] - (double)observed_post[i];
        sse += d * d;
    }
    double mse = sse / (double)b->embed_dim;

    if (pred_on_heap) {
        nimcp_free(pred);
    }

    /* Guard against NaN/Inf from upstream numerical issues — treat as
     * maximally-surprising (score → 0). */
    if (!isfinite(mse) || mse < 0.0) {
        return 0.0f;
    }

    float quality = (float)(1.0 / (1.0 + mse));
    return quality;
}

/*============================================================================
 * Accessors
 *==========================================================================*/

uint32_t consolidation_jepa_bridge_n_steps(const consolidation_jepa_bridge_t* b) {
    return b ? b->n_steps : 0u;
}

float consolidation_jepa_bridge_last_loss(const consolidation_jepa_bridge_t* b) {
    return b ? b->last_loss : 0.0f;
}

int consolidation_jepa_bridge_tick_from_vec(consolidation_jepa_bridge_t* b,
                                             const float* vec,
                                             uint32_t dim) {
    if (!b || !vec || dim == 0 || !b->prev_embed) return -1;
    float scratch_stack[1024];
    float* work = (b->embed_dim <= 1024) ? scratch_stack
        : (float*)nimcp_calloc(b->embed_dim, sizeof(float));
    if (!work) return -1;
    uint32_t copy = dim < b->embed_dim ? dim : b->embed_dim;
    if (copy > 0) memcpy(work, vec, sizeof(float) * copy);
    if (copy < b->embed_dim) {
        memset(work + copy, 0, sizeof(float) * (b->embed_dim - copy));
    }
    int rc = -1;
    if (b->has_prev) {
        rc = consolidation_jepa_bridge_record(b, b->prev_embed, work,
                                               b->embed_dim, NULL);
    }
    memcpy(b->prev_embed, work, sizeof(float) * b->embed_dim);
    b->has_prev = true;
    if (work != scratch_stack) nimcp_free(work);
    return rc;
}
