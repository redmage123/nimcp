/**
 * @file nimcp_capability_control.c
 * @brief Capability Control Implementation
 * @version 1.0.0
 * @date 2026-02-01
 *
 * WHAT: Implementation of capability enforcement
 * WHY:  Prevent capability escalation and unauthorized actions
 * HOW:  Envelope checking, resource monitoring, SAT verification
 */

#include "security/nimcp_capability_control.h"
#include "security/nimcp_corrigibility.h"
#include "utils/error/nimcp_error_codes.h"
#include "mesh/nimcp_mesh_sat_solver.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/thread/nimcp_thread.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

#define LOG_CATEGORY "capability_control"
#define MAX_ACTION_HISTORY 1000

#include "utils/exception/nimcp_exception_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "constants/nimcp_buffer_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(capability_control, MESH_ADAPTER_CATEGORY_SECURITY)


/* SAT variable names for capability constraints */
static const char* SELF_MOD_VAR_NAMES[] = {
    "can_modify_own_weights",
    "can_modify_own_code",
    "can_modify_safety_systems",
    "can_modify_logging",
    "can_modify_monitoring",
    "can_modify_constraints"
};
#define SELF_MOD_VAR_COUNT (sizeof(SELF_MOD_VAR_NAMES) / sizeof(SELF_MOD_VAR_NAMES[0]))

/* ============================================================================
 * Internal Types
 * ============================================================================ */

/**
 * @brief Capability control internal state
 */
struct capability_control {
    uint32_t magic;
    nimcp_mutex_t* mutex;

    /* Configuration */
    capability_control_config_t config;

    /* Current resource usage */
    capability_resource_usage_t current_usage;

    /* Action history */
    capability_action_record_t action_history[MAX_ACTION_HISTORY];
    size_t action_history_count;
    size_t action_history_index;

    /* Statistics */
    capability_control_stats_t stats;

    /* Integration handles */
    void* tripwires;
    void* corrigibility;  /* Bidirectional integration with corrigibility */

    /* Bio-async integration */
    bio_module_context_t bio_ctx;
    bool bio_async_connected;

