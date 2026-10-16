/**
 * @file test_snn_joy_bridge.cpp
 * @brief Unit tests for SNN-Joy bridge
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "snn/bridges/nimcp_snn_joy_bridge.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_config.h"

class SNNJoyBridgeTest : public ::testing::Test {
protected:
    snn_network_t* snn;
    snn_joy_bridge_t* bridge;

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
        if (bridge) snn_joy_bridge_destroy(bridge);
        if (snn) snn_network_destroy(snn);
    }
};

TEST_F(SNNJoyBridgeTest, DefaultConfigInitialization) {
    snn_joy_config_t config;
    snn_joy_config_default(&config);

    EXPECT_GT(config.joy_burst_threshold, 0.0f);
    EXPECT_GT(config.reward_prediction_gain, 0.0f);
    EXPECT_GT(config.burst_frequency_max, config.burst_frequency_min);
    EXPECT_GT(config.burst_duration_ms, 0.0f);
}

TEST_F(SNNJoyBridgeTest, BridgeCreation) {
    snn_joy_config_t config;
    snn_joy_config_default(&config);

    bridge = snn_joy_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(SNNJoyBridgeTest, BridgeCreationNullParams) {
    snn_joy_config_t config;
    snn_joy_config_default(&config);

    EXPECT_EQ(snn_joy_bridge_create(nullptr, snn, nullptr), nullptr);
    EXPECT_EQ(snn_joy_bridge_create(&config, nullptr, nullptr), nullptr);
}

TEST_F(SNNJoyBridgeTest, BridgeDestruction) {
    snn_joy_config_t config;
    snn_joy_config_default(&config);

    bridge = snn_joy_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_joy_bridge_destroy(bridge);
    bridge = nullptr;
}

TEST_F(SNNJoyBridgeTest, BioAsyncConnectionStatus) {
    snn_joy_config_t config;
    snn_joy_config_default(&config);

    bridge = snn_joy_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_FALSE(snn_joy_bridge_is_bio_async_connected(bridge));
}

TEST_F(SNNJoyBridgeTest, BioAsyncConnect) {
    snn_joy_config_t config;
    snn_joy_config_default(&config);

    bridge = snn_joy_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_joy_bridge_connect_bio_async(bridge);
    EXPECT_TRUE(ret <= 0);  // 0 = success, negative = router not available
}

TEST_F(SNNJoyBridgeTest, BioAsyncDisconnect) {
    snn_joy_config_t config;
    snn_joy_config_default(&config);

    bridge = snn_joy_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(snn_joy_bridge_disconnect_bio_async(bridge), 0);
}

TEST_F(SNNJoyBridgeTest, GetLevel) {
    snn_joy_config_t config;
    snn_joy_config_default(&config);

    bridge = snn_joy_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float level = snn_joy_get_level(bridge);
    EXPECT_GE(level, 0.0f);
    EXPECT_LE(level, 1.0f);
}

TEST_F(SNNJoyBridgeTest, GetRPE) {
    snn_joy_config_t config;
    snn_joy_config_default(&config);

    bridge = snn_joy_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float rpe = snn_joy_get_rpe(bridge);
    EXPECT_GE(rpe, -1.0f);
    EXPECT_LE(rpe, 1.0f);
}

TEST_F(SNNJoyBridgeTest, IsBursting) {
    snn_joy_config_t config;
    snn_joy_config_default(&config);

    bridge = snn_joy_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    bool bursting = snn_joy_is_bursting(bridge);
    EXPECT_FALSE(bursting);  // Initially not bursting
}

TEST_F(SNNJoyBridgeTest, GetBridgeState) {
    snn_joy_config_t config;
    snn_joy_config_default(&config);

    bridge = snn_joy_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_joy_state_t state;
    int ret = snn_joy_bridge_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(state.burst_count, 0);
}

TEST_F(SNNJoyBridgeTest, GetStatistics) {
    snn_joy_config_t config;
    snn_joy_config_default(&config);

    bridge = snn_joy_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    uint32_t sync_count, burst_count;
    float avg_joy;
    int ret = snn_joy_get_stats(bridge, &sync_count, &burst_count, &avg_joy);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(sync_count, 0);
    EXPECT_EQ(burst_count, 0);
}

TEST_F(SNNJoyBridgeTest, ResetStatistics) {
    snn_joy_config_t config;
    snn_joy_config_default(&config);

    bridge = snn_joy_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_joy_reset_stats(bridge);

    uint32_t sync_count;
    snn_joy_get_stats(bridge, &sync_count, nullptr, nullptr);
    EXPECT_EQ(sync_count, 0);
}

TEST_F(SNNJoyBridgeTest, BridgeUpdate) {
    snn_joy_config_t config;
    snn_joy_config_default(&config);

    bridge = snn_joy_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_joy_bridge_update(bridge, 1.0f);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNJoyBridgeTest, DecodeFromSpikes) {
    snn_joy_config_t config;
    snn_joy_config_default(&config);

    bridge = snn_joy_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float joy, rpe;
    int ret = snn_joy_decode_from_spikes(bridge, &joy, &rpe);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNJoyBridgeTest, DetectBurst) {
    snn_joy_config_t config;
    snn_joy_config_default(&config);

    bridge = snn_joy_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_burst_state_t burst;
    int ret = snn_joy_detect_burst(bridge, &burst);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNJoyBridgeTest, ComputeRPE) {
    snn_joy_config_t config;
    snn_joy_config_default(&config);
    config.enable_rpe_tracking = true;

    bridge = snn_joy_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float rpe;
    int ret = snn_joy_compute_rpe(bridge, 0.5f, 0.8f, &rpe);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(rpe, 0.0f);  // Actual > predicted = positive RPE
}

TEST_F(SNNJoyBridgeTest, ModulatePopulations) {
    snn_joy_config_t config;
    snn_joy_config_default(&config);

    bridge = snn_joy_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_joy_modulate_populations(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNJoyBridgeTest, EncodeToSpikes) {
    snn_joy_config_t config;
    snn_joy_config_default(&config);

    bridge = snn_joy_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_joy_encode_to_spikes(bridge, 0.7f, 0.3f);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNJoyBridgeTest, NullBridgeReturnsDefaults) {
    EXPECT_FLOAT_EQ(snn_joy_get_level(nullptr), 0.0f);
    EXPECT_FLOAT_EQ(snn_joy_get_rpe(nullptr), 0.0f);
    EXPECT_FALSE(snn_joy_is_bursting(nullptr));
    EXPECT_FALSE(snn_joy_bridge_is_bio_async_connected(nullptr));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
