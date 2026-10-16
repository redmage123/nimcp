/**
 * @file nimcp_future.h
 * @brief High-performance futures/promises system for asynchronous operations
 *
 * WHAT: Lock-free futures/promises with combinators for async composition
 * WHY:  Enable efficient async workflows across brain modules with <1μs overhead
 * HOW:  Atomic state machine + condition variables + memory pool integration
 *
 * ARCHITECTURE:
 *
 *   Producer Thread                Consumer Thread(s)
 *   ┌─────────────┐               ┌──────────────────┐
 *   │  Promise    │               │     Future       │
 *   │             │               │                  │
 *   │ promise =   │               │ future =         │
 *   │  create()   │               │  get_future()    │
 *   │             │               │                  │
 *   │ // do work  │               │ wait() or        │
 *   │ ...         │               │ wait_timeout()   │
 *   │             │               │                  │
 *   │ complete()  │──────────────>│ get_result()     │
 *   │  or fail()  │   (signals)   │                  │
 *   └─────────────┘               └──────────────────┘
 *
 * STATE MACHINE:
 *
 *   [PENDING] ──complete()──> [COMPLETED]
 *       │
 *       ├──────fail()─────> [FAILED]
 *       │
 *       └────cancel()────> [CANCELLED]
 *
 * COMBINATORS:
 * - then(future, callback):  Chain async operations
 * - all(futures[]):          Wait for all futures
 * - any(futures[]):          Wait for first completion
 * - map(future, transform):  Transform result
 *
 * PERFORMANCE:
 * - Create + Complete:  <1μs (target)
 * - Wait (ready):       ~50ns (atomic check)
 * - Wait (blocking):    context switch overhead
 * - Clone (shared):     ~100ns (refcount increment)
 *
 * THREAD SAFETY:
 * - All operations are thread-safe
 * - Lock-free reads via atomics
 * - Mutex only for blocking waits
 * - Reference counting for shared futures
 *
 * USAGE EXAMPLE:
 * ```c
 * // Create promise
 * nimcp_promise_t promise = nimcp_promise_create(sizeof(float));
 * nimcp_future_t future = nimcp_promise_get_future(promise);
 *
 * // Producer thread
 * float result = 42.0f;
 * nimcp_promise_complete(promise, &result);
 *
 * // Consumer thread
 * float value;
 * if (nimcp_future_wait_timeout(future, 1000)) {
 *     nimcp_future_get(future, &value);
 *     printf("Result: %f\n", value);
 * }
 *
 * nimcp_future_destroy(future);
 * nimcp_promise_destroy(promise);
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-11-28
 * @version 1.0.0
 */

#ifndef NIMCP_FUTURE_H
#define NIMCP_FUTURE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "common/nimcp_export.h"

/* Include canonical error type definition */
#ifndef NIMCP_ERROR_TYPE_DEFINED
#include "utils/error/nimcp_error_codes.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct nimcp_promise_struct* nimcp_promise_t;
typedef struct nimcp_future_struct* nimcp_future_t;

//=============================================================================
// Future States
//=============================================================================

/**
 * @brief Future state enumeration
 *
 * State transitions are atomic and monotonic (can only advance, never regress)
 */
typedef enum {
    NIMCP_FUTURE_PENDING = 0,      /**< Initial state, waiting for completion */
    NIMCP_FUTURE_COMPLETED = 1,    /**< Successfully completed with result */
    NIMCP_FUTURE_FAILED = 2,       /**< Failed with error code */
    NIMCP_FUTURE_CANCELLED = 3     /**< Cancelled before completion */
} nimcp_future_state_t;

//=============================================================================
// Callback Types
//=============================================================================

/**
 * @brief Callback invoked when future completes
 *
 * @param result Pointer to result data (NULL if failed/cancelled)
 * @param error Error code (NIMCP_SUCCESS if completed)
 * @param user_data User-provided context
 */
typedef void (*nimcp_future_callback_t)(const void* result, nimcp_error_t error, void* user_data);

/**
 * @brief Transformation function for map combinator
 *
 * @param input Input result from source future
 * @param output Output buffer for transformed result
 * @param user_data User-provided context
 * @return NIMCP_SUCCESS on success, error code on failure
 */
typedef nimcp_error_t (*nimcp_future_transform_t)(const void* input, void* output, void* user_data);

//=============================================================================
// Statistics Structure
//=============================================================================

