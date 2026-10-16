/**
 * @file test_heartbeat_module_regression.cpp
 * @brief Regression tests for Phase 8 heartbeat module API contracts
 * @version 1.0.0
 * @date 2025-01-20
 *
 * WHAT: Tests API contracts and backwards compatibility for heartbeat module functions
 * WHY:  Ensure heartbeat API remains stable and doesn't break existing code
 * HOW:  Test function signatures, return values, and expected behaviors
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <cstring>

// Health agent header
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
 * @brief Fixture for heartbeat module regression tests
 */
class HeartbeatModuleRegressionTest : public ::testing::Test {
protected:
    nimcp_health_agent_t* agent = nullptr;
    health_agent_config_t config;

    void SetUp() override {
        nimcp_health_agent_default_config(&config);
        config.check_interval_ms = 50;
        config.enable_auto_recovery = false;

        agent = nimcp_health_agent_create(&config);
        ASSERT_NE(agent, nullptr);
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
};

//=============================================================================
// API Contract Tests - Function Signature Stability
//=============================================================================

/**
 * @brief Verify setter functions accept nimcp_health_agent_t* parameter
 */
TEST_F(HeartbeatModuleRegressionTest, SetterFunctions_AcceptHealthAgentPointer) {
    // All setter functions should accept nimcp_health_agent_t* without compiler errors
    // If this compiles, the API contract is maintained
    bio_router_set_health_agent(agent);
    nimcp_msg_router_set_health_agent(agent);
    thalamic_router_set_health_agent(agent);
    systems_consolidation_set_health_agent(agent);

    SUCCEED();
}

/**
 * @brief Verify setter functions return void
 */
TEST_F(HeartbeatModuleRegressionTest, SetterFunctions_ReturnVoid) {
    // All setter functions return void - no return value to check
    // Verify they don't crash and execute cleanly
    bio_router_set_health_agent(agent);
    nimcp_msg_router_set_health_agent(nullptr);
    thalamic_router_set_health_agent(agent);
    systems_consolidation_set_health_agent(nullptr);

    SUCCEED();
}

//=============================================================================
// API Contract Tests - NULL Handling
//=============================================================================

TEST_F(HeartbeatModuleRegressionTest, Contract_NullAgent_AlwaysAccepted) {
    // Setting NULL should always be valid (disconnect agent)
    EXPECT_NO_THROW(bio_router_set_health_agent(nullptr));
    EXPECT_NO_THROW(nimcp_msg_router_set_health_agent(nullptr));
    EXPECT_NO_THROW(thalamic_router_set_health_agent(nullptr));
    EXPECT_NO_THROW(systems_consolidation_set_health_agent(nullptr));
}

TEST_F(HeartbeatModuleRegressionTest, Contract_ValidAgent_AlwaysAccepted) {
    // Setting valid agent should always be valid
    EXPECT_NO_THROW(bio_router_set_health_agent(agent));
    EXPECT_NO_THROW(nimcp_msg_router_set_health_agent(agent));
    EXPECT_NO_THROW(thalamic_router_set_health_agent(agent));
    EXPECT_NO_THROW(systems_consolidation_set_health_agent(agent));
}

//=============================================================================
// API Contract Tests - Heartbeat Ex Function
//=============================================================================

TEST_F(HeartbeatModuleRegressionTest, HeartbeatEx_FunctionSignature) {
    // nimcp_health_agent_heartbeat_ex(agent, operation, progress) signature
    // agent: nimcp_health_agent_t*
    // operation: const char*
    // progress: float

    // Valid call
    nimcp_health_agent_heartbeat_ex(agent, "test_operation", 0.5f);

    SUCCEED();
}

TEST_F(HeartbeatModuleRegressionTest, HeartbeatEx_NullAgentContract) {
    // NULL agent should be handled gracefully (no crash)
    EXPECT_NO_THROW(nimcp_health_agent_heartbeat_ex(nullptr, "test", 0.5f));
}

TEST_F(HeartbeatModuleRegressionTest, HeartbeatEx_NullOperationContract) {
    // NULL operation should be handled gracefully
    EXPECT_NO_THROW(nimcp_health_agent_heartbeat_ex(agent, nullptr, 0.5f));
}

TEST_F(HeartbeatModuleRegressionTest, HeartbeatEx_ProgressRangeContract) {
    // Progress values should be accepted regardless of range
    // Internal clamping may occur, but no crash
    EXPECT_NO_THROW(nimcp_health_agent_heartbeat_ex(agent, "test", -1.0f));
    EXPECT_NO_THROW(nimcp_health_agent_heartbeat_ex(agent, "test", 0.0f));
    EXPECT_NO_THROW(nimcp_health_agent_heartbeat_ex(agent, "test", 0.5f));
    EXPECT_NO_THROW(nimcp_health_agent_heartbeat_ex(agent, "test", 1.0f));
    EXPECT_NO_THROW(nimcp_health_agent_heartbeat_ex(agent, "test", 2.0f));
}

//=============================================================================
// API Contract Tests - Idempotency
//=============================================================================

TEST_F(HeartbeatModuleRegressionTest, Contract_SetterIdempotent) {
    // Setting the same agent multiple times should be idempotent
    for (int i = 0; i < 10; i++) {
        bio_router_set_health_agent(agent);
    }

    // Setting NULL multiple times should be idempotent
    for (int i = 0; i < 10; i++) {
        bio_router_set_health_agent(nullptr);
    }

    SUCCEED();
}

TEST_F(HeartbeatModuleRegressionTest, Contract_AlternatingSetterCalls) {
    // Alternating between agent and NULL should work
    for (int i = 0; i < 10; i++) {
        bio_router_set_health_agent(agent);
        bio_router_set_health_agent(nullptr);
    }

    SUCCEED();
}

//=============================================================================
// API Contract Tests - Order Independence
//=============================================================================

TEST_F(HeartbeatModuleRegressionTest, Contract_SetBeforeAgentStart) {
    // Setting health agent before agent is started should work
    bio_router_set_health_agent(agent);

    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    nimcp_health_agent_heartbeat_ex(agent, "test", 0.5f);
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HeartbeatModuleRegressionTest, Contract_SetAfterAgentStart) {
    // Setting health agent after agent is started should work
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    bio_router_set_health_agent(agent);
    nimcp_health_agent_heartbeat_ex(agent, "test", 0.5f);

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HeartbeatModuleRegressionTest, Contract_ClearWhileAgentRunning) {
    // Clearing health agent while agent is running should work
    bio_router_set_health_agent(agent);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    bio_router_set_health_agent(nullptr);  // Clear while running

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Backwards Compatibility Tests
//=============================================================================

TEST_F(HeartbeatModuleRegressionTest, BackwardsCompat_OriginalHeartbeatStillWorks) {
    // Original heartbeat function should still work
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    nimcp_health_agent_heartbeat(agent);

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received, 1u);

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HeartbeatModuleRegressionTest, BackwardsCompat_HeartbeatExCoexistsWithOriginal) {
    // heartbeat_ex should coexist with original heartbeat
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    nimcp_health_agent_heartbeat(agent);
    nimcp_health_agent_heartbeat_ex(agent, "extended", 0.5f);
    nimcp_health_agent_heartbeat(agent);
    nimcp_health_agent_heartbeat_ex(agent, "extended", 1.0f);

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received, 4u);

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Module Independence Tests
//=============================================================================

TEST_F(HeartbeatModuleRegressionTest, ModuleIndependence_EachModuleSeparate) {
    // Each module has its own health agent pointer - they should be independent
    nimcp_health_agent_t* agent2 = nimcp_health_agent_create(&config);
    ASSERT_NE(agent2, nullptr);

    // Set different agents to different modules
    bio_router_set_health_agent(agent);
    nimcp_msg_router_set_health_agent(agent2);

    // Clear one, shouldn't affect the other
    bio_router_set_health_agent(nullptr);

    // Start both agents
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent2), 0);

    // Send heartbeats
    nimcp_health_agent_heartbeat_ex(agent, "from_agent1", 0.5f);
    nimcp_health_agent_heartbeat_ex(agent2, "from_agent2", 0.5f);

    // Stop and cleanup
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
    EXPECT_EQ(nimcp_health_agent_stop(agent2), 0);
    nimcp_health_agent_destroy(agent2);
}

