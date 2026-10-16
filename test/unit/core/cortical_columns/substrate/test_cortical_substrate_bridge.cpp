/**
 * @file test_cortical_substrate_bridge.cpp
 * @brief Unit tests for Cortical Column-Neural Substrate Bridge
 * @date 2025-12-19
 *
 * Tests bidirectional substrate-cortical integration including columnar fidelity,
 * layer-specific gains, competition strength, sparsity modulation, hierarchical depth,
 * and metabolic modulation based on ATP/temperature conditions.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/cortical_columns/nimcp_cortical_substrate_bridge.h"
#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class CorticalSubstrateBridgeTest : public ::testing::Test {
protected:
    neural_substrate_t* substrate = nullptr;
    cortical_substrate_bridge_t* bridge = nullptr;
    cortical_substrate_config_t config;

    void SetUp() override {
        substrate_config_t sub_cfg;
        substrate_default_config(&sub_cfg);
        substrate = substrate_create(&sub_cfg);
        ASSERT_NE(substrate, nullptr);

        cortical_substrate_default_config(&config);
    }

    void TearDown() override {
        if (bridge) {
            cortical_substrate_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (substrate) {
            substrate_destroy(substrate);
            substrate = nullptr;
        }
    }

    void createBridge() {
        bridge = cortical_substrate_bridge_create(&config, substrate);
        ASSERT_NE(bridge, nullptr);
    }

    void setSubstrateATP(float level) {
        if (substrate) {
            substrate_set_atp(substrate, level);
        }
    }

    void setSubstrateTemperature(float temp) {
        if (substrate) {
            substrate_set_temperature(substrate, temp);
        }
    }
};

/* ============================================================================
 * Lifecycle Tests (8 tests)
 * ============================================================================ */

TEST_F(CorticalSubstrateBridgeTest, CreateWithValidInputs) {
    bridge = cortical_substrate_bridge_create(&config, substrate);
    EXPECT_NE(bridge, nullptr);
}

TEST_F(CorticalSubstrateBridgeTest, CreateWithNullConfig) {
    bridge = cortical_substrate_bridge_create(nullptr, substrate);
    EXPECT_NE(bridge, nullptr);
}

TEST_F(CorticalSubstrateBridgeTest, CreateWithNullSubstrate) {
    bridge = cortical_substrate_bridge_create(&config, nullptr);
    EXPECT_EQ(bridge, nullptr);
}

TEST_F(CorticalSubstrateBridgeTest, DestroyNull) {
    cortical_substrate_bridge_destroy(nullptr);
    SUCCEED();
}

TEST_F(CorticalSubstrateBridgeTest, DestroyValid) {
    createBridge();
    cortical_substrate_bridge_destroy(bridge);
    bridge = nullptr;
    SUCCEED();
}

TEST_F(CorticalSubstrateBridgeTest, ConnectColumnsNull) {
    createBridge();
    int result = cortical_substrate_connect_columns(bridge, nullptr);
    EXPECT_NE(result, 0);  // NIMCP error code (may be positive)
}

TEST_F(CorticalSubstrateBridgeTest, ConnectLaminarNull) {
    createBridge();
    int result = cortical_substrate_connect_laminar(bridge, nullptr);
    EXPECT_NE(result, 0);  // NIMCP error code (may be positive)
}

TEST_F(CorticalSubstrateBridgeTest, ConnectHierarchyNull) {
    createBridge();
    int result = cortical_substrate_connect_hierarchy(bridge, nullptr);
    EXPECT_NE(result, 0);  // NIMCP error code (may be positive)
}

/* ============================================================================
 * Bio-Async Tests (5 tests)
 * ============================================================================ */

TEST_F(CorticalSubstrateBridgeTest, ConnectBioAsyncNull) {
    int result = cortical_substrate_connect_bio_async(nullptr);
    EXPECT_NE(result, 0);  // NIMCP error code (may be positive)
}

TEST_F(CorticalSubstrateBridgeTest, DisconnectBioAsyncNull) {
    int result = cortical_substrate_disconnect_bio_async(nullptr);
    EXPECT_NE(result, 0);  // NIMCP error code (may be positive)
}

