/**
 * @file nimcp_inter_layer_router.h
 * @brief Inter-Layer Router - Routes messages between layers
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Routes messages between different layers in the hierarchy
 * WHY:  Enables communication between non-adjacent and adjacent layers
 * HOW:  Maintains routing tables, message queues, handles bottom-up/top-down flow
 *
 * ROUTING RULES:
 * ==============
 * 1. Bottom-up messages flow from lower to higher layers
 * 2. Top-down messages flow from higher to lower layers
 * 3. Adjacent layers have direct connections
 * 4. Non-adjacent layers route through intermediaries OR direct shortcuts
 * 5. Priority determines queue ordering
 *
 * MESSAGE FLOW:
 * =============
 *   Source Module → Inter-Layer Router → Target Layer Queue → Target Module
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_INTER_LAYER_ROUTER_H
#define NIMCP_INTER_LAYER_ROUTER_H

#include "nimcp_layer_types.h"
#include "nimcp_layer_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Opaque Handle
//=============================================================================

/**
 * @brief Opaque inter-layer router handle
 */
typedef struct nimcp_inter_layer_router_struct* nimcp_inter_layer_router_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Routing mode
 */
typedef enum {
    NIMCP_ROUTE_MODE_DIRECT = 0,    /**< Direct routing (shortcuts allowed) */
    NIMCP_ROUTE_MODE_HIERARCHICAL,  /**< Strict hierarchical (through intermediaries) */
    NIMCP_ROUTE_MODE_HYBRID         /**< Direct for adjacent, hierarchical for distant */
} nimcp_route_mode_t;

/**
 * @brief Router configuration
 */
typedef struct {
    nimcp_route_mode_t route_mode;  /**< Routing mode */
    uint32_t default_queue_depth;   /**< Default message queue depth */
    uint32_t max_pending_messages;  /**< Max pending messages globally */
    uint32_t process_batch_size;    /**< Messages to process per update */
    bool enable_priority_queuing;   /**< Enable priority-based queuing */
    bool enable_message_logging;    /**< Log all routed messages */
    bool enable_latency_tracking;   /**< Track message latency */
    uint32_t timeout_ms;            /**< Message timeout (0 = no timeout) */
} nimcp_inter_layer_router_config_t;

/**
 * @brief Router statistics
 */
typedef struct {
    uint64_t messages_routed;       /**< Total messages routed */
    uint64_t messages_dropped;      /**< Messages dropped (timeout/queue full) */
    uint64_t bottom_up_count;       /**< Bottom-up messages */
    uint64_t top_down_count;        /**< Top-down messages */
    uint64_t direct_routes;         /**< Direct (shortcut) routes used */
    uint64_t hierarchical_routes;   /**< Hierarchical routes used */
    float avg_latency_us;           /**< Average routing latency */
    float max_latency_us;           /**< Maximum routing latency */
    uint32_t current_queue_depth;   /**< Current total queue depth */
} nimcp_inter_layer_router_stats_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default router configuration
 */
NIMCP_EXPORT nimcp_inter_layer_router_config_t nimcp_inter_layer_router_default_config(void);

/**
 * @brief Create inter-layer router
 *
 * WHAT: Creates the message router for inter-layer communication
 * WHY:  Central point for all cross-layer message routing
 * HOW:  Creates queues, initializes routing tables
 *
 * @param config Configuration (NULL for defaults)
 * @param registry Layer registry for routing info
 * @return Router handle or NULL on failure
 */
NIMCP_EXPORT nimcp_inter_layer_router_t nimcp_inter_layer_router_create(
    const nimcp_inter_layer_router_config_t* config,
    nimcp_layer_registry_t registry
);

/**
 * @brief Destroy router
 *
 * @param router Router to destroy (NULL safe)
 */
NIMCP_EXPORT void nimcp_inter_layer_router_destroy(nimcp_inter_layer_router_t router);

