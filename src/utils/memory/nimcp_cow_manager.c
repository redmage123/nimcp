#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_cow_manager.c - Copy-on-Write Manager Implementation
//=============================================================================
/**
 * @file nimcp_cow_manager.c
 * @brief CoW manager with reference counting and lazy copy implementation
 *
 * WHAT: Implements Copy-on-Write with O(1) reference and O(m) lazy copy
 * WHY:  Enable 10x memory savings for sparse object usage patterns
 * HOW:  Atomic refcounting + lazy memcpy/pool allocation + state tracking
 *
 * THREAD SAFETY:
 * - Atomic operations for refcount
 * - Mutex for copy operations
 * - Lock-free reads of shared data
 *
 * MEMORY LAYOUT:
 *
 *   Manager:
 *   ┌─────────────────────────────────────┐
 *   │ config (size, pool, callbacks)      │
 *   │ template_data*  (shared)            │
 *   │ template_refcount (atomic)          │
 *   │ active_handles list                 │
 *   │ statistics                          │
 *   │ mutex                               │
 *   └─────────────────────────────────────┘
 *
 *   Handle (per user):
 *   ┌─────────────────────────────────────┐
 *   │ manager* (back pointer)             │
 *   │ data* (shared or private)           │
 *   │ state (SHARED/PRIVATE/INVALID)      │
 *   │ next* (linked list)                 │
 *   └─────────────────────────────────────┘
 *
 * SRP: This module has ONE responsibility - CoW memory management
 *
 * @author NIMCP Development Team
 * @date 2025-11-21
 * @version 1.0.0
 */

#include "utils/memory/nimcp_cow_manager.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/validation/nimcp_common.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <time.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(cow_manager)

//=============================================================================
// Recursion Guard for NIMCP_THROW_TO_IMMUNE
//=============================================================================
/**
 * WHY:  If exception creation triggers CoW memory operations, infinite
 *       recursion can occur. This thread-local guard prevents re-entry.
 * HOW:  _Atomic prevents compiler reordering if longjmp occurs during throw.
 *       memory_order_relaxed is sufficient for single-thread access.
 */
static _Thread_local _Atomic bool g_cow_in_throw = false;

#define COW_SAFE_THROW(code, msg) \
    do { \
        if (!atomic_load_explicit(&g_cow_in_throw, memory_order_relaxed)) { \
            atomic_store_explicit(&g_cow_in_throw, true, memory_order_relaxed); \
            NIMCP_THROW_TO_IMMUNE(code, msg); \
            atomic_store_explicit(&g_cow_in_throw, false, memory_order_relaxed); \
        } \
    } while(0)

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief CoW handle structure
 */
struct cow_handle_struct {
    struct cow_manager_struct* manager;  /**< Back pointer to manager */
    void* data;                          /**< Pointer to data (shared or private) */
    cow_state_t state;                   /**< Current state */
    bool owns_data;                      /**< true if private copy owned by handle */
    struct cow_handle_struct* next;      /**< Next in manager's handle list */
};

/**
 * @brief CoW manager structure
 */
struct cow_manager_struct {
    // Configuration
    size_t data_size;
    memory_pool_t pool;
    cow_copy_fn copy_fn;
    cow_destructor_fn dtor_fn;
    void* user_data;
    bool enable_tracking;

    // Template data
    void* template_data;
    atomic_size_t template_refcount;  /**< Atomic reference count */

    // Handle tracking
    struct cow_handle_struct* handles;  /**< Linked list of active handles */
    size_t handle_count;

    // Statistics
    cow_stats_t stats;

