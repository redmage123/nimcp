#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_sparse_synapse.c - Sparse Synapse Allocation Implementation
//=============================================================================

#include "core/neuralnet/nimcp_sparse_synapse.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/memory/nimcp_memory_pool.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/validation/nimcp_common.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "security/nimcp_bbb_helpers.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#ifndef __cplusplus
    #include <stdatomic.h>
#else
    #include <atomic>
    #define _Atomic(T) std::atomic<T>
#endif

//=============================================================================
// Size-Class Pool Constants
//=============================================================================

/**
 * Size-class pool design for variable-size overflow allocations:
 * - Class 0: 16 handles   (minimum overflow size)
 * - Class 1: 32 handles
 * - Class 2: 64 handles
 * - Class 3: 128 handles
 * - Class 4: 256 handles
 * - Class 5: 512 handles
 * - Class 6: 1024 handles
 * - Class 7: 2048 handles (maximum reasonable overflow)
 *
 * WHY: memory_pool requires FIXED block sizes, but overflow arrays GROW.
 * Solution: Multiple pools at power-of-2 sizes. Allocate from smallest
 * fitting class. On grow, allocate from next class and copy.
 */
#define SIZE_CLASS_COUNT 10
#define SIZE_CLASS_MIN_HANDLES 16
#define SIZE_CLASS_MAX_HANDLES 8192

// Size class capacities (handles per block)
static const uint32_t SIZE_CLASS_CAPACITIES[SIZE_CLASS_COUNT] = {
    16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192
};

// Blocks per size class (more small blocks, fewer large blocks)
static const uint32_t SIZE_CLASS_BLOCK_COUNTS[SIZE_CLASS_COUNT] = {
    1024, 512, 256, 128, 64, 32, 16, 8, 4, 2
};

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Sparse synapse pool internal structure
 *
 * WHAT: Pool implementation with SIZE-CLASS pools and statistics tracking
 * WHY:  Efficient allocation for VARIABLE-SIZE overflow arrays
 * HOW:  Multiple memory_pools at power-of-2 sizes + atomic counters + mutex
 *
 * ARCHITECTURE:
 *   Overflow needs 20 handles → allocate from class 1 (32-handle blocks)
 *   Overflow grows to 40 handles → allocate from class 2 (64-handle blocks)
 *   Each size class is a separate memory_pool with fixed block size
 */
typedef struct sparse_synapse_pool_struct {
    uint32_t magic;                     /**< Magic number for validation */

    // Configuration
    sparse_synapse_pool_config_t config; /**< Pool configuration */

    // Size-class pools for overflow allocation
    // Each pool has fixed block size = SIZE_CLASS_CAPACITIES[i] * sizeof(synapse_handle_t)
    memory_pool_t size_class_pools[SIZE_CLASS_COUNT];

    // Statistics (atomic for thread-safety)
    _Atomic uint64_t total_synapses;    /**< Total synapses across all neurons */
    _Atomic uint64_t embedded_synapses; /**< Synapses in embedded arrays */
    _Atomic uint64_t overflow_synapses; /**< Synapses in overflow arrays */
    _Atomic uint64_t total_allocations; /**< Total allocation calls */
    _Atomic uint64_t total_deallocations; /**< Total deallocation calls */
    _Atomic uint64_t failed_allocations; /**< Failed allocations */
    _Atomic uint64_t overflow_allocations; /**< Overflow allocation events */
    _Atomic size_t peak_pool_usage;     /**< Peak pool utilization */
    _Atomic uint64_t size_class_hits[SIZE_CLASS_COUNT]; /**< Allocations per class */

    // Security
    bbb_system_t bbb_system;            /**< BBB security validation system */

    // Thread safety
    nimcp_mutex_t mutex;                /**< Mutex for pool operations */
} sparse_synapse_pool_struct_t;

//=============================================================================
// Size-Class Helper Functions
//=============================================================================

/**
 * @brief Find size class index for given capacity
 * @param capacity Required capacity in handles
 * @return Size class index (0-7), or -1 if too large
 */
static inline int find_size_class(uint32_t capacity) {
    if (capacity == 0) return 0;
    if (capacity > SIZE_CLASS_CAPACITIES[SIZE_CLASS_COUNT - 1]) return -1;
    /* Power-of-2 size classes: use bit scan to find class in O(1)
     * SIZE_CLASS_CAPACITIES = {16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192}
     * which are 2^4 through 2^13 (10 classes). */
    uint32_t rounded = capacity - 1;
    rounded |= rounded >> 1;
    rounded |= rounded >> 2;
    rounded |= rounded >> 4;
    rounded |= rounded >> 8;
    rounded |= rounded >> 16;
    rounded++;
    /* Map power-of-2 to class index: 2^4=16 -> class 0, 2^5=32 -> class 1, etc. */
    int bits = 31 - __builtin_clz(rounded);
    int class_idx = bits - 4;  /* 2^4=16 is class 0 */
    if (class_idx < 0) class_idx = 0;
    if (class_idx >= SIZE_CLASS_COUNT) return -1;
    return class_idx;
}

/**
 * @brief Get capacity for size class
 * @param class_index Size class index
 * @return Capacity in handles
 */
static inline uint32_t get_size_class_capacity(int class_index) {
    if (class_index < 0 || class_index >= SIZE_CLASS_COUNT) {
        return 0;
    }
    return SIZE_CLASS_CAPACITIES[class_index];
}

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Validate pool handle with BBB security
 *
 * WHAT: Check if pool handle is valid using BBB validation
 * WHY:  Prevent crashes and security violations from NULL/corrupted pointers
 * HOW:  BBB pointer validation + magic number check
 */
static inline bool validate_pool(const sparse_synapse_pool_t pool) {
    // WHAT: Check for NULL pointer
    // WHY: NULL dereference protection
    // HOW: Early guard clause
    if (pool == NULL) {
        LOG_ERROR("Sparse synapse pool is NULL");
        return false;
    }

    // WHAT: Validate magic number
    // WHY: Detect memory corruption or invalid pool handle
    // HOW: Compare against known magic value
    if (pool->magic != SPARSE_SYNAPSE_MAGIC) {
        LOG_ERROR("Sparse synapse pool has invalid magic number: 0x%X (expected 0x%X)",
                  pool->magic, SPARSE_SYNAPSE_MAGIC);
        return false;
    }

    // WHAT: BBB pointer validation
    // WHY: Catch wild pointers and memory violations
    // HOW: Use BBB system if available
    if (pool->bbb_system != NULL) {
        bbb_validation_result_t result = {0};
        if (!bbb_validate_pointer(pool->bbb_system, pool, sizeof(sparse_synapse_pool_struct_t), &result)) {
            LOG_ERROR("BBB validation failed for pool pointer: %s", result.reason);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "validate_pool: bbb_validate_pointer is NULL");
            return false;
        }
    }

    return true;
}

/**
 * @brief Validate storage structure with BBB security
 *
 * WHAT: Validate sparse synapse storage integrity
 * WHY: Prevent buffer overflows and memory corruption
 * HOW: BBB validation + bounds checking
 */
static inline bool validate_storage(const sparse_synapse_storage_t* storage) {
    // WHAT: Check for NULL pointer
    // WHY: NULL dereference protection
    // HOW: Early guard clause
    if (storage == NULL) {
        LOG_ERROR("Sparse synapse storage is NULL");
        return false;
    }

    // WHAT: Validate embedded count bounds
    // WHY: Prevent buffer overflow in embedded array
    // HOW: Check against maximum capacity
    if (storage->embedded_count > SPARSE_SYNAPSE_EMBEDDED_CAPACITY) {
        LOG_ERROR("Embedded count %u exceeds capacity %u",
                  storage->embedded_count, SPARSE_SYNAPSE_EMBEDDED_CAPACITY);
        return false;
    }

    // WHAT: Validate overflow count bounds
    // WHY: Prevent buffer overflow in overflow array
    // HOW: Check count against capacity
    if (storage->overflow_count > storage->overflow_capacity) {
        LOG_ERROR("Overflow count %u exceeds capacity %u",
                  storage->overflow_count, storage->overflow_capacity);
        return false;
    }

    // WHAT: Validate overflow pointer consistency
    // WHY: Detect dangling pointers or memory corruption
    // HOW: Ensure capacity and pointer are consistent
    if (storage->overflow_capacity > 0 && storage->overflow == NULL) {
        LOG_ERROR("Overflow capacity %u but overflow pointer is NULL",
                  storage->overflow_capacity);
        return false;
    }

    return true;
}

/**
 * @brief Lock pool (if thread-safe enabled)
 */
static inline void pool_lock(sparse_synapse_pool_t pool) {
    if (pool->config.thread_safe) {
        nimcp_mutex_lock(&pool->mutex);
    }
}

/**
 * @brief Unlock pool (if thread-safe enabled)
 */
static inline void pool_unlock(sparse_synapse_pool_t pool) {
    if (pool->config.thread_safe) {
        nimcp_mutex_unlock(&pool->mutex);
    }
}

/**
 * @brief Allocate overflow array from size-class pool
 *
 * WHAT: Allocate overflow handles from appropriate size-class pool
 * WHY:  Extend capacity beyond embedded array with O(1) pooled allocation
 * HOW:  Find smallest fitting size class, allocate from that pool
 *
 * @param pool Pool handle
 * @param capacity Desired capacity (will be rounded up to size class)
 * @param actual_capacity Output: actual capacity allocated (size class capacity)
 * @return Pointer to allocated handles, or NULL on failure
 */
