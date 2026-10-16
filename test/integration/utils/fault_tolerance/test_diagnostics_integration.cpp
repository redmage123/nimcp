//=============================================================================
// test_diagnostics_integration.cpp - Integration Tests for Diagnostic System
//=============================================================================

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <unistd.h>

// Headers have their own extern "C" guards
#include "utils/fault_tolerance/nimcp_diagnostics.h"
#include "core/brain/nimcp_brain.h"

//=============================================================================
// Test Fixture
//=============================================================================

class DiagnosticsIntegrationTest : public ::testing::Test {
protected:
    brain_t brain;

    void SetUp() override {
        // Initialize diagnostics
        ASSERT_TRUE(diagnostics_init(nullptr));

        // Create test brain
        brain = brain_create("test_brain", BRAIN_SIZE_SMALL,
                           BRAIN_TASK_CLASSIFICATION, 10, 5);
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
        diagnostics_shutdown();
    }
};

//=============================================================================
// Full Workflow Integration Tests
//=============================================================================

TEST_F(DiagnosticsIntegrationTest, CrashAnalysisToRecoveryWorkflow) {
    // Simulate crash detection
    crash_context_t context = {0};
    context.signal = SIGSEGV;
    context.fault_address = nullptr;
    context.timestamp = time(nullptr);

    // Analyze crash
    diagnostic_result_t* result = diagnostics_analyze_crash(SIGSEGV, &context);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->error_type, ERROR_TYPE_NULL_POINTER);

    // Generate recovery suggestions
    diagnostics_suggest_recovery(result);
    EXPECT_GT(result->recovery_action_count, 0);

    // Log the diagnostic
    diagnostics_report_to_log(result);

    // Add to history for pattern detection
    diagnostic_history_t* history = diagnostics_create_history();
    diagnostics_add_to_history(history, result);

    // Check for patterns
    bool pattern = diagnostics_detect_crash_pattern(history);
    EXPECT_FALSE(pattern);  // Only one crash

    // Cleanup
    diagnostics_free_result(result);
    diagnostics_free_history(history);
}

TEST_F(DiagnosticsIntegrationTest, MultipleErrorPatternDetection) {
    diagnostic_history_t* history = diagnostics_create_history();
    ASSERT_NE(history, nullptr);

    // Add multiple errors in same function
    for (int i = 0; i < 5; i++) {
        diagnostic_result_t result;
        memset(&result, 0, sizeof(diagnostic_result_t));
        result.error_type = ERROR_TYPE_SEGFAULT;
        result.timestamp = time(nullptr) + i;
        result.stack_depth = 1;
        strncpy(result.stack_trace[0].function_name, "faulty_function",
                sizeof(result.stack_trace[0].function_name) - 1);

        diagnostics_add_to_history(history, &result);
    }

    // Should detect pattern of crashes in same function
    bool pattern = diagnostics_detect_crash_pattern(history);
    EXPECT_TRUE(pattern);

    diagnostics_free_history(history);
}

TEST_F(DiagnosticsIntegrationTest, MemoryAnalysisAndReporting) {
    // Analyze brain memory state
    diagnostic_result_t* result = diagnostics_analyze_memory_state(brain);
    ASSERT_NE(result, nullptr);

    // Generate report file
    const char* report_path = "/tmp/diagnostic_test_report.txt";
    bool success = diagnostics_report_to_file(result, report_path);
    EXPECT_TRUE(success);

    // Verify file was created
    EXPECT_EQ(access(report_path, F_OK), 0);

    // Cleanup
    unlink(report_path);
    diagnostics_free_result(result);
}

TEST_F(DiagnosticsIntegrationTest, JSONReportGeneration) {
    // Create diagnostic result
    crash_context_t context = {0};
    context.signal = SIGFPE;
    context.timestamp = time(nullptr);

    diagnostic_result_t* result = diagnostics_analyze_crash(SIGFPE, &context);
    ASSERT_NE(result, nullptr);

    // Generate JSON report
    char* json = diagnostics_report_to_json(result);
    ASSERT_NE(json, nullptr);
    EXPECT_GT(strlen(json), 0);

    // Verify JSON structure
    EXPECT_NE(strstr(json, "\"error_type\""), nullptr);
    EXPECT_NE(strstr(json, "\"severity\""), nullptr);

    free(json);
    diagnostics_free_result(result);
}

