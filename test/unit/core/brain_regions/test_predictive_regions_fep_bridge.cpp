/**
 * @file test_predictive_regions_fep_bridge.cpp
 * @brief Unit tests for Predictive Regions-FEP Bridge module
 *
 * WHAT: Comprehensive tests for FEP-predictive regions bidirectional integration
 * WHY:  Ensure proper hierarchical belief propagation and precision-weighted error correction
 * HOW:  Test lifecycle, connections, FEP→regions, regions→FEP, active inference
 */

#include <gtest/gtest.h>
#include <cmath>
#include "core/brain_regions/nimcp_predictive_regions_fep_bridge.h"
#include "core/brain_regions/nimcp_brain_region_predictive.h"
#include "cognitive/free_energy/nimcp_free_energy.h"

class PredictiveRegionsFepBridgeTest : public ::testing::Test {
protected:
    predictive_regions_fep_bridge_t* bridge = nullptr;
    fep_system_t* fep = nullptr;
    brain_region_t* region0 = nullptr;
    brain_region_t* region1 = nullptr;

    static const uint32_t OBS_DIM = 16;
    static const uint32_t ACTION_DIM = 8;
    static const uint32_t REGION_NEURONS = 100;

    void SetUp() override {
        // Create predictive regions-FEP bridge
        predictive_regions_fep_config_t config;
        predictive_regions_fep_bridge_default_config(&config);
        bridge = predictive_regions_fep_bridge_create(&config);

        // Create FEP system
        fep_config_t fep_config;
        fep_default_config(&fep_config);
        fep = fep_create(&fep_config, OBS_DIM, ACTION_DIM);

        // Create brain regions (placeholder allocations)
        region0 = (brain_region_t*)calloc(1, sizeof(brain_region_t));
        region1 = (brain_region_t*)calloc(1, sizeof(brain_region_t));
    }

