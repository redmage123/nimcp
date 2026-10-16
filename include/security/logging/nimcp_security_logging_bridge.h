/**
 * @file nimcp_security_logging_bridge.h
 * @brief Security-Logging Bridge for Comprehensive Audit Trails
 * @version 1.0.0
 * @date 2026-01-09
 *
 * WHAT: Bidirectional bridge connecting security systems with logging infrastructure
 * WHY:  Enable comprehensive audit trails, threat pattern detection from logs,
 *       and tamper-proof security event recording
 * HOW:  Security events flow to logging; log analysis feeds back threat intelligence
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 * Modeled on the immune system's memory and logging mechanisms:
 * - Dendritic cells (security) present antigens to memory systems (logging)
 * - Memory B cells retain patterns of past threats (log analysis)
 * - Cytokine signaling (real-time alerts) coordinates immediate responses
 * - Complement system (audit trail) marks and tracks all foreign entities
 *
 * ARCHITECTURE:
 * ==================================================================================
 * ```
 * +===========================================================================+
 * |          SECURITY-LOGGING BRIDGE (Audit & Pattern Detection)              |
 * +===========================================================================+
 * |                                                                           |
 * |   SECURITY SYSTEMS                          LOGGING SYSTEMS               |
 * |   +-------------------+                    +-------------------+          |
 * |   | Blood-Brain       |                    | NIMCP Logging     |          |
 * |   | Barrier (BBB)     |----+          +--->| System            |          |
 * |   +-------------------+    |          |    +-------------------+          |
 * |   +-------------------+    |          |    +-------------------+          |
 * |   | Anomaly Detector  |----+    BI    +--->| Encrypted Audit   |          |
 * |   +-------------------+    |    DI    |    +-------------------+          |
 * |   +-------------------+    |    RE    |    +-------------------+          |
 * |   | Rate Limiter      |----+    CT    +--->| PR Logging        |          |
 * |   +-------------------+    |    IO    |    | Bridge            |          |
 * |   +-------------------+    |    NA    |    +-------------------+          |
 * |   | Policy Engine     |----+----L-----|                                   |
 * |   +-------------------+         |                                         |
 * |                                 v                                         |
 * |                    +------------------------+                             |
 * |                    | Pattern Analysis       |                             |
 * |                    | - Threat extraction    |                             |
 * |                    | - Anomaly detection    |                             |
 * |                    | - Trend analysis       |                             |
 * |                    +------------------------+                             |
 * |                                                                           |
 * +===========================================================================+
 * ```
 *
 * DATA FLOW:
 * ==================================================================================
 *
 * Security -> Logging (Event Recording):
 * - Threat detection events with full context
 * - Policy enforcement decisions and outcomes
 * - Access control events (allow/deny)
 * - BBB perimeter defense events
 * - Rate limiting triggers
 * - Cryptographic operation records
 *
 * Logging -> Security (Pattern Feedback):
 * - Threat pattern extraction from historical logs
 * - Anomaly detection based on log patterns
 * - Attack sequence identification
 * - Behavioral baseline deviation alerts
 * - Correlation of distributed attacks
 *
 * SECURITY EVENT CATEGORIES:
 * ==================================================================================
 * - THREAT:     Detected threats (malware, injection, overflow)
 * - ACCESS:     Access control events (auth, authz, permissions)
 * - POLICY:     Policy enforcement (rules applied, violations)
 * - AUDIT:      General audit trail (who did what when)
 * - BBB:        Blood-brain barrier events (perimeter defense)
 * - ANOMALY:    Anomaly detection events
 * - CRYPTO:     Cryptographic operations (encrypt, decrypt, sign)
 * - RATE_LIMIT: Rate limiting events (throttle, block)
 *
 * FEATURES:
 * ==================================================================================
 * - Structured JSON logging for machine analysis
 * - Integration with encrypted audit for tamper-proof records
 * - Real-time log streaming via callbacks
 * - Log rotation with configurable retention policies
 * - Pattern extraction for threat intelligence
 * - Anomaly detection from log patterns
 * - Bio-async integration for distributed logging
 *
 * THREAD SAFETY:
 * ==================================================================================
 * - All public functions are thread-safe via internal mutex
 * - Lock-free fast path for high-frequency logging
 * - Atomic counters for statistics
 *
 * PERFORMANCE:
 * ==================================================================================
 * - Log entry: O(1) with lock-free ring buffer
 * - Pattern analysis: O(n) where n = analysis window size
 * - Export: O(n) where n = number of entries
 * - Memory: ~8KB base + configurable buffer (default ~4MB)
 *
 * @author NIMCP Security Team
 */

#ifndef NIMCP_SECURITY_LOGGING_BRIDGE_H
#define NIMCP_SECURITY_LOGGING_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Bridge base infrastructure - MUST come first for struct embedding */
#include "utils/bridge/nimcp_bridge_base.h"

/* Security system dependencies */
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "security/nimcp_anomaly_detector.h"
#include "security/nimcp_rate_limiter.h"
#include "security/nimcp_encrypted_audit.h"

/* Logging system dependencies */
#include "utils/logging/nimcp_logging.h"

/* Async integration */
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"

