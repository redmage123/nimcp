#include <stddef.h>  /* for NULL */
// Must define _GNU_SOURCE for pthread_barrier on Linux
#ifdef __linux__
    #define _GNU_SOURCE
#endif

//=============================================================================
// nimcp_hot_inject.c - Hot Injection Mechanism Implementation
//=============================================================================
/**
 * @file nimcp_hot_inject.c
 * @brief Hot injection for runtime code patching
 *
 * WHAT: Thread-safe runtime code patching via dlopen/dlsym with atomic swap
 * WHY:  Enable live patching without restart, supporting zero-downtime updates
 * HOW:  Load .so, pause threads at barrier, atomic swap via dispatch table, resume
 *
 * DESIGN DECISIONS:
 * - pthread_barrier for thread synchronization (POSIX, not Windows)
 * - Atomic flag for pause request (lock-free check in hot path)
 * - Integration with fn_dispatch for atomic pointer swap
 * - Crash tracking with auto-rollback threshold
 * - Patch history for auditing and cleanup
 *
 * BIOLOGICAL GROUNDING:
 * Mirrors neuroplasticity - synaptic connections modified at runtime.
 * Crash tracking mirrors brain's error correction mechanisms.
 * Apoptosis (unload) mirrors programmed cell death for cleanup.
 *
 * SRP: This module handles ONLY hot injection mechanics
 *      Function dispatch is delegated to fn_dispatch module
 *
 * @author NIMCP Development Team
 * @date 2025-12-27
 * @version 1.0.0
 */

#include "utils/code/nimcp_hot_inject.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_time.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <dlfcn.h>
#include "utils/thread/nimcp_thread.h"

//=============================================================================
// Module Constants
//=============================================================================

#define LOG_MODULE "hot_inject"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(hot_inject)

/** @brief Maximum registered threads for barrier */
#define HOT_INJECT_MAX_THREADS 256

/** @brief Initial patch array capacity */
#define HOT_INJECT_INITIAL_CAPACITY 64

//=============================================================================
// Internal Data Structures
//=============================================================================

/**
 * @brief Internal hot injector state
 */
struct hot_injector {
    uint32_t magic;                            /**< Magic for validation */
    hot_inject_config_t config;                /**< Configuration */
    fn_dispatch_table_t* dispatch;             /**< Function dispatch table */

    // Patch tracking
    injected_patch_t* patches;                 /**< Array of patches */
    uint32_t patch_count;                      /**< Number of patches */
    uint32_t patch_capacity;                   /**< Capacity of patches array */
    uint64_t next_patch_id;                    /**< Next patch ID */

    // Thread synchronization
    hot_inject_thread_t threads[HOT_INJECT_MAX_THREADS]; /**< Registered threads */
    uint32_t thread_count;                     /**< Number of registered threads */
    nimcp_mutex_t thread_mutex;              /**< Protects thread array */

    // Barrier for thread synchronization
    pthread_barrier_t barrier;                 /**< Synchronization barrier */
    volatile bool barrier_initialized;         /**< Whether barrier is ready */
    volatile bool pause_requested;             /**< Flag for pause request */
    volatile bool swap_in_progress;            /**< Flag during swap */

    // Main lock
    nimcp_mutex_t lock;                      /**< Protects injector state */

    // Validation
    hot_inject_validate_fn validator;          /**< Validation callback */
    void* validator_data;                      /**< Validator user data */

    // Statistics
    uint32_t total_patches;                    /**< Total patches ever injected */
    uint32_t total_rollbacks;                  /**< Total rollbacks performed */
    uint32_t total_crashes;                    /**< Total crashes recorded */
};

//=============================================================================
// Internal Helper Declarations
//=============================================================================

static int grow_patches(hot_injector_t injector);
static injected_patch_t* find_patch_by_id(hot_injector_t injector, uint64_t id);
static injected_patch_t* find_patch_by_function(hot_injector_t injector, const char* fn_name);
static int reinit_barrier(hot_injector_t injector);
static uint64_t get_time_ms(void);

//=============================================================================
// Lifecycle Functions
//=============================================================================

NIMCP_EXPORT hot_inject_config_t hot_inject_default_config(void)
{
    hot_inject_config_t config = {
        .pause_timeout_ms = HOT_INJECT_DEFAULT_PAUSE_TIMEOUT_MS,
        .validate_timeout_ms = HOT_INJECT_DEFAULT_VALIDATE_TIMEOUT_MS,
        .require_validation = false,
        .crash_threshold = HOT_INJECT_DEFAULT_CRASH_THRESHOLD,
        .verbose = false
    };
    return config;
}

