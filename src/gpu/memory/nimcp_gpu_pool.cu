/**
 * @file nimcp_gpu_pool.cu
 * @brief GPU Memory Pool Implementation with Three Allocator Strategies
 *
 * WHAT: Complete implementation of GPU memory pool with pluggable allocators
 * WHY:  High-performance memory management for neural network training
 * HOW:  Three allocator strategies: Bump, Buddy, Slab
 *
 * ALLOCATOR IMPLEMENTATIONS:
 *
 * 1. BUMP ALLOCATOR:
 *    - Linear allocation with reset-based deallocation
 *    - O(1) alloc, O(1) reset, no individual frees
 *    - Best for per-iteration scratch memory
 *
 * 2. BUDDY ALLOCATOR:
 *    - Power-of-2 block splitting/coalescing
 *    - O(log n) alloc/free, bounded fragmentation
 *    - Best for varied-size persistent allocations
 *
 * 3. SLAB ALLOCATOR:
 *    - Fixed-size pools with free lists
 *    - O(1) alloc/free, zero external fragmentation
 *    - Best for known-size activation tensors
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifdef NIMCP_ENABLE_CUDA
#include <cuda_runtime.h>
#endif

#include "gpu/memory/nimcp_gpu_pool.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define LOG_MODULE "GPU_POOL"

//=============================================================================
// Internal Constants
//=============================================================================

#define BUMP_ALLOCATOR_MAGIC  0x42554D50  // 'BUMP'
#define BUDDY_ALLOCATOR_MAGIC 0x42554459  // 'BUDY'
#define SLAB_ALLOCATOR_MAGIC  0x534C4142  // 'SLAB'

#define MIN_BUDDY_ORDER 8   // 256 bytes minimum
#define MAX_BUDDY_ORDER 30  // ~1GB maximum block

//=============================================================================
// CUDA Kernels for Memory Operations
//=============================================================================

#ifdef NIMCP_ENABLE_CUDA

/**
 * @brief Kernel to zero memory asynchronously
 */
__global__ void kernel_memzero(uint32_t* data, size_t num_words) {
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    size_t stride = blockDim.x * gridDim.x;

    for (size_t i = idx; i < num_words; i += stride) {
        data[i] = 0;
    }
}

/**
 * @brief Kernel to copy memory within GPU
 */
__global__ void kernel_memcpy(uint32_t* dst, const uint32_t* src, size_t num_words) {
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    size_t stride = blockDim.x * gridDim.x;

    for (size_t i = idx; i < num_words; i += stride) {
        dst[i] = src[i];
    }
}

/**
 * @brief Kernel for compacting memory during defragmentation
 *
 * Each thread handles one block's worth of data
 */
__global__ void kernel_compact_block(
    uint8_t* dst,
    const uint8_t* src,
    const size_t* offsets,
    const size_t* sizes,
    size_t num_blocks
) {
    size_t block_idx = blockIdx.x;
    if (block_idx >= num_blocks) return;

    size_t src_offset = offsets[block_idx * 2];
    size_t dst_offset = offsets[block_idx * 2 + 1];
    size_t block_size = sizes[block_idx];

    // Each thread in block copies a portion
    for (size_t i = threadIdx.x; i < block_size; i += blockDim.x) {
        dst[dst_offset + i] = src[src_offset + i];
    }
}

/**
 * @brief Launch async memory zeroing
 */
static void launch_memzero_async(void* ptr, size_t size, cudaStream_t stream) {
    if (!ptr || size == 0) return;

    size_t num_words = (size + 3) / 4;
    int block_size = 256;
    int grid_size = (int)((num_words + block_size - 1) / block_size);
    if (grid_size > 65535) grid_size = 65535;

    kernel_memzero<<<grid_size, block_size, 0, stream>>>((uint32_t*)ptr, num_words);
}

/**
 * @brief Launch async memory copy
 */
static void launch_memcpy_async(void* dst, const void* src, size_t size, cudaStream_t stream) {
    if (!dst || !src || size == 0) return;

    size_t num_words = (size + 3) / 4;
    int block_size = 256;
    int grid_size = (int)((num_words + block_size - 1) / block_size);
    if (grid_size > 65535) grid_size = 65535;

    kernel_memcpy<<<grid_size, block_size, 0, stream>>>(
        (uint32_t*)dst, (const uint32_t*)src, num_words);
}

#endif // NIMCP_ENABLE_CUDA

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get current timestamp in nanoseconds
 */
static uint64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/**
 * @brief Align size up to alignment boundary
 */
