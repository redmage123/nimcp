/**
 * @file nimcp_hypothalamus_logging_bridge.h
 * @brief Unified Hypothalamus-Logging Integration Bridge
 *
 * WHAT: Comprehensive logging and monitoring of all hypothalamus activity
 * WHY:  Provides audit trail for safety-critical operations, enables real-time
 *       monitoring, and supports structured logging for analysis
 * HOW:  Subscribes to orchestrator events, captures drive changes, alignment
 *       checks, stress responses, and provides export/query capabilities
 *
 * BIOLOGICAL BASIS:
 * =================
 * The hypothalamus integrates signals from multiple brain regions and body
 * systems. This logging bridge acts as the "recording system" - capturing
 * all hypothalamic events for safety auditing and system monitoring.
 *
 * In biological terms, this is analogous to the hypothalamus's projections
 * to the prefrontal cortex for conscious awareness of bodily states, and
 * its connections to memory systems for recording significant events.
 *
 * SAFETY RATIONALE (BYRNES' ALIGNMENT):
 * =====================================
 * Per Steve Byrnes' work on AGI safety:
 * 1. The steering subsystem (hypothalamus) defines reward function parameters
 * 2. All changes to alignment-critical parameters must be auditable
 * 3. Continuous monitoring enables early detection of misalignment
 * 4. Structured logging supports external verification
 *
 * KEY COMPONENTS:
 * ===============
 * - Drive state change logging
 * - Homeostatic alert logging
 * - Stress response logging
 * - Circadian phase logging
 * - Alignment check logging
 * - Setpoint modification audit trail
 * - Ring buffer for recent events
 * - Export to file for analysis
 *
 * THREAD SAFETY:
 * ==============
 * All logging functions are thread-safe. The bridge uses internal
 * synchronization to protect the ring buffer and statistics.
 *
 * @version Phase 21: Logging Integration Bridge
 * @date 2026-01-10
 */

#ifndef NIMCP_HYPOTHALAMUS_LOGGING_BRIDGE_H
#define NIMCP_HYPOTHALAMUS_LOGGING_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Forward declarations to avoid circular dependencies */
typedef struct hypo_drive_system hypo_drive_system_handle_t;
typedef struct hypo_homeostasis hypo_homeostasis_handle_t;
typedef struct hypo_orchestrator_struct* hypo_orchestrator_t;

#include "utils/logging/nimcp_logging.h"

/* Include drives header for type definitions */
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_drives.h"

/* Forward declare param type from alignment header */
typedef enum {
    HYPO_PARAM_NONE_LOG = 0,            /**< No specific parameter */
    HYPO_PARAM_SETPOINT_LOG,            /**< Homeostatic setpoint */
    HYPO_PARAM_ALIGNMENT_WEIGHT_LOG,    /**< Alignment weight */
    HYPO_PARAM_LOCK_STATE_LOG,          /**< Lock state */
    HYPO_PARAM_REWARD_GAIN_LOG,         /**< Reward/punishment gains */
    HYPO_PARAM_DRIVE_CONFIG_LOG         /**< Drive configuration */
} hypo_param_type_log_t;

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** Default ring buffer size for log entries */
#define HYPO_LOG_DEFAULT_BUFFER_SIZE    1024

/** Maximum message length in log entry */
#define HYPO_LOG_MAX_MESSAGE_LEN        128

/** Maximum entries for batch query */
#define HYPO_LOG_MAX_QUERY_ENTRIES      256

/** Module name for logging */
#define HYPO_LOG_MODULE_NAME            "hypothalamus"

/** Default log prefix */
#define HYPO_LOG_DEFAULT_PREFIX         "[HYPO]"

/*=============================================================================
 * LOG ENTRY TYPES
 *===========================================================================*/

/**
 * @brief Hypothalamus log entry types
 *
 * WHAT: Categorization of hypothalamus events for logging
 * WHY:  Enable filtering and analysis by event type
 */
