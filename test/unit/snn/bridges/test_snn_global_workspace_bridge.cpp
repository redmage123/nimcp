/**
 * @file test_snn_global_workspace_bridge.cpp
 * @brief Unit tests for SNN-Global Workspace bridge
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "snn/bridges/nimcp_snn_global_workspace_bridge.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_config.h"

class SNNGlobalWorkspaceBridgeTest : public ::testing::Test {
protected:
    snn_network_t* snn;
    snn_global_workspace_bridge_t* bridge;

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
        if (bridge) snn_global_workspace_bridge_destroy(bridge);
        if (snn) snn_network_destroy(snn);
    }
};

TEST_F(SNNGlobalWorkspaceBridgeTest, DefaultConfigInitialization) {
    snn_global_workspace_config_t config;
    snn_global_workspace_config_default(&config);

    EXPECT_GT(config.competition_rate_threshold, 0.0f);
    EXPECT_GT(config.broadcast_encoding_gain, 0.0f);
    EXPECT_GT(config.ignition_rate_threshold, 0.0f);
    EXPECT_GT(config.update_interval_ms, 0.0f);
}

TEST_F(SNNGlobalWorkspaceBridgeTest, BridgeCreation) {
    snn_global_workspace_config_t config;
    snn_global_workspace_config_default(&config);

    bridge = snn_global_workspace_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(SNNGlobalWorkspaceBridgeTest, BridgeCreationNullParams) {
    snn_global_workspace_config_t config;
    snn_global_workspace_config_default(&config);

    EXPECT_EQ(snn_global_workspace_bridge_create(nullptr, snn, nullptr), nullptr);
    EXPECT_EQ(snn_global_workspace_bridge_create(&config, nullptr, nullptr), nullptr);
}

TEST_F(SNNGlobalWorkspaceBridgeTest, BridgeDestruction) {
    snn_global_workspace_config_t config;
    snn_global_workspace_config_default(&config);

    bridge = snn_global_workspace_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_global_workspace_bridge_destroy(bridge);
    bridge = nullptr;
}

TEST_F(SNNGlobalWorkspaceBridgeTest, BioAsyncConnectionStatus) {
    snn_global_workspace_config_t config;
    snn_global_workspace_config_default(&config);

    bridge = snn_global_workspace_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_FALSE(snn_global_workspace_bridge_is_bio_async_connected(bridge));
}

TEST_F(SNNGlobalWorkspaceBridgeTest, BioAsyncConnect) {
    snn_global_workspace_config_t config;
    snn_global_workspace_config_default(&config);

    bridge = snn_global_workspace_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_global_workspace_bridge_connect_bio_async(bridge);
    EXPECT_TRUE(ret <= 0);  // 0 = success, negative = router not available
}

TEST_F(SNNGlobalWorkspaceBridgeTest, BioAsyncDisconnect) {
    snn_global_workspace_config_t config;
    snn_global_workspace_config_default(&config);

    bridge = snn_global_workspace_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(snn_global_workspace_bridge_disconnect_bio_async(bridge), 0);
}

TEST_F(SNNGlobalWorkspaceBridgeTest, GetBroadcastRate) {
    snn_global_workspace_config_t config;
    snn_global_workspace_config_default(&config);

    bridge = snn_global_workspace_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float rate = snn_global_workspace_get_broadcast_rate(bridge);
    EXPECT_GE(rate, 0.0f);
}

TEST_F(SNNGlobalWorkspaceBridgeTest, GetCompetitionStrength) {
    snn_global_workspace_config_t config;
    snn_global_workspace_config_default(&config);

    bridge = snn_global_workspace_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float strength = snn_global_workspace_get_competition_strength(bridge);
    EXPECT_GE(strength, 0.0f);
}

TEST_F(SNNGlobalWorkspaceBridgeTest, IsBroadcasting) {
    snn_global_workspace_config_t config;
    snn_global_workspace_config_default(&config);

    bridge = snn_global_workspace_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    bool broadcasting = snn_global_workspace_is_broadcasting(bridge);
    EXPECT_FALSE(broadcasting);  // Initially not broadcasting
}

TEST_F(SNNGlobalWorkspaceBridgeTest, GetBridgeState) {
    snn_global_workspace_config_t config;
    snn_global_workspace_config_default(&config);

    bridge = snn_global_workspace_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_global_workspace_state_t state;
    int ret = snn_global_workspace_bridge_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(state.broadcast_count, 0);
}

TEST_F(SNNGlobalWorkspaceBridgeTest, GetBridgeStateNullOutput) {
    snn_global_workspace_config_t config;
    snn_global_workspace_config_default(&config);

    bridge = snn_global_workspace_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_global_workspace_bridge_get_state(bridge, nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(SNNGlobalWorkspaceBridgeTest, GetStatistics) {
    snn_global_workspace_config_t config;
    snn_global_workspace_config_default(&config);

    bridge = snn_global_workspace_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    uint32_t broadcast_count, wins;
    float avg_strength;
    int ret = snn_global_workspace_get_stats(bridge, &broadcast_count, &wins, &avg_strength);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(broadcast_count, 0);
    EXPECT_EQ(wins, 0);
}

TEST_F(SNNGlobalWorkspaceBridgeTest, ResetStatistics) {
    snn_global_workspace_config_t config;
    snn_global_workspace_config_default(&config);

    bridge = snn_global_workspace_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_global_workspace_reset_stats(bridge);

    uint32_t broadcast_count;
    snn_global_workspace_get_stats(bridge, &broadcast_count, nullptr, nullptr);
    EXPECT_EQ(broadcast_count, 0);
}

TEST_F(SNNGlobalWorkspaceBridgeTest, BridgeUpdate) {
    snn_global_workspace_config_t config;
    snn_global_workspace_config_default(&config);

    bridge = snn_global_workspace_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_global_workspace_bridge_update(bridge, 1.0f);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNGlobalWorkspaceBridgeTest, ProcessBroadcast) {
    snn_global_workspace_config_t config;
    snn_global_workspace_config_default(&config);

    bridge = snn_global_workspace_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_global_workspace_bridge_process_broadcast(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNGlobalWorkspaceBridgeTest, SubmitCompetition) {
    snn_global_workspace_config_t config;
    snn_global_workspace_config_default(&config);

    bridge = snn_global_workspace_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_global_workspace_bridge_submit_competition(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNGlobalWorkspaceBridgeTest, ComputeCompetitionStrength) {
    snn_global_workspace_config_t config;
    snn_global_workspace_config_default(&config);

    bridge = snn_global_workspace_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float strength = snn_global_workspace_compute_competition_strength(bridge, 50.0f);
    EXPECT_GE(strength, 0.0f);
}

TEST_F(SNNGlobalWorkspaceBridgeTest, NullBridgeReturnsDefaults) {
    EXPECT_FLOAT_EQ(snn_global_workspace_get_broadcast_rate(nullptr), 0.0f);
    EXPECT_FLOAT_EQ(snn_global_workspace_get_competition_strength(nullptr), 0.0f);
    EXPECT_FALSE(snn_global_workspace_is_broadcasting(nullptr));
    EXPECT_FALSE(snn_global_workspace_bridge_is_bio_async_connected(nullptr));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
