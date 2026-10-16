//=============================================================================
// nimcp_unified_memory.h - Unified Memory Manager with CoW Integration
//=============================================================================
/**
 * @file nimcp_unified_memory.h
 * @brief Unified memory management with automatic CoW strategy selection
 *
 * WHAT: Single API for memory allocation with Copy-on-Write support
 * WHY:  Simplify memory management across brain modules, auto-select best strategy
 * HOW:  Abstracts object-level and page-level CoW behind unified interface
 *
 * ARCHITECTURE:
 *
 *   Unified Memory Manager:
 *   ┌──────────────────────────────────────────────────────────────────────┐
 *   │                    Unified Memory API                                │
 *   │  unified_mem_create, unified_mem_acquire, unified_mem_write, etc.   │
 *   └──────────────────────────────────────────────────────────────────────┘
 *                                    │
 *                    ┌───────────────┴───────────────┐
 *                    │      Strategy Selection       │
 *                    │   (based on size/config)      │
 *                    └───────────────┬───────────────┘
 *                                    │
 *          ┌─────────────────────────┼─────────────────────────┐
 *          │                         │                         │
 *          ▼                         ▼                         ▼
 *   ┌──────────────┐         ┌──────────────┐         ┌──────────────┐
 *   │ Object-Level │         │  Page-Level  │         │  Direct Pool │
 *   │     CoW      │         │     CoW      │         │  (no CoW)    │
 *   │ (cow_manager)│         │ (page_cow)   │         │ (memory_pool)│
 *   └──────────────┘         └──────────────┘         └──────────────┘
 *          │                         │                         │
 *          └─────────┬───────────────┼─────────────────────────┘
 *                    │               │
 *                    ▼               ▼
 *            ┌──────────────┐ ┌──────────────┐
 *            │ Memory Pool  │ │ Page Pool    │
 *            │ (objects)    │ │ (4KB pages)  │
 *            └──────────────┘ └──────────────┘
 *
 * STRATEGY SELECTION:
 * - size < PAGE_SIZE (4KB): Object-level CoW with memory pool
 * - size >= PAGE_SIZE && CoW needed: Page-level CoW
 * - size >= PAGE_SIZE && no CoW: Direct pool allocation
 * - Override via config: Force specific strategy
 *
 * USE CASES:
 * 1. Weight Matrices (50MB): Page-level CoW for efficient cloning
 * 2. Activation Buffers (4KB-64KB): Object-level CoW or direct pool
 * 3. Small Structures (<4KB): Object-level CoW
 * 4. Checkpoints: Instant snapshots via page-level CoW
 *
 * THREAD SAFETY:
 * - All operations are thread-safe
 * - Lock-free reads to shared data
 * - Atomic operations for reference counting
 *
 * @author NIMCP Development Team
 * @date 2025-11-27
 * @version 1.0.0
 */

#ifndef NIMCP_UNIFIED_MEMORY_H
#define NIMCP_UNIFIED_MEMORY_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "utils/memory/nimcp_memory_pool.h"
#include "utils/memory/nimcp_cow_manager.h"
#include "utils/memory/nimcp_page_cow.h"

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

/**
 * @brief Default size threshold for page-level CoW (64KB)
 *
 * Allocations >= this size use page-level CoW when CoW is requested.
 * Below this threshold, object-level CoW is used.
 */
#define UNIFIED_MEM_PAGE_THRESHOLD (64 * 1024)

/**
 * @brief Maximum unified memory managers per process
 */
#define UNIFIED_MEM_MAX_MANAGERS 64

/**
 * @brief Default page pool size (1024 pages = 4MB)
 */
#define UNIFIED_MEM_DEFAULT_PAGE_POOL_SIZE 1024

//=============================================================================
// Types and Enumerations
//=============================================================================

/**
 * @brief Memory allocation strategy
 */
typedef enum {
    UNIFIED_STRATEGY_AUTO,          /**< Auto-select based on size and config */
    UNIFIED_STRATEGY_OBJECT_COW,    /**< Force object-level CoW (cow_manager) */
    UNIFIED_STRATEGY_PAGE_COW,      /**< Force page-level CoW (page_cow) */
    UNIFIED_STRATEGY_POOL_DIRECT,   /**< Direct pool allocation (no CoW) */
    UNIFIED_STRATEGY_MALLOC_DIRECT  /**< Direct malloc (fallback) */
} unified_mem_strategy_t;

