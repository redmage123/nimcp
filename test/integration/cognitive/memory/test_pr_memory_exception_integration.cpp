/**
 * @file test_pr_memory_exception_integration.cpp
 * @brief Integration tests for PR memory system exception handling
 * @date 2026-01-25
 *
 * WHAT: Integration tests for exception handling across PR memory modules
 * WHY:  Verify exception handling works correctly when modules interact
 * HOW:  Test multi-module scenarios with brain immune system integration
 *
 * INTEGRATION SCENARIOS TESTED:
 * - pr_memory_node + procedural memory integration
 * - pr_memory_node + prospective memory integration
 * - Exception propagation to brain immune system
 * - Multi-module exception recovery
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <cstring>
#include <atomic>
#include <thread>
#include <chrono>

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

class PRMemoryExceptionIntegrationTest : public ::testing::Test {
protected:
    static std::atomic<int> exception_count;
    static std::atomic<int> last_error_code;
    static nimcp_handler_registration_t* registration;

    void SetUp() override {
        exception_count = 0;
        last_error_code = 0;

        nimcp_exception_system_init();

        nimcp_handler_options_t options;
        nimcp_handler_default_options(&options);
        options.handler = test_exception_handler;
        options.user_data = nullptr;
        options.priority = NIMCP_HANDLER_PRIORITY_HIGH;
        options.name = "pr_memory_integration_test_handler";
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
        last_error_code = ex->code;
        return false;  // Don't suppress
    }

    void reset_counters() {
        exception_count = 0;
        last_error_code = 0;
    }
};

std::atomic<int> PRMemoryExceptionIntegrationTest::exception_count{0};
std::atomic<int> PRMemoryExceptionIntegrationTest::last_error_code{0};
nimcp_handler_registration_t* PRMemoryExceptionIntegrationTest::registration = nullptr;

//=============================================================================
// PR Memory Node Manager Integration Tests
//=============================================================================

TEST_F(PRMemoryExceptionIntegrationTest, NodeManagerCreate_ValidConfig_Succeeds) {
    pr_node_manager_config_t config = pr_node_manager_default_config();
    pr_node_manager_t manager = pr_node_manager_create(&config);

    EXPECT_NE(manager, nullptr);

    if (manager) {
        pr_node_manager_destroy(manager);
    }
}

TEST_F(PRMemoryExceptionIntegrationTest, NodeManager_CreateAndDestroyMultiple_NoLeaks) {
    pr_node_manager_config_t config = pr_node_manager_default_config();

    // Create and destroy multiple managers to test cleanup
    for (int i = 0; i < 5; i++) {
        pr_node_manager_t manager = pr_node_manager_create(&config);
        EXPECT_NE(manager, nullptr);
        if (manager) {
            pr_node_manager_destroy(manager);
        }
    }

    // No exceptions should be thrown during normal create/destroy
    EXPECT_EQ(exception_count.load(), 0);
}

TEST_F(PRMemoryExceptionIntegrationTest, NodeManager_CreateNode_ExceptionOnNullManager) {
    uint8_t data[] = {1, 2, 3, 4};
    pr_node_config_t config = pr_memory_node_default_config();

    // Creating node with NULL manager should throw exception
    pr_memory_node_t* node = pr_memory_node_create(nullptr, data, sizeof(data), &config);

    EXPECT_EQ(node, nullptr);
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

//=============================================================================
// Procedural Memory Integration Tests
//=============================================================================

TEST_F(PRMemoryExceptionIntegrationTest, ProceduralGetNode_NullPm_ThrowsException) {
    // Try to get a skill node from NULL procedural memory
    pr_memory_node_t* node = procedural_get_skill_node(nullptr, 1);

    EXPECT_EQ(node, nullptr);
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PRMemoryExceptionIntegrationTest, ProceduralGetHabitNode_NullPm_ThrowsException) {
    // Try to get a habit node from NULL procedural memory
    pr_memory_node_t* node = procedural_get_habit_node(nullptr, 1);

    EXPECT_EQ(node, nullptr);
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PRMemoryExceptionIntegrationTest, ProceduralMultipleOperations_NullPm_AllReturnErrors) {
    uint64_t id = 0;

    // Multiple operations with NULL procedural memory should all fail gracefully
    procedural_error_t r1 = procedural_create_skill(nullptr, "test", PROC_MOTOR, &id);
    EXPECT_NE(r1, PROC_SUCCESS);

    uint64_t step_id = 0;
    procedural_error_t r2 = procedural_add_step(nullptr, 1, "action", 100.0f, 0.9f, &step_id);
    EXPECT_NE(r2, PROC_SUCCESS);

    procedural_error_t r3 = procedural_practice(nullptr, 1, 0.8f, 1000.0f);
    EXPECT_NE(r3, PROC_SUCCESS);

    procedural_exec_result_t result;
    procedural_error_t r4 = procedural_execute(nullptr, 1, &result);
    EXPECT_NE(r4, PROC_SUCCESS);

    procedural_error_t r5 = procedural_remove_skill(nullptr, 1);
    EXPECT_NE(r5, PROC_SUCCESS);
}

//=============================================================================
// Prospective Memory Integration Tests
//=============================================================================

TEST_F(PRMemoryExceptionIntegrationTest, ProspectiveGetNode_NullPm_ThrowsException) {
    // Try to get memory node from NULL prospective memory
    pr_memory_node_t* node = prospective_get_memory_node(nullptr, 1);

    EXPECT_EQ(node, nullptr);
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PRMemoryExceptionIntegrationTest, ProspectiveMultipleOperations_NullPm_AllReturnErrors) {
    uint64_t id = 0;

    // Multiple operations with NULL prospective memory should all fail gracefully
    prospective_error_t r1 = prospective_create_time_intention(
        nullptr, 1000.0f, false, "test", 5.0f, 5.0f, &id);
    EXPECT_NE(r1, PROSP_SUCCESS);

    prospective_error_t r2 = prospective_create_activity_intention(
        nullptr, 1, "test", 5.0f, 5.0f, &id);
    EXPECT_NE(r2, PROSP_SUCCESS);

    prime_signature_t trigger;
    memset(&trigger, 0, sizeof(trigger));
    prospective_error_t r3 = prospective_create_event_intention(
        nullptr, &trigger, 0.7f, "test", 5.0f, 5.0f, &id);
    EXPECT_NE(r3, PROSP_SUCCESS);

    prospective_trigger_result_t triggered[10];
    size_t count = 0;
    prospective_error_t r4 = prospective_update(nullptr, 1.0f, triggered, 10, &count);
    EXPECT_NE(r4, PROSP_SUCCESS);

    prospective_error_t r5 = prospective_execute_intention(nullptr, 1);
    EXPECT_NE(r5, PROSP_SUCCESS);

    prospective_error_t r6 = prospective_cancel_intention(nullptr, 1);
    EXPECT_NE(r6, PROSP_SUCCESS);
}

//=============================================================================
// Cross-Module Integration Tests
//=============================================================================

TEST_F(PRMemoryExceptionIntegrationTest, NodeAndProcedural_NullChain_ExceptionsPropagate) {
    // Test exception propagation through cross-module calls
    reset_counters();

    // First call to procedural with NULL
    pr_memory_node_t* node1 = procedural_get_skill_node(nullptr, 1);
    EXPECT_EQ(node1, nullptr);
    int first_count = exception_count.load();
    EXPECT_GT(first_count, 0);

    // Second call to node manager with NULL
    unified_mem_manager_t mem = pr_node_manager_get_mem_manager(nullptr);
    EXPECT_EQ(mem, nullptr);
    EXPECT_GT(exception_count.load(), first_count);
}

TEST_F(PRMemoryExceptionIntegrationTest, NodeAndProspective_NullChain_ExceptionsPropagate) {
    // Test exception propagation through cross-module calls
    reset_counters();

    // First call to prospective with NULL
    pr_memory_node_t* node1 = prospective_get_memory_node(nullptr, 1);
    EXPECT_EQ(node1, nullptr);
    int first_count = exception_count.load();
    EXPECT_GT(first_count, 0);

    // Second call to node signature with NULL
    const prime_signature_t* sig = pr_memory_node_get_signature(nullptr);
    EXPECT_EQ(sig, nullptr);
    EXPECT_GT(exception_count.load(), first_count);
}

TEST_F(PRMemoryExceptionIntegrationTest, AllModules_SequentialNullCalls_AllCaptured) {
    // Test that exceptions from all modules are properly captured
    reset_counters();

    // Call each module with NULL
    pr_memory_node_t* node1 = pr_memory_node_clone(nullptr, nullptr);
    EXPECT_EQ(node1, nullptr);

    pr_memory_node_t* node2 = procedural_get_skill_node(nullptr, 1);
    EXPECT_EQ(node2, nullptr);

    pr_memory_node_t* node3 = prospective_get_memory_node(nullptr, 1);
    EXPECT_EQ(node3, nullptr);

    unified_mem_manager_t mem = pr_node_manager_get_mem_manager(nullptr);
    EXPECT_EQ(mem, nullptr);

    const prime_signature_t* sig = pr_memory_node_get_signature(nullptr);
    EXPECT_EQ(sig, nullptr);

    // All exception-throwing calls should be captured
    EXPECT_GE(exception_count.load(), 4);
}

//=============================================================================
// Exception Handler Registration Tests
//=============================================================================

TEST_F(PRMemoryExceptionIntegrationTest, ExceptionHandler_ReceivesCorrectInfo) {
    // Trigger an exception and verify handler receives correct info
    reset_counters();

    pr_memory_node_t* node = pr_memory_node_create(nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(node, nullptr);

    // Verify exception info
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PRMemoryExceptionIntegrationTest, MultipleHandlers_AllReceiveExceptions) {
    // Register a second handler
    static std::atomic<int> secondary_count{0};
    secondary_count = 0;

    nimcp_handler_options_t options2;
    nimcp_handler_default_options(&options2);
    options2.handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        (void)ex;
        (void)user_data;
        secondary_count++;
        return false;
    };
    options2.priority = NIMCP_HANDLER_PRIORITY_NORMAL;
    options2.name = "secondary_handler";
    nimcp_handler_registration_t* reg2 = nimcp_handler_register(&options2);

    // Trigger an exception
    reset_counters();
    pr_memory_node_t* node = pr_memory_node_create(nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(node, nullptr);

    // Both handlers should receive the exception
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_GT(secondary_count.load(), 0);

    nimcp_handler_unregister(reg2);
}

//=============================================================================
// Memory Node Integration with Signature Tests
//=============================================================================

TEST_F(PRMemoryExceptionIntegrationTest, NodeSignatureOperations_NullNodes_HandleGracefully) {
    // Test signature operations with NULL nodes
    const prime_signature_t* sig = pr_memory_node_get_signature(nullptr);
    EXPECT_EQ(sig, nullptr);

    pr_node_error_t result = pr_memory_node_update_signature(nullptr);
    EXPECT_NE(result, PR_NODE_SUCCESS);

    prime_signature_t new_sig;
    memset(&new_sig, 0, sizeof(new_sig));
    result = pr_memory_node_set_signature(nullptr, &new_sig);
    EXPECT_NE(result, PR_NODE_SUCCESS);

    float similarity = pr_memory_node_signature_similarity(
        nullptr, nullptr, PRIME_SIG_SIMILARITY_COSINE);
    EXPECT_LE(similarity, 0.0f);
}

//=============================================================================
// Memory Tier Operations Integration Tests
//=============================================================================

TEST_F(PRMemoryExceptionIntegrationTest, NodeTierOperations_NullNode_ReturnsErrors) {
    // Test tier operations with NULL node
    pr_node_error_t result;

    result = pr_memory_node_promote(nullptr);
    EXPECT_NE(result, PR_NODE_SUCCESS);

    result = pr_memory_node_demote(nullptr);
    EXPECT_NE(result, PR_NODE_SUCCESS);

    result = pr_memory_node_set_tier(nullptr, PR_MEMORY_TIER_Z1);
    EXPECT_NE(result, PR_NODE_SUCCESS);

    pr_memory_tier_t tier = pr_memory_node_get_tier(nullptr);
    EXPECT_EQ(tier, PR_MEMORY_TIER_Z0);
}

//=============================================================================
// Concurrent Exception Handling Tests
//=============================================================================

TEST_F(PRMemoryExceptionIntegrationTest, ConcurrentExceptions_AllCaptured) {
    // Test that concurrent exceptions from multiple threads are all captured
    reset_counters();

    std::atomic<int> threads_done{0};
    const int num_threads = 4;
    const int ops_per_thread = 10;

    auto thread_func = [&]() {
        for (int i = 0; i < ops_per_thread; i++) {
            // Trigger exceptions from different modules
            pr_memory_node_t* node = procedural_get_skill_node(nullptr, 1);
            (void)node;
        }
        threads_done++;
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back(thread_func);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(threads_done.load(), num_threads);
    // All threads should have triggered exceptions
    EXPECT_GE(exception_count.load(), num_threads * ops_per_thread);
}

//=============================================================================
// Decay and Reinforcement Integration Tests
//=============================================================================

TEST_F(PRMemoryExceptionIntegrationTest, DecayOperations_NullNode_HandleGracefully) {
    // Test decay operations with NULL node
    float strength = pr_memory_node_apply_decay(nullptr, 1.0f);
    EXPECT_LE(strength, 0.0f);

    strength = pr_memory_node_reinforce(nullptr, 0.1f);
    EXPECT_LE(strength, 0.0f);

    bool eligible = pr_memory_node_check_eligibility(nullptr);
    EXPECT_FALSE(eligible);

    pr_node_error_t result = pr_memory_node_set_decay_rate(nullptr, 0.01f);
    EXPECT_NE(result, PR_NODE_SUCCESS);
}

//=============================================================================
// Stats Integration Tests
//=============================================================================

TEST_F(PRMemoryExceptionIntegrationTest, GetStats_NullParameters_HandleGracefully) {
    // Test stats operations with NULL parameters
    pr_node_stats_t node_stats;
    pr_node_error_t result = pr_memory_node_get_stats(nullptr, &node_stats);
    EXPECT_NE(result, PR_NODE_SUCCESS);

    procedural_stats_t proc_stats;
    procedural_error_t proc_result = procedural_get_stats(nullptr, &proc_stats);
    EXPECT_NE(proc_result, PROC_SUCCESS);

    prospective_stats_t prosp_stats;
    prospective_error_t prosp_result = prospective_get_stats(nullptr, &prosp_stats);
    EXPECT_NE(prosp_result, PROSP_SUCCESS);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
