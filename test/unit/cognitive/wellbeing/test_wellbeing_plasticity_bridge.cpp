/**
 * @file test_wellbeing_plasticity_bridge.cpp
 * @brief Unit tests for Wellbeing Plasticity Bridge
 */

#include <gtest/gtest.h>
#include <cmath>

#include "cognitive/wellbeing/nimcp_wellbeing_plasticity_bridge.h"

class WellbeingPlasticityBridgeTest : public ::testing::Test {
protected:
    wellbeing_plasticity_bridge_t* bridge = nullptr;

    void SetUp() override {
        wellbeing_plasticity_config_t config = wellbeing_plasticity_config_default();
        config.enable_bio_async = false;
        bridge = wellbeing_plasticity_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            wellbeing_plasticity_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(WellbeingPlasticityBridgeTest, CreateWithDefaults) {
    wellbeing_plasticity_bridge_t* test_bridge = wellbeing_plasticity_create(nullptr);
    ASSERT_NE(test_bridge, nullptr);
    wellbeing_plasticity_destroy(test_bridge);
}

TEST_F(WellbeingPlasticityBridgeTest, CreateWithConfig) {
    wellbeing_plasticity_config_t config = wellbeing_plasticity_config_default();
    config.base_learning_rate = 0.005f;
    config.max_synapses = 128;
    wellbeing_plasticity_bridge_t* test_bridge = wellbeing_plasticity_create(&config);
    ASSERT_NE(test_bridge, nullptr);
    wellbeing_plasticity_destroy(test_bridge);
}

TEST_F(WellbeingPlasticityBridgeTest, Reset) {
    EXPECT_EQ(wellbeing_plasticity_reset(bridge), 0);
}

TEST_F(WellbeingPlasticityBridgeTest, ResetNull) {
    EXPECT_EQ(wellbeing_plasticity_reset(nullptr), -1);
}

TEST_F(WellbeingPlasticityBridgeTest, DestroyNull) {
    wellbeing_plasticity_destroy(nullptr); // Should not crash
}

//=============================================================================
// Default Config Tests
//=============================================================================

TEST_F(WellbeingPlasticityBridgeTest, DefaultConfigValues) {
    wellbeing_plasticity_config_t config = wellbeing_plasticity_config_default();
    EXPECT_EQ(config.base_learning_rate, WELLBEING_PLASTICITY_DEFAULT_LR);
    EXPECT_EQ(config.max_synapses, WELLBEING_PLASTICITY_MAX_SYNAPSES);
    EXPECT_GT(config.stdp_tau_plus_ms, 0.0f);
    EXPECT_GT(config.stdp_tau_minus_ms, 0.0f);
    EXPECT_TRUE(config.protect_resilience);
}

//=============================================================================
// Synapse Management Tests
//=============================================================================

TEST_F(WellbeingPlasticityBridgeTest, RegisterSynapse) {
    EXPECT_EQ(wellbeing_plasticity_register_synapse(bridge, 1, WELLBEING_SYNAPSE_HEDONIC, 0.5f), 0);
}

TEST_F(WellbeingPlasticityBridgeTest, RegisterSynapseNull) {
    EXPECT_EQ(wellbeing_plasticity_register_synapse(nullptr, 1, WELLBEING_SYNAPSE_HEDONIC, 0.5f), -1);
}

TEST_F(WellbeingPlasticityBridgeTest, RegisterDuplicateSynapse) {
    EXPECT_EQ(wellbeing_plasticity_register_synapse(bridge, 1, WELLBEING_SYNAPSE_HEDONIC, 0.5f), 0);
    EXPECT_EQ(wellbeing_plasticity_register_synapse(bridge, 1, WELLBEING_SYNAPSE_HEDONIC, 0.5f), -1);
}

TEST_F(WellbeingPlasticityBridgeTest, UnregisterSynapse) {
    wellbeing_plasticity_register_synapse(bridge, 1, WELLBEING_SYNAPSE_HEDONIC, 0.5f);
    EXPECT_EQ(wellbeing_plasticity_unregister_synapse(bridge, 1), 0);
}

TEST_F(WellbeingPlasticityBridgeTest, UnregisterNonexistentSynapse) {
    EXPECT_EQ(wellbeing_plasticity_unregister_synapse(bridge, 999), -1);
}

TEST_F(WellbeingPlasticityBridgeTest, GetSynapse) {
    wellbeing_plasticity_register_synapse(bridge, 1, WELLBEING_SYNAPSE_HEDONIC, 0.5f);

    wellbeing_plasticity_synapse_t syn;
    EXPECT_EQ(wellbeing_plasticity_get_synapse(bridge, 1, &syn), 0);
    EXPECT_EQ(syn.synapse_id, 1u);
    EXPECT_EQ(syn.type, WELLBEING_SYNAPSE_HEDONIC);
    EXPECT_FLOAT_EQ(syn.weight, 0.5f);
}

TEST_F(WellbeingPlasticityBridgeTest, GetSynapseNonexistent) {
    wellbeing_plasticity_synapse_t syn;
    EXPECT_EQ(wellbeing_plasticity_get_synapse(bridge, 999, &syn), -1);
}

TEST_F(WellbeingPlasticityBridgeTest, ProtectSynapse) {
    wellbeing_plasticity_register_synapse(bridge, 1, WELLBEING_SYNAPSE_HEDONIC, 0.5f);
    EXPECT_EQ(wellbeing_plasticity_protect_synapse(bridge, 1, true), 0);

    wellbeing_plasticity_synapse_t syn;
    wellbeing_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_TRUE(syn.is_protected);
}

TEST_F(WellbeingPlasticityBridgeTest, AutoProtectResilience) {
    wellbeing_plasticity_register_synapse(bridge, 1, WELLBEING_SYNAPSE_RESILIENCE, 0.5f);

    wellbeing_plasticity_synapse_t syn;
    wellbeing_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_TRUE(syn.is_protected);
}

//=============================================================================
// Learning Tests
//=============================================================================

TEST_F(WellbeingPlasticityBridgeTest, LearnPositiveExperience) {
    wellbeing_plasticity_register_synapse(bridge, 1, WELLBEING_SYNAPSE_HEDONIC, 0.5f);
    EXPECT_EQ(wellbeing_plasticity_learn(bridge, WELLBEING_LEARN_POSITIVE_EXPERIENCE, 0.8f, 1, 0.9f), 0);

    wellbeing_plasticity_synapse_t syn;
    wellbeing_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_GT(syn.weight, 0.5f);
}

TEST_F(WellbeingPlasticityBridgeTest, LearnNegativeExperience) {
    wellbeing_plasticity_register_synapse(bridge, 1, WELLBEING_SYNAPSE_HEDONIC, 0.5f);
    EXPECT_EQ(wellbeing_plasticity_learn(bridge, WELLBEING_LEARN_NEGATIVE_EXPERIENCE, 0.8f, 1, 0.9f), 0);

    wellbeing_plasticity_synapse_t syn;
    wellbeing_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_LT(syn.weight, 0.5f);
}

TEST_F(WellbeingPlasticityBridgeTest, LearnNull) {
    EXPECT_EQ(wellbeing_plasticity_learn(nullptr, WELLBEING_LEARN_POSITIVE_EXPERIENCE, 0.5f, 1, 0.5f), -1);
}

TEST_F(WellbeingPlasticityBridgeTest, LearnProtectedSynapse) {
    wellbeing_plasticity_register_synapse(bridge, 1, WELLBEING_SYNAPSE_RESILIENCE, 0.5f);

    wellbeing_plasticity_synapse_t before;
    wellbeing_plasticity_get_synapse(bridge, 1, &before);

    wellbeing_plasticity_learn(bridge, WELLBEING_LEARN_POSITIVE_EXPERIENCE, 1.0f, 1, 1.0f);

    wellbeing_plasticity_synapse_t after;
    wellbeing_plasticity_get_synapse(bridge, 1, &after);
    EXPECT_FLOAT_EQ(before.weight, after.weight);
}

TEST_F(WellbeingPlasticityBridgeTest, LearnStressRecovery) {
    wellbeing_plasticity_register_synapse(bridge, 1, WELLBEING_SYNAPSE_HEDONIC, 0.5f);
    EXPECT_EQ(wellbeing_plasticity_learn(bridge, WELLBEING_LEARN_STRESS_RECOVERED, 0.8f, 1, 0.7f), 0);

    wellbeing_plasticity_synapse_t syn;
    wellbeing_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_GT(syn.weight, 0.5f);
}

TEST_F(WellbeingPlasticityBridgeTest, LearnSocialSupport) {
    wellbeing_plasticity_register_synapse(bridge, 1, WELLBEING_SYNAPSE_SOCIAL, 0.5f);
    EXPECT_EQ(wellbeing_plasticity_learn(bridge, WELLBEING_LEARN_SOCIAL_SUPPORT, 0.9f, 1, 0.8f), 0);

    wellbeing_plasticity_synapse_t syn;
    wellbeing_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_GT(syn.weight, 0.5f);
}

TEST_F(WellbeingPlasticityBridgeTest, LearnMeaningFound) {
    wellbeing_plasticity_register_synapse(bridge, 1, WELLBEING_SYNAPSE_EUDAIMONIC, 0.5f);
    EXPECT_EQ(wellbeing_plasticity_learn(bridge, WELLBEING_LEARN_MEANING_FOUND, 0.9f, 1, 0.85f), 0);

    wellbeing_plasticity_synapse_t syn;
    wellbeing_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_GT(syn.weight, 0.5f);
}

//=============================================================================
// STDP Tests
//=============================================================================

TEST_F(WellbeingPlasticityBridgeTest, ApplySTDPPotentiation) {
    wellbeing_plasticity_register_synapse(bridge, 1, WELLBEING_SYNAPSE_HEDONIC, 0.5f);

    float delta = wellbeing_plasticity_apply_stdp(bridge, 1, 10.0f, 15.0f);
    EXPECT_TRUE(std::isfinite(delta));
    EXPECT_GT(delta, 0.0f);
}

TEST_F(WellbeingPlasticityBridgeTest, ApplySTDPDepression) {
    wellbeing_plasticity_register_synapse(bridge, 1, WELLBEING_SYNAPSE_HEDONIC, 0.5f);

    float delta = wellbeing_plasticity_apply_stdp(bridge, 1, 15.0f, 10.0f);
    EXPECT_TRUE(std::isfinite(delta));
    EXPECT_LT(delta, 0.0f);
}

TEST_F(WellbeingPlasticityBridgeTest, ApplySTDPNull) {
    float delta = wellbeing_plasticity_apply_stdp(nullptr, 1, 10.0f, 15.0f);
    EXPECT_TRUE(std::isnan(delta));
}

TEST_F(WellbeingPlasticityBridgeTest, ApplySTDPProtected) {
    wellbeing_plasticity_register_synapse(bridge, 1, WELLBEING_SYNAPSE_RESILIENCE, 0.5f);

    float delta = wellbeing_plasticity_apply_stdp(bridge, 1, 10.0f, 15.0f);
    EXPECT_FLOAT_EQ(delta, 0.0f);
}

//=============================================================================
// Reward Modulation Tests
//=============================================================================

TEST_F(WellbeingPlasticityBridgeTest, ApplyReward) {
    wellbeing_plasticity_register_synapse(bridge, 1, WELLBEING_SYNAPSE_HEDONIC, 0.5f);
    EXPECT_EQ(wellbeing_plasticity_apply_reward(bridge, 0.5f), 0);
}

TEST_F(WellbeingPlasticityBridgeTest, ApplyRewardNull) {
    EXPECT_EQ(wellbeing_plasticity_apply_reward(nullptr, 0.5f), -1);
}

TEST_F(WellbeingPlasticityBridgeTest, ApplyRewardClamped) {
    wellbeing_plasticity_register_synapse(bridge, 1, WELLBEING_SYNAPSE_HEDONIC, 0.5f);
    EXPECT_EQ(wellbeing_plasticity_apply_reward(bridge, 10.0f), 0); // Should clamp to 1.0
    EXPECT_EQ(wellbeing_plasticity_apply_reward(bridge, -10.0f), 0); // Should clamp to -1.0
}

//=============================================================================
// BCM and Homeostatic Tests
//=============================================================================

TEST_F(WellbeingPlasticityBridgeTest, UpdateBCM) {
    wellbeing_plasticity_register_synapse(bridge, 1, WELLBEING_SYNAPSE_HEDONIC, 0.5f);
    EXPECT_EQ(wellbeing_plasticity_update_bcm(bridge, 10.0f), 0);
}

TEST_F(WellbeingPlasticityBridgeTest, UpdateBCMNull) {
    EXPECT_EQ(wellbeing_plasticity_update_bcm(nullptr, 10.0f), -1);
}

TEST_F(WellbeingPlasticityBridgeTest, UpdateBCMZeroDt) {
    EXPECT_EQ(wellbeing_plasticity_update_bcm(bridge, 0.0f), -1);
}

TEST_F(WellbeingPlasticityBridgeTest, HomeostaticUpdate) {
    wellbeing_plasticity_register_synapse(bridge, 1, WELLBEING_SYNAPSE_HEDONIC, 0.5f);
    EXPECT_EQ(wellbeing_plasticity_homeostatic_update(bridge, 10.0f), 0);
}

TEST_F(WellbeingPlasticityBridgeTest, HomeostaticUpdateNull) {
    EXPECT_EQ(wellbeing_plasticity_homeostatic_update(nullptr, 10.0f), -1);
}

//=============================================================================
// Trace and Consolidation Tests
//=============================================================================

TEST_F(WellbeingPlasticityBridgeTest, UpdateTraces) {
    wellbeing_plasticity_register_synapse(bridge, 1, WELLBEING_SYNAPSE_HEDONIC, 0.5f);
    EXPECT_EQ(wellbeing_plasticity_update_traces(bridge, 10.0f), 0);
}

TEST_F(WellbeingPlasticityBridgeTest, UpdateTracesNull) {
    EXPECT_EQ(wellbeing_plasticity_update_traces(nullptr, 10.0f), -1);
}

TEST_F(WellbeingPlasticityBridgeTest, Consolidate) {
    wellbeing_plasticity_register_synapse(bridge, 1, WELLBEING_SYNAPSE_HEDONIC, 0.5f);
    EXPECT_EQ(wellbeing_plasticity_consolidate(bridge), 0);
}

TEST_F(WellbeingPlasticityBridgeTest, ConsolidateNull) {
    EXPECT_EQ(wellbeing_plasticity_consolidate(nullptr), -1);
}

//=============================================================================
// Wellbeing Protection Tests
//=============================================================================

TEST_F(WellbeingPlasticityBridgeTest, ProtectResilience) {
    // Register some resilience synapses
    wellbeing_plasticity_register_synapse(bridge, 1, WELLBEING_SYNAPSE_RESILIENCE, 0.5f);
    wellbeing_plasticity_register_synapse(bridge, 2, WELLBEING_SYNAPSE_RESILIENCE, 0.6f);
    wellbeing_plasticity_register_synapse(bridge, 3, WELLBEING_SYNAPSE_HEDONIC, 0.5f);

    int protected_count = wellbeing_plasticity_protect_resilience(bridge);
    // Resilience synapses are auto-protected, so count may vary
    EXPECT_GE(protected_count, 0);
}

TEST_F(WellbeingPlasticityBridgeTest, ProtectFlourishing) {
    // Register high-weight synapse
    wellbeing_plasticity_register_synapse(bridge, 1, WELLBEING_SYNAPSE_HEDONIC, 0.9f);
    wellbeing_plasticity_register_synapse(bridge, 2, WELLBEING_SYNAPSE_EUDAIMONIC, 0.85f);

    int protected_count = wellbeing_plasticity_protect_flourishing(bridge);
    EXPECT_GE(protected_count, 0);
}

//=============================================================================
// State Query Tests
//=============================================================================

TEST_F(WellbeingPlasticityBridgeTest, GetFoundationState) {
    wellbeing_foundation_state_t state;
    EXPECT_EQ(wellbeing_plasticity_get_foundation_state(bridge, &state), 0);
    EXPECT_GE(state.hedonic_sensitivity, 0.0f);
    EXPECT_LE(state.hedonic_sensitivity, 1.0f);
    EXPECT_GE(state.resilience_level, 0.0f);
    EXPECT_LE(state.resilience_level, 1.0f);
}

TEST_F(WellbeingPlasticityBridgeTest, GetFoundationStateNull) {
    EXPECT_EQ(wellbeing_plasticity_get_foundation_state(nullptr, nullptr), -1);
    wellbeing_foundation_state_t state;
    EXPECT_EQ(wellbeing_plasticity_get_foundation_state(nullptr, &state), -1);
    EXPECT_EQ(wellbeing_plasticity_get_foundation_state(bridge, nullptr), -1);
}

TEST_F(WellbeingPlasticityBridgeTest, GetBridgeState) {
    wellbeing_plasticity_bridge_state_t state;
    EXPECT_EQ(wellbeing_plasticity_get_state(bridge, &state), 0);
    EXPECT_EQ(state.state, WELLBEING_PLASTICITY_STATE_IDLE);
    EXPECT_EQ(state.active_synapses, 0u);
}

TEST_F(WellbeingPlasticityBridgeTest, GetStats) {
    wellbeing_plasticity_stats_t stats;
    EXPECT_EQ(wellbeing_plasticity_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.total_learning_events, 0u);
}

TEST_F(WellbeingPlasticityBridgeTest, ResetStats) {
    wellbeing_plasticity_register_synapse(bridge, 1, WELLBEING_SYNAPSE_HEDONIC, 0.5f);
    wellbeing_plasticity_learn(bridge, WELLBEING_LEARN_POSITIVE_EXPERIENCE, 0.5f, 1, 0.5f);

    wellbeing_plasticity_stats_t stats;
    wellbeing_plasticity_get_stats(bridge, &stats);
    EXPECT_GT(stats.total_learning_events, 0u);

    EXPECT_EQ(wellbeing_plasticity_reset_stats(bridge), 0);
    wellbeing_plasticity_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_learning_events, 0u);
}

//=============================================================================
// Callback Tests
//=============================================================================

TEST_F(WellbeingPlasticityBridgeTest, RegisterLearnCallback) {
    EXPECT_EQ(wellbeing_plasticity_register_learn_callback(bridge, nullptr, nullptr), 0);
}

TEST_F(WellbeingPlasticityBridgeTest, RegisterFoundationCallback) {
    EXPECT_EQ(wellbeing_plasticity_register_foundation_callback(bridge, nullptr, nullptr), 0);
}

//=============================================================================
// Bio-Async Tests
//=============================================================================

TEST_F(WellbeingPlasticityBridgeTest, BioAsyncNotEnabled) {
    EXPECT_EQ(wellbeing_plasticity_bio_async_connect(bridge), -1);
    EXPECT_FALSE(wellbeing_plasticity_is_bio_async_connected(bridge));
}

TEST_F(WellbeingPlasticityBridgeTest, BioAsyncDisconnectSafe) {
    EXPECT_EQ(wellbeing_plasticity_bio_async_disconnect(bridge), 0);
}

//=============================================================================
// Integration Workflow Tests
//=============================================================================

TEST_F(WellbeingPlasticityBridgeTest, FullLearningWorkflow) {
    // Register synapses
    for (int i = 0; i < 10; i++) {
        wellbeing_plasticity_register_synapse(bridge, i, WELLBEING_SYNAPSE_HEDONIC, 0.5f);
    }

    // Learning cycle
    for (int cycle = 0; cycle < 20; cycle++) {
        for (int i = 0; i < 10; i++) {
            wellbeing_plasticity_learn(bridge, WELLBEING_LEARN_POSITIVE_EXPERIENCE, 0.3f, i, 0.8f);
            wellbeing_plasticity_apply_stdp(bridge, i, (float)cycle, (float)cycle + 5.0f);
        }
        wellbeing_plasticity_apply_reward(bridge, 0.5f);
        wellbeing_plasticity_update_traces(bridge, 1.0f);
    }

    wellbeing_plasticity_update_bcm(bridge, 10.0f);
    wellbeing_plasticity_homeostatic_update(bridge, 10.0f);
    wellbeing_plasticity_consolidate(bridge);

    wellbeing_plasticity_stats_t stats;
    wellbeing_plasticity_get_stats(bridge, &stats);
    EXPECT_GT(stats.total_learning_events, 100u);
    EXPECT_GT(stats.weight_updates, 100u);
}

TEST_F(WellbeingPlasticityBridgeTest, ResilienceLearning) {
    // Use HEDONIC synapse (not auto-protected like RESILIENCE)
    wellbeing_plasticity_register_synapse(bridge, 1, WELLBEING_SYNAPSE_HEDONIC, 0.5f);

    for (int i = 0; i < 50; i++) {
        wellbeing_plasticity_learn(bridge, WELLBEING_LEARN_STRESS_RECOVERED, 0.2f, 1, 0.7f);
    }

    wellbeing_plasticity_synapse_t syn;
    wellbeing_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_NE(syn.weight, 0.5f);
}

TEST_F(WellbeingPlasticityBridgeTest, FoundationEvolution) {
    wellbeing_plasticity_register_synapse(bridge, 1, WELLBEING_SYNAPSE_HEDONIC, 0.5f);
    wellbeing_plasticity_register_synapse(bridge, 2, WELLBEING_SYNAPSE_SOCIAL, 0.5f);
    wellbeing_plasticity_register_synapse(bridge, 3, WELLBEING_SYNAPSE_EUDAIMONIC, 0.5f);

    wellbeing_foundation_state_t initial;
    wellbeing_plasticity_get_foundation_state(bridge, &initial);

    // Apply various learning events
    for (int i = 0; i < 20; i++) {
        wellbeing_plasticity_learn(bridge, WELLBEING_LEARN_POSITIVE_EXPERIENCE, 0.5f, 1, 0.8f);
        wellbeing_plasticity_learn(bridge, WELLBEING_LEARN_SOCIAL_SUPPORT, 0.5f, 2, 0.7f);
        wellbeing_plasticity_learn(bridge, WELLBEING_LEARN_MEANING_FOUND, 0.5f, 3, 0.9f);
    }

    wellbeing_foundation_state_t final_state;
    wellbeing_plasticity_get_foundation_state(bridge, &final_state);

    // Foundation should have evolved
    EXPECT_GT(final_state.hedonic_sensitivity, initial.hedonic_sensitivity);
    EXPECT_GT(final_state.social_connection_strength, initial.social_connection_strength);
    EXPECT_GT(final_state.eudaimonic_strength, initial.eudaimonic_strength);
}
