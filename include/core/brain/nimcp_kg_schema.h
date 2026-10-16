/**
 * @file nimcp_kg_schema.h
 * @brief Schema Evolution and Migration for Brain Knowledge Graph
 * @version 1.0.0
 * @date 2026-01-16
 *
 * WHAT: Schema versioning and migration infrastructure for brain KG
 * WHY:  Enable safe evolution of KG structure over time with rollback capability
 * HOW:  Version tracking, migration scripts, execution engine with history
 *
 * ARCHITECTURE:
 * ```
 * +=========================================================================+
 * |                    KG SCHEMA EVOLUTION SYSTEM                           |
 * +=========================================================================+
 * |                                                                         |
 * |   +-------------------+     +-------------------+     +---------------+ |
 * |   | Schema Version    |     | Migration Scripts |     | Migration     | |
 * |   | (major.minor.     |---->| (up/down scripts  |---->| Executor      | |
 * |   |  patch-label)     |     |  per version)     |     | (apply/roll)  | |
 * |   +-------------------+     +-------------------+     +---------------+ |
 * |           |                         |                        |         |
 * |           v                         v                        v         |
 * |   +-------------------+     +-------------------+     +---------------+ |
 * |   | Version Compare   |     | Script Registry   |     | Migration     | |
 * |   | (semantic ver)    |     | (ordered list)    |     | History       | |
 * |   +-------------------+     +-------------------+     +---------------+ |
 * |                                                                         |
 * +=========================================================================+
 * ```
 *
 * USAGE:
 * ```c
 * // Get current schema version
 * kg_schema_version_t current = kg_schema_get_current(kg);
 *
 * // Register migration
 * kg_migration_script_t migration = {
 *     .from_version = {1, 0, 0, ""},
 *     .to_version = {1, 1, 0, ""},
 *     .description = "Add hemisphere field to nodes",
 *     .up_script = "ALTER TABLE nodes ADD COLUMN hemisphere INT",
 *     .down_script = "ALTER TABLE nodes DROP COLUMN hemisphere",
 *     .is_reversible = true,
 *     .estimated_duration_sec = 5
 * };
 * kg_schema_register_migration(&migration);
 *
 * // Execute migration
 * kg_migration_result_t result;
 * kg_schema_migrate_up(kg, &result);
 * ```
 *
 * THREAD SAFETY: All operations are thread-safe via internal mutex
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_KG_SCHEMA_H
#define NIMCP_KG_SCHEMA_H

#include "core/brain/nimcp_brain_kg.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

/** Maximum length of schema version label */
#define KG_SCHEMA_MAX_LABEL_LEN         32

/** Maximum length of migration description */
#define KG_SCHEMA_MAX_DESC_LEN          256

/** Maximum length of error message */
#define KG_SCHEMA_MAX_ERROR_LEN         512

/** Maximum registered migrations */
#define KG_SCHEMA_MAX_MIGRATIONS        256

/** Maximum migration history entries */
#define KG_SCHEMA_MAX_HISTORY           1024

/* ============================================================================
 * DATA STRUCTURES
 * ============================================================================ */

/**
 * @brief Schema version (semantic versioning)
 *
 * WHAT: Identifies a specific schema version
 * WHY:  Enable precise version comparison and migration targeting
 * HOW:  Semantic versioning with optional label for pre-release/beta
 *
 * Version comparison rules:
 * - major: Breaking changes (incompatible schema changes)
 * - minor: Backward-compatible additions (new fields, new node types)
 * - patch: Bug fixes (no schema changes)
 * - label: Optional qualifier (e.g., "beta", "rc1")
 */
typedef struct {
    uint32_t major;                      /**< Breaking changes */
    uint32_t minor;                      /**< Backward-compatible additions */
    uint32_t patch;                      /**< Bug fixes */
    char label[KG_SCHEMA_MAX_LABEL_LEN]; /**< Optional label (e.g., "beta") */
} kg_schema_version_t;

/**
 * @brief Migration direction
 *
 * WHAT: Direction of schema migration
 * WHY:  Support both upgrades and rollbacks
 * HOW:  Enum for up/down migration selection
 */