NIMCP_EXPORT hot_injector_t hot_injector_create(
    const hot_inject_config_t* config,
    fn_dispatch_table_t* dispatch_table)
{
    // Guard: NULL dispatch table
    if (!dispatch_table) {
        LOG_MODULE_ERROR(LOG_MODULE, "NULL dispatch table");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dispatch_table is NULL");

        return NULL;
    }

    // Guard: Invalid dispatch table
    if (!fn_dispatch_is_valid(dispatch_table)) {
        LOG_MODULE_ERROR(LOG_MODULE, "Invalid dispatch table");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hot_injector_create: fn_dispatch_is_valid is NULL");
        return NULL;
    }

    // Allocate injector
    struct hot_injector* injector = nimcp_calloc(1, sizeof(struct hot_injector));
    if (!injector) {
        LOG_MODULE_ERROR(LOG_MODULE, "Failed to allocate injector");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "injector is NULL");

        return NULL;
    }

    // Copy configuration
    if (config) {
        injector->config = *config;
    } else {
        injector->config = hot_inject_default_config();
    }

    // Store dispatch table reference
    injector->dispatch = dispatch_table;

    // Initialize main lock
    if (nimcp_mutex_init(&injector->lock, NULL) != 0) {
        LOG_MODULE_ERROR(LOG_MODULE, "Failed to initialize lock");
        nimcp_free(injector);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "hot_injector_create: validation failed");
        return NULL;
    }

    // Initialize thread mutex
    if (nimcp_mutex_init(&injector->thread_mutex, NULL) != 0) {
        LOG_MODULE_ERROR(LOG_MODULE, "Failed to initialize thread mutex");
        nimcp_mutex_destroy(&injector->lock);
        nimcp_free(injector);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "hot_injector_create: validation failed");
        return NULL;
    }

    // Allocate initial patch array
    injector->patch_capacity = HOT_INJECT_INITIAL_CAPACITY;
    injector->patches = nimcp_calloc(injector->patch_capacity, sizeof(injected_patch_t));
    if (!injector->patches) {
        LOG_MODULE_ERROR(LOG_MODULE, "Failed to allocate patch array");
        nimcp_mutex_destroy(&injector->thread_mutex);
        nimcp_mutex_destroy(&injector->lock);
        nimcp_free(injector);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hot_injector_create: injector->patches is NULL");
        return NULL;
    }

    // Initialize state
    injector->patch_count = 0;
    injector->next_patch_id = 1;
    injector->thread_count = 0;
    injector->barrier_initialized = false;
    injector->pause_requested = false;
    injector->swap_in_progress = false;
    injector->validator = NULL;
    injector->validator_data = NULL;
    injector->total_patches = 0;
    injector->total_rollbacks = 0;
    injector->total_crashes = 0;

    // Initialize thread array
    memset(injector->threads, 0, sizeof(injector->threads));

    // Set magic last
    injector->magic = HOT_INJECT_MAGIC;

    if (injector->config.verbose) {
        LOG_MODULE_INFO(LOG_MODULE, "Created hot injector with capacity %u",
                        injector->patch_capacity);
    }

    return injector;
}

NIMCP_EXPORT void hot_injector_destroy(hot_injector_t injector)
{
    // Guard: NULL
    if (!injector) {
        return;
    }

    // Guard: Invalid
    if (injector->magic != HOT_INJECT_MAGIC) {
        LOG_MODULE_WARN(LOG_MODULE, "Destroying invalid injector");
        return;
    }

    // Unload all patches
    for (uint32_t i = 0; i < injector->patch_count; i++) {
        injected_patch_t* patch = &injector->patches[i];
        if (patch->patch_handle) {
            dlclose(patch->patch_handle);
            patch->patch_handle = NULL;
        }
    }

    // Destroy barrier if initialized
    if (injector->barrier_initialized) {
        pthread_barrier_destroy(&injector->barrier);
        injector->barrier_initialized = false;
    }

    // Free patches array
    if (injector->patches) {
        nimcp_free(injector->patches);
        injector->patches = NULL;
    }

    // Destroy mutexes
    nimcp_mutex_destroy(&injector->thread_mutex);
    nimcp_mutex_destroy(&injector->lock);

    // Invalidate magic
    injector->magic = 0;

    nimcp_free(injector);

    LOG_MODULE_DEBUG(LOG_MODULE, "Destroyed hot injector");
}

//=============================================================================
// Core Injection Functions
//=============================================================================

