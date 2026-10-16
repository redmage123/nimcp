/**
 * @file test_feature_extractor_fep_bridge.cpp
 * @brief Unit tests for Feature Extractor - FEP Bridge
 * @version 1.0.0
 * @date 2025-12-12
 */

#include <gtest/gtest.h>
#include "middleware/features/nimcp_feature_extractor_fep_bridge.h"

class FeatureExtractorFepBridgeTest : public ::testing::Test {
protected:
    feature_extractor_fep_bridge_t* bridge = nullptr;

    void SetUp() override {
        feature_extractor_fep_config_t config;
        feature_extractor_fep_bridge_default_config(&config);
        bridge = feature_extractor_fep_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge) {
            feature_extractor_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(FeatureExtractorFepBridgeTest, DefaultConfigInitialization) {
    feature_extractor_fep_config_t config;
    int result = feature_extractor_fep_bridge_default_config(&config);

    EXPECT_EQ(result, 0);
    EXPECT_TRUE(config.enable_hierarchical_selection);
    EXPECT_TRUE(config.enable_precision_gating);
    EXPECT_TRUE(config.enable_prediction_gating);
    EXPECT_TRUE(config.enable_entropy_feedback);
    EXPECT_TRUE(config.enable_oscillation_state);
    EXPECT_GT(config.hierarchy_sensitivity, 0.0f);
}

TEST_F(FeatureExtractorFepBridgeTest, DefaultConfigNullPointer) {
    int result = feature_extractor_fep_bridge_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(FeatureExtractorFepBridgeTest, BridgeCreation) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(FeatureExtractorFepBridgeTest, BridgeCreationWithNullConfig) {
    feature_extractor_fep_bridge_t* bridge_null = feature_extractor_fep_bridge_create(nullptr);
    EXPECT_NE(bridge_null, nullptr);
    feature_extractor_fep_bridge_destroy(bridge_null);
}

TEST_F(FeatureExtractorFepBridgeTest, BridgeDestroyNullSafe) {
    feature_extractor_fep_bridge_destroy(nullptr);
    // Should not crash
    SUCCEED();
}

/* ============================================================================
 * Connection Tests
 * ============================================================================ */

TEST_F(FeatureExtractorFepBridgeTest, ConnectExtractorSuccess) {
    feature_extractor_t extractor = {};
    int result = feature_extractor_fep_bridge_connect_extractor(bridge, extractor);
    EXPECT_EQ(result, 0);
}

TEST_F(FeatureExtractorFepBridgeTest, ConnectExtractorNullBridge) {
    feature_extractor_t extractor = {};
    int result = feature_extractor_fep_bridge_connect_extractor(nullptr, extractor);
    EXPECT_EQ(result, -1);
}

TEST_F(FeatureExtractorFepBridgeTest, ConnectFepSuccess) {
    fep_system_t fep = {};
    int result = feature_extractor_fep_bridge_connect_fep(bridge, &fep);
    EXPECT_EQ(result, 0);
}

TEST_F(FeatureExtractorFepBridgeTest, ConnectFepNullBridge) {
    fep_system_t fep = {};
    int result = feature_extractor_fep_bridge_connect_fep(nullptr, &fep);
    EXPECT_EQ(result, -1);
}

TEST_F(FeatureExtractorFepBridgeTest, DisconnectSuccess) {
    int result = feature_extractor_fep_bridge_disconnect(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(FeatureExtractorFepBridgeTest, DisconnectNullBridge) {
    int result = feature_extractor_fep_bridge_disconnect(nullptr);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * FEP → Feature Extraction Direction Tests
 * ============================================================================ */

TEST_F(FeatureExtractorFepBridgeTest, SelectHierarchicalFeaturesLevel0) {
    int result = feature_extractor_fep_select_hierarchical_features(bridge, FEP_HIERARCHY_LEVEL_RATE);
    EXPECT_EQ(result, 0);
}

TEST_F(FeatureExtractorFepBridgeTest, SelectHierarchicalFeaturesLevel1) {
    int result = feature_extractor_fep_select_hierarchical_features(bridge, FEP_HIERARCHY_LEVEL_TEMPORAL);
    EXPECT_EQ(result, 0);
}

TEST_F(FeatureExtractorFepBridgeTest, SelectHierarchicalFeaturesLevel2) {
    int result = feature_extractor_fep_select_hierarchical_features(bridge, FEP_HIERARCHY_LEVEL_POPULATION);
    EXPECT_EQ(result, 0);
}

TEST_F(FeatureExtractorFepBridgeTest, SelectHierarchicalFeaturesLevel3) {
    int result = feature_extractor_fep_select_hierarchical_features(bridge, FEP_HIERARCHY_LEVEL_OSCILLATION);
    EXPECT_EQ(result, 0);
}

TEST_F(FeatureExtractorFepBridgeTest, SelectHierarchicalFeaturesNullBridge) {
    int result = feature_extractor_fep_select_hierarchical_features(nullptr, FEP_HIERARCHY_LEVEL_RATE);
    EXPECT_EQ(result, -1);
}

TEST_F(FeatureExtractorFepBridgeTest, GateByPrecisionHigh) {
    int result = feature_extractor_fep_gate_by_precision(bridge, 0.9f);
    EXPECT_EQ(result, 0);
}

TEST_F(FeatureExtractorFepBridgeTest, GateByPrecisionMedium) {
    int result = feature_extractor_fep_gate_by_precision(bridge, 0.5f);
    EXPECT_EQ(result, 0);
}

TEST_F(FeatureExtractorFepBridgeTest, GateByPrecisionLow) {
    int result = feature_extractor_fep_gate_by_precision(bridge, 0.2f);
    EXPECT_EQ(result, 0);
}

TEST_F(FeatureExtractorFepBridgeTest, GateByPrecisionNullBridge) {
    int result = feature_extractor_fep_gate_by_precision(nullptr, 0.5f);
    EXPECT_EQ(result, -1);
}

TEST_F(FeatureExtractorFepBridgeTest, SetExpectedFeatures) {
    int result = feature_extractor_fep_set_expected_features(bridge, 10.0f);
    EXPECT_EQ(result, 0);
}

TEST_F(FeatureExtractorFepBridgeTest, SetExpectedFeaturesNullBridge) {
    int result = feature_extractor_fep_set_expected_features(nullptr, 10.0f);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Feature Extraction → FEP Direction Tests
 * ============================================================================ */

TEST_F(FeatureExtractorFepBridgeTest, ReportFeatures) {
    middleware_features_t features = {};
    features.mean_firing_rate = 12.5f;
    features.synchrony_index = 0.7f;
    int result = feature_extractor_fep_report_features(bridge, &features);
    EXPECT_EQ(result, 0);
}

TEST_F(FeatureExtractorFepBridgeTest, ReportFeaturesNullBridge) {
    middleware_features_t features = {};
    int result = feature_extractor_fep_report_features(nullptr, &features);
    EXPECT_EQ(result, -1);
}

TEST_F(FeatureExtractorFepBridgeTest, ReportFeaturesNullFeatures) {
    int result = feature_extractor_fep_report_features(bridge, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(FeatureExtractorFepBridgeTest, UpdateUncertaintyFromEntropyLow) {
    int result = feature_extractor_fep_update_uncertainty_from_entropy(bridge, 1.2f);
    EXPECT_EQ(result, 0);
}

TEST_F(FeatureExtractorFepBridgeTest, UpdateUncertaintyFromEntropyHigh) {
    int result = feature_extractor_fep_update_uncertainty_from_entropy(bridge, 4.0f);
    EXPECT_EQ(result, 0);
}

TEST_F(FeatureExtractorFepBridgeTest, UpdateUncertaintyFromEntropyNullBridge) {
    int result = feature_extractor_fep_update_uncertainty_from_entropy(nullptr, 2.0f);
    EXPECT_EQ(result, -1);
}

TEST_F(FeatureExtractorFepBridgeTest, InferStateFromOscillationsGamma) {
    int result = feature_extractor_fep_infer_state_from_oscillations(bridge, 0.8f, 0.3f, 0.2f);
    EXPECT_EQ(result, 0);
}

TEST_F(FeatureExtractorFepBridgeTest, InferStateFromOscillationsAlpha) {
    int result = feature_extractor_fep_infer_state_from_oscillations(bridge, 0.2f, 0.3f, 0.7f);
    EXPECT_EQ(result, 0);
}

TEST_F(FeatureExtractorFepBridgeTest, InferStateFromOscillationsBeta) {
    int result = feature_extractor_fep_infer_state_from_oscillations(bridge, 0.3f, 0.8f, 0.2f);
    EXPECT_EQ(result, 0);
}

TEST_F(FeatureExtractorFepBridgeTest, InferStateFromOscillationsNullBridge) {
    int result = feature_extractor_fep_infer_state_from_oscillations(nullptr, 0.5f, 0.5f, 0.5f);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Update Cycle Tests
 * ============================================================================ */

TEST_F(FeatureExtractorFepBridgeTest, BridgeUpdate) {
    int result = feature_extractor_fep_bridge_update(bridge, 100);
    EXPECT_EQ(result, 0);
}

TEST_F(FeatureExtractorFepBridgeTest, BridgeUpdateNullBridge) {
    int result = feature_extractor_fep_bridge_update(nullptr, 100);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * State/Stats Tests
 * ============================================================================ */

TEST_F(FeatureExtractorFepBridgeTest, GetState) {
    feature_extractor_fep_state_t state;
    int result = feature_extractor_fep_bridge_get_state(bridge, &state);
    EXPECT_EQ(result, 0);
}

TEST_F(FeatureExtractorFepBridgeTest, GetStateNullBridge) {
    feature_extractor_fep_state_t state;
    int result = feature_extractor_fep_bridge_get_state(nullptr, &state);
    EXPECT_EQ(result, -1);
}

TEST_F(FeatureExtractorFepBridgeTest, GetStateNullOutput) {
    int result = feature_extractor_fep_bridge_get_state(bridge, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(FeatureExtractorFepBridgeTest, GetStats) {
    feature_extractor_fep_stats_t stats;
    int result = feature_extractor_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
}

TEST_F(FeatureExtractorFepBridgeTest, GetStatsNullBridge) {
    feature_extractor_fep_stats_t stats;
    int result = feature_extractor_fep_bridge_get_stats(nullptr, &stats);
    EXPECT_EQ(result, -1);
}

TEST_F(FeatureExtractorFepBridgeTest, GetStatsNullOutput) {
    feature_extractor_fep_stats_t stats;
    int result = feature_extractor_fep_bridge_get_stats(bridge, nullptr);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Bio-Async Tests
 * ============================================================================ */

TEST_F(FeatureExtractorFepBridgeTest, ConnectBioAsync) {
    int result = feature_extractor_fep_bridge_connect_bio_async(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(FeatureExtractorFepBridgeTest, ConnectBioAsyncNullBridge) {
    int result = feature_extractor_fep_bridge_connect_bio_async(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(FeatureExtractorFepBridgeTest, DisconnectBioAsync) {
    feature_extractor_fep_bridge_connect_bio_async(bridge);
    int result = feature_extractor_fep_bridge_disconnect_bio_async(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(FeatureExtractorFepBridgeTest, DisconnectBioAsyncNullBridge) {
    int result = feature_extractor_fep_bridge_disconnect_bio_async(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(FeatureExtractorFepBridgeTest, IsBioAsyncConnected) {
    bool connected = feature_extractor_fep_bridge_is_bio_async_connected(bridge);
    EXPECT_FALSE(connected);

    feature_extractor_fep_bridge_connect_bio_async(bridge);
    connected = feature_extractor_fep_bridge_is_bio_async_connected(bridge);
    // May or may not be true depending on bio-async availability
}

TEST_F(FeatureExtractorFepBridgeTest, IsBioAsyncConnectedNullBridge) {
    bool connected = feature_extractor_fep_bridge_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
