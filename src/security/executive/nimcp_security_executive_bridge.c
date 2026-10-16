/**
 * @file nimcp_security_executive_bridge.c
 * @brief Security-Executive Bridge Implementation
 * @version 1.0.0
 * @date 2025-01-09
 *
 * WHAT: Bidirectional integration between security subsystem and executive control
 * WHY:  Executive tasks require authorization, resource limits, and capability checks.
 *       Security benefits from task audit trails and resource usage monitoring.
 * HOW:  Security authorizes tasks based on policy; executive reports task lifecycle
 *       for audit; security enforces resource budgets; executive enforces deadlines.
 *
 * @author NIMCP Development Team
 */

#include "security/executive/nimcp_security_executive_bridge.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(security_executive_bridge, MESH_ADAPTER_CATEGORY_SECURITY)


/* ============================================================================
 * Internal Constants
 * ============================================================================ */

/** Default audit buffer size */
#define DEFAULT_AUDIT_BUFFER_SIZE      64

/** Default resource warning threshold */
#define DEFAULT_RESOURCE_WARNING       0.8f

/** Module ID for bio-async */
#define SECURITY_EXEC_MODULE_ID        0x53454300  /* 'SEC\0' */

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get current timestamp in milliseconds
 */
static uint64_t get_timestamp_ms(void) {
    return nimcp_time_get_us() / 1000;
}

/**
 * @brief Get current timestamp in nanoseconds for performance tracking
 */
static uint64_t get_timestamp_ns(void) {
    return nimcp_time_get_us() * 1000;
}

/**
 * @brief Add audit record to circular buffer (unlocked)
 */
static int add_audit_record_unlocked(
    security_executive_bridge_t* bridge,
    const security_audit_record_t* record
) {
    if (!bridge->audit_buffer) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Store at head position */
    uint32_t max_entries = bridge->config.max_audit_entries;
    if (max_entries == 0) {
        max_entries = DEFAULT_AUDIT_BUFFER_SIZE;
    }

    bridge->audit_buffer[bridge->audit_head] = *record;
    bridge->audit_head = (bridge->audit_head + 1) % max_entries;

    if (bridge->audit_count < max_entries) {
        bridge->audit_count++;
    }

    bridge->stats.audit_events_logged++;
    bridge->state.audit_buffer_used = bridge->audit_count;

    return 0;
}

/**
 * @brief Update running average
 */