NIMCP_EXPORT int hot_inject_patch(
    hot_injector_t injector,
    const char* so_path,
    const char* fn_name,
    injected_patch_t* result)
{
    // Guard: NULL parameters
    if (!injector || !so_path || !fn_name) {
        return HOT_INJECT_ERR_NULL;
    }

    // Guard: Invalid injector
    if (!hot_inject_is_valid(injector)) {
        return HOT_INJECT_ERR_INVALID_STATE;
    }

    int err;
    void* handle = NULL;
    void* new_fn = NULL;
    void* old_fn = NULL;

    nimcp_mutex_lock(&injector->lock);

    // Check capacity
    if (injector->patch_count >= HOT_INJECT_MAX_PATCHES) {
        nimcp_mutex_unlock(&injector->lock);
        LOG_MODULE_ERROR(LOG_MODULE, "Maximum patches reached");
        return HOT_INJECT_ERR_FULL;
    }

    // Grow array if needed
    if (injector->patch_count >= injector->patch_capacity) {
        err = grow_patches(injector);
        if (err != HOT_INJECT_OK) {
            nimcp_mutex_unlock(&injector->lock);
            return err;
        }
    }

    // Get patch slot
    injected_patch_t* patch = &injector->patches[injector->patch_count];
    memset(patch, 0, sizeof(injected_patch_t));

    // Initialize patch
    patch->id = injector->next_patch_id++;
    strncpy(patch->patch_so_path, so_path, HOT_INJECT_PATH_MAX - 1);
    strncpy(patch->function_name, fn_name, HOT_INJECT_NAME_MAX - 1);
    patch->state = INJECT_STATE_PENDING;
    patch->inject_time = get_time_ms();

    nimcp_mutex_unlock(&injector->lock);

    if (injector->config.verbose) {
        LOG_MODULE_INFO(LOG_MODULE, "Starting injection: %s -> %s", so_path, fn_name);
    }

    // Phase 1: Load .so
    patch->state = INJECT_STATE_LOADING;
    err = hot_inject_load_so(injector, so_path, fn_name, &handle, &new_fn);
    if (err != HOT_INJECT_OK) {
        patch->state = INJECT_STATE_FAILED;
        LOG_MODULE_ERROR(LOG_MODULE, "Failed to load patch: %s", hot_inject_strerror(err));
        return err;
    }

    patch->patch_handle = handle;
    patch->new_function = new_fn;

    // Phase 2: Pause threads
    patch->state = INJECT_STATE_PAUSING;
    err = hot_inject_pause_threads(injector);
    if (err != HOT_INJECT_OK) {
        patch->state = INJECT_STATE_FAILED;
        dlclose(handle);
        patch->patch_handle = NULL;
        LOG_MODULE_ERROR(LOG_MODULE, "Failed to pause threads: %s", hot_inject_strerror(err));
        return err;
    }

    // Phase 3: Atomic swap
    patch->state = INJECT_STATE_SWAPPING;
    err = hot_inject_swap(injector, fn_name, new_fn, &old_fn);
    if (err != HOT_INJECT_OK) {
        patch->state = INJECT_STATE_FAILED;
        hot_inject_resume_threads(injector);
        dlclose(handle);
        patch->patch_handle = NULL;
        LOG_MODULE_ERROR(LOG_MODULE, "Failed to swap function: %s", hot_inject_strerror(err));
        return err;
    }

    patch->old_function = old_fn;

    // Phase 4: Resume threads
    patch->state = INJECT_STATE_RESUMING;
    err = hot_inject_resume_threads(injector);
    if (err != HOT_INJECT_OK) {
        // Swap succeeded but resume failed - critical error
        LOG_MODULE_ERROR(LOG_MODULE, "Failed to resume threads: %s", hot_inject_strerror(err));
        // Try to rollback
        fn_dispatch_swap(injector->dispatch, fn_name, old_fn, NULL);
        patch->state = INJECT_STATE_FAILED;
        dlclose(handle);
        patch->patch_handle = NULL;
        return err;
    }

    // Phase 5: Validation (if enabled)
    if (injector->config.require_validation && injector->validator) {
        if (!injector->validator(patch, injector->validator_data)) {
            LOG_MODULE_WARN(LOG_MODULE, "Validation failed, rolling back");
            hot_inject_rollback(injector, patch->id);
            return HOT_INJECT_ERR_VALIDATION;
        }
    }

    // Success
    patch->state = INJECT_STATE_ACTIVE;

    nimcp_mutex_lock(&injector->lock);
    injector->patch_count++;
    injector->total_patches++;
    nimcp_mutex_unlock(&injector->lock);

    // Copy result if requested
    if (result) {
        *result = *patch;
    }

    if (injector->config.verbose) {
        LOG_MODULE_INFO(LOG_MODULE, "Injection complete: patch %lu for %s",
                        patch->id, fn_name);
    }

    return HOT_INJECT_OK;
}

