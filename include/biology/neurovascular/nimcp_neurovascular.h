/**
 * @file nimcp_neurovascular.h
 * @brief Neurovascular Coupling Module - Brain blood flow regulation
 * @version 1.0.0
 * @date 2026-01-11
 *
 * WHAT: Neurovascular coupling and hemodynamic response modeling
 * WHY:  Neural activity requires blood flow for oxygen/glucose delivery (fMRI basis)
 * HOW:  Model astrocyte-mediated coupling, blood flow, and BOLD signal
 *
 * KEY CONCEPTS:
 * - Neurovascular Coupling: Neural activity triggers local blood flow increase
 * - BOLD Signal: Blood Oxygen Level Dependent signal for fMRI
 * - Hemodynamic Response Function (HRF): Temporal dynamics of blood response
 * - Cerebral Blood Flow (CBF): Volume of blood per time per tissue mass
 * - Cerebral Blood Volume (CBV): Total blood volume in tissue
 * - Oxygen Extraction Fraction (OEF): Ratio of O2 consumed to O2 delivered
 *
 * BIOLOGICAL BASIS:
 * - Neurons signal astrocytes via glutamate and K+
 * - Astrocytes release vasoactive substances (NO, prostaglandins)
 * - Smooth muscle relaxation increases vessel diameter
 * - Increased CBF brings oxygenated hemoglobin
 * - BOLD signal reflects deoxyhemoglobin concentration
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_NEUROVASCULAR_H
#define NIMCP_NEUROVASCULAR_H

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

/** Baseline cerebral blood flow (mL/100g/min) */
#define NVC_BASELINE_CBF                50.0f

/** Baseline cerebral blood volume (mL/100g) */
#define NVC_BASELINE_CBV                4.0f

/** Baseline oxygen extraction fraction */
#define NVC_BASELINE_OEF                0.4f

/** Maximum CBF increase factor */
#define NVC_MAX_CBF_INCREASE            2.5f

/** HRF time to peak (seconds) */
#define NVC_HRF_TIME_TO_PEAK            5.0f

/** HRF duration (seconds) */
#define NVC_HRF_DURATION                20.0f

/** Post-stimulus undershoot depth */
#define NVC_UNDERSHOOT_DEPTH            0.1f

/** Maximum neurovascular units */
#define NVC_MAX_UNITS                   128

/** HRF kernel size (samples) */
#define NVC_HRF_KERNEL_SIZE             30

/** Neurovascular coupling delay (seconds) */
#define NVC_COUPLING_DELAY              0.5f

//=============================================================================
// Error Codes
//=============================================================================

typedef enum {
    NVC_OK = 0,
    NVC_ERR_NULL_PTR = -1,
    NVC_ERR_INVALID_PARAM = -2,
    NVC_ERR_NOT_INITIALIZED = -3,
    NVC_ERR_ALREADY_INITIALIZED = -4,
    NVC_ERR_NO_MEMORY = -5,
    NVC_ERR_UNIT_NOT_FOUND = -6,
    NVC_ERR_CAPACITY_EXCEEDED = -7,
    NVC_ERR_HYPOPERFUSION = -8,
    NVC_ERR_HYPERPERFUSION = -9
} nimcp_nvc_error_t;

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Neurovascular unit state
 */
typedef enum {
    NVC_STATE_RESTING = 0,              /**< Baseline state */
    NVC_STATE_ACTIVATED,                 /**< Increased perfusion */
    NVC_STATE_PEAK,                      /**< Maximum response */
    NVC_STATE_UNDERSHOOT,                /**< Post-stimulus dip */
    NVC_STATE_RECOVERING                 /**< Returning to baseline */
} nimcp_nvc_state_t;

/**
 * @brief Blood vessel type
 */