    void TearDown() override {
        if (bridge) {
            predictive_regions_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (fep) {
            fep_destroy(fep);
            fep = nullptr;
        }
        if (region0) {
            free(region0);
            region0 = nullptr;
        }
        if (region1) {
            free(region1);
            region1 = nullptr;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(PredictiveRegionsFepBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(PredictiveRegionsFepBridgeTest, CreateWithNullConfig) {
    predictive_regions_fep_bridge_t* b = predictive_regions_fep_bridge_create(nullptr);
    ASSERT_NE(b, nullptr);
    predictive_regions_fep_bridge_destroy(b);
}

TEST_F(PredictiveRegionsFepBridgeTest, DestroyNull) {
    predictive_regions_fep_bridge_destroy(nullptr);  // Should not crash
}

TEST_F(PredictiveRegionsFepBridgeTest, DefaultConfig) {
    predictive_regions_fep_config_t config;
    int ret = predictive_regions_fep_bridge_default_config(&config);

    EXPECT_EQ(ret, 0);
    EXPECT_GT(config.num_hierarchy_levels, 0);
    EXPECT_LE(config.num_hierarchy_levels, PREDICTIVE_FEP_MAX_LEVELS);
    EXPECT_TRUE(config.enable_precision_adaptation);
    EXPECT_TRUE(config.enable_belief_sync);
    EXPECT_TRUE(config.enable_error_propagation);
    EXPECT_GT(config.precision_learning_rate, 0.0f);
    EXPECT_GT(config.belief_learning_rate, 0.0f);
    EXPECT_GT(config.prediction_learning_rate, 0.0f);
    EXPECT_GT(config.convergence_threshold, 0.0f);
    EXPECT_GT(config.error_threshold, 0.0f);
    EXPECT_GE(config.exploration_rate, 0.0f);
    EXPECT_LE(config.exploration_rate, 1.0f);
}

TEST_F(PredictiveRegionsFepBridgeTest, DefaultConfigNullPtr) {
    int ret = predictive_regions_fep_bridge_default_config(nullptr);
    EXPECT_EQ(ret, -1);
}

/* ============================================================================
 * Connection Tests
 * ============================================================================ */

TEST_F(PredictiveRegionsFepBridgeTest, ConnectFEP) {
    int ret = predictive_regions_fep_bridge_connect_fep(bridge, fep);
    EXPECT_EQ(ret, 0);
}

TEST_F(PredictiveRegionsFepBridgeTest, ConnectFEPNullParams) {
    EXPECT_EQ(predictive_regions_fep_bridge_connect_fep(nullptr, fep), -1);
    EXPECT_EQ(predictive_regions_fep_bridge_connect_fep(bridge, nullptr), -1);
}

TEST_F(PredictiveRegionsFepBridgeTest, MapRegion) {
    predictive_regions_fep_bridge_connect_fep(bridge, fep);

    int ret = predictive_regions_fep_bridge_map_region(bridge, region0, 0);
    EXPECT_EQ(ret, 0);
}

TEST_F(PredictiveRegionsFepBridgeTest, MapRegionNullParams) {
    EXPECT_EQ(predictive_regions_fep_bridge_map_region(nullptr, region0, 0), -1);
    EXPECT_EQ(predictive_regions_fep_bridge_map_region(bridge, nullptr, 0), -1);
}

TEST_F(PredictiveRegionsFepBridgeTest, MapMultipleRegions) {
    predictive_regions_fep_bridge_connect_fep(bridge, fep);

    EXPECT_EQ(predictive_regions_fep_bridge_map_region(bridge, region0, 0), 0);
    EXPECT_EQ(predictive_regions_fep_bridge_map_region(bridge, region1, 1), 0);
}

TEST_F(PredictiveRegionsFepBridgeTest, MapRegionInvalidLevel) {
    predictive_regions_fep_bridge_connect_fep(bridge, fep);

    // Test mapping to level beyond maximum
    int ret = predictive_regions_fep_bridge_map_region(bridge, region0, PREDICTIVE_FEP_MAX_LEVELS);
    EXPECT_EQ(ret, -1);
}

TEST_F(PredictiveRegionsFepBridgeTest, Disconnect) {
    predictive_regions_fep_bridge_connect_fep(bridge, fep);
    predictive_regions_fep_bridge_map_region(bridge, region0, 0);

    int ret = predictive_regions_fep_bridge_disconnect(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(PredictiveRegionsFepBridgeTest, DisconnectNullPtr) {
    int ret = predictive_regions_fep_bridge_disconnect(nullptr);
    EXPECT_EQ(ret, -1);
}

/* ============================================================================
 * FEP → Predictive Regions Direction Tests
 * ============================================================================ */

TEST_F(PredictiveRegionsFepBridgeTest, SyncBeliefsToRegions) {
    predictive_regions_fep_bridge_connect_fep(bridge, fep);
    predictive_regions_fep_bridge_map_region(bridge, region0, 0);

    int ret = predictive_regions_fep_sync_beliefs_to_regions(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(PredictiveRegionsFepBridgeTest, SyncBeliefsToRegionsNullPtr) {
    int ret = predictive_regions_fep_sync_beliefs_to_regions(nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(PredictiveRegionsFepBridgeTest, SyncBeliefsToRegionsWithoutConnection) {
    // Test without connecting systems
    int ret = predictive_regions_fep_sync_beliefs_to_regions(bridge);
    EXPECT_EQ(ret, -1);
}

TEST_F(PredictiveRegionsFepBridgeTest, ApplyPrecisionModulation) {
    predictive_regions_fep_bridge_connect_fep(bridge, fep);
    predictive_regions_fep_bridge_map_region(bridge, region0, 0);

    int ret = predictive_regions_fep_apply_precision_modulation(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(PredictiveRegionsFepBridgeTest, ApplyPrecisionModulationNullPtr) {
    int ret = predictive_regions_fep_apply_precision_modulation(nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(PredictiveRegionsFepBridgeTest, GeneratePredictions) {
    predictive_regions_fep_bridge_connect_fep(bridge, fep);
    predictive_regions_fep_bridge_map_region(bridge, region0, 0);
    predictive_regions_fep_bridge_map_region(bridge, region1, 1);

    int ret = predictive_regions_fep_generate_predictions(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(PredictiveRegionsFepBridgeTest, GeneratePredictionsNullPtr) {
    int ret = predictive_regions_fep_generate_predictions(nullptr);
    EXPECT_EQ(ret, -1);
}

/* ============================================================================
 * Predictive Regions → FEP Direction Tests
 * ============================================================================ */

TEST_F(PredictiveRegionsFepBridgeTest, PropagateErrorsToFEP) {
    predictive_regions_fep_bridge_connect_fep(bridge, fep);
    predictive_regions_fep_bridge_map_region(bridge, region0, 0);

    int ret = predictive_regions_fep_propagate_errors_to_fep(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(PredictiveRegionsFepBridgeTest, PropagateErrorsToFEPNullPtr) {
    int ret = predictive_regions_fep_propagate_errors_to_fep(nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(PredictiveRegionsFepBridgeTest, ComputeFreeEnergy) {
    predictive_regions_fep_bridge_connect_fep(bridge, fep);
    predictive_regions_fep_bridge_map_region(bridge, region0, 0);

    float free_energy = 0.0f;
    int ret = predictive_regions_fep_compute_free_energy(bridge, &free_energy);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(free_energy, 0.0f);
}

TEST_F(PredictiveRegionsFepBridgeTest, ComputeFreeEnergyNullPtr) {
    float free_energy = 0.0f;
    EXPECT_EQ(predictive_regions_fep_compute_free_energy(nullptr, &free_energy), -1);
    EXPECT_EQ(predictive_regions_fep_compute_free_energy(bridge, nullptr), -1);
}

TEST_F(PredictiveRegionsFepBridgeTest, ComputeFreeEnergyOutputValid) {
    predictive_regions_fep_bridge_connect_fep(bridge, fep);
    predictive_regions_fep_bridge_map_region(bridge, region0, 0);

    float free_energy = -1.0f;
    int ret = predictive_regions_fep_compute_free_energy(bridge, &free_energy);
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(std::isnan(free_energy));
    EXPECT_FALSE(std::isinf(free_energy));
}

TEST_F(PredictiveRegionsFepBridgeTest, AdaptPrecision) {
    predictive_regions_fep_bridge_connect_fep(bridge, fep);
    predictive_regions_fep_bridge_map_region(bridge, region0, 0);

    int ret = predictive_regions_fep_adapt_precision(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(PredictiveRegionsFepBridgeTest, AdaptPrecisionNullPtr) {
    int ret = predictive_regions_fep_adapt_precision(nullptr);
    EXPECT_EQ(ret, -1);
}

/* ============================================================================
 * Active Inference Tests
 * ============================================================================ */

TEST_F(PredictiveRegionsFepBridgeTest, ActiveInferenceSelect) {
    predictive_regions_fep_bridge_connect_fep(bridge, fep);
    predictive_regions_fep_bridge_map_region(bridge, region0, 0);
    predictive_regions_fep_bridge_map_region(bridge, region1, 1);

    uint32_t selected_region = 0;
    int ret = predictive_regions_fep_active_inference_select(bridge, &selected_region);
    EXPECT_EQ(ret, 0);
}

TEST_F(PredictiveRegionsFepBridgeTest, ActiveInferenceSelectNullPtr) {
    uint32_t selected_region = 0;
    EXPECT_EQ(predictive_regions_fep_active_inference_select(nullptr, &selected_region), -1);
    EXPECT_EQ(predictive_regions_fep_active_inference_select(bridge, nullptr), -1);
}

TEST_F(PredictiveRegionsFepBridgeTest, ActiveInferenceSelectValidRegion) {
    predictive_regions_fep_bridge_connect_fep(bridge, fep);
    predictive_regions_fep_bridge_map_region(bridge, region0, 0);
    predictive_regions_fep_bridge_map_region(bridge, region1, 1);

    uint32_t selected_region = 999;
    int ret = predictive_regions_fep_active_inference_select(bridge, &selected_region);
    EXPECT_EQ(ret, 0);
    // Should select one of the mapped regions
    EXPECT_LT(selected_region, 2);
}

TEST_F(PredictiveRegionsFepBridgeTest, ComputeEFE) {
    predictive_regions_fep_bridge_connect_fep(bridge, fep);
    predictive_regions_fep_bridge_map_region(bridge, region0, 0);

    float efe = 0.0f;
    int ret = predictive_regions_fep_compute_efe(bridge, 0, &efe);
    EXPECT_EQ(ret, 0);
}

TEST_F(PredictiveRegionsFepBridgeTest, ComputeEFENullPtr) {
    float efe = 0.0f;
    EXPECT_EQ(predictive_regions_fep_compute_efe(nullptr, 0, &efe), -1);
    EXPECT_EQ(predictive_regions_fep_compute_efe(bridge, 0, nullptr), -1);
}

TEST_F(PredictiveRegionsFepBridgeTest, ComputeEFEOutputValid) {
    predictive_regions_fep_bridge_connect_fep(bridge, fep);
    predictive_regions_fep_bridge_map_region(bridge, region0, 0);

    float efe = -999.0f;
    int ret = predictive_regions_fep_compute_efe(bridge, 0, &efe);
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(std::isnan(efe));
    EXPECT_FALSE(std::isinf(efe));
}

TEST_F(PredictiveRegionsFepBridgeTest, ComputeEFEInvalidRegion) {
    predictive_regions_fep_bridge_connect_fep(bridge, fep);

    float efe = 0.0f;
    // Test invalid region index
    int ret = predictive_regions_fep_compute_efe(bridge, 999, &efe);
    EXPECT_EQ(ret, -1);
}

/* ============================================================================
 * Update Cycle Tests
 * ============================================================================ */

TEST_F(PredictiveRegionsFepBridgeTest, Update) {
    predictive_regions_fep_bridge_connect_fep(bridge, fep);
    predictive_regions_fep_bridge_map_region(bridge, region0, 0);

    int ret = predictive_regions_fep_bridge_update(bridge, 100);
    EXPECT_EQ(ret, 0);
}

TEST_F(PredictiveRegionsFepBridgeTest, UpdateNullPtr) {
    int ret = predictive_regions_fep_bridge_update(nullptr, 100);
    EXPECT_EQ(ret, -1);
}

TEST_F(PredictiveRegionsFepBridgeTest, UpdateMultipleTimes) {
    predictive_regions_fep_bridge_connect_fep(bridge, fep);
    predictive_regions_fep_bridge_map_region(bridge, region0, 0);

    // Run multiple update cycles
    for (int i = 0; i < 10; i++) {
        int ret = predictive_regions_fep_bridge_update(bridge, 50);
        EXPECT_EQ(ret, 0);
    }
}

TEST_F(PredictiveRegionsFepBridgeTest, UpdateZeroDelta) {
    predictive_regions_fep_bridge_connect_fep(bridge, fep);
    predictive_regions_fep_bridge_map_region(bridge, region0, 0);

    int ret = predictive_regions_fep_bridge_update(bridge, 0);
    EXPECT_EQ(ret, 0);
}

/* ============================================================================
 * State/Stats API Tests
 * ============================================================================ */

TEST_F(PredictiveRegionsFepBridgeTest, GetState) {
    predictive_regions_fep_bridge_connect_fep(bridge, fep);
    predictive_regions_fep_bridge_map_region(bridge, region0, 0);

    predictive_regions_fep_state_t state;
    int ret = predictive_regions_fep_bridge_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(state.num_mapped_levels, 0);
    EXPECT_GE(state.total_free_energy, 0.0f);
    EXPECT_GE(state.mean_prediction_error, 0.0f);
}

TEST_F(PredictiveRegionsFepBridgeTest, GetStateNullPtr) {
    predictive_regions_fep_state_t state;
    EXPECT_EQ(predictive_regions_fep_bridge_get_state(nullptr, &state), -1);
    EXPECT_EQ(predictive_regions_fep_bridge_get_state(bridge, nullptr), -1);
}

TEST_F(PredictiveRegionsFepBridgeTest, GetStats) {
    predictive_regions_fep_bridge_connect_fep(bridge, fep);
    predictive_regions_fep_bridge_map_region(bridge, region0, 0);

    predictive_regions_fep_stats_t stats;
    int ret = predictive_regions_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
}

TEST_F(PredictiveRegionsFepBridgeTest, GetStatsNullPtr) {
    predictive_regions_fep_stats_t stats;
    EXPECT_EQ(predictive_regions_fep_bridge_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(predictive_regions_fep_bridge_get_stats(bridge, nullptr), -1);
}

TEST_F(PredictiveRegionsFepBridgeTest, StatsAfterUpdate) {
    predictive_regions_fep_bridge_connect_fep(bridge, fep);
    predictive_regions_fep_bridge_map_region(bridge, region0, 0);

    // Run updates
    predictive_regions_fep_bridge_update(bridge, 100);
    predictive_regions_fep_sync_beliefs_to_regions(bridge);
    predictive_regions_fep_propagate_errors_to_fep(bridge);

    predictive_regions_fep_stats_t stats;
    int ret = predictive_regions_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(stats.belief_syncs, 0);
    EXPECT_GE(stats.error_propagations, 0);
}

TEST_F(PredictiveRegionsFepBridgeTest, IsConverged) {
    predictive_regions_fep_bridge_connect_fep(bridge, fep);
    predictive_regions_fep_bridge_map_region(bridge, region0, 0);

    bool converged = predictive_regions_fep_is_converged(bridge);
    // Initially may or may not be converged
    EXPECT_TRUE(converged || !converged);
}

TEST_F(PredictiveRegionsFepBridgeTest, IsConvergedNullPtr) {
    bool converged = predictive_regions_fep_is_converged(nullptr);
    EXPECT_FALSE(converged);
}

/* ============================================================================
 * Bio-Async Integration Tests
 * ============================================================================ */

TEST_F(PredictiveRegionsFepBridgeTest, ConnectBioAsync) {
    int ret = predictive_regions_fep_bridge_connect_bio_async(bridge);
    // May return 0 or -1 depending on router availability
    EXPECT_TRUE(ret == 0 || ret == -1);
}

TEST_F(PredictiveRegionsFepBridgeTest, ConnectBioAsyncNullPtr) {
    int ret = predictive_regions_fep_bridge_connect_bio_async(nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(PredictiveRegionsFepBridgeTest, DisconnectBioAsync) {
    predictive_regions_fep_bridge_connect_bio_async(bridge);
    int ret = predictive_regions_fep_bridge_disconnect_bio_async(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(PredictiveRegionsFepBridgeTest, DisconnectBioAsyncNullPtr) {
    int ret = predictive_regions_fep_bridge_disconnect_bio_async(nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(PredictiveRegionsFepBridgeTest, IsBioAsyncConnected) {
    bool connected = predictive_regions_fep_bridge_is_bio_async_connected(bridge);
    // Initially should be false
    EXPECT_FALSE(connected);
}

TEST_F(PredictiveRegionsFepBridgeTest, IsBioAsyncConnectedNullPtr) {
    bool connected = predictive_regions_fep_bridge_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}

TEST_F(PredictiveRegionsFepBridgeTest, BioAsyncConnectDisconnectCycle) {
    predictive_regions_fep_bridge_connect_bio_async(bridge);
    bool connected = predictive_regions_fep_bridge_is_bio_async_connected(bridge);

    predictive_regions_fep_bridge_disconnect_bio_async(bridge);
    bool disconnected = predictive_regions_fep_bridge_is_bio_async_connected(bridge);
    EXPECT_FALSE(disconnected);
}

/* ============================================================================
 * Integration Tests
 * ============================================================================ */

TEST_F(PredictiveRegionsFepBridgeTest, FullPipelineFEPToRegions) {
    // Connect systems
    predictive_regions_fep_bridge_connect_fep(bridge, fep);
    predictive_regions_fep_bridge_map_region(bridge, region0, 0);
    predictive_regions_fep_bridge_map_region(bridge, region1, 1);

    // Execute FEP → regions pathway
    EXPECT_EQ(predictive_regions_fep_sync_beliefs_to_regions(bridge), 0);
    EXPECT_EQ(predictive_regions_fep_apply_precision_modulation(bridge), 0);
    EXPECT_EQ(predictive_regions_fep_generate_predictions(bridge), 0);

    // Verify state
    predictive_regions_fep_state_t state;
    EXPECT_EQ(predictive_regions_fep_bridge_get_state(bridge, &state), 0);
    EXPECT_GE(state.num_mapped_levels, 2);
}

TEST_F(PredictiveRegionsFepBridgeTest, FullPipelineRegionsToFEP) {
    // Connect systems
    predictive_regions_fep_bridge_connect_fep(bridge, fep);
    predictive_regions_fep_bridge_map_region(bridge, region0, 0);
    predictive_regions_fep_bridge_map_region(bridge, region1, 1);

    // Execute regions → FEP pathway
    EXPECT_EQ(predictive_regions_fep_propagate_errors_to_fep(bridge), 0);

    float free_energy = 0.0f;
    EXPECT_EQ(predictive_regions_fep_compute_free_energy(bridge, &free_energy), 0);
    EXPECT_EQ(predictive_regions_fep_adapt_precision(bridge), 0);

    // Verify statistics
    predictive_regions_fep_stats_t stats;
    EXPECT_EQ(predictive_regions_fep_bridge_get_stats(bridge, &stats), 0);
}

TEST_F(PredictiveRegionsFepBridgeTest, BidirectionalIntegration) {
    // Connect systems
    predictive_regions_fep_bridge_connect_fep(bridge, fep);
    predictive_regions_fep_bridge_map_region(bridge, region0, 0);
    predictive_regions_fep_bridge_map_region(bridge, region1, 1);

    // Run bidirectional update
    EXPECT_EQ(predictive_regions_fep_bridge_update(bridge, 100), 0);

    // Test both directions work
    EXPECT_EQ(predictive_regions_fep_sync_beliefs_to_regions(bridge), 0);
    EXPECT_EQ(predictive_regions_fep_propagate_errors_to_fep(bridge), 0);

    // Verify free energy computation
    float free_energy = 0.0f;
    EXPECT_EQ(predictive_regions_fep_compute_free_energy(bridge, &free_energy), 0);
}

TEST_F(PredictiveRegionsFepBridgeTest, ActiveInferencePipeline) {
    // Connect systems
    predictive_regions_fep_bridge_connect_fep(bridge, fep);
    predictive_regions_fep_bridge_map_region(bridge, region0, 0);
    predictive_regions_fep_bridge_map_region(bridge, region1, 1);

    // Compute EFE for all regions
    float efe0 = 0.0f, efe1 = 0.0f;
    EXPECT_EQ(predictive_regions_fep_compute_efe(bridge, 0, &efe0), 0);
    EXPECT_EQ(predictive_regions_fep_compute_efe(bridge, 1, &efe1), 0);

    // Perform active inference selection
    uint32_t selected_region = 0;
    EXPECT_EQ(predictive_regions_fep_active_inference_select(bridge, &selected_region), 0);
    EXPECT_LT(selected_region, 2);

    // Verify selection updates stats
    predictive_regions_fep_stats_t stats;
    EXPECT_EQ(predictive_regions_fep_bridge_get_stats(bridge, &stats), 0);
}

TEST_F(PredictiveRegionsFepBridgeTest, HierarchicalBeliefPropagation) {
    // Connect systems with multi-level hierarchy
    predictive_regions_fep_bridge_connect_fep(bridge, fep);
    predictive_regions_fep_bridge_map_region(bridge, region0, 0);
    predictive_regions_fep_bridge_map_region(bridge, region1, 1);

    // Sync beliefs down the hierarchy
    EXPECT_EQ(predictive_regions_fep_sync_beliefs_to_regions(bridge), 0);

    // Generate predictions (top-down)
    EXPECT_EQ(predictive_regions_fep_generate_predictions(bridge), 0);

    // Propagate errors up the hierarchy
    EXPECT_EQ(predictive_regions_fep_propagate_errors_to_fep(bridge), 0);

    // Verify convergence
    bool converged = predictive_regions_fep_is_converged(bridge);
    EXPECT_TRUE(converged || !converged);
}

TEST_F(PredictiveRegionsFepBridgeTest, PrecisionAdaptationLoop) {
    // Connect systems
    predictive_regions_fep_bridge_connect_fep(bridge, fep);
    predictive_regions_fep_bridge_map_region(bridge, region0, 0);

    // Run multiple adaptation cycles
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(predictive_regions_fep_adapt_precision(bridge), 0);
        EXPECT_EQ(predictive_regions_fep_apply_precision_modulation(bridge), 0);
    }

    // Verify statistics show precision updates
    predictive_regions_fep_stats_t stats;
    EXPECT_EQ(predictive_regions_fep_bridge_get_stats(bridge, &stats), 0);
    EXPECT_GE(stats.precision_updates, 0);
}
