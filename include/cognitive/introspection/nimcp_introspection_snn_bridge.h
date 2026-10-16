/**
 * @file nimcp_introspection_snn_bridge.h
 * @brief Introspection - SNN Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between introspection engine and spiking neural networks
 * WHY:  Enable biologically-plausible metacognitive processing through
 *       population coding and spike-timing dynamics
 * HOW:  Encode self-awareness states as spike patterns, decode metacognitive
 *       insights from neural activity patterns
 *
 * THEORETICAL FOUNDATIONS:
 * - Dehaene (2014): Global Neuronal Workspace - consciousness and metacognition
 * - Fleming (2010): Metacognition and confidence in neural circuits
 * - Cleeremans (2011): Radical plasticity thesis for consciousness
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal cortex (PFC) for self-monitoring and metacognition
 * - Anterior cingulate cortex (ACC) for error detection and conflict
 * - Insula for interoceptive awareness
 * - Default mode network (DMN) for self-referential processing
 *
 * INTEGRATION WITH CONSCIOUSNESS:
 * - Uncertainty estimation through population variability
 * - Pattern activity monitoring via synchrony detection
 * - State awareness through activity pattern recognition
 *
 * @see nimcp_introspection.h
 * @see nimcp_snn_network.h
 */

#ifndef NIMCP_INTROSPECTION_SNN_BRIDGE_H
#define NIMCP_INTROSPECTION_SNN_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum introspection dimensions to encode */
#define INTROSPECTION_SNN_MAX_DIMENSIONS     16

/** @brief Neurons per introspection dimension */
#define INTROSPECTION_SNN_NEURONS_PER_DIM    32

/** @brief Default uncertainty threshold */
#define INTROSPECTION_SNN_UNCERTAINTY_THRESH 0.5f

/** @brief Default encoding window (ms) */
#define INTROSPECTION_SNN_ENCODING_WINDOW    50.0f

/** @brief Bio-async module ID */
#define BIO_MODULE_INTROSPECTION_SNN         0x0D40

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Introspection dimension types for SNN encoding
 */
typedef enum {
    INTROSPECTION_DIM_CERTAINTY = 0,    /**< Epistemic certainty level */
    INTROSPECTION_DIM_UNCERTAINTY,       /**< Epistemic uncertainty level */
    INTROSPECTION_DIM_CONFIDENCE,        /**< Decision confidence */
    INTROSPECTION_DIM_ALERTNESS,         /**< Current alertness/arousal */
    INTROSPECTION_DIM_ATTENTION_FOCUS,   /**< Attention focus strength */
    INTROSPECTION_DIM_PATTERN_MATCH,     /**< Pattern recognition strength */
    INTROSPECTION_DIM_STATE_CHANGE,      /**< State change detection */
    INTROSPECTION_DIM_CONFLICT,          /**< Internal conflict signal */
    INTROSPECTION_DIM_METACOGNITION,     /**< Meta-level processing */
    INTROSPECTION_DIM_SELF_REFERENCE,    /**< Self-referential activity */
    INTROSPECTION_DIM_ERROR_SIGNAL,      /**< Error detection signal */
    INTROSPECTION_DIM_INTEGRATION,       /**< Information integration */
    INTROSPECTION_DIM_COUNT
} introspection_snn_dimension_t;

/**
 * @brief Encoding methods for introspective contexts
 */
typedef enum {
    INTROSPECTION_SNN_ENCODE_RATE = 0,   /**< Rate coding of dimensions */
    INTROSPECTION_SNN_ENCODE_TEMPORAL,    /**< Temporal spike patterns */
    INTROSPECTION_SNN_ENCODE_POPULATION,  /**< Population vector coding */
    INTROSPECTION_SNN_ENCODE_SYNCHRONY    /**< Synchrony-based encoding */
} introspection_snn_encoding_t;

/**
 * @brief Decoding methods for metacognitive states
 */
typedef enum {
    INTROSPECTION_SNN_DECODE_THRESHOLD = 0, /**< Threshold-based detection */
    INTROSPECTION_SNN_DECODE_COMPETITION,    /**< Winner-take-all */
    INTROSPECTION_SNN_DECODE_SOFTMAX,        /**< Soft probabilistic */
    INTROSPECTION_SNN_DECODE_INTEGRATION     /**< Evidence accumulation */
} introspection_snn_decoding_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    INTROSPECTION_SNN_STATE_IDLE = 0,
    INTROSPECTION_SNN_STATE_ENCODING,
    INTROSPECTION_SNN_STATE_PROCESSING,
    INTROSPECTION_SNN_STATE_DECODING,
    INTROSPECTION_SNN_STATE_SIMULATING,
    INTROSPECTION_SNN_STATE_ERROR
} introspection_snn_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Introspection-SNN bridge configuration
 */
