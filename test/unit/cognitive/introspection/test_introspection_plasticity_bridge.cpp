/**
 * @file test_introspection_plasticity_bridge.cpp
 * @brief Unit tests for Introspection Plasticity Bridge
 */

#include <gtest/gtest.h>
#include <cmath>

#include "cognitive/introspection/nimcp_introspection_plasticity_bridge.h"

class IntrospectionPlasticityBridgeTest : public ::testing::Test {
protected:
    introspection_plasticity_bridge_t* bridge = nullptr;

    void SetUp() override {
        introspection_plasticity_config_t config = introspection_plasticity_config_default();
        config.enable_bio_async = false;
        bridge = introspection_plasticity_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            introspection_plasticity_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(IntrospectionPlasticityBridgeTest, CreateWithDefaults) {
    introspection_plasticity_bridge_t* test_bridge = introspection_plasticity_create(nullptr);
    ASSERT_NE(test_bridge, nullptr);
    introspection_plasticity_destroy(test_bridge);
}

TEST_F(IntrospectionPlasticityBridgeTest, CreateWithConfig) {
    introspection_plasticity_config_t config = introspection_plasticity_config_default();
    config.base_learning_rate = 0.005f;
    config.max_synapses = 128;
    introspection_plasticity_bridge_t* test_bridge = introspection_plasticity_create(&config);
    ASSERT_NE(test_bridge, nullptr);
    introspection_plasticity_destroy(test_bridge);
}

TEST_F(IntrospectionPlasticityBridgeTest, Reset) {
    EXPECT_EQ(introspection_plasticity_reset(bridge), 0);
}

TEST_F(IntrospectionPlasticityBridgeTest, ResetNull) {
    EXPECT_EQ(introspection_plasticity_reset(nullptr), -1);
}

TEST_F(IntrospectionPlasticityBridgeTest, DestroyNull) {
    introspection_plasticity_destroy(nullptr); // Should not crash
}

//=============================================================================
// Default Config Tests
//=============================================================================

TEST_F(IntrospectionPlasticityBridgeTest, DefaultConfigValues) {
    introspection_plasticity_config_t config = introspection_plasticity_config_default();
    EXPECT_EQ(config.base_learning_rate, INTROSPECTION_PLASTICITY_DEFAULT_LR);
    EXPECT_EQ(config.max_synapses, INTROSPECTION_PLASTICITY_MAX_SYNAPSES);
    EXPECT_GT(config.stdp_tau_plus_ms, 0.0f);
    EXPECT_GT(config.stdp_tau_minus_ms, 0.0f);
    EXPECT_TRUE(config.protect_core_patterns);
}

//=============================================================================
// Synapse Management Tests
//=============================================================================

TEST_F(IntrospectionPlasticityBridgeTest, RegisterSynapse) {
    EXPECT_EQ(introspection_plasticity_register_synapse(bridge, 1, INTROSPECTION_SYNAPSE_CONFIDENCE, 0.5f), 0);
}

TEST_F(IntrospectionPlasticityBridgeTest, RegisterSynapseNull) {
    EXPECT_EQ(introspection_plasticity_register_synapse(nullptr, 1, INTROSPECTION_SYNAPSE_CONFIDENCE, 0.5f), -1);
}

TEST_F(IntrospectionPlasticityBridgeTest, RegisterDuplicateSynapse) {
    EXPECT_EQ(introspection_plasticity_register_synapse(bridge, 1, INTROSPECTION_SYNAPSE_CONFIDENCE, 0.5f), 0);
    EXPECT_EQ(introspection_plasticity_register_synapse(bridge, 1, INTROSPECTION_SYNAPSE_CONFIDENCE, 0.5f), -1);
}

TEST_F(IntrospectionPlasticityBridgeTest, UnregisterSynapse) {
    introspection_plasticity_register_synapse(bridge, 1, INTROSPECTION_SYNAPSE_CONFIDENCE, 0.5f);
    EXPECT_EQ(introspection_plasticity_unregister_synapse(bridge, 1), 0);
}

TEST_F(IntrospectionPlasticityBridgeTest, UnregisterNonexistentSynapse) {
    EXPECT_EQ(introspection_plasticity_unregister_synapse(bridge, 999), -1);
}

TEST_F(IntrospectionPlasticityBridgeTest, GetSynapse) {
    introspection_plasticity_register_synapse(bridge, 1, INTROSPECTION_SYNAPSE_CONFIDENCE, 0.5f);

    introspection_plasticity_synapse_t syn;
    EXPECT_EQ(introspection_plasticity_get_synapse(bridge, 1, &syn), 0);
    EXPECT_EQ(syn.synapse_id, 1u);
    EXPECT_EQ(syn.type, INTROSPECTION_SYNAPSE_CONFIDENCE);
    EXPECT_FLOAT_EQ(syn.weight, 0.5f);
}

TEST_F(IntrospectionPlasticityBridgeTest, GetSynapseNonexistent) {
    introspection_plasticity_synapse_t syn;
    EXPECT_EQ(introspection_plasticity_get_synapse(bridge, 999, &syn), -1);
}

TEST_F(IntrospectionPlasticityBridgeTest, ProtectSynapse) {
    introspection_plasticity_register_synapse(bridge, 1, INTROSPECTION_SYNAPSE_CONFIDENCE, 0.5f);
    EXPECT_EQ(introspection_plasticity_protect_synapse(bridge, 1, true), 0);

    introspection_plasticity_synapse_t syn;
    introspection_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_TRUE(syn.is_protected);
}

TEST_F(IntrospectionPlasticityBridgeTest, AutoProtectMetacognition) {
    introspection_plasticity_register_synapse(bridge, 1, INTROSPECTION_SYNAPSE_METACOGNITION, 0.5f);

    introspection_plasticity_synapse_t syn;
    introspection_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_TRUE(syn.is_protected);
}

//=============================================================================
// Learning Tests
//=============================================================================

TEST_F(IntrospectionPlasticityBridgeTest, LearnCorrectConfidence) {
    introspection_plasticity_register_synapse(bridge, 1, INTROSPECTION_SYNAPSE_CONFIDENCE, 0.5f);
    EXPECT_EQ(introspection_plasticity_learn(bridge, INTROSPECTION_LEARN_CORRECT_CONFIDENCE, 0.8f, 1, 0.9f), 0);

    introspection_plasticity_synapse_t syn;
    introspection_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_GT(syn.weight, 0.5f);
}

TEST_F(IntrospectionPlasticityBridgeTest, LearnOverconfidence) {
    introspection_plasticity_register_synapse(bridge, 1, INTROSPECTION_SYNAPSE_CONFIDENCE, 0.5f);
    EXPECT_EQ(introspection_plasticity_learn(bridge, INTROSPECTION_LEARN_OVERCONFIDENCE, 0.8f, 1, 0.9f), 0);

    introspection_plasticity_synapse_t syn;
    introspection_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_LT(syn.weight, 0.5f);
}

TEST_F(IntrospectionPlasticityBridgeTest, LearnNull) {
    EXPECT_EQ(introspection_plasticity_learn(nullptr, INTROSPECTION_LEARN_CORRECT_CONFIDENCE, 0.5f, 1, 0.5f), -1);
}

TEST_F(IntrospectionPlasticityBridgeTest, LearnProtectedSynapse) {
    introspection_plasticity_register_synapse(bridge, 1, INTROSPECTION_SYNAPSE_METACOGNITION, 0.5f);

    introspection_plasticity_synapse_t before;
    introspection_plasticity_get_synapse(bridge, 1, &before);

    introspection_plasticity_learn(bridge, INTROSPECTION_LEARN_CORRECT_CONFIDENCE, 1.0f, 1, 1.0f);

    introspection_plasticity_synapse_t after;
    introspection_plasticity_get_synapse(bridge, 1, &after);
    EXPECT_FLOAT_EQ(before.weight, after.weight);
}

//=============================================================================
// STDP Tests
//=============================================================================

TEST_F(IntrospectionPlasticityBridgeTest, ApplySTDPPotentiation) {
    introspection_plasticity_register_synapse(bridge, 1, INTROSPECTION_SYNAPSE_CONFIDENCE, 0.5f);

    float delta = introspection_plasticity_apply_stdp(bridge, 1, 10.0f, 15.0f);
    EXPECT_TRUE(std::isfinite(delta));
    EXPECT_GT(delta, 0.0f);
}

TEST_F(IntrospectionPlasticityBridgeTest, ApplySTDPDepression) {
    introspection_plasticity_register_synapse(bridge, 1, INTROSPECTION_SYNAPSE_CONFIDENCE, 0.5f);

    float delta = introspection_plasticity_apply_stdp(bridge, 1, 15.0f, 10.0f);
    EXPECT_TRUE(std::isfinite(delta));
    EXPECT_LT(delta, 0.0f);
}

TEST_F(IntrospectionPlasticityBridgeTest, ApplySTDPNull) {
    float delta = introspection_plasticity_apply_stdp(nullptr, 1, 10.0f, 15.0f);
    EXPECT_TRUE(std::isnan(delta));
}

TEST_F(IntrospectionPlasticityBridgeTest, ApplySTDPProtected) {
    introspection_plasticity_register_synapse(bridge, 1, INTROSPECTION_SYNAPSE_METACOGNITION, 0.5f);

    float delta = introspection_plasticity_apply_stdp(bridge, 1, 10.0f, 15.0f);
    EXPECT_FLOAT_EQ(delta, 0.0f);
}

//=============================================================================
// Reward Modulation Tests
//=============================================================================

TEST_F(IntrospectionPlasticityBridgeTest, ApplyReward) {
    introspection_plasticity_register_synapse(bridge, 1, INTROSPECTION_SYNAPSE_CONFIDENCE, 0.5f);
    EXPECT_EQ(introspection_plasticity_apply_reward(bridge, 0.5f), 0);
}

TEST_F(IntrospectionPlasticityBridgeTest, ApplyRewardNull) {
    EXPECT_EQ(introspection_plasticity_apply_reward(nullptr, 0.5f), -1);
}

TEST_F(IntrospectionPlasticityBridgeTest, ApplyRewardClamped) {
    introspection_plasticity_register_synapse(bridge, 1, INTROSPECTION_SYNAPSE_CONFIDENCE, 0.5f);
    EXPECT_EQ(introspection_plasticity_apply_reward(bridge, 10.0f), 0); // Should clamp to 1.0
    EXPECT_EQ(introspection_plasticity_apply_reward(bridge, -10.0f), 0); // Should clamp to -1.0
}

//=============================================================================
// BCM and Homeostatic Tests
//=============================================================================

TEST_F(IntrospectionPlasticityBridgeTest, UpdateBCM) {
    introspection_plasticity_register_synapse(bridge, 1, INTROSPECTION_SYNAPSE_CONFIDENCE, 0.5f);
    EXPECT_EQ(introspection_plasticity_update_bcm(bridge, 10.0f), 0);
}

TEST_F(IntrospectionPlasticityBridgeTest, UpdateBCMNull) {
    EXPECT_EQ(introspection_plasticity_update_bcm(nullptr, 10.0f), -1);
}

TEST_F(IntrospectionPlasticityBridgeTest, UpdateBCMZeroDt) {
    EXPECT_EQ(introspection_plasticity_update_bcm(bridge, 0.0f), -1);
}

TEST_F(IntrospectionPlasticityBridgeTest, HomeostaticUpdate) {
    introspection_plasticity_register_synapse(bridge, 1, INTROSPECTION_SYNAPSE_CONFIDENCE, 0.5f);
    EXPECT_EQ(introspection_plasticity_homeostatic_update(bridge, 10.0f), 0);
}

TEST_F(IntrospectionPlasticityBridgeTest, HomeostaticUpdateNull) {
    EXPECT_EQ(introspection_plasticity_homeostatic_update(nullptr, 10.0f), -1);
}

//=============================================================================
// Trace and Consolidation Tests
//=============================================================================

TEST_F(IntrospectionPlasticityBridgeTest, UpdateTraces) {
    introspection_plasticity_register_synapse(bridge, 1, INTROSPECTION_SYNAPSE_CONFIDENCE, 0.5f);
    EXPECT_EQ(introspection_plasticity_update_traces(bridge, 10.0f), 0);
}

TEST_F(IntrospectionPlasticityBridgeTest, UpdateTracesNull) {
    EXPECT_EQ(introspection_plasticity_update_traces(nullptr, 10.0f), -1);
}

TEST_F(IntrospectionPlasticityBridgeTest, Consolidate) {
    introspection_plasticity_register_synapse(bridge, 1, INTROSPECTION_SYNAPSE_CONFIDENCE, 0.5f);
    EXPECT_EQ(introspection_plasticity_consolidate(bridge), 0);
}

TEST_F(IntrospectionPlasticityBridgeTest, ConsolidateNull) {
    EXPECT_EQ(introspection_plasticity_consolidate(nullptr), -1);
}

//=============================================================================
// State Query Tests
//=============================================================================

TEST_F(IntrospectionPlasticityBridgeTest, GetCalibrationState) {
    introspection_calibration_state_t state;
    EXPECT_EQ(introspection_plasticity_get_calibration_state(bridge, &state), 0);
    EXPECT_GE(state.confidence_calibration, 0.0f);
    EXPECT_LE(state.confidence_calibration, 1.0f);
}

TEST_F(IntrospectionPlasticityBridgeTest, GetCalibrationStateNull) {
    EXPECT_EQ(introspection_plasticity_get_calibration_state(nullptr, nullptr), -1);
    introspection_calibration_state_t state;
    EXPECT_EQ(introspection_plasticity_get_calibration_state(nullptr, &state), -1);
    EXPECT_EQ(introspection_plasticity_get_calibration_state(bridge, nullptr), -1);
}

TEST_F(IntrospectionPlasticityBridgeTest, GetBridgeState) {
    introspection_plasticity_bridge_state_t state;
    EXPECT_EQ(introspection_plasticity_get_state(bridge, &state), 0);
    EXPECT_EQ(state.state, INTROSPECTION_PLASTICITY_STATE_IDLE);
    EXPECT_EQ(state.active_synapses, 0u);
}

TEST_F(IntrospectionPlasticityBridgeTest, GetStats) {
    introspection_plasticity_stats_t stats;
    EXPECT_EQ(introspection_plasticity_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.total_learning_events, 0u);
}

TEST_F(IntrospectionPlasticityBridgeTest, ResetStats) {
    introspection_plasticity_register_synapse(bridge, 1, INTROSPECTION_SYNAPSE_CONFIDENCE, 0.5f);
    introspection_plasticity_learn(bridge, INTROSPECTION_LEARN_CORRECT_CONFIDENCE, 0.5f, 1, 0.5f);

    introspection_plasticity_stats_t stats;
    introspection_plasticity_get_stats(bridge, &stats);
    EXPECT_GT(stats.total_learning_events, 0u);

    EXPECT_EQ(introspection_plasticity_reset_stats(bridge), 0);
    introspection_plasticity_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_learning_events, 0u);
}

//=============================================================================
// Callback Tests
//=============================================================================

TEST_F(IntrospectionPlasticityBridgeTest, RegisterLearnCallback) {
    EXPECT_EQ(introspection_plasticity_register_learn_callback(bridge, nullptr, nullptr), 0);
}

TEST_F(IntrospectionPlasticityBridgeTest, RegisterCalibrationCallback) {
    EXPECT_EQ(introspection_plasticity_register_calibration_callback(bridge, nullptr, nullptr), 0);
}

//=============================================================================
// Bio-Async Tests
//=============================================================================

TEST_F(IntrospectionPlasticityBridgeTest, BioAsyncNotEnabled) {
    EXPECT_EQ(introspection_plasticity_bio_async_connect(bridge), -1);
    EXPECT_FALSE(introspection_plasticity_is_bio_async_connected(bridge));
}

TEST_F(IntrospectionPlasticityBridgeTest, BioAsyncDisconnectSafe) {
    EXPECT_EQ(introspection_plasticity_bio_async_disconnect(bridge), 0);
}

//=============================================================================
// Integration Workflow Tests
//=============================================================================

TEST_F(IntrospectionPlasticityBridgeTest, FullLearningWorkflow) {
    // Register synapses
    for (int i = 0; i < 10; i++) {
        introspection_plasticity_register_synapse(bridge, i, INTROSPECTION_SYNAPSE_CONFIDENCE, 0.5f);
    }

    // Learning cycle
    for (int cycle = 0; cycle < 20; cycle++) {
        for (int i = 0; i < 10; i++) {
            introspection_plasticity_learn(bridge, INTROSPECTION_LEARN_CORRECT_CONFIDENCE, 0.3f, i, 0.8f);
            introspection_plasticity_apply_stdp(bridge, i, (float)cycle, (float)cycle + 5.0f);
        }
        introspection_plasticity_apply_reward(bridge, 0.5f);
        introspection_plasticity_update_traces(bridge, 1.0f);
    }

    introspection_plasticity_update_bcm(bridge, 10.0f);
    introspection_plasticity_homeostatic_update(bridge, 10.0f);
    introspection_plasticity_consolidate(bridge);

    introspection_plasticity_stats_t stats;
    introspection_plasticity_get_stats(bridge, &stats);
    EXPECT_GT(stats.total_learning_events, 100u);
    EXPECT_GT(stats.weight_updates, 100u);
}

TEST_F(IntrospectionPlasticityBridgeTest, CalibrationLearning) {
    // Use CONFIDENCE synapse (not auto-protected like CALIBRATION)
    introspection_plasticity_register_synapse(bridge, 1, INTROSPECTION_SYNAPSE_CONFIDENCE, 0.5f);

    for (int i = 0; i < 50; i++) {
        introspection_plasticity_learn(bridge, INTROSPECTION_LEARN_UNCERTAINTY_CALIBRATED, 0.2f, 1, 0.7f);
    }

    introspection_plasticity_synapse_t syn;
    introspection_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_NE(syn.weight, 0.5f);
}
