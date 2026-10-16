/**
 * @file nimcp_hot_inject.h
 * @brief Hot injection mechanism for applying patches to running code
 *
 * WHAT: Thread-safe runtime code patching via dlopen/dlsym with atomic function swap
 * WHY:  Enable live patching of running systems without restart, supporting zero-downtime
 *       updates, A/B testing, and rapid rollback of faulty code
 * HOW:  Load patch .so via dlopen, pause threads at barrier, atomic swap via dispatch table,
 *       resume threads, with crash tracking and auto-rollback
 *
 * BIOLOGICAL GROUNDING:
 * This mechanism mirrors neuroplasticity in the brain, where synaptic connections
 * can be modified during runtime. Just as neurons can form new connections or
 * strengthen/weaken existing ones, hot injection allows function implementations
 * to be replaced dynamically. The crash tracking and auto-rollback mirror the
 * brain's error correction mechanisms.
 *
 * ARCHITECTURE:
 *
 *   Patch Request                           Hot Injector
 *   ┌─────────────────┐                     ┌──────────────────────────────────┐
 *   │ hot_inject_patch│                     │                                  │
 *   │   (so_path,     │                     │  ┌─────────────────────────────┐ │
 *   │    fn_name)     │────────────────────>│  │ 1. dlopen(patch.so)         │ │
 *   └─────────────────┘                     │  │ 2. dlsym(new_function)      │ │
 *                                           │  │ 3. Pause threads at barrier │ │
 *                                           │  │ 4. fn_dispatch_swap()       │ │
 *                                           │  │ 5. Memory barrier           │ │
 *                                           │  │ 6. Resume threads           │ │
 *                                           │  │ 7. Track for apoptosis      │ │
 *                                           │  └─────────────────────────────┘ │
 *                                           └──────────────────────────────────┘
 *
 * THREAD BARRIER APPROACH:
 *
 *   Thread 1   Thread 2   Thread 3        Injector
 *       │          │          │               │
 *       │          │          │    ┌──────────┴──────────┐
 *       │          │          │    │ Set pause_requested │
 *       │          │          │    └──────────┬──────────┘
 *       ▼          ▼          ▼               │
 *   ┌───────────────────────────────┐         │
 *   │   pthread_barrier_wait()      │<────────┘
 *   │   (all threads synchronized)  │
 *   └───────────────────────────────┘
 *                 │                           │
 *                 │    ┌──────────────────────┴───────────────────────┐
 *                 │    │ Atomic swap: dispatch_table[fn] = new_fn    │
 *                 │    │ __sync_synchronize() (memory barrier)        │
 *                 │    └──────────────────────┬───────────────────────┘
 *                 │                           │
 *   ┌─────────────┴─────────────┐             │
 *   │ pthread_barrier_wait()    │<────────────┘
 *   │ (resume all threads)      │
 *   └───────────────────────────┘
 *       │          │          │
 *       ▼          ▼          ▼
 *   (continue with new function)
 *
 * FEATURES:
 * - Thread-safe injection with pthread barrier synchronization
 * - Atomic function pointer swap via fn_dispatch integration
 * - Rollback capability for crash recovery
 * - Crash tracking with auto-rollback after threshold
 * - Patch tracking for cleanup (apoptosis)
 * - Configurable timeouts and validation
 * - Verbose mode for debugging
 *
 * THREAD SAFETY:
 * - All public functions are thread-safe
 * - Uses pthread barrier for thread synchronization during swap
 * - Memory barriers ensure visibility of changes
 *
 * USAGE:
 * ```c
 * // Create injector with dispatch table
 * hot_inject_config_t config = hot_inject_default_config();
 * config.crash_threshold = 3;  // Auto-rollback after 3 crashes
 *
 * fn_dispatch_table_t* dispatch = fn_dispatch_create();
 * fn_dispatch_register(dispatch, "my_function", original_fn);
 *
 * hot_injector_t injector = hot_injector_create(&config, dispatch);
 *
 * // Inject a patch
 * injected_patch_t result;
 * int err = hot_inject_patch(injector, "patches/my_patch.so", "my_function", &result);
 * if (err == HOT_INJECT_OK) {
 *     printf("Injected patch %lu\n", result.id);
 * }
 *
 * // Record crash (if detected)
 * hot_inject_record_crash(injector, "my_function");
 *
 * // Rollback if needed
 * hot_inject_rollback(injector, result.id);
 *
 * // Cleanup (apoptosis)
 * hot_inject_unload(injector, old_patch_id);
 *
 * // Destroy injector
 * hot_injector_destroy(injector);
 * fn_dispatch_destroy(dispatch);
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-12-27
 * @version 1.0.0
 */

