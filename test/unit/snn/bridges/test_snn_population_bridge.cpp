/**
 * @file test_snn_population_bridge.cpp
 * @brief Unit tests for SNN-Population Coding bridge
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
#include "snn/bridges/nimcp_snn_population_bridge.h"
#include "snn/nimcp_snn_config.h"
#include "utils/memory/nimcp_memory.h"

class SNNPopulationBridgeTest : public ::testing::Test {
protected:
    snn_network_t* network;
    snn_population_bridge_t* bridge;

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
        if (bridge) snn_population_bridge_destroy(bridge);
        if (network) snn_network_destroy(network);
    }
};

/* Test 1: Config defaults */
TEST_F(SNNPopulationBridgeTest, ConfigDefaults) {
    snn_population_config_t config;
    snn_population_config_default(&config);

    EXPECT_GT(config.tuning_width_rad, 0.0f);
    EXPECT_GT(config.max_firing_rate_hz, 0.0f);
    EXPECT_TRUE(config.enable_vector_sum);
    EXPECT_TRUE(config.auto_generate_tuning);
}

/* Test 2: Bridge creation */
TEST_F(SNNPopulationBridgeTest, BridgeCreation) {
    bridge = snn_population_bridge_create(nullptr, network);
    ASSERT_NE(bridge, nullptr);
    EXPECT_TRUE(bridge->connected);
    EXPECT_NE(bridge->encoder, nullptr);
    EXPECT_NE(bridge->tuning_curves, nullptr);
}

/* Test 3: Custom config */
TEST_F(SNNPopulationBridgeTest, CustomConfig) {
    snn_population_config_t config;
    snn_population_config_default(&config);
    config.max_firing_rate_hz = 200.0f;
    config.enable_pca = true;

    bridge = snn_population_bridge_create(&config, network);
    ASSERT_NE(bridge, nullptr);
    EXPECT_EQ(bridge->config.max_firing_rate_hz, 200.0f);
    EXPECT_TRUE(bridge->config.enable_pca);
}

/* Test 4: Null network fails */
TEST_F(SNNPopulationBridgeTest, NullNetworkFails) {
    bridge = snn_population_bridge_create(nullptr, nullptr);
    EXPECT_EQ(bridge, nullptr);
}

/* Test 5: Process empty spikes */
TEST_F(SNNPopulationBridgeTest, ProcessEmptySpikes) {
    bridge = snn_population_bridge_create(nullptr, network);
    ASSERT_NE(bridge, nullptr);

    snn_spike_t spikes_out[10];
    uint32_t n_out;
    int result = snn_population_bridge_process(bridge, nullptr, 0,
                                               spikes_out, 10, &n_out);
    EXPECT_EQ(result, 0);
}

/* Test 6: Process valid spikes */
TEST_F(SNNPopulationBridgeTest, ProcessValidSpikes) {
    bridge = snn_population_bridge_create(nullptr, network);
    ASSERT_NE(bridge, nullptr);

    snn_spike_t spikes_in[3];
    spikes_in[0] = {1000, 0, 0};
    spikes_in[1] = {2000, 1, 0};
    spikes_in[2] = {3000, 2, 0};

    snn_spike_t spikes_out[10];
    uint32_t n_out;
    int result = snn_population_bridge_process(bridge, spikes_in, 3,
                                               spikes_out, 10, &n_out);
    EXPECT_EQ(result, 0);
    EXPECT_GT(bridge->stats.vectors_encoded, 0);
}

/* Test 7: Update state */
TEST_F(SNNPopulationBridgeTest, UpdateState) {
    bridge = snn_population_bridge_create(nullptr, network);
    ASSERT_NE(bridge, nullptr);

    int result = snn_population_bridge_update(bridge, 1.0f);
    EXPECT_EQ(result, 0);
}

/* Test 8: Encode vector */
TEST_F(SNNPopulationBridgeTest, EncodeVector) {
    bridge = snn_population_bridge_create(nullptr, network);
    ASSERT_NE(bridge, nullptr);

    vector3d_t vector;
    int result = snn_population_bridge_encode_vector(bridge, 0, &vector);
    EXPECT_EQ(result, 0);
}

/* Test 9: Decode vector */
TEST_F(SNNPopulationBridgeTest, DecodeVector) {
    bridge = snn_population_bridge_create(nullptr, network);
    ASSERT_NE(bridge, nullptr);

    vector3d_t vector = {1.0f, 0.0f, 0.0f, 1.0f};
    int result = snn_population_bridge_decode_vector(bridge, 0, &vector);
    EXPECT_EQ(result, 0);
}

/* Test 10: Set tuning curve */
TEST_F(SNNPopulationBridgeTest, SetTuning) {
    bridge = snn_population_bridge_create(nullptr, network);
    ASSERT_NE(bridge, nullptr);

    tuning_curve_t tuning;
    tuning.preferred_direction = {1.0f, 0.0f, 0.0f, 1.0f};
    tuning.tuning_width = 0.5f;
    tuning.max_rate = 100.0f;

    int result = snn_population_bridge_set_tuning(bridge, 0, 0, &tuning);
    EXPECT_EQ(result, 0);
}

/* Test 11: Get tuning curve */
TEST_F(SNNPopulationBridgeTest, GetTuning) {
    bridge = snn_population_bridge_create(nullptr, network);
    ASSERT_NE(bridge, nullptr);

    tuning_curve_t tuning;
    int result = snn_population_bridge_get_tuning(bridge, 0, 0, &tuning);
    EXPECT_EQ(result, 0);
}

/* Test 12: Generate tuning curves */
TEST_F(SNNPopulationBridgeTest, GenerateTuning) {
    bridge = snn_population_bridge_create(nullptr, network);
    ASSERT_NE(bridge, nullptr);

    int result = snn_population_bridge_generate_tuning(bridge, 0);
    EXPECT_EQ(result, 0);
}

/* Test 13: Get current vector */
TEST_F(SNNPopulationBridgeTest, GetCurrentVector) {
    bridge = snn_population_bridge_create(nullptr, network);
    ASSERT_NE(bridge, nullptr);

    vector3d_t vector;
    int result = snn_population_bridge_get_current_vector(bridge, 0, &vector);
    EXPECT_EQ(result, 0);
}

/* Test 14: Get statistics */
TEST_F(SNNPopulationBridgeTest, GetStatistics) {
    bridge = snn_population_bridge_create(nullptr, network);
    ASSERT_NE(bridge, nullptr);

    snn_population_stats_t stats;
    int result = snn_population_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
}

/* Test 15: Reset statistics */
TEST_F(SNNPopulationBridgeTest, ResetStatistics) {
    bridge = snn_population_bridge_create(nullptr, network);
    ASSERT_NE(bridge, nullptr);

    bridge->stats.vectors_encoded = 100;
    snn_population_bridge_reset_stats(bridge);
    EXPECT_EQ(bridge->stats.vectors_encoded, 0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
