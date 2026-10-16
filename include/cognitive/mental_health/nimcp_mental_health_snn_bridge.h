/**
 * @file nimcp_mental_health_snn_bridge.h
 * @brief Mental Health - SNN Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between mental health monitoring and spiking neural networks
 * WHY:  Enable biologically-plausible mental health state encoding through
 *       population coding and spike-timing dynamics
 * HOW:  Encode mood/anxiety/depression dimensions as spike patterns, decode
 *       emotional regulation signals from neural activity patterns
 *
 * THEORETICAL FOUNDATIONS:
 * - Beck (1967): Cognitive theory of depression
 * - Seligman (1975): Learned helplessness and depression
 * - LeDoux (2015): Anxiety circuits in the brain
 * - Davidson (2000): Affective neuroscience and emotion regulation
 *
 * BIOLOGICAL BASIS:
 * - Amygdala for anxiety and threat detection encoding
 * - Prefrontal cortex for emotional regulation
 * - Anterior cingulate for mood state monitoring
 * - Hippocampus for stress response and memory encoding
 * - Nucleus accumbens for reward/anhedonia states
 *
 * INTEGRATION WITH MENTAL HEALTH:
 * - Mood state encoding via population activity
 * - Anxiety detection through firing rate patterns
 * - Depression markers via reduced activity patterns
 * - Resilience factors through adaptation dynamics
 *
 * @see nimcp_mental_health.h
 * @see nimcp_snn_network.h
 */

#ifndef NIMCP_MENTAL_HEALTH_SNN_BRIDGE_H
#define NIMCP_MENTAL_HEALTH_SNN_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum mental health dimensions to encode */
#define MENTAL_HEALTH_SNN_MAX_DIMENSIONS     16

/** @brief Neurons per mental health dimension */
#define MENTAL_HEALTH_SNN_NEURONS_PER_DIM    32

/** @brief Default anxiety threshold */
#define MENTAL_HEALTH_SNN_ANXIETY_THRESH     0.6f

/** @brief Default depression threshold */
#define MENTAL_HEALTH_SNN_DEPRESSION_THRESH  0.5f

/** @brief Default encoding window (ms) */
#define MENTAL_HEALTH_SNN_ENCODING_WINDOW    50.0f

/** @brief Bio-async module ID */
#define BIO_MODULE_MENTAL_HEALTH_SNN         0x0D60

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Mental health dimension types for SNN encoding
 */
typedef enum {
    MENTAL_HEALTH_DIM_MOOD_STATE = 0,       /**< Overall mood level [-1 to 1] */
    MENTAL_HEALTH_DIM_ANXIETY_LEVEL,         /**< Anxiety/worry intensity */
    MENTAL_HEALTH_DIM_DEPRESSION_LEVEL,      /**< Depression/anhedonia intensity */
    MENTAL_HEALTH_DIM_STRESS_RESPONSE,       /**< Stress reactivity level */
    MENTAL_HEALTH_DIM_EMOTIONAL_REGULATION,  /**< Regulation capacity */
    MENTAL_HEALTH_DIM_RESILIENCE,            /**< Resilience factor */
    MENTAL_HEALTH_DIM_ENGAGEMENT,            /**< Behavioral engagement level */
    MENTAL_HEALTH_DIM_SOCIAL_FUNCTION,       /**< Social functioning capacity */
    MENTAL_HEALTH_DIM_COGNITIVE_CLARITY,     /**< Cognitive clarity vs fog */
    MENTAL_HEALTH_DIM_SLEEP_QUALITY,         /**< Sleep-related factors */
    MENTAL_HEALTH_DIM_COUNT
} mental_health_snn_dimension_t;

/**
 * @brief Encoding methods for mental health contexts
 */
typedef enum {
    MENTAL_HEALTH_SNN_ENCODE_RATE = 0,       /**< Rate coding of dimensions */
    MENTAL_HEALTH_SNN_ENCODE_TEMPORAL,        /**< Temporal spike patterns */
    MENTAL_HEALTH_SNN_ENCODE_POPULATION,      /**< Population vector coding */
    MENTAL_HEALTH_SNN_ENCODE_SYNCHRONY        /**< Synchrony-based encoding */
} mental_health_snn_encoding_t;

