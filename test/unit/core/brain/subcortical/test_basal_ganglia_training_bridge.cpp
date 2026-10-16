/**
 * @file test_basal_ganglia_training_bridge.cpp
 * @brief Unit tests for BG-training plasticity bridge
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/brain/subcortical/nimcp_basal_ganglia_training_bridge.h"

class BGTrainingBridgeTest : public ::testing::Test {
protected:
    bgtr_bridge_t* bridge = nullptr;
    basal_ganglia_t* bg = nullptr;

    void SetUp() override {
        bgtr_bridge_config_t config;
        bgtr_bridge_default_config(&config);
        bridge = bgtr_bridge_create(&config, 8);
        ASSERT_NE(bridge, nullptr);

        bg = basal_ganglia_create(nullptr);
    }

    void TearDown() override {
        if (bridge) bgtr_bridge_destroy(bridge);
        if (bg) basal_ganglia_destroy(bg);
    }
};

// ============================================================================
// Lifecycle Tests
// ============================================================================

TEST_F(BGTrainingBridgeTest, CreateDestroy) {
    EXPECT_NE(bridge, nullptr);
}

TEST_F(BGTrainingBridgeTest, CreateWithNullConfig) {
    bgtr_bridge_t* b = bgtr_bridge_create(nullptr, 8);
    ASSERT_NE(b, nullptr);
    bgtr_bridge_destroy(b);
}

TEST_F(BGTrainingBridgeTest, DefaultConfig) {
    bgtr_bridge_config_t config;
    bgtr_bridge_default_config(&config);

    EXPECT_EQ(config.learning_type, BGTR_LEARN_ACTOR_CRITIC);
    EXPECT_FLOAT_EQ(config.learning_rate, BGTR_DEFAULT_LEARNING_RATE);
    EXPECT_FLOAT_EQ(config.trace_decay, BGTR_DEFAULT_TRACE_DECAY);
    EXPECT_TRUE(config.enable_eligibility);
    EXPECT_TRUE(config.enable_habit_learning);
    EXPECT_TRUE(config.enable_d1_d2_asymmetry);
}

TEST_F(BGTrainingBridgeTest, Reset) {
    bgtr_bridge_record_action(bridge, 0, 0, 1000);
    bgtr_bridge_record_action(bridge, 1, 0, 1100);

    int ret = bgtr_bridge_reset(bridge);
    EXPECT_EQ(ret, 0);

    EXPECT_EQ(bgtr_bridge_get_trace_count(bridge), 0u);
}

// ============================================================================
// Connection Tests
// ============================================================================

TEST_F(BGTrainingBridgeTest, ConnectBG) {
    int ret = bgtr_bridge_connect_bg(bridge, bg);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(bgtr_bridge_is_connected(bridge));
}

TEST_F(BGTrainingBridgeTest, NotConnectedInitially) {
    EXPECT_FALSE(bgtr_bridge_is_connected(bridge));
}

// ============================================================================
// Eligibility Trace Tests
// ============================================================================

TEST_F(BGTrainingBridgeTest, RecordAction) {
    int ret = bgtr_bridge_record_action(bridge, 0, 0, 1000);
    EXPECT_EQ(ret, 0);

    EXPECT_EQ(bgtr_bridge_get_trace_count(bridge), 1u);
}

TEST_F(BGTrainingBridgeTest, MultipleTraces) {
    bgtr_bridge_record_action(bridge, 0, 0, 1000);
    bgtr_bridge_record_action(bridge, 1, 0, 1100);
    bgtr_bridge_record_action(bridge, 2, 0, 1200);

    EXPECT_EQ(bgtr_bridge_get_trace_count(bridge), 3u);
}

TEST_F(BGTrainingBridgeTest, GetTrace) {
    bgtr_bridge_record_action(bridge, 3, 0, 1000);

    float trace = bgtr_bridge_get_trace(bridge, 3);
    EXPECT_FLOAT_EQ(trace, 1.0f);  // Newly created trace has value 1.0
}

TEST_F(BGTrainingBridgeTest, TraceDecay) {
    bgtr_bridge_record_action(bridge, 0, 0, 1000);

    float trace_before = bgtr_bridge_get_trace(bridge, 0);

    bgtr_bridge_decay_traces(bridge, 100.0f);  // Decay for 100ms

    float trace_after = bgtr_bridge_get_trace(bridge, 0);

    EXPECT_LT(trace_after, trace_before);
}

TEST_F(BGTrainingBridgeTest, ClearTraces) {
    bgtr_bridge_record_action(bridge, 0, 0, 1000);
    bgtr_bridge_record_action(bridge, 1, 0, 1100);

    bgtr_bridge_clear_traces(bridge);

    EXPECT_EQ(bgtr_bridge_get_trace_count(bridge), 0u);
}

TEST_F(BGTrainingBridgeTest, NoTraceForUnrecordedAction) {
    float trace = bgtr_bridge_get_trace(bridge, 5);
    EXPECT_FLOAT_EQ(trace, 0.0f);
}

// ============================================================================
// Learning Tests
// ============================================================================

TEST_F(BGTrainingBridgeTest, ProcessReward) {
    // Without training context, just test the interface
    int ret = bgtr_bridge_process_reward(bridge, 1.0f, 0.5f);
    EXPECT_EQ(ret, 0);  // Should succeed (no-op without training context)
}

TEST_F(BGTrainingBridgeTest, UpdateWeights) {
    // Record an action first
    bgtr_bridge_record_action(bridge, 0, 0, 1000);

    // Update weights with positive RPE
    int updates = bgtr_bridge_update_weights(bridge, 0.5f);
    // Without training context, no actual updates occur
    EXPECT_GE(updates, 0);
}

TEST_F(BGTrainingBridgeTest, StrengthenHabit) {
    int ret = bgtr_bridge_strengthen_habit(bridge, 0, 0.1f);
    EXPECT_EQ(ret, 0);  // Should succeed even without training context
}

// ============================================================================
// Weight Access Tests
// ============================================================================

TEST_F(BGTrainingBridgeTest, GetD1Weight) {
    // Without training context, returns default
    float weight = bgtr_bridge_get_d1_weight(bridge, 0);
    EXPECT_FLOAT_EQ(weight, 0.5f);
}

TEST_F(BGTrainingBridgeTest, GetD2Weight) {
    float weight = bgtr_bridge_get_d2_weight(bridge, 0);
    EXPECT_FLOAT_EQ(weight, 0.5f);
}

TEST_F(BGTrainingBridgeTest, GetActionValue) {
    float value = bgtr_bridge_get_action_value(bridge, 0);
    // D1 - D2 = 0.5 - 0.5 = 0.0
    EXPECT_FLOAT_EQ(value, 0.0f);
}

TEST_F(BGTrainingBridgeTest, InvalidActionWeight) {
    float weight = bgtr_bridge_get_d1_weight(bridge, 1000);
    EXPECT_LT(weight, 0.0f);  // Error indicator
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_F(BGTrainingBridgeTest, Statistics) {
    bgtr_bridge_record_action(bridge, 0, 0, 1000);
    bgtr_bridge_process_reward(bridge, 1.0f, 0.5f);

    bgtr_bridge_stats_t stats;
    int ret = bgtr_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(stats.traces_created, 1u);
    EXPECT_EQ(stats.reward_events, 1u);
}

TEST_F(BGTrainingBridgeTest, RewardEventTracking) {
    bgtr_bridge_process_reward(bridge, 1.0f, 0.5f);
    bgtr_bridge_process_reward(bridge, 0.5f, 0.5f);
    bgtr_bridge_process_reward(bridge, -0.5f, 0.0f);

    bgtr_bridge_stats_t stats;
    bgtr_bridge_get_stats(bridge, &stats);

    EXPECT_EQ(stats.reward_events, 2u);  // Positive rewards
    EXPECT_EQ(stats.punishment_events, 1u);  // Negative reward
}

TEST_F(BGTrainingBridgeTest, ResetStats) {
    bgtr_bridge_record_action(bridge, 0, 0, 1000);
    bgtr_bridge_process_reward(bridge, 1.0f, 0.5f);

    bgtr_bridge_reset_stats(bridge);

    bgtr_bridge_stats_t stats;
    bgtr_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.traces_created, 0u);
    EXPECT_EQ(stats.reward_events, 0u);
}

// ============================================================================
// Utility Tests
// ============================================================================

TEST_F(BGTrainingBridgeTest, LearningTypeNames) {
    EXPECT_STREQ(bgtr_learning_type_name(BGTR_LEARN_ACTOR_CRITIC), "actor_critic");
    EXPECT_STREQ(bgtr_learning_type_name(BGTR_LEARN_THREE_FACTOR), "three_factor");
    EXPECT_STREQ(bgtr_learning_type_name(BGTR_LEARN_REWARD_MODULATED), "reward_modulated");
    EXPECT_STREQ(bgtr_learning_type_name(BGTR_LEARN_HABIT_FORMATION), "habit_formation");
}

TEST_F(BGTrainingBridgeTest, PathwayTargetNames) {
    EXPECT_STREQ(bgtr_pathway_target_name(BGTR_TARGET_DIRECT), "direct");
    EXPECT_STREQ(bgtr_pathway_target_name(BGTR_TARGET_INDIRECT), "indirect");
    EXPECT_STREQ(bgtr_pathway_target_name(BGTR_TARGET_BOTH), "both");
    EXPECT_STREQ(bgtr_pathway_target_name(BGTR_TARGET_DMS), "dms");
    EXPECT_STREQ(bgtr_pathway_target_name(BGTR_TARGET_DLS), "dls");
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(BGTrainingBridgeTest, NullBridge) {
    EXPECT_EQ(bgtr_bridge_reset(nullptr), -1);
    EXPECT_EQ(bgtr_bridge_record_action(nullptr, 0, 0, 0), -1);
    EXPECT_FALSE(bgtr_bridge_is_connected(nullptr));
}

TEST_F(BGTrainingBridgeTest, InvalidAction) {
    int ret = bgtr_bridge_record_action(bridge, 1000, 0, 1000);
    EXPECT_EQ(ret, -1);
}

TEST_F(BGTrainingBridgeTest, ZeroActions) {
    // Create bridge with 0 actions - should use default
    bgtr_bridge_t* b = bgtr_bridge_create(nullptr, 0);
    ASSERT_NE(b, nullptr);
    bgtr_bridge_destroy(b);
}
