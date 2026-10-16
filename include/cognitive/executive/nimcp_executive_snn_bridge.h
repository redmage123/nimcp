/**
 * @file nimcp_executive_snn_bridge.h
 * @brief Executive Function - SNN Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between executive functions and spiking neural networks
 * WHY:  Enable biologically-plausible executive control through population coding
 *       and spike-timing dynamics for cognitive control operations
 * HOW:  Encode executive states as spike patterns, decode control signals
 *       from neural activity patterns
 *
 * THEORETICAL FOUNDATIONS:
 * - Miller & Cohen (2001): Prefrontal Cortex and Cognitive Control
 * - Botvinick et al. (2001): Conflict Monitoring and Cognitive Control
 * - Miyake et al. (2000): Unity and Diversity of Executive Functions
 *
 * BIOLOGICAL BASIS:
 * - Dorsolateral prefrontal cortex (dlPFC) for working memory and planning
 * - Anterior cingulate cortex (ACC) for conflict monitoring and error detection
 * - Orbitofrontal cortex (OFC) for flexible adaptation and goal maintenance
 * - Basal ganglia for action selection and response inhibition
 *
 * INTEGRATION WITH COGNITIVE CONTROL:
 * - Response inhibition through lateral inhibition circuits
 * - Task switching through attractor dynamics
 * - Conflict monitoring via ACC-like activity patterns
 * - Goal maintenance through persistent neural activity
 *
 * @see nimcp_executive.h
 * @see nimcp_snn_network.h
 */

#ifndef NIMCP_EXECUTIVE_SNN_BRIDGE_H
#define NIMCP_EXECUTIVE_SNN_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum executive dimensions to encode */
#define EXECUTIVE_SNN_MAX_DIMENSIONS     16

/** @brief Neurons per executive dimension */
#define EXECUTIVE_SNN_NEURONS_PER_DIM    32

/** @brief Default conflict threshold */
#define EXECUTIVE_SNN_CONFLICT_THRESH    0.5f

/** @brief Default encoding window (ms) */
#define EXECUTIVE_SNN_ENCODING_WINDOW    50.0f

/** @brief Bio-async module ID */
#define BIO_MODULE_EXECUTIVE_SNN         0x0E40

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Executive dimension types for SNN encoding
 */
typedef enum {
    EXEC_DIM_INHIBITION = 0,       /**< Response inhibition strength */
    EXEC_DIM_WORKING_MEMORY,       /**< Working memory load */
    EXEC_DIM_FLEXIBILITY,          /**< Cognitive flexibility */
    EXEC_DIM_PLANNING,             /**< Planning/sequencing activity */
    EXEC_DIM_TASK_SWITCHING,       /**< Task switching cost */
    EXEC_DIM_ATTENTION_CONTROL,    /**< Attentional control */
    EXEC_DIM_GOAL_MAINTENANCE,     /**< Goal maintenance strength */
    EXEC_DIM_CONFLICT_MONITOR,     /**< Conflict monitoring signal */
    EXEC_DIM_ERROR_CORRECTION,     /**< Error correction activity */
    EXEC_DIM_RESOURCE_ALLOCATION,  /**< Resource allocation level */
    EXEC_DIM_COUNT
} executive_snn_dimension_t;

/**
 * @brief Encoding methods for executive states
 */
typedef enum {
    EXECUTIVE_SNN_ENCODE_RATE = 0,    /**< Rate coding of dimensions */
    EXECUTIVE_SNN_ENCODE_TEMPORAL,     /**< Temporal spike patterns */
    EXECUTIVE_SNN_ENCODE_POPULATION,   /**< Population vector coding */
    EXECUTIVE_SNN_ENCODE_SYNCHRONY     /**< Synchrony-based encoding */
} executive_snn_encoding_t;

/**
 * @brief Decoding methods for control signals
 */
typedef enum {
    EXECUTIVE_SNN_DECODE_THRESHOLD = 0, /**< Threshold-based detection */
    EXECUTIVE_SNN_DECODE_COMPETITION,    /**< Winner-take-all */
    EXECUTIVE_SNN_DECODE_SOFTMAX,        /**< Soft probabilistic */
    EXECUTIVE_SNN_DECODE_INTEGRATION     /**< Evidence accumulation */
} executive_snn_decoding_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    EXECUTIVE_SNN_STATE_IDLE = 0,
    EXECUTIVE_SNN_STATE_ENCODING,
    EXECUTIVE_SNN_STATE_PROCESSING,
    EXECUTIVE_SNN_STATE_DECODING,
    EXECUTIVE_SNN_STATE_SIMULATING,
    EXECUTIVE_SNN_STATE_ERROR
} executive_snn_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Executive-SNN bridge configuration
 */
