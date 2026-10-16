/**
 * @file test_curiosity_plasticity_bridge.cpp
 * @brief Unit tests for Curiosity Plasticity Bridge
 */

#include <gtest/gtest.h>
#include <cmath>

#include "cognitive/curiosity/nimcp_curiosity_plasticity_bridge.h"

class CuriosityPlasticityBridgeTest : public ::testing::Test {
protected:
    curiosity_plasticity_bridge_t* bridge = nullptr;

    void SetUp() override {
        curiosity_plasticity_config_t config = curiosity_plasticity_config_default();
        config.enable_bio_async = false;
        bridge = curiosity_plasticity_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            curiosity_plasticity_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(CuriosityPlasticityBridgeTest, CreateWithDefaults) {
    curiosity_plasticity_bridge_t* test_bridge = curiosity_plasticity_create(nullptr);
    ASSERT_NE(test_bridge, nullptr);
    curiosity_plasticity_destroy(test_bridge);
}

TEST_F(CuriosityPlasticityBridgeTest, CreateWithConfig) {
    curiosity_plasticity_config_t config = curiosity_plasticity_config_default();
    config.base_learning_rate = 0.005f;
    config.max_synapses = 128;
    curiosity_plasticity_bridge_t* test_bridge = curiosity_plasticity_create(&config);
    ASSERT_NE(test_bridge, nullptr);
    curiosity_plasticity_destroy(test_bridge);
}

TEST_F(CuriosityPlasticityBridgeTest, Reset) {
    EXPECT_EQ(curiosity_plasticity_reset(bridge), 0);
}

TEST_F(CuriosityPlasticityBridgeTest, ResetNull) {
    EXPECT_EQ(curiosity_plasticity_reset(nullptr), -1);
}

TEST_F(CuriosityPlasticityBridgeTest, DestroyNull) {
    curiosity_plasticity_destroy(nullptr); // Should not crash
}

//=============================================================================
// Default Config Tests
//=============================================================================

TEST_F(CuriosityPlasticityBridgeTest, DefaultConfigValues) {
    curiosity_plasticity_config_t config = curiosity_plasticity_config_default();
    EXPECT_EQ(config.base_learning_rate, CURIOSITY_PLASTICITY_DEFAULT_LR);
    EXPECT_EQ(config.max_synapses, CURIOSITY_PLASTICITY_MAX_SYNAPSES);
    EXPECT_GT(config.stdp_tau_plus_ms, 0.0f);
    EXPECT_GT(config.stdp_tau_minus_ms, 0.0f);
    EXPECT_TRUE(config.protect_exploration_drive);
    EXPECT_TRUE(config.protect_learning_progress);
}

//=============================================================================
// Synapse Management Tests
//=============================================================================

TEST_F(CuriosityPlasticityBridgeTest, RegisterSynapse) {
    EXPECT_EQ(curiosity_plasticity_register_synapse(bridge, 1, CURIOSITY_SYNAPSE_NOVELTY, 0.5f), 0);
}

TEST_F(CuriosityPlasticityBridgeTest, RegisterSynapseNull) {
    EXPECT_EQ(curiosity_plasticity_register_synapse(nullptr, 1, CURIOSITY_SYNAPSE_NOVELTY, 0.5f), -1);
}

TEST_F(CuriosityPlasticityBridgeTest, RegisterDuplicateSynapse) {
    EXPECT_EQ(curiosity_plasticity_register_synapse(bridge, 1, CURIOSITY_SYNAPSE_NOVELTY, 0.5f), 0);
    EXPECT_EQ(curiosity_plasticity_register_synapse(bridge, 1, CURIOSITY_SYNAPSE_NOVELTY, 0.5f), -1);
}

TEST_F(CuriosityPlasticityBridgeTest, UnregisterSynapse) {
    curiosity_plasticity_register_synapse(bridge, 1, CURIOSITY_SYNAPSE_NOVELTY, 0.5f);
    EXPECT_EQ(curiosity_plasticity_unregister_synapse(bridge, 1), 0);
}

TEST_F(CuriosityPlasticityBridgeTest, UnregisterNonexistentSynapse) {
    EXPECT_EQ(curiosity_plasticity_unregister_synapse(bridge, 999), -1);
}

TEST_F(CuriosityPlasticityBridgeTest, GetSynapse) {
    curiosity_plasticity_register_synapse(bridge, 1, CURIOSITY_SYNAPSE_NOVELTY, 0.5f);

    curiosity_plasticity_synapse_t syn;
    EXPECT_EQ(curiosity_plasticity_get_synapse(bridge, 1, &syn), 0);
    EXPECT_EQ(syn.synapse_id, 1u);
    EXPECT_EQ(syn.type, CURIOSITY_SYNAPSE_NOVELTY);
    EXPECT_FLOAT_EQ(syn.weight, 0.5f);
}

TEST_F(CuriosityPlasticityBridgeTest, GetSynapseNonexistent) {
    curiosity_plasticity_synapse_t syn;
    EXPECT_EQ(curiosity_plasticity_get_synapse(bridge, 999, &syn), -1);
}

TEST_F(CuriosityPlasticityBridgeTest, ProtectSynapse) {
    curiosity_plasticity_register_synapse(bridge, 1, CURIOSITY_SYNAPSE_NOVELTY, 0.5f);
    EXPECT_EQ(curiosity_plasticity_protect_synapse(bridge, 1, true), 0);

    curiosity_plasticity_synapse_t syn;
    curiosity_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_TRUE(syn.is_protected);
}

TEST_F(CuriosityPlasticityBridgeTest, AutoProtectExploration) {
    curiosity_plasticity_register_synapse(bridge, 1, CURIOSITY_SYNAPSE_EXPLORATION, 0.5f);

    curiosity_plasticity_synapse_t syn;
    curiosity_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_TRUE(syn.is_protected);
}

TEST_F(CuriosityPlasticityBridgeTest, AutoProtectLearning) {
    curiosity_plasticity_register_synapse(bridge, 1, CURIOSITY_SYNAPSE_LEARNING, 0.5f);

    curiosity_plasticity_synapse_t syn;
    curiosity_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_TRUE(syn.is_protected);
}

//=============================================================================
// Learning Tests
//=============================================================================

TEST_F(CuriosityPlasticityBridgeTest, LearnNoveltyConfirmed) {
    curiosity_plasticity_register_synapse(bridge, 1, CURIOSITY_SYNAPSE_NOVELTY, 0.5f);
    EXPECT_EQ(curiosity_plasticity_learn(bridge, CURIOSITY_LEARN_NOVELTY_CONFIRMED, 0.8f, 1, 0.9f), 0);

    curiosity_plasticity_synapse_t syn;
    curiosity_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_GT(syn.weight, 0.5f);
}

TEST_F(CuriosityPlasticityBridgeTest, LearnFalseNovelty) {
    curiosity_plasticity_register_synapse(bridge, 1, CURIOSITY_SYNAPSE_NOVELTY, 0.5f);
    EXPECT_EQ(curiosity_plasticity_learn(bridge, CURIOSITY_LEARN_FALSE_NOVELTY, 0.8f, 1, 0.9f), 0);

    curiosity_plasticity_synapse_t syn;
    curiosity_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_LT(syn.weight, 0.5f);
}

TEST_F(CuriosityPlasticityBridgeTest, LearnInfoGainHigh) {
    curiosity_plasticity_register_synapse(bridge, 1, CURIOSITY_SYNAPSE_INFORMATION, 0.5f);
    EXPECT_EQ(curiosity_plasticity_learn(bridge, CURIOSITY_LEARN_INFO_GAIN_HIGH, 0.8f, 1, 0.9f), 0);

    curiosity_plasticity_synapse_t syn;
    curiosity_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_GT(syn.weight, 0.5f);
}

TEST_F(CuriosityPlasticityBridgeTest, LearnExplorationSuccess) {
    curiosity_plasticity_register_synapse(bridge, 1, CURIOSITY_SYNAPSE_INTEREST, 0.5f);
    EXPECT_EQ(curiosity_plasticity_learn(bridge, CURIOSITY_LEARN_EXPLORATION_SUCCESS, 0.8f, 1, 0.9f), 0);

    curiosity_plasticity_synapse_t syn;
    curiosity_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_GT(syn.weight, 0.5f);
}

TEST_F(CuriosityPlasticityBridgeTest, LearnNull) {
    EXPECT_EQ(curiosity_plasticity_learn(nullptr, CURIOSITY_LEARN_NOVELTY_CONFIRMED, 0.5f, 1, 0.5f), -1);
}

TEST_F(CuriosityPlasticityBridgeTest, LearnProtectedSynapse) {
    curiosity_plasticity_register_synapse(bridge, 1, CURIOSITY_SYNAPSE_EXPLORATION, 0.5f);

    curiosity_plasticity_synapse_t before;
    curiosity_plasticity_get_synapse(bridge, 1, &before);

    curiosity_plasticity_learn(bridge, CURIOSITY_LEARN_NOVELTY_CONFIRMED, 1.0f, 1, 1.0f);

    curiosity_plasticity_synapse_t after;
    curiosity_plasticity_get_synapse(bridge, 1, &after);
    EXPECT_FLOAT_EQ(before.weight, after.weight);
}

//=============================================================================
// STDP Tests
//=============================================================================

TEST_F(CuriosityPlasticityBridgeTest, ApplySTDPPotentiation) {
    curiosity_plasticity_register_synapse(bridge, 1, CURIOSITY_SYNAPSE_NOVELTY, 0.5f);

    float delta = curiosity_plasticity_apply_stdp(bridge, 1, 10.0f, 15.0f);
    EXPECT_TRUE(std::isfinite(delta));
    EXPECT_GT(delta, 0.0f);
}

TEST_F(CuriosityPlasticityBridgeTest, ApplySTDPDepression) {
    curiosity_plasticity_register_synapse(bridge, 1, CURIOSITY_SYNAPSE_NOVELTY, 0.5f);

    float delta = curiosity_plasticity_apply_stdp(bridge, 1, 15.0f, 10.0f);
    EXPECT_TRUE(std::isfinite(delta));
    EXPECT_LT(delta, 0.0f);
}

TEST_F(CuriosityPlasticityBridgeTest, ApplySTDPNull) {
    float delta = curiosity_plasticity_apply_stdp(nullptr, 1, 10.0f, 15.0f);
    EXPECT_TRUE(std::isnan(delta));
}

TEST_F(CuriosityPlasticityBridgeTest, ApplySTDPProtected) {
    curiosity_plasticity_register_synapse(bridge, 1, CURIOSITY_SYNAPSE_EXPLORATION, 0.5f);

    float delta = curiosity_plasticity_apply_stdp(bridge, 1, 10.0f, 15.0f);
    EXPECT_FLOAT_EQ(delta, 0.0f);
}

//=============================================================================
// Reward Modulation Tests
//=============================================================================

TEST_F(CuriosityPlasticityBridgeTest, ApplyReward) {
    curiosity_plasticity_register_synapse(bridge, 1, CURIOSITY_SYNAPSE_NOVELTY, 0.5f);
    EXPECT_EQ(curiosity_plasticity_apply_reward(bridge, 0.5f), 0);
}

TEST_F(CuriosityPlasticityBridgeTest, ApplyRewardNull) {
    EXPECT_EQ(curiosity_plasticity_apply_reward(nullptr, 0.5f), -1);
}

TEST_F(CuriosityPlasticityBridgeTest, ApplyRewardClamped) {
    curiosity_plasticity_register_synapse(bridge, 1, CURIOSITY_SYNAPSE_NOVELTY, 0.5f);
    EXPECT_EQ(curiosity_plasticity_apply_reward(bridge, 10.0f), 0); // Should clamp to 1.0
    EXPECT_EQ(curiosity_plasticity_apply_reward(bridge, -10.0f), 0); // Should clamp to -1.0
}

//=============================================================================
// BCM and Homeostatic Tests
//=============================================================================

TEST_F(CuriosityPlasticityBridgeTest, UpdateBCM) {
    curiosity_plasticity_register_synapse(bridge, 1, CURIOSITY_SYNAPSE_NOVELTY, 0.5f);
    EXPECT_EQ(curiosity_plasticity_update_bcm(bridge, 10.0f), 0);
}

TEST_F(CuriosityPlasticityBridgeTest, UpdateBCMNull) {
    EXPECT_EQ(curiosity_plasticity_update_bcm(nullptr, 10.0f), -1);
}

TEST_F(CuriosityPlasticityBridgeTest, UpdateBCMZeroDt) {
    EXPECT_EQ(curiosity_plasticity_update_bcm(bridge, 0.0f), -1);
}

TEST_F(CuriosityPlasticityBridgeTest, HomeostaticUpdate) {
    curiosity_plasticity_register_synapse(bridge, 1, CURIOSITY_SYNAPSE_NOVELTY, 0.5f);
    EXPECT_EQ(curiosity_plasticity_homeostatic_update(bridge, 10.0f), 0);
}

TEST_F(CuriosityPlasticityBridgeTest, HomeostaticUpdateNull) {
    EXPECT_EQ(curiosity_plasticity_homeostatic_update(nullptr, 10.0f), -1);
}

//=============================================================================
// Trace and Consolidation Tests
//=============================================================================

TEST_F(CuriosityPlasticityBridgeTest, UpdateTraces) {
    curiosity_plasticity_register_synapse(bridge, 1, CURIOSITY_SYNAPSE_NOVELTY, 0.5f);
    EXPECT_EQ(curiosity_plasticity_update_traces(bridge, 10.0f), 0);
}

TEST_F(CuriosityPlasticityBridgeTest, UpdateTracesNull) {
    EXPECT_EQ(curiosity_plasticity_update_traces(nullptr, 10.0f), -1);
}

TEST_F(CuriosityPlasticityBridgeTest, Consolidate) {
    curiosity_plasticity_register_synapse(bridge, 1, CURIOSITY_SYNAPSE_NOVELTY, 0.5f);
    EXPECT_EQ(curiosity_plasticity_consolidate(bridge), 0);
}

TEST_F(CuriosityPlasticityBridgeTest, ConsolidateNull) {
    EXPECT_EQ(curiosity_plasticity_consolidate(nullptr), -1);
}

//=============================================================================
// State Query Tests
//=============================================================================

TEST_F(CuriosityPlasticityBridgeTest, GetExplorationState) {
    curiosity_exploration_state_t state;
    EXPECT_EQ(curiosity_plasticity_get_exploration_state(bridge, &state), 0);
    EXPECT_GE(state.novelty_sensitivity, 0.0f);
    EXPECT_GE(state.exploration_calibration, 0.0f);
}

TEST_F(CuriosityPlasticityBridgeTest, GetExplorationStateNull) {
    EXPECT_EQ(curiosity_plasticity_get_exploration_state(nullptr, nullptr), -1);
    curiosity_exploration_state_t state;
    EXPECT_EQ(curiosity_plasticity_get_exploration_state(nullptr, &state), -1);
    EXPECT_EQ(curiosity_plasticity_get_exploration_state(bridge, nullptr), -1);
}

TEST_F(CuriosityPlasticityBridgeTest, GetBridgeState) {
    curiosity_plasticity_bridge_state_t state;
    EXPECT_EQ(curiosity_plasticity_get_state(bridge, &state), 0);
    EXPECT_EQ(state.state, CURIOSITY_PLASTICITY_STATE_IDLE);
    EXPECT_EQ(state.active_synapses, 0u);
}

TEST_F(CuriosityPlasticityBridgeTest, GetStats) {
    curiosity_plasticity_stats_t stats;
    EXPECT_EQ(curiosity_plasticity_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.total_learning_events, 0u);
}

TEST_F(CuriosityPlasticityBridgeTest, ResetStats) {
    curiosity_plasticity_register_synapse(bridge, 1, CURIOSITY_SYNAPSE_NOVELTY, 0.5f);
    curiosity_plasticity_learn(bridge, CURIOSITY_LEARN_NOVELTY_CONFIRMED, 0.5f, 1, 0.5f);

    curiosity_plasticity_stats_t stats;
    curiosity_plasticity_get_stats(bridge, &stats);
    EXPECT_GT(stats.total_learning_events, 0u);

    EXPECT_EQ(curiosity_plasticity_reset_stats(bridge), 0);
    curiosity_plasticity_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_learning_events, 0u);
}

//=============================================================================
// Callback Tests
//=============================================================================

TEST_F(CuriosityPlasticityBridgeTest, RegisterLearnCallback) {
    EXPECT_EQ(curiosity_plasticity_register_learn_callback(bridge, nullptr, nullptr), 0);
}

TEST_F(CuriosityPlasticityBridgeTest, RegisterExplorationCallback) {
    EXPECT_EQ(curiosity_plasticity_register_exploration_callback(bridge, nullptr, nullptr), 0);
}

//=============================================================================
// Bio-Async Tests
//=============================================================================

TEST_F(CuriosityPlasticityBridgeTest, BioAsyncNotEnabled) {
    EXPECT_EQ(curiosity_plasticity_bio_async_connect(bridge), -1);
    EXPECT_FALSE(curiosity_plasticity_is_bio_async_connected(bridge));
}

TEST_F(CuriosityPlasticityBridgeTest, BioAsyncDisconnectSafe) {
    EXPECT_EQ(curiosity_plasticity_bio_async_disconnect(bridge), 0);
}

//=============================================================================
// Integration Workflow Tests
//=============================================================================

TEST_F(CuriosityPlasticityBridgeTest, FullLearningWorkflow) {
    // Register synapses
    for (int i = 0; i < 10; i++) {
        curiosity_plasticity_register_synapse(bridge, i, CURIOSITY_SYNAPSE_NOVELTY, 0.5f);
    }

    // Learning cycle
    for (int cycle = 0; cycle < 20; cycle++) {
        for (int i = 0; i < 10; i++) {
            curiosity_plasticity_learn(bridge, CURIOSITY_LEARN_NOVELTY_CONFIRMED, 0.3f, i, 0.8f);
            curiosity_plasticity_apply_stdp(bridge, i, (float)cycle, (float)cycle + 5.0f);
        }
        curiosity_plasticity_apply_reward(bridge, 0.5f);
        curiosity_plasticity_update_traces(bridge, 1.0f);
    }

    curiosity_plasticity_update_bcm(bridge, 10.0f);
    curiosity_plasticity_homeostatic_update(bridge, 10.0f);
    curiosity_plasticity_consolidate(bridge);

    curiosity_plasticity_stats_t stats;
    curiosity_plasticity_get_stats(bridge, &stats);
    EXPECT_GT(stats.total_learning_events, 100u);
    EXPECT_GT(stats.weight_updates, 100u);
}

TEST_F(CuriosityPlasticityBridgeTest, ExplorationLearning) {
    // Use non-protected synapse
    curiosity_plasticity_register_synapse(bridge, 1, CURIOSITY_SYNAPSE_NOVELTY, 0.5f);

    for (int i = 0; i < 50; i++) {
        curiosity_plasticity_learn(bridge, CURIOSITY_LEARN_EXPLORATION_SUCCESS, 0.2f, 1, 0.7f);
    }

    curiosity_plasticity_synapse_t syn;
    curiosity_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_NE(syn.weight, 0.5f);
}
