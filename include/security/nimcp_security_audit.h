/**
 * @file nimcp_security_audit.h
 * @brief Security Audit Logging and Reporting
 *
 * WHAT: Provides comprehensive audit logging for security events,
 *       policy decisions, and compliance reporting.
 *
 * WHY:  Security auditing is essential for:
 *       - Forensic analysis after incidents
 *       - Compliance with security policies
 *       - Detecting patterns of attack
 *       - Accountability and non-repudiation
 *
 * HOW:  Maintains tamper-evident audit logs with:
 *       - Cryptographic chaining (hash chains)
 *       - Timestamping with monotonic ordering
 *       - Structured event records
 *       - Report generation
 *
 * Part of Phase SC-1: Security Coverage Framework (Tier 0.7)
 */

#ifndef NIMCP_SECURITY_AUDIT_H
#define NIMCP_SECURITY_AUDIT_H

#include "security/nimcp_security.h"
#include "utils/validation/nimcp_common.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Maximum audit log entries in memory */
#define NIMCP_AUDIT_MAX_ENTRIES 10000

/** Maximum event message length */
#define NIMCP_AUDIT_MAX_MSG_LEN 512

/** Maximum source identifier length */
#define NIMCP_AUDIT_MAX_SOURCE_LEN 64

/** Hash size for chain integrity */
#define NIMCP_AUDIT_HASH_SIZE 32

/** Default log rotation size (bytes) */
#define NIMCP_AUDIT_DEFAULT_ROTATION_SIZE (10 * 1024 * 1024)

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Audit event categories
 */
typedef enum {
    NIMCP_AUDIT_CAT_ACCESS = 0,      /**< Access control events */
    NIMCP_AUDIT_CAT_AUTHENTICATION,   /**< Authentication events */
    NIMCP_AUDIT_CAT_AUTHORIZATION,    /**< Authorization decisions */
    NIMCP_AUDIT_CAT_INTEGRITY,        /**< Integrity violations */
    NIMCP_AUDIT_CAT_CONFIGURATION,    /**< Configuration changes */
    NIMCP_AUDIT_CAT_POLICY,           /**< Policy enforcement */
    NIMCP_AUDIT_CAT_THREAT,           /**< Threat detection */
    NIMCP_AUDIT_CAT_SYSTEM,           /**< System events */
    NIMCP_AUDIT_CAT_NEURAL,           /**< Neural subsystem events */
    NIMCP_AUDIT_CAT_DIRECTIVE         /**< Core directive events */
} nimcp_audit_category_t;

/**
 * @brief Audit event severity levels
 */
typedef enum {
    NIMCP_AUDIT_SEV_DEBUG = 0,       /**< Debug information */
    NIMCP_AUDIT_SEV_INFO,            /**< Informational */
    NIMCP_AUDIT_SEV_NOTICE,          /**< Normal but significant */
    NIMCP_AUDIT_SEV_WARNING,         /**< Warning conditions */
    NIMCP_AUDIT_SEV_ERROR,           /**< Error conditions */
    NIMCP_AUDIT_SEV_CRITICAL,        /**< Critical conditions */
    NIMCP_AUDIT_SEV_ALERT,           /**< Immediate action needed */
    NIMCP_AUDIT_SEV_EMERGENCY        /**< System unusable */
} nimcp_audit_severity_t;

/**
 * @brief Audit event outcomes
 */
typedef enum {
    NIMCP_AUDIT_OUTCOME_SUCCESS = 0, /**< Operation succeeded */
    NIMCP_AUDIT_OUTCOME_FAILURE,     /**< Operation failed */
    NIMCP_AUDIT_OUTCOME_DENIED,      /**< Access denied */
    NIMCP_AUDIT_OUTCOME_ERROR,       /**< Error occurred */
    NIMCP_AUDIT_OUTCOME_UNKNOWN      /**< Outcome unknown */
} nimcp_audit_outcome_t;

/**
 * @brief Log output destinations
 */
typedef enum {
    NIMCP_AUDIT_DEST_MEMORY = 0x01,  /**< In-memory ring buffer */
    NIMCP_AUDIT_DEST_FILE = 0x02,    /**< File output */
    NIMCP_AUDIT_DEST_SYSLOG = 0x04,  /**< System log */
    NIMCP_AUDIT_DEST_CALLBACK = 0x08 /**< Custom callback */
} nimcp_audit_destination_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Audit event record
 */
