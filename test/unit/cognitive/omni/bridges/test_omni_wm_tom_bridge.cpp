/**
 * @file test_omni_wm_tom_bridge.cpp
 * @brief Comprehensive unit tests for World Model Theory of Mind Bridge
 *
 * WHAT: Tests for WM-ToM bidirectional bridge for social world modeling
 * WHY:  ToM bridge enables mental state prediction and social reasoning via world model
 * HOW:  Tests all APIs: lifecycle, connections, mental state prediction, social trajectory,
 *       counterfactual belief, false belief detection, agent tracking, and edge cases
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <algorithm>

extern "C" {
#include "cognitive/omni/bridges/nimcp_omni_wm_tom_bridge.h"
#include "utils/error/nimcp_error_codes.h"
}

// =============================================================================
// Constants and Helpers
// =============================================================================

static constexpr float FLOAT_TOLERANCE = 1e-5f;
static constexpr agent_id_t TEST_AGENT_ID = 1;
static constexpr agent_id_t TEST_AGENT_ID_2 = 2;
static constexpr uint32_t TEST_HORIZON_STEPS = 5;

static bool float_equals(float a, float b, float tol = FLOAT_TOLERANCE)
{
    return std::fabs(a - b) < tol;
}

// Helper to create a test mental state
static tom_agent_mental_state_t create_test_mental_state(agent_id_t agent_id)
{
    tom_agent_mental_state_t state;
    memset(&state, 0, sizeof(state));
    state.agent_id = agent_id;
    state.emotional_state = WM_TOM_EMOTION_NEUTRAL;
    state.emotional_intensity = 0.5f;
    state.confidence = 0.8f;

    // Fill belief state with test values
    for (int i = 0; i < OMNI_WM_STATE_DIM && i < 10; i++) {
        state.belief_state[i] = (float)i * 0.1f;
    }

    return state;
}

// =============================================================================
// Test Fixture
// =============================================================================

class OmniWmTomBridgeTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Create bridge with default config for most tests
        bridge_ = omni_wm_tom_bridge_create(nullptr);
    }

    void TearDown() override
    {
        if (bridge_) {
            omni_wm_tom_bridge_destroy(bridge_);
            bridge_ = nullptr;
        }
    }

    // Helper to create config with custom settings
    omni_wm_tom_bridge_config_t create_custom_config()
    {
        omni_wm_tom_bridge_config_t config;
        omni_wm_tom_bridge_default_config(&config);
        config.enable_modulation = true;
        config.sensitivity = 1.5f;
        config.enable_mental_state_prediction = true;
        config.enable_false_belief_detection = true;
        config.enable_counterfactual_reasoning = true;
        config.max_tracked_agents = 16;
        return config;
    }

    omni_wm_tom_bridge_t* bridge_ = nullptr;
};

// =============================================================================
// 1. Default Config Tests
// =============================================================================

TEST_F(OmniWmTomBridgeTest, DefaultConfigBasic)
{
    omni_wm_tom_bridge_config_t config;
    nimcp_error_t result = omni_wm_tom_bridge_default_config(&config);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmTomBridgeTest, DefaultConfigNullFails)
{
    nimcp_error_t result = omni_wm_tom_bridge_default_config(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmTomBridgeTest, DefaultConfigSetsReasonableValues)
{
    omni_wm_tom_bridge_config_t config;
    ASSERT_EQ(omni_wm_tom_bridge_default_config(&config), NIMCP_SUCCESS);

    // Check sensitivity is in valid range
    EXPECT_GE(config.sensitivity, 0.5f);
    EXPECT_LE(config.sensitivity, 2.0f);

    // Check threshold values are reasonable
    EXPECT_GE(config.false_belief_threshold, 0.0f);
    EXPECT_LE(config.false_belief_threshold, 1.0f);

    EXPECT_GT(config.default_prediction_horizon, 0u);
    EXPECT_GT(config.max_tracked_agents, 0u);
    EXPECT_LE(config.max_tracked_agents, WM_TOM_MAX_AGENTS);
}

TEST_F(OmniWmTomBridgeTest, ValidateConfigValidConfig)
{
    omni_wm_tom_bridge_config_t config;
    omni_wm_tom_bridge_default_config(&config);

    nimcp_error_t result = omni_wm_tom_bridge_validate_config(&config);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmTomBridgeTest, ValidateConfigNullFails)
{
    nimcp_error_t result = omni_wm_tom_bridge_validate_config(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// 2. Lifecycle Tests - Create
// =============================================================================

TEST_F(OmniWmTomBridgeTest, CreateWithNullConfig)
{
    ASSERT_NE(bridge_, nullptr);
}

TEST_F(OmniWmTomBridgeTest, CreateWithCustomConfig)
{
    omni_wm_tom_bridge_config_t config = create_custom_config();

    omni_wm_tom_bridge_t* bridge = omni_wm_tom_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Verify config was applied
    EXPECT_TRUE(bridge->config.enable_modulation);
    EXPECT_FLOAT_EQ(bridge->config.sensitivity, 1.5f);
    EXPECT_TRUE(bridge->config.enable_mental_state_prediction);
    EXPECT_EQ(bridge->config.max_tracked_agents, 16u);

    omni_wm_tom_bridge_destroy(bridge);
}

TEST_F(OmniWmTomBridgeTest, CreateInitializesBaseFields)
{
    ASSERT_NE(bridge_, nullptr);

    EXPECT_EQ(bridge_->base.module_id, BIO_MODULE_WM_TOM_BRIDGE);
    EXPECT_NE(bridge_->base.module_name, nullptr);
    EXPECT_FALSE(bridge_->base.bridge_active);
}

TEST_F(OmniWmTomBridgeTest, CreateInitializesAgentTracking)
{
    ASSERT_NE(bridge_, nullptr);

    EXPECT_EQ(bridge_->tracked_agent_count, 0u);
    EXPECT_EQ(bridge_->belief_gap_count, 0u);
}

TEST_F(OmniWmTomBridgeTest, CreateInitializesStats)
{
    ASSERT_NE(bridge_, nullptr);

    EXPECT_EQ(bridge_->stats.mental_state_predictions, 0u);
    EXPECT_EQ(bridge_->stats.false_beliefs_detected, 0u);
    EXPECT_EQ(bridge_->stats.trajectory_predictions, 0u);
    EXPECT_EQ(bridge_->stats.total_updates, 0u);
    EXPECT_EQ(bridge_->stats.errors_total, 0u);
}

// =============================================================================
// 3. Lifecycle Tests - Destroy
// =============================================================================

TEST_F(OmniWmTomBridgeTest, DestroyNullSafe)
{
    omni_wm_tom_bridge_destroy(nullptr);
}

TEST_F(OmniWmTomBridgeTest, DestroyValidBridge)
{
    omni_wm_tom_bridge_t* bridge = omni_wm_tom_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);
    omni_wm_tom_bridge_destroy(bridge);
}

// =============================================================================
// 4. Lifecycle Tests - Reset
// =============================================================================

TEST_F(OmniWmTomBridgeTest, ResetBasic)
{
    nimcp_error_t result = omni_wm_tom_bridge_reset(bridge_);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmTomBridgeTest, ResetNullFails)
{
    nimcp_error_t result = omni_wm_tom_bridge_reset(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmTomBridgeTest, ResetClearsStats)
{
    bridge_->stats.mental_state_predictions = 100;
    bridge_->stats.false_beliefs_detected = 5;
    bridge_->stats.total_updates = 50;

    nimcp_error_t result = omni_wm_tom_bridge_reset(bridge_);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    EXPECT_EQ(bridge_->stats.mental_state_predictions, 0u);
    EXPECT_EQ(bridge_->stats.false_beliefs_detected, 0u);
    EXPECT_EQ(bridge_->stats.total_updates, 0u);
}

TEST_F(OmniWmTomBridgeTest, ResetPreservesConfig)
{
    bridge_->config.sensitivity = 1.75f;
    bridge_->config.enable_empathy_simulation = true;

    omni_wm_tom_bridge_reset(bridge_);

    EXPECT_FLOAT_EQ(bridge_->config.sensitivity, 1.75f);
    EXPECT_TRUE(bridge_->config.enable_empathy_simulation);
}

// =============================================================================
// 5. Connection Tests
// =============================================================================

TEST_F(OmniWmTomBridgeTest, ConnectNullBridgeFails)
{
    omni_world_model_t* dummy_wm = reinterpret_cast<omni_world_model_t*>(0x1234);
    theory_of_mind_t dummy_tom = reinterpret_cast<theory_of_mind_t>(0x5678);

    nimcp_error_t result = omni_wm_tom_bridge_connect(nullptr, dummy_wm, dummy_tom, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmTomBridgeTest, ConnectNullWorldModelFails)
{
    theory_of_mind_t dummy_tom = reinterpret_cast<theory_of_mind_t>(0x5678);

    nimcp_error_t result = omni_wm_tom_bridge_connect(bridge_, nullptr, dummy_tom, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmTomBridgeTest, ConnectNullTomFails)
{
    omni_world_model_t* dummy_wm = reinterpret_cast<omni_world_model_t*>(0x1234);

    nimcp_error_t result = omni_wm_tom_bridge_connect(bridge_, dummy_wm, nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmTomBridgeTest, ConnectMinimalSystems)
{
    omni_world_model_t* dummy_wm = reinterpret_cast<omni_world_model_t*>(0x1234);
    theory_of_mind_t dummy_tom = reinterpret_cast<theory_of_mind_t>(0x5678);

    nimcp_error_t result = omni_wm_tom_bridge_connect(bridge_, dummy_wm, dummy_tom, nullptr);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(bridge_->world_model, dummy_wm);
    EXPECT_EQ(bridge_->tom, dummy_tom);
    EXPECT_TRUE(omni_wm_tom_bridge_is_connected(bridge_));
}

TEST_F(OmniWmTomBridgeTest, ConnectAllSystems)
{
    omni_world_model_t* dummy_wm = reinterpret_cast<omni_world_model_t*>(0x1234);
    theory_of_mind_t dummy_tom = reinterpret_cast<theory_of_mind_t>(0x5678);
    mirror_neurons_t dummy_mirror = reinterpret_cast<mirror_neurons_t>(0x9ABC);

    nimcp_error_t result = omni_wm_tom_bridge_connect(bridge_, dummy_wm, dummy_tom, dummy_mirror);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    EXPECT_EQ(bridge_->world_model, dummy_wm);
    EXPECT_EQ(bridge_->tom, dummy_tom);
    EXPECT_EQ(bridge_->mirror, dummy_mirror);
}

TEST_F(OmniWmTomBridgeTest, ConnectWorldModelSeparate)
{
    omni_world_model_t* dummy_wm = reinterpret_cast<omni_world_model_t*>(0x1234);

    nimcp_error_t result = omni_wm_tom_bridge_connect_world_model(bridge_, dummy_wm);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(bridge_->world_model, dummy_wm);
}

TEST_F(OmniWmTomBridgeTest, ConnectTomSeparate)
{
    theory_of_mind_t dummy_tom = reinterpret_cast<theory_of_mind_t>(0x5678);

    nimcp_error_t result = omni_wm_tom_bridge_connect_tom(bridge_, dummy_tom);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(bridge_->tom, dummy_tom);
}

TEST_F(OmniWmTomBridgeTest, ConnectMirrorSeparate)
{
    mirror_neurons_t dummy_mirror = reinterpret_cast<mirror_neurons_t>(0x9ABC);

    nimcp_error_t result = omni_wm_tom_bridge_connect_mirror(bridge_, dummy_mirror);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(bridge_->mirror, dummy_mirror);
}

TEST_F(OmniWmTomBridgeTest, ConnectSocialBridgeSeparate)
{
    tom_social_bridge_t* dummy_social = reinterpret_cast<tom_social_bridge_t*>(0xDEF0);

    nimcp_error_t result = omni_wm_tom_bridge_connect_social(bridge_, dummy_social);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(bridge_->social_bridge, dummy_social);
}

TEST_F(OmniWmTomBridgeTest, IsConnectedWithNoConnection)
{
    EXPECT_FALSE(omni_wm_tom_bridge_is_connected(bridge_));
}

TEST_F(OmniWmTomBridgeTest, IsConnectedNullFalse)
{
    EXPECT_FALSE(omni_wm_tom_bridge_is_connected(nullptr));
}

// =============================================================================
// 6. Update Tests
// =============================================================================

TEST_F(OmniWmTomBridgeTest, UpdateNullBridgeFails)
{
    nimcp_error_t result = omni_wm_tom_bridge_update(nullptr, 0.016f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmTomBridgeTest, UpdateWithoutConnectionHandled)
{
    nimcp_error_t result = omni_wm_tom_bridge_update(bridge_, 0.016f);
    // Should handle gracefully without crash
}

TEST_F(OmniWmTomBridgeTest, UpdateWithZeroDt)
{
    omni_world_model_t* dummy_wm = reinterpret_cast<omni_world_model_t*>(0x1234);
    theory_of_mind_t dummy_tom = reinterpret_cast<theory_of_mind_t>(0x5678);
    omni_wm_tom_bridge_connect(bridge_, dummy_wm, dummy_tom, nullptr);

    nimcp_error_t result = omni_wm_tom_bridge_update(bridge_, 0.0f);
    // Should handle zero dt gracefully
}

TEST_F(OmniWmTomBridgeTest, UpdateWithNegativeDt)
{
    omni_world_model_t* dummy_wm = reinterpret_cast<omni_world_model_t*>(0x1234);
    theory_of_mind_t dummy_tom = reinterpret_cast<theory_of_mind_t>(0x5678);
    omni_wm_tom_bridge_connect(bridge_, dummy_wm, dummy_tom, nullptr);

    nimcp_error_t result = omni_wm_tom_bridge_update(bridge_, -0.016f);
    // Should handle negative dt
}

// =============================================================================
// 7. Mental State Prediction Tests (predict_mental_state)
// =============================================================================

TEST_F(OmniWmTomBridgeTest, PredictMentalStateNullBridgeFails)
{
    tom_agent_mental_state_t predicted_state;
    nimcp_error_t result = omni_wm_tom_bridge_predict_mental_state(
        nullptr, TEST_AGENT_ID, TEST_HORIZON_STEPS, &predicted_state);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmTomBridgeTest, PredictMentalStateNullOutputFails)
{
    nimcp_error_t result = omni_wm_tom_bridge_predict_mental_state(
        bridge_, TEST_AGENT_ID, TEST_HORIZON_STEPS, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmTomBridgeTest, PredictMentalStateWithoutConnectionHandled)
{
    tom_agent_mental_state_t predicted_state;
    memset(&predicted_state, 0, sizeof(predicted_state));

    nimcp_error_t result = omni_wm_tom_bridge_predict_mental_state(
        bridge_, TEST_AGENT_ID, TEST_HORIZON_STEPS, &predicted_state);
    // Should handle gracefully
}

TEST_F(OmniWmTomBridgeTest, PredictMentalStateZeroHorizonHandled)
{
    tom_agent_mental_state_t predicted_state;
    memset(&predicted_state, 0, sizeof(predicted_state));

    nimcp_error_t result = omni_wm_tom_bridge_predict_mental_state(
        bridge_, TEST_AGENT_ID, 0, &predicted_state);
    // Zero horizon may be invalid
}

TEST_F(OmniWmTomBridgeTest, UpdateMentalStateNullBridgeFails)
{
    tom_agent_mental_state_t state = create_test_mental_state(TEST_AGENT_ID);
    nimcp_error_t result = omni_wm_tom_bridge_update_mental_state(nullptr, TEST_AGENT_ID, &state);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmTomBridgeTest, UpdateMentalStateNullStateFails)
{
    nimcp_error_t result = omni_wm_tom_bridge_update_mental_state(bridge_, TEST_AGENT_ID, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// 8. Social Trajectory Prediction Tests (predict_social_trajectory)
// =============================================================================

TEST_F(OmniWmTomBridgeTest, PredictSocialTrajectoryNullBridgeFails)
{
    tom_social_trajectory_t trajectory;
    nimcp_error_t result = omni_wm_tom_bridge_predict_social_trajectory(
        nullptr, TEST_AGENT_ID, TEST_HORIZON_STEPS, &trajectory);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmTomBridgeTest, PredictSocialTrajectoryNullOutputFails)
{
    nimcp_error_t result = omni_wm_tom_bridge_predict_social_trajectory(
        bridge_, TEST_AGENT_ID, TEST_HORIZON_STEPS, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmTomBridgeTest, PredictSocialTrajectoryWithoutConnectionHandled)
{
    tom_social_trajectory_t trajectory;
    memset(&trajectory, 0, sizeof(trajectory));

    nimcp_error_t result = omni_wm_tom_bridge_predict_social_trajectory(
        bridge_, TEST_AGENT_ID, TEST_HORIZON_STEPS, &trajectory);
    // Should handle gracefully
}

TEST_F(OmniWmTomBridgeTest, PredictSocialTrajectoryZeroHorizonHandled)
{
    tom_social_trajectory_t trajectory;
    memset(&trajectory, 0, sizeof(trajectory));

    nimcp_error_t result = omni_wm_tom_bridge_predict_social_trajectory(
        bridge_, TEST_AGENT_ID, 0, &trajectory);
    // Zero horizon handling
}

TEST_F(OmniWmTomBridgeTest, PredictSocialTrajectoryMaxHorizon)
{
    tom_social_trajectory_t trajectory;
    memset(&trajectory, 0, sizeof(trajectory));

    nimcp_error_t result = omni_wm_tom_bridge_predict_social_trajectory(
        bridge_, TEST_AGENT_ID, WM_TOM_MAX_HORIZON, &trajectory);
    // Max horizon handling
}

TEST_F(OmniWmTomBridgeTest, PredictSocialTrajectoryExceedsMaxHorizon)
{
    tom_social_trajectory_t trajectory;
    memset(&trajectory, 0, sizeof(trajectory));

    nimcp_error_t result = omni_wm_tom_bridge_predict_social_trajectory(
        bridge_, TEST_AGENT_ID, WM_TOM_MAX_HORIZON + 10, &trajectory);
    // Should handle exceeding max - may clamp or error
}

TEST_F(OmniWmTomBridgeTest, PredictJointTrajectoryNullBridgeFails)
{
    agent_id_t agent_ids[] = {TEST_AGENT_ID, TEST_AGENT_ID_2};
    tom_social_trajectory_t trajectories[2];

    nimcp_error_t result = omni_wm_tom_bridge_predict_joint_trajectory(
        nullptr, agent_ids, 2, TEST_HORIZON_STEPS, trajectories);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmTomBridgeTest, PredictJointTrajectoryNullIdsFails)
{
    tom_social_trajectory_t trajectories[2];

    nimcp_error_t result = omni_wm_tom_bridge_predict_joint_trajectory(
        bridge_, nullptr, 2, TEST_HORIZON_STEPS, trajectories);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmTomBridgeTest, PredictJointTrajectoryZeroAgents)
{
    agent_id_t agent_ids[] = {TEST_AGENT_ID};
    tom_social_trajectory_t trajectories[1];

    nimcp_error_t result = omni_wm_tom_bridge_predict_joint_trajectory(
        bridge_, agent_ids, 0, TEST_HORIZON_STEPS, trajectories);
    // Zero agents handling
}

// =============================================================================
// 9. Counterfactual Belief Tests (counterfactual_belief)
// =============================================================================

TEST_F(OmniWmTomBridgeTest, CounterfactualBeliefNullBridgeFails)
{
    float hypothetical_belief[OMNI_WM_STATE_DIM] = {0};
    tom_social_trajectory_t trajectory;

    nimcp_error_t result = omni_wm_tom_bridge_counterfactual_belief(
        nullptr, TEST_AGENT_ID, hypothetical_belief, &trajectory);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmTomBridgeTest, CounterfactualBeliefNullBeliefFails)
{
    tom_social_trajectory_t trajectory;

    nimcp_error_t result = omni_wm_tom_bridge_counterfactual_belief(
        bridge_, TEST_AGENT_ID, nullptr, &trajectory);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmTomBridgeTest, CounterfactualBeliefNullOutputFails)
{
    float hypothetical_belief[OMNI_WM_STATE_DIM] = {0};

    nimcp_error_t result = omni_wm_tom_bridge_counterfactual_belief(
        bridge_, TEST_AGENT_ID, hypothetical_belief, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmTomBridgeTest, CounterfactualBeliefWithoutConnectionHandled)
{
    float hypothetical_belief[OMNI_WM_STATE_DIM] = {0};
    tom_social_trajectory_t trajectory;
    memset(&trajectory, 0, sizeof(trajectory));

    nimcp_error_t result = omni_wm_tom_bridge_counterfactual_belief(
        bridge_, TEST_AGENT_ID, hypothetical_belief, &trajectory);
    // Should handle gracefully
}

TEST_F(OmniWmTomBridgeTest, CounterfactualActionNullBridgeFails)
{
    float hypothetical_action[OMNI_WM_ACTION_DIM] = {0};
    tom_social_trajectory_t trajectory;

    nimcp_error_t result = omni_wm_tom_bridge_counterfactual_action(
        nullptr, TEST_AGENT_ID, hypothetical_action, OMNI_WM_ACTION_DIM, &trajectory);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmTomBridgeTest, CounterfactualActionNullActionFails)
{
    tom_social_trajectory_t trajectory;

    nimcp_error_t result = omni_wm_tom_bridge_counterfactual_action(
        bridge_, TEST_AGENT_ID, nullptr, OMNI_WM_ACTION_DIM, &trajectory);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmTomBridgeTest, CounterfactualActionZeroDimHandled)
{
    float hypothetical_action[1] = {0};
    tom_social_trajectory_t trajectory;
    memset(&trajectory, 0, sizeof(trajectory));

    nimcp_error_t result = omni_wm_tom_bridge_counterfactual_action(
        bridge_, TEST_AGENT_ID, hypothetical_action, 0, &trajectory);
    // Zero dim handling
}

// =============================================================================
// 10. False Belief Detection Tests (detect_false_beliefs)
// =============================================================================

TEST_F(OmniWmTomBridgeTest, DetectFalseBeliefsNullBridgeFails)
{
    tom_belief_reality_gap_t gaps[WM_TOM_MAX_AGENTS];
    uint32_t gap_count;

    nimcp_error_t result = omni_wm_tom_bridge_detect_false_beliefs(nullptr, gaps, &gap_count);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmTomBridgeTest, DetectFalseBeliefsNullOutputFails)
{
    uint32_t gap_count;

    nimcp_error_t result = omni_wm_tom_bridge_detect_false_beliefs(bridge_, nullptr, &gap_count);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmTomBridgeTest, DetectFalseBeliefsNullCountFails)
{
    tom_belief_reality_gap_t gaps[WM_TOM_MAX_AGENTS];

    nimcp_error_t result = omni_wm_tom_bridge_detect_false_beliefs(bridge_, gaps, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmTomBridgeTest, DetectFalseBeliefsWithoutConnectionHandled)
{
    tom_belief_reality_gap_t gaps[WM_TOM_MAX_AGENTS];
    uint32_t gap_count = 0;

    nimcp_error_t result = omni_wm_tom_bridge_detect_false_beliefs(bridge_, gaps, &gap_count);
    // Should handle gracefully - likely returns 0 gaps
}

TEST_F(OmniWmTomBridgeTest, GetBeliefGapNullBridgeFails)
{
    tom_belief_reality_gap_t gap;

    nimcp_error_t result = omni_wm_tom_bridge_get_belief_gap(nullptr, TEST_AGENT_ID, &gap);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmTomBridgeTest, GetBeliefGapNullOutputFails)
{
    nimcp_error_t result = omni_wm_tom_bridge_get_belief_gap(bridge_, TEST_AGENT_ID, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmTomBridgeTest, GetBeliefGapUnknownAgentHandled)
{
    tom_belief_reality_gap_t gap;
    memset(&gap, 0, sizeof(gap));

    nimcp_error_t result = omni_wm_tom_bridge_get_belief_gap(bridge_, 9999, &gap);
    // Unknown agent - may return error or default gap
}

// =============================================================================
// 11. Agent Tracking Tests
// =============================================================================

TEST_F(OmniWmTomBridgeTest, TrackAgentNullBridgeFails)
{
    nimcp_error_t result = omni_wm_tom_bridge_track_agent(nullptr, TEST_AGENT_ID, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmTomBridgeTest, TrackAgentBasic)
{
    nimcp_error_t result = omni_wm_tom_bridge_track_agent(bridge_, TEST_AGENT_ID, nullptr);
    // Without full initialization, may or may not succeed
}

TEST_F(OmniWmTomBridgeTest, TrackAgentWithInitialState)
{
    tom_agent_mental_state_t initial_state = create_test_mental_state(TEST_AGENT_ID);

    nimcp_error_t result = omni_wm_tom_bridge_track_agent(bridge_, TEST_AGENT_ID, &initial_state);
    // Should handle provided initial state
}

TEST_F(OmniWmTomBridgeTest, UntrackAgentNullBridgeFails)
{
    nimcp_error_t result = omni_wm_tom_bridge_untrack_agent(nullptr, TEST_AGENT_ID);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmTomBridgeTest, UntrackAgentUnknownAgentHandled)
{
    nimcp_error_t result = omni_wm_tom_bridge_untrack_agent(bridge_, 9999);
    // Untracking unknown agent should be handled
}

TEST_F(OmniWmTomBridgeTest, GetAgentStateNullBridgeFails)
{
    tom_agent_mental_state_t state;

    nimcp_error_t result = omni_wm_tom_bridge_get_agent_state(nullptr, TEST_AGENT_ID, &state);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmTomBridgeTest, GetAgentStateNullOutputFails)
{
    nimcp_error_t result = omni_wm_tom_bridge_get_agent_state(bridge_, TEST_AGENT_ID, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmTomBridgeTest, SetFocusAgentNullBridgeFails)
{
    nimcp_error_t result = omni_wm_tom_bridge_set_focus_agent(nullptr, TEST_AGENT_ID);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmTomBridgeTest, SetFocusAgentBasic)
{
    nimcp_error_t result = omni_wm_tom_bridge_set_focus_agent(bridge_, TEST_AGENT_ID);
    // May succeed or fail depending on whether agent is tracked
}

// =============================================================================
// 12. Social Context Tests
// =============================================================================

TEST_F(OmniWmTomBridgeTest, SetCooperativeContextNullBridgeFails)
{
    nimcp_error_t result = omni_wm_tom_bridge_set_cooperative_context(nullptr, true);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmTomBridgeTest, SetCooperativeContextTrue)
{
    nimcp_error_t result = omni_wm_tom_bridge_set_cooperative_context(bridge_, true);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_TRUE(bridge_->cooperative_mode);
}

TEST_F(OmniWmTomBridgeTest, SetCooperativeContextFalse)
{
    nimcp_error_t result = omni_wm_tom_bridge_set_cooperative_context(bridge_, false);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_FALSE(bridge_->cooperative_mode);
}

TEST_F(OmniWmTomBridgeTest, SetCompetitiveContextNullBridgeFails)
{
    nimcp_error_t result = omni_wm_tom_bridge_set_competitive_context(nullptr, true);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmTomBridgeTest, SetCompetitiveContextTrue)
{
    nimcp_error_t result = omni_wm_tom_bridge_set_competitive_context(bridge_, true);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_TRUE(bridge_->competitive_mode);
}

TEST_F(OmniWmTomBridgeTest, SetCompetitiveContextFalse)
{
    nimcp_error_t result = omni_wm_tom_bridge_set_competitive_context(bridge_, false);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_FALSE(bridge_->competitive_mode);
}

// =============================================================================
// 13. Training Tests
// =============================================================================

TEST_F(OmniWmTomBridgeTest, TrainFromInteractionNullBridgeFails)
{
    tom_social_interaction_t interaction;
    memset(&interaction, 0, sizeof(interaction));

    nimcp_error_t result = omni_wm_tom_bridge_train_from_interaction(nullptr, &interaction);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmTomBridgeTest, TrainFromInteractionNullInteractionFails)
{
    nimcp_error_t result = omni_wm_tom_bridge_train_from_interaction(bridge_, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmTomBridgeTest, TrainBatchNullBridgeFails)
{
    nimcp_error_t result = omni_wm_tom_bridge_train_batch(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// 14. Mirror Neuron Integration Tests
// =============================================================================

TEST_F(OmniWmTomBridgeTest, OnMirrorActionNullBridgeFails)
{
    float action[OMNI_WM_ACTION_DIM] = {0};

    nimcp_error_t result = omni_wm_tom_bridge_on_mirror_action(
        nullptr, TEST_AGENT_ID, action, OMNI_WM_ACTION_DIM, 0.9f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmTomBridgeTest, OnMirrorActionNullActionFails)
{
    nimcp_error_t result = omni_wm_tom_bridge_on_mirror_action(
        bridge_, TEST_AGENT_ID, nullptr, OMNI_WM_ACTION_DIM, 0.9f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmTomBridgeTest, OnMirrorActionZeroDimHandled)
{
    float action[1] = {0};

    nimcp_error_t result = omni_wm_tom_bridge_on_mirror_action(
        bridge_, TEST_AGENT_ID, action, 0, 0.9f);
    // Zero dim handling
}

TEST_F(OmniWmTomBridgeTest, OnMirrorActionLowConfidence)
{
    float action[OMNI_WM_ACTION_DIM] = {0};

    nimcp_error_t result = omni_wm_tom_bridge_on_mirror_action(
        bridge_, TEST_AGENT_ID, action, OMNI_WM_ACTION_DIM, 0.0f);
    // Zero confidence handling
}

// =============================================================================
// 15. Empathy Simulation Tests
// =============================================================================

TEST_F(OmniWmTomBridgeTest, EmpathySimulationNullBridgeFails)
{
    float simulated_state[OMNI_WM_STATE_DIM];

    nimcp_error_t result = omni_wm_tom_bridge_empathy_simulation(
        nullptr, TEST_AGENT_ID, simulated_state);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmTomBridgeTest, EmpathySimulationNullOutputFails)
{
    nimcp_error_t result = omni_wm_tom_bridge_empathy_simulation(
        bridge_, TEST_AGENT_ID, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// 16. Query API Tests
// =============================================================================

TEST_F(OmniWmTomBridgeTest, GetWmEffectsBasic)
{
    const omni_wm_to_tom_effects_t* effects = omni_wm_tom_bridge_get_wm_effects(bridge_);
    EXPECT_NE(effects, nullptr);
}

TEST_F(OmniWmTomBridgeTest, GetWmEffectsNullBridge)
{
    const omni_wm_to_tom_effects_t* effects = omni_wm_tom_bridge_get_wm_effects(nullptr);
    EXPECT_EQ(effects, nullptr);
}

TEST_F(OmniWmTomBridgeTest, GetTomEffectsBasic)
{
    const tom_to_omni_wm_effects_t* effects = omni_wm_tom_bridge_get_tom_effects(bridge_);
    EXPECT_NE(effects, nullptr);
}

TEST_F(OmniWmTomBridgeTest, GetTomEffectsNullBridge)
{
    const tom_to_omni_wm_effects_t* effects = omni_wm_tom_bridge_get_tom_effects(nullptr);
    EXPECT_EQ(effects, nullptr);
}

// =============================================================================
// 17. Statistics Tests
// =============================================================================

TEST_F(OmniWmTomBridgeTest, GetStatsBasic)
{
    omni_wm_tom_bridge_stats_t stats;
    memset(&stats, 0xFF, sizeof(stats));

    nimcp_error_t result = omni_wm_tom_bridge_get_stats(bridge_, &stats);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(stats.total_updates, 0u);
}

TEST_F(OmniWmTomBridgeTest, GetStatsNullBridgeFails)
{
    omni_wm_tom_bridge_stats_t stats;
    nimcp_error_t result = omni_wm_tom_bridge_get_stats(nullptr, &stats);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmTomBridgeTest, GetStatsNullOutputFails)
{
    nimcp_error_t result = omni_wm_tom_bridge_get_stats(bridge_, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmTomBridgeTest, ResetStatsBasic)
{
    bridge_->stats.mental_state_predictions = 100;
    bridge_->stats.errors_total = 5;

    nimcp_error_t result = omni_wm_tom_bridge_reset_stats(bridge_);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    EXPECT_EQ(bridge_->stats.mental_state_predictions, 0u);
    EXPECT_EQ(bridge_->stats.errors_total, 0u);
}

TEST_F(OmniWmTomBridgeTest, ResetStatsNullBridgeFails)
{
    nimcp_error_t result = omni_wm_tom_bridge_reset_stats(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// 18. Bio-Async Tests
// =============================================================================

TEST_F(OmniWmTomBridgeTest, ConnectBioAsyncNullBridgeFails)
{
    nimcp_error_t result = omni_wm_tom_bridge_connect_bio_async(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmTomBridgeTest, DisconnectBioAsyncNullBridgeFails)
{
    nimcp_error_t result = omni_wm_tom_bridge_disconnect_bio_async(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmTomBridgeTest, IsBioAsyncConnectedNullFalse)
{
    EXPECT_FALSE(omni_wm_tom_bridge_is_bio_async_connected(nullptr));
}

TEST_F(OmniWmTomBridgeTest, IsBioAsyncConnectedInitiallyFalse)
{
    EXPECT_FALSE(omni_wm_tom_bridge_is_bio_async_connected(bridge_));
}

// =============================================================================
// 19. Utility Function Tests
// =============================================================================

TEST_F(OmniWmTomBridgeTest, EmotionToStringAllEmotions)
{
    EXPECT_NE(omni_wm_tom_emotion_to_string(WM_TOM_EMOTION_UNKNOWN), nullptr);
    EXPECT_NE(omni_wm_tom_emotion_to_string(WM_TOM_EMOTION_NEUTRAL), nullptr);
    EXPECT_NE(omni_wm_tom_emotion_to_string(WM_TOM_EMOTION_JOY), nullptr);
    EXPECT_NE(omni_wm_tom_emotion_to_string(WM_TOM_EMOTION_SADNESS), nullptr);
    EXPECT_NE(omni_wm_tom_emotion_to_string(WM_TOM_EMOTION_ANGER), nullptr);
    EXPECT_NE(omni_wm_tom_emotion_to_string(WM_TOM_EMOTION_FEAR), nullptr);
    EXPECT_NE(omni_wm_tom_emotion_to_string(WM_TOM_EMOTION_SURPRISE), nullptr);
    EXPECT_NE(omni_wm_tom_emotion_to_string(WM_TOM_EMOTION_DISGUST), nullptr);
    EXPECT_NE(omni_wm_tom_emotion_to_string(WM_TOM_EMOTION_ANXIETY), nullptr);
    EXPECT_NE(omni_wm_tom_emotion_to_string(WM_TOM_EMOTION_CALM), nullptr);
}

TEST_F(OmniWmTomBridgeTest, EmotionToStringInvalid)
{
    const char* str = omni_wm_tom_emotion_to_string(static_cast<wm_tom_emotion_t>(999));
    EXPECT_NE(str, nullptr);
}

// =============================================================================
// 20. Memory Safety Tests
// =============================================================================

TEST_F(OmniWmTomBridgeTest, MultipleCreateDestroy)
{
    for (int i = 0; i < 10; i++) {
        omni_wm_tom_bridge_t* b = omni_wm_tom_bridge_create(nullptr);
        ASSERT_NE(b, nullptr);
        omni_wm_tom_bridge_destroy(b);
    }
}

TEST_F(OmniWmTomBridgeTest, MutexIsInitialized)
{
    ASSERT_NE(bridge_, nullptr);
    EXPECT_NE(bridge_->base.mutex, nullptr);
}

TEST_F(OmniWmTomBridgeTest, ConfigAllFeaturesEnabled)
{
    omni_wm_tom_bridge_config_t config;
    omni_wm_tom_bridge_default_config(&config);

    config.enable_modulation = true;
    config.enable_mental_state_prediction = true;
    config.enable_false_belief_detection = true;
    config.enable_counterfactual_reasoning = true;
    config.enable_social_training = true;
    config.enable_joint_prediction = true;
    config.enable_mirror_integration = true;
    config.enable_empathy_simulation = true;
    config.enable_bio_async = true;

    omni_wm_tom_bridge_t* bridge = omni_wm_tom_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);
    omni_wm_tom_bridge_destroy(bridge);
}

TEST_F(OmniWmTomBridgeTest, ConfigAllFeaturesDisabled)
{
    omni_wm_tom_bridge_config_t config;
    omni_wm_tom_bridge_default_config(&config);

    config.enable_modulation = false;
    config.enable_mental_state_prediction = false;
    config.enable_false_belief_detection = false;
    config.enable_counterfactual_reasoning = false;
    config.enable_social_training = false;
    config.enable_joint_prediction = false;
    config.enable_mirror_integration = false;
    config.enable_empathy_simulation = false;
    config.enable_bio_async = false;

    omni_wm_tom_bridge_t* bridge = omni_wm_tom_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);
    omni_wm_tom_bridge_destroy(bridge);
}

// Main function for standalone execution
int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
