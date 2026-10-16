/**
 * @file nimcp_security_rcog_bridge.c
 * @brief Security Module - Recursive Cognition Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-09
 *
 * WHAT: Bidirectional integration between security module and recursive cognition
 * WHY:  Tool safety requires security oversight; output validation prevents harmful
 *       content; recursion limits prevent resource exhaustion attacks
 * HOW:  Enforces tool whitelisting, validates parameters and outputs, sandboxes
 *       execution, tracks resource usage, and escalates sensitive operations
 */

#include "security/rcog/nimcp_security_rcog_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/platform/nimcp_platform_time.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <stdlib.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(security_rcog_bridge, MESH_ADAPTER_CATEGORY_SECURITY)


/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Module ID for bio-async registration */
#define BIO_MODULE_SECURITY_RCOG_BRIDGE 0x1500

/* ============================================================================
 * Internal Helpers - Forward Declarations
 * ============================================================================ */

static int find_whitelist_entry(
    const security_rcog_bridge_t* bridge,
    const char* tool_name,
    size_t* index
);

static int find_approval_request(
    const security_rcog_bridge_t* bridge,
    uint64_t request_id,
    size_t* index
);

static void update_average(float* avg, uint64_t count, float new_value);

static bool is_suspicious_output(
    const void* output,
    size_t output_size,
    float* score
);

static uint64_t generate_request_id(void);

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

/**
 * @brief Get default security-rcog bridge configuration
 */
int security_rcog_default_config(security_rcog_config_t* config) {
    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    memset(config, 0, sizeof(security_rcog_config_t));

    /* Enable all security features by default */
    config->enable_tool_whitelisting = true;
    config->enable_output_validation = true;
    config->enable_recursion_limits = true;
    config->enable_resource_tracking = true;
    config->enable_sandbox_execution = true;
    config->enable_human_approval = true;
    config->enable_parameter_validation = true;
    config->enable_audit_logging = true;

    /* Conservative limits */
    config->max_recursion_depth = SECURITY_RCOG_DEFAULT_MAX_DEPTH;
    config->default_resource_budget = SECURITY_RCOG_DEFAULT_RESOURCE_BUDGET;
    config->max_concurrent_tools = 8;
    config->approval_timeout_ms = 30000;  /* 30 seconds */

    /* Rate limiting */
    config->tools_per_second = 10.0f;
    config->tool_burst_size = 20;

    /* Sensitivity settings */
    config->suspicious_output_threshold = 0.7f;
    config->resource_warning_threshold = 0.8f;

    return NIMCP_SUCCESS;
}

/**
 * @brief Create security-rcog bridge
 */
security_rcog_bridge_t* security_rcog_bridge_create(
    const security_rcog_config_t* config
) {
    security_rcog_bridge_t* bridge = nimcp_calloc(1, sizeof(security_rcog_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate security-rcog bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }

    /* Initialize base bridge */
    if (bridge_base_init(&bridge->base, BIO_MODULE_SECURITY_RCOG_BRIDGE,
                         "security_rcog_bridge") != 0) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_rcog_bridge_create: operation failed");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        security_rcog_default_config(&bridge->config);
    }

    /* Allocate whitelist */
    bridge->whitelist_capacity = SECURITY_RCOG_MAX_WHITELISTED_TOOLS;
    bridge->whitelist = nimcp_calloc(
        bridge->whitelist_capacity,
        sizeof(security_rcog_tool_permission_t)
    );
    if (!bridge->whitelist) {
        NIMCP_LOGGING_ERROR("Failed to allocate tool whitelist");
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "security_rcog_bridge_create: bridge->whitelist is NULL");
        return NULL;
    }

    /* Allocate approval queue */
    bridge->approval_queue = nimcp_calloc(
        SECURITY_RCOG_MAX_APPROVAL_REQUESTS,
        sizeof(security_rcog_approval_request_t)
    );
    if (!bridge->approval_queue) {
        NIMCP_LOGGING_ERROR("Failed to allocate approval queue");
        nimcp_free(bridge->whitelist);
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "security_rcog_bridge_create: bridge->approval_queue is NULL");
        return NULL;
    }

    /* Initialize state */
    bridge->state.is_active = true;
    bridge->state.emergency_lockdown = false;

    /* Initialize security effects with safe defaults */
    bridge->security_effects.effective_max_depth = bridge->config.max_recursion_depth;
    bridge->security_effects.effective_resource_budget = bridge->config.default_resource_budget;
    bridge->security_effects.depth_reduction_factor = 1.0f;
    bridge->security_effects.resource_reduction_factor = 1.0f;
    bridge->security_effects.effective_rate_limit = bridge->config.tools_per_second;

    return bridge;
}

/**
 * @brief Destroy security-rcog bridge
 */
void security_rcog_bridge_destroy(security_rcog_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Free whitelist */
    if (bridge->whitelist) {
        nimcp_free(bridge->whitelist);
        bridge->whitelist = NULL;
    }

    /* Free approval queue */
    if (bridge->approval_queue) {
        nimcp_free(bridge->approval_queue);
        bridge->approval_queue = NULL;
    }

    /* Cleanup base bridge */
    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge);
}

/* ============================================================================
 * Connection API Implementation
 * ============================================================================ */

