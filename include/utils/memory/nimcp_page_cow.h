//=============================================================================
// nimcp_page_cow.h - Page-Level Copy-on-Write for Large Data Structures
//=============================================================================
/**
 * @file nimcp_page_cow.h
 * @brief Page-level COW using mmap/mprotect for fine-grained memory sharing
 *
 * WHAT: Page-level COW for large matrices and data structures
 * WHY:  Object-level COW copies entire objects; page-level copies only 4KB pages
 * HOW:  mmap for allocation, mprotect for read-only, SIGSEGV handler for COW
 *
 * ARCHITECTURE:
 *
 *   Page-Level COW Memory Layout:
 *   ┌──────────────────────────────────────────────────────────────────────┐
 *   │  Shared Pages (read-only, multiple references)                       │
 *   │  ┌──────────┬──────────┬──────────┬──────────┬──────────┐           │
 *   │  │  Page 0  │  Page 1  │  Page 2  │  Page 3  │  Page N  │           │
 *   │  │  (4KB)   │  (4KB)   │  (4KB)   │  (4KB)   │  (4KB)   │           │
 *   │  │  REF=3   │  REF=2   │  REF=3   │  REF=1   │  REF=3   │           │
 *   │  └──────────┴──────────┴──────────┴──────────┴──────────┘           │
 *   └──────────────────────────────────────────────────────────────────────┘
 *
 *   On Write to Page 1:
 *   ┌──────────────────────────────────────────────────────────────────────┐
 *   │  1. SIGSEGV triggered (page is read-only)                            │
 *   │  2. Handler allocates new private page                               │
 *   │  3. Copies 4KB from shared page                                      │
 *   │  4. Remaps view to private page                                      │
 *   │  5. Decrements shared page refcount                                  │
 *   │  6. Write proceeds to private page                                   │
 *   └──────────────────────────────────────────────────────────────────────┘
 *
 * USE CASES:
 * 1. Neural Network Weights: 50MB weights, modify few pages during fine-tuning
 * 2. Knowledge Graphs: Large graphs with localized modifications
 * 3. Brain Snapshots: Instant snapshots with page-level delta tracking
 * 4. P2P Replication: Share weights across nodes, COW on learning
 *
 * PERFORMANCE:
 * - Clone: O(1) - Just reference, no page copy
 * - First Write: O(1) per page - Copy single 4KB page (~1us)
 * - Subsequent Writes: O(1) - Direct write to private page
 * - Memory Overhead: ~8 bytes per 4KB page (0.2%)
 *
 * THREAD SAFETY:
 * - Signal handler is async-signal-safe
 * - Page table protected by spinlock
 * - Lock-free reads to shared pages
 *
 * LIMITATIONS:
 * - POSIX-only (mmap, mprotect, SIGSEGV)
 * - Minimum granularity is page size (typically 4KB)
 * - Signal handler overhead on first write to each page
 *
 * @author NIMCP Development Team
 * @date 2025-11-24
 * @version 1.0.0
 */

#ifndef NIMCP_PAGE_COW_H
#define NIMCP_PAGE_COW_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>

