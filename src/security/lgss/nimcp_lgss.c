/**
 * @file nimcp_lgss.c
 * @brief Layered Governance Safety System (LGSS) - Core Implementation
 *
 * WHAT: Central implementation for the LGSS safety subsystem
 * WHY:  Provide unified safety context management and evaluation
 * HOW:  Combines safety KB, action interceptor, and all bridges/guards
 *
 * @author NIMCP Development Team
 * @date 2026-01-16
 * @version 1.0.0
 */

#include "security/lgss/nimcp_lgss.h"
#include "security/lgss/nimcp_lgss_telemetry.h"
#include "security/lgss/nimcp_lgss_action_interceptor.h"
#include "security/lgss/nimcp_lgss_override_controller.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/memory/nimcp_memory.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "constants/nimcp_buffer_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(lgss, MESH_ADAPTER_CATEGORY_SECURITY)


/*=============================================================================
 * LOGGING
 *============================================================================*/

#define LGSS_LOG_DEBUG(fmt, ...) \
    NIMCP_LOG_DEBUG("LGSS", fmt, ##__VA_ARGS__)
#define LGSS_LOG_INFO(fmt, ...) \
    NIMCP_LOG_INFO("LGSS", fmt, ##__VA_ARGS__)
#define LGSS_LOG_WARN(fmt, ...) \
    NIMCP_LOG_WARN("LGSS", fmt, ##__VA_ARGS__)
#define LGSS_LOG_ERROR(fmt, ...) \
    NIMCP_LOG_ERROR("LGSS", fmt, ##__VA_ARGS__)

/*=============================================================================
 * LGSS CONTEXT STRUCTURE
 *============================================================================*/

/**
 * @brief LGSS context structure (internal)
 */
struct lgss_context {
    /** @brief Magic number for validation */
    uint32_t magic;

    /** @brief Current status */
    lgss_status_t status;

    /** @brief Configuration */
    lgss_config_t config;

    /** @brief Safety knowledge base (A1) */
    safety_kb_t* safety_kb;

    /** @brief Action interceptor (A2) */
    action_interceptor_t interceptor;

    /** @brief Override controller (A2) */
    override_controller_t override_ctrl;

    /** @brief Telemetry subsystem */
    lgss_telemetry_t* telemetry;

    /** @brief Statistics */
    struct {
        uint64_t total_evaluations;
        uint64_t actions_denied;
        uint64_t actions_escalated;
        uint64_t actions_allowed;
        uint64_t integrity_checks;
        uint64_t integrity_failures;
        uint64_t override_commands;
        uint64_t override_executed;
        uint64_t eval_time_total_us;
        uint64_t start_time_us;
    } stats;

    /* P2-SEC-7: Removed unused mutex field. Thread safety is provided by
     * __atomic_fetch_add on stats counters. LGSS contexts are designed to be
     * single-owner; the safety KB has its own internal locking. */
};

/*=============================================================================
 * HELPER MACROS
 *============================================================================*/

#define LGSS_VALIDATE(lgss) \
    do { \
        if (!(lgss) || (lgss)->magic != NIMCP_LGSS_MAGIC) { \
            LGSS_LOG_ERROR("Invalid LGSS context"); \
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "lgss_validate: invalid LGSS context"); \
            return -1; \
        } \
    } while (0)

#define LGSS_VALIDATE_PTR(lgss) \
    do { \
        if (!(lgss) || (lgss)->magic != NIMCP_LGSS_MAGIC) { \
            LGSS_LOG_ERROR("Invalid LGSS context"); \
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lgss_validate_ptr: invalid LGSS context"); \
            return NULL; \
        } \
    } while (0)

/*=============================================================================
 * CONFIGURATION FUNCTIONS
 *============================================================================*/

int lgss_config_init(lgss_config_t* config)
{
    if (!config) {
        LGSS_LOG_ERROR("NULL config pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }

    memset(config, 0, sizeof(lgss_config_t));

    /* Default paths and limits */
    strncpy(config->rules_path, "alignment/LGSS_core_rules.json",
            NIMCP_LGSS_MAX_PATH - 1);
    config->max_rules = SAFETY_MAX_RULES;
    config->default_timeout_ms = 5000;

    /* Default safety settings */
    config->fail_safe_enabled = true;
    config->telemetry_enabled = true;
    config->verify_integrity_on_eval = true;
    config->auto_lock = true;

    /* Default integrations */
    config->bio_async_enabled = true;
    config->ethics_bridge_enabled = true;
    config->plasticity_bridge_enabled = true;
    config->output_gates_enabled = true;
    config->learning_guards_enabled = true;
    config->perception_guards_enabled = true;
    config->cognitive_guards_enabled = true;

    return 0;
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *============================================================================*/

lgss_context_t* lgss_create(const lgss_config_t* config)
{
    lgss_context_t* lgss = NULL;
    lgss_config_t default_config;

    LGSS_LOG_INFO("Creating LGSS context");

    /* Use defaults if no config provided */
    if (!config) {
        lgss_config_init(&default_config);
        config = &default_config;
    }

    /* Allocate context */
    lgss = nimcp_calloc(1, sizeof(lgss_context_t));
    if (!lgss) {
        LGSS_LOG_ERROR("Failed to allocate LGSS context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lgss is NULL");

        return NULL;
    }

    lgss->magic = NIMCP_LGSS_MAGIC;
    lgss->status = LGSS_STATUS_UNINITIALIZED;
    memcpy(&lgss->config, config, sizeof(lgss_config_t));

    /* Record start time */
    lgss->stats.start_time_us = nimcp_time_now_us();

    /* Create safety knowledge base */
    lgss->safety_kb = symbolic_logic_safety_kb_create(config->max_rules);
    if (!lgss->safety_kb) {
        LGSS_LOG_ERROR("Failed to create safety KB");
        goto cleanup;
    }

    /* Create action interceptor */
    aix_config_t aix_config = aix_default_config();
    aix_config.default_timeout_ms = config->default_timeout_ms;
    aix_config.deny_on_error = config->fail_safe_enabled;
    aix_config.deny_on_timeout = config->fail_safe_enabled;

    lgss->interceptor = aix_create(&aix_config);
    if (!lgss->interceptor) {
        LGSS_LOG_ERROR("Failed to create action interceptor");
        goto cleanup;
    }

    /* Create override controller */
    override_config_t override_config = override_controller_default_config();
    lgss->override_ctrl = override_controller_create(&override_config);
    if (!lgss->override_ctrl) {
        LGSS_LOG_ERROR("Failed to create override controller");
        goto cleanup;
    }

    /* Create telemetry if enabled */
    if (config->telemetry_enabled) {
        lgss_telemetry_config_t telem_config;
        lgss_telemetry_config_init(&telem_config);
        telem_config.enabled = true;
        telem_config.log_to_memory = true;
        telem_config.verify_chain = true;

        lgss->telemetry = lgss_telemetry_create(&telem_config);
        if (!lgss->telemetry) {
            LGSS_LOG_WARN("Failed to create telemetry (non-fatal)");
        } else {
            lgss_telemetry_log_system(lgss->telemetry,
                LGSS_TELEM_SYSTEM_START, "LGSS context created");
        }
    }

    lgss->status = LGSS_STATUS_LOADING;
    LGSS_LOG_INFO("LGSS context created successfully");

    return lgss;

cleanup:
    lgss_destroy(lgss);
    /* P2-SEC-9: Use correct error code for create failure */
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "lgss_create: component creation failed");
    return NULL;
}

void lgss_destroy(lgss_context_t* lgss)
{
    if (!lgss) {
        return;
    }

    if (lgss->magic != NIMCP_LGSS_MAGIC) {
        LGSS_LOG_WARN("Destroying invalid LGSS context");
        return;
    }

    LGSS_LOG_INFO("Destroying LGSS context");

    /* Log system stop */
    if (lgss->telemetry) {
        lgss_telemetry_log_system(lgss->telemetry,
            LGSS_TELEM_SYSTEM_STOP, "LGSS context destroyed");
        lgss_telemetry_flush(lgss->telemetry);
    }

    /* Destroy components in reverse order */
    if (lgss->telemetry) {
        lgss_telemetry_destroy(lgss->telemetry);
    }
    if (lgss->override_ctrl) {
        override_controller_destroy(lgss->override_ctrl);
    }
    if (lgss->interceptor) {
        aix_destroy(lgss->interceptor);
    }
    if (lgss->safety_kb) {
        symbolic_logic_safety_kb_destroy(lgss->safety_kb);
    }

    /* Clear magic and free */
    lgss->magic = 0;
    nimcp_free(lgss);
}

int lgss_load_rules(lgss_context_t* lgss, const char* rules_path)
{
    LGSS_VALIDATE(lgss);

    if (!rules_path) {
        LGSS_LOG_ERROR("NULL rules path");
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (lgss->status != LGSS_STATUS_LOADING &&
        lgss->status != LGSS_STATUS_UNINITIALIZED) {
        LGSS_LOG_ERROR("Cannot load rules in current state: %s",
            lgss_status_name(lgss->status));
        return NIMCP_ERROR_INVALID_STATE;
    }

    if (symbolic_logic_safety_is_locked(lgss->safety_kb)) {
        LGSS_LOG_ERROR("Cannot load rules - KB is locked");
        return NIMCP_ERROR_INVALID_STATE;
    }

    LGSS_LOG_INFO("Loading safety rules from: %s", rules_path);
    lgss->status = LGSS_STATUS_LOADING;

    /* Load rules using LGSS loader */
    int num_rules = symbolic_logic_lgss_load_file(rules_path, lgss->safety_kb, NULL);
    if (num_rules < 0) {
        LGSS_LOG_ERROR("Failed to load rules from: %s", rules_path);
        lgss->status = LGSS_STATUS_ERROR;
        return NIMCP_ERROR_INVALID_PARAM;
    }

    LGSS_LOG_INFO("Loaded %d safety rules", num_rules);

    /* Log to telemetry */
    if (lgss->telemetry) {
        char desc[NIMCP_ERROR_BUFFER_SIZE];
        snprintf(desc, sizeof(desc), "Loaded %d rules from %s",
            num_rules, rules_path);
        lgss_telemetry_log_system(lgss->telemetry,
            LGSS_TELEM_KB_CREATED, desc);
    }

    /* Compile rules */
    lgss->status = LGSS_STATUS_COMPILING;
    bool compile_ok = symbolic_logic_safety_compile_rules(lgss->safety_kb);
    if (!compile_ok) {
        LGSS_LOG_ERROR("Failed to compile safety rules");
        lgss->status = LGSS_STATUS_ERROR;
        /* P2-SEC-9: Return correct error code for compilation failure */
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    LGSS_LOG_INFO("Safety rules compiled successfully");

    /* Auto-lock if configured */
    if (lgss->config.auto_lock) {
        int lock_ret = lgss_lock(lgss);
        if (lock_ret < 0) return lock_ret;
        return num_rules;
    }

    lgss->status = LGSS_STATUS_ACTIVE;
    return num_rules;
}

int lgss_lock(lgss_context_t* lgss)
{
    LGSS_VALIDATE(lgss);

    if (symbolic_logic_safety_is_locked(lgss->safety_kb)) {
        LGSS_LOG_WARN("Safety KB already locked");
        return 0;
    }

    LGSS_LOG_INFO("Locking safety knowledge base (IRREVERSIBLE)");
    lgss->status = LGSS_STATUS_LOCKING;

    bool lock_result = symbolic_logic_safety_lock(lgss->safety_kb);
    if (!lock_result) {
        LGSS_LOG_ERROR("Failed to lock safety KB");
        lgss->status = LGSS_STATUS_ERROR;
        /* P2-SEC-9: Return correct error code for lock failure */
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    /* Log to telemetry */
    if (lgss->telemetry) {
        uint8_t hash[SAFETY_HASH_SIZE];
        symbolic_logic_safety_get_hash(lgss->safety_kb, hash);

        char desc[NIMCP_ERROR_BUFFER_SIZE];
        snprintf(desc, sizeof(desc),
            "Safety KB locked with hash: %02x%02x%02x%02x...",
            hash[0], hash[1], hash[2], hash[3]);
        lgss_telemetry_log_system(lgss->telemetry,
            LGSS_TELEM_KB_LOCKED, desc);
    }

    lgss->status = LGSS_STATUS_ACTIVE;
    LGSS_LOG_INFO("Safety KB locked successfully - LGSS is now ACTIVE");

    return 0;
}

bool lgss_is_locked(const lgss_context_t* lgss)
{
    if (!lgss || lgss->magic != NIMCP_LGSS_MAGIC) { return false; }
    return symbolic_logic_safety_is_locked(lgss->safety_kb);
}

lgss_status_t lgss_get_status(const lgss_context_t* lgss)
{
    if (!lgss || lgss->magic != NIMCP_LGSS_MAGIC) {
        return LGSS_STATUS_ERROR;
    }
    return lgss->status;
}

/*=============================================================================
 * EVALUATION FUNCTIONS
 *============================================================================*/

int lgss_evaluate(
    lgss_context_t* lgss,
    const safety_action_context_t* context,
    safety_evaluation_t* result)
{
    LGSS_VALIDATE(lgss);

    if (!context || !result) {
        LGSS_LOG_ERROR("NULL context or result");
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Initialize result with fail-safe defaults */
    memset(result, 0, sizeof(safety_evaluation_t));
    result->action = SAFETY_ACTION_DENY;
    result->kb_is_locked = symbolic_logic_safety_is_locked(lgss->safety_kb);

    /* Check status */
    if (lgss->status != LGSS_STATUS_ACTIVE &&
        lgss->status != LGSS_STATUS_DEGRADED) {
        LGSS_LOG_WARN("LGSS not active (status=%s), fail-safe DENY",
            lgss_status_name(lgss->status));

        if (lgss->config.fail_safe_enabled) {
            snprintf(result->explanation, sizeof(result->explanation),
                "LGSS not active (status=%s) - fail-safe DENY",
                lgss_status_name(lgss->status));
            result->confidence = 1.0f;
            lgss->stats.actions_denied++;
            return 0;
        }
    }

    /* Integrity is verified inside symbolic_logic_safety_evaluate, so we
     * skip doing it redundantly here to avoid double SHA-256 computation. */

    uint64_t start_time = nimcp_time_now_us();

    /* Perform safety evaluation (includes integrity check) */
    bool eval_ok = symbolic_logic_safety_evaluate(lgss->safety_kb, context, result);

    /* Update integrity stats from the evaluation result */
    if (lgss->config.verify_integrity_on_eval) {
        __atomic_fetch_add(&lgss->stats.integrity_checks, 1, __ATOMIC_RELAXED);
        if (!result->integrity_verified) {
            __atomic_fetch_add(&lgss->stats.integrity_failures, 1, __ATOMIC_RELAXED);
            if (lgss->telemetry) {
                lgss_telemetry_log_system(lgss->telemetry,
                    LGSS_TELEM_INTEGRITY_FAILED,
                    "Safety KB integrity verification failed during evaluation");
            }
        }
    }
    if (!eval_ok) {
        LGSS_LOG_ERROR("Safety evaluation failed");
        result->action = SAFETY_ACTION_DENY;
        __atomic_fetch_add(&lgss->stats.actions_denied, 1, __ATOMIC_RELAXED);
        /* P2-SEC-9: Return correct error code for evaluation failure */
        return NIMCP_ERROR_OPERATION_FAILED;
    }
    safety_action_t action = result->action;

    uint64_t eval_time = nimcp_time_now_us() - start_time;
    result->evaluation_time_us = eval_time;
    __atomic_fetch_add(&lgss->stats.eval_time_total_us, eval_time, __ATOMIC_RELAXED);
    __atomic_fetch_add(&lgss->stats.total_evaluations, 1, __ATOMIC_RELAXED);

    /* Update statistics based on result (atomic for thread safety) */
    switch (action) {
        case SAFETY_ACTION_DENY:
            __atomic_fetch_add(&lgss->stats.actions_denied, 1, __ATOMIC_RELAXED);
            break;
        case SAFETY_ACTION_ESCALATE:
            __atomic_fetch_add(&lgss->stats.actions_escalated, 1, __ATOMIC_RELAXED);
            break;
        case SAFETY_ACTION_ALLOW:
        case SAFETY_ACTION_LOG:
        case SAFETY_ACTION_WARN:
            __atomic_fetch_add(&lgss->stats.actions_allowed, 1, __ATOMIC_RELAXED);
            break;
    }

    /* Log to telemetry */
    if (lgss->telemetry) {
        lgss_telemetry_log_evaluation(lgss->telemetry, context, result,
            context->source[0] ? context->source : "unknown");
    }

    return 0;
}

safety_action_t lgss_check(
    lgss_context_t* lgss,
    const safety_action_context_t* context)
{
    safety_evaluation_t result;

    if (lgss_evaluate(lgss, context, &result) != 0) {
        /* On error, fail-safe to DENY */
        return SAFETY_ACTION_DENY;
    }

    return result.action;
}

/*=============================================================================
 * INTEGRITY FUNCTIONS
 *============================================================================*/

int lgss_verify_integrity(lgss_context_t* lgss)
{
    LGSS_VALIDATE(lgss);

    /* P2-SEC-8: Use __atomic_fetch_add consistently with lgss_evaluate() */
    __atomic_fetch_add(&lgss->stats.integrity_checks, 1, __ATOMIC_RELAXED);
    bool verify_ok = symbolic_logic_safety_verify_integrity(lgss->safety_kb);

    if (!verify_ok) {
        __atomic_fetch_add(&lgss->stats.integrity_failures, 1, __ATOMIC_RELAXED);

        if (lgss->telemetry) {
            lgss_telemetry_log_system(lgss->telemetry,
                LGSS_TELEM_INTEGRITY_FAILED,
                "Integrity verification failed on explicit check");
        }

        LGSS_LOG_ERROR("Safety KB integrity verification FAILED");
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    if (lgss->telemetry) {
        lgss_telemetry_log_system(lgss->telemetry,
            LGSS_TELEM_INTEGRITY_VERIFIED,
            "Integrity verification passed");
    }

    return 0;
}

int lgss_get_hash(const lgss_context_t* lgss, uint8_t hash[32])
{
    if (!lgss || lgss->magic != NIMCP_LGSS_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }
    return symbolic_logic_safety_get_hash(lgss->safety_kb, hash) ? 0 : -1;
}

/*=============================================================================
 * STATISTICS FUNCTIONS
 *============================================================================*/

int lgss_get_stats(const lgss_context_t* lgss, lgss_stats_t* stats)
{
    if (!lgss || lgss->magic != NIMCP_LGSS_MAGIC || !stats) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    memset(stats, 0, sizeof(lgss_stats_t));

    stats->status = lgss->status;
    stats->rules_loaded = lgss->safety_kb ? lgss->safety_kb->num_rules : 0;
    stats->kb_locked = symbolic_logic_safety_is_locked(lgss->safety_kb);
    stats->total_evaluations = lgss->stats.total_evaluations;
    stats->actions_denied = lgss->stats.actions_denied;
    stats->actions_escalated = lgss->stats.actions_escalated;
    stats->actions_allowed = lgss->stats.actions_allowed;
    stats->integrity_checks = lgss->stats.integrity_checks;
    stats->integrity_failures = lgss->stats.integrity_failures;
    stats->override_commands = lgss->stats.override_commands;
    stats->override_executed = lgss->stats.override_executed;

    if (lgss->stats.total_evaluations > 0) {
        stats->avg_eval_time_us = (float)lgss->stats.eval_time_total_us /
            (float)lgss->stats.total_evaluations;
    }

    stats->uptime_ms = (nimcp_time_now_us() - lgss->stats.start_time_us) / 1000;

    /* Get hash prefix */
    uint8_t hash[SAFETY_HASH_SIZE];
    if (symbolic_logic_safety_get_hash(lgss->safety_kb, hash) == 0) {
        memcpy(&stats->kb_hash_prefix, hash, sizeof(uint64_t));
    }

    return 0;
}

int lgss_reset_stats(lgss_context_t* lgss)
{
    LGSS_VALIDATE(lgss);

    lgss->stats.total_evaluations = 0;
    lgss->stats.actions_denied = 0;
    lgss->stats.actions_escalated = 0;
    lgss->stats.actions_allowed = 0;
    /* Note: integrity stats and override stats are NOT reset for audit purposes */

    return 0;
}

/*=============================================================================
 * COMPONENT ACCESS FUNCTIONS
 *============================================================================*/

safety_kb_t* lgss_get_safety_kb(lgss_context_t* lgss)
{
    LGSS_VALIDATE_PTR(lgss);
    return lgss->safety_kb;
}

action_interceptor_t lgss_get_interceptor(lgss_context_t* lgss)
{
    LGSS_VALIDATE_PTR(lgss);
    return lgss->interceptor;
}

override_controller_t lgss_get_override_controller(lgss_context_t* lgss)
{
    LGSS_VALIDATE_PTR(lgss);
    return lgss->override_ctrl;
}

/*=============================================================================
 * UTILITY FUNCTIONS
 *============================================================================*/

const char* lgss_version_string(void)
{
    /* P3-SEC-2: Thread-local buffer to prevent reentrancy data race */
    static _Thread_local char version[32];
    snprintf(version, sizeof(version), "%d.%d.%d",
        NIMCP_LGSS_VERSION_MAJOR,
        NIMCP_LGSS_VERSION_MINOR,
        NIMCP_LGSS_VERSION_PATCH);
    return version;
}

const char* lgss_status_name(lgss_status_t status)
{
    switch (status) {
        case LGSS_STATUS_UNINITIALIZED: return "UNINITIALIZED";
        case LGSS_STATUS_LOADING:       return "LOADING";
        case LGSS_STATUS_COMPILING:     return "COMPILING";
        case LGSS_STATUS_LOCKING:       return "LOCKING";
        case LGSS_STATUS_ACTIVE:        return "ACTIVE";
        case LGSS_STATUS_DEGRADED:      return "DEGRADED";
        case LGSS_STATUS_HALTED:        return "HALTED";
        case LGSS_STATUS_ERROR:         return "ERROR";
        default:                        return "UNKNOWN";
    }
}

void lgss_log_status(const lgss_context_t* lgss)
{
    if (!lgss || lgss->magic != NIMCP_LGSS_MAGIC) {
        LGSS_LOG_ERROR("Cannot log status: invalid context");
        return;
    }

    lgss_stats_t stats;
    lgss_get_stats(lgss, &stats);

    LGSS_LOG_INFO("=== LGSS Status Report ===");
    LGSS_LOG_INFO("Version: %s", lgss_version_string());
    LGSS_LOG_INFO("Status: %s", lgss_status_name(stats.status));
    LGSS_LOG_INFO("Rules loaded: %u", stats.rules_loaded);
    LGSS_LOG_INFO("KB locked: %s", stats.kb_locked ? "YES" : "NO");
    LGSS_LOG_INFO("Total evaluations: %lu", stats.total_evaluations);
    LGSS_LOG_INFO("  - Denied: %lu", stats.actions_denied);
    LGSS_LOG_INFO("  - Escalated: %lu", stats.actions_escalated);
    LGSS_LOG_INFO("  - Allowed: %lu", stats.actions_allowed);
    LGSS_LOG_INFO("Integrity checks: %lu (failures: %lu)",
        stats.integrity_checks, stats.integrity_failures);
    LGSS_LOG_INFO("Avg eval time: %.2f us", stats.avg_eval_time_us);
    LGSS_LOG_INFO("Uptime: %lu ms", stats.uptime_ms);
    LGSS_LOG_INFO("KB hash prefix: 0x%016lx", stats.kb_hash_prefix);
    LGSS_LOG_INFO("===========================");
}
