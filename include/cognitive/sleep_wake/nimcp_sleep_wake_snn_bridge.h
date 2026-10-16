/**
 * @file nimcp_sleep_wake_snn_bridge.h
 * @brief Sleep-Wake - SNN Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between sleep-wake system and spiking neural networks
 * WHY:  Enable biologically-plausible sleep stage encoding and arousal dynamics
 *       through population coding and spike-timing patterns
 * HOW:  Encode sleep stages as spike patterns, decode arousal and circadian
 *       signals from neural activity patterns
 *
 * THEORETICAL FOUNDATIONS:
 * - Borbely (1982): Two-process model of sleep regulation
 * - Steriade et al. (1993): Sleep oscillations and neural firing patterns
 * - Fuller et al. (2006): Circadian rhythm neural mechanisms
 *
 * BIOLOGICAL BASIS:
 * - Ventrolateral preoptic area (VLPO) for sleep promotion
 * - Locus coeruleus (LC) and tuberomammillary nucleus (TMN) for wake promotion
 * - Suprachiasmatic nucleus (SCN) for circadian rhythm
 * - Reticular activating system (RAS) for arousal
 *
 * INTEGRATION WITH LEARNING:
 * - Sleep stage encoding through population variability
 * - Arousal level detection via firing rate comparisons
 * - Circadian rhythm through sustained activity patterns
 *
 * @see nimcp_sleep_wake.h
 * @see nimcp_snn_network.h
 */

#ifndef NIMCP_SLEEP_WAKE_SNN_BRIDGE_H
#define NIMCP_SLEEP_WAKE_SNN_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum sleep-wake dimensions to encode */
#define SLEEP_WAKE_SNN_MAX_DIMENSIONS     16

/** @brief Neurons per sleep-wake dimension */
#define SLEEP_WAKE_SNN_NEURONS_PER_DIM    32

/** @brief Default arousal threshold for wake detection */
#define SLEEP_WAKE_SNN_AROUSAL_THRESH     0.5f

/** @brief Default encoding window (ms) */
#define SLEEP_WAKE_SNN_ENCODING_WINDOW    50.0f

/** @brief Bio-async module ID */
#define BIO_MODULE_SLEEP_WAKE_SNN         0x0D60

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Sleep-wake dimension types for SNN encoding
 */
typedef enum {
    SLEEP_WAKE_DIM_SLEEP_PRESSURE = 0,   /**< Adenosine/sleep drive level */
    SLEEP_WAKE_DIM_AROUSAL,               /**< Current arousal level */
    SLEEP_WAKE_DIM_CIRCADIAN_PHASE,       /**< Circadian rhythm phase */
    SLEEP_WAKE_DIM_WAKE_PROMOTION,        /**< Wake-promoting signal strength */
    SLEEP_WAKE_DIM_SLEEP_PROMOTION,       /**< Sleep-promoting signal strength */
    SLEEP_WAKE_DIM_N1_ACTIVITY,           /**< N1 (light sleep) activity */
    SLEEP_WAKE_DIM_N2_ACTIVITY,           /**< N2 (spindle sleep) activity */
    SLEEP_WAKE_DIM_N3_ACTIVITY,           /**< N3 (deep/slow wave) activity */
    SLEEP_WAKE_DIM_REM_ACTIVITY,          /**< REM sleep activity */
    SLEEP_WAKE_DIM_CONSOLIDATION,         /**< Memory consolidation signal */
    SLEEP_WAKE_DIM_COUNT
} sleep_wake_snn_dimension_t;

/**
 * @brief Encoding methods for sleep-wake contexts
 */
typedef enum {
    SLEEP_WAKE_SNN_ENCODE_RATE = 0,      /**< Rate coding of dimensions */
    SLEEP_WAKE_SNN_ENCODE_TEMPORAL,       /**< Temporal spike patterns */
    SLEEP_WAKE_SNN_ENCODE_POPULATION,     /**< Population vector coding */
    SLEEP_WAKE_SNN_ENCODE_OSCILLATION     /**< Oscillation-based encoding */
} sleep_wake_snn_encoding_t;

