/**
 * @file nimcp_kg_gc.h
 * @brief Knowledge Graph Garbage Collection and Compaction
 * @version 1.0.0
 * @date 2026-01-16
 *
 * WHAT: Automatic and manual garbage collection for brain knowledge graph
 * WHY:  KG accumulates stale data (orphaned nodes, expired soft-deletes, old snapshots)
 *       that must be reclaimed to maintain performance and memory efficiency
 * HOW:  Configurable GC with target-specific collection, incremental processing,
 *       and storage compaction/defragmentation
 *
 * BIOLOGICAL BASIS:
 * The brain continuously prunes unused synaptic connections during sleep
 * (synaptic homeostasis). This GC system mirrors that process by identifying
 * and removing orphaned connections, expired memories, and redundant data.
 *
 * GC TARGETS:
 * - Orphaned nodes: Nodes with no incoming or outgoing edges
 * - Soft-deleted items: Items marked for deletion past their TTL
 * - Old snapshots: Historical weight snapshots beyond retention period
 * - Stale cache: Cache entries that haven't been accessed
 * - WAL segments: Write-ahead log segments that are fully applied
 * - Tombstones: Deletion markers no longer needed for replication
 *
 * USAGE:
 * ```c
 * // Create GC context with configuration
 * kg_gc_config_t config = {
 *     .gc_targets = KG_GC_ORPHANED_NODES | KG_GC_SOFT_DELETED,
 *     .soft_delete_ttl_hours = 24,
 *     .enable_auto_gc = true,
 *     .gc_interval_minutes = 60
 * };
 * kg_gc_context_t* gc = kg_gc_create(kg, &config);
 *
 * // Manual GC run
 * kg_gc_run(gc, KG_GC_ALL);
 *
 * // Get statistics
 * kg_gc_stats_t stats;
 * kg_gc_analyze(gc, &stats);
 *
 * // Schedule future GC
 * kg_gc_schedule(gc, timestamp_in_ms);
 *
 * kg_gc_destroy(gc);
 * ```
 *
 * THREAD SAFETY: All operations are thread-safe via internal mutex
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_KG_GC_H
#define NIMCP_KG_GC_H

#include "core/brain/nimcp_brain_kg.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Default soft delete TTL in hours */
#define KG_GC_DEFAULT_SOFT_DELETE_TTL_HOURS     24

/** Default snapshot retention in hours */
#define KG_GC_DEFAULT_SNAPSHOT_RETENTION_HOURS  168  /* 7 days */

/** Default maximum GC cycle duration in milliseconds */
#define KG_GC_DEFAULT_MAX_DURATION_MS           5000

/** Default automatic GC interval in minutes */
#define KG_GC_DEFAULT_INTERVAL_MINUTES          60

/** Default GC trigger threshold (percentage of waste) */
#define KG_GC_DEFAULT_THRESHOLD_PERCENT         0.20f

/** Maximum items to process per incremental GC cycle */
#define KG_GC_DEFAULT_INCREMENTAL_BATCH         256

/* ============================================================================
 * GC Target Types
 * ============================================================================ */

/**
 * @brief GC target types (bitmask)
 *
 * Specifies which types of garbage to collect. Can be combined with
 * bitwise OR to collect multiple target types in a single run.
 */
typedef enum {
    KG_GC_ORPHANED_NODES = 1 << 0,   /**< Nodes with no edges */
    KG_GC_SOFT_DELETED   = 1 << 1,   /**< Expired soft-deleted items */
    KG_GC_OLD_SNAPSHOTS  = 1 << 2,   /**< Historical weight snapshots */
    KG_GC_STALE_CACHE    = 1 << 3,   /**< Stale cache entries */
    KG_GC_WAL_SEGMENTS   = 1 << 4,   /**< Old WAL segments */
    KG_GC_TOMBSTONES     = 1 << 5,   /**< Deletion markers */
    KG_GC_ALL            = 0xFFFFFFFF /**< All GC targets */
} kg_gc_target_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief GC statistics
 *
 * Provides detailed statistics about GC operations including counts of
 * items found, removed, and resources reclaimed.
 */
