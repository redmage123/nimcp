/**
 * @file nimcp_async.c
 * @brief Asynchronous inference and futures implementation
 * @version 2.7.0
 * @date 2025-11-04
 */

#include "utils/async/nimcp_async.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread_pool.h"
#include "utils/time/nimcp_time.h"
#include <string.h>
#include <errno.h>

//=============================================================================
// Future Creation & Destruction
//=============================================================================

nimcp_future_t nimcp_future_create(void) {
    nimcp_future_t future = (nimcp_future_t)nimcp_calloc(1, sizeof(struct nimcp_future_handle));
    if (!future) {
        return NULL;
    }

    future->status = NIMCP_FUTURE_PENDING;
    future->result = NULL;
    future->result_size = 0;
    future->error_code = 0;
    future->error_message[0] = '\0';
    future->cancel_requested = false;
    future->callback = NULL;
    future->callback_user_data = NULL;
    future->created_at_ms = nimcp_time_get_ms();
    future->completed_at_ms = 0;

    // Initialize synchronization primitives
    if (pthread_mutex_init(&future->mutex, NULL) != 0) {
        nimcp_free(future);
        return NULL;
    }

    if (pthread_cond_init(&future->cond, NULL) != 0) {
        pthread_mutex_destroy(&future->mutex);
        nimcp_free(future);
        return NULL;
    }

    return future;
}

void nimcp_future_destroy(nimcp_future_t future, bool cancel_if_pending) {
    if (!future) {
        return;
    }

    // Cancel if requested and still pending
    if (cancel_if_pending && future->status == NIMCP_FUTURE_PENDING) {
        nimcp_future_cancel(future);
    }

    // Wait for completion if still pending
    if (future->status == NIMCP_FUTURE_PENDING) {
        nimcp_future_wait(future, 0);  // 0 = infinite wait
    }

    // Free result buffer
    if (future->result) {
        nimcp_free(future->result);
    }

    // Destroy synchronization primitives
    pthread_cond_destroy(&future->cond);
    pthread_mutex_destroy(&future->mutex);

    // Free future
    nimcp_free(future);
}

//=============================================================================
// Future Status & Results
//=============================================================================

nimcp_future_status_t nimcp_future_status(nimcp_future_t future) {
    if (!future) {
        return NIMCP_FUTURE_ERROR;
    }

    pthread_mutex_lock(&future->mutex);
    nimcp_future_status_t status = future->status;
    pthread_mutex_unlock(&future->mutex);

    return status;
}

bool nimcp_future_is_done(nimcp_future_t future) {
    if (!future) {
        return true;
    }

    nimcp_future_status_t status = nimcp_future_status(future);
    return (status == NIMCP_FUTURE_READY ||
            status == NIMCP_FUTURE_ERROR ||
            status == NIMCP_FUTURE_CANCELLED);
}

nimcp_future_status_t nimcp_future_wait(nimcp_future_t future, uint32_t timeout_ms) {
    if (!future) {
        return NIMCP_FUTURE_ERROR;
    }

    pthread_mutex_lock(&future->mutex);

    // Already done?
    if (future->status != NIMCP_FUTURE_PENDING) {
        nimcp_future_status_t status = future->status;
        pthread_mutex_unlock(&future->mutex);
        return status;
    }

    // Wait for completion
    if (timeout_ms == 0) {
        // Infinite wait
        while (future->status == NIMCP_FUTURE_PENDING) {
            pthread_cond_wait(&future->cond, &future->mutex);
        }
    } else {
        // Timed wait
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += timeout_ms / 1000;
        ts.tv_nsec += (timeout_ms % 1000) * 1000000;
        if (ts.tv_nsec >= 1000000000) {
            ts.tv_sec += 1;
            ts.tv_nsec -= 1000000000;
        }

        while (future->status == NIMCP_FUTURE_PENDING) {
            int result = pthread_cond_timedwait(&future->cond, &future->mutex, &ts);
            if (result == ETIMEDOUT) {
                // Timeout - still pending
                pthread_mutex_unlock(&future->mutex);
                return NIMCP_FUTURE_PENDING;
            }
        }
    }

    nimcp_future_status_t status = future->status;
    pthread_mutex_unlock(&future->mutex);
    return status;
}

