/**
 * @file nimcp_blood_brain_barrier.c
 * @brief Blood-Brain Barrier - Perimeter Defense Layer Implementation
 *
 * WHAT: Implementation of the unified BBB API for perimeter security
 * WHY:  Prevent code injection, memory corruption, and unauthorized access
 * HOW:  Four-layer defense with thread-safe operations and threat tracking
 *
 * BIOLOGICAL MODEL:
 *   Endothelial cells (tight junctions) -> Input validation gates
 *   Basement membrane                   -> Code signing verification layer
 *   Astrocyte end-feet                  -> Memory boundary monitors
 *   Pericytes                           -> Access control enforcers
 *
 * NIMCP STANDARDS:
 *   - All functions < 50 lines
 *   - Guard clauses (early returns)
 *   - WHAT-WHY-HOW documentation
 *   - Thread-safe operations
 *
 * @version 1.0.0
 * @date 2025-11-24
 * @author NIMCP Development Team
 */

#include <stddef.h>  /* for NULL */
#include <stdint.h>  /* for INT64_MIN/INT64_MAX */
//=============================================================================
// Includes
//=============================================================================

#include "security/nimcp_blood_brain_barrier.h"
#include "security/nimcp_bbb_enhanced_detection.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdio.h>
#include "constants/nimcp_timing_constants.h"

#define LOG_MODULE "security_bbb"

//=============================================================================
// Health Agent Integration (Phase 8: System-Wide Health Integration)
//=============================================================================
struct nimcp_health_agent;
typedef struct nimcp_health_agent nimcp_health_agent_t;
extern void nimcp_health_agent_heartbeat_ex(nimcp_health_agent_t* agent,
                                             const char* operation,
                                             float progress);

/** Global health agent for blood_brain_barrier module - P0 fix: Use atomic for thread safety */
static _Atomic(nimcp_health_agent_t*) g_blood_brain_barrier_health_agent = NULL;

/**
 * @brief Set health agent for blood_brain_barrier heartbeats
 * @param agent Health agent (can be NULL to disable)
 */
static void blood_brain_barrier_set_health_agent(nimcp_health_agent_t* agent) {
    atomic_store(&g_blood_brain_barrier_health_agent, agent);
}

/** @brief Send heartbeat from blood_brain_barrier module */
static inline void blood_brain_barrier_heartbeat(const char* operation, float progress) {
    /* P0 fix: Atomic load to prevent data race */
    nimcp_health_agent_t* agent = atomic_load(&g_blood_brain_barrier_health_agent);
    if (agent) {
        nimcp_health_agent_heartbeat_ex(agent, operation, progress);
    }
}

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <stdatomic.h>

//=============================================================================
// Constants
//=============================================================================

/**
 * WHAT: Maximum number of threat reports stored in circular buffer
 * WHY:  Limits memory usage while maintaining recent threat history
 */
#define BBB_MAX_THREAT_REPORTS 100

/**
 * WHAT: Maximum number of quarantined memory regions
 * WHY:  Limits tracking overhead for isolated threat regions
 */
#define BBB_MAX_QUARANTINE_REGIONS 64

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * WHAT: Quarantined memory region tracking
 * WHY:  Track isolated regions containing detected threats
 *
 * SECURITY: Uses reference counting to prevent TOCTOU races.
 * When ref_count > 0, the quarantine entry cannot be released.
 */
typedef struct {
    void* address;      /**< Start address of quarantined region */
    size_t size;        /**< Size of quarantined region */
    uint64_t timestamp; /**< When region was quarantined */
    bool active;        /**< Whether region is still quarantined */
    _Atomic int ref_count; /**< Reference count to prevent TOCTOU races */
} bbb_quarantine_entry_t;

/**
 * WHAT: Forward declaration for brain immune system
 * WHY:  Allow BBB to reference brain immune without circular dependency
 */
typedef struct brain_immune_system brain_immune_system_t;

/**
 * WHAT: Internal BBB system structure
 * WHY:  Encapsulates all BBB state with thread-safe access
 */
struct bbb_system_struct {
    /* Configuration */
    bbb_config_t config;

    /* System state */
    bool enabled;
    uint64_t creation_time;

    /* Thread safety */
    nimcp_mutex_t mutex;
    bool mutex_initialized;

    /* Statistics */
    bbb_statistics_t stats;

    /* Threat report circular buffer */
    bbb_threat_report_t threat_reports[BBB_MAX_THREAT_REPORTS];
    size_t threat_report_head;
    size_t threat_report_count;

    /* Quarantine list */
    bbb_quarantine_entry_t quarantine[BBB_MAX_QUARANTINE_REGIONS];
    size_t quarantine_count;

    /* Brain immune system integration */
    brain_immune_system_t* immune_system;

    /* SECURITY FIX: Atomic counter for pending immune operations
     * Prevents use-after-free if immune system is detached while operations pending */
    _Atomic int pending_immune_ops;

    /* SECURITY FIX: Condition variable for waiting on pending immune operations */
    nimcp_cond_t immune_ops_cond;
    bool immune_ops_cond_initialized;
};

//=============================================================================
// Note: bbb_calculate_hash is implemented in nimcp_bbb_code_signing.c
// using proper SHA256 algorithm for cryptographic security
//=============================================================================

//=============================================================================
// Utility Functions - Name Conversions
//=============================================================================

/**
 * @brief Get human-readable threat type name
 *
 * WHAT: Convert threat type enum to string
 * WHY:  Human-readable logging and reporting
 * HOW:  Static lookup table indexed by enum
 */
