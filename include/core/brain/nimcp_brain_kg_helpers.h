/**
 * @file nimcp_brain_kg_helpers.h
 * @brief Helper utilities for module KG integration
 * @version 1.0.0
 * @date 2025-12-31
 *
 * WHAT: Convenience functions for modules to interact with internal KG
 * WHY:  Consistent pattern, graceful degradation, thread-safe access
 * HOW:  Wrapper functions that handle NULL checks and access levels
 *
 * USAGE:
 * ```c
 * // In module struct
 * typedef struct {
 *     kg_module_context_t kg_ctx;
 *     // ... other fields
 * } my_module_t;
 *
 * // Initialize
 * kg_module_init(&module->kg_ctx, brain, "my_module");
 *
 * // Query (no token needed)
 * brain_kg_node_id_t node = kg_find_node_safe(&module->kg_ctx, "target");
 * brain_kg_node_list_t* neighbors = kg_get_neighbors_safe(&module->kg_ctx);
 *
 * // Update (admin token needed)
 * kg_module_update_state(&module->kg_ctx, BRAIN_KG_STATE_ACTIVE, admin_token);
 * ```
 */

#ifndef NIMCP_BRAIN_KG_HELPERS_H
#define NIMCP_BRAIN_KG_HELPERS_H

#include "core/brain/nimcp_brain_kg.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration to avoid circular includes */
struct brain_struct;
typedef struct brain_struct* brain_t;

/* ============================================================================
 * KG MODULE CONTEXT
 * ============================================================================ */

/**
 * @brief KG access context for modules
 *
 * Modules store this in their struct to cache KG access information.
 * Enables graceful degradation when KG is disabled.
 */
typedef struct {
    brain_kg_t* kg;                      /**< Pointer to internal KG (or NULL) */
    brain_kg_node_id_t self_node_id;     /**< This module's node ID */
    bool kg_available;                   /**< Is KG available for queries? */
    const char* module_name;             /**< Module name (for logging) */
} kg_module_context_t;

/* ============================================================================
 * INITIALIZATION HELPERS
 * ============================================================================ */

/**
 * @brief Initialize KG context for a module
 *
 * WHAT: Setup module's KG access context
 * WHY:  Consistent initialization pattern across modules
 * HOW:  Get KG from brain, find module's node, cache references
 *
 * @param ctx Output context to initialize
 * @param brain Brain instance (may be NULL)
 * @param module_name Module name for node lookup
 * @return 0 on success (even if KG disabled), -1 on error
 *
 * @note If KG is disabled, ctx->kg_available will be false but function succeeds
 */
int kg_module_init(
    kg_module_context_t* ctx,
    brain_t brain,
    const char* module_name
);

/**
 * @brief Initialize KG context with direct KG pointer
 *
 * Use this when brain pointer is not available but KG pointer is.
 *
 * @param ctx Output context
 * @param kg KG pointer (may be NULL)
 * @param module_name Module name
 * @return 0 on success
 */
int kg_module_init_direct(
    kg_module_context_t* ctx,
    brain_kg_t* kg,
    const char* module_name
);

/**
 * @brief Create KG node for module if it doesn't exist
 *
 * WHAT: Create a new KG node for this module
 * WHY:  Some modules are dynamically registered after KG population
 * HOW:  Check if node exists, create if not, update context
 *
 * @param ctx Module context
 * @param type Node type for new node
 * @param description Node description
 * @param admin_token Admin token for write access
 * @return Node ID or BRAIN_KG_INVALID_NODE on error
 *
 * @note Requires ADMIN access level
 */
brain_kg_node_id_t kg_module_ensure_node(
    kg_module_context_t* ctx,
    brain_kg_node_type_t type,
    const char* description,
    uint64_t admin_token
);

/* ============================================================================
 * STATE SYNC HELPERS
 * ============================================================================ */

/**
 * @brief Update module's KG node state
 *
 * @param ctx Module context
 * @param state New state
 * @param admin_token Admin token for write access
 * @return 0 on success, -1 on error, 0 if KG disabled (no-op)
 */
int kg_module_update_state(
    kg_module_context_t* ctx,
    brain_kg_node_state_t state,
    uint64_t admin_token
);

/**
 * @brief Set module pointer in KG node
 *
 * @param ctx Module context
 * @param module_ptr Pointer to module instance
 * @param admin_token Admin token
 * @return 0 on success
 */
int kg_module_set_ptr(
    kg_module_context_t* ctx,
    void* module_ptr,
    uint64_t admin_token
);

/**
 * @brief Add metadata to module's KG node
 *
 * @param ctx Module context
 * @param key Metadata key
 * @param value Metadata value
 * @param admin_token Admin token
 * @return 0 on success
 */
int kg_module_add_metadata(
    kg_module_context_t* ctx,
    const char* key,
    const char* value,
    uint64_t admin_token
);

/* ============================================================================
 * QUERY HELPERS (No token needed - read-only)
 * ============================================================================ */

/**
 * @brief Find node by name (safe wrapper)
 *
 * @param ctx Module context
 * @param name Node name to find
 * @return Node ID or BRAIN_KG_INVALID_NODE if not found or KG disabled
 */
brain_kg_node_id_t kg_find_node_safe(
    const kg_module_context_t* ctx,
    const char* name
);

/**
 * @brief Get node by ID (safe wrapper)
 *
 * @param ctx Module context
 * @param id Node ID
 * @return Node pointer or NULL
 */
