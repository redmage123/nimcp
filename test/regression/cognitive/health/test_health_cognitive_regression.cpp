/**
 * @file test_health_cognitive_regression.cpp
 * @brief Regression tests for Phase 8 cognitive health subsystems
 *
 * TEST COVERAGE:
 * - Null pointer handling edge cases
 * - Memory safety and cleanup
 * - State machine transitions
 * - Error recovery scenarios
 * - Resource limits and boundaries
 * - Previously identified bugs
 *
 * PHASE: 8 - Collective & Recursive Cognition Integration (Section 27)
 *
 * @author NIMCP Development Team
 * @date 2025-01-18
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>

// Phase 8 headers
#include "cognitive/health/nimcp_health_cognitive_bridge.h"
#include "cognitive/health/nimcp_collective_health.h"
#include "cognitive/health/nimcp_rcog_health.h"
#include "cognitive/health/nimcp_meta_health.h"
#include "utils/fault_tolerance/nimcp_health_agent.h"

/*=============================================================================
 * Test Fixture
 *===========================================================================*/

class HealthCognitiveRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        health_agent_config_t agent_config;
        nimcp_health_agent_default_config(&agent_config);
        agent_ = nimcp_health_agent_create(&agent_config);
    }

    void TearDown() override {
        if (bridge_) {
            health_cognitive_bridge_destroy(bridge_);
            bridge_ = nullptr;
        }
        if (collective_monitor_) {
            collective_health_monitor_destroy(collective_monitor_);
            collective_monitor_ = nullptr;
        }
        if (rcog_health_) {
            rcog_health_destroy(rcog_health_);
            rcog_health_ = nullptr;
        }
        if (meta_reflector_) {
            meta_health_destroy(meta_reflector_);
            meta_reflector_ = nullptr;
        }
        if (agent_) {
            nimcp_health_agent_destroy(agent_);
            agent_ = nullptr;
        }
    }

    nimcp_health_agent_t* agent_ = nullptr;
    health_cognitive_bridge_t* bridge_ = nullptr;
    collective_health_monitor_t* collective_monitor_ = nullptr;
    rcog_health_integration_t* rcog_health_ = nullptr;
    meta_health_reflector_t* meta_reflector_ = nullptr;
};

/*=============================================================================
 * 1. Null Pointer Regression Tests
 *===========================================================================*/

TEST_F(HealthCognitiveRegressionTest, BridgeNullHandlingRegression) {
    // All bridge functions should handle NULL gracefully
    EXPECT_EQ(health_cognitive_bridge_start(nullptr), -1);
    EXPECT_EQ(health_cognitive_bridge_stop(nullptr), -1);
    health_cognitive_bridge_destroy(nullptr);  // Should not crash

    intelligent_handling_result_t result;
    EXPECT_EQ(health_cognitive_intelligent_handle(nullptr,
        HEALTH_MSG_ANOMALY_DETECTED, HEALTH_SOURCE_MEMORY,
        HEALTH_SEVERITY_WARNING, &result), -1);

    bridge_ = health_cognitive_bridge_create(nullptr, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge_, nullptr);

    EXPECT_EQ(health_cognitive_intelligent_handle(bridge_,
        HEALTH_MSG_ANOMALY_DETECTED, HEALTH_SOURCE_MEMORY,
        HEALTH_SEVERITY_WARNING, nullptr), -1);
}

TEST_F(HealthCognitiveRegressionTest, CollectiveNullHandlingRegression) {
    // Collective health NULL handling
    EXPECT_EQ(collective_health_monitor_start(nullptr), -1);
    EXPECT_EQ(collective_health_monitor_stop(nullptr), -1);
    EXPECT_FALSE(collective_health_monitor_is_running(nullptr));
    collective_health_monitor_destroy(nullptr);  // Should not crash

    collective_health_summary_t summary;
    EXPECT_EQ(collective_health_get_summary(nullptr, &summary), -1);

    // Null agent should return NULL
    collective_health_monitor_t* m = collective_health_monitor_create(nullptr, nullptr, nullptr);
    EXPECT_EQ(m, nullptr);
}

