/**
 * @file nimcp_api_cognitive.c
 * @brief Cognitive systems API implementation
 *
 * This module handles cognitive system operations including:
 * - Working memory management
 * - Global workspace theory implementation
 * - Ethics engine operations
 * - Knowledge system management
 *
 * Responsibilities:
 * - Working memory add/get/refresh/stats
 * - Global workspace compete/read/subscribe/stats
 * - Ethics evaluation
 * - Knowledge base operations
 */

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "API_COGNITIVE"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(api_cognitive)

/* API Exception Integration (Phase 7) */
extern void set_error(const char* fmt, ...);
#define NIMCP_API_SET_ERROR(fmt, ...) set_error(fmt, ##__VA_ARGS__)
#include "api/nimcp_api_exception.h"

#include "nimcp.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "cognitive/nimcp_working_memory.h"
#include "cognitive/global_workspace/nimcp_global_workspace.h"
#include "cognitive/ethics/nimcp_ethics.h"
#include "cognitive/knowledge/nimcp_knowledge.h"
#include "utils/memory/nimcp_memory.h"
#include "security/nimcp_bbb_helpers.h"
#include <stdio.h>
#include <string.h>

//=============================================================================
// External References (from nimcp.c)
//=============================================================================

// These functions are defined in nimcp.c and shared across modules
extern void set_error(const char* fmt, ...);
extern const char* nimcp_get_error(void);

//=============================================================================
// Working Memory API Implementation
//=============================================================================

nimcp_status_t nimcp_brain_working_memory_add(
    nimcp_brain_t brain,
    const float* data,
    uint32_t size,
    float salience)
{
    // Guard: Validate brain
    NIMCP_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "NULL brain provided to working_memory_add");
    NIMCP_CHECK_THROW(brain->internal_brain, NIMCP_ERROR_INVALID, "Brain has NULL internal_brain in working_memory_add");

    // Guard: Validate parameters FIRST (before checking subsystem availability)
    NIMCP_CHECK_THROW(data, NIMCP_ERROR_NULL_ARG, "NULL data provided to working_memory_add");
    NIMCP_CHECK_THROW(size > 0, NIMCP_ERROR_INVALID, "Invalid size (0) provided to working_memory_add");

    // Guard: Check if working memory enabled (after parameter validation)
    working_memory_t* wm = brain_get_working_memory(brain->internal_brain);
    NIMCP_CHECK_THROW(wm, NIMCP_ERROR_INVALID, "Working memory not enabled in brain config");

    // Add to working memory
    bool success = working_memory_add(wm, data, size, salience);
    if (!success) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Failed to add item to working memory");
        return NIMCP_ERROR;
    }

    return NIMCP_OK;
}

const float* nimcp_brain_working_memory_get(
    nimcp_brain_t brain,
    uint32_t index,
    uint32_t* size_out)
{
    // Guard: Validate brain
    if (!brain || !brain->internal_brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Invalid brain handle in working_memory_get");
        return NULL;
    }

    // Guard: Check if working memory enabled
    working_memory_t* wm = brain_get_working_memory(brain->internal_brain);
    if (!wm) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Working memory not enabled in working_memory_get");
        return NULL;
    }

    // Get item
    const float* item = working_memory_get(wm, index, size_out);
    if (!item) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Invalid index %u or empty working memory", index);
        return NULL;
    }

    return item;
}