typedef struct {
    uint64_t orphaned_nodes_found;   /**< Orphaned nodes detected */
    uint64_t orphaned_nodes_removed; /**< Orphaned nodes successfully removed */
    uint64_t soft_deleted_expired;   /**< Soft-deleted items that expired */
    uint64_t snapshots_pruned;       /**< Historical snapshots pruned */
    uint64_t cache_entries_cleared;  /**< Stale cache entries cleared */
    uint64_t wal_segments_removed;   /**< WAL segments removed */
    uint64_t tombstones_cleared;     /**< Tombstones cleared */
    uint64_t bytes_reclaimed;        /**< Total bytes reclaimed */
    uint64_t duration_ms;            /**< Duration of last GC run */
    uint64_t last_gc_timestamp;      /**< Timestamp of last GC completion */
    uint32_t gc_runs_total;          /**< Total number of GC runs */
    uint32_t gc_runs_auto;           /**< Automatic GC runs */
    uint32_t gc_runs_manual;         /**< Manual GC runs */
    float fragmentation_before;      /**< Fragmentation before last compaction */
    float fragmentation_after;       /**< Fragmentation after last compaction */
} kg_gc_stats_t;

/**
 * @brief GC configuration
 *
 * Configures garbage collection behavior including target types,
 * retention periods, timing, and automatic collection settings.
 */
typedef struct {
    uint32_t gc_targets;             /**< Bitmask of kg_gc_target_t */
    uint32_t soft_delete_ttl_hours;  /**< Time before permanent deletion */
    uint32_t snapshot_retention_hours; /**< Keep snapshots for this long */
    uint32_t cache_ttl_hours;        /**< Cache entry TTL */
    uint32_t max_gc_duration_ms;     /**< Max time per GC cycle */
    uint32_t gc_interval_minutes;    /**< Automatic GC frequency */
    uint32_t incremental_batch_size; /**< Items per incremental cycle */
    bool enable_auto_gc;             /**< Enable automatic GC */
    bool enable_compaction;          /**< Defragment storage after GC */
    bool enable_wal_gc;              /**< Enable WAL segment collection */
    float gc_threshold_percent;      /**< Trigger GC when waste > threshold */
} kg_gc_config_t;

/**
 * @brief GC context handle (opaque)
 */
typedef struct kg_gc_context kg_gc_context_t;

/**
 * @brief GC progress callback
 *
 * Called periodically during GC operations to report progress.
 *
 * @param items_processed Number of items processed so far
 * @param total_items Estimated total items to process
 * @param user_data User-provided context
 * @return true to continue, false to abort GC
 */
typedef bool (*kg_gc_progress_fn)(
    uint64_t items_processed,
    uint64_t total_items,
    void* user_data
);

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default GC configuration
 *
 * WHAT: Initialize configuration with sensible defaults
 * WHY:  Provide safe starting point for GC configuration
 * HOW:  Set reasonable TTLs, intervals, and thresholds
 *
 * @param config Output configuration to initialize
 * @return 0 on success, -1 on error (NULL config)
 */
int kg_gc_default_config(kg_gc_config_t* config);

/**
 * @brief Create GC context for a knowledge graph
 *
 * WHAT: Initialize garbage collection system for a KG
 * WHY:  Enable automatic and manual garbage collection
 * HOW:  Allocate context, configure settings, optionally start auto-GC
 *
 * @param kg Brain knowledge graph to manage
 * @param config Configuration (NULL for defaults)
 * @return GC context or NULL on error
 */
kg_gc_context_t* kg_gc_create(brain_kg_t* kg, const kg_gc_config_t* config);

/**
 * @brief Destroy GC context
 *
 * WHAT: Clean up GC resources
 * WHY:  Free memory, cancel scheduled GC, stop auto-GC thread
 * HOW:  Stop timers, join threads, free allocations
 *
 * @param gc GC context (NULL safe)
 * @note Does NOT destroy the underlying knowledge graph
 */
void kg_gc_destroy(kg_gc_context_t* gc);

/**
 * @brief Update GC configuration
 *
 * WHAT: Modify GC behavior at runtime
 * WHY:  Adjust settings without recreating context
 * HOW:  Apply new settings, restart timers if needed
 *
 * @param gc GC context
 * @param config New configuration
 * @return 0 on success, -1 on error
 */
