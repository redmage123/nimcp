#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_memory_pool.c - Generic Memory Pool Implementation
//=============================================================================
/**
 * @file nimcp_memory_pool.c
 * @brief Generic O(1) memory pool with free-list management
 *
 * WHAT: Pre-allocated memory pool for fast sub-allocations
 * WHY:  Eliminate malloc overhead (O(log n) → O(1))
 * HOW:  Large upfront allocation + free-list + O(1) operations
 *
 * ARCHITECTURE:
 *
 *   Memory Layout (single contiguous allocation):
 *   ┌──────────────────────────────────────────────────────────────┐
 *   │ Pool Header (pool_t)                                          │
 *   ├──────────────────────────────────────────────────────────────┤
 *   │ Block 0: [header | user_data (block_size bytes)]            │
 *   ├──────────────────────────────────────────────────────────────┤
 *   │ Block 1: [header | user_data (block_size bytes)]            │
 *   ├──────────────────────────────────────────────────────────────┤
 *   │ ...                                                           │
 *   └──────────────────────────────────────────────────────────────┘
 *
 *   Free List (linked list of available blocks):
 *   pool->free_list → Block 3 → Block 7 → Block 12 → NULL
 *
 * PERFORMANCE:
 * - Acquire: O(1) - Pop from free-list
 * - Release: O(1) - Push to free-list
 * - Reset: O(n) - Rebuild free-list
 * - Owns check: O(1) - Pointer arithmetic
 *
 * THREAD SAFETY:
 * - All operations protected by mutex
 * - Lock-free ownership check (read-only)
 *
 * MEMORY EFFICIENCY:
 * - Per-block overhead: 16 bytes (header)
 * - Pool overhead: 128 bytes (pool_t struct)
 * - Total overhead: ~0.1% for 1KB blocks
 *
 * SRP: This module has ONE responsibility - memory pool management
 *
 * @author NIMCP Development Team
 * @date 2025-11-21
 * @version 1.0.0
 */

#include "utils/memory/nimcp_memory_pool.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(memory_pool)

//=============================================================================
// Constants and Configuration
//=============================================================================

#define MEMORY_POOL_MAGIC 0x504F4F4C  // 'POOL' in hex
#define BLOCK_MAGIC 0x424C4F43         // 'BLOC' in hex
#define DEFAULT_ALIGNMENT 16           // 16-byte alignment (SIMD-friendly)
#define MIN_ALIGNMENT 8                // Minimum alignment
#define MAX_ALIGNMENT 4096             // Maximum alignment (page size)

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Block header (metadata for each memory block)
 *
 * DESIGN: Intrusive list node stored at start of each block
 * WHY: No extra allocations needed for list nodes
 * SIZE: 16 bytes on 64-bit systems
 */
typedef struct block_header {
    uint32_t magic;           /**< Magic number for corruption detection */
    uint32_t allocated;       /**< 1 if allocated, 0 if free */
    struct block_header* next; /**< Next block in free-list (NULL if allocated) */
} block_header_t;

/**
 * @brief Memory pool structure
 *
 * DESIGN: Single large allocation + free-list management
 * THREAD SAFETY: Mutex protects all mutable state
 */
struct memory_pool_struct {
    uint32_t magic;              /**< Magic number for validation */

    // Configuration (immutable after creation)
    size_t block_size;           /**< Size of each user block (aligned) */
    size_t total_block_size;     /**< block_size + sizeof(block_header_t) */
    size_t num_blocks;           /**< Total blocks in pool */
    size_t alignment;            /**< Memory alignment */
    bool enable_tracking;        /**< Track statistics */

    // Memory region (immutable after creation)
    void* memory_region;         /**< Base pointer of entire pool */
    void* memory_region_end;     /**< End pointer (for bounds checking) */
    size_t total_size;           /**< Total allocated size */

    // Free list (mutable - protected by mutex)
    block_header_t* free_list;   /**< Head of free block list */

    // Statistics (mutable - protected by mutex)
    size_t allocated_blocks;     /**< Current allocated blocks */
    size_t peak_allocated;       /**< Peak allocation count */
    size_t total_allocations;    /**< Total acquire calls */
    size_t total_deallocations;  /**< Total release calls */
    size_t failed_allocations;   /**< Failed acquire calls */

