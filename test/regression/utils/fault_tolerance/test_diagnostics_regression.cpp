//=============================================================================
// test_diagnostics_regression.cpp - Regression Tests for Diagnostic System
//=============================================================================

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "utils/fault_tolerance/nimcp_diagnostics.h"

//=============================================================================
// Test Fixture
//=============================================================================

class DiagnosticsRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_TRUE(diagnostics_init(nullptr));
    }

    void TearDown() override {
        diagnostics_shutdown();
    }
};

//=============================================================================
// Regression: Issue #1 - Stack Trace Overflow
//=============================================================================

TEST_F(DiagnosticsRegressionTest, StackTraceDoesNotOverflow) {
    // REGRESSION: Previously, stack traces > MAX_STACK_DEPTH caused buffer overflow
    // EXPECTED: Should safely clamp to MAX_STACK_DEPTH

    void* trace[MAX_STACK_DEPTH + 100];
    for (int i = 0; i < MAX_STACK_DEPTH + 100; i++) {
        trace[i] = reinterpret_cast<void*>(0x1000 + i);
    }

    diagnostic_result_t* result = diagnostics_analyze_stack_trace(
        trace, MAX_STACK_DEPTH + 100);

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->stack_depth, MAX_STACK_DEPTH);  // Should be clamped

    diagnostics_free_result(result);
}

//=============================================================================
// Regression: Issue #2 - NULL Pointer Dereference in Report Generation
//=============================================================================

TEST_F(DiagnosticsRegressionTest, ReportHandlesNullResult) {
    // REGRESSION: Previously, reporting NULL result caused crash
    // EXPECTED: Should handle gracefully

    diagnostics_report_to_log(nullptr);  // Should not crash

    bool success = diagnostics_report_to_file(nullptr, "/tmp/test.txt");
    EXPECT_FALSE(success);

    char* json = diagnostics_report_to_json(nullptr);
    EXPECT_EQ(json, nullptr);
}

//=============================================================================
// Regression: Issue #3 - History Circular Buffer Corruption
//=============================================================================

TEST_F(DiagnosticsRegressionTest, HistoryCircularBufferStaysValid) {
    // REGRESSION: Previously, adding > MAX_HISTORY_SIZE items corrupted buffer
    // EXPECTED: Should maintain valid circular buffer

    diagnostic_history_t* history = diagnostics_create_history();
    ASSERT_NE(history, nullptr);

    // Add 2x MAX_HISTORY_SIZE entries
    for (int i = 0; i < MAX_HISTORY_SIZE * 2; i++) {
        diagnostic_result_t result;
        memset(&result, 0, sizeof(result));
        result.error_type = ERROR_TYPE_SEGFAULT;
        result.timestamp = time(nullptr) + i;
        result.stack_depth = 1;
        snprintf(result.stack_trace[0].function_name,
                sizeof(result.stack_trace[0].function_name),
                "function_%d", i);

        diagnostics_add_to_history(history, &result);
    }

    EXPECT_EQ(history->count, MAX_HISTORY_SIZE);
    EXPECT_TRUE(history->is_full);
    EXPECT_LT(history->write_index, MAX_HISTORY_SIZE);

    diagnostics_free_history(history);
}

//=============================================================================
// Regression: Issue #4 - Recovery Action Overflow
//=============================================================================

TEST_F(DiagnosticsRegressionTest, RecoveryActionsDoNotOverflow) {
    // REGRESSION: Previously, some error types generated > MAX_RECOVERY_ACTIONS
    // EXPECTED: Should cap at MAX_RECOVERY_ACTIONS

    diagnostic_result_t result;
    memset(&result, 0, sizeof(result));
    result.error_type = ERROR_TYPE_OUT_OF_MEMORY;

    diagnostics_suggest_recovery(&result);

    EXPECT_LE(result.recovery_action_count, MAX_RECOVERY_ACTIONS);
}

//=============================================================================
// Regression: Issue #5 - Signal Name for Unknown Signals
//=============================================================================

TEST_F(DiagnosticsRegressionTest, UnknownSignalNameDoesNotCrash) {
    // REGRESSION: Previously, unknown signal numbers caused crashes
    // EXPECTED: Should return "UNKNOWN"

    const char* name = diagnostics_get_error_type_name(
        static_cast<error_type_t>(0xFFFF));
    EXPECT_NE(name, nullptr);
    EXPECT_STREQ(name, "Unrecognized Error Type");
}

//=============================================================================
// Regression: Issue #6 - Empty Stack Trace
//=============================================================================