/**
 * @brief Future system statistics
 *
 * Tracks performance metrics and resource usage
 */
typedef struct {
    // Creation/Destruction
    uint64_t promises_created;         /**< Total promises created */
    uint64_t promises_destroyed;       /**< Total promises destroyed */
    uint64_t futures_created;          /**< Total futures created */
    uint64_t futures_destroyed;        /**< Total futures destroyed */
    uint64_t active_promises;          /**< Currently active promises */
    uint64_t active_futures;           /**< Currently active futures */

    // State transitions
    uint64_t completions;              /**< Successful completions */
    uint64_t failures;                 /**< Failed completions */
    uint64_t cancellations;            /**< Cancelled operations */

    // Wait operations
    uint64_t waits_total;              /**< Total wait operations */
    uint64_t waits_immediate;          /**< Waits that returned immediately */
    uint64_t waits_blocked;            /**< Waits that blocked */
    uint64_t waits_timeout;            /**< Waits that timed out */

    // Combinators
    uint64_t then_chains;              /**< Then-chain operations */
    uint64_t all_operations;           /**< All combinator operations */
    uint64_t any_operations;           /**< Any combinator operations */
    uint64_t map_operations;           /**< Map operations */

    // Performance metrics
    uint64_t total_wait_time_ns;       /**< Total time spent waiting */
    uint64_t total_completion_time_ns; /**< Total time to complete */
    uint64_t avg_wait_time_ns;         /**< Average wait time */
    uint64_t avg_completion_time_ns;   /**< Average completion time */

    // Memory
    size_t total_memory_bytes;         /**< Total memory allocated */
    size_t result_memory_bytes;        /**< Memory used for results */
    size_t shared_futures;             /**< Futures sharing results */
    size_t memory_saved_bytes;         /**< Memory saved via sharing */

    // Reference counting
    uint64_t refcount_increments;      /**< Total refcount increments */
    uint64_t refcount_decrements;      /**< Total refcount decrements */
} nimcp_future_stats_t;

//=============================================================================
// Promise API (Producer Side)
//=============================================================================

/**
 * @brief Create a new promise
 *
 * WHAT: Creates promise for future result of specified size
 * WHY:  Producer needs promise to publish result when ready
 * HOW:  Allocates shared state, initializes atomics, sets up condvar
 *
 * @param result_size Size of result data in bytes (0 for void result)
 * @return Promise handle or NULL on failure
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe
 * MEMORY: Allocates from unified memory pool if available
 *
 * EXAMPLE:
 * ```c
 * nimcp_promise_t promise = nimcp_promise_create(sizeof(int));
 * if (!promise) {
 *     // Handle allocation failure
 * }
 * ```
 */
NIMCP_EXPORT nimcp_promise_t nimcp_promise_create(size_t result_size);

/**
 * @brief Complete promise with success result
 *
 * WHAT: Marks promise as completed, stores result, wakes waiters
 * WHY:  Publish computed result to all consumers
 * HOW:  CAS state to COMPLETED, copy result, broadcast condvar
 *
 * @param promise Promise handle
 * @param result Pointer to result data (NULL if result_size=0)
 * @return NIMCP_SUCCESS or error code
 *
 * COMPLEXITY: O(1) + O(w) where w = number of waiters
 * THREAD SAFETY: Thread-safe, can only complete once
 * ATOMICITY: Uses CAS to ensure single completion
 *
 * ERRORS:
 * - NIMCP_ERROR_NULL_POINTER: promise or result is NULL
 * - NIMCP_ERROR_INVALID_STATE: Already completed/failed/cancelled
 *
 * EXAMPLE:
 * ```c
 * int result = 42;
 * nimcp_error_t err = nimcp_promise_complete(promise, &result);
 * ```
 */
NIMCP_EXPORT nimcp_error_t nimcp_promise_complete(nimcp_promise_t promise, const void* result);

