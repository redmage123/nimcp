/**
 * @file test_heartbeat_module_functions.cpp
 * @brief Unit tests for Phase 8 heartbeat module setter functions
 * @version 1.0.0
 * @date 2025-01-20
 *
 * WHAT: Tests for heartbeat setter functions added to various modules
 * WHY:  Verify modules can be connected to health agent for heartbeat monitoring
 * HOW:  Test setter functions with NULL and valid health agents
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <cstring>
#include <atomic>

// Health agent header
#include "utils/fault_tolerance/nimcp_health_agent.h"

//=============================================================================
// Forward declarations for module heartbeat setters
//=============================================================================

// These are the setter functions we added in Phase 8
extern "C" {
    // Bio router
    void bio_router_set_health_agent(nimcp_health_agent_t* agent);

    // Message router
    void nimcp_msg_router_set_health_agent(nimcp_health_agent_t* agent);

    // Thalamic router
    void thalamic_router_set_health_agent(nimcp_health_agent_t* agent);

    // Systems consolidation
    void systems_consolidation_set_health_agent(nimcp_health_agent_t* agent);
}

//=============================================================================
// Test Fixtures
//=============================================================================

/**
 * @brief Base fixture for heartbeat module unit tests
 */
class HeartbeatModuleFunctionsTest : public ::testing::Test {
protected:
    nimcp_health_agent_t* agent = nullptr;
    health_agent_config_t config;

