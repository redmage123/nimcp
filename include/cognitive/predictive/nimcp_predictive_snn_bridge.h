/**
 * @file nimcp_predictive_snn_bridge.h
 * @brief Predictive Processing - SNN Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between predictive coding and spiking neural networks
 * WHY:  Enable biologically-plausible predictive processing through
 *       population coding and spike-timing dynamics
 * HOW:  Encode prediction dimensions as spike patterns, decode anticipation
 *       signals from neural activity patterns
 *
 * THEORETICAL FOUNDATIONS:
 * - Friston (2010): Free Energy Principle and Predictive Coding
 * - Rao & Ballard (1999): Predictive coding in visual cortex
 * - Bastos et al. (2012): Canonical microcircuits for predictive coding
 *
 * BIOLOGICAL BASIS:
 * - Cortical hierarchies for prediction generation
 * - Pyramidal cells for top-down predictions
 * - Error units for bottom-up prediction errors
 * - Precision-weighting through neuromodulation
 *
 * INTEGRATION WITH LEARNING:
 * - Prediction error encoding through population variability
 * - Model updating via firing rate adaptations
 * - Anticipation through sustained activity patterns
 *
 * @see nimcp_predictive.h
 * @see nimcp_snn_network.h
 */

#ifndef NIMCP_PREDICTIVE_SNN_BRIDGE_H
#define NIMCP_PREDICTIVE_SNN_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum prediction dimensions to encode */
#define PREDICTIVE_SNN_MAX_DIMENSIONS     16

/** @brief Neurons per prediction dimension */
#define PREDICTIVE_SNN_NEURONS_PER_DIM    32

/** @brief Default prediction error threshold */
#define PREDICTIVE_SNN_ERROR_THRESH       0.5f

/** @brief Default encoding window (ms) */
#define PREDICTIVE_SNN_ENCODING_WINDOW    50.0f

/** @brief Bio-async module ID */
#define BIO_MODULE_PREDICTIVE_SNN         0x0D60

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Prediction dimension types for SNN encoding
 */
typedef enum {
    PREDICTIVE_DIM_PREDICTION = 0,       /**< Current prediction level */
    PREDICTIVE_DIM_ERROR,                /**< Prediction error magnitude */
    PREDICTIVE_DIM_PRECISION,            /**< Precision/confidence weighting */
    PREDICTIVE_DIM_ANTICIPATION,         /**< Temporal anticipation strength */
    PREDICTIVE_DIM_MODEL_STATE,          /**< Internal model state */
    PREDICTIVE_DIM_FREE_ENERGY,          /**< Free energy level */
    PREDICTIVE_DIM_SURPRISE,             /**< Surprise/violation detection */
    PREDICTIVE_DIM_HIERARCHY_LEVEL,      /**< Hierarchical processing level */
    PREDICTIVE_DIM_INFERENCE,            /**< Active inference signal */
    PREDICTIVE_DIM_EXPECTATION,          /**< Expectation signal strength */
    PREDICTIVE_DIM_COUNT
} predictive_snn_dimension_t;

/**
 * @brief Encoding methods for predictive contexts
 */
typedef enum {
    PREDICTIVE_SNN_ENCODE_RATE = 0,      /**< Rate coding of dimensions */
    PREDICTIVE_SNN_ENCODE_TEMPORAL,       /**< Temporal spike patterns */
    PREDICTIVE_SNN_ENCODE_POPULATION,     /**< Population vector coding */
    PREDICTIVE_SNN_ENCODE_SYNCHRONY       /**< Synchrony-based encoding */
} predictive_snn_encoding_t;

/**
 * @brief Decoding methods for anticipation states
 */
typedef enum {
    PREDICTIVE_SNN_DECODE_THRESHOLD = 0, /**< Threshold-based detection */
    PREDICTIVE_SNN_DECODE_COMPETITION,    /**< Winner-take-all */
    PREDICTIVE_SNN_DECODE_SOFTMAX,        /**< Soft probabilistic */
    PREDICTIVE_SNN_DECODE_INTEGRATION     /**< Evidence accumulation */
} predictive_snn_decoding_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    PREDICTIVE_SNN_STATE_IDLE = 0,
    PREDICTIVE_SNN_STATE_ENCODING,
    PREDICTIVE_SNN_STATE_PROCESSING,
    PREDICTIVE_SNN_STATE_DECODING,
    PREDICTIVE_SNN_STATE_SIMULATING,
    PREDICTIVE_SNN_STATE_ERROR
} predictive_snn_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Predictive-SNN bridge configuration
 */
