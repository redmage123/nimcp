/**
 * @file nimcp_omni_jepa_bridge.c
 * @brief Omni World Model — JEPA Latent-Space Predictor Bridge (impl)
 *
 * Implementation mirrors the octopus Phase 4m integration pattern
 * (`src/cognitive/octopus/nimcp_octopus.c`): a single `jepa_predictor_t`
 * plus two reusable `jepa_latent_t` buffers driven by
 * `jepa_predictor_train_step`. All allocations go through
 * `nimcp_calloc` / `nimcp_free` per project memory policy.
 */

#include "cognitive/omni/nimcp_omni_jepa_bridge.h"
#include "cognitive/jepa/nimcp_jepa_predictor.h"
#include "cognitive/jepa/nimcp_jepa_latent.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#include <stdbool.h>
#include <string.h>

/* ============================================================================
 * Internal struct
 * ============================================================================
 *
 * Kept deliberately small; the bridge is a pass-through wrapper. The two
 * latent buffers are reused across calls to avoid per-step heap churn.
 */
struct omni_jepa_bridge_s {
    jepa_predictor_t* predictor;   /* owned */
    jepa_latent_t*    ctx_latent;  /* owned, reused as context / input */
    jepa_latent_t*    tgt_latent;  /* owned, reused as target / prediction */
    uint32_t          state_dim;
    uint32_t          n_steps;
    float             last_loss;
    /* Self-drive rolling prev embed (owned). */
    float*            prev_embed;
    bool              has_prev;
};

/* ============================================================================
 * Lifecycle
 * ========================================================================= */

omni_jepa_bridge_t* omni_jepa_bridge_create(uint32_t state_dim) {
    if (state_dim == 0) {
        NIMCP_LOGGING_WARN("omni_jepa_bridge_create: state_dim must be > 0");
        return NULL;
    }

    omni_jepa_bridge_t* b =
        (omni_jepa_bridge_t*)nimcp_calloc(1, sizeof(omni_jepa_bridge_t));
    if (!b) {
        NIMCP_LOGGING_WARN("omni_jepa_bridge_create: alloc failed");
        return NULL;
    }

    b->state_dim = state_dim;
    b->n_steps   = 0;
    b->last_loss = 0.0f;

    /* Configure the predictor per the Phase 4m template:
     *   input = output = hidden = state_dim, 1 hidden layer,
     *   lr=1e-3, wd=1e-5, no dropout, layer-norm on, FEP off. */
    jepa_predictor_config_t jcfg;
    memset(&jcfg, 0, sizeof(jcfg));
    (void)jepa_predictor_default_config(&jcfg);
    jcfg.input_dim         = state_dim;
    jcfg.output_dim        = state_dim;
    jcfg.hidden_dim        = state_dim;
    jcfg.num_layers        = 1;
    jcfg.learning_rate     = 1e-3f;
    jcfg.weight_decay      = 1e-5f;
    jcfg.dropout_rate      = 0.0f;
    jcfg.enable_layer_norm = true;
    jcfg.enable_fep        = false;

    b->predictor = jepa_predictor_create(&jcfg);
    if (!b->predictor) {
        NIMCP_LOGGING_WARN("omni_jepa_bridge_create: predictor create failed "
                           "(state_dim=%u)", state_dim);
        nimcp_free(b);
        return NULL;
    }

    /* Put predictor in training mode by default; consumers that only want
     * forward-only inference can ignore this — predict() does not depend
     * on the training flag. */
    (void)jepa_predictor_set_training(b->predictor, true);

    b->ctx_latent = jepa_latent_create_dim(state_dim);
    b->tgt_latent = jepa_latent_create_dim(state_dim);
    b->prev_embed = (float*)nimcp_calloc(state_dim, sizeof(float));
    if (!b->ctx_latent || !b->tgt_latent || !b->prev_embed) {
        NIMCP_LOGGING_WARN("omni_jepa_bridge_create: latent/prev alloc "
                           "failed (state_dim=%u)", state_dim);
        if (b->ctx_latent) jepa_latent_destroy(b->ctx_latent);
        if (b->tgt_latent) jepa_latent_destroy(b->tgt_latent);
        if (b->prev_embed) nimcp_free(b->prev_embed);
        jepa_predictor_destroy(b->predictor);
        nimcp_free(b);
        return NULL;
    }

    NIMCP_LOGGING_DEBUG("omni_jepa_bridge_create: live (state_dim=%u, "
                        "layers=1, lr=1e-3, wd=1e-5, layer_norm=on, fep=off)",
                        state_dim);
    return b;
}

