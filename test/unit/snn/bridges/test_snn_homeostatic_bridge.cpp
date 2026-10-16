/**
 * @file test_snn_homeostatic_bridge.cpp
 * @brief Unit tests for SNN-Homeostatic plasticity bridge
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "snn/bridges/nimcp_snn_homeostatic_bridge.h"
#include "utils/memory/nimcp_memory.h"

class SNNHomeostaticBridgeTest : public ::testing::Test {
protected:
    snn_network_t* network;
    homeostatic_controller_t controller;
    snn_homeostatic_bridge_t* bridge;
    snn_homeostatic_bridge_config_t config;

    void SetUp() override {
        network = (snn_network_t*)nimcp_malloc(sizeof(snn_network_t));
        memset(network, 0, sizeof(snn_network_t));
        network->magic = SNN_MAGIC;

        homeostatic_config_t h_config = homeostatic_config_default();
        controller = homeostatic_controller_create(&h_config, 50);

        snn_homeostatic_bridge_config_default(&config);
        bridge = snn_homeostatic_bridge_create(&config, network, controller, 50);
    }

    void TearDown() override {
        if (bridge) snn_homeostatic_bridge_destroy(bridge);
        if (controller) homeostatic_controller_destroy(controller);
        if (network) nimcp_free(network);
    }
};

TEST_F(SNNHomeostaticBridgeTest, ConfigDefault) {
    snn_homeostatic_bridge_config_t cfg;
    snn_homeostatic_bridge_config_default(&cfg);

    EXPECT_FLOAT_EQ(cfg.target_rate_hz, 5.0f);
    EXPECT_TRUE(cfg.enable_threshold_adaptation);
}

TEST_F(SNNHomeostaticBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
    EXPECT_EQ(bridge->n_neurons, 50);
}

TEST_F(SNNHomeostaticBridgeTest, CreateNullParams) {
    auto b = snn_homeostatic_bridge_create(nullptr, network, controller, 50);
    EXPECT_EQ(b, nullptr);
}

TEST_F(SNNHomeostaticBridgeTest, BioAsync) {
    int ret = snn_homeostatic_bridge_disconnect_bio_async(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNHomeostaticBridgeTest, UpdateRates) {
    int ret = snn_homeostatic_bridge_update_rates(bridge, 1.0f);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNHomeostaticBridgeTest, ApplyPlasticity) {
    int ret = snn_homeostatic_bridge_apply_plasticity(bridge, 1.0f);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNHomeostaticBridgeTest, FullUpdate) {
    int ret = snn_homeostatic_bridge_update(bridge, 1.0f);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNHomeostaticBridgeTest, GetEffects) {
    snn_homeostatic_bridge_update(bridge, 1.0f);

    snn_homeostatic_effects_t effects;
    int ret = snn_homeostatic_bridge_get_effects(bridge, &effects);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNHomeostaticBridgeTest, GetAvgRate) {
    float rate = snn_homeostatic_bridge_get_avg_rate(bridge);
    EXPECT_GE(rate, 0.0f);
}

TEST_F(SNNHomeostaticBridgeTest, IsStable) {
    /* Initially unstable (neuron rates are 0, below target 5Hz) */
    bool initial_stable = snn_homeostatic_bridge_is_stable(bridge);

    /* Run updates to let homeostasis adjust - simulate neurons hitting target */
    for (int i = 0; i < 10; i++) {
        /* Simulate neurons at target rate by setting their rates */
        for (uint32_t n = 0; n < bridge->n_neurons; n++) {
            bridge->neuron_states[n].current_rate = bridge->config.target_rate_hz;
        }
        snn_homeostatic_bridge_update(bridge, 1.0f);
    }

    bool stable = snn_homeostatic_bridge_is_stable(bridge);
    EXPECT_TRUE(stable);
}

TEST_F(SNNHomeostaticBridgeTest, Statistics) {
    snn_homeostatic_bridge_update(bridge, 1.0f);

    uint32_t adjustments, checks, updates;
    int ret = snn_homeostatic_bridge_get_stats(bridge, &adjustments, &checks, &updates);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNHomeostaticBridgeTest, ResetStats) {
    snn_homeostatic_bridge_update(bridge, 1.0f);
    snn_homeostatic_bridge_reset_stats(bridge);

    EXPECT_EQ(bridge->threshold_adjustments, 0);
    EXPECT_EQ(bridge->stability_checks, 0);
}

TEST_F(SNNHomeostaticBridgeTest, WeightChanges) {
    uint32_t ids[10];
    float adjustments[10];
    uint32_t n;

    int ret = snn_homeostatic_bridge_get_weight_changes(bridge, ids, adjustments, 10, &n);
    EXPECT_EQ(ret, 0);
}

TEST_F(SNNHomeostaticBridgeTest, NeuronStates) {
    ASSERT_NE(bridge->neuron_states, nullptr);
    EXPECT_EQ(bridge->neuron_states[0].neuron_id, 0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
