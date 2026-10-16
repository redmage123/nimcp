/**
 * @file nimcp_proton_pumps.h
 * @brief Proton Pump Systems - Active pH regulation mechanisms
 * @version 1.0.0
 * @date 2026-01-11
 *
 * WHAT: Detailed proton pump modeling for pH regulation
 * WHY:  Proton pumps are essential for vesicle acidification and pH homeostasis
 * HOW:  Model V-ATPase, NHE, NBC, and other proton transporters
 *
 * KEY CONCEPTS:
 * - V-ATPase: Vacuolar H+-ATPase acidifies synaptic vesicles
 * - NHE: Na+/H+ exchanger regulates intracellular pH
 * - NBC: Na+/HCO3- cotransporter for bicarbonate transport
 * - AE: Anion exchanger (Cl-/HCO3-) for chloride/bicarbonate exchange
 * - MCT: Monocarboxylate transporter for lactate/H+ cotransport
 *
 * BIOLOGICAL BASIS:
 * - V-ATPase creates ~pH 5.5 in vesicles for NT loading
 * - NHE exports H+ during intracellular acidification
 * - These pumps require ATP and are regulated by cellular state
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_PROTON_PUMPS_H
#define NIMCP_PROTON_PUMPS_H

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

/** V-ATPase maximum turnover rate (H+/sec) */
#define PUMP_VATPASE_MAX_RATE           100.0f

/** NHE maximum exchange rate (H+/sec) */
#define PUMP_NHE_MAX_RATE               50.0f

/** NBC maximum transport rate (HCO3-/sec) */
#define PUMP_NBC_MAX_RATE               30.0f

/** AE maximum exchange rate (Cl-/sec) */
#define PUMP_AE_MAX_RATE                20.0f

/** MCT maximum cotransport rate (lactate/sec) */
#define PUMP_MCT_MAX_RATE               40.0f

/** ATP cost per V-ATPase cycle (ATP/3H+) */
#define PUMP_VATPASE_ATP_COST           1.0f

/** ATP equivalent for NHE (uses Na+ gradient) */
#define PUMP_NHE_ATP_EQUIVALENT         0.33f

/** Maximum pump density per region */
#define PUMP_MAX_DENSITY                1000

/** Pump activation time constant (ms) */
#define PUMP_ACTIVATION_TAU             50.0f

//=============================================================================
// Error Codes
//=============================================================================

typedef enum {
    PUMP_OK = 0,
    PUMP_ERR_NULL_PTR = -1,
    PUMP_ERR_INVALID_PARAM = -2,
    PUMP_ERR_NOT_INITIALIZED = -3,
    PUMP_ERR_NO_ATP = -4,
    PUMP_ERR_GRADIENT_DEPLETED = -5,
    PUMP_ERR_PUMP_INHIBITED = -6,
    PUMP_ERR_CAPACITY_EXCEEDED = -7
} nimcp_pump_error_t;

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief V-ATPase subunit composition
 */
typedef enum {
    VATPASE_SUBUNIT_V0 = 0,             /**< Membrane domain */
    VATPASE_SUBUNIT_V1,                  /**< Cytoplasmic domain */
    VATPASE_SUBUNIT_COUNT
} nimcp_vatpase_subunit_t;

/**
 * @brief NHE isoforms
 */
typedef enum {
    NHE_ISOFORM_NHE1 = 0,               /**< Ubiquitous housekeeping */
    NHE_ISOFORM_NHE2,                    /**< Epithelial */
    NHE_ISOFORM_NHE3,                    /**< Epithelial */
    NHE_ISOFORM_NHE5,                    /**< Brain-specific */
    NHE_ISOFORM_COUNT
} nimcp_nhe_isoform_t;

/**
 * @brief Pump regulation state
 */
typedef enum {
    PUMP_STATE_INACTIVE = 0,            /**< Pump not operating */
    PUMP_STATE_BASAL,                    /**< Baseline activity */
    PUMP_STATE_ACTIVATED,                /**< Enhanced activity */
    PUMP_STATE_INHIBITED,                /**< Reduced activity */
    PUMP_STATE_SATURATED                 /**< Maximum capacity */
} nimcp_pump_state_t;

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct nimcp_vatpase_s nimcp_vatpase_t;
typedef struct nimcp_nhe_s nimcp_nhe_t;
typedef struct nimcp_nbc_s nimcp_nbc_t;
typedef struct nimcp_pump_system_s nimcp_pump_system_t;
typedef struct nimcp_pump_config_s nimcp_pump_config_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief V-ATPase pump state
 */
struct nimcp_vatpase_s {
    /* Kinetics */
    float max_rate;                     /**< Maximum H+/sec */
    float current_rate;                 /**< Current H+/sec */
    float km_atp;                       /**< ATP Km (mM) */
    float km_h;                         /**< H+ Km for inhibition */

    /* Energy coupling */
    float coupling_efficiency;          /**< H+/ATP ratio */
    float atp_hydrolysis_rate;          /**< ATP consumed/sec */

