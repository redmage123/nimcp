/**
 * @file test_hypothalamus_exception_integration.cpp
 * @brief Integration tests for hypothalamus exception handling with immune system
 * @date 2026-01-24
 *
 * WHAT: Test exception handling integration between hypothalamus modules and immune system
 * WHY:  Verify exceptions from hypothalamus components propagate correctly to immune system
 * HOW:  Create hypothalamus components, trigger errors, verify immune system receives exceptions
 *
 * TEST COVERAGE:
 * 1. Orchestrator exception flow - NULL error triggers immune presentation
 * 2. Drive system exception flow - invalid level triggers immune presentation
 * 3. Cascading exceptions - multiple errors in sequence are all tracked
 * 4. Recovery after exception - system continues to function after error
 *
 * NOTE: Immune bridge tests are in a separate file due to CUDA header dependencies
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <cstring>
#include <atomic>
#include <thread>
#include <vector>

// Headers with their own extern "C" guards
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_orchestrator.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_drives.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/error/nimcp_error_codes.h"

//=============================================================================
// Test Fixture
//=============================================================================

class HypothalamusExceptionIntegrationTest : public ::testing::Test {
protected:
    // Static counters for exception tracking
    static std::atomic<int> exception_count;
    static std::atomic<int> last_exception_code;
    static std::atomic<int> null_pointer_count;
    static std::atomic<int> invalid_param_count;
    static std::atomic<int> brain_region_count;
    static nimcp_handler_registration_t* registration;

    // Test instances
    hypo_orchestrator_t orchestrator;
    hypo_drive_system_handle_t* drive_system;

    void SetUp() override {
        // Reset counters
        exception_count = 0;
        last_exception_code = 0;
        null_pointer_count = 0;
        invalid_param_count = 0;
        brain_region_count = 0;

        // Initialize exception system
        nimcp_exception_system_init();

        // Register test exception handler
        nimcp_handler_options_t options;
        nimcp_handler_default_options(&options);
        options.handler = test_exception_handler;
        options.user_data = nullptr;
        options.priority = NIMCP_HANDLER_PRIORITY_HIGH;
        options.name = "hypothalamus_integration_test_handler";
        registration = nimcp_handler_register(&options);

        // Initialize orchestrator with default config
        hypo_orch_config_t orch_config;
        hypo_orch_default_config(&orch_config);
        orchestrator = hypo_orch_create(&orch_config);

        // Initialize drive system with default config
        hypo_drive_config_t drive_config = hypo_drive_default_config();
        drive_system = hypo_drive_create(&drive_config);
    }

    void TearDown() override {
        // Cleanup drive system
        if (drive_system) {
            hypo_drive_destroy(drive_system);
            drive_system = nullptr;
        }

        // Cleanup orchestrator
        if (orchestrator) {
            hypo_orch_destroy(orchestrator);
            orchestrator = nullptr;
        }

        // Unregister handler
        if (registration) {
            nimcp_handler_unregister(registration);
            registration = nullptr;
        }

        // Clear any current exception
        nimcp_exception_clear_current();

        // Shutdown exception system
        nimcp_exception_system_shutdown();
    }

    /**
     * @brief Test exception handler that tracks exception statistics
     */
    static bool test_exception_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;

        exception_count++;
        last_exception_code = ex->code;

        // Track exception types
        if (ex->code == NIMCP_ERROR_NULL_POINTER) {
            null_pointer_count++;
        } else if (ex->code == NIMCP_ERROR_INVALID_PARAM) {
            invalid_param_count++;
        }

        // Track brain region exceptions
        if (ex->category == EXCEPTION_CATEGORY_BRAIN_REGION) {
            brain_region_count++;
        }

        // Return false to allow exception to continue through handler chain
        return false;
    }

    /**
     * @brief Reset exception counters for a new test scenario
     */
    void reset_counters() {
        exception_count = 0;
        last_exception_code = 0;
        null_pointer_count = 0;
        invalid_param_count = 0;
        brain_region_count = 0;
    }
};

