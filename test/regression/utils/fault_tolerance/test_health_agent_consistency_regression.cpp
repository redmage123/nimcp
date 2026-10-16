/**
 * @file test_health_agent_consistency_regression.cpp
 * @brief Regression tests for NIMCP Health Agent State Consistency Manager (Phase 3)
 * @version 1.0.0
 * @date 2025-01-18
 *
 * WHAT: Regression tests to ensure consistency functions don't break
 * WHY:  Prevent regressions in consistency checking behavior across releases
 * HOW:  Test specific behaviors that must remain consistent
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <cstring>
#include <atomic>
#include <vector>

// Health agent header
#include "utils/fault_tolerance/nimcp_health_agent.h"

//=============================================================================
// Test Fixtures
//=============================================================================

/**
 * @brief Fixture for health agent consistency regression tests
 */
class HealthAgentConsistencyRegressionTest : public ::testing::Test {
protected:
    nimcp_health_agent_t* agent = nullptr;
    health_agent_config_t config;

    void SetUp() override {
        nimcp_health_agent_default_config(&config);
        config.check_interval_ms = 100;
        config.enable_auto_recovery = false;

        // Enable consistency checks
        config.consistency.check_reference_counts = true;
        config.consistency.check_pointer_canaries = true;
        config.consistency.check_struct_magic = true;
        config.consistency.check_mutex_state = true;
        config.consistency.check_circular_buffers = true;
        config.consistency.check_kg_consistency = true;
        config.consistency.check_neuron_values = true;
        config.consistency.consistency_check_interval_ms = 100;

        agent = nimcp_health_agent_create(&config);
        ASSERT_NE(agent, nullptr) << "Failed to create health agent";
    }

    void TearDown() override {
        if (agent) {
            if (nimcp_health_agent_is_running(agent)) {
                nimcp_health_agent_stop(agent);
            }
            nimcp_health_agent_destroy(agent);
            agent = nullptr;
        }
    }
};

//=============================================================================
// Return Value Contract Tests
// These tests verify the documented return value contract is maintained
//=============================================================================

TEST_F(HealthAgentConsistencyRegressionTest, ReturnValues_NullAgentAlwaysReturnsMinusOne) {
    // All consistency functions MUST return -1 for NULL agent
    // This is a documented behavior that must not change

    health_agent_consistency_result_t result;
    health_agent_consistency_config_t config;
    uint32_t test_struct = 0xDEADBEEF;

    EXPECT_EQ(nimcp_health_agent_check_consistency(nullptr, &result), -1);
    EXPECT_EQ(nimcp_health_agent_get_consistency_status(nullptr, &result), -1);
    EXPECT_EQ(nimcp_health_agent_update_consistency_config(nullptr, &config), -1);
    EXPECT_EQ(nimcp_health_agent_register_struct(nullptr, &test_struct, 0xDEADBEEF, "test"), -1);
    EXPECT_EQ(nimcp_health_agent_unregister_struct(nullptr, &test_struct), -1);
    EXPECT_EQ(nimcp_health_agent_request_check(nullptr), -1);
}

TEST_F(HealthAgentConsistencyRegressionTest, ReturnValues_ValidAgentReturnsZero) {
    // All consistency functions MUST return 0 for valid agent
    // when parameters are valid

    health_agent_consistency_result_t result;
    health_agent_consistency_config_t new_config;
    memset(&new_config, 0, sizeof(new_config));

    uint32_t test_struct = 0xDEADBEEF;

    EXPECT_EQ(nimcp_health_agent_check_consistency(agent, &result), 0);
    EXPECT_EQ(nimcp_health_agent_get_consistency_status(agent, &result), 0);
    EXPECT_EQ(nimcp_health_agent_update_consistency_config(agent, &new_config), 0);
    EXPECT_EQ(nimcp_health_agent_register_struct(agent, &test_struct, 0xDEADBEEF, "test"), 0);
    EXPECT_EQ(nimcp_health_agent_unregister_struct(agent, &test_struct), 0);
    EXPECT_EQ(nimcp_health_agent_request_check(agent), 0);
}