/**
 * @brief Current state of a unified memory handle
 */
typedef enum {
    UNIFIED_STATE_SHARED,           /**< Using shared/template data (read-only) */
    UNIFIED_STATE_PRIVATE,          /**< Has private copy (writable) */
    UNIFIED_STATE_DIRECT,           /**< Direct allocation (no CoW) */
    UNIFIED_STATE_INVALID           /**< Invalid/freed handle */
} unified_mem_state_t;

/**
 * @brief Unified memory manager handle (opaque)
 */
typedef struct unified_mem_manager_struct* unified_mem_manager_t;

/**
 * @brief Unified memory handle (opaque)
 *
 * Represents an allocation from the unified memory system.
 * May use object-level CoW, page-level CoW, or direct allocation internally.
 */
typedef struct unified_mem_handle_struct* unified_mem_handle_t;

/**
 * @brief Unified memory snapshot handle (opaque)
 *
 * Represents a snapshot for fast checkpoint/rollback.
 */
typedef struct unified_mem_snapshot_struct* unified_mem_snapshot_t;

/**
 * @brief Unified memory manager configuration
 */
typedef struct {
    // Size thresholds
    size_t page_threshold;          /**< Size threshold for page-level CoW */

    // Pool configuration
    size_t object_pool_block_size;  /**< Block size for object pool (0 = auto) */
    size_t object_pool_num_blocks;  /**< Number of blocks in object pool */
    size_t page_pool_num_pages;     /**< Number of pages in page pool (0 = use mmap) */

    // Strategy settings
    unified_mem_strategy_t default_strategy; /**< Default strategy for new allocations */
    bool enable_cow;                /**< Enable CoW by default */

    // Tracking
    bool enable_tracking;           /**< Enable statistics tracking */

    // Callbacks
    void* user_data;                /**< User context for callbacks */
} unified_mem_config_t;

/**
 * @brief Unified memory allocation request
 */
typedef struct {
    size_t size;                    /**< Allocation size in bytes */
    const void* initial_data;       /**< Initial data to copy (NULL = zero) */
    unified_mem_strategy_t strategy; /**< Strategy (AUTO for auto-select) */
    bool enable_cow;                /**< Enable CoW for this allocation */
    size_t alignment;               /**< Required alignment (0 = default) */
} unified_mem_request_t;

/**
 * @brief Unified memory statistics
 */
typedef struct {
    // Overall statistics
    size_t total_allocations;       /**< Total allocations made */
    size_t active_handles;          /**< Currently active handles */
    size_t peak_handles;            /**< Peak active handles */

    // Strategy breakdown
    size_t object_cow_allocations;  /**< Allocations using object-level CoW */
    size_t page_cow_allocations;    /**< Allocations using page-level CoW */
    size_t pool_direct_allocations; /**< Direct pool allocations */
    size_t malloc_direct_allocations; /**< Direct malloc allocations */

    // CoW statistics
    size_t cow_triggers;            /**< Total CoW copy operations */
    size_t shared_handles;          /**< Handles using shared data */
    size_t private_handles;         /**< Handles with private copies */

    // Memory statistics
    size_t total_memory_bytes;      /**< Total memory under management */
    size_t shared_memory_bytes;     /**< Memory in shared state */
    size_t private_memory_bytes;    /**< Memory in private copies */
    size_t memory_saved_bytes;      /**< Memory saved by CoW sharing */

    // Pool statistics
    size_t object_pool_utilization; /**< Object pool utilization (percentage * 100) */
    size_t page_pool_utilization;   /**< Page pool utilization (percentage * 100) */

    // Performance statistics
    uint64_t total_alloc_time_ns;   /**< Total time in allocations */
    uint64_t total_cow_time_ns;     /**< Total time in CoW operations */
} unified_mem_stats_t;

//=============================================================================
// Unified Memory Manager API
//=============================================================================