#ifndef NIMCP_HOT_INJECT_H
#define NIMCP_HOT_INJECT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <pthread.h>

#include "utils/dispatch/nimcp_fn_dispatch.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Export Macro
//=============================================================================

#ifndef NIMCP_EXPORT
    #if defined(__GNUC__) && __GNUC__ >= 4
        #define NIMCP_EXPORT __attribute__((visibility("default")))
    #else
        #define NIMCP_EXPORT
    #endif
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum path length for patch .so files */
#define HOT_INJECT_PATH_MAX 512

/** @brief Maximum function name length */
#define HOT_INJECT_NAME_MAX 128

/** @brief Default pause timeout in milliseconds */
#define HOT_INJECT_DEFAULT_PAUSE_TIMEOUT_MS 5000

/** @brief Default validation timeout in milliseconds */
#define HOT_INJECT_DEFAULT_VALIDATE_TIMEOUT_MS 1000

/** @brief Default crash threshold for auto-rollback */
#define HOT_INJECT_DEFAULT_CRASH_THRESHOLD 5

/** @brief Maximum number of tracked patches */
#define HOT_INJECT_MAX_PATCHES 1024

/** @brief Magic value for validation */
#define HOT_INJECT_MAGIC 0x484F5449  // 'HOTI'

//=============================================================================
// Error Codes
//=============================================================================

/** @brief Hot injection error codes */
typedef enum {
    HOT_INJECT_OK = 0,                     /**< Success */
    HOT_INJECT_ERR_NULL = -1,              /**< NULL pointer argument */
    HOT_INJECT_ERR_INVALID_STATE = -2,     /**< Invalid injector state */
    HOT_INJECT_ERR_DLOPEN = -3,            /**< dlopen failed */
    HOT_INJECT_ERR_DLSYM = -4,             /**< dlsym failed */
    HOT_INJECT_ERR_PAUSE_TIMEOUT = -5,     /**< Thread pause timeout */
    HOT_INJECT_ERR_SWAP_FAILED = -6,       /**< Function swap failed */
    HOT_INJECT_ERR_NOT_FOUND = -7,         /**< Patch not found */
    HOT_INJECT_ERR_NO_MEMORY = -8,         /**< Memory allocation failed */
    HOT_INJECT_ERR_ALREADY_LOADED = -9,    /**< Patch already loaded */
    HOT_INJECT_ERR_VALIDATION = -10,       /**< Validation failed */
    HOT_INJECT_ERR_BARRIER = -11,          /**< Barrier operation failed */
    HOT_INJECT_ERR_FULL = -12,             /**< Maximum patches reached */
    HOT_INJECT_ERR_DISPATCH = -13          /**< Dispatch table operation failed */
} hot_inject_error_t;

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief State of an injected patch
 *
 * WHAT: Tracks the lifecycle state of a patch injection
 * WHY:  Enable monitoring and debugging of injection process
 * HOW:  State machine from PENDING through ACTIVE or FAILED
 */
typedef enum {
    INJECT_STATE_PENDING = 0,     /**< Patch queued but not started */
    INJECT_STATE_LOADING,         /**< Loading .so via dlopen */
    INJECT_STATE_PAUSING,         /**< Pausing threads at barrier */
    INJECT_STATE_SWAPPING,        /**< Performing atomic swap */
    INJECT_STATE_RESUMING,        /**< Resuming threads */
    INJECT_STATE_ACTIVE,          /**< Patch successfully applied */
    INJECT_STATE_ROLLED_BACK,     /**< Patch was rolled back */
    INJECT_STATE_FAILED           /**< Injection failed */
} inject_state_t;

