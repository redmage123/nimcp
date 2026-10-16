/**
 * @file test_snn_curiosity_bridge.cpp
 * @brief Unit tests for SNN-Curiosity bridge
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "snn/bridges/nimcp_snn_curiosity_bridge.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_config.h"

class SNNCuriosityBridgeTest : public ::testing::Test {
protected:
    snn_network_t* snn;
    snn_curiosity_bridge_t* bridge;

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
        if (bridge) snn_curiosity_bridge_destroy(bridge);
        if (snn) snn_network_destroy(snn);
    }
};

TEST_F(SNNCuriosityBridgeTest, DefaultConfigInitialization) {
    snn_curiosity_config_t config;
    snn_curiosity_config_default(&config);

    EXPECT_GT(config.novelty_threshold, 0.0f);
    EXPECT_GT(config.exploration_drive_gain, 0.0f);
    EXPECT_GT(config.habituation_rate, 0.0f);
    EXPECT_GT(config.update_interval_ms, 0.0f);
}

TEST_F(SNNCuriosityBridgeTest, BridgeCreation) {
    snn_curiosity_config_t config;
    snn_curiosity_config_default(&config);

    bridge = snn_curiosity_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(SNNCuriosityBridgeTest, BridgeCreationNullParams) {
    snn_curiosity_config_t config;
    snn_curiosity_config_default(&config);

    EXPECT_EQ(snn_curiosity_bridge_create(nullptr, snn, nullptr), nullptr);
    EXPECT_EQ(snn_curiosity_bridge_create(&config, nullptr, nullptr), nullptr);
}

TEST_F(SNNCuriosityBridgeTest, BridgeDestruction) {
    snn_curiosity_config_t config;
    snn_curiosity_config_default(&config);

    bridge = snn_curiosity_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_curiosity_bridge_destroy(bridge);
    bridge = nullptr;
}

TEST_F(SNNCuriosityBridgeTest, BioAsyncConnectionStatus) {
    snn_curiosity_config_t config;
    snn_curiosity_config_default(&config);

    bridge = snn_curiosity_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_FALSE(snn_curiosity_bridge_is_bio_async_connected(bridge));
}

TEST_F(SNNCuriosityBridgeTest, BioAsyncConnect) {
    snn_curiosity_config_t config;
    snn_curiosity_config_default(&config);

    bridge = snn_curiosity_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_curiosity_bridge_connect_bio_async(bridge);
    EXPECT_TRUE(ret <= 0);  // 0 = success, negative = router not available
}

TEST_F(SNNCuriosityBridgeTest, BioAsyncDisconnect) {
    snn_curiosity_config_t config;
    snn_curiosity_config_default(&config);

    bridge = snn_curiosity_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(snn_curiosity_bridge_disconnect_bio_async(bridge), 0);
}

TEST_F(SNNCuriosityBridgeTest, GetLevel) {
    snn_curiosity_config_t config;
    snn_curiosity_config_default(&config);

    bridge = snn_curiosity_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float level = snn_curiosity_get_level(bridge);
    EXPECT_GE(level, 0.0f);
    EXPECT_LE(level, 1.0f);
}

TEST_F(SNNCuriosityBridgeTest, GetNoveltyResponse) {
    snn_curiosity_config_t config;
    snn_curiosity_config_default(&config);

    bridge = snn_curiosity_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float novelty = snn_curiosity_get_novelty_response(bridge);
    EXPECT_GE(novelty, 0.0f);
}

TEST_F(SNNCuriosityBridgeTest, GetExplorationCount) {
    snn_curiosity_config_t config;
    snn_curiosity_config_default(&config);

    bridge = snn_curiosity_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    uint32_t count = snn_curiosity_get_exploration_count(bridge);
    EXPECT_EQ(count, 0);  // Initially zero
}

TEST_F(SNNCuriosityBridgeTest, IsExploring) {
    snn_curiosity_config_t config;
    snn_curiosity_config_default(&config);

    bridge = snn_curiosity_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    bool exploring = snn_curiosity_is_exploring(bridge);
    EXPECT_FALSE(exploring);  // Initially not exploring
}

TEST_F(SNNCuriosityBridgeTest, IsNovel) {
    snn_curiosity_config_t config;
    snn_curiosity_config_default(&config);

    bridge = snn_curiosity_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    bool novel = snn_curiosity_is_novel(bridge);
    EXPECT_FALSE(novel);  // Initially not novel
}

TEST_F(SNNCuriosityBridgeTest, GetBridgeState) {
    snn_curiosity_config_t config;
    snn_curiosity_config_default(&config);

    bridge = snn_curiosity_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_curiosity_state_t state;
    int ret = snn_curiosity_bridge_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(state.exploration_count, 0);
}

TEST_F(SNNCuriosityBridgeTest, GetStatistics) {
    snn_curiosity_config_t config;
    snn_curiosity_config_default(&config);

    bridge = snn_curiosity_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    uint32_t novelty_events, exploration_count;
    float avg_novelty;
    int ret = snn_curiosity_get_stats(bridge, &novelty_events, &exploration_count, &avg_novelty);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(novelty_events, 0);
    EXPECT_EQ(exploration_count, 0);
}

TEST_F(SNNCuriosityBridgeTest, ResetStatistics) {
    snn_curiosity_config_t config;
    snn_curiosity_config_default(&config);

    bridge = snn_curiosity_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_curiosity_reset_stats(bridge);

    uint32_t novelty_events;
    snn_curiosity_get_stats(bridge, &novelty_events, nullptr, nullptr);
    EXPECT_EQ(novelty_events, 0);
}

TEST_F(SNNCuriosityBridgeTest, BridgeUpdate) {
    snn_curiosity_config_t config;
    snn_curiosity_config_default(&config);

    bridge = snn_curiosity_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_curiosity_bridge_update(bridge, 1.0f);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNCuriosityBridgeTest, EncodeNovelty) {
    snn_curiosity_config_t config;
    snn_curiosity_config_default(&config);

    bridge = snn_curiosity_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_curiosity_bridge_encode_novelty(bridge, 0.8f);
    EXPECT_TRUE(ret <= 0);  // 0 = success, negative = population not configured
}

TEST_F(SNNCuriosityBridgeTest, TriggerExploration) {
    snn_curiosity_config_t config;
    snn_curiosity_config_default(&config);

    bridge = snn_curiosity_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_curiosity_bridge_trigger_exploration(bridge);
    EXPECT_TRUE(ret <= 0);  // 0 = success, negative = population not configured
}

TEST_F(SNNCuriosityBridgeTest, ApplyHabituation) {
    snn_curiosity_config_t config;
    snn_curiosity_config_default(&config);

    bridge = snn_curiosity_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_curiosity_bridge_apply_habituation(bridge, 10.0f);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNCuriosityBridgeTest, ComputeCuriosityLevel) {
    snn_curiosity_config_t config;
    snn_curiosity_config_default(&config);

    bridge = snn_curiosity_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float level = snn_curiosity_compute_curiosity_level(bridge, 50.0f, 0.2f);
    EXPECT_GE(level, 0.0f);
}

TEST_F(SNNCuriosityBridgeTest, NullBridgeReturnsDefaults) {
    EXPECT_FLOAT_EQ(snn_curiosity_get_level(nullptr), 0.0f);
    EXPECT_FLOAT_EQ(snn_curiosity_get_novelty_response(nullptr), 0.0f);
    EXPECT_EQ(snn_curiosity_get_exploration_count(nullptr), 0);
    EXPECT_FALSE(snn_curiosity_is_exploring(nullptr));
    EXPECT_FALSE(snn_curiosity_is_novel(nullptr));
    EXPECT_FALSE(snn_curiosity_bridge_is_bio_async_connected(nullptr));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