typedef struct {
    /* Network dimensions */
    uint32_t num_dimensions;             /**< Number of introspection dimensions */
    uint32_t neurons_per_dim;            /**< Neurons per dimension */
    uint32_t hidden_dim;                 /**< Hidden layer dimension */

    /* Timing parameters */
    float dt_ms;                         /**< Simulation timestep (ms) */
    float encoding_window_ms;            /**< Encoding time window */
    float integration_tau_ms;            /**< Evidence integration time constant */

    /* Encoding parameters */
    introspection_snn_encoding_t encoding; /**< Encoding method */
    float encoding_gain;                 /**< Encoding strength gain */
    float baseline_rate_hz;              /**< Baseline firing rate */
    float max_rate_hz;                   /**< Maximum firing rate */

    /* Decoding parameters */
    introspection_snn_decoding_t decoding; /**< Decoding method */
    float uncertainty_threshold;         /**< Threshold for uncertainty detection */
    float confidence_threshold;          /**< Minimum confidence required */
    float state_change_threshold;        /**< Threshold for state change */

    /* Network dynamics */
    bool enable_competition;             /**< Enable dimension competition */
    float inhibition_strength;           /**< Lateral inhibition weight */
    bool enable_error_detection;         /**< Enable error signal detection */
    float error_threshold;               /**< Error detection threshold */

    /* Metacognition integration */
    bool enable_metacognition;           /**< Enable meta-level processing */
    float metacognition_gain;            /**< Metacognition signal gain */
    bool enable_self_reference;          /**< Enable self-referential circuits */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
    bool enable_plasticity_integration;  /**< Enable plasticity bridge */
} introspection_snn_config_t;

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
} introspection_dim_state_t;

/**
 * @brief Metacognitive insight output
 */
typedef struct {
    float certainty_level;               /**< Current certainty [0-1] */
    float uncertainty_level;             /**< Current uncertainty [0-1] */
    float confidence;                    /**< Decision confidence [0-1] */
    float alertness;                     /**< Alertness/arousal level */
    float attention_focus;               /**< Attention focus strength */
    bool state_change_detected;          /**< State change detected */
    bool error_detected;                 /**< Error signal detected */
    float error_magnitude;               /**< Error magnitude if detected */
    float metacognition_level;           /**< Metacognitive activity level */
    float integration_score;             /**< Information integration (phi-like) */
} introspection_insight_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    introspection_snn_state_t state;     /**< Current operational state */
    uint32_t active_dimensions;          /**< Number of active dimensions */
    float total_activity;                /**< Total network activity */
    float mean_confidence;               /**< Mean decision confidence */
    float uncertainty_signal;            /**< Current uncertainty signal */
    float error_signal;                  /**< Current error signal */
} introspection_snn_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_evaluations;          /**< Total evaluations performed */
    uint64_t total_simulations;          /**< Total simulation steps */
    uint64_t total_spikes;               /**< Total spikes generated */
    uint64_t uncertainty_detections;     /**< High uncertainty detections */
    uint64_t error_detections;           /**< Error signal detections */
    uint64_t state_changes;              /**< State changes detected */
    float mean_evaluation_time_ms;       /**< Mean evaluation latency */
    float mean_confidence;               /**< Mean confidence score */
} introspection_snn_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct introspection_snn_bridge introspection_snn_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Uncertainty detection callback */
typedef void (*introspection_snn_uncertainty_callback_t)(
    introspection_snn_bridge_t* bridge,
    float uncertainty_level,
    uint64_t latency_us,
    void* user_data
);

/** @brief Insight ready callback */
typedef void (*introspection_snn_insight_callback_t)(
    introspection_snn_bridge_t* bridge,
    const introspection_insight_t* insight,
    void* user_data
);

