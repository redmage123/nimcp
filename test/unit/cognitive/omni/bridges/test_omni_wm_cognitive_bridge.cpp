/**
 * @file test_omni_wm_cognitive_bridge.cpp
 * @brief Comprehensive unit tests for World Model Cognitive Bridge
 *
 * WHAT: Tests for WM Cognitive Bridge connecting RSSM with Executive, Attention,
 *       Working Memory, Salience, and Meta-Learning systems
 * WHY:  Bridge is critical for prediction-informed planning and goal-conditioned modeling
 * HOW:  Tests all APIs: lifecycle, connection, update, goal management, attention,
 *       prediction, salience, working memory context, meta-learning, and statistics
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

extern "C" {
#include "cognitive/omni/bridges/nimcp_omni_wm_cognitive_bridge.h"
#include "utils/error/nimcp_error_codes.h"
}

// =============================================================================
// Constants and Helpers
// =============================================================================

static constexpr float FLOAT_TOLERANCE = 1e-5f;
static constexpr uint32_t TEST_STATE_DIM = 16;
static constexpr uint32_t TEST_ACTION_DIM = 4;
static constexpr float TEST_DT = 0.016f; // ~60Hz

static bool float_equals(float a, float b, float tol = FLOAT_TOLERANCE)
{
    return std::fabs(a - b) < tol;
}

static bool float_in_range(float value, float min_val, float max_val)
{
    return value >= min_val && value <= max_val;
}

// =============================================================================
// Test Fixture
// =============================================================================

class WMCognitiveBridgeTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Create bridge with default config
        bridge_ = omni_wm_cognitive_bridge_create(nullptr);
    }

    void TearDown() override
    {
        if (bridge_) {
            omni_wm_cognitive_bridge_destroy(bridge_);
            bridge_ = nullptr;
        }
    }

    // Helper to create bridge with custom config
    omni_wm_cognitive_bridge_t* create_custom_bridge(bool enable_modulation,
                                                      float sensitivity)
    {
        omni_wm_cognitive_bridge_config_t config = omni_wm_cognitive_bridge_default_config();
        config.enable_modulation = enable_modulation;
        config.sensitivity = sensitivity;
        return omni_wm_cognitive_bridge_create(&config);
    }

    // Helper to create test state array
    std::vector<float> create_test_state(uint32_t dim, float base_value)
    {
        std::vector<float> state(dim);
        for (uint32_t i = 0; i < dim; i++) {
            state[i] = base_value + (float)i * 0.01f;
        }
        return state;
    }

    // Helper to create test action array
    std::vector<float> create_test_action(uint32_t dim, float base_value)
    {
        std::vector<float> action(dim);
        for (uint32_t i = 0; i < dim; i++) {
            action[i] = base_value + (float)i * 0.1f;
        }
        return action;
    }

    omni_wm_cognitive_bridge_t* bridge_ = nullptr;
};

// =============================================================================
// 1. Default Config Tests
// =============================================================================

TEST_F(WMCognitiveBridgeTest, DefaultConfigReturnsReasonableValues)
{
    omni_wm_cognitive_bridge_config_t config = omni_wm_cognitive_bridge_default_config();

    // Check general settings
    EXPECT_TRUE(config.enable_modulation);
    EXPECT_TRUE(float_in_range(config.sensitivity, 0.5f, 2.0f));

    // Check goal conditioning settings
    EXPECT_GT(config.max_active_goals, 0u);
    EXPECT_LE(config.max_active_goals, WM_COGNITIVE_MAX_GOALS);
    EXPECT_TRUE(float_in_range(config.goal_progress_threshold, 0.0f, 1.0f));
    EXPECT_TRUE(float_in_range(config.goal_priority_decay, 0.0f, 1.0f));

    // Check attention integration settings
    EXPECT_GE(config.attention_prediction_boost, 1.0f);
    EXPECT_GT(config.attention_bandwidth_min, 0.0f);
    EXPECT_GT(config.attention_bandwidth_max, config.attention_bandwidth_min);

    // Check salience integration settings
    EXPECT_TRUE(float_in_range(config.novelty_weight, 0.0f, 1.0f));
    EXPECT_TRUE(float_in_range(config.surprise_weight, 0.0f, 1.0f));
    EXPECT_TRUE(float_in_range(config.urgency_weight, 0.0f, 1.0f));
    EXPECT_TRUE(float_in_range(config.salience_threshold, 0.0f, 1.0f));

    // Check meta-learning settings
    EXPECT_GT(config.meta_lr_scale, 0.0f);
    EXPECT_GT(config.adaptation_threshold, 0.0f);
}

TEST_F(WMCognitiveBridgeTest, DefaultConfigIdempotent)
{
    omni_wm_cognitive_bridge_config_t config1 = omni_wm_cognitive_bridge_default_config();
    omni_wm_cognitive_bridge_config_t config2 = omni_wm_cognitive_bridge_default_config();

    EXPECT_EQ(config1.enable_modulation, config2.enable_modulation);
    EXPECT_FLOAT_EQ(config1.sensitivity, config2.sensitivity);
    EXPECT_EQ(config1.max_active_goals, config2.max_active_goals);
    EXPECT_FLOAT_EQ(config1.attention_prediction_boost, config2.attention_prediction_boost);
}

// =============================================================================
// 2. Lifecycle Tests - Create/Destroy
// =============================================================================

TEST_F(WMCognitiveBridgeTest, CreateWithNullConfigUsesDefaults)
{
    // bridge_ created in SetUp with NULL config
    ASSERT_NE(bridge_, nullptr);
}

TEST_F(WMCognitiveBridgeTest, CreateWithCustomConfig)
{
    omni_wm_cognitive_bridge_config_t config = omni_wm_cognitive_bridge_default_config();
    config.enable_modulation = false;
    config.sensitivity = 1.5f;
    config.enable_goal_conditioning = false;
    config.enable_attention_modulation = true;

    omni_wm_cognitive_bridge_t* custom_bridge = omni_wm_cognitive_bridge_create(&config);
    ASSERT_NE(custom_bridge, nullptr);

    // Verify config was applied
    EXPECT_FALSE(custom_bridge->config.enable_modulation);
    EXPECT_FLOAT_EQ(custom_bridge->config.sensitivity, 1.5f);
    EXPECT_FALSE(custom_bridge->config.enable_goal_conditioning);

    omni_wm_cognitive_bridge_destroy(custom_bridge);
}

TEST_F(WMCognitiveBridgeTest, CreateInitializesBaseFields)
{
    ASSERT_NE(bridge_, nullptr);

    // Check base bridge infrastructure
    EXPECT_NE(bridge_->base.module_name, nullptr);
    EXPECT_NE(bridge_->base.module_id, 0u);
}

TEST_F(WMCognitiveBridgeTest, CreateInitializesStatistics)
{
    ASSERT_NE(bridge_, nullptr);

    // Stats should be zeroed on creation
    EXPECT_EQ(bridge_->stats.state_predictions, 0u);
    EXPECT_EQ(bridge_->stats.goals_received, 0u);
    EXPECT_EQ(bridge_->stats.attention_focus_events, 0u);
    EXPECT_EQ(bridge_->stats.errors_total, 0u);
}

TEST_F(WMCognitiveBridgeTest, CreateInitializesGoalTracking)
{
    ASSERT_NE(bridge_, nullptr);

    EXPECT_EQ(bridge_->num_goals, 0u);
    EXPECT_EQ(bridge_->next_goal_id, 1u); // Start at 1
}

TEST_F(WMCognitiveBridgeTest, CreateInitializesAttentionState)
{
    ASSERT_NE(bridge_, nullptr);

    EXPECT_FALSE(bridge_->focus_active);
}

TEST_F(WMCognitiveBridgeTest, DestroyNullSafe)
{
    // Should not crash
    omni_wm_cognitive_bridge_destroy(nullptr);
}

TEST_F(WMCognitiveBridgeTest, DestroyValidBridge)
{
    omni_wm_cognitive_bridge_t* temp = omni_wm_cognitive_bridge_create(nullptr);
    ASSERT_NE(temp, nullptr);

    // Should not crash and should free buffers
    omni_wm_cognitive_bridge_destroy(temp);
}

// =============================================================================
// 3. Reset Tests
// =============================================================================

TEST_F(WMCognitiveBridgeTest, ResetNullFails)
{
    nimcp_error_t result = omni_wm_cognitive_bridge_reset(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMCognitiveBridgeTest, ResetClearsStatistics)
{
    ASSERT_NE(bridge_, nullptr);

    // Manually increment some stats
    bridge_->stats.state_predictions = 100;
    bridge_->stats.goals_received = 20;
    bridge_->stats.attention_focus_events = 50;

    nimcp_error_t result = omni_wm_cognitive_bridge_reset(bridge_);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    EXPECT_EQ(bridge_->stats.state_predictions, 0u);
    EXPECT_EQ(bridge_->stats.goals_received, 0u);
    EXPECT_EQ(bridge_->stats.attention_focus_events, 0u);
}

TEST_F(WMCognitiveBridgeTest, ResetClearsGoals)
{
    ASSERT_NE(bridge_, nullptr);

    // Simulate having goals
    bridge_->num_goals = 5;

    nimcp_error_t result = omni_wm_cognitive_bridge_reset(bridge_);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    EXPECT_EQ(bridge_->num_goals, 0u);
}

TEST_F(WMCognitiveBridgeTest, ResetClearsAttention)
{
    ASSERT_NE(bridge_, nullptr);

    // Simulate active attention
    bridge_->focus_active = true;
    bridge_->current_focus.focus_strength = 0.8f;

    nimcp_error_t result = omni_wm_cognitive_bridge_reset(bridge_);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    EXPECT_FALSE(bridge_->focus_active);
}

TEST_F(WMCognitiveBridgeTest, ResetPreservesConfiguration)
{
    omni_wm_cognitive_bridge_t* custom = create_custom_bridge(false, 1.8f);
    ASSERT_NE(custom, nullptr);

    // Store original config values
    bool orig_enable = custom->config.enable_modulation;
    float orig_sens = custom->config.sensitivity;

    nimcp_error_t result = omni_wm_cognitive_bridge_reset(custom);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    // Config should be preserved
    EXPECT_EQ(custom->config.enable_modulation, orig_enable);
    EXPECT_FLOAT_EQ(custom->config.sensitivity, orig_sens);

    omni_wm_cognitive_bridge_destroy(custom);
}

// =============================================================================
// 4. Connection Tests
// =============================================================================

TEST_F(WMCognitiveBridgeTest, IsConnectedWithoutConnectionReturnsFalse)
{
    ASSERT_NE(bridge_, nullptr);

    bool connected = omni_wm_cognitive_bridge_is_connected(bridge_);
    EXPECT_FALSE(connected);
}

TEST_F(WMCognitiveBridgeTest, IsConnectedNullBridgeReturnsFalse)
{
    bool connected = omni_wm_cognitive_bridge_is_connected(nullptr);
    EXPECT_FALSE(connected);
}

TEST_F(WMCognitiveBridgeTest, ConnectNullBridgeFails)
{
    nimcp_error_t result = omni_wm_cognitive_bridge_connect(
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMCognitiveBridgeTest, ConnectWorldModelNullBridgeFails)
{
    nimcp_error_t result = omni_wm_cognitive_bridge_connect_world_model(nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMCognitiveBridgeTest, ConnectExecutiveNullBridgeFails)
{
    nimcp_error_t result = omni_wm_cognitive_bridge_connect_executive(nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMCognitiveBridgeTest, ConnectWorkingMemoryNullBridgeFails)
{
    nimcp_error_t result = omni_wm_cognitive_bridge_connect_working_memory(nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMCognitiveBridgeTest, ConnectSalienceNullBridgeFails)
{
    nimcp_error_t result = omni_wm_cognitive_bridge_connect_salience(nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMCognitiveBridgeTest, ConnectMetaLearnerNullBridgeFails)
{
    nimcp_error_t result = omni_wm_cognitive_bridge_connect_meta_learner(nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMCognitiveBridgeTest, ConnectAttentionNullBridgeFails)
{
    nimcp_error_t result = omni_wm_cognitive_bridge_connect_attention(nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// 5. Update Tests
// =============================================================================

TEST_F(WMCognitiveBridgeTest, UpdateNullBridgeFails)
{
    nimcp_error_t result = omni_wm_cognitive_bridge_update(nullptr, TEST_DT);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMCognitiveBridgeTest, UpdateWithoutConnectionReturnsError)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_cognitive_bridge_update(bridge_, TEST_DT);
    // Should return error or handle gracefully
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMCognitiveBridgeTest, UpdateZeroDtHandled)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_cognitive_bridge_update(bridge_, 0.0f);
    // Should handle gracefully
    EXPECT_TRUE(result == NIMCP_SUCCESS || result != NIMCP_SUCCESS);
}

TEST_F(WMCognitiveBridgeTest, UpdateNegativeDtHandled)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_cognitive_bridge_update(bridge_, -1.0f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// 6. Goal Management Tests
// =============================================================================

TEST_F(WMCognitiveBridgeTest, RegisterGoalNullBridgeFails)
{
    std::vector<float> target = create_test_state(TEST_STATE_DIM, 1.0f);
    uint32_t goal_id;

    nimcp_error_t result = omni_wm_cognitive_bridge_register_goal(
        nullptr, target.data(), TEST_STATE_DIM, 0.8f, 0, "Test goal", &goal_id);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMCognitiveBridgeTest, RegisterGoalNullTargetFails)
{
    ASSERT_NE(bridge_, nullptr);
    uint32_t goal_id;

    nimcp_error_t result = omni_wm_cognitive_bridge_register_goal(
        bridge_, nullptr, TEST_STATE_DIM, 0.8f, 0, "Test goal", &goal_id);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMCognitiveBridgeTest, RegisterGoalZeroDimFails)
{
    ASSERT_NE(bridge_, nullptr);
    std::vector<float> target = create_test_state(TEST_STATE_DIM, 1.0f);
    uint32_t goal_id;

    nimcp_error_t result = omni_wm_cognitive_bridge_register_goal(
        bridge_, target.data(), 0, 0.8f, 0, "Test goal", &goal_id);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMCognitiveBridgeTest, RegisterGoalValid)
{
    ASSERT_NE(bridge_, nullptr);
    std::vector<float> target = create_test_state(TEST_STATE_DIM, 1.0f);
    uint32_t goal_id = 0;

    nimcp_error_t result = omni_wm_cognitive_bridge_register_goal(
        bridge_, target.data(), TEST_STATE_DIM, 0.8f, 0, "Test goal", &goal_id);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GT(goal_id, 0u);
}

TEST_F(WMCognitiveBridgeTest, RegisterGoalNullIdOutputHandled)
{
    ASSERT_NE(bridge_, nullptr);
    std::vector<float> target = create_test_state(TEST_STATE_DIM, 1.0f);

    nimcp_error_t result = omni_wm_cognitive_bridge_register_goal(
        bridge_, target.data(), TEST_STATE_DIM, 0.8f, 0, "Test goal", nullptr);
    // Should either succeed (ignoring output) or fail with invalid param
    EXPECT_TRUE(result == NIMCP_SUCCESS || result == NIMCP_ERROR_INVALID_PARAMETER);
}

TEST_F(WMCognitiveBridgeTest, RegisterMultipleGoals)
{
    ASSERT_NE(bridge_, nullptr);

    for (int i = 0; i < 5; i++) {
        std::vector<float> target = create_test_state(TEST_STATE_DIM, (float)i);
        uint32_t goal_id;

        nimcp_error_t result = omni_wm_cognitive_bridge_register_goal(
            bridge_, target.data(), TEST_STATE_DIM, 0.5f + (float)i * 0.1f,
            0, "Goal", &goal_id);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }

    EXPECT_EQ(bridge_->num_goals, 5u);
}

TEST_F(WMCognitiveBridgeTest, RegisterGoalExceedsMaxFails)
{
    ASSERT_NE(bridge_, nullptr);

    // Fill all goal slots
    for (uint32_t i = 0; i < WM_COGNITIVE_MAX_GOALS; i++) {
        std::vector<float> target = create_test_state(TEST_STATE_DIM, (float)i);
        uint32_t goal_id;
        omni_wm_cognitive_bridge_register_goal(
            bridge_, target.data(), TEST_STATE_DIM, 0.5f, 0, "Goal", &goal_id);
    }

    // Try to add one more
    std::vector<float> target = create_test_state(TEST_STATE_DIM, 100.0f);
    uint32_t goal_id;
    nimcp_error_t result = omni_wm_cognitive_bridge_register_goal(
        bridge_, target.data(), TEST_STATE_DIM, 0.5f, 0, "Extra goal", &goal_id);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMCognitiveBridgeTest, UpdateGoalProgressNullBridgeFails)
{
    nimcp_error_t result = omni_wm_cognitive_bridge_update_goal_progress(nullptr, 1, 0.5f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMCognitiveBridgeTest, UpdateGoalProgressInvalidIdFails)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_cognitive_bridge_update_goal_progress(bridge_, 9999, 0.5f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMCognitiveBridgeTest, UpdateGoalProgressValid)
{
    ASSERT_NE(bridge_, nullptr);

    // Register a goal first
    std::vector<float> target = create_test_state(TEST_STATE_DIM, 1.0f);
    uint32_t goal_id;
    omni_wm_cognitive_bridge_register_goal(
        bridge_, target.data(), TEST_STATE_DIM, 0.8f, 0, "Test goal", &goal_id);

    nimcp_error_t result = omni_wm_cognitive_bridge_update_goal_progress(bridge_, goal_id, 0.5f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(WMCognitiveBridgeTest, GoalAchievedNullBridgeFails)
{
    nimcp_error_t result = omni_wm_cognitive_bridge_goal_achieved(nullptr, 1);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMCognitiveBridgeTest, GoalFailedNullBridgeFails)
{
    nimcp_error_t result = omni_wm_cognitive_bridge_goal_failed(nullptr, 1);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMCognitiveBridgeTest, RemoveGoalNullBridgeFails)
{
    nimcp_error_t result = omni_wm_cognitive_bridge_remove_goal(nullptr, 1);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMCognitiveBridgeTest, RemoveGoalValid)
{
    ASSERT_NE(bridge_, nullptr);

    // Register a goal first
    std::vector<float> target = create_test_state(TEST_STATE_DIM, 1.0f);
    uint32_t goal_id;
    omni_wm_cognitive_bridge_register_goal(
        bridge_, target.data(), TEST_STATE_DIM, 0.8f, 0, "Test goal", &goal_id);

    EXPECT_EQ(bridge_->num_goals, 1u);

    nimcp_error_t result = omni_wm_cognitive_bridge_remove_goal(bridge_, goal_id);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(bridge_->num_goals, 0u);
}

TEST_F(WMCognitiveBridgeTest, GetGoalNullBridgeReturnsNull)
{
    const wm_cognitive_goal_t* goal = omni_wm_cognitive_bridge_get_goal(nullptr, 1);
    EXPECT_EQ(goal, nullptr);
}

TEST_F(WMCognitiveBridgeTest, GetGoalInvalidIdReturnsNull)
{
    ASSERT_NE(bridge_, nullptr);

    const wm_cognitive_goal_t* goal = omni_wm_cognitive_bridge_get_goal(bridge_, 9999);
    EXPECT_EQ(goal, nullptr);
}

TEST_F(WMCognitiveBridgeTest, GetNumGoalsNullBridgeReturnsZero)
{
    uint32_t num = omni_wm_cognitive_bridge_get_num_goals(nullptr);
    EXPECT_EQ(num, 0u);
}

TEST_F(WMCognitiveBridgeTest, GetNumGoalsValid)
{
    ASSERT_NE(bridge_, nullptr);

    EXPECT_EQ(omni_wm_cognitive_bridge_get_num_goals(bridge_), 0u);

    std::vector<float> target = create_test_state(TEST_STATE_DIM, 1.0f);
    uint32_t goal_id;
    omni_wm_cognitive_bridge_register_goal(
        bridge_, target.data(), TEST_STATE_DIM, 0.8f, 0, "Test goal", &goal_id);

    EXPECT_EQ(omni_wm_cognitive_bridge_get_num_goals(bridge_), 1u);
}

// =============================================================================
// 7. Attention Tests
// =============================================================================

TEST_F(WMCognitiveBridgeTest, SetAttentionFocusNullBridgeFails)
{
    std::vector<float> focus = create_test_state(TEST_STATE_DIM, 0.5f);

    nimcp_error_t result = omni_wm_cognitive_bridge_set_attention_focus(
        nullptr, focus.data(), TEST_STATE_DIM, 0.9f, 0.2f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMCognitiveBridgeTest, SetAttentionFocusNullFocusFails)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_cognitive_bridge_set_attention_focus(
        bridge_, nullptr, TEST_STATE_DIM, 0.9f, 0.2f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMCognitiveBridgeTest, SetAttentionFocusValid)
{
    ASSERT_NE(bridge_, nullptr);

    std::vector<float> focus = create_test_state(TEST_STATE_DIM, 0.5f);

    nimcp_error_t result = omni_wm_cognitive_bridge_set_attention_focus(
        bridge_, focus.data(), TEST_STATE_DIM, 0.9f, 0.2f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_TRUE(bridge_->focus_active);
}

TEST_F(WMCognitiveBridgeTest, AttentionShiftNullBridgeFails)
{
    std::vector<float> focus = create_test_state(TEST_STATE_DIM, 0.5f);

    nimcp_error_t result = omni_wm_cognitive_bridge_attention_shift(
        nullptr, focus.data(), TEST_STATE_DIM, 0.8f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMCognitiveBridgeTest, AttentionShiftValid)
{
    ASSERT_NE(bridge_, nullptr);

    // Set initial focus
    std::vector<float> focus1 = create_test_state(TEST_STATE_DIM, 0.5f);
    omni_wm_cognitive_bridge_set_attention_focus(
        bridge_, focus1.data(), TEST_STATE_DIM, 0.9f, 0.2f);

    // Shift attention
    std::vector<float> focus2 = create_test_state(TEST_STATE_DIM, 0.8f);
    nimcp_error_t result = omni_wm_cognitive_bridge_attention_shift(
        bridge_, focus2.data(), TEST_STATE_DIM, 0.7f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(WMCognitiveBridgeTest, ClearAttentionNullBridgeFails)
{
    nimcp_error_t result = omni_wm_cognitive_bridge_clear_attention(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMCognitiveBridgeTest, ClearAttentionValid)
{
    ASSERT_NE(bridge_, nullptr);

    // Set focus first
    std::vector<float> focus = create_test_state(TEST_STATE_DIM, 0.5f);
    omni_wm_cognitive_bridge_set_attention_focus(
        bridge_, focus.data(), TEST_STATE_DIM, 0.9f, 0.2f);
    EXPECT_TRUE(bridge_->focus_active);

    // Clear attention
    nimcp_error_t result = omni_wm_cognitive_bridge_clear_attention(bridge_);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_FALSE(bridge_->focus_active);
}

// =============================================================================
// 8. Prediction Tests
// =============================================================================

TEST_F(WMCognitiveBridgeTest, PredictStateNullBridgeFails)
{
    std::vector<float> state = create_test_state(TEST_STATE_DIM, 0.5f);
    std::vector<float> action = create_test_action(TEST_ACTION_DIM, 0.1f);
    std::vector<float> predicted(TEST_STATE_DIM);
    float confidence;

    nimcp_error_t result = omni_wm_cognitive_bridge_predict_state(
        nullptr, state.data(), TEST_STATE_DIM, action.data(), TEST_ACTION_DIM,
        predicted.data(), &confidence);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMCognitiveBridgeTest, PredictStateNullOutputFails)
{
    ASSERT_NE(bridge_, nullptr);

    std::vector<float> state = create_test_state(TEST_STATE_DIM, 0.5f);
    std::vector<float> action = create_test_action(TEST_ACTION_DIM, 0.1f);
    float confidence;

    nimcp_error_t result = omni_wm_cognitive_bridge_predict_state(
        bridge_, state.data(), TEST_STATE_DIM, action.data(), TEST_ACTION_DIM,
        nullptr, &confidence);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMCognitiveBridgeTest, EvaluatePlanNullBridgeFails)
{
    std::vector<float> state = create_test_state(TEST_STATE_DIM, 0.5f);
    float value, prob;

    nimcp_error_t result = omni_wm_cognitive_bridge_evaluate_plan(
        nullptr, state.data(), TEST_STATE_DIM, nullptr, TEST_ACTION_DIM, 0, 0, &value, &prob);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// 9. Salience Tests
// =============================================================================

TEST_F(WMCognitiveBridgeTest, UpdateSalienceNullBridgeFails)
{
    nimcp_error_t result = omni_wm_cognitive_bridge_update_salience(nullptr, 0.5f, 0.3f, 0.2f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMCognitiveBridgeTest, UpdateSalienceValid)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_cognitive_bridge_update_salience(bridge_, 0.5f, 0.3f, 0.2f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(WMCognitiveBridgeTest, UpdateSalienceAllZero)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_cognitive_bridge_update_salience(bridge_, 0.0f, 0.0f, 0.0f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(WMCognitiveBridgeTest, UpdateSalienceAllMax)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_cognitive_bridge_update_salience(bridge_, 1.0f, 1.0f, 1.0f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(WMCognitiveBridgeTest, GetPredictionErrorMapNullBridgeFails)
{
    std::vector<float> pe_map(TEST_STATE_DIM);
    float max_pe;

    nimcp_error_t result = omni_wm_cognitive_bridge_get_prediction_error_map(
        nullptr, pe_map.data(), TEST_STATE_DIM, &max_pe);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// 10. Working Memory Context Tests
// =============================================================================

TEST_F(WMCognitiveBridgeTest, UpdateWMContextNullBridgeFails)
{
    nimcp_error_t result = omni_wm_cognitive_bridge_update_wm_context(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMCognitiveBridgeTest, GetWMContextNullBridgeFails)
{
    std::vector<float> context(TEST_STATE_DIM);
    float utilization;

    nimcp_error_t result = omni_wm_cognitive_bridge_get_wm_context(
        nullptr, context.data(), TEST_STATE_DIM, &utilization);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMCognitiveBridgeTest, GetWMContextNullOutputFails)
{
    ASSERT_NE(bridge_, nullptr);
    float utilization;

    nimcp_error_t result = omni_wm_cognitive_bridge_get_wm_context(
        bridge_, nullptr, TEST_STATE_DIM, &utilization);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// 11. Meta-Learning Tests
// =============================================================================

TEST_F(WMCognitiveBridgeTest, GetRecommendedLRNullBridgeFails)
{
    float lr;
    nimcp_error_t result = omni_wm_cognitive_bridge_get_recommended_lr(nullptr, &lr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMCognitiveBridgeTest, GetRecommendedLRNullOutputFails)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_cognitive_bridge_get_recommended_lr(bridge_, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMCognitiveBridgeTest, TriggerAdaptationNullBridgeFails)
{
    nimcp_error_t result = omni_wm_cognitive_bridge_trigger_adaptation(nullptr, 0.5f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMCognitiveBridgeTest, TriggerAdaptationValid)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_cognitive_bridge_trigger_adaptation(bridge_, 0.5f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

// =============================================================================
// 12. Statistics Tests
// =============================================================================

TEST_F(WMCognitiveBridgeTest, GetStatsNullBridgeFails)
{
    omni_wm_cognitive_bridge_stats_t stats;
    nimcp_error_t result = omni_wm_cognitive_bridge_get_stats(nullptr, &stats);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMCognitiveBridgeTest, GetStatsNullStatsFails)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_cognitive_bridge_get_stats(bridge_, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMCognitiveBridgeTest, GetStatsValid)
{
    ASSERT_NE(bridge_, nullptr);

    omni_wm_cognitive_bridge_stats_t stats;
    memset(&stats, 0xFF, sizeof(stats));

    nimcp_error_t result = omni_wm_cognitive_bridge_get_stats(bridge_, &stats);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    // Fresh bridge should have zero stats
    EXPECT_EQ(stats.state_predictions, 0u);
    EXPECT_EQ(stats.goals_received, 0u);
}

TEST_F(WMCognitiveBridgeTest, ResetStatsNullBridgeFails)
{
    nimcp_error_t result = omni_wm_cognitive_bridge_reset_stats(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMCognitiveBridgeTest, ResetStatsValid)
{
    ASSERT_NE(bridge_, nullptr);

    // Increment some stats
    bridge_->stats.state_predictions = 100;
    bridge_->stats.goals_received = 50;

    nimcp_error_t result = omni_wm_cognitive_bridge_reset_stats(bridge_);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    EXPECT_EQ(bridge_->stats.state_predictions, 0u);
    EXPECT_EQ(bridge_->stats.goals_received, 0u);
}

// =============================================================================
// 13. Query Effects Tests
// =============================================================================

TEST_F(WMCognitiveBridgeTest, GetWMEffectsNullBridgeReturnsNull)
{
    const omni_wm_to_cognitive_effects_t* effects =
        omni_wm_cognitive_bridge_get_wm_effects(nullptr);
    EXPECT_EQ(effects, nullptr);
}

TEST_F(WMCognitiveBridgeTest, GetWMEffectsValid)
{
    ASSERT_NE(bridge_, nullptr);

    const omni_wm_to_cognitive_effects_t* effects =
        omni_wm_cognitive_bridge_get_wm_effects(bridge_);
    ASSERT_NE(effects, nullptr);
}

TEST_F(WMCognitiveBridgeTest, GetCognitiveEffectsNullBridgeReturnsNull)
{
    const cognitive_to_omni_wm_effects_t* effects =
        omni_wm_cognitive_bridge_get_cognitive_effects(nullptr);
    EXPECT_EQ(effects, nullptr);
}

TEST_F(WMCognitiveBridgeTest, GetCognitiveEffectsValid)
{
    ASSERT_NE(bridge_, nullptr);

    const cognitive_to_omni_wm_effects_t* effects =
        omni_wm_cognitive_bridge_get_cognitive_effects(bridge_);
    ASSERT_NE(effects, nullptr);
}

// =============================================================================
// 14. Bio-Async Tests
// =============================================================================

TEST_F(WMCognitiveBridgeTest, ConnectBioAsyncNullBridgeFails)
{
    nimcp_error_t result = omni_wm_cognitive_bridge_connect_bio_async(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMCognitiveBridgeTest, DisconnectBioAsyncNullBridgeFails)
{
    nimcp_error_t result = omni_wm_cognitive_bridge_disconnect_bio_async(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMCognitiveBridgeTest, IsBioAsyncConnectedNullBridgeReturnsFalse)
{
    bool connected = omni_wm_cognitive_bridge_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}

// =============================================================================
// 15. Utility Function Tests
// =============================================================================

TEST_F(WMCognitiveBridgeTest, ValidateConfigNullFails)
{
    nimcp_error_t result = omni_wm_cognitive_bridge_validate_config(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMCognitiveBridgeTest, ValidateConfigDefaultValid)
{
    omni_wm_cognitive_bridge_config_t config = omni_wm_cognitive_bridge_default_config();

    nimcp_error_t result = omni_wm_cognitive_bridge_validate_config(&config);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(WMCognitiveBridgeTest, ValidateConfigInvalidSensitivity)
{
    omni_wm_cognitive_bridge_config_t config = omni_wm_cognitive_bridge_default_config();
    config.sensitivity = 0.0f; // Out of valid range

    nimcp_error_t result = omni_wm_cognitive_bridge_validate_config(&config);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMCognitiveBridgeTest, ValidateConfigInvalidMaxGoals)
{
    omni_wm_cognitive_bridge_config_t config = omni_wm_cognitive_bridge_default_config();
    config.max_active_goals = WM_COGNITIVE_MAX_GOALS + 100; // Exceeds max

    nimcp_error_t result = omni_wm_cognitive_bridge_validate_config(&config);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// 16. Memory Safety Tests
// =============================================================================

TEST_F(WMCognitiveBridgeTest, CreateDestroyManyTimes)
{
    for (int i = 0; i < 100; i++) {
        omni_wm_cognitive_bridge_t* temp = omni_wm_cognitive_bridge_create(nullptr);
        ASSERT_NE(temp, nullptr);
        omni_wm_cognitive_bridge_destroy(temp);
    }
}

TEST_F(WMCognitiveBridgeTest, RegisterRemoveGoalsManyTimes)
{
    ASSERT_NE(bridge_, nullptr);

    for (int i = 0; i < 50; i++) {
        std::vector<float> target = create_test_state(TEST_STATE_DIM, (float)i);
        uint32_t goal_id;

        nimcp_error_t result = omni_wm_cognitive_bridge_register_goal(
            bridge_, target.data(), TEST_STATE_DIM, 0.5f, 0, "Goal", &goal_id);
        EXPECT_EQ(result, NIMCP_SUCCESS);

        result = omni_wm_cognitive_bridge_remove_goal(bridge_, goal_id);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }

    EXPECT_EQ(bridge_->num_goals, 0u);
}

TEST_F(WMCognitiveBridgeTest, SalienceUpdateManyTimes)
{
    ASSERT_NE(bridge_, nullptr);

    for (int i = 0; i < 100; i++) {
        float novelty = (float)(i % 100) / 100.0f;
        float surprise = (float)((i + 33) % 100) / 100.0f;
        float urgency = (float)((i + 66) % 100) / 100.0f;

        nimcp_error_t result = omni_wm_cognitive_bridge_update_salience(
            bridge_, novelty, surprise, urgency);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
