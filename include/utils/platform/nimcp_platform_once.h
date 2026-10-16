/**
 * @file nimcp_platform_once.h
 * @brief Cross-platform one-time initialization abstraction (SRP: Once operations only)
 *
 * WHAT: Platform-agnostic once-only initialization API
 * WHY:  pthread_once doesn't exist on Windows, need unified once API
 * HOW:  Wrap pthread_once (POSIX) and InitOnceExecuteOnce (Windows)
 *
 * SRP: This module has ONE responsibility - one-time initialization operations
 * DESIGN PATTERN: Adapter pattern
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_PLATFORM_ONCE_H
#define NIMCP_PLATFORM_ONCE_H

#include "utils/platform/nimcp_platform.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * ONCE-INITIALIZATION TYPE
 * ======================================================================== */

#if defined(NIMCP_PLATFORM_POSIX)
    #include <pthread.h>
    typedef pthread_once_t nimcp_platform_once_t;
    #define NIMCP_PLATFORM_ONCE_INIT PTHREAD_ONCE_INIT

#elif defined(NIMCP_PLATFORM_WINDOWS)
    #include <windows.h>
    typedef INIT_ONCE nimcp_platform_once_t;
    #define NIMCP_PLATFORM_ONCE_INIT INIT_ONCE_STATIC_INIT

#else
    #error "Unsupported platform for once initialization"
#endif

/* ========================================================================
 * ONCE-INITIALIZATION FUNCTION TYPES
 * ======================================================================== */

/**
 * WHAT: Callback function signature for one-time initialization
 * WHY:  Encapsulates the initialization routine
 * HOW:  Function pointer that takes void and returns void
 *
 * COMPLEXITY: O(1) or depends on actual initialization routine
 * THREAD-SAFE: Must be thread-safe to be called concurrently
 */
typedef void (*nimcp_platform_once_routine_t)(void);

/* ========================================================================
 * ONCE-INITIALIZATION FUNCTIONS
 * ======================================================================== */

/**
 * WHAT: Execute initialization routine exactly once across all threads
 * WHY:  Guarantee thread-safe, one-time initialization of shared state
 * HOW:  Wrap pthread_once (POSIX) or InitOnceExecuteOnce (Windows)
 *
 * BEHAVIOR:
 * - First call: Executes init_routine and remembers completion
 * - Subsequent calls: Returns immediately without executing routine
 * - Thread-safe: If multiple threads call simultaneously, only one executes routine
 * - Blocking: Threads calling after routine starts will block until completion
 *
 * EXAMPLE:
 *   static nimcp_platform_once_t once = NIMCP_PLATFORM_ONCE_INIT;
 *
 *   void init_function(void) {
 *       // Initialize shared state
 *       global_resource = allocate_resource();
 *   }
 *
 *   void worker_thread(void) {
 *       nimcp_platform_once(&once, init_function);
 *       // At this point, global_resource is guaranteed to be initialized
 *       use_resource(global_resource);
 *   }
 *
 * @param once_control Pointer to once control variable (must be static/persistent)
 * @param init_routine Function to call exactly once
 * @return 0 on success, error code on failure
 *
 * COMPLEXITY: O(1) - constant time after first execution
 * THREAD-SAFE: Yes - designed for multi-threaded scenarios
 *
 * NOTES:
 * - once_control must persist for the lifetime of the program
 * - init_routine will be called exactly once, regardless of thread count
 * - Should be used for initializing global/static resources
 * - If init_routine fails or never completes, behavior is undefined on some platforms
 */
int nimcp_platform_once(nimcp_platform_once_t* once_control,
                        nimcp_platform_once_routine_t init_routine);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PLATFORM_ONCE_H */
