/**
 * @file nimcp_platform_thread.h
 * @brief Cross-platform thread abstraction (SRP: Thread lifecycle only)
 *
 * WHAT: Platform-agnostic thread creation/management API
 * WHY:  pthread doesn't exist on Windows, need unified thread API
 * HOW:  Wrap pthread (POSIX) and Windows threads
 *
 * SRP: This module has ONE responsibility - thread lifecycle management
 * DESIGN PATTERN: Adapter pattern
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_PLATFORM_THREAD_H
#define NIMCP_PLATFORM_THREAD_H

#include "utils/platform/nimcp_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * THREAD TYPE
 * ======================================================================== */

#if defined(NIMCP_PLATFORM_POSIX)
    #include <pthread.h>
    typedef pthread_t nimcp_platform_thread_t;

#elif defined(NIMCP_PLATFORM_WINDOWS)
    #include <windows.h>
    typedef HANDLE nimcp_platform_thread_t;

#else
    #error "Unsupported platform for threading"
#endif

/* ========================================================================
 * THREAD FUNCTIONS
 * ======================================================================== */

/**
 * WHAT: Thread start routine signature
 * WHY:  Same across platforms (void* in, void* out)
 * HOW:  Function pointer typedef
 */
typedef void* (*nimcp_thread_func_t)(void* arg);

/**
 * WHAT: Create and start a new thread
 * WHY:  Cross-platform thread creation
 * HOW:  Wrap pthread_create or CreateThread
 *
 * @param thread Output: thread handle
 * @param start_routine Function to run in thread
 * @param arg Argument passed to start_routine
 * @return 0 on success, error code on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int nimcp_platform_thread_create(nimcp_platform_thread_t* thread,
                                 nimcp_thread_func_t start_routine,
                                 void* arg);

/**
 * WHAT: Wait for thread to complete
 * WHY:  Join thread, get return value
 * HOW:  Wrap pthread_join or WaitForSingleObject
 *
 * @param thread Thread to wait for
 * @param retval Output: thread return value (can be NULL)
 * @return 0 on success, error code on failure
 *
 * COMPLEXITY: O(1) + wait time
 * THREAD-SAFE: Yes
 */
int nimcp_platform_thread_join(nimcp_platform_thread_t thread, void** retval);

/**
 * WHAT: Detach thread (runs independently)
 * WHY:  Thread cleans up automatically when done
 * HOW:  Wrap pthread_detach or CloseHandle
 *
 * @param thread Thread to detach
 * @return 0 on success, error code on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int nimcp_platform_thread_detach(nimcp_platform_thread_t thread);

/**
 * WHAT: Get current thread ID
 * WHY:  Identify which thread is running
 * HOW:  Wrap pthread_self or GetCurrentThreadId
 *
 * @return Current thread handle/ID
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
nimcp_platform_thread_t nimcp_platform_thread_self(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PLATFORM_THREAD_H */
