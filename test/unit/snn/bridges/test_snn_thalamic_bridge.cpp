/**
 * @file test_snn_thalamic_bridge.cpp
 * @brief Unit tests for SNN-Thalamic bridge
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
#include "snn/bridges/nimcp_snn_thalamic_bridge.h"
#include "snn/nimcp_snn_config.h"
#include "utils/memory/nimcp_memory.h"

class SNNThalamicBridgeTest : public ::testing::Test {
protected:
    snn_network_t* network;
    thalamic_router_t* router;
    snn_thalamic_bridge_t* bridge;

    void SetUp() override {
        snn_config_t config;
        snn_config_default(&config);
        config.n_inputs = 10;
        config.n_outputs = 5;
        config.n_populations = 2;
        network = snn_network_create(&config);
        ASSERT_NE(network, nullptr);

        thalamic_router_config_t router_cfg = thalamic_router_default_config();
        router = thalamic_router_create(&router_cfg);
        ASSERT_NE(router, nullptr);

        bridge = nullptr;
    }

    void TearDown() override {
        if (bridge) snn_thalamic_bridge_destroy(bridge);
        if (router) thalamic_router_destroy(router);
        if (network) snn_network_destroy(network);
    }
};

/* Test 1: Config defaults */
TEST_F(SNNThalamicBridgeTest, ConfigDefaults) {
    snn_thalamic_config_t config;
    snn_thalamic_config_default(&config);

    EXPECT_EQ(config.default_mode, THALAMIC_MODE_ADAPTIVE);
    EXPECT_TRUE(config.enable_mode_switching);
    EXPECT_TRUE(config.enable_attention_gating);
    EXPECT_TRUE(config.enable_ct_loop);
}

/* Test 2: Bridge creation */
TEST_F(SNNThalamicBridgeTest, BridgeCreation) {
    bridge = snn_thalamic_bridge_create(nullptr, network, router);
    ASSERT_NE(bridge, nullptr);
    EXPECT_TRUE(bridge->connected);
    EXPECT_NE(bridge->neuron_modes, nullptr);
    EXPECT_NE(bridge->attention_weights, nullptr);
}

/* Test 3: Custom config */
TEST_F(SNNThalamicBridgeTest, CustomConfig) {
    snn_thalamic_config_t config;
    snn_thalamic_config_default(&config);
    config.default_mode = THALAMIC_MODE_BURST;
    config.enable_trn_inhibition = true;

    bridge = snn_thalamic_bridge_create(&config, network, router);
    ASSERT_NE(bridge, nullptr);
    EXPECT_EQ(bridge->config.default_mode, THALAMIC_MODE_BURST);
    EXPECT_TRUE(bridge->config.enable_trn_inhibition);
}

/* Test 4: Null network fails */
TEST_F(SNNThalamicBridgeTest, NullNetworkFails) {
    bridge = snn_thalamic_bridge_create(nullptr, nullptr, router);
    EXPECT_EQ(bridge, nullptr);
}

/* Test 5: Null router fails */
TEST_F(SNNThalamicBridgeTest, NullRouterFails) {
    bridge = snn_thalamic_bridge_create(nullptr, network, nullptr);
    EXPECT_EQ(bridge, nullptr);
}

/* Test 6: Process empty spikes */
TEST_F(SNNThalamicBridgeTest, ProcessEmptySpikes) {
    bridge = snn_thalamic_bridge_create(nullptr, network, router);
    ASSERT_NE(bridge, nullptr);

    snn_spike_t spikes_out[10];
    uint32_t n_out;
    int result = snn_thalamic_bridge_process(bridge, nullptr, 0,
                                             spikes_out, 10, &n_out);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(n_out, 0);
}

/* Test 7: Process valid spikes */
TEST_F(SNNThalamicBridgeTest, ProcessValidSpikes) {
    bridge = snn_thalamic_bridge_create(nullptr, network, router);
    ASSERT_NE(bridge, nullptr);

    snn_spike_t spikes_in[3];
    spikes_in[0] = {1000, 0, 0};
    spikes_in[1] = {2000, 1, 0};
    spikes_in[2] = {3000, 2, 0};

    snn_spike_t spikes_out[10];
    uint32_t n_out;
    int result = snn_thalamic_bridge_process(bridge, spikes_in, 3,
                                             spikes_out, 10, &n_out);
    EXPECT_EQ(result, 0);
    EXPECT_GT(bridge->stats.spikes_relayed, 0);
}

