/**
 * @file test_introspection_snn_bridge.cpp
 * @brief Unit tests for Introspection SNN Bridge
 */

#include <gtest/gtest.h>
#include <cmath>

#include "cognitive/introspection/nimcp_introspection_snn_bridge.h"

class IntrospectionSNNBridgeTest : public ::testing::Test {
protected:
    introspection_snn_bridge_t* bridge = nullptr;

    void SetUp() override {
        introspection_snn_config_t config = introspection_snn_config_default();
        config.enable_bio_async = false;
        bridge = introspection_snn_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            introspection_snn_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(IntrospectionSNNBridgeTest, CreateWithDefaults) {
    introspection_snn_bridge_t* test_bridge = introspection_snn_create(nullptr);
    ASSERT_NE(test_bridge, nullptr);
    introspection_snn_destroy(test_bridge);
}

TEST_F(IntrospectionSNNBridgeTest, CreateWithConfig) {
    introspection_snn_config_t config = introspection_snn_config_default();
    config.num_dimensions = 8;
    config.neurons_per_dim = 16;
    introspection_snn_bridge_t* test_bridge = introspection_snn_create(&config);
    ASSERT_NE(test_bridge, nullptr);
    introspection_snn_destroy(test_bridge);
}

TEST_F(IntrospectionSNNBridgeTest, CreateWithInvalidConfig) {
    introspection_snn_config_t config = introspection_snn_config_default();
    config.num_dimensions = 0;
    introspection_snn_bridge_t* test_bridge = introspection_snn_create(&config);
    EXPECT_EQ(test_bridge, nullptr);
}

TEST_F(IntrospectionSNNBridgeTest, Reset) {
    EXPECT_EQ(introspection_snn_reset(bridge), 0);
}

TEST_F(IntrospectionSNNBridgeTest, ResetNull) {
    EXPECT_EQ(introspection_snn_reset(nullptr), -1);
}

TEST_F(IntrospectionSNNBridgeTest, DestroyNull) {
    introspection_snn_destroy(nullptr); // Should not crash
}

//=============================================================================
// Default Config Tests
//=============================================================================

TEST_F(IntrospectionSNNBridgeTest, DefaultConfigValues) {
    introspection_snn_config_t config = introspection_snn_config_default();
    EXPECT_EQ(config.num_dimensions, INTROSPECTION_DIM_COUNT);
    EXPECT_EQ(config.neurons_per_dim, INTROSPECTION_SNN_NEURONS_PER_DIM);
    EXPECT_GT(config.dt_ms, 0.0f);
    EXPECT_TRUE(config.enable_metacognition);
}

//=============================================================================
// Encoding Tests
//=============================================================================

TEST_F(IntrospectionSNNBridgeTest, EncodeState) {
    float dims[INTROSPECTION_DIM_COUNT] = {0};
    dims[INTROSPECTION_DIM_CERTAINTY] = 0.8f;
    dims[INTROSPECTION_DIM_CONFIDENCE] = 0.7f;

    int spike_count = introspection_snn_encode_state(bridge, dims, INTROSPECTION_DIM_COUNT);
    EXPECT_GE(spike_count, 0);
}

TEST_F(IntrospectionSNNBridgeTest, EncodeStateNull) {
    EXPECT_EQ(introspection_snn_encode_state(nullptr, nullptr, 0), -1);
    EXPECT_EQ(introspection_snn_encode_state(bridge, nullptr, 0), -1);
}

TEST_F(IntrospectionSNNBridgeTest, EncodeUncertainty) {
    int spike_count = introspection_snn_encode_uncertainty(bridge, 0.3f, 0.2f);
    EXPECT_GE(spike_count, 0);
}

TEST_F(IntrospectionSNNBridgeTest, EncodePattern) {
    int spike_count = introspection_snn_encode_pattern(bridge, 0.9f, 5);
    EXPECT_GE(spike_count, 0);
}

TEST_F(IntrospectionSNNBridgeTest, EncodeError) {
    int spike_count = introspection_snn_encode_error(bridge, 0.6f, 1);
    EXPECT_GE(spike_count, 0);
}

//=============================================================================
// Simulation Tests
//=============================================================================

TEST_F(IntrospectionSNNBridgeTest, SimulateBasic) {
    float dims[INTROSPECTION_DIM_COUNT] = {0.5f};
    introspection_snn_encode_state(bridge, dims, 1);
    EXPECT_EQ(introspection_snn_simulate(bridge, 10.0f), 0);
}

TEST_F(IntrospectionSNNBridgeTest, SimulateNull) {
    EXPECT_EQ(introspection_snn_simulate(nullptr, 10.0f), -1);
}

TEST_F(IntrospectionSNNBridgeTest, SimulateZeroDuration) {
    EXPECT_EQ(introspection_snn_simulate(bridge, 0.0f), -1);
}

TEST_F(IntrospectionSNNBridgeTest, SimulateNegativeDuration) {
    EXPECT_EQ(introspection_snn_simulate(bridge, -10.0f), -1);
}

TEST_F(IntrospectionSNNBridgeTest, Step) {
    float dims[INTROSPECTION_DIM_COUNT] = {0.5f};
    introspection_snn_encode_state(bridge, dims, 1);
    EXPECT_EQ(introspection_snn_step(bridge), 0);
}

TEST_F(IntrospectionSNNBridgeTest, Forward) {
    float inputs[5] = {0.5f, 0.6f, 0.7f, 0.4f, 0.3f};
    int spike_count = introspection_snn_forward(bridge, inputs, 5);
    EXPECT_GE(spike_count, 0);
}

//=============================================================================
// Insight Decoding Tests
//=============================================================================

TEST_F(IntrospectionSNNBridgeTest, GetInsight) {
    float dims[INTROSPECTION_DIM_COUNT] = {0};
    dims[INTROSPECTION_DIM_CERTAINTY] = 0.9f;
    dims[INTROSPECTION_DIM_CONFIDENCE] = 0.85f;
    introspection_snn_encode_state(bridge, dims, INTROSPECTION_DIM_COUNT);
    introspection_snn_simulate(bridge, 20.0f);

    introspection_insight_t insight;
    EXPECT_EQ(introspection_snn_get_insight(bridge, &insight), 0);
    EXPECT_GE(insight.certainty_level, 0.0f);
    EXPECT_LE(insight.certainty_level, 1.0f);
    EXPECT_GE(insight.confidence, 0.0f);
    EXPECT_LE(insight.confidence, 1.0f);
}

TEST_F(IntrospectionSNNBridgeTest, GetInsightNull) {
    EXPECT_EQ(introspection_snn_get_insight(nullptr, nullptr), -1);
    introspection_insight_t insight;
    EXPECT_EQ(introspection_snn_get_insight(nullptr, &insight), -1);
    EXPECT_EQ(introspection_snn_get_insight(bridge, nullptr), -1);
}

TEST_F(IntrospectionSNNBridgeTest, GetActivations) {
    float activations[INTROSPECTION_DIM_COUNT];
    EXPECT_EQ(introspection_snn_get_activations(bridge, activations, INTROSPECTION_DIM_COUNT), 0);
}

//=============================================================================
// Detection Tests
//=============================================================================

TEST_F(IntrospectionSNNBridgeTest, CheckUncertainty) {
    float level;
    bool high_uncertainty = introspection_snn_check_uncertainty(bridge, &level);
    EXPECT_GE(level, 0.0f);
    EXPECT_LE(level, 1.0f);
}

TEST_F(IntrospectionSNNBridgeTest, CheckError) {
    introspection_snn_encode_error(bridge, 0.8f, 1);
    introspection_snn_simulate(bridge, 10.0f);

    float level;
    introspection_snn_check_error(bridge, &level);
    EXPECT_GE(level, 0.0f);
}

TEST_F(IntrospectionSNNBridgeTest, CheckStateChange) {
    float dims1[INTROSPECTION_DIM_COUNT] = {0.2f};
    float dims2[INTROSPECTION_DIM_COUNT] = {0.9f};

    introspection_snn_encode_state(bridge, dims1, 1);
    introspection_snn_simulate(bridge, 10.0f);

    introspection_snn_encode_state(bridge, dims2, 1);
    introspection_snn_simulate(bridge, 10.0f);

    float magnitude;
    introspection_snn_check_state_change(bridge, &magnitude);
}

//=============================================================================
// State Query Tests
//=============================================================================

TEST_F(IntrospectionSNNBridgeTest, GetDimState) {
    introspection_dim_state_t state;
    EXPECT_EQ(introspection_snn_get_dim_state(bridge, 0, &state), 0);
}

TEST_F(IntrospectionSNNBridgeTest, GetDimStateInvalidDim) {
    introspection_dim_state_t state;
    EXPECT_EQ(introspection_snn_get_dim_state(bridge, 100, &state), -1);
}

TEST_F(IntrospectionSNNBridgeTest, GetBridgeState) {
    introspection_snn_bridge_state_t state;
    EXPECT_EQ(introspection_snn_get_state(bridge, &state), 0);
    EXPECT_EQ(state.state, INTROSPECTION_SNN_STATE_IDLE);
}

TEST_F(IntrospectionSNNBridgeTest, GetStats) {
    introspection_snn_stats_t stats;
    EXPECT_EQ(introspection_snn_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.total_evaluations, 0u);
}

TEST_F(IntrospectionSNNBridgeTest, ResetStats) {
    float dims[1] = {0.5f};
    introspection_snn_encode_state(bridge, dims, 1);
    introspection_snn_simulate(bridge, 10.0f);

    introspection_snn_stats_t stats;
    introspection_snn_get_stats(bridge, &stats);
    EXPECT_GT(stats.total_evaluations, 0u);

    EXPECT_EQ(introspection_snn_reset_stats(bridge), 0);
    introspection_snn_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_evaluations, 0u);
}

TEST_F(IntrospectionSNNBridgeTest, GetConfidence) {
    float confidence = introspection_snn_get_confidence(bridge);
    EXPECT_GE(confidence, 0.0f);
    EXPECT_LE(confidence, 1.0f);
}

TEST_F(IntrospectionSNNBridgeTest, GetTotalActivity) {
    float activity = introspection_snn_get_total_activity(bridge);
    EXPECT_GE(activity, 0.0f);
}

//=============================================================================
// Callback Tests
//=============================================================================

static int uncertainty_callback_count = 0;
static void test_uncertainty_callback(introspection_snn_bridge_t*, float, uint64_t, void*) {
    uncertainty_callback_count++;
}

TEST_F(IntrospectionSNNBridgeTest, RegisterUncertaintyCallback) {
    EXPECT_EQ(introspection_snn_register_uncertainty_callback(bridge, test_uncertainty_callback, nullptr), 0);
}

TEST_F(IntrospectionSNNBridgeTest, RegisterInsightCallback) {
    EXPECT_EQ(introspection_snn_register_insight_callback(bridge, nullptr, nullptr), 0);
}

TEST_F(IntrospectionSNNBridgeTest, RegisterErrorCallback) {
    EXPECT_EQ(introspection_snn_register_error_callback(bridge, nullptr, nullptr), 0);
}

//=============================================================================
// Bio-Async Tests
//=============================================================================

TEST_F(IntrospectionSNNBridgeTest, BioAsyncNotEnabled) {
    EXPECT_EQ(introspection_snn_bio_async_connect(bridge), -1);
    EXPECT_FALSE(introspection_snn_is_bio_async_connected(bridge));
}

TEST_F(IntrospectionSNNBridgeTest, BioAsyncDisconnectSafe) {
    EXPECT_EQ(introspection_snn_bio_async_disconnect(bridge), 0);
}

//=============================================================================
// Integration Workflow Tests
//=============================================================================

TEST_F(IntrospectionSNNBridgeTest, FullWorkflow) {
    // Encode introspective context
    float dims[INTROSPECTION_DIM_COUNT] = {0};
    dims[INTROSPECTION_DIM_CERTAINTY] = 0.8f;
    dims[INTROSPECTION_DIM_UNCERTAINTY] = 0.2f;
    dims[INTROSPECTION_DIM_CONFIDENCE] = 0.75f;
    dims[INTROSPECTION_DIM_ALERTNESS] = 0.9f;
    dims[INTROSPECTION_DIM_METACOGNITION] = 0.6f;

    int spike_count = introspection_snn_encode_state(bridge, dims, INTROSPECTION_DIM_COUNT);
    EXPECT_GE(spike_count, 0);

    // Simulate
    EXPECT_EQ(introspection_snn_simulate(bridge, 30.0f), 0);

    // Get insight
    introspection_insight_t insight;
    EXPECT_EQ(introspection_snn_get_insight(bridge, &insight), 0);

    // Verify all fields are valid
    EXPECT_GE(insight.certainty_level, 0.0f);
    EXPECT_LE(insight.certainty_level, 1.0f);
    EXPECT_GE(insight.uncertainty_level, 0.0f);
    EXPECT_LE(insight.uncertainty_level, 1.0f);
    EXPECT_GE(insight.confidence, 0.0f);
    EXPECT_LE(insight.confidence, 1.0f);
}

TEST_F(IntrospectionSNNBridgeTest, MultipleEvaluations) {
    for (int i = 0; i < 10; i++) {
        float dims[INTROSPECTION_DIM_COUNT] = {0};
        dims[0] = (float)i / 10.0f;
        dims[1] = 1.0f - (float)i / 10.0f;

        introspection_snn_encode_state(bridge, dims, 2);
        introspection_snn_simulate(bridge, 10.0f);
    }

    introspection_snn_stats_t stats;
    introspection_snn_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_evaluations, 10u);
}
