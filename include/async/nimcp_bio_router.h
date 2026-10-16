/**
 * @file nimcp_bio_router.h
 * @brief Bio-async message router for inter-module communication
 *
 * WHAT: Central message routing infrastructure for bio-async messaging
 * WHY:  Decouples modules by routing messages without direct dependencies
 * HOW:  Modules register handlers; router dispatches based on message type
 *
 * ARCHITECTURE:
 * ┌─────────────────────────────────────────────────────────────────────┐
 * │                        BIO MESSAGE ROUTER                           │
 * ├─────────────────────────────────────────────────────────────────────┤
 * │  ┌─────────┐    ┌─────────────┐    ┌─────────────┐    ┌─────────┐  │
 * │  │ Module A│───▶│   Outbox    │───▶│   Router    │───▶│  Inbox  │  │
 * │  └─────────┘    │  (per-mod)  │    │  (central)  │    │(per-mod)│  │
 * │                 └─────────────┘    └─────────────┘    └────┬────┘  │
 * │                                                            │       │
 * │                                          ┌─────────────────▼────┐  │
 * │                                          │      Module B        │  │
 * │                                          │  (handler invoked)   │  │
 * │                                          └──────────────────────┘  │
 * └─────────────────────────────────────────────────────────────────────┘
 *
 * USAGE:
 * 1. Initialize router: bio_router_init()
 * 2. Register module: bio_router_register_module()
 * 3. Register handlers: bio_router_register_handler()
 * 4. Send messages: bio_router_send() or bio_router_send_async()
 * 5. Process inbox: bio_router_process_inbox() (called by module's loop)
 *
 * @author NIMCP Development Team
 * @date 2025-11-28
 */

#ifndef NIMCP_BIO_ROUTER_H
#define NIMCP_BIO_ROUTER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * FORWARD DECLARATIONS
 *============================================================================*/

typedef struct bio_router_struct* bio_router_t;
typedef struct bio_module_context_struct* bio_module_context_t;

/*=============================================================================
 * CALLBACK TYPES
 *============================================================================*/

/**
 * @brief Message handler callback type
 *
 * @param msg Pointer to message (header + payload)
 * @param msg_size Total message size
 * @param response_promise Promise to complete with response (may be NULL)
 * @param user_data Module-specific context
 * @return NIMCP_SUCCESS or error code
 */
typedef nimcp_error_t (*bio_message_handler_t)(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data
);

/**
 * @brief Broadcast handler for predictive coding updates
 *
 * @param signal_name Signal being observed
 * @param value Current value
 * @param user_data Module context
 */
typedef void (*bio_prediction_observer_t)(
    const char* signal_name,
    float value,
    void* user_data
);

/*=============================================================================
 * ROUTER CONFIGURATION
 *============================================================================*/

/**
 * @brief Router configuration
 */
typedef struct {
    uint32_t max_modules;           /**< Maximum registered modules */
    uint32_t inbox_capacity;        /**< Messages per module inbox */
    uint32_t outbox_capacity;       /**< Messages per module outbox */
    size_t max_message_size;        /**< Maximum single message size */
    uint32_t worker_threads;        /**< Router worker threads */
    bool enable_logging;            /**< Log all routed messages */
    bool enable_statistics;         /**< Track routing statistics */
    float routing_timeout_ms;       /**< Default routing timeout */
    bool enable_predictive_protocol; /**< Enable predictive prefetching */
} bio_router_config_t;

/**
 * @brief Module registration info
 */
typedef struct {
    bio_module_id_t module_id;
    const char* module_name;
    uint32_t inbox_capacity;        /**< 0 = use default */
    void* user_data;                /**< Passed to all handlers */
} bio_module_info_t;

/*=============================================================================
 * ROUTER STATISTICS
 *============================================================================*/

typedef struct {
    uint64_t messages_routed;
    uint64_t messages_dropped;
    uint64_t broadcasts_sent;
    uint64_t handler_errors;
    uint64_t timeouts;
    float avg_routing_latency_us;
    float max_routing_latency_us;
    uint32_t active_modules;
    uint32_t pending_messages;
} bio_router_stats_t;

/*=============================================================================
 * ROUTER LIFECYCLE API
 *============================================================================*/

/**
 * @brief Get default router configuration
 */
bio_router_config_t bio_router_default_config(void);

/**
 * @brief Initialize the global message router
 *
 * @param config Configuration (NULL for defaults)
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t bio_router_init(const bio_router_config_t* config);

/**
 * @brief Get the global router instance
 */
