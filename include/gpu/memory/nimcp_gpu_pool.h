/**
 * @file nimcp_gpu_pool.h
 * @brief GPU Memory Pool with Object-Oriented Design Patterns
 *
 * WHAT: Unified GPU memory pool system with pluggable allocator strategies
 * WHY:  Eliminate cudaMalloc overhead, enable stream-ordered allocations,
 *       reduce fragmentation, and provide predictable memory behavior
 * HOW:  Strategy pattern for allocators, singleton for global pool, observer
 *       pattern for usage statistics
 *
 * ARCHITECTURE:
 *
 *   +------------------------------------------------------------------+
 *   |                    GPU MEMORY POOL SYSTEM                        |
 *   |                                                                  |
 *   |  +---------------------+   +-------------------------+           |
 *   |  | Pool Manager        |   | Memory Block Tracker    |           |
 *   |  | (Singleton Pattern) |   | (Observer Pattern)      |           |
 *   |  +---------------------+   +-------------------------+           |
 *   |           |                          |                           |
 *   |           v                          v                           |
 *   |  +------------------------------------------------+              |
 *   |  |          Allocator Interface                   |              |
 *   |  |          (Strategy Pattern)                    |              |
 *   |  +------------------------------------------------+              |
 *   |           |              |              |                        |
 *   |           v              v              v                        |
 *   |  +-------------+  +-------------+  +-------------+              |
 *   |  |   Bump      |  |   Buddy     |  |   Slab      |              |
 *   |  | Allocator   |  | Allocator   |  | Allocator   |              |
 *   |  | (Scratch)   |  | (Persist)   |  | (Activation)|              |
 *   |  +-------------+  +-------------+  +-------------+              |
 *   +------------------------------------------------------------------+
 *
 * ALLOCATOR STRATEGIES:
 *
 * 1. BUMP ALLOCATOR (POOL_TYPE_SCRATCH):
 *    - O(1) allocation, O(1) reset
 *    - No individual frees, reset all at once
 *    - Ideal for per-iteration temporary tensors
 *    - Maximum speed, zero fragmentation
 *
 * 2. BUDDY ALLOCATOR (POOL_TYPE_PERSISTENT):
 *    - O(log n) allocation and free
 *    - Power-of-2 block sizes
 *    - Good for varied-size allocations (weights, gradients)
 *    - Bounded fragmentation
 *
 * 3. SLAB ALLOCATOR (POOL_TYPE_ACTIVATION):
 *    - O(1) allocation and free
 *    - Fixed-size pools for common tensor sizes
 *    - Ideal for activation tensors with known sizes
 *    - Zero external fragmentation
 *
 * MEMORY ALIGNMENT:
 *    - 256-byte alignment for optimal coalescing
 *    - Supports custom alignment per allocation
 *
 * STREAM ORDERING:
 *    - Allocations can be associated with CUDA streams
 *    - Enables safe async memory reuse
 *
 * THREAD SAFETY:
 *    - All operations are thread-safe via mutex
 *    - Lock-free ownership checks
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_GPU_POOL_H
#define NIMCP_GPU_POOL_H

#include "common/nimcp_export.h"
#include "gpu/context/nimcp_gpu_context.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants and Configuration
//=============================================================================

#define NIMCP_GPU_POOL_MAGIC         0x47505530  /**< 'GPU0' magic number */
#define NIMCP_GPU_POOL_DEFAULT_ALIGN 256         /**< 256-byte alignment for coalescing */
#define NIMCP_GPU_POOL_MIN_BLOCK     256         /**< Minimum block size */
#define NIMCP_GPU_POOL_MAX_SLABS     32          /**< Maximum slab size classes */
#define NIMCP_GPU_POOL_BUDDY_LEVELS  24          /**< Max buddy tree levels (16MB max block) */

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct nimcp_gpu_pool_s nimcp_gpu_pool_t;
typedef struct nimcp_gpu_allocator_s nimcp_gpu_allocator_t;
typedef struct nimcp_gpu_block_s nimcp_gpu_block_t;

//=============================================================================
// Allocator Interface (Strategy Pattern)
//=============================================================================