typedef struct {
    /* Network dimensions */
    uint32_t num_dimensions;             /**< Number of prediction dimensions */
    uint32_t neurons_per_dim;            /**< Neurons per dimension */
    uint32_t hidden_dim;                 /**< Hidden layer dimension */

    /* Timing parameters */
    float dt_ms;                         /**< Simulation timestep (ms) */
    float encoding_window_ms;            /**< Encoding time window */
    float integration_tau_ms;            /**< Evidence integration time constant */

    /* Encoding parameters */
    predictive_snn_encoding_t encoding;  /**< Encoding method */
    float encoding_gain;                 /**< Encoding strength gain */
    float baseline_rate_hz;              /**< Baseline firing rate */
    float max_rate_hz;                   /**< Maximum firing rate */

    /* Decoding parameters */
    predictive_snn_decoding_t decoding;  /**< Decoding method */
    float error_threshold;               /**< Threshold for error detection */
    float anticipation_threshold;        /**< Minimum anticipation drive */
    float state_change_threshold;        /**< Threshold for state change */

    /* Network dynamics */
    bool enable_competition;             /**< Enable dimension competition */
    float inhibition_strength;           /**< Lateral inhibition weight */
    bool enable_error_detection;         /**< Enable error signal detection */
    float error_sensitivity;             /**< Error detection sensitivity */

    /* Anticipation integration */
    bool enable_anticipation;            /**< Enable anticipation circuits */
    float anticipation_gain;             /**< Anticipation signal gain */
    bool enable_model_updating;          /**< Enable model state updating */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
    bool enable_plasticity_integration;  /**< Enable plasticity bridge */
} predictive_snn_config_t;

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
} predictive_dim_state_t;

/**
 * @brief Anticipation output
 */
typedef struct {
    float prediction_level;              /**< Current prediction [0-1] */
    float error_level;                   /**< Prediction error [0-1] */
    float precision_level;               /**< Precision weighting [0-1] */
    float anticipation_drive;            /**< Anticipation drive strength */
    float model_confidence;              /**< Internal model confidence */
    bool error_detected;                 /**< Error threshold exceeded */
    bool high_anticipation;              /**< High anticipation state */
    float anticipation_magnitude;        /**< Anticipation magnitude if detected */
    float free_energy_level;             /**< Free energy estimate */
    float expectation_strength;          /**< Expectation signal strength */
} predictive_anticipation_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    predictive_snn_state_t state;        /**< Current operational state */
    uint32_t active_dimensions;          /**< Number of active dimensions */
    float total_activity;                /**< Total network activity */
    float mean_anticipation;             /**< Mean anticipation drive */
    float error_signal;                  /**< Current error signal */
    float precision_signal;              /**< Current precision signal */
} predictive_snn_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_evaluations;          /**< Total evaluations performed */
    uint64_t total_simulations;          /**< Total simulation steps */
    uint64_t total_spikes;               /**< Total spikes generated */
    uint64_t error_detections;           /**< Error detections */
    uint64_t high_anticipation_events;   /**< High anticipation events */
    uint64_t state_changes;              /**< State changes detected */
    float mean_evaluation_time_ms;       /**< Mean evaluation latency */
    float mean_anticipation;             /**< Mean anticipation drive */
} predictive_snn_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct predictive_snn_bridge predictive_snn_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Error detection callback */
typedef void (*predictive_snn_error_callback_t)(
    predictive_snn_bridge_t* bridge,
    float error_level,
    uint64_t latency_us,
    void* user_data
);

/** @brief Anticipation ready callback */
typedef void (*predictive_snn_anticipation_callback_t)(
    predictive_snn_bridge_t* bridge,
    const predictive_anticipation_t* anticipation,
    void* user_data
);

