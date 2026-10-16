/**
 * @file nimcp_async.h
 * @brief Asynchronous inference and futures
 * @version 2.7.0
 * @date 2025-11-04
 *
 * Enhancement 4: Async Inference
 *
 * WHAT: Non-blocking inference with future-based API
 * WHY: Better throughput, responsive UI, pipeline multiple inferences
 * HOW: Thread pool, futures with condition variables, timeout support
 *
 * Benefits:
 * - Overlap I/O with compute
 * - Non-blocking API
 * - Cancellable operations
 */

#ifndef NIMCP_ASYNC_H
#define NIMCP_ASYNC_H

#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Future Status
//=============================================================================

/**
 * @brief Status of async operation
 */
typedef enum {
    NIMCP_FUTURE_PENDING,     ///< Still computing
    NIMCP_FUTURE_READY,       ///< Result available
    NIMCP_FUTURE_ERROR,       ///< Computation failed
    NIMCP_FUTURE_CANCELLED    ///< Operation cancelled
} nimcp_future_status_t;

//=============================================================================
// Future Handle (Opaque)
//=============================================================================

/**
 * @brief Opaque handle to async operation
 */
typedef struct nimcp_future_handle* nimcp_future_t;

/**
 * @brief Future completion callback
 *
 * Called when future completes (success or error).
 * Callback executes on worker thread - keep it fast!
 *
 * @param future Future handle
 * @param user_data User-provided context
 */
typedef void (*nimcp_future_callback_t)(nimcp_future_t future, void* user_data);

//=============================================================================
// Future Creation & Management
//=============================================================================

/**
 * @brief Create future for async operation
 *
 * Internal use only - futures created by async APIs.
 *
 * @return Future handle or NULL on error
 */
nimcp_future_t nimcp_future_create(void);

/**
 * @brief Destroy future and free resources
 *
 * Waits for completion if still pending.
 * Cancels operation if cancel_if_pending=true.
 *
 * @param future Future handle
 * @param cancel_if_pending Cancel if still running?
 */
NIMCP_EXPORT void nimcp_future_destroy(nimcp_future_t future, bool cancel_if_pending);

//=============================================================================
// Future Status & Results
//=============================================================================

/**
 * @brief Check future status (non-blocking)
 *
 * @param future Future handle
 * @return Current status
 */
NIMCP_EXPORT nimcp_future_status_t nimcp_future_status(nimcp_future_t future);

/**
 * @brief Check if future is done (non-blocking)
 *
 * @param future Future handle
 * @return true if READY, ERROR, or CANCELLED
 */
NIMCP_EXPORT bool nimcp_future_is_done(nimcp_future_t future);

/**
 * @brief Wait for future completion (blocking)
 *
 * Blocks until future is READY, ERROR, or CANCELLED.
 *
 * @param future Future handle
 * @param timeout_ms Timeout in milliseconds (0 = infinite)
 * @return Status after wait
 */
NIMCP_EXPORT nimcp_future_status_t nimcp_future_wait(
    nimcp_future_t future,
    uint32_t timeout_ms
);

/**
 * @brief Wait for future with result extraction (blocking)
 *
 * Convenience function that waits and gets result in one call.
 *
 * @param future Future handle
 * @param timeout_ms Timeout in milliseconds (0 = infinite)
 * @param result_buffer Output buffer for result
 * @param buffer_size Size of result buffer
 * @return true on success (READY), false on error/timeout/cancel
 */
NIMCP_EXPORT bool nimcp_future_wait_and_get(
    nimcp_future_t future,
    uint32_t timeout_ms,
    void* result_buffer,
    size_t buffer_size
);

//=============================================================================
// Future Cancellation
//=============================================================================

/**
 * @brief Request cancellation of pending future
 *
 * Note: Cancellation is cooperative - worker checks periodically.
 * Already-completed futures cannot be cancelled.
 *
 * @param future Future handle
 * @return true if cancellation requested, false if already done
 */
NIMCP_EXPORT bool nimcp_future_cancel(nimcp_future_t future);

/**
 * @brief Check if cancellation was requested
 *
 * Worker functions should call this periodically and abort if true.
 *
 * @param future Future handle
 * @return true if cancel requested
 */
NIMCP_EXPORT bool nimcp_future_is_cancelled(nimcp_future_t future);

//=============================================================================
// Future Result Access
//=============================================================================