/**
 * @brief Fail promise with error code
 *
 * WHAT: Marks promise as failed, stores error, wakes waiters
 * WHY:  Propagate errors to consumers
 * HOW:  CAS state to FAILED, store error code, broadcast condvar
 *
 * @param promise Promise handle
 * @param error Error code (must be failure code)
 * @return NIMCP_SUCCESS or error code
 *
 * COMPLEXITY: O(1) + O(w) where w = number of waiters
 * THREAD SAFETY: Thread-safe, can only fail once
 *
 * ERRORS:
 * - NIMCP_ERROR_NULL_POINTER: promise is NULL
 * - NIMCP_ERROR_INVALID_STATE: Already completed/failed/cancelled
 * - NIMCP_ERROR_INVALID_PARAMETER: error is not a failure code
 *
 * EXAMPLE:
 * ```c
 * nimcp_promise_fail(promise, NIMCP_ERROR_NO_MEMORY);
 * ```
 */
NIMCP_EXPORT nimcp_error_t nimcp_promise_fail(nimcp_promise_t promise, nimcp_error_t error);

/**
 * @brief Get future handle from promise
 *
 * WHAT: Creates future handle for consuming promise result
 * WHY:  Allow multiple consumers to wait for same result
 * HOW:  Increments refcount, returns new handle to shared state
 *
 * @param promise Promise handle
 * @return Future handle or NULL on failure
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe
 * REFCOUNTING: Increments reference count
 *
 * NOTE: Each future handle must be destroyed separately
 *
 * EXAMPLE:
 * ```c
 * nimcp_future_t future = nimcp_promise_get_future(promise);
 * // Can call multiple times to get multiple future handles
 * ```
 */
NIMCP_EXPORT nimcp_future_t nimcp_promise_get_future(nimcp_promise_t promise);

/**
 * @brief Destroy promise handle
 *
 * WHAT: Releases promise handle, decrements refcount
 * WHY:  Free producer-side resources
 * HOW:  Decrements refcount, frees if last reference
 *
 * @param promise Promise handle (NULL safe)
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe
 *
 * NOTE: Shared state is freed when both promise and all futures are destroyed
 */
NIMCP_EXPORT void nimcp_promise_destroy(nimcp_promise_t promise);

//=============================================================================
// Future API (Consumer Side)
//=============================================================================

/**
 * @brief Get current future state
 *
 * WHAT: Returns current state without blocking
 * WHY:  Check if result is ready without waiting
 * HOW:  Atomic load with acquire semantics
 *
 * @param future Future handle
 * @return Current state
 *
 * COMPLEXITY: O(1) - lock-free
 * THREAD SAFETY: Thread-safe
 * PERFORMANCE: ~50ns (atomic load)
 *
 * EXAMPLE:
 * ```c
 * if (nimcp_future_state(future) == NIMCP_FUTURE_COMPLETED) {
 *     // Result is ready
 * }
 * ```
 */
NIMCP_EXPORT nimcp_future_state_t nimcp_future_state(nimcp_future_t future);

/**
 * @brief Wait for future to complete (blocking)
 *
 * WHAT: Blocks until future is completed/failed/cancelled
 * WHY:  Synchronize with async operation
 * HOW:  Fast-path atomic check, slow-path condvar wait
 *
 * @param future Future handle
 * @return true if completed, false if failed/cancelled
 *
 * COMPLEXITY: O(1) if ready, O(wait) if pending
 * THREAD SAFETY: Thread-safe
 *
 * EXAMPLE:
 * ```c
 * if (nimcp_future_wait(future)) {
 *     // Successfully completed
 * } else {
 *     // Failed or cancelled
 * }
 * ```
 */
NIMCP_EXPORT bool nimcp_future_wait(nimcp_future_t future);

/**
 * @brief Wait for future with timeout
 *
 * WHAT: Blocks until future completes or timeout expires
 * WHY:  Prevent indefinite blocking
 * HOW:  Fast-path check, timed condvar wait
 *
 * @param future Future handle
 * @param timeout_ms Timeout in milliseconds (0 = immediate check)
 * @return true if completed, false if timeout/failed/cancelled
 *
 * COMPLEXITY: O(1) if ready, O(wait) up to timeout
 * THREAD SAFETY: Thread-safe
 *
 * EXAMPLE:
 * ```c
 * if (nimcp_future_wait_timeout(future, 1000)) {
 *     // Completed within 1 second
 * } else {
 *     // Timeout or failure
 * }
 * ```
 */
NIMCP_EXPORT bool nimcp_future_wait_timeout(nimcp_future_t future, uint32_t timeout_ms);