static synapse_handle_t* allocate_overflow(sparse_synapse_pool_t pool, uint32_t capacity,
                                           uint32_t* actual_capacity) {
    if (!validate_pool(pool)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pool_unlock: validate_pool is NULL");
        return NULL;
    }

    // Find appropriate size class
    int class_idx = find_size_class(capacity);

    void* ptr = NULL;
    uint32_t class_capacity;

    if (class_idx < 0) {
        // Oversized allocation — exceeds max size class, use direct malloc.
        // This happens when neurons accumulate many connections (e.g. output
        // neurons in large-backbone networks with >8192 incoming connections).
        class_capacity = capacity;
        size_t block_size = (size_t)capacity * sizeof(synapse_handle_t);
        ptr = nimcp_malloc(block_size);
        if (ptr == NULL) {
            LOG_ERROR("Failed to allocate oversized overflow (capacity=%u, %zu bytes)",
                      capacity, block_size);
            if (pool->config.enable_statistics) {
                atomic_fetch_add(&pool->failed_allocations, 1);
            }
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "allocate_overflow: oversized malloc failed");
            return NULL;
        }
        memset(ptr, 0, block_size);
        LOG_DEBUG("Oversized overflow allocated via nimcp_malloc (capacity=%u, %zu bytes)",
                  capacity, block_size);
    } else {
        // Normal path — use size-class pool with malloc fallback
        class_capacity = SIZE_CLASS_CAPACITIES[class_idx];
        size_t block_size = class_capacity * sizeof(synapse_handle_t);

        pool_lock(pool);
        if (pool->size_class_pools[class_idx] != NULL) {
            ptr = memory_pool_acquire(pool->size_class_pools[class_idx]);
        }
        pool_unlock(pool);

        if (ptr == NULL) {
            ptr = nimcp_malloc(block_size);
            if (ptr == NULL) {
                LOG_ERROR("Failed to allocate overflow array (capacity=%u, class=%d)",
                          capacity, class_idx);
                if (pool->config.enable_statistics) {
                    atomic_fetch_add(&pool->failed_allocations, 1);
                }
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "allocate_overflow: malloc fallback failed");
                return NULL;
            }
            LOG_DEBUG("Overflow allocated via nimcp_malloc fallback (class %d exhausted)", class_idx);
        }

        memset(ptr, 0, block_size);
    }

    // Return actual capacity
    if (actual_capacity) {
        *actual_capacity = class_capacity;
    }

    // Update statistics
    if (pool->config.enable_statistics) {
        atomic_fetch_add(&pool->overflow_allocations, 1);
        atomic_fetch_add(&pool->total_allocations, 1);
        if (class_idx >= 0) {
            atomic_fetch_add(&pool->size_class_hits[class_idx], 1);
        }
    }

    LOG_DEBUG("Allocated overflow array: requested=%u, actual=%u, class=%d, size=%zu bytes",
              capacity, class_capacity, class_idx,
              (size_t)class_capacity * sizeof(synapse_handle_t));
    return (synapse_handle_t*)ptr;
}

/**
 * @brief Free overflow array back to size-class pool
 *
 * WHAT: Return overflow handles to appropriate size-class pool
 * WHY:  Enable memory reuse with O(1) pooled deallocation
 * HOW:  Determine size class from capacity, return to that pool
 *
 * @param pool Pool handle
 * @param overflow Overflow array to free
 * @param capacity Capacity of the overflow array (determines size class)
 */
static void free_overflow(sparse_synapse_pool_t pool, synapse_handle_t* overflow,
                          uint32_t capacity) {
    if (!validate_pool(pool) || overflow == NULL) {
        return;
    }

    // Find the size class this allocation belongs to
    int class_idx = find_size_class(capacity);

    // Try to return to size-class pool
    bool returned_to_pool = false;
    if (class_idx >= 0) {
        pool_lock(pool);

        if (pool->size_class_pools[class_idx] != NULL) {
            // Check if this block belongs to the pool
            if (memory_pool_owns(pool->size_class_pools[class_idx], overflow)) {
                memory_pool_release(pool->size_class_pools[class_idx], overflow);
                returned_to_pool = true;
            }
        }

        pool_unlock(pool);
    }

    // If not from pool (was nimcp_malloc fallback), use nimcp_free
    if (!returned_to_pool) {
        nimcp_free(overflow);
    }

    if (pool->config.enable_statistics) {
        atomic_fetch_add(&pool->total_deallocations, 1);
    }

    LOG_DEBUG("Freed overflow array: capacity=%u, class=%d, pooled=%d",
              capacity, class_idx, returned_to_pool);
}

/**
 * @brief Grow overflow array using next size class
 *
 * WHAT: Increase overflow to next size class
 * WHY:  Amortize allocation overhead, use O(1) pooled allocation
 * HOW:  Allocate from next size class, copy old data, free old to current class
 *
 * @param pool Pool handle
 * @param storage Storage to grow
 * @return 0 on success, -1 on failure
 */
static int grow_overflow(sparse_synapse_pool_t pool, sparse_synapse_storage_t* storage) {
    if (!validate_pool(pool) || !validate_storage(storage)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "grow_overflow: required parameter is NULL (validate_pool, validate_storage)");
        return -1;
    }

    // Calculate desired new capacity (2x growth, minimum 16)
    uint32_t desired_capacity = storage->overflow_capacity > 0 ?
                                storage->overflow_capacity * 2 : SIZE_CLASS_MIN_HANDLES;

    LOG_DEBUG("Growing overflow: old_capacity=%u, desired=%u",
              storage->overflow_capacity, desired_capacity);

    // Allocate new overflow array (actual capacity will be size class capacity)
    uint32_t actual_capacity = 0;
    synapse_handle_t* new_overflow = allocate_overflow(pool, desired_capacity, &actual_capacity);
    if (new_overflow == NULL) {
        LOG_ERROR("Failed to grow overflow array");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "grow_overflow: validation failed");
        return -1;
    }

    // Copy old data
    uint32_t old_capacity = storage->overflow_capacity;
    if (storage->overflow != NULL && storage->overflow_count > 0) {
        memcpy(new_overflow, storage->overflow,
               storage->overflow_count * sizeof(synapse_handle_t));
        free_overflow(pool, storage->overflow, old_capacity);
    }

    // Update storage with actual size class capacity
    storage->overflow = new_overflow;
    storage->overflow_capacity = actual_capacity;

    LOG_DEBUG("Overflow grown: actual_capacity=%u (from size class)", actual_capacity);
    return 0;
}

//=============================================================================
// Pool Lifecycle Implementation
//=============================================================================

sparse_synapse_pool_config_t sparse_synapse_pool_default_config(void) {
    sparse_synapse_pool_config_t config = {
        .pool_size = SPARSE_SYNAPSE_DEFAULT_POOL_SIZE,
        .enable_statistics = true,
        .thread_safe = true
    };
    return config;
}

