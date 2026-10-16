/**
 * @file nimcp_novelty_detection.h
 * @brief Novelty/Surprise Detection for Locus Coeruleus
 * @version 1.0.0
 * @date 2026-01-11
 *
 * WHAT: Novelty and surprise detection triggering LC phasic responses
 * WHY:  LC responds to novel/surprising stimuli with phasic bursts
 * HOW:  Compare input to predictions, compute surprise, trigger bursts
 *
 * KEY CONCEPTS:
 * - Novelty: Deviation from learned expectations
 * - Surprise: Unexpected events (prediction error)
 * - Habituation: Decreasing response to repeated stimuli
 * - Dishabituation: Restoration of response by novel stimulus
 *
 * BIOLOGICAL BASIS:
 * - LC neurons fire phasically to unexpected stimuli
 * - Response habituates with repetition
 * - Novel stimuli trigger attention reorienting
 * - Surprise magnitude correlates with phasic burst intensity
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_NOVELTY_DETECTION_H
#define NIMCP_NOVELTY_DETECTION_H

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

/** Maximum input dimension */
#define NOVELTY_MAX_INPUT_DIM           256

/** Memory buffer size for habituation */
#define NOVELTY_MEMORY_SIZE             64

/** Default novelty threshold */
#define NOVELTY_DEFAULT_THRESHOLD       0.3f

/** Habituation time constant (ms) */
#define NOVELTY_HABITUATION_TAU_MS      5000.0f

/** Surprise decay time constant (ms) */
#define NOVELTY_SURPRISE_TAU_MS         100.0f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Novelty detector type
 */
typedef enum {
    NOVELTY_DETECTOR_STATISTICAL = 0,   /**< Statistical deviation */
    NOVELTY_DETECTOR_PREDICTIVE,        /**< Prediction error */
    NOVELTY_DETECTOR_FAMILIARITY,       /**< Familiarity-based */
    NOVELTY_DETECTOR_HYBRID,            /**< Combined approach */
    NOVELTY_DETECTOR_COUNT
} nimcp_novelty_detector_t;

/**
 * @brief Novelty event type
 */
typedef enum {
    NOVELTY_EVENT_NONE = 0,             /**< No novelty detected */
    NOVELTY_EVENT_MILD,                 /**< Mild novelty */
    NOVELTY_EVENT_MODERATE,             /**< Moderate novelty */
    NOVELTY_EVENT_HIGH,                 /**< High novelty */
    NOVELTY_EVENT_EXTREME               /**< Extreme/startling */
} nimcp_novelty_event_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Running statistics for input features
 */
typedef struct {
    float mean;                         /**< Running mean */
    float variance;                     /**< Running variance */
    float min;                          /**< Minimum observed */
    float max;                          /**< Maximum observed */
    uint64_t sample_count;              /**< Number of samples */
    float learning_rate;                /**< Update rate */
} nimcp_novelty_stats_t;

/**
 * @brief Habituation state for stimuli
 */
typedef struct {
    float stimulus_hash;                /**< Stimulus signature */
    float familiarity;                  /**< Familiarity level (0-1) */
    float exposure_count;               /**< Weighted exposure count */
    float last_seen_time;               /**< Last exposure time */
    float habituation_level;            /**< Current habituation (0-1) */
} nimcp_habituation_entry_t;

/**
 * @brief Prediction model for surprise computation
 */
typedef struct {
    float* predicted;                   /**< Predicted input */
    float* weights;                     /**< Prediction weights */
    uint32_t input_dim;                 /**< Input dimension */
    float prediction_error;             /**< Current prediction error */
    float learning_rate;                /**< Model learning rate */
} nimcp_novelty_predictor_t;

/**
 * @brief Novelty detection result
 */
typedef struct {
    float novelty_score;                /**< Overall novelty (0-1) */
    float surprise_magnitude;           /**< Surprise component (0-1) */
    float familiarity;                  /**< Familiarity component (0-1) */
    float deviation;                    /**< Statistical deviation (z-score) */
    float prediction_error;             /**< Prediction error component */
    nimcp_novelty_event_t event_type;   /**< Discrete classification */
    bool should_trigger_burst;          /**< Should trigger phasic burst */
    float recommended_burst_intensity;  /**< Burst intensity if triggered */
} nimcp_novelty_result_t;

/**
 * @brief Complete novelty detection system
 */
typedef struct {
    /* Detector configuration */
    nimcp_novelty_detector_t detector_type;
    uint32_t input_dimension;           /**< Expected input size */

    /* Running statistics per feature */
    nimcp_novelty_stats_t* feature_stats;

    /* Habituation memory */
    nimcp_habituation_entry_t habituation_memory[NOVELTY_MEMORY_SIZE];
    uint32_t memory_index;
    uint32_t memory_count;

    /* Prediction model */
    nimcp_novelty_predictor_t predictor;

    /* Current state */
    float current_novelty;              /**< Current novelty signal */
    float current_surprise;             /**< Current surprise level */
    float exploration_drive;            /**< Derived exploration drive */
    nimcp_novelty_result_t last_result; /**< Most recent result */

    /* Thresholds */
    float novelty_threshold;            /**< Threshold for novelty event */
    float surprise_threshold;           /**< Threshold for surprise */
    float burst_threshold;              /**< Threshold for phasic burst */

    /* Temporal dynamics */
    float habituation_tau;              /**< Habituation time constant */
    float surprise_decay_tau;           /**< Surprise decay constant */
    float novelty_decay_tau;            /**< Novelty signal decay */

    /* State */
    bool initialized;
    float current_time;
} nimcp_novelty_system_t;