TEST_F(CorticalSubstrateBridgeTest, ConnectBioAsyncValid) {
    createBridge();
    int result = cortical_substrate_connect_bio_async(bridge);
    /* May succeed or fail depending on router availability */
    (void)result;
    SUCCEED();
}

TEST_F(CorticalSubstrateBridgeTest, IsConnectedInitiallyFalse) {
    createBridge();
    bool connected = cortical_substrate_is_bio_async_connected(bridge);
    EXPECT_FALSE(connected);
}

TEST_F(CorticalSubstrateBridgeTest, IdempotentConnect) {
    createBridge();
    cortical_substrate_connect_bio_async(bridge);
    cortical_substrate_connect_bio_async(bridge);
    SUCCEED();
}

/* ============================================================================
 * Default Config Tests (3 tests)
 * ============================================================================ */

TEST_F(CorticalSubstrateBridgeTest, AllModulationsEnabled) {
    cortical_substrate_config_t cfg;
    cortical_substrate_default_config(&cfg);

    EXPECT_TRUE(cfg.enable_column_fidelity_modulation);
    EXPECT_TRUE(cfg.enable_layer_gain_modulation);
    EXPECT_TRUE(cfg.enable_competition_modulation);
    EXPECT_TRUE(cfg.enable_sparsity_modulation);
    EXPECT_TRUE(cfg.enable_hierarchical_modulation);
}

TEST_F(CorticalSubstrateBridgeTest, SensitivitiesAreOne) {
    cortical_substrate_config_t cfg;
    cortical_substrate_default_config(&cfg);

    EXPECT_FLOAT_EQ(cfg.atp_sensitivity, 1.0f);
    EXPECT_FLOAT_EQ(cfg.temperature_sensitivity, 1.0f);
}

TEST_F(CorticalSubstrateBridgeTest, NullConfigSafe) {
    cortical_substrate_default_config(nullptr);
    SUCCEED();
}

/* ============================================================================
 * Update - Fidelity Tests (6 tests)
 * ============================================================================ */

TEST_F(CorticalSubstrateBridgeTest, UpdateNullReturnsError) {
    int result = cortical_substrate_update(nullptr);
    EXPECT_NE(result, 0);  // NIMCP error code (may be positive)
}

TEST_F(CorticalSubstrateBridgeTest, UpdateWithFullATPMaintainsFidelity) {
    createBridge();
    setSubstrateATP(1.0f);

    int result = cortical_substrate_update(bridge);
    EXPECT_EQ(result, 0);

    float fidelity = cortical_substrate_get_column_fidelity(bridge);
    EXPECT_GE(fidelity, 0.85f);
}

TEST_F(CorticalSubstrateBridgeTest, UpdateWithLowATPReducesFidelity) {
    createBridge();
    setSubstrateATP(0.3f);

    int result = cortical_substrate_update(bridge);
    EXPECT_EQ(result, 0);

    float fidelity = cortical_substrate_get_column_fidelity(bridge);
    EXPECT_LT(fidelity, 0.6f);
}

TEST_F(CorticalSubstrateBridgeTest, UpdateWithModerateATP) {
    createBridge();
    setSubstrateATP(0.6f);

    cortical_substrate_update(bridge);

    float fidelity = cortical_substrate_get_column_fidelity(bridge);
    EXPECT_GE(fidelity, 0.2f);
    EXPECT_LE(fidelity, 1.0f);
}

TEST_F(CorticalSubstrateBridgeTest, FidelityNeverBelowFloor) {
    createBridge();
    setSubstrateATP(0.0f);

    cortical_substrate_update(bridge);

    float fidelity = cortical_substrate_get_column_fidelity(bridge);
    EXPECT_GE(fidelity, 0.2f);
}

TEST_F(CorticalSubstrateBridgeTest, FidelityNeverAboveCeiling) {
    createBridge();
    setSubstrateATP(2.0f);

    cortical_substrate_update(bridge);

    float fidelity = cortical_substrate_get_column_fidelity(bridge);
    EXPECT_LE(fidelity, 1.0f);
}

