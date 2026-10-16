//=============================================================================
// nimcp_pr_logging_bridge.h - Prime Resonant Logging Bridge
//=============================================================================
/**
 * @file nimcp_pr_logging_bridge.h
 * @brief Comprehensive logging and tracing for PR Memory System
 *
 * WHAT: Full logging and tracing infrastructure for Prime Resonant memory
 * WHY:  Enable debugging, analysis, performance profiling, and system
 *       understanding through comprehensive operation logging
 * HOW:  Ring buffer for memory traces, structured log entries, export
 *       capabilities, and configurable verbosity levels
 *
 * LOGGING ARCHITECTURE:
 *
 *   Logging Layers:
 *   +-----------------------------------------------------------------------+
 *   |                                                                       |
 *   |  +----------------------+                                             |
 *   |  | Application Layer    |  High-level memory operations              |
 *   |  | (encode, retrieve)   |                                             |
 *   |  +----------+-----------+                                             |
 *   |             |                                                         |
 *   |  +----------v-----------+                                             |
 *   |  | Middleware Layer     |  Resonance, entanglement, consolidation    |
 *   |  | (sync, modulate)     |                                             |
 *   |  +----------+-----------+                                             |
 *   |             |                                                         |
 *   |  +----------v-----------+                                             |
 *   |  | Core Layer           |  Node operations, quaternion changes       |
 *   |  | (create, update)     |                                             |
 *   |  +----------+-----------+                                             |
 *   |             |                                                         |
 *   |  +----------v-----------+                                             |
 *   |  | Logging Bridge       |  Captures all operations                   |
 *   |  | (ring buffer, export)|                                             |
 *   |  +----------------------+                                             |
 *   |                                                                       |
 *   +-----------------------------------------------------------------------+
 *
 *   Log Entry Structure:
 *   +-----------------------------------------------------------------------+
 *   |  +-------------+------------------------------------------------+    |
 *   |  | Timestamp   | Nanosecond precision for performance analysis  |    |
 *   |  +-------------+------------------------------------------------+    |
 *   |  | Level       | TRACE, DEBUG, INFO, WARN, ERROR, FATAL         |    |
 *   |  +-------------+------------------------------------------------+    |
 *   |  | Category    | ENCODE, RETRIEVE, CONSOLIDATE, etc.            |    |
 *   |  +-------------+------------------------------------------------+    |
 *   |  | Memory ID   | Associated memory node (if any)                |    |
 *   |  +-------------+------------------------------------------------+    |
 *   |  | Quaternion  | Memory state at time of operation              |    |
 *   |  +-------------+------------------------------------------------+    |
 *   |  | Details     | Operation-specific data (variable length)      |    |
 *   |  +-------------+------------------------------------------------+    |
 *   +-----------------------------------------------------------------------+
 *
 *   Memory Trace Recording:
 *   +-----------------------------------------------------------------------+
 *   |  Traces capture full memory operation history:                        |
 *   |                                                                       |
 *   |  [T1] CREATE   mem_42  tier=Z0  q=(1.0, 0.0, 0.5, 1.0)               |
 *   |  [T2] RETRIEVE mem_42  resonance=0.85  latency=12ms                  |
 *   |  [T3] MODIFY   mem_42  q=(1.0, 0.0, 0.5, 1.0) -> (0.9, 0.1, 0.6, 0.9)|
 *   |  [T4] PROMOTE  mem_42  tier=Z0 -> Z1                                 |
 *   |  [T5] ENTANGLE mem_42 <-> mem_87  strength=0.7                       |
 *   |                                                                       |
 *   |  Traces enable:                                                       |
 *   |  - Post-hoc analysis of memory behavior                              |
 *   |  - Performance profiling and optimization                            |
 *   |  - Debugging unexpected memory states                                |
 *   |  - System understanding and visualization                            |
 *   +-----------------------------------------------------------------------+
 *
 *   Export Formats:
 *   +-----------------------------------------------------------------------+
 *   |  Format      | Description                    | Use Case             |
 *   |--------------|--------------------------------|----------------------|
 *   |  JSON        | Structured, machine-readable   | Analysis pipelines   |
 *   |  CSV         | Tabular, spreadsheet-friendly  | Quick analysis       |
 *   |  Binary      | Compact, fast to write/read    | High-volume logging  |
 *   |  Human       | Formatted text                 | Console/debugging    |
 *   +-----------------------------------------------------------------------+
 *
 * PERFORMANCE:
 * - Log entry: O(1) ring buffer insertion
 * - Export: O(n) where n = number of entries
 * - Filtering: O(n) with early termination
 * - Minimal impact on memory operations (<1us overhead)
 *
 * MEMORY:
 * - pr_logging_bridge_t: ~4KB base structure
 * - Ring buffer: configurable (default 16K entries = ~2MB)
 * - Export buffers: allocated on demand
 *
 * THREAD SAFETY:
 * - All public functions are thread-safe via internal mutex
 * - Lock-free fast path for high-frequency logging
 *
 * INTEGRATION:
 * - Core: All PR memory operations
 * - Middleware: Resonance, entanglement, consolidation
 * - Application: Query patterns, usage statistics
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#ifndef NIMCP_PR_LOGGING_BRIDGE_H
#define NIMCP_PR_LOGGING_BRIDGE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

// Dependencies
#include "cognitive/memory/core/nimcp_quaternion.h"
#include "cognitive/memory/core/nimcp_pr_memory_node.h"
#include "cognitive/memory/core/nimcp_prime_signature.h"

#ifdef __cplusplus
extern "C" {
#endif

// Export macro (for shared library builds)
#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

//=============================================================================
// Constants
//=============================================================================

/** Default ring buffer capacity (entries) */
#define PR_LOG_DEFAULT_BUFFER_SIZE      16384