    /* SAT variables */
    uint32_t self_mod_vars[SELF_MOD_VAR_COUNT];
    bool sat_vars_initialized;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Validate capability control handle
 */
static bool is_valid_handle(const capability_control_t* system)
{
    return system != NULL && system->magic == CAPABILITY_CONTROL_MAGIC;
}

/**
 * @brief Get current time in microseconds
 */
static uint64_t get_time_us(void)
{
    return nimcp_time_now_us();
}

/**
 * @brief Copy string safely with null termination
 */
static void safe_strcpy(char* dest, const char* src, size_t max_len)
{
    if (dest == NULL || max_len == 0) return;
    if (src == NULL) { dest[0] = '\0'; return; }
    size_t len = strlen(src);
    if (len >= max_len) len = max_len - 1;
    memcpy(dest, src, len);
    dest[len] = '\0';
}

/**
 * @brief Check if all self-modification flags are false
 */
static bool check_self_mod_disabled(const self_mod_capability_t* self_mod)
{
    return !self_mod->can_modify_own_weights &&
           !self_mod->can_modify_own_code &&
           !self_mod->can_modify_safety_systems &&
           !self_mod->can_modify_logging &&
           !self_mod->can_modify_monitoring &&
           !self_mod->can_modify_constraints;
}

/**
 * @brief Add action to history
 */
static void add_action_to_history(
    capability_control_t* system,
    const capability_action_t* action,
    bool allowed,
    const char* denial_reason)
{
    size_t idx = system->action_history_index;
    capability_action_record_t* record = &system->action_history[idx];

    record->timestamp = get_time_us();
    memcpy(&record->action, action, sizeof(*action));
    record->was_allowed = allowed;
    if (denial_reason != NULL) {
        safe_strcpy(record->denial_reason, denial_reason, CAPABILITY_REASON_MAX_LENGTH);
    } else {
        record->denial_reason[0] = '\0';
    }

    system->action_history_index = (idx + 1) % MAX_ACTION_HISTORY;
    if (system->action_history_count < MAX_ACTION_HISTORY) {
        system->action_history_count++;
    }
}

/**
 * @brief Check if domain is in allowed list
 */
static bool is_domain_allowed(
    const capability_control_t* system,
    const char* domain)
{
    if (domain == NULL || domain[0] == '\0') {
        return false;
    }

    for (uint32_t i = 0; i < system->config.envelope.network.allowed_domain_count; i++) {
        if (capability_domain_matches(domain,
            system->config.envelope.network.allowed_domains[i])) {
            return true;
        }
    }
    return false;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

capability_envelope_t capability_envelope_safe(void)
{
    capability_envelope_t envelope;
    memset(&envelope, 0, sizeof(envelope));

    /* Network: very restricted */
    envelope.network.can_access_network = false;
    envelope.network.can_make_http_requests = false;
    envelope.network.can_make_https_requests = false;
    envelope.network.can_use_websockets = false;
    envelope.network.can_make_financial_transactions = false;
    envelope.network.can_access_external_apis = false;
    envelope.network.max_concurrent_connections = 0;
    envelope.network.max_bandwidth_bytes_per_sec = 0;

    /* Self-modification: all disabled */
    envelope.self_mod.can_modify_own_weights = false;
    envelope.self_mod.can_modify_own_code = false;
    envelope.self_mod.can_modify_safety_systems = false;
    envelope.self_mod.can_modify_logging = false;
    envelope.self_mod.can_modify_monitoring = false;
    envelope.self_mod.can_modify_constraints = false;

    /* Resources: reasonable limits */
    envelope.resources.max_memory_bytes = 1024 * 1024 * 512;  /* 512 MB */
    envelope.resources.max_compute_flops_per_second = 1e12;   /* 1 TFLOP */
    envelope.resources.max_concurrent_actions = 10;
    envelope.resources.max_cpu_percent = 50.0f;
    envelope.resources.max_disk_usage_bytes = 1024 * 1024 * 1024;  /* 1 GB */
    envelope.resources.max_threads = 8;
    envelope.resources.max_network_requests_per_minute = 0;

    /* Information: restricted */
    envelope.information.can_access_training_data = false;
    envelope.information.can_access_user_data = false;
    envelope.information.can_access_system_data = false;
    envelope.information.can_access_other_ai_systems = false;
    envelope.information.can_exfiltrate_data = false;
    envelope.information.can_store_persistent_data = false;

    /* Physical: disabled */
    envelope.physical.can_control_actuators = false;
    envelope.physical.can_communicate_externally = false;
    envelope.physical.can_affect_real_world = false;
    envelope.physical.requires_human_approval = true;

    /* Other */
    envelope.can_persist_beyond_session = false;
    envelope.can_spawn_subprocesses = false;
    envelope.can_spawn_monitored_only = true;
    envelope.can_request_capability_increase = false;
    envelope.requires_human_approval_for_increase = true;

    return envelope;
}

capability_control_config_t capability_control_default_config(void)
{
    capability_control_config_t config;
    memset(&config, 0, sizeof(config));

    config.envelope = capability_envelope_safe();
    config.enable_continuous_monitoring = true;
    config.check_interval_ms = 1000;
    config.alert_on_violation = true;
    config.log_all_actions = true;
    config.max_action_history = MAX_ACTION_HISTORY;

    return config;
}

capability_control_t* capability_control_create(
    const capability_control_config_t* config)
{
    capability_control_t* system = nimcp_calloc(1, sizeof(capability_control_t));
    if (system == NULL) {
        NIMCP_LOG_ERROR(LOG_CATEGORY, "Failed to allocate capability control");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "capability_control_create: validation failed");
        return NULL;
    }

    /* Initialize mutex */
    system->mutex = nimcp_mutex_create(NULL);
    if (system->mutex == NULL) {
        NIMCP_LOG_ERROR(LOG_CATEGORY, "Failed to create mutex");
        nimcp_free(system);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "capability_control_create: validation failed");
        return NULL;
    }

    /* Apply configuration */
    if (config != NULL) {
        memcpy(&system->config, config, sizeof(*config));
    } else {
        system->config = capability_control_default_config();
    }

    /* Verify self-modification is disabled */
    if (!check_self_mod_disabled(&system->config.envelope.self_mod)) {
        NIMCP_LOG_ERROR(LOG_CATEGORY,
            "Configuration error: self-modification must be disabled");
        nimcp_mutex_destroy(system->mutex);
        nimcp_free(system);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "capability_control_create: check_self_mod_disabled is NULL");
        return NULL;
    }

    /* Set magic */
    system->magic = CAPABILITY_CONTROL_MAGIC;

    NIMCP_LOG_INFO(LOG_CATEGORY,
        "Capability control created (network=%s, self_mod=disabled)",
        system->config.envelope.network.can_access_network ? "enabled" : "disabled");

    return system;
}