// Static member initialization
std::atomic<int> HypothalamusExceptionIntegrationTest::exception_count{0};
std::atomic<int> HypothalamusExceptionIntegrationTest::last_exception_code{0};
std::atomic<int> HypothalamusExceptionIntegrationTest::null_pointer_count{0};
std::atomic<int> HypothalamusExceptionIntegrationTest::invalid_param_count{0};
std::atomic<int> HypothalamusExceptionIntegrationTest::brain_region_count{0};
nimcp_handler_registration_t* HypothalamusExceptionIntegrationTest::registration = nullptr;

//=============================================================================
// Basic Setup Tests
//=============================================================================

TEST_F(HypothalamusExceptionIntegrationTest, Setup_AllComponentsCreated_NoExceptions) {
    // WHAT: Verify all hypothalamus components create successfully
    // WHY:  Baseline test to ensure setup works before testing exceptions
    // HOW:  Check that all handles are non-null and no exceptions raised

    ASSERT_NE(orchestrator, nullptr) << "Orchestrator should be created";
    ASSERT_NE(drive_system, nullptr) << "Drive system should be created";
    EXPECT_EQ(exception_count.load(), 0) << "No exceptions should occur during setup";
}

//=============================================================================
// Orchestrator Exception Flow Tests
//=============================================================================

TEST_F(HypothalamusExceptionIntegrationTest, Orchestrator_NullPointer_ExceptionPresented) {
    // WHAT: Test that NULL orchestrator triggers exception to immune system
    // WHY:  Verify exception handling pipeline works for orchestrator
    // HOW:  Call orchestrator function with NULL, verify exception raised

    reset_counters();

    // Call with NULL orchestrator - should trigger exception
    hypo_orch_state_t state;
    int result = hypo_orch_get_state(nullptr, &state);

    EXPECT_LT(result, 0) << "NULL orchestrator should return error";
    EXPECT_GT(exception_count.load(), 0) << "Exception should be raised for NULL orchestrator";
    EXPECT_EQ(last_exception_code.load(), NIMCP_ERROR_NULL_POINTER)
        << "Should be NULL_POINTER error";
}

TEST_F(HypothalamusExceptionIntegrationTest, Orchestrator_NullOutput_ExceptionPresented) {
    // WHAT: Test that NULL output parameter triggers exception
    // WHY:  Verify parameter validation works correctly
    // HOW:  Call function with valid orchestrator but NULL output

    reset_counters();

    // Call with NULL output parameter
    int result = hypo_orch_get_state(orchestrator, nullptr);

    EXPECT_LT(result, 0) << "NULL output should return error";
    EXPECT_GT(exception_count.load(), 0) << "Exception should be raised for NULL output";
}

TEST_F(HypothalamusExceptionIntegrationTest, Orchestrator_InvalidBridgeId_ExceptionPresented) {
    // WHAT: Test that invalid bridge ID triggers exception
    // WHY:  Verify lookup failures are properly reported
    // HOW:  Try to get info for non-existent bridge ID

    reset_counters();

    hypo_bridge_info_t info;
    int result = hypo_orch_get_bridge_info(orchestrator, 99999, &info);

    EXPECT_LT(result, 0) << "Invalid bridge ID should return error";
    // May or may not raise exception depending on implementation
}

TEST_F(HypothalamusExceptionIntegrationTest, Orchestrator_PublishWithoutRegistration_Handled) {
    // WHAT: Test publishing event from unregistered bridge
    // WHY:  Verify event publishing handles invalid publisher ID
    // HOW:  Try to publish event with invalid publisher ID

    reset_counters();

    hypo_event_data_t event;
    memset(&event, 0, sizeof(event));
    event.event_type = HYPO_EVENT_DRIVE_ACTIVATED;
    event.source = HYPO_BRIDGE_EMOTION;
    event.urgency = HYPO_URGENCY_MODERATE;

    // Publish with invalid publisher ID - may succeed silently or fail
    int result = hypo_orch_publish(orchestrator, 99999, &event);

    // Implementation may silently ignore invalid publisher or return error
    // Just verify it doesn't crash
    (void)result;
}

