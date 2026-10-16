/**
 * @file nimcp_neuromod_jepa_bridge.c
 * @brief JEPA bridge for the neuromodulator system -- implementation.
 *
 * See include/plasticity/neuromodulators/nimcp_neuromod_jepa_bridge.h for
 * the API contract and rationale.
 *
 * Design notes (keep in sync with octopus Phase 4m):
 *  - Single shared jepa_predictor_t + two shared jepa_latent_t buffers.
 *    Reused every training step -- hot path is zero-alloc.
 *  - Channels are slotted stably as:
 *      [0] = NEUROMOD_DOPAMINE
 *      [1] = NEUROMOD_SEROTONIN
 *      [2] = NEUROMOD_ACETYLCHOLINE
 *      [3] = NEUROMOD_NOREPINEPHRINE
 *      [4] = NEUROMOD_GABA
 *      [5] = NEUROMOD_GLUTAMATE
 *    Changing this order is a breaking change: the JEPA predictor's learned
 *    weights encode the channel identity.
 *  - All allocation flows through nimcp_calloc / nimcp_free. No raw malloc.
 *  - Levels are defensively clamped to [0,1] before being handed to JEPA.
 */

#include "plasticity/neuromodulators/nimcp_neuromod_jepa_bridge.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
#include "cognitive/jepa/nimcp_jepa_predictor.h"
#include "cognitive/jepa/nimcp_jepa_latent.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

/*============================================================================
 * Private struct
 *==========================================================================*/

struct neuromod_jepa_bridge_s {
    /* Non-owning reference to the neuromodulator system. */
    neuromodulator_system_t sys;

    /* JEPA machinery -- owned. */
    jepa_predictor_t* predictor;
    jepa_latent_t*    ctx_latent;
    jepa_latent_t*    tgt_latent;

    /* Rolling previous-step snapshot and gate. */
    float   prev_levels[NIMCP_NEUROMOD_JEPA_DIM];
    bool    has_prev;

    /* Stats. */
    uint32_t n_steps;
    float    last_loss;
};

/*============================================================================
 * Private helpers
 *==========================================================================*/

/* Fixed channel -> enum mapping. Matches the first six NEUROMOD_* values. */
static const neuromodulator_type_t k_channel_to_type[NIMCP_NEUROMOD_JEPA_DIM] = {
    NEUROMOD_DOPAMINE,       /* [0] DA  */
    NEUROMOD_SEROTONIN,      /* [1] 5-HT */
    NEUROMOD_ACETYLCHOLINE,  /* [2] ACh */
    NEUROMOD_NOREPINEPHRINE, /* [3] NE  */
    NEUROMOD_GABA,           /* [4] GABA */
    NEUROMOD_GLUTAMATE,      /* [5] GLU */
};

static inline float _clamp01(float x) {
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    /* NaN falls through both comparisons; treat as 0 for safety. */
    if (!(x == x)) return 0.0f;
    return x;
}

/* Populate `out` with the 6 current levels, each clamped to [0,1]. */
static void _read_levels(neuromodulator_system_t sys,
                         float out[NIMCP_NEUROMOD_JEPA_DIM]) {
    for (uint32_t i = 0; i < NIMCP_NEUROMOD_JEPA_DIM; i++) {
        float v = neuromodulator_get_level(sys, k_channel_to_type[i]);
        out[i] = _clamp01(v);
    }
}

/*============================================================================
 * Public API
 *==========================================================================*/

neuromod_jepa_bridge_t* neuromod_jepa_bridge_create(neuromodulator_system_t sys) {
    if (!sys) {
        NIMCP_LOGGING_WARN("neuromod_jepa_bridge_create: NULL neuromodulator system");
        return NULL;
    }

    neuromod_jepa_bridge_t* b = (neuromod_jepa_bridge_t*)nimcp_calloc(
        1, sizeof(neuromod_jepa_bridge_t));
    if (!b) {
        NIMCP_LOGGING_ERROR("neuromod_jepa_bridge_create: calloc(bridge) failed");
        return NULL;
    }

    b->sys       = sys;
    b->has_prev  = false;
    b->n_steps   = 0;
    b->last_loss = 0.0f;

    /* Configure the JEPA predictor: 6->6 with 6-dim hidden, 1 layer,
     * lr=1e-3, wd=1e-5, layer-norm on, FEP off. Start from the module's
     * defaults so any new fields get sane values, then override. */
    jepa_predictor_config_t jcfg = {0};
    (void)jepa_predictor_default_config(&jcfg);
    jcfg.input_dim         = NIMCP_NEUROMOD_JEPA_DIM;
    jcfg.output_dim        = NIMCP_NEUROMOD_JEPA_DIM;
    jcfg.hidden_dim        = NIMCP_NEUROMOD_JEPA_DIM;
    jcfg.num_layers        = 1;
    jcfg.learning_rate     = 1e-3f;
    jcfg.weight_decay      = 1e-5f;
    jcfg.dropout_rate      = 0.0f;
    jcfg.enable_layer_norm = true;
    jcfg.enable_fep        = false;

    b->predictor = jepa_predictor_create(&jcfg);
    if (!b->predictor) {
        NIMCP_LOGGING_ERROR("neuromod_jepa_bridge_create: jepa_predictor_create failed");
        nimcp_free(b);
        return NULL;
    }
    (void)jepa_predictor_set_training(b->predictor, true);

    b->ctx_latent = jepa_latent_create_dim(NIMCP_NEUROMOD_JEPA_DIM);
    if (!b->ctx_latent) {
        NIMCP_LOGGING_ERROR("neuromod_jepa_bridge_create: ctx_latent alloc failed");
        jepa_predictor_destroy(b->predictor);
        nimcp_free(b);
        return NULL;
    }

    b->tgt_latent = jepa_latent_create_dim(NIMCP_NEUROMOD_JEPA_DIM);
    if (!b->tgt_latent) {
        NIMCP_LOGGING_ERROR("neuromod_jepa_bridge_create: tgt_latent alloc failed");
        jepa_latent_destroy(b->ctx_latent);
        jepa_predictor_destroy(b->predictor);
        nimcp_free(b);
        return NULL;
    }

    NIMCP_LOGGING_INFO("neuromod_jepa_bridge_create: live (dim=%u, 1 hidden layer, "
                       "lr=1e-3, wd=1e-5, layer_norm=on, fep=off)",
                       (unsigned)NIMCP_NEUROMOD_JEPA_DIM);
    return b;
}