/**
 * @brief Decoding methods for emotional states
 */
typedef enum {
    MENTAL_HEALTH_SNN_DECODE_THRESHOLD = 0,  /**< Threshold-based detection */
    MENTAL_HEALTH_SNN_DECODE_COMPETITION,     /**< Winner-take-all */
    MENTAL_HEALTH_SNN_DECODE_SOFTMAX,         /**< Soft probabilistic */
    MENTAL_HEALTH_SNN_DECODE_INTEGRATION      /**< Evidence accumulation */
} mental_health_snn_decoding_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    MENTAL_HEALTH_SNN_STATE_IDLE = 0,
    MENTAL_HEALTH_SNN_STATE_ENCODING,
    MENTAL_HEALTH_SNN_STATE_PROCESSING,
    MENTAL_HEALTH_SNN_STATE_DECODING,
    MENTAL_HEALTH_SNN_STATE_SIMULATING,
    MENTAL_HEALTH_SNN_STATE_ERROR
} mental_health_snn_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Mental Health-SNN bridge configuration
 */
typedef struct {
    /* Network dimensions */
    uint32_t num_dimensions;             /**< Number of mental health dimensions */
    uint32_t neurons_per_dim;            /**< Neurons per dimension */
    uint32_t hidden_dim;                 /**< Hidden layer dimension */

    /* Timing parameters */
    float dt_ms;                         /**< Simulation timestep (ms) */
    float encoding_window_ms;            /**< Encoding time window */
    float integration_tau_ms;            /**< Evidence integration time constant */

    /* Encoding parameters */
    mental_health_snn_encoding_t encoding; /**< Encoding method */
    float encoding_gain;                 /**< Encoding strength gain */
    float baseline_rate_hz;              /**< Baseline firing rate */
    float max_rate_hz;                   /**< Maximum firing rate */

    /* Decoding parameters */
    mental_health_snn_decoding_t decoding; /**< Decoding method */
    float anxiety_threshold;             /**< Threshold for anxiety detection */
    float depression_threshold;          /**< Threshold for depression detection */
    float state_change_threshold;        /**< Threshold for state change */

    /* Network dynamics */
    bool enable_competition;             /**< Enable dimension competition */
    float inhibition_strength;           /**< Lateral inhibition weight */
    bool enable_mood_tracking;           /**< Enable mood state detection */
    float mood_sensitivity;              /**< Mood detection sensitivity */

    /* Emotional regulation integration */
    bool enable_regulation;              /**< Enable regulation circuits */
    float regulation_gain;               /**< Regulation signal gain */
    bool enable_resilience_tracking;     /**< Enable resilience factor tracking */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
    bool enable_plasticity_integration;  /**< Enable plasticity bridge */
} mental_health_snn_config_t;

//=============================================================================
// State Structures
//=============================================================================

/**
 * @brief Per-dimension state tracking
 */
typedef struct {
    float activation;                    /**< Current activation level */
    float accumulated_evidence;          /**< Accumulated evidence */
    uint32_t spike_count;                /**< Recent spike count */
    float mean_rate_hz;                  /**< Mean firing rate */
    uint64_t last_spike_time_us;         /**< Last spike timestamp */
} mental_health_dim_state_t;

/**
 * @brief Emotional state output
 */
