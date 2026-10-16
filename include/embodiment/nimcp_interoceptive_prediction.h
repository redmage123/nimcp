/**
 * @file nimcp_interoceptive_prediction.h
 * @brief Visceral Prediction and Interoceptive Processing for NIMCP
 *
 * Biological Inspiration:
 * - Insular cortex: Primary center for interoceptive awareness
 * - Anterior cingulate cortex: Prediction error processing
 * - Hypothalamus: Homeostatic regulation and setpoint control
 * - Autonomic nervous system: Visceral state monitoring
 * - Allostasis: Predictive regulation of internal states
 *
 * This module enables:
 * - Internal state prediction and monitoring
 * - Prediction error computation for visceral signals
 * - Homeostatic integration and regulation
 * - Allostatic load tracking
 * - Interoceptive accuracy assessment
 *
 * Key Features:
 * - Multi-organ system monitoring
 * - Predictive homeostasis
 * - Prediction error-driven learning
 * - Emotional state integration
 * - Stress response modeling
 * - Statistics tracking
 *
 * @version 1.0
 * @date 2025-01-13
 */

#ifndef NIMCP_INTEROCEPTIVE_PREDICTION_H
#define NIMCP_INTEROCEPTIVE_PREDICTION_H

#include "utils/validation/nimcp_common.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/**
 * @brief Maximum organ systems tracked
 */
#define NIMCP_INTERO_MAX_SYSTEMS 16

/**
 * @brief Maximum signals per system
 */
#define NIMCP_INTERO_MAX_SIGNALS 32

/**
 * @brief Prediction history size
 */
#define NIMCP_INTERO_HISTORY_SIZE 128

/**
 * @brief Maximum homeostatic setpoints
 */
#define NIMCP_INTERO_MAX_SETPOINTS 32

/* ============================================================================
 * Error Codes
 * ============================================================================ */

/**
 * @brief Interoception-specific error codes (9300-9399 range)
 */
typedef enum {
    NIMCP_INTERO_OK = 0,                      /**< Operation successful */
    NIMCP_INTERO_ERROR = 9300,                /**< Generic error */
    NIMCP_INTERO_ERROR_NULL_PARAM = 9301,     /**< Null parameter */
    NIMCP_INTERO_ERROR_INVALID_CONFIG = 9302, /**< Invalid configuration */
    NIMCP_INTERO_ERROR_NOT_INITIALIZED = 9303,/**< Not initialized */
    NIMCP_INTERO_ERROR_SYSTEM_LIMIT = 9304,   /**< System limit reached */
    NIMCP_INTERO_ERROR_INVALID_SYSTEM = 9305, /**< Invalid system ID */
    NIMCP_INTERO_ERROR_SIGNAL_LIMIT = 9306,   /**< Signal limit reached */
    NIMCP_INTERO_ERROR_INVALID_SIGNAL = 9307, /**< Invalid signal ID */
    NIMCP_INTERO_ERROR_PREDICTION = 9308,     /**< Prediction failed */
    NIMCP_INTERO_ERROR_MEMORY = 9309          /**< Memory allocation failed */
} nimcp_intero_error_t;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Organ system types
 */
typedef enum {
    NIMCP_SYSTEM_UNKNOWN = 0,
    NIMCP_SYSTEM_CARDIOVASCULAR,     /**< Heart rate, blood pressure */
    NIMCP_SYSTEM_RESPIRATORY,        /**< Breathing rate, O2 levels */
    NIMCP_SYSTEM_DIGESTIVE,          /**< Hunger, satiety, gut */
    NIMCP_SYSTEM_THERMAL,            /**< Temperature regulation */
    NIMCP_SYSTEM_METABOLIC,          /**< Energy, glucose */
    NIMCP_SYSTEM_MUSCULAR,           /**< Muscle tension, fatigue */
    NIMCP_SYSTEM_IMMUNE,             /**< Inflammation markers */
    NIMCP_SYSTEM_ENDOCRINE,          /**< Hormonal levels */
    NIMCP_SYSTEM_URINARY,            /**< Bladder fullness */
    NIMCP_SYSTEM_VESTIBULAR,         /**< Balance, motion */
    NIMCP_SYSTEM_PAIN,               /**< Nociception */
    NIMCP_SYSTEM_COUNT
} nimcp_organ_system_t;