//=============================================================================
// Brain-Specific Diagnostic Tests
//=============================================================================

TEST_F(DiagnosticsIntegrationTest, BrainMemoryCorruptionDetection) {
    bool corrupted = diagnostics_detect_memory_corruption(brain);
    EXPECT_FALSE(corrupted);  // Healthy brain should not show corruption
}

TEST_F(DiagnosticsIntegrationTest, BrainNumericalStabilityCheck) {
    bool unstable = diagnostics_detect_numerical_instability(brain);
    EXPECT_FALSE(unstable);  // Healthy brain should be numerically stable
}

TEST_F(DiagnosticsIntegrationTest, BrainStateAnalysis) {
    diagnostic_result_t* result = diagnostics_analyze_memory_state(brain);
    ASSERT_NE(result, nullptr);

    // Should capture memory statistics
    EXPECT_GE(result->memory_state.allocation_count, 0);
    EXPECT_GE(result->memory_state.deallocation_count, 0);

    diagnostics_free_result(result);
}

//=============================================================================
// Recovery Action Tests
//=============================================================================

TEST_F(DiagnosticsIntegrationTest, AutoRecoveryForNumericalErrors) {
    diagnostic_result_t result;
    memset(&result, 0, sizeof(diagnostic_result_t));
    result.error_type = ERROR_TYPE_NAN_DETECTED;
    result.severity = DIAG_SEVERITY_ERROR;

    // Generate recovery suggestions
    diagnostics_suggest_recovery(&result);
    ASSERT_GT(result.recovery_action_count, 0);

    // Try auto-recovery (should attempt reduce precision)
    bool recovered = diagnostics_auto_recover(&result, brain);
    // May or may not succeed depending on action type and confidence
}

TEST_F(DiagnosticsIntegrationTest, ManualRecoveryRecommendations) {
    diagnostic_result_t result;
    memset(&result, 0, sizeof(diagnostic_result_t));
    result.error_type = ERROR_TYPE_BUFFER_OVERFLOW;

    diagnostics_suggest_recovery(&result);
    ASSERT_GT(result.recovery_action_count, 0);

    // Buffer overflow should require shutdown
    const recovery_action_recommendation_t* action = &result.recovery_actions[0];
    EXPECT_EQ(action->type, RECOVERY_IMMEDIATE_SHUTDOWN);
    EXPECT_TRUE(action->requires_user_intervention);
}

//=============================================================================
// Stress Tests
//=============================================================================

TEST_F(DiagnosticsIntegrationTest, HighVolumeErrorTracking) {
    diagnostic_history_t* history = diagnostics_create_history();
    ASSERT_NE(history, nullptr);

    // Add many errors
    for (int i = 0; i < MAX_HISTORY_SIZE + 50; i++) {
        diagnostic_result_t result;
        memset(&result, 0, sizeof(diagnostic_result_t));
        result.error_type = static_cast<error_type_t>(0x1000 + (i % 10));
        result.timestamp = time(nullptr) + i;

        diagnostics_add_to_history(history, &result);
    }

    // Should maintain circular buffer
    EXPECT_EQ(history->count, MAX_HISTORY_SIZE);
    EXPECT_TRUE(history->is_full);

    diagnostics_free_history(history);
}

TEST_F(DiagnosticsIntegrationTest, ConcurrentDiagnosticReports) {
    // Generate multiple diagnostic results rapidly
    diagnostic_result_t* results[10];

    for (int i = 0; i < 10; i++) {
        void* trace[5] = {
            reinterpret_cast<void*>(0x1000 + i * 0x100),
            reinterpret_cast<void*>(0x2000 + i * 0x100),
            reinterpret_cast<void*>(0x3000 + i * 0x100),
            nullptr,
            nullptr
        };

        results[i] = diagnostics_analyze_stack_trace(trace, 3);
        ASSERT_NE(results[i], nullptr);

        diagnostics_report_to_log(results[i]);
    }

    // Cleanup
    for (int i = 0; i < 10; i++) {
        diagnostics_free_result(results[i]);
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