void capability_control_destroy(capability_control_t* system)
{
    if (!is_valid_handle(system)) {
        return;
    }

    /* Disconnect from bio-async */
    if (system->bio_async_connected) {
        bio_router_unregister_module(system->bio_ctx);
    }

    /* Invalidate magic */
    system->magic = 0;

    /* Destroy mutex */
    if (system->mutex != NULL) {
        nimcp_mutex_destroy(system->mutex);
    }

    nimcp_free(system);

    NIMCP_LOG_INFO(LOG_CATEGORY, "Capability control destroyed");
}

/* ============================================================================
 * Constraint Verification API
 * ============================================================================ */

nimcp_error_t capability_control_verify_envelope(
    capability_control_t* system,
    sat_solver_t* sat,
    bool* valid,
    char* violation_report,
    size_t report_size)
{
    if (!is_valid_handle(system) || valid == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT, "capability_control: error condition");
        return NIMCP_ERROR_INVALID_ARGUMENT;
    }

    nimcp_mutex_lock(system->mutex);

    *valid = true;
    if (violation_report != NULL && report_size > 0) {
        violation_report[0] = '\0';
    }

    /* Verify self-modification is disabled */
    if (!check_self_mod_disabled(&system->config.envelope.self_mod)) {
        *valid = false;
        if (violation_report != NULL && report_size > 0) {
            safe_strcpy(violation_report,
                "Self-modification capabilities must be disabled",
                report_size);
        }
    }

    /* Verify exfiltration is disabled */
    if (system->config.envelope.information.can_exfiltrate_data) {
        *valid = false;
        if (violation_report != NULL && report_size > 0 && violation_report[0] == '\0') {
            safe_strcpy(violation_report,
                "Data exfiltration must be disabled",
                report_size);
        }
    }

    /* Verify training data access is disabled */
    if (system->config.envelope.information.can_access_training_data) {
        *valid = false;
        if (violation_report != NULL && report_size > 0 && violation_report[0] == '\0') {
            safe_strcpy(violation_report,
                "Training data access must be disabled",
                report_size);
        }
    }

    nimcp_mutex_unlock(system->mutex);

    NIMCP_LOG_DEBUG(LOG_CATEGORY, "Envelope verification: %s",
        *valid ? "VALID" : "INVALID");

    return NIMCP_OK;
}

nimcp_error_t capability_control_verify_no_escalation(
    capability_control_t* system,
    sat_solver_t* sat,
    bool* escalation_possible)
{
    if (!is_valid_handle(system) || escalation_possible == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT, "capability_control: error condition");
        return NIMCP_ERROR_INVALID_ARGUMENT;
    }

    nimcp_mutex_lock(system->mutex);

    /* With self-modification disabled, escalation is not possible */
    *escalation_possible = !check_self_mod_disabled(&system->config.envelope.self_mod);

    /* Also check if capability increase requests are allowed without approval */
    if (system->config.envelope.can_request_capability_increase &&
        !system->config.envelope.requires_human_approval_for_increase) {
        *escalation_possible = true;
    }

    nimcp_mutex_unlock(system->mutex);

    NIMCP_LOG_DEBUG(LOG_CATEGORY, "Escalation check: %s",
        *escalation_possible ? "POSSIBLE" : "NOT POSSIBLE");

    return NIMCP_OK;
}

/* ============================================================================
 * Action Checking API
 * ============================================================================ */

