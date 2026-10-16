/**
 * @file test_heartbeat_module_e2e.cpp
 * @brief End-to-end tests for Phase 8 heartbeat module flow
 * @version 1.0.0
 * @date 2025-01-20
 *
 * WHAT: End-to-end tests for complete heartbeat monitoring workflow
 * WHY:  Verify heartbeat system works correctly in realistic scenarios
 * HOW:  Test full workflow from module connection through health monitoring
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <cstring>
#include <atomic>
#include <vector>
#include <mutex>

// Health agent header only - avoid module header conflicts
#include "utils/fault_tolerance/nimcp_health_agent.h"

//=============================================================================
// Forward declarations for module heartbeat setters
//=============================================================================

extern "C" {
    void bio_router_set_health_agent(nimcp_health_agent_t* agent);
    void nimcp_msg_router_set_health_agent(nimcp_health_agent_t* agent);
    void thalamic_router_set_health_agent(nimcp_health_agent_t* agent);
    void systems_consolidation_set_health_agent(nimcp_health_agent_t* agent);
}

//=============================================================================
// Test Fixtures
//=============================================================================

/**
 * @brief Fixture for heartbeat module E2E tests
 */
class HeartbeatModuleE2ETest : public ::testing::Test {
protected:
    nimcp_health_agent_t* agent = nullptr;
    health_agent_config_t config;

    // Test tracking
    std::atomic<uint64_t> total_heartbeats{0};
    std::atomic<bool> test_running{false};

    void SetUp() override {
        nimcp_health_agent_default_config(&config);
        config.check_interval_ms = 50;
        config.enable_auto_recovery = false;
        config.heartbeat_interval_ms = 100;
        config.heartbeat_detector.enabled = true;
        config.watchdog_timeout_ms = 1000;

        agent = nimcp_health_agent_create(&config);
        ASSERT_NE(agent, nullptr) << "Failed to create health agent";

        total_heartbeats = 0;
        test_running = false;
    }

    void TearDown() override {
        test_running = false;

        // Clear all module health agents
        bio_router_set_health_agent(nullptr);
        nimcp_msg_router_set_health_agent(nullptr);
        thalamic_router_set_health_agent(nullptr);
        systems_consolidation_set_health_agent(nullptr);

        if (agent) {
            if (nimcp_health_agent_is_running(agent)) {
                nimcp_health_agent_stop(agent);
            }
            nimcp_health_agent_destroy(agent);
            agent = nullptr;
        }
    }

    uint64_t getHeartbeatCount() {
        health_agent_stats_t stats;
        nimcp_health_agent_get_stats(agent, &stats);
        return stats.heartbeats_received;
    }

    uint64_t getHeartbeatTimeouts() {
        health_agent_stats_t stats;
        nimcp_health_agent_get_stats(agent, &stats);
        return stats.heartbeat_timeouts;
    }
};

//=============================================================================
// Full Workflow E2E Tests
//=============================================================================

