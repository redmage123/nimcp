/**
 * @file nimcp_hypothalamus_adapter.h
 * @brief Brain adapter for Hypothalamus integration
 *
 * WHAT: Unified adapter connecting hypothalamus functions to the brain system
 * WHY:  Enable homeostatic regulation, circadian rhythms, stress response, and autonomic control
 * HOW:  Orchestrates SCN, HPA axis, and autonomic nuclei as a cohesive unit
 *
 * ARCHITECTURE:
 * - Wraps all hypothalamic sub-modules (SCN, PVN, LH, VMH, DMH, arcuate)
 * - Provides high-level API for homeostatic regulation
 * - Integrates with limbic system for emotional influences
 * - Connects to brainstem for autonomic output
 * - Connects to pituitary for neuroendocrine control
 *
 * BIOLOGICAL BASIS:
 * - Models hypothalamic nuclei and their specific functions
 * - Suprachiasmatic Nucleus (SCN): Master circadian pacemaker
 * - Paraventricular Nucleus (PVN): HPA axis, autonomic output
 * - Lateral Hypothalamus (LH): Arousal, feeding
 * - Ventromedial Hypothalamus (VMH): Satiety, defensive behavior
 * - Arcuate Nucleus: Appetite, metabolism, GnRH neurons
 *
 * KEY FUNCTIONS:
 * - Homeostatic regulation: Temperature, hunger, thirst, osmolality
 * - Circadian rhythms: Sleep-wake cycle, hormone release timing
 * - Stress response: HPA axis activation, cortisol cascade
 * - Autonomic control: Sympathetic/parasympathetic balance
 *
 * @version Phase H1: Hypothalamus Brain Integration
 * @date 2025-12-30
 */

#ifndef NIMCP_HYPOTHALAMUS_ADAPTER_H
#define NIMCP_HYPOTHALAMUS_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Bio-async communication system */
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

/* Logging system */
#include "utils/logging/nimcp_logging.h"

/* Unified memory system */
#include "utils/memory/nimcp_unified_memory.h"

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Default configuration values
 */
#define HYPOTHALAMUS_DEFAULT_CIRCADIAN_PERIOD_HOURS   24.0f
#define HYPOTHALAMUS_DEFAULT_TEMP_SETPOINT_C          37.0f
#define HYPOTHALAMUS_DEFAULT_CORTISOL_BASELINE        0.3f
#define HYPOTHALAMUS_DEFAULT_AUTONOMIC_BALANCE        0.5f
#define HYPOTHALAMUS_DEFAULT_HUNGER_THRESHOLD         0.7f
#define HYPOTHALAMUS_DEFAULT_THIRST_THRESHOLD         0.6f

/**
 * @brief Hypothalamus adapter configuration
 */
typedef struct {
    /* Circadian settings */
    float circadian_period_hours;    /**< Circadian period (default: 24.0) */
    float initial_phase;             /**< Initial circadian phase [0, 2*PI] */
    bool enable_circadian;           /**< Enable circadian rhythm simulation */

    /* Homeostatic setpoints */
    float temperature_setpoint_c;    /**< Core body temperature setpoint */
    float osmolality_setpoint;       /**< Blood osmolality setpoint (mOsm/kg) */
    float glucose_setpoint;          /**< Blood glucose setpoint (mg/dL) */

    /* Stress response */
    bool enable_hpa_axis;            /**< Enable HPA axis simulation */
    float cortisol_baseline;         /**< Baseline cortisol level [0, 1] */
    float crh_sensitivity;           /**< CRH release sensitivity */
    float cortisol_feedback_gain;    /**< Negative feedback strength */

    /* Autonomic control */
    bool enable_autonomic;           /**< Enable autonomic regulation */
    float sympathetic_bias;          /**< Sympathetic bias [0=para, 1=sympa] */

    /* Appetite and metabolism */
    bool enable_appetite;            /**< Enable hunger/satiety simulation */
    float hunger_threshold;          /**< Hunger drive activation threshold */
    float thirst_threshold;          /**< Thirst drive activation threshold */
    float leptin_sensitivity;        /**< Leptin receptor sensitivity */
    float ghrelin_sensitivity;       /**< Ghrelin receptor sensitivity */

    /* Integration */
    bool enable_limbic_input;        /**< Enable amygdala/hippocampus input */
    bool enable_brainstem_output;    /**< Enable medulla/pons output */
    bool enable_pituitary_link;      /**< Enable neuroendocrine output */

    /* Bio-async communication */
    bool enable_bio_async;           /**< Enable bio-async messaging */
    nimcp_bio_channel_type_t default_channel; /**< Default neuromodulator channel */

    /* Event system */
    bool enable_events;              /**< Enable event bus integration */
} hypothalamus_config_t;