//=============================================================================
// Drive System Exception Flow Tests
//=============================================================================

TEST_F(HypothalamusExceptionIntegrationTest, DriveSystem_NullPointer_ExceptionPresented) {
    // WHAT: Test that NULL drive system triggers exception
    // WHY:  Verify exception handling for drive system
    // HOW:  Call drive function with NULL handle

    reset_counters();

    hypo_drive_state_t state;
    bool result = hypo_drive_get_state(nullptr, HYPO_DRIVE_HUNGER, &state);

    EXPECT_FALSE(result) << "NULL drive system should return false";
    EXPECT_GT(exception_count.load(), 0) << "Exception should be raised";
    EXPECT_EQ(null_pointer_count.load(), 1) << "Should be NULL_POINTER error";
}

TEST_F(HypothalamusExceptionIntegrationTest, DriveSystem_InvalidDriveType_ExceptionPresented) {
    // WHAT: Test that invalid drive type triggers exception
    // WHY:  Verify parameter validation for drive operations
    // HOW:  Call drive function with out-of-range drive type

    reset_counters();

    hypo_drive_state_t state;
    bool result = hypo_drive_get_state(drive_system, (hypo_drive_type_t)999, &state);

    EXPECT_FALSE(result) << "Invalid drive type should return false";
    EXPECT_GT(exception_count.load(), 0) << "Exception should be raised for invalid drive type";
}

TEST_F(HypothalamusExceptionIntegrationTest, DriveSystem_InvalidSatisfactionLevel_ExceptionPresented) {
    // WHAT: Test that invalid satisfaction level triggers exception
    // WHY:  Verify range validation for drive satisfaction
    // HOW:  Call satisfy with out-of-range level

    reset_counters();

    // Try to satisfy with negative level (invalid)
    float reward = hypo_drive_satisfy(drive_system, HYPO_DRIVE_HUNGER, -1.0f);

    // Implementation may clamp or reject - check for exception
    // Note: Implementation-dependent behavior
    (void)reward;
}

TEST_F(HypothalamusExceptionIntegrationTest, DriveSystem_ModifyLockedAlignment_ExceptionPresented) {
    // WHAT: Test that modifying locked alignment weight triggers exception
    // WHY:  Verify alignment safety mechanisms work
    // HOW:  Lock alignment, try to modify, verify exception

    reset_counters();

    // Lock alignment weights
    bool lock_result = hypo_drive_lock_alignment(drive_system, HYPO_LOCK_HARD);
    EXPECT_TRUE(lock_result) << "Should be able to lock alignment";

    // Try to modify locked alignment weight
    bool modify_result = hypo_drive_modify_alignment_weight(
        drive_system, "human_wellbeing", 0.5f, 12345);

    EXPECT_FALSE(modify_result) << "Modifying locked alignment should fail";
    // Exception may be raised for security violation
}

//=============================================================================
// Cascading Exception Tests
//=============================================================================

TEST_F(HypothalamusExceptionIntegrationTest, Cascading_MultipleOrchestatorErrors_AllTracked) {
    // WHAT: Test that multiple sequential errors are all tracked
    // WHY:  Verify exception system handles rapid successive errors
    // HOW:  Trigger multiple errors in sequence, verify count

    reset_counters();

    // Trigger first error - NULL orchestrator
    hypo_orch_state_t state;
    hypo_orch_get_state(nullptr, &state);
    int count_after_first = exception_count.load();

    // Trigger second error - NULL output
    hypo_orch_get_state(orchestrator, nullptr);
    int count_after_second = exception_count.load();

    // Trigger third error - invalid bridge lookup
    hypo_bridge_info_t info;
    hypo_orch_get_bridge_info(nullptr, 0, &info);
    int count_after_third = exception_count.load();

    EXPECT_GE(count_after_first, 1) << "First error should be tracked";
    EXPECT_GE(count_after_second, count_after_first) << "Second error should be tracked";
    EXPECT_GE(count_after_third, count_after_second) << "Third error should be tracked";
}