static inline size_t align_up(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

/**
 * @brief Check if value is power of 2
 */
static inline bool is_power_of_2(size_t value) {
    return value != 0 && (value & (value - 1)) == 0;
}

/**
 * @brief Get order (log2) of size, rounded up
 */
static inline size_t get_order(size_t size) {
    if (size <= 1) return 0;
    size_t order = 0;
    size_t s = size - 1;
    while (s > 0) {
        s >>= 1;
        order++;
    }
    return order;
}

/**
 * @brief Get size from order (2^order)
 */
static inline size_t order_to_size(size_t order) {
    return (size_t)1 << order;
}

//=============================================================================
// BUMP ALLOCATOR IMPLEMENTATION
//=============================================================================

/**
 * @brief Bump allocator internal structure
 *
 * DESIGN: Simple linear allocation with reset
 * - Maintains a single "bump pointer" that advances with each allocation
 * - No individual frees; reset returns pointer to beginning
 * - Extremely fast (single atomic increment possible)
 */
typedef struct bump_allocator_s {
    nimcp_gpu_allocator_t base;     /**< Base allocator (must be first) */
    size_t offset;                  /**< Current offset from base */
    size_t high_water_mark;         /**< Peak allocation offset */
    uint64_t allocation_count;      /**< Number of allocations */
    nimcp_platform_mutex_t mutex;   /**< Thread safety */
} bump_allocator_t;

// Forward declarations for vtable
static void* bump_alloc(void* self, size_t size, size_t alignment, void* stream);
static void bump_free(void* self, void* ptr);
static size_t bump_reset(void* self);
static size_t bump_get_used(void* self);
static size_t bump_get_available(void* self);
static bool bump_owns(void* self, const void* ptr);
static size_t bump_defragment(void* self, void* stream);
static void bump_destroy(void* self);

/**
 * @brief Bump allocator vtable
 */
static const nimcp_gpu_allocator_ops_t bump_ops = {
    .alloc = bump_alloc,
    .free = bump_free,
    .reset = bump_reset,
    .get_used = bump_get_used,
    .get_available = bump_get_available,
    .owns = bump_owns,
    .defragment = bump_defragment,
    .destroy = bump_destroy
};

/**
 * @brief Create bump allocator
 */
static nimcp_gpu_allocator_t* bump_allocator_create(
    nimcp_gpu_context_t* ctx,
    size_t total_size
) {
    bump_allocator_t* alloc = (bump_allocator_t*)nimcp_calloc(1, sizeof(bump_allocator_t));
    if (!alloc) {
        LOG_ERROR("Failed to allocate bump allocator structure");
        return NULL;
    }

    // Initialize base
    alloc->base.ops = &bump_ops;
    alloc->base.ctx = ctx;
    alloc->base.total_size = total_size;
    alloc->base.magic = BUMP_ALLOCATOR_MAGIC;

#ifdef NIMCP_ENABLE_CUDA
    // Allocate GPU memory
    cudaError_t err = cudaMalloc(&alloc->base.base_ptr, total_size);
    if (err != cudaSuccess) {
        LOG_ERROR("Failed to allocate %zu bytes of GPU memory: %s",
                  total_size, cudaGetErrorString(err));
        nimcp_free(alloc);
        return NULL;
    }
#else
    alloc->base.base_ptr = nimcp_aligned_malloc(total_size, NIMCP_GPU_POOL_DEFAULT_ALIGN);
    if (!alloc->base.base_ptr) {
        LOG_ERROR("Failed to allocate %zu bytes of CPU memory", total_size);
        nimcp_free(alloc);
        return NULL;
    }
#endif

    alloc->offset = 0;
    alloc->high_water_mark = 0;
    alloc->allocation_count = 0;

    if (nimcp_platform_mutex_init(&alloc->mutex, false) != 0) {
        LOG_ERROR("Failed to initialize mutex");
#ifdef NIMCP_ENABLE_CUDA
        cudaFree(alloc->base.base_ptr);
#else
        nimcp_aligned_free(alloc->base.base_ptr);
#endif
        nimcp_free(alloc);
        return NULL;
    }

    LOG_DEBUG("Created bump allocator: %zu bytes", total_size);
    return &alloc->base;
}

static void* bump_alloc(void* self, size_t size, size_t alignment, void* stream) {
    bump_allocator_t* alloc = (bump_allocator_t*)self;
    if (!alloc || alloc->base.magic != BUMP_ALLOCATOR_MAGIC) return NULL;
    if (size == 0) return NULL;

    (void)stream; // Bump allocator ignores stream

    if (alignment == 0) alignment = NIMCP_GPU_POOL_DEFAULT_ALIGN;

    nimcp_platform_mutex_lock(&alloc->mutex);

    // Align current offset
    size_t aligned_offset = align_up(alloc->offset, alignment);
    size_t new_offset = aligned_offset + size;

    if (new_offset > alloc->base.total_size) {
        nimcp_platform_mutex_unlock(&alloc->mutex);
        LOG_WARN("Bump allocator OOM: requested %zu, available %zu",
                 size, alloc->base.total_size - alloc->offset);
        return NULL;
    }

    void* ptr = (uint8_t*)alloc->base.base_ptr + aligned_offset;
    alloc->offset = new_offset;
    alloc->allocation_count++;

    if (new_offset > alloc->high_water_mark) {
        alloc->high_water_mark = new_offset;
    }

    nimcp_platform_mutex_unlock(&alloc->mutex);

    LOG_TRACE("Bump alloc: %zu bytes at offset %zu, ptr=%p",
              size, aligned_offset, ptr);
    return ptr;
}

static void bump_free(void* self, void* ptr) {
    // Bump allocator doesn't support individual frees
    (void)self;
    (void)ptr;
    LOG_TRACE("Bump free ignored (use reset)");
}

static size_t bump_reset(void* self) {
    bump_allocator_t* alloc = (bump_allocator_t*)self;
    if (!alloc || alloc->base.magic != BUMP_ALLOCATOR_MAGIC) return 0;

    nimcp_platform_mutex_lock(&alloc->mutex);

    uint64_t count = alloc->allocation_count;
    alloc->offset = 0;
    alloc->allocation_count = 0;

    nimcp_platform_mutex_unlock(&alloc->mutex);

    LOG_DEBUG("Bump allocator reset: %llu allocations freed",
              (unsigned long long)count);
    return (size_t)count;
}

static size_t bump_get_used(void* self) {
    bump_allocator_t* alloc = (bump_allocator_t*)self;
    if (!alloc || alloc->base.magic != BUMP_ALLOCATOR_MAGIC) return 0;
    return alloc->offset;
}

static size_t bump_get_available(void* self) {
    bump_allocator_t* alloc = (bump_allocator_t*)self;
    if (!alloc || alloc->base.magic != BUMP_ALLOCATOR_MAGIC) return 0;
    return alloc->base.total_size - alloc->offset;
}

static bool bump_owns(void* self, const void* ptr) {
    bump_allocator_t* alloc = (bump_allocator_t*)self;
    if (!alloc || alloc->base.magic != BUMP_ALLOCATOR_MAGIC) return false;

    const uint8_t* p = (const uint8_t*)ptr;
    const uint8_t* base = (const uint8_t*)alloc->base.base_ptr;
    return p >= base && p < base + alloc->base.total_size;
}

static size_t bump_defragment(void* self, void* stream) {
    (void)self;
    (void)stream;
    // Bump allocator has no fragmentation
    return 0;
}

static void bump_destroy(void* self) {
    bump_allocator_t* alloc = (bump_allocator_t*)self;
    if (!alloc || alloc->base.magic != BUMP_ALLOCATOR_MAGIC) return;

    nimcp_platform_mutex_destroy(&alloc->mutex);

#ifdef NIMCP_ENABLE_CUDA
    if (alloc->base.base_ptr) {
        cudaFree(alloc->base.base_ptr);
    }
#else
    if (alloc->base.base_ptr) {
        nimcp_aligned_free(alloc->base.base_ptr);
    }
#endif

    alloc->base.magic = 0;
    nimcp_free(alloc);
    LOG_DEBUG("Destroyed bump allocator");
}

//=============================================================================
// BUDDY ALLOCATOR IMPLEMENTATION
//=============================================================================

/**
 * @brief Buddy block structure
 *
 * Stored at the beginning of each block for tracking
 */
typedef struct buddy_block_s {
    uint32_t magic;                 /**< Magic number for validation */
    uint32_t order;                 /**< Block order (size = 2^order) */
    uint32_t is_free;               /**< 1 if free, 0 if allocated */
    uint32_t is_split;              /**< 1 if split into children */
    struct buddy_block_s* next;     /**< Next in free list */
    struct buddy_block_s* prev;     /**< Previous in free list */
} buddy_block_t;

#define BUDDY_BLOCK_MAGIC 0x42444259  // 'BDBY'

/**
 * @brief Buddy allocator internal structure
 */
typedef struct buddy_allocator_s {
    nimcp_gpu_allocator_t base;     /**< Base allocator (must be first) */
    buddy_block_t* free_lists[MAX_BUDDY_ORDER + 1]; /**< Free lists per order */
    size_t min_order;               /**< Minimum block order */
    size_t max_order;               /**< Maximum block order (entire pool) */
    size_t used_bytes;              /**< Currently allocated bytes */
    uint64_t allocation_count;      /**< Number of allocations */
    uint64_t split_count;           /**< Number of block splits */
    uint64_t coalesce_count;        /**< Number of block coalesces */

    // Block metadata stored separately (on CPU)
    buddy_block_t* block_metadata;  /**< Array of block metadata */
    size_t num_min_blocks;          /**< Number of minimum-size blocks */

    nimcp_platform_mutex_t mutex;   /**< Thread safety */
} buddy_allocator_t;

// Forward declarations
static void* buddy_alloc(void* self, size_t size, size_t alignment, void* stream);
static void buddy_free(void* self, void* ptr);
static size_t buddy_reset(void* self);
static size_t buddy_get_used(void* self);
static size_t buddy_get_available(void* self);
static bool buddy_owns(void* self, const void* ptr);
static size_t buddy_defragment(void* self, void* stream);
static void buddy_destroy(void* self);

static const nimcp_gpu_allocator_ops_t buddy_ops = {
    .alloc = buddy_alloc,
    .free = buddy_free,
    .reset = buddy_reset,
    .get_used = buddy_get_used,
    .get_available = buddy_get_available,
    .owns = buddy_owns,
    .defragment = buddy_defragment,
    .destroy = buddy_destroy
};

/**
 * @brief Get block index from pointer
 */
static size_t buddy_get_block_index(buddy_allocator_t* alloc, void* ptr) {
    size_t offset = (uint8_t*)ptr - (uint8_t*)alloc->base.base_ptr;
    return offset >> alloc->min_order;
}

/**
 * @brief Get pointer from block index
 */
static void* buddy_get_block_ptr(buddy_allocator_t* alloc, size_t index) {
    size_t offset = index << alloc->min_order;
    return (uint8_t*)alloc->base.base_ptr + offset;
}

/**
 * @brief Get buddy block index
 */
static size_t buddy_get_buddy_index(size_t index, size_t order, size_t min_order) {
    size_t block_count = (size_t)1 << (order - min_order);
    return index ^ block_count;
}

/**
 * @brief Remove block from free list
 */
static void buddy_remove_from_free_list(buddy_allocator_t* alloc, buddy_block_t* block) {
    size_t order = block->order;

    if (block->prev) {
        block->prev->next = block->next;
    } else {
        alloc->free_lists[order] = block->next;
    }

    if (block->next) {
        block->next->prev = block->prev;
    }

    block->next = NULL;
    block->prev = NULL;
}

/**
 * @brief Add block to free list
 */
static void buddy_add_to_free_list(buddy_allocator_t* alloc, buddy_block_t* block) {
    size_t order = block->order;

    block->next = alloc->free_lists[order];
    block->prev = NULL;

    if (alloc->free_lists[order]) {
        alloc->free_lists[order]->prev = block;
    }

    alloc->free_lists[order] = block;
    block->is_free = 1;
}

/**
 * @brief Split a block into two buddies
 */
static bool buddy_split_block(buddy_allocator_t* alloc, size_t index, size_t order) {
    if (order <= alloc->min_order) return false;

    buddy_block_t* block = &alloc->block_metadata[index];

    // Remove from current free list
    buddy_remove_from_free_list(alloc, block);

    // Calculate child indices
    size_t child_order = order - 1;
    size_t child_block_count = (size_t)1 << (child_order - alloc->min_order);
    size_t left_index = index;
    size_t right_index = index + child_block_count;

    // Initialize children
    buddy_block_t* left = &alloc->block_metadata[left_index];
    buddy_block_t* right = &alloc->block_metadata[right_index];

    left->magic = BUDDY_BLOCK_MAGIC;
    left->order = (uint32_t)child_order;
    left->is_split = 0;

    right->magic = BUDDY_BLOCK_MAGIC;
    right->order = (uint32_t)child_order;
    right->is_split = 0;

    // Mark parent as split
    block->is_split = 1;

    // Add children to free list
    buddy_add_to_free_list(alloc, left);
    buddy_add_to_free_list(alloc, right);

    alloc->split_count++;
    return true;
}

/**
 * @brief Try to coalesce block with buddy
 */
static bool buddy_try_coalesce(buddy_allocator_t* alloc, size_t index) {
    buddy_block_t* block = &alloc->block_metadata[index];
    size_t order = block->order;

    if (order >= alloc->max_order) return false;

    // Find buddy
    size_t buddy_index = buddy_get_buddy_index(index, order, alloc->min_order);
    if (buddy_index >= alloc->num_min_blocks) return false;

    buddy_block_t* buddy = &alloc->block_metadata[buddy_index];

    // Check if buddy is free and same order
    if (!buddy->is_free || buddy->order != order || buddy->is_split) {
        return false;
    }

    // Remove both from free lists
    buddy_remove_from_free_list(alloc, block);
    buddy_remove_from_free_list(alloc, buddy);

    // Parent is the lower index
    size_t parent_index = index < buddy_index ? index : buddy_index;
    buddy_block_t* parent = &alloc->block_metadata[parent_index];

    // Mark as coalesced
    parent->order = (uint32_t)(order + 1);
    parent->is_split = 0;
    parent->is_free = 0;

    // Add parent to free list
    buddy_add_to_free_list(alloc, parent);

    alloc->coalesce_count++;

    // Try to coalesce further up
    buddy_try_coalesce(alloc, parent_index);

    return true;
}

/**
 * @brief Create buddy allocator
 */
static nimcp_gpu_allocator_t* buddy_allocator_create(
    nimcp_gpu_context_t* ctx,
    size_t total_size,
    size_t min_block_size
) {
    buddy_allocator_t* alloc = (buddy_allocator_t*)nimcp_calloc(1, sizeof(buddy_allocator_t));
    if (!alloc) {
        LOG_ERROR("Failed to allocate buddy allocator structure");
        return NULL;
    }

    // Calculate orders
    size_t min_order = get_order(min_block_size);
    if (min_order < MIN_BUDDY_ORDER) min_order = MIN_BUDDY_ORDER;

    size_t max_order = get_order(total_size);
    if (max_order > MAX_BUDDY_ORDER) max_order = MAX_BUDDY_ORDER;

    // Round total size to power of 2
    total_size = order_to_size(max_order);

    // Calculate number of minimum blocks
    size_t num_min_blocks = total_size >> min_order;

    // Initialize base
    alloc->base.ops = &buddy_ops;
    alloc->base.ctx = ctx;
    alloc->base.total_size = total_size;
    alloc->base.magic = BUDDY_ALLOCATOR_MAGIC;

    alloc->min_order = min_order;
    alloc->max_order = max_order;
    alloc->num_min_blocks = num_min_blocks;
    alloc->used_bytes = 0;

    // Allocate block metadata (on CPU)
    alloc->block_metadata = (buddy_block_t*)nimcp_calloc(num_min_blocks, sizeof(buddy_block_t));
    if (!alloc->block_metadata) {
        LOG_ERROR("Failed to allocate buddy block metadata");
        nimcp_free(alloc);
        return NULL;
    }

#ifdef NIMCP_ENABLE_CUDA
    // Allocate GPU memory
    cudaError_t err = cudaMalloc(&alloc->base.base_ptr, total_size);
    if (err != cudaSuccess) {
        LOG_ERROR("Failed to allocate %zu bytes of GPU memory: %s",
                  total_size, cudaGetErrorString(err));
        nimcp_free(alloc->block_metadata);
        nimcp_free(alloc);
        return NULL;
    }
#else
    alloc->base.base_ptr = nimcp_aligned_malloc(total_size, NIMCP_GPU_POOL_DEFAULT_ALIGN);
    if (!alloc->base.base_ptr) {
        LOG_ERROR("Failed to allocate %zu bytes of CPU memory", total_size);
        nimcp_free(alloc->block_metadata);
        nimcp_free(alloc);
        return NULL;
    }
#endif

    // Initialize free lists
    for (size_t i = 0; i <= MAX_BUDDY_ORDER; i++) {
        alloc->free_lists[i] = NULL;
    }

    // Initialize root block
    buddy_block_t* root = &alloc->block_metadata[0];
    root->magic = BUDDY_BLOCK_MAGIC;
    root->order = (uint32_t)max_order;
    root->is_free = 0;
    root->is_split = 0;
    buddy_add_to_free_list(alloc, root);

    if (nimcp_platform_mutex_init(&alloc->mutex, false) != 0) {
        LOG_ERROR("Failed to initialize mutex");
#ifdef NIMCP_ENABLE_CUDA
        cudaFree(alloc->base.base_ptr);
#else
        nimcp_aligned_free(alloc->base.base_ptr);
#endif
        nimcp_free(alloc->block_metadata);
        nimcp_free(alloc);
        return NULL;
    }

    LOG_DEBUG("Created buddy allocator: %zu bytes, orders %zu-%zu",
              total_size, min_order, max_order);
    return &alloc->base;
}

static void* buddy_alloc(void* self, size_t size, size_t alignment, void* stream) {
    buddy_allocator_t* alloc = (buddy_allocator_t*)self;
    if (!alloc || alloc->base.magic != BUDDY_ALLOCATOR_MAGIC) return NULL;
    if (size == 0) return NULL;

    (void)stream; // Buddy allocator tracks internally

    // Add alignment overhead
    if (alignment == 0) alignment = NIMCP_GPU_POOL_DEFAULT_ALIGN;
    size = align_up(size, alignment);

    // Find required order
    size_t required_order = get_order(size);
    if (required_order < alloc->min_order) required_order = alloc->min_order;
    if (required_order > alloc->max_order) {
        LOG_WARN("Buddy alloc: size %zu exceeds max block size", size);
        return NULL;
    }

    nimcp_platform_mutex_lock(&alloc->mutex);

    // Find smallest available block
    size_t order = required_order;
    while (order <= alloc->max_order && !alloc->free_lists[order]) {
        order++;
    }

    if (order > alloc->max_order) {
        nimcp_platform_mutex_unlock(&alloc->mutex);
        LOG_WARN("Buddy allocator OOM: no block of order %zu available", required_order);
        return NULL;
    }

    // Split blocks until we get desired order
    buddy_block_t* block = alloc->free_lists[order];
    size_t index = (size_t)(block - alloc->block_metadata);

    while (order > required_order) {
        if (!buddy_split_block(alloc, index, order)) {
            nimcp_platform_mutex_unlock(&alloc->mutex);
            LOG_ERROR("Failed to split buddy block");
            return NULL;
        }
        order--;
    }

    // Allocate the block
    block = alloc->free_lists[required_order];
    index = (size_t)(block - alloc->block_metadata);
    buddy_remove_from_free_list(alloc, block);
    block->is_free = 0;

    // Get pointer
    void* ptr = buddy_get_block_ptr(alloc, index);

    // Update stats
    size_t block_size = order_to_size(required_order);
    alloc->used_bytes += block_size;
    alloc->allocation_count++;

    nimcp_platform_mutex_unlock(&alloc->mutex);

    LOG_TRACE("Buddy alloc: %zu bytes (order %zu), ptr=%p",
              size, required_order, ptr);
    return ptr;
}

static void buddy_free(void* self, void* ptr) {
    buddy_allocator_t* alloc = (buddy_allocator_t*)self;
    if (!alloc || alloc->base.magic != BUDDY_ALLOCATOR_MAGIC) return;
    if (!ptr) return;

    if (!buddy_owns(self, ptr)) {
        LOG_WARN("Buddy free: pointer %p not owned by allocator", ptr);
        return;
    }

    nimcp_platform_mutex_lock(&alloc->mutex);

    size_t index = buddy_get_block_index(alloc, ptr);
    buddy_block_t* block = &alloc->block_metadata[index];

    if (block->is_free) {
        LOG_WARN("Buddy free: double free detected at %p", ptr);
        nimcp_platform_mutex_unlock(&alloc->mutex);
        return;
    }

    size_t block_size = order_to_size(block->order);
    alloc->used_bytes -= block_size;

    // Add to free list and try to coalesce
    buddy_add_to_free_list(alloc, block);
    buddy_try_coalesce(alloc, index);

    nimcp_platform_mutex_unlock(&alloc->mutex);

    LOG_TRACE("Buddy free: ptr=%p (order %u)", ptr, block->order);
}

static size_t buddy_reset(void* self) {
    buddy_allocator_t* alloc = (buddy_allocator_t*)self;
    if (!alloc || alloc->base.magic != BUDDY_ALLOCATOR_MAGIC) return 0;

    nimcp_platform_mutex_lock(&alloc->mutex);

    size_t freed_count = alloc->allocation_count;

    // Clear all free lists
    for (size_t i = 0; i <= MAX_BUDDY_ORDER; i++) {
        alloc->free_lists[i] = NULL;
    }

    // Reset all metadata
    memset(alloc->block_metadata, 0, alloc->num_min_blocks * sizeof(buddy_block_t));

    // Initialize root block
    buddy_block_t* root = &alloc->block_metadata[0];
    root->magic = BUDDY_BLOCK_MAGIC;
    root->order = (uint32_t)alloc->max_order;
    root->is_free = 0;
    root->is_split = 0;
    buddy_add_to_free_list(alloc, root);

    alloc->used_bytes = 0;
    alloc->allocation_count = 0;

    nimcp_platform_mutex_unlock(&alloc->mutex);

    LOG_DEBUG("Buddy allocator reset: %zu allocations freed", freed_count);
    return freed_count;
}

static size_t buddy_get_used(void* self) {
    buddy_allocator_t* alloc = (buddy_allocator_t*)self;
    if (!alloc || alloc->base.magic != BUDDY_ALLOCATOR_MAGIC) return 0;
    return alloc->used_bytes;
}

static size_t buddy_get_available(void* self) {
    buddy_allocator_t* alloc = (buddy_allocator_t*)self;
    if (!alloc || alloc->base.magic != BUDDY_ALLOCATOR_MAGIC) return 0;
    return alloc->base.total_size - alloc->used_bytes;
}

static bool buddy_owns(void* self, const void* ptr) {
    buddy_allocator_t* alloc = (buddy_allocator_t*)self;
    if (!alloc || alloc->base.magic != BUDDY_ALLOCATOR_MAGIC) return false;

    const uint8_t* p = (const uint8_t*)ptr;
    const uint8_t* base = (const uint8_t*)alloc->base.base_ptr;
    return p >= base && p < base + alloc->base.total_size;
}

static size_t buddy_defragment(void* self, void* stream) {
    buddy_allocator_t* alloc = (buddy_allocator_t*)self;
    if (!alloc || alloc->base.magic != BUDDY_ALLOCATOR_MAGIC) return 0;

    (void)stream;

    // Note: True defragmentation would require moving allocated blocks,
    // which needs cooperation from clients. For now, we just try to
    // coalesce any free buddies that might have been missed.

    nimcp_platform_mutex_lock(&alloc->mutex);

    size_t initial_coalesce = alloc->coalesce_count;

    // Try to coalesce starting from smallest blocks
    for (size_t order = alloc->min_order; order < alloc->max_order; order++) {
        buddy_block_t* block = alloc->free_lists[order];
        while (block) {
            buddy_block_t* next = block->next;
            size_t index = (size_t)(block - alloc->block_metadata);
            buddy_try_coalesce(alloc, index);
            block = next;
        }
    }

    size_t coalesced = alloc->coalesce_count - initial_coalesce;

    nimcp_platform_mutex_unlock(&alloc->mutex);

    LOG_DEBUG("Buddy defragment: coalesced %zu block pairs", coalesced);
    return coalesced * order_to_size(alloc->min_order);
}

static void buddy_destroy(void* self) {
    buddy_allocator_t* alloc = (buddy_allocator_t*)self;
    if (!alloc || alloc->base.magic != BUDDY_ALLOCATOR_MAGIC) return;

    nimcp_platform_mutex_destroy(&alloc->mutex);

#ifdef NIMCP_ENABLE_CUDA
    if (alloc->base.base_ptr) {
        cudaFree(alloc->base.base_ptr);
    }
#else
    if (alloc->base.base_ptr) {
        nimcp_aligned_free(alloc->base.base_ptr);
    }
#endif

    nimcp_free(alloc->block_metadata);
    alloc->base.magic = 0;
    nimcp_free(alloc);
    LOG_DEBUG("Destroyed buddy allocator");
}

//=============================================================================
// SLAB ALLOCATOR IMPLEMENTATION
//=============================================================================

/**
 * @brief Slab block header (stored at beginning of each free block)
 */
typedef struct slab_block_s {
    struct slab_block_s* next;      /**< Next free block in slab */
} slab_block_t;

/**
 * @brief Slab descriptor (one per size class)
 */
typedef struct slab_s {
    void* base_ptr;                 /**< Base pointer for this slab */
    size_t block_size;              /**< Size of each block */
    size_t num_blocks;              /**< Total blocks in slab */
    size_t free_count;              /**< Number of free blocks */
    slab_block_t* free_list;        /**< Head of free list */
    uint64_t allocation_count;      /**< Allocations from this slab */
} slab_t;

/**
 * @brief Slab allocator internal structure
 */
typedef struct slab_allocator_s {
    nimcp_gpu_allocator_t base;     /**< Base allocator (must be first) */
    slab_t slabs[NIMCP_GPU_POOL_MAX_SLABS]; /**< Slab array */
    size_t num_slabs;               /**< Number of configured slabs */
    size_t total_used;              /**< Total bytes in use */
    nimcp_platform_mutex_t mutex;   /**< Thread safety */
} slab_allocator_t;

// Forward declarations
static void* slab_alloc(void* self, size_t size, size_t alignment, void* stream);
static void slab_free(void* self, void* ptr);
static size_t slab_reset(void* self);
static size_t slab_get_used(void* self);
static size_t slab_get_available(void* self);
static bool slab_owns(void* self, const void* ptr);
static size_t slab_defragment(void* self, void* stream);
static void slab_destroy(void* self);

static const nimcp_gpu_allocator_ops_t slab_ops = {
    .alloc = slab_alloc,
    .free = slab_free,
    .reset = slab_reset,
    .get_used = slab_get_used,
    .get_available = slab_get_available,
    .owns = slab_owns,
    .defragment = slab_defragment,
    .destroy = slab_destroy
};

/**
 * @brief Find slab containing pointer
 */
static slab_t* slab_find_containing(slab_allocator_t* alloc, void* ptr) {
    for (size_t i = 0; i < alloc->num_slabs; i++) {
        slab_t* slab = &alloc->slabs[i];
        uint8_t* base = (uint8_t*)slab->base_ptr;
        size_t slab_size = slab->block_size * slab->num_blocks;

        if ((uint8_t*)ptr >= base && (uint8_t*)ptr < base + slab_size) {
            return slab;
        }
    }
    return NULL;
}

/**
 * @brief Find best-fit slab for size
 */
static slab_t* slab_find_best_fit(slab_allocator_t* alloc, size_t size) {
    slab_t* best = NULL;

    for (size_t i = 0; i < alloc->num_slabs; i++) {
        slab_t* slab = &alloc->slabs[i];
        if (slab->block_size >= size && slab->free_count > 0) {
            if (!best || slab->block_size < best->block_size) {
                best = slab;
            }
        }
    }

    return best;
}

/**
 * @brief Initialize a slab
 */
static bool slab_init(slab_t* slab, void* base_ptr, size_t block_size, size_t num_blocks) {
    slab->base_ptr = base_ptr;
    slab->block_size = block_size;
    slab->num_blocks = num_blocks;
    slab->free_count = num_blocks;
    slab->allocation_count = 0;
    slab->free_list = NULL;

    // Build free list (linking blocks from back to front)
    for (size_t i = num_blocks; i > 0; i--) {
        slab_block_t* block = (slab_block_t*)((uint8_t*)base_ptr + (i - 1) * block_size);
        block->next = slab->free_list;
        slab->free_list = block;
    }

    return true;
}

/**
 * @brief Create slab allocator
 */
static nimcp_gpu_allocator_t* slab_allocator_create(
    nimcp_gpu_context_t* ctx,
    size_t total_size,
    const nimcp_gpu_slab_config_t* configs,
    size_t num_configs
) {
    if (num_configs == 0 || num_configs > NIMCP_GPU_POOL_MAX_SLABS) {
        LOG_ERROR("Invalid number of slab configs: %zu", num_configs);
        return NULL;
    }

    slab_allocator_t* alloc = (slab_allocator_t*)nimcp_calloc(1, sizeof(slab_allocator_t));
    if (!alloc) {
        LOG_ERROR("Failed to allocate slab allocator structure");
        return NULL;
    }

    // Calculate total required size
    size_t required_size = 0;
    for (size_t i = 0; i < num_configs; i++) {
        size_t aligned_block = align_up(configs[i].block_size, NIMCP_GPU_POOL_DEFAULT_ALIGN);
        required_size += aligned_block * configs[i].num_blocks;
    }

    if (required_size > total_size) {
        total_size = required_size;
    }

    // Initialize base
    alloc->base.ops = &slab_ops;
    alloc->base.ctx = ctx;
    alloc->base.total_size = total_size;
    alloc->base.magic = SLAB_ALLOCATOR_MAGIC;

#ifdef NIMCP_ENABLE_CUDA
    cudaError_t err = cudaMalloc(&alloc->base.base_ptr, total_size);
    if (err != cudaSuccess) {
        LOG_ERROR("Failed to allocate %zu bytes of GPU memory: %s",
                  total_size, cudaGetErrorString(err));
        nimcp_free(alloc);
        return NULL;
    }
#else
    alloc->base.base_ptr = nimcp_aligned_malloc(total_size, NIMCP_GPU_POOL_DEFAULT_ALIGN);
    if (!alloc->base.base_ptr) {
        LOG_ERROR("Failed to allocate %zu bytes of CPU memory", total_size);
        nimcp_free(alloc);
        return NULL;
    }
#endif

    // Initialize slabs
    alloc->num_slabs = num_configs;
    uint8_t* current_ptr = (uint8_t*)alloc->base.base_ptr;

    for (size_t i = 0; i < num_configs; i++) {
        size_t aligned_block = align_up(configs[i].block_size, NIMCP_GPU_POOL_DEFAULT_ALIGN);
        slab_init(&alloc->slabs[i], current_ptr, aligned_block, configs[i].num_blocks);
        current_ptr += aligned_block * configs[i].num_blocks;

        LOG_DEBUG("Slab %zu: block_size=%zu, num_blocks=%zu",
                  i, aligned_block, configs[i].num_blocks);
    }

    alloc->total_used = 0;

    if (nimcp_platform_mutex_init(&alloc->mutex, false) != 0) {
        LOG_ERROR("Failed to initialize mutex");
#ifdef NIMCP_ENABLE_CUDA
        cudaFree(alloc->base.base_ptr);
#else
        nimcp_aligned_free(alloc->base.base_ptr);
#endif
        nimcp_free(alloc);
        return NULL;
    }

    LOG_DEBUG("Created slab allocator: %zu bytes, %zu slabs", total_size, num_configs);
    return &alloc->base;
}

static void* slab_alloc(void* self, size_t size, size_t alignment, void* stream) {
    slab_allocator_t* alloc = (slab_allocator_t*)self;
    if (!alloc || alloc->base.magic != SLAB_ALLOCATOR_MAGIC) return NULL;
    if (size == 0) return NULL;

    (void)stream;

    if (alignment == 0) alignment = NIMCP_GPU_POOL_DEFAULT_ALIGN;
    size = align_up(size, alignment);

    nimcp_platform_mutex_lock(&alloc->mutex);

    slab_t* slab = slab_find_best_fit(alloc, size);
    if (!slab) {
        nimcp_platform_mutex_unlock(&alloc->mutex);
        LOG_WARN("Slab allocator: no slab available for size %zu", size);
        return NULL;
    }

    // Pop from free list
    slab_block_t* block = slab->free_list;
    slab->free_list = block->next;
    slab->free_count--;
    slab->allocation_count++;
    alloc->total_used += slab->block_size;

    nimcp_platform_mutex_unlock(&alloc->mutex);

    LOG_TRACE("Slab alloc: %zu bytes from slab (block_size=%zu), ptr=%p",
              size, slab->block_size, (void*)block);
    return (void*)block;
}

static void slab_free(void* self, void* ptr) {
    slab_allocator_t* alloc = (slab_allocator_t*)self;
    if (!alloc || alloc->base.magic != SLAB_ALLOCATOR_MAGIC) return;
    if (!ptr) return;

    nimcp_platform_mutex_lock(&alloc->mutex);

    slab_t* slab = slab_find_containing(alloc, ptr);
    if (!slab) {
        nimcp_platform_mutex_unlock(&alloc->mutex);
        LOG_WARN("Slab free: pointer %p not owned by allocator", ptr);
        return;
    }

    // Verify alignment
    size_t offset = (uint8_t*)ptr - (uint8_t*)slab->base_ptr;
    if (offset % slab->block_size != 0) {
        nimcp_platform_mutex_unlock(&alloc->mutex);
        LOG_WARN("Slab free: pointer %p has invalid offset", ptr);
        return;
    }

    // Push to free list
    slab_block_t* block = (slab_block_t*)ptr;
    block->next = slab->free_list;
    slab->free_list = block;
    slab->free_count++;
    alloc->total_used -= slab->block_size;

    nimcp_platform_mutex_unlock(&alloc->mutex);

    LOG_TRACE("Slab free: ptr=%p", ptr);
}

static size_t slab_reset(void* self) {
    slab_allocator_t* alloc = (slab_allocator_t*)self;
    if (!alloc || alloc->base.magic != SLAB_ALLOCATOR_MAGIC) return 0;

    nimcp_platform_mutex_lock(&alloc->mutex);

    size_t freed_count = 0;

    for (size_t i = 0; i < alloc->num_slabs; i++) {
        slab_t* slab = &alloc->slabs[i];
        size_t allocated = slab->num_blocks - slab->free_count;
        freed_count += allocated;

        // Rebuild free list
        slab->free_list = NULL;
        for (size_t j = slab->num_blocks; j > 0; j--) {
            slab_block_t* block = (slab_block_t*)((uint8_t*)slab->base_ptr +
                                                   (j - 1) * slab->block_size);
            block->next = slab->free_list;
            slab->free_list = block;
        }
        slab->free_count = slab->num_blocks;
        slab->allocation_count = 0;
    }

    alloc->total_used = 0;

    nimcp_platform_mutex_unlock(&alloc->mutex);

    LOG_DEBUG("Slab allocator reset: %zu allocations freed", freed_count);
    return freed_count;
}

static size_t slab_get_used(void* self) {
    slab_allocator_t* alloc = (slab_allocator_t*)self;
    if (!alloc || alloc->base.magic != SLAB_ALLOCATOR_MAGIC) return 0;
    return alloc->total_used;
}

static size_t slab_get_available(void* self) {
    slab_allocator_t* alloc = (slab_allocator_t*)self;
    if (!alloc || alloc->base.magic != SLAB_ALLOCATOR_MAGIC) return 0;
    return alloc->base.total_size - alloc->total_used;
}

static bool slab_owns(void* self, const void* ptr) {
    slab_allocator_t* alloc = (slab_allocator_t*)self;
    if (!alloc || alloc->base.magic != SLAB_ALLOCATOR_MAGIC) return false;

    const uint8_t* p = (const uint8_t*)ptr;
    const uint8_t* base = (const uint8_t*)alloc->base.base_ptr;
    return p >= base && p < base + alloc->base.total_size;
}

static size_t slab_defragment(void* self, void* stream) {
    (void)self;
    (void)stream;
    // Slab allocator has no fragmentation by design
    return 0;
}

static void slab_destroy(void* self) {
    slab_allocator_t* alloc = (slab_allocator_t*)self;
    if (!alloc || alloc->base.magic != SLAB_ALLOCATOR_MAGIC) return;

    nimcp_platform_mutex_destroy(&alloc->mutex);

#ifdef NIMCP_ENABLE_CUDA
    if (alloc->base.base_ptr) {
        cudaFree(alloc->base.base_ptr);
    }
#else
    if (alloc->base.base_ptr) {
        nimcp_aligned_free(alloc->base.base_ptr);
    }
#endif

    alloc->base.magic = 0;
    nimcp_free(alloc);
    LOG_DEBUG("Destroyed slab allocator");
}

//=============================================================================
// POOL IMPLEMENTATION
//=============================================================================

/**
 * @brief Default slab configurations for activation pool
 */
static const nimcp_gpu_slab_config_t default_slab_configs[] = {
    { 256,    4096 },    // 256B blocks (1MB total)
    { 1024,   2048 },    // 1KB blocks (2MB total)
    { 4096,   1024 },    // 4KB blocks (4MB total)
    { 16384,  512 },     // 16KB blocks (8MB total)
    { 65536,  256 },     // 64KB blocks (16MB total)
    { 262144, 128 },     // 256KB blocks (32MB total)
    { 1048576, 64 },     // 1MB blocks (64MB total)
    { 4194304, 32 },     // 4MB blocks (128MB total)
};
static const size_t default_num_slab_configs =
    sizeof(default_slab_configs) / sizeof(default_slab_configs[0]);

nimcp_gpu_pool_config_t nimcp_gpu_pool_config_default(void) {
    nimcp_gpu_pool_config_t config;
    memset(&config, 0, sizeof(config));

    config.initial_size = 1ULL * 1024 * 1024 * 1024;  // 1GB default
    config.max_size = 16ULL * 1024 * 1024 * 1024;     // 16GB max
    config.block_size = NIMCP_GPU_POOL_MIN_BLOCK;
    config.alignment = NIMCP_GPU_POOL_DEFAULT_ALIGN;
    config.enable_defrag = false;
    config.enable_stats = true;
    config.growth_factor = 1.5f;
    config.type = NIMCP_GPU_POOL_TYPE_PERSISTENT;
    config.buddy_min_order = MIN_BUDDY_ORDER;
    config.buddy_max_order = 30;

    return config;
}

nimcp_gpu_pool_config_t nimcp_gpu_pool_config_scratch(size_t size_bytes) {
    nimcp_gpu_pool_config_t config = nimcp_gpu_pool_config_default();
    config.initial_size = size_bytes;
    config.max_size = size_bytes;
    config.type = NIMCP_GPU_POOL_TYPE_SCRATCH;
    config.enable_defrag = false;
    return config;
}

nimcp_gpu_pool_config_t nimcp_gpu_pool_config_persistent(size_t size_bytes) {
    nimcp_gpu_pool_config_t config = nimcp_gpu_pool_config_default();
    config.initial_size = size_bytes;
    config.max_size = size_bytes * 2;
    config.type = NIMCP_GPU_POOL_TYPE_PERSISTENT;
    config.enable_defrag = true;
    return config;
}

nimcp_gpu_pool_config_t nimcp_gpu_pool_config_activation(
    size_t size_bytes,
    const size_t* slab_sizes,
    size_t num_slabs
) {
    nimcp_gpu_pool_config_t config = nimcp_gpu_pool_config_default();
    config.initial_size = size_bytes;
    config.max_size = size_bytes;
    config.type = NIMCP_GPU_POOL_TYPE_ACTIVATION;
    config.enable_defrag = false;

    // Configure slabs
    if (slab_sizes && num_slabs > 0) {
        size_t count = num_slabs < NIMCP_GPU_POOL_MAX_SLABS ?
                       num_slabs : NIMCP_GPU_POOL_MAX_SLABS;
        for (size_t i = 0; i < count; i++) {
            config.slab_configs[i].block_size = slab_sizes[i];
            // Default: allocate ~10% of pool for each slab
            config.slab_configs[i].num_blocks = (size_bytes / 10) / slab_sizes[i];
            if (config.slab_configs[i].num_blocks < 1) {
                config.slab_configs[i].num_blocks = 1;
            }
        }
        config.num_slab_configs = count;
    } else {
        // Use defaults
        size_t count = default_num_slab_configs < NIMCP_GPU_POOL_MAX_SLABS ?
                       default_num_slab_configs : NIMCP_GPU_POOL_MAX_SLABS;
        for (size_t i = 0; i < count; i++) {
            config.slab_configs[i] = default_slab_configs[i];
        }
        config.num_slab_configs = count;
    }

    return config;
}

/**
 * @brief Create allocator based on pool type
 */
static nimcp_gpu_allocator_t* create_allocator_for_type(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_pool_config_t* config
) {
    switch (config->type) {
        case NIMCP_GPU_POOL_TYPE_SCRATCH:
            return bump_allocator_create(ctx, config->initial_size);

        case NIMCP_GPU_POOL_TYPE_PERSISTENT:
            return buddy_allocator_create(ctx, config->initial_size, config->block_size);

        case NIMCP_GPU_POOL_TYPE_ACTIVATION:
            if (config->num_slab_configs > 0) {
                return slab_allocator_create(ctx, config->initial_size,
                                             config->slab_configs,
                                             config->num_slab_configs);
            } else {
                return slab_allocator_create(ctx, config->initial_size,
                                             default_slab_configs,
                                             default_num_slab_configs);
            }

        default:
            LOG_ERROR("Unknown pool type: %d", config->type);
            return NULL;
    }
}

nimcp_gpu_pool_t* nimcp_gpu_pool_create(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_pool_config_t* config
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx) {
        LOG_ERROR("NULL GPU context");
        return NULL;
    }

    // Use defaults if no config provided
    nimcp_gpu_pool_config_t default_config = nimcp_gpu_pool_config_default();
    if (!config) {
        config = &default_config;
    }

    // Allocate pool structure
    nimcp_gpu_pool_t* pool = (nimcp_gpu_pool_t*)nimcp_calloc(1, sizeof(nimcp_gpu_pool_t));
    if (!pool) {
        LOG_ERROR("Failed to allocate pool structure");
        return NULL;
    }

    pool->magic = NIMCP_GPU_POOL_MAGIC;
    pool->ctx = ctx;
    memcpy(&pool->config, config, sizeof(nimcp_gpu_pool_config_t));

    // Create allocator
    pool->allocator = create_allocator_for_type(ctx, config);
    if (!pool->allocator) {
        LOG_ERROR("Failed to create allocator");
        nimcp_free(pool);
        return NULL;
    }

    // Initialize block tracking
    pool->blocks_head = NULL;
    pool->blocks_tail = NULL;
    pool->free_blocks = NULL;
    pool->num_blocks = 0;

    // Pre-allocate some block structures
    size_t prealloc_blocks = 1024;
    for (size_t i = 0; i < prealloc_blocks; i++) {
        nimcp_gpu_block_t* block = (nimcp_gpu_block_t*)nimcp_calloc(1, sizeof(nimcp_gpu_block_t));
        if (block) {
            block->next = pool->free_blocks;
            pool->free_blocks = block;
        }
    }

    // Initialize mutex
    pool->mutex = nimcp_calloc(1, sizeof(nimcp_platform_mutex_t));
    if (!pool->mutex ||
        nimcp_platform_mutex_init((nimcp_platform_mutex_t*)pool->mutex, false) != 0) {
        LOG_ERROR("Failed to initialize pool mutex");
        // Cleanup
        while (pool->free_blocks) {
            nimcp_gpu_block_t* next = pool->free_blocks->next;
            nimcp_free(pool->free_blocks);
            pool->free_blocks = next;
        }
        pool->allocator->ops->destroy(pool->allocator);
        if (pool->mutex) nimcp_free(pool->mutex);
        nimcp_free(pool);
        return NULL;
    }

    // Initialize stats
    memset(&pool->stats, 0, sizeof(nimcp_gpu_pool_stats_t));
    pool->stats.total_size = config->initial_size;

    pool->initialized = true;
    pool->next_alloc_id = 1;
    pool->observers = NULL;

    const char* type_names[] = { "SCRATCH", "PERSISTENT", "ACTIVATION", "CUSTOM" };
    LOG_INFO("Created GPU pool: type=%s, size=%zu MB",
             type_names[config->type],
             config->initial_size / (1024 * 1024));

    return pool;
}