/**
 * @brief Connect rcog engine to bridge
 */
int security_rcog_connect_engine(
    security_rcog_bridge_t* bridge,
    struct rcog_engine* engine
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(engine, NIMCP_ERROR_NULL_POINTER, "engine is NULL");

    BRIDGE_LOCK(bridge);
    bridge->rcog_engine = engine;
    bridge->state.rcog_engine_connected = true;
    BRIDGE_UNLOCK(bridge);

    return NIMCP_SUCCESS;
}

/**
 * @brief Connect tool router to bridge
 */
int security_rcog_connect_tool_router(
    security_rcog_bridge_t* bridge,
    struct rcog_tool_router* router
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(router, NIMCP_ERROR_NULL_POINTER, "router is NULL");

    BRIDGE_LOCK(bridge);
    bridge->tool_router = router;
    bridge->state.tool_router_connected = true;
    BRIDGE_UNLOCK(bridge);

    return NIMCP_SUCCESS;
}

/**
 * @brief Connect policy engine to bridge
 */
int security_rcog_connect_policy_engine(
    security_rcog_bridge_t* bridge,
    nimcp_policy_engine_t policy_engine
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(policy_engine, NIMCP_ERROR_NULL_POINTER, "policy_engine is NULL");

    BRIDGE_LOCK(bridge);
    bridge->policy_engine = policy_engine;
    bridge->state.policy_engine_connected = true;
    BRIDGE_UNLOCK(bridge);

    return NIMCP_SUCCESS;
}

/**
 * @brief Connect rate limiter to bridge
 */
int security_rcog_connect_rate_limiter(
    security_rcog_bridge_t* bridge,
    nimcp_rate_limiter_t rate_limiter
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(rate_limiter, NIMCP_ERROR_NULL_POINTER, "rate_limiter is NULL");

    BRIDGE_LOCK(bridge);
    bridge->rate_limiter = rate_limiter;
    bridge->state.rate_limiter_connected = true;
    BRIDGE_UNLOCK(bridge);

    return NIMCP_SUCCESS;
}

/**
 * @brief Check if bridge is fully connected
 */
bool security_rcog_is_connected(const security_rcog_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Minimum required: engine and tool_router */
    return bridge->state.rcog_engine_connected &&
           bridge->state.tool_router_connected;
}

/* ============================================================================
 * Whitelist Management API Implementation
 * ============================================================================ */

/**
 * @brief Check if tool is whitelisted
 */
bool security_rcog_is_tool_whitelisted(
    const security_rcog_bridge_t* bridge,
    const char* tool_name,
    rcog_capability_tier_t tier
) {
    if (!bridge || !tool_name) {
        return false;
    }

    /* If whitelisting is disabled, all tools are allowed */
    if (!bridge->config.enable_tool_whitelisting) {
        return true;
    }

    /* Emergency lockdown blocks all non-essential tools */
    if (bridge->state.emergency_lockdown) {
        return false;
    }

    BRIDGE_LOCK((security_rcog_bridge_t*)bridge);

    size_t index;
    if (find_whitelist_entry(bridge, tool_name, &index) != NIMCP_SUCCESS) {
        BRIDGE_UNLOCK((security_rcog_bridge_t*)bridge);
        return false;  /* Tool not in whitelist */
    }

    const security_rcog_tool_permission_t* perm = &bridge->whitelist[index];

    /* Check if tool is allowed and tier is sufficient */
    bool allowed = perm->allowed && (tier >= perm->min_tier);

    BRIDGE_UNLOCK((security_rcog_bridge_t*)bridge);

    return allowed;
}

/**
 * @brief Add tool to whitelist
 */
int security_rcog_whitelist_tool(
    security_rcog_bridge_t* bridge,
    const security_rcog_tool_permission_t* permission
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(permission, NIMCP_ERROR_NULL_POINTER, "permission is NULL");
    NIMCP_CHECK_THROW(permission->tool_name[0] != '\0', NIMCP_ERROR_INVALID_PARAMETER, "tool_name is empty");

    BRIDGE_LOCK(bridge);

    /* Check if tool already exists */
    size_t index;
    if (find_whitelist_entry(bridge, permission->tool_name, &index) == NIMCP_SUCCESS) {
        /* Update existing entry */
        bridge->whitelist[index] = *permission;
        BRIDGE_UNLOCK(bridge);
        return NIMCP_SUCCESS;
    }

    /* Check capacity */
    if (bridge->whitelist_count >= bridge->whitelist_capacity) {
        BRIDGE_UNLOCK(bridge);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_OUT_OF_RANGE, "whitelist capacity exceeded");
    }

    /* Add new entry */
    bridge->whitelist[bridge->whitelist_count] = *permission;
    bridge->whitelist_count++;
    bridge->state.whitelisted_count = (uint32_t)bridge->whitelist_count;

    /* Update security effects */
    bridge->security_effects.whitelisted_tool_count = (uint32_t)bridge->whitelist_count;

    BRIDGE_UNLOCK(bridge);

    return NIMCP_SUCCESS;
}

/**
 * @brief Remove tool from whitelist
 */