bool nimcp_future_wait_and_get(nimcp_future_t future, uint32_t timeout_ms,
                                void* result_buffer, size_t buffer_size) {
    if (!future || !result_buffer) {
        return false;
    }

    nimcp_future_status_t status = nimcp_future_wait(future, timeout_ms);

    if (status != NIMCP_FUTURE_READY) {
        return false;
    }

    return nimcp_future_get_result(future, result_buffer, buffer_size);
}

//=============================================================================
// Future Cancellation
//=============================================================================

bool nimcp_future_cancel(nimcp_future_t future) {
    if (!future) {
        return false;
    }

    pthread_mutex_lock(&future->mutex);

    // Can only cancel pending futures
    if (future->status != NIMCP_FUTURE_PENDING) {
        pthread_mutex_unlock(&future->mutex);
        return false;
    }

    // Request cancellation
    future->cancel_requested = true;

    // Update status
    future->status = NIMCP_FUTURE_CANCELLED;
    future->completed_at_ms = nimcp_time_get_ms();

    // Signal waiters
    pthread_cond_broadcast(&future->cond);

    pthread_mutex_unlock(&future->mutex);

    // Invoke callback if registered
    if (future->callback) {
        future->callback(future, future->callback_user_data);
    }

    return true;
}

bool nimcp_future_is_cancelled(nimcp_future_t future) {
    if (!future) {
        return false;
    }

    pthread_mutex_lock(&future->mutex);
    bool cancelled = future->cancel_requested;
    pthread_mutex_unlock(&future->mutex);

    return cancelled;
}

//=============================================================================
// Future Result Access
//=============================================================================

bool nimcp_future_get_result(nimcp_future_t future, void* result_buffer, size_t buffer_size) {
    if (!future || !result_buffer) {
        return false;
    }

    pthread_mutex_lock(&future->mutex);

    // Wait if still pending
    while (future->status == NIMCP_FUTURE_PENDING) {
        pthread_cond_wait(&future->cond, &future->mutex);
    }

    // Only READY status has valid result
    if (future->status != NIMCP_FUTURE_READY || !future->result) {
        pthread_mutex_unlock(&future->mutex);
        return false;
    }

    // Check buffer size
    if (buffer_size < future->result_size) {
        pthread_mutex_unlock(&future->mutex);
        return false;
    }

    // Copy result
    memcpy(result_buffer, future->result, future->result_size);

    pthread_mutex_unlock(&future->mutex);
    return true;
}

int nimcp_future_get_error(nimcp_future_t future) {
    if (!future) {
        return -1;
    }

    pthread_mutex_lock(&future->mutex);
    int error_code = future->error_code;
    pthread_mutex_unlock(&future->mutex);

    return error_code;
}

const char* nimcp_future_get_error_message(nimcp_future_t future) {
    if (!future) {
        return "Invalid future handle";
    }

    pthread_mutex_lock(&future->mutex);

    if (future->status != NIMCP_FUTURE_ERROR) {
        pthread_mutex_unlock(&future->mutex);
        return NULL;
    }

    // Return pointer to error message (valid until future destroyed)
    const char* msg = future->error_message;
    pthread_mutex_unlock(&future->mutex);

    return msg;
}

//=============================================================================
// Future Callbacks
//=============================================================================

bool nimcp_future_on_complete(nimcp_future_t future,
                               nimcp_future_callback_t callback,
                               void* user_data) {
    if (!future || !callback) {
        return false;
    }

    pthread_mutex_lock(&future->mutex);

    // Register callback
    future->callback = callback;
    future->callback_user_data = user_data;

    // If already complete, invoke immediately
    bool already_complete = (future->status != NIMCP_FUTURE_PENDING);

    pthread_mutex_unlock(&future->mutex);

    if (already_complete) {
        callback(future, user_data);
    }

    return true;
}

//=============================================================================
// Future Combinators
//=============================================================================

