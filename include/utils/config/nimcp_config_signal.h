//=============================================================================
// nimcp_config_signal.h - Atomic Config Reload with Rollback Support
//=============================================================================
/**
 * @file nimcp_config_signal.h
 * @brief Atomic configuration reload with versioning and rollback
 *
 * WHAT: Signal-triggered atomic config reload with snapshot/rollback
 * WHY:  Safe hot-reload with automatic rollback on validation failure
 * HOW:  Snapshot before reload, validate, atomically swap or rollback
 *
 * ARCHITECTURE:
 *
 *   Config Snapshot & Rollback System:
 *   ┌─────────────────────────────────────────────────────────────┐
 *   │  SIGHUP Signal                                              │
 *   └───────┬─────────────────────────────────────────────────────┘
 *           │
 *           ▼
 *   ┌─────────────────────────────────────────────────────────────┐
 *   │  1. Create Snapshot (v42)                                   │
 *   │     ┌──────────────┐                                        │
 *   │     │ Current v42  │────┐                                   │
 *   │     └──────────────┘    │                                   │
 *   │                         │ (CoW clone)                       │
 *   │                         ▼                                   │
 *   │                    ┌──────────────┐                         │
 *   │                    │ Snapshot v42 │                         │
 *   │                    └──────────────┘                         │
 *   └─────────────────────────────────────────────────────────────┘
 *           │
 *           ▼
 *   ┌─────────────────────────────────────────────────────────────┐
 *   │  2. Parse New Config (v43)                                  │
 *   │     ┌──────────────┐                                        │
 *   │     │ Temp v43     │                                        │
 *   │     └──────────────┘                                        │
 *   └─────────────────────────────────────────────────────────────┘
 *           │
 *           ▼
 *   ┌─────────────────────────────────────────────────────────────┐
 *   │  3. Validate New Config                                     │
 *   │     ┌──────────────┐                                        │
 *   │     │ Range checks │                                        │
 *   │     │ Type checks  │                                        │
 *   │     │ Callbacks    │                                        │
 *   │     └──────────────┘                                        │
 *   └─────────────────────────────────────────────────────────────┘
 *           │
 *           ├─────────────┬─────────────┐
 *           │ Valid       │ Invalid     │
 *           ▼             ▼             │
 *   ┌──────────────┐ ┌──────────────┐  │
 *   │ 4a. Atomic   │ │ 4b. Rollback │  │
 *   │     Swap     │ │     to v42   │  │
 *   │   v42→v43    │ │              │  │
 *   └──────────────┘ └──────────────┘  │
 *           │             │             │
 *           │             └─────────────┘
 *           │                   │
 *           ▼                   ▼
 *   ┌──────────────┐   ┌──────────────┐
 *   │ Success!     │   │ Rollback OK  │
 *   │ Running v43  │   │ Running v42  │
 *   └──────────────┘   └──────────────┘
 *
 * VERSION HISTORY:
 *   ┌──────────────────────────────────────────────┐
 *   │ History (circular buffer, max 10 versions)   │
 *   │  [0] v38 ← oldest                            │
 *   │  [1] v39                                     │
 *   │  [2] v40                                     │
 *   │  [3] v41                                     │
 *   │  [4] v42 ← current                           │
 *   │  [5] (empty)                                 │
 *   │  ...                                         │
 *   └──────────────────────────────────────────────┘
 *
 * FEATURES:
 * - Atomic reload: all-or-nothing config updates
 * - Automatic rollback on validation failure
 * - Version history with manual rollback support
 * - SIGHUP signal integration
 * - Thread-safe snapshot creation
 * - CoW-optimized snapshots via unified memory
 * - Pre-reload and post-reload callbacks
 * - Configurable history depth
 * - No downtime during reload
 * - Metrics tracking (reload count, failures, etc.)
 *
 * SAFETY GUARANTEES:
 * 1. Readers never see partial config updates
 * 2. Failed reloads never corrupt running config
 * 3. Always can roll back to last known-good state
 * 4. No crashes on malformed config files
 *
 * USAGE:
 *
 * 1. Initialize with rollback support:
 * ```c
 * config_init("/etc/nimcp/config.ini");
 * config_set_history_size(10);  // Keep last 10 versions
 * ```
 *
 * 2. Automatic reload on SIGHUP:
 * ```bash
 * kill -HUP <pid>
 * ```
 *
 * 3. Manual atomic reload:
 * ```c
 * if (!config_atomic_reload("/etc/nimcp/config_new.ini")) {
 *     LOG_ERROR("Reload failed, rolled back to v%u", config_get_version());
 * }
 * ```
 *
 * 4. Manual rollback:
 * ```c
 * // Roll back to previous version
 * if (!config_rollback()) {
 *     LOG_FATAL("Rollback failed!");
 * }
 *
 * // Roll back to specific version
 * config_rollback_to_version(42);
 * ```
 *
 * 5. Create manual snapshot for experimentation:
 * ```c
 * config_snapshot_t snap = config_create_snapshot();
 *
 * // Try experimental config changes
 * config_set_float("learning_rate", 0.1);
 *
 * // If didn't work, restore snapshot
 * if (!experiment_successful) {
 *     config_restore_snapshot(snap);
 * }
 *
 * config_destroy_snapshot(snap);
 * ```
 *
 * THREAD SAFETY:
 * - All operations are thread-safe
 * - Readers can access config during reload
 * - Atomic swap ensures consistency
 *
 * @author NIMCP Development Team
 * @date 2025-11-28
 * @version 1.0.0
 */