typedef enum {
    NVC_VESSEL_ARTERIOLE = 0,           /**< Small arteries (primary regulation) */
    NVC_VESSEL_CAPILLARY,                /**< Exchange vessels */
    NVC_VESSEL_VENULE,                   /**< Small veins */
    NVC_VESSEL_COUNT
} nimcp_nvc_vessel_t;

/**
 * @brief Vasoactive mechanism
 */
typedef enum {
    NVC_MECHANISM_NO = 0,               /**< Nitric oxide */
    NVC_MECHANISM_PROSTAGLANDIN,         /**< Prostaglandins */
    NVC_MECHANISM_K_CHANNEL,             /**< Potassium channels */
    NVC_MECHANISM_ADENOSINE,             /**< Adenosine (metabolic) */
    NVC_MECHANISM_COUNT
} nimcp_nvc_mechanism_t;

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct nimcp_nvc_unit_s nimcp_nvc_unit_t;
typedef struct nimcp_nvc_hrf_s nimcp_nvc_hrf_t;
typedef struct nimcp_nvc_bold_s nimcp_nvc_bold_t;
typedef struct nimcp_nvc_system_s nimcp_nvc_system_t;
typedef struct nimcp_nvc_config_s nimcp_nvc_config_t;
typedef struct nimcp_nvc_metrics_s nimcp_nvc_metrics_t;

//=============================================================================
// Callback Types
//=============================================================================

/**
 * @brief Callback for perfusion changes
 */
typedef void (*nimcp_nvc_perfusion_callback_t)(
    nimcp_nvc_unit_t* unit,
    float old_cbf,
    float new_cbf,
    void* user_data
);

/**
 * @brief Callback for BOLD signal updates
 */
typedef void (*nimcp_nvc_bold_callback_t)(
    nimcp_nvc_unit_t* unit,
    float bold_signal,
    void* user_data
);

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Hemodynamic Response Function
 */
struct nimcp_nvc_hrf_s {
    float kernel[NVC_HRF_KERNEL_SIZE];  /**< HRF kernel samples */
    float time_to_peak;                 /**< Time to maximum response (s) */
    float undershoot_ratio;             /**< Post-stimulus undershoot */
    float fwhm;                         /**< Full width at half maximum */
    uint32_t kernel_length;             /**< Effective kernel length */
    float dt;                           /**< Sample interval (s) */
};

/**
 * @brief BOLD signal state
 */
struct nimcp_nvc_bold_s {
    float signal;                       /**< Current BOLD signal (% change) */
    float cbf_change;                   /**< CBF change (% baseline) */
    float cbv_change;                   /**< CBV change (% baseline) */
    float cmro2_change;                 /**< CMRO2 change (% baseline) */
    float deoxyhemoglobin;              /**< Relative dHb concentration */
    float oef;                          /**< Oxygen extraction fraction */
};

/**
 * @brief Neurovascular unit (region with coupled neurons/vessels)
 */
struct nimcp_nvc_unit_s {
    uint32_t id;                        /**< Unit identifier */
    char name[64];                      /**< Unit name */
    float position[3];                  /**< 3D position */

    /* Neural activity */
    float neural_activity;              /**< Input activity level (0-1) */
    float activity_history[NVC_HRF_KERNEL_SIZE]; /**< Recent activity */
    uint32_t history_index;             /**< Current history position */

    /* Blood flow */
    float cbf;                          /**< Cerebral blood flow */
    float cbf_baseline;                 /**< Baseline CBF */
    float cbf_target;                   /**< Target CBF from activity */

    /* Blood volume */
    float cbv;                          /**< Cerebral blood volume */
    float cbv_baseline;                 /**< Baseline CBV */

    /* Oxygenation */
    float oef;                          /**< Oxygen extraction fraction */
    float cmro2;                        /**< Cerebral metabolic rate O2 */

    /* Vessel state */
    float vessel_diameter;              /**< Relative diameter (1.0 = baseline) */
    float vessel_tone;                  /**< Vascular tone (0=dilated, 1=constricted) */