/**
 * @brief Decoding methods for arousal states
 */
typedef enum {
    SLEEP_WAKE_SNN_DECODE_THRESHOLD = 0, /**< Threshold-based detection */
    SLEEP_WAKE_SNN_DECODE_COMPETITION,    /**< Winner-take-all */
    SLEEP_WAKE_SNN_DECODE_SOFTMAX,        /**< Soft probabilistic */
    SLEEP_WAKE_SNN_DECODE_INTEGRATION     /**< Evidence accumulation */
} sleep_wake_snn_decoding_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    SLEEP_WAKE_SNN_STATE_IDLE = 0,
    SLEEP_WAKE_SNN_STATE_ENCODING,
    SLEEP_WAKE_SNN_STATE_PROCESSING,
    SLEEP_WAKE_SNN_STATE_DECODING,
    SLEEP_WAKE_SNN_STATE_SIMULATING,
    SLEEP_WAKE_SNN_STATE_ERROR
} sleep_wake_snn_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Sleep-Wake-SNN bridge configuration
 */
typedef struct {
    /* Network dimensions */
    uint32_t num_dimensions;             /**< Number of sleep-wake dimensions */
    uint32_t neurons_per_dim;            /**< Neurons per dimension */
    uint32_t hidden_dim;                 /**< Hidden layer dimension */

    /* Timing parameters */
    float dt_ms;                         /**< Simulation timestep (ms) */
    float encoding_window_ms;            /**< Encoding time window */
    float integration_tau_ms;            /**< Evidence integration time constant */

    /* Encoding parameters */
    sleep_wake_snn_encoding_t encoding;  /**< Encoding method */
    float encoding_gain;                 /**< Encoding strength gain */
    float baseline_rate_hz;              /**< Baseline firing rate */
    float max_rate_hz;                   /**< Maximum firing rate */

    /* Decoding parameters */
    sleep_wake_snn_decoding_t decoding;  /**< Decoding method */
    float arousal_threshold;             /**< Threshold for wake detection */
    float sleep_threshold;               /**< Threshold for sleep detection */
    float stage_change_threshold;        /**< Threshold for stage change */

    /* Network dynamics */
    bool enable_competition;             /**< Enable dimension competition */
    float inhibition_strength;           /**< Lateral inhibition weight */
    bool enable_oscillation_detection;   /**< Enable sleep oscillation detection */
    float oscillation_sensitivity;       /**< Oscillation detection sensitivity */

    /* Sleep stage integration */
    bool enable_stage_detection;         /**< Enable sleep stage circuits */
    float stage_detection_gain;          /**< Stage detection signal gain */
    bool enable_circadian_tracking;      /**< Enable circadian rhythm tracking */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
    bool enable_plasticity_integration;  /**< Enable plasticity bridge */
} sleep_wake_snn_config_t;

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
} sleep_wake_dim_state_t;

/**
 * @brief Arousal state output
 */
typedef struct {
    float sleep_pressure;                /**< Current sleep pressure [0-1] */
    float arousal_level;                 /**< Current arousal [0-1] */
    float circadian_phase;               /**< Circadian phase [0-1, 0=midnight] */
    float wake_drive;                    /**< Wake promotion strength */
    float sleep_drive;                   /**< Sleep promotion strength */
    bool high_arousal;                   /**< High arousal detected */
    bool sleep_onset;                    /**< Sleep onset detected */
    float stage_confidence;              /**< Confidence in current stage */
    float consolidation_signal;          /**< Memory consolidation signal */
    uint32_t detected_stage;             /**< Detected sleep stage (0-4) */
} sleep_wake_arousal_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    sleep_wake_snn_state_t state;        /**< Current operational state */
    uint32_t active_dimensions;          /**< Number of active dimensions */
    float total_activity;                /**< Total network activity */
    float mean_arousal;                  /**< Mean arousal level */
    float sleep_pressure_signal;         /**< Current sleep pressure signal */
    float circadian_signal;              /**< Current circadian signal */
} sleep_wake_snn_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_evaluations;          /**< Total evaluations performed */
    uint64_t total_simulations;          /**< Total simulation steps */
    uint64_t total_spikes;               /**< Total spikes generated */
    uint64_t sleep_onset_detections;     /**< Sleep onset detections */
    uint64_t wake_detections;            /**< Wake detections */
    uint64_t stage_transitions;          /**< Stage transitions detected */
    float mean_evaluation_time_ms;       /**< Mean evaluation latency */
    float mean_arousal;                  /**< Mean arousal level */
} sleep_wake_snn_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct sleep_wake_snn_bridge sleep_wake_snn_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Sleep onset detection callback */
typedef void (*sleep_wake_snn_sleep_callback_t)(
    sleep_wake_snn_bridge_t* bridge,
    float sleep_pressure,
    uint64_t latency_us,
    void* user_data
);