/*=============================================================================
 * STATUS AND STATE
 *===========================================================================*/

/**
 * @brief Hypothalamus processing status
 */
typedef enum {
    HYPOTHALAMUS_STATUS_IDLE = 0,        /**< Normal operation */
    HYPOTHALAMUS_STATUS_STRESS_RESPONSE, /**< HPA axis activated */
    HYPOTHALAMUS_STATUS_THERMAL_ALERT,   /**< Temperature out of range */
    HYPOTHALAMUS_STATUS_HUNGER_DRIVE,    /**< Hunger drive active */
    HYPOTHALAMUS_STATUS_THIRST_DRIVE,    /**< Thirst drive active */
    HYPOTHALAMUS_STATUS_CIRCADIAN_SHIFT, /**< Circadian phase transition */
    HYPOTHALAMUS_STATUS_AUTONOMIC_ALERT, /**< Autonomic imbalance */
    HYPOTHALAMUS_STATUS_ERROR            /**< Error state */
} hypothalamus_status_t;

/**
 * @brief Error codes for hypothalamus operations
 */
typedef enum {
    HYPOTHALAMUS_ERROR_NONE = 0,
    HYPOTHALAMUS_ERROR_INVALID_CONFIG,
    HYPOTHALAMUS_ERROR_HOMEOSTATIC_FAILURE,
    HYPOTHALAMUS_ERROR_CIRCADIAN_DISRUPTION,
    HYPOTHALAMUS_ERROR_HPA_DYSFUNCTION,
    HYPOTHALAMUS_ERROR_AUTONOMIC_IMBALANCE,
    HYPOTHALAMUS_ERROR_LIMBIC_DISCONNECT,
    HYPOTHALAMUS_ERROR_PITUITARY_FAILURE,
    HYPOTHALAMUS_ERROR_INTERNAL
} hypothalamus_error_t;

/*=============================================================================
 * CIRCADIAN RHYTHM STRUCTURES
 *===========================================================================*/

/**
 * @brief Hypothalamus circadian phase descriptor
 *
 * Note: Uses hypothalamus-specific naming to avoid conflict with medulla circadian types
 */
typedef enum {
    HYPO_CIRCADIAN_PHASE_EARLY_MORNING = 0,  /**< 06:00-09:00 - cortisol peak, waking */
    HYPO_CIRCADIAN_PHASE_LATE_MORNING,       /**< 09:00-12:00 - high alertness */
    HYPO_CIRCADIAN_PHASE_EARLY_AFTERNOON,    /**< 12:00-15:00 - post-meal dip */
    HYPO_CIRCADIAN_PHASE_LATE_AFTERNOON,     /**< 15:00-18:00 - second alertness peak */
    HYPO_CIRCADIAN_PHASE_EVENING,            /**< 18:00-21:00 - melatonin onset */
    HYPO_CIRCADIAN_PHASE_EARLY_NIGHT,        /**< 21:00-00:00 - sleep preparation */
    HYPO_CIRCADIAN_PHASE_MID_NIGHT,          /**< 00:00-03:00 - deep sleep */
    HYPO_CIRCADIAN_PHASE_LATE_NIGHT          /**< 03:00-06:00 - REM peak */
} hypo_circadian_phase_t;

/**
 * @brief Hypothalamus circadian rhythm state
 */
typedef struct {
    float phase;                      /**< Current phase [0, 2*PI] */
    float amplitude;                  /**< Rhythm amplitude [0, 1] */
    hypo_circadian_phase_t period;    /**< Current circadian period */
    float melatonin_level;            /**< Melatonin concentration [0, 1] */
    float cortisol_level;             /**< Cortisol concentration [0, 1] */
    float alertness;                  /**< Alertness level [0, 1] */
    float sleep_pressure;             /**< Homeostatic sleep drive [0, 1] */
    uint64_t current_time_us;         /**< Internal time counter */
} hypo_circadian_state_t;

