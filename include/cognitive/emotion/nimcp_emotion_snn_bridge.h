/**
 * @file nimcp_emotion_snn_bridge.h
 * @brief Emotion System - SNN Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between emotion recognition system and SNN module
 * WHY:  Enable spike-based emotion processing and bio-plausible emotional learning
 * HOW:  Convert emotional observations to spikes, decode SNN output to emotion states
 *
 * THEORETICAL FOUNDATIONS:
 * - Barrett (2017): Theory of Constructed Emotion
 * - LeDoux (2012): Emotion circuits in the brain
 * - Damasio (1994): Somatic marker hypothesis
 *
 * BIOLOGICAL BASIS:
 * - Amygdala neurons fire with precise timing for emotional stimuli
 * - Population coding encodes emotion categories and intensity
 * - Spike timing conveys emotional salience and urgency
 * - STDP enables emotional conditioning and learning
 *
 * INTEGRATION FLOWS:
 *
 * Emotion System --> SNN:
 *   1. Emotion observations encoded as input spike trains
 *   2. Valence/arousal converted to population activity patterns
 *   3. Emotion intensity modulates firing rate gain
 *   4. STDP eligibility traces updated for emotional learning
 *
 * SNN --> Emotion System:
 *   1. SNN output decoded to emotion category confidences
 *   2. Population firing rates inform emotion intensity
 *   3. Spike synchrony indicates coherent emotional state
 *   4. Temporal dynamics track emotion transitions
 *
 * BIO-ASYNC MESSAGES:
 * - EMOTION_SNN_MSG_SPIKE_EVENT: Spike events from emotion processing
 * - EMOTION_SNN_MSG_CATEGORY_UPDATE: Emotion category recognition updates
 * - EMOTION_SNN_MSG_VALENCE_AROUSAL: Valence/arousal dimension updates
 * - EMOTION_SNN_MSG_INTENSITY_CHANGE: Emotion intensity changes
 *
 * @see nimcp_snn.h
 * @see nimcp_emotion_recognition.h
 * @see nimcp_emotion_plasticity_bridge.h
 */

#ifndef NIMCP_EMOTION_SNN_BRIDGE_H
#define NIMCP_EMOTION_SNN_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "snn/nimcp_snn.h"
#include "cognitive/nimcp_emotion_recognition.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum emotion categories in SNN encoding */
#define EMOTION_SNN_MAX_CATEGORIES          EMOTION_COUNT

/** @brief Default neurons per emotion population */
#define EMOTION_SNN_NEURONS_PER_CATEGORY    32

/** @brief Default input encoding dimension (multimodal features) */
#define EMOTION_SNN_INPUT_DIM               64

/** @brief Default valence-arousal encoding dimension */
#define EMOTION_SNN_VA_DIM                  16

/** @brief Bio-async module ID for emotion-SNN bridge */
#define BIO_MODULE_EMOTION_SNN_BRIDGE       0x0B00

/** @brief Default simulation timestep (ms) */
#define EMOTION_SNN_DEFAULT_DT              1.0f

/** @brief Default spike encoding window (ms) */
#define EMOTION_SNN_ENCODING_WINDOW         50.0f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Spike encoding method for emotion observations
 */
typedef enum {
    EMOTION_SNN_ENCODE_RATE = 0,         /**< Rate coding (intensity = rate) */
    EMOTION_SNN_ENCODE_TEMPORAL,         /**< Temporal/latency coding */
    EMOTION_SNN_ENCODE_POPULATION,       /**< Population vector coding */
    EMOTION_SNN_ENCODE_PHASE             /**< Phase coding (emotion oscillations) */
} emotion_snn_encoding_t;

/**
 * @brief Decoding method for SNN output to emotion state
 */
typedef enum {
    EMOTION_SNN_DECODE_WINNER = 0,       /**< Winner-take-all category */
    EMOTION_SNN_DECODE_SOFTMAX,          /**< Softmax probability distribution */
    EMOTION_SNN_DECODE_POPULATION,       /**< Population vector decoding */
    EMOTION_SNN_DECODE_TEMPORAL          /**< Temporal pattern matching */
} emotion_snn_decoding_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    EMOTION_SNN_STATE_IDLE = 0,          /**< Ready for input */
    EMOTION_SNN_STATE_ENCODING,          /**< Encoding emotion input */
    EMOTION_SNN_STATE_SIMULATING,        /**< Running SNN simulation */
    EMOTION_SNN_STATE_DECODING,          /**< Decoding SNN output */
    EMOTION_SNN_STATE_DISABLED           /**< Bridge disabled */
} emotion_snn_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Emotion-SNN bridge configuration
 */