/** @brief High anticipation callback */
typedef void (*predictive_snn_high_anticipation_callback_t)(
    predictive_snn_bridge_t* bridge,
    float anticipation_level,
    uint32_t anticipation_dim,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
predictive_snn_config_t predictive_snn_config_default(void);

/**
 * @brief Create predictive SNN bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
predictive_snn_bridge_t* predictive_snn_create(const predictive_snn_config_t* config);

/**
 * @brief Destroy predictive SNN bridge
 * @param bridge Bridge to destroy
 */
void predictive_snn_destroy(predictive_snn_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int predictive_snn_reset(predictive_snn_bridge_t* bridge);

//=============================================================================
// Encoding Functions
//=============================================================================

/**
 * @brief Encode predictive state into SNN
 * @param bridge Bridge handle
 * @param dimensions Dimension activation values [0-1]
 * @param num_dims Number of dimensions
 * @return Spike count on success, -1 on failure
 */
int predictive_snn_encode_state(
    predictive_snn_bridge_t* bridge,
    const float* dimensions,
    uint32_t num_dims
);

/**
 * @brief Encode prediction error
 * @param bridge Bridge handle
 * @param error Prediction error level [0-1]
 * @param precision Precision weighting [0-1]
 * @return Spike count on success, -1 on failure
 */
int predictive_snn_encode_error(
    predictive_snn_bridge_t* bridge,
    float error,
    float precision
);

/**
 * @brief Encode model state
 * @param bridge Bridge handle
 * @param model_state Internal model state [0-1]
 * @param hierarchy_level Hierarchical processing level
 * @return Spike count on success, -1 on failure
 */
int predictive_snn_encode_model_state(
    predictive_snn_bridge_t* bridge,
    float model_state,
    uint32_t hierarchy_level
);

/**
 * @brief Encode free energy
 * @param bridge Bridge handle
 * @param free_energy Free energy level [0-1]
 * @param energy_type Energy type classification
 * @return Spike count on success, -1 on failure
 */
int predictive_snn_encode_free_energy(
    predictive_snn_bridge_t* bridge,
    float free_energy,
    uint32_t energy_type
);

//=============================================================================
// Simulation Functions
//=============================================================================

/**
 * @brief Simulate predictive processing
 * @param bridge Bridge handle
 * @param duration_ms Simulation duration
 * @return 0 on success, -1 on failure
 */
int predictive_snn_simulate(predictive_snn_bridge_t* bridge, float duration_ms);

/**
 * @brief Single simulation step
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int predictive_snn_step(predictive_snn_bridge_t* bridge);

/**
 * @brief Forward pass through network
 * @param bridge Bridge handle
 * @param inputs Input values
 * @param input_count Input size
 * @return Spike count, -1 on failure
 */
int predictive_snn_forward(
    predictive_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
);

//=============================================================================
// Decoding Functions
//=============================================================================

/**
 * @brief Get anticipation from SNN activity
 * @param bridge Bridge handle
 * @param anticipation Output anticipation structure
 * @return 0 on success, -1 on failure
 */
int predictive_snn_get_anticipation(
    predictive_snn_bridge_t* bridge,
    predictive_anticipation_t* anticipation
);

/**
 * @brief Get dimension activations
 * @param bridge Bridge handle
 * @param activations Output buffer for activations
 * @param num_dims Number of dimensions to query
 * @return 0 on success, -1 on failure
 */
int predictive_snn_get_activations(
    predictive_snn_bridge_t* bridge,
    float* activations,
    uint32_t num_dims
);

/**
 * @brief Check for high error
 * @param bridge Bridge handle
 * @param error_level Output error level
 * @return true if high error detected, false otherwise
 */
bool predictive_snn_check_error(
    predictive_snn_bridge_t* bridge,
    float* error_level
);

/**
 * @brief Check for high anticipation
 * @param bridge Bridge handle
 * @param anticipation_level Output anticipation level
 * @return true if high anticipation detected, false otherwise
 */
bool predictive_snn_check_anticipation(
    predictive_snn_bridge_t* bridge,
    float* anticipation_level
);

/**
 * @brief Check for state change
 * @param bridge Bridge handle
 * @param change_magnitude Output change magnitude
 * @return true if state change detected, false otherwise
 */
bool predictive_snn_check_state_change(
    predictive_snn_bridge_t* bridge,
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
int predictive_snn_get_dim_state(
    predictive_snn_bridge_t* bridge,
    uint32_t dim,
    predictive_dim_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int predictive_snn_get_state(
    predictive_snn_bridge_t* bridge,
    predictive_snn_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int predictive_snn_get_stats(predictive_snn_bridge_t* bridge, predictive_snn_stats_t* stats);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int predictive_snn_reset_stats(predictive_snn_bridge_t* bridge);

/**
 * @brief Get current anticipation drive level
 * @param bridge Bridge handle
 * @return Anticipation drive [0-1], -1 on error
 */
float predictive_snn_get_anticipation_level(predictive_snn_bridge_t* bridge);

/**
 * @brief Get total network activity
 * @param bridge Bridge handle
 * @return Activity level, -1 on error
 */
float predictive_snn_get_total_activity(predictive_snn_bridge_t* bridge);

//=============================================================================
// Callback Registration
//=============================================================================

/**
 * @brief Register error detection callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int predictive_snn_register_error_callback(
    predictive_snn_bridge_t* bridge,
    predictive_snn_error_callback_t callback,
    void* user_data
);

/**
 * @brief Register anticipation callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int predictive_snn_register_anticipation_callback(
    predictive_snn_bridge_t* bridge,
    predictive_snn_anticipation_callback_t callback,
    void* user_data
);

/**
 * @brief Register high anticipation callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int predictive_snn_register_high_anticipation_callback(
    predictive_snn_bridge_t* bridge,
    predictive_snn_high_anticipation_callback_t callback,
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
int predictive_snn_bio_async_connect(predictive_snn_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int predictive_snn_bio_async_disconnect(predictive_snn_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool predictive_snn_is_bio_async_connected(predictive_snn_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PREDICTIVE_SNN_BRIDGE_H */
