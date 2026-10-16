/**
 * @file test_snn_self_model_bridge.cpp
 * @brief Unit tests for SNN-Self Model bridge
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "snn/bridges/nimcp_snn_self_model_bridge.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_config.h"

class SNNSelfModelBridgeTest : public ::testing::Test {
protected:
    snn_network_t* snn;
    snn_self_model_bridge_t* bridge;

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
        if (bridge) snn_self_model_bridge_destroy(bridge);
        if (snn) snn_network_destroy(snn);
    }
};

TEST_F(SNNSelfModelBridgeTest, DefaultConfigInitialization) {
    snn_self_model_config_t config;
    snn_self_model_config_default(&config);

    EXPECT_GT(config.self_reference_threshold, 0.0f);
    EXPECT_GT(config.identity_stability_gain, 0.0f);
    EXPECT_GT(config.integration_time_window_ms, 0.0f);
    EXPECT_GT(config.update_interval_ms, 0.0f);
}

TEST_F(SNNSelfModelBridgeTest, BridgeCreation) {
    snn_self_model_config_t config;
    snn_self_model_config_default(&config);

    bridge = snn_self_model_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(SNNSelfModelBridgeTest, BridgeCreationNullParams) {
    snn_self_model_config_t config;
    snn_self_model_config_default(&config);

    EXPECT_EQ(snn_self_model_bridge_create(nullptr, snn, nullptr), nullptr);
    EXPECT_EQ(snn_self_model_bridge_create(&config, nullptr, nullptr), nullptr);
}

TEST_F(SNNSelfModelBridgeTest, BridgeDestruction) {
    snn_self_model_config_t config;
    snn_self_model_config_default(&config);

    bridge = snn_self_model_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_self_model_bridge_destroy(bridge);
    bridge = nullptr;
}

TEST_F(SNNSelfModelBridgeTest, BioAsyncConnectionStatus) {
    snn_self_model_config_t config;
    snn_self_model_config_default(&config);

    bridge = snn_self_model_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_FALSE(snn_self_model_bridge_is_bio_async_connected(bridge));
}

TEST_F(SNNSelfModelBridgeTest, BioAsyncConnect) {
    snn_self_model_config_t config;
    snn_self_model_config_default(&config);

    bridge = snn_self_model_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_self_model_bridge_connect_bio_async(bridge);
    EXPECT_TRUE(ret <= 0);  // 0 = success, negative = router not available
}

TEST_F(SNNSelfModelBridgeTest, BioAsyncDisconnect) {
    snn_self_model_config_t config;
    snn_self_model_config_default(&config);

    bridge = snn_self_model_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(snn_self_model_bridge_disconnect_bio_async(bridge), 0);
}

TEST_F(SNNSelfModelBridgeTest, GetCoherence) {
    snn_self_model_config_t config;
    snn_self_model_config_default(&config);

    bridge = snn_self_model_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float coherence = snn_self_model_get_coherence(bridge);
    EXPECT_GE(coherence, 0.0f);
    EXPECT_LE(coherence, 1.0f);
}

TEST_F(SNNSelfModelBridgeTest, GetStability) {
    snn_self_model_config_t config;
    snn_self_model_config_default(&config);

    bridge = snn_self_model_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float stability = snn_self_model_get_stability(bridge);
    EXPECT_GE(stability, 0.0f);
    EXPECT_LE(stability, 1.0f);
}

TEST_F(SNNSelfModelBridgeTest, CheckCoherentSelf) {
    snn_self_model_config_t config;
    snn_self_model_config_default(&config);

    bridge = snn_self_model_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    bool coherent = snn_self_model_check_coherent_self(bridge);
    EXPECT_FALSE(coherent);  // Initially not coherent
}

TEST_F(SNNSelfModelBridgeTest, GetBridgeState) {
    snn_self_model_config_t config;
    snn_self_model_config_default(&config);

    bridge = snn_self_model_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_self_model_state_t state;
    int ret = snn_self_model_bridge_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(state.self_reference_count, 0);
}

TEST_F(SNNSelfModelBridgeTest, GetBridgeStateNullOutput) {
    snn_self_model_config_t config;
    snn_self_model_config_default(&config);

    bridge = snn_self_model_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_self_model_bridge_get_state(bridge, nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(SNNSelfModelBridgeTest, GetStatistics) {
    snn_self_model_config_t config;
    snn_self_model_config_default(&config);

    bridge = snn_self_model_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    uint32_t reference_count, coherent_detections;
    float avg_coherence;
    int ret = snn_self_model_get_stats(bridge, &reference_count, &coherent_detections, &avg_coherence);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(reference_count, 0);
    EXPECT_EQ(coherent_detections, 0);
}

TEST_F(SNNSelfModelBridgeTest, ResetStatistics) {
    snn_self_model_config_t config;
    snn_self_model_config_default(&config);

    bridge = snn_self_model_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    snn_self_model_reset_stats(bridge);

    uint32_t reference_count;
    snn_self_model_get_stats(bridge, &reference_count, nullptr, nullptr);
    EXPECT_EQ(reference_count, 0);
}

TEST_F(SNNSelfModelBridgeTest, BridgeUpdate) {
    snn_self_model_config_t config;
    snn_self_model_config_default(&config);

    bridge = snn_self_model_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_self_model_bridge_update(bridge, 1.0f);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNSelfModelBridgeTest, ComputeCoherence) {
    snn_self_model_config_t config;
    snn_self_model_config_default(&config);

    bridge = snn_self_model_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float coherence = snn_self_model_compute_coherence(bridge, 50.0f);
    EXPECT_GE(coherence, 0.0f);
    EXPECT_LE(coherence, 1.0f);
}

TEST_F(SNNSelfModelBridgeTest, UpdateStability) {
    snn_self_model_config_t config;
    snn_self_model_config_default(&config);

    bridge = snn_self_model_bridge_create(&config, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    float spike_train[] = {1.0f, 0.0f, 1.0f, 0.0f, 1.0f};
    float stability;
    int ret = snn_self_model_update_stability(bridge, spike_train, 5, &stability);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(stability, 0.0f);
}

TEST_F(SNNSelfModelBridgeTest, NullBridgeReturnsDefaults) {
    EXPECT_FLOAT_EQ(snn_self_model_get_coherence(nullptr), 0.0f);
    EXPECT_FLOAT_EQ(snn_self_model_get_stability(nullptr), 0.0f);
    EXPECT_FALSE(snn_self_model_check_coherent_self(nullptr));
    EXPECT_FALSE(snn_self_model_bridge_is_bio_async_connected(nullptr));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