void nimcp_gpu_pool_destroy(nimcp_gpu_pool_t* pool) {
    if (!pool) return;
    if (pool->magic != NIMCP_GPU_POOL_MAGIC) return;

    // Check for leaks
    size_t leaks = nimcp_gpu_pool_check_leaks(pool);
    if (leaks > 0) {
        LOG_WARN("Destroying pool with %zu leaked allocations", leaks);
    }

    // Destroy allocator
    if (pool->allocator) {
        pool->allocator->ops->destroy(pool->allocator);
    }

    // Free all block structures
    nimcp_gpu_block_t* block = pool->blocks_head;
    while (block) {
        nimcp_gpu_block_t* next = block->next;
        nimcp_free(block);
        block = next;
    }

    while (pool->free_blocks) {
        nimcp_gpu_block_t* next = pool->free_blocks->next;
        nimcp_free(pool->free_blocks);
        pool->free_blocks = next;
    }

    // Free observers
    nimcp_gpu_block_observer_t* obs = pool->observers;
    while (obs) {
        nimcp_gpu_block_observer_t* next = obs->next;
        nimcp_free(obs);
        obs = next;
    }

    // Destroy mutex
    if (pool->mutex) {
        nimcp_platform_mutex_destroy((nimcp_platform_mutex_t*)pool->mutex);
        nimcp_free(pool->mutex);
    }

    pool->magic = 0;
    nimcp_free(pool);
    LOG_INFO("Destroyed GPU pool");
}

