/**
 * @file test_time.cpp
 * @brief Comprehensive unit tests for nimcp_time utilities
 *
 * WHAT: Tests for time functions including wall clock, monotonic, elapsed, and sleep
 * WHY: Ensure timing functions work correctly across different precision levels
 * HOW: GoogleTest framework with fixture classes for organized testing
 */

#include <gtest/gtest.h>
#include <chrono>
#include <thread>

#include "utils/time/nimcp_time.h"

//=============================================================================
// Wall Clock Time Tests
//=============================================================================

class WallClockTimeTest : public ::testing::Test {
   protected:
    void SetUp() override {}
    void TearDown() override {}
};

/**
 * WHAT: Test wall clock time returns reasonable values
 * WHY: Verify basic functionality of time retrieval
 */
TEST_F(WallClockTimeTest, GetMicrosecondsReturnsNonZero)
{
    uint64_t time_us = nimcp_time_get_us();
    EXPECT_GT(time_us, 0);

    // Should be reasonably close to current Unix epoch time
    // (greater than Jan 1, 2020 = 1577836800 seconds)
    uint64_t min_expected_us = 1577836800ULL * 1000000ULL;
    EXPECT_GT(time_us, min_expected_us);
}

/**
 * WHAT: Test wall clock time increases
 * WHY: Time should always move forward
 */
TEST_F(WallClockTimeTest, GetMicrosecondsIncreases)
{
    uint64_t time1 = nimcp_time_get_us();
    std::this_thread::sleep_for(std::chrono::microseconds(1000));
    uint64_t time2 = nimcp_time_get_us();

    EXPECT_GT(time2, time1);
    EXPECT_GE(time2 - time1, 1000);  // At least 1ms elapsed
}

/**
 * WHAT: Test millisecond precision wall clock
 * WHY: Verify conversion from microseconds works correctly
 */
TEST_F(WallClockTimeTest, GetMillisecondsMatchesMicroseconds)
{
    uint64_t time_us = nimcp_time_get_us();
    uint64_t time_ms = nimcp_time_get_ms();

    // They should be within 1ms of each other (conversion rounding)
    uint64_t converted_ms = time_us / 1000;
    EXPECT_LE(converted_ms > time_ms ? converted_ms - time_ms : time_ms - converted_ms, 1);
}

/**
 * WHAT: Test second precision wall clock
 * WHY: Verify conversion to seconds works correctly
 */
TEST_F(WallClockTimeTest, GetSecondsMatchesMicroseconds)
{
    uint64_t time_us = nimcp_time_get_us();
    uint64_t time_sec = nimcp_time_get_sec();

    // They should be within 1 second of each other
    uint64_t converted_sec = time_us / 1000000;
    EXPECT_LE(converted_sec > time_sec ? converted_sec - time_sec : time_sec - converted_sec, 1);
}

//=============================================================================
// Monotonic Time Tests
//=============================================================================

class MonotonicTimeTest : public ::testing::Test {
   protected:
    void SetUp() override {}
    void TearDown() override {}
};

/**
 * WHAT: Test monotonic time returns non-zero values
 * WHY: Verify basic functionality
 */
TEST_F(MonotonicTimeTest, GetNanosecondsReturnsNonZero)
{
    uint64_t time_ns = nimcp_time_monotonic_ns();
    EXPECT_GT(time_ns, 0);
}

/**
 * WHAT: Test monotonic time increases
 * WHY: Monotonic time should never go backwards
 */
TEST_F(MonotonicTimeTest, MonotonicTimeNeverGoesBackward)
{
    uint64_t time1 = nimcp_time_monotonic_ns();
    std::this_thread::sleep_for(std::chrono::microseconds(100));
    uint64_t time2 = nimcp_time_monotonic_ns();
    std::this_thread::sleep_for(std::chrono::microseconds(100));
    uint64_t time3 = nimcp_time_monotonic_ns();

    EXPECT_GE(time2, time1);
    EXPECT_GE(time3, time2);
    EXPECT_GT(time3, time1);
}

/**
 * WHAT: Test microsecond monotonic time
 * WHY: Verify precision conversion
 */
TEST_F(MonotonicTimeTest, MonotonicMicrosecondsMatchesNanoseconds)
{
    uint64_t time_ns = nimcp_time_monotonic_ns();
    uint64_t time_us = nimcp_time_monotonic_us();

    // Should be within reasonable range (1ms tolerance for timing variations)
    uint64_t converted_us = time_ns / 1000;
    uint64_t diff = converted_us > time_us ? converted_us - time_us : time_us - converted_us;
    EXPECT_LT(diff, 1000);  // Within 1ms
}

