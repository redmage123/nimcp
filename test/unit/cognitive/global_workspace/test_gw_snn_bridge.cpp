/**
 * @file test_gw_snn_bridge.cpp
 * @brief Unit tests for Global Workspace SNN Bridge
 */

#include <gtest/gtest.h>
#include <cmath>

#include "cognitive/global_workspace/nimcp_gw_snn_bridge.h"

class GWSNNBridgeTest : public ::testing::Test {
protected:
    gw_snn_bridge_t* bridge = nullptr;

    void SetUp() override {
        gw_snn_config_t config = gw_snn_config_default();
        config.enable_bio_async = false;
        bridge = gw_snn_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            gw_snn_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(GWSNNBridgeTest, CreateWithDefaults) {
    gw_snn_bridge_t* test_bridge = gw_snn_create(nullptr);
    ASSERT_NE(test_bridge, nullptr);
    gw_snn_destroy(test_bridge);
}

TEST_F(GWSNNBridgeTest, CreateWithConfig) {
    gw_snn_config_t config = gw_snn_config_default();
    config.num_dimensions = 8;
    config.neurons_per_dim = 16;
    gw_snn_bridge_t* test_bridge = gw_snn_create(&config);
    ASSERT_NE(test_bridge, nullptr);
    gw_snn_destroy(test_bridge);
}

TEST_F(GWSNNBridgeTest, CreateWithInvalidConfig) {
    gw_snn_config_t config = gw_snn_config_default();
    config.num_dimensions = 0;
    gw_snn_bridge_t* test_bridge = gw_snn_create(&config);
    EXPECT_EQ(test_bridge, nullptr);
}

TEST_F(GWSNNBridgeTest, Reset) {
    EXPECT_EQ(gw_snn_reset(bridge), 0);
}

TEST_F(GWSNNBridgeTest, ResetNull) {
    EXPECT_EQ(gw_snn_reset(nullptr), -1);
}

TEST_F(GWSNNBridgeTest, DestroyNull) {
    gw_snn_destroy(nullptr); // Should not crash
}

//=============================================================================
// Default Config Tests
//=============================================================================

TEST_F(GWSNNBridgeTest, DefaultConfigValues) {
    gw_snn_config_t config = gw_snn_config_default();
    EXPECT_EQ(config.num_dimensions, GW_DIM_COUNT);
    EXPECT_EQ(config.neurons_per_dim, GW_SNN_NEURONS_PER_DIM);
    EXPECT_GT(config.dt_ms, 0.0f);
    EXPECT_TRUE(config.enable_competition);
}

//=============================================================================
// Encoding Tests
//=============================================================================

TEST_F(GWSNNBridgeTest, EncodeState) {
    float dims[GW_DIM_COUNT] = {0};
    dims[GW_DIM_BROADCAST] = 0.8f;
    dims[GW_DIM_IGNITION] = 0.7f;

    int spike_count = gw_snn_encode_state(bridge, dims, GW_DIM_COUNT);
    EXPECT_GE(spike_count, 0);
}

TEST_F(GWSNNBridgeTest, EncodeStateNull) {
    EXPECT_EQ(gw_snn_encode_state(nullptr, nullptr, 0), -1);
    EXPECT_EQ(gw_snn_encode_state(bridge, nullptr, 0), -1);
}

TEST_F(GWSNNBridgeTest, EncodeBroadcast) {
    int spike_count = gw_snn_encode_broadcast(bridge, 0.8f, 1);
    EXPECT_GE(spike_count, 0);
}

TEST_F(GWSNNBridgeTest, EncodeCompetition) {
    int spike_count = gw_snn_encode_competition(bridge, 0.9f, 5);
    EXPECT_GE(spike_count, 0);
}

TEST_F(GWSNNBridgeTest, EncodeIgnition) {
    int spike_count = gw_snn_encode_ignition(bridge, 0.6f, 1);
    EXPECT_GE(spike_count, 0);
}

//=============================================================================
// Simulation Tests
//=============================================================================

TEST_F(GWSNNBridgeTest, SimulateBasic) {
    float dims[GW_DIM_COUNT] = {0.5f};
    gw_snn_encode_state(bridge, dims, 1);
    EXPECT_EQ(gw_snn_simulate(bridge, 10.0f), 0);
}

TEST_F(GWSNNBridgeTest, SimulateNull) {
    EXPECT_EQ(gw_snn_simulate(nullptr, 10.0f), -1);
}

TEST_F(GWSNNBridgeTest, SimulateZeroDuration) {
    EXPECT_EQ(gw_snn_simulate(bridge, 0.0f), -1);
}

TEST_F(GWSNNBridgeTest, SimulateNegativeDuration) {
    EXPECT_EQ(gw_snn_simulate(bridge, -10.0f), -1);
}

TEST_F(GWSNNBridgeTest, Step) {
    float dims[GW_DIM_COUNT] = {0.5f};
    gw_snn_encode_state(bridge, dims, 1);
    EXPECT_EQ(gw_snn_step(bridge), 0);
}

TEST_F(GWSNNBridgeTest, Forward) {
    float inputs[5] = {0.5f, 0.6f, 0.7f, 0.4f, 0.3f};
    int spike_count = gw_snn_forward(bridge, inputs, 5);
    EXPECT_GE(spike_count, 0);
}

//=============================================================================
// Conscious Access Decoding Tests
//=============================================================================

TEST_F(GWSNNBridgeTest, GetConsciousAccess) {
    float dims[GW_DIM_COUNT] = {0};
    dims[GW_DIM_BROADCAST] = 0.9f;
    dims[GW_DIM_IGNITION] = 0.85f;
    gw_snn_encode_state(bridge, dims, GW_DIM_COUNT);
    gw_snn_simulate(bridge, 20.0f);

    gw_conscious_access_t access;
    EXPECT_EQ(gw_snn_get_conscious_access(bridge, &access), 0);
    EXPECT_GE(access.broadcast_strength, 0.0f);
    EXPECT_LE(access.broadcast_strength, 1.0f);
    EXPECT_GE(access.ignition_level, 0.0f);
    EXPECT_LE(access.ignition_level, 1.0f);
}

TEST_F(GWSNNBridgeTest, GetConsciousAccessNull) {
    EXPECT_EQ(gw_snn_get_conscious_access(nullptr, nullptr), -1);
    gw_conscious_access_t access;
    EXPECT_EQ(gw_snn_get_conscious_access(nullptr, &access), -1);
    EXPECT_EQ(gw_snn_get_conscious_access(bridge, nullptr), -1);
}

TEST_F(GWSNNBridgeTest, GetActivations) {
    float activations[GW_DIM_COUNT];
    EXPECT_EQ(gw_snn_get_activations(bridge, activations, GW_DIM_COUNT), 0);
}

//=============================================================================
// Detection Tests
//=============================================================================

TEST_F(GWSNNBridgeTest, CheckIgnition) {
    float level;
    bool ignition = gw_snn_check_ignition(bridge, &level);
    EXPECT_GE(level, 0.0f);
    EXPECT_LE(level, 1.0f);
}

TEST_F(GWSNNBridgeTest, CheckBroadcast) {
    gw_snn_encode_broadcast(bridge, 0.8f, 1);
    gw_snn_simulate(bridge, 10.0f);

    float strength;
    gw_snn_check_broadcast(bridge, &strength);
    EXPECT_GE(strength, 0.0f);
}

TEST_F(GWSNNBridgeTest, CheckBinding) {
    float strength;
    gw_snn_check_binding(bridge, &strength);
    EXPECT_GE(strength, 0.0f);
}

//=============================================================================
// State Query Tests
//=============================================================================

TEST_F(GWSNNBridgeTest, GetDimState) {
    gw_dim_state_t state;
    EXPECT_EQ(gw_snn_get_dim_state(bridge, 0, &state), 0);
}

TEST_F(GWSNNBridgeTest, GetDimStateInvalidDim) {
    gw_dim_state_t state;
    EXPECT_EQ(gw_snn_get_dim_state(bridge, 100, &state), -1);
}

TEST_F(GWSNNBridgeTest, GetBridgeState) {
    gw_snn_bridge_state_t state;
    EXPECT_EQ(gw_snn_get_state(bridge, &state), 0);
    EXPECT_EQ(state.state, GW_SNN_STATE_IDLE);
}

TEST_F(GWSNNBridgeTest, GetStats) {
    gw_snn_stats_t stats;
    EXPECT_EQ(gw_snn_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.total_evaluations, 0u);
}

TEST_F(GWSNNBridgeTest, ResetStats) {
    float dims[1] = {0.5f};
    gw_snn_encode_state(bridge, dims, 1);
    gw_snn_simulate(bridge, 10.0f);

    gw_snn_stats_t stats;
    gw_snn_get_stats(bridge, &stats);
    EXPECT_GT(stats.total_evaluations, 0u);

    EXPECT_EQ(gw_snn_reset_stats(bridge), 0);
    gw_snn_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_evaluations, 0u);
}

TEST_F(GWSNNBridgeTest, GetBroadcastStrength) {
    float strength = gw_snn_get_broadcast_strength(bridge);
    EXPECT_GE(strength, 0.0f);
    EXPECT_LE(strength, 1.0f);
}

TEST_F(GWSNNBridgeTest, GetTotalActivity) {
    float activity = gw_snn_get_total_activity(bridge);
    EXPECT_GE(activity, 0.0f);
}

//=============================================================================
// Callback Tests
//=============================================================================

static int ignition_callback_count = 0;
static void test_ignition_callback(gw_snn_bridge_t*, float, uint64_t, void*) {
    ignition_callback_count++;
}

TEST_F(GWSNNBridgeTest, RegisterIgnitionCallback) {
    EXPECT_EQ(gw_snn_register_ignition_callback(bridge, test_ignition_callback, nullptr), 0);
}

TEST_F(GWSNNBridgeTest, RegisterConsciousCallback) {
    EXPECT_EQ(gw_snn_register_conscious_callback(bridge, nullptr, nullptr), 0);
}

TEST_F(GWSNNBridgeTest, RegisterBroadcastCallback) {
    EXPECT_EQ(gw_snn_register_broadcast_callback(bridge, nullptr, nullptr), 0);
}

//=============================================================================
// Bio-Async Tests
//=============================================================================

TEST_F(GWSNNBridgeTest, BioAsyncNotEnabled) {
    EXPECT_EQ(gw_snn_bio_async_connect(bridge), -1);
    EXPECT_FALSE(gw_snn_is_bio_async_connected(bridge));
}

TEST_F(GWSNNBridgeTest, BioAsyncDisconnectSafe) {
    EXPECT_EQ(gw_snn_bio_async_disconnect(bridge), 0);
}

//=============================================================================
// Integration Workflow Tests
//=============================================================================

TEST_F(GWSNNBridgeTest, ConsciousAccessWorkflow) {
    // Encode high broadcast + ignition
    float dims[GW_DIM_COUNT] = {0};
    dims[GW_DIM_BROADCAST] = 0.9f;
    dims[GW_DIM_IGNITION] = 0.8f;
    dims[GW_DIM_COMPETITION] = 0.7f;

    int spikes = gw_snn_encode_state(bridge, dims, GW_DIM_COUNT);
    EXPECT_GE(spikes, 0);

    // Simulate
    EXPECT_EQ(gw_snn_simulate(bridge, 50.0f), 0);

    // Get conscious access
    gw_conscious_access_t access;
    EXPECT_EQ(gw_snn_get_conscious_access(bridge, &access), 0);

    // Should show activity
    EXPECT_GE(access.broadcast_strength, 0.0f);
}

TEST_F(GWSNNBridgeTest, BroadcastDetectionWorkflow) {
    // Encode strong broadcast
    int spikes = gw_snn_encode_broadcast(bridge, 0.9f, 1);
    EXPECT_GE(spikes, 0);

    // Simulate
    EXPECT_EQ(gw_snn_simulate(bridge, 30.0f), 0);

    // Check broadcast detection
    float strength;
    bool active = gw_snn_check_broadcast(bridge, &strength);
    EXPECT_GE(strength, 0.0f);
}

TEST_F(GWSNNBridgeTest, IgnitionDetectionWorkflow) {
    // Encode strong ignition
    int spikes = gw_snn_encode_ignition(bridge, 0.9f, 3);
    EXPECT_GE(spikes, 0);

    // Simulate
    EXPECT_EQ(gw_snn_simulate(bridge, 30.0f), 0);

    // Check ignition detection
    float level;
    gw_snn_check_ignition(bridge, &level);
    EXPECT_GE(level, 0.0f);
}