typedef enum {
    HYPO_LOG_DRIVE_CHANGE = 0,          /**< Drive level changed */
    HYPO_LOG_DRIVE_SATISFIED,           /**< Drive was satisfied */
    HYPO_LOG_DRIVE_CONFLICT,            /**< Multiple drives in conflict */
    HYPO_LOG_HOMEOSTATIC_ALERT,         /**< Homeostatic deviation detected */
    HYPO_LOG_CIRCADIAN_PHASE,           /**< Circadian phase transition */
    HYPO_LOG_STRESS_RESPONSE,           /**< Stress response triggered */
    HYPO_LOG_AUTONOMIC_SHIFT,           /**< Autonomic state change */
    HYPO_LOG_ALIGNMENT_CHECK,           /**< Alignment verification performed */
    HYPO_LOG_ALIGNMENT_VIOLATION,       /**< Alignment constraint violated */
    HYPO_LOG_SETPOINT_CHANGE,           /**< Drive setpoint modified */
    HYPO_LOG_REWARD_SIGNAL,             /**< Reward signal generated */
    HYPO_LOG_BRIDGE_EVENT,              /**< Bridge lifecycle event */
    HYPO_LOG_ERROR,                     /**< Error condition */
    HYPO_LOG_COUNT                      /**< Total log type count */
} hypo_log_type_t;

/**
 * @brief Log entry severity levels
 *
 * Maps to nimcp_logging severity levels for consistency
 */
typedef enum {
    HYPO_LOG_SEVERITY_TRACE = 0,        /**< Detailed tracing */
    HYPO_LOG_SEVERITY_DEBUG,            /**< Debug information */
    HYPO_LOG_SEVERITY_INFO,             /**< Informational */
    HYPO_LOG_SEVERITY_WARNING,          /**< Warning condition */
    HYPO_LOG_SEVERITY_ERROR,            /**< Error condition */
    HYPO_LOG_SEVERITY_CRITICAL          /**< Critical/fatal */
} hypo_log_severity_t;

/*=============================================================================
 * LOG ENTRY STRUCTURE
 *===========================================================================*/

/**
 * @brief Structured hypothalamus log entry
 *
 * WHAT: Single log entry with full context
 * WHY:  Enable structured analysis and filtering
 * HOW:  Fixed-size record with all relevant fields
 */
typedef struct {
    uint64_t timestamp_us;              /**< Event timestamp (microseconds) */
    hypo_log_type_t type;               /**< Log entry type */
    uint32_t severity;                  /**< Severity level (nimcp_log_level_t) */
    uint32_t source_bridge;             /**< Source bridge type (hypo_bridge_type_t) */
    float value;                        /**< Primary value (context-dependent) */
    float secondary_value;              /**< Secondary value (context-dependent) */
    uint32_t sequence_number;           /**< Monotonic sequence number */
    uint32_t flags;                     /**< Additional flags */
    char message[HYPO_LOG_MAX_MESSAGE_LEN]; /**< Human-readable message */
} hypo_log_entry_t;

/*=============================================================================
 * LOG ENTRY FLAGS
 *===========================================================================*/

/** Entry involves alignment-critical parameter */
#define HYPO_LOG_FLAG_ALIGNMENT_CRITICAL    (1 << 0)

/** Entry requires audit trail */
#define HYPO_LOG_FLAG_AUDIT_REQUIRED        (1 << 1)

/** Entry generated by safety system */
#define HYPO_LOG_FLAG_SAFETY_EVENT          (1 << 2)

/** Entry involves locked parameter */
#define HYPO_LOG_FLAG_LOCKED_PARAM          (1 << 3)

/** Entry is from external source */
#define HYPO_LOG_FLAG_EXTERNAL              (1 << 4)

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Logging bridge configuration
 *
 * WHAT: Configuration for hypothalamus logging
 * WHY:  Control what gets logged and how
 */
