/**
 * @file nimcp_pattern_db.h
 * @brief Runtime-Updateable Pattern Database for Threat Detection
 *
 * WHAT: Dynamic pattern database for detecting security threats in real-time
 * WHY:  Enables adaptive security responses without system restart
 * HOW:  Lock-free concurrent access, versioned updates, atomic rollback
 *
 * FEATURES:
 * - Runtime pattern addition/removal without restart
 * - Pattern versioning with rollback support
 * - Atomic batch updates (all-or-nothing)
 * - Thread-safe lock-free reads
 * - Pattern categories (SQL injection, XSS, shell injection, etc.)
 * - Priority-based pattern matching
 * - Hot-reload from file/network
 * - Integration with bio-async messaging
 *
 * ARCHITECTURE:
 * ┌─────────────────────────────────────────────────────────────┐
 * │                    Pattern Database                         │
 * ├─────────────────────────────────────────────────────────────┤
 * │  ┌─────────────┐    ┌──────────────┐    ┌────────────────┐ │
 * │  │   Patterns  │───▶│  Compiler    │───▶│  Matcher       │ │
 * │  │   (Text)    │    │  (Regex/DFA) │    │  (Lock-free)   │ │
 * │  └─────────────┘    └──────────────┘    └────────────────┘ │
 * │         │                                        │          │
 * │         │           ┌──────────────┐             │          │
 * │         └──────────▶│   Versions   │◀────────────┘          │
 * │                     │   (Snapshots)│                        │
 * │                     └──────────────┘                        │
 * └─────────────────────────────────────────────────────────────┘
 *
 * USAGE:
 * ```c
 * // Create pattern database
 * nimcp_pattern_db_config_t config = nimcp_pattern_db_default_config();
 * nimcp_pattern_db_t db = nimcp_pattern_db_create(&config);
 *
 * // Add patterns
 * nimcp_pattern_entry_t entry = {
 *     .pattern = "(?i)(union|select).*from",
 *     .category = NIMCP_PATTERN_SQL_INJECTION,
 *     .priority = 10,
 *     .weight = 1.0f,
 *     .description = "SQL injection attempt"
 * };
 * nimcp_pattern_id_t id;
 * nimcp_pattern_db_add(db, &entry, &id);
 *
 * // Match against input
 * nimcp_pattern_match_result_t result;
 * nimcp_pattern_db_match(db, user_input, &result);
 * if (result.matched) {
 *     printf("Threat detected: %s\n", result.description);
 * }
 *
 * // Rollback on bad update
 * uint32_t version = nimcp_pattern_db_version(db);
 * // ... add new patterns ...
 * if (something_wrong) {
 *     nimcp_pattern_db_rollback(db, version);  // Atomic rollback
 * }
 * ```
 *
 * @author NIMCP Security Team
 * @date 2025-12-07
 * @version 1.0.0
 */

#ifndef NIMCP_PATTERN_DB_H
#define NIMCP_PATTERN_DB_H

#include "utils/validation/nimcp_common.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define NIMCP_PATTERN_DB_MAGIC 0x50415444        /**< 'PATD' */
#define NIMCP_PATTERN_DB_VERSION 1               /**< Database schema version */
#define NIMCP_PATTERN_MAX_LENGTH 1024            /**< Maximum pattern length */
#define NIMCP_PATTERN_MAX_DESCRIPTION 256        /**< Maximum description length */
#define NIMCP_PATTERN_MAX_CATEGORIES 16          /**< Maximum pattern categories */
#define NIMCP_PATTERN_DEFAULT_CAPACITY 1024      /**< Default pattern capacity */
#define NIMCP_PATTERN_MAX_SNAPSHOTS 16           /**< Maximum version snapshots */

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Opaque pattern database handle
 */
typedef struct nimcp_pattern_db_impl* nimcp_pattern_db_t;

/**
 * @brief Pattern identifier (unique within database)
 */
typedef uint32_t nimcp_pattern_id_t;

#define NIMCP_PATTERN_ID_INVALID 0

/**
 * @brief Pattern category enumeration
 *
 * WHAT: Categories of security patterns for classification
 * WHY:  Different attack vectors require different detection approaches
 * HOW:  Patterns tagged with category for filtering and statistics
 */