int kg_gc_update_config(kg_gc_context_t* gc, const kg_gc_config_t* config);

/**
 * @brief Get current GC configuration
 *
 * @param gc GC context
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int kg_gc_get_config(const kg_gc_context_t* gc, kg_gc_config_t* config);

/* ============================================================================
 * Manual GC Operations
 * ============================================================================ */

/**
 * @brief Run garbage collection
 *
 * WHAT: Execute full GC cycle for specified targets
 * WHY:  Reclaim memory from unused/expired graph elements
 * HOW:  Scan KG for each target type, remove eligible items
 *
 * Performs a blocking GC operation. For long-running GC on large graphs,
 * consider using kg_gc_run_incremental() instead.
 *
 * @param gc GC context
 * @param targets Bitmask of kg_gc_target_t to collect
 * @return Number of items collected, or -1 on error
 */
int kg_gc_run(kg_gc_context_t* gc, uint32_t targets);

/**
 * @brief Run incremental garbage collection
 *
 * WHAT: Execute partial GC cycle processing limited items
 * WHY:  Avoid blocking operations on large graphs
 * HOW:  Process batch of items, save state for next call
 *
 * Call repeatedly until it returns 0 (no more items) to complete
 * a full GC cycle incrementally.
 *
 * @param gc GC context
 * @param max_items Maximum items to process this cycle
 * @return Items processed this cycle, 0 when complete, -1 on error
 */
int kg_gc_run_incremental(kg_gc_context_t* gc, uint32_t max_items);

/**
 * @brief Run GC with progress callback
 *
 * WHAT: Execute GC with progress reporting
 * WHY:  Enable UI updates and cancellation for long GC runs
 * HOW:  Invoke callback periodically during scan/removal
 *
 * @param gc GC context
 * @param targets Bitmask of kg_gc_target_t to collect
 * @param progress_fn Progress callback (NULL to skip)
 * @param user_data Context passed to callback
 * @return Number of items collected, or -1 on error
 */
int kg_gc_run_with_progress(
    kg_gc_context_t* gc,
    uint32_t targets,
    kg_gc_progress_fn progress_fn,
    void* user_data
);

/**
 * @brief Compact storage after GC
 *
 * WHAT: Defragment KG storage
 * WHY:  Reclaim fragmented space after multiple deletions
 * HOW:  Relocate active items to contiguous storage
 *
 * @param gc GC context
 * @return Bytes reclaimed by compaction, or -1 on error
 */
int kg_gc_compact(kg_gc_context_t* gc);

/**
 * @brief Defragment knowledge graph storage
 *
 * WHAT: Optimize storage layout without removing items
 * WHY:  Improve cache locality and access patterns
 * HOW:  Reorganize data structures for optimal access
 *
 * @param gc GC context
 * @return 0 on success, -1 on error
 */
int kg_gc_defragment(kg_gc_context_t* gc);

/**
 * @brief Cancel in-progress GC operation
 *
 * WHAT: Abort a running GC cycle
 * WHY:  Allow graceful shutdown or priority interruption
 * HOW:  Set cancellation flag, wait for current item to complete
 *
 * @param gc GC context
 * @return 0 on success, -1 if no GC in progress
 */
int kg_gc_cancel(kg_gc_context_t* gc);

/* ============================================================================
 * Analysis Functions
 * ============================================================================ */

/**
 * @brief Analyze KG and get GC statistics
 *
 * WHAT: Scan KG to determine garbage candidates
 * WHY:  Preview what GC would collect without removing
 * HOW:  Count orphans, expired items, etc.
 *
 * @param gc GC context
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int kg_gc_analyze(const kg_gc_context_t* gc, kg_gc_stats_t* stats);

/**
 * @brief Find orphaned nodes (no edges)
 *
 * WHAT: Identify nodes with no connections
 * WHY:  Preview orphans before removal
 * HOW:  Scan all nodes, check edge counts
 *
 * @param gc GC context
 * @param orphans Output array for orphan node IDs (caller-allocated)
 * @param count Input: array capacity, Output: orphan count
 * @return 0 on success, -1 on error
 */