sparse_synapse_pool_t sparse_synapse_pool_create(
    const sparse_synapse_pool_config_t* config
) {
    LOG_INFO("Creating sparse synapse pool");

    // WHAT: Use defaults if config is NULL
    // WHY: Allow convenient creation with sensible defaults
    // HOW: Substitute default config when NULL provided
    sparse_synapse_pool_config_t default_config = sparse_synapse_pool_default_config();
    const sparse_synapse_pool_config_t* cfg = config ? config : &default_config;

    // WHAT: BBB validation of pool size
    // WHY: Prevent integer overflow and unreasonable allocations
    // HOW: Check against maximum reasonable pool size
    if (cfg->pool_size == 0 || cfg->pool_size > SPARSE_SYNAPSE_MAX_POOL_SIZE) {
        LOG_ERROR("Invalid pool size: %zu (must be 1..%u)", cfg->pool_size, SPARSE_SYNAPSE_MAX_POOL_SIZE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sparse_synapse_pool_create: cfg->pool_size is zero");
        return NULL;
    }

    // WHAT: Allocate pool structure
    // WHY: Encapsulate pool state
    // HOW: Heap allocation for flexibility
    sparse_synapse_pool_t pool = (sparse_synapse_pool_t)nimcp_malloc(sizeof(sparse_synapse_pool_struct_t));
    if (pool == NULL) {
        LOG_ERROR("Failed to allocate pool structure");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sparse_synapse_pool_create: validation failed");
        return NULL;
    }

    // WHAT: Initialize structure
    // WHY: Clean slate for all fields
    // HOW: Zero memory then set specific values
    memset(pool, 0, sizeof(sparse_synapse_pool_struct_t));
    pool->magic = SPARSE_SYNAPSE_MAGIC;
    pool->config = *cfg;

    // WHAT: Initialize BBB security system
    // WHY: Enable runtime security validation
    // HOW: Create BBB system for input validation
    pool->bbb_system = bbb_system_create(NULL);  // Use default config
    if (pool->bbb_system == NULL) {
        LOG_DEBUG("BBB security validation disabled for sparse synapse pool");
        // Continue without BBB - not fatal
    } else {
        LOG_DEBUG("BBB security system initialized for sparse synapse pool");
    }

    // WHAT: Create size-class pools for overflow handles
    // WHY: Pre-allocate memory at different sizes for O(1) pooled allocation
    // HOW: Create a memory_pool for each size class
    LOG_DEBUG("Creating %d size-class pools for overflow allocation", SIZE_CLASS_COUNT);

    bool pool_creation_failed = false;
    for (int i = 0; i < SIZE_CLASS_COUNT; i++) {
        size_t block_size = SIZE_CLASS_CAPACITIES[i] * sizeof(synapse_handle_t);
        uint32_t num_blocks = SIZE_CLASS_BLOCK_COUNTS[i];

        // Scale block counts by pool_size ratio
        uint32_t scaled_blocks = (uint32_t)((uint64_t)num_blocks * cfg->pool_size /
                                             SPARSE_SYNAPSE_DEFAULT_POOL_SIZE);
        if (scaled_blocks < 4) scaled_blocks = 4;  // Minimum 4 blocks per class

        memory_pool_config_t pool_config = memory_pool_default_config(block_size, scaled_blocks);
        pool->size_class_pools[i] = memory_pool_create(&pool_config);

        if (pool->size_class_pools[i] == NULL) {
            LOG_WARNING("Failed to create size-class pool %d (size=%zu, blocks=%u) - will use fallback",
                        i, block_size, scaled_blocks);
            // Continue without this pool - will use nimcp_malloc fallback
        } else {
            LOG_TRACE("Size-class pool %d created: capacity=%u handles, blocks=%u",
                      i, SIZE_CLASS_CAPACITIES[i], scaled_blocks);
        }

        // Initialize per-class statistics
        atomic_store(&pool->size_class_hits[i], 0);
    }

    // WHAT: Initialize mutex for thread safety
    // WHY: Protect concurrent pool operations
    // HOW: Create mutex if thread_safe flag enabled
    if (cfg->thread_safe) {
        if (nimcp_mutex_init(&pool->mutex, NULL) != NIMCP_SUCCESS) {
            LOG_ERROR("Failed to initialize pool mutex");
            // Cleanup size-class pools
            for (int i = 0; i < SIZE_CLASS_COUNT; i++) {
                if (pool->size_class_pools[i] != NULL) {
                    memory_pool_destroy(pool->size_class_pools[i]);
                }
            }
            if (pool->bbb_system != NULL) {
                bbb_system_destroy(pool->bbb_system);
            }
            nimcp_free(pool);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sparse_synapse_pool_create: validation failed");
            return NULL;
        }
    }

    // WHAT: Initialize statistics counters
    // WHY: Track pool usage and performance
    // HOW: Atomic variables for thread-safe access
    atomic_store(&pool->total_synapses, 0);
    atomic_store(&pool->embedded_synapses, 0);
    atomic_store(&pool->overflow_synapses, 0);
    atomic_store(&pool->total_allocations, 0);
    atomic_store(&pool->total_deallocations, 0);
    atomic_store(&pool->failed_allocations, 0);
    atomic_store(&pool->overflow_allocations, 0);
    atomic_store(&pool->peak_pool_usage, 0);

    LOG_INFO("Sparse synapse pool created: pool_size=%zu, thread_safe=%d, bbb_enabled=%d",
             cfg->pool_size, cfg->thread_safe, pool->bbb_system != NULL);

    return pool;
}

void sparse_synapse_pool_destroy(sparse_synapse_pool_t pool) {
    // WHAT: Validate pool before destruction
    // WHY: Prevent double-free or invalid destruction
    // HOW: Use validate_pool helper
    if (!validate_pool(pool)) {
        return;
    }

    LOG_INFO("Destroying sparse synapse pool");

    // WHAT: Print final statistics if enabled
    // WHY: Provide usage summary for debugging/optimization
    // HOW: Read atomic counters before cleanup
    if (pool->config.enable_statistics) {
        LOG_INFO("Final pool statistics:");
        LOG_INFO("  Total synapses: %lu", atomic_load(&pool->total_synapses));
        LOG_INFO("  Embedded: %lu, Overflow: %lu",
                 atomic_load(&pool->embedded_synapses),
                 atomic_load(&pool->overflow_synapses));
        LOG_INFO("  Allocations: %lu, Deallocations: %lu",
                 atomic_load(&pool->total_allocations),
                 atomic_load(&pool->total_deallocations));
        LOG_INFO("  Failed allocations: %lu", atomic_load(&pool->failed_allocations));
    }

    // WHAT: Destroy BBB security system
    // WHY: Free security resources
    // HOW: Call BBB cleanup if initialized
    if (pool->bbb_system != NULL) {
        bbb_system_destroy(pool->bbb_system);
        pool->bbb_system = NULL;
    }

    // WHAT: Destroy size-class pools
    // WHY: Free all pre-allocated overflow blocks
    // HOW: Iterate and destroy each size-class pool
    LOG_DEBUG("Destroying %d size-class pools", SIZE_CLASS_COUNT);
    for (int i = 0; i < SIZE_CLASS_COUNT; i++) {
        if (pool->size_class_pools[i] != NULL) {
            memory_pool_destroy(pool->size_class_pools[i]);
            pool->size_class_pools[i] = NULL;
        }
    }

    // WHAT: Destroy mutex
    // WHY: Release synchronization resources
    // HOW: Destroy mutex if thread-safe enabled
    if (pool->config.thread_safe) {
        nimcp_mutex_destroy(&pool->mutex);
    }

    // WHAT: Clear magic and free
    // WHY: Detect use-after-free in validation
    // HOW: Zero magic number before freeing
    pool->magic = 0;
    nimcp_free(pool);

    LOG_INFO("Sparse synapse pool destroyed");
}

//=============================================================================
// Synapse Storage Implementation
//=============================================================================

void sparse_synapse_storage_init(sparse_synapse_storage_t* storage) {
    if (storage == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sparse_synapse_storage_init: storage is NULL");
        return;
    }

    memset(storage, 0, sizeof(sparse_synapse_storage_t));
    storage->embedded_count = 0;
    storage->overflow = NULL;
    storage->overflow_count = 0;
    storage->overflow_capacity = 0;
}

void sparse_synapse_storage_cleanup(
    sparse_synapse_pool_t pool,
    sparse_synapse_storage_t* storage
) {
    if (!validate_pool(pool) || !validate_storage(storage)) {
        return;
    }

    // Update statistics (remove synapses)
    if (pool->config.enable_statistics) {
        uint64_t total = storage->embedded_count + storage->overflow_count;
        atomic_fetch_sub(&pool->total_synapses, total);
        atomic_fetch_sub(&pool->embedded_synapses, storage->embedded_count);
        atomic_fetch_sub(&pool->overflow_synapses, storage->overflow_count);
    }

    // Free overflow if allocated
    if (storage->overflow != NULL) {
        free_overflow(pool, storage->overflow, storage->overflow_capacity);
        storage->overflow = NULL;
        storage->overflow_capacity = 0;
        storage->overflow_count = 0;
    }

    // Reset embedded count
    storage->embedded_count = 0;
}

int sparse_synapse_add(
    sparse_synapse_pool_t pool,
    sparse_synapse_storage_t* storage,
    uint32_t target_neuron_id,
    float weight
) {
    // WHAT: Validate pool and storage
    // WHY: Prevent corruption and crashes
    // HOW: Use validation helpers
    if (!validate_pool(pool) || !validate_storage(storage)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sparse_synapse_add: required parameter is NULL (validate_pool, validate_storage)");
        return -1;
    }

    // WHAT: BBB validation of target neuron ID
    // WHY: Prevent wild pointer access or invalid targets
    // HOW: Check against maximum reasonable neuron ID
    if (!bbb_validate_range_u(target_neuron_id, 0, 100000000, "sparse_synapse_add")) {
        LOG_ERROR("BBB validation failed for target_neuron_id: %u", target_neuron_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "sparse_synapse_add: bbb_validate_range_u is NULL");
        return -1;
    }

    // WHAT: Validate weight is finite
    // WHY: Prevent NaN/Inf propagation in network
    // HOW: Check with isfinite()
    if (!isfinite(weight)) {
        LOG_ERROR("Invalid weight (NaN or Inf): %f", weight);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "sparse_synapse_add: isfinite is NULL");
        return -1;
    }

    // WHAT: Create synapse handle
    // WHY: Encapsulate synapse data
    // HOW: Initialize struct with target, weight, and no metadata by default
    synapse_handle_t handle = {
        .target_neuron_id = target_neuron_id,
        .weight = weight,
        .strength = 1.0F,
        .metadata_index = SPARSE_SYNAPSE_NO_METADATA,
        .peer_index = SPARSE_SYNAPSE_NO_PEER
    };

    // WHAT: Try to add to embedded array first
    // WHY: Optimize for common case (most neurons have <64 synapses)
    // HOW: Check if space available in embedded array
    if (storage->embedded_count < SPARSE_SYNAPSE_EMBEDDED_CAPACITY) {
        storage->embedded[storage->embedded_count] = handle;
        storage->embedded_count++;

        // WHAT: Update statistics
        // WHY: Track memory usage
        // HOW: Atomic increment
        if (pool->config.enable_statistics) {
            atomic_fetch_add(&pool->total_synapses, 1);
            atomic_fetch_add(&pool->embedded_synapses, 1);
        }

        LOG_TRACE("Added synapse to embedded: target=%u, weight=%f, count=%u",
                  target_neuron_id, weight, storage->embedded_count);
        return 0;
    }

    // Embedded full, use overflow
    // Check if we need to allocate or grow overflow
    if (storage->overflow == NULL) {
        // First overflow allocation - use minimum size class
        uint32_t actual_capacity = 0;
        storage->overflow = allocate_overflow(pool, SIZE_CLASS_MIN_HANDLES, &actual_capacity);
        if (storage->overflow == NULL) {
            /* Pool exhausted — embedded capacity still works, skip overflow.
             * Don't THROW — rebuild_incoming calls this millions of times. */
            return -1;
        }
        storage->overflow_capacity = actual_capacity;
        storage->overflow_count = 0;
    } else if (storage->overflow_count >= storage->overflow_capacity) {
        // Need to grow overflow
        if (grow_overflow(pool, storage) != 0) {
            return -1;
        }
    }

    // Add to overflow
    storage->overflow[storage->overflow_count] = handle;
    storage->overflow_count++;

    // Update statistics
    if (pool->config.enable_statistics) {
        atomic_fetch_add(&pool->total_synapses, 1);
        atomic_fetch_add(&pool->overflow_synapses, 1);

        // Update peak usage (sum across all size-class pools)
        size_t current_usage = 0;
        for (int i = 0; i < SIZE_CLASS_COUNT; i++) {
            if (pool->size_class_pools[i] != NULL) {
                memory_pool_stats_t stats;
                if (memory_pool_get_stats(pool->size_class_pools[i], &stats)) {
                    current_usage += stats.allocated_blocks;
                }
            }
        }
        size_t peak = atomic_load(&pool->peak_pool_usage);
        if (current_usage > peak) {
            atomic_store(&pool->peak_pool_usage, current_usage);
        }
    }

    LOG_TRACE("Added synapse to overflow: target=%u, weight=%f, overflow_count=%u",
              target_neuron_id, weight, storage->overflow_count);
    return 0;
}

int sparse_synapse_storage_reserve(
    sparse_synapse_pool_t pool,
    sparse_synapse_storage_t* storage,
    uint32_t count
) {
    if (!validate_pool(pool) || !validate_storage(storage)) return -1;
    if (count == 0) return 0;

    /* Embedded portion needs no allocation — it's inline.
     * Only pre-allocate overflow if count > EMBEDDED_CAPACITY. */
    if (count <= SPARSE_SYNAPSE_EMBEDDED_CAPACITY) return 0;

    uint32_t overflow_needed = count - SPARSE_SYNAPSE_EMBEDDED_CAPACITY;
    if (storage->overflow == NULL || storage->overflow_capacity < overflow_needed) {
        /* Free existing undersized overflow if any */
        if (storage->overflow) {
            free_overflow(pool, storage->overflow, storage->overflow_capacity);
            storage->overflow = NULL;
            storage->overflow_capacity = 0;
            storage->overflow_count = 0;
        }
        uint32_t actual = 0;
        storage->overflow = allocate_overflow(pool, overflow_needed, &actual);
        if (!storage->overflow) return -1;
        storage->overflow_capacity = actual;
        storage->overflow_count = 0;
    }
    return 0;
}

int sparse_synapse_remove(
    sparse_synapse_pool_t pool,
    sparse_synapse_storage_t* storage,
    uint32_t index
) {
    if (!validate_pool(pool) || !validate_storage(storage)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sparse_synapse_remove: required parameter is NULL (validate_pool, validate_storage)");
        return -1;
    }

    uint32_t total = storage->embedded_count + storage->overflow_count;
    if (index >= total) {
        LOG_DEBUG("sparse_synapse_remove: index %u >= total %u, skipping", index, total);
        return -1;
    }

    // Determine if removing from embedded or overflow
    if (index < storage->embedded_count) {
        // Remove from embedded (swap with last embedded element)
        if (index < storage->embedded_count - 1) {
            storage->embedded[index] = storage->embedded[storage->embedded_count - 1];
        }
        storage->embedded_count--;

        // Update statistics
        if (pool->config.enable_statistics) {
            atomic_fetch_sub(&pool->total_synapses, 1);
            atomic_fetch_sub(&pool->embedded_synapses, 1);
        }

        LOG_DEBUG("Removed synapse from embedded: index=%u, remaining=%u",
                  index, storage->embedded_count);
    } else {
        // Remove from overflow
        uint32_t overflow_index = index - storage->embedded_count;
        if (overflow_index < storage->overflow_count - 1) {
            storage->overflow[overflow_index] =
                storage->overflow[storage->overflow_count - 1];
        }
        storage->overflow_count--;

        // Update statistics
        if (pool->config.enable_statistics) {
            atomic_fetch_sub(&pool->total_synapses, 1);
            atomic_fetch_sub(&pool->overflow_synapses, 1);
        }

        LOG_DEBUG("Removed synapse from overflow: index=%u, remaining=%u",
                  overflow_index, storage->overflow_count);
    }

    return 0;
}

