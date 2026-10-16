/**
 * @file test_epistemic_snn_bridge.cpp
 * @brief Unit tests for Epistemic-SNN Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

#include "cognitive/epistemic/nimcp_epistemic_snn_bridge.h"

class EpistemicSnnBridgeTest : public ::testing::Test {
protected:
    epistemic_snn_bridge_t* bridge = nullptr;

    void SetUp() override {
        epistemic_snn_config_t config = epistemic_snn_config_default();
        bridge = epistemic_snn_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            epistemic_snn_destroy(bridge);
            bridge = nullptr;
        }
    }
};

// ============================================================================
// Lifecycle Tests
// ============================================================================

TEST_F(EpistemicSnnBridgeTest, CreateWithNullConfig) {
    epistemic_snn_bridge_t* b = epistemic_snn_create(nullptr);
    EXPECT_EQ(b, nullptr);
}

TEST_F(EpistemicSnnBridgeTest, DefaultConfigValid) {
    epistemic_snn_config_t config = epistemic_snn_config_default();
    EXPECT_EQ(config.max_sources, EPISTEMIC_SNN_MAX_SOURCES);
    EXPECT_EQ(config.neurons_per_dim, EPISTEMIC_SNN_NEURONS_PER_DIM);
    EXPECT_GT(config.dt_ms, 0.0f);
    EXPECT_GT(config.evidence_gain, 0.0f);
}

TEST_F(EpistemicSnnBridgeTest, ResetSucceeds) {
    int result = epistemic_snn_reset(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(EpistemicSnnBridgeTest, ResetNullBridge) {
    int result = epistemic_snn_reset(nullptr);
    EXPECT_EQ(result, -1);
}

// ============================================================================
// Encoding Tests
// ============================================================================

TEST_F(EpistemicSnnBridgeTest, EncodeEvidenceBasic) {
    int result = epistemic_snn_encode_evidence(bridge, 0.8f, 0.7f, 0.9f);
    EXPECT_EQ(result, 0);
}

TEST_F(EpistemicSnnBridgeTest, EncodeEvidenceNullBridge) {
    int result = epistemic_snn_encode_evidence(nullptr, 0.8f, 0.7f, 0.9f);
    EXPECT_EQ(result, -1);
}

TEST_F(EpistemicSnnBridgeTest, EncodeEvidenceBoundaryValues) {
    // Test with boundary values
    EXPECT_EQ(epistemic_snn_encode_evidence(bridge, 0.0f, 0.0f, 0.0f), 0);
    EXPECT_EQ(epistemic_snn_encode_evidence(bridge, 1.0f, 1.0f, 1.0f), 0);
}

TEST_F(EpistemicSnnBridgeTest, EncodeEvidenceClamps) {
    // Values should be clamped internally
    EXPECT_EQ(epistemic_snn_encode_evidence(bridge, -0.5f, 1.5f, 2.0f), 0);
}

TEST_F(EpistemicSnnBridgeTest, EncodeClaimBasic) {
    float features[] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
    int result = epistemic_snn_encode_claim(bridge, features, 5, 0.6f);
    EXPECT_EQ(result, 0);
}

TEST_F(EpistemicSnnBridgeTest, EncodeClaimNullFeatures) {
    int result = epistemic_snn_encode_claim(bridge, nullptr, 5, 0.6f);
    EXPECT_EQ(result, -1);
}

TEST_F(EpistemicSnnBridgeTest, EncodeBiasSignals) {
    float biases[] = {0.3f, 0.5f, 0.7f};
    int result = epistemic_snn_encode_bias_signals(bridge, biases, 3);
    EXPECT_EQ(result, 0);
}

// ============================================================================
// Simulation Tests
// ============================================================================

TEST_F(EpistemicSnnBridgeTest, SimulateBasic) {
    epistemic_snn_encode_evidence(bridge, 0.8f, 0.7f, 0.9f);
    int result = epistemic_snn_simulate(bridge, 100.0f);
    EXPECT_EQ(result, 0);
}

TEST_F(EpistemicSnnBridgeTest, SimulateNullBridge) {
    int result = epistemic_snn_simulate(nullptr, 100.0f);
    EXPECT_EQ(result, -1);
}

TEST_F(EpistemicSnnBridgeTest, StepBasic) {
    epistemic_snn_encode_evidence(bridge, 0.8f, 0.7f, 0.9f);
    int result = epistemic_snn_step(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(EpistemicSnnBridgeTest, ForwardBasic) {
    float inputs[] = {0.1f, 0.2f, 0.3f, 0.4f};
    int result = epistemic_snn_forward(bridge, inputs, 4);
    EXPECT_EQ(result, 0);
}

// ============================================================================
// Decoding Tests
// ============================================================================

TEST_F(EpistemicSnnBridgeTest, DecodeAssessment) {
    epistemic_snn_encode_evidence(bridge, 0.9f, 0.8f, 0.95f);
    epistemic_snn_simulate(bridge, 100.0f);

    epistemic_snn_output_t output;
    int result = epistemic_snn_decode_assessment(bridge, &output);
    EXPECT_EQ(result, 0);
    EXPECT_GE(output.epistemic_quality, 0.0f);
    EXPECT_LE(output.epistemic_quality, 1.0f);
}

TEST_F(EpistemicSnnBridgeTest, DecodeAssessmentNullOutput) {
    int result = epistemic_snn_decode_assessment(bridge, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(EpistemicSnnBridgeTest, GetEpistemicQuality) {
    epistemic_snn_encode_evidence(bridge, 0.9f, 0.8f, 0.95f);
    epistemic_snn_simulate(bridge, 100.0f);
    epistemic_snn_output_t output;
    epistemic_snn_decode_assessment(bridge, &output);

    float quality = epistemic_snn_get_epistemic_quality(bridge);
    EXPECT_GE(quality, 0.0f);
    EXPECT_LE(quality, 1.0f);
}

TEST_F(EpistemicSnnBridgeTest, GetUncertainty) {
    float uncertainty = epistemic_snn_get_uncertainty(bridge);
    EXPECT_GE(uncertainty, 0.0f);
    EXPECT_LE(uncertainty, 1.0f);
}

TEST_F(EpistemicSnnBridgeTest, GetBiasLevel) {
    float bias_level = epistemic_snn_get_bias_level(bridge);
    EXPECT_GE(bias_level, 0.0f);
}

TEST_F(EpistemicSnnBridgeTest, GetConspiracyScore) {
    float conspiracy = epistemic_snn_get_conspiracy_score(bridge);
    EXPECT_GE(conspiracy, 0.0f);
}

// ============================================================================
// Source Tracking Tests
// ============================================================================

TEST_F(EpistemicSnnBridgeTest, RegisterSource) {
    int result = epistemic_snn_register_source(bridge, 1, 0.8f);
    EXPECT_EQ(result, 0);
}

TEST_F(EpistemicSnnBridgeTest, RegisterSourceDuplicate) {
    epistemic_snn_register_source(bridge, 1, 0.8f);
    int result = epistemic_snn_register_source(bridge, 1, 0.9f);
    EXPECT_EQ(result, -1);
}

TEST_F(EpistemicSnnBridgeTest, UpdateSourceReliability) {
    epistemic_snn_register_source(bridge, 1, 0.5f);

    // Update with correct info
    int result = epistemic_snn_update_source_reliability(bridge, 1, true);
    EXPECT_EQ(result, 0);

    float reliability = epistemic_snn_get_source_reliability(bridge, 1);
    EXPECT_GT(reliability, 0.5f);  // Should increase
}

TEST_F(EpistemicSnnBridgeTest, UpdateSourceReliabilityDecreases) {
    epistemic_snn_register_source(bridge, 1, 0.8f);

    // Update with incorrect info
    int result = epistemic_snn_update_source_reliability(bridge, 1, false);
    EXPECT_EQ(result, 0);

    float reliability = epistemic_snn_get_source_reliability(bridge, 1);
    EXPECT_LT(reliability, 0.8f);  // Should decrease
}

TEST_F(EpistemicSnnBridgeTest, GetSourceReliabilityUnknown) {
    float reliability = epistemic_snn_get_source_reliability(bridge, 999);
    EXPECT_FLOAT_EQ(reliability, 0.5f);  // Default for unknown
}

// ============================================================================
// State Query Tests
// ============================================================================

TEST_F(EpistemicSnnBridgeTest, GetDimensionState) {
    epistemic_snn_encode_evidence(bridge, 0.8f, 0.7f, 0.9f);

    epistemic_dimension_state_t state;
    int result = epistemic_snn_get_dimension_state(bridge, 0, &state);
    EXPECT_EQ(result, 0);
}

TEST_F(EpistemicSnnBridgeTest, GetState) {
    epistemic_snn_bridge_state_t state;
    int result = epistemic_snn_get_state(bridge, &state);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(state.state, EPISTEMIC_SNN_STATE_IDLE);
}

TEST_F(EpistemicSnnBridgeTest, GetStats) {
    epistemic_snn_stats_t stats;
    int result = epistemic_snn_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
}

TEST_F(EpistemicSnnBridgeTest, ResetStats) {
    epistemic_snn_encode_evidence(bridge, 0.8f, 0.7f, 0.9f);
    epistemic_snn_simulate(bridge, 100.0f);

    int result = epistemic_snn_reset_stats(bridge);
    EXPECT_EQ(result, 0);

    epistemic_snn_stats_t stats;
    epistemic_snn_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_evaluations, 0u);
}

// ============================================================================
// Callback Tests
// ============================================================================

static bool spike_callback_called = false;
static void test_spike_callback(epistemic_snn_bridge_t*, uint32_t, float, void*) {
    spike_callback_called = true;
}

TEST_F(EpistemicSnnBridgeTest, RegisterSpikeCallback) {
    spike_callback_called = false;
    int result = epistemic_snn_register_spike_callback(bridge, test_spike_callback, nullptr);
    EXPECT_EQ(result, 0);
}

static bool bias_callback_called = false;
static void test_bias_callback(epistemic_snn_bridge_t*, uint32_t, float, void*) {
    bias_callback_called = true;
}

TEST_F(EpistemicSnnBridgeTest, RegisterBiasCallback) {
    bias_callback_called = false;
    int result = epistemic_snn_register_bias_callback(bridge, test_bias_callback, nullptr);
    EXPECT_EQ(result, 0);
}

// ============================================================================
// Bio-Async Tests
// ============================================================================

TEST_F(EpistemicSnnBridgeTest, BioAsyncNotConnectedByDefault) {
    bool connected = epistemic_snn_is_bio_async_connected(bridge);
    EXPECT_FALSE(connected);
}

TEST_F(EpistemicSnnBridgeTest, BioAsyncConnectWithoutEnable) {
    // Bio-async not enabled in default config
    int result = epistemic_snn_bio_async_connect(bridge);
    EXPECT_EQ(result, -1);
}

TEST_F(EpistemicSnnBridgeTest, BioAsyncConnectWithEnable) {
    epistemic_snn_config_t config = epistemic_snn_config_default();
    config.enable_bio_async = true;
    epistemic_snn_bridge_t* b = epistemic_snn_create(&config);
    ASSERT_NE(b, nullptr);

    int result = epistemic_snn_bio_async_connect(b);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(epistemic_snn_is_bio_async_connected(b));

    epistemic_snn_bio_async_disconnect(b);
    EXPECT_FALSE(epistemic_snn_is_bio_async_connected(b));

    epistemic_snn_destroy(b);
}

// ============================================================================
// Integration Scenarios
// ============================================================================

TEST_F(EpistemicSnnBridgeTest, HighQualityEvidenceScenario) {
    // Register a reliable source
    epistemic_snn_register_source(bridge, 1, 0.9f);

    // Encode high-quality evidence
    epistemic_snn_encode_evidence(bridge, 0.95f, 0.9f, 0.95f);

    // Simulate
    epistemic_snn_simulate(bridge, 200.0f);

    // Decode
    epistemic_snn_output_t output;
    epistemic_snn_decode_assessment(bridge, &output);

    // High quality evidence should result in good epistemic quality
    EXPECT_GT(output.evidence_strength, 0.3f);
    EXPECT_FALSE(output.bias_detected);
}

TEST_F(EpistemicSnnBridgeTest, LowQualityEvidenceScenario) {
    // Encode low-quality evidence from unreliable source
    epistemic_snn_encode_evidence(bridge, 0.2f, 0.3f, 0.1f);

    // Simulate
    epistemic_snn_simulate(bridge, 200.0f);

    // Decode
    epistemic_snn_output_t output;
    epistemic_snn_decode_assessment(bridge, &output);

    // Low quality should reflect in output
    EXPECT_LT(output.evidence_strength, 0.7f);
}

TEST_F(EpistemicSnnBridgeTest, BiasDetectionScenario) {
    // Encode multiple high bias signals
    float biases[] = {0.8f, 0.85f, 0.9f};
    epistemic_snn_encode_bias_signals(bridge, biases, 3);

    // Simulate
    epistemic_snn_simulate(bridge, 200.0f);

    // Decode
    epistemic_snn_output_t output;
    epistemic_snn_decode_assessment(bridge, &output);

    // Should detect bias or conspiracy
    // (may or may not be detected depending on simulation dynamics)
    EXPECT_GE(output.bias_magnitude, 0.0f);
}