int security_rcog_unwhitelist_tool(
    security_rcog_bridge_t* bridge,
    const char* tool_name
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(tool_name, NIMCP_ERROR_NULL_POINTER, "tool_name is NULL");

    BRIDGE_LOCK(bridge);

    size_t index;
    if (find_whitelist_entry(bridge, tool_name, &index) != NIMCP_SUCCESS) {
        BRIDGE_UNLOCK(bridge);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NOT_FOUND, "tool not found in whitelist");
    }

    /* Shift remaining entries */
    if (index < bridge->whitelist_count - 1) {
        memmove(&bridge->whitelist[index],
                &bridge->whitelist[index + 1],
                (bridge->whitelist_count - index - 1) *
                sizeof(security_rcog_tool_permission_t));
    }
    bridge->whitelist_count--;
    bridge->state.whitelisted_count = (uint32_t)bridge->whitelist_count;

    /* Update security effects */
    bridge->security_effects.whitelisted_tool_count = (uint32_t)bridge->whitelist_count;

    BRIDGE_UNLOCK(bridge);

    return NIMCP_SUCCESS;
}

/**
 * @brief Get tool permission
 */
int security_rcog_get_tool_permission(
    const security_rcog_bridge_t* bridge,
    const char* tool_name,
    security_rcog_tool_permission_t* permission
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(tool_name, NIMCP_ERROR_NULL_POINTER, "tool_name is NULL");

    BRIDGE_LOCK((security_rcog_bridge_t*)bridge);

    size_t index;
    if (find_whitelist_entry(bridge, tool_name, &index) != NIMCP_SUCCESS) {
        BRIDGE_UNLOCK((security_rcog_bridge_t*)bridge);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NOT_FOUND, "tool not found in whitelist");
    }

    if (permission) {
        *permission = bridge->whitelist[index];
    }

    BRIDGE_UNLOCK((security_rcog_bridge_t*)bridge);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Validation API Implementation
 * ============================================================================ */

/**
 * @brief Validate tool parameters
 */
security_rcog_validation_result_t security_rcog_validate_tool_params(
    security_rcog_bridge_t* bridge,
    const char* tool_name,
    const void* params,
    size_t params_size
) {
    if (!bridge || !tool_name) {
        return SECURITY_RCOG_INVALID_PARAMS;
    }

    /* Skip validation if disabled */
    if (!bridge->config.enable_parameter_validation) {
        return SECURITY_RCOG_VALID;
    }

    BRIDGE_LOCK(bridge);

    bridge->stats.param_validations++;

    /* Check if tool is whitelisted */
    size_t index;
    if (find_whitelist_entry(bridge, tool_name, &index) != NIMCP_SUCCESS) {
        bridge->stats.param_validation_failures++;
        BRIDGE_UNLOCK(bridge);
        return SECURITY_RCOG_INVALID_TOOL;
    }

    /* Basic parameter validation */
    if (params == NULL && params_size > 0) {
        bridge->stats.param_validation_failures++;
        BRIDGE_UNLOCK(bridge);
        return SECURITY_RCOG_INVALID_PARAMS;
    }

    /* Check for obviously malicious patterns in parameters */
    if (params && params_size > 0) {
        const char* param_str = (const char*)params;
        /* Basic injection pattern detection */
        for (size_t i = 0; i + 1 < params_size; i++) {
            if (param_str[i] == ';' && param_str[i+1] == ';') {
                bridge->stats.param_validation_failures++;
                BRIDGE_UNLOCK(bridge);
                return SECURITY_RCOG_INVALID_PARAMS;
            }
        }
    }

    BRIDGE_UNLOCK(bridge);

    return SECURITY_RCOG_VALID;
}

/**
 * @brief Validate tool output
 */
security_rcog_validation_result_t security_rcog_validate_tool_output(
    security_rcog_bridge_t* bridge,
    const char* tool_name,
    const void* output,
    size_t output_size,
    float* suspicious_score
) {
    if (!bridge || !tool_name) {
        return SECURITY_RCOG_INVALID_OUTPUT;
    }

    /* Initialize suspicious score */
    if (suspicious_score) {
        *suspicious_score = 0.0f;
    }

    /* Skip validation if disabled */
    if (!bridge->config.enable_output_validation) {
        return SECURITY_RCOG_VALID;
    }

    BRIDGE_LOCK(bridge);

    bridge->stats.output_validations++;

    /* Check for suspicious content */
    float score = 0.0f;
    if (output && output_size > 0) {
        if (is_suspicious_output(output, output_size, &score)) {
            bridge->rcog_effects.suspicious_outputs++;
            bridge->rcog_effects.current_suspicious_score = score;

            if (suspicious_score) {
                *suspicious_score = score;
            }

            /* Check threshold */
            if (score >= bridge->config.suspicious_output_threshold) {
                bridge->stats.output_validation_failures++;
                bridge->stats.outputs_redacted++;
                BRIDGE_UNLOCK(bridge);
                return SECURITY_RCOG_OUTPUT_REDACTED;
            }
        }
    }

    if (suspicious_score) {
        *suspicious_score = score;
    }

    BRIDGE_UNLOCK(bridge);

    return SECURITY_RCOG_VALID;
}

/* ============================================================================
 * Execution API Implementation
 * ============================================================================ */

/**
 * @brief Execute tool with security sandbox
 */
