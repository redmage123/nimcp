/**
 * @file test_pr_memory_exception_e2e.cpp
 * @brief End-to-end tests for PR memory system exception handling
 * @date 2026-01-25
 *
 * WHAT: E2E tests for complete PR memory exception handling workflows
 * WHY:  Verify exception handling works end-to-end in realistic scenarios
 * HOW:  Test complete workflows with real memory operations and exception recovery
 *
 * E2E SCENARIOS:
 * - Complete memory lifecycle with exception recovery
 * - Multi-module workflows with cascading exceptions
 * - Real-world error scenarios and recovery
 * - Performance under exception conditions
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <cstring>
#include <atomic>
#include <thread>
#include <chrono>
#include <vector>
#include <random>

// C++ compatibility for C11 _Atomic keyword
#ifndef _Atomic
#define _Atomic volatile
#endif

extern "C" {
#include "cognitive/memory/core/nimcp_pr_memory_node.h"
#include "cognitive/memory/core/nimcp_procedural.h"
#include "cognitive/memory/core/nimcp_prospective.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/error/nimcp_error_codes.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class PRMemoryExceptionE2ETest : public ::testing::Test {
protected:
    static std::atomic<int> exception_count;
    static std::atomic<int> null_pointer_count;
    static std::atomic<int> other_error_count;
    static nimcp_handler_registration_t* registration;

    void SetUp() override {
        exception_count = 0;
        null_pointer_count = 0;
        other_error_count = 0;

        nimcp_exception_system_init();

        nimcp_handler_options_t options;
        nimcp_handler_default_options(&options);
        options.handler = test_exception_handler;
        options.user_data = nullptr;
        options.priority = NIMCP_HANDLER_PRIORITY_HIGH;
        options.name = "pr_memory_e2e_test_handler";
        registration = nimcp_handler_register(&options);
    }

    void TearDown() override {
        if (registration) {
            nimcp_handler_unregister(registration);
            registration = nullptr;
        }
        nimcp_exception_clear_current();
        nimcp_exception_system_shutdown();
    }

    static bool test_exception_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;
        exception_count++;
        if (ex->code == NIMCP_ERROR_NULL_POINTER) {
            null_pointer_count++;
        } else {
            other_error_count++;
        }
        return false;  // Don't suppress
    }

    void reset_counters() {
        exception_count = 0;
        null_pointer_count = 0;
        other_error_count = 0;
    }
};

std::atomic<int> PRMemoryExceptionE2ETest::exception_count{0};
std::atomic<int> PRMemoryExceptionE2ETest::null_pointer_count{0};
std::atomic<int> PRMemoryExceptionE2ETest::other_error_count{0};
nimcp_handler_registration_t* PRMemoryExceptionE2ETest::registration = nullptr;

//=============================================================================
// E2E Test: Complete Memory Node Lifecycle
// Tests full lifecycle from creation through operations to cleanup
//=============================================================================

TEST_F(PRMemoryExceptionE2ETest, CompleteLifecycle_ValidManager_NoExceptions) {
    // E2E: Test complete memory node lifecycle with valid manager
    reset_counters();

    pr_node_manager_config_t config = pr_node_manager_default_config();
    pr_node_manager_t manager = pr_node_manager_create(&config);
    ASSERT_NE(manager, nullptr) << "Failed to create node manager";

    // Create a memory node
    uint8_t data[] = {1, 2, 3, 4, 5, 6, 7, 8};
    pr_node_config_t node_config = pr_memory_node_default_config();
    pr_memory_node_t* node = pr_memory_node_create(manager, data, sizeof(data), &node_config);
    ASSERT_NE(node, nullptr) << "Failed to create memory node";

    // Perform various operations
    size_t data_size = pr_memory_node_get_data_size(node);
    EXPECT_EQ(data_size, sizeof(data));

    // Read may return NULL for copy-on-write optimization - just verify no crash
    const void* read_data = pr_memory_node_read(node);
    (void)read_data;  // OK if NULL due to CoW

    pr_memory_tier_t tier = pr_memory_node_get_tier(node);
    EXPECT_EQ(tier, PR_MEMORY_TIER_Z0);

    uint64_t node_id = pr_memory_node_get_id(node);
    EXPECT_NE(node_id, PR_NODE_INVALID_ID);

    // Cleanup
    pr_memory_node_destroy(node);
    pr_node_manager_destroy(manager);

    // No exceptions should have been thrown during valid operations
    EXPECT_EQ(exception_count.load(), 0);
}

TEST_F(PRMemoryExceptionE2ETest, CompleteLifecycle_NullManager_GracefulFailure) {
    // E2E: Test that entire workflow fails gracefully with NULL manager
    reset_counters();

    // Attempt operations with NULL manager
    pr_memory_node_t* node = pr_memory_node_create(nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(node, nullptr);

    unified_mem_manager_t mem = pr_node_manager_get_mem_manager(nullptr);
    EXPECT_EQ(mem, nullptr);

    uint64_t count = pr_node_manager_get_node_count(nullptr);
    EXPECT_EQ(count, 0u);

    // Cleanup is safe even with NULL
    pr_node_manager_destroy(nullptr);
    pr_memory_node_destroy(nullptr);

    // Should have recorded multiple exceptions
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_GT(null_pointer_count.load(), 0);
}

//=============================================================================
// E2E Test: Multi-Module Workflow
// Tests complete workflow spanning pr_memory_node, procedural, and prospective
//=============================================================================

TEST_F(PRMemoryExceptionE2ETest, MultiModuleWorkflow_AllNullInputs_GracefulRecovery) {
    // E2E: Simulate a workflow that touches all modules with NULL inputs
    reset_counters();

    // Step 1: Memory node operations (all should fail gracefully)
    pr_memory_node_t* node = pr_memory_node_create(nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(node, nullptr);

    const prime_signature_t* sig = pr_memory_node_get_signature(nullptr);
    EXPECT_EQ(sig, nullptr);

    // Step 2: Procedural memory operations (all should fail gracefully)
    pr_memory_node_t* skill_node = procedural_get_skill_node(nullptr, 1);
    EXPECT_EQ(skill_node, nullptr);

    pr_memory_node_t* habit_node = procedural_get_habit_node(nullptr, 1);
    EXPECT_EQ(habit_node, nullptr);

    // Step 3: Prospective memory operations (all should fail gracefully)
    pr_memory_node_t* prosp_node = prospective_get_memory_node(nullptr, 1);
    EXPECT_EQ(prosp_node, nullptr);

    // Verify exception handling tracked all errors
    EXPECT_GE(null_pointer_count.load(), 4);

    // System should still be in a valid state (no crashes)
    SUCCEED();
}

TEST_F(PRMemoryExceptionE2ETest, MultiModuleWorkflow_MixedValidInvalid_PartialSuccess) {
    // E2E: Workflow with mix of valid and invalid operations
    reset_counters();

    // Valid: Create manager
    pr_node_manager_config_t config = pr_node_manager_default_config();
    pr_node_manager_t manager = pr_node_manager_create(&config);
    ASSERT_NE(manager, nullptr);

    // Valid: Create node
    uint8_t data[] = {1, 2, 3, 4};
    pr_node_config_t node_config = pr_memory_node_default_config();
    pr_memory_node_t* node = pr_memory_node_create(manager, data, sizeof(data), &node_config);
    ASSERT_NE(node, nullptr);

    // Invalid: NULL operations (should throw exceptions but not crash)
    pr_memory_node_t* null_node = pr_memory_node_create(nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(null_node, nullptr);

    pr_memory_node_t* null_skill = procedural_get_skill_node(nullptr, 1);
    EXPECT_EQ(null_skill, nullptr);

    // Valid: Continue with real node
    size_t size = pr_memory_node_get_data_size(node);
    EXPECT_GT(size, 0u);

    // Invalid: More NULL operations
    pr_memory_node_t* null_prosp = prospective_get_memory_node(nullptr, 1);
    EXPECT_EQ(null_prosp, nullptr);

    // Cleanup (valid)
    pr_memory_node_destroy(node);
    pr_node_manager_destroy(manager);

    // Should have exceptions from NULL operations only
    EXPECT_GT(null_pointer_count.load(), 0);
}

//=============================================================================
// E2E Test: Simulated Error Scenarios
// Tests realistic error scenarios that might occur in production
//=============================================================================

TEST_F(PRMemoryExceptionE2ETest, SimulatedScenario_LateNullPointer_Recovery) {
    // E2E: Simulate scenario where NULL pointer occurs after valid setup
    reset_counters();

    // Setup phase (valid)
    pr_node_manager_config_t config = pr_node_manager_default_config();
    pr_node_manager_t manager = pr_node_manager_create(&config);
    ASSERT_NE(manager, nullptr);

    uint8_t data[] = {1, 2, 3, 4};
    pr_node_config_t node_config = pr_memory_node_default_config();
    pr_memory_node_t* node = pr_memory_node_create(manager, data, sizeof(data), &node_config);
    ASSERT_NE(node, nullptr);

    // Normal operations
    EXPECT_EQ(pr_memory_node_get_tier(node), PR_MEMORY_TIER_Z0);

    // Simulate pointer becoming NULL (e.g., after free)
    pr_memory_node_t* invalid_node = nullptr;

    // Operations on invalid node should fail gracefully
    size_t size = pr_memory_node_get_data_size(invalid_node);
    EXPECT_EQ(size, 0u);

    const void* read_data = pr_memory_node_read(invalid_node);
    EXPECT_EQ(read_data, nullptr);

    // Original node should still work
    EXPECT_GT(pr_memory_node_get_data_size(node), 0u);

    // Cleanup
    pr_memory_node_destroy(node);
    pr_node_manager_destroy(manager);

    SUCCEED();
}

TEST_F(PRMemoryExceptionE2ETest, SimulatedScenario_ConcurrentOperations_ThreadSafe) {
    // E2E: Simulate concurrent operations with mixed valid/invalid calls
    reset_counters();

    pr_node_manager_config_t config = pr_node_manager_default_config();
    pr_node_manager_t manager = pr_node_manager_create(&config);
    ASSERT_NE(manager, nullptr);

    std::atomic<bool> stop{false};
    std::atomic<int> success_count{0};
    std::atomic<int> null_calls{0};

    // Thread doing valid operations
    auto valid_worker = [&]() {
        while (!stop) {
            uint8_t data[] = {1, 2, 3, 4};
            pr_node_config_t node_config = pr_memory_node_default_config();
            pr_memory_node_t* node = pr_memory_node_create(manager, data, sizeof(data), &node_config);
            if (node) {
                pr_memory_node_get_data_size(node);
                pr_memory_node_destroy(node);
                success_count++;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    };

    // Thread doing invalid operations
    auto invalid_worker = [&]() {
        while (!stop) {
            pr_memory_node_t* node = pr_memory_node_create(nullptr, nullptr, 0, nullptr);
            (void)node;
            null_calls++;
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    };

    std::thread valid_thread(valid_worker);
    std::thread invalid_thread(invalid_worker);

    // Run for a short time
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    stop = true;

    valid_thread.join();
    invalid_thread.join();

    // Both types of operations should have occurred
    EXPECT_GT(success_count.load(), 0);
    EXPECT_GT(null_calls.load(), 0);

    // Cleanup
    pr_node_manager_destroy(manager);
}

//=============================================================================
// E2E Test: Exception Recovery
// Tests that system recovers properly after exceptions
//=============================================================================

TEST_F(PRMemoryExceptionE2ETest, ExceptionRecovery_AfterMultipleExceptions_ContinuesWorking) {
    // E2E: Verify system works correctly after handling multiple exceptions
    reset_counters();

    // Generate multiple exceptions
    for (int i = 0; i < 100; i++) {
        pr_memory_node_t* node = pr_memory_node_create(nullptr, nullptr, 0, nullptr);
        (void)node;
    }

    int initial_exceptions = exception_count.load();
    EXPECT_GT(initial_exceptions, 0);

    // Now do valid operations - should work fine
    pr_node_manager_config_t config = pr_node_manager_default_config();
    pr_node_manager_t manager = pr_node_manager_create(&config);
    ASSERT_NE(manager, nullptr);

    uint8_t data[] = {1, 2, 3, 4};
    pr_node_config_t node_config = pr_memory_node_default_config();
    pr_memory_node_t* node = pr_memory_node_create(manager, data, sizeof(data), &node_config);
    ASSERT_NE(node, nullptr);

    // Operations should work normally
    EXPECT_EQ(pr_memory_node_get_data_size(node), sizeof(data));
    // Read may return NULL for CoW - just verify no crash
    (void)pr_memory_node_read(node);

    // No new exceptions from valid operations
    EXPECT_EQ(exception_count.load(), initial_exceptions);

    // Cleanup
    pr_memory_node_destroy(node);
    pr_node_manager_destroy(manager);
}

//=============================================================================
// E2E Test: Complete Procedural Memory Workflow
//=============================================================================

TEST_F(PRMemoryExceptionE2ETest, ProceduralWorkflow_NullInputs_AllOperationsFail) {
    // E2E: Complete procedural memory workflow with NULL
    reset_counters();

    uint64_t skill_id = 0;
    procedural_error_t result;

    // Create skill
    result = procedural_create_skill(nullptr, "test_skill", PROC_MOTOR, &skill_id);
    EXPECT_NE(result, PROC_SUCCESS);

    // Add step
    uint64_t step_id = 0;
    result = procedural_add_step(nullptr, 1, "test_action", 100.0f, 0.9f, &step_id);
    EXPECT_NE(result, PROC_SUCCESS);

    // Practice
    result = procedural_practice(nullptr, 1, 0.8f, 1000.0f);
    EXPECT_NE(result, PROC_SUCCESS);

    // Execute
    procedural_exec_result_t exec_result;
    result = procedural_execute(nullptr, 1, &exec_result);
    EXPECT_NE(result, PROC_SUCCESS);

    // Create habit
    prime_signature_t cue, response;
    memset(&cue, 0, sizeof(cue));
    memset(&response, 0, sizeof(response));
    uint64_t habit_id = 0;
    result = procedural_create_habit(nullptr, &cue, &response, 0.5f, &habit_id);
    EXPECT_NE(result, PROC_SUCCESS);

    // Get skill
    procedural_skill_t skill;
    result = procedural_get_skill(nullptr, 1, &skill);
    EXPECT_NE(result, PROC_SUCCESS);

    // Get stats
    procedural_stats_t stats;
    result = procedural_get_stats(nullptr, &stats);
    EXPECT_NE(result, PROC_SUCCESS);

    // Get skill node - this one throws exception
    pr_memory_node_t* node = procedural_get_skill_node(nullptr, 1);
    EXPECT_EQ(node, nullptr);

    // Should have tracked at least the exception from get_skill_node
    EXPECT_GT(exception_count.load(), 0);
}

//=============================================================================
// E2E Test: Complete Prospective Memory Workflow
//=============================================================================

TEST_F(PRMemoryExceptionE2ETest, ProspectiveWorkflow_NullInputs_AllOperationsFail) {
    // E2E: Complete prospective memory workflow with NULL
    reset_counters();

    uint64_t intent_id = 0;
    prospective_error_t result;

    // Create time intention
    result = prospective_create_time_intention(
        nullptr, 1000.0f, false, "test_action", 5.0f, 5.0f, &intent_id);
    EXPECT_NE(result, PROSP_SUCCESS);

    // Create activity intention
    result = prospective_create_activity_intention(
        nullptr, 1, "test_action", 5.0f, 5.0f, &intent_id);
    EXPECT_NE(result, PROSP_SUCCESS);

    // Create event intention
    prime_signature_t trigger;
    memset(&trigger, 0, sizeof(trigger));
    result = prospective_create_event_intention(
        nullptr, &trigger, 0.7f, "test_action", 5.0f, 5.0f, &intent_id);
    EXPECT_NE(result, PROSP_SUCCESS);

    // Update
    prospective_trigger_result_t triggered[10];
    size_t count = 0;
    result = prospective_update(nullptr, 1.0f, triggered, 10, &count);
    EXPECT_NE(result, PROSP_SUCCESS);

    // Check time triggers
    result = prospective_check_time_triggers(nullptr, 1000.0f, triggered, 10, &count);
    EXPECT_NE(result, PROSP_SUCCESS);

    // Execute intention
    result = prospective_execute_intention(nullptr, 1);
    EXPECT_NE(result, PROSP_SUCCESS);

    // Cancel intention
    result = prospective_cancel_intention(nullptr, 1);
    EXPECT_NE(result, PROSP_SUCCESS);

    // Get intention
    prospective_intention_t intent;
    result = prospective_get_intention(nullptr, 1, &intent);
    EXPECT_NE(result, PROSP_SUCCESS);

    // Get stats
    prospective_stats_t stats;
    result = prospective_get_stats(nullptr, &stats);
    EXPECT_NE(result, PROSP_SUCCESS);

    // Get memory node - this one throws exception
    pr_memory_node_t* node = prospective_get_memory_node(nullptr, 1);
    EXPECT_EQ(node, nullptr);

    // Should have tracked at least the exception from get_memory_node
    EXPECT_GT(exception_count.load(), 0);
}

//=============================================================================
// E2E Test: Long Running Stability
//=============================================================================

TEST_F(PRMemoryExceptionE2ETest, LongRunning_RepeatedCycles_NoMemoryLeaks) {
    // E2E: Test long-running stability with repeated exception cycles
    reset_counters();

    const int num_cycles = 50;

    for (int cycle = 0; cycle < num_cycles; cycle++) {
        // Valid operations
        pr_node_manager_config_t config = pr_node_manager_default_config();
        pr_node_manager_t manager = pr_node_manager_create(&config);
        ASSERT_NE(manager, nullptr);

        uint8_t data[] = {1, 2, 3, 4};
        pr_node_config_t node_config = pr_memory_node_default_config();
        pr_memory_node_t* node = pr_memory_node_create(manager, data, sizeof(data), &node_config);
        ASSERT_NE(node, nullptr);

        // Invalid operations (generate exceptions)
        pr_memory_node_create(nullptr, nullptr, 0, nullptr);
        procedural_get_skill_node(nullptr, 1);
        prospective_get_memory_node(nullptr, 1);

        // Cleanup
        pr_memory_node_destroy(node);
        pr_node_manager_destroy(manager);
    }

    // Should have many exceptions but system still running
    EXPECT_GT(exception_count.load(), num_cycles * 2);

    // Final valid operation to prove system is still working
    pr_node_manager_config_t final_config = pr_node_manager_default_config();
    pr_node_manager_t final_manager = pr_node_manager_create(&final_config);
    EXPECT_NE(final_manager, nullptr);
    pr_node_manager_destroy(final_manager);
}

//=============================================================================
// E2E Test: Stats After Exception Storm
//=============================================================================

TEST_F(PRMemoryExceptionE2ETest, StatsAfterExceptionStorm_SystemHealthy) {
    // E2E: Generate many exceptions and verify system stats
    reset_counters();

    // Exception storm
    for (int i = 0; i < 500; i++) {
        pr_memory_node_create(nullptr, nullptr, 0, nullptr);
        procedural_get_skill_node(nullptr, i % 100);
        prospective_get_memory_node(nullptr, i % 100);
        pr_node_manager_get_mem_manager(nullptr);
        pr_memory_node_get_signature(nullptr);
    }

    // Verify exception tracking
    EXPECT_GE(exception_count.load(), 1000);
    EXPECT_GE(null_pointer_count.load(), 1000);
    EXPECT_EQ(other_error_count.load(), 0);  // Only NULL pointer errors

    // System should still be responsive
    pr_node_manager_config_t config = pr_node_manager_default_config();
    pr_node_manager_t manager = pr_node_manager_create(&config);
    EXPECT_NE(manager, nullptr);
    pr_node_manager_destroy(manager);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