typedef enum {
    NIMCP_PATTERN_SQL_INJECTION = 0,     /**< SQL injection (UNION, SELECT, etc.) */
    NIMCP_PATTERN_XSS,                   /**< Cross-site scripting (<script>, javascript:) */
    NIMCP_PATTERN_SHELL_INJECTION,       /**< Shell command injection (;, |, &&) */
    NIMCP_PATTERN_PATH_TRAVERSAL,        /**< Directory traversal (../, etc.) */
    NIMCP_PATTERN_FORMAT_STRING,         /**< Format string attacks (%s, %n) */
    NIMCP_PATTERN_PROMPT_INJECTION,      /**< LLM prompt injection */
    NIMCP_PATTERN_BUFFER_OVERFLOW,       /**< Buffer overflow patterns */
    NIMCP_PATTERN_LDAP_INJECTION,        /**< LDAP injection */
    NIMCP_PATTERN_XML_INJECTION,         /**< XML/XXE injection */
    NIMCP_PATTERN_COMMAND_INJECTION,     /**< OS command injection */
    NIMCP_PATTERN_CUSTOM,                /**< User-defined custom patterns */
    NIMCP_PATTERN_CATEGORY_COUNT
} nimcp_pattern_category_t;

/**
 * @brief Pattern entry structure
 *
 * WHAT: Complete specification of a security pattern
 * WHY:  Encapsulates all metadata needed for matching and reporting
 * HOW:  Compiled into internal representation on addition
 */
typedef struct {
    const char* pattern;                 /**< Regex pattern (PCRE2 compatible) */
    nimcp_pattern_category_t category;   /**< Pattern category */
    uint32_t priority;                   /**< Match priority (higher = checked first) */
    float weight;                        /**< Threat weight (0.0-1.0) */
    const char* description;             /**< Human-readable description */
    uint32_t flags;                      /**< Pattern flags (case-insensitive, etc.) */
} nimcp_pattern_entry_t;

/**
 * @brief Pattern flags
 */
#define NIMCP_PATTERN_FLAG_CASE_INSENSITIVE  (1 << 0)  /**< Case-insensitive match */
#define NIMCP_PATTERN_FLAG_MULTILINE         (1 << 1)  /**< Multiline mode */
#define NIMCP_PATTERN_FLAG_DOTALL            (1 << 2)  /**< Dot matches newline */
#define NIMCP_PATTERN_FLAG_EXTENDED          (1 << 3)  /**< Extended regex syntax */
#define NIMCP_PATTERN_FLAG_UTF8              (1 << 4)  /**< UTF-8 input */

/**
 * @brief Pattern match result
 *
 * WHAT: Result of matching input against pattern database
 * WHY:  Provides detailed information about threat detected
 * HOW:  Populated by matching engine with best match
 */
typedef struct {
    bool matched;                        /**< True if pattern matched */
    nimcp_pattern_id_t pattern_id;       /**< ID of matched pattern */
    nimcp_pattern_category_t category;   /**< Category of matched pattern */
    float threat_score;                  /**< Aggregated threat score (0.0-1.0) */
    uint32_t match_count;                /**< Number of patterns matched */
    char description[NIMCP_PATTERN_MAX_DESCRIPTION];  /**< Description of threat */
    size_t match_offset;                 /**< Offset of match in input */
    size_t match_length;                 /**< Length of matched substring */
} nimcp_pattern_match_result_t;

/**
 * @brief Pattern database statistics
 */
typedef struct {
    uint32_t total_patterns;             /**< Total patterns in database */
    uint32_t patterns_by_category[NIMCP_PATTERN_CATEGORY_COUNT];  /**< Per-category counts */
    uint32_t current_version;            /**< Current database version */
    uint32_t total_matches;              /**< Total matches performed */
    uint32_t total_hits;                 /**< Total successful matches */
    float avg_match_time_us;             /**< Average match time (microseconds) */
    float max_match_time_us;             /**< Maximum match time */
    size_t memory_usage_bytes;           /**< Estimated memory usage */
    uint32_t snapshot_count;             /**< Number of version snapshots */
} nimcp_pattern_db_stats_t;

/**
 * @brief Pattern database configuration
 */
typedef struct {
    uint32_t initial_capacity;           /**< Initial pattern capacity */
    uint32_t max_patterns;               /**< Maximum patterns (0 = unlimited) */
    uint32_t max_snapshots;              /**< Maximum version snapshots */
    bool enable_statistics;              /**< Track matching statistics */
    bool enable_validation;              /**< Validate patterns before adding */
    bool enable_bio_async;               /**< Enable bio-async integration */
    bio_module_id_t module_id;           /**< Module ID for bio-async */
    float match_timeout_ms;              /**< Per-match timeout */
    uint32_t worker_threads;             /**< Number of worker threads (0 = auto) */
} nimcp_pattern_db_config_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Get default pattern database configuration
 *
 * WHAT: Returns sensible default configuration
 * WHY:  Simplifies initialization for common use cases
 * HOW:  Static defaults tuned for typical workloads
 *
 * @return Default configuration structure
 */
nimcp_pattern_db_config_t nimcp_pattern_db_default_config(void);

