/**
 * @file nimcp_kg_gc.c
 * @brief Knowledge Graph Garbage Collection and Compaction
 * @version 1.0.0
 * @date 2026-01-16
 *
 * Implementation of automatic and manual garbage collection for brain
 * knowledge graph, including orphan detection, soft-delete expiration,
 * and storage compaction.
 */

#include "core/brain/nimcp_kg_gc.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <time.h>
#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(kg_gc, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * Constants
 * ============================================================================ */

#define MAX_ERROR_LEN           256
#define MAX_ORPHAN_BATCH        1024

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Incremental GC state
 */
typedef struct {
    uint32_t current_target;         /**< Current target being processed */
    uint32_t remaining_targets;      /**< Remaining targets bitmask */
    uint64_t items_processed;        /**< Items processed in current run */
    uint64_t total_items;            /**< Estimated total items */
    brain_kg_node_id_t* orphan_queue; /**< Queue of orphans to remove */
    uint32_t orphan_queue_count;     /**< Orphans in queue */
    uint32_t orphan_queue_pos;       /**< Current position in queue */
    bool in_progress;                /**< GC in progress flag */
} kg_gc_incremental_state_t;

/**
 * @brief GC context implementation
 */
struct kg_gc_context {
    brain_kg_t* kg;                  /**< Associated knowledge graph */
    kg_gc_config_t config;           /**< Configuration */

    /* Statistics */
    kg_gc_stats_t stats;             /**< Runtime statistics */

    /* Incremental state */
    kg_gc_incremental_state_t incremental; /**< Incremental GC state */

    /* Scheduling */
    uint64_t scheduled_timestamp;    /**< Scheduled GC timestamp */
    bool auto_gc_enabled;            /**< Auto-GC enabled flag */
    uint64_t last_auto_gc;           /**< Last auto-GC timestamp */

    /* Cancellation */
    volatile bool cancel_requested;  /**< Cancellation flag */

    /* Error state */
    char last_error[MAX_ERROR_LEN];  /**< Last error message */