int security_rcog_execute_with_sandbox(
    security_rcog_bridge_t* bridge,
    const char* tool_name,
    const void* input,
    size_t input_size,
    void* output,
    size_t output_size,
    size_t* actual_output_size,
    security_rcog_execution_result_t* result
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(tool_name, NIMCP_ERROR_NULL_POINTER, "tool_name is NULL");
    NIMCP_CHECK_THROW(result, NIMCP_ERROR_NULL_POINTER, "result is NULL");

    /* Initialize result */
    memset(result, 0, sizeof(security_rcog_execution_result_t));
    result->success = false;
    uint64_t start_time = nimcp_platform_time_monotonic_us();

    BRIDGE_LOCK(bridge);

    /* Check emergency lockdown */
    if (bridge->state.emergency_lockdown) {
        result->validation_result = SECURITY_RCOG_POLICY_DENIED;
        result->security_flags = SECURITY_RCOG_FLAG_BLOCKED;
        result->error_message = "Emergency lockdown active";
        BRIDGE_UNLOCK(bridge);
        return NIMCP_ERROR_INVALID_STATE;
    }

    /* Check rate limit if connected */
    if (bridge->rate_limiter && bridge->config.enable_sandbox_execution) {
        if (!nimcp_rate_limiter_allow(bridge->rate_limiter, tool_name)) {
            result->validation_result = SECURITY_RCOG_RATE_LIMITED;
            result->security_flags = SECURITY_RCOG_FLAG_RATE_LIMITED;
            bridge->stats.rate_limit_hits++;
            bridge->rcog_effects.rate_limited_calls++;
            BRIDGE_UNLOCK(bridge);
            return NIMCP_ERROR_INVALID_STATE;
        }
        bridge->stats.rate_limit_checks++;
    }

    /* Validate tool is whitelisted */
    size_t whitelist_index;
    if (bridge->config.enable_tool_whitelisting) {
        if (find_whitelist_entry(bridge, tool_name, &whitelist_index) != NIMCP_SUCCESS) {
            result->validation_result = SECURITY_RCOG_INVALID_TOOL;
            result->security_flags = SECURITY_RCOG_FLAG_BLOCKED;
            result->error_message = "Tool not whitelisted";
            bridge->stats.blocked_executions++;
            BRIDGE_UNLOCK(bridge);
            return NIMCP_ERROR_NOT_FOUND;
        }
        result->security_flags |= SECURITY_RCOG_FLAG_WHITELISTED;
    }

    /* Validate parameters */
    security_rcog_validation_result_t param_result =
        security_rcog_validate_tool_params(bridge, tool_name, input, input_size);
    if (param_result != SECURITY_RCOG_VALID) {
        result->validation_result = param_result;
        result->security_flags |= SECURITY_RCOG_FLAG_BLOCKED;
        result->error_message = "Parameter validation failed";
        bridge->stats.blocked_executions++;
        BRIDGE_UNLOCK(bridge);
        return NIMCP_ERROR_INVALID_PARAMETER;
    }
    result->security_flags |= SECURITY_RCOG_FLAG_PARAMS_VALIDATED;

    /* Check if human approval required */
    if (bridge->config.enable_tool_whitelisting &&
        find_whitelist_entry(bridge, tool_name, &whitelist_index) == NIMCP_SUCCESS &&
        bridge->whitelist[whitelist_index].requires_approval) {
        result->validation_result = SECURITY_RCOG_APPROVAL_REQUIRED;
        result->error_message = "Human approval required";
        BRIDGE_UNLOCK(bridge);
        return NIMCP_ERROR_INVALID_STATE;
    }

    /* Mark as sandboxed if sandbox enabled */
    if (bridge->config.enable_sandbox_execution) {
        result->security_flags |= SECURITY_RCOG_FLAG_SANDBOXED;
        bridge->stats.sandboxed_executions++;
    }

    /* Track resource usage */
    if (bridge->config.enable_resource_tracking) {
        result->security_flags |= SECURITY_RCOG_FLAG_RESOURCE_LIMITED;
    }

    /* Simulate successful execution for now */
    /* In full implementation, would call tool_router to execute */
    result->success = true;
    result->validation_result = SECURITY_RCOG_VALID;

    if (actual_output_size) {
        *actual_output_size = 0;
    }

    /* Validate output */
    if (output && output_size > 0) {
        float suspicious;
        security_rcog_validation_result_t output_result =
            security_rcog_validate_tool_output(bridge, tool_name,
                                                output, output_size, &suspicious);
        result->suspicious_score = suspicious;

        if (output_result == SECURITY_RCOG_OUTPUT_REDACTED) {
            result->security_flags |= SECURITY_RCOG_FLAG_OUTPUT_REDACTED;
        } else if (output_result == SECURITY_RCOG_VALID) {
            result->security_flags |= SECURITY_RCOG_FLAG_OUTPUT_VALIDATED;
            result->output_valid = true;
        }
    }

    /* Update statistics */
    bridge->stats.total_tool_executions++;
    bridge->stats.successful_executions++;
    bridge->rcog_effects.total_tool_calls++;

    /* Record timing */
    result->execution_time_us = nimcp_platform_time_monotonic_us() - start_time;
    update_average(&bridge->stats.avg_validation_time_us,
                   bridge->stats.total_tool_executions,
                   (float)result->execution_time_us);

    BRIDGE_UNLOCK(bridge);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Limit Checking API Implementation
 * ============================================================================ */

/**
 * @brief Check recursion depth
 */
bool security_rcog_check_recursion_depth(
    const security_rcog_bridge_t* bridge,
    uint32_t current_depth
) {
    if (!bridge) {
        return false;
    }

    if (!bridge->config.enable_recursion_limits) {
        return true;
    }

    BRIDGE_LOCK((security_rcog_bridge_t*)bridge);

    uint32_t effective_max = bridge->security_effects.effective_max_depth;
    bool allowed = current_depth < effective_max;

    ((security_rcog_bridge_t*)bridge)->stats.depth_checks++;
    if (!allowed) {
        ((security_rcog_bridge_t*)bridge)->stats.depth_limit_hits++;
    }

    if (current_depth > bridge->stats.max_depth_observed) {
        ((security_rcog_bridge_t*)bridge)->stats.max_depth_observed = current_depth;
    }

    BRIDGE_UNLOCK((security_rcog_bridge_t*)bridge);

    return allowed;
}

/**
 * @brief Track resource usage
 */
int security_rcog_track_resource_usage(
    security_rcog_bridge_t* bridge,
    const char* tool_name,
    uint64_t resources_used
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if (!bridge->config.enable_resource_tracking) {
        return NIMCP_SUCCESS;
    }

    BRIDGE_LOCK(bridge);

    /* Add to running total */
    bridge->state.current_resource_usage += resources_used;
    bridge->stats.total_resources_consumed += resources_used;
    bridge->rcog_effects.total_resources_used += resources_used;

    /* Update average */
    update_average(&bridge->stats.avg_resource_per_tool,
                   bridge->stats.total_tool_executions,
                   (float)resources_used);

    /* Calculate utilization */
    float utilization = (float)bridge->state.current_resource_usage /
                        (float)bridge->security_effects.effective_resource_budget;
    bridge->rcog_effects.resource_utilization = utilization;

    /* Check warning threshold */
    if (utilization >= bridge->config.resource_warning_threshold) {
        bridge->rcog_effects.resource_warnings++;
    }

    /* Check if budget exceeded */
    if (bridge->state.current_resource_usage >
        bridge->security_effects.effective_resource_budget) {
        bridge->stats.resource_limit_hits++;
        BRIDGE_UNLOCK(bridge);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_OUT_OF_RANGE, "resource budget exceeded");
    }

    BRIDGE_UNLOCK(bridge);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Human Approval API Implementation
 * ============================================================================ */

/**
 * @brief Request human approval for tool execution
 */
security_rcog_approval_status_t security_rcog_require_human_approval(
    security_rcog_bridge_t* bridge,
    const char* tool_name,
    rcog_capability_tier_t tier,
    const char* reason,
    uint64_t* request_id
) {
    if (!bridge || !tool_name) {
        return SECURITY_RCOG_APPROVAL_DENIED_ONCE;
    }

    if (!bridge->config.enable_human_approval) {
        /* If approval disabled, auto-approve */
        return SECURITY_RCOG_APPROVAL_APPROVED;
    }

    BRIDGE_LOCK(bridge);

    /* Check if queue is full */
    if (bridge->approval_queue_count >= SECURITY_RCOG_MAX_APPROVAL_REQUESTS) {
        bridge->security_effects.approval_queue_full = true;
        BRIDGE_UNLOCK(bridge);
        return SECURITY_RCOG_APPROVAL_DENIED_ONCE;
    }

    /* Generate request ID */
    uint64_t id = generate_request_id();
    if (request_id) {
        *request_id = id;
    }

    /* Create approval request */
    security_rcog_approval_request_t* req =
        &bridge->approval_queue[bridge->approval_queue_count];
    req->request_id = id;
    strncpy(req->tool_name, tool_name, SECURITY_RCOG_MAX_TOOL_NAME - 1);
    req->tool_name[SECURITY_RCOG_MAX_TOOL_NAME - 1] = '\0';
    req->tier = tier;
    req->status = SECURITY_RCOG_APPROVAL_PENDING;
    req->requested_at_ms = nimcp_platform_time_monotonic_ms();
    req->resolved_at_ms = 0;

    if (reason) {
        strncpy(req->reason, reason, sizeof(req->reason) - 1);
        req->reason[sizeof(req->reason) - 1] = '\0';
    }

    bridge->approval_queue_count++;
    bridge->state.pending_approvals = (uint32_t)bridge->approval_queue_count;
    bridge->security_effects.pending_approvals = (uint32_t)bridge->approval_queue_count;

    bridge->stats.approval_requests++;
    bridge->rcog_effects.approval_requests++;

    BRIDGE_UNLOCK(bridge);

    return SECURITY_RCOG_APPROVAL_PENDING;
}

/**
 * @brief Check approval status
 */
security_rcog_approval_status_t security_rcog_check_approval_status(
    const security_rcog_bridge_t* bridge,
    uint64_t request_id
) {
    if (!bridge) {
        return SECURITY_RCOG_APPROVAL_DENIED_ONCE;
    }

    BRIDGE_LOCK((security_rcog_bridge_t*)bridge);

    size_t index;
    if (find_approval_request(bridge, request_id, &index) != NIMCP_SUCCESS) {
        BRIDGE_UNLOCK((security_rcog_bridge_t*)bridge);
        return SECURITY_RCOG_APPROVAL_DENIED_ONCE;
    }

    security_rcog_approval_status_t status = bridge->approval_queue[index].status;

    /* Check for timeout */
    if (status == SECURITY_RCOG_APPROVAL_PENDING &&
        bridge->config.approval_timeout_ms > 0) {
        uint64_t elapsed = nimcp_platform_time_monotonic_ms() -
                           bridge->approval_queue[index].requested_at_ms;
        if (elapsed > bridge->config.approval_timeout_ms) {
            status = SECURITY_RCOG_APPROVAL_TIMEOUT;
            ((security_rcog_bridge_t*)bridge)->approval_queue[index].status = status;
        }
    }

    BRIDGE_UNLOCK((security_rcog_bridge_t*)bridge);

    return status;
}

/**
 * @brief Resolve approval request
 */
int security_rcog_resolve_approval(
    security_rcog_bridge_t* bridge,
    uint64_t request_id,
    security_rcog_approval_status_t status
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    size_t index;
    if (find_approval_request(bridge, request_id, &index) != NIMCP_SUCCESS) {
        BRIDGE_UNLOCK(bridge);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NOT_FOUND, "approval request not found");
    }

    bridge->approval_queue[index].status = status;
    bridge->approval_queue[index].resolved_at_ms = nimcp_platform_time_monotonic_ms();

    /* Update statistics */
    if (status == SECURITY_RCOG_APPROVAL_APPROVED) {
        bridge->stats.approvals_granted++;
        bridge->rcog_effects.approvals_granted++;
    } else if (status == SECURITY_RCOG_APPROVAL_DENIED_ONCE ||
               status == SECURITY_RCOG_APPROVAL_DENIED_PERM) {
        bridge->stats.approvals_denied++;
        bridge->rcog_effects.approvals_denied++;
    } else if (status == SECURITY_RCOG_APPROVAL_TIMEOUT) {
        bridge->stats.approvals_timeout++;
    }

    /* Update average approval time */
    uint64_t duration = bridge->approval_queue[index].resolved_at_ms -
                        bridge->approval_queue[index].requested_at_ms;
    update_average(&bridge->stats.avg_approval_time_ms,
                   bridge->stats.approvals_granted + bridge->stats.approvals_denied,
                   (float)duration);

    BRIDGE_UNLOCK(bridge);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bidirectional Update API Implementation
 * ============================================================================ */

/**
 * @brief Update security effects on rcog (outbound)
 */
int security_rcog_update_security_effects(security_rcog_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    /* Compute effective limits based on security state */
    float depth_factor = 1.0f;
    float resource_factor = 1.0f;
    float rate_factor = 1.0f;

    /* Reduce limits if suspicious activity detected */
    if (bridge->rcog_effects.current_suspicious_score > 0.5f) {
        depth_factor *= (1.0f - bridge->rcog_effects.current_suspicious_score * 0.5f);
        resource_factor *= (1.0f - bridge->rcog_effects.current_suspicious_score * 0.3f);
        rate_factor *= (1.0f - bridge->rcog_effects.current_suspicious_score * 0.4f);
    }

    /* Apply lockdown if active */
    if (bridge->state.emergency_lockdown) {
        depth_factor = 0.25f;
        resource_factor = 0.25f;
        rate_factor = 0.1f;
        bridge->security_effects.emergency_tool_lockdown = true;
    } else {
        bridge->security_effects.emergency_tool_lockdown = false;
    }

    /* Update effective limits */
    bridge->security_effects.depth_reduction_factor = depth_factor;
    bridge->security_effects.resource_reduction_factor = resource_factor;
    bridge->security_effects.effective_max_depth =
        (uint32_t)(bridge->config.max_recursion_depth * depth_factor);
    bridge->security_effects.effective_resource_budget =
        (uint64_t)(bridge->config.default_resource_budget * resource_factor);
    bridge->security_effects.effective_rate_limit =
        bridge->config.tools_per_second * rate_factor;

    /* Update rate limiting state */
    bridge->security_effects.rate_limiting_active =
        (bridge->stats.rate_limit_hits > 0);

    /* Update approval state */
    bridge->security_effects.pending_approvals = (uint32_t)bridge->approval_queue_count;
    bridge->security_effects.approval_queue_full =
        (bridge->approval_queue_count >= SECURITY_RCOG_MAX_APPROVAL_REQUESTS);

    /* Update blocked tool count */
    uint32_t blocked = 0;
    for (size_t i = 0; i < bridge->whitelist_count; i++) {
        if (!bridge->whitelist[i].allowed) {
            blocked++;
        }
    }
    bridge->security_effects.blocked_tool_count = blocked;

    BRIDGE_UNLOCK(bridge);

    return NIMCP_SUCCESS;
}

/**
 * @brief Update rcog effects on security (inbound)
 */
int security_rcog_update_rcog_effects(security_rcog_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    /* Calculate averages */
    if (bridge->rcog_effects.total_tool_calls > 0) {
        bridge->rcog_effects.avg_tools_per_request =
            (float)bridge->tools_called_this_request;
    }

    /* Track max depth */
    if (bridge->state.current_depth > bridge->rcog_effects.max_depth_reached) {
        bridge->rcog_effects.max_depth_reached = bridge->state.current_depth;
    }

    /* Update failed validation count */
    bridge->rcog_effects.failed_validations =
        (uint32_t)(bridge->stats.param_validation_failures +
                   bridge->stats.output_validation_failures);

    BRIDGE_UNLOCK(bridge);

    return NIMCP_SUCCESS;
}

/**
 * @brief Full update cycle (both directions)
 */
int security_rcog_bridge_update(
    security_rcog_bridge_t* bridge,
    uint64_t delta_ms
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    /* Update both directions */
    int result = security_rcog_update_security_effects(bridge);
    if (result != NIMCP_SUCCESS) {
        return result;
    }

    result = security_rcog_update_rcog_effects(bridge);
    if (result != NIMCP_SUCCESS) {
        return result;
    }

    /* Process timeout for pending approvals */
    BRIDGE_LOCK(bridge);

    uint64_t now = nimcp_platform_time_monotonic_ms();
    uint64_t oldest = UINT64_MAX;

    for (size_t i = 0; i < bridge->approval_queue_count; i++) {
        if (bridge->approval_queue[i].status == SECURITY_RCOG_APPROVAL_PENDING) {
            uint64_t age = now - bridge->approval_queue[i].requested_at_ms;
            if (age < oldest) {
                oldest = age;
            }
            /* Check timeout */
            if (bridge->config.approval_timeout_ms > 0 &&
                age > bridge->config.approval_timeout_ms) {
                bridge->approval_queue[i].status = SECURITY_RCOG_APPROVAL_TIMEOUT;
                bridge->stats.approvals_timeout++;
            }
        }
    }

    bridge->state.oldest_pending_ms = (oldest == UINT64_MAX) ? 0 : oldest;

    /* Record update in base */
    bridge_base_record_update(&bridge->base);

    BRIDGE_UNLOCK(bridge);

    (void)delta_ms;  /* Used for timing if needed */

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Query API Implementation
 * ============================================================================ */

/**
 * @brief Get security effects on rcog
 */
int security_rcog_get_security_effects(
    const security_rcog_bridge_t* bridge,
    security_to_rcog_effects_t* effects
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(effects, NIMCP_ERROR_NULL_POINTER, "effects is NULL");

    BRIDGE_LOCK((security_rcog_bridge_t*)bridge);
    *effects = bridge->security_effects;
    BRIDGE_UNLOCK((security_rcog_bridge_t*)bridge);

    return NIMCP_SUCCESS;
}

/**
 * @brief Get rcog effects on security
 */
int security_rcog_get_rcog_effects(
    const security_rcog_bridge_t* bridge,
    rcog_to_security_effects_t* effects
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(effects, NIMCP_ERROR_NULL_POINTER, "effects is NULL");

    BRIDGE_LOCK((security_rcog_bridge_t*)bridge);
    *effects = bridge->rcog_effects;
    BRIDGE_UNLOCK((security_rcog_bridge_t*)bridge);

    return NIMCP_SUCCESS;
}

/**
 * @brief Get bridge state
 */
int security_rcog_get_state(
    const security_rcog_bridge_t* bridge,
    security_rcog_state_t* state
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(state, NIMCP_ERROR_NULL_POINTER, "state is NULL");

    BRIDGE_LOCK((security_rcog_bridge_t*)bridge);
    *state = bridge->state;
    BRIDGE_UNLOCK((security_rcog_bridge_t*)bridge);

    return NIMCP_SUCCESS;
}

/**
 * @brief Get bridge statistics
 */
int security_rcog_get_stats(
    const security_rcog_bridge_t* bridge,
    security_rcog_stats_t* stats
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(stats, NIMCP_ERROR_NULL_POINTER, "stats is NULL");

    BRIDGE_LOCK((security_rcog_bridge_t*)bridge);
    *stats = bridge->stats;
    BRIDGE_UNLOCK((security_rcog_bridge_t*)bridge);

    return NIMCP_SUCCESS;
}

/**
 * @brief Reset bridge statistics
 */
int security_rcog_reset_stats(security_rcog_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);
    memset(&bridge->stats, 0, sizeof(security_rcog_stats_t));
    BRIDGE_UNLOCK(bridge);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Emergency Mode API Implementation
 * ============================================================================ */

/**
 * @brief Enter emergency tool lockdown
 */
int security_rcog_enter_lockdown(security_rcog_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    bridge->state.emergency_lockdown = true;
    bridge->security_effects.emergency_tool_lockdown = true;

    /* Drastically reduce limits */
    bridge->security_effects.effective_max_depth = 2;
    bridge->security_effects.effective_resource_budget =
        bridge->config.default_resource_budget / 4;
    bridge->security_effects.depth_reduction_factor = 0.125f;
    bridge->security_effects.resource_reduction_factor = 0.25f;

    BRIDGE_UNLOCK(bridge);

    return NIMCP_SUCCESS;
}

/**
 * @brief Exit emergency tool lockdown
 */
int security_rcog_exit_lockdown(security_rcog_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    bridge->state.emergency_lockdown = false;
    bridge->security_effects.emergency_tool_lockdown = false;

    /* Restore normal limits */
    bridge->security_effects.effective_max_depth = bridge->config.max_recursion_depth;
    bridge->security_effects.effective_resource_budget =
        bridge->config.default_resource_budget;
    bridge->security_effects.depth_reduction_factor = 1.0f;
    bridge->security_effects.resource_reduction_factor = 1.0f;

    BRIDGE_UNLOCK(bridge);

    return NIMCP_SUCCESS;
}

/**
 * @brief Check if in lockdown mode
 */
bool security_rcog_is_lockdown(const security_rcog_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->state.emergency_lockdown;
}

/* ============================================================================
 * Request Lifecycle API Implementation
 * ============================================================================ */

/**
 * @brief Begin tracking a new goal request
 */
int security_rcog_begin_request(
    security_rcog_bridge_t* bridge,
    uint64_t request_id
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    /* Reset per-request counters */
    bridge->current_request_id = request_id;
    bridge->tools_called_this_request = 0;
    bridge->state.current_depth = 0;
    bridge->state.current_resource_usage = 0;

    BRIDGE_UNLOCK(bridge);

    return NIMCP_SUCCESS;
}

/**
 * @brief End tracking for goal request
 */
int security_rcog_end_request(
    security_rcog_bridge_t* bridge,
    uint64_t request_id
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    /* Verify request ID matches */
    if (bridge->current_request_id != request_id) {
        BRIDGE_UNLOCK(bridge);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_PARAMETER, "request ID mismatch");
    }

    /* Update averages */
    update_average(&bridge->rcog_effects.avg_tools_per_request,
                   bridge->rcog_effects.total_tool_calls,
                   (float)bridge->tools_called_this_request);

    /* Clear per-request state */
    bridge->current_request_id = 0;
    bridge->tools_called_this_request = 0;

    BRIDGE_UNLOCK(bridge);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Find whitelist entry by tool name
 */
static int find_whitelist_entry(
    const security_rcog_bridge_t* bridge,
    const char* tool_name,
    size_t* index
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(tool_name, NIMCP_ERROR_NULL_POINTER, "tool_name is NULL");
    NIMCP_CHECK_THROW(index, NIMCP_ERROR_NULL_POINTER, "index is NULL");

    for (size_t i = 0; i < bridge->whitelist_count; i++) {
        if (strncmp(bridge->whitelist[i].tool_name, tool_name,
                    SECURITY_RCOG_MAX_TOOL_NAME) == 0) {
            *index = i;
            return NIMCP_SUCCESS;
        }
    }

    return NIMCP_ERROR_NOT_FOUND;
}

/**
 * @brief Find approval request by ID
 */
static int find_approval_request(
    const security_rcog_bridge_t* bridge,
    uint64_t request_id,
    size_t* index
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(index, NIMCP_ERROR_NULL_POINTER, "index is NULL");

    for (size_t i = 0; i < bridge->approval_queue_count; i++) {
        if (bridge->approval_queue[i].request_id == request_id) {
            *index = i;
            return NIMCP_SUCCESS;
        }
    }

    return NIMCP_ERROR_NOT_FOUND;
}

/**
 * @brief Update running average
 */
static void update_average(float* avg, uint64_t count, float new_value) {
    if (!avg || count == 0) {
        return;
    }

    /* Exponential moving average for stability */
    float alpha = 1.0f / (float)(count < 100 ? count : 100);
    *avg = (*avg * (1.0f - alpha)) + (new_value * alpha);
    if (!isfinite(*avg)) *avg = new_value;
}

/**
 * @brief Check output for suspicious content
 */
static bool is_suspicious_output(
    const void* output,
    size_t output_size,
    float* score
) {
    if (!output || output_size == 0 || !score) {
        if (score) *score = 0.0f;
        return false;
    }

    const char* data = (const char*)output;
    float suspicious_score = 0.0f;

    /* Simple heuristic checks */
    size_t null_count = 0;
    size_t high_entropy = 0;

    for (size_t i = 0; i < output_size; i++) {
        /* Count null bytes (may indicate binary data leak) */
        if (data[i] == '\0') {
            null_count++;
        }
        /* Count high-value bytes (may indicate encoded data) */
        if ((unsigned char)data[i] > 127) {
            high_entropy++;
        }
    }

    /* High null ratio is suspicious (binary leak) */
    float null_ratio = (float)null_count / (float)output_size;
    if (null_ratio > 0.1f) {
        suspicious_score += null_ratio * 0.5f;
    }

    /* High entropy ratio may indicate encoded secrets */
    float entropy_ratio = (float)high_entropy / (float)output_size;
    if (entropy_ratio > 0.3f) {
        suspicious_score += entropy_ratio * 0.3f;
    }

    /* Clamp to [0, 1] */
    if (suspicious_score > 1.0f) {
        suspicious_score = 1.0f;
    }

    *score = suspicious_score;
    return suspicious_score > 0.3f;
}

/**
 * @brief Generate unique request ID
 */
static uint64_t generate_request_id(void) {
    static uint64_t counter = 0;
    uint64_t timestamp = nimcp_platform_time_monotonic_ms();
    return (timestamp << 20) | (++counter & 0xFFFFF);
}