    /* Regulation */
    float v0_v1_assembly;               /**< Assembly state (0-1) */
    float glucose_sensitivity;          /**< Glucose regulation */
    float ph_sensitivity;               /**< Luminal pH inhibition */

    /* State */
    nimcp_pump_state_t state;
    float activity_level;               /**< Current activity (0-1) */

    /* Metrics */
    uint64_t total_h_transported;
    float total_atp_consumed;
};

/**
 * @brief NHE exchanger state
 */
struct nimcp_nhe_s {
    nimcp_nhe_isoform_t isoform;        /**< NHE isoform type */

    /* Kinetics */
    float max_rate;                     /**< Maximum exchange rate */
    float current_rate;                 /**< Current exchange rate */
    float km_h_in;                      /**< Intracellular H+ Km */
    float km_na_out;                    /**< Extracellular Na+ Km */

    /* Allosteric regulation */
    float h_modifier_site;              /**< H+ modifier site occupancy */
    float set_point;                    /**< pH set point */

    /* Regulation */
    float phosphorylation_state;        /**< PKC/growth factor regulation */
    float calmodulin_binding;           /**< Ca2+/calmodulin regulation */

    /* State */
    nimcp_pump_state_t state;
    float activity_level;

    /* Metrics */
    uint64_t total_exchanges;
};

/**
 * @brief NBC cotransporter state
 */
struct nimcp_nbc_s {
    /* Kinetics */
    float max_rate;                     /**< Maximum transport rate */
    float current_rate;                 /**< Current transport rate */
    float stoichiometry;                /**< Na+:HCO3- ratio (1:2 or 1:3) */

    /* Gradients */
    float na_gradient;                  /**< Na+ driving force */
    float hco3_gradient;                /**< HCO3- driving force */

    /* Regulation */
    float camp_sensitivity;             /**< cAMP regulation */
    float carbonic_anhydrase_coupling;  /**< CA activity coupling */

    /* State */
    nimcp_pump_state_t state;
    float activity_level;

    /* Metrics */
    uint64_t total_transported;
};

/**
 * @brief Pump system configuration
 */
struct nimcp_pump_config_s {
    /* V-ATPase settings */
    float vatpase_density;              /**< Pumps per vesicle */
    float vatpase_max_rate;             /**< Maximum rate */

    /* NHE settings */
    nimcp_nhe_isoform_t nhe_isoform;    /**< Which NHE isoform */
    float nhe_density;                  /**< Exchangers per cell */
    float nhe_set_point;                /**< Target intracellular pH */

    /* NBC settings */
    float nbc_density;                  /**< Cotransporters per cell */
    float nbc_stoichiometry;            /**< Na+:HCO3- ratio */

    /* ATP availability */
    float atp_concentration;            /**< Available ATP (mM) */
    float atp_regeneration_rate;        /**< ATP resynthesis rate */

    /* Ion gradients */
    float na_gradient;                  /**< Na+ gradient strength */
    float cl_concentration;             /**< Cl- concentration */
};

/**
 * @brief Complete pump system
 */
struct nimcp_pump_system_s {
    /* Individual pumps */
    nimcp_vatpase_t vatpase;
    nimcp_nhe_t nhe;
    nimcp_nbc_t nbc;

    /* Configuration */
    nimcp_pump_config_t config;

    /* Resource tracking */
    float atp_available;                /**< Current ATP */
    float atp_consumed;                 /**< Total ATP used */

    /* Net proton flux */
    float net_h_flux;                   /**< Net H+ movement */
    float net_hco3_flux;                /**< Net HCO3- movement */

    /* State */
    bool initialized;
    uint64_t update_count;
};

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Initialize pump system
 * @param system Pump system to initialize
 * @param config Configuration (NULL for defaults)
 * @return PUMP_OK on success
 */
NIMCP_EXPORT nimcp_pump_error_t nimcp_pump_init(
    nimcp_pump_system_t* system,
    const nimcp_pump_config_t* config
);

/**
 * @brief Shutdown pump system
 * @param system Pump system to shutdown
 * @return PUMP_OK on success
 */
NIMCP_EXPORT nimcp_pump_error_t nimcp_pump_shutdown(
    nimcp_pump_system_t* system
);

/**
 * @brief Reset pump system to initial state
 * @param system Pump system to reset
 * @return PUMP_OK on success
 */
NIMCP_EXPORT nimcp_pump_error_t nimcp_pump_reset(
    nimcp_pump_system_t* system
);

//=============================================================================
// V-ATPase API
//=============================================================================

/**
 * @brief Set V-ATPase activity
 * @param vatpase V-ATPase pump
 * @param activity Activity level (0-1)
 * @return PUMP_OK on success
 */
NIMCP_EXPORT nimcp_pump_error_t nimcp_vatpase_set_activity(
    nimcp_vatpase_t* vatpase,
    float activity
);

/**
 * @brief Update V-ATPase assembly state
 * @param vatpase V-ATPase pump
 * @param assembly V0-V1 assembly level (0-1)
 * @return PUMP_OK on success
 */
