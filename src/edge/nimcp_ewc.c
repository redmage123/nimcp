/**
 * @file nimcp_ewc.c
 * @brief Elastic Weight Consolidation (EWC) — catastrophic forgetting protection
 *
 * WHAT: Protects locally-important weights from being overwritten during
 *       federated weight blending with a master brain.
 * WHY:  Edge devices learn specialized knowledge; naive averaging destroys it.
 * HOW:  Fisher Information diagonal measures per-weight importance.
 *       During blending, high-Fisher weights stay closer to local values.
 */

#include "edge/nimcp_edge.h"
#include "utils/memory/nimcp_memory.h"
#include <string.h>
#include <math.h>

/* ============================================================================
 * Create / Destroy
 * ============================================================================ */

nimcp_ewc_state_t* nimcp_ewc_create(uint32_t num_params, float lambda) {
    if (num_params == 0) {
        return NULL;
    }

    nimcp_ewc_state_t* ewc = (nimcp_ewc_state_t*)nimcp_calloc(1, sizeof(nimcp_ewc_state_t));
    if (!ewc) {
        return NULL;
    }

    ewc->fisher_diagonal = (float*)nimcp_calloc(num_params, sizeof(float));
    if (!ewc->fisher_diagonal) {
        nimcp_free(ewc);
        return NULL;
    }

    ewc->anchor_weights = (float*)nimcp_calloc(num_params, sizeof(float));
    if (!ewc->anchor_weights) {
        nimcp_free(ewc->fisher_diagonal);
        nimcp_free(ewc);
        return NULL;
    }

    ewc->num_params = num_params;
    ewc->ewc_lambda = lambda;
    ewc->initialized = false;

    return ewc;
}

void nimcp_ewc_destroy(nimcp_ewc_state_t* ewc) {
    if (!ewc) {
        return;
    }
    nimcp_free(ewc->fisher_diagonal);
    nimcp_free(ewc->anchor_weights);
    nimcp_free(ewc);
}

/* ============================================================================
 * Fisher Information
 * ============================================================================ */

int nimcp_ewc_compute_fisher(nimcp_ewc_state_t* ewc,
                              const float* gradients, uint32_t num_samples) {
    if (!ewc || !gradients || num_samples == 0) {
        return -1;
    }

    /* F_i = mean(gradient_i^2) over samples.
     * gradients layout: num_samples consecutive arrays of ewc->num_params floats.
     * Accumulate squared gradients then divide by num_samples. */
    memset(ewc->fisher_diagonal, 0, ewc->num_params * sizeof(float));

    for (uint32_t s = 0; s < num_samples; s++) {
        const float* sample_grad = gradients + (size_t)s * ewc->num_params;
        for (uint32_t i = 0; i < ewc->num_params; i++) {
            ewc->fisher_diagonal[i] += sample_grad[i] * sample_grad[i];
        }
    }

    float inv_samples = 1.0f / (float)num_samples;
    for (uint32_t i = 0; i < ewc->num_params; i++) {
        ewc->fisher_diagonal[i] *= inv_samples;
    }

    ewc->initialized = true;
    return 0;
}

/* ============================================================================
 * Anchor Weights
 * ============================================================================ */

int nimcp_ewc_set_anchor(nimcp_ewc_state_t* ewc, const float* weights) {
    if (!ewc || !weights) {
        return -1;
    }

    memcpy(ewc->anchor_weights, weights, ewc->num_params * sizeof(float));
    return 0;
}

/* ============================================================================
 * EWC-Aware Weight Blending
 * ============================================================================ */

int nimcp_ewc_blend_weights(const nimcp_ewc_state_t* ewc,
                             float* local_weights, const float* master_weights,
                             float base_blend_ratio) {
    if (!ewc || !local_weights || !master_weights) {
        return -1;
    }
    if (!ewc->initialized) {
        return -1;
    }

    static const float PENALTY_THRESHOLD = 1.0f;
    static const float HIGH_IMPORTANCE_LOCAL  = 0.9f;  /* 90% local for important weights */
    static const float HIGH_IMPORTANCE_MASTER = 0.1f;  /* 10% master */
    static const float LOW_IMPORTANCE_LOCAL   = 0.3f;  /* 30% local for unimportant weights */
    static const float LOW_IMPORTANCE_MASTER  = 0.7f;  /* 70% master */

    for (uint32_t i = 0; i < ewc->num_params; i++) {
        float diff = master_weights[i] - ewc->anchor_weights[i];
        float penalty = ewc->ewc_lambda * ewc->fisher_diagonal[i] * diff * diff;

        float local_ratio, master_ratio;
        if (penalty > PENALTY_THRESHOLD) {
            /* High importance: keep mostly local */
            local_ratio  = HIGH_IMPORTANCE_LOCAL;
            master_ratio = HIGH_IMPORTANCE_MASTER;
        } else {
            /* Low importance: accept mostly master */
            local_ratio  = LOW_IMPORTANCE_LOCAL;
            master_ratio = LOW_IMPORTANCE_MASTER;
        }

        local_weights[i] = local_ratio * local_weights[i]
                         + master_ratio * master_weights[i];
    }

    (void)base_blend_ratio; /* Reserved for future tuning */
    return 0;
}
