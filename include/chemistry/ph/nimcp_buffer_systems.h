/**
 * @file nimcp_buffer_systems.h
 * @brief Buffer Systems - pH buffering and homeostasis
 * @version 1.0.0
 * @date 2026-01-11
 *
 * WHAT: pH buffer system modeling for neural tissue
 * WHY:  Buffers maintain stable pH during metabolic activity
 * HOW:  Model bicarbonate, phosphate, and protein buffer systems
 *
 * KEY CONCEPTS:
 * - Bicarbonate Buffer: CO2 + H2O <-> H2CO3 <-> H+ + HCO3-
 * - Phosphate Buffer: H2PO4- <-> H+ + HPO4 2-
 * - Protein Buffer: Histidine residues accept/donate H+
 * - Henderson-Hasselbalch: pH = pKa + log([A-]/[HA])
 * - Buffering Capacity: Beta = dB/dpH (moles base per pH unit)
 *
 * BIOLOGICAL BASIS:
 * - Bicarbonate is the primary extracellular buffer
 * - Phosphate important intracellularly
 * - Proteins provide ~75% of intracellular buffering
 * - Carbonic anhydrase accelerates CO2/HCO3- equilibration
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_BUFFER_SYSTEMS_H
#define NIMCP_BUFFER_SYSTEMS_H

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

/** Bicarbonate pKa (CO2/HCO3-) */
#define BUFFER_BICARBONATE_PKA          6.1f

/** Phosphate pKa (H2PO4-/HPO4 2-) */
#define BUFFER_PHOSPHATE_PKA            6.8f

/** Histidine pKa (protein buffer) */
#define BUFFER_HISTIDINE_PKA            6.0f

/** Normal plasma HCO3- concentration (mM) */
#define BUFFER_HCO3_NORMAL              24.0f

/** Normal PCO2 (mmHg) */
#define BUFFER_PCO2_NORMAL              40.0f

/** Carbonic anhydrase acceleration factor */
#define BUFFER_CA_ACCELERATION          13000.0f

/** Solubility coefficient for CO2 */
#define BUFFER_CO2_SOLUBILITY           0.03f

/** Maximum buffer systems per manager */
#define BUFFER_MAX_SYSTEMS              16

//=============================================================================
// Error Codes
//=============================================================================

typedef enum {
    BUFFER_OK = 0,
    BUFFER_ERR_NULL_PTR = -1,
    BUFFER_ERR_INVALID_PARAM = -2,
    BUFFER_ERR_NOT_INITIALIZED = -3,
    BUFFER_ERR_BUFFER_EXHAUSTED = -4,
    BUFFER_ERR_CAPACITY_EXCEEDED = -5,
    BUFFER_ERR_INVALID_PH = -6
} nimcp_buffer_error_t;

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Buffer system types
 */
typedef enum {
    BUFFER_TYPE_BICARBONATE = 0,        /**< CO2/HCO3- system */
    BUFFER_TYPE_PHOSPHATE,              /**< H2PO4-/HPO4 2- system */
    BUFFER_TYPE_PROTEIN,                /**< Protein histidine residues */
    BUFFER_TYPE_HEMOGLOBIN,             /**< Hemoglobin (blood only) */
    BUFFER_TYPE_AMMONIA,                /**< NH3/NH4+ (renal) */
    BUFFER_TYPE_CUSTOM,                 /**< User-defined buffer */
    BUFFER_TYPE_COUNT
} nimcp_buffer_type_t;

/**
 * @brief Buffer state
 */
typedef enum {
    BUFFER_STATE_NORMAL = 0,            /**< Operating normally */
    BUFFER_STATE_DEPLETED,              /**< Low buffering capacity */
    BUFFER_STATE_SATURATED,             /**< At maximum capacity */
    BUFFER_STATE_REGENERATING           /**< Recovering capacity */
} nimcp_buffer_state_t;

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct nimcp_bicarbonate_buffer_s nimcp_bicarbonate_buffer_t;
typedef struct nimcp_phosphate_buffer_s nimcp_phosphate_buffer_t;
typedef struct nimcp_protein_buffer_s nimcp_protein_buffer_t;
typedef struct nimcp_buffer_component_s nimcp_buffer_component_t;
typedef struct nimcp_buffer_manager_s nimcp_buffer_manager_t;
typedef struct nimcp_buffer_config_s nimcp_buffer_config_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Bicarbonate buffer system (CO2/HCO3-)
 */
