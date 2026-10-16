//=============================================================================
// nimcp_knowledge_cow.h - Copy-on-Write Wrapper for Knowledge System
//=============================================================================
/**
 * @file nimcp_knowledge_cow.h
 * @brief COW wrapper enabling shared knowledge bases across multiple brains
 *
 * WHAT: Thin COW wrapper around knowledge_system_t using page-level COW
 * WHY:  95% memory savings when multiple brains share common knowledge
 * HOW:  Page-level COW (nimcp_page_cow.h) applied to knowledge system memory
 *
 * ARCHITECTURE:
 *
 *   Knowledge COW Sharing Model:
 *   ┌──────────────────────────────────────────────────────────────────────┐
 *   │  Base Knowledge System (20MB common knowledge)                       │
 *   │  ┌────────────────────────────────────────────────────────────────┐ │
 *   │  │  Page-Level COW Region                                         │ │
 *   │  │  [Page 0][Page 1][Page 2]...[Page N]  (all shared initially)  │ │
 *   │  └────────────────────────────────────────────────────────────────┘ │
 *   └──────────────────────────────────────────────────────────────────────┘
 *                    │
 *        ┌──────────┼──────────┬──────────┐
 *        │          │          │          │
 *   ┌────▼────┐┌────▼────┐┌────▼────┐┌────▼────┐
 *   │ Brain 1 ││ Brain 2 ││ Brain 3 ││ Brain N │
 *   │ [View]  ││ [View]  ││ [View]  ││ [View]  │
 *   │ Shared  ││ Shared  ││ +2 priv ││ Shared  │
 *   └─────────┘└─────────┘└─────────┘└─────────┘
 *
 *   Brain 3 personalized 2 pages → only 2 pages copied
 *   Memory: 20MB base + 8KB (2 pages) = 20.008MB for 4 brains
 *   Without COW: 20MB × 4 = 80MB
 *   Savings: 75% (more brains = more savings)
 *
 * USE CASES:
 * 1. Multi-brain deployments sharing common knowledge
 * 2. A/B testing with personalized knowledge variants
 * 3. Checkpointing knowledge state for rollback
 * 4. P2P knowledge replication with local modifications
 *
 * THREAD SAFETY:
 * - Read operations are lock-free
 * - Write operations trigger page-level COW atomically
 * - Multiple readers can access shared pages concurrently
 *
 * @author NIMCP Development Team
 * @date 2025-11-24
 * @version 1.0.0
 */

#ifndef NIMCP_KNOWLEDGE_COW_H
#define NIMCP_KNOWLEDGE_COW_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "utils/memory/nimcp_page_cow.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

//=============================================================================
// Types
//=============================================================================

/**
 * @brief COW-enabled knowledge base handle (opaque)
 *
 * Wraps a knowledge system with page-level COW semantics
 */
typedef struct knowledge_cow_base_struct* knowledge_cow_base_t;

/**
 * @brief COW view into a knowledge base (opaque)
 *
 * Each brain gets a view that shares pages until modification
 */
typedef struct knowledge_cow_view_struct* knowledge_cow_view_t;

/**
 * @brief Knowledge COW configuration
 */
typedef struct {
    size_t max_knowledge_size;      /**< Maximum knowledge data size in bytes */
    bool enable_tracking;           /**< Track COW statistics */
    bool enable_snapshots;          /**< Enable snapshot/rollback support */
} knowledge_cow_config_t;

/**
 * @brief Knowledge COW statistics
 */
typedef struct {
    size_t total_views;             /**< Total views created */
    size_t active_views;            /**< Currently active views */
    size_t shared_pages;            /**< Pages still shared across views */
    size_t private_pages;           /**< Pages made private (total) */
    size_t memory_saved_bytes;      /**< Memory saved by sharing */
    size_t total_knowledge_bytes;   /**< Total knowledge data size */
    uint64_t cow_triggers;          /**< Number of COW copy operations */
} knowledge_cow_stats_t;