/** Maximum message length */
#define PR_LOG_MAX_MESSAGE_LENGTH       256

/** Maximum details length */
#define PR_LOG_MAX_DETAILS_LENGTH       512

/** Maximum file path length */
#define PR_LOG_MAX_PATH_LENGTH          256

/** Maximum entries per export batch */
#define PR_LOG_MAX_EXPORT_BATCH         4096

/** Default flush interval (milliseconds) */
#define PR_LOG_DEFAULT_FLUSH_INTERVAL   1000

/** Log format magic number */
#define PR_LOG_BINARY_MAGIC             0x50524C47  /* "PRLG" */

/** Log format version */
#define PR_LOG_FORMAT_VERSION           1

//=============================================================================
// Type Definitions - Enumerations
//=============================================================================

/**
 * @brief Log severity levels
 *
 * WHAT: Categories of log message severity
 * WHY:  Enable filtering by importance level
 */
typedef enum {
    PR_LOG_LEVEL_TRACE = 0,     /**< Very detailed tracing */
    PR_LOG_LEVEL_DEBUG,         /**< Debug information */
    PR_LOG_LEVEL_INFO,          /**< Informational messages */
    PR_LOG_LEVEL_WARN,          /**< Warning conditions */
    PR_LOG_LEVEL_ERROR,         /**< Error conditions */
    PR_LOG_LEVEL_FATAL,         /**< Fatal errors */
    PR_LOG_LEVEL_COUNT          /**< Number of levels */
} pr_log_level_t;

/**
 * @brief Log operation categories
 *
 * WHAT: Categories of memory operations being logged
 * WHY:  Enable filtering by operation type
 */
typedef enum {
    PR_LOG_CAT_ENCODE = 0,      /**< Memory encoding operations */
    PR_LOG_CAT_RETRIEVE,        /**< Memory retrieval operations */
    PR_LOG_CAT_CONSOLIDATE,     /**< Consolidation events */
    PR_LOG_CAT_PROMOTE,         /**< Tier promotion */
    PR_LOG_CAT_DEMOTE,          /**< Tier demotion */
    PR_LOG_CAT_DECAY,           /**< Memory decay */
    PR_LOG_CAT_ENTANGLE,        /**< Entanglement operations */
    PR_LOG_CAT_RESONANCE,       /**< Resonance calculations */
    PR_LOG_CAT_STATE,           /**< State changes */
    PR_LOG_CAT_CREATE,          /**< Node creation */
    PR_LOG_CAT_DESTROY,         /**< Node destruction */
    PR_LOG_CAT_CLONE,           /**< COW cloning */
    PR_LOG_CAT_SYNC,            /**< Synchronization events */
    PR_LOG_CAT_ERROR,           /**< Error events */
    PR_LOG_CAT_PERFORMANCE,     /**< Performance metrics */
    PR_LOG_CAT_SYSTEM,          /**< System events */
    PR_LOG_CAT_COUNT            /**< Number of categories */
} pr_log_category_t;

