/**
 * @file nimcp_snn_bio_async.h
 * @brief Bio-async integration for Spiking Neural Networks
 *
 * WHAT: Inter-module messaging for SNN state and spike events
 * WHY:  Enable SNN to communicate with other NIMCP modules
 * HOW:  Bio-router registration, message handlers, spike broadcast
 *
 * INTEGRATION:
 * - Uses existing bio-async infrastructure
 * - Module IDs: BIO_MODULE_SNN_CORE through BIO_MODULE_SNN_IMMUNE
 * - Supports spike event broadcasting and phase synchronization
 *
 * BIOLOGICAL BASIS:
 * - Spike propagation across brain regions
 * - Neural oscillation synchronization
 * - Cross-regional communication
 *
 * @author NIMCP Team
 * @date 2024
 */

#ifndef NIMCP_SNN_BIO_ASYNC_H
#define NIMCP_SNN_BIO_ASYNC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "snn/nimcp_snn_types.h"
#include "snn/nimcp_snn_network.h"
#include "async/nimcp_bio_async.h"

//=============================================================================
// Message Types
//=============================================================================

/**
 * @brief SNN bio-async message types
 *
 * WHAT: Types of messages SNN can send/receive
 * WHY:  Route messages to correct handlers
 * HOW:  Enum-based message typing
 */
typedef enum {
    SNN_BIO_MSG_SPIKE_EVENT = 0,     /**< Spike event (single or batch) */
    SNN_BIO_MSG_STATE_BROADCAST,     /**< Network state broadcast */
    SNN_BIO_MSG_RATE_UPDATE,         /**< Firing rate update */
    SNN_BIO_MSG_SYNC_REQUEST,        /**< Phase sync request */
    SNN_BIO_MSG_SYNC_RESPONSE,       /**< Phase sync response */
    SNN_BIO_MSG_TRAINING_EVENT,      /**< Training-related event */
    SNN_BIO_MSG_STDP_EVENT,          /**< STDP weight update event */
    SNN_BIO_MSG_POPULATION_ACTIVITY, /**< Population coding activity */
    SNN_BIO_MSG_INSTABILITY,         /**< Network instability alert */
    SNN_BIO_MSG_IMMUNE_ALERT,        /**< Immune system alert */
    SNN_BIO_MSG_COUNT
} snn_bio_msg_type_t;

//=============================================================================
// Message Structures
//=============================================================================

/**
 * @brief Spike event message
 *
 * WHAT: Information about spike events
 * WHY:  Broadcast spike activity to other modules
 * HOW:  Neuron ID, timing, optional metadata
 */
typedef struct {
    uint32_t network_id;       /**< Source network ID */
    uint32_t population_id;    /**< Source population ID */
    uint32_t neuron_id;        /**< Spiking neuron ID */
    float spike_time;          /**< Spike time (ms) */
    float membrane_v;          /**< Membrane potential at spike */
    bool is_burst;             /**< Part of a burst */
} snn_bio_spike_msg_t;

/**
 * @brief Network state message
 *
 * WHAT: Full network state snapshot
 * WHY:  Share state with other modules
 * HOW:  Population rates, activity patterns
 */
typedef struct {
    uint32_t network_id;          /**< Network ID */
    uint32_t n_populations;       /**< Number of populations */
    const float* rates;           /**< Firing rates per population */
    uint32_t rates_size;          /**< Size of rates array */
    float t;                      /**< Current simulation time */
    uint32_t total_spikes;        /**< Total spikes in this step */
    float mean_rate;              /**< Mean network firing rate */
} snn_bio_state_msg_t;

/**
 * @brief STDP event message
 *
 * WHAT: Weight change from STDP
 * WHY:  Notify plasticity system
 * HOW:  Pre/post IDs, weight change
 */
typedef struct {
    uint32_t network_id;       /**< Network ID */
    uint32_t pre_id;           /**< Presynaptic neuron ID */
    uint32_t post_id;          /**< Postsynaptic neuron ID */
    float delta_w;             /**< Weight change */
    float new_weight;          /**< New weight value */
    float dt;                  /**< Spike timing difference */
} snn_bio_stdp_msg_t;

