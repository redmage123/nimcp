/**
 * @file nimcp_metabolic_adapter.h
 * @brief Metabolic/ATP Adapter for Chemistry Layer
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Adapts metabolic dynamics (ATP/ADP, glucose) for Chemistry layer
 * WHY:  Energy metabolism constrains neural computation
 * HOW:  Implements nimcp_module_interface_t for Chemistry layer registration
 *
 * NEURAL METABOLISM:
 * - ATP production via glycolysis and oxidative phosphorylation
 * - ATP consumption by Na+/K+ ATPase, neurotransmitter synthesis
 * - Glucose uptake and lactate shuttle
 * - Metabolic fatigue effects on neural activity
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_METABOLIC_ADAPTER_H
#define NIMCP_METABOLIC_ADAPTER_H

#include "integration/core/nimcp_layer_types.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Opaque Handle
//=============================================================================

typedef struct nimcp_metabolic_adapter_struct* nimcp_metabolic_adapter_t;

//=============================================================================
// Configuration
//=============================================================================

typedef struct {
    uint32_t num_compartments;          /**< Number of metabolic compartments */
    float basal_atp_mm;                 /**< Baseline [ATP] (mM) */
    float max_atp_mm;                   /**< Maximum [ATP] (mM) */
    float atp_production_rate;          /**< ATP synthesis rate (mM/s) */
    float glucose_km;                   /**< Glucose Km for uptake (mM) */
    bool enable_fatigue;                /**< Enable metabolic fatigue */
    bool enable_lactate;                /**< Enable lactate shuttle */
    bool enable_logging;                /**< Enable adapter logging */
} nimcp_metabolic_adapter_config_t;

//=============================================================================
// State and Statistics
//=============================================================================

typedef struct {
    float mean_atp_mm;                  /**< Average [ATP] (mM) */
    float mean_adp_mm;                  /**< Average [ADP] (mM) */
    float glucose_level;                /**< Current glucose (mM) */
    float lactate_level;                /**< Current lactate (mM) */
    float energy_ratio;                 /**< ATP/(ATP+ADP) energy charge */
    float fatigue_index;                /**< Metabolic fatigue (0-1) */
    bool is_active;                     /**< Whether adapter is active */
} nimcp_metabolic_adapter_state_t;

typedef struct {
    uint64_t updates_processed;         /**< Number of update calls */
    uint64_t messages_handled;          /**< Messages processed */
    uint64_t atp_depletion_events;      /**< ATP depletion events */
    float total_atp_consumed;           /**< Cumulative ATP consumed */
} nimcp_metabolic_adapter_stats_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default adapter configuration
 */
NIMCP_EXPORT nimcp_metabolic_adapter_config_t nimcp_metabolic_adapter_default_config(void);

/**
 * @brief Create metabolic adapter
 *
 * @param config Configuration (NULL for defaults)
 * @return Adapter handle or NULL on failure
 */
NIMCP_EXPORT nimcp_metabolic_adapter_t nimcp_metabolic_adapter_create(
    const nimcp_metabolic_adapter_config_t* config
);

/**
 * @brief Destroy adapter
 *
 * @param adapter Adapter to destroy (NULL safe)
 */
NIMCP_EXPORT void nimcp_metabolic_adapter_destroy(nimcp_metabolic_adapter_t adapter);

/**
 * @brief Get module interface for layer registration
 *
 * @param adapter Adapter handle
 * @return Module interface pointer (owned by adapter)
 */
NIMCP_EXPORT nimcp_module_interface_t* nimcp_metabolic_adapter_get_interface(
    nimcp_metabolic_adapter_t adapter
);

//=============================================================================
// Operations API
//=============================================================================

/**
 * @brief Consume ATP (due to neural activity)
 *
 * @param adapter Adapter handle
 * @param compartment_idx Compartment index (-1 for all)
 * @param atp_consumed ATP consumed (mM)
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_metabolic_adapter_consume_atp(
    nimcp_metabolic_adapter_t adapter,
    int compartment_idx,
    float atp_consumed
);

/**
 * @brief Set glucose availability
 *
 * @param adapter Adapter handle
 * @param glucose_mm Glucose concentration (mM)
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_metabolic_adapter_set_glucose(
    nimcp_metabolic_adapter_t adapter,
    float glucose_mm
);

/**
 * @brief Get ATP levels
 *
 * @param adapter Adapter handle
 * @param atp_out Output array for ATP levels (mM)
 * @param max_count Size of output array
 * @param count_out Number returned
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_metabolic_adapter_get_atp(
    nimcp_metabolic_adapter_t adapter,
    float* atp_out,
    uint32_t max_count,
    uint32_t* count_out
);

/**
 * @brief Get energy charge ratio
 *
 * Energy charge = (ATP + 0.5*ADP) / (ATP + ADP + AMP)
 *
 * @param adapter Adapter handle
 * @param energy_charge_out Output energy charge (0-1)
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_metabolic_adapter_get_energy_charge(
    nimcp_metabolic_adapter_t adapter,
    float* energy_charge_out
);

//=============================================================================
// State/Stats API
//=============================================================================

/**
 * @brief Get adapter state
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_metabolic_adapter_get_state(
    nimcp_metabolic_adapter_t adapter,
    nimcp_metabolic_adapter_state_t* state_out
);

/**
 * @brief Get adapter statistics
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_metabolic_adapter_get_stats(
    nimcp_metabolic_adapter_t adapter,
    nimcp_metabolic_adapter_stats_t* stats_out
);

/**
 * @brief Reset statistics
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_metabolic_adapter_reset_stats(
    nimcp_metabolic_adapter_t adapter
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_METABOLIC_ADAPTER_H */