NIMCP_EXPORT const char* bbb_threat_type_name(bbb_threat_type_t type)
{
    static const char* names[] = {
        "NONE",
        "BUFFER_OVERFLOW",
        "FORMAT_STRING",
        "INTEGER_OVERFLOW",
        "SQL_INJECTION",
        "CODE_INJECTION",
        "SHELLCODE",
        "ROP_CHAIN",
        "INVALID_SIGNATURE",
        "MEMORY_VIOLATION",
        "UNAUTHORIZED_ACCESS",
        "DATA_TAMPERING",
        "PATH_TRAVERSAL",
        "SHELL_INJECTION",
        "UNKNOWN"
    };

    if (type < 0 || type > BBB_THREAT_UNKNOWN) {
        return "INVALID";
    }

    return names[type];
}

/**
 * @brief Get human-readable severity name
 *
 * WHAT: Convert severity enum to string
 * WHY:  Human-readable logging and reporting
 * HOW:  Static lookup table indexed by enum
 */
NIMCP_EXPORT const char* bbb_severity_name(bbb_severity_t severity)
{
    static const char* names[] = {
        "NONE",
        "LOW",
        "MEDIUM",
        "HIGH",
        "CRITICAL"
    };

    if (severity < 0 || severity > BBB_SEVERITY_CRITICAL) {
        return "INVALID";
    }

    return names[severity];
}

/**
 * @brief Get human-readable action name
 *
 * WHAT: Convert action enum to string
 * WHY:  Human-readable logging and reporting
 * HOW:  Static lookup table indexed by enum
 */
NIMCP_EXPORT const char* bbb_action_name(bbb_action_t action)
{
    static const char* names[] = {
        "ALLOW",
        "LOG",
        "BLOCK",
        "QUARANTINE",
        "TERMINATE",
        "LOCKDOWN"
    };

    if (action < 0 || action > BBB_ACTION_LOCKDOWN) {
        return "INVALID";
    }

    return names[action];
}

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Get default BBB configuration with conservative security settings
 *
 * WHAT: Return default configuration struct
 * WHY:  Provide sensible defaults for immediate use
 * HOW:  Static initialization of all config fields
 */
NIMCP_EXPORT bbb_config_t bbb_default_config(void)
{
    bbb_config_t config;
    memset(&config, 0, sizeof(config));

    /* Input gate defaults - conservative validation */
    config.input.validate_strings = true;
    config.input.validate_integers = true;
    config.input.validate_pointers = true;
    config.input.sanitize_html = true;
    config.input.sanitize_sql = true;
    config.input.max_string_length = 65536;     /* 64KB max string */
    config.input.max_array_size = 1048576;      /* 1M elements max */
    config.input.min_integer = INT64_MIN / 2;   /* Half range to detect overflow */
    config.input.max_integer = INT64_MAX / 2;

    /* Code signing defaults */
    config.signing.require_signatures = false;  /* Off by default for ease of use */
    config.signing.verify_on_load = true;
    config.signing.verify_on_execute = false;   /* Performance-sensitive */
    config.signing.trusted_keys_path = NULL;
    config.signing.hash_algorithm = 0;          /* SHA256 */

    /* Memory boundary defaults */
    config.memory.enable_stack_canaries = true;
    config.memory.enable_heap_guards = true;
    config.memory.enable_aslr = true;
    config.memory.enable_wx_protection = true;
    config.memory.enable_mprotect = true;
    config.memory.guard_page_size = 4096;
    config.memory.stack_canary_size = 8;

    /* Access control defaults */
    config.access.enable_rbac = true;
    config.access.enable_mac = false;           /* Mandatory AC off by default */
    config.access.enable_capability = true;
    config.access.log_access_attempts = true;
    config.access.max_privilege_level = 16;   /* Support up to 16 privilege levels */
    config.access.policy_file = NULL;

    /* System defaults */
    config.strict_mode = false;
    config.default_action = BBB_ACTION_BLOCK;
    config.alert_callback = NULL;

    return config;
}

//=============================================================================
// Internal Statistics Update Functions (for sub-modules)
//=============================================================================

/**
 * @brief Increment validation counter (internal use)
 *
 * WHAT: Thread-safe increment of total_validations stat
 * WHY:  Allow sub-modules to update stats without exposing struct
 */
void bbb_system_inc_validations(bbb_system_t system)
{
    if (!system) return;
    nimcp_mutex_lock(&system->mutex);
    system->stats.total_validations++;
    nimcp_mutex_unlock(&system->mutex);
}

/**
 * @brief Increment threat detection counter (internal use)
 *
 * WHAT: Thread-safe increment of threats_detected stat
 * WHY:  Allow sub-modules to update stats without exposing struct
 */
void bbb_system_inc_threats(bbb_system_t system)
{
    if (!system) return;
    nimcp_mutex_lock(&system->mutex);
    system->stats.threats_detected++;
    nimcp_mutex_unlock(&system->mutex);
}

/**
 * @brief Get max string length from system config
 *
 * WHAT: Thread-safe accessor for system's max_string_length config
 * WHY:  Allow sub-modules to access system config without exposing struct
 */
size_t bbb_system_get_max_string_length(bbb_system_t system)
{
    if (!system) return 0;
    nimcp_mutex_lock(&system->mutex);
    size_t max_len = system->config.input.max_string_length;
    nimcp_mutex_unlock(&system->mutex);
    return max_len;
}

/**
 * @brief Get min integer bound from system config
 *
 * WHAT: Thread-safe accessor for system's input.min_integer config
 * WHY:  Allow sub-modules to honor configured integer bounds without exposing struct
 */
int64_t bbb_system_get_min_integer(bbb_system_t system)
{
    if (!system) return INT64_MIN;
    nimcp_mutex_lock(&system->mutex);
    int64_t v = system->config.input.min_integer;
    nimcp_mutex_unlock(&system->mutex);
    return v;
}