/**
 * WHAT: Test millisecond monotonic time
 * WHY: Verify precision conversion
 */
TEST_F(MonotonicTimeTest, MonotonicMillisecondsMatchesNanoseconds)
{
    uint64_t time_ns = nimcp_time_monotonic_ns();
    uint64_t time_ms = nimcp_time_monotonic_ms();

    // Should be within reasonable range (10ms tolerance)
    uint64_t converted_ms = time_ns / 1000000;
    uint64_t diff = converted_ms > time_ms ? converted_ms - time_ms : time_ms - converted_ms;
    EXPECT_LT(diff, 10);  // Within 10ms
}

//=============================================================================
// Elapsed Time Tests
//=============================================================================

class ElapsedTimeTest : public ::testing::Test {
   protected:
    void SetUp() override {}
    void TearDown() override {}
};

/**
 * WHAT: Test elapsed microseconds calculation
 * WHY: Verify duration measurement works correctly
 */
TEST_F(ElapsedTimeTest, ElapsedMicrosecondsAccurate)
{
    uint64_t start = nimcp_time_monotonic_us();

    // Sleep for approximately 5ms
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    uint64_t elapsed = nimcp_time_elapsed_us(start);

    // Should be at least 5ms (5000us) and less than 50ms (allowing for system variance)
    EXPECT_GE(elapsed, 5000);
    EXPECT_LT(elapsed, 50000);
}

/**
 * WHAT: Test elapsed milliseconds calculation
 * WHY: Verify duration measurement in milliseconds
 */
TEST_F(ElapsedTimeTest, ElapsedMillisecondsAccurate)
{
    uint64_t start = nimcp_time_monotonic_ms();

    // Sleep for approximately 10ms
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    uint64_t elapsed = nimcp_time_elapsed_ms(start);

    // Should be at least 10ms and less than 100ms
    EXPECT_GE(elapsed, 10);
    EXPECT_LT(elapsed, 100);
}

/**
 * WHAT: Test elapsed nanoseconds calculation
 * WHY: Verify high-precision duration measurement
 */
TEST_F(ElapsedTimeTest, ElapsedNanosecondsAccurate)
{
    uint64_t start = nimcp_time_monotonic_ns();

    // Sleep for approximately 2ms
    std::this_thread::sleep_for(std::chrono::milliseconds(2));

    uint64_t elapsed = nimcp_time_elapsed_ns(start);

    // Should be at least 2ms (2,000,000ns) and less than 20ms
    EXPECT_GE(elapsed, 2000000);
    EXPECT_LT(elapsed, 20000000);
}

/**
 * WHAT: Test elapsed time with zero start (edge case)
 * WHY: Verify handling of wraparound scenario
 */
TEST_F(ElapsedTimeTest, ElapsedFromZeroStart)
{
    uint64_t elapsed_us = nimcp_time_elapsed_us(0);
    uint64_t elapsed_ms = nimcp_time_elapsed_ms(0);
    uint64_t elapsed_ns = nimcp_time_elapsed_ns(0);

    // All should return reasonable non-zero values
    EXPECT_GT(elapsed_us, 0);
    EXPECT_GT(elapsed_ms, 0);
    EXPECT_GT(elapsed_ns, 0);
}

/**
 * WHAT: Test multiple consecutive elapsed time measurements
 * WHY: Ensure repeated measurements are consistent
 */
TEST_F(ElapsedTimeTest, ConsecutiveElapsedMeasurements)
{
    uint64_t start = nimcp_time_monotonic_us();

    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    uint64_t elapsed1 = nimcp_time_elapsed_us(start);

    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    uint64_t elapsed2 = nimcp_time_elapsed_us(start);

    // Second measurement should be larger than first
    EXPECT_GT(elapsed2, elapsed1);

    // Difference should be approximately 1ms
    uint64_t diff = elapsed2 - elapsed1;
    EXPECT_GE(diff, 1000);   // At least 1ms
    EXPECT_LT(diff, 10000);  // Less than 10ms
}

//=============================================================================
// Time Conversion Tests
//=============================================================================

class TimeConversionTest : public ::testing::Test {
   protected:
    void SetUp() override {}
    void TearDown() override {}
};

/**
 * WHAT: Test microseconds to milliseconds conversion
 * WHY: Verify conversion accuracy
 */
