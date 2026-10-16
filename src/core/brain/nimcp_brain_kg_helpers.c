/**
 * @file nimcp_brain_kg_helpers.c
 * @brief Implementation of KG helper utilities for module integration
 * @version 1.0.0
 * @date 2025-12-31
 *
 * WHAT: Helper functions for modules to interact with internal KG
 * WHY:  Consistent pattern, graceful degradation, simplified API
 * HOW:  Wrapper functions with NULL checks and access level handling
 */

#include "core/brain/nimcp_brain_kg_helpers.h"
#include "core/brain/nimcp_brain_kg.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>

#define LOG_MODULE "KG_HELPERS"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_kg_helpers, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * INITIALIZATION HELPERS
 * ============================================================================ */

int kg_module_init(
    kg_module_context_t* ctx,
    brain_t brain,
    const char* module_name)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return -1;
    }

    /* Initialize to safe defaults */
    memset(ctx, 0, sizeof(*ctx));
    ctx->kg = NULL;
    ctx->self_node_id = BRAIN_KG_INVALID_NODE;
    ctx->kg_available = false;
    ctx->module_name = module_name;

    /* Check if brain and KG are available */
    if (!brain) {
        LOG_DEBUG("No brain provided for module '%s' - KG disabled",
                  module_name ? module_name : "unknown");
        return 0;  /* Success - just no KG available */
    }

    if (!brain->internal_kg_enabled || !brain->internal_kg) {
        LOG_DEBUG("Internal KG disabled for module '%s'",
                  module_name ? module_name : "unknown");
        return 0;
    }

    /* KG is available */
    ctx->kg = brain->internal_kg;
    ctx->kg_available = true;

    /* Try to find module's node */
    if (module_name) {
        ctx->self_node_id = brain_kg_find_node(ctx->kg, module_name);
        if (ctx->self_node_id != BRAIN_KG_INVALID_NODE) {
            LOG_DEBUG("Module '%s' found in KG with node ID %u",
                      module_name, ctx->self_node_id);
        } else {
            LOG_DEBUG("Module '%s' not found in KG - may need to create node",
                      module_name);
        }
    }

    return 0;
}

int kg_module_init_direct(
    kg_module_context_t* ctx,
    brain_kg_t* kg,
    const char* module_name)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return -1;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->kg = kg;
    ctx->self_node_id = BRAIN_KG_INVALID_NODE;
    ctx->kg_available = (kg != NULL);
    ctx->module_name = module_name;

    if (ctx->kg_available && module_name) {
        ctx->self_node_id = brain_kg_find_node(kg, module_name);
    }

    return 0;
}

brain_kg_node_id_t kg_module_ensure_node(
    kg_module_context_t* ctx,
    brain_kg_node_type_t type,
    const char* description,
    uint64_t admin_token)
{
    if (!kg_is_available(ctx) || !ctx->module_name) {
        return BRAIN_KG_INVALID_NODE;
    }

    /* Check if node already exists */
    if (ctx->self_node_id != BRAIN_KG_INVALID_NODE) {
        return ctx->self_node_id;
    }

    /* Need admin access to create node */
    brain_kg_set_access_level(ctx->kg, BRAIN_KG_ACCESS_ADMIN, admin_token);

    /* Create the node */
    brain_kg_node_id_t node_id = brain_kg_add_node(
        ctx->kg,
        ctx->module_name,
        type,
        description ? description : "Dynamically registered module"
    );

    /* Reset access level */
    brain_kg_set_access_level(ctx->kg, BRAIN_KG_ACCESS_READ, 0);

    if (node_id != BRAIN_KG_INVALID_NODE) {
        ctx->self_node_id = node_id;
        LOG_INFO("Created KG node for module '%s' with ID %u",
                 ctx->module_name, node_id);
    }

    return node_id;
}

/* ============================================================================
 * STATE SYNC HELPERS
 * ============================================================================ */

int kg_module_update_state(
    kg_module_context_t* ctx,
    brain_kg_node_state_t state,
    uint64_t admin_token)
{
    if (!kg_has_node(ctx)) {
        return 0;  /* No-op if KG or node not available */
    }

    brain_kg_set_access_level(ctx->kg, BRAIN_KG_ACCESS_ADMIN, admin_token);
    int result = brain_kg_update_node(ctx->kg, ctx->self_node_id, NULL, state);
    brain_kg_set_access_level(ctx->kg, BRAIN_KG_ACCESS_READ, 0);

    return result;
}