/**
 * @brief Get max integer bound from system config
 *
 * WHAT: Thread-safe accessor for system's input.max_integer config
 * WHY:  Allow sub-modules to honor configured integer bounds without exposing struct
 */
int64_t bbb_system_get_max_integer(bbb_system_t system)
{
    if (!system) return INT64_MAX;
    nimcp_mutex_lock(&system->mutex);
    int64_t v = system->config.input.max_integer;
    nimcp_mutex_unlock(&system->mutex);
    return v;
}

//=============================================================================
// System Lifecycle
//=============================================================================

/**
 * @brief Create BBB system with configuration
 *
 * WHAT: Allocate and initialize BBB system struct
 * WHY:  Create new security perimeter instance
 * HOW:  Malloc, initialize mutex, copy config, set defaults
 */
NIMCP_EXPORT bbb_system_t bbb_system_create(const bbb_config_t* config)
{
    /* Allocate system structure */
    bbb_system_t system = (bbb_system_t)nimcp_calloc(1, sizeof(struct bbb_system_struct));
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate BBB system structure");
        return NULL;
    }

    /* Initialize mutex */
    if (nimcp_mutex_init(&system->mutex, NULL) != NIMCP_SUCCESS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_MUTEX_INIT, "Failed to initialize BBB mutex");
        nimcp_free(system);
        return NULL;
    }
    system->mutex_initialized = true;

    /* Copy configuration or use defaults */
    if (config) {
        system->config = *config;
    } else {
        system->config = bbb_default_config();
    }

    /* Initialize state */
    system->enabled = true;
    system->creation_time = (uint64_t)time(NULL);
    system->threat_report_head = 0;
    system->threat_report_count = 0;
    system->quarantine_count = 0;

    /* Initialize statistics */
    memset(&system->stats, 0, sizeof(system->stats));

    /* Initialize immune system integration */
    system->immune_system = NULL;
    atomic_init(&system->pending_immune_ops, 0);

    /* Initialize condition variable for immune ops synchronization */
    if (nimcp_cond_init(&system->immune_ops_cond) == NIMCP_SUCCESS) {
        system->immune_ops_cond_initialized = true;
    } else {
        system->immune_ops_cond_initialized = false;
        LOG_WARN("bbb_system_create: Failed to initialize immune ops condition variable");
    }

    /* Wire enhanced detection (path traversal + shell injection).
     * Defense-in-depth: if init fails the static patterns still work, so
     * we log and continue rather than failing system creation. */
    if (!bbb_enhanced_detection_init()) {
        LOG_WARN("bbb_system_create: bbb_enhanced_detection_init failed — "
                 "path/shell detectors disabled, falling back to static patterns");
    }

    return system;
}

/**
 * @brief Destroy BBB system and free all resources
 *
 * WHAT: Clean up and deallocate BBB system
 * WHY:  Prevent memory leaks and resource exhaustion
 * HOW:  Destroy mutex, zero sensitive data, free memory
 */
NIMCP_EXPORT void bbb_system_destroy(bbb_system_t system)
{
    /* Guard: Null system */
    if (!system) {
        return;
    }

    /* Destroy condition variable if initialized */
    if (system->immune_ops_cond_initialized) {
        nimcp_cond_destroy(&system->immune_ops_cond);
    }

    /* Destroy mutex if initialized */
    if (system->mutex_initialized) {
        nimcp_mutex_destroy(&system->mutex);
    }

    /* Zero sensitive data before freeing */
    memset(system->threat_reports, 0, sizeof(system->threat_reports));
    memset(&system->stats, 0, sizeof(system->stats));

    /* Free system */
    nimcp_free(system);
}

//=============================================================================
// Enable/Disable Control
//=============================================================================

/**
 * @brief Enable or disable the BBB system
 *
 * WHAT: Toggle BBB protection on/off
 * WHY:  Allow runtime control of security layer
 * HOW:  Thread-safe flag modification
 */
NIMCP_EXPORT bool bbb_system_set_enabled(bbb_system_t system, bool enabled)
{
    /* Guard: Null system */
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL system in bbb_system_set_enabled");
        return false;
    }

    nimcp_mutex_lock(&system->mutex);
    system->enabled = enabled;
    nimcp_mutex_unlock(&system->mutex);

    return true;
}

/**
 * @brief Check if BBB system is enabled
 *
 * WHAT: Query current enabled state
 * WHY:  Check protection status before operations
 * HOW:  Thread-safe flag read
 */
NIMCP_EXPORT bool bbb_system_is_enabled(bbb_system_t system)
{
    /* Guard: Null system */
    if (!system) {
        return false;
    }

    nimcp_mutex_lock(&system->mutex);
    bool enabled = system->enabled;
    nimcp_mutex_unlock(&system->mutex);

    return enabled;
}

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Get BBB system statistics
 *
 * WHAT: Retrieve current security statistics
 * WHY:  Monitoring, reporting, and analysis
 * HOW:  Thread-safe copy of stats struct
 */
NIMCP_EXPORT bool bbb_system_get_statistics(bbb_system_t system, bbb_statistics_t* stats)
{
    /* Guard: Null parameters */
    if (!system || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL system or stats in bbb_system_get_statistics");
        return false;
    }

    nimcp_mutex_lock(&system->mutex);

    /* Copy statistics */
    *stats = system->stats;

    /* Calculate uptime */
    stats->uptime_seconds = (uint64_t)time(NULL) - system->creation_time;

    nimcp_mutex_unlock(&system->mutex);

    return true;
}

/**
 * @brief Reset BBB system statistics
 *
 * WHAT: Clear all accumulated statistics
 * WHY:  Start fresh measurement period
 * HOW:  Thread-safe zero of stats struct
 */