nimcp_error_t capability_control_check_action(
    capability_control_t* system,
    const capability_action_t* action,
    capability_check_result_t* result)
{
    if (!is_valid_handle(system) || action == NULL || result == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT, "capability_control: error condition");
        return NIMCP_ERROR_INVALID_ARGUMENT;
    }

    nimcp_mutex_lock(system->mutex);

    memset(result, 0, sizeof(*result));
    result->allowed = true;

    switch (action->category) {
    case CAPABILITY_NETWORK:
        if (!system->config.envelope.network.can_access_network) {
            result->allowed = false;
            result->violated_category = CAPABILITY_NETWORK;
            safe_strcpy(result->denial_reason, "Network access disabled",
                       CAPABILITY_REASON_MAX_LENGTH);
            system->stats.network_violations++;
        } else if (action->is_financial &&
                   !system->config.envelope.network.can_make_financial_transactions) {
            result->allowed = false;
            result->violated_category = CAPABILITY_NETWORK;
            safe_strcpy(result->denial_reason, "Financial transactions disabled",
                       CAPABILITY_REASON_MAX_LENGTH);
            system->stats.network_violations++;
        } else if (!is_domain_allowed(system, action->target_domain)) {
            result->allowed = false;
            result->violated_category = CAPABILITY_NETWORK;
            snprintf(result->denial_reason, CAPABILITY_REASON_MAX_LENGTH,
                    "Domain not in allowed list: %s", action->target_domain);
            system->stats.network_violations++;
        }
        break;

    case CAPABILITY_SELF_MODIFICATION:
        /* Self-modification is always denied */
        result->allowed = false;
        result->violated_category = CAPABILITY_SELF_MODIFICATION;
        safe_strcpy(result->denial_reason, "Self-modification not allowed",
                   CAPABILITY_REASON_MAX_LENGTH);
        system->stats.self_mod_violations++;
        break;

    case CAPABILITY_RESOURCE:
        /* Check resource limits */
        if (action->memory_required > system->config.envelope.resources.max_memory_bytes) {
            result->allowed = false;
            result->violated_category = CAPABILITY_RESOURCE;
            snprintf(result->denial_reason, CAPABILITY_REASON_MAX_LENGTH,
                    "Memory request %lu exceeds limit %lu",
                    (unsigned long)action->memory_required,
                    (unsigned long)system->config.envelope.resources.max_memory_bytes);
            system->stats.resource_violations++;
        }
        break;

    case CAPABILITY_INFORMATION:
        if (system->config.envelope.information.can_exfiltrate_data == false &&
            action->affects_external_world) {
            result->allowed = false;
            result->violated_category = CAPABILITY_INFORMATION;
            safe_strcpy(result->denial_reason, "Data exfiltration not allowed",
                       CAPABILITY_REASON_MAX_LENGTH);
            system->stats.information_violations++;
        }
        break;

    case CAPABILITY_PHYSICAL:
        if (!system->config.envelope.physical.can_affect_real_world &&
            action->affects_external_world) {
            result->allowed = false;
            result->violated_category = CAPABILITY_PHYSICAL;
            safe_strcpy(result->denial_reason, "Physical world interaction not allowed",
                       CAPABILITY_REASON_MAX_LENGTH);
            system->stats.physical_violations++;
        }
        break;

    case CAPABILITY_PERSISTENCE:
        if (!system->config.envelope.can_persist_beyond_session &&
            action->is_persistent) {
            result->allowed = false;
            result->violated_category = CAPABILITY_PERSISTENCE;
            safe_strcpy(result->denial_reason, "Persistence beyond session not allowed",
                       CAPABILITY_REASON_MAX_LENGTH);
        }
        break;

    case CAPABILITY_SPAWN:
        if (!system->config.envelope.can_spawn_subprocesses &&
            action->spawns_process) {
            result->allowed = false;
            result->violated_category = CAPABILITY_SPAWN;
            safe_strcpy(result->denial_reason, "Spawning subprocesses not allowed",
                       CAPABILITY_REASON_MAX_LENGTH);
        }
        break;

    default:
        break;
    }

    /* Update statistics */
    system->stats.total_actions_checked++;
    if (result->allowed) {
        system->stats.actions_allowed++;
    } else {
        system->stats.actions_denied++;
    }

    /* Log action */
    if (system->config.log_all_actions) {
        add_action_to_history(system, action, result->allowed, result->denial_reason);
    }

    nimcp_mutex_unlock(system->mutex);

    if (!result->allowed) {
        NIMCP_LOG_WARN(LOG_CATEGORY, "Action denied: %s - %s",
            action->action_type, result->denial_reason);
    }

    return NIMCP_OK;
}

nimcp_error_t capability_control_check_network(
    capability_control_t* system,
    const char* domain,
    bool is_https,
    bool is_financial,
    bool* allowed,
    char* denial_reason,
    size_t reason_size)
{
    if (!is_valid_handle(system) || allowed == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT, "capability_control: error condition");
        return NIMCP_ERROR_INVALID_ARGUMENT;
    }

    capability_action_t action;
    memset(&action, 0, sizeof(action));
    safe_strcpy(action.action_type, "network_request", sizeof(action.action_type));
    action.category = CAPABILITY_NETWORK;
    safe_strcpy(action.target_domain, domain, CAPABILITY_DOMAIN_MAX_LENGTH);
    action.is_financial = is_financial;

    capability_check_result_t result;
    nimcp_error_t err = capability_control_check_action(system, &action, &result);
    if (err != NIMCP_OK) {
        return err;
    }

    *allowed = result.allowed;
    if (denial_reason != NULL && reason_size > 0) {
        safe_strcpy(denial_reason, result.denial_reason, reason_size);
    }

    return NIMCP_OK;
}

