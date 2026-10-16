/**
 * @file test_snn_sleep_bridge.cpp
 * @brief Unit tests for SNN-Sleep bridge
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "snn/bridges/nimcp_snn_sleep_bridge.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_config.h"

class SNNSleepBridgeTest : public ::testing::Test {
protected:
    snn_network_t* snn;
    snn_sleep_bridge_t* bridge;

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
        if (bridge) snn_sleep_bridge_destroy(bridge);
        if (snn) snn_network_destroy(snn);
    }
};

TEST_F(SNNSleepBridgeTest, DefaultConfigInitialization) {
    snn_sleep_config_t config;
    snn_sleep_config_default(&config);

    EXPECT_GT(config.spindle_frequency, 0.0f);
    EXPECT_GT(config.spindle_bandwidth, 0.0f);
    EXPECT_GT(config.spindle_min_duration_ms, 0.0f);
    EXPECT_GT(config.slow_wave_max_freq, 0.0f);
    EXPECT_GT(config.update_interval_ms, 0.0f);
}

TEST_F(SNNSleepBridgeTest, BridgeCreation) {
    snn_sleep_config_t config;
    snn_sleep_config_default(&config);

    bridge = snn_sleep_bridge_create(&config, snn);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(SNNSleepBridgeTest, BridgeCreationNullParams) {
    snn_sleep_config_t config;
    snn_sleep_config_default(&config);

    EXPECT_EQ(snn_sleep_bridge_create(nullptr, snn), nullptr);
    EXPECT_EQ(snn_sleep_bridge_create(&config, nullptr), nullptr);
}

TEST_F(SNNSleepBridgeTest, BridgeDestruction) {
    snn_sleep_config_t config;
    snn_sleep_config_default(&config);

    bridge = snn_sleep_bridge_create(&config, snn);
    ASSERT_NE(bridge, nullptr);

    snn_sleep_bridge_destroy(bridge);
    bridge = nullptr;
}

TEST_F(SNNSleepBridgeTest, BioAsyncConnectionStatus) {
    snn_sleep_config_t config;
    snn_sleep_config_default(&config);

    bridge = snn_sleep_bridge_create(&config, snn);
    ASSERT_NE(bridge, nullptr);

    EXPECT_FALSE(snn_sleep_bridge_is_bio_async_connected(bridge));
}

TEST_F(SNNSleepBridgeTest, BioAsyncConnect) {
    snn_sleep_config_t config;
    snn_sleep_config_default(&config);

    bridge = snn_sleep_bridge_create(&config, snn);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_sleep_bridge_connect_bio_async(bridge);
    EXPECT_TRUE(ret <= 0);  // 0 = success, negative = router not available
}

TEST_F(SNNSleepBridgeTest, BioAsyncDisconnect) {
    snn_sleep_config_t config;
    snn_sleep_config_default(&config);

    bridge = snn_sleep_bridge_create(&config, snn);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(snn_sleep_bridge_disconnect_bio_async(bridge), 0);
}

TEST_F(SNNSleepBridgeTest, GetStage) {
    snn_sleep_config_t config;
    snn_sleep_config_default(&config);

    bridge = snn_sleep_bridge_create(&config, snn);
    ASSERT_NE(bridge, nullptr);

    snn_sleep_stage_t stage = snn_sleep_get_stage(bridge);
    EXPECT_GE(stage, SNN_SLEEP_WAKE);
    EXPECT_LE(stage, SNN_SLEEP_UNKNOWN);
}

TEST_F(SNNSleepBridgeTest, GetStageDuration) {
    snn_sleep_config_t config;
    snn_sleep_config_default(&config);

    bridge = snn_sleep_bridge_create(&config, snn);
    ASSERT_NE(bridge, nullptr);

    float duration = snn_sleep_get_stage_duration(bridge);
    EXPECT_GE(duration, 0.0f);
}

TEST_F(SNNSleepBridgeTest, GetSpindleCount) {
    snn_sleep_config_t config;
    snn_sleep_config_default(&config);

    bridge = snn_sleep_bridge_create(&config, snn);
    ASSERT_NE(bridge, nullptr);

    uint32_t count = snn_sleep_get_spindle_count(bridge);
    EXPECT_EQ(count, 0);
}

TEST_F(SNNSleepBridgeTest, GetSlowWaveCount) {
    snn_sleep_config_t config;
    snn_sleep_config_default(&config);

    bridge = snn_sleep_bridge_create(&config, snn);
    ASSERT_NE(bridge, nullptr);

    uint32_t count = snn_sleep_get_slow_wave_count(bridge);
    EXPECT_EQ(count, 0);
}

TEST_F(SNNSleepBridgeTest, GetRemActivity) {
    snn_sleep_config_t config;
    snn_sleep_config_default(&config);

    bridge = snn_sleep_bridge_create(&config, snn);
    ASSERT_NE(bridge, nullptr);

    float activity = snn_sleep_get_rem_activity(bridge);
    EXPECT_GE(activity, 0.0f);
    EXPECT_LE(activity, 1.0f);
}

TEST_F(SNNSleepBridgeTest, GetBridgeState) {
    snn_sleep_config_t config;
    snn_sleep_config_default(&config);

    bridge = snn_sleep_bridge_create(&config, snn);
    ASSERT_NE(bridge, nullptr);

    snn_sleep_state_t state;
    int ret = snn_sleep_bridge_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(state.spindle_count, 0);
    EXPECT_EQ(state.slow_wave_count, 0);
}

TEST_F(SNNSleepBridgeTest, GetArchitecture) {
    snn_sleep_config_t config;
    snn_sleep_config_default(&config);

    bridge = snn_sleep_bridge_create(&config, snn);
    ASSERT_NE(bridge, nullptr);

    float total_time;
    float time_in_stage[6];
    int ret = snn_sleep_get_architecture(bridge, &total_time, time_in_stage);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(total_time, 0.0f);
}

TEST_F(SNNSleepBridgeTest, ResetStatistics) {
    snn_sleep_config_t config;
    snn_sleep_config_default(&config);

    bridge = snn_sleep_bridge_create(&config, snn);
    ASSERT_NE(bridge, nullptr);

    snn_sleep_reset_stats(bridge);

    uint32_t spindle_count = snn_sleep_get_spindle_count(bridge);
    EXPECT_EQ(spindle_count, 0);
}

TEST_F(SNNSleepBridgeTest, BridgeUpdate) {
    snn_sleep_config_t config;
    snn_sleep_config_default(&config);

    bridge = snn_sleep_bridge_create(&config, snn);
    ASSERT_NE(bridge, nullptr);

    // Update may return error if cortical population not configured
    int ret = snn_sleep_bridge_update(bridge, 1.0f);
    EXPECT_TRUE(ret == 0 || ret == -5);  // 0 success or -5 (SNN_ERROR_INVALID_STATE)
}

TEST_F(SNNSleepBridgeTest, ClassifyStage) {
    snn_sleep_config_t config;
    snn_sleep_config_default(&config);

    bridge = snn_sleep_bridge_create(&config, snn);
    ASSERT_NE(bridge, nullptr);

    snn_sleep_stage_t stage = snn_sleep_classify_stage(bridge);
    EXPECT_GE(stage, SNN_SLEEP_WAKE);
    EXPECT_LE(stage, SNN_SLEEP_UNKNOWN);
}

TEST_F(SNNSleepBridgeTest, ConsolidateMemory) {
    snn_sleep_config_t config;
    snn_sleep_config_default(&config);

    bridge = snn_sleep_bridge_create(&config, snn);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_sleep_consolidate_memory(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNSleepBridgeTest, ReplaySequence) {
    snn_sleep_config_t config;
    snn_sleep_config_default(&config);
    config.enable_replay = true;

    bridge = snn_sleep_bridge_create(&config, snn);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_sleep_replay_sequence(bridge, 0);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNSleepBridgeTest, NullBridgeReturnsDefaults) {
    EXPECT_EQ(snn_sleep_get_stage(nullptr), SNN_SLEEP_UNKNOWN);
    EXPECT_FLOAT_EQ(snn_sleep_get_stage_duration(nullptr), 0.0f);
    EXPECT_EQ(snn_sleep_get_spindle_count(nullptr), 0);
    EXPECT_EQ(snn_sleep_get_slow_wave_count(nullptr), 0);
    EXPECT_FLOAT_EQ(snn_sleep_get_rem_activity(nullptr), 0.0f);
    EXPECT_FALSE(snn_sleep_bridge_is_bio_async_connected(nullptr));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
