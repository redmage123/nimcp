/**
 * @file nimcp_snn_adaptation.c
 * @brief Implementation of shared spike-rate adaptation (AHP and Na/K pump).
 *
 * Math: adapt_var[n] tracks a per-neuron exponentially-decaying quantity
 * that is bumped on each spike. The hyperpolarizing current applied to
 * the membrane is gain_mv × adapt_var. One struct, two parameter sets
 * (fast AHP ~150ms, slow pump ~5s) — DRY.
 */

#include "snn/nimcp_snn_adaptation.h"
#include "utils/memory/nimcp_memory.h"

#include <math.h>
#include <string.h>

snn_adaptation_state_t* snn_adaptation_create(uint32_t n_neurons,
                                              float tau_ms,
                                              float gain_mv,
                                              float spike_bump) {
    /* Guard: reject invalid parameters. */
    if (n_neurons == 0) {
        return NULL;
    }
    if (!(tau_ms > 0.0f)) {
        return NULL;
    }
    if (gain_mv < 0.0f) {
        return NULL;
    }

    snn_adaptation_state_t* a =
        (snn_adaptation_state_t*)nimcp_malloc(sizeof(snn_adaptation_state_t));
    if (!a) {
        return NULL;
    }

    a->n_neurons  = n_neurons;
    a->tau_ms     = tau_ms;
    a->gain_mv    = gain_mv;
    a->spike_bump = spike_bump;

    /* adapt_var starts at zero — no adaptation before the first spike. */
    a->adapt_var = (float*)nimcp_calloc((size_t)n_neurons, sizeof(float));
    if (!a->adapt_var) {
        nimcp_free(a);
        return NULL;
    }

    return a;
}

void snn_adaptation_destroy(snn_adaptation_state_t* a) {
    if (!a) {
        return;
    }
    if (a->adapt_var) {
        nimcp_free(a->adapt_var);
    }
    nimcp_free(a);
}

void snn_adaptation_reset(snn_adaptation_state_t* a) {
    if (!a) {
        return;
    }
    if (!a->adapt_var) {
        return;
    }
    if (a->n_neurons == 0) {
        return;
    }
    memset(a->adapt_var, 0, (size_t)a->n_neurons * sizeof(float));
}

void snn_adaptation_compute_hyperpol(const snn_adaptation_state_t* a,
                                     float* out_hyperpol_mv,
                                     float  dt_ms) {
    (void)dt_ms;  /* reserved for future use — decay lives in _update */
    if (!a || !out_hyperpol_mv) {
        return;
    }
    if (!a->adapt_var || a->n_neurons == 0) {
        return;
    }

    const float gain = a->gain_mv;
    const uint32_t n = a->n_neurons;
    for (uint32_t i = 0; i < n; ++i) {
        out_hyperpol_mv[i] = gain * a->adapt_var[i];
    }
}

void snn_adaptation_update(snn_adaptation_state_t* a,
                           const float* fired,
                           float dt_ms) {
    if (!a || !fired) {
        return;
    }
    if (!a->adapt_var || a->n_neurons == 0) {
        return;
    }

    /* Decay factor computed once outside the loop. tau_ms guaranteed > 0
     * by the constructor, so division is safe. */
    const float decay = expf(-dt_ms / a->tau_ms);
    const float bump  = a->spike_bump;
    const uint32_t n  = a->n_neurons;

    for (uint32_t i = 0; i < n; ++i) {
        float v = a->adapt_var[i] * decay;
        if (fired[i] > 0.5f) {
            v += bump;
        }
        a->adapt_var[i] = v;
    }
}