synapse_handle_t* sparse_synapse_get(
    sparse_synapse_storage_t* storage,
    uint32_t index
) {
    if (!validate_storage(storage)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sparse_synapse_get: validate_storage is NULL");
        return NULL;
    }

    uint32_t total = storage->embedded_count + storage->overflow_count;
    if (index >= total) {
        /* Hot-path lookup — return NULL silently; callers already handle NULL.
         * Stale peer_index values after checkpoint restore cause frequent
         * out-of-bounds here; throwing 5000+/sec kills performance. */
        return NULL;
    }

    // Return from embedded or overflow
    if (index < storage->embedded_count) {
        return &storage->embedded[index];
    } else {
        if (!storage->overflow) {
            return NULL;
        }
        uint32_t overflow_index = index - storage->embedded_count;
        return &storage->overflow[overflow_index];
    }
}

uint32_t sparse_synapse_count(const sparse_synapse_storage_t* storage) {
    if (!validate_storage(storage)) {
        return 0;
    }
    return storage->embedded_count + storage->overflow_count;
}

uint32_t sparse_synapse_compact(
    sparse_synapse_pool_t pool,
    sparse_synapse_storage_t* storage
) {
    if (!validate_pool(pool) || !validate_storage(storage)) {
        return 0;
    }

    // Check if compaction is beneficial
    uint32_t total = storage->embedded_count + storage->overflow_count;
    if (total <= SPARSE_SYNAPSE_EMBEDDED_CAPACITY) {
        // All synapses can fit in embedded
        uint32_t moved = 0;

        // Copy overflow to embedded
        if (storage->overflow_count > 0) {
            memcpy(&storage->embedded[storage->embedded_count],
                   storage->overflow,
                   storage->overflow_count * sizeof(synapse_handle_t));
            moved = storage->overflow_count;
            storage->embedded_count += storage->overflow_count;

            // Update statistics
            if (pool->config.enable_statistics) {
                atomic_fetch_add(&pool->embedded_synapses, moved);
                atomic_fetch_sub(&pool->overflow_synapses, moved);
            }
        }

        // Free overflow (pass capacity before clearing it)
        if (storage->overflow != NULL) {
            uint32_t old_capacity = storage->overflow_capacity;
            free_overflow(pool, storage->overflow, old_capacity);
            storage->overflow = NULL;
            storage->overflow_capacity = 0;
            storage->overflow_count = 0;
        }

        LOG_INFO("Compacted storage: moved %u synapses to embedded", moved);
        return moved;
    }

    return 0;
}

//=============================================================================
// Iterator Implementation
//=============================================================================

void sparse_synapse_iterator_init(
    sparse_synapse_iterator_t* it,
    sparse_synapse_storage_t* storage
) {
    if (it == NULL || storage == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sparse_synapse_iterator_init: iterator or storage is NULL");
        return;
    }

    it->storage = storage;
    it->index = 0;
    it->in_overflow = false;
}

synapse_handle_t* sparse_synapse_iterator_next(sparse_synapse_iterator_t* it) {
    if (it == NULL || it->storage == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sparse_synapse_iterator_next: validation failed");
        return NULL;
    }

    sparse_synapse_storage_t* storage = it->storage;

    // Check if we're in embedded array
    if (!it->in_overflow) {
        if (it->index < storage->embedded_count) {
            return &storage->embedded[it->index++];
        }
        // Switch to overflow
        it->in_overflow = true;
        it->index = 0;
    }

    // Check overflow
    if (it->in_overflow && it->index < storage->overflow_count) {
        return &storage->overflow[it->index++];
    }

    // End of iteration — normal control flow, not an error
    return NULL;
}

bool sparse_synapse_iterator_has_next(const sparse_synapse_iterator_t* it) {
    if (it == NULL || it->storage == NULL) {
        return false;
    }

    sparse_synapse_storage_t* storage = it->storage;

    if (!it->in_overflow) {
        return it->index < storage->embedded_count;
    } else {
        return it->index < storage->overflow_count;
    }
}

void sparse_synapse_iterator_reset(sparse_synapse_iterator_t* it) {
    if (it == NULL) {
        return;
    }
    it->index = 0;
    it->in_overflow = false;
}

//=============================================================================
// Statistics Implementation
//=============================================================================

int sparse_synapse_pool_get_stats(
    sparse_synapse_pool_t pool,
    sparse_synapse_stats_t* stats
) {
    if (!validate_pool(pool) || stats == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sparse_synapse_pool_get_stats: validate_pool is NULL");
        return -1;
    }

    memset(stats, 0, sizeof(sparse_synapse_stats_t));

    // Read atomic counters
    stats->total_synapses = atomic_load(&pool->total_synapses);
    stats->embedded_synapses = atomic_load(&pool->embedded_synapses);
    stats->overflow_synapses = atomic_load(&pool->overflow_synapses);
    stats->total_allocations = atomic_load(&pool->total_allocations);
    stats->total_deallocations = atomic_load(&pool->total_deallocations);
    stats->failed_allocations = atomic_load(&pool->failed_allocations);
    stats->overflow_allocations = atomic_load(&pool->overflow_allocations);
    stats->peak_pool_usage = atomic_load(&pool->peak_pool_usage);

    // Get aggregated pool statistics from all size-class pools
    size_t total_blocks = 0, allocated_blocks = 0, free_blocks = 0, pool_bytes = 0;
    for (int i = 0; i < SIZE_CLASS_COUNT; i++) {
        if (pool->size_class_pools[i] != NULL) {
            memory_pool_stats_t class_stats;
            if (memory_pool_get_stats(pool->size_class_pools[i], &class_stats)) {
                total_blocks += class_stats.total_blocks;
                allocated_blocks += class_stats.allocated_blocks;
                free_blocks += class_stats.free_blocks;
                pool_bytes += class_stats.pool_size_bytes;
            }
        }
    }
    stats->pool_size = total_blocks;
    stats->pool_used = allocated_blocks;
    stats->pool_available = free_blocks;

    // Calculate memory usage
    stats->embedded_memory_bytes = stats->embedded_synapses * sizeof(synapse_handle_t);
    stats->overflow_memory_bytes = stats->overflow_synapses * sizeof(synapse_handle_t);
    stats->pool_memory_bytes = pool_bytes;
    stats->total_memory_bytes = stats->embedded_memory_bytes +
                                stats->overflow_memory_bytes +
                                stats->pool_memory_bytes;

    // Calculate derived metrics
    if (stats->pool_size > 0) {
        stats->pool_utilization = (float)stats->pool_used / (float)stats->pool_size;
    }

    if (stats->total_synapses > 0) {
        // Estimate neurons (assuming average usage)
        uint64_t estimated_neurons = stats->embedded_synapses / 32 +
                                     stats->overflow_synapses / 64;
        if (estimated_neurons > 0) {
            stats->avg_synapses_per_neuron = (float)stats->total_synapses /
                                             (float)estimated_neurons;
        }
        stats->overflow_percentage = (float)stats->overflow_synapses /
                                     (float)stats->total_synapses;
    }

    return 0;
}

void sparse_synapse_pool_reset_stats(sparse_synapse_pool_t pool) {
    if (!validate_pool(pool)) {
        return;
    }

    atomic_store(&pool->total_allocations, 0);
    atomic_store(&pool->total_deallocations, 0);
    atomic_store(&pool->failed_allocations, 0);
    atomic_store(&pool->overflow_allocations, 0);
    atomic_store(&pool->peak_pool_usage, 0);

    LOG_INFO("Pool statistics reset");
}

float sparse_synapse_pool_utilization(sparse_synapse_pool_t pool) {
    if (!validate_pool(pool)) {
        return 0.0F;
    }

    // Aggregate utilization across all size-class pools
    size_t total_blocks = 0, allocated_blocks = 0;
    for (int i = 0; i < SIZE_CLASS_COUNT; i++) {
        if (pool->size_class_pools[i] != NULL) {
            memory_pool_stats_t stats;
            if (memory_pool_get_stats(pool->size_class_pools[i], &stats)) {
                total_blocks += stats.total_blocks;
                allocated_blocks += stats.allocated_blocks;
            }
        }
    }

    if (total_blocks == 0) {
        return 0.0F;
    }

    return (float)allocated_blocks / (float)total_blocks;
}

size_t sparse_synapse_pool_available(sparse_synapse_pool_t pool) {
    if (!validate_pool(pool)) {
        return 0;
    }

    // Sum available blocks across all size-class pools
    size_t total_available = 0;
    for (int i = 0; i < SIZE_CLASS_COUNT; i++) {
        if (pool->size_class_pools[i] != NULL) {
            total_available += memory_pool_get_available(pool->size_class_pools[i]);
        }
    }
    return total_available;
}

//=============================================================================
// Utility Implementation
//=============================================================================

float sparse_synapse_memory_savings(
    sparse_synapse_pool_t pool,
    size_t num_neurons,
    size_t dense_synapses_per_neuron,
    size_t bytes_per_synapse
) {
    if (!validate_pool(pool) || num_neurons == 0 ||
        dense_synapses_per_neuron == 0 || bytes_per_synapse == 0) {
        return 0.0F;
    }

    // Calculate dense allocation
    size_t dense_memory = num_neurons * dense_synapses_per_neuron * bytes_per_synapse;

    // Calculate sparse allocation
    sparse_synapse_stats_t stats;
    if (sparse_synapse_pool_get_stats(pool, &stats) != 0) {
        return 0.0F;
    }

    size_t sparse_memory = stats.total_memory_bytes;

    // Calculate savings percentage
    if (dense_memory == 0) {
        return 0.0F;
    }

    float savings = 1.0F - ((float)sparse_memory / (float)dense_memory);
    return fmaxf(0.0F, fminf(1.0F, savings));  // Clamp to [0, 1]
}