/**
 * @brief Export format types
 *
 * WHAT: Available output formats for log export
 * WHY:  Support different analysis needs
 */
typedef enum {
    PR_LOG_FORMAT_JSON = 0,     /**< JSON structured format */
    PR_LOG_FORMAT_CSV,          /**< CSV tabular format */
    PR_LOG_FORMAT_BINARY,       /**< Compact binary format */
    PR_LOG_FORMAT_HUMAN,        /**< Human-readable text */
    PR_LOG_FORMAT_COUNT         /**< Number of formats */
} pr_log_format_t;

/**
 * @brief Logging bridge error codes
 */
typedef enum {
    PR_LOG_SUCCESS = 0,                 /**< Operation succeeded */
    PR_LOG_ERROR_NULL_POINTER = -1,     /**< NULL pointer argument */
    PR_LOG_ERROR_NO_MEMORY = -2,        /**< Memory allocation failed */
    PR_LOG_ERROR_NOT_INITIALIZED = -3,  /**< Bridge not initialized */
    PR_LOG_ERROR_INVALID_CONFIG = -4,   /**< Invalid configuration */
    PR_LOG_ERROR_BUFFER_FULL = -5,      /**< Ring buffer overflow */
    PR_LOG_ERROR_IO = -6,               /**< I/O error */
    PR_LOG_ERROR_INVALID_FORMAT = -7,   /**< Invalid export format */
    PR_LOG_ERROR_FILTER_MISMATCH = -8   /**< No entries match filter */
} pr_log_error_t;

//=============================================================================
// Type Definitions - Structures
//=============================================================================

/**
 * @brief Single log entry
 *
 * WHAT: Complete record of a logged operation
 * WHY:  Enable detailed analysis and replay
 */
typedef struct {
    /* Timing */
    uint64_t timestamp_ns;              /**< Nanosecond timestamp */
    uint64_t sequence_number;           /**< Global sequence number */

    /* Classification */
    pr_log_level_t level;               /**< Severity level */
    pr_log_category_t category;         /**< Operation category */

    /* Memory context */
    uint64_t memory_id;                 /**< Associated memory (0 if none) */
    uint64_t correlation_id;            /**< Related memory (for entanglement, etc.) */
    pr_memory_tier_t tier;              /**< Memory tier at time of operation */
    nimcp_quaternion_t state;           /**< Quaternion state */

    /* Operation details */
    float metric_value;                 /**< Primary metric (resonance, latency, etc.) */
    float secondary_value;              /**< Secondary metric */
    uint32_t flags;                     /**< Operation-specific flags */

    /* Message */
    char message[PR_LOG_MAX_MESSAGE_LENGTH]; /**< Human-readable message */

    /* Thread info */
    uint64_t thread_id;                 /**< Originating thread ID */
} pr_log_entry_t;

/**
 * @brief Extended log entry with additional details
 *
 * WHAT: Log entry with variable-length details
 * WHY:  Support rich contextual information
 */
typedef struct {
    pr_log_entry_t base;                /**< Base entry */
    size_t details_length;              /**< Length of details string */
    char details[PR_LOG_MAX_DETAILS_LENGTH]; /**< Extended details */
    prime_signature_t* signature;       /**< Optional signature (NULL if none) */
} pr_log_entry_extended_t;

/**
 * @brief Log filter criteria
 *
 * WHAT: Filter parameters for querying logs
 * WHY:  Enable selective retrieval and export
 */
typedef struct {
    /* Time range */
    uint64_t start_time_ns;             /**< Start of time range (0 = no limit) */
    uint64_t end_time_ns;               /**< End of time range (0 = no limit) */

    /* Level filter */
    pr_log_level_t min_level;           /**< Minimum severity level */
    pr_log_level_t max_level;           /**< Maximum severity level */

    /* Category filter */
    uint32_t category_mask;             /**< Bitmask of categories (0 = all) */

    /* Memory filter */
    uint64_t memory_id;                 /**< Specific memory ID (0 = all) */
    pr_memory_tier_t tier;              /**< Specific tier (PR_MEMORY_TIER_COUNT = all) */

    /* Metric filter */
    float min_metric;                   /**< Minimum metric value */
    float max_metric;                   /**< Maximum metric value */

    /* Limit */
    size_t max_entries;                 /**< Maximum entries to return (0 = no limit) */
} pr_log_filter_t;