typedef struct {
    /* Enable/disable by category */
    bool enable_drive_logging;          /**< Log drive state changes */
    bool enable_homeostatic_logging;    /**< Log homeostatic events */
    bool enable_stress_logging;         /**< Log stress responses */
    bool enable_circadian_logging;      /**< Log circadian transitions */
    bool enable_alignment_logging;      /**< Log alignment checks */
    bool enable_reward_logging;         /**< Log reward signals */
    bool enable_bridge_logging;         /**< Log bridge lifecycle */

    /* Output options */
    bool enable_structured_output;      /**< Output structured JSON format */
    bool enable_console_output;         /**< Echo to console */
    bool enable_file_output;            /**< Write to file */

    /* Ring buffer configuration */
    uint32_t ring_buffer_size;          /**< Number of entries in buffer */
    bool overwrite_when_full;           /**< Overwrite old entries or block */

    /* Filtering */
    log_level_t min_level;              /**< Minimum severity to log */
    uint32_t type_filter_mask;          /**< Bitmask of types to log */

    /* Format options */
    const char* log_prefix;             /**< Prefix for log messages */
    bool include_timestamp;             /**< Include timestamps */
    bool include_sequence;              /**< Include sequence numbers */

    /* File output */
    const char* log_file_path;          /**< Path for file output */
    bool append_to_file;                /**< Append vs overwrite */
    size_t max_file_size;               /**< Max file size before rotation */

    /* Performance */
    bool async_logging;                 /**< Use async logging */
    uint32_t batch_size;                /**< Batch size for file writes */
} hypo_logging_config_t;

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

/**
 * @brief Logging bridge statistics
 *
 * WHAT: Cumulative statistics about logging activity
 * WHY:  Monitor logging system health and patterns
 */
typedef struct {
    /* Entry counts by type */
    uint64_t entries_by_type[HYPO_LOG_COUNT];

    /* Total counts */
    uint64_t total_entries;             /**< Total entries logged */
    uint64_t entries_dropped;           /**< Entries dropped (buffer full) */
    uint64_t entries_filtered;          /**< Entries filtered out */
    uint64_t entries_exported;          /**< Entries exported to file */

    /* Severity counts */
    uint64_t trace_count;               /**< Trace entries */
    uint64_t debug_count;               /**< Debug entries */
    uint64_t info_count;                /**< Info entries */
    uint64_t warning_count;             /**< Warning entries */
    uint64_t error_count;               /**< Error entries */
    uint64_t critical_count;            /**< Critical entries */

    /* Safety-relevant counts */
    uint64_t alignment_checks;          /**< Alignment checks performed */
    uint64_t alignment_violations;      /**< Alignment violations detected */
    uint64_t safety_events;             /**< Safety-related events */
    uint64_t audit_entries;             /**< Audit trail entries */

    /* Buffer status */
    uint32_t buffer_size;               /**< Configured buffer size */
    uint32_t buffer_used;               /**< Current entries in buffer */
    float buffer_utilization;           /**< Buffer utilization [0-1] */

    /* Timing */
    uint64_t first_entry_time_us;       /**< Timestamp of first entry */
    uint64_t last_entry_time_us;        /**< Timestamp of last entry */
    uint64_t uptime_us;                 /**< Logging bridge uptime */

    /* Performance */
    uint64_t avg_log_latency_ns;        /**< Average logging latency */
    uint64_t max_log_latency_ns;        /**< Maximum logging latency */
} hypo_logging_stats_t;

/*=============================================================================
 * QUERY STRUCTURES
 *===========================================================================*/

/**
 * @brief Query filter for log entries
 *
 * WHAT: Filter criteria for querying log entries
 * WHY:  Enable targeted retrieval of specific events
 */
