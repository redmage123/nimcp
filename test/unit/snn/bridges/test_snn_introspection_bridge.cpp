/**
 * @file test_snn_introspection_bridge.cpp
 * @brief Unit tests for SNN-Introspection bridge
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "snn/bridges/nimcp_snn_introspection_bridge.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_config.h"

class SNNIntrospectionBridgeTest : public ::testing::Test {
protected:
    snn_network_t* snn;
    snn_introspection_bridge_t* bridge;

    void SetUp() override {
        snn_config_t snn_config;
        snn_config_default(&snn_config);
        snn_config.n_inputs = 10;
        snn_config.n_outputs = 10;
        snn_config.n_populations = 2;
        snn_config.dt = 0.1f;
        snn = snn_network_create(&snn_config);
        ASSERT_NE(snn, nullptr);

        bridge = nullptr;
    }

    void TearDown() override {
        if (bridge) snn_introspection_bridge_destroy(bridge);
        if (snn) snn_network_destroy(snn);
    }
};

TEST_F(SNNIntrospectionBridgeTest, DefaultConfigInitialization) {
    snn_introspection_config_t config;
    snn_introspection_config_default(&config);

    EXPECT_GT(config.phi_rate_min, 0.0f);
    EXPECT_GT(config.phi_rate_max, config.phi_rate_min);
    EXPECT_GT(config.consciousness_threshold, 0.0f);
    EXPECT_GT(config.integration_time_window_ms, 0.0f);
    EXPECT_GT(config.update_interval_ms, 0.0f);
}

TEST_F(SNNIntrospectionBridgeTest, BridgeCreation) {
    snn_introspection_config_t config;
    snn_introspection_config_default(&config);

    bridge = snn_introspection_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(SNNIntrospectionBridgeTest, BridgeCreationNullParams) {
    snn_introspection_config_t config;
    snn_introspection_config_default(&config);

    EXPECT_EQ(snn_introspection_bridge_create(nullptr, snn, nullptr), nullptr);
    EXPECT_EQ(snn_introspection_bridge_create(&config, nullptr, nullptr), nullptr);
}

TEST_F(SNNIntrospectionBridgeTest, BridgeDestruction) {
    snn_introspection_config_t config;
    snn_introspection_config_default(&config);

    bridge = snn_introspection_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_introspection_bridge_destroy(bridge);
    bridge = nullptr;
}

TEST_F(SNNIntrospectionBridgeTest, BioAsyncConnectionStatus) {
    snn_introspection_config_t config;
    snn_introspection_config_default(&config);

    bridge = snn_introspection_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_FALSE(snn_introspection_bridge_is_bio_async_connected(bridge));
}

TEST_F(SNNIntrospectionBridgeTest, BioAsyncConnect) {
    snn_introspection_config_t config;
    snn_introspection_config_default(&config);

    bridge = snn_introspection_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_introspection_bridge_connect_bio_async(bridge);
    EXPECT_TRUE(ret <= 0);  // 0 = success, negative = router not available
}

TEST_F(SNNIntrospectionBridgeTest, BioAsyncDisconnect) {
    snn_introspection_config_t config;
    snn_introspection_config_default(&config);

    bridge = snn_introspection_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(snn_introspection_bridge_disconnect_bio_async(bridge), 0);
}

TEST_F(SNNIntrospectionBridgeTest, GetPhi) {
    snn_introspection_config_t config;
    snn_introspection_config_default(&config);

    bridge = snn_introspection_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float phi = snn_introspection_get_phi(bridge);
    EXPECT_GE(phi, 0.0f);
}

TEST_F(SNNIntrospectionBridgeTest, GetUncertainty) {
    snn_introspection_config_t config;
    snn_introspection_config_default(&config);

    bridge = snn_introspection_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float uncertainty = snn_introspection_get_uncertainty(bridge);
    EXPECT_GE(uncertainty, 0.0f);
    EXPECT_LE(uncertainty, 1.0f);
}

TEST_F(SNNIntrospectionBridgeTest, CheckConsciousness) {
    snn_introspection_config_t config;
    snn_introspection_config_default(&config);

    bridge = snn_introspection_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    bool conscious = snn_introspection_check_consciousness(bridge);
    // Initially not conscious
    EXPECT_FALSE(conscious);
}

TEST_F(SNNIntrospectionBridgeTest, GetBridgeState) {
    snn_introspection_config_t config;
    snn_introspection_config_default(&config);

    bridge = snn_introspection_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_introspection_state_t state;
    int ret = snn_introspection_bridge_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNIntrospectionBridgeTest, GetBridgeStateNullOutput) {
    snn_introspection_config_t config;
    snn_introspection_config_default(&config);

    bridge = snn_introspection_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_introspection_bridge_get_state(bridge, nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(SNNIntrospectionBridgeTest, GetStatistics) {
    snn_introspection_config_t config;
    snn_introspection_config_default(&config);

    bridge = snn_introspection_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    uint32_t pattern_matches, consciousness_detections;
    float avg_phi;
    int ret = snn_introspection_get_stats(bridge, &pattern_matches, &consciousness_detections, &avg_phi);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(pattern_matches, 0);
    EXPECT_EQ(consciousness_detections, 0);
}

TEST_F(SNNIntrospectionBridgeTest, ResetStatistics) {
    snn_introspection_config_t config;
    snn_introspection_config_default(&config);

    bridge = snn_introspection_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_introspection_reset_stats(bridge);

    uint32_t pattern_matches;
    int ret = snn_introspection_get_stats(bridge, &pattern_matches, nullptr, nullptr);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(pattern_matches, 0);
}

TEST_F(SNNIntrospectionBridgeTest, BridgeUpdate) {
    snn_introspection_config_t config;
    snn_introspection_config_default(&config);

    bridge = snn_introspection_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_introspection_bridge_update(bridge, 1.0f);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNIntrospectionBridgeTest, BridgeUpdateNull) {
    int ret = snn_introspection_bridge_update(nullptr, 1.0f);
    EXPECT_NE(ret, 0);
}

TEST_F(SNNIntrospectionBridgeTest, EstimatePhi) {
    snn_introspection_config_t config;
    snn_introspection_config_default(&config);

    bridge = snn_introspection_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float phi = snn_introspection_estimate_phi(bridge, 50.0f);
    EXPECT_GE(phi, 0.0f);
}

TEST_F(SNNIntrospectionBridgeTest, DetectPatterns) {
    snn_introspection_config_t config;
    snn_introspection_config_default(&config);
    config.enable_pattern_detection = true;

    bridge = snn_introspection_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float spike_train[] = {1.0f, 0.0f, 1.0f, 0.0f, 1.0f};
    float coherence;
    int ret = snn_introspection_detect_patterns(bridge, spike_train, 5, &coherence);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNIntrospectionBridgeTest, NullBridgeReturnsDefaults) {
    EXPECT_FLOAT_EQ(snn_introspection_get_phi(nullptr), 0.0f);
    EXPECT_FLOAT_EQ(snn_introspection_get_uncertainty(nullptr), 0.0f);
    EXPECT_FALSE(snn_introspection_check_consciousness(nullptr));
    EXPECT_FALSE(snn_introspection_bridge_is_bio_async_connected(nullptr));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