    /* Vasoactive signals */
    float no_level;                     /**< Nitric oxide */
    float prostaglandin_level;          /**< Prostaglandins */
    float adenosine_level;              /**< Adenosine */
    float potassium_level;              /**< Extracellular K+ */

    /* Astrocyte coupling */
    float astrocyte_calcium;            /**< Astrocyte Ca2+ wave */
    float astrocyte_coupling;           /**< Coupling strength */

    /* HRF */
    nimcp_nvc_hrf_t hrf;                /**< Hemodynamic response function */

    /* BOLD */
    nimcp_nvc_bold_t bold;              /**< BOLD signal state */

    /* State */
    nimcp_nvc_state_t state;
    float time_since_activation;        /**< Time since last activation */
    bool initialized;
};

/**
 * @brief Neurovascular system configuration
 */
struct nimcp_nvc_config_s {
    /* Baseline values */
    float baseline_cbf;                 /**< mL/100g/min */
    float baseline_cbv;                 /**< mL/100g */
    float baseline_oef;                 /**< Fraction */

    /* HRF parameters */
    float hrf_time_to_peak;             /**< seconds */
    float hrf_undershoot_ratio;         /**< Relative undershoot */
    float hrf_duration;                 /**< Total response duration */

    /* Coupling parameters */
    float coupling_strength;            /**< Neural-vascular coupling */
    float coupling_delay;               /**< Coupling delay (s) */

    /* Response limits */
    float max_cbf_increase;             /**< Maximum CBF ratio */
    float max_cbv_increase;             /**< Maximum CBV ratio */

    /* BOLD parameters (Balloon model) */
    float tau_mtt;                      /**< Mean transit time (s) */
    float alpha_grubb;                  /**< Grubb's exponent */
    float e0;                           /**< Resting O2 extraction */

    /* Callbacks */
    nimcp_nvc_perfusion_callback_t on_perfusion_change;
    nimcp_nvc_bold_callback_t on_bold_update;
    void* callback_data;
};

/**
 * @brief Neurovascular system metrics
 */
struct nimcp_nvc_metrics_s {
    /* Averages */
    float mean_cbf;
    float mean_cbv;
    float mean_bold;
    float mean_oef;

    /* Extremes */
    float max_cbf;
    float min_cbf;
    float max_bold;
    float min_bold;

    /* Activity */
    uint32_t activated_units;
    uint32_t total_units;

    /* Events */
    uint32_t activation_events;
    uint32_t perfusion_warnings;

    /* Time */
    float total_simulation_time;
    uint64_t update_count;
};

/**
 * @brief Main neurovascular system
 */
struct nimcp_nvc_system_s {
    /* Units */
    nimcp_nvc_unit_t units[NVC_MAX_UNITS];
    uint32_t num_units;

    /* Configuration */
    nimcp_nvc_config_t config;

    /* Metrics */
    nimcp_nvc_metrics_t metrics;

    /* Global state */
    float global_cbf;                   /**< System-wide CBF */
    float global_bold;                  /**< System-wide BOLD */
    float perfusion_reserve;            /**< Available flow increase */

    /* State */
    bool initialized;
    uint64_t update_count;
    float current_time;                 /**< Simulation time (s) */
};

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Initialize neurovascular system
 * @param system System to initialize
 * @param config Configuration (NULL for defaults)
 * @return NVC_OK on success
 */
NIMCP_EXPORT nimcp_nvc_error_t nimcp_nvc_init(
    nimcp_nvc_system_t* system,
    const nimcp_nvc_config_t* config
);

/**
 * @brief Shutdown neurovascular system
 * @param system System to shutdown
 * @return NVC_OK on success
 */
NIMCP_EXPORT nimcp_nvc_error_t nimcp_nvc_shutdown(
    nimcp_nvc_system_t* system
);

/**
 * @brief Reset to baseline
 * @param system System to reset
 * @return NVC_OK on success
 */
