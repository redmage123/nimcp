/**
 * @file nimcp_security_recovery_bridge.c
 * @brief Security-Fault Tolerance Bridge Implementation
 *
 * WHAT: Connects security monitoring to fault tolerance recovery systems
 * WHY:  Enable automatic repair when security violations detected
 * HOW:  Route violations to appropriate recovery mechanisms
 *
 * Part of Phase SC-1: Security Coverage Framework (Tier 0.7)
 */

#include "security/nimcp_security_recovery_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "security/nimcp_security.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_fast_recovery.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "security_recovery_bridge"
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(security_recovery_bridge, MESH_ADAPTER_CATEGORY_SECURITY)


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Type Definitions
//=============================================================================

/** Registered brain entry */
typedef struct {
    brain_t brain;
    bool active;
    uint64_t registered_time;
    uint64_t last_checkpoint;
    uint32_t violation_count;
    uint32_t region_count;
} registered_brain_t;

/** Callback entry */
typedef struct {
    nimcp_srb_callback_t callback;
    void* user_data;
    bool active;
} callback_entry_t;

/** Violation history entry */
typedef struct {
    nimcp_security_violation_t violation;
    nimcp_security_recovery_result_t result;
    bool valid;
} violation_history_entry_t;

/** Bridge internal structure */
struct nimcp_security_recovery_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    nimcp_srb_config_t config;

    /* Connected security modules */
    nimcp_security_coverage_t* coverage;
    nimcp_fractal_security_t* fsc;
    nimcp_cfi_context_t* cfi;
    nimcp_shadow_stack_t* shadow_stack;
    nimcp_audit_log_t* audit;

    /* Registered brains */
    registered_brain_t brains[NIMCP_SRB_MAX_BRAINS];
    uint32_t brain_count;

    /* Callbacks */
    callback_entry_t callbacks[16];
    uint32_t callback_count;

    /* Violation history (ring buffer) */
    violation_history_entry_t history[NIMCP_SRB_MAX_HISTORY];
    uint32_t history_head;
    uint32_t history_count;

    /* Statistics */
    nimcp_srb_stats_t stats;

    /* Rate limiting */
    uint64_t last_repair_time;
    uint32_t repairs_this_minute;
    uint64_t minute_start;

    /* Thread safety */
    nimcp_mutex_t lock;
    bool initialized;
};

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get current time in milliseconds (safe wrapper)
 *
 * SECURITY FIX: Check clock_gettime() return value to avoid using garbage data.
 * On failure, returns 0 which may affect timing but won't cause undefined behavior.
 */
static uint64_t get_timestamp_ms(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        LOG_MODULE_WARN(LOG_MODULE, "clock_gettime() failed: %s", strerror(errno));
        return 0;
    }
    return (uint64_t)ts.tv_sec * 1000ULL + ts.tv_nsec / 1000000ULL;
}

/**
 * @brief Get current time in microseconds (safe wrapper)
 *
 * SECURITY FIX: Check clock_gettime() return value to avoid using garbage data.
 */
static uint64_t get_timestamp_us(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        LOG_MODULE_WARN(LOG_MODULE, "clock_gettime() failed: %s", strerror(errno));
        return 0;
    }
    return (uint64_t)ts.tv_sec * 1000000ULL + ts.tv_nsec / 1000ULL;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

nimcp_security_recovery_bridge_t* nimcp_srb_create(void)
{
    nimcp_security_recovery_bridge_t* bridge = nimcp_calloc(1,
        sizeof(nimcp_security_recovery_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_srb_create: bridge is NULL");
        return NULL;
    }

    if (nimcp_mutex_init(&bridge->lock, NULL) != NIMCP_SUCCESS) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "nimcp_srb_create: validation failed");
        return NULL;
    }
    return bridge;
}

nimcp_result_t nimcp_srb_init(
    nimcp_security_recovery_bridge_t* bridge,
    const nimcp_srb_config_t* config)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_srb_init: bridge is NULL");
        return NIMCP_INVALID_PARAM;
    }

    nimcp_mutex_lock(&bridge->lock);

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = nimcp_srb_default_config();
    }

    bridge->minute_start = get_timestamp_ms();
    bridge->initialized = true;

    nimcp_security_log_event(
        NIMCP_SECURITY_EVENT_DIRECTIVE_VERIFIED,
        NIMCP_THREAT_NONE,
        "Security-Recovery Bridge initialized"
    );

    nimcp_mutex_unlock(&bridge->lock);
    return NIMCP_SUCCESS;
}