/* Common utilities */
#include "utils/validation/nimcp_common.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * CONSTANTS
 *============================================================================*/

/** @brief Magic number for validation */
#define SECURITY_LOGGING_BRIDGE_MAGIC       0x53454C47  /* 'SELG' */

/** @brief Bridge version */
#define SECURITY_LOGGING_BRIDGE_VERSION     1

/** @brief Default ring buffer size (entries) */
#define SECURITY_LOG_DEFAULT_BUFFER_SIZE    8192

/** @brief Maximum security log message length */
#define SECURITY_LOG_MAX_MESSAGE_LEN        512

/** @brief Maximum event details length */
#define SECURITY_LOG_MAX_DETAILS_LEN        1024

/** @brief Maximum source identifier length */
#define SECURITY_LOG_MAX_SOURCE_LEN         128

/** @brief Maximum pattern signature length */
#define SECURITY_LOG_MAX_PATTERN_LEN        256

/** @brief Maximum concurrent stream callbacks */
#define SECURITY_LOG_MAX_STREAM_CALLBACKS   8

/** @brief Default retention period (days) */
#define SECURITY_LOG_DEFAULT_RETENTION_DAYS 90

/** @brief Default analysis window (entries) */
#define SECURITY_LOG_DEFAULT_ANALYSIS_WINDOW 1000

/** @brief Module ID for bio-async registration */
#define BIO_MODULE_SECURITY_LOGGING         0x00008500

/*=============================================================================
 * SECURITY LOG CATEGORIES
 *============================================================================*/

/**
 * @brief Security event categories for logging
 *
 * WHAT: Classification of security events by type
 * WHY:  Enable filtering, routing, and analysis by category
 */
typedef enum {
    SECURITY_LOG_CAT_THREAT = 0,        /**< Threat detection events */
    SECURITY_LOG_CAT_ACCESS,            /**< Access control events */
    SECURITY_LOG_CAT_POLICY,            /**< Policy enforcement events */
    SECURITY_LOG_CAT_AUDIT,             /**< General audit trail events */
    SECURITY_LOG_CAT_BBB,               /**< Blood-brain barrier events */
    SECURITY_LOG_CAT_ANOMALY,           /**< Anomaly detection events */
    SECURITY_LOG_CAT_CRYPTO,            /**< Cryptographic operations */
    SECURITY_LOG_CAT_RATE_LIMIT,        /**< Rate limiting events */
    SECURITY_LOG_CAT_COUNT              /**< Number of categories */
} security_log_category_t;

/**
 * @brief Security event severity levels
 *
 * WHAT: Severity classification aligned with threat levels
 * WHY:  Prioritize response and filtering
 */
typedef enum {
    SECURITY_LOG_SEV_DEBUG = 0,         /**< Debug information */
    SECURITY_LOG_SEV_INFO,              /**< Informational */
    SECURITY_LOG_SEV_NOTICE,            /**< Normal but significant */
    SECURITY_LOG_SEV_WARNING,           /**< Warning condition */
    SECURITY_LOG_SEV_ERROR,             /**< Error condition */
    SECURITY_LOG_SEV_CRITICAL,          /**< Critical security event */
    SECURITY_LOG_SEV_ALERT,             /**< Immediate action required */
    SECURITY_LOG_SEV_EMERGENCY,         /**< System unusable */
    SECURITY_LOG_SEV_COUNT              /**< Number of severity levels */
} security_log_severity_t;

/**
 * @brief Security event action outcomes
 *
 * WHAT: Result of security event handling
 * WHY:  Track what action was taken for each event
 */
typedef enum {
    SECURITY_LOG_ACTION_NONE = 0,       /**< No action taken (informational) */
    SECURITY_LOG_ACTION_ALLOW,          /**< Access/operation allowed */
    SECURITY_LOG_ACTION_DENY,           /**< Access/operation denied */
    SECURITY_LOG_ACTION_BLOCK,          /**< Source blocked */
    SECURITY_LOG_ACTION_QUARANTINE,     /**< Threat quarantined */
    SECURITY_LOG_ACTION_ALERT,          /**< Alert generated */
    SECURITY_LOG_ACTION_TERMINATE,      /**< Process terminated */
    SECURITY_LOG_ACTION_LOCKDOWN,       /**< System lockdown initiated */
    SECURITY_LOG_ACTION_COUNT           /**< Number of actions */
} security_log_action_t;

/**
 * @brief Log output format types
 */
typedef enum {
    SECURITY_LOG_FORMAT_TEXT = 0,       /**< Human-readable text */
    SECURITY_LOG_FORMAT_JSON,           /**< Structured JSON */
    SECURITY_LOG_FORMAT_SYSLOG,         /**< Syslog format */
    SECURITY_LOG_FORMAT_CEF,            /**< Common Event Format */
    SECURITY_LOG_FORMAT_BINARY,         /**< Compact binary */
    SECURITY_LOG_FORMAT_COUNT           /**< Number of formats */
} security_log_format_t;

/**
 * @brief Pattern detection types
 */