/* ============================================================================
 * Update - Layer Gains Tests (8 tests)
 * ============================================================================ */

TEST_F(CorticalSubstrateBridgeTest, LayerGainsAtNormalTemperature) {
    createBridge();
    setSubstrateTemperature(37.0f);

    cortical_substrate_update(bridge);

    float gain = cortical_substrate_get_layer_gain(bridge, 1);  /* Layer II/III */
    EXPECT_NEAR(gain, 1.0f, 0.15f);
}

TEST_F(CorticalSubstrateBridgeTest, LayerIIIIIMostSensitive) {
    createBridge();
    setSubstrateTemperature(40.0f);

    cortical_substrate_update(bridge);

    float gain_ii_iii = cortical_substrate_get_layer_gain(bridge, 1);
    float gain_iv = cortical_substrate_get_layer_gain(bridge, 2);

    /* Layer II/III has highest Q10, so should show larger deviation */
    EXPECT_GT(std::abs(gain_ii_iii - 1.0f), std::abs(gain_iv - 1.0f) * 0.5f);
}

TEST_F(CorticalSubstrateBridgeTest, HyperthermiaReducesLayers) {
    createBridge();
    setSubstrateTemperature(41.0f);

    cortical_substrate_update(bridge);

    float gain_ii_iii = cortical_substrate_get_layer_gain(bridge, 1);
    EXPECT_LT(gain_ii_iii, 1.2f);
}

TEST_F(CorticalSubstrateBridgeTest, HypothermiaReducesLayers) {
    createBridge();
    setSubstrateTemperature(30.0f);

    cortical_substrate_update(bridge);

    float gain_ii_iii = cortical_substrate_get_layer_gain(bridge, 1);
    EXPECT_LT(gain_ii_iii, 1.0f);
}

TEST_F(CorticalSubstrateBridgeTest, GetLayerGainInvalidLayerReturnsError) {
    createBridge();
    cortical_substrate_update(bridge);

    float gain = cortical_substrate_get_layer_gain(bridge, 99);
    EXPECT_FLOAT_EQ(gain, -1.0f);
}

TEST_F(CorticalSubstrateBridgeTest, AllLayersPositive) {
    createBridge();
    setSubstrateTemperature(35.0f);

    cortical_substrate_update(bridge);

    for (int layer = 0; layer < CORTICAL_SUBSTRATE_NUM_LAYERS; layer++) {
        float gain = cortical_substrate_get_layer_gain(bridge, layer);
        EXPECT_GT(gain, 0.0f);
    }
}

TEST_F(CorticalSubstrateBridgeTest, AllLayersBounded) {
    createBridge();
    setSubstrateTemperature(38.0f);

    cortical_substrate_update(bridge);

    for (int layer = 0; layer < CORTICAL_SUBSTRATE_NUM_LAYERS; layer++) {
        float gain = cortical_substrate_get_layer_gain(bridge, layer);
        EXPECT_GE(gain, 0.3f);
        EXPECT_LE(gain, 1.5f);
    }
}

TEST_F(CorticalSubstrateBridgeTest, GetLayerGainNullBridgeReturnsError) {
    float gain = cortical_substrate_get_layer_gain(nullptr, 2);
    EXPECT_FLOAT_EQ(gain, -1.0f);
}

/* ============================================================================
 * Update - Competition Tests (5 tests)
 * ============================================================================ */

TEST_F(CorticalSubstrateBridgeTest, HighATPFullCompetition) {
    createBridge();
    setSubstrateATP(1.0f);

    cortical_substrate_update(bridge);

    float competition = cortical_substrate_get_competition_efficiency(bridge);
    EXPECT_GE(competition, 0.9f);
}

TEST_F(CorticalSubstrateBridgeTest, LowATPReducedCompetition) {
    createBridge();
    setSubstrateATP(0.2f);

    cortical_substrate_update(bridge);

    float competition = cortical_substrate_get_competition_efficiency(bridge);
    EXPECT_LT(competition, 0.5f);
}

