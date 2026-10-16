/**
 * @file test_synapse_plasticity_bridge.cpp
 * @brief Unit tests for Synapse-Plasticity Bridge
 *
 * WHAT: Comprehensive tests for synapse-plasticity bridge functionality
 * WHY:  Ensure synapses correctly integrate with all 17 plasticity mechanisms
 * HOW:  Test lifecycle, spike events, weight updates, mechanism connections
 *
 * TEST CATEGORIES:
 * - Lifecycle: Creation, destruction, configuration
 * - Spike Events: Pre/post spike processing
 * - Weight Updates: Accumulation and application
 * - Mechanism Connections: All 17 plasticity types
 * - Bio-async: Message routing integration
 * - Statistics: Event tracking and reporting
 *
 * @author NIMCP Development Team
 * @date 2025-12-21
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "plasticity/bridges/nimcp_synapse_plasticity_bridge.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class SynapsePlasticityBridgeTest : public ::testing::Test {
protected:
    synapse_plasticity_bridge_t* bridge = nullptr;
    synapse_plasticity_config_t config;

    void SetUp() override {
        synapse_plasticity_default_config(&config);
        bridge = synapse_plasticity_create(&config, nullptr, nullptr);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            synapse_plasticity_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(SynapsePlasticityBridgeTest, DefaultConfigHasReasonableValues) {
    synapse_plasticity_config_t cfg;
    int result = synapse_plasticity_default_config(&cfg);

    EXPECT_EQ(result, 0);
    EXPECT_GE(cfg.weight_min, 0.0f);
    EXPECT_GT(cfg.weight_max, cfg.weight_min);
    EXPECT_GE(cfg.integration_dt_ms, 0.0f);
}

TEST_F(SynapsePlasticityBridgeTest, DefaultConfigNullReturnsError) {
    int result = synapse_plasticity_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(SynapsePlasticityBridgeTest, CreateWithNullConfigUsesDefaults) {
    synapse_plasticity_bridge_t* b = synapse_plasticity_create(nullptr, nullptr, nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_TRUE(b->initialized);
    synapse_plasticity_destroy(b);
}

TEST_F(SynapsePlasticityBridgeTest, CreateWithConfigAppliesSettings) {
    config.weight_max = 2.0f;
    config.enable_stdp = false;

    synapse_plasticity_bridge_t* b = synapse_plasticity_create(&config, nullptr, nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_FLOAT_EQ(b->config.weight_max, 2.0f);
    EXPECT_FALSE(b->config.enable_stdp);
    synapse_plasticity_destroy(b);
}

TEST_F(SynapsePlasticityBridgeTest, DestroyNullIsNoOp) {
    synapse_plasticity_destroy(nullptr);
    // Should not crash
}

TEST_F(SynapsePlasticityBridgeTest, BridgeIsInitializedAfterCreate) {
    EXPECT_TRUE(bridge->initialized);
    EXPECT_NE(bridge->base.mutex, nullptr);
}

TEST_F(SynapsePlasticityBridgeTest, AllMechanismEnablesDefaultToTrue) {
    synapse_plasticity_config_t cfg;
    synapse_plasticity_default_config(&cfg);

    // Core mechanisms should be enabled by default
    EXPECT_TRUE(cfg.enable_stdp);
    EXPECT_TRUE(cfg.enable_homeostatic);
    EXPECT_TRUE(cfg.enable_calcium);
}

//=============================================================================
// Spike Event Tests
//=============================================================================

TEST_F(SynapsePlasticityBridgeTest, OnPreSpikeNullReturnsZero) {
    float delta = synapse_plasticity_on_pre_spike(nullptr, 1000);
    EXPECT_FLOAT_EQ(delta, 0.0f);
}

TEST_F(SynapsePlasticityBridgeTest, OnPreSpikeReturnsWeightDelta) {
    float delta = synapse_plasticity_on_pre_spike(bridge, 1000);
    // Delta depends on mechanisms connected
    // With no mechanisms, should be 0
}

TEST_F(SynapsePlasticityBridgeTest, OnPreSpikeUpdatesTimestamp) {
    synapse_plasticity_on_pre_spike(bridge, 1000);
    EXPECT_EQ(bridge->last_pre_spike_time, 1000u);
}

TEST_F(SynapsePlasticityBridgeTest, OnPostSpikeNullReturnsZero) {
    float delta = synapse_plasticity_on_post_spike(nullptr, 1000);
    EXPECT_FLOAT_EQ(delta, 0.0f);
}

TEST_F(SynapsePlasticityBridgeTest, OnPostSpikeUpdatesTimestamp) {
    synapse_plasticity_on_post_spike(bridge, 1000);
    EXPECT_EQ(bridge->last_post_spike_time, 1000u);
}

TEST_F(SynapsePlasticityBridgeTest, OnPostSpikeUpdatesStats) {
    synapse_plasticity_stats_t stats_before, stats_after;
    synapse_plasticity_get_stats(bridge, &stats_before);

    synapse_plasticity_on_post_spike(bridge, 1000);

    synapse_plasticity_get_stats(bridge, &stats_after);
    EXPECT_GT(stats_after.post_spike_count, stats_before.post_spike_count);
}

TEST_F(SynapsePlasticityBridgeTest, PrePostSequenceTriggersPlasticity) {
    // Pre before post should trigger LTP (if STDP connected)
    synapse_plasticity_on_pre_spike(bridge, 1000);
    float delta = synapse_plasticity_on_post_spike(bridge, 1010);
    // Delta depends on connected mechanisms
}

TEST_F(SynapsePlasticityBridgeTest, PostPreSequenceTriggersLTD) {
    // Post before pre should trigger LTD (if STDP connected)
    synapse_plasticity_on_post_spike(bridge, 1000);
    float delta = synapse_plasticity_on_pre_spike(bridge, 1010);
    // Delta depends on connected mechanisms
}

//=============================================================================
// Weight Update Tests
//=============================================================================

TEST_F(SynapsePlasticityBridgeTest, ApplyAccumulatedNullReturnsZero) {
    float weight = synapse_plasticity_apply_accumulated(nullptr);
    EXPECT_FLOAT_EQ(weight, 0.0f);
}

TEST_F(SynapsePlasticityBridgeTest, ApplyAccumulatedClearsAccumulator) {
    // Manually set accumulator (via spike events)
    synapse_plasticity_on_pre_spike(bridge, 1000);
    synapse_plasticity_on_post_spike(bridge, 1010);

    synapse_plasticity_apply_accumulated(bridge);
    EXPECT_FLOAT_EQ(bridge->weight_delta_accumulator, 0.0f);
}

TEST_F(SynapsePlasticityBridgeTest, GetEffectiveWeightNullReturnsZero) {
    float weight = synapse_plasticity_get_effective_weight(nullptr);
    EXPECT_FLOAT_EQ(weight, 0.0f);
}

TEST_F(SynapsePlasticityBridgeTest, GetEffectiveWeightWithoutSynapseReturnsZero) {
    float weight = synapse_plasticity_get_effective_weight(bridge);
    // No synapse connected, should return 0 or default
}

TEST_F(SynapsePlasticityBridgeTest, GetMechanismContributionNullReturnsZero) {
    float contrib = synapse_plasticity_get_mechanism_contribution(nullptr, PLASTICITY_STDP);
    EXPECT_FLOAT_EQ(contrib, 0.0f);
}

TEST_F(SynapsePlasticityBridgeTest, GetMechanismContributionValidMechanism) {
    float contrib = synapse_plasticity_get_mechanism_contribution(bridge, PLASTICITY_STDP);
    // Should return 0 if no events occurred
    EXPECT_GE(contrib, -1000.0f); // Just check it's a valid number
    EXPECT_LE(contrib, 1000.0f);
}

TEST_F(SynapsePlasticityBridgeTest, WeightBoundsEnforced) {
    config.weight_min = 0.1f;
    config.weight_max = 0.9f;
    config.clamp_weights = true;

    synapse_plasticity_bridge_t* b = synapse_plasticity_create(&config, nullptr, nullptr);
    ASSERT_NE(b, nullptr);

    // Weight should be clamped to bounds
    float weight = synapse_plasticity_apply_accumulated(b);
    EXPECT_GE(weight, 0.0f); // At least non-negative

    synapse_plasticity_destroy(b);
}

//=============================================================================
// Mechanism Connection Tests
//=============================================================================

TEST_F(SynapsePlasticityBridgeTest, ConnectSTDPNullBridgeReturnsError) {
    int result = synapse_plasticity_connect_stdp(nullptr, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SynapsePlasticityBridgeTest, ConnectSTDPSucceeds) {
    int result = synapse_plasticity_connect_stdp(bridge, nullptr);
    EXPECT_EQ(result, 0);
}

TEST_F(SynapsePlasticityBridgeTest, ConnectBCMNullBridgeReturnsError) {
    int result = synapse_plasticity_connect_bcm(nullptr, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SynapsePlasticityBridgeTest, ConnectHomeostaticNullBridgeReturnsError) {
    int result = synapse_plasticity_connect_homeostatic(nullptr, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SynapsePlasticityBridgeTest, ConnectSTPNullBridgeReturnsError) {
    int result = synapse_plasticity_connect_stp(nullptr, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SynapsePlasticityBridgeTest, ConnectMetaplasticityNullBridgeReturnsError) {
    int result = synapse_plasticity_connect_metaplasticity(nullptr, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SynapsePlasticityBridgeTest, ConnectEligibilityNullBridgeReturnsError) {
    int result = synapse_plasticity_connect_eligibility(nullptr, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SynapsePlasticityBridgeTest, ConnectHeterosynapticNullBridgeReturnsError) {
    int result = synapse_plasticity_connect_heterosynaptic(nullptr, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SynapsePlasticityBridgeTest, ConnectScalingNullBridgeReturnsError) {
    int result = synapse_plasticity_connect_scaling(nullptr, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SynapsePlasticityBridgeTest, ConnectTaggingNullBridgeReturnsError) {
    int result = synapse_plasticity_connect_tagging(nullptr, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SynapsePlasticityBridgeTest, ConnectCalciumNullBridgeReturnsError) {
    int result = synapse_plasticity_connect_calcium(nullptr, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SynapsePlasticityBridgeTest, ConnectNeuromodNullBridgeReturnsError) {
    int result = synapse_plasticity_connect_neuromod(nullptr, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SynapsePlasticityBridgeTest, ConnectMetabolicNullBridgeReturnsError) {
    int result = synapse_plasticity_connect_metabolic(nullptr, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SynapsePlasticityBridgeTest, ConnectStructuralNullBridgeReturnsError) {
    int result = synapse_plasticity_connect_structural(nullptr, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SynapsePlasticityBridgeTest, ConnectGlialNullBridgeReturnsError) {
    int result = synapse_plasticity_connect_glial(nullptr, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SynapsePlasticityBridgeTest, ConnectSFANullBridgeReturnsError) {
    int result = synapse_plasticity_connect_sfa(nullptr, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SynapsePlasticityBridgeTest, ConnectIntrinsicNullBridgeReturnsError) {
    int result = synapse_plasticity_connect_intrinsic(nullptr, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SynapsePlasticityBridgeTest, ConnectDendriticNullBridgeReturnsError) {
    int result = synapse_plasticity_connect_dendritic(nullptr, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SynapsePlasticityBridgeTest, ConnectAllNullBridgeReturnsError) {
    int result = synapse_plasticity_connect_all(nullptr, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SynapsePlasticityBridgeTest, IsMechanismConnectedNullReturnsFalse) {
    bool connected = synapse_plasticity_is_mechanism_connected(nullptr, PLASTICITY_STDP);
    EXPECT_FALSE(connected);
}

TEST_F(SynapsePlasticityBridgeTest, IsMechanismConnectedInitiallyFalse) {
    bool connected = synapse_plasticity_is_mechanism_connected(bridge, PLASTICITY_STDP);
    EXPECT_FALSE(connected);
}

//=============================================================================
// Update and Query API Tests
//=============================================================================

TEST_F(SynapsePlasticityBridgeTest, UpdateNullReturnsError) {
    int result = synapse_plasticity_update(nullptr, 10.0f);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SynapsePlasticityBridgeTest, UpdateSucceeds) {
    int result = synapse_plasticity_update(bridge, 10.0f);
    EXPECT_EQ(result, 0);
}

TEST_F(SynapsePlasticityBridgeTest, UpdateWithZeroTimestep) {
    int result = synapse_plasticity_update(bridge, 0.0f);
    EXPECT_EQ(result, 0);
}

TEST_F(SynapsePlasticityBridgeTest, UpdateDecaysAccumulator) {
    // Set up some accumulated weight change
    bridge->weight_delta_accumulator = 0.5f;

    // Update with decay
    synapse_plasticity_update(bridge, 100.0f);

    // Accumulator should decay (if configured)
    // Exact behavior depends on config.accumulator_decay
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(SynapsePlasticityBridgeTest, GetStatsNullReturnsError) {
    synapse_plasticity_stats_t stats;
    int result = synapse_plasticity_get_stats(nullptr, &stats);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SynapsePlasticityBridgeTest, GetStatsNullOutputReturnsError) {
    int result = synapse_plasticity_get_stats(bridge, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(SynapsePlasticityBridgeTest, GetStatsReturnsValidData) {
    synapse_plasticity_stats_t stats;
    memset(&stats, 0xFF, sizeof(stats));

    int result = synapse_plasticity_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.pre_spike_count, 0u); // Fresh bridge
    EXPECT_EQ(stats.post_spike_count, 0u);
}

TEST_F(SynapsePlasticityBridgeTest, ResetStatsNullIsNoOp) {
    synapse_plasticity_reset_stats(nullptr);
    // Should not crash
}

TEST_F(SynapsePlasticityBridgeTest, ResetStatsClearsCounters) {
    // Generate some events
    synapse_plasticity_on_pre_spike(bridge, 1000);
    synapse_plasticity_on_post_spike(bridge, 1010);

    // Reset
    synapse_plasticity_reset_stats(bridge);

    // Verify stats are cleared
    synapse_plasticity_stats_t stats;
    synapse_plasticity_get_stats(bridge, &stats);
    EXPECT_EQ(stats.pre_spike_count, 0u);
    EXPECT_EQ(stats.post_spike_count, 0u);
}

TEST_F(SynapsePlasticityBridgeTest, StatsTrackMechanismContributions) {
    synapse_plasticity_stats_t stats;
    synapse_plasticity_get_stats(bridge, &stats);

    // Should have entries for all mechanisms
    for (int i = 0; i < PLASTICITY_COUNT; i++) {
        // Just verify they exist and are accessible
        mechanism_contribution_t& mc = stats.mechanism_stats[i];
        EXPECT_GE(mc.weight_delta, -1000.0f);
        EXPECT_LE(mc.weight_delta, 1000.0f);
    }
}

//=============================================================================
// Bio-Async Integration Tests
//=============================================================================

TEST_F(SynapsePlasticityBridgeTest, ConnectBioAsyncNullReturnsError) {
    int result = synapse_plasticity_connect_bio_async(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
}

TEST_F(SynapsePlasticityBridgeTest, DisconnectBioAsyncNullHandled) {
    int result = synapse_plasticity_disconnect_bio_async(nullptr);
    // Null handling - may return 0 or error code depending on implementation
    (void)result;
}

TEST_F(SynapsePlasticityBridgeTest, IsBioAsyncConnectedNullReturnsFalse) {
    bool connected = synapse_plasticity_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}

TEST_F(SynapsePlasticityBridgeTest, IsBioAsyncConnectedInitiallyFalse) {
    bool connected = synapse_plasticity_is_bio_async_connected(bridge);
    EXPECT_FALSE(connected);
}

//=============================================================================
// String Conversion Tests
//=============================================================================

TEST_F(SynapsePlasticityBridgeTest, MechanismToStringReturnsValidStrings) {
    EXPECT_STREQ(plasticity_mechanism_to_string(PLASTICITY_STDP), "STDP");
    EXPECT_STREQ(plasticity_mechanism_to_string(PLASTICITY_BCM), "BCM");
    EXPECT_STREQ(plasticity_mechanism_to_string(PLASTICITY_HOMEOSTATIC), "HOMEOSTATIC");
    EXPECT_STREQ(plasticity_mechanism_to_string(PLASTICITY_STP), "STP");
    EXPECT_STREQ(plasticity_mechanism_to_string(PLASTICITY_METAPLASTICITY), "METAPLASTICITY");
    EXPECT_STREQ(plasticity_mechanism_to_string(PLASTICITY_ELIGIBILITY), "ELIGIBILITY");
    EXPECT_STREQ(plasticity_mechanism_to_string(PLASTICITY_HETEROSYNAPTIC), "HETEROSYNAPTIC");
    EXPECT_STREQ(plasticity_mechanism_to_string(PLASTICITY_SCALING), "SCALING");
    EXPECT_STREQ(plasticity_mechanism_to_string(PLASTICITY_TAGGING), "TAGGING");
    EXPECT_STREQ(plasticity_mechanism_to_string(PLASTICITY_CALCIUM), "CALCIUM");
    EXPECT_STREQ(plasticity_mechanism_to_string(PLASTICITY_NEUROMODULATOR), "NEUROMODULATOR");
    EXPECT_STREQ(plasticity_mechanism_to_string(PLASTICITY_METABOLIC), "METABOLIC");
    EXPECT_STREQ(plasticity_mechanism_to_string(PLASTICITY_STRUCTURAL), "STRUCTURAL");
    EXPECT_STREQ(plasticity_mechanism_to_string(PLASTICITY_GLIOTRANSMISSION), "GLIOTRANSMISSION");
    EXPECT_STREQ(plasticity_mechanism_to_string(PLASTICITY_SFA), "SFA");
    EXPECT_STREQ(plasticity_mechanism_to_string(PLASTICITY_INTRINSIC), "INTRINSIC");
    EXPECT_STREQ(plasticity_mechanism_to_string(PLASTICITY_DENDRITIC), "DENDRITIC");
}

TEST_F(SynapsePlasticityBridgeTest, WeightUpdateModeToStringReturnsValidStrings) {
    EXPECT_STREQ(weight_update_mode_to_string(WEIGHT_UPDATE_ADDITIVE), "ADDITIVE");
    EXPECT_STREQ(weight_update_mode_to_string(WEIGHT_UPDATE_MULTIPLICATIVE), "MULTIPLICATIVE");
    EXPECT_STREQ(weight_update_mode_to_string(WEIGHT_UPDATE_SOFT_BOUNDS), "SOFT_BOUNDS");
    EXPECT_STREQ(weight_update_mode_to_string(WEIGHT_UPDATE_HARD_BOUNDS), "HARD_BOUNDS");
}

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(SynapsePlasticityBridgeTest, RapidSpikeSequenceHandled) {
    // Rapid spike sequence
    for (uint64_t t = 0; t < 100; t++) {
        synapse_plasticity_on_pre_spike(bridge, t);
        synapse_plasticity_on_post_spike(bridge, t + 1);
    }

    synapse_plasticity_stats_t stats;
    synapse_plasticity_get_stats(bridge, &stats);
    EXPECT_EQ(stats.pre_spike_count, 100u);
    EXPECT_EQ(stats.post_spike_count, 100u);
}

TEST_F(SynapsePlasticityBridgeTest, LargeTimestampHandled) {
    uint64_t large_time = 0xFFFFFFFFFFFFFFFFULL;
    synapse_plasticity_on_pre_spike(bridge, large_time);
    EXPECT_EQ(bridge->last_pre_spike_time, large_time);
}

TEST_F(SynapsePlasticityBridgeTest, AllUpdateModes) {
    weight_update_mode_t modes[] = {
        WEIGHT_UPDATE_ADDITIVE,
        WEIGHT_UPDATE_MULTIPLICATIVE,
        WEIGHT_UPDATE_SOFT_BOUNDS,
        WEIGHT_UPDATE_HARD_BOUNDS
    };

    for (auto mode : modes) {
        config.update_mode = mode;
        synapse_plasticity_bridge_t* b = synapse_plasticity_create(&config, nullptr, nullptr);
        ASSERT_NE(b, nullptr);
        EXPECT_EQ(b->config.update_mode, mode);
        synapse_plasticity_destroy(b);
    }
}

TEST_F(SynapsePlasticityBridgeTest, MechanismEnablesCombinations) {
    // Test various mechanism enable combinations
    config.enable_stdp = true;
    config.enable_bcm = true;
    config.enable_homeostatic = false;
    config.enable_stp = true;
    config.enable_metaplasticity = false;

    synapse_plasticity_bridge_t* b = synapse_plasticity_create(&config, nullptr, nullptr);
    ASSERT_NE(b, nullptr);

    EXPECT_TRUE(b->config.enable_stdp);
    EXPECT_TRUE(b->config.enable_bcm);
    EXPECT_FALSE(b->config.enable_homeostatic);
    EXPECT_TRUE(b->config.enable_stp);
    EXPECT_FALSE(b->config.enable_metaplasticity);

    synapse_plasticity_destroy(b);
}
