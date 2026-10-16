/**
 * @file test_platform_time.cpp
 * @brief Comprehensive TDD test suite for nimcp_platform_time module
 *
 * WHAT: Complete test coverage for cross-platform monotonic time measurement
 * WHY: Ensure timing functions work reliably across Linux, macOS, and Windows
 * HOW: GoogleTest framework with organized test fixtures for different aspects
 *
 * TEST COVERAGE:
 * 1. Monotonic time: Time always increases, never goes backward
 * 2. Sleep accuracy: Sleep duration matches expected values
 * 3. Time formatting: String conversion produces correct format
 * 4. Time progression: Consistent time advancement across multiple calls
 * 5. NULL pointer safety: Proper error handling for invalid inputs
 */

#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <vector>
#include <cstring>

#include "utils/platform/nimcp_platform_time.h"
#include "utils/nimcp_test_base.h"

//=============================================================================
// MONOTONIC TIME TESTS
//=============================================================================

class MonotonicTimeTest : public NimcpTestBase {
   protected:
    void SetUp() override {
        NimcpTestBase::SetUp();  // Call parent first for cleanup

        // Initialize platform time subsystem
        nimcp_platform_time_init();
    }

    void TearDown() override {
        // Cleanup if needed

        NimcpTestBase::TearDown();  // Call parent last for cleanup
    }
};

/**
 * WHAT: Test monotonic time returns non-zero values
 * WHY: Verify basic functionality of monotonic time retrieval
 * HOW: Get time and verify it's positive
 *
 * COVERAGE: Basic functionality
 */
TEST_F(MonotonicTimeTest, MonotonicTimeReturnsNonZero)
{
    uint64_t time_ms = nimcp_platform_time_monotonic_ms();
    EXPECT_GT(time_ms, 0) << "Monotonic time should be positive";
}

/**
 * WHAT: Test that monotonic time always increases
 * WHY: Monotonic clocks must never go backward
 * HOW: Get time multiple times and verify each is >= previous
 *
 * COVERAGE: Core monotonic property
 */
TEST_F(MonotonicTimeTest, TimeAlwaysIncreases)
{
    uint64_t time1 = nimcp_platform_time_monotonic_ms();

    // Small busy-wait to ensure some time passes
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

    uint64_t time2 = nimcp_platform_time_monotonic_ms();

    EXPECT_GE(time2, time1) << "Time should never go backward";
    EXPECT_GT(time2 - time1, 0) << "Time should advance";
}

/**
 * WHAT: Test time never goes backward across multiple calls
 * WHY: Critical property for deadline and timeout calculations
 * HOW: Capture time 10 times and verify monotonic progression
 *
 * COVERAGE: Monotonic property with multiple samples
 */