typedef enum {
    KG_MIGRATE_UP = 0,                   /**< Upgrade to newer version */
    KG_MIGRATE_DOWN                      /**< Rollback to older version */
} kg_migration_direction_t;

/**
 * @brief Migration status
 *
 * WHAT: Current state of a migration operation
 * WHY:  Track migration progress and detect failures
 * HOW:  State machine for migration lifecycle
 */
typedef enum {
    KG_MIGRATE_PENDING = 0,              /**< Migration not yet started */
    KG_MIGRATE_IN_PROGRESS,              /**< Migration currently executing */
    KG_MIGRATE_COMPLETED,                /**< Migration finished successfully */
    KG_MIGRATE_FAILED,                   /**< Migration failed with error */
    KG_MIGRATE_ROLLED_BACK               /**< Migration was rolled back */
} kg_migration_status_t;

/**
 * @brief Migration script
 *
 * WHAT: Definition of a schema migration between two versions
 * WHY:  Encapsulate upgrade/downgrade logic with metadata
 * HOW:  Contains both up and down scripts for reversibility
 *
 * Scripts can be:
 * - SQL-like commands for structured changes
 * - Custom transformation logic (function pointers optional)
 * - Declarative node/edge type definitions
 */
typedef struct {
    kg_schema_version_t from_version;    /**< Source version */
    kg_schema_version_t to_version;      /**< Target version */
    char description[KG_SCHEMA_MAX_DESC_LEN]; /**< Human-readable description */
    char* up_script;                     /**< SQL/commands for upgrade */
    char* down_script;                   /**< SQL/commands for rollback */
    bool is_reversible;                  /**< Can be rolled back */
    uint32_t estimated_duration_sec;     /**< Estimated execution time */
} kg_migration_script_t;

/**
 * @brief Migration result
 *
 * WHAT: Outcome of a migration operation
 * WHY:  Report success/failure with detailed metrics
 * HOW:  Captures timing, row counts, and error details
 */
typedef struct {
    kg_migration_status_t status;        /**< Final status */
    kg_schema_version_t from_version;    /**< Version before migration */
    kg_schema_version_t to_version;      /**< Version after migration */
    uint64_t started_at;                 /**< Start timestamp (ms since epoch) */
    uint64_t completed_at;               /**< Completion timestamp (ms) */
    uint64_t duration_ms;                /**< Total duration in milliseconds */
    char error_message[KG_SCHEMA_MAX_ERROR_LEN]; /**< Error details if failed */
    uint32_t rows_affected;              /**< Number of nodes/edges modified */
} kg_migration_result_t;

/* ============================================================================
 * SCHEMA MANAGEMENT API
 * ============================================================================ */

/**
 * @brief Get current schema version of a knowledge graph
 *
 * WHAT: Retrieve the current schema version from a KG
 * WHY:  Determine what migrations need to be applied
 * HOW:  Read version metadata stored in KG
 *
 * @param kg Knowledge graph handle
 * @return Current schema version (zeroed struct if not set)
 */
kg_schema_version_t kg_schema_get_current(const brain_kg_t* kg);

/**
 * @brief Set schema version for a knowledge graph
 *
 * WHAT: Update the schema version metadata in a KG
 * WHY:  Mark KG as upgraded/downgraded after migration
 * HOW:  Store version in KG metadata
 *
 * @param kg Knowledge graph handle
 * @param version Version to set
 * @return 0 on success, -1 on error
 */
int kg_schema_set_version(brain_kg_t* kg, const kg_schema_version_t* version);

/**
 * @brief Compare two schema versions
 *
 * WHAT: Semantic version comparison
 * WHY:  Determine migration direction and ordering
 * HOW:  Compare major, then minor, then patch, then label
 *
 * @param a First version
 * @param b Second version
 * @return -1 if a < b, 0 if a == b, 1 if a > b
 */
int kg_schema_compare(const kg_schema_version_t* a, const kg_schema_version_t* b);