typedef enum {
    SECURITY_PATTERN_NONE = 0,          /**< No pattern detected */
    SECURITY_PATTERN_SCAN,              /**< Port/vulnerability scan */
    SECURITY_PATTERN_BRUTE_FORCE,       /**< Brute force attack */
    SECURITY_PATTERN_DOS,               /**< Denial of service */
    SECURITY_PATTERN_INJECTION,         /**< Injection attack sequence */
    SECURITY_PATTERN_EXFILTRATION,      /**< Data exfiltration */
    SECURITY_PATTERN_LATERAL_MOVE,      /**< Lateral movement */
    SECURITY_PATTERN_PRIVILEGE_ESC,     /**< Privilege escalation */
    SECURITY_PATTERN_CUSTOM,            /**< Custom defined pattern */
    SECURITY_PATTERN_COUNT              /**< Number of pattern types */
} security_pattern_type_t;

/*=============================================================================
 * SECURITY LOG ENTRY STRUCTURES
 *============================================================================*/

/**
 * @brief Security log entry
 *
 * WHAT: Complete security event record
 * WHY:  Comprehensive audit trail with full context
 */
typedef struct {
    /* Identification */
    uint64_t entry_id;                  /**< Unique entry identifier */
    uint64_t timestamp_ns;              /**< Nanosecond timestamp */
    uint32_t sequence_number;           /**< Monotonic sequence */

    /* Classification */
    security_log_category_t category;   /**< Event category */
    security_log_severity_t severity;   /**< Severity level */
    security_log_action_t action;       /**< Action taken */

    /* Threat context */
    nimcp_threat_level_t threat_level;  /**< NIMCP threat level mapping */
    bbb_threat_type_t bbb_threat;       /**< BBB threat type (if applicable) */
    security_pattern_type_t pattern;    /**< Detected attack pattern */

    /* Source information */
    char source_module[SECURITY_LOG_MAX_SOURCE_LEN];  /**< Originating module */
    char source_id[SECURITY_LOG_MAX_SOURCE_LEN];      /**< Source identifier */
    uint32_t source_thread_id;          /**< Thread ID */

    /* Event content */
    char message[SECURITY_LOG_MAX_MESSAGE_LEN];       /**< Event message */
    char details[SECURITY_LOG_MAX_DETAILS_LEN];       /**< Extended details */

    /* Metrics */
    float confidence_score;             /**< Detection confidence [0.0, 1.0] */
    float anomaly_score;                /**< Anomaly score (if applicable) */
    uint64_t processing_time_ns;        /**< Time to process event */

    /* Correlation */
    uint64_t correlation_id;            /**< For grouping related events */
    uint64_t parent_entry_id;           /**< Parent event (if part of chain) */
} security_log_entry_t;

/**
 * @brief Threat pattern signature
 *
 * WHAT: Extracted attack pattern from log analysis
 * WHY:  Feed threat intelligence back to security systems
 */
typedef struct {
    /* Identification */
    uint64_t pattern_id;                /**< Unique pattern ID */
    security_pattern_type_t type;       /**< Pattern classification */
    uint64_t first_seen_ns;             /**< First occurrence */
    uint64_t last_seen_ns;              /**< Most recent occurrence */

    /* Pattern details */
    char signature[SECURITY_LOG_MAX_PATTERN_LEN];     /**< Pattern signature */
    char description[SECURITY_LOG_MAX_MESSAGE_LEN];   /**< Human description */

    /* Statistics */
    uint64_t occurrence_count;          /**< Times pattern seen */
    float severity_avg;                 /**< Average severity */
    float confidence;                   /**< Pattern confidence */

    /* Associated events */
    uint64_t sample_entry_ids[8];       /**< Sample triggering entries */
    size_t sample_count;                /**< Number of samples */
} security_threat_pattern_t;

/*=============================================================================
 * LOG STREAMING CALLBACK
 *============================================================================*/

/**
 * @brief Real-time log stream callback
 *
 * WHAT: Callback invoked for each security log entry
 * WHY:  Enable real-time monitoring and alerting
 *
 * @param entry     The log entry
 * @param user_data User context
 * @return true to continue streaming, false to stop
 */
typedef bool (*security_log_stream_callback_t)(
    const security_log_entry_t* entry,
    void* user_data
);

/**
 * @brief Pattern detection callback
 *
 * WHAT: Callback when attack pattern is detected
 * WHY:  Enable real-time threat intelligence updates
 *
 * @param pattern   Detected pattern
 * @param user_data User context
 */
typedef void (*security_pattern_callback_t)(
    const security_threat_pattern_t* pattern,
    void* user_data
);

/*=============================================================================
 * CONFIGURATION STRUCTURES
 *============================================================================*/

/**
 * @brief Log rotation configuration
 */
typedef struct {
    bool enabled;                       /**< Enable log rotation */
    size_t max_file_size_mb;            /**< Max file size before rotation */
    uint32_t max_rotated_files;         /**< Max rotated files to keep */
    uint32_t rotation_interval_hours;   /**< Time-based rotation (0 = size only) */
    bool compress_rotated;              /**< Compress rotated files */
} security_log_rotation_config_t;

/**
 * @brief Log retention policy
 */