typedef struct {
    uint64_t sequence;               /**< Monotonic sequence number */
    uint64_t timestamp;              /**< Event timestamp (ns) */
    nimcp_audit_category_t category; /**< Event category */
    nimcp_audit_severity_t severity; /**< Severity level */
    nimcp_audit_outcome_t outcome;   /**< Event outcome */
    char source[NIMCP_AUDIT_MAX_SOURCE_LEN];  /**< Event source */
    char message[NIMCP_AUDIT_MAX_MSG_LEN];    /**< Event message */
    uint32_t subject_id;             /**< Subject (who) */
    uint32_t object_id;              /**< Object (what) */
    uint32_t action_id;              /**< Action performed */
    void* context_data;              /**< Additional context */
    size_t context_size;             /**< Context data size */
    uint8_t prev_hash[NIMCP_AUDIT_HASH_SIZE]; /**< Previous entry hash */
    uint8_t hash[NIMCP_AUDIT_HASH_SIZE];      /**< This entry hash */
} nimcp_audit_event_t;

/**
 * @brief Audit configuration
 */
typedef struct {
    uint32_t destinations;           /**< Bitmask of destinations */
    const char* log_file_path;       /**< File path for file output */
    size_t rotation_size;            /**< Log rotation size */
    uint32_t max_memory_entries;     /**< Max entries in memory */
    nimcp_audit_severity_t min_severity; /**< Minimum severity to log */
    bool enable_chain_verification;  /**< Enable hash chain */
    bool enable_timestamps;          /**< Include timestamps */
    bool enable_stack_traces;        /**< Include stack traces */
    bool synchronous_write;          /**< Sync writes immediately */
} nimcp_audit_config_t;

/**
 * @brief Audit statistics
 */
typedef struct {
    uint64_t total_events;           /**< Total events logged */
    uint64_t events_by_category[10]; /**< Events per category */
    uint64_t events_by_severity[8];  /**< Events per severity */
    uint64_t chain_verifications;    /**< Chain verifications */
    uint64_t chain_failures;         /**< Chain verification failures */
    uint64_t dropped_events;         /**< Events dropped (overflow) */
    uint64_t file_rotations;         /**< Log file rotations */
    uint64_t bytes_written;          /**< Total bytes written */
} nimcp_audit_stats_t;

/**
 * @brief Query filter for searching audit logs
 */
typedef struct {
    uint64_t start_time;             /**< Start timestamp filter */
    uint64_t end_time;               /**< End timestamp filter */
    nimcp_audit_category_t category; /**< Category filter (-1 = all) */
    nimcp_audit_severity_t min_severity; /**< Min severity filter */
    uint32_t subject_id;             /**< Subject filter (0 = all) */
    const char* source_pattern;      /**< Source pattern (NULL = all) */
    const char* message_pattern;     /**< Message pattern (NULL = all) */
    uint32_t max_results;            /**< Maximum results to return */
} nimcp_audit_query_t;

/**
 * @brief Audit callback type
 */
typedef void (*nimcp_audit_callback_t)(
    const nimcp_audit_event_t* event,
    void* user_data
);

/**
 * @brief Audit context (opaque handle)
 */
typedef struct nimcp_audit_log nimcp_audit_log_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create audit log system
 *
 * @return Audit log context or NULL on failure
 */
nimcp_audit_log_t* nimcp_audit_create(void);

/**
 * @brief Initialize audit log with configuration
 *
 * @param audit Audit log context
 * @param config Configuration parameters
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_audit_init(
    nimcp_audit_log_t* audit,
    const nimcp_audit_config_t* config
);

/**
 * @brief Destroy audit log system
 *
 * @param audit Audit log context
 */
void nimcp_audit_destroy(nimcp_audit_log_t* audit);

/**
 * @brief Get default configuration
 *
 * @return Default configuration
 */
nimcp_audit_config_t nimcp_audit_default_config(void);

//=============================================================================
// Event Logging Functions
//=============================================================================

