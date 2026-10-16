/**
 * @file nimcp_snn_adaptation.h
 * @brief Shared spike-rate adaptation mechanism (AHP and Na/K pump).
 *
 * Single-responsibility module: spike-triggered exponentially-decaying
 * hyperpolarizing current. Used for both fast M-current (tau ~150ms)
 * and slow Na+/K+ pump (tau ~5s) adaptations — the math is identical,
 * only the parameters differ.
 */
#ifndef NIMCP_SNN_ADAPTATION_H
#define NIMCP_SNN_ADAPTATION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct snn_adaptation_state_s {
    uint32_t n_neurons;
    float*   adapt_var;   /* per-neuron adaptation variable; >= 0 */
    float    tau_ms;      /* exponential decay time constant (ms) */
    float    gain_mv;     /* mV of hyperpolarization per unit adapt_var */
    float    spike_bump;  /* increment on spike (usually 1.0) */
} snn_adaptation_state_t;

/**
 * Allocate an adaptation state. Returns NULL on allocation failure.
 * All adapt_var entries start at 0.
 */
snn_adaptation_state_t* snn_adaptation_create(uint32_t n_neurons,
                                              float tau_ms,
                                              float gain_mv,
                                              float spike_bump);

void snn_adaptation_destroy(snn_adaptation_state_t* a);

/** Zero adapt_var without freeing memory. */
void snn_adaptation_reset(snn_adaptation_state_t* a);

/**
 * Fill out_hyperpol_mv[n] = gain_mv × adapt_var[n]. Used by the LIF
 * step BEFORE dV is computed. Caller must pre-allocate a buffer of
 * at least n_neurons floats. The buffer is overwritten (not accumulated).
 *
 * dt_ms is unused here — the decay happens in snn_adaptation_update —
 * but is accepted in the signature for symmetry and future extension.
 */
void snn_adaptation_compute_hyperpol(const snn_adaptation_state_t* a,
                                     float* out_hyperpol_mv,
                                     float  dt_ms);

/**
 * Apply per-step update: decay adapt_var by exp(-dt_ms/tau_ms), then
 * bump adapt_var[n] += spike_bump wherever fired[n] > 0.5. Called AFTER
 * the LIF firing decision so adaptation tracks the spikes that just
 * occurred.
 */
void snn_adaptation_update(snn_adaptation_state_t* a,
                           const float* fired,
                           float dt_ms);

#ifdef __cplusplus
}
#endif
#endif /* NIMCP_SNN_ADAPTATION_H */
