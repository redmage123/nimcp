/**
 * @file nimcp_rcog_snn_bridge.h
 * @brief Recursive Cognition - SNN Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between recursive cognition engine and spiking neural networks
 * WHY:  Enable biologically-plausible recursive processing through population coding
 *       and spike-timing dynamics for meta-cognitive operations
 * HOW:  Encode recursion depth, self-reference loops, and hierarchical processing
 *       as spike patterns; decode cognitive state from neural activity
 *
 * THEORETICAL FOUNDATIONS:
 * - Hofstadter (1979): Strange loops and recursive self-awareness
 * - Goel & Grafman (2000): Prefrontal cortex and meta-cognitive monitoring
 * - Fleming & Dolan (2012): Metacognition and the functional neuroanatomy
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal cortex for recursive planning and self-monitoring
 * - Anterior cingulate cortex for error monitoring and adjustment
 * - Parietal cortex for hierarchical representation
 * - Default mode network for self-referential processing
 *
 * INTEGRATION WITH RECURSIVE PROCESSING:
 * - Recursion depth tracking through population activity levels
 * - Self-reference detection via recurrent activity patterns
 * - Hierarchical decomposition through layer-wise processing
 * - Meta-cognitive state through firing rate modulation
 *
 * @see nimcp_rcog_engine.h
 * @see nimcp_snn_network.h
 */

#ifndef NIMCP_RCOG_SNN_BRIDGE_H
#define NIMCP_RCOG_SNN_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum recursive dimensions to encode */
#define RCOG_SNN_MAX_DIMENSIONS     16

/** @brief Neurons per recursive dimension */
#define RCOG_SNN_NEURONS_PER_DIM    32

/** @brief Default recursion depth threshold */
#define RCOG_SNN_DEPTH_THRESH       0.5f

/** @brief Default encoding window (ms) */
#define RCOG_SNN_ENCODING_WINDOW    50.0f

/** @brief Bio-async module ID */
#define BIO_MODULE_RCOG_SNN         0x0D60

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Recursive dimension types for SNN encoding
 */
typedef enum {
    RCOG_DIM_RECURSION_DEPTH = 0,        /**< Current recursion depth [0-1] */
    RCOG_DIM_META_COGNITIVE_LEVEL,        /**< Meta-cognitive awareness level */
    RCOG_DIM_SELF_REFERENCE,              /**< Self-reference loop intensity */
    RCOG_DIM_HIERARCHICAL_POSITION,       /**< Position in hierarchy [0-1] */
    RCOG_DIM_PROBLEM_COMPLEXITY,          /**< Problem complexity estimate */
    RCOG_DIM_DECOMPOSITION_PROGRESS,      /**< Decomposition completion [0-1] */
    RCOG_DIM_AGGREGATION_CONFIDENCE,      /**< Aggregation confidence level */
    RCOG_DIM_REFINEMENT_PROGRESS,         /**< Answer refinement progress */
    RCOG_DIM_WORKING_MEMORY_LOAD,         /**< Working memory utilization */
    RCOG_DIM_ATTENTION_FOCUS,             /**< Attention focus intensity */
    RCOG_DIM_COUNT
} rcog_snn_dimension_t;

/**
 * @brief Encoding methods for recursive contexts
 */
typedef enum {
    RCOG_SNN_ENCODE_RATE = 0,            /**< Rate coding of dimensions */
    RCOG_SNN_ENCODE_TEMPORAL,             /**< Temporal spike patterns */
    RCOG_SNN_ENCODE_POPULATION,           /**< Population vector coding */
    RCOG_SNN_ENCODE_HIERARCHICAL          /**< Hierarchical encoding */
} rcog_snn_encoding_t;

/**
 * @brief Decoding methods for cognitive states
 */
typedef enum {
    RCOG_SNN_DECODE_THRESHOLD = 0,       /**< Threshold-based detection */
    RCOG_SNN_DECODE_COMPETITION,          /**< Winner-take-all */
    RCOG_SNN_DECODE_SOFTMAX,              /**< Soft probabilistic */
    RCOG_SNN_DECODE_INTEGRATION           /**< Evidence accumulation */
} rcog_snn_decoding_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    RCOG_SNN_STATE_IDLE = 0,
    RCOG_SNN_STATE_ENCODING,
    RCOG_SNN_STATE_PROCESSING,
    RCOG_SNN_STATE_DECODING,
    RCOG_SNN_STATE_SIMULATING,
    RCOG_SNN_STATE_ERROR
} rcog_snn_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Recursive Cognition-SNN bridge configuration
 */
