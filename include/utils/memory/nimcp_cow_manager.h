//=============================================================================
// nimcp_cow_manager.h - Copy-on-Write Manager for Shared Data
//=============================================================================
/**
 * @file nimcp_cow_manager.h
 * @brief Copy-on-Write manager with reference counting and lazy initialization
 *
 * WHAT: CoW manager for sharing data with lazy private copies
 * WHY:  Enable 10x memory savings when many objects share same initial state
 * HOW:  Reference counting + lazy copy on first write + pool integration
 *
 * ARCHITECTURE:
 *
 *   CoW Shared Data Model:
 *   ┌──────────────────────────────────────────────────┐
 *   │  Template Data (refcount = 3)                    │
 *   │  ┌────────────────────────────────────────────┐  │
 *   │  │  Shared data (read-only for all users)    │  │
 *   │  └────────────────────────────────────────────┘  │
 *   └────────────────┬─────────────┬──────────────────┘
 *                    │             │
 *        ┌───────────┴────┬────────┴──────────┐
 *        │                │                   │
 *   ┌────▼────┐      ┌────▼────┐        ┌────▼────┐
 *   │ User 1  │      │ User 2  │        │ User 3  │
 *   │ [Shared]│      │ [Shared]│        │ [Shared]│
 *   └─────────┘      └─────────┘        └─────────┘
 *
 *   After User 2 writes (CoW trigger):
 *   ┌──────────────────────────────────────────────────┐
 *   │  Template Data (refcount = 2)                    │
 *   └────────────────┬──────────────────────────────────┘
 *                    │
 *        ┌───────────┴────┐                  │
 *        │                │                  │
 *   ┌────▼────┐      ┌────▼────────┐   ┌────▼────┐
 *   │ User 1  │      │ User 2      │   │ User 3  │
 *   │ [Shared]│      │ [Private]◄──┼───┤ [Shared]│
 *   └─────────┘      └─────────────┘   └─────────┘
 *                         │
 *                    (Private copy
 *                     from pool)
 *
 * USE CASES:
 * 1. Sparse Channel Usage: 1000 channels, 100 active → 10x memory savings
 * 2. Fork-like Operations: Clone brain, modify subset → fast cloning
 * 3. Checkpointing: Snapshot state, track changes → instant snapshots
 * 4. Template Initialization: Create many objects from template
 *
 * PERFORMANCE:
 * - Reference: O(1) - Increment refcount
 * - CoW Trigger: O(m) - Copy data (m = size), but from pool (fast)
 * - Release: O(1) - Decrement refcount, free if zero
 *
 * THREAD SAFETY:
 * - Atomic reference counting
 * - Mutex for copy operations
 * - Lock-free reads for shared data
 *
 * @author NIMCP Development Team
 * @date 2025-11-21
 * @version 1.0.0
 */

#ifndef NIMCP_COW_MANAGER_H
#define NIMCP_COW_MANAGER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include "utils/memory/nimcp_memory_pool.h"

