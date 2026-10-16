/**
 * @file nimcp_collective_snn_bridge.h
 * @brief Collective Cognition - SNN Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between collective cognition engine and spiking neural networks
 * WHY:  Enable biologically-plausible distributed cognition through population coding
 *       and spike-timing dynamics for swarm intelligence and group synchronization
 * HOW:  Encode collective dimensions as spike patterns, decode coordination
 *       signals from neural activity patterns
 *
 * THEORETICAL FOUNDATIONS:
 * - Couzin (2009): Collective cognition in animal groups
 * - Seeley (2010): Honeybee democracy and swarm intelligence
 * - Conradt & Roper (2005): Consensus decision making in animals
 *
 * BIOLOGICAL BASIS:
 * - Mirror neuron systems for action understanding and coordination
 * - Hippocampal theta coupling for group memory synchronization
 * - Prefrontal cortex for joint attention and shared goals
 * - Anterior cingulate for conflict monitoring in collective decisions
 *
 * INTEGRATION WITH LEARNING:
 * - Swarm consensus through distributed spike synchronization
 * - Group coherence via population vector alignment
 * - Distributed decision making through competitive dynamics
 *
 * @see nimcp_collective_cognition.h
 * @see nimcp_snn_network.h
 */

#ifndef NIMCP_COLLECTIVE_SNN_BRIDGE_H
#define NIMCP_COLLECTIVE_SNN_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum collective dimensions to encode */
#define COLLECTIVE_SNN_MAX_DIMENSIONS     16

/** @brief Neurons per collective dimension */
#define COLLECTIVE_SNN_NEURONS_PER_DIM    32

/** @brief Default synchronization threshold */
#define COLLECTIVE_SNN_SYNC_THRESH        0.5f

/** @brief Default encoding window (ms) */
#define COLLECTIVE_SNN_ENCODING_WINDOW    50.0f

/** @brief Bio-async module ID */
#define BIO_MODULE_COLLECTIVE_SNN         0x1230

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Collective dimension types for SNN encoding
 */
typedef enum {
    COLLECTIVE_DIM_SWARM_COHERENCE = 0,    /**< Swarm coherence level */
    COLLECTIVE_DIM_GROUP_SYNC,              /**< Group synchronization */
    COLLECTIVE_DIM_SHARED_INTENTION,        /**< Shared intentionality strength */
    COLLECTIVE_DIM_DISTRIBUTED_DECISION,    /**< Distributed decision progress */
    COLLECTIVE_DIM_JOINT_ATTENTION,         /**< Joint attention focus */
    COLLECTIVE_DIM_ROLE_COORDINATION,       /**< Role coordination quality */
    COLLECTIVE_DIM_CONSENSUS_LEVEL,         /**< Consensus building level */
    COLLECTIVE_DIM_INFORMATION_SHARING,     /**< Information sharing rate */
    COLLECTIVE_DIM_TRUST_NETWORK,           /**< Trust network strength */
    COLLECTIVE_DIM_EMERGENCE,               /**< Emergent behavior detection */
    COLLECTIVE_DIM_COUNT
} collective_snn_dimension_t;

/**
 * @brief Encoding methods for collective contexts
 */
typedef enum {
    COLLECTIVE_SNN_ENCODE_RATE = 0,        /**< Rate coding of dimensions */
    COLLECTIVE_SNN_ENCODE_TEMPORAL,         /**< Temporal spike patterns */
    COLLECTIVE_SNN_ENCODE_POPULATION,       /**< Population vector coding */
    COLLECTIVE_SNN_ENCODE_SYNCHRONY         /**< Synchrony-based encoding */
} collective_snn_encoding_t;

/**
 * @brief Decoding methods for coordination states
 */
typedef enum {
    COLLECTIVE_SNN_DECODE_THRESHOLD = 0,   /**< Threshold-based detection */
    COLLECTIVE_SNN_DECODE_COMPETITION,      /**< Winner-take-all */
    COLLECTIVE_SNN_DECODE_SOFTMAX,          /**< Soft probabilistic */
    COLLECTIVE_SNN_DECODE_INTEGRATION       /**< Evidence accumulation */
} collective_snn_decoding_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    COLLECTIVE_SNN_STATE_IDLE = 0,
    COLLECTIVE_SNN_STATE_ENCODING,
    COLLECTIVE_SNN_STATE_PROCESSING,
    COLLECTIVE_SNN_STATE_DECODING,
    COLLECTIVE_SNN_STATE_SIMULATING,
    COLLECTIVE_SNN_STATE_ERROR
} collective_snn_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Collective-SNN bridge configuration
 */