/**
 * @brief Log an audit event
 *
 * @param audit Audit log context
 * @param category Event category
 * @param severity Severity level
 * @param outcome Event outcome
 * @param source Event source identifier
 * @param message Event message
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_audit_log(
    nimcp_audit_log_t* audit,
    nimcp_audit_category_t category,
    nimcp_audit_severity_t severity,
    nimcp_audit_outcome_t outcome,
    const char* source,
    const char* message
);

/**
 * @brief Log event with formatted message
 *
 * @param audit Audit log context
 * @param category Event category
 * @param severity Severity level
 * @param outcome Event outcome
 * @param source Event source
 * @param format Printf-style format string
 * @param ... Format arguments
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_audit_logf(
    nimcp_audit_log_t* audit,
    nimcp_audit_category_t category,
    nimcp_audit_severity_t severity,
    nimcp_audit_outcome_t outcome,
    const char* source,
    const char* format,
    ...
);

/**
 * @brief Log event with full details
 *
 * @param audit Audit log context
 * @param event Event record to log
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_audit_log_event(
    nimcp_audit_log_t* audit,
    const nimcp_audit_event_t* event
);

/**
 * @brief Log access control decision
 *
 * @param audit Audit log context
 * @param subject_id Subject making request
 * @param object_id Object being accessed
 * @param action Requested action
 * @param granted Was access granted
 * @param reason Reason for decision
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_audit_log_access(
    nimcp_audit_log_t* audit,
    uint32_t subject_id,
    uint32_t object_id,
    const char* action,
    bool granted,
    const char* reason
);

/**
 * @brief Log security threat detection
 *
 * @param audit Audit log context
 * @param threat_level Threat severity
 * @param threat_type Type of threat
 * @param details Threat details
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_audit_log_threat(
    nimcp_audit_log_t* audit,
    nimcp_threat_level_t threat_level,
    const char* threat_type,
    const char* details
);

/**
 * @brief Log configuration change
 *
 * @param audit Audit log context
 * @param component Changed component
 * @param parameter Changed parameter
 * @param old_value Previous value
 * @param new_value New value
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_audit_log_config_change(
    nimcp_audit_log_t* audit,
    const char* component,
    const char* parameter,
    const char* old_value,
    const char* new_value
);

//=============================================================================
// Query and Retrieval
//=============================================================================

/**
 * @brief Query audit log
 *
 * @param audit Audit log context
 * @param query Query filter
 * @param results Output: array of matching events (caller frees)
 * @param count Output: number of results
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_audit_query(
    nimcp_audit_log_t* audit,
    const nimcp_audit_query_t* query,
    nimcp_audit_event_t** results,
    uint32_t* count
);

/**
 * @brief Get recent events
 *
 * @param audit Audit log context
 * @param count Number of events to retrieve
 * @param events Output: array of events (caller frees)
 * @param actual Output: actual count returned
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_audit_get_recent(
    nimcp_audit_log_t* audit,
    uint32_t count,
    nimcp_audit_event_t** events,
    uint32_t* actual
);

/**
 * @brief Get event by sequence number
 *
 * @param audit Audit log context
 * @param sequence Sequence number
 * @param event Output: event record
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_audit_get_by_sequence(
    nimcp_audit_log_t* audit,
    uint64_t sequence,
    nimcp_audit_event_t* event
);

//=============================================================================
// Chain Verification
//=============================================================================

/**
 * @brief Verify audit log integrity
 *
 * Checks hash chain for tampering.
 *
 * @param audit Audit log context
 * @param first_broken Output: first broken sequence (if any)
 * @return true if chain intact
 */
bool nimcp_audit_verify_chain(
    nimcp_audit_log_t* audit,
    uint64_t* first_broken
);

/**
 * @brief Verify specific event
 *
 * @param audit Audit log context
 * @param sequence Sequence to verify
 * @return true if event valid
 */
bool nimcp_audit_verify_event(
    nimcp_audit_log_t* audit,
    uint64_t sequence
);

//=============================================================================
// Reporting
//=============================================================================

/**
 * @brief Generate audit report
 *
 * @param audit Audit log context
 * @param query Query filter for report scope
 * @param buffer Output buffer
 * @param size Buffer size
 * @return Bytes written
 */
int nimcp_audit_generate_report(
    nimcp_audit_log_t* audit,
    const nimcp_audit_query_t* query,
    char* buffer,
    size_t size
);

