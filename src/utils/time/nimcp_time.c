/**
 * @file nimcp_time.c
 * @brief Time and timing utilities implementation
 *
 * WHAT: Portable timing functions for timestamps and performance measurement
 * WHY: Centralize time-related operations across the codebase
 * HOW: Platform-specific implementations using POSIX APIs
 */

#include "utils/time/nimcp_time.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/validation/nimcp_common.h"
#include "api/nimcp_api_exception.h"
#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(time)

//=============================================================================
// Platform Detection
//=============================================================================

#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)
    #define NIMCP_POSIX 1
#else
    #define NIMCP_POSIX 0
#endif

//=============================================================================
// Wall Clock Time
//=============================================================================

uint64_t nimcp_time_get_us(void)
{
    LOG_TRACE("Entering nimcp_time_get_us");
#if NIMCP_POSIX
    /**
     * WHAT: Use clock_gettime for better precision if available
     * WHY: More accurate than gettimeofday on modern systems
     * HOW: CLOCK_REALTIME provides wall clock time
     */
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
        return (uint64_t) ts.tv_sec * NIMCP_US_PER_SEC + (uint64_t) ts.tv_nsec / NIMCP_NS_PER_US;
    }

    /**
     * WHAT: Fallback to gettimeofday if clock_gettime fails
     * WHY: Maximum portability
     */
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t) tv.tv_sec * NIMCP_US_PER_SEC + (uint64_t) tv.tv_usec;
#else
    /**
     * WHAT: Windows fallback (if needed in future)
     * WHY: Cross-platform support
     */
    #error "Platform not supported - implement time functions for this platform"
#endif
}

uint64_t nimcp_time_get_ms(void)
{
    LOG_TRACE("Entering nimcp_time_get_ms");
    return nimcp_time_get_us() / NIMCP_US_PER_MS;
}

uint64_t nimcp_time_get_sec(void)
{
    LOG_TRACE("Entering nimcp_time_get_sec");
    return nimcp_time_get_us() / NIMCP_US_PER_SEC;
}

//=============================================================================
// Monotonic Time
//=============================================================================

uint64_t nimcp_time_monotonic_ns(void)
{
    LOG_TRACE("Entering nimcp_time_monotonic_ns");
#if NIMCP_POSIX
    /**
     * WHAT: Use CLOCK_MONOTONIC for steady, non-adjustable time
     * WHY: Immune to system clock changes
     * HOW: clock_gettime with nanosecond precision
     */
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        /**
         * WHAT: Fallback to CLOCK_REALTIME if MONOTONIC unavailable
         * WHY: Some systems may not support CLOCK_MONOTONIC
         * NOTE: This defeats the purpose but ensures we don't crash
         */
        clock_gettime(CLOCK_REALTIME, &ts);
    }

    return (uint64_t) ts.tv_sec * NIMCP_NS_PER_SEC + (uint64_t) ts.tv_nsec;
#else
    #error "Platform not supported - implement monotonic time for this platform"
#endif
}

uint64_t nimcp_time_monotonic_us(void)
{
    LOG_TRACE("Entering nimcp_time_monotonic_us");
    return nimcp_time_monotonic_ns() / NIMCP_NS_PER_US;
}

uint64_t nimcp_time_monotonic_ms(void)
{
    LOG_TRACE("Entering nimcp_time_monotonic_ms");
    return nimcp_time_monotonic_ns() / NIMCP_NS_PER_MS;
}

//=============================================================================
// Duration and Elapsed Time Calculations
//=============================================================================

uint64_t nimcp_time_elapsed_us(uint64_t start_us)
{
    LOG_TRACE("Entering nimcp_time_elapsed_us");
    uint64_t now = nimcp_time_monotonic_us();

    /**
     * WHAT: Handle potential wraparound (extremely unlikely with uint64_t)
     * WHY: Safety for long-running systems
     * HOW: Check if now < start, which would indicate wraparound
     */
    if (now < start_us) {
        /**
         * WHAT: Wraparound occurred (after ~584,942 years)
         * WHY: uint64_t microseconds wraps after 2^64 microseconds
         * HOW: Calculate elapsed considering wraparound
         */
        return (UINT64_MAX - start_us) + now + 1;
    }

    return now - start_us;
}

uint64_t nimcp_time_elapsed_ms(uint64_t start_ms)
{
    LOG_TRACE("Entering nimcp_time_elapsed_ms");
    uint64_t now = nimcp_time_monotonic_ms();

    if (now < start_ms) {
        return (UINT64_MAX - start_ms) + now + 1;
    }

    return now - start_ms;
}

uint64_t nimcp_time_elapsed_ns(uint64_t start_ns)
{
    LOG_TRACE("Entering nimcp_time_elapsed_ns");
    uint64_t now = nimcp_time_monotonic_ns();

    if (now < start_ns) {
        return (UINT64_MAX - start_ns) + now + 1;
    }

    return now - start_ns;
}

//=============================================================================
// Sleep and Delay Functions
//=============================================================================

void nimcp_time_sleep_us(uint64_t us)
{
    LOG_TRACE("Entering nimcp_time_sleep_us");
#if NIMCP_POSIX
    /**
     * WHAT: Use nanosleep for portable microsecond sleep
     * WHY: More portable than usleep (which is deprecated)
     * HOW: Convert microseconds to timespec
     */
    struct timespec ts;
    ts.tv_sec = us / NIMCP_US_PER_SEC;
    ts.tv_nsec = (us % NIMCP_US_PER_SEC) * NIMCP_NS_PER_US;

    /**
     * WHAT: Handle EINTR (interrupted by signal)
     * WHY: nanosleep can be interrupted by signals
     * HOW: Restart with remaining time
     */
    struct timespec remaining;
    while (nanosleep(&ts, &remaining) != 0) {
        if (errno == EINTR) {
            /**
             * WHAT: Interrupted by signal, continue sleeping
             * WHY: User requested full sleep duration
             * HOW: Use remaining time for next sleep call
             */
            ts = remaining;
        } else {
            /**
             * WHAT: Actual error occurred
             * WHY: Something went wrong with nanosleep
             * HOW: Break out and return
             */
            break;
        }
    }
#else
    #error "Platform not supported - implement sleep for this platform"
#endif
}

void nimcp_time_sleep_ms(uint64_t ms)
{
    LOG_TRACE("Entering nimcp_time_sleep_ms");
    nimcp_time_sleep_us(ms * NIMCP_US_PER_MS);
}
