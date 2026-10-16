/**
 * @file nimcp_mesh_bio_integration.h
 * @brief Bio-Async Router to Mesh Network Integration
 *
 * WHAT: Wires the bio-async router to use mesh network for message routing
 * WHY:  Enable consensus-based routing for bio-router messages through mesh channels
 * HOW:  Hook into bio_router_send, translate to mesh transactions, route via channels
 *
 * ARCHITECTURE:
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────────┐
 * │                    BIO-ASYNC TO MESH INTEGRATION                             │
 * ├─────────────────────────────────────────────────────────────────────────────┤
 * │                                                                              │
 * │  ┌──────────────┐     ┌──────────────────┐     ┌──────────────────┐         │
 * │  │  Bio-Router  │────►│  Bio Integration │────►│   Mesh Network   │         │
 * │  │   (Async)    │     │                  │     │   (Consensus)    │         │
 * │  └──────────────┘     │  - Message Hook  │     └──────────────────┘         │
 * │                       │  - Category Map  │                                   │
 * │  ┌──────────────┐     │  - Priority Map  │     ┌──────────────────┐         │
 * │  │   Module A   │◄────│  - Tx Callback   │◄────│   Committed TX   │         │
 * │  │              │     │                  │     │                  │         │
 * │  └──────────────┘     └──────────────────┘     └──────────────────┘         │
 * │                                                                              │
 * └─────────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * CATEGORY TO CHANNEL MAPPING:
 * - NEURAL (0x0100)     -> SUBCORTICAL channel
 * - PLASTICITY (0x0200) -> SUBCORTICAL channel
 * - NEUROMOD (0x0300)   -> SUBCORTICAL channel
 * - PERCEPTION (0x0400) -> RIGHT_HEMISPHERE channel
 * - COGNITIVE (0x0500)  -> LEFT_HEMISPHERE channel
 * - MOTOR (0x0600)      -> SUBCORTICAL channel
 * - SECURITY (0x0700)   -> SYSTEM channel
 * - SYSTEM (0x0800)     -> SYSTEM channel
 *
 * PRIORITY MAPPING:
 * - BIO_MSG_FLAG_URGENT -> MESH_TX_EMERGENCY_OVERRIDE
 * - Normal priority     -> Category-specific endorsement policy
 *
 * @author NIMCP Development Team
 * @date 2025-02-01
 * @version 1.0.0
 */

#ifndef NIMCP_MESH_BIO_INTEGRATION_H
#define NIMCP_MESH_BIO_INTEGRATION_H

#include "mesh/nimcp_mesh_types.h"
#include "mesh/nimcp_mesh_bio_bridge.h"
#include "mesh/nimcp_mesh_integration.h"
#include "mesh/nimcp_mesh_transaction.h"
#include "async/nimcp_bio_messages.h"  /* For bio_module_id_t enum including BIO_MODULE_MESH_ROUTE */
#include "utils/error/nimcp_error_codes.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct mesh_bio_integration mesh_bio_integration_t;
typedef struct bio_router_struct* bio_router_t;
typedef struct mesh_bootstrap mesh_bootstrap_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Note: BIO_MODULE_MESH_ROUTE (0xFFFD) is defined in async/nimcp_bio_messages.h
 * as part of the bio_module_id_t enum */

/** @brief Maximum pending mesh callbacks */
#define MESH_BIO_MAX_PENDING_CALLBACKS 256

/** @brief Default fallback timeout for mesh routing (ms) */
#define MESH_BIO_DEFAULT_TIMEOUT_MS 100

/* ============================================================================
 * Priority Mapping
 * ============================================================================ */

/**
 * @brief Bio message priority levels
 */
