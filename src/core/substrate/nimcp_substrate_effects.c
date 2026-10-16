/**
 * @file nimcp_substrate_effects.c
 * @brief Shared substrate-to-effects computation + activity debit.
 *
 * Network-agnostic helper consumed by SNN/LNN/CNN/FNO/HNN adapters. Reads
 * the substrate state (ATP, temperature, ion gradient, membrane integrity)
 * and emits axon_substrate_effects_t + dendrite_substrate_effects_t using
 * biologically-motivated formulas (Q10 for temperature, linear/min for ATP).
 *
 * Formulas chosen so that at baseline (atp=1.0, T=37°C, ion=1.0, mem=1.0)
 * every multiplier collapses to its "no effect" value (1.0, or 0.5+0.5×1×1=1.0
 * for spike_reliability).
 */

#include "core/substrate/nimcp_substrate_effects.h"
#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"

#include <math.h>
#include <string.h>

/* Physiologic temperature bounds (°C). Values outside this range are either
 * non-survivable in vivo or have no sensible Q10 interpretation. Clamp here
 * so that downstream formulas (e.g. spike_threshold_mod = nmda * (2 - q10))
 * receive a bounded temperature regardless of sensor/config noise. */
#define SUBSTRATE_EFFECTS_TEMP_MIN_C          20.0f
#define SUBSTRATE_EFFECTS_TEMP_MAX_C          45.0f

/* Q10 coefficient for generic channel kinetics — Hodgkin & Huxley (1952). */
#define SUBSTRATE_EFFECTS_Q10                 2.3f
#define SUBSTRATE_EFFECTS_REFERENCE_TEMP_C    37.0f

/* ATP cost per spike (normalized [0,1] substrate ATP pool). */
#define SUBSTRATE_EFFECTS_ATP_PER_SPIKE       1.0e-8f
#define SUBSTRATE_EFFECTS_ATP_PER_PLASTICITY  5.0e-8f