NIMCP_EXPORT nimcp_nvc_error_t nimcp_nvc_reset(
    nimcp_nvc_system_t* system
);

//=============================================================================
// Unit Management API
//=============================================================================

/**
 * @brief Add neurovascular unit
 * @param system System
 * @param name Unit name
 * @param position 3D position
 * @param[out] unit_id Assigned ID
 * @return NVC_OK on success
 */
NIMCP_EXPORT nimcp_nvc_error_t nimcp_nvc_add_unit(
    nimcp_nvc_system_t* system,
    const char* name,
    const float position[3],
    uint32_t* unit_id
);

/**
 * @brief Get unit by ID
 * @param system System
 * @param unit_id Unit ID
 * @return Unit pointer or NULL
 */
NIMCP_EXPORT nimcp_nvc_unit_t* nimcp_nvc_get_unit(
    nimcp_nvc_system_t* system,
    uint32_t unit_id
);

/**
 * @brief Remove unit
 * @param system System
 * @param unit_id Unit to remove
 * @return NVC_OK on success
 */
NIMCP_EXPORT nimcp_nvc_error_t nimcp_nvc_remove_unit(
    nimcp_nvc_system_t* system,
    uint32_t unit_id
);

//=============================================================================
// Neural Activity API
//=============================================================================

/**
 * @brief Set neural activity for a unit
 * @param unit Unit to activate
 * @param activity Activity level (0-1)
 * @return NVC_OK on success
 */
NIMCP_EXPORT nimcp_nvc_error_t nimcp_nvc_set_activity(
    nimcp_nvc_unit_t* unit,
    float activity
);

/**
 * @brief Apply activity pulse
 * @param unit Unit
 * @param amplitude Pulse amplitude
 * @param duration Duration (ms)
 * @return NVC_OK on success
 */
NIMCP_EXPORT nimcp_nvc_error_t nimcp_nvc_apply_stimulus(
    nimcp_nvc_unit_t* unit,
    float amplitude,
    float duration
);

/**
 * @brief Set vasoactive signal level
 * @param unit Unit
 * @param mechanism Which vasoactive mechanism
 * @param level Signal level
 * @return NVC_OK on success
 */
NIMCP_EXPORT nimcp_nvc_error_t nimcp_nvc_set_vasoactive(
    nimcp_nvc_unit_t* unit,
    nimcp_nvc_mechanism_t mechanism,
    float level
);

//=============================================================================
// Blood Flow API
//=============================================================================

/**
 * @brief Get current CBF
 * @param unit Unit
 * @param[out] cbf CBF value
 * @return NVC_OK on success
 */
NIMCP_EXPORT nimcp_nvc_error_t nimcp_nvc_get_cbf(
    const nimcp_nvc_unit_t* unit,
    float* cbf
);

/**
 * @brief Get CBF change from baseline
 * @param unit Unit
 * @param[out] change CBF change (% baseline)
 * @return NVC_OK on success
 */
NIMCP_EXPORT nimcp_nvc_error_t nimcp_nvc_get_cbf_change(
    const nimcp_nvc_unit_t* unit,
    float* change
);

/**
 * @brief Get oxygen extraction fraction
 * @param unit Unit
 * @param[out] oef OEF value
 * @return NVC_OK on success
 */
NIMCP_EXPORT nimcp_nvc_error_t nimcp_nvc_get_oef(
    const nimcp_nvc_unit_t* unit,
    float* oef
);

//=============================================================================
// BOLD Signal API
//=============================================================================

/**
 * @brief Get BOLD signal
 * @param unit Unit
 * @param[out] bold BOLD signal (% change)
 * @return NVC_OK on success
 */
NIMCP_EXPORT nimcp_nvc_error_t nimcp_nvc_get_bold(
    const nimcp_nvc_unit_t* unit,
    float* bold
);