TEST_F(HealthCognitiveRegressionTest, RcogNullHandlingRegression) {
    rcog_health_destroy(nullptr);  // Should not crash

    rcog_health_goal_t goal;
    rcog_health_init_goal(&goal);
    rcog_health_answer_t answer;
    EXPECT_EQ(rcog_health_submit_goal(nullptr, &goal, &answer), -1);

    rcog_health_ = rcog_health_create(nullptr, agent_, nullptr);
    ASSERT_NE(rcog_health_, nullptr);

    EXPECT_EQ(rcog_health_submit_goal(rcog_health_, nullptr, &answer), -1);
    EXPECT_EQ(rcog_health_submit_goal(rcog_health_, &goal, nullptr), -1);
}

TEST_F(HealthCognitiveRegressionTest, MetaNullHandlingRegression) {
    EXPECT_EQ(meta_health_start(nullptr), -1);
    EXPECT_EQ(meta_health_stop(nullptr), -1);
    meta_health_destroy(nullptr);  // Should not crash

    meta_health_decision_t decision;
    meta_health_init_decision(&decision);
    EXPECT_EQ(meta_health_record_decision(nullptr, &decision), -1);

    meta_reflector_ = meta_health_create(nullptr, nullptr, nullptr);
    ASSERT_NE(meta_reflector_, nullptr);

    EXPECT_EQ(meta_health_record_decision(meta_reflector_, nullptr), -1);
}

/*=============================================================================
 * 2. Double Operation Regression Tests
 *===========================================================================*/

TEST_F(HealthCognitiveRegressionTest, BridgeDoubleStartStopRegression) {
    bridge_ = health_cognitive_bridge_create(nullptr, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge_, nullptr);

    // Double start should be safe
    EXPECT_EQ(health_cognitive_bridge_start(bridge_), 0);
    EXPECT_EQ(health_cognitive_bridge_start(bridge_), 0);

    // Double stop should be safe
    EXPECT_EQ(health_cognitive_bridge_stop(bridge_), 0);
    EXPECT_EQ(health_cognitive_bridge_stop(bridge_), 0);
}

TEST_F(HealthCognitiveRegressionTest, CollectiveDoubleStartStopRegression) {
    collective_monitor_ = collective_health_monitor_create(agent_, nullptr, nullptr);
    ASSERT_NE(collective_monitor_, nullptr);

    EXPECT_EQ(collective_health_monitor_start(collective_monitor_), 0);
    EXPECT_EQ(collective_health_monitor_start(collective_monitor_), 0);

    EXPECT_EQ(collective_health_monitor_stop(collective_monitor_), 0);
    EXPECT_EQ(collective_health_monitor_stop(collective_monitor_), 0);
}

TEST_F(HealthCognitiveRegressionTest, MetaDoubleStartStopRegression) {
    meta_reflector_ = meta_health_create(nullptr, nullptr, nullptr);
    ASSERT_NE(meta_reflector_, nullptr);

    EXPECT_EQ(meta_health_start(meta_reflector_), 0);
    EXPECT_EQ(meta_health_start(meta_reflector_), 0);

    EXPECT_EQ(meta_health_stop(meta_reflector_), 0);
    EXPECT_EQ(meta_health_stop(meta_reflector_), 0);
}

/*=============================================================================
 * 3. Boundary Condition Regression Tests
 *===========================================================================*/

TEST_F(HealthCognitiveRegressionTest, MaxDecisionsRecordedRegression) {
    meta_reflector_ = meta_health_create(nullptr, nullptr, nullptr);
    ASSERT_NE(meta_reflector_, nullptr);

    EXPECT_EQ(meta_health_start(meta_reflector_), 0);

    // Record many decisions (up to internal limit)
    const int MAX_DECISIONS = 1000;
    for (int i = 0; i < MAX_DECISIONS; i++) {
        meta_health_decision_t decision;
        meta_health_init_decision(&decision);
        decision.anomaly_type = (health_agent_msg_type_t)(i % 5 + 1);
        decision.timestamp_us = i * 1000;

        int result = meta_health_record_decision(meta_reflector_, &decision);
        // Should either succeed or gracefully handle overflow
        EXPECT_GE(result, -1);
    }

    // Reflection should still work
    meta_health_reflection_result_t reflection;
    EXPECT_EQ(meta_health_reflect(meta_reflector_, &reflection), 0);

    EXPECT_EQ(meta_health_stop(meta_reflector_), 0);
}

