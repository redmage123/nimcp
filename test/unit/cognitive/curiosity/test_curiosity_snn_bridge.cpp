/**
 * @file test_curiosity_snn_bridge.cpp
 * @brief Unit tests for Curiosity SNN Bridge
 */

#include <gtest/gtest.h>
#include <cmath>

#include "cognitive/curiosity/nimcp_curiosity_snn_bridge.h"

class CuriositySNNBridgeTest : public ::testing::Test {
protected:
    curiosity_snn_bridge_t* bridge = nullptr;

    void SetUp() override {
        curiosity_snn_config_t config = curiosity_snn_config_default();
        config.enable_bio_async = false;
        bridge = curiosity_snn_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            curiosity_snn_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(CuriositySNNBridgeTest, CreateWithDefaults) {
    curiosity_snn_bridge_t* test_bridge = curiosity_snn_create(nullptr);
    ASSERT_NE(test_bridge, nullptr);
    curiosity_snn_destroy(test_bridge);
}

TEST_F(CuriositySNNBridgeTest, CreateWithConfig) {
    curiosity_snn_config_t config = curiosity_snn_config_default();
    config.num_dimensions = 8;
    config.neurons_per_dim = 16;
    curiosity_snn_bridge_t* test_bridge = curiosity_snn_create(&config);
    ASSERT_NE(test_bridge, nullptr);
    curiosity_snn_destroy(test_bridge);
}

TEST_F(CuriositySNNBridgeTest, CreateWithInvalidConfig) {
    curiosity_snn_config_t config = curiosity_snn_config_default();
    config.num_dimensions = 0;
    curiosity_snn_bridge_t* test_bridge = curiosity_snn_create(&config);
    EXPECT_EQ(test_bridge, nullptr);
}

TEST_F(CuriositySNNBridgeTest, Reset) {
    EXPECT_EQ(curiosity_snn_reset(bridge), 0);
}

TEST_F(CuriositySNNBridgeTest, ResetNull) {
    EXPECT_EQ(curiosity_snn_reset(nullptr), -1);
}

TEST_F(CuriositySNNBridgeTest, DestroyNull) {
    curiosity_snn_destroy(nullptr); // Should not crash
}

//=============================================================================
// Default Config Tests
//=============================================================================

TEST_F(CuriositySNNBridgeTest, DefaultConfigValues) {
    curiosity_snn_config_t config = curiosity_snn_config_default();
    EXPECT_EQ(config.num_dimensions, CURIOSITY_DIM_COUNT);
    EXPECT_EQ(config.neurons_per_dim, CURIOSITY_SNN_NEURONS_PER_DIM);
    EXPECT_GT(config.dt_ms, 0.0f);
    EXPECT_TRUE(config.enable_novelty_detection);
    EXPECT_TRUE(config.enable_exploration);
}

//=============================================================================
// Encoding Tests
//=============================================================================

TEST_F(CuriositySNNBridgeTest, EncodeState) {
    float dims[CURIOSITY_DIM_COUNT] = {0};
    dims[CURIOSITY_DIM_NOVELTY] = 0.8f;
    dims[CURIOSITY_DIM_EXPLORATION] = 0.7f;

    int spike_count = curiosity_snn_encode_state(bridge, dims, CURIOSITY_DIM_COUNT);
    EXPECT_GE(spike_count, 0);
}

TEST_F(CuriositySNNBridgeTest, EncodeStateNull) {
    EXPECT_EQ(curiosity_snn_encode_state(nullptr, nullptr, 0), -1);
    EXPECT_EQ(curiosity_snn_encode_state(bridge, nullptr, 0), -1);
}

TEST_F(CuriositySNNBridgeTest, EncodeNovelty) {
    int spike_count = curiosity_snn_encode_novelty(bridge, 0.8f, 0.6f);
    EXPECT_GE(spike_count, 0);
}

TEST_F(CuriositySNNBridgeTest, EncodeKnowledgeGap) {
    int spike_count = curiosity_snn_encode_knowledge_gap(bridge, 0.7f, 5);
    EXPECT_GE(spike_count, 0);
}

TEST_F(CuriositySNNBridgeTest, EncodeInfoGain) {
    int spike_count = curiosity_snn_encode_info_gain(bridge, 0.9f, 1);
    EXPECT_GE(spike_count, 0);
}

//=============================================================================
// Simulation Tests
//=============================================================================

TEST_F(CuriositySNNBridgeTest, SimulateBasic) {
    float dims[CURIOSITY_DIM_COUNT] = {0.5f};
    curiosity_snn_encode_state(bridge, dims, 1);
    EXPECT_EQ(curiosity_snn_simulate(bridge, 10.0f), 0);
}

TEST_F(CuriositySNNBridgeTest, SimulateNull) {
    EXPECT_EQ(curiosity_snn_simulate(nullptr, 10.0f), -1);
}

TEST_F(CuriositySNNBridgeTest, SimulateZeroDuration) {
    EXPECT_EQ(curiosity_snn_simulate(bridge, 0.0f), -1);
}

TEST_F(CuriositySNNBridgeTest, SimulateNegativeDuration) {
    EXPECT_EQ(curiosity_snn_simulate(bridge, -10.0f), -1);
}

TEST_F(CuriositySNNBridgeTest, Step) {
    float dims[CURIOSITY_DIM_COUNT] = {0.5f};
    curiosity_snn_encode_state(bridge, dims, 1);
    EXPECT_EQ(curiosity_snn_step(bridge), 0);
}

TEST_F(CuriositySNNBridgeTest, Forward) {
    float inputs[5] = {0.5f, 0.6f, 0.7f, 0.4f, 0.3f};
    int spike_count = curiosity_snn_forward(bridge, inputs, 5);
    EXPECT_GE(spike_count, 0);
}

//=============================================================================
// Drive Decoding Tests
//=============================================================================

TEST_F(CuriositySNNBridgeTest, GetDrive) {
    float dims[CURIOSITY_DIM_COUNT] = {0};
    dims[CURIOSITY_DIM_NOVELTY] = 0.9f;
    dims[CURIOSITY_DIM_EXPLORATION] = 0.85f;
    curiosity_snn_encode_state(bridge, dims, CURIOSITY_DIM_COUNT);
    curiosity_snn_simulate(bridge, 20.0f);

    curiosity_drive_t drive;
    EXPECT_EQ(curiosity_snn_get_drive(bridge, &drive), 0);
    EXPECT_GE(drive.novelty_level, 0.0f);
    EXPECT_LE(drive.novelty_level, 1.0f);
    EXPECT_GE(drive.exploration_drive, 0.0f);
    EXPECT_LE(drive.exploration_drive, 1.0f);
}

TEST_F(CuriositySNNBridgeTest, GetDriveNull) {
    EXPECT_EQ(curiosity_snn_get_drive(nullptr, nullptr), -1);
    curiosity_drive_t drive;
    EXPECT_EQ(curiosity_snn_get_drive(nullptr, &drive), -1);
    EXPECT_EQ(curiosity_snn_get_drive(bridge, nullptr), -1);
}

TEST_F(CuriositySNNBridgeTest, GetActivations) {
    float activations[CURIOSITY_DIM_COUNT];
    EXPECT_EQ(curiosity_snn_get_activations(bridge, activations, CURIOSITY_DIM_COUNT), 0);
}

//=============================================================================
// Detection Tests
//=============================================================================

TEST_F(CuriositySNNBridgeTest, CheckNovelty) {
    float level;
    bool detected = curiosity_snn_check_novelty(bridge, &level);
    EXPECT_GE(level, 0.0f);
    EXPECT_LE(level, 1.0f);
}

TEST_F(CuriositySNNBridgeTest, CheckInterest) {
    float level;
    curiosity_snn_check_interest(bridge, &level);
    EXPECT_GE(level, 0.0f);
    EXPECT_LE(level, 1.0f);
}

TEST_F(CuriositySNNBridgeTest, CheckStateChange) {
    float dims1[CURIOSITY_DIM_COUNT] = {0.2f};
    float dims2[CURIOSITY_DIM_COUNT] = {0.9f};

    curiosity_snn_encode_state(bridge, dims1, 1);
    curiosity_snn_simulate(bridge, 10.0f);

    curiosity_snn_encode_state(bridge, dims2, 1);
    curiosity_snn_simulate(bridge, 10.0f);

    float magnitude;
    curiosity_snn_check_state_change(bridge, &magnitude);
}

//=============================================================================
// State Query Tests
//=============================================================================

TEST_F(CuriositySNNBridgeTest, GetDimState) {
    curiosity_dim_state_t state;
    EXPECT_EQ(curiosity_snn_get_dim_state(bridge, 0, &state), 0);
}

TEST_F(CuriositySNNBridgeTest, GetDimStateInvalidDim) {
    curiosity_dim_state_t state;
    EXPECT_EQ(curiosity_snn_get_dim_state(bridge, 100, &state), -1);
}

TEST_F(CuriositySNNBridgeTest, GetBridgeState) {
    curiosity_snn_bridge_state_t state;
    EXPECT_EQ(curiosity_snn_get_state(bridge, &state), 0);
    EXPECT_EQ(state.state, CURIOSITY_SNN_STATE_IDLE);
}

TEST_F(CuriositySNNBridgeTest, GetStats) {
    curiosity_snn_stats_t stats;
    EXPECT_EQ(curiosity_snn_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.total_evaluations, 0u);
}

TEST_F(CuriositySNNBridgeTest, ResetStats) {
    float dims[1] = {0.5f};
    curiosity_snn_encode_state(bridge, dims, 1);
    curiosity_snn_simulate(bridge, 10.0f);

    curiosity_snn_stats_t stats;
    curiosity_snn_get_stats(bridge, &stats);
    EXPECT_GT(stats.total_evaluations, 0u);

    EXPECT_EQ(curiosity_snn_reset_stats(bridge), 0);
    curiosity_snn_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_evaluations, 0u);
}

TEST_F(CuriositySNNBridgeTest, GetExploration) {
    float exploration = curiosity_snn_get_exploration(bridge);
    EXPECT_GE(exploration, 0.0f);
    EXPECT_LE(exploration, 1.0f);
}

TEST_F(CuriositySNNBridgeTest, GetTotalActivity) {
    float activity = curiosity_snn_get_total_activity(bridge);
    EXPECT_GE(activity, 0.0f);
}

//=============================================================================
// Callback Tests
//=============================================================================

static int novelty_callback_count = 0;
static void test_novelty_callback(curiosity_snn_bridge_t*, float, uint64_t, void*) {
    novelty_callback_count++;
}

TEST_F(CuriositySNNBridgeTest, RegisterNoveltyCallback) {
    EXPECT_EQ(curiosity_snn_register_novelty_callback(bridge, test_novelty_callback, nullptr), 0);
}

TEST_F(CuriositySNNBridgeTest, RegisterDriveCallback) {
    EXPECT_EQ(curiosity_snn_register_drive_callback(bridge, nullptr, nullptr), 0);
}

TEST_F(CuriositySNNBridgeTest, RegisterInterestCallback) {
    EXPECT_EQ(curiosity_snn_register_interest_callback(bridge, nullptr, nullptr), 0);
}

//=============================================================================
// Bio-Async Tests
//=============================================================================

TEST_F(CuriositySNNBridgeTest, BioAsyncNotEnabled) {
    EXPECT_EQ(curiosity_snn_bio_async_connect(bridge), -1);
    EXPECT_FALSE(curiosity_snn_is_bio_async_connected(bridge));
}

TEST_F(CuriositySNNBridgeTest, BioAsyncDisconnectSafe) {
    EXPECT_EQ(curiosity_snn_bio_async_disconnect(bridge), 0);
}

//=============================================================================
// Integration Workflow Tests
//=============================================================================

TEST_F(CuriositySNNBridgeTest, FullWorkflow) {
    // Encode curiosity context
    float dims[CURIOSITY_DIM_COUNT] = {0};
    dims[CURIOSITY_DIM_NOVELTY] = 0.8f;
    dims[CURIOSITY_DIM_SURPRISE] = 0.7f;
    dims[CURIOSITY_DIM_EXPLORATION] = 0.75f;
    dims[CURIOSITY_DIM_INFORMATION_GAIN] = 0.6f;
    dims[CURIOSITY_DIM_INTEREST] = 0.85f;

    int spike_count = curiosity_snn_encode_state(bridge, dims, CURIOSITY_DIM_COUNT);
    EXPECT_GE(spike_count, 0);

    // Simulate
    EXPECT_EQ(curiosity_snn_simulate(bridge, 30.0f), 0);

    // Get drive
    curiosity_drive_t drive;
    EXPECT_EQ(curiosity_snn_get_drive(bridge, &drive), 0);

    // Verify all fields are valid
    EXPECT_GE(drive.novelty_level, 0.0f);
    EXPECT_LE(drive.novelty_level, 1.0f);
    EXPECT_GE(drive.exploration_drive, 0.0f);
    EXPECT_LE(drive.exploration_drive, 1.0f);
    EXPECT_GE(drive.information_gain, 0.0f);
    EXPECT_LE(drive.information_gain, 1.0f);
}

TEST_F(CuriositySNNBridgeTest, MultipleEvaluations) {
    for (int i = 0; i < 10; i++) {
        float dims[CURIOSITY_DIM_COUNT] = {0};
        dims[0] = (float)i / 10.0f;
        dims[1] = 1.0f - (float)i / 10.0f;

        curiosity_snn_encode_state(bridge, dims, 2);
        curiosity_snn_simulate(bridge, 10.0f);
    }

    curiosity_snn_stats_t stats;
    curiosity_snn_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_evaluations, 10u);
}