#ifndef NIMCP_CONFIG_SIGNAL_H
#define NIMCP_CONFIG_SIGNAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Export macro (for shared library builds)
#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

//=============================================================================
// Constants and Configuration
//=============================================================================

/** @brief Default version history size */
#define CONFIG_DEFAULT_HISTORY_SIZE 10

/** @brief Maximum version history size */
#define CONFIG_MAX_HISTORY_SIZE 100

/** @brief Default snapshot capacity */
#define CONFIG_DEFAULT_SNAPSHOT_CAPACITY 256

//=============================================================================
// Types and Structures
//=============================================================================

/**
 * @brief Config snapshot handle (opaque)
 *
 * Represents a point-in-time snapshot of the entire config state.
 * Can be used for rollback or restoration.
 */
typedef struct config_snapshot_internal* config_snapshot_t;

/**
 * @brief Reload validation callback
 *
 * Called before atomic swap to validate new config.
 * Return true to accept, false to reject (triggers rollback).
 *
 * @param user_data User context
 * @return true to accept new config, false to rollback
 */
typedef bool (*config_reload_validator_t)(void* user_data);

/**
 * @brief Pre-reload callback
 *
 * Called after snapshot created but before parsing new config.
 * Allows modules to prepare for reload.
 *
 * @param version_before Version before reload
 * @param user_data User context
 */
typedef void (*config_pre_reload_callback_t)(uint32_t version_before, void* user_data);

/**
 * @brief Post-reload callback
 *
 * Called after successful reload.
 *
 * @param version_before Previous version
 * @param version_after New version
 * @param success True if reload succeeded, false if rolled back
 * @param user_data User context
 */
typedef void (*config_post_reload_callback_t)(
    uint32_t version_before,
    uint32_t version_after,
    bool success,
    void* user_data
);

/**
 * @brief Atomic reload statistics
 */
typedef struct {
    uint64_t atomic_reloads;        /**< Number of atomic reloads attempted */
    uint64_t atomic_reload_failures; /**< Number of failed atomic reloads */
    uint64_t rollbacks;             /**< Number of manual rollbacks */
    uint64_t rollback_failures;     /**< Number of failed rollbacks */
    uint64_t snapshots_created;     /**< Total snapshots created */
    uint64_t snapshots_destroyed;   /**< Total snapshots destroyed */
    uint64_t validation_failures;   /**< Validation callback rejections */
    uint32_t current_version;       /**< Current config version */
    uint32_t oldest_version;        /**< Oldest version in history */
    uint32_t history_depth;         /**< Number of versions in history */
    uint32_t max_history_size;      /**< Maximum history size */
    uint64_t last_reload_time_ns;   /**< Last reload timestamp */
    uint64_t last_rollback_time_ns; /**< Last rollback timestamp */
} config_atomic_stats_t;

//=============================================================================
// Snapshot API
//=============================================================================

/**
 * @brief Create snapshot of current config state
 *
 * WHAT: Captures entire config state at current moment
 * WHY:  Enable rollback and experimentation
 * HOW:  CoW clone of all config entries via unified memory
 *
 * @return Snapshot handle or NULL on failure
 *
 * COMPLEXITY: O(1) with CoW, O(n) without
 * MEMORY: O(n) for config entries (shared via CoW until modified)
 *
 * EXAMPLE:
 * ```c
 * config_snapshot_t snap = config_create_snapshot();
 * // Make risky changes...
 * config_restore_snapshot(snap);  // Undo
 * config_destroy_snapshot(snap);
 * ```
 */
NIMCP_EXPORT config_snapshot_t config_create_snapshot(void);

/**
 * @brief Destroy snapshot and free resources
 *
 * @param snap Snapshot handle (safe to pass NULL)
 */
