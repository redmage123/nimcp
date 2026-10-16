/**
 * @file test_utils_platform_time.cpp
 * @brief Comprehensive unit tests for platform time utilities
 *
 * WHAT: Complete test coverage for cross-platform monotonic time measurement
 * WHY:  Ensure timing functions work reliably and accurately across all platforms
 * HOW:  GoogleTest framework with organized fixtures and comprehensive coverage
 *
 * TEST COVERAGE:
 * 1. Initialization and lifecycle
 * 2. Monotonic time accuracy and precision
 * 3. Time difference calculations
 * 4. Sleep function accuracy
 * 5. Time formatting and string conversion
 * 6. Monotonic clock properties (never goes backward)
 * 7. Resolution and precision verification
 * 8. Edge cases and boundary conditions
 * 9. Stress testing and performance
 * 10. Integration scenarios
 *
 * PLATFORM SUPPORT:
 * - Linux: clock_gettime(CLOCK_MONOTONIC)
 * - macOS: mach_absolute_time()
 * - Windows: QueryPerformanceCounter()
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <vector>
#include <cstring>
#include <cmath>

#include "utils/platform/nimcp_platform_time.h"

//=============================================================================
// Test Fixture - Base Setup
//=============================================================================

class UtilsPlatformTimeTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize platform time subsystem
        int result = nimcp_platform_time_init();
        ASSERT_EQ(result, 0) << "Platform time initialization failed";
    }

    void TearDown() override {
        // Cleanup if needed (none required for time functions)
    }

    // Helper: Sleep for specified milliseconds using C++ thread
    void sleep_ms(uint32_t ms) {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }

    // Helper: Check if two values are within tolerance
    bool within_tolerance(uint64_t value, uint64_t expected, uint64_t tolerance) {
        uint64_t diff = (value > expected) ? (value - expected) : (expected - value);
        return diff <= tolerance;
    }
};

//=============================================================================
// TEST 1: Initialization and Reinitialization
//=============================================================================

/**
 * WHAT: Test platform time initialization
 * WHY:  Verify initialization succeeds and is idempotent
 * HOW:  Call init multiple times and verify success
 *
 * COVERAGE: Initialization, idempotency
 */
TEST_F(UtilsPlatformTimeTest, InitializationSucceeds) {
    // First initialization (already done in SetUp)
    int result1 = nimcp_platform_time_init();
    EXPECT_EQ(result1, 0) << "First re-init should succeed";

    // Second initialization - should be idempotent
    int result2 = nimcp_platform_time_init();
    EXPECT_EQ(result2, 0) << "Second re-init should succeed (idempotent)";

    // Third initialization
    int result3 = nimcp_platform_time_init();
    EXPECT_EQ(result3, 0) << "Third re-init should succeed (idempotent)";

    // Time functions should still work after multiple inits
    uint64_t time = nimcp_platform_time_monotonic_ms();
    EXPECT_GT(time, 0) << "Time should be positive after multiple inits";
}

//=============================================================================
// TEST 2: Monotonic Time Accuracy
//=============================================================================

/**
 * WHAT: Test monotonic time returns accurate values
 * WHY:  Verify time measurement is working and returns reasonable values
 * HOW:  Get time, sleep, get time again, verify difference
 *
 * COVERAGE: Basic monotonic time accuracy
 */
TEST_F(UtilsPlatformTimeTest, MonotonicTimeAccuracy) {
    uint64_t start = nimcp_platform_time_monotonic_ms();
    EXPECT_GT(start, 0) << "Start time should be positive";

    // Sleep for 50ms
    nimcp_platform_sleep_ms(50);

    uint64_t end = nimcp_platform_time_monotonic_ms();
    EXPECT_GT(end, start) << "End time should be greater than start";

    // Calculate elapsed time
    uint64_t elapsed = end - start;

    // Should be at least 50ms (requested sleep time)
    EXPECT_GE(elapsed, 50) << "Elapsed time should be at least 50ms";

    // Should not be excessively longer (allow 5x for system scheduling)
    EXPECT_LT(elapsed, 250) << "Elapsed time should not exceed 250ms (5x tolerance)";

    // Report actual elapsed time for diagnostics
    GTEST_LOG_(INFO) << "Requested 50ms, measured " << elapsed << "ms";
}