NIMCP_EXPORT int hot_inject_load_so(
    hot_injector_t injector,
    const char* so_path,
    const char* fn_name,
    void** handle_out,
    void** fn_out)
{
    // Guard: NULL parameters
    if (!injector || !so_path || !fn_name || !handle_out || !fn_out) {
        return HOT_INJECT_ERR_NULL;
    }

    // Guard: Invalid injector
    if (!hot_inject_is_valid(injector)) {
        return HOT_INJECT_ERR_INVALID_STATE;
    }

    // Clear output
    *handle_out = NULL;
    *fn_out = NULL;

    // Clear any existing error
    dlerror();

    // Open shared object
    void* handle = dlopen(so_path, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        const char* error = dlerror();
        LOG_MODULE_ERROR(LOG_MODULE, "dlopen failed for %s: %s", so_path,
                         error ? error : "unknown error");
        return HOT_INJECT_ERR_DLOPEN;
    }

    // Clear error again
    dlerror();

    // Look up symbol
    void* fn = dlsym(handle, fn_name);
    const char* error = dlerror();
    if (error) {
        LOG_MODULE_ERROR(LOG_MODULE, "dlsym failed for %s: %s", fn_name, error);
        dlclose(handle);
        return HOT_INJECT_ERR_DLSYM;
    }

    // Symbol may be NULL if it's a valid symbol with NULL value
    // We still consider this success
    if (!fn) {
        LOG_MODULE_WARN(LOG_MODULE, "Symbol %s resolved to NULL", fn_name);
    }

    *handle_out = handle;
    *fn_out = fn;

    if (injector->config.verbose) {
        LOG_MODULE_DEBUG(LOG_MODULE, "Loaded %s from %s", fn_name, so_path);
    }

    return HOT_INJECT_OK;
}

NIMCP_EXPORT int hot_inject_pause_threads(hot_injector_t injector)
{
    // Guard: NULL
    if (!injector) {
        return HOT_INJECT_ERR_NULL;
    }

    // Guard: Invalid
    if (!hot_inject_is_valid(injector)) {
        return HOT_INJECT_ERR_INVALID_STATE;
    }

    nimcp_mutex_lock(&injector->thread_mutex);

    // If no threads registered, no need to pause
    if (injector->thread_count == 0) {
        nimcp_mutex_unlock(&injector->thread_mutex);
        if (injector->config.verbose) {
            LOG_MODULE_DEBUG(LOG_MODULE, "No threads to pause");
        }
        return HOT_INJECT_OK;
    }

    // Reinitialize barrier with current thread count + 1 (for injector)
    int err = reinit_barrier(injector);
    if (err != HOT_INJECT_OK) {
        nimcp_mutex_unlock(&injector->thread_mutex);
        return err;
    }

    // Signal pause request
    __atomic_store_n(&injector->pause_requested, true, __ATOMIC_SEQ_CST);
    __atomic_store_n(&injector->swap_in_progress, true, __ATOMIC_SEQ_CST);

    nimcp_mutex_unlock(&injector->thread_mutex);

    // Wait at barrier for all threads
    if (injector->config.verbose) {
        LOG_MODULE_DEBUG(LOG_MODULE, "Waiting for %u threads at barrier",
                         injector->thread_count);
    }

    int result = pthread_barrier_wait(&injector->barrier);
    if (result != 0 && result != PTHREAD_BARRIER_SERIAL_THREAD) {
        LOG_MODULE_ERROR(LOG_MODULE, "Barrier wait failed: %d", result);
        __atomic_store_n(&injector->pause_requested, false, __ATOMIC_SEQ_CST);
        return HOT_INJECT_ERR_BARRIER;
    }

    if (injector->config.verbose) {
        LOG_MODULE_DEBUG(LOG_MODULE, "All threads paused at barrier");
    }

    return HOT_INJECT_OK;
}

NIMCP_EXPORT int hot_inject_swap(
    hot_injector_t injector,
    const char* fn_name,
    void* new_fn,
    void** old_fn_out)
{
    // Guard: NULL parameters
    if (!injector || !fn_name) {
        return HOT_INJECT_ERR_NULL;
    }

    // Guard: Invalid
    if (!hot_inject_is_valid(injector)) {
        return HOT_INJECT_ERR_INVALID_STATE;
    }

    // Perform swap via dispatch table
    int result = fn_dispatch_swap(injector->dispatch, fn_name, new_fn, old_fn_out);
    if (result != FN_DISPATCH_OK) {
        LOG_MODULE_ERROR(LOG_MODULE, "Dispatch swap failed for %s: %s",
                         fn_name, fn_dispatch_strerror(result));
        return HOT_INJECT_ERR_DISPATCH;
    }

    // Memory barrier to ensure visibility
    __sync_synchronize();

    if (injector->config.verbose) {
        LOG_MODULE_DEBUG(LOG_MODULE, "Swapped function %s", fn_name);
    }

    return HOT_INJECT_OK;
}

NIMCP_EXPORT int hot_inject_resume_threads(hot_injector_t injector)
{
    // Guard: NULL
    if (!injector) {
        return HOT_INJECT_ERR_NULL;
    }

    // Guard: Invalid
    if (!hot_inject_is_valid(injector)) {
        return HOT_INJECT_ERR_INVALID_STATE;
    }

    // Clear pause request
    __atomic_store_n(&injector->swap_in_progress, false, __ATOMIC_SEQ_CST);
    __atomic_store_n(&injector->pause_requested, false, __ATOMIC_SEQ_CST);

    // If no threads registered, nothing to resume
    nimcp_mutex_lock(&injector->thread_mutex);
    if (injector->thread_count == 0 || !injector->barrier_initialized) {
        nimcp_mutex_unlock(&injector->thread_mutex);
        return HOT_INJECT_OK;
    }
    nimcp_mutex_unlock(&injector->thread_mutex);

    // Wait at barrier to release all threads
    int result = pthread_barrier_wait(&injector->barrier);
    if (result != 0 && result != PTHREAD_BARRIER_SERIAL_THREAD) {
        LOG_MODULE_ERROR(LOG_MODULE, "Resume barrier wait failed: %d", result);
        return HOT_INJECT_ERR_BARRIER;
    }

    if (injector->config.verbose) {
        LOG_MODULE_DEBUG(LOG_MODULE, "All threads resumed");
    }

    return HOT_INJECT_OK;
}

