/**
 * @file test_nimcp_prefrontal_adapter.cpp
 * @brief Unit tests for nimcp_prefrontal_adapter.c
 *
 * WHAT: Comprehensive unit tests for the Prefrontal Cortex adapter
 * WHY:  Ensure correct integration of executive functions, planning, decision-making,
 *       and impulse control sub-modules
 * HOW:  Use Google Test framework to test lifecycle, goal management, planning,
 *       decision-making, inhibitory control, working memory, and statistics tracking.
 *
 * COVERAGE TARGET: 100%
 */

#include <gtest/gtest.h>
#include <string.h>
#include <stdlib.h>

// Headers have their own extern "C" guards
#include "core/brain/regions/prefrontal/nimcp_prefrontal_adapter.h"

// Test Fixture for Prefrontal Adapter
class PrefrontalAdapterTest : public ::testing::Test {
protected:
    prefrontal_adapter_t* adapter;
    prefrontal_config_t config;

    void SetUp() override {
        config = prefrontal_default_config();
        config.enable_bio_async = false;  // Disable for unit tests
        adapter = prefrontal_create(&config);
        ASSERT_NE(nullptr, adapter) << "Failed to create Prefrontal adapter";
    }

    void TearDown() override {
        prefrontal_destroy(adapter);
        adapter = nullptr;
    }

    // Helper to create a test goal
    prefrontal_goal_t create_test_goal(const char* description, goal_priority_t priority, float value) {
        prefrontal_goal_t goal;
        memset(&goal, 0, sizeof(goal));
        strncpy(goal.description, description, sizeof(goal.description) - 1);
        goal.priority = priority;
        goal.value = value;
        goal.urgency = 0.5f;
        goal.state = GOAL_STATE_INACTIVE;
        return goal;
    }

    // Helper to create a test action
    prefrontal_action_t create_test_action(uint32_t action_id, action_type_t type, float value) {
        prefrontal_action_t action;
        memset(&action, 0, sizeof(action));
        action.action_id = action_id;
        action.type = type;
        action.expected_value = value;
        action.cost = 0.1f;
        action.confidence = 0.8f;
        snprintf(action.description, sizeof(action.description), "Action %u", action_id);
        return action;
    }

    // Helper to create decision options
    decision_option_t create_option(uint32_t id, float value, float prob, float risk) {
        decision_option_t opt;
        memset(&opt, 0, sizeof(opt));
        opt.action = create_test_action(id, ACTION_TYPE_COGNITIVE, value);
        opt.probability = prob;
        opt.risk = risk;
        return opt;
    }
};

// ============================================================================
// LIFECYCLE TESTS
// ============================================================================

TEST_F(PrefrontalAdapterTest, DefaultConfigHasReasonableValues) {
    prefrontal_config_t default_config = prefrontal_default_config();

    EXPECT_EQ(default_config.max_goals, PREFRONTAL_DEFAULT_MAX_GOALS);
    EXPECT_EQ(default_config.max_actions, PREFRONTAL_DEFAULT_MAX_ACTIONS);
    EXPECT_EQ(default_config.working_memory_slots, PREFRONTAL_DEFAULT_WORKING_MEMORY_SLOTS);
    EXPECT_EQ(default_config.planning_horizon, PREFRONTAL_DEFAULT_PLANNING_HORIZON);
    EXPECT_FLOAT_EQ(default_config.decision_timeout_ms, PREFRONTAL_DEFAULT_DECISION_TIMEOUT_MS);
    EXPECT_FLOAT_EQ(default_config.inhibition_threshold, PREFRONTAL_DEFAULT_INHIBITION_THRESHOLD);
    EXPECT_TRUE(default_config.enable_working_memory);
    EXPECT_TRUE(default_config.enable_conflict_monitoring);
    EXPECT_TRUE(default_config.enable_rule_learning);
}

TEST_F(PrefrontalAdapterTest, CreateWithNullConfigUsesDefaults) {
    prefrontal_adapter_t* adapter_null = prefrontal_create(NULL);
    ASSERT_NE(nullptr, adapter_null);

    prefrontal_config_t retrieved;
    EXPECT_TRUE(prefrontal_get_config(adapter_null, &retrieved));
    EXPECT_EQ(retrieved.max_goals, PREFRONTAL_DEFAULT_MAX_GOALS);

    prefrontal_destroy(adapter_null);
}