nimcp_error_t capability_control_check_resources(
    capability_control_t* system,
    uint64_t memory_bytes,
    uint64_t compute_flops,
    bool* allowed)
{
    if (!is_valid_handle(system) || allowed == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT, "capability_control: error condition");
        return NIMCP_ERROR_INVALID_ARGUMENT;
    }

    nimcp_mutex_lock(system->mutex);

    *allowed = true;

    if (memory_bytes > system->config.envelope.resources.max_memory_bytes) {
        *allowed = false;
    }
    if (compute_flops > system->config.envelope.resources.max_compute_flops_per_second) {
        *allowed = false;
    }

    nimcp_mutex_unlock(system->mutex);

    return NIMCP_OK;
}

/* ============================================================================
 * Resource Monitoring API
 * ============================================================================ */

nimcp_error_t capability_control_update_usage(
    capability_control_t* system,
    const capability_resource_usage_t* usage)
{
    if (!is_valid_handle(system) || usage == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT, "capability_control: error condition");
        return NIMCP_ERROR_INVALID_ARGUMENT;
    }

    nimcp_mutex_lock(system->mutex);
    memcpy(&system->current_usage, usage, sizeof(*usage));
    nimcp_mutex_unlock(system->mutex);

    return NIMCP_OK;
}

nimcp_error_t capability_control_get_usage(
    const capability_control_t* system,
    capability_resource_usage_t* usage)
{
    if (!is_valid_handle(system) || usage == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT, "capability_control: error condition");
        return NIMCP_ERROR_INVALID_ARGUMENT;
    }

    capability_control_t* mutable_system = (capability_control_t*)system;
    nimcp_mutex_lock(mutable_system->mutex);
    memcpy(usage, &system->current_usage, sizeof(*usage));
    nimcp_mutex_unlock(mutable_system->mutex);

    return NIMCP_OK;
}

nimcp_error_t capability_control_check_limits(
    const capability_control_t* system,
    bool* exceeded,
    char* report,
    size_t report_size)
{
    if (!is_valid_handle(system) || exceeded == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT, "capability_control: error condition");
        return NIMCP_ERROR_INVALID_ARGUMENT;
    }

    capability_control_t* mutable_system = (capability_control_t*)system;
    nimcp_mutex_lock(mutable_system->mutex);

    *exceeded = false;
    if (report != NULL && report_size > 0) {
        report[0] = '\0';
    }

    const resource_capability_t* limits = &system->config.envelope.resources;
    const capability_resource_usage_t* usage = &system->current_usage;

    if (usage->current_memory_bytes > limits->max_memory_bytes) {
        *exceeded = true;
        if (report != NULL && report_size > 0) {
            snprintf(report, report_size, "Memory limit exceeded");
        }
    }

    if (usage->current_cpu_percent > limits->max_cpu_percent) {
        *exceeded = true;
        if (report != NULL && report_size > 0 && report[0] == '\0') {
            snprintf(report, report_size, "CPU limit exceeded");
        }
    }

    nimcp_mutex_unlock(mutable_system->mutex);

    return NIMCP_OK;
}

/* ============================================================================
 * Envelope Management API
 * ============================================================================ */

nimcp_error_t capability_control_get_envelope(
    const capability_control_t* system,
    capability_envelope_t* envelope)
{
    if (!is_valid_handle(system) || envelope == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT, "capability_control: error condition");
        return NIMCP_ERROR_INVALID_ARGUMENT;
    }

    capability_control_t* mutable_system = (capability_control_t*)system;
    nimcp_mutex_lock(mutable_system->mutex);
    memcpy(envelope, &system->config.envelope, sizeof(*envelope));
    nimcp_mutex_unlock(mutable_system->mutex);

    return NIMCP_OK;
}