/*=============================================================================
 * HOMEOSTATIC REGULATION STRUCTURES
 *===========================================================================*/

/**
 * @brief Homeostatic parameter with setpoint and error
 */
typedef struct {
    float setpoint;              /**< Target value */
    float current_value;         /**< Measured value */
    float error;                 /**< Deviation from setpoint */
    float integral_error;        /**< Accumulated error (for integral control) */
    float correction_signal;     /**< Output correction signal */
} homeostatic_parameter_t;

/**
 * @brief Thermoregulation state
 */
typedef struct {
    homeostatic_parameter_t core_temp;  /**< Core body temperature */
    float skin_temp;                     /**< Peripheral temperature */
    float heat_production;               /**< Metabolic heat generation */
    float heat_loss;                     /**< Heat dissipation rate */
    bool shivering_active;               /**< Shivering thermogenesis active */
    bool sweating_active;                /**< Evaporative cooling active */
    bool vasoconstriction;               /**< Peripheral vasoconstriction */
    bool vasodilation;                   /**< Peripheral vasodilation */
} thermoregulation_state_t;

/**
 * @brief Hunger and satiety state
 */
typedef struct {
    homeostatic_parameter_t blood_glucose; /**< Blood glucose level */
    float ghrelin_level;                   /**< Ghrelin (hunger hormone) */
    float leptin_level;                    /**< Leptin (satiety hormone) */
    float npy_level;                       /**< Neuropeptide Y (appetite) */
    float pomc_level;                      /**< POMC (satiety) */
    float hunger_drive;                    /**< Computed hunger drive [0, 1] */
    float satiety_signal;                  /**< Satiety inhibition [0, 1] */
    bool feeding_motivated;                /**< Feeding behavior triggered */
} appetite_state_t;

/**
 * @brief Thirst and osmolality state
 */
typedef struct {
    homeostatic_parameter_t osmolality;  /**< Blood osmolality */
    homeostatic_parameter_t blood_volume; /**< Blood volume */
    float vasopressin_level;              /**< ADH/vasopressin [0, 1] */
    float thirst_drive;                   /**< Computed thirst drive [0, 1] */
    bool drinking_motivated;              /**< Drinking behavior triggered */
} hydration_state_t;

/*=============================================================================
 * HPA AXIS STRUCTURES
 *===========================================================================*/

/**
 * @brief HPA axis state (hypothalamic-pituitary-adrenal)
 */
typedef struct {
    float crh_level;             /**< CRH from PVN [0, 1] */
    float acth_level;            /**< ACTH from pituitary [0, 1] */
    float cortisol_level;        /**< Cortisol from adrenal [0, 1] */
    float stress_input;          /**< Total stress signal [0, 1] */
    float negative_feedback;     /**< Cortisol negative feedback */
    bool chronic_stress;         /**< Chronic stress detected */
    float hpa_sensitivity;       /**< HPA axis sensitivity (can adapt) */
    uint64_t last_activation_us; /**< Last HPA activation time */
    uint32_t activation_count;   /**< Number of recent activations */
} hpa_axis_state_t;

/*=============================================================================
 * AUTONOMIC CONTROL STRUCTURES
 *===========================================================================*/

/**
 * @brief Autonomic nervous system state
 */
typedef struct {
    float sympathetic_tone;      /**< Sympathetic activity [0, 1] */
    float parasympathetic_tone;  /**< Parasympathetic activity [0, 1] */
    float heart_rate_mod;        /**< Heart rate modulation factor */
    float blood_pressure_mod;    /**< Blood pressure modulation factor */
    float respiratory_rate_mod;  /**< Respiratory rate modulation */
    float pupil_dilation;        /**< Pupil diameter [0=constricted, 1=dilated] */
    float digestive_activity;    /**< GI activity level [0, 1] */
    bool fight_or_flight;        /**< Acute stress response active */
    bool rest_and_digest;        /**< Parasympathetic dominance */
} autonomic_state_t;