//=============================================================================
// Knowledge COW Base API
//=============================================================================

/**
 * @brief Create a COW-enabled knowledge base
 *
 * WHAT: Creates a shareable knowledge base with COW semantics
 * WHY:  Enable multiple brains to share common knowledge efficiently
 * HOW:  Wraps knowledge data in page-level COW region
 *
 * @param config Configuration for COW behavior
 * @param initial_data Initial knowledge data (can be NULL for empty)
 * @param data_size Size of initial data in bytes
 * @return Knowledge COW base handle or NULL on failure
 *
 * EXAMPLE:
 * ```c
 * knowledge_cow_config_t config = {
 *     .max_knowledge_size = 50 * 1024 * 1024,  // 50MB max
 *     .enable_tracking = true,
 *     .enable_snapshots = true,
 * };
 * knowledge_cow_base_t base = knowledge_cow_base_create(&config, kb_data, kb_size);
 * ```
 */
NIMCP_EXPORT knowledge_cow_base_t knowledge_cow_base_create(
    const knowledge_cow_config_t* config,
    const void* initial_data,
    size_t data_size
);

/**
 * @brief Destroy a COW knowledge base
 *
 * WARNING: All views must be destroyed before destroying the base
 *
 * @param base Knowledge COW base to destroy
 */
NIMCP_EXPORT void knowledge_cow_base_destroy(knowledge_cow_base_t base);

/**
 * @brief Get statistics for the knowledge COW base
 *
 * @param base Knowledge COW base
 * @param stats Output statistics
 * @return true on success
 */
NIMCP_EXPORT bool knowledge_cow_base_get_stats(
    knowledge_cow_base_t base,
    knowledge_cow_stats_t* stats
);

/**
 * @brief Update base knowledge (propagates to all shared views)
 *
 * CAUTION: This updates the shared template. Views that haven't
 * made private copies will see the new data.
 *
 * @param base Knowledge COW base
 * @param data New knowledge data
 * @param data_size Size of data
 * @param offset Offset into knowledge base (0 for beginning)
 * @return true on success
 */
NIMCP_EXPORT bool knowledge_cow_base_update(
    knowledge_cow_base_t base,
    const void* data,
    size_t data_size,
    size_t offset
);

//=============================================================================
// Knowledge COW View API
//=============================================================================

/**
 * @brief Create a view into the knowledge COW base
 *
 * WHAT: Creates a view that shares pages with the base
 * WHY:  Each brain gets its own view for potential personalization
 * HOW:  O(1) creation - just increments page refcounts
 *
 * @param base Knowledge COW base
 * @return View handle or NULL on failure
 *
 * EXAMPLE:
 * ```c
 * // Each brain gets a view
 * for (int i = 0; i < num_brains; i++) {
 *     brains[i].knowledge_view = knowledge_cow_view_create(shared_kb);
 *     // All views share same physical pages - no copy yet
 * }
 * ```
 */
NIMCP_EXPORT knowledge_cow_view_t knowledge_cow_view_create(knowledge_cow_base_t base);

/**
 * @brief Clone a view (COW clone from existing view)
 *
 * @param source Source view to clone
 * @return New view handle or NULL on failure
 */
NIMCP_EXPORT knowledge_cow_view_t knowledge_cow_view_clone(knowledge_cow_view_t source);

/**
 * @brief Destroy a knowledge COW view
 *
 * @param view View to destroy
 */
NIMCP_EXPORT void knowledge_cow_view_destroy(knowledge_cow_view_t view);

/**
 * @brief Get read-only pointer to knowledge data
 *
 * WHAT: Returns pointer for reading without triggering COW
 * WHY:  Fast read access to shared knowledge
 * HOW:  Direct pointer to (possibly shared) memory
 *
 * @param view Knowledge COW view
 * @return const pointer to knowledge data
 *
 * IMPORTANT: Do NOT cast away const - use knowledge_cow_view_write for modifications
 */
NIMCP_EXPORT const void* knowledge_cow_view_read(knowledge_cow_view_t view);