/* Test 8: Update state */
TEST_F(SNNThalamicBridgeTest, UpdateState) {
    bridge = snn_thalamic_bridge_create(nullptr, network, router);
    ASSERT_NE(bridge, nullptr);

    int result = snn_thalamic_bridge_update(bridge, 1.0f);
    EXPECT_EQ(result, 0);
}

/* Test 9: Set relay mode */
TEST_F(SNNThalamicBridgeTest, SetMode) {
    bridge = snn_thalamic_bridge_create(nullptr, network, router);
    ASSERT_NE(bridge, nullptr);

    int result = snn_thalamic_bridge_set_mode(bridge, 0, THALAMIC_MODE_BURST);
    EXPECT_EQ(result, 0);

    thalamic_relay_mode_t mode;
    result = snn_thalamic_bridge_get_mode(bridge, 0, &mode);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(mode, THALAMIC_MODE_BURST);
}

/* Test 10: Detect mode from timing */
TEST_F(SNNThalamicBridgeTest, DetectMode) {
    bridge = snn_thalamic_bridge_create(nullptr, network, router);
    ASSERT_NE(bridge, nullptr);

    /* First spike */
    thalamic_relay_mode_t mode1 = snn_thalamic_bridge_detect_mode(bridge, 0, 1000);
    EXPECT_EQ(mode1, THALAMIC_MODE_TONIC);

    /* Burst: ISI < 4ms = 4000us */
    thalamic_relay_mode_t mode2 = snn_thalamic_bridge_detect_mode(bridge, 0, 3000);
    EXPECT_EQ(mode2, THALAMIC_MODE_BURST);
}

/* Test 11: Set attention */
TEST_F(SNNThalamicBridgeTest, SetAttention) {
    bridge = snn_thalamic_bridge_create(nullptr, network, router);
    ASSERT_NE(bridge, nullptr);

    int result = snn_thalamic_bridge_set_attention(bridge, 0, 0.7f);
    EXPECT_EQ(result, 0);

    float attention;
    result = snn_thalamic_bridge_get_attention(bridge, 0, &attention);
    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(attention, 0.7f);
}

/* Test 12: CT feedback */
TEST_F(SNNThalamicBridgeTest, CTFeedback) {
    snn_thalamic_config_t config;
    snn_thalamic_config_default(&config);
    config.enable_ct_loop = true;

    bridge = snn_thalamic_bridge_create(&config, network, router);
    ASSERT_NE(bridge, nullptr);

    snn_spike_t spike = {1000, 0, 0};
    int result = snn_thalamic_bridge_ct_feedback(bridge, &spike);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(bridge->ct_buffer_count, 1);
}

/* Test 13: Process CT loop */
TEST_F(SNNThalamicBridgeTest, ProcessCTLoop) {
    snn_thalamic_config_t config;
    snn_thalamic_config_default(&config);
    config.enable_ct_loop = true;

    bridge = snn_thalamic_bridge_create(&config, network, router);
    ASSERT_NE(bridge, nullptr);

    int result = snn_thalamic_bridge_process_ct_loop(bridge, 10000);
    EXPECT_EQ(result, 0);
}

/* Test 14: Get statistics */
TEST_F(SNNThalamicBridgeTest, GetStatistics) {
    bridge = snn_thalamic_bridge_create(nullptr, network, router);
    ASSERT_NE(bridge, nullptr);

    snn_thalamic_stats_t stats;
    int result = snn_thalamic_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
}

/* Test 15: Reset statistics */
TEST_F(SNNThalamicBridgeTest, ResetStatistics) {
    bridge = snn_thalamic_bridge_create(nullptr, network, router);
    ASSERT_NE(bridge, nullptr);

    bridge->stats.spikes_relayed = 100;
    snn_thalamic_bridge_reset_stats(bridge);
    EXPECT_EQ(bridge->stats.spikes_relayed, 0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