/**
 * @brief Get BOLD state
 * @param unit Unit
 * @param[out] bold_state BOLD state structure
 * @return NVC_OK on success
 */
NIMCP_EXPORT nimcp_nvc_error_t nimcp_nvc_get_bold_state(
    const nimcp_nvc_unit_t* unit,
    nimcp_nvc_bold_t* bold_state
);

/**
 * @brief Generate synthetic fMRI timeseries
 * @param unit Unit
 * @param stimulus_times Array of stimulus onset times (s)
 * @param num_stimuli Number of stimuli
 * @param dt Sampling interval (s)
 * @param duration Total duration (s)
 * @param[out] timeseries Output array
 * @param[out] num_samples Number of samples generated
 * @return NVC_OK on success
 */
NIMCP_EXPORT nimcp_nvc_error_t nimcp_nvc_generate_fmri(
    nimcp_nvc_unit_t* unit,
    const float* stimulus_times,
    uint32_t num_stimuli,
    float dt,
    float duration,
    float* timeseries,
    uint32_t* num_samples
);

//=============================================================================
// HRF API
//=============================================================================

/**
 * @brief Initialize HRF with default double-gamma parameters
 * @param hrf HRF structure
 * @param dt Sample interval (s)
 * @return NVC_OK on success
 */
NIMCP_EXPORT nimcp_nvc_error_t nimcp_nvc_init_hrf(
    nimcp_nvc_hrf_t* hrf,
    float dt
);

/**
 * @brief Set custom HRF parameters
 * @param hrf HRF structure
 * @param time_to_peak Peak time (s)
 * @param undershoot_ratio Undershoot depth
 * @param fwhm Full width at half max (s)
 * @return NVC_OK on success
 */
NIMCP_EXPORT nimcp_nvc_error_t nimcp_nvc_set_hrf_params(
    nimcp_nvc_hrf_t* hrf,
    float time_to_peak,
    float undershoot_ratio,
    float fwhm
);

/**
 * @brief Convolve activity with HRF
 * @param hrf HRF structure
 * @param activity_history Activity history array
 * @param history_length Length of history
 * @param[out] response Hemodynamic response
 * @return NVC_OK on success
 */
NIMCP_EXPORT nimcp_nvc_error_t nimcp_nvc_convolve_hrf(
    const nimcp_nvc_hrf_t* hrf,
    const float* activity_history,
    uint32_t history_length,
    float* response
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update system (single timestep)
 * @param system System
 * @param dt Time delta (ms)
 * @return NVC_OK on success
 */
NIMCP_EXPORT nimcp_nvc_error_t nimcp_nvc_update(
    nimcp_nvc_system_t* system,
    float dt
);

/**
 * @brief Update single unit
 * @param system System
 * @param unit Unit to update
 * @param dt Time delta (ms)
 * @return NVC_OK on success
 */
NIMCP_EXPORT nimcp_nvc_error_t nimcp_nvc_update_unit(
    nimcp_nvc_system_t* system,
    nimcp_nvc_unit_t* unit,
    float dt
);

//=============================================================================
// Metrics API
//=============================================================================

/**
 * @brief Get system metrics
 * @param system System
 * @param[out] metrics Metrics output
 * @return NVC_OK on success
 */
NIMCP_EXPORT nimcp_nvc_error_t nimcp_nvc_get_metrics(
    const nimcp_nvc_system_t* system,
    nimcp_nvc_metrics_t* metrics
);

/**
 * @brief Get unit state
 * @param unit Unit
 * @return Current state
 */
NIMCP_EXPORT nimcp_nvc_state_t nimcp_nvc_get_state(
    const nimcp_nvc_unit_t* unit
);

/**
 * @brief Get error string
 * @param error Error code
 * @return Human-readable string
 */
NIMCP_EXPORT const char* nimcp_nvc_error_string(nimcp_nvc_error_t error);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NEUROVASCULAR_H */