/**
 * @brief Get result from completed future
 *
 * WHAT: Copies result data to output buffer
 * WHY:  Retrieve computed value
 * HOW:  Checks state, copies from shared storage
 *
 * @param future Future handle
 * @param out_result Output buffer (size must match result_size)
 * @return NIMCP_SUCCESS or error code
 *
 * COMPLEXITY: O(m) where m = result_size
 * THREAD SAFETY: Thread-safe
 *
 * ERRORS:
 * - NIMCP_ERROR_NULL_POINTER: future or out_result is NULL
 * - NIMCP_ERROR_INVALID_STATE: Not in COMPLETED state
 *
 * NOTE: Must call wait() or check state() before get()
 *
 * EXAMPLE:
 * ```c
 * int result;
 * if (nimcp_future_get(future, &result) == NIMCP_SUCCESS) {
 *     printf("Result: %d\n", result);
 * }
 * ```
 */
NIMCP_EXPORT nimcp_error_t nimcp_future_get(nimcp_future_t future, void* out_result);

/**
 * @brief Get error code from failed future
 *
 * WHAT: Returns error code if future failed
 * WHY:  Retrieve failure reason
 * HOW:  Returns stored error code
 *
 * @param future Future handle
 * @return Error code (NIMCP_SUCCESS if not failed)
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe
 *
 * EXAMPLE:
 * ```c
 * if (!nimcp_future_wait(future)) {
 *     nimcp_error_t err = nimcp_future_get_error(future);
 *     printf("Failed: %s\n", nimcp_error_to_string(err));
 * }
 * ```
 */
NIMCP_EXPORT nimcp_error_t nimcp_future_get_error(nimcp_future_t future);

/**
 * @brief Check if future is ready (non-blocking)
 *
 * WHAT: Returns true if future has completed/failed/cancelled
 * WHY:  Quick check without blocking
 * HOW:  Atomic state check
 *
 * @param future Future handle
 * @return true if ready, false if pending
 *
 * COMPLEXITY: O(1) - lock-free
 * THREAD SAFETY: Thread-safe
 * PERFORMANCE: ~50ns
 *
 * EXAMPLE:
 * ```c
 * while (!nimcp_future_is_ready(future)) {
 *     // Do other work
 *     process_events();
 * }
 * ```
 */
NIMCP_EXPORT bool nimcp_future_is_ready(nimcp_future_t future);

/**
 * @brief Cancel a pending future
 *
 * WHAT: Attempts to cancel pending future
 * WHY:  Abort unnecessary computation
 * HOW:  CAS state to CANCELLED if still pending
 *
 * @param future Future handle
 * @return true if cancelled, false if already completed/failed
 *
 * COMPLEXITY: O(1) + O(w) where w = waiters
 * THREAD SAFETY: Thread-safe
 *
 * NOTE: Cancellation is best-effort; producer may ignore it
 *
 * EXAMPLE:
 * ```c
 * if (!nimcp_future_is_ready(future)) {
 *     nimcp_future_cancel(future);
 * }
 * ```
 */
NIMCP_EXPORT bool nimcp_future_cancel(nimcp_future_t future);

/**
 * @brief Destroy future handle
 *
 * WHAT: Releases future handle, decrements refcount
 * WHY:  Free consumer-side resources
 * HOW:  Decrements refcount, frees if last reference
 *
 * @param future Future handle (NULL safe)
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe
 */
NIMCP_EXPORT void nimcp_future_destroy(nimcp_future_t future);

//=============================================================================
// Combinator API
//=============================================================================

/**
 * @brief Chain continuation after future completes
 *
 * WHAT: Execute callback when future completes
 * WHY:  Compose async operations without blocking
 * HOW:  Registers callback, invokes on completion
 *
 * @param future Future to watch
 * @param callback Function to invoke on completion
 * @param user_data Context passed to callback
 * @return NIMCP_SUCCESS or error code
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe
 *
 * NOTE: Callback may be invoked immediately if already completed
 *
 * EXAMPLE:
 * ```c
 * void on_complete(const void* result, nimcp_error_t err, void* ctx) {
 *     if (err == NIMCP_SUCCESS) {
 *         int* val = (int*)result;
 *         printf("Result: %d\n", *val);
 *     }
 * }
 * nimcp_future_then(future, on_complete, NULL);
 * ```
 */
NIMCP_EXPORT nimcp_error_t nimcp_future_then(
    nimcp_future_t future,
    nimcp_future_callback_t callback,
    void* user_data
);