nimcp_status_t nimcp_brain_working_memory_stats(
    nimcp_brain_t brain,
    uint32_t* current_size_out,
    uint32_t* capacity_out)
{
    // Guard: Validate brain
    NIMCP_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "NULL brain provided to working_memory_stats");
    NIMCP_CHECK_THROW(brain->internal_brain, NIMCP_ERROR_INVALID, "Brain has NULL internal_brain in working_memory_stats");

    // Guard: Validate output parameters
    NIMCP_CHECK_THROW(current_size_out, NIMCP_ERROR_NULL_ARG, "NULL current_size_out in working_memory_stats");
    NIMCP_CHECK_THROW(capacity_out, NIMCP_ERROR_NULL_ARG, "NULL capacity_out in working_memory_stats");

    // Guard: Check if working memory enabled
    working_memory_t* wm = brain_get_working_memory(brain->internal_brain);
    NIMCP_CHECK_THROW(wm, NIMCP_ERROR_INVALID, "Working memory not enabled in working_memory_stats");

    // Get stats
    working_memory_stats_t stats;
    working_memory_get_stats(wm, &stats);

    *current_size_out = stats.current_size;
    *capacity_out = stats.capacity;

    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_working_memory_refresh(
    nimcp_brain_t brain,
    uint32_t index)
{
    // Guard: Validate brain
    NIMCP_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "NULL brain provided to working_memory_refresh");
    NIMCP_CHECK_THROW(brain->internal_brain, NIMCP_ERROR_INVALID, "Brain has NULL internal_brain in working_memory_refresh");

    // Guard: Check if working memory enabled
    working_memory_t* wm = brain_get_working_memory(brain->internal_brain);
    NIMCP_CHECK_THROW(wm, NIMCP_ERROR_INVALID, "Working memory not enabled in working_memory_refresh");

    // Refresh item
    bool success = working_memory_refresh(wm, index);
    if (!success) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Invalid index %u for working memory refresh", index);
        return -1;
    }

    return NIMCP_OK;
}

//=============================================================================
// Global Workspace API Implementation
//=============================================================================

/**
 * @brief Helper to convert public API module enum to internal enum
 */
static cognitive_module_t convert_module_enum(nimcp_cognitive_module_t public_module)
{
    // Direct mapping - enums have same values
    return (cognitive_module_t)public_module;
}

nimcp_status_t nimcp_brain_workspace_compete(
    nimcp_brain_t brain,
    nimcp_cognitive_module_t module,
    const float* content,
    uint32_t content_dim,
    float strength)
{
    // Guard: Validate brain
    NIMCP_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "NULL brain provided to workspace_compete");
    NIMCP_CHECK_THROW(brain->internal_brain, NIMCP_ERROR_INVALID, "Brain has NULL internal_brain in workspace_compete");

    // Guard: Validate parameters FIRST (before checking subsystem availability)
    NIMCP_CHECK_THROW(content, NIMCP_ERROR_NULL_ARG, "NULL content provided to workspace_compete");
    NIMCP_CHECK_THROW(content_dim > 0, NIMCP_ERROR_INVALID, "Invalid content_dim (0) in workspace_compete");
    NIMCP_CHECK_THROW(strength >= 0.0f && strength <= 1.0f, NIMCP_ERROR_INVALID, "Strength must be in range [0.0, 1.0]");

    // Guard: Check if global workspace enabled (after parameter validation)
    global_workspace_t* workspace = brain_get_global_workspace(brain->internal_brain);
    NIMCP_CHECK_THROW(workspace, NIMCP_ERROR_INVALID, "Global workspace not enabled in brain config");

    // Convert module enum and compete
    cognitive_module_t internal_module = convert_module_enum(module);
    bool won = global_workspace_compete(workspace, internal_module, content, content_dim, strength);

    if (won) {
        return NIMCP_OK;
    } else {
        return NIMCP_ERROR;  // Normal competition loss, not an error condition
    }
}