/** @brief Error detection callback */
typedef void (*introspection_snn_error_callback_t)(
    introspection_snn_bridge_t* bridge,
    float error_level,
    uint32_t error_dim,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
introspection_snn_config_t introspection_snn_config_default(void);

/**
 * @brief Create introspection SNN bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
introspection_snn_bridge_t* introspection_snn_create(const introspection_snn_config_t* config);

/**
 * @brief Destroy introspection SNN bridge
 * @param bridge Bridge to destroy
 */
void introspection_snn_destroy(introspection_snn_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int introspection_snn_reset(introspection_snn_bridge_t* bridge);

//=============================================================================
// Encoding Functions
//=============================================================================

/**
 * @brief Encode introspective state into SNN
 * @param bridge Bridge handle
 * @param dimensions Dimension activation values [0-1]
 * @param num_dims Number of dimensions
 * @return Spike count on success, -1 on failure
 */
int introspection_snn_encode_state(
    introspection_snn_bridge_t* bridge,
    const float* dimensions,
    uint32_t num_dims
);

/**
 * @brief Encode uncertainty estimate
 * @param bridge Bridge handle
 * @param epistemic Epistemic uncertainty [0-1]
 * @param aleatoric Aleatoric uncertainty [0-1]
 * @return Spike count on success, -1 on failure
 */
int introspection_snn_encode_uncertainty(
    introspection_snn_bridge_t* bridge,
    float epistemic,
    float aleatoric
);

/**
 * @brief Encode pattern activity
 * @param bridge Bridge handle
 * @param pattern_strength Pattern match strength [0-1]
 * @param pattern_count Number of active patterns
 * @return Spike count on success, -1 on failure
 */
int introspection_snn_encode_pattern(
    introspection_snn_bridge_t* bridge,
    float pattern_strength,
    uint32_t pattern_count
);

/**
 * @brief Encode error signal
 * @param bridge Bridge handle
 * @param error_magnitude Error magnitude [0-1]
 * @param error_type Error type classification
 * @return Spike count on success, -1 on failure
 */
int introspection_snn_encode_error(
    introspection_snn_bridge_t* bridge,
    float error_magnitude,
    uint32_t error_type
);

//=============================================================================
// Simulation Functions
//=============================================================================

/**
 * @brief Simulate introspective processing
 * @param bridge Bridge handle
 * @param duration_ms Simulation duration
 * @return 0 on success, -1 on failure
 */
int introspection_snn_simulate(introspection_snn_bridge_t* bridge, float duration_ms);

/**
 * @brief Single simulation step
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int introspection_snn_step(introspection_snn_bridge_t* bridge);

/**
 * @brief Forward pass through network
 * @param bridge Bridge handle
 * @param inputs Input values
 * @param input_count Input size
 * @return Spike count, -1 on failure
 */
int introspection_snn_forward(
    introspection_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
);

//=============================================================================
// Decoding Functions
//=============================================================================

/**
 * @brief Get metacognitive insight from SNN activity
 * @param bridge Bridge handle
 * @param insight Output insight structure
 * @return 0 on success, -1 on failure
 */
int introspection_snn_get_insight(
    introspection_snn_bridge_t* bridge,
    introspection_insight_t* insight
);

/**
 * @brief Get dimension activations
 * @param bridge Bridge handle
 * @param activations Output buffer for activations
 * @param num_dims Number of dimensions to query
 * @return 0 on success, -1 on failure
 */
int introspection_snn_get_activations(
    introspection_snn_bridge_t* bridge,
    float* activations,
    uint32_t num_dims
);

/**
 * @brief Check for high uncertainty
 * @param bridge Bridge handle
 * @param uncertainty_level Output uncertainty level
 * @return true if high uncertainty detected, false otherwise
 */
bool introspection_snn_check_uncertainty(
    introspection_snn_bridge_t* bridge,
    float* uncertainty_level
);

/**
 * @brief Check for error detection
 * @param bridge Bridge handle
 * @param error_level Output error level
 * @return true if error detected, false otherwise
 */
bool introspection_snn_check_error(
    introspection_snn_bridge_t* bridge,
    float* error_level
);

/**
 * @brief Check for state change
 * @param bridge Bridge handle
 * @param change_magnitude Output change magnitude
 * @return true if state change detected, false otherwise
 */
bool introspection_snn_check_state_change(
    introspection_snn_bridge_t* bridge,
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
int introspection_snn_get_dim_state(
    introspection_snn_bridge_t* bridge,
    uint32_t dim,
    introspection_dim_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int introspection_snn_get_state(
    introspection_snn_bridge_t* bridge,
    introspection_snn_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int introspection_snn_get_stats(introspection_snn_bridge_t* bridge, introspection_snn_stats_t* stats);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int introspection_snn_reset_stats(introspection_snn_bridge_t* bridge);

/**
 * @brief Get current confidence level
 * @param bridge Bridge handle
 * @return Confidence level [0-1], -1 on error
 */
float introspection_snn_get_confidence(introspection_snn_bridge_t* bridge);

/**
 * @brief Get total network activity
 * @param bridge Bridge handle
 * @return Activity level, -1 on error
 */
float introspection_snn_get_total_activity(introspection_snn_bridge_t* bridge);

//=============================================================================
// Callback Registration
//=============================================================================

/**
 * @brief Register uncertainty detection callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int introspection_snn_register_uncertainty_callback(
    introspection_snn_bridge_t* bridge,
    introspection_snn_uncertainty_callback_t callback,
    void* user_data
);

/**
 * @brief Register insight callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int introspection_snn_register_insight_callback(
    introspection_snn_bridge_t* bridge,
    introspection_snn_insight_callback_t callback,
    void* user_data
);

/**
 * @brief Register error detection callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int introspection_snn_register_error_callback(
    introspection_snn_bridge_t* bridge,
    introspection_snn_error_callback_t callback,
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
int introspection_snn_bio_async_connect(introspection_snn_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int introspection_snn_bio_async_disconnect(introspection_snn_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool introspection_snn_is_bio_async_connected(introspection_snn_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_INTROSPECTION_SNN_BRIDGE_H */
