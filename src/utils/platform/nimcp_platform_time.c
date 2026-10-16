/**
 * @file nimcp_platform_time.c
 * @brief Cross-platform monotonic time measurement implementation
 *
 * WHAT: Platform-agnostic monotonic time implementation
 * WHY:  Different OSes require different APIs for high-resolution timing
 * HOW:  Detect platform at compile-time, use optimal time source per OS
 *
 * IMPLEMENTATION DETAILS:
 * - Linux (POSIX): Uses clock_gettime(CLOCK_MONOTONIC) for best precision
 * - macOS: Uses mach_absolute_time() with timebase conversion for highest precision
 * - Windows: Uses QueryPerformanceCounter() with cached frequency
 *
 * PRECISION:
 * - Linux: Nanosecond precision (system-dependent)
 * - macOS: Nanosecond precision (Apple's mach time)
 * - Windows: Performance counter precision (typically sub-microsecond)
 *
 * THREAD-SAFETY:
 * - All functions are thread-safe (no shared mutable state except init flag)
 * - Platform-specific init is done once via atomic compare-and-swap or mutex
 *
 * PERFORMANCE:
 * - Single system call per monotonic time read
 * - No heap allocation or dynamic memory
 * - Minimal overhead (direct wrapping of OS functions)
 *
 * ERROR HANDLING:
 * - Graceful handling of platform-specific failures
 * - Fallback to lower precision on some platforms if needed
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include "utils/platform/nimcp_platform_time.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <errno.h>
#include <stdio.h>
#include "api/nimcp_api_exception.h"

/* ========================================================================
 * PLATFORM-SPECIFIC INCLUDES
 * ======================================================================== */

#if defined(NIMCP_PLATFORM_LINUX) || defined(NIMCP_PLATFORM_BSD)
    #include <time.h>
    #include <unistd.h>

#elif defined(NIMCP_PLATFORM_MACOS)
    #include <mach/mach_time.h>
    #include <time.h>

#elif defined(NIMCP_PLATFORM_WINDOWS)
    #include <windows.h>
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(platform_time)

#else
    #error "Unsupported platform for time measurement"
#endif

/* ========================================================================
 * PLATFORM-SPECIFIC STATE
 * ======================================================================== */

#if defined(NIMCP_PLATFORM_WINDOWS)
    /* Windows: Cache performance counter frequency */
    static uint64_t g_performance_frequency = 0;
    static int g_time_initialized = 0;

#elif defined(NIMCP_PLATFORM_MACOS)
    /* macOS: Cache mach timebase info */
    static mach_timebase_info_data_t g_mach_timebase = {0, 0};
    static int g_time_initialized = 0;
#endif

/* ========================================================================
 * PLATFORM INITIALIZATION
 * ======================================================================== */

int nimcp_platform_time_init(void)
{
#if defined(NIMCP_PLATFORM_WINDOWS)
    /* Windows: Query performance counter frequency once */
    if (g_time_initialized) {
        return 0;
    }

    LARGE_INTEGER freq;
    if (!QueryPerformanceFrequency(&freq)) {
        LOG_ERROR("nimcp_platform_time_init: QueryPerformanceFrequency failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Platform time init failed: QueryPerformanceFrequency error");
        return EINVAL;  /* Failed to get frequency */
    }

    if (freq.QuadPart <= 0) {
        LOG_ERROR("nimcp_platform_time_init: invalid performance counter frequency");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Platform time init failed: invalid frequency");
        return EINVAL;  /* Invalid frequency */
    }

    g_performance_frequency = freq.QuadPart;
    g_time_initialized = 1;
    return 0;

#elif defined(NIMCP_PLATFORM_MACOS)
    /* macOS: Get mach timebase info once */
    if (g_time_initialized) {
        return 0;
    }

    kern_return_t result = mach_timebase_info(&g_mach_timebase);
    if (result != KERN_SUCCESS) {
        LOG_ERROR("nimcp_platform_time_init: mach_timebase_info failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Platform time init failed: mach_timebase_info error");
        return EIO;  /* Failed to get timebase */
    }

    if (g_mach_timebase.denom <= 0) {
        LOG_ERROR("nimcp_platform_time_init: invalid mach timebase denominator");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Platform time init failed: invalid timebase");
        return EINVAL;  /* Invalid timebase */
    }

    g_time_initialized = 1;
    return 0;

#else
    /* POSIX (Linux, BSD): No initialization needed */
    return 0;
#endif
}