//=============================================================================
// TEST 3: Monotonic Clock Never Goes Backward
//=============================================================================

/**
 * WHAT: Test monotonic property - time never goes backward
 * WHY:  Critical property for deadline and timeout calculations
 * HOW:  Sample time repeatedly and verify monotonic progression
 *
 * COVERAGE: Monotonic property, multiple samples
 */
TEST_F(UtilsPlatformTimeTest, MonotonicClockNeverGoesBackward) {
    const int sample_count = 100;
    std::vector<uint64_t> times;
    times.reserve(sample_count);

    // Collect many time samples with small delays
    for (int i = 0; i < sample_count; i++) {
        times.push_back(nimcp_platform_time_monotonic_ms());

        // Small delay to ensure time progresses
        if (i % 10 == 0) {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }

    // Verify strictly monotonic property (never goes backward)
    for (size_t i = 1; i < times.size(); i++) {
        EXPECT_GE(times[i], times[i-1])
            << "Time went backward at sample " << i
            << " (previous: " << times[i-1] << ", current: " << times[i] << ")";
    }

    // Verify overall progression
    EXPECT_GT(times.back(), times.front())
        << "Time should progress over " << sample_count << " samples";

    // Calculate and report total progression
    uint64_t total_progression = times.back() - times.front();
    GTEST_LOG_(INFO) << "Total time progression over " << sample_count
                     << " samples: " << total_progression << "ms";
}

//=============================================================================
// TEST 4: Time Difference Calculations
//=============================================================================

/**
 * WHAT: Test accurate time difference calculations
 * WHY:  Verify duration measurement is precise
 * HOW:  Measure known sleep durations and verify accuracy
 *
 * COVERAGE: Duration measurement, multiple sleep values
 */
TEST_F(UtilsPlatformTimeTest, TimeDifferenceCalculations) {
    const uint32_t test_durations[] = {5, 10, 20, 50, 100};

    for (uint32_t duration : test_durations) {
        uint64_t start = nimcp_platform_time_monotonic_ms();

        nimcp_platform_sleep_ms(duration);

        uint64_t end = nimcp_platform_time_monotonic_ms();
        uint64_t measured = end - start;

        // Should be at least the requested duration
        EXPECT_GE(measured, duration)
            << "Measured time should be >= requested " << duration << "ms";

        // Should not be excessively longer (allow 3x for shorter sleeps)
        uint64_t max_allowed = duration * 3;
        EXPECT_LT(measured, max_allowed)
            << "Measured time should be < " << max_allowed
            << "ms for " << duration << "ms sleep";

        // Calculate accuracy percentage
        double accuracy = 100.0 * (double)measured / (double)duration;
        GTEST_LOG_(INFO) << "Sleep " << duration << "ms: measured "
                        << measured << "ms (accuracy: " << accuracy << "%)";
    }
}

//=============================================================================
// TEST 5: Sleep Function Accuracy
//=============================================================================

/**
 * WHAT: Test sleep function accuracy across various durations
 * WHY:  Verify sleep respects requested duration
 * HOW:  Sleep for various durations and measure actual elapsed time
 *
 * COVERAGE: Sleep accuracy, edge cases (zero, very short, long)
 */
TEST_F(UtilsPlatformTimeTest, SleepFunctionAccuracy) {
    // Test zero sleep
    {
        uint64_t start = nimcp_platform_time_monotonic_ms();
        nimcp_platform_sleep_ms(0);
        uint64_t elapsed = nimcp_platform_time_monotonic_ms() - start;
        EXPECT_LE(elapsed, 2) << "Zero sleep should return almost immediately";
    }

    // Test very short sleep (1ms)
    {
        uint64_t start = nimcp_platform_time_monotonic_ms();
        nimcp_platform_sleep_ms(1);
        uint64_t elapsed = nimcp_platform_time_monotonic_ms() - start;
        EXPECT_GE(elapsed, 1) << "1ms sleep should elapse at least 1ms";
        EXPECT_LT(elapsed, 50) << "1ms sleep should not take excessive time";
    }

    // Test moderate sleep (25ms)
    {
        uint64_t start = nimcp_platform_time_monotonic_ms();
        nimcp_platform_sleep_ms(25);
        uint64_t elapsed = nimcp_platform_time_monotonic_ms() - start;
        EXPECT_GE(elapsed, 25) << "25ms sleep should elapse at least 25ms";
        EXPECT_LT(elapsed, 100) << "25ms sleep should be reasonable";
    }

    // Test longer sleep (100ms)
    {
        uint64_t start = nimcp_platform_time_monotonic_ms();
        nimcp_platform_sleep_ms(100);
        uint64_t elapsed = nimcp_platform_time_monotonic_ms() - start;
        EXPECT_GE(elapsed, 100) << "100ms sleep should elapse at least 100ms";
        EXPECT_LT(elapsed, 200) << "100ms sleep should not exceed 200ms";
    }

    // Test consecutive sleeps accumulate correctly
    {
        uint64_t start = nimcp_platform_time_monotonic_ms();
        nimcp_platform_sleep_ms(10);
        nimcp_platform_sleep_ms(10);
        nimcp_platform_sleep_ms(10);
        uint64_t elapsed = nimcp_platform_time_monotonic_ms() - start;
        EXPECT_GE(elapsed, 30) << "Three 10ms sleeps should total >= 30ms";
        EXPECT_LT(elapsed, 150) << "Three 10ms sleeps should be reasonable";
    }
}

//=============================================================================
// TEST 6: Timestamp Formatting
//=============================================================================

/**
 * WHAT: Test time-to-string formatting
 * WHY:  Verify formatted output is correct and readable
 * HOW:  Format various time values and verify format correctness
 *
 * COVERAGE: String formatting, all time components (days, hours, min, sec, ms)
 */
TEST_F(UtilsPlatformTimeTest, TimestampFormatting) {
    char buffer[256];

    // Test small time (500ms)
    {
        memset(buffer, 0, sizeof(buffer));
        int result = nimcp_platform_time_to_string(500, buffer, sizeof(buffer));
        EXPECT_EQ(result, 0) << "Format should succeed for 500ms";
        EXPECT_NE(buffer[0], '\0') << "Buffer should not be empty";
        EXPECT_TRUE(strstr(buffer, "500") != nullptr && strstr(buffer, "ms") != nullptr)
            << "Format should contain milliseconds: " << buffer;
        GTEST_LOG_(INFO) << "500ms formatted as: " << buffer;
    }

    // Test with seconds (65 seconds = 1m 5s)
    {
        memset(buffer, 0, sizeof(buffer));
        uint64_t time_ms = 65 * 1000;  // 65 seconds
        int result = nimcp_platform_time_to_string(time_ms, buffer, sizeof(buffer));
        EXPECT_EQ(result, 0) << "Format should succeed for 65 seconds";
        EXPECT_TRUE(strstr(buffer, "1 m") != nullptr) << "Should show minutes";
        EXPECT_TRUE(strstr(buffer, "5 s") != nullptr) << "Should show seconds";
        GTEST_LOG_(INFO) << "65 seconds formatted as: " << buffer;
    }

    // Test with hours (7200 seconds = 2 hours)
    {
        memset(buffer, 0, sizeof(buffer));
        uint64_t time_ms = 7200 * 1000;  // 2 hours
        int result = nimcp_platform_time_to_string(time_ms, buffer, sizeof(buffer));
        EXPECT_EQ(result, 0) << "Format should succeed for 2 hours";
        EXPECT_TRUE(strstr(buffer, "2 h") != nullptr) << "Should show hours";
        GTEST_LOG_(INFO) << "2 hours formatted as: " << buffer;
    }

    // Test with days (86400 seconds = 1 day)
    {
        memset(buffer, 0, sizeof(buffer));
        uint64_t time_ms = 86400 * 1000ULL;  // 1 day
        int result = nimcp_platform_time_to_string(time_ms, buffer, sizeof(buffer));
        EXPECT_EQ(result, 0) << "Format should succeed for 1 day";
        EXPECT_TRUE(strstr(buffer, "1 d") != nullptr) << "Should show days";
        GTEST_LOG_(INFO) << "1 day formatted as: " << buffer;
    }

    // Test complex time (1d 2h 30m 45s 123ms)
    {
        memset(buffer, 0, sizeof(buffer));
        uint64_t time_ms = (1ULL * 86400 + 2 * 3600 + 30 * 60 + 45) * 1000 + 123;
        int result = nimcp_platform_time_to_string(time_ms, buffer, sizeof(buffer));
        EXPECT_EQ(result, 0) << "Format should succeed for complex time";
        EXPECT_TRUE(strstr(buffer, "1 d") != nullptr) << "Should show days";
        EXPECT_TRUE(strstr(buffer, "2 h") != nullptr) << "Should show hours";
        EXPECT_TRUE(strstr(buffer, "30 m") != nullptr) << "Should show minutes";
        EXPECT_TRUE(strstr(buffer, "45 s") != nullptr) << "Should show seconds";
        EXPECT_TRUE(strstr(buffer, "123") != nullptr) << "Should show milliseconds";
        GTEST_LOG_(INFO) << "Complex time formatted as: " << buffer;
    }

    // Test zero time
    {
        memset(buffer, 0, sizeof(buffer));
        int result = nimcp_platform_time_to_string(0, buffer, sizeof(buffer));
        EXPECT_EQ(result, 0) << "Format should succeed for zero";
        EXPECT_TRUE(strstr(buffer, "0") != nullptr) << "Should contain 0";
        GTEST_LOG_(INFO) << "Zero time formatted as: " << buffer;
    }
}

//=============================================================================
// TEST 7: Formatting Edge Cases and Error Handling
//=============================================================================

/**
 * WHAT: Test time formatting error handling
 * WHY:  Verify proper handling of invalid inputs
 * HOW:  Test NULL buffer, small buffer, edge cases
 *
 * COVERAGE: NULL safety, buffer size validation, edge cases
 */
TEST_F(UtilsPlatformTimeTest, FormattingErrorHandling) {
    // Test NULL buffer
    {
        int result = nimcp_platform_time_to_string(1000, nullptr, 256);
        EXPECT_EQ(result, -1) << "Should reject NULL buffer";
    }

    // Test zero-sized buffer
    {
        char buffer[256];
        int result = nimcp_platform_time_to_string(1000, buffer, 0);
        EXPECT_EQ(result, -1) << "Should reject zero-size buffer";
    }

    // Test buffer too small (< 32 bytes minimum)
    {
        char buffer[16];
        int result = nimcp_platform_time_to_string(1000, buffer, sizeof(buffer));
        EXPECT_EQ(result, -1) << "Should reject buffer < 32 bytes";
    }

    // Test exactly minimum buffer size (32 bytes)
    {
        char buffer[32];
        memset(buffer, 0, sizeof(buffer));
        int result = nimcp_platform_time_to_string(1000, buffer, sizeof(buffer));
        EXPECT_EQ(result, 0) << "Should accept exactly 32-byte buffer";
        EXPECT_NE(buffer[0], '\0') << "Buffer should be populated";
    }

    // Test large time value (999 days)
    {
        char buffer[256];
        memset(buffer, 0, sizeof(buffer));
        uint64_t time_ms = 999ULL * 86400 * 1000;
        int result = nimcp_platform_time_to_string(time_ms, buffer, sizeof(buffer));
        EXPECT_EQ(result, 0) << "Should handle large time values";
        EXPECT_TRUE(strstr(buffer, "999") != nullptr) << "Should show correct day count";
        GTEST_LOG_(INFO) << "999 days formatted as: " << buffer;
    }

    // Test very large time value (close to uint64_t max)
    {
        char buffer[256];
        memset(buffer, 0, sizeof(buffer));
        uint64_t time_ms = UINT64_MAX / 2;  // Half of max to avoid overflow
        int result = nimcp_platform_time_to_string(time_ms, buffer, sizeof(buffer));
        EXPECT_EQ(result, 0) << "Should handle very large time values";
        EXPECT_NE(buffer[0], '\0') << "Buffer should be populated";
    }
}

//=============================================================================
// TEST 8: Resolution and Precision
//=============================================================================

/**
 * WHAT: Test monotonic clock resolution and precision
 * WHY:  Verify clock has sufficient resolution for timing
 * HOW:  Take rapid measurements and analyze resolution
 *
 * COVERAGE: Clock resolution, precision verification
 */
TEST_F(UtilsPlatformTimeTest, ResolutionAndPrecision) {
    const int sample_count = 1000;
    std::vector<uint64_t> times;
    times.reserve(sample_count);

    // Collect rapid time samples
    for (int i = 0; i < sample_count; i++) {
        times.push_back(nimcp_platform_time_monotonic_ms());
    }

    // Verify all samples are valid
    for (uint64_t time : times) {
        EXPECT_GT(time, 0) << "All time samples should be positive";
    }

    // Verify monotonic property
    for (size_t i = 1; i < times.size(); i++) {
        EXPECT_GE(times[i], times[i-1])
            << "Rapid sample " << i << " violated monotonic property";
    }

    // Analyze resolution - count unique time values
    std::vector<uint64_t> unique_times = times;
    std::sort(unique_times.begin(), unique_times.end());
    auto last = std::unique(unique_times.begin(), unique_times.end());
    unique_times.erase(last, unique_times.end());

    // Calculate statistics
    uint64_t total_range = times.back() - times.front();
    size_t unique_count = unique_times.size();
    double uniqueness_ratio = (double)unique_count / (double)sample_count * 100.0;

    GTEST_LOG_(INFO) << "Resolution analysis over " << sample_count << " rapid samples:";
    GTEST_LOG_(INFO) << "  Total time range: " << total_range << "ms";
    GTEST_LOG_(INFO) << "  Unique values: " << unique_count << " ("
                     << uniqueness_ratio << "%)";

    // Note: On many platforms, millisecond precision means rapid calls return
    // the same value. This is expected behavior, not a failure.
    // The key test is that time progresses over measurable intervals.

    // Test that time progresses over slightly longer period
    uint64_t start = nimcp_platform_time_monotonic_ms();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    uint64_t end = nimcp_platform_time_monotonic_ms();
    EXPECT_GT(end, start) << "Time should progress over 5ms delay";

    // Verify resolution is at least millisecond precision
    // (time advances by at least 1ms over a 5ms delay)
    uint64_t progression = end - start;
    EXPECT_GE(progression, 1) << "Time should advance by at least 1ms";
}

//=============================================================================
// TEST 9: Stress Test - Performance Under Load
//=============================================================================

/**
 * WHAT: Stress test monotonic time under heavy load
 * WHY:  Verify stability and performance under sustained usage
 * HOW:  Call time functions many times and verify consistency
 *
 * COVERAGE: Performance, stability, no degradation
 */
TEST_F(UtilsPlatformTimeTest, StressTestPerformance) {
    const int iteration_count = 100000;
    std::vector<uint64_t> times;
    times.reserve(iteration_count);

    // Measure time for many rapid calls
    auto stress_start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iteration_count; i++) {
        times.push_back(nimcp_platform_time_monotonic_ms());
    }

    auto stress_end = std::chrono::high_resolution_clock::now();
    auto stress_duration = std::chrono::duration_cast<std::chrono::microseconds>(
        stress_end - stress_start);

    // Verify all samples maintained monotonic property
    for (size_t i = 1; i < times.size(); i++) {
        EXPECT_GE(times[i], times[i-1])
            << "Stress test: monotonic violation at sample " << i;
    }

    // Calculate performance metrics
    double ops_per_second = (double)iteration_count /
                           ((double)stress_duration.count() / 1000000.0);
    double avg_time_per_call_ns = (double)stress_duration.count() * 1000.0 /
                                  (double)iteration_count;

    GTEST_LOG_(INFO) << "Stress test performance (" << iteration_count << " calls):";
    GTEST_LOG_(INFO) << "  Operations/second: " << ops_per_second;
    GTEST_LOG_(INFO) << "  Avg time per call: " << avg_time_per_call_ns << " ns";
    GTEST_LOG_(INFO) << "  Total duration: " << stress_duration.count() << " us";

    // Verify reasonable performance (should be fast)
    EXPECT_LT(avg_time_per_call_ns, 10000)
        << "Each call should take less than 10 microseconds on average";

    // Test many format calls
    char buffer[256];
    int format_errors = 0;

    for (int i = 0; i < 1000; i++) {
        uint64_t time_ms = i * 1000 + (i % 1000);
        int result = nimcp_platform_time_to_string(time_ms, buffer, sizeof(buffer));
        if (result != 0) {
            format_errors++;
        }
    }

    EXPECT_EQ(format_errors, 0) << "All format calls should succeed";

    // Test many sleep calls
    uint64_t sleep_start = nimcp_platform_time_monotonic_ms();
    for (int i = 0; i < 50; i++) {
        nimcp_platform_sleep_ms(1);
    }
    uint64_t sleep_elapsed = nimcp_platform_time_monotonic_ms() - sleep_start;

    EXPECT_GE(sleep_elapsed, 50) << "50 x 1ms sleeps should be >= 50ms";
    GTEST_LOG_(INFO) << "50 x 1ms sleeps took: " << sleep_elapsed << "ms";
}