bool nimcp_gpu_pool_is_valid(const nimcp_gpu_pool_t* pool) {
    return pool && pool->magic == NIMCP_GPU_POOL_MAGIC && pool->initialized;
}

/**
 * @brief Get a block structure from free list or allocate new
 */
static nimcp_gpu_block_t* pool_get_block_struct(nimcp_gpu_pool_t* pool) {
    if (pool->free_blocks) {
        nimcp_gpu_block_t* block = pool->free_blocks;
        pool->free_blocks = block->next;
        memset(block, 0, sizeof(nimcp_gpu_block_t));
        return block;
    }
    return (nimcp_gpu_block_t*)nimcp_calloc(1, sizeof(nimcp_gpu_block_t));
}

/**
 * @brief Return block structure to free list
 */
static void pool_return_block_struct(nimcp_gpu_pool_t* pool, nimcp_gpu_block_t* block) {
    block->next = pool->free_blocks;
    pool->free_blocks = block;
}

/**
 * @brief Notify observers of allocation event
 */
static void pool_notify_observers(
    nimcp_gpu_pool_t* pool,
    nimcp_gpu_block_t* block,
    bool is_allocation
) {
    nimcp_gpu_block_observer_t* obs = pool->observers;
    while (obs) {
        obs->callback(block, is_allocation, obs->user_data);
        obs = obs->next;
    }
}

