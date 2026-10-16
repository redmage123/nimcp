/**
 * @file test_snn_routing_bridge.cpp
 * @brief Unit tests for SNN-Routing bridge
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
#include "snn/bridges/nimcp_snn_routing_bridge.h"
#include "snn/nimcp_snn_config.h"
#include "utils/memory/nimcp_memory.h"

class SNNRoutingBridgeTest : public ::testing::Test {
protected:
    snn_network_t* network;
    thalamic_router_t* router;
    snn_routing_bridge_t* bridge;

    void SetUp() override {
        /* Create minimal SNN network */
        snn_config_t config;
        snn_config_default(&config);
        config.n_inputs = 10;
        config.n_outputs = 5;
        config.n_populations = 2;
        network = snn_network_create(&config);
        ASSERT_NE(network, nullptr);

        /* Create thalamic router */
        thalamic_router_config_t router_cfg = thalamic_router_default_config();
        router = thalamic_router_create(&router_cfg);
        ASSERT_NE(router, nullptr);

        bridge = nullptr;
    }

    void TearDown() override {
        if (bridge) snn_routing_bridge_destroy(bridge);
        if (router) thalamic_router_destroy(router);
        if (network) snn_network_destroy(network);
    }
};

/* Test 1: Config defaults */
TEST_F(SNNRoutingBridgeTest, ConfigDefaults) {
    snn_routing_config_t config;
    snn_routing_config_default(&config);

    EXPECT_FLOAT_EQ(config.attention_threshold, 0.1f);
    EXPECT_EQ(config.default_priority, NIMCP_PRIORITY_HIGH);
    EXPECT_TRUE(config.enable_burst_routing);
    EXPECT_TRUE(config.enable_selective_routing);
}

/* Test 2: Bridge creation */
TEST_F(SNNRoutingBridgeTest, BridgeCreation) {
    bridge = snn_routing_bridge_create(nullptr, network, router);
    ASSERT_NE(bridge, nullptr);
    EXPECT_TRUE(bridge->connected);
    EXPECT_NE(bridge->route_map, nullptr);
    EXPECT_NE(bridge->attention_weights, nullptr);
}

/* Test 3: Bridge creation with custom config */
TEST_F(SNNRoutingBridgeTest, BridgeCreationWithConfig) {
    snn_routing_config_t config;
    snn_routing_config_default(&config);
    config.max_queue_size = 500;
    config.enable_burst_routing = false;

    bridge = snn_routing_bridge_create(&config, network, router);
    ASSERT_NE(bridge, nullptr);
    EXPECT_EQ(bridge->config.max_queue_size, 500);
    EXPECT_FALSE(bridge->config.enable_burst_routing);
}

/* Test 4: Null network creation fails */
TEST_F(SNNRoutingBridgeTest, NullNetworkFails) {
    bridge = snn_routing_bridge_create(nullptr, nullptr, router);
    EXPECT_EQ(bridge, nullptr);
}

/* Test 5: Null router creation fails */
TEST_F(SNNRoutingBridgeTest, NullRouterFails) {
    bridge = snn_routing_bridge_create(nullptr, network, nullptr);
    EXPECT_EQ(bridge, nullptr);
}

/* Test 6: Set attention weight */
TEST_F(SNNRoutingBridgeTest, SetAttention) {
    bridge = snn_routing_bridge_create(nullptr, network, router);
    ASSERT_NE(bridge, nullptr);

    int result = snn_routing_bridge_set_attention(bridge, 0, 0.5f);
    EXPECT_EQ(result, 0);

    float attention;
    result = snn_routing_bridge_get_attention(bridge, 0, &attention);
    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(attention, 0.5f);
}

/* Test 7: Invalid population ID */
TEST_F(SNNRoutingBridgeTest, InvalidPopulationID) {
    bridge = snn_routing_bridge_create(nullptr, network, router);
    ASSERT_NE(bridge, nullptr);

    int result = snn_routing_bridge_set_attention(bridge, 999, 0.5f);
    EXPECT_NE(result, 0);
}

/* Test 8: Add route */
TEST_F(SNNRoutingBridgeTest, AddRoute) {
    bridge = snn_routing_bridge_create(nullptr, network, router);
    ASSERT_NE(bridge, nullptr);

    int result = snn_routing_bridge_add_route(bridge, 0, 100);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(bridge->stats.active_routes, 1);
}

/* Test 9: Remove route */
TEST_F(SNNRoutingBridgeTest, RemoveRoute) {
    bridge = snn_routing_bridge_create(nullptr, network, router);
    ASSERT_NE(bridge, nullptr);

    snn_routing_bridge_add_route(bridge, 0, 100);
    int result = snn_routing_bridge_remove_route(bridge, 0, 100);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(bridge->stats.active_routes, 0);
}

/* Test 10: Clear all routes */
TEST_F(SNNRoutingBridgeTest, ClearRoutes) {
    bridge = snn_routing_bridge_create(nullptr, network, router);
    ASSERT_NE(bridge, nullptr);

    snn_routing_bridge_add_route(bridge, 0, 100);
    snn_routing_bridge_add_route(bridge, 1, 101);
    snn_routing_bridge_clear_routes(bridge);
    EXPECT_EQ(bridge->stats.active_routes, 0);
}

/* Test 11: Process empty spikes */
TEST_F(SNNRoutingBridgeTest, ProcessEmptySpikes) {
    bridge = snn_routing_bridge_create(nullptr, network, router);
    ASSERT_NE(bridge, nullptr);

    snn_spike_t spikes_out[10];
    uint32_t n_out;
    int result = snn_routing_bridge_process(bridge, nullptr, 0,
                                            spikes_out, 10, &n_out);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(n_out, 0);
}

/* Test 12: Process valid spikes */
TEST_F(SNNRoutingBridgeTest, ProcessValidSpikes) {
    bridge = snn_routing_bridge_create(nullptr, network, router);
    ASSERT_NE(bridge, nullptr);

    snn_routing_bridge_add_route(bridge, 0, 100);

    snn_spike_t spikes_in[3];
    spikes_in[0] = {1000, 0, 0};
    spikes_in[1] = {2000, 1, 0};
    spikes_in[2] = {3000, 2, 0};

    snn_spike_t spikes_out[10];
    uint32_t n_out;
    int result = snn_routing_bridge_process(bridge, spikes_in, 3,
                                            spikes_out, 10, &n_out);
    EXPECT_EQ(result, 0);
    EXPECT_GT(bridge->stats.spikes_routed, 0);
}

/* Test 13: Update routing state */
TEST_F(SNNRoutingBridgeTest, UpdateState) {
    bridge = snn_routing_bridge_create(nullptr, network, router);
    ASSERT_NE(bridge, nullptr);

    float prev_time = bridge->last_update_time;
    int result = snn_routing_bridge_update(bridge, 1.0f);
    EXPECT_EQ(result, 0);
    EXPECT_GT(bridge->last_update_time, prev_time);
}

/* Test 14: Get statistics */
TEST_F(SNNRoutingBridgeTest, GetStatistics) {
    bridge = snn_routing_bridge_create(nullptr, network, router);
    ASSERT_NE(bridge, nullptr);

    snn_routing_stats_t stats;
    int result = snn_routing_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
}

/* Test 15: Reset statistics */
TEST_F(SNNRoutingBridgeTest, ResetStatistics) {
    bridge = snn_routing_bridge_create(nullptr, network, router);
    ASSERT_NE(bridge, nullptr);

    bridge->stats.spikes_routed = 100;
    snn_routing_bridge_reset_stats(bridge);
    EXPECT_EQ(bridge->stats.spikes_routed, 0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
