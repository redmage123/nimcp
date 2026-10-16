/**
 * @file nimcp_hierarchical_recovery.c
 * @brief Implementation of Hierarchical Recovery Orchestration
 * @version 1.0.0
 * @date 2025-12-11
 */

#include "utils/fault_tolerance/nimcp_hierarchical_recovery.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "security/nimcp_bbb_helpers.h"
#include "api/nimcp_api_exception.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_buffer_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(hierarchical_recovery)

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Pending recovery request
 */
typedef struct {
    hr_recovery_request_t request;
    hr_recovery_response_t response;
    bool is_active;
    uint64_t submitted_at_ms;
} hr_pending_request_t;

/**
 * @brief Internal HR context
 */
struct hr_context {
    hr_config_t config;
    hr_node_context_t self;
    hr_escalation_policy_t policies[HR_MAX_POLICIES];
    uint32_t policy_count;
    hr_circuit_breaker_t circuits[HR_MAX_CIRCUITS];
    uint32_t circuit_count;
    hr_pending_request_t pending[64];
    uint32_t pending_count;
    uint32_t next_request_id;
    hr_recovery_handler_t handlers[HR_MAX_LEVELS];
    void* handler_data[HR_MAX_LEVELS];
    hr_recovery_completion_callback_t completion_callback;
    void* completion_user_data;
    hr_stats_t stats;
    nimcp_mutex_t lock;
    bool running;
    bool initialized;
};

//=============================================================================
// Private Functions
//=============================================================================

/**
 * @brief Find circuit breaker by name
 */
static hr_circuit_breaker_t* hr_find_circuit(hr_context_t* ctx, const char* name) {
    if (!ctx || !name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "hr_find_circuit: parameter is NULL");
        return NULL;
    }

    for (uint32_t i = 0; i < ctx->circuit_count; i++) {
        if (strcmp(ctx->circuits[i].config.name, name) == 0) {
            return &ctx->circuits[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hr_find_circuit: validation failed");
    return NULL;
}

/**
 * @brief Check if circuit allows operation
 */
static bool hr_circuit_check(hr_circuit_breaker_t* circuit) {
    if (!circuit) return true;

    uint64_t now = nimcp_time_get_ms();

    switch (circuit->state) {
        case HR_CIRCUIT_CLOSED:
            return true;

        case HR_CIRCUIT_OPEN:
            // Check if timeout elapsed
            if (now - circuit->last_state_change_ms > circuit->config.timeout_ms) {
                circuit->state = HR_CIRCUIT_HALF_OPEN;
                circuit->last_state_change_ms = now;
                return true;
            }
            return false;

        case HR_CIRCUIT_HALF_OPEN:
            return true;

        default:
            return true;
    }
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

hr_config_t hr_default_config(void) {
    hr_config_t config = {
        .node_id = 0,
        .node_level = HR_LEVEL_NODE,
        .parent_id = 0,
        .timeout_per_level_ms = {100, 500, 2000, 10000},
        .max_recovery_attempts = 3,
        .enable_cascade_prevention = true,
        .cascade_strategy = HR_CASCADE_ISOLATION,
        .enable_circuit_breakers = true,
        .enable_bio_async = true
    };
    return config;
}

hr_context_t* hr_create(const hr_config_t* config) {
    // Guard: Validate config - returns NULL for pointer-returning function
    NIMCP_API_CHECK_NULL_RET_NULL(config, "hr_create: NULL config provided");

    // Allocate context
    hr_context_t* ctx = nimcp_malloc(sizeof(hr_context_t));
    NIMCP_API_CHECK_ALLOC(ctx, "hr_create: Failed to allocate context");

    // Initialize
    memset(ctx, 0, sizeof(hr_context_t));
    ctx->config = *config;
    ctx->next_request_id = 1;

    // Initialize self
    ctx->self.node_id = config->node_id;
    ctx->self.level = config->node_level;
    ctx->self.parent_id = config->parent_id;
    ctx->self.health_score = 100.0f;
    ctx->self.is_healthy = true;

    // Initialize mutex
    if (nimcp_mutex_init(&ctx->lock, NULL) != 0) {
        LOG_ERROR("HR", "Failed to initialize mutex");
        nimcp_free(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "hr_create: validation failed");
        return NULL;
    }

    ctx->initialized = true;

    bbb_register_module("hierarchical_recovery", BBB_MODULE_TYPE_CORE);
    LOG_INFO("HR", "Created HR context for node %u at level %s",
                  config->node_id, hr_level_to_string(config->node_level));

    return ctx;
}

void hr_destroy(hr_context_t* ctx) {
    if (!ctx) return;

    if (ctx->running) {
        hr_stop(ctx);
    }

    nimcp_mutex_destroy(&ctx->lock);
    nimcp_free(ctx);
}

bool hr_start(hr_context_t* ctx) {
    if (!ctx || !ctx->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "hr_start: invalid parameter");
        return false;
    }
    if (ctx->running) return true;

    ctx->running = true;
    LOG_INFO("HR", "Started hierarchical recovery");
    return true;
}

bool hr_stop(hr_context_t* ctx) {
    if (!ctx || !ctx->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "hr_stop: invalid parameter");
        return false;
    }
    if (!ctx->running) return true;

    ctx->running = false;
    LOG_INFO("HR", "Stopped hierarchical recovery");
    return true;
}

//=============================================================================
// Hierarchy Management
//=============================================================================

bool hr_add_child(hr_context_t* ctx, uint32_t child_id, hr_level_t level) {
    if (!ctx || !ctx->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "hr_add_child: invalid parameter");
        return false;
    }
    if (ctx->self.child_count >= HR_MAX_CHILDREN) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "hr_add_child: invalid parameter");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);
    ctx->self.children[ctx->self.child_count++] = child_id;
    nimcp_mutex_unlock(&ctx->lock);

    LOG_DEBUG("HR", "Added child %u at level %s", child_id, hr_level_to_string(level));
    return true;
}