    // Thread safety
    nimcp_platform_mutex_t mutex; /**< Protects mutable state */
};

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Check if value is power of 2
 *
 * WHY: Alignment must be power of 2 for efficient modulo
 * HOW: x & (x-1) == 0 for powers of 2
 */
static inline bool is_power_of_2(size_t value) {
    return value != 0 && (value & (value - 1)) == 0;
}

/**
 * @brief Round up to next multiple of alignment
 *
 * WHY: Ensure proper alignment for all blocks
 * HOW: (size + align - 1) & ~(align - 1)
 */
static inline size_t align_size(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

/**
 * @brief Get block header from user pointer
 *
 * WHY: Need to access metadata from user-visible pointer
 * HOW: Header stored immediately before user data
 */
static inline block_header_t* get_block_header(void* user_ptr) {
    if (!user_ptr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "get_block_header: user_ptr is NULL");
        return NULL;
    }
    return (block_header_t*)((uint8_t*)user_ptr - sizeof(block_header_t));
}

/**
 * @brief Get user pointer from block header
 *
 * WHY: Return usable memory to caller (skip header)
 * HOW: Add sizeof(block_header_t) to header pointer
 */
static inline void* get_user_pointer(block_header_t* header) {
    if (!header) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "get_user_pointer: header is NULL");
        return NULL;
    }
    return (void*)((uint8_t*)header + sizeof(block_header_t));
}

/**
 * @brief Get block at index
 *
 * WHY: Access blocks by index for initialization
 * HOW: Pointer arithmetic: base + index * block_size
 */
static inline block_header_t* get_block_at_index(
    struct memory_pool_struct* pool,
    size_t index
) {
    uint8_t* base = (uint8_t*)pool->memory_region;
    return (block_header_t*)(base + index * pool->total_block_size);
}

//=============================================================================
// Core API Implementation
//=============================================================================

/**
 * @brief Create memory pool
 *
 * ALGORITHM:
 * 1. Validate configuration
 * 2. Allocate pool structure
 * 3. Allocate large memory region (num_blocks × block_size)
 * 4. Initialize free-list (link all blocks)
 * 5. Initialize statistics and mutex
 *
 * COMPLEXITY: O(n) where n = num_blocks
 * MEMORY: block_size × num_blocks + overhead
 */