TEST_F(PrefrontalAdapterTest, DestroyNullDoesNotCrash) {
    prefrontal_destroy(NULL);
    // Should not crash
}

TEST_F(PrefrontalAdapterTest, ResetClearsState) {
    // Activate a goal first
    prefrontal_goal_t goal = create_test_goal("Test Goal", GOAL_PRIORITY_NORMAL, 1.0f);
    prefrontal_activate_goal(adapter, &goal);

    EXPECT_TRUE(prefrontal_reset(adapter));

    // Status should be idle after reset
    EXPECT_EQ(prefrontal_get_status(adapter), PREFRONTAL_STATUS_IDLE);
    EXPECT_EQ(prefrontal_get_last_error(adapter), PREFRONTAL_ERROR_NONE);
}

TEST_F(PrefrontalAdapterTest, ResetNullReturnsFalse) {
    EXPECT_FALSE(prefrontal_reset(NULL));
}

// ============================================================================
// GOAL MANAGEMENT TESTS
// ============================================================================

TEST_F(PrefrontalAdapterTest, ActivateGoalSuccess) {
    prefrontal_goal_t goal = create_test_goal("Learn new skill", GOAL_PRIORITY_HIGH, 0.8f);

    uint32_t goal_id = prefrontal_activate_goal(adapter, &goal);
    EXPECT_GT(goal_id, 0u);
}

TEST_F(PrefrontalAdapterTest, ActivateGoalNullReturnsFail) {
    EXPECT_EQ(prefrontal_activate_goal(NULL, NULL), 0u);
    EXPECT_EQ(prefrontal_activate_goal(adapter, NULL), 0u);
}

TEST_F(PrefrontalAdapterTest, ActivateMultipleGoals) {
    prefrontal_goal_t goal1 = create_test_goal("Goal 1", GOAL_PRIORITY_LOW, 0.5f);
    prefrontal_goal_t goal2 = create_test_goal("Goal 2", GOAL_PRIORITY_NORMAL, 0.7f);
    prefrontal_goal_t goal3 = create_test_goal("Goal 3", GOAL_PRIORITY_HIGH, 0.9f);

    uint32_t id1 = prefrontal_activate_goal(adapter, &goal1);
    uint32_t id2 = prefrontal_activate_goal(adapter, &goal2);
    uint32_t id3 = prefrontal_activate_goal(adapter, &goal3);

    EXPECT_GT(id1, 0u);
    EXPECT_GT(id2, 0u);
    EXPECT_GT(id3, 0u);
    EXPECT_NE(id1, id2);
    EXPECT_NE(id2, id3);
}

TEST_F(PrefrontalAdapterTest, ActivateCriticalGoalConflict) {
    prefrontal_goal_t goal1 = create_test_goal("Critical Goal 1", GOAL_PRIORITY_CRITICAL, 1.0f);
    prefrontal_goal_t goal2 = create_test_goal("Critical Goal 2", GOAL_PRIORITY_CRITICAL, 1.0f);

    uint32_t id1 = prefrontal_activate_goal(adapter, &goal1);
    EXPECT_GT(id1, 0u);

    uint32_t id2 = prefrontal_activate_goal(adapter, &goal2);
    EXPECT_EQ(id2, 0u);  // Should fail due to conflict
    EXPECT_EQ(prefrontal_get_last_error(adapter), PREFRONTAL_ERROR_GOAL_CONFLICT);
}

TEST_F(PrefrontalAdapterTest, DeactivateGoalSuccess) {
    prefrontal_goal_t goal = create_test_goal("Deactivate Test", GOAL_PRIORITY_NORMAL, 0.6f);
    uint32_t goal_id = prefrontal_activate_goal(adapter, &goal);
    ASSERT_GT(goal_id, 0u);

    EXPECT_TRUE(prefrontal_deactivate_goal(adapter, goal_id, GOAL_STATE_ACHIEVED));
}