//=============================================================================
// TEST 10: Integration Scenarios
//=============================================================================

/**
 * WHAT: Test realistic integration scenarios
 * WHY:  Verify all functions work together in real-world use cases
 * HOW:  Simulate performance profiling, timeout handling, logging
 *
 * COVERAGE: Integration, real-world usage patterns
 */
TEST_F(UtilsPlatformTimeTest, IntegrationScenarios) {
    // Scenario 1: Performance profiling pattern
    {
        std::vector<uint64_t> operation_times;

        for (int i = 0; i < 5; i++) {
            uint64_t op_start = nimcp_platform_time_monotonic_ms();

            // Simulate some work
            nimcp_platform_sleep_ms(5);

            uint64_t op_duration = nimcp_platform_time_monotonic_ms() - op_start;
            operation_times.push_back(op_duration);
        }

        // Verify all measurements are reasonable
        for (size_t i = 0; i < operation_times.size(); i++) {
            EXPECT_GE(operation_times[i], 5)
                << "Operation " << i << " should take >= 5ms";
            EXPECT_LT(operation_times[i], 50)
                << "Operation " << i << " should take < 50ms";
        }

        // Calculate average
        uint64_t total = 0;
        for (uint64_t time : operation_times) {
            total += time;
        }
        double average = (double)total / (double)operation_times.size();
        GTEST_LOG_(INFO) << "Performance profiling: avg operation time = "
                        << average << "ms";
    }

    // Scenario 2: Timeout detection pattern
    {
        uint64_t timeout_start = nimcp_platform_time_monotonic_ms();
        const uint64_t timeout_ms = 100;
        bool timeout_detected = false;

        while (!timeout_detected) {
            uint64_t elapsed = nimcp_platform_time_monotonic_ms() - timeout_start;
            if (elapsed >= timeout_ms) {
                timeout_detected = true;
            }

            // Simulate work
            nimcp_platform_sleep_ms(10);
        }

        uint64_t actual_timeout = nimcp_platform_time_monotonic_ms() - timeout_start;
        EXPECT_GE(actual_timeout, timeout_ms)
            << "Timeout should fire after " << timeout_ms << "ms";
        EXPECT_LT(actual_timeout, timeout_ms * 2)
            << "Timeout should not be excessive";
        GTEST_LOG_(INFO) << "Timeout detection: requested " << timeout_ms
                        << "ms, actual " << actual_timeout << "ms";
    }

    // Scenario 3: Logging with timestamps pattern
    {
        struct LogEntry {
            uint64_t timestamp_ms;
            char formatted_time[256];
            const char* message;
        };

        std::vector<LogEntry> log_entries;

        for (int i = 0; i < 3; i++) {
            LogEntry entry;
            entry.timestamp_ms = nimcp_platform_time_monotonic_ms();
            entry.message = (i == 0) ? "Start" : (i == 1) ? "Processing" : "Complete";

            int result = nimcp_platform_time_to_string(
                entry.timestamp_ms,
                entry.formatted_time,
                sizeof(entry.formatted_time)
            );
            EXPECT_EQ(result, 0) << "Timestamp formatting should succeed";

            log_entries.push_back(entry);

            if (i < 2) {
                nimcp_platform_sleep_ms(20);
            }
        }

        // Verify log entries are in order
        for (size_t i = 1; i < log_entries.size(); i++) {
            EXPECT_GT(log_entries[i].timestamp_ms, log_entries[i-1].timestamp_ms)
                << "Log entry " << i << " should have later timestamp";

            GTEST_LOG_(INFO) << "Log[" << i << "]: "
                            << log_entries[i].message << " at "
                            << log_entries[i].formatted_time;
        }
    }

    // Scenario 4: Rate limiting pattern
    {
        const uint64_t min_interval_ms = 15;
        uint64_t last_operation_time = 0;
        int operations_performed = 0;

        uint64_t rate_start = nimcp_platform_time_monotonic_ms();
        const uint64_t test_duration_ms = 100;

        while ((nimcp_platform_time_monotonic_ms() - rate_start) < test_duration_ms) {
            uint64_t now = nimcp_platform_time_monotonic_ms();

            if (last_operation_time == 0 ||
                (now - last_operation_time) >= min_interval_ms) {
                // Perform rate-limited operation
                operations_performed++;
                last_operation_time = now;
            }

            nimcp_platform_sleep_ms(5);
        }

        // Verify rate limiting worked
        int expected_max_ops = (test_duration_ms / min_interval_ms) + 2;  // +2 for tolerance
        EXPECT_LE(operations_performed, expected_max_ops)
            << "Rate limiting should restrict operations";
        EXPECT_GT(operations_performed, 0)
            << "Should perform at least some operations";

        GTEST_LOG_(INFO) << "Rate limiting: performed " << operations_performed
                        << " operations in " << test_duration_ms
                        << "ms (limit: 1 per " << min_interval_ms << "ms)";
    }
}

