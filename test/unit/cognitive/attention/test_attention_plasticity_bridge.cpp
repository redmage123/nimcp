/**
 * @file test_attention_plasticity_bridge.cpp
 * @brief Unit tests for Attention System - Plasticity Bridge integration
 * @date 2026-01-06
 *
 * Tests bidirectional integration between attention system and plasticity mechanisms:
 * - Attention --> Plasticity: Focus/shift events trigger STDP learning
 * - Plasticity --> Attention: Weight updates modulate attention biases
 * - STDP learning rules
 * - BCM metaplasticity
 * - Habituation/novelty mechanics
 * - Reward-modulated plasticity
 */

#include <gtest/gtest.h>

// Note: Header has its own extern "C" guards
#include "cognitive/attention/nimcp_attention_plasticity_bridge.h"

#include "utils/time/nimcp_time.h"

#include <cmath>
#include <cstring>
#include <vector>

//=============================================================================
// Test Fixtures
//=============================================================================

class AttentionPlasticityBridgeTest : public ::testing::Test {
protected:
    attention_plasticity_bridge_t* bridge = nullptr;

    void SetUp() override {
        attention_plasticity_config_t config = attention_plasticity_config_default();
        config.enable_bio_async = false;  /* Disable for unit tests */
        config.enable_immune_modulation = false;
        config.enable_sleep_consolidation = false;
        bridge = attention_plasticity_create(&config);
        ASSERT_NE(bridge, nullptr) << "Failed to create attention-plasticity bridge";
    }

    void TearDown() override {
        if (bridge) {
            attention_plasticity_destroy(bridge);
            bridge = nullptr;
        }
    }