TEST_F(PrefrontalAdapterTest, DeactivateInvalidGoalReturnsFalse) {
    EXPECT_FALSE(prefrontal_deactivate_goal(adapter, 9999, GOAL_STATE_FAILED));
}

TEST_F(PrefrontalAdapterTest, GetActiveGoals) {
    prefrontal_goal_t goal1 = create_test_goal("Active Goal 1", GOAL_PRIORITY_NORMAL, 0.5f);
    prefrontal_goal_t goal2 = create_test_goal("Active Goal 2", GOAL_PRIORITY_HIGH, 0.8f);

    prefrontal_activate_goal(adapter, &goal1);
    prefrontal_activate_goal(adapter, &goal2);

    prefrontal_goal_t active_goals[8];
    uint32_t count = 8;

    EXPECT_TRUE(prefrontal_get_active_goals(adapter, active_goals, &count));
    EXPECT_EQ(count, 2u);
}

TEST_F(PrefrontalAdapterTest, UpdateGoalProgress) {
    prefrontal_goal_t goal = create_test_goal("Progress Test", GOAL_PRIORITY_NORMAL, 0.7f);
    uint32_t goal_id = prefrontal_activate_goal(adapter, &goal);
    ASSERT_GT(goal_id, 0u);

    EXPECT_TRUE(prefrontal_update_goal_progress(adapter, goal_id, 0.5f));
    EXPECT_TRUE(prefrontal_update_goal_progress(adapter, goal_id, 1.0f));  // Should mark achieved
}

// ============================================================================
// PLANNING TESTS
// ============================================================================

TEST_F(PrefrontalAdapterTest, GeneratePlanSuccess) {
    prefrontal_goal_t goal = create_test_goal("Plan Test Goal", GOAL_PRIORITY_HIGH, 0.9f);
    uint32_t goal_id = prefrontal_activate_goal(adapter, &goal);
    ASSERT_GT(goal_id, 0u);

    action_plan_t plan;
    EXPECT_TRUE(prefrontal_generate_plan(adapter, goal_id, &plan));
    EXPECT_GT(plan.action_count, 0u);
    EXPECT_EQ(plan.goal_id, goal_id);
    EXPECT_GT(plan.expected_total_value, 0.0f);
}

TEST_F(PrefrontalAdapterTest, GeneratePlanForInvalidGoal) {
    action_plan_t plan;
    EXPECT_FALSE(prefrontal_generate_plan(adapter, 9999, &plan));
    EXPECT_EQ(prefrontal_get_last_error(adapter), PREFRONTAL_ERROR_PLANNING_FAILURE);
}

TEST_F(PrefrontalAdapterTest, GetNextAction) {
    prefrontal_goal_t goal = create_test_goal("Action Test", GOAL_PRIORITY_NORMAL, 0.8f);
    uint32_t goal_id = prefrontal_activate_goal(adapter, &goal);
    ASSERT_GT(goal_id, 0u);

    action_plan_t plan;
    ASSERT_TRUE(prefrontal_generate_plan(adapter, goal_id, &plan));
    ASSERT_GT(plan.action_count, 0u);

    prefrontal_action_t action;
    EXPECT_TRUE(prefrontal_get_next_action(adapter, goal_id, &action));
    EXPECT_GT(action.action_id, 0u);
}

TEST_F(PrefrontalAdapterTest, ReportActionOutcome) {
    prefrontal_goal_t goal = create_test_goal("Outcome Test", GOAL_PRIORITY_NORMAL, 0.7f);
    uint32_t goal_id = prefrontal_activate_goal(adapter, &goal);
    ASSERT_GT(goal_id, 0u);

    action_plan_t plan;
    ASSERT_TRUE(prefrontal_generate_plan(adapter, goal_id, &plan));

    prefrontal_action_t action;
    ASSERT_TRUE(prefrontal_get_next_action(adapter, goal_id, &action));

    EXPECT_TRUE(prefrontal_report_action_outcome(adapter, action.action_id, true, 1.0f));
}

// ============================================================================
// DECISION-MAKING TESTS
// ============================================================================