NIMCP_EXPORT void config_destroy_snapshot(config_snapshot_t snap);

/**
 * @brief Restore config from snapshot
 *
 * WHAT: Reverts entire config state to snapshot
 * WHY:  Undo changes, recover from bad config
 * HOW:  Atomic swap of config table with snapshot data
 *
 * @param snap Snapshot to restore
 * @return true on success, false on failure
 *
 * WARNING: Destroys current config state!
 */
NIMCP_EXPORT bool config_restore_snapshot(config_snapshot_t snap);

/**
 * @brief Get snapshot version
 *
 * @param snap Snapshot handle
 * @return Version number captured in snapshot
 */
NIMCP_EXPORT uint32_t config_snapshot_get_version(config_snapshot_t snap);

/**
 * @brief Get snapshot timestamp
 *
 * @param snap Snapshot handle
 * @return Timestamp when snapshot was created (nanoseconds since epoch)
 */
NIMCP_EXPORT uint64_t config_snapshot_get_timestamp(config_snapshot_t snap);

/**
 * @brief Clone snapshot
 *
 * @param snap Snapshot to clone
 * @return New snapshot or NULL on failure
 */
NIMCP_EXPORT config_snapshot_t config_clone_snapshot(config_snapshot_t snap);

//=============================================================================
// Atomic Reload API
//=============================================================================

/**
 * @brief Atomically reload config from file
 *
 * WHAT: Load new config with automatic rollback on failure
 * WHY:  Safe hot-reload without downtime or corruption
 * HOW:  Snapshot → Parse → Validate → Swap (or Rollback)
 *
 * @param path Path to new config file (NULL = reload current path)
 * @return true on success, false on failure (rolled back)
 *
 * COMPLEXITY: O(n) where n = number of config entries
 * THREAD SAFETY: Thread-safe, readers block during swap only
 *
 * STEPS:
 * 1. Create snapshot of current config (v42)
 * 2. Parse new config file into temp table (v43)
 * 3. Run validation callbacks
 * 4. If valid: atomically swap tables (v42→v43)
 * 5. If invalid: discard temp, keep snapshot (stay at v42)
 * 6. Trigger post-reload callbacks
 *
 * EXAMPLE:
 * ```c
 * if (config_atomic_reload(NULL)) {
 *     LOG_INFO("Config reloaded successfully to v%u", config_get_version());
 * } else {
 *     LOG_ERROR("Config reload failed, rolled back");
 * }
 * ```
 */
NIMCP_EXPORT bool config_atomic_reload(const char* path);

/**
 * @brief Rollback to previous config version
 *
 * WHAT: Restore config to previous version in history
 * WHY:  Manual recovery from bad config
 * HOW:  Restore from most recent snapshot in history
 *
 * @return true on success, false if no previous version
 *
 * EXAMPLE:
 * ```c
 * // Undo last reload
 * if (config_rollback()) {
 *     LOG_INFO("Rolled back to v%u", config_get_version());
 * }
 * ```
 */
NIMCP_EXPORT bool config_rollback(void);

/**
 * @brief Rollback to specific config version
 *
 * WHAT: Restore config to arbitrary version in history
 * WHY:  Jump back multiple versions
 * HOW:  Search history, restore matching snapshot
 *
 * @param version Version number to restore
 * @return true on success, false if version not in history
 *
 * EXAMPLE:
 * ```c
 * // Go back to version 42
 * if (!config_rollback_to_version(42)) {
 *     LOG_ERROR("Version 42 not in history");
 * }
 * ```
 */
NIMCP_EXPORT bool config_rollback_to_version(uint32_t version);

/**
 * @brief Get current config version
 *
 * @return Current version number (increments on each reload)
 */
NIMCP_EXPORT uint32_t config_get_version(void);

/**
 * @brief Get available version history
 *
 * @param versions Output array for version numbers
 * @param max_versions Size of output array
 * @return Number of versions written (oldest to newest)
 *
 * EXAMPLE:
 * ```c
 * uint32_t versions[10];
 * size_t count = config_get_version_history(versions, 10);
 * for (size_t i = 0; i < count; i++) {
 *     printf("Version %u available\n", versions[i]);
 * }
 * ```
 */
NIMCP_EXPORT size_t config_get_version_history(uint32_t* versions, size_t max_versions);

//=============================================================================
// History Configuration API
//=============================================================================

/**
 * @brief Set maximum version history size
 *
 * WHAT: Configure how many old versions to keep
 * WHY:  Trade memory for rollback depth
 * HOW:  Limits circular buffer of snapshots
 *
 * @param max_size Maximum versions to keep (1-100)
 *
 * NOTE: Larger history uses more memory but enables deeper rollback
 *
 * EXAMPLE:
 * ```c
 * config_set_history_size(20);  // Keep last 20 versions
 * ```
 */