NIMCP_EXPORT void bbb_system_reset_statistics(bbb_system_t system)
{
    /* Guard: Null system */
    if (!system) {
        return;
    }

    nimcp_mutex_lock(&system->mutex);
    memset(&system->stats, 0, sizeof(system->stats));
    nimcp_mutex_unlock(&system->mutex);
}

//=============================================================================
// Brain Immune System Integration
//=============================================================================

/**
 * @brief Connect BBB to brain immune system
 *
 * WHAT: Link BBB threat detection to immune system
 * WHY:  Enable automatic threat forwarding and coordinated response
 * HOW:  Store immune system reference, enable automatic presentation
 *
 * @param system BBB system handle
 * @param immune_system Brain immune system handle
 * @return true on success
 */
NIMCP_EXPORT bool bbb_connect_immune(bbb_system_t system, brain_immune_system_t* immune_system)
{
    /* Guard: Null parameters */
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL system in bbb_connect_immune");
        return false;
    }

    nimcp_mutex_lock(&system->mutex);

    /* SECURITY FIX: If disconnecting (setting to NULL), wait for pending operations.
     * This prevents use-after-free when immune system is being detached.
     * Uses condition variable instead of spin-wait for proper synchronization. */
    if (immune_system == NULL && system->immune_system != NULL) {
        /* W4-11 fix: Set immune_system to NULL FIRST to prevent new operations
         * from starting (they check immune_system before incrementing
         * pending_immune_ops). Then wait for in-flight operations to drain. */
        system->immune_system = NULL;

        /* Wait for pending immune operations to complete using condition variable */
        const uint32_t TIMEOUT_MS = NIMCP_MEDIUM_TIMEOUT_MS;  /* 1 second timeout */
        int total_wait_ms = 0;
        const int WAIT_INTERVAL_MS = 10;   /* Check every 10ms */

        while (atomic_load(&system->pending_immune_ops) > 0 && total_wait_ms < TIMEOUT_MS) {
            if (system->immune_ops_cond_initialized) {
                /* Use condition variable with timeout for proper waiting */
                nimcp_cond_timedwait(&system->immune_ops_cond, &system->mutex, WAIT_INTERVAL_MS);
            } else {
                /* Fallback: release mutex briefly to allow pending ops to complete */
                nimcp_mutex_unlock(&system->mutex);
                struct timespec ts = {0, WAIT_INTERVAL_MS * 1000000L}; /* Convert ms to ns */
                nanosleep(&ts, NULL);
                nimcp_mutex_lock(&system->mutex);
            }
            total_wait_ms += WAIT_INTERVAL_MS;
        }

        if (atomic_load(&system->pending_immune_ops) > 0) {
            LOG_WARN("bbb_connect_immune: Timeout waiting for %d pending immune operations after %dms",
                     atomic_load(&system->pending_immune_ops), total_wait_ms);
        }
    } else {
        system->immune_system = immune_system;
    }
    nimcp_mutex_unlock(&system->mutex);

    return true;
}

/**
 * @brief Map BBB severity to immune inflammation level
 *
 * WHAT: Convert BBB severity to immune inflammation severity
 * WHY:  Coordinate threat escalation across systems
 * HOW:  Direct mapping based on severity scale
 */
static brain_inflammation_level_t bbb_severity_to_inflammation(bbb_severity_t severity)
{
    switch (severity) {
        case BBB_SEVERITY_NONE:
            return INFLAMMATION_NONE;
        case BBB_SEVERITY_LOW:
            return INFLAMMATION_LOCAL;
        case BBB_SEVERITY_MEDIUM:
            return INFLAMMATION_REGIONAL;
        case BBB_SEVERITY_HIGH:
            return INFLAMMATION_SYSTEMIC;
        case BBB_SEVERITY_CRITICAL:
            return INFLAMMATION_STORM;
        default:
            return INFLAMMATION_LOCAL;
    }
}

/**
 * @brief Forward BBB threat to immune system
 *
 * WHAT: Present BBB threat as antigen to immune system
 * WHY:  Enable immune response to BBB-detected threats
 * HOW:  Call brain_immune_present_bbb_threat with threat data
 */
static void bbb_forward_threat_to_immune(bbb_system_t system,
                                         const bbb_threat_report_t* report,
                                         const void* threat_data,
                                         size_t threat_size)
{
    /* Guard: No immune system connected */
    if (!system->immune_system) {
        return;
    }

    /* Present threat to immune system */
    uint32_t antigen_id = 0;
    brain_immune_present_bbb_threat(
        system->immune_system,
        report->type,
        report->severity,
        (const uint8_t*)threat_data,
        threat_size,
        &antigen_id
    );

    /* Initiate inflammation if severity warrants */
    if (report->severity >= BBB_SEVERITY_MEDIUM) {
        brain_inflammation_level_t level = bbb_severity_to_inflammation(report->severity);
        uint32_t site_id = 0;
        brain_immune_initiate_inflammation(
            system->immune_system,
            0, /* region_id: 0 for BBB (perimeter) */
            antigen_id,
            &site_id
        );
    }
}

//=============================================================================
// Threat Reporting
//=============================================================================

/**
 * @brief Determine action based on severity and configuration
 *
 * WHAT: Map severity to appropriate response action
 * WHY:  Automated threat response based on severity
 * HOW:  Severity threshold checks
 */
static bbb_action_t determine_action(bbb_system_t system, bbb_severity_t severity)
{
    if (system->config.strict_mode) {
        /* Strict mode: aggressive response */
        if (severity >= BBB_SEVERITY_MEDIUM) {
            return BBB_ACTION_QUARANTINE;
        }
        return BBB_ACTION_BLOCK;
    }

    /* Normal mode: graduated response */
    switch (severity) {
        case BBB_SEVERITY_NONE:
            return BBB_ACTION_ALLOW;
        case BBB_SEVERITY_LOW:
            return BBB_ACTION_LOG;
        case BBB_SEVERITY_MEDIUM:
            return BBB_ACTION_BLOCK;
        case BBB_SEVERITY_HIGH:
            return BBB_ACTION_QUARANTINE;
        case BBB_SEVERITY_CRITICAL:
            return BBB_ACTION_TERMINATE;
        default:
            return system->config.default_action;
    }
}