bio_router_t bio_router_get_global(void);

/**
 * @brief Shutdown router and cleanup
 */
void bio_router_shutdown(void);

/**
 * @brief Check if router is initialized
 */
bool bio_router_is_initialized(void);

/**
 * @brief Get router statistics
 */
nimcp_error_t bio_router_get_stats(bio_router_stats_t* stats);

/**
 * @brief Reset statistics
 */
void bio_router_reset_stats(void);

/*=============================================================================
 * MODULE REGISTRATION API
 *============================================================================*/

/**
 * @brief Register a module with the router
 *
 * @param info Module information
 * @return Module context handle or NULL on failure
 */
bio_module_context_t bio_router_register_module(const bio_module_info_t* info);

/**
 * @brief Unregister a module
 *
 * @param ctx Module context to unregister
 */
void bio_router_unregister_module(bio_module_context_t ctx);

/**
 * @brief Register handler for specific message type
 *
 * @param ctx Module context
 * @param msg_type Message type to handle
 * @param handler Handler function
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t bio_router_register_handler(
    bio_module_context_t ctx,
    bio_message_type_t msg_type,
    bio_message_handler_t handler
);

/**
 * @brief Register handler for all messages of a category
 *
 * @param ctx Module context
 * @param category_base Base message type (e.g., 0x0200 for all plasticity)
 * @param handler Handler function
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t bio_router_register_category_handler(
    bio_module_context_t ctx,
    uint32_t category_base,
    bio_message_handler_t handler
);

/**
 * @brief Unregister handler for specific message type
 *
 * @param ctx Module context
 * @param msg_type Message type to unregister
 * @return NIMCP_SUCCESS or error if not found
 */
nimcp_error_t bio_router_unregister_handler(
    bio_module_context_t ctx,
    bio_message_type_t msg_type
);

/**
 * @brief Clear all handlers for a module
 *
 * WHAT: Remove all registered handlers from module
 * WHY:  Clean slate for re-registration (prevents handler accumulation)
 * HOW:  Reset handler_count to zero
 *
 * @param ctx Module context
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t bio_router_clear_handlers(bio_module_context_t ctx);

/*=============================================================================
 * MESSAGE SENDING API
 *============================================================================*/

/**
 * @brief Send message synchronously (blocks until delivered)
 *
 * @param ctx Source module context
 * @param msg Message to send (header + payload)
 * @param msg_size Total message size
 * @param timeout_ms Timeout (0 = default)
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t bio_router_send(
    bio_module_context_t ctx,
    const void* msg,
    size_t msg_size,
    uint32_t timeout_ms
);

/**
 * @brief Send message asynchronously with promise
 *
 * @param ctx Source module context
 * @param msg Message to send
 * @param msg_size Message size
 * @param channel Neuromodulator channel for response
 * @return Promise for response (caller must destroy)
 */
nimcp_bio_promise_t bio_router_send_async(
    bio_module_context_t ctx,
    const void* msg,
    size_t msg_size,
    nimcp_bio_channel_type_t channel
);

/**
 * @brief Send message and wait for response
 *
 * @param ctx Source module context
 * @param request Request message
 * @param request_size Request size
 * @param response Output buffer for response
 * @param response_size Response buffer size
 * @param timeout_ms Timeout
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t bio_router_request(
    bio_module_context_t ctx,
    const void* request,
    size_t request_size,
    void* response,
    size_t response_size,
    uint32_t timeout_ms
);

/**
 * @brief Broadcast message to all modules
 *
 * @param ctx Source module context
 * @param msg Message to broadcast
 * @param msg_size Message size
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t bio_router_broadcast(
    bio_module_context_t ctx,
    const void* msg,
    size_t msg_size
);

/*=============================================================================
 * MESSAGE RECEIVING API
 *============================================================================*/

/**
 * @brief Process pending messages in module's inbox
 *
 * Processes up to max_messages, invoking registered handlers.
 *
 * @param ctx Module context
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed
 */
uint32_t bio_router_process_inbox(
    bio_module_context_t ctx,
    uint32_t max_messages
);

/**
 * @brief Get count of pending messages
 *
 * @param ctx Module context
 * @return Number of messages in inbox
 */
uint32_t bio_router_inbox_count(bio_module_context_t ctx);

/**
 * @brief Wait for messages with timeout
 *
 * @param ctx Module context
 * @param timeout_ms Timeout (0 = non-blocking)
 * @return true if messages available
 */
