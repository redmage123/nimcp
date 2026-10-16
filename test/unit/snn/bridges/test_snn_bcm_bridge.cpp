/**
 * @file test_snn_bcm_bridge.cpp
 * @brief Unit tests for SNN-BCM plasticity bridge
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "snn/bridges/nimcp_snn_bcm_bridge.h"
#include "utils/memory/nimcp_memory.h"

class SNNBCMBridgeTest : public ::testing::Test {
protected:
    snn_network_t* network;
    bcm_synapse_t* bcm_synapses;
    snn_bcm_bridge_t* bridge;
    snn_bcm_bridge_config_t config;

    void SetUp() override {
        network = (snn_network_t*)nimcp_malloc(sizeof(snn_network_t));
        memset(network, 0, sizeof(snn_network_t));
        network->magic = SNN_MAGIC;

        bcm_synapses = (bcm_synapse_t*)nimcp_malloc(sizeof(bcm_synapse_t) * 100);
        for (int i = 0; i < 100; i++) {
            bcm_synapses[i] = bcm_synapse_init(0.5f, 1.0f);
        }

        snn_bcm_bridge_config_default(&config);
        bridge = snn_bcm_bridge_create(&config, network, bcm_synapses, 100, 50);
    }

    void TearDown() override {
        if (bridge) snn_bcm_bridge_destroy(bridge);
        if (bcm_synapses) nimcp_free(bcm_synapses);
        if (network) nimcp_free(network);
    }
};

TEST_F(SNNBCMBridgeTest, ConfigDefault) {
    snn_bcm_bridge_config_t cfg;
    snn_bcm_bridge_config_default(&cfg);

    EXPECT_FLOAT_EQ(cfg.rate_window_ms, 100.0f);
    EXPECT_TRUE(cfg.sync_thresholds);
    EXPECT_TRUE(cfg.rate_dependent_lr);
}

TEST_F(SNNBCMBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
    EXPECT_EQ(bridge->n_synapses, 100);
    EXPECT_EQ(bridge->n_neurons, 50);
}

TEST_F(SNNBCMBridgeTest, CreateNullParams) {
    auto b = snn_bcm_bridge_create(nullptr, network, bcm_synapses, 100, 50);
    EXPECT_EQ(b, nullptr);
}

TEST_F(SNNBCMBridgeTest, BioAsync) {
    EXPECT_FALSE(snn_bcm_bridge_is_bio_async_connected(nullptr));

    int ret = snn_bcm_bridge_disconnect_bio_async(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNBCMBridgeTest, UpdateRates) {
    int ret = snn_bcm_bridge_update_rates(bridge, 1.0f);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNBCMBridgeTest, UpdateThresholds) {
    int ret = snn_bcm_bridge_update_thresholds(bridge, 10.0f);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(bridge->threshold_updates, 1);
}

TEST_F(SNNBCMBridgeTest, ApplyPlasticity) {
    int ret = snn_bcm_bridge_apply_plasticity(bridge, 1.0f);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(bridge->plasticity_events, 1);
}

TEST_F(SNNBCMBridgeTest, FullUpdate) {
    int ret = snn_bcm_bridge_update(bridge, 1.0f);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(bridge->last_update_time_ms, 0.0f);
}

TEST_F(SNNBCMBridgeTest, GetEffects) {
    snn_bcm_bridge_update(bridge, 1.0f);

    snn_bcm_effects_t effects;
    int ret = snn_bcm_bridge_get_effects(bridge, &effects);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNBCMBridgeTest, GetAvgThreshold) {
    snn_bcm_bridge_update_thresholds(bridge, 10.0f);
    float threshold = snn_bcm_bridge_get_avg_threshold(bridge);
    EXPECT_GE(threshold, 0.0f);
}

TEST_F(SNNBCMBridgeTest, GetNeuronRate) {
    float rate = snn_bcm_bridge_get_neuron_rate(bridge, 0);
    EXPECT_GE(rate, 0.0f);

    rate = snn_bcm_bridge_get_neuron_rate(bridge, 999);
    EXPECT_LT(rate, 0.0f);
}

TEST_F(SNNBCMBridgeTest, LTPDominant) {
    bool dominant = snn_bcm_bridge_is_ltp_dominant(bridge);
    EXPECT_FALSE(dominant);
}

TEST_F(SNNBCMBridgeTest, Statistics) {
    snn_bcm_bridge_update(bridge, 1.0f);

    uint32_t events, threshold_updates, updates;
    int ret = snn_bcm_bridge_get_stats(bridge, &events, &threshold_updates, &updates);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNBCMBridgeTest, ResetStats) {
    snn_bcm_bridge_update(bridge, 1.0f);
    snn_bcm_bridge_reset_stats(bridge);

    EXPECT_EQ(bridge->plasticity_events, 0);
    EXPECT_EQ(bridge->threshold_updates, 0);
}

TEST_F(SNNBCMBridgeTest, WeightChanges) {
    uint32_t ids[10];
    float deltas[10];
    uint32_t n;

    int ret = snn_bcm_bridge_get_weight_changes(bridge, ids, deltas, 10, &n);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(n, 0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
