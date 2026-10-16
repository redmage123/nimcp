/**
 * @file test_snn_attention_bridge.cpp
 * @brief Unit tests for SNN-Attention bridge
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "snn/bridges/nimcp_snn_attention_bridge.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_config.h"
#include "plasticity/attention/nimcp_attention.h"

class SNNAttentionBridgeTest : public ::testing::Test {
protected:
    snn_network_t* snn;
    multihead_attention_t attention_obj;  /* Opaque pointer */
    snn_attention_bridge_t* bridge;

    void SetUp() override {
        // Create minimal SNN network
        snn_config_t snn_config;
        snn_config_default(&snn_config);
        snn_config.n_inputs = 10;
        snn_config.n_outputs = 10;
        snn_config.n_populations = 2;
        snn_config.dt = 0.1f;
        snn = snn_network_create(&snn_config);
        ASSERT_NE(snn, nullptr);

        // Create multihead attention
        multihead_attention_config_t attn_config = {};
        attn_config.num_heads = 4;
        attn_config.input_dim = 64;
        attn_config.output_dim = 64;
        attn_config.sequence_length = 10;
        attention_obj = multihead_attention_create(&attn_config);
        ASSERT_NE(attention_obj, nullptr);

        bridge = nullptr;
    }

    void TearDown() override {
        if (bridge) {
            snn_attention_bridge_destroy(bridge);
        }
        if (attention_obj) {
            multihead_attention_destroy(attention_obj);
        }
        if (snn) {
            snn_network_destroy(snn);
        }
    }
};

// Test 1: Default configuration initialization
TEST_F(SNNAttentionBridgeTest, DefaultConfigInitialization) {
    snn_attention_config_t config;
    snn_attention_config_default(&config);

    EXPECT_EQ(config.spike_rate_min, 10.0f);
    EXPECT_EQ(config.spike_rate_max, 80.0f);
    EXPECT_TRUE(config.enable_gamma_sync);
    EXPECT_EQ(config.gamma_frequency, 40.0f);
    EXPECT_FLOAT_EQ(config.attention_boost_factor, 1.5f);
}

// Test 2: Bridge creation with valid parameters
TEST_F(SNNAttentionBridgeTest, BridgeCreation) {
    snn_attention_config_t config;
    snn_attention_config_default(&config);

    bridge = snn_attention_bridge_create(&config, snn, &attention_obj);
    ASSERT_NE(bridge, nullptr);
}

// Test 3: Bridge creation with NULL parameters fails
TEST_F(SNNAttentionBridgeTest, BridgeCreationNullParams) {
    snn_attention_config_t config;
    snn_attention_config_default(&config);

    EXPECT_EQ(snn_attention_bridge_create(nullptr, snn, &attention_obj), nullptr);
    EXPECT_EQ(snn_attention_bridge_create(&config, nullptr, &attention_obj), nullptr);
    EXPECT_EQ(snn_attention_bridge_create(&config, snn, nullptr), nullptr);
}

// Test 4: Bridge destruction
TEST_F(SNNAttentionBridgeTest, BridgeDestruction) {
    snn_attention_config_t config;
    snn_attention_config_default(&config);

    bridge = snn_attention_bridge_create(&config, snn, &attention_obj);
    ASSERT_NE(bridge, nullptr);

    snn_attention_bridge_destroy(bridge);
    bridge = nullptr;  // Prevent double-free in TearDown
}

// Test 5: Compute gate signal from spike rate
TEST_F(SNNAttentionBridgeTest, ComputeGateSignal) {
    snn_attention_config_t config;
    snn_attention_config_default(&config);

    bridge = snn_attention_bridge_create(&config, snn, &attention_obj);
    ASSERT_NE(bridge, nullptr);

    // Test min rate
    float gate = snn_attention_compute_gate_signal(bridge, 10.0f);
    EXPECT_FLOAT_EQ(gate, 0.0f);

    // Test max rate
    gate = snn_attention_compute_gate_signal(bridge, 80.0f);
    EXPECT_FLOAT_EQ(gate, 1.0f);

    // Test mid rate
    gate = snn_attention_compute_gate_signal(bridge, 45.0f);
    EXPECT_GT(gate, 0.0f);
    EXPECT_LT(gate, 1.0f);
}

// Test 6: Gamma oscillation state initialization
TEST_F(SNNAttentionBridgeTest, GammaStateInitialization) {
    snn_attention_config_t config;
    snn_attention_config_default(&config);

    bridge = snn_attention_bridge_create(&config, snn, &attention_obj);
    ASSERT_NE(bridge, nullptr);

    snn_gamma_state_t gamma;
    int ret = snn_attention_get_gamma_state(bridge, &gamma);
    EXPECT_EQ(ret, 0);
    EXPECT_FLOAT_EQ(gamma.frequency, 40.0f);
    EXPECT_FALSE(gamma.is_synchronized);
}