const brain_kg_node_t* kg_get_node_safe(
    const kg_module_context_t* ctx,
    brain_kg_node_id_t id
);

/**
 * @brief Get neighbors of module's node
 *
 * Returns all nodes connected to this module (both incoming and outgoing).
 *
 * @param ctx Module context
 * @return Node list (caller must free) or NULL if KG disabled
 */
brain_kg_node_list_t* kg_get_neighbors_safe(const kg_module_context_t* ctx);

/**
 * @brief Get outgoing connections from module
 *
 * @param ctx Module context
 * @return Edge list (caller must free) or NULL
 */
brain_kg_edge_list_t* kg_get_outgoing_safe(const kg_module_context_t* ctx);

/**
 * @brief Get incoming connections to module
 *
 * @param ctx Module context
 * @return Edge list (caller must free) or NULL
 */
brain_kg_edge_list_t* kg_get_incoming_safe(const kg_module_context_t* ctx);

/**
 * @brief Check if module is connected to target
 *
 * @param ctx Module context
 * @param target Target node ID
 * @return true if connected (directly or indirectly), false otherwise
 */
bool kg_are_connected_safe(
    const kg_module_context_t* ctx,
    brain_kg_node_id_t target
);

/**
 * @brief Get all nodes of specified type
 *
 * @param ctx Module context
 * @param type Node type to filter
 * @return Node list (caller must free) or NULL
 */
brain_kg_node_list_t* kg_get_nodes_by_type_safe(
    const kg_module_context_t* ctx,
    brain_kg_node_type_t type
);

/**
 * @brief Get all nodes in specified state
 *
 * @param ctx Module context
 * @param state State to filter
 * @return Node list (caller must free) or NULL
 */
brain_kg_node_list_t* kg_get_nodes_by_state_safe(
    const kg_module_context_t* ctx,
    brain_kg_node_state_t state
);

/**
 * @brief Find shortest path to target node
 *
 * @param ctx Module context
 * @param target Target node ID
 * @return Path (caller must free) or NULL if no path or KG disabled
 */
brain_kg_path_t* kg_find_path_safe(
    const kg_module_context_t* ctx,
    brain_kg_node_id_t target
);

/**
 * @brief Get reachable nodes from module
 *
 * @param ctx Module context
 * @param max_depth Maximum traversal depth
 * @return Node list (caller must free) or NULL
 */
brain_kg_node_list_t* kg_get_reachable_safe(
    const kg_module_context_t* ctx,
    uint32_t max_depth
);

/* ============================================================================
 * EDGE HELPERS
 * ============================================================================ */

/**
 * @brief Add edge from module to another node
 *
 * @param ctx Module context
 * @param to Target node ID
 * @param type Edge type
 * @param description Edge description
 * @param weight Edge weight [0.0-1.0]
 * @param admin_token Admin token
 * @return Edge ID or BRAIN_KG_INVALID_NODE on error
 */
brain_kg_edge_id_t kg_module_add_edge_to(
    kg_module_context_t* ctx,
    brain_kg_node_id_t to,
    brain_kg_edge_type_t type,
    const char* description,
    float weight,
    uint64_t admin_token
);

/**
 * @brief Add edge from another node to module
 *
 * @param ctx Module context
 * @param from Source node ID
 * @param type Edge type
 * @param description Edge description
 * @param weight Edge weight
 * @param admin_token Admin token
 * @return Edge ID or BRAIN_KG_INVALID_NODE on error
 */
brain_kg_edge_id_t kg_module_add_edge_from(
    kg_module_context_t* ctx,
    brain_kg_node_id_t from,
    brain_kg_edge_type_t type,
    const char* description,
    float weight,
    uint64_t admin_token
);

/* ============================================================================
 * STATISTICS HELPERS
 * ============================================================================ */

/**
 * @brief Get KG statistics
 *
 * @param ctx Module context
 * @param stats Output statistics
 * @return 0 on success, -1 if KG disabled
 */
int kg_get_stats_safe(
    const kg_module_context_t* ctx,
    brain_kg_stats_t* stats
);

/**
 * @brief Check if KG is available
 *
 * @param ctx Module context
 * @return true if KG is available for queries
 */
static inline bool kg_is_available(const kg_module_context_t* ctx) {
    return ctx && ctx->kg_available && ctx->kg;
}

/**
 * @brief Check if module has valid node in KG
 *
 * @param ctx Module context
 * @return true if module has a valid self_node_id
 */
static inline bool kg_has_node(const kg_module_context_t* ctx) {
    return kg_is_available(ctx) && ctx->self_node_id != BRAIN_KG_INVALID_NODE;
}

/* ============================================================================
 * W16 CONSUMER HELPERS — exposed so non-brain modules (e.g. middleware) can
 * query the KG via a brain_t pointer without pulling in brain_internal.h.
 * ============================================================================ */

/**
 * @brief W16-C: Query attention_focus node and return true if found.
 *
 * Checks whether the brain's internal KG has a node named "attention_focus".
 * If yes, bumps the kg_consumer_hits counter and returns true. Caller can
 * use the return value to bias an attention threshold (more sensitive when
 * focus is present).
 *
 * Graceful: returns false if KG disabled, brain NULL, or node missing.
 *
 * @param brain Brain instance (may be NULL)
 * @return true if attention_focus node exists in the KG, false otherwise
 */
bool brain_kg_helpers_query_attention_focus(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_KG_HELPERS_H */