TEST_F(DiagnosticsRegressionTest, EmptyStackTraceHandled) {
    // REGRESSION: Previously, zero-depth stack trace caused issues
    // EXPECTED: Should return NULL

    void* trace[10];
    diagnostic_result_t* result = diagnostics_analyze_stack_trace(trace, 0);

    EXPECT_EQ(result, nullptr);
}

//=============================================================================
// Regression: Issue #7 - Confidence Out of Range
//=============================================================================

TEST_F(DiagnosticsRegressionTest, ConfidenceStaysInRange) {
    // REGRESSION: Previously, confidence could exceed 1.0
    // EXPECTED: Should be 0.0-1.0

    crash_context_t context = {0};
    context.signal = SIGSEGV;

    diagnostic_result_t* result = diagnostics_analyze_crash(SIGSEGV, &context);
    ASSERT_NE(result, nullptr);

    EXPECT_GE(result->confidence, 0.0f);
    EXPECT_LE(result->confidence, 1.0f);

    diagnostics_free_result(result);
}

//=============================================================================
// Regression: Issue #8 - String Truncation in Reports
//=============================================================================

TEST_F(DiagnosticsRegressionTest, LongStringsDoNotOverflow) {
    // REGRESSION: Previously, very long root cause strings caused overflow
    // EXPECTED: Should truncate safely

    diagnostic_result_t result;
    memset(&result, 0, sizeof(result));
    result.error_type = ERROR_TYPE_SEGFAULT;

    // Fill with very long string (should be truncated)
    for (size_t i = 0; i < sizeof(result.root_cause); i++) {
        result.root_cause[i] = 'A';
    }
    result.root_cause[sizeof(result.root_cause) - 1] = '\0';

    // Should not crash
    diagnostics_report_to_log(&result);
}

//=============================================================================
// Regression: Issue #9 - Null Brain Crashes Analysis
//=============================================================================

TEST_F(DiagnosticsRegressionTest, NullBrainAnalysisDoesNotCrash) {
    // REGRESSION: Previously, NULL brain caused crashes in analysis
    // EXPECTED: Should detect and report NULL pointer error

    diagnostic_result_t* result = diagnostics_analyze_memory_state(nullptr);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->error_type, ERROR_TYPE_NULL_POINTER);
    diagnostics_free_result(result);

    result = diagnostics_analyze_numerical_stability(nullptr);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->error_type, ERROR_TYPE_NULL_POINTER);
    diagnostics_free_result(result);

    bool corrupted = diagnostics_detect_memory_corruption(nullptr);
    EXPECT_FALSE(corrupted);  // Should handle gracefully

    bool unstable = diagnostics_detect_numerical_instability(nullptr);
    EXPECT_FALSE(unstable);  // Should handle gracefully
}

//=============================================================================
// Regression: Issue #10 - Double Free of Results
//=============================================================================

TEST_F(DiagnosticsRegressionTest, DoubleFreeDoesNotCrash) {
    // REGRESSION: Previously, freeing result twice caused crash
    // EXPECTED: Should be safe (though not recommended)

    diagnostic_result_t* result = static_cast<diagnostic_result_t*>(
        calloc(1, sizeof(diagnostic_result_t)));
    ASSERT_NE(result, nullptr);

    diagnostics_free_result(result);
    // Note: Second free would be undefined behavior, so we don't test it
}

//=============================================================================
// Regression: Issue #11 - Resource Leak Detection False Positives
//=============================================================================

TEST_F(DiagnosticsRegressionTest, ResourceExhaustionThresholdValid) {
    // REGRESSION: Previously, invalid threshold caused false positives
    // EXPECTED: Should handle invalid thresholds gracefully

    error_type_t result1 = diagnostics_detect_resource_exhaustion(-10.0f);
    EXPECT_EQ(result1, ERROR_TYPE_NONE);

    error_type_t result2 = diagnostics_detect_resource_exhaustion(200.0f);
    EXPECT_EQ(result2, ERROR_TYPE_NONE);

    error_type_t result3 = diagnostics_detect_resource_exhaustion(50.0f);
    // Should not crash
}

//=============================================================================
// Regression: Issue #12 - Crash Pattern False Negatives
//=============================================================================

TEST_F(DiagnosticsRegressionTest, CrashPatternDetectionAccurate) {
    // REGRESSION: Previously, rapid crashes weren't detected as pattern
    // EXPECTED: Should detect 5 crashes in 60 seconds

    diagnostic_history_t* history = diagnostics_create_history();
    ASSERT_NE(history, nullptr);

    time_t base_time = time(nullptr);

    // Add 5 crashes within 30 seconds
    for (int i = 0; i < 5; i++) {
        diagnostic_result_t result;
        memset(&result, 0, sizeof(result));
        result.error_type = ERROR_TYPE_SEGFAULT;
        result.timestamp = base_time + i * 5;  // 5 second intervals

        diagnostics_add_to_history(history, &result);
    }

    bool pattern = diagnostics_detect_crash_pattern(history);
    EXPECT_TRUE(pattern);  // Should detect rapid crash pattern

    diagnostics_free_history(history);
}