/**
 * @brief Signal types
 */
typedef enum {
    NIMCP_SIGNAL_UNKNOWN = 0,

    /* Cardiovascular */
    NIMCP_SIGNAL_HEART_RATE,         /**< Heart rate (bpm) */
    NIMCP_SIGNAL_BLOOD_PRESSURE_SYS, /**< Systolic BP (mmHg) */
    NIMCP_SIGNAL_BLOOD_PRESSURE_DIA, /**< Diastolic BP (mmHg) */
    NIMCP_SIGNAL_HRV,                /**< Heart rate variability */

    /* Respiratory */
    NIMCP_SIGNAL_BREATHING_RATE,     /**< Breaths per minute */
    NIMCP_SIGNAL_OXYGEN_SAT,         /**< Blood oxygen saturation */
    NIMCP_SIGNAL_CO2_LEVEL,          /**< CO2 level */
    NIMCP_SIGNAL_BREATHING_DEPTH,    /**< Tidal volume */

    /* Digestive */
    NIMCP_SIGNAL_HUNGER,             /**< Hunger level [0-1] */
    NIMCP_SIGNAL_SATIETY,            /**< Satiety level [0-1] */
    NIMCP_SIGNAL_THIRST,             /**< Thirst level [0-1] */
    NIMCP_SIGNAL_GUT_ACTIVITY,       /**< Gut motility */
    NIMCP_SIGNAL_NAUSEA,             /**< Nausea level [0-1] */

    /* Thermal */
    NIMCP_SIGNAL_CORE_TEMP,          /**< Core temperature (C) */
    NIMCP_SIGNAL_SKIN_TEMP,          /**< Skin temperature (C) */
    NIMCP_SIGNAL_SWEATING,           /**< Sweat rate */
    NIMCP_SIGNAL_SHIVERING,          /**< Shivering intensity */

    /* Metabolic */
    NIMCP_SIGNAL_ENERGY_LEVEL,       /**< Perceived energy [0-1] */
    NIMCP_SIGNAL_GLUCOSE,            /**< Blood glucose (mg/dL) */
    NIMCP_SIGNAL_ATP_LEVEL,          /**< Cellular ATP */

    /* Muscular */
    NIMCP_SIGNAL_MUSCLE_TENSION,     /**< Overall tension [0-1] */
    NIMCP_SIGNAL_FATIGUE,            /**< Fatigue level [0-1] */
    NIMCP_SIGNAL_SORENESS,           /**< Muscle soreness [0-1] */

    /* Immune */
    NIMCP_SIGNAL_INFLAMMATION,       /**< Inflammation level [0-1] */
    NIMCP_SIGNAL_SICKNESS,           /**< Sickness behavior [0-1] */

    /* Endocrine */
    NIMCP_SIGNAL_CORTISOL,           /**< Stress hormone */
    NIMCP_SIGNAL_ADRENALINE,         /**< Fight/flight hormone */
    NIMCP_SIGNAL_DOPAMINE,           /**< Reward/motivation */
    NIMCP_SIGNAL_SEROTONIN,          /**< Mood/wellbeing */

    /* Pain */
    NIMCP_SIGNAL_PAIN_INTENSITY,     /**< Pain level [0-1] */
    NIMCP_SIGNAL_PAIN_LOCATION,      /**< Pain location index */
    NIMCP_SIGNAL_DISCOMFORT,         /**< General discomfort [0-1] */

    NIMCP_SIGNAL_COUNT
} nimcp_intero_signal_type_t;

/**
 * @brief Homeostatic state
 */
