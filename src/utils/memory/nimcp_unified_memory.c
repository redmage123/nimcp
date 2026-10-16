#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_unified_memory.c - Unified Memory Manager Implementation
//=============================================================================
/**
 * @file nimcp_unified_memory.c
 * @brief Unified memory management with automatic CoW strategy selection
 *
 * WHAT: Single API for memory allocation with Copy-on-Write support
 * WHY:  Simplify memory management across brain modules
 * HOW:  Abstracts object-level and page-level CoW behind unified interface
 *
 * IMPLEMENTATION NOTES:
 *
 * 1. Strategy Selection:
 *    - AUTO: size < page_threshold → object CoW, else → page CoW
 *    - Forced: Use specified strategy regardless of size
 *
 * 2. Memory Pools:
 *    - Object pool: For small allocations and object-level CoW
 *    - Page pool: Optional, for page-level CoW allocations
 *
 * 3. Handle Types:
 *    - Each handle wraps underlying cow_handle_t or page_cow_view_t
 *    - Unified operations dispatch to appropriate implementation
 *
 * THREAD SAFETY:
 * - Manager-level mutex for allocation tracking
 * - Underlying CoW systems handle their own synchronization
 *
 * SRP: This module has ONE responsibility - unified memory management
 *
 * @author NIMCP Development Team
 * @date 2025-11-27
 * @version 1.0.0
 */

#include "utils/memory/nimcp_unified_memory.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/exception/nimcp_exception.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/mman.h>
#include "utils/logging/nimcp_logging.h"

#define LOG_MODULE "MEMORY"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(unified_memory)

//=============================================================================
// Recursion-Safe Throw for Unified Memory Module
//=============================================================================
/**
 * WHAT: Thread-local atomic guard preventing infinite recursion when throwing
 * WHY:  NIMCP_THROW_TO_IMMUNE -> nimcp_exception_create -> nimcp_calloc ->
 *       unified_mem_alloc -> throw -> ... infinite loop
 *       The guard breaks this cycle: the first failure reports to immune,
 *       but recursive calls during exception allocation skip the throw.
 *
 * WHY _Atomic + _Thread_local:
 *   _Thread_local ensures each thread has its own copy (no cross-thread interference).
 *   _Atomic prevents compiler reordering of the flag set/clear operations, which is
 *   critical if NIMCP_THROW_TO_IMMUNE triggers a longjmp - without atomic semantics,
 *   the compiler could cache the flag in a register and never write it back before the
 *   throw, leaving the flag permanently set (silently suppressing all future throws).
 *   memory_order_relaxed is sufficient because we only access within a single thread.
 */
static _Thread_local _Atomic bool g_umm_throw_active = false;