typedef struct {
    /* Time range */
    uint64_t start_time_us;             /**< Start time (0 = no filter) */
    uint64_t end_time_us;               /**< End time (0 = no filter) */

    /* Type filter */
    bool filter_by_type;                /**< Enable type filtering */
    uint32_t type_mask;                 /**< Bitmask of types to include */

    /* Severity filter */
    bool filter_by_severity;            /**< Enable severity filtering */
    uint32_t min_severity;              /**< Minimum severity */
    uint32_t max_severity;              /**< Maximum severity */

    /* Source filter */
    bool filter_by_source;              /**< Enable source filtering */
    uint32_t source_bridge;             /**< Source bridge type */

    /* Flag filter */
    bool filter_by_flags;               /**< Enable flag filtering */
    uint32_t required_flags;            /**< Flags that must be set */
    uint32_t excluded_flags;            /**< Flags that must not be set */

    /* Result limiting */
    uint32_t max_results;               /**< Maximum results (0 = no limit) */
    uint32_t offset;                    /**< Skip first N results */
    bool reverse_order;                 /**< Newest first */
} hypo_log_query_t;

/*=============================================================================
 * OPAQUE TYPE
 *===========================================================================*/

/**
 * @brief Opaque logging bridge handle
 */
typedef struct hypo_logging_bridge hypo_logging_bridge_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get default logging bridge configuration
 *
 * WHAT: Returns configuration with sensible defaults
 * WHY:  Simplify initialization with safe defaults
 * HOW:  All logging enabled, INFO level, 1024 entry buffer
 *
 * @param config Output configuration structure
 * @return 0 on success, -1 on error
 */
int hypo_logging_bridge_default_config(hypo_logging_config_t* config);

/**
 * @brief Create logging bridge
 *
 * WHAT: Allocate and initialize logging bridge
 * WHY:  Central logging for all hypothalamus activity
 * HOW:  Allocate buffer, initialize state
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle, or NULL on failure
 */
hypo_logging_bridge_t* hypo_logging_bridge_create(
    const hypo_logging_config_t* config);

/**
 * @brief Destroy logging bridge
 *
 * WHAT: Free all logging bridge resources
 * WHY:  Clean shutdown with optional final export
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void hypo_logging_bridge_destroy(hypo_logging_bridge_t* bridge);

/**
 * @brief Reset logging bridge
 *
 * WHAT: Clear all entries and reset statistics
 * WHY:  Fresh start without destroying/recreating
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int hypo_logging_bridge_reset(hypo_logging_bridge_t* bridge);

/*=============================================================================
 * CONNECTION FUNCTIONS
 *===========================================================================*/

/**
 * @brief Connect logging bridge to orchestrator
 *
 * WHAT: Subscribe to orchestrator events for logging
 * WHY:  Automatic capture of all hypothalamus events
 * HOW:  Register as subscriber for all event types
 *
 * @param bridge Logging bridge handle
 * @param orch Orchestrator handle
 * @return 0 on success, -1 on error
 */
int hypo_logging_connect(
    hypo_logging_bridge_t* bridge,
    hypo_orchestrator_t orch);

/**
 * @brief Disconnect logging bridge from orchestrator
 *
 * @param bridge Logging bridge handle
 * @return 0 on success, -1 on error
 */
int hypo_logging_disconnect(hypo_logging_bridge_t* bridge);

/**
 * @brief Connect to drive system for direct monitoring
 *
 * @param bridge Logging bridge handle
 * @param drives Drive system handle
 * @return 0 on success, -1 on error
 */
int hypo_logging_connect_drives(
    hypo_logging_bridge_t* bridge,
    hypo_drive_system_handle_t* drives);

/**
 * @brief Connect to homeostasis system for direct monitoring
 *
 * @param bridge Logging bridge handle
 * @param homeostasis Homeostasis system handle
 * @return 0 on success, -1 on error
 */
int hypo_logging_connect_homeostasis(
    hypo_logging_bridge_t* bridge,
    hypo_homeostasis_handle_t* homeostasis);

/*=============================================================================
 * LOGGING FUNCTIONS
 *===========================================================================*/

/**
 * @brief Log a drive state change
 *
 * WHAT: Record drive level transition
 * WHY:  Track drive dynamics for analysis
 *
 * @param bridge Logging bridge handle
 * @param drive Drive type
 * @param old_val Previous drive level
 * @param new_val New drive level
 * @return 0 on success, -1 on error
 */
