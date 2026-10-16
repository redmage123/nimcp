#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_thread_rwlock.c - Read-Write Lock Operations
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
#include <time.h>
#include <pthread.h>

#define LOG_MODULE "thread_rwlock"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(thread_rwlock)

// External declarations for error handling (defined in nimcp_thread.c)
extern void set_thread_error(int error_code, const char* format, ...);

//=============================================================================
// Read-Write Lock Operations
//=============================================================================

/**
 * @brief Initialize read-write lock (Adapter for pthread_rwlock_init)
 *
 * WHY READ-WRITE LOCKS:
 * - Allow multiple concurrent readers (shared access)
 * - Exclusive write access (only one writer, no readers)
 * - Better performance for read-heavy workloads
 * - Used in neuromodulator system for concurrent reads
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Fully safe (creates new independent lock)
 *
 * @param lock Read-write lock to initialize
 * @return NIMCP_SUCCESS or NIMCP_ERROR_INVALID_PARAM or NIMCP_ERROR_SYSTEM
 */
nimcp_result_t nimcp_rwlock_init(nimcp_rwlock_t* lock)
{
    if (!lock) {
        set_thread_error(NIMCP_ERROR_INVALID_PARAM, "Invalid rwlock pointer");
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_PARAM, "invalid parameter");
    }

    int result = pthread_rwlock_init(lock, NULL);
    if (result != 0) {
        set_thread_error(NIMCP_ERROR_SYSTEM, "RWlock initialization failed: %s", strerror(result));
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_SYSTEM, "system error");
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Destroy read-write lock (Adapter for pthread_rwlock_destroy)
 *
 * PRECONDITIONS:
 * - Lock must not be held by any thread
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Caller must ensure no concurrent use
 *
 * @param lock Read-write lock to destroy
 * @return NIMCP_SUCCESS or NIMCP_ERROR_INVALID_PARAM or NIMCP_ERROR_SYSTEM
 */
nimcp_result_t nimcp_rwlock_destroy(nimcp_rwlock_t* lock)
{
    if (!lock) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_PARAM, "invalid parameter");
    }

    int result = pthread_rwlock_destroy(lock);
    if (result != 0) {
        set_thread_error(NIMCP_ERROR_SYSTEM, "RWlock destruction failed: %s", strerror(result));
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_SYSTEM, "system error");
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Acquire read lock (Adapter for pthread_rwlock_rdlock)
 *
 * WHY READ LOCK:
 * - Multiple readers can hold lock simultaneously
 * - Blocks if writer holds lock
 * - Allows concurrent read access to shared data
 *
 * COMPLEXITY: O(1) if no writers
 * THREAD SAFETY: Fully safe
 *
 * @param lock Read-write lock
 * @return NIMCP_SUCCESS or NIMCP_ERROR_INVALID_PARAM or NIMCP_ERROR_SYSTEM
 */
nimcp_result_t nimcp_rwlock_rdlock(nimcp_rwlock_t* lock)
{
    if (!lock) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_PARAM, "invalid parameter");
    }

    int result = pthread_rwlock_rdlock(lock);
    if (result != 0) {
        set_thread_error(NIMCP_ERROR_SYSTEM, "RWlock rdlock failed: %s", strerror(result));
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_SYSTEM, "system error");
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Acquire write lock (Adapter for pthread_rwlock_wrlock)
 *
 * WHY WRITE LOCK:
 * - Exclusive access (no readers, no other writers)
 * - Blocks until all readers and writers release lock
 * - Required for modifying shared data
 *
 * COMPLEXITY: O(1) once acquired (may wait for readers to finish)
 * THREAD SAFETY: Fully safe
 *
 * @param lock Read-write lock
 * @return NIMCP_SUCCESS or NIMCP_ERROR_INVALID_PARAM or NIMCP_ERROR_SYSTEM
 */
nimcp_result_t nimcp_rwlock_wrlock(nimcp_rwlock_t* lock)
{
    if (!lock) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_PARAM, "invalid parameter");
    }

    int result = pthread_rwlock_wrlock(lock);
    if (result != 0) {
        set_thread_error(NIMCP_ERROR_SYSTEM, "RWlock wrlock failed: %s", strerror(result));
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_SYSTEM, "system error");
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Unlock read-write lock (Adapter for pthread_rwlock_unlock)
 *
 * WHY UNLOCK:
 * - Release read or write lock
 * - Allow other threads to acquire lock
 * - Works for both read and write locks
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Must be called by thread that acquired lock
 *
 * @param lock Read-write lock to unlock
 * @return NIMCP_SUCCESS or NIMCP_ERROR_INVALID_PARAM or NIMCP_ERROR_SYSTEM
 */
nimcp_result_t nimcp_rwlock_unlock(nimcp_rwlock_t* lock)
{
    if (!lock) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_PARAM, "invalid parameter");
    }

    int result = pthread_rwlock_unlock(lock);
    if (result != 0) {
        set_thread_error(NIMCP_ERROR_SYSTEM, "RWlock unlock failed: %s", strerror(result));
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_SYSTEM, "system error");
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Try to acquire read lock without blocking (Adapter for pthread_rwlock_tryrdlock)
 *
 * WHY TRYRDLOCK:
 * - Non-blocking alternative to rdlock
 * - Allows opportunistic read access patterns
 * - Useful for lock-free read paths with fallback
 *
 * COMPLEXITY: O(1) (always non-blocking)
 * THREAD SAFETY: Fully safe
 *
 * @param lock Read-write lock
 * @return NIMCP_SUCCESS if locked, NIMCP_BUSY if would block, NIMCP_ERROR_* on error
 */
