/**
 * @file nimcp_platform_rwlock.h
 * @brief Cross-platform read-write lock abstraction (SRP: RWLock operations only)
 *
 * WHAT: Platform-agnostic read-write lock API
 * WHY:  pthread_rwlock doesn't exist on Windows, need unified rwlock API
 * HOW:  Wrap pthread_rwlock (POSIX) and SRWLOCK (Windows Vista+)
 *
 * SRP: This module has ONE responsibility - read-write lock operations
 * DESIGN PATTERN: Adapter pattern
 *
 * BEHAVIOR:
 * - Multiple readers can hold the lock simultaneously
 * - Only one writer can hold the lock (exclusive)
 * - Writers have priority (policy varies by platform)
 * - All operations block until lock acquired
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_PLATFORM_RWLOCK_H
#define NIMCP_PLATFORM_RWLOCK_H

#include "utils/platform/nimcp_platform.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * READ-WRITE LOCK TYPE
 * ======================================================================== */

/**
 * WHAT: Cross-platform read-write lock type
 * WHY:  POSIX and Windows have different types
 * HOW:  Use conditional typedef for platform
 *
 * NOTES:
 * - POSIX: pthread_rwlock_t is standard
 * - Windows: SRWLOCK is built-in type (initialized to 0)
 */

#if defined(NIMCP_PLATFORM_POSIX)
    #include <pthread.h>
    typedef pthread_rwlock_t nimcp_platform_rwlock_t;

#elif defined(NIMCP_PLATFORM_WINDOWS)
    #include <windows.h>
    typedef SRWLOCK nimcp_platform_rwlock_t;

#else
    #error "Unsupported platform for read-write locks"
#endif

/* ========================================================================
 * READ-WRITE LOCK FUNCTIONS
 * ======================================================================== */

/**
 * WHAT: Initialize a read-write lock
 * WHY:  Must initialize before use
 * HOW:  Wrap pthread_rwlock_init or SRWLOCK_INIT
 *
 * NOTES:
 * - On POSIX: Initializes pthread_rwlock_t
 * - On Windows: SRWLOCK is zero-initialized on creation, but this function
 *   provides explicit initialization for consistency
 *
 * @param rwlock Read-write lock to initialize
 * @return 0 on success, error code on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (on different locks)
 */
int nimcp_platform_rwlock_init(nimcp_platform_rwlock_t* rwlock);

/**
 * WHAT: Destroy a read-write lock
 * WHY:  Free resources
 * HOW:  Wrap pthread_rwlock_destroy or no-op (Windows SRWLOCK)
 *
 * NOTES:
 * - On POSIX: Destroys pthread_rwlock_t
 * - On Windows: SRWLOCK doesn't require explicit cleanup
 * - Must not be locked when destroyed
 *
 * @param rwlock Read-write lock to destroy
 * @return 0 on success, error code on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No (must not be locked)
 */
int nimcp_platform_rwlock_destroy(nimcp_platform_rwlock_t* rwlock);

/**
 * WHAT: Acquire read lock (shared access)
 * WHY:  Multiple readers can hold lock simultaneously
 * HOW:  Wrap pthread_rwlock_rdlock or AcquireSRWLockShared
 *
 * NOTES:
 * - Blocks if exclusive writer holds lock
 * - Multiple readers can proceed concurrently
 * - POSIX writer preference: writer may starve readers
 * - Windows writer priority: SRWLOCK prefers writers
 *
 * @param rwlock Read-write lock to acquire
 * @return 0 on success, error code on failure
 *
 * COMPLEXITY: O(1) + wait time
 * THREAD-SAFE: Yes
 */
int nimcp_platform_rwlock_rdlock(nimcp_platform_rwlock_t* rwlock);