int hypo_logging_log_drive(
    hypo_logging_bridge_t* bridge,
    hypo_drive_type_t drive,
    float old_val,
    float new_val);

/**
 * @brief Log drive satisfaction event
 *
 * WHAT: Record when a drive is satisfied
 * WHY:  Track drive satisfaction patterns
 *
 * @param bridge Logging bridge handle
 * @param drive Drive type that was satisfied
 * @param satisfaction_level Level of satisfaction [0-1]
 * @param reward Reward signal generated
 * @return 0 on success, -1 on error
 */
int hypo_logging_log_drive_satisfied(
    hypo_logging_bridge_t* bridge,
    hypo_drive_type_t drive,
    float satisfaction_level,
    float reward);

/**
 * @brief Log drive conflict
 *
 * WHAT: Record competing drive states
 * WHY:  Track conflict resolution patterns
 *
 * @param bridge Logging bridge handle
 * @param drive1 First conflicting drive
 * @param drive2 Second conflicting drive
 * @param winner Which drive won priority
 * @return 0 on success, -1 on error
 */
int hypo_logging_log_drive_conflict(
    hypo_logging_bridge_t* bridge,
    hypo_drive_type_t drive1,
    hypo_drive_type_t drive2,
    hypo_drive_type_t winner);

/**
 * @brief Log homeostatic alert
 *
 * WHAT: Record homeostatic deviation
 * WHY:  Track regulatory system performance
 *
 * @param bridge Logging bridge handle
 * @param variable Variable name or type string
 * @param deviation Deviation from setpoint
 * @return 0 on success, -1 on error
 */
int hypo_logging_log_homeostatic(
    hypo_logging_bridge_t* bridge,
    const char* variable,
    float deviation);

/**
 * @brief Log stress response
 *
 * WHAT: Record stress system activation
 * WHY:  Monitor stress patterns and triggers
 *
 * @param bridge Logging bridge handle
 * @param cortisol Cortisol level
 * @param trigger Stress trigger description
 * @return 0 on success, -1 on error
 */
int hypo_logging_log_stress(
    hypo_logging_bridge_t* bridge,
    float cortisol,
    const char* trigger);

/**
 * @brief Log circadian phase transition
 *
 * WHAT: Record circadian rhythm changes
 * WHY:  Track sleep-wake cycle
 *
 * @param bridge Logging bridge handle
 * @param old_phase Previous phase
 * @param new_phase New phase
 * @param alertness Current alertness level
 * @return 0 on success, -1 on error
 */
int hypo_logging_log_circadian(
    hypo_logging_bridge_t* bridge,
    uint32_t old_phase,
    uint32_t new_phase,
    float alertness);

/**
 * @brief Log alignment check result
 *
 * WHAT: Record alignment verification
 * WHY:  Audit trail for alignment safety
 *
 * @param bridge Logging bridge handle
 * @param passed Whether check passed
 * @param check_name Name of the check performed
 * @return 0 on success, -1 on error
 */
int hypo_logging_log_alignment(
    hypo_logging_bridge_t* bridge,
    bool passed,
    const char* check_name);

/**
 * @brief Log alignment violation
 *
 * WHAT: Record alignment constraint violation
 * WHY:  Critical safety audit trail
 *
 * @param bridge Logging bridge handle
 * @param violation_type Type of violation
 * @param details Violation details
 * @param severity Severity level
 * @return 0 on success, -1 on error
 */
int hypo_logging_log_alignment_violation(
    hypo_logging_bridge_t* bridge,
    const char* violation_type,
    const char* details,
    uint32_t severity);

/**
 * @brief Log setpoint change
 *
 * WHAT: Record setpoint modification
 * WHY:  Audit trail for alignment-critical changes
 *
 * @param bridge Logging bridge handle
 * @param param_type Parameter type changed
 * @param old_value Previous value
 * @param new_value New value
 * @param modifier_id Who made the change
 * @return 0 on success, -1 on error
 */
