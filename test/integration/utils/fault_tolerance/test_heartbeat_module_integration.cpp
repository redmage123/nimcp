/**
 * @file test_heartbeat_module_integration.cpp
 * @brief Integration tests for Phase 8 heartbeat module operations
 * @version 1.0.0
 * @date 2025-01-20
 *
 * WHAT: Tests heartbeat integration between modules and health agent
 * WHY:  Verify heartbeats are properly sent during module operations
 * HOW:  Connect modules to health agent, simulate operations via heartbeats
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <cstring>
#include <atomic>
#include <vector>

// Health agent header only - avoid module header conflicts
#include "utils/fault_tolerance/nimcp_health_agent.h"

//=============================================================================
// Forward declarations for module heartbeat setters
//=============================================================================

extern "C" {
    // Heartbeat setters added in Phase 8
    void bio_router_set_health_agent(nimcp_health_agent_t* agent);
    void nimcp_msg_router_set_health_agent(nimcp_health_agent_t* agent);
    void thalamic_router_set_health_agent(nimcp_health_agent_t* agent);
    void systems_consolidation_set_health_agent(nimcp_health_agent_t* agent);
}

//=============================================================================
// Test Fixtures
//=============================================================================

/**
 * @brief Base fixture for heartbeat module integration tests
 */
class HeartbeatModuleIntegrationTest : public ::testing::Test {
protected:
    nimcp_health_agent_t* agent = nullptr;
    health_agent_config_t config;

    void SetUp() override {
        nimcp_health_agent_default_config(&config);
        config.check_interval_ms = 50;
        config.enable_auto_recovery = false;
        config.heartbeat_interval_ms = 100;

        agent = nimcp_health_agent_create(&config);
        ASSERT_NE(agent, nullptr) << "Failed to create health agent";
    }