nimcp_status_t nimcp_brain_workspace_read(
    nimcp_brain_t brain,
    float* content,
    uint32_t max_dim,
    uint32_t* actual_dim,
    nimcp_cognitive_module_t* source_module)
{
    // Guard: Validate brain
    NIMCP_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "NULL brain provided to workspace_read");
    NIMCP_CHECK_THROW(brain->internal_brain, NIMCP_ERROR_INVALID, "Brain has NULL internal_brain in workspace_read");

    // Guard: Validate parameters FIRST (before checking subsystem availability)
    NIMCP_CHECK_THROW(content, NIMCP_ERROR_NULL_ARG, "NULL content buffer provided to workspace_read");
    NIMCP_CHECK_THROW(actual_dim, NIMCP_ERROR_NULL_ARG, "NULL actual_dim provided to workspace_read");
    NIMCP_CHECK_THROW(source_module, NIMCP_ERROR_NULL_ARG, "NULL source_module provided to workspace_read");
    NIMCP_CHECK_THROW(max_dim > 0, NIMCP_ERROR_INVALID, "Invalid max_dim (0) in workspace_read");

    // Guard: Check if global workspace enabled (after parameter validation)
    global_workspace_t* workspace = brain_get_global_workspace(brain->internal_brain);
    NIMCP_CHECK_THROW(workspace, NIMCP_ERROR_INVALID, "Global workspace not enabled in brain config");

    // Read broadcast
    cognitive_module_t internal_source;
    bool success = global_workspace_read_broadcast(
        workspace, content, max_dim, actual_dim, &internal_source
    );

    if (success) {
        *source_module = (nimcp_cognitive_module_t)internal_source;
        return NIMCP_OK;
    } else {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "No broadcast available or buffer too small in workspace_read");
        return NIMCP_ERROR;
    }
}

nimcp_status_t nimcp_brain_workspace_subscribe(
    nimcp_brain_t brain,
    nimcp_cognitive_module_t module)
{
    // Guard: Validate brain
    NIMCP_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "NULL brain provided to workspace_subscribe");
    NIMCP_CHECK_THROW(brain->internal_brain, NIMCP_ERROR_INVALID, "Brain has NULL internal_brain in workspace_subscribe");

    // Guard: Check if global workspace enabled
    global_workspace_t* workspace = brain_get_global_workspace(brain->internal_brain);
    NIMCP_CHECK_THROW(workspace, NIMCP_ERROR_INVALID, "Global workspace not enabled in workspace_subscribe");

    // Subscribe module
    cognitive_module_t internal_module = convert_module_enum(module);
    bool success = global_workspace_subscribe(workspace, internal_module);

    if (success) {
        return NIMCP_OK;
    } else {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Failed to subscribe module to workspace");
        return NIMCP_ERROR;
    }
}

nimcp_status_t nimcp_brain_workspace_unsubscribe(
    nimcp_brain_t brain,
    nimcp_cognitive_module_t module)
{
    // Guard: Validate brain
    NIMCP_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "NULL brain provided to workspace_unsubscribe");
    NIMCP_CHECK_THROW(brain->internal_brain, NIMCP_ERROR_INVALID, "Brain has NULL internal_brain in workspace_unsubscribe");

    // Guard: Check if global workspace enabled
    global_workspace_t* workspace = brain_get_global_workspace(brain->internal_brain);
    NIMCP_CHECK_THROW(workspace, NIMCP_ERROR_INVALID, "Global workspace not enabled in workspace_unsubscribe");

    // Unsubscribe module
    cognitive_module_t internal_module = convert_module_enum(module);
    bool success = global_workspace_unsubscribe(workspace, internal_module);

    if (success) {
        return NIMCP_OK;
    } else {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Failed to unsubscribe module from workspace");
        return NIMCP_ERROR;
    }
}