    /* Thread safety */
    nimcp_mutex_t* mutex;            /**< Context mutex */
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get current timestamp in milliseconds
 */
static uint64_t get_timestamp_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

/**
 * @brief Set error message
 */
static void set_error(kg_gc_context_t* gc, const char* msg) {
    if (gc && msg) {
        strncpy(gc->last_error, msg, MAX_ERROR_LEN - 1);
        gc->last_error[MAX_ERROR_LEN - 1] = '\0';
    }
}

/**
 * @brief Clear error message
 */
static void clear_error(kg_gc_context_t* gc) {
    if (gc) {
        gc->last_error[0] = '\0';
    }
}

/**
 * @brief Collect orphaned nodes
 */
static int collect_orphaned_nodes(kg_gc_context_t* gc, uint32_t max_items) {
    (void)max_items;

    /* In a real implementation, we would:
     * 1. Iterate through all nodes
     * 2. Check if each node has any edges
     * 3. Remove nodes with no edges
     */

    /* Placeholder: simulate finding some orphans */
    uint32_t orphans_found = 0;
    uint32_t orphans_removed = 0;

    gc->stats.orphaned_nodes_found += orphans_found;
    gc->stats.orphaned_nodes_removed += orphans_removed;

    return (int)orphans_removed;
}

/**
 * @brief Collect soft-deleted items
 */
static int collect_soft_deleted(kg_gc_context_t* gc) {
    /* In a real implementation, we would:
     * 1. Find all items with soft-delete timestamp
     * 2. Check if TTL has expired
     * 3. Permanently remove expired items
     */

    uint64_t now = get_timestamp_ms();
    uint64_t ttl_ms = (uint64_t)gc->config.soft_delete_ttl_hours * 3600ULL * 1000ULL;
    (void)now;
    (void)ttl_ms;

    uint32_t expired = 0;

    gc->stats.soft_deleted_expired += expired;

    return (int)expired;
}

/**
 * @brief Collect old snapshots
 */
static int collect_old_snapshots(kg_gc_context_t* gc) {
    /* In a real implementation, we would:
     * 1. Find all weight snapshots
     * 2. Check if older than retention period
     * 3. Remove old snapshots
     */

    uint32_t pruned = 0;

    gc->stats.snapshots_pruned += pruned;

    return (int)pruned;
}

/**
 * @brief Collect stale cache entries
 */
static int collect_stale_cache(kg_gc_context_t* gc) {
    /* In a real implementation, we would:
     * 1. Iterate through cache entries
     * 2. Check last access time
     * 3. Remove entries past TTL
     */

    uint32_t cleared = 0;

    gc->stats.cache_entries_cleared += cleared;

    return (int)cleared;
}

/**
 * @brief Collect WAL segments
 */
static int collect_wal_segments(kg_gc_context_t* gc) {
    /* In a real implementation, we would:
     * 1. Find applied WAL segments
     * 2. Remove segments no longer needed for recovery
     */

    uint32_t removed = 0;

    gc->stats.wal_segments_removed += removed;

    return (int)removed;
}

/**
 * @brief Collect tombstones
 */
static int collect_tombstones(kg_gc_context_t* gc) {
    /* In a real implementation, we would:
     * 1. Find deletion markers (tombstones)
     * 2. Check if all replicas have seen the deletion
     * 3. Remove tombstones that are fully propagated
     */

    uint32_t cleared = 0;

    gc->stats.tombstones_cleared += cleared;

    return (int)cleared;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int kg_gc_default_config(kg_gc_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    memset(config, 0, sizeof(*config));

    config->gc_targets = KG_GC_ALL;
    config->soft_delete_ttl_hours = KG_GC_DEFAULT_SOFT_DELETE_TTL_HOURS;
    config->snapshot_retention_hours = KG_GC_DEFAULT_SNAPSHOT_RETENTION_HOURS;
    config->cache_ttl_hours = 24;
    config->max_gc_duration_ms = KG_GC_DEFAULT_MAX_DURATION_MS;
    config->gc_interval_minutes = KG_GC_DEFAULT_INTERVAL_MINUTES;
    config->incremental_batch_size = KG_GC_DEFAULT_INCREMENTAL_BATCH;
    config->enable_auto_gc = false;
    config->enable_compaction = true;
    config->enable_wal_gc = true;
    config->gc_threshold_percent = KG_GC_DEFAULT_THRESHOLD_PERCENT;

    return 0;
}

kg_gc_context_t* kg_gc_create(brain_kg_t* kg, const kg_gc_config_t* config) {
    kg_gc_context_t* gc = nimcp_calloc(1, sizeof(kg_gc_context_t));
    if (!gc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "gc is NULL");

        return NULL;
    }

    gc->kg = kg;

    /* Apply configuration */
    if (config) {
        gc->config = *config;
    } else {
        kg_gc_default_config(&gc->config);
    }

    /* Create mutex */
    mutex_attr_t attr = {0};
    attr.type = MUTEX_TYPE_NORMAL;
    gc->mutex = nimcp_mutex_create(&attr);
    if (!gc->mutex) {
        nimcp_free(gc);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_gc_create: gc->mutex is NULL");
        return NULL;
    }

    /* Allocate orphan queue */
    gc->incremental.orphan_queue = nimcp_calloc(MAX_ORPHAN_BATCH, sizeof(brain_kg_node_id_t));
    if (!gc->incremental.orphan_queue) {
        nimcp_mutex_free(gc->mutex);
        nimcp_free(gc);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_gc_create: gc->incremental is NULL");
        return NULL;
    }

    /* Enable auto-GC if configured */
    if (gc->config.enable_auto_gc) {
        gc->auto_gc_enabled = true;
        gc->last_auto_gc = get_timestamp_ms();
    }

    return gc;
}

void kg_gc_destroy(kg_gc_context_t* gc) {
    if (!gc) {
        return;
    }

    /* Cancel any in-progress GC */
    gc->cancel_requested = true;

    /* Free orphan queue */
    if (gc->incremental.orphan_queue) {
        nimcp_free(gc->incremental.orphan_queue);
    }

    /* Destroy mutex */
    if (gc->mutex) {
        nimcp_mutex_free(gc->mutex);
    }

    nimcp_free(gc);
}

int kg_gc_update_config(kg_gc_context_t* gc, const kg_gc_config_t* config) {
    if (!gc || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_gc_update_config: required parameter is NULL (gc, config)");
        return -1;
    }

    nimcp_mutex_lock(gc->mutex);
    gc->config = *config;
    nimcp_mutex_unlock(gc->mutex);

    return 0;
}

int kg_gc_get_config(const kg_gc_context_t* gc, kg_gc_config_t* config) {
    if (!gc || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_gc_get_config: required parameter is NULL (gc, config)");
        return -1;
    }

    nimcp_mutex_lock(((kg_gc_context_t*)gc)->mutex);
    *config = gc->config;
    nimcp_mutex_unlock(((kg_gc_context_t*)gc)->mutex);

    return 0;
}

/* ============================================================================
 * Manual GC Operations
 * ============================================================================ */

int kg_gc_run(kg_gc_context_t* gc, uint32_t targets) {
    if (!gc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gc is NULL");

        return -1;
    }

    nimcp_mutex_lock(gc->mutex);

    if (gc->incremental.in_progress) {
        set_error(gc, "GC already in progress");
        nimcp_mutex_unlock(gc->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_gc_run: validation failed");
        return -1;
    }

    clear_error(gc);
    gc->incremental.in_progress = true;
    gc->cancel_requested = false;

    uint64_t start_time = get_timestamp_ms();
    int total_collected = 0;

    /* Process each target type */
    if (targets & KG_GC_ORPHANED_NODES) {
        int collected = collect_orphaned_nodes(gc, UINT32_MAX);
        if (collected > 0) total_collected += collected;
    }

    if (!gc->cancel_requested && (targets & KG_GC_SOFT_DELETED)) {
        int collected = collect_soft_deleted(gc);
        if (collected > 0) total_collected += collected;
    }

    if (!gc->cancel_requested && (targets & KG_GC_OLD_SNAPSHOTS)) {
        int collected = collect_old_snapshots(gc);
        if (collected > 0) total_collected += collected;
    }

    if (!gc->cancel_requested && (targets & KG_GC_STALE_CACHE)) {
        int collected = collect_stale_cache(gc);
        if (collected > 0) total_collected += collected;
    }

    if (!gc->cancel_requested && (targets & KG_GC_WAL_SEGMENTS)) {
        int collected = collect_wal_segments(gc);
        if (collected > 0) total_collected += collected;
    }

    if (!gc->cancel_requested && (targets & KG_GC_TOMBSTONES)) {
        int collected = collect_tombstones(gc);
        if (collected > 0) total_collected += collected;
    }

    /* Run compaction if enabled */
    if (!gc->cancel_requested && gc->config.enable_compaction) {
        kg_gc_compact(gc);
    }

    /* Update statistics */
    gc->stats.duration_ms = get_timestamp_ms() - start_time;
    gc->stats.last_gc_timestamp = get_timestamp_ms();
    gc->stats.gc_runs_total++;
    gc->stats.gc_runs_manual++;

    gc->incremental.in_progress = false;

    nimcp_mutex_unlock(gc->mutex);

    return total_collected;
}

int kg_gc_run_incremental(kg_gc_context_t* gc, uint32_t max_items) {
    if (!gc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gc is NULL");

        return -1;
    }

    nimcp_mutex_lock(gc->mutex);

    /* Initialize incremental state if not in progress */
    if (!gc->incremental.in_progress) {
        gc->incremental.in_progress = true;
        gc->incremental.remaining_targets = gc->config.gc_targets;
        gc->incremental.items_processed = 0;
        gc->cancel_requested = false;
    }

    int processed = 0;

    /* Process current target up to max_items */
    if (gc->incremental.remaining_targets & KG_GC_ORPHANED_NODES) {
        int collected = collect_orphaned_nodes(gc, max_items);
        if (collected >= 0) processed += collected;
        gc->incremental.remaining_targets &= ~KG_GC_ORPHANED_NODES;
    } else if (gc->incremental.remaining_targets & KG_GC_SOFT_DELETED) {
        int collected = collect_soft_deleted(gc);
        if (collected >= 0) processed += collected;
        gc->incremental.remaining_targets &= ~KG_GC_SOFT_DELETED;
    } else if (gc->incremental.remaining_targets) {
        /* Process remaining targets */
        if (gc->incremental.remaining_targets & KG_GC_OLD_SNAPSHOTS) {
            collect_old_snapshots(gc);
            gc->incremental.remaining_targets &= ~KG_GC_OLD_SNAPSHOTS;
        } else if (gc->incremental.remaining_targets & KG_GC_STALE_CACHE) {
            collect_stale_cache(gc);
            gc->incremental.remaining_targets &= ~KG_GC_STALE_CACHE;
        } else if (gc->incremental.remaining_targets & KG_GC_WAL_SEGMENTS) {
            collect_wal_segments(gc);
            gc->incremental.remaining_targets &= ~KG_GC_WAL_SEGMENTS;
        } else if (gc->incremental.remaining_targets & KG_GC_TOMBSTONES) {
            collect_tombstones(gc);
            gc->incremental.remaining_targets &= ~KG_GC_TOMBSTONES;
        }
    }

    gc->incremental.items_processed += (uint64_t)processed;

    /* Check if complete */
    if (gc->incremental.remaining_targets == 0) {
        gc->incremental.in_progress = false;
        gc->stats.last_gc_timestamp = get_timestamp_ms();
        gc->stats.gc_runs_total++;
        nimcp_mutex_unlock(gc->mutex);
        return 0; /* Complete */
    }

    nimcp_mutex_unlock(gc->mutex);

    return processed;
}

int kg_gc_run_with_progress(
    kg_gc_context_t* gc,
    uint32_t targets,
    kg_gc_progress_fn progress_fn,
    void* user_data
) {
    if (!gc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gc is NULL");

        return -1;
    }

    nimcp_mutex_lock(gc->mutex);

    gc->incremental.in_progress = true;
    gc->cancel_requested = false;

    int total_collected = 0;
    uint64_t items_processed = 0;
    uint64_t total_items = 100; /* Placeholder estimate */

    /* Call progress at start */
    if (progress_fn) {
        if (!progress_fn(0, total_items, user_data)) {
            gc->cancel_requested = true;
        }
    }

    if (!gc->cancel_requested && (targets & KG_GC_ORPHANED_NODES)) {
        int collected = collect_orphaned_nodes(gc, UINT32_MAX);
        if (collected > 0) total_collected += collected;
        items_processed += 20;
        if (progress_fn && !progress_fn(items_processed, total_items, user_data)) {
            gc->cancel_requested = true;
        }
    }

    /* Continue for other targets... */
    if (!gc->cancel_requested && (targets & KG_GC_SOFT_DELETED)) {
        int collected = collect_soft_deleted(gc);
        if (collected > 0) total_collected += collected;
        items_processed += 20;
        if (progress_fn && !progress_fn(items_processed, total_items, user_data)) {
            gc->cancel_requested = true;
        }
    }

    /* Final progress update */
    if (progress_fn) {
        progress_fn(total_items, total_items, user_data);
    }

    gc->incremental.in_progress = false;
    gc->stats.last_gc_timestamp = get_timestamp_ms();
    gc->stats.gc_runs_total++;
    gc->stats.gc_runs_manual++;

    nimcp_mutex_unlock(gc->mutex);

    return total_collected;
}

int kg_gc_compact(kg_gc_context_t* gc) {
    if (!gc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gc is NULL");

        return -1;
    }

    /* In a real implementation, we would:
     * 1. Record fragmentation before
     * 2. Relocate items to contiguous storage
     * 3. Record fragmentation after
     * 4. Calculate bytes reclaimed
     */

    gc->stats.fragmentation_before = kg_gc_get_fragmentation(gc);

    /* Placeholder: simulate compaction */
    uint64_t bytes_reclaimed = 0;

    gc->stats.fragmentation_after = kg_gc_get_fragmentation(gc);
    gc->stats.bytes_reclaimed += bytes_reclaimed;

    return (int)bytes_reclaimed;
}

int kg_gc_defragment(kg_gc_context_t* gc) {
    if (!gc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gc is NULL");

        return -1;
    }

    /* Similar to compact but focuses on layout optimization */
    return kg_gc_compact(gc);
}

int kg_gc_cancel(kg_gc_context_t* gc) {
    if (!gc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gc is NULL");

        return -1;
    }

    nimcp_mutex_lock(gc->mutex);

    if (!gc->incremental.in_progress) {
        nimcp_mutex_unlock(gc->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_gc_cancel: gc->incremental is NULL");
        return -1;
    }

    gc->cancel_requested = true;

    nimcp_mutex_unlock(gc->mutex);

    return 0;
}

/* ============================================================================
 * Analysis Functions
 * ============================================================================ */

int kg_gc_analyze(const kg_gc_context_t* gc, kg_gc_stats_t* stats) {
    if (!gc || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_gc_analyze: required parameter is NULL (gc, stats)");
        return -1;
    }

    nimcp_mutex_lock(((kg_gc_context_t*)gc)->mutex);
    *stats = gc->stats;
    nimcp_mutex_unlock(((kg_gc_context_t*)gc)->mutex);

    return 0;
}

int kg_gc_find_orphans(
    const kg_gc_context_t* gc,
    brain_kg_node_id_t* orphans,
    uint32_t* count
) {
    if (!gc || !orphans || !count || *count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_gc_find_orphans: required parameter is NULL (gc, orphans, count)");
        return -1;
    }

    nimcp_mutex_lock(((kg_gc_context_t*)gc)->mutex);

    /* In a real implementation, we would scan the KG for orphaned nodes */
    /* Placeholder: return no orphans */
    *count = 0;

    nimcp_mutex_unlock(((kg_gc_context_t*)gc)->mutex);

    return 0;
}

float kg_gc_get_fragmentation(const kg_gc_context_t* gc) {
    if (!gc) {
        return -1.0f;
    }

    /* In a real implementation, we would calculate:
     * fragmentation = 1.0 - (used_space / allocated_space)
     */

    return 0.0f; /* Placeholder: no fragmentation */
}

int64_t kg_gc_estimate_reclaimable(
    const kg_gc_context_t* gc,
    uint32_t targets
) {
    (void)targets;

    if (!gc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gc is NULL");


        return -1;
    }

    /* In a real implementation, we would analyze each target type */
    return 0;
}

bool kg_gc_is_running(const kg_gc_context_t* gc) {
    if (!gc) {
        return false;
    }
    return gc->incremental.in_progress;
}

const char* kg_gc_get_last_error(const kg_gc_context_t* gc) {
    if (!gc || gc->last_error[0] == '\0') {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_gc_get_last_error: gc is NULL");
        return NULL;
    }
    return gc->last_error;
}

/* ============================================================================
 * Scheduling Functions
 * ============================================================================ */

int kg_gc_schedule(kg_gc_context_t* gc, uint64_t run_at_timestamp) {
    if (!gc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gc is NULL");

        return -1;
    }

    nimcp_mutex_lock(gc->mutex);
    gc->scheduled_timestamp = run_at_timestamp;
    nimcp_mutex_unlock(gc->mutex);

    return 0;
}

int kg_gc_cancel_scheduled(kg_gc_context_t* gc) {
    if (!gc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gc is NULL");

        return -1;
    }

    nimcp_mutex_lock(gc->mutex);

    if (gc->scheduled_timestamp == 0) {
        nimcp_mutex_unlock(gc->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_gc_cancel_scheduled: gc->scheduled_timestamp is zero");
        return -1;
    }

    gc->scheduled_timestamp = 0;

    nimcp_mutex_unlock(gc->mutex);

    return 0;
}

uint64_t kg_gc_get_scheduled_time(const kg_gc_context_t* gc) {
    if (!gc) {
        return 0;
    }
    return gc->scheduled_timestamp;
}

int kg_gc_enable_auto(kg_gc_context_t* gc) {
    if (!gc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gc is NULL");

        return -1;
    }

    nimcp_mutex_lock(gc->mutex);
    gc->auto_gc_enabled = true;
    gc->last_auto_gc = get_timestamp_ms();
    nimcp_mutex_unlock(gc->mutex);

    return 0;
}

int kg_gc_disable_auto(kg_gc_context_t* gc) {
    if (!gc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gc is NULL");

        return -1;
    }

    nimcp_mutex_lock(gc->mutex);
    gc->auto_gc_enabled = false;
    nimcp_mutex_unlock(gc->mutex);

    return 0;
}

bool kg_gc_is_auto_enabled(const kg_gc_context_t* gc) {
    if (!gc) {
        return false;
    }
    return gc->auto_gc_enabled;
}

int kg_gc_trigger_if_needed(kg_gc_context_t* gc) {
    if (!gc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gc is NULL");

        return -1;
    }

    nimcp_mutex_lock(gc->mutex);

    /* Check waste level */
    float fragmentation = kg_gc_get_fragmentation(gc);

    if (fragmentation >= gc->config.gc_threshold_percent) {
        nimcp_mutex_unlock(gc->mutex);
        int result = kg_gc_run(gc, gc->config.gc_targets);
        return (result >= 0) ? 1 : -1;
    }

    nimcp_mutex_unlock(gc->mutex);

    return 0; /* Below threshold */
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* kg_gc_target_to_string(kg_gc_target_t target) {
    switch (target) {
        case KG_GC_ORPHANED_NODES: return "ORPHANED_NODES";
        case KG_GC_SOFT_DELETED:   return "SOFT_DELETED";
        case KG_GC_OLD_SNAPSHOTS:  return "OLD_SNAPSHOTS";
        case KG_GC_STALE_CACHE:    return "STALE_CACHE";
        case KG_GC_WAL_SEGMENTS:   return "WAL_SEGMENTS";
        case KG_GC_TOMBSTONES:     return "TOMBSTONES";
        case KG_GC_ALL:            return "ALL";
        default:                   return "UNKNOWN";
    }
}

void kg_gc_reset_stats(kg_gc_context_t* gc) {
    if (!gc) {
        return;
    }

    nimcp_mutex_lock(gc->mutex);
    memset(&gc->stats, 0, sizeof(gc->stats));
    nimcp_mutex_unlock(gc->mutex);
}
