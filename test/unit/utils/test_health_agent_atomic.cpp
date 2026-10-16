/**
 * @file test_health_agent_atomic.cpp
 * @brief Tests for atomic health agents (P3-3)
 *
 * WHAT: Verify health agent creation, heartbeat, and stats API
 * WHY:  P3-3 added atomic health agent global pointers and macros
 * HOW:  Test agent lifecycle, heartbeat reporting, statistics retrieval
 *
 * Function signatures tested (from include/utils/fault_tolerance/nimcp_health_agent.h):
 *   nimcp_health_agent_t* nimcp_health_agent_create(const health_agent_config_t* config);
 *   void nimcp_health_agent_default_config(health_agent_config_t* config);
 *   void nimcp_health_agent_destroy(nimcp_health_agent_t* agent);
 *   void nimcp_health_agent_heartbeat(nimcp_health_agent_t* agent);
 *   void nimcp_health_agent_heartbeat_ex(nimcp_health_agent_t* agent,
 *                                         const char* operation, float progress);
 *   bool nimcp_health_agent_is_running(const nimcp_health_agent_t* agent);
 *   void nimcp_health_agent_get_stats(const nimcp_health_agent_t* agent,
 *                                      health_agent_stats_t* stats);
 *
 * Macro tested (from include/utils/fault_tolerance/nimcp_health_agent_macros.h):
 *   NIMCP_DECLARE_HEALTH_AGENT(module)
 *     - generates: g_module_health_agent, module_set_health_agent(), module_heartbeat()
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "utils/fault_tolerance/nimcp_health_agent.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class HealthAgentTest : public ::testing::Test {
protected:
    nimcp_health_agent_t* agent = nullptr;

    void SetUp() override {
        health_agent_config_t config;
        nimcp_health_agent_default_config(&config);
        config.heartbeat_interval_ms = 200;
        config.watchdog_timeout_ms = 1000;
        config.check_interval_ms = 100;
        agent = nimcp_health_agent_create(&config);
    }

    void TearDown() override {
        if (agent) {
            nimcp_health_agent_destroy(agent);
            agent = nullptr;
        }
    }
};

/* ============================================================================
 * Creation / Destruction Tests
 * ============================================================================ */

TEST_F(HealthAgentTest, CreateWithConfig) {
    ASSERT_NE(agent, nullptr);
}

TEST(HealthAgentBasicTest, CreateWithDefaults) {
    nimcp_health_agent_t* a = nimcp_health_agent_create(nullptr);
    ASSERT_NE(a, nullptr);
    nimcp_health_agent_destroy(a);
}

TEST(HealthAgentBasicTest, DestroyNull) {
    // nimcp_health_agent_destroy(NULL) should be safe
    nimcp_health_agent_destroy(nullptr);
    SUCCEED() << "Destroying NULL agent did not crash";
}

TEST(HealthAgentBasicTest, DefaultConfig) {
    health_agent_config_t config;
    nimcp_health_agent_default_config(&config);

    EXPECT_GT(config.heartbeat_interval_ms, 0u);
    EXPECT_GT(config.watchdog_timeout_ms, 0u);
    EXPECT_GT(config.check_interval_ms, 0u);
}

/* ============================================================================
 * Heartbeat Tests
 * ============================================================================ */

TEST_F(HealthAgentTest, HeartbeatDoesNotCrash) {
    nimcp_health_agent_heartbeat(agent);
    SUCCEED() << "Heartbeat call completed without crash";
}

TEST_F(HealthAgentTest, HeartbeatExDoesNotCrash) {
    nimcp_health_agent_heartbeat_ex(agent, "test_operation", 0.5f);
    SUCCEED() << "Extended heartbeat completed without crash";
}

TEST_F(HealthAgentTest, HeartbeatNullAgent) {
    // Heartbeat with NULL agent should be safe (no-op)
    nimcp_health_agent_heartbeat(nullptr);
    SUCCEED() << "Heartbeat with NULL agent did not crash";
}

TEST_F(HealthAgentTest, HeartbeatExNullAgent) {
    nimcp_health_agent_heartbeat_ex(nullptr, "test", 0.0f);
    SUCCEED() << "Extended heartbeat with NULL agent did not crash";
}

TEST_F(HealthAgentTest, MultipleHeartbeats) {
    for (int i = 0; i < 10; i++) {
        nimcp_health_agent_heartbeat_ex(
            agent,
            "batch_processing",
            static_cast<float>(i) / 10.0f
        );
    }
    SUCCEED() << "Multiple heartbeats completed";
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(HealthAgentTest, GetStatsDoesNotCrash) {
    health_agent_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    nimcp_health_agent_get_stats(agent, &stats);
    SUCCEED() << "Get stats completed without crash";
}

TEST_F(HealthAgentTest, StatsAfterHeartbeats) {
    // Send some heartbeats
    for (int i = 0; i < 5; i++) {
        nimcp_health_agent_heartbeat(agent);
    }

    health_agent_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    nimcp_health_agent_get_stats(agent, &stats);

    // heartbeats_received should be at least the ones we sent
    EXPECT_GE(stats.heartbeats_received, 5u);
}

/* ============================================================================
 * Running State Tests
 * ============================================================================ */

TEST_F(HealthAgentTest, NotRunningBeforeStart) {
    // Agent is created but not started
    EXPECT_FALSE(nimcp_health_agent_is_running(agent));
}

/* ============================================================================
 * NIMCP_DECLARE_HEALTH_AGENT Macro Tests
 * ============================================================================ */

// Declare a test module health agent using the macro
NIMCP_DECLARE_HEALTH_AGENT(test_module)

TEST_F(HealthAgentTest, SetHealthAgentMacro) {
    // Set the health agent via macro-generated function
    test_module_set_health_agent(agent);

    // The heartbeat should work through the macro-generated function
    test_module_heartbeat("macro_test", 0.75f);
    SUCCEED() << "Macro-generated set and heartbeat functions work";
}

TEST(HealthAgentMacroTest, SetNullAgent) {
    // Set NULL should be safe
    test_module_set_health_agent(nullptr);

    // Heartbeat with NULL agent should be no-op
    test_module_heartbeat("null_test", 0.0f);
    SUCCEED() << "Macro functions handle NULL agent safely";
}
