/**
 * @file nimcp_gw_snn_bridge.h
 * @brief Global Workspace - SNN Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between global workspace and spiking neural networks
 * WHY:  Enable biologically-plausible conscious broadcast through
 *       population coding and spike-timing dynamics
 * HOW:  Encode workspace states as spike patterns, decode conscious access
 *       signals from neural activity patterns
 *
 * THEORETICAL FOUNDATIONS:
 * - Baars (1988): Global Workspace Theory of consciousness
 * - Dehaene (2014): Global Neuronal Workspace - ignition and broadcast
 * - Tononi (2004): Integrated Information Theory - binding and integration
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal-parietal network for workspace implementation
 * - Gamma synchrony (40Hz) for binding and broadcast
 * - Ignition cascade for conscious access threshold
 * - Winner-take-all dynamics for coalition competition
 *
 * INTEGRATION WITH CONSCIOUSNESS:
 * - Broadcast strength through population activity
 * - Ignition detection via synchrony thresholds
 * - Coalition formation through lateral connectivity
 * - Access consciousness through output decoding
 *
 * @see nimcp_global_workspace.h
 * @see nimcp_snn_network.h
 */

#ifndef NIMCP_GW_SNN_BRIDGE_H
#define NIMCP_GW_SNN_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum GW dimensions to encode */
#define GW_SNN_MAX_DIMENSIONS     16

/** @brief Neurons per GW dimension */
#define GW_SNN_NEURONS_PER_DIM    32

/** @brief Default ignition threshold */
#define GW_SNN_IGNITION_THRESH    0.6f

/** @brief Default encoding window (ms) */
#define GW_SNN_ENCODING_WINDOW    50.0f

/** @brief Bio-async module ID */
#define BIO_MODULE_GW_SNN         0x0D50

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Global Workspace dimension types for SNN encoding
 */
typedef enum {
    GW_DIM_BROADCAST = 0,           /**< Global broadcast strength */
    GW_DIM_IGNITION,                /**< Ignition threshold */
    GW_DIM_COMPETITION,             /**< Coalition competition */
    GW_DIM_INTEGRATION,             /**< Information integration */
    GW_DIM_ACCESS_CONSCIOUSNESS,    /**< Access consciousness level */
    GW_DIM_BINDING,                 /**< Feature binding */
    GW_DIM_WORKING_MEMORY_ACCESS,   /**< Working memory access */
    GW_DIM_ATTENTION_WINNER,        /**< Attention competition winner */
    GW_DIM_COALITION_STRENGTH,      /**< Coalition strength */
    GW_DIM_CONSCIOUS_CONTENT,       /**< Conscious content signal */
    GW_DIM_COUNT
} gw_snn_dimension_t;

/**
 * @brief Encoding methods for workspace content
 */
typedef enum {
    GW_SNN_ENCODE_RATE = 0,         /**< Rate coding of dimensions */
    GW_SNN_ENCODE_TEMPORAL,         /**< Temporal spike patterns */
    GW_SNN_ENCODE_POPULATION,       /**< Population vector coding */
    GW_SNN_ENCODE_SYNCHRONY         /**< Synchrony-based encoding (gamma) */
} gw_snn_encoding_t;

/**
 * @brief Decoding methods for conscious access
 */
typedef enum {
    GW_SNN_DECODE_THRESHOLD = 0,    /**< Threshold-based detection */
    GW_SNN_DECODE_COMPETITION,      /**< Winner-take-all */
    GW_SNN_DECODE_SOFTMAX,          /**< Soft probabilistic */
    GW_SNN_DECODE_INTEGRATION       /**< Evidence accumulation */
} gw_snn_decoding_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    GW_SNN_STATE_IDLE = 0,
    GW_SNN_STATE_ENCODING,
    GW_SNN_STATE_PROCESSING,
    GW_SNN_STATE_DECODING,
    GW_SNN_STATE_SIMULATING,
    GW_SNN_STATE_ERROR
} gw_snn_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Global Workspace-SNN bridge configuration
 */
