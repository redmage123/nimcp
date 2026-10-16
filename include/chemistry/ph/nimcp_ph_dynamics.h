/**
 * @file nimcp_ph_dynamics.h
 * @brief pH Dynamics Module - Extracellular and intracellular pH regulation
 * @version 1.0.0
 * @date 2026-01-11
 *
 * WHAT: pH dynamics modeling for neural tissue
 * WHY:  pH affects ion channel conductance, neurotransmitter release, and metabolism
 * HOW:  Model proton pumps, buffer systems, and pH-dependent effects
 *
 * KEY CONCEPTS:
 * - Extracellular pH: Typically ~7.4, affects NMDA receptors and synaptic function
 * - Intracellular pH: Typically ~7.2, affects metabolic enzymes
 * - Synaptic Vesicle pH: ~5.5, critical for neurotransmitter loading
 * - Buffer Systems: Bicarbonate and protein buffers maintain homeostasis
 * - pH Conductance Modulation: Acidosis decreases most ion channel conductance
 *
 * BIOLOGICAL BASIS:
 * - V-ATPase acidifies synaptic vesicles for NT loading
 * - NHE exchangers regulate intracellular pH
 * - Bicarbonate buffer system (CO2/HCO3-) is primary blood buffer
 * - Activity-dependent acidification affects neural excitability
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_PH_DYNAMICS_H
#define NIMCP_PH_DYNAMICS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

//=============================================================================
// Constants
//=============================================================================

/** Physiological extracellular pH */
#define PH_EXTRACELLULAR_NORMAL         7.4f

/** Physiological intracellular pH */
#define PH_INTRACELLULAR_NORMAL         7.2f

/** Synaptic vesicle pH (acidic for NT loading) */
#define PH_VESICLE_NORMAL               5.5f

/** Maximum pH deviation before critical */
#define PH_MAX_DEVIATION                0.5f

/** Minimum pH for function */
#define PH_MINIMUM_VIABLE               6.5f

/** Maximum pH for function */
#define PH_MAXIMUM_VIABLE               7.8f

/** Maximum number of monitored regions */
#define PH_MAX_REGIONS                  64

/** Maximum buffer systems per region */
#define PH_MAX_BUFFERS                  8

/** pH update timestep (ms) */
#define PH_UPDATE_TIMESTEP_MS           1.0f

//=============================================================================
// Error Codes
//=============================================================================

typedef enum {
    PH_OK = 0,
    PH_ERR_NULL_PTR = -1,
    PH_ERR_INVALID_PARAM = -2,
    PH_ERR_NOT_INITIALIZED = -3,
    PH_ERR_ALREADY_INITIALIZED = -4,
    PH_ERR_NO_MEMORY = -5,
    PH_ERR_REGION_NOT_FOUND = -6,
    PH_ERR_REGION_FULL = -7,
    PH_ERR_CRITICAL_ACIDOSIS = -8,
    PH_ERR_CRITICAL_ALKALOSIS = -9,
    PH_ERR_PUMP_FAILURE = -10
} nimcp_ph_error_t;

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief pH compartment types
 */
typedef enum {
    PH_COMPARTMENT_EXTRACELLULAR = 0,   /**< Extracellular space */
    PH_COMPARTMENT_INTRACELLULAR,        /**< Cytoplasm */
    PH_COMPARTMENT_VESICULAR,            /**< Synaptic vesicles */
    PH_COMPARTMENT_MITOCHONDRIAL,        /**< Mitochondrial matrix */
    PH_COMPARTMENT_COUNT
} nimcp_ph_compartment_t;

/**
 * @brief pH health status
 */
typedef enum {
    PH_STATUS_NORMAL = 0,               /**< pH within normal range */
    PH_STATUS_MILD_ACIDOSIS,            /**< Slight acidification */
    PH_STATUS_MILD_ALKALOSIS,           /**< Slight alkalinization */
    PH_STATUS_MODERATE_ACIDOSIS,        /**< Significant acidification */
    PH_STATUS_MODERATE_ALKALOSIS,       /**< Significant alkalinization */
    PH_STATUS_SEVERE_ACIDOSIS,          /**< Critical acidification */
    PH_STATUS_SEVERE_ALKALOSIS          /**< Critical alkalinization */
} nimcp_ph_status_t;