typedef enum {
    NIMCP_HOMEO_OPTIMAL = 0,         /**< Within optimal range */
    NIMCP_HOMEO_MILD_DEVIATION,      /**< Slight deviation */
    NIMCP_HOMEO_MODERATE_DEVIATION,  /**< Moderate deviation */
    NIMCP_HOMEO_SEVERE_DEVIATION,    /**< Severe deviation */
    NIMCP_HOMEO_CRITICAL,            /**< Critical deviation */
    NIMCP_HOMEO_COUNT
} nimcp_homeostatic_state_t;

/**
 * @brief Arousal state
 */
typedef enum {
    NIMCP_AROUSAL_VERY_LOW = 0,      /**< Very calm/sleepy */
    NIMCP_AROUSAL_LOW,               /**< Relaxed */
    NIMCP_AROUSAL_MODERATE,          /**< Normal alertness */
    NIMCP_AROUSAL_HIGH,              /**< Heightened alertness */
    NIMCP_AROUSAL_VERY_HIGH,         /**< High arousal/stress */
    NIMCP_AROUSAL_COUNT
} nimcp_arousal_state_t;

/**
 * @brief Emotional valence
 */
typedef enum {
    NIMCP_VALENCE_VERY_NEGATIVE = 0,
    NIMCP_VALENCE_NEGATIVE,
    NIMCP_VALENCE_NEUTRAL,
    NIMCP_VALENCE_POSITIVE,
    NIMCP_VALENCE_VERY_POSITIVE,
    NIMCP_VALENCE_COUNT
} nimcp_valence_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Homeostatic setpoint
 */
typedef struct {
    nimcp_intero_signal_type_t signal; /**< Signal type */
    double target_value;              /**< Target/setpoint value */
    double tolerance_low;             /**< Lower tolerance bound */
    double tolerance_high;            /**< Upper tolerance bound */
    double critical_low;              /**< Critical low threshold */
    double critical_high;             /**< Critical high threshold */
    double current_value;             /**< Current value */
    double regulation_gain;           /**< Regulation strength */
} nimcp_setpoint_t;

/**
 * @brief Interoceptive signal
 */
typedef struct {
    uint32_t signal_id;               /**< Signal identifier */
    nimcp_intero_signal_type_t type;  /**< Signal type */
    nimcp_organ_system_t system;      /**< Associated system */

    double value;                     /**< Current sensed value */
    double predicted_value;           /**< Predicted value */
    double prediction_error;          /**< Prediction error */
    double precision;                 /**< Signal precision/reliability */

    double rate_of_change;            /**< Change rate */
    double value_history[8];          /**< Recent history */
    uint32_t history_index;           /**< History buffer index */

    uint64_t timestamp;               /**< Measurement timestamp */
    bool is_valid;                    /**< Signal is valid */
} nimcp_intero_signal_t;

/**
 * @brief Organ system state
 */
typedef struct {
    uint32_t system_id;               /**< System identifier */
    nimcp_organ_system_t type;        /**< System type */

    /* Signals */
    nimcp_intero_signal_t signals[NIMCP_INTERO_MAX_SIGNALS];
    uint32_t num_signals;

    /* Homeostatic state */
    nimcp_homeostatic_state_t homeo_state; /**< Current state */
    double deviation_magnitude;       /**< How far from optimal */
    double allostatic_load;           /**< Chronic stress accumulation */

    /* Activity */
    double activity_level;            /**< System activity [0-1] */
    bool is_active;                   /**< System is active */

    uint64_t last_update;             /**< Last update timestamp */
} nimcp_system_state_t;

/**
 * @brief Prediction result
 */
typedef struct {
    nimcp_intero_signal_type_t signal; /**< Predicted signal */
    double predicted_value;           /**< Predicted value */
    double confidence;                /**< Prediction confidence [0-1] */
    double prediction_horizon;        /**< How far ahead (seconds) */

    double actual_value;              /**< Actual value (if available) */
    double prediction_error;          /**< Error magnitude */
    bool actual_available;            /**< Actual value was received */
} nimcp_intero_prediction_t;

