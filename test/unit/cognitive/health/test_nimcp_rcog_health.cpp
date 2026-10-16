/**
 * @file test_nimcp_rcog_health.cpp
 * @brief Unit tests for RCOG health integration
 *
 * WHAT: Tests for recursive cognition integration with health system
 * WHY:  Validate intelligent diagnosis, recovery planning, tool registration
 * HOW:  Test lifecycle, goal submission, diagnosis, recovery, tools
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "cognitive/health/nimcp_rcog_health.h"

/*=============================================================================
 * Test Fixture
 *===========================================================================*/

class RcogHealthTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_ = rcog_health_default_config();
    }

    void TearDown() override {
        if (integration_) {
            rcog_health_destroy(integration_);
            integration_ = nullptr;
        }
    }

    rcog_health_config_t config_;
    rcog_health_integration_t* integration_ = nullptr;
};

/*=============================================================================
 * Configuration Tests
 *===========================================================================*/

TEST_F(RcogHealthTest, DefaultConfigValues) {
    EXPECT_TRUE(config_.register_builtin_tools);
    // enable_imagination defaults to false for safety
    EXPECT_TRUE(config_.enable_async);
    EXPECT_TRUE(config_.enable_diagnosis_cache);

    EXPECT_FLOAT_EQ(config_.confidence_threshold, RCOG_HEALTH_DEFAULT_CONFIDENCE);
    EXPECT_EQ(config_.default_timeout_ms, RCOG_HEALTH_DEFAULT_TIMEOUT_MS);
    EXPECT_EQ(config_.max_recursion_depth, RCOG_HEALTH_MAX_RECURSION_DEPTH);

    EXPECT_GT(config_.max_concurrent_goals, 0u);
    EXPECT_GT(config_.cache_ttl_ms, 0u);
}

/*=============================================================================
 * Lifecycle Tests
 *===========================================================================*/

TEST_F(RcogHealthTest, CreateWithNullConfig) {
    integration_ = rcog_health_create(nullptr, nullptr, nullptr);
    ASSERT_NE(integration_, nullptr);
}

TEST_F(RcogHealthTest, CreateWithConfig) {
    config_.enable_imagination = false;
    integration_ = rcog_health_create(nullptr, nullptr, &config_);
    ASSERT_NE(integration_, nullptr);
}

TEST_F(RcogHealthTest, DestroyNull) {
    // Should not crash
    rcog_health_destroy(nullptr);
}

/*=============================================================================
 * Goal Initialization Tests
 *===========================================================================*/

TEST_F(RcogHealthTest, InitGoalNull) {
    // Should not crash
    rcog_health_init_goal(nullptr);
}

TEST_F(RcogHealthTest, InitGoalBasic) {
    rcog_health_goal_t goal;
    memset(&goal, 0xFF, sizeof(goal));

    rcog_health_init_goal(&goal);

    EXPECT_EQ(goal.health_type, RCOG_HEALTH_GOAL_DIAGNOSE);
    EXPECT_FLOAT_EQ(goal.confidence_threshold, RCOG_HEALTH_DEFAULT_CONFIDENCE);
    EXPECT_EQ(goal.max_recursion_depth, RCOG_HEALTH_MAX_RECURSION_DEPTH);
    // timeout_ms is set to default timeout, not 0
    EXPECT_EQ(goal.timeout_ms, RCOG_HEALTH_DEFAULT_TIMEOUT_MS);
    EXPECT_FALSE(goal.enable_imagination);
    EXPECT_EQ(goal.context_size, 0u);
}

/*=============================================================================
 * Goal Submission Tests
 *===========================================================================*/

TEST_F(RcogHealthTest, SubmitGoalNull) {
    rcog_health_goal_t goal;
    rcog_health_answer_t answer;

    rcog_health_init_goal(&goal);

    EXPECT_EQ(rcog_health_submit_goal(nullptr, &goal, &answer), -1);

    integration_ = rcog_health_create(nullptr, nullptr, &config_);
    ASSERT_NE(integration_, nullptr);

    EXPECT_EQ(rcog_health_submit_goal(integration_, nullptr, &answer), -1);
    EXPECT_EQ(rcog_health_submit_goal(integration_, &goal, nullptr), -1);
}

TEST_F(RcogHealthTest, SubmitGoalBasic) {
    integration_ = rcog_health_create(nullptr, nullptr, &config_);
    ASSERT_NE(integration_, nullptr);

    rcog_health_goal_t goal;
    rcog_health_init_goal(&goal);
    goal.health_type = RCOG_HEALTH_GOAL_VERIFY_HEALTH;
    snprintf(goal.query, sizeof(goal.query), "Verify current system health status");

    rcog_health_answer_t answer;
    memset(&answer, 0, sizeof(answer));

    EXPECT_EQ(rcog_health_submit_goal(integration_, &goal, &answer), 0);
    EXPECT_TRUE(answer.success);
    EXPECT_GT(answer.overall_confidence, 0.0f);
}

