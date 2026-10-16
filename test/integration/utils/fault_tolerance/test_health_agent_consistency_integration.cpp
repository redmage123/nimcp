/**
 * @file test_health_agent_consistency_integration.cpp
 * @brief Integration tests for NIMCP Health Agent State Consistency Manager (Phase 3)
 * @version 1.0.0
 * @date 2025-01-18
 *
 * WHAT: Test health agent consistency checks with running agent and connected modules
 * WHY:  Verify consistency checks work correctly with real module interactions
 * HOW:  Run agent, perform operations, verify consistency checks pass/fail appropriately
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
 * @brief Base fixture for health agent consistency integration tests
 */
class HealthAgentConsistencyIntegrationTest : public ::testing::Test {
protected:
    nimcp_health_agent_t* agent = nullptr;
    health_agent_config_t config;

    void SetUp() override {
        nimcp_health_agent_default_config(&config);
        config.check_interval_ms = 50;
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

/**
 * @brief Fixture with running agent for consistency integration tests
 */
class RunningAgentConsistencyIntegrationTest : public HealthAgentConsistencyIntegrationTest {
protected:
    void SetUp() override {
        HealthAgentConsistencyIntegrationTest::SetUp();
        ASSERT_EQ(nimcp_health_agent_start(agent), 0);
        ASSERT_TRUE(nimcp_health_agent_is_running(agent));
    }

    void TearDown() override {
        if (agent && nimcp_health_agent_is_running(agent)) {
            nimcp_health_agent_stop(agent);
        }
        HealthAgentConsistencyIntegrationTest::TearDown();
    }
};

//=============================================================================
// Basic Integration Tests
//=============================================================================

TEST_F(HealthAgentConsistencyIntegrationTest, AgentCreatedWithConsistencyConfig) {
    // Agent should be created with consistency config enabled
    health_agent_consistency_result_t result;
    EXPECT_EQ(nimcp_health_agent_check_consistency(agent, &result), 0);
    EXPECT_TRUE(result.overall_passed);
}

TEST_F(HealthAgentConsistencyIntegrationTest, StartStopWithConsistencyChecks) {
    // Start agent
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));

    // Wait for some consistency checks
    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    // Stop agent
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
    EXPECT_FALSE(nimcp_health_agent_is_running(agent));

    // Final consistency check should still pass
    health_agent_consistency_result_t result;
    EXPECT_EQ(nimcp_health_agent_check_consistency(agent, &result), 0);
    EXPECT_TRUE(result.overall_passed);
}

//=============================================================================
// Running Agent Consistency Tests
//=============================================================================

TEST_F(RunningAgentConsistencyIntegrationTest, PeriodicConsistencyChecks) {
    // Let agent run for a few check intervals
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Verify agent still healthy
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));

    // Explicit check should pass
    health_agent_consistency_result_t result;
    EXPECT_EQ(nimcp_health_agent_check_consistency(agent, &result), 0);
    EXPECT_TRUE(result.overall_passed);
}

TEST_F(RunningAgentConsistencyIntegrationTest, RequestCheckWhileRunning) {
    // Request explicit check
    EXPECT_EQ(nimcp_health_agent_request_check(agent), 0);

    // Wait for check to be processed
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Get status
    health_agent_consistency_result_t result;
    EXPECT_EQ(nimcp_health_agent_get_consistency_status(agent, &result), 0);
}

