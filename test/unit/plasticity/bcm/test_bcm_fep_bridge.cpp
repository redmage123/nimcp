/**
 * @file test_bcm_fep_bridge.cpp
 * @brief Unit tests for BCM-FEP bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Comprehensive unit tests for BCM-FEP integration bridge
 * WHY:  Verify bidirectional FEP-BCM interaction for complexity regularization
 * HOW:  Test lifecycle, connections, FEP→BCM, BCM→FEP, state/stats, bio-async
 */

#include <gtest/gtest.h>
#include "plasticity/bcm/nimcp_bcm_fep_bridge.h"

class BcmFepBridgeTest : public ::testing::Test {
protected:
    bcm_fep_bridge_t* bridge = nullptr;

    void SetUp() override {
        bcm_fep_config_t config;
        bcm_fep_bridge_default_config(&config);
        bridge = bcm_fep_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge) {
            bcm_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(BcmFepBridgeTest, CreateDestroy) {
    ASSERT_NE(nullptr, bridge);
}

TEST_F(BcmFepBridgeTest, CreateWithNullConfig) {
    bcm_fep_bridge_t* b = bcm_fep_bridge_create(nullptr);
    ASSERT_NE(nullptr, b);
    bcm_fep_bridge_destroy(b);
}

TEST_F(BcmFepBridgeTest, DestroyNull) {
    bcm_fep_bridge_destroy(nullptr);  // Should not crash
}

TEST_F(BcmFepBridgeTest, DefaultConfig) {
    bcm_fep_config_t config;
    int result = bcm_fep_bridge_default_config(&config);
    EXPECT_EQ(0, result);

    EXPECT_GT(config.complexity_threshold_gain, 0.0f);
    EXPECT_GT(config.precision_selectivity_gain, 0.0f);
    EXPECT_TRUE(config.enable_complexity_regularization);
    EXPECT_TRUE(config.enable_precision_modulation);
}

TEST_F(BcmFepBridgeTest, DefaultConfigNull) {
    int result = bcm_fep_bridge_default_config(nullptr);
    EXPECT_EQ(-1, result);
}

/* ============================================================================
 * Connection Tests
 * ============================================================================ */

TEST_F(BcmFepBridgeTest, ConnectFep) {
    fep_system_t* fep = nullptr;  // Mock FEP system
    int result = bcm_fep_bridge_connect_fep(bridge, fep);
    EXPECT_EQ(0, result);
}

TEST_F(BcmFepBridgeTest, ConnectFepNull) {
    int result = bcm_fep_bridge_connect_fep(nullptr, nullptr);
    EXPECT_EQ(-1, result);
}

TEST_F(BcmFepBridgeTest, ConnectBcm) {
    bcm_synapse_t synapses[10];
    int result = bcm_fep_bridge_connect_bcm(bridge, synapses, 10);
    EXPECT_EQ(0, result);
}

TEST_F(BcmFepBridgeTest, ConnectBcmNull) {
    int result = bcm_fep_bridge_connect_bcm(nullptr, nullptr, 0);
    EXPECT_EQ(-1, result);
}

TEST_F(BcmFepBridgeTest, Disconnect) {
    int result = bcm_fep_bridge_disconnect(bridge);
    EXPECT_EQ(0, result);
}

TEST_F(BcmFepBridgeTest, DisconnectNull) {
    int result = bcm_fep_bridge_disconnect(nullptr);
    EXPECT_EQ(-1, result);
}

/* ============================================================================
 * FEP → BCM Direction Tests
 * ============================================================================ */

TEST_F(BcmFepBridgeTest, ApplyComplexityRegularizationLow) {
    float complexity = 0.5f;  // Low complexity
    float scaling = bcm_fep_apply_complexity_regularization(bridge, complexity);
    EXPECT_GT(scaling, 0.0f);
    EXPECT_LE(scaling, 10.0f);  // Within reasonable range
}

TEST_F(BcmFepBridgeTest, ApplyComplexityRegularizationHigh) {
    float complexity = 8.0f;  // High complexity
    float scaling = bcm_fep_apply_complexity_regularization(bridge, complexity);
    EXPECT_GT(scaling, 1.0f);  // Should increase threshold
}

TEST_F(BcmFepBridgeTest, ApplyComplexityRegularizationNull) {
    float scaling = bcm_fep_apply_complexity_regularization(nullptr, 1.0f);
    EXPECT_EQ(1.0f, scaling);  // Baseline
}

TEST_F(BcmFepBridgeTest, ApplyPrecisionModulationLow) {
    float precision = 0.2f;  // Low precision
    float scaling = bcm_fep_apply_precision_modulation(bridge, precision);
    EXPECT_GE(scaling, BCM_FEP_PRECISION_MIN);
    EXPECT_LE(scaling, BCM_FEP_PRECISION_MAX);
}

TEST_F(BcmFepBridgeTest, ApplyPrecisionModulationHigh) {
    float precision = 1.8f;  // High precision
    float scaling = bcm_fep_apply_precision_modulation(bridge, precision);
    EXPECT_GT(scaling, 1.0f);
    EXPECT_LE(scaling, BCM_FEP_PRECISION_MAX);
}

TEST_F(BcmFepBridgeTest, ApplyPrecisionModulationNull) {
    float scaling = bcm_fep_apply_precision_modulation(nullptr, 1.0f);
    EXPECT_EQ(1.0f, scaling);
}

TEST_F(BcmFepBridgeTest, ApplyPeGatingLow) {
    float pe = 0.1f;  // Low PE
    float scaling = bcm_fep_apply_pe_gating(bridge, pe);
    EXPECT_GE(scaling, BCM_FEP_LR_MIN_FACTOR);
    EXPECT_LE(scaling, BCM_FEP_LR_MAX_FACTOR);
}

TEST_F(BcmFepBridgeTest, ApplyPeGatingHigh) {
    float pe = 5.0f;  // High PE
    float scaling = bcm_fep_apply_pe_gating(bridge, pe);
    EXPECT_GT(scaling, 1.0f);
    EXPECT_LE(scaling, BCM_FEP_LR_MAX_FACTOR);
}

TEST_F(BcmFepBridgeTest, ApplyPeGatingNull) {
    float scaling = bcm_fep_apply_pe_gating(nullptr, 1.0f);
    EXPECT_EQ(1.0f, scaling);
}

TEST_F(BcmFepBridgeTest, GetEffectiveThreshold) {
    float base_threshold = 1.0f;
    float effective = bcm_fep_get_effective_threshold(bridge, base_threshold);
    EXPECT_GT(effective, 0.0f);
}

TEST_F(BcmFepBridgeTest, GetEffectiveThresholdNull) {
    float effective = bcm_fep_get_effective_threshold(nullptr, 1.0f);
    EXPECT_EQ(1.0f, effective);
}

TEST_F(BcmFepBridgeTest, GetEffectiveThresholdWithComplexity) {
    bcm_fep_apply_complexity_regularization(bridge, 5.0f);
    float base_threshold = 1.0f;
    float effective = bcm_fep_get_effective_threshold(bridge, base_threshold);
    EXPECT_NE(effective, base_threshold);  // Should be modulated
}

TEST_F(BcmFepBridgeTest, GetEffectiveLr) {
    float base_lr = 0.01f;
    float effective_lr = bcm_fep_get_effective_lr(bridge, base_lr);
    EXPECT_GT(effective_lr, 0.0f);
}

TEST_F(BcmFepBridgeTest, GetEffectiveLrNull) {
    float effective_lr = bcm_fep_get_effective_lr(nullptr, 0.01f);
    EXPECT_EQ(0.01f, effective_lr);
}

/* ============================================================================
 * BCM → FEP Direction Tests
 * ============================================================================ */

TEST_F(BcmFepBridgeTest, ReportThresholdChanges) {
    float threshold_delta = 0.05f;
    int result = bcm_fep_report_threshold_changes(bridge, threshold_delta);
    EXPECT_EQ(0, result);
}

TEST_F(BcmFepBridgeTest, ReportThresholdChangesNull) {
    int result = bcm_fep_report_threshold_changes(nullptr, 0.05f);
    EXPECT_EQ(-1, result);
}

TEST_F(BcmFepBridgeTest, ComputeSparsity) {
    float sparsity = bcm_fep_compute_sparsity(bridge);
    EXPECT_GE(sparsity, 0.0f);
    EXPECT_LE(sparsity, 1.0f);
}

TEST_F(BcmFepBridgeTest, ComputeSparsityNull) {
    float sparsity = bcm_fep_compute_sparsity(nullptr);
    EXPECT_EQ(0.0f, sparsity);
}

TEST_F(BcmFepBridgeTest, ReportSparsity) {
    int result = bcm_fep_report_sparsity(bridge, 50);
    EXPECT_EQ(0, result);
}

TEST_F(BcmFepBridgeTest, ReportSparsityNull) {
    int result = bcm_fep_report_sparsity(nullptr, 50);
    EXPECT_EQ(-1, result);
}

/* ============================================================================
 * Update Cycle Tests
 * ============================================================================ */

TEST_F(BcmFepBridgeTest, Update) {
    int result = bcm_fep_bridge_update(bridge, 10);
    EXPECT_EQ(0, result);
}

TEST_F(BcmFepBridgeTest, UpdateNull) {
    int result = bcm_fep_bridge_update(nullptr, 10);
    EXPECT_EQ(-1, result);
}

TEST_F(BcmFepBridgeTest, UpdateMultipleTimes) {
    for (int i = 0; i < 10; i++) {
        int result = bcm_fep_bridge_update(bridge, 5);
        EXPECT_EQ(0, result);
    }
}

/* ============================================================================
 * State/Stats Tests
 * ============================================================================ */

TEST_F(BcmFepBridgeTest, GetState) {
    bcm_fep_state_t state;
    int result = bcm_fep_bridge_get_state(bridge, &state);
    EXPECT_EQ(0, result);
}

TEST_F(BcmFepBridgeTest, GetStateNull) {
    bcm_fep_state_t state;
    int result = bcm_fep_bridge_get_state(nullptr, &state);
    EXPECT_EQ(-1, result);

    result = bcm_fep_bridge_get_state(bridge, nullptr);
    EXPECT_EQ(-1, result);
}

TEST_F(BcmFepBridgeTest, GetStats) {
    bcm_fep_stats_t stats;
    int result = bcm_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(0, result);
}

TEST_F(BcmFepBridgeTest, GetStatsNull) {
    bcm_fep_stats_t stats;
    int result = bcm_fep_bridge_get_stats(nullptr, &stats);
    EXPECT_EQ(-1, result);

    result = bcm_fep_bridge_get_stats(bridge, nullptr);
    EXPECT_EQ(-1, result);
}

TEST_F(BcmFepBridgeTest, StatsAfterOperations) {
    bcm_fep_apply_complexity_regularization(bridge, 3.0f);
    bcm_fep_apply_precision_modulation(bridge, 1.5f);
    bcm_fep_report_sparsity(bridge, 40);
    bcm_fep_bridge_update(bridge, 10);

    bcm_fep_stats_t stats;
    int result = bcm_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(0, result);
    EXPECT_GT(stats.total_updates, 0u);
}

/* ============================================================================
 * Bio-Async Tests
 * ============================================================================ */

TEST_F(BcmFepBridgeTest, ConnectBioAsync) {
    int result = bcm_fep_bridge_connect_bio_async(bridge);
    EXPECT_EQ(0, result);
}

TEST_F(BcmFepBridgeTest, ConnectBioAsyncNull) {
    int result = bcm_fep_bridge_connect_bio_async(nullptr);
    EXPECT_EQ(-1, result);
}

TEST_F(BcmFepBridgeTest, DisconnectBioAsync) {
    bcm_fep_bridge_connect_bio_async(bridge);
    int result = bcm_fep_bridge_disconnect_bio_async(bridge);
    EXPECT_EQ(0, result);
}

TEST_F(BcmFepBridgeTest, DisconnectBioAsyncNull) {
    int result = bcm_fep_bridge_disconnect_bio_async(nullptr);
    EXPECT_EQ(-1, result);
}

TEST_F(BcmFepBridgeTest, IsBioAsyncConnected) {
    bool connected = bcm_fep_bridge_is_bio_async_connected(bridge);
    EXPECT_FALSE(connected);  // Not connected initially
}

TEST_F(BcmFepBridgeTest, IsBioAsyncConnectedNull) {
    bool connected = bcm_fep_bridge_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