typedef struct {
    /* Network dimensions */
    uint32_t input_dim;                  /**< Input feature dimension */
    uint32_t hidden_dim;                 /**< Hidden layer size */
    uint32_t output_dim;                 /**< Output (emotion categories) */
    uint32_t va_dim;                     /**< Valence-arousal dimension */

    /* Encoding parameters */
    emotion_snn_encoding_t encoding;     /**< Spike encoding method */
    float encoding_gain;                 /**< Input-to-spike gain */
    float intensity_gain;                /**< Intensity modulation gain */
    float baseline_rate_hz;              /**< Baseline firing rate */
    float max_rate_hz;                   /**< Maximum firing rate */

    /* Decoding parameters */
    emotion_snn_decoding_t decoding;     /**< Output decoding method */
    float decoding_threshold;            /**< Confidence threshold */
    float confidence_gain;               /**< Confidence scaling */
    float temporal_smoothing;            /**< Temporal averaging alpha */

    /* Valence-Arousal encoding */
    bool enable_va_encoding;             /**< Enable valence-arousal populations */
    float va_encoding_gain;              /**< VA population gain */

    /* Simulation parameters */
    float dt_ms;                         /**< Simulation timestep */
    float simulation_window_ms;          /**< Simulation window per update */

    /* Integration */
    bool enable_bio_async;               /**< Enable bio-async messaging */
    bool enable_plasticity_integration;  /**< Enable plasticity bridge */
    bool enable_immune_modulation;       /**< Enable immune system modulation */
} emotion_snn_config_t;

//=============================================================================
// State Structures
//=============================================================================

/**
 * @brief Current emotion encoding state
 */
typedef struct {
    emotion_category_t current_category; /**< Most likely emotion */
    float category_confidences[EMOTION_COUNT]; /**< Per-category confidence */
    float valence;                       /**< Current valence [-1, 1] */
    float arousal;                       /**< Current arousal [0, 1] */
    float intensity;                     /**< Current intensity [0, 1] */
    uint64_t last_update_us;             /**< Last update timestamp */
} emotion_snn_emotion_state_t;

/**
 * @brief Bridge runtime state
 */
typedef struct {
    emotion_snn_state_t state;           /**< Current operational state */
    emotion_snn_emotion_state_t emotion; /**< Current emotion state */
    uint32_t active_populations;         /**< Number of active populations */
    float avg_firing_rate;               /**< Average network firing rate */
    bool bio_async_connected;            /**< Bio-async connection status */
} emotion_snn_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_observations;         /**< Total emotion observations */
    uint64_t total_spikes_generated;     /**< Total spikes generated */
    uint64_t total_decodings;            /**< Total decoding operations */
    uint64_t category_detections[EMOTION_COUNT]; /**< Per-category counts */
    float avg_confidence;                /**< Average detection confidence */
    float avg_processing_time_ms;        /**< Average processing time */
    uint64_t emotion_transitions;        /**< Number of emotion changes */
} emotion_snn_stats_t;

//=============================================================================
// Main Bridge Structure
//=============================================================================

/** @brief Forward declaration */
typedef struct emotion_snn_bridge emotion_snn_bridge_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Get default configuration
 * @return Default configuration values
 */
emotion_snn_config_t emotion_snn_config_default(void);

/**
 * @brief Create emotion-SNN bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge instance or NULL on failure
 */
emotion_snn_bridge_t* emotion_snn_create(const emotion_snn_config_t* config);

/**
 * @brief Destroy emotion-SNN bridge
 * @param bridge Bridge to destroy
 */