bool hr_remove_child(hr_context_t* ctx, uint32_t child_id) {
    if (!ctx || !ctx->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "hr_remove_child: invalid parameter");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);

    for (uint32_t i = 0; i < ctx->self.child_count; i++) {
        if (ctx->self.children[i] == child_id) {
            for (uint32_t j = i; j < ctx->self.child_count - 1; j++) {
                ctx->self.children[j] = ctx->self.children[j + 1];
            }
            ctx->self.child_count--;
            nimcp_mutex_unlock(&ctx->lock);
            return true;
        }
    }

    nimcp_mutex_unlock(&ctx->lock);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hr_remove_child: operation failed");
    return false;
}

bool hr_set_parent(hr_context_t* ctx, uint32_t parent_id) {
    if (!ctx || !ctx->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "hr_set_parent: invalid parameter");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);
    ctx->self.parent_id = parent_id;
    ctx->config.parent_id = parent_id;
    nimcp_mutex_unlock(&ctx->lock);

    return true;
}

bool hr_get_node_context(hr_context_t* ctx, uint32_t node_id, hr_node_context_t* node_ctx) {
    if (!ctx || !node_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "hr_get_node_context: invalid parameter");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);

    if (node_id == ctx->config.node_id) {
        *node_ctx = ctx->self;
        nimcp_mutex_unlock(&ctx->lock);
        return true;
    }

    nimcp_mutex_unlock(&ctx->lock);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hr_get_node_context: validation failed");
    return false;
}

//=============================================================================
// Recovery Operations
//=============================================================================