/**
 * @brief Format schema version as string
 *
 * WHAT: Convert version to human-readable string
 * WHY:  Logging, display, debugging
 * HOW:  Format as "major.minor.patch[-label]"
 *
 * @param version Version to format
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Number of characters written (excluding null)
 */
int kg_schema_version_to_string(
    const kg_schema_version_t* version,
    char* buffer,
    size_t buffer_size
);

/**
 * @brief Parse schema version from string
 *
 * WHAT: Parse version string to struct
 * WHY:  Load versions from config files or user input
 * HOW:  Parse "major.minor.patch[-label]" format
 *
 * @param str Version string
 * @param version Output version struct
 * @return 0 on success, -1 on parse error
 */
int kg_schema_version_from_string(
    const char* str,
    kg_schema_version_t* version
);

/* ============================================================================
 * MIGRATION MANAGEMENT API
 * ============================================================================ */

/**
 * @brief Register a migration script
 *
 * WHAT: Add a migration to the global registry
 * WHY:  Build migration path between versions
 * HOW:  Store in ordered list by version
 *
 * @param migration Migration script to register
 * @return 0 on success, -1 on error (e.g., duplicate, invalid)
 */
int kg_schema_register_migration(const kg_migration_script_t* migration);

/**
 * @brief List all registered migrations
 *
 * WHAT: Get all registered migration scripts
 * WHY:  Display available migrations, plan migration path
 * HOW:  Copy migration list to caller-provided array
 *
 * @param migrations Output array (caller allocated)
 * @param count In: array capacity, Out: number of migrations
 * @return 0 on success, -1 on error
 */
int kg_schema_list_migrations(kg_migration_script_t* migrations, uint32_t* count);

/**
 * @brief Get pending migrations for a KG
 *
 * WHAT: Find migrations needed to reach latest version
 * WHY:  Plan and display pending upgrades
 * HOW:  Compare current version to available migrations
 *
 * @param kg Knowledge graph handle (for current version)
 * @param migrations Output array (caller allocated)
 * @param count In: array capacity, Out: number of pending migrations
 * @return 0 on success, -1 on error
 */
int kg_schema_get_pending_migrations(
    const brain_kg_t* kg,
    kg_migration_script_t* migrations,
    uint32_t* count
);

/**
 * @brief Find migration script between two versions
 *
 * WHAT: Locate specific migration by version range
 * WHY:  Execute targeted migration
 * HOW:  Search registry for matching from/to versions
 *
 * @param from Source version
 * @param to Target version
 * @return Pointer to migration script or NULL if not found
 */
const kg_migration_script_t* kg_schema_find_migration(
    const kg_schema_version_t* from,
    const kg_schema_version_t* to
);

/**
 * @brief Clear all registered migrations
 *
 * WHAT: Remove all migrations from registry
 * WHY:  Reset for testing or reinitialization
 * HOW:  Clear internal migration list
 *
 * @return 0 on success
 */
int kg_schema_clear_migrations(void);

/* ============================================================================
 * MIGRATION EXECUTION API
 * ============================================================================ */

/**
 * @brief Migrate knowledge graph to specific version
 *
 * WHAT: Execute migration(s) to reach target version
 * WHY:  Upgrade or downgrade KG schema as needed
 * HOW:  Find migration path, execute scripts in order
 *
 * Executes all intermediate migrations if target is not adjacent.
 * Supports both upgrade (target > current) and downgrade (target < current).
 *
 * @param kg Knowledge graph to migrate
 * @param target Target schema version
 * @param result Output migration result (can be NULL)
 * @return 0 on success, -1 on error (check result for details)
 */
int kg_schema_migrate(
    brain_kg_t* kg,
    const kg_schema_version_t* target,
    kg_migration_result_t* result
);

/**
 * @brief Migrate knowledge graph to next version
 *
 * WHAT: Execute single upgrade migration
 * WHY:  Incremental upgrade with verification between steps
 * HOW:  Find and execute next pending migration
 *
 * @param kg Knowledge graph to migrate
 * @param result Output migration result (can be NULL)
 * @return 0 on success, -1 on error, 1 if already at latest
 */