/**
 * @brief Interoceptive awareness assessment
 */
typedef struct {
    double accuracy;                  /**< Interoceptive accuracy [0-1] */
    double sensitivity;               /**< Signal detection sensitivity */
    double stability;                 /**< Awareness stability over time */

    /* Per-system accuracy */
    double system_accuracy[NIMCP_SYSTEM_COUNT];

    uint64_t assessment_time;         /**< When assessment was made */
} nimcp_intero_awareness_t;

/**
 * @brief Emotional state derived from interoception
 */
typedef struct {
    nimcp_arousal_state_t arousal;    /**< Current arousal level */
    nimcp_valence_t valence;          /**< Current valence */

    double arousal_value;             /**< Arousal [0-1] */
    double valence_value;             /**< Valence [-1, 1] */

    double stress_level;              /**< Composite stress [0-1] */
    double wellbeing;                 /**< Composite wellbeing [0-1] */
    double energy;                    /**< Perceived energy [0-1] */

    bool stress_response_active;      /**< Acute stress response */
    uint64_t update_time;             /**< Last update */
} nimcp_emotional_state_t;

/**
 * @brief Allostatic load assessment
 */
typedef struct {
    double total_load;                /**< Total allostatic load [0-1] */
    double acute_stress;              /**< Acute stress component */
    double chronic_stress;            /**< Chronic stress component */
    double recovery_capacity;         /**< Recovery capacity [0-1] */

    /* Per-system contribution */
    double system_load[NIMCP_SYSTEM_COUNT];

    uint64_t assessment_time;
} nimcp_allostatic_load_t;

/**
 * @brief Interoceptive statistics
 */
typedef struct {
    uint64_t total_signals;           /**< Total signals processed */
    uint64_t total_predictions;       /**< Total predictions made */
    uint64_t prediction_errors;       /**< Significant prediction errors */
    uint64_t homeostatic_violations;  /**< Times outside optimal range */

    double avg_prediction_error;      /**< Average prediction error */
    double avg_intero_accuracy;       /**< Average interoceptive accuracy */
    double avg_arousal;               /**< Average arousal level */
    double avg_stress;                /**< Average stress level */

    uint32_t active_systems;          /**< Currently active systems */

    uint64_t creation_time;           /**< System creation timestamp */
} nimcp_intero_stats_t;

/**
 * @brief Configuration parameters
 */
typedef struct {
    /* Prediction parameters */
    double prediction_learning_rate;  /**< Learning rate for predictions */
    double prediction_decay;          /**< Prediction decay rate */
    double error_threshold;           /**< Threshold for significant error */

    /* Homeostasis parameters */
    double regulation_strength;       /**< Homeostatic regulation strength */
    double allostatic_decay;          /**< Allostatic load decay rate */
    double critical_threshold;        /**< Threshold for critical state */

    /* Emotional mapping */
    bool enable_emotional_mapping;    /**< Map intero to emotions */
    double arousal_sensitivity;       /**< Arousal detection sensitivity */
    double valence_sensitivity;       /**< Valence detection sensitivity */

    /* Assessment */
    double assessment_window;         /**< Window for accuracy assessment (s) */

    /* Update rate */
    double update_rate_hz;            /**< Processing update rate */
} nimcp_intero_config_t;

/**
 * @brief Interoceptive context (opaque)
 */
typedef struct nimcp_intero_context nimcp_intero_context_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * @param config Configuration to fill
 */
void nimcp_intero_default_config(nimcp_intero_config_t* config);

/**
 * @brief Create interoceptive context
 *
 * @param config Configuration parameters
 * @return Context pointer or NULL on failure
 */
nimcp_intero_context_t* nimcp_intero_create(const nimcp_intero_config_t* config);

/**
 * @brief Initialize existing context
 *
 * @param ctx Context to initialize
 * @param config Configuration parameters
 * @return NIMCP_INTERO_OK on success
 */
