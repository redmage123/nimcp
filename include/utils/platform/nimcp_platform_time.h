/**
 * @file nimcp_platform_time.h
 * @brief Cross-platform time measurement abstraction (SRP: Time operations only)
 *
 * WHAT: Platform-agnostic monotonic time measurement API
 * WHY:  Different OSes provide different high-resolution timing mechanisms
 *       (clock_gettime on Linux, mach_absolute_time on macOS, QueryPerformance on Windows)
 * HOW:  Wrap platform-specific time functions with unified interface
 *
 * SRP: This module has ONE responsibility - monotonic time measurement
 *      This is distinct from nimcp_time.h which provides higher-level time utilities
 *      This module focuses on raw platform time operations
 *
 * DESIGN PATTERN: Adapter pattern - adapts different platform time APIs to unified interface
 *
 * SUPPORTED PLATFORMS:
 * - Linux (POSIX): clock_gettime(CLOCK_MONOTONIC)
 * - macOS: mach_absolute_time() for high-precision timing
 * - Windows: QueryPerformanceCounter/QueryPerformanceFrequency
 *
 * PRECISION:
 * - POSIX: Nanosecond precision (on Linux)
 * - macOS: Nanosecond precision (very high-resolution)
 * - Windows: Performance counter tick precision
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_PLATFORM_TIME_H
#define NIMCP_PLATFORM_TIME_H

#include "utils/platform/nimcp_platform.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * CORE TIME MEASUREMENT
 * ======================================================================== */

/**
 * WHAT: Get monotonic time in milliseconds
 * WHY:  Monotonic time never goes backward, ideal for duration measurement
 *       Used for performance timing, timeout calculations, elapsed time
 * HOW:  Linux: clock_gettime(CLOCK_MONOTONIC) / 1e6
 *       macOS: mach_absolute_time() / mach_timebase (convert to ms)
 *       Windows: QueryPerformanceCounter() scaled to milliseconds
 *
 * @return Monotonic time in milliseconds (arbitrary epoch, only for duration)
 *
 * COMPLEXITY: O(1)
 * PRECISION: Millisecond precision (actual resolution platform-dependent)
 * THREAD-SAFE: Yes
 * PLATFORM-SPECIFIC: Yes (different implementations per OS)
 *
 * USAGE:
 *   uint64_t start = nimcp_platform_time_monotonic_ms();
 *   // ... do work ...
 *   uint64_t elapsed = nimcp_platform_time_monotonic_ms() - start;
 *
 * NOTE: Value wraps after ~584 million years, not a concern in practice
 *       Use nimcp_time_monotonic_ms() in nimcp_time.h for higher-level API
 */
uint64_t nimcp_platform_time_monotonic_ms(void);

/**
 * WHAT: Get monotonic time in microseconds
 * WHY:  Higher precision timing for latency measurement and sub-millisecond operations
 * HOW:  Same as _ms but returns microseconds instead of milliseconds
 *
 * @return Monotonic time in microseconds (arbitrary epoch, only for duration)
 *
 * COMPLEXITY: O(1)
 * PRECISION: Microsecond precision (actual resolution platform-dependent)
 * THREAD-SAFE: Yes
 */
uint64_t nimcp_platform_time_monotonic_us(void);

/**
 * WHAT: Sleep for specified milliseconds
 * WHY:  Platform-independent sleep with millisecond granularity
 *       Used for delays, throttling, timeout implementation
 * HOW:  Linux/macOS: nanosleep()
 *       Windows: Sleep()
 *
 * @param ms Milliseconds to sleep
 *
 * COMPLEXITY: O(1) (system scheduler dependent)
 * PRECISION: Millisecond precision (actual resolution platform-dependent)
 * THREAD-SAFE: Yes (per-thread operation)
 *
 * NOTES:
 * - Actual sleep time may be longer due to system scheduling
 * - Not interruptible by signals on POSIX systems (unlike nanosleep)
 * - Block size matters for performance in tight loops
 *
 * USAGE:
 *   nimcp_platform_sleep_ms(100);  // Sleep for 100ms
 */
void nimcp_platform_sleep_ms(uint32_t ms);

/**
 * WHAT: Format monotonic time to human-readable string
 * WHY:  Convert raw time values to readable format for logging/display
 *       Useful for timing information in logs and diagnostics
 * HOW:  Convert milliseconds to formatted string with days:hours:minutes:seconds.ms
 *
 * @param time_ms Monotonic time in milliseconds
 * @param buffer Output buffer for formatted string
 * @param size Size of output buffer (must be at least 32 bytes)
 *
 * @return 0 on success, -1 on failure (buffer too small or NULL pointer)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (per-thread operation, no shared state)
 *
 * BUFFER REQUIREMENTS:
 * - Minimum 32 bytes for safe formatting
 * - Format: "DDd HHh MMm SSs.mmms" (e.g., "0d 0h 1m 30s.500ms")
 *
 * USAGE:
 *   uint64_t time = nimcp_platform_time_monotonic_ms();
 *   char buffer[64];
 *   if (nimcp_platform_time_to_string(time, buffer, sizeof(buffer)) == 0) {
 *       printf("Elapsed: %s\n", buffer);
 *   }
 */
int nimcp_platform_time_to_string(uint64_t time_ms, char* buffer, size_t size);

/* ========================================================================
 * INTERNAL PLATFORM-SPECIFIC TYPES (Private)
 * ======================================================================== */

/**
 * WHAT: Platform-specific time state for calibration and conversion
 * WHY:  Some platforms (Windows, macOS) need to store frequency/timebase
 *       data for converting performance counters to milliseconds
 * HOW:  Opaque structure initialized at module load time
 *
 * NOTE: This is internal - applications should not depend on this structure
 *       It may change between platform implementations
 */
typedef struct {
#if defined(NIMCP_PLATFORM_WINDOWS)
    uint64_t frequency;  /* Performance counter frequency in counts/second */
#elif defined(NIMCP_PLATFORM_MACOS)
    mach_timebase_info_data_t timebase;  /* Conversion factor for mach time */
#endif
} nimcp_platform_time_state_t;

/**
 * WHAT: Initialize platform-specific time subsystem
 * WHY:  Some platforms need to calibrate/initialize time conversion
 *       (e.g., Windows QueryPerformanceFrequency, macOS mach_timebase_info)
 * HOW:  Call once at startup to initialize internal state
 *
 * @return 0 on success, error code on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Must be called before other time functions
 *
 * NOTE: Automatically called on first use, but explicit call ensures
 *       deterministic initialization timing
 */
int nimcp_platform_time_init(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PLATFORM_TIME_H */
