/**
 * @file test_snn_love_bridge.cpp
 * @brief Unit tests for SNN-Love bridge
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "snn/bridges/nimcp_snn_love_bridge.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_config.h"

class SNNLoveBridgeTest : public ::testing::Test {
protected:
    snn_network_t* snn;
    snn_love_bridge_t* bridge;

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
        if (bridge) snn_love_bridge_destroy(bridge);
        if (snn) snn_network_destroy(snn);
    }
};

TEST_F(SNNLoveBridgeTest, DefaultConfigInitialization) {
    snn_love_config_t config;
    snn_love_config_default(&config);

    EXPECT_GT(config.attachment_threshold, 0.0f);
    EXPECT_GT(config.bonding_strength_gain, 0.0f);
    EXPECT_GT(config.sustained_activity_min_ms, 0.0f);
    EXPECT_GT(config.synchrony_threshold, 0.0f);
}

TEST_F(SNNLoveBridgeTest, BridgeCreation) {
    snn_love_config_t config;
    snn_love_config_default(&config);

    bridge = snn_love_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(SNNLoveBridgeTest, BridgeCreationNullParams) {
    snn_love_config_t config;
    snn_love_config_default(&config);

    EXPECT_EQ(snn_love_bridge_create(nullptr, snn, nullptr), nullptr);
    EXPECT_EQ(snn_love_bridge_create(&config, nullptr, nullptr), nullptr);
}

TEST_F(SNNLoveBridgeTest, BridgeDestruction) {
    snn_love_config_t config;
    snn_love_config_default(&config);

    bridge = snn_love_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_love_bridge_destroy(bridge);
    bridge = nullptr;
}

TEST_F(SNNLoveBridgeTest, BioAsyncConnectionStatus) {
    snn_love_config_t config;
    snn_love_config_default(&config);

    bridge = snn_love_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_FALSE(snn_love_bridge_is_bio_async_connected(bridge));
}

TEST_F(SNNLoveBridgeTest, BioAsyncConnect) {
    snn_love_config_t config;
    snn_love_config_default(&config);

    bridge = snn_love_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_love_bridge_connect_bio_async(bridge);
    EXPECT_TRUE(ret <= 0);  // 0 = success, negative = router not available
}

TEST_F(SNNLoveBridgeTest, BioAsyncDisconnect) {
    snn_love_config_t config;
    snn_love_config_default(&config);

    bridge = snn_love_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(snn_love_bridge_disconnect_bio_async(bridge), 0);
}

TEST_F(SNNLoveBridgeTest, GetAttachmentLevel) {
    snn_love_config_t config;
    snn_love_config_default(&config);

    bridge = snn_love_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float level = snn_love_get_attachment_level(bridge);
    EXPECT_GE(level, 0.0f);
    EXPECT_LE(level, 1.0f);
}

TEST_F(SNNLoveBridgeTest, GetBondingStrength) {
    snn_love_config_t config;
    snn_love_config_default(&config);

    bridge = snn_love_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float strength = snn_love_get_bonding_strength(bridge);
    EXPECT_GE(strength, 0.0f);
    EXPECT_LE(strength, 1.0f);
}

TEST_F(SNNLoveBridgeTest, IsSustained) {
    snn_love_config_t config;
    snn_love_config_default(&config);

    bridge = snn_love_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    bool sustained = snn_love_is_sustained(bridge);
    EXPECT_FALSE(sustained);  // Initially not sustained
}

TEST_F(SNNLoveBridgeTest, GetBridgeState) {
    snn_love_config_t config;
    snn_love_config_default(&config);

    bridge = snn_love_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_love_state_t state;
    int ret = snn_love_bridge_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(state.bonding_events_count, 0);
}

TEST_F(SNNLoveBridgeTest, GetStatistics) {
    snn_love_config_t config;
    snn_love_config_default(&config);

    bridge = snn_love_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    uint32_t sync_count, bonding_events;
    float avg_attachment;
    int ret = snn_love_get_stats(bridge, &sync_count, &bonding_events, &avg_attachment);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(sync_count, 0);
    EXPECT_EQ(bonding_events, 0);
}

TEST_F(SNNLoveBridgeTest, ResetStatistics) {
    snn_love_config_t config;
    snn_love_config_default(&config);

    bridge = snn_love_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_love_reset_stats(bridge);

    uint32_t sync_count;
    snn_love_get_stats(bridge, &sync_count, nullptr, nullptr);
    EXPECT_EQ(sync_count, 0);
}

TEST_F(SNNLoveBridgeTest, BridgeUpdate) {
    snn_love_config_t config;
    snn_love_config_default(&config);

    bridge = snn_love_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_love_bridge_update(bridge, 1.0f);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNLoveBridgeTest, DecodeFromSpikes) {
    snn_love_config_t config;
    snn_love_config_default(&config);

    bridge = snn_love_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float attachment, bonding;
    int ret = snn_love_decode_from_spikes(bridge, &attachment, &bonding);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNLoveBridgeTest, DetectSustainedActivity) {
    snn_love_config_t config;
    snn_love_config_default(&config);

    bridge = snn_love_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_sustained_activity_state_t sustained;
    int ret = snn_love_detect_sustained_activity(bridge, &sustained);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNLoveBridgeTest, DetectSynchrony) {
    snn_love_config_t config;
    snn_love_config_default(&config);
    config.enable_synchrony_detection = true;

    bridge = snn_love_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float synchrony;
    int ret = snn_love_detect_synchrony(bridge, &synchrony);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(synchrony, 0.0f);
    EXPECT_LE(synchrony, 1.0f);
}

TEST_F(SNNLoveBridgeTest, ModulatePopulations) {
    snn_love_config_t config;
    snn_love_config_default(&config);

    bridge = snn_love_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_love_modulate_populations(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNLoveBridgeTest, EncodeToSpikes) {
    snn_love_config_t config;
    snn_love_config_default(&config);

    bridge = snn_love_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_love_encode_to_spikes(bridge, 0.8f, 0.6f);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNLoveBridgeTest, NullBridgeReturnsDefaults) {
    EXPECT_FLOAT_EQ(snn_love_get_attachment_level(nullptr), 0.0f);
    EXPECT_FLOAT_EQ(snn_love_get_bonding_strength(nullptr), 0.0f);
    EXPECT_FALSE(snn_love_is_sustained(nullptr));
    EXPECT_FALSE(snn_love_bridge_is_bio_async_connected(nullptr));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