typedef struct {
    uint32_t retention_days;            /**< Days to retain logs */
    bool archive_before_delete;         /**< Archive before deletion */
    char archive_path[256];             /**< Archive destination path */
    security_log_severity_t min_archive_severity; /**< Min severity to archive */
} security_log_retention_config_t;

/**
 * @brief Pattern analysis configuration
 */
typedef struct {
    bool enabled;                       /**< Enable pattern analysis */
    uint32_t analysis_window_size;      /**< Entries to analyze */
    uint32_t analysis_interval_ms;      /**< Analysis frequency */
    float min_pattern_confidence;       /**< Minimum confidence to report */
    uint32_t min_occurrences;           /**< Minimum occurrences for pattern */
    bool feed_to_anomaly_detector;      /**< Feed patterns to anomaly detector */
} security_log_pattern_config_t;

/**
 * @brief Security logging bridge configuration
 */
typedef struct {
    /* Buffer settings */
    size_t buffer_capacity;             /**< Ring buffer size (entries) */
    bool overwrite_on_full;             /**< Overwrite oldest when full */

    /* Filtering */
    security_log_severity_t min_severity;  /**< Minimum severity to log */
    uint32_t enabled_categories;        /**< Bitmask of enabled categories */

    /* Output settings */
    bool log_to_console;                /**< Echo to console */
    bool log_to_file;                   /**< Write to file */
    char log_file_path[256];            /**< Log file path */
    security_log_format_t format;       /**< Output format */

    /* Encrypted audit integration */
    bool enable_encrypted_audit;        /**< Send to encrypted audit */
    nimcp_encrypted_audit_t encrypted_audit; /**< Encrypted audit handle */

    /* NIMCP logging integration */
    bool enable_nimcp_logging;          /**< Send to NIMCP logger */
    nimcp_logger_t nimcp_logger;        /**< NIMCP logger handle */

    /* Rotation and retention */
    security_log_rotation_config_t rotation;   /**< Rotation settings */
    security_log_retention_config_t retention; /**< Retention settings */

    /* Pattern analysis */
    security_log_pattern_config_t pattern_analysis; /**< Analysis settings */

    /* Bio-async integration */
    bool enable_bio_async;              /**< Enable bio-async messaging */

    /* Performance */
    bool enable_timestamps;             /**< Include nanosecond timestamps */
    bool enable_metrics;                /**< Include processing metrics */
} security_logging_bridge_config_t;

/*=============================================================================
 * BRIDGE STATE STRUCTURES
 *============================================================================*/

/**
 * @brief Security logging bridge state
 */
typedef struct {
    bool active;                        /**< Bridge is active */
    bool logging_enabled;               /**< Logging currently enabled */
    bool pattern_analysis_running;      /**< Pattern analysis active */
    uint64_t last_entry_time_ns;        /**< Time of last entry */
    uint64_t last_analysis_time_ns;     /**< Time of last pattern analysis */
    uint32_t active_stream_callbacks;   /**< Number of active stream callbacks */
    uint32_t pending_entries;           /**< Entries pending write */
} security_logging_bridge_state_t;

/**
 * @brief Security logging bridge statistics
 */
typedef struct {
    /* Entry counts */
    uint64_t total_entries;             /**< Total entries logged */
    uint64_t entries_by_category[SECURITY_LOG_CAT_COUNT]; /**< Per-category */
    uint64_t entries_by_severity[SECURITY_LOG_SEV_COUNT]; /**< Per-severity */
    uint64_t entries_dropped;           /**< Dropped due to buffer full */
    uint64_t entries_filtered;          /**< Filtered by severity/category */

    /* Pattern analysis */
    uint64_t patterns_detected;         /**< Attack patterns detected */
    uint64_t pattern_callbacks_invoked; /**< Pattern callbacks called */
    uint64_t analysis_runs;             /**< Pattern analysis executions */

    /* Streaming */
    uint64_t stream_callbacks_invoked;  /**< Stream callback invocations */
    uint64_t stream_errors;             /**< Stream callback errors */

    /* Integration */
    uint64_t encrypted_audit_writes;    /**< Writes to encrypted audit */
    uint64_t nimcp_logger_writes;       /**< Writes to NIMCP logger */
    uint64_t file_writes;               /**< Writes to file */
    uint64_t file_rotations;            /**< File rotation events */

    /* Performance */
    float avg_log_time_ns;              /**< Average time to log entry */
    float max_log_time_ns;              /**< Maximum log time */
    float avg_analysis_time_ns;         /**< Average analysis time */

    /* Buffer status */
    size_t current_buffer_size;         /**< Current entries in buffer */
    size_t buffer_capacity;             /**< Buffer capacity */
    float buffer_utilization;           /**< Percentage full */
    uint64_t buffer_overwrites;         /**< Times buffer wrapped */
} security_logging_bridge_stats_t;

/**
 * @brief Effects from security system to logging
 *
 * WHAT: Security events to be logged
 * WHY:  Track bidirectional data flow
 */
typedef struct {
    uint64_t events_pending;            /**< Events waiting to be logged */
    uint64_t threat_events;             /**< Threat events this cycle */
    uint64_t policy_events;             /**< Policy events this cycle */
    uint64_t access_events;             /**< Access events this cycle */
    security_log_severity_t max_severity; /**< Highest severity this cycle */
} security_to_logging_effects_t;

