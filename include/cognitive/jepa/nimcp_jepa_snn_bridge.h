/**
 * @file nimcp_jepa_snn_bridge.h
 * @brief JEPA - SNN Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between JEPA predictor and spiking neural networks
 * WHY:  Enable biologically-plausible self-supervised learning through
 *       population coding and spike-timing dynamics in latent space
 * HOW:  Encode JEPA latent representations as spike patterns, decode predictions
 *       from neural activity patterns
 *
 * THEORETICAL FOUNDATIONS:
 * - LeCun (2022): JEPA - Joint Embedding Predictive Architecture
 * - Gerstner (2002): Spike-timing dependent plasticity
 * - Maass (2002): Computing with spiking neurons
 *
 * BIOLOGICAL BASIS:
 * - Predictive coding in cortical hierarchies
 * - Population coding for latent representations
 * - Temporal integration for context embedding
 * - Error signals as spike rate modulation
 *
 * INTEGRATION WITH LEARNING:
 * - Latent space predictions through population activity
 * - Prediction error as spike rate deviation
 * - Multi-modal integration via synchronized populations
 *
 * @see nimcp_jepa_predictor.h
 * @see nimcp_snn_network.h
 */

#ifndef NIMCP_JEPA_SNN_BRIDGE_H
#define NIMCP_JEPA_SNN_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum JEPA dimensions to encode */
#define JEPA_SNN_MAX_DIMENSIONS      16

/** @brief Neurons per latent dimension */
#define JEPA_SNN_NEURONS_PER_DIM     32

/** @brief Default prediction error threshold */
#define JEPA_SNN_PRED_ERROR_THRESH   0.5f

/** @brief Default encoding window (ms) */
#define JEPA_SNN_ENCODING_WINDOW     50.0f

/** @brief Bio-async module ID */
#define BIO_MODULE_JEPA_SNN          0x0E50

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief JEPA dimension types for SNN encoding
 */
typedef enum {
    JEPA_DIM_LATENT_CONTEXT = 0,         /**< Context latent representation */
    JEPA_DIM_LATENT_TARGET,               /**< Target latent representation */
    JEPA_DIM_PREDICTION_ERROR,            /**< Prediction error magnitude */
    JEPA_DIM_CONTEXT_EMBEDDING,           /**< Context embedding state */
    JEPA_DIM_MULTIMODAL_FUSION,           /**< Multi-modal integration level */
    JEPA_DIM_SELF_SUPERVISED,             /**< Self-supervised learning signal */
    JEPA_DIM_MASKING_PATTERN,             /**< Masking pattern encoding */
    JEPA_DIM_PRECISION_WEIGHT,            /**< FEP precision weighting */
    JEPA_DIM_TEMPORAL_CONTEXT,            /**< Temporal context state */
    JEPA_DIM_PREDICTION_CONFIDENCE,       /**< Prediction confidence level */
    JEPA_DIM_COUNT
} jepa_snn_dimension_t;

/**
 * @brief Encoding methods for latent representations
 */
typedef enum {
    JEPA_SNN_ENCODE_RATE = 0,            /**< Rate coding of dimensions */
    JEPA_SNN_ENCODE_TEMPORAL,             /**< Temporal spike patterns */
    JEPA_SNN_ENCODE_POPULATION,           /**< Population vector coding */
    JEPA_SNN_ENCODE_SYNCHRONY             /**< Synchrony-based encoding */
} jepa_snn_encoding_t;

/**
 * @brief Decoding methods for prediction states
 */
typedef enum {
    JEPA_SNN_DECODE_THRESHOLD = 0,       /**< Threshold-based detection */
    JEPA_SNN_DECODE_COMPETITION,          /**< Winner-take-all */
    JEPA_SNN_DECODE_SOFTMAX,              /**< Soft probabilistic */
    JEPA_SNN_DECODE_INTEGRATION           /**< Evidence accumulation */
} jepa_snn_decoding_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    JEPA_SNN_STATE_IDLE = 0,
    JEPA_SNN_STATE_ENCODING,
    JEPA_SNN_STATE_PROCESSING,
    JEPA_SNN_STATE_DECODING,
    JEPA_SNN_STATE_SIMULATING,
    JEPA_SNN_STATE_ERROR
} jepa_snn_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief JEPA-SNN bridge configuration
 */
