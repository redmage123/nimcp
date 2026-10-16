/**
 * @file nimcp_mirror_snn_bridge.h
 * @brief Mirror Neuron - SNN Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-05
 *
 * WHAT: Bidirectional bridge between mirror neuron system and SNN module
 * WHY:  Enable spike-based computation and bio-plausible learning for mirror neurons
 * HOW:  Convert observations/executions to spikes, integrate SNN output back to mirror
 *
 * THEORETICAL FOUNDATIONS:
 * - Rizzolatti & Craighero (2004): Mirror neuron system
 * - Maass (2002): Spiking neural networks computation
 * - Gerstner & Kistler (2002): Spiking neuron models
 *
 * BIOLOGICAL BASIS:
 * - Mirror neurons fire with precise spike timing for observed actions
 * - Temporal coding conveys action information in spike patterns
 * - Population coding enables robust action representation
 * - Spike timing drives STDP learning in observation-execution pathways
 *
 * INTEGRATION FLOWS:
 *
 * Mirror Neurons --> SNN:
 *   1. Observation events encoded as input spike trains
 *   2. Action features converted to population activity
 *   3. Mirror resonance strength modulates firing rate
 *   4. STDP eligibility traces updated on spikes
 *
 * SNN --> Mirror Neurons:
 *   1. SNN output decoded to action recognition confidence
 *   2. Population firing rates inform mirror activation levels
 *   3. Spike synchrony indicates coherent action representation
 *   4. SNN health status affects mirror processing
 *
 * BIO-ASYNC MESSAGES:
 * - SNN_BIO_MSG_SPIKE_EVENT: Spike events from mirror neuron activity
 * - SNN_BIO_MSG_RATE_UPDATE: Firing rate updates to mirror system
 * - SNN_BIO_MSG_POPULATION_ACTIVITY: Population coding for action recognition
 * - SNN_BIO_MSG_TRAINING_EVENT: STDP/training events for mirror learning
 *
 * @see nimcp_snn.h
 * @see nimcp_mirror_neurons.h
 * @see nimcp_mirror_plasticity_bridge.h
 */

#ifndef NIMCP_MIRROR_SNN_BRIDGE_H
#define NIMCP_MIRROR_SNN_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "snn/nimcp_snn.h"
#include "snn/nimcp_snn_bio_async.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum mirror neuron populations in SNN */
#define MIRROR_SNN_MAX_POPULATIONS          8

/** @brief Default neurons per mirror action population */
#define MIRROR_SNN_NEURONS_PER_ACTION       64

/** @brief Default input encoding dimension */
#define MIRROR_SNN_INPUT_DIM                128

/** @brief Default output decoding dimension */
#define MIRROR_SNN_OUTPUT_DIM               32

/** @brief Maximum tracked actions */
#define MIRROR_SNN_MAX_ACTIONS              128

/** @brief Bio-async module ID for mirror-SNN bridge */
#define BIO_MODULE_MIRROR_SNN_BRIDGE        0x0A00

/** @brief Default simulation timestep (ms) */
#define MIRROR_SNN_DEFAULT_DT               1.0f

/** @brief Default spike encoding window (ms) */
#define MIRROR_SNN_ENCODING_WINDOW          100.0f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Spike encoding method for mirror observations
 */
typedef enum {
    MIRROR_SNN_ENCODE_RATE = 0,          /**< Rate coding */
    MIRROR_SNN_ENCODE_TEMPORAL,          /**< Temporal/latency coding */
    MIRROR_SNN_ENCODE_POPULATION,        /**< Population coding */
    MIRROR_SNN_ENCODE_BURST              /**< Burst coding for salient events */
} mirror_snn_encoding_t;

/**
 * @brief Mirror-SNN bridge state
 */