/**
 * WHAT: Acquire write lock (exclusive access)
 * WHY:  Only one writer, no readers allowed
 * HOW:  Wrap pthread_rwlock_wrlock or AcquireSRWLockExclusive
 *
 * NOTES:
 * - Blocks if any readers or writers hold lock
 * - Exclusive access - no concurrent readers or writers
 * - POSIX writer preference: writer may starve readers
 * - Windows writer priority: SRWLOCK prefers writers
 *
 * @param rwlock Read-write lock to acquire
 * @return 0 on success, error code on failure
 *
 * COMPLEXITY: O(1) + wait time
 * THREAD-SAFE: Yes
 */
int nimcp_platform_rwlock_wrlock(nimcp_platform_rwlock_t* rwlock);

/**
 * WHAT: Try to acquire read lock (non-blocking)
 * WHY:  Avoid blocking if exclusive writer holds lock
 * HOW:  Wrap pthread_rwlock_tryrdlock or Windows SRW try pattern
 *
 * NOTES:
 * - Returns immediately with EBUSY if lock not available
 * - Windows doesn't have native trylock for SRWLOCK
 * - Windows implementation uses TryAcquireSRWLockShared (Windows 7+)
 *
 * @param rwlock Read-write lock to try locking
 * @return 0 on success, EBUSY if locked, error code otherwise
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int nimcp_platform_rwlock_tryrdlock(nimcp_platform_rwlock_t* rwlock);

/**
 * WHAT: Try to acquire write lock (non-blocking)
 * WHY:  Avoid blocking if any readers/writers hold lock
 * HOW:  Wrap pthread_rwlock_trywrlock or Windows SRW try pattern
 *
 * NOTES:
 * - Returns immediately with EBUSY if lock not available
 * - Windows doesn't have native trylock for SRWLOCK
 * - Windows implementation uses TryAcquireSRWLockExclusive (Windows 7+)
 *
 * @param rwlock Read-write lock to try locking
 * @return 0 on success, EBUSY if locked, error code otherwise
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int nimcp_platform_rwlock_trywrlock(nimcp_platform_rwlock_t* rwlock);

/**
 * WHAT: Release read lock
 * WHY:  Allow other waiters to proceed
 * HOW:  Wrap pthread_rwlock_unlock or ReleaseSRWLockShared
 *
 * NOTES:
 * - Only call after successful rdlock
 * - On Windows: SRWLOCK requires explicit Shared/Exclusive release
 * - Must track which mode lock was acquired in (handled by caller)
 * - For simplicity, applications typically use separate unlock functions
 *
 * @param rwlock Read-write lock to release
 * @return 0 on success, error code on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int nimcp_platform_rwlock_rdunlock(nimcp_platform_rwlock_t* rwlock);

/**
 * WHAT: Release write lock
 * WHY:  Allow other waiters to proceed
 * HOW:  Wrap pthread_rwlock_unlock or ReleaseSRWLockExclusive
 *
 * NOTES:
 * - Only call after successful wrlock
 * - On Windows: SRWLOCK requires explicit Shared/Exclusive release
 * - Must track which mode lock was acquired in (handled by caller)
 * - For simplicity, applications typically use separate unlock functions
 *
 * @param rwlock Read-write lock to release
 * @return 0 on success, error code on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int nimcp_platform_rwlock_wrunlock(nimcp_platform_rwlock_t* rwlock);

/**
 * @brief Generic unlock for read-write lock
 *
 * WHAT: Release lock from either read or write mode
 * WHY:  Convenience wrapper when caller knows they hold the lock
 * HOW:  On POSIX, same as pthread_rwlock_unlock (works for both)
 *       On Windows, this is a NO-OP since SRW locks auto-detect
 *
 * NOTE: For maximum portability, prefer the specific unlock functions
 *       (nimcp_platform_rwlock_rdunlock/wrunlock) when the lock type is known.
 *       This function is provided for convenience when the lock mode is tracked.
 *
 * @param rwlock Read-write lock to release
 * @return 0 on success, error code on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int nimcp_platform_rwlock_unlock(nimcp_platform_rwlock_t* rwlock);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PLATFORM_RWLOCK_H */