typedef struct {
    /* Network dimensions */
    uint32_t num_dimensions;             /**< Number of GW dimensions */
    uint32_t neurons_per_dim;            /**< Neurons per dimension */
    uint32_t hidden_dim;                 /**< Hidden layer dimension */

    /* Timing parameters */
    float dt_ms;                         /**< Simulation timestep (ms) */
    float encoding_window_ms;            /**< Encoding time window */
    float integration_tau_ms;            /**< Evidence integration time constant */

    /* Encoding parameters */
    gw_snn_encoding_t encoding;          /**< Encoding method */
    float encoding_gain;                 /**< Encoding strength gain */
    float baseline_rate_hz;              /**< Baseline firing rate */
    float max_rate_hz;                   /**< Maximum firing rate */

    /* Decoding parameters */
    gw_snn_decoding_t decoding;          /**< Decoding method */
    float ignition_threshold;            /**< Threshold for ignition detection */
    float broadcast_threshold;           /**< Minimum broadcast strength */
    float consciousness_threshold;       /**< Access consciousness threshold */

    /* Network dynamics */
    bool enable_competition;             /**< Enable coalition competition */
    float inhibition_strength;           /**< Lateral inhibition weight */
    bool enable_binding;                 /**< Enable feature binding */
    float binding_threshold;             /**< Binding detection threshold */

    /* Conscious access integration */
    bool enable_access_consciousness;    /**< Enable access consciousness signals */
    float access_gain;                   /**< Access consciousness gain */
    bool enable_broadcast_detection;     /**< Enable broadcast detection */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
    bool enable_plasticity_integration;  /**< Enable plasticity bridge */
} gw_snn_config_t;

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
} gw_dim_state_t;

/**
 * @brief Conscious access output
 */
typedef struct {
    float broadcast_strength;            /**< Current broadcast strength [0-1] */
    float ignition_level;                /**< Ignition level [0-1] */
    float competition_result;            /**< Competition winner strength [0-1] */
    float integration_score;             /**< Information integration (phi-like) */
    float access_consciousness;          /**< Access consciousness level */
    bool ignition_detected;              /**< Ignition event detected */
    bool broadcast_active;               /**< Global broadcast active */
    float binding_strength;              /**< Feature binding strength */
    float coalition_strength;            /**< Coalition strength */
    float attention_winner;              /**< Attention competition winner */
} gw_conscious_access_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    gw_snn_state_t state;                /**< Current operational state */
    uint32_t active_dimensions;          /**< Number of active dimensions */
    float total_activity;                /**< Total network activity */
    float mean_broadcast;                /**< Mean broadcast strength */
    float ignition_signal;               /**< Current ignition signal */
    float consciousness_level;           /**< Current consciousness level */
} gw_snn_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_evaluations;          /**< Total evaluations performed */
    uint64_t total_simulations;          /**< Total simulation steps */
    uint64_t total_spikes;               /**< Total spikes generated */
    uint64_t ignition_events;            /**< Ignition events detected */
    uint64_t broadcast_events;           /**< Broadcast events triggered */
    uint64_t binding_events;             /**< Feature binding events */
    float mean_evaluation_time_ms;       /**< Mean evaluation latency */
    float mean_broadcast_strength;       /**< Mean broadcast strength */
} gw_snn_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct gw_snn_bridge gw_snn_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Ignition detection callback */
typedef void (*gw_snn_ignition_callback_t)(
    gw_snn_bridge_t* bridge,
    float ignition_level,
    uint64_t latency_us,
    void* user_data
);

/** @brief Conscious access callback */
typedef void (*gw_snn_conscious_callback_t)(
    gw_snn_bridge_t* bridge,
    const gw_conscious_access_t* access,
    void* user_data
);

