/**
 * @file nimcp_snn_homeo_math.h
 * @brief Per-neuron homeostasis math shared by the CUDA kernels and the
 *        CPU emulation of the device-resident homeostasis API (2026-11-05).
 *
 * One definition of each update so the device path and its CPU emulation
 * cannot drift apart. Mirrors the host reference: snn_adaptation_update
 * (AHP / pump) and the inline IP + depression loop of snn_cb_post_spike_pop.
 */
#ifndef NIMCP_SNN_HOMEO_MATH_H
#define NIMCP_SNN_HOMEO_MATH_H

#include <stdbool.h>
#include <stdint.h>

#include "gpu/snn/nimcp_snn_gpu.h"

#ifdef __CUDACC__
#define NIMCP_HOMEO_HD static __host__ __device__ __forceinline__
#else
#define NIMCP_HOMEO_HD static inline
#endif

/** Population of neuron @p i: the last p with off[p] <= i (empty pops skipped). */
NIMCP_HOMEO_HD uint32_t nimcp_snn_homeo_pop_of(const uint32_t* off, uint32_t n_pops,
                                               uint32_t i) {
    uint32_t lo = 0, hi = n_pops;
    while (hi - lo > 1) {
        const uint32_t mid = (lo + hi) / 2;
        if (off[mid] <= i) lo = mid; else hi = mid;
    }
    return lo;
}

/** Input current delta before the LIF step: basket - hyperpolarization × pump_factor. */
NIMCP_HOMEO_HD float nimcp_snn_homeo_input_delta(const nimcp_snn_homeo_pop_params_t* pp,
                                                 float ahp_var, float pump_var,
                                                 float pump_factor) {
    float hyp = 0.0f;
    if (pp->flags & NIMCP_SNN_HOMEO_AHP)  hyp += pp->ahp_gain * ahp_var;
    if (pp->flags & NIMCP_SNN_HOMEO_PUMP) hyp += pp->pump_gain * pump_var;
    return pp->basket - hyp * pump_factor;
}

/** Post-spike update of one neuron's state. */
NIMCP_HOMEO_HD void nimcp_snn_homeo_post_neuron(const nimcp_snn_homeo_pop_params_t* pp,
                                                const nimcp_snn_homeo_globals_t* g,
                                                bool fired,
                                                float* ahp_var, float* pump_var,
                                                float* rate_ema, float* thresh_offset,
                                                float* depression) {
    if (pp->flags & NIMCP_SNN_HOMEO_AHP) {
        float v = *ahp_var * pp->ahp_decay;
        if (fired) v += pp->ahp_bump;
        *ahp_var = v;
    }
    if (pp->flags & NIMCP_SNN_HOMEO_PUMP) {
        float v = *pump_var * pp->pump_decay;
        if (fired) v += pp->pump_bump;
        *pump_var = v;
    }
    if (pp->flags & NIMCP_SNN_HOMEO_IP) {
        const float f = fired ? 1.0f : 0.0f;
        const float r = (1.0f - g->ip_alpha) * *rate_ema + g->ip_alpha * f;
        *rate_ema = r;
        float t = *thresh_offset + g->ip_gain * (r - pp->ip_target);
        if (t > g->ip_cap)  t = g->ip_cap;
        if (t < -g->ip_cap) t = -g->ip_cap;
        *thresh_offset = t;
        float d = *depression * g->dep_decay;
        if (fired) {
            d += g->dep_inc;
            if (d > g->dep_cap) d = g->dep_cap;
        }
        *depression = d;
    }
}

#endif /* NIMCP_SNN_HOMEO_MATH_H */