int kg_schema_migrate_up(brain_kg_t* kg, kg_migration_result_t* result);

/**
 * @brief Rollback knowledge graph to previous version
 *
 * WHAT: Execute single downgrade migration
 * WHY:  Revert problematic upgrade
 * HOW:  Find and execute down script for current version
 *
 * @param kg Knowledge graph to rollback
 * @param result Output migration result (can be NULL)
 * @return 0 on success, -1 on error, 1 if already at earliest
 */
int kg_schema_migrate_down(brain_kg_t* kg, kg_migration_result_t* result);

/**
 * @brief Rollback last successful migration
 *
 * WHAT: Undo the most recent migration
 * WHY:  Quick recovery from failed migration
 * HOW:  Look up last migration in history and execute down script
 *
 * @param kg Knowledge graph to rollback
 * @param result Output migration result (can be NULL)
 * @return 0 on success, -1 on error (e.g., nothing to rollback)
 */
int kg_schema_rollback_last(brain_kg_t* kg, kg_migration_result_t* result);

/* ============================================================================
 * MIGRATION HISTORY API
 * ============================================================================ */

/**
 * @brief Get migration history for a knowledge graph
 *
 * WHAT: Retrieve record of past migrations
 * WHY:  Audit trail, troubleshooting, verification
 * HOW:  Query migration history stored with KG
 *
 * @param kg Knowledge graph handle
 * @param history Output array (caller allocated)
 * @param count In: array capacity, Out: number of history entries
 * @return 0 on success, -1 on error
 */
int kg_schema_get_migration_history(
    const brain_kg_t* kg,
    kg_migration_result_t* history,
    uint32_t* count
);

/**
 * @brief Clear migration history
 *
 * WHAT: Remove all migration history records
 * WHY:  Reset for testing or after verified baseline
 * HOW:  Clear history storage in KG
 *
 * @param kg Knowledge graph handle
 * @return 0 on success, -1 on error
 */
int kg_schema_clear_history(brain_kg_t* kg);

/* ============================================================================
 * COMPATIBILITY API
 * ============================================================================ */

/**
 * @brief Check if schema versions are compatible
 *
 * WHAT: Determine if actual version satisfies required version
 * WHY:  Validate KG compatibility before operations
 * HOW:  Check if actual >= required (for same major version)
 *
 * Compatibility rules:
 * - Same major version: actual.minor >= required.minor
 * - Different major: incompatible
 * - Patch versions ignored for compatibility
 *
 * @param required Required minimum version
 * @param actual Actual version to check
 * @return true if compatible, false otherwise
 */
bool kg_schema_is_compatible(
    const kg_schema_version_t* required,
    const kg_schema_version_t* actual
);

/**
 * @brief Check if migration is needed
 *
 * WHAT: Determine if KG needs migration to reach target
 * WHY:  Avoid unnecessary migration operations
 * HOW:  Compare current version to target
 *
 * @param kg Knowledge graph handle
 * @param target Target version (NULL for latest registered)
 * @return true if migration needed, false if already at target
 */
bool kg_schema_needs_migration(
    const brain_kg_t* kg,
    const kg_schema_version_t* target
);

/**
 * @brief Get latest registered schema version
 *
 * WHAT: Find the highest registered version
 * WHY:  Determine migration target for full upgrade
 * HOW:  Scan registered migrations for highest to_version
 *
 * @return Latest version (zeroed if no migrations registered)
 */
kg_schema_version_t kg_schema_get_latest(void);

/* ============================================================================
 * STRING CONVERSION
 * ============================================================================ */

/**
 * @brief Convert migration direction to string
 *
 * @param direction Migration direction
 * @return Human-readable string
 */
const char* kg_migration_direction_to_string(kg_migration_direction_t direction);

/**
 * @brief Convert migration status to string
 *
 * @param status Migration status
 * @return Human-readable string
 */
const char* kg_migration_status_to_string(kg_migration_status_t status);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_KG_SCHEMA_H */
