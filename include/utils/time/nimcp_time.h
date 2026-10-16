/**
 * @file nimcp_time.h
 * @brief Time and timing utilities for NIMCP
 *
 * WHAT: Portable timing functions for timestamps and performance measurement
 * WHY: Centralize time-related operations across the codebase
 * HOW: Wrappers around clock_gettime and gettimeofday with consistent units
 *
 * USAGE:
 *   uint64_t start = nimcp_time_monotonic_us();
 *   // ... do work ...
 *   uint64_t elapsed = nimcp_time_elapsed_us(start);
 *
 *   uint64_t timestamp = nimcp_time_get_ms();
 *
 * PERFORMANCE:
 *   - Microsecond precision on most systems
 *   - Minimal overhead (direct system call wrappers)
 *   - Monotonic clocks never go backwards
 */

#ifndef NIMCP_TIME_H
#define NIMCP_TIME_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Wall Clock Time (Can jump forward/backward with system clock changes)
//=============================================================================

/**
 * WHAT: Get current wall clock time in microseconds since Unix epoch
 * WHY: Timestamps for logging, events, absolute time references
 * HOW: gettimeofday() or clock_gettime(CLOCK_REALTIME)
 *
 * @return Microseconds since Unix epoch (Jan 1, 1970)
 *
 * NOTE: This can jump forward or backward if system clock is adjusted
 * Use nimcp_time_monotonic_us() for duration measurements
 */
uint64_t nimcp_time_get_us(void);

/**
 * @brief Alias for nimcp_time_get_us() for code compatibility
 */
#define nimcp_time_now_us() nimcp_time_get_us()

/**
 * @brief Alias for nimcp_time_monotonic_ns() for code compatibility
 */
#define nimcp_time_now_ns() nimcp_time_monotonic_ns()

/**
 * WHAT: Get current wall clock time in milliseconds since Unix epoch
 * WHY: Timestamps with millisecond precision (less overhead than microseconds)
 * HOW: nimcp_time_get_us() / 1000
 *
 * @return Milliseconds since Unix epoch (Jan 1, 1970)
 *
 * NOTE: This can jump forward or backward if system clock is adjusted
 */
uint64_t nimcp_time_get_ms(void);

/**
 * WHAT: Get current wall clock time in seconds since Unix epoch
 * WHY: Timestamps with second precision (compatibility with time_t)
 * HOW: nimcp_time_get_us() / 1000000
 *
 * @return Seconds since Unix epoch (Jan 1, 1970)
 */
uint64_t nimcp_time_get_sec(void);

//=============================================================================
// Monotonic Time (Never goes backward, ideal for duration measurements)
//=============================================================================

/**
 * WHAT: Get monotonic time in microseconds
 * WHY: Measure elapsed time without system clock changes affecting results
 * HOW: clock_gettime(CLOCK_MONOTONIC)
 *
 * @return Monotonic microseconds (arbitrary epoch, only for duration measurement)
 *
 * NOTE: Value is only meaningful for computing elapsed time
 * Never goes backwards (unlike wall clock time)
 * Ideal for performance measurements and timeouts
 */
uint64_t nimcp_time_monotonic_us(void);

/**
 * WHAT: Get monotonic time in milliseconds
 * WHY: Measure elapsed time with millisecond precision
 * HOW: nimcp_time_monotonic_us() / 1000
 *
 * @return Monotonic milliseconds
 */
uint64_t nimcp_time_monotonic_ms(void);

/**
 * WHAT: Get monotonic time in nanoseconds
 * WHY: High-precision timing for critical performance measurements
 * HOW: clock_gettime(CLOCK_MONOTONIC) with nanosecond resolution
 *
 * @return Monotonic nanoseconds
 *
 * NOTE: Actual resolution depends on system capabilities
 */
uint64_t nimcp_time_monotonic_ns(void);

//=============================================================================
// Duration and Elapsed Time Calculations
//=============================================================================