TEST_F(HealthAgentConsistencyRegressionTest, ReturnValues_NullResultAllowed) {
    // check_consistency MUST accept NULL result (just doesn't return result)
    EXPECT_EQ(nimcp_health_agent_check_consistency(agent, nullptr), 0);
}

TEST_F(HealthAgentConsistencyRegressionTest, ReturnValues_NullResultNotAllowedForGetStatus) {
    // get_consistency_status MUST reject NULL result
    EXPECT_EQ(nimcp_health_agent_get_consistency_status(agent, nullptr), -1);
}

//=============================================================================
// Validate Magic Contract Tests
//=============================================================================

TEST_F(HealthAgentConsistencyRegressionTest, ValidateMagic_NullPtrReturnsFalse) {
    // validate_magic MUST return false for NULL ptr
    EXPECT_FALSE(nimcp_health_agent_validate_magic(nullptr, 0x12345678, "test"));
}

TEST_F(HealthAgentConsistencyRegressionTest, ValidateMagic_MatchingMagicReturnsTrue) {
    // validate_magic MUST return true when magic matches
    uint32_t test_struct = 0xDEADBEEF;
    EXPECT_TRUE(nimcp_health_agent_validate_magic(&test_struct, 0xDEADBEEF, "test"));
}

TEST_F(HealthAgentConsistencyRegressionTest, ValidateMagic_MismatchedMagicReturnsFalse) {
    // validate_magic MUST return false when magic doesn't match
    uint32_t test_struct = 0xDEADBEEF;
    EXPECT_FALSE(nimcp_health_agent_validate_magic(&test_struct, 0xCAFEBABE, "test"));
}

TEST_F(HealthAgentConsistencyRegressionTest, ValidateMagic_AcceptsNullName) {
    // validate_magic MUST accept NULL name (uses "unknown" internally)
    uint32_t test_struct = 0xDEADBEEF;
    EXPECT_TRUE(nimcp_health_agent_validate_magic(&test_struct, 0xDEADBEEF, nullptr));
}

//=============================================================================
// Register/Unregister Contract Tests
//=============================================================================

TEST_F(HealthAgentConsistencyRegressionTest, RegisterStruct_NullPtrReturnsMinusOne) {
    // register_struct MUST return -1 for NULL ptr
    EXPECT_EQ(nimcp_health_agent_register_struct(agent, nullptr, 0x12345678, "test"), -1);
}

TEST_F(HealthAgentConsistencyRegressionTest, RegisterStruct_NullNameReturnsMinusOne) {
    // register_struct MUST return -1 for NULL name
    uint32_t test_struct = 0x12345678;
    EXPECT_EQ(nimcp_health_agent_register_struct(agent, &test_struct, 0x12345678, nullptr), -1);
}

TEST_F(HealthAgentConsistencyRegressionTest, UnregisterStruct_NotFoundReturnsMinusOne) {
    // unregister_struct MUST return -1 if struct not found
    uint32_t test_struct = 0x12345678;
    EXPECT_EQ(nimcp_health_agent_unregister_struct(agent, &test_struct), -1);
}

TEST_F(HealthAgentConsistencyRegressionTest, UnregisterStruct_DoubleUnregisterReturnsMinusOne) {
    // Double unregister MUST return -1 on second call
    uint32_t test_struct = 0x12345678;
    EXPECT_EQ(nimcp_health_agent_register_struct(agent, &test_struct, 0x12345678, "test"), 0);
    EXPECT_EQ(nimcp_health_agent_unregister_struct(agent, &test_struct), 0);
    EXPECT_EQ(nimcp_health_agent_unregister_struct(agent, &test_struct), -1);
}

