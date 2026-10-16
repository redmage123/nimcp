/**
 * @file nimcp_ephaptic_coupling_adapter.h
 * @brief Ephaptic Coupling Adapter for Physics Layer
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Adapts ephaptic (electric field) coupling for Physics layer integration
 * WHY:  Enables electric field effects between neurons in layer messaging
 * HOW:  Implements nimcp_module_interface_t for Physics layer registration
 *
 * EPHAPTIC COUPLING:
 * Ephaptic coupling refers to non-synaptic neural communication through
 * extracellular electric fields. This adapter models:
 * - Local field potentials (LFP) from neural populations
 * - Field effect modulation on nearby neurons
 * - Synchronization effects from ephaptic interactions
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_EPHAPTIC_COUPLING_ADAPTER_H
#define NIMCP_EPHAPTIC_COUPLING_ADAPTER_H

#include "integration/core/nimcp_layer_types.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Opaque Handle
//=============================================================================

typedef struct nimcp_ephaptic_adapter_struct* nimcp_ephaptic_adapter_t;

//=============================================================================
// Configuration
//=============================================================================

typedef struct {
    uint32_t num_neurons;               /**< Number of neurons in population */
    float coupling_strength;            /**< Base coupling coefficient (0-1) */
    float decay_constant;               /**< Spatial decay constant (mm) */
    float field_threshold;              /**< Minimum field strength to propagate */
    bool enable_lfp_computation;        /**< Compute local field potentials */
    bool enable_synchronization;        /**< Model sync effects */
    bool enable_logging;                /**< Enable adapter logging */
} nimcp_ephaptic_adapter_config_t;

//=============================================================================
// State and Statistics
//=============================================================================

typedef struct {
    float mean_field_strength;          /**< Average field strength (mV/mm) */
    float lfp_amplitude;                /**< Current LFP amplitude (mV) */
    float sync_index;                   /**< Population synchronization (0-1) */
    uint32_t interactions_computed;     /**< Number of interactions this step */
    bool is_active;                     /**< Whether adapter is active */
} nimcp_ephaptic_adapter_state_t;

typedef struct {
    uint64_t updates_processed;         /**< Number of update calls */
    uint64_t messages_handled;          /**< Messages processed */
    uint64_t fields_computed;           /**< Total field computations */
    float avg_update_time_us;           /**< Average update time */
} nimcp_ephaptic_adapter_stats_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default adapter configuration
 */
NIMCP_EXPORT nimcp_ephaptic_adapter_config_t nimcp_ephaptic_adapter_default_config(void);

/**
 * @brief Create ephaptic coupling adapter
 *
 * @param config Configuration (NULL for defaults)
 * @return Adapter handle or NULL on failure
 */
NIMCP_EXPORT nimcp_ephaptic_adapter_t nimcp_ephaptic_adapter_create(
    const nimcp_ephaptic_adapter_config_t* config
);

/**
 * @brief Destroy adapter
 *
 * @param adapter Adapter to destroy (NULL safe)
 */
NIMCP_EXPORT void nimcp_ephaptic_adapter_destroy(nimcp_ephaptic_adapter_t adapter);

/**
 * @brief Get module interface for layer registration
 *
 * @param adapter Adapter handle
 * @return Module interface pointer (owned by adapter)
 */
NIMCP_EXPORT nimcp_module_interface_t* nimcp_ephaptic_adapter_get_interface(
    nimcp_ephaptic_adapter_t adapter
);

//=============================================================================
// Operations API
//=============================================================================

/**
 * @brief Set membrane potentials for field computation
 *
 * @param adapter Adapter handle
 * @param potentials Array of membrane potentials (mV)
 * @param count Number of potentials
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_ephaptic_adapter_set_potentials(
    nimcp_ephaptic_adapter_t adapter,
    const float* potentials,
    uint32_t count
);

/**
 * @brief Compute ephaptic field effects
 *
 * @param adapter Adapter handle
 * @param field_effects_out Output array for computed field effects
 * @param max_effects Size of output array
 * @param count_out Number of effects returned
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_ephaptic_adapter_compute_fields(
    nimcp_ephaptic_adapter_t adapter,
    float* field_effects_out,
    uint32_t max_effects,
    uint32_t* count_out
);

/**
 * @brief Get current local field potential
 *
 * @param adapter Adapter handle
 * @param lfp_out Output LFP value (mV)
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_ephaptic_adapter_get_lfp(
    nimcp_ephaptic_adapter_t adapter,
    float* lfp_out
);

//=============================================================================
// State/Stats API
//=============================================================================

/**
 * @brief Get adapter state
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_ephaptic_adapter_get_state(
    nimcp_ephaptic_adapter_t adapter,
    nimcp_ephaptic_adapter_state_t* state_out
);

/**
 * @brief Get adapter statistics
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_ephaptic_adapter_get_stats(
    nimcp_ephaptic_adapter_t adapter,
    nimcp_ephaptic_adapter_stats_t* stats_out
);

/**
 * @brief Reset statistics
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_ephaptic_adapter_reset_stats(
    nimcp_ephaptic_adapter_t adapter
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EPHAPTIC_COUPLING_ADAPTER_H */