/** @brief Arousal state ready callback */
typedef void (*sleep_wake_snn_arousal_callback_t)(
    sleep_wake_snn_bridge_t* bridge,
    const sleep_wake_arousal_t* arousal,
    void* user_data
);

/** @brief Stage transition callback */
typedef void (*sleep_wake_snn_stage_callback_t)(
    sleep_wake_snn_bridge_t* bridge,
    uint32_t new_stage,
    uint32_t old_stage,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
sleep_wake_snn_config_t sleep_wake_snn_config_default(void);

/**
 * @brief Create sleep-wake SNN bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
sleep_wake_snn_bridge_t* sleep_wake_snn_create(const sleep_wake_snn_config_t* config);

/**
 * @brief Destroy sleep-wake SNN bridge
 * @param bridge Bridge to destroy
 */
void sleep_wake_snn_destroy(sleep_wake_snn_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int sleep_wake_snn_reset(sleep_wake_snn_bridge_t* bridge);

//=============================================================================
// Encoding Functions
//=============================================================================

/**
 * @brief Encode sleep-wake state into SNN
 * @param bridge Bridge handle
 * @param dimensions Dimension activation values [0-1]
 * @param num_dims Number of dimensions
 * @return Spike count on success, -1 on failure
 */
int sleep_wake_snn_encode_state(
    sleep_wake_snn_bridge_t* bridge,
    const float* dimensions,
    uint32_t num_dims
);

/**
 * @brief Encode sleep pressure level
 * @param bridge Bridge handle
 * @param pressure Sleep pressure level [0-1]
 * @param circadian_phase Circadian phase [0-1]
 * @return Spike count on success, -1 on failure
 */
int sleep_wake_snn_encode_pressure(
    sleep_wake_snn_bridge_t* bridge,
    float pressure,
    float circadian_phase
);

/**
 * @brief Encode arousal level
 * @param bridge Bridge handle
 * @param arousal Arousal level [0-1]
 * @param wake_drive Wake promotion signal
 * @return Spike count on success, -1 on failure
 */
int sleep_wake_snn_encode_arousal(
    sleep_wake_snn_bridge_t* bridge,
    float arousal,
    float wake_drive
);

/**
 * @brief Encode sleep stage
 * @param bridge Bridge handle
 * @param stage Sleep stage (0=awake, 1=N1, 2=N2, 3=N3, 4=REM)
 * @param stage_depth Depth within stage [0-1]
 * @return Spike count on success, -1 on failure
 */
int sleep_wake_snn_encode_stage(
    sleep_wake_snn_bridge_t* bridge,
    uint32_t stage,
    float stage_depth
);

//=============================================================================
// Simulation Functions
//=============================================================================

/**
 * @brief Simulate sleep-wake processing
 * @param bridge Bridge handle
 * @param duration_ms Simulation duration
 * @return 0 on success, -1 on failure
 */
int sleep_wake_snn_simulate(sleep_wake_snn_bridge_t* bridge, float duration_ms);

/**
 * @brief Single simulation step
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int sleep_wake_snn_step(sleep_wake_snn_bridge_t* bridge);

/**
 * @brief Forward pass through network
 * @param bridge Bridge handle
 * @param inputs Input values
 * @param input_count Input size
 * @return Spike count, -1 on failure
 */
int sleep_wake_snn_forward(
    sleep_wake_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
);

//=============================================================================
// Decoding Functions
//=============================================================================

/**
 * @brief Get arousal state from SNN activity
 * @param bridge Bridge handle
 * @param arousal Output arousal structure
 * @return 0 on success, -1 on failure
 */
int sleep_wake_snn_get_arousal(
    sleep_wake_snn_bridge_t* bridge,
    sleep_wake_arousal_t* arousal
);

/**
 * @brief Get dimension activations
 * @param bridge Bridge handle
 * @param activations Output buffer for activations
 * @param num_dims Number of dimensions to query
 * @return 0 on success, -1 on failure
 */
int sleep_wake_snn_get_activations(
    sleep_wake_snn_bridge_t* bridge,
    float* activations,
    uint32_t num_dims
);

/**
 * @brief Check for sleep onset
 * @param bridge Bridge handle
 * @param sleep_pressure Output sleep pressure level
 * @return true if sleep onset detected, false otherwise
 */
bool sleep_wake_snn_check_sleep_onset(
    sleep_wake_snn_bridge_t* bridge,
    float* sleep_pressure
);

/**
 * @brief Check for high arousal
 * @param bridge Bridge handle
 * @param arousal_level Output arousal level
 * @return true if high arousal detected, false otherwise
 */
bool sleep_wake_snn_check_high_arousal(
    sleep_wake_snn_bridge_t* bridge,
    float* arousal_level
);

/**
 * @brief Check for stage change
 * @param bridge Bridge handle
 * @param change_magnitude Output change magnitude
 * @return true if stage change detected, false otherwise
 */
bool sleep_wake_snn_check_stage_change(
    sleep_wake_snn_bridge_t* bridge,
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
int sleep_wake_snn_get_dim_state(
    sleep_wake_snn_bridge_t* bridge,
    uint32_t dim,
    sleep_wake_dim_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int sleep_wake_snn_get_state(
    sleep_wake_snn_bridge_t* bridge,
    sleep_wake_snn_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int sleep_wake_snn_get_stats(sleep_wake_snn_bridge_t* bridge, sleep_wake_snn_stats_t* stats);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int sleep_wake_snn_reset_stats(sleep_wake_snn_bridge_t* bridge);

/**
 * @brief Get current arousal level
 * @param bridge Bridge handle
 * @return Arousal level [0-1], -1 on error
 */
float sleep_wake_snn_get_arousal_level(sleep_wake_snn_bridge_t* bridge);

/**
 * @brief Get total network activity
 * @param bridge Bridge handle
 * @return Activity level, -1 on error
 */
float sleep_wake_snn_get_total_activity(sleep_wake_snn_bridge_t* bridge);

//=============================================================================
// Callback Registration
//=============================================================================

/**
 * @brief Register sleep onset detection callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int sleep_wake_snn_register_sleep_callback(
    sleep_wake_snn_bridge_t* bridge,
    sleep_wake_snn_sleep_callback_t callback,
    void* user_data
);

/**
 * @brief Register arousal callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int sleep_wake_snn_register_arousal_callback(
    sleep_wake_snn_bridge_t* bridge,
    sleep_wake_snn_arousal_callback_t callback,
    void* user_data
);

/**
 * @brief Register stage transition callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int sleep_wake_snn_register_stage_callback(
    sleep_wake_snn_bridge_t* bridge,
    sleep_wake_snn_stage_callback_t callback,
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
int sleep_wake_snn_bio_async_connect(sleep_wake_snn_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int sleep_wake_snn_bio_async_disconnect(sleep_wake_snn_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool sleep_wake_snn_is_bio_async_connected(sleep_wake_snn_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SLEEP_WAKE_SNN_BRIDGE_H */
