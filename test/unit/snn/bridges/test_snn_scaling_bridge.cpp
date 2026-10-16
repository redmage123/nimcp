/**
 * @file test_snn_scaling_bridge.cpp
 * @brief Unit tests for SNN-Synaptic Scaling bridge
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "snn/bridges/nimcp_snn_scaling_bridge.h"
#include "utils/memory/nimcp_memory.h"

class SNNScalingBridgeTest : public ::testing::Test {
protected:
    snn_network_t* network;
    synaptic_scaling_state_t* scaling_states;
    snn_scaling_bridge_t* bridge;
    snn_scaling_bridge_config_t config;

    void SetUp() override {
        network = (snn_network_t*)nimcp_malloc(sizeof(snn_network_t));
        memset(network, 0, sizeof(snn_network_t));
        network->magic = SNN_MAGIC;

        scaling_states = (synaptic_scaling_state_t*)
            nimcp_malloc(sizeof(synaptic_scaling_state_t) * 50);
        for (int i = 0; i < 50; i++) {
            scaling_states[i] = synaptic_scaling_state_init(5.0f);
        }

        snn_scaling_bridge_config_default(&config);
        bridge = snn_scaling_bridge_create(&config, network, scaling_states, 50, 200);
    }

    void TearDown() override {
        if (bridge) snn_scaling_bridge_destroy(bridge);
        if (scaling_states) nimcp_free(scaling_states);
        if (network) nimcp_free(network);
    }
};

TEST_F(SNNScalingBridgeTest, ConfigDefault) {
    snn_scaling_bridge_config_t cfg;
    snn_scaling_bridge_config_default(&cfg);

    EXPECT_FLOAT_EQ(cfg.target_rate_hz, 5.0f);
    EXPECT_FLOAT_EQ(cfg.scaling_exponent, 1.0f);
    EXPECT_TRUE(cfg.enable_bio_async);
}

TEST_F(SNNScalingBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
    EXPECT_EQ(bridge->n_neurons, 50);
    EXPECT_EQ(bridge->n_synapses, 200);
}

TEST_F(SNNScalingBridgeTest, CreateNullParams) {
    auto b = snn_scaling_bridge_create(nullptr, network, scaling_states, 50, 200);
    EXPECT_EQ(b, nullptr);
}

TEST_F(SNNScalingBridgeTest, BioAsync) {
    EXPECT_FALSE(snn_scaling_bridge_is_bio_async_connected(nullptr));

    int ret = snn_scaling_bridge_disconnect_bio_async(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNScalingBridgeTest, ComputeFactors) {
    int ret = snn_scaling_bridge_compute_factors(bridge, 1.0f);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNScalingBridgeTest, ApplyPlasticity) {
    int ret = snn_scaling_bridge_apply_plasticity(bridge, 1.0f);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(bridge->scaling_events, 1);
}

TEST_F(SNNScalingBridgeTest, FullUpdate) {
    int ret = snn_scaling_bridge_update(bridge, 1.0f);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(bridge->last_update_time_ms, 0.0f);
}

TEST_F(SNNScalingBridgeTest, GetEffects) {
    snn_scaling_bridge_update(bridge, 1.0f);

    snn_scaling_effects_t effects;
    int ret = snn_scaling_bridge_get_effects(bridge, &effects);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNScalingBridgeTest, GetAvgFactor) {
    snn_scaling_bridge_compute_factors(bridge, 1.0f);
    float factor = snn_scaling_bridge_get_avg_factor(bridge);
    EXPECT_GT(factor, 0.0f);
}

TEST_F(SNNScalingBridgeTest, Statistics) {
    snn_scaling_bridge_update(bridge, 1.0f);

    uint32_t events, updates;
    int ret = snn_scaling_bridge_get_stats(bridge, &events, &updates);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(events, 0);
}

TEST_F(SNNScalingBridgeTest, ResetStats) {
    snn_scaling_bridge_update(bridge, 1.0f);
    snn_scaling_bridge_reset_stats(bridge);

    EXPECT_EQ(bridge->scaling_events, 0);
    EXPECT_FLOAT_EQ(bridge->last_update_time_ms, 0.0f);
}

TEST_F(SNNScalingBridgeTest, WeightChanges) {
    uint32_t ids[10];
    float factors[10];
    uint32_t n;

    int ret = snn_scaling_bridge_get_weight_changes(bridge, ids, factors, 10, &n);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(n, 0);
}

TEST_F(SNNScalingBridgeTest, ScalingFactorRange) {
    snn_scaling_bridge_compute_factors(bridge, 1.0f);

    for (uint32_t i = 0; i < bridge->n_neurons; i++) {
        float factor = scaling_states[i].scaling_factor;
        EXPECT_GE(factor, config.min_scaling_factor);
        EXPECT_LE(factor, config.max_scaling_factor);
    }
}

TEST_F(SNNScalingBridgeTest, MultipleUpdates) {
    for (int i = 0; i < 10; i++) {
        int ret = snn_scaling_bridge_update(bridge, 1.0f);
        EXPECT_EQ(ret, 0);
    }

    EXPECT_EQ(bridge->scaling_events, 10);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