/**
 * @brief Information about an injected patch
 *
 * WHAT: Tracks all metadata for a single injected patch
 * WHY:  Enable rollback, monitoring, and cleanup
 * HOW:  Store handles, pointers, timestamps, and statistics
 */
typedef struct {
    uint64_t id;                           /**< Unique patch identifier */
    char patch_so_path[HOT_INJECT_PATH_MAX]; /**< Path to patch .so file */
    char function_name[HOT_INJECT_NAME_MAX]; /**< Name of patched function */
    void* patch_handle;                    /**< dlopen handle */
    void* new_function;                    /**< New function pointer */
    void* old_function;                    /**< Saved for rollback */
    inject_state_t state;                  /**< Current state */
    uint64_t inject_time;                  /**< Timestamp when injected (ms) */
    uint64_t calls_since_inject;           /**< Calls since injection */
    uint64_t crashes_since_inject;         /**< Crashes since injection */
} injected_patch_t;

/**
 * @brief Configuration for hot injector
 *
 * WHAT: Configurable parameters for injection behavior
 * WHY:  Allow tuning for different environments
 * HOW:  Struct with timeout, threshold, and behavior flags
 */
typedef struct {
    uint32_t pause_timeout_ms;             /**< Timeout for thread pause (ms) */
    uint32_t validate_timeout_ms;          /**< Timeout for validation (ms) */
    bool require_validation;               /**< Require validation callback */
    uint32_t crash_threshold;              /**< Auto-rollback after N crashes */
    bool verbose;                          /**< Enable verbose logging */
} hot_inject_config_t;

/**
 * @brief Thread registration for barrier synchronization
 *
 * WHAT: Registration entry for a thread participating in pause
 * WHY:  Track which threads need to synchronize
 * HOW:  Store thread ID and ready flag
 */
typedef struct {
    pthread_t thread_id;                   /**< Thread identifier */
    volatile bool ready;                   /**< Thread reached barrier */
    volatile bool active;                  /**< Thread is active */
} hot_inject_thread_t;

/**
 * @brief Validation callback function type
 *
 * Called after swap to validate new function works correctly.
 *
 * @param patch The patch being validated
 * @param user_data User context
 * @return true if validation passed, false to trigger rollback
 */
typedef bool (*hot_inject_validate_fn)(const injected_patch_t* patch, void* user_data);

/**
 * @brief Opaque hot injector handle
 */
typedef struct hot_injector* hot_injector_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create a hot injector instance
 *
 * WHAT: Allocate and initialize a hot injector
 * WHY:  Entry point for using hot injection
 * HOW:  Allocate state, initialize locks, link to dispatch table
 *
 * @param config Configuration (NULL for defaults)
 * @param dispatch_table Function dispatch table to use
 * @return Hot injector handle or NULL on failure
 *
 * THREAD SAFETY: Thread-safe
 * MEMORY: Caller owns returned handle
 *
 * EXAMPLE:
 * ```c
 * hot_inject_config_t config = hot_inject_default_config();
 * fn_dispatch_table_t* dispatch = fn_dispatch_create();
 * hot_injector_t injector = hot_injector_create(&config, dispatch);
 * ```
 */
NIMCP_EXPORT hot_injector_t hot_injector_create(
    const hot_inject_config_t* config,
    fn_dispatch_table_t* dispatch_table);

/**
 * @brief Destroy a hot injector and free all resources
 *
 * WHAT: Clean up injector and unload all patches
 * WHY:  Proper cleanup prevents resource leaks
 * HOW:  Unload patches, destroy locks, free memory
 *
 * @param injector Hot injector to destroy (NULL-safe)
 *
 * THREAD SAFETY: NOT thread-safe - ensure no concurrent access
 * MEMORY: Frees all injector resources, unloads patches
 */