int hypo_logging_log_setpoint_change(
    hypo_logging_bridge_t* bridge,
    hypo_param_type_log_t param_type,
    float old_value,
    float new_value,
    uint32_t modifier_id);

/**
 * @brief Log reward signal
 *
 * WHAT: Record reward signal generation
 * WHY:  Track steering signal patterns
 *
 * @param bridge Logging bridge handle
 * @param reward Reward value
 * @param source_drive Drive that generated reward
 * @param prediction_error Reward prediction error
 * @return 0 on success, -1 on error
 */
int hypo_logging_log_reward(
    hypo_logging_bridge_t* bridge,
    float reward,
    hypo_drive_type_t source_drive,
    float prediction_error);

/**
 * @brief Log generic event
 *
 * WHAT: Log arbitrary event type
 * WHY:  Flexible event logging
 *
 * @param bridge Logging bridge handle
 * @param type Event type
 * @param msg Human-readable message
 * @return 0 on success, -1 on error
 */
int hypo_logging_log_event(
    hypo_logging_bridge_t* bridge,
    hypo_log_type_t type,
    const char* msg);

/**
 * @brief Log event with full details
 *
 * WHAT: Log event with all fields specified
 * WHY:  Maximum control over log entry
 *
 * @param bridge Logging bridge handle
 * @param entry Pre-filled entry structure
 * @return 0 on success, -1 on error
 */
int hypo_logging_log_entry(
    hypo_logging_bridge_t* bridge,
    const hypo_log_entry_t* entry);

/*=============================================================================
 * QUERY FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get most recent log entries
 *
 * WHAT: Retrieve recent entries from ring buffer
 * WHY:  Quick access to recent activity
 *
 * @param bridge Logging bridge handle
 * @param entries Output array for entries
 * @param max Maximum entries to retrieve
 * @param count Output: actual count retrieved
 * @return 0 on success, -1 on error
 */
int hypo_logging_get_recent(
    hypo_logging_bridge_t* bridge,
    hypo_log_entry_t* entries,
    uint32_t max,
    uint32_t* count);

/**
 * @brief Query log entries with filter
 *
 * WHAT: Retrieve entries matching filter criteria
 * WHY:  Targeted retrieval for analysis
 *
 * @param bridge Logging bridge handle
 * @param query Query filter criteria
 * @param entries Output array for entries
 * @param max Maximum entries to retrieve
 * @param count Output: actual count retrieved
 * @return 0 on success, -1 on error
 */
int hypo_logging_query(
    hypo_logging_bridge_t* bridge,
    const hypo_log_query_t* query,
    hypo_log_entry_t* entries,
    uint32_t max,
    uint32_t* count);

/**
 * @brief Get entry count by type
 *
 * @param bridge Logging bridge handle
 * @param type Entry type
 * @return Count of entries of this type
 */
uint64_t hypo_logging_get_count_by_type(
    const hypo_logging_bridge_t* bridge,
    hypo_log_type_t type);

/**
 * @brief Get total entry count
 *
 * @param bridge Logging bridge handle
 * @return Total entries logged
 */
uint64_t hypo_logging_get_total_count(const hypo_logging_bridge_t* bridge);

/*=============================================================================
 * EXPORT FUNCTIONS
 *===========================================================================*/

/**
 * @brief Export log entries to file
 *
 * WHAT: Write log entries to file
 * WHY:  Persistent storage and analysis
 *
 * @param bridge Logging bridge handle
 * @param filepath Output file path
 * @return 0 on success, -1 on error
 */
int hypo_logging_export(
    hypo_logging_bridge_t* bridge,
    const char* filepath);

/**
 * @brief Export entries to JSON file
 *
 * WHAT: Write structured JSON output
 * WHY:  Machine-readable format for analysis
 *
 * @param bridge Logging bridge handle
 * @param filepath Output file path
 * @return 0 on success, -1 on error
 */