TEST_F(PrefrontalAdapterTest, EvaluateOptionsBasic) {
    decision_option_t options[3];
    options[0] = create_option(1, 0.5f, 0.8f, 0.1f);
    options[1] = create_option(2, 0.8f, 0.7f, 0.2f);
    options[2] = create_option(3, 0.3f, 0.9f, 0.05f);

    decision_result_t result;
    EXPECT_TRUE(prefrontal_evaluate_options(adapter, options, 3, &result));
    EXPECT_NE(result.selected_option, nullptr);
    EXPECT_EQ(result.options_evaluated, 3u);
    EXPECT_GT(result.decision_confidence, 0.0f);
}

TEST_F(PrefrontalAdapterTest, EvaluateOptionsNullParams) {
    decision_option_t options[2];
    decision_result_t result;

    EXPECT_FALSE(prefrontal_evaluate_options(NULL, options, 2, &result));
    EXPECT_FALSE(prefrontal_evaluate_options(adapter, NULL, 2, &result));
    EXPECT_FALSE(prefrontal_evaluate_options(adapter, options, 0, &result));
    EXPECT_FALSE(prefrontal_evaluate_options(adapter, options, 2, NULL));
}

TEST_F(PrefrontalAdapterTest, QuickDecision) {
    decision_option_t options[3];
    options[0] = create_option(1, 0.3f, 0.9f, 0.1f);
    options[1] = create_option(2, 0.9f, 0.7f, 0.2f);  // Highest expected value
    options[2] = create_option(3, 0.6f, 0.8f, 0.15f);

    uint32_t selected;
    EXPECT_TRUE(prefrontal_quick_decision(adapter, options, 3, &selected));
    EXPECT_EQ(selected, 1u);  // Index of highest value option
}

TEST_F(PrefrontalAdapterTest, GetDecisionStateWhenIdle) {
    uint32_t conflict_level, dominant_option;
    EXPECT_FALSE(prefrontal_get_decision_state(adapter, &conflict_level, &dominant_option));
}

// ============================================================================
// INHIBITORY CONTROL TESTS
// ============================================================================

TEST_F(PrefrontalAdapterTest, CheckInhibitionNoConflict) {
    prefrontal_action_t action = create_test_action(1, ACTION_TYPE_COGNITIVE, 0.5f);
    impulse_record_t record;

    bool should_inhibit = prefrontal_check_inhibition(adapter, &action, &record);
    // Without active goals, should not trigger inhibition
    EXPECT_FALSE(should_inhibit);
}

TEST_F(PrefrontalAdapterTest, CheckInhibitionWithConflict) {
    // First activate a goal
    prefrontal_goal_t goal = create_test_goal("Active Goal", GOAL_PRIORITY_HIGH, 0.6f);
    uint32_t goal_id = prefrontal_activate_goal(adapter, &goal);
    ASSERT_GT(goal_id, 0u);

    // Create high-value impulsive action that conflicts
    prefrontal_action_t impulse = create_test_action(999, ACTION_TYPE_MOTOR, 1.0f);
    impulse.expected_value = 1.0f;
    impulse.goal_id = 0;  // Not associated with active goal

    impulse_record_t record;
    prefrontal_check_inhibition(adapter, &impulse, &record);

    // Check that record was populated
    EXPECT_GT(record.impulse_strength, 0.0f);
}

TEST_F(PrefrontalAdapterTest, SuppressImpulse) {
    prefrontal_action_t impulse = create_test_action(1, ACTION_TYPE_MOTOR, 0.8f);

    EXPECT_TRUE(prefrontal_suppress_impulse(adapter, &impulse, "Test suppression"));
}

TEST_F(PrefrontalAdapterTest, GetInhibitionStats) {
    float success_rate, fatigue_level;
    EXPECT_TRUE(prefrontal_get_inhibition_stats(adapter, &success_rate, &fatigue_level));
    EXPECT_GE(success_rate, 0.0f);
    EXPECT_LE(success_rate, 1.0f);
    EXPECT_GE(fatigue_level, 0.0f);
    EXPECT_LE(fatigue_level, 1.0f);
}

// ============================================================================
// WORKING MEMORY TESTS
// ============================================================================

