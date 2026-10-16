/**
 * @file nimcp_substrate_effects.h
 * @brief Shared substrate-to-effects-struct helper + small inline utilities.
 *
 * Single source of truth for converting neural_substrate_t state into the
 * per-network effect multipliers consumed by SNN/LNN/CNN/FNO/HNN adapters.
 * Wraps the existing axon-dendrite substrate bridge math into a clean
 * network-agnostic API.
 */
#ifndef NIMCP_SUBSTRATE_EFFECTS_H
#define NIMCP_SUBSTRATE_EFFECTS_H

#include <stdint.h>
#include <stdbool.h>

#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward decl (full type in nimcp_neural_substrate.h). */
struct neural_substrate;
typedef struct neural_substrate neural_substrate_t;

/**
 * @brief Per-axon substrate-modulated effect multipliers.
 *
 * Migrated from include/core/nimcp_axon_dendrite_substrate_bridge.h (now
 * deleted) into this header as the single source of truth for substrate-
 * effect types. Cached as values on snn_network_t / lnn_network_t /
 * cortex_cnn_processor_t and refreshed per step via
 * substrate_compute_effects().
 */
typedef struct {
    /* Conduction velocity modulation */
    float temperature_q10_factor;     /**< Q10 temperature effect [0.5-1.5] */
    float atp_velocity_factor;        /**< ATP effect on velocity [0-1] */
    float myelin_efficiency;          /**< Myelination ATP efficiency [0-1] */
    float overall_velocity_mod;       /**< Combined velocity multiplier */

    /* Action potential reliability */
    float ion_gradient_strength;     /**< Na+/K+ gradient quality [0-1] */
    float ap_amplitude_mod;           /**< AP amplitude modulation [0-1] */
    float spike_reliability;          /**< Propagation reliability [0-1] */

    /* Refractory period modulation */
    float pump_activity;              /**< Na+/K+-ATPase activity [0-1] */
    float refractory_period_mod;      /**< Refractory multiplier [0.7-1.3] */

    /* Axonal transport */
    float transport_efficiency;       /**< Vesicle transport [0-1] */
    float kinesin_activity;           /**< Motor protein activity [0-1] */

    /* Membrane effects */
    float membrane_capacitance_mod;   /**< Capacitance change [0.8-1.2] */
    float membrane_leak_mod;          /**< Leak conductance [1.0-3.0] */

    /* Overall capacity */
    float overall_capacity;           /**< Combined axon capacity [0-1] */
} axon_substrate_effects_t;

/**
 * @brief Per-dendrite substrate-modulated effect multipliers.
 */
typedef struct {
    /* Integration efficiency */
    float membrane_time_constant_mod; /**< τ_m modulation [0.5-1.5] */
    float space_constant_mod;         /**< λ modulation [0.5-1.5] */
    float integration_efficiency;     /**< Overall integration [0-1] */
    float attenuation_mod;            /**< Voltage attenuation [1.0-2.0] */

    /* Dendritic spike threshold */
    float nmda_mg_block_mod;          /**< Mg2+ block sensitivity [0.8-1.2] */
    float spike_threshold_mod;        /**< Threshold voltage shift [0.8-1.2] */
    float na_channel_availability;    /**< Dendritic Na+ channels [0-1] */

    /* Calcium dynamics */
    float ca_pump_efficiency;         /**< Ca2+ extrusion [0-1] */
    float ca_buffer_capacity;         /**< Buffering capacity [0-1] */
    float ca_handling_mod;            /**< Overall Ca2+ handling [0-1] */

    /* Plasticity capacity */
    float ltp_capacity;               /**< LTP induction ability [0-1] */
    float ltd_capacity;               /**< LTD induction ability [0-1] */
    float spine_growth_capacity;      /**< Structural plasticity [0-1] */
    float plasticity_mod;             /**< Overall plasticity [0-1] */

    /* Overall capacity */
    float overall_capacity;           /**< Combined dendrite capacity [0-1] */
} dendrite_substrate_effects_t;