nimcp_result_t nimcp_rwlock_tryrdlock(nimcp_rwlock_t* lock)
{
    if (!lock) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_PARAM, "invalid parameter");
    }

    int result = pthread_rwlock_tryrdlock(lock);

    // EBUSY means lock is held by writer (expected, not error)
    if (result == EBUSY) {
        return NIMCP_BUSY;
    } else if (result != 0) {
        set_thread_error(NIMCP_ERROR_SYSTEM, "RWlock tryrdlock failed: %s", strerror(result));
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_SYSTEM, "system error");
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Try to acquire write lock without blocking (Adapter for pthread_rwlock_trywrlock)
 *
 * WHY TRYWRLOCK:
 * - Non-blocking alternative to wrlock
 * - Avoid deadlock in complex lock hierarchies
 * - Allows try-and-defer patterns for write operations
 *
 * COMPLEXITY: O(1) (always non-blocking)
 * THREAD SAFETY: Fully safe
 *
 * @param lock Read-write lock
 * @return NIMCP_SUCCESS if locked, NIMCP_BUSY if would block, NIMCP_ERROR_* on error
 */
nimcp_result_t nimcp_rwlock_trywrlock(nimcp_rwlock_t* lock)
{
    if (!lock) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_PARAM, "invalid parameter");
    }

    int result = pthread_rwlock_trywrlock(lock);

    // EBUSY means lock is held (expected, not error)
    if (result == EBUSY) {
        return NIMCP_BUSY;
    } else if (result != 0) {
        set_thread_error(NIMCP_ERROR_SYSTEM, "RWlock trywrlock failed: %s", strerror(result));
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_SYSTEM, "system error");
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Try to acquire read lock with timeout (Adapter for pthread_rwlock_timedrdlock)
 *
 * WHY TIMED READ LOCK:
 * - Prevent indefinite blocking on read operations
 * - Implement watchdog patterns for read access
 * - Graceful degradation when write lock held too long
 *
 * COMPLEXITY: O(1) to enter wait, blocks up to timeout_ms
 * THREAD SAFETY: Fully safe
 *
 * @param lock Read-write lock
 * @param timeout_ms Timeout in milliseconds
 * @return NIMCP_SUCCESS if locked, NIMCP_BUSY if timeout, NIMCP_ERROR_* on error
 */
nimcp_result_t nimcp_rwlock_timedrdlock(nimcp_rwlock_t* lock, uint32_t timeout_ms)
{
    if (!lock) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_PARAM, "invalid parameter");
    }

    // Calculate absolute timeout (CLOCK_REALTIME)
    struct timespec abstime;
    if (clock_gettime(CLOCK_REALTIME, &abstime) != 0) {
        set_thread_error(NIMCP_ERROR_SYSTEM, "clock_gettime failed: %s", strerror(errno));
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_SYSTEM, "system error");
    }

    // Add timeout milliseconds to current time
    abstime.tv_sec += timeout_ms / 1000;
    abstime.tv_nsec += (timeout_ms % 1000) * 1000000L;

    // Handle nanosecond overflow (carry to seconds)
    if (abstime.tv_nsec >= 1000000000L) {
        abstime.tv_sec += 1;
        abstime.tv_nsec -= 1000000000L;
    }

    int result = pthread_rwlock_timedrdlock(lock, &abstime);

    // ETIMEDOUT is expected outcome (timeout expired)
    if (result == ETIMEDOUT) {
        return NIMCP_BUSY;  // Consistent with trylock semantics
    } else if (result != 0) {
        set_thread_error(NIMCP_ERROR_SYSTEM, "RWlock timedrdlock failed: %s", strerror(result));
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_SYSTEM, "system error");
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Try to acquire write lock with timeout (Adapter for pthread_rwlock_timedwrlock)
 *
 * WHY TIMED WRITE LOCK:
 * - Prevent indefinite blocking on write operations
 * - Implement watchdog patterns for write access
 * - Detect excessive reader activity (starvation prevention)
 *
 * COMPLEXITY: O(1) to enter wait, blocks up to timeout_ms
 * THREAD SAFETY: Fully safe
 *
 * @param lock Read-write lock
 * @param timeout_ms Timeout in milliseconds
 * @return NIMCP_SUCCESS if locked, NIMCP_BUSY if timeout, NIMCP_ERROR_* on error
 */
nimcp_result_t nimcp_rwlock_timedwrlock(nimcp_rwlock_t* lock, uint32_t timeout_ms)
{
    if (!lock) {
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_PARAM, "invalid parameter");
    }

    // Calculate absolute timeout (CLOCK_REALTIME)
    struct timespec abstime;
    if (clock_gettime(CLOCK_REALTIME, &abstime) != 0) {
        set_thread_error(NIMCP_ERROR_SYSTEM, "clock_gettime failed: %s", strerror(errno));
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_SYSTEM, "system error");
    }

    // Add timeout milliseconds to current time
    abstime.tv_sec += timeout_ms / 1000;
    abstime.tv_nsec += (timeout_ms % 1000) * 1000000L;

    // Handle nanosecond overflow (carry to seconds)
    if (abstime.tv_nsec >= 1000000000L) {
        abstime.tv_sec += 1;
        abstime.tv_nsec -= 1000000000L;
    }

    int result = pthread_rwlock_timedwrlock(lock, &abstime);

    // ETIMEDOUT is expected outcome (timeout expired)
    if (result == ETIMEDOUT) {
        return NIMCP_BUSY;  // Consistent with trylock semantics
    } else if (result != 0) {
        set_thread_error(NIMCP_ERROR_SYSTEM, "RWlock timedwrlock failed: %s", strerror(result));
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_SYSTEM, "system error");
    }

    return NIMCP_SUCCESS;
}