//=============================================================================
// Bonus Test: Cross-Platform Consistency
//=============================================================================

/**
 * WHAT: Verify consistent behavior across platforms
 * WHY:  Ensure portable code works identically everywhere
 * HOW:  Test timing patterns that should be consistent
 *
 * COVERAGE: Cross-platform behavior
 */
TEST_F(UtilsPlatformTimeTest, CrossPlatformConsistency) {
    // Test that basic operations work consistently
    uint64_t time1 = nimcp_platform_time_monotonic_ms();
    EXPECT_GT(time1, 0) << "Time should be positive on all platforms";

    nimcp_platform_sleep_ms(10);

    uint64_t time2 = nimcp_platform_time_monotonic_ms();
    uint64_t diff = time2 - time1;

    EXPECT_GE(diff, 10) << "10ms sleep should work on all platforms";
    EXPECT_LT(diff, 100) << "10ms sleep should not vary wildly across platforms";

    // Test formatting works consistently
    char buffer[256];
    int result = nimcp_platform_time_to_string(12345, buffer, sizeof(buffer));
    EXPECT_EQ(result, 0) << "Formatting should work on all platforms";
    EXPECT_NE(buffer[0], '\0') << "Format should produce output on all platforms";

    // Test that initialization is truly idempotent
    for (int i = 0; i < 10; i++) {
        int init_result = nimcp_platform_time_init();
        EXPECT_EQ(init_result, 0) << "Init should succeed repeatedly on all platforms";
    }

    GTEST_LOG_(INFO) << "Cross-platform consistency tests passed";
}