nimcp_intero_error_t nimcp_intero_init(
    nimcp_intero_context_t* ctx,
    const nimcp_intero_config_t* config
);

/**
 * @brief Reset interoceptive context
 *
 * @param ctx Context to reset
 * @return NIMCP_INTERO_OK on success
 */
nimcp_intero_error_t nimcp_intero_reset(nimcp_intero_context_t* ctx);

/**
 * @brief Destroy interoceptive context
 *
 * @param ctx Context to destroy
 */
void nimcp_intero_destroy(nimcp_intero_context_t* ctx);

/* ============================================================================
 * System Management API
 * ============================================================================ */

/**
 * @brief Register organ system
 *
 * @param ctx Interoceptive context
 * @param system_type System type to register
 * @param system_id Output system ID
 * @return NIMCP_INTERO_OK on success
 */
nimcp_intero_error_t nimcp_intero_register_system(
    nimcp_intero_context_t* ctx,
    nimcp_organ_system_t system_type,
    uint32_t* system_id
);

/**
 * @brief Get system state
 *
 * @param ctx Interoceptive context
 * @param system_id System to query
 * @param state Output state
 * @return NIMCP_INTERO_OK on success
 */
nimcp_intero_error_t nimcp_intero_get_system(
    const nimcp_intero_context_t* ctx,
    uint32_t system_id,
    nimcp_system_state_t* state
);

/**
 * @brief Get all systems
 *
 * @param ctx Interoceptive context
 * @param systems Output array
 * @param max_systems Maximum to return
 * @param num_systems Output count
 * @return NIMCP_INTERO_OK on success
 */
nimcp_intero_error_t nimcp_intero_get_all_systems(
    const nimcp_intero_context_t* ctx,
    nimcp_system_state_t* systems,
    uint32_t max_systems,
    uint32_t* num_systems
);

/**
 * @brief Initialize standard body systems
 *
 * @param ctx Interoceptive context
 * @return NIMCP_INTERO_OK on success
 */
nimcp_intero_error_t nimcp_intero_init_standard_systems(
    nimcp_intero_context_t* ctx
);

/* ============================================================================
 * Signal Processing API
 * ============================================================================ */

/**
 * @brief Process interoceptive signal
 *
 * WHAT: Processes visceral signal and updates predictions
 * WHY:  Core signal processing for interoception
 *
 * @param ctx Interoceptive context
 * @param signal Signal to process
 * @return NIMCP_INTERO_OK on success
 */
nimcp_intero_error_t nimcp_intero_process_signal(
    nimcp_intero_context_t* ctx,
    const nimcp_intero_signal_t* signal
);

/**
 * @brief Batch process signals
 *
 * @param ctx Interoceptive context
 * @param signals Array of signals
 * @param num_signals Number of signals
 * @return NIMCP_INTERO_OK on success
 */
nimcp_intero_error_t nimcp_intero_process_signals(
    nimcp_intero_context_t* ctx,
    const nimcp_intero_signal_t* signals,
    uint32_t num_signals
);

/**
 * @brief Get current signal value
 *
 * @param ctx Interoceptive context
 * @param signal_type Signal to query
 * @param value Output value
 * @return NIMCP_INTERO_OK on success
 */
nimcp_intero_error_t nimcp_intero_get_signal(
    const nimcp_intero_context_t* ctx,
    nimcp_intero_signal_type_t signal_type,
    double* value
);

/**
 * @brief Get prediction error for signal
 *
 * @param ctx Interoceptive context
 * @param signal_type Signal to query
 * @param error Output error
 * @return NIMCP_INTERO_OK on success
 */
nimcp_intero_error_t nimcp_intero_get_prediction_error(
    const nimcp_intero_context_t* ctx,
    nimcp_intero_signal_type_t signal_type,
    double* error
);

/* ============================================================================
 * Prediction API
 * ============================================================================ */