static inline float clampf(float x, float lo, float hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

nimcp_error_t substrate_compute_effects(
    const neural_substrate_t*     substrate,
    axon_substrate_effects_t*     out_axon,
    dendrite_substrate_effects_t* out_dend)
{
    if (!substrate) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Read substrate state. We access fields directly (const, no mutex);
     * single-reader snapshot is acceptable for a per-step computation. */
    float atp   = clampf(substrate->metabolic.atp_level,       0.0f, 1.0f);
    /* Clamp temperature to physiologic range before computing Q10, so that
     * extreme/sensor-noise temperature values don't produce unbounded q10
     * values which downstream formulas (e.g. spike_threshold_mod) consume
     * pre-clamp. See Bug #5. */
    float tempC = clampf(substrate->physical.temperature,
                         SUBSTRATE_EFFECTS_TEMP_MIN_C,
                         SUBSTRATE_EFFECTS_TEMP_MAX_C);
    float ion   = clampf(substrate->physical.ion_balance,      0.0f, 1.0f);
    float mem   = clampf(substrate->physical.membrane_integrity, 0.0f, 1.0f);

    /* Q10: rate(T) = Q10^((T - T_ref)/10). At T_ref = 37°C → 1.0. */
    float q10_factor = powf(SUBSTRATE_EFFECTS_Q10,
                            (tempC - SUBSTRATE_EFFECTS_REFERENCE_TEMP_C) / 10.0f);

    float atp_safe = (atp < 0.1f) ? 0.1f : atp;

    if (out_axon) {
        axon_substrate_effects_t* a = out_axon;
        memset(a, 0, sizeof(*a));

        a->temperature_q10_factor   = q10_factor;
        a->atp_velocity_factor      = atp;                    /* linear in ATP */
        a->myelin_efficiency        = atp;                    /* requires pump ATP */
        a->overall_velocity_mod     = q10_factor * atp;

        a->ion_gradient_strength    = ion;
        a->ap_amplitude_mod         = 0.5f + 0.5f * ion;      /* [0.5,1.0] */
        a->spike_reliability        = 0.5f + 0.5f * atp * ion; /* multiplicative */

        a->pump_activity            = atp;
        a->refractory_period_mod    = 1.0f / atp_safe;        /* low ATP → longer tref */

        a->transport_efficiency     = atp;
        a->kinesin_activity         = atp;

        a->membrane_capacitance_mod = 0.8f + 0.2f * mem;
        a->membrane_leak_mod        = 1.0f + (1.0f - mem);    /* [1,2] */

        /* Combined scalar: min of the limiting factors. */
        float oc = atp;
        if (ion < oc) oc = ion;
        if (a->ap_amplitude_mod < oc) oc = a->ap_amplitude_mod;
        a->overall_capacity = oc;
    }

    if (out_dend) {
        dendrite_substrate_effects_t* d = out_dend;
        memset(d, 0, sizeof(*d));

        /* Cable: τ_m ∝ R_m·C_m, λ ∝ √R_m — both fall with membrane damage. */
        d->membrane_time_constant_mod = 0.5f + 0.5f * mem;    /* [0.5,1.0] */
        d->space_constant_mod         = sqrtf(mem);
        d->integration_efficiency     = mem;
        d->attenuation_mod            = 1.0f + (1.0f - mem);  /* [1,2] */

        /* NMDA Mg2+ block sensitivity + Na channel availability scale w/ ions. */
        float nmda_mg_block           = 0.8f + 0.4f * ion;    /* [0.8,1.2] */
        d->nmda_mg_block_mod          = nmda_mg_block;
        d->na_channel_availability    = ion;
        /* Spike threshold: product of NMDA Mg2+ block (ion-driven) and the
         * inverse temperature effect (2 - q10). Formula matches
         * axon_dendrite_substrate_bridge.c:534,547 — ion × temp composition,
         * then clamp to the same [0.8,1.2] band the helper previously used. */
        d->spike_threshold_mod        = clampf(nmda_mg_block * (2.0f - q10_factor),
                                              0.8f, 1.2f);

        /* Ca handling: pumps need ATP, buffers partially ATP-dependent.
         * Compute pump/buffer into locals first, then assign and derive
         * the combined handling_mod — avoids silent breakage if a future
         * edit reorders the struct-field assignments. See Bug #4. */
        float ca_pump                 = atp;
        float ca_buffer               = 0.7f + 0.3f * atp;
        d->ca_pump_efficiency         = ca_pump;
        d->ca_buffer_capacity         = ca_buffer;
        d->ca_handling_mod            = 0.5f * (ca_pump + ca_buffer);

        /* Plasticity: LTP/LTD scale with ATP; LTD cheaper. */
        d->ltp_capacity               = atp;
        d->ltd_capacity               = clampf(atp * 1.1f, 0.0f, 1.0f);
        d->spine_growth_capacity      = atp;
        d->plasticity_mod             = atp;

        float oc = atp;
        if (ion < oc) oc = ion;
        if (mem < oc) oc = mem;
        d->overall_capacity = oc;
    }

    return NIMCP_SUCCESS;
}

void substrate_debit_activity(
    neural_substrate_t* substrate,
    uint32_t            region_id,
    uint32_t            n_spikes,
    uint32_t            n_plasticity_updates)
{
    (void)region_id;  /* single compartment for v1 */

    if (!substrate) {
        return;
    }

    /* V1: straight ATP decrement with tiny per-event cost. Full compartmentalized
     * accounting (ion gradient degradation, regional ATP) arrives in a later
     * phase; for now this is a safe, bounded-debit stub. */
    float debit = (float)n_spikes            * SUBSTRATE_EFFECTS_ATP_PER_SPIKE
                + (float)n_plasticity_updates * SUBSTRATE_EFFECTS_ATP_PER_PLASTICITY;

    if (debit > 0.0f) {
        /* Serialize the read-modify-write. Phase 2+3 share one
         * neural_substrate_t across SNN, LNN, and per-cortex CNN; all
         * those networks can call substrate_debit_activity concurrently
         * from different threads. Without locking, lost-update races
         * silently under-debit ATP. Matches the locking pattern in
         * src/core/neural_substrate/nimcp_neural_substrate.c (see
         * substrate_consume_atp / substrate_set_atp / etc.). See Bug #1. */
        if (substrate->mutex) {
            nimcp_platform_mutex_lock(substrate->mutex);
        }
        float new_atp = substrate->metabolic.atp_level - debit;
        if (new_atp < 0.0f) new_atp = 0.0f;
        substrate->metabolic.atp_level = new_atp;
        if (substrate->mutex) {
            nimcp_platform_mutex_unlock(substrate->mutex);
        }
    }

    LOG_DEBUG("substrate_debit_activity: region=%u spikes=%u plast=%u debit=%.3e",
              region_id, n_spikes, n_plasticity_updates, (double)debit);
}