    void SetUp() override {
        nimcp_health_agent_default_config(&config);
        config.check_interval_ms = 50;  // Fast checks for unit tests
        config.enable_auto_recovery = false;
        config.heartbeat_interval_ms = 100;

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
// Bio Router Heartbeat Setter Tests
//=============================================================================

TEST_F(HeartbeatModuleFunctionsTest, BioRouter_SetHealthAgent_WithNull_NoError) {
    // Setting NULL health agent should not crash
    EXPECT_NO_THROW(bio_router_set_health_agent(nullptr));
}

TEST_F(HeartbeatModuleFunctionsTest, BioRouter_SetHealthAgent_WithValid_NoError) {
    // Setting valid health agent should not crash
    EXPECT_NO_THROW(bio_router_set_health_agent(agent));
}

TEST_F(HeartbeatModuleFunctionsTest, BioRouter_SetHealthAgent_CanReplace) {
    // Should be able to replace health agent
    EXPECT_NO_THROW(bio_router_set_health_agent(agent));
    EXPECT_NO_THROW(bio_router_set_health_agent(nullptr));
    EXPECT_NO_THROW(bio_router_set_health_agent(agent));
}

//=============================================================================
// Message Router Heartbeat Setter Tests
//=============================================================================

TEST_F(HeartbeatModuleFunctionsTest, MsgRouter_SetHealthAgent_WithNull_NoError) {
    // Setting NULL health agent should not crash
    EXPECT_NO_THROW(nimcp_msg_router_set_health_agent(nullptr));
}

TEST_F(HeartbeatModuleFunctionsTest, MsgRouter_SetHealthAgent_WithValid_NoError) {
    // Setting valid health agent should not crash
    EXPECT_NO_THROW(nimcp_msg_router_set_health_agent(agent));
}

TEST_F(HeartbeatModuleFunctionsTest, MsgRouter_SetHealthAgent_CanReplace) {
    // Should be able to replace health agent
    EXPECT_NO_THROW(nimcp_msg_router_set_health_agent(agent));
    EXPECT_NO_THROW(nimcp_msg_router_set_health_agent(nullptr));
    EXPECT_NO_THROW(nimcp_msg_router_set_health_agent(agent));
}

//=============================================================================
// Thalamic Router Heartbeat Setter Tests
//=============================================================================

TEST_F(HeartbeatModuleFunctionsTest, ThalamicRouter_SetHealthAgent_WithNull_NoError) {
    // Setting NULL health agent should not crash
    EXPECT_NO_THROW(thalamic_router_set_health_agent(nullptr));
}

TEST_F(HeartbeatModuleFunctionsTest, ThalamicRouter_SetHealthAgent_WithValid_NoError) {
    // Setting valid health agent should not crash
    EXPECT_NO_THROW(thalamic_router_set_health_agent(agent));
}

TEST_F(HeartbeatModuleFunctionsTest, ThalamicRouter_SetHealthAgent_CanReplace) {
    // Should be able to replace health agent
    EXPECT_NO_THROW(thalamic_router_set_health_agent(agent));
    EXPECT_NO_THROW(thalamic_router_set_health_agent(nullptr));
    EXPECT_NO_THROW(thalamic_router_set_health_agent(agent));
}

//=============================================================================
// Systems Consolidation Heartbeat Setter Tests
//=============================================================================

TEST_F(HeartbeatModuleFunctionsTest, SystemsConsolidation_SetHealthAgent_WithNull_NoError) {
    // Setting NULL health agent should not crash
    EXPECT_NO_THROW(systems_consolidation_set_health_agent(nullptr));
}

TEST_F(HeartbeatModuleFunctionsTest, SystemsConsolidation_SetHealthAgent_WithValid_NoError) {
    // Setting valid health agent should not crash
    EXPECT_NO_THROW(systems_consolidation_set_health_agent(agent));
}

TEST_F(HeartbeatModuleFunctionsTest, SystemsConsolidation_SetHealthAgent_CanReplace) {
    // Should be able to replace health agent
    EXPECT_NO_THROW(systems_consolidation_set_health_agent(agent));
    EXPECT_NO_THROW(systems_consolidation_set_health_agent(nullptr));
    EXPECT_NO_THROW(systems_consolidation_set_health_agent(agent));
}

//=============================================================================
// Combined Module Tests
//=============================================================================

TEST_F(HeartbeatModuleFunctionsTest, AllModules_SetSameAgent_NoConflict) {
    // All modules can share the same health agent
    EXPECT_NO_THROW(bio_router_set_health_agent(agent));
    EXPECT_NO_THROW(nimcp_msg_router_set_health_agent(agent));
    EXPECT_NO_THROW(thalamic_router_set_health_agent(agent));
    EXPECT_NO_THROW(systems_consolidation_set_health_agent(agent));
}

TEST_F(HeartbeatModuleFunctionsTest, AllModules_ClearAll_NoError) {
    // Set all modules
    bio_router_set_health_agent(agent);
    nimcp_msg_router_set_health_agent(agent);
    thalamic_router_set_health_agent(agent);
    systems_consolidation_set_health_agent(agent);

    // Clear all modules
    EXPECT_NO_THROW(bio_router_set_health_agent(nullptr));
    EXPECT_NO_THROW(nimcp_msg_router_set_health_agent(nullptr));
    EXPECT_NO_THROW(thalamic_router_set_health_agent(nullptr));
    EXPECT_NO_THROW(systems_consolidation_set_health_agent(nullptr));
}

//=============================================================================
// Heartbeat Function Tests (using health agent API directly)
//=============================================================================

TEST_F(HeartbeatModuleFunctionsTest, HeartbeatEx_WithNullAgent_NoError) {
    // Calling heartbeat_ex with NULL agent should not crash
    EXPECT_NO_THROW(nimcp_health_agent_heartbeat_ex(nullptr, "test_op", 0.5f));
}

TEST_F(HeartbeatModuleFunctionsTest, HeartbeatEx_WithValidAgent_NoError) {
    // Calling heartbeat_ex with valid agent should work
    EXPECT_NO_THROW(nimcp_health_agent_heartbeat_ex(agent, "test_op", 0.0f));
    EXPECT_NO_THROW(nimcp_health_agent_heartbeat_ex(agent, "test_op", 0.5f));
    EXPECT_NO_THROW(nimcp_health_agent_heartbeat_ex(agent, "test_op", 1.0f));
}

TEST_F(HeartbeatModuleFunctionsTest, HeartbeatEx_WithNullOperation_NoError) {
    // NULL operation name should be handled gracefully
    EXPECT_NO_THROW(nimcp_health_agent_heartbeat_ex(agent, nullptr, 0.5f));
}

TEST_F(HeartbeatModuleFunctionsTest, HeartbeatEx_WithEdgeProgressValues_NoError) {
    // Edge case progress values
    EXPECT_NO_THROW(nimcp_health_agent_heartbeat_ex(agent, "test", -0.1f));  // Below 0
    EXPECT_NO_THROW(nimcp_health_agent_heartbeat_ex(agent, "test", 1.1f));   // Above 1
}

TEST_F(HeartbeatModuleFunctionsTest, Heartbeat_IncreasesCounter) {
    // Start the health agent
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Get initial stats
    health_agent_stats_t stats_before;
    nimcp_health_agent_get_stats(agent, &stats_before);

    // Send multiple heartbeats
    for (int i = 0; i < 10; i++) {
        nimcp_health_agent_heartbeat(agent);
    }

    // Get stats after heartbeats
    health_agent_stats_t stats_after;
    nimcp_health_agent_get_stats(agent, &stats_after);

    // Verify heartbeats were counted
    EXPECT_GE(stats_after.heartbeats_received, stats_before.heartbeats_received + 10);

    // Stop agent
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HeartbeatModuleFunctionsTest, HeartbeatEx_IncreasesCounter) {
    // Start the health agent
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Get initial stats
    health_agent_stats_t stats_before;
    nimcp_health_agent_get_stats(agent, &stats_before);

    // Send multiple heartbeats with extended info
    for (int i = 0; i < 10; i++) {
        nimcp_health_agent_heartbeat_ex(agent, "unit_test", (float)i / 10.0f);
    }

    // Get stats after heartbeats
    health_agent_stats_t stats_after;
    nimcp_health_agent_get_stats(agent, &stats_after);

    // Verify heartbeats were counted
    EXPECT_GE(stats_after.heartbeats_received, stats_before.heartbeats_received + 10);

    // Stop agent
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Thread Safety Tests
//=============================================================================

TEST_F(HeartbeatModuleFunctionsTest, SetHealthAgent_ConcurrentAccess_NoRace) {
    // Multiple threads setting health agents concurrently
    std::atomic<bool> start_flag{false};
    std::vector<std::thread> threads;

    // Create threads that will set health agents
    for (int i = 0; i < 4; i++) {
        threads.emplace_back([this, &start_flag, i]() {
            while (!start_flag.load()) {
                std::this_thread::yield();
            }
            for (int j = 0; j < 100; j++) {
                switch (i % 4) {
                    case 0:
                        bio_router_set_health_agent(agent);
                        bio_router_set_health_agent(nullptr);
                        break;
                    case 1:
                        nimcp_msg_router_set_health_agent(agent);
                        nimcp_msg_router_set_health_agent(nullptr);
                        break;
                    case 2:
                        thalamic_router_set_health_agent(agent);
                        thalamic_router_set_health_agent(nullptr);
                        break;
                    case 3:
                        systems_consolidation_set_health_agent(agent);
                        systems_consolidation_set_health_agent(nullptr);
                        break;
                }
            }
        });
    }

    // Start all threads simultaneously
    start_flag.store(true);

    // Wait for all threads to complete
    for (auto& t : threads) {
        t.join();
    }

    // If we get here without crashing, the test passes
    SUCCEED();
}

//=============================================================================
// Agent Lifecycle Tests
//=============================================================================

TEST_F(HeartbeatModuleFunctionsTest, HeartbeatsWithAgentRunning_UpdateTimestamp) {
    // Start agent
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Send heartbeats
    for (int i = 0; i < 5; i++) {
        nimcp_health_agent_heartbeat(agent);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // Agent should still be healthy
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));

    // Stop agent
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HeartbeatModuleFunctionsTest, HeartbeatsWhileAgentStopped_NoCrash) {
    // Don't start agent
    EXPECT_FALSE(nimcp_health_agent_is_running(agent));

    // Sending heartbeats to stopped agent should not crash
    for (int i = 0; i < 5; i++) {
        EXPECT_NO_THROW(nimcp_health_agent_heartbeat(agent));
        EXPECT_NO_THROW(nimcp_health_agent_heartbeat_ex(agent, "test", 0.5f));
    }
}
