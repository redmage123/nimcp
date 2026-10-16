/**
 * @file nimcp_tom_snn_bridge.h
 * @brief Theory of Mind - SNN Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between Theory of Mind engine and spiking neural networks
 * WHY:  Enable biologically-plausible social cognition processing through
 *       population coding and spike-timing dynamics
 * HOW:  Encode mental state attributions as spike patterns, decode social
 *       inferences from neural activity patterns
 *
 * THEORETICAL FOUNDATIONS:
 * - Premack & Woodruff (1978): Theory of Mind concept origin
 * - Baron-Cohen (1997): Mindblindness and social cognition
 * - Frith & Frith (2003): Neural basis of mentalizing
 * - Gallese & Goldman (1998): Mirror neurons and simulation theory
 *
 * BIOLOGICAL BASIS:
 * - Temporoparietal junction (TPJ) for perspective-taking
 * - Medial prefrontal cortex (mPFC) for mental state inference
 * - Superior temporal sulcus (STS) for biological motion and intention
 * - Anterior cingulate cortex (ACC) for empathic processing
 *
 * INTEGRATION WITH SOCIAL COGNITION:
 * - Belief state encoding via population coding
 * - Intention recognition through temporal spike patterns
 * - Perspective-taking as cross-population synchrony
 * - Empathic accuracy through resonance mechanisms
 *
 * @see nimcp_theory_of_mind.h
 * @see nimcp_snn_network.h
 */

#ifndef NIMCP_TOM_SNN_BRIDGE_H
#define NIMCP_TOM_SNN_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum Theory of Mind dimensions to encode */
#define TOM_SNN_MAX_DIMENSIONS          16

/** @brief Neurons per ToM dimension */
#define TOM_SNN_NEURONS_PER_DIM         32

/** @brief Default false belief detection threshold */
#define TOM_SNN_DECEPTION_THRESHOLD     0.5f

/** @brief Default encoding window (ms) */
#define TOM_SNN_ENCODING_WINDOW         50.0f

/** @brief Bio-async module ID */
#define BIO_MODULE_TOM_SNN              0x0D50

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Theory of Mind dimension types for SNN encoding
 */
typedef enum {
    TOM_DIM_BELIEF_STATE = 0,       /**< Mental state attribution */
    TOM_DIM_DESIRE_STATE,            /**< Goal/desire inference */
    TOM_DIM_INTENTION,               /**< Intention recognition */
    TOM_DIM_PERSPECTIVE,             /**< Perspective taking */
    TOM_DIM_EMOTION_INFERENCE,       /**< Inferring others' emotions */
    TOM_DIM_SOCIAL_CONTEXT,          /**< Social situation understanding */
    TOM_DIM_DECEPTION_DETECTION,     /**< Detecting false beliefs */
    TOM_DIM_SHARED_ATTENTION,        /**< Joint attention */
    TOM_DIM_EMPATHIC_ACCURACY,       /**< Accuracy of empathic inference */
    TOM_DIM_MENTAL_SIMULATION,       /**< Simulating others' mental states */
    TOM_DIM_COUNT
} tom_snn_dimension_t;

/**
 * @brief Encoding methods for social contexts
 */
typedef enum {
    TOM_SNN_ENCODE_RATE = 0,        /**< Rate coding of dimensions */
    TOM_SNN_ENCODE_TEMPORAL,         /**< Temporal spike patterns */
    TOM_SNN_ENCODE_POPULATION,       /**< Population vector coding */
    TOM_SNN_ENCODE_SYNCHRONY         /**< Synchrony-based encoding */
} tom_snn_encoding_t;

/**
 * @brief Decoding methods for social inferences
 */
typedef enum {
    TOM_SNN_DECODE_THRESHOLD = 0,   /**< Threshold-based detection */
    TOM_SNN_DECODE_COMPETITION,      /**< Winner-take-all */
    TOM_SNN_DECODE_SOFTMAX,          /**< Soft probabilistic */
    TOM_SNN_DECODE_INTEGRATION       /**< Evidence accumulation */
} tom_snn_decoding_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    TOM_SNN_STATE_IDLE = 0,
    TOM_SNN_STATE_ENCODING,
    TOM_SNN_STATE_PROCESSING,
    TOM_SNN_STATE_DECODING,
    TOM_SNN_STATE_SIMULATING,
    TOM_SNN_STATE_ERROR
} tom_snn_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief ToM-SNN bridge configuration
 */