void* nimcp_gpu_pool_alloc(
    nimcp_gpu_pool_t* pool,
    size_t size,
    size_t alignment,
    void* stream
) {
    return nimcp_gpu_pool_alloc_tagged(pool, size, alignment, stream, NULL);
}

void* nimcp_gpu_pool_alloc_tagged(
    nimcp_gpu_pool_t* pool,
    size_t size,
    size_t alignment,
    void* stream,
    const char* tag
) {
    if (!nimcp_gpu_pool_is_valid(pool)) return NULL;
    if (size == 0) return NULL;

    uint64_t start_time = 0;
    if (pool->config.enable_stats) {
        start_time = get_time_ns();
    }

    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)pool->mutex);

    // Delegate to allocator
    void* ptr = pool->allocator->ops->alloc(pool->allocator, size, alignment, stream);

    if (!ptr) {
        pool->stats.failed_allocations++;
        nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)pool->mutex);
        return NULL;
    }

    // Track allocation
    nimcp_gpu_block_t* block = pool_get_block_struct(pool);
    if (!block) {
        pool->allocator->ops->nimcp_free(pool->allocator, ptr);
        nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)pool->mutex);
        return NULL;
    }

    block->ptr = ptr;
    block->size = size;
    block->state = NIMCP_GPU_BLOCK_ALLOCATED;
    block->stream = stream;
    block->alloc_time = get_time_ns();
    block->alloc_id = pool->next_alloc_id++;
    block->tag = tag;

    // Add to allocated list
    block->prev = pool->blocks_tail;
    block->next = NULL;
    if (pool->blocks_tail) {
        pool->blocks_tail->next = block;
    } else {
        pool->blocks_head = block;
    }
    pool->blocks_tail = block;
    pool->num_blocks++;

    // Update stats
    if (pool->config.enable_stats) {
        pool->stats.allocation_count++;
        pool->stats.current_allocations++;
        pool->stats.used_size += size;

        if (pool->stats.used_size > pool->stats.peak_used) {
            pool->stats.peak_used = pool->stats.used_size;
        }
        if (pool->stats.current_allocations > pool->stats.peak_allocations) {
            pool->stats.peak_allocations = pool->stats.current_allocations;
        }

        uint64_t elapsed = get_time_ns() - start_time;
        pool->stats.total_alloc_time_ns += elapsed;
    }

    // Notify observers
    if (pool->observers) {
        pool_notify_observers(pool, block, true);
    }

    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)pool->mutex);

    return ptr;
}

