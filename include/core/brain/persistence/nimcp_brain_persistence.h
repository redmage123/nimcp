//=============================================================================
// nimcp_brain_persistence.h - Brain Persistence & Snapshot Management
//=============================================================================
/**
 * @file nimcp_brain_persistence.h
 * @brief Brain state persistence, snapshot management, and recovery
 *
 * WHAT: Provides save/load/snapshot APIs for brain state persistence
 * WHY:  Enable model checkpointing, disaster recovery, A/B testing, version control
 * HOW:  Serialize brain state to files with versioning, compression, encryption support
 *
 * ARCHITECTURE:
 * - Persistence API: Save/load complete brain state (network + metadata + subsystems)
 * - Snapshot API: Named, timestamped snapshots with metadata
 * - Format Versioning: Support for format evolution and backward compatibility
 * - Security: Optional compression and encryption (AES-256)
 *
 * PERFORMANCE:
 * - Save: O(n*c + k) where n=neurons, c=connections, k=labels
 * - Load: O(n*c + k) with validation
 * - Snapshot operations: Same as save/load + directory management
 *
 * DESIGN PRINCIPLES:
 * - Atomic operations: Partial writes don't corrupt existing state
 * - Validation: Strict checks on load to prevent buffer overflows
 * - Modularity: Each subsystem saves to separate file
 * - Extensibility: Version header supports format evolution
 */

#ifndef NIMCP_BRAIN_PERSISTENCE_H
#define NIMCP_BRAIN_PERSISTENCE_H

#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "security/nimcp_security_integration.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Phase PERSIST-1: Module Initialization and Security Integration
//=============================================================================

/**
 * @brief Persistence module configuration
 *
 * WHAT: Configuration for persistence operations
 * WHY:  Enable unified memory and security integration for save/load
 * HOW:  Configure memory pooling, CoW, and security tracking
 */
typedef struct {
    // Memory Integration (Phase PERSIST-1.1)
    bool use_unified_memory;              /**< Use unified memory for buffers */
    unified_mem_manager_t memory_manager; /**< External memory manager (NULL = create internal) */
    bool enable_cow_snapshots;            /**< Use CoW for instant snapshots */

    // Security Integration (Phase PERSIST-1.2)
    bool enable_security;                        /**< Enable security module integration */
    nimcp_sec_integration_t* security_context;   /**< External security context */

    // Buffer settings
    size_t read_buffer_size;              /**< Read buffer size (default 64KB) */
    size_t write_buffer_size;             /**< Write buffer size (default 64KB) */

    // Integrity checking
    bool enable_checksum;                 /**< Compute checksums for integrity */
    bool verify_on_load;                  /**< Verify checksum on load */
} persistence_config_t;

/**
 * @brief Persistence module statistics
 *
 * WHAT: Statistics for persistence operations
 * WHY:  Monitor save/load performance and resource usage
 */
typedef struct {
    // Operation counts
    uint64_t total_saves;                 /**< Total save operations */
    uint64_t total_loads;                 /**< Total load operations */
    uint64_t total_snapshots_created;     /**< Snapshots created */
    uint64_t total_snapshots_restored;    /**< Snapshots restored */
    uint64_t total_snapshots_deleted;     /**< Snapshots deleted */

    // Byte counts
    uint64_t bytes_written;               /**< Total bytes written */
    uint64_t bytes_read;                  /**< Total bytes read */

    // Memory integration stats
    uint64_t pool_allocations;            /**< Allocations from pool */
    uint64_t malloc_allocations;          /**< Direct malloc allocations */
    uint64_t cow_snapshots;               /**< CoW snapshots created */
    uint64_t memory_saved_bytes;          /**< Memory saved by CoW */

    // Performance
    uint64_t total_save_time_ms;          /**< Total time in save operations */
    uint64_t total_load_time_ms;          /**< Total time in load operations */

    // Errors
    uint64_t save_errors;                 /**< Save operation failures */
    uint64_t load_errors;                 /**< Load operation failures */
    uint64_t checksum_failures;           /**< Checksum verification failures */
} persistence_stats_t;

/**
 * @brief Initialize persistence module
 *
 * WHAT: Initialize persistence module with security registration
 * WHY:  Enable security tracking and audit for persistence operations
 * HOW:  Register with security module, initialize statistics
 *
 * @param security_ctx Security integration context (NULL to skip registration)
 * @return true on success, false on failure
 */
bool persistence_init(nimcp_sec_integration_t* security_ctx);

/**
 * @brief Shutdown persistence module
 *
 * WHAT: Clean shutdown of persistence module
 * WHY:  Unregister from security, cleanup resources
 */
void persistence_shutdown(void);

/**
 * @brief Get persistence module security ID
 *
 * @return Security module ID (0 if not registered)
 */
uint32_t persistence_get_security_module_id(void);