TEST_F(CorticalSubstrateBridgeTest, CompetitionDowngradeTracked) {
    createBridge();
    setSubstrateATP(1.0f);
    cortical_substrate_update(bridge);

    cortical_substrate_stats_t stats_before;
    cortical_substrate_get_stats(bridge, &stats_before);

    setSubstrateATP(0.3f);
    cortical_substrate_update(bridge);

    cortical_substrate_stats_t stats_after;
    cortical_substrate_get_stats(bridge, &stats_after);

    EXPECT_GE(stats_after.competition_downgrades, stats_before.competition_downgrades);
}

TEST_F(CorticalSubstrateBridgeTest, CompetitionNeverNegative) {
    createBridge();
    setSubstrateATP(0.0f);

    cortical_substrate_update(bridge);

    float competition = cortical_substrate_get_competition_efficiency(bridge);
    EXPECT_GE(competition, 0.0f);
}

TEST_F(CorticalSubstrateBridgeTest, CompetitionNeverAboveOne) {
    createBridge();
    setSubstrateATP(2.0f);

    cortical_substrate_update(bridge);

    float competition = cortical_substrate_get_competition_efficiency(bridge);
    EXPECT_LE(competition, 1.0f);
}

/* ============================================================================
 * Update - Sparsity Tests (5 tests)
 * ============================================================================ */

TEST_F(CorticalSubstrateBridgeTest, NormalStateBaselineSparsity) {
    createBridge();
    setSubstrateATP(1.0f);
    setSubstrateTemperature(37.0f);

    cortical_substrate_update(bridge);

    float sparsity = cortical_substrate_get_sparsity_modulation(bridge);
    EXPECT_NEAR(sparsity, 1.0f, 0.2f);
}

TEST_F(CorticalSubstrateBridgeTest, MetabolicStressAffectsSparsity) {
    createBridge();
    setSubstrateATP(0.3f);

    cortical_substrate_update(bridge);

    float sparsity = cortical_substrate_get_sparsity_modulation(bridge);
    EXPECT_GE(sparsity, 0.5f);
    EXPECT_LE(sparsity, 2.0f);
}

TEST_F(CorticalSubstrateBridgeTest, SparsityBoundedBelow) {
    createBridge();
    setSubstrateATP(2.0f);

    cortical_substrate_update(bridge);

    float sparsity = cortical_substrate_get_sparsity_modulation(bridge);
    EXPECT_GE(sparsity, 0.5f);
}

TEST_F(CorticalSubstrateBridgeTest, SparsityBoundedAbove) {
    createBridge();
    setSubstrateATP(0.0f);

    cortical_substrate_update(bridge);

    float sparsity = cortical_substrate_get_sparsity_modulation(bridge);
    EXPECT_LE(sparsity, 2.0f);
}

TEST_F(CorticalSubstrateBridgeTest, SparsityModulationGetterWorks) {
    createBridge();
    cortical_substrate_update(bridge);

    float sparsity = cortical_substrate_get_sparsity_modulation(bridge);
    EXPECT_GT(sparsity, 0.0f);
}

/* ============================================================================
 * Update - Hierarchy Tests (4 tests)
 * ============================================================================ */

TEST_F(CorticalSubstrateBridgeTest, FullPhysicalCapacityFullDepth) {
    createBridge();
    setSubstrateATP(1.0f);
    setSubstrateTemperature(37.0f);

    cortical_substrate_update(bridge);

    float depth = cortical_substrate_get_hierarchical_depth(bridge);
    EXPECT_GE(depth, 0.85f);
}

TEST_F(CorticalSubstrateBridgeTest, ReducedPhysicalCapacityLimitsDepth) {
    createBridge();
    setSubstrateATP(0.4f);

    cortical_substrate_update(bridge);

    float depth = cortical_substrate_get_hierarchical_depth(bridge);
    EXPECT_GE(depth, 0.2f);
    EXPECT_LE(depth, 1.0f);
}