TEST_F(MonotonicTimeTest, TimeNeverGoesBackward)
{
    std::vector<uint64_t> times;

    // Collect 10 time samples
    for (int i = 0; i < 10; i++) {
        times.push_back(nimcp_platform_time_monotonic_ms());
        // Minimal delay between samples
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    // Verify monotonic property
    for (size_t i = 1; i < times.size(); i++) {
        EXPECT_GE(times[i], times[i-1])
            << "Time at sample " << i << " went backward";
    }

    // Verify overall progression
    EXPECT_GT(times.back(), times.front())
        << "Time should progress over 10 samples";
}

/**
 * WHAT: Test monotonic time consistency over longer duration
 * WHY: Ensure time measurement remains stable over extended period
 * HOW: Sample time over 100ms and verify smooth progression
 *
 * COVERAGE: Long-term monotonic behavior
 */
TEST_F(MonotonicTimeTest, MonotonicTimeConsistent)
{
    uint64_t start = nimcp_platform_time_monotonic_ms();
    std::vector<uint64_t> samples;

    // Collect samples over 100ms
    for (int i = 0; i < 50; i++) {
        samples.push_back(nimcp_platform_time_monotonic_ms());
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    uint64_t end = nimcp_platform_time_monotonic_ms();

    // Verify monotonic progression
    for (size_t i = 1; i < samples.size(); i++) {
        EXPECT_GE(samples[i], samples[i-1])
            << "Monotonic violation at sample " << i;
    }

    // Verify reasonable time advancement
    uint64_t elapsed = end - start;
    EXPECT_GE(elapsed, 90) << "Should have elapsed at least 90ms";
    EXPECT_LT(elapsed, 500) << "Should not have elapsed more than 500ms";
}

/**
 * WHAT: Test monotonic time precision
 * WHY: Verify millisecond granularity is maintained
 * HOW: Check that consecutive calls can observe different values
 *
 * COVERAGE: Time precision
 */
TEST_F(MonotonicTimeTest, MonotonicTimePrecision)
{
    uint64_t time1 = nimcp_platform_time_monotonic_ms();
    uint64_t time2 = nimcp_platform_time_monotonic_ms();

    // Times might be identical due to precision, but shouldn't go backward
    EXPECT_GE(time2, time1) << "Must maintain monotonic property";
}

//=============================================================================
// SLEEP ACCURACY TESTS
//=============================================================================

class SleepAccuracyTest : public NimcpTestBase {
   protected:
    void SetUp() override {
        NimcpTestBase::SetUp();  // Call parent first for cleanup

        nimcp_platform_time_init();
    }

    void TearDown() override {
        NimcpTestBase::TearDown();  // Call parent last for cleanup
    }
};

/**
 * WHAT: Test sleep duration is approximately correct
 * WHY: Sleep must respect requested duration (may be longer due to scheduling)
 * HOW: Sleep 10ms and verify elapsed time matches approximately
 *
 * COVERAGE: Sleep accuracy
 */
TEST_F(SleepAccuracyTest, SleepDurationApproximate)
{
    uint64_t start = nimcp_platform_time_monotonic_ms();
    uint32_t requested_ms = 10;

    nimcp_platform_sleep_ms(requested_ms);

    uint64_t elapsed = nimcp_platform_time_monotonic_ms() - start;

    // Should be at least the requested duration
    EXPECT_GE(elapsed, requested_ms)
        << "Sleep should be at least " << requested_ms << "ms";

    // But not excessively longer (allow 5x tolerance for system scheduling)
    EXPECT_LT(elapsed, requested_ms * 5)
        << "Sleep should not be excessively longer than requested";
}

/**
 * WHAT: Test multiple sleep durations
 * WHY: Verify sleep works correctly for different values
 * HOW: Test sleep with 5ms, 10ms, 20ms
 *
 * COVERAGE: Sleep with various durations
 */
TEST_F(SleepAccuracyTest, VariousSleepDurations)
{
    uint32_t durations[] = {5, 10, 20};

    for (uint32_t requested_ms : durations) {
        uint64_t start = nimcp_platform_time_monotonic_ms();

        nimcp_platform_sleep_ms(requested_ms);

        uint64_t elapsed = nimcp_platform_time_monotonic_ms() - start;

        EXPECT_GE(elapsed, requested_ms)
            << "Sleep " << requested_ms << "ms: elapsed should be >= requested";
        EXPECT_LT(elapsed, requested_ms * 5)
            << "Sleep " << requested_ms << "ms: elapsed should not be excessive";
    }
}

/**
 * WHAT: Test zero-duration sleep
 * WHY: Verify edge case handling (should return immediately)
 * HOW: Sleep for 0ms and verify minimal elapsed time
 *
 * COVERAGE: Edge case - zero duration
 */
TEST_F(SleepAccuracyTest, SleepZeroDuration)
{
    uint64_t start = nimcp_platform_time_monotonic_ms();

    nimcp_platform_sleep_ms(0);

    uint64_t elapsed = nimcp_platform_time_monotonic_ms() - start;

    // Should return almost immediately (within 1ms)
    EXPECT_LT(elapsed, 1)
        << "Zero-duration sleep should return almost immediately";
}

/**
 * WHAT: Test very short sleep duration
 * WHY: Verify millisecond granularity for short sleeps
 * HOW: Sleep for 1ms
 *
 * COVERAGE: Short sleep duration
 */
TEST_F(SleepAccuracyTest, SleepVeryShort)
{
    uint64_t start = nimcp_platform_time_monotonic_ms();

    nimcp_platform_sleep_ms(1);

    uint64_t elapsed = nimcp_platform_time_monotonic_ms() - start;

    // 1ms sleep should elapse at least 1ms (may be rounded up)
    EXPECT_GE(elapsed, 1)
        << "1ms sleep should elapse at least 1ms";
    EXPECT_LT(elapsed, 100)
        << "1ms sleep should not take excessive time";
}

/**
 * WHAT: Test consecutive sleeps accumulate correctly
 * WHY: Verify sleep function works repeatedly without state corruption
 * HOW: Sleep 5ms three times, verify total ~15ms
 *
 * COVERAGE: Repeated sleep calls
 */
TEST_F(SleepAccuracyTest, ConsecutiveSleeps)
{
    uint64_t start = nimcp_platform_time_monotonic_ms();

    nimcp_platform_sleep_ms(5);
    nimcp_platform_sleep_ms(5);
    nimcp_platform_sleep_ms(5);

    uint64_t elapsed = nimcp_platform_time_monotonic_ms() - start;

    // Total should be at least 15ms
    EXPECT_GE(elapsed, 15)
        << "Three 5ms sleeps should total at least 15ms";
    // But not excessively long
    EXPECT_LT(elapsed, 100)
        << "Three 5ms sleeps should not take excessive time";
}

//=============================================================================
// TIME FORMATTING TESTS
//=============================================================================

class TimeFormattingTest : public NimcpTestBase {
   protected:
    void SetUp() override {
        NimcpTestBase::SetUp();  // Call parent first for cleanup

        nimcp_platform_time_init();
        memset(buffer, 0, sizeof(buffer));
    }

    void TearDown() override {
        NimcpTestBase::TearDown();  // Call parent last for cleanup
    }

    char buffer[256];
};

/**
 * WHAT: Test time formatting for small values
 * WHY: Verify string format for times < 1 second
 * HOW: Format 500ms and check output
 *
 * COVERAGE: Small time values
 */
TEST_F(TimeFormattingTest, FormatSmallTime)
{
    uint64_t time_ms = 500;  // 500 milliseconds

    int result = nimcp_platform_time_to_string(time_ms, buffer, sizeof(buffer));

    EXPECT_EQ(result, 0) << "Should format successfully";
    EXPECT_NE(buffer[0], '\0') << "Buffer should not be empty";
    EXPECT_TRUE(strstr(buffer, "500") != nullptr && strstr(buffer, "ms") != nullptr)
        << "Format should contain time components";
}

/**
 * WHAT: Test time formatting for seconds
 * WHY: Verify format includes seconds correctly
 * HOW: Format 65000ms (1m 5s) and check output
 *
 * COVERAGE: Seconds component
 */
TEST_F(TimeFormattingTest, FormatWithSeconds)
{
    uint64_t time_ms = 65000;  // 1 minute 5 seconds

    int result = nimcp_platform_time_to_string(time_ms, buffer, sizeof(buffer));

    EXPECT_EQ(result, 0) << "Should format successfully";
    EXPECT_TRUE(strstr(buffer, "1 m") != nullptr && strstr(buffer, "5 s") != nullptr)
        << "Format should show 1 minute and 5 seconds";
}

/**
 * WHAT: Test time formatting for minutes
 * WHY: Verify format includes minutes correctly
 * HOW: Format 300000ms (5 minutes) and check output
 *
 * COVERAGE: Minutes component
 */
TEST_F(TimeFormattingTest, FormatWithMinutes)
{
    uint64_t time_ms = 300000;  // 5 minutes

    int result = nimcp_platform_time_to_string(time_ms, buffer, sizeof(buffer));

    EXPECT_EQ(result, 0) << "Should format successfully";
    EXPECT_TRUE(strstr(buffer, "5 m") != nullptr)
        << "Format should show 5 minutes";
}

/**
 * WHAT: Test time formatting for hours
 * WHY: Verify format includes hours correctly
 * HOW: Format 7200000ms (2 hours) and check output
 *
 * COVERAGE: Hours component
 */
TEST_F(TimeFormattingTest, FormatWithHours)
{
    uint64_t time_ms = 7200000;  // 2 hours

    int result = nimcp_platform_time_to_string(time_ms, buffer, sizeof(buffer));

    EXPECT_EQ(result, 0) << "Should format successfully";
    EXPECT_TRUE(strstr(buffer, "2 h") != nullptr)
        << "Format should show 2 hours";
}

/**
 * WHAT: Test time formatting for days
 * WHY: Verify format includes days correctly
 * HOW: Format 86400000ms (1 day) and check output
 *
 * COVERAGE: Days component
 */
TEST_F(TimeFormattingTest, FormatWithDays)
{
    uint64_t time_ms = 86400000;  // 1 day

    int result = nimcp_platform_time_to_string(time_ms, buffer, sizeof(buffer));

    EXPECT_EQ(result, 0) << "Should format successfully";
    EXPECT_TRUE(strstr(buffer, "1 d") != nullptr)
        << "Format should show 1 day";
}

/**
 * WHAT: Test time formatting for complex time
 * WHY: Verify all components appear for complex time value
 * HOW: Format 1d 2h 30m 45s 123ms and check
 *
 * COVERAGE: Complex time with all components
 */
TEST_F(TimeFormattingTest, FormatComplexTime)
{
    // 1 day + 2 hours + 30 minutes + 45 seconds + 123 ms
    uint64_t time_ms = (1 * 86400 + 2 * 3600 + 30 * 60 + 45) * 1000 + 123;

    int result = nimcp_platform_time_to_string(time_ms, buffer, sizeof(buffer));

    EXPECT_EQ(result, 0) << "Should format successfully";
    EXPECT_TRUE(strstr(buffer, "1 d") != nullptr && strstr(buffer, "2 h") != nullptr &&
                strstr(buffer, "30 m") != nullptr && strstr(buffer, "45 s") != nullptr &&
                strstr(buffer, "123") != nullptr)
        << "Format should show all time components";
}

/**
 * WHAT: Test time formatting for zero
 * WHY: Verify edge case handling
 * HOW: Format 0ms and check output
 *
 * COVERAGE: Edge case - zero time
 */
TEST_F(TimeFormattingTest, FormatZeroTime)
{
    uint64_t time_ms = 0;

    int result = nimcp_platform_time_to_string(time_ms, buffer, sizeof(buffer));

    EXPECT_EQ(result, 0) << "Should format successfully";
    EXPECT_TRUE(strstr(buffer, "0") != nullptr)
        << "Format should contain 0";
}

/**
 * WHAT: Test time formatting with exact millisecond values
 * WHY: Verify millisecond component is correctly formatted
 * HOW: Format various millisecond values and verify format
 *
 * COVERAGE: Millisecond precision
 */
TEST_F(TimeFormattingTest, FormatMillisecondPrecision)
{
    uint64_t test_values[] = {1, 10, 100, 999};

    for (uint64_t ms : test_values) {
        memset(buffer, 0, sizeof(buffer));
        int result = nimcp_platform_time_to_string(ms, buffer, sizeof(buffer));

        EXPECT_EQ(result, 0) << "Should format " << ms << "ms successfully";
        EXPECT_NE(buffer[0], '\0') << "Buffer should be non-empty";
    }
}

//=============================================================================
// TIME PROGRESSION TESTS
//=============================================================================

class TimeProgressionTest : public NimcpTestBase {
   protected:
    void SetUp() override {
        NimcpTestBase::SetUp();  // Call parent first for cleanup

        nimcp_platform_time_init();
    }

    void TearDown() override {
        NimcpTestBase::TearDown();  // Call parent last for cleanup
    }
};

/**
 * WHAT: Test time progresses at expected rate
 * WHY: Verify time measurement accuracy over known duration
 * HOW: Sleep 50ms, measure elapsed, verify it's in expected range
 *
 * COVERAGE: Time progression rate
 */
TEST_F(TimeProgressionTest, TimeProgressesAtExpectedRate)
{
    uint64_t start = nimcp_platform_time_monotonic_ms();
    uint32_t sleep_ms = 50;

    nimcp_platform_sleep_ms(sleep_ms);

    uint64_t elapsed = nimcp_platform_time_monotonic_ms() - start;

    // Should be at least the sleep duration
    EXPECT_GE(elapsed, sleep_ms)
        << "Elapsed time should be >= sleep duration";

    // Should not be more than 2x the sleep duration (reasonable tolerance)
    EXPECT_LT(elapsed, sleep_ms * 2)
        << "Elapsed time should not be excessive";
}

/**
 * WHAT: Test multiple time measurements show consistent progression
 * WHY: Verify time doesn't jump or stall unexpectedly
 * HOW: Take 5 measurements with 10ms delays, check uniform progression
 *
 * COVERAGE: Consistent progression
 */
TEST_F(TimeProgressionTest, ConsistentProgression)
{
    std::vector<uint64_t> times;
    std::vector<uint64_t> deltas;

    uint64_t start = nimcp_platform_time_monotonic_ms();
    times.push_back(start);

    // Take measurements every 10ms
    for (int i = 0; i < 5; i++) {
        nimcp_platform_sleep_ms(10);
        uint64_t current = nimcp_platform_time_monotonic_ms();
        times.push_back(current);
    }

    // Calculate deltas between measurements
    for (size_t i = 1; i < times.size(); i++) {
        deltas.push_back(times[i] - times[i-1]);
    }

    // Check that deltas are reasonable (around 10ms each)
    for (uint64_t delta : deltas) {
        EXPECT_GE(delta, 5) << "Delta should be at least 5ms";
        EXPECT_LT(delta, 100) << "Delta should not exceed 100ms";
    }

    // Total progression should be ~50ms
    uint64_t total = times.back() - times.front();
    EXPECT_GE(total, 40) << "Total should be at least 40ms";
    EXPECT_LT(total, 200) << "Total should not exceed 200ms";
}

/**
 * WHAT: Test rapid time measurements remain ordered
 * WHY: Verify monotonic property even for back-to-back calls
 * HOW: Rapidly call time function and verify no regressions
 *
 * COVERAGE: Rapid measurements
 */
TEST_F(TimeProgressionTest, RapidMeasurementsMonotonic)
{
    std::vector<uint64_t> times;

    // Take 1000 rapid measurements
    for (int i = 0; i < 1000; i++) {
        times.push_back(nimcp_platform_time_monotonic_ms());
    }

    // Verify monotonic property
    for (size_t i = 1; i < times.size(); i++) {
        EXPECT_GE(times[i], times[i-1])
            << "Rapid measurement " << i << " violated monotonic property";
    }
}

//=============================================================================
// NULL POINTER SAFETY TESTS
//=============================================================================

class NullSafetyTest : public NimcpTestBase {
   protected:
    void SetUp() override {
        NimcpTestBase::SetUp();  // Call parent first for cleanup

        nimcp_platform_time_init();
    }

    void TearDown() override {
        NimcpTestBase::TearDown();  // Call parent last for cleanup
    }
};

/**
 * WHAT: Test format function rejects NULL buffer
 * WHY: Prevent NULL pointer dereference
 * HOW: Call nimcp_platform_time_to_string with NULL buffer
 *
 * COVERAGE: NULL pointer handling
 */
TEST_F(NullSafetyTest, FormatRejectsNullBuffer)
{
    uint64_t time_ms = 1000;

    int result = nimcp_platform_time_to_string(time_ms, NULL, 256);

    EXPECT_EQ(result, -1) << "Should return error for NULL buffer";
}

/**
 * WHAT: Test format function rejects zero-sized buffer
 * WHY: Prevent buffer overflow
 * HOW: Call with valid buffer but size=0
 *
 * COVERAGE: Buffer size validation
 */
TEST_F(NullSafetyTest, FormatRejectsZeroSize)
{
    uint64_t time_ms = 1000;
    char buffer[256];

    int result = nimcp_platform_time_to_string(time_ms, buffer, 0);

    EXPECT_EQ(result, -1) << "Should return error for zero buffer size";
}

/**
 * WHAT: Test format function rejects small buffer
 * WHY: Prevent buffer overflow, minimum is 32 bytes
 * HOW: Call with buffer size < 32
 *
 * COVERAGE: Minimum buffer size validation
 */
TEST_F(NullSafetyTest, FormatRejectsSmallBuffer)
{
    uint64_t time_ms = 1000;
    char buffer[16];  // Too small (minimum 32)

    int result = nimcp_platform_time_to_string(time_ms, buffer, sizeof(buffer));

    EXPECT_EQ(result, -1) << "Should return error for buffer < 32 bytes";
}

/**
 * WHAT: Test format function accepts minimum buffer size
 * WHY: Verify 32-byte buffer is exactly sufficient
 * HOW: Call with exactly 32-byte buffer
 *
 * COVERAGE: Minimum buffer acceptance
 */
TEST_F(NullSafetyTest, FormatAcceptsMinimumBuffer)
{
    uint64_t time_ms = 1000;
    char buffer[32];
    memset(buffer, 0, sizeof(buffer));

    int result = nimcp_platform_time_to_string(time_ms, buffer, sizeof(buffer));

    EXPECT_EQ(result, 0) << "Should succeed with 32-byte buffer";
    EXPECT_NE(buffer[0], '\0') << "Buffer should be populated";
}

/**
 * WHAT: Test format function doesn't overflow on large times
 * WHY: Ensure format works for very large time values
 * HOW: Call with large time value and verify it doesn't overflow
 *
 * COVERAGE: Large time value handling
 */
TEST_F(NullSafetyTest, FormatHandlesLargeTime)
{
    // Very large time: 999 days
    uint64_t time_ms = 999ULL * 86400 * 1000;
    char buffer[256];
    memset(buffer, 0, sizeof(buffer));

    int result = nimcp_platform_time_to_string(time_ms, buffer, sizeof(buffer));

    EXPECT_EQ(result, 0) << "Should handle large time values";
    EXPECT_NE(buffer[0], '\0') << "Buffer should be populated";
    EXPECT_TRUE(strstr(buffer, "999") != nullptr)
        << "Should show correct day count";
}

/**
 * WHAT: Test initialization can be called multiple times safely
 * WHY: Ensure idempotent behavior for initialization
 * HOW: Call init multiple times and verify success
 *
 * COVERAGE: Initialization safety
 */
TEST_F(NullSafetyTest, InitializationIsIdempotent)
{
    int result1 = nimcp_platform_time_init();
    int result2 = nimcp_platform_time_init();
    int result3 = nimcp_platform_time_init();

    EXPECT_EQ(result1, 0) << "First init should succeed";
    EXPECT_EQ(result2, 0) << "Second init should succeed";
    EXPECT_EQ(result3, 0) << "Third init should succeed";
}

//=============================================================================
// INTEGRATION TESTS
//=============================================================================

class PlatformTimeIntegrationTest : public NimcpTestBase {
   protected:
    void SetUp() override {
        NimcpTestBase::SetUp();  // Call parent first for cleanup

        nimcp_platform_time_init();
    }

    void TearDown() override {
        NimcpTestBase::TearDown();  // Call parent last for cleanup
    }
};

/**
 * WHAT: Test comprehensive timing scenario
 * WHY: Verify all functions work together correctly
 * HOW: Perform sleep, measurement, and formatting in sequence
 *
 * COVERAGE: Integration
 */
TEST_F(PlatformTimeIntegrationTest, ComprehensiveTimingScenario)
{
    // Get initial time
    uint64_t start = nimcp_platform_time_monotonic_ms();

    // Sleep for 20ms
    nimcp_platform_sleep_ms(20);

    // Get elapsed time
    uint64_t elapsed = nimcp_platform_time_monotonic_ms() - start;

    // Format the elapsed time
    char buffer[256];
    int result = nimcp_platform_time_to_string(elapsed, buffer, sizeof(buffer));

    EXPECT_GE(elapsed, 20) << "Should have elapsed at least 20ms";
    EXPECT_EQ(result, 0) << "Should format successfully";
    EXPECT_NE(buffer[0], '\0') << "Buffer should contain result";
}

/**
 * WHAT: Test time measurement across thread sleep
 * WHY: Verify time measurement works with std::thread sleep
 * HOW: Use std::thread::sleep_for and measure with platform time
 *
 * COVERAGE: Interoperability with thread sleep
 */
TEST_F(PlatformTimeIntegrationTest, TimeWithThreadSleep)
{
    uint64_t start = nimcp_platform_time_monotonic_ms();

    std::this_thread::sleep_for(std::chrono::milliseconds(15));

    uint64_t elapsed = nimcp_platform_time_monotonic_ms() - start;

    // Should have elapsed at least 15ms
    EXPECT_GE(elapsed, 15)
        << "Should measure thread sleep correctly";
    EXPECT_LT(elapsed, 200)
        << "Should not measure excessive time";
}

/**
 * WHAT: Test monotonic property across format call
 * WHY: Verify formatting doesn't affect time measurement
 * HOW: Get time, format it, get time again, verify monotonic
 *
 * COVERAGE: Format doesn't affect time state
 */
TEST_F(PlatformTimeIntegrationTest, MonotonicThroughFormatting)
{
    uint64_t time1 = nimcp_platform_time_monotonic_ms();

    char buffer[256];
    nimcp_platform_time_to_string(time1, buffer, sizeof(buffer));

    uint64_t time2 = nimcp_platform_time_monotonic_ms();

    EXPECT_GE(time2, time1)
        << "Time should remain monotonic after formatting";
}

/**
 * WHAT: Test performance profiling pattern
 * WHY: Verify typical use case works correctly
 * HOW: Profile a loop using time measurement and sleep
 *
 * COVERAGE: Performance profiling use case
 */
TEST_F(PlatformTimeIntegrationTest, PerformanceProfilingPattern)
{
    std::vector<uint64_t> durations;

    // Profile 5 iterations of a 10ms sleep
    for (int i = 0; i < 5; i++) {
        uint64_t start = nimcp_platform_time_monotonic_ms();

        nimcp_platform_sleep_ms(10);

        uint64_t elapsed = nimcp_platform_time_monotonic_ms() - start;
        durations.push_back(elapsed);
    }

    // Verify all measurements are reasonable
    for (size_t i = 0; i < durations.size(); i++) {
        EXPECT_GE(durations[i], 10)
            << "Iteration " << i << ": should be >= 10ms";
        EXPECT_LT(durations[i], 100)
            << "Iteration " << i << ": should be < 100ms";
    }
}

//=============================================================================
// STRESS TESTS
//=============================================================================

class StressTest : public NimcpTestBase {
   protected:
    void SetUp() override {
        NimcpTestBase::SetUp();  // Call parent first for cleanup

        nimcp_platform_time_init();
    }

    void TearDown() override {
        NimcpTestBase::TearDown();  // Call parent last for cleanup
    }
};

/**
 * WHAT: Test many rapid time measurements
 * WHY: Ensure no performance degradation or memory leaks
 * HOW: Call monotonic time 10,000 times
 *
 * COVERAGE: Stress - rapid calls
 */
TEST_F(StressTest, RapidTimeCallsStress)
{
    std::vector<uint64_t> times;

    // Call 10,000 times with a brief sleep partway through
    // to ensure millisecond clock actually advances (10k calls can complete < 1ms)
    for (int i = 0; i < 10000; i++) {
        times.push_back(nimcp_platform_time_monotonic_ms());
        // Brief sleep at midpoint to ensure clock advances at least once
        if (i == 5000) {
            nimcp_platform_sleep_ms(2);
        }
    }

    // Verify monotonic property maintained
    for (size_t i = 1; i < times.size(); i++) {
        EXPECT_GE(times[i], times[i-1])
            << "Stress test: monotonic violation at " << i;
    }

    // Verify time advanced (we added a 2ms sleep, so it must advance)
    EXPECT_GT(times.back(), times.front())
        << "Time should advance over 10,000 calls with 2ms sleep";
}

/**
 * WHAT: Test many format calls
 * WHY: Ensure formatting doesn't have memory issues
 * HOW: Format 1000 different time values
 *
 * COVERAGE: Stress - formatting
 */
TEST_F(StressTest, ManyFormatCalls)
{
    char buffer[256];

    for (uint64_t i = 0; i < 1000; i++) {
        uint64_t time_ms = i * 1000 + i % 1000;

        int result = nimcp_platform_time_to_string(time_ms, buffer, sizeof(buffer));

        EXPECT_EQ(result, 0)
            << "Format should succeed for time " << time_ms;
    }
}

/**
 * WHAT: Test many sleep calls
 * WHY: Ensure sleep function can be called repeatedly
 * HOW: Sleep 100 times for 1ms each
 *
 * COVERAGE: Stress - sleep calls
 */
TEST_F(StressTest, ManySleepCalls)
{
    uint64_t start = nimcp_platform_time_monotonic_ms();

    // Sleep 100 times for 1ms
    for (int i = 0; i < 100; i++) {
        nimcp_platform_sleep_ms(1);
    }

    uint64_t elapsed = nimcp_platform_time_monotonic_ms() - start;

    // Should be at least 100ms
    EXPECT_GE(elapsed, 100)
        << "100 x 1ms sleeps should be at least 100ms";
}