typedef struct {
    /* Network dimensions */
    uint32_t num_dimensions;             /**< Number of collective dimensions */
    uint32_t neurons_per_dim;            /**< Neurons per dimension */
    uint32_t hidden_dim;                 /**< Hidden layer dimension */

    /* Timing parameters */
    float dt_ms;                         /**< Simulation timestep (ms) */
    float encoding_window_ms;            /**< Encoding time window */
    float integration_tau_ms;            /**< Evidence integration time constant */

    /* Encoding parameters */
    collective_snn_encoding_t encoding;  /**< Encoding method */
    float encoding_gain;                 /**< Encoding strength gain */
    float baseline_rate_hz;              /**< Baseline firing rate */
    float max_rate_hz;                   /**< Maximum firing rate */

    /* Decoding parameters */
    collective_snn_decoding_t decoding;  /**< Decoding method */
    float sync_threshold;                /**< Threshold for sync detection */
    float coordination_threshold;        /**< Minimum coordination drive */
    float state_change_threshold;        /**< Threshold for state change */

    /* Network dynamics */
    bool enable_competition;             /**< Enable dimension competition */
    float inhibition_strength;           /**< Lateral inhibition weight */
    bool enable_sync_detection;          /**< Enable sync signal detection */
    float sync_sensitivity;              /**< Sync detection sensitivity */

    /* Coordination integration */
    bool enable_coordination;            /**< Enable coordination circuits */
    float coordination_gain;             /**< Coordination signal gain */
    bool enable_emergence_tracking;      /**< Enable emergence detection */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
    bool enable_plasticity_integration;  /**< Enable plasticity bridge */
} collective_snn_config_t;

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
} collective_dim_state_t;

/**
 * @brief Coordination drive output
 */
typedef struct {
    float swarm_coherence;               /**< Current swarm coherence [0-1] */
    float group_sync_level;              /**< Group synchronization [0-1] */
    float shared_intention;              /**< Shared intentionality [0-1] */
    float coordination_drive;            /**< Coordination drive strength */
    float consensus_level;               /**< Consensus building level */
    bool sync_detected;                  /**< Sync detected */
    bool high_coordination;              /**< High coordination state */
    float coordination_magnitude;        /**< Coordination magnitude if detected */
    float emergence_level;               /**< Emergent behavior level */
    float trust_strength;                /**< Trust network strength */
} collective_drive_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    collective_snn_state_t state;        /**< Current operational state */
    uint32_t active_dimensions;          /**< Number of active dimensions */
    float total_activity;                /**< Total network activity */
    float mean_coordination;             /**< Mean coordination drive */
    float sync_signal;                   /**< Current sync signal */
    float emergence_signal;              /**< Current emergence signal */
} collective_snn_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_evaluations;          /**< Total evaluations performed */
    uint64_t total_simulations;          /**< Total simulation steps */
    uint64_t total_spikes;               /**< Total spikes generated */
    uint64_t sync_detections;            /**< Sync detections */
    uint64_t high_coordination_events;   /**< High coordination events */
    uint64_t state_changes;              /**< State changes detected */
    float mean_evaluation_time_ms;       /**< Mean evaluation latency */
    float mean_coordination;             /**< Mean coordination drive */
} collective_snn_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct collective_snn_bridge collective_snn_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Sync detection callback */
typedef void (*collective_snn_sync_callback_t)(
    collective_snn_bridge_t* bridge,
    float sync_level,
    uint64_t latency_us,
    void* user_data
);

/** @brief Drive ready callback */
typedef void (*collective_snn_drive_callback_t)(
    collective_snn_bridge_t* bridge,
    const collective_drive_t* drive,
    void* user_data
);