TEST_F(HealthCognitiveRegressionTest, MaxGoalsSubmittedRegression) {
    rcog_health_ = rcog_health_create(nullptr, agent_, nullptr);
    ASSERT_NE(rcog_health_, nullptr);

    // Submit many goals
    const int MAX_GOALS = 100;
    for (int i = 0; i < MAX_GOALS; i++) {
        rcog_health_goal_t goal;
        rcog_health_init_goal(&goal);
        goal.health_type = (rcog_health_goal_type_t)(i % 3);
        goal.anomaly_type = (health_agent_msg_type_t)(i % 5 + 1);

        rcog_health_answer_t answer;
        int result = rcog_health_submit_goal(rcog_health_, &goal, &answer);
        // Should either succeed or gracefully handle overflow
        EXPECT_GE(result, -1);
    }
}

TEST_F(HealthCognitiveRegressionTest, ZeroTimestampRegression) {
    meta_reflector_ = meta_health_create(nullptr, nullptr, nullptr);
    ASSERT_NE(meta_reflector_, nullptr);

    // Zero timestamp should be handled
    meta_health_decision_t decision;
    meta_health_init_decision(&decision);
    decision.timestamp_us = 0;

    EXPECT_EQ(meta_health_record_decision(meta_reflector_, &decision), 0);
}

TEST_F(HealthCognitiveRegressionTest, MaxTimestampRegression) {
    meta_reflector_ = meta_health_create(nullptr, nullptr, nullptr);
    ASSERT_NE(meta_reflector_, nullptr);

    // Max timestamp should be handled
    meta_health_decision_t decision;
    meta_health_init_decision(&decision);
    decision.timestamp_us = UINT64_MAX;

    EXPECT_EQ(meta_health_record_decision(meta_reflector_, &decision), 0);
}

/*=============================================================================
 * 4. State Transition Regression Tests
 *===========================================================================*/

TEST_F(HealthCognitiveRegressionTest, OperationsWhileStoppedRegression) {
    bridge_ = health_cognitive_bridge_create(nullptr, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge_, nullptr);

    // Operations while stopped should be handled gracefully
    intelligent_handling_result_t result;
    health_cognitive_init_handling_result(&result);

    // This should still work (or return error gracefully)
    int ret = health_cognitive_intelligent_handle(bridge_,
        HEALTH_MSG_ANOMALY_DETECTED, HEALTH_SOURCE_MEMORY,
        HEALTH_SEVERITY_WARNING, &result);
    EXPECT_GE(ret, -1);
}

TEST_F(HealthCognitiveRegressionTest, CollectiveOperationsWhileStoppedRegression) {
    collective_monitor_ = collective_health_monitor_create(agent_, nullptr, nullptr);
    ASSERT_NE(collective_monitor_, nullptr);

    // Operations while stopped
    collective_health_summary_t summary;
    int ret = collective_health_get_summary(collective_monitor_, &summary);
    EXPECT_GE(ret, -1);

    collective_anomaly_proposal_t proposal;
    collective_health_init_proposal(&proposal);
    collective_anomaly_consensus_t consensus;
    ret = collective_health_propose_anomaly(collective_monitor_, &proposal, &consensus);
    EXPECT_GE(ret, -1);
}

/*=============================================================================
 * 5. Memory Safety Regression Tests
 *===========================================================================*/

TEST_F(HealthCognitiveRegressionTest, BridgeCreateDestroyLoopRegression) {
    // Create and destroy multiple times to check for memory leaks
    for (int i = 0; i < 10; i++) {
        health_cognitive_bridge_t* b = health_cognitive_bridge_create(
            nullptr, nullptr, nullptr, nullptr);
        ASSERT_NE(b, nullptr);

        EXPECT_EQ(health_cognitive_bridge_start(b), 0);
        EXPECT_EQ(health_cognitive_bridge_stop(b), 0);

        health_cognitive_bridge_destroy(b);
    }
}