typedef struct {
    /* Network dimensions */
    uint32_t num_dimensions;             /**< Number of recursive dimensions */
    uint32_t neurons_per_dim;            /**< Neurons per dimension */
    uint32_t hidden_dim;                 /**< Hidden layer dimension */

    /* Timing parameters */
    float dt_ms;                         /**< Simulation timestep (ms) */
    float encoding_window_ms;            /**< Encoding time window */
    float integration_tau_ms;            /**< Evidence integration time constant */

    /* Encoding parameters */
    rcog_snn_encoding_t encoding;        /**< Encoding method */
    float encoding_gain;                 /**< Encoding strength gain */
    float baseline_rate_hz;              /**< Baseline firing rate */
    float max_rate_hz;                   /**< Maximum firing rate */

    /* Decoding parameters */
    rcog_snn_decoding_t decoding;        /**< Decoding method */
    float depth_threshold;               /**< Threshold for depth detection */
    float meta_cognitive_threshold;      /**< Minimum meta-cognitive level */
    float state_change_threshold;        /**< Threshold for state change */

    /* Network dynamics */
    bool enable_competition;             /**< Enable dimension competition */
    float inhibition_strength;           /**< Lateral inhibition weight */
    bool enable_self_reference_detection; /**< Enable self-reference detection */
    float self_reference_sensitivity;    /**< Self-reference detection sensitivity */

    /* Recursive processing integration */
    bool enable_depth_tracking;          /**< Enable recursion depth tracking */
    float depth_tracking_gain;           /**< Depth signal gain */
    bool enable_meta_cognitive;          /**< Enable meta-cognitive monitoring */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
    bool enable_plasticity_integration;  /**< Enable plasticity bridge */
} rcog_snn_config_t;

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
} rcog_dim_state_t;

/**
 * @brief Recursive cognitive state output
 */
typedef struct {
    float recursion_depth;               /**< Current recursion depth [0-1] */
    float meta_cognitive_level;          /**< Meta-cognitive awareness [0-1] */
    float self_reference_intensity;      /**< Self-reference loop strength */
    float hierarchical_position;         /**< Position in hierarchy [0-1] */
    float problem_complexity;            /**< Problem complexity [0-1] */
    bool deep_recursion_detected;        /**< Deep recursion detected */
    bool self_reference_loop;            /**< Self-reference loop detected */
    float decomposition_progress;        /**< Decomposition progress */
    float aggregation_confidence;        /**< Aggregation confidence */
    float refinement_progress;           /**< Refinement progress [0-1] */
} rcog_cognitive_state_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    rcog_snn_state_t state;              /**< Current operational state */
    uint32_t active_dimensions;          /**< Number of active dimensions */
    float total_activity;                /**< Total network activity */
    float mean_depth;                    /**< Mean recursion depth */
    float meta_cognitive_signal;         /**< Current meta-cognitive signal */
    float self_reference_signal;         /**< Current self-reference signal */
} rcog_snn_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_evaluations;          /**< Total evaluations performed */
    uint64_t total_simulations;          /**< Total simulation steps */
    uint64_t total_spikes;               /**< Total spikes generated */
    uint64_t deep_recursion_events;      /**< Deep recursion detections */
    uint64_t self_reference_events;      /**< Self-reference loop detections */
    uint64_t state_changes;              /**< State changes detected */
    float mean_evaluation_time_ms;       /**< Mean evaluation latency */
    float mean_depth;                    /**< Mean recursion depth */
} rcog_snn_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct rcog_snn_bridge rcog_snn_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Deep recursion detection callback */
typedef void (*rcog_snn_depth_callback_t)(
    rcog_snn_bridge_t* bridge,
    float depth_level,
    uint64_t latency_us,
    void* user_data
);

/** @brief Cognitive state ready callback */
typedef void (*rcog_snn_state_callback_t)(
    rcog_snn_bridge_t* bridge,
    const rcog_cognitive_state_t* state,
    void* user_data
);