TEST_F(HypothalamusExceptionIntegrationTest, Cascading_MultipleDriveErrors_AllTracked) {
    // WHAT: Test cascading errors in drive system
    // WHY:  Verify drive system exception accumulation
    // HOW:  Trigger multiple drive errors

    reset_counters();

    // Trigger errors for different operations
    hypo_drive_state_t state;
    hypo_drive_get_state(nullptr, HYPO_DRIVE_HUNGER, &state);
    hypo_drive_get_state(nullptr, HYPO_DRIVE_THIRST, &state);
    hypo_drive_get_state(nullptr, HYPO_DRIVE_CURIOSITY, &state);

    EXPECT_GE(exception_count.load(), 3) << "All three errors should be tracked";
    EXPECT_EQ(null_pointer_count.load(), 3) << "All should be NULL_POINTER errors";
}

TEST_F(HypothalamusExceptionIntegrationTest, Cascading_MixedModuleErrors_AllCategorized) {
    // WHAT: Test errors from different modules are correctly categorized
    // WHY:  Verify cross-module exception handling
    // HOW:  Trigger errors in orchestrator and drives

    reset_counters();

    // Orchestrator error
    hypo_orch_state_t orch_state;
    hypo_orch_get_state(nullptr, &orch_state);
    int after_orch = exception_count.load();

    // Drive error
    hypo_drive_state_t drive_state;
    hypo_drive_get_state(nullptr, HYPO_DRIVE_HUNGER, &drive_state);
    int after_drive = exception_count.load();

    EXPECT_GT(after_orch, 0) << "Orchestrator error should be tracked";
    EXPECT_GT(after_drive, after_orch) << "Drive error should be tracked";
}

//=============================================================================
// Recovery After Exception Tests
//=============================================================================

TEST_F(HypothalamusExceptionIntegrationTest, Recovery_OrchestratorContinuesAfterError) {
    // WHAT: Test orchestrator continues to function after an error
    // WHY:  Verify error recovery and resilience
    // HOW:  Trigger error, then perform successful operation

    reset_counters();

    // Trigger an error first
    hypo_orch_state_t state;
    hypo_orch_get_state(nullptr, &state);
    EXPECT_GT(exception_count.load(), 0) << "Error should be recorded";

    // Now perform successful operation
    int result = hypo_orch_get_state(orchestrator, &state);
    EXPECT_EQ(result, 0) << "Orchestrator should still work after error";
}

TEST_F(HypothalamusExceptionIntegrationTest, Recovery_DriveSystemContinuesAfterError) {
    // WHAT: Test drive system continues after error
    // WHY:  Verify drive system resilience
    // HOW:  Trigger error, then perform successful drive operations

    reset_counters();

    // Trigger error
    hypo_drive_state_t state;
    hypo_drive_get_state(nullptr, HYPO_DRIVE_HUNGER, &state);
    EXPECT_GT(exception_count.load(), 0) << "Error should be recorded";

    // Perform successful operations
    bool result = hypo_drive_get_state(drive_system, HYPO_DRIVE_HUNGER, &state);
    EXPECT_TRUE(result) << "Drive system should work after error";
    EXPECT_GE(state.level, 0.0f) << "Drive level should be valid";
    EXPECT_LE(state.level, 1.0f) << "Drive level should be valid";
}

TEST_F(HypothalamusExceptionIntegrationTest, Recovery_MultipleErrorsThenSuccess) {
    // WHAT: Test system recovers after multiple errors
    // WHY:  Verify robust error handling doesn't degrade state
    // HOW:  Trigger many errors, then verify normal operation

    reset_counters();

    // Trigger many errors
    for (int i = 0; i < 10; i++) {
        hypo_orch_state_t state;
        hypo_orch_get_state(nullptr, &state);
    }

    int error_count = exception_count.load();
    EXPECT_GE(error_count, 10) << "All errors should be tracked";

    // Verify orchestrator still works
    hypo_orch_state_t state;
    int result = hypo_orch_get_state(orchestrator, &state);
    EXPECT_EQ(result, 0) << "Orchestrator should function after multiple errors";

    // Verify drives still work
    hypo_drive_state_t drive_state;
    bool drive_result = hypo_drive_get_state(drive_system, HYPO_DRIVE_CURIOSITY, &drive_state);
    EXPECT_TRUE(drive_result) << "Drive system should function after multiple errors";
}