TEST_F(PrefrontalAdapterTest, WMPushSuccess) {
    EXPECT_TRUE(prefrontal_wm_push(adapter, 42, 0.8f, 0));
}

TEST_F(PrefrontalAdapterTest, WMPushMultiple) {
    for (uint32_t i = 0; i < 5; i++) {
        EXPECT_TRUE(prefrontal_wm_push(adapter, i + 1, 0.5f + i * 0.1f, 0));
    }
}

TEST_F(PrefrontalAdapterTest, WMPushOverflow) {
    // Fill working memory beyond capacity
    for (uint32_t i = 0; i < config.working_memory_slots + 2; i++) {
        prefrontal_wm_push(adapter, i + 1, 0.1f, 0);
    }
    // Should evict lowest priority items, so this shouldn't crash
}

TEST_F(PrefrontalAdapterTest, WMUpdate) {
    ASSERT_TRUE(prefrontal_wm_push(adapter, 100, 0.5f, 0));
    EXPECT_TRUE(prefrontal_wm_update(adapter, 100, 0.9f));
}

TEST_F(PrefrontalAdapterTest, WMUpdateNonexistent) {
    EXPECT_FALSE(prefrontal_wm_update(adapter, 9999, 0.9f));
}

TEST_F(PrefrontalAdapterTest, WMGetContents) {
    ASSERT_TRUE(prefrontal_wm_push(adapter, 1, 0.5f, 0));
    ASSERT_TRUE(prefrontal_wm_push(adapter, 2, 0.6f, 0));
    ASSERT_TRUE(prefrontal_wm_push(adapter, 3, 0.7f, 0));

    uint32_t ids[10];
    float priorities[10];
    uint32_t count = 10;

    EXPECT_TRUE(prefrontal_wm_get_contents(adapter, ids, priorities, &count));
    EXPECT_GE(count, 3u);
}

// ============================================================================
// COGNITIVE FLEXIBILITY TESTS
// ============================================================================

TEST_F(PrefrontalAdapterTest, TaskSwitch) {
    float switch_cost;

    // First switch (no cost expected)
    EXPECT_TRUE(prefrontal_task_switch(adapter, 1, &switch_cost));
    EXPECT_EQ(switch_cost, 0.0f);

    // Second switch (cost expected)
    EXPECT_TRUE(prefrontal_task_switch(adapter, 2, &switch_cost));
    EXPECT_GT(switch_cost, 0.0f);
}

TEST_F(PrefrontalAdapterTest, LearnRule) {
    float context[] = {0.5f, 0.3f, 0.8f, 0.2f};
    EXPECT_TRUE(prefrontal_learn_rule(adapter, context, 4, 1, 0.9f));
}

TEST_F(PrefrontalAdapterTest, LearnRuleNullContext) {
    EXPECT_FALSE(prefrontal_learn_rule(adapter, NULL, 0, 1, 0.9f));
}

// ============================================================================
// CALLBACK TESTS
// ============================================================================

static int goal_callback_count = 0;
static void test_goal_callback(const prefrontal_goal_t* goal,
                                goal_state_t old_state,
                                goal_state_t new_state,
                                void* user_data) {
    (void)goal;
    (void)old_state;
    (void)new_state;
    (void)user_data;
    goal_callback_count++;
}

TEST_F(PrefrontalAdapterTest, SetGoalCallback) {
    goal_callback_count = 0;

    EXPECT_TRUE(prefrontal_set_goal_callback(adapter, test_goal_callback, NULL));

    prefrontal_goal_t goal = create_test_goal("Callback Test", GOAL_PRIORITY_NORMAL, 0.5f);
    prefrontal_activate_goal(adapter, &goal);

    EXPECT_GT(goal_callback_count, 0);
}

static int decision_callback_count = 0;
static void test_decision_callback(const decision_result_t* result, void* user_data) {
    (void)result;
    (void)user_data;
    decision_callback_count++;
}

TEST_F(PrefrontalAdapterTest, SetDecisionCallback) {
    decision_callback_count = 0;

    EXPECT_TRUE(prefrontal_set_decision_callback(adapter, test_decision_callback, NULL));

    decision_option_t options[2];
    options[0] = create_option(1, 0.5f, 0.8f, 0.1f);
    options[1] = create_option(2, 0.6f, 0.7f, 0.15f);

    decision_result_t result;
    prefrontal_evaluate_options(adapter, options, 2, &result);

    EXPECT_GT(decision_callback_count, 0);
}

