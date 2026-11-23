/**
 * @file nimcp_neurotransmitter_adapter.h
 * @brief Neurotransmitter Dynamics Adapter for Chemistry Layer
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Adapts neurotransmitter release/uptake for Chemistry layer
 * WHY:  Chemical signaling is the primary mode of neural communication
 * HOW:  Implements nimcp_module_interface_t for Chemistry layer registration
 *
 * NEUROTRANSMITTER DYNAMICS:
 * - Vesicle release (discrete vesicles, or graded/continuous release)
 * - Synaptic cleft diffusion (spillover)
 * - Receptor binding kinetics (Hill, per transmitter)
 * - Reuptake and enzymatic degradation (per transmitter)
 *
 * USAGE:
 *   nimcp_nt_adapter_t a = nimcp_nt_adapter_create(&cfg);   // ready to use
 *   nimcp_nt_adapter_release(a, i, 1);                      // or release_graded()
 *   nimcp_nt_adapter_step(a, 1.0f);                         // advance 1 ms
 *   nimcp_nt_adapter_get_occupancy(a, occ, n, &cnt);
 *
 * TIME UNITS: nimcp_nt_adapter_step() takes MILLISECONDS. The layer-interface
 * callback interface->update(module, dt) takes SECONDS (dt = 0.001 is 1 ms)
 * and is equivalent to nimcp_nt_adapter_step(a, dt * 1000).
 * All internal time constants are in ms, concentrations in mM.
 *
 * INTEGRATION: exact exponential updates (clearance, receptor relaxation),
 * internally sub-stepped to <= 1 ms; unconditionally stable for any dt.
 * Values below ~1e-6 are flushed to zero so silent synapses never become
 * denormal floats (no reliance on caller-set FTZ/DAZ).
 *
 * THREAD SAFETY: none -- one thread per adapter (or external locking).
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_NEUROTRANSMITTER_ADAPTER_H
#define NIMCP_NEUROTRANSMITTER_ADAPTER_H

#include "integration/core/nimcp_layer_types.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Opaque Handle
//=============================================================================

typedef struct nimcp_nt_adapter_struct* nimcp_nt_adapter_t;

//=============================================================================
// Neurotransmitter Types
//=============================================================================

typedef enum {
    NT_GLUTAMATE = 0,                   /**< Excitatory (AMPA/NMDA) */
    NT_GABA,                            /**< Inhibitory (GABA-A/GABA-B) */
    NT_ACETYLCHOLINE,                   /**< Cholinergic */
    NT_GLYCINE,                         /**< Inhibitory (spinal) */
    NT_COUNT
} neurotransmitter_type_t;

/**
 * @brief Receptor + clearance kinetics of one transmitter
 *
 * Occupancy target is the Hill function c^n / (c^n + Kd^n) of the cleft
 * concentration; occupancy relaxes to it with tau_on_ms when rising and
 * tau_off_ms (deactivation/unbinding) when falling. Cleft concentration is
 * cleared with time constant clearance_tau_ms (uptake / enzymatic).
 *
 * Defaults (nimcp_nt_get_default_kinetics):
 *   transmitter  Kd/EC50  Hill  tau_on  tau_off  clearance
 *   glutamate    0.50 mM  1.5   1.0 ms  1.0 ms   1.2 ms  (AMPA; Clements 1992)
 *   GABA         0.15 mM  1.7   0.5 ms  10  ms   2.0 ms  (GABA-A; slow deactivation)
 *   ACh          0.05 mM  1.9   0.3 ms  1.0 ms   0.2 ms  (nicotinic; AChE)
 *   glycine      0.08 mM  2.5   0.5 ms  6.0 ms   2.0 ms  (GlyR a1b; GlyT)
 */