static float update_running_avg(float current_avg, uint64_t count, float new_value) {
    if (count == 0) {
        return new_value;
    }
    return (current_avg * (float)(count - 1) + new_value) / (float)count;
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

int security_executive_default_config(security_executive_config_t* config) {
    if (!config) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    memset(config, 0, sizeof(security_executive_config_t));

    /* Task Authorization */
    config->enable_task_authorization = true;
    config->enable_capability_checks = true;
    config->enable_policy_evaluation = true;
    config->strict_mode = false;

    /* Resource Management */
    config->enable_resource_limits = true;
    config->enable_rate_limiting = true;
    config->resource_check_interval_ms = SECURITY_EXEC_DEFAULT_RESOURCE_INTERVAL;
    config->resource_warning_threshold = DEFAULT_RESOURCE_WARNING;

    /* Deadline Enforcement */
    config->enable_deadline_enforcement = true;
    config->deadline_grace_period_ms = SECURITY_EXEC_DEFAULT_DEADLINE_GRACE;
    config->abort_on_deadline_exceeded = false;

    /* Audit Trail */
    config->enable_audit_logging = true;
    config->enable_encrypted_audit = false;
    config->max_audit_entries = SECURITY_EXEC_MAX_AUDIT_TASKS;

    /* Rollback */
    config->enable_secure_rollback = true;
    config->rollback_on_auth_failure = true;

    /* Sensitivity Factors */
    config->security_sensitivity = 1.0f;
    config->executive_sensitivity = 1.0f;

    return 0;
}

security_executive_bridge_t* security_executive_bridge_create(
    const security_executive_config_t* config
) {
    /* Allocate bridge structure */
    security_executive_bridge_t* bridge = nimcp_malloc(sizeof(security_executive_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate security_executive_bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "security_executive_bridge_create: allocation failed");

        return NULL;
    }
    memset(bridge, 0, sizeof(security_executive_bridge_t));

    /* Initialize config */
    if (config) {
        bridge->config = *config;
    } else {
        security_executive_default_config(&bridge->config);
    }

    /* Initialize base bridge */
    if (bridge_base_init(&bridge->base, SECURITY_EXEC_MODULE_ID, "security_executive_bridge") != 0) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "security_executive_bridge_create: validation failed");
        return NULL;
    }

    /* Allocate audit buffer */
    uint32_t audit_size = bridge->config.max_audit_entries;
    if (audit_size == 0) {
        audit_size = DEFAULT_AUDIT_BUFFER_SIZE;
    }

    bridge->audit_buffer = nimcp_malloc(sizeof(security_audit_record_t) * audit_size);
    if (!bridge->audit_buffer) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "security_executive_bridge_create: bridge->audit_buffer is NULL");
        return NULL;
    }
    memset(bridge->audit_buffer, 0, sizeof(security_audit_record_t) * audit_size);
    bridge->audit_head = 0;
    bridge->audit_count = 0;

    /* Initialize state */
    bridge->state.strict_mode_active = bridge->config.strict_mode;
    bridge->state.last_update_time = get_timestamp_ms();

    return bridge;
}

void security_executive_bridge_destroy(security_executive_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Free audit buffer */
    if (bridge->audit_buffer) {
        nimcp_free(bridge->audit_buffer);
        bridge->audit_buffer = NULL;
    }

    /* Cleanup base bridge */
    bridge_base_cleanup(&bridge->base);

    /* Free bridge structure */
    nimcp_free(bridge);
}

/* ============================================================================
 * Connection API Implementation
 * ============================================================================ */