typedef enum {
    MIRROR_SNN_STATE_IDLE = 0,
    MIRROR_SNN_STATE_ENCODING,           /**< Encoding observation to spikes */
    MIRROR_SNN_STATE_SIMULATING,         /**< Running SNN simulation */
    MIRROR_SNN_STATE_DECODING,           /**< Decoding SNN output */
    MIRROR_SNN_STATE_TRAINING            /**< Training mode active */
} mirror_snn_state_t;

/**
 * @brief Action encoding status
 */
typedef enum {
    MIRROR_SNN_ACTION_INACTIVE = 0,
    MIRROR_SNN_ACTION_OBSERVED,          /**< Action being observed */
    MIRROR_SNN_ACTION_EXECUTING,         /**< Action being executed */
    MIRROR_SNN_ACTION_RECOGNIZED         /**< Action recognized from SNN output */
} mirror_snn_action_status_t;

//=============================================================================
// Configuration Structures
//=============================================================================

/**
 * @brief Mirror-SNN bridge configuration
 *
 * WHAT: Parameters controlling mirror-SNN integration
 * WHY:  Enable customization of encoding, simulation, and training
 */
typedef struct {
    /* Network topology */
    uint32_t input_dim;                  /**< Input feature dimension */
    uint32_t hidden_dim;                 /**< Hidden layer size */
    uint32_t output_dim;                 /**< Output dimension (actions) */
    uint32_t neurons_per_action;         /**< Neurons per action population */

    /* Encoding parameters */
    mirror_snn_encoding_t encoding_method; /**< Spike encoding method */
    float encoding_gain;                 /**< Input-to-spike conversion gain */
    float encoding_threshold;            /**< Minimum input for spike */
    float encoding_window_ms;            /**< Time window for encoding */

    /* Simulation parameters */
    float dt_ms;                         /**< Simulation timestep */
    float simulation_duration_ms;        /**< Default simulation duration */
    bool enable_recurrence;              /**< Enable recurrent connections */

    /* Training parameters */
    bool enable_training;                /**< Enable STDP training */
    float learning_rate;                 /**< Base learning rate */
    bool enable_reward_modulation;       /**< Enable R-STDP */

    /* Decoding parameters */
    float decoding_threshold;            /**< Min rate for action recognition */
    float confidence_gain;               /**< Output-to-confidence scaling */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async messaging */
    uint32_t bio_async_priority;         /**< Message priority */

    /* Immune integration */
    bool enable_immune_integration;      /**< Connect to immune system */

    /* Update timing */
    float update_interval_ms;            /**< Bridge update frequency */
} mirror_snn_config_t;

//=============================================================================
// State Structures
//=============================================================================

/**
 * @brief Per-action spike tracking
 */
typedef struct {
    uint32_t action_id;                  /**< Action identifier */
    mirror_snn_action_status_t status;   /**< Current status */

    /* Spike statistics */
    uint32_t spike_count;                /**< Spikes in current window */
    float firing_rate;                   /**< Current firing rate (Hz) */
    float avg_spike_time;                /**< Average spike latency */

    /* Recognition output */
    float recognition_confidence;        /**< SNN output confidence */
    float population_coherence;          /**< Inter-neuron synchrony */

    /* Timing */
    uint64_t last_spike_us;              /**< Last spike timestamp */
    uint64_t observation_start_us;       /**< Observation window start */
} mirror_snn_action_state_t;

/**
 * @brief Bridge state snapshot
 */