typedef struct {
    /* Network dimensions */
    uint32_t num_dimensions;             /**< Number of ToM dimensions */
    uint32_t neurons_per_dim;            /**< Neurons per dimension */
    uint32_t hidden_dim;                 /**< Hidden layer dimension */

    /* Timing parameters */
    float dt_ms;                         /**< Simulation timestep (ms) */
    float encoding_window_ms;            /**< Encoding time window */
    float integration_tau_ms;            /**< Evidence integration time constant */

    /* Encoding parameters */
    tom_snn_encoding_t encoding;         /**< Encoding method */
    float encoding_gain;                 /**< Encoding strength gain */
    float baseline_rate_hz;              /**< Baseline firing rate */
    float max_rate_hz;                   /**< Maximum firing rate */

    /* Decoding parameters */
    tom_snn_decoding_t decoding;         /**< Decoding method */
    float inference_threshold;           /**< Threshold for inference */
    float deception_threshold;           /**< Threshold for deception detection */
    float confidence_threshold;          /**< Minimum confidence required */

    /* Network dynamics */
    bool enable_competition;             /**< Enable dimension competition */
    float inhibition_strength;           /**< Lateral inhibition weight */
    bool enable_deception_detection;     /**< Enable false belief detection */
    float deception_sensitivity;         /**< Deception detection sensitivity */

    /* Mental simulation */
    bool enable_mental_simulation;       /**< Enable mental simulation circuits */
    float simulation_gain;               /**< Mental simulation strength */
    bool enable_perspective_taking;      /**< Enable perspective-taking */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
    bool enable_plasticity_integration;  /**< Enable plasticity bridge */
} tom_snn_config_t;

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
} tom_dim_state_t;

/**
 * @brief Social inference output
 */
typedef struct {
    float belief_state;                  /**< Inferred belief state [0-1] */
    float desire_state;                  /**< Inferred desire/goal [0-1] */
    float intention_clarity;             /**< Intention inference clarity [0-1] */
    float perspective_alignment;         /**< Perspective alignment [0-1] */
    float empathic_accuracy;             /**< Empathic inference accuracy [0-1] */
    float social_context_match;          /**< Social context understanding [0-1] */
    bool deception_detected;             /**< False belief detected */
    float deception_confidence;          /**< Deception detection confidence */
    float shared_attention_strength;     /**< Joint attention strength */
    float mental_simulation_depth;       /**< Depth of mental simulation */
    float confidence;                    /**< Overall inference confidence */
} tom_inference_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    tom_snn_state_t state;               /**< Current operational state */
    uint32_t active_dimensions;          /**< Number of active dimensions */
    float total_activity;                /**< Total network activity */
    float mean_confidence;               /**< Mean inference confidence */
    float deception_signal;              /**< Current deception signal */
    float empathy_signal;                /**< Current empathy signal */
} tom_snn_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_evaluations;          /**< Total evaluations performed */
    uint64_t total_simulations;          /**< Total simulation steps */
    uint64_t total_spikes;               /**< Total spikes generated */
    uint64_t deception_detections;       /**< Deception detections */
    uint64_t perspective_switches;       /**< Perspective-taking events */
    uint64_t belief_updates;             /**< Belief state updates */
    float mean_inference_time_ms;        /**< Mean inference latency */
    float mean_confidence;               /**< Mean confidence score */
} tom_snn_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct tom_snn_bridge tom_snn_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Deception detection callback */
typedef void (*tom_snn_deception_callback_t)(
    tom_snn_bridge_t* bridge,
    float deception_level,
    uint64_t latency_us,
    void* user_data
);

/** @brief Inference ready callback */
typedef void (*tom_snn_inference_callback_t)(
    tom_snn_bridge_t* bridge,
    const tom_inference_t* inference,
    void* user_data
);