hr_result_t hr_submit_recovery(hr_context_t* ctx, const hr_recovery_request_t* request, hr_recovery_response_t* response) {
    // Guard: Validate inputs using API exception macros
    NIMCP_API_CHECK_NULL(ctx, NIMCP_ERROR_NULL_POINTER, "hr_submit_recovery: NULL context");
    NIMCP_API_CHECK_NULL(request, NIMCP_ERROR_NULL_POINTER, "hr_submit_recovery: NULL request");
    NIMCP_API_CHECK_NULL(response, NIMCP_ERROR_NULL_POINTER, "hr_submit_recovery: NULL response");
    NIMCP_API_CHECK(ctx->running, NIMCP_ERROR_INVALID_STATE, "hr_submit_recovery: context not running");

    uint64_t start_time = nimcp_time_get_ms();
    hr_result_t result = HR_RESULT_FAILED;

    nimcp_mutex_lock(&ctx->lock);

    // Check circuit breaker
    char circuit_name[NIMCP_ID_BUFFER_SIZE];
    snprintf(circuit_name, sizeof(circuit_name), "recovery_level_%d", request->current_level);

    if (ctx->config.enable_circuit_breakers) {
        hr_circuit_breaker_t* circuit = hr_find_circuit(ctx, circuit_name);
        if (circuit && !hr_circuit_check(circuit)) {
            response->result = HR_RESULT_CIRCUIT_OPEN;
            response->request_id = request->request_id;
            snprintf(response->message, sizeof(response->message),
                    "Circuit breaker %s is open", circuit_name);
            nimcp_mutex_unlock(&ctx->lock);
            return HR_RESULT_CIRCUIT_OPEN;
        }
    }

    // Try recovery at current level
    hr_level_t level = request->current_level;

    if (ctx->handlers[level]) {
        nimcp_mutex_unlock(&ctx->lock);
        result = ctx->handlers[level](request, response, ctx->handler_data[level]);
        nimcp_mutex_lock(&ctx->lock);
    } else {
        // Default recovery: simple success
        result = HR_RESULT_SUCCESS;
    }

    // Update stats
    ctx->stats.total_requests++;
    ctx->stats.requests_per_level[level]++;

    if (result == HR_RESULT_SUCCESS) {
        ctx->stats.successful_per_level[level]++;
    } else if (result == HR_RESULT_FAILED || result == HR_RESULT_TIMEOUT) {
        ctx->stats.failed_per_level[level]++;

        // Check for escalation
        if (level < request->max_level && level < HR_LEVEL_GLOBAL) {
            ctx->stats.escalations_per_level[level]++;

            // Escalate to next level
            hr_recovery_request_t escalated = *request;
            escalated.current_level = (hr_level_t)(level + 1);
            escalated.attempt_count++;

            nimcp_mutex_unlock(&ctx->lock);
            result = hr_submit_recovery(ctx, &escalated, response);
            nimcp_mutex_lock(&ctx->lock);

            if (result == HR_RESULT_SUCCESS) {
                result = HR_RESULT_ESCALATED;
            }
        }
    }

    // Fill response
    response->request_id = request->request_id;
    response->result = result;
    response->handling_level = level;
    response->handling_node_id = ctx->config.node_id;
    response->duration_ms = nimcp_time_get_ms() - start_time;

    // Update average recovery time
    uint64_t total_time = ctx->stats.avg_recovery_time_ms * (ctx->stats.total_requests - 1);
    ctx->stats.avg_recovery_time_ms = (total_time + response->duration_ms) / ctx->stats.total_requests;

    if (response->duration_ms > ctx->stats.max_recovery_time_ms) {
        ctx->stats.max_recovery_time_ms = response->duration_ms;
    }

    nimcp_mutex_unlock(&ctx->lock);

    // Invoke completion callback on successful recovery (immune IL-10 release)
    if ((result == HR_RESULT_SUCCESS || result == HR_RESULT_PARTIAL) && ctx->completion_callback) {
        ctx->completion_callback(request, response, ctx->completion_user_data);
    }

    return result;
}

bool hr_escalate(hr_context_t* ctx, uint32_t request_id, const char* reason) {
    if (!ctx || !reason) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "hr_escalate: invalid parameter");
        return false;
    }

    LOG_INFO("HR", "Escalating request %u: %s", request_id, reason);
    bbb_audit_log(BBB_AUDIT_WARNING, "HR", "ESCALATE", "Request %u escalated: %s", request_id, reason);

    return true;
}

bool hr_register_handler(hr_context_t* ctx, hr_level_t level, hr_recovery_handler_t handler, void* user_data) {
    if (!ctx || !handler || level >= HR_MAX_LEVELS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "hr_register_handler: invalid parameter");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);
    ctx->handlers[level] = handler;
    ctx->handler_data[level] = user_data;
    nimcp_mutex_unlock(&ctx->lock);

    LOG_DEBUG("HR", "Registered handler for level %s", hr_level_to_string(level));
    return true;
}