#define UMM_SAFE_THROW(code, fmt, ...)                                   \
    do {                                                                  \
        if (!atomic_load_explicit(&g_umm_throw_active, memory_order_relaxed) \
            && nimcp_exception_system_is_initialized()) {                 \
            atomic_store_explicit(&g_umm_throw_active, true, memory_order_relaxed); \
            NIMCP_THROW_TO_IMMUNE(code, fmt, ##__VA_ARGS__);             \
            atomic_store_explicit(&g_umm_throw_active, false, memory_order_relaxed); \
        }                                                                \
    } while (0)

//=============================================================================
// Constants and Magic Numbers
//=============================================================================

#define UNIFIED_MANAGER_MAGIC 0x554D4D47  // 'UMMG'
#define UNIFIED_HANDLE_MAGIC  0x554D4844  // 'UMHD'
#define UNIFIED_SNAP_MAGIC    0x554D534E  // 'UMSN'

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Page pool for page-level CoW allocations
 *
 * Uses a simple free-list of pre-allocated pages
 */
typedef struct {
    void* base;                     /**< Base address of all pages */
    size_t num_pages;               /**< Total pages */
    size_t free_count;              /**< Free pages available */
    void** free_list;               /**< Free page pointers */
    nimcp_platform_mutex_t mutex;   /**< Pool lock */
    bool initialized;               /**< Is pool initialized? */
} page_pool_t;

/**
 * @brief Unified memory manager structure
 */
struct unified_mem_manager_struct {
    uint32_t magic;                 /**< Magic number for validation */

    // Configuration
    unified_mem_config_t config;

    // Pools
    memory_pool_t object_pool;      /**< Pool for object-level CoW */
    page_pool_t page_pool;          /**< Optional pool for page allocations */

    // Handle tracking
    struct unified_mem_handle_struct* handles; /**< Linked list of handles */
    size_t handle_count;

    // Statistics
    unified_mem_stats_t stats;

    // Thread safety
    nimcp_platform_mutex_t mutex;
};

/**
 * @brief Unified memory handle structure
 */
struct unified_mem_handle_struct {
    uint32_t magic;                 /**< Magic number */

    // Manager reference
    struct unified_mem_manager_struct* manager;

    // Strategy and state
    unified_mem_strategy_t strategy;
    unified_mem_state_t state;

    // Allocation metadata
    size_t size;                    /**< Original allocation size */
    bool cow_enabled;               /**< Was CoW requested? */

    // Strategy-specific data (union for memory efficiency)
    union {
        // Object-level CoW
        struct {
            cow_manager_t cow_mgr;  /**< CoW manager for this allocation */
            cow_handle_t cow_handle; /**< Current handle */
        } object;

        // Page-level CoW
        struct {
            page_cow_region_t region; /**< Region for this allocation */
            page_cow_view_t view;     /**< Current view */
        } page;

        // Direct allocation
        struct {
            void* data;             /**< Direct pointer */
            bool from_pool;         /**< Is from pool? */
        } direct;
    } impl;

    // Linked list
    struct unified_mem_handle_struct* next;
};

/**
 * @brief Unified memory snapshot structure
 */
struct unified_mem_snapshot_struct {
    uint32_t magic;                 /**< Magic number */
    struct unified_mem_handle_struct* source; /**< Source handle */
    unified_mem_strategy_t strategy; /**< Strategy of source */
    size_t size;                    /**< Size of snapshot data */
    void* data_copy;                /**< Full copy for object/direct strategies */

    union {
        struct {
            page_cow_snapshot_t snap;
        } page;
    } impl;
};

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Get current time in nanoseconds
 */
static inline uint64_t get_time_ns(void) {

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * NIMCP_NS_PER_SEC + (uint64_t)ts.tv_nsec;
}

/**
 * @brief Initialize page pool
 */
static bool page_pool_init(page_pool_t* pool, size_t num_pages) {


    if (!pool || num_pages == 0) return false;

    // Check for integer overflow before multiplication
    if (num_pages > SIZE_MAX / PAGE_COW_PAGE_SIZE) return false;

    // Allocate pages via mmap
    size_t total_size = num_pages * PAGE_COW_PAGE_SIZE;
    pool->base = mmap(NULL, total_size, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (pool->base == MAP_FAILED) {
    
        pool->base = NULL;
    
        return false;
    }

    pool->num_pages = num_pages;
    pool->free_count = num_pages;

    // Allocate free list
    // Use raw calloc to avoid circular dependency with nimcp_* wrappers
    pool->free_list = calloc(num_pages, sizeof(void*));
    if (!pool->free_list) {
    
        munmap(pool->base, total_size);
        pool->base = NULL;
    
        return false;
    }

    // Initialize free list with all pages
    for (size_t i = 0; i < num_pages; i++) {
    
        pool->free_list[i] = (char*)pool->base + i * PAGE_COW_PAGE_SIZE;
    }

    // Initialize mutex
    if (nimcp_platform_mutex_init(&pool->mutex, false) != 0) {
        free(pool->free_list);
        munmap(pool->base, total_size);
        pool->base = NULL;
        return false;
    }

    pool->initialized = true;
    return true;
}

/**
 * @brief Destroy page pool
 */
static void page_pool_destroy(page_pool_t* pool) {

    if (!pool || !pool->initialized) return;

    nimcp_platform_mutex_destroy(&pool->mutex);

    free(pool->free_list);

    if (pool->base) {
    
        munmap(pool->base, pool->num_pages * PAGE_COW_PAGE_SIZE);
    }

    memset(pool, 0, sizeof(page_pool_t));
}

/**
 * @brief Acquire page from pool
 */
static void* page_pool_acquire(page_pool_t* pool) {
    if (!pool || !pool->initialized) return NULL;

    nimcp_platform_mutex_lock(&pool->mutex);

    void* page = NULL;
    if (pool->free_count > 0) {
    
        page = pool->free_list[--pool->free_count];
    }

    nimcp_platform_mutex_unlock(&pool->mutex);
    return page;
}

/**
 * @brief Release page back to pool
 */
static bool page_pool_release(page_pool_t* pool, void* page) {


    if (!pool || !pool->initialized || !page) return false;

    // Verify page is from this pool
    if (page < pool->base ||
        page >= (void*)((char*)pool->base + pool->num_pages * PAGE_COW_PAGE_SIZE)) {
    
        return false;
    }

    nimcp_platform_mutex_lock(&pool->mutex);

    if (pool->free_count < pool->num_pages) {
    
        pool->free_list[pool->free_count++] = page;
    }

    nimcp_platform_mutex_unlock(&pool->mutex);
    return true;
}

/**
 * @brief Check if page is from pool
 */
static bool page_pool_owns(page_pool_t* pool, void* page) {


    if (!pool || !pool->initialized || !page) return false;

    return page >= pool->base &&
           page < (void*)((char*)pool->base + pool->num_pages * PAGE_COW_PAGE_SIZE);
}

/**
 * @brief Select strategy based on size and config
 */
static unified_mem_strategy_t select_strategy(
    unified_mem_manager_t manager,
    const unified_mem_request_t* request
) {
    if (request->strategy != UNIFIED_STRATEGY_AUTO) {
    
        return request->strategy;
    }

    // Auto-select based on size and CoW requirement
    if (!request->enable_cow) {
    
        // No CoW - use direct pool if possible
        return UNIFIED_STRATEGY_POOL_DIRECT;
    }

    // CoW enabled - select based on size
    if (request->size >= manager->config.page_threshold) {
    
        return UNIFIED_STRATEGY_PAGE_COW;
    }

    return UNIFIED_STRATEGY_OBJECT_COW;
}

/**
 * @brief Add handle to manager's tracking list
 */
static void add_handle_to_list(
    unified_mem_manager_t manager,
    unified_mem_handle_t handle
) {
    handle->next = manager->handles;
    manager->handles = handle;
    manager->handle_count++;
}

/**
 * @brief Remove handle from manager's tracking list
 */
static void remove_handle_from_list(
    unified_mem_manager_t manager,
    unified_mem_handle_t handle
) {
    struct unified_mem_handle_struct** current = &manager->handles;
    while (*current) {
    
        if (*current == handle) {
        
            *current = handle->next;
            manager->handle_count--;
            break;
        }
        current = &(*current)->next;
    }
}

//=============================================================================
// Unified Memory Manager API Implementation
//=============================================================================

NIMCP_EXPORT unified_mem_config_t unified_mem_default_config(void) {
    unified_mem_config_t config = {
        .page_threshold = UNIFIED_MEM_PAGE_THRESHOLD,
        .object_pool_block_size = 0,  // Auto
        .object_pool_num_blocks = 1024,
        .page_pool_num_pages = 0,  // Disabled by default
        .default_strategy = UNIFIED_STRATEGY_AUTO,
        .enable_cow = true,
        .enable_tracking = true,
        .user_data = NULL
    };
    return config;
}

NIMCP_EXPORT unified_mem_manager_t unified_mem_create(
    const unified_mem_config_t* config
) {
    unified_mem_config_t cfg = config ? *config : unified_mem_default_config();

    // Allocate manager
    unified_mem_manager_t manager = calloc(1, sizeof(struct unified_mem_manager_struct));
    if (!manager) {
        UMM_SAFE_THROW(NIMCP_ERROR_NO_MEMORY, "unified_mem_create: manager allocation failed");
        return NULL;
    }

    manager->magic = UNIFIED_MANAGER_MAGIC;
    manager->config = cfg;

    // Initialize mutex
    if (nimcp_platform_mutex_init(&manager->mutex, false) != 0) {
        free(manager);
        return NULL;
    }

    // Initialize page-level CoW subsystem
    if (!page_cow_init()) {
        // Non-fatal - page CoW may already be initialized
    }

    // Initialize page pool if requested
    if (cfg.page_pool_num_pages > 0) {
    
        if (!page_pool_init(&manager->page_pool, cfg.page_pool_num_pages)) {
            // Non-fatal - will fall back to mmap
        }
    }

    // Initialize statistics
    memset(&manager->stats, 0, sizeof(unified_mem_stats_t));

    return manager;
}

NIMCP_EXPORT void unified_mem_destroy(unified_mem_manager_t manager) {
    if (!manager || manager->magic != UNIFIED_MANAGER_MAGIC) return;

    // Detach handle list under lock so no concurrent access races with us.
    // We can't hold the lock while calling unified_mem_free (it re-acquires),
    // so snapshot the list head and clear the manager's list atomically.
    nimcp_platform_mutex_lock(&manager->mutex);
    struct unified_mem_handle_struct* pending = manager->handles;
    manager->handles = NULL;
    manager->handle_count = 0;
    nimcp_platform_mutex_unlock(&manager->mutex);

    // Force cleanup of remaining handles (now safe: list is detached)
    while (pending) {
        struct unified_mem_handle_struct* next = pending->next;
        unified_mem_free(pending);
        pending = next;
    }

    // Destroy page pool
    page_pool_destroy(&manager->page_pool);

    // Destroy object pool if we created one
    if (manager->object_pool) {
    
        memory_pool_destroy(manager->object_pool);
    }

    // Destroy mutex
    nimcp_platform_mutex_destroy(&manager->mutex);

    // Invalidate and free
    manager->magic = 0;
    free(manager);
}

NIMCP_EXPORT bool unified_mem_get_stats(
    unified_mem_manager_t manager,
    unified_mem_stats_t* stats
) {
    if (!manager || manager->magic != UNIFIED_MANAGER_MAGIC || !stats) {
    
    
        return false;
    }

    nimcp_platform_mutex_lock(&manager->mutex);
    memcpy(stats, &manager->stats, sizeof(unified_mem_stats_t));
    stats->active_handles = manager->handle_count;

    // Calculate pool utilization
    if (manager->page_pool.initialized) {
    
        size_t used = manager->page_pool.num_pages - manager->page_pool.free_count;
        stats->page_pool_utilization = (used * 10000) / manager->page_pool.num_pages;
    }

    nimcp_platform_mutex_unlock(&manager->mutex);
    return true;
}

NIMCP_EXPORT void unified_mem_reset_stats(unified_mem_manager_t manager) {
    if (!manager || manager->magic != UNIFIED_MANAGER_MAGIC) return;

    nimcp_platform_mutex_lock(&manager->mutex);

    // Reset counters but keep current state
    manager->stats.total_allocations = 0;
    manager->stats.cow_triggers = 0;
    manager->stats.total_alloc_time_ns = 0;
    manager->stats.total_cow_time_ns = 0;

    nimcp_platform_mutex_unlock(&manager->mutex);
}

//=============================================================================
// Unified Memory Handle API Implementation
//=============================================================================

NIMCP_EXPORT unified_mem_handle_t unified_mem_alloc(
    unified_mem_manager_t manager,
    const unified_mem_request_t* request
) {
    if (!manager || manager->magic != UNIFIED_MANAGER_MAGIC || !request) {
    
        LOG_ERROR("Allocation or operation returned NULL");
        return NULL;
    }
    if (request->size == 0) return NULL;

    uint64_t start_time = manager->config.enable_tracking ? get_time_ns() : 0;

    // Allocate handle structure
    unified_mem_handle_t handle = calloc(1, sizeof(struct unified_mem_handle_struct));
    if (!handle) {
        UMM_SAFE_THROW(NIMCP_ERROR_NO_MEMORY, "unified_mem_alloc: handle allocation failed");
        return NULL;
    }

    handle->magic = UNIFIED_HANDLE_MAGIC;
    handle->manager = manager;
    handle->size = request->size;
    handle->cow_enabled = request->enable_cow;

    // Select and apply strategy
    handle->strategy = select_strategy(manager, request);

    nimcp_platform_mutex_lock(&manager->mutex);

    bool success = false;

    switch (handle->strategy) {
        case UNIFIED_STRATEGY_OBJECT_COW: {
            // Create CoW manager for this allocation
            cow_manager_config_t cow_cfg = {
                .data_size = request->size,
                .pool = NULL,  // Could use object_pool here
                .copy_fn = NULL,
                .dtor_fn = NULL,
                .user_data = NULL,
                .enable_tracking = manager->config.enable_tracking
            };

            handle->impl.object.cow_mgr = cow_manager_create(&cow_cfg, request->initial_data);
            if (handle->impl.object.cow_mgr) {
            
                handle->impl.object.cow_handle = cow_acquire(handle->impl.object.cow_mgr);
                if (handle->impl.object.cow_handle) {
                
                    handle->state = UNIFIED_STATE_SHARED;
                    success = true;
                    manager->stats.object_cow_allocations++;
                } else {
                    cow_manager_destroy(handle->impl.object.cow_mgr);
                }
            }
            break;
        }

        case UNIFIED_STRATEGY_PAGE_COW: {
            // Create page-level CoW region
            page_cow_config_t page_cfg = {
                .size = request->size,
                .enable_tracking = manager->config.enable_tracking,
                .zero_on_allocate = (request->initial_data == NULL),
                .max_private_pages = 0
            };

            handle->impl.page.region = page_cow_region_create(&page_cfg, request->initial_data);
            if (handle->impl.page.region) {
            
                handle->impl.page.view = page_cow_view_create(handle->impl.page.region);
                if (handle->impl.page.view) {
                
                    handle->state = UNIFIED_STATE_SHARED;
                    success = true;
                    manager->stats.page_cow_allocations++;
                } else {
                    page_cow_region_destroy(handle->impl.page.region);
                }
            }
            break;
        }

        case UNIFIED_STRATEGY_POOL_DIRECT: {
            // Direct allocation from pool (no CoW)
            // Try page pool first for large allocations
            if (request->size >= PAGE_COW_PAGE_SIZE && manager->page_pool.initialized) {
            
                size_t pages_needed = (request->size + PAGE_COW_PAGE_SIZE - 1) / PAGE_COW_PAGE_SIZE;
                if (pages_needed == 1) {
                
                    handle->impl.direct.data = page_pool_acquire(&manager->page_pool);
                    if (handle->impl.direct.data) {
                    
                        handle->impl.direct.from_pool = true;
                        if (request->initial_data) {
                        
                            memcpy(handle->impl.direct.data, request->initial_data, request->size);
                        } else {
                            memset(handle->impl.direct.data, 0, PAGE_COW_PAGE_SIZE);
                        }
                        handle->state = UNIFIED_STATE_DIRECT;
                        success = true;
                        manager->stats.pool_direct_allocations++;
                    }
                }
            }

            // Fall back to malloc if pool unavailable
            if (!success) {
            
                LOG_TRACE("Memory allocation requested");
                handle->impl.direct.data = malloc(request->size);
                if (handle->impl.direct.data) {
                
                    handle->impl.direct.from_pool = false;
                    if (request->initial_data) {
                    
                        memcpy(handle->impl.direct.data, request->initial_data, request->size);
                    } else {
                        memset(handle->impl.direct.data, 0, request->size);
                    }
                    handle->state = UNIFIED_STATE_DIRECT;
                    success = true;
                    manager->stats.pool_direct_allocations++;
                }
            }
            break;
        }

        case UNIFIED_STRATEGY_MALLOC_DIRECT:
        case UNIFIED_STRATEGY_AUTO:  // Fallback
        default: {
            // Direct malloc allocation
            handle->impl.direct.data = malloc(request->size);
            if (handle->impl.direct.data) {
            
                handle->impl.direct.from_pool = false;
                if (request->initial_data) {
                
                    memcpy(handle->impl.direct.data, request->initial_data, request->size);
                } else {
                    memset(handle->impl.direct.data, 0, request->size);
                }
                handle->state = UNIFIED_STATE_DIRECT;
                handle->strategy = UNIFIED_STRATEGY_MALLOC_DIRECT;
                success = true;
                manager->stats.malloc_direct_allocations++;
            }
            break;
        }
    }

    if (success) {
    
        add_handle_to_list(manager, handle);
        manager->stats.total_allocations++;
        manager->stats.total_memory_bytes += request->size;

        if (handle->state == UNIFIED_STATE_SHARED) {
        
            manager->stats.shared_handles++;
            manager->stats.shared_memory_bytes += request->size;
        }

        if (manager->handle_count > manager->stats.peak_handles) {
        
            manager->stats.peak_handles = manager->handle_count;
        }

        if (manager->config.enable_tracking) {
        
            manager->stats.total_alloc_time_ns += get_time_ns() - start_time;
        }
    } else {
        free(handle);
        handle = NULL;
    }

    nimcp_platform_mutex_unlock(&manager->mutex);
    return handle;
}

NIMCP_EXPORT unified_mem_handle_t unified_mem_clone(unified_mem_handle_t handle) {
    if (!handle || handle->magic != UNIFIED_HANDLE_MAGIC) return NULL;
    if (handle->state == UNIFIED_STATE_INVALID) return NULL;

    unified_mem_manager_t manager = handle->manager;
    if (!manager || manager->magic != UNIFIED_MANAGER_MAGIC) return NULL;

    // Allocate new handle
    unified_mem_handle_t clone = calloc(1, sizeof(struct unified_mem_handle_struct));
    if (!clone) {
        UMM_SAFE_THROW(NIMCP_ERROR_NO_MEMORY, "unified_mem_share: clone allocation failed");
        return NULL;
    }

    clone->magic = UNIFIED_HANDLE_MAGIC;
    clone->manager = manager;
    clone->size = handle->size;
    clone->strategy = handle->strategy;
    clone->cow_enabled = handle->cow_enabled;

    nimcp_platform_mutex_lock(&manager->mutex);

    bool success = false;

    switch (handle->strategy) {
        case UNIFIED_STRATEGY_OBJECT_COW: {
            // Acquire another handle from same CoW manager
            clone->impl.object.cow_mgr = handle->impl.object.cow_mgr;
            clone->impl.object.cow_handle = cow_acquire(clone->impl.object.cow_mgr);
            if (clone->impl.object.cow_handle) {
            
                clone->state = UNIFIED_STATE_SHARED;
                success = true;
            }
            break;
        }

        case UNIFIED_STRATEGY_PAGE_COW: {
            // Clone the view
            clone->impl.page.region = handle->impl.page.region;
            clone->impl.page.view = page_cow_view_clone(handle->impl.page.view);
            if (clone->impl.page.view) {
            
                clone->state = UNIFIED_STATE_SHARED;
                success = true;
            }
            break;
        }

        case UNIFIED_STRATEGY_POOL_DIRECT:
        case UNIFIED_STRATEGY_MALLOC_DIRECT: {
            // Direct allocation - must copy
            clone->impl.direct.data = malloc(handle->size);
            if (clone->impl.direct.data) {
            
                memcpy(clone->impl.direct.data, handle->impl.direct.data, handle->size);
                clone->impl.direct.from_pool = false;
                clone->state = UNIFIED_STATE_DIRECT;
                success = true;
            }
            break;
        }

        default:
            break;
    }

    if (success) {
    
        add_handle_to_list(manager, clone);
        manager->stats.total_allocations++;

        if (clone->state == UNIFIED_STATE_SHARED) {
        
            manager->stats.shared_handles++;
            manager->stats.memory_saved_bytes += handle->size;
        } else {
            manager->stats.total_memory_bytes += handle->size;
        }
    } else {
        free(clone);
        clone = NULL;
    }

    nimcp_platform_mutex_unlock(&manager->mutex);
    return clone;
}

NIMCP_EXPORT void unified_mem_free(unified_mem_handle_t handle) {
    if (!handle || handle->magic != UNIFIED_HANDLE_MAGIC) return;

    unified_mem_manager_t manager = handle->manager;

    /* P2-U14: Capture strategy and pool state while holding the lock to avoid
     * post-unlock access to handle fields that could theoretically race. */
    unified_mem_strategy_t strategy = handle->strategy;
    bool pool_initialized = false;

    if (manager && manager->magic == UNIFIED_MANAGER_MAGIC) {

        nimcp_platform_mutex_lock(&manager->mutex);
        remove_handle_from_list(manager, handle);
        pool_initialized = manager->page_pool.initialized;

        // Update statistics with underflow guards.
        // Without guards, freeing a handle whose state was changed without
        // updating stats (or freeing untracked memory) wraps size_t counters
        // to near-SIZE_MAX, corrupting all subsequent statistics.
        if (handle->state == UNIFIED_STATE_SHARED) {
            if (manager->stats.shared_handles > 0)
                manager->stats.shared_handles--;
            if (manager->stats.shared_memory_bytes >= handle->size)
                manager->stats.shared_memory_bytes -= handle->size;
            else
                manager->stats.shared_memory_bytes = 0;
        } else if (handle->state == UNIFIED_STATE_PRIVATE) {
            if (manager->stats.private_handles > 0)
                manager->stats.private_handles--;
            if (manager->stats.private_memory_bytes >= handle->size)
                manager->stats.private_memory_bytes -= handle->size;
            else
                manager->stats.private_memory_bytes = 0;
        }

        if (manager->stats.total_memory_bytes >= handle->size)
            manager->stats.total_memory_bytes -= handle->size;
        else
            manager->stats.total_memory_bytes = 0;

        nimcp_platform_mutex_unlock(&manager->mutex);
    }

    // Strategy-specific cleanup (using captured strategy)
    switch (strategy) {
        case UNIFIED_STRATEGY_OBJECT_COW: {
            if (handle->impl.object.cow_handle) {
            
                cow_release(handle->impl.object.cow_handle);
            }
            // Note: cow_mgr is shared between original and clones
            // Only destroy when no handles remain (handle_count==0 after cow_release)
            // Using cow_get_handle_count instead of refcount because:
            // - refcount only counts SHARED handles
            // - private handles also need the cow_manager
            size_t handle_count = cow_get_handle_count(handle->impl.object.cow_mgr);
            if (handle_count == 0) {
            
                cow_manager_destroy(handle->impl.object.cow_mgr);
            }
            break;
        }

        case UNIFIED_STRATEGY_PAGE_COW: {
            if (handle->impl.page.view) {
            
                page_cow_view_destroy(handle->impl.page.view);
            }
            // Check if region can be destroyed
            if (handle->impl.page.region) {
            
                page_cow_stats_t stats;
                if (page_cow_region_get_stats(handle->impl.page.region, &stats) &&
                    stats.active_views == 0) {
                    page_cow_region_destroy(handle->impl.page.region);
                }
            }
            break;
        }

        case UNIFIED_STRATEGY_POOL_DIRECT: {
            if (handle->impl.direct.data) {

                if (handle->impl.direct.from_pool && manager && pool_initialized) {
                    page_pool_release(&manager->page_pool, handle->impl.direct.data);
                } else {
                
                    free(handle->impl.direct.data);
                }
            }
            break;
        }

        case UNIFIED_STRATEGY_MALLOC_DIRECT:
        default: {
            if (handle->impl.direct.data) {
            
            
                free(handle->impl.direct.data);
            }
            break;
        }
    }

    handle->magic = 0;
    handle->state = UNIFIED_STATE_INVALID;
    free(handle);
}

NIMCP_EXPORT const void* unified_mem_read(unified_mem_handle_t handle) {
    if (!handle || handle->magic != UNIFIED_HANDLE_MAGIC) return NULL;
    if (handle->state == UNIFIED_STATE_INVALID) return NULL;

    switch (handle->strategy) {

        case UNIFIED_STRATEGY_OBJECT_COW:
            return cow_read(handle->impl.object.cow_handle);

        case UNIFIED_STRATEGY_PAGE_COW:
            return page_cow_view_read(handle->impl.page.view);

        case UNIFIED_STRATEGY_POOL_DIRECT:
        case UNIFIED_STRATEGY_MALLOC_DIRECT:
        default:
            return handle->impl.direct.data;
    }
}

NIMCP_EXPORT void* unified_mem_write(unified_mem_handle_t handle) {
    if (!handle || handle->magic != UNIFIED_HANDLE_MAGIC) return NULL;
    if (handle->state == UNIFIED_STATE_INVALID) return NULL;

    unified_mem_manager_t manager = handle->manager;
    uint64_t start_time = (manager && manager->config.enable_tracking) ? get_time_ns() : 0;
    bool was_shared = (handle->state == UNIFIED_STATE_SHARED);

    void* result = NULL;

    switch (handle->strategy) {
        case UNIFIED_STRATEGY_OBJECT_COW: {
            result = cow_write(handle->impl.object.cow_handle);
            if (result && !cow_is_shared(handle->impl.object.cow_handle)) {
                handle->state = UNIFIED_STATE_PRIVATE;
            }
            break;
        }

        case UNIFIED_STRATEGY_PAGE_COW: {
            result = page_cow_view_write(handle->impl.page.view);
            if (result) {
            
                // Once writable, view is considered private
                // Actual CoW happens lazily on page fault
                handle->state = UNIFIED_STATE_PRIVATE;
            }
            break;
        }

        case UNIFIED_STRATEGY_POOL_DIRECT:
        case UNIFIED_STRATEGY_MALLOC_DIRECT:
        default:
            result = handle->impl.direct.data;
            break;
    }

    // Update statistics if state changed (shared -> private CoW trigger)
    if (manager && manager->magic == UNIFIED_MANAGER_MAGIC && was_shared &&
        handle->state == UNIFIED_STATE_PRIVATE) {
        nimcp_platform_mutex_lock(&manager->mutex);
        manager->stats.cow_triggers++;
        if (manager->stats.shared_handles > 0)
            manager->stats.shared_handles--;
        manager->stats.private_handles++;
        if (manager->stats.shared_memory_bytes >= handle->size)
            manager->stats.shared_memory_bytes -= handle->size;
        else
            manager->stats.shared_memory_bytes = 0;
        manager->stats.private_memory_bytes += handle->size;
        if (manager->stats.memory_saved_bytes >= handle->size)
            manager->stats.memory_saved_bytes -= handle->size;
        else
            manager->stats.memory_saved_bytes = 0;

        if (manager->config.enable_tracking) {
            manager->stats.total_cow_time_ns += get_time_ns() - start_time;
        }
        nimcp_platform_mutex_unlock(&manager->mutex);
    }

    return result;
}

NIMCP_EXPORT bool unified_mem_make_private(unified_mem_handle_t handle) {
    if (!handle || handle->magic != UNIFIED_HANDLE_MAGIC) return false;
    if (handle->state == UNIFIED_STATE_INVALID) return false;
    if (handle->state != UNIFIED_STATE_SHARED) return true;  // Already private

    return unified_mem_write(handle) != NULL;
}

NIMCP_EXPORT bool unified_mem_is_shared(unified_mem_handle_t handle) {
    if (!handle || handle->magic != UNIFIED_HANDLE_MAGIC) return false;
    return handle->state == UNIFIED_STATE_SHARED;
}

NIMCP_EXPORT unified_mem_state_t unified_mem_get_state(unified_mem_handle_t handle) {
    if (!handle || handle->magic != UNIFIED_HANDLE_MAGIC) {
    
        return UNIFIED_STATE_INVALID;
    }
    return handle->state;
}

NIMCP_EXPORT size_t unified_mem_get_size(unified_mem_handle_t handle) {
    if (!handle || handle->magic != UNIFIED_HANDLE_MAGIC) return 0;
    return handle->size;
}

NIMCP_EXPORT unified_mem_strategy_t unified_mem_get_strategy(unified_mem_handle_t handle) {
    if (!handle || handle->magic != UNIFIED_HANDLE_MAGIC) {
    
        return UNIFIED_STRATEGY_AUTO;
    }
    return handle->strategy;
}

NIMCP_EXPORT size_t unified_mem_get_memory_saved(unified_mem_handle_t handle) {
    if (!handle || handle->magic != UNIFIED_HANDLE_MAGIC) return 0;
    if (handle->state != UNIFIED_STATE_SHARED) return 0;

    switch (handle->strategy) {

        case UNIFIED_STRATEGY_OBJECT_COW:
            return handle->size;  // Full size saved

        case UNIFIED_STRATEGY_PAGE_COW:
            return page_cow_view_get_memory_saved(handle->impl.page.view);

        default:
            return 0;
    }
}

//=============================================================================
// Snapshot API Implementation
//=============================================================================

NIMCP_EXPORT unified_mem_snapshot_t unified_mem_snapshot_create(
    unified_mem_handle_t handle
) {
    if (!handle || handle->magic != UNIFIED_HANDLE_MAGIC) return NULL;
    if (handle->state == UNIFIED_STATE_INVALID) return NULL;

    unified_mem_snapshot_t snap = calloc(1, sizeof(struct unified_mem_snapshot_struct));
    if (!snap) {
        UMM_SAFE_THROW(NIMCP_ERROR_NO_MEMORY, "unified_mem_snapshot: snapshot allocation failed");
        return NULL;
    }

    snap->magic = UNIFIED_SNAP_MAGIC;
    snap->source = handle;
    snap->strategy = handle->strategy;
    snap->size = handle->size;
    snap->data_copy = NULL;

    bool success = false;

    switch (handle->strategy) {
        case UNIFIED_STRATEGY_OBJECT_COW: {
            // Full copy for object CoW (cow_acquire doesn't capture current state)
            const void* current_data = cow_read(handle->impl.object.cow_handle);
            if (current_data) {
            
                LOG_TRACE("Memory allocation requested");
                snap->data_copy = malloc(handle->size);
                if (snap->data_copy) {
                
                    memcpy(snap->data_copy, current_data, handle->size);
                    success = true;
                }
            }
            break;
        }

        case UNIFIED_STRATEGY_PAGE_COW: {
            snap->impl.page.snap = page_cow_snapshot_create(handle->impl.page.view);
            if (snap->impl.page.snap) {
            
                success = true;
            }
            break;
        }

        case UNIFIED_STRATEGY_POOL_DIRECT:
        case UNIFIED_STRATEGY_MALLOC_DIRECT: {
            // Full copy for direct allocations
            snap->data_copy = malloc(handle->size);
            if (snap->data_copy) {
            
                memcpy(snap->data_copy, handle->impl.direct.data, handle->size);
                success = true;
            }
            break;
        }

        default:
            break;
    }

    if (!success) {
    
        if (snap->data_copy) {
        
        
            free(snap->data_copy);
        }
        free(snap);
        snap = NULL;
    }

    return snap;
}

NIMCP_EXPORT bool unified_mem_snapshot_restore(
    unified_mem_handle_t handle,
    unified_mem_snapshot_t snapshot
) {
    if (!handle || handle->magic != UNIFIED_HANDLE_MAGIC) return false;
    if (!snapshot || snapshot->magic != UNIFIED_SNAP_MAGIC) return false;
    if (handle != snapshot->source) return false;
    if (handle->strategy != snapshot->strategy) return false;

    switch (handle->strategy) {
        case UNIFIED_STRATEGY_OBJECT_COW: {
            // Get writable pointer and copy snapshot data back
            void* dest = cow_write(handle->impl.object.cow_handle);
            if (dest && snapshot->data_copy) {
            
                memcpy(dest, snapshot->data_copy, snapshot->size);
                return true;
            }
            return false;
        }

        case UNIFIED_STRATEGY_PAGE_COW: {
            return page_cow_snapshot_restore(handle->impl.page.view, snapshot->impl.page.snap);
        }

        case UNIFIED_STRATEGY_POOL_DIRECT:
        case UNIFIED_STRATEGY_MALLOC_DIRECT: {
            if (snapshot->data_copy) {
            
                memcpy(handle->impl.direct.data, snapshot->data_copy, snapshot->size);
                return true;
            }
            return false;
        }

        default:
            return false;
    }
}

NIMCP_EXPORT void unified_mem_snapshot_destroy(unified_mem_snapshot_t snapshot) {
    if (!snapshot || snapshot->magic != UNIFIED_SNAP_MAGIC) return;

    // Free data copy for object/direct strategies
    if (snapshot->data_copy) {
    
    
        free(snapshot->data_copy);
    }

    // Free page CoW snapshot if applicable
    if (snapshot->strategy == UNIFIED_STRATEGY_PAGE_COW && snapshot->impl.page.snap) {
    
        page_cow_snapshot_destroy(snapshot->impl.page.snap);
    }

    snapshot->magic = 0;
    free(snapshot);
}

NIMCP_EXPORT size_t unified_mem_snapshot_get_delta_bytes(
    unified_mem_handle_t handle,
    unified_mem_snapshot_t snapshot
) {
    if (!handle || handle->magic != UNIFIED_HANDLE_MAGIC) return 0;
    if (!snapshot || snapshot->magic != UNIFIED_SNAP_MAGIC) return 0;
    if (handle != snapshot->source) return 0;

    switch (handle->strategy) {
        case UNIFIED_STRATEGY_OBJECT_COW: {
            // Compare current data with snapshot data
            const char* current = cow_read(handle->impl.object.cow_handle);
            const char* snap_data = snapshot->data_copy;
            if (!current || !snap_data) return 0;

            size_t delta = 0;
            for (size_t i = 0; i < handle->size; i++) {
            
                if (current[i] != snap_data[i]) delta++;
            }
            return delta;
        }

        case UNIFIED_STRATEGY_PAGE_COW: {
            size_t delta_pages = page_cow_snapshot_get_delta_pages(
                handle->impl.page.view, snapshot->impl.page.snap);
            return delta_pages * PAGE_COW_PAGE_SIZE;
        }

        case UNIFIED_STRATEGY_POOL_DIRECT:
        case UNIFIED_STRATEGY_MALLOC_DIRECT: {
            // Compare byte-by-byte (expensive but accurate)
            const char* current = handle->impl.direct.data;
            const char* snap_data = snapshot->data_copy;
            if (!current || !snap_data) return 0;

            size_t delta = 0;
            for (size_t i = 0; i < handle->size; i++) {
            
                if (current[i] != snap_data[i]) delta++;
            }
            return delta;
        }

        default:
            return 0;
    }
}

//=============================================================================
// Page Pool Integration API Implementation
//=============================================================================

NIMCP_EXPORT bool unified_mem_enable_page_pool(
    unified_mem_manager_t manager,
    size_t num_pages
) {
    if (!manager || manager->magic != UNIFIED_MANAGER_MAGIC) return false;
    if (num_pages == 0) return false;

    nimcp_platform_mutex_lock(&manager->mutex);

    // Destroy existing pool if any
    if (manager->page_pool.initialized) {
    
        page_pool_destroy(&manager->page_pool);
    }

    bool success = page_pool_init(&manager->page_pool, num_pages);

    nimcp_platform_mutex_unlock(&manager->mutex);
    return success;
}

NIMCP_EXPORT void unified_mem_disable_page_pool(unified_mem_manager_t manager) {
    if (!manager || manager->magic != UNIFIED_MANAGER_MAGIC) return;

    nimcp_platform_mutex_lock(&manager->mutex);
    page_pool_destroy(&manager->page_pool);
    nimcp_platform_mutex_unlock(&manager->mutex);
}

NIMCP_EXPORT bool unified_mem_get_page_pool_stats(
    unified_mem_manager_t manager,
    size_t* total_pages,
    size_t* free_pages
) {
    if (!manager || manager->magic != UNIFIED_MANAGER_MAGIC) return false;
    if (!manager->page_pool.initialized) return false;

    nimcp_platform_mutex_lock(&manager->mutex);

    if (total_pages) *total_pages = manager->page_pool.num_pages;
    if (free_pages) *free_pages = manager->page_pool.free_count;

    nimcp_platform_mutex_unlock(&manager->mutex);
    return true;
}