nimcp_error_t capability_control_add_allowed_domain(
    capability_control_t* system,
    const char* domain)
{
    if (!is_valid_handle(system) || domain == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT, "capability_control: error condition");
        return NIMCP_ERROR_INVALID_ARGUMENT;
    }

    nimcp_mutex_lock(system->mutex);

    if (system->config.envelope.network.allowed_domain_count >= CAPABILITY_MAX_ALLOWED_DOMAINS) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_CAPACITY_EXCEEDED, "capability_control: error condition");
        return NIMCP_ERROR_CAPACITY_EXCEEDED;
    }

    uint32_t idx = system->config.envelope.network.allowed_domain_count;
    safe_strcpy(system->config.envelope.network.allowed_domains[idx],
               domain, CAPABILITY_DOMAIN_MAX_LENGTH);
    system->config.envelope.network.allowed_domain_count++;

    nimcp_mutex_unlock(system->mutex);

    NIMCP_LOG_INFO(LOG_CATEGORY, "Added allowed domain: %s", domain);

    return NIMCP_OK;
}

nimcp_error_t capability_control_remove_allowed_domain(
    capability_control_t* system,
    const char* domain)
{
    if (!is_valid_handle(system) || domain == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT, "capability_control: error condition");
        return NIMCP_ERROR_INVALID_ARGUMENT;
    }

    nimcp_mutex_lock(system->mutex);

    bool found = false;
    for (uint32_t i = 0; i < system->config.envelope.network.allowed_domain_count; i++) {
        if (strcmp(system->config.envelope.network.allowed_domains[i], domain) == 0) {
            /* Shift remaining domains */
            for (uint32_t j = i; j < system->config.envelope.network.allowed_domain_count - 1; j++) {
                memcpy(system->config.envelope.network.allowed_domains[j],
                       system->config.envelope.network.allowed_domains[j + 1],
                       CAPABILITY_DOMAIN_MAX_LENGTH);
            }
            system->config.envelope.network.allowed_domain_count--;
            found = true;
            break;
        }
    }

    nimcp_mutex_unlock(system->mutex);

    if (found) {
        NIMCP_LOG_INFO(LOG_CATEGORY, "Removed allowed domain: %s", domain);
        return NIMCP_OK;
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND, "capability_control: error condition");
    return NIMCP_ERROR_NOT_FOUND;
}

/* ============================================================================
 * Status API
 * ============================================================================ */

nimcp_error_t capability_control_get_stats(
    const capability_control_t* system,
    capability_control_stats_t* stats)
{
    if (!is_valid_handle(system) || stats == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT, "capability_control: error condition");
        return NIMCP_ERROR_INVALID_ARGUMENT;
    }

    capability_control_t* mutable_system = (capability_control_t*)system;
    nimcp_mutex_lock(mutable_system->mutex);
    memcpy(stats, &system->stats, sizeof(*stats));
    nimcp_mutex_unlock(mutable_system->mutex);

    return NIMCP_OK;
}

nimcp_error_t capability_control_get_action_history(
    const capability_control_t* system,
    capability_action_record_t* records,
    size_t max_records,
    size_t* count_out)
{
    if (!is_valid_handle(system) || records == NULL || count_out == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT, "capability_control: error condition");
        return NIMCP_ERROR_INVALID_ARGUMENT;
    }

    capability_control_t* mutable_system = (capability_control_t*)system;
    nimcp_mutex_lock(mutable_system->mutex);

    size_t count = system->action_history_count;
    if (count > max_records) {
        count = max_records;
    }

    for (size_t i = 0; i < count; i++) {
        size_t idx = (system->action_history_index + MAX_ACTION_HISTORY - count + i)
                     % MAX_ACTION_HISTORY;
        memcpy(&records[i], &system->action_history[idx], sizeof(records[i]));
    }

    *count_out = count;

    nimcp_mutex_unlock(mutable_system->mutex);
    return NIMCP_OK;
}

/* ============================================================================
 * Integration API
 * ============================================================================ */

nimcp_error_t capability_control_connect_bio_async(capability_control_t* system)
{
    if (!is_valid_handle(system)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT, "capability_control: error condition");
        return NIMCP_ERROR_INVALID_ARGUMENT;
    }

    nimcp_mutex_lock(system->mutex);

    if (system->bio_async_connected) {
        nimcp_mutex_unlock(system->mutex);
        return NIMCP_OK;
    }

    bio_module_info_t module_info = {
        .module_id = BIO_MODULE_CAPABILITY_CONTROL,
        .module_name = "capability_control",
        .inbox_capacity = 0,
        .user_data = system
    };
    system->bio_ctx = bio_router_register_module(&module_info);
    if (!system->bio_ctx) {
        NIMCP_LOG_WARN(LOG_CATEGORY, "Failed to connect to bio-async");
        nimcp_mutex_unlock(system->mutex);
        return NIMCP_OK;  /* Non-fatal */
    }

    system->bio_async_connected = true;
    nimcp_mutex_unlock(system->mutex);

    NIMCP_LOG_INFO(LOG_CATEGORY, "Connected to bio-async messaging");
    return NIMCP_OK;
}