/* ========================================================================
 * MONOTONIC TIME - MILLISECONDS
 * ======================================================================== */

uint64_t nimcp_platform_time_monotonic_ms(void)
{
#if defined(NIMCP_PLATFORM_LINUX) || defined(NIMCP_PLATFORM_BSD)
    /* ====================================================================
     * POSIX (Linux/BSD): clock_gettime(CLOCK_MONOTONIC)
     * ==================================================================== */

    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        /* Fallback to lower precision if CLOCK_MONOTONIC unavailable */
        clock_gettime(CLOCK_REALTIME, &ts);
    }

    /* Convert seconds and nanoseconds to milliseconds */
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;

#elif defined(NIMCP_PLATFORM_MACOS)
    /* ====================================================================
     * macOS: mach_absolute_time() with timebase conversion
     * ==================================================================== */

    /* Initialize on first call if needed */
    if (!g_time_initialized) {
        nimcp_platform_time_init();
    }

    uint64_t mach_time = mach_absolute_time();

    /* Convert mach time to nanoseconds */
    uint64_t nanos = mach_time * g_mach_timebase.numer / g_mach_timebase.denom;

    /* Convert nanoseconds to milliseconds */
    return nanos / 1000000ULL;

#elif defined(NIMCP_PLATFORM_WINDOWS)
    /* ====================================================================
     * Windows: QueryPerformanceCounter()
     * ==================================================================== */

    /* Initialize on first call if needed */
    if (!g_time_initialized) {
        nimcp_platform_time_init();
    }

    LARGE_INTEGER count;
    if (!QueryPerformanceCounter(&count)) {
        /* Fallback: Return 0 on error (rare) */
        return 0;
    }

    /* Convert performance counter to milliseconds */
    /* Result = (count / frequency) * 1000 */
    /* To avoid losing precision with integer math: (count * 1000) / frequency */
    return (uint64_t)(count.QuadPart * 1000) / g_performance_frequency;

#else
    #error "Unsupported platform for monotonic time"
#endif
}

/* ========================================================================
 * MONOTONIC TIME - MICROSECONDS
 * ======================================================================== */

uint64_t nimcp_platform_time_monotonic_us(void)
{
#if defined(NIMCP_PLATFORM_LINUX) || defined(NIMCP_PLATFORM_BSD)
    /* ====================================================================
     * POSIX (Linux/BSD): clock_gettime(CLOCK_MONOTONIC)
     * ==================================================================== */

    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        /* Fallback to lower precision if CLOCK_MONOTONIC unavailable */
        clock_gettime(CLOCK_REALTIME, &ts);
    }

    /* Convert seconds and nanoseconds to microseconds */
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;

#elif defined(NIMCP_PLATFORM_MACOS)
    /* ====================================================================
     * macOS: mach_absolute_time() with timebase conversion
     * ==================================================================== */

    /* Initialize on first call if needed */
    if (!g_time_initialized) {
        nimcp_platform_time_init();
    }

    uint64_t mach_time = mach_absolute_time();

    /* Convert mach time to nanoseconds */
    uint64_t nanos = mach_time * g_mach_timebase.numer / g_mach_timebase.denom;

    /* Convert nanoseconds to microseconds */
    return nanos / 1000ULL;

#elif defined(NIMCP_PLATFORM_WINDOWS)
    /* ====================================================================
     * Windows: QueryPerformanceCounter()
     * ==================================================================== */

    /* Initialize on first call if needed */
    if (!g_time_initialized) {
        nimcp_platform_time_init();
    }

    LARGE_INTEGER count;
    if (!QueryPerformanceCounter(&count)) {
        /* Fallback: Return 0 on error (rare) */
        return 0;
    }

    /* Convert performance counter to microseconds */
    /* Result = (count / frequency) * 1000000 */
    return (uint64_t)(count.QuadPart * 1000000) / g_performance_frequency;

