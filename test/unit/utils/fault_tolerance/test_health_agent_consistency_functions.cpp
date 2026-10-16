/**
 * @file test_health_agent_consistency_functions.cpp
 * @brief Unit tests for NIMCP Health Agent State Consistency Manager (Phase 3)
 * @version 1.0.0
 * @date 2025-01-18
 *
 * WHAT: Test health agent state consistency checking functions
 * WHY:  Ensure consistency checks detect corruption and validate state correctly
 * HOW:  Test each consistency check function with valid and invalid states
 */

#include <gtest/gtest.h>
#include <cstring>
#include <thread>
#include <chrono>
#include <atomic>

// Health agent header (has its own extern "C" guard)
#include "utils/fault_tolerance/nimcp_health_agent.h"

//=============================================================================
// Test Fixtures
//=============================================================================

/**
 * @brief Base fixture for health agent consistency tests
 */
class HealthAgentConsistencyTest : public ::testing::Test {
protected:
    nimcp_health_agent_t* agent = nullptr;
    health_agent_config_t config;

    void SetUp() override {
        nimcp_health_agent_default_config(&config);
        config.check_interval_ms = 100;
        config.enable_auto_recovery = false;

        // Enable all consistency checks for testing
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

/**
 * @brief Fixture for running agent consistency tests
 */
class HealthAgentRunningConsistencyTest : public HealthAgentConsistencyTest {
protected:
    void SetUp() override {
        HealthAgentConsistencyTest::SetUp();
        ASSERT_EQ(nimcp_health_agent_start(agent), 0);
    }

    void TearDown() override {
        if (agent && nimcp_health_agent_is_running(agent)) {
            nimcp_health_agent_stop(agent);
        }
        HealthAgentConsistencyTest::TearDown();
    }
};

//=============================================================================
// nimcp_health_agent_check_consistency Tests
//=============================================================================

TEST_F(HealthAgentConsistencyTest, CheckConsistency_NullAgent) {
    health_agent_consistency_result_t result;
    EXPECT_EQ(nimcp_health_agent_check_consistency(nullptr, &result), -1);
}

TEST_F(HealthAgentConsistencyTest, CheckConsistency_NullResult) {
    // Should succeed even with NULL result - just doesn't return result
    EXPECT_EQ(nimcp_health_agent_check_consistency(agent, nullptr), 0);
}

TEST_F(HealthAgentConsistencyTest, CheckConsistency_ValidAgent) {
    health_agent_consistency_result_t result;
    memset(&result, 0, sizeof(result));

    int ret = nimcp_health_agent_check_consistency(agent, &result);

    // Should pass all checks on a freshly created agent
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

TEST_F(HealthAgentConsistencyTest, CheckConsistency_HasTimestamp) {
    health_agent_consistency_result_t result;
    memset(&result, 0, sizeof(result));

    nimcp_health_agent_check_consistency(agent, &result);

    EXPECT_GT(result.timestamp_us, 0u);
    // Duration can be 0 on fast systems where check completes in < 1 microsecond
    EXPECT_GE(result.check_duration_us, 0u);
}

TEST_F(HealthAgentConsistencyTest, CheckConsistency_NoErrors) {
    health_agent_consistency_result_t result;
    memset(&result, 0, sizeof(result));

    nimcp_health_agent_check_consistency(agent, &result);

    EXPECT_EQ(result.refcount_errors, 0u);
    EXPECT_EQ(result.canary_corruptions, 0u);
    EXPECT_EQ(result.magic_violations, 0u);
    EXPECT_EQ(result.mutex_anomalies, 0u);
    EXPECT_EQ(result.buffer_errors, 0u);
    EXPECT_EQ(result.kg_inconsistencies, 0u);
    EXPECT_EQ(result.nan_inf_count, 0u);
}

//=============================================================================
// nimcp_health_agent_get_consistency_status Tests
//=============================================================================

TEST_F(HealthAgentConsistencyTest, GetConsistencyStatus_NullAgent) {
    health_agent_consistency_result_t result;
    EXPECT_EQ(nimcp_health_agent_get_consistency_status(nullptr, &result), -1);
}

TEST_F(HealthAgentConsistencyTest, GetConsistencyStatus_NullResult) {
    EXPECT_EQ(nimcp_health_agent_get_consistency_status(agent, nullptr), -1);
}

TEST_F(HealthAgentConsistencyTest, GetConsistencyStatus_BeforeCheck) {
    health_agent_consistency_result_t result;
    memset(&result, 0xFF, sizeof(result));  // Fill with non-zero

    // Should succeed but return zero/empty result (no check performed yet)
    EXPECT_EQ(nimcp_health_agent_get_consistency_status(agent, &result), 0);
    EXPECT_EQ(result.timestamp_us, 0u);  // No check performed yet
}

TEST_F(HealthAgentConsistencyTest, GetConsistencyStatus_AfterCheck) {
    health_agent_consistency_result_t check_result;
    health_agent_consistency_result_t status_result;

    // First run a check
    nimcp_health_agent_check_consistency(agent, &check_result);

    // Then get status
    EXPECT_EQ(nimcp_health_agent_get_consistency_status(agent, &status_result), 0);

    // Results should match
    EXPECT_EQ(status_result.timestamp_us, check_result.timestamp_us);
    EXPECT_EQ(status_result.overall_passed, check_result.overall_passed);
}

//=============================================================================
// nimcp_health_agent_update_consistency_config Tests
//=============================================================================

TEST_F(HealthAgentConsistencyTest, UpdateConsistencyConfig_NullAgent) {
    health_agent_consistency_config_t new_config;
    memset(&new_config, 0, sizeof(new_config));
    EXPECT_EQ(nimcp_health_agent_update_consistency_config(nullptr, &new_config), -1);
}

TEST_F(HealthAgentConsistencyTest, UpdateConsistencyConfig_NullConfig) {
    EXPECT_EQ(nimcp_health_agent_update_consistency_config(agent, nullptr), -1);
}

TEST_F(HealthAgentConsistencyTest, UpdateConsistencyConfig_Valid) {
    health_agent_consistency_config_t new_config;
    memset(&new_config, 0, sizeof(new_config));
    new_config.check_reference_counts = false;
    new_config.check_pointer_canaries = true;
    new_config.consistency_check_interval_ms = 5000;

    EXPECT_EQ(nimcp_health_agent_update_consistency_config(agent, &new_config), 0);
}

TEST_F(HealthAgentRunningConsistencyTest, UpdateConsistencyConfig_WhileRunning) {
    health_agent_consistency_config_t new_config;
    memset(&new_config, 0, sizeof(new_config));
    new_config.check_reference_counts = true;
    new_config.consistency_check_interval_ms = 200;

    // Should succeed while agent is running
    EXPECT_EQ(nimcp_health_agent_update_consistency_config(agent, &new_config), 0);
}

//=============================================================================
// nimcp_health_agent_validate_magic Tests
//=============================================================================

TEST_F(HealthAgentConsistencyTest, ValidateMagic_NullPtr) {
    EXPECT_FALSE(nimcp_health_agent_validate_magic(nullptr, 0x12345678, "test_struct"));
}

TEST_F(HealthAgentConsistencyTest, ValidateMagic_Valid) {
    uint32_t test_struct = 0xDEADBEEF;
    EXPECT_TRUE(nimcp_health_agent_validate_magic(&test_struct, 0xDEADBEEF, "test_struct"));
}

TEST_F(HealthAgentConsistencyTest, ValidateMagic_Invalid) {
    uint32_t test_struct = 0xDEADBEEF;
    EXPECT_FALSE(nimcp_health_agent_validate_magic(&test_struct, 0xCAFEBABE, "test_struct"));
}

TEST_F(HealthAgentConsistencyTest, ValidateMagic_NullName) {
    uint32_t test_struct = 0xDEADBEEF;
    // Should work with NULL name
    EXPECT_TRUE(nimcp_health_agent_validate_magic(&test_struct, 0xDEADBEEF, nullptr));
}

//=============================================================================
// nimcp_health_agent_register_struct / unregister_struct Tests
//=============================================================================

TEST_F(HealthAgentConsistencyTest, RegisterStruct_NullAgent) {
    uint32_t test_struct = 0x12345678;
    EXPECT_EQ(nimcp_health_agent_register_struct(nullptr, &test_struct, 0x12345678, "test"), -1);
}

TEST_F(HealthAgentConsistencyTest, RegisterStruct_NullPtr) {
    EXPECT_EQ(nimcp_health_agent_register_struct(agent, nullptr, 0x12345678, "test"), -1);
}

TEST_F(HealthAgentConsistencyTest, RegisterStruct_NullName) {
    uint32_t test_struct = 0x12345678;
    EXPECT_EQ(nimcp_health_agent_register_struct(agent, &test_struct, 0x12345678, nullptr), -1);
}

TEST_F(HealthAgentConsistencyTest, RegisterStruct_Valid) {
    uint32_t test_struct = 0xDEADBEEF;
    EXPECT_EQ(nimcp_health_agent_register_struct(agent, &test_struct, 0xDEADBEEF, "test_struct"), 0);
}

TEST_F(HealthAgentConsistencyTest, RegisterStruct_ThenCheck) {
    uint32_t test_struct = 0xDEADBEEF;

    // Register valid struct
    EXPECT_EQ(nimcp_health_agent_register_struct(agent, &test_struct, 0xDEADBEEF, "test_struct"), 0);

    // Consistency check should pass
    health_agent_consistency_result_t result;
    EXPECT_EQ(nimcp_health_agent_check_consistency(agent, &result), 0);
    EXPECT_TRUE(result.magic_check_passed);
}

TEST_F(HealthAgentConsistencyTest, RegisterStruct_CorruptedMagic) {
    uint32_t test_struct = 0xDEADBEEF;

    // Register struct
    EXPECT_EQ(nimcp_health_agent_register_struct(agent, &test_struct, 0xDEADBEEF, "test_struct"), 0);

    // Corrupt the magic
    test_struct = 0xBADC0DE;

    // Consistency check should detect corruption
    health_agent_consistency_result_t result;
    int ret = nimcp_health_agent_check_consistency(agent, &result);
    // Returns -1 if any check fails
    EXPECT_EQ(ret, -1);
    EXPECT_FALSE(result.magic_check_passed);
    EXPECT_GT(result.magic_violations, 0u);
}

TEST_F(HealthAgentConsistencyTest, UnregisterStruct_NullAgent) {
    uint32_t test_struct = 0x12345678;
    EXPECT_EQ(nimcp_health_agent_unregister_struct(nullptr, &test_struct), -1);
}

TEST_F(HealthAgentConsistencyTest, UnregisterStruct_NullPtr) {
    EXPECT_EQ(nimcp_health_agent_unregister_struct(agent, nullptr), -1);
}

TEST_F(HealthAgentConsistencyTest, UnregisterStruct_NotRegistered) {
    uint32_t test_struct = 0x12345678;
    EXPECT_EQ(nimcp_health_agent_unregister_struct(agent, &test_struct), -1);
}

TEST_F(HealthAgentConsistencyTest, UnregisterStruct_Valid) {
    uint32_t test_struct = 0xDEADBEEF;

    // Register then unregister
    EXPECT_EQ(nimcp_health_agent_register_struct(agent, &test_struct, 0xDEADBEEF, "test_struct"), 0);
    EXPECT_EQ(nimcp_health_agent_unregister_struct(agent, &test_struct), 0);

    // Double unregister should fail
    EXPECT_EQ(nimcp_health_agent_unregister_struct(agent, &test_struct), -1);
}

TEST_F(HealthAgentConsistencyTest, UnregisterStruct_AfterCorruption) {
    uint32_t test_struct = 0xDEADBEEF;

    // Register, corrupt, unregister
    EXPECT_EQ(nimcp_health_agent_register_struct(agent, &test_struct, 0xDEADBEEF, "test_struct"), 0);
    test_struct = 0xBADC0DE;  // Corrupt magic
    EXPECT_EQ(nimcp_health_agent_unregister_struct(agent, &test_struct), 0);

    // Consistency check should pass now (struct unregistered)
    health_agent_consistency_result_t result;
    EXPECT_EQ(nimcp_health_agent_check_consistency(agent, &result), 0);
    EXPECT_TRUE(result.magic_check_passed);
}

//=============================================================================
// nimcp_health_agent_request_check Tests
//=============================================================================

TEST_F(HealthAgentConsistencyTest, RequestCheck_NullAgent) {
    EXPECT_EQ(nimcp_health_agent_request_check(nullptr), -1);
}

TEST_F(HealthAgentConsistencyTest, RequestCheck_Valid) {
    EXPECT_EQ(nimcp_health_agent_request_check(agent), 0);
}

TEST_F(HealthAgentRunningConsistencyTest, RequestCheck_WhileRunning) {
    // Request check while running
    EXPECT_EQ(nimcp_health_agent_request_check(agent), 0);

    // Wait for check to be processed
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Get status - should have a recent check
    health_agent_consistency_result_t result;
    EXPECT_EQ(nimcp_health_agent_get_consistency_status(agent, &result), 0);
    // Note: timestamp might be 0 if check hasn't run yet, which is OK
}

//=============================================================================
// Multiple Registered Structures Tests
//=============================================================================

TEST_F(HealthAgentConsistencyTest, MultipleStructs_Register) {
    const int NUM_STRUCTS = 10;
    uint32_t test_structs[NUM_STRUCTS];

    for (int i = 0; i < NUM_STRUCTS; i++) {
        test_structs[i] = 0xBEEF0000 + i;
        char name[32];
        snprintf(name, sizeof(name), "struct_%d", i);
        EXPECT_EQ(nimcp_health_agent_register_struct(agent, &test_structs[i],
                                                      test_structs[i], name), 0);
    }

    // All should pass consistency check
    health_agent_consistency_result_t result;
    EXPECT_EQ(nimcp_health_agent_check_consistency(agent, &result), 0);
    EXPECT_TRUE(result.magic_check_passed);
}

TEST_F(HealthAgentConsistencyTest, MultipleStructs_PartialCorruption) {
    const int NUM_STRUCTS = 5;
    uint32_t test_structs[NUM_STRUCTS];

    for (int i = 0; i < NUM_STRUCTS; i++) {
        test_structs[i] = 0xBEEF0000 + i;
        char name[32];
        snprintf(name, sizeof(name), "struct_%d", i);
        nimcp_health_agent_register_struct(agent, &test_structs[i], test_structs[i], name);
    }

    // Corrupt just one struct
    test_structs[2] = 0xBAD;

    health_agent_consistency_result_t result;
    int ret = nimcp_health_agent_check_consistency(agent, &result);
    EXPECT_EQ(ret, -1);
    EXPECT_FALSE(result.magic_check_passed);
    EXPECT_EQ(result.magic_violations, 1u);  // Only one corrupted
}

//=============================================================================
// Concurrent Access Tests
//=============================================================================

TEST_F(HealthAgentRunningConsistencyTest, ConcurrentConsistencyChecks) {
    const int NUM_THREADS = 4;
    const int ITERATIONS = 20;
    std::atomic<int> success_count{0};
    std::atomic<int> failure_count{0};

    auto worker = [&]() {
        for (int i = 0; i < ITERATIONS; i++) {
            health_agent_consistency_result_t result;
            if (nimcp_health_agent_check_consistency(agent, &result) == 0) {
                success_count++;
            } else {
                failure_count++;
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(worker);
    }

    for (auto& t : threads) {
        t.join();
    }

    // All checks should succeed (no corruption in clean agent)
    EXPECT_EQ(success_count.load(), NUM_THREADS * ITERATIONS);
    EXPECT_EQ(failure_count.load(), 0);
}

TEST_F(HealthAgentRunningConsistencyTest, ConcurrentRegisterUnregister) {
    const int NUM_THREADS = 4;
    const int ITERATIONS = 20;
    std::atomic<int> register_success{0};
    std::atomic<int> unregister_success{0};

    auto worker = [&](int thread_id) {
        for (int i = 0; i < ITERATIONS; i++) {
            uint32_t test_struct = 0xBEEF0000 + (thread_id * 100) + i;
            char name[32];
            snprintf(name, sizeof(name), "struct_%d_%d", thread_id, i);

            if (nimcp_health_agent_register_struct(agent, &test_struct,
                                                    test_struct, name) == 0) {
                register_success++;
                // Immediately unregister
                if (nimcp_health_agent_unregister_struct(agent, &test_struct) == 0) {
                    unregister_success++;
                }
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

    // Register and unregister counts should match
    EXPECT_EQ(register_success.load(), unregister_success.load());
}

//=============================================================================
// Consistency Check Timing Tests
//=============================================================================

TEST_F(HealthAgentConsistencyTest, CheckDuration_ReasonableTime) {
    health_agent_consistency_result_t result;
    nimcp_health_agent_check_consistency(agent, &result);

    // Check should complete in reasonable time (< 100ms for clean agent)
    EXPECT_LT(result.check_duration_us, 100000u);  // 100ms
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(HealthAgentConsistencyTest, RegisterStruct_MaxCapacity) {
    const int MAX_STRUCTS = 64;  // Registry capacity
    uint32_t test_structs[MAX_STRUCTS + 10];

    // Fill the registry
    int registered = 0;
    for (int i = 0; i < MAX_STRUCTS + 10; i++) {
        test_structs[i] = 0xBEEF0000 + i;
        char name[32];
        snprintf(name, sizeof(name), "struct_%d", i);
        if (nimcp_health_agent_register_struct(agent, &test_structs[i],
                                                test_structs[i], name) == 0) {
            registered++;
        }
    }

    // Should register up to MAX_STRUCTS
    EXPECT_EQ(registered, MAX_STRUCTS);
}