    // Thread safety
    nimcp_platform_mutex_t mutex;
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
 * @brief Default copy function (memcpy)
 */
static bool default_copy_fn(void* dest, const void* src, size_t size, void* user_data) {
    (void)user_data;
    if (!dest || !src) {
        COW_SAFE_THROW(NIMCP_ERROR_NULL_POINTER, "default_copy_fn: required parameter is NULL (dest, src)");
        return false;
    }
    memcpy(dest, src, size);
    return true;
}

/**
 * @brief Add handle to manager's tracking list
 */
static void add_handle_to_list(cow_manager_t manager, cow_handle_t handle) {
    nimcp_platform_mutex_lock(&manager->mutex);
    handle->next = manager->handles;
    manager->handles = handle;
    manager->handle_count++;
    nimcp_platform_mutex_unlock(&manager->mutex);
}

/**
 * @brief Remove handle from manager's tracking list
 */
static void remove_handle_from_list(cow_manager_t manager, cow_handle_t handle) {
    nimcp_platform_mutex_lock(&manager->mutex);

    struct cow_handle_struct** current = &manager->handles;
    while (*current) {
        if (*current == handle) {
            *current = handle->next;
            manager->handle_count--;
            break;
        }
        current = &(*current)->next;
    }

    nimcp_platform_mutex_unlock(&manager->mutex);
}

//=============================================================================
// CoW Manager API Implementation
//=============================================================================

NIMCP_EXPORT cow_manager_t cow_manager_create(
    const cow_manager_config_t* config,
    const void* template_data
) {
    if (!config || config->data_size == 0) {
        COW_SAFE_THROW(NIMCP_ERROR_INVALID_PARAM, "cow_manager_create: config is NULL or data_size is 0");
        return NULL;
    }

    // Allocate manager (use nimcp_calloc for tracking)
    cow_manager_t manager = nimcp_calloc(1, sizeof(struct cow_manager_struct));
    if (!manager) {

        COW_SAFE_THROW(NIMCP_ERROR_NO_MEMORY, "manager is NULL");

        return NULL;

    }

    // Copy configuration
    manager->data_size = config->data_size;
    manager->pool = config->pool;
    manager->copy_fn = config->copy_fn ? config->copy_fn : default_copy_fn;
    manager->dtor_fn = config->dtor_fn;
    manager->user_data = config->user_data;
    manager->enable_tracking = config->enable_tracking;

    // Allocate and copy template data
    manager->template_data = nimcp_malloc(config->data_size);
    if (!manager->template_data) {
        nimcp_free(manager);
        COW_SAFE_THROW(NIMCP_ERROR_NO_MEMORY, "cow_manager_create: manager->template_data is NULL");
        return NULL;
    }

    if (template_data) {
        memcpy(manager->template_data, template_data, config->data_size);
    } else {
        memset(manager->template_data, 0, config->data_size);
    }

    // Initialize refcount to 0 (no handles yet)
    atomic_init(&manager->template_refcount, 0);

    // Initialize handle list
    manager->handles = NULL;
    manager->handle_count = 0;

    // Initialize statistics
    memset(&manager->stats, 0, sizeof(cow_stats_t));

    // Initialize mutex
    if (nimcp_platform_mutex_init(&manager->mutex, false) != 0) {
        nimcp_free(manager->template_data);
        nimcp_free(manager);
        COW_SAFE_THROW(NIMCP_ERROR_NOT_INITIALIZED, "cow_manager_create: validation failed");
        return NULL;
    }

    return manager;
}

NIMCP_EXPORT void cow_manager_destroy(cow_manager_t manager) {
    if (!manager) return;

    // Check for active handles
    if (manager->handle_count > 0) {
        // WARNING: Handles still active
        // In production, might want to force cleanup or assert
    }

    // Destroy mutex
    nimcp_platform_mutex_destroy(&manager->mutex);

    // Free template data
    if (manager->template_data) {
        if (manager->dtor_fn) {
            manager->dtor_fn(manager->template_data, manager->user_data);
        }
        nimcp_free(manager->template_data);
    }

    // Free manager
    nimcp_free(manager);
}

NIMCP_EXPORT cow_handle_t cow_acquire(cow_manager_t manager) {
    if (!manager) {

        COW_SAFE_THROW(NIMCP_ERROR_NULL_POINTER, "manager is NULL");

        return NULL;

    }

    // Allocate handle (use nimcp_calloc for tracking)
    cow_handle_t handle = nimcp_calloc(1, sizeof(struct cow_handle_struct));
    if (!handle) {

        COW_SAFE_THROW(NIMCP_ERROR_NO_MEMORY, "handle is NULL");

        return NULL;

    }

    // Initialize handle
    handle->manager = manager;
    handle->data = manager->template_data;  // Point to shared template
    handle->state = COW_STATE_SHARED;
    handle->owns_data = false;
    handle->next = NULL;

    // Increment template refcount (atomic)
    atomic_fetch_add(&manager->template_refcount, 1);

    // Add to handle list
    add_handle_to_list(manager, handle);

    // Update statistics
    if (manager->enable_tracking) {
        nimcp_platform_mutex_lock(&manager->mutex);
        manager->stats.total_handles++;
        manager->stats.active_shared++;
        manager->stats.template_refcount = atomic_load(&manager->template_refcount);
        nimcp_platform_mutex_unlock(&manager->mutex);
    }

    return handle;
}

NIMCP_EXPORT void cow_release(cow_handle_t handle) {
    if (!handle) return;

    cow_manager_t manager = handle->manager;
    if (!manager) return;

    // If private, free private copy
    if (handle->state == COW_STATE_PRIVATE && handle->owns_data && handle->data) {
        if (manager->dtor_fn) {
            manager->dtor_fn(handle->data, manager->user_data);
        }

        // Return to pool or free
        if (manager->pool && memory_pool_owns(manager->pool, handle->data)) {
            memory_pool_release(manager->pool, handle->data);
        } else {
            nimcp_free(handle->data);
        }

        // Update statistics
        if (manager->enable_tracking) {
            nimcp_platform_mutex_lock(&manager->mutex);
            manager->stats.active_private--;
            nimcp_platform_mutex_unlock(&manager->mutex);
        }
    }

    // If shared, decrement refcount
    if (handle->state == COW_STATE_SHARED) {
        atomic_fetch_sub(&manager->template_refcount, 1);

        // Update statistics
        if (manager->enable_tracking) {
            nimcp_platform_mutex_lock(&manager->mutex);
            manager->stats.active_shared--;
            manager->stats.template_refcount = atomic_load(&manager->template_refcount);
            nimcp_platform_mutex_unlock(&manager->mutex);
        }
    }

    // Remove from handle list
    remove_handle_from_list(manager, handle);

    // Mark invalid and free
    handle->state = COW_STATE_INVALID;
    nimcp_free(handle);
}

NIMCP_EXPORT const void* cow_read(cow_handle_t handle) {
    if (!handle || handle->state == COW_STATE_INVALID) {
        COW_SAFE_THROW(NIMCP_ERROR_NULL_POINTER, "cow_read: handle is NULL");
        return NULL;
    }
    return handle->data;  // Lock-free read
}

NIMCP_EXPORT void* cow_write(cow_handle_t handle) {
    if (!handle || handle->state == COW_STATE_INVALID) {
        COW_SAFE_THROW(NIMCP_ERROR_NULL_POINTER, "cow_write: handle is NULL");
        return NULL;
    }

    // If already private, return immediately
    if (handle->state == COW_STATE_PRIVATE) {
        return handle->data;
    }

    // Trigger CoW copy
    cow_manager_t manager = handle->manager;
    if (!manager) {

        COW_SAFE_THROW(NIMCP_ERROR_NULL_POINTER, "manager is NULL");

        return NULL;

    }

    nimcp_platform_mutex_lock(&manager->mutex);

    // Double-check state (another thread might have copied)
    if (handle->state == COW_STATE_PRIVATE) {
        nimcp_platform_mutex_unlock(&manager->mutex);
        return handle->data;
    }

    // Measure copy time
    uint64_t start_time = manager->enable_tracking ? get_time_ns() : 0;

    // Allocate private copy (from pool if available)
    void* private_data = NULL;
    if (manager->pool) {
        private_data = memory_pool_acquire(manager->pool);
    }
    if (!private_data) {
        private_data = nimcp_malloc(manager->data_size);
    }

    if (!private_data) {
        if (manager->enable_tracking) {
            manager->stats.failed_copies++;
        }
        nimcp_platform_mutex_unlock(&manager->mutex);
        COW_SAFE_THROW(NIMCP_ERROR_NO_MEMORY, "cow_write: validation failed");
        return NULL;
    }

    // Copy data from template
    bool copy_success = manager->copy_fn(
        private_data,
        manager->template_data,
        manager->data_size,
        manager->user_data
    );

    if (!copy_success) {
        // Cleanup on copy failure
        if (manager->pool && memory_pool_owns(manager->pool, private_data)) {
            memory_pool_release(manager->pool, private_data);
        } else {
            nimcp_free(private_data);
        }
        if (manager->enable_tracking) {
            manager->stats.failed_copies++;
        }
        nimcp_platform_mutex_unlock(&manager->mutex);
        COW_SAFE_THROW(NIMCP_ERROR_NULL_POINTER, "cow_write: validation failed");
        return NULL;
    }

    // Update handle state
    atomic_fetch_sub(&manager->template_refcount, 1);  // No longer using template
    handle->data = private_data;
    handle->state = COW_STATE_PRIVATE;
    handle->owns_data = true;

    // Update statistics
    if (manager->enable_tracking) {
        uint64_t end_time = get_time_ns();
        manager->stats.active_shared--;
        manager->stats.active_private++;
        manager->stats.cow_triggers++;
        manager->stats.template_refcount = atomic_load(&manager->template_refcount);
        manager->stats.total_copy_time_ns += (end_time - start_time);
    }

    nimcp_platform_mutex_unlock(&manager->mutex);

    return private_data;
}

NIMCP_EXPORT bool cow_is_shared(cow_handle_t handle) {
    if (!handle) {
        return false;
    }
    return handle->state == COW_STATE_SHARED;
}

NIMCP_EXPORT cow_state_t cow_get_state(cow_handle_t handle) {
    if (!handle) return COW_STATE_INVALID;
    return handle->state;
}

NIMCP_EXPORT bool cow_make_private(cow_handle_t handle) {
    // Just trigger cow_write but discard result
    return cow_write(handle) != NULL;
}

NIMCP_EXPORT size_t cow_get_refcount(cow_manager_t manager) {
    if (!manager) return 0;
    return atomic_load(&manager->template_refcount);
}

NIMCP_EXPORT size_t cow_get_handle_count(cow_manager_t manager) {
    if (!manager) return 0;
    nimcp_platform_mutex_lock(&manager->mutex);
    size_t count = manager->handle_count;
    nimcp_platform_mutex_unlock(&manager->mutex);
    return count;
}

NIMCP_EXPORT bool cow_get_stats(cow_manager_t manager, cow_stats_t* stats) {
    if (!manager || !stats) {
        COW_SAFE_THROW(NIMCP_ERROR_NULL_POINTER, "cow_get_stats: required parameter is NULL (manager, stats)");
        return false;
    }

    nimcp_platform_mutex_lock(&manager->mutex);

    // Copy statistics
    memcpy(stats, &manager->stats, sizeof(cow_stats_t));

    // Calculate memory savings
    size_t private_copies = manager->stats.active_private;
    size_t shared_users = manager->stats.active_shared;
    stats->memory_saved_bytes = shared_users * manager->data_size;

    nimcp_platform_mutex_unlock(&manager->mutex);

    return true;
}

NIMCP_EXPORT void cow_reset_stats(cow_manager_t manager) {
    if (!manager) return;

    nimcp_platform_mutex_lock(&manager->mutex);

    // Keep handle counts, reset performance metrics
    manager->stats.total_handles = 0;
    manager->stats.cow_triggers = 0;
    manager->stats.failed_copies = 0;
    manager->stats.total_copy_time_ns = 0;

    nimcp_platform_mutex_unlock(&manager->mutex);
}

NIMCP_EXPORT bool cow_calculate_memory_usage(
    cow_manager_t manager,
    size_t* shared_bytes,
    size_t* private_bytes,
    size_t* overhead_bytes
) {
    if (!manager) {
        COW_SAFE_THROW(NIMCP_ERROR_NULL_POINTER, "cow_calculate_memory_usage: manager is NULL");
        return false;
    }

    nimcp_platform_mutex_lock(&manager->mutex);

    if (shared_bytes) {
        *shared_bytes = manager->data_size;  // One template
    }

    if (private_bytes) {
        *private_bytes = manager->stats.active_private * manager->data_size;
    }

    if (overhead_bytes) {
        // Manager overhead + per-handle overhead
        *overhead_bytes = sizeof(struct cow_manager_struct) +
                          manager->handle_count * sizeof(struct cow_handle_struct);
    }

    nimcp_platform_mutex_unlock(&manager->mutex);

    return true;
}
