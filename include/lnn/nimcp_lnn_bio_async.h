/**
 * @file nimcp_lnn_bio_async.h
 * @brief Bio-async integration for Liquid Neural Networks
 *
 * WHAT: Bio-async messaging layer for LNN networks
 * WHY:  Enable inter-module communication for LNN state, training, and synchronization
 * HOW:  Register LNN as bio-async module, handle message passing, phase sync
 *
 * BIOLOGICAL GROUNDING:
 * - State broadcasts map to neuromodulator release (dopamine for completion signals)
 * - Tau updates map to metabolic signaling (time constant modulation)
 * - Phase sync enables oscillation-based coordination across modules
 * - Training events trigger system-wide plasticity coordination
 *
 * MESSAGE TYPES:
 * - LNN_BIO_MSG_STATE_BROADCAST: Current network state (x, tau, health)
 * - LNN_BIO_MSG_TAU_UPDATE: Time constant changed
 * - LNN_BIO_MSG_GRADIENT_READY: Gradients computed
 * - LNN_BIO_MSG_INSTABILITY: Detected instability (NaN, explosion, etc.)
 * - LNN_BIO_MSG_SYNC_REQUEST: Request phase synchronization
 * - LNN_BIO_MSG_SYNC_RESPONSE: Response to sync request
 * - LNN_BIO_MSG_TRAINING_EVENT: Training step/epoch event
 *
 * USAGE:
 * ```c
 * // Connect LNN network to bio-async
 * lnn_bio_async_connect(network, BIO_MODULE_LNN_CORE);
 *
 * // Broadcast current state to other modules
 * lnn_bio_async_broadcast_state(network, BIO_MODULE_ALL);
 *
 * // Request phase synchronization with oscillation module
 * lnn_bio_async_request_sync(network, BIO_OSC_GAMMA, 0.8f);
 * lnn_bio_async_wait_sync(network, 1000);  // Wait up to 1s
 *
 * // Register custom message handler
 * lnn_bio_async_register_handler(network, LNN_BIO_MSG_GRADIENT_READY,
 *     my_handler, user_data);
 *
 * // Process incoming messages
 * lnn_bio_async_process(network, 100);  // Process for up to 100ms
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-12-20
 */

#ifndef NIMCP_LNN_BIO_ASYNC_H
#define NIMCP_LNN_BIO_ASYNC_H

#include "lnn/nimcp_lnn_types.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_async.h"

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Bio-Async Message Types for LNN
 *===========================================================================*/

/**
 * @brief LNN-specific bio-async message types
 *
 * WHAT: Message types for LNN inter-module communication
 * WHY:  Different message types have different semantic meanings
 * HOW:  Enum-based categorization aligned with bio-async protocol
 */
typedef enum {
    LNN_BIO_MSG_STATE_BROADCAST = 0,    /**< Broadcast current state */
    LNN_BIO_MSG_TAU_UPDATE,             /**< Time constant changed */
    LNN_BIO_MSG_GRADIENT_READY,         /**< Gradients computed */
    LNN_BIO_MSG_INSTABILITY,            /**< Detected instability */
    LNN_BIO_MSG_SYNC_REQUEST,           /**< Request phase sync */
    LNN_BIO_MSG_SYNC_RESPONSE,          /**< Response to sync request */
    LNN_BIO_MSG_TRAINING_EVENT,         /**< Training step/epoch event */
    LNN_BIO_MSG_COUNT
} lnn_bio_msg_type_t;

/*=============================================================================
 * Message Structures
 *===========================================================================*/

/**
 * @brief State broadcast message
 *
 * WHAT: Broadcasts current LNN network state
 * WHY:  Enable other modules to monitor LNN dynamics
 * HOW:  Contains state vector, time constants, and health info
 */
typedef struct {
    uint32_t network_id;        /**< Network identifier */
    uint32_t layer_id;          /**< Layer identifier */
    float* state;               /**< Current state values [n_neurons] */
    uint32_t state_size;        /**< Number of state values */
    float* tau;                 /**< Current time constants [n_neurons] */
    uint32_t tau_size;          /**< Number of tau values */
    float t;                    /**< Current simulation time (ms) */
    lnn_state_health_t health;  /**< Network health status */
} lnn_bio_state_msg_t;

/**
 * @brief Phase synchronization message
 *
 * WHAT: Request or response for phase synchronization
 * WHY:  Enable oscillation-based coordination across modules
 * HOW:  Contains oscillation band, phase, frequency, coherence target
 */
