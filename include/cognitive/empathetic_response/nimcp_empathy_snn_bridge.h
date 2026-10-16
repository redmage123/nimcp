/**
 * @file nimcp_empathy_snn_bridge.h
 * @brief Empathy - SNN Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between empathy engine and spiking neural networks
 * WHY:  Enable biologically-plausible empathetic processing through
 *       population coding and spike-timing dynamics
 * HOW:  Encode empathy dimensions as spike patterns, decode emotional
 *       response signals from neural activity patterns
 *
 * THEORETICAL FOUNDATIONS:
 * - Preston & de Waal (2002): Perception-action model of empathy
 * - Decety & Jackson (2004): Neural mechanisms of empathy
 * - Singer & Lamm (2009): Social neuroscience of empathy
 *
 * BIOLOGICAL BASIS:
 * - Mirror neuron system for emotional mirroring
 * - Anterior insula for interoception and empathic concern
 * - Anterior cingulate cortex for empathic distress
 * - Medial prefrontal cortex for perspective-taking
 * - Temporoparietal junction for self-other distinction
 *
 * INTEGRATION WITH LEARNING:
 * - Affective sharing through synchronized activity
 * - Perspective-taking via population coding
 * - Compassion response through sustained activation
 *
 * @see nimcp_empathetic_response.h
 * @see nimcp_snn_network.h
 */

#ifndef NIMCP_EMPATHY_SNN_BRIDGE_H
#define NIMCP_EMPATHY_SNN_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum empathy dimensions to encode */
#define EMPATHY_SNN_MAX_DIMENSIONS     16

/** @brief Neurons per empathy dimension */
#define EMPATHY_SNN_NEURONS_PER_DIM    32

/** @brief Default compassion threshold */
#define EMPATHY_SNN_COMPASSION_THRESH  0.5f

/** @brief Default encoding window (ms) */
#define EMPATHY_SNN_ENCODING_WINDOW    50.0f

/** @brief Bio-async module ID */
#define BIO_MODULE_EMPATHY_SNN         0x0D60

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Empathy dimension types for SNN encoding
 */
typedef enum {
    EMPATHY_DIM_EMOTIONAL_MIRRORING = 0, /**< Emotional mirroring level */
    EMPATHY_DIM_PERSPECTIVE_TAKING,       /**< Perspective-taking capacity */
    EMPATHY_DIM_AFFECTIVE_SHARING,        /**< Affective sharing state */
    EMPATHY_DIM_EMPATHIC_CONCERN,         /**< Empathic concern level */
    EMPATHY_DIM_COMPASSION,               /**< Compassion response strength */
    EMPATHY_DIM_SELF_OTHER_DISTINCTION,   /**< Self-other boundary clarity */
    EMPATHY_DIM_EMOTIONAL_REGULATION,     /**< Emotion regulation capacity */
    EMPATHY_DIM_DISTRESS_TOLERANCE,       /**< Distress tolerance level */
    EMPATHY_DIM_PROSOCIAL_MOTIVATION,     /**< Prosocial motivation strength */
    EMPATHY_DIM_VALIDATION_RESPONSE,      /**< Validation response readiness */
    EMPATHY_DIM_COUNT
} empathy_snn_dimension_t;

/**
 * @brief Encoding methods for empathy contexts
 */
typedef enum {
    EMPATHY_SNN_ENCODE_RATE = 0,         /**< Rate coding of dimensions */
    EMPATHY_SNN_ENCODE_TEMPORAL,          /**< Temporal spike patterns */
    EMPATHY_SNN_ENCODE_POPULATION,        /**< Population vector coding */
    EMPATHY_SNN_ENCODE_SYNCHRONY          /**< Synchrony-based encoding */
} empathy_snn_encoding_t;

/**
 * @brief Decoding methods for empathy states
 */
typedef enum {
    EMPATHY_SNN_DECODE_THRESHOLD = 0,    /**< Threshold-based detection */
    EMPATHY_SNN_DECODE_COMPETITION,       /**< Winner-take-all */
    EMPATHY_SNN_DECODE_SOFTMAX,           /**< Soft probabilistic */
    EMPATHY_SNN_DECODE_INTEGRATION        /**< Evidence accumulation */
} empathy_snn_decoding_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    EMPATHY_SNN_STATE_IDLE = 0,
    EMPATHY_SNN_STATE_ENCODING,
    EMPATHY_SNN_STATE_PROCESSING,
    EMPATHY_SNN_STATE_DECODING,
    EMPATHY_SNN_STATE_SIMULATING,
    EMPATHY_SNN_STATE_ERROR
} empathy_snn_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Empathy-SNN bridge configuration
 */