bool nimcp_future_wait_all(nimcp_future_t* futures, uint32_t num_futures, uint32_t timeout_ms) {
    if (!futures || num_futures == 0) {
        return false;
    }

    uint64_t start_ms = nimcp_time_get_ms();
    uint64_t deadline_ms = (timeout_ms == 0) ? UINT64_MAX : (start_ms + timeout_ms);

    // Wait for each future
    for (uint32_t i = 0; i < num_futures; i++) {
        uint64_t now_ms = nimcp_time_get_ms();

        // Check timeout
        if (now_ms >= deadline_ms) {
            return false;  // Timeout
        }

        uint32_t remaining_ms = (timeout_ms == 0) ? 0 : (uint32_t)(deadline_ms - now_ms);

        nimcp_future_status_t status = nimcp_future_wait(futures[i], remaining_ms);

        if (status == NIMCP_FUTURE_PENDING) {
            return false;  // Timeout on this future
        }
    }

    return true;  // All completed
}

bool nimcp_future_wait_any(nimcp_future_t* futures, uint32_t num_futures,
                            uint32_t timeout_ms, uint32_t* completed_index) {
    if (!futures || num_futures == 0) {
        return false;
    }

    uint64_t start_ms = nimcp_time_get_ms();
    uint64_t deadline_ms = (timeout_ms == 0) ? UINT64_MAX : (start_ms + timeout_ms);

    // Poll futures until one completes or timeout
    while (true) {
        for (uint32_t i = 0; i < num_futures; i++) {
            if (nimcp_future_is_done(futures[i])) {
                if (completed_index) {
                    *completed_index = i;
                }
                return true;
            }
        }

        // Check timeout
        uint64_t now_ms = nimcp_time_get_ms();
        if (timeout_ms > 0 && now_ms >= deadline_ms) {
            return false;  // Timeout
        }

        // Brief sleep to avoid busy-wait
        struct timespec sleep_time = {0, 1000000};  // 1ms
        nanosleep(&sleep_time, NULL);
    }
}

//=============================================================================
// Future Utilities
//=============================================================================

uint64_t nimcp_future_elapsed_ms(nimcp_future_t future) {
    if (!future) {
        return 0;
    }

    pthread_mutex_lock(&future->mutex);

    uint64_t elapsed;
    if (future->completed_at_ms > 0) {
        // Completed - return total time
        elapsed = future->completed_at_ms - future->created_at_ms;
    } else {
        // Still running - return time so far
        elapsed = nimcp_time_get_ms() - future->created_at_ms;
    }

    pthread_mutex_unlock(&future->mutex);

    return elapsed;
}

void nimcp_future_set_result(nimcp_future_t future, const void* result, size_t result_size) {
    if (!future) {
        return;
    }

    pthread_mutex_lock(&future->mutex);

    // Allocate result buffer
    future->result = nimcp_malloc(result_size);
    if (!future->result) {
        // Out of memory - set error instead
        future->status = NIMCP_FUTURE_ERROR;
        future->error_code = ENOMEM;
        snprintf(future->error_message, sizeof(future->error_message),
                 "Out of memory allocating result buffer");
    } else {
        // Copy result
        memcpy(future->result, result, result_size);
        future->result_size = result_size;
        future->status = NIMCP_FUTURE_READY;
    }

    future->completed_at_ms = nimcp_time_get_ms();

    // Signal waiters
    pthread_cond_broadcast(&future->cond);

    pthread_mutex_unlock(&future->mutex);

    // Invoke callback if registered
    if (future->callback) {
        future->callback(future, future->callback_user_data);
    }
}

void nimcp_future_set_error(nimcp_future_t future, int error_code, const char* error_message) {
    if (!future) {
        return;
    }

    pthread_mutex_lock(&future->mutex);

    future->status = NIMCP_FUTURE_ERROR;
    future->error_code = error_code;

    if (error_message) {
        snprintf(future->error_message, sizeof(future->error_message), "%s", error_message);
    } else {
        future->error_message[0] = '\0';
    }

    future->completed_at_ms = nimcp_time_get_ms();

    // Signal waiters
    pthread_cond_broadcast(&future->cond);

    pthread_mutex_unlock(&future->mutex);

    // Invoke callback if registered
    if (future->callback) {
        future->callback(future, future->callback_user_data);
    }
}