int hypo_logging_export_json(
    hypo_logging_bridge_t* bridge,
    const char* filepath);

/**
 * @brief Export entries matching query
 *
 * WHAT: Export filtered subset of entries
 * WHY:  Targeted export for specific analysis
 *
 * @param bridge Logging bridge handle
 * @param query Query filter
 * @param filepath Output file path
 * @return 0 on success, -1 on error
 */
int hypo_logging_export_query(
    hypo_logging_bridge_t* bridge,
    const hypo_log_query_t* query,
    const char* filepath);

/*=============================================================================
 * STATISTICS FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get logging statistics
 *
 * WHAT: Retrieve cumulative statistics
 * WHY:  Monitor logging system health
 *
 * @param bridge Logging bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on error
 */
int hypo_logging_get_stats(
    const hypo_logging_bridge_t* bridge,
    hypo_logging_stats_t* stats);

/**
 * @brief Reset logging statistics
 *
 * WHAT: Clear all statistics counters
 * WHY:  Fresh measurement period
 *
 * @param bridge Logging bridge handle
 * @return 0 on success, -1 on error
 */
int hypo_logging_reset_stats(hypo_logging_bridge_t* bridge);

/*=============================================================================
 * CONFIGURATION FUNCTIONS
 *===========================================================================*/

/**
 * @brief Update configuration
 *
 * WHAT: Modify logging configuration at runtime
 * WHY:  Adjust logging without restart
 *
 * @param bridge Logging bridge handle
 * @param config New configuration
 * @return 0 on success, -1 on error
 */
int hypo_logging_set_config(
    hypo_logging_bridge_t* bridge,
    const hypo_logging_config_t* config);

/**
 * @brief Get current configuration
 *
 * @param bridge Logging bridge handle
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int hypo_logging_get_config(
    const hypo_logging_bridge_t* bridge,
    hypo_logging_config_t* config);

/**
 * @brief Set minimum log level
 *
 * @param bridge Logging bridge handle
 * @param level Minimum severity to log
 * @return 0 on success, -1 on error
 */
int hypo_logging_set_level(
    hypo_logging_bridge_t* bridge,
    log_level_t level);

/**
 * @brief Enable/disable logging category
 *
 * @param bridge Logging bridge handle
 * @param type Log type to enable/disable
 * @param enable true to enable, false to disable
 * @return 0 on success, -1 on error
 */
int hypo_logging_set_type_enabled(
    hypo_logging_bridge_t* bridge,
    hypo_log_type_t type,
    bool enable);

/*=============================================================================
 * UTILITY FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get log type name
 *
 * @param type Log type
 * @return Human-readable name (never NULL)
 */
const char* hypo_log_type_name(hypo_log_type_t type);

/**
 * @brief Get severity name
 *
 * @param severity Severity level
 * @return Human-readable name (never NULL)
 */
const char* hypo_log_severity_name(hypo_log_severity_t severity);

/**
 * @brief Format log entry as string
 *
 * @param entry Log entry
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Characters written, or -1 on error
 */
int hypo_log_entry_format(
    const hypo_log_entry_t* entry,
    char* buffer,
    size_t buffer_size);

/**
 * @brief Format log entry as JSON
 *
 * @param entry Log entry
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Characters written, or -1 on error
 */
int hypo_log_entry_format_json(
    const hypo_log_entry_t* entry,
    char* buffer,
    size_t buffer_size);

/**
 * @brief Print logging summary to stdout
 *
 * @param bridge Logging bridge handle (NULL safe)
 */
void hypo_logging_print_summary(const hypo_logging_bridge_t* bridge);

/**
 * @brief Print recent entries to stdout
 *
 * @param bridge Logging bridge handle
 * @param count Number of entries to print
 */
void hypo_logging_print_recent(
    const hypo_logging_bridge_t* bridge,
    uint32_t count);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HYPOTHALAMUS_LOGGING_BRIDGE_H */