/**
 * @brief Effects from logging to security (pattern feedback)
 *
 * WHAT: Patterns and anomalies detected from logs
 * WHY:  Feed threat intelligence back to security
 */
typedef struct {
    uint32_t patterns_detected;         /**< New patterns detected */
    uint32_t anomalies_detected;        /**< Anomalies from log analysis */
    float threat_trend_score;           /**< Overall threat trend [0.0, 1.0] */
    bool escalation_recommended;        /**< Recommend security escalation */
    security_pattern_type_t primary_pattern; /**< Most significant pattern */
} logging_to_security_effects_t;

/*=============================================================================
 * BRIDGE STRUCTURE
 *============================================================================*/

/**
 * @brief Security logging bridge
 *
 * WHAT: Main bridge structure connecting security and logging systems
 * WHY:  Centralized audit trail and pattern detection
 *
 * NOTE: bridge_base_t MUST be the first member for proper casting
 */
typedef struct {
    bridge_base_t base;                 /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    security_logging_bridge_config_t config; /**< Bridge configuration */

    /* Connected systems */
    bbb_system_t bbb_system;            /**< Blood-brain barrier system */
    nimcp_anomaly_detector_t anomaly_detector; /**< Anomaly detector */
    nimcp_rate_limiter_t rate_limiter;  /**< Rate limiter */

    /* Bridge effects */
    security_to_logging_effects_t security_effects; /**< Security -> Logging */
    logging_to_security_effects_t logging_effects;  /**< Logging -> Security */

    /* State and statistics */
    security_logging_bridge_state_t state; /**< Current state */
    security_logging_bridge_stats_t stats; /**< Statistics */
} security_logging_bridge_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *============================================================================*/

/**
 * @brief Get default bridge configuration
 *
 * WHAT: Returns sensible defaults for security logging
 * WHY:  Simplifies initialization
 *
 * @return Default configuration structure
 */
int security_logging_default_config(security_logging_bridge_config_t* config);

/**
 * @brief Create security logging bridge
 *
 * WHAT: Allocates and initializes the bridge
 * WHY:  Entry point for security logging integration
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge pointer or NULL on failure
 *
 * COMPLEXITY: O(buffer_capacity)
 * MEMORY: ~8KB + buffer (default ~4MB)
 */
security_logging_bridge_t* security_logging_bridge_create(
    const security_logging_bridge_config_t* config
);

/**
 * @brief Destroy security logging bridge
 *
 * WHAT: Cleanup and free all resources
 * WHY:  Prevent memory leaks
 *
 * @param bridge Bridge to destroy (NULL safe)
 *
 * NOTE: Flushes pending entries before destruction
 */