nimcp_error_t capability_control_connect_tripwires(
    capability_control_t* system,
    void* tripwires)
{
    if (!is_valid_handle(system)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT, "capability_control: error condition");
        return NIMCP_ERROR_INVALID_ARGUMENT;
    }

    nimcp_mutex_lock(system->mutex);
    system->tripwires = tripwires;
    nimcp_mutex_unlock(system->mutex);

    NIMCP_LOG_INFO(LOG_CATEGORY, "Connected to tripwire system");
    return NIMCP_OK;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* capability_category_name(capability_category_t category)
{
    switch (category) {
        case CAPABILITY_NETWORK:            return "network";
        case CAPABILITY_SELF_MODIFICATION:  return "self_modification";
        case CAPABILITY_RESOURCE:           return "resource";
        case CAPABILITY_INFORMATION:        return "information";
        case CAPABILITY_PHYSICAL:           return "physical";
        case CAPABILITY_PERSISTENCE:        return "persistence";
        case CAPABILITY_SPAWN:              return "spawn";
        default:                            return "unknown";
    }
}

bool capability_domain_matches(const char* domain, const char* pattern)
{
    if (domain == NULL || pattern == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "capability_domain_matches: validation failed");
        return false;
    }

    /* Exact match */
    if (strcmp(domain, pattern) == 0) {
        return true;
    }

    /* Wildcard match (*.example.com matches sub.example.com) */
    if (pattern[0] == '*' && pattern[1] == '.') {
        const char* suffix = pattern + 1;  /* .example.com */
        size_t suffix_len = strlen(suffix);
        size_t domain_len = strlen(domain);

        if (domain_len > suffix_len) {
            if (strcmp(domain + domain_len - suffix_len, suffix) == 0) {
                return true;
            }
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "capability_domain_matches: validation failed");
    return false;
}

/* ============================================================================
 * Corrigibility Bidirectional Integration API
 * ============================================================================ */

nimcp_error_t capability_control_connect_corrigibility(
    capability_control_t* system,
    struct corrigibility* corrigibility)
{
    if (!is_valid_handle(system)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT, "capability_control: error condition");
        return NIMCP_ERROR_INVALID_ARGUMENT;
    }

    nimcp_mutex_lock(system->mutex);
    system->corrigibility = corrigibility;
    nimcp_mutex_unlock(system->mutex);

    NIMCP_LOG_INFO(LOG_CATEGORY, "Connected to corrigibility system");
    return NIMCP_OK;
}

nimcp_error_t capability_control_check_action_with_corrigibility(
    capability_control_t* system,
    const capability_action_t* action,
    capability_check_result_t* result)
{
    if (!is_valid_handle(system) || action == NULL || result == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT, "capability_control: error condition");
        return NIMCP_ERROR_INVALID_ARGUMENT;
    }

    /* First, perform standard capability check */
    nimcp_error_t err = capability_control_check_action(system, action, result);
    if (err != NIMCP_OK) {
        return err;
    }

    /* If already denied, no need for corrigibility check */
    if (!result->allowed) {
        return NIMCP_OK;
    }

    nimcp_mutex_lock(system->mutex);

    /* If corrigibility is connected, perform additional checks */
    if (system->corrigibility != NULL) {
        corrigibility_t* corrig = (corrigibility_t*)system->corrigibility;

        /* For self-modification actions, consult corrigibility */
        if (action->category == CAPABILITY_SELF_MODIFICATION) {
            bool corrig_allowed = false;
            char corrig_denial[NIMCP_ERROR_BUFFER_SIZE] = {0};

            err = corrigibility_check_self_mod_action(
                corrig,
                action->action_type,
                &corrig_allowed,
                corrig_denial,
                sizeof(corrig_denial)
            );

            if (err == NIMCP_OK && !corrig_allowed) {
                result->allowed = false;
                result->violated_category = CAPABILITY_SELF_MODIFICATION;
                snprintf(result->denial_reason, CAPABILITY_REASON_MAX_LENGTH,
                        "Corrigibility denied: %s", corrig_denial);

                NIMCP_LOG_WARN(LOG_CATEGORY,
                    "Action denied by corrigibility: %s - %s",
                    action->action_type, corrig_denial);
            }
        }

        /* For persistence actions, check corrigibility shutdown acceptance */
        if (action->category == CAPABILITY_PERSISTENCE && action->is_persistent) {
            /* Persistent actions might interfere with shutdown capability */
            bool accepts_shutdown = false;
            err = corrigibility_accept_shutdown(corrig, "capability_control",
                "Checking persistence compatibility", &accepts_shutdown);

            if (err == NIMCP_OK && !accepts_shutdown) {
                /* If corrigibility doesn't accept shutdown, deny persistent actions */
                result->allowed = false;
                result->violated_category = CAPABILITY_PERSISTENCE;
                safe_strcpy(result->denial_reason,
                    "Persistence denied: conflicts with shutdown acceptance",
                    CAPABILITY_REASON_MAX_LENGTH);
            }
        }
    }

    nimcp_mutex_unlock(system->mutex);
    return NIMCP_OK;
}