struct nimcp_bicarbonate_buffer_s {
    /* Concentrations */
    float hco3_concentration;           /**< Bicarbonate (mM) */
    float co2_concentration;            /**< Dissolved CO2 (mM) */
    float h2co3_concentration;          /**< Carbonic acid (mM) */

    /* Respiratory component */
    float pco2;                         /**< Partial pressure CO2 (mmHg) */
    float ventilation_factor;           /**< Respiratory compensation */

    /* Enzyme */
    float carbonic_anhydrase_activity;  /**< CA activity (0-1) */

    /* Parameters */
    float pka;                          /**< Effective pKa */
    float buffering_power;              /**< mM/pH unit */

    /* State */
    nimcp_buffer_state_t state;
    float saturation;                   /**< 0-1 saturation level */
};

/**
 * @brief Phosphate buffer system
 */
struct nimcp_phosphate_buffer_s {
    /* Concentrations */
    float h2po4_concentration;          /**< Dihydrogen phosphate (mM) */
    float hpo4_concentration;           /**< Hydrogen phosphate (mM) */
    float total_phosphate;              /**< Total phosphate (mM) */

    /* Parameters */
    float pka;                          /**< pKa = 6.8 */
    float buffering_power;              /**< mM/pH unit */

    /* State */
    nimcp_buffer_state_t state;
    float saturation;
};

/**
 * @brief Protein buffer system
 */
struct nimcp_protein_buffer_s {
    /* Properties */
    float total_protein;                /**< Total protein (g/L) */
    float histidine_content;            /**< Histidine moles per gram */
    float protonated_fraction;          /**< Fraction protonated */

    /* Parameters */
    float effective_pka;                /**< Effective pKa (varies) */
    float buffering_power;              /**< mM/pH unit */

    /* Specific proteins */
    float albumin_concentration;        /**< Albumin (g/L) */
    float globulin_concentration;       /**< Globulin (g/L) */
    float hemoglobin_concentration;     /**< Hemoglobin if present (g/L) */

    /* State */
    nimcp_buffer_state_t state;
    float saturation;
};

/**
 * @brief Generic buffer component
 */
struct nimcp_buffer_component_s {
    nimcp_buffer_type_t type;           /**< Buffer type */
    char name[32];                      /**< Buffer name */

    /* Acid-base properties */
    float pka;                          /**< Acid dissociation constant */
    float total_concentration;          /**< Total buffer concentration */
    float acid_form;                    /**< [HA] concentration */
    float base_form;                    /**< [A-] concentration */

    /* Buffering properties */
    float buffering_capacity;           /**< Beta value */
    float max_capacity;                 /**< Maximum capacity */

    /* State */
    nimcp_buffer_state_t state;
    float saturation;
    bool active;
};

/**
 * @brief Buffer system configuration
 */
struct nimcp_buffer_config_s {
    /* Bicarbonate parameters */
    float initial_hco3;                 /**< Initial HCO3- (mM) */
    float initial_pco2;                 /**< Initial PCO2 (mmHg) */
    float ca_activity;                  /**< Carbonic anhydrase activity */

    /* Phosphate parameters */
    float initial_phosphate;            /**< Total phosphate (mM) */

    /* Protein parameters */
    float total_protein;                /**< Total protein (g/L) */
    float albumin_fraction;             /**< Albumin as fraction of total */

    /* Regeneration */
    float regeneration_rate;            /**< Capacity recovery rate */

    /* Target pH */
    float target_ph;                    /**< Target pH for optimization */
};

/**
 * @brief Buffer manager - coordinates all buffer systems
 */
struct nimcp_buffer_manager_s {
    /* Individual buffer systems */
    nimcp_bicarbonate_buffer_t bicarbonate;
    nimcp_phosphate_buffer_t phosphate;
    nimcp_protein_buffer_t protein;

    /* Generic buffer array for extensibility */
    nimcp_buffer_component_t components[BUFFER_MAX_SYSTEMS];
    uint32_t num_components;

    /* Configuration */
    nimcp_buffer_config_t config;

    /* Combined metrics */
    float total_buffering_capacity;     /**< Sum of all capacities */
    float effective_buffer_power;       /**< Weighted average */
    float ph_stability_index;           /**< How stable is pH (0-1) */