/**
 * @brief Get persistence statistics
 *
 * @param stats Output statistics structure
 * @return true on success
 */
bool persistence_get_stats(persistence_stats_t* stats);

/**
 * @brief Reset persistence statistics
 */
void persistence_reset_stats(void);

/**
 * @brief Get default persistence configuration
 *
 * @return Default configuration with sensible values
 */
persistence_config_t persistence_default_config(void);

//=============================================================================
// Extended Save/Load API with Memory/Security Integration
//=============================================================================

/**
 * @brief Save brain with extended configuration
 *
 * WHAT: Save brain with unified memory and security integration
 * WHY:  Enable efficient buffering and security audit trail
 * HOW:  Use configured memory manager for buffers, record security interactions
 *
 * @param brain Brain instance
 * @param filepath Path to save to
 * @param config Persistence configuration (NULL for defaults)
 * @return true on success, false on error
 */
bool brain_save_ex(brain_t brain, const char* filepath, const persistence_config_t* config);

/**
 * @brief Load brain with extended configuration
 *
 * WHAT: Load brain with unified memory and security integration
 * WHY:  Enable efficient buffering and security audit trail
 * HOW:  Use configured memory manager for buffers, record security interactions
 *
 * @param filepath Path to load from
 * @param config Persistence configuration (NULL for defaults)
 * @return Brain instance or NULL on error
 */
brain_t brain_load_ex(const char* filepath, const persistence_config_t* config);

/**
 * @brief Create instant snapshot using CoW
 *
 * WHAT: Create instant snapshot without copying data
 * WHY:  O(1) snapshot creation for checkpointing
 * HOW:  Uses page-level CoW if available
 *
 * @param brain Brain instance
 * @param name Snapshot name
 * @param description Optional description
 * @param config Persistence configuration (NULL for defaults)
 * @return true on success
 */
bool brain_save_snapshot_cow(brain_t brain, const char* name,
                             const char* description, const persistence_config_t* config);

//=============================================================================
// Persistence API
//=============================================================================

/**
 * @brief Save brain to file
 *
 * WHAT: Persist complete brain state to disk
 * WHY:  Enable model checkpointing, transfer learning, disaster recovery
 * HOW:  Save network → metadata → subsystems to separate files
 *
 * FILES CREATED:
 * - {filepath}         : Network structure and weights
 * - {filepath}.meta    : Brain configuration, labels, stats, timestamps
 * - {filepath}.knowledge : Knowledge system state (if exists)
 * - {filepath}.executive : Executive controller state (if exists)
 * - {filepath}.pink_noise : Pink noise neuromodulator state (if exists)
 * - {filepath}.mirror_neurons : Mirror neuron system state (if exists)
 *
 * COMPLEXITY: O(n*c + k + m) where:
 *   n = neurons, c = connections per neuron
 *   k = output labels, m = working memory items
 *
 * @param brain Brain instance (non-NULL)
 * @param filepath Base filepath for saving (non-NULL)
 * @return true on success, false on error
 */
bool brain_save(brain_t brain, const char* filepath);

/**
 * @brief Load brain from file
 *
 * WHAT: Restore complete brain state from disk
 * WHY:  Enable model reuse, transfer learning, disaster recovery
 * HOW:  Load network → metadata → subsystems, validate all inputs
 *
 * SECURITY: All loaded data is validated to prevent buffer overflows:
 * - Config fields checked for NaN/Inf
 * - Array sizes checked against maximum limits
 * - String lengths validated before allocation
 * - File format version compatibility checked
 *
 * COMPLEXITY: O(n*c + k + m) where:
 *   n = neurons, c = connections per neuron
 *   k = output labels, m = working memory items
 *
 * @param filepath Base filepath for loading (non-NULL)
 * @return Brain instance on success, NULL on error
 */
brain_t brain_load(const char* filepath);

//=============================================================================
// Snapshot API - Named State Snapshots
//=============================================================================

/**
 * @brief Snapshot metadata structure
 *
 * WHAT: Metadata for a brain snapshot
 * WHY:  Enable snapshot discovery, version tracking, audit trails
 * HOW:  Stored in .snapshot.info file alongside snapshot
 *
 * NOTE: brain_snapshot_info_t is defined in nimcp_brain.h (lines 1251-1258)
 *       and included via "core/brain/nimcp_brain.h" above
 */

