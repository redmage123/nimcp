/**
 * @file test_snn_grief_bridge.cpp
 * @brief Unit tests for SNN-Grief bridge
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "snn/bridges/nimcp_snn_grief_bridge.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_config.h"

class SNNGriefBridgeTest : public ::testing::Test {
protected:
    snn_network_t* snn;
    snn_grief_bridge_t* bridge;

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
        if (bridge) snn_grief_bridge_destroy(bridge);
        if (snn) snn_network_destroy(snn);
    }
};

TEST_F(SNNGriefBridgeTest, DefaultConfigInitialization) {
    snn_grief_config_t config;
    snn_grief_config_default(&config);

    EXPECT_GT(config.grief_intensity_threshold, 0.0f);
    EXPECT_GT(config.recovery_rate, 0.0f);
    EXPECT_GE(config.slow_wave_min_freq, 0.0f);
    EXPECT_GT(config.slow_wave_max_freq, config.slow_wave_min_freq);
}

TEST_F(SNNGriefBridgeTest, BridgeCreation) {
    snn_grief_config_t config;
    snn_grief_config_default(&config);

    bridge = snn_grief_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(SNNGriefBridgeTest, BridgeCreationNullParams) {
    snn_grief_config_t config;
    snn_grief_config_default(&config);

    EXPECT_EQ(snn_grief_bridge_create(nullptr, snn, nullptr), nullptr);
    EXPECT_EQ(snn_grief_bridge_create(&config, nullptr, nullptr), nullptr);
}

TEST_F(SNNGriefBridgeTest, BridgeDestruction) {
    snn_grief_config_t config;
    snn_grief_config_default(&config);

    bridge = snn_grief_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_grief_bridge_destroy(bridge);
    bridge = nullptr;
}

TEST_F(SNNGriefBridgeTest, BioAsyncConnectionStatus) {
    snn_grief_config_t config;
    snn_grief_config_default(&config);

    bridge = snn_grief_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_FALSE(snn_grief_bridge_is_bio_async_connected(bridge));
}

TEST_F(SNNGriefBridgeTest, BioAsyncConnect) {
    snn_grief_config_t config;
    snn_grief_config_default(&config);

    bridge = snn_grief_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_grief_bridge_connect_bio_async(bridge);
    EXPECT_TRUE(ret <= 0);  // 0 = success, negative = router not available
}

TEST_F(SNNGriefBridgeTest, BioAsyncDisconnect) {
    snn_grief_config_t config;
    snn_grief_config_default(&config);

    bridge = snn_grief_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(snn_grief_bridge_disconnect_bio_async(bridge), 0);
}

TEST_F(SNNGriefBridgeTest, GetIntensity) {
    snn_grief_config_t config;
    snn_grief_config_default(&config);

    bridge = snn_grief_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float intensity = snn_grief_get_intensity(bridge);
    EXPECT_GE(intensity, 0.0f);
    EXPECT_LE(intensity, 1.0f);
}

TEST_F(SNNGriefBridgeTest, GetRecoveryProgress) {
    snn_grief_config_t config;
    snn_grief_config_default(&config);

    bridge = snn_grief_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float progress = snn_grief_get_recovery_progress(bridge);
    EXPECT_GE(progress, 0.0f);
    EXPECT_LE(progress, 1.0f);
}

TEST_F(SNNGriefBridgeTest, IsSlowWaveActive) {
    snn_grief_config_t config;
    snn_grief_config_default(&config);

    bridge = snn_grief_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    bool active = snn_grief_is_slow_wave_active(bridge);
    EXPECT_FALSE(active);  // Initially not active
}

TEST_F(SNNGriefBridgeTest, GetBridgeState) {
    snn_grief_config_t config;
    snn_grief_config_default(&config);

    bridge = snn_grief_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_grief_state_t state;
    int ret = snn_grief_bridge_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(state.sync_count, 0);
}

TEST_F(SNNGriefBridgeTest, GetStatistics) {
    snn_grief_config_t config;
    snn_grief_config_default(&config);

    bridge = snn_grief_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    uint32_t sync_count;
    float avg_grief, avg_recovery;
    int ret = snn_grief_get_stats(bridge, &sync_count, &avg_grief, &avg_recovery);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(sync_count, 0);
}

TEST_F(SNNGriefBridgeTest, ResetStatistics) {
    snn_grief_config_t config;
    snn_grief_config_default(&config);

    bridge = snn_grief_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_grief_reset_stats(bridge);

    uint32_t sync_count;
    snn_grief_get_stats(bridge, &sync_count, nullptr, nullptr);
    EXPECT_EQ(sync_count, 0);
}

TEST_F(SNNGriefBridgeTest, BridgeUpdate) {
    snn_grief_config_t config;
    snn_grief_config_default(&config);

    bridge = snn_grief_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_grief_bridge_update(bridge, 1.0f);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNGriefBridgeTest, DecodeFromSpikes) {
    snn_grief_config_t config;
    snn_grief_config_default(&config);

    bridge = snn_grief_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float grief, recovery;
    int ret = snn_grief_decode_from_spikes(bridge, &grief, &recovery);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNGriefBridgeTest, DetectSlowWave) {
    snn_grief_config_t config;
    snn_grief_config_default(&config);

    bridge = snn_grief_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_slow_wave_state_t slow_wave;
    int ret = snn_grief_detect_slow_wave(bridge, &slow_wave);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNGriefBridgeTest, DetectRumination) {
    snn_grief_config_t config;
    snn_grief_config_default(&config);
    config.enable_rumination_detection = true;

    bridge = snn_grief_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float rumination;
    int ret = snn_grief_detect_rumination(bridge, &rumination);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNGriefBridgeTest, ModulatePopulations) {
    snn_grief_config_t config;
    snn_grief_config_default(&config);

    bridge = snn_grief_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_grief_modulate_populations(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNGriefBridgeTest, EncodeToSpikes) {
    snn_grief_config_t config;
    snn_grief_config_default(&config);

    bridge = snn_grief_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_grief_encode_to_spikes(bridge, 0.5f, 0.3f);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNGriefBridgeTest, NullBridgeReturnsDefaults) {
    EXPECT_FLOAT_EQ(snn_grief_get_intensity(nullptr), 0.0f);
    EXPECT_FLOAT_EQ(snn_grief_get_recovery_progress(nullptr), 0.0f);
    EXPECT_FALSE(snn_grief_is_slow_wave_active(nullptr));
    EXPECT_FALSE(snn_grief_bridge_is_bio_async_connected(nullptr));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