/*=============================================================================
 * COMPLETE STATE STRUCTURE
 *===========================================================================*/

/**
 * @brief Complete hypothalamus state
 */
typedef struct {
    /* Circadian rhythm */
    hypo_circadian_state_t circadian;

    /* Homeostatic regulation */
    thermoregulation_state_t thermoregulation;
    appetite_state_t appetite;
    hydration_state_t hydration;

    /* Stress response */
    hpa_axis_state_t hpa_axis;

    /* Autonomic control */
    autonomic_state_t autonomic;

    /* Overall status */
    hypothalamus_status_t status;
    hypothalamus_error_t last_error;
    uint64_t current_time_us;
} hypothalamus_state_t;

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

/**
 * @brief Adapter statistics
 */
typedef struct {
    /* Update counts */
    uint64_t updates_processed;      /**< Total update cycles */
    uint64_t circadian_ticks;        /**< Circadian clock ticks */
    uint64_t homeostatic_corrections; /**< Homeostatic corrections */

    /* Stress events */
    uint64_t stress_activations;     /**< HPA axis activations */
    uint64_t chronic_stress_episodes; /**< Chronic stress detected */

    /* Homeostatic events */
    uint64_t thermal_alerts;         /**< Temperature out of range */
    uint64_t hunger_episodes;        /**< Hunger drive activations */
    uint64_t thirst_episodes;        /**< Thirst drive activations */

    /* Autonomic events */
    uint64_t sympathetic_bursts;     /**< Fight-or-flight episodes */
    uint64_t parasympathetic_switches; /**< Rest-and-digest transitions */

    /* Timing */
    float avg_update_latency_us;     /**< Average update latency */
    float max_update_latency_us;     /**< Maximum update latency */
} hypothalamus_stats_t;

/*=============================================================================
 * CALLBACK TYPES
 *===========================================================================*/

/**
 * @brief Callback for homeostatic alerts
 */
typedef void (*hypothalamus_alert_callback_t)(
    hypothalamus_status_t status,
    const void* alert_data,
    void* user_data
);

/**
 * @brief Callback for neuroendocrine output
 */
typedef void (*hypothalamus_hormone_callback_t)(
    uint32_t hormone_type,
    float level,
    void* user_data
);

/**
 * @brief Callback for autonomic output
 */
typedef void (*hypothalamus_autonomic_callback_t)(
    const autonomic_state_t* autonomic,
    void* user_data
);

/*=============================================================================
 * OPAQUE TYPE
 *===========================================================================*/

/** @brief Opaque hypothalamus adapter type */
typedef struct hypothalamus_adapter hypothalamus_adapter_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get default configuration
 *
 * WHAT: Returns default configuration for hypothalamus adapter
 * WHY:  Provide biologically-motivated defaults for common use cases
 * HOW:  Initialize all fields with reasonable values
 *
 * @return Default configuration structure
 */
hypothalamus_config_t hypothalamus_default_config(void);

/**
 * @brief Create hypothalamus adapter
 *
 * WHAT: Allocate and initialize the adapter with all sub-modules
 * WHY:  Central point for hypothalamic function initialization
 * HOW:  Create SCN, homeostatic controllers, HPA axis, autonomic regulators
 *
 * @param config Configuration (NULL for defaults)
 * @return New adapter instance, or NULL on failure
 */
hypothalamus_adapter_t* hypothalamus_create(const hypothalamus_config_t* config);

/**
 * @brief Destroy hypothalamus adapter
 *
 * WHAT: Free all resources associated with the adapter
 * WHY:  Prevent memory leaks
 * HOW:  Destroy sub-modules, free internal state
 *
 * @param adapter Adapter to destroy
 */
void hypothalamus_destroy(hypothalamus_adapter_t* adapter);

/**
 * @brief Reset adapter state
 *
 * WHAT: Reset to initial homeostatic state
 * WHY:  Prepare for new simulation without full reinitialization
 * HOW:  Reset all sub-modules to baseline values
 *
 * @param adapter Adapter instance
 * @return true on success, false on failure
 */
bool hypothalamus_reset(hypothalamus_adapter_t* adapter);

/*=============================================================================
 * CIRCADIAN RHYTHM FUNCTIONS
 *===========================================================================*/