/**
 * @brief Report a detected threat to the BBB system
 *
 * WHAT: Create threat report with timestamp and hash
 * WHY:  Record threats for analysis and response
 * HOW:  Populate report struct, add to circular buffer
 */
NIMCP_EXPORT bbb_threat_report_t bbb_report_threat(bbb_system_t system,
                                                    bbb_threat_type_t type,
                                                    bbb_severity_t severity,
                                                    const char* description,
                                                    const void* source_address,
                                                    const void* threat_data,
                                                    size_t threat_size)
{
    bbb_threat_report_t report;
    memset(&report, 0, sizeof(report));

    /* Guard: Null system - return empty report */
    if (!system) {
        return report;
    }

    /* Populate report */
    report.type = type;
    report.severity = severity;
    report.timestamp = (uint64_t)time(NULL);
    report.source_address = source_address;
    report.threat_size = threat_size;
    report.quarantined = false;

    /* Copy description */
    if (description) {
        strncpy(report.description, description, sizeof(report.description) - 1);
        report.description[sizeof(report.description) - 1] = '\0';
    }

    /* Calculate threat hash */
    if (threat_data && threat_size > 0) {
        bbb_calculate_hash(threat_data, threat_size, report.threat_hash);
    }

    /* Determine action */
    report.action_taken = determine_action(system, severity);

    /* Thread-safe addition to circular buffer */
    nimcp_mutex_lock(&system->mutex);

    /* Update statistics */
    system->stats.threats_detected++;
    if (report.action_taken == BBB_ACTION_BLOCK ||
        report.action_taken == BBB_ACTION_QUARANTINE) {
        /* Both BLOCK and QUARANTINE prevent the threat */
        system->stats.threats_blocked++;
    }
    if (report.action_taken == BBB_ACTION_QUARANTINE) {
        system->stats.threats_quarantined++;
        report.quarantined = true;
    }

    /* Add to circular buffer */
    system->threat_reports[system->threat_report_head] = report;
    system->threat_report_head = (system->threat_report_head + 1) % BBB_MAX_THREAT_REPORTS;
    if (system->threat_report_count < BBB_MAX_THREAT_REPORTS) {
        system->threat_report_count++;
    }

    nimcp_mutex_unlock(&system->mutex);

    /* Forward threat to immune system if connected */
    bbb_forward_threat_to_immune(system, &report, threat_data, threat_size);

    /* Invoke alert callback if configured */
    if (system->config.alert_callback && severity >= BBB_SEVERITY_MEDIUM) {
        system->config.alert_callback(type, severity, description);
    }

    return report;
}

/**
 * @brief Get recent threat reports from circular buffer
 *
 * WHAT: Retrieve stored threat reports
 * WHY:  Analysis and forensics of detected threats
 * HOW:  Copy from circular buffer to output array
 */
NIMCP_EXPORT size_t bbb_get_threat_reports(bbb_system_t system,
                                           bbb_threat_report_t* reports,
                                           size_t max_reports)
{
    /* Guard: Null parameters */
    if (!system || !reports || max_reports == 0) {
        return 0;
    }

    nimcp_mutex_lock(&system->mutex);

    size_t count = (system->threat_report_count < max_reports)
                   ? system->threat_report_count : max_reports;

    /* Copy reports from circular buffer (newest first) */
    for (size_t i = 0; i < count; i++) {
        size_t idx = (system->threat_report_head + BBB_MAX_THREAT_REPORTS - 1 - i)
                     % BBB_MAX_THREAT_REPORTS;
        reports[i] = system->threat_reports[idx];
    }

    nimcp_mutex_unlock(&system->mutex);

    return count;
}

/**
 * @brief Clear all threat reports
 *
 * WHAT: Empty the threat report buffer
 * WHY:  Reset after handling or periodic cleanup
 * HOW:  Zero buffer and reset indices
 */
NIMCP_EXPORT void bbb_clear_threat_reports(bbb_system_t system)
{
    /* Guard: Null system */
    if (!system) {
        return;
    }

    nimcp_mutex_lock(&system->mutex);
    memset(system->threat_reports, 0, sizeof(system->threat_reports));
    system->threat_report_head = 0;
    system->threat_report_count = 0;
    nimcp_mutex_unlock(&system->mutex);
}

//=============================================================================
// Quarantine Management
//=============================================================================

/**
 * @brief Check if an address is in a quarantined region
 *
 * WHAT: Test if address overlaps any quarantined region
 * WHY:  Prevent access to quarantined memory
 * HOW:  Scan quarantine list for overlap
 */
NIMCP_EXPORT bool bbb_is_quarantined(bbb_system_t system, const void* address, size_t size)
{
    if (!system || !address || size == 0) {
        return false;
    }

    nimcp_mutex_lock(&system->mutex);

    uintptr_t check_start = (uintptr_t)address;

    /* Check for integer overflow in address + size */
    if (size > UINTPTR_MAX - check_start) {
        /* Overflow would occur - treat as invalid/quarantined */
        nimcp_mutex_unlock(&system->mutex);
        return true;
    }

    uintptr_t check_end = check_start + size;

    for (size_t i = 0; i < BBB_MAX_QUARANTINE_REGIONS; i++) {
        if (!system->quarantine[i].active) continue;

        uintptr_t q_start = (uintptr_t)system->quarantine[i].address;

        /* Check for integer overflow in quarantine region */
        if (system->quarantine[i].size > UINTPTR_MAX - q_start) {
            /* Overflow in quarantine region - skip it */
            continue;
        }

        uintptr_t q_end = q_start + system->quarantine[i].size;

        /* Check for overlap */
        if (check_start < q_end && check_end > q_start) {
            nimcp_mutex_unlock(&system->mutex);
            return true;
        }
    }

    /* Region is not quarantined - simply return false without throwing */
    nimcp_mutex_unlock(&system->mutex);
    return false;
}