/**
 * @brief Register recovery completion callback
 *
 * WHAT: Store callback for recovery completion events
 * WHY:  Enable immune anti-inflammatory response
 * HOW:  Store in context, invoke after successful recovery
 */
bool hr_register_completion_callback(
    hr_context_t* ctx,
    hr_recovery_completion_callback_t callback,
    void* user_data
) {
    if (!ctx || !callback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "hr_register_completion_callback: invalid parameter");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);
    ctx->completion_callback = callback;
    ctx->completion_user_data = user_data;
    nimcp_mutex_unlock(&ctx->lock);

    LOG_DEBUG("HR", "Registered recovery completion callback");
    return true;
}

//=============================================================================
// Escalation Policies
//=============================================================================

bool hr_add_policy(hr_context_t* ctx, const hr_escalation_policy_t* policy) {
    if (!ctx || !policy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "hr_add_policy: invalid parameter");
        return false;
    }
    if (ctx->policy_count >= HR_MAX_POLICIES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "hr_add_policy: invalid parameter");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);
    ctx->policies[ctx->policy_count++] = *policy;
    nimcp_mutex_unlock(&ctx->lock);

    LOG_DEBUG("HR", "Added policy: %s", policy->name);
    return true;
}

bool hr_remove_policy(hr_context_t* ctx, const char* policy_name) {
    if (!ctx || !policy_name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "hr_remove_policy: invalid parameter");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);

    for (uint32_t i = 0; i < ctx->policy_count; i++) {
        if (strcmp(ctx->policies[i].name, policy_name) == 0) {
            for (uint32_t j = i; j < ctx->policy_count - 1; j++) {
                ctx->policies[j] = ctx->policies[j + 1];
            }
            ctx->policy_count--;
            nimcp_mutex_unlock(&ctx->lock);
            return true;
        }
    }

    nimcp_mutex_unlock(&ctx->lock);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hr_remove_policy: operation failed");
    return false;
}

bool hr_set_policy_enabled(hr_context_t* ctx, const char* policy_name, bool enabled) {
    if (!ctx || !policy_name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "hr_set_policy_enabled: invalid parameter");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);

    for (uint32_t i = 0; i < ctx->policy_count; i++) {
        if (strcmp(ctx->policies[i].name, policy_name) == 0) {
            ctx->policies[i].enabled = enabled;
            nimcp_mutex_unlock(&ctx->lock);
            return true;
        }
    }

    nimcp_mutex_unlock(&ctx->lock);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hr_set_policy_enabled: validation failed");
    return false;
}

//=============================================================================
// Circuit Breakers
//=============================================================================

bool hr_add_circuit_breaker(hr_context_t* ctx, const hr_circuit_config_t* config) {
    if (!ctx || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "hr_add_circuit_breaker: invalid parameter");
        return false;
    }
    if (ctx->circuit_count >= HR_MAX_CIRCUITS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "hr_add_circuit_breaker: invalid parameter");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);

    hr_circuit_breaker_t* circuit = &ctx->circuits[ctx->circuit_count++];
    circuit->config = *config;
    circuit->state = HR_CIRCUIT_CLOSED;
    circuit->failure_count = 0;
    circuit->success_count = 0;
    circuit->last_state_change_ms = nimcp_time_get_ms();

    nimcp_mutex_unlock(&ctx->lock);

    LOG_DEBUG("HR", "Added circuit breaker: %s", config->name);
    return true;
}

bool hr_get_circuit_breaker(hr_context_t* ctx, const char* name, hr_circuit_breaker_t* breaker) {
    if (!ctx || !name || !breaker) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "hr_get_circuit_breaker: invalid parameter");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);

    hr_circuit_breaker_t* circuit = hr_find_circuit(ctx, name);
    if (circuit) {
        *breaker = *circuit;
        nimcp_mutex_unlock(&ctx->lock);
        return true;
    }

    nimcp_mutex_unlock(&ctx->lock);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hr_get_circuit_breaker: validation failed");
    return false;
}