//=============================================================================
// Consistency Result Structure Contract Tests
//=============================================================================

TEST_F(HealthAgentConsistencyRegressionTest, Result_HasTimestamp) {
    // Result MUST have a non-zero timestamp after check
    health_agent_consistency_result_t result;
    memset(&result, 0, sizeof(result));

    nimcp_health_agent_check_consistency(agent, &result);

    EXPECT_GT(result.timestamp_us, 0u);
}

TEST_F(HealthAgentConsistencyRegressionTest, Result_HasCheckDuration) {
    // Result MUST have check_duration_us set
    health_agent_consistency_result_t result;
    memset(&result, 0, sizeof(result));

    nimcp_health_agent_check_consistency(agent, &result);

    EXPECT_GE(result.check_duration_us, 0u);
}

TEST_F(HealthAgentConsistencyRegressionTest, Result_AllChecksPassed_OverallPassedTrue) {
    // If all individual checks pass, overall_passed MUST be true
    health_agent_consistency_result_t result;
    memset(&result, 0, sizeof(result));

    nimcp_health_agent_check_consistency(agent, &result);

    if (result.refcount_check_passed && result.canary_check_passed &&
        result.magic_check_passed && result.mutex_check_passed &&
        result.buffer_check_passed && result.kg_check_passed &&
        result.neuron_check_passed) {
        EXPECT_TRUE(result.overall_passed);
    }
}

