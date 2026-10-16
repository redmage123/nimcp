/**
 * @file nimcp_hh_dynamics_adapter.h
 * @brief Hodgkin-Huxley Dynamics Adapter for Physics Layer
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Adapts neuron model implementations for Physics layer integration
 * WHY:  Connects existing HH/biophysical neuron models to layer infrastructure
 * HOW:  Wraps nimcp_neuron_model API to implement nimcp_module_interface_t
 *
 * USAGE:
 * ======
 *   // Create adapter
 *   nimcp_hh_adapter_t adapter = nimcp_hh_adapter_create(NULL);
 *
 *   // Get module interface for layer registration
 *   nimcp_module_interface_t* iface = nimcp_hh_adapter_get_interface(adapter);
 *
 *   // Connect to physics layer coordinator
 *   nimcp_physics_intra_connect_hh_dynamics(physics_coord, adapter, iface);
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_HH_DYNAMICS_ADAPTER_H
#define NIMCP_HH_DYNAMICS_ADAPTER_H

#include "integration/core/nimcp_layer_types.h"
#include "core/neuron_models/nimcp_neuron_model.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Opaque Handle
//=============================================================================

typedef struct nimcp_hh_adapter_struct* nimcp_hh_adapter_t;

//=============================================================================
// Configuration
//=============================================================================

typedef struct {
    neuron_model_type_t model_type;     /**< Which neuron model to use */
    uint32_t num_neurons;               /**< Number of neurons to simulate */
    float dt_ms;                        /**< Timestep in milliseconds */
    bool enable_detailed_biophysics;    /**< Enable detailed ion channel dynamics */
    bool enable_energy_tracking;        /**< Track metabolic energy consumption */
    bool enable_logging;                /**< Enable adapter logging */
} nimcp_hh_adapter_config_t;

//=============================================================================
// State and Statistics
//=============================================================================

typedef struct {
    float mean_membrane_potential;      /**< Average membrane potential (mV) */
    float mean_firing_rate;             /**< Average firing rate (Hz) */
    float total_energy_consumed;        /**< Total ATP consumed */
    uint32_t total_spikes;              /**< Total spike count */
    bool is_active;                     /**< Whether adapter is active */
} nimcp_hh_adapter_state_t;

typedef struct {
    uint64_t updates_processed;         /**< Number of update calls */
    uint64_t messages_handled;          /**< Messages processed */
    uint64_t spikes_generated;          /**< Spikes produced */
    float avg_update_time_us;           /**< Average update time */
} nimcp_hh_adapter_stats_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default adapter configuration
 */
NIMCP_EXPORT nimcp_hh_adapter_config_t nimcp_hh_adapter_default_config(void);

/**
 * @brief Create HH dynamics adapter
 *
 * @param config Configuration (NULL for defaults)
 * @return Adapter handle or NULL on failure
 */
NIMCP_EXPORT nimcp_hh_adapter_t nimcp_hh_adapter_create(
    const nimcp_hh_adapter_config_t* config
);

/**
 * @brief Destroy adapter
 *
 * @param adapter Adapter to destroy (NULL safe)
 */
NIMCP_EXPORT void nimcp_hh_adapter_destroy(nimcp_hh_adapter_t adapter);

/**
 * @brief Get module interface for layer registration
 *
 * @param adapter Adapter handle
 * @return Module interface pointer (owned by adapter)
 */
NIMCP_EXPORT nimcp_module_interface_t* nimcp_hh_adapter_get_interface(
    nimcp_hh_adapter_t adapter
);

//=============================================================================
// Operations API
//=============================================================================

/**
 * @brief Inject current into neurons
 *
 * @param adapter Adapter handle
 * @param neuron_idx Neuron index (or -1 for all)
 * @param current_nA Injected current in nanoamperes
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_hh_adapter_inject_current(
    nimcp_hh_adapter_t adapter,
    int neuron_idx,
    float current_nA
);

/**
 * @brief Get spike output from adapter
 *
 * @param adapter Adapter handle
 * @param spikes_out Output array for spike flags
 * @param max_spikes Size of output array
 * @param count_out Number of spikes returned
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_hh_adapter_get_spikes(
    nimcp_hh_adapter_t adapter,
    bool* spikes_out,
    uint32_t max_spikes,
    uint32_t* count_out
);

/**
 * @brief Get membrane potentials
 *
 * @param adapter Adapter handle
 * @param potentials_out Output array for potentials (mV)
 * @param max_potentials Size of output array
 * @param count_out Number of potentials returned
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_hh_adapter_get_potentials(
    nimcp_hh_adapter_t adapter,
    float* potentials_out,
    uint32_t max_potentials,
    uint32_t* count_out
);

//=============================================================================
// State/Stats API
//=============================================================================

/**
 * @brief Get adapter state
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_hh_adapter_get_state(
    nimcp_hh_adapter_t adapter,
    nimcp_hh_adapter_state_t* state_out
);

/**
 * @brief Get adapter statistics
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_hh_adapter_get_stats(
    nimcp_hh_adapter_t adapter,
    nimcp_hh_adapter_stats_t* stats_out
);

/**
 * @brief Reset statistics
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_hh_adapter_reset_stats(
    nimcp_hh_adapter_t adapter
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HH_DYNAMICS_ADAPTER_H */