/**
 * @brief Generate security summary
 *
 * @param audit Audit log context
 * @param buffer Output buffer
 * @param size Buffer size
 * @return Bytes written
 */
int nimcp_audit_security_summary(
    nimcp_audit_log_t* audit,
    char* buffer,
    size_t size
);

/**
 * @brief Export audit log to file
 *
 * @param audit Audit log context
 * @param filepath Export file path
 * @param query Optional filter (NULL = all)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_audit_export(
    nimcp_audit_log_t* audit,
    const char* filepath,
    const nimcp_audit_query_t* query
);

//=============================================================================
// Log Management
//=============================================================================

/**
 * @brief Rotate log file
 *
 * @param audit Audit log context
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_audit_rotate(nimcp_audit_log_t* audit);

/**
 * @brief Flush pending writes
 *
 * @param audit Audit log context
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_audit_flush(nimcp_audit_log_t* audit);

/**
 * @brief Clear in-memory log
 *
 * @param audit Audit log context
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_audit_clear(nimcp_audit_log_t* audit);

/**
 * @brief Set audit callback
 *
 * @param audit Audit log context
 * @param callback Callback function
 * @param user_data User context
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_audit_set_callback(
    nimcp_audit_log_t* audit,
    nimcp_audit_callback_t callback,
    void* user_data
);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Get audit statistics
 *
 * @param audit Audit log context
 * @param stats Output: statistics
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_audit_get_stats(
    nimcp_audit_log_t* audit,
    nimcp_audit_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param audit Audit log context
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_audit_reset_stats(nimcp_audit_log_t* audit);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get category name
 *
 * @param category Category
 * @return Category name string
 */
const char* nimcp_audit_category_name(nimcp_audit_category_t category);

/**
 * @brief Get severity name
 *
 * @param severity Severity level
 * @return Severity name string
 */
const char* nimcp_audit_severity_name(nimcp_audit_severity_t severity);

/**
 * @brief Get outcome name
 *
 * @param outcome Outcome
 * @return Outcome name string
 */
const char* nimcp_audit_outcome_name(nimcp_audit_outcome_t outcome);

/**
 * @brief Format event as string
 *
 * @param event Event to format
 * @param buffer Output buffer
 * @param size Buffer size
 * @return Bytes written
 */
int nimcp_audit_format_event(
    const nimcp_audit_event_t* event,
    char* buffer,
    size_t size
);

//=============================================================================
// Convenience Macros
//=============================================================================

/**
 * @brief Log debug event
 */
#define NIMCP_AUDIT_DEBUG(audit, source, msg) \
    nimcp_audit_log(audit, NIMCP_AUDIT_CAT_SYSTEM, \
                    NIMCP_AUDIT_SEV_DEBUG, NIMCP_AUDIT_OUTCOME_SUCCESS, \
                    source, msg)

/**
 * @brief Log info event
 */
#define NIMCP_AUDIT_INFO(audit, source, msg) \
    nimcp_audit_log(audit, NIMCP_AUDIT_CAT_SYSTEM, \
                    NIMCP_AUDIT_SEV_INFO, NIMCP_AUDIT_OUTCOME_SUCCESS, \
                    source, msg)

/**
 * @brief Log warning event
 */
#define NIMCP_AUDIT_WARN(audit, source, msg) \
    nimcp_audit_log(audit, NIMCP_AUDIT_CAT_SYSTEM, \
                    NIMCP_AUDIT_SEV_WARNING, NIMCP_AUDIT_OUTCOME_UNKNOWN, \
                    source, msg)

/**
 * @brief Log error event
 */
#define NIMCP_AUDIT_ERROR(audit, source, msg) \
    nimcp_audit_log(audit, NIMCP_AUDIT_CAT_SYSTEM, \
                    NIMCP_AUDIT_SEV_ERROR, NIMCP_AUDIT_OUTCOME_FAILURE, \
                    source, msg)

/**
 * @brief Log critical event
 */
#define NIMCP_AUDIT_CRITICAL(audit, source, msg) \
    nimcp_audit_log(audit, NIMCP_AUDIT_CAT_SYSTEM, \
                    NIMCP_AUDIT_SEV_CRITICAL, NIMCP_AUDIT_OUTCOME_FAILURE, \
                    source, msg)

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_SECURITY_AUDIT_H