//=============================================================================
// Rollback Functions
//=============================================================================

NIMCP_EXPORT int hot_inject_rollback(hot_injector_t injector, uint64_t patch_id)
{
    // Guard: NULL
    if (!injector) {
        return HOT_INJECT_ERR_NULL;
    }

    // Guard: Invalid
    if (!hot_inject_is_valid(injector)) {
        return HOT_INJECT_ERR_INVALID_STATE;
    }

    nimcp_mutex_lock(&injector->lock);

    // Find patch
    injected_patch_t* patch = find_patch_by_id(injector, patch_id);
    if (!patch) {
        nimcp_mutex_unlock(&injector->lock);
        LOG_MODULE_ERROR(LOG_MODULE, "Patch %lu not found", patch_id);
        return HOT_INJECT_ERR_NOT_FOUND;
    }

    // Check state
    if (patch->state != INJECT_STATE_ACTIVE) {
        nimcp_mutex_unlock(&injector->lock);
        LOG_MODULE_WARN(LOG_MODULE, "Patch %lu not active, cannot rollback", patch_id);
        return HOT_INJECT_ERR_INVALID_STATE;
    }

    const char* fn_name = patch->function_name;
    void* old_fn = patch->old_function;

    nimcp_mutex_unlock(&injector->lock);

    if (injector->config.verbose) {
        LOG_MODULE_INFO(LOG_MODULE, "Rolling back patch %lu for %s", patch_id, fn_name);
    }

    // Pause threads
    int err = hot_inject_pause_threads(injector);
    if (err != HOT_INJECT_OK) {
        LOG_MODULE_ERROR(LOG_MODULE, "Failed to pause for rollback");
        return err;
    }

    // Swap back to old function
    err = hot_inject_swap(injector, fn_name, old_fn, NULL);
    if (err != HOT_INJECT_OK) {
        hot_inject_resume_threads(injector);
        LOG_MODULE_ERROR(LOG_MODULE, "Failed to swap for rollback");
        return err;
    }

    // Resume threads
    err = hot_inject_resume_threads(injector);
    if (err != HOT_INJECT_OK) {
        LOG_MODULE_ERROR(LOG_MODULE, "Failed to resume after rollback");
        return err;
    }

    // Update patch state
    nimcp_mutex_lock(&injector->lock);
    patch->state = INJECT_STATE_ROLLED_BACK;
    injector->total_rollbacks++;
    nimcp_mutex_unlock(&injector->lock);

    LOG_MODULE_INFO(LOG_MODULE, "Rolled back patch %lu", patch_id);

    return HOT_INJECT_OK;
}

NIMCP_EXPORT int hot_inject_unload(hot_injector_t injector, uint64_t patch_id)
{
    // Guard: NULL
    if (!injector) {
        return HOT_INJECT_ERR_NULL;
    }

    // Guard: Invalid
    if (!hot_inject_is_valid(injector)) {
        return HOT_INJECT_ERR_INVALID_STATE;
    }

    nimcp_mutex_lock(&injector->lock);

    // Find patch
    injected_patch_t* patch = find_patch_by_id(injector, patch_id);
    if (!patch) {
        nimcp_mutex_unlock(&injector->lock);
        LOG_MODULE_ERROR(LOG_MODULE, "Patch %lu not found for unload", patch_id);
        return HOT_INJECT_ERR_NOT_FOUND;
    }

    // Warn if trying to unload active patch
    if (patch->state == INJECT_STATE_ACTIVE) {
        LOG_MODULE_WARN(LOG_MODULE, "Unloading active patch %lu - should rollback first",
                        patch_id);
    }

    // Close the shared object
    if (patch->patch_handle) {
        dlclose(patch->patch_handle);
        patch->patch_handle = NULL;
    }

    // Mark as unloaded (keep entry for auditing, but clear pointers)
    patch->new_function = NULL;

    if (injector->config.verbose) {
        LOG_MODULE_INFO(LOG_MODULE, "Unloaded patch %lu (apoptosis)", patch_id);
    }

    nimcp_mutex_unlock(&injector->lock);

    return HOT_INJECT_OK;
}

//=============================================================================
// Monitoring Functions
//=============================================================================

