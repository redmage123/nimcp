/**
 * @file test_snn_meta_learning_bridge.cpp
 * @brief Unit tests for SNN-Meta Learning bridge
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "snn/bridges/nimcp_snn_meta_learning_bridge.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_config.h"

class SNNMetaLearningBridgeTest : public ::testing::Test {
protected:
    snn_network_t* snn;
    snn_meta_learning_bridge_t* bridge;

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
        if (bridge) snn_meta_learning_bridge_destroy(bridge);
        if (snn) snn_network_destroy(snn);
    }
};

TEST_F(SNNMetaLearningBridgeTest, DefaultConfigInitialization) {
    snn_meta_learning_config_t config;
    snn_meta_learning_config_default(&config);

    EXPECT_GT(config.meta_learning_rate, 0.0f);
    EXPECT_GT(config.adaptation_threshold, 0.0f);
    EXPECT_GT(config.integration_time_window_ms, 0.0f);
    EXPECT_GT(config.update_interval_ms, 0.0f);
}

TEST_F(SNNMetaLearningBridgeTest, BridgeCreation) {
    snn_meta_learning_config_t config;
    snn_meta_learning_config_default(&config);

    bridge = snn_meta_learning_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(SNNMetaLearningBridgeTest, BridgeCreationNullParams) {
    snn_meta_learning_config_t config;
    snn_meta_learning_config_default(&config);

    EXPECT_EQ(snn_meta_learning_bridge_create(nullptr, snn, nullptr), nullptr);
    EXPECT_EQ(snn_meta_learning_bridge_create(&config, nullptr, nullptr), nullptr);
}

TEST_F(SNNMetaLearningBridgeTest, BridgeDestruction) {
    snn_meta_learning_config_t config;
    snn_meta_learning_config_default(&config);

    bridge = snn_meta_learning_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_meta_learning_bridge_destroy(bridge);
    bridge = nullptr;
}

TEST_F(SNNMetaLearningBridgeTest, BioAsyncConnectionStatus) {
    snn_meta_learning_config_t config;
    snn_meta_learning_config_default(&config);

    bridge = snn_meta_learning_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_FALSE(snn_meta_learning_bridge_is_bio_async_connected(bridge));
}

TEST_F(SNNMetaLearningBridgeTest, BioAsyncConnect) {
    snn_meta_learning_config_t config;
    snn_meta_learning_config_default(&config);

    bridge = snn_meta_learning_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_meta_learning_bridge_connect_bio_async(bridge);
    EXPECT_TRUE(ret <= 0);  // 0 = success, negative = router not available
}

TEST_F(SNNMetaLearningBridgeTest, BioAsyncDisconnect) {
    snn_meta_learning_config_t config;
    snn_meta_learning_config_default(&config);

    bridge = snn_meta_learning_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(snn_meta_learning_bridge_disconnect_bio_async(bridge), 0);
}

TEST_F(SNNMetaLearningBridgeTest, GetAdaptationLevel) {
    snn_meta_learning_config_t config;
    snn_meta_learning_config_default(&config);

    bridge = snn_meta_learning_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float level = snn_meta_learning_get_adaptation_level(bridge);
    EXPECT_GE(level, 0.0f);
}

TEST_F(SNNMetaLearningBridgeTest, GetEfficiency) {
    snn_meta_learning_config_t config;
    snn_meta_learning_config_default(&config);

    bridge = snn_meta_learning_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float efficiency = snn_meta_learning_get_efficiency(bridge);
    EXPECT_GE(efficiency, 0.0f);
    EXPECT_LE(efficiency, 1.0f);
}

TEST_F(SNNMetaLearningBridgeTest, CheckAdaptationActive) {
    snn_meta_learning_config_t config;
    snn_meta_learning_config_default(&config);

    bridge = snn_meta_learning_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    bool active = snn_meta_learning_check_adaptation_active(bridge);
    EXPECT_FALSE(active);  // Initially not active
}

TEST_F(SNNMetaLearningBridgeTest, GetBridgeState) {
    snn_meta_learning_config_t config;
    snn_meta_learning_config_default(&config);

    bridge = snn_meta_learning_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_meta_learning_state_t state;
    int ret = snn_meta_learning_bridge_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(state.meta_updates_count, 0);
}

TEST_F(SNNMetaLearningBridgeTest, GetStatistics) {
    snn_meta_learning_config_t config;
    snn_meta_learning_config_default(&config);

    bridge = snn_meta_learning_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    uint32_t update_count, adaptation_detections;
    float avg_adaptation;
    int ret = snn_meta_learning_get_stats(bridge, &update_count, &adaptation_detections, &avg_adaptation);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(update_count, 0);
}

TEST_F(SNNMetaLearningBridgeTest, ResetStatistics) {
    snn_meta_learning_config_t config;
    snn_meta_learning_config_default(&config);

    bridge = snn_meta_learning_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_meta_learning_reset_stats(bridge);

    uint32_t update_count;
    snn_meta_learning_get_stats(bridge, &update_count, nullptr, nullptr);
    EXPECT_EQ(update_count, 0);
}

TEST_F(SNNMetaLearningBridgeTest, BridgeUpdate) {
    snn_meta_learning_config_t config;
    snn_meta_learning_config_default(&config);

    bridge = snn_meta_learning_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_meta_learning_bridge_update(bridge, 1.0f);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNMetaLearningBridgeTest, ComputeAdaptation) {
    snn_meta_learning_config_t config;
    snn_meta_learning_config_default(&config);

    bridge = snn_meta_learning_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float adaptation = snn_meta_learning_compute_adaptation(bridge, 50.0f);
    EXPECT_GE(adaptation, 0.0f);
}

TEST_F(SNNMetaLearningBridgeTest, UpdateEfficiency) {
    snn_meta_learning_config_t config;
    snn_meta_learning_config_default(&config);
    config.enable_efficiency_tracking = true;

    bridge = snn_meta_learning_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float spike_train[] = {1.0f, 0.0f, 1.0f, 0.0f, 1.0f};
    float efficiency;
    int ret = snn_meta_learning_update_efficiency(bridge, spike_train, 5, &efficiency);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNMetaLearningBridgeTest, NullBridgeReturnsDefaults) {
    EXPECT_FLOAT_EQ(snn_meta_learning_get_adaptation_level(nullptr), 0.0f);
    EXPECT_FLOAT_EQ(snn_meta_learning_get_efficiency(nullptr), 0.0f);
    EXPECT_FALSE(snn_meta_learning_check_adaptation_active(nullptr));
    EXPECT_FALSE(snn_meta_learning_bridge_is_bio_async_connected(nullptr));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
