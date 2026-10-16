/**
 * @file nimcp_msg_router.h
 * @brief Message routing for collective cognition protocol
 *
 * WHAT: Routes messages to appropriate handlers based on type
 * WHY: Decouple message parsing from handling logic
 * HOW: Handler registration + dispatch based on message type
 *
 * DESIGN:
 * - Fast path messages dispatched directly (minimal overhead)
 * - Protobuf messages decoded then dispatched
 * - Per-type handler registration
 * - Default handler for unregistered types
 *
 * @author NIMCP Team
 * @date 2025-01-01
 */

#ifndef NIMCP_MSG_ROUTER_H
#define NIMCP_MSG_ROUTER_H

#include "networking/protocol/nimcp_msg_framing.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Constants
 *===========================================================================*/

/** Maximum number of registered handlers */
#define NIMCP_MAX_MSG_HANDLERS      64

/** Maximum number of pending messages in queue */
#define NIMCP_MSG_QUEUE_SIZE        256

/*=============================================================================
 * Types
 *===========================================================================*/

/**
 * @brief Message handler callback signature
 *
 * @param header Message header
 * @param payload Message payload (raw bytes)
 * @param payload_len Payload length
 * @param user_data User-provided context
 * @return 0 on success, negative on error
 */
typedef int (*nimcp_msg_handler_fn)(
    const nimcp_msg_header_t* header,
    const uint8_t* payload,
    size_t payload_len,
    void* user_data
);

/**
 * @brief Fast message handler callback signature
 *
 * @param msg Fast message (24 bytes)
 * @param user_data User-provided context
 * @return 0 on success, negative on error
 */
typedef int (*nimcp_fast_msg_handler_fn)(
    const nimcp_fast_msg_t* msg,
    void* user_data
);

/**
 * @brief Handler registration entry
 */
typedef struct {
    nimcp_msg_type_t msg_type;
    nimcp_msg_handler_fn handler;
    nimcp_fast_msg_handler_fn fast_handler;
    void* user_data;
    bool is_fast_path;
} nimcp_msg_handler_entry_t;

/**
 * @brief Router configuration
 */
typedef struct {
    bool enable_queue;              /**< Queue messages instead of immediate dispatch */
    uint32_t queue_size;            /**< Queue capacity (if enabled) */
    bool enable_stats;              /**< Track routing statistics */
    nimcp_msg_handler_fn default_handler;  /**< Handler for unregistered types */
    void* default_user_data;
} nimcp_msg_router_config_t;

/**
 * @brief Router statistics
 */
typedef struct {
    uint64_t messages_routed;
    uint64_t fast_messages_routed;
    uint64_t protobuf_messages_routed;
    uint64_t handler_errors;
    uint64_t unhandled_messages;
    uint64_t queue_overflows;
    uint64_t bytes_routed;
} nimcp_msg_router_stats_t;

/**
 * @brief Opaque router handle
 */
typedef struct nimcp_msg_router nimcp_msg_router_t;

/*=============================================================================
 * Configuration API
 *===========================================================================*/

/**
 * @brief Get default router configuration
 *
 * @return Default configuration
 */
nimcp_msg_router_config_t nimcp_msg_router_default_config(void);

/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

/**
 * @brief Create message router
 *
 * @param config Configuration (NULL for defaults)
 * @return Router handle or NULL on failure
 */
nimcp_msg_router_t* nimcp_msg_router_create(
    const nimcp_msg_router_config_t* config
);

/**
 * @brief Destroy router and free resources
 *
 * @param router Router to destroy
 */
void nimcp_msg_router_destroy(nimcp_msg_router_t* router);

/**
 * @brief Reset router state
 *
 * Clears queue and resets statistics, but keeps handlers registered.
 *
 * @param router Router to reset
 * @return 0 on success, -1 on error
 */
int nimcp_msg_router_reset(nimcp_msg_router_t* router);

/*=============================================================================
 * Handler Registration API
 *===========================================================================*/

/**
 * @brief Register handler for message type
 *
 * @param router Router handle
 * @param msg_type Message type to handle
 * @param handler Handler callback
 * @param user_data Context passed to handler
 * @return 0 on success, -1 on error
 */
int nimcp_msg_router_register(
    nimcp_msg_router_t* router,
    nimcp_msg_type_t msg_type,
    nimcp_msg_handler_fn handler,
    void* user_data
);