void nimcp_gpu_pool_free(nimcp_gpu_pool_t* pool, void* ptr) {
    if (!nimcp_gpu_pool_is_valid(pool)) return;
    if (!ptr) return;

    uint64_t start_time = 0;
    if (pool->config.enable_stats) {
        start_time = get_time_ns();
    }

    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)pool->mutex);

    // Find block in tracking list
    nimcp_gpu_block_t* block = pool->blocks_head;
    while (block && block->ptr != ptr) {
        block = block->next;
    }

    if (!block) {
        LOG_WARN("Pool free: pointer %p not tracked", ptr);
        nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)pool->mutex);
        return;
    }

    // Notify observers before free
    if (pool->observers) {
        pool_notify_observers(pool, block, false);
    }

    size_t freed_size = block->size;

    // Remove from tracking list
    if (block->prev) {
        block->prev->next = block->next;
    } else {
        pool->blocks_head = block->next;
    }
    if (block->next) {
        block->next->prev = block->prev;
    } else {
        pool->blocks_tail = block->prev;
    }
    pool->num_blocks--;

    // Return block struct to pool
    pool_return_block_struct(pool, block);

    // Delegate to allocator
    pool->allocator->ops->nimcp_free(pool->allocator, ptr);

    // Update stats
    if (pool->config.enable_stats) {
        pool->stats.free_count++;
        pool->stats.current_allocations--;
        pool->stats.used_size -= freed_size;

        uint64_t elapsed = get_time_ns() - start_time;
        pool->stats.total_free_time_ns += elapsed;
    }

    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)pool->mutex);
}