int kg_module_set_ptr(
    kg_module_context_t* ctx,
    void* module_ptr,
    uint64_t admin_token)
{
    if (!kg_has_node(ctx)) {
        return 0;
    }

    brain_kg_set_access_level(ctx->kg, BRAIN_KG_ACCESS_ADMIN, admin_token);
    int result = brain_kg_set_module_ptr(ctx->kg, ctx->self_node_id, module_ptr);
    brain_kg_set_access_level(ctx->kg, BRAIN_KG_ACCESS_READ, 0);

    return result;
}

int kg_module_add_metadata(
    kg_module_context_t* ctx,
    const char* key,
    const char* value,
    uint64_t admin_token)
{
    if (!kg_has_node(ctx) || !key || !value) {
        return 0;
    }

    brain_kg_set_access_level(ctx->kg, BRAIN_KG_ACCESS_ADMIN, admin_token);
    int result = brain_kg_add_metadata(ctx->kg, ctx->self_node_id, key, value);
    brain_kg_set_access_level(ctx->kg, BRAIN_KG_ACCESS_READ, 0);

    return result;
}

/* ============================================================================
 * QUERY HELPERS
 * ============================================================================ */

brain_kg_node_id_t kg_find_node_safe(
    const kg_module_context_t* ctx,
    const char* name)
{
    if (!kg_is_available(ctx) || !name) {
        return BRAIN_KG_INVALID_NODE;
    }
    return brain_kg_find_node(ctx->kg, name);
}

const brain_kg_node_t* kg_get_node_safe(
    const kg_module_context_t* ctx,
    brain_kg_node_id_t id)
{
    if (!kg_is_available(ctx)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_get_node_safe: kg_is_available is NULL");
        return NULL;
    }
    return brain_kg_get_node(ctx->kg, id);
}

brain_kg_node_list_t* kg_get_neighbors_safe(const kg_module_context_t* ctx)
{
    if (!kg_has_node(ctx)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_get_neighbors_safe: kg_has_node is NULL");
        return NULL;
    }
    return brain_kg_get_neighbors(ctx->kg, ctx->self_node_id);
}

brain_kg_edge_list_t* kg_get_outgoing_safe(const kg_module_context_t* ctx)
{
    if (!kg_has_node(ctx)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_get_outgoing_safe: kg_has_node is NULL");
        return NULL;
    }
    return brain_kg_get_outgoing(ctx->kg, ctx->self_node_id);
}

brain_kg_edge_list_t* kg_get_incoming_safe(const kg_module_context_t* ctx)
{
    if (!kg_has_node(ctx)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_get_incoming_safe: kg_has_node is NULL");
        return NULL;
    }
    return brain_kg_get_incoming(ctx->kg, ctx->self_node_id);
}

bool kg_are_connected_safe(
    const kg_module_context_t* ctx,
    brain_kg_node_id_t target)
{
    if (!kg_has_node(ctx) || target == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_are_connected_safe: kg_has_node is NULL");
        return false;
    }
    return brain_kg_are_connected(ctx->kg, ctx->self_node_id, target);
}

brain_kg_node_list_t* kg_get_nodes_by_type_safe(
    const kg_module_context_t* ctx,
    brain_kg_node_type_t type)
{
    if (!kg_is_available(ctx)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_get_nodes_by_type_safe: kg_is_available is NULL");
        return NULL;
    }
    return brain_kg_get_nodes_by_type(ctx->kg, type);
}

brain_kg_node_list_t* kg_get_nodes_by_state_safe(
    const kg_module_context_t* ctx,
    brain_kg_node_state_t state)
{
    if (!kg_is_available(ctx)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_get_nodes_by_state_safe: kg_is_available is NULL");
        return NULL;
    }

    /* Get all nodes and filter by state */
    brain_kg_node_list_t* all_nodes = brain_kg_get_all_nodes(ctx->kg);
    if (!all_nodes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "all_nodes is NULL");

        return NULL;
    }

    /* Count matching nodes */
    uint32_t matching = 0;
    for (uint32_t i = 0; i < all_nodes->count; i++) {
        if (all_nodes->nodes[i]->state == state) {
            matching++;
        }
    }

    if (matching == 0) {
        brain_kg_node_list_destroy(all_nodes);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_get_nodes_by_state_safe: matching is zero");
        return NULL;
    }

    /* Allocate result list */
    brain_kg_node_list_t* result = nimcp_malloc(sizeof(brain_kg_node_list_t));
    if (!result) {
        brain_kg_node_list_destroy(all_nodes);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_get_nodes_by_state_safe: result is NULL");
        return NULL;
    }

    result->nodes = nimcp_malloc(matching * sizeof(brain_kg_node_t*));
    if (!result->nodes) {
        nimcp_free(result);
        brain_kg_node_list_destroy(all_nodes);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_get_nodes_by_state_safe: result->nodes is NULL");
        return NULL;
    }

    result->count = 0;
    result->capacity = matching;

    /* Copy matching nodes */
    for (uint32_t i = 0; i < all_nodes->count; i++) {
        if (all_nodes->nodes[i]->state == state) {
            result->nodes[result->count++] = all_nodes->nodes[i];
        }
    }

    brain_kg_node_list_destroy(all_nodes);
    return result;
}