typedef struct {
    mirror_snn_state_t state;            /**< Current bridge state */

    /* SNN health */
    snn_state_health_t snn_health;       /**< SNN network health */
    float mean_firing_rate;              /**< Network mean rate (Hz) */
    float sparsity;                      /**< Spike sparsity */

    /* Mirror integration */
    float mirror_to_snn_signal;          /**< Mirror --> SNN signal strength */
    float snn_to_mirror_signal;          /**< SNN --> Mirror signal strength */

    /* Action tracking */
    uint32_t active_observations;        /**< Currently observed actions */
    uint32_t recognized_actions;         /**< Actions recognized by SNN */

    /* Training state */
    bool training_active;                /**< Training mode on */
    uint32_t weight_updates;             /**< Weight updates this session */
    float current_loss;                  /**< Current training loss */
} mirror_snn_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Processing counts */
    uint64_t total_observations;         /**< Total observations processed */
    uint64_t total_spikes_generated;     /**< Total input spikes generated */
    uint64_t total_simulation_steps;     /**< Total SNN steps */
    uint64_t total_recognitions;         /**< Successful action recognitions */

    /* Training statistics */
    uint64_t training_iterations;        /**< Training iterations */
    uint64_t stdp_events;                /**< STDP weight updates */
    float avg_weight_change;             /**< Average weight change */

    /* Performance metrics */
    float avg_recognition_latency_ms;    /**< Mean recognition time */
    float recognition_accuracy;          /**< Recognition accuracy */
    float encoding_efficiency;           /**< Spikes per information bit */

    /* Bio-async statistics */
    uint64_t bio_messages_sent;          /**< Messages sent */
    uint64_t bio_messages_received;      /**< Messages received */
    uint64_t bio_messages_dropped;       /**< Messages dropped */

    /* Resource usage */
    float avg_simulation_time_us;        /**< Average sim time per step */
    size_t memory_usage_bytes;           /**< Current memory usage */
} mirror_snn_stats_t;

//=============================================================================
// Callback Types
//=============================================================================

/**
 * @brief Spike event callback (SNN --> Mirror)
 *
 * Called when SNN generates spikes that affect mirror processing
 */
typedef void (*mirror_snn_spike_callback_t)(
    uint32_t population_id,
    uint32_t neuron_id,
    float spike_time,
    void* user_data
);

/**
 * @brief Action recognition callback
 *
 * Called when SNN recognizes an action from observation spikes
 */
typedef void (*mirror_snn_recognition_callback_t)(
    uint32_t action_id,
    float confidence,
    float latency_ms,
    void* user_data
);

/**
 * @brief Training event callback
 *
 * Called on STDP or reward-modulated learning events
 */
typedef void (*mirror_snn_training_callback_t)(
    uint32_t synapse_id,
    float weight_change,
    float new_weight,
    void* user_data
);

/**
 * @brief Health status callback
 *
 * Called when SNN health status changes
 */
typedef void (*mirror_snn_health_callback_t)(
    snn_state_health_t old_health,
    snn_state_health_t new_health,
    void* user_data
);

//=============================================================================
// Bridge Context
//=============================================================================

/** Forward declaration */
typedef struct mirror_snn_bridge mirror_snn_bridge_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default configuration
 *
 * WHAT: Return sensible defaults for mirror-SNN integration
 * WHY:  Provide biological plausible starting point
 *
 * @return Default configuration
 */
mirror_snn_config_t mirror_snn_config_default(void);

/**
 * @brief Create mirror-SNN bridge
 *
 * WHAT: Initialize bidirectional bridge between mirror neurons and SNN
 * WHY:  Enable spike-based computation for mirror neuron processing
 * HOW:  Create SNN network, configure encoding/decoding, register handlers
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
mirror_snn_bridge_t* mirror_snn_create(const mirror_snn_config_t* config);

/**
 * @brief Create bridge with existing SNN
 *
 * WHAT: Create bridge using externally managed SNN
 * WHY:  Share SNN with other modules
 *
 * @param config Configuration (NULL for defaults)
 * @param snn Existing SNN network (bridge does NOT own)
 * @return Bridge handle or NULL on error
 */
mirror_snn_bridge_t* mirror_snn_create_with_network(
    const mirror_snn_config_t* config,
    snn_network_t* snn
);

/**
 * @brief Destroy mirror-SNN bridge
 *
 * @param bridge Bridge to destroy (NULL-safe)
 */