size_t nimcp_gpu_pool_reset(nimcp_gpu_pool_t* pool) {
    if (!nimcp_gpu_pool_is_valid(pool)) return 0;

    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)pool->mutex);

    // Reset allocator
    size_t count = pool->allocator->ops->reset(pool->allocator);

    // Clear tracking list
    nimcp_gpu_block_t* block = pool->blocks_head;
    while (block) {
        nimcp_gpu_block_t* next = block->next;
        pool_return_block_struct(pool, block);
        block = next;
    }
    pool->blocks_head = NULL;
    pool->blocks_tail = NULL;
    pool->num_blocks = 0;

    // Reset stats
    if (pool->config.enable_stats) {
        pool->stats.current_allocations = 0;
        pool->stats.used_size = 0;
    }

    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)pool->mutex);

    LOG_DEBUG("Pool reset: %zu allocations freed", count);
    return count;
}

size_t nimcp_gpu_pool_defragment(nimcp_gpu_pool_t* pool) {
    if (!nimcp_gpu_pool_is_valid(pool)) return 0;
    if (!pool->config.enable_defrag) return 0;

    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)pool->mutex);

#ifdef NIMCP_ENABLE_CUDA
    cudaStream_t stream = (cudaStream_t)nimcp_gpu_get_compute_stream(pool->ctx);
#else
    void* stream = NULL;
#endif

    size_t reclaimed = pool->allocator->ops->defragment(pool->allocator, stream);

    if (pool->config.enable_stats) {
        pool->stats.defrag_count++;
        pool->stats.defrag_bytes_reclaimed += reclaimed;
    }

    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)pool->mutex);

    return reclaimed;
}

bool nimcp_gpu_pool_reserve(nimcp_gpu_pool_t* pool, size_t size) {
    if (!nimcp_gpu_pool_is_valid(pool)) return false;

    size_t available = pool->allocator->ops->get_available(pool->allocator);
    if (available >= size) return true;

    // Pool growth not implemented yet - would need to reallocate
    LOG_WARN("Pool reserve: requested %zu but only %zu available", size, available);
    return false;
}

size_t nimcp_gpu_pool_trim(nimcp_gpu_pool_t* pool) {
    if (!nimcp_gpu_pool_is_valid(pool)) return 0;

    // Trim not implemented - would need to track high water mark
    // and potentially reallocate to smaller size
    return 0;
}

bool nimcp_gpu_pool_owns(const nimcp_gpu_pool_t* pool, const void* ptr) {
    if (!nimcp_gpu_pool_is_valid(pool)) return false;
    return pool->allocator->ops->owns(pool->allocator, ptr);
}

bool nimcp_gpu_pool_get_stats(
    const nimcp_gpu_pool_t* pool,
    nimcp_gpu_pool_stats_t* stats
) {
    if (!nimcp_gpu_pool_is_valid(pool) || !stats) return false;

    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)pool->mutex);

    memcpy(stats, &pool->stats, sizeof(nimcp_gpu_pool_stats_t));

    // Update dynamic stats
    stats->available_size = pool->allocator->ops->get_available(pool->allocator);
    stats->num_blocks = pool->num_blocks;

    // Calculate fragmentation
    size_t total_tracked = 0;
    nimcp_gpu_block_t* block = pool->blocks_head;
    while (block) {
        total_tracked += block->size;
        block = block->next;
    }

    size_t allocator_used = pool->allocator->ops->get_used(pool->allocator);
    stats->fragmentation_bytes = allocator_used > total_tracked ?
                                  allocator_used - total_tracked : 0;
    stats->fragmentation_ratio = allocator_used > 0 ?
                                  (float)stats->fragmentation_bytes / (float)allocator_used : 0.0f;

    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)pool->mutex);

    return true;
}

