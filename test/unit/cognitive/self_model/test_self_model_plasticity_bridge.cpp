/**
 * @file test_self_model_plasticity_bridge.cpp
 * @brief Unit tests for Self Model Plasticity Bridge
 */

#include <gtest/gtest.h>
#include <cmath>

#include "cognitive/self_model/nimcp_self_model_plasticity_bridge.h"

class SelfModelPlasticityBridgeTest : public ::testing::Test {
protected:
    self_model_plasticity_bridge_t* bridge = nullptr;

    void SetUp() override {
        self_model_plasticity_config_t config = self_model_plasticity_config_default();
        config.enable_bio_async = false;
        bridge = self_model_plasticity_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            self_model_plasticity_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(SelfModelPlasticityBridgeTest, CreateWithDefaults) {
    self_model_plasticity_bridge_t* test_bridge = self_model_plasticity_create(nullptr);
    ASSERT_NE(test_bridge, nullptr);
    self_model_plasticity_destroy(test_bridge);
}

TEST_F(SelfModelPlasticityBridgeTest, CreateWithConfig) {
    self_model_plasticity_config_t config = self_model_plasticity_config_default();
    config.base_learning_rate = 0.005f;
    config.max_synapses = 128;
    self_model_plasticity_bridge_t* test_bridge = self_model_plasticity_create(&config);
    ASSERT_NE(test_bridge, nullptr);
    self_model_plasticity_destroy(test_bridge);
}

TEST_F(SelfModelPlasticityBridgeTest, Reset) {
    EXPECT_EQ(self_model_plasticity_reset(bridge), 0);
}

TEST_F(SelfModelPlasticityBridgeTest, ResetNull) {
    EXPECT_EQ(self_model_plasticity_reset(nullptr), -1);
}

TEST_F(SelfModelPlasticityBridgeTest, DestroyNull) {
    self_model_plasticity_destroy(nullptr); // Should not crash
}

//=============================================================================
// Default Config Tests
//=============================================================================

TEST_F(SelfModelPlasticityBridgeTest, DefaultConfigValues) {
    self_model_plasticity_config_t config = self_model_plasticity_config_default();
    EXPECT_EQ(config.base_learning_rate, SELF_MODEL_PLASTICITY_DEFAULT_LR);
    EXPECT_EQ(config.max_synapses, SELF_MODEL_PLASTICITY_MAX_SYNAPSES);
    EXPECT_GT(config.stdp_tau_plus_ms, 0.0f);
    EXPECT_GT(config.stdp_tau_minus_ms, 0.0f);
    EXPECT_TRUE(config.protect_identity);
    EXPECT_TRUE(config.protect_boundary);
}

//=============================================================================
// Synapse Management Tests
//=============================================================================

TEST_F(SelfModelPlasticityBridgeTest, RegisterSynapse) {
    EXPECT_EQ(self_model_plasticity_register_synapse(bridge, 1, SELF_SYNAPSE_AGENCY, 0.5f), 0);
}

TEST_F(SelfModelPlasticityBridgeTest, RegisterSynapseNull) {
    EXPECT_EQ(self_model_plasticity_register_synapse(nullptr, 1, SELF_SYNAPSE_AGENCY, 0.5f), -1);
}

TEST_F(SelfModelPlasticityBridgeTest, RegisterDuplicateSynapse) {
    EXPECT_EQ(self_model_plasticity_register_synapse(bridge, 1, SELF_SYNAPSE_AGENCY, 0.5f), 0);
    EXPECT_EQ(self_model_plasticity_register_synapse(bridge, 1, SELF_SYNAPSE_AGENCY, 0.5f), -1);
}

TEST_F(SelfModelPlasticityBridgeTest, UnregisterSynapse) {
    self_model_plasticity_register_synapse(bridge, 1, SELF_SYNAPSE_AGENCY, 0.5f);
    EXPECT_EQ(self_model_plasticity_unregister_synapse(bridge, 1), 0);
}

TEST_F(SelfModelPlasticityBridgeTest, UnregisterNonexistentSynapse) {
    EXPECT_EQ(self_model_plasticity_unregister_synapse(bridge, 999), -1);
}

TEST_F(SelfModelPlasticityBridgeTest, GetSynapse) {
    self_model_plasticity_register_synapse(bridge, 1, SELF_SYNAPSE_AGENCY, 0.5f);

    self_model_plasticity_synapse_t syn;
    EXPECT_EQ(self_model_plasticity_get_synapse(bridge, 1, &syn), 0);
    EXPECT_EQ(syn.synapse_id, 1u);
    EXPECT_EQ(syn.type, SELF_SYNAPSE_AGENCY);
    EXPECT_FLOAT_EQ(syn.weight, 0.5f);
}

TEST_F(SelfModelPlasticityBridgeTest, GetSynapseNonexistent) {
    self_model_plasticity_synapse_t syn;
    EXPECT_EQ(self_model_plasticity_get_synapse(bridge, 999, &syn), -1);
}

TEST_F(SelfModelPlasticityBridgeTest, ProtectSynapse) {
    self_model_plasticity_register_synapse(bridge, 1, SELF_SYNAPSE_AGENCY, 0.5f);
    EXPECT_EQ(self_model_plasticity_protect_synapse(bridge, 1, true), 0);

    self_model_plasticity_synapse_t syn;
    self_model_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_TRUE(syn.is_protected);
}

TEST_F(SelfModelPlasticityBridgeTest, AutoProtectIdentity) {
    self_model_plasticity_register_synapse(bridge, 1, SELF_SYNAPSE_IDENTITY, 0.5f);

    self_model_plasticity_synapse_t syn;
    self_model_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_TRUE(syn.is_protected);
}

TEST_F(SelfModelPlasticityBridgeTest, AutoProtectBoundary) {
    self_model_plasticity_register_synapse(bridge, 1, SELF_SYNAPSE_BOUNDARY, 0.5f);

    self_model_plasticity_synapse_t syn;
    self_model_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_TRUE(syn.is_protected);
}

//=============================================================================
// Learning Tests
//=============================================================================

TEST_F(SelfModelPlasticityBridgeTest, LearnAgencyConfirmed) {
    self_model_plasticity_register_synapse(bridge, 1, SELF_SYNAPSE_AGENCY, 0.5f);
    EXPECT_EQ(self_model_plasticity_learn(bridge, SELF_LEARN_AGENCY_CONFIRMED, 0.8f, 1, 0.9f), 0);

    self_model_plasticity_synapse_t syn;
    self_model_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_GT(syn.weight, 0.5f);
}

TEST_F(SelfModelPlasticityBridgeTest, LearnAgencyViolated) {
    self_model_plasticity_register_synapse(bridge, 1, SELF_SYNAPSE_AGENCY, 0.5f);
    EXPECT_EQ(self_model_plasticity_learn(bridge, SELF_LEARN_AGENCY_VIOLATED, 0.8f, 1, 0.9f), 0);

    self_model_plasticity_synapse_t syn;
    self_model_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_LT(syn.weight, 0.5f);
}

TEST_F(SelfModelPlasticityBridgeTest, LearnBoundaryClarified) {
    self_model_plasticity_register_synapse(bridge, 1, SELF_SYNAPSE_NARRATIVE, 0.5f);
    EXPECT_EQ(self_model_plasticity_learn(bridge, SELF_LEARN_BOUNDARY_CLARIFIED, 0.8f, 1, 0.9f), 0);

    self_model_plasticity_synapse_t syn;
    self_model_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_GT(syn.weight, 0.5f);
}

TEST_F(SelfModelPlasticityBridgeTest, LearnNull) {
    EXPECT_EQ(self_model_plasticity_learn(nullptr, SELF_LEARN_AGENCY_CONFIRMED, 0.5f, 1, 0.5f), -1);
}

TEST_F(SelfModelPlasticityBridgeTest, LearnProtectedSynapse) {
    self_model_plasticity_register_synapse(bridge, 1, SELF_SYNAPSE_IDENTITY, 0.5f);

    self_model_plasticity_synapse_t before;
    self_model_plasticity_get_synapse(bridge, 1, &before);

    self_model_plasticity_learn(bridge, SELF_LEARN_AGENCY_CONFIRMED, 1.0f, 1, 1.0f);

    self_model_plasticity_synapse_t after;
    self_model_plasticity_get_synapse(bridge, 1, &after);
    EXPECT_FLOAT_EQ(before.weight, after.weight);
}

//=============================================================================
// STDP Tests
//=============================================================================

TEST_F(SelfModelPlasticityBridgeTest, ApplySTDPPotentiation) {
    self_model_plasticity_register_synapse(bridge, 1, SELF_SYNAPSE_AGENCY, 0.5f);

    float delta = self_model_plasticity_apply_stdp(bridge, 1, 10.0f, 15.0f);
    EXPECT_TRUE(std::isfinite(delta));
    EXPECT_GT(delta, 0.0f);
}

TEST_F(SelfModelPlasticityBridgeTest, ApplySTDPDepression) {
    self_model_plasticity_register_synapse(bridge, 1, SELF_SYNAPSE_AGENCY, 0.5f);

    float delta = self_model_plasticity_apply_stdp(bridge, 1, 15.0f, 10.0f);
    EXPECT_TRUE(std::isfinite(delta));
    EXPECT_LT(delta, 0.0f);
}

TEST_F(SelfModelPlasticityBridgeTest, ApplySTDPNull) {
    float delta = self_model_plasticity_apply_stdp(nullptr, 1, 10.0f, 15.0f);
    EXPECT_TRUE(std::isnan(delta));
}

TEST_F(SelfModelPlasticityBridgeTest, ApplySTDPProtected) {
    self_model_plasticity_register_synapse(bridge, 1, SELF_SYNAPSE_IDENTITY, 0.5f);

    float delta = self_model_plasticity_apply_stdp(bridge, 1, 10.0f, 15.0f);
    EXPECT_FLOAT_EQ(delta, 0.0f);
}

//=============================================================================
// Reward Modulation Tests
//=============================================================================

TEST_F(SelfModelPlasticityBridgeTest, ApplyReward) {
    self_model_plasticity_register_synapse(bridge, 1, SELF_SYNAPSE_AGENCY, 0.5f);
    EXPECT_EQ(self_model_plasticity_apply_reward(bridge, 0.5f), 0);
}

TEST_F(SelfModelPlasticityBridgeTest, ApplyRewardNull) {
    EXPECT_EQ(self_model_plasticity_apply_reward(nullptr, 0.5f), -1);
}

TEST_F(SelfModelPlasticityBridgeTest, ApplyRewardClamped) {
    self_model_plasticity_register_synapse(bridge, 1, SELF_SYNAPSE_AGENCY, 0.5f);
    EXPECT_EQ(self_model_plasticity_apply_reward(bridge, 10.0f), 0); // Should clamp to 1.0
    EXPECT_EQ(self_model_plasticity_apply_reward(bridge, -10.0f), 0); // Should clamp to -1.0
}

//=============================================================================
// BCM and Homeostatic Tests
//=============================================================================

TEST_F(SelfModelPlasticityBridgeTest, UpdateBCM) {
    self_model_plasticity_register_synapse(bridge, 1, SELF_SYNAPSE_AGENCY, 0.5f);
    EXPECT_EQ(self_model_plasticity_update_bcm(bridge, 10.0f), 0);
}

TEST_F(SelfModelPlasticityBridgeTest, UpdateBCMNull) {
    EXPECT_EQ(self_model_plasticity_update_bcm(nullptr, 10.0f), -1);
}

TEST_F(SelfModelPlasticityBridgeTest, UpdateBCMZeroDt) {
    EXPECT_EQ(self_model_plasticity_update_bcm(bridge, 0.0f), -1);
}

TEST_F(SelfModelPlasticityBridgeTest, HomeostaticUpdate) {
    self_model_plasticity_register_synapse(bridge, 1, SELF_SYNAPSE_AGENCY, 0.5f);
    EXPECT_EQ(self_model_plasticity_homeostatic_update(bridge, 10.0f), 0);
}

TEST_F(SelfModelPlasticityBridgeTest, HomeostaticUpdateNull) {
    EXPECT_EQ(self_model_plasticity_homeostatic_update(nullptr, 10.0f), -1);
}

//=============================================================================
// Trace and Consolidation Tests
//=============================================================================

TEST_F(SelfModelPlasticityBridgeTest, UpdateTraces) {
    self_model_plasticity_register_synapse(bridge, 1, SELF_SYNAPSE_AGENCY, 0.5f);
    EXPECT_EQ(self_model_plasticity_update_traces(bridge, 10.0f), 0);
}

TEST_F(SelfModelPlasticityBridgeTest, UpdateTracesNull) {
    EXPECT_EQ(self_model_plasticity_update_traces(nullptr, 10.0f), -1);
}

TEST_F(SelfModelPlasticityBridgeTest, Consolidate) {
    self_model_plasticity_register_synapse(bridge, 1, SELF_SYNAPSE_AGENCY, 0.5f);
    EXPECT_EQ(self_model_plasticity_consolidate(bridge), 0);
}

TEST_F(SelfModelPlasticityBridgeTest, ConsolidateNull) {
    EXPECT_EQ(self_model_plasticity_consolidate(nullptr), -1);
}

//=============================================================================
// State Query Tests
//=============================================================================

TEST_F(SelfModelPlasticityBridgeTest, GetCalibrationState) {
    self_model_calibration_state_t state;
    EXPECT_EQ(self_model_plasticity_get_calibration_state(bridge, &state), 0);
    EXPECT_GE(state.boundary_calibration, 0.0f);
    EXPECT_LE(state.boundary_calibration, 1.0f);
}

TEST_F(SelfModelPlasticityBridgeTest, GetCalibrationStateNull) {
    EXPECT_EQ(self_model_plasticity_get_calibration_state(nullptr, nullptr), -1);
    self_model_calibration_state_t state;
    EXPECT_EQ(self_model_plasticity_get_calibration_state(nullptr, &state), -1);
    EXPECT_EQ(self_model_plasticity_get_calibration_state(bridge, nullptr), -1);
}

TEST_F(SelfModelPlasticityBridgeTest, GetBridgeState) {
    self_model_plasticity_bridge_state_t state;
    EXPECT_EQ(self_model_plasticity_get_state(bridge, &state), 0);
    EXPECT_EQ(state.state, SELF_MODEL_PLASTICITY_STATE_IDLE);
    EXPECT_EQ(state.active_synapses, 0u);
}

TEST_F(SelfModelPlasticityBridgeTest, GetStats) {
    self_model_plasticity_stats_t stats;
    EXPECT_EQ(self_model_plasticity_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.total_learning_events, 0u);
}

TEST_F(SelfModelPlasticityBridgeTest, ResetStats) {
    self_model_plasticity_register_synapse(bridge, 1, SELF_SYNAPSE_AGENCY, 0.5f);
    self_model_plasticity_learn(bridge, SELF_LEARN_AGENCY_CONFIRMED, 0.5f, 1, 0.5f);

    self_model_plasticity_stats_t stats;
    self_model_plasticity_get_stats(bridge, &stats);
    EXPECT_GT(stats.total_learning_events, 0u);

    EXPECT_EQ(self_model_plasticity_reset_stats(bridge), 0);
    self_model_plasticity_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_learning_events, 0u);
}

//=============================================================================
// Callback Tests
//=============================================================================

TEST_F(SelfModelPlasticityBridgeTest, RegisterLearnCallback) {
    EXPECT_EQ(self_model_plasticity_register_learn_callback(bridge, nullptr, nullptr), 0);
}

TEST_F(SelfModelPlasticityBridgeTest, RegisterCalibrationCallback) {
    EXPECT_EQ(self_model_plasticity_register_calibration_callback(bridge, nullptr, nullptr), 0);
}

//=============================================================================
// Bio-Async Tests
//=============================================================================

TEST_F(SelfModelPlasticityBridgeTest, BioAsyncNotEnabled) {
    EXPECT_EQ(self_model_plasticity_bio_async_connect(bridge), -1);
    EXPECT_FALSE(self_model_plasticity_is_bio_async_connected(bridge));
}

TEST_F(SelfModelPlasticityBridgeTest, BioAsyncDisconnectSafe) {
    EXPECT_EQ(self_model_plasticity_bio_async_disconnect(bridge), 0);
}

//=============================================================================
// Integration Workflow Tests
//=============================================================================

TEST_F(SelfModelPlasticityBridgeTest, FullLearningWorkflow) {
    // Register synapses (avoid protected types)
    for (int i = 0; i < 10; i++) {
        self_model_plasticity_register_synapse(bridge, i, SELF_SYNAPSE_AGENCY, 0.5f);
    }

    // Learning cycle
    for (int cycle = 0; cycle < 20; cycle++) {
        for (int i = 0; i < 10; i++) {
            self_model_plasticity_learn(bridge, SELF_LEARN_AGENCY_CONFIRMED, 0.3f, i, 0.8f);
            self_model_plasticity_apply_stdp(bridge, i, (float)cycle, (float)cycle + 5.0f);
        }
        self_model_plasticity_apply_reward(bridge, 0.5f);
        self_model_plasticity_update_traces(bridge, 1.0f);
    }

    self_model_plasticity_update_bcm(bridge, 10.0f);
    self_model_plasticity_homeostatic_update(bridge, 10.0f);
    self_model_plasticity_consolidate(bridge);

    self_model_plasticity_stats_t stats;
    self_model_plasticity_get_stats(bridge, &stats);
    EXPECT_GT(stats.total_learning_events, 100u);
    EXPECT_GT(stats.weight_updates, 100u);
}

TEST_F(SelfModelPlasticityBridgeTest, AgencyLearning) {
    // Use AGENCY synapse (not auto-protected)
    self_model_plasticity_register_synapse(bridge, 1, SELF_SYNAPSE_AGENCY, 0.5f);

    for (int i = 0; i < 50; i++) {
        self_model_plasticity_learn(bridge, SELF_LEARN_AGENCY_CONFIRMED, 0.2f, 1, 0.7f);
    }

    self_model_plasticity_synapse_t syn;
    self_model_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_NE(syn.weight, 0.5f);
}

TEST_F(SelfModelPlasticityBridgeTest, ProtectedIdentityPreserved) {
    // Register identity synapse (auto-protected)
    self_model_plasticity_register_synapse(bridge, 1, SELF_SYNAPSE_IDENTITY, 0.5f);

    // Try to modify it with many learning events
    for (int i = 0; i < 100; i++) {
        self_model_plasticity_learn(bridge, SELF_LEARN_IDENTITY_REINFORCED, 1.0f, 1, 1.0f);
        self_model_plasticity_apply_stdp(bridge, 1, 0.0f, 10.0f);
    }

    self_model_plasticity_synapse_t syn;
    self_model_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_FLOAT_EQ(syn.weight, 0.5f); // Should be unchanged
}

TEST_F(SelfModelPlasticityBridgeTest, ProtectedBoundaryPreserved) {
    // Register boundary synapse (auto-protected)
    self_model_plasticity_register_synapse(bridge, 1, SELF_SYNAPSE_BOUNDARY, 0.5f);

    // Try to modify it
    for (int i = 0; i < 100; i++) {
        self_model_plasticity_learn(bridge, SELF_LEARN_BOUNDARY_CLARIFIED, 1.0f, 1, 1.0f);
    }

    self_model_plasticity_synapse_t syn;
    self_model_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_FLOAT_EQ(syn.weight, 0.5f); // Should be unchanged
}