typedef struct {
    float mood_level;                    /**< Current mood [-1 to 1] */
    float anxiety_level;                 /**< Current anxiety [0-1] */
    float depression_level;              /**< Current depression [0-1] */
    float stress_level;                  /**< Stress response strength */
    float regulation_capacity;           /**< Emotional regulation ability */
    bool anxiety_detected;               /**< High anxiety detected */
    bool depression_detected;            /**< Depression markers detected */
    float resilience_factor;             /**< Current resilience level */
    float engagement_level;              /**< Behavioral engagement */
    float cognitive_clarity;             /**< Cognitive clarity level */
} mental_health_emotional_state_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    mental_health_snn_state_t state;     /**< Current operational state */
    uint32_t active_dimensions;          /**< Number of active dimensions */
    float total_activity;                /**< Total network activity */
    float mean_mood;                     /**< Mean mood state */
    float anxiety_signal;                /**< Current anxiety signal */
    float depression_signal;             /**< Current depression signal */
} mental_health_snn_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_evaluations;          /**< Total evaluations performed */
    uint64_t total_simulations;          /**< Total simulation steps */
    uint64_t total_spikes;               /**< Total spikes generated */
    uint64_t anxiety_detections;         /**< Anxiety detections */
    uint64_t depression_detections;      /**< Depression detections */
    uint64_t state_changes;              /**< State changes detected */
    float mean_evaluation_time_ms;       /**< Mean evaluation latency */
    float mean_mood;                     /**< Mean mood level */
} mental_health_snn_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct mental_health_snn_bridge mental_health_snn_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Anxiety detection callback */
typedef void (*mental_health_snn_anxiety_callback_t)(
    mental_health_snn_bridge_t* bridge,
    float anxiety_level,
    uint64_t latency_us,
    void* user_data
);

/** @brief Emotional state ready callback */
typedef void (*mental_health_snn_state_callback_t)(
    mental_health_snn_bridge_t* bridge,
    const mental_health_emotional_state_t* state,
    void* user_data
);