void omni_jepa_bridge_destroy(omni_jepa_bridge_t* b) {
    if (!b) return;

    NIMCP_LOGGING_DEBUG("omni_jepa_bridge_destroy: state_dim=%u n_steps=%u "
                        "last_loss=%.4f",
                        b->state_dim, b->n_steps, b->last_loss);

    if (b->ctx_latent) {
        jepa_latent_destroy(b->ctx_latent);
        b->ctx_latent = NULL;
    }
    if (b->tgt_latent) {
        jepa_latent_destroy(b->tgt_latent);
        b->tgt_latent = NULL;
    }
    if (b->predictor) {
        jepa_predictor_destroy(b->predictor);
        b->predictor = NULL;
    }
    if (b->prev_embed) {
        nimcp_free(b->prev_embed);
        b->prev_embed = NULL;
    }
    nimcp_free(b);
}

/* ============================================================================
 * Training / prediction
 * ========================================================================= */

int omni_jepa_bridge_train_step(omni_jepa_bridge_t* b,
                                const float* prev_state,
                                const float* cur_state,
                                uint32_t dim,
                                float* loss_out) {
    if (!b || !b->predictor || !b->ctx_latent || !b->tgt_latent) {
        NIMCP_LOGGING_WARN("omni_jepa_bridge_train_step: invalid bridge");
        if (loss_out) *loss_out = 0.0f;
        return -1;
    }
    if (!prev_state || !cur_state) {
        NIMCP_LOGGING_WARN("omni_jepa_bridge_train_step: null state vector");
        if (loss_out) *loss_out = 0.0f;
        return -1;
    }
    if (dim != b->state_dim) {
        NIMCP_LOGGING_WARN("omni_jepa_bridge_train_step: dim mismatch "
                           "(got=%u expected=%u)", dim, b->state_dim);
        if (loss_out) *loss_out = 0.0f;
        return -1;
    }

    /* Copy raw vectors into the reusable latent buffers. Both calls use
     * the bridge's own state_dim so there is no chance of over-read. */
    if (jepa_latent_set_embedding(b->ctx_latent, prev_state, b->state_dim) != 0) {
        NIMCP_LOGGING_WARN("omni_jepa_bridge_train_step: ctx set_embedding failed");
        if (loss_out) *loss_out = 0.0f;
        return -1;
    }
    if (jepa_latent_set_embedding(b->tgt_latent, cur_state, b->state_dim) != 0) {
        NIMCP_LOGGING_WARN("omni_jepa_bridge_train_step: tgt set_embedding failed");
        if (loss_out) *loss_out = 0.0f;
        return -1;
    }

    float step_loss = 0.0f;
    if (jepa_predictor_train_step(b->predictor, b->ctx_latent,
                                  b->tgt_latent, &step_loss) != 0) {
        NIMCP_LOGGING_WARN("omni_jepa_bridge_train_step: predictor train_step "
                           "failed (n_steps=%u)", b->n_steps);
        if (loss_out) *loss_out = 0.0f;
        return -1;
    }

    b->n_steps   += 1;
    b->last_loss  = step_loss;
    if (loss_out) *loss_out = step_loss;

    NIMCP_LOGGING_DEBUG("omni_jepa_bridge_train_step: step=%u loss=%.4f",
                        b->n_steps, step_loss);
    return 0;
}

