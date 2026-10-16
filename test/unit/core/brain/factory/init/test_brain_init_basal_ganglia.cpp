//=============================================================================
// test_brain_init_basal_ganglia.cpp - Basal Ganglia Brain Init Unit Tests
//=============================================================================
/**
 * @file test_brain_init_basal_ganglia.cpp
 * @brief GoogleTest unit tests for basal ganglia brain factory initialization
 *
 * Tests the BG brain initialization functions:
 * - nimcp_brain_factory_init_basal_ganglia_subsystem
 * - nimcp_brain_bg_default_config
 * - nimcp_brain_bg_step
 * - nimcp_brain_bg_select_action
 * - nimcp_brain_bg_process_reward
 * - Integration callbacks (emotional, goal, arousal)
 * - Query functions (stats, enabled, behavior, motivation)
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-12-30
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/brain/factory/nimcp_brain_factory.h"
#include "core/brain/factory/init/nimcp_brain_init_basal_ganglia.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "nimcp.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BrainInitBasalGangliaTest : public ::testing::Test {
protected:
    brain_t test_brain = nullptr;

    void SetUp() override {
        // Create a minimal test brain
        test_brain = brain_create(
            "bg_test_brain",
            BRAIN_SIZE_TINY,
            BRAIN_TASK_CLASSIFICATION,
            10,
            4  // 4 outputs for action selection
        );
        ASSERT_NE(test_brain, nullptr);
    }

    void TearDown() override {
        if (test_brain) {
            brain_destroy(test_brain);
            test_brain = nullptr;
        }
    }
};

//=============================================================================
// Initialization Tests
//=============================================================================

TEST_F(BrainInitBasalGangliaTest, Init_NullBrain) {
    bool result = nimcp_brain_factory_init_basal_ganglia_subsystem(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(BrainInitBasalGangliaTest, Init_Success) {
    // First ensure BG is not initialized
    struct brain_struct* b = (struct brain_struct*)test_brain;
    b->basal_ganglia = nullptr;
    b->basal_ganglia_enabled = false;

    bool result = nimcp_brain_factory_init_basal_ganglia_subsystem(test_brain);
    EXPECT_TRUE(result);
    EXPECT_NE(b->basal_ganglia, nullptr);
    EXPECT_TRUE(b->basal_ganglia_enabled);
}

TEST_F(BrainInitBasalGangliaTest, Init_AlreadyInitialized) {
    struct brain_struct* b = (struct brain_struct*)test_brain;
    b->basal_ganglia = nullptr;

    // First initialization
    nimcp_brain_factory_init_basal_ganglia_subsystem(test_brain);
    void* first_ptr = b->basal_ganglia;

    // Second initialization should be idempotent
    bool result = nimcp_brain_factory_init_basal_ganglia_subsystem(test_brain);
    EXPECT_TRUE(result);
    EXPECT_EQ(b->basal_ganglia, first_ptr);
}

TEST_F(BrainInitBasalGangliaTest, Init_SetsTimestamp) {
    struct brain_struct* b = (struct brain_struct*)test_brain;
    b->basal_ganglia = nullptr;

    nimcp_brain_factory_init_basal_ganglia_subsystem(test_brain);
    EXPECT_EQ(b->last_basal_ganglia_update_us, 0u);
}

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(BrainInitBasalGangliaTest, DefaultConfig_NullConfig) {
    // Should not crash
    nimcp_brain_bg_default_config(test_brain, nullptr);
}

TEST_F(BrainInitBasalGangliaTest, DefaultConfig_NullBrain) {
    bg_enhanced_config_t config;
    // Should not crash, uses defaults
    nimcp_brain_bg_default_config(nullptr, &config);
}

TEST_F(BrainInitBasalGangliaTest, DefaultConfig_SetsNumActions) {
    struct brain_struct* b = (struct brain_struct*)test_brain;
    b->config.num_outputs = 8;

    bg_enhanced_config_t config;
    nimcp_brain_bg_default_config(test_brain, &config);

    EXPECT_EQ(config.core_config.num_actions, 8u);
}

TEST_F(BrainInitBasalGangliaTest, DefaultConfig_EnablesBasicFeatures) {
    bg_enhanced_config_t config;
    nimcp_brain_bg_default_config(test_brain, &config);

    // These should always be enabled
    EXPECT_TRUE(config.features.enable_beta_oscillations);
    EXPECT_TRUE(config.features.enable_multi_neuromod);
    EXPECT_TRUE(config.features.enable_interneurons);
    EXPECT_TRUE(config.features.enable_temporal_credit);
}

//=============================================================================
// Query Function Tests
//=============================================================================

TEST_F(BrainInitBasalGangliaTest, IsEnabled_NullBrain) {
    bool enabled = nimcp_brain_bg_is_enabled(nullptr);
    EXPECT_FALSE(enabled);
}

TEST_F(BrainInitBasalGangliaTest, IsEnabled_NotInitialized) {
    struct brain_struct* b = (struct brain_struct*)test_brain;
    b->basal_ganglia = nullptr;
    b->basal_ganglia_enabled = false;

    bool enabled = nimcp_brain_bg_is_enabled(test_brain);
    EXPECT_FALSE(enabled);
}

TEST_F(BrainInitBasalGangliaTest, IsEnabled_AfterInit) {
    struct brain_struct* b = (struct brain_struct*)test_brain;
    b->basal_ganglia = nullptr;

    nimcp_brain_factory_init_basal_ganglia_subsystem(test_brain);

    bool enabled = nimcp_brain_bg_is_enabled(test_brain);
    EXPECT_TRUE(enabled);
}

TEST_F(BrainInitBasalGangliaTest, GetMotivation_NullBrain) {
    float motivation = nimcp_brain_bg_get_motivation(nullptr);
    EXPECT_NEAR(motivation, 0.0f, 0.01f);
}

TEST_F(BrainInitBasalGangliaTest, GetMotivation_NotInitialized) {
    struct brain_struct* b = (struct brain_struct*)test_brain;
    b->basal_ganglia = nullptr;
    b->basal_ganglia_enabled = false;

    float motivation = nimcp_brain_bg_get_motivation(test_brain);
    EXPECT_NEAR(motivation, 0.5f, 0.01f);  // Default neutral
}

TEST_F(BrainInitBasalGangliaTest, GetBehaviorType_NullBrain) {
    bgod_behavior_type_t type = nimcp_brain_bg_get_behavior_type(nullptr);
    EXPECT_EQ(type, BGOD_BEHAVIOR_UNKNOWN);
}

TEST_F(BrainInitBasalGangliaTest, GetBehaviorType_NotInitialized) {
    struct brain_struct* b = (struct brain_struct*)test_brain;
    b->basal_ganglia = nullptr;
    b->basal_ganglia_enabled = false;

    bgod_behavior_type_t type = nimcp_brain_bg_get_behavior_type(test_brain);
    EXPECT_EQ(type, BGOD_BEHAVIOR_UNKNOWN);
}

TEST_F(BrainInitBasalGangliaTest, GetStats_NullBrain) {
    bg_enhanced_stats_t stats;
    int result = nimcp_brain_bg_get_stats(nullptr, &stats);
    EXPECT_LT(result, 0);
}

TEST_F(BrainInitBasalGangliaTest, GetStats_NullStats) {
    int result = nimcp_brain_bg_get_stats(test_brain, nullptr);
    EXPECT_LT(result, 0);
}

TEST_F(BrainInitBasalGangliaTest, GetStats_NotInitialized) {
    struct brain_struct* b = (struct brain_struct*)test_brain;
    b->basal_ganglia = nullptr;
    b->basal_ganglia_enabled = false;

    bg_enhanced_stats_t stats;
    int result = nimcp_brain_bg_get_stats(test_brain, &stats);
    EXPECT_LT(result, 0);
}

//=============================================================================
// Processing Tests
//=============================================================================

TEST_F(BrainInitBasalGangliaTest, Step_NullBrain) {
    int result = nimcp_brain_bg_step(nullptr, 1.0f);
    EXPECT_LT(result, 0);
}

TEST_F(BrainInitBasalGangliaTest, Step_NotInitialized) {
    struct brain_struct* b = (struct brain_struct*)test_brain;
    b->basal_ganglia = nullptr;
    b->basal_ganglia_enabled = false;

    int result = nimcp_brain_bg_step(test_brain, 1.0f);
    EXPECT_EQ(result, 0);  // Silently skips
}

TEST_F(BrainInitBasalGangliaTest, Step_AfterInit) {
    struct brain_struct* b = (struct brain_struct*)test_brain;
    b->basal_ganglia = nullptr;

    nimcp_brain_factory_init_basal_ganglia_subsystem(test_brain);

    int result = nimcp_brain_bg_step(test_brain, 1.0f);
    EXPECT_EQ(result, 0);
}

TEST_F(BrainInitBasalGangliaTest, SelectAction_NullBrain) {
    float input[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    uint32_t action;
    int result = nimcp_brain_bg_select_action(nullptr, input, &action);
    EXPECT_LT(result, 0);
}

TEST_F(BrainInitBasalGangliaTest, SelectAction_NullInput) {
    uint32_t action;
    int result = nimcp_brain_bg_select_action(test_brain, nullptr, &action);
    EXPECT_LT(result, 0);
}

TEST_F(BrainInitBasalGangliaTest, SelectAction_NullOutput) {
    float input[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    int result = nimcp_brain_bg_select_action(test_brain, input, nullptr);
    EXPECT_LT(result, 0);
}

TEST_F(BrainInitBasalGangliaTest, SelectAction_FallbackWithoutBG) {
    struct brain_struct* b = (struct brain_struct*)test_brain;
    b->basal_ganglia = nullptr;
    b->basal_ganglia_enabled = false;
    b->config.num_outputs = 4;

    // Should fall back to max activation
    float input[4] = {0.2f, 0.8f, 0.3f, 0.1f};
    uint32_t action;
    int result = nimcp_brain_bg_select_action(test_brain, input, &action);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(action, 1u);  // Index of max value
}

TEST_F(BrainInitBasalGangliaTest, SelectAction_AfterInit) {
    struct brain_struct* b = (struct brain_struct*)test_brain;
    b->basal_ganglia = nullptr;

    nimcp_brain_factory_init_basal_ganglia_subsystem(test_brain);

    float input[4] = {0.2f, 0.8f, 0.3f, 0.1f};
    uint32_t action;
    int result = nimcp_brain_bg_select_action(test_brain, input, &action);
    EXPECT_EQ(result, 0);
    // Enhanced BG should still prefer highest input
    EXPECT_EQ(action, 1u);
}

TEST_F(BrainInitBasalGangliaTest, ProcessReward_NullBrain) {
    int result = nimcp_brain_bg_process_reward(nullptr, 1.0f, 0.5f);
    EXPECT_LT(result, 0);
}

TEST_F(BrainInitBasalGangliaTest, ProcessReward_NotInitialized) {
    struct brain_struct* b = (struct brain_struct*)test_brain;
    b->basal_ganglia = nullptr;
    b->basal_ganglia_enabled = false;

    int result = nimcp_brain_bg_process_reward(test_brain, 1.0f, 0.5f);
    EXPECT_EQ(result, 0);  // Silently skips
}

TEST_F(BrainInitBasalGangliaTest, ProcessReward_AfterInit) {
    struct brain_struct* b = (struct brain_struct*)test_brain;
    b->basal_ganglia = nullptr;

    nimcp_brain_factory_init_basal_ganglia_subsystem(test_brain);

    // Positive surprise
    int result = nimcp_brain_bg_process_reward(test_brain, 1.0f, 0.0f);
    EXPECT_EQ(result, 0);

    // Negative surprise
    result = nimcp_brain_bg_process_reward(test_brain, 0.0f, 1.0f);
    EXPECT_EQ(result, 0);
}

//=============================================================================
// Integration Callback Tests
//=============================================================================

TEST_F(BrainInitBasalGangliaTest, OnEmotionalSignal_NullBrain) {
    // Should not crash
    nimcp_brain_bg_on_emotional_signal(nullptr, 0.5f, 0.5f);
}

TEST_F(BrainInitBasalGangliaTest, OnEmotionalSignal_NotInitialized) {
    struct brain_struct* b = (struct brain_struct*)test_brain;
    b->basal_ganglia = nullptr;
    b->basal_ganglia_enabled = false;

    // Should not crash
    nimcp_brain_bg_on_emotional_signal(test_brain, 0.5f, 0.5f);
}

TEST_F(BrainInitBasalGangliaTest, OnEmotionalSignal_AfterInit) {
    struct brain_struct* b = (struct brain_struct*)test_brain;
    b->basal_ganglia = nullptr;

    nimcp_brain_factory_init_basal_ganglia_subsystem(test_brain);

    // Positive valence
    nimcp_brain_bg_on_emotional_signal(test_brain, 0.8f, 0.5f);

    // Negative valence
    nimcp_brain_bg_on_emotional_signal(test_brain, -0.8f, 0.5f);
}

TEST_F(BrainInitBasalGangliaTest, OnGoalChange_NullBrain) {
    // Should not crash
    nimcp_brain_bg_on_goal_change(nullptr, 1, true);
}

TEST_F(BrainInitBasalGangliaTest, OnGoalChange_NotInitialized) {
    struct brain_struct* b = (struct brain_struct*)test_brain;
    b->basal_ganglia = nullptr;
    b->basal_ganglia_enabled = false;

    // Should not crash
    nimcp_brain_bg_on_goal_change(test_brain, 1, true);
}

TEST_F(BrainInitBasalGangliaTest, OnGoalChange_AfterInit) {
    struct brain_struct* b = (struct brain_struct*)test_brain;
    b->basal_ganglia = nullptr;

    nimcp_brain_factory_init_basal_ganglia_subsystem(test_brain);

    // Activate goal
    nimcp_brain_bg_on_goal_change(test_brain, 1, true);

    // Deactivate goal
    nimcp_brain_bg_on_goal_change(test_brain, 1, false);
}

TEST_F(BrainInitBasalGangliaTest, OnArousalChange_NullBrain) {
    // Should not crash
    nimcp_brain_bg_on_arousal_change(nullptr, 0.5f);
}

TEST_F(BrainInitBasalGangliaTest, OnArousalChange_NotInitialized) {
    struct brain_struct* b = (struct brain_struct*)test_brain;
    b->basal_ganglia = nullptr;
    b->basal_ganglia_enabled = false;

    // Should not crash
    nimcp_brain_bg_on_arousal_change(test_brain, 0.5f);
}

TEST_F(BrainInitBasalGangliaTest, OnArousalChange_AfterInit) {
    struct brain_struct* b = (struct brain_struct*)test_brain;
    b->basal_ganglia = nullptr;

    nimcp_brain_factory_init_basal_ganglia_subsystem(test_brain);

    // High arousal
    nimcp_brain_bg_on_arousal_change(test_brain, 0.9f);

    // Low arousal (fatigue)
    nimcp_brain_bg_on_arousal_change(test_brain, 0.1f);
}

//=============================================================================
// Destroy Tests
//=============================================================================

TEST_F(BrainInitBasalGangliaTest, Destroy_NullBrain) {
    // Should not crash
    nimcp_brain_bg_destroy(nullptr);
}

TEST_F(BrainInitBasalGangliaTest, Destroy_NotInitialized) {
    struct brain_struct* b = (struct brain_struct*)test_brain;
    b->basal_ganglia = nullptr;
    b->basal_ganglia_enabled = false;

    // Should not crash
    nimcp_brain_bg_destroy(test_brain);
}

TEST_F(BrainInitBasalGangliaTest, Destroy_AfterInit) {
    struct brain_struct* b = (struct brain_struct*)test_brain;
    b->basal_ganglia = nullptr;

    nimcp_brain_factory_init_basal_ganglia_subsystem(test_brain);
    EXPECT_NE(b->basal_ganglia, nullptr);
    EXPECT_TRUE(b->basal_ganglia_enabled);

    nimcp_brain_bg_destroy(test_brain);
    EXPECT_EQ(b->basal_ganglia, nullptr);
    EXPECT_FALSE(b->basal_ganglia_enabled);
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(BrainInitBasalGangliaTest, FullLifecycle) {
    struct brain_struct* b = (struct brain_struct*)test_brain;
    b->basal_ganglia = nullptr;

    // Initialize
    ASSERT_TRUE(nimcp_brain_factory_init_basal_ganglia_subsystem(test_brain));
    EXPECT_TRUE(nimcp_brain_bg_is_enabled(test_brain));

    // Step
    EXPECT_EQ(nimcp_brain_bg_step(test_brain, 1.0f), 0);

    // Action selection
    float input[4] = {0.2f, 0.6f, 0.8f, 0.1f};
    uint32_t action;
    EXPECT_EQ(nimcp_brain_bg_select_action(test_brain, input, &action), 0);
    EXPECT_EQ(action, 2u);  // Highest activation

    // Reward processing
    EXPECT_EQ(nimcp_brain_bg_process_reward(test_brain, 1.0f, 0.5f), 0);

    // Callbacks
    nimcp_brain_bg_on_emotional_signal(test_brain, 0.5f, 0.7f);
    nimcp_brain_bg_on_goal_change(test_brain, 0, true);
    nimcp_brain_bg_on_arousal_change(test_brain, 0.6f);

    // Query - motivation may be 0.0 if NAC outputs haven't accumulated yet
    float motivation = nimcp_brain_bg_get_motivation(test_brain);
    EXPECT_GE(motivation, 0.0f);
    EXPECT_LE(motivation, 1.0f);

    // Destroy
    nimcp_brain_bg_destroy(test_brain);
    EXPECT_FALSE(nimcp_brain_bg_is_enabled(test_brain));
}