/**
 * @brief Bridge configuration
 *
 * WHAT: Parameters controlling logging behavior
 * WHY:  Allow customization for different use cases
 */
typedef struct {
    /* Buffer settings */
    size_t buffer_capacity;             /**< Ring buffer size (entries) */
    bool overwrite_on_full;             /**< Overwrite oldest on full (vs block) */

    /* Level settings */
    pr_log_level_t min_level;           /**< Minimum level to record */
    uint32_t enabled_categories;        /**< Bitmask of enabled categories */

    /* Output settings */
    bool log_to_console;                /**< Echo to stdout */
    bool log_to_file;                   /**< Write to file */
    char log_file_path[PR_LOG_MAX_PATH_LENGTH]; /**< File path */
    pr_log_format_t file_format;        /**< File output format */

    /* Performance settings */
    bool enable_timestamps;             /**< Include timestamps (slight overhead) */
    bool enable_thread_id;              /**< Include thread IDs */
    bool enable_signatures;             /**< Include prime signatures (memory heavy) */
    uint64_t flush_interval_ms;         /**< Auto-flush interval (0 = manual) */

    /* Filtering */
    bool filter_duplicates;             /**< Suppress repeated identical entries */
    uint32_t duplicate_window_ms;       /**< Window for duplicate detection */
} pr_logging_config_t;

/**
 * @brief Logging statistics
 *
 * WHAT: Operational metrics for the logging bridge
 * WHY:  Monitor logging performance and usage
 */
typedef struct {
    /* Entry counts */
    uint64_t total_entries;             /**< Total entries logged */
    uint64_t entries_by_level[PR_LOG_LEVEL_COUNT]; /**< Per-level counts */
    uint64_t entries_by_category[PR_LOG_CAT_COUNT]; /**< Per-category counts */
    uint64_t entries_dropped;           /**< Entries dropped (buffer full) */
    uint64_t duplicates_filtered;       /**< Duplicate entries filtered */

    /* Buffer status */
    size_t current_size;                /**< Current entries in buffer */
    size_t buffer_capacity;             /**< Total buffer capacity */
    float buffer_utilization;           /**< Percentage full */
    uint64_t overwrites;                /**< Times buffer wrapped */

    /* Export statistics */
    uint64_t exports_completed;         /**< Successful exports */
    uint64_t entries_exported;          /**< Total entries exported */
    uint64_t export_bytes_written;      /**< Total bytes exported */

    /* Performance */
    float avg_log_time_ns;              /**< Average time to log entry */
    float max_log_time_ns;              /**< Maximum log time */
    uint64_t file_writes;               /**< File write operations */
    uint64_t file_flushes;              /**< File flush operations */

    /* Timing */
    uint64_t last_entry_time_ns;        /**< Time of last entry */
    uint64_t last_export_time_ns;       /**< Time of last export */
} pr_logging_stats_t;

/**
 * @brief Export result
 *
 * WHAT: Result of an export operation
 * WHY:  Provide feedback on export success
 */
typedef struct {
    pr_log_error_t error;               /**< Error code */
    size_t entries_exported;            /**< Number of entries exported */
    size_t bytes_written;               /**< Bytes written */
    uint64_t first_timestamp_ns;        /**< First entry timestamp */
    uint64_t last_timestamp_ns;         /**< Last entry timestamp */
    char file_path[PR_LOG_MAX_PATH_LENGTH]; /**< Output file path */
} pr_export_result_t;

/**
 * @brief Opaque logging bridge handle
 */
typedef struct pr_logging_bridge_struct* pr_logging_bridge_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * WHAT: Returns sensible defaults for general logging
 * WHY:  Provides starting point for typical usage
 *
 * @return Default configuration structure
 */
NIMCP_EXPORT pr_logging_config_t pr_logging_config_default(void);

/**
 * @brief Validate bridge configuration
 *
 * @param config Configuration to validate
 * @return true if valid, false otherwise
 */
NIMCP_EXPORT bool pr_logging_config_validate(const pr_logging_config_t* config);

/**
 * @brief Get default filter (matches all entries)
 *
 * @return Default filter structure
 */
NIMCP_EXPORT pr_log_filter_t pr_log_filter_default(void);

/**
 * @brief Create filter for specific memory
 *
 * @param memory_id Memory ID to filter on
 * @return Filter matching only specified memory
 */