int omni_jepa_bridge_predict(omni_jepa_bridge_t* b,
                             const float* state,
                             uint32_t dim,
                             float* out_pred) {
    if (!b || !b->predictor || !b->ctx_latent || !b->tgt_latent) {
        NIMCP_LOGGING_WARN("omni_jepa_bridge_predict: invalid bridge");
        return -1;
    }
    if (!state || !out_pred) {
        NIMCP_LOGGING_WARN("omni_jepa_bridge_predict: null state/out buffer");
        return -1;
    }
    if (dim != b->state_dim) {
        NIMCP_LOGGING_WARN("omni_jepa_bridge_predict: dim mismatch "
                           "(got=%u expected=%u)", dim, b->state_dim);
        return -1;
    }

    if (jepa_latent_set_embedding(b->ctx_latent, state, b->state_dim) != 0) {
        NIMCP_LOGGING_WARN("omni_jepa_bridge_predict: ctx set_embedding failed");
        return -1;
    }

    /* Forward only — reuse tgt_latent as the prediction output sink. */
    if (jepa_predictor_predict(b->predictor, b->ctx_latent, b->tgt_latent) != 0) {
        NIMCP_LOGGING_WARN("omni_jepa_bridge_predict: predictor forward failed");
        return -1;
    }

    if (jepa_latent_get_embedding(b->tgt_latent, out_pred, b->state_dim) != 0) {
        NIMCP_LOGGING_WARN("omni_jepa_bridge_predict: get_embedding failed");
        return -1;
    }

    NIMCP_LOGGING_DEBUG("omni_jepa_bridge_predict: dim=%u ok", b->state_dim);
    return 0;
}

/* ============================================================================
 * Stats accessors
 * ========================================================================= */

uint32_t omni_jepa_bridge_n_steps(const omni_jepa_bridge_t* b) {
    if (!b) return 0;
    return b->n_steps;
}

float omni_jepa_bridge_last_loss(const omni_jepa_bridge_t* b) {
    if (!b) return 0.0f;
    return b->last_loss;
}

/* Copy up to b->state_dim floats from src (of length src_dim) into dst.
 * Truncates if src_dim > state_dim, zero-pads the tail if src_dim <
 * state_dim. Keeps the state_dim-shaped latent buffers consistent even
 * when the caller ticks with a differently-sized vector. */
static void _omni_copy_clamped(const omni_jepa_bridge_t* b,
                                const float* src, uint32_t src_dim,
                                float* dst) {
    uint32_t copy = src_dim < b->state_dim ? src_dim : b->state_dim;
    if (copy > 0) memcpy(dst, src, sizeof(float) * copy);
    if (copy < b->state_dim) {
        memset(dst + copy, 0, sizeof(float) * (b->state_dim - copy));
    }
}

int omni_jepa_bridge_tick_from_vec(omni_jepa_bridge_t* b,
                                    const float* cur,
                                    uint32_t dim) {
    if (!b || !cur || dim == 0 || !b->prev_embed) return -1;

    /* Pack caller vec into state_dim-shaped scratch. Stack-alloc cap 1024. */
    float scratch_stack[1024];
    float* work = (b->state_dim <= 1024) ? scratch_stack
        : (float*)nimcp_calloc(b->state_dim, sizeof(float));
    if (!work) return -1;
    _omni_copy_clamped(b, cur, dim, work);

    int rc = -1;
    if (b->has_prev) {
        rc = omni_jepa_bridge_train_step(b, b->prev_embed, work,
                                          b->state_dim, NULL);
    }
    memcpy(b->prev_embed, work, sizeof(float) * b->state_dim);
    b->has_prev = true;
    if (work != scratch_stack) nimcp_free(work);
    return rc;
}