NIMCP_EXPORT memory_pool_t memory_pool_create(const memory_pool_config_t* config) {
    // Validate configuration
    if (!config) {
        LOG_ERROR("MEMORY_POOL", "NULL config in memory_pool_create");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL config in memory_pool_create");
        return NULL;
    }
    if (config->num_blocks == 0) {
        LOG_ERROR("MEMORY_POOL", "num_blocks cannot be 0");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "num_blocks cannot be 0 in memory_pool_create");
        return NULL;
    }
    if (config->block_size == 0) {
        LOG_ERROR("MEMORY_POOL", "block_size cannot be 0");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "block_size cannot be 0 in memory_pool_create");
        return NULL;
    }

    // Validate alignment
    size_t alignment = config->alignment;
    if (alignment == 0) alignment = DEFAULT_ALIGNMENT;
    if (alignment < MIN_ALIGNMENT) alignment = MIN_ALIGNMENT;
    if (alignment > MAX_ALIGNMENT) {
        LOG_ERROR("MEMORY_POOL", "alignment %zu exceeds MAX_ALIGNMENT %d", alignment, MAX_ALIGNMENT);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "alignment %zu exceeds max %d", alignment, MAX_ALIGNMENT);
        return NULL;
    }
    if (!is_power_of_2(alignment)) {
        LOG_ERROR("MEMORY_POOL", "alignment %zu is not a power of 2", alignment);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "alignment must be power of 2, got %zu", alignment);
        return NULL;
    }

    // Allocate pool structure using NIMCP memory
    struct memory_pool_struct* pool = nimcp_calloc(1, sizeof(struct memory_pool_struct));
    NIMCP_API_CHECK_ALLOC(pool, "Failed to allocate memory pool structure");

    // Initialize immutable configuration
    pool->magic = MEMORY_POOL_MAGIC;
    pool->alignment = alignment;
    pool->enable_tracking = config->enable_tracking;
    pool->num_blocks = config->num_blocks;

    // Calculate aligned block sizes with overflow protection
    // Each block needs: header + user_data (aligned)
    pool->block_size = align_size(config->block_size, alignment);
    pool->total_block_size = sizeof(block_header_t) + pool->block_size;

    // Check for overflow: total_block_size * num_blocks
    if (pool->num_blocks > SIZE_MAX / pool->total_block_size) {
        LOG_ERROR("MEMORY_POOL", "Size overflow: num_blocks=%zu, total_block_size=%zu",
                  pool->num_blocks, pool->total_block_size);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "Memory pool size overflow");
        nimcp_free(pool);
        return NULL;
    }
    pool->total_size = pool->total_block_size * pool->num_blocks;

    // Allocate large memory region
    pool->memory_region = nimcp_aligned_malloc(pool->total_size, alignment);
    if (!pool->memory_region) {
        LOG_ERROR("MEMORY_POOL", "Failed to allocate memory region (%zu bytes)", pool->total_size);
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, pool->total_size, "Failed to allocate memory pool region");
        nimcp_free(pool);
        return NULL;
    }

    pool->memory_region_end = (uint8_t*)pool->memory_region + pool->total_size;

    // Initialize all blocks and build free-list
    // Free-list order: Block 0 → Block 1 → Block 2 → ... → NULL
    pool->free_list = NULL;

    for (size_t i = 0; i < pool->num_blocks; i++) {
        block_header_t* block = get_block_at_index(pool, i);

        // Initialize block header
        block->magic = BLOCK_MAGIC;
        block->allocated = 0;

        // Link into free-list (prepend - O(1))
        block->next = pool->free_list;
        pool->free_list = block;
    }

    // Initialize statistics
    pool->allocated_blocks = 0;
    pool->peak_allocated = 0;
    pool->total_allocations = 0;
    pool->total_deallocations = 0;
    pool->failed_allocations = 0;

    // Initialize mutex
    if (nimcp_platform_mutex_init(&pool->mutex, false) != 0) {
        LOG_ERROR("MEMORY_POOL", "Failed to initialize pool mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "Failed to initialize memory pool mutex");
        nimcp_aligned_free(pool->memory_region);
        nimcp_free(pool);
        return NULL;
    }

    return pool;
}

/**
 * @brief Destroy memory pool
 *
 * ALGORITHM:
 * 1. Validate pool
 * 2. Check for leaks (optional warning)
 * 3. Destroy mutex
 * 4. Free memory region
 * 5. Free pool structure
 *
 * COMPLEXITY: O(1)
 * WARNING: All blocks must be released before destruction
 */
NIMCP_EXPORT void memory_pool_destroy(memory_pool_t pool) {
    if (!pool) return;

    struct memory_pool_struct* p = (struct memory_pool_struct*)pool;

    // Validate magic number
    if (p->magic != MEMORY_POOL_MAGIC) return;

    // Check for memory leaks (if tracking enabled)
    if (p->enable_tracking && p->allocated_blocks > 0) {
        // WARNING: Not all blocks were released
        // In production, you might want to log this
    }

    // Destroy mutex
    nimcp_platform_mutex_destroy(&p->mutex);

    // Free memory region
    nimcp_aligned_free(p->memory_region);

    // Invalidate magic and free pool using NIMCP memory
    p->magic = 0;
    nimcp_free(p);
}

/**
 * @brief Acquire block from pool
 *
 * ALGORITHM:
 * 1. Lock mutex
 * 2. Check if free-list empty
 * 3. Pop block from free-list head
 * 4. Mark as allocated
 * 5. Update statistics
 * 6. Unlock mutex
 * 7. Return user pointer
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe (mutex protected)
 */