TEST_F(HealthCognitiveRegressionTest, CollectiveCreateDestroyLoopRegression) {
    for (int i = 0; i < 10; i++) {
        collective_health_monitor_t* m = collective_health_monitor_create(
            agent_, nullptr, nullptr);
        ASSERT_NE(m, nullptr);

        EXPECT_EQ(collective_health_monitor_start(m), 0);
        EXPECT_EQ(collective_health_monitor_stop(m), 0);

        collective_health_monitor_destroy(m);
    }
}

TEST_F(HealthCognitiveRegressionTest, RcogCreateDestroyLoopRegression) {
    for (int i = 0; i < 10; i++) {
        rcog_health_integration_t* r = rcog_health_create(nullptr, agent_, nullptr);
        ASSERT_NE(r, nullptr);
        rcog_health_destroy(r);
    }
}

TEST_F(HealthCognitiveRegressionTest, MetaCreateDestroyLoopRegression) {
    for (int i = 0; i < 10; i++) {
        meta_health_reflector_t* m = meta_health_create(nullptr, nullptr, nullptr);
        ASSERT_NE(m, nullptr);

        EXPECT_EQ(meta_health_start(m), 0);
        EXPECT_EQ(meta_health_stop(m), 0);

        meta_health_destroy(m);
    }
}

/*=============================================================================
 * 6. Invalid Enum Value Regression Tests
 *===========================================================================*/

TEST_F(HealthCognitiveRegressionTest, InvalidMessageTypeRegression) {
    bridge_ = health_cognitive_bridge_create(nullptr, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge_, nullptr);

    EXPECT_EQ(health_cognitive_bridge_start(bridge_), 0);

    intelligent_handling_result_t result;
    health_cognitive_init_handling_result(&result);

    // Invalid message type should be handled
    int ret = health_cognitive_intelligent_handle(bridge_,
        (health_agent_msg_type_t)999,
        HEALTH_SOURCE_MEMORY,
        HEALTH_SEVERITY_WARNING, &result);
    // Should either work or return error, not crash
    EXPECT_GE(ret, -1);

    EXPECT_EQ(health_cognitive_bridge_stop(bridge_), 0);
}

TEST_F(HealthCognitiveRegressionTest, InvalidSeverityRegression) {
    bridge_ = health_cognitive_bridge_create(nullptr, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge_, nullptr);

    EXPECT_EQ(health_cognitive_bridge_start(bridge_), 0);

    intelligent_handling_result_t result;
    health_cognitive_init_handling_result(&result);

    // Invalid severity should be handled
    int ret = health_cognitive_intelligent_handle(bridge_,
        HEALTH_MSG_ANOMALY_DETECTED,
        HEALTH_SOURCE_MEMORY,
        (health_agent_severity_t)999, &result);
    EXPECT_GE(ret, -1);

    EXPECT_EQ(health_cognitive_bridge_stop(bridge_), 0);
}

TEST_F(HealthCognitiveRegressionTest, InvalidSourceRegression) {
    bridge_ = health_cognitive_bridge_create(nullptr, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge_, nullptr);

    EXPECT_EQ(health_cognitive_bridge_start(bridge_), 0);

    intelligent_handling_result_t result;
    health_cognitive_init_handling_result(&result);

    // Invalid source should be handled
    int ret = health_cognitive_intelligent_handle(bridge_,
        HEALTH_MSG_ANOMALY_DETECTED,
        (health_agent_source_t)999,
        HEALTH_SEVERITY_WARNING, &result);
    EXPECT_GE(ret, -1);

    EXPECT_EQ(health_cognitive_bridge_stop(bridge_), 0);
}

/*=============================================================================
 * 7. Statistics Reset Regression Tests
 *===========================================================================*/