void sparse_synapse_pool_print_stats(sparse_synapse_pool_t pool, bool verbose) {
    if (!validate_pool(pool)) {
        return;
    }

    sparse_synapse_stats_t stats;
    if (sparse_synapse_pool_get_stats(pool, &stats) != 0) {
        printf("Failed to retrieve pool statistics\n");
        return;
    }

    printf("\n");
    printf("========================================\n");
    printf("Sparse Synapse Pool Statistics\n");
    printf("========================================\n");
    printf("\n");

    printf("Synapse Counts:\n");
    printf("  Total synapses:    %12lu\n", stats.total_synapses);
    printf("  Embedded:          %12lu (%.1f%%)\n",
           stats.embedded_synapses,
           stats.total_synapses > 0 ?
           (100.0 * stats.embedded_synapses / stats.total_synapses) : 0.0);
    printf("  Overflow:          %12lu (%.1f%%)\n",
           stats.overflow_synapses,
           stats.total_synapses > 0 ?
           (100.0 * stats.overflow_synapses / stats.total_synapses) : 0.0);
    printf("\n");

    printf("Memory Usage:\n");
    printf("  Embedded memory:   %12zu bytes (%.2f MB)\n",
           stats.embedded_memory_bytes,
           stats.embedded_memory_bytes / (1024.0 * 1024.0));
    printf("  Overflow memory:   %12zu bytes (%.2f MB)\n",
           stats.overflow_memory_bytes,
           stats.overflow_memory_bytes / (1024.0 * 1024.0));
    printf("  Pool memory:       %12zu bytes (%.2f MB)\n",
           stats.pool_memory_bytes,
           stats.pool_memory_bytes / (1024.0 * 1024.0));
    printf("  Total memory:      %12zu bytes (%.2f MB)\n",
           stats.total_memory_bytes,
           stats.total_memory_bytes / (1024.0 * 1024.0));
    printf("\n");

    printf("Pool Status:\n");
    printf("  Pool size:         %12zu handles\n", stats.pool_size);
    printf("  Pool used:         %12zu handles\n", stats.pool_used);
    printf("  Pool available:    %12zu handles\n", stats.pool_available);
    printf("  Pool utilization:  %12.1f%%\n", stats.pool_utilization * 100.0);
    printf("  Peak usage:        %12zu handles\n", stats.peak_pool_usage);
    printf("\n");

    if (verbose) {
        printf("Allocation Statistics:\n");
        printf("  Total allocations:   %12lu\n", stats.total_allocations);
        printf("  Total deallocations: %12lu\n", stats.total_deallocations);
        printf("  Failed allocations:  %12lu\n", stats.failed_allocations);
        printf("  Overflow allocs:     %12lu\n", stats.overflow_allocations);
        printf("\n");

        printf("Performance Metrics:\n");
        printf("  Avg synapses/neuron: %12.1f\n", stats.avg_synapses_per_neuron);
        printf("  Overflow percentage: %12.1f%%\n", stats.overflow_percentage * 100.0);
        printf("\n");
    }

    printf("========================================\n");
    printf("\n");
}

//=============================================================================
// Synapse Metadata Pool Implementation
//=============================================================================
//
// WHAT: Pool of full synapse_t objects for advanced synapse features
// WHY:  Most synapses only need target+weight; allocate full metadata on-demand
// HOW:  Fixed-size array of synapse_t with free-list allocation
//

// Include synapse_t definition
#include "core/neuralnet/nimcp_neuralnet.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(sparse_synapse)

// Default metadata pool size (5% of typical 200K synapses)
#define SYNAPSE_METADATA_POOL_DEFAULT_SIZE 10000
#define SYNAPSE_METADATA_POOL_MAGIC 0x534D5054  // 'SMPT'

// Chunked block allocator constants — blocks never move, so pointers stay stable
#define SYNAPSE_METADATA_BLOCK_SHIFT 16
#define SYNAPSE_METADATA_BLOCK_SIZE  (1u << SYNAPSE_METADATA_BLOCK_SHIFT)  // 65536
#define SYNAPSE_METADATA_BLOCK_MASK  (SYNAPSE_METADATA_BLOCK_SIZE - 1)
#define SYNAPSE_METADATA_INITIAL_MAX_BLOCKS 16

/**
 * @brief Synapse metadata pool internal structure
 *
 * Uses a chunked block allocator: synapse_t objects are stored in fixed-size
 * blocks that are never moved or reallocated. This guarantees that pointers
 * obtained via synapse_metadata_pool_get() remain valid even after the pool
 * grows. Index decomposition: block = idx >> 16, offset = idx & 0xFFFF.
 */
typedef struct synapse_metadata_pool_struct {
    uint32_t magic;                     /**< Magic number for validation */
    synapse_metadata_pool_config_t config;

    // Chunked block storage — blocks never move, pointers stay stable
    synapse_t** blocks;                 /**< Array of block pointers */
    uint32_t num_blocks;                /**< Number of allocated blocks */
    uint32_t max_blocks;                /**< Capacity of blocks pointer array */

    // Free-list allocator
    uint32_t* free_list;                /**< Free indices (stack-based) */
    uint32_t free_count;                /**< Number of free slots */
    uint32_t free_list_capacity;        /**< Allocated capacity of free_list */
    uint32_t pool_size;                 /**< Total pool capacity */

    // Statistics
    _Atomic uint64_t total_allocations;
    _Atomic uint64_t total_deallocations;
    _Atomic uint64_t failed_allocations;
    _Atomic size_t peak_usage;

    // Thread safety
    nimcp_mutex_t mutex;

    // Capacity flag — set once when pool reaches max size and can't grow.
    // Read lock-free by fast-path rejection in allocate(). Monotonic false→true.
    volatile bool at_capacity;
} synapse_metadata_pool_struct_t;

// Validation helper
static bool validate_metadata_pool(synapse_metadata_pool_t pool) {
    return pool != NULL && pool->magic == SYNAPSE_METADATA_POOL_MAGIC;
}

synapse_metadata_pool_config_t synapse_metadata_pool_default_config(void) {
    return (synapse_metadata_pool_config_t){
        .pool_size = SYNAPSE_METADATA_POOL_DEFAULT_SIZE,
        .enable_statistics = true,
        .thread_safe = true
    };
}

synapse_metadata_pool_t synapse_metadata_pool_create(
    const synapse_metadata_pool_config_t* config
) {
    // Use defaults if no config provided
    synapse_metadata_pool_config_t cfg;
    if (config != NULL) {
        cfg = *config;
    } else {
        cfg = synapse_metadata_pool_default_config();
    }

    // Validate pool size
    if (cfg.pool_size == 0 || cfg.pool_size > SPARSE_SYNAPSE_MAX_POOL_SIZE) {
        LOG_ERROR("Invalid metadata pool size: %zu", cfg.pool_size);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "synapse_metadata_pool_create: cfg.pool_size is zero");
        return NULL;
    }

    // Allocate pool structure
    synapse_metadata_pool_struct_t* pool = (synapse_metadata_pool_struct_t*)
        nimcp_calloc(1, sizeof(synapse_metadata_pool_struct_t));
    if (pool == NULL) {
        LOG_ERROR("Failed to allocate metadata pool structure");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "synapse_metadata_pool_create: alloc failed");
        return NULL;
    }

    pool->magic = SYNAPSE_METADATA_POOL_MAGIC;
    pool->config = cfg;

    // Compute number of blocks needed (round up to full blocks)
    uint32_t num_blocks = ((uint32_t)cfg.pool_size + SYNAPSE_METADATA_BLOCK_SIZE - 1)
                          >> SYNAPSE_METADATA_BLOCK_SHIFT;
    if (num_blocks == 0) num_blocks = 1;
    uint32_t actual_pool_size = num_blocks * SYNAPSE_METADATA_BLOCK_SIZE;

    // Allocate blocks pointer array
    pool->max_blocks = num_blocks < SYNAPSE_METADATA_INITIAL_MAX_BLOCKS
                       ? SYNAPSE_METADATA_INITIAL_MAX_BLOCKS
                       : num_blocks * 2;
    pool->blocks = (synapse_t**)nimcp_calloc(pool->max_blocks, sizeof(synapse_t*));
    if (pool->blocks == NULL) {
        LOG_ERROR("Failed to allocate blocks array");
        nimcp_free(pool);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "synapse_metadata_pool_create: blocks alloc failed");
        return NULL;
    }

    // Allocate each block
    for (uint32_t b = 0; b < num_blocks; b++) {
        pool->blocks[b] = (synapse_t*)nimcp_calloc(SYNAPSE_METADATA_BLOCK_SIZE, sizeof(synapse_t));
        if (pool->blocks[b] == NULL) {
            LOG_ERROR("Failed to allocate metadata block %u", b);
            for (uint32_t j = 0; j < b; j++) {
                nimcp_free(pool->blocks[j]);
            }
            nimcp_free(pool->blocks);
            nimcp_free(pool);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "synapse_metadata_pool_create: block alloc failed");
            return NULL;
        }
    }
    pool->num_blocks = num_blocks;
    pool->pool_size = actual_pool_size;

    // Allocate free list
    pool->free_list_capacity = actual_pool_size;
    pool->free_list = (uint32_t*)nimcp_malloc(actual_pool_size * sizeof(uint32_t));
    if (pool->free_list == NULL) {
        LOG_ERROR("Failed to allocate free list");
        for (uint32_t b = 0; b < num_blocks; b++) {
            nimcp_free(pool->blocks[b]);
        }
        nimcp_free(pool->blocks);
        nimcp_free(pool);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "synapse_metadata_pool_create: free_list alloc failed");
        return NULL;
    }

    // Initialize free list (all slots free, in reverse order for LIFO)
    for (uint32_t i = 0; i < actual_pool_size; i++) {
        pool->free_list[i] = actual_pool_size - 1 - i;
    }
    pool->free_count = actual_pool_size;

    // Initialize mutex if thread-safe
    if (cfg.thread_safe) {
        if (nimcp_mutex_init(&pool->mutex, NULL) != 0) {
            LOG_ERROR("Failed to initialize metadata pool mutex");
            nimcp_free(pool->free_list);
            for (uint32_t b = 0; b < num_blocks; b++) {
                nimcp_free(pool->blocks[b]);
            }
            nimcp_free(pool->blocks);
            nimcp_free(pool);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "synapse_metadata_pool_create: mutex init failed");
            return NULL;
        }
    }

    // Initialize atomic counters
    atomic_store(&pool->total_allocations, 0);
    atomic_store(&pool->total_deallocations, 0);
    atomic_store(&pool->failed_allocations, 0);
    atomic_store(&pool->peak_usage, 0);

    LOG_INFO("Created synapse metadata pool with %u slots in %u blocks (%.2f MB)",
             pool->pool_size, pool->num_blocks,
             (pool->pool_size * sizeof(synapse_t)) / (1024.0 * 1024.0));

    return pool;
}