typedef struct {
    /* Network dimensions */
    uint32_t num_dimensions;             /**< Number of executive dimensions */
    uint32_t neurons_per_dim;            /**< Neurons per dimension */
    uint32_t hidden_dim;                 /**< Hidden layer dimension */

    /* Timing parameters */
    float dt_ms;                         /**< Simulation timestep (ms) */
    float encoding_window_ms;            /**< Encoding time window */
    float integration_tau_ms;            /**< Evidence integration time constant */

    /* Encoding parameters */
    executive_snn_encoding_t encoding;   /**< Encoding method */
    float encoding_gain;                 /**< Encoding strength gain */
    float baseline_rate_hz;              /**< Baseline firing rate */
    float max_rate_hz;                   /**< Maximum firing rate */

    /* Decoding parameters */
    executive_snn_decoding_t decoding;   /**< Decoding method */
    float conflict_threshold;            /**< Threshold for conflict detection */
    float inhibition_threshold;          /**< Minimum inhibition required */
    float goal_change_threshold;         /**< Threshold for goal state change */

    /* Network dynamics */
    bool enable_competition;             /**< Enable dimension competition */
    float inhibition_strength;           /**< Lateral inhibition weight */
    bool enable_conflict_detection;      /**< Enable conflict signal detection */
    float error_threshold;               /**< Error detection threshold */

    /* Executive control integration */
    bool enable_goal_maintenance;        /**< Enable goal maintenance circuits */
    float goal_persistence_gain;         /**< Goal persistence signal gain */
    bool enable_task_switching;          /**< Enable task switching detection */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
    bool enable_plasticity_integration;  /**< Enable plasticity bridge */
} executive_snn_config_t;

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
} executive_dim_state_t;

/**
 * @brief Executive control output
 */
typedef struct {
    float inhibition_level;              /**< Current inhibition [0-1] */
    float working_memory_load;           /**< Working memory load [0-1] */
    float flexibility_level;             /**< Cognitive flexibility [0-1] */
    float planning_activity;             /**< Planning activity level */
    float task_switching_cost;           /**< Task switching cost [0-1] */
    float attention_control;             /**< Attentional control level */
    bool goal_change_detected;           /**< Goal state change detected */
    bool conflict_detected;              /**< Conflict signal detected */
    float conflict_magnitude;            /**< Conflict magnitude if detected */
    float goal_strength;                 /**< Goal maintenance strength */
    float resource_allocation;           /**< Resource allocation level */
} executive_control_output_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    executive_snn_state_t state;         /**< Current operational state */
    uint32_t active_dimensions;          /**< Number of active dimensions */
    float total_activity;                /**< Total network activity */
    float mean_inhibition;               /**< Mean inhibition level */
    float conflict_signal;               /**< Current conflict signal */
    float error_signal;                  /**< Current error signal */
} executive_snn_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_evaluations;          /**< Total evaluations performed */
    uint64_t total_simulations;          /**< Total simulation steps */
    uint64_t total_spikes;               /**< Total spikes generated */
    uint64_t conflict_detections;        /**< Conflict signal detections */
    uint64_t error_detections;           /**< Error signal detections */
    uint64_t goal_changes;               /**< Goal state changes detected */
    float mean_evaluation_time_ms;       /**< Mean evaluation latency */
    float mean_inhibition;               /**< Mean inhibition score */
} executive_snn_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct executive_snn_bridge executive_snn_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Conflict detection callback */
typedef void (*executive_snn_conflict_callback_t)(
    executive_snn_bridge_t* bridge,
    float conflict_level,
    uint64_t latency_us,
    void* user_data
);

/** @brief Control output ready callback */
typedef void (*executive_snn_control_callback_t)(
    executive_snn_bridge_t* bridge,
    const executive_control_output_t* output,
    void* user_data
);