/**
 * @brief Update circadian clock
 *
 * WHAT: Advance the circadian oscillator
 * WHY:  Maintain accurate 24-hour rhythm
 * HOW:  Update phase, compute melatonin/cortisol levels
 *
 * @param adapter Adapter instance
 * @param delta_time_us Time elapsed in microseconds
 * @return true on success
 */
bool hypothalamus_update_circadian(hypothalamus_adapter_t* adapter,
                                    uint64_t delta_time_us);

/**
 * @brief Get current circadian phase
 *
 * @param adapter Adapter instance
 * @return Current circadian phase
 */
hypo_circadian_phase_t hypothalamus_get_circadian_phase(
    const hypothalamus_adapter_t* adapter);

/**
 * @brief Get circadian state
 *
 * @param adapter Adapter instance
 * @param state Output circadian state
 * @return true on success
 */
bool hypothalamus_get_circadian_state(const hypothalamus_adapter_t* adapter,
                                       hypo_circadian_state_t* state);

/**
 * @brief Apply light exposure
 *
 * WHAT: Simulate light input to SCN
 * WHY:  Light is the primary zeitgeber for circadian entrainment
 * HOW:  Phase-shift the circadian oscillator
 *
 * @param adapter Adapter instance
 * @param intensity Light intensity [0, 1]
 * @param duration_ms Duration of exposure
 * @return Phase shift in radians
 */
float hypothalamus_apply_light(hypothalamus_adapter_t* adapter,
                                float intensity,
                                float duration_ms);

/*=============================================================================
 * HOMEOSTATIC REGULATION FUNCTIONS
 *===========================================================================*/

/**
 * @brief Update homeostatic regulation
 *
 * WHAT: Update all homeostatic controllers
 * WHY:  Maintain body temperature, blood glucose, etc.
 * HOW:  PID-like control with biological constraints
 *
 * @param adapter Adapter instance
 * @param delta_time_us Time elapsed in microseconds
 * @return true on success
 */
bool hypothalamus_update_homeostasis(hypothalamus_adapter_t* adapter,
                                      uint64_t delta_time_us);

/**
 * @brief Set core temperature input
 *
 * @param adapter Adapter instance
 * @param temperature_c Current core temperature in Celsius
 * @return true on success
 */
bool hypothalamus_set_temperature(hypothalamus_adapter_t* adapter,
                                   float temperature_c);

/**
 * @brief Set blood glucose input
 *
 * @param adapter Adapter instance
 * @param glucose_mg_dl Current blood glucose in mg/dL
 * @return true on success
 */
bool hypothalamus_set_blood_glucose(hypothalamus_adapter_t* adapter,
                                     float glucose_mg_dl);

/**
 * @brief Set blood osmolality input
 *
 * @param adapter Adapter instance
 * @param osmolality_mosm Current osmolality in mOsm/kg
 * @return true on success
 */
bool hypothalamus_set_osmolality(hypothalamus_adapter_t* adapter,
                                  float osmolality_mosm);

/**
 * @brief Get thermoregulation state
 *
 * @param adapter Adapter instance
 * @param state Output thermoregulation state
 * @return true on success
 */
bool hypothalamus_get_thermoregulation(const hypothalamus_adapter_t* adapter,
                                        thermoregulation_state_t* state);

/**
 * @brief Get appetite state
 *
 * @param adapter Adapter instance
 * @param state Output appetite state
 * @return true on success
 */
bool hypothalamus_get_appetite(const hypothalamus_adapter_t* adapter,
                                appetite_state_t* state);

/**
 * @brief Get hydration state
 *
 * @param adapter Adapter instance
 * @param state Output hydration state
 * @return true on success
 */
bool hypothalamus_get_hydration(const hypothalamus_adapter_t* adapter,
                                 hydration_state_t* state);

/*=============================================================================
 * HPA AXIS (STRESS RESPONSE) FUNCTIONS
 *===========================================================================*/

/**
 * @brief Update HPA axis
 *
 * WHAT: Update stress response system
 * WHY:  Model cortisol cascade and negative feedback
 * HOW:  CRH → ACTH → Cortisol with feedback inhibition
 *
 * @param adapter Adapter instance
 * @param delta_time_us Time elapsed in microseconds
 * @return true on success
 */