#ifdef __cplusplus
extern "C" {
#endif

// Export macro (for shared library builds)
#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

//=============================================================================
// Types and Configuration
//=============================================================================

/**
 * @brief CoW handle (opaque pointer to CoW-managed data)
 */
typedef struct cow_handle_struct* cow_handle_t;

/**
 * @brief CoW manager handle (opaque)
 */
typedef struct cow_manager_struct* cow_manager_t;

/**
 * @brief Copy function signature for custom data types
 *
 * @param dest Destination buffer (from pool or malloc)
 * @param src Source buffer (template)
 * @param size Size in bytes
 * @param user_data User-provided context
 * @return true on success, false on failure
 *
 * WHY: Some data structures need deep copy (e.g., pointers, handles)
 * DEFAULT: memcpy for simple data
 */
typedef bool (*cow_copy_fn)(void* dest, const void* src, size_t size, void* user_data);

/**
 * @brief Destructor function for CoW data
 *
 * @param data Data to destroy
 * @param user_data User-provided context
 *
 * WHY: Custom cleanup for complex data structures
 * DEFAULT: No-op for simple data
 */
typedef void (*cow_destructor_fn)(void* data, void* user_data);

/**
 * @brief CoW manager configuration
 */
typedef struct {
    size_t data_size;           /**< Size of data being managed */
    memory_pool_t pool;         /**< Memory pool for fast copies (optional) */
    cow_copy_fn copy_fn;        /**< Custom copy function (NULL = memcpy) */
    cow_destructor_fn dtor_fn;  /**< Custom destructor (NULL = no-op) */
    void* user_data;            /**< User context for callbacks */
    bool enable_tracking;       /**< Track statistics */
} cow_manager_config_t;

/**
 * @brief CoW handle state
 */
typedef enum {
    COW_STATE_SHARED,           /**< Shared template (read-only) */
    COW_STATE_PRIVATE,          /**< Private copy (writable) */
    COW_STATE_INVALID           /**< Invalid/freed handle */
} cow_state_t;

/**
 * @brief CoW statistics
 */
typedef struct {
    size_t total_handles;       /**< Total CoW handles created */
    size_t active_shared;       /**< Active handles using shared data */
    size_t active_private;      /**< Active handles with private copies */
    size_t template_refcount;   /**< Current template reference count */
    size_t cow_triggers;        /**< Total CoW copy operations */
    size_t failed_copies;       /**< Failed CoW operations */
    size_t memory_saved_bytes;  /**< Memory saved by sharing */
    uint64_t total_copy_time_ns; /**< Total time spent copying */
} cow_stats_t;

//=============================================================================
// CoW Manager API
//=============================================================================

/**
 * @brief Create CoW manager
 *
 * WHAT: Initialize CoW manager with template data
 * WHY:  Enable sharing of template across multiple handles
 * HOW:  Allocates template, initializes refcount = 0
 *
 * @param config Manager configuration
 * @param template_data Initial template data (copied)
 * @return Manager handle or NULL on failure
 *
 * COMPLEXITY: O(m) where m = data_size (template copy)
 * MEMORY: data_size + manager overhead (~128 bytes)
 *
 * EXAMPLE:
 * ```c
 * float template[1024] = {0};  // Shared template
 * cow_manager_config_t config = {
 *     .data_size = sizeof(template),
 *     .pool = my_pool,  // Fast allocation
 *     .copy_fn = NULL,  // Default memcpy
 * };
 * cow_manager_t mgr = cow_manager_create(&config, template);
 * ```
 */
NIMCP_EXPORT cow_manager_t cow_manager_create(
    const cow_manager_config_t* config,
    const void* template_data
);

/**
 * @brief Destroy CoW manager
 *
 * WHAT: Frees manager and template (if refcount = 0)
 * WHY:  Clean shutdown
 * HOW:  Checks refcount, frees if safe
 *
 * @param manager Manager handle
 *
 * WARNING: All handles must be released before destruction
 */
NIMCP_EXPORT void cow_manager_destroy(cow_manager_t manager);

/**
 * @brief Acquire CoW handle (reference shared template)
 *
 * WHAT: Creates handle that references shared template
 * WHY:  Fast O(1) initialization without allocation
 * HOW:  Increments refcount, returns handle
 *
 * @param manager Manager handle
 * @return CoW handle or NULL on failure
 *
 * COMPLEXITY: O(1)
 * MEMORY: ~32 bytes (handle structure only)
 * THREAD SAFETY: Thread-safe (atomic refcount)
 *
 * EXAMPLE:
 * ```c
 * cow_handle_t h1 = cow_acquire(mgr);  // Shared
 * cow_handle_t h2 = cow_acquire(mgr);  // Also shared
 * // Both reference same template, no allocation
 * ```
 */
NIMCP_EXPORT cow_handle_t cow_acquire(cow_manager_t manager);

/**
 * @brief Release CoW handle
 *
 * WHAT: Decrements refcount, frees private data if owned
 * WHY:  Return resources to pool/system
 * HOW:  Atomic decrement, cleanup on last release
 *
 * @param handle Handle to release
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe (atomic refcount)
 */
NIMCP_EXPORT void cow_release(cow_handle_t handle);

/**
 * @brief Get read-only pointer to data
 *
 * WHAT: Returns pointer for reading (shared or private)
 * WHY:  Fast read access without triggering CoW
 * HOW:  Returns data pointer directly
 *
 * @param handle CoW handle
 * @return const pointer to data
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Lock-free (read-only)
 *
 * IMPORTANT: Do NOT cast away const and write!
 */
NIMCP_EXPORT const void* cow_read(cow_handle_t handle);

/**
 * @brief Get writable pointer to data (triggers CoW if shared)
 *
 * WHAT: Returns writable pointer, copying if needed
 * WHY:  Enable writes while preserving shared template
 * HOW:  If shared: copy from pool, mark private, return; else return
 *
 * @param handle CoW handle
 * @return Writable pointer or NULL on failure
 *
 * COMPLEXITY: O(1) if private, O(m) if shared (m = data_size)
 * THREAD SAFETY: Thread-safe (mutex on copy)
 *
 * EXAMPLE:
 * ```c
 * cow_handle_t h = cow_acquire(mgr);
 * const float* ro = cow_read(h);       // Read-only, no copy
 * float* rw = cow_write(h);            // Triggers CoW copy!
 * rw[0] = 42.0f;                       // Safe to write
 * ```
 */
NIMCP_EXPORT void* cow_write(cow_handle_t handle);

/**
 * @brief Check if handle is using shared template
 *
 * @param handle CoW handle
 * @return true if shared, false if private
 */
NIMCP_EXPORT bool cow_is_shared(cow_handle_t handle);

/**
 * @brief Get handle state
 *
 * @param handle CoW handle
 * @return Handle state
 */
NIMCP_EXPORT cow_state_t cow_get_state(cow_handle_t handle);

/**
 * @brief Force handle to become private (pre-emptive CoW)
 *
 * WHAT: Explicitly triggers CoW copy
 * WHY:  Pre-allocate if you know you'll write later
 * HOW:  Same as cow_write() but doesn't return pointer
 *
 * @param handle CoW handle
 * @return true on success, false on failure
 *
 * USE CASE: Batch operations where writes are deferred
 */
NIMCP_EXPORT bool cow_make_private(cow_handle_t handle);

/**
 * @brief Get reference count of shared template
 *
 * @param manager Manager handle
 * @return Current reference count
 *
 * USE CASE: Monitoring sharing effectiveness
 */
NIMCP_EXPORT size_t cow_get_refcount(cow_manager_t manager);

/**
 * @brief Get total handle count for the manager
 *
 * WHY DIFFERENT FROM REFCOUNT:
 * - refcount only counts SHARED handles (pointing to template)
 * - handle_count includes ALL handles (shared + private)
 * - Use handle_count to determine when cow_manager can be safely destroyed
 *
 * @param manager Manager handle
 * @return Total number of active handles (shared + private)
 */
NIMCP_EXPORT size_t cow_get_handle_count(cow_manager_t manager);

/**
 * @brief Get CoW statistics
 *
 * @param manager Manager handle
 * @param stats Output statistics structure
 * @return true on success, false on failure
 */
NIMCP_EXPORT bool cow_get_stats(cow_manager_t manager, cow_stats_t* stats);

/**
 * @brief Reset statistics
 *
 * @param manager Manager handle
 */
NIMCP_EXPORT void cow_reset_stats(cow_manager_t manager);

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Calculate memory usage for CoW system
 *
 * @param manager Manager handle
 * @param shared_bytes Output: bytes used by shared template
 * @param private_bytes Output: bytes used by private copies
 * @param overhead_bytes Output: manager + handle overhead
 * @return true on success
 *
 * USE CASE: Memory profiling and optimization
 */
NIMCP_EXPORT bool cow_calculate_memory_usage(
    cow_manager_t manager,
    size_t* shared_bytes,
    size_t* private_bytes,
    size_t* overhead_bytes
);

/**
 * @brief Get default CoW manager configuration
 *
 * @param data_size Size of data to manage
 * @param pool Memory pool for fast copies (NULL = use malloc)
 * @return Default configuration
 */
static inline cow_manager_config_t cow_default_config(
    size_t data_size,
    memory_pool_t pool
) {
    cow_manager_config_t config = {
        .data_size = data_size,
        .pool = pool,
        .copy_fn = NULL,        // Default memcpy
        .dtor_fn = NULL,        // No destructor
        .user_data = NULL,
        .enable_tracking = true
    };
    return config;
}

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_COW_MANAGER_H