/** @brief High coordination callback */
typedef void (*collective_snn_coordination_callback_t)(
    collective_snn_bridge_t* bridge,
    float coordination_level,
    uint32_t coordination_dim,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
collective_snn_config_t collective_snn_config_default(void);

/**
 * @brief Create collective SNN bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
collective_snn_bridge_t* collective_snn_create(const collective_snn_config_t* config);

/**
 * @brief Destroy collective SNN bridge
 * @param bridge Bridge to destroy
 */
void collective_snn_destroy(collective_snn_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int collective_snn_reset(collective_snn_bridge_t* bridge);

//=============================================================================
// Encoding Functions
//=============================================================================

/**
 * @brief Encode collective state into SNN
 * @param bridge Bridge handle
 * @param dimensions Dimension activation values [0-1]
 * @param num_dims Number of dimensions
 * @return Spike count on success, -1 on failure
 */
int collective_snn_encode_state(
    collective_snn_bridge_t* bridge,
    const float* dimensions,
    uint32_t num_dims
);

/**
 * @brief Encode swarm intelligence state
 * @param bridge Bridge handle
 * @param coherence Swarm coherence level [0-1]
 * @param sync Group synchronization [0-1]
 * @return Spike count on success, -1 on failure
 */
int collective_snn_encode_swarm(
    collective_snn_bridge_t* bridge,
    float coherence,
    float sync
);

/**
 * @brief Encode collective decision state
 * @param bridge Bridge handle
 * @param consensus Consensus level [0-1]
 * @param participant_count Number of active participants
 * @return Spike count on success, -1 on failure
 */
int collective_snn_encode_decision(
    collective_snn_bridge_t* bridge,
    float consensus,
    uint32_t participant_count
);

/**
 * @brief Encode shared intentionality
 * @param bridge Bridge handle
 * @param intention Shared intention strength [0-1]
 * @param intent_type Intention type classification
 * @return Spike count on success, -1 on failure
 */
int collective_snn_encode_intention(
    collective_snn_bridge_t* bridge,
    float intention,
    uint32_t intent_type
);

//=============================================================================
// Simulation Functions
//=============================================================================

/**
 * @brief Simulate collective processing
 * @param bridge Bridge handle
 * @param duration_ms Simulation duration
 * @return 0 on success, -1 on failure
 */
int collective_snn_simulate(collective_snn_bridge_t* bridge, float duration_ms);

/**
 * @brief Single simulation step
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int collective_snn_step(collective_snn_bridge_t* bridge);

/**
 * @brief Forward pass through network
 * @param bridge Bridge handle
 * @param inputs Input values
 * @param input_count Input size
 * @return Spike count, -1 on failure
 */
int collective_snn_forward(
    collective_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
);

//=============================================================================
// Decoding Functions
//=============================================================================

/**
 * @brief Get coordination drive from SNN activity
 * @param bridge Bridge handle
 * @param drive Output drive structure
 * @return 0 on success, -1 on failure
 */
int collective_snn_get_drive(
    collective_snn_bridge_t* bridge,
    collective_drive_t* drive
);

/**
 * @brief Get dimension activations
 * @param bridge Bridge handle
 * @param activations Output buffer for activations
 * @param num_dims Number of dimensions to query
 * @return 0 on success, -1 on failure
 */
int collective_snn_get_activations(
    collective_snn_bridge_t* bridge,
    float* activations,
    uint32_t num_dims
);

/**
 * @brief Check for high synchronization
 * @param bridge Bridge handle
 * @param sync_level Output sync level
 * @return true if high sync detected, false otherwise
 */
bool collective_snn_check_sync(
    collective_snn_bridge_t* bridge,
    float* sync_level
);

/**
 * @brief Check for high coordination
 * @param bridge Bridge handle
 * @param coordination_level Output coordination level
 * @return true if high coordination detected, false otherwise
 */
bool collective_snn_check_coordination(
    collective_snn_bridge_t* bridge,
    float* coordination_level
);

/**
 * @brief Check for state change
 * @param bridge Bridge handle
 * @param change_magnitude Output change magnitude
 * @return true if state change detected, false otherwise
 */
bool collective_snn_check_state_change(
    collective_snn_bridge_t* bridge,
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
int collective_snn_get_dim_state(
    collective_snn_bridge_t* bridge,
    uint32_t dim,
    collective_dim_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int collective_snn_get_state(
    collective_snn_bridge_t* bridge,
    collective_snn_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int collective_snn_get_stats(collective_snn_bridge_t* bridge, collective_snn_stats_t* stats);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int collective_snn_reset_stats(collective_snn_bridge_t* bridge);

/**
 * @brief Get current coordination drive level
 * @param bridge Bridge handle
 * @return Coordination drive [0-1], -1 on error
 */
float collective_snn_get_coordination(collective_snn_bridge_t* bridge);

/**
 * @brief Get total network activity
 * @param bridge Bridge handle
 * @return Activity level, -1 on error
 */
float collective_snn_get_total_activity(collective_snn_bridge_t* bridge);

//=============================================================================
// Callback Registration
//=============================================================================

/**
 * @brief Register sync detection callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int collective_snn_register_sync_callback(
    collective_snn_bridge_t* bridge,
    collective_snn_sync_callback_t callback,
    void* user_data
);

/**
 * @brief Register drive callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int collective_snn_register_drive_callback(
    collective_snn_bridge_t* bridge,
    collective_snn_drive_callback_t callback,
    void* user_data
);

/**
 * @brief Register high coordination callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int collective_snn_register_coordination_callback(
    collective_snn_bridge_t* bridge,
    collective_snn_coordination_callback_t callback,
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
int collective_snn_bio_async_connect(collective_snn_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int collective_snn_bio_async_disconnect(collective_snn_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool collective_snn_is_bio_async_connected(collective_snn_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_COLLECTIVE_SNN_BRIDGE_H */