int kg_gc_find_orphans(
    const kg_gc_context_t* gc,
    brain_kg_node_id_t* orphans,
    uint32_t* count
);

/**
 * @brief Get storage fragmentation level
 *
 * WHAT: Calculate current fragmentation percentage
 * WHY:  Determine if compaction is beneficial
 * HOW:  Compare allocated vs used space
 *
 * @param gc GC context
 * @return Fragmentation ratio [0.0 - 1.0], or -1.0 on error
 */
float kg_gc_get_fragmentation(const kg_gc_context_t* gc);

/**
 * @brief Get estimated reclaimable bytes
 *
 * WHAT: Estimate space that would be freed by GC
 * WHY:  Help decide when to run GC
 * HOW:  Analyze garbage candidates without removing
 *
 * @param gc GC context
 * @param targets Bitmask of kg_gc_target_t to analyze
 * @return Estimated reclaimable bytes, or -1 on error
 */
int64_t kg_gc_estimate_reclaimable(
    const kg_gc_context_t* gc,
    uint32_t targets
);

/**
 * @brief Check if GC is currently running
 *
 * @param gc GC context
 * @return true if GC is in progress
 */
bool kg_gc_is_running(const kg_gc_context_t* gc);

/**
 * @brief Get last GC error message
 *
 * @param gc GC context
 * @return Error message or NULL if no error
 */
const char* kg_gc_get_last_error(const kg_gc_context_t* gc);

/* ============================================================================
 * Scheduling Functions
 * ============================================================================ */

/**
 * @brief Schedule GC to run at a specific time
 *
 * WHAT: Queue a GC run for future execution
 * WHY:  Run GC during low-activity periods
 * HOW:  Set timer to trigger GC at specified timestamp
 *
 * @param gc GC context
 * @param run_at_timestamp Unix timestamp (ms) for GC execution
 * @return 0 on success, -1 on error
 */
int kg_gc_schedule(kg_gc_context_t* gc, uint64_t run_at_timestamp);

/**
 * @brief Cancel scheduled GC
 *
 * WHAT: Cancel a pending scheduled GC
 * WHY:  Override previously scheduled GC
 * HOW:  Clear timer and scheduled state
 *
 * @param gc GC context
 * @return 0 on success, -1 if no scheduled GC
 */
int kg_gc_cancel_scheduled(kg_gc_context_t* gc);

/**
 * @brief Get scheduled GC timestamp
 *
 * @param gc GC context
 * @return Scheduled timestamp, or 0 if not scheduled
 */
uint64_t kg_gc_get_scheduled_time(const kg_gc_context_t* gc);

/**
 * @brief Enable automatic GC
 *
 * WHAT: Start automatic GC at configured interval
 * WHY:  Maintain KG hygiene without manual intervention
 * HOW:  Start periodic timer based on gc_interval_minutes
 *
 * @param gc GC context
 * @return 0 on success, -1 on error
 */
int kg_gc_enable_auto(kg_gc_context_t* gc);

/**
 * @brief Disable automatic GC
 *
 * WHAT: Stop automatic GC
 * WHY:  Pause auto-GC during critical operations
 * HOW:  Stop periodic timer
 *
 * @param gc GC context
 * @return 0 on success
 */
int kg_gc_disable_auto(kg_gc_context_t* gc);

/**
 * @brief Check if automatic GC is enabled
 *
 * @param gc GC context
 * @return true if auto-GC is enabled
 */
bool kg_gc_is_auto_enabled(const kg_gc_context_t* gc);

/**
 * @brief Force GC trigger based on threshold
 *
 * WHAT: Run GC if waste exceeds configured threshold
 * WHY:  Conditional GC based on current state
 * HOW:  Analyze waste, run GC if above gc_threshold_percent
 *
 * @param gc GC context
 * @return 1 if GC was triggered, 0 if below threshold, -1 on error
 */
int kg_gc_trigger_if_needed(kg_gc_context_t* gc);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Convert GC target to string
 *
 * @param target GC target type
 * @return Human-readable string
 */
const char* kg_gc_target_to_string(kg_gc_target_t target);

/**
 * @brief Reset GC statistics
 *
 * @param gc GC context
 */
void kg_gc_reset_stats(kg_gc_context_t* gc);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_KG_GC_H */