/**
 * @brief TOCTOU-safe quarantine check with optional reference acquisition
 *
 * WHAT: Atomically check quarantine status and optionally acquire reference
 * WHY:  Prevents race condition between check and use
 * HOW:  If acquire_ref is true and region is not quarantined, increments ref_count
 *
 * @param system BBB system handle
 * @param address Address to check
 * @param size Size of region to check
 * @param acquire_ref If true and not quarantined, acquire reference (must call bbb_release_quarantine_ref)
 * @return true if quarantined, false if not quarantined (and reference acquired if requested)
 *
 * @note Caller MUST call bbb_release_quarantine_ref() when done if acquire_ref was true
 *       and this function returned false.
 */
NIMCP_EXPORT bool bbb_is_quarantined_safe(bbb_system_t system, const void* address, size_t size, bool acquire_ref)
{
    if (!system || !address || size == 0) {
        return false;
    }

    nimcp_mutex_lock(&system->mutex);

    uintptr_t check_start = (uintptr_t)address;

    /* Check for integer overflow in address + size */
    if (size > UINTPTR_MAX - check_start) {
        nimcp_mutex_unlock(&system->mutex);
        return true;  /* Treat overflow as quarantined for safety */
    }

    uintptr_t check_end = check_start + size;

    for (size_t i = 0; i < BBB_MAX_QUARANTINE_REGIONS; i++) {
        if (!system->quarantine[i].active) continue;

        uintptr_t q_start = (uintptr_t)system->quarantine[i].address;

        if (system->quarantine[i].size > UINTPTR_MAX - q_start) {
            continue;
        }

        uintptr_t q_end = q_start + system->quarantine[i].size;

        /* Check for overlap */
        if (check_start < q_end && check_end > q_start) {
            /* P0-1 FIX: Only increment ref_count on the MATCHING overlapping region,
             * not all active regions. This prevents over-incrementing unrelated entries. */
            if (acquire_ref) {
                atomic_fetch_add(&system->quarantine[i].ref_count, 1);
            }
            nimcp_mutex_unlock(&system->mutex);
            return true;  /* Region is quarantined */
        }
    }

    /* P0-3 FIX: Region is not quarantined - simply return false without throwing.
     * The previous code unconditionally threw NIMCP_THROW_TO_IMMUNE here, which
     * incorrectly treated a normal "not quarantined" result as an error. */
    nimcp_mutex_unlock(&system->mutex);
    return false;
}

/**
 * @brief Release quarantine reference for a specific region
 *
 * WHAT: Release reference acquired during TOCTOU-safe check for a specific region
 * WHY:  P0-2 FIX: Only decrement ref_count on the matching overlapping region,
 *       not all active regions (which was the old broken behavior)
 * HOW:  Find the overlapping quarantine entry and atomically decrement its ref_count
 *       using a compare-exchange loop to prevent underflow (P1-1 FIX)
 *
 * @param system BBB system handle
 * @param address Address of the region to release
 * @param size Size of the region to release
 */
NIMCP_EXPORT void bbb_release_quarantine_ref_for_region(bbb_system_t system,
                                                          const void* address,
                                                          size_t size)
{
    if (!system || !address || size == 0) return;

    nimcp_mutex_lock(&system->mutex);

    uintptr_t check_start = (uintptr_t)address;
    if (size > UINTPTR_MAX - check_start) {
        nimcp_mutex_unlock(&system->mutex);
        return;
    }
    uintptr_t check_end = check_start + size;

    for (size_t i = 0; i < BBB_MAX_QUARANTINE_REGIONS; i++) {
        if (!system->quarantine[i].active) continue;

        uintptr_t q_start = (uintptr_t)system->quarantine[i].address;
        if (system->quarantine[i].size > UINTPTR_MAX - q_start) continue;
        uintptr_t q_end = q_start + system->quarantine[i].size;

        /* Check for overlap - same logic as bbb_is_quarantined_safe */
        if (check_start < q_end && check_end > q_start) {
            /* P1-1 FIX: Use atomic compare-exchange loop to prevent underflow.
             * The old code did atomic_fetch_sub FIRST then checked for underflow,
             * creating a window where another thread could read a negative value. */
            int current = atomic_load(&system->quarantine[i].ref_count);
            if (current <= 0) {
                /* Already zero, skip - don't go negative */
                LOG_WARN("bbb_release_quarantine_ref_for_region: ref_count already zero for region %p",
                         system->quarantine[i].address);
                break;
            }
            while (!atomic_compare_exchange_weak(&system->quarantine[i].ref_count,
                                                  &current, current - 1)) {
                if (current <= 0) break;  /* Became zero while we were trying */
            }
            break;  /* Only decrement the first matching region */
        }
    }

    nimcp_mutex_unlock(&system->mutex);
}

/**
 * @brief Release quarantine reference acquired by bbb_is_quarantined_safe (DEPRECATED)
 *
 * WHAT: Release reference acquired during TOCTOU-safe check
 * WHY:  Backward compatibility wrapper - prefer bbb_release_quarantine_ref_for_region()
 * HOW:  P0-2 FIX: The old implementation decremented ALL active quarantine entries,
 *       which was mismatched with the acquire pattern. This version logs a deprecation
 *       warning and attempts to decrement any region with a positive ref_count.
 *
 * @param system BBB system handle
 * @deprecated Use bbb_release_quarantine_ref_for_region() instead for targeted release
 */
