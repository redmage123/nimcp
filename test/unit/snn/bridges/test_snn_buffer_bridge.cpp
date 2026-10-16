/**
 * @file test_snn_buffer_bridge.cpp
 * @brief Unit tests for SNN-Buffer bridge
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
#include "snn/bridges/nimcp_snn_buffer_bridge.h"
#include "snn/nimcp_snn_config.h"
#include "utils/memory/nimcp_memory.h"

class SNNBufferBridgeTest : public ::testing::Test {
protected:
    snn_network_t* network;
    snn_buffer_bridge_t* bridge;

    void SetUp() override {
        snn_config_t config;
        snn_config_default(&config);
        config.n_inputs = 10;
        config.n_outputs = 5;
        config.n_populations = 2;
        network = snn_network_create(&config);
        ASSERT_NE(network, nullptr);

        bridge = nullptr;
    }

    void TearDown() override {
        if (bridge) snn_buffer_bridge_destroy(bridge);
        if (network) snn_network_destroy(network);
    }
};

/* Test 1: Config defaults */
TEST_F(SNNBufferBridgeTest, ConfigDefaults) {
    snn_buffer_config_t config;
    snn_buffer_config_default(&config);

    EXPECT_EQ(config.buffer_capacity, 1000);
    EXPECT_EQ(config.overflow, OVERFLOW_OVERWRITE);
    EXPECT_TRUE(config.enable_population_buffers);
    EXPECT_TRUE(config.enable_delay_lines);
}

/* Test 2: Bridge creation */
TEST_F(SNNBufferBridgeTest, BridgeCreation) {
    bridge = snn_buffer_bridge_create(nullptr, network);
    ASSERT_NE(bridge, nullptr);
    EXPECT_TRUE(bridge->connected);
    EXPECT_NE(bridge->buffers, nullptr);
    EXPECT_GT(bridge->n_buffers, 0);
}

/* Test 3: Custom config */
TEST_F(SNNBufferBridgeTest, CustomConfig) {
    snn_buffer_config_t config;
    snn_buffer_config_default(&config);
    config.buffer_capacity = 500;
    config.enable_spike_replay = false;

    bridge = snn_buffer_bridge_create(&config, network);
    ASSERT_NE(bridge, nullptr);
    EXPECT_EQ(bridge->config.buffer_capacity, 500);
    EXPECT_FALSE(bridge->config.enable_spike_replay);
}

/* Test 4: Null network fails */
TEST_F(SNNBufferBridgeTest, NullNetworkFails) {
    bridge = snn_buffer_bridge_create(nullptr, nullptr);
    EXPECT_EQ(bridge, nullptr);
}

/* Test 5: Process empty spikes */
TEST_F(SNNBufferBridgeTest, ProcessEmptySpikes) {
    bridge = snn_buffer_bridge_create(nullptr, network);
    ASSERT_NE(bridge, nullptr);

    snn_spike_t spikes_out[10];
    uint32_t n_out;
    int result = snn_buffer_bridge_process(bridge, nullptr, 0,
                                           spikes_out, 10, &n_out);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(n_out, 0);
}

/* Test 6: Process valid spikes */
TEST_F(SNNBufferBridgeTest, ProcessValidSpikes) {
    bridge = snn_buffer_bridge_create(nullptr, network);
    ASSERT_NE(bridge, nullptr);

    snn_spike_t spikes_in[3];
    spikes_in[0] = {1000, 0, 0};
    spikes_in[1] = {2000, 1, 0};
    spikes_in[2] = {3000, 2, 0};

    snn_spike_t spikes_out[10];
    uint32_t n_out;
    int result = snn_buffer_bridge_process(bridge, spikes_in, 3,
                                           spikes_out, 10, &n_out);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(bridge->stats.spikes_buffered, 3);
}

/* Test 7: Update state */
TEST_F(SNNBufferBridgeTest, UpdateState) {
    bridge = snn_buffer_bridge_create(nullptr, network);
    ASSERT_NE(bridge, nullptr);

    int result = snn_buffer_bridge_update(bridge, 1.0f);
    EXPECT_EQ(result, 0);
}

/* Test 8: Clear buffer */
TEST_F(SNNBufferBridgeTest, ClearBuffer) {
    bridge = snn_buffer_bridge_create(nullptr, network);
    ASSERT_NE(bridge, nullptr);

    int result = snn_buffer_bridge_clear_buffer(bridge, 0);
    EXPECT_EQ(result, 0);
}

/* Test 9: Clear all buffers */
TEST_F(SNNBufferBridgeTest, ClearAllBuffers) {
    bridge = snn_buffer_bridge_create(nullptr, network);
    ASSERT_NE(bridge, nullptr);

    snn_buffer_bridge_clear_all(bridge);
    /* No assertion, just verify no crash */
}

/* Test 10: Get window */
TEST_F(SNNBufferBridgeTest, GetWindow) {
    bridge = snn_buffer_bridge_create(nullptr, network);
    ASSERT_NE(bridge, nullptr);

    snn_spike_t spikes_out[10];
    uint32_t n_out;
    int result = snn_buffer_bridge_get_window(bridge, 0, 50.0f,
                                              spikes_out, 10, &n_out);
    EXPECT_EQ(result, 0);
}

/* Test 11: Get statistics */
TEST_F(SNNBufferBridgeTest, GetStatistics) {
    bridge = snn_buffer_bridge_create(nullptr, network);
    ASSERT_NE(bridge, nullptr);

    snn_buffer_stats_t stats;
    int result = snn_buffer_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
}

/* Test 12: Reset statistics */
TEST_F(SNNBufferBridgeTest, ResetStatistics) {
    bridge = snn_buffer_bridge_create(nullptr, network);
    ASSERT_NE(bridge, nullptr);

    bridge->stats.spikes_buffered = 100;
    snn_buffer_bridge_reset_stats(bridge);
    EXPECT_EQ(bridge->stats.spikes_buffered, 0);
}

/* Test 13: Get utilization */
TEST_F(SNNBufferBridgeTest, GetUtilization) {
    bridge = snn_buffer_bridge_create(nullptr, network);
    ASSERT_NE(bridge, nullptr);

    float util = snn_buffer_bridge_get_utilization(bridge, 0);
    EXPECT_GE(util, 0.0f);
    EXPECT_LE(util, 100.0f);
}

/* Test 14: Add delayed spike */
TEST_F(SNNBufferBridgeTest, AddDelayedSpike) {
    snn_buffer_config_t config;
    snn_buffer_config_default(&config);
    config.enable_delay_lines = true;

    bridge = snn_buffer_bridge_create(&config, network);
    ASSERT_NE(bridge, nullptr);

    snn_spike_t spike = {1000, 0, 0};
    int result = snn_buffer_bridge_add_delayed_spike(bridge, &spike, 5.0f);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(bridge->n_delayed_spikes, 1);
}

/* Test 15: Deliver delayed spikes */
TEST_F(SNNBufferBridgeTest, DeliverDelayedSpikes) {
    snn_buffer_config_t config;
    snn_buffer_config_default(&config);
    config.enable_delay_lines = true;

    bridge = snn_buffer_bridge_create(&config, network);
    ASSERT_NE(bridge, nullptr);

    snn_spike_t spike = {1000, 0, 0};
    snn_buffer_bridge_add_delayed_spike(bridge, &spike, 1.0f);

    snn_spike_t spikes_out[10];
    uint32_t n_delivered;
    int result = snn_buffer_bridge_deliver_delayed_spikes(bridge, 10000,
                                                          spikes_out, 10,
                                                          &n_delivered);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(n_delivered, 1);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
