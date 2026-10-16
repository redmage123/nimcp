/**
 * @file test_snn_shadow_bridge.cpp
 * @brief Unit tests for SNN-Shadow bridge
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "snn/bridges/nimcp_snn_shadow_bridge.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_config.h"

class SNNShadowBridgeTest : public ::testing::Test {
protected:
    snn_network_t* snn;
    snn_shadow_bridge_t* bridge;

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
        if (bridge) snn_shadow_bridge_destroy(bridge);
        if (snn) snn_network_destroy(snn);
    }
};

TEST_F(SNNShadowBridgeTest, DefaultConfigInitialization) {
    snn_shadow_config_t config;
    snn_shadow_config_default(&config);

    EXPECT_GT(config.shadow_activation_threshold, 0.0f);
    EXPECT_GT(config.integration_rate, 0.0f);
    EXPECT_GT(config.background_frequency_max, config.background_frequency_min);
    EXPECT_GT(config.update_interval_ms, 0.0f);
}

TEST_F(SNNShadowBridgeTest, BridgeCreation) {
    snn_shadow_config_t config;
    snn_shadow_config_default(&config);

    bridge = snn_shadow_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(SNNShadowBridgeTest, BridgeCreationNullParams) {
    snn_shadow_config_t config;
    snn_shadow_config_default(&config);

    EXPECT_EQ(snn_shadow_bridge_create(nullptr, snn, nullptr), nullptr);
    EXPECT_EQ(snn_shadow_bridge_create(&config, nullptr, nullptr), nullptr);
}

TEST_F(SNNShadowBridgeTest, BridgeDestruction) {
    snn_shadow_config_t config;
    snn_shadow_config_default(&config);

    bridge = snn_shadow_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_shadow_bridge_destroy(bridge);
    bridge = nullptr;
}

TEST_F(SNNShadowBridgeTest, BioAsyncConnectionStatus) {
    snn_shadow_config_t config;
    snn_shadow_config_default(&config);

    bridge = snn_shadow_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_FALSE(snn_shadow_bridge_is_bio_async_connected(bridge));
}

TEST_F(SNNShadowBridgeTest, GetActivityLevel) {
    snn_shadow_config_t config;
    snn_shadow_config_default(&config);

    bridge = snn_shadow_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float level = snn_shadow_get_activity_level(bridge);
    EXPECT_GE(level, 0.0f);
    EXPECT_LE(level, 1.0f);
}

TEST_F(SNNShadowBridgeTest, GetDmnActivity) {
    snn_shadow_config_t config;
    snn_shadow_config_default(&config);

    bridge = snn_shadow_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float dmn = snn_shadow_get_dmn_activity(bridge);
    EXPECT_GE(dmn, 0.0f);
    EXPECT_LE(dmn, 1.0f);
}

TEST_F(SNNShadowBridgeTest, IsBackgroundActive) {
    snn_shadow_config_t config;
    snn_shadow_config_default(&config);

    bridge = snn_shadow_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    bool active = snn_shadow_is_background_active(bridge);
    EXPECT_FALSE(active);  // Initially not active
}

TEST_F(SNNShadowBridgeTest, GetBridgeState) {
    snn_shadow_config_t config;
    snn_shadow_config_default(&config);

    bridge = snn_shadow_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_shadow_state_t state;
    int ret = snn_shadow_bridge_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(state.sync_count, 0);
}

TEST_F(SNNShadowBridgeTest, GetStatistics) {
    snn_shadow_config_t config;
    snn_shadow_config_default(&config);

    bridge = snn_shadow_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    uint32_t sync_count, integration_events;
    float avg_shadow;
    int ret = snn_shadow_get_stats(bridge, &sync_count, &integration_events, &avg_shadow);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(sync_count, 0);
    EXPECT_EQ(integration_events, 0);
}

TEST_F(SNNShadowBridgeTest, ResetStatistics) {
    snn_shadow_config_t config;
    snn_shadow_config_default(&config);

    bridge = snn_shadow_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_shadow_reset_stats(bridge);

    uint32_t sync_count;
    snn_shadow_get_stats(bridge, &sync_count, nullptr, nullptr);
    EXPECT_EQ(sync_count, 0);
}

TEST_F(SNNShadowBridgeTest, BridgeUpdate) {
    snn_shadow_config_t config;
    snn_shadow_config_default(&config);

    bridge = snn_shadow_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_shadow_bridge_update(bridge, 1.0f);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNShadowBridgeTest, DecodeFromSpikes) {
    snn_shadow_config_t config;
    snn_shadow_config_default(&config);

    bridge = snn_shadow_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float shadow, dmn;
    int ret = snn_shadow_decode_from_spikes(bridge, &shadow, &dmn);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNShadowBridgeTest, DetectBackgroundPattern) {
    snn_shadow_config_t config;
    snn_shadow_config_default(&config);

    bridge = snn_shadow_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_background_pattern_state_t background;
    int ret = snn_shadow_detect_background_pattern(bridge, &background);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNShadowBridgeTest, DetectIntegrationEvent) {
    snn_shadow_config_t config;
    snn_shadow_config_default(&config);
    config.enable_integration_tracking = true;

    bridge = snn_shadow_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    bool integrated;
    int ret = snn_shadow_detect_integration_event(bridge, &integrated);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNShadowBridgeTest, ModulatePopulations) {
    snn_shadow_config_t config;
    snn_shadow_config_default(&config);

    bridge = snn_shadow_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_shadow_modulate_populations(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNShadowBridgeTest, EncodeToSpikes) {
    snn_shadow_config_t config;
    snn_shadow_config_default(&config);

    bridge = snn_shadow_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_shadow_encode_to_spikes(bridge, 0.3f, 0.5f);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNShadowBridgeTest, NullBridgeReturnsDefaults) {
    EXPECT_FLOAT_EQ(snn_shadow_get_activity_level(nullptr), 0.0f);
    EXPECT_FLOAT_EQ(snn_shadow_get_dmn_activity(nullptr), 0.0f);
    EXPECT_FALSE(snn_shadow_is_background_active(nullptr));
    EXPECT_FALSE(snn_shadow_bridge_is_bio_async_connected(nullptr));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