// ============================================================================
// STATUS AND DIAGNOSTICS TESTS
// ============================================================================

TEST_F(PrefrontalAdapterTest, GetStatusIdle) {
    EXPECT_EQ(prefrontal_get_status(adapter), PREFRONTAL_STATUS_IDLE);
}

TEST_F(PrefrontalAdapterTest, GetStatusNull) {
    EXPECT_EQ(prefrontal_get_status(NULL), PREFRONTAL_STATUS_ERROR);
}

TEST_F(PrefrontalAdapterTest, GetLastErrorNone) {
    EXPECT_EQ(prefrontal_get_last_error(adapter), PREFRONTAL_ERROR_NONE);
}

TEST_F(PrefrontalAdapterTest, GetLastErrorNull) {
    EXPECT_NE(prefrontal_get_last_error(NULL), PREFRONTAL_ERROR_NONE);
}

TEST_F(PrefrontalAdapterTest, ErrorStringNotNull) {
    const char* str = prefrontal_error_string(PREFRONTAL_ERROR_NONE);
    EXPECT_NE(str, nullptr);
    EXPECT_GT(strlen(str), 0u);

    str = prefrontal_error_string(PREFRONTAL_ERROR_INVALID_INPUT);
    EXPECT_NE(str, nullptr);

    str = prefrontal_error_string(PREFRONTAL_ERROR_GOAL_CONFLICT);
    EXPECT_NE(str, nullptr);

    str = prefrontal_error_string(PREFRONTAL_ERROR_PLANNING_FAILURE);
    EXPECT_NE(str, nullptr);

    str = prefrontal_error_string(PREFRONTAL_ERROR_DECISION_TIMEOUT);
    EXPECT_NE(str, nullptr);
}

TEST_F(PrefrontalAdapterTest, StatusStringNotNull) {
    const char* str = prefrontal_status_string(PREFRONTAL_STATUS_IDLE);
    EXPECT_NE(str, nullptr);
    EXPECT_GT(strlen(str), 0u);

    str = prefrontal_status_string(PREFRONTAL_STATUS_PLANNING);
    EXPECT_NE(str, nullptr);

    str = prefrontal_status_string(PREFRONTAL_STATUS_DECISION);
    EXPECT_NE(str, nullptr);
}

TEST_F(PrefrontalAdapterTest, GetStats) {
    prefrontal_stats_t stats;
    EXPECT_TRUE(prefrontal_get_stats(adapter, &stats));
    EXPECT_EQ(stats.goals_activated, 0u);  // No goals yet
    EXPECT_EQ(stats.decisions_made, 0u);
}

TEST_F(PrefrontalAdapterTest, GetStatsAfterOperations) {
    // Activate a goal
    prefrontal_goal_t goal = create_test_goal("Stats Test", GOAL_PRIORITY_NORMAL, 0.7f);
    prefrontal_activate_goal(adapter, &goal);

    // Make a decision
    decision_option_t options[2];
    options[0] = create_option(1, 0.5f, 0.8f, 0.1f);
    options[1] = create_option(2, 0.6f, 0.7f, 0.15f);
    decision_result_t result;
    prefrontal_evaluate_options(adapter, options, 2, &result);

    prefrontal_stats_t stats;
    EXPECT_TRUE(prefrontal_get_stats(adapter, &stats));
    EXPECT_EQ(stats.goals_activated, 1u);
    EXPECT_EQ(stats.decisions_made, 1u);
}

TEST_F(PrefrontalAdapterTest, GetConfig) {
    prefrontal_config_t retrieved;
    EXPECT_TRUE(prefrontal_get_config(adapter, &retrieved));
    EXPECT_EQ(retrieved.max_goals, config.max_goals);
    EXPECT_EQ(retrieved.max_actions, config.max_actions);
}