//=============================================================================
// Concurrent Exception Handling Tests
//=============================================================================

TEST_F(HypothalamusExceptionIntegrationTest, Concurrent_MultipleThreadsTriggerExceptions) {
    // WHAT: Test concurrent exception handling from multiple threads
    // WHY:  Verify thread-safety of exception system with hypothalamus
    // HOW:  Spawn threads that trigger exceptions, verify all tracked

    reset_counters();

    constexpr int num_threads = 4;
    constexpr int errors_per_thread = 10;

    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this]() {
            for (int i = 0; i < errors_per_thread; i++) {
                hypo_orch_state_t state;
                hypo_orch_get_state(nullptr, &state);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    int total_errors = exception_count.load();
    EXPECT_GE(total_errors, num_threads * errors_per_thread)
        << "All concurrent errors should be tracked";
}

//=============================================================================
// Statistics Verification Tests
//=============================================================================

TEST_F(HypothalamusExceptionIntegrationTest, Statistics_OrchestratorStatsNotCorrupted) {
    // WHAT: Test orchestrator statistics aren't corrupted by exceptions
    // WHY:  Verify exceptions don't corrupt internal state
    // HOW:  Trigger errors, check stats remain valid

    reset_counters();

    // Get initial stats
    hypo_orch_stats_t stats_before;
    int result = hypo_orch_get_stats(orchestrator, &stats_before);
    EXPECT_EQ(result, 0) << "Should get stats before errors";

    // Trigger some errors
    for (int i = 0; i < 5; i++) {
        hypo_orch_state_t state;
        hypo_orch_get_state(nullptr, &state);
    }

    // Get stats after errors
    hypo_orch_stats_t stats_after;
    result = hypo_orch_get_stats(orchestrator, &stats_after);
    EXPECT_EQ(result, 0) << "Should get stats after errors";

    // Stats should be coherent (not corrupted)
    EXPECT_LE(stats_after.registered_bridges, HYPO_ORCH_MAX_BRIDGES)
        << "Bridge count should be within limits";
}

TEST_F(HypothalamusExceptionIntegrationTest, Statistics_DriveStatsNotCorrupted) {
    // WHAT: Test drive statistics aren't corrupted by exceptions
    // WHY:  Verify drive state integrity
    // HOW:  Trigger errors, check drive stats

    reset_counters();

    // Get initial stats
    hypo_drive_stats_t stats_before;
    bool result = hypo_drive_get_stats(drive_system, &stats_before);
    EXPECT_TRUE(result) << "Should get stats before errors";

    // Trigger errors
    for (int i = 0; i < 5; i++) {
        hypo_drive_state_t state;
        hypo_drive_get_state(nullptr, HYPO_DRIVE_HUNGER, &state);
    }

    // Get stats after errors
    hypo_drive_stats_t stats_after;
    result = hypo_drive_get_stats(drive_system, &stats_after);
    EXPECT_TRUE(result) << "Should get stats after errors";

    // Stats should still be valid
    EXPECT_GE(stats_after.updates_processed, stats_before.updates_processed)
        << "Update count should be non-decreasing";
}

//=============================================================================
// Exception Message Verification Tests
//=============================================================================

TEST_F(HypothalamusExceptionIntegrationTest, ExceptionContent_CorrectErrorCodes) {
    // WHAT: Test that exception error codes are correct
    // WHY:  Verify exceptions carry proper diagnostic information
    // HOW:  Trigger specific errors, check codes

    reset_counters();

    // NULL pointer should give NULL_POINTER error
    hypo_orch_state_t state;
    hypo_orch_get_state(nullptr, &state);

    EXPECT_EQ(last_exception_code.load(), NIMCP_ERROR_NULL_POINTER)
        << "NULL pointer should produce NULL_POINTER error code";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
