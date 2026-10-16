//=============================================================================
// nimcp_logging.h - Enhanced Logging System with Async Support
//=============================================================================
/**
 * @file nimcp_logging.h
 * @brief High-performance logging system with async support, rotation, and filtering
 *
 * WHAT: Comprehensive logging framework with async writes, rotation, and filtering
 * WHY:  Production-ready logging for brain modules with minimal overhead
 * HOW:  Lock-free ring buffer, background writer thread, pluggable destinations
 *
 * ARCHITECTURE:
 *
 *   Producer Threads                   Consumer Thread
 *   ┌─────────────┐                    ┌─────────────────┐
 *   │ LOG_INFO()  │──┐                 │  Async Writer   │
 *   └─────────────┘  │                 │  (background)   │
 *   ┌─────────────┐  │  ┌───────────┐  │                 │
 *   │ LOG_DEBUG() │──┼─>│ Ring      │──>│ ┌─────────────┐│
 *   └─────────────┘  │  │ Buffer    │  │ │ File Output ││
 *   ┌─────────────┐  │  │ (lock-    │  │ └─────────────┘│
 *   │ LOG_ERROR() │──┘  │  free)    │  │ ┌─────────────┐│
 *   └─────────────┘     └───────────┘  │ │Console Out  ││
 *                                      │ └─────────────┘│
 *                                      │ ┌─────────────┐│
 *                                      │ │Syslog Out   ││
 *                                      │ └─────────────┘│
 *                                      └─────────────────┘
 *
 * FEATURES:
 * - Async logging with lock-free ring buffer
 * - Multiple output destinations (file, console, syslog, callback)
 * - Log level filtering (runtime configurable)
 * - Module/category filtering
 * - Automatic log rotation (size and time-based)
 * - Structured logging (JSON format option)
 * - Rate limiting to prevent log flooding
 * - Performance metrics tracking
 * - Context/correlation IDs for request tracing
 * - Source location tracking (__FILE__, __LINE__)
 * - Color output for console
 * - Unified memory integration
 * - Security module registration
 *
 * THREAD SAFETY:
 * - All logging calls are thread-safe
 * - Lock-free fast path for async mode
 * - Blocking only when ring buffer is full
 *
 * @author NIMCP Development Team
 * @date 2025-11-28
 * @version 2.0.0
 */

#ifndef NIMCP_LOGGING_H
#define NIMCP_LOGGING_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>  /* snprintf/printf family — pulled in centrally so */
                    /* cognitive .c files that use these don't have to */
                    /* include <stdio.h> themselves.                    */

