//=============================================================================
// nimcp_memory_pool.h - Generic Memory Pool Utility
//=============================================================================
/**
 * @file nimcp_memory_pool.h
 * @brief Generic memory pool with O(1) allocation for all NIMCP components
 *
 * WHAT: Pre-allocated memory pool with fast O(1) allocation/deallocation
 * WHY:  Reduce malloc overhead from O(n×m) allocations to O(1) pool operations
 * HOW:  Large upfront allocation + free-list management + optional CoW
 *
 * ARCHITECTURE:
 *
 *   Memory Pool Layout:
 *   ┌─────────────────────────────────────────────────────────┐
 *   │  Pool Header (metadata)                                 │
 *   ├─────────────────────────────────────────────────────────┤
 *   │  Block 0: [allocated=true,  next=NULL,   data...]      │
 *   ├─────────────────────────────────────────────────────────┤
 *   │  Block 1: [allocated=false, next=Block3, data...]      │ ◄── Free list
 *   ├─────────────────────────────────────────────────────────┤
 *   │  Block 2: [allocated=true,  next=NULL,   data...]      │
 *   ├─────────────────────────────────────────────────────────┤
 *   │  Block 3: [allocated=false, next=Block5, data...]      │
 *   ├─────────────────────────────────────────────────────────┤
 *   │  ...                                                     │
 *   └─────────────────────────────────────────────────────────┘
 *
 * PERFORMANCE:
 * - nimcp_malloc(): O(log n) system call overhead
 * - pool_acquire(): O(1) free-list pop
 * - pool_release(): O(1) free-list push
 *
 * MEMORY SAVINGS WITH COW:
 * - Without CoW: 1000 channels × 2,220 allocations = 2.2M allocations
 * - With CoW: 1 template + 100 active × 2,220 = 222K allocations (10x less)
 *
 * THREAD SAFETY: Protected by mutex for concurrent access
 *
 * @author NIMCP Development Team
 * @date 2025-11-21
 * @version 1.0.0
 */

#ifndef NIMCP_MEMORY_POOL_H
#define NIMCP_MEMORY_POOL_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "utils/memory/nimcp_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

// Export macro (for shared library builds)
#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

//=============================================================================
// Configuration and Types
//=============================================================================

/**
 * @brief Memory pool configuration
 */
typedef struct {
    size_t block_size;          /**< Size of each memory block */
    size_t num_blocks;          /**< Number of blocks in pool */
    size_t alignment;           /**< Memory alignment (power of 2) */
    bool enable_tracking;       /**< Track allocation statistics */
    bool enable_guard_pages;    /**< Enable memory corruption detection */
} memory_pool_config_t;

/**
 * @brief Memory pool handle (opaque)
 */
typedef struct memory_pool_struct* memory_pool_t;

/**
 * @brief Memory pool statistics
 */
typedef struct {
    size_t total_blocks;        /**< Total blocks in pool */
    size_t allocated_blocks;    /**< Currently allocated blocks */
    size_t free_blocks;         /**< Currently free blocks */
    size_t peak_allocated;      /**< Peak allocation count */
    size_t total_allocations;   /**< Total allocations made */
    size_t total_deallocations; /**< Total deallocations made */
    size_t failed_allocations;  /**< Failed allocation attempts */
    size_t pool_size_bytes;     /**< Total pool size in bytes */
    size_t wasted_bytes;        /**< Internal fragmentation */
} memory_pool_stats_t;

//=============================================================================
// Core Memory Pool API
//=============================================================================

/**
 * @brief Create memory pool
 *
 * WHAT: Allocates large memory pool upfront for fast sub-allocations
 * WHY:  Eliminate per-object malloc overhead
 * HOW:  Single large malloc + free-list initialization
 *
 * @param config Pool configuration (NULL for defaults)
 * @return Pool handle or NULL on failure
 *
 * COMPLEXITY: O(n) where n = num_blocks (initialization)
 * MEMORY: block_size × num_blocks + overhead
 *
 * EXAMPLE:
 * ```c
 * memory_pool_config_t config = {
 *     .block_size = 1024,      // 1KB blocks
 *     .num_blocks = 10000,     // 10K blocks
 *     .alignment = 16,         // 16-byte aligned
 *     .enable_tracking = true
 * };
 * memory_pool_t pool = memory_pool_create(&config);
 * ```
 */