bool hypothalamus_update_hpa_axis(hypothalamus_adapter_t* adapter,
                                   uint64_t delta_time_us);

/**
 * @brief Apply stress input
 *
 * WHAT: Signal stress to the HPA axis
 * WHY:  External stressors activate cortisol release
 * HOW:  Increase CRH release from PVN
 *
 * @param adapter Adapter instance
 * @param stress_level Stress intensity [0, 1]
 * @return Resulting cortisol change
 */
float hypothalamus_apply_stress(hypothalamus_adapter_t* adapter,
                                 float stress_level);

/**
 * @brief Get HPA axis state
 *
 * @param adapter Adapter instance
 * @param state Output HPA state
 * @return true on success
 */
bool hypothalamus_get_hpa_state(const hypothalamus_adapter_t* adapter,
                                 hpa_axis_state_t* state);

/**
 * @brief Get current cortisol level
 *
 * @param adapter Adapter instance
 * @return Cortisol level [0, 1]
 */
float hypothalamus_get_cortisol(const hypothalamus_adapter_t* adapter);

/*=============================================================================
 * AUTONOMIC CONTROL FUNCTIONS
 *===========================================================================*/

/**
 * @brief Update autonomic nervous system
 *
 * WHAT: Update sympathetic/parasympathetic balance
 * WHY:  Regulate heart rate, blood pressure, digestion
 * HOW:  Integrate inputs from stress, circadian, homeostatic systems
 *
 * @param adapter Adapter instance
 * @param delta_time_us Time elapsed in microseconds
 * @return true on success
 */
bool hypothalamus_update_autonomic(hypothalamus_adapter_t* adapter,
                                    uint64_t delta_time_us);

/**
 * @brief Get autonomic state
 *
 * @param adapter Adapter instance
 * @param state Output autonomic state
 * @return true on success
 */
bool hypothalamus_get_autonomic(const hypothalamus_adapter_t* adapter,
                                 autonomic_state_t* state);

/**
 * @brief Get sympathetic/parasympathetic balance
 *
 * @param adapter Adapter instance
 * @return Balance [0=fully parasympathetic, 1=fully sympathetic]
 */
float hypothalamus_get_autonomic_balance(const hypothalamus_adapter_t* adapter);

/*=============================================================================
 * INTEGRATED UPDATE FUNCTION
 *===========================================================================*/

/**
 * @brief Update all hypothalamus subsystems
 *
 * WHAT: Complete update cycle for all functions
 * WHY:  Single entry point for brain integration
 * HOW:  Update circadian, homeostasis, HPA, autonomic in sequence
 *
 * @param adapter Adapter instance
 * @param delta_time_us Time elapsed in microseconds
 * @return true on success
 */
bool hypothalamus_update(hypothalamus_adapter_t* adapter,
                          uint64_t delta_time_us);

/**
 * @brief Get complete hypothalamus state
 *
 * @param adapter Adapter instance
 * @param state Output complete state
 * @return true on success
 */
bool hypothalamus_get_state(const hypothalamus_adapter_t* adapter,
                             hypothalamus_state_t* state);

/*=============================================================================
 * STATUS AND DIAGNOSTICS
 *===========================================================================*/

/**
 * @brief Get current processing status
 *
 * @param adapter Adapter instance
 * @return Current status
 */
hypothalamus_status_t hypothalamus_get_status(
    const hypothalamus_adapter_t* adapter);

/**
 * @brief Get last error code
 *
 * @param adapter Adapter instance
 * @return Last error, or HYPOTHALAMUS_ERROR_NONE
 */
hypothalamus_error_t hypothalamus_get_last_error(
    const hypothalamus_adapter_t* adapter);

/**
 * @brief Get error description string
 *
 * @param error Error code
 * @return Human-readable error description
 */
const char* hypothalamus_error_string(hypothalamus_error_t error);

/**
 * @brief Get status description string
 *
 * @param status Status code
 * @return Human-readable status description
 */
const char* hypothalamus_status_string(hypothalamus_status_t status);

/**
 * @brief Get adapter statistics
 *
 * @param adapter Adapter instance
 * @param stats Output statistics structure
 * @return true on success
 */