/** @brief Depression detection callback */
typedef void (*mental_health_snn_depression_callback_t)(
    mental_health_snn_bridge_t* bridge,
    float depression_level,
    uint32_t depression_dim,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
mental_health_snn_config_t mental_health_snn_config_default(void);

/**
 * @brief Create mental health SNN bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
mental_health_snn_bridge_t* mental_health_snn_create(const mental_health_snn_config_t* config);

/**
 * @brief Destroy mental health SNN bridge
 * @param bridge Bridge to destroy
 */
void mental_health_snn_destroy(mental_health_snn_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int mental_health_snn_reset(mental_health_snn_bridge_t* bridge);

//=============================================================================
// Encoding Functions
//=============================================================================

/**
 * @brief Encode mental health state into SNN
 * @param bridge Bridge handle
 * @param dimensions Dimension activation values [0-1]
 * @param num_dims Number of dimensions
 * @return Spike count on success, -1 on failure
 */
int mental_health_snn_encode_state(
    mental_health_snn_bridge_t* bridge,
    const float* dimensions,
    uint32_t num_dims
);

/**
 * @brief Encode mood state
 * @param bridge Bridge handle
 * @param mood Mood level [-1 to 1]
 * @param stability Mood stability [0-1]
 * @return Spike count on success, -1 on failure
 */
int mental_health_snn_encode_mood(
    mental_health_snn_bridge_t* bridge,
    float mood,
    float stability
);

/**
 * @brief Encode anxiety level
 * @param bridge Bridge handle
 * @param anxiety Anxiety level [0-1]
 * @param threat_count Number of active perceived threats
 * @return Spike count on success, -1 on failure
 */
int mental_health_snn_encode_anxiety(
    mental_health_snn_bridge_t* bridge,
    float anxiety,
    uint32_t threat_count
);

/**
 * @brief Encode depression markers
 * @param bridge Bridge handle
 * @param depression Depression level [0-1]
 * @param anhedonia Anhedonia/lack of pleasure [0-1]
 * @return Spike count on success, -1 on failure
 */
int mental_health_snn_encode_depression(
    mental_health_snn_bridge_t* bridge,
    float depression,
    float anhedonia
);

/**
 * @brief Encode stress response
 * @param bridge Bridge handle
 * @param stress Stress level [0-1]
 * @param stressor_type Type of stressor (0=acute, 1=chronic)
 * @return Spike count on success, -1 on failure
 */
int mental_health_snn_encode_stress(
    mental_health_snn_bridge_t* bridge,
    float stress,
    uint32_t stressor_type
);

//=============================================================================
// Simulation Functions
//=============================================================================

/**
 * @brief Simulate mental health processing
 * @param bridge Bridge handle
 * @param duration_ms Simulation duration
 * @return 0 on success, -1 on failure
 */
int mental_health_snn_simulate(mental_health_snn_bridge_t* bridge, float duration_ms);

/**
 * @brief Single simulation step
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int mental_health_snn_step(mental_health_snn_bridge_t* bridge);

/**
 * @brief Forward pass through network
 * @param bridge Bridge handle
 * @param inputs Input values
 * @param input_count Input size
 * @return Spike count, -1 on failure
 */
int mental_health_snn_forward(
    mental_health_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
);

//=============================================================================
// Decoding Functions
//=============================================================================

/**
 * @brief Get emotional state from SNN activity
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int mental_health_snn_get_emotional_state(
    mental_health_snn_bridge_t* bridge,
    mental_health_emotional_state_t* state
);

/**
 * @brief Get dimension activations
 * @param bridge Bridge handle
 * @param activations Output buffer for activations
 * @param num_dims Number of dimensions to query
 * @return 0 on success, -1 on failure
 */
int mental_health_snn_get_activations(
    mental_health_snn_bridge_t* bridge,
    float* activations,
    uint32_t num_dims
);

/**
 * @brief Check for high anxiety
 * @param bridge Bridge handle
 * @param anxiety_level Output anxiety level
 * @return true if high anxiety detected, false otherwise
 */
bool mental_health_snn_check_anxiety(
    mental_health_snn_bridge_t* bridge,
    float* anxiety_level
);

/**
 * @brief Check for depression markers
 * @param bridge Bridge handle
 * @param depression_level Output depression level
 * @return true if depression detected, false otherwise
 */
bool mental_health_snn_check_depression(
    mental_health_snn_bridge_t* bridge,
    float* depression_level
);

/**
 * @brief Check for state change
 * @param bridge Bridge handle
 * @param change_magnitude Output change magnitude
 * @return true if state change detected, false otherwise
 */
bool mental_health_snn_check_state_change(
    mental_health_snn_bridge_t* bridge,
    float* change_magnitude
);

//=============================================================================
// State Query Functions
//=============================================================================

/**
 * @brief Get dimension state
 * @param bridge Bridge handle
 * @param dim Dimension index
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int mental_health_snn_get_dim_state(
    mental_health_snn_bridge_t* bridge,
    uint32_t dim,
    mental_health_dim_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int mental_health_snn_get_state(
    mental_health_snn_bridge_t* bridge,
    mental_health_snn_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int mental_health_snn_get_stats(mental_health_snn_bridge_t* bridge, mental_health_snn_stats_t* stats);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int mental_health_snn_reset_stats(mental_health_snn_bridge_t* bridge);

/**
 * @brief Get current mood level
 * @param bridge Bridge handle
 * @return Mood level [-1 to 1], -2 on error
 */
float mental_health_snn_get_mood(mental_health_snn_bridge_t* bridge);

/**
 * @brief Get total network activity
 * @param bridge Bridge handle
 * @return Activity level, -1 on error
 */
float mental_health_snn_get_total_activity(mental_health_snn_bridge_t* bridge);

//=============================================================================
// Callback Registration
//=============================================================================

/**
 * @brief Register anxiety detection callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int mental_health_snn_register_anxiety_callback(
    mental_health_snn_bridge_t* bridge,
    mental_health_snn_anxiety_callback_t callback,
    void* user_data
);

/**
 * @brief Register emotional state callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int mental_health_snn_register_state_callback(
    mental_health_snn_bridge_t* bridge,
    mental_health_snn_state_callback_t callback,
    void* user_data
);

/**
 * @brief Register depression detection callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int mental_health_snn_register_depression_callback(
    mental_health_snn_bridge_t* bridge,
    mental_health_snn_depression_callback_t callback,
    void* user_data
);

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * @brief Connect to bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int mental_health_snn_bio_async_connect(mental_health_snn_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int mental_health_snn_bio_async_disconnect(mental_health_snn_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool mental_health_snn_is_bio_async_connected(mental_health_snn_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MENTAL_HEALTH_SNN_BRIDGE_H */