/**
 * @brief Wait for all futures to complete
 *
 * WHAT: Creates future that completes when all inputs complete
 * WHY:  Synchronize multiple async operations
 * HOW:  Atomic counter, completes when all finish
 *
 * @param futures Array of future handles
 * @param count Number of futures
 * @return Combined future or NULL on failure
 *
 * COMPLEXITY: O(n) where n = count
 * THREAD SAFETY: Thread-safe
 *
 * NOTE: Result contains array of success flags (bool[count])
 *
 * EXAMPLE:
 * ```c
 * nimcp_future_t futures[3] = {future1, future2, future3};
 * nimcp_future_t all = nimcp_future_all(futures, 3);
 * nimcp_future_wait(all);
 * ```
 */
NIMCP_EXPORT nimcp_future_t nimcp_future_all(
    nimcp_future_t* futures,
    size_t count
);

/**
 * @brief Wait for any future to complete
 *
 * WHAT: Creates future that completes when first input completes
 * WHY:  Race multiple operations, use fastest
 * HOW:  Atomic flag, first completion wins
 *
 * @param futures Array of future handles
 * @param count Number of futures
 * @return Combined future or NULL on failure
 *
 * COMPLEXITY: O(n) where n = count
 * THREAD SAFETY: Thread-safe
 *
 * NOTE: Result contains index of first completed future (size_t)
 *
 * EXAMPLE:
 * ```c
 * nimcp_future_t futures[3] = {future1, future2, future3};
 * nimcp_future_t any = nimcp_future_any(futures, 3);
 * size_t winner;
 * nimcp_future_get(any, &winner);
 * ```
 */
NIMCP_EXPORT nimcp_future_t nimcp_future_any(
    nimcp_future_t* futures,
    size_t count
);

/**
 * @brief Transform future result
 *
 * WHAT: Creates future with transformed result
 * WHY:  Pipeline transformations without blocking
 * HOW:  Applies transform function when source completes
 *
 * @param source Source future
 * @param transform Transformation function
 * @param output_size Size of transformed result
 * @param user_data Context for transform function
 * @return Transformed future or NULL on failure
 *
 * COMPLEXITY: O(1) + transform cost
 * THREAD SAFETY: Thread-safe
 *
 * EXAMPLE:
 * ```c
 * nimcp_error_t double_it(const void* in, void* out, void* ctx) {
 *     *(int*)out = *(const int*)in * 2;
 *     return NIMCP_SUCCESS;
 * }
 * nimcp_future_t doubled = nimcp_future_map(
 *     original, double_it, sizeof(int), NULL
 * );
 * ```
 */
NIMCP_EXPORT nimcp_future_t nimcp_future_map(
    nimcp_future_t source,
    nimcp_future_transform_t transform,
    size_t output_size,
    void* user_data
);

//=============================================================================
// Module Initialization API
//=============================================================================

/**
 * @brief Initialize the async/futures module
 *
 * WHAT: Initializes the futures system with security and memory integration
 * WHY:  Enable unified memory pools and security monitoring
 * HOW:  Registers module with security, sets up memory manager
 *
 * @param security_ctx Security integration context (NULL to skip security)
 * @param memory_mgr Unified memory manager (NULL to use malloc/free)
 * @return NIMCP_SUCCESS or error code
 *
 * NOTE: Must be called before using any future/promise functions
 *       if security or unified memory integration is desired.
 *       If not called, module will use malloc/free and no security.
 *
 * EXAMPLE:
 * ```c
 * nimcp_sec_integration_t* sec = nimcp_sec_integration_create();
 * unified_mem_manager_t mem = unified_mem_create(NULL);
 * nimcp_future_init(sec, mem);
 * ```
 */
NIMCP_EXPORT nimcp_error_t nimcp_future_init(
    void* security_ctx,
    void* memory_mgr
);

