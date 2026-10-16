/**
 * @file test_population_coding_fep_bridge.cpp
 * @brief Unit tests for Population Coding - FEP Bridge
 * @version 1.0.0
 * @date 2025-12-12
 */

#include <gtest/gtest.h>
#include "middleware/features/nimcp_population_coding_fep_bridge.h"

class PopulationCodingFepBridgeTest : public ::testing::Test {
protected:
    population_coding_fep_bridge_t* bridge = nullptr;

    void SetUp() override {
        population_coding_fep_config_t config;
        population_coding_fep_bridge_default_config(&config);
        bridge = population_coding_fep_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge) {
            population_coding_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(PopulationCodingFepBridgeTest, DefaultConfigInitialization) {
    population_coding_fep_config_t config;
    int result = population_coding_fep_bridge_default_config(&config);

    EXPECT_EQ(result, 0);
    EXPECT_TRUE(config.enable_precision_tuning);
    EXPECT_TRUE(config.enable_prediction_baseline);
    EXPECT_TRUE(config.enable_synchrony_confidence);
    EXPECT_TRUE(config.enable_sparsity_optimization);
    EXPECT_GT(config.tuning_sensitivity, 0.0f);
    EXPECT_GT(config.baseline_sensitivity, 0.0f);
}

TEST_F(PopulationCodingFepBridgeTest, DefaultConfigNullPointer) {
    int result = population_coding_fep_bridge_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(PopulationCodingFepBridgeTest, BridgeCreation) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(PopulationCodingFepBridgeTest, BridgeCreationWithNullConfig) {
    population_coding_fep_bridge_t* bridge_null = population_coding_fep_bridge_create(nullptr);
    EXPECT_NE(bridge_null, nullptr);
    population_coding_fep_bridge_destroy(bridge_null);
}

TEST_F(PopulationCodingFepBridgeTest, BridgeDestroyNullSafe) {
    population_coding_fep_bridge_destroy(nullptr);
    // Should not crash
    SUCCEED();
}

/* ============================================================================
 * Connection Tests
 * ============================================================================ */

TEST_F(PopulationCodingFepBridgeTest, ConnectEncoderSuccess) {
    population_coding_encoder_t encoder = {};
    int result = population_coding_fep_bridge_connect_encoder(bridge, encoder);
    EXPECT_EQ(result, 0);
}

TEST_F(PopulationCodingFepBridgeTest, ConnectEncoderNullBridge) {
    population_coding_encoder_t encoder = {};
    int result = population_coding_fep_bridge_connect_encoder(nullptr, encoder);
    EXPECT_EQ(result, -1);
}

TEST_F(PopulationCodingFepBridgeTest, ConnectFepSuccess) {
    fep_system_t fep = {};
    int result = population_coding_fep_bridge_connect_fep(bridge, &fep);
    EXPECT_EQ(result, 0);
}

TEST_F(PopulationCodingFepBridgeTest, ConnectFepNullBridge) {
    fep_system_t fep = {};
    int result = population_coding_fep_bridge_connect_fep(nullptr, &fep);
    EXPECT_EQ(result, -1);
}

TEST_F(PopulationCodingFepBridgeTest, DisconnectSuccess) {
    int result = population_coding_fep_bridge_disconnect(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(PopulationCodingFepBridgeTest, DisconnectNullBridge) {
    int result = population_coding_fep_bridge_disconnect(nullptr);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * FEP → Population Coding Direction Tests
 * ============================================================================ */

TEST_F(PopulationCodingFepBridgeTest, ApplyPrecisionTuningHighPrecision) {
    int result = population_coding_fep_apply_precision_tuning(bridge, 0.9f);
    EXPECT_EQ(result, 0);

    float tuning_width;
    population_coding_fep_get_tuning_width(bridge, &tuning_width);
    EXPECT_LT(tuning_width, FEP_PRECISION_MED_TUNING_WIDTH);
}

TEST_F(PopulationCodingFepBridgeTest, ApplyPrecisionTuningLowPrecision) {
    int result = population_coding_fep_apply_precision_tuning(bridge, 0.2f);
    EXPECT_EQ(result, 0);

    float tuning_width;
    population_coding_fep_get_tuning_width(bridge, &tuning_width);
    EXPECT_GT(tuning_width, FEP_PRECISION_MED_TUNING_WIDTH);
}

TEST_F(PopulationCodingFepBridgeTest, ApplyPrecisionTuningNullBridge) {
    int result = population_coding_fep_apply_precision_tuning(nullptr, 0.5f);
    EXPECT_EQ(result, -1);
}

TEST_F(PopulationCodingFepBridgeTest, SetBaselineActivation) {
    int result = population_coding_fep_set_baseline(bridge, 0.7f);
    EXPECT_EQ(result, 0);

    float baseline;
    population_coding_fep_get_baseline(bridge, &baseline);
    EXPECT_GT(baseline, 0.0f);
    EXPECT_LE(baseline, MAX_BASELINE_ACTIVATION);
}

TEST_F(PopulationCodingFepBridgeTest, SetBaselineNullBridge) {
    int result = population_coding_fep_set_baseline(nullptr, 0.5f);
    EXPECT_EQ(result, -1);
}

TEST_F(PopulationCodingFepBridgeTest, AdjustSynchronyThreshold) {
    float initial_pe = 0.2f;
    int result1 = population_coding_fep_adjust_synchrony_threshold(bridge, initial_pe);
    EXPECT_EQ(result1, 0);

    float high_pe = 0.8f;
    int result2 = population_coding_fep_adjust_synchrony_threshold(bridge, high_pe);
    EXPECT_EQ(result2, 0);
}

TEST_F(PopulationCodingFepBridgeTest, AdjustSynchronyThresholdNullBridge) {
    int result = population_coding_fep_adjust_synchrony_threshold(nullptr, 0.5f);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Population Coding → FEP Direction Tests
 * ============================================================================ */

TEST_F(PopulationCodingFepBridgeTest, ReportObservation) {
    vector3d_t vector = {0.7f, 0.3f, 0.0f};
    int result = population_coding_fep_report_observation(bridge, &vector);
    EXPECT_EQ(result, 0);
}

TEST_F(PopulationCodingFepBridgeTest, ReportObservationNullBridge) {
    vector3d_t vector = {0.5f, 0.5f, 0.0f};
    int result = population_coding_fep_report_observation(nullptr, &vector);
    EXPECT_EQ(result, -1);
}

TEST_F(PopulationCodingFepBridgeTest, ReportObservationNullVector) {
    int result = population_coding_fep_report_observation(bridge, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(PopulationCodingFepBridgeTest, UpdatePrecisionFromSynchronyHigh) {
    int result = population_coding_fep_update_precision_from_synchrony(bridge, 0.8f);
    EXPECT_EQ(result, 0);
}

TEST_F(PopulationCodingFepBridgeTest, UpdatePrecisionFromSynchronyLow) {
    int result = population_coding_fep_update_precision_from_synchrony(bridge, 0.2f);
    EXPECT_EQ(result, 0);
}

TEST_F(PopulationCodingFepBridgeTest, UpdatePrecisionFromSynchronyNullBridge) {
    int result = population_coding_fep_update_precision_from_synchrony(nullptr, 0.5f);
    EXPECT_EQ(result, -1);
}

TEST_F(PopulationCodingFepBridgeTest, ReportSparsity) {
    int result = population_coding_fep_report_sparsity(bridge, 0.1f);
    EXPECT_EQ(result, 0);
}

TEST_F(PopulationCodingFepBridgeTest, ReportSparsityNullBridge) {
    int result = population_coding_fep_report_sparsity(nullptr, 0.1f);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Update Cycle Tests
 * ============================================================================ */

TEST_F(PopulationCodingFepBridgeTest, BridgeUpdate) {
    int result = population_coding_fep_bridge_update(bridge, 100);
    EXPECT_EQ(result, 0);
}

TEST_F(PopulationCodingFepBridgeTest, BridgeUpdateNullBridge) {
    int result = population_coding_fep_bridge_update(nullptr, 100);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * State/Stats Tests
 * ============================================================================ */

TEST_F(PopulationCodingFepBridgeTest, GetState) {
    population_coding_fep_state_t state;
    int result = population_coding_fep_bridge_get_state(bridge, &state);
    EXPECT_EQ(result, 0);
}

TEST_F(PopulationCodingFepBridgeTest, GetStateNullBridge) {
    population_coding_fep_state_t state;
    int result = population_coding_fep_bridge_get_state(nullptr, &state);
    EXPECT_EQ(result, -1);
}

TEST_F(PopulationCodingFepBridgeTest, GetStateNullOutput) {
    int result = population_coding_fep_bridge_get_state(bridge, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(PopulationCodingFepBridgeTest, GetStats) {
    population_coding_fep_stats_t stats;
    int result = population_coding_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
}

TEST_F(PopulationCodingFepBridgeTest, GetStatsNullBridge) {
    population_coding_fep_stats_t stats;
    int result = population_coding_fep_bridge_get_stats(nullptr, &stats);
    EXPECT_EQ(result, -1);
}

TEST_F(PopulationCodingFepBridgeTest, GetTuningWidth) {
    float tuning_width;
    int result = population_coding_fep_get_tuning_width(bridge, &tuning_width);
    EXPECT_EQ(result, 0);
}

TEST_F(PopulationCodingFepBridgeTest, GetTuningWidthNullBridge) {
    float tuning_width;
    int result = population_coding_fep_get_tuning_width(nullptr, &tuning_width);
    EXPECT_EQ(result, -1);
}

TEST_F(PopulationCodingFepBridgeTest, GetBaseline) {
    float baseline;
    int result = population_coding_fep_get_baseline(bridge, &baseline);
    EXPECT_EQ(result, 0);
}

TEST_F(PopulationCodingFepBridgeTest, GetBaselineNullBridge) {
    float baseline;
    int result = population_coding_fep_get_baseline(nullptr, &baseline);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Bio-Async Tests
 * ============================================================================ */

TEST_F(PopulationCodingFepBridgeTest, ConnectBioAsync) {
    int result = population_coding_fep_bridge_connect_bio_async(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(PopulationCodingFepBridgeTest, ConnectBioAsyncNullBridge) {
    int result = population_coding_fep_bridge_connect_bio_async(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(PopulationCodingFepBridgeTest, DisconnectBioAsync) {
    population_coding_fep_bridge_connect_bio_async(bridge);
    int result = population_coding_fep_bridge_disconnect_bio_async(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(PopulationCodingFepBridgeTest, IsBioAsyncConnected) {
    bool connected = population_coding_fep_bridge_is_bio_async_connected(bridge);
    EXPECT_FALSE(connected);

    population_coding_fep_bridge_connect_bio_async(bridge);
    connected = population_coding_fep_bridge_is_bio_async_connected(bridge);
    // May or may not be true depending on bio-async availability
}

TEST_F(PopulationCodingFepBridgeTest, IsBioAsyncConnectedNullBridge) {
    bool connected = population_coding_fep_bridge_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
