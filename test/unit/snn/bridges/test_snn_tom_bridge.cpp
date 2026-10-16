/**
 * @file test_snn_tom_bridge.cpp
 * @brief Unit tests for SNN-Theory of Mind bridge
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "snn/bridges/nimcp_snn_tom_bridge.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_config.h"

class SNNTomBridgeTest : public ::testing::Test {
protected:
    snn_network_t* snn;
    snn_tom_bridge_t* bridge;

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
        if (bridge) snn_tom_bridge_destroy(bridge);
        if (snn) snn_network_destroy(snn);
    }
};

TEST_F(SNNTomBridgeTest, DefaultConfigInitialization) {
    snn_tom_config_t config;
    snn_tom_config_default(&config);

    EXPECT_GT(config.mentalizing_threshold, 0.0f);
    EXPECT_GT(config.perspective_shift_rate, 0.0f);
    EXPECT_GT(config.integration_time_window_ms, 0.0f);
    EXPECT_GT(config.update_interval_ms, 0.0f);
}

TEST_F(SNNTomBridgeTest, BridgeCreation) {
    snn_tom_config_t config;
    snn_tom_config_default(&config);

    bridge = snn_tom_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(SNNTomBridgeTest, BridgeCreationNullParams) {
    snn_tom_config_t config;
    snn_tom_config_default(&config);

    EXPECT_EQ(snn_tom_bridge_create(nullptr, snn, nullptr), nullptr);
    EXPECT_EQ(snn_tom_bridge_create(&config, nullptr, nullptr), nullptr);
}

TEST_F(SNNTomBridgeTest, BridgeDestruction) {
    snn_tom_config_t config;
    snn_tom_config_default(&config);

    bridge = snn_tom_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_tom_bridge_destroy(bridge);
    bridge = nullptr;
}

TEST_F(SNNTomBridgeTest, BioAsyncConnectionStatus) {
    snn_tom_config_t config;
    snn_tom_config_default(&config);

    bridge = snn_tom_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_FALSE(snn_tom_bridge_is_bio_async_connected(bridge));
}

TEST_F(SNNTomBridgeTest, BioAsyncConnect) {
    snn_tom_config_t config;
    snn_tom_config_default(&config);

    bridge = snn_tom_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_tom_bridge_connect_bio_async(bridge);
    EXPECT_TRUE(ret <= 0);  // 0 = success, negative = router not available
}

TEST_F(SNNTomBridgeTest, BioAsyncDisconnect) {
    snn_tom_config_t config;
    snn_tom_config_default(&config);

    bridge = snn_tom_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(snn_tom_bridge_disconnect_bio_async(bridge), 0);
}

TEST_F(SNNTomBridgeTest, GetMentalizingActivity) {
    snn_tom_config_t config;
    snn_tom_config_default(&config);

    bridge = snn_tom_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float activity = snn_tom_get_mentalizing_activity(bridge);
    EXPECT_GE(activity, 0.0f);
}

TEST_F(SNNTomBridgeTest, GetPerspectiveAccuracy) {
    snn_tom_config_t config;
    snn_tom_config_default(&config);

    bridge = snn_tom_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float accuracy = snn_tom_get_perspective_accuracy(bridge);
    EXPECT_GE(accuracy, 0.0f);
    EXPECT_LE(accuracy, 1.0f);
}

TEST_F(SNNTomBridgeTest, CheckMentalizingActive) {
    snn_tom_config_t config;
    snn_tom_config_default(&config);

    bridge = snn_tom_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    bool active = snn_tom_check_mentalizing_active(bridge);
    EXPECT_FALSE(active);  // Initially not active
}

TEST_F(SNNTomBridgeTest, GetBridgeState) {
    snn_tom_config_t config;
    snn_tom_config_default(&config);

    bridge = snn_tom_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_tom_state_t state;
    int ret = snn_tom_bridge_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(state.attribution_count, 0);
}

TEST_F(SNNTomBridgeTest, GetStatistics) {
    snn_tom_config_t config;
    snn_tom_config_default(&config);

    bridge = snn_tom_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    uint32_t attribution_count, mentalizing_detections;
    float avg_activity;
    int ret = snn_tom_get_stats(bridge, &attribution_count, &mentalizing_detections, &avg_activity);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(attribution_count, 0);
}

TEST_F(SNNTomBridgeTest, ResetStatistics) {
    snn_tom_config_t config;
    snn_tom_config_default(&config);

    bridge = snn_tom_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_tom_reset_stats(bridge);

    uint32_t attribution_count;
    snn_tom_get_stats(bridge, &attribution_count, nullptr, nullptr);
    EXPECT_EQ(attribution_count, 0);
}

TEST_F(SNNTomBridgeTest, BridgeUpdate) {
    snn_tom_config_t config;
    snn_tom_config_default(&config);

    bridge = snn_tom_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_tom_bridge_update(bridge, 1.0f);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNTomBridgeTest, ComputeMentalizing) {
    snn_tom_config_t config;
    snn_tom_config_default(&config);

    bridge = snn_tom_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float mentalizing = snn_tom_compute_mentalizing(bridge, 50.0f);
    EXPECT_GE(mentalizing, 0.0f);
}

TEST_F(SNNTomBridgeTest, UpdatePerspective) {
    snn_tom_config_t config;
    snn_tom_config_default(&config);

    bridge = snn_tom_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float spike_train[] = {1.0f, 0.0f, 1.0f, 0.0f, 1.0f};
    float accuracy;
    int ret = snn_tom_update_perspective(bridge, spike_train, 5, &accuracy);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNTomBridgeTest, NullBridgeReturnsDefaults) {
    EXPECT_FLOAT_EQ(snn_tom_get_mentalizing_activity(nullptr), 0.0f);
    EXPECT_FLOAT_EQ(snn_tom_get_perspective_accuracy(nullptr), 0.0f);
    EXPECT_FALSE(snn_tom_check_mentalizing_active(nullptr));
    EXPECT_FALSE(snn_tom_bridge_is_bio_async_connected(nullptr));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