typedef struct {
    /* Network dimensions */
    uint32_t num_dimensions;             /**< Number of empathy dimensions */
    uint32_t neurons_per_dim;            /**< Neurons per dimension */
    uint32_t hidden_dim;                 /**< Hidden layer dimension */

    /* Timing parameters */
    float dt_ms;                         /**< Simulation timestep (ms) */
    float encoding_window_ms;            /**< Encoding time window */
    float integration_tau_ms;            /**< Evidence integration time constant */

    /* Encoding parameters */
    empathy_snn_encoding_t encoding;     /**< Encoding method */
    float encoding_gain;                 /**< Encoding strength gain */
    float baseline_rate_hz;              /**< Baseline firing rate */
    float max_rate_hz;                   /**< Maximum firing rate */

    /* Decoding parameters */
    empathy_snn_decoding_t decoding;     /**< Decoding method */
    float compassion_threshold;          /**< Threshold for compassion activation */
    float empathic_concern_threshold;    /**< Minimum empathic concern level */
    float state_change_threshold;        /**< Threshold for state change */

    /* Network dynamics */
    bool enable_competition;             /**< Enable dimension competition */
    float inhibition_strength;           /**< Lateral inhibition weight */
    bool enable_mirroring_detection;     /**< Enable emotional mirroring detection */
    float mirroring_sensitivity;         /**< Mirroring detection sensitivity */

    /* Compassion integration */
    bool enable_compassion;              /**< Enable compassion circuits */
    float compassion_gain;               /**< Compassion signal gain */
    bool enable_distress_modulation;     /**< Enable distress modulation */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
    bool enable_plasticity_integration;  /**< Enable plasticity bridge */
} empathy_snn_config_t;

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
} empathy_dim_state_t;

/**
 * @brief Empathic response output
 */
typedef struct {
    float mirroring_level;               /**< Current emotional mirroring [0-1] */
    float perspective_taking;            /**< Perspective-taking level [0-1] */
    float affective_sharing;             /**< Affective sharing state [0-1] */
    float empathic_concern;              /**< Empathic concern level [0-1] */
    float compassion_response;           /**< Compassion response strength */
    bool high_empathy_detected;          /**< High empathy state detected */
    bool compassion_activated;           /**< Compassion response triggered */
    float distress_tolerance;            /**< Current distress tolerance */
    float prosocial_motivation;          /**< Prosocial motivation level */
    float validation_readiness;          /**< Readiness to validate emotions */
} empathy_response_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    empathy_snn_state_t state;           /**< Current operational state */
    uint32_t active_dimensions;          /**< Number of active dimensions */
    float total_activity;                /**< Total network activity */
    float mean_empathy;                  /**< Mean empathy level */
    float mirroring_signal;              /**< Current mirroring signal */
    float compassion_signal;             /**< Current compassion signal */
} empathy_snn_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_evaluations;          /**< Total evaluations performed */
    uint64_t total_simulations;          /**< Total simulation steps */
    uint64_t total_spikes;               /**< Total spikes generated */
    uint64_t high_empathy_events;        /**< High empathy detections */
    uint64_t compassion_activations;     /**< Compassion activations */
    uint64_t state_changes;              /**< State changes detected */
    float mean_evaluation_time_ms;       /**< Mean evaluation latency */
    float mean_empathy_level;            /**< Mean empathy level */
} empathy_snn_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct empathy_snn_bridge empathy_snn_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Mirroring detection callback */
typedef void (*empathy_snn_mirroring_callback_t)(
    empathy_snn_bridge_t* bridge,
    float mirroring_level,
    uint64_t latency_us,
    void* user_data
);

/** @brief Empathic response ready callback */
typedef void (*empathy_snn_response_callback_t)(
    empathy_snn_bridge_t* bridge,
    const empathy_response_t* response,
    void* user_data
);