/** @brief Broadcast event callback */
typedef void (*gw_snn_broadcast_callback_t)(
    gw_snn_bridge_t* bridge,
    float broadcast_strength,
    uint32_t winning_dim,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
gw_snn_config_t gw_snn_config_default(void);

/**
 * @brief Create Global Workspace SNN bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
gw_snn_bridge_t* gw_snn_create(const gw_snn_config_t* config);

/**
 * @brief Destroy Global Workspace SNN bridge
 * @param bridge Bridge to destroy
 */
void gw_snn_destroy(gw_snn_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int gw_snn_reset(gw_snn_bridge_t* bridge);

//=============================================================================
// Encoding Functions
//=============================================================================

/**
 * @brief Encode workspace state into SNN
 * @param bridge Bridge handle
 * @param dimensions Dimension activation values [0-1]
 * @param num_dims Number of dimensions
 * @return Spike count on success, -1 on failure
 */
int gw_snn_encode_state(
    gw_snn_bridge_t* bridge,
    const float* dimensions,
    uint32_t num_dims
);

/**
 * @brief Encode broadcast signal
 * @param bridge Bridge handle
 * @param broadcast_strength Broadcast strength [0-1]
 * @param source_module Source module ID
 * @return Spike count on success, -1 on failure
 */
int gw_snn_encode_broadcast(
    gw_snn_bridge_t* bridge,
    float broadcast_strength,
    uint32_t source_module
);

/**
 * @brief Encode competition signal
 * @param bridge Bridge handle
 * @param competition_strength Competition strength [0-1]
 * @param num_competitors Number of active competitors
 * @return Spike count on success, -1 on failure
 */
int gw_snn_encode_competition(
    gw_snn_bridge_t* bridge,
    float competition_strength,
    uint32_t num_competitors
);

/**
 * @brief Encode ignition signal
 * @param bridge Bridge handle
 * @param ignition_strength Ignition strength [0-1]
 * @param cascade_depth Ignition cascade depth
 * @return Spike count on success, -1 on failure
 */
int gw_snn_encode_ignition(
    gw_snn_bridge_t* bridge,
    float ignition_strength,
    uint32_t cascade_depth
);

//=============================================================================
// Simulation Functions
//=============================================================================

/**
 * @brief Simulate workspace processing
 * @param bridge Bridge handle
 * @param duration_ms Simulation duration
 * @return 0 on success, -1 on failure
 */
int gw_snn_simulate(gw_snn_bridge_t* bridge, float duration_ms);

/**
 * @brief Single simulation step
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int gw_snn_step(gw_snn_bridge_t* bridge);

/**
 * @brief Forward pass through network
 * @param bridge Bridge handle
 * @param inputs Input values
 * @param input_count Input size
 * @return Spike count, -1 on failure
 */
int gw_snn_forward(
    gw_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
);

//=============================================================================
// Decoding Functions
//=============================================================================

/**
 * @brief Get conscious access state from SNN activity
 * @param bridge Bridge handle
 * @param access Output access structure
 * @return 0 on success, -1 on failure
 */
int gw_snn_get_conscious_access(
    gw_snn_bridge_t* bridge,
    gw_conscious_access_t* access
);

/**
 * @brief Get dimension activations
 * @param bridge Bridge handle
 * @param activations Output buffer for activations
 * @param num_dims Number of dimensions to query
 * @return 0 on success, -1 on failure
 */
int gw_snn_get_activations(
    gw_snn_bridge_t* bridge,
    float* activations,
    uint32_t num_dims
);

/**
 * @brief Check for ignition event
 * @param bridge Bridge handle
 * @param ignition_level Output ignition level
 * @return true if ignition detected, false otherwise
 */
bool gw_snn_check_ignition(
    gw_snn_bridge_t* bridge,
    float* ignition_level
);

/**
 * @brief Check for active broadcast
 * @param bridge Bridge handle
 * @param broadcast_strength Output broadcast strength
 * @return true if broadcast active, false otherwise
 */
bool gw_snn_check_broadcast(
    gw_snn_bridge_t* bridge,
    float* broadcast_strength
);

/**
 * @brief Check for binding event
 * @param bridge Bridge handle
 * @param binding_strength Output binding strength
 * @return true if binding detected, false otherwise
 */
bool gw_snn_check_binding(
    gw_snn_bridge_t* bridge,
    float* binding_strength
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
int gw_snn_get_dim_state(
    gw_snn_bridge_t* bridge,
    uint32_t dim,
    gw_dim_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int gw_snn_get_state(
    gw_snn_bridge_t* bridge,
    gw_snn_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int gw_snn_get_stats(gw_snn_bridge_t* bridge, gw_snn_stats_t* stats);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int gw_snn_reset_stats(gw_snn_bridge_t* bridge);

/**
 * @brief Get current broadcast strength
 * @param bridge Bridge handle
 * @return Broadcast strength [0-1], -1 on error
 */
float gw_snn_get_broadcast_strength(gw_snn_bridge_t* bridge);

/**
 * @brief Get total network activity
 * @param bridge Bridge handle
 * @return Activity level, -1 on error
 */
float gw_snn_get_total_activity(gw_snn_bridge_t* bridge);

//=============================================================================
// Callback Registration
//=============================================================================

/**
 * @brief Register ignition detection callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int gw_snn_register_ignition_callback(
    gw_snn_bridge_t* bridge,
    gw_snn_ignition_callback_t callback,
    void* user_data
);

/**
 * @brief Register conscious access callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int gw_snn_register_conscious_callback(
    gw_snn_bridge_t* bridge,
    gw_snn_conscious_callback_t callback,
    void* user_data
);

/**
 * @brief Register broadcast event callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int gw_snn_register_broadcast_callback(
    gw_snn_bridge_t* bridge,
    gw_snn_broadcast_callback_t callback,
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
int gw_snn_bio_async_connect(gw_snn_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int gw_snn_bio_async_disconnect(gw_snn_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool gw_snn_is_bio_async_connected(gw_snn_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GW_SNN_BRIDGE_H */