/**
 * @brief Allocator operations interface (vtable)
 *
 * WHAT: Abstract interface for memory allocation strategies
 * WHY:  Enables pluggable allocator implementations
 * HOW:  Function pointers for polymorphic dispatch
 *
 * DESIGN PATTERN: Strategy Pattern
 * - Client (pool) delegates allocation to strategy (allocator)
 * - Strategies can be swapped at runtime
 */
typedef struct nimcp_gpu_allocator_ops_s {
    /**
     * @brief Allocate memory with alignment
     *
     * @param self Allocator instance
     * @param size Requested size in bytes
     * @param alignment Alignment requirement (power of 2)
     * @param stream CUDA stream for stream-ordered allocation (can be NULL)
     * @return Device pointer or NULL on failure
     */
    void* (*alloc)(void* self, size_t size, size_t alignment, void* stream);

    /**
     * @brief Free memory
     *
     * @param self Allocator instance
     * @param ptr Device pointer to free
     */
    void (*free)(void* self, void* ptr);

    /**
     * @brief Reset allocator (free all allocations)
     *
     * @param self Allocator instance
     * @return Number of blocks freed
     */
    size_t (*reset)(void* self);

    /**
     * @brief Get used memory in bytes
     *
     * @param self Allocator instance
     * @return Used memory in bytes
     */
    size_t (*get_used)(void* self);

    /**
     * @brief Get available memory in bytes
     *
     * @param self Allocator instance
     * @return Available memory in bytes
     */
    size_t (*get_available)(void* self);

    /**
     * @brief Check if allocator owns pointer
     *
     * @param self Allocator instance
     * @param ptr Pointer to check
     * @return true if ptr belongs to this allocator
     */
    bool (*owns)(void* self, const void* ptr);

    /**
     * @brief Defragment memory (if supported)
     *
     * @param self Allocator instance
     * @param stream CUDA stream for async operations
     * @return Bytes reclaimed
     */
    size_t (*defragment)(void* self, void* stream);

    /**
     * @brief Destroy allocator and free resources
     *
     * @param self Allocator instance
     */
    void (*destroy)(void* self);
} nimcp_gpu_allocator_ops_t;

/**
 * @brief Base allocator structure (inherited by concrete allocators)
 */
struct nimcp_gpu_allocator_s {
    const nimcp_gpu_allocator_ops_t* ops;   /**< Virtual function table */
    nimcp_gpu_context_t* ctx;               /**< GPU context */
    void* base_ptr;                         /**< Base device memory pointer */
    size_t total_size;                      /**< Total pool size */
    uint32_t magic;                         /**< Magic number for validation */
};

//=============================================================================
// Pool Configuration
//=============================================================================

/**
 * @brief Pool type enumeration
 *
 * DESIGN: Different pool types use different allocator strategies
 */
typedef enum nimcp_gpu_pool_type_e {
    NIMCP_GPU_POOL_TYPE_SCRATCH = 0,    /**< Bump allocator for temporary tensors */
    NIMCP_GPU_POOL_TYPE_PERSISTENT,     /**< Buddy allocator for weights */
    NIMCP_GPU_POOL_TYPE_ACTIVATION,     /**< Slab allocator for fixed-size activations */
    NIMCP_GPU_POOL_TYPE_CUSTOM,         /**< User-provided allocator */
    NIMCP_GPU_POOL_TYPE_COUNT
} nimcp_gpu_pool_type_t;

/**
 * @brief Slab size class configuration
 */
typedef struct nimcp_gpu_slab_config_s {
    size_t block_size;      /**< Size of blocks in this slab */
    size_t num_blocks;      /**< Number of blocks to pre-allocate */
} nimcp_gpu_slab_config_t;

/**
 * @brief Pool configuration structure
 *
 * WHAT: Configuration for creating GPU memory pools
 * WHY:  Flexible setup for different usage patterns
 */