/**
 * @brief Register fast path handler for message type
 *
 * Fast handlers receive pre-parsed nimcp_fast_msg_t.
 *
 * @param router Router handle
 * @param msg_type Message type (must be fast path type)
 * @param handler Fast handler callback
 * @param user_data Context passed to handler
 * @return 0 on success, -1 on error
 */
int nimcp_msg_router_register_fast(
    nimcp_msg_router_t* router,
    nimcp_msg_type_t msg_type,
    nimcp_fast_msg_handler_fn handler,
    void* user_data
);

/**
 * @brief Unregister handler for message type
 *
 * @param router Router handle
 * @param msg_type Message type to unregister
 * @return 0 on success, -1 if not found
 */
int nimcp_msg_router_unregister(
    nimcp_msg_router_t* router,
    nimcp_msg_type_t msg_type
);

/**
 * @brief Check if handler is registered for message type
 *
 * @param router Router handle
 * @param msg_type Message type to check
 * @return true if handler registered
 */
bool nimcp_msg_router_has_handler(
    const nimcp_msg_router_t* router,
    nimcp_msg_type_t msg_type
);

/*=============================================================================
 * Routing API
 *===========================================================================*/

/**
 * @brief Route a raw message
 *
 * Parses header, validates, and dispatches to registered handler.
 *
 * @param router Router handle
 * @param data Raw message bytes (header + payload)
 * @param len Total message length
 * @return 0 on success, negative on error
 */
int nimcp_msg_router_route(
    nimcp_msg_router_t* router,
    const uint8_t* data,
    size_t len
);

/**
 * @brief Route a fast message directly
 *
 * Skips header parsing for pre-constructed fast messages.
 *
 * @param router Router handle
 * @param msg Fast message
 * @return 0 on success, negative on error
 */
int nimcp_msg_router_route_fast(
    nimcp_msg_router_t* router,
    const nimcp_fast_msg_t* msg
);

/**
 * @brief Route a pre-parsed message
 *
 * For when header is already parsed.
 *
 * @param router Router handle
 * @param header Parsed header
 * @param payload Payload bytes
 * @param payload_len Payload length
 * @return 0 on success, negative on error
 */
int nimcp_msg_router_route_parsed(
    nimcp_msg_router_t* router,
    const nimcp_msg_header_t* header,
    const uint8_t* payload,
    size_t payload_len
);

/*=============================================================================
 * Queue API (if enabled)
 *===========================================================================*/

/**
 * @brief Queue a message for later processing
 *
 * @param router Router handle
 * @param data Raw message bytes
 * @param len Message length
 * @return 0 on success, -1 if queue full
 */
int nimcp_msg_router_queue(
    nimcp_msg_router_t* router,
    const uint8_t* data,
    size_t len
);

/**
 * @brief Process queued messages
 *
 * @param router Router handle
 * @param max_messages Max messages to process (0 = all)
 * @return Number of messages processed, negative on error
 */
int nimcp_msg_router_process_queue(
    nimcp_msg_router_t* router,
    uint32_t max_messages
);

/**
 * @brief Get number of queued messages
 *
 * @param router Router handle
 * @return Queue depth
 */
uint32_t nimcp_msg_router_queue_depth(const nimcp_msg_router_t* router);

/**
 * @brief Clear message queue
 *
 * @param router Router handle
 */
void nimcp_msg_router_clear_queue(nimcp_msg_router_t* router);

/*=============================================================================
 * Statistics API
 *===========================================================================*/

/**
 * @brief Get router statistics
 *
 * @param router Router handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int nimcp_msg_router_get_stats(
    const nimcp_msg_router_t* router,
    nimcp_msg_router_stats_t* stats
);

/**
 * @brief Reset router statistics
 *
 * @param router Router handle
 */
void nimcp_msg_router_reset_stats(nimcp_msg_router_t* router);

/*=============================================================================
 * Debug API
 *===========================================================================*/

/**
 * @brief Dump router state for debugging
 *
 * @param router Router handle
 */
void nimcp_msg_router_dump(const nimcp_msg_router_t* router);

/*=============================================================================
 * Health Agent Integration
 *===========================================================================*/

/**
 * @brief Set health agent for message router heartbeats
 *
 * WHAT: Associate health agent with message router for health monitoring
 * WHY:  Enables heartbeat reporting during message processing
 * HOW:  Store agent reference, send heartbeats during dispatch
 *
 * @param agent Health agent instance (can be NULL to disable)
 */
struct nimcp_health_agent;
void nimcp_msg_router_set_health_agent(struct nimcp_health_agent* agent);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MSG_ROUTER_H */