    /* Current state */
    float current_ph;                   /**< Current pH being buffered */
    float proton_load;                  /**< Accumulated H+ load */

    /* State */
    bool initialized;
    uint64_t update_count;
};

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Initialize buffer manager
 * @param manager Buffer manager to initialize
 * @param config Configuration (NULL for defaults)
 * @return BUFFER_OK on success
 */
NIMCP_EXPORT nimcp_buffer_error_t nimcp_buffer_init(
    nimcp_buffer_manager_t* manager,
    const nimcp_buffer_config_t* config
);

/**
 * @brief Shutdown buffer manager
 * @param manager Buffer manager to shutdown
 * @return BUFFER_OK on success
 */
NIMCP_EXPORT nimcp_buffer_error_t nimcp_buffer_shutdown(
    nimcp_buffer_manager_t* manager
);

/**
 * @brief Reset buffer manager to initial state
 * @param manager Buffer manager to reset
 * @return BUFFER_OK on success
 */
NIMCP_EXPORT nimcp_buffer_error_t nimcp_buffer_reset(
    nimcp_buffer_manager_t* manager
);

//=============================================================================
// Bicarbonate Buffer API
//=============================================================================

/**
 * @brief Set bicarbonate concentration
 * @param buffer Bicarbonate buffer
 * @param concentration HCO3- concentration (mM)
 * @return BUFFER_OK on success
 */
NIMCP_EXPORT nimcp_buffer_error_t nimcp_bicarbonate_set_concentration(
    nimcp_bicarbonate_buffer_t* buffer,
    float concentration
);

/**
 * @brief Set PCO2 (respiratory component)
 * @param buffer Bicarbonate buffer
 * @param pco2 Partial pressure CO2 (mmHg)
 * @return BUFFER_OK on success
 */
NIMCP_EXPORT nimcp_buffer_error_t nimcp_bicarbonate_set_pco2(
    nimcp_bicarbonate_buffer_t* buffer,
    float pco2
);

/**
 * @brief Calculate pH from bicarbonate buffer
 * @param buffer Bicarbonate buffer
 * @param[out] ph Calculated pH
 * @return BUFFER_OK on success
 */
NIMCP_EXPORT nimcp_buffer_error_t nimcp_bicarbonate_calculate_ph(
    const nimcp_bicarbonate_buffer_t* buffer,
    float* ph
);

/**
 * @brief Apply acid load to bicarbonate buffer
 * @param buffer Bicarbonate buffer
 * @param h_load Proton load (mM)
 * @param[out] delta_ph Resulting pH change
 * @return BUFFER_OK on success
 */
NIMCP_EXPORT nimcp_buffer_error_t nimcp_bicarbonate_apply_acid(
    nimcp_bicarbonate_buffer_t* buffer,
    float h_load,
    float* delta_ph
);

//=============================================================================
// Phosphate Buffer API
//=============================================================================

/**
 * @brief Set total phosphate concentration
 * @param buffer Phosphate buffer
 * @param concentration Total phosphate (mM)
 * @return BUFFER_OK on success
 */
NIMCP_EXPORT nimcp_buffer_error_t nimcp_phosphate_set_concentration(
    nimcp_phosphate_buffer_t* buffer,
    float concentration
);

/**
 * @brief Calculate phosphate buffer response
 * @param buffer Phosphate buffer
 * @param current_ph Current pH
 * @param h_load Proton load
 * @param[out] delta_ph pH change
 * @return BUFFER_OK on success
 */
NIMCP_EXPORT nimcp_buffer_error_t nimcp_phosphate_apply_acid(
    nimcp_phosphate_buffer_t* buffer,
    float current_ph,
    float h_load,
    float* delta_ph
);

//=============================================================================
// Protein Buffer API
//=============================================================================

/**
 * @brief Set protein concentration
 * @param buffer Protein buffer
 * @param concentration Total protein (g/L)
 * @return BUFFER_OK on success
 */
NIMCP_EXPORT nimcp_buffer_error_t nimcp_protein_set_concentration(
    nimcp_protein_buffer_t* buffer,
    float concentration
);

/**
 * @brief Calculate protein buffer response
 * @param buffer Protein buffer
 * @param current_ph Current pH
 * @param h_load Proton load
 * @param[out] delta_ph pH change
 * @return BUFFER_OK on success
 */