/**
 * @brief Predict future signal value
 *
 * WHAT: Generates prediction for future visceral state
 * WHY:  Predictive interoception for allostasis
 *
 * @param ctx Interoceptive context
 * @param signal_type Signal to predict
 * @param horizon Prediction horizon (seconds)
 * @param prediction Output prediction
 * @return NIMCP_INTERO_OK on success
 */
nimcp_intero_error_t nimcp_intero_predict(
    nimcp_intero_context_t* ctx,
    nimcp_intero_signal_type_t signal_type,
    double horizon,
    nimcp_intero_prediction_t* prediction
);

/**
 * @brief Update prediction with actual value
 *
 * WHAT: Computes prediction error and updates model
 * WHY:  Learning from prediction errors
 *
 * @param ctx Interoceptive context
 * @param signal_type Signal type
 * @param actual_value Actual observed value
 * @return NIMCP_INTERO_OK on success
 */
nimcp_intero_error_t nimcp_intero_update_prediction(
    nimcp_intero_context_t* ctx,
    nimcp_intero_signal_type_t signal_type,
    double actual_value
);

/**
 * @brief Get weighted precision prediction error
 *
 * @param ctx Interoceptive context
 * @param total_error Output total weighted error
 * @return NIMCP_INTERO_OK on success
 */
nimcp_intero_error_t nimcp_intero_get_total_prediction_error(
    const nimcp_intero_context_t* ctx,
    double* total_error
);

/* ============================================================================
 * Homeostatic Integration API
 * ============================================================================ */

/**
 * @brief Set homeostatic setpoint
 *
 * @param ctx Interoceptive context
 * @param setpoint Setpoint to add/update
 * @return NIMCP_INTERO_OK on success
 */
nimcp_intero_error_t nimcp_intero_set_setpoint(
    nimcp_intero_context_t* ctx,
    const nimcp_setpoint_t* setpoint
);

/**
 * @brief Get homeostatic setpoint
 *
 * @param ctx Interoceptive context
 * @param signal_type Signal to query
 * @param setpoint Output setpoint
 * @return NIMCP_INTERO_OK on success
 */
nimcp_intero_error_t nimcp_intero_get_setpoint(
    const nimcp_intero_context_t* ctx,
    nimcp_intero_signal_type_t signal_type,
    nimcp_setpoint_t* setpoint
);

/**
 * @brief Get homeostatic state
 *
 * @param ctx Interoceptive context
 * @param signal_type Signal to query
 * @param state Output homeostatic state
 * @param deviation Output deviation magnitude
 * @return NIMCP_INTERO_OK on success
 */
nimcp_intero_error_t nimcp_intero_get_homeostatic_state(
    const nimcp_intero_context_t* ctx,
    nimcp_intero_signal_type_t signal_type,
    nimcp_homeostatic_state_t* state,
    double* deviation
);

/**
 * @brief Compute regulatory drive
 *
 * WHAT: Computes drive signal to restore homeostasis
 * WHY:  Generates motivation for regulatory behavior
 *
 * @param ctx Interoceptive context
 * @param signal_type Signal for which to compute drive
 * @param drive Output regulatory drive [-1, 1]
 * @return NIMCP_INTERO_OK on success
 */
nimcp_intero_error_t nimcp_intero_compute_drive(
    nimcp_intero_context_t* ctx,
    nimcp_intero_signal_type_t signal_type,
    double* drive
);

/* ============================================================================
 * Allostatic Load API
 * ============================================================================ */

/**
 * @brief Get allostatic load assessment
 *
 * @param ctx Interoceptive context
 * @param load Output allostatic load
 * @return NIMCP_INTERO_OK on success
 */
nimcp_intero_error_t nimcp_intero_get_allostatic_load(
    const nimcp_intero_context_t* ctx,
    nimcp_allostatic_load_t* load
);

/**
 * @brief Simulate stress event
 *
 * WHAT: Applies stress to the system
 * WHY:  Models acute stress response
 *
 * @param ctx Interoceptive context
 * @param intensity Stress intensity [0-1]
 * @param duration Duration (seconds)
 * @return NIMCP_INTERO_OK on success
 */