typedef struct nimcp_gpu_pool_config_s {
    size_t initial_size;            /**< Initial pool size in bytes (e.g., 15GB) */
    size_t max_size;                /**< Maximum pool size in bytes (e.g., 18GB) */
    size_t block_size;              /**< Minimum block size (default: 256) */
    size_t alignment;               /**< Memory alignment (default: 256) */
    bool enable_defrag;             /**< Enable defragmentation support */
    bool enable_stats;              /**< Track allocation statistics */
    float growth_factor;            /**< Pool growth factor when exhausted (e.g., 1.5) */
    nimcp_gpu_pool_type_t type;     /**< Pool type (determines allocator) */

    // Slab allocator specific
    nimcp_gpu_slab_config_t slab_configs[NIMCP_GPU_POOL_MAX_SLABS];
    size_t num_slab_configs;        /**< Number of slab size classes */

    // Buddy allocator specific
    size_t buddy_min_order;         /**< Minimum block order (2^order bytes) */
    size_t buddy_max_order;         /**< Maximum block order */
} nimcp_gpu_pool_config_t;

//=============================================================================
// Memory Block Tracking (Observer Pattern)
//=============================================================================

/**
 * @brief Memory block state
 */
typedef enum nimcp_gpu_block_state_e {
    NIMCP_GPU_BLOCK_FREE = 0,       /**< Block is available */
    NIMCP_GPU_BLOCK_ALLOCATED,      /**< Block is in use */
    NIMCP_GPU_BLOCK_PENDING,        /**< Waiting for stream sync */
} nimcp_gpu_block_state_t;

/**
 * @brief Memory block metadata
 *
 * WHAT: Tracks individual memory allocations
 * WHY:  Enables debugging, leak detection, and usage analysis
 */
struct nimcp_gpu_block_s {
    void* ptr;                      /**< Device pointer */
    size_t size;                    /**< Allocated size */
    size_t offset;                  /**< Offset from pool base */
    nimcp_gpu_block_state_t state;  /**< Current state */
    void* stream;                   /**< Associated CUDA stream */
    uint64_t alloc_time;            /**< Allocation timestamp (for LRU) */
    uint64_t alloc_id;              /**< Unique allocation ID */
    const char* tag;                /**< Optional debug tag */
    struct nimcp_gpu_block_s* next; /**< Next block in list */
    struct nimcp_gpu_block_s* prev; /**< Previous block in list */
};

/**
 * @brief Block observer callback type
 *
 * DESIGN PATTERN: Observer Pattern
 * - Observers are notified of allocation/free events
 * - Enables external monitoring and logging
 */
typedef void (*nimcp_gpu_block_observer_fn)(
    const nimcp_gpu_block_t* block,
    bool is_allocation,
    void* user_data
);

/**
 * @brief Block observer registration
 */
typedef struct nimcp_gpu_block_observer_s {
    nimcp_gpu_block_observer_fn callback;
    void* user_data;
    struct nimcp_gpu_block_observer_s* next;
} nimcp_gpu_block_observer_t;

//=============================================================================
// Pool Statistics
//=============================================================================

/**
 * @brief Memory pool statistics
 */
typedef struct nimcp_gpu_pool_stats_s {
    // Memory metrics
    size_t total_size;              /**< Total pool size */
    size_t used_size;               /**< Currently used size */
    size_t peak_used;               /**< Peak memory usage */
    size_t available_size;          /**< Available for allocation */
    size_t fragmentation_bytes;     /**< Internal fragmentation */
    float fragmentation_ratio;      /**< Fragmentation as percentage */

    // Allocation metrics
    uint64_t allocation_count;      /**< Total allocations made */
    uint64_t free_count;            /**< Total frees made */
    uint64_t failed_allocations;    /**< Failed allocation attempts */
    uint64_t current_allocations;   /**< Active allocations */
    uint64_t peak_allocations;      /**< Peak concurrent allocations */

    // Performance metrics
    uint64_t total_alloc_time_ns;   /**< Total time in allocations */
    uint64_t total_free_time_ns;    /**< Total time in frees */
    uint64_t defrag_count;          /**< Number of defragmentations */
    uint64_t defrag_bytes_reclaimed;/**< Bytes recovered by defrag */

    // Pool-specific
    size_t num_blocks;              /**< Number of tracked blocks */
    size_t largest_free_block;      /**< Largest contiguous free block */
} nimcp_gpu_pool_stats_t;