/**
 * @brief Buffer system type
 */
typedef enum {
    PH_BUFFER_BICARBONATE = 0,          /**< CO2/HCO3- system */
    PH_BUFFER_PHOSPHATE,                /**< H2PO4-/HPO4 2- system */
    PH_BUFFER_PROTEIN,                  /**< Protein buffering */
    PH_BUFFER_HEMOGLOBIN,               /**< Hemoglobin in blood */
    PH_BUFFER_COUNT
} nimcp_ph_buffer_type_t;

/**
 * @brief Proton pump type
 */
typedef enum {
    PH_PUMP_V_ATPASE = 0,               /**< Vesicular ATPase */
    PH_PUMP_NHE,                        /**< Na+/H+ exchanger */
    PH_PUMP_NBC,                        /**< Na+/HCO3- cotransporter */
    PH_PUMP_AE,                         /**< Anion exchanger (Cl-/HCO3-) */
    PH_PUMP_MCT,                        /**< Monocarboxylate transporter */
    PH_PUMP_COUNT
} nimcp_ph_pump_type_t;

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct nimcp_ph_system_s nimcp_ph_system_t;
typedef struct nimcp_ph_region_s nimcp_ph_region_t;
typedef struct nimcp_ph_buffer_s nimcp_ph_buffer_t;
typedef struct nimcp_ph_pump_s nimcp_ph_pump_t;
typedef struct nimcp_ph_config_s nimcp_ph_config_t;
typedef struct nimcp_ph_metrics_s nimcp_ph_metrics_t;

//=============================================================================
// Callback Types
//=============================================================================

/**
 * @brief Callback for pH status changes
 */
typedef void (*nimcp_ph_status_callback_t)(
    nimcp_ph_region_t* region,
    nimcp_ph_status_t old_status,
    nimcp_ph_status_t new_status,
    void* user_data
);

/**
 * @brief Callback for critical pH events
 */
typedef void (*nimcp_ph_critical_callback_t)(
    nimcp_ph_region_t* region,
    nimcp_ph_compartment_t compartment,
    float ph_value,
    void* user_data
);

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Buffer system state
 */
struct nimcp_ph_buffer_s {
    nimcp_ph_buffer_type_t type;        /**< Buffer type */
    float concentration;                /**< Buffer concentration (mM) */
    float pka;                          /**< Acid dissociation constant */
    float buffering_capacity;           /**< Beta value (mol/pH unit) */
    float saturation;                   /**< Current saturation (0-1) */
    bool active;                        /**< Buffer is active */
};

/**
 * @brief Proton pump state
 */
struct nimcp_ph_pump_s {
    nimcp_ph_pump_type_t type;          /**< Pump type */
    float max_activity;                 /**< Maximum pump rate */
    float current_activity;             /**< Current pump rate (0-1) */
    float km;                           /**< Michaelis constant */
    float atp_cost;                     /**< ATP per proton */
    nimcp_ph_compartment_t source;      /**< Source compartment */
    nimcp_ph_compartment_t target;      /**< Target compartment */
    bool enabled;                       /**< Pump is enabled */
};

/**
 * @brief pH region state
 */
struct nimcp_ph_region_s {
    uint32_t id;                        /**< Region identifier */
    char name[64];                      /**< Region name */

    /* Compartment pH values */
    float ph[PH_COMPARTMENT_COUNT];     /**< pH per compartment */
    float ph_target[PH_COMPARTMENT_COUNT]; /**< Target pH values */

    /* Buffer systems */
    nimcp_ph_buffer_t buffers[PH_MAX_BUFFERS];
    uint32_t num_buffers;

    /* Proton pumps */
    nimcp_ph_pump_t pumps[PH_PUMP_COUNT];

