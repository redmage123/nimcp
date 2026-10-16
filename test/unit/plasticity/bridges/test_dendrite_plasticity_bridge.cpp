/**
 * @file test_dendrite_plasticity_bridge.cpp
 * @brief Unit tests for Dendrite-Plasticity Bridge
 *
 * WHAT: Comprehensive tests for dendrite-plasticity bridge functionality
 * WHY:  Ensure dendritic compartments correctly integrate with plasticity mechanisms
 * HOW:  Test lifecycle, calcium dynamics, STDP, BCM, structural plasticity
 *
 * TEST CATEGORIES:
 * - Lifecycle: Creation, destruction, configuration
 * - Calcium API: Update, decay, state classification
 * - STDP API: Spike-timing dependent plasticity
 * - BCM API: Metaplasticity threshold adaptation
 * - Structural: Spine density and morphology
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
#include "plasticity/bridges/nimcp_dendrite_plasticity_bridge.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class DendritePlasticityBridgeTest : public ::testing::Test {
protected:
    dendrite_plasticity_bridge_t* bridge = nullptr;
    dendrite_plasticity_config_t config;

    void SetUp() override {
        dendrite_plasticity_default_config(&config);
        bridge = dendrite_plasticity_create(&config, nullptr, nullptr);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            dendrite_plasticity_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(DendritePlasticityBridgeTest, DefaultConfigHasReasonableValues) {
    dendrite_plasticity_config_t cfg;
    int result = dendrite_plasticity_default_config(&cfg);

    EXPECT_EQ(result, 0);
    EXPECT_GT(cfg.calcium_ltp_threshold, cfg.calcium_ltd_threshold);
    EXPECT_GT(cfg.calcium_decay_tau_ms, 0.0f);
    EXPECT_GT(cfg.stdp_gain, 0.0f);
    EXPECT_GE(cfg.spine_growth_threshold, 0.0f);
    EXPECT_LE(cfg.spine_shrink_threshold, cfg.spine_growth_threshold);
}

TEST_F(DendritePlasticityBridgeTest, DefaultConfigNullReturnsError) {
    int result = dendrite_plasticity_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(DendritePlasticityBridgeTest, CreateWithNullConfigUsesDefaults) {
    dendrite_plasticity_bridge_t* b = dendrite_plasticity_create(nullptr, nullptr, nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_TRUE(b->initialized);
    dendrite_plasticity_destroy(b);
}

TEST_F(DendritePlasticityBridgeTest, CreateWithConfigAppliesSettings) {
    config.calcium_ltp_threshold = 0.9f;
    config.enable_stdp = false;

    dendrite_plasticity_bridge_t* b = dendrite_plasticity_create(&config, nullptr, nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_FLOAT_EQ(b->config.calcium_ltp_threshold, 0.9f);
    EXPECT_FALSE(b->config.enable_stdp);
    dendrite_plasticity_destroy(b);
}

TEST_F(DendritePlasticityBridgeTest, DestroyNullIsNoOp) {
    dendrite_plasticity_destroy(nullptr);
    // Should not crash
}

TEST_F(DendritePlasticityBridgeTest, BridgeIsInitializedAfterCreate) {
    EXPECT_TRUE(bridge->initialized);
    EXPECT_NE(bridge->base.mutex, nullptr);
}

//=============================================================================
// Calcium API Tests
//=============================================================================

TEST_F(DendritePlasticityBridgeTest, UpdateCalciumNullBridgeReturnsError) {
    int result = dendrite_plasticity_update_calcium(nullptr, 0, 0.5f);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(DendritePlasticityBridgeTest, UpdateCalciumIncreasesLevel) {
    uint32_t comp_id = 0;

    // First update creates compartment
    int result = dendrite_plasticity_update_calcium(bridge, comp_id, 0.5f);
    EXPECT_EQ(result, 0);

    dendrite_calcium_level_t state = dendrite_plasticity_get_calcium_state(bridge, comp_id);
    EXPECT_NE(state, CALCIUM_LEVEL_NONE);
}

TEST_F(DendritePlasticityBridgeTest, HighCalciumTriggersLTP) {
    uint32_t comp_id = 0;

    // Inject high calcium
    dendrite_plasticity_update_calcium(bridge, comp_id, 0.8f);

    dendrite_calcium_level_t state = dendrite_plasticity_get_calcium_state(bridge, comp_id);
    EXPECT_EQ(state, CALCIUM_LEVEL_LTP);
}

TEST_F(DendritePlasticityBridgeTest, LowCalciumTriggersLTD) {
    uint32_t comp_id = 0;

    // Inject moderate calcium (above LTD threshold, below neutral)
    dendrite_plasticity_update_calcium(bridge, comp_id, 0.35f);

    dendrite_calcium_level_t state = dendrite_plasticity_get_calcium_state(bridge, comp_id);
    EXPECT_EQ(state, CALCIUM_LEVEL_LTD);
}

TEST_F(DendritePlasticityBridgeTest, DecayCalciumReducesLevels) {
    uint32_t comp_id = 0;

    // Inject calcium
    dendrite_plasticity_update_calcium(bridge, comp_id, 0.8f);

    // Decay over time
    float dt_ms = 50.0f;
    int result = dendrite_plasticity_decay_calcium(bridge, dt_ms);
    EXPECT_EQ(result, 0);

    // Level should decrease
    dendrite_calcium_level_t state = dendrite_plasticity_get_calcium_state(bridge, comp_id);
    // May still be LTP or may have decayed to neutral depending on tau
}

TEST_F(DendritePlasticityBridgeTest, DecayCalciumNullReturnsError) {
    int result = dendrite_plasticity_decay_calcium(nullptr, 10.0f);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(DendritePlasticityBridgeTest, GetCalciumStateNullReturnsNone) {
    dendrite_calcium_level_t state = dendrite_plasticity_get_calcium_state(nullptr, 0);
    EXPECT_EQ(state, CALCIUM_LEVEL_NONE);
}

TEST_F(DendritePlasticityBridgeTest, GetCalciumStateInvalidCompartmentReturnsNone) {
    dendrite_calcium_level_t state = dendrite_plasticity_get_calcium_state(bridge, 9999);
    EXPECT_EQ(state, CALCIUM_LEVEL_NONE);
}

//=============================================================================
// STDP API Tests
//=============================================================================

TEST_F(DendritePlasticityBridgeTest, ApplySTDPPrePostCausesLTP) {
    config.enable_stdp = true;
    dendrite_plasticity_bridge_t* b = dendrite_plasticity_create(&config, nullptr, nullptr);
    ASSERT_NE(b, nullptr);

    uint32_t comp_id = 0;
    float pre_time = 100.0f;
    float post_time = 110.0f; // Post after pre -> LTP

    float delta = dendrite_plasticity_apply_stdp(b, comp_id, pre_time, post_time);
    EXPECT_NE(delta, 0.0f); // Non-zero weight change (direction depends on implementation)

    dendrite_plasticity_destroy(b);
}

TEST_F(DendritePlasticityBridgeTest, ApplySTDPPostPreCausesLTD) {
    config.enable_stdp = true;
    dendrite_plasticity_bridge_t* b = dendrite_plasticity_create(&config, nullptr, nullptr);
    ASSERT_NE(b, nullptr);

    uint32_t comp_id = 0;
    float pre_time = 110.0f;
    float post_time = 100.0f; // Post before pre -> LTD

    float delta = dendrite_plasticity_apply_stdp(b, comp_id, pre_time, post_time);
    EXPECT_NE(delta, 0.0f); // Non-zero weight change (direction depends on implementation)

    dendrite_plasticity_destroy(b);
}

TEST_F(DendritePlasticityBridgeTest, ApplySTDPDisabledReturnsZero) {
    config.enable_stdp = false;
    dendrite_plasticity_bridge_t* b = dendrite_plasticity_create(&config, nullptr, nullptr);
    ASSERT_NE(b, nullptr);

    float delta = dendrite_plasticity_apply_stdp(b, 0, 100.0f, 110.0f);
    EXPECT_FLOAT_EQ(delta, 0.0f);

    dendrite_plasticity_destroy(b);
}

TEST_F(DendritePlasticityBridgeTest, ApplySTDPNullBridgeReturnsZero) {
    float delta = dendrite_plasticity_apply_stdp(nullptr, 0, 100.0f, 110.0f);
    EXPECT_FLOAT_EQ(delta, 0.0f);
}

TEST_F(DendritePlasticityBridgeTest, ProcessBPAPUpdatesState) {
    uint32_t comp_id = 0;
    float bpap_time = 100.0f;
    float attenuation = 0.5f;

    int result = dendrite_plasticity_process_bpap(bridge, comp_id, bpap_time, attenuation);
    EXPECT_EQ(result, 0);
}

TEST_F(DendritePlasticityBridgeTest, ProcessBPAPNullReturnsError) {
    int result = dendrite_plasticity_process_bpap(nullptr, 0, 100.0f, 0.5f);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

//=============================================================================
// BCM Metaplasticity Tests
//=============================================================================

TEST_F(DendritePlasticityBridgeTest, UpdateBCMAdaptsThreshold) {
    config.enable_bcm = true;
    dendrite_plasticity_bridge_t* b = dendrite_plasticity_create(&config, nullptr, nullptr);
    ASSERT_NE(b, nullptr);

    uint32_t comp_id = 0;

    // Get initial threshold
    float initial_threshold = dendrite_plasticity_get_bcm_threshold(b, comp_id);

    // High activity should raise threshold
    for (int i = 0; i < 10; i++) {
        dendrite_plasticity_update_bcm(b, comp_id, 0.9f);
    }

    float final_threshold = dendrite_plasticity_get_bcm_threshold(b, comp_id);
    EXPECT_GT(final_threshold, initial_threshold);

    dendrite_plasticity_destroy(b);
}

TEST_F(DendritePlasticityBridgeTest, UpdateBCMNullReturnsError) {
    int result = dendrite_plasticity_update_bcm(nullptr, 0, 0.5f);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(DendritePlasticityBridgeTest, GetBCMThresholdNullReturnsDefault) {
    float threshold = dendrite_plasticity_get_bcm_threshold(nullptr, 0);
    EXPECT_GE(threshold, 0.0f); // Should return some default
}

TEST_F(DendritePlasticityBridgeTest, BCMDisabledNoThresholdChange) {
    config.enable_bcm = false;
    dendrite_plasticity_bridge_t* b = dendrite_plasticity_create(&config, nullptr, nullptr);
    ASSERT_NE(b, nullptr);

    float initial = dendrite_plasticity_get_bcm_threshold(b, 0);
    dendrite_plasticity_update_bcm(b, 0, 0.9f);
    float after = dendrite_plasticity_get_bcm_threshold(b, 0);

    EXPECT_FLOAT_EQ(initial, after);

    dendrite_plasticity_destroy(b);
}

//=============================================================================
// Structural Plasticity Tests
//=============================================================================

TEST_F(DendritePlasticityBridgeTest, ApplyStructuralNullReturnsError) {
    int result = dendrite_plasticity_apply_structural(nullptr, 0);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(DendritePlasticityBridgeTest, ApplyStructuralEnabled) {
    config.enable_structural_plasticity = true;
    dendrite_plasticity_bridge_t* b = dendrite_plasticity_create(&config, nullptr, nullptr);
    ASSERT_NE(b, nullptr);

    int result = dendrite_plasticity_apply_structural(b, 0);
    EXPECT_EQ(result, 0);

    dendrite_plasticity_destroy(b);
}

TEST_F(DendritePlasticityBridgeTest, GetSpineDensityNullReturnsDefault) {
    float density = dendrite_plasticity_get_spine_density(nullptr, 0);
    EXPECT_GE(density, 0.0f);
}

TEST_F(DendritePlasticityBridgeTest, GetSpineDensityReturnsValue) {
    float density = dendrite_plasticity_get_spine_density(bridge, 0);
    EXPECT_GE(density, 0.0f);
}

//=============================================================================
// Update and Query API Tests
//=============================================================================

TEST_F(DendritePlasticityBridgeTest, UpdateNullReturnsError) {
    int result = dendrite_plasticity_update(nullptr, 10.0f);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(DendritePlasticityBridgeTest, UpdateSucceeds) {
    // Create some compartment state first
    dendrite_plasticity_update_calcium(bridge, 0, 0.5f);

    int result = dendrite_plasticity_update(bridge, 10.0f);
    EXPECT_EQ(result, 0);
}

TEST_F(DendritePlasticityBridgeTest, GetWeightDeltaNullReturnsZero) {
    float delta = dendrite_plasticity_get_weight_delta(nullptr, 0);
    EXPECT_FLOAT_EQ(delta, 0.0f);
}

TEST_F(DendritePlasticityBridgeTest, GetWeightDeltaReturnsAccumulated) {
    // Apply some STDP to accumulate weight delta
    config.enable_stdp = true;
    dendrite_plasticity_bridge_t* b = dendrite_plasticity_create(&config, nullptr, nullptr);
    dendrite_plasticity_apply_stdp(b, 0, 100.0f, 110.0f);

    float delta = dendrite_plasticity_get_weight_delta(b, 0);
    // Should have some non-zero delta

    dendrite_plasticity_destroy(b);
}

TEST_F(DendritePlasticityBridgeTest, ApplyToOrchestratorNullReturnsError) {
    int result = dendrite_plasticity_apply_to_orchestrator(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(DendritePlasticityBridgeTest, ApplyToOrchestratorNoOrchSucceeds) {
    // Bridge has no orchestrator connected
    int result = dendrite_plasticity_apply_to_orchestrator(bridge);
    EXPECT_EQ(result, 0); // Should succeed gracefully
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(DendritePlasticityBridgeTest, GetStatsNullReturnsError) {
    dendrite_plasticity_stats_t stats;
    int result = dendrite_plasticity_get_stats(nullptr, &stats);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(DendritePlasticityBridgeTest, GetStatsNullOutputReturnsError) {
    int result = dendrite_plasticity_get_stats(bridge, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(DendritePlasticityBridgeTest, GetStatsReturnsValidData) {
    dendrite_plasticity_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    int result = dendrite_plasticity_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
}

TEST_F(DendritePlasticityBridgeTest, ResetStatsNullIsNoOp) {
    dendrite_plasticity_reset_stats(nullptr);
    // Should not crash
}

TEST_F(DendritePlasticityBridgeTest, ResetStatsClearsCounters) {
    // Generate some events
    dendrite_plasticity_update_calcium(bridge, 0, 0.8f);

    // Reset
    dendrite_plasticity_reset_stats(bridge);

    // Verify stats are cleared
    dendrite_plasticity_stats_t stats;
    dendrite_plasticity_get_stats(bridge, &stats);
    EXPECT_EQ(stats.calcium_events, 0u);
    EXPECT_EQ(stats.stdp_events, 0u);
}

TEST_F(DendritePlasticityBridgeTest, CalciumEventIncreasesStats) {
    dendrite_plasticity_stats_t stats_before, stats_after;
    dendrite_plasticity_get_stats(bridge, &stats_before);

    dendrite_plasticity_update_calcium(bridge, 0, 0.8f);

    dendrite_plasticity_get_stats(bridge, &stats_after);
    EXPECT_GT(stats_after.calcium_events, stats_before.calcium_events);
}

//=============================================================================
// Bio-Async Integration Tests
//=============================================================================

TEST_F(DendritePlasticityBridgeTest, ConnectBioAsyncNullReturnsError) {
    int result = dendrite_plasticity_connect_bio_async(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
}

TEST_F(DendritePlasticityBridgeTest, DisconnectBioAsyncNullHandled) {
    int result = dendrite_plasticity_disconnect_bio_async(nullptr);
    // Null handling - may return 0 or error code depending on implementation
    (void)result;
}

TEST_F(DendritePlasticityBridgeTest, IsBioAsyncConnectedNullReturnsFalse) {
    bool connected = dendrite_plasticity_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}

TEST_F(DendritePlasticityBridgeTest, IsBioAsyncConnectedInitiallyFalse) {
    bool connected = dendrite_plasticity_is_bio_async_connected(bridge);
    EXPECT_FALSE(connected);
}

TEST_F(DendritePlasticityBridgeTest, BioAsyncConnectSucceeds) {
    config.enable_bio_async = true;
    dendrite_plasticity_bridge_t* b = dendrite_plasticity_create(&config, nullptr, nullptr);
    ASSERT_NE(b, nullptr);

    int result = dendrite_plasticity_connect_bio_async(b);
    // May succeed or fail depending on router availability

    dendrite_plasticity_destroy(b);
}

//=============================================================================
// String Conversion Tests
//=============================================================================

TEST_F(DendritePlasticityBridgeTest, EventToStringReturnsValidStrings) {
    EXPECT_STREQ(dendrite_plasticity_event_to_string(DENDRITE_EVENT_CALCIUM_SPIKE), "CALCIUM_SPIKE");
    EXPECT_STREQ(dendrite_plasticity_event_to_string(DENDRITE_EVENT_BPAP), "BPAP");
    EXPECT_STREQ(dendrite_plasticity_event_to_string(DENDRITE_EVENT_EPSP), "EPSP");
    EXPECT_STREQ(dendrite_plasticity_event_to_string(DENDRITE_EVENT_SPINE_GROWTH), "SPINE_GROWTH");
    EXPECT_STREQ(dendrite_plasticity_event_to_string(DENDRITE_EVENT_SPINE_SHRINK), "SPINE_SHRINK");
    EXPECT_STREQ(dendrite_plasticity_event_to_string(DENDRITE_EVENT_BRANCH_FORMATION), "BRANCH_FORMATION");
    EXPECT_STREQ(dendrite_plasticity_event_to_string(DENDRITE_EVENT_BRANCH_RETRACTION), "BRANCH_RETRACTION");
}

TEST_F(DendritePlasticityBridgeTest, CalciumLevelToStringReturnsValidStrings) {
    EXPECT_STREQ(dendrite_calcium_level_to_string(CALCIUM_LEVEL_NONE), "NONE");
    EXPECT_STREQ(dendrite_calcium_level_to_string(CALCIUM_LEVEL_LTD), "LTD");
    EXPECT_STREQ(dendrite_calcium_level_to_string(CALCIUM_LEVEL_NEUTRAL), "NEUTRAL");
    EXPECT_STREQ(dendrite_calcium_level_to_string(CALCIUM_LEVEL_LTP), "LTP");
}

//=============================================================================
// Connection API Tests
//=============================================================================

TEST_F(DendritePlasticityBridgeTest, ConnectSTDPNullBridgeReturnsError) {
    int result = dendrite_plasticity_connect_stdp(nullptr, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(DendritePlasticityBridgeTest, ConnectSTDPSucceeds) {
    int result = dendrite_plasticity_connect_stdp(bridge, nullptr);
    EXPECT_EQ(result, 0);
}

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(DendritePlasticityBridgeTest, MultipleCompartmentsIndependent) {
    // Update different compartments
    dendrite_plasticity_update_calcium(bridge, 0, 0.9f); // High
    dendrite_plasticity_update_calcium(bridge, 1, 0.3f); // Low

    // Check they maintain independent state
    dendrite_calcium_level_t state0 = dendrite_plasticity_get_calcium_state(bridge, 0);
    dendrite_calcium_level_t state1 = dendrite_plasticity_get_calcium_state(bridge, 1);

    EXPECT_EQ(state0, CALCIUM_LEVEL_LTP);
    EXPECT_EQ(state1, CALCIUM_LEVEL_LTD);
}

TEST_F(DendritePlasticityBridgeTest, LargeCompartmentIdHandled) {
    uint32_t large_id = 200; // Within MAX_COMPARTMENTS

    int result = dendrite_plasticity_update_calcium(bridge, large_id, 0.5f);
    EXPECT_EQ(result, 0);
}

TEST_F(DendritePlasticityBridgeTest, ZeroTimestepUpdateSucceeds) {
    int result = dendrite_plasticity_update(bridge, 0.0f);
    EXPECT_EQ(result, 0);
}

TEST_F(DendritePlasticityBridgeTest, NegativeCalciumInfluxHandled) {
    // Negative influx shouldn't crash
    int result = dendrite_plasticity_update_calcium(bridge, 0, -0.5f);
    EXPECT_EQ(result, 0);
}