/** @brief Compassion activation callback */
typedef void (*empathy_snn_compassion_callback_t)(
    empathy_snn_bridge_t* bridge,
    float compassion_level,
    uint32_t compassion_dim,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
empathy_snn_config_t empathy_snn_config_default(void);

/**
 * @brief Create empathy SNN bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
empathy_snn_bridge_t* empathy_snn_create(const empathy_snn_config_t* config);

/**
 * @brief Destroy empathy SNN bridge
 * @param bridge Bridge to destroy
 */
void empathy_snn_destroy(empathy_snn_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int empathy_snn_reset(empathy_snn_bridge_t* bridge);

//=============================================================================
// Encoding Functions
//=============================================================================

/**
 * @brief Encode empathy state into SNN
 * @param bridge Bridge handle
 * @param dimensions Dimension activation values [0-1]
 * @param num_dims Number of dimensions
 * @return Spike count on success, -1 on failure
 */
int empathy_snn_encode_state(
    empathy_snn_bridge_t* bridge,
    const float* dimensions,
    uint32_t num_dims
);

/**
 * @brief Encode emotional mirroring level
 * @param bridge Bridge handle
 * @param mirroring Emotional mirroring level [0-1]
 * @param target_emotion Target emotion intensity [0-1]
 * @return Spike count on success, -1 on failure
 */
int empathy_snn_encode_mirroring(
    empathy_snn_bridge_t* bridge,
    float mirroring,
    float target_emotion
);

/**
 * @brief Encode perspective-taking state
 * @param bridge Bridge handle
 * @param perspective Perspective-taking level [0-1]
 * @param self_other_clarity Self-other distinction clarity [0-1]
 * @return Spike count on success, -1 on failure
 */
int empathy_snn_encode_perspective(
    empathy_snn_bridge_t* bridge,
    float perspective,
    float self_other_clarity
);

/**
 * @brief Encode compassion response
 * @param bridge Bridge handle
 * @param compassion Compassion level [0-1]
 * @param empathic_concern Empathic concern level [0-1]
 * @return Spike count on success, -1 on failure
 */
int empathy_snn_encode_compassion(
    empathy_snn_bridge_t* bridge,
    float compassion,
    float empathic_concern
);

//=============================================================================
// Simulation Functions
//=============================================================================

/**
 * @brief Simulate empathy processing
 * @param bridge Bridge handle
 * @param duration_ms Simulation duration
 * @return 0 on success, -1 on failure
 */
int empathy_snn_simulate(empathy_snn_bridge_t* bridge, float duration_ms);

/**
 * @brief Single simulation step
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int empathy_snn_step(empathy_snn_bridge_t* bridge);

/**
 * @brief Forward pass through network
 * @param bridge Bridge handle
 * @param inputs Input values
 * @param input_count Input size
 * @return Spike count, -1 on failure
 */
int empathy_snn_forward(
    empathy_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
);

//=============================================================================
// Decoding Functions
//=============================================================================

/**
 * @brief Get empathic response from SNN activity
 * @param bridge Bridge handle
 * @param response Output response structure
 * @return 0 on success, -1 on failure
 */
int empathy_snn_get_response(
    empathy_snn_bridge_t* bridge,
    empathy_response_t* response
);

/**
 * @brief Get dimension activations
 * @param bridge Bridge handle
 * @param activations Output buffer for activations
 * @param num_dims Number of dimensions to query
 * @return 0 on success, -1 on failure
 */
int empathy_snn_get_activations(
    empathy_snn_bridge_t* bridge,
    float* activations,
    uint32_t num_dims
);

/**
 * @brief Check for high empathy state
 * @param bridge Bridge handle
 * @param empathy_level Output empathy level
 * @return true if high empathy detected, false otherwise
 */
bool empathy_snn_check_empathy(
    empathy_snn_bridge_t* bridge,
    float* empathy_level
);

/**
 * @brief Check for compassion activation
 * @param bridge Bridge handle
 * @param compassion_level Output compassion level
 * @return true if compassion activated, false otherwise
 */
bool empathy_snn_check_compassion(
    empathy_snn_bridge_t* bridge,
    float* compassion_level
);

/**
 * @brief Check for state change
 * @param bridge Bridge handle
 * @param change_magnitude Output change magnitude
 * @return true if state change detected, false otherwise
 */
bool empathy_snn_check_state_change(
    empathy_snn_bridge_t* bridge,
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
int empathy_snn_get_dim_state(
    empathy_snn_bridge_t* bridge,
    uint32_t dim,
    empathy_dim_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int empathy_snn_get_state(
    empathy_snn_bridge_t* bridge,
    empathy_snn_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int empathy_snn_get_stats(empathy_snn_bridge_t* bridge, empathy_snn_stats_t* stats);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int empathy_snn_reset_stats(empathy_snn_bridge_t* bridge);

/**
 * @brief Get current empathic concern level
 * @param bridge Bridge handle
 * @return Empathic concern [0-1], -1 on error
 */
float empathy_snn_get_empathic_concern(empathy_snn_bridge_t* bridge);

/**
 * @brief Get total network activity
 * @param bridge Bridge handle
 * @return Activity level, -1 on error
 */
float empathy_snn_get_total_activity(empathy_snn_bridge_t* bridge);

//=============================================================================
// Callback Registration
//=============================================================================

/**
 * @brief Register mirroring detection callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int empathy_snn_register_mirroring_callback(
    empathy_snn_bridge_t* bridge,
    empathy_snn_mirroring_callback_t callback,
    void* user_data
);

/**
 * @brief Register empathic response callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int empathy_snn_register_response_callback(
    empathy_snn_bridge_t* bridge,
    empathy_snn_response_callback_t callback,
    void* user_data
);

/**
 * @brief Register compassion activation callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int empathy_snn_register_compassion_callback(
    empathy_snn_bridge_t* bridge,
    empathy_snn_compassion_callback_t callback,
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
int empathy_snn_bio_async_connect(empathy_snn_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int empathy_snn_bio_async_disconnect(empathy_snn_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool empathy_snn_is_bio_async_connected(empathy_snn_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EMPATHY_SNN_BRIDGE_H */