void security_logging_bridge_destroy(security_logging_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * WHAT: Clear statistics and state, preserve configuration
 * WHY:  Enable reuse without reconnection
 *
 * @param bridge Bridge to reset
 * @return 0 on success, error code on failure
 */
int security_logging_bridge_reset(security_logging_bridge_t* bridge);

/*=============================================================================
 * CONNECTION FUNCTIONS
 *============================================================================*/

/**
 * @brief Connect BBB system
 *
 * @param bridge Security logging bridge
 * @param bbb    Blood-brain barrier system
 * @return 0 on success, error code on failure
 */
int security_logging_connect_bbb(
    security_logging_bridge_t* bridge,
    bbb_system_t bbb
);

/**
 * @brief Connect anomaly detector
 *
 * @param bridge   Security logging bridge
 * @param detector Anomaly detector
 * @return 0 on success, error code on failure
 */
int security_logging_connect_anomaly_detector(
    security_logging_bridge_t* bridge,
    nimcp_anomaly_detector_t detector
);

/**
 * @brief Connect rate limiter
 *
 * @param bridge  Security logging bridge
 * @param limiter Rate limiter
 * @return 0 on success, error code on failure
 */
int security_logging_connect_rate_limiter(
    security_logging_bridge_t* bridge,
    nimcp_rate_limiter_t limiter
);

/**
 * @brief Connect encrypted audit system
 *
 * @param bridge Security logging bridge
 * @param audit  Encrypted audit handle
 * @return 0 on success, error code on failure
 */
int security_logging_connect_encrypted_audit(
    security_logging_bridge_t* bridge,
    nimcp_encrypted_audit_t audit
);

/**
 * @brief Connect NIMCP logger
 *
 * @param bridge Security logging bridge
 * @param logger NIMCP logger handle
 * @return 0 on success, error code on failure
 */
int security_logging_connect_nimcp_logger(
    security_logging_bridge_t* bridge,
    nimcp_logger_t logger
);

/**
 * @brief Disconnect all systems
 *
 * @param bridge Security logging bridge
 * @return 0 on success, error code on failure
 */
int security_logging_disconnect_all(security_logging_bridge_t* bridge);

/**
 * @brief Check if bridge is connected
 *
 * @param bridge Security logging bridge
 * @return true if at least one output connected
 */
bool security_logging_is_connected(const security_logging_bridge_t* bridge);

/*=============================================================================
 * CORE LOGGING FUNCTIONS
 *============================================================================*/

/**
 * @brief Log security event
 *
 * WHAT: Record a security event with full context
 * WHY:  Core audit trail function
 *
 * @param bridge   Security logging bridge
 * @param entry    Event entry to log
 * @return 0 on success, error code on failure
 *
 * COMPLEXITY: O(1) amortized
 * THREAD SAFETY: Thread-safe
 */
int security_logging_log_entry(
    security_logging_bridge_t* bridge,
    const security_log_entry_t* entry
);

/**
 * @brief Log threat detection event
 *
 * @param bridge      Security logging bridge
 * @param threat      Threat level
 * @param bbb_threat  BBB threat type
 * @param source      Source module/ID
 * @param message     Event message
 * @param action      Action taken
 * @return 0 on success, error code on failure
 */
int security_logging_log_threat(
    security_logging_bridge_t* bridge,
    nimcp_threat_level_t threat,
    bbb_threat_type_t bbb_threat,
    const char* source,
    const char* message,
    security_log_action_t action
);

/**
 * @brief Log access control event
 *
 * @param bridge  Security logging bridge
 * @param allowed Whether access was allowed
 * @param subject Subject requesting access
 * @param object  Object being accessed
 * @param message Additional details
 * @return 0 on success, error code on failure
 */
int security_logging_log_access(
    security_logging_bridge_t* bridge,
    bool allowed,
    const char* subject,
    const char* object,
    const char* message
);

/**
 * @brief Log policy enforcement event
 *
 * @param bridge    Security logging bridge
 * @param policy_id Policy identifier
 * @param action    Action taken
 * @param target    Target of policy
 * @param message   Policy details
 * @return 0 on success, error code on failure
 */
int security_logging_log_policy(
    security_logging_bridge_t* bridge,
    const char* policy_id,
    security_log_action_t action,
    const char* target,
    const char* message
);

/**
 * @brief Log BBB event
 *
 * @param bridge  Security logging bridge
 * @param threat  BBB threat type
 * @param severity BBB severity
 * @param action  Action taken
 * @param details Event details
 * @return 0 on success, error code on failure
 */
int security_logging_log_bbb(
    security_logging_bridge_t* bridge,
    bbb_threat_type_t threat,
    bbb_severity_t severity,
    bbb_action_t action,
    const char* details
);

/**
 * @brief Log anomaly detection event
 *
 * @param bridge  Security logging bridge
 * @param result  Anomaly detection result
 * @param input   Input that triggered (can be NULL)
 * @param message Additional details
 * @return 0 on success, error code on failure
 */
int security_logging_log_anomaly(
    security_logging_bridge_t* bridge,
    const nimcp_anomaly_result_t* result,
    const void* input,
    const char* message
);

/**
 * @brief Log rate limiting event
 *
 * @param bridge    Security logging bridge
 * @param client_id Client identifier
 * @param allowed   Whether request was allowed
 * @param stats     Client statistics
 * @return 0 on success, error code on failure
 */
int security_logging_log_rate_limit(
    security_logging_bridge_t* bridge,
    const char* client_id,
    bool allowed,
    const nimcp_client_stats_t* stats
);

/**
 * @brief Log cryptographic operation
 *
 * @param bridge    Security logging bridge
 * @param operation Operation type (encrypt/decrypt/sign/verify)
 * @param success   Whether operation succeeded
 * @param key_id    Key identifier (for audit)
 * @param message   Additional details
 * @return 0 on success, error code on failure
 */
int security_logging_log_crypto(
    security_logging_bridge_t* bridge,
    const char* operation,
    bool success,
    const char* key_id,
    const char* message
);

/**
 * @brief Log general audit event
 *
 * @param bridge   Security logging bridge
 * @param severity Event severity
 * @param source   Event source
 * @param message  Event message
 * @param details  Extended details (can be NULL)
 * @return 0 on success, error code on failure
 */
int security_logging_log_audit(
    security_logging_bridge_t* bridge,
    security_log_severity_t severity,
    const char* source,
    const char* message,
    const char* details
);

/*=============================================================================
 * STREAMING FUNCTIONS
 *============================================================================*/

/**
 * @brief Register stream callback
 *
 * WHAT: Register callback for real-time log streaming
 * WHY:  Enable real-time monitoring and alerting
 *
 * @param bridge    Security logging bridge
 * @param callback  Callback function
 * @param user_data User context
 * @param filter    Category/severity filter (0 = all)
 * @return Callback ID or -1 on failure
 */
int security_logging_register_stream(
    security_logging_bridge_t* bridge,
    security_log_stream_callback_t callback,
    void* user_data,
    uint32_t filter
);

/**
 * @brief Unregister stream callback
 *
 * @param bridge      Security logging bridge
 * @param callback_id ID returned from register
 * @return 0 on success, error code on failure
 */
int security_logging_unregister_stream(
    security_logging_bridge_t* bridge,
    int callback_id
);

/**
 * @brief Register pattern detection callback
 *
 * @param bridge    Security logging bridge
 * @param callback  Callback function
 * @param user_data User context
 * @return 0 on success, error code on failure
 */
int security_logging_register_pattern_callback(
    security_logging_bridge_t* bridge,
    security_pattern_callback_t callback,
    void* user_data
);

/*=============================================================================
 * PATTERN ANALYSIS FUNCTIONS
 *============================================================================*/

/**
 * @brief Run pattern analysis
 *
 * WHAT: Analyze logs for attack patterns
 * WHY:  Feed threat intelligence to security systems
 *
 * @param bridge Security logging bridge
 * @return Number of patterns detected
 */
int security_logging_analyze_patterns(security_logging_bridge_t* bridge);

/**
 * @brief Get detected patterns
 *
 * @param bridge      Security logging bridge
 * @param patterns    Output array
 * @param max_count   Maximum patterns to return
 * @param actual_count Output: actual count
 * @return 0 on success, error code on failure
 */
int security_logging_get_patterns(
    const security_logging_bridge_t* bridge,
    security_threat_pattern_t* patterns,
    size_t max_count,
    size_t* actual_count
);

/**
 * @brief Clear detected patterns
 *
 * @param bridge Security logging bridge
 * @return 0 on success, error code on failure
 */
int security_logging_clear_patterns(security_logging_bridge_t* bridge);

/**
 * @brief Feed pattern to anomaly detector
 *
 * @param bridge  Security logging bridge
 * @param pattern Pattern to feed
 * @return 0 on success, error code on failure
 */
int security_logging_feed_pattern_to_detector(
    security_logging_bridge_t* bridge,
    const security_threat_pattern_t* pattern
);

/*=============================================================================
 * QUERY FUNCTIONS
 *============================================================================*/

/**
 * @brief Query log entries
 *
 * @param bridge      Security logging bridge
 * @param start_time  Start timestamp (0 = beginning)
 * @param end_time    End timestamp (0 = now)
 * @param categories  Category bitmask (0 = all)
 * @param min_severity Minimum severity
 * @param entries     Output array
 * @param max_count   Maximum entries
 * @param actual_count Output: actual count
 * @return 0 on success, error code on failure
 */
int security_logging_query_entries(
    const security_logging_bridge_t* bridge,
    uint64_t start_time,
    uint64_t end_time,
    uint32_t categories,
    security_log_severity_t min_severity,
    security_log_entry_t* entries,
    size_t max_count,
    size_t* actual_count
);

/**
 * @brief Get recent entries
 *
 * @param bridge      Security logging bridge
 * @param count       Number of entries to get
 * @param entries     Output array
 * @param actual_count Output: actual count
 * @return 0 on success, error code on failure
 */
int security_logging_get_recent(
    const security_logging_bridge_t* bridge,
    size_t count,
    security_log_entry_t* entries,
    size_t* actual_count
);

/**
 * @brief Search entries by message content
 *
 * @param bridge      Security logging bridge
 * @param search_term Search string
 * @param entries     Output array
 * @param max_count   Maximum entries
 * @param actual_count Output: actual count
 * @return 0 on success, error code on failure
 */
int security_logging_search(
    const security_logging_bridge_t* bridge,
    const char* search_term,
    security_log_entry_t* entries,
    size_t max_count,
    size_t* actual_count
);

/**
 * @brief Count entries matching criteria
 *
 * @param bridge      Security logging bridge
 * @param start_time  Start timestamp
 * @param end_time    End timestamp
 * @param categories  Category bitmask
 * @param min_severity Minimum severity
 * @return Number of matching entries
 */
size_t security_logging_count_entries(
    const security_logging_bridge_t* bridge,
    uint64_t start_time,
    uint64_t end_time,
    uint32_t categories,
    security_log_severity_t min_severity
);

/*=============================================================================
 * EXPORT FUNCTIONS
 *============================================================================*/

/**
 * @brief Export logs to file
 *
 * @param bridge    Security logging bridge
 * @param file_path Output file path
 * @param format    Export format
 * @param start_time Start timestamp (0 = all)
 * @param end_time  End timestamp (0 = all)
 * @return Number of entries exported, or -1 on error
 */
int security_logging_export_to_file(
    security_logging_bridge_t* bridge,
    const char* file_path,
    security_log_format_t format,
    uint64_t start_time,
    uint64_t end_time
);

/**
 * @brief Export entry to JSON string
 *
 * @param entry       Entry to export
 * @param buffer      Output buffer
 * @param buffer_size Buffer size
 * @return Bytes written, or -1 on error
 */
int security_logging_entry_to_json(
    const security_log_entry_t* entry,
    char* buffer,
    size_t buffer_size
);

/**
 * @brief Rotate log file
 *
 * @param bridge Security logging bridge
 * @return 0 on success, error code on failure
 */
int security_logging_rotate(security_logging_bridge_t* bridge);

/**
 * @brief Flush pending entries
 *
 * @param bridge Security logging bridge
 * @return 0 on success, error code on failure
 */
int security_logging_flush(security_logging_bridge_t* bridge);

/*=============================================================================
 * STATISTICS FUNCTIONS
 *============================================================================*/

/**
 * @brief Get bridge statistics
 *
 * @param bridge Security logging bridge
 * @param stats  Output statistics
 * @return 0 on success, error code on failure
 */
int security_logging_get_stats(
    const security_logging_bridge_t* bridge,
    security_logging_bridge_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge Security logging bridge
 * @return 0 on success, error code on failure
 */
int security_logging_reset_stats(security_logging_bridge_t* bridge);

/**
 * @brief Get current effects
 *
 * @param bridge Security logging bridge
 * @param security_effects Output: Security -> Logging effects
 * @param logging_effects  Output: Logging -> Security effects
 * @return 0 on success, error code on failure
 */
int security_logging_get_effects(
    const security_logging_bridge_t* bridge,
    security_to_logging_effects_t* security_effects,
    logging_to_security_effects_t* logging_effects
);

/*=============================================================================
 * UPDATE FUNCTIONS
 *============================================================================*/

/**
 * @brief Update bridge (process pending events)
 *
 * WHAT: Process pending events and run analysis
 * WHY:  Main update loop integration point
 *
 * @param bridge Security logging bridge
 * @return 0 on success, error code on failure
 *
 * SIDE EFFECTS:
 * - Processes pending log entries
 * - Runs pattern analysis (if due)
 * - Invokes stream callbacks
 * - Updates statistics
 */
int security_logging_update(security_logging_bridge_t* bridge);

/**
 * @brief Apply modulation (feed patterns to security)
 *
 * WHAT: Feed detected patterns back to security systems
 * WHY:  Close the feedback loop
 *
 * @param bridge Security logging bridge
 * @return 0 on success, error code on failure
 */
int security_logging_apply_modulation(security_logging_bridge_t* bridge);

/*=============================================================================
 * BIO-ASYNC FUNCTIONS
 *============================================================================*/

/**
 * @brief Connect to bio-async router
 *
 * @param bridge Security logging bridge
 * @return 0 on success, error code on failure
 */
int security_logging_connect_bio_async(security_logging_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Security logging bridge
 * @return 0 on success, error code on failure
 */
int security_logging_disconnect_bio_async(security_logging_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 *
 * @param bridge Security logging bridge
 * @return true if connected
 */
bool security_logging_is_bio_async_connected(const security_logging_bridge_t* bridge);

/**
 * @brief Process bio-async inbox messages
 *
 * @param bridge       Security logging bridge
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed
 */
uint32_t security_logging_process_inbox(
    security_logging_bridge_t* bridge,
    uint32_t max_messages
);

/**
 * @brief Send log event via bio-async broadcast
 *
 * @param bridge Security logging bridge
 * @param entry  Entry to broadcast
 * @return 0 on success, error code on failure
 */
int security_logging_broadcast_event(
    security_logging_bridge_t* bridge,
    const security_log_entry_t* entry
);

/*=============================================================================
 * UTILITY FUNCTIONS
 *============================================================================*/

/**
 * @brief Get category name
 *
 * @param category Category enum
 * @return Human-readable name
 */
const char* security_log_category_name(security_log_category_t category);

/**
 * @brief Get severity name
 *
 * @param severity Severity enum
 * @return Human-readable name
 */
const char* security_log_severity_name(security_log_severity_t severity);

/**
 * @brief Get action name
 *
 * @param action Action enum
 * @return Human-readable name
 */
const char* security_log_action_name(security_log_action_t action);

/**
 * @brief Get pattern type name
 *
 * @param type Pattern type enum
 * @return Human-readable name
 */
const char* security_pattern_type_name(security_pattern_type_t type);

/**
 * @brief Get format name
 *
 * @param format Format enum
 * @return Human-readable name
 */
const char* security_log_format_name(security_log_format_t format);

/**
 * @brief Convert threat level to log severity
 *
 * @param threat NIMCP threat level
 * @return Corresponding log severity
 */
security_log_severity_t security_threat_to_severity(nimcp_threat_level_t threat);

/**
 * @brief Convert BBB severity to log severity
 *
 * @param bbb_severity BBB severity level
 * @return Corresponding log severity
 */
security_log_severity_t security_bbb_to_log_severity(bbb_severity_t bbb_severity);

/**
 * @brief Create entry with current timestamp
 *
 * @param entry    Entry to initialize
 * @param category Event category
 * @param severity Event severity
 * @param message  Event message
 * @return 0 on success, error code on failure
 */
int security_log_entry_init(
    security_log_entry_t* entry,
    security_log_category_t category,
    security_log_severity_t severity,
    const char* message
);

/**
 * @brief Get current time in nanoseconds
 *
 * @return Nanoseconds since epoch
 */
uint64_t security_log_current_time_ns(void);

/**
 * @brief Create category bitmask
 *
 * @param category Category to include
 * @return Bitmask with category enabled
 */
#define SECURITY_LOG_CAT_MASK(category) (1u << (category))

/**
 * @brief Bitmask for all categories
 */
#define SECURITY_LOG_CAT_ALL ((1u << SECURITY_LOG_CAT_COUNT) - 1)

/**
 * @brief Print entry to stdout (debug)
 *
 * @param entry Entry to print
 */
void security_log_entry_print(const security_log_entry_t* entry);

/**
 * @brief Print bridge summary
 *
 * @param bridge Security logging bridge
 */
void security_logging_bridge_print_summary(const security_logging_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SECURITY_LOGGING_BRIDGE_H */
