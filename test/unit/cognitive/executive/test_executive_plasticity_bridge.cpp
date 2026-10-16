/**
 * @file test_executive_plasticity_bridge.cpp
 * @brief Unit tests for Executive Plasticity Bridge
 */

#include <gtest/gtest.h>
#include <cmath>

#include "cognitive/executive/nimcp_executive_plasticity_bridge.h"

class ExecutivePlasticityBridgeTest : public ::testing::Test {
protected:
    executive_plasticity_bridge_t* bridge = nullptr;

    void SetUp() override {
        executive_plasticity_config_t config = executive_plasticity_config_default();
        config.enable_bio_async = false;
        bridge = executive_plasticity_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            executive_plasticity_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(ExecutivePlasticityBridgeTest, CreateWithDefaults) {
    executive_plasticity_bridge_t* test_bridge = executive_plasticity_create(nullptr);
    ASSERT_NE(test_bridge, nullptr);
    executive_plasticity_destroy(test_bridge);
}

TEST_F(ExecutivePlasticityBridgeTest, CreateWithConfig) {
    executive_plasticity_config_t config = executive_plasticity_config_default();
    config.base_learning_rate = 0.005f;
    config.max_synapses = 128;
    executive_plasticity_bridge_t* test_bridge = executive_plasticity_create(&config);
    ASSERT_NE(test_bridge, nullptr);
    executive_plasticity_destroy(test_bridge);
}

TEST_F(ExecutivePlasticityBridgeTest, Reset) {
    EXPECT_EQ(executive_plasticity_reset(bridge), 0);
}

TEST_F(ExecutivePlasticityBridgeTest, ResetNull) {
    EXPECT_EQ(executive_plasticity_reset(nullptr), -1);
}

TEST_F(ExecutivePlasticityBridgeTest, DestroyNull) {
    executive_plasticity_destroy(nullptr); // Should not crash
}

//=============================================================================
// Default Config Tests
//=============================================================================

TEST_F(ExecutivePlasticityBridgeTest, DefaultConfigValues) {
    executive_plasticity_config_t config = executive_plasticity_config_default();
    EXPECT_EQ(config.base_learning_rate, EXECUTIVE_PLASTICITY_DEFAULT_LR);
    EXPECT_EQ(config.max_synapses, EXECUTIVE_PLASTICITY_MAX_SYNAPSES);
    EXPECT_GT(config.stdp_tau_plus_ms, 0.0f);
    EXPECT_GT(config.stdp_tau_minus_ms, 0.0f);
    EXPECT_TRUE(config.protect_inhibition_circuits);
}

//=============================================================================
// Synapse Management Tests
//=============================================================================

TEST_F(ExecutivePlasticityBridgeTest, RegisterSynapse) {
    EXPECT_EQ(executive_plasticity_register_synapse(bridge, 1, EXEC_SYNAPSE_FLEXIBILITY, 0.5f), 0);
}

TEST_F(ExecutivePlasticityBridgeTest, RegisterSynapseNull) {
    EXPECT_EQ(executive_plasticity_register_synapse(nullptr, 1, EXEC_SYNAPSE_FLEXIBILITY, 0.5f), -1);
}

TEST_F(ExecutivePlasticityBridgeTest, RegisterDuplicateSynapse) {
    EXPECT_EQ(executive_plasticity_register_synapse(bridge, 1, EXEC_SYNAPSE_FLEXIBILITY, 0.5f), 0);
    EXPECT_EQ(executive_plasticity_register_synapse(bridge, 1, EXEC_SYNAPSE_FLEXIBILITY, 0.5f), -1);
}

TEST_F(ExecutivePlasticityBridgeTest, UnregisterSynapse) {
    executive_plasticity_register_synapse(bridge, 1, EXEC_SYNAPSE_FLEXIBILITY, 0.5f);
    EXPECT_EQ(executive_plasticity_unregister_synapse(bridge, 1), 0);
}

TEST_F(ExecutivePlasticityBridgeTest, UnregisterNonexistentSynapse) {
    EXPECT_EQ(executive_plasticity_unregister_synapse(bridge, 999), -1);
}

TEST_F(ExecutivePlasticityBridgeTest, GetSynapse) {
    executive_plasticity_register_synapse(bridge, 1, EXEC_SYNAPSE_FLEXIBILITY, 0.5f);

    executive_plasticity_synapse_t syn;
    EXPECT_EQ(executive_plasticity_get_synapse(bridge, 1, &syn), 0);
    EXPECT_EQ(syn.synapse_id, 1u);
    EXPECT_EQ(syn.type, EXEC_SYNAPSE_FLEXIBILITY);
    EXPECT_FLOAT_EQ(syn.weight, 0.5f);
}

TEST_F(ExecutivePlasticityBridgeTest, GetSynapseNonexistent) {
    executive_plasticity_synapse_t syn;
    EXPECT_EQ(executive_plasticity_get_synapse(bridge, 999, &syn), -1);
}

TEST_F(ExecutivePlasticityBridgeTest, ProtectSynapse) {
    executive_plasticity_register_synapse(bridge, 1, EXEC_SYNAPSE_FLEXIBILITY, 0.5f);
    EXPECT_EQ(executive_plasticity_protect_synapse(bridge, 1, true), 0);

    executive_plasticity_synapse_t syn;
    executive_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_TRUE(syn.is_protected);
}

TEST_F(ExecutivePlasticityBridgeTest, AutoProtectInhibition) {
    executive_plasticity_register_synapse(bridge, 1, EXEC_SYNAPSE_INHIBITION, 0.5f);

    executive_plasticity_synapse_t syn;
    executive_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_TRUE(syn.is_protected);
}

TEST_F(ExecutivePlasticityBridgeTest, AutoProtectGoal) {
    executive_plasticity_register_synapse(bridge, 1, EXEC_SYNAPSE_GOAL, 0.5f);

    executive_plasticity_synapse_t syn;
    executive_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_TRUE(syn.is_protected);
}

//=============================================================================
// Learning Tests
//=============================================================================

TEST_F(ExecutivePlasticityBridgeTest, LearnSuccessfulInhibition) {
    executive_plasticity_register_synapse(bridge, 1, EXEC_SYNAPSE_FLEXIBILITY, 0.5f);
    EXPECT_EQ(executive_plasticity_learn(bridge, EXEC_LEARN_SUCCESSFUL_INHIBITION, 0.8f, 1, 0.9f), 0);

    executive_plasticity_synapse_t syn;
    executive_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_GT(syn.weight, 0.5f);
}

TEST_F(ExecutivePlasticityBridgeTest, LearnFailedInhibition) {
    executive_plasticity_register_synapse(bridge, 1, EXEC_SYNAPSE_FLEXIBILITY, 0.5f);
    EXPECT_EQ(executive_plasticity_learn(bridge, EXEC_LEARN_FAILED_INHIBITION, 0.8f, 1, 0.9f), 0);

    executive_plasticity_synapse_t syn;
    executive_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_LT(syn.weight, 0.5f);
}

TEST_F(ExecutivePlasticityBridgeTest, LearnNull) {
    EXPECT_EQ(executive_plasticity_learn(nullptr, EXEC_LEARN_SUCCESSFUL_INHIBITION, 0.5f, 1, 0.5f), -1);
}

TEST_F(ExecutivePlasticityBridgeTest, LearnProtectedSynapse) {
    executive_plasticity_register_synapse(bridge, 1, EXEC_SYNAPSE_INHIBITION, 0.5f);

    executive_plasticity_synapse_t before;
    executive_plasticity_get_synapse(bridge, 1, &before);

    executive_plasticity_learn(bridge, EXEC_LEARN_SUCCESSFUL_INHIBITION, 1.0f, 1, 1.0f);

    executive_plasticity_synapse_t after;
    executive_plasticity_get_synapse(bridge, 1, &after);
    EXPECT_FLOAT_EQ(before.weight, after.weight);
}

//=============================================================================
// STDP Tests
//=============================================================================

TEST_F(ExecutivePlasticityBridgeTest, ApplySTDPPotentiation) {
    executive_plasticity_register_synapse(bridge, 1, EXEC_SYNAPSE_FLEXIBILITY, 0.5f);

    float delta = executive_plasticity_apply_stdp(bridge, 1, 10.0f, 15.0f);
    EXPECT_TRUE(std::isfinite(delta));
    EXPECT_GT(delta, 0.0f);
}

TEST_F(ExecutivePlasticityBridgeTest, ApplySTDPDepression) {
    executive_plasticity_register_synapse(bridge, 1, EXEC_SYNAPSE_FLEXIBILITY, 0.5f);

    float delta = executive_plasticity_apply_stdp(bridge, 1, 15.0f, 10.0f);
    EXPECT_TRUE(std::isfinite(delta));
    EXPECT_LT(delta, 0.0f);
}

TEST_F(ExecutivePlasticityBridgeTest, ApplySTDPNull) {
    float delta = executive_plasticity_apply_stdp(nullptr, 1, 10.0f, 15.0f);
    EXPECT_TRUE(std::isnan(delta));
}

TEST_F(ExecutivePlasticityBridgeTest, ApplySTDPProtected) {
    executive_plasticity_register_synapse(bridge, 1, EXEC_SYNAPSE_INHIBITION, 0.5f);

    float delta = executive_plasticity_apply_stdp(bridge, 1, 10.0f, 15.0f);
    EXPECT_FLOAT_EQ(delta, 0.0f);
}

//=============================================================================
// Reward Modulation Tests
//=============================================================================

TEST_F(ExecutivePlasticityBridgeTest, ApplyReward) {
    executive_plasticity_register_synapse(bridge, 1, EXEC_SYNAPSE_FLEXIBILITY, 0.5f);
    EXPECT_EQ(executive_plasticity_apply_reward(bridge, 0.5f), 0);
}

TEST_F(ExecutivePlasticityBridgeTest, ApplyRewardNull) {
    EXPECT_EQ(executive_plasticity_apply_reward(nullptr, 0.5f), -1);
}

TEST_F(ExecutivePlasticityBridgeTest, ApplyRewardClamped) {
    executive_plasticity_register_synapse(bridge, 1, EXEC_SYNAPSE_FLEXIBILITY, 0.5f);
    EXPECT_EQ(executive_plasticity_apply_reward(bridge, 10.0f), 0); // Should clamp to 1.0
    EXPECT_EQ(executive_plasticity_apply_reward(bridge, -10.0f), 0); // Should clamp to -1.0
}

//=============================================================================
// BCM and Homeostatic Tests
//=============================================================================

TEST_F(ExecutivePlasticityBridgeTest, UpdateBCM) {
    executive_plasticity_register_synapse(bridge, 1, EXEC_SYNAPSE_FLEXIBILITY, 0.5f);
    EXPECT_EQ(executive_plasticity_update_bcm(bridge, 10.0f), 0);
}

TEST_F(ExecutivePlasticityBridgeTest, UpdateBCMNull) {
    EXPECT_EQ(executive_plasticity_update_bcm(nullptr, 10.0f), -1);
}

TEST_F(ExecutivePlasticityBridgeTest, UpdateBCMZeroDt) {
    EXPECT_EQ(executive_plasticity_update_bcm(bridge, 0.0f), -1);
}

TEST_F(ExecutivePlasticityBridgeTest, HomeostaticUpdate) {
    executive_plasticity_register_synapse(bridge, 1, EXEC_SYNAPSE_FLEXIBILITY, 0.5f);
    EXPECT_EQ(executive_plasticity_homeostatic_update(bridge, 10.0f), 0);
}

TEST_F(ExecutivePlasticityBridgeTest, HomeostaticUpdateNull) {
    EXPECT_EQ(executive_plasticity_homeostatic_update(nullptr, 10.0f), -1);
}

//=============================================================================
// Trace and Consolidation Tests
//=============================================================================

TEST_F(ExecutivePlasticityBridgeTest, UpdateTraces) {
    executive_plasticity_register_synapse(bridge, 1, EXEC_SYNAPSE_FLEXIBILITY, 0.5f);
    EXPECT_EQ(executive_plasticity_update_traces(bridge, 10.0f), 0);
}

TEST_F(ExecutivePlasticityBridgeTest, UpdateTracesNull) {
    EXPECT_EQ(executive_plasticity_update_traces(nullptr, 10.0f), -1);
}

TEST_F(ExecutivePlasticityBridgeTest, Consolidate) {
    executive_plasticity_register_synapse(bridge, 1, EXEC_SYNAPSE_FLEXIBILITY, 0.5f);
    EXPECT_EQ(executive_plasticity_consolidate(bridge), 0);
}

TEST_F(ExecutivePlasticityBridgeTest, ConsolidateNull) {
    EXPECT_EQ(executive_plasticity_consolidate(nullptr), -1);
}

//=============================================================================
// State Query Tests
//=============================================================================

TEST_F(ExecutivePlasticityBridgeTest, GetCalibrationState) {
    executive_calibration_state_t state;
    EXPECT_EQ(executive_plasticity_get_calibration_state(bridge, &state), 0);
    EXPECT_GE(state.control_calibration, 0.0f);
    EXPECT_LE(state.control_calibration, 1.0f);
}

TEST_F(ExecutivePlasticityBridgeTest, GetCalibrationStateNull) {
    EXPECT_EQ(executive_plasticity_get_calibration_state(nullptr, nullptr), -1);
    executive_calibration_state_t state;
    EXPECT_EQ(executive_plasticity_get_calibration_state(nullptr, &state), -1);
    EXPECT_EQ(executive_plasticity_get_calibration_state(bridge, nullptr), -1);
}

TEST_F(ExecutivePlasticityBridgeTest, GetBridgeState) {
    executive_plasticity_bridge_state_t state;
    EXPECT_EQ(executive_plasticity_get_state(bridge, &state), 0);
    EXPECT_EQ(state.state, EXECUTIVE_PLASTICITY_STATE_IDLE);
    EXPECT_EQ(state.active_synapses, 0u);
}

TEST_F(ExecutivePlasticityBridgeTest, GetStats) {
    executive_plasticity_stats_t stats;
    EXPECT_EQ(executive_plasticity_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.total_learning_events, 0u);
}

TEST_F(ExecutivePlasticityBridgeTest, ResetStats) {
    executive_plasticity_register_synapse(bridge, 1, EXEC_SYNAPSE_FLEXIBILITY, 0.5f);
    executive_plasticity_learn(bridge, EXEC_LEARN_SUCCESSFUL_INHIBITION, 0.5f, 1, 0.5f);

    executive_plasticity_stats_t stats;
    executive_plasticity_get_stats(bridge, &stats);
    EXPECT_GT(stats.total_learning_events, 0u);

    EXPECT_EQ(executive_plasticity_reset_stats(bridge), 0);
    executive_plasticity_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_learning_events, 0u);
}

//=============================================================================
// Callback Tests
//=============================================================================

TEST_F(ExecutivePlasticityBridgeTest, RegisterLearnCallback) {
    EXPECT_EQ(executive_plasticity_register_learn_callback(bridge, nullptr, nullptr), 0);
}

TEST_F(ExecutivePlasticityBridgeTest, RegisterCalibrationCallback) {
    EXPECT_EQ(executive_plasticity_register_calibration_callback(bridge, nullptr, nullptr), 0);
}

//=============================================================================
// Bio-Async Tests
//=============================================================================

TEST_F(ExecutivePlasticityBridgeTest, BioAsyncNotEnabled) {
    EXPECT_EQ(executive_plasticity_bio_async_connect(bridge), -1);
    EXPECT_FALSE(executive_plasticity_is_bio_async_connected(bridge));
}

TEST_F(ExecutivePlasticityBridgeTest, BioAsyncDisconnectSafe) {
    EXPECT_EQ(executive_plasticity_bio_async_disconnect(bridge), 0);
}

//=============================================================================
// Integration Workflow Tests
//=============================================================================

TEST_F(ExecutivePlasticityBridgeTest, FullLearningWorkflow) {
    // Register synapses
    for (int i = 0; i < 10; i++) {
        executive_plasticity_register_synapse(bridge, i, EXEC_SYNAPSE_FLEXIBILITY, 0.5f);
    }

    // Learning cycle
    for (int cycle = 0; cycle < 20; cycle++) {
        for (int i = 0; i < 10; i++) {
            executive_plasticity_learn(bridge, EXEC_LEARN_SUCCESSFUL_INHIBITION, 0.3f, i, 0.8f);
            executive_plasticity_apply_stdp(bridge, i, (float)cycle, (float)cycle + 5.0f);
        }
        executive_plasticity_apply_reward(bridge, 0.5f);
        executive_plasticity_update_traces(bridge, 1.0f);
    }

    executive_plasticity_update_bcm(bridge, 10.0f);
    executive_plasticity_homeostatic_update(bridge, 10.0f);
    executive_plasticity_consolidate(bridge);

    executive_plasticity_stats_t stats;
    executive_plasticity_get_stats(bridge, &stats);
    EXPECT_GT(stats.total_learning_events, 100u);
    EXPECT_GT(stats.weight_updates, 100u);
}

TEST_F(ExecutivePlasticityBridgeTest, ExecutiveControlLearning) {
    // Use FLEXIBILITY synapse (not auto-protected like INHIBITION)
    executive_plasticity_register_synapse(bridge, 1, EXEC_SYNAPSE_FLEXIBILITY, 0.5f);

    for (int i = 0; i < 50; i++) {
        executive_plasticity_learn(bridge, EXEC_LEARN_TASK_SWITCH_SUCCESS, 0.2f, 1, 0.7f);
    }

    executive_plasticity_synapse_t syn;
    executive_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_NE(syn.weight, 0.5f);
}