nimcp_status_t nimcp_brain_workspace_has_broadcast(
    nimcp_brain_t brain,
    bool* has_broadcast)
{
    // Guard: Validate brain
    NIMCP_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "NULL brain provided to workspace_has_broadcast");
    NIMCP_CHECK_THROW(brain->internal_brain, NIMCP_ERROR_INVALID, "Brain has NULL internal_brain in workspace_has_broadcast");

    // Guard: Validate output parameter
    NIMCP_CHECK_THROW(has_broadcast, NIMCP_ERROR_NULL_ARG, "NULL has_broadcast parameter");

    // Guard: Check if global workspace enabled
    global_workspace_t* workspace = brain_get_global_workspace(brain->internal_brain);
    NIMCP_CHECK_THROW(workspace, NIMCP_ERROR_INVALID, "Global workspace not enabled in workspace_has_broadcast");

    // Check for broadcast
    *has_broadcast = global_workspace_has_broadcast(workspace);
    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_workspace_stats(
    nimcp_brain_t brain,
    uint32_t* total_broadcasts,
    uint32_t* total_competitions,
    float* avg_strength)
{
    // Guard: Validate brain
    NIMCP_CHECK_THROW(brain, NIMCP_ERROR_NULL_ARG, "NULL brain provided to workspace_stats");
    NIMCP_CHECK_THROW(brain->internal_brain, NIMCP_ERROR_INVALID, "Brain has NULL internal_brain in workspace_stats");

    // Guard: Validate output parameters
    NIMCP_CHECK_THROW(total_broadcasts, NIMCP_ERROR_NULL_ARG, "NULL total_broadcasts in workspace_stats");
    NIMCP_CHECK_THROW(total_competitions, NIMCP_ERROR_NULL_ARG, "NULL total_competitions in workspace_stats");
    NIMCP_CHECK_THROW(avg_strength, NIMCP_ERROR_NULL_ARG, "NULL avg_strength in workspace_stats");

    // Guard: Check if global workspace enabled
    global_workspace_t* workspace = brain_get_global_workspace(brain->internal_brain);
    NIMCP_CHECK_THROW(workspace, NIMCP_ERROR_INVALID, "Global workspace not enabled in workspace_stats");

    // Get statistics
    workspace_statistics_t stats;
    bool success = global_workspace_get_statistics(workspace, &stats);

    if (success) {
        *total_broadcasts = (uint32_t)stats.total_broadcasts;
        *total_competitions = (uint32_t)stats.total_competitions;
        // Note: avg_strength not tracked in statistics, return 0.0
        *avg_strength = 0.0f;
        return NIMCP_OK;
    } else {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Failed to get workspace statistics");
        return NIMCP_ERROR;
    }
}

//=============================================================================
// Ethics API Implementation
//=============================================================================

nimcp_ethics_t nimcp_ethics_create(void) {
    nimcp_ethics_t handle = (nimcp_ethics_t)nimcp_malloc(sizeof(struct nimcp_ethics_handle));
    if (!handle) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(struct nimcp_ethics_handle),
            "Failed to allocate ethics handle");
        return NULL;
    }

    // Create default ethics configuration
    ethics_config_t config = {0};
    config.policies = NULL;
    config.num_policies = 0;
    config.callback = NULL;
    config.callback_context = NULL;
    config.default_severity = 0.5f;
    config.enable_learning = true;
    config.action_feature_size = 32;
    config.max_agents = 16;
    config.golden_rule_threshold = 0.0f;
    config.empathy_weight = 0.5f;

    // Create internal ethics engine
    handle->internal_ethics = ethics_engine_create(&config);

    if (!handle->internal_ethics) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Failed to create internal ethics engine");
        nimcp_free(handle);
        return NULL;
    }

    return handle;
}

void nimcp_ethics_destroy(nimcp_ethics_t ethics) {
    if (!ethics) {
        return;
    }

    if (ethics->internal_ethics) {
        ethics_engine_destroy(ethics->internal_ethics);
    }

    nimcp_free(ethics);
}

nimcp_status_t nimcp_ethics_check(
    nimcp_ethics_t ethics,
    const float* situation,
    uint32_t num_features,
    float* out_score)
{
    NIMCP_CHECK_THROW(ethics, NIMCP_ERROR_NULL_ARG, "Ethics handle is NULL in ethics_check");
    NIMCP_CHECK_THROW(situation, NIMCP_ERROR_NULL_ARG, "Situation array is NULL in ethics_check");
    NIMCP_CHECK_THROW(out_score, NIMCP_ERROR_NULL_ARG, "Output score pointer is NULL in ethics_check");

    // Create action context from situation features
    action_context_t action = {0};
    action.features = (float*)situation;  // Cast away const for internal API
    action.num_features = num_features;
    action.affected_agents = NULL;
    action.num_affected_agents = 0;
    action.predicted_harm = 0.0f;

    // Evaluate using internal ethics engine
    ethics_evaluation_t eval = ethics_engine_evaluate_action(ethics->internal_ethics, &action);

    // Convert evaluation to simple score
    // Golden Rule score: -1 (harmful) to +1 (beneficial)
    *out_score = eval.golden_rule_score;

    return NIMCP_OK;
}

