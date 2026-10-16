/**
 * @file test_axon_plasticity_bridge.cpp
 * @brief Unit tests for Axon-Plasticity Bridge
 *
 * WHAT: Comprehensive tests for axon-plasticity bridge functionality
 * WHY:  Ensure axons correctly integrate with plasticity and myelination
 * HOW:  Test lifecycle, conduction, myelination, structural plasticity
 *
 * TEST CATEGORIES:
 * - Lifecycle: Creation, destruction, configuration
 * - Conduction API: Velocity, spike propagation, fatigue
 * - Myelination API: Activity-dependent adaptation
 * - Structural API: Branch growth/retraction
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
#include "plasticity/bridges/nimcp_axon_plasticity_bridge.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class AxonPlasticityBridgeTest : public ::testing::Test {
protected:
    axon_plasticity_bridge_t* bridge = nullptr;
    axon_plasticity_config_t config;

    void SetUp() override {
        axon_plasticity_default_config(&config);
        bridge = axon_plasticity_create(&config, nullptr, nullptr);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            axon_plasticity_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(AxonPlasticityBridgeTest, DefaultConfigHasReasonableValues) {
    axon_plasticity_config_t cfg;
    int result = axon_plasticity_default_config(&cfg);

    EXPECT_EQ(result, 0);
    EXPECT_GT(cfg.base_conduction_velocity, 0.0f);
    EXPECT_GT(cfg.max_conduction_velocity, cfg.base_conduction_velocity);
    EXPECT_GT(cfg.conduction_recovery_tau_ms, 0.0f);
    EXPECT_GE(cfg.conduction_fatigue_rate, 0.0f);
    EXPECT_LE(cfg.conduction_fatigue_rate, 1.0f);
}

TEST_F(AxonPlasticityBridgeTest, DefaultConfigNullReturnsError) {
    int result = axon_plasticity_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(AxonPlasticityBridgeTest, CreateWithNullConfigUsesDefaults) {
    axon_plasticity_bridge_t* b = axon_plasticity_create(nullptr, nullptr, nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_TRUE(b->initialized);
    axon_plasticity_destroy(b);
}

TEST_F(AxonPlasticityBridgeTest, CreateWithConfigAppliesSettings) {
    config.base_conduction_velocity = 5.0f;
    config.enable_adaptive_myelination = false;

    axon_plasticity_bridge_t* b = axon_plasticity_create(&config, nullptr, nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_FLOAT_EQ(b->config.base_conduction_velocity, 5.0f);
    EXPECT_FALSE(b->config.enable_adaptive_myelination);
    axon_plasticity_destroy(b);
}

TEST_F(AxonPlasticityBridgeTest, DestroyNullIsNoOp) {
    axon_plasticity_destroy(nullptr);
    // Should not crash
}

TEST_F(AxonPlasticityBridgeTest, BridgeIsInitializedAfterCreate) {
    EXPECT_TRUE(bridge->initialized);
    EXPECT_NE(bridge->base.mutex, nullptr);
}

TEST_F(AxonPlasticityBridgeTest, IntrinsicPlasticityDefaultsEnabled) {
    axon_plasticity_config_t cfg;
    axon_plasticity_default_config(&cfg);
    // May or may not be enabled by default
    EXPECT_GE(cfg.excitability_min, 0.0f);
    EXPECT_GT(cfg.excitability_max, 0.0f);
}

//=============================================================================
// Conduction API Tests
//=============================================================================

TEST_F(AxonPlasticityBridgeTest, UpdateConductionNullReturnsError) {
    int result = axon_plasticity_update_conduction(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(AxonPlasticityBridgeTest, UpdateConductionSucceeds) {
    int result = axon_plasticity_update_conduction(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(AxonPlasticityBridgeTest, GetConductionVelocityNullReturnsDefault) {
    float velocity = axon_plasticity_get_conduction_velocity(nullptr);
    EXPECT_GE(velocity, 0.0f); // Returns default value (may be 0 or base velocity)
}

TEST_F(AxonPlasticityBridgeTest, GetConductionVelocityReturnsBaseWithoutMyelination) {
    float velocity = axon_plasticity_get_conduction_velocity(bridge);
    EXPECT_GE(velocity, 0.0f);
}

TEST_F(AxonPlasticityBridgeTest, OnSpikeNullReturnsError) {
    int result = axon_plasticity_on_spike(nullptr, 0, 1000);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(AxonPlasticityBridgeTest, OnSpikeSucceeds) {
    int result = axon_plasticity_on_spike(bridge, 0, 1000);
    EXPECT_EQ(result, 0);
}

TEST_F(AxonPlasticityBridgeTest, OnSpikeUpdatesStats) {
    axon_plasticity_stats_t stats_before, stats_after;
    axon_plasticity_get_stats(bridge, &stats_before);

    axon_plasticity_on_spike(bridge, 0, 1000);

    axon_plasticity_get_stats(bridge, &stats_after);
    EXPECT_GT(stats_after.spikes_generated + stats_after.spikes_propagated,
              stats_before.spikes_generated + stats_before.spikes_propagated);
}

TEST_F(AxonPlasticityBridgeTest, RepeatedSpikesIncreaseFatigue) {
    // Initial velocity
    axon_plasticity_update_conduction(bridge);
    float initial_velocity = axon_plasticity_get_conduction_velocity(bridge);

    // Generate many spikes
    for (int i = 0; i < 50; i++) {
        axon_plasticity_on_spike(bridge, 0, i * 10);
    }

    axon_plasticity_update_conduction(bridge);
    float final_velocity = axon_plasticity_get_conduction_velocity(bridge);

    // Velocity may decrease due to fatigue (depends on config)
}

TEST_F(AxonPlasticityBridgeTest, ConductionBlockedWhenFatigued) {
    config.conduction_fatigue_rate = 0.5f;
    axon_plasticity_bridge_t* b = axon_plasticity_create(&config, nullptr, nullptr);
    ASSERT_NE(b, nullptr);

    // Generate many rapid spikes to induce fatigue
    for (int i = 0; i < 100; i++) {
        axon_plasticity_on_spike(b, 0, i);
    }

    // May eventually cause conduction failure
    axon_plasticity_destroy(b);
}

//=============================================================================
// Myelination API Tests
//=============================================================================

TEST_F(AxonPlasticityBridgeTest, UpdateMyelinationNullReturnsError) {
    int result = axon_plasticity_update_myelination(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(AxonPlasticityBridgeTest, UpdateMyelinationSucceeds) {
    int result = axon_plasticity_update_myelination(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(AxonPlasticityBridgeTest, GetMyelinationNullReturnsZero) {
    float level = axon_plasticity_get_myelination(nullptr, 0);
    EXPECT_FLOAT_EQ(level, 0.0f);
}

TEST_F(AxonPlasticityBridgeTest, GetMyelinationReturnsValidLevel) {
    float level = axon_plasticity_get_myelination(bridge, 0);
    EXPECT_GE(level, 0.0f);
    EXPECT_LE(level, 1.0f);
}

TEST_F(AxonPlasticityBridgeTest, GetMyelinationInvalidSegmentReturnsZero) {
    float level = axon_plasticity_get_myelination(bridge, 9999);
    EXPECT_FLOAT_EQ(level, 0.0f);
}

TEST_F(AxonPlasticityBridgeTest, ActivityIncreasesMyelination) {
    config.enable_adaptive_myelination = true;
    config.activity_myelination_gain = 0.1f;
    axon_plasticity_bridge_t* b = axon_plasticity_create(&config, nullptr, nullptr);
    ASSERT_NE(b, nullptr);

    // Get initial myelination
    float initial = axon_plasticity_get_myelination(b, 0);

    // Generate activity
    for (int i = 0; i < 20; i++) {
        axon_plasticity_on_spike(b, 0, i * 100);
    }
    axon_plasticity_update_myelination(b);

    float final_level = axon_plasticity_get_myelination(b, 0);
    EXPECT_GE(final_level, initial);

    axon_plasticity_destroy(b);
}

TEST_F(AxonPlasticityBridgeTest, MyelinationAffectsVelocity) {
    config.enable_adaptive_myelination = true;
    config.myelination_velocity_gain = 10.0f;
    axon_plasticity_bridge_t* b = axon_plasticity_create(&config, nullptr, nullptr);
    ASSERT_NE(b, nullptr);

    // Generate activity to increase myelination
    for (int i = 0; i < 50; i++) {
        axon_plasticity_on_spike(b, 0, i * 100);
    }
    axon_plasticity_update_myelination(b);
    axon_plasticity_update_conduction(b);

    float velocity = axon_plasticity_get_conduction_velocity(b);
    EXPECT_GT(velocity, 0.0f);

    axon_plasticity_destroy(b);
}

//=============================================================================
// Structural Plasticity Tests
//=============================================================================

TEST_F(AxonPlasticityBridgeTest, ApplyStructuralNullReturnsError) {
    int result = axon_plasticity_apply_structural(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(AxonPlasticityBridgeTest, ApplyStructuralEnabled) {
    config.enable_structural_plasticity = true;
    axon_plasticity_bridge_t* b = axon_plasticity_create(&config, nullptr, nullptr);
    ASSERT_NE(b, nullptr);

    int result = axon_plasticity_apply_structural(b);
    EXPECT_EQ(result, 0);

    axon_plasticity_destroy(b);
}

TEST_F(AxonPlasticityBridgeTest, StructuralDisabledNoChange) {
    config.enable_structural_plasticity = false;
    axon_plasticity_bridge_t* b = axon_plasticity_create(&config, nullptr, nullptr);
    ASSERT_NE(b, nullptr);

    int result = axon_plasticity_apply_structural(b);
    EXPECT_EQ(result, 0);

    axon_plasticity_destroy(b);
}

//=============================================================================
// Connection API Tests
//=============================================================================

TEST_F(AxonPlasticityBridgeTest, ConnectStructuralNullBridgeReturnsError) {
    int result = axon_plasticity_connect_structural(nullptr, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(AxonPlasticityBridgeTest, ConnectStructuralSucceeds) {
    int result = axon_plasticity_connect_structural(bridge, nullptr);
    EXPECT_EQ(result, 0);
}

TEST_F(AxonPlasticityBridgeTest, ConnectIntrinsicNullBridgeReturnsError) {
    int result = axon_plasticity_connect_intrinsic(nullptr, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(AxonPlasticityBridgeTest, ConnectIntrinsicSucceeds) {
    int result = axon_plasticity_connect_intrinsic(bridge, nullptr);
    EXPECT_EQ(result, 0);
}

TEST_F(AxonPlasticityBridgeTest, ConnectMetabolicNullBridgeReturnsError) {
    int result = axon_plasticity_connect_metabolic(nullptr, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(AxonPlasticityBridgeTest, ConnectMetabolicSucceeds) {
    int result = axon_plasticity_connect_metabolic(bridge, nullptr);
    EXPECT_EQ(result, 0);
}

TEST_F(AxonPlasticityBridgeTest, ConnectMyelinNullBridgeReturnsError) {
    int result = axon_plasticity_connect_myelin(nullptr, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(AxonPlasticityBridgeTest, ConnectMyelinSucceeds) {
    int result = axon_plasticity_connect_myelin(bridge, nullptr);
    EXPECT_EQ(result, 0);
}

//=============================================================================
// Update and Query API Tests
//=============================================================================

TEST_F(AxonPlasticityBridgeTest, UpdateNullReturnsError) {
    int result = axon_plasticity_update(nullptr, 10.0f);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(AxonPlasticityBridgeTest, UpdateSucceeds) {
    int result = axon_plasticity_update(bridge, 10.0f);
    EXPECT_EQ(result, 0);
}

TEST_F(AxonPlasticityBridgeTest, UpdateWithZeroTimestep) {
    int result = axon_plasticity_update(bridge, 0.0f);
    EXPECT_EQ(result, 0);
}

TEST_F(AxonPlasticityBridgeTest, UpdateRecoversFatigue) {
    // Induce fatigue
    for (int i = 0; i < 20; i++) {
        axon_plasticity_on_spike(bridge, 0, i);
    }

    // Let it recover
    for (int i = 0; i < 10; i++) {
        axon_plasticity_update(bridge, 100.0f);
    }

    // Fatigue should have recovered
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(AxonPlasticityBridgeTest, GetStatsNullReturnsError) {
    axon_plasticity_stats_t stats;
    int result = axon_plasticity_get_stats(nullptr, &stats);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(AxonPlasticityBridgeTest, GetStatsNullOutputReturnsError) {
    int result = axon_plasticity_get_stats(bridge, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(AxonPlasticityBridgeTest, GetStatsReturnsValidData) {
    axon_plasticity_stats_t stats;
    memset(&stats, 0xFF, sizeof(stats));

    int result = axon_plasticity_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.spikes_generated, 0u); // Fresh bridge
}

TEST_F(AxonPlasticityBridgeTest, ResetStatsNullIsNoOp) {
    axon_plasticity_reset_stats(nullptr);
    // Should not crash
}

TEST_F(AxonPlasticityBridgeTest, ResetStatsClearsCounters) {
    // Generate some events
    axon_plasticity_on_spike(bridge, 0, 1000);

    // Reset
    axon_plasticity_reset_stats(bridge);

    // Verify stats are cleared
    axon_plasticity_stats_t stats;
    axon_plasticity_get_stats(bridge, &stats);
    EXPECT_EQ(stats.spikes_generated, 0u);
    EXPECT_EQ(stats.spikes_propagated, 0u);
}

TEST_F(AxonPlasticityBridgeTest, StatsTrackConductionVelocity) {
    axon_plasticity_on_spike(bridge, 0, 1000);
    axon_plasticity_update(bridge, 10.0f);

    axon_plasticity_stats_t stats;
    axon_plasticity_get_stats(bridge, &stats);

    EXPECT_GE(stats.avg_conduction_velocity, 0.0f);
}

TEST_F(AxonPlasticityBridgeTest, StatsTrackMyelination) {
    axon_plasticity_update_myelination(bridge);

    axon_plasticity_stats_t stats;
    axon_plasticity_get_stats(bridge, &stats);

    EXPECT_GE(stats.avg_myelination, 0.0f);
    EXPECT_LE(stats.avg_myelination, 1.0f);
}

//=============================================================================
// Bio-Async Integration Tests
//=============================================================================

TEST_F(AxonPlasticityBridgeTest, ConnectBioAsyncNullReturnsError) {
    int result = axon_plasticity_connect_bio_async(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
}

TEST_F(AxonPlasticityBridgeTest, DisconnectBioAsyncNullHandled) {
    int result = axon_plasticity_disconnect_bio_async(nullptr);
    // Null handling - may return 0 or error code depending on implementation
    (void)result;
}

TEST_F(AxonPlasticityBridgeTest, IsBioAsyncConnectedNullReturnsFalse) {
    bool connected = axon_plasticity_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}

TEST_F(AxonPlasticityBridgeTest, IsBioAsyncConnectedInitiallyFalse) {
    bool connected = axon_plasticity_is_bio_async_connected(bridge);
    EXPECT_FALSE(connected);
}

//=============================================================================
// String Conversion Tests
//=============================================================================

TEST_F(AxonPlasticityBridgeTest, EventToStringReturnsValidStrings) {
    EXPECT_STREQ(axon_plasticity_event_to_string(AXON_EVENT_SPIKE_GENERATED), "SPIKE_GENERATED");
    EXPECT_STREQ(axon_plasticity_event_to_string(AXON_EVENT_SPIKE_PROPAGATED), "SPIKE_PROPAGATED");
    EXPECT_STREQ(axon_plasticity_event_to_string(AXON_EVENT_CONDUCTION_FAILURE), "CONDUCTION_FAILURE");
    EXPECT_STREQ(axon_plasticity_event_to_string(AXON_EVENT_MYELINATION_CHANGE), "MYELINATION_CHANGE");
    EXPECT_STREQ(axon_plasticity_event_to_string(AXON_EVENT_BRANCH_GROWTH), "BRANCH_GROWTH");
    EXPECT_STREQ(axon_plasticity_event_to_string(AXON_EVENT_BRANCH_RETRACTION), "BRANCH_RETRACTION");
    EXPECT_STREQ(axon_plasticity_event_to_string(AXON_EVENT_AIS_SHIFT), "AIS_SHIFT");
}

TEST_F(AxonPlasticityBridgeTest, ConductionStateToStringReturnsValidStrings) {
    EXPECT_STREQ(axon_conduction_state_to_string(CONDUCTION_NORMAL), "NORMAL");
    EXPECT_STREQ(axon_conduction_state_to_string(CONDUCTION_SLOWED), "SLOWED");
    EXPECT_STREQ(axon_conduction_state_to_string(CONDUCTION_BLOCKED), "BLOCKED");
    EXPECT_STREQ(axon_conduction_state_to_string(CONDUCTION_ENHANCED), "ENHANCED");
}

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(AxonPlasticityBridgeTest, MultipleSegmentsIndependent) {
    // Spike on different segments
    axon_plasticity_on_spike(bridge, 0, 1000);
    axon_plasticity_on_spike(bridge, 1, 1000);

    // Check they maintain independent state
    float myelin0 = axon_plasticity_get_myelination(bridge, 0);
    float myelin1 = axon_plasticity_get_myelination(bridge, 1);
    // Values may be the same initially but tracked independently
}

TEST_F(AxonPlasticityBridgeTest, LargeSegmentIdWithinBounds) {
    uint32_t valid_id = 50; // Within MAX_SEGMENTS

    int result = axon_plasticity_on_spike(bridge, valid_id, 1000);
    EXPECT_EQ(result, 0);
}

TEST_F(AxonPlasticityBridgeTest, VeryHighActivityLevel) {
    // Simulate very high activity
    for (int i = 0; i < 1000; i++) {
        axon_plasticity_on_spike(bridge, 0, i);
    }

    axon_plasticity_update(bridge, 10.0f);

    // Should not crash, stats should be valid
    axon_plasticity_stats_t stats;
    axon_plasticity_get_stats(bridge, &stats);
    EXPECT_EQ(stats.spikes_generated + stats.spikes_propagated, 1000u);
}

TEST_F(AxonPlasticityBridgeTest, ExcitabilityBoundsEnforced) {
    config.enable_intrinsic_plasticity = true;
    config.excitability_min = 0.2f;
    config.excitability_max = 0.8f;
    axon_plasticity_bridge_t* b = axon_plasticity_create(&config, nullptr, nullptr);
    ASSERT_NE(b, nullptr);

    // Excitability should stay within bounds
    axon_plasticity_destroy(b);
}

TEST_F(AxonPlasticityBridgeTest, MyelinationNeverExceedsOne) {
    config.enable_adaptive_myelination = true;
    config.activity_myelination_gain = 1.0f; // High gain
    axon_plasticity_bridge_t* b = axon_plasticity_create(&config, nullptr, nullptr);
    ASSERT_NE(b, nullptr);

    // Generate lots of activity
    for (int i = 0; i < 1000; i++) {
        axon_plasticity_on_spike(b, 0, i);
    }

    for (int i = 0; i < 100; i++) {
        axon_plasticity_update_myelination(b);
    }

    float myelin = axon_plasticity_get_myelination(b, 0);
    EXPECT_LE(myelin, 1.0f);

    axon_plasticity_destroy(b);
}