TEST_F(HealthCognitiveRegressionTest, MetaStatsResetRegression) {
    meta_reflector_ = meta_health_create(nullptr, nullptr, nullptr);
    ASSERT_NE(meta_reflector_, nullptr);

    // Record some decisions
    meta_health_decision_t decision;
    meta_health_init_decision(&decision);
    meta_health_record_decision(meta_reflector_, &decision);

    // Reset stats
    meta_health_reset_stats(meta_reflector_);

    // Stats should be zeroed
    meta_health_stats_t stats;
    EXPECT_EQ(meta_health_get_stats(meta_reflector_, &stats), 0);
    EXPECT_EQ(stats.decisions_recorded, 0u);
}

TEST_F(HealthCognitiveRegressionTest, RcogStatsResetRegression) {
    rcog_health_ = rcog_health_create(nullptr, agent_, nullptr);
    ASSERT_NE(rcog_health_, nullptr);

    // Submit a goal
    rcog_health_goal_t goal;
    rcog_health_init_goal(&goal);
    rcog_health_answer_t answer;
    rcog_health_submit_goal(rcog_health_, &goal, &answer);

    // Reset stats
    rcog_health_reset_stats(rcog_health_);

    // Stats should be zeroed
    rcog_health_stats_t stats;
    EXPECT_EQ(rcog_health_get_stats(rcog_health_, &stats), 0);
    EXPECT_EQ(stats.goals_submitted, 0u);
}

/*=============================================================================
 * 8. Outcome Recording Regression Tests
 *===========================================================================*/

TEST_F(HealthCognitiveRegressionTest, OutcomeWithoutDecisionRegression) {
    meta_reflector_ = meta_health_create(nullptr, nullptr, nullptr);
    ASSERT_NE(meta_reflector_, nullptr);

    // Recording outcome without matching decision should be handled
    int ret = meta_health_record_outcome(meta_reflector_, 12345,
        META_HEALTH_OUTCOME_SUCCESS, true, 100, 0.9f);
    // Should either succeed (orphan outcome) or return error, not crash
    EXPECT_GE(ret, -1);
}

TEST_F(HealthCognitiveRegressionTest, DuplicateOutcomeRegression) {
    meta_reflector_ = meta_health_create(nullptr, nullptr, nullptr);
    ASSERT_NE(meta_reflector_, nullptr);

    // Record decision
    meta_health_decision_t decision;
    meta_health_init_decision(&decision);
    decision.timestamp_us = 1000;
    EXPECT_EQ(meta_health_record_decision(meta_reflector_, &decision), 0);

    // Record outcome twice
    EXPECT_EQ(meta_health_record_outcome(meta_reflector_, 1000,
        META_HEALTH_OUTCOME_SUCCESS, true, 100, 0.9f), 0);

    int ret = meta_health_record_outcome(meta_reflector_, 1000,
        META_HEALTH_OUTCOME_FAILURE, false, 200, 0.5f);
    // Second outcome might overwrite or be ignored, not crash
    EXPECT_GE(ret, -1);
}

/*=============================================================================
 * 9. Async Operations Regression Tests
 *===========================================================================*/

TEST_F(HealthCognitiveRegressionTest, AsyncReflectionRegression) {
    meta_reflector_ = meta_health_create(nullptr, nullptr, nullptr);
    ASSERT_NE(meta_reflector_, nullptr);

    EXPECT_EQ(meta_health_start(meta_reflector_), 0);

    // Start async reflection
    uint64_t request_id = 0;
    EXPECT_EQ(meta_health_reflect_async(meta_reflector_, &request_id), 0);
    EXPECT_NE(request_id, 0u);

    // Check result (may be pending or complete)
    meta_health_reflection_result_t result;
    int status = meta_health_get_reflection_result(meta_reflector_, request_id, &result);
    EXPECT_GE(status, 0);  // 0 = pending, 1 = complete

    EXPECT_EQ(meta_health_stop(meta_reflector_), 0);
}

TEST_F(HealthCognitiveRegressionTest, InvalidRequestIdRegression) {
    meta_reflector_ = meta_health_create(nullptr, nullptr, nullptr);
    ASSERT_NE(meta_reflector_, nullptr);

    // Query invalid request ID
    meta_health_reflection_result_t result;
    int status = meta_health_get_reflection_result(meta_reflector_, 99999, &result);
    // Should return error or "not found"
    EXPECT_LE(status, 0);
}