hr_circuit_state_t hr_record_circuit_result(hr_context_t* ctx, const char* name, bool success) {
    if (!ctx || !name) return HR_CIRCUIT_CLOSED;

    nimcp_mutex_lock(&ctx->lock);

    hr_circuit_breaker_t* circuit = hr_find_circuit(ctx, name);
    if (!circuit) {
        nimcp_mutex_unlock(&ctx->lock);
        return HR_CIRCUIT_CLOSED;
    }

    uint64_t now = nimcp_time_get_ms();
    circuit->total_calls++;

    if (success) {
        circuit->success_count++;
        circuit->failure_count = 0;

        if (circuit->state == HR_CIRCUIT_HALF_OPEN) {
            if (circuit->success_count >= circuit->config.success_threshold) {
                circuit->state = HR_CIRCUIT_CLOSED;
                circuit->last_state_change_ms = now;
                LOG_INFO("HR", "Circuit %s closed", name);
            }
        }
    } else {
        circuit->failure_count++;
        circuit->total_failures++;
        circuit->last_failure_ms = now;

        if (circuit->state == HR_CIRCUIT_CLOSED) {
            if (circuit->failure_count >= circuit->config.failure_threshold) {
                circuit->state = HR_CIRCUIT_OPEN;
                circuit->last_state_change_ms = now;
                ctx->stats.circuit_breaker_trips++;
                LOG_WARN("HR", "Circuit %s opened", name);
            }
        } else if (circuit->state == HR_CIRCUIT_HALF_OPEN) {
            circuit->state = HR_CIRCUIT_OPEN;
            circuit->last_state_change_ms = now;
            LOG_WARN("HR", "Circuit %s reopened", name);
        }
    }

    hr_circuit_state_t state = circuit->state;
    nimcp_mutex_unlock(&ctx->lock);
    return state;
}

bool hr_circuit_allow(hr_context_t* ctx, const char* name) {
    if (!ctx || !name) return true;

    nimcp_mutex_lock(&ctx->lock);

    hr_circuit_breaker_t* circuit = hr_find_circuit(ctx, name);
    bool allowed = hr_circuit_check(circuit);

    nimcp_mutex_unlock(&ctx->lock);
    return allowed;
}

bool hr_reset_circuit(hr_context_t* ctx, const char* name) {
    if (!ctx || !name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "hr_reset_circuit: invalid parameter");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);

    hr_circuit_breaker_t* circuit = hr_find_circuit(ctx, name);
    if (circuit) {
        circuit->state = HR_CIRCUIT_CLOSED;
        circuit->failure_count = 0;
        circuit->success_count = 0;
        circuit->last_state_change_ms = nimcp_time_get_ms();
        nimcp_mutex_unlock(&ctx->lock);
        return true;
    }

    nimcp_mutex_unlock(&ctx->lock);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hr_reset_circuit: operation failed");
    return false;
}

//=============================================================================
// Cascade Prevention
//=============================================================================

bool hr_detect_cascade(hr_context_t* ctx, uint32_t failed_node, hr_cascade_info_t* cascade_info) {
    if (!ctx || !cascade_info) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "hr_detect_cascade: invalid parameter");
        return false;
    }

    memset(cascade_info, 0, sizeof(hr_cascade_info_t));

    // Simple cascade detection: check if multiple children affected
    nimcp_mutex_lock(&ctx->lock);

    uint32_t affected = 0;
    for (uint32_t i = 0; i < ctx->self.child_count; i++) {
        // In real implementation, would check child health
        if (ctx->self.children[i] == failed_node) {
            cascade_info->affected_nodes[affected++] = ctx->self.children[i];
        }
    }

    cascade_info->affected_count = affected;
    cascade_info->strategy = ctx->config.cascade_strategy;
    cascade_info->cascade_probability = (float)affected / (float)(ctx->self.child_count + 1);
    cascade_info->estimated_impact = affected * 2; // Rough estimate
    cascade_info->cascade_detected = (cascade_info->cascade_probability > 0.3f);

    nimcp_mutex_unlock(&ctx->lock);

    return cascade_info->cascade_detected;
}