typedef struct {
    uint32_t network_id;                /**< Network identifier */
    nimcp_oscillation_band_t band;      /**< Oscillation band */
    float phase;                        /**< Current phase [0, 2π] */
    float frequency;                    /**< Current frequency (Hz) */
    float coherence_target;             /**< Desired coherence level [0,1] */
} lnn_bio_sync_msg_t;

/**
 * @brief Training event message
 *
 * WHAT: Broadcasts training progress events
 * WHY:  Coordinate system-wide plasticity and learning
 * HOW:  Contains step, epoch, loss, LR, gradient norm
 */
typedef struct {
    uint32_t network_id;        /**< Network identifier */
    uint64_t step;              /**< Training step number */
    uint64_t epoch;             /**< Training epoch number */
    float loss;                 /**< Current loss value */
    float lr;                   /**< Current learning rate */
    float gradient_norm;        /**< Gradient L2 norm */
    uint8_t flags;              /**< Event flags (step/epoch complete) */
} lnn_bio_training_msg_t;

/**
 * @brief Training event flags
 */
#define LNN_TRAINING_FLAG_STEP_COMPLETE     (1 << 0)
#define LNN_TRAINING_FLAG_EPOCH_COMPLETE    (1 << 1)

/*=============================================================================
 * Connection API
 *===========================================================================*/

/**
 * @brief Connect LNN network to bio-async router
 *
 * WHAT: Register LNN network as bio-async module
 * WHY:  Enable inter-module messaging for LNN
 * HOW:  Register module with bio-router, allocate inbox/outbox
 *
 * @param network LNN network to connect
 * @param module_id Bio-async module ID (e.g., BIO_MODULE_LNN_CORE)
 * @return 0 on success, negative error code on failure
 *
 * BIOLOGICAL MAPPING:
 * - Registration maps to synaptic connection formation
 * - Module ID maps to neural population identity
 * - Inbox/outbox map to dendritic/axonal messaging
 *
 * ERRORS:
 * - LNN_ERROR_NULL_POINTER: network is NULL
 * - LNN_ERROR_INVALID_STATE: bio-async not initialized
 * - NIMCP_ERROR_NO_MEMORY: Failed to allocate inbox/outbox
 */
int lnn_bio_async_connect(lnn_network_t* network, uint16_t module_id);

/**
 * @brief Disconnect LNN network from bio-async router
 *
 * WHAT: Unregister LNN network from bio-async
 * WHY:  Clean up resources when LNN no longer needs messaging
 * HOW:  Unregister module, free inbox/outbox
 *
 * @param network LNN network to disconnect
 * @return 0 on success, negative error code on failure
 */
int lnn_bio_async_disconnect(lnn_network_t* network);

/**
 * @brief Check if LNN network is connected to bio-async
 *
 * @param network LNN network to check
 * @return true if connected, false otherwise
 */
bool lnn_bio_async_is_connected(const lnn_network_t* network);

/*=============================================================================
 * Broadcasting API
 *===========================================================================*/

/**
 * @brief Broadcast current LNN state to other modules
 *
 * WHAT: Send current network state to target module(s)
 * WHY:  Enable other modules to monitor LNN dynamics
 * HOW:  Package state/tau/health into message, route via bio-async
 *
 * @param network LNN network
 * @param target_module Target module ID (BIO_MODULE_ALL for broadcast)
 * @return 0 on success, negative error code on failure
 *
 * BIOLOGICAL MAPPING:
 * - State broadcast maps to neuromodulator release (dopamine)
 * - Target routing maps to axonal projection specificity
 * - Message delivery maps to synaptic transmission
 *
 * CHANNEL: DOPAMINE (completion/state update signal)
 */
int lnn_bio_async_broadcast_state(lnn_network_t* network, uint16_t target_module);

/**
 * @brief Broadcast time constant update
 *
 * WHAT: Notify other modules that tau has changed
 * WHY:  Tau changes affect system timescales, need coordination
 * HOW:  Send tau values to target modules
 *
 * @param network LNN network
 * @param target_module Target module ID (BIO_MODULE_ALL for broadcast)
 * @return 0 on success, negative error code on failure
 *
 * BIOLOGICAL MAPPING:
 * - Tau update maps to metabolic signaling
 * - Time constant changes affect system-wide dynamics
 *
 * CHANNEL: SEROTONIN (slow state change signal)
 */
int lnn_bio_async_broadcast_tau(lnn_network_t* network, uint16_t target_module);