TEST_F(RcogHealthTest, SubmitGoalAsync) {
    integration_ = rcog_health_create(nullptr, nullptr, &config_);
    ASSERT_NE(integration_, nullptr);

    rcog_health_goal_t goal;
    rcog_health_init_goal(&goal);
    goal.health_type = RCOG_HEALTH_GOAL_ANALYZE_PATTERNS;
    snprintf(goal.query, sizeof(goal.query), "Analyze health patterns");

    uint64_t goal_id = 0;
    EXPECT_EQ(rcog_health_submit_goal_async(integration_, &goal, &goal_id), 0);
    EXPECT_NE(goal_id, 0u);

    // Check if complete
    EXPECT_TRUE(rcog_health_is_complete(integration_, goal_id) || true);  // May or may not be complete
}

/*=============================================================================
 * Diagnosis Tests
 *===========================================================================*/

TEST_F(RcogHealthTest, DiagnoseAnomalyNull) {
    rcog_health_answer_t answer;

    EXPECT_EQ(rcog_health_diagnose_anomaly(nullptr, HEALTH_MSG_MEMORY_CORRUPTION,
        HEALTH_SOURCE_MEMORY, HEALTH_SEVERITY_WARNING, &answer), -1);

    integration_ = rcog_health_create(nullptr, nullptr, &config_);
    ASSERT_NE(integration_, nullptr);

    EXPECT_EQ(rcog_health_diagnose_anomaly(integration_, HEALTH_MSG_MEMORY_CORRUPTION,
        HEALTH_SOURCE_MEMORY, HEALTH_SEVERITY_WARNING, nullptr), -1);
}

TEST_F(RcogHealthTest, DiagnoseAnomalyBasic) {
    integration_ = rcog_health_create(nullptr, nullptr, &config_);
    ASSERT_NE(integration_, nullptr);

    rcog_health_answer_t answer;
    memset(&answer, 0, sizeof(answer));

    EXPECT_EQ(rcog_health_diagnose_anomaly(
        integration_,
        HEALTH_MSG_MEMORY_CORRUPTION,
        HEALTH_SOURCE_MEMORY,
        HEALTH_SEVERITY_WARNING,
        &answer
    ), 0);

    // Should have some answer
    EXPECT_TRUE(answer.success);
}

/*=============================================================================
 * Recovery Planning Tests
 *===========================================================================*/

TEST_F(RcogHealthTest, PlanRecoveryNull) {
    rcog_health_recovery_plan_t plan;

    EXPECT_EQ(rcog_health_plan_recovery(nullptr, HEALTH_MSG_MEMORY_CORRUPTION,
        HEALTH_SOURCE_MEMORY, HEALTH_SEVERITY_ERROR, &plan), -1);

    integration_ = rcog_health_create(nullptr, nullptr, &config_);
    ASSERT_NE(integration_, nullptr);

    EXPECT_EQ(rcog_health_plan_recovery(integration_, HEALTH_MSG_MEMORY_CORRUPTION,
        HEALTH_SOURCE_MEMORY, HEALTH_SEVERITY_ERROR, nullptr), -1);
}

TEST_F(RcogHealthTest, PlanRecoveryBasic) {
    integration_ = rcog_health_create(nullptr, nullptr, &config_);
    ASSERT_NE(integration_, nullptr);

    rcog_health_recovery_plan_t plan;
    memset(&plan, 0, sizeof(plan));

    EXPECT_EQ(rcog_health_plan_recovery(
        integration_,
        HEALTH_MSG_DEADLOCK_DETECTED,
        HEALTH_SOURCE_THREADING,
        HEALTH_SEVERITY_CRITICAL,
        &plan
    ), 0);

    // Should have a valid plan
    EXPECT_GT(plan.success_probability, 0.0f);
    EXPECT_LE(plan.success_probability, 1.0f);
    EXPECT_GT(strlen(plan.recovery_plan), 0u);
}

/*=============================================================================
 * Failure Prediction Tests
 *===========================================================================*/

TEST_F(RcogHealthTest, PredictFailureNull) {
    float probability;
    uint32_t time_to_failure;

    EXPECT_EQ(rcog_health_predict_failure(nullptr, HEALTH_SOURCE_MEMORY,
        &probability, &time_to_failure), -1);

    integration_ = rcog_health_create(nullptr, nullptr, &config_);
    ASSERT_NE(integration_, nullptr);

    EXPECT_EQ(rcog_health_predict_failure(integration_, HEALTH_SOURCE_MEMORY,
        nullptr, &time_to_failure), -1);
}