#ifdef __cplusplus
extern "C" {
#endif

// Export macro (for shared library builds)
#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

//=============================================================================
// Types and Constants
//=============================================================================

/**
 * @brief Page size constant (typically 4KB)
 */
#define PAGE_COW_PAGE_SIZE 4096

/**
 * @brief Maximum concurrent page COW regions
 */
#define PAGE_COW_MAX_REGIONS 256

/**
 * @brief Page-level COW region handle (opaque)
 */
typedef struct page_cow_region_struct* page_cow_region_t;

/**
 * @brief Page-level COW view handle (opaque)
 *
 * A view is a reference to a shared region. Multiple views can share
 * the same underlying pages until one of them writes.
 */
typedef struct page_cow_view_struct* page_cow_view_t;

/**
 * @brief Page state enumeration
 */
typedef enum {
    PAGE_STATE_SHARED,      /**< Page is shared (read-only) */
    PAGE_STATE_PRIVATE,     /**< Page is private (writable) */
    PAGE_STATE_UNMAPPED     /**< Page is not mapped */
} page_state_t;

/**
 * @brief Page-level COW configuration
 */
typedef struct {
    size_t size;                    /**< Total size in bytes (will be page-aligned) */
    bool enable_tracking;           /**< Track statistics */
    bool zero_on_allocate;          /**< Zero-initialize new pages */
    size_t max_private_pages;       /**< Max private pages per view (0 = unlimited) */
} page_cow_config_t;

/**
 * @brief Page-level COW statistics
 */
typedef struct {
    // Region statistics
    size_t total_pages;             /**< Total pages in region */
    size_t shared_pages;            /**< Pages still shared */
    size_t private_pages;           /**< Pages made private (total across views) */

    // View statistics
    size_t total_views;             /**< Total views created */
    size_t active_views;            /**< Currently active views */

    // Performance statistics
    uint64_t cow_faults;            /**< Total COW page faults handled */
    uint64_t total_bytes_copied;    /**< Total bytes copied by COW */
    uint64_t total_copy_time_ns;    /**< Total time spent in COW copies */

    // Memory statistics
    size_t memory_used_bytes;       /**< Total memory used */
    size_t memory_saved_bytes;      /**< Memory saved by sharing */
} page_cow_stats_t;

//=============================================================================
// Page-Level COW Region API
//=============================================================================

/**
 * @brief Initialize page-level COW subsystem
 *
 * WHAT: Installs SIGSEGV handler for COW page faults
 * WHY:  Must be called before creating any page COW regions
 * HOW:  sigaction() to install custom handler
 *
 * @return true on success, false on failure
 *
 * THREAD SAFETY: Call once from main thread before any page COW operations
 * WARNING: This modifies the process-wide SIGSEGV handler
 */
NIMCP_EXPORT bool page_cow_init(void);

/**
 * @brief Shutdown page-level COW subsystem
 *
 * WHAT: Restores original SIGSEGV handler
 * WHY:  Clean shutdown
 * HOW:  Restore saved handler
 *
 * WARNING: All page COW regions must be destroyed before calling this
 */
NIMCP_EXPORT void page_cow_shutdown(void);

/**
 * @brief Create a page-level COW region
 *
 * WHAT: Allocates a page-aligned memory region with COW support
 * WHY:  Initial allocation that can be shared via views
 * HOW:  mmap anonymous mapping
 *
 * @param config Configuration for the region
 * @param initial_data Initial data to copy (NULL = zero-initialize)
 * @return Region handle or NULL on failure
 *
 * COMPLEXITY: O(n) where n = number of pages (for initial copy)
 * MEMORY: size bytes (page-aligned up)
 *
 * EXAMPLE:
 * ```c
 * page_cow_config_t config = {
 *     .size = 50 * 1024 * 1024,  // 50MB weights
 *     .enable_tracking = true,
 * };
 * page_cow_region_t region = page_cow_region_create(&config, weights_data);
 * ```
 */
NIMCP_EXPORT page_cow_region_t page_cow_region_create(
    const page_cow_config_t* config,
    const void* initial_data
);

/**
 * @brief Destroy a page-level COW region
 *
 * WHAT: Frees the region and all associated pages
 * WHY:  Clean shutdown
 * HOW:  munmap all pages, free metadata
 *
 * @param region Region handle
 *
 * WARNING: All views must be destroyed before destroying the region
 */
NIMCP_EXPORT void page_cow_region_destroy(page_cow_region_t region);

/**
 * @brief Get region statistics
 *
 * @param region Region handle
 * @param stats Output statistics
 * @return true on success
 */
NIMCP_EXPORT bool page_cow_region_get_stats(
    page_cow_region_t region,
    page_cow_stats_t* stats
);

/**
 * @brief Get the size of the region in bytes
 *
 * @param region Region handle
 * @return Size in bytes (page-aligned)
 */
NIMCP_EXPORT size_t page_cow_region_get_size(page_cow_region_t region);

//=============================================================================
// Page-Level COW View API
//=============================================================================

/**
 * @brief Create a view into a page-level COW region
 *
 * WHAT: Creates a new view that shares pages with the region
 * WHY:  O(1) clone operation without copying data
 * HOW:  Reference counting on shared pages
 *
 * @param region Region to create view from
 * @return View handle or NULL on failure
 *
 * COMPLEXITY: O(1) - Just increments reference counts
 * MEMORY: ~64 bytes for view metadata + page table
 *
 * EXAMPLE:
 * ```c
 * // Clone for each worker thread (instant, shares all pages)
 * for (int i = 0; i < num_workers; i++) {
 *     views[i] = page_cow_view_create(region);
 *     // All views share same physical pages
 * }
 * ```
 */
NIMCP_EXPORT page_cow_view_t page_cow_view_create(page_cow_region_t region);

/**
 * @brief Create a view from an existing view (COW clone)
 *
 * WHAT: Creates a COW clone of an existing view
 * WHY:  Enable fork-like semantics for views
 * HOW:  Share all pages, mark as read-only
 *
 * @param source Source view to clone
 * @return New view handle or NULL on failure
 *
 * COMPLEXITY: O(1) - Reference count manipulation only
 */
NIMCP_EXPORT page_cow_view_t page_cow_view_clone(page_cow_view_t source);

/**
 * @brief Destroy a view
 *
 * WHAT: Releases the view and decrements page reference counts
 * WHY:  Free resources when view no longer needed
 * HOW:  Decrement refcounts, free private pages
 *
 * @param view View handle
 */
NIMCP_EXPORT void page_cow_view_destroy(page_cow_view_t view);

/**
 * @brief Get read-only pointer to view data
 *
 * WHAT: Returns pointer for reading without triggering COW
 * WHY:  Fast read access to shared data
 * HOW:  Direct pointer to mapped memory
 *
 * @param view View handle
 * @return const pointer to data, or NULL on error
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Lock-free
 *
 * IMPORTANT: Do NOT cast away const and write to this pointer!
 *            It will cause SIGSEGV and undefined behavior.
 */
NIMCP_EXPORT const void* page_cow_view_read(page_cow_view_t view);

/**
 * @brief Get writable pointer to view data
 *
 * WHAT: Returns writable pointer, marking view as potentially dirty
 * WHY:  Enable writes with automatic COW on page faults
 * HOW:  Returns same memory, but writes trigger SIGSEGV handler
 *
 * @param view View handle
 * @return Writable pointer to data, or NULL on error
 *
 * COMPLEXITY: O(1) for getting pointer
 *             O(page_size) on first write to each shared page (COW)
 *
 * NOTE: The actual COW happens lazily when you write, not when you call this.
 *       This just prepares the view for potential writes.
 *
 * EXAMPLE:
 * ```c
 * float* weights = (float*)page_cow_view_write(view);
 * weights[1000] = 0.5f;  // May trigger COW if page was shared
 * weights[1001] = 0.6f;  // No COW - same page already private
 * ```
 */
NIMCP_EXPORT void* page_cow_view_write(page_cow_view_t view);

/**
 * @brief Force a specific page to become private
 *
 * WHAT: Pre-emptively triggers COW for a page
 * WHY:  Batch COW operations when you know you'll write
 * HOW:  Allocate private page, copy, remap
 *
 * @param view View handle
 * @param page_index Page index (0-based)
 * @return true on success, false on failure
 *
 * COMPLEXITY: O(page_size) - one page copy
 */
NIMCP_EXPORT bool page_cow_view_make_page_private(
    page_cow_view_t view,
    size_t page_index
);

/**
 * @brief Force a range of pages to become private
 *
 * WHAT: Pre-emptively triggers COW for multiple pages
 * WHY:  Reduce signal handler overhead for known write regions
 * HOW:  Batch page allocation and copying
 *
 * @param view View handle
 * @param start_page Starting page index
 * @param num_pages Number of pages to make private
 * @return Number of pages made private (0 on error)
 *
 * COMPLEXITY: O(num_pages * page_size)
 */
NIMCP_EXPORT size_t page_cow_view_make_range_private(
    page_cow_view_t view,
    size_t start_page,
    size_t num_pages
);

/**
 * @brief Get the state of a specific page
 *
 * @param view View handle
 * @param page_index Page index
 * @return Page state
 */
NIMCP_EXPORT page_state_t page_cow_view_get_page_state(
    page_cow_view_t view,
    size_t page_index
);

/**
 * @brief Get the number of private pages in this view
 *
 * @param view View handle
 * @return Number of private pages
 */
NIMCP_EXPORT size_t page_cow_view_get_private_page_count(page_cow_view_t view);

/**
 * @brief Get the number of shared pages in this view
 *
 * @param view View handle
 * @return Number of shared pages
 */
NIMCP_EXPORT size_t page_cow_view_get_shared_page_count(page_cow_view_t view);

/**
 * @brief Calculate memory savings for this view
 *
 * @param view View handle
 * @return Bytes saved by sharing pages
 */
NIMCP_EXPORT size_t page_cow_view_get_memory_saved(page_cow_view_t view);

//=============================================================================
// Snapshot API (Phase 4 - Built on Page-Level COW)
//=============================================================================

/**
 * @brief Snapshot handle for fast state save/restore
 */
typedef struct page_cow_snapshot_struct* page_cow_snapshot_t;

/**
 * @brief Create a snapshot of a view
 *
 * WHAT: Creates instant snapshot with COW semantics
 * WHY:  Fast checkpointing without copying data
 * HOW:  Clone view, mark all pages as shared
 *
 * @param view View to snapshot
 * @return Snapshot handle or NULL on failure
 *
 * COMPLEXITY: O(1) - No data copying
 * MEMORY: ~32 bytes metadata only
 *
 * EXAMPLE:
 * ```c
 * // Before risky operation
 * page_cow_snapshot_t snap = page_cow_snapshot_create(view);
 *
 * // Try operation
 * if (!risky_operation(view)) {
 *     // Rollback
 *     page_cow_snapshot_restore(view, snap);
 * }
 *
 * page_cow_snapshot_destroy(snap);
 * ```
 */
NIMCP_EXPORT page_cow_snapshot_t page_cow_snapshot_create(page_cow_view_t view);

/**
 * @brief Restore a view from a snapshot
 *
 * WHAT: Restores view to snapshot state
 * WHY:  Fast rollback after failed operation
 * HOW:  Discard private pages, re-share snapshot pages
 *
 * @param view View to restore
 * @param snapshot Snapshot to restore from
 * @return true on success, false on failure
 *
 * COMPLEXITY: O(num_private_pages) - Release private pages
 *
 * WARNING: This discards all changes made since the snapshot
 */
NIMCP_EXPORT bool page_cow_snapshot_restore(
    page_cow_view_t view,
    page_cow_snapshot_t snapshot
);

/**
 * @brief Destroy a snapshot
 *
 * WHAT: Releases snapshot resources
 * WHY:  Free memory when snapshot no longer needed
 * HOW:  Decrement refcounts, free if last reference
 *
 * @param snapshot Snapshot handle
 */
NIMCP_EXPORT void page_cow_snapshot_destroy(page_cow_snapshot_t snapshot);

/**
 * @brief Get the number of pages that have diverged from snapshot
 *
 * @param view Current view
 * @param snapshot Snapshot to compare against
 * @return Number of pages that differ
 */
NIMCP_EXPORT size_t page_cow_snapshot_get_delta_pages(
    page_cow_view_t view,
    page_cow_snapshot_t snapshot
);

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Get page index for a byte offset
 *
 * @param offset Byte offset into region
 * @return Page index
 */
static inline size_t page_cow_offset_to_page(size_t offset) {
    return offset / PAGE_COW_PAGE_SIZE;
}

/**
 * @brief Get byte offset for start of page
 *
 * @param page_index Page index
 * @return Byte offset
 */
static inline size_t page_cow_page_to_offset(size_t page_index) {
    return page_index * PAGE_COW_PAGE_SIZE;
}

/**
 * @brief Round size up to page boundary
 *
 * @param size Size in bytes
 * @return Page-aligned size
 */
static inline size_t page_cow_align_size(size_t size) {
    return (size + PAGE_COW_PAGE_SIZE - 1) & ~(PAGE_COW_PAGE_SIZE - 1);
}

/**
 * @brief Calculate number of pages for a given size
 *
 * @param size Size in bytes
 * @return Number of pages (rounded up)
 */
static inline size_t page_cow_num_pages(size_t size) {
    return (size + PAGE_COW_PAGE_SIZE - 1) / PAGE_COW_PAGE_SIZE;
}

/**
 * @brief Get default configuration
 *
 * @param size Size in bytes
 * @return Default configuration
 */
static inline page_cow_config_t page_cow_default_config(size_t size) {
    page_cow_config_t config = {
        .size = size,
        .enable_tracking = true,
        .zero_on_allocate = true,
        .max_private_pages = 0,  // Unlimited
    };
    return config;
}

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_PAGE_COW_H