typedef struct {
    /* Network dimensions */
    uint32_t num_dimensions;             /**< Number of JEPA dimensions */
    uint32_t neurons_per_dim;            /**< Neurons per dimension */
    uint32_t hidden_dim;                 /**< Hidden layer dimension */

    /* Timing parameters */
    float dt_ms;                         /**< Simulation timestep (ms) */
    float encoding_window_ms;            /**< Encoding time window */
    float integration_tau_ms;            /**< Evidence integration time constant */

    /* Encoding parameters */
    jepa_snn_encoding_t encoding;        /**< Encoding method */
    float encoding_gain;                 /**< Encoding strength gain */
    float baseline_rate_hz;              /**< Baseline firing rate */
    float max_rate_hz;                   /**< Maximum firing rate */

    /* Decoding parameters */
    jepa_snn_decoding_t decoding;        /**< Decoding method */
    float prediction_error_threshold;    /**< Threshold for error detection */
    float confidence_threshold;          /**< Minimum prediction confidence */
    float state_change_threshold;        /**< Threshold for state change */

    /* Network dynamics */
    bool enable_competition;             /**< Enable dimension competition */
    float inhibition_strength;           /**< Lateral inhibition weight */
    bool enable_error_detection;         /**< Enable prediction error detection */
    float error_sensitivity;             /**< Prediction error sensitivity */

    /* Prediction integration */
    bool enable_prediction;              /**< Enable prediction circuits */
    float prediction_gain;               /**< Prediction signal gain */
    bool enable_context_tracking;        /**< Enable context tracking */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
    bool enable_plasticity_integration;  /**< Enable plasticity bridge */
} jepa_snn_config_t;

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
} jepa_dim_state_t;

/**
 * @brief JEPA prediction output from SNN
 */
typedef struct {
    float context_level;                 /**< Context embedding strength [0-1] */
    float target_level;                  /**< Target encoding strength [0-1] */
    float prediction_error;              /**< Prediction error magnitude [0-1] */
    float prediction_confidence;         /**< Prediction confidence */
    float multimodal_integration;        /**< Multi-modal fusion level */
    bool error_detected;                 /**< Prediction error detected */
    bool high_confidence;                /**< High confidence state */
    float confidence_magnitude;          /**< Confidence magnitude if detected */
    float self_supervised_signal;        /**< Self-supervised learning signal */
    float temporal_context;              /**< Temporal context signal */
} jepa_prediction_output_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    jepa_snn_state_t state;              /**< Current operational state */
    uint32_t active_dimensions;          /**< Number of active dimensions */
    float total_activity;                /**< Total network activity */
    float mean_prediction;               /**< Mean prediction strength */
    float error_signal;                  /**< Current error signal */
    float context_signal;                /**< Current context signal */
} jepa_snn_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_evaluations;          /**< Total evaluations performed */
    uint64_t total_simulations;          /**< Total simulation steps */
    uint64_t total_spikes;               /**< Total spikes generated */
    uint64_t error_detections;           /**< Prediction error detections */
    uint64_t high_confidence_events;     /**< High confidence events */
    uint64_t state_changes;              /**< State changes detected */
    float mean_evaluation_time_ms;       /**< Mean evaluation latency */
    float mean_prediction_error;         /**< Mean prediction error */
} jepa_snn_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct jepa_snn_bridge jepa_snn_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Prediction error detection callback */
typedef void (*jepa_snn_error_callback_t)(
    jepa_snn_bridge_t* bridge,
    float error_level,
    uint64_t latency_us,
    void* user_data
);

/** @brief Prediction output ready callback */
typedef void (*jepa_snn_prediction_callback_t)(
    jepa_snn_bridge_t* bridge,
    const jepa_prediction_output_t* output,
    void* user_data
);