void emotion_snn_destroy(emotion_snn_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge instance
 * @return 0 on success, -1 on failure
 */
int emotion_snn_reset(emotion_snn_bridge_t* bridge);

//=============================================================================
// Encoding Functions (Emotion --> SNN)
//=============================================================================

/**
 * @brief Encode emotion observation as spike train
 * @param bridge Bridge instance
 * @param result Emotion recognition result
 * @return Number of spikes generated, or -1 on failure
 */
int emotion_snn_encode_observation(
    emotion_snn_bridge_t* bridge,
    const emotion_recognition_result_t* result
);

/**
 * @brief Encode multimodal emotion features
 * @param bridge Bridge instance
 * @param features Feature vector
 * @param n_features Number of features
 * @param valence Valence value [-1, 1]
 * @param arousal Arousal value [0, 1]
 * @return Number of spikes generated, or -1 on failure
 */
int emotion_snn_encode_features(
    emotion_snn_bridge_t* bridge,
    const float* features,
    uint32_t n_features,
    float valence,
    float arousal
);

/**
 * @brief Encode valence-arousal coordinates
 * @param bridge Bridge instance
 * @param valence Valence [-1, 1]
 * @param arousal Arousal [0, 1]
 * @param intensity Intensity [0, 1]
 * @return Number of spikes generated, or -1 on failure
 */
int emotion_snn_encode_valence_arousal(
    emotion_snn_bridge_t* bridge,
    float valence,
    float arousal,
    float intensity
);

//=============================================================================
// Simulation Functions
//=============================================================================

/**
 * @brief Run SNN simulation step
 * @param bridge Bridge instance
 * @param duration_ms Simulation duration in milliseconds
 * @return 0 on success, -1 on failure
 */
int emotion_snn_simulate(emotion_snn_bridge_t* bridge, float duration_ms);

/**
 * @brief Process single timestep
 * @param bridge Bridge instance
 * @return 0 on success, -1 on failure
 */
int emotion_snn_step(emotion_snn_bridge_t* bridge);

//=============================================================================
// Decoding Functions (SNN --> Emotion)
//=============================================================================

/**
 * @brief Get emotion category confidences from SNN output
 * @param bridge Bridge instance
 * @param confidences Output array for confidences [EMOTION_COUNT]
 * @return Most likely emotion category, or EMOTION_UNKNOWN on failure
 */
emotion_category_t emotion_snn_get_category_confidences(
    emotion_snn_bridge_t* bridge,
    float* confidences
);

/**
 * @brief Get decoded valence-arousal state
 * @param bridge Bridge instance
 * @param valence Output valence [-1, 1]
 * @param arousal Output arousal [0, 1]
 * @return 0 on success, -1 on failure
 */
int emotion_snn_get_valence_arousal(
    emotion_snn_bridge_t* bridge,
    float* valence,
    float* arousal
);

/**
 * @brief Get complete decoded emotion state
 * @param bridge Bridge instance
 * @param emotion_state Output emotion state
 * @return 0 on success, -1 on failure
 */
int emotion_snn_get_emotion_state(
    emotion_snn_bridge_t* bridge,
    emotion_snn_emotion_state_t* emotion_state
);

/**
 * @brief Decode emotion transition probability
 * @param bridge Bridge instance
 * @param from_category Current emotion
 * @param to_category Target emotion
 * @return Transition probability [0, 1]
 */
float emotion_snn_get_transition_prob(
    emotion_snn_bridge_t* bridge,
    emotion_category_t from_category,
    emotion_category_t to_category
);

//=============================================================================
// State and Statistics
//=============================================================================

/**
 * @brief Get bridge state
 * @param bridge Bridge instance
 * @param state Output state
 * @return 0 on success, -1 on failure
 */
int emotion_snn_get_state(
    const emotion_snn_bridge_t* bridge,
    emotion_snn_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge instance
 * @param stats Output statistics
 * @return 0 on success, -1 on failure
 */
int emotion_snn_get_stats(
    const emotion_snn_bridge_t* bridge,
    emotion_snn_stats_t* stats
);

/**
 * @brief Reset statistics
 * @param bridge Bridge instance
 */
void emotion_snn_reset_stats(emotion_snn_bridge_t* bridge);

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * @brief Connect to bio-async router
 * @param bridge Bridge instance
 * @return 0 on success, -1 on failure
 */
int emotion_snn_connect_bio_async(emotion_snn_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge instance
 * @return 0 on success, -1 on failure
 */
int emotion_snn_disconnect_bio_async(emotion_snn_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge instance
 * @return true if connected
 */
bool emotion_snn_is_bio_async_connected(const emotion_snn_bridge_t* bridge);

//=============================================================================
// Modulation Functions
//=============================================================================

/**
 * @brief Modulate encoding gain based on arousal
 * @param bridge Bridge instance
 * @param arousal_level Current arousal [0, 1]
 * @return 0 on success, -1 on failure
 */
int emotion_snn_modulate_by_arousal(
    emotion_snn_bridge_t* bridge,
    float arousal_level
);

/**
 * @brief Set intensity-based rate modulation
 * @param bridge Bridge instance
 * @param intensity Emotion intensity [0, 1]
 * @return 0 on success, -1 on failure
 */
int emotion_snn_set_intensity_modulation(
    emotion_snn_bridge_t* bridge,
    float intensity
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EMOTION_SNN_BRIDGE_H */