bool bio_router_wait_for_messages(
    bio_module_context_t ctx,
    uint32_t timeout_ms
);

/*=============================================================================
 * PREDICTIVE CODING INTEGRATION
 *============================================================================*/

/**
 * @brief Register as observer for a signal (predictive coding)
 *
 * @param ctx Module context
 * @param signal_name Signal to observe
 * @param initial_prediction Initial expected value
 * @param precision Initial precision
 * @param callback Called on prediction errors
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t bio_router_observe_signal(
    bio_module_context_t ctx,
    const char* signal_name,
    float initial_prediction,
    float precision,
    bio_prediction_observer_t callback
);

/**
 * @brief Publish signal value (triggers predictive updates)
 *
 * @param ctx Module context
 * @param signal_name Signal name
 * @param value Current value
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t bio_router_publish_signal(
    bio_module_context_t ctx,
    const char* signal_name,
    float value
);

/*=============================================================================
 * PHASE SYNCHRONIZATION HELPERS
 *============================================================================*/

/**
 * @brief Create phase sync group from multiple module responses
 *
 * @param ctx Source module context
 * @param band Oscillation band
 * @param targets Array of target module IDs
 * @param target_count Number of targets
 * @param request Request message to send to all
 * @param request_size Request size
 * @return Phase sync handle for coordinated waiting
 */
nimcp_phase_sync_t bio_router_sync_request(
    bio_module_context_t ctx,
    nimcp_oscillation_band_t band,
    const bio_module_id_t* targets,
    size_t target_count,
    const void* request,
    size_t request_size
);

/*=============================================================================
 * GLIAL WAVE INTEGRATION
 *============================================================================*/

/**
 * @brief Initiate glial wave broadcast
 *
 * @param ctx Source module context
 * @param intensity Wave intensity
 * @param metadata Optional metadata to carry
 * @return Wave handle
 */
nimcp_glial_wave_t bio_router_initiate_wave(
    bio_module_context_t ctx,
    float intensity,
    const void* metadata
);

/**
 * @brief Register for glial wave arrival
 *
 * @param ctx Module context
 * @param callback Called when wave arrives
 * @param user_data User context
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t bio_router_on_wave_arrival(
    bio_module_context_t ctx,
    nimcp_wave_callback_t callback,
    void* user_data
);

/*=============================================================================
 * UTILITY MACROS
 *============================================================================*/

/**
 * @brief Helper to create and send a typed message
 */
#define BIO_SEND_MSG(ctx, msg_type, target, ...) do { \
    msg_type msg = { __VA_ARGS__ }; \
    bio_msg_init_header(&msg.header, msg_type##_TYPE, \
        bio_module_context_get_id(ctx), target, sizeof(msg)); \
    bio_router_send(ctx, &msg, sizeof(msg), 0); \
} while(0)

/**
 * @brief Helper to create async request
 */
#define BIO_ASYNC_REQUEST(ctx, msg_type, target, channel, ...) ({ \
    msg_type msg = { __VA_ARGS__ }; \
    bio_msg_init_header(&msg.header, msg_type##_TYPE, \
        bio_module_context_get_id(ctx), target, sizeof(msg)); \
    bio_router_send_async(ctx, &msg, sizeof(msg), channel); \
})

/*=============================================================================
 * BRAIN IMMUNE INTEGRATION
 *============================================================================*/

/**
 * @brief Connect brain immune system to bio-async router
 *
 * WHAT: Register brain immune system with bio-async for cytokine messaging
 * WHY:  Enable immune coordination via bio-async neuromodulator channels
 * HOW:  Register immune module, set up NOREPINEPHRINE channel handlers
 *
 * @param immune_system Brain immune system to connect
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t bio_async_connect_immune(void* immune_system);

/**
 * @brief Broadcast cytokine release via bio-async
 *
 * WHAT: Send immune cytokine signal to all modules
 * WHY:  Coordinate immune response across system
 * HOW:  Broadcast immune alert on NOREPINEPHRINE channel
 *
 * @param cytokine_type Type of cytokine released
 * @param concentration Signal strength (0-1)
 * @param source_cell Source cell ID
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t bio_async_broadcast_cytokine(
    uint32_t cytokine_type,
    float concentration,
    uint32_t source_cell
);

/**
 * @brief Send inflammation alert as high-priority message
 *
 * WHAT: Send inflammation escalation alert
 * WHY:  Notify system of immune response escalation
 * HOW:  High-priority broadcast on NOREPINEPHRINE channel
 *
 * @param region_id Affected region
 * @param severity Inflammation severity (0-4)
 * @param antigen_id Triggering antigen
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t bio_async_inflammation_alert(
    uint32_t region_id,
    uint32_t severity,
    uint32_t antigen_id
);

/**
 * @brief Notify immune phase change
 *
 * WHAT: Broadcast immune system phase transition
 * WHY:  Coordinate system-wide immune state awareness
 * HOW:  Broadcast phase change message
 *
 * @param old_phase Previous phase
 * @param new_phase New phase
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t bio_async_immune_phase_change(
    uint32_t old_phase,
    uint32_t new_phase
);

/*=============================================================================
 * MODULE CONTEXT ACCESSORS
 *============================================================================*/