/** @brief High confidence callback */
typedef void (*jepa_snn_confidence_callback_t)(
    jepa_snn_bridge_t* bridge,
    float confidence_level,
    uint32_t confidence_dim,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
jepa_snn_config_t jepa_snn_config_default(void);

/**
 * @brief Create JEPA SNN bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
jepa_snn_bridge_t* jepa_snn_create(const jepa_snn_config_t* config);

/**
 * @brief Destroy JEPA SNN bridge
 * @param bridge Bridge to destroy
 */
void jepa_snn_destroy(jepa_snn_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int jepa_snn_reset(jepa_snn_bridge_t* bridge);

//=============================================================================
// Encoding Functions
//=============================================================================

/**
 * @brief Encode JEPA state into SNN
 * @param bridge Bridge handle
 * @param dimensions Dimension activation values [0-1]
 * @param num_dims Number of dimensions
 * @return Spike count on success, -1 on failure
 */
int jepa_snn_encode_state(
    jepa_snn_bridge_t* bridge,
    const float* dimensions,
    uint32_t num_dims
);

/**
 * @brief Encode latent representation
 * @param bridge Bridge handle
 * @param context Context latent value [0-1]
 * @param target Target latent value [0-1]
 * @return Spike count on success, -1 on failure
 */
int jepa_snn_encode_latent(
    jepa_snn_bridge_t* bridge,
    float context,
    float target
);

/**
 * @brief Encode prediction error
 * @param bridge Bridge handle
 * @param error_magnitude Error magnitude [0-1]
 * @param error_dimension Error dimension index
 * @return Spike count on success, -1 on failure
 */
int jepa_snn_encode_prediction_error(
    jepa_snn_bridge_t* bridge,
    float error_magnitude,
    uint32_t error_dimension
);

/**
 * @brief Encode context embedding
 * @param bridge Bridge handle
 * @param context_strength Context embedding strength [0-1]
 * @param context_type Context type classification
 * @return Spike count on success, -1 on failure
 */
int jepa_snn_encode_context(
    jepa_snn_bridge_t* bridge,
    float context_strength,
    uint32_t context_type
);

//=============================================================================
// Simulation Functions
//=============================================================================

/**
 * @brief Simulate JEPA processing
 * @param bridge Bridge handle
 * @param duration_ms Simulation duration
 * @return 0 on success, -1 on failure
 */
int jepa_snn_simulate(jepa_snn_bridge_t* bridge, float duration_ms);

/**
 * @brief Single simulation step
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int jepa_snn_step(jepa_snn_bridge_t* bridge);

/**
 * @brief Forward pass through network
 * @param bridge Bridge handle
 * @param inputs Input values
 * @param input_count Input size
 * @return Spike count, -1 on failure
 */
int jepa_snn_forward(
    jepa_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
);

//=============================================================================
// Decoding Functions
//=============================================================================

/**
 * @brief Get prediction output from SNN activity
 * @param bridge Bridge handle
 * @param output Output prediction structure
 * @return 0 on success, -1 on failure
 */
int jepa_snn_get_prediction(
    jepa_snn_bridge_t* bridge,
    jepa_prediction_output_t* output
);

/**
 * @brief Get dimension activations
 * @param bridge Bridge handle
 * @param activations Output buffer for activations
 * @param num_dims Number of dimensions to query
 * @return 0 on success, -1 on failure
 */
int jepa_snn_get_activations(
    jepa_snn_bridge_t* bridge,
    float* activations,
    uint32_t num_dims
);

/**
 * @brief Check for prediction error
 * @param bridge Bridge handle
 * @param error_level Output error level
 * @return true if prediction error detected, false otherwise
 */
bool jepa_snn_check_prediction_error(
    jepa_snn_bridge_t* bridge,
    float* error_level
);

/**
 * @brief Check for high confidence
 * @param bridge Bridge handle
 * @param confidence_level Output confidence level
 * @return true if high confidence detected, false otherwise
 */
bool jepa_snn_check_confidence(
    jepa_snn_bridge_t* bridge,
    float* confidence_level
);

/**
 * @brief Check for state change
 * @param bridge Bridge handle
 * @param change_magnitude Output change magnitude
 * @return true if state change detected, false otherwise
 */
bool jepa_snn_check_state_change(
    jepa_snn_bridge_t* bridge,
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
int jepa_snn_get_dim_state(
    jepa_snn_bridge_t* bridge,
    uint32_t dim,
    jepa_dim_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int jepa_snn_get_state(
    jepa_snn_bridge_t* bridge,
    jepa_snn_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int jepa_snn_get_stats(jepa_snn_bridge_t* bridge, jepa_snn_stats_t* stats);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int jepa_snn_reset_stats(jepa_snn_bridge_t* bridge);

/**
 * @brief Get current prediction confidence level
 * @param bridge Bridge handle
 * @return Prediction confidence [0-1], -1 on error
 */
float jepa_snn_get_prediction_confidence(jepa_snn_bridge_t* bridge);

/**
 * @brief Get total network activity
 * @param bridge Bridge handle
 * @return Activity level, -1 on error
 */
float jepa_snn_get_total_activity(jepa_snn_bridge_t* bridge);

//=============================================================================
// Callback Registration
//=============================================================================

/**
 * @brief Register prediction error detection callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int jepa_snn_register_error_callback(
    jepa_snn_bridge_t* bridge,
    jepa_snn_error_callback_t callback,
    void* user_data
);

/**
 * @brief Register prediction output callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int jepa_snn_register_prediction_callback(
    jepa_snn_bridge_t* bridge,
    jepa_snn_prediction_callback_t callback,
    void* user_data
);

/**
 * @brief Register high confidence callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int jepa_snn_register_confidence_callback(
    jepa_snn_bridge_t* bridge,
    jepa_snn_confidence_callback_t callback,
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
int jepa_snn_bio_async_connect(jepa_snn_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int jepa_snn_bio_async_disconnect(jepa_snn_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool jepa_snn_is_bio_async_connected(jepa_snn_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_JEPA_SNN_BRIDGE_H */
