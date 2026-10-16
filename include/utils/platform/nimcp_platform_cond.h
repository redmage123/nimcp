/**
 * @file nimcp_platform_cond.h
 * @brief Cross-platform condition variable abstraction (SRP: Condition variables only)
 *
 * WHAT: Platform-agnostic condition variable API
 * WHY:  pthread_cond doesn't exist on Windows, need unified CV API
 * HOW:  Wrap pthread_cond (POSIX) and Event objects (Windows)
 *
 * SRP: This module has ONE responsibility - condition variable operations
 * DESIGN PATTERN: Adapter pattern
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_PLATFORM_COND_H
#define NIMCP_PLATFORM_COND_H

#include "utils/platform/nimcp_platform.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * CONDITION VARIABLE TYPE
 * ======================================================================== */

#if defined(NIMCP_PLATFORM_POSIX)
    #include <pthread.h>
    typedef pthread_cond_t nimcp_platform_cond_t;

#elif defined(NIMCP_PLATFORM_WINDOWS)
    #include <windows.h>
    typedef CONDITION_VARIABLE nimcp_platform_cond_t;

#else
    #error "Unsupported platform for condition variables"
#endif

/* ========================================================================
 * CONDITION VARIABLE FUNCTIONS
 * ======================================================================== */

/**
 * WHAT: Initialize condition variable
 * WHY:  Must initialize before use
 * HOW:  Wrap pthread_cond_init or InitializeConditionVariable
 *
 * @param cond Condition variable to initialize
 * @return 0 on success, error code on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (on different cond vars)
 */
int nimcp_platform_cond_init(nimcp_platform_cond_t* cond);

/**
 * WHAT: Destroy condition variable
 * WHY:  Free resources
 * HOW:  Wrap pthread_cond_destroy (no-op on Windows)
 *
 * @param cond Condition variable to destroy
 * @return 0 on success, error code on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No (must not be in use)
 */
int nimcp_platform_cond_destroy(nimcp_platform_cond_t* cond);

/**
 * WHAT: Wait on condition variable
 * WHY:  Block until signaled
 * HOW:  Wrap pthread_cond_wait or SleepConditionVariableCS
 *
 * @param cond Condition variable to wait on
 * @param mutex Associated mutex (must be locked)
 * @return 0 on success, error code on failure
 *
 * COMPLEXITY: O(1) + wait time
 * THREAD-SAFE: Yes
 */
int nimcp_platform_cond_wait(nimcp_platform_cond_t* cond,
                             nimcp_platform_mutex_t* mutex);

/**
 * WHAT: Wait on condition variable with timeout
 * WHY:  Block until signaled or timeout
 * HOW:  Wrap pthread_cond_timedwait or SleepConditionVariableCS
 *
 * @param cond Condition variable
 * @param mutex Associated mutex (must be locked)
 * @param timeout_ms Timeout in milliseconds
 * @return 0 on success, ETIMEDOUT on timeout, error code otherwise
 *
 * COMPLEXITY: O(1) + wait time
 * THREAD-SAFE: Yes
 */
int nimcp_platform_cond_timedwait(nimcp_platform_cond_t* cond,
                                  nimcp_platform_mutex_t* mutex,
                                  uint32_t timeout_ms);

/**
 * WHAT: Signal one waiting thread
 * WHY:  Wake up single waiter
 * HOW:  Wrap pthread_cond_signal or WakeConditionVariable
 *
 * @param cond Condition variable to signal
 * @return 0 on success, error code on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int nimcp_platform_cond_signal(nimcp_platform_cond_t* cond);

/**
 * WHAT: Signal all waiting threads
 * WHY:  Wake up all waiters
 * HOW:  Wrap pthread_cond_broadcast or WakeAllConditionVariable
 *
 * @param cond Condition variable to broadcast
 * @return 0 on success, error code on failure
 *
 * COMPLEXITY: O(n) where n = waiting threads
 * THREAD-SAFE: Yes
 */
int nimcp_platform_cond_broadcast(nimcp_platform_cond_t* cond);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PLATFORM_COND_H */
