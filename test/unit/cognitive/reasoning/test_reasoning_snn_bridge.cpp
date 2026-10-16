/**
 * @file test_reasoning_snn_bridge.cpp
 * @brief Unit tests for Reasoning SNN Bridge
 */

#include <gtest/gtest.h>
#include <cmath>

#include "cognitive/reasoning/nimcp_reasoning_snn_bridge.h"

class ReasoningSNNBridgeTest : public ::testing::Test {
protected:
    reasoning_snn_bridge_t* bridge = nullptr;

    void SetUp() override {
        reasoning_snn_config_t config = reasoning_snn_config_default();
        config.enable_bio_async = false;
        bridge = reasoning_snn_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            reasoning_snn_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(ReasoningSNNBridgeTest, CreateWithDefaults) {
    reasoning_snn_bridge_t* test_bridge = reasoning_snn_create(nullptr);
    ASSERT_NE(test_bridge, nullptr);
    reasoning_snn_destroy(test_bridge);
}

TEST_F(ReasoningSNNBridgeTest, CreateWithConfig) {
    reasoning_snn_config_t config = reasoning_snn_config_default();
    config.num_dimensions = 8;
    config.neurons_per_dim = 16;
    reasoning_snn_bridge_t* test_bridge = reasoning_snn_create(&config);
    ASSERT_NE(test_bridge, nullptr);
    reasoning_snn_destroy(test_bridge);
}

TEST_F(ReasoningSNNBridgeTest, CreateWithInvalidConfig) {
    reasoning_snn_config_t config = reasoning_snn_config_default();
    config.num_dimensions = 0;
    reasoning_snn_bridge_t* test_bridge = reasoning_snn_create(&config);
    EXPECT_EQ(test_bridge, nullptr);
}

TEST_F(ReasoningSNNBridgeTest, Reset) {
    EXPECT_EQ(reasoning_snn_reset(bridge), 0);
}

TEST_F(ReasoningSNNBridgeTest, ResetNull) {
    EXPECT_EQ(reasoning_snn_reset(nullptr), -1);
}

TEST_F(ReasoningSNNBridgeTest, DestroyNull) {
    reasoning_snn_destroy(nullptr); // Should not crash
}

//=============================================================================
// Default Config Tests
//=============================================================================

TEST_F(ReasoningSNNBridgeTest, DefaultConfigValues) {
    reasoning_snn_config_t config = reasoning_snn_config_default();
    EXPECT_EQ(config.num_dimensions, REASON_DIM_COUNT);
    EXPECT_EQ(config.neurons_per_dim, REASONING_SNN_NEURONS_PER_DIM);
    EXPECT_GT(config.dt_ms, 0.0f);
    EXPECT_TRUE(config.enable_causal_chains);
}

//=============================================================================
// Encoding Tests
//=============================================================================

TEST_F(ReasoningSNNBridgeTest, EncodeState) {
    float dims[REASON_DIM_COUNT] = {0};
    dims[REASON_DIM_DEDUCTION] = 0.8f;
    dims[REASON_DIM_LOGICAL_VALIDITY] = 0.7f;

    int spike_count = reasoning_snn_encode_state(bridge, dims, REASON_DIM_COUNT);
    EXPECT_GE(spike_count, 0);
}

TEST_F(ReasoningSNNBridgeTest, EncodeStateNull) {
    EXPECT_EQ(reasoning_snn_encode_state(nullptr, nullptr, 0), -1);
    EXPECT_EQ(reasoning_snn_encode_state(bridge, nullptr, 0), -1);
}

TEST_F(ReasoningSNNBridgeTest, EncodeDeduction) {
    int spike_count = reasoning_snn_encode_deduction(bridge, 0.9f, 0.8f);
    EXPECT_GE(spike_count, 0);
}

TEST_F(ReasoningSNNBridgeTest, EncodeCausal) {
    int spike_count = reasoning_snn_encode_causal(bridge, 0.7f, 0.6f);
    EXPECT_GE(spike_count, 0);
}

TEST_F(ReasoningSNNBridgeTest, EncodeEvidence) {
    int spike_count = reasoning_snn_encode_evidence(bridge, 0.8f, 5);
    EXPECT_GE(spike_count, 0);
}

//=============================================================================
// Simulation Tests
//=============================================================================

TEST_F(ReasoningSNNBridgeTest, SimulateBasic) {
    float dims[REASON_DIM_COUNT] = {0.5f};
    reasoning_snn_encode_state(bridge, dims, 1);
    EXPECT_EQ(reasoning_snn_simulate(bridge, 10.0f), 0);
}

TEST_F(ReasoningSNNBridgeTest, SimulateNull) {
    EXPECT_EQ(reasoning_snn_simulate(nullptr, 10.0f), -1);
}

TEST_F(ReasoningSNNBridgeTest, SimulateZeroDuration) {
    EXPECT_EQ(reasoning_snn_simulate(bridge, 0.0f), -1);
}

TEST_F(ReasoningSNNBridgeTest, SimulateNegativeDuration) {
    EXPECT_EQ(reasoning_snn_simulate(bridge, -10.0f), -1);
}

TEST_F(ReasoningSNNBridgeTest, Step) {
    float dims[REASON_DIM_COUNT] = {0.5f};
    reasoning_snn_encode_state(bridge, dims, 1);
    EXPECT_EQ(reasoning_snn_step(bridge), 0);
}

TEST_F(ReasoningSNNBridgeTest, Forward) {
    float inputs[5] = {0.5f, 0.6f, 0.7f, 0.4f, 0.3f};
    int spike_count = reasoning_snn_forward(bridge, inputs, 5);
    EXPECT_GE(spike_count, 0);
}

//=============================================================================
// Inference Decoding Tests
//=============================================================================

TEST_F(ReasoningSNNBridgeTest, GetInference) {
    float dims[REASON_DIM_COUNT] = {0};
    dims[REASON_DIM_DEDUCTION] = 0.9f;
    dims[REASON_DIM_LOGICAL_VALIDITY] = 0.85f;
    reasoning_snn_encode_state(bridge, dims, REASON_DIM_COUNT);
    reasoning_snn_simulate(bridge, 20.0f);

    reasoning_inference_t inference;
    EXPECT_EQ(reasoning_snn_get_inference(bridge, &inference), 0);
    EXPECT_GE(inference.deduction_strength, 0.0f);
    EXPECT_LE(inference.deduction_strength, 1.0f);
    EXPECT_GE(inference.logical_validity, 0.0f);
    EXPECT_LE(inference.logical_validity, 1.0f);
}

TEST_F(ReasoningSNNBridgeTest, GetInferenceNull) {
    EXPECT_EQ(reasoning_snn_get_inference(nullptr, nullptr), -1);
    reasoning_inference_t inference;
    EXPECT_EQ(reasoning_snn_get_inference(nullptr, &inference), -1);
    EXPECT_EQ(reasoning_snn_get_inference(bridge, nullptr), -1);
}

TEST_F(ReasoningSNNBridgeTest, GetActivations) {
    float activations[REASON_DIM_COUNT];
    EXPECT_EQ(reasoning_snn_get_activations(bridge, activations, REASON_DIM_COUNT), 0);
}

//=============================================================================
// Detection Tests
//=============================================================================

TEST_F(ReasoningSNNBridgeTest, CheckConflict) {
    float level;
    bool conflict = reasoning_snn_check_conflict(bridge, &level);
    EXPECT_GE(level, 0.0f);
    EXPECT_LE(level, 1.0f);
}

TEST_F(ReasoningSNNBridgeTest, CheckConclusion) {
    float dims[REASON_DIM_COUNT] = {0};
    dims[REASON_DIM_DEDUCTION] = 0.9f;
    dims[REASON_DIM_LOGICAL_VALIDITY] = 0.9f;
    dims[REASON_DIM_EVIDENCE_WEIGHT] = 0.8f;
    reasoning_snn_encode_state(bridge, dims, REASON_DIM_COUNT);
    reasoning_snn_simulate(bridge, 30.0f);

    float validity;
    reasoning_snn_check_conclusion(bridge, &validity);
    EXPECT_GE(validity, 0.0f);
}

TEST_F(ReasoningSNNBridgeTest, CheckCausal) {
    reasoning_snn_encode_causal(bridge, 0.9f, 0.8f);
    reasoning_snn_simulate(bridge, 10.0f);

    float strength;
    reasoning_snn_check_causal(bridge, &strength);
    EXPECT_GE(strength, 0.0f);
}

//=============================================================================
// State Query Tests
//=============================================================================

TEST_F(ReasoningSNNBridgeTest, GetDimState) {
    reasoning_dim_state_t state;
    EXPECT_EQ(reasoning_snn_get_dim_state(bridge, 0, &state), 0);
}

TEST_F(ReasoningSNNBridgeTest, GetDimStateInvalidDim) {
    reasoning_dim_state_t state;
    EXPECT_EQ(reasoning_snn_get_dim_state(bridge, 100, &state), -1);
}

TEST_F(ReasoningSNNBridgeTest, GetBridgeState) {
    reasoning_snn_bridge_state_t state;
    EXPECT_EQ(reasoning_snn_get_state(bridge, &state), 0);
    EXPECT_EQ(state.state, REASONING_SNN_STATE_IDLE);
}

TEST_F(ReasoningSNNBridgeTest, GetStats) {
    reasoning_snn_stats_t stats;
    EXPECT_EQ(reasoning_snn_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.total_evaluations, 0u);
}

TEST_F(ReasoningSNNBridgeTest, ResetStats) {
    float dims[1] = {0.5f};
    reasoning_snn_encode_state(bridge, dims, 1);
    reasoning_snn_simulate(bridge, 10.0f);

    reasoning_snn_stats_t stats;
    reasoning_snn_get_stats(bridge, &stats);
    EXPECT_GT(stats.total_evaluations, 0u);

    EXPECT_EQ(reasoning_snn_reset_stats(bridge), 0);
    reasoning_snn_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_evaluations, 0u);
}

TEST_F(ReasoningSNNBridgeTest, GetInferenceStrength) {
    float strength = reasoning_snn_get_inference_strength(bridge);
    EXPECT_GE(strength, 0.0f);
    EXPECT_LE(strength, 1.0f);
}

TEST_F(ReasoningSNNBridgeTest, GetTotalActivity) {
    float activity = reasoning_snn_get_total_activity(bridge);
    EXPECT_GE(activity, 0.0f);
}

//=============================================================================
// Callback Tests
//=============================================================================

static int conflict_callback_count = 0;
static void test_conflict_callback(reasoning_snn_bridge_t*, float, uint64_t, void*) {
    conflict_callback_count++;
}

TEST_F(ReasoningSNNBridgeTest, RegisterConflictCallback) {
    EXPECT_EQ(reasoning_snn_register_conflict_callback(bridge, test_conflict_callback, nullptr), 0);
}

TEST_F(ReasoningSNNBridgeTest, RegisterInferenceCallback) {
    EXPECT_EQ(reasoning_snn_register_inference_callback(bridge, nullptr, nullptr), 0);
}

TEST_F(ReasoningSNNBridgeTest, RegisterConclusionCallback) {
    EXPECT_EQ(reasoning_snn_register_conclusion_callback(bridge, nullptr, nullptr), 0);
}

//=============================================================================
// Bio-Async Tests
//=============================================================================

TEST_F(ReasoningSNNBridgeTest, BioAsyncNotEnabled) {
    EXPECT_EQ(reasoning_snn_bio_async_connect(bridge), -1);
    EXPECT_FALSE(reasoning_snn_is_bio_async_connected(bridge));
}

TEST_F(ReasoningSNNBridgeTest, BioAsyncDisconnectSafe) {
    EXPECT_EQ(reasoning_snn_bio_async_disconnect(bridge), 0);
}

//=============================================================================
// Integration Workflow Tests
//=============================================================================

TEST_F(ReasoningSNNBridgeTest, FullWorkflow) {
    // Encode reasoning context
    float dims[REASON_DIM_COUNT] = {0};
    dims[REASON_DIM_DEDUCTION] = 0.8f;
    dims[REASON_DIM_INDUCTION] = 0.6f;
    dims[REASON_DIM_CAUSAL] = 0.75f;
    dims[REASON_DIM_LOGICAL_VALIDITY] = 0.9f;
    dims[REASON_DIM_EVIDENCE_WEIGHT] = 0.7f;

    int spike_count = reasoning_snn_encode_state(bridge, dims, REASON_DIM_COUNT);
    EXPECT_GE(spike_count, 0);

    // Simulate
    EXPECT_EQ(reasoning_snn_simulate(bridge, 30.0f), 0);

    // Get inference
    reasoning_inference_t inference;
    EXPECT_EQ(reasoning_snn_get_inference(bridge, &inference), 0);

    // Verify all fields are valid
    EXPECT_GE(inference.deduction_strength, 0.0f);
    EXPECT_LE(inference.deduction_strength, 1.0f);
    EXPECT_GE(inference.induction_strength, 0.0f);
    EXPECT_LE(inference.induction_strength, 1.0f);
    EXPECT_GE(inference.causal_confidence, 0.0f);
    EXPECT_LE(inference.causal_confidence, 1.0f);
}

TEST_F(ReasoningSNNBridgeTest, MultipleEvaluations) {
    for (int i = 0; i < 10; i++) {
        float dims[REASON_DIM_COUNT] = {0};
        dims[0] = (float)i / 10.0f;
        dims[1] = 1.0f - (float)i / 10.0f;

        reasoning_snn_encode_state(bridge, dims, 2);
        reasoning_snn_simulate(bridge, 10.0f);
    }

    reasoning_snn_stats_t stats;
    reasoning_snn_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_evaluations, 10u);
}

TEST_F(ReasoningSNNBridgeTest, DeductiveReasoningChain) {
    // Simulate a deductive reasoning chain
    reasoning_snn_encode_deduction(bridge, 1.0f, 0.95f);
    reasoning_snn_simulate(bridge, 20.0f);

    reasoning_inference_t inference;
    reasoning_snn_get_inference(bridge, &inference);
    EXPECT_GE(inference.deduction_strength, 0.0f);
}

TEST_F(ReasoningSNNBridgeTest, CausalReasoningChain) {
    // Simulate causal reasoning
    reasoning_snn_encode_causal(bridge, 0.9f, 0.85f);
    reasoning_snn_simulate(bridge, 20.0f);

    float causal_strength;
    bool has_causal = reasoning_snn_check_causal(bridge, &causal_strength);
    EXPECT_GE(causal_strength, 0.0f);
}