/**
 * Compute both effect structs from the substrate state. Reads the substrate's
 * chemistry/ion/membrane fields, applies biologically-motivated formulas
 * (Q10 for temperature, linear for ATP fraction, etc.) and fills the two
 * out params. Safe to call with NULL outputs (skipped).
 *
 * Returns NIMCP_SUCCESS on success. NULL substrate is an error.
 */
nimcp_error_t substrate_compute_effects(
    const neural_substrate_t*        substrate,
    axon_substrate_effects_t*        out_axon_effects,
    dendrite_substrate_effects_t*    out_dend_effects);

/**
 * Report network activity back to the substrate. Called by per-network
 * adapters at end of step. Substrate integrates: spikes → ATP consumption,
 * plasticity updates → additional ATP, cumulative firing → ion gradient
 * degradation.
 *
 * region_id identifies the chemistry compartment (0 = global). For the
 * first cut we accept a single compartment.
 */
void substrate_debit_activity(
    neural_substrate_t* substrate,
    uint32_t region_id,
    uint32_t n_spikes,
    uint32_t n_plasticity_updates);

/*===========================================================================
 * Inline helpers — shared across all per-network adapters to avoid
 * duplication. Pure math, no side effects.
 *===========================================================================*/

/** Scale a time constant by dendritic substrate modulation. Clamp safe. */
static inline float substrate_apply_tau(float tau_base,
                                        const dendrite_substrate_effects_t* d) {
    if (!d) return tau_base;
    /* Detect a zero-initialized (uninitialized) effects cache: both
     * membrane_time_constant_mod and overall_capacity at exactly 0.0f means
     * the effects have never been computed. Return tau_base unscaled so that
     * an uninitialized cache doesn't collapse time constants to the 0.1 floor
     * and disrupt dynamics. Belt-and-suspenders for Bug #3. */
    if (d->membrane_time_constant_mod == 0.0f && d->overall_capacity == 0.0f) {
        return tau_base;
    }
    float v = tau_base * d->membrane_time_constant_mod;
    return (v < 0.1f) ? 0.1f : v;  /* floor to avoid div-by-zero downstream */
}

/** Scale refractory period by axonal substrate modulation. */
static inline float substrate_apply_tref(float tref_base,
                                         const axon_substrate_effects_t* a) {
    if (!a) return tref_base;
    float v = tref_base * a->refractory_period_mod;
    return (v < 0.0f) ? 0.0f : v;
}

/** Scale a learning rate by dendritic plasticity capacity.
 *
 * Bug #1 safety belt: a cache that was memset-zeroed but never populated
 * has plasticity_mod==0 AND overall_capacity==0 (both fields are baseline
 * 1.0f when the substrate helper runs). That signature means the caller
 * attached a substrate but did not refresh the cache — returning lr_base
 * unscaled prevents silent learning death instead of lr *= 0. Real
 * depleted substrate still has overall_capacity > 0 (ATP + ion factors),
 * so this guard only fires on genuinely uninitialized caches. */
static inline float substrate_apply_lr(float lr_base,
                                       const dendrite_substrate_effects_t* d) {
    if (!d) return lr_base;
    /* Detect a zero-initialized (uninitialized) effects cache: both
     * plasticity_mod and overall_capacity at exactly 0.0f means the cache
     * has never been populated. Returning lr_base * 0 would silently kill
     * R-STDP / learning; return the unscaled base instead. Complements the
     * attach-side initialization fix in snn_network_attach_substrate. */
    if (d->plasticity_mod == 0.0f && d->overall_capacity == 0.0f) {
        return lr_base;
    }
    return lr_base * d->plasticity_mod;
}

/**
 * True if a threshold-crossing spike should survive propagation.
 * rand01 must be in [0, 1]. Returns true when reliability is high enough.
 * When a is NULL, spike always survives (conservative default).
 */
static inline bool substrate_spike_survives(const axon_substrate_effects_t* a,
                                            float rand01) {
    if (!a) return true;
    return rand01 < a->spike_reliability;
}

#ifdef __cplusplus
}
#endif
#endif /* NIMCP_SUBSTRATE_EFFECTS_H */