typedef enum mesh_bio_priority {
    MESH_BIO_PRIORITY_LOW = 0,        /**< Low priority, batch processing OK */
    MESH_BIO_PRIORITY_NORMAL,         /**< Standard priority */
    MESH_BIO_PRIORITY_HIGH,           /**< High priority, fast path */
    MESH_BIO_PRIORITY_URGENT,         /**< Urgent, emergency override */
    MESH_BIO_PRIORITY_CRITICAL        /**< Critical, bypass normal endorsement */
} mesh_bio_priority_t;

/**
 * @brief Endorsement policy for priority level
 */
typedef struct mesh_bio_priority_policy {
    mesh_bio_priority_t priority;     /**< Priority level */
    const char* policy_name;          /**< Endorsement policy to use */
    uint32_t min_endorsers;           /**< Minimum endorsers required */
    float timeout_factor;             /**< Timeout multiplier (1.0 = default) */
} mesh_bio_priority_policy_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Integration configuration
 */
typedef struct mesh_bio_integration_config {
    /* Routing behavior */
    bool enable_mesh_routing;         /**< Enable mesh-based routing */
    bool fallback_to_direct;          /**< Fall back to direct routing if mesh unavailable */
    bool enable_bidirectional;        /**< Route mesh commits back to bio-router */

    /* Message filtering */
    bool route_broadcasts;            /**< Route broadcast messages through mesh */
    bool route_kg_dispatch;           /**< Route KG dispatch through mesh */
    uint32_t min_category_for_mesh;   /**< Minimum category ID for mesh routing (0 = all) */

    /* Priority configuration */
    bool honor_urgent_flag;           /**< Map BIO_MSG_FLAG_URGENT to emergency */
    mesh_bio_priority_t default_priority; /**< Default priority for unmapped messages */

    /* Timeouts */
    float default_timeout_ms;         /**< Default mesh routing timeout */
    float urgent_timeout_ms;          /**< Timeout for urgent messages */

    /* Callbacks */
    bool async_callbacks;             /**< Use async callbacks for mesh completion */

    /* Logging */
    bool verbose_logging;

} mesh_bio_integration_config_t;

/**
 * @brief Integration statistics
 */
typedef struct mesh_bio_integration_stats {
    /* Routing stats */
    uint64_t messages_intercepted;    /**< Bio messages intercepted */
    uint64_t routed_through_mesh;     /**< Routed through mesh */
    uint64_t direct_fallback;         /**< Fell back to direct routing */
    uint64_t routing_failures;        /**< Failed to route */

    /* Mesh transaction stats */
    uint64_t transactions_created;    /**< Mesh transactions created */
    uint64_t transactions_committed;  /**< Successfully committed */
    uint64_t transactions_failed;     /**< Failed transactions */
    uint64_t transactions_expired;    /**< Timed out transactions */

    /* Priority stats */
    uint64_t urgent_messages;         /**< Urgent priority messages */
    uint64_t normal_messages;         /**< Normal priority messages */
    uint64_t low_messages;            /**< Low priority messages */

    /* Callback stats */
    uint64_t callbacks_pending;       /**< Pending completion callbacks */
    uint64_t callbacks_completed;     /**< Completed callbacks */

    /* Per-category routing */
    uint64_t category_neural;
    uint64_t category_plasticity;
    uint64_t category_neuromod;
    uint64_t category_perception;
    uint64_t category_cognitive;
    uint64_t category_motor;
    uint64_t category_security;
    uint64_t category_system;

} mesh_bio_integration_stats_t;

/* ============================================================================
 * Callback Types
 * ============================================================================ */

/**
 * @brief Called when mesh transaction commits (for async routing)
 *
 * @param original_msg Original bio message that was routed
 * @param msg_size Original message size
 * @param result Mesh transaction result
 * @param ctx User context
 */
typedef void (*mesh_bio_commit_callback_t)(
    const void* original_msg,
    size_t msg_size,
    const mesh_result_t* result,
    void* ctx
);

/**
 * @brief Routing decision callback (custom routing logic)
 *
 * @param msg Bio message header
 * @param msg_size Message size
 * @param ctx User context
 * @return true to route through mesh, false for direct routing
 */