/**
 * @brief Create pattern database
 *
 * WHAT: Initialize new pattern database instance
 * WHY:  Required before any pattern operations
 * HOW:  Allocates memory, initializes lock-free structures
 *
 * @param config Configuration (NULL for defaults)
 * @return Database handle or NULL on failure
 */
nimcp_pattern_db_t nimcp_pattern_db_create(const nimcp_pattern_db_config_t* config);

/**
 * @brief Destroy pattern database
 *
 * WHAT: Free all resources associated with database
 * WHY:  Prevent memory leaks
 * HOW:  Releases patterns, snapshots, statistics
 *
 * @param db Database handle
 */
void nimcp_pattern_db_destroy(nimcp_pattern_db_t db);

//=============================================================================
// Pattern Management
//=============================================================================

/**
 * @brief Add pattern to database
 *
 * WHAT: Add new pattern with automatic compilation
 * WHY:  Enable runtime threat detection updates
 * HOW:  Compiles pattern, validates, inserts atomically
 *
 * @param db Database handle
 * @param entry Pattern entry specification
 * @param id Output: assigned pattern ID (can be NULL)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_pattern_db_add(
    nimcp_pattern_db_t db,
    const nimcp_pattern_entry_t* entry,
    nimcp_pattern_id_t* id
);

/**
 * @brief Remove pattern from database
 *
 * WHAT: Remove pattern by ID
 * WHY:  Allow retiring outdated or ineffective patterns
 * HOW:  Marks pattern as deleted, reclaimed on next version
 *
 * @param db Database handle
 * @param id Pattern ID to remove
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_pattern_db_remove(
    nimcp_pattern_db_t db,
    nimcp_pattern_id_t id
);

/**
 * @brief Update existing pattern
 *
 * WHAT: Replace pattern definition atomically
 * WHY:  Refine patterns without remove/add cycle
 * HOW:  Compiles new version, swaps atomically
 *
 * @param db Database handle
 * @param id Pattern ID to update
 * @param entry New pattern definition
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_pattern_db_update(
    nimcp_pattern_db_t db,
    nimcp_pattern_id_t id,
    const nimcp_pattern_entry_t* entry
);

/**
 * @brief Get pattern entry by ID
 *
 * WHAT: Retrieve pattern definition
 * WHY:  Inspect current pattern state
 * HOW:  Lock-free read of pattern metadata
 *
 * @param db Database handle
 * @param id Pattern ID
 * @param entry Output: pattern entry (caller provides buffer)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_pattern_db_get(
    nimcp_pattern_db_t db,
    nimcp_pattern_id_t id,
    nimcp_pattern_entry_t* entry
);

//=============================================================================
// Bulk Operations
//=============================================================================

/**
 * @brief Import multiple patterns atomically
 *
 * WHAT: Add multiple patterns in single transaction
 * WHY:  Ensure consistent database state
 * HOH:  All patterns added or none (rollback on error)
 *
 * @param db Database handle
 * @param entries Array of pattern entries
 * @param count Number of entries
 * @return NIMCP_SUCCESS or error code (all rolled back on error)
 */
nimcp_error_t nimcp_pattern_db_import(
    nimcp_pattern_db_t db,
    const nimcp_pattern_entry_t* entries,
    size_t count
);

/**
 * @brief Load patterns from file
 *
 * WHAT: Load pattern database from JSON file
 * WHY:  Enable persistent pattern storage
 * HOW:  Parses JSON, validates, imports atomically
 *
 * @param db Database handle
 * @param filepath Path to JSON pattern file
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_pattern_db_load(
    nimcp_pattern_db_t db,
    const char* filepath
);

/**
 * @brief Save patterns to file
 *
 * WHAT: Export current pattern database to JSON
 * WHY:  Enable backup and transfer of patterns
 * HOW:  Serializes all active patterns to file
 *
 * @param db Database handle
 * @param filepath Output file path
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_pattern_db_save(
    nimcp_pattern_db_t db,
    const char* filepath
);

/**
 * @brief Clear all patterns
 *
 * WHAT: Remove all patterns from database
 * WHY:  Reset database to clean state
 * HOW:  Atomic clear with version increment
 *
 * @param db Database handle
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_pattern_db_clear(nimcp_pattern_db_t db);

//=============================================================================
// Versioning and Rollback
//=============================================================================

/**
 * @brief Get current database version
 *
 * WHAT: Return current version number
 * WHY:  Track database changes over time
 * HOW:  Version increments on any modification
 *
 * @param db Database handle
 * @return Current version number (0 if invalid)
 */
uint32_t nimcp_pattern_db_version(nimcp_pattern_db_t db);