bool hypothalamus_get_stats(const hypothalamus_adapter_t* adapter,
                             hypothalamus_stats_t* stats);

/**
 * @brief Get adapter configuration
 *
 * @param adapter Adapter instance
 * @param config Output configuration structure
 * @return true on success
 */
bool hypothalamus_get_config(const hypothalamus_adapter_t* adapter,
                              hypothalamus_config_t* config);

/*=============================================================================
 * CALLBACK REGISTRATION
 *===========================================================================*/

/**
 * @brief Set homeostatic alert callback
 *
 * @param adapter Adapter instance
 * @param callback Alert handler function
 * @param user_data User context passed to callback
 * @return true on success
 */
bool hypothalamus_set_alert_callback(hypothalamus_adapter_t* adapter,
                                      hypothalamus_alert_callback_t callback,
                                      void* user_data);

/**
 * @brief Set hormone output callback
 *
 * @param adapter Adapter instance
 * @param callback Hormone handler function
 * @param user_data User context passed to callback
 * @return true on success
 */
bool hypothalamus_set_hormone_callback(hypothalamus_adapter_t* adapter,
                                        hypothalamus_hormone_callback_t callback,
                                        void* user_data);

/**
 * @brief Set autonomic output callback
 *
 * @param adapter Adapter instance
 * @param callback Autonomic handler function
 * @param user_data User context passed to callback
 * @return true on success
 */
bool hypothalamus_set_autonomic_callback(hypothalamus_adapter_t* adapter,
                                          hypothalamus_autonomic_callback_t callback,
                                          void* user_data);

/*=============================================================================
 * BIO-ASYNC COMMUNICATION
 *===========================================================================*/

/**
 * @brief Get bio-async module context
 *
 * WHAT: Returns the bio-async module context for hypothalamus
 * WHY:  Allow external modules to send messages to hypothalamus
 * HOW:  Returns internal bio_module_context_t
 *
 * @param adapter Adapter instance
 * @return Bio-async module context, or NULL if not enabled
 */
bio_module_context_t hypothalamus_get_bio_context(
    hypothalamus_adapter_t* adapter);

/**
 * @brief Process pending bio-async messages
 *
 * WHAT: Process messages in hypothalamus inbox
 * WHY:  Handle incoming requests from other modules
 * HOW:  Calls bio_router_process_inbox and invokes handlers
 *
 * @param adapter Adapter instance
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed
 */
uint32_t hypothalamus_process_bio_messages(hypothalamus_adapter_t* adapter,
                                            uint32_t max_messages);

/**
 * @brief Broadcast circadian phase change
 *
 * WHAT: Notify all modules of circadian phase transition
 * WHY:  Allow sleep system, neuromodulators to sync
 * HOW:  Broadcasts BIO_MSG_CIRCADIAN_PHASE_CHANGE
 *
 * @param adapter Adapter instance
 * @param new_phase New circadian phase
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t hypothalamus_broadcast_circadian_phase(
    hypothalamus_adapter_t* adapter,
    hypo_circadian_phase_t new_phase);

/**
 * @brief Broadcast stress response
 *
 * WHAT: Notify all modules of HPA axis activation
 * WHY:  Allow emotional system, attention to respond
 * HOW:  Broadcasts BIO_MSG_STRESS_RESPONSE
 *
 * @param adapter Adapter instance
 * @param cortisol_level Current cortisol level
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t hypothalamus_broadcast_stress_response(
    hypothalamus_adapter_t* adapter,
    float cortisol_level);

/**
 * @brief Broadcast homeostatic alert
 *
 * WHAT: Notify all modules of homeostatic imbalance
 * WHY:  Allow behavior system to prioritize corrective actions
 * HOW:  Broadcasts BIO_MSG_HOMEOSTATIC_ALERT
 *
 * @param adapter Adapter instance
 * @param alert_type Type of homeostatic alert
 * @param urgency Alert urgency [0, 1]
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t hypothalamus_broadcast_homeostatic_alert(
    hypothalamus_adapter_t* adapter,
    uint32_t alert_type,
    float urgency);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HYPOTHALAMUS_ADAPTER_H */