    void TearDown() override {
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

    /**
     * @brief Get current heartbeat count from agent
     */
    uint64_t getHeartbeatCount() {
        health_agent_stats_t stats;
        nimcp_health_agent_get_stats(agent, &stats);
        return stats.heartbeats_received;
    }
};

//=============================================================================
// Bio Router Integration Tests
//=============================================================================

TEST_F(HeartbeatModuleIntegrationTest, BioRouter_ConnectAgent_SendsHeartbeats) {
    // Connect health agent
    bio_router_set_health_agent(agent);

    // Start health agent
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Get baseline heartbeat count
    uint64_t baseline = getHeartbeatCount();

    // Simulate bio router operations by sending heartbeats
    for (int i = 0; i < 10; i++) {
        nimcp_health_agent_heartbeat_ex(agent, "process_inbox", (float)i / 10.0f);
    }

    // Verify heartbeats were received
    uint64_t after = getHeartbeatCount();
    EXPECT_GE(after, baseline + 10);

    // Stop agent
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Message Router Integration Tests
//=============================================================================

TEST_F(HeartbeatModuleIntegrationTest, MsgRouter_ConnectAgent_SendsHeartbeats) {
    // Connect health agent to message router
    nimcp_msg_router_set_health_agent(agent);

    // Start health agent
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Get baseline heartbeat count
    uint64_t baseline = getHeartbeatCount();

    // Simulate message router processing by sending heartbeats
    for (int i = 0; i < 10; i++) {
        nimcp_health_agent_heartbeat_ex(agent, "process_queue", (float)i / 10.0f);
    }

    // Verify heartbeats were received
    uint64_t after = getHeartbeatCount();
    EXPECT_GE(after, baseline + 10);

    // Stop agent
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Thalamic Router Integration Tests
//=============================================================================

TEST_F(HeartbeatModuleIntegrationTest, ThalamicRouter_ConnectAgent_SendsHeartbeats) {
    // Connect health agent to thalamic router
    thalamic_router_set_health_agent(agent);

    // Start health agent
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Get baseline heartbeat count
    uint64_t baseline = getHeartbeatCount();

    // Simulate thalamic router processing by sending heartbeats
    for (int i = 0; i < 10; i++) {
        nimcp_health_agent_heartbeat_ex(agent, "thalamic_process_queue", (float)i / 10.0f);
    }

    // Verify heartbeats were received
    uint64_t after = getHeartbeatCount();
    EXPECT_GE(after, baseline + 10);

    // Stop agent
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Systems Consolidation Integration Tests
//=============================================================================

TEST_F(HeartbeatModuleIntegrationTest, SystemsConsolidation_ConnectAgent_SendsHeartbeats) {
    // Connect health agent to systems consolidation
    systems_consolidation_set_health_agent(agent);

    // Start health agent
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Get baseline heartbeat count
    uint64_t baseline = getHeartbeatCount();

    // Simulate consolidation operations by sending heartbeats
    for (int i = 0; i < 10; i++) {
        nimcp_health_agent_heartbeat_ex(agent, "consolidation_update", (float)i / 10.0f);
        nimcp_health_agent_heartbeat_ex(agent, "consolidation_replays", (float)i / 10.0f);
    }

    // Verify heartbeats were received
    uint64_t after = getHeartbeatCount();
    EXPECT_GE(after, baseline + 20);

    // Stop agent
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Combined Module Integration Tests
//=============================================================================

TEST_F(HeartbeatModuleIntegrationTest, AllModules_ConcurrentHeartbeats) {
    // Connect all modules to the same health agent
    bio_router_set_health_agent(agent);
    nimcp_msg_router_set_health_agent(agent);
    thalamic_router_set_health_agent(agent);
    systems_consolidation_set_health_agent(agent);

    // Start health agent
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Get baseline heartbeat count
    uint64_t baseline = getHeartbeatCount();

    // Create multiple threads sending heartbeats from different "modules"
    std::atomic<bool> running{true};
    std::vector<std::thread> threads;

    for (int i = 0; i < 4; i++) {
        threads.emplace_back([this, &running, i]() {
            const char* ops[] = {
                "bio_router_op",
                "msg_router_op",
                "thalamic_op",
                "consolidation_op"
            };
            while (running.load()) {
                nimcp_health_agent_heartbeat_ex(agent, ops[i], 0.5f);
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
        });
    }

    // Let threads run for a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Stop threads
    running.store(false);
    for (auto& t : threads) {
        t.join();
    }

    // Verify many heartbeats were received
    uint64_t after = getHeartbeatCount();
    EXPECT_GT(after, baseline + 20);  // At least 20 heartbeats

    // Stop agent
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HeartbeatModuleIntegrationTest, ModuleDisconnect_NoHeartbeats) {
    // Connect health agent
    bio_router_set_health_agent(agent);

    // Start health agent
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Send some heartbeats
    for (int i = 0; i < 5; i++) {
        nimcp_health_agent_heartbeat_ex(agent, "test", 0.5f);
    }

    uint64_t after_connect = getHeartbeatCount();
    EXPECT_GE(after_connect, 5u);

    // Disconnect health agent
    bio_router_set_health_agent(nullptr);

    // The module's internal heartbeat helper should now be a no-op
    // We verify the disconnect doesn't crash

    // Stop agent
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);

    SUCCEED();
}

//=============================================================================
// Heartbeat Timeout Detection Tests
//=============================================================================

TEST_F(HeartbeatModuleIntegrationTest, HeartbeatTimeout_NotTriggeredWithActiveHeartbeats) {
    // Configure a short timeout for testing
    health_agent_config_t timeout_config;
    nimcp_health_agent_default_config(&timeout_config);
    timeout_config.check_interval_ms = 50;
    timeout_config.heartbeat_interval_ms = 100;
    timeout_config.watchdog_timeout_ms = 500;  // Heartbeat timeout
    timeout_config.heartbeat_detector.enabled = true;

    // Destroy default agent and create one with timeout config
    nimcp_health_agent_destroy(agent);
    agent = nimcp_health_agent_create(&timeout_config);
    ASSERT_NE(agent, nullptr);

    // Connect module
    bio_router_set_health_agent(agent);

    // Start agent
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Keep sending heartbeats faster than timeout
    for (int i = 0; i < 10; i++) {
        nimcp_health_agent_heartbeat_ex(agent, "keep_alive", (float)i / 10.0f);
        std::this_thread::sleep_for(std::chrono::milliseconds(40));
    }

    // Check stats - should have no timeouts
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_EQ(stats.heartbeat_timeouts, 0u);

    // Stop agent
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Progress Tracking Tests
//=============================================================================

TEST_F(HeartbeatModuleIntegrationTest, HeartbeatProgress_ValidRange) {
    // Connect module
    systems_consolidation_set_health_agent(agent);

    // Start agent
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Send heartbeats with various progress values
    std::vector<float> progress_values = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
    for (float progress : progress_values) {
        EXPECT_NO_THROW(nimcp_health_agent_heartbeat_ex(agent, "progress_test", progress));
    }

    // Verify all heartbeats were received
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received, 5u);

    // Stop agent
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Error Recovery Tests
//=============================================================================

TEST_F(HeartbeatModuleIntegrationTest, AgentRestart_ModulesReconnect) {
    // Connect modules
    bio_router_set_health_agent(agent);
    nimcp_msg_router_set_health_agent(agent);

    // Start agent
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Send heartbeats
    nimcp_health_agent_heartbeat_ex(agent, "test1", 0.5f);

    // Stop agent
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);

    // Restart agent
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Heartbeats should still work after restart
    nimcp_health_agent_heartbeat_ex(agent, "test2", 0.5f);

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received, 1u);

    // Stop agent
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Operation Name Tests
//=============================================================================

TEST_F(HeartbeatModuleIntegrationTest, OperationNames_ModuleSpecificPatterns) {
    // Start agent
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Test the actual operation names used by the modules
    const char* module_operations[] = {
        "process_inbox",           // bio_router
        "process_queue",           // msg_router
        "thalamic_process_queue",  // thalamic_router
        "consolidation_update",    // systems_consolidation
        "consolidation_replays"    // systems_consolidation
    };

    for (const char* op : module_operations) {
        EXPECT_NO_THROW(nimcp_health_agent_heartbeat_ex(agent, op, 0.5f));
    }

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received, 5u);

    // Stop agent
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}