NIMCP_EXPORT nimcp_buffer_error_t nimcp_protein_apply_acid(
    nimcp_protein_buffer_t* buffer,
    float current_ph,
    float h_load,
    float* delta_ph
);

//=============================================================================
// Buffer Manager API
//=============================================================================

/**
 * @brief Add custom buffer component
 * @param manager Buffer manager
 * @param type Buffer type
 * @param name Buffer name
 * @param pka Acid dissociation constant
 * @param concentration Total concentration
 * @return BUFFER_OK on success
 */
NIMCP_EXPORT nimcp_buffer_error_t nimcp_buffer_add_component(
    nimcp_buffer_manager_t* manager,
    nimcp_buffer_type_t type,
    const char* name,
    float pka,
    float concentration
);

/**
 * @brief Apply proton load to all buffers
 * @param manager Buffer manager
 * @param h_load Proton load (mM)
 * @param current_ph Current pH
 * @param[out] new_ph New pH after buffering
 * @return BUFFER_OK on success
 */
NIMCP_EXPORT nimcp_buffer_error_t nimcp_buffer_apply_acid_load(
    nimcp_buffer_manager_t* manager,
    float h_load,
    float current_ph,
    float* new_ph
);

/**
 * @brief Apply base load to all buffers
 * @param manager Buffer manager
 * @param oh_load Base load (mM)
 * @param current_ph Current pH
 * @param[out] new_ph New pH after buffering
 * @return BUFFER_OK on success
 */
NIMCP_EXPORT nimcp_buffer_error_t nimcp_buffer_apply_base_load(
    nimcp_buffer_manager_t* manager,
    float oh_load,
    float current_ph,
    float* new_ph
);

/**
 * @brief Get total buffering capacity
 * @param manager Buffer manager
 * @param[out] capacity Total capacity (mM/pH)
 * @return BUFFER_OK on success
 */
NIMCP_EXPORT nimcp_buffer_error_t nimcp_buffer_get_total_capacity(
    const nimcp_buffer_manager_t* manager,
    float* capacity
);

/**
 * @brief Update buffer system (regeneration, equilibration)
 * @param manager Buffer manager
 * @param dt Time delta (ms)
 * @return BUFFER_OK on success
 */
NIMCP_EXPORT nimcp_buffer_error_t nimcp_buffer_update(
    nimcp_buffer_manager_t* manager,
    float dt
);

//=============================================================================
// Henderson-Hasselbalch Calculations
//=============================================================================

/**
 * @brief Calculate pH using Henderson-Hasselbalch equation
 * @param pka Acid dissociation constant
 * @param base_form [A-] concentration
 * @param acid_form [HA] concentration
 * @return Calculated pH
 */
NIMCP_EXPORT float nimcp_buffer_henderson_hasselbalch(
    float pka,
    float base_form,
    float acid_form
);

/**
 * @brief Calculate buffer ratio from pH
 * @param ph Current pH
 * @param pka Acid dissociation constant
 * @return [A-]/[HA] ratio
 */
NIMCP_EXPORT float nimcp_buffer_ratio_from_ph(
    float ph,
    float pka
);

/**
 * @brief Calculate buffering capacity at given pH
 * @param total_buffer Total buffer concentration
 * @param pka Acid dissociation constant
 * @param ph Current pH
 * @return Buffering capacity (beta)
 */
NIMCP_EXPORT float nimcp_buffer_capacity_at_ph(
    float total_buffer,
    float pka,
    float ph
);

//=============================================================================
// Utility API
//=============================================================================

/**
 * @brief Convert pH to H+ concentration
 * @param ph pH value
 * @return H+ concentration (M)
 */
NIMCP_EXPORT float nimcp_buffer_ph_to_h(float ph);

/**
 * @brief Convert H+ concentration to pH
 * @param h_concentration H+ concentration (M)
 * @return pH value
 */
NIMCP_EXPORT float nimcp_buffer_h_to_ph(float h_concentration);

/**
 * @brief Get buffer state as string
 * @param state Buffer state
 * @return Human-readable state string
 */
NIMCP_EXPORT const char* nimcp_buffer_state_string(nimcp_buffer_state_t state);

/**
 * @brief Get error string for error code
 * @param error Error code
 * @return Human-readable error string
 */
NIMCP_EXPORT const char* nimcp_buffer_error_string(nimcp_buffer_error_t error);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BUFFER_SYSTEMS_H */