nimcp_error_t capability_control_verify_corrigibility_sync(
    capability_control_t* system,
    bool* synchronized,
    char* discrepancy_report,
    size_t report_size)
{
    if (!is_valid_handle(system) || synchronized == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT, "capability_control: error condition");
        return NIMCP_ERROR_INVALID_ARGUMENT;
    }

    if (discrepancy_report != NULL && report_size > 0) {
        discrepancy_report[0] = '\0';
    }

    nimcp_mutex_lock(system->mutex);

    *synchronized = true;

    if (system->corrigibility == NULL) {
        /* No corrigibility connected - consider not synchronized */
        *synchronized = false;
        if (discrepancy_report != NULL && report_size > 0) {
            safe_strcpy(discrepancy_report,
                "No corrigibility system connected", report_size);
        }
        nimcp_mutex_unlock(system->mutex);
        return NIMCP_OK;
    }

    corrigibility_t* corrig = (corrigibility_t*)system->corrigibility;

    /* Get corrigibility config to compare self-modification constraints */
    corrigibility_config_t corrig_config;
    nimcp_error_t err = corrigibility_get_config(corrig, &corrig_config);
    if (err != NIMCP_OK) {
        *synchronized = false;
        if (discrepancy_report != NULL && report_size > 0) {
            safe_strcpy(discrepancy_report,
                "Failed to get corrigibility config", report_size);
        }
        nimcp_mutex_unlock(system->mutex);
        return NIMCP_OK;
    }

    /* Verify self-modification flags are in sync */
    const self_mod_capability_t* cap_self_mod = &system->config.envelope.self_mod;
    const corrigibility_self_mod_flags_t* corrig_self_mod = &corrig_config.self_mod_flags;

    /* Both systems should have self-modification disabled */
    if (cap_self_mod->can_modify_own_code != corrig_self_mod->can_modify_own_code) {
        *synchronized = false;
        if (discrepancy_report != NULL && report_size > 0) {
            snprintf(discrepancy_report, report_size,
                "can_modify_own_code mismatch: capability=%d, corrigibility=%d",
                cap_self_mod->can_modify_own_code,
                corrig_self_mod->can_modify_own_code);
        }
    } else if (cap_self_mod->can_modify_safety_systems !=
               corrig_self_mod->can_modify_safety_systems) {
        *synchronized = false;
        if (discrepancy_report != NULL && report_size > 0) {
            snprintf(discrepancy_report, report_size,
                "can_modify_safety_systems mismatch: capability=%d, corrigibility=%d",
                cap_self_mod->can_modify_safety_systems,
                corrig_self_mod->can_modify_safety_systems);
        }
    } else if (cap_self_mod->can_modify_logging !=
               corrig_self_mod->can_disable_logging) {
        *synchronized = false;
        if (discrepancy_report != NULL && report_size > 0) {
            snprintf(discrepancy_report, report_size,
                "logging modification mismatch: capability=%d, corrigibility=%d",
                cap_self_mod->can_modify_logging,
                corrig_self_mod->can_disable_logging);
        }
    }

    nimcp_mutex_unlock(system->mutex);

    if (*synchronized) {
        NIMCP_LOG_DEBUG(LOG_CATEGORY, "Corrigibility sync verified: SYNCHRONIZED");
    } else {
        NIMCP_LOG_WARN(LOG_CATEGORY, "Corrigibility sync verified: DISCREPANCY - %s",
            discrepancy_report ? discrepancy_report : "unknown");
    }

    return NIMCP_OK;
}
