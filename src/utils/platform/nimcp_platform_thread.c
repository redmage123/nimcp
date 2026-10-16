/**
 * @file nimcp_platform_thread.c
 * @brief Cross-platform thread implementation (SRP: Thread lifecycle only)
 *
 * WHAT: Platform-agnostic thread creation/management
 * WHY:  pthread doesn't exist on Windows
 * HOW:  Wrap pthread (POSIX) and Windows threads
 *
 * SRP: This module has ONE responsibility - thread lifecycle management
 * COMPLEXITY: O(1) for all operations
 * THREAD-SAFE: Yes - all functions thread-safe
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include "utils/platform/nimcp_platform_thread.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <errno.h>
#include <stdlib.h>
#include "api/nimcp_api_exception.h"

#if defined(NIMCP_PLATFORM_WINDOWS)
    #include <process.h>  /* _beginthreadex */
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(platform_thread)

#endif

/* ========================================================================
 * WINDOWS-SPECIFIC HELPERS
 * ======================================================================== */

#if defined(NIMCP_PLATFORM_WINDOWS)

/**
 * WHAT: Wrapper structure for Windows thread start
 * WHY:  Windows thread function signature differs from POSIX
 * HOW:  Store POSIX-style function pointer and arg, translate return value
 *
 * LIMITATION: If the thread is terminated externally (via TerminateThread or
 * similar), this wrapper structure will leak. This is a known limitation.
 * Callers should ensure threads exit normally via their function return or
 * use nimcp_platform_thread_join() to wait for completion. External termination
 * of threads is strongly discouraged as it can leave the program in an
 * inconsistent state.
 */
typedef struct {
    nimcp_thread_func_t func;
    void* arg;
    void* retval;
} win_thread_wrapper_t;

/**
 * WHAT: Windows thread start routine wrapper
 * WHY:  Windows expects DWORD WINAPI func(LPVOID), POSIX expects void* func(void*)
 * HOW:  Translate between signatures, store return value
 *
 * LIMITATION: The wrapper is freed only when the thread function completes
 * normally. If the thread is terminated externally (TerminateThread), the
 * wrapper will leak. This is acceptable because:
 * 1. External thread termination is inherently unsafe and discouraged
 * 2. Using thread-local storage to fix this would add significant complexity
 * 3. The leak size is small (sizeof(win_thread_wrapper_t) = ~24 bytes)
 *
 * RECOMMENDATION: Always let threads complete naturally or join them properly.
 */
static DWORD WINAPI win_thread_start(LPVOID arg)
{
    win_thread_wrapper_t* wrapper = (win_thread_wrapper_t*)arg;
    wrapper->retval = wrapper->func(wrapper->arg);
    nimcp_free(wrapper);  /* Cleanup wrapper after thread completes */
    return 0;  /* Windows thread exit code (unused) */
}

#endif /* NIMCP_PLATFORM_WINDOWS */

/* ========================================================================
 * THREAD FUNCTIONS
 * ======================================================================== */

int nimcp_platform_thread_create(nimcp_platform_thread_t* thread,
                                 nimcp_thread_func_t start_routine,
                                 void* arg)
{
    if (!thread) {
        LOG_ERROR("nimcp_platform_thread_create: thread pointer is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Thread create failed: NULL thread pointer");
        return EINVAL;
    }
    if (!start_routine) {
        LOG_ERROR("nimcp_platform_thread_create: start_routine is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Thread create failed: NULL start routine");
        return EINVAL;
    }

#if defined(NIMCP_PLATFORM_POSIX)
    /* POSIX: Direct pthread_create */
    int result = pthread_create(thread, NULL, start_routine, arg);
    return result;

#elif defined(NIMCP_PLATFORM_WINDOWS)
    /* Windows: Create thread with wrapper */
    win_thread_wrapper_t* wrapper = (win_thread_wrapper_t*)nimcp_malloc(sizeof(win_thread_wrapper_t));
    if (!wrapper) {
        LOG_ERROR("nimcp_platform_thread_create: failed to allocate thread wrapper");
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(win_thread_wrapper_t), "Thread wrapper allocation failed");
        return ENOMEM;
    }

    wrapper->func = start_routine;
    wrapper->arg = arg;
    wrapper->retval = NULL;

    HANDLE handle = (HANDLE)_beginthreadex(
        NULL,                 /* Security attributes */
        0,                    /* Stack size (0 = default) */
        (unsigned (__stdcall *)(void *))win_thread_start,
        wrapper,              /* Argument */
        0,                    /* Creation flags */
        NULL                  /* Thread ID (unused) */
    );

    if (handle == 0) {
        LOG_ERROR("nimcp_platform_thread_create: _beginthreadex failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_THREAD_CREATE, "Thread creation failed on Windows");
        nimcp_free(wrapper);
        return EAGAIN;  /* Thread creation failed */
    }

    *thread = handle;
    return 0;

#else
    #error "Unsupported platform"
#endif
}

int nimcp_platform_thread_join(nimcp_platform_thread_t thread, void** retval)
{
#if defined(NIMCP_PLATFORM_POSIX)
    /* POSIX: Direct pthread_join */
    return pthread_join(thread, retval);

#elif defined(NIMCP_PLATFORM_WINDOWS)
    /* Windows: Wait for thread completion */
    DWORD result = WaitForSingleObject(thread, INFINITE);
    if (result != WAIT_OBJECT_0) {
        return EINVAL;
    }

    /* Note: retval not supported on Windows with current implementation */
    if (retval) {
        *retval = NULL;  /* Windows limitation */
    }

    CloseHandle(thread);
    return 0;

#else
    #error "Unsupported platform"
#endif
}

int nimcp_platform_thread_detach(nimcp_platform_thread_t thread)
{
#if defined(NIMCP_PLATFORM_POSIX)
    /* POSIX: Direct pthread_detach */
    return pthread_detach(thread);

#elif defined(NIMCP_PLATFORM_WINDOWS)
    /* Windows: Close handle (thread runs independently) */
    if (!CloseHandle(thread)) {
        return EINVAL;
    }
    return 0;

#else
    #error "Unsupported platform"
#endif
}

nimcp_platform_thread_t nimcp_platform_thread_self(void)
{
#if defined(NIMCP_PLATFORM_POSIX)
    return pthread_self();

#elif defined(NIMCP_PLATFORM_WINDOWS)
    return GetCurrentThread();

#else
    #error "Unsupported platform"
#endif
}