    /* Activity effects */
    float activity_level;               /**< Neural activity (0-1) */
    float metabolic_acid_production;    /**< Acid production rate */

    /* Status */
    nimcp_ph_status_t status;           /**< Current pH status */
    bool initialized;
};

/**
 * @brief pH system configuration
 */
struct nimcp_ph_config_s {
    /* Initial pH values */
    float initial_extracellular_ph;
    float initial_intracellular_ph;
    float initial_vesicular_ph;

    /* Buffering parameters */
    float bicarbonate_concentration;    /**< mM */
    float phosphate_concentration;      /**< mM */
    float protein_concentration;        /**< mM */

    /* Pump activities */
    float v_atpase_activity;            /**< Vesicular pump */
    float nhe_activity;                 /**< Na+/H+ exchanger */

    /* Dynamics parameters */
    float ph_recovery_rate;             /**< pH units/sec */
    float activity_acid_factor;         /**< Acid production per activity */

    /* Thresholds */
    float acidosis_threshold;           /**< pH below this is acidotic */
    float alkalosis_threshold;          /**< pH above this is alkalotic */

    /* Callbacks */
    nimcp_ph_status_callback_t on_status_change;
    nimcp_ph_critical_callback_t on_critical;
    void* callback_data;
};

/**
 * @brief pH system metrics
 */
struct nimcp_ph_metrics_s {
    /* Current values */
    float mean_extracellular_ph;
    float mean_intracellular_ph;
    float mean_vesicular_ph;

    /* Deviations */
    float max_ph_deviation;
    float min_ph_deviation;

    /* Buffer status */
    float total_buffering_capacity;
    float buffer_saturation;

    /* Pump activity */
    float total_pump_activity;
    float atp_consumption;

    /* Events */
    uint32_t acidosis_events;
    uint32_t alkalosis_events;
    uint32_t critical_events;

    /* Time tracking */
    uint64_t last_update_time;
    float total_simulation_time;
};

/**
 * @brief Main pH system
 */
struct nimcp_ph_system_s {
    /* Regions */
    nimcp_ph_region_t regions[PH_MAX_REGIONS];
    uint32_t num_regions;

    /* Configuration */
    nimcp_ph_config_t config;

    /* Metrics */
    nimcp_ph_metrics_t metrics;

    /* Effects on neural function */
    float conductance_modifier;         /**< Ion channel modulation */
    float release_modifier;             /**< NT release modulation */
    float metabolic_modifier;           /**< Metabolism modulation */

    /* State */
    bool initialized;
    uint64_t update_count;
};

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Initialize the pH system
 * @param system pH system to initialize
 * @param config Configuration (NULL for defaults)
 * @return PH_OK on success, error code otherwise
 */
NIMCP_EXPORT nimcp_ph_error_t nimcp_ph_init(
    nimcp_ph_system_t* system,
    const nimcp_ph_config_t* config
);

/**
 * @brief Shutdown the pH system
 * @param system pH system to shutdown
 * @return PH_OK on success
 */
NIMCP_EXPORT nimcp_ph_error_t nimcp_ph_shutdown(
    nimcp_ph_system_t* system
);

/**
 * @brief Reset pH system to initial state
 * @param system pH system to reset
 * @return PH_OK on success
 */
NIMCP_EXPORT nimcp_ph_error_t nimcp_ph_reset(
    nimcp_ph_system_t* system
);

//=============================================================================
// Region Management API
//=============================================================================

/**
 * @brief Add a region to the pH system
 * @param system pH system
 * @param name Region name
 * @param[out] region_id Assigned region ID
 * @return PH_OK on success
 */
NIMCP_EXPORT nimcp_ph_error_t nimcp_ph_add_region(
    nimcp_ph_system_t* system,
    const char* name,
    uint32_t* region_id
);

/**
 * @brief Get region by ID
 * @param system pH system
 * @param region_id Region ID
 * @return Region pointer or NULL
 */
NIMCP_EXPORT nimcp_ph_region_t* nimcp_ph_get_region(
    nimcp_ph_system_t* system,
    uint32_t region_id
);

