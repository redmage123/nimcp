/**
 * @file test_health_agent_consistency_e2e.cpp
 * @brief End-to-end tests for NIMCP Health Agent State Consistency Manager (Phase 3)
 * @version 1.0.0
 * @date 2025-01-18
 *
 * WHAT: End-to-end tests for complete consistency checking workflows
 * WHY:  Verify consistency system works correctly in realistic scenarios
 * HOW:  Simulate real-world usage patterns with running agent
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <cstring>
#include <atomic>
#include <vector>
#include <random>

// Health agent header
#include "utils/fault_tolerance/nimcp_health_agent.h"

//=============================================================================
// Test Fixtures
//=============================================================================

/**
 * @brief Fixture for health agent consistency e2e tests
 */
class HealthAgentConsistencyE2ETest : public ::testing::Test {
protected:
    nimcp_health_agent_t* agent = nullptr;
    health_agent_config_t config;

    void SetUp() override {
        nimcp_health_agent_default_config(&config);
        config.check_interval_ms = 50;
        config.enable_auto_recovery = false;

        // Enable all consistency checks
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
// Full Lifecycle E2E Tests
//=============================================================================

TEST_F(HealthAgentConsistencyE2ETest, FullLifecycle_CreateRunCheckStop) {
    // Complete lifecycle: create -> start -> check -> stop -> destroy

    // Start agent
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));

    // Let agent run and perform periodic checks
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Explicit consistency check
    health_agent_consistency_result_t result;
    EXPECT_EQ(nimcp_health_agent_check_consistency(agent, &result), 0);
    EXPECT_TRUE(result.overall_passed);

    // Stop agent
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
    EXPECT_FALSE(nimcp_health_agent_is_running(agent));

    // Final check after stop
    EXPECT_EQ(nimcp_health_agent_check_consistency(agent, &result), 0);
    EXPECT_TRUE(result.overall_passed);
}

TEST_F(HealthAgentConsistencyE2ETest, FullLifecycle_MultipleRestarts) {
    // Test multiple start/stop cycles with consistency checks

    for (int cycle = 0; cycle < 3; cycle++) {
        // Start
        EXPECT_EQ(nimcp_health_agent_start(agent), 0);
        EXPECT_TRUE(nimcp_health_agent_is_running(agent));

        // Run for a bit
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Check consistency
        health_agent_consistency_result_t result;
        EXPECT_EQ(nimcp_health_agent_check_consistency(agent, &result), 0);
        EXPECT_TRUE(result.overall_passed);

        // Stop
        EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
        EXPECT_FALSE(nimcp_health_agent_is_running(agent));
    }
}

//=============================================================================
// Struct Registry Workflow E2E Tests
//=============================================================================

TEST_F(HealthAgentConsistencyE2ETest, StructRegistry_FullWorkflow) {
    // Simulate real-world struct registration workflow
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Create some "module" structures
    struct TestModule {
        uint32_t magic;
        char name[32];
        int data[100];
    };

    const int NUM_MODULES = 10;
    TestModule modules[NUM_MODULES];

    // Initialize and register modules
    for (int i = 0; i < NUM_MODULES; i++) {
        modules[i].magic = 0xA0D00000 + i;
        snprintf(modules[i].name, sizeof(modules[i].name), "Module_%d", i);

        EXPECT_EQ(nimcp_health_agent_register_struct(agent, &modules[i],
                                                      modules[i].magic,
                                                      modules[i].name), 0);
    }

    // Run for a bit with registered modules
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Verify all modules pass consistency check
    health_agent_consistency_result_t result;
    EXPECT_EQ(nimcp_health_agent_check_consistency(agent, &result), 0);
    EXPECT_TRUE(result.magic_check_passed);

    // Simulate module shutdown - unregister in reverse order
    for (int i = NUM_MODULES - 1; i >= 0; i--) {
        EXPECT_EQ(nimcp_health_agent_unregister_struct(agent, &modules[i]), 0);
    }

    // Final check should still pass
    EXPECT_EQ(nimcp_health_agent_check_consistency(agent, &result), 0);
    EXPECT_TRUE(result.overall_passed);

    nimcp_health_agent_stop(agent);
}

TEST_F(HealthAgentConsistencyE2ETest, StructRegistry_CorruptionDetection) {
    // Simulate corruption detection workflow
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Create and register a module
    struct CorruptibleModule {
        uint32_t magic;
        char data[256];
    };

    CorruptibleModule module;
    module.magic = 0x600DA61C;
    EXPECT_EQ(nimcp_health_agent_register_struct(agent, &module, 0x600DA61C, "corruptible"), 0);

    // Verify it passes initially
    health_agent_consistency_result_t result;
    EXPECT_EQ(nimcp_health_agent_check_consistency(agent, &result), 0);
    EXPECT_TRUE(result.magic_check_passed);

    // Simulate memory corruption (e.g., buffer overflow from neighbor)
    module.magic = 0xC00DBAD0;

    // Consistency check should detect corruption
    int check_result = nimcp_health_agent_check_consistency(agent, &result);
    EXPECT_EQ(check_result, -1);
    EXPECT_FALSE(result.magic_check_passed);
    EXPECT_GT(result.magic_violations, 0u);

    // Fix the corruption
    module.magic = 0x600DA61C;

    // Should pass again
    EXPECT_EQ(nimcp_health_agent_check_consistency(agent, &result), 0);
    EXPECT_TRUE(result.magic_check_passed);

    nimcp_health_agent_unregister_struct(agent, &module);
    nimcp_health_agent_stop(agent);
}

