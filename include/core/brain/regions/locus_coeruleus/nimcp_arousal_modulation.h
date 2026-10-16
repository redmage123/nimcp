/**
 * @file nimcp_arousal_modulation.h
 * @brief Arousal Modulation System for Locus Coeruleus
 * @version 1.0.0
 * @date 2026-01-11
 *
 * WHAT: Arousal state modulation via NE release patterns
 * WHY:  LC controls global arousal, alertness, and vigilance states
 * HOW:  Map NE levels to arousal dimensions, compute gain modulation
 *
 * KEY CONCEPTS:
 * - Arousal: General activation/activation state
 * - Alertness: Readiness to respond to stimuli
 * - Vigilance: Sustained attention capability
 * - Gain Modulation: NE-dependent signal amplification
 *
 * BIOLOGICAL BASIS:
 * - Low NE: Drowsy, inattentive
 * - Moderate NE: Alert, focused
 * - High NE: Hypervigilant, anxious
 * - Inverted-U relationship between NE and performance
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_AROUSAL_MODULATION_H
#define NIMCP_AROUSAL_MODULATION_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

//=============================================================================
// Constants
//=============================================================================

/** Optimal NE level for performance (nM) */
#define AROUSAL_OPTIMAL_NE              30.0f

/** Arousal decay time constant (ms) */
#define AROUSAL_DECAY_TAU_MS            1000.0f

/** Vigilance decay time constant (ms) */
#define AROUSAL_VIGILANCE_TAU_MS        5000.0f

/** Maximum gain modulation factor */
#define AROUSAL_MAX_GAIN                2.5f

/** Minimum gain modulation factor */
#define AROUSAL_MIN_GAIN                0.2f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Arousal states (discrete classification)
 */
typedef enum {
    AROUSAL_STATE_SLEEP = 0,            /**< Sleep/drowsy (very low NE) */
    AROUSAL_STATE_DROWSY,               /**< Drowsy (low NE) */
    AROUSAL_STATE_RELAXED,              /**< Relaxed wakefulness */
    AROUSAL_STATE_ALERT,                /**< Alert, optimal (moderate NE) */
    AROUSAL_STATE_VIGILANT,             /**< Heightened vigilance */
    AROUSAL_STATE_HYPERAROUSED,         /**< Over-aroused (high NE) */
    AROUSAL_STATE_STRESSED,             /**< Stress response (very high NE) */
    AROUSAL_STATE_COUNT
} nimcp_arousal_state_t;

/**
 * @brief Performance curve type
 */
typedef enum {
    AROUSAL_CURVE_LINEAR = 0,           /**< Linear NE-performance */
    AROUSAL_CURVE_INVERTED_U,           /**< Yerkes-Dodson (optimal middle) */
    AROUSAL_CURVE_SIGMOID,              /**< Sigmoidal relationship */
    AROUSAL_CURVE_COUNT
} nimcp_arousal_curve_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Arousal dimension state
 */
typedef struct {
    float arousal;                      /**< General arousal (0-1) */
    float alertness;                    /**< Alertness level (0-1) */
    float vigilance;                    /**< Sustained attention (0-1) */
    float activation;                   /**< Behavioral activation (0-1) */
} nimcp_arousal_dimensions_t;

/**
 * @brief Gain modulation output
 */
typedef struct {
    float signal_gain;                  /**< Signal amplification */
    float noise_suppression;            /**< Noise reduction factor */
    float signal_to_noise;              /**< Effective SNR */
    float responsiveness;               /**< Response probability */
} nimcp_gain_modulation_t;

/**
 * @brief Performance metrics
 */
typedef struct {
    float cognitive_efficiency;         /**< Cognitive performance (0-1) */
    float reaction_time_modifier;       /**< Effect on reaction time */
    float accuracy_modifier;            /**< Effect on accuracy */
    float fatigue_level;                /**< Accumulated fatigue */
} nimcp_arousal_performance_t;

/**
 * @brief Arousal modulation system
 */
typedef struct {
    /* Current state */
    nimcp_arousal_state_t state;        /**< Discrete state classification */
    nimcp_arousal_dimensions_t dimensions; /**< Continuous dimensions */

    /* NE-dependent modulation */
    nimcp_gain_modulation_t gain;       /**< Gain modulation output */
    nimcp_arousal_performance_t performance; /**< Performance modifiers */

    /* Internal variables */
    float ne_input;                     /**< Current NE level input */
    float target_arousal;               /**< Target arousal level */
    float arousal_velocity;             /**< Rate of arousal change */

    /* History for adaptation */
    float arousal_history[32];          /**< Recent arousal levels */
    uint32_t history_index;
    float mean_arousal;                 /**< Running mean */

    /* Circadian/homeostatic */
    float circadian_drive;              /**< Circadian arousal drive */
    float homeostatic_pressure;         /**< Sleep pressure */

    /* Configuration */
    nimcp_arousal_curve_t curve_type;   /**< Performance curve type */
    float optimal_ne;                   /**< Optimal NE for performance */
    float arousal_tau;                  /**< Arousal time constant */
    float vigilance_tau;                /**< Vigilance decay constant */

    /* State */
    bool initialized;
    float current_time;
} nimcp_arousal_system_t;