/**
 * @brief Create unified memory manager
 *
 * WHAT: Creates manager that handles all memory allocations with CoW support
 * WHY:  Single point of memory management for brain modules
 * HOW:  Initializes object pool, page pool, and CoW subsystems
 *
 * @param config Configuration (NULL for defaults)
 * @return Manager handle or NULL on failure
 *
 * COMPLEXITY: O(n) where n = pool sizes
 * MEMORY: object_pool_size + page_pool_size + overhead
 *
 * EXAMPLE:
 * ```c
 * unified_mem_config_t config = {
 *     .page_threshold = 64 * 1024,
 *     .object_pool_num_blocks = 1000,
 *     .page_pool_num_pages = 256,
 *     .enable_cow = true,
 *     .enable_tracking = true
 * };
 * unified_mem_manager_t mgr = unified_mem_create(&config);
 * ```
 */
NIMCP_EXPORT unified_mem_manager_t unified_mem_create(
    const unified_mem_config_t* config
);

/**
 * @brief Destroy unified memory manager
 *
 * WHAT: Frees all resources associated with the manager
 * WHY:  Clean shutdown
 * HOW:  Releases pools, destroys CoW managers, frees metadata
 *
 * @param manager Manager handle
 *
 * WARNING: All handles must be released before destroying the manager
 */
NIMCP_EXPORT void unified_mem_destroy(unified_mem_manager_t manager);

/**
 * @brief Get default configuration
 *
 * @return Default configuration with sensible values
 */
NIMCP_EXPORT unified_mem_config_t unified_mem_default_config(void);

/**
 * @brief Get manager statistics
 *
 * @param manager Manager handle
 * @param stats Output statistics structure
 * @return true on success, false on failure
 */