//=============================================================================
// Concurrent Operations E2E Tests
//=============================================================================

TEST_F(HealthAgentConsistencyE2ETest, ConcurrentOperations_SimulatedWorkload) {
    // Simulate realistic concurrent workload
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    const int DURATION_MS = 2000;
    const int NUM_WORKERS = 4;
    std::atomic<bool> stop_flag{false};
    std::atomic<int> heartbeat_count{0};
    std::atomic<int> check_count{0};
    std::atomic<int> register_count{0};

    // Heartbeat worker - simulates main application heartbeat
    auto heartbeat_worker = [&]() {
        while (!stop_flag.load()) {
            nimcp_health_agent_heartbeat(agent);
            heartbeat_count++;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    };

    // Check worker - simulates periodic health checks
    auto check_worker = [&]() {
        while (!stop_flag.load()) {
            health_agent_consistency_result_t result;
            if (nimcp_health_agent_check_consistency(agent, &result) == 0) {
                check_count++;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    };

    // Register worker - simulates dynamic module loading/unloading
    auto register_worker = [&](int id) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(10, 50);

        while (!stop_flag.load()) {
            uint32_t test_struct = 0xD1100000 + id;
            char name[32];
            snprintf(name, sizeof(name), "dynamic_%d", id);

            if (nimcp_health_agent_register_struct(agent, &test_struct,
                                                    test_struct, name) == 0) {
                register_count++;
                std::this_thread::sleep_for(std::chrono::milliseconds(dis(gen)));
                nimcp_health_agent_unregister_struct(agent, &test_struct);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(dis(gen)));
        }
    };

    // Start workers
    std::vector<std::thread> threads;
    threads.emplace_back(heartbeat_worker);
    threads.emplace_back(check_worker);
    for (int i = 0; i < NUM_WORKERS; i++) {
        threads.emplace_back(register_worker, i);
    }

    // Let it run
    std::this_thread::sleep_for(std::chrono::milliseconds(DURATION_MS));
    stop_flag.store(true);

    // Wait for all workers to finish
    for (auto& t : threads) {
        t.join();
    }

    // Final verification
    EXPECT_GT(heartbeat_count.load(), 0);
    EXPECT_GT(check_count.load(), 0);
    EXPECT_GT(register_count.load(), 0);

    // Final consistency check
    health_agent_consistency_result_t result;
    EXPECT_EQ(nimcp_health_agent_check_consistency(agent, &result), 0);
    EXPECT_TRUE(result.overall_passed);

    nimcp_health_agent_stop(agent);
}

//=============================================================================
// Configuration Change E2E Tests
//=============================================================================

TEST_F(HealthAgentConsistencyE2ETest, ConfigChange_DynamicReconfiguration) {
    // Simulate dynamic configuration changes during operation
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Initial configuration - all checks enabled
    health_agent_consistency_result_t result;
    EXPECT_EQ(nimcp_health_agent_check_consistency(agent, &result), 0);
    EXPECT_TRUE(result.overall_passed);

    // Disable some checks
    health_agent_consistency_config_t new_config;
    memset(&new_config, 0, sizeof(new_config));
    new_config.check_reference_counts = true;
    new_config.check_pointer_canaries = false;  // Disabled
    new_config.check_struct_magic = true;
    new_config.check_mutex_state = false;       // Disabled
    new_config.check_circular_buffers = true;
    new_config.consistency_check_interval_ms = 200;

    EXPECT_EQ(nimcp_health_agent_update_consistency_config(agent, &new_config), 0);

    // Let agent run with new config
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Should still pass
    EXPECT_EQ(nimcp_health_agent_check_consistency(agent, &result), 0);
    EXPECT_TRUE(result.overall_passed);

    // Re-enable all checks
    new_config.check_pointer_canaries = true;
    new_config.check_mutex_state = true;
    new_config.check_kg_consistency = true;
    new_config.check_neuron_values = true;

    EXPECT_EQ(nimcp_health_agent_update_consistency_config(agent, &new_config), 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Should still pass with all checks
    EXPECT_EQ(nimcp_health_agent_check_consistency(agent, &result), 0);
    EXPECT_TRUE(result.overall_passed);

    nimcp_health_agent_stop(agent);
}

//=============================================================================
// Stress E2E Tests
//=============================================================================

TEST_F(HealthAgentConsistencyE2ETest, Stress_HighFrequencyChecks) {
    // Stress test with high-frequency consistency checks
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    const int NUM_CHECKS = 100;
    int passed = 0;
    int failed = 0;

    for (int i = 0; i < NUM_CHECKS; i++) {
        health_agent_consistency_result_t result;
        if (nimcp_health_agent_check_consistency(agent, &result) == 0) {
            if (result.overall_passed) {
                passed++;
            } else {
                failed++;
            }
        } else {
            failed++;
        }
    }

    // All checks should pass on a healthy agent
    EXPECT_EQ(passed, NUM_CHECKS);
    EXPECT_EQ(failed, 0);

    nimcp_health_agent_stop(agent);
}

TEST_F(HealthAgentConsistencyE2ETest, Stress_RegistryChurn) {
    // Stress test with high registry churn
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    const int ITERATIONS = 50;
    uint32_t test_structs[10];

    for (int iter = 0; iter < ITERATIONS; iter++) {
        // Register batch
        for (int i = 0; i < 10; i++) {
            test_structs[i] = 0xC4000000 + (iter * 10) + i;
            char name[32];
            snprintf(name, sizeof(name), "churn_%d_%d", iter, i);
            nimcp_health_agent_register_struct(agent, &test_structs[i],
                                                test_structs[i], name);
        }

        // Verify consistency
        health_agent_consistency_result_t result;
        EXPECT_EQ(nimcp_health_agent_check_consistency(agent, &result), 0);
        EXPECT_TRUE(result.magic_check_passed);

        // Unregister batch
        for (int i = 0; i < 10; i++) {
            nimcp_health_agent_unregister_struct(agent, &test_structs[i]);
        }
    }

    nimcp_health_agent_stop(agent);
}

//=============================================================================
// Recovery E2E Tests
//=============================================================================

TEST_F(HealthAgentConsistencyE2ETest, Recovery_AfterCorruption) {
    // Simulate recovery workflow after detecting corruption
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    uint32_t module1 = 0xAABB0001;
    uint32_t module2 = 0xAABB0002;
    uint32_t module3 = 0xAABB0003;

    nimcp_health_agent_register_struct(agent, &module1, module1, "module1");
    nimcp_health_agent_register_struct(agent, &module2, module2, "module2");
    nimcp_health_agent_register_struct(agent, &module3, module3, "module3");

    // Verify healthy state
    health_agent_consistency_result_t result;
    EXPECT_EQ(nimcp_health_agent_check_consistency(agent, &result), 0);
    EXPECT_TRUE(result.overall_passed);

    // Simulate corruption in module2
    module2 = 0xBADC0DE0;

    // Detect corruption
    EXPECT_EQ(nimcp_health_agent_check_consistency(agent, &result), -1);
    EXPECT_FALSE(result.magic_check_passed);

    // Recovery: unregister corrupted module (simulates module restart)
    nimcp_health_agent_unregister_struct(agent, &module2);

    // Verify partial recovery
    EXPECT_EQ(nimcp_health_agent_check_consistency(agent, &result), 0);
    EXPECT_TRUE(result.overall_passed);

    // Re-initialize module2
    module2 = 0xAABB0002;
    nimcp_health_agent_register_struct(agent, &module2, module2, "module2");

    // Full recovery
    EXPECT_EQ(nimcp_health_agent_check_consistency(agent, &result), 0);
    EXPECT_TRUE(result.overall_passed);

    // Cleanup
    nimcp_health_agent_unregister_struct(agent, &module1);
    nimcp_health_agent_unregister_struct(agent, &module2);
    nimcp_health_agent_unregister_struct(agent, &module3);

    nimcp_health_agent_stop(agent);
}

//=============================================================================
// Stats Accumulation E2E Tests
//=============================================================================

TEST_F(HealthAgentConsistencyE2ETest, Stats_AccumulateOverTime) {
    // Verify stats accumulate correctly over time
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Get initial stats
    health_agent_stats_t initial_stats;
    nimcp_health_agent_get_stats(agent, &initial_stats);

    // Perform operations
    const int HEARTBEATS = 20;
    const int CHECKS = 5;

    for (int i = 0; i < HEARTBEATS; i++) {
        nimcp_health_agent_heartbeat(agent);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    for (int i = 0; i < CHECKS; i++) {
        health_agent_consistency_result_t result;
        nimcp_health_agent_check_consistency(agent, &result);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Get final stats
    health_agent_stats_t final_stats;
    nimcp_health_agent_get_stats(agent, &final_stats);

    // Verify accumulation
    EXPECT_GE(final_stats.heartbeats_received,
              initial_stats.heartbeats_received + HEARTBEATS);
    EXPECT_GT(final_stats.uptime_ms, initial_stats.uptime_ms);

    nimcp_health_agent_stop(agent);
}