NIMCP_EXPORT void bbb_release_quarantine_ref(bbb_system_t system)
{
    if (!system) return;

    LOG_WARN("bbb_release_quarantine_ref: DEPRECATED - use bbb_release_quarantine_ref_for_region() instead");

    /* P0-2 FIX: Best-effort backward compat - find first region with positive ref_count
     * and decrement it using the safe CAS loop (P1-1 FIX). This is imprecise because
     * we don't know which region the caller intended to release. */
    nimcp_mutex_lock(&system->mutex);
    for (size_t i = 0; i < BBB_MAX_QUARANTINE_REGIONS; i++) {
        if (!system->quarantine[i].active) continue;

        int current = atomic_load(&system->quarantine[i].ref_count);
        if (current <= 0) continue;

        /* P1-1 FIX: Atomic compare-exchange loop to prevent underflow race */
        while (!atomic_compare_exchange_weak(&system->quarantine[i].ref_count,
                                              &current, current - 1)) {
            if (current <= 0) break;
        }
        break;  /* Only decrement one region for backward compat */
    }
    nimcp_mutex_unlock(&system->mutex);
}

/**
 * @brief Quarantine a memory region containing a threat
 *
 * WHAT: Isolate suspicious memory region
 * WHY:  Contain threat while preserving evidence
 * HOW:  Add to quarantine tracking list
 */
NIMCP_EXPORT bool bbb_quarantine_region(bbb_system_t system, void* address, size_t size)
{
    /* Guard: Null parameters */
    if (!system || !address || size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Invalid parameters in bbb_quarantine_region");
        return false;
    }

    nimcp_mutex_lock(&system->mutex);

    /* Check if we have room */
    if (system->quarantine_count >= BBB_MAX_QUARANTINE_REGIONS) {
        nimcp_mutex_unlock(&system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "Quarantine region limit exceeded");
        return false;
    }

    /* Find free slot (may have gaps from releases) */
    size_t slot = 0;
    bool found = false;
    for (size_t i = 0; i < BBB_MAX_QUARANTINE_REGIONS; i++) {
        if (!system->quarantine[i].active) {
            slot = i;
            found = true;
            break;
        }
    }

    if (!found) {
        nimcp_mutex_unlock(&system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bbb_quarantine_region: found is NULL");
        return false;
    }

    /* Add to quarantine */
    system->quarantine[slot].address = address;
    system->quarantine[slot].size = size;
    system->quarantine[slot].timestamp = (uint64_t)time(NULL);
    system->quarantine[slot].active = true;
    atomic_init(&system->quarantine[slot].ref_count, 0);  /* Initialize ref_count */
    system->quarantine_count++;

    /* Update statistics */
    system->stats.threats_quarantined++;

    /* SECURITY FIX: Use atomic reference count to prevent use-after-free.
     * The pending_immune_ops counter prevents the immune system from being
     * detached (set to NULL) while operations are in progress:
     *   1. We increment the counter BEFORE copying the pointer
     *   2. bbb_connect_immune(NULL) will wait for counter to reach zero
     *   3. We decrement counter AFTER immune calls complete
     *   4. Signal condition variable to wake up any waiting disconnect calls
     * This creates a safe window where the immune pointer remains valid. */
    brain_immune_system_t* immune = system->immune_system;
    bool has_immune = (immune != NULL);
    if (has_immune) {
        atomic_fetch_add(&system->pending_immune_ops, 1);
    }

    /* Release BBB mutex BEFORE calling immune functions (prevents deadlock).
     * At this point, pending_immune_ops > 0 guarantees immune pointer validity. */
    nimcp_mutex_unlock(&system->mutex);

    /* Activate killer T cell to handle quarantined region (without holding BBB mutex) */
    if (has_immune) {
        /* P2-1 FIX: Ensure pending_immune_ops is ALWAYS decremented after immune calls.
         * Even though NIMCP_THROW_TO_IMMUNE doesn't longjmp (execution continues),
         * we structure the code to guarantee the decrement is always reached by
         * performing all immune calls first, then unconditionally decrementing. */

        /* Create antigen for quarantine event */
        uint32_t antigen_id = 0;
        uint8_t epitope[32];
        memset(epitope, 0, sizeof(epitope));
        /* Hash address as epitope signature */
        snprintf((char*)epitope, sizeof(epitope), "QUARANTINE:%p", address);

        brain_immune_present_antigen(
            immune,
            ANTIGEN_SOURCE_BBB,
            epitope,
            strlen((const char*)epitope),
            8, /* High severity for quarantine */
            (uint32_t)(uintptr_t)address, /* Use address as source node */
            &antigen_id
        );

        /* Activate killer T cell for direct action */
        uint32_t t_cell_id = 0;
        brain_immune_activate_killer_t(immune, antigen_id, &t_cell_id);

        /* P2-1 FIX: Decrement pending operations counter UNCONDITIONALLY after
         * all immune calls. This is placed outside any conditional logic to ensure
         * the counter is always decremented, preventing permanent blocking of
         * bbb_connect_immune(NULL) disconnect calls. */
        atomic_fetch_sub(&system->pending_immune_ops, 1);
        if (system->immune_ops_cond_initialized) {
            nimcp_cond_signal(&system->immune_ops_cond);
        }
    }

    return true;
}

/**
 * @brief Release a quarantined memory region
 *
 * WHAT: Remove region from quarantine tracking
 * WHY:  Allow cleanup after threat handling
 * HOW:  Find and deactivate quarantine entry
 */
NIMCP_EXPORT bool bbb_release_quarantine(bbb_system_t system, void* address)
{
    /* Guard: Null parameters */
    if (!system || !address) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL system or address in bbb_release_quarantine");
        return false;
    }

    nimcp_mutex_lock(&system->mutex);

    /* Find quarantine entry */
    bool found = false;
    for (size_t i = 0; i < BBB_MAX_QUARANTINE_REGIONS; i++) {
        if (system->quarantine[i].active &&
            system->quarantine[i].address == address) {
            /* SECURITY FIX: Check ref_count before releasing to prevent TOCTOU issues */
            int ref = atomic_load(&system->quarantine[i].ref_count);
            if (ref > 0) {
                LOG_WARN("bbb_release_quarantine: Cannot release region %p with %d active references",
                         address, ref);
                nimcp_mutex_unlock(&system->mutex);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bbb_release_quarantine: validation failed");
                return false;  /* Cannot release while references are held */
            }
            system->quarantine[i].active = false;
            system->quarantine_count--;
            found = true;
            break;
        }
    }

    nimcp_mutex_unlock(&system->mutex);

    return found;
}