//=============================================================================
// Main Pool Structure
//=============================================================================

/**
 * @brief GPU Memory Pool
 *
 * WHAT: Manages GPU memory with pluggable allocation strategies
 * WHY:  Reduces allocation overhead, enables memory reuse
 * HOW:  Delegates to allocator strategy, tracks blocks, notifies observers
 *
 * DESIGN PATTERNS:
 * - Strategy: Pluggable allocator implementations
 * - Observer: Block event notifications
 * - Facade: Simple interface to complex subsystems
 */
struct nimcp_gpu_pool_s {
    uint32_t magic;                         /**< Magic number for validation */
    nimcp_gpu_allocator_t* allocator;       /**< Active allocator strategy */
    nimcp_gpu_pool_config_t config;         /**< Pool configuration */
    nimcp_gpu_context_t* ctx;               /**< GPU context */

    // Block tracking
    nimcp_gpu_block_t* blocks_head;         /**< Head of allocated blocks list */
    nimcp_gpu_block_t* blocks_tail;         /**< Tail of allocated blocks list */
    nimcp_gpu_block_t* free_blocks;         /**< Pool of free block structs */
    size_t num_blocks;                      /**< Number of tracked blocks */

    // Observer list
    nimcp_gpu_block_observer_t* observers;  /**< Registered observers */

    // Statistics
    nimcp_gpu_pool_stats_t stats;           /**< Runtime statistics */

    // Thread safety
    void* mutex;                            /**< Mutex for thread safety */

    // State
    bool initialized;                       /**< Pool initialized flag */
    uint64_t next_alloc_id;                 /**< Next allocation ID */
};

//=============================================================================
// Pool Lifecycle API
//=============================================================================

/**
 * @brief Get default pool configuration
 *
 * @return Default configuration suitable for most use cases
 */
NIMCP_EXPORT nimcp_gpu_pool_config_t nimcp_gpu_pool_config_default(void);

/**
 * @brief Get configuration for scratch pool (bump allocator)
 *
 * @param size_bytes Pool size in bytes
 * @return Configuration for scratch memory
 */
NIMCP_EXPORT nimcp_gpu_pool_config_t nimcp_gpu_pool_config_scratch(size_t size_bytes);

/**
 * @brief Get configuration for persistent pool (buddy allocator)
 *
 * @param size_bytes Pool size in bytes
 * @return Configuration for persistent memory
 */
NIMCP_EXPORT nimcp_gpu_pool_config_t nimcp_gpu_pool_config_persistent(size_t size_bytes);

/**
 * @brief Get configuration for activation pool (slab allocator)
 *
 * @param size_bytes Pool size in bytes
 * @param slab_sizes Array of slab sizes to use
 * @param num_slabs Number of slab sizes
 * @return Configuration for activation memory
 */
NIMCP_EXPORT nimcp_gpu_pool_config_t nimcp_gpu_pool_config_activation(
    size_t size_bytes,
    const size_t* slab_sizes,
    size_t num_slabs
);

/**
 * @brief Create GPU memory pool
 *
 * WHAT: Allocates and initializes GPU memory pool
 * WHY:  Centralized memory management for GPU allocations
 * HOW:  Allocates device memory, initializes allocator strategy
 *
 * @param ctx GPU context (required)
 * @param config Pool configuration (NULL for defaults)
 * @return Pool handle or NULL on failure
 *
 * COMPLEXITY: O(1) for bump/buddy, O(k) for slab where k = num slab classes
 * THREAD SAFETY: NOT thread-safe (create from single thread)
 *
 * EXAMPLE:
 *   nimcp_gpu_pool_config_t config = nimcp_gpu_pool_config_default();
 *   config.initial_size = 15ULL * 1024 * 1024 * 1024;  // 15GB
 *   nimcp_gpu_pool_t* pool = nimcp_gpu_pool_create(ctx, &config);
 */
NIMCP_EXPORT nimcp_gpu_pool_t* nimcp_gpu_pool_create(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_pool_config_t* config
);

/**
 * @brief Destroy GPU memory pool
 *
 * @param pool Pool to destroy (can be NULL)
 *
 * WARNING: All allocations should be freed before destruction
 */