/**
 * @brief Phase synchronization message
 *
 * WHAT: Phase sync request/response
 * WHY:  Coordinate oscillations across modules
 * HOW:  Band, phase, coherence target
 */
typedef struct {
    uint32_t network_id;             /**< Network ID */
    nimcp_oscillation_band_t band;   /**< Oscillation band */
    float phase;                     /**< Current phase (radians) */
    float frequency;                 /**< Frequency (Hz) */
    float coherence_target;          /**< Target coherence */
} snn_bio_sync_msg_t;

/**
 * @brief Training event message
 *
 * WHAT: Training-related event
 * WHY:  Coordinate training across modules
 * HOW:  Event type, metrics
 */
typedef struct {
    uint32_t network_id;          /**< Network ID */
    snn_train_mode_t mode;        /**< Training mode */
    float loss;                   /**< Current loss */
    float learning_rate;          /**< Current learning rate */
    uint64_t step;                /**< Training step */
    uint32_t weight_updates;      /**< Number of weight updates */
} snn_bio_training_msg_t;

/**
 * @brief Population activity message
 *
 * WHAT: Population-coded activity
 * WHY:  Share population coding with other modules
 * HOW:  Neuron IDs and firing times
 */
typedef struct {
    uint32_t network_id;          /**< Network ID */
    uint32_t population_id;       /**< Population ID */
    uint32_t n_active;            /**< Number of active neurons */
    const uint32_t* neuron_ids;   /**< Active neuron IDs */
    const float* spike_times;     /**< Spike times */
    float window_start;           /**< Time window start */
    float window_end;             /**< Time window end */
} snn_bio_population_msg_t;

//=============================================================================
// Message Handler Type
//=============================================================================

/**
 * @brief SNN bio-async message handler callback
 *
 * @param network Target network
 * @param type Message type
 * @param msg Message data
 * @param msg_size Message size in bytes
 * @param user_data User context
 * @return 0 on success, error code on failure
 */
typedef int (*snn_bio_msg_handler_t)(
    snn_network_t* network,
    snn_bio_msg_type_t type,
    const void* msg,
    size_t msg_size,
    void* user_data
);

//=============================================================================
// Connection API
//=============================================================================

/**
 * @brief Connect SNN network to bio-async
 *
 * WHAT: Register network with bio-router
 * WHY:  Enable inter-module messaging
 * HOW:  Create context, register module
 *
 * @param network SNN network
 * @param module_id Bio-async module ID (use BIO_MODULE_SNN_CORE)
 * @return 0 on success, error code on failure
 */
int snn_bio_async_connect(snn_network_t* network, uint16_t module_id);

/**
 * @brief Disconnect SNN network from bio-async
 *
 * WHAT: Unregister from bio-router
 * WHY:  Clean shutdown
 * HOW:  Cleanup context, unregister
 *
 * @param network SNN network
 * @return 0 on success, error code on failure
 */
int snn_bio_async_disconnect(snn_network_t* network);

/**
 * @brief Check if network is connected to bio-async
 *
 * @param network SNN network
 * @return true if connected
 */
bool snn_bio_async_is_connected(const snn_network_t* network);

//=============================================================================
// Broadcasting API
//=============================================================================

/**
 * @brief Broadcast spike event
 *
 * WHAT: Send spike event to other modules
 * WHY:  Share spike activity
 * HOW:  Package and broadcast via bio-router
 *
 * @param network SNN network
 * @param event Spike event message
 * @return 0 on success, error code on failure
 */
int snn_bio_async_broadcast_spike(
    snn_network_t* network,
    const snn_bio_spike_msg_t* event
);

/**
 * @brief Broadcast network state
 *
 * WHAT: Send full state to other modules
 * WHY:  Share state snapshot
 * HOW:  Package and broadcast
 *
 * @param network SNN network
 * @param target_module Target module ID (0 = broadcast all)
 * @return 0 on success, error code on failure
 */