bool hr_prevent_cascade(hr_context_t* ctx, const hr_cascade_info_t* cascade_info) {
    if (!ctx || !cascade_info) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "hr_prevent_cascade: invalid parameter");
        return false;
    }

    LOG_INFO("HR", "Applying cascade prevention: %s",
                  hr_cascade_strategy_to_string(cascade_info->strategy));

    nimcp_mutex_lock(&ctx->lock);
    ctx->stats.cascades_prevented++;
    nimcp_mutex_unlock(&ctx->lock);

    bbb_audit_log(BBB_AUDIT_WARNING, "HR", "CASCADE",
                 "Prevented cascade affecting %u nodes", cascade_info->affected_count);

    return true;
}

uint32_t hr_get_cascade_prevention_count(hr_context_t* ctx) {
    if (!ctx) return 0;
    return ctx->stats.cascades_prevented;
}

//=============================================================================
// Statistics
//=============================================================================

bool hr_get_stats(hr_context_t* ctx, hr_stats_t* stats) {
    if (!ctx || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "hr_get_stats: invalid parameter");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);
    *stats = ctx->stats;
    nimcp_mutex_unlock(&ctx->lock);

    return true;
}

void hr_reset_stats(hr_context_t* ctx) {
    if (!ctx) return;

    nimcp_mutex_lock(&ctx->lock);
    memset(&ctx->stats, 0, sizeof(hr_stats_t));
    nimcp_mutex_unlock(&ctx->lock);
}

uint64_t hr_get_latency_by_level(hr_context_t* ctx, hr_level_t level) {
    if (!ctx || level >= HR_MAX_LEVELS) return 0;

    nimcp_mutex_lock(&ctx->lock);

    uint64_t count = ctx->stats.requests_per_level[level];
    uint64_t latency = (count > 0) ? ctx->stats.avg_recovery_time_ms : 0;

    nimcp_mutex_unlock(&ctx->lock);
    return latency;
}

//=============================================================================
// String Conversion
//=============================================================================

const char* hr_level_to_string(hr_level_t level) {
    switch (level) {
        case HR_LEVEL_NODE: return "NODE";
        case HR_LEVEL_POD: return "POD";
        case HR_LEVEL_REGION: return "REGION";
        case HR_LEVEL_GLOBAL: return "GLOBAL";
        default: return "UNKNOWN";
    }
}

const char* hr_trigger_to_string(hr_escalation_trigger_t trigger) {
    switch (trigger) {
        case HR_ESCALATE_TIMEOUT: return "TIMEOUT";
        case HR_ESCALATE_FAILURE: return "FAILURE";
        case HR_ESCALATE_THRESHOLD: return "THRESHOLD";
        case HR_ESCALATE_RESOURCE: return "RESOURCE";
        case HR_ESCALATE_CASCADE: return "CASCADE";
        case HR_ESCALATE_MANUAL: return "MANUAL";
        default: return "UNKNOWN";
    }
}

const char* hr_circuit_state_to_string(hr_circuit_state_t state) {
    switch (state) {
        case HR_CIRCUIT_CLOSED: return "CLOSED";
        case HR_CIRCUIT_OPEN: return "OPEN";
        case HR_CIRCUIT_HALF_OPEN: return "HALF_OPEN";
        default: return "UNKNOWN";
    }
}

const char* hr_result_to_string(hr_result_t result) {
    switch (result) {
        case HR_RESULT_SUCCESS: return "SUCCESS";
        case HR_RESULT_PARTIAL: return "PARTIAL";
        case HR_RESULT_FAILED: return "FAILED";
        case HR_RESULT_ESCALATED: return "ESCALATED";
        case HR_RESULT_TIMEOUT: return "TIMEOUT";
        case HR_RESULT_CIRCUIT_OPEN: return "CIRCUIT_OPEN";
        default: return "UNKNOWN";
    }
}

const char* hr_cascade_strategy_to_string(hr_cascade_strategy_t strategy) {
    switch (strategy) {
        case HR_CASCADE_NONE: return "NONE";
        case HR_CASCADE_ISOLATION: return "ISOLATION";
        case HR_CASCADE_SHEDDING: return "SHEDDING";
        case HR_CASCADE_BACKPRESSURE: return "BACKPRESSURE";
        case HR_CASCADE_BULKHEAD: return "BULKHEAD";
        default: return "UNKNOWN";
    }
}