/**
 * @brief Initialize futures module with bio-async backend
 *
 * WHAT: Initializes futures with biologically-inspired async mechanisms
 * WHY:  Enable neuromodulator-based signaling, phase coupling, predictive coding
 * HOW:  Initialize both traditional and bio-async systems, enable bio backend
 *
 * @param security_ctx Security integration context (NULL to skip security)
 * @param memory_mgr Unified memory manager (NULL to use malloc/free)
 * @param bio_config Bio-async configuration (NULL for defaults)
 * @return NIMCP_SUCCESS or error code
 *
 * BIOLOGICAL MECHANISMS:
 * - Promises use neuromodulator channels (DOPAMINE for fast completion)
 * - Futures track confidence decay over time
 * - all() combinator uses phase coupling (Kuramoto oscillators)
 * - Results have biologically-realistic decay dynamics
 *
 * EXAMPLE:
 * ```c
 * nimcp_bio_async_config_t config = nimcp_bio_async_default_config();
 * nimcp_future_init_bio(NULL, NULL, &config);
 *
 * // Now all promises/futures use biological signaling
 * nimcp_promise_t p = nimcp_promise_create(sizeof(float));
 * ```
 */
NIMCP_EXPORT nimcp_error_t nimcp_future_init_bio(
    void* security_ctx,
    void* memory_mgr,
    const void* bio_config  /* nimcp_bio_async_config_t* */
);

/**
 * @brief Enable or disable bio-async backend at runtime
 *
 * WHAT: Toggle between traditional and bio-async modes
 * WHY:  Allow switching for testing or performance comparison
 * HOW:  Sets internal flag; new promises follow selected mode
 *
 * @param enable true to enable bio-async, false for traditional
 * @return NIMCP_SUCCESS or error if bio-async not initialized
 *
 * NOTE: Existing promises/futures keep their original mode.
 *       Only newly created promises are affected.
 */
NIMCP_EXPORT nimcp_error_t nimcp_future_set_bio_backend(bool enable);

/**
 * @brief Check if bio-async backend is currently enabled
 *
 * @return true if bio-async backend is active
 */
NIMCP_EXPORT bool nimcp_future_is_bio_backend_enabled(void);

/**
 * @brief Shutdown the async/futures module
 *
 * WHAT: Cleans up module resources, unregisters from security
 * WHY:  Proper cleanup on application shutdown
 * HOW:  Unregisters from security, releases memory manager reference
 *
 * NOTE: All futures/promises should be destroyed before calling this.
 *       Also shuts down bio-async if it was enabled.
 */
NIMCP_EXPORT void nimcp_future_shutdown(void);

/**
 * @brief Check if futures module is initialized
 *
 * @return true if initialized, false otherwise
 */
NIMCP_EXPORT bool nimcp_future_is_initialized(void);

/**
 * @brief Get the futures module security ID
 *
 * @return Security module ID (0 if not registered)
 */
NIMCP_EXPORT uint32_t nimcp_future_get_security_id(void);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get global future system statistics
 *
 * WHAT: Returns aggregated statistics for all futures
 * WHY:  Monitor performance and resource usage
 * HOW:  Atomically reads global counters
 *
 * @param stats Output statistics structure
 * @return NIMCP_SUCCESS or error code
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe
 *
 * EXAMPLE:
 * ```c
 * nimcp_future_stats_t stats;
 * nimcp_future_get_stats(&stats);
 * printf("Avg wait: %llu ns\n", stats.avg_wait_time_ns);
 * ```
 */
NIMCP_EXPORT nimcp_error_t nimcp_future_get_stats(nimcp_future_stats_t* stats);

/**
 * @brief Reset global statistics
 *
 * WHAT: Resets all counters to zero
 * WHY:  Start fresh measurement period
 * HOW:  Atomically resets global counters
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe
 */
NIMCP_EXPORT void nimcp_future_reset_stats(void);

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Get state name as string
 *
 * @param state State enum
 * @return Human-readable string
 */
static inline const char* nimcp_future_state_name(nimcp_future_state_t state)
{
    switch (state) {
        case NIMCP_FUTURE_PENDING: return "pending";
        case NIMCP_FUTURE_COMPLETED: return "completed";
        case NIMCP_FUTURE_FAILED: return "failed";
        case NIMCP_FUTURE_CANCELLED: return "cancelled";
        default: return "unknown";
    }
}

/**
 * @brief Check if state indicates completion (any terminal state)
 *
 * @param state State enum
 * @return true if completed/failed/cancelled
 */
static inline bool nimcp_future_state_is_terminal(nimcp_future_state_t state)
{
    return state != NIMCP_FUTURE_PENDING;
}

/**
 * @brief Check if state indicates success
 *
 * @param state State enum
 * @return true if completed successfully
 */
static inline bool nimcp_future_state_is_success(nimcp_future_state_t state)
{
    return state == NIMCP_FUTURE_COMPLETED;
}

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_FUTURE_H
