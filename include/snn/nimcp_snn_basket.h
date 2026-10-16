/**
 * @file nimcp_snn_basket.h
 * @brief First-class basket cell pool — fast-spiking inhibitory interneurons.
 *
 * One basket_pool attached per excitatory population. Its cells receive
 * aggregate drive from the parent population's mean firing rate and
 * project strong uniform inhibition back to every neuron in the parent.
 * This approximates the PV+ basket cell inhibitory micro-circuit
 * (biological cortex, ~20% of neurons) at population scale.
 *
 * Narrow interface: network step never touches basket internals.
 */
#ifndef NIMCP_SNN_BASKET_H
#define NIMCP_SNN_BASKET_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct snn_basket_pool_s {
    uint32_t id;
    uint32_t parent_pop_id;
    uint32_t n_cells;

    /* Basket-cell LIF state — owned. */
    float* membrane_v;      /* [n_cells] */
    float* refractory;      /* [n_cells] ms remaining */
    float* spike_output;    /* [n_cells] binary (0.0 or 1.0) */

    /* Parameters — defaults chosen for fast-spiking interneuron profile. */
    float tau_mem_ms;       /* default 5.0 */
    float t_ref_ms;         /* default 1.0 */
    float v_thresh;         /* default -52.0 */
    float v_reset;          /* default -70.0 */
    float v_rest;           /* default -65.0 */

    /* Coupling gains. */
    float gain_drive_from_parent; /* mV depolarization per unit parent-mean-rate */
    float gain_inhib_to_parent;   /* mV to add to parent I_syn per unit basket mean spike */
    float tau_drive_ms;           /* EMA smoothing of parent rate input */
    float drive_filtered;         /* internal EMA state */
} snn_basket_pool_t;

/**
 * Allocate a basket pool. n_cells = max(1, parent_n_neurons × fraction).
 * fraction clamped to [0.01, 0.5]. Defaults come from snn_basket_defaults().
 * Returns NULL on alloc failure.
 */
snn_basket_pool_t* snn_basket_pool_create(uint32_t parent_pop_id,
                                          uint32_t parent_n_neurons,
                                          float fraction);

void snn_basket_pool_destroy(snn_basket_pool_t* bp);

/** Reset membrane/refractory/spikes to rest, drive_filtered to zero. */
void snn_basket_pool_reset(snn_basket_pool_t* bp);

/**
 * Called BEFORE parent pop LIF step. For each neuron in the parent pop,
 * add gain_inhib_to_parent × mean_basket_spike_output to parent_I_syn[n].
 * mean_basket_spike_output is basket's mean spike vector from the PREVIOUS
 * basket step. Uniform feedback (same value to every parent neuron).
 *
 * gain_inhib_to_parent is negative — the function does not flip sign.
 * If basket has not yet been stepped (cold start), injects zero.
 */
void snn_basket_pool_emit_inhibition(const snn_basket_pool_t* bp,
                                     float* parent_I_syn,
                                     uint32_t parent_n);

/**
 * Called AFTER parent pop LIF step. Updates drive_filtered by one-step
 * EMA on parent_mean_fire_rate, computes each basket cell's LIF update
 * using drive_filtered × gain_drive_from_parent as the input current,
 * emits spikes where v >= v_thresh. Respects refractory per-cell.
 *
 * parent_mean_fire_rate is the fraction of parent neurons that fired
 * this step, i.e. in [0, 1].
 */
void snn_basket_pool_step(snn_basket_pool_t* bp,
                          float parent_mean_fire_rate,
                          float dt_ms);

/** Diagnostic: fraction of basket cells that fired on the last step. */
float snn_basket_pool_mean_rate(const snn_basket_pool_t* bp);

#ifdef __cplusplus
}
#endif
#endif /* NIMCP_SNN_BASKET_H */