brain_kg_path_t* kg_find_path_safe(
    const kg_module_context_t* ctx,
    brain_kg_node_id_t target)
{
    if (!kg_has_node(ctx) || target == BRAIN_KG_INVALID_NODE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_find_path_safe: kg_has_node is NULL");
        return NULL;
    }
    return brain_kg_find_path(ctx->kg, ctx->self_node_id, target);
}

brain_kg_node_list_t* kg_get_reachable_safe(
    const kg_module_context_t* ctx,
    uint32_t max_depth)
{
    if (!kg_has_node(ctx)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_get_reachable_safe: kg_has_node is NULL");
        return NULL;
    }
    return brain_kg_get_reachable(ctx->kg, ctx->self_node_id, max_depth);
}

/* ============================================================================
 * EDGE HELPERS
 * ============================================================================ */

brain_kg_edge_id_t kg_module_add_edge_to(
    kg_module_context_t* ctx,
    brain_kg_node_id_t to,
    brain_kg_edge_type_t type,
    const char* description,
    float weight,
    uint64_t admin_token)
{
    if (!kg_has_node(ctx) || to == BRAIN_KG_INVALID_NODE) {
        return BRAIN_KG_INVALID_NODE;
    }

    brain_kg_set_access_level(ctx->kg, BRAIN_KG_ACCESS_ADMIN, admin_token);
    brain_kg_edge_id_t edge_id = brain_kg_add_edge(
        ctx->kg,
        ctx->self_node_id,
        to,
        type,
        description,
        weight
    );
    brain_kg_set_access_level(ctx->kg, BRAIN_KG_ACCESS_READ, 0);

    return edge_id;
}

brain_kg_edge_id_t kg_module_add_edge_from(
    kg_module_context_t* ctx,
    brain_kg_node_id_t from,
    brain_kg_edge_type_t type,
    const char* description,
    float weight,
    uint64_t admin_token)
{
    if (!kg_has_node(ctx) || from == BRAIN_KG_INVALID_NODE) {
        return BRAIN_KG_INVALID_NODE;
    }

    brain_kg_set_access_level(ctx->kg, BRAIN_KG_ACCESS_ADMIN, admin_token);
    brain_kg_edge_id_t edge_id = brain_kg_add_edge(
        ctx->kg,
        from,
        ctx->self_node_id,
        type,
        description,
        weight
    );
    brain_kg_set_access_level(ctx->kg, BRAIN_KG_ACCESS_READ, 0);

    return edge_id;
}

/* ============================================================================
 * STATISTICS HELPERS
 * ============================================================================ */

int kg_get_stats_safe(
    const kg_module_context_t* ctx,
    brain_kg_stats_t* stats)
{
    if (!kg_is_available(ctx) || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_get_stats_safe: required parameter is NULL (kg_is_available, stats)");
        return -1;
    }
    return brain_kg_get_stats(ctx->kg, stats);
}

/* ============================================================================
 * W16 CONSUMER HELPERS
 * ============================================================================ */

bool brain_kg_helpers_query_attention_focus(brain_t brain)
{
    if (!brain) return false;
    if (!brain->internal_kg_enabled || !brain->internal_kg) return false;

    brain_kg_node_id_t focus = brain_kg_find_node(
        brain->internal_kg, "attention_focus");
    if (focus == BRAIN_KG_INVALID_NODE) {
        return false;
    }

    __atomic_add_fetch(&brain->kg_consumer_hits, 1, __ATOMIC_RELAXED);
    brain->module_activity.kg_consumer_hits++;
    return true;
}
