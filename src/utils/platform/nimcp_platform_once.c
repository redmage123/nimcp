/**
 * @file nimcp_platform_once.c
 * @brief Cross-platform one-time initialization implementation (SRP: Once only)
 *
 * WHAT: Platform-agnostic one-time initialization implementation
 * WHY:  pthread_once doesn't exist on Windows
 * HOW:  Wrap pthread_once (POSIX) and InitOnceExecuteOnce (Windows)
 *
 * SRP: This module has ONE responsibility - one-time initialization operations
 * COMPLEXITY: O(1) - constant time after first execution
 * THREAD-SAFE: Yes - all functions thread-safe
 *
 * IMPLEMENTATION NOTES:
 * - POSIX: Direct wrapper around pthread_once
 * - Windows: Wraps InitOnceExecuteOnce with callback adapter
 *   - Windows requires a callback with specific signature: BOOL CALLBACK
 *   - We adapt this to accept the POSIX-style void function callback
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include "utils/platform/nimcp_platform_once.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <errno.h>
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(platform_once)

/* ========================================================================
 * WINDOWS-SPECIFIC CALLBACK WRAPPER
 * ======================================================================== */

#if defined(NIMCP_PLATFORM_WINDOWS)

/**
 * WHAT: Callback wrapper for Windows InitOnceExecuteOnce
 * WHY:  Windows requires specific callback signature (BOOL CALLBACK)
 *       POSIX uses void callback, so we need an adapter
 * HOW:  Wrap the user's routine and return appropriate value
 *
 * @param InitOnce Windows init-once handle (unused)
 * @param Parameter User's callback function pointer (nimcp_platform_once_routine_t)
 * @param Context Unused context
 * @return TRUE on success (required by Windows API)
 */
static BOOL CALLBACK nimcp_platform_once_callback(
    PINIT_ONCE InitOnce,
    PVOID Parameter,
    PVOID* Context)
{
    /* Parameter is the user's callback function */
    (void)InitOnce;      /* Unused on Windows */
    (void)Context;       /* Unused on Windows */

    if (Parameter) {
        nimcp_platform_once_routine_t routine =
            (nimcp_platform_once_routine_t)Parameter;
        routine();
    }

    return TRUE;  /* Return TRUE to indicate success */
}

#endif

/* ========================================================================
 * ONCE-INITIALIZATION FUNCTIONS
 * ======================================================================== */

int nimcp_platform_once(nimcp_platform_once_t* once_control,
                        nimcp_platform_once_routine_t init_routine)
{
    if (!once_control) {
        LOG_ERROR("nimcp_platform_once: once_control pointer is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Once init failed: NULL once_control pointer");
        return EINVAL;
    }
    if (!init_routine) {
        LOG_ERROR("nimcp_platform_once: init_routine pointer is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Once init failed: NULL init_routine pointer");
        return EINVAL;
    }

#if defined(NIMCP_PLATFORM_POSIX)
    /**
     * POSIX: Use pthread_once
     * - Direct wrapper: handles all synchronization
     * - Returns 0 on success, error code otherwise
     * - Guaranteed thread-safe by POSIX standard
     */
    return pthread_once(once_control, init_routine);

#elif defined(NIMCP_PLATFORM_WINDOWS)
    /**
     * Windows: Use InitOnceExecuteOnce
     * - Requires callback with different signature
     * - We use our callback wrapper to adapt POSIX-style function
     * - Pass init_routine as Parameter to callback wrapper
     * - INFINITE timeout ensures we wait for initialization
     */
    BOOL result = InitOnceExecuteOnce(
        once_control,                              /* Init-once handle */
        nimcp_platform_once_callback,              /* Callback wrapper */
        (PVOID)init_routine,                       /* Pass user routine as parameter */
        NULL                                       /* No output context needed */
    );

    return result ? 0 : EINVAL;

#else
    #error "Unsupported platform"
#endif
}