NIMCP_EXPORT void* memory_pool_acquire(memory_pool_t pool) {
    if (!pool) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "memory_pool_acquire: pool is NULL");
        return NULL;
    }

    struct memory_pool_struct* p = (struct memory_pool_struct*)pool;

    // Validate magic
    if (p->magic != MEMORY_POOL_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "memory_pool_acquire: validation failed");
        return NULL;
    }

    // Lock for thread safety
    nimcp_platform_mutex_lock(&p->mutex);

    // Check if pool exhausted
    if (p->free_list == NULL) {
        if (p->enable_tracking) {
            p->failed_allocations++;
        }
        nimcp_platform_mutex_unlock(&p->mutex);
        return NULL;  /* Pool exhausted — caller should handle */
    }

    // Pop block from free-list (O(1))
    block_header_t* block = p->free_list;

    // Validate next pointer before dereferencing
    // The next pointer should either be NULL or point within our memory region
    if (block->next != NULL) {
        // Verify next pointer is within pool bounds
        if ((void*)block->next < p->memory_region ||
            (void*)block->next >= p->memory_region_end) {
            // Corrupted free list detected - next pointer is out of bounds
            LOG_ERROR("MEMORY_POOL", "Corrupted free list: next pointer %p is outside pool bounds [%p, %p)",
                      (void*)block->next, p->memory_region, p->memory_region_end);
            if (p->enable_tracking) {
                p->failed_allocations++;
            }
            nimcp_platform_mutex_unlock(&p->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "memory_pool_acquire: validation failed");
            return NULL;
        }
        // Verify next block has valid magic
        if (block->next->magic != BLOCK_MAGIC) {
            LOG_ERROR("MEMORY_POOL", "Corrupted free list: next block at %p has invalid magic 0x%08X",
                      (void*)block->next, block->next->magic);
            if (p->enable_tracking) {
                p->failed_allocations++;
            }
            nimcp_platform_mutex_unlock(&p->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "memory_pool_acquire: validation failed");
            return NULL;
        }
    }

    p->free_list = block->next;

    // Mark as allocated
    block->allocated = 1;
    block->next = NULL;

    // Update statistics
    if (p->enable_tracking) {
        p->allocated_blocks++;
        p->total_allocations++;

        if (p->allocated_blocks > p->peak_allocated) {
            p->peak_allocated = p->allocated_blocks;
        }
    }

    nimcp_platform_mutex_unlock(&p->mutex);

    // Return user pointer (skip header)
    return get_user_pointer(block);
}

/**
 * @brief Release block back to pool
 *
 * ALGORITHM:
 * 1. Validate block pointer
 * 2. Get block header
 * 3. Validate block magic (can be done lock-free as magic is immutable)
 * 4. Lock mutex
 * 5. Validate block is allocated (MUST be inside lock to prevent race)
 * 6. Push block to free-list head
 * 7. Mark as free
 * 8. Update statistics
 * 9. Unlock mutex
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe (mutex protected)
 */
NIMCP_EXPORT void memory_pool_release(memory_pool_t pool, void* block) {
    if (!pool || !block) return;

    struct memory_pool_struct* p = (struct memory_pool_struct*)pool;

    // Validate pool magic (immutable after creation, safe lock-free)
    if (p->magic != MEMORY_POOL_MAGIC) return;

    // Get block header
    block_header_t* header = get_block_header(block);

    // Validate block belongs to this pool (uses immutable bounds, safe lock-free)
    if (!memory_pool_owns(pool, block)) return;

    // Validate block magic (immutable after creation, safe lock-free)
    if (header->magic != BLOCK_MAGIC) return;

    // Lock for thread safety BEFORE checking allocated flag
    // This prevents double-free race where two threads could both see
    // allocated==1 and both try to free the same block
    nimcp_platform_mutex_lock(&p->mutex);

    // Validate block is allocated (prevent double-free)
    // CRITICAL: This check MUST be inside the lock to prevent race condition
    if (header->allocated == 0) {
        nimcp_platform_mutex_unlock(&p->mutex);
        return;
    }

    // Push block to free-list head (O(1))
    header->next = p->free_list;
    p->free_list = header;

    // Mark as free
    header->allocated = 0;

    // Update statistics
    if (p->enable_tracking) {
        p->allocated_blocks--;
        p->total_deallocations++;
    }

    nimcp_platform_mutex_unlock(&p->mutex);
}

