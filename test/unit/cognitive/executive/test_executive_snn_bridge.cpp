/**
 * @file test_executive_snn_bridge.cpp
 * @brief Unit tests for Executive SNN Bridge
 */

#include <gtest/gtest.h>
#include <cmath>

#include "cognitive/executive/nimcp_executive_snn_bridge.h"

class ExecutiveSNNBridgeTest : public ::testing::Test {
protected:
    executive_snn_bridge_t* bridge = nullptr;

    void SetUp() override {
        executive_snn_config_t config = executive_snn_config_default();
        config.enable_bio_async = false;
        bridge = executive_snn_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            executive_snn_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(ExecutiveSNNBridgeTest, CreateWithDefaults) {
    executive_snn_bridge_t* test_bridge = executive_snn_create(nullptr);
    ASSERT_NE(test_bridge, nullptr);
    executive_snn_destroy(test_bridge);
}

TEST_F(ExecutiveSNNBridgeTest, CreateWithConfig) {
    executive_snn_config_t config = executive_snn_config_default();
    config.num_dimensions = 8;
    config.neurons_per_dim = 16;
    executive_snn_bridge_t* test_bridge = executive_snn_create(&config);
    ASSERT_NE(test_bridge, nullptr);
    executive_snn_destroy(test_bridge);
}

TEST_F(ExecutiveSNNBridgeTest, CreateWithInvalidConfig) {
    executive_snn_config_t config = executive_snn_config_default();
    config.num_dimensions = 0;
    executive_snn_bridge_t* test_bridge = executive_snn_create(&config);
    EXPECT_EQ(test_bridge, nullptr);
}

TEST_F(ExecutiveSNNBridgeTest, Reset) {
    EXPECT_EQ(executive_snn_reset(bridge), 0);
}

TEST_F(ExecutiveSNNBridgeTest, ResetNull) {
    EXPECT_EQ(executive_snn_reset(nullptr), -1);
}

TEST_F(ExecutiveSNNBridgeTest, DestroyNull) {
    executive_snn_destroy(nullptr); // Should not crash
}

//=============================================================================
// Default Config Tests
//=============================================================================

TEST_F(ExecutiveSNNBridgeTest, DefaultConfigValues) {
    executive_snn_config_t config = executive_snn_config_default();
    EXPECT_EQ(config.num_dimensions, EXEC_DIM_COUNT);
    EXPECT_EQ(config.neurons_per_dim, EXECUTIVE_SNN_NEURONS_PER_DIM);
    EXPECT_GT(config.dt_ms, 0.0f);
    EXPECT_TRUE(config.enable_conflict_detection);
}

//=============================================================================
// Encoding Tests
//=============================================================================

TEST_F(ExecutiveSNNBridgeTest, EncodeState) {
    float dims[EXEC_DIM_COUNT] = {0};
    dims[EXEC_DIM_INHIBITION] = 0.8f;
    dims[EXEC_DIM_WORKING_MEMORY] = 0.7f;

    int spike_count = executive_snn_encode_state(bridge, dims, EXEC_DIM_COUNT);
    EXPECT_GE(spike_count, 0);
}

TEST_F(ExecutiveSNNBridgeTest, EncodeStateNull) {
    EXPECT_EQ(executive_snn_encode_state(nullptr, nullptr, 0), -1);
    EXPECT_EQ(executive_snn_encode_state(bridge, nullptr, 0), -1);
}

TEST_F(ExecutiveSNNBridgeTest, EncodeInhibition) {
    int spike_count = executive_snn_encode_inhibition(bridge, 0.9f, 0.8f);
    EXPECT_GE(spike_count, 0);
}

TEST_F(ExecutiveSNNBridgeTest, EncodeTask) {
    int spike_count = executive_snn_encode_task(bridge, 0.7f, 3);
    EXPECT_GE(spike_count, 0);
}

TEST_F(ExecutiveSNNBridgeTest, EncodeConflict) {
    int spike_count = executive_snn_encode_conflict(bridge, 0.8f, 1);
    EXPECT_GE(spike_count, 0);
}

//=============================================================================
// Simulation Tests
//=============================================================================

TEST_F(ExecutiveSNNBridgeTest, SimulateBasic) {
    float dims[EXEC_DIM_COUNT] = {0.5f};
    executive_snn_encode_state(bridge, dims, 1);
    EXPECT_EQ(executive_snn_simulate(bridge, 10.0f), 0);
}

TEST_F(ExecutiveSNNBridgeTest, SimulateNull) {
    EXPECT_EQ(executive_snn_simulate(nullptr, 10.0f), -1);
}

TEST_F(ExecutiveSNNBridgeTest, SimulateZeroDuration) {
    EXPECT_EQ(executive_snn_simulate(bridge, 0.0f), -1);
}

TEST_F(ExecutiveSNNBridgeTest, SimulateNegativeDuration) {
    EXPECT_EQ(executive_snn_simulate(bridge, -10.0f), -1);
}

TEST_F(ExecutiveSNNBridgeTest, Step) {
    float dims[EXEC_DIM_COUNT] = {0.5f};
    executive_snn_encode_state(bridge, dims, 1);
    EXPECT_EQ(executive_snn_step(bridge), 0);
}

TEST_F(ExecutiveSNNBridgeTest, Forward) {
    float inputs[5] = {0.5f, 0.6f, 0.7f, 0.4f, 0.3f};
    int spike_count = executive_snn_forward(bridge, inputs, 5);
    EXPECT_GE(spike_count, 0);
}

//=============================================================================
// Control Output Tests
//=============================================================================

TEST_F(ExecutiveSNNBridgeTest, GetControlOutput) {
    float dims[EXEC_DIM_COUNT] = {0};
    dims[EXEC_DIM_INHIBITION] = 0.9f;
    dims[EXEC_DIM_WORKING_MEMORY] = 0.7f;
    executive_snn_encode_state(bridge, dims, EXEC_DIM_COUNT);
    executive_snn_simulate(bridge, 20.0f);

    executive_control_output_t output;
    EXPECT_EQ(executive_snn_get_control_output(bridge, &output), 0);
    EXPECT_GE(output.inhibition_level, 0.0f);
    EXPECT_LE(output.inhibition_level, 1.0f);
    EXPECT_GE(output.flexibility_level, 0.0f);
    EXPECT_LE(output.flexibility_level, 1.0f);
}

TEST_F(ExecutiveSNNBridgeTest, GetControlOutputNull) {
    EXPECT_EQ(executive_snn_get_control_output(nullptr, nullptr), -1);
    executive_control_output_t output;
    EXPECT_EQ(executive_snn_get_control_output(nullptr, &output), -1);
    EXPECT_EQ(executive_snn_get_control_output(bridge, nullptr), -1);
}

TEST_F(ExecutiveSNNBridgeTest, GetActivations) {
    float activations[EXEC_DIM_COUNT];
    EXPECT_EQ(executive_snn_get_activations(bridge, activations, EXEC_DIM_COUNT), 0);
}

//=============================================================================
// Detection Tests
//=============================================================================

TEST_F(ExecutiveSNNBridgeTest, CheckConflict) {
    float level;
    bool conflict = executive_snn_check_conflict(bridge, &level);
    EXPECT_GE(level, 0.0f);
    EXPECT_LE(level, 1.0f);
}

TEST_F(ExecutiveSNNBridgeTest, CheckError) {
    executive_snn_encode_conflict(bridge, 0.8f, 1);
    executive_snn_simulate(bridge, 10.0f);

    float level;
    executive_snn_check_error(bridge, &level);
    EXPECT_GE(level, 0.0f);
}

TEST_F(ExecutiveSNNBridgeTest, CheckGoalChange) {
    float dims1[EXEC_DIM_COUNT] = {0.2f};
    float dims2[EXEC_DIM_COUNT] = {0.9f};

    executive_snn_encode_state(bridge, dims1, 1);
    executive_snn_simulate(bridge, 10.0f);

    executive_snn_encode_state(bridge, dims2, 1);
    executive_snn_simulate(bridge, 10.0f);

    float magnitude;
    executive_snn_check_goal_change(bridge, &magnitude);
}

//=============================================================================
// State Query Tests
//=============================================================================

TEST_F(ExecutiveSNNBridgeTest, GetDimState) {
    executive_dim_state_t state;
    EXPECT_EQ(executive_snn_get_dim_state(bridge, 0, &state), 0);
}

TEST_F(ExecutiveSNNBridgeTest, GetDimStateInvalidDim) {
    executive_dim_state_t state;
    EXPECT_EQ(executive_snn_get_dim_state(bridge, 100, &state), -1);
}

TEST_F(ExecutiveSNNBridgeTest, GetBridgeState) {
    executive_snn_bridge_state_t state;
    EXPECT_EQ(executive_snn_get_state(bridge, &state), 0);
    EXPECT_EQ(state.state, EXECUTIVE_SNN_STATE_IDLE);
}

TEST_F(ExecutiveSNNBridgeTest, GetStats) {
    executive_snn_stats_t stats;
    EXPECT_EQ(executive_snn_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.total_evaluations, 0u);
}

TEST_F(ExecutiveSNNBridgeTest, ResetStats) {
    float dims[1] = {0.5f};
    executive_snn_encode_state(bridge, dims, 1);
    executive_snn_simulate(bridge, 10.0f);

    executive_snn_stats_t stats;
    executive_snn_get_stats(bridge, &stats);
    EXPECT_GT(stats.total_evaluations, 0u);

    EXPECT_EQ(executive_snn_reset_stats(bridge), 0);
    executive_snn_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_evaluations, 0u);
}

TEST_F(ExecutiveSNNBridgeTest, GetInhibition) {
    float inhibition = executive_snn_get_inhibition(bridge);
    EXPECT_GE(inhibition, 0.0f);
    EXPECT_LE(inhibition, 1.0f);
}

TEST_F(ExecutiveSNNBridgeTest, GetTotalActivity) {
    float activity = executive_snn_get_total_activity(bridge);
    EXPECT_GE(activity, 0.0f);
}

//=============================================================================
// Callback Tests
//=============================================================================

static int conflict_callback_count = 0;
static void test_conflict_callback(executive_snn_bridge_t*, float, uint64_t, void*) {
    conflict_callback_count++;
}

TEST_F(ExecutiveSNNBridgeTest, RegisterConflictCallback) {
    EXPECT_EQ(executive_snn_register_conflict_callback(bridge, test_conflict_callback, nullptr), 0);
}

TEST_F(ExecutiveSNNBridgeTest, RegisterControlCallback) {
    EXPECT_EQ(executive_snn_register_control_callback(bridge, nullptr, nullptr), 0);
}

TEST_F(ExecutiveSNNBridgeTest, RegisterErrorCallback) {
    EXPECT_EQ(executive_snn_register_error_callback(bridge, nullptr, nullptr), 0);
}

//=============================================================================
// Bio-Async Tests
//=============================================================================

TEST_F(ExecutiveSNNBridgeTest, BioAsyncNotEnabled) {
    EXPECT_EQ(executive_snn_bio_async_connect(bridge), -1);
    EXPECT_FALSE(executive_snn_is_bio_async_connected(bridge));
}

TEST_F(ExecutiveSNNBridgeTest, BioAsyncDisconnectSafe) {
    EXPECT_EQ(executive_snn_bio_async_disconnect(bridge), 0);
}

//=============================================================================
// Integration Workflow Tests
//=============================================================================

TEST_F(ExecutiveSNNBridgeTest, FullWorkflow) {
    // Encode executive context
    float dims[EXEC_DIM_COUNT] = {0};
    dims[EXEC_DIM_INHIBITION] = 0.8f;
    dims[EXEC_DIM_WORKING_MEMORY] = 0.6f;
    dims[EXEC_DIM_FLEXIBILITY] = 0.75f;
    dims[EXEC_DIM_PLANNING] = 0.7f;
    dims[EXEC_DIM_GOAL_MAINTENANCE] = 0.85f;

    int spike_count = executive_snn_encode_state(bridge, dims, EXEC_DIM_COUNT);
    EXPECT_GE(spike_count, 0);

    // Simulate
    EXPECT_EQ(executive_snn_simulate(bridge, 30.0f), 0);

    // Get control output
    executive_control_output_t output;
    EXPECT_EQ(executive_snn_get_control_output(bridge, &output), 0);

    // Verify all fields are valid
    EXPECT_GE(output.inhibition_level, 0.0f);
    EXPECT_LE(output.inhibition_level, 1.0f);
    EXPECT_GE(output.working_memory_load, 0.0f);
    EXPECT_LE(output.working_memory_load, 1.0f);
    EXPECT_GE(output.flexibility_level, 0.0f);
    EXPECT_LE(output.flexibility_level, 1.0f);
}

TEST_F(ExecutiveSNNBridgeTest, MultipleEvaluations) {
    for (int i = 0; i < 10; i++) {
        float dims[EXEC_DIM_COUNT] = {0};
        dims[0] = (float)i / 10.0f;
        dims[1] = 1.0f - (float)i / 10.0f;

        executive_snn_encode_state(bridge, dims, 2);
        executive_snn_simulate(bridge, 10.0f);
    }

    executive_snn_stats_t stats;
    executive_snn_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_evaluations, 10u);
}