TEST_F(HealthAgentConsistencyRegressionTest, Result_CleanAgentPassesAll) {
    // A freshly created agent MUST pass all consistency checks
    health_agent_consistency_result_t result;

    int ret = nimcp_health_agent_check_consistency(agent, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(result.overall_passed);
    EXPECT_TRUE(result.refcount_check_passed);
    EXPECT_TRUE(result.canary_check_passed);
    EXPECT_TRUE(result.magic_check_passed);
    EXPECT_TRUE(result.mutex_check_passed);
    EXPECT_TRUE(result.buffer_check_passed);
    EXPECT_TRUE(result.kg_check_passed);
    EXPECT_TRUE(result.neuron_check_passed);
}

//=============================================================================
// Magic Corruption Detection Contract Tests
//=============================================================================

TEST_F(HealthAgentConsistencyRegressionTest, DetectsCorruptedRegisteredStruct) {
    // Consistency check MUST detect corrupted magic in registered structs
    uint32_t test_struct = 0xDEADBEEF;

    nimcp_health_agent_register_struct(agent, &test_struct, 0xDEADBEEF, "test");

    // Corrupt the magic
    test_struct = 0xBADC0DE;

    health_agent_consistency_result_t result;
    int ret = nimcp_health_agent_check_consistency(agent, &result);

    EXPECT_EQ(ret, -1);  // Overall failure
    EXPECT_FALSE(result.magic_check_passed);
    EXPECT_GT(result.magic_violations, 0u);
}

TEST_F(HealthAgentConsistencyRegressionTest, UnregisteredStructNotChecked) {
    // After unregistering, corrupted struct MUST NOT cause failure
    uint32_t test_struct = 0xDEADBEEF;

    nimcp_health_agent_register_struct(agent, &test_struct, 0xDEADBEEF, "test");
    nimcp_health_agent_unregister_struct(agent, &test_struct);

    // Corrupt after unregister
    test_struct = 0xBADC0DE;

    health_agent_consistency_result_t result;
    int ret = nimcp_health_agent_check_consistency(agent, &result);

    EXPECT_EQ(ret, 0);  // Should pass
    EXPECT_TRUE(result.magic_check_passed);
}

//=============================================================================
// Registry Capacity Contract Tests
//=============================================================================

TEST_F(HealthAgentConsistencyRegressionTest, RegistryCapacity_Is64) {
    // Registry MUST support at least 64 registered structs
    const int EXPECTED_CAPACITY = 64;
    uint32_t test_structs[EXPECTED_CAPACITY + 10];
    int registered = 0;

    for (int i = 0; i < EXPECTED_CAPACITY + 10; i++) {
        test_structs[i] = 0xBEEF0000 + i;
        char name[32];
        snprintf(name, sizeof(name), "struct_%d", i);
        if (nimcp_health_agent_register_struct(agent, &test_structs[i],
                                                test_structs[i], name) == 0) {
            registered++;
        }
    }

    EXPECT_EQ(registered, EXPECTED_CAPACITY);

    // Clean up
    for (int i = 0; i < registered; i++) {
        nimcp_health_agent_unregister_struct(agent, &test_structs[i]);
    }
}

//=============================================================================
// Thread Safety Contract Tests
//=============================================================================

TEST_F(HealthAgentConsistencyRegressionTest, ThreadSafe_ConcurrentChecks) {
    // Concurrent consistency checks MUST not crash or corrupt state
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    const int NUM_THREADS = 4;
    const int ITERATIONS = 50;
    std::atomic<int> completed{0};

    auto worker = [&]() {
        for (int i = 0; i < ITERATIONS; i++) {
            health_agent_consistency_result_t result;
            nimcp_health_agent_check_consistency(agent, &result);
            completed++;
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(worker);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(completed.load(), NUM_THREADS * ITERATIONS);

    nimcp_health_agent_stop(agent);
}

TEST_F(HealthAgentConsistencyRegressionTest, ThreadSafe_ConcurrentRegisterUnregister) {
    // Concurrent register/unregister MUST not crash or corrupt state
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    const int NUM_THREADS = 4;
    const int ITERATIONS = 20;
    std::atomic<int> operations{0};

    auto worker = [&](int id) {
        for (int i = 0; i < ITERATIONS; i++) {
            uint32_t test_struct = 0xABCD0000 + (id * 100) + i;
            char name[32];
            snprintf(name, sizeof(name), "t%d_s%d", id, i);

            if (nimcp_health_agent_register_struct(agent, &test_struct,
                                                    test_struct, name) == 0) {
                operations++;
                nimcp_health_agent_unregister_struct(agent, &test_struct);
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(worker, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(operations.load(), 0);

    nimcp_health_agent_stop(agent);
}

//=============================================================================
// Running Agent Contract Tests
//=============================================================================

TEST_F(HealthAgentConsistencyRegressionTest, RunningAgent_ChecksStillWork) {
    // Consistency checks MUST work while agent is running
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    health_agent_consistency_result_t result;
    EXPECT_EQ(nimcp_health_agent_check_consistency(agent, &result), 0);
    EXPECT_TRUE(result.overall_passed);

    nimcp_health_agent_stop(agent);
}

TEST_F(HealthAgentConsistencyRegressionTest, StoppedAgent_ChecksStillWork) {
    // Consistency checks MUST work after agent is stopped
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);

    health_agent_consistency_result_t result;
    EXPECT_EQ(nimcp_health_agent_check_consistency(agent, &result), 0);
    EXPECT_TRUE(result.overall_passed);
}

//=============================================================================
// Config Update Contract Tests
//=============================================================================

TEST_F(HealthAgentConsistencyRegressionTest, ConfigUpdate_NullConfigReturnsMinusOne) {
    // update_consistency_config MUST return -1 for NULL config
    EXPECT_EQ(nimcp_health_agent_update_consistency_config(agent, nullptr), -1);
}

TEST_F(HealthAgentConsistencyRegressionTest, ConfigUpdate_SucceedsWhileRunning) {
    // Config update MUST succeed while agent is running
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    health_agent_consistency_config_t new_config;
    memset(&new_config, 0, sizeof(new_config));
    new_config.consistency_check_interval_ms = 200;

    EXPECT_EQ(nimcp_health_agent_update_consistency_config(agent, &new_config), 0);

    nimcp_health_agent_stop(agent);
}