NIMCP_EXPORT bool unified_mem_get_stats(
    unified_mem_manager_t manager,
    unified_mem_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param manager Manager handle
 */
NIMCP_EXPORT void unified_mem_reset_stats(unified_mem_manager_t manager);

//=============================================================================
// Unified Memory Handle API
//=============================================================================

/**
 * @brief Allocate memory with unified API
 *
 * WHAT: Allocates memory with automatic strategy selection
 * WHY:  Simple API that picks best strategy based on size and config
 * HOW:  Checks thresholds, selects strategy, allocates accordingly
 *
 * @param manager Manager handle
 * @param request Allocation request
 * @return Memory handle or NULL on failure
 *
 * COMPLEXITY: O(1) for pool, O(m) for initial data copy
 * THREAD SAFETY: Thread-safe
 *
 * EXAMPLE:
 * ```c
 * // Allocate 50MB weight matrix with CoW
 * unified_mem_request_t req = {
 *     .size = 50 * 1024 * 1024,
 *     .initial_data = weights,
 *     .strategy = UNIFIED_STRATEGY_AUTO,
 *     .enable_cow = true
 * };
 * unified_mem_handle_t h = unified_mem_alloc(mgr, &req);
 * ```
 */
NIMCP_EXPORT unified_mem_handle_t unified_mem_alloc(
    unified_mem_manager_t manager,
    const unified_mem_request_t* request
);

/**
 * @brief Clone an existing handle (CoW reference)
 *
 * WHAT: Creates a CoW clone of an existing handle
 * WHY:  Fast O(1) cloning for brain state duplication
 * HOW:  Increments refcount for CoW handles, copies for direct
 *
 * @param handle Handle to clone
 * @return New handle or NULL on failure
 *
 * COMPLEXITY: O(1) for CoW, O(m) for direct allocation
 *
 * EXAMPLE:
 * ```c
 * // Clone weights for parallel worker
 * unified_mem_handle_t worker_weights = unified_mem_clone(master_weights);
 * // Both share same data until one writes
 * ```
 */
NIMCP_EXPORT unified_mem_handle_t unified_mem_clone(
    unified_mem_handle_t handle
);

/**
 * @brief Free memory handle
 *
 * WHAT: Releases handle and decrements refcounts
 * WHY:  Return resources to pools/system
 * HOW:  Strategy-specific cleanup
 *
 * @param handle Handle to free
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe
 */
NIMCP_EXPORT void unified_mem_free(unified_mem_handle_t handle);

/**
 * @brief Get read-only pointer to data
 *
 * WHAT: Returns const pointer for reading without CoW trigger
 * WHY:  Fast read access to shared data
 * HOW:  Direct pointer return
 *
 * @param handle Memory handle
 * @return const pointer to data, or NULL on error
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Lock-free
 *
 * IMPORTANT: Do NOT cast away const and write!
 */
NIMCP_EXPORT const void* unified_mem_read(unified_mem_handle_t handle);

/**
 * @brief Get writable pointer to data
 *
 * WHAT: Returns writable pointer, triggers CoW if needed
 * WHY:  Enable writes while preserving shared state
 * HOW:  Strategy-specific CoW handling
 *
 * @param handle Memory handle
 * @return Writable pointer or NULL on failure
 *
 * COMPLEXITY: O(1) if private, O(m) if CoW triggered
 * THREAD SAFETY: Thread-safe (mutex on CoW)
 *
 * EXAMPLE:
 * ```c
 * float* weights = (float*)unified_mem_write(h);
 * weights[0] = 0.5f;  // May trigger CoW
 * ```
 */
NIMCP_EXPORT void* unified_mem_write(unified_mem_handle_t handle);

/**
 * @brief Force handle to become private
 *
 * WHAT: Pre-emptively triggers CoW copy
 * WHY:  Batch CoW operations for performance
 * HOW:  Allocates private copy if shared
 *
 * @param handle Memory handle
 * @return true on success, false on failure
 */
NIMCP_EXPORT bool unified_mem_make_private(unified_mem_handle_t handle);

/**
 * @brief Check if handle is using shared data
 *
 * @param handle Memory handle
 * @return true if shared (CoW), false if private or direct
 */
NIMCP_EXPORT bool unified_mem_is_shared(unified_mem_handle_t handle);

/**
 * @brief Get handle state
 *
 * @param handle Memory handle
 * @return Current state
 */
NIMCP_EXPORT unified_mem_state_t unified_mem_get_state(
    unified_mem_handle_t handle
);

/**
 * @brief Get allocation size
 *
 * @param handle Memory handle
 * @return Size in bytes
 */
NIMCP_EXPORT size_t unified_mem_get_size(unified_mem_handle_t handle);

/**
 * @brief Get the strategy used for this handle
 *
 * @param handle Memory handle
 * @return Strategy in use
 */
NIMCP_EXPORT unified_mem_strategy_t unified_mem_get_strategy(
    unified_mem_handle_t handle
);

/**
 * @brief Get memory savings for this handle
 *
 * @param handle Memory handle
 * @return Bytes saved by sharing (0 if private or direct)
 */
NIMCP_EXPORT size_t unified_mem_get_memory_saved(
    unified_mem_handle_t handle
);

//=============================================================================
// Snapshot API (for checkpointing)
//=============================================================================

/**
 * @brief Create snapshot of a handle
 *
 * WHAT: Creates instant snapshot with CoW semantics
 * WHY:  Fast checkpointing for rollback/recovery
 * HOW:  Strategy-specific snapshot creation
 *
 * @param handle Handle to snapshot
 * @return Snapshot handle or NULL on failure
 *
 * COMPLEXITY: O(1) for page-level, O(1) for object-level (refcount)
 *
 * EXAMPLE:
 * ```c
 * unified_mem_snapshot_t snap = unified_mem_snapshot_create(weights);
 * // Perform risky operation
 * if (failed) {
 *     unified_mem_snapshot_restore(weights, snap);
 * }
 * unified_mem_snapshot_destroy(snap);
 * ```
 */
NIMCP_EXPORT unified_mem_snapshot_t unified_mem_snapshot_create(
    unified_mem_handle_t handle
);

/**
 * @brief Restore handle from snapshot
 *
 * WHAT: Restores handle to snapshot state
 * WHY:  Fast rollback after failure
 * HOW:  Discards changes, re-shares snapshot data
 *
 * @param handle Handle to restore
 * @param snapshot Snapshot to restore from
 * @return true on success, false on failure
 *
 * WARNING: Discards all changes made since snapshot
 */
NIMCP_EXPORT bool unified_mem_snapshot_restore(
    unified_mem_handle_t handle,
    unified_mem_snapshot_t snapshot
);

/**
 * @brief Destroy snapshot
 *
 * @param snapshot Snapshot handle
 */
NIMCP_EXPORT void unified_mem_snapshot_destroy(
    unified_mem_snapshot_t snapshot
);

/**
 * @brief Get bytes changed since snapshot
 *
 * @param handle Current handle
 * @param snapshot Snapshot to compare
 * @return Bytes that have diverged
 */
NIMCP_EXPORT size_t unified_mem_snapshot_get_delta_bytes(
    unified_mem_handle_t handle,
    unified_mem_snapshot_t snapshot
);

//=============================================================================
// Page Pool Integration API
//=============================================================================

/**
 * @brief Enable page pool for page-level CoW
 *
 * WHAT: Creates a memory pool for page allocations
 * WHY:  Faster page allocation than mmap for frequent CoW operations
 * HOW:  Pre-allocates pages, uses pool instead of mmap
 *
 * @param manager Manager handle
 * @param num_pages Number of pages to pre-allocate
 * @return true on success, false on failure
 *
 * NOTE: This is optional. Page CoW works without it using mmap.
 */
NIMCP_EXPORT bool unified_mem_enable_page_pool(
    unified_mem_manager_t manager,
    size_t num_pages
);

/**
 * @brief Disable page pool (revert to mmap)
 *
 * @param manager Manager handle
 *
 * WARNING: Only call when no page-level handles are active
 */
NIMCP_EXPORT void unified_mem_disable_page_pool(
    unified_mem_manager_t manager
);

/**
 * @brief Get page pool statistics
 *
 * @param manager Manager handle
 * @param total_pages Output: total pages in pool
 * @param free_pages Output: free pages in pool
 * @return true on success, false if no page pool
 */
NIMCP_EXPORT bool unified_mem_get_page_pool_stats(
    unified_mem_manager_t manager,
    size_t* total_pages,
    size_t* free_pages
);

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Create allocation request with defaults
 *
 * @param size Allocation size
 * @param initial_data Initial data (NULL = zero)
 * @param enable_cow Enable Copy-on-Write
 * @return Request structure
 */
static inline unified_mem_request_t unified_mem_request(
    size_t size,
    const void* initial_data,
    bool enable_cow
) {
    unified_mem_request_t req = {
        .size = size,
        .initial_data = initial_data,
        .strategy = UNIFIED_STRATEGY_AUTO,
        .enable_cow = enable_cow,
        .alignment = 0
    };
    return req;
}

/**
 * @brief Create request for direct pool allocation (no CoW)
 *
 * @param size Allocation size
 * @return Request structure
 */
static inline unified_mem_request_t unified_mem_request_direct(size_t size) {
    unified_mem_request_t req = {
        .size = size,
        .initial_data = NULL,
        .strategy = UNIFIED_STRATEGY_POOL_DIRECT,
        .enable_cow = false,
        .alignment = 0
    };
    return req;
}

/**
 * @brief Create request forcing page-level CoW
 *
 * @param size Allocation size
 * @param initial_data Initial data
 * @return Request structure
 */
static inline unified_mem_request_t unified_mem_request_page_cow(
    size_t size,
    const void* initial_data
) {
    unified_mem_request_t req = {
        .size = size,
        .initial_data = initial_data,
        .strategy = UNIFIED_STRATEGY_PAGE_COW,
        .enable_cow = true,
        .alignment = 0
    };
    return req;
}

/**
 * @brief Get strategy name as string
 *
 * @param strategy Strategy enum
 * @return Human-readable string
 */
static inline const char* unified_mem_strategy_name(
    unified_mem_strategy_t strategy
) {
    switch (strategy) {
        case UNIFIED_STRATEGY_AUTO: return "auto";
        case UNIFIED_STRATEGY_OBJECT_COW: return "object_cow";
        case UNIFIED_STRATEGY_PAGE_COW: return "page_cow";
        case UNIFIED_STRATEGY_POOL_DIRECT: return "pool_direct";
        case UNIFIED_STRATEGY_MALLOC_DIRECT: return "malloc_direct";
        default: return "unknown";
    }
}

/**
 * @brief Get state name as string
 *
 * @param state State enum
 * @return Human-readable string
 */
static inline const char* unified_mem_state_name(unified_mem_state_t state) {
    switch (state) {
        case UNIFIED_STATE_SHARED: return "shared";
        case UNIFIED_STATE_PRIVATE: return "private";
        case UNIFIED_STATE_DIRECT: return "direct";
        case UNIFIED_STATE_INVALID: return "invalid";
        default: return "unknown";
    }
}

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_UNIFIED_MEMORY_H
