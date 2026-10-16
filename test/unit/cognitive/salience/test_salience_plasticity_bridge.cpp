/**
 * @file test_salience_plasticity_bridge.cpp
 * @brief Unit tests for Salience-Plasticity Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

#include "cognitive/salience/nimcp_salience_plasticity_bridge.h"

class SaliencePlasticityBridgeTest : public ::testing::Test {
protected:
    salience_plasticity_bridge_t* bridge = nullptr;

    void SetUp() override {
        salience_plasticity_config_t config = salience_plasticity_config_default();
        bridge = salience_plasticity_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            salience_plasticity_destroy(bridge);
            bridge = nullptr;
        }
    }
};

// ============================================================================
// Lifecycle Tests
// ============================================================================

TEST_F(SaliencePlasticityBridgeTest, CreateWithNullConfig) {
    salience_plasticity_bridge_t* b = salience_plasticity_create(nullptr);
    EXPECT_EQ(b, nullptr);
}

TEST_F(SaliencePlasticityBridgeTest, DefaultConfigValid) {
    salience_plasticity_config_t config = salience_plasticity_config_default();
    EXPECT_GT(config.stdp_ltp_window_ms, 0.0f);
    EXPECT_GT(config.stdp_a_plus, 0.0f);
    EXPECT_GT(config.stdp_a_minus, 0.0f);
    EXPECT_TRUE(config.enable_habituation);
}

TEST_F(SaliencePlasticityBridgeTest, ResetSucceeds) {
    int result = salience_plasticity_reset(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(SaliencePlasticityBridgeTest, ResetNullBridge) {
    int result = salience_plasticity_reset(nullptr);
    EXPECT_EQ(result, -1);
}

// ============================================================================
// Synapse Management Tests
// ============================================================================

TEST_F(SaliencePlasticityBridgeTest, RegisterSynapse) {
    int result = salience_plasticity_register_synapse(
        bridge, 1, SALIENCE_SYNAPSE_NOVELTY, 0, 0.5f);
    EXPECT_EQ(result, 0);
}

TEST_F(SaliencePlasticityBridgeTest, RegisterSynapseDuplicate) {
    salience_plasticity_register_synapse(bridge, 1, SALIENCE_SYNAPSE_NOVELTY, 0, 0.5f);
    int result = salience_plasticity_register_synapse(
        bridge, 1, SALIENCE_SYNAPSE_SURPRISE, 1, 0.6f);
    EXPECT_EQ(result, -1);
}

TEST_F(SaliencePlasticityBridgeTest, UnregisterSynapse) {
    salience_plasticity_register_synapse(bridge, 1, SALIENCE_SYNAPSE_NOVELTY, 0, 0.5f);
    int result = salience_plasticity_unregister_synapse(bridge, 1);
    EXPECT_EQ(result, 0);
}

TEST_F(SaliencePlasticityBridgeTest, UnregisterNonexistentSynapse) {
    int result = salience_plasticity_unregister_synapse(bridge, 999);
    EXPECT_EQ(result, -1);
}

TEST_F(SaliencePlasticityBridgeTest, GetSynapse) {
    salience_plasticity_register_synapse(bridge, 1, SALIENCE_SYNAPSE_NOVELTY, 0, 0.5f);

    salience_plasticity_synapse_t synapse;
    int result = salience_plasticity_get_synapse(bridge, 1, &synapse);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(synapse.synapse_id, 1u);
    EXPECT_EQ(synapse.type, SALIENCE_SYNAPSE_NOVELTY);
    EXPECT_FLOAT_EQ(synapse.weight, 0.5f);
}

TEST_F(SaliencePlasticityBridgeTest, GetNonexistentSynapse) {
    salience_plasticity_synapse_t synapse;
    int result = salience_plasticity_get_synapse(bridge, 999, &synapse);
    EXPECT_EQ(result, -1);
}

// ============================================================================
// Event Recording Tests
// ============================================================================

TEST_F(SaliencePlasticityBridgeTest, AttentionEvent) {
    salience_plasticity_register_synapse(bridge, 1, SALIENCE_SYNAPSE_NOVELTY, 0, 0.5f);

    int result = salience_plasticity_attention_event(bridge, 0, 0.8f, 1000);
    EXPECT_EQ(result, 0);
}

TEST_F(SaliencePlasticityBridgeTest, AttentionFeedbackCorrect) {
    salience_plasticity_register_synapse(bridge, 1, SALIENCE_SYNAPSE_VALUE, 0, 0.5f);
    salience_plasticity_attention_event(bridge, 0, 0.8f, 1000);

    int result = salience_plasticity_attention_feedback(bridge, 0, true, 2000);
    EXPECT_EQ(result, 0);
}

TEST_F(SaliencePlasticityBridgeTest, AttentionFeedbackIncorrect) {
    salience_plasticity_register_synapse(bridge, 1, SALIENCE_SYNAPSE_VALUE, 0, 0.5f);
    salience_plasticity_attention_event(bridge, 0, 0.8f, 1000);

    int result = salience_plasticity_attention_feedback(bridge, 0, false, 2000);
    EXPECT_EQ(result, 0);
}

TEST_F(SaliencePlasticityBridgeTest, FeatureExposure) {
    salience_plasticity_register_synapse(bridge, 1, SALIENCE_SYNAPSE_HABITUATION, 0, 0.5f);

    int result = salience_plasticity_feature_exposure(bridge, 0, 0.7f, 1000);
    EXPECT_EQ(result, 0);
}

TEST_F(SaliencePlasticityBridgeTest, NoveltyResponse) {
    salience_plasticity_register_synapse(bridge, 1, SALIENCE_SYNAPSE_NOVELTY, 0, 0.5f);

    int result = salience_plasticity_novelty_response(bridge, 0, 0.9f, true, 1000);
    EXPECT_EQ(result, 0);
}

TEST_F(SaliencePlasticityBridgeTest, Reward) {
    salience_plasticity_register_synapse(bridge, 1, SALIENCE_SYNAPSE_VALUE, 0, 0.5f);
    salience_plasticity_attention_event(bridge, 0, 0.8f, 1000);

    int result = salience_plasticity_reward(bridge, 1.0f, 2000);
    EXPECT_EQ(result, 0);
}

// ============================================================================
// Update Tests
// ============================================================================

TEST_F(SaliencePlasticityBridgeTest, UpdateBasic) {
    int result = salience_plasticity_update(bridge, 10.0f);
    EXPECT_EQ(result, 0);
}

TEST_F(SaliencePlasticityBridgeTest, UpdateDecaysEligibility) {
    salience_plasticity_register_synapse(bridge, 1, SALIENCE_SYNAPSE_NOVELTY, 0, 0.5f);
    salience_plasticity_attention_event(bridge, 0, 0.8f, 1000);

    salience_plasticity_synapse_t synapse_before;
    salience_plasticity_get_synapse(bridge, 1, &synapse_before);
    float elig_before = synapse_before.eligibility_trace;

    salience_plasticity_update(bridge, 100.0f);

    salience_plasticity_synapse_t synapse_after;
    salience_plasticity_get_synapse(bridge, 1, &synapse_after);

    EXPECT_LT(synapse_after.eligibility_trace, elig_before);
}

TEST_F(SaliencePlasticityBridgeTest, Consolidate) {
    salience_plasticity_register_synapse(bridge, 1, SALIENCE_SYNAPSE_VALUE, 0, 0.5f);

    // Create history of attention events
    for (int i = 0; i < 10; i++) {
        salience_plasticity_attention_event(bridge, 0, 0.8f, i * 1000);
        salience_plasticity_attention_feedback(bridge, 0, true, i * 1000 + 500);
    }

    int result = salience_plasticity_consolidate(bridge);
    EXPECT_EQ(result, 0);
}

// ============================================================================
// Query Tests
// ============================================================================

TEST_F(SaliencePlasticityBridgeTest, GetLearnedSalience) {
    salience_plasticity_register_synapse(bridge, 1, SALIENCE_SYNAPSE_NOVELTY, 0, 0.7f);

    float salience = salience_plasticity_get_learned_salience(bridge, 0);
    EXPECT_GE(salience, 0.0f);
}

TEST_F(SaliencePlasticityBridgeTest, GetHabituation) {
    salience_plasticity_feature_exposure(bridge, 0, 0.8f, 1000);

    float habituation = salience_plasticity_get_habituation(bridge, 0);
    EXPECT_GE(habituation, 0.0f);
    EXPECT_LE(habituation, 1.0f);
}

TEST_F(SaliencePlasticityBridgeTest, GetValueEstimate) {
    salience_plasticity_register_synapse(bridge, 1, SALIENCE_SYNAPSE_VALUE, 0, 0.5f);
    salience_plasticity_attention_event(bridge, 0, 0.8f, 1000);
    salience_plasticity_reward(bridge, 1.0f, 2000);

    float value = salience_plasticity_get_value_estimate(bridge, 0);
    EXPECT_GE(value, 0.0f);
}

TEST_F(SaliencePlasticityBridgeTest, GetFeatureLearning) {
    salience_plasticity_feature_exposure(bridge, 0, 0.8f, 1000);
    salience_plasticity_feature_exposure(bridge, 0, 0.7f, 2000);

    salience_feature_learning_t learning;
    int result = salience_plasticity_get_feature_learning(bridge, 0, &learning);
    EXPECT_EQ(result, 0);
    EXPECT_GE(learning.exposure_count, 1u);
}

// ============================================================================
// State and Stats Tests
// ============================================================================

TEST_F(SaliencePlasticityBridgeTest, GetState) {
    salience_plasticity_bridge_state_t state;
    int result = salience_plasticity_get_state(bridge, &state);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(state.state, SALIENCE_PLASTICITY_STATE_IDLE);
}

TEST_F(SaliencePlasticityBridgeTest, GetStats) {
    salience_plasticity_stats_t stats;
    int result = salience_plasticity_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
}

TEST_F(SaliencePlasticityBridgeTest, ResetStats) {
    salience_plasticity_attention_event(bridge, 0, 0.8f, 1000);

    salience_plasticity_reset_stats(bridge);

    salience_plasticity_stats_t stats;
    salience_plasticity_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_attention_events, 0u);
}

// ============================================================================
// Callback Tests
// ============================================================================

static bool weight_callback_called = false;
static void test_weight_callback(uint32_t, uint32_t, float, float, salience_learn_event_t, void*) {
    weight_callback_called = true;
}

TEST_F(SaliencePlasticityBridgeTest, SetWeightCallback) {
    weight_callback_called = false;
    int result = salience_plasticity_set_weight_callback(bridge, test_weight_callback, nullptr);
    EXPECT_EQ(result, 0);

    // Trigger callback
    salience_plasticity_register_synapse(bridge, 1, SALIENCE_SYNAPSE_VALUE, 0, 0.5f);
    salience_plasticity_attention_event(bridge, 0, 0.8f, 1000);
    salience_plasticity_attention_feedback(bridge, 0, true, 2000);

    EXPECT_TRUE(weight_callback_called);
}

static bool habituation_callback_called = false;
static void test_habituation_callback(uint32_t, float, float, void*) {
    habituation_callback_called = true;
}

TEST_F(SaliencePlasticityBridgeTest, SetHabituationCallback) {
    habituation_callback_called = false;
    int result = salience_plasticity_set_habituation_callback(bridge, test_habituation_callback, nullptr);
    EXPECT_EQ(result, 0);

    // Trigger callback
    salience_plasticity_feature_exposure(bridge, 0, 0.8f, 1000);
    salience_plasticity_feature_exposure(bridge, 0, 0.8f, 2000);

    EXPECT_TRUE(habituation_callback_called);
}

// ============================================================================
// Bio-Async Tests
// ============================================================================

TEST_F(SaliencePlasticityBridgeTest, BioAsyncNotConnectedByDefault) {
    bool connected = salience_plasticity_is_bio_async_connected(bridge);
    EXPECT_FALSE(connected);
}

TEST_F(SaliencePlasticityBridgeTest, BioAsyncConnectWithEnable) {
    salience_plasticity_config_t config = salience_plasticity_config_default();
    config.enable_bio_async = true;
    salience_plasticity_bridge_t* b = salience_plasticity_create(&config);
    ASSERT_NE(b, nullptr);

    int result = salience_plasticity_connect_bio_async(b);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(salience_plasticity_is_bio_async_connected(b));

    salience_plasticity_disconnect_bio_async(b);
    EXPECT_FALSE(salience_plasticity_is_bio_async_connected(b));

    salience_plasticity_destroy(b);
}

// ============================================================================
// Learning Scenarios
// ============================================================================

TEST_F(SaliencePlasticityBridgeTest, HabituationOverTime) {
    salience_plasticity_config_t config = salience_plasticity_config_default();
    config.enable_habituation = true;
    salience_plasticity_bridge_t* b = salience_plasticity_create(&config);
    ASSERT_NE(b, nullptr);

    // Repeated exposure should increase habituation
    for (int i = 0; i < 10; i++) {
        salience_plasticity_feature_exposure(b, 0, 0.8f, i * 1000);
        salience_plasticity_update(b, 10.0f);
    }

    float habituation = salience_plasticity_get_habituation(b, 0);
    EXPECT_GT(habituation, 0.0f);

    salience_plasticity_destroy(b);
}

TEST_F(SaliencePlasticityBridgeTest, ValueLearningWithReward) {
    salience_plasticity_config_t config = salience_plasticity_config_default();
    config.enable_value_learning = true;
    salience_plasticity_bridge_t* b = salience_plasticity_create(&config);
    ASSERT_NE(b, nullptr);

    salience_plasticity_register_synapse(b, 1, SALIENCE_SYNAPSE_VALUE, 0, 0.5f);

    // Attend and reward
    for (int i = 0; i < 5; i++) {
        salience_plasticity_attention_event(b, 0, 0.8f, i * 2000);
        salience_plasticity_reward(b, 1.0f, i * 2000 + 1000);
        salience_plasticity_update(b, 10.0f);
    }

    float value = salience_plasticity_get_value_estimate(b, 0);
    EXPECT_GT(value, 0.0f);

    salience_plasticity_destroy(b);
}

TEST_F(SaliencePlasticityBridgeTest, NoveltySeekingBehavior) {
    salience_plasticity_config_t config = salience_plasticity_config_default();
    config.enable_novelty_seeking = true;
    salience_plasticity_bridge_t* b = salience_plasticity_create(&config);
    ASSERT_NE(b, nullptr);

    salience_plasticity_register_synapse(b, 1, SALIENCE_SYNAPSE_NOVELTY, 0, 0.5f);

    // Rewarded novelty response
    salience_plasticity_novelty_response(b, 0, 0.9f, true, 1000);
    salience_plasticity_update(b, 10.0f);

    salience_plasticity_synapse_t synapse;
    salience_plasticity_get_synapse(b, 1, &synapse);

    EXPECT_GT(synapse.weight, 0.5f);  // Should increase with rewarded novelty

    salience_plasticity_destroy(b);
}

TEST_F(SaliencePlasticityBridgeTest, AttentionLearningCorrectFeedback) {
    salience_plasticity_register_synapse(bridge, 1, SALIENCE_SYNAPSE_VALUE, 0, 0.5f);

    // Multiple correct attention events
    for (int i = 0; i < 10; i++) {
        salience_plasticity_attention_event(bridge, 0, 0.8f, i * 2000);
        salience_plasticity_attention_feedback(bridge, 0, true, i * 2000 + 1000);
        salience_plasticity_update(bridge, 10.0f);
    }

    salience_plasticity_synapse_t synapse;
    salience_plasticity_get_synapse(bridge, 1, &synapse);

    EXPECT_GT(synapse.weight, 0.5f);  // Weight should increase with correct attention
}

TEST_F(SaliencePlasticityBridgeTest, AttentionLearningIncorrectFeedback) {
    salience_plasticity_register_synapse(bridge, 1, SALIENCE_SYNAPSE_VALUE, 0, 0.8f);

    // Multiple incorrect attention events
    for (int i = 0; i < 10; i++) {
        salience_plasticity_attention_event(bridge, 0, 0.8f, i * 2000);
        salience_plasticity_attention_feedback(bridge, 0, false, i * 2000 + 1000);
        salience_plasticity_update(bridge, 10.0f);
    }

    salience_plasticity_synapse_t synapse;
    salience_plasticity_get_synapse(bridge, 1, &synapse);

    EXPECT_LT(synapse.weight, 0.8f);  // Weight should decrease with incorrect attention
}

TEST_F(SaliencePlasticityBridgeTest, RewardModulatedLearning) {
    salience_plasticity_register_synapse(bridge, 1, SALIENCE_SYNAPSE_VALUE, 0, 0.5f);

    // Create eligibility and reward
    salience_plasticity_attention_event(bridge, 0, 0.9f, 1000);
    salience_plasticity_reward(bridge, 1.0f, 2000);

    salience_plasticity_synapse_t synapse;
    salience_plasticity_get_synapse(bridge, 1, &synapse);

    EXPECT_GT(synapse.weight, 0.5f);  // Weight should increase with positive reward
}
