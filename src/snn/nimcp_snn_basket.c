/**
 * @file nimcp_snn_basket.c
 * @brief Fast-spiking basket cell pool implementation.
 */

#include "snn/nimcp_snn_basket.h"
#include "utils/memory/nimcp_memory.h"

#include <string.h>
#include <stdint.h>

/* Fast-spiking interneuron defaults — shorter tau, lower threshold,
 * shorter refractory than pyramidal cells. */
#define BASKET_TAU_MEM_MS        5.0f
#define BASKET_T_REF_MS          1.0f
#define BASKET_V_THRESH        -52.0f
#define BASKET_V_RESET         -70.0f
#define BASKET_V_REST          -65.0f
#define BASKET_GAIN_DRIVE       30.0f
#define BASKET_GAIN_INHIB       -3.0f
#define BASKET_TAU_DRIVE_MS     10.0f

#define BASKET_FRACTION_MIN     0.01f
#define BASKET_FRACTION_MAX     0.5f

static void basket_init_state(snn_basket_pool_t* bp) {
    /* Membrane starts at rest, not zero — prevents spurious near-threshold
     * potentials on first step. */
    for (uint32_t i = 0; i < bp->n_cells; ++i) {
        bp->membrane_v[i] = bp->v_rest;
    }
    memset(bp->refractory, 0, sizeof(float) * bp->n_cells);
    memset(bp->spike_output, 0, sizeof(float) * bp->n_cells);
    bp->drive_filtered = 0.0f;
}

snn_basket_pool_t* snn_basket_pool_create(uint32_t parent_pop_id,
                                          uint32_t parent_n_neurons,
                                          float fraction) {
    /* Can't synthesize inhibition for a non-existent parent. */
    if (parent_n_neurons == 0) {
        return NULL;
    }

    if (fraction < BASKET_FRACTION_MIN) fraction = BASKET_FRACTION_MIN;
    if (fraction > BASKET_FRACTION_MAX) fraction = BASKET_FRACTION_MAX;

    uint32_t n_cells = (uint32_t)((float)parent_n_neurons * fraction);
    if (n_cells < 1) n_cells = 1;

    snn_basket_pool_t* bp = (snn_basket_pool_t*)nimcp_malloc(sizeof(snn_basket_pool_t));
    if (!bp) return NULL;
    memset(bp, 0, sizeof(*bp));

    bp->id = 0;
    bp->parent_pop_id = parent_pop_id;
    bp->n_cells = n_cells;

    bp->membrane_v   = (float*)nimcp_malloc(sizeof(float) * n_cells);
    bp->refractory   = (float*)nimcp_malloc(sizeof(float) * n_cells);
    bp->spike_output = (float*)nimcp_malloc(sizeof(float) * n_cells);
    if (!bp->membrane_v || !bp->refractory || !bp->spike_output) {
        snn_basket_pool_destroy(bp);
        return NULL;
    }

    bp->tau_mem_ms = BASKET_TAU_MEM_MS;
    bp->t_ref_ms   = BASKET_T_REF_MS;
    bp->v_thresh   = BASKET_V_THRESH;
    bp->v_reset    = BASKET_V_RESET;
    bp->v_rest     = BASKET_V_REST;

    bp->gain_drive_from_parent = BASKET_GAIN_DRIVE;
    bp->gain_inhib_to_parent   = BASKET_GAIN_INHIB;
    bp->tau_drive_ms           = BASKET_TAU_DRIVE_MS;

    basket_init_state(bp);
    return bp;
}

void snn_basket_pool_destroy(snn_basket_pool_t* bp) {
    if (!bp) return;
    if (bp->membrane_v)   nimcp_free(bp->membrane_v);
    if (bp->refractory)   nimcp_free(bp->refractory);
    if (bp->spike_output) nimcp_free(bp->spike_output);
    nimcp_free(bp);
}

void snn_basket_pool_reset(snn_basket_pool_t* bp) {
    if (!bp) return;
    basket_init_state(bp);
}

void snn_basket_pool_emit_inhibition(const snn_basket_pool_t* bp,
                                     float* parent_I_syn,
                                     uint32_t parent_n) {
    if (!bp || !parent_I_syn || parent_n == 0) return;

    /* Compute basket mean once; uniform feedback to every parent neuron. */
    float sum = 0.0f;
    for (uint32_t i = 0; i < bp->n_cells; ++i) {
        sum += bp->spike_output[i];
    }
    float mean_spike = (bp->n_cells > 0) ? sum / (float)bp->n_cells : 0.0f;
    float delta = bp->gain_inhib_to_parent * mean_spike;

    if (delta == 0.0f) return;

    for (uint32_t n = 0; n < parent_n; ++n) {
        parent_I_syn[n] += delta;
    }
}

void snn_basket_pool_step(snn_basket_pool_t* bp,
                          float parent_mean_fire_rate,
                          float dt_ms) {
    if (!bp || dt_ms <= 0.0f) return;

    /* One-step EMA on parent drive — smooths bursty input from pyramidal pop. */
    float alpha = (bp->tau_drive_ms > 0.0f) ? (dt_ms / bp->tau_drive_ms) : 1.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    bp->drive_filtered += alpha * (parent_mean_fire_rate - bp->drive_filtered);

    float I_input = bp->drive_filtered * bp->gain_drive_from_parent;
    float tau_inv = (bp->tau_mem_ms > 0.0f) ? (dt_ms / bp->tau_mem_ms) : 1.0f;

    for (uint32_t i = 0; i < bp->n_cells; ++i) {
        float v = bp->membrane_v[i];
        float r = bp->refractory[i];

        float dv = (bp->v_rest - v + I_input) * tau_inv;
        v += dv;

        if (v >= bp->v_thresh && r <= 0.0f) {
            v = bp->v_reset;
            r = bp->t_ref_ms;
            bp->spike_output[i] = 1.0f;
        } else {
            bp->spike_output[i] = 0.0f;
        }

        r -= dt_ms;
        if (r < 0.0f) r = 0.0f;

        bp->membrane_v[i] = v;
        bp->refractory[i] = r;
    }
}

float snn_basket_pool_mean_rate(const snn_basket_pool_t* bp) {
    if (!bp || bp->n_cells == 0) return 0.0f;
    float sum = 0.0f;
    for (uint32_t i = 0; i < bp->n_cells; ++i) {
        sum += bp->spike_output[i];
    }
    return sum / (float)bp->n_cells;
}