/** @brief Error detection callback */
typedef void (*executive_snn_error_callback_t)(
    executive_snn_bridge_t* bridge,
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
executive_snn_config_t executive_snn_config_default(void);

/**
 * @brief Create executive SNN bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
executive_snn_bridge_t* executive_snn_create(const executive_snn_config_t* config);

/**
 * @brief Destroy executive SNN bridge
 * @param bridge Bridge to destroy
 */
void executive_snn_destroy(executive_snn_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int executive_snn_reset(executive_snn_bridge_t* bridge);

//=============================================================================
// Encoding Functions
//=============================================================================

/**
 * @brief Encode executive state into SNN
 * @param bridge Bridge handle
 * @param dimensions Dimension activation values [0-1]
 * @param num_dims Number of dimensions
 * @return Spike count on success, -1 on failure
 */
int executive_snn_encode_state(
    executive_snn_bridge_t* bridge,
    const float* dimensions,
    uint32_t num_dims
);

/**
 * @brief Encode inhibition requirement
 * @param bridge Bridge handle
 * @param inhibition_strength Required inhibition strength [0-1]
 * @param urgency Urgency of inhibition [0-1]
 * @return Spike count on success, -1 on failure
 */
int executive_snn_encode_inhibition(
    executive_snn_bridge_t* bridge,
    float inhibition_strength,
    float urgency
);

/**
 * @brief Encode task context
 * @param bridge Bridge handle
 * @param task_load Task complexity [0-1]
 * @param task_count Number of active tasks
 * @return Spike count on success, -1 on failure
 */
int executive_snn_encode_task(
    executive_snn_bridge_t* bridge,
    float task_load,
    uint32_t task_count
);

/**
 * @brief Encode conflict signal
 * @param bridge Bridge handle
 * @param conflict_magnitude Conflict magnitude [0-1]
 * @param conflict_type Conflict type classification
 * @return Spike count on success, -1 on failure
 */
int executive_snn_encode_conflict(
    executive_snn_bridge_t* bridge,
    float conflict_magnitude,
    uint32_t conflict_type
);

//=============================================================================
// Simulation Functions
//=============================================================================

/**
 * @brief Simulate executive processing
 * @param bridge Bridge handle
 * @param duration_ms Simulation duration
 * @return 0 on success, -1 on failure
 */
int executive_snn_simulate(executive_snn_bridge_t* bridge, float duration_ms);

/**
 * @brief Single simulation step
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int executive_snn_step(executive_snn_bridge_t* bridge);

/**
 * @brief Forward pass through network
 * @param bridge Bridge handle
 * @param inputs Input values
 * @param input_count Input size
 * @return Spike count, -1 on failure
 */
int executive_snn_forward(
    executive_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
);

//=============================================================================
// Decoding Functions
//=============================================================================

/**
 * @brief Get executive control output from SNN activity
 * @param bridge Bridge handle
 * @param output Output control structure
 * @return 0 on success, -1 on failure
 */
int executive_snn_get_control_output(
    executive_snn_bridge_t* bridge,
    executive_control_output_t* output
);

/**
 * @brief Get dimension activations
 * @param bridge Bridge handle
 * @param activations Output buffer for activations
 * @param num_dims Number of dimensions to query
 * @return 0 on success, -1 on failure
 */
int executive_snn_get_activations(
    executive_snn_bridge_t* bridge,
    float* activations,
    uint32_t num_dims
);

/**
 * @brief Check for conflict detection
 * @param bridge Bridge handle
 * @param conflict_level Output conflict level
 * @return true if conflict detected, false otherwise
 */
bool executive_snn_check_conflict(
    executive_snn_bridge_t* bridge,
    float* conflict_level
);

/**
 * @brief Check for error detection
 * @param bridge Bridge handle
 * @param error_level Output error level
 * @return true if error detected, false otherwise
 */
bool executive_snn_check_error(
    executive_snn_bridge_t* bridge,
    float* error_level
);

/**
 * @brief Check for goal state change
 * @param bridge Bridge handle
 * @param change_magnitude Output change magnitude
 * @return true if goal state change detected, false otherwise
 */
bool executive_snn_check_goal_change(
    executive_snn_bridge_t* bridge,
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
int executive_snn_get_dim_state(
    executive_snn_bridge_t* bridge,
    uint32_t dim,
    executive_dim_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int executive_snn_get_state(
    executive_snn_bridge_t* bridge,
    executive_snn_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int executive_snn_get_stats(executive_snn_bridge_t* bridge, executive_snn_stats_t* stats);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int executive_snn_reset_stats(executive_snn_bridge_t* bridge);

/**
 * @brief Get current inhibition level
 * @param bridge Bridge handle
 * @return Inhibition level [0-1], -1 on error
 */
float executive_snn_get_inhibition(executive_snn_bridge_t* bridge);

/**
 * @brief Get total network activity
 * @param bridge Bridge handle
 * @return Activity level, -1 on error
 */
float executive_snn_get_total_activity(executive_snn_bridge_t* bridge);

//=============================================================================
// Callback Registration
//=============================================================================

/**
 * @brief Register conflict detection callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int executive_snn_register_conflict_callback(
    executive_snn_bridge_t* bridge,
    executive_snn_conflict_callback_t callback,
    void* user_data
);

/**
 * @brief Register control output callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int executive_snn_register_control_callback(
    executive_snn_bridge_t* bridge,
    executive_snn_control_callback_t callback,
    void* user_data
);

/**
 * @brief Register error detection callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int executive_snn_register_error_callback(
    executive_snn_bridge_t* bridge,
    executive_snn_error_callback_t callback,
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
int executive_snn_bio_async_connect(executive_snn_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int executive_snn_bio_async_disconnect(executive_snn_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool executive_snn_is_bio_async_connected(executive_snn_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EXECUTIVE_SNN_BRIDGE_H */