void mirror_snn_destroy(mirror_snn_bridge_t* bridge);

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register bridge with bio-async system
 * WHY:  Enable inter-module spike and state communication
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int mirror_snn_connect_bio_async(mirror_snn_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge handle
 * @return 0 on success
 */
int mirror_snn_disconnect_bio_async(mirror_snn_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 *
 * @param bridge Bridge handle
 * @return true if connected
 */
bool mirror_snn_is_bio_async_connected(const mirror_snn_bridge_t* bridge);

/**
 * @brief Process pending bio-async messages
 *
 * WHAT: Process incoming messages from other modules
 * WHY:  Respond to external spike events and state changes
 *
 * @param bridge Bridge handle
 * @param timeout_ms Timeout (0 = non-blocking)
 * @return Number of messages processed
 */
int mirror_snn_process_messages(mirror_snn_bridge_t* bridge, int timeout_ms);

//=============================================================================
// Immune System Integration
//=============================================================================

/**
 * @brief Connect to brain immune system
 *
 * WHAT: Integrate with immune system for cytokine modulation
 * WHY:  Inflammatory signals affect neural excitability and plasticity
 *
 * @param bridge Bridge handle
 * @param immune_system Immune system handle
 * @return 0 on success, error code on failure
 */
int mirror_snn_connect_immune(
    mirror_snn_bridge_t* bridge,
    void* immune_system
);

/**
 * @brief Disconnect from immune system
 *
 * @param bridge Bridge handle
 * @return 0 on success
 */
int mirror_snn_disconnect_immune(mirror_snn_bridge_t* bridge);

/**
 * @brief Apply immune modulation to SNN
 *
 * WHAT: Update SNN parameters based on immune state
 * WHY:  Inflammation modulates excitability, learning rate
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int mirror_snn_apply_immune_modulation(mirror_snn_bridge_t* bridge);

//=============================================================================
// Mirror --> SNN Pathway (Observation Encoding)
//=============================================================================

/**
 * @brief Encode mirror observation as spike train
 *
 * WHAT: Convert observed action features to SNN input spikes
 * WHY:  Transform continuous observation to spike-based representation
 * HOW:  Apply rate/temporal/population coding based on config
 *
 * @param bridge Bridge handle
 * @param action_id Action being observed
 * @param features Action feature vector
 * @param feature_dim Feature dimension
 * @param observation_strength Observation confidence [0, 1]
 * @return Number of spikes generated, -1 on error
 */
int mirror_snn_encode_observation(
    mirror_snn_bridge_t* bridge,
    uint32_t action_id,
    const float* features,
    uint32_t feature_dim,
    float observation_strength
);

/**
 * @brief Encode execution attempt as spike train
 *
 * WHAT: Convert motor execution to SNN input spikes
 * WHY:  Enable observation-execution timing for STDP
 *
 * @param bridge Bridge handle
 * @param action_id Action being executed
 * @param motor_command Motor command features
 * @param command_dim Command dimension
 * @param execution_strength Execution intensity [0, 1]
 * @return Number of spikes generated, -1 on error
 */
int mirror_snn_encode_execution(
    mirror_snn_bridge_t* bridge,
    uint32_t action_id,
    const float* motor_command,
    uint32_t command_dim,
    float execution_strength
);

/**
 * @brief Set input tensor for SNN
 *
 * WHAT: Provide pre-formatted input tensor to SNN
 * WHY:  Direct tensor input for batch processing
 *
 * @param bridge Bridge handle
 * @param input Input tensor (shape: [input_dim])
 * @return 0 on success, error code on failure
 */
int mirror_snn_set_input_tensor(
    mirror_snn_bridge_t* bridge,
    const nimcp_tensor_t* input
);

//=============================================================================
// SNN --> Mirror Pathway (Output Decoding)
//=============================================================================

/**
 * @brief Run SNN simulation and decode output
 *
 * WHAT: Simulate SNN and extract action recognition
 * WHY:  Process encoded input through spike network
 *
 * @param bridge Bridge handle
 * @param duration_ms Simulation duration
 * @return Number of spikes generated, -1 on error
 */
int mirror_snn_simulate(mirror_snn_bridge_t* bridge, float duration_ms);

/**
 * @brief Get recognized action from SNN output
 *
 * WHAT: Decode SNN output to action recognition
 * WHY:  Extract highest-confidence recognized action
 *
 * @param bridge Bridge handle
 * @param action_id Output: Recognized action ID
 * @param confidence Output: Recognition confidence [0, 1]
 * @return 0 on success, error code if no recognition
 */
int mirror_snn_get_recognized_action(
    mirror_snn_bridge_t* bridge,
    uint32_t* action_id,
    float* confidence
);

/**
 * @brief Get action confidence scores
 *
 * WHAT: Get confidence scores for all actions
 * WHY:  Access full probability distribution over actions
 *
 * @param bridge Bridge handle
 * @param confidences Output array (size: num_actions)
 * @param num_actions Number of actions
 * @return Number of actions with non-zero confidence
 */
int mirror_snn_get_action_confidences(
    mirror_snn_bridge_t* bridge,
    float* confidences,
    uint32_t num_actions
);

/**
 * @brief Get population firing rate
 *
 * WHAT: Query firing rate for action population
 * WHY:  Monitor population-level activity
 *
 * @param bridge Bridge handle
 * @param action_id Action population
 * @param window_ms Time window for rate calculation
 * @return Firing rate (Hz), -1 on error
 */
float mirror_snn_get_population_rate(
    mirror_snn_bridge_t* bridge,
    uint32_t action_id,
    float window_ms
);

/**
 * @brief Get output tensor
 *
 * WHAT: Get decoded SNN output as tensor
 * WHY:  Direct tensor output for downstream processing
 *
 * @param bridge Bridge handle
 * @param output Output tensor (pre-allocated, shape: [output_dim])
 * @return 0 on success, error code on failure
 */
int mirror_snn_get_output_tensor(
    mirror_snn_bridge_t* bridge,
    nimcp_tensor_t* output
);

//=============================================================================
// Complete Forward Pass
//=============================================================================

/**
 * @brief Complete observation-to-recognition pass
 *
 * WHAT: Encode observation, simulate, decode action recognition
 * WHY:  Single call for complete processing pipeline
 *
 * @param bridge Bridge handle
 * @param action_id Action being observed
 * @param features Action feature vector
 * @param feature_dim Feature dimension
 * @param observation_strength Observation confidence
 * @param recognized_action Output: Recognized action
 * @param recognition_confidence Output: Recognition confidence
 * @return 0 on success, error code on failure
 */
int mirror_snn_forward(
    mirror_snn_bridge_t* bridge,
    uint32_t action_id,
    const float* features,
    uint32_t feature_dim,
    float observation_strength,
    uint32_t* recognized_action,
    float* recognition_confidence
);

//=============================================================================
// Training API
//=============================================================================

/**
 * @brief Enable/disable training mode
 *
 * @param bridge Bridge handle
 * @param enable true to enable training
 * @return 0 on success, error code on failure
 */
int mirror_snn_set_training(mirror_snn_bridge_t* bridge, bool enable);

/**
 * @brief Apply STDP learning
 *
 * WHAT: Apply spike-timing dependent plasticity
 * WHY:  Learn observation-execution associations
 *
 * @param bridge Bridge handle
 * @return Number of synapses updated
 */
int mirror_snn_apply_stdp(mirror_snn_bridge_t* bridge);

/**
 * @brief Apply reward-modulated STDP
 *
 * WHAT: Apply eligibility traces scaled by reward
 * WHY:  Reinforcement learning for action recognition
 *
 * @param bridge Bridge handle
 * @param reward Reward signal [-1, 1]
 * @return Number of synapses updated
 */
int mirror_snn_apply_reward(mirror_snn_bridge_t* bridge, float reward);

/**
 * @brief Train on observation-action pair
 *
 * WHAT: Single training iteration with target
 * WHY:  Supervised learning for action recognition
 *
 * @param bridge Bridge handle
 * @param features Input features
 * @param feature_dim Feature dimension
 * @param target_action Target action ID
 * @return Training loss, negative on error
 */
float mirror_snn_train_step(
    mirror_snn_bridge_t* bridge,
    const float* features,
    uint32_t feature_dim,
    uint32_t target_action
);

//=============================================================================
// Callback Registration
//=============================================================================

/**
 * @brief Register spike event callback
 *
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User context
 * @return 0 on success, error code on failure
 */
int mirror_snn_register_spike_callback(
    mirror_snn_bridge_t* bridge,
    mirror_snn_spike_callback_t callback,
    void* user_data
);

/**
 * @brief Register action recognition callback
 *
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User context
 * @return 0 on success, error code on failure
 */
int mirror_snn_register_recognition_callback(
    mirror_snn_bridge_t* bridge,
    mirror_snn_recognition_callback_t callback,
    void* user_data
);

/**
 * @brief Register training event callback
 *
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User context
 * @return 0 on success, error code on failure
 */
int mirror_snn_register_training_callback(
    mirror_snn_bridge_t* bridge,
    mirror_snn_training_callback_t callback,
    void* user_data
);

/**
 * @brief Register health status callback
 *
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User context
 * @return 0 on success, error code on failure
 */
int mirror_snn_register_health_callback(
    mirror_snn_bridge_t* bridge,
    mirror_snn_health_callback_t callback,
    void* user_data
);

//=============================================================================
// State Query API
//=============================================================================

/**
 * @brief Get bridge state snapshot
 *
 * @param bridge Bridge handle
 * @param state Output state
 * @return 0 on success, error code on failure
 */
int mirror_snn_get_state(
    const mirror_snn_bridge_t* bridge,
    mirror_snn_bridge_state_t* state
);

/**
 * @brief Get action state
 *
 * @param bridge Bridge handle
 * @param action_id Action to query
 * @param state Output action state
 * @return 0 on success, error code if not found
 */
int mirror_snn_get_action_state(
    const mirror_snn_bridge_t* bridge,
    uint32_t action_id,
    mirror_snn_action_state_t* state
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, error code on failure
 */
int mirror_snn_get_stats(
    const mirror_snn_bridge_t* bridge,
    mirror_snn_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge handle
 */
void mirror_snn_reset_stats(mirror_snn_bridge_t* bridge);

/**
 * @brief Check SNN health
 *
 * @param bridge Bridge handle
 * @return SNN health status
 */
snn_state_health_t mirror_snn_check_health(const mirror_snn_bridge_t* bridge);

//=============================================================================
// Main Update Loop
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Synchronize mirror and SNN state
 * WHY:  Keep bidirectional signals current
 * HOW:  Process messages, update rates, check health
 *
 * @param bridge Bridge handle
 * @param dt_ms Time delta
 * @return 0 on success, error code on failure
 */
int mirror_snn_update(mirror_snn_bridge_t* bridge, float dt_ms);

/**
 * @brief Reset bridge state
 *
 * WHAT: Clear all spike history and reset SNN
 * WHY:  Start fresh for new observation sequence
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int mirror_snn_reset(mirror_snn_bridge_t* bridge);

//=============================================================================
// Direct SNN Access
//=============================================================================

/**
 * @brief Get underlying SNN network
 *
 * WHAT: Direct access to SNN for advanced operations
 * WHY:  Enable custom SNN manipulation
 *
 * @param bridge Bridge handle
 * @return SNN network handle (do not destroy)
 */
snn_network_t* mirror_snn_get_network(mirror_snn_bridge_t* bridge);

/**
 * @brief Get SNN statistics
 *
 * @param bridge Bridge handle
 * @param stats Output SNN statistics
 * @return 0 on success, error code on failure
 */
int mirror_snn_get_snn_stats(
    const mirror_snn_bridge_t* bridge,
    snn_stats_t* stats
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MIRROR_SNN_BRIDGE_H */
