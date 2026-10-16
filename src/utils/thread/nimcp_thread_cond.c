#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_thread_cond.c - Condition Variable Operations
//=============================================================================

#include "utils/thread/nimcp_thread.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <errno.h>
#include <string.h>

#define LOG_MODULE "thread_cond"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(thread_cond)

// External declarations for error handling (defined in nimcp_thread.c)
extern void set_thread_error(int error_code, const char* format, ...);

//=============================================================================
// Condition Variables
//=============================================================================

/**
 * @brief Initialize condition variable (Adapter for pthread_cond_init)
 *
 * WHY CONDITION VARIABLES:
 * - Efficient thread notification (no polling)
 * - Wait for condition to become true
 * - Producer-consumer pattern
 * - Work queue pattern
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Fully safe (creates new independent condvar)
 *
 * @param cond Condition variable to initialize
 * @return NIMCP_SUCCESS or NIMCP_ERROR_INVALID_PARAM or NIMCP_ERROR_SYSTEM
 */
nimcp_result_t nimcp_cond_init(nimcp_cond_t* cond)
{
    if (!cond) {
        set_thread_error(NIMCP_ERROR_INVALID_PARAM, "Invalid cond pointer");
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_PARAM, "invalid parameter");
    }

    int result = nimcp_platform_cond_init(cond);
    if (result != 0) {
        set_thread_error(NIMCP_ERROR_SYSTEM, "Cond initialization failed: %s", strerror(result));
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_SYSTEM, "system error");
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Destroy condition variable (Adapter for pthread_cond_destroy)
 *
 * WHY DESTROY:
 * - Free kernel resources
 * - Clean shutdown
 *
 * PRECONDITIONS:
 * - No threads waiting on condition variable
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Caller must ensure no concurrent waiters
 *
 * @param cond Condition variable to destroy
 * @return NIMCP_SUCCESS or NIMCP_ERROR_INVALID_PARAM or NIMCP_ERROR_SYSTEM
 */