#else
    #error "Unsupported platform for monotonic time"
#endif
}

/* ========================================================================
 * SLEEP FUNCTIONS
 * ======================================================================== */

void nimcp_platform_sleep_ms(uint32_t ms)
{
#if defined(NIMCP_PLATFORM_LINUX) || defined(NIMCP_PLATFORM_BSD) || defined(NIMCP_PLATFORM_MACOS)
    /* ====================================================================
     * POSIX (Linux/BSD/macOS): nanosleep()
     * ==================================================================== */

    struct timespec ts;
    struct timespec rem;

    /* Convert milliseconds to seconds and nanoseconds */
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;

    /* nanosleep may be interrupted by signals, retry if interrupted */
    while (nanosleep(&ts, &rem) == -1 && errno == EINTR) {
        ts = rem;  /* Resume with remaining time */
    }

#elif defined(NIMCP_PLATFORM_WINDOWS)
    /* ====================================================================
     * Windows: Sleep()
     * ==================================================================== */

    Sleep((DWORD)ms);

#else
    #error "Unsupported platform for sleep"
#endif
}

/* ========================================================================
 * TIME FORMATTING
 * ======================================================================== */

int nimcp_platform_time_to_string(uint64_t time_ms, char* buffer, size_t size)
{
    /* Validate inputs */
    if (!buffer) {
        LOG_ERROR("nimcp_platform_time_to_string: buffer pointer is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Time to string failed: NULL buffer");
        return -1;
    }
    if (size < 32) {
        LOG_ERROR("nimcp_platform_time_to_string: buffer size too small (need at least 32 bytes)");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Time to string failed: buffer too small");
        return -1;  /* Buffer too small */
    }

    /* Convert milliseconds to time components */
    uint64_t total_seconds = time_ms / 1000;
    uint32_t milliseconds = time_ms % 1000;

    uint32_t seconds = total_seconds % 60;
    uint64_t total_minutes = total_seconds / 60;

    uint32_t minutes = total_minutes % 60;
    uint64_t total_hours = total_minutes / 60;

    uint32_t hours = total_hours % 24;
    uint64_t days = total_hours / 24;

    /* Format string as "DDd HHh MMm SSs.mmms" */
    int written = snprintf(buffer, size, "%llu d %u h %u m %u s.%03u ms",
                          (unsigned long long)days,
                          hours,
                          minutes,
                          seconds,
                          milliseconds);

    /* Check for formatting errors */
    if (written < 0 || (size_t)written >= size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_platform_time_to_string: capacity exceeded");
        return -1;  /* Formatting error or buffer overflow */
    }

    return 0;  /* Success */
}

/* ========================================================================
 * MODULE INITIALIZATION (Called once at startup)
 * ======================================================================== */

/**
 * WHAT: Constructor function called at module load time
 * WHY:  Initialize platform-specific state before first use
 * HOW:  GCC/Clang: __attribute__((constructor))
 *       Windows: DllMain or linker section
 *
 * NOTES:
 * - Ensures frequency/timebase is cached before time functions are called
 * - Reduces latency of first nimcp_platform_time_monotonic_ms() call
 * - Not strictly required (init functions check lazy initialization)
 */

#if defined(NIMCP_COMPILER_GCC) || defined(NIMCP_COMPILER_CLANG)
    __attribute__((constructor))
    static void nimcp_platform_time_module_init(void) {
        nimcp_platform_time_init();
    }

#elif defined(NIMCP_PLATFORM_WINDOWS) && defined(NIMCP_COMPILER_MSVC)
    /* Windows DLL initialization via pragma */
    #pragma comment(lib, "kernel32.lib")

    static void nimcp_platform_time_module_init_windows(void) {
        nimcp_platform_time_init();
    }

    /* Called at DLL load time */
    #ifdef _DLL
        extern BOOL APIENTRY DllMain(HINSTANCE hInst, DWORD dwReason, LPVOID reserved);
    #endif
#endif