/**
 * @brief Save brain snapshot with compression and encryption
 *
 * WHAT: Create a named, timestamped snapshot of complete brain state
 * WHY:  Enable backups, A/B testing, version control, disaster recovery
 * HOW:  Save to snapshot_dir with optional compression/encryption
 *
 * SNAPSHOT FORMAT:
 * - Filename: {snapshot_dir}/{name}_{timestamp}.snapshot
 * - Metadata: {snapshot_dir}/{name}_{timestamp}.snapshot.info
 * - Includes: All brain state (network, subsystems, knowledge)
 * - Compression: zlib (if enabled in config)
 * - Encryption: AES-256 (if enabled in config)
 *
 * EXAMPLE:
 * ```c
 * brain_save_snapshot(brain, "experiment_v1", "Before parameter tuning");
 * // Creates: snapshots/experiment_v1_1642531200.snapshot
 * //          snapshots/experiment_v1_1642531200.snapshot.info
 * ```
 *
 * @param brain Brain instance (non-NULL)
 * @param name Snapshot name (non-NULL, e.g., "before_experiment", "v1.0")
 * @param description Optional description (can be NULL)
 * @return true on success, false on error
 */
bool brain_save_snapshot(brain_t brain, const char* name, const char* description);

/**
 * @brief Restore brain from snapshot
 *
 * WHAT: Load brain state from most recent snapshot with given name
 * WHY:  Restore previous state, rollback changes, A/B testing
 * HOW:  Find latest snapshot → decompress/decrypt → load state
 *
 * BEHAVIOR:
 * - Finds most recent snapshot matching {name}
 * - If brain parameter is non-NULL, warns that in-place restore not yet implemented
 * - Returns new brain instance with restored state
 *
 * EXAMPLE:
 * ```c
 * brain_t restored = brain_restore_snapshot(NULL, "experiment_v1");
 * // Loads: snapshots/experiment_v1_<latest_timestamp>.snapshot
 * ```
 *
 * @param brain Brain instance to restore into (currently ignored, pass NULL)
 * @param name Snapshot name to restore (non-NULL)
 * @return Brain instance with restored state, or NULL on error
 */
brain_t brain_restore_snapshot(brain_t brain, const char* name);

/**
 * @brief List available snapshots
 *
 * WHAT: Enumerate all snapshots in snapshot directory
 * WHY:  Allow users to see available restore points
 * HOW:  Scan snapshot_dir, parse .snapshot.info metadata files
 *
 * EXAMPLE:
 * ```c
 * brain_snapshot_info_t infos[100];
 * uint32_t count;
 * brain_list_snapshots(brain, infos, 100, &count);
 * for (uint32_t i = 0; i < count; i++) {
 *     printf("%s: %s (size: %u)\n", infos[i].name,
 *            infos[i].description, infos[i].file_size);
 * }
 * ```
 *
 * @param brain Brain instance (for snapshot_dir config)
 * @param infos Output array of snapshot info (allocated by caller)
 * @param max_count Maximum number of snapshots to return
 * @param out_count Output: actual number of snapshots found
 * @return true on success, false on error
 */
bool brain_list_snapshots(brain_t brain, brain_snapshot_info_t* infos,
                         uint32_t max_count, uint32_t* out_count);

/**
 * @brief Delete snapshot and associated files
 *
 * WHAT: Remove snapshot and all associated files
 * WHY:  Free disk space, remove obsolete restore points
 * HOW:  Find latest snapshot matching name → delete all files
 *
 * FILES DELETED:
 * - {name}_{timestamp}.snapshot
 * - {name}_{timestamp}.snapshot.info
 * - {name}_{timestamp}.snapshot.meta
 * - {name}_{timestamp}.snapshot.knowledge (if exists)
 *
 * @param brain Brain instance (for snapshot_dir config)
 * @param name Snapshot name to delete (non-NULL)
 * @return true on success, false on error
 */
bool brain_delete_snapshot(brain_t brain, const char* name);

//=============================================================================
// Internal Helper Functions (for nimcp_brain.c use only)
//=============================================================================

/**
 * @brief Save working memory state to file
 *
 * INTERNAL USE ONLY
 *
 * @param wm Working memory instance (nullable)
 * @param file File handle (non-NULL)
 * @return true on success, false on error
 */
bool nimcp_brain_save_working_memory_state(working_memory_t* wm, FILE* file);

/**
 * @brief Load working memory state from file
 *
 * INTERNAL USE ONLY
 *
 * @param brain Brain instance (non-NULL)
 * @param file File handle (non-NULL)
 * @return true on success (non-fatal on WM failure)
 */
bool nimcp_brain_load_working_memory_state(brain_t brain, FILE* file);

/**
 * @brief Save metadata file
 *
 * INTERNAL USE ONLY
 *
 * @param brain Brain instance (non-NULL)
 * @param filepath Base filepath (non-NULL)
 * @return true on success, false on error
 */
bool nimcp_brain_save_metadata(brain_t brain, const char* filepath);

/**
 * @brief Load metadata file
 *
 * INTERNAL USE ONLY
 *
 * @param brain Brain instance (non-NULL)
 * @param filepath Base filepath (non-NULL)
 * @return true on success, false on error
 */
bool nimcp_brain_load_metadata(brain_t brain, const char* filepath);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_BRAIN_PERSISTENCE_H