TEST_F(TimeConversionTest, MicrosecondsToMilliseconds)
{
    EXPECT_EQ(nimcp_time_us_to_ms(1000), 1);
    EXPECT_EQ(nimcp_time_us_to_ms(5000), 5);
    EXPECT_EQ(nimcp_time_us_to_ms(1000000), 1000);
    EXPECT_EQ(nimcp_time_us_to_ms(0), 0);
}

/**
 * WHAT: Test microseconds to seconds conversion
 * WHY: Verify conversion accuracy
 */
TEST_F(TimeConversionTest, MicrosecondsToSeconds)
{
    EXPECT_EQ(nimcp_time_us_to_sec(1000000), 1);
    EXPECT_EQ(nimcp_time_us_to_sec(5000000), 5);
    EXPECT_EQ(nimcp_time_us_to_sec(60000000), 60);
    EXPECT_EQ(nimcp_time_us_to_sec(0), 0);
}

/**
 * WHAT: Test milliseconds to microseconds conversion
 * WHY: Verify conversion accuracy
 */
TEST_F(TimeConversionTest, MillisecondsToMicroseconds)
{
    EXPECT_EQ(nimcp_time_ms_to_us(1), 1000);
    EXPECT_EQ(nimcp_time_ms_to_us(5), 5000);
    EXPECT_EQ(nimcp_time_ms_to_us(1000), 1000000);
    EXPECT_EQ(nimcp_time_ms_to_us(0), 0);
}

/**
 * WHAT: Test seconds to microseconds conversion
 * WHY: Verify conversion accuracy
 */
TEST_F(TimeConversionTest, SecondsToMicroseconds)
{
    EXPECT_EQ(nimcp_time_sec_to_us(1), 1000000);
    EXPECT_EQ(nimcp_time_sec_to_us(5), 5000000);
    EXPECT_EQ(nimcp_time_sec_to_us(60), 60000000);
    EXPECT_EQ(nimcp_time_sec_to_us(0), 0);
}

/**
 * WHAT: Test nanoseconds to microseconds conversion
 * WHY: Verify conversion accuracy
 */
TEST_F(TimeConversionTest, NanosecondsToMicroseconds)
{
    EXPECT_EQ(nimcp_time_ns_to_us(1000), 1);
    EXPECT_EQ(nimcp_time_ns_to_us(5000), 5);
    EXPECT_EQ(nimcp_time_ns_to_us(1000000), 1000);
    EXPECT_EQ(nimcp_time_ns_to_us(0), 0);
}

/**
 * WHAT: Test microseconds to nanoseconds conversion
 * WHY: Verify conversion accuracy
 */
TEST_F(TimeConversionTest, MicrosecondsToNanoseconds)
{
    EXPECT_EQ(nimcp_time_us_to_ns(1), 1000);
    EXPECT_EQ(nimcp_time_us_to_ns(5), 5000);
    EXPECT_EQ(nimcp_time_us_to_ns(1000), 1000000);
    EXPECT_EQ(nimcp_time_us_to_ns(0), 0);
}

/**
 * WHAT: Test conversion round-trip accuracy
 * WHY: Ensure conversions are reversible
 */
TEST_F(TimeConversionTest, ConversionRoundTrip)
{
    uint64_t original_us = 123456;

    // us -> ms -> us
    uint64_t ms = nimcp_time_us_to_ms(original_us);
    uint64_t back_us = nimcp_time_ms_to_us(ms);
    // Should be within rounding tolerance
    EXPECT_GE(back_us, (original_us / 1000) * 1000);
    EXPECT_LT(back_us, original_us + 1000);

    // us -> ns -> us
    uint64_t ns = nimcp_time_us_to_ns(original_us);
    uint64_t back_us2 = nimcp_time_ns_to_us(ns);
    EXPECT_EQ(back_us2, original_us);
}

//=============================================================================
// Sleep Function Tests
//=============================================================================

class SleepTest : public ::testing::Test {
   protected:
    void SetUp() override {}
    void TearDown() override {}
};

/**
 * WHAT: Test microsecond sleep function
 * WHY: Verify sleep duration is accurate
 */
TEST_F(SleepTest, SleepMicrosecondsAccurate)
{
    uint64_t start = nimcp_time_monotonic_us();

    // Sleep for 5ms = 5000us
    nimcp_time_sleep_us(5000);

    uint64_t elapsed = nimcp_time_elapsed_us(start);

    // Should be at least 5ms and less than 50ms (allowing for system scheduling)
    EXPECT_GE(elapsed, 5000);
    EXPECT_LT(elapsed, 50000);
}

/**
 * WHAT: Test millisecond sleep function
 * WHY: Verify sleep duration is accurate
 */
