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
 * - Vesicle release probability
 * - Synaptic cleft diffusion
 * - Receptor binding kinetics
 * - Reuptake and enzymatic degradation
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

//=============================================================================
// Configuration
//=============================================================================

typedef struct {
    uint32_t num_synapses;              /**< Number of synaptic sites */
    float release_probability;          /**< Base release probability */
    float cleft_volume_fl;              /**< Cleft volume (femtoliters) */
    float reuptake_rate;                /**< Reuptake rate constant */
    float diffusion_rate;               /**< Diffusion coefficient */
    float vesicle_content_mm;           /**< NT per vesicle (mM equivalent) */
    neurotransmitter_type_t default_nt; /**< Default neurotransmitter type */
    bool enable_spillover;              /**< Enable extrasynaptic spillover */
    bool enable_logging;                /**< Enable adapter logging */
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
} nimcp_nt_adapter_stats_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default adapter configuration
 */
NIMCP_EXPORT nimcp_nt_adapter_config_t nimcp_nt_adapter_default_config(void);

/**
 * @brief Create neurotransmitter adapter
 *
 * @param config Configuration (NULL for defaults)
 * @return Adapter handle or NULL on failure
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