NIMCP_EXPORT void nimcp_gpu_pool_destroy(nimcp_gpu_pool_t* pool);

/**
 * @brief Check if pool is valid
 *
 * @param pool Pool to check
 * @return true if pool is valid and initialized
 */
NIMCP_EXPORT bool nimcp_gpu_pool_is_valid(const nimcp_gpu_pool_t* pool);

//=============================================================================
// Allocation API
//=============================================================================

/**
 * @brief Allocate memory from pool
 *
 * WHAT: Fast allocation from pre-allocated pool
 * WHY:  Avoid cudaMalloc overhead
 * HOW:  Delegates to allocator strategy
 *
 * @param pool Pool handle
 * @param size Size in bytes
 * @param alignment Alignment (0 for default 256-byte)
 * @param stream CUDA stream for stream-ordering (NULL for default)
 * @return Device pointer or NULL on failure
 *
 * COMPLEXITY: O(1) for bump/slab, O(log n) for buddy
 * THREAD SAFETY: Thread-safe (mutex protected)
 *
 * EXAMPLE:
 *   void* ptr = nimcp_gpu_pool_alloc(pool, 1024 * 1024, 256, stream);
 *   if (ptr) {
 *       // Use memory
 *       nimcp_gpu_pool_free(pool, ptr);
 *   }
 */
NIMCP_EXPORT void* nimcp_gpu_pool_alloc(
    nimcp_gpu_pool_t* pool,
    size_t size,
    size_t alignment,
    void* stream
);

/**
 * @brief Allocate memory with debug tag
 *
 * @param pool Pool handle
 * @param size Size in bytes
 * @param alignment Alignment
 * @param stream CUDA stream
 * @param tag Debug tag for tracking
 * @return Device pointer or NULL on failure
 */
NIMCP_EXPORT void* nimcp_gpu_pool_alloc_tagged(
    nimcp_gpu_pool_t* pool,
    size_t size,
    size_t alignment,
    void* stream,
    const char* tag
);

/**
 * @brief Free memory back to pool
 *
 * @param pool Pool handle
 * @param ptr Device pointer to free
 *
 * COMPLEXITY: O(1) for bump (no-op), O(1) for slab, O(log n) for buddy
 * THREAD SAFETY: Thread-safe (mutex protected)
 */
NIMCP_EXPORT void nimcp_gpu_pool_free(nimcp_gpu_pool_t* pool, void* ptr);

/**
 * @brief Reset pool (free all allocations)
 *
 * WHAT: Bulk deallocation of all memory
 * WHY:  Fast cleanup between iterations
 * HOW:  Resets allocator state without freeing underlying memory
 *
 * @param pool Pool handle
 * @return Number of blocks freed
 *
 * WARNING: Invalidates all outstanding pointers
 */
NIMCP_EXPORT size_t nimcp_gpu_pool_reset(nimcp_gpu_pool_t* pool);

//=============================================================================
// Memory Management API
//=============================================================================

/**
 * @brief Defragment pool memory
 *
 * WHAT: Compact memory to reduce fragmentation
 * WHY:  Recover unusable fragmented space
 * HOW:  Moves blocks to create contiguous free space
 *
 * @param pool Pool handle
 * @return Bytes reclaimed
 *
 * NOTE: Only supported by buddy allocator
 */
NIMCP_EXPORT size_t nimcp_gpu_pool_defragment(nimcp_gpu_pool_t* pool);

/**
 * @brief Reserve memory without allocating
 *
 * WHAT: Pre-allocate memory for future use
 * WHY:  Ensure memory availability before critical path
 * HOW:  Grows pool if needed
 *
 * @param pool Pool handle
 * @param size Size to reserve
 * @return true if reservation successful
 */
NIMCP_EXPORT bool nimcp_gpu_pool_reserve(nimcp_gpu_pool_t* pool, size_t size);

/**
 * @brief Release unused memory
 *
 * WHAT: Return unused memory to system
 * WHY:  Reduce memory footprint when not needed
 * HOW:  Shrinks pool to used size plus headroom
 *
 * @param pool Pool handle
 * @return Bytes released
 */