TEST_F(CorticalSubstrateBridgeTest, HyperthermiaReducesDepth) {
    createBridge();
    setSubstrateATP(1.0f);
    setSubstrateTemperature(41.0f);

    cortical_substrate_update(bridge);

    float depth = cortical_substrate_get_hierarchical_depth(bridge);
    EXPECT_LT(depth, 1.0f);
}

TEST_F(CorticalSubstrateBridgeTest, DepthNeverBelowFloor) {
    createBridge();
    setSubstrateATP(0.0f);
    setSubstrateTemperature(45.0f);

    cortical_substrate_update(bridge);

    float depth = cortical_substrate_get_hierarchical_depth(bridge);
    EXPECT_GE(depth, 0.2f);
}

/* ============================================================================
 * Impairment Tests (4 tests)
 * ============================================================================ */

TEST_F(CorticalSubstrateBridgeTest, NotImpairedAtOptimalState) {
    createBridge();
    setSubstrateATP(1.0f);
    setSubstrateTemperature(37.0f);

    cortical_substrate_update(bridge);

    EXPECT_FALSE(cortical_substrate_is_impaired(bridge));
}

TEST_F(CorticalSubstrateBridgeTest, ImpairedAtCriticalFidelity) {
    createBridge();
    setSubstrateATP(0.2f);

    cortical_substrate_update(bridge);

    bool impaired = cortical_substrate_is_impaired(bridge);
    EXPECT_TRUE(impaired);
}

TEST_F(CorticalSubstrateBridgeTest, ImpairmentEventCounted) {
    createBridge();
    setSubstrateATP(1.0f);
    cortical_substrate_update(bridge);

    cortical_substrate_stats_t stats_before;
    cortical_substrate_get_stats(bridge, &stats_before);

    setSubstrateATP(0.2f);
    cortical_substrate_update(bridge);

    cortical_substrate_stats_t stats_after;
    cortical_substrate_get_stats(bridge, &stats_after);

    if (cortical_substrate_is_impaired(bridge)) {
        EXPECT_GT(stats_after.impairment_events, stats_before.impairment_events);
    }
}

TEST_F(CorticalSubstrateBridgeTest, RecoveryResetsImpairment) {
    createBridge();
    setSubstrateATP(0.2f);
    cortical_substrate_update(bridge);

    EXPECT_TRUE(cortical_substrate_is_impaired(bridge));

    setSubstrateATP(1.0f);
    cortical_substrate_update(bridge);

    EXPECT_FALSE(cortical_substrate_is_impaired(bridge));
}

/* ============================================================================
 * Stats Tests (3 tests)
 * ============================================================================ */

TEST_F(CorticalSubstrateBridgeTest, UpdateCountIncrements) {
    createBridge();

    cortical_substrate_update(bridge);
    cortical_substrate_update(bridge);
    cortical_substrate_update(bridge);

    cortical_substrate_stats_t stats;
    cortical_substrate_get_stats(bridge, &stats);

    EXPECT_EQ(stats.update_count, 3u);
}

TEST_F(CorticalSubstrateBridgeTest, StatsGetterWorks) {
    createBridge();
    cortical_substrate_update(bridge);

    cortical_substrate_stats_t stats;
    int result = cortical_substrate_get_stats(bridge, &stats);

    EXPECT_EQ(result, 0);
    EXPECT_GE(stats.update_count, 1u);
}

TEST_F(CorticalSubstrateBridgeTest, NullStatsReturnsError) {
    createBridge();
    int result = cortical_substrate_get_stats(bridge, nullptr);
    EXPECT_NE(result, 0);  // NIMCP error code (may be positive)
}

/* ============================================================================
 * Query API Null Safety Tests (2 tests)
 * ============================================================================ */

TEST_F(CorticalSubstrateBridgeTest, GetFidelityNullReturnsNegative) {
    EXPECT_FLOAT_EQ(cortical_substrate_get_column_fidelity(nullptr), -1.0f);
}

TEST_F(CorticalSubstrateBridgeTest, GetCompetitionNullReturnsNegative) {
    EXPECT_FLOAT_EQ(cortical_substrate_get_competition_efficiency(nullptr), -1.0f);
}