/**
 * @brief Remove a region
 * @param system pH system
 * @param region_id Region ID to remove
 * @return PH_OK on success
 */
NIMCP_EXPORT nimcp_ph_error_t nimcp_ph_remove_region(
    nimcp_ph_system_t* system,
    uint32_t region_id
);

//=============================================================================
// pH Control API
//=============================================================================

/**
 * @brief Set pH for a compartment
 * @param region Region to modify
 * @param compartment Target compartment
 * @param ph_value New pH value
 * @return PH_OK on success
 */
NIMCP_EXPORT nimcp_ph_error_t nimcp_ph_set_compartment_ph(
    nimcp_ph_region_t* region,
    nimcp_ph_compartment_t compartment,
    float ph_value
);

/**
 * @brief Get pH for a compartment
 * @param region Region to query
 * @param compartment Target compartment
 * @param[out] ph_value Current pH value
 * @return PH_OK on success
 */
NIMCP_EXPORT nimcp_ph_error_t nimcp_ph_get_compartment_ph(
    const nimcp_ph_region_t* region,
    nimcp_ph_compartment_t compartment,
    float* ph_value
);

/**
 * @brief Apply acid load to a region
 * @param region Region to acidify
 * @param compartment Target compartment
 * @param acid_load Proton load (mM equivalent)
 * @return PH_OK on success
 */
NIMCP_EXPORT nimcp_ph_error_t nimcp_ph_apply_acid_load(
    nimcp_ph_region_t* region,
    nimcp_ph_compartment_t compartment,
    float acid_load
);

/**
 * @brief Apply base load to a region
 * @param region Region to alkalinize
 * @param compartment Target compartment
 * @param base_load Base load (mM equivalent)
 * @return PH_OK on success
 */
NIMCP_EXPORT nimcp_ph_error_t nimcp_ph_apply_base_load(
    nimcp_ph_region_t* region,
    nimcp_ph_compartment_t compartment,
    float base_load
);

//=============================================================================
// Buffer System API
//=============================================================================

/**
 * @brief Add a buffer system to a region
 * @param region Target region
 * @param type Buffer type
 * @param concentration Buffer concentration (mM)
 * @return PH_OK on success
 */
NIMCP_EXPORT nimcp_ph_error_t nimcp_ph_add_buffer(
    nimcp_ph_region_t* region,
    nimcp_ph_buffer_type_t type,
    float concentration
);

/**
 * @brief Get total buffering capacity
 * @param region Region to query
 * @param compartment Target compartment
 * @param[out] capacity Total buffering capacity
 * @return PH_OK on success
 */
NIMCP_EXPORT nimcp_ph_error_t nimcp_ph_get_buffering_capacity(
    const nimcp_ph_region_t* region,
    nimcp_ph_compartment_t compartment,
    float* capacity
);

/**
 * @brief Calculate buffer response to pH change
 * @param region Region to calculate
 * @param delta_h Proton concentration change
 * @param[out] delta_ph Resulting pH change
 * @return PH_OK on success
 */
NIMCP_EXPORT nimcp_ph_error_t nimcp_ph_calculate_buffer_response(
    const nimcp_ph_region_t* region,
    float delta_h,
    float* delta_ph
);

//=============================================================================
// Proton Pump API
//=============================================================================

/**
 * @brief Set pump activity
 * @param region Target region
 * @param pump_type Pump type
 * @param activity Activity level (0-1)
 * @return PH_OK on success
 */
NIMCP_EXPORT nimcp_ph_error_t nimcp_ph_set_pump_activity(
    nimcp_ph_region_t* region,
    nimcp_ph_pump_type_t pump_type,
    float activity
);

/**
 * @brief Get pump activity
 * @param region Target region
 * @param pump_type Pump type
 * @param[out] activity Current activity level
 * @return PH_OK on success
 */
NIMCP_EXPORT nimcp_ph_error_t nimcp_ph_get_pump_activity(
    const nimcp_ph_region_t* region,
    nimcp_ph_pump_type_t pump_type,
    float* activity
);