int security_executive_bridge_connect_executive(
    security_executive_bridge_t* bridge,
    executive_controller_t* executive
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(executive);

    BRIDGE_LOCK(bridge);
    bridge->executive = executive;
    bridge->base.system_a = executive;
    bridge->base.system_a_connected = true;
    bridge->base.bridge_active = bridge->base.system_a_connected && bridge->base.system_b_connected;
    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_executive_bridge_connect_policy_engine(
    security_executive_bridge_t* bridge,
    nimcp_policy_engine_t policy_engine
) {
    BRIDGE_NULL_CHECK(bridge);
    if (!policy_engine) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    BRIDGE_LOCK(bridge);
    bridge->policy_engine = policy_engine;
    bridge->base.system_b = policy_engine;
    bridge->base.system_b_connected = true;
    bridge->base.bridge_active = bridge->base.system_a_connected && bridge->base.system_b_connected;
    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_executive_bridge_connect_rate_limiter(
    security_executive_bridge_t* bridge,
    nimcp_rate_limiter_t rate_limiter
) {
    BRIDGE_NULL_CHECK(bridge);
    if (!rate_limiter) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    BRIDGE_LOCK(bridge);
    bridge->rate_limiter = rate_limiter;
    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_executive_bridge_connect_capability_system(
    security_executive_bridge_t* bridge,
    nimcp_capability_system_t* capability_system
) {
    BRIDGE_NULL_CHECK(bridge);
    if (!capability_system) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    BRIDGE_LOCK(bridge);
    bridge->capability_system = capability_system;
    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_executive_bridge_disconnect(security_executive_bridge_t* bridge) {
    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);

    bridge->executive = NULL;
    bridge->policy_engine = NULL;
    bridge->rate_limiter = NULL;
    bridge->capability_system = NULL;

    bridge->base.system_a = NULL;
    bridge->base.system_b = NULL;
    bridge->base.system_a_connected = false;
    bridge->base.system_b_connected = false;
    bridge->base.bridge_active = false;

    BRIDGE_UNLOCK(bridge);

    return 0;
}

bool security_executive_bridge_is_connected(
    const security_executive_bridge_t* bridge
) {
    BRIDGE_NULL_CHECK_BOOL(bridge);
    return bridge->base.bridge_active;
}

/* ============================================================================
 * Security -> Executive Direction
 * ============================================================================ */

int security_executive_authorize_task(
    security_executive_bridge_t* bridge,
    const task_descriptor_t* task,
    uint32_t agent_id,
    const nimcp_capability_t* capabilities,
    uint32_t num_capabilities,
    security_auth_result_t* result
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(task);
    BRIDGE_NULL_CHECK(result);

    uint64_t start_time = get_timestamp_ns();

    BRIDGE_LOCK(bridge);

    memset(result, 0, sizeof(security_auth_result_t));
    result->auth_timestamp = get_timestamp_ms();

    bridge->stats.total_auth_requests++;

    /* Check rate limiting first */
    if (bridge->config.enable_rate_limiting && bridge->rate_limiter) {
        char client_id[32];
        snprintf(client_id, sizeof(client_id), "agent_%u", agent_id);

        if (!nimcp_rate_limiter_allow(bridge->rate_limiter, client_id)) {
            result->authorized = false;
            result->decision = SECURITY_AUTH_DENIED_RATE_LIMIT;
            snprintf(result->denied_reason, SECURITY_EXEC_MAX_REASON_LEN,
                     "Rate limit exceeded for agent %u", agent_id);
            bridge->stats.auth_denied_rate_limit++;
            bridge->security_effects.rate_limited = true;
            BRIDGE_UNLOCK(bridge);
            return 0;
        }
    }

    /* Check capabilities if enabled */
    if (bridge->config.enable_capability_checks && bridge->capability_system) {
        security_capability_check_t cap_check;
        int cap_result = security_executive_check_capabilities(
            bridge, agent_id, capabilities, num_capabilities, NIMCP_PERM_EXECUTE, &cap_check);

        if (cap_result == 0 && !cap_check.has_all_capabilities) {
            result->authorized = false;
            result->decision = SECURITY_AUTH_DENIED_CAPABILITY;
            snprintf(result->denied_reason, SECURITY_EXEC_MAX_REASON_LEN,
                     "Missing %u required capabilities", cap_check.num_missing);
            bridge->stats.auth_denied_capability++;

            /* Copy missing capabilities */
            result->num_required_capabilities = cap_check.num_missing;
            for (uint32_t i = 0; i < cap_check.num_missing && i < SECURITY_EXEC_MAX_CAPABILITIES; i++) {
                result->required_capabilities[i] = cap_check.missing_capabilities[i];
            }

            BRIDGE_UNLOCK(bridge);
            return 0;
        }
    }

    /* Check policy engine if enabled */
    if (bridge->config.enable_policy_evaluation && bridge->policy_engine) {
        nimcp_policy_context_t ctx = nimcp_policy_context_create();
        if (ctx) {
            nimcp_policy_context_set_int(ctx, "agent_id", agent_id);
            nimcp_policy_context_set_int(ctx, "task_id", task->task_id);
            nimcp_policy_context_set_int(ctx, "task_type", task->type);
            nimcp_policy_context_set_int(ctx, "priority", task->priority);
            nimcp_policy_context_set_string(ctx, "task_name", task->name);

            nimcp_policy_result_t policy_result;
            nimcp_error_t policy_err = nimcp_policy_evaluate(bridge->policy_engine, ctx, &policy_result);

            if (policy_err == NIMCP_SUCCESS && policy_result.action == NIMCP_POLICY_ACTION_DENY) {
                result->authorized = false;
                result->decision = SECURITY_AUTH_DENIED_POLICY;
                if (policy_result.rule_name) {
                    snprintf(result->denied_reason, SECURITY_EXEC_MAX_REASON_LEN,
                             "Denied by policy rule: %s", policy_result.rule_name);
                } else {
                    snprintf(result->denied_reason, SECURITY_EXEC_MAX_REASON_LEN,
                             "Task denied by security policy");
                }
                bridge->stats.auth_denied_policy++;
                nimcp_policy_result_free(&policy_result);
                nimcp_policy_context_destroy(ctx);
                BRIDGE_UNLOCK(bridge);
                return 0;
            }

            nimcp_policy_result_free(&policy_result);
            nimcp_policy_context_destroy(ctx);
        }
    }

    /* Authorization granted */
    result->authorized = true;
    result->decision = SECURITY_AUTH_ALLOWED;

    /* Allocate default resource budgets */
    result->cpu_budget_ms = 10000;     /* 10 seconds CPU */
    result->memory_budget_bytes = 1024 * 1024 * 100;  /* 100 MB */
    result->time_budget_ms = 60000;    /* 1 minute wall time */

    bridge->stats.auth_granted++;
    bridge->state.active_tasks++;

    /* Record auth time */
    result->auth_time_ns = get_timestamp_ns() - start_time;
    bridge->stats.avg_auth_time_ns = update_running_avg(
        bridge->stats.avg_auth_time_ns,
        bridge->stats.total_auth_requests,
        (float)result->auth_time_ns);

    bridge->state.last_authorization_time = get_timestamp_ms();

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_executive_check_capabilities(
    security_executive_bridge_t* bridge,
    uint32_t agent_id,
    const nimcp_capability_t* capabilities,
    uint32_t num_capabilities,
    uint32_t required,
    security_capability_check_t* result
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(result);

    memset(result, 0, sizeof(security_capability_check_t));

    /* If no capability system connected, assume all capabilities present */
    if (!bridge->capability_system) {
        result->has_all_capabilities = true;
        return 0;
    }

    BRIDGE_LOCK(bridge);

    uint32_t found_permissions = 0;
    result->num_missing = 0;

    /* Check each provided capability */
    for (uint32_t i = 0; i < num_capabilities; i++) {
        if (nimcp_capability_is_valid(bridge->capability_system, capabilities[i])) {
            uint32_t perms = nimcp_capability_get_permissions(
                bridge->capability_system, capabilities[i]);
            found_permissions |= perms;
        }
    }

    /* Check if all required permissions are met */
    if ((found_permissions & required) == required) {
        result->has_all_capabilities = true;
    } else {
        result->has_all_capabilities = false;

        /* Identify missing permissions */
        uint32_t missing = required & ~found_permissions;
        uint32_t idx = 0;

        for (uint32_t bit = 1; bit != 0 && idx < SECURITY_EXEC_MAX_CAPABILITIES; bit <<= 1) {
            if (missing & bit) {
                result->missing_capabilities[idx++] = bit;
            }
        }
        result->num_missing = idx;
    }

    result->can_delegate = (found_permissions & NIMCP_PERM_DELEGATE) != 0;

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_executive_allocate_resources(
    security_executive_bridge_t* bridge,
    uint32_t task_id,
    const security_resource_budget_t* requested,
    security_resource_budget_t* granted
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(requested);
    BRIDGE_NULL_CHECK(granted);

    BRIDGE_LOCK(bridge);

    memset(granted, 0, sizeof(security_resource_budget_t));

    /* Copy limits (may apply caps based on policy) */
    granted->cpu_limit_ms = requested->cpu_limit_ms;
    granted->memory_limit_bytes = requested->memory_limit_bytes;
    granted->time_limit_ms = requested->time_limit_ms;
    granted->io_read_limit = requested->io_read_limit;
    granted->io_write_limit = requested->io_write_limit;

    /* Apply resource caps if enabled */
    if (bridge->config.enable_resource_limits) {
        /* Cap maximum values for security */
        if (granted->cpu_limit_ms > 60000) {
            granted->cpu_limit_ms = 60000;  /* Max 1 minute CPU */
        }
        if (granted->memory_limit_bytes > 1024ULL * 1024 * 1024) {
            granted->memory_limit_bytes = 1024ULL * 1024 * 1024;  /* Max 1 GB */
        }
        if (granted->time_limit_ms > 300000) {
            granted->time_limit_ms = 300000;  /* Max 5 minutes wall time */
        }
    }

    granted->budget_exceeded = false;

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_executive_enforce_deadline(
    security_executive_bridge_t* bridge,
    uint32_t task_id,
    uint64_t deadline_ms
) {
    BRIDGE_NULL_CHECK(bridge);

    if (!bridge->config.enable_deadline_enforcement) {
        return 0;
    }

    BRIDGE_LOCK(bridge);

    uint64_t now = get_timestamp_ms();
    bridge->stats.deadline_checks++;

    if (now > deadline_ms) {
        /* Deadline exceeded */
        bridge->stats.deadline_violations++;
        bridge->state.tasks_past_deadline++;
        bridge->security_effects.deadline_exceeded_count++;

        /* Log audit event */
        if (bridge->config.enable_audit_logging) {
            security_audit_record_t record;
            memset(&record, 0, sizeof(record));
            record.task_id = task_id;
            record.event = SECURITY_AUDIT_DEADLINE_EXCEEDED;
            record.timestamp = now;
            add_audit_record_unlocked(bridge, &record);
        }

        BRIDGE_UNLOCK(bridge);

        if (bridge->config.abort_on_deadline_exceeded) {
            return NIMCP_ERROR_TIMEOUT;
        }
        return 0;
    }

    /* Check for deadline warning (within grace period) */
    uint64_t warning_threshold = deadline_ms - bridge->config.deadline_grace_period_ms;
    if (now > warning_threshold) {
        bridge->stats.deadline_warnings++;
        bridge->state.tasks_near_deadline++;
        bridge->security_effects.deadline_warning_count++;

        if (bridge->config.enable_audit_logging) {
            security_audit_record_t record;
            memset(&record, 0, sizeof(record));
            record.task_id = task_id;
            record.event = SECURITY_AUDIT_DEADLINE_WARNING;
            record.timestamp = now;
            add_audit_record_unlocked(bridge, &record);
        }
    }

    BRIDGE_UNLOCK(bridge);

    return 0;
}

/* ============================================================================
 * Executive -> Security Direction
 * ============================================================================ */

int security_executive_audit_task_start(
    security_executive_bridge_t* bridge,
    const task_descriptor_t* task,
    uint32_t agent_id
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(task);

    if (!bridge->config.enable_audit_logging) {
        return 0;
    }

    BRIDGE_LOCK(bridge);

    security_audit_record_t record;
    memset(&record, 0, sizeof(record));

    record.task_id = task->task_id;
    record.event = SECURITY_AUDIT_TASK_STARTED;
    record.timestamp = get_timestamp_ms();
    record.agent_id = agent_id;

    /* Copy task details */
    strncpy(record.task_name, task->name, sizeof(record.task_name) - 1);
    record.task_type = task->type;
    record.priority = task->priority;

    add_audit_record_unlocked(bridge, &record);

    bridge->executive_effects.tasks_started++;
    bridge->state.pending_authorizations++;

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_executive_audit_task_completion(
    security_executive_bridge_t* bridge,
    const task_descriptor_t* task,
    bool success,
    int result_code,
    const security_resource_budget_t* resources
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(task);

    if (!bridge->config.enable_audit_logging) {
        return 0;
    }

    BRIDGE_LOCK(bridge);

    security_audit_record_t record;
    memset(&record, 0, sizeof(record));

    record.task_id = task->task_id;
    record.event = success ? SECURITY_AUDIT_TASK_COMPLETED : SECURITY_AUDIT_TASK_FAILED;
    record.timestamp = get_timestamp_ms();
    record.result_code = result_code;

    /* Copy task details */
    strncpy(record.task_name, task->name, sizeof(record.task_name) - 1);
    record.task_type = task->type;
    record.priority = task->priority;

    /* Calculate execution time */
    if (task->started_ms > 0) {
        record.execution_time_ms = record.timestamp - task->started_ms;
    }

    /* Copy resource usage if provided */
    if (resources) {
        record.resources = *resources;
    }

    add_audit_record_unlocked(bridge, &record);

    /* Update executive effects */
    if (success) {
        bridge->executive_effects.tasks_completed++;
    } else {
        bridge->executive_effects.tasks_failed++;

        /* Update failure rate */
        uint32_t total = bridge->executive_effects.tasks_completed +
                        bridge->executive_effects.tasks_failed;
        if (total > 0) {
            bridge->executive_effects.task_failure_rate =
                (float)bridge->executive_effects.tasks_failed / (float)total;
        }
    }

    /* Update state */
    if (bridge->state.active_tasks > 0) {
        bridge->state.active_tasks--;
    }

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_executive_rollback_task(
    security_executive_bridge_t* bridge,
    uint32_t task_id,
    const char* reason,
    security_rollback_status_t* status
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(status);

    if (!bridge->config.enable_secure_rollback) {
        *status = SECURITY_ROLLBACK_NOT_NEEDED;
        return 0;
    }

    BRIDGE_LOCK(bridge);

    bridge->stats.rollback_attempts++;

    /* Log rollback event */
    if (bridge->config.enable_audit_logging) {
        security_audit_record_t record;
        memset(&record, 0, sizeof(record));
        record.task_id = task_id;
        record.event = SECURITY_AUDIT_TASK_ROLLBACK;
        record.timestamp = get_timestamp_ms();
        add_audit_record_unlocked(bridge, &record);
    }

    /* Perform rollback - in real implementation would restore state */
    *status = SECURITY_ROLLBACK_SUCCESS;
    bridge->stats.rollback_successes++;

    /* Update state */
    if (bridge->state.active_tasks > 0) {
        bridge->state.active_tasks--;
    }

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_executive_report_resource_usage(
    security_executive_bridge_t* bridge,
    uint32_t task_id,
    const security_resource_budget_t* resources
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(resources);

    BRIDGE_LOCK(bridge);

    bridge->stats.resource_checks++;

    /* Update aggregate resource tracking */
    bridge->executive_effects.total_cpu_used_ms += resources->cpu_used_ms;
    bridge->executive_effects.total_memory_bytes = resources->memory_used_bytes;

    /* Check for budget exceeded */
    if (resources->budget_exceeded) {
        bridge->stats.resource_violations++;
        bridge->state.any_budget_exceeded = true;

        if (bridge->config.enable_audit_logging) {
            security_audit_record_t record;
            memset(&record, 0, sizeof(record));
            record.task_id = task_id;
            record.event = SECURITY_AUDIT_RESOURCE_EXCEEDED;
            record.timestamp = get_timestamp_ms();
            record.resources = *resources;
            add_audit_record_unlocked(bridge, &record);
        }
    }

    /* Check warning threshold */
    float utilization = 0.0f;
    if (resources->cpu_limit_ms > 0) {
        utilization = (float)resources->cpu_used_ms / (float)resources->cpu_limit_ms;
    }

    if (utilization > bridge->config.resource_warning_threshold) {
        bridge->stats.resource_warnings++;
        bridge->state.tasks_at_resource_limit++;

        if (bridge->config.enable_audit_logging) {
            security_audit_record_t record;
            memset(&record, 0, sizeof(record));
            record.task_id = task_id;
            record.event = SECURITY_AUDIT_RESOURCE_WARNING;
            record.timestamp = get_timestamp_ms();
            record.resources = *resources;
            add_audit_record_unlocked(bridge, &record);
        }
    }

    /* Update overall utilization */
    bridge->state.overall_resource_utilization = utilization;
    bridge->security_effects.resource_utilization = utilization;

    if (utilization > bridge->config.resource_warning_threshold) {
        bridge->security_effects.resource_constrained = true;
    }

    bridge->state.last_resource_check_time = get_timestamp_ms();

    BRIDGE_UNLOCK(bridge);

    return 0;
}

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

int security_executive_bridge_update(
    security_executive_bridge_t* bridge,
    uint64_t delta_ms
) {
    BRIDGE_NULL_CHECK(bridge);

    uint64_t start_time = get_timestamp_ns();

    BRIDGE_LOCK(bridge);

    bridge->stats.bridge_updates++;
    bridge->state.last_update_time = get_timestamp_ms();

    /* Reset periodic counters */
    bridge->security_effects.blocked_task_count = 0;
    bridge->security_effects.deadline_warning_count = 0;
    bridge->security_effects.deadline_exceeded_count = 0;

    /* Update rate limiting state */
    if (bridge->rate_limiter) {
        nimcp_rate_limit_stats_t rate_stats;
        if (nimcp_rate_limiter_get_stats(bridge->rate_limiter, &rate_stats) == NIMCP_SUCCESS) {
            bridge->state.rate_limiting_active = (rate_stats.requests_denied > 0);
            bridge->security_effects.rate_limited = bridge->state.rate_limiting_active;
        }
    }

    /* Record update time */
    uint64_t update_time = get_timestamp_ns() - start_time;
    bridge->stats.avg_update_time_ns = update_running_avg(
        bridge->stats.avg_update_time_ns,
        bridge->stats.bridge_updates,
        (float)update_time);

    /* Record in base bridge */
    bridge_base_record_update(&bridge->base);

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_executive_apply_security_effects(
    security_executive_bridge_t* bridge
) {
    BRIDGE_NULL_CHECK(bridge);

    if (!bridge->executive) {
        return 0;
    }

    BRIDGE_LOCK(bridge);

    /* Apply effects to executive - in full implementation would:
     * - Block tasks that failed authorization
     * - Apply resource constraints
     * - Signal rate limiting state
     */

    float sensitivity = nimcp_clampf(bridge->config.security_sensitivity, 0.5f, 2.0f);

    /* Scale effects by sensitivity */
    bridge->security_effects.resource_utilization *= sensitivity;

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_executive_apply_executive_effects(
    security_executive_bridge_t* bridge
) {
    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);

    /* Check for anomalies in task patterns */
    if (bridge->executive_effects.task_failure_rate > 0.5f) {
        bridge->executive_effects.unusual_task_pattern = true;
    }

    /* Check for resource spikes */
    if (bridge->state.overall_resource_utilization > bridge->config.resource_warning_threshold) {
        bridge->executive_effects.resource_spike = bridge->state.overall_resource_utilization;
    }

    /* Update active task count in effects */
    bridge->executive_effects.active_task_count = bridge->state.active_tasks;

    float sensitivity = nimcp_clampf(bridge->config.executive_sensitivity, 0.5f, 2.0f);
    (void)sensitivity;  /* Used for scaling in full implementation */

    BRIDGE_UNLOCK(bridge);

    return 0;
}

/* ============================================================================
 * Query API Implementation
 * ============================================================================ */

int security_executive_bridge_get_state(
    const security_executive_bridge_t* bridge,
    security_executive_state_t* state
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(state);

    security_executive_bridge_t* mutable_bridge = (security_executive_bridge_t*)bridge;

    BRIDGE_LOCK(mutable_bridge);
    *state = bridge->state;
    BRIDGE_UNLOCK(mutable_bridge);

    return 0;
}

int security_executive_bridge_get_stats(
    const security_executive_bridge_t* bridge,
    security_executive_stats_t* stats
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(stats);

    security_executive_bridge_t* mutable_bridge = (security_executive_bridge_t*)bridge;

    BRIDGE_LOCK(mutable_bridge);
    *stats = bridge->stats;
    BRIDGE_UNLOCK(mutable_bridge);

    return 0;
}

int security_executive_get_security_effects(
    const security_executive_bridge_t* bridge,
    security_to_executive_effects_t* effects
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(effects);

    security_executive_bridge_t* mutable_bridge = (security_executive_bridge_t*)bridge;

    BRIDGE_LOCK(mutable_bridge);
    *effects = bridge->security_effects;
    BRIDGE_UNLOCK(mutable_bridge);

    return 0;
}

int security_executive_get_executive_effects(
    const security_executive_bridge_t* bridge,
    executive_to_security_effects_t* effects
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(effects);

    security_executive_bridge_t* mutable_bridge = (security_executive_bridge_t*)bridge;

    BRIDGE_LOCK(mutable_bridge);
    *effects = bridge->executive_effects;
    BRIDGE_UNLOCK(mutable_bridge);

    return 0;
}

int security_executive_get_audit_records(
    const security_executive_bridge_t* bridge,
    security_audit_record_t* records,
    uint32_t max_records,
    uint32_t* num_records
) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(records);
    BRIDGE_NULL_CHECK(num_records);

    if (max_records == 0) {
        *num_records = 0;
        return 0;
    }

    security_executive_bridge_t* mutable_bridge = (security_executive_bridge_t*)bridge;

    BRIDGE_LOCK(mutable_bridge);

    uint32_t count = bridge->audit_count;
    if (count > max_records) {
        count = max_records;
    }

    uint32_t max_entries = bridge->config.max_audit_entries;
    if (max_entries == 0) {
        max_entries = DEFAULT_AUDIT_BUFFER_SIZE;
    }

    /* Copy records from circular buffer, oldest first */
    uint32_t read_idx;
    if (bridge->audit_count < max_entries) {
        read_idx = 0;
    } else {
        read_idx = bridge->audit_head;
    }

    for (uint32_t i = 0; i < count; i++) {
        records[i] = bridge->audit_buffer[read_idx];
        read_idx = (read_idx + 1) % max_entries;
    }

    *num_records = count;

    BRIDGE_UNLOCK(mutable_bridge);

    return 0;
}

int security_executive_flush_audit(security_executive_bridge_t* bridge) {
    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);

    /* In a full implementation, would persist to storage */
    bridge->stats.audit_flushes++;
    bridge->state.last_audit_flush = get_timestamp_ms();

    /* Clear buffer after flush */
    bridge->audit_head = 0;
    bridge->audit_count = 0;
    bridge->state.audit_buffer_used = 0;

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_executive_bridge_reset_stats(security_executive_bridge_t* bridge) {
    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);
    memset(&bridge->stats, 0, sizeof(security_executive_stats_t));
    BRIDGE_UNLOCK(bridge);

    return 0;
}

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

int security_executive_bridge_connect_bio_async(
    security_executive_bridge_t* bridge
) {
    BRIDGE_NULL_CHECK(bridge);
    return bridge_base_connect_bio_async(&bridge->base);
}

int security_executive_bridge_disconnect_bio_async(
    security_executive_bridge_t* bridge
) {
    BRIDGE_NULL_CHECK(bridge);
    return bridge_base_disconnect_bio_async(&bridge->base);
}

bool security_executive_bridge_is_bio_async_connected(
    const security_executive_bridge_t* bridge
) {
    BRIDGE_NULL_CHECK_BOOL(bridge);
    return bridge_base_is_bio_async_connected(&bridge->base);
}