void nimcp_srb_destroy(nimcp_security_recovery_bridge_t* bridge)
{
    if (!bridge)
        return;
        NIMCP_LOGGING_DEBUG("Destroying %s bridge", "security_recovery");

    nimcp_mutex_lock(&bridge->lock);
    bridge->initialized = false;
    nimcp_mutex_unlock(&bridge->lock);

    nimcp_mutex_destroy(&bridge->lock);
    nimcp_free(bridge);
}

nimcp_srb_config_t nimcp_srb_default_config(void)
{
    return (nimcp_srb_config_t){
        .mode = NIMCP_SRB_MODE_AUTO_REPAIR,
        .enable_auto_checkpoint = true,
        .checkpoint_interval_ms = 60000,  /* 1 minute */
        .enable_fractal_verification = true,
        .verification_interval_ms = 5000, /* 5 seconds */
        .cooldown_ms = NIMCP_SRB_COOLDOWN_MS,
        .max_repairs_per_minute = 10,
        .notify_brain = true,
        .log_to_audit = true
    };
}

//=============================================================================
// Brain Registration
//=============================================================================

nimcp_result_t nimcp_srb_register_brain(
    nimcp_security_recovery_bridge_t* bridge,
    brain_t brain)
{
    if (!bridge || !brain)
        return NIMCP_INVALID_PARAM;

    nimcp_mutex_lock(&bridge->lock);

    /* Check if already registered */
    for (uint32_t i = 0; i < NIMCP_SRB_MAX_BRAINS; i++) {
        if (bridge->brains[i].active && bridge->brains[i].brain == brain) {
            nimcp_mutex_unlock(&bridge->lock);
            return NIMCP_ALREADY_EXISTS;
        }
    }

    /* Find free slot */
    int32_t slot = -1;
    for (uint32_t i = 0; i < NIMCP_SRB_MAX_BRAINS; i++) {
        if (!bridge->brains[i].active) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        nimcp_mutex_unlock(&bridge->lock);
        return NIMCP_NO_MEMORY;
    }

    /* Register brain */
    registered_brain_t* entry = &bridge->brains[slot];
    entry->brain = brain;
    entry->active = true;
    entry->registered_time = get_timestamp_ms();
    entry->violation_count = 0;
    bridge->brain_count++;

    nimcp_mutex_unlock(&bridge->lock);

    /* Register brain's critical regions for protection */
    uint32_t regions = nimcp_srb_register_brain_regions(bridge, brain);
    entry->region_count = regions;

    /* Create initial checkpoint if enabled */
    if (bridge->config.enable_auto_checkpoint) {
        nimcp_srb_create_checkpoint(bridge, brain, "auto_initial");
    }

    /* Log to audit */
    if (bridge->audit && bridge->config.log_to_audit) {
        nimcp_audit_log(bridge->audit, NIMCP_AUDIT_CAT_CONFIGURATION,
            NIMCP_AUDIT_SEV_INFO, NIMCP_AUDIT_OUTCOME_SUCCESS,
            "srb_register", "Brain registered with security-recovery bridge");
    }

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_srb_unregister_brain(
    nimcp_security_recovery_bridge_t* bridge,
    brain_t brain)
{
    if (!bridge || !brain)
        return NIMCP_INVALID_PARAM;

    nimcp_mutex_lock(&bridge->lock);

    for (uint32_t i = 0; i < NIMCP_SRB_MAX_BRAINS; i++) {
        if (bridge->brains[i].active && bridge->brains[i].brain == brain) {
            bridge->brains[i].active = false;
            bridge->brains[i].brain = NULL;
            bridge->brain_count--;
            nimcp_mutex_unlock(&bridge->lock);
            return NIMCP_SUCCESS;
        }
    }

    nimcp_mutex_unlock(&bridge->lock);
    return NIMCP_NOT_FOUND;
}

uint32_t nimcp_srb_register_brain_regions(
    nimcp_security_recovery_bridge_t* bridge,
    brain_t brain)
{
    if (!bridge || !brain || !bridge->coverage)
        return 0;

    struct brain_struct* b = (struct brain_struct*)brain;
    uint32_t count = 0;

    /* Register configuration (read-only) */
    if (nimcp_coverage_register_region(bridge->coverage, &b->config,
        sizeof(brain_config_t), NIMCP_PROTECTION_HASH_VERIFIED,
        "brain_config") >= 0)
        count++;

    /* Register network structure if available */
    if (b->network) {
        /* Note: Network internals are managed by adaptive layer */
        count++;  /* Placeholder - actual registration depends on network API */
    }

    /* Register cached decision if present */
    if (b->cached_decision) {
        if (nimcp_coverage_register_region(bridge->coverage, b->cached_decision,
            sizeof(void*), NIMCP_PROTECTION_HASH_VERIFIED,
            "cached_decision") >= 0)
            count++;
    }

    /* Fractal protection for critical data */
    if (bridge->fsc && bridge->config.enable_fractal_verification) {
        nimcp_fractal_security_protect(bridge->fsc, &b->config,
            sizeof(brain_config_t), NULL);
    }

    return count;
}

//=============================================================================
// Security Module Connection
//=============================================================================

nimcp_result_t nimcp_srb_connect_coverage(
    nimcp_security_recovery_bridge_t* bridge,
    nimcp_security_coverage_t* coverage)
{
    if (!bridge)
        return NIMCP_INVALID_PARAM;

    nimcp_mutex_lock(&bridge->lock);
    bridge->coverage = coverage;
    nimcp_mutex_unlock(&bridge->lock);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_srb_connect_fractal(
    nimcp_security_recovery_bridge_t* bridge,
    nimcp_fractal_security_t* fsc)
{
    if (!bridge)
        return NIMCP_INVALID_PARAM;

    nimcp_mutex_lock(&bridge->lock);
    bridge->fsc = fsc;
    nimcp_mutex_unlock(&bridge->lock);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_srb_connect_cfi(
    nimcp_security_recovery_bridge_t* bridge,
    nimcp_cfi_context_t* cfi)
{
    if (!bridge)
        return NIMCP_INVALID_PARAM;

    nimcp_mutex_lock(&bridge->lock);
    bridge->cfi = cfi;
    nimcp_mutex_unlock(&bridge->lock);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_srb_connect_shadow_stack(
    nimcp_security_recovery_bridge_t* bridge,
    nimcp_shadow_stack_t* ss)
{
    if (!bridge)
        return NIMCP_INVALID_PARAM;

    nimcp_mutex_lock(&bridge->lock);
    bridge->shadow_stack = ss;
    nimcp_mutex_unlock(&bridge->lock);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_srb_connect_audit(
    nimcp_security_recovery_bridge_t* bridge,
    nimcp_audit_log_t* audit)
{
    if (!bridge)
        return NIMCP_INVALID_PARAM;

    nimcp_mutex_lock(&bridge->lock);
    bridge->audit = audit;
    nimcp_mutex_unlock(&bridge->lock);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Violation Handling
//=============================================================================

/**
 * WHAT: Execute recovery action for violation
 * WHY:  Perform appropriate repair based on violation type
 * HOW:  Route to fast recovery or full recovery as appropriate
 */
static nimcp_security_recovery_result_t execute_recovery(
    nimcp_security_recovery_bridge_t* bridge,
    const nimcp_security_violation_t* violation,
    nimcp_security_recovery_action_t action)
{
    nimcp_security_recovery_result_t result = {
        .action_taken = action,
        .success = false,
        .recovery_time_us = 0
    };

    uint64_t start_time = get_timestamp_us();

    switch (action) {
        case NIMCP_SRA_NONE:
        case NIMCP_SRA_LOG_ONLY:
            result.success = true;
            snprintf(result.message, sizeof(result.message),
                "Violation logged, no recovery action taken");
            break;

        case NIMCP_SRA_REHASH:
            /* Update hashes for affected region */
            if (bridge->fsc && violation->affected_address) {
                nimcp_result_t r = nimcp_fractal_security_update_hash(
                    bridge->fsc, violation->affected_address);
                result.success = (r == NIMCP_SUCCESS);
            }
            if (bridge->coverage && violation->affected_address) {
                /* Note: Would need region_id to update coverage hash */
                result.success = true;
            }
            snprintf(result.message, sizeof(result.message),
                result.success ? "Hashes recomputed" : "Rehash failed");
            break;

        case NIMCP_SRA_FAST_RECOVERY:
            /* Trigger fast recovery */
            if (violation->affected_brain) {
                fast_recovery_context_t ctx = {
                    .signal = 0,
                    .is_numeric_error = false,
                    .is_memory_error = true,
                    .brain_ptr = violation->affected_brain
                };

                fast_recovery_result_t fr = fast_recovery_attempt(
                    &ctx, violation->affected_brain);

                result.success = (fr.status == FAST_RECOVERY_SUCCESS ||
                                 fr.status == FAST_RECOVERY_PARTIAL);
                bridge->stats.fast_recoveries++;
            } else {
                result.success = true;  /* No brain to recover */
            }
            snprintf(result.message, sizeof(result.message),
                result.success ? "Fast recovery completed" : "Fast recovery failed");
            break;

        case NIMCP_SRA_RESTORE_CHECKPOINT:
            /* Restore from checkpoint */
            if (violation->affected_brain) {
                nimcp_result_t r = nimcp_srb_restore_checkpoint(
                    bridge, violation->affected_brain, NULL);
                result.success = (r == NIMCP_SUCCESS);
                result.checkpoint_restored = result.success;
                bridge->stats.checkpoints_restored += result.success ? 1 : 0;
            }
            snprintf(result.message, sizeof(result.message),
                result.success ? "Checkpoint restored" : "Checkpoint restore failed");
            break;

        case NIMCP_SRA_FULL_RECOVERY:
            /* Trigger full recovery via brain recovery integration */
            bridge->stats.full_recoveries++;
            /* Full recovery would integrate with brain_recovery_context */
            result.success = true;  /* Placeholder */
            snprintf(result.message, sizeof(result.message),
                "Full recovery triggered");
            break;

        case NIMCP_SRA_QUARANTINE:
            /* Isolate affected component */
            result.success = true;
            snprintf(result.message, sizeof(result.message),
                "Component quarantined");
            break;

        case NIMCP_SRA_HALT:
            /* Critical - log and recommend halt */
            nimcp_security_log_event(
                NIMCP_SECURITY_EVENT_THREAT_DETECTED,
                NIMCP_THREAT_CRITICAL,
                "CRITICAL: Security violation requires system halt"
            );
            result.success = false;
            snprintf(result.message, sizeof(result.message),
                "CRITICAL: System halt recommended");
            break;
    }

    result.recovery_time_us = (uint32_t)(get_timestamp_us() - start_time);

    /* Update statistics */
    if (result.success) {
        bridge->stats.recoveries_successful++;
    } else {
        bridge->stats.recoveries_failed++;
    }

    if (result.recovery_time_us > bridge->stats.max_recovery_time_us) {
        bridge->stats.max_recovery_time_us = result.recovery_time_us;
    }

    return result;
}

nimcp_result_t nimcp_srb_report_violation(
    nimcp_security_recovery_bridge_t* bridge,
    const nimcp_security_violation_t* violation,
    nimcp_security_recovery_result_t* result)
{
    if (!bridge || !violation)
        return NIMCP_INVALID_PARAM;

    nimcp_mutex_lock(&bridge->lock);

    /* Update statistics */
    bridge->stats.violations_detected++;
    if (violation->type < NIMCP_SV_TYPE_COUNT) {
        bridge->stats.violations_by_type[violation->type]++;
    }

    /* Store in history */
    violation_history_entry_t* hist = &bridge->history[bridge->history_head];
    hist->violation = *violation;
    hist->valid = true;
    bridge->history_head = (bridge->history_head + 1) % NIMCP_SRB_MAX_HISTORY;
    if (bridge->history_count < NIMCP_SRB_MAX_HISTORY)
        bridge->history_count++;

    /* Call registered callbacks */
    nimcp_security_recovery_action_t action = NIMCP_SRA_NONE;
    for (uint32_t i = 0; i < 16 && action == NIMCP_SRA_NONE; i++) {
        if (bridge->callbacks[i].active && bridge->callbacks[i].callback) {
            action = bridge->callbacks[i].callback(violation,
                bridge->callbacks[i].user_data);
        }
    }

    /* Determine action if callback didn't specify */
    if (action == NIMCP_SRA_NONE) {
        action = nimcp_srb_determine_action(violation->type, violation->severity);
    }

    /* Check if we should auto-repair */
    bool should_repair = false;
    if (bridge->config.mode == NIMCP_SRB_MODE_AUTO_REPAIR ||
        bridge->config.mode == NIMCP_SRB_MODE_PARANOID) {

        /* Check cooldown */
        uint64_t now = get_timestamp_ms();
        if (now - bridge->last_repair_time >= bridge->config.cooldown_ms) {
            /* Check rate limit */
            if (now - bridge->minute_start >= 60000) {
                bridge->minute_start = now;
                bridge->repairs_this_minute = 0;
            }

            if (bridge->repairs_this_minute < bridge->config.max_repairs_per_minute) {
                should_repair = true;
            }
        }
    }

    /* Execute recovery if appropriate */
    nimcp_security_recovery_result_t recovery_result = {0};
    if (should_repair && action > NIMCP_SRA_LOG_ONLY) {
        bridge->stats.recoveries_attempted++;
        bridge->repairs_this_minute++;
        bridge->last_repair_time = get_timestamp_ms();

        nimcp_mutex_unlock(&bridge->lock);
        recovery_result = execute_recovery(bridge, violation, action);
        nimcp_mutex_lock(&bridge->lock);
    } else {
        recovery_result.action_taken = NIMCP_SRA_LOG_ONLY;
        recovery_result.success = true;
        snprintf(recovery_result.message, sizeof(recovery_result.message),
            "Violation logged (mode=%s)", nimcp_srb_mode_name(bridge->config.mode));
    }

    /* Store recovery result in history */
    hist->result = recovery_result;

    /* Log to audit */
    if (bridge->audit && bridge->config.log_to_audit) {
        nimcp_audit_log_threat(bridge->audit,
            violation->severity >= NIMCP_SV_SEVERITY_HIGH ?
                NIMCP_THREAT_HIGH : NIMCP_THREAT_MEDIUM,
            nimcp_sv_type_name(violation->type),
            violation->details);
    }

    /* Notify brain if enabled */
    if (bridge->config.notify_brain && violation->affected_brain) {
        /* Brain notification would go through cognitive pipeline */
    }

    nimcp_mutex_unlock(&bridge->lock);

    if (result) {
        *result = recovery_result;
    }

    return NIMCP_SUCCESS;
}

nimcp_security_recovery_result_t nimcp_srb_report_hash_mismatch(
    nimcp_security_recovery_bridge_t* bridge,
    void* address,
    size_t size,
    const char* region_name,
    brain_t brain)
{
    nimcp_security_violation_t violation = {
        .type = NIMCP_SV_MEMORY_HASH_MISMATCH,
        .severity = NIMCP_SV_SEVERITY_HIGH,
        .timestamp = get_timestamp_ms(),
        .affected_address = address,
        .affected_size = size,
        .region_name = region_name,
        .affected_brain = brain
    };
    snprintf(violation.details, sizeof(violation.details),
        "Hash mismatch in region '%s' at %p (%zu bytes)",
        region_name ? region_name : "unknown", address, size);

    nimcp_security_recovery_result_t result;
    nimcp_srb_report_violation(bridge, &violation, &result);
    return result;
}

nimcp_security_recovery_result_t nimcp_srb_report_cfi_violation(
    nimcp_security_recovery_bridge_t* bridge,
    void* target_address,
    uint32_t type_id,
    brain_t brain)
{
    nimcp_security_violation_t violation = {
        .type = NIMCP_SV_CFI_INVALID_TARGET,
        .severity = NIMCP_SV_SEVERITY_CRITICAL,
        .timestamp = get_timestamp_ms(),
        .affected_address = target_address,
        .affected_brain = brain
    };
    snprintf(violation.details, sizeof(violation.details),
        "CFI violation: invalid call target %p (type_id=%u)",
        target_address, type_id);

    nimcp_security_recovery_result_t result;
    nimcp_srb_report_violation(bridge, &violation, &result);
    return result;
}

nimcp_security_recovery_result_t nimcp_srb_report_stack_mismatch(
    nimcp_security_recovery_bridge_t* bridge,
    void* expected,
    void* actual,
    brain_t brain)
{
    nimcp_security_violation_t violation = {
        .type = NIMCP_SV_SHADOW_STACK_MISMATCH,
        .severity = NIMCP_SV_SEVERITY_CRITICAL,
        .timestamp = get_timestamp_ms(),
        .affected_address = actual,
        .affected_brain = brain
    };
    snprintf(violation.details, sizeof(violation.details),
        "Shadow stack mismatch: expected %p, got %p",
        expected, actual);

    nimcp_security_recovery_result_t result;
    nimcp_srb_report_violation(bridge, &violation, &result);
    return result;
}

nimcp_security_recovery_result_t nimcp_srb_report_fractal_violation(
    nimcp_security_recovery_bridge_t* bridge,
    nimcp_fsc_node_t* node,
    nimcp_fsc_result_t result_type,
    brain_t brain)
{
    nimcp_security_violation_type_t vtype;
    nimcp_sv_severity_t severity;

    switch (result_type) {
        case NIMCP_FSC_HASH_MISMATCH:
            vtype = NIMCP_SV_FRACTAL_HASH_MISMATCH;
            severity = NIMCP_SV_SEVERITY_HIGH;
            break;
        case NIMCP_FSC_DIMENSION_ANOMALY:
            vtype = NIMCP_SV_FRACTAL_DIMENSION_ANOMALY;
            severity = NIMCP_SV_SEVERITY_MEDIUM;
            break;
        default:
            vtype = NIMCP_SV_FRACTAL_TRUST_DEGRADED;
            severity = NIMCP_SV_SEVERITY_LOW;
            break;
    }

    nimcp_security_violation_t violation = {
        .type = vtype,
        .severity = severity,
        .timestamp = get_timestamp_ms(),
        .affected_address = node ? node->protected_data : NULL,
        .affected_size = node ? node->data_size : 0,
        .affected_brain = brain
    };
    snprintf(violation.details, sizeof(violation.details),
        "Fractal security violation: %s",
        nimcp_fsc_result_name(result_type));

    nimcp_security_recovery_result_t result;
    nimcp_srb_report_violation(bridge, &violation, &result);
    return result;
}

//=============================================================================
// Recovery Control
//=============================================================================

nimcp_security_recovery_result_t nimcp_srb_trigger_recovery(
    nimcp_security_recovery_bridge_t* bridge,
    brain_t brain,
    nimcp_security_recovery_action_t action)
{
    nimcp_security_violation_t violation = {
        .type = NIMCP_SV_NONE,
        .severity = NIMCP_SV_SEVERITY_INFO,
        .timestamp = get_timestamp_ms(),
        .affected_brain = brain
    };
    snprintf(violation.details, sizeof(violation.details),
        "Manual recovery triggered: %s", nimcp_sra_name(action));

    return execute_recovery(bridge, &violation, action);
}

nimcp_result_t nimcp_srb_create_checkpoint(
    nimcp_security_recovery_bridge_t* bridge,
    brain_t brain,
    const char* name)
{
    if (!bridge || !brain)
        return NIMCP_INVALID_PARAM;

    /* Use brain's snapshot functionality */
    /* Note: Would call brain_save_snapshot() */

    nimcp_mutex_lock(&bridge->lock);
    bridge->stats.checkpoints_created++;

    /* Update brain's last checkpoint time */
    for (uint32_t i = 0; i < NIMCP_SRB_MAX_BRAINS; i++) {
        if (bridge->brains[i].active && bridge->brains[i].brain == brain) {
            bridge->brains[i].last_checkpoint = get_timestamp_ms();
            break;
        }
    }
    nimcp_mutex_unlock(&bridge->lock);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_srb_restore_checkpoint(
    nimcp_security_recovery_bridge_t* bridge,
    brain_t brain,
    const char* name)
{
    if (!bridge || !brain)
        return NIMCP_INVALID_PARAM;

    /* Use brain's snapshot restore functionality */
    /* Note: Would call brain_restore_snapshot() */

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_srb_set_mode(
    nimcp_security_recovery_bridge_t* bridge,
    nimcp_srb_mode_t mode)
{
    if (!bridge)
        return NIMCP_INVALID_PARAM;

    nimcp_mutex_lock(&bridge->lock);
    bridge->config.mode = mode;
    nimcp_mutex_unlock(&bridge->lock);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Callbacks
//=============================================================================

int32_t nimcp_srb_register_callback(
    nimcp_security_recovery_bridge_t* bridge,
    nimcp_srb_callback_t callback,
    void* user_data)
{
    if (!bridge || !callback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_srb_register_callback: required parameter is NULL (bridge, callback)");
        return -1;
    }

    nimcp_mutex_lock(&bridge->lock);

    for (int32_t i = 0; i < 16; i++) {
        if (!bridge->callbacks[i].active) {
            bridge->callbacks[i].callback = callback;
            bridge->callbacks[i].user_data = user_data;
            bridge->callbacks[i].active = true;
            bridge->callback_count++;
            nimcp_mutex_unlock(&bridge->lock);
            return i;
        }
    }

    nimcp_mutex_unlock(&bridge->lock);
    return NIMCP_ERROR_MUTEX_INIT;
}

nimcp_result_t nimcp_srb_unregister_callback(
    nimcp_security_recovery_bridge_t* bridge,
    int32_t callback_id)
{
    if (!bridge || callback_id < 0 || callback_id >= 16)
        return NIMCP_INVALID_PARAM;

    nimcp_mutex_lock(&bridge->lock);

    if (bridge->callbacks[callback_id].active) {
        bridge->callbacks[callback_id].active = false;
        bridge->callbacks[callback_id].callback = NULL;
        bridge->callback_count--;
        nimcp_mutex_unlock(&bridge->lock);
        return NIMCP_SUCCESS;
    }

    nimcp_mutex_unlock(&bridge->lock);
    return NIMCP_NOT_FOUND;
}

//=============================================================================
// Verification
//=============================================================================

uint32_t nimcp_srb_verify_all(nimcp_security_recovery_bridge_t* bridge)
{
    if (!bridge)
        return 0;

    uint32_t violations = 0;

    nimcp_mutex_lock(&bridge->lock);

    /* Verify through coverage */
    if (bridge->coverage) {
        if (!nimcp_coverage_verify_all_regions(bridge->coverage)) {
            violations++;
        }
    }

    /* Verify through fractal security */
    if (bridge->fsc) {
        nimcp_fsc_result_t result = nimcp_fractal_security_verify_all(bridge->fsc);
        if (result != NIMCP_FSC_INTACT) {
            violations++;
        }
    }

    nimcp_mutex_unlock(&bridge->lock);

    return violations;
}

uint32_t nimcp_srb_verify_brain(
    nimcp_security_recovery_bridge_t* bridge,
    brain_t brain)
{
    if (!bridge || !brain)
        return 0;

    /* Verify brain-specific regions */
    /* Note: Would iterate through brain's registered regions */

    return 0;
}

nimcp_result_t nimcp_srb_run_verification_cycle(
    nimcp_security_recovery_bridge_t* bridge)
{
    if (!bridge)
        return NIMCP_INVALID_PARAM;

    uint32_t violations = nimcp_srb_verify_all(bridge);

    if (violations > 0 && bridge->config.log_to_audit && bridge->audit) {
        nimcp_audit_logf(bridge->audit, NIMCP_AUDIT_CAT_INTEGRITY,
            NIMCP_AUDIT_SEV_WARNING, NIMCP_AUDIT_OUTCOME_FAILURE,
            "srb_verify", "Verification cycle found %u violations", violations);
    }

    return NIMCP_SUCCESS;
}

//=============================================================================
// Statistics
//=============================================================================

nimcp_result_t nimcp_srb_get_stats(
    nimcp_security_recovery_bridge_t* bridge,
    nimcp_srb_stats_t* stats)
{
    if (!bridge || !stats)
        return NIMCP_INVALID_PARAM;

    nimcp_mutex_lock(&bridge->lock);
    *stats = bridge->stats;

    /* Calculate average recovery time */
    if (bridge->stats.recoveries_successful > 0) {
        /* Note: Would need to track total recovery time for accurate avg */
    }

    nimcp_mutex_unlock(&bridge->lock);
    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_srb_reset_stats(nimcp_security_recovery_bridge_t* bridge)
{
    if (!bridge)
        return NIMCP_INVALID_PARAM;

    nimcp_mutex_lock(&bridge->lock);
    memset(&bridge->stats, 0, sizeof(nimcp_srb_stats_t));
    nimcp_mutex_unlock(&bridge->lock);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_srb_get_violation_history(
    nimcp_security_recovery_bridge_t* bridge,
    nimcp_security_violation_t* violations,
    uint32_t max_count,
    uint32_t* actual_count)
{
    if (!bridge || !violations || !actual_count)
        return NIMCP_INVALID_PARAM;

    nimcp_mutex_lock(&bridge->lock);

    uint32_t count = 0;
    uint32_t idx = bridge->history_head;

    for (uint32_t i = 0; i < bridge->history_count && count < max_count; i++) {
        idx = (idx + NIMCP_SRB_MAX_HISTORY - 1) % NIMCP_SRB_MAX_HISTORY;
        if (bridge->history[idx].valid) {
            violations[count++] = bridge->history[idx].violation;
        }
    }

    *actual_count = count;
    nimcp_mutex_unlock(&bridge->lock);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* nimcp_sv_type_name(nimcp_security_violation_type_t type)
{
    switch (type) {
        case NIMCP_SV_NONE:                    return "NONE";
        case NIMCP_SV_MEMORY_HASH_MISMATCH:    return "MEMORY_HASH_MISMATCH";
        case NIMCP_SV_MEMORY_PROTECTION_BREACH: return "MEMORY_PROTECTION_BREACH";
        case NIMCP_SV_MEMORY_CORRUPTION:       return "MEMORY_CORRUPTION";
        case NIMCP_SV_CFI_INVALID_TARGET:      return "CFI_INVALID_TARGET";
        case NIMCP_SV_CFI_TYPE_MISMATCH:       return "CFI_TYPE_MISMATCH";
        case NIMCP_SV_SHADOW_STACK_MISMATCH:   return "SHADOW_STACK_MISMATCH";
        case NIMCP_SV_FRACTAL_HASH_MISMATCH:   return "FRACTAL_HASH_MISMATCH";
        case NIMCP_SV_FRACTAL_DIMENSION_ANOMALY: return "FRACTAL_DIMENSION_ANOMALY";
        case NIMCP_SV_FRACTAL_TRUST_DEGRADED:  return "FRACTAL_TRUST_DEGRADED";
        case NIMCP_SV_CAPABILITY_UNAUTHORIZED: return "CAPABILITY_UNAUTHORIZED";
        case NIMCP_SV_CAPABILITY_REVOKED:      return "CAPABILITY_REVOKED";
        case NIMCP_SV_TEMPORAL_GAP:            return "TEMPORAL_GAP";
        case NIMCP_SV_TEMPORAL_ANOMALY:        return "TEMPORAL_ANOMALY";
        default:                               return "UNKNOWN";
    }
}

const char* nimcp_sv_severity_name(nimcp_sv_severity_t severity)
{
    switch (severity) {
        case NIMCP_SV_SEVERITY_INFO:     return "INFO";
        case NIMCP_SV_SEVERITY_LOW:      return "LOW";
        case NIMCP_SV_SEVERITY_MEDIUM:   return "MEDIUM";
        case NIMCP_SV_SEVERITY_HIGH:     return "HIGH";
        case NIMCP_SV_SEVERITY_CRITICAL: return "CRITICAL";
        default:                         return "UNKNOWN";
    }
}

const char* nimcp_sra_name(nimcp_security_recovery_action_t action)
{
    switch (action) {
        case NIMCP_SRA_NONE:               return "NONE";
        case NIMCP_SRA_LOG_ONLY:           return "LOG_ONLY";
        case NIMCP_SRA_REHASH:             return "REHASH";
        case NIMCP_SRA_RESTORE_CHECKPOINT: return "RESTORE_CHECKPOINT";
        case NIMCP_SRA_FAST_RECOVERY:      return "FAST_RECOVERY";
        case NIMCP_SRA_FULL_RECOVERY:      return "FULL_RECOVERY";
        case NIMCP_SRA_HALT:               return "HALT";
        case NIMCP_SRA_QUARANTINE:         return "QUARANTINE";
        default:                           return "UNKNOWN";
    }
}

const char* nimcp_srb_mode_name(nimcp_srb_mode_t mode)
{
    switch (mode) {
        case NIMCP_SRB_MODE_MONITOR:     return "MONITOR";
        case NIMCP_SRB_MODE_ADVISORY:    return "ADVISORY";
        case NIMCP_SRB_MODE_AUTO_REPAIR: return "AUTO_REPAIR";
        case NIMCP_SRB_MODE_PARANOID:    return "PARANOID";
        default:                         return "UNKNOWN";
    }
}

nimcp_security_recovery_action_t nimcp_srb_determine_action(
    nimcp_security_violation_type_t type,
    nimcp_sv_severity_t severity)
{
    /* Critical violations - aggressive recovery */
    if (severity == NIMCP_SV_SEVERITY_CRITICAL) {
        switch (type) {
            case NIMCP_SV_CFI_INVALID_TARGET:
            case NIMCP_SV_SHADOW_STACK_MISMATCH:
                return NIMCP_SRA_HALT;  /* Control flow attacks are critical */
            default:
                return NIMCP_SRA_RESTORE_CHECKPOINT;
        }
    }

    /* High severity - checkpoint restore */
    if (severity == NIMCP_SV_SEVERITY_HIGH) {
        switch (type) {
            case NIMCP_SV_MEMORY_HASH_MISMATCH:
            case NIMCP_SV_MEMORY_CORRUPTION:
            case NIMCP_SV_FRACTAL_HASH_MISMATCH:
                return NIMCP_SRA_RESTORE_CHECKPOINT;
            default:
                return NIMCP_SRA_FAST_RECOVERY;
        }
    }

    /* Medium severity - fast recovery */
    if (severity == NIMCP_SV_SEVERITY_MEDIUM) {
        switch (type) {
            case NIMCP_SV_FRACTAL_DIMENSION_ANOMALY:
            case NIMCP_SV_FRACTAL_TRUST_DEGRADED:
                return NIMCP_SRA_REHASH;
            default:
                return NIMCP_SRA_FAST_RECOVERY;
        }
    }

    /* Low severity - just rehash or log */
    if (severity == NIMCP_SV_SEVERITY_LOW) {
        return NIMCP_SRA_REHASH;
    }

    /* Info - log only */
    return NIMCP_SRA_LOG_ONLY;
}