/**
 * WHAT: Calculate elapsed microseconds since a start time
 * WHY: Convenient wrapper for duration calculation
 * HOW: current_time - start_time
 *
 * @param start_us Start time in microseconds (from nimcp_time_monotonic_us)
 * @return Elapsed microseconds
 *
 * USAGE:
 *   uint64_t start = nimcp_time_monotonic_us();
 *   do_work();
 *   uint64_t elapsed = nimcp_time_elapsed_us(start);
 */
uint64_t nimcp_time_elapsed_us(uint64_t start_us);

/**
 * WHAT: Calculate elapsed milliseconds since a start time
 * WHY: Convenient wrapper for duration calculation
 * HOW: (current_time - start_time) / 1000
 *
 * @param start_ms Start time in milliseconds (from nimcp_time_monotonic_ms)
 * @return Elapsed milliseconds
 */
uint64_t nimcp_time_elapsed_ms(uint64_t start_ms);

/**
 * WHAT: Calculate elapsed nanoseconds since a start time
 * WHY: High-precision duration measurement
 * HOW: current_time - start_time
 *
 * @param start_ns Start time in nanoseconds (from nimcp_time_monotonic_ns)
 * @return Elapsed nanoseconds
 */
uint64_t nimcp_time_elapsed_ns(uint64_t start_ns);

//=============================================================================
// Time Conversion Utilities
//=============================================================================

/**
 * WHAT: Convert microseconds to milliseconds
 * WHY: Common conversion for display and logging
 * HOW: us / 1000
 *
 * @param us Microseconds
 * @return Milliseconds
 */
static inline uint64_t nimcp_time_us_to_ms(uint64_t us)
{
    return us / 1000;
}

/**
 * WHAT: Convert microseconds to seconds
 * WHY: Common conversion for display
 * HOW: us / 1000000
 *
 * @param us Microseconds
 * @return Seconds
 */
static inline uint64_t nimcp_time_us_to_sec(uint64_t us)
{
    return us / 1000000;
}

/**
 * WHAT: Convert milliseconds to microseconds
 * WHY: Convert from coarse to fine precision
 * HOW: ms * 1000
 *
 * @param ms Milliseconds
 * @return Microseconds
 */
static inline uint64_t nimcp_time_ms_to_us(uint64_t ms)
{
    return ms * 1000;
}

/**
 * WHAT: Convert seconds to microseconds
 * WHY: Convert from coarse to fine precision
 * HOW: sec * 1000000
 *
 * @param sec Seconds
 * @return Microseconds
 */
static inline uint64_t nimcp_time_sec_to_us(uint64_t sec)
{
    return sec * 1000000;
}

/**
 * WHAT: Convert nanoseconds to microseconds
 * WHY: Common conversion from high-precision to standard precision
 * HOW: ns / 1000
 *
 * @param ns Nanoseconds
 * @return Microseconds
 */
static inline uint64_t nimcp_time_ns_to_us(uint64_t ns)
{
    return ns / 1000;
}

/**
 * WHAT: Convert microseconds to nanoseconds
 * WHY: Convert to high-precision representation
 * HOW: us * 1000
 *
 * @param us Microseconds
 * @return Nanoseconds
 */
static inline uint64_t nimcp_time_us_to_ns(uint64_t us)
{
    return us * 1000;
}

//=============================================================================
// Sleep and Delay Functions
//=============================================================================

/**
 * WHAT: Sleep for specified microseconds
 * WHY: Platform-independent microsecond sleep
 * HOW: nanosleep or usleep
 *
 * @param us Microseconds to sleep
 *
 * NOTE: Actual sleep time may be longer due to system scheduling
 */
void nimcp_time_sleep_us(uint64_t us);

/**
 * WHAT: Sleep for specified milliseconds
 * WHY: Platform-independent millisecond sleep
 * HOW: nimcp_time_sleep_us(ms * 1000)
 *
 * @param ms Milliseconds to sleep
 */
void nimcp_time_sleep_ms(uint64_t ms);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TIME_H */