NIMCP_EXPORT int hot_inject_get_patch(
    hot_injector_t injector,
    uint64_t patch_id,
    injected_patch_t* patch_out)
{
    // Guard: NULL
    if (!injector || !patch_out) {
        return HOT_INJECT_ERR_NULL;
    }

    // Guard: Invalid
    if (!hot_inject_is_valid(injector)) {
        return HOT_INJECT_ERR_INVALID_STATE;
    }

    nimcp_mutex_lock(&injector->lock);

    injected_patch_t* patch = find_patch_by_id(injector, patch_id);
    if (!patch) {
        nimcp_mutex_unlock(&injector->lock);
        return HOT_INJECT_ERR_NOT_FOUND;
    }

    *patch_out = *patch;

    nimcp_mutex_unlock(&injector->lock);

    return HOT_INJECT_OK;
}

NIMCP_EXPORT int hot_inject_record_crash(hot_injector_t injector, const char* fn_name)
{
    // Guard: NULL
    if (!injector || !fn_name) {
        return HOT_INJECT_ERR_NULL;
    }

    // Guard: Invalid
    if (!hot_inject_is_valid(injector)) {
        return HOT_INJECT_ERR_INVALID_STATE;
    }

    nimcp_mutex_lock(&injector->lock);

    // Find active patch for this function
    injected_patch_t* patch = find_patch_by_function(injector, fn_name);
    if (!patch || patch->state != INJECT_STATE_ACTIVE) {
        nimcp_mutex_unlock(&injector->lock);
        // Still record crash with dispatch table
        fn_dispatch_record_crash(injector->dispatch, fn_name);
        return HOT_INJECT_OK;
    }

    // Increment crash count
    patch->crashes_since_inject++;
    injector->total_crashes++;

    uint64_t patch_id = patch->id;
    uint64_t crash_count = patch->crashes_since_inject;
    uint32_t threshold = injector->config.crash_threshold;

    nimcp_mutex_unlock(&injector->lock);

    LOG_MODULE_WARN(LOG_MODULE, "Recorded crash for %s (patch %lu, count: %lu)",
                    fn_name, patch_id, crash_count);

    // Also record with dispatch table for quarantine tracking
    fn_dispatch_record_crash(injector->dispatch, fn_name);

    // Auto-rollback if threshold exceeded
    if (crash_count >= threshold) {
        LOG_MODULE_ERROR(LOG_MODULE, "Crash threshold exceeded for patch %lu, auto-rollback",
                         patch_id);
        hot_inject_rollback(injector, patch_id);
    }

    return HOT_INJECT_OK;
}

NIMCP_EXPORT uint32_t hot_inject_list_active(
    hot_injector_t injector,
    injected_patch_t* patches,
    uint32_t max)
{
    // Guard: NULL
    if (!injector || !patches || max == 0) {
        return 0;
    }

    // Guard: Invalid
    if (!hot_inject_is_valid(injector)) {
        return 0;
    }

    nimcp_mutex_lock(&injector->lock);

    uint32_t count = 0;
    for (uint32_t i = 0; i < injector->patch_count && count < max; i++) {
        if (injector->patches[i].state == INJECT_STATE_ACTIVE) {
            patches[count++] = injector->patches[i];
        }
    }

    nimcp_mutex_unlock(&injector->lock);

    return count;
}

//=============================================================================
// Thread Registration Functions
//=============================================================================

NIMCP_EXPORT int hot_inject_register_thread(hot_injector_t injector)
{
    // Guard: NULL
    if (!injector) {
        return HOT_INJECT_ERR_NULL;
    }

    // Guard: Invalid
    if (!hot_inject_is_valid(injector)) {
        return HOT_INJECT_ERR_INVALID_STATE;
    }

    pthread_t self = pthread_self();

    nimcp_mutex_lock(&injector->thread_mutex);

    // Check if already registered
    for (uint32_t i = 0; i < injector->thread_count; i++) {
        if (pthread_equal(injector->threads[i].thread_id, self)) {
            injector->threads[i].active = true;
            nimcp_mutex_unlock(&injector->thread_mutex);
            return HOT_INJECT_OK;
        }
    }

    // Check capacity
    if (injector->thread_count >= HOT_INJECT_MAX_THREADS) {
        nimcp_mutex_unlock(&injector->thread_mutex);
        LOG_MODULE_ERROR(LOG_MODULE, "Maximum threads registered");
        return HOT_INJECT_ERR_FULL;
    }

    // Add thread
    uint32_t idx = injector->thread_count;
    injector->threads[idx].thread_id = self;
    injector->threads[idx].ready = false;
    injector->threads[idx].active = true;
    injector->thread_count++;

    if (injector->config.verbose) {
        LOG_MODULE_DEBUG(LOG_MODULE, "Registered thread %u (total: %u)",
                         (unsigned)idx, injector->thread_count);
    }

    nimcp_mutex_unlock(&injector->thread_mutex);

    return HOT_INJECT_OK;
}