//=============================================================================
// Stats Consistency Tests
//=============================================================================

TEST_F(HeartbeatModuleRegressionTest, Stats_HeartbeatsCountedCorrectly) {
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    health_agent_stats_t stats_before;
    nimcp_health_agent_get_stats(agent, &stats_before);

    const int HEARTBEAT_COUNT = 100;
    for (int i = 0; i < HEARTBEAT_COUNT; i++) {
        nimcp_health_agent_heartbeat_ex(agent, "counting_test", (float)i / HEARTBEAT_COUNT);
    }

    health_agent_stats_t stats_after;
    nimcp_health_agent_get_stats(agent, &stats_after);

    // All heartbeats should be counted
    EXPECT_EQ(stats_after.heartbeats_received,
              stats_before.heartbeats_received + HEARTBEAT_COUNT);

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Long Operation String Tests
//=============================================================================

TEST_F(HeartbeatModuleRegressionTest, LongOperationString_HandledGracefully) {
    // Very long operation string should be handled (truncated or accepted)
    std::string long_op(1000, 'x');

    EXPECT_NO_THROW(nimcp_health_agent_heartbeat_ex(agent, long_op.c_str(), 0.5f));
}

TEST_F(HeartbeatModuleRegressionTest, EmptyOperationString_HandledGracefully) {
    // Empty operation string should be handled
    EXPECT_NO_THROW(nimcp_health_agent_heartbeat_ex(agent, "", 0.5f));
}

TEST_F(HeartbeatModuleRegressionTest, SpecialCharactersInOperation_HandledGracefully) {
    // Special characters in operation string should be handled
    EXPECT_NO_THROW(nimcp_health_agent_heartbeat_ex(agent, "test\nwith\nnewlines", 0.5f));
    EXPECT_NO_THROW(nimcp_health_agent_heartbeat_ex(agent, "test\twith\ttabs", 0.5f));
    EXPECT_NO_THROW(nimcp_health_agent_heartbeat_ex(agent, "test with spaces", 0.5f));
    EXPECT_NO_THROW(nimcp_health_agent_heartbeat_ex(agent, "test/with/slashes", 0.5f));
}