void synapse_metadata_pool_destroy(synapse_metadata_pool_t pool) {
    if (!validate_metadata_pool(pool)) {
        return;
    }

    // Destroy mutex
    if (pool->config.thread_safe) {
        nimcp_mutex_destroy(&pool->mutex);
    }

    // Free all blocks
    if (pool->blocks != NULL) {
        for (uint32_t b = 0; b < pool->num_blocks; b++) {
            if (pool->blocks[b] != NULL) {
                nimcp_free(pool->blocks[b]);
            }
        }
        nimcp_free(pool->blocks);
    }

    // Free the free list
    if (pool->free_list != NULL) {
        nimcp_free(pool->free_list);
    }

    pool->magic = 0;  // Invalidate
    nimcp_free(pool);

    LOG_DEBUG("Destroyed synapse metadata pool");
}

bool synapse_metadata_pool_is_full(synapse_metadata_pool_t pool) {
    if (!pool) return true;
    return pool->at_capacity;
}

uint32_t synapse_metadata_pool_allocate(synapse_metadata_pool_t pool) {
    if (!validate_metadata_pool(pool)) {
        return SPARSE_SYNAPSE_NO_METADATA;
    }

    /* Fast-path rejection: once the pool has reached max capacity and failed
     * at least once, it will never grow again. Skip the mutex entirely.
     * Uses a sticky flag (at_capacity) set inside the mutex-protected path
     * when growth fails — no race: monotonically false→true. */
    if (pool->at_capacity) {
        atomic_fetch_add(&pool->failed_allocations, 1);
        return SPARSE_SYNAPSE_NO_METADATA;
    }

    // Lock if thread-safe
    if (pool->config.thread_safe) {
        nimcp_mutex_lock(&pool->mutex);
    }

    // Grow pool by adding a new block if exhausted
    if (pool->free_count == 0) {
        uint32_t new_pool_size = pool->pool_size + SYNAPSE_METADATA_BLOCK_SIZE;
        if (new_pool_size > SPARSE_SYNAPSE_MAX_POOL_SIZE) {
            pool->at_capacity = true;  /* Sticky flag — enables fast-path rejection */
            if (pool->config.thread_safe) {
                nimcp_mutex_unlock(&pool->mutex);
            }
            uint64_t fails = atomic_fetch_add(&pool->failed_allocations, 1);
            if ((fails & (fails - 1)) == 0) {  /* Log at powers of 2 */
                LOG_WARN("Metadata pool at maximum capacity (%u), %lu failed allocations",
                         pool->pool_size, (unsigned long)(fails + 1));
            }
            return SPARSE_SYNAPSE_NO_METADATA;
        }

        // Grow blocks pointer array if needed (safe — no one holds ptrs into it)
        if (pool->num_blocks >= pool->max_blocks) {
            uint32_t new_max = pool->max_blocks * 2;
            synapse_t** new_blocks = (synapse_t**)nimcp_realloc(
                pool->blocks, new_max * sizeof(synapse_t*));
            if (new_blocks == NULL) {
                if (pool->config.thread_safe) {
                    nimcp_mutex_unlock(&pool->mutex);
                }
                atomic_fetch_add(&pool->failed_allocations, 1);
                LOG_WARN("Metadata pool grow failed (blocks array OOM)");
                return SPARSE_SYNAPSE_NO_METADATA;
            }
            memset(&new_blocks[pool->max_blocks], 0,
                   (new_max - pool->max_blocks) * sizeof(synapse_t*));
            pool->blocks = new_blocks;
            pool->max_blocks = new_max;
        }

        // Allocate new block (never moved after creation — pointers stay stable)
        synapse_t* new_block = (synapse_t*)nimcp_calloc(
            SYNAPSE_METADATA_BLOCK_SIZE, sizeof(synapse_t));
        if (new_block == NULL) {
            if (pool->config.thread_safe) {
                nimcp_mutex_unlock(&pool->mutex);
            }
            atomic_fetch_add(&pool->failed_allocations, 1);
            LOG_WARN("Metadata pool grow failed (block alloc OOM at %u slots)",
                     pool->pool_size);
            return SPARSE_SYNAPSE_NO_METADATA;
        }
        pool->blocks[pool->num_blocks] = new_block;
        pool->num_blocks++;

        // Grow free list (safe — no one holds ptrs into it, only uint32 indices)
        uint32_t new_fl_cap = pool->free_list_capacity + SYNAPSE_METADATA_BLOCK_SIZE;
        uint32_t* new_fl = (uint32_t*)nimcp_realloc(
            pool->free_list, new_fl_cap * sizeof(uint32_t));
        if (new_fl == NULL) {
            // Undo the block allocation
            nimcp_free(new_block);
            pool->blocks[--pool->num_blocks] = NULL;
            if (pool->config.thread_safe) {
                nimcp_mutex_unlock(&pool->mutex);
            }
            atomic_fetch_add(&pool->failed_allocations, 1);
            LOG_WARN("Metadata pool grow failed (free list OOM)");
            return SPARSE_SYNAPSE_NO_METADATA;
        }
        pool->free_list = new_fl;
        pool->free_list_capacity = new_fl_cap;

        // Add new block's indices to free list (reverse order for LIFO)
        uint32_t old_size = pool->pool_size;
        for (uint32_t i = 0; i < SYNAPSE_METADATA_BLOCK_SIZE; i++) {
            pool->free_list[i] = old_size + SYNAPSE_METADATA_BLOCK_SIZE - 1 - i;
        }
        pool->free_count = SYNAPSE_METADATA_BLOCK_SIZE;
        pool->pool_size = new_pool_size;
        pool->config.pool_size = new_pool_size;

        LOG_INFO("Metadata pool grew: added block %u (%u total slots, %.2f MB total)",
                 pool->num_blocks - 1, pool->pool_size,
                 (pool->pool_size * sizeof(synapse_t)) / (1024.0 * 1024.0));
    }

    // Pop from free list
    uint32_t index = pool->free_list[--pool->free_count];

    // Update peak usage
    if (pool->config.enable_statistics) {
        atomic_fetch_add(&pool->total_allocations, 1);
        size_t current_used = pool->pool_size - pool->free_count;
        size_t peak = atomic_load(&pool->peak_usage);
        if (current_used > peak) {
            atomic_store(&pool->peak_usage, current_used);
        }
    }

    if (pool->config.thread_safe) {
        nimcp_mutex_unlock(&pool->mutex);
    }

    // Initialize synapse to defaults (chunked access)
    uint32_t blk = index >> SYNAPSE_METADATA_BLOCK_SHIFT;
    uint32_t off = index & SYNAPSE_METADATA_BLOCK_MASK;
    memset(&pool->blocks[blk][off], 0, sizeof(synapse_t));
    pool->blocks[blk][off].cold_index = SYNAPSE_COLD_NONE;  // No cold data by default

    LOG_TRACE("Allocated metadata slot %u (%u remaining)", index, pool->free_count);
    return index;
}

void synapse_metadata_pool_free(synapse_metadata_pool_t pool, uint32_t index) {
    if (!validate_metadata_pool(pool)) {
        return;
    }

    // Validate index
    if (index == SPARSE_SYNAPSE_NO_METADATA || index >= pool->pool_size) {
        LOG_WARN("Invalid metadata index for free: %u", index);
        return;
    }

    if (pool->config.thread_safe) {
        nimcp_mutex_lock(&pool->mutex);
    }

    // Push to free list
    pool->free_list[pool->free_count++] = index;

    if (pool->config.enable_statistics) {
        atomic_fetch_add(&pool->total_deallocations, 1);
    }

    if (pool->config.thread_safe) {
        nimcp_mutex_unlock(&pool->mutex);
    }

    LOG_DEBUG("Freed metadata slot %u", index);
}

synapse_t* synapse_metadata_pool_get(
    synapse_metadata_pool_t pool,
    uint32_t index
) {
    if (!validate_metadata_pool(pool)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "synapse_metadata_pool_get: validate_metadata_pool is NULL");
        return NULL;
    }

    // Handle no-metadata sentinel — normal for synapses created after pool exhaustion
    if (index == SPARSE_SYNAPSE_NO_METADATA) {
        return NULL;
    }

    // Validate index
    if (index >= pool->pool_size) {
        LOG_WARN("Invalid metadata index: %u", index);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "synapse_metadata_pool_get: capacity exceeded");
        return NULL;
    }

    // Chunked access — block never moves, returned pointer stays stable
    uint32_t blk = index >> SYNAPSE_METADATA_BLOCK_SHIFT;
    uint32_t off = index & SYNAPSE_METADATA_BLOCK_MASK;
    return &pool->blocks[blk][off];
}

float synapse_metadata_pool_utilization(synapse_metadata_pool_t pool) {
    if (!validate_metadata_pool(pool)) {
        return 0.0F;
    }

    if (pool->pool_size == 0) {
        return 0.0F;
    }

    return (float)(pool->pool_size - pool->free_count) / (float)pool->pool_size;
}

size_t synapse_metadata_pool_available(synapse_metadata_pool_t pool) {
    if (!validate_metadata_pool(pool)) {
        return 0;
    }

    return pool->free_count;
}

//=============================================================================
// Synapse Cold Pool Implementation (NIMCP 2.11: Hot/cold split)
//=============================================================================

#define SYNAPSE_COLD_POOL_MAGIC 0xC01DC01D
#define SYNAPSE_COLD_BLOCK_SHIFT 14
#define SYNAPSE_COLD_BLOCK_SIZE  (1u << SYNAPSE_COLD_BLOCK_SHIFT)  // 16384
#define SYNAPSE_COLD_BLOCK_MASK  (SYNAPSE_COLD_BLOCK_SIZE - 1)
#define SYNAPSE_COLD_INITIAL_MAX_BLOCKS 16
#define SYNAPSE_COLD_MAX_POOL_SIZE (64u * 1024u * 1024u)  // 64M max cold slots

typedef struct synapse_cold_pool_struct {
    uint32_t magic;
    synapse_cold_pool_config_t config;
    synapse_cold_t** blocks;
    uint32_t num_blocks;
    uint32_t max_blocks;
    uint32_t* free_list;
    uint32_t free_count;
    uint32_t free_list_capacity;
    uint32_t pool_size;
    _Atomic uint64_t total_allocations;
    _Atomic uint64_t total_deallocations;
    nimcp_mutex_t mutex;
} synapse_cold_pool_struct_t;