NIMCP_EXPORT int hot_inject_unregister_thread(hot_injector_t injector)
{
    // Guard: NULL
    if (!injector) {
        return HOT_INJECT_ERR_NULL;
    }

    // Guard: Invalid
    if (!hot_inject_is_valid(injector)) {
        return HOT_INJECT_ERR_INVALID_STATE;
    }

    pthread_t self = pthread_self();

    nimcp_mutex_lock(&injector->thread_mutex);

    // Find thread
    for (uint32_t i = 0; i < injector->thread_count; i++) {
        if (pthread_equal(injector->threads[i].thread_id, self)) {
            // Mark inactive instead of removing to avoid index shift
            injector->threads[i].active = false;

            if (injector->config.verbose) {
                LOG_MODULE_DEBUG(LOG_MODULE, "Unregistered thread %u", (unsigned)i);
            }

            nimcp_mutex_unlock(&injector->thread_mutex);
            return HOT_INJECT_OK;
        }
    }

    nimcp_mutex_unlock(&injector->thread_mutex);

    // Not found - not an error
    return HOT_INJECT_OK;
}

NIMCP_EXPORT bool hot_inject_pause_requested(hot_injector_t injector)
{
    if (!injector || !hot_inject_is_valid(injector)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hot_inject_pause_requested: required parameter is NULL (injector, hot_inject_is_valid)");
        return false;
    }

    return __atomic_load_n(&injector->pause_requested, __ATOMIC_SEQ_CST);
}

NIMCP_EXPORT int hot_inject_thread_barrier_wait(hot_injector_t injector)
{
    // Guard: NULL
    if (!injector) {
        return HOT_INJECT_ERR_NULL;
    }

    // Guard: Invalid
    if (!hot_inject_is_valid(injector)) {
        return HOT_INJECT_ERR_INVALID_STATE;
    }

    // Wait at first barrier (pause)
    if (injector->barrier_initialized) {
        int result = pthread_barrier_wait(&injector->barrier);
        if (result != 0 && result != PTHREAD_BARRIER_SERIAL_THREAD) {
            return HOT_INJECT_ERR_BARRIER;
        }
    }

    // Wait at second barrier (resume)
    if (injector->barrier_initialized) {
        int result = pthread_barrier_wait(&injector->barrier);
        if (result != 0 && result != PTHREAD_BARRIER_SERIAL_THREAD) {
            return HOT_INJECT_ERR_BARRIER;
        }
    }

    return HOT_INJECT_OK;
}

//=============================================================================
// Validation Functions
//=============================================================================

NIMCP_EXPORT void hot_inject_set_validator(
    hot_injector_t injector,
    hot_inject_validate_fn validate,
    void* user_data)
{
    if (!injector || !hot_inject_is_valid(injector)) {
        return;
    }

    nimcp_mutex_lock(&injector->lock);
    injector->validator = validate;
    injector->validator_data = user_data;
    nimcp_mutex_unlock(&injector->lock);
}

//=============================================================================
// Utility Functions
//=============================================================================

NIMCP_EXPORT bool hot_inject_is_valid(hot_injector_t injector)
{
    if (!injector) {
        return false;
    }
    return injector->magic == HOT_INJECT_MAGIC;
}

NIMCP_EXPORT const char* hot_inject_strerror(hot_inject_error_t error)
{
    switch (error) {
        case HOT_INJECT_OK:
            return "Success";
        case HOT_INJECT_ERR_NULL:
            return "NULL pointer argument";
        case HOT_INJECT_ERR_INVALID_STATE:
            return "Invalid injector state";
        case HOT_INJECT_ERR_DLOPEN:
            return "dlopen failed";
        case HOT_INJECT_ERR_DLSYM:
            return "dlsym failed";
        case HOT_INJECT_ERR_PAUSE_TIMEOUT:
            return "Thread pause timeout";
        case HOT_INJECT_ERR_SWAP_FAILED:
            return "Function swap failed";
        case HOT_INJECT_ERR_NOT_FOUND:
            return "Patch not found";
        case HOT_INJECT_ERR_NO_MEMORY:
            return "Memory allocation failed";
        case HOT_INJECT_ERR_ALREADY_LOADED:
            return "Patch already loaded";
        case HOT_INJECT_ERR_VALIDATION:
            return "Validation failed";
        case HOT_INJECT_ERR_BARRIER:
            return "Barrier operation failed";
        case HOT_INJECT_ERR_FULL:
            return "Maximum patches reached";
        case HOT_INJECT_ERR_DISPATCH:
            return "Dispatch table operation failed";
        default:
            return "Unknown error";
    }
}

NIMCP_EXPORT const char* hot_inject_state_name(inject_state_t state)
{
    switch (state) {
        case INJECT_STATE_PENDING:
            return "PENDING";
        case INJECT_STATE_LOADING:
            return "LOADING";
        case INJECT_STATE_PAUSING:
            return "PAUSING";
        case INJECT_STATE_SWAPPING:
            return "SWAPPING";
        case INJECT_STATE_RESUMING:
            return "RESUMING";
        case INJECT_STATE_ACTIVE:
            return "ACTIVE";
        case INJECT_STATE_ROLLED_BACK:
            return "ROLLED_BACK";
        case INJECT_STATE_FAILED:
            return "FAILED";
        default:
            return "UNKNOWN";
    }
}