typedef bool (*mesh_bio_route_decision_t)(
    const void* msg,
    size_t msg_size,
    void* ctx
);

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default integration configuration
 *
 * @param config Output configuration
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_bio_integration_default_config(
    mesh_bio_integration_config_t* config
);

/**
 * @brief Create bio-mesh integration
 *
 * WHAT: Create integration layer between bio-router and mesh network
 * WHY:  Enable consensus-based routing for bio-async messages
 * HOW:  Creates hooks, channel mappings, and callback infrastructure
 *
 * @param bootstrap Mesh bootstrap handle (provides mesh_integration)
 * @param config Configuration (NULL for defaults)
 * @return Integration handle or NULL on failure
 */
mesh_bio_integration_t* mesh_bio_integration_create(
    mesh_bootstrap_t* bootstrap,
    const mesh_bio_integration_config_t* config
);

/**
 * @brief Destroy bio-mesh integration
 *
 * @param integration Integration to destroy (NULL-safe)
 */
void mesh_bio_integration_destroy(mesh_bio_integration_t* integration);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect integration to bio-router
 *
 * WHAT: Install routing hooks in bio-router
 * WHY:  Intercept messages for mesh-based routing
 * HOW:  Register as routing interceptor, receive messages before dispatch
 *
 * After connection:
 * - Messages matching criteria are routed through mesh
 * - Other messages pass through to normal bio-router dispatch
 * - Mesh commits can trigger callbacks to original senders
 *
 * @param integration Integration handle
 * @param router Bio-router to connect (NULL = use global router)
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_bio_integration_connect(
    mesh_bio_integration_t* integration,
    bio_router_t router
);

/**
 * @brief Disconnect integration from bio-router
 *
 * @param integration Integration handle
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_bio_integration_disconnect(
    mesh_bio_integration_t* integration
);

/**
 * @brief Check if integration is connected
 *
 * @param integration Integration handle
 * @return true if connected to bio-router
 */
bool mesh_bio_integration_is_connected(
    const mesh_bio_integration_t* integration
);

/**
 * @brief Check if mesh routing is available
 *
 * @param integration Integration handle
 * @return true if mesh network is available for routing
 */
bool mesh_bio_integration_mesh_available(
    const mesh_bio_integration_t* integration
);

/* ============================================================================
 * Routing API
 * ============================================================================ */

/**
 * @brief Route bio message through mesh network
 *
 * WHAT: Translate bio message to mesh transaction and submit
 * WHY:  Enable consensus-based message delivery
 * HOW:  Category to channel mapping, priority to policy mapping, submit TX
 *
 * @param integration Integration handle
 * @param msg Bio message (with header)
 * @param msg_size Total message size
 * @param callback Optional completion callback (may be NULL)
 * @param ctx Callback context
 * @return NIMCP_SUCCESS on success, or error if routing failed
 */
nimcp_error_t mesh_bio_integration_route_message(
    mesh_bio_integration_t* integration,
    const void* msg,
    size_t msg_size,
    mesh_bio_commit_callback_t callback,
    void* ctx
);