typedef struct {
    float kd_mm;                        /**< Half-activation concentration (mM) */
    float hill_n;                       /**< Hill coefficient */
    float tau_on_ms;                    /**< Occupancy rise time constant (ms) */
    float tau_off_ms;                   /**< Occupancy decay (deactivation) time constant (ms) */
    float clearance_tau_ms;             /**< Cleft clearance time constant (ms) */
} nimcp_nt_kinetics_t;

//=============================================================================
// Configuration
//=============================================================================

typedef struct {
    uint32_t num_synapses;              /**< Number of synaptic sites (> 0) */
    float release_probability;          /**< Base release probability */
    float cleft_volume_fl;              /**< Cleft volume (femtoliters) */
    float reuptake_rate;                /**< Cleft clearance rate (1/ms) applied to EVERY
                                             synapse; 0 (default) = each synapse uses its
                                             transmitter's clearance_tau_ms */
    float diffusion_rate;               /**< Spillover rate (1/ms) while cleft > 0.1 mM */
    float vesicle_content_mm;           /**< NT per vesicle (mM equivalent) */
    neurotransmitter_type_t default_nt; /**< Transmitter of every synapse at init
                                             (change per synapse with
                                             nimcp_nt_adapter_set_synapse_type) */
    bool enable_spillover;              /**< Enable extrasynaptic spillover */
    bool enable_logging;                /**< Enable adapter logging */
    float graded_max_rate;              /**< Graded release at drive 1 (vesicles/ms/site) */
} nimcp_nt_adapter_config_t;

//=============================================================================
// State and Statistics
//=============================================================================

typedef struct {
    float mean_cleft_conc_mm;           /**< Average cleft concentration (mM) */
    float mean_receptor_occupancy;      /**< Average receptor binding (0-1) */
    float extrasynaptic_conc_mm;        /**< Spillover concentration */
    uint32_t release_events;            /**< Total release events */
    bool is_active;                     /**< Whether adapter is active */
} nimcp_nt_adapter_state_t;

typedef struct {
    uint64_t updates_processed;         /**< Number of update calls */
    uint64_t messages_handled;          /**< Messages processed */
    uint64_t vesicles_released;         /**< Total vesicles released */
    uint64_t reuptake_events;           /**< Reuptake events */
    double graded_vesicles_released;    /**< Vesicle-equivalents released by graded release */
} nimcp_nt_adapter_stats_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default adapter configuration
 */
NIMCP_EXPORT nimcp_nt_adapter_config_t nimcp_nt_adapter_default_config(void);

/**
 * @brief Default receptor/clearance kinetics of a transmitter
 *
 * @param nt Transmitter
 * @param out Output kinetics
 * @return NIMCP_LAYER_OK, NIMCP_LAYER_ERR_NULL_PTR, or
 *         NIMCP_LAYER_ERR_INVALID_MODULE for nt >= NT_COUNT
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_nt_get_default_kinetics(
    neurotransmitter_type_t nt,
    nimcp_nt_kinetics_t* out
);

/**
 * @brief Create neurotransmitter adapter
 *
 * The returned adapter is initialised and ready to use (all synapses carry
 * config->default_nt). Calling interface->init() afterwards is allowed and
 * re-initialises it with the given config (state reset, arrays resized).
 *
 * @param config Configuration (NULL for defaults)
 * @return Adapter handle or NULL on failure (allocation, num_synapses == 0,
 *         invalid default_nt)
 */
NIMCP_EXPORT nimcp_nt_adapter_t nimcp_nt_adapter_create(
    const nimcp_nt_adapter_config_t* config
);

/**
 * @brief Destroy adapter
 *
 * @param adapter Adapter to destroy (NULL safe)
 */
NIMCP_EXPORT void nimcp_nt_adapter_destroy(nimcp_nt_adapter_t adapter);

/**
 * @brief Get module interface for layer registration
 *
 * @param adapter Adapter handle
 * @return Module interface pointer (owned by adapter)
 */
NIMCP_EXPORT nimcp_module_interface_t* nimcp_nt_adapter_get_interface(
    nimcp_nt_adapter_t adapter
);