nimcp_result_t nimcp_cond_destroy(nimcp_cond_t* cond)
{
    NIMCP_CHECK_THROW(cond, NIMCP_ERROR_INVALID_PARAM, "cond is NULL");

    int result = nimcp_platform_cond_destroy(cond);
    if (result != 0) {
        set_thread_error(NIMCP_ERROR_SYSTEM, "Cond destruction failed: %s", strerror(result));
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_SYSTEM, "system error");
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Wait on condition variable (Adapter for pthread_cond_wait)
 *
 * WHY WAIT:
 * - Efficient blocking (no busy-wait)
 * - Atomically releases mutex and waits
 * - Re-acquires mutex when signaled
 *
 * STANDARD PATTERN (always use while loop!):
 *   nimcp_mutex_lock(&mutex);
 *   while (!condition_is_true) {
 *       nimcp_cond_wait(&cond, &mutex);  // Releases mutex, waits, re-acquires
 *   }
 *   // condition is true, mutex is locked
 *   process_condition();
 *   nimcp_mutex_unlock(&mutex);
 *
 * WHY WHILE LOOP (not if):
 * - SPURIOUS WAKEUPS: pthread_cond_wait can wake without signal (OS behavior)
 * - MULTIPLE WAITERS: Another thread may consume condition before this thread runs
 * - CORRECTNESS: Always re-check condition after wake
 *
 * COMPLEXITY: O(1) to enter wait, blocks until signaled
 * THREAD SAFETY: Fully safe (atomic unlock+wait)
 *
 * @param cond Condition variable to wait on
 * @param mutex Mutex that must be locked by caller (will be released during wait)
 * @return NIMCP_SUCCESS or NIMCP_ERROR_INVALID_PARAM or NIMCP_ERROR_SYSTEM
 */
nimcp_result_t nimcp_cond_wait(nimcp_cond_t* cond, nimcp_mutex_t* mutex)
{
    NIMCP_CHECK_THROW(cond && mutex, NIMCP_ERROR_INVALID_PARAM, "cond or mutex is NULL");

    int result = nimcp_platform_cond_wait(cond, mutex);
    if (result != 0) {
        set_thread_error(NIMCP_ERROR_SYSTEM, "Cond wait failed: %s", strerror(result));
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_SYSTEM, "system error");
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Wait on condition variable with timeout (Adapter for pthread_cond_timedwait)
 *
 * WHY TIMEOUT:
 * - Prevent indefinite blocking (watchdog pattern)
 * - Implement polling with backoff (check periodically)
 * - Handle slow/unresponsive operations
 *
 * TYPICAL USAGE (timeout pattern):
 *   nimcp_mutex_lock(&mutex);
 *   while (!condition && !timeout) {
 *       nimcp_result_t r = nimcp_cond_timedwait(&cond, &mutex, 5000);  // 5s
 *       if (r == NIMCP_BUSY) {
 *           timeout = true;  // Timed out waiting
 *       }
 *   }
 *   nimcp_mutex_unlock(&mutex);
 *
 * COMPLEXITY: O(1) to enter wait, blocks up to timeout_ms
 * THREAD SAFETY: Fully safe (atomic unlock+wait)
 *
 * @param cond Condition variable to wait on
 * @param mutex Mutex that must be locked by caller
 * @param timeout_ms Timeout in milliseconds
 * @return NIMCP_SUCCESS if signaled, NIMCP_BUSY if timeout, NIMCP_ERROR_* on error
 */
nimcp_result_t nimcp_cond_timedwait(nimcp_cond_t* cond, nimcp_mutex_t* mutex, uint32_t timeout_ms)
{
    NIMCP_CHECK_THROW(cond && mutex, NIMCP_ERROR_INVALID_PARAM, "cond or mutex is NULL");

    // Use platform abstraction for timed wait
    int result = nimcp_platform_cond_timedwait(cond, mutex, timeout_ms);

    // ETIMEDOUT is expected outcome (not error)
    if (result == ETIMEDOUT) {
        return NIMCP_BUSY;  // Consistent with trylock semantics
    } else if (result != 0) {
        set_thread_error(NIMCP_ERROR_SYSTEM, "Cond timedwait failed: %s", strerror(result));
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_SYSTEM, "system error");
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Signal one waiting thread (Adapter for pthread_cond_signal)
 *
 * WHY SIGNAL:
 * - Wake one waiter (efficient for single consumer)
 * - Notify condition change
 * - Producer-consumer pattern
 *
 * TYPICAL USAGE (producer):
 *   nimcp_mutex_lock(&mutex);
 *   queue_push(item);        // Modify condition
 *   condition = true;        // Set flag
 *   nimcp_cond_signal(&cond);  // Wake one waiter
 *   nimcp_mutex_unlock(&mutex);
 *
 * WHY SIGNAL UNDER LOCK:
 * - Not required but recommended
 * - Prevents "wake-up waiting" race
 * - Waiter can immediately check condition
 *
 * SIGNAL vs BROADCAST:
 * - Use signal for single consumer (work queue)
 * - Use broadcast for multiple consumers (barrier)
 *
 * COMPLEXITY: O(1) (wakes one thread)
 * THREAD SAFETY: Fully safe
 *
 * @param cond Condition variable to signal
 * @return NIMCP_SUCCESS or NIMCP_ERROR_INVALID_PARAM or NIMCP_ERROR_SYSTEM
 */
nimcp_result_t nimcp_cond_signal(nimcp_cond_t* cond)
{
    NIMCP_CHECK_THROW(cond, NIMCP_ERROR_INVALID_PARAM, "cond is NULL");

    int result = nimcp_platform_cond_signal(cond);
    if (result != 0) {
        set_thread_error(NIMCP_ERROR_SYSTEM, "Cond signal failed: %s", strerror(result));
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_SYSTEM, "system error");
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Signal all waiting threads (Adapter for pthread_cond_broadcast)
 *
 * WHY BROADCAST:
 * - Wake ALL waiters (multiple consumers)
 * - Barrier pattern (release all threads at once)
 * - Shutdown signal (wake all to exit)
 *
 * TYPICAL USAGE (shutdown):
 *   nimcp_mutex_lock(&mutex);
 *   shutdown_flag = true;        // Set condition
 *   nimcp_cond_broadcast(&cond);   // Wake all waiters
 *   nimcp_mutex_unlock(&mutex);
 *
 * THUNDERING HERD CONCERN:
 * - All threads wake simultaneously
 * - Serialize on mutex acquisition
 * - Most threads may find condition already consumed
 * - Trade-off: Simplicity vs efficiency
 *
 * WHEN TO USE BROADCAST (not signal):
 * - Multiple threads need to check same condition
 * - Barrier synchronization
 * - Shutdown/cleanup operations
 *
 * COMPLEXITY: O(n) where n = number of waiters (wake all + n context switches)
 * THREAD SAFETY: Fully safe
 *
 * @param cond Condition variable to broadcast
 * @return NIMCP_SUCCESS or NIMCP_ERROR_INVALID_PARAM or NIMCP_ERROR_SYSTEM
 */
nimcp_result_t nimcp_cond_broadcast(nimcp_cond_t* cond)
{
    NIMCP_CHECK_THROW(cond, NIMCP_ERROR_INVALID_PARAM, "cond is NULL");

    int result = nimcp_platform_cond_broadcast(cond);
    if (result != 0) {
        set_thread_error(NIMCP_ERROR_SYSTEM, "Cond broadcast failed: %s", strerror(result));
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_SYSTEM, "system error");
    }

    return NIMCP_SUCCESS;
}