TEST_F(RunningAgentConsistencyIntegrationTest, HeartbeatDoesNotCorruptState) {
    // Send heartbeats while running
    for (int i = 0; i < 20; i++) {
        nimcp_health_agent_heartbeat(agent);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Consistency check should still pass
    health_agent_consistency_result_t result;
    EXPECT_EQ(nimcp_health_agent_check_consistency(agent, &result), 0);
    EXPECT_TRUE(result.overall_passed);
}

TEST_F(RunningAgentConsistencyIntegrationTest, RegisterStructWhileRunning) {
    uint32_t test_struct = 0xDEADBEEF;

    // Register struct while agent is running
    EXPECT_EQ(nimcp_health_agent_register_struct(agent, &test_struct,
                                                  0xDEADBEEF, "test_struct"), 0);

    // Wait for a check cycle
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // Consistency check should pass
    health_agent_consistency_result_t result;
    EXPECT_EQ(nimcp_health_agent_check_consistency(agent, &result), 0);
    EXPECT_TRUE(result.magic_check_passed);

    // Clean up
    EXPECT_EQ(nimcp_health_agent_unregister_struct(agent, &test_struct), 0);
}

TEST_F(RunningAgentConsistencyIntegrationTest, UnregisterStructWhileRunning) {
    uint32_t test_struct = 0xCAFEBABE;

    // Register, wait, unregister
    EXPECT_EQ(nimcp_health_agent_register_struct(agent, &test_struct,
                                                  0xCAFEBABE, "cafe_struct"), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(nimcp_health_agent_unregister_struct(agent, &test_struct), 0);

    // Consistency check should pass
    health_agent_consistency_result_t result;
    EXPECT_EQ(nimcp_health_agent_check_consistency(agent, &result), 0);
    EXPECT_TRUE(result.overall_passed);
}

//=============================================================================
// Concurrent Access Tests
//=============================================================================

TEST_F(RunningAgentConsistencyIntegrationTest, ConcurrentHeartbeatsAndChecks) {
    const int NUM_THREADS = 4;
    const int ITERATIONS = 50;
    std::atomic<int> heartbeat_count{0};
    std::atomic<int> check_count{0};

    auto heartbeat_worker = [&]() {
        for (int i = 0; i < ITERATIONS; i++) {
            nimcp_health_agent_heartbeat(agent);
            heartbeat_count++;
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    };

    auto check_worker = [&]() {
        for (int i = 0; i < ITERATIONS / 10; i++) {
            health_agent_consistency_result_t result;
            nimcp_health_agent_check_consistency(agent, &result);
            check_count++;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(heartbeat_worker);
        threads.emplace_back(check_worker);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(heartbeat_count.load(), NUM_THREADS * ITERATIONS);

    // Final check should pass
    health_agent_consistency_result_t result;
    EXPECT_EQ(nimcp_health_agent_check_consistency(agent, &result), 0);
    EXPECT_TRUE(result.overall_passed);
}

TEST_F(RunningAgentConsistencyIntegrationTest, ConcurrentStructRegistration) {
    const int NUM_THREADS = 4;
    const int STRUCTS_PER_THREAD = 10;
    std::atomic<int> success_count{0};

    auto worker = [&](int thread_id) {
        uint32_t structs[STRUCTS_PER_THREAD];
        for (int i = 0; i < STRUCTS_PER_THREAD; i++) {
            structs[i] = 0xABCD0000 + (thread_id * 100) + i;
            char name[32];
            snprintf(name, sizeof(name), "t%d_s%d", thread_id, i);

            if (nimcp_health_agent_register_struct(agent, &structs[i],
                                                    structs[i], name) == 0) {
                success_count++;
            }

            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }

        // Unregister all
        for (int i = 0; i < STRUCTS_PER_THREAD; i++) {
            nimcp_health_agent_unregister_struct(agent, &structs[i]);
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(worker, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    // Should have registered at least some (up to 64 total capacity)
    EXPECT_GT(success_count.load(), 0);

    // Final check should pass
    health_agent_consistency_result_t result;
    EXPECT_EQ(nimcp_health_agent_check_consistency(agent, &result), 0);
    EXPECT_TRUE(result.overall_passed);
}

//=============================================================================
// Config Update Integration Tests
//=============================================================================

TEST_F(RunningAgentConsistencyIntegrationTest, UpdateConfigWhileRunning) {
    // Update consistency config while running
    health_agent_consistency_config_t new_config;
    memset(&new_config, 0, sizeof(new_config));
    new_config.check_reference_counts = true;
    new_config.check_pointer_canaries = false;  // Disable canary checks
    new_config.check_struct_magic = true;
    new_config.consistency_check_interval_ms = 50;

    EXPECT_EQ(nimcp_health_agent_update_consistency_config(agent, &new_config), 0);

    // Wait for new config to take effect
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // Should still be running
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));
}

TEST_F(RunningAgentConsistencyIntegrationTest, DisableAllChecksWhileRunning) {
    // Disable all consistency checks
    health_agent_consistency_config_t disabled_config;
    memset(&disabled_config, 0, sizeof(disabled_config));
    // All checks are false by default after memset

    EXPECT_EQ(nimcp_health_agent_update_consistency_config(agent, &disabled_config), 0);

    // Wait and verify agent still running
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));
}

//=============================================================================
// Long-Running Tests
//=============================================================================

TEST_F(RunningAgentConsistencyIntegrationTest, ExtendedOperation) {
    const int DURATION_MS = 1000;  // 1 second
    const int CHECK_INTERVAL_MS = 100;

    auto start = std::chrono::steady_clock::now();

    while (std::chrono::steady_clock::now() - start <
           std::chrono::milliseconds(DURATION_MS)) {
        // Periodic heartbeats
        nimcp_health_agent_heartbeat(agent);

        // Occasional explicit checks
        health_agent_consistency_result_t result;
        if (nimcp_health_agent_check_consistency(agent, &result) != 0) {
            FAIL() << "Consistency check failed during extended operation";
        }
        EXPECT_TRUE(result.overall_passed);

        std::this_thread::sleep_for(std::chrono::milliseconds(CHECK_INTERVAL_MS));
    }

    EXPECT_TRUE(nimcp_health_agent_is_running(agent));
}

//=============================================================================
// Stats Integration Tests
//=============================================================================

TEST_F(RunningAgentConsistencyIntegrationTest, StatsAccumulateWithChecks) {
    // Get initial stats
    health_agent_stats_t initial_stats;
    nimcp_health_agent_get_stats(agent, &initial_stats);

    // Run for a bit with heartbeats
    for (int i = 0; i < 10; i++) {
        nimcp_health_agent_heartbeat(agent);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // Get final stats
    health_agent_stats_t final_stats;
    nimcp_health_agent_get_stats(agent, &final_stats);

    // Stats should have accumulated
    EXPECT_GE(final_stats.heartbeats_received, initial_stats.heartbeats_received + 10);
    EXPECT_GE(final_stats.checks_performed, initial_stats.checks_performed);
}

//=============================================================================
// Recovery After Stop Tests
//=============================================================================

TEST_F(HealthAgentConsistencyIntegrationTest, RestartWithConsistency) {
    // Start agent
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Stop agent
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
    EXPECT_FALSE(nimcp_health_agent_is_running(agent));

    // Check consistency while stopped
    health_agent_consistency_result_t result;
    EXPECT_EQ(nimcp_health_agent_check_consistency(agent, &result), 0);
    EXPECT_TRUE(result.overall_passed);

    // Start again
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));

    // Check consistency while running again
    EXPECT_EQ(nimcp_health_agent_check_consistency(agent, &result), 0);
    EXPECT_TRUE(result.overall_passed);
}
