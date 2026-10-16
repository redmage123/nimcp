/**
 * @file test_meta_learning_plasticity_bridge.cpp
 * @brief Unit tests for Meta Learning Plasticity Bridge
 */

#include <gtest/gtest.h>
#include <cmath>

#include "cognitive/meta_learning/nimcp_meta_learning_plasticity_bridge.h"

class MetaLearningPlasticityBridgeTest : public ::testing::Test {
protected:
    meta_learning_plasticity_bridge_t* bridge = nullptr;

    void SetUp() override {
        meta_learning_plasticity_config_t config = meta_learning_plasticity_config_default();
        config.enable_bio_async = false;
        bridge = meta_learning_plasticity_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            meta_learning_plasticity_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(MetaLearningPlasticityBridgeTest, CreateWithDefaults) {
    meta_learning_plasticity_bridge_t* test_bridge = meta_learning_plasticity_create(nullptr);
    ASSERT_NE(test_bridge, nullptr);
    meta_learning_plasticity_destroy(test_bridge);
}

TEST_F(MetaLearningPlasticityBridgeTest, CreateWithConfig) {
    meta_learning_plasticity_config_t config = meta_learning_plasticity_config_default();
    config.base_learning_rate = 0.005f;
    config.max_synapses = 128;
    meta_learning_plasticity_bridge_t* test_bridge = meta_learning_plasticity_create(&config);
    ASSERT_NE(test_bridge, nullptr);
    meta_learning_plasticity_destroy(test_bridge);
}

TEST_F(MetaLearningPlasticityBridgeTest, Reset) {
    EXPECT_EQ(meta_learning_plasticity_reset(bridge), 0);
}

TEST_F(MetaLearningPlasticityBridgeTest, ResetNull) {
    EXPECT_EQ(meta_learning_plasticity_reset(nullptr), -1);
}

TEST_F(MetaLearningPlasticityBridgeTest, DestroyNull) {
    meta_learning_plasticity_destroy(nullptr); // Should not crash
}

//=============================================================================
// Default Config Tests
//=============================================================================

TEST_F(MetaLearningPlasticityBridgeTest, DefaultConfigValues) {
    meta_learning_plasticity_config_t config = meta_learning_plasticity_config_default();
    EXPECT_EQ(config.base_learning_rate, META_LEARNING_PLASTICITY_DEFAULT_LR);
    EXPECT_EQ(config.max_synapses, META_LEARNING_PLASTICITY_MAX_SYNAPSES);
    EXPECT_GT(config.stdp_tau_plus_ms, 0.0f);
    EXPECT_GT(config.stdp_tau_minus_ms, 0.0f);
    EXPECT_TRUE(config.protect_core_patterns);
}

//=============================================================================
// Synapse Management Tests
//=============================================================================

TEST_F(MetaLearningPlasticityBridgeTest, RegisterSynapse) {
    EXPECT_EQ(meta_learning_plasticity_register_synapse(bridge, 1, META_SYNAPSE_STRATEGY, 0.5f), 0);
}

TEST_F(MetaLearningPlasticityBridgeTest, RegisterSynapseNull) {
    EXPECT_EQ(meta_learning_plasticity_register_synapse(nullptr, 1, META_SYNAPSE_STRATEGY, 0.5f), -1);
}

TEST_F(MetaLearningPlasticityBridgeTest, RegisterDuplicateSynapse) {
    EXPECT_EQ(meta_learning_plasticity_register_synapse(bridge, 1, META_SYNAPSE_STRATEGY, 0.5f), 0);
    EXPECT_EQ(meta_learning_plasticity_register_synapse(bridge, 1, META_SYNAPSE_STRATEGY, 0.5f), -1);
}

TEST_F(MetaLearningPlasticityBridgeTest, UnregisterSynapse) {
    meta_learning_plasticity_register_synapse(bridge, 1, META_SYNAPSE_STRATEGY, 0.5f);
    EXPECT_EQ(meta_learning_plasticity_unregister_synapse(bridge, 1), 0);
}

TEST_F(MetaLearningPlasticityBridgeTest, UnregisterNonexistentSynapse) {
    EXPECT_EQ(meta_learning_plasticity_unregister_synapse(bridge, 999), -1);
}

TEST_F(MetaLearningPlasticityBridgeTest, GetSynapse) {
    meta_learning_plasticity_register_synapse(bridge, 1, META_SYNAPSE_STRATEGY, 0.5f);

    meta_learning_plasticity_synapse_t syn;
    EXPECT_EQ(meta_learning_plasticity_get_synapse(bridge, 1, &syn), 0);
    EXPECT_EQ(syn.synapse_id, 1u);
    EXPECT_EQ(syn.type, META_SYNAPSE_STRATEGY);
    EXPECT_FLOAT_EQ(syn.weight, 0.5f);
}

TEST_F(MetaLearningPlasticityBridgeTest, GetSynapseNonexistent) {
    meta_learning_plasticity_synapse_t syn;
    EXPECT_EQ(meta_learning_plasticity_get_synapse(bridge, 999, &syn), -1);
}

TEST_F(MetaLearningPlasticityBridgeTest, ProtectSynapse) {
    meta_learning_plasticity_register_synapse(bridge, 1, META_SYNAPSE_STRATEGY, 0.5f);
    EXPECT_EQ(meta_learning_plasticity_protect_synapse(bridge, 1, true), 0);

    meta_learning_plasticity_synapse_t syn;
    meta_learning_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_TRUE(syn.is_protected);
}

TEST_F(MetaLearningPlasticityBridgeTest, AutoProtectLearningRate) {
    // LEARNING_RATE should be auto-protected
    meta_learning_plasticity_register_synapse(bridge, 1, META_SYNAPSE_LEARNING_RATE, 0.5f);

    meta_learning_plasticity_synapse_t syn;
    meta_learning_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_TRUE(syn.is_protected);
}

TEST_F(MetaLearningPlasticityBridgeTest, AutoProtectConsolidation) {
    // CONSOLIDATION should be auto-protected
    meta_learning_plasticity_register_synapse(bridge, 1, META_SYNAPSE_CONSOLIDATION, 0.5f);

    meta_learning_plasticity_synapse_t syn;
    meta_learning_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_TRUE(syn.is_protected);
}

//=============================================================================
// Learning Tests
//=============================================================================

TEST_F(MetaLearningPlasticityBridgeTest, LearnRateCorrect) {
    meta_learning_plasticity_register_synapse(bridge, 1, META_SYNAPSE_STRATEGY, 0.5f);
    EXPECT_EQ(meta_learning_plasticity_learn(bridge, META_LEARN_RATE_CORRECT, 0.8f, 1, 0.9f), 0);

    meta_learning_plasticity_synapse_t syn;
    meta_learning_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_GT(syn.weight, 0.5f);
}

TEST_F(MetaLearningPlasticityBridgeTest, LearnRateTooHigh) {
    meta_learning_plasticity_register_synapse(bridge, 1, META_SYNAPSE_STRATEGY, 0.5f);
    EXPECT_EQ(meta_learning_plasticity_learn(bridge, META_LEARN_RATE_TOO_HIGH, 0.8f, 1, 0.9f), 0);

    meta_learning_plasticity_synapse_t syn;
    meta_learning_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_LT(syn.weight, 0.5f);
}

TEST_F(MetaLearningPlasticityBridgeTest, LearnNull) {
    EXPECT_EQ(meta_learning_plasticity_learn(nullptr, META_LEARN_RATE_CORRECT, 0.5f, 1, 0.5f), -1);
}

TEST_F(MetaLearningPlasticityBridgeTest, LearnProtectedSynapse) {
    meta_learning_plasticity_register_synapse(bridge, 1, META_SYNAPSE_LEARNING_RATE, 0.5f);

    meta_learning_plasticity_synapse_t before;
    meta_learning_plasticity_get_synapse(bridge, 1, &before);

    meta_learning_plasticity_learn(bridge, META_LEARN_RATE_CORRECT, 1.0f, 1, 1.0f);

    meta_learning_plasticity_synapse_t after;
    meta_learning_plasticity_get_synapse(bridge, 1, &after);
    EXPECT_FLOAT_EQ(before.weight, after.weight);
}

//=============================================================================
// STDP Tests
//=============================================================================

TEST_F(MetaLearningPlasticityBridgeTest, ApplySTDPPotentiation) {
    meta_learning_plasticity_register_synapse(bridge, 1, META_SYNAPSE_STRATEGY, 0.5f);

    float delta = meta_learning_plasticity_apply_stdp(bridge, 1, 10.0f, 15.0f);
    EXPECT_TRUE(std::isfinite(delta));
    EXPECT_GT(delta, 0.0f);
}

TEST_F(MetaLearningPlasticityBridgeTest, ApplySTDPDepression) {
    meta_learning_plasticity_register_synapse(bridge, 1, META_SYNAPSE_STRATEGY, 0.5f);

    float delta = meta_learning_plasticity_apply_stdp(bridge, 1, 15.0f, 10.0f);
    EXPECT_TRUE(std::isfinite(delta));
    EXPECT_LT(delta, 0.0f);
}

TEST_F(MetaLearningPlasticityBridgeTest, ApplySTDPNull) {
    float delta = meta_learning_plasticity_apply_stdp(nullptr, 1, 10.0f, 15.0f);
    EXPECT_TRUE(std::isnan(delta));
}

TEST_F(MetaLearningPlasticityBridgeTest, ApplySTDPProtected) {
    meta_learning_plasticity_register_synapse(bridge, 1, META_SYNAPSE_LEARNING_RATE, 0.5f);

    float delta = meta_learning_plasticity_apply_stdp(bridge, 1, 10.0f, 15.0f);
    EXPECT_FLOAT_EQ(delta, 0.0f);
}

//=============================================================================
// Reward Modulation Tests
//=============================================================================

TEST_F(MetaLearningPlasticityBridgeTest, ApplyReward) {
    meta_learning_plasticity_register_synapse(bridge, 1, META_SYNAPSE_STRATEGY, 0.5f);
    EXPECT_EQ(meta_learning_plasticity_apply_reward(bridge, 0.5f), 0);
}

TEST_F(MetaLearningPlasticityBridgeTest, ApplyRewardNull) {
    EXPECT_EQ(meta_learning_plasticity_apply_reward(nullptr, 0.5f), -1);
}

TEST_F(MetaLearningPlasticityBridgeTest, ApplyRewardClamped) {
    meta_learning_plasticity_register_synapse(bridge, 1, META_SYNAPSE_STRATEGY, 0.5f);
    EXPECT_EQ(meta_learning_plasticity_apply_reward(bridge, 10.0f), 0); // Should clamp to 1.0
    EXPECT_EQ(meta_learning_plasticity_apply_reward(bridge, -10.0f), 0); // Should clamp to -1.0
}

//=============================================================================
// BCM and Homeostatic Tests
//=============================================================================

TEST_F(MetaLearningPlasticityBridgeTest, UpdateBCM) {
    meta_learning_plasticity_register_synapse(bridge, 1, META_SYNAPSE_STRATEGY, 0.5f);
    EXPECT_EQ(meta_learning_plasticity_update_bcm(bridge, 10.0f), 0);
}

TEST_F(MetaLearningPlasticityBridgeTest, UpdateBCMNull) {
    EXPECT_EQ(meta_learning_plasticity_update_bcm(nullptr, 10.0f), -1);
}

TEST_F(MetaLearningPlasticityBridgeTest, UpdateBCMZeroDt) {
    EXPECT_EQ(meta_learning_plasticity_update_bcm(bridge, 0.0f), -1);
}

TEST_F(MetaLearningPlasticityBridgeTest, HomeostaticUpdate) {
    meta_learning_plasticity_register_synapse(bridge, 1, META_SYNAPSE_STRATEGY, 0.5f);
    EXPECT_EQ(meta_learning_plasticity_homeostatic_update(bridge, 10.0f), 0);
}

TEST_F(MetaLearningPlasticityBridgeTest, HomeostaticUpdateNull) {
    EXPECT_EQ(meta_learning_plasticity_homeostatic_update(nullptr, 10.0f), -1);
}

//=============================================================================
// Trace and Consolidation Tests
//=============================================================================

TEST_F(MetaLearningPlasticityBridgeTest, UpdateTraces) {
    meta_learning_plasticity_register_synapse(bridge, 1, META_SYNAPSE_STRATEGY, 0.5f);
    EXPECT_EQ(meta_learning_plasticity_update_traces(bridge, 10.0f), 0);
}

TEST_F(MetaLearningPlasticityBridgeTest, UpdateTracesNull) {
    EXPECT_EQ(meta_learning_plasticity_update_traces(nullptr, 10.0f), -1);
}

TEST_F(MetaLearningPlasticityBridgeTest, Consolidate) {
    meta_learning_plasticity_register_synapse(bridge, 1, META_SYNAPSE_STRATEGY, 0.5f);
    EXPECT_EQ(meta_learning_plasticity_consolidate(bridge), 0);
}

TEST_F(MetaLearningPlasticityBridgeTest, ConsolidateNull) {
    EXPECT_EQ(meta_learning_plasticity_consolidate(nullptr), -1);
}

//=============================================================================
// State Query Tests
//=============================================================================

TEST_F(MetaLearningPlasticityBridgeTest, GetAdaptationState) {
    meta_learning_adaptation_state_t state;
    EXPECT_EQ(meta_learning_plasticity_get_adaptation_state(bridge, &state), 0);
    EXPECT_GE(state.transfer_calibration, 0.0f);
    EXPECT_LE(state.transfer_calibration, 1.0f);
}

TEST_F(MetaLearningPlasticityBridgeTest, GetAdaptationStateNull) {
    EXPECT_EQ(meta_learning_plasticity_get_adaptation_state(nullptr, nullptr), -1);
    meta_learning_adaptation_state_t state;
    EXPECT_EQ(meta_learning_plasticity_get_adaptation_state(nullptr, &state), -1);
    EXPECT_EQ(meta_learning_plasticity_get_adaptation_state(bridge, nullptr), -1);
}

TEST_F(MetaLearningPlasticityBridgeTest, GetBridgeState) {
    meta_learning_plasticity_bridge_state_t state;
    EXPECT_EQ(meta_learning_plasticity_get_state(bridge, &state), 0);
    EXPECT_EQ(state.state, META_LEARNING_PLASTICITY_STATE_IDLE);
    EXPECT_EQ(state.active_synapses, 0u);
}

TEST_F(MetaLearningPlasticityBridgeTest, GetStats) {
    meta_learning_plasticity_stats_t stats;
    EXPECT_EQ(meta_learning_plasticity_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.total_learning_events, 0u);
}

TEST_F(MetaLearningPlasticityBridgeTest, ResetStats) {
    meta_learning_plasticity_register_synapse(bridge, 1, META_SYNAPSE_STRATEGY, 0.5f);
    meta_learning_plasticity_learn(bridge, META_LEARN_RATE_CORRECT, 0.5f, 1, 0.5f);

    meta_learning_plasticity_stats_t stats;
    meta_learning_plasticity_get_stats(bridge, &stats);
    EXPECT_GT(stats.total_learning_events, 0u);

    EXPECT_EQ(meta_learning_plasticity_reset_stats(bridge), 0);
    meta_learning_plasticity_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_learning_events, 0u);
}

//=============================================================================
// Callback Tests
//=============================================================================

TEST_F(MetaLearningPlasticityBridgeTest, RegisterLearnCallback) {
    EXPECT_EQ(meta_learning_plasticity_register_learn_callback(bridge, nullptr, nullptr), 0);
}

TEST_F(MetaLearningPlasticityBridgeTest, RegisterAdaptationCallback) {
    EXPECT_EQ(meta_learning_plasticity_register_adaptation_callback(bridge, nullptr, nullptr), 0);
}

//=============================================================================
// Bio-Async Tests
//=============================================================================

TEST_F(MetaLearningPlasticityBridgeTest, BioAsyncNotEnabled) {
    EXPECT_EQ(meta_learning_plasticity_bio_async_connect(bridge), -1);
    EXPECT_FALSE(meta_learning_plasticity_is_bio_async_connected(bridge));
}

TEST_F(MetaLearningPlasticityBridgeTest, BioAsyncDisconnectSafe) {
    EXPECT_EQ(meta_learning_plasticity_bio_async_disconnect(bridge), 0);
}

//=============================================================================
// Integration Workflow Tests
//=============================================================================

TEST_F(MetaLearningPlasticityBridgeTest, FullLearningWorkflow) {
    // Register synapses
    for (int i = 0; i < 10; i++) {
        meta_learning_plasticity_register_synapse(bridge, i, META_SYNAPSE_STRATEGY, 0.5f);
    }

    // Learning cycle
    for (int cycle = 0; cycle < 20; cycle++) {
        for (int i = 0; i < 10; i++) {
            meta_learning_plasticity_learn(bridge, META_LEARN_RATE_CORRECT, 0.3f, i, 0.8f);
            meta_learning_plasticity_apply_stdp(bridge, i, (float)cycle, (float)cycle + 5.0f);
        }
        meta_learning_plasticity_apply_reward(bridge, 0.5f);
        meta_learning_plasticity_update_traces(bridge, 1.0f);
    }

    meta_learning_plasticity_update_bcm(bridge, 10.0f);
    meta_learning_plasticity_homeostatic_update(bridge, 10.0f);
    meta_learning_plasticity_consolidate(bridge);

    meta_learning_plasticity_stats_t stats;
    meta_learning_plasticity_get_stats(bridge, &stats);
    EXPECT_GT(stats.total_learning_events, 100u);
    EXPECT_GT(stats.weight_updates, 100u);
}

TEST_F(MetaLearningPlasticityBridgeTest, TransferLearning) {
    // Use TRANSFER synapse (not auto-protected)
    meta_learning_plasticity_register_synapse(bridge, 1, META_SYNAPSE_TRANSFER, 0.5f);

    for (int i = 0; i < 50; i++) {
        meta_learning_plasticity_learn(bridge, META_LEARN_TRANSFER_SUCCESS, 0.2f, 1, 0.7f);
    }

    meta_learning_plasticity_synapse_t syn;
    meta_learning_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_NE(syn.weight, 0.5f);
}
