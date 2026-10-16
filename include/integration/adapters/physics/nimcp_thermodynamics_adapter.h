/**
 * @file nimcp_thermodynamics_adapter.h
 * @brief Thermodynamics Adapter for Physics Layer
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Adapts thermal dynamics for Physics layer integration
 * WHY:  Models temperature effects on neural computation and metabolism
 * HOW:  Implements nimcp_module_interface_t for Physics layer registration
 *
 * THERMODYNAMICS IN NEURAL SYSTEMS:
 * Temperature affects:
 * - Ion channel kinetics (Q10 coefficients)
 * - Metabolic rate and ATP production
 * - Protein folding and enzyme activity
 * - Neural firing rates and thresholds
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_THERMODYNAMICS_ADAPTER_H
#define NIMCP_THERMODYNAMICS_ADAPTER_H

#include "integration/core/nimcp_layer_types.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Opaque Handle
//=============================================================================

typedef struct nimcp_thermo_adapter_struct* nimcp_thermo_adapter_t;

//=============================================================================
// Configuration
//=============================================================================

typedef struct {
    uint32_t num_regions;               /**< Number of thermal regions */
    float base_temperature_c;           /**< Base temperature (Celsius) */
    float q10_coefficient;              /**< Temperature sensitivity (Q10) */
    float thermal_diffusivity;          /**< Heat diffusion rate (mm^2/s) */
    float metabolic_heat_rate;          /**< Heat generation from metabolism */
    bool enable_heat_dissipation;       /**< Model heat loss to environment */
    bool enable_metabolic_coupling;     /**< Couple to metabolic activity */
    bool enable_logging;                /**< Enable adapter logging */
} nimcp_thermo_adapter_config_t;

//=============================================================================
// State and Statistics
//=============================================================================

typedef struct {
    float mean_temperature_c;           /**< Average temperature (C) */
    float max_temperature_c;            /**< Maximum local temperature */
    float min_temperature_c;            /**< Minimum local temperature */
    float total_heat_generated;         /**< Cumulative heat (J) */
    float kinetic_scaling;              /**< Current kinetic rate scaling */
    bool is_active;                     /**< Whether adapter is active */
} nimcp_thermo_adapter_state_t;

typedef struct {
    uint64_t updates_processed;         /**< Number of update calls */
    uint64_t messages_handled;          /**< Messages processed */
    uint64_t temperature_events;        /**< Temp change events */
    float avg_update_time_us;           /**< Average update time */
} nimcp_thermo_adapter_stats_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default adapter configuration
 */
NIMCP_EXPORT nimcp_thermo_adapter_config_t nimcp_thermo_adapter_default_config(void);

/**
 * @brief Create thermodynamics adapter
 *
 * @param config Configuration (NULL for defaults)
 * @return Adapter handle or NULL on failure
 */
NIMCP_EXPORT nimcp_thermo_adapter_t nimcp_thermo_adapter_create(
    const nimcp_thermo_adapter_config_t* config
);

/**
 * @brief Destroy adapter
 *
 * @param adapter Adapter to destroy (NULL safe)
 */
NIMCP_EXPORT void nimcp_thermo_adapter_destroy(nimcp_thermo_adapter_t adapter);

/**
 * @brief Get module interface for layer registration
 *
 * @param adapter Adapter handle
 * @return Module interface pointer (owned by adapter)
 */
NIMCP_EXPORT nimcp_module_interface_t* nimcp_thermo_adapter_get_interface(
    nimcp_thermo_adapter_t adapter
);

//=============================================================================
// Operations API
//=============================================================================

/**
 * @brief Add metabolic heat to a region
 *
 * @param adapter Adapter handle
 * @param region_idx Region index
 * @param heat_joules Heat energy in joules
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_thermo_adapter_add_heat(
    nimcp_thermo_adapter_t adapter,
    int region_idx,
    float heat_joules
);

/**
 * @brief Get kinetic rate scaling factor
 *
 * The Q10 coefficient describes how much faster reactions proceed
 * for a 10C temperature increase. Returns the scaling factor for
 * the current temperature relative to the base temperature.
 *
 * @param adapter Adapter handle
 * @param scaling_out Output scaling factor
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_thermo_adapter_get_kinetic_scaling(
    nimcp_thermo_adapter_t adapter,
    float* scaling_out
);

/**
 * @brief Get regional temperatures
 *
 * @param adapter Adapter handle
 * @param temps_out Output array for temperatures (C)
 * @param max_temps Size of output array
 * @param count_out Number of temps returned
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_thermo_adapter_get_temperatures(
    nimcp_thermo_adapter_t adapter,
    float* temps_out,
    uint32_t max_temps,
    uint32_t* count_out
);

/**
 * @brief Set external temperature (environment)
 *
 * @param adapter Adapter handle
 * @param temp_c External temperature in Celsius
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_thermo_adapter_set_external_temp(
    nimcp_thermo_adapter_t adapter,
    float temp_c
);

//=============================================================================
// State/Stats API
//=============================================================================

/**
 * @brief Get adapter state
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_thermo_adapter_get_state(
    nimcp_thermo_adapter_t adapter,
    nimcp_thermo_adapter_state_t* state_out
);

/**
 * @brief Get adapter statistics
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_thermo_adapter_get_stats(
    nimcp_thermo_adapter_t adapter,
    nimcp_thermo_adapter_stats_t* stats_out
);

/**
 * @brief Reset statistics
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_thermo_adapter_reset_stats(
    nimcp_thermo_adapter_t adapter
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_THERMODYNAMICS_ADAPTER_H */