/**
 * @brief Create snapshot of current database state
 *
 * WHAT: Save current database state for later rollback
 * WHY:  Enable undo of bad pattern updates
 * HOW:  Copy-on-write snapshot with reference counting
 *
 * @param db Database handle
 * @param snapshot_id Output: snapshot identifier
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_pattern_db_snapshot(
    nimcp_pattern_db_t db,
    uint32_t* snapshot_id
);

/**
 * @brief Rollback to previous version
 *
 * WHAT: Atomically restore database to previous version
 * WHY:  Undo problematic pattern updates
 * HOW:  Swaps to snapshot, increments version
 *
 * @param db Database handle
 * @param version Target version number
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_pattern_db_rollback(
    nimcp_pattern_db_t db,
    uint32_t version
);

//=============================================================================
// Pattern Matching
//=============================================================================

/**
 * @brief Match input against all patterns
 *
 * WHAT: Check input string against pattern database
 * WHY:  Core threat detection function
 * HOW:  Lock-free parallel matching with priority ordering
 *
 * BIOLOGICAL INTEGRATION:
 * - Uses dopamine channel for completion signaling
 * - Generates prediction error on unexpected matches
 * - Integrates with bio-async for event notification
 *
 * @param db Database handle
 * @param input Input string to check
 * @param result Output: match result
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_pattern_db_match(
    nimcp_pattern_db_t db,
    const char* input,
    nimcp_pattern_match_result_t* result
);

/**
 * @brief Match input against specific category
 *
 * WHAT: Check input against patterns in category
 * WHY:  Optimize matching for known attack vector
 * HOW:  Filters to category before matching
 *
 * @param db Database handle
 * @param input Input string to check
 * @param category Category to match against
 * @param result Output: match result
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_pattern_db_match_category(
    nimcp_pattern_db_t db,
    const char* input,
    nimcp_pattern_category_t category,
    nimcp_pattern_match_result_t* result
);

/**
 * @brief Match with custom timeout
 *
 * WHAT: Match with per-call timeout override
 * WHY:  Handle variable-length inputs efficiently
 * HOW:  Aborts matching after timeout
 *
 * @param db Database handle
 * @param input Input string
 * @param timeout_ms Timeout in milliseconds
 * @param result Output: match result
 * @return NIMCP_SUCCESS, NIMCP_TIMEOUT, or error code
 */
nimcp_error_t nimcp_pattern_db_match_timeout(
    nimcp_pattern_db_t db,
    const char* input,
    float timeout_ms,
    nimcp_pattern_match_result_t* result
);

//=============================================================================
// Statistics and Monitoring
//=============================================================================

/**
 * @brief Get database statistics
 *
 * WHAT: Retrieve comprehensive database statistics
 * WHY:  Monitor performance and effectiveness
 * HOW:  Aggregates counters and timing information
 *
 * @param db Database handle
 * @param stats Output: statistics structure
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_pattern_db_get_stats(
    nimcp_pattern_db_t db,
    nimcp_pattern_db_stats_t* stats
);

/**
 * @brief Reset statistics counters
 *
 * WHAT: Clear all statistics to zero
 * WHY:  Enable fresh measurement period
 * HOW:  Atomic reset of all counters
 *
 * @param db Database handle
 */
void nimcp_pattern_db_reset_stats(nimcp_pattern_db_t db);

/**
 * @brief Get category name as string
 *
 * WHAT: Convert category enum to string
 * WHY:  Human-readable logging and reporting
 * HOW:  Static string lookup
 *
 * @param category Pattern category
 * @return Category name string
 */
const char* nimcp_pattern_category_name(nimcp_pattern_category_t category);

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * @brief Process bio-async inbox messages
 *
 * WHAT: Handle incoming bio-async messages for pattern database
 * WHY:  Enable remote pattern updates and queries
 * HOW:  Processes messages from module inbox
 *
 * MESSAGE TYPES:
 * - PATTERN_ADD: Add new pattern
 * - PATTERN_REMOVE: Remove pattern
 * - PATTERN_UPDATE: Update pattern
 * - PATTERN_RELOAD: Reload from file
 * - PATTERN_ROLLBACK: Rollback to version
 *
 * @param db Database handle
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed
 */
uint32_t nimcp_pattern_db_process_inbox(
    nimcp_pattern_db_t db,
    uint32_t max_messages
);

/**
 * @brief Register pattern database with bio-async router
 *
 * WHAT: Register module with central bio-async router
 * WHY:  Enable inter-module communication
 * HOW:  Registers handlers for pattern management messages
 *
 * @param db Database handle
 * @param module_id Module ID for registration
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_pattern_db_register_bio_async(
    nimcp_pattern_db_t db,
    bio_module_id_t module_id
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PATTERN_DB_H */