/**
 * @brief Get module ID from context
 */
bio_module_id_t bio_module_context_get_id(bio_module_context_t ctx);

/**
 * @brief Get module name from context
 */
const char* bio_module_context_get_name(bio_module_context_t ctx);

/**
 * @brief Get user data from context
 */
void* bio_module_context_get_user_data(bio_module_context_t ctx);

/*=============================================================================
 * ORCHESTRATOR INTEGRATION (KG-Based Runtime Module Assembly)
 *============================================================================*/

/* Forward declaration */
struct bio_async_orchestrator;
struct brain_kg;

/**
 * @brief Set orchestrator reference for the router
 *
 * WHAT: Associate orchestrator with global router instance
 * WHY:  Enables modules to access orchestrator for KG-driven registration
 * HOW:  Store reference in router, accessible via bio_router_get_orchestrator()
 *
 * @param orchestrator Orchestrator to associate
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t bio_router_set_orchestrator(struct bio_async_orchestrator* orchestrator);

/*=============================================================================
 * BRAIN KNOWLEDGE GRAPH INTEGRATION (Phase 7: Runtime Message Orchestration)
 *============================================================================*/

/**
 * @brief Set brain KG reference for KG-driven message dispatch
 *
 * WHAT: Associate brain_kg with global router for handler lookup
 * WHY:  Enables BIO_MODULE_KG_DISPATCH routing mode
 * HOW:  Store KG reference, use for message-type handler lookup
 *
 * When a message has target_module set to BIO_MODULE_KG_DISPATCH (0xFFFE),
 * the router queries the brain_kg to find all modules that handle this
 * message type, then dispatches to all of them.
 *
 * @param kg Brain knowledge graph (NULL to disconnect)
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t bio_router_set_brain_kg(struct brain_kg* kg);

/**
 * @brief Get brain KG reference from router
 *
 * @return brain_kg pointer or NULL if not set
 */
struct brain_kg* bio_router_get_brain_kg(void);

/**
 * @brief Check if KG-driven dispatch is available
 *
 * @return true if brain_kg is set and message-type index is usable
 */
bool bio_router_kg_dispatch_available(void);

/**
 * @brief Get orchestrator reference from router
 *
 * WHAT: Retrieve associated orchestrator for callback registration
 * WHY:  Modules use this to register KG-driven handler callbacks
 * HOW:  Return stored orchestrator reference
 *
 * @return Orchestrator pointer or NULL if not set
 */
struct bio_async_orchestrator* bio_router_get_orchestrator(void);

/**
 * @brief Register KG-driven handler callback via router
 *
 * WHAT: Convenience wrapper for registering wiring callbacks
 * WHY:  Simplifies module migration to KG-driven pattern
 * HOW:  Gets orchestrator from router, registers callback
 *
 * @param module_id Module ID
 * @param callback Handler callback function (wiring_handler_callback_t)
 * @param user_data User data passed to callback
 * @return NIMCP_SUCCESS or error if orchestrator not set
 */
nimcp_error_t bio_router_register_wiring_callback(
    bio_module_id_t module_id,
    void* callback,
    void* user_data
);

/**
 * @brief Set health agent for bio router heartbeats
 *
 * WHAT: Associate health agent with bio router for health monitoring
 * WHY:  Enables heartbeat reporting during message processing
 * HOW:  Store agent reference, send heartbeats during dispatch
 *
 * @param agent Health agent instance (can be NULL to disable)
 */
struct nimcp_health_agent;
void bio_router_set_health_agent(struct nimcp_health_agent* agent);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BIO_ROUTER_H */
