/**
 * @file test_gw_plasticity_bridge.cpp
 * @brief Unit tests for Global Workspace Plasticity Bridge
 */

#include <gtest/gtest.h>
#include <cmath>

#include "cognitive/global_workspace/nimcp_gw_plasticity_bridge.h"

class GWPlasticityBridgeTest : public ::testing::Test {
protected:
    gw_plasticity_bridge_t* bridge = nullptr;

    void SetUp() override {
        gw_plasticity_config_t config = gw_plasticity_config_default();
        config.enable_bio_async = false;
        bridge = gw_plasticity_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            gw_plasticity_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(GWPlasticityBridgeTest, CreateWithDefaults) {
    gw_plasticity_bridge_t* test_bridge = gw_plasticity_create(nullptr);
    ASSERT_NE(test_bridge, nullptr);
    gw_plasticity_destroy(test_bridge);
}

TEST_F(GWPlasticityBridgeTest, CreateWithConfig) {
    gw_plasticity_config_t config = gw_plasticity_config_default();
    config.base_learning_rate = 0.02f;
    config.max_synapses = 128;
    gw_plasticity_bridge_t* test_bridge = gw_plasticity_create(&config);
    ASSERT_NE(test_bridge, nullptr);
    gw_plasticity_destroy(test_bridge);
}

TEST_F(GWPlasticityBridgeTest, Reset) {
    EXPECT_EQ(gw_plasticity_reset(bridge), 0);
}

TEST_F(GWPlasticityBridgeTest, ResetNull) {
    EXPECT_EQ(gw_plasticity_reset(nullptr), -1);
}

TEST_F(GWPlasticityBridgeTest, DestroyNull) {
    gw_plasticity_destroy(nullptr); // Should not crash
}

//=============================================================================
// Default Config Tests
//=============================================================================

TEST_F(GWPlasticityBridgeTest, DefaultConfigValues) {
    gw_plasticity_config_t config = gw_plasticity_config_default();
    EXPECT_EQ(config.base_learning_rate, GW_PLASTICITY_DEFAULT_LR);
    EXPECT_GT(config.stdp_tau_plus_ms, 0.0f);
    EXPECT_GT(config.stdp_tau_minus_ms, 0.0f);
    EXPECT_TRUE(config.protect_broadcast_synapses);
    EXPECT_TRUE(config.protect_integration_synapses);
}

//=============================================================================
// Synapse Management Tests
//=============================================================================

TEST_F(GWPlasticityBridgeTest, RegisterSynapse) {
    EXPECT_EQ(gw_plasticity_register_synapse(bridge, 1, GW_SYNAPSE_BROADCAST, 0.5f), 0);
}

TEST_F(GWPlasticityBridgeTest, RegisterSynapseDuplicate) {
    EXPECT_EQ(gw_plasticity_register_synapse(bridge, 1, GW_SYNAPSE_BROADCAST, 0.5f), 0);
    EXPECT_EQ(gw_plasticity_register_synapse(bridge, 1, GW_SYNAPSE_IGNITION, 0.6f), -1);
}

TEST_F(GWPlasticityBridgeTest, RegisterSynapseNull) {
    EXPECT_EQ(gw_plasticity_register_synapse(nullptr, 1, GW_SYNAPSE_BROADCAST, 0.5f), -1);
}

TEST_F(GWPlasticityBridgeTest, UnregisterSynapse) {
    EXPECT_EQ(gw_plasticity_register_synapse(bridge, 1, GW_SYNAPSE_BROADCAST, 0.5f), 0);
    EXPECT_EQ(gw_plasticity_unregister_synapse(bridge, 1), 0);
}

TEST_F(GWPlasticityBridgeTest, UnregisterNonexistent) {
    EXPECT_EQ(gw_plasticity_unregister_synapse(bridge, 999), -1);
}

TEST_F(GWPlasticityBridgeTest, GetSynapse) {
    EXPECT_EQ(gw_plasticity_register_synapse(bridge, 1, GW_SYNAPSE_BROADCAST, 0.5f), 0);

    gw_plasticity_synapse_t synapse;
    EXPECT_EQ(gw_plasticity_get_synapse(bridge, 1, &synapse), 0);
    EXPECT_EQ(synapse.synapse_id, 1u);
    EXPECT_EQ(synapse.type, GW_SYNAPSE_BROADCAST);
    EXPECT_NEAR(synapse.weight, 0.5f, 0.01f);
}

TEST_F(GWPlasticityBridgeTest, GetSynapseNotFound) {
    gw_plasticity_synapse_t synapse;
    EXPECT_EQ(gw_plasticity_get_synapse(bridge, 999, &synapse), -1);
}

TEST_F(GWPlasticityBridgeTest, ProtectSynapse) {
    EXPECT_EQ(gw_plasticity_register_synapse(bridge, 1, GW_SYNAPSE_COMPETITION, 0.5f), 0);
    EXPECT_EQ(gw_plasticity_protect_synapse(bridge, 1, true), 0);

    gw_plasticity_synapse_t synapse;
    EXPECT_EQ(gw_plasticity_get_synapse(bridge, 1, &synapse), 0);
    EXPECT_TRUE(synapse.is_protected);
}

TEST_F(GWPlasticityBridgeTest, BroadcastSynapseAutoProtected) {
    EXPECT_EQ(gw_plasticity_register_synapse(bridge, 1, GW_SYNAPSE_BROADCAST, 0.5f), 0);

    gw_plasticity_synapse_t synapse;
    EXPECT_EQ(gw_plasticity_get_synapse(bridge, 1, &synapse), 0);
    EXPECT_TRUE(synapse.is_protected);
}

TEST_F(GWPlasticityBridgeTest, IntegrationSynapseAutoProtected) {
    EXPECT_EQ(gw_plasticity_register_synapse(bridge, 1, GW_SYNAPSE_INTEGRATION, 0.5f), 0);

    gw_plasticity_synapse_t synapse;
    EXPECT_EQ(gw_plasticity_get_synapse(bridge, 1, &synapse), 0);
    EXPECT_TRUE(synapse.is_protected);
}

//=============================================================================
// Learning Tests
//=============================================================================

TEST_F(GWPlasticityBridgeTest, LearnBroadcastSuccess) {
    EXPECT_EQ(gw_plasticity_register_synapse(bridge, 1, GW_SYNAPSE_COMPETITION, 0.5f), 0);
    EXPECT_EQ(gw_plasticity_learn(bridge, GW_LEARN_BROADCAST_SUCCESS, 0.8f, 1, 1.0f), 0);
}

TEST_F(GWPlasticityBridgeTest, LearnBroadcastFailure) {
    EXPECT_EQ(gw_plasticity_register_synapse(bridge, 1, GW_SYNAPSE_COMPETITION, 0.5f), 0);
    EXPECT_EQ(gw_plasticity_learn(bridge, GW_LEARN_BROADCAST_FAILURE, 0.6f, 1, 1.0f), 0);
}

TEST_F(GWPlasticityBridgeTest, LearnIgnitionTriggered) {
    EXPECT_EQ(gw_plasticity_register_synapse(bridge, 1, GW_SYNAPSE_IGNITION, 0.5f), 0);
    EXPECT_EQ(gw_plasticity_learn(bridge, GW_LEARN_IGNITION_TRIGGERED, 0.9f, 1, 1.0f), 0);
}

TEST_F(GWPlasticityBridgeTest, LearnCompetitionWon) {
    EXPECT_EQ(gw_plasticity_register_synapse(bridge, 1, GW_SYNAPSE_COALITION, 0.5f), 0);
    EXPECT_EQ(gw_plasticity_learn(bridge, GW_LEARN_COMPETITION_WON, 0.7f, 1, 1.0f), 0);
}

TEST_F(GWPlasticityBridgeTest, LearnProtectedSynapseBlocked) {
    EXPECT_EQ(gw_plasticity_register_synapse(bridge, 1, GW_SYNAPSE_BROADCAST, 0.5f), 0);

    gw_plasticity_synapse_t before;
    EXPECT_EQ(gw_plasticity_get_synapse(bridge, 1, &before), 0);

    // Attempt to learn on protected synapse
    EXPECT_EQ(gw_plasticity_learn(bridge, GW_LEARN_BROADCAST_SUCCESS, 1.0f, 1, 1.0f), 0);

    gw_plasticity_synapse_t after;
    EXPECT_EQ(gw_plasticity_get_synapse(bridge, 1, &after), 0);

    // Weight should be unchanged
    EXPECT_NEAR(before.weight, after.weight, 0.001f);
}

TEST_F(GWPlasticityBridgeTest, LearnNull) {
    EXPECT_EQ(gw_plasticity_learn(nullptr, GW_LEARN_BROADCAST_SUCCESS, 0.5f, 1, 1.0f), -1);
}

//=============================================================================
// STDP Tests
//=============================================================================

TEST_F(GWPlasticityBridgeTest, STDPPotentiation) {
    EXPECT_EQ(gw_plasticity_register_synapse(bridge, 1, GW_SYNAPSE_COALITION, 0.5f), 0);

    // Post after pre -> potentiation
    float delta = gw_plasticity_apply_stdp(bridge, 1, 0.0f, 10.0f);
    EXPECT_FALSE(std::isnan(delta));
    EXPECT_GT(delta, 0.0f);
}

TEST_F(GWPlasticityBridgeTest, STDPDepression) {
    EXPECT_EQ(gw_plasticity_register_synapse(bridge, 1, GW_SYNAPSE_COALITION, 0.5f), 0);

    // Pre after post -> depression
    float delta = gw_plasticity_apply_stdp(bridge, 1, 10.0f, 0.0f);
    EXPECT_FALSE(std::isnan(delta));
    EXPECT_LT(delta, 0.0f);
}

TEST_F(GWPlasticityBridgeTest, STDPProtectedBlocked) {
    EXPECT_EQ(gw_plasticity_register_synapse(bridge, 1, GW_SYNAPSE_BROADCAST, 0.5f), 0);

    float delta = gw_plasticity_apply_stdp(bridge, 1, 0.0f, 10.0f);
    EXPECT_NEAR(delta, 0.0f, 0.001f);
}

TEST_F(GWPlasticityBridgeTest, STDPNull) {
    float delta = gw_plasticity_apply_stdp(nullptr, 1, 0.0f, 10.0f);
    EXPECT_TRUE(std::isnan(delta));
}

//=============================================================================
// Reward Modulation Tests
//=============================================================================

TEST_F(GWPlasticityBridgeTest, ApplyPositiveReward) {
    EXPECT_EQ(gw_plasticity_register_synapse(bridge, 1, GW_SYNAPSE_COALITION, 0.5f), 0);

    // Set eligibility by learning
    gw_plasticity_learn(bridge, GW_LEARN_COMPETITION_WON, 0.5f, 1, 1.0f);

    EXPECT_EQ(gw_plasticity_apply_reward(bridge, 0.5f), 0);
}

TEST_F(GWPlasticityBridgeTest, ApplyNegativeReward) {
    EXPECT_EQ(gw_plasticity_register_synapse(bridge, 1, GW_SYNAPSE_COALITION, 0.5f), 0);
    EXPECT_EQ(gw_plasticity_apply_reward(bridge, -0.5f), 0);
}

TEST_F(GWPlasticityBridgeTest, ApplyRewardNull) {
    EXPECT_EQ(gw_plasticity_apply_reward(nullptr, 0.5f), -1);
}

//=============================================================================
// BCM Tests
//=============================================================================

TEST_F(GWPlasticityBridgeTest, UpdateBCM) {
    EXPECT_EQ(gw_plasticity_register_synapse(bridge, 1, GW_SYNAPSE_COALITION, 0.5f), 0);
    EXPECT_EQ(gw_plasticity_update_bcm(bridge, 10.0f), 0);
}

TEST_F(GWPlasticityBridgeTest, UpdateBCMNull) {
    EXPECT_EQ(gw_plasticity_update_bcm(nullptr, 10.0f), -1);
}

TEST_F(GWPlasticityBridgeTest, UpdateBCMInvalidDt) {
    EXPECT_EQ(gw_plasticity_update_bcm(bridge, 0.0f), -1);
    EXPECT_EQ(gw_plasticity_update_bcm(bridge, -10.0f), -1);
}

//=============================================================================
// Homeostatic Tests
//=============================================================================

TEST_F(GWPlasticityBridgeTest, HomeostaticUpdate) {
    EXPECT_EQ(gw_plasticity_register_synapse(bridge, 1, GW_SYNAPSE_COALITION, 0.5f), 0);
    EXPECT_EQ(gw_plasticity_homeostatic_update(bridge, 10.0f), 0);
}

TEST_F(GWPlasticityBridgeTest, HomeostaticNull) {
    EXPECT_EQ(gw_plasticity_homeostatic_update(nullptr, 10.0f), -1);
}

//=============================================================================
// Trace Update Tests
//=============================================================================

TEST_F(GWPlasticityBridgeTest, UpdateTraces) {
    EXPECT_EQ(gw_plasticity_register_synapse(bridge, 1, GW_SYNAPSE_COALITION, 0.5f), 0);
    EXPECT_EQ(gw_plasticity_update_traces(bridge, 10.0f), 0);
}

TEST_F(GWPlasticityBridgeTest, UpdateTracesNull) {
    EXPECT_EQ(gw_plasticity_update_traces(nullptr, 10.0f), -1);
}

//=============================================================================
// Consolidation Tests
//=============================================================================

TEST_F(GWPlasticityBridgeTest, Consolidate) {
    EXPECT_EQ(gw_plasticity_register_synapse(bridge, 1, GW_SYNAPSE_COALITION, 0.5f), 0);
    EXPECT_EQ(gw_plasticity_consolidate(bridge), 0);
}

TEST_F(GWPlasticityBridgeTest, ConsolidateNull) {
    EXPECT_EQ(gw_plasticity_consolidate(nullptr), -1);
}

//=============================================================================
// State Query Tests
//=============================================================================

TEST_F(GWPlasticityBridgeTest, GetAccessLearningState) {
    gw_access_learning_state_t state;
    EXPECT_EQ(gw_plasticity_get_access_learning_state(bridge, &state), 0);
    EXPECT_GE(state.broadcast_sensitivity, 0.0f);
    EXPECT_LE(state.broadcast_sensitivity, 1.0f);
}

TEST_F(GWPlasticityBridgeTest, GetBridgeState) {
    gw_plasticity_bridge_state_t state;
    EXPECT_EQ(gw_plasticity_get_state(bridge, &state), 0);
    EXPECT_EQ(state.state, GW_PLASTICITY_STATE_IDLE);
}

TEST_F(GWPlasticityBridgeTest, GetStats) {
    gw_plasticity_stats_t stats;
    EXPECT_EQ(gw_plasticity_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.total_learning_events, 0u);
}

TEST_F(GWPlasticityBridgeTest, ResetStats) {
    EXPECT_EQ(gw_plasticity_register_synapse(bridge, 1, GW_SYNAPSE_COALITION, 0.5f), 0);
    gw_plasticity_learn(bridge, GW_LEARN_BROADCAST_SUCCESS, 0.5f, 1, 1.0f);

    gw_plasticity_stats_t stats;
    gw_plasticity_get_stats(bridge, &stats);
    EXPECT_GT(stats.total_learning_events, 0u);

    EXPECT_EQ(gw_plasticity_reset_stats(bridge), 0);
    gw_plasticity_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_learning_events, 0u);
}

//=============================================================================
// Callback Tests
//=============================================================================

static int learn_callback_count = 0;
static void test_learn_callback(gw_plasticity_bridge_t*, gw_learn_event_t, float, void*) {
    learn_callback_count++;
}

TEST_F(GWPlasticityBridgeTest, RegisterLearnCallback) {
    EXPECT_EQ(gw_plasticity_register_learn_callback(bridge, test_learn_callback, nullptr), 0);
}

TEST_F(GWPlasticityBridgeTest, RegisterAccessCallback) {
    EXPECT_EQ(gw_plasticity_register_access_callback(bridge, nullptr, nullptr), 0);
}

//=============================================================================
// Bio-Async Tests
//=============================================================================

TEST_F(GWPlasticityBridgeTest, BioAsyncNotEnabled) {
    EXPECT_EQ(gw_plasticity_bio_async_connect(bridge), -1);
    EXPECT_FALSE(gw_plasticity_is_bio_async_connected(bridge));
}

TEST_F(GWPlasticityBridgeTest, BioAsyncDisconnectSafe) {
    EXPECT_EQ(gw_plasticity_bio_async_disconnect(bridge), 0);
}

//=============================================================================
// Integration Workflow Tests
//=============================================================================

TEST_F(GWPlasticityBridgeTest, BroadcastLearningWorkflow) {
    // Register coalition synapse
    EXPECT_EQ(gw_plasticity_register_synapse(bridge, 1, GW_SYNAPSE_COALITION, 0.5f), 0);

    gw_plasticity_synapse_t before;
    EXPECT_EQ(gw_plasticity_get_synapse(bridge, 1, &before), 0);

    // Learn from broadcast success
    EXPECT_EQ(gw_plasticity_learn(bridge, GW_LEARN_BROADCAST_SUCCESS, 0.8f, 1, 1.0f), 0);

    gw_plasticity_synapse_t after;
    EXPECT_EQ(gw_plasticity_get_synapse(bridge, 1, &after), 0);

    // Weight should increase
    EXPECT_GT(after.weight, before.weight);
}

TEST_F(GWPlasticityBridgeTest, CompetitionLearningWorkflow) {
    // Register coalition synapse
    EXPECT_EQ(gw_plasticity_register_synapse(bridge, 1, GW_SYNAPSE_COALITION, 0.5f), 0);

    // Win competition
    EXPECT_EQ(gw_plasticity_learn(bridge, GW_LEARN_COMPETITION_WON, 0.9f, 1, 1.0f), 0);

    // Get stats
    gw_plasticity_stats_t stats;
    EXPECT_EQ(gw_plasticity_get_stats(bridge, &stats), 0);
    EXPECT_GT(stats.competition_events, 0u);
}

TEST_F(GWPlasticityBridgeTest, STDPLearningWorkflow) {
    // Register synapse
    EXPECT_EQ(gw_plasticity_register_synapse(bridge, 1, GW_SYNAPSE_COALITION, 0.5f), 0);

    gw_plasticity_synapse_t before;
    EXPECT_EQ(gw_plasticity_get_synapse(bridge, 1, &before), 0);

    // Apply STDP potentiation
    float delta = gw_plasticity_apply_stdp(bridge, 1, 0.0f, 15.0f);
    EXPECT_GT(delta, 0.0f);

    gw_plasticity_synapse_t after;
    EXPECT_EQ(gw_plasticity_get_synapse(bridge, 1, &after), 0);

    // Weight should increase
    EXPECT_GT(after.weight, before.weight);
}