#ifdef __cplusplus
extern "C" {
#endif

// Export macro (for shared library builds)
#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

#ifndef PATH_MAX
    #define PATH_MAX 4096
#endif

//=============================================================================
// Constants and Configuration
//=============================================================================

/** @brief Maximum log message length */
#define NIMCP_LOG_MAX_MESSAGE_LEN 4096

/** @brief Maximum module name length */
#define NIMCP_LOG_MAX_MODULE_LEN 64

/** @brief Maximum context ID length */
#define NIMCP_LOG_MAX_CONTEXT_LEN 64

/** @brief Maximum file path length in log entries */
#define NIMCP_LOG_MAX_FILE_LEN 256

/** @brief Default ring buffer size (number of entries) */
#define NIMCP_LOG_DEFAULT_BUFFER_SIZE 8192

/** @brief Default max log file size before rotation (10MB) */
#define NIMCP_LOG_DEFAULT_MAX_FILE_SIZE (10 * 1024 * 1024)

/** @brief Default max rotated files to keep */
#define NIMCP_LOG_DEFAULT_MAX_ROTATED_FILES 5

/** @brief Default flush interval in milliseconds */
#define NIMCP_LOG_DEFAULT_FLUSH_INTERVAL_MS 100

/** @brief Rate limit window in milliseconds */
#define NIMCP_LOG_RATE_LIMIT_WINDOW_MS 1000

/** @brief Default rate limit (messages per window) */
#define NIMCP_LOG_DEFAULT_RATE_LIMIT 1000

/** @brief Magic value for validation */
#define NIMCP_LOG_MAGIC 0x4C4F4747  // 'LOGG'

//=============================================================================
// Types and Enumerations
//=============================================================================

/**
 * @brief Log levels (severity)
 *
 * Levels are ordered from most verbose (TRACE) to most severe (FATAL).
 * Setting the threshold to a level includes all messages at that level
 * and above (more severe).
 */
typedef enum {
    LOG_LEVEL_TRACE = 0,    /**< Most verbose: detailed tracing */
    LOG_LEVEL_DEBUG = 1,    /**< Debug information */
    LOG_LEVEL_INFO = 2,     /**< Informational messages */
    LOG_LEVEL_WARN = 3,     /**< Warning conditions */
    LOG_LEVEL_ERROR = 4,    /**< Error conditions */
    LOG_LEVEL_FATAL = 5,    /**< Critical/fatal errors */
    LOG_LEVEL_OFF = 6,      /**< Disable all logging */
    LOG_LEVEL_COUNT
} log_level_t;

/**
 * @brief Log output format
 */
typedef enum {
    NIMCP_LOG_FORMAT_TEXT = 0,      /**< Human-readable text format */
    NIMCP_LOG_FORMAT_JSON,          /**< Structured JSON format */
    NIMCP_LOG_FORMAT_COMPACT,       /**< Compact text (no timestamps) */
    NIMCP_LOG_FORMAT_SYSLOG         /**< Syslog-compatible format */
} nimcp_log_format_t;

/**
 * @brief Log output destination flags (can be combined)
 */
typedef enum {
    NIMCP_LOG_DEST_NONE = 0,        /**< No output */
    NIMCP_LOG_DEST_FILE = (1 << 0), /**< Output to file */
    NIMCP_LOG_DEST_CONSOLE = (1 << 1), /**< Output to console (stderr) */
    NIMCP_LOG_DEST_SYSLOG = (1 << 2),  /**< Output to syslog */
    NIMCP_LOG_DEST_CALLBACK = (1 << 3), /**< Output to callback function */
    NIMCP_LOG_DEST_ALL = 0xFF       /**< All destinations */
} nimcp_log_dest_t;

/**
 * @brief Log rotation mode
 */
typedef enum {
    NIMCP_LOG_ROTATE_NONE = 0,      /**< No rotation */
    NIMCP_LOG_ROTATE_SIZE,          /**< Rotate when file exceeds size */
    NIMCP_LOG_ROTATE_TIME,          /**< Rotate at time intervals */
    NIMCP_LOG_ROTATE_BOTH           /**< Rotate on size OR time */
} nimcp_log_rotate_mode_t;

/**
 * @brief Time-based rotation interval
 */
typedef enum {
    NIMCP_LOG_ROTATE_HOURLY = 0,
    NIMCP_LOG_ROTATE_DAILY,
    NIMCP_LOG_ROTATE_WEEKLY,
    NIMCP_LOG_ROTATE_MONTHLY
} nimcp_log_rotate_interval_t;

/**
 * @brief Async mode state
 */
typedef enum {
    NIMCP_LOG_ASYNC_OFF = 0,        /**< Synchronous logging */
    NIMCP_LOG_ASYNC_ON,             /**< Async with background thread */
    NIMCP_LOG_ASYNC_HYBRID          /**< Async for INFO/DEBUG, sync for ERROR/FATAL */
} nimcp_log_async_mode_t;

/**
 * @brief Color mode for console output
 */
typedef enum {
    NIMCP_LOG_COLOR_OFF = 0,        /**< No colors */
    NIMCP_LOG_COLOR_AUTO,           /**< Auto-detect terminal support */
    NIMCP_LOG_COLOR_ON              /**< Force colors */
} nimcp_log_color_mode_t;

//=============================================================================
// Opaque Handle Types
//=============================================================================

/**
 * @brief Logger instance handle (opaque)
 */
typedef struct nimcp_logger_struct* nimcp_logger_t;

/**
 * @brief Log context handle for correlation (opaque)
 */
typedef struct nimcp_log_context_struct* nimcp_log_context_t;

//=============================================================================
// Callback Types
//=============================================================================

/**
 * @brief Log message output callback function type
 *
 * This is the new enhanced callback type. The legacy nimcp_log_callback_t
 * type in nimcp_common.h is still supported for backward compatibility.
 *
 * @param level Log level
 * @param timestamp Unix timestamp (seconds)
 * @param module Module name (may be NULL)
 * @param file Source file (may be NULL)
 * @param line Source line
 * @param message Formatted message
 * @param user_data User context
 */
typedef void (*nimcp_log_output_callback_t)(
    log_level_t level,
    uint64_t timestamp,
    const char* module,
    const char* file,
    int line,
    const char* message,
    void* user_data
);

/**
 * @brief Log filter callback function type
 *
 * Return true to allow the message, false to suppress it.
 *
 * @param level Log level
 * @param module Module name
 * @param message Message content
 * @param user_data User context
 * @return true to allow, false to suppress
 */
typedef bool (*nimcp_log_filter_t)(
    log_level_t level,
    const char* module,
    const char* message,
    void* user_data
);

//=============================================================================
// Configuration Structures
//=============================================================================

/**
 * @brief Rotation configuration
 */
typedef struct {
    nimcp_log_rotate_mode_t mode;       /**< Rotation mode */
    size_t max_file_size;               /**< Max size before rotation (bytes) */
    nimcp_log_rotate_interval_t interval; /**< Time interval for rotation */
    uint32_t max_rotated_files;         /**< Max rotated files to keep */
    bool compress_rotated;              /**< Compress rotated files */
} nimcp_log_rotation_config_t;

/**
 * @brief Rate limiting configuration
 */
typedef struct {
    bool enabled;                       /**< Enable rate limiting */
    uint32_t max_per_second;            /**< Max messages per second */
    uint32_t burst_size;                /**< Allowed burst size */
    bool per_module;                    /**< Apply limit per module */
} nimcp_log_rate_limit_config_t;

/**
 * @brief Logger configuration
 */
typedef struct {
    // Basic settings
    log_level_t level;                  /**< Minimum level to log */
    uint32_t destinations;              /**< Output destinations (bitmask) */
    nimcp_log_format_t format;          /**< Output format */

    // File output settings
    const char* file_path;              /**< Log file path (NULL = default) */
    bool append_mode;                   /**< Append to existing file */

    // Async settings
    nimcp_log_async_mode_t async_mode;  /**< Async mode */
    size_t buffer_size;                 /**< Ring buffer size (entries) */
    uint32_t flush_interval_ms;         /**< Flush interval (ms) */

    // Rotation settings
    nimcp_log_rotation_config_t rotation; /**< Rotation configuration */

    // Rate limiting
    nimcp_log_rate_limit_config_t rate_limit; /**< Rate limit configuration */

    // Console settings
    nimcp_log_color_mode_t color_mode;  /**< Color output mode */

    // Callback settings
    nimcp_log_output_callback_t callback;  /**< Output callback */
    void* callback_user_data;           /**< Callback user data */

    // Filter settings
    nimcp_log_filter_t filter;          /**< Filter callback */
    void* filter_user_data;             /**< Filter user data */

    // Source location
    bool include_source_location;       /**< Include file:line in output */

    // Memory integration
    void* memory_manager;               /**< Unified memory manager (optional) */

    // Security integration
    void* security_context;             /**< Security context (optional) */
} nimcp_log_config_t;

/**
 * @brief Log entry structure (for internal use and callbacks)
 */
typedef struct {
    log_level_t level;                  /**< Log level */
    uint64_t timestamp_ns;              /**< Timestamp in nanoseconds */
    uint32_t thread_id;                 /**< Thread ID */
    char module[NIMCP_LOG_MAX_MODULE_LEN]; /**< Module name */
    char context_id[NIMCP_LOG_MAX_CONTEXT_LEN]; /**< Correlation context */
    char file[NIMCP_LOG_MAX_FILE_LEN];  /**< Source file */
    int line;                           /**< Source line */
    char message[NIMCP_LOG_MAX_MESSAGE_LEN]; /**< Formatted message */
    uint32_t sequence;                  /**< Sequence number */
} nimcp_log_entry_t;

/**
 * @brief Logger statistics
 */
typedef struct {
    // Message counts
    uint64_t messages_logged;           /**< Total messages logged */
    uint64_t messages_dropped;          /**< Messages dropped (rate limit/overflow) */
    uint64_t messages_filtered;         /**< Messages filtered out */

    // Per-level counts
    uint64_t level_counts[LOG_LEVEL_COUNT]; /**< Count per level */

    // Async statistics
    uint64_t async_writes;              /**< Async writes performed */
    uint64_t sync_writes;               /**< Sync writes performed */
    uint64_t buffer_overflows;          /**< Ring buffer overflow events */
    uint64_t flush_operations;          /**< Flush operations performed */

    // Rotation statistics
    uint64_t rotations_performed;       /**< Log rotations performed */
    uint64_t bytes_written;             /**< Total bytes written */
    uint64_t current_file_size;         /**< Current file size */

    // Rate limiting statistics
    uint64_t rate_limit_hits;           /**< Rate limit activations */

    // Performance metrics
    uint64_t total_log_time_ns;         /**< Total time spent logging */
    uint64_t max_log_time_ns;           /**< Max single log time */
    uint64_t avg_log_time_ns;           /**< Average log time */

    // Memory statistics
    size_t memory_used;                 /**< Memory used by logger */
    size_t buffer_utilization;          /**< Ring buffer utilization % */

    // System info
    uint64_t uptime_ns;                 /**< Logger uptime */
    uint32_t active_contexts;           /**< Active correlation contexts */
} nimcp_log_stats_t;

//=============================================================================
// Logger Lifecycle API
//=============================================================================

/**
 * @brief Create logger instance
 *
 * WHAT: Creates a new logger with specified configuration
 * WHY:  Flexible logger creation for different use cases
 * HOW:  Allocates resources, starts async thread if needed
 *
 * @param config Configuration (NULL for defaults)
 * @return Logger handle or NULL on failure
 *
 * COMPLEXITY: O(n) where n = buffer_size
 * THREAD SAFETY: Thread-safe
 *
 * EXAMPLE:
 * ```c
 * nimcp_log_config_t config = nimcp_log_default_config();
 * config.level = LOG_LEVEL_DEBUG;
 * config.async_mode = NIMCP_LOG_ASYNC_ON;
 * nimcp_logger_t logger = nimcp_log_create(&config);
 * ```
 */
NIMCP_EXPORT nimcp_logger_t nimcp_log_create(const nimcp_log_config_t* config);

/**
 * @brief Destroy logger instance
 *
 * @param logger Logger handle
 *
 * NOTE: Flushes pending messages before destroying
 */
NIMCP_EXPORT void nimcp_log_destroy(nimcp_logger_t logger);

/**
 * @brief Get default configuration
 *
 * @return Default configuration with sensible values
 */
NIMCP_EXPORT nimcp_log_config_t nimcp_log_default_config(void);

/**
 * @brief Flush pending log messages
 *
 * @param logger Logger handle (NULL for global)
 */
NIMCP_EXPORT void nimcp_log_flush(nimcp_logger_t logger);

/**
 * @brief Check if logger is initialized
 *
 * @param logger Logger handle (NULL for global)
 * @return true if initialized
 */
NIMCP_EXPORT bool nimcp_log_is_initialized(nimcp_logger_t logger);

//=============================================================================
// Global Logger API (Convenience Functions)
//=============================================================================

/**
 * @brief Initialize global logger
 *
 * WHAT: Initialize the default global logger instance
 * WHY:  Simple API for most use cases
 * HOW:  Creates singleton logger with provided or default config
 *
 * @param config Configuration (NULL for defaults)
 * @return 0 on success, -1 on failure
 *
 * EXAMPLE:
 * ```c
 * nimcp_log_config_t config = nimcp_log_default_config();
 * config.file_path = "/var/log/nimcp/brain.log";
 * nimcp_log_init(&config);
 * ```
 */
NIMCP_EXPORT int nimcp_log_init(const nimcp_log_config_t* config);

/**
 * @brief Shutdown global logger
 */
NIMCP_EXPORT void nimcp_log_shutdown(void);

/**
 * @brief Get global logger instance
 *
 * @return Global logger handle (may be NULL if not initialized)
 */
NIMCP_EXPORT nimcp_logger_t nimcp_log_get_global(void);

/**
 * @brief Set global logger instance
 *
 * @param logger Logger to set as global
 */
NIMCP_EXPORT void nimcp_log_set_global(nimcp_logger_t logger);

//=============================================================================
// Legacy Compatibility API
//=============================================================================

/**
 * @brief Initialize logging (legacy API)
 *
 * @param log_file Log file path (NULL for default)
 */
NIMCP_EXPORT void log_init(const char* log_file);

/**
 * @brief Log message (legacy API)
 *
 * @param level Log level
 * @param format Printf-style format
 * @param ... Format arguments
 */
NIMCP_EXPORT void log_message(log_level_t level, const char* format, ...);

/**
 * @brief Close logging (legacy API)
 */
NIMCP_EXPORT void log_close(void);

/**
 * @brief Legacy log function
 *
 * @param level Log level
 * @param format Printf-style format
 * @param ... Format arguments
 */
NIMCP_EXPORT void nimcp_log(log_level_t level, const char* format, ...);

//=============================================================================
// Core Logging API
//=============================================================================

/**
 * @brief Log message with full context
 *
 * WHAT: Core logging function with all parameters
 * WHY:  Maximum flexibility for complex logging needs
 * HOW:  Formats message, applies filters, routes to destinations
 *
 * @param logger Logger handle (NULL for global)
 * @param level Log level
 * @param module Module name (may be NULL)
 * @param file Source file (may be NULL)
 * @param line Source line
 * @param format Printf-style format
 * @param ... Format arguments
 */
NIMCP_EXPORT void nimcp_log_write(
    nimcp_logger_t logger,
    log_level_t level,
    const char* module,
    const char* file,
    int line,
    const char* format,
    ...
);

/**
 * @brief Log message with va_list
 *
 * @param logger Logger handle (NULL for global)
 * @param level Log level
 * @param module Module name
 * @param file Source file
 * @param line Source line
 * @param format Printf-style format
 * @param args Argument list
 */
NIMCP_EXPORT void nimcp_log_writev(
    nimcp_logger_t logger,
    log_level_t level,
    const char* module,
    const char* file,
    int line,
    const char* format,
    va_list args
);

/**
 * @brief Log pre-formatted entry
 *
 * @param logger Logger handle (NULL for global)
 * @param entry Pre-formatted log entry
 */
NIMCP_EXPORT void nimcp_log_entry(
    nimcp_logger_t logger,
    const nimcp_log_entry_t* entry
);

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Set log level threshold
 *
 * @param logger Logger handle (NULL for global)
 * @param level Minimum level to log
 */
NIMCP_EXPORT void nimcp_log_set_level(nimcp_logger_t logger, log_level_t level);

/**
 * @brief Get current log level
 *
 * @param logger Logger handle (NULL for global)
 * @return Current level
 */
NIMCP_EXPORT log_level_t nimcp_log_get_level(nimcp_logger_t logger);

/**
 * @brief Check if level would be logged
 *
 * @param logger Logger handle (NULL for global)
 * @param level Level to check
 * @return true if level would be logged
 */
NIMCP_EXPORT bool nimcp_log_is_level_enabled(nimcp_logger_t logger, log_level_t level);

/**
 * @brief Set output destinations
 *
 * @param logger Logger handle (NULL for global)
 * @param destinations Destination bitmask
 */
NIMCP_EXPORT void nimcp_log_set_destinations(nimcp_logger_t logger, uint32_t destinations);

/**
 * @brief Set output format
 *
 * @param logger Logger handle (NULL for global)
 * @param format Output format
 */
NIMCP_EXPORT void nimcp_log_set_format(nimcp_logger_t logger, nimcp_log_format_t format);

/**
 * @brief Set color mode
 *
 * @param logger Logger handle (NULL for global)
 * @param mode Color mode
 */
NIMCP_EXPORT void nimcp_log_set_color_mode(nimcp_logger_t logger, nimcp_log_color_mode_t mode);

/**
 * @brief Set output callback
 *
 * @param logger Logger handle (NULL for global)
 * @param callback Callback function
 * @param user_data User context
 */
NIMCP_EXPORT void nimcp_log_set_callback(
    nimcp_logger_t logger,
    nimcp_log_output_callback_t callback,
    void* user_data
);

/**
 * @brief Set filter callback
 *
 * @param logger Logger handle (NULL for global)
 * @param filter Filter function
 * @param user_data User context
 */
NIMCP_EXPORT void nimcp_log_set_filter(
    nimcp_logger_t logger,
    nimcp_log_filter_t filter,
    void* user_data
);

//=============================================================================
// Module Filtering API
//=============================================================================

/**
 * @brief Enable logging for specific module
 *
 * @param logger Logger handle (NULL for global)
 * @param module Module name
 * @param level Minimum level for this module
 */
NIMCP_EXPORT void nimcp_log_enable_module(
    nimcp_logger_t logger,
    const char* module,
    log_level_t level
);

/**
 * @brief Disable logging for specific module
 *
 * @param logger Logger handle (NULL for global)
 * @param module Module name
 */
NIMCP_EXPORT void nimcp_log_disable_module(
    nimcp_logger_t logger,
    const char* module
);

/**
 * @brief Clear all module filters
 *
 * @param logger Logger handle (NULL for global)
 */
NIMCP_EXPORT void nimcp_log_clear_module_filters(nimcp_logger_t logger);

//=============================================================================
// Context/Correlation API
//=============================================================================

/**
 * @brief Create logging context for correlation
 *
 * WHAT: Creates a context ID for correlating related log messages
 * WHY:  Track requests/operations across multiple log entries
 * HOW:  Generates unique ID, associates with thread
 *
 * @param logger Logger handle (NULL for global)
 * @param context_id Context ID string (NULL to auto-generate)
 * @return Context handle or NULL on failure
 *
 * EXAMPLE:
 * ```c
 * nimcp_log_context_t ctx = nimcp_log_context_create(NULL, "request-123");
 * // All logs in this thread now include context-id
 * LOG_INFO("Processing request");  // includes context-id
 * nimcp_log_context_destroy(ctx);
 * ```
 */
NIMCP_EXPORT nimcp_log_context_t nimcp_log_context_create(
    nimcp_logger_t logger,
    const char* context_id
);

/**
 * @brief Destroy logging context
 *
 * @param context Context handle
 */
NIMCP_EXPORT void nimcp_log_context_destroy(nimcp_log_context_t context);

/**
 * @brief Get current context ID for thread
 *
 * @param logger Logger handle (NULL for global)
 * @return Context ID string or NULL
 */
NIMCP_EXPORT const char* nimcp_log_get_context_id(nimcp_logger_t logger);

/**
 * @brief Set context ID for current thread
 *
 * @param logger Logger handle (NULL for global)
 * @param context_id Context ID string (NULL to clear)
 */
NIMCP_EXPORT void nimcp_log_set_context_id(nimcp_logger_t logger, const char* context_id);

//=============================================================================
// Rotation API
//=============================================================================

/**
 * @brief Force log rotation
 *
 * @param logger Logger handle (NULL for global)
 * @return 0 on success, -1 on failure
 */
NIMCP_EXPORT int nimcp_log_rotate(nimcp_logger_t logger);

/**
 * @brief Configure rotation
 *
 * @param logger Logger handle (NULL for global)
 * @param config Rotation configuration
 */
NIMCP_EXPORT void nimcp_log_set_rotation(
    nimcp_logger_t logger,
    const nimcp_log_rotation_config_t* config
);

/**
 * @brief Get current log file path
 *
 * @param logger Logger handle (NULL for global)
 * @return Current file path or NULL
 */
NIMCP_EXPORT const char* nimcp_log_get_file_path(nimcp_logger_t logger);

/**
 * @brief Set log file path (triggers rotation)
 *
 * @param logger Logger handle (NULL for global)
 * @param path New file path
 * @return 0 on success, -1 on failure
 */
NIMCP_EXPORT int nimcp_log_set_file_path(nimcp_logger_t logger, const char* path);

//=============================================================================
// Rate Limiting API
//=============================================================================

/**
 * @brief Configure rate limiting
 *
 * @param logger Logger handle (NULL for global)
 * @param config Rate limit configuration
 */
NIMCP_EXPORT void nimcp_log_set_rate_limit(
    nimcp_logger_t logger,
    const nimcp_log_rate_limit_config_t* config
);

/**
 * @brief Reset rate limit counters
 *
 * @param logger Logger handle (NULL for global)
 */
NIMCP_EXPORT void nimcp_log_reset_rate_limit(nimcp_logger_t logger);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get logger statistics
 *
 * @param logger Logger handle (NULL for global)
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
NIMCP_EXPORT int nimcp_log_get_stats(nimcp_logger_t logger, nimcp_log_stats_t* stats);

/**
 * @brief Reset statistics
 *
 * @param logger Logger handle (NULL for global)
 */
NIMCP_EXPORT void nimcp_log_reset_stats(nimcp_logger_t logger);

/**
 * @brief Get security module ID
 *
 * @param logger Logger handle (NULL for global)
 * @return Security module ID or 0 if not registered
 */
NIMCP_EXPORT uint32_t nimcp_log_get_security_id(nimcp_logger_t logger);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get level name string
 *
 * @param level Log level
 * @return Level name
 */
NIMCP_EXPORT const char* nimcp_log_level_name(log_level_t level);

/**
 * @brief Parse level from string
 *
 * @param name Level name (e.g., "DEBUG", "info")
 * @return Log level or LOG_LEVEL_INFO on error
 */
NIMCP_EXPORT log_level_t nimcp_log_level_from_string(const char* name);

/**
 * @brief Get format name string
 *
 * @param format Format enum
 * @return Format name
 */
NIMCP_EXPORT const char* nimcp_log_format_name(nimcp_log_format_t format);

/**
 * @brief Check if stdout/stderr is a TTY (for color detection)
 *
 * @return true if TTY
 */
NIMCP_EXPORT bool nimcp_log_is_tty(void);

//=============================================================================
// Convenience Logging Macros
//=============================================================================

/**
 * @brief Macros for logging with source location
 *
 * These macros automatically include file and line information.
 * They use the global logger instance.
 */

#define LOG_TRACE(...) \
    nimcp_log_write(NULL, LOG_LEVEL_TRACE, NULL, __FILE__, __LINE__, __VA_ARGS__)

#define LOG_DEBUG(...) \
    nimcp_log_write(NULL, LOG_LEVEL_DEBUG, NULL, __FILE__, __LINE__, __VA_ARGS__)

#define LOG_INFO(...) \
    nimcp_log_write(NULL, LOG_LEVEL_INFO, NULL, __FILE__, __LINE__, __VA_ARGS__)

#define LOG_WARNING(...) \
    nimcp_log_write(NULL, LOG_LEVEL_WARN, NULL, __FILE__, __LINE__, __VA_ARGS__)

#define LOG_WARN(...) \
    nimcp_log_write(NULL, LOG_LEVEL_WARN, NULL, __FILE__, __LINE__, __VA_ARGS__)

#define LOG_ERROR(...) \
    nimcp_log_write(NULL, LOG_LEVEL_ERROR, NULL, __FILE__, __LINE__, __VA_ARGS__)

#define LOG_FATAL(...) \
    nimcp_log_write(NULL, LOG_LEVEL_FATAL, NULL, __FILE__, __LINE__, __VA_ARGS__)

/**
 * @brief Module-specific logging macros
 *
 * Usage: LOG_MODULE_INFO("brain", "Initialized with %d neurons", count);
 */

#define LOG_MODULE_TRACE(module, ...) \
    nimcp_log_write(NULL, LOG_LEVEL_TRACE, module, __FILE__, __LINE__, __VA_ARGS__)

#define LOG_MODULE_DEBUG(module, ...) \
    nimcp_log_write(NULL, LOG_LEVEL_DEBUG, module, __FILE__, __LINE__, __VA_ARGS__)

#define LOG_MODULE_INFO(module, ...) \
    nimcp_log_write(NULL, LOG_LEVEL_INFO, module, __FILE__, __LINE__, __VA_ARGS__)

#define LOG_MODULE_WARN(module, ...) \
    nimcp_log_write(NULL, LOG_LEVEL_WARN, module, __FILE__, __LINE__, __VA_ARGS__)

#define LOG_MODULE_ERROR(module, ...) \
    nimcp_log_write(NULL, LOG_LEVEL_ERROR, module, __FILE__, __LINE__, __VA_ARGS__)

#define LOG_MODULE_FATAL(module, ...) \
    nimcp_log_write(NULL, LOG_LEVEL_FATAL, module, __FILE__, __LINE__, __VA_ARGS__)

/**
 * @brief Conditional logging macros (only evaluate args if level enabled)
 */

#define LOG_IF(level, ...) \
    do { \
        if (nimcp_log_is_level_enabled(NULL, level)) { \
            nimcp_log_write(NULL, level, NULL, __FILE__, __LINE__, __VA_ARGS__); \
        } \
    } while(0)

#define LOG_DEBUG_IF(cond, ...) \
    do { \
        if ((cond) && nimcp_log_is_level_enabled(NULL, LOG_LEVEL_DEBUG)) { \
            LOG_DEBUG(__VA_ARGS__); \
        } \
    } while(0)

/**
 * @brief Rate-limited logging macros
 *
 * Only log once per N calls to prevent log flooding.
 */

#define LOG_EVERY_N(level, n, ...) \
    do { \
        static uint32_t _log_count = 0; \
        if (++_log_count % (n) == 1) { \
            nimcp_log_write(NULL, level, NULL, __FILE__, __LINE__, __VA_ARGS__); \
        } \
    } while(0)

#define LOG_FIRST_N(level, n, ...) \
    do { \
        static uint32_t _log_count = 0; \
        if (_log_count < (n)) { \
            ++_log_count; \
            nimcp_log_write(NULL, level, NULL, __FILE__, __LINE__, __VA_ARGS__); \
        } \
    } while(0)

/**
 * @brief Long-form macros (NIMCP_LOGGING_ prefix) for compatibility
 */

#define NIMCP_LOGGING_TRACE(...) LOG_TRACE(__VA_ARGS__)
#define NIMCP_LOGGING_DEBUG(...) LOG_DEBUG(__VA_ARGS__)
#define NIMCP_LOGGING_INFO(...)  LOG_INFO(__VA_ARGS__)
#define NIMCP_LOGGING_WARN(...)  LOG_WARN(__VA_ARGS__)
#define NIMCP_LOGGING_ERROR(...) LOG_ERROR(__VA_ARGS__)
#define NIMCP_LOGGING_FATAL(...) LOG_FATAL(__VA_ARGS__)

/**
 * @brief Short-form macros (NIMCP_LOG_ prefix) for convenience
 */

#define NIMCP_LOG_TRACE(...) LOG_TRACE(__VA_ARGS__)
#define NIMCP_LOG_DEBUG(...) LOG_DEBUG(__VA_ARGS__)
#define NIMCP_LOG_INFO(...)  LOG_INFO(__VA_ARGS__)
#define NIMCP_LOG_WARN(...)  LOG_WARN(__VA_ARGS__)
#define NIMCP_LOG_ERROR(...) LOG_ERROR(__VA_ARGS__)
#define NIMCP_LOG_FATAL(...) LOG_FATAL(__VA_ARGS__)

/**
 * @brief Assertion-style logging
 */

#define LOG_ASSERT(cond, ...) \
    do { \
        if (!(cond)) { \
            LOG_FATAL("Assertion failed: " #cond " - " __VA_ARGS__); \
        } \
    } while(0)

/**
 * @brief Function-style logging macros with module ID
 *
 * These macros provide compatibility with code that uses function-style
 * logging calls with numeric module IDs. The module_id is prepended to
 * the log message as [0xXXXX].
 *
 * Usage: nimcp_log_debug(0x1234, "message %d", value);
 */
#define nimcp_log_debug(module_id, fmt, ...) \
    nimcp_log_write(NULL, LOG_LEVEL_DEBUG, NULL, __FILE__, __LINE__, \
                    "[0x%04X] " fmt, (unsigned int)(uintptr_t)(module_id), ##__VA_ARGS__)

#define nimcp_log_info(module_id, fmt, ...) \
    nimcp_log_write(NULL, LOG_LEVEL_INFO, NULL, __FILE__, __LINE__, \
                    "[0x%04X] " fmt, (unsigned int)(uintptr_t)(module_id), ##__VA_ARGS__)

#define nimcp_log_warning(module_id, fmt, ...) \
    nimcp_log_write(NULL, LOG_LEVEL_WARN, NULL, __FILE__, __LINE__, \
                    "[0x%04X] " fmt, (unsigned int)(uintptr_t)(module_id), ##__VA_ARGS__)

#define nimcp_log_warn(module_id, fmt, ...) \
    nimcp_log_write(NULL, LOG_LEVEL_WARN, NULL, __FILE__, __LINE__, \
                    "[0x%04X] " fmt, (unsigned int)(uintptr_t)(module_id), ##__VA_ARGS__)

#define nimcp_log_error(module_id, fmt, ...) \
    nimcp_log_write(NULL, LOG_LEVEL_ERROR, NULL, __FILE__, __LINE__, \
                    "[0x%04X] " fmt, (unsigned int)(uintptr_t)(module_id), ##__VA_ARGS__)

/**
 * @brief Get a named logger (alias for nimcp_log_get_global for compatibility)
 * @param name Logger name (currently ignored)
 * @return Global logger
 */
static inline nimcp_logger_t nimcp_logger_get(const char* name) {
    (void)name;
    return nimcp_log_get_global();
}

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_LOGGING_H