void neuromod_jepa_bridge_destroy(neuromod_jepa_bridge_t* b) {
    if (!b) return;

    NIMCP_LOGGING_INFO("neuromod_jepa_bridge_destroy: n_steps=%u last_loss=%.6f",
                       b->n_steps, b->last_loss);

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
    /* sys is non-owning -- do NOT destroy. */
    b->sys = NULL;

    nimcp_free(b);
}

int neuromod_jepa_bridge_train_step(neuromod_jepa_bridge_t* b, float* loss_out) {
    if (loss_out) *loss_out = 0.0f;
    if (!b || !b->sys || !b->predictor || !b->ctx_latent || !b->tgt_latent) {
        return -1;
    }

    /* Read current levels (clamped). */
    float cur_levels[NIMCP_NEUROMOD_JEPA_DIM];
    _read_levels(b->sys, cur_levels);

    /* First-call path: capture the snapshot, no training yet. */
    if (!b->has_prev) {
        for (uint32_t i = 0; i < NIMCP_NEUROMOD_JEPA_DIM; i++) {
            b->prev_levels[i] = cur_levels[i];
        }
        b->has_prev = true;
        NIMCP_LOGGING_TRACE("neuromod_jepa_bridge_train_step: first call, "
                            "captured prev snapshot only");
        return -1;
    }

    /* Wire up the JEPA latents for this step. */
    if (jepa_latent_set_embedding(b->ctx_latent, b->prev_levels,
                                   NIMCP_NEUROMOD_JEPA_DIM) != 0) {
        NIMCP_LOGGING_WARN("neuromod_jepa_bridge_train_step: ctx set_embedding failed");
        /* Still roll the snapshot forward so we don't freeze on a bad state. */
        for (uint32_t i = 0; i < NIMCP_NEUROMOD_JEPA_DIM; i++) {
            b->prev_levels[i] = cur_levels[i];
        }
        return -1;
    }
    if (jepa_latent_set_embedding(b->tgt_latent, cur_levels,
                                   NIMCP_NEUROMOD_JEPA_DIM) != 0) {
        NIMCP_LOGGING_WARN("neuromod_jepa_bridge_train_step: tgt set_embedding failed");
        for (uint32_t i = 0; i < NIMCP_NEUROMOD_JEPA_DIM; i++) {
            b->prev_levels[i] = cur_levels[i];
        }
        return -1;
    }

    float step_loss = 0.0f;
    int rc = jepa_predictor_train_step(b->predictor, b->ctx_latent, b->tgt_latent,
                                        &step_loss);

    /* Always advance the prev snapshot: whether the train step succeeded
     * or not, the chemical trajectory has moved forward and we don't want
     * to re-use stale prev next time. */
    for (uint32_t i = 0; i < NIMCP_NEUROMOD_JEPA_DIM; i++) {
        b->prev_levels[i] = cur_levels[i];
    }

    if (rc != 0) {
        NIMCP_LOGGING_WARN("neuromod_jepa_bridge_train_step: "
                           "jepa_predictor_train_step rc=%d", rc);
        return -1;
    }

    b->n_steps++;
    b->last_loss = step_loss;
    if (loss_out) *loss_out = step_loss;

    NIMCP_LOGGING_TRACE("neuromod_jepa_bridge_train_step: step=%u loss=%.6f",
                        b->n_steps, step_loss);
    return 0;
}

uint32_t neuromod_jepa_bridge_n_steps(const neuromod_jepa_bridge_t* b) {
    if (!b) return 0;
    return b->n_steps;
}

float neuromod_jepa_bridge_last_loss(const neuromod_jepa_bridge_t* b) {
    if (!b) return 0.0f;
    return b->last_loss;
}