/**
 * @brief Enable/disable a pump
 * @param region Target region
 * @param pump_type Pump type
 * @param enabled Enable state
 * @return PH_OK on success
 */
NIMCP_EXPORT nimcp_ph_error_t nimcp_ph_set_pump_enabled(
    nimcp_ph_region_t* region,
    nimcp_ph_pump_type_t pump_type,
    bool enabled
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update pH system (single timestep)
 * @param system pH system to update
 * @param dt Time delta in milliseconds
 * @return PH_OK on success
 */
NIMCP_EXPORT nimcp_ph_error_t nimcp_ph_update(
    nimcp_ph_system_t* system,
    float dt
);

/**
 * @brief Update a single region
 * @param system pH system
 * @param region Region to update
 * @param dt Time delta in milliseconds
 * @return PH_OK on success
 */
NIMCP_EXPORT nimcp_ph_error_t nimcp_ph_update_region(
    nimcp_ph_system_t* system,
    nimcp_ph_region_t* region,
    float dt
);

/**
 * @brief Set neural activity level for a region
 * @param region Target region
 * @param activity Activity level (0-1)
 * @return PH_OK on success
 */
NIMCP_EXPORT nimcp_ph_error_t nimcp_ph_set_activity(
    nimcp_ph_region_t* region,
    float activity
);

//=============================================================================
// Effects API
//=============================================================================

/**
 * @brief Get pH effect on ion channel conductance
 * @param system pH system
 * @param region_id Region ID
 * @param[out] modifier Conductance modifier (0-1)
 * @return PH_OK on success
 */
NIMCP_EXPORT nimcp_ph_error_t nimcp_ph_get_conductance_modifier(
    const nimcp_ph_system_t* system,
    uint32_t region_id,
    float* modifier
);

/**
 * @brief Get pH effect on neurotransmitter release
 * @param system pH system
 * @param region_id Region ID
 * @param[out] modifier Release modifier (0-1)
 * @return PH_OK on success
 */
NIMCP_EXPORT nimcp_ph_error_t nimcp_ph_get_release_modifier(
    const nimcp_ph_system_t* system,
    uint32_t region_id,
    float* modifier
);

/**
 * @brief Get pH effect on metabolism
 * @param system pH system
 * @param region_id Region ID
 * @param[out] modifier Metabolic modifier (0-1)
 * @return PH_OK on success
 */
NIMCP_EXPORT nimcp_ph_error_t nimcp_ph_get_metabolic_modifier(
    const nimcp_ph_system_t* system,
    uint32_t region_id,
    float* modifier
);

/**
 * @brief Get overall neural function modifier
 * @param system pH system
 * @param[out] modifier Combined modifier (0-1)
 * @return PH_OK on success
 */
NIMCP_EXPORT nimcp_ph_error_t nimcp_ph_get_function_modifier(
    const nimcp_ph_system_t* system,
    float* modifier
);

//=============================================================================
// Status and Metrics API
//=============================================================================

/**
 * @brief Get pH system status
 * @param region Region to query
 * @return Current pH status
 */
NIMCP_EXPORT nimcp_ph_status_t nimcp_ph_get_status(
    const nimcp_ph_region_t* region
);

/**
 * @brief Get pH system metrics
 * @param system pH system
 * @param[out] metrics Metrics output
 * @return PH_OK on success
 */
NIMCP_EXPORT nimcp_ph_error_t nimcp_ph_get_metrics(
    const nimcp_ph_system_t* system,
    nimcp_ph_metrics_t* metrics
);

/**
 * @brief Check if pH is in critical range
 * @param region Region to check
 * @param compartment Compartment to check
 * @return true if critical
 */
NIMCP_EXPORT bool nimcp_ph_is_critical(
    const nimcp_ph_region_t* region,
    nimcp_ph_compartment_t compartment
);

/**
 * @brief Get error string for error code
 * @param error Error code
 * @return Human-readable error string
 */
NIMCP_EXPORT const char* nimcp_ph_error_string(nimcp_ph_error_t error);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PH_DYNAMICS_H */