/**
 * @brief Novelty detection configuration
 */
typedef struct {
    /* Detector */
    nimcp_novelty_detector_t detector_type;
    uint32_t input_dimension;

    /* Thresholds */
    float novelty_threshold;
    float surprise_threshold;
    float burst_threshold;

    /* Time constants */
    float habituation_tau_ms;
    float surprise_decay_tau_ms;
    float novelty_decay_tau_ms;

    /* Learning rates */
    float stats_learning_rate;
    float predictor_learning_rate;
    float habituation_rate;

    /* Sensitivity */
    float sensitivity;                  /**< Overall sensitivity (0-1) */
    float z_score_threshold;            /**< Z-score for statistical outlier */
} nimcp_novelty_config_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default novelty detection configuration
 */
NIMCP_EXPORT nimcp_novelty_config_t nimcp_novelty_default_config(void);

/**
 * @brief Initialize novelty detection system
 */
NIMCP_EXPORT int nimcp_novelty_init(
    nimcp_novelty_system_t* system,
    const nimcp_novelty_config_t* config
);

/**
 * @brief Shutdown novelty detection system
 */
NIMCP_EXPORT int nimcp_novelty_shutdown(nimcp_novelty_system_t* system);

/**
 * @brief Reset novelty system
 */
NIMCP_EXPORT int nimcp_novelty_reset(nimcp_novelty_system_t* system);

//=============================================================================
// Detection API
//=============================================================================

/**
 * @brief Process input and detect novelty
 * @param system Novelty system
 * @param input Input array
 * @param input_size Size of input
 * @param[out] result Detection result
 * @return 0 on success
 */
NIMCP_EXPORT int nimcp_novelty_detect(
    nimcp_novelty_system_t* system,
    const float* input,
    uint32_t input_size,
    nimcp_novelty_result_t* result
);

/**
 * @brief Update novelty system (temporal dynamics)
 * @param system Novelty system
 * @param dt Time delta (ms)
 */
NIMCP_EXPORT int nimcp_novelty_update(
    nimcp_novelty_system_t* system,
    float dt
);

/**
 * @brief Compute statistical novelty
 * @param system Novelty system
 * @param input Input array
 * @param input_size Input size
 * @return Statistical novelty score (0-1)
 */
NIMCP_EXPORT float nimcp_novelty_compute_statistical(
    nimcp_novelty_system_t* system,
    const float* input,
    uint32_t input_size
);

/**
 * @brief Compute prediction-based surprise
 * @param system Novelty system
 * @param input Input array
 * @param input_size Input size
 * @return Surprise magnitude (0-1)
 */
NIMCP_EXPORT float nimcp_novelty_compute_surprise(
    nimcp_novelty_system_t* system,
    const float* input,
    uint32_t input_size
);

/**
 * @brief Get familiarity for input
 * @param system Novelty system
 * @param input Input array
 * @param input_size Input size
 * @return Familiarity (0-1, 1=very familiar)
 */
NIMCP_EXPORT float nimcp_novelty_get_familiarity(
    nimcp_novelty_system_t* system,
    const float* input,
    uint32_t input_size
);

//=============================================================================
// Habituation API
//=============================================================================

/**
 * @brief Update habituation for stimulus
 * @param system Novelty system
 * @param input Input array
 * @param input_size Input size
 */
NIMCP_EXPORT int nimcp_novelty_habituate(
    nimcp_novelty_system_t* system,
    const float* input,
    uint32_t input_size
);

/**
 * @brief Trigger dishabituation
 * @param system Novelty system
 * @param strength Dishabituation strength (0-1)
 */
NIMCP_EXPORT int nimcp_novelty_dishabituate(
    nimcp_novelty_system_t* system,
    float strength
);

/**
 * @brief Clear habituation memory
 * @param system Novelty system
 */
NIMCP_EXPORT int nimcp_novelty_clear_habituation(
    nimcp_novelty_system_t* system
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get current novelty signal
 * @param system Novelty system
 * @return Current novelty (0-1)
 */
NIMCP_EXPORT float nimcp_novelty_get_current(
    const nimcp_novelty_system_t* system
);

/**
 * @brief Get current surprise level
 * @param system Novelty system
 * @return Current surprise (0-1)
 */
NIMCP_EXPORT float nimcp_novelty_get_surprise(
    const nimcp_novelty_system_t* system
);

/**
 * @brief Get exploration drive
 * @param system Novelty system
 * @return Exploration drive (0-1)
 */
NIMCP_EXPORT float nimcp_novelty_get_exploration_drive(
    const nimcp_novelty_system_t* system
);

/**
 * @brief Get last detection result
 * @param system Novelty system
 * @param[out] result Last result
 */
NIMCP_EXPORT int nimcp_novelty_get_last_result(
    const nimcp_novelty_system_t* system,
    nimcp_novelty_result_t* result
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NOVELTY_DETECTION_H */
