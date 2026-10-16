/**
 * @file test_homeostatic_fep_bridge.cpp
 * @brief Unit tests for Homeostatic-FEP bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Comprehensive unit tests for Homeostatic-FEP integration bridge
 * WHY:  Verify bidirectional FEP-Homeostatic interaction for precision normalization
 * HOW:  Test lifecycle, connections, FEP→Homeostatic, Homeostatic→FEP, state/stats
 */

#include <gtest/gtest.h>
#include "plasticity/homeostatic/nimcp_homeostatic_fep_bridge.h"

class HomeostaticFepBridgeTest : public ::testing::Test {
protected:
    homeostatic_fep_bridge_t* bridge = nullptr;

    void SetUp() override {
        homeostatic_fep_config_t config;
        homeostatic_fep_bridge_default_config(&config);
        bridge = homeostatic_fep_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge) {
            homeostatic_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(HomeostaticFepBridgeTest, CreateDestroy) {
    ASSERT_NE(nullptr, bridge);
}

TEST_F(HomeostaticFepBridgeTest, CreateWithNullConfig) {
    homeostatic_fep_bridge_t* b = homeostatic_fep_bridge_create(nullptr);
    ASSERT_NE(nullptr, b);
    homeostatic_fep_bridge_destroy(b);
}

TEST_F(HomeostaticFepBridgeTest, DestroyNull) {
    homeostatic_fep_bridge_destroy(nullptr);  // Should not crash
}

TEST_F(HomeostaticFepBridgeTest, DefaultConfig) {
    homeostatic_fep_config_t config;
    int result = homeostatic_fep_bridge_default_config(&config);
    EXPECT_EQ(0, result);

    EXPECT_GT(config.precision_target_gain, 0.0f);
    EXPECT_GT(config.free_energy_scaling_factor, 0.0f);
    EXPECT_TRUE(config.enable_precision_normalization);
    EXPECT_TRUE(config.enable_fe_modulation);
}

TEST_F(HomeostaticFepBridgeTest, DefaultConfigNull) {
    int result = homeostatic_fep_bridge_default_config(nullptr);
    EXPECT_EQ(-1, result);
}

/* ============================================================================
 * Connection Tests
 * ============================================================================ */

TEST_F(HomeostaticFepBridgeTest, ConnectFep) {
    fep_system_t* fep = nullptr;  // Mock FEP system
    int result = homeostatic_fep_bridge_connect_fep(bridge, fep);
    EXPECT_EQ(0, result);
}

TEST_F(HomeostaticFepBridgeTest, ConnectFepNull) {
    int result = homeostatic_fep_bridge_connect_fep(nullptr, nullptr);
    EXPECT_EQ(-1, result);
}

TEST_F(HomeostaticFepBridgeTest, ConnectHomeostatic) {
    homeostatic_controller_t controller = {0};  // Mock controller
    int result = homeostatic_fep_bridge_connect_homeostatic(bridge, controller);
    EXPECT_EQ(0, result);
}

TEST_F(HomeostaticFepBridgeTest, ConnectHomeostaticNull) {
    homeostatic_controller_t controller = {0};
    int result = homeostatic_fep_bridge_connect_homeostatic(nullptr, controller);
    EXPECT_EQ(-1, result);
}

TEST_F(HomeostaticFepBridgeTest, Disconnect) {
    int result = homeostatic_fep_bridge_disconnect(bridge);
    EXPECT_EQ(0, result);
}

TEST_F(HomeostaticFepBridgeTest, DisconnectNull) {
    int result = homeostatic_fep_bridge_disconnect(nullptr);
    EXPECT_EQ(-1, result);
}

/* ============================================================================
 * FEP → Homeostatic Direction Tests
 * ============================================================================ */

TEST_F(HomeostaticFepBridgeTest, ApplyPrecisionNormalizationLow) {
    float precision = 0.2f;  // Low precision
    float normalized = homeostatic_fep_apply_precision_normalization(bridge, precision);
    EXPECT_GE(normalized, HOMEOSTATIC_FEP_PRECISION_MIN);
    EXPECT_LE(normalized, HOMEOSTATIC_FEP_PRECISION_MAX);
}

TEST_F(HomeostaticFepBridgeTest, ApplyPrecisionNormalizationHigh) {
    float precision = 1.8f;  // High precision
    float normalized = homeostatic_fep_apply_precision_normalization(bridge, precision);
    EXPECT_GT(normalized, 0.0f);
    EXPECT_LE(normalized, HOMEOSTATIC_FEP_PRECISION_MAX);
}

TEST_F(HomeostaticFepBridgeTest, ApplyPrecisionNormalizationNull) {
    float normalized = homeostatic_fep_apply_precision_normalization(nullptr, 1.0f);
    EXPECT_EQ(1.0f, normalized);
}

TEST_F(HomeostaticFepBridgeTest, ApplyFeModulationLow) {
    float free_energy = 0.5f;  // Low free energy
    float modulation = homeostatic_fep_apply_fe_modulation(bridge, free_energy);
    EXPECT_GT(modulation, 0.0f);
}

TEST_F(HomeostaticFepBridgeTest, ApplyFeModulationHigh) {
    float free_energy = 10.0f;  // High free energy
    float modulation = homeostatic_fep_apply_fe_modulation(bridge, free_energy);
    EXPECT_GT(modulation, 0.0f);
}

TEST_F(HomeostaticFepBridgeTest, ApplyFeModulationNull) {
    float modulation = homeostatic_fep_apply_fe_modulation(nullptr, 1.0f);
    EXPECT_EQ(1.0f, modulation);
}

TEST_F(HomeostaticFepBridgeTest, GetEffectiveTargetRate) {
    float base_rate = 5.0f;
    float effective = homeostatic_fep_get_effective_target_rate(bridge, base_rate);
    EXPECT_GT(effective, 0.0f);
}

TEST_F(HomeostaticFepBridgeTest, GetEffectiveTargetRateNull) {
    float effective = homeostatic_fep_get_effective_target_rate(nullptr, 5.0f);
    EXPECT_EQ(5.0f, effective);
}

TEST_F(HomeostaticFepBridgeTest, GetEffectiveTargetRateWithPrecision) {
    homeostatic_fep_apply_precision_normalization(bridge, 1.5f);
    float base_rate = 5.0f;
    float effective = homeostatic_fep_get_effective_target_rate(bridge, base_rate);
    EXPECT_NE(effective, base_rate);  // Should be modulated
}

/* ============================================================================
 * Homeostatic → FEP Direction Tests
 * ============================================================================ */

TEST_F(HomeostaticFepBridgeTest, ReportScaling) {
    float scaling_factor = 1.2f;
    int result = homeostatic_fep_report_scaling(bridge, scaling_factor);
    EXPECT_EQ(0, result);
}

TEST_F(HomeostaticFepBridgeTest, ReportScalingNull) {
    int result = homeostatic_fep_report_scaling(nullptr, 1.2f);
    EXPECT_EQ(-1, result);
}

TEST_F(HomeostaticFepBridgeTest, ReportScalingDownscale) {
    float scaling_factor = 0.8f;  // Downscaling
    int result = homeostatic_fep_report_scaling(bridge, scaling_factor);
    EXPECT_EQ(0, result);
}

/* ============================================================================
 * Update Cycle Tests
 * ============================================================================ */

TEST_F(HomeostaticFepBridgeTest, Update) {
    int result = homeostatic_fep_bridge_update(bridge, 10);
    EXPECT_EQ(0, result);
}

TEST_F(HomeostaticFepBridgeTest, UpdateNull) {
    int result = homeostatic_fep_bridge_update(nullptr, 10);
    EXPECT_EQ(-1, result);
}

TEST_F(HomeostaticFepBridgeTest, UpdateMultipleTimes) {
    for (int i = 0; i < 10; i++) {
        int result = homeostatic_fep_bridge_update(bridge, 5);
        EXPECT_EQ(0, result);
    }
}

/* ============================================================================
 * State/Stats Tests
 * ============================================================================ */

TEST_F(HomeostaticFepBridgeTest, GetState) {
    homeostatic_fep_state_t state;
    int result = homeostatic_fep_bridge_get_state(bridge, &state);
    EXPECT_EQ(0, result);
}

TEST_F(HomeostaticFepBridgeTest, GetStateNull) {
    homeostatic_fep_state_t state;
    int result = homeostatic_fep_bridge_get_state(nullptr, &state);
    EXPECT_EQ(-1, result);

    result = homeostatic_fep_bridge_get_state(bridge, nullptr);
    EXPECT_EQ(-1, result);
}

TEST_F(HomeostaticFepBridgeTest, GetStats) {
    homeostatic_fep_stats_t stats;
    int result = homeostatic_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(0, result);
}

TEST_F(HomeostaticFepBridgeTest, GetStatsNull) {
    homeostatic_fep_stats_t stats;
    int result = homeostatic_fep_bridge_get_stats(nullptr, &stats);
    EXPECT_EQ(-1, result);

    result = homeostatic_fep_bridge_get_stats(bridge, nullptr);
    EXPECT_EQ(-1, result);
}

TEST_F(HomeostaticFepBridgeTest, StatsAfterOperations) {
    homeostatic_fep_apply_precision_normalization(bridge, 1.3f);
    homeostatic_fep_apply_fe_modulation(bridge, 2.5f);
    homeostatic_fep_report_scaling(bridge, 1.1f);
    homeostatic_fep_bridge_update(bridge, 10);

    homeostatic_fep_stats_t stats;
    int result = homeostatic_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(0, result);
    EXPECT_GT(stats.total_updates, 0u);
}

/* ============================================================================
 * Bio-Async Tests
 * ============================================================================ */

TEST_F(HomeostaticFepBridgeTest, ConnectBioAsync) {
    int result = homeostatic_fep_bridge_connect_bio_async(bridge);
    EXPECT_EQ(0, result);
}

TEST_F(HomeostaticFepBridgeTest, ConnectBioAsyncNull) {
    int result = homeostatic_fep_bridge_connect_bio_async(nullptr);
    EXPECT_EQ(-1, result);
}

TEST_F(HomeostaticFepBridgeTest, DisconnectBioAsync) {
    homeostatic_fep_bridge_connect_bio_async(bridge);
    int result = homeostatic_fep_bridge_disconnect_bio_async(bridge);
    EXPECT_EQ(0, result);
}

TEST_F(HomeostaticFepBridgeTest, DisconnectBioAsyncNull) {
    int result = homeostatic_fep_bridge_disconnect_bio_async(nullptr);
    EXPECT_EQ(-1, result);
}

TEST_F(HomeostaticFepBridgeTest, IsBioAsyncConnected) {
    bool connected = homeostatic_fep_bridge_is_bio_async_connected(bridge);
    EXPECT_FALSE(connected);  // Not connected initially
}

TEST_F(HomeostaticFepBridgeTest, IsBioAsyncConnectedNull) {
    bool connected = homeostatic_fep_bridge_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