NIMCP_EXPORT void hot_injector_destroy(hot_injector_t injector);

/**
 * @brief Get default configuration
 *
 * WHAT: Return sensible default configuration
 * WHY:  Convenience for common use cases
 * HOW:  Static struct with default values
 *
 * @return Default configuration
 */
NIMCP_EXPORT hot_inject_config_t hot_inject_default_config(void);

//=============================================================================
// Core Injection Functions
//=============================================================================

/**
 * @brief Inject a patch - full injection flow
 *
 * WHAT: Complete injection from load to activation
 * WHY:  Single call for common case
 * HOW:  Load .so, pause threads, swap, resume
 *
 * @param injector Hot injector handle
 * @param so_path Path to patch .so file
 * @param fn_name Name of function to patch
 * @param result Output: patch information
 * @return HOT_INJECT_OK on success, error code on failure
 *
 * THREAD SAFETY: Thread-safe (acquires locks)
 * MEMORY: Loads .so into memory
 *
 * EXAMPLE:
 * ```c
 * injected_patch_t result;
 * int err = hot_inject_patch(injector, "patches/fix.so", "buggy_function", &result);
 * if (err == HOT_INJECT_OK) {
 *     printf("Patch %lu active\n", result.id);
 * }
 * ```
 */
NIMCP_EXPORT int hot_inject_patch(
    hot_injector_t injector,
    const char* so_path,
    const char* fn_name,
    injected_patch_t* result);

/**
 * @brief Load patch .so file and resolve symbol
 *
 * WHAT: Load shared object and get function pointer
 * WHY:  Separate from swap for two-phase injection
 * HOW:  dlopen + dlsym
 *
 * @param injector Hot injector handle
 * @param so_path Path to patch .so file
 * @param fn_name Name of function to resolve
 * @param handle_out Output: dlopen handle
 * @param fn_out Output: function pointer
 * @return HOT_INJECT_OK on success, error code on failure
 *
 * THREAD SAFETY: Thread-safe
 * MEMORY: Loads .so into memory (caller must dlclose)
 */
NIMCP_EXPORT int hot_inject_load_so(
    hot_injector_t injector,
    const char* so_path,
    const char* fn_name,
    void** handle_out,
    void** fn_out);

/**
 * @brief Pause all registered threads at barrier
 *
 * WHAT: Signal threads to pause and wait at barrier
 * WHY:  Ensure no threads are executing target function during swap
 * HOW:  Set pause flag, wait for barrier with timeout
 *
 * @param injector Hot injector handle
 * @return HOT_INJECT_OK on success, HOT_INJECT_ERR_PAUSE_TIMEOUT on timeout
 *
 * THREAD SAFETY: Thread-safe
 * BLOCKING: Blocks until all threads reach barrier or timeout
 */
NIMCP_EXPORT int hot_inject_pause_threads(hot_injector_t injector);

/**
 * @brief Perform atomic function swap
 *
 * WHAT: Swap function pointer in dispatch table
 * WHY:  Replace function implementation
 * HOW:  fn_dispatch_swap + memory barrier
 *
 * @param injector Hot injector handle
 * @param fn_name Name of function to swap
 * @param new_fn New function pointer
 * @param old_fn_out Output: previous function pointer
 * @return HOT_INJECT_OK on success, error code on failure
 *
 * THREAD SAFETY: Thread-safe (must be called with threads paused)
 */
NIMCP_EXPORT int hot_inject_swap(
    hot_injector_t injector,
    const char* fn_name,
    void* new_fn,
    void** old_fn_out);

/**
 * @brief Resume all paused threads
 *
 * WHAT: Release threads from barrier
 * WHY:  Continue execution after swap
 * HOW:  Signal barrier completion
 *
 * @param injector Hot injector handle
 * @return HOT_INJECT_OK on success, error code on failure
 *
 * THREAD SAFETY: Thread-safe
 */
NIMCP_EXPORT int hot_inject_resume_threads(hot_injector_t injector);

