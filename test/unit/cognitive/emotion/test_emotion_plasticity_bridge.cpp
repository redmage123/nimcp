/**
 * @file test_emotion_plasticity_bridge.cpp
 * @brief Unit tests for Emotion System - Plasticity Bridge integration
 * @date 2026-01-06
 *
 * Tests bidirectional integration between emotion system and plasticity mechanisms:
 * - Emotion --> Plasticity: Stimulus/response events trigger STDP learning
 * - Plasticity --> Emotion: Weight updates modulate emotional sensitivity
 * - Extinction learning
 * - Reward-modulated plasticity
 */

#include <gtest/gtest.h>

// Note: Header has its own extern "C" guards
#include "cognitive/emotion/nimcp_emotion_plasticity_bridge.h"

#include "utils/time/nimcp_time.h"

#include <cmath>
#include <cstring>
#include <vector>

//=============================================================================
// Test Fixtures
//=============================================================================

class EmotionPlasticityBridgeTest : public ::testing::Test {
protected:
    emotion_plasticity_bridge_t* bridge = nullptr;

    void SetUp() override {
        emotion_plasticity_config_t config = emotion_plasticity_config_default();
        config.enable_bio_async = false;  /* Disable for unit tests */
        config.enable_immune_modulation = false;
        config.enable_sleep_consolidation = false;
        bridge = emotion_plasticity_create(&config);
        ASSERT_NE(bridge, nullptr) << "Failed to create emotion-plasticity bridge";
    }

    void TearDown() override {
        if (bridge) {
            emotion_plasticity_destroy(bridge);
            bridge = nullptr;
        }
    }