NIMCP_EXPORT pr_log_filter_t pr_log_filter_for_memory(uint64_t memory_id);

/**
 * @brief Create filter for time range
 *
 * @param start_ns Start timestamp (nanoseconds)
 * @param end_ns End timestamp (nanoseconds)
 * @return Filter for time range
 */
NIMCP_EXPORT pr_log_filter_t pr_log_filter_for_time_range(
    uint64_t start_ns,
    uint64_t end_ns
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create logging bridge
 *
 * WHAT: Allocates and initializes logging bridge
 * WHY:  Entry point for PR memory logging
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 *
 * COMPLEXITY: O(buffer_capacity)
 * MEMORY: ~4KB + buffer (default ~2MB)
 */
NIMCP_EXPORT pr_logging_bridge_t pr_logging_bridge_create(
    const pr_logging_config_t* config
);

/**
 * @brief Destroy logging bridge
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
NIMCP_EXPORT void pr_logging_bridge_destroy(pr_logging_bridge_t bridge);

/**
 * @brief Flush pending entries to file
 *
 * @param bridge Logging bridge
 * @return PR_LOG_SUCCESS or error code
 */
NIMCP_EXPORT pr_log_error_t pr_logging_bridge_flush(pr_logging_bridge_t bridge);

/**
 * @brief Clear all log entries
 *
 * @param bridge Logging bridge
 * @return PR_LOG_SUCCESS or error code
 */
NIMCP_EXPORT pr_log_error_t pr_logging_bridge_clear(pr_logging_bridge_t bridge);

/**
 * @brief Set minimum log level
 *
 * @param bridge Logging bridge
 * @param level New minimum level
 * @return PR_LOG_SUCCESS or error code
 */
NIMCP_EXPORT pr_log_error_t pr_logging_bridge_set_level(
    pr_logging_bridge_t bridge,
    pr_log_level_t level
);

/**
 * @brief Enable/disable category
 *
 * @param bridge Logging bridge
 * @param category Category to modify
 * @param enabled true to enable, false to disable
 * @return PR_LOG_SUCCESS or error code
 */
NIMCP_EXPORT pr_log_error_t pr_logging_bridge_set_category(
    pr_logging_bridge_t bridge,
    pr_log_category_t category,
    bool enabled
);

//=============================================================================
// Core Logging Functions
//=============================================================================

/**
 * @brief Log a basic entry
 *
 * WHAT: Record a log entry with minimal overhead
 * WHY:  Fast path for high-frequency logging
 *
 * @param bridge Logging bridge
 * @param level Severity level
 * @param category Operation category
 * @param message Human-readable message
 * @return PR_LOG_SUCCESS or error code
 *
 * COMPLEXITY: O(1) amortized
 *
 * EXAMPLE:
 * ```c
 * pr_logging_bridge_log(bridge, PR_LOG_LEVEL_INFO, PR_LOG_CAT_ENCODE,
 *     "Encoded new memory");
 * ```
 */
NIMCP_EXPORT pr_log_error_t pr_logging_bridge_log(
    pr_logging_bridge_t bridge,
    pr_log_level_t level,
    pr_log_category_t category,
    const char* message
);

/**
 * @brief Log entry with memory context
 *
 * @param bridge Logging bridge
 * @param level Severity level
 * @param category Operation category
 * @param memory_id Associated memory ID
 * @param message Human-readable message
 * @return PR_LOG_SUCCESS or error code
 */
NIMCP_EXPORT pr_log_error_t pr_logging_bridge_log_memory(
    pr_logging_bridge_t bridge,
    pr_log_level_t level,
    pr_log_category_t category,
    uint64_t memory_id,
    const char* message
);

/**
 * @brief Log entry with full context
 *
 * @param bridge Logging bridge
 * @param entry Complete entry to log
 * @return PR_LOG_SUCCESS or error code
 */
NIMCP_EXPORT pr_log_error_t pr_logging_bridge_log_entry(
    pr_logging_bridge_t bridge,
    const pr_log_entry_t* entry
);

//=============================================================================
// Memory Operation Logging Functions
//=============================================================================

/**
 * @brief Log memory encode operation
 *
 * WHAT: Record memory encoding with full details
 * WHY:  Track when and how memories are created
 *
 * @param bridge Logging bridge
 * @param node Memory node that was encoded
 * @param encoding_time_ns Time taken to encode (nanoseconds)
 * @return PR_LOG_SUCCESS or error code
 *
 * EXAMPLE:
 * ```c
 * pr_logging_bridge_log_encode(bridge, node, 12500);  // 12.5us encoding
 * ```
 */
NIMCP_EXPORT pr_log_error_t pr_logging_bridge_log_encode(
    pr_logging_bridge_t bridge,
    const pr_memory_node_t* node,
    uint64_t encoding_time_ns
);

/**
 * @brief Log memory retrieve operation
 *
 * WHAT: Record memory retrieval with resonance and latency
 * WHY:  Track retrieval patterns and performance
 *
 * @param bridge Logging bridge
 * @param node Memory node that was retrieved
 * @param resonance_score Resonance score of retrieval
 * @param retrieval_time_ns Time taken to retrieve (nanoseconds)
 * @return PR_LOG_SUCCESS or error code
 */
NIMCP_EXPORT pr_log_error_t pr_logging_bridge_log_retrieve(
    pr_logging_bridge_t bridge,
    const pr_memory_node_t* node,
    float resonance_score,
    uint64_t retrieval_time_ns
);

/**
 * @brief Log memory consolidation
 *
 * WHAT: Record consolidation event
 * WHY:  Track memory strengthening over time
 *
 * @param bridge Logging bridge
 * @param node Memory node being consolidated
 * @param old_strength Previous strength
 * @param new_strength New strength
 * @return PR_LOG_SUCCESS or error code
 */
NIMCP_EXPORT pr_log_error_t pr_logging_bridge_log_consolidate(
    pr_logging_bridge_t bridge,
    const pr_memory_node_t* node,
    float old_strength,
    float new_strength
);

/**
 * @brief Log tier promotion
 *
 * @param bridge Logging bridge
 * @param node Memory node promoted
 * @param from_tier Original tier
 * @param to_tier New tier
 * @return PR_LOG_SUCCESS or error code
 */
NIMCP_EXPORT pr_log_error_t pr_logging_bridge_log_promote(
    pr_logging_bridge_t bridge,
    const pr_memory_node_t* node,
    pr_memory_tier_t from_tier,
    pr_memory_tier_t to_tier
);

/**
 * @brief Log tier demotion
 *
 * @param bridge Logging bridge
 * @param node Memory node demoted
 * @param from_tier Original tier
 * @param to_tier New tier
 * @return PR_LOG_SUCCESS or error code
 */
NIMCP_EXPORT pr_log_error_t pr_logging_bridge_log_demote(
    pr_logging_bridge_t bridge,
    const pr_memory_node_t* node,
    pr_memory_tier_t from_tier,
    pr_memory_tier_t to_tier
);

/**
 * @brief Log memory decay
 *
 * @param bridge Logging bridge
 * @param memory_id Memory ID
 * @param old_strength Previous strength
 * @param new_strength New strength after decay
 * @param elapsed_time_ms Time elapsed since last decay
 * @return PR_LOG_SUCCESS or error code
 */
NIMCP_EXPORT pr_log_error_t pr_logging_bridge_log_decay(
    pr_logging_bridge_t bridge,
    uint64_t memory_id,
    float old_strength,
    float new_strength,
    uint64_t elapsed_time_ms
);

/**
 * @brief Log entanglement creation
 *
 * @param bridge Logging bridge
 * @param memory_id_1 First memory
 * @param memory_id_2 Second memory
 * @param resonance Entanglement strength
 * @return PR_LOG_SUCCESS or error code
 */
NIMCP_EXPORT pr_log_error_t pr_logging_bridge_log_entangle(
    pr_logging_bridge_t bridge,
    uint64_t memory_id_1,
    uint64_t memory_id_2,
    float resonance
);

/**
 * @brief Log state change
 *
 * @param bridge Logging bridge
 * @param memory_id Memory ID
 * @param old_state Previous quaternion state
 * @param new_state New quaternion state
 * @param reason Reason for change
 * @return PR_LOG_SUCCESS or error code
 */
NIMCP_EXPORT pr_log_error_t pr_logging_bridge_log_state_change(
    pr_logging_bridge_t bridge,
    uint64_t memory_id,
    nimcp_quaternion_t old_state,
    nimcp_quaternion_t new_state,
    const char* reason
);

/**
 * @brief Log error
 *
 * @param bridge Logging bridge
 * @param category Related category
 * @param error_code Error code
 * @param message Error message
 * @param memory_id Related memory (0 if none)
 * @return PR_LOG_SUCCESS or error code
 */
NIMCP_EXPORT pr_log_error_t pr_logging_bridge_log_error(
    pr_logging_bridge_t bridge,
    pr_log_category_t category,
    int error_code,
    const char* message,
    uint64_t memory_id
);

//=============================================================================
// Export Functions
//=============================================================================

/**
 * @brief Export log entries to file
 *
 * WHAT: Write log entries to file in specified format
 * WHY:  Enable offline analysis and archival
 *
 * @param bridge Logging bridge
 * @param file_path Output file path
 * @param format Export format
 * @param filter Filter criteria (NULL for all)
 * @param result Output: export result
 * @return PR_LOG_SUCCESS or error code
 *
 * EXAMPLE:
 * ```c
 * pr_export_result_t result;
 * pr_logging_bridge_export_trace(bridge, "/tmp/trace.json",
 *     PR_LOG_FORMAT_JSON, NULL, &result);
 * printf("Exported %zu entries\n", result.entries_exported);
 * ```
 */
NIMCP_EXPORT pr_log_error_t pr_logging_bridge_export_trace(
    pr_logging_bridge_t bridge,
    const char* file_path,
    pr_log_format_t format,
    const pr_log_filter_t* filter,
    pr_export_result_t* result
);

/**
 * @brief Export to file handle
 *
 * @param bridge Logging bridge
 * @param file Output file handle
 * @param format Export format
 * @param filter Filter criteria (NULL for all)
 * @param result Output: export result
 * @return PR_LOG_SUCCESS or error code
 */
NIMCP_EXPORT pr_log_error_t pr_logging_bridge_export_to_file(
    pr_logging_bridge_t bridge,
    FILE* file,
    pr_log_format_t format,
    const pr_log_filter_t* filter,
    pr_export_result_t* result
);

/**
 * @brief Export to memory buffer
 *
 * @param bridge Logging bridge
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @param format Export format
 * @param filter Filter criteria (NULL for all)
 * @param bytes_written Output: bytes written
 * @return PR_LOG_SUCCESS or error code
 */
NIMCP_EXPORT pr_log_error_t pr_logging_bridge_export_to_buffer(
    pr_logging_bridge_t bridge,
    char* buffer,
    size_t buffer_size,
    pr_log_format_t format,
    const pr_log_filter_t* filter,
    size_t* bytes_written
);

/**
 * @brief Get estimated export size
 *
 * @param bridge Logging bridge
 * @param format Export format
 * @param filter Filter criteria (NULL for all)
 * @return Estimated size in bytes
 */
NIMCP_EXPORT size_t pr_logging_bridge_estimate_export_size(
    const pr_logging_bridge_t bridge,
    pr_log_format_t format,
    const pr_log_filter_t* filter
);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get entries matching filter
 *
 * @param bridge Logging bridge
 * @param filter Filter criteria
 * @param entries Output array
 * @param max_entries Maximum entries to return
 * @param count Output: actual count returned
 * @return PR_LOG_SUCCESS or error code
 */
NIMCP_EXPORT pr_log_error_t pr_logging_bridge_get_entries(
    const pr_logging_bridge_t bridge,
    const pr_log_filter_t* filter,
    pr_log_entry_t* entries,
    size_t max_entries,
    size_t* count
);

/**
 * @brief Get most recent entries
 *
 * @param bridge Logging bridge
 * @param entries Output array
 * @param max_entries Maximum entries to return
 * @param count Output: actual count returned
 * @return PR_LOG_SUCCESS or error code
 */
NIMCP_EXPORT pr_log_error_t pr_logging_bridge_get_recent(
    const pr_logging_bridge_t bridge,
    pr_log_entry_t* entries,
    size_t max_entries,
    size_t* count
);

/**
 * @brief Count entries matching filter
 *
 * @param bridge Logging bridge
 * @param filter Filter criteria
 * @return Number of matching entries
 */
NIMCP_EXPORT size_t pr_logging_bridge_count_entries(
    const pr_logging_bridge_t bridge,
    const pr_log_filter_t* filter
);

/**
 * @brief Get entry count in buffer
 *
 * @param bridge Logging bridge
 * @return Current number of entries
 */
NIMCP_EXPORT size_t pr_logging_bridge_get_entry_count(
    const pr_logging_bridge_t bridge
);

//=============================================================================
// Statistics Functions
//=============================================================================

/**
 * @brief Get logging statistics
 *
 * @param bridge Logging bridge
 * @param stats Output statistics structure
 * @return PR_LOG_SUCCESS or error code
 */
NIMCP_EXPORT pr_log_error_t pr_logging_bridge_get_stats(
    const pr_logging_bridge_t bridge,
    pr_logging_stats_t* stats
);

/**
 * @brief Reset statistics counters
 *
 * @param bridge Logging bridge
 * @return PR_LOG_SUCCESS or error code
 */
NIMCP_EXPORT pr_log_error_t pr_logging_bridge_reset_stats(
    pr_logging_bridge_t bridge
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get error string for error code
 *
 * @param error Error code
 * @return Human-readable error string
 */
NIMCP_EXPORT const char* pr_log_error_string(pr_log_error_t error);

/**
 * @brief Get log level name
 *
 * @param level Log level
 * @return Human-readable level name
 */
NIMCP_EXPORT const char* pr_log_level_name(pr_log_level_t level);

/**
 * @brief Get log category name
 *
 * @param category Log category
 * @return Human-readable category name
 */
NIMCP_EXPORT const char* pr_log_category_name(pr_log_category_t category);

/**
 * @brief Get format name
 *
 * @param format Export format
 * @return Human-readable format name
 */
NIMCP_EXPORT const char* pr_log_format_name(pr_log_format_t format);

/**
 * @brief Format entry as string
 *
 * @param entry Entry to format
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Pointer to buffer, or NULL on error
 */
NIMCP_EXPORT char* pr_log_entry_to_string(
    const pr_log_entry_t* entry,
    char* buffer,
    size_t buffer_size
);

/**
 * @brief Print entry to stdout
 *
 * @param entry Entry to print
 */
NIMCP_EXPORT void pr_log_entry_print(const pr_log_entry_t* entry);

/**
 * @brief Print bridge summary
 *
 * @param bridge Logging bridge
 */
NIMCP_EXPORT void pr_logging_bridge_print_summary(const pr_logging_bridge_t bridge);

/**
 * @brief Get current time in nanoseconds
 *
 * @return Nanoseconds since epoch (or monotonic start)
 */
NIMCP_EXPORT uint64_t pr_log_current_time_ns(void);

/**
 * @brief Get current time in milliseconds
 *
 * @return Milliseconds since epoch
 */
NIMCP_EXPORT uint64_t pr_log_current_time_ms(void);

/**
 * @brief Validate bridge internal consistency
 *
 * @param bridge Logging bridge
 * @return true if consistent, false if corruption detected
 */
NIMCP_EXPORT bool pr_logging_bridge_validate(const pr_logging_bridge_t bridge);

//=============================================================================
// Convenience Macros
//=============================================================================

/**
 * @brief Log at TRACE level
 */
#define PR_LOG_TRACE(bridge, cat, msg) \
    pr_logging_bridge_log(bridge, PR_LOG_LEVEL_TRACE, cat, msg)

/**
 * @brief Log at DEBUG level
 */
#define PR_LOG_DEBUG(bridge, cat, msg) \
    pr_logging_bridge_log(bridge, PR_LOG_LEVEL_DEBUG, cat, msg)

/**
 * @brief Log at INFO level
 */
#define PR_LOG_INFO(bridge, cat, msg) \
    pr_logging_bridge_log(bridge, PR_LOG_LEVEL_INFO, cat, msg)

/**
 * @brief Log at WARN level
 */
#define PR_LOG_WARN(bridge, cat, msg) \
    pr_logging_bridge_log(bridge, PR_LOG_LEVEL_WARN, cat, msg)

/**
 * @brief Log at ERROR level
 */
#define PR_LOG_ERROR(bridge, cat, msg) \
    pr_logging_bridge_log(bridge, PR_LOG_LEVEL_ERROR, cat, msg)

/**
 * @brief Create category bitmask from single category
 */
#define PR_LOG_CAT_MASK(cat) (1u << (cat))

/**
 * @brief Create category bitmask for all categories
 */
#define PR_LOG_CAT_ALL ((1u << PR_LOG_CAT_COUNT) - 1)

#ifdef __cplusplus
}
#endif

#endif // NIMCP_PR_LOGGING_BRIDGE_H
