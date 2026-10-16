/**
 * @file nimcp_platform_mutex.h
 * @brief Cross-platform mutex abstraction (SRP: Mutex operations only)
 *
 * WHAT: Platform-agnostic mutex API
 * WHY:  pthread_mutex doesn't exist on Windows, need unified mutex API
 * HOW:  Wrap pthread_mutex (POSIX) and CRITICAL_SECTION (Windows)
 *
 * SRP: This module has ONE responsibility - mutex operations
 * DESIGN PATTERN: Adapter pattern
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_PLATFORM_MUTEX_H
#define NIMCP_PLATFORM_MUTEX_H

#include "utils/platform/nimcp_platform.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * MUTEX TYPE
 * ======================================================================== */

#if defined(NIMCP_PLATFORM_POSIX)
    #include <pthread.h>
    typedef pthread_mutex_t nimcp_platform_mutex_t;

#elif defined(NIMCP_PLATFORM_WINDOWS)
    #include <windows.h>
    typedef CRITICAL_SECTION nimcp_platform_mutex_t;

#else
    #error "Unsupported platform for mutex"
#endif

/* ========================================================================
 * MUTEX FUNCTIONS
 * ======================================================================== */

/**
 * WHAT: Initialize a mutex
 * WHY:  Must initialize before use
 * HOW:  Wrap pthread_mutex_init or InitializeCriticalSection
 *
 * @param mutex Mutex to initialize
 * @param recursive true for recursive mutex, false for normal
 * @return 0 on success, error code on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (on different mutexes)
 */
int nimcp_platform_mutex_init(nimcp_platform_mutex_t* mutex, bool recursive);

/**
 * WHAT: Destroy a mutex
 * WHY:  Free resources
 * HOW:  Wrap pthread_mutex_destroy or DeleteCriticalSection
 *
 * @param mutex Mutex to destroy
 * @return 0 on success, error code on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No (must not be locked)
 */
int nimcp_platform_mutex_destroy(nimcp_platform_mutex_t* mutex);

/**
 * WHAT: Lock a mutex (blocking)
 * WHY:  Acquire exclusive access
 * HOW:  Wrap pthread_mutex_lock or EnterCriticalSection
 *
 * @param mutex Mutex to lock
 * @return 0 on success, error code on failure
 *
 * COMPLEXITY: O(1) + wait time
 * THREAD-SAFE: Yes
 */
int nimcp_platform_mutex_lock(nimcp_platform_mutex_t* mutex);

/**
 * WHAT: Try to lock mutex (non-blocking)
 * WHY:  Avoid blocking if mutex busy
 * HOW:  Wrap pthread_mutex_trylock or TryEnterCriticalSection
 *
 * @param mutex Mutex to try locking
 * @return 0 on success, EBUSY if locked, error code otherwise
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int nimcp_platform_mutex_trylock(nimcp_platform_mutex_t* mutex);

/**
 * WHAT: Unlock a mutex
 * WHY:  Release exclusive access
 * HOW:  Wrap pthread_mutex_unlock or LeaveCriticalSection
 *
 * @param mutex Mutex to unlock
 * @return 0 on success, error code on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int nimcp_platform_mutex_unlock(nimcp_platform_mutex_t* mutex);

/**
 * WHAT: Create and initialize a mutex (dynamically allocated)
 * WHY:  Convenience wrapper for heap-allocated mutexes
 * HOW:  Allocates mutex and calls nimcp_platform_mutex_init
 *
 * @return Pointer to initialized mutex or NULL on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
nimcp_platform_mutex_t* nimcp_platform_mutex_create(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PLATFORM_MUTEX_H */