void nimcp_gpu_pool_reset_stats(nimcp_gpu_pool_t* pool) {
    if (!nimcp_gpu_pool_is_valid(pool)) return;

    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)pool->mutex);

    // Keep current state, reset counters
    pool->stats.allocation_count = 0;
    pool->stats.free_count = 0;
    pool->stats.failed_allocations = 0;
    pool->stats.total_alloc_time_ns = 0;
    pool->stats.total_free_time_ns = 0;
    pool->stats.defrag_count = 0;
    pool->stats.defrag_bytes_reclaimed = 0;
    pool->stats.peak_used = pool->stats.used_size;
    pool->stats.peak_allocations = pool->stats.current_allocations;

    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)pool->mutex);
}

void nimcp_gpu_pool_print_stats(const nimcp_gpu_pool_t* pool) {
    if (!nimcp_gpu_pool_is_valid(pool)) {
        printf("GPU Pool: Invalid\n");
        return;
    }

    nimcp_gpu_pool_stats_t stats;
    nimcp_gpu_pool_get_stats(pool, &stats);

    printf("GPU Memory Pool Statistics:\n");
    printf("  Total Size: %zu MB\n", stats.total_size / (1024 * 1024));
    printf("  Used Size: %zu MB (%.1f%%)\n",
           stats.used_size / (1024 * 1024),
           100.0 * stats.used_size / stats.total_size);
    printf("  Peak Used: %zu MB\n", stats.peak_used / (1024 * 1024));
    printf("  Available: %zu MB\n", stats.available_size / (1024 * 1024));
    printf("  Fragmentation: %zu KB (%.2f%%)\n",
           stats.fragmentation_bytes / 1024,
           100.0 * stats.fragmentation_ratio);
    printf("  Allocations: %llu total, %llu current, %llu peak\n",
           (unsigned long long)stats.allocation_count,
           (unsigned long long)stats.current_allocations,
           (unsigned long long)stats.peak_allocations);
    printf("  Frees: %llu\n", (unsigned long long)stats.free_count);
    printf("  Failed: %llu\n", (unsigned long long)stats.failed_allocations);
    printf("  Avg Alloc Time: %.2f us\n",
           stats.allocation_count > 0 ?
           (double)stats.total_alloc_time_ns / stats.allocation_count / 1000.0 : 0.0);
    printf("  Avg Free Time: %.2f us\n",
           stats.free_count > 0 ?
           (double)stats.total_free_time_ns / stats.free_count / 1000.0 : 0.0);
}

int nimcp_gpu_pool_stats_to_string(
    const nimcp_gpu_pool_t* pool,
    char* buffer,
    size_t size
) {
    if (!nimcp_gpu_pool_is_valid(pool) || !buffer || size == 0) return 0;

    nimcp_gpu_pool_stats_t stats;
    nimcp_gpu_pool_get_stats(pool, &stats);

    return snprintf(buffer, size,
                    "GPU Pool: %zu/%zu MB (%.1f%%), %llu allocs, frag=%.2f%%",
                    stats.used_size / (1024 * 1024),
                    stats.total_size / (1024 * 1024),
                    100.0 * stats.used_size / stats.total_size,
                    (unsigned long long)stats.current_allocations,
                    100.0 * stats.fragmentation_ratio);
}

bool nimcp_gpu_pool_add_observer(
    nimcp_gpu_pool_t* pool,
    nimcp_gpu_block_observer_fn callback,
    void* user_data
) {
    if (!nimcp_gpu_pool_is_valid(pool) || !callback) return false;

    nimcp_gpu_block_observer_t* obs =
        (nimcp_gpu_block_observer_t*)nimcp_calloc(1, sizeof(nimcp_gpu_block_observer_t));
    if (!obs) return false;

    obs->callback = callback;
    obs->user_data = user_data;

    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)pool->mutex);
    obs->next = pool->observers;
    pool->observers = obs;
    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)pool->mutex);

    return true;
}

bool nimcp_gpu_pool_remove_observer(
    nimcp_gpu_pool_t* pool,
    nimcp_gpu_block_observer_fn callback
) {
    if (!nimcp_gpu_pool_is_valid(pool) || !callback) return false;

    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)pool->mutex);

    nimcp_gpu_block_observer_t** pp = &pool->observers;
    while (*pp) {
        if ((*pp)->callback == callback) {
            nimcp_gpu_block_observer_t* obs = *pp;
            *pp = obs->next;
            nimcp_free(obs);
            nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)pool->mutex);
            return true;
        }
        pp = &(*pp)->next;
    }

    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)pool->mutex);
    return false;
}

//=============================================================================
// Global Pool Manager (Singleton)
//=============================================================================

static nimcp_gpu_pool_t* g_global_pool = NULL;
static nimcp_platform_mutex_t g_global_pool_mutex;
static bool g_global_pool_mutex_initialized = false;

bool nimcp_gpu_pool_manager_init(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_pool_config_t* config
) {
    if (!g_global_pool_mutex_initialized) {
        if (nimcp_platform_mutex_init(&g_global_pool_mutex, false) != 0) {
            LOG_ERROR("Failed to initialize global pool mutex");
            return false;
        }
        g_global_pool_mutex_initialized = true;
    }

    nimcp_platform_mutex_lock(&g_global_pool_mutex);

    if (g_global_pool) {
        LOG_WARN("Global pool already initialized");
        nimcp_platform_mutex_unlock(&g_global_pool_mutex);
        return true;
    }

    g_global_pool = nimcp_gpu_pool_create(ctx, config);

    nimcp_platform_mutex_unlock(&g_global_pool_mutex);

    return g_global_pool != NULL;
}

void nimcp_gpu_pool_manager_shutdown(void) {
    if (!g_global_pool_mutex_initialized) return;

    nimcp_platform_mutex_lock(&g_global_pool_mutex);

    if (g_global_pool) {
        nimcp_gpu_pool_destroy(g_global_pool);
        g_global_pool = NULL;
    }

    nimcp_platform_mutex_unlock(&g_global_pool_mutex);
}

nimcp_gpu_pool_t* nimcp_gpu_pool_manager_get(void) {
    return g_global_pool;
}

void* nimcp_gpu_pool_manager_alloc(size_t size) {
    if (!g_global_pool) return NULL;
    return nimcp_gpu_pool_alloc(g_global_pool, size, 0, NULL);
}

void nimcp_gpu_pool_manager_free(void* ptr) {
    if (!g_global_pool) return;
    nimcp_gpu_pool_free(g_global_pool, ptr);
}

//=============================================================================
// Debug and Diagnostic Functions
//=============================================================================

void nimcp_gpu_pool_dump_allocations(const nimcp_gpu_pool_t* pool) {
    if (!nimcp_gpu_pool_is_valid(pool)) {
        printf("GPU Pool: Invalid\n");
        return;
    }

    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)pool->mutex);

    printf("GPU Pool Allocations (%zu blocks):\n", pool->num_blocks);
    printf("  %-4s  %-16s  %-12s  %-10s  %s\n",
           "ID", "Address", "Size", "Age(ms)", "Tag");
    printf("  %-4s  %-16s  %-12s  %-10s  %s\n",
           "----", "----------------", "------------", "----------", "---");

    uint64_t now = get_time_ns();
    nimcp_gpu_block_t* block = pool->blocks_head;

    while (block) {
        uint64_t age_ms = (now - block->alloc_time) / 1000000;
        printf("  %-4llu  %p  %-12zu  %-10llu  %s\n",
               (unsigned long long)block->alloc_id,
               block->ptr,
               block->size,
               (unsigned long long)age_ms,
               block->tag ? block->tag : "-");
        block = block->next;
    }

    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)pool->mutex);
}

size_t nimcp_gpu_pool_check_leaks(const nimcp_gpu_pool_t* pool) {
    if (!nimcp_gpu_pool_is_valid(pool)) return 0;
    return pool->num_blocks;
}

bool nimcp_gpu_pool_validate(const nimcp_gpu_pool_t* pool) {
    if (!pool) return false;
    if (pool->magic != NIMCP_GPU_POOL_MAGIC) return false;
    if (!pool->allocator) return false;
    if (!pool->initialized) return false;

    // Validate allocator magic
    if (pool->allocator->magic != BUMP_ALLOCATOR_MAGIC &&
        pool->allocator->magic != BUDDY_ALLOCATOR_MAGIC &&
        pool->allocator->magic != SLAB_ALLOCATOR_MAGIC) {
        return false;
    }

    return true;
}