    /* Helper to register multiple synapses for an emotion */
    void register_synapses_for_emotion(emotion_category_t emotion, uint32_t count, float init_weight) {
        for (uint32_t i = 0; i < count; i++) {
            uint32_t synapse_id = emotion * 1000 + i;  // Unique ID per emotion
            emotion_plasticity_register_synapse(bridge, synapse_id,
                EMOTION_SYNAPSE_STIMULUS_TO_EMOTION, emotion, init_weight);
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(EmotionPlasticityBridgeTest, CreateDestroy) {
    EXPECT_NE(bridge, nullptr);
}

TEST_F(EmotionPlasticityBridgeTest, CreateWithDefaultConfig) {
    emotion_plasticity_bridge_t* b = emotion_plasticity_create(nullptr);
    ASSERT_NE(b, nullptr);
    emotion_plasticity_destroy(b);
}

TEST_F(EmotionPlasticityBridgeTest, DestroyNull) {
    /* Should not crash */
    emotion_plasticity_destroy(nullptr);
}

TEST_F(EmotionPlasticityBridgeTest, ResetBridge) {
    register_synapses_for_emotion(EMOTION_FEAR, 3, 0.7f);

    int ret = emotion_plasticity_reset(bridge);
    EXPECT_EQ(ret, 0) << "Reset should succeed";

    /* After reset, weights should be back to initial values */
    emotion_plasticity_synapse_t synapse;
    ret = emotion_plasticity_get_synapse(bridge, EMOTION_FEAR * 1000, &synapse);
    EXPECT_EQ(ret, 0);
    EXPECT_FLOAT_EQ(synapse.weight, synapse.initial_weight);
}

TEST_F(EmotionPlasticityBridgeTest, ResetNull) {
    EXPECT_EQ(emotion_plasticity_reset(nullptr), -1);
}

//=============================================================================
// Default Configuration Tests
//=============================================================================

TEST_F(EmotionPlasticityBridgeTest, DefaultConfigValues) {
    emotion_plasticity_config_t config = emotion_plasticity_config_default();

    /* STDP parameters */
    EXPECT_GT(config.stdp_ltp_window_ms, 0.0f);
    EXPECT_GT(config.stdp_ltd_window_ms, 0.0f);
    EXPECT_GT(config.stdp_a_plus, 0.0f);
    EXPECT_GT(config.stdp_a_minus, 0.0f);
    EXPECT_GT(config.stdp_tau_plus, 0.0f);
    EXPECT_GT(config.stdp_tau_minus, 0.0f);

    /* Valence modulation */
    EXPECT_TRUE(config.enable_valence_modulation);
    EXPECT_GT(config.positive_valence_ltp_boost, 0.0f);
    EXPECT_GT(config.negative_valence_ltd_boost, 0.0f);

    /* Arousal modulation */
    EXPECT_TRUE(config.enable_arousal_modulation);
    EXPECT_GT(config.arousal_learning_gain, 0.0f);

    /* BCM */
    EXPECT_TRUE(config.enable_bcm);
    EXPECT_GT(config.bcm_threshold_tau, 0.0f);
    EXPECT_GT(config.bcm_activity_tau, 0.0f);

    /* Homeostatic */
    EXPECT_TRUE(config.enable_homeostatic);
    EXPECT_GT(config.target_response_rate, 0.0f);
    EXPECT_GT(config.homeostatic_tau_ms, 0.0f);

    /* Eligibility */
    EXPECT_TRUE(config.enable_eligibility);
    EXPECT_GT(config.eligibility_decay, 0.0f);
    EXPECT_GT(config.reward_modulation_gain, 0.0f);

    /* Weight bounds */
    EXPECT_GE(config.weight_min, 0.0f);
    EXPECT_GT(config.weight_max, config.weight_min);
    EXPECT_GE(config.initial_weight, config.weight_min);
    EXPECT_LE(config.initial_weight, config.weight_max);

    /* Extinction */
    EXPECT_TRUE(config.enable_extinction);
    EXPECT_GT(config.extinction_rate, 0.0f);
    EXPECT_GT(config.spontaneous_recovery_tau, 0.0f);
}

//=============================================================================
// Synapse Management Tests
//=============================================================================

TEST_F(EmotionPlasticityBridgeTest, RegisterSynapse) {
    int ret = emotion_plasticity_register_synapse(
        bridge, 1, EMOTION_SYNAPSE_STIMULUS_TO_EMOTION, EMOTION_FEAR, 0.5f);

    EXPECT_EQ(ret, 0) << "Should register synapse successfully";
}

TEST_F(EmotionPlasticityBridgeTest, RegisterSynapseAlreadyExists) {
    emotion_plasticity_register_synapse(
        bridge, 1, EMOTION_SYNAPSE_STIMULUS_TO_EMOTION, EMOTION_FEAR, 0.5f);

    /* Registering same synapse_id again should return 0 (already registered) */
    int ret = emotion_plasticity_register_synapse(
        bridge, 1, EMOTION_SYNAPSE_STIMULUS_TO_EMOTION, EMOTION_HAPPINESS, 0.7f);

    EXPECT_EQ(ret, 0) << "Should return 0 for already registered synapse";
}

TEST_F(EmotionPlasticityBridgeTest, RegisterSynapseInvalidEmotion) {
    int ret = emotion_plasticity_register_synapse(
        bridge, 1, EMOTION_SYNAPSE_STIMULUS_TO_EMOTION, EMOTION_COUNT, 0.5f);

    EXPECT_EQ(ret, -1) << "Should fail for invalid emotion category";
}

TEST_F(EmotionPlasticityBridgeTest, RegisterSynapseNullBridge) {
    int ret = emotion_plasticity_register_synapse(
        nullptr, 1, EMOTION_SYNAPSE_STIMULUS_TO_EMOTION, EMOTION_FEAR, 0.5f);

    EXPECT_EQ(ret, -1);
}

TEST_F(EmotionPlasticityBridgeTest, RegisterMultipleSynapseTypes) {
    int ret1 = emotion_plasticity_register_synapse(
        bridge, 1, EMOTION_SYNAPSE_STIMULUS_TO_EMOTION, EMOTION_FEAR, 0.5f);
    int ret2 = emotion_plasticity_register_synapse(
        bridge, 2, EMOTION_SYNAPSE_EMOTION_TO_RESPONSE, EMOTION_FEAR, 0.6f);
    int ret3 = emotion_plasticity_register_synapse(
        bridge, 3, EMOTION_SYNAPSE_CONTEXT_TO_EMOTION, EMOTION_HAPPINESS, 0.7f);
    int ret4 = emotion_plasticity_register_synapse(
        bridge, 4, EMOTION_SYNAPSE_INTERHEMISPHERIC, EMOTION_ANGER, 0.4f);

    EXPECT_EQ(ret1, 0);
    EXPECT_EQ(ret2, 0);
    EXPECT_EQ(ret3, 0);
    EXPECT_EQ(ret4, 0);
}

TEST_F(EmotionPlasticityBridgeTest, UnregisterSynapse) {
    emotion_plasticity_register_synapse(
        bridge, 1, EMOTION_SYNAPSE_STIMULUS_TO_EMOTION, EMOTION_FEAR, 0.5f);

    int ret = emotion_plasticity_unregister_synapse(bridge, 1);
    EXPECT_EQ(ret, 0) << "Should unregister synapse";

    /* Try to unregister again - should fail */
    ret = emotion_plasticity_unregister_synapse(bridge, 1);
    EXPECT_EQ(ret, -1) << "Should fail for non-existent synapse";
}

TEST_F(EmotionPlasticityBridgeTest, UnregisterSynapseNullBridge) {
    EXPECT_EQ(emotion_plasticity_unregister_synapse(nullptr, 1), -1);
}

TEST_F(EmotionPlasticityBridgeTest, GetSynapseState) {
    emotion_plasticity_register_synapse(
        bridge, 42, EMOTION_SYNAPSE_STIMULUS_TO_EMOTION, EMOTION_SADNESS, 0.7f);

    emotion_plasticity_synapse_t state;
    int ret = emotion_plasticity_get_synapse(bridge, 42, &state);

    EXPECT_EQ(ret, 0) << "Should get synapse state";
    EXPECT_EQ(state.synapse_id, 42u);
    EXPECT_EQ(state.type, EMOTION_SYNAPSE_STIMULUS_TO_EMOTION);
    EXPECT_EQ(state.associated_emotion, EMOTION_SADNESS);
    EXPECT_FLOAT_EQ(state.weight, 0.7f);
    EXPECT_FLOAT_EQ(state.initial_weight, 0.7f);
    EXPECT_FLOAT_EQ(state.eligibility_trace, 0.0f);
    EXPECT_FLOAT_EQ(state.extinction_level, 0.0f);
}

TEST_F(EmotionPlasticityBridgeTest, GetSynapseNotFound) {
    emotion_plasticity_synapse_t state;
    int ret = emotion_plasticity_get_synapse(bridge, 99999, &state);

    EXPECT_EQ(ret, -1) << "Should fail for non-existent synapse";
}

TEST_F(EmotionPlasticityBridgeTest, GetSynapseNullParams) {
    emotion_plasticity_register_synapse(
        bridge, 1, EMOTION_SYNAPSE_STIMULUS_TO_EMOTION, EMOTION_FEAR, 0.5f);

    emotion_plasticity_synapse_t state;
    EXPECT_EQ(emotion_plasticity_get_synapse(nullptr, 1, &state), -1);
    EXPECT_EQ(emotion_plasticity_get_synapse(bridge, 1, nullptr), -1);
}

//=============================================================================
// STDP Weight Updates Tests
//=============================================================================

TEST_F(EmotionPlasticityBridgeTest, StimulusRecording) {
    register_synapses_for_emotion(EMOTION_FEAR, 3, 0.5f);

    int ret = emotion_plasticity_stimulus(bridge, EMOTION_FEAR, 1.0f, 1000000);
    EXPECT_EQ(ret, 0) << "Stimulus should succeed";
}

TEST_F(EmotionPlasticityBridgeTest, StimulusNullBridge) {
    EXPECT_EQ(emotion_plasticity_stimulus(nullptr, EMOTION_FEAR, 1.0f, 1000000), -1);
}

TEST_F(EmotionPlasticityBridgeTest, StimulusInvalidEmotion) {
    EXPECT_EQ(emotion_plasticity_stimulus(bridge, EMOTION_COUNT, 1.0f, 1000000), -1);
}

TEST_F(EmotionPlasticityBridgeTest, ResponseRecording) {
    register_synapses_for_emotion(EMOTION_FEAR, 3, 0.5f);

    int ret = emotion_plasticity_response(bridge, EMOTION_FEAR, 0.8f, 1000000);
    EXPECT_EQ(ret, 0) << "Response should succeed";
}

TEST_F(EmotionPlasticityBridgeTest, ResponseNullBridge) {
    EXPECT_EQ(emotion_plasticity_response(nullptr, EMOTION_FEAR, 0.8f, 1000000), -1);
}

TEST_F(EmotionPlasticityBridgeTest, ResponseInvalidEmotion) {
    EXPECT_EQ(emotion_plasticity_response(bridge, EMOTION_COUNT, 0.8f, 1000000), -1);
}

TEST_F(EmotionPlasticityBridgeTest, STDPLTPStimulusBeforeResponse) {
    emotion_plasticity_register_synapse(
        bridge, 1, EMOTION_SYNAPSE_STIMULUS_TO_EMOTION, EMOTION_FEAR, 0.5f);

    /* Stimulus (pre-spike) at t=1s */
    emotion_plasticity_stimulus(bridge, EMOTION_FEAR, 1.0f, 1000000);

    /* Response (post-spike) at t=1.01s (10ms later) - should trigger LTP */
    emotion_plasticity_response(bridge, EMOTION_FEAR, 1.0f, 1010000);

    emotion_plasticity_synapse_t state;
    emotion_plasticity_get_synapse(bridge, 1, &state);

    EXPECT_GT(state.weight, 0.5f) << "Pre before post should cause LTP (weight increase)";
}

TEST_F(EmotionPlasticityBridgeTest, STDPLTDResponseBeforeStimulus) {
    emotion_plasticity_register_synapse(
        bridge, 1, EMOTION_SYNAPSE_STIMULUS_TO_EMOTION, EMOTION_FEAR, 0.5f);

    /* Response (post-spike) at t=1s */
    emotion_plasticity_response(bridge, EMOTION_FEAR, 1.0f, 1000000);

    /* Stimulus (pre-spike) at t=1.01s (10ms later) - should trigger LTD */
    emotion_plasticity_stimulus(bridge, EMOTION_FEAR, 1.0f, 1010000);

    emotion_plasticity_synapse_t state;
    emotion_plasticity_get_synapse(bridge, 1, &state);

    EXPECT_LT(state.weight, 0.5f) << "Post before pre should cause LTD (weight decrease)";
}

TEST_F(EmotionPlasticityBridgeTest, STDPWindowExpiry) {
    emotion_plasticity_register_synapse(
        bridge, 1, EMOTION_SYNAPSE_STIMULUS_TO_EMOTION, EMOTION_FEAR, 0.5f);

    /* Stimulus at t=0 */
    emotion_plasticity_stimulus(bridge, EMOTION_FEAR, 1.0f, 0);

    /* Response at t=100ms (beyond typical STDP window) */
    emotion_plasticity_response(bridge, EMOTION_FEAR, 1.0f, 100000);

    emotion_plasticity_synapse_t state;
    emotion_plasticity_get_synapse(bridge, 1, &state);

    /* Weight should not change significantly (may still be 0.5 or very close) */
    EXPECT_NEAR(state.weight, 0.5f, 0.01f) << "Spikes beyond window should not cause weight change";
}

//=============================================================================
// Extinction Learning Tests
//=============================================================================

TEST_F(EmotionPlasticityBridgeTest, ExtinctionTrial) {
    emotion_plasticity_register_synapse(
        bridge, 1, EMOTION_SYNAPSE_STIMULUS_TO_EMOTION, EMOTION_FEAR, 0.8f);

    /* Multiple extinction trials */
    for (int i = 0; i < 5; i++) {
        int ret = emotion_plasticity_extinction_trial(bridge, EMOTION_FEAR, i * 1000000);
        EXPECT_EQ(ret, 0) << "Extinction trial should succeed";
    }

    /* Check that extinction level increased */
    float extinction_level = emotion_plasticity_get_extinction_level(bridge, EMOTION_FEAR);
    EXPECT_GT(extinction_level, 0.0f) << "Extinction level should increase after trials";

    /* Check that weight decreased */
    emotion_plasticity_synapse_t state;
    emotion_plasticity_get_synapse(bridge, 1, &state);
    EXPECT_LT(state.weight, 0.8f) << "Weight should decrease due to extinction";
}

TEST_F(EmotionPlasticityBridgeTest, ExtinctionTrialNullBridge) {
    EXPECT_EQ(emotion_plasticity_extinction_trial(nullptr, EMOTION_FEAR, 1000000), -1);
}

TEST_F(EmotionPlasticityBridgeTest, ExtinctionTrialInvalidEmotion) {
    EXPECT_EQ(emotion_plasticity_extinction_trial(bridge, EMOTION_COUNT, 1000000), -1);
}

TEST_F(EmotionPlasticityBridgeTest, GetExtinctionLevel) {
    register_synapses_for_emotion(EMOTION_FEAR, 1, 0.8f);

    /* Initial extinction level should be 0 */
    float level = emotion_plasticity_get_extinction_level(bridge, EMOTION_FEAR);
    EXPECT_FLOAT_EQ(level, 0.0f);

    /* After extinction trial */
    emotion_plasticity_extinction_trial(bridge, EMOTION_FEAR, 1000000);
    level = emotion_plasticity_get_extinction_level(bridge, EMOTION_FEAR);
    EXPECT_GT(level, 0.0f);
}

TEST_F(EmotionPlasticityBridgeTest, GetExtinctionLevelNullBridge) {
    float level = emotion_plasticity_get_extinction_level(nullptr, EMOTION_FEAR);
    EXPECT_FLOAT_EQ(level, 0.0f);
}

TEST_F(EmotionPlasticityBridgeTest, GetExtinctionLevelInvalidEmotion) {
    float level = emotion_plasticity_get_extinction_level(bridge, EMOTION_COUNT);
    EXPECT_FLOAT_EQ(level, 0.0f);
}

//=============================================================================
// Reward Modulation Tests
//=============================================================================

TEST_F(EmotionPlasticityBridgeTest, RewardPositive) {
    register_synapses_for_emotion(EMOTION_HAPPINESS, 3, 0.5f);

    /* Create activity to build eligibility traces */
    emotion_plasticity_stimulus(bridge, EMOTION_HAPPINESS, 1.0f, 1000000);

    /* Apply positive reward */
    int ret = emotion_plasticity_reward(bridge, 0.5f, 1020000);
    EXPECT_EQ(ret, 0) << "Positive reward should succeed";
}

TEST_F(EmotionPlasticityBridgeTest, RewardNegative) {
    register_synapses_for_emotion(EMOTION_FEAR, 3, 0.5f);

    emotion_plasticity_stimulus(bridge, EMOTION_FEAR, 1.0f, 1000000);

    /* Apply negative reward (punishment) */
    int ret = emotion_plasticity_reward(bridge, -0.5f, 1020000);
    EXPECT_EQ(ret, 0) << "Negative reward should succeed";
}

TEST_F(EmotionPlasticityBridgeTest, RewardNullBridge) {
    EXPECT_EQ(emotion_plasticity_reward(nullptr, 0.5f, 1000000), -1);
}

TEST_F(EmotionPlasticityBridgeTest, RewardWithEligibilityTrace) {
    emotion_plasticity_register_synapse(
        bridge, 1, EMOTION_SYNAPSE_STIMULUS_TO_EMOTION, EMOTION_HAPPINESS, 0.5f);

    /* Stimulus sets eligibility trace */
    emotion_plasticity_stimulus(bridge, EMOTION_HAPPINESS, 1.0f, 1000000);

    /* Record initial weight */
    emotion_plasticity_synapse_t state_before;
    emotion_plasticity_get_synapse(bridge, 1, &state_before);

    /* Apply reward - should modulate based on eligibility trace */
    emotion_plasticity_reward(bridge, 1.0f, 1010000);

    emotion_plasticity_synapse_t state_after;
    emotion_plasticity_get_synapse(bridge, 1, &state_after);

    /* If eligibility trace was active, weight should change */
    /* Note: The actual weight change depends on eligibility_trace value set by stimulus */
}

//=============================================================================
// Update Function Tests
//=============================================================================

TEST_F(EmotionPlasticityBridgeTest, Update) {
    register_synapses_for_emotion(EMOTION_FEAR, 5, 0.5f);

    int ret = emotion_plasticity_update(bridge, 10.0f);
    EXPECT_EQ(ret, 0) << "Update should succeed";
}

TEST_F(EmotionPlasticityBridgeTest, UpdateNullBridge) {
    EXPECT_EQ(emotion_plasticity_update(nullptr, 10.0f), -1);
}

TEST_F(EmotionPlasticityBridgeTest, UpdateMultipleTimesteps) {
    register_synapses_for_emotion(EMOTION_FEAR, 3, 0.5f);

    /* Run multiple update cycles */
    for (int i = 0; i < 100; i++) {
        int ret = emotion_plasticity_update(bridge, 1.0f);
        EXPECT_EQ(ret, 0);
    }
}

TEST_F(EmotionPlasticityBridgeTest, Consolidate) {
    register_synapses_for_emotion(EMOTION_FEAR, 3, 0.3f);
    register_synapses_for_emotion(EMOTION_HAPPINESS, 3, 0.7f);

    int ret = emotion_plasticity_consolidate(bridge);
    EXPECT_EQ(ret, 0) << "Consolidation should succeed";
}

TEST_F(EmotionPlasticityBridgeTest, ConsolidateNullBridge) {
    EXPECT_EQ(emotion_plasticity_consolidate(nullptr), -1);
}

TEST_F(EmotionPlasticityBridgeTest, ConsolidateStrengthensStrong) {
    /* Register synapse with weight above midpoint */
    emotion_plasticity_register_synapse(
        bridge, 1, EMOTION_SYNAPSE_STIMULUS_TO_EMOTION, EMOTION_HAPPINESS, 0.7f);

    emotion_plasticity_synapse_t state_before;
    emotion_plasticity_get_synapse(bridge, 1, &state_before);

    emotion_plasticity_consolidate(bridge);

    emotion_plasticity_synapse_t state_after;
    emotion_plasticity_get_synapse(bridge, 1, &state_after);

    EXPECT_GT(state_after.weight, state_before.weight)
        << "Consolidation should strengthen strong synapses";
}

TEST_F(EmotionPlasticityBridgeTest, ConsolidateWeakensWeak) {
    /* Register synapse with weight below midpoint */
    emotion_plasticity_register_synapse(
        bridge, 1, EMOTION_SYNAPSE_STIMULUS_TO_EMOTION, EMOTION_FEAR, 0.3f);

    emotion_plasticity_synapse_t state_before;
    emotion_plasticity_get_synapse(bridge, 1, &state_before);

    emotion_plasticity_consolidate(bridge);

    emotion_plasticity_synapse_t state_after;
    emotion_plasticity_get_synapse(bridge, 1, &state_after);

    EXPECT_LT(state_after.weight, state_before.weight)
        << "Consolidation should weaken weak synapses";
}

//=============================================================================
// Query Functions Tests
//=============================================================================

TEST_F(EmotionPlasticityBridgeTest, GetResponseModulation) {
    register_synapses_for_emotion(EMOTION_FEAR, 3, 0.7f);

    float modulation;
    int ret = emotion_plasticity_get_response_modulation(bridge, EMOTION_FEAR, &modulation);

    EXPECT_EQ(ret, 0) << "Should get response modulation";
    EXPECT_GT(modulation, 0.0f);
}

TEST_F(EmotionPlasticityBridgeTest, GetResponseModulationNullParams) {
    float modulation;
    EXPECT_EQ(emotion_plasticity_get_response_modulation(nullptr, EMOTION_FEAR, &modulation), -1);
    EXPECT_EQ(emotion_plasticity_get_response_modulation(bridge, EMOTION_FEAR, nullptr), -1);
}

TEST_F(EmotionPlasticityBridgeTest, GetResponseModulationInvalidEmotion) {
    float modulation;
    EXPECT_EQ(emotion_plasticity_get_response_modulation(bridge, EMOTION_COUNT, &modulation), -1);
}

TEST_F(EmotionPlasticityBridgeTest, GetSensitivity) {
    register_synapses_for_emotion(EMOTION_HAPPINESS, 3, 0.6f);

    /* Run an update to compute sensitivity from synapse weights */
    emotion_plasticity_update(bridge, 10.0f);

    float sensitivity = emotion_plasticity_get_sensitivity(bridge, EMOTION_HAPPINESS);
    EXPECT_GT(sensitivity, 0.0f);
}

TEST_F(EmotionPlasticityBridgeTest, GetSensitivityNullBridge) {
    float sensitivity = emotion_plasticity_get_sensitivity(nullptr, EMOTION_FEAR);
    EXPECT_FLOAT_EQ(sensitivity, 1.0f);  /* Default value */
}

TEST_F(EmotionPlasticityBridgeTest, GetSensitivityInvalidEmotion) {
    float sensitivity = emotion_plasticity_get_sensitivity(bridge, EMOTION_COUNT);
    EXPECT_FLOAT_EQ(sensitivity, 1.0f);  /* Default value */
}

//=============================================================================
// State and Statistics Tests
//=============================================================================

TEST_F(EmotionPlasticityBridgeTest, GetState) {
    register_synapses_for_emotion(EMOTION_FEAR, 5, 0.5f);

    emotion_plasticity_bridge_state_t state;
    int ret = emotion_plasticity_get_state(bridge, &state);

    EXPECT_EQ(ret, 0) << "Should get state";
    EXPECT_EQ(state.state, EMOTION_PLASTICITY_STATE_IDLE);
    EXPECT_EQ(state.registered_synapses, 5u);
    EXPECT_FLOAT_EQ(state.global_learning_rate, 1.0f);
    EXPECT_FALSE(state.bio_async_connected);
}

TEST_F(EmotionPlasticityBridgeTest, GetStateNullParams) {
    emotion_plasticity_bridge_state_t state;
    EXPECT_EQ(emotion_plasticity_get_state(nullptr, &state), -1);
    EXPECT_EQ(emotion_plasticity_get_state(bridge, nullptr), -1);
}

TEST_F(EmotionPlasticityBridgeTest, GetStats) {
    emotion_plasticity_stats_t stats;
    int ret = emotion_plasticity_get_stats(bridge, &stats);

    EXPECT_EQ(ret, 0) << "Should get stats";
    EXPECT_EQ(stats.total_observations, 0u);
    EXPECT_EQ(stats.total_responses, 0u);
    EXPECT_EQ(stats.ltp_events, 0u);
    EXPECT_EQ(stats.ltd_events, 0u);
}

TEST_F(EmotionPlasticityBridgeTest, GetStatsNullParams) {
    emotion_plasticity_stats_t stats;
    EXPECT_EQ(emotion_plasticity_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(emotion_plasticity_get_stats(bridge, nullptr), -1);
}

TEST_F(EmotionPlasticityBridgeTest, StatsTracking) {
    register_synapses_for_emotion(EMOTION_FEAR, 1, 0.5f);

    /* Generate activity */
    emotion_plasticity_stimulus(bridge, EMOTION_FEAR, 1.0f, 1000000);
    emotion_plasticity_response(bridge, EMOTION_FEAR, 1.0f, 1010000);

    emotion_plasticity_stats_t stats;
    emotion_plasticity_get_stats(bridge, &stats);

    EXPECT_EQ(stats.total_observations, 1u);
    EXPECT_EQ(stats.total_responses, 1u);
    EXPECT_EQ(stats.total_pre_spikes, 1u);
    EXPECT_EQ(stats.total_post_spikes, 1u);
}

TEST_F(EmotionPlasticityBridgeTest, ResetStats) {
    register_synapses_for_emotion(EMOTION_FEAR, 1, 0.5f);

    /* Generate activity */
    emotion_plasticity_stimulus(bridge, EMOTION_FEAR, 1.0f, 1000000);
    emotion_plasticity_response(bridge, EMOTION_FEAR, 1.0f, 1010000);

    emotion_plasticity_reset_stats(bridge);

    emotion_plasticity_stats_t stats;
    emotion_plasticity_get_stats(bridge, &stats);

    EXPECT_EQ(stats.total_observations, 0u);
    EXPECT_EQ(stats.total_responses, 0u);
    EXPECT_EQ(stats.total_pre_spikes, 0u);
    EXPECT_EQ(stats.total_post_spikes, 0u);
}

TEST_F(EmotionPlasticityBridgeTest, ResetStatsNull) {
    /* Should not crash */
    emotion_plasticity_reset_stats(nullptr);
}

//=============================================================================
// Callback Tests
//=============================================================================

static int g_weight_callback_count = 0;
static float g_last_old_weight = 0.0f;
static float g_last_new_weight = 0.0f;
static emotion_learn_event_t g_last_event_type = EMOTION_LEARN_CONDITIONING;

static void test_weight_callback(uint32_t synapse_id,
                                  emotion_category_t emotion,
                                  float old_weight,
                                  float new_weight,
                                  emotion_learn_event_t event_type,
                                  void* user_data) {
    g_weight_callback_count++;
    g_last_old_weight = old_weight;
    g_last_new_weight = new_weight;
    g_last_event_type = event_type;
}

TEST_F(EmotionPlasticityBridgeTest, SetWeightCallback) {
    g_weight_callback_count = 0;

    int ret = emotion_plasticity_set_weight_callback(bridge, test_weight_callback, nullptr);
    EXPECT_EQ(ret, 0) << "Should set callback";
}

TEST_F(EmotionPlasticityBridgeTest, SetWeightCallbackNullBridge) {
    EXPECT_EQ(emotion_plasticity_set_weight_callback(nullptr, test_weight_callback, nullptr), -1);
}

TEST_F(EmotionPlasticityBridgeTest, WeightCallbackTriggered) {
    g_weight_callback_count = 0;
    g_last_old_weight = 0.0f;
    g_last_new_weight = 0.0f;

    emotion_plasticity_set_weight_callback(bridge, test_weight_callback, nullptr);

    emotion_plasticity_register_synapse(
        bridge, 1, EMOTION_SYNAPSE_STIMULUS_TO_EMOTION, EMOTION_FEAR, 0.5f);

    /* Create LTP event */
    emotion_plasticity_stimulus(bridge, EMOTION_FEAR, 1.0f, 1000000);
    emotion_plasticity_response(bridge, EMOTION_FEAR, 1.0f, 1010000);

    EXPECT_GT(g_weight_callback_count, 0) << "Callback should be triggered";
    EXPECT_GT(g_last_new_weight, g_last_old_weight) << "LTP should increase weight";
    EXPECT_EQ(g_last_event_type, EMOTION_LEARN_CONDITIONING);
}

TEST_F(EmotionPlasticityBridgeTest, WeightCallbackOnExtinction) {
    g_weight_callback_count = 0;

    emotion_plasticity_set_weight_callback(bridge, test_weight_callback, nullptr);

    emotion_plasticity_register_synapse(
        bridge, 1, EMOTION_SYNAPSE_STIMULUS_TO_EMOTION, EMOTION_FEAR, 0.8f);

    emotion_plasticity_extinction_trial(bridge, EMOTION_FEAR, 1000000);

    EXPECT_GT(g_weight_callback_count, 0) << "Callback should be triggered on extinction";
    EXPECT_EQ(g_last_event_type, EMOTION_LEARN_EXTINCTION);
}

//=============================================================================
// Modulation Function Tests
//=============================================================================

TEST_F(EmotionPlasticityBridgeTest, SetValenceModulation) {
    int ret = emotion_plasticity_set_valence_modulation(bridge, 0.5f);
    EXPECT_EQ(ret, 0);

    emotion_plasticity_bridge_state_t state;
    emotion_plasticity_get_state(bridge, &state);
    EXPECT_FLOAT_EQ(state.current_valence_mod, 0.5f);
}

TEST_F(EmotionPlasticityBridgeTest, SetValenceModulationClamped) {
    /* Test clamping to [-1, 1] */
    emotion_plasticity_set_valence_modulation(bridge, 2.0f);

    emotion_plasticity_bridge_state_t state;
    emotion_plasticity_get_state(bridge, &state);
    EXPECT_FLOAT_EQ(state.current_valence_mod, 1.0f);

    emotion_plasticity_set_valence_modulation(bridge, -2.0f);
    emotion_plasticity_get_state(bridge, &state);
    EXPECT_FLOAT_EQ(state.current_valence_mod, -1.0f);
}

TEST_F(EmotionPlasticityBridgeTest, SetValenceModulationNull) {
    EXPECT_EQ(emotion_plasticity_set_valence_modulation(nullptr, 0.5f), -1);
}

TEST_F(EmotionPlasticityBridgeTest, SetArousalModulation) {
    int ret = emotion_plasticity_set_arousal_modulation(bridge, 0.8f);
    EXPECT_EQ(ret, 0);

    emotion_plasticity_bridge_state_t state;
    emotion_plasticity_get_state(bridge, &state);
    EXPECT_FLOAT_EQ(state.current_arousal_mod, 0.8f);
}

TEST_F(EmotionPlasticityBridgeTest, SetArousalModulationClamped) {
    /* Test clamping to [0, 1] */
    emotion_plasticity_set_arousal_modulation(bridge, 2.0f);

    emotion_plasticity_bridge_state_t state;
    emotion_plasticity_get_state(bridge, &state);
    EXPECT_FLOAT_EQ(state.current_arousal_mod, 1.0f);

    emotion_plasticity_set_arousal_modulation(bridge, -0.5f);
    emotion_plasticity_get_state(bridge, &state);
    EXPECT_FLOAT_EQ(state.current_arousal_mod, 0.0f);
}

TEST_F(EmotionPlasticityBridgeTest, SetArousalModulationNull) {
    EXPECT_EQ(emotion_plasticity_set_arousal_modulation(nullptr, 0.5f), -1);
}

TEST_F(EmotionPlasticityBridgeTest, ValenceAffectsLTP) {
    /* Test that positive valence boosts LTP */
    emotion_plasticity_register_synapse(
        bridge, 1, EMOTION_SYNAPSE_STIMULUS_TO_EMOTION, EMOTION_HAPPINESS, 0.5f);

    emotion_plasticity_set_valence_modulation(bridge, 0.8f);

    emotion_plasticity_stimulus(bridge, EMOTION_HAPPINESS, 1.0f, 1000000);
    emotion_plasticity_response(bridge, EMOTION_HAPPINESS, 1.0f, 1010000);

    emotion_plasticity_synapse_t state;
    emotion_plasticity_get_synapse(bridge, 1, &state);

    /* With positive valence, LTP should be boosted */
    EXPECT_GT(state.weight, 0.5f);
}

//=============================================================================
// Bio-Async Integration Tests
//=============================================================================

TEST_F(EmotionPlasticityBridgeTest, ConnectBioAsync) {
    /* Create bridge with bio-async enabled */
    emotion_plasticity_config_t config = emotion_plasticity_config_default();
    config.enable_bio_async = true;

    emotion_plasticity_bridge_t* async_bridge = emotion_plasticity_create(&config);
    ASSERT_NE(async_bridge, nullptr);

    int ret = emotion_plasticity_connect_bio_async(async_bridge);
    EXPECT_EQ(ret, 0);

    EXPECT_TRUE(emotion_plasticity_is_bio_async_connected(async_bridge));

    emotion_plasticity_destroy(async_bridge);
}

TEST_F(EmotionPlasticityBridgeTest, ConnectBioAsyncDisabled) {
    /* Bridge was created with bio-async disabled */
    int ret = emotion_plasticity_connect_bio_async(bridge);
    EXPECT_EQ(ret, 0);  /* Returns 0 when disabled */

    EXPECT_FALSE(emotion_plasticity_is_bio_async_connected(bridge));
}

TEST_F(EmotionPlasticityBridgeTest, ConnectBioAsyncNull) {
    EXPECT_EQ(emotion_plasticity_connect_bio_async(nullptr), -1);
}

TEST_F(EmotionPlasticityBridgeTest, DisconnectBioAsync) {
    emotion_plasticity_config_t config = emotion_plasticity_config_default();
    config.enable_bio_async = true;

    emotion_plasticity_bridge_t* async_bridge = emotion_plasticity_create(&config);
    ASSERT_NE(async_bridge, nullptr);

    emotion_plasticity_connect_bio_async(async_bridge);
    EXPECT_TRUE(emotion_plasticity_is_bio_async_connected(async_bridge));

    int ret = emotion_plasticity_disconnect_bio_async(async_bridge);
    EXPECT_EQ(ret, 0);

    EXPECT_FALSE(emotion_plasticity_is_bio_async_connected(async_bridge));

    emotion_plasticity_destroy(async_bridge);
}

TEST_F(EmotionPlasticityBridgeTest, DisconnectBioAsyncNull) {
    EXPECT_EQ(emotion_plasticity_disconnect_bio_async(nullptr), -1);
}

TEST_F(EmotionPlasticityBridgeTest, IsBioAsyncConnectedNull) {
    EXPECT_FALSE(emotion_plasticity_is_bio_async_connected(nullptr));
}

//=============================================================================
// Weight Bounds Tests
//=============================================================================

TEST_F(EmotionPlasticityBridgeTest, WeightBoundsRespected) {
    emotion_plasticity_register_synapse(
        bridge, 1, EMOTION_SYNAPSE_STIMULUS_TO_EMOTION, EMOTION_FEAR, 0.95f);

    /* Try to trigger massive LTP to exceed bounds */
    for (int i = 0; i < 100; i++) {
        emotion_plasticity_stimulus(bridge, EMOTION_FEAR, 1.0f, i * 1000);
        emotion_plasticity_response(bridge, EMOTION_FEAR, 1.0f, i * 1000 + 5000);
    }

    emotion_plasticity_synapse_t state;
    emotion_plasticity_get_synapse(bridge, 1, &state);

    EXPECT_LE(state.weight, 1.0f) << "Weight should not exceed max";
    EXPECT_GE(state.weight, 0.0f) << "Weight should not go below min";
}

TEST_F(EmotionPlasticityBridgeTest, WeightBoundsLowerBound) {
    emotion_plasticity_register_synapse(
        bridge, 1, EMOTION_SYNAPSE_STIMULUS_TO_EMOTION, EMOTION_FEAR, 0.05f);

    /* Try to trigger massive LTD to go below bounds */
    for (int i = 0; i < 100; i++) {
        emotion_plasticity_response(bridge, EMOTION_FEAR, 1.0f, i * 1000);
        emotion_plasticity_stimulus(bridge, EMOTION_FEAR, 1.0f, i * 1000 + 5000);
    }

    emotion_plasticity_synapse_t state;
    emotion_plasticity_get_synapse(bridge, 1, &state);

    EXPECT_GE(state.weight, 0.0f) << "Weight should not go below min";
}

//=============================================================================
// Synapse Capacity Tests
//=============================================================================

TEST_F(EmotionPlasticityBridgeTest, MaxSynapseCapacity) {
    /* Register up to max synapses */
    int registered = 0;
    for (uint32_t i = 0; i < EMOTION_PLASTICITY_MAX_SYNAPSES + 10; i++) {
        int ret = emotion_plasticity_register_synapse(
            bridge, i, EMOTION_SYNAPSE_STIMULUS_TO_EMOTION,
            (emotion_category_t)(i % EMOTION_COUNT), 0.5f);
        if (ret == 0) {
            registered++;
        }
    }

    EXPECT_EQ(registered, EMOTION_PLASTICITY_MAX_SYNAPSES)
        << "Should register exactly max synapses";
}

//=============================================================================
// Null Parameter Handling Summary
//=============================================================================

TEST_F(EmotionPlasticityBridgeTest, NullBridgeHandling) {
    emotion_plasticity_synapse_t synapse;
    emotion_plasticity_bridge_state_t state;
    emotion_plasticity_stats_t stats;
    float modulation;

    EXPECT_EQ(emotion_plasticity_reset(nullptr), -1);
    EXPECT_EQ(emotion_plasticity_register_synapse(nullptr, 0, EMOTION_SYNAPSE_STIMULUS_TO_EMOTION, EMOTION_FEAR, 0), -1);
    EXPECT_EQ(emotion_plasticity_unregister_synapse(nullptr, 0), -1);
    EXPECT_EQ(emotion_plasticity_get_synapse(nullptr, 0, &synapse), -1);
    EXPECT_EQ(emotion_plasticity_stimulus(nullptr, EMOTION_FEAR, 0, 0), -1);
    EXPECT_EQ(emotion_plasticity_response(nullptr, EMOTION_FEAR, 0, 0), -1);
    EXPECT_EQ(emotion_plasticity_reward(nullptr, 0, 0), -1);
    EXPECT_EQ(emotion_plasticity_extinction_trial(nullptr, EMOTION_FEAR, 0), -1);
    EXPECT_EQ(emotion_plasticity_update(nullptr, 0), -1);
    EXPECT_EQ(emotion_plasticity_consolidate(nullptr), -1);
    EXPECT_EQ(emotion_plasticity_get_response_modulation(nullptr, EMOTION_FEAR, &modulation), -1);
    EXPECT_FLOAT_EQ(emotion_plasticity_get_extinction_level(nullptr, EMOTION_FEAR), 0.0f);
    EXPECT_FLOAT_EQ(emotion_plasticity_get_sensitivity(nullptr, EMOTION_FEAR), 1.0f);
    EXPECT_EQ(emotion_plasticity_get_state(nullptr, &state), -1);
    EXPECT_EQ(emotion_plasticity_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(emotion_plasticity_set_weight_callback(nullptr, test_weight_callback, nullptr), -1);
    EXPECT_EQ(emotion_plasticity_set_valence_modulation(nullptr, 0), -1);
    EXPECT_EQ(emotion_plasticity_set_arousal_modulation(nullptr, 0), -1);
    EXPECT_EQ(emotion_plasticity_connect_bio_async(nullptr), -1);
    EXPECT_EQ(emotion_plasticity_disconnect_bio_async(nullptr), -1);
    EXPECT_FALSE(emotion_plasticity_is_bio_async_connected(nullptr));
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