NIMCP_EXPORT memory_pool_t memory_pool_create(const memory_pool_config_t* config);

/**
 * @brief Destroy memory pool
 *
 * WHAT: Frees all pool memory
 * WHY:  Clean shutdown
 * HOW:  Validates all blocks freed, then frees pool
 *
 * @param pool Pool handle
 *
 * WARNING: All acquired blocks must be released before destruction
 */
NIMCP_EXPORT void memory_pool_destroy(memory_pool_t pool);

/**
 * @brief Acquire block from pool
 *
 * WHAT: Fast O(1) allocation from pre-allocated pool
 * WHY:  Avoid malloc overhead
 * HOW:  Pop from free-list, mark as allocated
 *
 * @param pool Pool handle
 * @return Pointer to block or NULL if pool exhausted
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe (mutex protected)
 *
 * EXAMPLE:
 * ```c
 * void* block = memory_pool_acquire(pool);
 * if (block) {
 *     // Use block...
 *     memory_pool_release(pool, block);
 * }
 * ```
 */
NIMCP_EXPORT void* memory_pool_acquire(memory_pool_t pool);

/**
 * @brief Release block back to pool
 *
 * WHAT: Fast O(1) deallocation, returns block to free-list
 * WHY:  Enable memory reuse
 * HOW:  Push to free-list, mark as free
 *
 * @param pool Pool handle
 * @param block Block pointer (from memory_pool_acquire)
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe (mutex protected)
 */
NIMCP_EXPORT void memory_pool_release(memory_pool_t pool, void* block);

/**
 * @brief Reset pool (free all allocations)
 *
 * WHAT: Marks all blocks as free without deallocation
 * WHY:  Fast bulk deallocation for reuse
 * HOW:  Rebuild free-list in O(n)
 *
 * @param pool Pool handle
 * @return Number of blocks reset
 *
 * WARNING: Invalidates all outstanding block pointers
 */
NIMCP_EXPORT size_t memory_pool_reset(memory_pool_t pool);

/**
 * @brief Get pool statistics
 *
 * @param pool Pool handle
 * @param stats Output statistics structure
 * @return true on success, false on failure
 */
NIMCP_EXPORT bool memory_pool_get_stats(
    memory_pool_t pool,
    memory_pool_stats_t* stats
);

/**
 * @brief Check if pointer belongs to pool
 *
 * @param pool Pool handle
 * @param ptr Pointer to check
 * @return true if ptr is from this pool
 */
NIMCP_EXPORT bool memory_pool_owns(memory_pool_t pool, const void* ptr);

/**
 * @brief Get block size
 *
 * @param pool Pool handle
 * @return Block size in bytes
 */
NIMCP_EXPORT size_t memory_pool_get_block_size(memory_pool_t pool);

/**
 * @brief Get available blocks
 *
 * @param pool Pool handle
 * @return Number of free blocks
 */
NIMCP_EXPORT size_t memory_pool_get_available(memory_pool_t pool);

//=============================================================================
// Default Configuration
//=============================================================================

/**
 * @brief Get default pool configuration
 *
 * @param block_size Desired block size
 * @param num_blocks Desired number of blocks
 * @return Default configuration
 */
static inline memory_pool_config_t memory_pool_default_config(
    size_t block_size,
    size_t num_blocks
) {
    memory_pool_config_t config = {
        .block_size = block_size,
        .num_blocks = num_blocks,
        .alignment = 16,              // 16-byte alignment
        .enable_tracking = true,      // Enable statistics
        .enable_guard_pages = false   // Disable for performance
    };
    return config;
}

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_MEMORY_POOL_H