NIMCP_EXPORT nimcp_pump_error_t nimcp_vatpase_set_assembly(
    nimcp_vatpase_t* vatpase,
    float assembly
);

/**
 * @brief Calculate V-ATPase proton flux
 * @param vatpase V-ATPase pump
 * @param atp_available Available ATP
 * @param luminal_ph Current vesicle pH
 * @param[out] h_flux Proton flux (H+/sec)
 * @param[out] atp_cost ATP consumed
 * @return PUMP_OK on success
 */
NIMCP_EXPORT nimcp_pump_error_t nimcp_vatpase_calculate_flux(
    const nimcp_vatpase_t* vatpase,
    float atp_available,
    float luminal_ph,
    float* h_flux,
    float* atp_cost
);

//=============================================================================
// NHE API
//=============================================================================

/**
 * @brief Set NHE activity
 * @param nhe NHE exchanger
 * @param activity Activity level (0-1)
 * @return PUMP_OK on success
 */
NIMCP_EXPORT nimcp_pump_error_t nimcp_nhe_set_activity(
    nimcp_nhe_t* nhe,
    float activity
);

/**
 * @brief Set NHE pH set point
 * @param nhe NHE exchanger
 * @param set_point Target intracellular pH
 * @return PUMP_OK on success
 */
NIMCP_EXPORT nimcp_pump_error_t nimcp_nhe_set_setpoint(
    nimcp_nhe_t* nhe,
    float set_point
);

/**
 * @brief Calculate NHE exchange rate
 * @param nhe NHE exchanger
 * @param intracellular_ph Current intracellular pH
 * @param extracellular_na Extracellular Na+ (mM)
 * @param[out] exchange_rate H+ export rate
 * @return PUMP_OK on success
 */
NIMCP_EXPORT nimcp_pump_error_t nimcp_nhe_calculate_exchange(
    const nimcp_nhe_t* nhe,
    float intracellular_ph,
    float extracellular_na,
    float* exchange_rate
);

//=============================================================================
// NBC API
//=============================================================================

/**
 * @brief Set NBC activity
 * @param nbc NBC cotransporter
 * @param activity Activity level (0-1)
 * @return PUMP_OK on success
 */
NIMCP_EXPORT nimcp_pump_error_t nimcp_nbc_set_activity(
    nimcp_nbc_t* nbc,
    float activity
);

/**
 * @brief Calculate NBC transport rate
 * @param nbc NBC cotransporter
 * @param na_gradient Na+ gradient
 * @param hco3_gradient HCO3- gradient
 * @param[out] transport_rate Net transport rate
 * @return PUMP_OK on success
 */
NIMCP_EXPORT nimcp_pump_error_t nimcp_nbc_calculate_transport(
    const nimcp_nbc_t* nbc,
    float na_gradient,
    float hco3_gradient,
    float* transport_rate
);

//=============================================================================
// System Update API
//=============================================================================

/**
 * @brief Update entire pump system
 * @param system Pump system to update
 * @param dt Time delta (ms)
 * @param intracellular_ph Current intracellular pH
 * @param extracellular_ph Current extracellular pH
 * @param vesicular_ph Current vesicular pH
 * @return PUMP_OK on success
 */
NIMCP_EXPORT nimcp_pump_error_t nimcp_pump_update(
    nimcp_pump_system_t* system,
    float dt,
    float intracellular_ph,
    float extracellular_ph,
    float vesicular_ph
);

/**
 * @brief Get net proton flux from all pumps
 * @param system Pump system
 * @param[out] h_flux Net H+ flux
 * @return PUMP_OK on success
 */
NIMCP_EXPORT nimcp_pump_error_t nimcp_pump_get_net_h_flux(
    const nimcp_pump_system_t* system,
    float* h_flux
);

/**
 * @brief Get total ATP consumption
 * @param system Pump system
 * @param[out] atp_rate ATP consumption rate
 * @return PUMP_OK on success
 */
NIMCP_EXPORT nimcp_pump_error_t nimcp_pump_get_atp_consumption(
    const nimcp_pump_system_t* system,
    float* atp_rate
);

/**
 * @brief Supply ATP to pump system
 * @param system Pump system
 * @param atp_amount ATP to add (mM)
 * @return PUMP_OK on success
 */
NIMCP_EXPORT nimcp_pump_error_t nimcp_pump_supply_atp(
    nimcp_pump_system_t* system,
    float atp_amount
);

//=============================================================================
// Utility API
//=============================================================================

/**
 * @brief Get pump state as string
 * @param state Pump state
 * @return Human-readable state string
 */
NIMCP_EXPORT const char* nimcp_pump_state_string(nimcp_pump_state_t state);

/**
 * @brief Get error string for error code
 * @param error Error code
 * @return Human-readable error string
 */
NIMCP_EXPORT const char* nimcp_pump_error_string(nimcp_pump_error_t error);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PROTON_PUMPS_H */