TEST_F(SleepTest, SleepMillisecondsAccurate)
{
    uint64_t start = nimcp_time_monotonic_ms();

    // Sleep for 10ms
    nimcp_time_sleep_ms(10);

    uint64_t elapsed = nimcp_time_elapsed_ms(start);

    // Should be at least 10ms and less than 100ms
    EXPECT_GE(elapsed, 10);
    EXPECT_LT(elapsed, 100);
}

/**
 * WHAT: Test zero sleep duration
 * WHY: Verify edge case handling
 */
TEST_F(SleepTest, SleepZeroDuration)
{
    uint64_t start = nimcp_time_monotonic_us();

    nimcp_time_sleep_us(0);

    uint64_t elapsed = nimcp_time_elapsed_us(start);

    // Should be very quick (less than 1ms)
    EXPECT_LT(elapsed, 1000);
}

/**
 * WHAT: Test very short sleep duration
 * WHY: Verify handling of sub-millisecond sleeps
 */
TEST_F(SleepTest, SleepShortDuration)
{
    uint64_t start = nimcp_time_monotonic_us();

    // Sleep for 100 microseconds
    nimcp_time_sleep_us(100);

    uint64_t elapsed = nimcp_time_elapsed_us(start);

    // Should complete within reasonable time (less than 10ms)
    EXPECT_LT(elapsed, 10000);
}

/**
 * WHAT: Test multiple consecutive sleeps
 * WHY: Ensure sleep function works repeatedly
 */
TEST_F(SleepTest, MultipleSleeps)
{
    uint64_t start = nimcp_time_monotonic_ms();

    // Three 2ms sleeps = ~6ms total
    nimcp_time_sleep_ms(2);
    nimcp_time_sleep_ms(2);
    nimcp_time_sleep_ms(2);

    uint64_t elapsed = nimcp_time_elapsed_ms(start);

    // Should be at least 6ms and less than 60ms
    EXPECT_GE(elapsed, 6);
    EXPECT_LT(elapsed, 60);
}

//=============================================================================
// Integration Tests
//=============================================================================

class TimeIntegrationTest : public ::testing::Test {
   protected:
    void SetUp() override {}
    void TearDown() override {}
};

/**
 * WHAT: Test timing a real operation
 * WHY: Verify time functions work in real-world scenario
 */
TEST_F(TimeIntegrationTest, TimingRealOperation)
{
    uint64_t start = nimcp_time_monotonic_us();

    // Simulate some work
    volatile uint64_t sum = 0;
    for (int i = 0; i < 1000000; i++) {
        sum += i;
    }

    uint64_t elapsed = nimcp_time_elapsed_us(start);

    // Should take some measurable time but not too long
    EXPECT_GT(elapsed, 0);
    EXPECT_LT(elapsed, 1000000);  // Less than 1 second
}

/**
 * WHAT: Test all time functions are consistent
 * WHY: Ensure different precision levels are synchronized
 */
TEST_F(TimeIntegrationTest, AllTimeFunctionsConsistent)
{
    uint64_t wall_us = nimcp_time_get_us();
    uint64_t wall_ms = nimcp_time_get_ms();
    uint64_t wall_sec = nimcp_time_get_sec();

    uint64_t mono_ns = nimcp_time_monotonic_ns();
    uint64_t mono_us = nimcp_time_monotonic_us();
    uint64_t mono_ms = nimcp_time_monotonic_ms();

    // Wall clock times should be consistent
    EXPECT_LE(wall_ms, wall_us / 1000 + 1);
    EXPECT_LE(wall_sec, wall_us / 1000000 + 1);

    // Monotonic times should be consistent
    EXPECT_LE(mono_us, mono_ns / 1000 + 1000);   // Allow 1ms tolerance
    EXPECT_LE(mono_ms, mono_ns / 1000000 + 10);  // Allow 10ms tolerance
}

/**
 * WHAT: Test performance profiling use case
 * WHY: Verify typical profiling scenario works
 */
TEST_F(TimeIntegrationTest, PerformanceProfiling)
{
    // Profile multiple operations
    std::vector<uint64_t> durations;

    for (int i = 0; i < 5; i++) {
        uint64_t start = nimcp_time_monotonic_us();
        nimcp_time_sleep_ms(1);
        uint64_t elapsed = nimcp_time_elapsed_us(start);
        durations.push_back(elapsed);
    }

    // All measurements should be reasonable
    for (uint64_t duration : durations) {
        EXPECT_GE(duration, 1000);   // At least 1ms
        EXPECT_LT(duration, 10000);  // Less than 10ms
    }
}
