/**
 * @file test_tom_snn_bridge.cpp
 * @brief Unit tests for Theory of Mind SNN Bridge
 */

#include <gtest/gtest.h>
#include <cmath>

#include "cognitive/theory_of_mind/nimcp_tom_snn_bridge.h"

class TOMSNNBridgeTest : public ::testing::Test {
protected:
    tom_snn_bridge_t* bridge = nullptr;

    void SetUp() override {
        tom_snn_config_t config = tom_snn_config_default();
        config.enable_bio_async = false;
        bridge = tom_snn_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            tom_snn_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(TOMSNNBridgeTest, CreateWithDefaults) {
    tom_snn_bridge_t* test_bridge = tom_snn_create(nullptr);
    ASSERT_NE(test_bridge, nullptr);
    tom_snn_destroy(test_bridge);
}

TEST_F(TOMSNNBridgeTest, CreateWithConfig) {
    tom_snn_config_t config = tom_snn_config_default();
    config.num_dimensions = 8;
    config.neurons_per_dim = 16;
    tom_snn_bridge_t* test_bridge = tom_snn_create(&config);
    ASSERT_NE(test_bridge, nullptr);
    tom_snn_destroy(test_bridge);
}

TEST_F(TOMSNNBridgeTest, CreateWithInvalidConfig) {
    tom_snn_config_t config = tom_snn_config_default();
    config.num_dimensions = 0;
    tom_snn_bridge_t* test_bridge = tom_snn_create(&config);
    EXPECT_EQ(test_bridge, nullptr);
}

TEST_F(TOMSNNBridgeTest, Reset) {
    EXPECT_EQ(tom_snn_reset(bridge), 0);
}

TEST_F(TOMSNNBridgeTest, ResetNull) {
    EXPECT_EQ(tom_snn_reset(nullptr), -1);
}

TEST_F(TOMSNNBridgeTest, DestroyNull) {
    tom_snn_destroy(nullptr); // Should not crash
}

//=============================================================================
// Default Config Tests
//=============================================================================

TEST_F(TOMSNNBridgeTest, DefaultConfigValues) {
    tom_snn_config_t config = tom_snn_config_default();
    EXPECT_EQ(config.num_dimensions, TOM_DIM_COUNT);
    EXPECT_EQ(config.neurons_per_dim, TOM_SNN_NEURONS_PER_DIM);
    EXPECT_GT(config.dt_ms, 0.0f);
    EXPECT_TRUE(config.enable_mental_simulation);
    EXPECT_TRUE(config.enable_perspective_taking);
}

//=============================================================================
// Encoding Tests
//=============================================================================

TEST_F(TOMSNNBridgeTest, EncodeContext) {
    float dims[TOM_DIM_COUNT] = {0};
    dims[TOM_DIM_BELIEF_STATE] = 0.8f;
    dims[TOM_DIM_INTENTION] = 0.7f;

    int spike_count = tom_snn_encode_context(bridge, dims, TOM_DIM_COUNT);
    EXPECT_GE(spike_count, 0);
}

TEST_F(TOMSNNBridgeTest, EncodeContextNull) {
    EXPECT_EQ(tom_snn_encode_context(nullptr, nullptr, 0), -1);
    EXPECT_EQ(tom_snn_encode_context(bridge, nullptr, 0), -1);
}

TEST_F(TOMSNNBridgeTest, EncodeBelief) {
    int spike_count = tom_snn_encode_belief(bridge, 0.7f, 0.3f);
    EXPECT_GE(spike_count, 0);
}

TEST_F(TOMSNNBridgeTest, EncodeIntention) {
    int spike_count = tom_snn_encode_intention(bridge, 0.8f, 0.6f, 0.5f);
    EXPECT_GE(spike_count, 0);
}

TEST_F(TOMSNNBridgeTest, EncodeEmpathy) {
    int spike_count = tom_snn_encode_empathy(bridge, 0.9f, 0.7f);
    EXPECT_GE(spike_count, 0);
}

//=============================================================================
// Simulation Tests
//=============================================================================

TEST_F(TOMSNNBridgeTest, SimulateBasic) {
    float dims[TOM_DIM_COUNT] = {0.5f};
    tom_snn_encode_context(bridge, dims, 1);
    EXPECT_EQ(tom_snn_simulate(bridge, 10.0f), 0);
}

TEST_F(TOMSNNBridgeTest, SimulateNull) {
    EXPECT_EQ(tom_snn_simulate(nullptr, 10.0f), -1);
}

TEST_F(TOMSNNBridgeTest, SimulateZeroDuration) {
    EXPECT_EQ(tom_snn_simulate(bridge, 0.0f), -1);
}

TEST_F(TOMSNNBridgeTest, SimulateNegativeDuration) {
    EXPECT_EQ(tom_snn_simulate(bridge, -10.0f), -1);
}

TEST_F(TOMSNNBridgeTest, Step) {
    float dims[TOM_DIM_COUNT] = {0.5f};
    tom_snn_encode_context(bridge, dims, 1);
    EXPECT_EQ(tom_snn_step(bridge), 0);
}

TEST_F(TOMSNNBridgeTest, Forward) {
    float inputs[5] = {0.5f, 0.6f, 0.7f, 0.4f, 0.3f};
    int spike_count = tom_snn_forward(bridge, inputs, 5);
    EXPECT_GE(spike_count, 0);
}

//=============================================================================
// Inference Decoding Tests
//=============================================================================

TEST_F(TOMSNNBridgeTest, GetInference) {
    float dims[TOM_DIM_COUNT] = {0};
    dims[TOM_DIM_BELIEF_STATE] = 0.9f;
    dims[TOM_DIM_INTENTION] = 0.85f;
    tom_snn_encode_context(bridge, dims, TOM_DIM_COUNT);
    tom_snn_simulate(bridge, 20.0f);

    tom_inference_t inference;
    EXPECT_EQ(tom_snn_get_inference(bridge, &inference), 0);
    EXPECT_GE(inference.belief_state, 0.0f);
    EXPECT_LE(inference.belief_state, 1.0f);
    EXPECT_GE(inference.confidence, 0.0f);
    EXPECT_LE(inference.confidence, 1.0f);
}

TEST_F(TOMSNNBridgeTest, GetInferenceNull) {
    EXPECT_EQ(tom_snn_get_inference(nullptr, nullptr), -1);
    tom_inference_t inference;
    EXPECT_EQ(tom_snn_get_inference(nullptr, &inference), -1);
    EXPECT_EQ(tom_snn_get_inference(bridge, nullptr), -1);
}

TEST_F(TOMSNNBridgeTest, GetActivations) {
    float activations[TOM_DIM_COUNT];
    EXPECT_EQ(tom_snn_get_activations(bridge, activations, TOM_DIM_COUNT), 0);
}

//=============================================================================
// Detection Tests
//=============================================================================

TEST_F(TOMSNNBridgeTest, CheckDeception) {
    float level;
    bool deception_detected = tom_snn_check_deception(bridge, &level);
    EXPECT_GE(level, 0.0f);
    EXPECT_LE(level, 1.0f);
}

TEST_F(TOMSNNBridgeTest, CheckPerspectiveShift) {
    float level;
    tom_snn_check_perspective_shift(bridge, &level);
    EXPECT_GE(level, 0.0f);
}

TEST_F(TOMSNNBridgeTest, CheckEmpathy) {
    tom_snn_encode_empathy(bridge, 0.8f, 0.7f);
    tom_snn_simulate(bridge, 10.0f);

    float level;
    tom_snn_check_empathy(bridge, &level);
    EXPECT_GE(level, 0.0f);
}

//=============================================================================
// State Query Tests
//=============================================================================

TEST_F(TOMSNNBridgeTest, GetDimState) {
    tom_dim_state_t state;
    EXPECT_EQ(tom_snn_get_dim_state(bridge, 0, &state), 0);
}

TEST_F(TOMSNNBridgeTest, GetDimStateInvalidDim) {
    tom_dim_state_t state;
    EXPECT_EQ(tom_snn_get_dim_state(bridge, 100, &state), -1);
}

TEST_F(TOMSNNBridgeTest, GetBridgeState) {
    tom_snn_bridge_state_t state;
    EXPECT_EQ(tom_snn_get_state(bridge, &state), 0);
    EXPECT_EQ(state.state, TOM_SNN_STATE_IDLE);
}

TEST_F(TOMSNNBridgeTest, GetStats) {
    tom_snn_stats_t stats;
    EXPECT_EQ(tom_snn_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.total_evaluations, 0u);
}

TEST_F(TOMSNNBridgeTest, ResetStats) {
    float dims[1] = {0.5f};
    tom_snn_encode_context(bridge, dims, 1);
    tom_snn_simulate(bridge, 10.0f);

    tom_snn_stats_t stats;
    tom_snn_get_stats(bridge, &stats);
    EXPECT_GT(stats.total_evaluations, 0u);

    EXPECT_EQ(tom_snn_reset_stats(bridge), 0);
    tom_snn_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_evaluations, 0u);
}

TEST_F(TOMSNNBridgeTest, GetConfidence) {
    float confidence = tom_snn_get_confidence(bridge);
    EXPECT_GE(confidence, 0.0f);
    EXPECT_LE(confidence, 1.0f);
}

TEST_F(TOMSNNBridgeTest, GetTotalActivity) {
    float activity = tom_snn_get_total_activity(bridge);
    EXPECT_GE(activity, 0.0f);
}

//=============================================================================
// Callback Tests
//=============================================================================

static int deception_callback_count = 0;
static void test_deception_callback(tom_snn_bridge_t*, float, uint64_t, void*) {
    deception_callback_count++;
}

TEST_F(TOMSNNBridgeTest, RegisterDeceptionCallback) {
    EXPECT_EQ(tom_snn_register_deception_callback(bridge, test_deception_callback, nullptr), 0);
}

TEST_F(TOMSNNBridgeTest, RegisterInferenceCallback) {
    EXPECT_EQ(tom_snn_register_inference_callback(bridge, nullptr, nullptr), 0);
}

TEST_F(TOMSNNBridgeTest, RegisterPerspectiveCallback) {
    EXPECT_EQ(tom_snn_register_perspective_callback(bridge, nullptr, nullptr), 0);
}

//=============================================================================
// Bio-Async Tests
//=============================================================================

TEST_F(TOMSNNBridgeTest, BioAsyncNotEnabled) {
    EXPECT_EQ(tom_snn_bio_async_connect(bridge), -1);
    EXPECT_FALSE(tom_snn_is_bio_async_connected(bridge));
}

TEST_F(TOMSNNBridgeTest, BioAsyncDisconnectSafe) {
    EXPECT_EQ(tom_snn_bio_async_disconnect(bridge), 0);
}

//=============================================================================
// Integration Workflow Tests
//=============================================================================

TEST_F(TOMSNNBridgeTest, FullWorkflow) {
    // Encode social context
    float dims[TOM_DIM_COUNT] = {0};
    dims[TOM_DIM_BELIEF_STATE] = 0.8f;
    dims[TOM_DIM_DESIRE_STATE] = 0.7f;
    dims[TOM_DIM_INTENTION] = 0.75f;
    dims[TOM_DIM_PERSPECTIVE] = 0.9f;
    dims[TOM_DIM_EMPATHIC_ACCURACY] = 0.6f;

    int spike_count = tom_snn_encode_context(bridge, dims, TOM_DIM_COUNT);
    EXPECT_GE(spike_count, 0);

    // Simulate
    EXPECT_EQ(tom_snn_simulate(bridge, 30.0f), 0);

    // Get inference
    tom_inference_t inference;
    EXPECT_EQ(tom_snn_get_inference(bridge, &inference), 0);

    // Verify all fields are valid
    EXPECT_GE(inference.belief_state, 0.0f);
    EXPECT_LE(inference.belief_state, 1.0f);
    EXPECT_GE(inference.desire_state, 0.0f);
    EXPECT_LE(inference.desire_state, 1.0f);
    EXPECT_GE(inference.confidence, 0.0f);
    EXPECT_LE(inference.confidence, 1.0f);
}

TEST_F(TOMSNNBridgeTest, MultipleEvaluations) {
    for (int i = 0; i < 10; i++) {
        float dims[TOM_DIM_COUNT] = {0};
        dims[0] = (float)i / 10.0f;
        dims[1] = 1.0f - (float)i / 10.0f;

        tom_snn_encode_context(bridge, dims, 2);
        tom_snn_simulate(bridge, 10.0f);
    }

    tom_snn_stats_t stats;
    tom_snn_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_evaluations, 10u);
}

TEST_F(TOMSNNBridgeTest, FalseBeliefDetection) {
    // Encode belief discrepancy (self believes X, other believes Y)
    tom_snn_encode_belief(bridge, 0.9f, 0.2f);  // Large discrepancy
    tom_snn_simulate(bridge, 20.0f);

    tom_inference_t inference;
    tom_snn_get_inference(bridge, &inference);

    // With large belief discrepancy, deception confidence should be higher
    EXPECT_GE(inference.deception_confidence, 0.0f);
}