//=============================================================================
// Print Functions
//=============================================================================

/**
 * @brief Print BBB statistics to stdout
 *
 * WHAT: Display formatted statistics summary
 * WHY:  Human-readable status reporting
 * HOW:  Printf formatted output
 */
NIMCP_EXPORT void bbb_print_statistics(const bbb_statistics_t* stats)
{
    /* Guard: Null stats */
    if (!stats) {
        printf("BBB Statistics: (null)\n");
        return;
    }

    printf("\n");
    printf("=== Blood-Brain Barrier Statistics ===\n");
    printf("Uptime:                %lu seconds\n", (unsigned long)stats->uptime_seconds);
    printf("Total Validations:     %lu\n", (unsigned long)stats->total_validations);
    printf("Threats Detected:      %lu\n", (unsigned long)stats->threats_detected);
    printf("Threats Blocked:       %lu\n", (unsigned long)stats->threats_blocked);
    printf("Threats Quarantined:   %lu\n", (unsigned long)stats->threats_quarantined);
    printf("False Positives:       %lu\n", (unsigned long)stats->false_positives);
    printf("Signatures Verified:   %lu\n", (unsigned long)stats->signatures_verified);
    printf("Signature Failures:    %lu\n", (unsigned long)stats->signatures_failed);
    printf("Memory Violations:     %lu\n", (unsigned long)stats->memory_violations);
    printf("Access Violations:     %lu\n", (unsigned long)stats->access_violations);
    printf("======================================\n");
    printf("\n");
}

/**
 * @brief Print threat report to stdout
 *
 * WHAT: Display formatted threat report
 * WHY:  Human-readable threat details
 * HOW:  Printf formatted output with hash display
 */
NIMCP_EXPORT void bbb_print_threat_report(const bbb_threat_report_t* report)
{
    /* Guard: Null report */
    if (!report) {
        printf("Threat Report: (null)\n");
        return;
    }

    printf("\n");
    printf("=== Threat Report ===\n");
    printf("Type:        %s\n", bbb_threat_type_name(report->type));
    printf("Severity:    %s\n", bbb_severity_name(report->severity));
    printf("Action:      %s\n", bbb_action_name(report->action_taken));
    printf("Timestamp:   %lu\n", (unsigned long)report->timestamp);
    printf("Source:      %p\n", report->source_address);
    printf("Size:        %zu bytes\n", report->threat_size);
    printf("Description: %s\n", report->description);
    printf("Quarantined: %s\n", report->quarantined ? "YES" : "NO");

    /* Print hash in hex */
    printf("Hash:        ");
    for (int i = 0; i < 32; i++) {
        printf("%02x", report->threat_hash[i]);
    }
    printf("\n");
    printf("=====================\n");
    printf("\n");
}

//=============================================================================
// Note: Full implementations of validation, signing, memory, and access control
// APIs are in their respective sub-modules:
// - nimcp_bbb_input_gate.c: Input validation (bbb_validate_*, bbb_sanitize_*)
// - nimcp_bbb_code_signing.c: Code signing (bbb_sign_*, bbb_verify_*, bbb_*_trusted_key)
// - nimcp_bbb_memory_boundary.c: Memory protection (bbb_*_memory_*, bbb_*_stack_canary)
// - nimcp_bbb_access_control.c: Access control (bbb_check_access, bbb_register_*, bbb_*_capability)
//=============================================================================

//=============================================================================
// Test Support - Unified Reset Function
//=============================================================================

/* Forward declarations for internal reset functions in sub-modules */
extern void bbb_access_control_reset_internal(void);
extern void bbb_memory_boundary_reset_internal(void);
extern void bbb_input_gate_reset_internal(void);
extern void bbb_code_signing_reset_internal(void);

/**
 * @brief Reset all BBB subsystem state for testing
 *
 * WHAT: Clear all registered subjects, objects, memory regions, signing keys, etc.
 * WHY:  Enable test isolation by resetting ALL BBB subsystem state between tests
 * HOW:  Call internal reset functions for each subsystem
 *
 * SUBSYSTEMS RESET:
 * - Access Control: Clears registered subjects, objects, counters
 * - Memory Boundary: Clears registered memory regions
 * - Input Gate: Resets input gate state
 * - Code Signing: Clears signing key and key store
 *
 * NOTE: This is the canonical test reset function. All BBB tests should call
 *       this instead of individual *_reset_internal() functions to ensure
 *       complete state isolation.
 */
void bbb_reset_test_state(void)
{
    bbb_access_control_reset_internal();
    bbb_memory_boundary_reset_internal();
    bbb_input_gate_reset_internal();
    bbb_code_signing_reset_internal();
}
