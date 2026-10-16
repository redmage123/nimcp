/**
 * @file test_snn_free_energy_bridge.cpp
 * @brief Unit tests for SNN-Free Energy bridge
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "snn/bridges/nimcp_snn_free_energy_bridge.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_config.h"

class SNNFreeEnergyBridgeTest : public ::testing::Test {
protected:
    snn_network_t* snn;
    snn_free_energy_bridge_t* bridge;

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
        if (bridge) snn_free_energy_bridge_destroy(bridge);
        if (snn) snn_network_destroy(snn);
    }
};

TEST_F(SNNFreeEnergyBridgeTest, DefaultConfigInitialization) {
    snn_free_energy_config_t config;
    snn_free_energy_config_default(&config);

    EXPECT_GT(config.prediction_error_gain, 0.0f);
    EXPECT_GT(config.surprise_threshold, 0.0f);
    EXPECT_GT(config.precision_weighting, 0.0f);
    EXPECT_GT(config.update_interval_ms, 0.0f);
}

TEST_F(SNNFreeEnergyBridgeTest, BridgeCreation) {
    snn_free_energy_config_t config;
    snn_free_energy_config_default(&config);

    bridge = snn_free_energy_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(SNNFreeEnergyBridgeTest, BridgeCreationNullParams) {
    snn_free_energy_config_t config;
    snn_free_energy_config_default(&config);

    EXPECT_EQ(snn_free_energy_bridge_create(nullptr, snn, nullptr), nullptr);
    EXPECT_EQ(snn_free_energy_bridge_create(&config, nullptr, nullptr), nullptr);
}

TEST_F(SNNFreeEnergyBridgeTest, BridgeDestruction) {
    snn_free_energy_config_t config;
    snn_free_energy_config_default(&config);

    bridge = snn_free_energy_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_free_energy_bridge_destroy(bridge);
    bridge = nullptr;
}

TEST_F(SNNFreeEnergyBridgeTest, BioAsyncConnectionStatus) {
    snn_free_energy_config_t config;
    snn_free_energy_config_default(&config);

    bridge = snn_free_energy_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_FALSE(snn_free_energy_bridge_is_bio_async_connected(bridge));
}

TEST_F(SNNFreeEnergyBridgeTest, BioAsyncConnect) {
    snn_free_energy_config_t config;
    snn_free_energy_config_default(&config);

    bridge = snn_free_energy_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_free_energy_bridge_connect_bio_async(bridge);
    EXPECT_TRUE(ret <= 0);  // 0 = success, negative = router not available
}

TEST_F(SNNFreeEnergyBridgeTest, BioAsyncDisconnect) {
    snn_free_energy_config_t config;
    snn_free_energy_config_default(&config);

    bridge = snn_free_energy_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(snn_free_energy_bridge_disconnect_bio_async(bridge), 0);
}

TEST_F(SNNFreeEnergyBridgeTest, GetFreeEnergyEstimate) {
    snn_free_energy_config_t config;
    snn_free_energy_config_default(&config);

    bridge = snn_free_energy_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float fe = snn_free_energy_get_estimate(bridge);
    EXPECT_GE(fe, 0.0f);
}

TEST_F(SNNFreeEnergyBridgeTest, GetPredictionError) {
    snn_free_energy_config_t config;
    snn_free_energy_config_default(&config);

    bridge = snn_free_energy_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float error = snn_free_energy_get_prediction_error(bridge);
    EXPECT_GE(error, 0.0f);
}

TEST_F(SNNFreeEnergyBridgeTest, GetSurpriseLevel) {
    snn_free_energy_config_t config;
    snn_free_energy_config_default(&config);

    bridge = snn_free_energy_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float surprise = snn_free_energy_get_surprise_level(bridge);
    EXPECT_GE(surprise, 0.0f);
}

TEST_F(SNNFreeEnergyBridgeTest, IsSurprised) {
    snn_free_energy_config_t config;
    snn_free_energy_config_default(&config);

    bridge = snn_free_energy_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    bool surprised = snn_free_energy_is_surprised(bridge);
    EXPECT_FALSE(surprised);  // Initially not surprised
}

TEST_F(SNNFreeEnergyBridgeTest, GetBridgeState) {
    snn_free_energy_config_t config;
    snn_free_energy_config_default(&config);

    bridge = snn_free_energy_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_free_energy_state_t state;
    int ret = snn_free_energy_bridge_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(state.prediction_error_count, 0);
}

TEST_F(SNNFreeEnergyBridgeTest, GetBridgeStateNullOutput) {
    snn_free_energy_config_t config;
    snn_free_energy_config_default(&config);

    bridge = snn_free_energy_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_free_energy_bridge_get_state(bridge, nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(SNNFreeEnergyBridgeTest, GetStatistics) {
    snn_free_energy_config_t config;
    snn_free_energy_config_default(&config);

    bridge = snn_free_energy_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    uint32_t error_count, surprise_events;
    float avg_fe;
    int ret = snn_free_energy_get_stats(bridge, &error_count, &surprise_events, &avg_fe);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(error_count, 0);
    EXPECT_EQ(surprise_events, 0);
}

TEST_F(SNNFreeEnergyBridgeTest, ResetStatistics) {
    snn_free_energy_config_t config;
    snn_free_energy_config_default(&config);

    bridge = snn_free_energy_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_free_energy_reset_stats(bridge);

    uint32_t error_count;
    snn_free_energy_get_stats(bridge, &error_count, nullptr, nullptr);
    EXPECT_EQ(error_count, 0);
}

TEST_F(SNNFreeEnergyBridgeTest, BridgeUpdate) {
    snn_free_energy_config_t config;
    snn_free_energy_config_default(&config);

    bridge = snn_free_energy_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_free_energy_bridge_update(bridge, 1.0f);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNFreeEnergyBridgeTest, EncodePredictionError) {
    snn_free_energy_config_t config;
    snn_free_energy_config_default(&config);

    bridge = snn_free_energy_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_free_energy_bridge_encode_prediction_error(bridge, 0.5f);
    EXPECT_TRUE(ret <= 0);  // 0 = success, negative = population not configured
}

TEST_F(SNNFreeEnergyBridgeTest, UpdatePrecision) {
    snn_free_energy_config_t config;
    snn_free_energy_config_default(&config);

    bridge = snn_free_energy_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_free_energy_bridge_update_precision(bridge, 0.8f);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNFreeEnergyBridgeTest, ComputeFreeEnergy) {
    snn_free_energy_config_t config;
    snn_free_energy_config_default(&config);

    bridge = snn_free_energy_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float fe = snn_free_energy_compute_free_energy(bridge, 50.0f, 20.0f);
    EXPECT_GE(fe, 0.0f);
}

TEST_F(SNNFreeEnergyBridgeTest, NullBridgeReturnsDefaults) {
    EXPECT_FLOAT_EQ(snn_free_energy_get_estimate(nullptr), 0.0f);
    EXPECT_FLOAT_EQ(snn_free_energy_get_prediction_error(nullptr), 0.0f);
    EXPECT_FLOAT_EQ(snn_free_energy_get_surprise_level(nullptr), 0.0f);
    EXPECT_FALSE(snn_free_energy_is_surprised(nullptr));
    EXPECT_FALSE(snn_free_energy_bridge_is_bio_async_connected(nullptr));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