    /* Helper to register multiple synapses for a head */
    void register_synapses_for_head(uint32_t head_idx, uint32_t count, float init_weight) {
        for (uint32_t i = 0; i < count; i++) {
            uint32_t synapse_id = head_idx * 1000 + i;  // Unique ID per head
            attention_plasticity_register_synapse(bridge, synapse_id,
                ATTENTION_SYNAPSE_QUERY_KEY, head_idx, init_weight);
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(AttentionPlasticityBridgeTest, CreateDestroy) {
    EXPECT_NE(bridge, nullptr);
}

TEST_F(AttentionPlasticityBridgeTest, CreateWithDefaultConfig) {
    attention_plasticity_bridge_t* b = attention_plasticity_create(nullptr);
    ASSERT_NE(b, nullptr);
    attention_plasticity_destroy(b);
}

TEST_F(AttentionPlasticityBridgeTest, DestroyNull) {
    /* Should not crash */
    attention_plasticity_destroy(nullptr);
}

TEST_F(AttentionPlasticityBridgeTest, ResetBridge) {
    register_synapses_for_head(0, 3, 0.7f);

    int ret = attention_plasticity_reset(bridge);
    EXPECT_EQ(ret, 0) << "Reset should succeed";

    /* After reset, weights should be back to initial values */
    attention_plasticity_synapse_t synapse;
    ret = attention_plasticity_get_synapse(bridge, 0, &synapse);
    EXPECT_EQ(ret, 0);
    EXPECT_FLOAT_EQ(synapse.weight, synapse.initial_weight);
}

TEST_F(AttentionPlasticityBridgeTest, ResetNull) {
    EXPECT_EQ(attention_plasticity_reset(nullptr), -1);
}

TEST_F(AttentionPlasticityBridgeTest, ResetClearsHabituation) {
    register_synapses_for_head(0, 3, 0.5f);

    /* Build up habituation */
    for (int i = 0; i < 10; i++) {
        attention_plasticity_habituation_trial(bridge, 0, i * 1000000);
    }

    float hab_before = attention_plasticity_get_habituation(bridge, 0);
    EXPECT_GT(hab_before, 0.0f);

    /* Reset should clear habituation */
    attention_plasticity_reset(bridge);

    float hab_after = attention_plasticity_get_habituation(bridge, 0);
    EXPECT_FLOAT_EQ(hab_after, 0.0f);
}

//=============================================================================
// Default Configuration Tests
//=============================================================================

TEST_F(AttentionPlasticityBridgeTest, DefaultConfigValues) {
    attention_plasticity_config_t config = attention_plasticity_config_default();

    /* STDP parameters */
    EXPECT_GT(config.stdp_ltp_window_ms, 0.0f);
    EXPECT_GT(config.stdp_ltd_window_ms, 0.0f);
    EXPECT_GT(config.stdp_a_plus, 0.0f);
    EXPECT_GT(config.stdp_a_minus, 0.0f);
    EXPECT_GT(config.stdp_tau_plus, 0.0f);
    EXPECT_GT(config.stdp_tau_minus, 0.0f);

    /* Attention modulation */
    EXPECT_TRUE(config.enable_attention_modulation);
    EXPECT_GT(config.focus_learning_boost, 0.0f);
    EXPECT_GT(config.unfocused_ltd_boost, 0.0f);
    EXPECT_GT(config.attention_learning_gain, 0.0f);

    /* Salience modulation */
    EXPECT_TRUE(config.enable_salience_modulation);
    EXPECT_GT(config.salience_learning_gain, 0.0f);
    EXPECT_GT(config.salience_threshold, 0.0f);

    /* BCM */
    EXPECT_TRUE(config.enable_bcm);
    EXPECT_GT(config.bcm_threshold_tau, 0.0f);
    EXPECT_GT(config.bcm_activity_tau, 0.0f);

    /* Homeostatic */
    EXPECT_TRUE(config.enable_homeostatic);
    EXPECT_GT(config.target_attention_rate, 0.0f);
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

    /* Habituation */
    EXPECT_TRUE(config.enable_habituation);
    EXPECT_GT(config.habituation_rate, 0.0f);
    EXPECT_GT(config.spontaneous_recovery_tau, 0.0f);

    /* Novelty */
    EXPECT_TRUE(config.enable_novelty_detection);
    EXPECT_GT(config.novelty_boost, 0.0f);
    EXPECT_GT(config.familiarity_threshold, 0.0f);
}

TEST_F(AttentionPlasticityBridgeTest, DefaultConfigSTDPWindow) {
    attention_plasticity_config_t config = attention_plasticity_config_default();
    EXPECT_FLOAT_EQ(config.stdp_ltp_window_ms, ATTENTION_PLASTICITY_STDP_WINDOW);
    EXPECT_FLOAT_EQ(config.stdp_ltd_window_ms, ATTENTION_PLASTICITY_STDP_WINDOW);
}

//=============================================================================
// Synapse Management Tests
//=============================================================================

TEST_F(AttentionPlasticityBridgeTest, RegisterSynapse) {
    int ret = attention_plasticity_register_synapse(
        bridge, 1, ATTENTION_SYNAPSE_QUERY_KEY, 0, 0.5f);

    EXPECT_EQ(ret, 0) << "Should register synapse successfully";
}

TEST_F(AttentionPlasticityBridgeTest, RegisterSynapseAllTypes) {
    int ret1 = attention_plasticity_register_synapse(
        bridge, 1, ATTENTION_SYNAPSE_QUERY_KEY, 0, 0.5f);
    int ret2 = attention_plasticity_register_synapse(
        bridge, 2, ATTENTION_SYNAPSE_KEY_VALUE, 0, 0.5f);
    int ret3 = attention_plasticity_register_synapse(
        bridge, 3, ATTENTION_SYNAPSE_HEAD_OUTPUT, 1, 0.5f);
    int ret4 = attention_plasticity_register_synapse(
        bridge, 4, ATTENTION_SYNAPSE_GATE_CONTROL, 1, 0.5f);
    int ret5 = attention_plasticity_register_synapse(
        bridge, 5, ATTENTION_SYNAPSE_SALIENCE, 2, 0.5f);
    int ret6 = attention_plasticity_register_synapse(
        bridge, 6, ATTENTION_SYNAPSE_COMPETITION, 2, 0.5f);

    EXPECT_EQ(ret1, 0);
    EXPECT_EQ(ret2, 0);
    EXPECT_EQ(ret3, 0);
    EXPECT_EQ(ret4, 0);
    EXPECT_EQ(ret5, 0);
    EXPECT_EQ(ret6, 0);
}

TEST_F(AttentionPlasticityBridgeTest, RegisterSynapseAlreadyExists) {
    attention_plasticity_register_synapse(
        bridge, 1, ATTENTION_SYNAPSE_QUERY_KEY, 0, 0.5f);

    /* Registering same synapse_id again should fail */
    int ret = attention_plasticity_register_synapse(
        bridge, 1, ATTENTION_SYNAPSE_KEY_VALUE, 1, 0.7f);

    EXPECT_EQ(ret, -1) << "Should fail for already registered synapse";
}

TEST_F(AttentionPlasticityBridgeTest, RegisterSynapseNullBridge) {
    int ret = attention_plasticity_register_synapse(
        nullptr, 1, ATTENTION_SYNAPSE_QUERY_KEY, 0, 0.5f);

    EXPECT_EQ(ret, -1);
}

TEST_F(AttentionPlasticityBridgeTest, RegisterSynapseWeightClamped) {
    /* Register with weight above max (1.0) */
    attention_plasticity_register_synapse(bridge, 1, ATTENTION_SYNAPSE_QUERY_KEY, 0, 5.0f);

    attention_plasticity_synapse_t synapse;
    attention_plasticity_get_synapse(bridge, 1, &synapse);

    EXPECT_LE(synapse.weight, 1.0f) << "Weight should be clamped to max";
}

TEST_F(AttentionPlasticityBridgeTest, RegisterSynapseWeightClampedLower) {
    /* Register with weight below min (0.0) */
    attention_plasticity_register_synapse(bridge, 1, ATTENTION_SYNAPSE_QUERY_KEY, 0, -1.0f);

    attention_plasticity_synapse_t synapse;
    attention_plasticity_get_synapse(bridge, 1, &synapse);

    EXPECT_GE(synapse.weight, 0.0f) << "Weight should be clamped to min";
}

TEST_F(AttentionPlasticityBridgeTest, UnregisterSynapse) {
    attention_plasticity_register_synapse(
        bridge, 1, ATTENTION_SYNAPSE_QUERY_KEY, 0, 0.5f);

    int ret = attention_plasticity_unregister_synapse(bridge, 1);
    EXPECT_EQ(ret, 0) << "Should unregister synapse";

    /* Try to unregister again - should fail */
    ret = attention_plasticity_unregister_synapse(bridge, 1);
    EXPECT_EQ(ret, -1) << "Should fail for non-existent synapse";
}

TEST_F(AttentionPlasticityBridgeTest, UnregisterSynapseNotFound) {
    int ret = attention_plasticity_unregister_synapse(bridge, 99999);
    EXPECT_EQ(ret, -1) << "Should fail for non-existent synapse";
}

TEST_F(AttentionPlasticityBridgeTest, UnregisterSynapseNullBridge) {
    EXPECT_EQ(attention_plasticity_unregister_synapse(nullptr, 1), -1);
}

TEST_F(AttentionPlasticityBridgeTest, GetSynapseState) {
    attention_plasticity_register_synapse(
        bridge, 42, ATTENTION_SYNAPSE_KEY_VALUE, 3, 0.7f);

    attention_plasticity_synapse_t state;
    int ret = attention_plasticity_get_synapse(bridge, 42, &state);

    EXPECT_EQ(ret, 0) << "Should get synapse state";
    EXPECT_EQ(state.synapse_id, 42u);
    EXPECT_EQ(state.type, ATTENTION_SYNAPSE_KEY_VALUE);
    EXPECT_EQ(state.head_idx, 3u);
    EXPECT_FLOAT_EQ(state.weight, 0.7f);
    EXPECT_FLOAT_EQ(state.initial_weight, 0.7f);
    EXPECT_FLOAT_EQ(state.eligibility_trace, 0.0f);
    EXPECT_FLOAT_EQ(state.habituation_level, 0.0f);
    EXPECT_FLOAT_EQ(state.familiarity, 0.0f);
    EXPECT_EQ(state.exposure_count, 0u);
}

TEST_F(AttentionPlasticityBridgeTest, GetSynapseNotFound) {
    attention_plasticity_synapse_t state;
    int ret = attention_plasticity_get_synapse(bridge, 99999, &state);

    EXPECT_EQ(ret, -1) << "Should fail for non-existent synapse";
}

TEST_F(AttentionPlasticityBridgeTest, GetSynapseNullParams) {
    attention_plasticity_register_synapse(
        bridge, 1, ATTENTION_SYNAPSE_QUERY_KEY, 0, 0.5f);

    attention_plasticity_synapse_t state;
    EXPECT_EQ(attention_plasticity_get_synapse(nullptr, 1, &state), -1);
    EXPECT_EQ(attention_plasticity_get_synapse(bridge, 1, nullptr), -1);
}

//=============================================================================
// Event Recording - Focus Tests
//=============================================================================

TEST_F(AttentionPlasticityBridgeTest, FocusRecording) {
    register_synapses_for_head(0, 3, 0.5f);

    int ret = attention_plasticity_focus(bridge, 0, 1.0f, 1000000);
    EXPECT_EQ(ret, 0) << "Focus should succeed";
}

TEST_F(AttentionPlasticityBridgeTest, FocusNullBridge) {
    EXPECT_EQ(attention_plasticity_focus(nullptr, 0, 1.0f, 1000000), -1);
}

TEST_F(AttentionPlasticityBridgeTest, FocusInvalidHead) {
    EXPECT_EQ(attention_plasticity_focus(bridge, ATTENTION_PLASTICITY_MAX_HEADS, 1.0f, 1000000), -1);
}

TEST_F(AttentionPlasticityBridgeTest, FocusUpdatesExposureCount) {
    attention_plasticity_register_synapse(bridge, 1, ATTENTION_SYNAPSE_QUERY_KEY, 0, 0.5f);

    attention_plasticity_focus(bridge, 0, 1.0f, 1000000);

    attention_plasticity_synapse_t synapse;
    attention_plasticity_get_synapse(bridge, 1, &synapse);

    EXPECT_EQ(synapse.exposure_count, 1u) << "Exposure count should increment";
}

TEST_F(AttentionPlasticityBridgeTest, FocusUpdatesEligibilityTrace) {
    attention_plasticity_register_synapse(bridge, 1, ATTENTION_SYNAPSE_QUERY_KEY, 0, 0.5f);

    attention_plasticity_focus(bridge, 0, 0.8f, 1000000);

    attention_plasticity_synapse_t synapse;
    attention_plasticity_get_synapse(bridge, 1, &synapse);

    EXPECT_GT(synapse.eligibility_trace, 0.0f) << "Eligibility trace should be set";
}

TEST_F(AttentionPlasticityBridgeTest, FocusUpdatesAttentionBias) {
    register_synapses_for_head(0, 1, 0.5f);

    float bias_before;
    attention_plasticity_get_bias(bridge, 0, &bias_before);

    /* Multiple focus events */
    for (int i = 0; i < 10; i++) {
        attention_plasticity_focus(bridge, 0, 1.0f, i * 1000000);
    }

    float bias_after;
    attention_plasticity_get_bias(bridge, 0, &bias_after);

    EXPECT_GT(bias_after, bias_before) << "Repeated focus should increase attention bias";
}

//=============================================================================
// Event Recording - Shift Tests
//=============================================================================

TEST_F(AttentionPlasticityBridgeTest, ShiftRecording) {
    register_synapses_for_head(0, 3, 0.5f);
    register_synapses_for_head(1, 3, 0.5f);

    int ret = attention_plasticity_shift(bridge, 0, 1, 0.8f, 1000000);
    EXPECT_EQ(ret, 0) << "Shift should succeed";
}

TEST_F(AttentionPlasticityBridgeTest, ShiftNullBridge) {
    EXPECT_EQ(attention_plasticity_shift(nullptr, 0, 1, 0.8f, 1000000), -1);
}

TEST_F(AttentionPlasticityBridgeTest, ShiftInvalidFromHead) {
    EXPECT_EQ(attention_plasticity_shift(bridge, ATTENTION_PLASTICITY_MAX_HEADS, 1, 0.8f, 1000000), -1);
}

TEST_F(AttentionPlasticityBridgeTest, ShiftInvalidToHead) {
    EXPECT_EQ(attention_plasticity_shift(bridge, 0, ATTENTION_PLASTICITY_MAX_HEADS, 0.8f, 1000000), -1);
}

TEST_F(AttentionPlasticityBridgeTest, ShiftCausesLTDOnSourceHead) {
    attention_plasticity_register_synapse(bridge, 1, ATTENTION_SYNAPSE_QUERY_KEY, 0, 0.5f);
    attention_plasticity_register_synapse(bridge, 2, ATTENTION_SYNAPSE_QUERY_KEY, 1, 0.5f);

    /* Focus on source head first */
    attention_plasticity_focus(bridge, 0, 1.0f, 1000000);

    float weight_before;
    {
        attention_plasticity_synapse_t synapse;
        attention_plasticity_get_synapse(bridge, 1, &synapse);
        weight_before = synapse.weight;
    }

    /* Shift away from source head */
    attention_plasticity_shift(bridge, 0, 1, 1.0f, 1010000);

    float weight_after;
    {
        attention_plasticity_synapse_t synapse;
        attention_plasticity_get_synapse(bridge, 1, &synapse);
        weight_after = synapse.weight;
    }

    EXPECT_LT(weight_after, weight_before) << "Shift should cause LTD on source head";
}

TEST_F(AttentionPlasticityBridgeTest, ShiftCausesLTPOnTargetHead) {
    attention_plasticity_register_synapse(bridge, 1, ATTENTION_SYNAPSE_QUERY_KEY, 0, 0.5f);
    attention_plasticity_register_synapse(bridge, 2, ATTENTION_SYNAPSE_QUERY_KEY, 1, 0.5f);

    float weight_before;
    {
        attention_plasticity_synapse_t synapse;
        attention_plasticity_get_synapse(bridge, 2, &synapse);
        weight_before = synapse.weight;
    }

    /* Shift to target head */
    attention_plasticity_shift(bridge, 0, 1, 1.0f, 1000000);

    float weight_after;
    {
        attention_plasticity_synapse_t synapse;
        attention_plasticity_get_synapse(bridge, 2, &synapse);
        weight_after = synapse.weight;
    }

    EXPECT_GT(weight_after, weight_before) << "Shift should cause LTP on target head";
}

//=============================================================================
// Event Recording - Salience Tests
//=============================================================================

TEST_F(AttentionPlasticityBridgeTest, SalienceRecording) {
    float salience_map[] = {0.1f, 0.5f, 0.9f, 0.3f};

    int ret = attention_plasticity_salience(bridge, salience_map, 4, 1000000);
    EXPECT_EQ(ret, 0) << "Salience should succeed";
}

TEST_F(AttentionPlasticityBridgeTest, SalienceNullBridge) {
    float salience_map[] = {0.5f};
    EXPECT_EQ(attention_plasticity_salience(nullptr, salience_map, 1, 1000000), -1);
}

TEST_F(AttentionPlasticityBridgeTest, SalienceNullMap) {
    EXPECT_EQ(attention_plasticity_salience(bridge, nullptr, 4, 1000000), -1);
}

TEST_F(AttentionPlasticityBridgeTest, SalienceModulatesLearningRate) {
    /* High salience should increase learning rate */
    float high_salience[] = {0.9f, 0.9f, 0.9f};
    attention_plasticity_salience(bridge, high_salience, 3, 1000000);

    attention_plasticity_bridge_state_t state;
    attention_plasticity_get_state(bridge, &state);

    EXPECT_GT(state.global_learning_rate, 1.0f) << "High salience should boost learning rate";
}

TEST_F(AttentionPlasticityBridgeTest, LowSalienceReducesLearningRate) {
    /* Low salience should reduce learning rate */
    float low_salience[] = {0.1f, 0.1f, 0.1f};
    attention_plasticity_salience(bridge, low_salience, 3, 1000000);

    attention_plasticity_bridge_state_t state;
    attention_plasticity_get_state(bridge, &state);

    EXPECT_LT(state.global_learning_rate, 1.0f) << "Low salience should reduce learning rate";
}

//=============================================================================
// Event Recording - Reward Tests
//=============================================================================

TEST_F(AttentionPlasticityBridgeTest, RewardPositive) {
    register_synapses_for_head(0, 3, 0.5f);

    /* Create eligibility by focusing */
    attention_plasticity_focus(bridge, 0, 1.0f, 1000000);

    /* Apply positive reward */
    int ret = attention_plasticity_reward(bridge, 0.5f, 1020000);
    EXPECT_EQ(ret, 0) << "Positive reward should succeed";
}

TEST_F(AttentionPlasticityBridgeTest, RewardNegative) {
    register_synapses_for_head(0, 3, 0.5f);

    attention_plasticity_focus(bridge, 0, 1.0f, 1000000);

    /* Apply negative reward (punishment) */
    int ret = attention_plasticity_reward(bridge, -0.5f, 1020000);
    EXPECT_EQ(ret, 0) << "Negative reward should succeed";
}

TEST_F(AttentionPlasticityBridgeTest, RewardNullBridge) {
    EXPECT_EQ(attention_plasticity_reward(nullptr, 0.5f, 1000000), -1);
}

TEST_F(AttentionPlasticityBridgeTest, RewardWithEligibilityTrace) {
    attention_plasticity_register_synapse(bridge, 1, ATTENTION_SYNAPSE_QUERY_KEY, 0, 0.5f);

    /* Focus sets eligibility trace */
    attention_plasticity_focus(bridge, 0, 1.0f, 1000000);

    attention_plasticity_synapse_t state_before;
    attention_plasticity_get_synapse(bridge, 1, &state_before);
    float weight_before = state_before.weight;

    /* Apply positive reward - should modulate based on eligibility trace */
    attention_plasticity_reward(bridge, 1.0f, 1010000);

    attention_plasticity_synapse_t state_after;
    attention_plasticity_get_synapse(bridge, 1, &state_after);

    /* With active eligibility trace and positive reward, weight should increase */
    EXPECT_GT(state_after.weight, weight_before) << "Positive reward should strengthen eligible synapse";
}

TEST_F(AttentionPlasticityBridgeTest, RewardAccumulatesInStats) {
    attention_plasticity_reward(bridge, 0.5f, 1000000);
    attention_plasticity_reward(bridge, 0.3f, 2000000);

    attention_plasticity_stats_t stats;
    attention_plasticity_get_stats(bridge, &stats);

    EXPECT_FLOAT_EQ(stats.total_reward, 0.8f);
}

//=============================================================================
// Event Recording - Habituation Tests
//=============================================================================

TEST_F(AttentionPlasticityBridgeTest, HabituationTrial) {
    register_synapses_for_head(0, 3, 0.8f);

    int ret = attention_plasticity_habituation_trial(bridge, 0, 1000000);
    EXPECT_EQ(ret, 0) << "Habituation trial should succeed";
}

TEST_F(AttentionPlasticityBridgeTest, HabituationTrialNullBridge) {
    EXPECT_EQ(attention_plasticity_habituation_trial(nullptr, 0, 1000000), -1);
}

TEST_F(AttentionPlasticityBridgeTest, HabituationTrialInvalidHead) {
    EXPECT_EQ(attention_plasticity_habituation_trial(bridge, ATTENTION_PLASTICITY_MAX_HEADS, 1000000), -1);
}

TEST_F(AttentionPlasticityBridgeTest, HabituationIncreasesWithTrials) {
    register_synapses_for_head(0, 1, 0.8f);

    float hab_before = attention_plasticity_get_habituation(bridge, 0);
    EXPECT_FLOAT_EQ(hab_before, 0.0f);

    /* Multiple habituation trials */
    for (int i = 0; i < 5; i++) {
        attention_plasticity_habituation_trial(bridge, 0, i * 1000000);
    }

    float hab_after = attention_plasticity_get_habituation(bridge, 0);
    EXPECT_GT(hab_after, hab_before) << "Habituation should increase with trials";
}

TEST_F(AttentionPlasticityBridgeTest, HabituationReducesSynapseWeight) {
    attention_plasticity_register_synapse(bridge, 1, ATTENTION_SYNAPSE_QUERY_KEY, 0, 0.8f);

    attention_plasticity_synapse_t state_before;
    attention_plasticity_get_synapse(bridge, 1, &state_before);

    /* Multiple habituation trials */
    for (int i = 0; i < 10; i++) {
        attention_plasticity_habituation_trial(bridge, 0, i * 1000000);
    }

    attention_plasticity_synapse_t state_after;
    attention_plasticity_get_synapse(bridge, 1, &state_after);

    EXPECT_LT(state_after.weight, state_before.weight) << "Habituation should reduce weight";
}

//=============================================================================
// Event Recording - Novelty Tests
//=============================================================================

TEST_F(AttentionPlasticityBridgeTest, NoveltyRecording) {
    register_synapses_for_head(0, 3, 0.5f);

    int ret = attention_plasticity_novelty(bridge, 0, 0.9f, 1000000);
    EXPECT_EQ(ret, 0) << "Novelty should succeed";
}

TEST_F(AttentionPlasticityBridgeTest, NoveltyNullBridge) {
    EXPECT_EQ(attention_plasticity_novelty(nullptr, 0, 0.9f, 1000000), -1);
}

TEST_F(AttentionPlasticityBridgeTest, NoveltyInvalidHead) {
    EXPECT_EQ(attention_plasticity_novelty(bridge, ATTENTION_PLASTICITY_MAX_HEADS, 0.9f, 1000000), -1);
}

TEST_F(AttentionPlasticityBridgeTest, HighNoveltyBoostsLearning) {
    attention_plasticity_register_synapse(bridge, 1, ATTENTION_SYNAPSE_QUERY_KEY, 0, 0.5f);

    attention_plasticity_synapse_t state_before;
    attention_plasticity_get_synapse(bridge, 1, &state_before);

    /* High novelty event */
    attention_plasticity_novelty(bridge, 0, 0.9f, 1000000);

    attention_plasticity_synapse_t state_after;
    attention_plasticity_get_synapse(bridge, 1, &state_after);

    EXPECT_GT(state_after.weight, state_before.weight) << "High novelty should boost learning";
}

TEST_F(AttentionPlasticityBridgeTest, NoveltyUpdatesNoveltyScore) {
    float score_before = attention_plasticity_get_novelty_score(bridge, 0);
    EXPECT_FLOAT_EQ(score_before, 1.0f);  /* Initial novelty is high */

    /* Low novelty (familiar) */
    attention_plasticity_novelty(bridge, 0, 0.2f, 1000000);

    float score_after = attention_plasticity_get_novelty_score(bridge, 0);
    EXPECT_FLOAT_EQ(score_after, 0.2f);
}

TEST_F(AttentionPlasticityBridgeTest, NoveltyResetsHabituation) {
    register_synapses_for_head(0, 1, 0.5f);

    /* Build up habituation */
    for (int i = 0; i < 10; i++) {
        attention_plasticity_habituation_trial(bridge, 0, i * 1000000);
    }

    float hab_before = attention_plasticity_get_habituation(bridge, 0);
    EXPECT_GT(hab_before, 0.0f);

    /* High novelty should reduce habituation */
    attention_plasticity_novelty(bridge, 0, 0.9f, 10000000);

    float hab_after = attention_plasticity_get_habituation(bridge, 0);
    EXPECT_LT(hab_after, hab_before) << "Novelty should reduce habituation";
}

//=============================================================================
// Update Function Tests
//=============================================================================

TEST_F(AttentionPlasticityBridgeTest, Update) {
    register_synapses_for_head(0, 5, 0.5f);

    int ret = attention_plasticity_update(bridge, 10.0f);
    EXPECT_EQ(ret, 0) << "Update should succeed";
}

TEST_F(AttentionPlasticityBridgeTest, UpdateNullBridge) {
    EXPECT_EQ(attention_plasticity_update(nullptr, 10.0f), -1);
}

TEST_F(AttentionPlasticityBridgeTest, UpdateMultipleTimesteps) {
    register_synapses_for_head(0, 3, 0.5f);

    /* Run multiple update cycles */
    for (int i = 0; i < 100; i++) {
        int ret = attention_plasticity_update(bridge, 1.0f);
        EXPECT_EQ(ret, 0);
    }
}

TEST_F(AttentionPlasticityBridgeTest, UpdateDecaysEligibilityTrace) {
    attention_plasticity_register_synapse(bridge, 1, ATTENTION_SYNAPSE_QUERY_KEY, 0, 0.5f);

    /* Set eligibility trace via focus */
    attention_plasticity_focus(bridge, 0, 1.0f, 1000000);

    attention_plasticity_synapse_t state_before;
    attention_plasticity_get_synapse(bridge, 1, &state_before);
    float trace_before = state_before.eligibility_trace;

    /* Update should decay trace */
    attention_plasticity_update(bridge, 10.0f);

    attention_plasticity_synapse_t state_after;
    attention_plasticity_get_synapse(bridge, 1, &state_after);

    EXPECT_LT(state_after.eligibility_trace, trace_before) << "Update should decay eligibility trace";
}

TEST_F(AttentionPlasticityBridgeTest, UpdateRecoversSpontaneousHabituation) {
    register_synapses_for_head(0, 1, 0.5f);

    /* Build up habituation */
    for (int i = 0; i < 10; i++) {
        attention_plasticity_habituation_trial(bridge, 0, i * 1000000);
    }

    float hab_before = attention_plasticity_get_habituation(bridge, 0);

    /* Many updates should cause spontaneous recovery */
    for (int i = 0; i < 1000; i++) {
        attention_plasticity_update(bridge, 100.0f);
    }

    float hab_after = attention_plasticity_get_habituation(bridge, 0);
    EXPECT_LT(hab_after, hab_before) << "Habituation should recover over time";
}

TEST_F(AttentionPlasticityBridgeTest, Consolidate) {
    register_synapses_for_head(0, 3, 0.3f);
    register_synapses_for_head(1, 3, 0.7f);

    int ret = attention_plasticity_consolidate(bridge);
    EXPECT_EQ(ret, 0) << "Consolidation should succeed";
}

TEST_F(AttentionPlasticityBridgeTest, ConsolidateNullBridge) {
    EXPECT_EQ(attention_plasticity_consolidate(nullptr), -1);
}

TEST_F(AttentionPlasticityBridgeTest, ConsolidateStrengthensStrong) {
    /* Register synapse with weight above midpoint */
    attention_plasticity_register_synapse(bridge, 1, ATTENTION_SYNAPSE_QUERY_KEY, 0, 0.7f);

    /* Enable sleep consolidation */
    attention_plasticity_config_t config = attention_plasticity_config_default();
    config.enable_sleep_consolidation = true;
    attention_plasticity_bridge_t* test_bridge = attention_plasticity_create(&config);
    ASSERT_NE(test_bridge, nullptr);

    attention_plasticity_register_synapse(test_bridge, 1, ATTENTION_SYNAPSE_QUERY_KEY, 0, 0.7f);

    attention_plasticity_synapse_t state_before;
    attention_plasticity_get_synapse(test_bridge, 1, &state_before);

    attention_plasticity_consolidate(test_bridge);

    attention_plasticity_synapse_t state_after;
    attention_plasticity_get_synapse(test_bridge, 1, &state_after);

    EXPECT_GT(state_after.weight, state_before.weight)
        << "Consolidation should strengthen strong synapses";

    attention_plasticity_destroy(test_bridge);
}

TEST_F(AttentionPlasticityBridgeTest, ConsolidateWeakensWeak) {
    /* Enable sleep consolidation */
    attention_plasticity_config_t config = attention_plasticity_config_default();
    config.enable_sleep_consolidation = true;
    attention_plasticity_bridge_t* test_bridge = attention_plasticity_create(&config);
    ASSERT_NE(test_bridge, nullptr);

    /* Register synapse with weight below midpoint */
    attention_plasticity_register_synapse(test_bridge, 1, ATTENTION_SYNAPSE_QUERY_KEY, 0, 0.3f);

    attention_plasticity_synapse_t state_before;
    attention_plasticity_get_synapse(test_bridge, 1, &state_before);

    attention_plasticity_consolidate(test_bridge);

    attention_plasticity_synapse_t state_after;
    attention_plasticity_get_synapse(test_bridge, 1, &state_after);

    EXPECT_LT(state_after.weight, state_before.weight)
        << "Consolidation should weaken weak synapses";

    attention_plasticity_destroy(test_bridge);
}

TEST_F(AttentionPlasticityBridgeTest, ConsolidateClearsEligibilityTrace) {
    attention_plasticity_config_t config = attention_plasticity_config_default();
    config.enable_sleep_consolidation = true;
    attention_plasticity_bridge_t* test_bridge = attention_plasticity_create(&config);
    ASSERT_NE(test_bridge, nullptr);

    attention_plasticity_register_synapse(test_bridge, 1, ATTENTION_SYNAPSE_QUERY_KEY, 0, 0.5f);
    attention_plasticity_focus(test_bridge, 0, 1.0f, 1000000);

    attention_plasticity_synapse_t state_before;
    attention_plasticity_get_synapse(test_bridge, 1, &state_before);
    EXPECT_GT(state_before.eligibility_trace, 0.0f);

    attention_plasticity_consolidate(test_bridge);

    attention_plasticity_synapse_t state_after;
    attention_plasticity_get_synapse(test_bridge, 1, &state_after);
    EXPECT_FLOAT_EQ(state_after.eligibility_trace, 0.0f) << "Consolidation should clear eligibility";

    attention_plasticity_destroy(test_bridge);
}

//=============================================================================
// Query Functions Tests
//=============================================================================

TEST_F(AttentionPlasticityBridgeTest, GetBias) {
    register_synapses_for_head(0, 3, 0.5f);

    float bias;
    int ret = attention_plasticity_get_bias(bridge, 0, &bias);

    EXPECT_EQ(ret, 0) << "Should get bias";
    EXPECT_FLOAT_EQ(bias, 0.0f) << "Initial bias should be 0";
}

TEST_F(AttentionPlasticityBridgeTest, GetBiasNullParams) {
    float bias;
    EXPECT_EQ(attention_plasticity_get_bias(nullptr, 0, &bias), -1);
    EXPECT_EQ(attention_plasticity_get_bias(bridge, 0, nullptr), -1);
}

TEST_F(AttentionPlasticityBridgeTest, GetBiasInvalidHead) {
    float bias;
    EXPECT_EQ(attention_plasticity_get_bias(bridge, ATTENTION_PLASTICITY_MAX_HEADS, &bias), -1);
}

TEST_F(AttentionPlasticityBridgeTest, GetModulation) {
    register_synapses_for_head(0, 3, 0.7f);
    register_synapses_for_head(1, 3, 0.5f);

    float modulation[ATTENTION_PLASTICITY_MAX_HEADS];
    int ret = attention_plasticity_get_modulation(bridge, modulation, ATTENTION_PLASTICITY_MAX_HEADS);

    EXPECT_EQ(ret, 0) << "Should get modulation";
    /* Modulation should be computed from bias, habituation, and novelty */
    EXPECT_GE(modulation[0], 0.0f);
    EXPECT_LE(modulation[0], 2.0f);
}

TEST_F(AttentionPlasticityBridgeTest, GetModulationNullParams) {
    float modulation[16];
    EXPECT_EQ(attention_plasticity_get_modulation(nullptr, modulation, 16), -1);
    EXPECT_EQ(attention_plasticity_get_modulation(bridge, nullptr, 16), -1);
}

TEST_F(AttentionPlasticityBridgeTest, GetModulationPartialHeads) {
    float modulation[4];
    int ret = attention_plasticity_get_modulation(bridge, modulation, 4);

    EXPECT_EQ(ret, 0) << "Should work with fewer heads than max";
}

TEST_F(AttentionPlasticityBridgeTest, GetHabituation) {
    float hab = attention_plasticity_get_habituation(bridge, 0);
    EXPECT_FLOAT_EQ(hab, 0.0f) << "Initial habituation should be 0";
}

TEST_F(AttentionPlasticityBridgeTest, GetHabituationNullBridge) {
    float hab = attention_plasticity_get_habituation(nullptr, 0);
    EXPECT_FLOAT_EQ(hab, -1.0f);
}

TEST_F(AttentionPlasticityBridgeTest, GetHabituationInvalidHead) {
    float hab = attention_plasticity_get_habituation(bridge, ATTENTION_PLASTICITY_MAX_HEADS);
    EXPECT_FLOAT_EQ(hab, -1.0f);
}

TEST_F(AttentionPlasticityBridgeTest, GetNoveltyScore) {
    float nov = attention_plasticity_get_novelty_score(bridge, 0);
    EXPECT_FLOAT_EQ(nov, 1.0f) << "Initial novelty score should be 1.0";
}

TEST_F(AttentionPlasticityBridgeTest, GetNoveltyScoreNullBridge) {
    float nov = attention_plasticity_get_novelty_score(nullptr, 0);
    EXPECT_FLOAT_EQ(nov, -1.0f);
}

TEST_F(AttentionPlasticityBridgeTest, GetNoveltyScoreInvalidHead) {
    float nov = attention_plasticity_get_novelty_score(bridge, ATTENTION_PLASTICITY_MAX_HEADS);
    EXPECT_FLOAT_EQ(nov, -1.0f);
}

TEST_F(AttentionPlasticityBridgeTest, GetSensitivity) {
    register_synapses_for_head(0, 3, 0.6f);

    float sensitivity = attention_plasticity_get_sensitivity(bridge, 0);
    EXPECT_NEAR(sensitivity, 0.6f, 0.01f) << "Sensitivity is average weight of head's synapses";
}

TEST_F(AttentionPlasticityBridgeTest, GetSensitivityNoSynapses) {
    float sensitivity = attention_plasticity_get_sensitivity(bridge, 0);
    EXPECT_FLOAT_EQ(sensitivity, 0.0f) << "No synapses = 0 sensitivity";
}

TEST_F(AttentionPlasticityBridgeTest, GetSensitivityNullBridge) {
    float sensitivity = attention_plasticity_get_sensitivity(nullptr, 0);
    EXPECT_FLOAT_EQ(sensitivity, -1.0f);
}

TEST_F(AttentionPlasticityBridgeTest, GetSensitivityInvalidHead) {
    float sensitivity = attention_plasticity_get_sensitivity(bridge, ATTENTION_PLASTICITY_MAX_HEADS);
    EXPECT_FLOAT_EQ(sensitivity, -1.0f);
}

//=============================================================================
// State and Statistics Tests
//=============================================================================

TEST_F(AttentionPlasticityBridgeTest, GetState) {
    register_synapses_for_head(0, 5, 0.5f);

    attention_plasticity_bridge_state_t state;
    int ret = attention_plasticity_get_state(bridge, &state);

    EXPECT_EQ(ret, 0) << "Should get state";
    EXPECT_EQ(state.state, ATTENTION_PLASTICITY_STATE_IDLE);
    EXPECT_EQ(state.registered_synapses, 5u);
    EXPECT_FLOAT_EQ(state.global_learning_rate, 1.0f);
    EXPECT_FALSE(state.bio_async_connected);
}

TEST_F(AttentionPlasticityBridgeTest, GetStateNullParams) {
    attention_plasticity_bridge_state_t state;
    EXPECT_EQ(attention_plasticity_get_state(nullptr, &state), -1);
    EXPECT_EQ(attention_plasticity_get_state(bridge, nullptr), -1);
}

TEST_F(AttentionPlasticityBridgeTest, GetStats) {
    attention_plasticity_stats_t stats;
    int ret = attention_plasticity_get_stats(bridge, &stats);

    EXPECT_EQ(ret, 0) << "Should get stats";
    EXPECT_EQ(stats.total_focus_events, 0u);
    EXPECT_EQ(stats.total_shift_events, 0u);
    EXPECT_EQ(stats.total_pre_spikes, 0u);
    EXPECT_EQ(stats.total_post_spikes, 0u);
    EXPECT_EQ(stats.ltp_events, 0u);
    EXPECT_EQ(stats.ltd_events, 0u);
}

TEST_F(AttentionPlasticityBridgeTest, GetStatsNullParams) {
    attention_plasticity_stats_t stats;
    EXPECT_EQ(attention_plasticity_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(attention_plasticity_get_stats(bridge, nullptr), -1);
}

TEST_F(AttentionPlasticityBridgeTest, StatsTracksFocusEvents) {
    register_synapses_for_head(0, 3, 0.5f);

    attention_plasticity_focus(bridge, 0, 1.0f, 1000000);
    attention_plasticity_focus(bridge, 0, 0.8f, 2000000);

    attention_plasticity_stats_t stats;
    attention_plasticity_get_stats(bridge, &stats);

    EXPECT_EQ(stats.total_focus_events, 2u);
}

TEST_F(AttentionPlasticityBridgeTest, StatsTracksShiftEvents) {
    register_synapses_for_head(0, 1, 0.5f);
    register_synapses_for_head(1, 1, 0.5f);

    attention_plasticity_focus(bridge, 0, 1.0f, 1000000);
    attention_plasticity_shift(bridge, 0, 1, 0.8f, 1010000);

    attention_plasticity_stats_t stats;
    attention_plasticity_get_stats(bridge, &stats);

    EXPECT_EQ(stats.total_shift_events, 1u);
}

TEST_F(AttentionPlasticityBridgeTest, StatsTracksHabituationEvents) {
    register_synapses_for_head(0, 1, 0.5f);

    attention_plasticity_habituation_trial(bridge, 0, 1000000);
    attention_plasticity_habituation_trial(bridge, 0, 2000000);

    attention_plasticity_stats_t stats;
    attention_plasticity_get_stats(bridge, &stats);

    EXPECT_EQ(stats.habituation_events, 2u);
}

TEST_F(AttentionPlasticityBridgeTest, ResetStats) {
    register_synapses_for_head(0, 1, 0.5f);

    /* Generate activity */
    attention_plasticity_focus(bridge, 0, 1.0f, 1000000);
    attention_plasticity_habituation_trial(bridge, 0, 2000000);

    attention_plasticity_reset_stats(bridge);

    attention_plasticity_stats_t stats;
    attention_plasticity_get_stats(bridge, &stats);

    EXPECT_EQ(stats.total_focus_events, 0u);
    EXPECT_EQ(stats.habituation_events, 0u);
    EXPECT_EQ(stats.total_pre_spikes, 0u);
}

TEST_F(AttentionPlasticityBridgeTest, ResetStatsNull) {
    /* Should not crash */
    attention_plasticity_reset_stats(nullptr);
}

//=============================================================================
// Callback Tests
//=============================================================================

static int g_weight_callback_count = 0;
static float g_last_old_weight = 0.0f;
static float g_last_new_weight = 0.0f;
static attention_learn_event_t g_last_event_type = ATTENTION_LEARN_FOCUS;
static uint32_t g_last_head_idx = 0;

static void test_weight_callback(uint32_t synapse_id,
                                  uint32_t head_idx,
                                  float old_weight,
                                  float new_weight,
                                  attention_learn_event_t event_type,
                                  void* user_data) {
    g_weight_callback_count++;
    g_last_old_weight = old_weight;
    g_last_new_weight = new_weight;
    g_last_event_type = event_type;
    g_last_head_idx = head_idx;

    if (user_data) {
        *static_cast<int*>(user_data) = synapse_id;
    }
}

static int g_shift_callback_count = 0;
static uint32_t g_shift_from = 0;
static uint32_t g_shift_to = 0;
static float g_shift_strength = 0.0f;

static void test_shift_callback(uint32_t old_head,
                                 uint32_t new_head,
                                 float shift_strength,
                                 void* user_data) {
    g_shift_callback_count++;
    g_shift_from = old_head;
    g_shift_to = new_head;
    g_shift_strength = shift_strength;
}

TEST_F(AttentionPlasticityBridgeTest, SetWeightCallback) {
    g_weight_callback_count = 0;

    int ret = attention_plasticity_set_weight_callback(bridge, test_weight_callback, nullptr);
    EXPECT_EQ(ret, 0) << "Should set callback";
}

TEST_F(AttentionPlasticityBridgeTest, SetWeightCallbackNullBridge) {
    EXPECT_EQ(attention_plasticity_set_weight_callback(nullptr, test_weight_callback, nullptr), -1);
}

TEST_F(AttentionPlasticityBridgeTest, SetWeightCallbackNullCallback) {
    /* Setting NULL callback should be allowed (to unregister) */
    int ret = attention_plasticity_set_weight_callback(bridge, nullptr, nullptr);
    EXPECT_EQ(ret, 0);
}

TEST_F(AttentionPlasticityBridgeTest, WeightCallbackTriggeredOnShift) {
    g_weight_callback_count = 0;
    g_last_old_weight = 0.0f;
    g_last_new_weight = 0.0f;

    attention_plasticity_set_weight_callback(bridge, test_weight_callback, nullptr);

    attention_plasticity_register_synapse(bridge, 1, ATTENTION_SYNAPSE_QUERY_KEY, 0, 0.5f);
    attention_plasticity_register_synapse(bridge, 2, ATTENTION_SYNAPSE_QUERY_KEY, 1, 0.5f);

    /* Focus then shift */
    attention_plasticity_focus(bridge, 0, 1.0f, 1000000);
    attention_plasticity_shift(bridge, 0, 1, 1.0f, 1010000);

    EXPECT_GT(g_weight_callback_count, 0) << "Callback should be triggered";
    EXPECT_EQ(g_last_event_type, ATTENTION_LEARN_SHIFT);
}

TEST_F(AttentionPlasticityBridgeTest, WeightCallbackWithUserData) {
    g_weight_callback_count = 0;
    int synapse_id_from_callback = -1;

    attention_plasticity_set_weight_callback(bridge, test_weight_callback, &synapse_id_from_callback);

    attention_plasticity_register_synapse(bridge, 42, ATTENTION_SYNAPSE_QUERY_KEY, 0, 0.5f);
    attention_plasticity_register_synapse(bridge, 2, ATTENTION_SYNAPSE_QUERY_KEY, 1, 0.5f);

    attention_plasticity_focus(bridge, 0, 1.0f, 1000000);
    attention_plasticity_shift(bridge, 0, 1, 1.0f, 1010000);

    EXPECT_GT(g_weight_callback_count, 0);
    /* One of the callback invocations should have set the synapse_id */
}

TEST_F(AttentionPlasticityBridgeTest, SetShiftCallback) {
    g_shift_callback_count = 0;

    int ret = attention_plasticity_set_shift_callback(bridge, test_shift_callback, nullptr);
    EXPECT_EQ(ret, 0) << "Should set callback";
}

TEST_F(AttentionPlasticityBridgeTest, SetShiftCallbackNullBridge) {
    EXPECT_EQ(attention_plasticity_set_shift_callback(nullptr, test_shift_callback, nullptr), -1);
}

TEST_F(AttentionPlasticityBridgeTest, ShiftCallbackTriggered) {
    g_shift_callback_count = 0;
    g_shift_from = 99;
    g_shift_to = 99;
    g_shift_strength = 0.0f;

    attention_plasticity_set_shift_callback(bridge, test_shift_callback, nullptr);

    attention_plasticity_shift(bridge, 2, 5, 0.75f, 1000000);

    EXPECT_EQ(g_shift_callback_count, 1);
    EXPECT_EQ(g_shift_from, 2u);
    EXPECT_EQ(g_shift_to, 5u);
    EXPECT_FLOAT_EQ(g_shift_strength, 0.75f);
}

//=============================================================================
// Modulation Function Tests
//=============================================================================

TEST_F(AttentionPlasticityBridgeTest, SetAttentionModulation) {
    int ret = attention_plasticity_set_attention_modulation(bridge, 1.5f);
    EXPECT_EQ(ret, 0);

    attention_plasticity_bridge_state_t state;
    attention_plasticity_get_state(bridge, &state);
    EXPECT_FLOAT_EQ(state.current_attention_mod, 1.5f);
}

TEST_F(AttentionPlasticityBridgeTest, SetAttentionModulationClamped) {
    /* Test clamping to [0.1, 2.0] */
    attention_plasticity_set_attention_modulation(bridge, 5.0f);

    attention_plasticity_bridge_state_t state;
    attention_plasticity_get_state(bridge, &state);
    EXPECT_FLOAT_EQ(state.current_attention_mod, 2.0f);

    attention_plasticity_set_attention_modulation(bridge, 0.01f);
    attention_plasticity_get_state(bridge, &state);
    EXPECT_FLOAT_EQ(state.current_attention_mod, 0.1f);
}

TEST_F(AttentionPlasticityBridgeTest, SetAttentionModulationNull) {
    EXPECT_EQ(attention_plasticity_set_attention_modulation(nullptr, 1.0f), -1);
}

TEST_F(AttentionPlasticityBridgeTest, SetSalienceModulation) {
    int ret = attention_plasticity_set_salience_modulation(bridge, 0.8f);
    EXPECT_EQ(ret, 0);

    attention_plasticity_bridge_state_t state;
    attention_plasticity_get_state(bridge, &state);
    EXPECT_FLOAT_EQ(state.current_salience_mod, 0.8f);
}

TEST_F(AttentionPlasticityBridgeTest, SetSalienceModulationClamped) {
    /* Test clamping to [0, 2.0] */
    attention_plasticity_set_salience_modulation(bridge, 5.0f);

    attention_plasticity_bridge_state_t state;
    attention_plasticity_get_state(bridge, &state);
    EXPECT_FLOAT_EQ(state.current_salience_mod, 2.0f);

    attention_plasticity_set_salience_modulation(bridge, -1.0f);
    attention_plasticity_get_state(bridge, &state);
    EXPECT_FLOAT_EQ(state.current_salience_mod, 0.0f);
}

TEST_F(AttentionPlasticityBridgeTest, SetSalienceModulationNull) {
    EXPECT_EQ(attention_plasticity_set_salience_modulation(nullptr, 0.5f), -1);
}

//=============================================================================
// Bio-Async Integration Tests
//=============================================================================

TEST_F(AttentionPlasticityBridgeTest, ConnectBioAsync) {
    /* Create bridge with bio-async enabled */
    attention_plasticity_config_t config = attention_plasticity_config_default();
    config.enable_bio_async = true;

    attention_plasticity_bridge_t* async_bridge = attention_plasticity_create(&config);
    ASSERT_NE(async_bridge, nullptr);

    int ret = attention_plasticity_connect_bio_async(async_bridge);
    EXPECT_EQ(ret, 0);

    EXPECT_TRUE(attention_plasticity_is_bio_async_connected(async_bridge));

    attention_plasticity_destroy(async_bridge);
}

TEST_F(AttentionPlasticityBridgeTest, ConnectBioAsyncDisabled) {
    /* Bridge was created with bio-async disabled */
    int ret = attention_plasticity_connect_bio_async(bridge);
    EXPECT_EQ(ret, 0);  /* Returns 0 when disabled */

    EXPECT_FALSE(attention_plasticity_is_bio_async_connected(bridge));
}

TEST_F(AttentionPlasticityBridgeTest, ConnectBioAsyncNull) {
    EXPECT_EQ(attention_plasticity_connect_bio_async(nullptr), -1);
}

TEST_F(AttentionPlasticityBridgeTest, DisconnectBioAsync) {
    attention_plasticity_config_t config = attention_plasticity_config_default();
    config.enable_bio_async = true;

    attention_plasticity_bridge_t* async_bridge = attention_plasticity_create(&config);
    ASSERT_NE(async_bridge, nullptr);

    attention_plasticity_connect_bio_async(async_bridge);
    EXPECT_TRUE(attention_plasticity_is_bio_async_connected(async_bridge));

    int ret = attention_plasticity_disconnect_bio_async(async_bridge);
    EXPECT_EQ(ret, 0);

    EXPECT_FALSE(attention_plasticity_is_bio_async_connected(async_bridge));

    attention_plasticity_destroy(async_bridge);
}

TEST_F(AttentionPlasticityBridgeTest, DisconnectBioAsyncNull) {
    EXPECT_EQ(attention_plasticity_disconnect_bio_async(nullptr), -1);
}

TEST_F(AttentionPlasticityBridgeTest, IsBioAsyncConnectedNull) {
    EXPECT_FALSE(attention_plasticity_is_bio_async_connected(nullptr));
}

TEST_F(AttentionPlasticityBridgeTest, DisconnectWhenNotConnected) {
    /* Should succeed even when not connected */
    int ret = attention_plasticity_disconnect_bio_async(bridge);
    EXPECT_EQ(ret, 0);
}

//=============================================================================
// Weight Bounds Tests
//=============================================================================

TEST_F(AttentionPlasticityBridgeTest, WeightBoundsRespected) {
    attention_plasticity_register_synapse(bridge, 1, ATTENTION_SYNAPSE_QUERY_KEY, 0, 0.95f);
    attention_plasticity_register_synapse(bridge, 2, ATTENTION_SYNAPSE_QUERY_KEY, 1, 0.95f);

    /* Try to trigger massive LTP to exceed bounds */
    for (int i = 0; i < 100; i++) {
        attention_plasticity_shift(bridge, 0, 1, 1.0f, i * 1000);
    }

    attention_plasticity_synapse_t state;
    attention_plasticity_get_synapse(bridge, 2, &state);

    EXPECT_LE(state.weight, 1.0f) << "Weight should not exceed max";
    EXPECT_GE(state.weight, 0.0f) << "Weight should not go below min";
}

TEST_F(AttentionPlasticityBridgeTest, WeightBoundsLowerBound) {
    attention_plasticity_register_synapse(bridge, 1, ATTENTION_SYNAPSE_QUERY_KEY, 0, 0.05f);
    attention_plasticity_register_synapse(bridge, 2, ATTENTION_SYNAPSE_QUERY_KEY, 1, 0.05f);

    /* Focus on source head */
    attention_plasticity_focus(bridge, 0, 1.0f, 1000);

    /* Try to trigger massive LTD to go below bounds */
    for (int i = 0; i < 100; i++) {
        attention_plasticity_shift(bridge, 0, 1, 1.0f, (i + 1) * 10000);
    }

    attention_plasticity_synapse_t state;
    attention_plasticity_get_synapse(bridge, 1, &state);

    EXPECT_GE(state.weight, 0.0f) << "Weight should not go below min";
}

//=============================================================================
// Synapse Capacity Tests
//=============================================================================

TEST_F(AttentionPlasticityBridgeTest, MaxSynapseCapacity) {
    /* Register up to max synapses */
    int registered = 0;
    for (uint32_t i = 0; i < ATTENTION_PLASTICITY_MAX_SYNAPSES + 10; i++) {
        int ret = attention_plasticity_register_synapse(
            bridge, i, ATTENTION_SYNAPSE_QUERY_KEY,
            i % ATTENTION_PLASTICITY_MAX_HEADS, 0.5f);
        if (ret == 0) {
            registered++;
        }
    }

    EXPECT_EQ(registered, ATTENTION_PLASTICITY_MAX_SYNAPSES)
        << "Should register exactly max synapses";
}

//=============================================================================
// STDP Learning Rules Tests
//=============================================================================

TEST_F(AttentionPlasticityBridgeTest, STDPDecaysWithTimeDifference) {
    /* Larger time differences should produce smaller weight changes */
    attention_plasticity_register_synapse(bridge, 1, ATTENTION_SYNAPSE_QUERY_KEY, 0, 0.5f);
    attention_plasticity_register_synapse(bridge, 2, ATTENTION_SYNAPSE_QUERY_KEY, 0, 0.5f);
    attention_plasticity_register_synapse(bridge, 3, ATTENTION_SYNAPSE_QUERY_KEY, 1, 0.5f);

    /* Focus then shift with small delay */
    attention_plasticity_focus(bridge, 0, 1.0f, 1000000);
    attention_plasticity_shift(bridge, 0, 1, 1.0f, 1005000);  /* 5ms delay */

    attention_plasticity_synapse_t state_small;
    attention_plasticity_get_synapse(bridge, 3, &state_small);
    float change_small = state_small.weight - 0.5f;

    /* Reset and try with larger delay */
    attention_plasticity_reset(bridge);
    attention_plasticity_register_synapse(bridge, 4, ATTENTION_SYNAPSE_QUERY_KEY, 0, 0.5f);
    attention_plasticity_register_synapse(bridge, 5, ATTENTION_SYNAPSE_QUERY_KEY, 1, 0.5f);

    attention_plasticity_focus(bridge, 0, 1.0f, 1000000);
    attention_plasticity_shift(bridge, 0, 1, 1.0f, 1030000);  /* 30ms delay */

    attention_plasticity_synapse_t state_large;
    attention_plasticity_get_synapse(bridge, 5, &state_large);
    float change_large = state_large.weight - 0.5f;

    /* Both should be positive LTP, but smaller delay should give larger change */
    EXPECT_GT(change_small, 0.0f);
    EXPECT_GT(change_large, 0.0f);
}

//=============================================================================
// BCM Metaplasticity Tests
//=============================================================================

TEST_F(AttentionPlasticityBridgeTest, BCMThresholdUpdates) {
    attention_plasticity_register_synapse(bridge, 1, ATTENTION_SYNAPSE_QUERY_KEY, 0, 0.5f);

    attention_plasticity_synapse_t state_before;
    attention_plasticity_get_synapse(bridge, 1, &state_before);
    float threshold_before = state_before.bcm_threshold;

    /* Generate activity */
    for (int i = 0; i < 10; i++) {
        attention_plasticity_focus(bridge, 0, 1.0f, i * 1000000);
        attention_plasticity_update(bridge, 100.0f);
    }

    attention_plasticity_synapse_t state_after;
    attention_plasticity_get_synapse(bridge, 1, &state_after);

    /* BCM threshold should slide based on activity */
    EXPECT_NE(state_after.bcm_threshold, threshold_before)
        << "BCM threshold should adapt to activity";
}

//=============================================================================
// Null Parameter Handling Summary
//=============================================================================

TEST_F(AttentionPlasticityBridgeTest, NullBridgeHandling) {
    attention_plasticity_synapse_t synapse;
    attention_plasticity_bridge_state_t state;
    attention_plasticity_stats_t stats;
    float bias;
    float modulation[16];

    EXPECT_EQ(attention_plasticity_reset(nullptr), -1);
    EXPECT_EQ(attention_plasticity_register_synapse(nullptr, 0, ATTENTION_SYNAPSE_QUERY_KEY, 0, 0), -1);
    EXPECT_EQ(attention_plasticity_unregister_synapse(nullptr, 0), -1);
    EXPECT_EQ(attention_plasticity_get_synapse(nullptr, 0, &synapse), -1);
    EXPECT_EQ(attention_plasticity_focus(nullptr, 0, 0, 0), -1);
    EXPECT_EQ(attention_plasticity_shift(nullptr, 0, 1, 0, 0), -1);
    EXPECT_EQ(attention_plasticity_salience(nullptr, modulation, 1, 0), -1);
    EXPECT_EQ(attention_plasticity_reward(nullptr, 0, 0), -1);
    EXPECT_EQ(attention_plasticity_habituation_trial(nullptr, 0, 0), -1);
    EXPECT_EQ(attention_plasticity_novelty(nullptr, 0, 0, 0), -1);
    EXPECT_EQ(attention_plasticity_update(nullptr, 0), -1);
    EXPECT_EQ(attention_plasticity_consolidate(nullptr), -1);
    EXPECT_EQ(attention_plasticity_get_bias(nullptr, 0, &bias), -1);
    EXPECT_EQ(attention_plasticity_get_modulation(nullptr, modulation, 16), -1);
    EXPECT_FLOAT_EQ(attention_plasticity_get_habituation(nullptr, 0), -1.0f);
    EXPECT_FLOAT_EQ(attention_plasticity_get_novelty_score(nullptr, 0), -1.0f);
    EXPECT_FLOAT_EQ(attention_plasticity_get_sensitivity(nullptr, 0), -1.0f);
    EXPECT_EQ(attention_plasticity_get_state(nullptr, &state), -1);
    EXPECT_EQ(attention_plasticity_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(attention_plasticity_set_weight_callback(nullptr, test_weight_callback, nullptr), -1);
    EXPECT_EQ(attention_plasticity_set_shift_callback(nullptr, test_shift_callback, nullptr), -1);
    EXPECT_EQ(attention_plasticity_set_attention_modulation(nullptr, 0), -1);
    EXPECT_EQ(attention_plasticity_set_salience_modulation(nullptr, 0), -1);
    EXPECT_EQ(attention_plasticity_connect_bio_async(nullptr), -1);
    EXPECT_EQ(attention_plasticity_disconnect_bio_async(nullptr), -1);
    EXPECT_FALSE(attention_plasticity_is_bio_async_connected(nullptr));
}

TEST_F(AttentionPlasticityBridgeTest, NullOutputParameterHandling) {
    EXPECT_EQ(attention_plasticity_get_synapse(bridge, 0, nullptr), -1);
    EXPECT_EQ(attention_plasticity_get_bias(bridge, 0, nullptr), -1);
    EXPECT_EQ(attention_plasticity_get_modulation(bridge, nullptr, 16), -1);
    EXPECT_EQ(attention_plasticity_get_state(bridge, nullptr), -1);
    EXPECT_EQ(attention_plasticity_get_stats(bridge, nullptr), -1);
}

//=============================================================================
// Integration Test - Full Learning Cycle
//=============================================================================

TEST_F(AttentionPlasticityBridgeTest, FullLearningCycle) {
    /* Register synapses for two heads */
    for (uint32_t i = 0; i < 5; i++) {
        attention_plasticity_register_synapse(bridge, i, ATTENTION_SYNAPSE_QUERY_KEY, 0, 0.5f);
        attention_plasticity_register_synapse(bridge, 100 + i, ATTENTION_SYNAPSE_QUERY_KEY, 1, 0.5f);
    }

    /* 1. Focus on head 0 */
    attention_plasticity_focus(bridge, 0, 1.0f, 1000000);

    /* 2. Apply salience */
    float salience[] = {0.8f, 0.8f, 0.8f};
    attention_plasticity_salience(bridge, salience, 3, 1001000);

    /* 3. Shift attention to head 1 */
    attention_plasticity_shift(bridge, 0, 1, 0.9f, 1010000);

    /* 4. Apply positive reward */
    attention_plasticity_reward(bridge, 0.5f, 1020000);

    /* 5. Update */
    attention_plasticity_update(bridge, 10.0f);

    /* 6. Apply novelty */
    attention_plasticity_novelty(bridge, 1, 0.8f, 1030000);

    /* 7. Run habituation trials on head 0 */
    for (int i = 0; i < 5; i++) {
        attention_plasticity_habituation_trial(bridge, 0, 2000000 + i * 1000000);
    }

    /* 8. Update again */
    attention_plasticity_update(bridge, 100.0f);

    /* Verify stats */
    attention_plasticity_stats_t stats;
    attention_plasticity_get_stats(bridge, &stats);

    EXPECT_EQ(stats.total_focus_events, 1u);
    EXPECT_EQ(stats.total_shift_events, 1u);
    EXPECT_EQ(stats.habituation_events, 5u);
    EXPECT_EQ(stats.novelty_events, 1u);
    EXPECT_GT(stats.total_reward, 0.0f);

    /* Verify head states */
    float hab0 = attention_plasticity_get_habituation(bridge, 0);
    float hab1 = attention_plasticity_get_habituation(bridge, 1);
    EXPECT_GT(hab0, hab1) << "Head 0 should be more habituated";

    float nov1 = attention_plasticity_get_novelty_score(bridge, 1);
    EXPECT_FLOAT_EQ(nov1, 0.8f);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
