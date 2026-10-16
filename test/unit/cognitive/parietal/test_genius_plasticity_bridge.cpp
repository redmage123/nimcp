/**
 * @file test_genius_plasticity_bridge.cpp
 * @brief Unit tests for Mathematical Genius-Plasticity Bridge
 * @date 2026-01-24
 */

#include <gtest/gtest.h>
#include <cmath>

extern "C" {
#include "cognitive/parietal/nimcp_genius_plasticity_bridge.h"
}

class GeniusPlasticityBridgeTest : public ::testing::Test {
protected:
    genius_plasticity_config_t config;
    genius_plasticity_bridge_t* bridge = nullptr;

    void SetUp() override {
        config = genius_plasticity_config_default();
    }

    void TearDown() override {
        if (bridge) {
            genius_plasticity_destroy(bridge);
            bridge = nullptr;
        }
    }
};

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST_F(GeniusPlasticityBridgeTest, DefaultConfigSetsReasonableDefaults) {
    genius_plasticity_config_t cfg = genius_plasticity_config_default();

    EXPECT_EQ(cfg.base_learning_rate, GENIUS_PLASTICITY_DEFAULT_LR);
    EXPECT_GT(cfg.stdp_tau_plus_ms, 0.0f);
    EXPECT_GT(cfg.stdp_tau_minus_ms, 0.0f);
    EXPECT_GT(cfg.stdp_a_plus, 0.0f);
    EXPECT_GT(cfg.stdp_a_minus, 0.0f);
    EXPECT_GT(cfg.bcm_tau_ms, 0.0f);
    EXPECT_GT(cfg.proof_success_boost, 0.0f);
    EXPECT_GT(cfg.breakthrough_boost, 0.0f);
    EXPECT_TRUE(cfg.protect_pattern_recognition);
    EXPECT_TRUE(cfg.protect_intuition);
    EXPECT_EQ(cfg.max_synapses, GENIUS_PLASTICITY_MAX_SYNAPSES);
}

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(GeniusPlasticityBridgeTest, CreateWithNullConfigUsesDefaults) {
    bridge = genius_plasticity_create(nullptr);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(GeniusPlasticityBridgeTest, CreateWithValidConfigSucceeds) {
    bridge = genius_plasticity_create(&config);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(GeniusPlasticityBridgeTest, DestroyNullIsSafe) {
    genius_plasticity_destroy(nullptr);
    // Should not crash
}

TEST_F(GeniusPlasticityBridgeTest, ResetSucceeds) {
    bridge = genius_plasticity_create(&config);
    ASSERT_NE(bridge, nullptr);

    int result = genius_plasticity_reset(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(GeniusPlasticityBridgeTest, ResetNullFails) {
    int result = genius_plasticity_reset(nullptr);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Synapse Management Tests
 * ============================================================================ */

TEST_F(GeniusPlasticityBridgeTest, RegisterSynapseSucceeds) {
    bridge = genius_plasticity_create(&config);
    ASSERT_NE(bridge, nullptr);

    int result = genius_plasticity_register_synapse(
        bridge, 1, GENIUS_SYNAPSE_PROOF_STEP, 0.5f, GENIUS_MODE_GAUSS);
    EXPECT_EQ(result, 0);
}

TEST_F(GeniusPlasticityBridgeTest, RegisterSynapseNullBridgeFails) {
    int result = genius_plasticity_register_synapse(
        nullptr, 1, GENIUS_SYNAPSE_PROOF_STEP, 0.5f, GENIUS_MODE_GAUSS);
    EXPECT_EQ(result, -1);
}

TEST_F(GeniusPlasticityBridgeTest, RegisterDuplicateSynapseFails) {
    bridge = genius_plasticity_create(&config);
    ASSERT_NE(bridge, nullptr);

    genius_plasticity_register_synapse(bridge, 1, GENIUS_SYNAPSE_PROOF_STEP, 0.5f, GENIUS_MODE_GAUSS);
    int result = genius_plasticity_register_synapse(bridge, 1, GENIUS_SYNAPSE_ANALOGY, 0.6f, GENIUS_MODE_ERDOS);
    EXPECT_EQ(result, -1);
}

TEST_F(GeniusPlasticityBridgeTest, UnregisterSynapseSucceeds) {
    bridge = genius_plasticity_create(&config);
    ASSERT_NE(bridge, nullptr);

    genius_plasticity_register_synapse(bridge, 1, GENIUS_SYNAPSE_PROOF_STEP, 0.5f, GENIUS_MODE_GAUSS);
    int result = genius_plasticity_unregister_synapse(bridge, 1);
    EXPECT_EQ(result, 0);
}

TEST_F(GeniusPlasticityBridgeTest, UnregisterNonexistentSynapseFails) {
    bridge = genius_plasticity_create(&config);
    ASSERT_NE(bridge, nullptr);

    int result = genius_plasticity_unregister_synapse(bridge, 999);
    EXPECT_EQ(result, -1);
}

TEST_F(GeniusPlasticityBridgeTest, GetSynapseSucceeds) {
    bridge = genius_plasticity_create(&config);
    ASSERT_NE(bridge, nullptr);

    genius_plasticity_register_synapse(bridge, 42, GENIUS_SYNAPSE_CONJECTURE, 0.7f, GENIUS_MODE_NEWTON);

    genius_plasticity_synapse_t synapse;
    int result = genius_plasticity_get_synapse(bridge, 42, &synapse);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(synapse.synapse_id, 42u);
    EXPECT_EQ(synapse.type, GENIUS_SYNAPSE_CONJECTURE);
    EXPECT_FLOAT_EQ(synapse.weight, 0.7f);
    EXPECT_EQ(synapse.associated_mode, GENIUS_MODE_NEWTON);
}

TEST_F(GeniusPlasticityBridgeTest, ProtectSynapseSucceeds) {
    bridge = genius_plasticity_create(&config);
    ASSERT_NE(bridge, nullptr);

    genius_plasticity_register_synapse(bridge, 1, GENIUS_SYNAPSE_PROOF_STEP, 0.5f, GENIUS_MODE_GAUSS);

    int result = genius_plasticity_protect_synapse(bridge, 1, true);
    EXPECT_EQ(result, 0);

    genius_plasticity_synapse_t synapse;
    genius_plasticity_get_synapse(bridge, 1, &synapse);
    EXPECT_TRUE(synapse.is_protected);
}

TEST_F(GeniusPlasticityBridgeTest, PatternRecognitionSynapseAutoProtected) {
    bridge = genius_plasticity_create(&config);
    ASSERT_NE(bridge, nullptr);

    genius_plasticity_register_synapse(
        bridge, 1, GENIUS_SYNAPSE_PATTERN_RECOGNITION, 0.5f, GENIUS_MODE_GAUSS);

    genius_plasticity_synapse_t synapse;
    genius_plasticity_get_synapse(bridge, 1, &synapse);
    EXPECT_TRUE(synapse.is_protected);
}

/* ============================================================================
 * Learning Tests
 * ============================================================================ */

TEST_F(GeniusPlasticityBridgeTest, LearnProofSuccessModifiesWeight) {
    bridge = genius_plasticity_create(&config);
    ASSERT_NE(bridge, nullptr);

    genius_plasticity_register_synapse(bridge, 1, GENIUS_SYNAPSE_PROOF_STEP, 0.5f, GENIUS_MODE_GAUSS);

    genius_plasticity_synapse_t before;
    genius_plasticity_get_synapse(bridge, 1, &before);

    int result = genius_plasticity_learn(bridge, GENIUS_LEARN_PROOF_SUCCESS, 0.8f, 1, 1.0f);
    EXPECT_EQ(result, 0);

    genius_plasticity_synapse_t after;
    genius_plasticity_get_synapse(bridge, 1, &after);
    EXPECT_GT(after.weight, before.weight);
}

TEST_F(GeniusPlasticityBridgeTest, LearnProofFailureReducesWeight) {
    bridge = genius_plasticity_create(&config);
    ASSERT_NE(bridge, nullptr);

    genius_plasticity_register_synapse(bridge, 1, GENIUS_SYNAPSE_PROOF_STEP, 0.5f, GENIUS_MODE_GAUSS);

    genius_plasticity_synapse_t before;
    genius_plasticity_get_synapse(bridge, 1, &before);

    int result = genius_plasticity_learn(bridge, GENIUS_LEARN_PROOF_FAILURE, 0.5f, 1, 1.0f);
    EXPECT_EQ(result, 0);

    genius_plasticity_synapse_t after;
    genius_plasticity_get_synapse(bridge, 1, &after);
    EXPECT_LT(after.weight, before.weight);
}

TEST_F(GeniusPlasticityBridgeTest, LearnNullBridgeFails) {
    int result = genius_plasticity_learn(nullptr, GENIUS_LEARN_PROOF_SUCCESS, 0.8f, 1, 1.0f);
    EXPECT_EQ(result, -1);
}

TEST_F(GeniusPlasticityBridgeTest, LearnNonexistentSynapseFails) {
    bridge = genius_plasticity_create(&config);
    ASSERT_NE(bridge, nullptr);

    int result = genius_plasticity_learn(bridge, GENIUS_LEARN_PROOF_SUCCESS, 0.8f, 999, 1.0f);
    EXPECT_EQ(result, -1);
}

TEST_F(GeniusPlasticityBridgeTest, ApplySTDPReturnsWeightChange) {
    bridge = genius_plasticity_create(&config);
    ASSERT_NE(bridge, nullptr);

    genius_plasticity_register_synapse(bridge, 1, GENIUS_SYNAPSE_PROOF_STEP, 0.5f, GENIUS_MODE_GAUSS);

    // Post after pre (LTP)
    float dw = genius_plasticity_apply_stdp(bridge, 1, 0.0f, 10.0f);
    EXPECT_FALSE(std::isnan(dw));
    EXPECT_GT(dw, 0.0f);  // Should be potentiation
}

TEST_F(GeniusPlasticityBridgeTest, ApplySTDPLTDWhenPreAfterPost) {
    bridge = genius_plasticity_create(&config);
    ASSERT_NE(bridge, nullptr);

    genius_plasticity_register_synapse(bridge, 1, GENIUS_SYNAPSE_PROOF_STEP, 0.5f, GENIUS_MODE_GAUSS);

    // Pre after post (LTD)
    float dw = genius_plasticity_apply_stdp(bridge, 1, 10.0f, 0.0f);
    EXPECT_FALSE(std::isnan(dw));
    EXPECT_LT(dw, 0.0f);  // Should be depression
}

TEST_F(GeniusPlasticityBridgeTest, ApplyProofRewardSucceeds) {
    bridge = genius_plasticity_create(&config);
    ASSERT_NE(bridge, nullptr);

    genius_plasticity_register_synapse(bridge, 1, GENIUS_SYNAPSE_PROOF_STEP, 0.5f, GENIUS_MODE_GAUSS);

    int result = genius_plasticity_apply_proof_reward(bridge, 0.9f, 0.8f);
    EXPECT_EQ(result, 0);
}

TEST_F(GeniusPlasticityBridgeTest, ApplyInsightRewardSucceeds) {
    bridge = genius_plasticity_create(&config);
    ASSERT_NE(bridge, nullptr);

    genius_plasticity_register_synapse(bridge, 1, GENIUS_SYNAPSE_PROOF_STEP, 0.5f, GENIUS_MODE_GAUSS);

    int result = genius_plasticity_apply_insight_reward(bridge, 0.7f, GENIUS_MODE_GAUSS);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * BCM and Homeostatic Tests
 * ============================================================================ */

TEST_F(GeniusPlasticityBridgeTest, UpdateBCMSucceeds) {
    bridge = genius_plasticity_create(&config);
    ASSERT_NE(bridge, nullptr);

    genius_plasticity_register_synapse(bridge, 1, GENIUS_SYNAPSE_PROOF_STEP, 0.5f, GENIUS_MODE_GAUSS);

    int result = genius_plasticity_update_bcm(bridge, 10.0f);
    EXPECT_EQ(result, 0);
}

TEST_F(GeniusPlasticityBridgeTest, HomeostaticUpdateSucceeds) {
    bridge = genius_plasticity_create(&config);
    ASSERT_NE(bridge, nullptr);

    genius_plasticity_register_synapse(bridge, 1, GENIUS_SYNAPSE_PROOF_STEP, 0.5f, GENIUS_MODE_GAUSS);

    int result = genius_plasticity_homeostatic_update(bridge, 100.0f);
    EXPECT_EQ(result, 0);
}

TEST_F(GeniusPlasticityBridgeTest, UpdateTracesSucceeds) {
    bridge = genius_plasticity_create(&config);
    ASSERT_NE(bridge, nullptr);

    genius_plasticity_register_synapse(bridge, 1, GENIUS_SYNAPSE_PROOF_STEP, 0.5f, GENIUS_MODE_GAUSS);

    int result = genius_plasticity_update_traces(bridge, 10.0f);
    EXPECT_EQ(result, 0);
}

TEST_F(GeniusPlasticityBridgeTest, ConsolidateSucceeds) {
    bridge = genius_plasticity_create(&config);
    ASSERT_NE(bridge, nullptr);

    genius_plasticity_register_synapse(bridge, 1, GENIUS_SYNAPSE_PROOF_STEP, 0.8f, GENIUS_MODE_GAUSS);

    int result = genius_plasticity_consolidate(bridge);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * State Query Tests
 * ============================================================================ */

TEST_F(GeniusPlasticityBridgeTest, GetLearningStateSucceeds) {
    bridge = genius_plasticity_create(&config);
    ASSERT_NE(bridge, nullptr);

    genius_learning_state_t state;
    int result = genius_plasticity_get_learning_state(bridge, &state);
    EXPECT_EQ(result, 0);
    EXPECT_GE(state.pattern_sensitivity, 0.0f);
    EXPECT_LE(state.pattern_sensitivity, 1.0f);
}

TEST_F(GeniusPlasticityBridgeTest, GetModeSkillSucceeds) {
    bridge = genius_plasticity_create(&config);
    ASSERT_NE(bridge, nullptr);

    float skill = genius_plasticity_get_mode_skill(bridge, GENIUS_MODE_GAUSS);
    EXPECT_GE(skill, 0.0f);
    EXPECT_LE(skill, 1.0f);
}

TEST_F(GeniusPlasticityBridgeTest, GetModeSkillInvalidModeFails) {
    bridge = genius_plasticity_create(&config);
    ASSERT_NE(bridge, nullptr);

    float skill = genius_plasticity_get_mode_skill(bridge, (genius_mode_t)99);
    EXPECT_LT(skill, 0.0f);
}

TEST_F(GeniusPlasticityBridgeTest, GetStateSucceeds) {
    bridge = genius_plasticity_create(&config);
    ASSERT_NE(bridge, nullptr);

    genius_plasticity_bridge_state_t state;
    int result = genius_plasticity_get_state(bridge, &state);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(state.state, GENIUS_PLASTICITY_STATE_IDLE);
}

TEST_F(GeniusPlasticityBridgeTest, GetStatsSucceeds) {
    bridge = genius_plasticity_create(&config);
    ASSERT_NE(bridge, nullptr);

    genius_plasticity_stats_t stats;
    int result = genius_plasticity_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
}

TEST_F(GeniusPlasticityBridgeTest, ResetStatsSucceeds) {
    bridge = genius_plasticity_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Generate some stats
    genius_plasticity_register_synapse(bridge, 1, GENIUS_SYNAPSE_PROOF_STEP, 0.5f, GENIUS_MODE_GAUSS);
    genius_plasticity_learn(bridge, GENIUS_LEARN_PROOF_SUCCESS, 0.8f, 1, 1.0f);

    int result = genius_plasticity_reset_stats(bridge);
    EXPECT_EQ(result, 0);

    genius_plasticity_stats_t stats;
    genius_plasticity_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_learning_events, 0u);
}

/* ============================================================================
 * Callback Tests
 * ============================================================================ */

static bool learn_callback_called = false;
static void test_learn_callback(genius_plasticity_bridge_t*, genius_learn_event_t, float, void*) {
    learn_callback_called = true;
}

TEST_F(GeniusPlasticityBridgeTest, RegisterLearnCallbackSucceeds) {
    bridge = genius_plasticity_create(&config);
    ASSERT_NE(bridge, nullptr);

    int result = genius_plasticity_register_learn_callback(bridge, test_learn_callback, nullptr);
    EXPECT_EQ(result, 0);
}

TEST_F(GeniusPlasticityBridgeTest, LearnCallbackInvoked) {
    bridge = genius_plasticity_create(&config);
    ASSERT_NE(bridge, nullptr);

    learn_callback_called = false;
    genius_plasticity_register_learn_callback(bridge, test_learn_callback, nullptr);
    genius_plasticity_register_synapse(bridge, 1, GENIUS_SYNAPSE_PROOF_STEP, 0.5f, GENIUS_MODE_GAUSS);
    genius_plasticity_learn(bridge, GENIUS_LEARN_PROOF_SUCCESS, 0.8f, 1, 1.0f);

    EXPECT_TRUE(learn_callback_called);
}

/* ============================================================================
 * Bio-Async Tests
 * ============================================================================ */

TEST_F(GeniusPlasticityBridgeTest, BioAsyncInitiallyDisconnected) {
    bridge = genius_plasticity_create(&config);
    ASSERT_NE(bridge, nullptr);

    EXPECT_FALSE(genius_plasticity_is_bio_async_connected(bridge));
}

TEST_F(GeniusPlasticityBridgeTest, BioAsyncConnectSucceeds) {
    bridge = genius_plasticity_create(&config);
    ASSERT_NE(bridge, nullptr);

    int result = genius_plasticity_bio_async_connect(bridge);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(genius_plasticity_is_bio_async_connected(bridge));
}

/* ============================================================================
 * Integration Tests
 * ============================================================================ */

TEST_F(GeniusPlasticityBridgeTest, FullLearningWorkflowSucceeds) {
    bridge = genius_plasticity_create(&config);
    ASSERT_NE(bridge, nullptr);

    // 1. Register synapses for different types
    genius_plasticity_register_synapse(bridge, 1, GENIUS_SYNAPSE_PROOF_STEP, 0.5f, GENIUS_MODE_GAUSS);
    genius_plasticity_register_synapse(bridge, 2, GENIUS_SYNAPSE_CONJECTURE, 0.5f, GENIUS_MODE_ERDOS);
    genius_plasticity_register_synapse(bridge, 3, GENIUS_SYNAPSE_ANALOGY, 0.5f, GENIUS_MODE_NEWTON);

    // 2. Apply various learning events
    genius_plasticity_learn(bridge, GENIUS_LEARN_PROOF_SUCCESS, 0.9f, 1, 1.0f);
    genius_plasticity_learn(bridge, GENIUS_LEARN_CONJECTURE_VERIFIED, 0.7f, 2, 0.8f);
    genius_plasticity_learn(bridge, GENIUS_LEARN_ANALOGY_FOUND, 0.6f, 3, 0.7f);

    // 3. Apply STDP
    genius_plasticity_apply_stdp(bridge, 1, 0.0f, 5.0f);

    // 4. Update BCM and homeostatic
    genius_plasticity_update_bcm(bridge, 100.0f);
    genius_plasticity_homeostatic_update(bridge, 100.0f);

    // 5. Consolidate
    genius_plasticity_consolidate(bridge);

    // 6. Check learning state
    genius_learning_state_t learning;
    genius_plasticity_get_learning_state(bridge, &learning);
    EXPECT_GT(learning.proof_skill, 0.3f);  // Should have improved

    // 7. Check stats
    genius_plasticity_stats_t stats;
    genius_plasticity_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_learning_events, 3u);
    EXPECT_EQ(stats.proof_success_events, 1u);
}