NIMCP_EXPORT int hot_inject_get_stats(
    hot_injector_t injector,
    uint32_t* total_patches,
    uint32_t* active_patches,
    uint32_t* total_rollbacks,
    uint32_t* total_crashes)
{
    // Guard: NULL
    if (!injector) {
        return HOT_INJECT_ERR_NULL;
    }

    // Guard: Invalid
    if (!hot_inject_is_valid(injector)) {
        return HOT_INJECT_ERR_INVALID_STATE;
    }

    nimcp_mutex_lock(&injector->lock);

    if (total_patches) {
        *total_patches = injector->total_patches;
    }

    if (active_patches) {
        uint32_t count = 0;
        for (uint32_t i = 0; i < injector->patch_count; i++) {
            if (injector->patches[i].state == INJECT_STATE_ACTIVE) {
                count++;
            }
        }
        *active_patches = count;
    }

    if (total_rollbacks) {
        *total_rollbacks = injector->total_rollbacks;
    }

    if (total_crashes) {
        *total_crashes = injector->total_crashes;
    }

    nimcp_mutex_unlock(&injector->lock);

    return HOT_INJECT_OK;
}

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Grow patch array capacity
 *
 * NOTE: Caller must hold injector lock
 */
static int grow_patches(hot_injector_t injector)
{
    uint32_t new_capacity = injector->patch_capacity * 2;
    if (new_capacity > HOT_INJECT_MAX_PATCHES) {
        new_capacity = HOT_INJECT_MAX_PATCHES;
    }

    injected_patch_t* new_patches = nimcp_realloc(
        injector->patches,
        new_capacity * sizeof(injected_patch_t)
    );

    if (!new_patches) {
        LOG_MODULE_ERROR(LOG_MODULE, "Failed to grow patch array");
        return HOT_INJECT_ERR_NO_MEMORY;
    }

    // Zero new entries
    memset(&new_patches[injector->patch_capacity], 0,
           (new_capacity - injector->patch_capacity) * sizeof(injected_patch_t));

    injector->patches = new_patches;
    injector->patch_capacity = new_capacity;

    if (injector->config.verbose) {
        LOG_MODULE_DEBUG(LOG_MODULE, "Grew patch array to %u", new_capacity);
    }

    return HOT_INJECT_OK;
}

/**
 * @brief Find patch by ID
 *
 * NOTE: Caller must hold injector lock
 */
static injected_patch_t* find_patch_by_id(hot_injector_t injector, uint64_t id)
{
    for (uint32_t i = 0; i < injector->patch_count; i++) {
        if (injector->patches[i].id == id) {
            return &injector->patches[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_patch_by_id: validation failed");
    return NULL;
}

/**
 * @brief Find active patch by function name
 *
 * NOTE: Caller must hold injector lock
 */
static injected_patch_t* find_patch_by_function(hot_injector_t injector, const char* fn_name)
{
    for (uint32_t i = 0; i < injector->patch_count; i++) {
        if (injector->patches[i].state == INJECT_STATE_ACTIVE &&
            strcmp(injector->patches[i].function_name, fn_name) == 0) {
            return &injector->patches[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_patch_by_function: operation failed");
    return NULL;
}

/**
 * @brief Reinitialize barrier with current thread count
 *
 * NOTE: Caller must hold thread_mutex
 */
static int reinit_barrier(hot_injector_t injector)
{
    // Destroy existing barrier if any
    if (injector->barrier_initialized) {
        pthread_barrier_destroy(&injector->barrier);
        injector->barrier_initialized = false;
    }

    // Count active threads
    uint32_t active_count = 0;
    for (uint32_t i = 0; i < injector->thread_count; i++) {
        if (injector->threads[i].active) {
            active_count++;
        }
    }

    if (active_count == 0) {
        // No threads to synchronize
        return HOT_INJECT_OK;
    }

    // Initialize barrier for active threads + injector thread
    int result = pthread_barrier_init(&injector->barrier, NULL, active_count + 1);
    if (result != 0) {
        LOG_MODULE_ERROR(LOG_MODULE, "Failed to initialize barrier: %d", result);
        return HOT_INJECT_ERR_BARRIER;
    }

    injector->barrier_initialized = true;

    if (injector->config.verbose) {
        LOG_MODULE_DEBUG(LOG_MODULE, "Initialized barrier for %u threads", active_count + 1);
    }

    return HOT_INJECT_OK;
}

/**
 * @brief Get current time in milliseconds
 */
static uint64_t get_time_ms(void)
{
    return nimcp_platform_time_monotonic_ms();
}