nimcp_intero_error_t nimcp_intero_apply_stress(
    nimcp_intero_context_t* ctx,
    double intensity,
    double duration
);

/**
 * @brief Simulate recovery
 *
 * @param ctx Interoceptive context
 * @param duration Recovery duration (seconds)
 * @return NIMCP_INTERO_OK on success
 */
nimcp_intero_error_t nimcp_intero_apply_recovery(
    nimcp_intero_context_t* ctx,
    double duration
);

/* ============================================================================
 * Emotional Mapping API
 * ============================================================================ */

/**
 * @brief Get emotional state derived from interoception
 *
 * @param ctx Interoceptive context
 * @param state Output emotional state
 * @return NIMCP_INTERO_OK on success
 */
nimcp_intero_error_t nimcp_intero_get_emotional_state(
    const nimcp_intero_context_t* ctx,
    nimcp_emotional_state_t* state
);

/**
 * @brief Map arousal from interoceptive signals
 *
 * @param ctx Interoceptive context
 * @param arousal Output arousal level
 * @param state Output arousal state
 * @return NIMCP_INTERO_OK on success
 */
nimcp_intero_error_t nimcp_intero_compute_arousal(
    const nimcp_intero_context_t* ctx,
    double* arousal,
    nimcp_arousal_state_t* state
);

/**
 * @brief Map valence from interoceptive signals
 *
 * @param ctx Interoceptive context
 * @param valence Output valence [-1, 1]
 * @param state Output valence state
 * @return NIMCP_INTERO_OK on success
 */
nimcp_intero_error_t nimcp_intero_compute_valence(
    const nimcp_intero_context_t* ctx,
    double* valence,
    nimcp_valence_t* state
);

/* ============================================================================
 * Assessment API
 * ============================================================================ */

/**
 * @brief Assess interoceptive awareness
 *
 * @param ctx Interoceptive context
 * @param awareness Output awareness assessment
 * @return NIMCP_INTERO_OK on success
 */
nimcp_intero_error_t nimcp_intero_assess_awareness(
    const nimcp_intero_context_t* ctx,
    nimcp_intero_awareness_t* awareness
);

/* ============================================================================
 * Update and Processing API
 * ============================================================================ */

/**
 * @brief Process one update cycle
 *
 * @param ctx Interoceptive context
 * @param delta_time Time since last update (seconds)
 * @return NIMCP_INTERO_OK on success
 */
nimcp_intero_error_t nimcp_intero_update(
    nimcp_intero_context_t* ctx,
    double delta_time
);

/* ============================================================================
 * Statistics and Utility API
 * ============================================================================ */

/**
 * @brief Get statistics
 *
 * @param ctx Interoceptive context
 * @param stats Output statistics
 * @return NIMCP_INTERO_OK on success
 */
nimcp_intero_error_t nimcp_intero_get_stats(
    const nimcp_intero_context_t* ctx,
    nimcp_intero_stats_t* stats
);

/**
 * @brief Get organ system name
 *
 * @param system System type
 * @return String name
 */
const char* nimcp_intero_system_name(nimcp_organ_system_t system);

/**
 * @brief Get signal type name
 *
 * @param signal Signal type
 * @return String name
 */
const char* nimcp_intero_signal_name(nimcp_intero_signal_type_t signal);

/**
 * @brief Get homeostatic state name
 *
 * @param state Homeostatic state
 * @return String name
 */
const char* nimcp_intero_homeo_state_name(nimcp_homeostatic_state_t state);

/**
 * @brief Get arousal state name
 *
 * @param state Arousal state
 * @return String name
 */
const char* nimcp_intero_arousal_name(nimcp_arousal_state_t state);

/**
 * @brief Get valence name
 *
 * @param valence Valence state
 * @return String name
 */
const char* nimcp_intero_valence_name(nimcp_valence_t valence);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_INTEROCEPTIVE_PREDICTION_H */
