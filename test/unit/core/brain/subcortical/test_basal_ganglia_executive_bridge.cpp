/**
 * @file test_basal_ganglia_executive_bridge.cpp
 * @brief Unit tests for BG-executive prefrontal bridge
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/brain/subcortical/nimcp_basal_ganglia_executive_bridge.h"

class BGExecutiveBridgeTest : public ::testing::Test {
protected:
    bge_bridge_t* bridge = nullptr;
    basal_ganglia_t* bg = nullptr;
    executive_controller_t* exec = nullptr;

    void SetUp() override {
        bge_bridge_config_t config;
        bge_bridge_default_config(&config);
        bridge = bge_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);

        bg = basal_ganglia_create(nullptr);
        exec = executive_create();
    }

    void TearDown() override {
        if (bridge) bge_bridge_destroy(bridge);
        if (bg) basal_ganglia_destroy(bg);
        if (exec) executive_destroy(exec);
    }
};

// ============================================================================
// Lifecycle Tests
// ============================================================================

TEST_F(BGExecutiveBridgeTest, CreateDestroy) {
    EXPECT_NE(bridge, nullptr);
}

TEST_F(BGExecutiveBridgeTest, CreateWithNullConfig) {
    bge_bridge_t* b = bge_bridge_create(nullptr);
    ASSERT_NE(b, nullptr);
    bge_bridge_destroy(b);
}

TEST_F(BGExecutiveBridgeTest, DefaultConfig) {
    bge_bridge_config_t config;
    bge_bridge_default_config(&config);

    EXPECT_FLOAT_EQ(config.pfc_weight, BGE_DEFAULT_PFC_WEIGHT);
    EXPECT_FLOAT_EQ(config.load_threshold, BGE_DEFAULT_LOAD_THRESHOLD);
    EXPECT_TRUE(config.enable_load_monitoring);
    EXPECT_TRUE(config.enable_conflict_detection);
    EXPECT_TRUE(config.enable_switch_cost);
}

TEST_F(BGExecutiveBridgeTest, Reset) {
    uint32_t goal_id;
    bge_bridge_register_goal(bridge, 0, 0.5f, 1.0f, &goal_id);

    int ret = bge_bridge_reset(bridge);
    EXPECT_EQ(ret, 0);

    EXPECT_EQ(bge_bridge_get_mode(bridge), BGE_CONTROL_GOAL_DIRECTED);
    EXPECT_FLOAT_EQ(bge_bridge_get_pfc_influence(bridge), 1.0f);
}

// ============================================================================
// Connection Tests
// ============================================================================

TEST_F(BGExecutiveBridgeTest, ConnectBG) {
    int ret = bge_bridge_connect_bg(bridge, bg);
    EXPECT_EQ(ret, 0);
}

TEST_F(BGExecutiveBridgeTest, ConnectExecutive) {
    int ret = bge_bridge_connect_executive(bridge, exec);
    EXPECT_EQ(ret, 0);
}

TEST_F(BGExecutiveBridgeTest, FullConnection) {
    bge_bridge_connect_bg(bridge, bg);
    bge_bridge_connect_executive(bridge, exec);

    EXPECT_TRUE(bge_bridge_is_connected(bridge));
}

TEST_F(BGExecutiveBridgeTest, NotConnectedInitially) {
    EXPECT_FALSE(bge_bridge_is_connected(bridge));
}

// ============================================================================
// Goal Management Tests
// ============================================================================

TEST_F(BGExecutiveBridgeTest, RegisterGoal) {
    uint32_t goal_id;
    int ret = bge_bridge_register_goal(bridge, 0, 0.8f, 1.0f, &goal_id);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(goal_id, 0u);
}

TEST_F(BGExecutiveBridgeTest, GoalState) {
    uint32_t goal_id;
    bge_bridge_register_goal(bridge, 0, 0.8f, 1.0f, &goal_id);

    bge_goal_state_t state = bge_bridge_get_goal_state(bridge, goal_id);
    EXPECT_EQ(state, BGE_GOAL_PENDING);
}

TEST_F(BGExecutiveBridgeTest, GoalAchieved) {
    uint32_t goal_id;
    bge_bridge_register_goal(bridge, 0, 0.8f, 1.0f, &goal_id);

    int ret = bge_bridge_goal_achieved(bridge, goal_id);
    EXPECT_EQ(ret, 0);

    EXPECT_EQ(bge_bridge_get_goal_state(bridge, goal_id), BGE_GOAL_ACHIEVED);
}

TEST_F(BGExecutiveBridgeTest, AbandonGoal) {
    uint32_t goal_id;
    bge_bridge_register_goal(bridge, 0, 0.8f, 1.0f, &goal_id);

    int ret = bge_bridge_abandon_goal(bridge, goal_id);
    EXPECT_EQ(ret, 0);

    EXPECT_EQ(bge_bridge_get_goal_state(bridge, goal_id), BGE_GOAL_ABANDONED);
}

TEST_F(BGExecutiveBridgeTest, GetTopGoal) {
    uint32_t goal1, goal2, goal3;
    bge_bridge_register_goal(bridge, 0, 0.3f, 1.0f, &goal1);
    bge_bridge_register_goal(bridge, 1, 0.9f, 1.0f, &goal2);  // Highest priority
    bge_bridge_register_goal(bridge, 2, 0.5f, 1.0f, &goal3);

    uint32_t top = bge_bridge_get_top_goal(bridge);
    EXPECT_EQ(top, goal2);
}

TEST_F(BGExecutiveBridgeTest, MaxGoals) {
    // Try to exceed max goals
    for (int i = 0; i < BGE_MAX_ACTIVE_GOALS + 2; i++) {
        uint32_t goal_id;
        int ret = bge_bridge_register_goal(bridge, i, 0.5f, 1.0f, &goal_id);
        if (i >= BGE_MAX_ACTIVE_GOALS) {
            EXPECT_EQ(ret, -1);
        } else {
            EXPECT_EQ(ret, 0);
        }
    }
}

// ============================================================================
// Control Mode Tests
// ============================================================================

TEST_F(BGExecutiveBridgeTest, UpdateControl) {
    bge_bridge_connect_executive(bridge, exec);

    int ret = bge_bridge_update_control(bridge, 1000);
    EXPECT_EQ(ret, 0);
}

TEST_F(BGExecutiveBridgeTest, GetMode) {
    EXPECT_EQ(bge_bridge_get_mode(bridge), BGE_CONTROL_GOAL_DIRECTED);
}

TEST_F(BGExecutiveBridgeTest, SetMode) {
    int ret = bge_bridge_set_mode(bridge, BGE_CONTROL_HABITUAL);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(bge_bridge_get_mode(bridge), BGE_CONTROL_HABITUAL);
}

TEST_F(BGExecutiveBridgeTest, ApplyControl) {
    float action_values[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};

    int ret = bge_bridge_apply_control(bridge, action_values, 8);
    EXPECT_EQ(ret, 0);
}

TEST_F(BGExecutiveBridgeTest, GoalBoostsTargetAction) {
    uint32_t goal_id;
    bge_bridge_register_goal(bridge, 2, 1.0f, 1.0f, &goal_id);

    float action_values[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    float original = action_values[2];

    bge_bridge_apply_control(bridge, action_values, 8);

    // Goal-relevant action should be boosted
    EXPECT_GT(action_values[2], original);
}

TEST_F(BGExecutiveBridgeTest, PFCInfluence) {
    float pfc = bge_bridge_get_pfc_influence(bridge);
    EXPECT_GT(pfc, 0.0f);
    EXPECT_LE(pfc, 1.0f);
}

// ============================================================================
// Inhibition Tests
// ============================================================================

TEST_F(BGExecutiveBridgeTest, InhibitAction) {
    int ret = bge_bridge_inhibit_action(bridge, 3, 0.8f);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(bge_bridge_is_inhibited(bridge, 3));
}

TEST_F(BGExecutiveBridgeTest, ReleaseInhibition) {
    bge_bridge_inhibit_action(bridge, 3, 0.8f);
    int ret = bge_bridge_release_inhibition(bridge, 3);
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(bge_bridge_is_inhibited(bridge, 3));
}

TEST_F(BGExecutiveBridgeTest, GlobalStop) {
    int ret = bge_bridge_global_stop(bridge);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(bge_bridge_get_mode(bridge), BGE_CONTROL_SUPPRESSED);
}

TEST_F(BGExecutiveBridgeTest, InhibitedActionSuppressed) {
    bge_bridge_inhibit_action(bridge, 2, 0.9f);

    float action_values[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    float original = action_values[2];

    bge_bridge_apply_control(bridge, action_values, 8);

    EXPECT_LT(action_values[2], original);
}

// ============================================================================
// Conflict Detection Tests
// ============================================================================

TEST_F(BGExecutiveBridgeTest, NoConflictInitially) {
    float level = bge_bridge_get_conflict(bridge);
    EXPECT_FLOAT_EQ(level, 0.0f);
}

TEST_F(BGExecutiveBridgeTest, DetectConflict) {
    bge_conflict_type_t type;
    float level;

    bool conflict = bge_bridge_detect_conflict(bridge, &type, &level);
    // Initially no conflict expected
    EXPECT_FALSE(conflict);
    EXPECT_EQ(type, BGE_CONFLICT_NONE);
}

TEST_F(BGExecutiveBridgeTest, GoalGoalConflict) {
    // Create multiple high-priority goals
    uint32_t goal1, goal2;
    bge_bridge_register_goal(bridge, 0, 0.9f, 1.0f, &goal1);
    bge_bridge_register_goal(bridge, 1, 0.9f, 1.0f, &goal2);

    bge_conflict_type_t type;
    float level;
    bge_bridge_detect_conflict(bridge, &type, &level);

    EXPECT_EQ(type, BGE_CONFLICT_GOAL_GOAL);
    EXPECT_GT(level, 0.0f);
}

// ============================================================================
// Task Switch Tests
// ============================================================================

TEST_F(BGExecutiveBridgeTest, TaskSwitch) {
    int ret = bge_bridge_task_switch(bridge, 1, 1000);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(bge_bridge_in_switch_cost(bridge));
}

TEST_F(BGExecutiveBridgeTest, SwitchCostExpires) {
    bge_bridge_task_switch(bridge, 1, 1000);

    // Check remaining time
    float remaining = bge_bridge_get_switch_cost_remaining(bridge, 1000);
    EXPECT_GT(remaining, 0.0f);

    // After switch cost duration
    remaining = bge_bridge_get_switch_cost_remaining(bridge, 1500);
    EXPECT_LT(remaining, 200.0f);
}

TEST_F(BGExecutiveBridgeTest, NotInSwitchCostInitially) {
    EXPECT_FALSE(bge_bridge_in_switch_cost(bridge));
}

// ============================================================================
// State Query Tests
// ============================================================================

TEST_F(BGExecutiveBridgeTest, GetControlState) {
    bge_control_state_t state;
    int ret = bge_bridge_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(state.mode, BGE_CONTROL_GOAL_DIRECTED);
}

TEST_F(BGExecutiveBridgeTest, GetCognitiveLoad) {
    float load = bge_bridge_get_cognitive_load(bridge);
    EXPECT_GE(load, 0.0f);
    EXPECT_LE(load, 1.0f);
}

TEST_F(BGExecutiveBridgeTest, GetHabitPressure) {
    float pressure = bge_bridge_get_habit_pressure(bridge);
    EXPECT_GE(pressure, 0.0f);
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_F(BGExecutiveBridgeTest, Statistics) {
    float action_values[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};

    for (int i = 0; i < 10; i++) {
        bge_bridge_apply_control(bridge, action_values, 8);
    }

    bge_bridge_stats_t stats;
    int ret = bge_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(stats.total_decisions, 10u);
}

TEST_F(BGExecutiveBridgeTest, ResetStats) {
    float action_values[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    bge_bridge_apply_control(bridge, action_values, 8);

    bge_bridge_reset_stats(bridge);

    bge_bridge_stats_t stats;
    bge_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_decisions, 0u);
}

// ============================================================================
// Utility Tests
// ============================================================================

TEST_F(BGExecutiveBridgeTest, ControlModeNames) {
    EXPECT_STREQ(bge_control_mode_name(BGE_CONTROL_GOAL_DIRECTED), "goal_directed");
    EXPECT_STREQ(bge_control_mode_name(BGE_CONTROL_HABITUAL), "habitual");
    EXPECT_STREQ(bge_control_mode_name(BGE_CONTROL_MIXED), "mixed");
    EXPECT_STREQ(bge_control_mode_name(BGE_CONTROL_SUPPRESSED), "suppressed");
}

TEST_F(BGExecutiveBridgeTest, GoalStateNames) {
    EXPECT_STREQ(bge_goal_state_name(BGE_GOAL_PENDING), "pending");
    EXPECT_STREQ(bge_goal_state_name(BGE_GOAL_ACTIVE), "active");
    EXPECT_STREQ(bge_goal_state_name(BGE_GOAL_ACHIEVED), "achieved");
    EXPECT_STREQ(bge_goal_state_name(BGE_GOAL_ABANDONED), "abandoned");
}

TEST_F(BGExecutiveBridgeTest, ConflictTypeNames) {
    EXPECT_STREQ(bge_conflict_type_name(BGE_CONFLICT_NONE), "none");
    EXPECT_STREQ(bge_conflict_type_name(BGE_CONFLICT_GOAL_HABIT), "goal_habit");
    EXPECT_STREQ(bge_conflict_type_name(BGE_CONFLICT_GOAL_GOAL), "goal_goal");
    EXPECT_STREQ(bge_conflict_type_name(BGE_CONFLICT_RESPONSE), "response");
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(BGExecutiveBridgeTest, NullBridge) {
    EXPECT_EQ(bge_bridge_reset(nullptr), -1);
    EXPECT_EQ(bge_bridge_update_control(nullptr, 0), -1);
    EXPECT_FALSE(bge_bridge_is_connected(nullptr));
}

TEST_F(BGExecutiveBridgeTest, NullActionValues) {
    int ret = bge_bridge_apply_control(bridge, nullptr, 8);
    EXPECT_EQ(ret, -1);
}

TEST_F(BGExecutiveBridgeTest, InvalidGoalId) {
    bge_goal_state_t state = bge_bridge_get_goal_state(bridge, 999);
    EXPECT_EQ(state, BGE_GOAL_ABANDONED);
}
