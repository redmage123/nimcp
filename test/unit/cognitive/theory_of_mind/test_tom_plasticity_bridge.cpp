/**
 * @file test_tom_plasticity_bridge.cpp
 * @brief Unit tests for Theory of Mind Plasticity Bridge
 */

#include <gtest/gtest.h>
#include <cmath>

#include "cognitive/theory_of_mind/nimcp_tom_plasticity_bridge.h"

class TOMPlasticityBridgeTest : public ::testing::Test {
protected:
    tom_plasticity_bridge_t* bridge = nullptr;

    void SetUp() override {
        tom_plasticity_config_t config = tom_plasticity_config_default();
        config.enable_bio_async = false;
        bridge = tom_plasticity_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            tom_plasticity_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(TOMPlasticityBridgeTest, CreateWithDefaults) {
    tom_plasticity_bridge_t* test_bridge = tom_plasticity_create(nullptr);
    ASSERT_NE(test_bridge, nullptr);
    tom_plasticity_destroy(test_bridge);
}

TEST_F(TOMPlasticityBridgeTest, CreateWithConfig) {
    tom_plasticity_config_t config = tom_plasticity_config_default();
    config.base_learning_rate = 0.05f;
    config.max_synapses = 128;
    tom_plasticity_bridge_t* test_bridge = tom_plasticity_create(&config);
    ASSERT_NE(test_bridge, nullptr);
    tom_plasticity_destroy(test_bridge);
}

TEST_F(TOMPlasticityBridgeTest, Reset) {
    EXPECT_EQ(tom_plasticity_reset(bridge), 0);
}

TEST_F(TOMPlasticityBridgeTest, ResetNull) {
    EXPECT_EQ(tom_plasticity_reset(nullptr), -1);
}

TEST_F(TOMPlasticityBridgeTest, DestroyNull) {
    tom_plasticity_destroy(nullptr); // Should not crash
}

//=============================================================================
// Default Config Tests
//=============================================================================

TEST_F(TOMPlasticityBridgeTest, DefaultConfigValues) {
    tom_plasticity_config_t config = tom_plasticity_config_default();
    EXPECT_EQ(config.base_learning_rate, TOM_PLASTICITY_DEFAULT_LR);
    EXPECT_GT(config.stdp_tau_plus_ms, 0.0f);
    EXPECT_GT(config.stdp_tau_minus_ms, 0.0f);
    EXPECT_TRUE(config.protect_belief_patterns);
    EXPECT_TRUE(config.protect_perspective_patterns);
}

//=============================================================================
// Synapse Management Tests
//=============================================================================

TEST_F(TOMPlasticityBridgeTest, RegisterSynapse) {
    EXPECT_EQ(tom_plasticity_register_synapse(bridge, 1, TOM_SYNAPSE_BELIEF, 0.5f), 0);
}

TEST_F(TOMPlasticityBridgeTest, RegisterSynapseNull) {
    EXPECT_EQ(tom_plasticity_register_synapse(nullptr, 1, TOM_SYNAPSE_BELIEF, 0.5f), -1);
}

TEST_F(TOMPlasticityBridgeTest, RegisterDuplicateSynapse) {
    EXPECT_EQ(tom_plasticity_register_synapse(bridge, 1, TOM_SYNAPSE_BELIEF, 0.5f), 0);
    EXPECT_EQ(tom_plasticity_register_synapse(bridge, 1, TOM_SYNAPSE_INTENTION, 0.6f), -1);
}

TEST_F(TOMPlasticityBridgeTest, UnregisterSynapse) {
    EXPECT_EQ(tom_plasticity_register_synapse(bridge, 1, TOM_SYNAPSE_BELIEF, 0.5f), 0);
    EXPECT_EQ(tom_plasticity_unregister_synapse(bridge, 1), 0);
}

TEST_F(TOMPlasticityBridgeTest, UnregisterNonexistentSynapse) {
    EXPECT_EQ(tom_plasticity_unregister_synapse(bridge, 999), -1);
}

TEST_F(TOMPlasticityBridgeTest, GetSynapse) {
    EXPECT_EQ(tom_plasticity_register_synapse(bridge, 1, TOM_SYNAPSE_EMPATHY, 0.7f), 0);

    tom_plasticity_synapse_t synapse;
    EXPECT_EQ(tom_plasticity_get_synapse(bridge, 1, &synapse), 0);
    EXPECT_EQ(synapse.synapse_id, 1u);
    EXPECT_EQ(synapse.type, TOM_SYNAPSE_EMPATHY);
    EXPECT_NEAR(synapse.weight, 0.7f, 0.01f);
}

TEST_F(TOMPlasticityBridgeTest, GetSynapseNull) {
    EXPECT_EQ(tom_plasticity_get_synapse(nullptr, 1, nullptr), -1);
    tom_plasticity_synapse_t synapse;
    EXPECT_EQ(tom_plasticity_get_synapse(bridge, 1, &synapse), -1); // Not registered
}

TEST_F(TOMPlasticityBridgeTest, ProtectSynapse) {
    EXPECT_EQ(tom_plasticity_register_synapse(bridge, 1, TOM_SYNAPSE_SOCIAL, 0.5f), 0);
    EXPECT_EQ(tom_plasticity_protect_synapse(bridge, 1, true), 0);

    tom_plasticity_synapse_t synapse;
    EXPECT_EQ(tom_plasticity_get_synapse(bridge, 1, &synapse), 0);
    EXPECT_TRUE(synapse.is_protected);
}

TEST_F(TOMPlasticityBridgeTest, AutoProtectBeliefSynapse) {
    EXPECT_EQ(tom_plasticity_register_synapse(bridge, 1, TOM_SYNAPSE_BELIEF, 0.5f), 0);

    tom_plasticity_synapse_t synapse;
    EXPECT_EQ(tom_plasticity_get_synapse(bridge, 1, &synapse), 0);
    EXPECT_TRUE(synapse.is_protected); // Belief patterns are auto-protected
}

TEST_F(TOMPlasticityBridgeTest, AutoProtectPerspectiveSynapse) {
    EXPECT_EQ(tom_plasticity_register_synapse(bridge, 1, TOM_SYNAPSE_PERSPECTIVE, 0.5f), 0);

    tom_plasticity_synapse_t synapse;
    EXPECT_EQ(tom_plasticity_get_synapse(bridge, 1, &synapse), 0);
    EXPECT_TRUE(synapse.is_protected); // Perspective patterns are auto-protected
}

//=============================================================================
// Learning Tests
//=============================================================================

TEST_F(TOMPlasticityBridgeTest, LearnCorrectBelief) {
    EXPECT_EQ(tom_plasticity_register_synapse(bridge, 1, TOM_SYNAPSE_INTENTION, 0.5f), 0);
    EXPECT_EQ(tom_plasticity_learn(bridge, TOM_LEARN_CORRECT_BELIEF, 0.8f, 1, 1.0f), 0);

    tom_plasticity_synapse_t synapse;
    EXPECT_EQ(tom_plasticity_get_synapse(bridge, 1, &synapse), 0);
    EXPECT_GT(synapse.weight, 0.5f); // Weight should increase
}

TEST_F(TOMPlasticityBridgeTest, LearnFalseBeliefDetected) {
    EXPECT_EQ(tom_plasticity_register_synapse(bridge, 1, TOM_SYNAPSE_SOCIAL, 0.5f), 0);
    EXPECT_EQ(tom_plasticity_learn(bridge, TOM_LEARN_FALSE_BELIEF_DETECTED, 0.9f, 1, 1.0f), 0);

    tom_plasticity_stats_t stats;
    EXPECT_EQ(tom_plasticity_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.false_belief_detections, 1u);
}

TEST_F(TOMPlasticityBridgeTest, LearnProtectedSynapseBlocked) {
    EXPECT_EQ(tom_plasticity_register_synapse(bridge, 1, TOM_SYNAPSE_BELIEF, 0.5f), 0);
    // Belief synapse is auto-protected

    EXPECT_EQ(tom_plasticity_learn(bridge, TOM_LEARN_CORRECT_BELIEF, 0.8f, 1, 1.0f), 0);

    tom_plasticity_stats_t stats;
    EXPECT_EQ(tom_plasticity_get_stats(bridge, &stats), 0);
    EXPECT_GT(stats.protected_updates_blocked, 0u);
}

TEST_F(TOMPlasticityBridgeTest, LearnNull) {
    EXPECT_EQ(tom_plasticity_learn(nullptr, TOM_LEARN_CORRECT_BELIEF, 0.8f, 1, 1.0f), -1);
}

TEST_F(TOMPlasticityBridgeTest, LearnNonexistentSynapse) {
    EXPECT_EQ(tom_plasticity_learn(bridge, TOM_LEARN_CORRECT_BELIEF, 0.8f, 999, 1.0f), -1);
}

//=============================================================================
// STDP Tests
//=============================================================================

TEST_F(TOMPlasticityBridgeTest, ApplySTDPPotentiation) {
    EXPECT_EQ(tom_plasticity_register_synapse(bridge, 1, TOM_SYNAPSE_EMPATHY, 0.5f), 0);

    // Post after pre -> LTP
    float delta = tom_plasticity_apply_stdp(bridge, 1, 0.0f, 10.0f);
    EXPECT_GT(delta, 0.0f);
}

TEST_F(TOMPlasticityBridgeTest, ApplySTDPDepression) {
    EXPECT_EQ(tom_plasticity_register_synapse(bridge, 1, TOM_SYNAPSE_EMPATHY, 0.5f), 0);

    // Pre after post -> LTD
    float delta = tom_plasticity_apply_stdp(bridge, 1, 10.0f, 0.0f);
    EXPECT_LT(delta, 0.0f);
}

TEST_F(TOMPlasticityBridgeTest, ApplySTDPNull) {
    EXPECT_TRUE(std::isnan(tom_plasticity_apply_stdp(nullptr, 1, 0.0f, 10.0f)));
}

TEST_F(TOMPlasticityBridgeTest, ApplySTDPProtectedSynapse) {
    EXPECT_EQ(tom_plasticity_register_synapse(bridge, 1, TOM_SYNAPSE_BELIEF, 0.5f), 0);
    // Belief synapse is auto-protected

    float delta = tom_plasticity_apply_stdp(bridge, 1, 0.0f, 10.0f);
    EXPECT_FLOAT_EQ(delta, 0.0f);
}

//=============================================================================
// Reward Modulation Tests
//=============================================================================

TEST_F(TOMPlasticityBridgeTest, ApplyReward) {
    EXPECT_EQ(tom_plasticity_register_synapse(bridge, 1, TOM_SYNAPSE_EMPATHY, 0.5f), 0);
    EXPECT_EQ(tom_plasticity_apply_reward(bridge, 0.8f), 0);
}

TEST_F(TOMPlasticityBridgeTest, ApplyRewardNull) {
    EXPECT_EQ(tom_plasticity_apply_reward(nullptr, 0.8f), -1);
}

TEST_F(TOMPlasticityBridgeTest, ApplyRewardClamp) {
    EXPECT_EQ(tom_plasticity_register_synapse(bridge, 1, TOM_SYNAPSE_SOCIAL, 0.5f), 0);
    EXPECT_EQ(tom_plasticity_apply_reward(bridge, 2.0f), 0);  // Should clamp to 1.0
    EXPECT_EQ(tom_plasticity_apply_reward(bridge, -2.0f), 0); // Should clamp to -1.0
}

//=============================================================================
// BCM and Homeostatic Tests
//=============================================================================

TEST_F(TOMPlasticityBridgeTest, UpdateBCM) {
    EXPECT_EQ(tom_plasticity_register_synapse(bridge, 1, TOM_SYNAPSE_INTENTION, 0.5f), 0);
    EXPECT_EQ(tom_plasticity_update_bcm(bridge, 10.0f), 0);
}

TEST_F(TOMPlasticityBridgeTest, UpdateBCMNull) {
    EXPECT_EQ(tom_plasticity_update_bcm(nullptr, 10.0f), -1);
}

TEST_F(TOMPlasticityBridgeTest, UpdateBCMInvalidDt) {
    EXPECT_EQ(tom_plasticity_update_bcm(bridge, 0.0f), -1);
    EXPECT_EQ(tom_plasticity_update_bcm(bridge, -10.0f), -1);
}

TEST_F(TOMPlasticityBridgeTest, HomeostaticUpdate) {
    EXPECT_EQ(tom_plasticity_register_synapse(bridge, 1, TOM_SYNAPSE_SOCIAL, 0.5f), 0);
    EXPECT_EQ(tom_plasticity_homeostatic_update(bridge, 10.0f), 0);
}

TEST_F(TOMPlasticityBridgeTest, HomeostaticUpdateNull) {
    EXPECT_EQ(tom_plasticity_homeostatic_update(nullptr, 10.0f), -1);
}

TEST_F(TOMPlasticityBridgeTest, UpdateTraces) {
    EXPECT_EQ(tom_plasticity_register_synapse(bridge, 1, TOM_SYNAPSE_EMPATHY, 0.5f), 0);
    EXPECT_EQ(tom_plasticity_update_traces(bridge, 10.0f), 0);
}

TEST_F(TOMPlasticityBridgeTest, UpdateTracesNull) {
    EXPECT_EQ(tom_plasticity_update_traces(nullptr, 10.0f), -1);
}

TEST_F(TOMPlasticityBridgeTest, Consolidate) {
    EXPECT_EQ(tom_plasticity_register_synapse(bridge, 1, TOM_SYNAPSE_EMPATHY, 0.5f), 0);
    EXPECT_EQ(tom_plasticity_consolidate(bridge), 0);
}

TEST_F(TOMPlasticityBridgeTest, ConsolidateNull) {
    EXPECT_EQ(tom_plasticity_consolidate(nullptr), -1);
}

//=============================================================================
// State Query Tests
//=============================================================================

TEST_F(TOMPlasticityBridgeTest, GetCalibrationState) {
    tom_calibration_state_t state;
    EXPECT_EQ(tom_plasticity_get_calibration_state(bridge, &state), 0);
    EXPECT_GT(state.belief_sensitivity, 0.0f);
    EXPECT_GT(state.empathy_strength, 0.0f);
}

TEST_F(TOMPlasticityBridgeTest, GetCalibrationStateNull) {
    EXPECT_EQ(tom_plasticity_get_calibration_state(nullptr, nullptr), -1);
    tom_calibration_state_t state;
    EXPECT_EQ(tom_plasticity_get_calibration_state(nullptr, &state), -1);
    EXPECT_EQ(tom_plasticity_get_calibration_state(bridge, nullptr), -1);
}

TEST_F(TOMPlasticityBridgeTest, GetBridgeState) {
    tom_plasticity_bridge_state_t state;
    EXPECT_EQ(tom_plasticity_get_state(bridge, &state), 0);
    EXPECT_EQ(state.state, TOM_PLASTICITY_STATE_IDLE);
    EXPECT_EQ(state.active_synapses, 0u);
}

TEST_F(TOMPlasticityBridgeTest, GetStats) {
    tom_plasticity_stats_t stats;
    EXPECT_EQ(tom_plasticity_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.total_learning_events, 0u);
}

TEST_F(TOMPlasticityBridgeTest, ResetStats) {
    EXPECT_EQ(tom_plasticity_register_synapse(bridge, 1, TOM_SYNAPSE_EMPATHY, 0.5f), 0);
    EXPECT_EQ(tom_plasticity_learn(bridge, TOM_LEARN_EMPATHY_ACCURATE, 0.8f, 1, 1.0f), 0);

    tom_plasticity_stats_t stats;
    EXPECT_EQ(tom_plasticity_get_stats(bridge, &stats), 0);
    EXPECT_GT(stats.total_learning_events, 0u);

    EXPECT_EQ(tom_plasticity_reset_stats(bridge), 0);
    EXPECT_EQ(tom_plasticity_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.total_learning_events, 0u);
}

//=============================================================================
// Callback Tests
//=============================================================================

static int learn_callback_count = 0;
static void test_learn_callback(tom_plasticity_bridge_t*, tom_learn_event_t, float, void*) {
    learn_callback_count++;
}

TEST_F(TOMPlasticityBridgeTest, RegisterLearnCallback) {
    EXPECT_EQ(tom_plasticity_register_learn_callback(bridge, test_learn_callback, nullptr), 0);
}

TEST_F(TOMPlasticityBridgeTest, RegisterCalibrationCallback) {
    EXPECT_EQ(tom_plasticity_register_calibration_callback(bridge, nullptr, nullptr), 0);
}

TEST_F(TOMPlasticityBridgeTest, LearnCallbackInvoked) {
    learn_callback_count = 0;
    EXPECT_EQ(tom_plasticity_register_learn_callback(bridge, test_learn_callback, nullptr), 0);
    EXPECT_EQ(tom_plasticity_register_synapse(bridge, 1, TOM_SYNAPSE_EMPATHY, 0.5f), 0);
    EXPECT_EQ(tom_plasticity_learn(bridge, TOM_LEARN_EMPATHY_ACCURATE, 0.8f, 1, 1.0f), 0);
    EXPECT_EQ(learn_callback_count, 1);
}

//=============================================================================
// Bio-Async Tests
//=============================================================================

TEST_F(TOMPlasticityBridgeTest, BioAsyncNotEnabled) {
    EXPECT_EQ(tom_plasticity_bio_async_connect(bridge), -1);
    EXPECT_FALSE(tom_plasticity_is_bio_async_connected(bridge));
}

TEST_F(TOMPlasticityBridgeTest, BioAsyncDisconnectSafe) {
    EXPECT_EQ(tom_plasticity_bio_async_disconnect(bridge), 0);
}

//=============================================================================
// Integration Workflow Tests
//=============================================================================

TEST_F(TOMPlasticityBridgeTest, FullWorkflow) {
    // Register synapses for social cognition learning
    EXPECT_EQ(tom_plasticity_register_synapse(bridge, 1, TOM_SYNAPSE_EMPATHY, 0.5f), 0);
    EXPECT_EQ(tom_plasticity_register_synapse(bridge, 2, TOM_SYNAPSE_SOCIAL, 0.5f), 0);
    EXPECT_EQ(tom_plasticity_register_synapse(bridge, 3, TOM_SYNAPSE_INTENTION, 0.5f), 0);

    // Simulate social learning
    EXPECT_EQ(tom_plasticity_learn(bridge, TOM_LEARN_EMPATHY_ACCURATE, 0.9f, 1, 1.0f), 0);
    EXPECT_EQ(tom_plasticity_learn(bridge, TOM_LEARN_SOCIAL_CONTEXT_CORRECT, 0.8f, 2, 1.0f), 0);
    EXPECT_EQ(tom_plasticity_learn(bridge, TOM_LEARN_INTENTION_CORRECT, 0.7f, 3, 1.0f), 0);

    // Apply reward
    EXPECT_EQ(tom_plasticity_apply_reward(bridge, 0.5f), 0);

    // Update dynamics
    EXPECT_EQ(tom_plasticity_update_bcm(bridge, 10.0f), 0);
    EXPECT_EQ(tom_plasticity_homeostatic_update(bridge, 10.0f), 0);

    // Consolidate
    EXPECT_EQ(tom_plasticity_consolidate(bridge), 0);

    // Verify stats
    tom_plasticity_stats_t stats;
    EXPECT_EQ(tom_plasticity_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.total_learning_events, 3u);
    EXPECT_GT(stats.empathy_accurate_events, 0u);
}

TEST_F(TOMPlasticityBridgeTest, MultipleLearningEvents) {
    EXPECT_EQ(tom_plasticity_register_synapse(bridge, 1, TOM_SYNAPSE_EMPATHY, 0.5f), 0);

    for (int i = 0; i < 10; i++) {
        tom_learn_event_t event = (i % 2 == 0) ? TOM_LEARN_EMPATHY_ACCURATE : TOM_LEARN_EMPATHY_ERROR;
        EXPECT_EQ(tom_plasticity_learn(bridge, event, 0.5f, 1, 1.0f), 0);
    }

    tom_plasticity_stats_t stats;
    EXPECT_EQ(tom_plasticity_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.total_learning_events, 10u);
}