//=============================================================================
// Operations API
//=============================================================================

/**
 * @brief Trigger neurotransmitter release
 *
 * @param adapter Adapter handle
 * @param synapse_idx Synapse index (-1 for all)
 * @param num_vesicles Number of vesicles to release
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_nt_adapter_release(
    nimcp_nt_adapter_t adapter,
    int synapse_idx,
    uint32_t num_vesicles
);

/**
 * @brief Graded (continuous) release, e.g. photoreceptor/bipolar ribbons
 *
 * Releases drive * config.graded_max_rate * dt_ms vesicle-equivalents
 * (fractional, mean-field) at the synapse, limited by its readily
 * releasable pool. Does not advance time: call nimcp_nt_adapter_step().
 *
 * @param adapter Adapter handle
 * @param synapse_idx Synapse index (-1 for all)
 * @param drive Presynaptic drive, clamped to [0, 1]
 * @param dt_ms Duration of the release window (ms, >= 0)
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_nt_adapter_release_graded(
    nimcp_nt_adapter_t adapter,
    int synapse_idx,
    float drive,
    float dt_ms
);

/**
 * @brief Graded release with one drive value per synapse
 *
 * @param drive Array of drives (clamped to [0, 1]); drive[i] -> synapse i
 * @param count Number of entries (min(count, num_synapses) are used)
 * @param dt_ms Duration of the release window (ms)
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_nt_adapter_release_graded_array(
    nimcp_nt_adapter_t adapter,
    const float* drive,
    uint32_t count,
    float dt_ms
);

/**
 * @brief Advance cleft/receptor dynamics by dt_ms MILLISECONDS
 *
 * Same as interface->update(adapter, dt_ms / 1000). Stable for any dt
 * (exact exponentials, sub-stepped to <= 1 ms). dt_ms <= 0 is a no-op.
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_nt_adapter_step(
    nimcp_nt_adapter_t adapter,
    float dt_ms
);

/**
 * @brief Set the transmitter of one synapse (or all with synapse_idx = -1)
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_nt_adapter_set_synapse_type(
    nimcp_nt_adapter_t adapter,
    int synapse_idx,
    neurotransmitter_type_t nt
);

/**
 * @brief Override the kinetics this adapter uses for a transmitter
 *
 * Values must be positive; kept across interface->init().
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_nt_adapter_set_kinetics(
    nimcp_nt_adapter_t adapter,
    neurotransmitter_type_t nt,
    const nimcp_nt_kinetics_t* kinetics
);

/**
 * @brief Get cleft concentrations
 *
 * @param adapter Adapter handle
 * @param conc_out Output array for concentrations (mM)
 * @param max_count Size of output array
 * @param count_out Number returned
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_nt_adapter_get_concentrations(
    nimcp_nt_adapter_t adapter,
    float* conc_out,
    uint32_t max_count,
    uint32_t* count_out
);

/**
 * @brief Get receptor occupancy
 *
 * @param adapter Adapter handle
 * @param occupancy_out Output array for occupancy (0-1)
 * @param max_count Size of output array
 * @param count_out Number returned
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_nt_adapter_get_occupancy(
    nimcp_nt_adapter_t adapter,
    float* occupancy_out,
    uint32_t max_count,
    uint32_t* count_out
);

//=============================================================================
// State/Stats API
//=============================================================================

/**
 * @brief Get adapter state
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_nt_adapter_get_state(
    nimcp_nt_adapter_t adapter,
    nimcp_nt_adapter_state_t* state_out
);

/**
 * @brief Get adapter statistics
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_nt_adapter_get_stats(
    nimcp_nt_adapter_t adapter,
    nimcp_nt_adapter_stats_t* stats_out
);

/**
 * @brief Reset statistics
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_nt_adapter_reset_stats(
    nimcp_nt_adapter_t adapter
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NEUROTRANSMITTER_ADAPTER_H */
