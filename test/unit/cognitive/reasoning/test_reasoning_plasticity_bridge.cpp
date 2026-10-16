/**
 * @file test_reasoning_plasticity_bridge.cpp
 * @brief Unit tests for Reasoning Plasticity Bridge
 */

#include <gtest/gtest.h>
#include <cmath>

#include "cognitive/reasoning/nimcp_reasoning_plasticity_bridge.h"

class ReasoningPlasticityBridgeTest : public ::testing::Test {
protected:
    reasoning_plasticity_bridge_t* bridge = nullptr;

    void SetUp() override {
        reasoning_plasticity_config_t config = reasoning_plasticity_config_default();
        config.enable_bio_async = false;
        bridge = reasoning_plasticity_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            reasoning_plasticity_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(ReasoningPlasticityBridgeTest, CreateWithDefaults) {
    reasoning_plasticity_bridge_t* test_bridge = reasoning_plasticity_create(nullptr);
    ASSERT_NE(test_bridge, nullptr);
    reasoning_plasticity_destroy(test_bridge);
}

TEST_F(ReasoningPlasticityBridgeTest, CreateWithConfig) {
    reasoning_plasticity_config_t config = reasoning_plasticity_config_default();
    config.base_learning_rate = 0.005f;
    config.max_synapses = 128;
    reasoning_plasticity_bridge_t* test_bridge = reasoning_plasticity_create(&config);
    ASSERT_NE(test_bridge, nullptr);
    reasoning_plasticity_destroy(test_bridge);
}

TEST_F(ReasoningPlasticityBridgeTest, Reset) {
    EXPECT_EQ(reasoning_plasticity_reset(bridge), 0);
}

TEST_F(ReasoningPlasticityBridgeTest, ResetNull) {
    EXPECT_EQ(reasoning_plasticity_reset(nullptr), -1);
}

TEST_F(ReasoningPlasticityBridgeTest, DestroyNull) {
    reasoning_plasticity_destroy(nullptr); // Should not crash
}

//=============================================================================
// Default Config Tests
//=============================================================================

TEST_F(ReasoningPlasticityBridgeTest, DefaultConfigValues) {
    reasoning_plasticity_config_t config = reasoning_plasticity_config_default();
    EXPECT_EQ(config.base_learning_rate, REASONING_PLASTICITY_DEFAULT_LR);
    EXPECT_EQ(config.max_synapses, REASONING_PLASTICITY_MAX_SYNAPSES);
    EXPECT_GT(config.stdp_tau_plus_ms, 0.0f);
    EXPECT_GT(config.stdp_tau_minus_ms, 0.0f);
    EXPECT_TRUE(config.protect_deduction);
    EXPECT_TRUE(config.protect_causal);
}

//=============================================================================
// Synapse Management Tests
//=============================================================================

TEST_F(ReasoningPlasticityBridgeTest, RegisterSynapse) {
    EXPECT_EQ(reasoning_plasticity_register_synapse(bridge, 1, REASON_SYNAPSE_INDUCTION, 0.5f), 0);
}

TEST_F(ReasoningPlasticityBridgeTest, RegisterSynapseNull) {
    EXPECT_EQ(reasoning_plasticity_register_synapse(nullptr, 1, REASON_SYNAPSE_INDUCTION, 0.5f), -1);
}

TEST_F(ReasoningPlasticityBridgeTest, RegisterDuplicateSynapse) {
    EXPECT_EQ(reasoning_plasticity_register_synapse(bridge, 1, REASON_SYNAPSE_INDUCTION, 0.5f), 0);
    EXPECT_EQ(reasoning_plasticity_register_synapse(bridge, 1, REASON_SYNAPSE_INDUCTION, 0.5f), -1);
}

TEST_F(ReasoningPlasticityBridgeTest, UnregisterSynapse) {
    reasoning_plasticity_register_synapse(bridge, 1, REASON_SYNAPSE_INDUCTION, 0.5f);
    EXPECT_EQ(reasoning_plasticity_unregister_synapse(bridge, 1), 0);
}

TEST_F(ReasoningPlasticityBridgeTest, UnregisterNonexistentSynapse) {
    EXPECT_EQ(reasoning_plasticity_unregister_synapse(bridge, 999), -1);
}

TEST_F(ReasoningPlasticityBridgeTest, GetSynapse) {
    reasoning_plasticity_register_synapse(bridge, 1, REASON_SYNAPSE_INDUCTION, 0.5f);

    reasoning_plasticity_synapse_t syn;
    EXPECT_EQ(reasoning_plasticity_get_synapse(bridge, 1, &syn), 0);
    EXPECT_EQ(syn.synapse_id, 1u);
    EXPECT_EQ(syn.type, REASON_SYNAPSE_INDUCTION);
    EXPECT_FLOAT_EQ(syn.weight, 0.5f);
}

TEST_F(ReasoningPlasticityBridgeTest, GetSynapseNonexistent) {
    reasoning_plasticity_synapse_t syn;
    EXPECT_EQ(reasoning_plasticity_get_synapse(bridge, 999, &syn), -1);
}

TEST_F(ReasoningPlasticityBridgeTest, ProtectSynapse) {
    reasoning_plasticity_register_synapse(bridge, 1, REASON_SYNAPSE_INDUCTION, 0.5f);
    EXPECT_EQ(reasoning_plasticity_protect_synapse(bridge, 1, true), 0);

    reasoning_plasticity_synapse_t syn;
    reasoning_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_TRUE(syn.is_protected);
}

TEST_F(ReasoningPlasticityBridgeTest, AutoProtectDeduction) {
    reasoning_plasticity_register_synapse(bridge, 1, REASON_SYNAPSE_DEDUCTION, 0.5f);

    reasoning_plasticity_synapse_t syn;
    reasoning_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_TRUE(syn.is_protected);
}

TEST_F(ReasoningPlasticityBridgeTest, AutoProtectCausal) {
    reasoning_plasticity_register_synapse(bridge, 1, REASON_SYNAPSE_CAUSAL, 0.5f);

    reasoning_plasticity_synapse_t syn;
    reasoning_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_TRUE(syn.is_protected);
}

//=============================================================================
// Learning Tests
//=============================================================================

TEST_F(ReasoningPlasticityBridgeTest, LearnValidConclusion) {
    reasoning_plasticity_register_synapse(bridge, 1, REASON_SYNAPSE_INDUCTION, 0.5f);
    EXPECT_EQ(reasoning_plasticity_learn(bridge, REASON_LEARN_VALID_CONCLUSION, 0.8f, 1, 0.9f), 0);

    reasoning_plasticity_synapse_t syn;
    reasoning_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_GT(syn.weight, 0.5f);
}

TEST_F(ReasoningPlasticityBridgeTest, LearnInvalidConclusion) {
    reasoning_plasticity_register_synapse(bridge, 1, REASON_SYNAPSE_INDUCTION, 0.5f);
    EXPECT_EQ(reasoning_plasticity_learn(bridge, REASON_LEARN_INVALID_CONCLUSION, 0.8f, 1, 0.9f), 0);

    reasoning_plasticity_synapse_t syn;
    reasoning_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_LT(syn.weight, 0.5f);
}

TEST_F(ReasoningPlasticityBridgeTest, LearnNull) {
    EXPECT_EQ(reasoning_plasticity_learn(nullptr, REASON_LEARN_VALID_CONCLUSION, 0.5f, 1, 0.5f), -1);
}

TEST_F(ReasoningPlasticityBridgeTest, LearnProtectedSynapse) {
    reasoning_plasticity_register_synapse(bridge, 1, REASON_SYNAPSE_DEDUCTION, 0.5f);

    reasoning_plasticity_synapse_t before;
    reasoning_plasticity_get_synapse(bridge, 1, &before);

    reasoning_plasticity_learn(bridge, REASON_LEARN_VALID_CONCLUSION, 1.0f, 1, 1.0f);

    reasoning_plasticity_synapse_t after;
    reasoning_plasticity_get_synapse(bridge, 1, &after);
    EXPECT_FLOAT_EQ(before.weight, after.weight);
}

TEST_F(ReasoningPlasticityBridgeTest, LearnCausalConfirmed) {
    reasoning_plasticity_register_synapse(bridge, 1, REASON_SYNAPSE_ANALOGY, 0.5f);
    EXPECT_EQ(reasoning_plasticity_learn(bridge, REASON_LEARN_CAUSAL_CONFIRMED, 0.7f, 1, 0.8f), 0);

    reasoning_plasticity_synapse_t syn;
    reasoning_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_GT(syn.weight, 0.5f);
}

TEST_F(ReasoningPlasticityBridgeTest, LearnCausalRefuted) {
    reasoning_plasticity_register_synapse(bridge, 1, REASON_SYNAPSE_ANALOGY, 0.5f);
    EXPECT_EQ(reasoning_plasticity_learn(bridge, REASON_LEARN_CAUSAL_REFUTED, 0.7f, 1, 0.8f), 0);

    reasoning_plasticity_synapse_t syn;
    reasoning_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_LT(syn.weight, 0.5f);
}

//=============================================================================
// STDP Tests
//=============================================================================

TEST_F(ReasoningPlasticityBridgeTest, ApplySTDPPotentiation) {
    reasoning_plasticity_register_synapse(bridge, 1, REASON_SYNAPSE_INDUCTION, 0.5f);

    float delta = reasoning_plasticity_apply_stdp(bridge, 1, 10.0f, 15.0f);
    EXPECT_TRUE(std::isfinite(delta));
    EXPECT_GT(delta, 0.0f);
}

TEST_F(ReasoningPlasticityBridgeTest, ApplySTDPDepression) {
    reasoning_plasticity_register_synapse(bridge, 1, REASON_SYNAPSE_INDUCTION, 0.5f);

    float delta = reasoning_plasticity_apply_stdp(bridge, 1, 15.0f, 10.0f);
    EXPECT_TRUE(std::isfinite(delta));
    EXPECT_LT(delta, 0.0f);
}

TEST_F(ReasoningPlasticityBridgeTest, ApplySTDPNull) {
    float delta = reasoning_plasticity_apply_stdp(nullptr, 1, 10.0f, 15.0f);
    EXPECT_TRUE(std::isnan(delta));
}

TEST_F(ReasoningPlasticityBridgeTest, ApplySTDPProtected) {
    reasoning_plasticity_register_synapse(bridge, 1, REASON_SYNAPSE_DEDUCTION, 0.5f);

    float delta = reasoning_plasticity_apply_stdp(bridge, 1, 10.0f, 15.0f);
    EXPECT_FLOAT_EQ(delta, 0.0f);
}

//=============================================================================
// Reward Modulation Tests
//=============================================================================

TEST_F(ReasoningPlasticityBridgeTest, ApplyReward) {
    reasoning_plasticity_register_synapse(bridge, 1, REASON_SYNAPSE_INDUCTION, 0.5f);
    EXPECT_EQ(reasoning_plasticity_apply_reward(bridge, 0.5f), 0);
}

TEST_F(ReasoningPlasticityBridgeTest, ApplyRewardNull) {
    EXPECT_EQ(reasoning_plasticity_apply_reward(nullptr, 0.5f), -1);
}

TEST_F(ReasoningPlasticityBridgeTest, ApplyRewardClamped) {
    reasoning_plasticity_register_synapse(bridge, 1, REASON_SYNAPSE_INDUCTION, 0.5f);
    EXPECT_EQ(reasoning_plasticity_apply_reward(bridge, 10.0f), 0); // Should clamp to 1.0
    EXPECT_EQ(reasoning_plasticity_apply_reward(bridge, -10.0f), 0); // Should clamp to -1.0
}

//=============================================================================
// BCM and Homeostatic Tests
//=============================================================================

TEST_F(ReasoningPlasticityBridgeTest, UpdateBCM) {
    reasoning_plasticity_register_synapse(bridge, 1, REASON_SYNAPSE_INDUCTION, 0.5f);
    EXPECT_EQ(reasoning_plasticity_update_bcm(bridge, 10.0f), 0);
}

TEST_F(ReasoningPlasticityBridgeTest, UpdateBCMNull) {
    EXPECT_EQ(reasoning_plasticity_update_bcm(nullptr, 10.0f), -1);
}

TEST_F(ReasoningPlasticityBridgeTest, UpdateBCMZeroDt) {
    EXPECT_EQ(reasoning_plasticity_update_bcm(bridge, 0.0f), -1);
}

TEST_F(ReasoningPlasticityBridgeTest, HomeostaticUpdate) {
    reasoning_plasticity_register_synapse(bridge, 1, REASON_SYNAPSE_INDUCTION, 0.5f);
    EXPECT_EQ(reasoning_plasticity_homeostatic_update(bridge, 10.0f), 0);
}

TEST_F(ReasoningPlasticityBridgeTest, HomeostaticUpdateNull) {
    EXPECT_EQ(reasoning_plasticity_homeostatic_update(nullptr, 10.0f), -1);
}

//=============================================================================
// Trace and Consolidation Tests
//=============================================================================

TEST_F(ReasoningPlasticityBridgeTest, UpdateTraces) {
    reasoning_plasticity_register_synapse(bridge, 1, REASON_SYNAPSE_INDUCTION, 0.5f);
    EXPECT_EQ(reasoning_plasticity_update_traces(bridge, 10.0f), 0);
}

TEST_F(ReasoningPlasticityBridgeTest, UpdateTracesNull) {
    EXPECT_EQ(reasoning_plasticity_update_traces(nullptr, 10.0f), -1);
}

TEST_F(ReasoningPlasticityBridgeTest, Consolidate) {
    reasoning_plasticity_register_synapse(bridge, 1, REASON_SYNAPSE_INDUCTION, 0.5f);
    EXPECT_EQ(reasoning_plasticity_consolidate(bridge), 0);
}

TEST_F(ReasoningPlasticityBridgeTest, ConsolidateNull) {
    EXPECT_EQ(reasoning_plasticity_consolidate(nullptr), -1);
}

//=============================================================================
// State Query Tests
//=============================================================================

TEST_F(ReasoningPlasticityBridgeTest, GetCalibrationState) {
    reasoning_calibration_state_t state;
    EXPECT_EQ(reasoning_plasticity_get_calibration_state(bridge, &state), 0);
    EXPECT_GE(state.deduction_strength, 0.0f);
    EXPECT_LE(state.deduction_strength, 1.0f);
}

TEST_F(ReasoningPlasticityBridgeTest, GetCalibrationStateNull) {
    EXPECT_EQ(reasoning_plasticity_get_calibration_state(nullptr, nullptr), -1);
    reasoning_calibration_state_t state;
    EXPECT_EQ(reasoning_plasticity_get_calibration_state(nullptr, &state), -1);
    EXPECT_EQ(reasoning_plasticity_get_calibration_state(bridge, nullptr), -1);
}

TEST_F(ReasoningPlasticityBridgeTest, GetBridgeState) {
    reasoning_plasticity_bridge_state_t state;
    EXPECT_EQ(reasoning_plasticity_get_state(bridge, &state), 0);
    EXPECT_EQ(state.state, REASONING_PLASTICITY_STATE_IDLE);
    EXPECT_EQ(state.active_synapses, 0u);
}

TEST_F(ReasoningPlasticityBridgeTest, GetStats) {
    reasoning_plasticity_stats_t stats;
    EXPECT_EQ(reasoning_plasticity_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.total_learning_events, 0u);
}

TEST_F(ReasoningPlasticityBridgeTest, ResetStats) {
    reasoning_plasticity_register_synapse(bridge, 1, REASON_SYNAPSE_INDUCTION, 0.5f);
    reasoning_plasticity_learn(bridge, REASON_LEARN_VALID_CONCLUSION, 0.5f, 1, 0.5f);

    reasoning_plasticity_stats_t stats;
    reasoning_plasticity_get_stats(bridge, &stats);
    EXPECT_GT(stats.total_learning_events, 0u);

    EXPECT_EQ(reasoning_plasticity_reset_stats(bridge), 0);
    reasoning_plasticity_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_learning_events, 0u);
}

//=============================================================================
// Callback Tests
//=============================================================================

TEST_F(ReasoningPlasticityBridgeTest, RegisterLearnCallback) {
    EXPECT_EQ(reasoning_plasticity_register_learn_callback(bridge, nullptr, nullptr), 0);
}

TEST_F(ReasoningPlasticityBridgeTest, RegisterCalibrationCallback) {
    EXPECT_EQ(reasoning_plasticity_register_calibration_callback(bridge, nullptr, nullptr), 0);
}

//=============================================================================
// Bio-Async Tests
//=============================================================================

TEST_F(ReasoningPlasticityBridgeTest, BioAsyncNotEnabled) {
    EXPECT_EQ(reasoning_plasticity_bio_async_connect(bridge), -1);
    EXPECT_FALSE(reasoning_plasticity_is_bio_async_connected(bridge));
}

TEST_F(ReasoningPlasticityBridgeTest, BioAsyncDisconnectSafe) {
    EXPECT_EQ(reasoning_plasticity_bio_async_disconnect(bridge), 0);
}

//=============================================================================
// Integration Workflow Tests
//=============================================================================

TEST_F(ReasoningPlasticityBridgeTest, FullLearningWorkflow) {
    // Register synapses (use INDUCTION which is not auto-protected)
    for (int i = 0; i < 10; i++) {
        reasoning_plasticity_register_synapse(bridge, i, REASON_SYNAPSE_INDUCTION, 0.5f);
    }

    // Learning cycle
    for (int cycle = 0; cycle < 20; cycle++) {
        for (int i = 0; i < 10; i++) {
            reasoning_plasticity_learn(bridge, REASON_LEARN_VALID_CONCLUSION, 0.3f, i, 0.8f);
            reasoning_plasticity_apply_stdp(bridge, i, (float)cycle, (float)cycle + 5.0f);
        }
        reasoning_plasticity_apply_reward(bridge, 0.5f);
        reasoning_plasticity_update_traces(bridge, 1.0f);
    }

    reasoning_plasticity_update_bcm(bridge, 10.0f);
    reasoning_plasticity_homeostatic_update(bridge, 10.0f);
    reasoning_plasticity_consolidate(bridge);

    reasoning_plasticity_stats_t stats;
    reasoning_plasticity_get_stats(bridge, &stats);
    EXPECT_GT(stats.total_learning_events, 100u);
    EXPECT_GT(stats.weight_updates, 100u);
}

TEST_F(ReasoningPlasticityBridgeTest, CausalLearning) {
    reasoning_plasticity_register_synapse(bridge, 1, REASON_SYNAPSE_EVIDENCE, 0.5f);

    for (int i = 0; i < 50; i++) {
        reasoning_plasticity_learn(bridge, REASON_LEARN_CAUSAL_CONFIRMED, 0.2f, 1, 0.7f);
    }

    reasoning_plasticity_synapse_t syn;
    reasoning_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_GT(syn.weight, 0.5f);
}

TEST_F(ReasoningPlasticityBridgeTest, AnalogyLearning) {
    reasoning_plasticity_register_synapse(bridge, 1, REASON_SYNAPSE_ANALOGY, 0.5f);

    for (int i = 0; i < 30; i++) {
        reasoning_plasticity_learn(bridge, REASON_LEARN_ANALOGY_MATCHED, 0.3f, 1, 0.8f);
    }

    reasoning_plasticity_synapse_t syn;
    reasoning_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_GT(syn.weight, 0.5f);

    reasoning_plasticity_stats_t stats;
    reasoning_plasticity_get_stats(bridge, &stats);
    EXPECT_GT(stats.analogy_learning_events, 0u);
}