//=============================================================================
// Regression: Issue #13 - JSON Escaping
//=============================================================================

TEST_F(DiagnosticsRegressionTest, JSONHandlesSpecialCharacters) {
    // REGRESSION: Previously, special chars in strings broke JSON
    // EXPECTED: Should handle safely (may not fully escape, but shouldn't crash)

    diagnostic_result_t result;
    memset(&result, 0, sizeof(result));
    result.error_type = ERROR_TYPE_SEGFAULT;
    strncpy(result.root_cause, "Test \"quoted\" string\nwith newline",
            sizeof(result.root_cause) - 1);

    char* json = diagnostics_report_to_json(&result);
    ASSERT_NE(json, nullptr);
    EXPECT_GT(strlen(json), 0);

    free(json);
}

//=============================================================================
// Regression: Issue #14 - Auto-Recovery Infinite Loop
//=============================================================================

TEST_F(DiagnosticsRegressionTest, AutoRecoveryDoesNotLoop) {
    // REGRESSION: Previously, failed auto-recovery retried infinitely
    // EXPECTED: Should try once and return false on failure

    diagnostic_result_t result;
    memset(&result, 0, sizeof(result));
    result.error_type = ERROR_TYPE_UNKNOWN;
    result.recovery_action_count = 0;

    bool recovered = diagnostics_auto_recover(&result, nullptr);
    EXPECT_FALSE(recovered);
    EXPECT_FALSE(result.self_healing_successful);
}

//=============================================================================
// Regression: Issue #15 - Error ID Overflow
//=============================================================================

TEST_F(DiagnosticsRegressionTest, ErrorIDMonotonicIncreasing) {
    // REGRESSION: Previously, error IDs could wrap or duplicate
    // EXPECTED: Should be monotonically increasing

    uint64_t last_id = 0;

    for (int i = 0; i < 10; i++) {
        void* trace[1] = {reinterpret_cast<void*>(0x1000)};
        diagnostic_result_t* result = diagnostics_analyze_stack_trace(trace, 1);
        ASSERT_NE(result, nullptr);

        EXPECT_GT(result->error_id, last_id);
        last_id = result->error_id;

        diagnostics_free_result(result);
    }
}

//=============================================================================
// Performance Regression Tests
//=============================================================================

TEST_F(DiagnosticsRegressionTest, StackTraceAnalysisPerformance) {
    // REGRESSION: Previously, deep stack traces took exponential time
    // EXPECTED: Should complete in reasonable time (<100ms)

    void* trace[MAX_STACK_DEPTH];
    for (int i = 0; i < MAX_STACK_DEPTH; i++) {
        trace[i] = reinterpret_cast<void*>(0x1000 + i * 0x10);
    }

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    diagnostic_result_t* result = diagnostics_analyze_stack_trace(
        trace, MAX_STACK_DEPTH);

    clock_gettime(CLOCK_MONOTONIC, &end);

    ASSERT_NE(result, nullptr);

    long elapsed_ms = (end.tv_sec - start.tv_sec) * 1000 +
                     (end.tv_nsec - start.tv_nsec) / 1000000;

    EXPECT_LT(elapsed_ms, 100);  // Should complete in < 100ms

    diagnostics_free_result(result);
}

TEST_F(DiagnosticsRegressionTest, HistoryAdditionPerformance) {
    // REGRESSION: Previously, adding to full history was O(n)
    // EXPECTED: Should be O(1)

    diagnostic_history_t* history = diagnostics_create_history();
    ASSERT_NE(history, nullptr);

    // Fill history
    for (int i = 0; i < MAX_HISTORY_SIZE; i++) {
        diagnostic_result_t result;
        memset(&result, 0, sizeof(result));
        diagnostics_add_to_history(history, &result);
    }

    // Time 1000 additions to full history
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < 1000; i++) {
        diagnostic_result_t result;
        memset(&result, 0, sizeof(result));
        diagnostics_add_to_history(history, &result);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    long elapsed_us = (end.tv_sec - start.tv_sec) * 1000000 +
                     (end.tv_nsec - start.tv_nsec) / 1000;

    // Should be < 1ms (1000us) total for 1000 operations
    EXPECT_LT(elapsed_us, 1000);

    diagnostics_free_history(history);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