//=============================================================================
// Rollback Functions
//=============================================================================

/**
 * @brief Rollback a patch to previous version
 *
 * WHAT: Restore function to pre-patch state
 * WHY:  Recover from faulty patches
 * HOW:  Swap back to old_function, update state
 *
 * @param injector Hot injector handle
 * @param patch_id Patch ID to rollback
 * @return HOT_INJECT_OK on success, error code on failure
 *
 * THREAD SAFETY: Thread-safe (pauses threads)
 *
 * EXAMPLE:
 * ```c
 * // Rollback patch after detecting issues
 * hot_inject_rollback(injector, patch.id);
 * ```
 */
NIMCP_EXPORT int hot_inject_rollback(hot_injector_t injector, uint64_t patch_id);

/**
 * @brief Unload a patch (apoptosis)
 *
 * WHAT: dlclose patch and remove from tracking
 * WHY:  Free resources from old patches
 * HOW:  dlclose + remove from patch list
 *
 * @param injector Hot injector handle
 * @param patch_id Patch ID to unload
 * @return HOT_INJECT_OK on success, error code on failure
 *
 * THREAD SAFETY: Thread-safe
 * MEMORY: Frees patch resources
 *
 * NOTE: Only call for patches that are no longer active (rolled back or superseded)
 */
NIMCP_EXPORT int hot_inject_unload(hot_injector_t injector, uint64_t patch_id);

//=============================================================================
// Monitoring Functions
//=============================================================================

/**
 * @brief Get patch information by ID
 *
 * WHAT: Retrieve current state of a patch
 * WHY:  Monitor patch status
 * HOW:  Look up in patch table
 *
 * @param injector Hot injector handle
 * @param patch_id Patch ID to look up
 * @param patch_out Output: patch information
 * @return HOT_INJECT_OK on success, HOT_INJECT_ERR_NOT_FOUND if not found
 *
 * THREAD SAFETY: Thread-safe
 */
NIMCP_EXPORT int hot_inject_get_patch(
    hot_injector_t injector,
    uint64_t patch_id,
    injected_patch_t* patch_out);

/**
 * @brief Record a crash for a patched function
 *
 * WHAT: Track crash and potentially auto-rollback
 * WHY:  Automatic recovery from faulty patches
 * HOW:  Increment crash count, rollback if threshold exceeded
 *
 * @param injector Hot injector handle
 * @param fn_name Name of function that crashed
 * @return HOT_INJECT_OK on success, error code on failure
 *
 * THREAD SAFETY: Thread-safe
 *
 * EXAMPLE:
 * ```c
 * // In signal handler or catch block
 * hot_inject_record_crash(injector, "crashed_function");
 * ```
 */
NIMCP_EXPORT int hot_inject_record_crash(hot_injector_t injector, const char* fn_name);

/**
 * @brief List all active patches
 *
 * WHAT: Get array of currently active patches
 * WHY:  Enumerate patches for monitoring
 * HOW:  Copy active patches to caller's array
 *
 * @param injector Hot injector handle
 * @param patches Output array for patches
 * @param max Maximum number of patches to return
 * @return Number of patches copied
 *
 * THREAD SAFETY: Thread-safe
 */
NIMCP_EXPORT uint32_t hot_inject_list_active(
    hot_injector_t injector,
    injected_patch_t* patches,
    uint32_t max);

//=============================================================================
// Thread Registration Functions
//=============================================================================

/**
 * @brief Register current thread for barrier synchronization
 *
 * WHAT: Add thread to barrier wait list
 * WHY:  Thread must be registered to participate in pause
 * HOW:  Add to thread list, update barrier count
 *
 * @param injector Hot injector handle
 * @return HOT_INJECT_OK on success, error code on failure
 *
 * THREAD SAFETY: Thread-safe
 *
 * NOTE: Call this from each thread that may be executing hot-swappable functions
 */
NIMCP_EXPORT int hot_inject_register_thread(hot_injector_t injector);