/**
 * @brief Broadcast training event
 *
 * WHAT: Send training progress event to other modules
 * WHY:  Coordinate system-wide plasticity and learning
 * HOW:  Package training metrics, route via bio-async
 *
 * @param network LNN network
 * @param event Training event message
 * @return 0 on success, negative error code on failure
 *
 * BIOLOGICAL MAPPING:
 * - Training events map to learning signals
 * - Broadcasts coordinate multi-region plasticity
 *
 * CHANNEL: DOPAMINE (reward/completion signal)
 */
int lnn_bio_async_broadcast_training_event(
    lnn_network_t* network,
    const lnn_bio_training_msg_t* event
);

/*=============================================================================
 * Phase Synchronization API
 *===========================================================================*/

/**
 * @brief Request phase synchronization with other modules
 *
 * WHAT: Request oscillation-based synchronization
 * WHY:  Coordinate LNN dynamics with other oscillating modules
 * HOW:  Send sync request, wait for coherence threshold
 *
 * @param network LNN network
 * @param band Oscillation band to synchronize (GAMMA, THETA, etc.)
 * @param coherence_target Desired coherence level [0,1]
 * @return 0 on success, negative error code on failure
 *
 * BIOLOGICAL MAPPING:
 * - Phase sync maps to neural oscillation coupling
 * - Coherence threshold maps to Kuramoto order parameter
 * - Band selection determines timescale of coordination
 *
 * CHANNEL: ACETYLCHOLINE (attention/fast coordination)
 */
int lnn_bio_async_request_sync(
    lnn_network_t* network,
    nimcp_oscillation_band_t band,
    float coherence_target
);

/**
 * @brief Wait for phase synchronization to complete
 *
 * WHAT: Block until phase sync achieves coherence
 * WHY:  Ensure coordination before proceeding
 * HOW:  Monitor sync responses, check coherence threshold
 *
 * @param network LNN network
 * @param timeout_ms Timeout in milliseconds (0 = use band default)
 * @return 0 on success (coherent), negative error code on timeout/failure
 */
int lnn_bio_async_wait_sync(lnn_network_t* network, int timeout_ms);

/*=============================================================================
 * Message Handler API
 *===========================================================================*/

/**
 * @brief Message handler callback type
 *
 * @param network LNN network receiving message
 * @param type Message type
 * @param msg Message payload
 * @param msg_size Message size in bytes
 * @param user_data User-provided context
 * @return 0 on success, negative error code on failure
 */
typedef int (*lnn_bio_msg_handler_t)(
    lnn_network_t* network,
    lnn_bio_msg_type_t type,
    const void* msg,
    size_t msg_size,
    void* user_data
);

/**
 * @brief Register message handler for specific message type
 *
 * WHAT: Register callback for incoming messages
 * WHY:  Enable custom handling of bio-async messages
 * HOW:  Store handler in network context, invoke on message arrival
 *
 * @param network LNN network
 * @param type Message type to handle
 * @param handler Handler callback function
 * @param user_data User context passed to handler
 * @return 0 on success, negative error code on failure
 *
 * NOTE: Multiple handlers can be registered for the same message type
 */
int lnn_bio_async_register_handler(
    lnn_network_t* network,
    lnn_bio_msg_type_t type,
    lnn_bio_msg_handler_t handler,
    void* user_data
);

/**
 * @brief Process pending bio-async messages
 *
 * WHAT: Process messages in network's inbox
 * WHY:  Handle incoming messages from other modules
 * HOW:  Dequeue messages, invoke registered handlers
 *
 * @param network LNN network
 * @param timeout_ms Maximum time to spend processing (0 = non-blocking)
 * @return Number of messages processed, or negative error code
 *
 * USAGE:
 * - Call periodically in network update loop
 * - Or call with timeout in dedicated message thread
 */
int lnn_bio_async_process(lnn_network_t* network, int timeout_ms);

/*=============================================================================
 * Utility Functions
 *===========================================================================*/

/**
 * @brief Get message type name as string
 *
 * @param type Message type
 * @return String name (static, do not free)
 */
const char* lnn_bio_msg_type_to_string(lnn_bio_msg_type_t type);

/**
 * @brief Get LNN bio-async statistics
 *
 * @param network LNN network
 * @param messages_sent Output: total messages sent
 * @param messages_received Output: total messages received
 * @param messages_dropped Output: messages dropped (inbox full)
 * @return 0 on success, negative error code on failure
 */
int lnn_bio_async_get_stats(
    const lnn_network_t* network,
    uint64_t* messages_sent,
    uint64_t* messages_received,
    uint64_t* messages_dropped
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LNN_BIO_ASYNC_H */
