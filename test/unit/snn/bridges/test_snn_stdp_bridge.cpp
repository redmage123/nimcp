/**
 * @file test_snn_stdp_bridge.cpp
 * @brief Unit tests for SNN-STDP plasticity bridge
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "snn/bridges/nimcp_snn_stdp_bridge.h"
#include "utils/memory/nimcp_memory.h"

class SNNSTDPBridgeTest : public ::testing::Test {
protected:
    snn_network_t* network;
    stdp_synapse_t* stdp_synapses;
    snn_stdp_bridge_t* bridge;
    snn_stdp_bridge_config_t config;

    void SetUp() override {
        /* Create minimal network */
        network = (snn_network_t*)nimcp_malloc(sizeof(snn_network_t));
        memset(network, 0, sizeof(snn_network_t));
        network->magic = SNN_MAGIC;

        /* Create STDP synapses */
        stdp_synapses = (stdp_synapse_t*)nimcp_malloc(sizeof(stdp_synapse_t) * 100);
        for (int i = 0; i < 100; i++) {
            stdp_synapse_init(&stdp_synapses[i]);
        }

        /* Create bridge */
        snn_stdp_bridge_config_default(&config);
        bridge = snn_stdp_bridge_create(&config, network, stdp_synapses, 100);
    }

    void TearDown() override {
        if (bridge) snn_stdp_bridge_destroy(bridge);
        if (stdp_synapses) nimcp_free(stdp_synapses);
        if (network) nimcp_free(network);
    }
};

TEST_F(SNNSTDPBridgeTest, ConfigDefault) {
    snn_stdp_bridge_config_t cfg;
    snn_stdp_bridge_config_default(&cfg);

    EXPECT_FLOAT_EQ(cfg.ltp_window_ms, 20.0f);
    EXPECT_FLOAT_EQ(cfg.ltd_window_ms, 20.0f);
    EXPECT_TRUE(cfg.sync_learning_rates);
    EXPECT_TRUE(cfg.enable_da_modulation);
    EXPECT_TRUE(cfg.enable_bio_async);
}

TEST_F(SNNSTDPBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
    EXPECT_TRUE(bridge->connected);
    EXPECT_EQ(bridge->n_synapses, 100);
}

TEST_F(SNNSTDPBridgeTest, CreateNullParams) {
    auto b1 = snn_stdp_bridge_create(nullptr, network, stdp_synapses, 100);
    EXPECT_EQ(b1, nullptr);

    auto b2 = snn_stdp_bridge_create(&config, nullptr, stdp_synapses, 100);
    EXPECT_EQ(b2, nullptr);

    auto b3 = snn_stdp_bridge_create(&config, network, nullptr, 100);
    EXPECT_EQ(b3, nullptr);

    auto b4 = snn_stdp_bridge_create(&config, network, stdp_synapses, 0);
    EXPECT_EQ(b4, nullptr);
}

TEST_F(SNNSTDPBridgeTest, BioAsyncConnection) {
    config.enable_bio_async = false;
    auto b = snn_stdp_bridge_create(&config, network, stdp_synapses, 100);
    ASSERT_NE(b, nullptr);

    EXPECT_FALSE(snn_stdp_bridge_is_bio_async_connected(b));

    int ret = snn_stdp_bridge_connect_bio_async(b);
    EXPECT_EQ(ret, 0);

    ret = snn_stdp_bridge_disconnect_bio_async(b);
    EXPECT_EQ(ret, 0);

    snn_stdp_bridge_destroy(b);
}

TEST_F(SNNSTDPBridgeTest, UpdateEffects) {
    int ret = snn_stdp_bridge_update_effects(bridge);
    EXPECT_EQ(ret, 0);

    snn_stdp_effects_t effects;
    ret = snn_stdp_bridge_get_effects(bridge, &effects);
    EXPECT_EQ(ret, 0);

    EXPECT_GT(effects.effective_a_plus, 0.0f);
    EXPECT_GT(effects.effective_a_minus, 0.0f);
}

TEST_F(SNNSTDPBridgeTest, ApplyPlasticity) {
    int ret = snn_stdp_bridge_apply_plasticity(bridge, 1.0f);
    EXPECT_EQ(ret, 0);

    uint32_t events, syncs, updates;
    snn_stdp_bridge_get_stats(bridge, &events, &syncs, &updates);
    EXPECT_EQ(events, 1);
}

TEST_F(SNNSTDPBridgeTest, FullUpdate) {
    int ret = snn_stdp_bridge_update(bridge, 1.0f);
    EXPECT_EQ(ret, 0);

    EXPECT_GT(bridge->last_update_time_ms, 0.0f);
}

TEST_F(SNNSTDPBridgeTest, RecordWeightChange) {
    int ret = snn_stdp_bridge_record_weight_change(bridge, 10, 0.05f);
    EXPECT_EQ(ret, 0);

    EXPECT_EQ(bridge->weight_change_count, 1);
    EXPECT_EQ(bridge->weight_changes[0].synapse_id, 10);
    EXPECT_FLOAT_EQ(bridge->weight_changes[0].delta_weight, 0.05f);
}

TEST_F(SNNSTDPBridgeTest, GetWeightChanges) {
    snn_stdp_bridge_record_weight_change(bridge, 1, 0.1f);
    snn_stdp_bridge_record_weight_change(bridge, 2, -0.05f);

    weight_change_record_t changes[10];
    uint32_t n_changes;
    int ret = snn_stdp_bridge_get_weight_changes(bridge, changes, 10, &n_changes);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(n_changes, 2);
}

TEST_F(SNNSTDPBridgeTest, MarkSynced) {
    snn_stdp_bridge_record_weight_change(bridge, 1, 0.1f);

    int ret = snn_stdp_bridge_mark_synced(bridge, 1);
    EXPECT_EQ(ret, 0);

    EXPECT_TRUE(bridge->weight_changes[0].applied_to_snn);
}

TEST_F(SNNSTDPBridgeTest, GetEffectiveAmplitudes) {
    float a_plus = snn_stdp_bridge_get_effective_a_plus(bridge);
    float a_minus = snn_stdp_bridge_get_effective_a_minus(bridge);

    EXPECT_GT(a_plus, 0.0f);
    EXPECT_GT(a_minus, 0.0f);
}

TEST_F(SNNSTDPBridgeTest, GetDAModulation) {
    float da = snn_stdp_bridge_get_da_modulation(bridge);
    EXPECT_GE(da, 0.0f);
}

TEST_F(SNNSTDPBridgeTest, Statistics) {
    snn_stdp_bridge_update(bridge, 1.0f);
    snn_stdp_bridge_update(bridge, 1.0f);

    uint32_t events, syncs, updates;
    int ret = snn_stdp_bridge_get_stats(bridge, &events, &syncs, &updates);

    EXPECT_EQ(ret, 0);
    EXPECT_GT(events, 0);
}

TEST_F(SNNSTDPBridgeTest, ResetStats) {
    snn_stdp_bridge_update(bridge, 1.0f);
    snn_stdp_bridge_reset_stats(bridge);

    EXPECT_EQ(bridge->plasticity_events, 0);
    EXPECT_EQ(bridge->weight_syncs, 0);
    EXPECT_FLOAT_EQ(bridge->last_update_time_ms, 0.0f);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