/**
 * @brief Unregister current thread from barrier
 *
 * WHAT: Remove thread from barrier wait list
 * WHY:  Thread shutting down or no longer needs hot-swap
 * HOW:  Remove from thread list, update barrier count
 *
 * @param injector Hot injector handle
 * @return HOT_INJECT_OK on success, error code on failure
 *
 * THREAD SAFETY: Thread-safe
 */
NIMCP_EXPORT int hot_inject_unregister_thread(hot_injector_t injector);

/**
 * @brief Check if thread pause is requested
 *
 * WHAT: Check if injection is waiting for this thread
 * WHY:  Thread should check periodically and call barrier_wait
 * HOW:  Read atomic flag
 *
 * @param injector Hot injector handle
 * @return true if pause requested, false otherwise
 *
 * THREAD SAFETY: Thread-safe (atomic read)
 *
 * EXAMPLE:
 * ```c
 * // In worker thread loop
 * while (running) {
 *     if (hot_inject_pause_requested(injector)) {
 *         hot_inject_thread_barrier_wait(injector);
 *     }
 *     // ... do work ...
 * }
 * ```
 */
NIMCP_EXPORT bool hot_inject_pause_requested(hot_injector_t injector);

/**
 * @brief Wait at barrier for injection to complete
 *
 * WHAT: Thread pauses here during injection
 * WHY:  Synchronize with injector during swap
 * HOW:  pthread_barrier_wait
 *
 * @param injector Hot injector handle
 * @return HOT_INJECT_OK when released
 *
 * THREAD SAFETY: Thread-safe
 * BLOCKING: Blocks until barrier released
 */
NIMCP_EXPORT int hot_inject_thread_barrier_wait(hot_injector_t injector);

//=============================================================================
// Validation Functions
//=============================================================================

/**
 * @brief Set validation callback
 *
 * WHAT: Register callback for post-swap validation
 * WHY:  Verify new function works before committing
 * HOW:  Store callback, called after swap
 *
 * @param injector Hot injector handle
 * @param validate Validation callback function
 * @param user_data User context for callback
 *
 * THREAD SAFETY: Thread-safe
 */
NIMCP_EXPORT void hot_inject_set_validator(
    hot_injector_t injector,
    hot_inject_validate_fn validate,
    void* user_data);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Check if injector is valid
 *
 * WHAT: Validate injector magic number and state
 * WHY:  Detect corruption or invalid handles
 * HOW:  Check magic and basic invariants
 *
 * @param injector Hot injector handle
 * @return true if valid, false otherwise
 *
 * THREAD SAFETY: Thread-safe
 */
NIMCP_EXPORT bool hot_inject_is_valid(hot_injector_t injector);

/**
 * @brief Get error message for error code
 *
 * WHAT: Convert error code to human-readable string
 * WHY:  Better error reporting
 * HOW:  Static lookup table
 *
 * @param error Error code
 * @return Error message string (static, do not free)
 */
NIMCP_EXPORT const char* hot_inject_strerror(hot_inject_error_t error);

/**
 * @brief Get state name string
 *
 * WHAT: Convert state enum to string
 * WHY:  Logging and debugging
 * HOW:  Static lookup table
 *
 * @param state Injection state
 * @return State name string (static, do not free)
 */
NIMCP_EXPORT const char* hot_inject_state_name(inject_state_t state);

/**
 * @brief Get injector statistics
 *
 * WHAT: Aggregate statistics about injector usage
 * WHY:  Monitor injector health
 * HOW:  Count patches, crashes, rollbacks
 *
 * @param injector Hot injector handle
 * @param total_patches Output: total patches injected
 * @param active_patches Output: currently active patches
 * @param total_rollbacks Output: total rollbacks performed
 * @param total_crashes Output: total crashes recorded
 * @return HOT_INJECT_OK on success, error code on failure
 */
NIMCP_EXPORT int hot_inject_get_stats(
    hot_injector_t injector,
    uint32_t* total_patches,
    uint32_t* active_patches,
    uint32_t* total_rollbacks,
    uint32_t* total_crashes);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HOT_INJECT_H */