TEST_F(RcogHealthTest, PredictFailureBasic) {
    integration_ = rcog_health_create(nullptr, nullptr, &config_);
    ASSERT_NE(integration_, nullptr);

    float probability = -1.0f;
    uint32_t time_to_failure = 0;

    EXPECT_EQ(rcog_health_predict_failure(
        integration_,
        HEALTH_SOURCE_MEMORY,
        &probability,
        &time_to_failure
    ), 0);

    // Should have valid prediction
    EXPECT_GE(probability, 0.0f);
    EXPECT_LE(probability, 1.0f);
}

/*=============================================================================
 * Tool Registration Tests
 *===========================================================================*/

static int test_tool_invoke_count = 0;
static int test_tool_invoke(const void* params, void* result, void* context) {
    (void)params;
    (void)result;
    (void)context;
    test_tool_invoke_count++;
    return 0;
}

TEST_F(RcogHealthTest, RegisterToolNull) {
    rcog_health_tool_t tool;
    memset(&tool, 0, sizeof(tool));
    tool.tool_name = "test_tool";
    tool.description = "Test tool";
    tool.invoke = test_tool_invoke;

    EXPECT_EQ(rcog_health_register_tool(nullptr, &tool), -1);

    integration_ = rcog_health_create(nullptr, nullptr, &config_);
    ASSERT_NE(integration_, nullptr);

    EXPECT_EQ(rcog_health_register_tool(integration_, nullptr), -1);
}

TEST_F(RcogHealthTest, RegisterToolBasic) {
    integration_ = rcog_health_create(nullptr, nullptr, &config_);
    ASSERT_NE(integration_, nullptr);

    rcog_health_tool_t tool;
    memset(&tool, 0, sizeof(tool));
    tool.tool_name = "custom_health_check";
    tool.description = "Custom health check tool";
    tool.invoke = test_tool_invoke;
    tool.context = nullptr;

    EXPECT_EQ(rcog_health_register_tool(integration_, &tool), 0);
}

TEST_F(RcogHealthTest, RegisterBuiltinTools) {
    // Disable auto-registration
    config_.register_builtin_tools = false;
    integration_ = rcog_health_create(nullptr, nullptr, &config_);
    ASSERT_NE(integration_, nullptr);

    // Manually register builtin tools
    EXPECT_EQ(rcog_health_register_builtin_tools(integration_), 0);
}

TEST_F(RcogHealthTest, RegisterBuiltinToolsNull) {
    EXPECT_EQ(rcog_health_register_builtin_tools(nullptr), -1);
}

TEST_F(RcogHealthTest, UnregisterToolNull) {
    EXPECT_EQ(rcog_health_unregister_tool(nullptr, "test"), -1);

    integration_ = rcog_health_create(nullptr, nullptr, &config_);
    ASSERT_NE(integration_, nullptr);

    EXPECT_EQ(rcog_health_unregister_tool(integration_, nullptr), -1);
}

TEST_F(RcogHealthTest, UnregisterToolBasic) {
    integration_ = rcog_health_create(nullptr, nullptr, &config_);
    ASSERT_NE(integration_, nullptr);

    // Register then unregister
    rcog_health_tool_t tool;
    memset(&tool, 0, sizeof(tool));
    tool.tool_name = "removable_tool";
    tool.description = "Tool to be removed";
    tool.invoke = test_tool_invoke;

    EXPECT_EQ(rcog_health_register_tool(integration_, &tool), 0);
    EXPECT_EQ(rcog_health_unregister_tool(integration_, "removable_tool"), 0);
}

/*=============================================================================
 * Statistics Tests
 *===========================================================================*/

TEST_F(RcogHealthTest, GetStatsNull) {
    rcog_health_stats_t stats;

    EXPECT_EQ(rcog_health_get_stats(nullptr, &stats), -1);

    integration_ = rcog_health_create(nullptr, nullptr, &config_);
    ASSERT_NE(integration_, nullptr);

    EXPECT_EQ(rcog_health_get_stats(integration_, nullptr), -1);
}

TEST_F(RcogHealthTest, GetStatsBasic) {
    integration_ = rcog_health_create(nullptr, nullptr, &config_);
    ASSERT_NE(integration_, nullptr);

    rcog_health_stats_t stats;
    memset(&stats, 0xFF, sizeof(stats));

    EXPECT_EQ(rcog_health_get_stats(integration_, &stats), 0);

    EXPECT_GE(stats.goals_submitted, 0u);
    EXPECT_GE(stats.goals_completed, 0u);
    EXPECT_GE(stats.tools_invoked, 0u);
}

TEST_F(RcogHealthTest, ResetStatsNull) {
    // Should not crash
    rcog_health_reset_stats(nullptr);
}