NIMCP_EXPORT size_t nimcp_gpu_pool_trim(nimcp_gpu_pool_t* pool);

/**
 * @brief Check if pool owns pointer
 *
 * @param pool Pool handle
 * @param ptr Pointer to check
 * @return true if ptr belongs to this pool
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Lock-free
 */
NIMCP_EXPORT bool nimcp_gpu_pool_owns(const nimcp_gpu_pool_t* pool, const void* ptr);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get pool statistics
 *
 * @param pool Pool handle
 * @param stats Output statistics structure
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_pool_get_stats(
    const nimcp_gpu_pool_t* pool,
    nimcp_gpu_pool_stats_t* stats
);

/**
 * @brief Reset pool statistics
 *
 * @param pool Pool handle
 */
NIMCP_EXPORT void nimcp_gpu_pool_reset_stats(nimcp_gpu_pool_t* pool);

/**
 * @brief Print pool statistics to stdout
 *
 * @param pool Pool handle
 */
NIMCP_EXPORT void nimcp_gpu_pool_print_stats(const nimcp_gpu_pool_t* pool);

/**
 * @brief Get statistics as formatted string
 *
 * @param pool Pool handle
 * @param buffer Output buffer
 * @param size Buffer size
 * @return Number of characters written
 */
NIMCP_EXPORT int nimcp_gpu_pool_stats_to_string(
    const nimcp_gpu_pool_t* pool,
    char* buffer,
    size_t size
);

//=============================================================================
// Observer API (Observer Pattern)
//=============================================================================

/**
 * @brief Register block observer
 *
 * @param pool Pool handle
 * @param callback Callback function
 * @param user_data User data passed to callback
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_pool_add_observer(
    nimcp_gpu_pool_t* pool,
    nimcp_gpu_block_observer_fn callback,
    void* user_data
);

/**
 * @brief Unregister block observer
 *
 * @param pool Pool handle
 * @param callback Callback to remove
 * @return true if observer was found and removed
 */
NIMCP_EXPORT bool nimcp_gpu_pool_remove_observer(
    nimcp_gpu_pool_t* pool,
    nimcp_gpu_block_observer_fn callback
);

//=============================================================================
// Global Pool Manager (Singleton Pattern)
//=============================================================================

/**
 * @brief Initialize global pool manager
 *
 * DESIGN PATTERN: Singleton
 * - Single instance for application-wide memory management
 * - Thread-safe initialization with double-checked locking
 *
 * @param ctx GPU context
 * @param config Configuration (NULL for defaults)
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_pool_manager_init(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_pool_config_t* config
);

/**
 * @brief Shutdown global pool manager
 */
NIMCP_EXPORT void nimcp_gpu_pool_manager_shutdown(void);

/**
 * @brief Get global pool instance
 *
 * @return Global pool or NULL if not initialized
 */
NIMCP_EXPORT nimcp_gpu_pool_t* nimcp_gpu_pool_manager_get(void);

/**
 * @brief Allocate from global pool
 *
 * @param size Size in bytes
 * @return Device pointer or NULL on failure
 */
NIMCP_EXPORT void* nimcp_gpu_pool_manager_alloc(size_t size);

/**
 * @brief Free to global pool
 *
 * @param ptr Device pointer to free
 */
NIMCP_EXPORT void nimcp_gpu_pool_manager_free(void* ptr);

//=============================================================================
// Debug and Diagnostic API
//=============================================================================

/**
 * @brief Dump all active allocations
 *
 * @param pool Pool handle
 */
NIMCP_EXPORT void nimcp_gpu_pool_dump_allocations(const nimcp_gpu_pool_t* pool);

/**
 * @brief Check for memory leaks
 *
 * @param pool Pool handle
 * @return Number of leaked blocks (0 if none)
 */
NIMCP_EXPORT size_t nimcp_gpu_pool_check_leaks(const nimcp_gpu_pool_t* pool);

/**
 * @brief Validate pool integrity
 *
 * @param pool Pool handle
 * @return true if pool is healthy
 */
NIMCP_EXPORT bool nimcp_gpu_pool_validate(const nimcp_gpu_pool_t* pool);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_GPU_POOL_H