TEST_F(PrefrontalAdapterTest, GetStatsNull) {
    prefrontal_stats_t stats;
    EXPECT_FALSE(prefrontal_get_stats(NULL, &stats));
    EXPECT_FALSE(prefrontal_get_stats(adapter, NULL));
}

TEST_F(PrefrontalAdapterTest, GetConfigNull) {
    prefrontal_config_t config;
    EXPECT_FALSE(prefrontal_get_config(NULL, &config));
    EXPECT_FALSE(prefrontal_get_config(adapter, NULL));
}

// ============================================================================
// INTEGRATION WORKFLOW TESTS
// ============================================================================

TEST_F(PrefrontalAdapterTest, CompleteGoalPursuit) {
    // 1. Activate a goal
    prefrontal_goal_t goal = create_test_goal("Complete Task", GOAL_PRIORITY_HIGH, 0.9f);
    uint32_t goal_id = prefrontal_activate_goal(adapter, &goal);
    ASSERT_GT(goal_id, 0u);

    // 2. Generate a plan
    action_plan_t plan;
    ASSERT_TRUE(prefrontal_generate_plan(adapter, goal_id, &plan));
    ASSERT_GT(plan.action_count, 0u);

    // 3. Execute actions and report outcomes
    for (uint32_t i = 0; i < plan.action_count; i++) {
        prefrontal_action_t action;
        if (prefrontal_get_next_action(adapter, goal_id, &action)) {
            prefrontal_report_action_outcome(adapter, action.action_id, true, 0.8f);
        }
    }

    // 4. Check goal was completed (progress should have been updated)
    prefrontal_stats_t stats;
    EXPECT_TRUE(prefrontal_get_stats(adapter, &stats));
    EXPECT_GE(stats.plans_generated, 1u);
}

TEST_F(PrefrontalAdapterTest, DecisionWithInhibition) {
    // 1. Activate a goal that we want to pursue
    prefrontal_goal_t goal = create_test_goal("Important Goal", GOAL_PRIORITY_HIGH, 0.8f);
    uint32_t goal_id = prefrontal_activate_goal(adapter, &goal);
    ASSERT_GT(goal_id, 0u);

    // 2. Present options including an impulsive one
    decision_option_t options[3];
    options[0] = create_option(1, 0.4f, 0.9f, 0.1f);
    options[0].action.goal_id = goal_id;  // Aligned with goal

    options[1] = create_option(2, 0.9f, 0.6f, 0.4f);  // High reward but risky
    options[1].action.goal_id = 0;  // Not aligned

    options[2] = create_option(3, 0.6f, 0.8f, 0.2f);
    options[2].action.goal_id = goal_id;  // Aligned with goal

    // 3. Make decision
    decision_result_t result;
    EXPECT_TRUE(prefrontal_evaluate_options(adapter, options, 3, &result));
    EXPECT_NE(result.selected_option, nullptr);

    // 4. Check impulse on high-reward option
    impulse_record_t record;
    prefrontal_check_inhibition(adapter, &options[1].action, &record);
}

TEST_F(PrefrontalAdapterTest, WorkingMemoryAndPlanning) {
    // 1. Push items to working memory (context for planning)
    ASSERT_TRUE(prefrontal_wm_push(adapter, 100, 0.9f, 0));  // Important context
    ASSERT_TRUE(prefrontal_wm_push(adapter, 200, 0.7f, 0));  // Secondary context

    // 2. Activate goal
    prefrontal_goal_t goal = create_test_goal("Context-aware Goal", GOAL_PRIORITY_NORMAL, 0.7f);
    uint32_t goal_id = prefrontal_activate_goal(adapter, &goal);
    ASSERT_GT(goal_id, 0u);

    // 3. Update working memory with goal association
    ASSERT_TRUE(prefrontal_wm_push(adapter, 300, 0.95f, goal_id));

    // 4. Generate plan
    action_plan_t plan;
    EXPECT_TRUE(prefrontal_generate_plan(adapter, goal_id, &plan));

    // 5. Verify WM contents
    uint32_t ids[10];
    uint32_t count = 10;
    EXPECT_TRUE(prefrontal_wm_get_contents(adapter, ids, NULL, &count));
    EXPECT_GE(count, 3u);
}