/**
 * @brief Route message with explicit priority
 *
 * @param integration Integration handle
 * @param msg Bio message
 * @param msg_size Message size
 * @param priority Priority level
 * @param callback Completion callback
 * @param ctx Callback context
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_bio_integration_route_priority(
    mesh_bio_integration_t* integration,
    const void* msg,
    size_t msg_size,
    mesh_bio_priority_t priority,
    mesh_bio_commit_callback_t callback,
    void* ctx
);

/**
 * @brief Route message to specific channel
 *
 * @param integration Integration handle
 * @param msg Bio message
 * @param msg_size Message size
 * @param channel Target channel ID
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_bio_integration_route_to_channel(
    mesh_bio_integration_t* integration,
    const void* msg,
    size_t msg_size,
    mesh_channel_id_t channel
);

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Set custom routing decision callback
 *
 * @param integration Integration handle
 * @param callback Decision callback
 * @param ctx Callback context
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_bio_integration_set_route_decision(
    mesh_bio_integration_t* integration,
    mesh_bio_route_decision_t callback,
    void* ctx
);

/**
 * @brief Set priority policy for a priority level
 *
 * @param integration Integration handle
 * @param policy Priority policy configuration
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_bio_integration_set_priority_policy(
    mesh_bio_integration_t* integration,
    const mesh_bio_priority_policy_t* policy
);

/**
 * @brief Map bio message category to mesh channel
 *
 * @param integration Integration handle
 * @param bio_category Bio message category (e.g., 0x0500 for COGNITIVE)
 * @param channel_id Target mesh channel
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_bio_integration_set_category_channel(
    mesh_bio_integration_t* integration,
    uint32_t bio_category,
    mesh_channel_id_t channel_id
);

/**
 * @brief Enable/disable mesh routing at runtime
 *
 * @param integration Integration handle
 * @param enabled true to enable, false to disable
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_bio_integration_set_enabled(
    mesh_bio_integration_t* integration,
    bool enabled
);

/* ============================================================================
 * Callback Management API
 * ============================================================================ */

/**
 * @brief Process pending mesh completion callbacks
 *
 * WHAT: Invoke callbacks for committed/failed mesh transactions
 * WHY:  Notify original message senders of routing outcome
 * HOW:  Check pending transactions, invoke callbacks for completed ones
 *
 * @param integration Integration handle
 * @param max_callbacks Maximum callbacks to process (0 = all)
 * @return Number of callbacks processed
 */
uint32_t mesh_bio_integration_process_callbacks(
    mesh_bio_integration_t* integration,
    uint32_t max_callbacks
);

/**
 * @brief Get count of pending callbacks
 *
 * @param integration Integration handle
 * @return Number of pending callbacks
 */
uint32_t mesh_bio_integration_pending_callbacks(
    const mesh_bio_integration_t* integration
);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

/**
 * @brief Get integration statistics
 *
 * @param integration Integration handle
 * @param stats Output statistics
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_bio_integration_get_stats(
    const mesh_bio_integration_t* integration,
    mesh_bio_integration_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param integration Integration handle
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_bio_integration_reset_stats(
    mesh_bio_integration_t* integration
);

/* ============================================================================
 * Bio-Router Hook Interface
 * ============================================================================ */

/**
 * @brief Message routing hook type
 *
 * This is the hook signature that bio-router will call for each message.
 *
 * @param msg Bio message
 * @param msg_size Message size
 * @param ctx Hook context (mesh_bio_integration_t*)
 * @return NIMCP_SUCCESS if message was handled, NIMCP_ERROR_NOT_FOUND to continue
 */
typedef nimcp_error_t (*mesh_bio_routing_hook_t)(
    const void* msg,
    size_t msg_size,
    void* ctx
);

/**
 * @brief Get routing hook function
 *
 * @param integration Integration handle
 * @return Routing hook function pointer
 */
mesh_bio_routing_hook_t mesh_bio_integration_get_hook(
    mesh_bio_integration_t* integration
);

/* ============================================================================
 * Global Integration Access
 * ============================================================================ */

/**
 * @brief Set global bio-mesh integration
 *
 * WHAT: Store global reference for bio-router hook access
 * WHY:  Allow bio-router to access integration without explicit reference
 *
 * @param integration Integration handle (NULL to clear)
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_bio_integration_set_global(
    mesh_bio_integration_t* integration
);

/**
 * @brief Get global bio-mesh integration
 *
 * @return Global integration or NULL if not set
 */
mesh_bio_integration_t* mesh_bio_integration_get_global(void);

/**
 * @brief Check if global mesh routing is available
 *
 * Convenience function for bio-router to check mesh availability.
 *
 * @return true if mesh routing is available
 */
bool mesh_bio_integration_global_available(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MESH_BIO_INTEGRATION_H */