static bool validate_cold_pool(synapse_cold_pool_t pool) {
    return pool != NULL && pool->magic == SYNAPSE_COLD_POOL_MAGIC;
}

synapse_cold_pool_config_t synapse_cold_pool_default_config(void) {
    return (synapse_cold_pool_config_t){
        .pool_size = SYNAPSE_COLD_BLOCK_SIZE,  // Start small, grow on demand
        .enable_statistics = true,
        .thread_safe = true
    };
}

synapse_cold_pool_t synapse_cold_pool_create(
    const synapse_cold_pool_config_t* config
) {
    synapse_cold_pool_config_t cfg;
    if (config != NULL) {
        cfg = *config;
    } else {
        cfg = synapse_cold_pool_default_config();
    }
    if (cfg.pool_size == 0) {
        cfg.pool_size = SYNAPSE_COLD_BLOCK_SIZE;
    }

    synapse_cold_pool_struct_t* pool = (synapse_cold_pool_struct_t*)
        nimcp_calloc(1, sizeof(synapse_cold_pool_struct_t));
    if (!pool) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "synapse_cold_pool_create: alloc failed");
        return NULL;
    }
    pool->magic = SYNAPSE_COLD_POOL_MAGIC;
    pool->config = cfg;

    uint32_t num_blocks = ((uint32_t)cfg.pool_size + SYNAPSE_COLD_BLOCK_SIZE - 1)
                          >> SYNAPSE_COLD_BLOCK_SHIFT;
    uint32_t actual_pool_size = num_blocks * SYNAPSE_COLD_BLOCK_SIZE;

    pool->max_blocks = (num_blocks < SYNAPSE_COLD_INITIAL_MAX_BLOCKS)
                     ? SYNAPSE_COLD_INITIAL_MAX_BLOCKS : num_blocks * 2;
    pool->blocks = (synapse_cold_t**)nimcp_calloc(pool->max_blocks, sizeof(synapse_cold_t*));
    if (!pool->blocks) {
        nimcp_free(pool);
        return NULL;
    }

    for (uint32_t b = 0; b < num_blocks; b++) {
        pool->blocks[b] = (synapse_cold_t*)nimcp_calloc(
            SYNAPSE_COLD_BLOCK_SIZE, sizeof(synapse_cold_t));
        if (!pool->blocks[b]) {
            for (uint32_t k = 0; k < b; k++) nimcp_free(pool->blocks[k]);
            nimcp_free(pool->blocks);
            nimcp_free(pool);
            return NULL;
        }
    }
    pool->num_blocks = num_blocks;
    pool->pool_size = actual_pool_size;

    pool->free_list_capacity = actual_pool_size;
    pool->free_list = (uint32_t*)nimcp_calloc(actual_pool_size, sizeof(uint32_t));
    if (!pool->free_list) {
        for (uint32_t b = 0; b < num_blocks; b++) nimcp_free(pool->blocks[b]);
        nimcp_free(pool->blocks);
        nimcp_free(pool);
        return NULL;
    }
    for (uint32_t i = 0; i < actual_pool_size; i++) {
        pool->free_list[i] = actual_pool_size - 1 - i;
    }
    pool->free_count = actual_pool_size;

    if (cfg.thread_safe) {
        nimcp_mutex_init(&pool->mutex, NULL);
    }

    LOG_INFO("Created synapse cold pool with %u slots in %u blocks (%.2f MB total)",
             pool->pool_size, pool->num_blocks,
             (pool->pool_size * sizeof(synapse_cold_t)) / (1024.0 * 1024.0));
    return pool;
}

void synapse_cold_pool_destroy(synapse_cold_pool_t pool) {
    if (!validate_cold_pool(pool)) return;
    if (pool->config.thread_safe) nimcp_mutex_destroy(&pool->mutex);
    if (pool->blocks) {
        for (uint32_t b = 0; b < pool->num_blocks; b++) {
            if (pool->blocks[b]) nimcp_free(pool->blocks[b]);
        }
        nimcp_free(pool->blocks);
    }
    if (pool->free_list) nimcp_free(pool->free_list);
    pool->magic = 0;
    nimcp_free(pool);
}

uint32_t synapse_cold_pool_allocate(synapse_cold_pool_t pool) {
    if (!validate_cold_pool(pool)) return SYNAPSE_COLD_NONE;

    if (pool->config.thread_safe) nimcp_mutex_lock(&pool->mutex);

    if (pool->free_count == 0) {
        uint32_t new_pool_size = pool->pool_size + SYNAPSE_COLD_BLOCK_SIZE;
        if (new_pool_size > SYNAPSE_COLD_MAX_POOL_SIZE) {
            if (pool->config.thread_safe) nimcp_mutex_unlock(&pool->mutex);
            return SYNAPSE_COLD_NONE;
        }

        if (pool->num_blocks >= pool->max_blocks) {
            uint32_t new_max = pool->max_blocks * 2;
            synapse_cold_t** new_blocks = (synapse_cold_t**)nimcp_realloc(
                pool->blocks, new_max * sizeof(synapse_cold_t*));
            if (!new_blocks) {
                if (pool->config.thread_safe) nimcp_mutex_unlock(&pool->mutex);
                return SYNAPSE_COLD_NONE;
            }
            memset(&new_blocks[pool->max_blocks], 0,
                   (new_max - pool->max_blocks) * sizeof(synapse_cold_t*));
            pool->blocks = new_blocks;
            pool->max_blocks = new_max;
        }

        synapse_cold_t* new_block = (synapse_cold_t*)nimcp_calloc(
            SYNAPSE_COLD_BLOCK_SIZE, sizeof(synapse_cold_t));
        if (!new_block) {
            if (pool->config.thread_safe) nimcp_mutex_unlock(&pool->mutex);
            return SYNAPSE_COLD_NONE;
        }
        pool->blocks[pool->num_blocks++] = new_block;

        uint32_t new_fl_cap = pool->free_list_capacity + SYNAPSE_COLD_BLOCK_SIZE;
        uint32_t* new_fl = (uint32_t*)nimcp_realloc(
            pool->free_list, new_fl_cap * sizeof(uint32_t));
        if (!new_fl) {
            nimcp_free(new_block);
            pool->blocks[--pool->num_blocks] = NULL;
            if (pool->config.thread_safe) nimcp_mutex_unlock(&pool->mutex);
            return SYNAPSE_COLD_NONE;
        }
        pool->free_list = new_fl;
        pool->free_list_capacity = new_fl_cap;

        uint32_t old_size = pool->pool_size;
        for (uint32_t i = 0; i < SYNAPSE_COLD_BLOCK_SIZE; i++) {
            pool->free_list[i] = old_size + SYNAPSE_COLD_BLOCK_SIZE - 1 - i;
        }
        pool->free_count = SYNAPSE_COLD_BLOCK_SIZE;
        pool->pool_size = new_pool_size;
        pool->config.pool_size = new_pool_size;

        LOG_INFO("Cold pool grew: block %u (%u total slots, %.2f MB)",
                 pool->num_blocks - 1, pool->pool_size,
                 (pool->pool_size * sizeof(synapse_cold_t)) / (1024.0 * 1024.0));
    }

    uint32_t index = pool->free_list[--pool->free_count];
    if (pool->config.enable_statistics) {
        atomic_fetch_add(&pool->total_allocations, 1);
    }

    if (pool->config.thread_safe) nimcp_mutex_unlock(&pool->mutex);

    // Zero-init the cold struct
    uint32_t blk = index >> SYNAPSE_COLD_BLOCK_SHIFT;
    uint32_t off = index & SYNAPSE_COLD_BLOCK_MASK;
    memset(&pool->blocks[blk][off], 0, sizeof(synapse_cold_t));

    return index;
}

void synapse_cold_pool_free(synapse_cold_pool_t pool, uint32_t index) {
    if (!validate_cold_pool(pool)) return;
    if (index == SYNAPSE_COLD_NONE || index >= pool->pool_size) return;

    if (pool->config.thread_safe) nimcp_mutex_lock(&pool->mutex);
    pool->free_list[pool->free_count++] = index;
    if (pool->config.enable_statistics) {
        atomic_fetch_add(&pool->total_deallocations, 1);
    }
    if (pool->config.thread_safe) nimcp_mutex_unlock(&pool->mutex);
}

synapse_cold_t* synapse_cold_pool_get(
    synapse_cold_pool_t pool, uint32_t index
) {
    if (!validate_cold_pool(pool)) return NULL;
    if (index == SYNAPSE_COLD_NONE || index >= pool->pool_size) return NULL;
    uint32_t blk = index >> SYNAPSE_COLD_BLOCK_SHIFT;
    uint32_t off = index & SYNAPSE_COLD_BLOCK_MASK;
    return &pool->blocks[blk][off];
}

//=============================================================================
// Extended Synapse Handle API Implementation
//=============================================================================

int sparse_synapse_add_with_metadata(
    sparse_synapse_pool_t handle_pool,
    synapse_metadata_pool_t metadata_pool,
    sparse_synapse_storage_t* storage,
    uint32_t target_neuron_id,
    float weight,
    int synapse_type
) {
    // Validate inputs
    if (!validate_pool(handle_pool) || !validate_metadata_pool(metadata_pool) ||
        !validate_storage(storage)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sparse_synapse_add_with_metadata: required parameter is NULL (validate_pool, validate_metadata_pool)");
        return -1;
    }

    // Allocate metadata slot — pool exhaustion is OK, synapse will be handle-only
    uint32_t metadata_index = synapse_metadata_pool_allocate(metadata_pool);

    // Add handle with basic sparse_synapse_add
    if (sparse_synapse_add(handle_pool, storage, target_neuron_id, weight) != 0) {
        // Rollback metadata allocation if we got one
        if (metadata_index != SPARSE_SYNAPSE_NO_METADATA) {
            synapse_metadata_pool_free(metadata_pool, metadata_index);
        }
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sparse_synapse_add_with_metadata: validation failed");
        return -1;
    }

    // Get the handle we just added (it's the last one)
    uint32_t handle_index = sparse_synapse_count(storage) - 1;
    synapse_handle_t* handle = sparse_synapse_get(storage, handle_index);
    if (handle == NULL) {
        LOG_ERROR("Failed to get handle after add");
        if (metadata_index != SPARSE_SYNAPSE_NO_METADATA) {
            synapse_metadata_pool_free(metadata_pool, metadata_index);
        }
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sparse_synapse_add_with_metadata: validation failed");
        return -1;
    }

    // Link handle to metadata (may be NO_METADATA if pool exhausted)
    handle->metadata_index = metadata_index;

    // Initialize the hot synapse_t fields
    synapse_t* syn = synapse_metadata_pool_get(metadata_pool, metadata_index);
    if (syn != NULL) {
        syn->target_id = target_neuron_id;
        syn->weight = weight;
        syn->plasticity = 1.0F;
        syn->strength = 1.0F;
        syn->meta_plasticity = 1.0F;
        syn->trace = 0.0F;
        syn->last_change = 0.0F;
        syn->last_active = 0;
        syn->semantic_relevance = 0.5f;
        syn->cold_index = SYNAPSE_COLD_NONE;
        // Cold fields (type, type_state, embedding_pool_index, embedding_dim,
        // STP, BCM, eligibility, ternary) initialized via SYNAPSE_ENSURE_COLD()
        // by callers that have network access (e.g., add_typed_connection,
        // backbone wiring, checkpoint load).
    }

    LOG_TRACE("Added synapse with metadata: target=%u, weight=%f, type=%d, metadata_index=%u",
              target_neuron_id, weight, synapse_type, metadata_index);

    return 0;
}

