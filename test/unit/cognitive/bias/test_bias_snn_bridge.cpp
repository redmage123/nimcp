/**
 * @file test_bias_snn_bridge.cpp
 * @brief Unit tests for Cognitive Bias-SNN Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

#include "cognitive/bias/nimcp_bias_snn_bridge.h"

class BiasSnnBridgeTest : public ::testing::Test {
protected:
    bias_snn_bridge_t* bridge = nullptr;

    void SetUp() override {
        bias_snn_config_t config = bias_snn_config_default();
        bridge = bias_snn_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            bias_snn_destroy(bridge);
            bridge = nullptr;
        }
    }
};

// ============================================================================
// Lifecycle Tests
// ============================================================================

TEST_F(BiasSnnBridgeTest, CreateWithNullConfig) {
    bias_snn_bridge_t* b = bias_snn_create(nullptr);
    EXPECT_EQ(b, nullptr);
}

TEST_F(BiasSnnBridgeTest, DefaultConfigValid) {
    bias_snn_config_t config = bias_snn_config_default();
    EXPECT_EQ(config.max_bias_types, BIAS_SNN_MAX_BIAS_TYPES);
    EXPECT_EQ(config.neurons_per_type, BIAS_SNN_NEURONS_PER_TYPE);
    EXPECT_GT(config.dt_ms, 0.0f);
    EXPECT_GT(config.bias_detection_threshold, 0.0f);
}

TEST_F(BiasSnnBridgeTest, ResetSucceeds) {
    int result = bias_snn_reset(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(BiasSnnBridgeTest, ResetNullBridge) {
    int result = bias_snn_reset(nullptr);
    EXPECT_EQ(result, -1);
}

// ============================================================================
// Encoding Tests
// ============================================================================

TEST_F(BiasSnnBridgeTest, EncodeEvidenceBasic) {
    float evidence[] = {0.1f, 0.2f, 0.3f};
    int result = bias_snn_encode_evidence(bridge, evidence, 3, 0.5f);
    EXPECT_EQ(result, 0);
}

TEST_F(BiasSnnBridgeTest, EncodeEvidenceNullEvidence) {
    // Should still succeed - just encodes prior
    int result = bias_snn_encode_evidence(bridge, nullptr, 0, 0.5f);
    EXPECT_EQ(result, 0);
}

TEST_F(BiasSnnBridgeTest, EncodeDecisionContext) {
    int result = bias_snn_encode_decision_context(bridge, 0.7f, 0.8f, 0.3f);
    EXPECT_EQ(result, 0);
}

TEST_F(BiasSnnBridgeTest, EncodeDecisionContextNullBridge) {
    int result = bias_snn_encode_decision_context(nullptr, 0.7f, 0.8f, 0.3f);
    EXPECT_EQ(result, -1);
}

TEST_F(BiasSnnBridgeTest, EncodePredictionError) {
    int result = bias_snn_encode_prediction_error(bridge, 0.6f, 0.9f);
    EXPECT_EQ(result, 0);
}

// ============================================================================
// Simulation Tests
// ============================================================================

TEST_F(BiasSnnBridgeTest, SimulateBasic) {
    bias_snn_encode_decision_context(bridge, 0.7f, 0.8f, 0.3f);
    int result = bias_snn_simulate(bridge, 100.0f);
    EXPECT_EQ(result, 0);
}

TEST_F(BiasSnnBridgeTest, SimulateNullBridge) {
    int result = bias_snn_simulate(nullptr, 100.0f);
    EXPECT_EQ(result, -1);
}

TEST_F(BiasSnnBridgeTest, StepBasic) {
    bias_snn_encode_decision_context(bridge, 0.7f, 0.8f, 0.3f);
    int result = bias_snn_step(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(BiasSnnBridgeTest, ForwardBasic) {
    float inputs[] = {0.1f, 0.2f, 0.3f, 0.4f};
    int result = bias_snn_forward(bridge, inputs, 4);
    EXPECT_EQ(result, 0);
}

// ============================================================================
// Detection Tests
// ============================================================================

TEST_F(BiasSnnBridgeTest, DetectBiases) {
    bias_snn_encode_decision_context(bridge, 0.9f, 0.2f, 0.5f);  // Strong anchor
    bias_snn_simulate(bridge, 200.0f);

    bias_snn_output_t output;
    int result = bias_snn_detect_biases(bridge, &output);
    EXPECT_EQ(result, 0);
    EXPECT_GE(output.overall_bias_level, 0.0f);
}

TEST_F(BiasSnnBridgeTest, DetectBiasesNullOutput) {
    int result = bias_snn_detect_biases(bridge, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(BiasSnnBridgeTest, GetBiasLevel) {
    bias_snn_encode_decision_context(bridge, 0.9f, 0.2f, 0.5f);
    bias_snn_simulate(bridge, 100.0f);

    float level = bias_snn_get_bias_level(bridge, BIAS_SNN_TYPE_ANCHORING);
    EXPECT_GE(level, 0.0f);
}

TEST_F(BiasSnnBridgeTest, GetBiasLevelInvalidType) {
    float level = bias_snn_get_bias_level(bridge, (bias_snn_type_t)100);
    EXPECT_FLOAT_EQ(level, 0.0f);
}

TEST_F(BiasSnnBridgeTest, GetOverallBias) {
    float bias = bias_snn_get_overall_bias(bridge);
    EXPECT_GE(bias, 0.0f);
}

TEST_F(BiasSnnBridgeTest, GetConflictLevel) {
    float conflict = bias_snn_get_conflict_level(bridge);
    EXPECT_GE(conflict, 0.0f);
}

TEST_F(BiasSnnBridgeTest, GetDominantBias) {
    bias_snn_type_t dominant = bias_snn_get_dominant_bias(bridge);
    EXPECT_GE(dominant, BIAS_SNN_TYPE_CONFIRMATION);
    EXPECT_LT(dominant, BIAS_SNN_TYPE_COUNT);
}

// ============================================================================
// State Query Tests
// ============================================================================

TEST_F(BiasSnnBridgeTest, GetTypeState) {
    bias_snn_encode_decision_context(bridge, 0.9f, 0.2f, 0.5f);
    bias_snn_simulate(bridge, 100.0f);

    bias_type_state_t state;
    int result = bias_snn_get_type_state(bridge, BIAS_SNN_TYPE_ANCHORING, &state);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(state.type, BIAS_SNN_TYPE_ANCHORING);
}

TEST_F(BiasSnnBridgeTest, GetTypeStateInvalidType) {
    bias_type_state_t state;
    int result = bias_snn_get_type_state(bridge, (bias_snn_type_t)100, &state);
    EXPECT_EQ(result, -1);
}

TEST_F(BiasSnnBridgeTest, GetState) {
    bias_snn_bridge_state_t state;
    int result = bias_snn_get_state(bridge, &state);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(state.state, BIAS_SNN_STATE_IDLE);
}

TEST_F(BiasSnnBridgeTest, GetStats) {
    bias_snn_stats_t stats;
    int result = bias_snn_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
}

TEST_F(BiasSnnBridgeTest, ResetStats) {
    bias_snn_encode_decision_context(bridge, 0.9f, 0.2f, 0.5f);
    bias_snn_simulate(bridge, 100.0f);

    int result = bias_snn_reset_stats(bridge);
    EXPECT_EQ(result, 0);

    bias_snn_stats_t stats;
    bias_snn_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_detections, 0u);
}

// ============================================================================
// Callback Tests
// ============================================================================

static bool detection_callback_called = false;
static void test_detection_callback(bias_snn_bridge_t*, bias_snn_type_t, float, float, void*) {
    detection_callback_called = true;
}

TEST_F(BiasSnnBridgeTest, RegisterDetectionCallback) {
    detection_callback_called = false;
    int result = bias_snn_register_detection_callback(bridge, test_detection_callback, nullptr);
    EXPECT_EQ(result, 0);
}

static bool conflict_callback_called = false;
static void test_conflict_callback(bias_snn_bridge_t*, float, void*) {
    conflict_callback_called = true;
}

TEST_F(BiasSnnBridgeTest, RegisterConflictCallback) {
    conflict_callback_called = false;
    int result = bias_snn_register_conflict_callback(bridge, test_conflict_callback, nullptr);
    EXPECT_EQ(result, 0);
}

// ============================================================================
// Bio-Async Tests
// ============================================================================

TEST_F(BiasSnnBridgeTest, BioAsyncNotConnectedByDefault) {
    bool connected = bias_snn_is_bio_async_connected(bridge);
    EXPECT_FALSE(connected);
}

TEST_F(BiasSnnBridgeTest, BioAsyncConnectWithEnable) {
    bias_snn_config_t config = bias_snn_config_default();
    config.enable_bio_async = true;
    bias_snn_bridge_t* b = bias_snn_create(&config);
    ASSERT_NE(b, nullptr);

    int result = bias_snn_bio_async_connect(b);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(bias_snn_is_bio_async_connected(b));

    bias_snn_bio_async_disconnect(b);
    EXPECT_FALSE(bias_snn_is_bio_async_connected(b));

    bias_snn_destroy(b);
}

// ============================================================================
// Utility Tests
// ============================================================================

TEST_F(BiasSnnBridgeTest, BiasTypeName) {
    EXPECT_STREQ(bias_snn_type_name(BIAS_SNN_TYPE_CONFIRMATION), "confirmation");
    EXPECT_STREQ(bias_snn_type_name(BIAS_SNN_TYPE_AVAILABILITY), "availability");
    EXPECT_STREQ(bias_snn_type_name(BIAS_SNN_TYPE_ANCHORING), "anchoring");
    EXPECT_STREQ(bias_snn_type_name(BIAS_SNN_TYPE_RECENCY), "recency");
    EXPECT_STREQ(bias_snn_type_name((bias_snn_type_t)100), "unknown");
}

// ============================================================================
// Bias Detection Scenarios
// ============================================================================

TEST_F(BiasSnnBridgeTest, AnchoringBiasScenario) {
    // Strong anchor value should activate anchoring neurons
    bias_snn_encode_decision_context(bridge, 0.95f, 0.3f, 0.0f);
    bias_snn_simulate(bridge, 200.0f);

    float anchoring = bias_snn_get_bias_level(bridge, BIAS_SNN_TYPE_ANCHORING);
    EXPECT_GT(anchoring, 0.0f);  // Should detect anchoring
}

TEST_F(BiasSnnBridgeTest, RecencyBiasScenario) {
    // High recent evidence weight should activate recency neurons
    bias_snn_encode_decision_context(bridge, 0.0f, 0.9f, 0.0f);
    bias_snn_simulate(bridge, 200.0f);

    float recency = bias_snn_get_bias_level(bridge, BIAS_SNN_TYPE_RECENCY);
    EXPECT_GT(recency, 0.0f);  // Should detect recency
}

TEST_F(BiasSnnBridgeTest, OptimismBiasScenario) {
    // Positive emotional valence should activate optimism neurons
    bias_snn_encode_decision_context(bridge, 0.0f, 0.5f, 0.8f);
    bias_snn_simulate(bridge, 200.0f);

    float optimism = bias_snn_get_bias_level(bridge, BIAS_SNN_TYPE_OPTIMISM);
    EXPECT_GT(optimism, 0.0f);  // Should detect optimism
}

TEST_F(BiasSnnBridgeTest, PessimismBiasScenario) {
    // Negative emotional valence should activate pessimism neurons
    bias_snn_encode_decision_context(bridge, 0.0f, 0.5f, -0.8f);
    bias_snn_simulate(bridge, 200.0f);

    float pessimism = bias_snn_get_bias_level(bridge, BIAS_SNN_TYPE_PESSIMISM);
    EXPECT_GT(pessimism, 0.0f);  // Should detect pessimism
}

TEST_F(BiasSnnBridgeTest, ConfirmationBiasScenario) {
    // Evidence aligned with prior belief
    float aligned_evidence[] = {0.6f, 0.65f, 0.7f};
    bias_snn_encode_evidence(bridge, aligned_evidence, 3, 0.65f);
    bias_snn_simulate(bridge, 200.0f);

    float confirmation = bias_snn_get_bias_level(bridge, BIAS_SNN_TYPE_CONFIRMATION);
    EXPECT_GE(confirmation, 0.0f);
}