/** @brief Self-reference loop callback */
typedef void (*rcog_snn_self_ref_callback_t)(
    rcog_snn_bridge_t* bridge,
    float intensity,
    uint32_t loop_depth,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
rcog_snn_config_t rcog_snn_config_default(void);

/**
 * @brief Create recursive cognition SNN bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
rcog_snn_bridge_t* rcog_snn_create(const rcog_snn_config_t* config);

/**
 * @brief Destroy recursive cognition SNN bridge
 * @param bridge Bridge to destroy
 */
void rcog_snn_destroy(rcog_snn_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int rcog_snn_reset(rcog_snn_bridge_t* bridge);

//=============================================================================
// Encoding Functions
//=============================================================================

/**
 * @brief Encode recursive state into SNN
 * @param bridge Bridge handle
 * @param dimensions Dimension activation values [0-1]
 * @param num_dims Number of dimensions
 * @return Spike count on success, -1 on failure
 */
int rcog_snn_encode_state(
    rcog_snn_bridge_t* bridge,
    const float* dimensions,
    uint32_t num_dims
);

/**
 * @brief Encode recursion depth
 * @param bridge Bridge handle
 * @param depth Current depth [0-1] (normalized)
 * @param max_depth Maximum depth for normalization
 * @return Spike count on success, -1 on failure
 */
int rcog_snn_encode_depth(
    rcog_snn_bridge_t* bridge,
    float depth,
    uint32_t max_depth
);

/**
 * @brief Encode meta-cognitive state
 * @param bridge Bridge handle
 * @param awareness Meta-cognitive awareness level [0-1]
 * @param confidence Self-confidence [0-1]
 * @return Spike count on success, -1 on failure
 */
int rcog_snn_encode_meta_cognitive(
    rcog_snn_bridge_t* bridge,
    float awareness,
    float confidence
);

/**
 * @brief Encode self-reference loop
 * @param bridge Bridge handle
 * @param intensity Loop intensity [0-1]
 * @param loop_depth Depth of self-reference
 * @return Spike count on success, -1 on failure
 */
int rcog_snn_encode_self_reference(
    rcog_snn_bridge_t* bridge,
    float intensity,
    uint32_t loop_depth
);

/**
 * @brief Encode hierarchical position
 * @param bridge Bridge handle
 * @param level Current level in hierarchy
 * @param total_levels Total hierarchy levels
 * @return Spike count on success, -1 on failure
 */
int rcog_snn_encode_hierarchy(
    rcog_snn_bridge_t* bridge,
    uint32_t level,
    uint32_t total_levels
);

//=============================================================================
// Simulation Functions
//=============================================================================

/**
 * @brief Simulate recursive processing
 * @param bridge Bridge handle
 * @param duration_ms Simulation duration
 * @return 0 on success, -1 on failure
 */
int rcog_snn_simulate(rcog_snn_bridge_t* bridge, float duration_ms);

/**
 * @brief Single simulation step
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int rcog_snn_step(rcog_snn_bridge_t* bridge);

/**
 * @brief Forward pass through network
 * @param bridge Bridge handle
 * @param inputs Input values
 * @param input_count Input size
 * @return Spike count, -1 on failure
 */
int rcog_snn_forward(
    rcog_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
);

//=============================================================================
// Decoding Functions
//=============================================================================

/**
 * @brief Get cognitive state from SNN activity
 * @param bridge Bridge handle
 * @param state Output cognitive state structure
 * @return 0 on success, -1 on failure
 */
int rcog_snn_get_cognitive_state(
    rcog_snn_bridge_t* bridge,
    rcog_cognitive_state_t* state
);

/**
 * @brief Get dimension activations
 * @param bridge Bridge handle
 * @param activations Output buffer for activations
 * @param num_dims Number of dimensions to query
 * @return 0 on success, -1 on failure
 */
int rcog_snn_get_activations(
    rcog_snn_bridge_t* bridge,
    float* activations,
    uint32_t num_dims
);

/**
 * @brief Check for deep recursion
 * @param bridge Bridge handle
 * @param depth_level Output depth level
 * @return true if deep recursion detected, false otherwise
 */
bool rcog_snn_check_deep_recursion(
    rcog_snn_bridge_t* bridge,
    float* depth_level
);

/**
 * @brief Check for self-reference loop
 * @param bridge Bridge handle
 * @param intensity Output intensity level
 * @return true if self-reference detected, false otherwise
 */
bool rcog_snn_check_self_reference(
    rcog_snn_bridge_t* bridge,
    float* intensity
);

/**
 * @brief Check for state change
 * @param bridge Bridge handle
 * @param change_magnitude Output change magnitude
 * @return true if state change detected, false otherwise
 */
bool rcog_snn_check_state_change(
    rcog_snn_bridge_t* bridge,
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
int rcog_snn_get_dim_state(
    rcog_snn_bridge_t* bridge,
    uint32_t dim,
    rcog_dim_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int rcog_snn_get_state(
    rcog_snn_bridge_t* bridge,
    rcog_snn_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int rcog_snn_get_stats(rcog_snn_bridge_t* bridge, rcog_snn_stats_t* stats);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int rcog_snn_reset_stats(rcog_snn_bridge_t* bridge);

/**
 * @brief Get current recursion depth level
 * @param bridge Bridge handle
 * @return Depth level [0-1], -1 on error
 */
float rcog_snn_get_depth(rcog_snn_bridge_t* bridge);

/**
 * @brief Get total network activity
 * @param bridge Bridge handle
 * @return Activity level, -1 on error
 */
float rcog_snn_get_total_activity(rcog_snn_bridge_t* bridge);

//=============================================================================
// Callback Registration
//=============================================================================

/**
 * @brief Register deep recursion callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int rcog_snn_register_depth_callback(
    rcog_snn_bridge_t* bridge,
    rcog_snn_depth_callback_t callback,
    void* user_data
);

/**
 * @brief Register cognitive state callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int rcog_snn_register_state_callback(
    rcog_snn_bridge_t* bridge,
    rcog_snn_state_callback_t callback,
    void* user_data
);

/**
 * @brief Register self-reference callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int rcog_snn_register_self_ref_callback(
    rcog_snn_bridge_t* bridge,
    rcog_snn_self_ref_callback_t callback,
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
int rcog_snn_bio_async_connect(rcog_snn_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int rcog_snn_bio_async_disconnect(rcog_snn_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool rcog_snn_is_bio_async_connected(rcog_snn_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RCOG_SNN_BRIDGE_H */