int snn_bio_async_broadcast_state(snn_network_t* network, uint16_t target_module);

/**
 * @brief Broadcast STDP event
 *
 * WHAT: Send weight update event
 * WHY:  Coordinate with plasticity system
 * HOW:  Package and broadcast
 *
 * @param network SNN network
 * @param event STDP event message
 * @return 0 on success, error code on failure
 */
int snn_bio_async_broadcast_stdp(
    snn_network_t* network,
    const snn_bio_stdp_msg_t* event
);

/**
 * @brief Broadcast training event
 *
 * WHAT: Send training event
 * WHY:  Coordinate training across modules
 * HOW:  Package and broadcast
 *
 * @param network SNN network
 * @param event Training event message
 * @return 0 on success, error code on failure
 */
int snn_bio_async_broadcast_training(
    snn_network_t* network,
    const snn_bio_training_msg_t* event
);

/**
 * @brief Broadcast population activity
 *
 * WHAT: Send population activity
 * WHY:  Share population coding
 * HOW:  Package and broadcast
 *
 * @param network SNN network
 * @param event Population activity message
 * @return 0 on success, error code on failure
 */
int snn_bio_async_broadcast_population(
    snn_network_t* network,
    const snn_bio_population_msg_t* event
);

//=============================================================================
// Phase Synchronization API
//=============================================================================

/**
 * @brief Request phase synchronization
 *
 * WHAT: Request sync with other modules
 * WHY:  Coordinate oscillations
 * HOW:  Send sync request via bio-router
 *
 * @param network SNN network
 * @param band Oscillation band
 * @param coherence_target Target coherence (0-1)
 * @return 0 on success, error code on failure
 */
int snn_bio_async_request_sync(
    snn_network_t* network,
    nimcp_oscillation_band_t band,
    float coherence_target
);

/**
 * @brief Wait for phase synchronization
 *
 * WHAT: Wait until phase sync achieved
 * WHY:  Blocking sync operation
 * HOW:  Poll until coherence target reached
 *
 * @param network SNN network
 * @param timeout_ms Timeout in milliseconds (0 = forever)
 * @return 0 on success, error code on timeout
 */
int snn_bio_async_wait_sync(snn_network_t* network, int timeout_ms);

//=============================================================================
// Message Handler API
//=============================================================================

/**
 * @brief Register message handler
 *
 * WHAT: Register callback for message type
 * WHY:  React to incoming messages
 * HOW:  Store handler in lookup table
 *
 * @param network SNN network
 * @param type Message type to handle
 * @param handler Callback function
 * @param user_data User context passed to handler
 * @return 0 on success, error code on failure
 */
int snn_bio_async_register_handler(
    snn_network_t* network,
    snn_bio_msg_type_t type,
    snn_bio_msg_handler_t handler,
    void* user_data
);

/**
 * @brief Process pending messages
 *
 * WHAT: Process messages in inbox
 * WHY:  Handle incoming messages
 * HOW:  Dequeue and invoke handlers
 *
 * @param network SNN network
 * @param timeout_ms Timeout for blocking (0 = non-blocking)
 * @return Number of messages processed
 */
int snn_bio_async_process(snn_network_t* network, int timeout_ms);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get message type name
 *
 * @param type Message type
 * @return Human-readable name
 */
const char* snn_bio_msg_type_to_string(snn_bio_msg_type_t type);

/**
 * @brief Get bio-async statistics
 *
 * @param network SNN network
 * @param messages_sent Output for sent count
 * @param messages_received Output for received count
 * @param messages_dropped Output for dropped count
 * @return 0 on success, error code on failure
 */
int snn_bio_async_get_stats(
    const snn_network_t* network,
    uint64_t* messages_sent,
    uint64_t* messages_received,
    uint64_t* messages_dropped
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SNN_BIO_ASYNC_H */