NIMCP_EXPORT void config_set_history_size(uint32_t max_size);

/**
 * @brief Get current history size setting
 *
 * @return Maximum versions kept in history
 */
NIMCP_EXPORT uint32_t config_get_history_size(void);

/**
 * @brief Clear version history
 *
 * WHAT: Delete all snapshots in history
 * WHY:  Free memory or start fresh
 * HOW:  Destroys all history snapshots except current
 *
 * WARNING: After this, rollback is not possible!
 */
NIMCP_EXPORT void config_clear_history(void);

//=============================================================================
// Validation and Callback API
//=============================================================================

/**
 * @brief Register reload validation callback
 *
 * WHAT: Add validator that checks new config before accepting
 * WHY:  Custom validation logic beyond type/range checks
 * HOW:  Callbacks run before atomic swap
 *
 * @param validator Validation function
 * @param user_data User context
 * @return Registration ID (use to unregister)
 *
 * EXAMPLE:
 * ```c
 * bool validate_learning_rate(void* user_data) {
 *     double lr = config_get_float("learning_rate", 0.0);
 *     return lr > 0.0 && lr < 1.0;  // Reject if out of range
 * }
 *
 * config_register_reload_validator(validate_learning_rate, NULL);
 * ```
 */
NIMCP_EXPORT uint32_t config_register_reload_validator(
    config_reload_validator_t validator,
    void* user_data
);

/**
 * @brief Unregister reload validator
 *
 * @param id Registration ID from register call
 */
NIMCP_EXPORT void config_unregister_reload_validator(uint32_t id);

/**
 * @brief Register pre-reload callback
 *
 * @param callback Callback function
 * @param user_data User context
 * @return Registration ID
 */
NIMCP_EXPORT uint32_t config_register_pre_reload_callback(
    config_pre_reload_callback_t callback,
    void* user_data
);

/**
 * @brief Unregister pre-reload callback
 *
 * @param id Registration ID
 */
NIMCP_EXPORT void config_unregister_pre_reload_callback(uint32_t id);

/**
 * @brief Register post-reload callback
 *
 * @param callback Callback function
 * @param user_data User context
 * @return Registration ID
 */
NIMCP_EXPORT uint32_t config_register_post_reload_callback(
    config_post_reload_callback_t callback,
    void* user_data
);

/**
 * @brief Unregister post-reload callback
 *
 * @param id Registration ID
 */
NIMCP_EXPORT void config_unregister_post_reload_callback(uint32_t id);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get atomic reload statistics
 *
 * @param stats Output statistics structure
 * @return true on success
 */
NIMCP_EXPORT bool config_get_atomic_stats(config_atomic_stats_t* stats);

/**
 * @brief Reset atomic reload statistics
 */
NIMCP_EXPORT void config_reset_atomic_stats(void);

//=============================================================================
// Signal Integration API
//=============================================================================

/**
 * @brief Install SIGHUP handler for automatic reload
 *
 * WHAT: Register signal handler that triggers atomic reload
 * WHY:  Standard Unix hot-reload mechanism
 * HOW:  Installs signal handler that calls config_atomic_reload()
 *
 * @return true on success, false on failure
 *
 * NOTE: Automatically called by config_init() if not disabled
 *
 * EXAMPLE:
 * ```bash
 * # Send SIGHUP to process to trigger reload
 * kill -HUP <pid>
 * ```
 */
NIMCP_EXPORT bool config_install_sighup_handler(void);

/**
 * @brief Uninstall SIGHUP handler
 *
 * @return true on success
 */
NIMCP_EXPORT bool config_uninstall_sighup_handler(void);

/**
 * @brief Check if SIGHUP handler is installed
 *
 * @return true if installed
 */
NIMCP_EXPORT bool config_is_sighup_handler_installed(void);

//=============================================================================
// Testing and Debugging API
//=============================================================================

/**
 * @brief Dump version history to log
 *
 * Logs all versions in history with timestamps.
 */
NIMCP_EXPORT void config_dump_version_history(void);

/**
 * @brief Force a version increment (for testing)
 *
 * @return New version number
 */
NIMCP_EXPORT uint32_t config_force_version_increment(void);

/**
 * @brief Compare current config with snapshot
 *
 * @param snap Snapshot to compare
 * @return Number of config entries that differ
 */
NIMCP_EXPORT size_t config_compare_with_snapshot(config_snapshot_t snap);

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_CONFIG_SIGNAL_H