/**
 * @brief Get writable pointer to knowledge data
 *
 * WHAT: Returns writable pointer, preparing for potential COW
 * WHY:  Enable modifications with automatic page-level COW
 * HOW:  Marks view as writable, COW happens on actual write
 *
 * @param view Knowledge COW view
 * @return Writable pointer to knowledge data
 *
 * NOTE: Actual COW copy happens lazily when you write to shared pages
 *
 * EXAMPLE:
 * ```c
 * // Brain 3 personalizes some knowledge
 * void* kb = knowledge_cow_view_write(brain3.knowledge_view);
 * // Modify at offset 1000 - only that page gets copied
 * memcpy((char*)kb + 1000, new_data, sizeof(new_data));
 * ```
 */
NIMCP_EXPORT void* knowledge_cow_view_write(knowledge_cow_view_t view);

/**
 * @brief Pre-emptively make a region private
 *
 * WHAT: Forces COW copy for a byte range before writing
 * WHY:  Batch COW operations when you know you'll modify a region
 * HOW:  Copies affected pages to private storage
 *
 * @param view Knowledge COW view
 * @param offset Byte offset into knowledge data
 * @param size Number of bytes to make private
 * @return true on success
 */
NIMCP_EXPORT bool knowledge_cow_view_make_region_private(
    knowledge_cow_view_t view,
    size_t offset,
    size_t size
);

/**
 * @brief Get memory saved by this view
 *
 * @param view Knowledge COW view
 * @return Bytes saved by sharing pages
 */
NIMCP_EXPORT size_t knowledge_cow_view_get_memory_saved(knowledge_cow_view_t view);

/**
 * @brief Check if view has any private pages
 *
 * @param view Knowledge COW view
 * @return true if view has modified (private) pages
 */
NIMCP_EXPORT bool knowledge_cow_view_is_modified(knowledge_cow_view_t view);

/**
 * @brief Get number of private pages in view
 *
 * @param view Knowledge COW view
 * @return Number of pages that have been copied
 */
NIMCP_EXPORT size_t knowledge_cow_view_get_private_page_count(knowledge_cow_view_t view);

//=============================================================================
// Knowledge COW Snapshot API
//=============================================================================

/**
 * @brief Knowledge snapshot handle
 */
typedef struct knowledge_cow_snapshot_struct* knowledge_cow_snapshot_t;

/**
 * @brief Create snapshot of current view state
 *
 * WHAT: Instant snapshot for rollback support
 * WHY:  Checkpoint knowledge before risky operations
 * HOW:  O(1) - shares pages with current view
 *
 * @param view View to snapshot
 * @return Snapshot handle or NULL on failure
 */
NIMCP_EXPORT knowledge_cow_snapshot_t knowledge_cow_snapshot_create(knowledge_cow_view_t view);

/**
 * @brief Restore view from snapshot
 *
 * WHAT: Roll back view to snapshot state
 * WHY:  Undo failed knowledge modifications
 * HOW:  Discards private pages, re-shares snapshot pages
 *
 * @param view View to restore
 * @param snapshot Snapshot to restore from
 * @return true on success
 */
NIMCP_EXPORT bool knowledge_cow_snapshot_restore(
    knowledge_cow_view_t view,
    knowledge_cow_snapshot_t snapshot
);

/**
 * @brief Destroy a snapshot
 *
 * @param snapshot Snapshot to destroy
 */
NIMCP_EXPORT void knowledge_cow_snapshot_destroy(knowledge_cow_snapshot_t snapshot);

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Get default configuration
 *
 * @param max_size Maximum knowledge size in bytes
 * @return Default configuration
 */
static inline knowledge_cow_config_t knowledge_cow_default_config(size_t max_size) {
    knowledge_cow_config_t config = {
        .max_knowledge_size = max_size,
        .enable_tracking = true,
        .enable_snapshots = true,
    };
    return config;
}

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_KNOWLEDGE_COW_H