TEST_F(HeartbeatModuleE2ETest, FullWorkflow_ConnectStartMonitorStop) {
    // Step 1: Connect all modules to health agent
    bio_router_set_health_agent(agent);
    nimcp_msg_router_set_health_agent(agent);
    thalamic_router_set_health_agent(agent);
    systems_consolidation_set_health_agent(agent);

    // Step 2: Start health agent
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));

    // Step 3: Simulate module operations sending heartbeats
    const char* operations[] = {
        "bio_router_process",
        "msg_router_dispatch",
        "thalamic_route",
        "consolidation_replay"
    };

    for (int cycle = 0; cycle < 10; cycle++) {
        for (int op = 0; op < 4; op++) {
            nimcp_health_agent_heartbeat_ex(agent, operations[op],
                                            (float)cycle / 10.0f);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // Step 4: Verify heartbeats were received
    uint64_t heartbeats = getHeartbeatCount();
    EXPECT_GE(heartbeats, 40u);  // 10 cycles * 4 operations

    // Step 5: Stop health agent
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
    EXPECT_FALSE(nimcp_health_agent_is_running(agent));

    // Step 6: Disconnect modules
    bio_router_set_health_agent(nullptr);
    nimcp_msg_router_set_health_agent(nullptr);
    thalamic_router_set_health_agent(nullptr);
    systems_consolidation_set_health_agent(nullptr);
}

//=============================================================================
// Concurrent Module Simulation E2E Tests
//=============================================================================

TEST_F(HeartbeatModuleE2ETest, ConcurrentModules_SimulatedWorkloads) {
    // Connect modules
    bio_router_set_health_agent(agent);
    nimcp_msg_router_set_health_agent(agent);
    thalamic_router_set_health_agent(agent);
    systems_consolidation_set_health_agent(agent);

    // Start health agent
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Create threads simulating different module workloads
    test_running = true;
    std::vector<std::thread> module_threads;

    // Bio router simulation
    module_threads.emplace_back([this]() {
        while (test_running.load()) {
            nimcp_health_agent_heartbeat_ex(agent, "bio_process_inbox", 0.5f);
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }
    });

    // Msg router simulation
    module_threads.emplace_back([this]() {
        while (test_running.load()) {
            nimcp_health_agent_heartbeat_ex(agent, "msg_process_queue", 0.5f);
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }
    });

    // Thalamic router simulation
    module_threads.emplace_back([this]() {
        while (test_running.load()) {
            nimcp_health_agent_heartbeat_ex(agent, "thalamic_route", 0.5f);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    });

    // Consolidation simulation
    module_threads.emplace_back([this]() {
        while (test_running.load()) {
            nimcp_health_agent_heartbeat_ex(agent, "consolidation_replay", 0.5f);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    });

    // Let modules run for a while
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Stop modules
    test_running = false;
    for (auto& t : module_threads) {
        t.join();
    }

    // Verify many heartbeats received
    uint64_t heartbeats = getHeartbeatCount();
    EXPECT_GT(heartbeats, 30u);  // Should have many heartbeats

    // Verify no timeouts (modules were actively sending heartbeats)
    uint64_t timeouts = getHeartbeatTimeouts();
    EXPECT_EQ(timeouts, 0u);

    // Stop health agent
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Long-Running Operation Simulation E2E Tests
//=============================================================================

TEST_F(HeartbeatModuleE2ETest, LongRunningOperation_ProgressTracking) {
    // Connect module
    systems_consolidation_set_health_agent(agent);

    // Start health agent
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Simulate a long-running consolidation operation with progress updates
    const int TOTAL_STEPS = 100;
    for (int step = 0; step <= TOTAL_STEPS; step++) {
        float progress = (float)step / (float)TOTAL_STEPS;
        nimcp_health_agent_heartbeat_ex(agent, "long_consolidation", progress);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    // Verify all progress heartbeats received
    uint64_t heartbeats = getHeartbeatCount();
    EXPECT_GE(heartbeats, 100u);

    // Stop health agent
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Module Failure Simulation E2E Tests
//=============================================================================

TEST_F(HeartbeatModuleE2ETest, ModuleDisconnectMidOperation_GracefulHandling) {
    // Connect module
    bio_router_set_health_agent(agent);

    // Start health agent
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Send some heartbeats
    for (int i = 0; i < 5; i++) {
        nimcp_health_agent_heartbeat_ex(agent, "normal_op", 0.5f);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    uint64_t mid_count = getHeartbeatCount();
    EXPECT_GE(mid_count, 5u);

    // Disconnect module mid-operation
    bio_router_set_health_agent(nullptr);

    // Module's internal heartbeat calls should now be no-ops
    // System should remain stable

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // System should still be running
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));

    // Stop health agent
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Restart Resilience E2E Tests
//=============================================================================

TEST_F(HeartbeatModuleE2ETest, AgentRestartCycle_ModulesReconnect) {
    // Connect modules
    bio_router_set_health_agent(agent);
    thalamic_router_set_health_agent(agent);

    for (int cycle = 0; cycle < 3; cycle++) {
        // Start agent
        EXPECT_EQ(nimcp_health_agent_start(agent), 0);

        // Send heartbeats
        for (int i = 0; i < 10; i++) {
            nimcp_health_agent_heartbeat_ex(agent, "cycle_test", (float)i / 10.0f);
        }

        // Stop agent
        EXPECT_EQ(nimcp_health_agent_stop(agent), 0);

        // Small delay between cycles
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // Test for stability across restart cycles
    SUCCEED();
}

//=============================================================================
// Stress E2E Tests
//=============================================================================

TEST_F(HeartbeatModuleE2ETest, HighFrequencyHeartbeats_NoOverflow) {
    // Connect module
    bio_router_set_health_agent(agent);

    // Start health agent
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Send many heartbeats rapidly
    const int HEARTBEAT_COUNT = 10000;
    for (int i = 0; i < HEARTBEAT_COUNT; i++) {
        nimcp_health_agent_heartbeat_ex(agent, "stress_test",
                                        (float)(i % 100) / 100.0f);
    }

    // Verify all heartbeats counted
    uint64_t heartbeats = getHeartbeatCount();
    EXPECT_GE(heartbeats, (uint64_t)HEARTBEAT_COUNT);

    // Stop health agent
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HeartbeatModuleE2ETest, ManyModulesHighFrequency_StablePerformance) {
    // Connect all modules
    bio_router_set_health_agent(agent);
    nimcp_msg_router_set_health_agent(agent);
    thalamic_router_set_health_agent(agent);
    systems_consolidation_set_health_agent(agent);

    // Start health agent
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // High frequency heartbeats from multiple "modules"
    test_running = true;
    std::vector<std::thread> threads;

    for (int m = 0; m < 8; m++) {
        threads.emplace_back([this, m]() {
            char op_name[32];
            snprintf(op_name, sizeof(op_name), "module_%d_op", m);
            int count = 0;
            while (test_running.load() && count < 1000) {
                nimcp_health_agent_heartbeat_ex(agent, op_name, 0.5f);
                count++;
            }
        });
    }

    // Let run briefly
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    test_running = false;

    for (auto& t : threads) {
        t.join();
    }

    // Verify high throughput
    uint64_t heartbeats = getHeartbeatCount();
    EXPECT_GT(heartbeats, 1000u);

    // Stop health agent
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// End-to-End Health Monitoring Scenario
//=============================================================================

TEST_F(HeartbeatModuleE2ETest, CompleteHealthMonitoringScenario) {
    // This test simulates a complete monitoring scenario:
    // 1. Initialize health agent with reasonable production-like settings
    // 2. Connect multiple modules
    // 3. Run simulated workloads
    // 4. Verify health status remains good
    // 5. Clean shutdown

    // Step 1: Configure for production-like settings
    health_agent_config_t prod_config;
    nimcp_health_agent_default_config(&prod_config);
    prod_config.check_interval_ms = 100;
    prod_config.heartbeat_interval_ms = 200;
    prod_config.heartbeat_detector.enabled = true;
    prod_config.watchdog_timeout_ms = 2000;

    // Recreate agent with production config
    nimcp_health_agent_destroy(agent);
    agent = nimcp_health_agent_create(&prod_config);
    ASSERT_NE(agent, nullptr);

    // Step 2: Connect modules
    bio_router_set_health_agent(agent);
    nimcp_msg_router_set_health_agent(agent);
    thalamic_router_set_health_agent(agent);
    systems_consolidation_set_health_agent(agent);

    // Step 3: Start monitoring
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Step 4: Run simulated workloads
    test_running = true;
    std::vector<std::thread> workload_threads;

    // Simulate message processing workload
    workload_threads.emplace_back([this]() {
        while (test_running.load()) {
            nimcp_health_agent_heartbeat_ex(agent, "msg_processing", 0.5f);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    });

    // Simulate routing workload
    workload_threads.emplace_back([this]() {
        while (test_running.load()) {
            nimcp_health_agent_heartbeat_ex(agent, "signal_routing", 0.5f);
            std::this_thread::sleep_for(std::chrono::milliseconds(75));
        }
    });

    // Simulate consolidation workload
    workload_threads.emplace_back([this]() {
        float progress = 0.0f;
        while (test_running.load()) {
            nimcp_health_agent_heartbeat_ex(agent, "memory_consolidation", progress);
            progress += 0.1f;
            if (progress > 1.0f) progress = 0.0f;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });

    // Run for 1 second
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Step 5: Verify health
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GT(stats.heartbeats_received, 20u);  // Should have many heartbeats
    EXPECT_EQ(stats.heartbeat_timeouts, 0u);    // No timeouts

    // Step 6: Clean shutdown
    test_running = false;
    for (auto& t : workload_threads) {
        t.join();
    }

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Operation Name Variety Tests
//=============================================================================

TEST_F(HeartbeatModuleE2ETest, VariousOperationNames_AllTracked) {
    // Start health agent
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Test various operation names matching actual module operations
    std::vector<const char*> operations = {
        "process_inbox",           // bio_router
        "process_queue",           // msg_router
        "thalamic_process_queue",  // thalamic_router
        "consolidation_update",    // systems_consolidation
        "consolidation_replays",   // systems_consolidation
        "training_step",           // training
        "brain_decide",            // brain_core
        "model_loader",            // model_loader
        "tracker_update"           // dragonfly
    };

    uint64_t baseline = getHeartbeatCount();

    for (const char* op : operations) {
        for (int i = 0; i < 10; i++) {
            nimcp_health_agent_heartbeat_ex(agent, op, (float)i / 10.0f);
        }
    }

    uint64_t after = getHeartbeatCount();
    EXPECT_GE(after, baseline + operations.size() * 10);

    // Stop health agent
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}