/**
 * @brief Reset router state
 *
 * Clears all queues and resets statistics.
 *
 * @param router Router handle
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_inter_layer_router_reset(
    nimcp_inter_layer_router_t router
);

//=============================================================================
// Message Routing API
//=============================================================================

/**
 * @brief Route a message to target layer
 *
 * WHAT: Sends a message to a target layer
 * WHY:  Primary inter-layer communication mechanism
 * HOW:  Determines route, queues message for delivery
 *
 * @param router Router handle
 * @param msg Message to route (ownership transferred if successful)
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_inter_layer_router_route(
    nimcp_inter_layer_router_t router,
    nimcp_layer_msg_t* msg
);

/**
 * @brief Route message with explicit direction
 *
 * @param router Router handle
 * @param msg Message to route
 * @param direction Explicit direction (bottom-up or top-down)
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_inter_layer_router_route_directed(
    nimcp_inter_layer_router_t router,
    nimcp_layer_msg_t* msg,
    nimcp_msg_direction_t direction
);

/**
 * @brief Broadcast message to all connected layers
 *
 * @param router Router handle
 * @param source_layer Source layer
 * @param msg Message to broadcast (cloned for each target)
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_inter_layer_router_broadcast(
    nimcp_inter_layer_router_t router,
    nimcp_layer_id_t source_layer,
    const nimcp_layer_msg_t* msg
);

/**
 * @brief Broadcast to specific direction only
 *
 * @param router Router handle
 * @param source_layer Source layer
 * @param msg Message to broadcast
 * @param direction Direction to broadcast (bottom-up or top-down)
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_inter_layer_router_broadcast_directed(
    nimcp_inter_layer_router_t router,
    nimcp_layer_id_t source_layer,
    const nimcp_layer_msg_t* msg,
    nimcp_msg_direction_t direction
);

//=============================================================================
// Message Processing API
//=============================================================================

/**
 * @brief Process pending messages for a layer
 *
 * WHAT: Delivers queued messages to layer modules
 * WHY:  Modules receive messages during their update cycle
 * HOW:  Dequeues messages, invokes module handlers
 *
 * @param router Router handle
 * @param layer_id Target layer
 * @param max_messages Maximum messages to process (0 = all)
 * @param processed_out Output: number of messages processed
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_inter_layer_router_process_layer(
    nimcp_inter_layer_router_t router,
    nimcp_layer_id_t layer_id,
    uint32_t max_messages,
    uint32_t* processed_out
);

/**
 * @brief Process all pending messages
 *
 * @param router Router handle
 * @param max_messages Maximum messages to process globally
 * @param processed_out Output: number of messages processed
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_inter_layer_router_process_all(
    nimcp_inter_layer_router_t router,
    uint32_t max_messages,
    uint32_t* processed_out
);

/**
 * @brief Peek at next message for a layer (doesn't remove)
 *
 * @param router Router handle
 * @param layer_id Layer ID
 * @param msg_out Output: message info (don't modify)
 * @return NIMCP_LAYER_OK, NIMCP_LAYER_ERR_QUEUE_EMPTY, or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_inter_layer_router_peek(
    nimcp_inter_layer_router_t router,
    nimcp_layer_id_t layer_id,
    const nimcp_layer_msg_t** msg_out
);

/**
 * @brief Get queue depth for a layer
 *
 * @param router Router handle
 * @param layer_id Layer ID
 * @return Queue depth or -1 on error
 */
NIMCP_EXPORT int nimcp_inter_layer_router_get_queue_depth(
    nimcp_inter_layer_router_t router,
    nimcp_layer_id_t layer_id
);

//=============================================================================
// Routing Control API
//=============================================================================

/**
 * @brief Enable/disable a route
 *
 * @param router Router handle
 * @param source Source layer
 * @param target Target layer
 * @param enabled Enable or disable
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_inter_layer_router_set_route_enabled(
    nimcp_inter_layer_router_t router,
    nimcp_layer_id_t source,
    nimcp_layer_id_t target,
    bool enabled
);

/**
 * @brief Set coupling strength for a connection
 *
 * Affects message propagation - lower strength may drop/delay messages.
 *
 * @param router Router handle
 * @param source Source layer
 * @param target Target layer
 * @param strength Coupling strength (0-1)
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_inter_layer_router_set_coupling(
    nimcp_inter_layer_router_t router,
    nimcp_layer_id_t source,
    nimcp_layer_id_t target,
    float strength
);

/**
 * @brief Check if route exists and is enabled
 *
 * @param router Router handle
 * @param source Source layer
 * @param target Target layer
 * @return true if route is available
 */
NIMCP_EXPORT bool nimcp_inter_layer_router_route_available(
    nimcp_inter_layer_router_t router,
    nimcp_layer_id_t source,
    nimcp_layer_id_t target
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get router statistics
 *
 * @param router Router handle
 * @param stats_out Output: statistics
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_inter_layer_router_get_stats(
    nimcp_inter_layer_router_t router,
    nimcp_inter_layer_router_stats_t* stats_out
);

/**
 * @brief Reset router statistics
 *
 * @param router Router handle
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_inter_layer_router_reset_stats(
    nimcp_inter_layer_router_t router
);

/**
 * @brief Get routing path between layers
 *
 * @param router Router handle
 * @param source Source layer
 * @param target Target layer
 * @param path_out Output: array of layer IDs in path (caller provides)
 * @param max_path_len Size of path_out array
 * @param path_len_out Output: actual path length
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_inter_layer_router_get_path(
    nimcp_inter_layer_router_t router,
    nimcp_layer_id_t source,
    nimcp_layer_id_t target,
    nimcp_layer_id_t* path_out,
    size_t max_path_len,
    size_t* path_len_out
);

//=============================================================================
// Callback API
//=============================================================================

/**
 * @brief Message routing callback
 */
typedef void (*nimcp_route_callback_t)(
    nimcp_layer_id_t source,
    nimcp_layer_id_t target,
    const nimcp_layer_msg_t* msg,
    void* user_data
);

/**
 * @brief Set callback for message routing events
 *
 * @param router Router handle
 * @param callback Callback function (NULL to clear)
 * @param user_data User data passed to callback
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_inter_layer_router_set_callback(
    nimcp_inter_layer_router_t router,
    nimcp_route_callback_t callback,
    void* user_data
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_INTER_LAYER_ROUTER_H */