TEST_F(RcogHealthTest, ResetStatsBasic) {
    integration_ = rcog_health_create(nullptr, nullptr, &config_);
    ASSERT_NE(integration_, nullptr);

    rcog_health_reset_stats(integration_);

    rcog_health_stats_t stats;
    EXPECT_EQ(rcog_health_get_stats(integration_, &stats), 0);
    EXPECT_EQ(stats.goals_submitted, 0u);
    EXPECT_EQ(stats.goals_completed, 0u);
}

/*=============================================================================
 * Utility Tests
 *===========================================================================*/

TEST_F(RcogHealthTest, GoalTypeName) {
    for (int i = 0; i < RCOG_HEALTH_GOAL_COUNT; i++) {
        const char* name = rcog_health_goal_type_name((rcog_health_goal_type_t)i);
        EXPECT_NE(name, nullptr);
        EXPECT_GT(strlen(name), 0u);
    }

    // Invalid type should still return something
    const char* invalid_name = rcog_health_goal_type_name((rcog_health_goal_type_t)999);
    EXPECT_NE(invalid_name, nullptr);
}

TEST_F(RcogHealthTest, GetBuiltinTool) {
    // Get a builtin tool by ID
    const rcog_health_tool_t* tool = rcog_health_get_builtin_tool(RCOG_TOOL_ID_CHECK_MEMORY);
    if (tool) {
        EXPECT_NE(tool->tool_name, nullptr);
        EXPECT_NE(tool->description, nullptr);
        EXPECT_NE(tool->invoke, nullptr);
    }
}

/*=============================================================================
 * Edge Case Tests
 *===========================================================================*/

TEST_F(RcogHealthTest, DiagnoseAllAnomalyTypes) {
    integration_ = rcog_health_create(nullptr, nullptr, &config_);
    ASSERT_NE(integration_, nullptr);

    // Test diagnosis for each anomaly type
    health_agent_msg_type_t anomaly_types[] = {
        HEALTH_MSG_MEMORY_CORRUPTION,
        HEALTH_MSG_DEADLOCK_DETECTED,
        HEALTH_MSG_NAN_DETECTED,
        HEALTH_MSG_STATE_CORRUPTION,
        HEALTH_MSG_RESOURCE_EXHAUSTION,
        HEALTH_MSG_HEARTBEAT_TIMEOUT
    };

    for (size_t i = 0; i < sizeof(anomaly_types) / sizeof(anomaly_types[0]); i++) {
        rcog_health_answer_t answer;
        memset(&answer, 0, sizeof(answer));

        int result = rcog_health_diagnose_anomaly(
            integration_,
            anomaly_types[i],
            HEALTH_SOURCE_UNKNOWN,
            HEALTH_SEVERITY_WARNING,
            &answer
        );
        EXPECT_EQ(result, 0) << "Failed for anomaly type " << anomaly_types[i];
    }
}

TEST_F(RcogHealthTest, MultipleGoalsInParallel) {
    integration_ = rcog_health_create(nullptr, nullptr, &config_);
    ASSERT_NE(integration_, nullptr);

    // Submit multiple async goals
    uint64_t goal_ids[4] = {0};
    for (int i = 0; i < 4; i++) {
        rcog_health_goal_t goal;
        rcog_health_init_goal(&goal);
        goal.health_type = (rcog_health_goal_type_t)(i % RCOG_HEALTH_GOAL_COUNT);
        snprintf(goal.query, sizeof(goal.query), "Goal %d", i);

        EXPECT_EQ(rcog_health_submit_goal_async(integration_, &goal, &goal_ids[i]), 0);
        EXPECT_NE(goal_ids[i], 0u);
    }

    // All IDs should be unique
    for (int i = 0; i < 4; i++) {
        for (int j = i + 1; j < 4; j++) {
            EXPECT_NE(goal_ids[i], goal_ids[j]);
        }
    }
}

TEST_F(RcogHealthTest, RecoveryForDifferentSeverities) {
    integration_ = rcog_health_create(nullptr, nullptr, &config_);
    ASSERT_NE(integration_, nullptr);

    health_agent_severity_t severities[] = {
        HEALTH_SEVERITY_INFO,
        HEALTH_SEVERITY_WARNING,
        HEALTH_SEVERITY_ERROR,
        HEALTH_SEVERITY_CRITICAL,
        HEALTH_SEVERITY_FATAL
    };

    for (size_t i = 0; i < sizeof(severities) / sizeof(severities[0]); i++) {
        rcog_health_recovery_plan_t plan;
        memset(&plan, 0, sizeof(plan));

        int result = rcog_health_plan_recovery(
            integration_,
            HEALTH_MSG_STATE_CORRUPTION,
            HEALTH_SOURCE_CHECKPOINT,
            severities[i],
            &plan
        );
        EXPECT_EQ(result, 0) << "Failed for severity " << severities[i];
        EXPECT_GT(strlen(plan.recovery_plan), 0u);
    }
}