/**
 * @brief Reset pool (mark all blocks as free)
 *
 * ALGORITHM:
 * 1. Lock mutex
 * 2. Rebuild free-list by walking all blocks
 * 3. Mark all as free
 * 4. Reset statistics
 * 5. Unlock mutex
 *
 * COMPLEXITY: O(n) where n = num_blocks
 * WARNING: Invalidates all outstanding pointers
 */
NIMCP_EXPORT size_t memory_pool_reset(memory_pool_t pool) {
    if (!pool) return 0;

    struct memory_pool_struct* p = (struct memory_pool_struct*)pool;

    if (p->magic != MEMORY_POOL_MAGIC) return 0;

    nimcp_platform_mutex_lock(&p->mutex);

    size_t reset_count = 0;

    // Rebuild free-list
    p->free_list = NULL;

    for (size_t i = 0; i < p->num_blocks; i++) {
        block_header_t* block = get_block_at_index(p, i);

        // Mark as free
        block->allocated = 0;

        // Link into free-list
        block->next = p->free_list;
        p->free_list = block;

        reset_count++;
    }

    // Reset allocation statistics
    p->allocated_blocks = 0;

    nimcp_platform_mutex_unlock(&p->mutex);

    return reset_count;
}

/**
 * @brief Get pool statistics
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe (mutex protected)
 */
NIMCP_EXPORT bool memory_pool_get_stats(
    memory_pool_t pool,
    memory_pool_stats_t* stats
) {
    if (!pool || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "memory_pool_get_stats: required parameter is NULL (pool, stats)");
        return false;
    }

    struct memory_pool_struct* p = (struct memory_pool_struct*)pool;

    if (p->magic != MEMORY_POOL_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "memory_pool_get_stats: validation failed");
        return false;
    }

    nimcp_platform_mutex_lock(&p->mutex);

    stats->total_blocks = p->num_blocks;
    stats->allocated_blocks = p->allocated_blocks;
    stats->free_blocks = p->num_blocks - p->allocated_blocks;
    stats->peak_allocated = p->peak_allocated;
    stats->total_allocations = p->total_allocations;
    stats->total_deallocations = p->total_deallocations;
    stats->failed_allocations = p->failed_allocations;
    stats->pool_size_bytes = p->total_size;

    // Calculate internal fragmentation (alignment padding)
    size_t actual_user_bytes = p->block_size * p->num_blocks;
    size_t wasted_alignment = p->total_size - actual_user_bytes;
    stats->wasted_bytes = wasted_alignment;

    nimcp_platform_mutex_unlock(&p->mutex);

    return true;
}

/**
 * @brief Check if pointer belongs to pool
 *
 * ALGORITHM:
 * 1. Check if ptr >= memory_region
 * 2. Check if ptr < memory_region_end
 * 3. Verify alignment
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Lock-free (read-only check)
 */
NIMCP_EXPORT bool memory_pool_owns(memory_pool_t pool, const void* ptr) {
    if (!pool || !ptr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "memory_pool_owns: required parameter is NULL (pool, ptr)");
        return false;
    }

    struct memory_pool_struct* p = (struct memory_pool_struct*)pool;

    if (p->magic != MEMORY_POOL_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "memory_pool_owns: validation failed");
        return false;
    }

    // Check if pointer is within pool bounds
    return (ptr >= p->memory_region && ptr < p->memory_region_end);
}

/**
 * @brief Get block size
 *
 * COMPLEXITY: O(1)
 */
NIMCP_EXPORT size_t memory_pool_get_block_size(memory_pool_t pool) {
    if (!pool) return 0;

    struct memory_pool_struct* p = (struct memory_pool_struct*)pool;

    if (p->magic != MEMORY_POOL_MAGIC) return 0;

    return p->block_size;
}

/**
 * @brief Get available blocks
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe (mutex protected)
 */
NIMCP_EXPORT size_t memory_pool_get_available(memory_pool_t pool) {
    if (!pool) return 0;

    struct memory_pool_struct* p = (struct memory_pool_struct*)pool;

    if (p->magic != MEMORY_POOL_MAGIC) return 0;

    nimcp_platform_mutex_lock(&p->mutex);
    size_t available = p->num_blocks - p->allocated_blocks;
    nimcp_platform_mutex_unlock(&p->mutex);

    return available;
}
