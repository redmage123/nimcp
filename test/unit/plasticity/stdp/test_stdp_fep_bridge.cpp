/**
 * @file test_stdp_fep_bridge.cpp
 * @brief Unit tests for STDP-FEP bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Comprehensive unit tests for STDP-FEP integration bridge
 * WHY:  Verify bidirectional FEP-STDP interaction for predictive learning
 * HOW:  Test lifecycle, connections, FEP→STDP, STDP→FEP, state/stats, bio-async
 */

#include <gtest/gtest.h>
#include "plasticity/stdp/nimcp_stdp_fep_bridge.h"

class StdpFepBridgeTest : public ::testing::Test {
protected:
    stdp_fep_bridge_t* bridge = nullptr;

    void SetUp() override {
        stdp_fep_config_t config;
        stdp_fep_bridge_default_config(&config);
        bridge = stdp_fep_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge) {
            stdp_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(StdpFepBridgeTest, CreateDestroy) {
    ASSERT_NE(nullptr, bridge);
}

TEST_F(StdpFepBridgeTest, CreateWithNullConfig) {
    stdp_fep_bridge_t* b = stdp_fep_bridge_create(nullptr);
    ASSERT_NE(nullptr, b);
    stdp_fep_bridge_destroy(b);
}

TEST_F(StdpFepBridgeTest, DestroyNull) {
    stdp_fep_bridge_destroy(nullptr);  // Should not crash
}

TEST_F(StdpFepBridgeTest, DefaultConfig) {
    stdp_fep_config_t config;
    int result = stdp_fep_bridge_default_config(&config);
    EXPECT_EQ(0, result);

    EXPECT_GT(config.pe_min_threshold, 0.0f);
    EXPECT_GT(config.pe_max_threshold, config.pe_min_threshold);
    EXPECT_GT(config.precision_sensitivity, 0.0f);
    EXPECT_TRUE(config.enable_pe_scaling);
    EXPECT_TRUE(config.enable_precision_weighting);
}

TEST_F(StdpFepBridgeTest, DefaultConfigNull) {
    int result = stdp_fep_bridge_default_config(nullptr);
    EXPECT_EQ(-1, result);
}

/* ============================================================================
 * Connection Tests
 * ============================================================================ */

TEST_F(StdpFepBridgeTest, ConnectFep) {
    fep_system_t* fep = nullptr;  // Mock FEP system
    int result = stdp_fep_bridge_connect_fep(bridge, fep);
    EXPECT_EQ(0, result);
}

TEST_F(StdpFepBridgeTest, ConnectFepNull) {
    int result = stdp_fep_bridge_connect_fep(nullptr, nullptr);
    EXPECT_EQ(-1, result);
}

TEST_F(StdpFepBridgeTest, ConnectStdp) {
    stdp_synapse_t synapses[10];
    int result = stdp_fep_bridge_connect_stdp(bridge, synapses, 10);
    EXPECT_EQ(0, result);
}

TEST_F(StdpFepBridgeTest, ConnectStdpNull) {
    int result = stdp_fep_bridge_connect_stdp(nullptr, nullptr, 0);
    EXPECT_EQ(-1, result);
}

TEST_F(StdpFepBridgeTest, Disconnect) {
    int result = stdp_fep_bridge_disconnect(bridge);
    EXPECT_EQ(0, result);
}

TEST_F(StdpFepBridgeTest, DisconnectNull) {
    int result = stdp_fep_bridge_disconnect(nullptr);
    EXPECT_EQ(-1, result);
}

/* ============================================================================
 * FEP → STDP Direction Tests
 * ============================================================================ */

TEST_F(StdpFepBridgeTest, ApplyPeScalingLowPe) {
    float pe = 0.2f;  // Below min threshold
    float scaling = stdp_fep_apply_pe_scaling(bridge, pe);
    EXPECT_GE(scaling, 0.0f);
    EXPECT_LE(scaling, STDP_FEP_LR_MAX_FACTOR);
}

TEST_F(StdpFepBridgeTest, ApplyPeScalingModeratePe) {
    float pe = 2.0f;  // Moderate PE
    float scaling = stdp_fep_apply_pe_scaling(bridge, pe);
    EXPECT_GT(scaling, 0.0f);
    EXPECT_LE(scaling, STDP_FEP_LR_MAX_FACTOR);
}

TEST_F(StdpFepBridgeTest, ApplyPeScalingHighPe) {
    float pe = 15.0f;  // Above max threshold
    float scaling = stdp_fep_apply_pe_scaling(bridge, pe);
    EXPECT_GT(scaling, 0.0f);
    EXPECT_LE(scaling, STDP_FEP_LR_MAX_FACTOR);
}

TEST_F(StdpFepBridgeTest, ApplyPeScalingNull) {
    float scaling = stdp_fep_apply_pe_scaling(nullptr, 1.0f);
    EXPECT_EQ(1.0f, scaling);  // Should return baseline
}

TEST_F(StdpFepBridgeTest, ApplyPrecisionWeightingLowPrecision) {
    float precision = 0.05f;  // Very low precision
    float scaling = stdp_fep_apply_precision_weighting(bridge, precision);
    EXPECT_GE(scaling, STDP_FEP_PRECISION_MIN);
    EXPECT_LE(scaling, STDP_FEP_PRECISION_MAX);
}

TEST_F(StdpFepBridgeTest, ApplyPrecisionWeightingHighPrecision) {
    float precision = 1.5f;  // High precision
    float scaling = stdp_fep_apply_precision_weighting(bridge, precision);
    EXPECT_GT(scaling, 1.0f);
    EXPECT_LE(scaling, STDP_FEP_PRECISION_MAX);
}

TEST_F(StdpFepBridgeTest, ApplyPrecisionWeightingNull) {
    float scaling = stdp_fep_apply_precision_weighting(nullptr, 1.0f);
    EXPECT_EQ(1.0f, scaling);
}

TEST_F(StdpFepBridgeTest, ApplyBeliefModulationConverged) {
    float belief_delta = 0.005f;  // Small change (converged)
    float scaling = stdp_fep_apply_belief_modulation(bridge, belief_delta);
    EXPECT_GE(scaling, STDP_FEP_LR_MIN_FACTOR);
    EXPECT_LE(scaling, STDP_FEP_LR_MAX_FACTOR);
}

TEST_F(StdpFepBridgeTest, ApplyBeliefModulationDiverging) {
    float belief_delta = 0.5f;  // Large change (learning)
    float scaling = stdp_fep_apply_belief_modulation(bridge, belief_delta);
    EXPECT_GT(scaling, 1.0f);
    EXPECT_LE(scaling, STDP_FEP_LR_MAX_FACTOR);
}

TEST_F(StdpFepBridgeTest, ApplyBeliefModulationNull) {
    float scaling = stdp_fep_apply_belief_modulation(nullptr, 0.1f);
    EXPECT_EQ(1.0f, scaling);
}

TEST_F(StdpFepBridgeTest, GetEffectiveLr) {
    float base_lr = 0.01f;
    float effective_lr = stdp_fep_get_effective_lr(bridge, base_lr);
    EXPECT_GT(effective_lr, 0.0f);
}

TEST_F(StdpFepBridgeTest, GetEffectiveLrNull) {
    float effective_lr = stdp_fep_get_effective_lr(nullptr, 0.01f);
    EXPECT_EQ(0.01f, effective_lr);
}

TEST_F(StdpFepBridgeTest, GetEffectiveLrWithModulation) {
    // Apply some modulation factors
    stdp_fep_apply_pe_scaling(bridge, 2.0f);
    stdp_fep_apply_precision_weighting(bridge, 1.2f);

    float base_lr = 0.01f;
    float effective_lr = stdp_fep_get_effective_lr(bridge, base_lr);
    EXPECT_NE(effective_lr, base_lr);  // Should be modulated
}

/* ============================================================================
 * STDP → FEP Direction Tests
 * ============================================================================ */

TEST_F(StdpFepBridgeTest, ReportWeightChanges) {
    float weight_delta = 0.05f;
    int result = stdp_fep_report_weight_changes(bridge, weight_delta);
    EXPECT_EQ(0, result);
}

TEST_F(StdpFepBridgeTest, ReportWeightChangesNull) {
    int result = stdp_fep_report_weight_changes(nullptr, 0.05f);
    EXPECT_EQ(-1, result);
}

TEST_F(StdpFepBridgeTest, ReportWeightChangesNegative) {
    float weight_delta = -0.03f;  // Depression
    int result = stdp_fep_report_weight_changes(bridge, weight_delta);
    EXPECT_EQ(0, result);
}

TEST_F(StdpFepBridgeTest, ComputeComplexityRegularization) {
    float regularization = stdp_fep_compute_complexity_regularization(bridge);
    EXPECT_GE(regularization, 0.0f);
    EXPECT_LE(regularization, 1.0f);
}

TEST_F(StdpFepBridgeTest, ComputeComplexityRegularizationNull) {
    float regularization = stdp_fep_compute_complexity_regularization(nullptr);
    EXPECT_EQ(1.0f, regularization);  // No regularization
}

/* ============================================================================
 * Update Cycle Tests
 * ============================================================================ */

TEST_F(StdpFepBridgeTest, Update) {
    int result = stdp_fep_bridge_update(bridge, 10);
    EXPECT_EQ(0, result);
}

TEST_F(StdpFepBridgeTest, UpdateNull) {
    int result = stdp_fep_bridge_update(nullptr, 10);
    EXPECT_EQ(-1, result);
}

TEST_F(StdpFepBridgeTest, UpdateMultipleTimes) {
    for (int i = 0; i < 10; i++) {
        int result = stdp_fep_bridge_update(bridge, 5);
        EXPECT_EQ(0, result);
    }
}

/* ============================================================================
 * State/Stats Tests
 * ============================================================================ */

TEST_F(StdpFepBridgeTest, GetState) {
    stdp_fep_state_t state;
    int result = stdp_fep_bridge_get_state(bridge, &state);
    EXPECT_EQ(0, result);
}

TEST_F(StdpFepBridgeTest, GetStateNull) {
    stdp_fep_state_t state;
    int result = stdp_fep_bridge_get_state(nullptr, &state);
    EXPECT_EQ(-1, result);

    result = stdp_fep_bridge_get_state(bridge, nullptr);
    EXPECT_EQ(-1, result);
}

TEST_F(StdpFepBridgeTest, GetStats) {
    stdp_fep_stats_t stats;
    int result = stdp_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(0, result);
}

TEST_F(StdpFepBridgeTest, GetStatsNull) {
    stdp_fep_stats_t stats;
    int result = stdp_fep_bridge_get_stats(nullptr, &stats);
    EXPECT_EQ(-1, result);

    result = stdp_fep_bridge_get_stats(bridge, nullptr);
    EXPECT_EQ(-1, result);
}

TEST_F(StdpFepBridgeTest, StatsAfterOperations) {
    // Perform some operations
    stdp_fep_apply_pe_scaling(bridge, 2.0f);
    stdp_fep_apply_precision_weighting(bridge, 1.2f);
    stdp_fep_report_weight_changes(bridge, 0.05f);
    stdp_fep_bridge_update(bridge, 10);

    stdp_fep_stats_t stats;
    int result = stdp_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(0, result);
    EXPECT_GT(stats.total_updates, 0u);
}

/* ============================================================================
 * Bio-Async Tests
 * ============================================================================ */

TEST_F(StdpFepBridgeTest, ConnectBioAsync) {
    int result = stdp_fep_bridge_connect_bio_async(bridge);
    EXPECT_EQ(0, result);
}

TEST_F(StdpFepBridgeTest, ConnectBioAsyncNull) {
    int result = stdp_fep_bridge_connect_bio_async(nullptr);
    EXPECT_EQ(-1, result);
}

TEST_F(StdpFepBridgeTest, DisconnectBioAsync) {
    stdp_fep_bridge_connect_bio_async(bridge);
    int result = stdp_fep_bridge_disconnect_bio_async(bridge);
    EXPECT_EQ(0, result);
}

TEST_F(StdpFepBridgeTest, DisconnectBioAsyncNull) {
    int result = stdp_fep_bridge_disconnect_bio_async(nullptr);
    EXPECT_EQ(-1, result);
}

TEST_F(StdpFepBridgeTest, IsBioAsyncConnected) {
    bool connected = stdp_fep_bridge_is_bio_async_connected(bridge);
    EXPECT_FALSE(connected);  // Not connected initially
}

TEST_F(StdpFepBridgeTest, IsBioAsyncConnectedAfterConnect) {
    stdp_fep_bridge_connect_bio_async(bridge);
    bool connected = stdp_fep_bridge_is_bio_async_connected(bridge);
    // May be true or false depending on bio-router availability
}

TEST_F(StdpFepBridgeTest, IsBioAsyncConnectedNull) {
    bool connected = stdp_fep_bridge_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