/** @brief Perspective switch callback */
typedef void (*tom_snn_perspective_callback_t)(
    tom_snn_bridge_t* bridge,
    float perspective_level,
    uint32_t perspective_dim,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
tom_snn_config_t tom_snn_config_default(void);

/**
 * @brief Create ToM SNN bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
tom_snn_bridge_t* tom_snn_create(const tom_snn_config_t* config);

/**
 * @brief Destroy ToM SNN bridge
 * @param bridge Bridge to destroy
 */
void tom_snn_destroy(tom_snn_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int tom_snn_reset(tom_snn_bridge_t* bridge);

//=============================================================================
// Encoding Functions
//=============================================================================

/**
 * @brief Encode social context into SNN
 * @param bridge Bridge handle
 * @param dimensions Dimension activation values [0-1]
 * @param num_dims Number of dimensions
 * @return Spike count on success, -1 on failure
 */
int tom_snn_encode_context(
    tom_snn_bridge_t* bridge,
    const float* dimensions,
    uint32_t num_dims
);

/**
 * @brief Encode belief state
 * @param bridge Bridge handle
 * @param self_belief Self's belief state [0-1]
 * @param other_belief Other's belief state [0-1]
 * @return Spike count on success, -1 on failure
 */
int tom_snn_encode_belief(
    tom_snn_bridge_t* bridge,
    float self_belief,
    float other_belief
);

/**
 * @brief Encode intention recognition
 * @param bridge Bridge handle
 * @param intention_strength Intention clarity [0-1]
 * @param goal_alignment Goal alignment level [0-1]
 * @param action_predictability Action predictability [0-1]
 * @return Spike count on success, -1 on failure
 */
int tom_snn_encode_intention(
    tom_snn_bridge_t* bridge,
    float intention_strength,
    float goal_alignment,
    float action_predictability
);

/**
 * @brief Encode empathic context
 * @param bridge Bridge handle
 * @param emotional_resonance Emotional mirroring strength [0-1]
 * @param cognitive_empathy Cognitive understanding [0-1]
 * @return Spike count on success, -1 on failure
 */
int tom_snn_encode_empathy(
    tom_snn_bridge_t* bridge,
    float emotional_resonance,
    float cognitive_empathy
);

//=============================================================================
// Simulation Functions
//=============================================================================

/**
 * @brief Simulate social cognition processing
 * @param bridge Bridge handle
 * @param duration_ms Simulation duration
 * @return 0 on success, -1 on failure
 */
int tom_snn_simulate(tom_snn_bridge_t* bridge, float duration_ms);

/**
 * @brief Single simulation step
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int tom_snn_step(tom_snn_bridge_t* bridge);

/**
 * @brief Forward pass through network
 * @param bridge Bridge handle
 * @param inputs Input values
 * @param input_count Input size
 * @return Spike count, -1 on failure
 */
int tom_snn_forward(
    tom_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
);

//=============================================================================
// Decoding Functions
//=============================================================================

/**
 * @brief Get social inference from SNN activity
 * @param bridge Bridge handle
 * @param inference Output inference structure
 * @return 0 on success, -1 on failure
 */
int tom_snn_get_inference(
    tom_snn_bridge_t* bridge,
    tom_inference_t* inference
);

/**
 * @brief Get dimension activations
 * @param bridge Bridge handle
 * @param activations Output buffer for activations
 * @param num_dims Number of dimensions to query
 * @return 0 on success, -1 on failure
 */
int tom_snn_get_activations(
    tom_snn_bridge_t* bridge,
    float* activations,
    uint32_t num_dims
);

/**
 * @brief Check for deception detection
 * @param bridge Bridge handle
 * @param deception_level Output deception level
 * @return true if deception detected, false otherwise
 */
bool tom_snn_check_deception(
    tom_snn_bridge_t* bridge,
    float* deception_level
);

/**
 * @brief Check for perspective shift
 * @param bridge Bridge handle
 * @param perspective_level Output perspective level
 * @return true if perspective shift detected, false otherwise
 */
bool tom_snn_check_perspective_shift(
    tom_snn_bridge_t* bridge,
    float* perspective_level
);

/**
 * @brief Check for empathic resonance
 * @param bridge Bridge handle
 * @param resonance_level Output resonance level
 * @return true if empathic resonance detected, false otherwise
 */
bool tom_snn_check_empathy(
    tom_snn_bridge_t* bridge,
    float* resonance_level
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
int tom_snn_get_dim_state(
    tom_snn_bridge_t* bridge,
    uint32_t dim,
    tom_dim_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int tom_snn_get_state(
    tom_snn_bridge_t* bridge,
    tom_snn_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int tom_snn_get_stats(tom_snn_bridge_t* bridge, tom_snn_stats_t* stats);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int tom_snn_reset_stats(tom_snn_bridge_t* bridge);

/**
 * @brief Get current inference confidence
 * @param bridge Bridge handle
 * @return Confidence level [0-1], -1 on error
 */
float tom_snn_get_confidence(tom_snn_bridge_t* bridge);

/**
 * @brief Get total network activity
 * @param bridge Bridge handle
 * @return Activity level, -1 on error
 */
float tom_snn_get_total_activity(tom_snn_bridge_t* bridge);

//=============================================================================
// Callback Registration
//=============================================================================

/**
 * @brief Register deception detection callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int tom_snn_register_deception_callback(
    tom_snn_bridge_t* bridge,
    tom_snn_deception_callback_t callback,
    void* user_data
);

/**
 * @brief Register inference callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int tom_snn_register_inference_callback(
    tom_snn_bridge_t* bridge,
    tom_snn_inference_callback_t callback,
    void* user_data
);

/**
 * @brief Register perspective switch callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int tom_snn_register_perspective_callback(
    tom_snn_bridge_t* bridge,
    tom_snn_perspective_callback_t callback,
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
int tom_snn_bio_async_connect(tom_snn_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int tom_snn_bio_async_disconnect(tom_snn_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool tom_snn_is_bio_async_connected(tom_snn_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TOM_SNN_BRIDGE_H */