/**
 * @brief Get result from completed future
 *
 * Only valid if status is READY.
 * Blocks if future is PENDING.
 *
 * @param future Future handle
 * @param result_buffer Output buffer for result
 * @param buffer_size Size of result buffer
 * @return true on success (result copied), false on error
 */
NIMCP_EXPORT bool nimcp_future_get_result(
    nimcp_future_t future,
    void* result_buffer,
    size_t buffer_size
);

/**
 * @brief Get error code from failed future
 *
 * Only valid if status is ERROR.
 *
 * @param future Future handle
 * @return Error code (0 if no error)
 */
NIMCP_EXPORT int nimcp_future_get_error(nimcp_future_t future);

/**
 * @brief Get error message from failed future
 *
 * Only valid if status is ERROR.
 *
 * @param future Future handle
 * @return Error message (or NULL if no error)
 */
NIMCP_EXPORT const char* nimcp_future_get_error_message(nimcp_future_t future);

//=============================================================================
// Future Callbacks
//=============================================================================

/**
 * @brief Register callback for future completion
 *
 * Callback executes once when future completes.
 * If future already complete, callback executes immediately.
 *
 * @param future Future handle
 * @param callback Callback function
 * @param user_data User context passed to callback
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_future_on_complete(
    nimcp_future_t future,
    nimcp_future_callback_t callback,
    void* user_data
);

//=============================================================================
// Future Combinators
//=============================================================================

/**
 * @brief Wait for all futures to complete
 *
 * Blocks until all futures are done (READY, ERROR, or CANCELLED).
 *
 * @param futures Array of future handles
 * @param num_futures Number of futures
 * @param timeout_ms Timeout in milliseconds (0 = infinite)
 * @return true if all completed, false if timeout
 */
NIMCP_EXPORT bool nimcp_future_wait_all(
    nimcp_future_t* futures,
    uint32_t num_futures,
    uint32_t timeout_ms
);

/**
 * @brief Wait for any future to complete
 *
 * Blocks until at least one future is done.
 *
 * @param futures Array of future handles
 * @param num_futures Number of futures
 * @param timeout_ms Timeout in milliseconds (0 = infinite)
 * @param completed_index Output: index of first completed future
 * @return true if any completed, false if timeout
 */
NIMCP_EXPORT bool nimcp_future_wait_any(
    nimcp_future_t* futures,
    uint32_t num_futures,
    uint32_t timeout_ms,
    uint32_t* completed_index
);

//=============================================================================
// Future Internal Structure (Do not use directly)
//=============================================================================

/**
 * @brief Future internal state
 *
 * WARNING: Internal use only. Subject to change.
 */
struct nimcp_future_handle {
    nimcp_future_status_t status;        ///< Current status

    // Result storage
    void* result;                        ///< Result buffer
    size_t result_size;                  ///< Result size in bytes
    int error_code;                      ///< Error code if ERROR
    char error_message[256];             ///< Error message if ERROR

    // Synchronization
    pthread_mutex_t mutex;               ///< Protects status/result
    pthread_cond_t cond;                 ///< Signals completion

    // Cancellation
    bool cancel_requested;               ///< Cancellation flag

    // Callback
    nimcp_future_callback_t callback;    ///< Completion callback
    void* callback_user_data;            ///< User data for callback

    // Metadata
    uint64_t created_at_ms;              ///< Creation timestamp
    uint64_t completed_at_ms;            ///< Completion timestamp
};

//=============================================================================
// Future Utilities
//=============================================================================

/**
 * @brief Get future elapsed time
 *
 * @param future Future handle
 * @return Elapsed time in milliseconds
 */
NIMCP_EXPORT uint64_t nimcp_future_elapsed_ms(nimcp_future_t future);

/**
 * @brief Set future result (internal use)
 *
 * Marks future as READY and signals waiters.
 *
 * @param future Future handle
 * @param result Result buffer to copy
 * @param result_size Result size in bytes
 */
void nimcp_future_set_result(nimcp_future_t future, const void* result, size_t result_size);

/**
 * @brief Set future error (internal use)
 *
 * Marks future as ERROR and signals waiters.
 *
 * @param future Future handle
 * @param error_code Error code
 * @param error_message Error description
 */
void nimcp_future_set_error(nimcp_future_t future, int error_code, const char* error_message);

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_ASYNC_H
