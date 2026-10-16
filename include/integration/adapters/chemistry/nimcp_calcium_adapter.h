/**
 * @file nimcp_calcium_adapter.h
 * @brief Calcium Signaling Adapter for Chemistry Layer
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Adapts calcium signaling dynamics for Chemistry layer integration
 * WHY:  Calcium is a key second messenger affecting plasticity and excitability
 * HOW:  Implements nimcp_module_interface_t for Chemistry layer registration
 *
 * CALCIUM SIGNALING IN NEURONS:
 * - Voltage-gated calcium channels (VGCCs)
 * - NMDA receptor calcium influx
 * - Intracellular calcium stores (ER)
 * - Calcium-activated proteins (CaMKII, calcineurin)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_CALCIUM_ADAPTER_H
#define NIMCP_CALCIUM_ADAPTER_H

#include "integration/core/nimcp_layer_types.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Opaque Handle
//=============================================================================

typedef struct nimcp_calcium_adapter_struct* nimcp_calcium_adapter_t;

//=============================================================================
// Configuration
//=============================================================================

typedef struct {
    uint32_t num_compartments;          /**< Number of calcium compartments */
    float basal_calcium_nm;             /**< Baseline [Ca2+] (nM) */
    float decay_tau_ms;                 /**< Calcium decay time constant (ms) */
    float vgcc_conductance;             /**< VGCC conductance coefficient */
    float nmda_fraction;                /**< NMDA calcium permeability */
    float er_pump_rate;                 /**< ER calcium uptake rate */
    bool enable_store_release;          /**< Enable ER store-operated release */
    bool enable_camkii;                 /**< Enable CaMKII dynamics */
    bool enable_logging;                /**< Enable adapter logging */
} nimcp_calcium_adapter_config_t;

//=============================================================================
// State and Statistics
//=============================================================================

typedef struct {
    float mean_calcium_nm;              /**< Average [Ca2+] (nM) */
    float max_calcium_nm;               /**< Peak [Ca2+] */
    float camkii_activity;              /**< CaMKII activation (0-1) */
    float calcineurin_activity;         /**< Calcineurin activation (0-1) */
    uint32_t calcium_events;            /**< Number of calcium transients */
    bool is_active;                     /**< Whether adapter is active */
} nimcp_calcium_adapter_state_t;

typedef struct {
    uint64_t updates_processed;         /**< Number of update calls */
    uint64_t messages_handled;          /**< Messages processed */
    uint64_t influx_events;             /**< Calcium influx events */
    float avg_update_time_us;           /**< Average update time */
} nimcp_calcium_adapter_stats_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default adapter configuration
 */
NIMCP_EXPORT nimcp_calcium_adapter_config_t nimcp_calcium_adapter_default_config(void);

/**
 * @brief Create calcium signaling adapter
 *
 * @param config Configuration (NULL for defaults)
 * @return Adapter handle or NULL on failure
 */
NIMCP_EXPORT nimcp_calcium_adapter_t nimcp_calcium_adapter_create(
    const nimcp_calcium_adapter_config_t* config
);

/**
 * @brief Destroy adapter
 *
 * @param adapter Adapter to destroy (NULL safe)
 */
NIMCP_EXPORT void nimcp_calcium_adapter_destroy(nimcp_calcium_adapter_t adapter);

/**
 * @brief Get module interface for layer registration
 *
 * @param adapter Adapter handle
 * @return Module interface pointer (owned by adapter)
 */
NIMCP_EXPORT nimcp_module_interface_t* nimcp_calcium_adapter_get_interface(
    nimcp_calcium_adapter_t adapter
);

//=============================================================================
// Operations API
//=============================================================================

/**
 * @brief Trigger calcium influx
 *
 * @param adapter Adapter handle
 * @param compartment_idx Compartment index (-1 for all)
 * @param calcium_nm Calcium concentration to add (nM)
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_calcium_adapter_influx(
    nimcp_calcium_adapter_t adapter,
    int compartment_idx,
    float calcium_nm
);

/**
 * @brief Get calcium concentrations
 *
 * @param adapter Adapter handle
 * @param calcium_out Output array for concentrations (nM)
 * @param max_count Size of output array
 * @param count_out Number returned
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_calcium_adapter_get_calcium(
    nimcp_calcium_adapter_t adapter,
    float* calcium_out,
    uint32_t max_count,
    uint32_t* count_out
);

/**
 * @brief Get CaMKII and calcineurin activity
 *
 * @param adapter Adapter handle
 * @param camkii_out CaMKII activity output (0-1)
 * @param calcineurin_out Calcineurin activity output (0-1)
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_calcium_adapter_get_kinase_activity(
    nimcp_calcium_adapter_t adapter,
    float* camkii_out,
    float* calcineurin_out
);

//=============================================================================
// State/Stats API
//=============================================================================

/**
 * @brief Get adapter state
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_calcium_adapter_get_state(
    nimcp_calcium_adapter_t adapter,
    nimcp_calcium_adapter_state_t* state_out
);

/**
 * @brief Get adapter statistics
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_calcium_adapter_get_stats(
    nimcp_calcium_adapter_t adapter,
    nimcp_calcium_adapter_stats_t* stats_out
);

/**
 * @brief Reset statistics
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_calcium_adapter_reset_stats(
    nimcp_calcium_adapter_t adapter
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CALCIUM_ADAPTER_H */