/**
 * @brief Arousal modulation configuration
 */
typedef struct {
    /* NE-arousal mapping */
    float optimal_ne;                   /**< Optimal NE level */
    nimcp_arousal_curve_t curve_type;   /**< Curve shape */
    float curve_steepness;              /**< Steepness parameter */

    /* Time constants */
    float arousal_tau_ms;               /**< Arousal time constant */
    float vigilance_tau_ms;             /**< Vigilance decay constant */
    float alertness_tau_ms;             /**< Alertness time constant */

    /* Gain parameters */
    float max_gain;                     /**< Maximum gain factor */
    float min_gain;                     /**< Minimum gain factor */
    float baseline_gain;                /**< Baseline gain */

    /* Circadian */
    bool enable_circadian;              /**< Enable circadian modulation */
    float circadian_amplitude;          /**< Circadian amplitude */

    /* Thresholds for state transitions */
    float drowsy_threshold;             /**< Arousal below = drowsy */
    float alert_threshold;              /**< Arousal above = alert */
    float hyperaroused_threshold;       /**< Arousal above = hyperaroused */
} nimcp_arousal_config_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default arousal configuration
 */
NIMCP_EXPORT nimcp_arousal_config_t nimcp_arousal_default_config(void);

/**
 * @brief Initialize arousal modulation system
 */
NIMCP_EXPORT int nimcp_arousal_init(
    nimcp_arousal_system_t* system,
    const nimcp_arousal_config_t* config
);

/**
 * @brief Shutdown arousal system
 */
NIMCP_EXPORT int nimcp_arousal_shutdown(nimcp_arousal_system_t* system);

/**
 * @brief Reset to baseline
 */
NIMCP_EXPORT int nimcp_arousal_reset(nimcp_arousal_system_t* system);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update arousal state from NE level
 * @param system Arousal system
 * @param ne_concentration Current NE level (nM)
 * @param dt Time delta (ms)
 */
NIMCP_EXPORT int nimcp_arousal_update(
    nimcp_arousal_system_t* system,
    float ne_concentration,
    float dt
);

/**
 * @brief Set target arousal level
 * @param system Arousal system
 * @param target Target arousal (0-1)
 */
NIMCP_EXPORT int nimcp_arousal_set_target(
    nimcp_arousal_system_t* system,
    float target
);

/**
 * @brief Apply circadian drive
 * @param system Arousal system
 * @param time_of_day Time of day (0-24 hours)
 */
NIMCP_EXPORT int nimcp_arousal_apply_circadian(
    nimcp_arousal_system_t* system,
    float time_of_day
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get current arousal state
 * @param system Arousal system
 * @return Current state
 */
NIMCP_EXPORT nimcp_arousal_state_t nimcp_arousal_get_state(
    const nimcp_arousal_system_t* system
);

/**
 * @brief Get arousal dimensions
 * @param system Arousal system
 * @param[out] dimensions Dimension values
 */
NIMCP_EXPORT int nimcp_arousal_get_dimensions(
    const nimcp_arousal_system_t* system,
    nimcp_arousal_dimensions_t* dimensions
);

/**
 * @brief Get gain modulation
 * @param system Arousal system
 * @param[out] gain Gain modulation values
 */
NIMCP_EXPORT int nimcp_arousal_get_gain(
    const nimcp_arousal_system_t* system,
    nimcp_gain_modulation_t* gain
);

/**
 * @brief Get performance modifiers
 * @param system Arousal system
 * @param[out] performance Performance values
 */
NIMCP_EXPORT int nimcp_arousal_get_performance(
    const nimcp_arousal_system_t* system,
    nimcp_arousal_performance_t* performance
);

/**
 * @brief Compute NE-to-performance mapping
 * @param ne_level NE concentration (nM)
 * @param optimal_ne Optimal NE level (nM)
 * @param curve_type Curve type
 * @return Performance value (0-1)
 */
NIMCP_EXPORT float nimcp_arousal_ne_to_performance(
    float ne_level,
    float optimal_ne,
    nimcp_arousal_curve_t curve_type
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_AROUSAL_MODULATION_H */
