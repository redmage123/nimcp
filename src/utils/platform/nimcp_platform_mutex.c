/**
 * @file nimcp_platform_mutex.c
 * @brief Cross-platform mutex implementation (SRP: Mutex operations only)
 *
 * WHAT: Platform-agnostic mutex implementation
 * WHY:  pthread_mutex doesn't exist on Windows
 * HOW:  Wrap pthread_mutex (POSIX) and CRITICAL_SECTION (Windows)
 *
 * SRP: This module has ONE responsibility - mutex operations
 * COMPLEXITY: O(1) for all operations
 * THREAD-SAFE: Yes - all functions thread-safe
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include "utils/platform/nimcp_platform_mutex.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
/* P1-6 FIX: Removed nimcp_exception_macros.h and nimcp_api_exception.h includes.
 * Platform mutex is the lowest layer - must not depend on exception/immune system
 * to avoid circular dependency (mutex -> immune -> mutex). */
#include <errno.h>
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(platform_mutex)

/* ========================================================================
 * MUTEX FUNCTIONS
 * ======================================================================== */

int nimcp_platform_mutex_init(nimcp_platform_mutex_t* mutex, bool recursive)
{
    if (!mutex) {
        /* P1-6 FIX: Platform layer must NOT depend on exception/immune system
         * (circular dependency: mutex -> immune -> mutex). Use LOG_ERROR only. */
        LOG_ERROR("nimcp_platform_mutex_init: mutex pointer is NULL");
        return EINVAL;
    }

#if defined(NIMCP_PLATFORM_POSIX)
    /* POSIX: pthread_mutex with optional recursive attribute */
    if (recursive) {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        int result = pthread_mutex_init(mutex, &attr);
        pthread_mutexattr_destroy(&attr);
        return result;
    } else {
        return pthread_mutex_init(mutex, NULL);
    }

#elif defined(NIMCP_PLATFORM_WINDOWS)
    /* Windows: CRITICAL_SECTION (always recursive) */
    InitializeCriticalSection(mutex);
    return 0;

#else
    #error "Unsupported platform"
#endif
}

int nimcp_platform_mutex_destroy(nimcp_platform_mutex_t* mutex)
{
    if (!mutex) {
        LOG_ERROR("nimcp_platform_mutex_destroy: mutex pointer is NULL");
        return EINVAL;
    }

#if defined(NIMCP_PLATFORM_POSIX)
    return pthread_mutex_destroy(mutex);

#elif defined(NIMCP_PLATFORM_WINDOWS)
    DeleteCriticalSection(mutex);
    return 0;

#else
    #error "Unsupported platform"
#endif
}

int nimcp_platform_mutex_lock(nimcp_platform_mutex_t* mutex)
{
    if (!mutex) {
        LOG_ERROR("nimcp_platform_mutex_lock: mutex pointer is NULL");
        return EINVAL;
    }

#if defined(NIMCP_PLATFORM_POSIX)
    return pthread_mutex_lock(mutex);

#elif defined(NIMCP_PLATFORM_WINDOWS)
    EnterCriticalSection(mutex);
    return 0;

#else
    #error "Unsupported platform"
#endif
}

int nimcp_platform_mutex_trylock(nimcp_platform_mutex_t* mutex)
{
    if (!mutex) {
        LOG_ERROR("nimcp_platform_mutex_trylock: mutex pointer is NULL");
        return EINVAL;
    }

#if defined(NIMCP_PLATFORM_POSIX)
    return pthread_mutex_trylock(mutex);

#elif defined(NIMCP_PLATFORM_WINDOWS)
    BOOL result = TryEnterCriticalSection(mutex);
    return result ? 0 : EBUSY;

#else
    #error "Unsupported platform"
#endif
}

int nimcp_platform_mutex_unlock(nimcp_platform_mutex_t* mutex)
{
    if (!mutex) {
        LOG_ERROR("nimcp_platform_mutex_unlock: mutex pointer is NULL");
        return EINVAL;
    }

#if defined(NIMCP_PLATFORM_POSIX)
    return pthread_mutex_unlock(mutex);

#elif defined(NIMCP_PLATFORM_WINDOWS)
    LeaveCriticalSection(mutex);
    return 0;

#else
    #error "Unsupported platform"
#endif
}

nimcp_platform_mutex_t* nimcp_platform_mutex_create(void)
{
    nimcp_platform_mutex_t* mutex = (nimcp_platform_mutex_t*)nimcp_malloc(
        sizeof(nimcp_platform_mutex_t)
    );
    /* P1-6 FIX: Don't use NIMCP_API_CHECK_ALLOC here - it calls into the
     * exception/immune system, creating a circular dependency. The platform
     * mutex layer is the lowest layer and must not depend upward. */
    if (!mutex) {
        LOG_ERROR("nimcp_platform_mutex_create: failed to allocate mutex structure");
        return NULL;
    }

    if (nimcp_platform_mutex_init(mutex, false) != 0) {
        LOG_ERROR("nimcp_platform_mutex_create: failed to initialize mutex");
        nimcp_free(mutex);
        return NULL;
    }

    return mutex;
}