synapse_t* sparse_synapse_get_metadata(
    synapse_metadata_pool_t metadata_pool,
    const synapse_handle_t* handle
) {
    if (handle == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "handle is NULL");

        return NULL;
    }

    return synapse_metadata_pool_get(metadata_pool, handle->metadata_index);
}

int sparse_synapse_remove_with_metadata(
    sparse_synapse_pool_t handle_pool,
    synapse_metadata_pool_t metadata_pool,
    sparse_synapse_storage_t* storage,
    uint32_t index
) {
    if (!validate_pool(handle_pool) || !validate_storage(storage)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sparse_synapse_remove_with_metadata: required parameter is NULL (validate_pool, validate_storage)");
        return -1;
    }

    // Get handle before removal to check for metadata
    // NOTE: sparse_synapse_get() returns NULL for out-of-bounds indices,
    // which happens legitimately during pruning when peer_index values
    // become stale. Do NOT throw to immune — callers handle -1 return.
    synapse_handle_t* handle = sparse_synapse_get(storage, index);
    if (handle == NULL) {
        return -1;
    }

    // Free metadata if present
    if (handle->metadata_index != SPARSE_SYNAPSE_NO_METADATA && metadata_pool != NULL) {
        synapse_metadata_pool_free(metadata_pool, handle->metadata_index);
    }

    // Remove handle
    return sparse_synapse_remove(handle_pool, storage, index);
}

//=============================================================================
// Ternary-Aware Sparse Synapse Operations (NIMCP 2.10)
//=============================================================================

/**
 * @brief Add synapse with ternary weight
 */
int sparse_synapse_add_ternary(
    sparse_synapse_pool_t pool,
    sparse_synapse_storage_t* storage,
    uint32_t target_neuron_id,
    trit_t ternary_weight
) {
    if (!validate_pool(pool) || !validate_storage(storage)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sparse_synapse_add_ternary: required parameter is NULL (validate_pool, validate_storage)");
        return -1;
    }

    // First add with zero float weight
    int result = sparse_synapse_add(pool, storage, target_neuron_id, 0.0f);
    if (result != 0) {
        return result;
    }

    // Get the newly added handle and set ternary fields
    uint32_t count = sparse_synapse_count(storage);
    if (count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sparse_synapse_add_ternary: count is zero");
        return -1;
    }

    synapse_handle_t* handle = sparse_synapse_get(storage, count - 1);
    if (handle == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sparse_synapse_add_ternary: validation failed");
        return -1;
    }

    handle->ternary_weight = ternary_weight;
    handle->use_ternary_weight = 1;

    return 0;
}

/**
 * @brief Get effective weight from handle (ternary or float)
 */
float sparse_synapse_get_effective_weight(
    const synapse_handle_t* handle,
    float positive_scale,
    float negative_scale
) {
    if (handle == NULL) {
        return 0.0f;
    }

    if (handle->use_ternary_weight) {
        if (handle->ternary_weight == TRIT_POSITIVE) {
            return positive_scale;
        } else if (handle->ternary_weight == TRIT_NEGATIVE) {
            return negative_scale;
        }
        return 0.0f;
    }

    return handle->weight;
}

/**
 * @brief Set weight with automatic ternary quantization
 */
void sparse_synapse_set_weight(
    synapse_handle_t* handle,
    float weight,
    float threshold,
    bool enable_ternary
) {
    if (handle == NULL) {
        return;
    }

    if (enable_ternary) {
        if (weight >= threshold) {
            handle->ternary_weight = TRIT_POSITIVE;
        } else if (weight <= -threshold) {
            handle->ternary_weight = TRIT_NEGATIVE;
        } else {
            handle->ternary_weight = TRIT_UNKNOWN;
        }
        handle->use_ternary_weight = 1;
    } else {
        handle->weight = weight;
        handle->use_ternary_weight = 0;
    }
}

/**
 * @brief Convert storage to ternary weights
 */
uint32_t sparse_synapse_convert_to_ternary(
    sparse_synapse_storage_t* storage,
    float threshold
) {
    if (!validate_storage(storage)) {
        return 0;
    }

    uint32_t converted = 0;
    uint32_t total = storage->embedded_count + storage->overflow_count;

    // Process embedded synapses
    for (uint32_t i = 0; i < storage->embedded_count; i++) {
        synapse_handle_t* handle = &storage->embedded[i];

        // Quantize float weight to ternary
        if (handle->weight >= threshold) {
            handle->ternary_weight = TRIT_POSITIVE;
        } else if (handle->weight <= -threshold) {
            handle->ternary_weight = TRIT_NEGATIVE;
        } else {
            handle->ternary_weight = TRIT_UNKNOWN;
        }
        handle->use_ternary_weight = 1;
        converted++;
    }

    // Process overflow synapses
    for (uint32_t i = 0; i < storage->overflow_count; i++) {
        synapse_handle_t* handle = &storage->overflow[i];

        if (handle->weight >= threshold) {
            handle->ternary_weight = TRIT_POSITIVE;
        } else if (handle->weight <= -threshold) {
            handle->ternary_weight = TRIT_NEGATIVE;
        } else {
            handle->ternary_weight = TRIT_UNKNOWN;
        }
        handle->use_ternary_weight = 1;
        converted++;
    }

    return converted;
}

/**
 * @brief Convert storage back to float weights
 */
uint32_t sparse_synapse_convert_to_float(
    sparse_synapse_storage_t* storage,
    float positive_scale,
    float negative_scale
) {
    if (!validate_storage(storage)) {
        return 0;
    }

    uint32_t converted = 0;

    // Process embedded synapses
    for (uint32_t i = 0; i < storage->embedded_count; i++) {
        synapse_handle_t* handle = &storage->embedded[i];

        if (handle->use_ternary_weight) {
            if (handle->ternary_weight == TRIT_POSITIVE) {
                handle->weight = positive_scale;
            } else if (handle->ternary_weight == TRIT_NEGATIVE) {
                handle->weight = negative_scale;
            } else {
                handle->weight = 0.0f;
            }
            handle->use_ternary_weight = 0;
            converted++;
        }
    }

    // Process overflow synapses
    for (uint32_t i = 0; i < storage->overflow_count; i++) {
        synapse_handle_t* handle = &storage->overflow[i];

        if (handle->use_ternary_weight) {
            if (handle->ternary_weight == TRIT_POSITIVE) {
                handle->weight = positive_scale;
            } else if (handle->ternary_weight == TRIT_NEGATIVE) {
                handle->weight = negative_scale;
            } else {
                handle->weight = 0.0f;
            }
            handle->use_ternary_weight = 0;
            converted++;
        }
    }

    return converted;
}

/**
 * @brief Get ternary weight statistics for storage
 */
void sparse_synapse_ternary_stats(
    const sparse_synapse_storage_t* storage,
    uint32_t* n_positive,
    uint32_t* n_zero,
    uint32_t* n_negative,
    uint32_t* n_ternary_enabled
) {
    uint32_t pos = 0, zero = 0, neg = 0, ternary = 0;

    if (validate_storage(storage)) {
        // Process embedded synapses
        for (uint32_t i = 0; i < storage->embedded_count; i++) {
            const synapse_handle_t* handle = &storage->embedded[i];
            if (handle->use_ternary_weight) {
                ternary++;
                if (handle->ternary_weight == TRIT_POSITIVE) pos++;
                else if (handle->ternary_weight == TRIT_NEGATIVE) neg++;
                else zero++;
            }
        }

        // Process overflow synapses
        for (uint32_t i = 0; i < storage->overflow_count; i++) {
            const synapse_handle_t* handle = &storage->overflow[i];
            if (handle->use_ternary_weight) {
                ternary++;
                if (handle->ternary_weight == TRIT_POSITIVE) pos++;
                else if (handle->ternary_weight == TRIT_NEGATIVE) neg++;
                else zero++;
            }
        }
    }

    if (n_positive) *n_positive = pos;
    if (n_zero) *n_zero = zero;
    if (n_negative) *n_negative = neg;
    if (n_ternary_enabled) *n_ternary_enabled = ternary;
}

/**
 * @brief Ternary-aware dot product for sparse synapses
 */
float sparse_synapse_ternary_dot(
    const sparse_synapse_storage_t* storage,
    const float* inputs,
    float positive_scale,
    float negative_scale
) {
    if (!validate_storage(storage) || inputs == NULL) {
        return 0.0f;
    }

    float sum = 0.0f;

    // Process embedded synapses
    for (uint32_t i = 0; i < storage->embedded_count; i++) {
        const synapse_handle_t* handle = &storage->embedded[i];
        float input_val = inputs[handle->target_neuron_id];

        if (handle->use_ternary_weight) {
            if (handle->ternary_weight == TRIT_POSITIVE) {
                sum += input_val * positive_scale;
            } else if (handle->ternary_weight == TRIT_NEGATIVE) {
                sum += input_val * negative_scale;
            }
            // TRIT_UNKNOWN contributes 0
        } else {
            sum += input_val * handle->weight;
        }
    }

    // Process overflow synapses
    for (uint32_t i = 0; i < storage->overflow_count; i++) {
        const synapse_handle_t* handle = &storage->overflow[i];
        float input_val = inputs[handle->target_neuron_id];

        if (handle->use_ternary_weight) {
            if (handle->ternary_weight == TRIT_POSITIVE) {
                sum += input_val * positive_scale;
            } else if (handle->ternary_weight == TRIT_NEGATIVE) {
                sum += input_val * negative_scale;
            }
        } else {
            sum += input_val * handle->weight;
        }
    }

    return sum;
}