//=============================================================================
// Knowledge API Implementation
//=============================================================================

nimcp_knowledge_t nimcp_knowledge_create(void) {
    nimcp_knowledge_t handle = (nimcp_knowledge_t)nimcp_malloc(sizeof(struct nimcp_knowledge_handle));
    if (!handle) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(struct nimcp_knowledge_handle),
            "Failed to allocate knowledge handle");
        return NULL;
    }

    // Create internal knowledge system
    handle->internal_knowledge = knowledge_system_create("default");

    if (!handle->internal_knowledge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Failed to create internal knowledge system");
        nimcp_free(handle);
        return NULL;
    }

    return handle;
}

void nimcp_knowledge_destroy(nimcp_knowledge_t knowledge) {
    if (!knowledge) {
        return;
    }

    if (knowledge->internal_knowledge) {
        knowledge_system_destroy(knowledge->internal_knowledge);
    }

    nimcp_free(knowledge);
}

nimcp_status_t nimcp_knowledge_add_fact(
    nimcp_knowledge_t knowledge,
    const char* subject,
    const char* predicate,
    const char* object)
{
    NIMCP_CHECK_THROW(knowledge, NIMCP_ERROR_NULL_ARG, "Knowledge handle is NULL in knowledge_add_fact");
    NIMCP_CHECK_THROW(subject, NIMCP_ERROR_NULL_ARG, "Subject is NULL in knowledge_add_fact");
    NIMCP_CHECK_THROW(predicate, NIMCP_ERROR_NULL_ARG, "Predicate is NULL in knowledge_add_fact");
    NIMCP_CHECK_THROW(object, NIMCP_ERROR_NULL_ARG, "Object is NULL in knowledge_add_fact");

    // Create a knowledge item from the fact
    knowledge_item_t item = {0};

    // Use subject as concept
    strncpy(item.concept_name, subject, sizeof(item.concept_name) - 1);

    // Create definition from predicate and object
    snprintf(item.definition, sizeof(item.definition), "%s %s", predicate, object);

    // Set defaults
    item.domain = KNOWLEDGE_DOMAIN_GENERAL;
    item.confidence = 1.0f;
    item.examples = NULL;
    item.num_examples = 0;
    item.related_concepts = NULL;
    item.num_related = 0;
    item.learned_timestamp = 0;
    item.reinforcement_count = 1;

    // Add to internal knowledge system
    bool success = knowledge_add_item(knowledge->internal_knowledge, &item);

    if (!success) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Failed to add knowledge item '%s'", subject);
        return NIMCP_ERROR;
    }

    return NIMCP_OK;
}

nimcp_status_t nimcp_knowledge_query(
    nimcp_knowledge_t knowledge,
    const char* query,
    char* out_result,
    uint32_t max_result_len)
{
    NIMCP_CHECK_THROW(knowledge, NIMCP_ERROR_NULL_ARG, "Knowledge handle is NULL in knowledge_query");
    NIMCP_CHECK_THROW(query, NIMCP_ERROR_NULL_ARG, "Query is NULL in knowledge_query");
    NIMCP_CHECK_THROW(out_result, NIMCP_ERROR_NULL_ARG, "Output result buffer is NULL in knowledge_query");
    NIMCP_CHECK_THROW(max_result_len > 0, NIMCP_ERROR_INVALID, "Invalid max_result_len (0) in knowledge_query");

    // Try to retrieve knowledge about the query concept
    knowledge_item_t item;
    bool found = knowledge_retrieve(knowledge->internal_knowledge, query, &item);

    if (found) {
        // Return the definition
        snprintf(out_result, max_result_len, "%s", item.definition);
    } else {
        // Concept not found
        snprintf(out_result, max_result_len, "No knowledge found about '%s'", query);
    }

    return NIMCP_OK;
}