// Test 7: Get gate signal
TEST_F(SNNAttentionBridgeTest, GetGateSignal) {
    snn_attention_config_t config;
    snn_attention_config_default(&config);

    bridge = snn_attention_bridge_create(&config, snn, &attention_obj);
    ASSERT_NE(bridge, nullptr);

    float gate = snn_attention_get_gate_signal(bridge);
    EXPECT_GE(gate, 0.0f);
    EXPECT_LE(gate, 1.0f);
}

// Test 8: Check gamma synchronization status
TEST_F(SNNAttentionBridgeTest, CheckGammaSynchronization) {
    snn_attention_config_t config;
    snn_attention_config_default(&config);

    bridge = snn_attention_bridge_create(&config, snn, &attention_obj);
    ASSERT_NE(bridge, nullptr);

    bool synced = snn_attention_is_gamma_synchronized(bridge);
    EXPECT_FALSE(synced);  // Initially not synchronized
}

// Test 9: Get bridge state
TEST_F(SNNAttentionBridgeTest, GetBridgeState) {
    snn_attention_config_t config;
    snn_attention_config_default(&config);

    bridge = snn_attention_bridge_create(&config, snn, &attention_obj);
    ASSERT_NE(bridge, nullptr);

    snn_attention_state_t state;
    int ret = snn_attention_bridge_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(state.sync_count, 0);
}

// Test 10: Get statistics
TEST_F(SNNAttentionBridgeTest, GetStatistics) {
    snn_attention_config_t config;
    snn_attention_config_default(&config);

    bridge = snn_attention_bridge_create(&config, snn, &attention_obj);
    ASSERT_NE(bridge, nullptr);

    uint32_t sync_count;
    float avg_gate, avg_rate;
    int ret = snn_attention_get_stats(bridge, &sync_count, &avg_gate, &avg_rate);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(sync_count, 0);
    EXPECT_FLOAT_EQ(avg_gate, 0.0f);
    EXPECT_FLOAT_EQ(avg_rate, 0.0f);
}

// Test 11: Reset statistics
TEST_F(SNNAttentionBridgeTest, ResetStatistics) {
    snn_attention_config_t config;
    snn_attention_config_default(&config);

    bridge = snn_attention_bridge_create(&config, snn, &attention_obj);
    ASSERT_NE(bridge, nullptr);

    snn_attention_reset_stats(bridge);

    uint32_t sync_count;
    int ret = snn_attention_get_stats(bridge, &sync_count, nullptr, nullptr);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(sync_count, 0);
}

// Test 12: Bio-async connection status
TEST_F(SNNAttentionBridgeTest, BioAsyncConnectionStatus) {
    snn_attention_config_t config;
    snn_attention_config_default(&config);

    bridge = snn_attention_bridge_create(&config, snn, &attention_obj);
    ASSERT_NE(bridge, nullptr);

    bool connected = snn_attention_bridge_is_bio_async_connected(bridge);
    EXPECT_FALSE(connected);  // Initially not connected
}

// Test 13: Bio-async connect (may fail if router unavailable)
TEST_F(SNNAttentionBridgeTest, BioAsyncConnect) {
    snn_attention_config_t config;
    snn_attention_config_default(&config);

    bridge = snn_attention_bridge_create(&config, snn, &attention_obj);
    ASSERT_NE(bridge, nullptr);

    // Connection may fail if router not available, that's OK
    int ret = snn_attention_bridge_connect_bio_async(bridge);
    // Don't assert success, just check it doesn't crash
    EXPECT_TRUE(ret <= 0);  // 0 = success, negative = router not available
}

// Test 14: Bio-async disconnect
TEST_F(SNNAttentionBridgeTest, BioAsyncDisconnect) {
    snn_attention_config_t config;
    snn_attention_config_default(&config);

    bridge = snn_attention_bridge_create(&config, snn, &attention_obj);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_attention_bridge_disconnect_bio_async(bridge);
    EXPECT_EQ(ret, 0);
}

// Test 15: Process function doesn't crash
TEST_F(SNNAttentionBridgeTest, ProcessFunction) {
    snn_attention_config_t config;
    snn_attention_config_default(&config);

    bridge = snn_attention_bridge_create(&config, snn, &attention_obj);
    ASSERT_NE(bridge, nullptr);

    float output[2] = {0.0f, 0.0f};
    int ret = snn_attention_bridge_process(bridge, nullptr, output);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(output[0], 0.0f);
    EXPECT_LE(output[0], 1.0f);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
