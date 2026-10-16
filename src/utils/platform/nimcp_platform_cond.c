/**
 * @file nimcp_platform_cond.c
 * @brief Cross-platform condition variable implementation (SRP: CVs only)
 *
 * WHAT: Platform-agnostic condition variable implementation
 * WHY:  pthread_cond doesn't exist on Windows
 * HOW:  Wrap pthread_cond (POSIX) and CONDITION_VARIABLE (Windows Vista+)
 *
 * SRP: This module has ONE responsibility - condition variable operations
 * COMPLEXITY: O(1) for signal, O(n) for broadcast
 * THREAD-SAFE: Yes - all functions thread-safe
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include "utils/platform/nimcp_platform_cond.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <errno.h>
#include "api/nimcp_api_exception.h"

#if defined(NIMCP_PLATFORM_POSIX)
    #include <sys/time.h>  /* gettimeofday for timedwait */
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(platform_cond)

#endif

/* ========================================================================
 * CONDITION VARIABLE FUNCTIONS
 * ======================================================================== */

int nimcp_platform_cond_init(nimcp_platform_cond_t* cond)
{
    if (!cond) {
        LOG_ERROR("nimcp_platform_cond_init: cond pointer is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Condition init failed: NULL pointer");
        return EINVAL;
    }

#if defined(NIMCP_PLATFORM_POSIX)
    return pthread_cond_init(cond, NULL);

#elif defined(NIMCP_PLATFORM_WINDOWS)
    /* Windows Vista+: Native condition variable support */
    InitializeConditionVariable(cond);
    return 0;

#else
    #error "Unsupported platform"
#endif
}

int nimcp_platform_cond_destroy(nimcp_platform_cond_t* cond)
{
    if (!cond) {
        LOG_ERROR("nimcp_platform_cond_destroy: cond pointer is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Condition destroy failed: NULL pointer");
        return EINVAL;
    }

#if defined(NIMCP_PLATFORM_POSIX)
    return pthread_cond_destroy(cond);

#elif defined(NIMCP_PLATFORM_WINDOWS)
    /* Windows: CONDITION_VARIABLE doesn't need explicit destruction */
    return 0;

#else
    #error "Unsupported platform"
#endif
}

int nimcp_platform_cond_wait(nimcp_platform_cond_t* cond,
                             nimcp_platform_mutex_t* mutex)
{
    if (!cond) {
        LOG_ERROR("nimcp_platform_cond_wait: cond pointer is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Condition wait failed: NULL cond pointer");
        return EINVAL;
    }
    if (!mutex) {
        LOG_ERROR("nimcp_platform_cond_wait: mutex pointer is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Condition wait failed: NULL mutex pointer");
        return EINVAL;
    }

#if defined(NIMCP_PLATFORM_POSIX)
    return pthread_cond_wait(cond, mutex);

#elif defined(NIMCP_PLATFORM_WINDOWS)
    /* Windows: SleepConditionVariableCS atomically releases mutex and waits */
    BOOL result = SleepConditionVariableCS(cond, mutex, INFINITE);
    return result ? 0 : EINVAL;

#else
    #error "Unsupported platform"
#endif
}

int nimcp_platform_cond_timedwait(nimcp_platform_cond_t* cond,
                                  nimcp_platform_mutex_t* mutex,
                                  uint32_t timeout_ms)
{
    if (!cond) {
        LOG_ERROR("nimcp_platform_cond_timedwait: cond pointer is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Condition timedwait failed: NULL cond pointer");
        return EINVAL;
    }
    if (!mutex) {
        LOG_ERROR("nimcp_platform_cond_timedwait: mutex pointer is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Condition timedwait failed: NULL mutex pointer");
        return EINVAL;
    }

#if defined(NIMCP_PLATFORM_POSIX)
    /* POSIX: Convert relative timeout to absolute timespec */
    struct timespec abs_timeout;
    struct timeval now;
    gettimeofday(&now, NULL);

    abs_timeout.tv_sec = now.tv_sec + (timeout_ms / 1000);
    abs_timeout.tv_nsec = (now.tv_usec * 1000) + ((timeout_ms % 1000) * 1000000);

    if (abs_timeout.tv_nsec >= 1000000000) {
        abs_timeout.tv_sec++;
        abs_timeout.tv_nsec -= 1000000000;
    }

    return pthread_cond_timedwait(cond, mutex, &abs_timeout);

#elif defined(NIMCP_PLATFORM_WINDOWS)
    /* Windows: SleepConditionVariableCS with timeout (in milliseconds) */
    BOOL result = SleepConditionVariableCS(cond, mutex, timeout_ms);

    if (result) {
        return 0;
    } else {
        DWORD error = GetLastError();
        return (error == ERROR_TIMEOUT) ? ETIMEDOUT : EINVAL;
    }

#else
    #error "Unsupported platform"
#endif
}

int nimcp_platform_cond_signal(nimcp_platform_cond_t* cond)
{
    if (!cond) {
        LOG_ERROR("nimcp_platform_cond_signal: cond pointer is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Condition signal failed: NULL pointer");
        return EINVAL;
    }

#if defined(NIMCP_PLATFORM_POSIX)
    return pthread_cond_signal(cond);

#elif defined(NIMCP_PLATFORM_WINDOWS)
    /* Windows: Wake one waiting thread */
    WakeConditionVariable(cond);
    return 0;

#else
    #error "Unsupported platform"
#endif
}

int nimcp_platform_cond_broadcast(nimcp_platform_cond_t* cond)
{
    if (!cond) {
        LOG_ERROR("nimcp_platform_cond_broadcast: cond pointer is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Condition broadcast failed: NULL pointer");
        return EINVAL;
    }

#if defined(NIMCP_PLATFORM_POSIX)
    return pthread_cond_broadcast(cond);

#elif defined(NIMCP_PLATFORM_WINDOWS)
    /* Windows: Wake all waiting threads */
    WakeAllConditionVariable(cond);
    return 0;

#else
    #error "Unsupported platform"
#endif
}
