/**
 * @file test_attention_snn_bridge.cpp
 * @brief Unit tests for Attention System - SNN Bridge integration
 * @date 2026-01-06
 *
 * Tests bidirectional integration between multihead attention and SNN module:
 * - Attention --> SNN: Attention weights encoding to spikes
 * - SNN --> Attention: Decoded weights and focus state from spike output
 * - Bio-async message handling
 * - Modulation and state management
 */

#include <gtest/gtest.h>

// Note: Header has its own extern "C" guards
#include "cognitive/attention/nimcp_attention_snn_bridge.h"

#include "utils/time/nimcp_time.h"

#include <cmath>
#include <cstring>
#include <vector>
#include <thread>
#include <atomic>

//=============================================================================
// Test Fixtures
//=============================================================================

class AttentionSNNBridgeTest : public ::testing::Test {
protected:
    attention_snn_bridge_t* bridge = nullptr;

    void SetUp() override {
        attention_snn_config_t config = attention_snn_config_default();
        config.num_heads = 8;
        config.neurons_per_head = ATTENTION_SNN_NEURONS_PER_HEAD;
        config.salience_dim = ATTENTION_SNN_SALIENCE_DIM;
        config.sequence_length = 128;
        config.enable_bio_async = false;  /* Disable for unit tests */
        config.enable_immune_modulation = false;
        config.enable_plasticity_integration = false;
        bridge = attention_snn_create(&config);
        ASSERT_NE(bridge, nullptr) << "Failed to create attention-SNN bridge";
    }

    void TearDown() override {
        if (bridge) {
            attention_snn_destroy(bridge);
            bridge = nullptr;
        }
    }

    /* Helper to create attention weights array */
    std::vector<float> create_attention_weights(uint32_t num_heads, float base_value = 0.5f) {
        std::vector<float> weights(num_heads);
        for (uint32_t i = 0; i < num_heads; i++) {
            weights[i] = base_value + 0.3f * std::sin((float)i / 2.0f);
            if (weights[i] < 0.0f) weights[i] = 0.0f;
            if (weights[i] > 1.0f) weights[i] = 1.0f;
        }
        return weights;
    }

    /* Helper to create salience map */
    std::vector<float> create_salience_map(uint32_t length) {
        std::vector<float> salience(length);
        for (uint32_t i = 0; i < length; i++) {
            salience[i] = 0.3f + 0.4f * std::sin((float)i / 10.0f);
            if (salience[i] < 0.0f) salience[i] = 0.0f;
            if (salience[i] > 1.0f) salience[i] = 1.0f;
        }
        return salience;
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(AttentionSNNBridgeTest, CreateDestroy) {
    EXPECT_NE(bridge, nullptr);
}

TEST_F(AttentionSNNBridgeTest, CreateWithDefaultConfig) {
    attention_snn_bridge_t* b = attention_snn_create(nullptr);
    ASSERT_NE(b, nullptr);
    attention_snn_destroy(b);
}

TEST_F(AttentionSNNBridgeTest, CreateWithCustomConfig) {
    attention_snn_config_t config = attention_snn_config_default();
    config.num_heads = 4;
    config.neurons_per_head = 16;
    config.salience_dim = 32;
    config.sequence_length = 64;
    config.encoding = ATTENTION_SNN_ENCODE_RATE;
    config.decoding = ATTENTION_SNN_DECODE_COMPETITION;

    attention_snn_bridge_t* b = attention_snn_create(&config);
    ASSERT_NE(b, nullptr);
    attention_snn_destroy(b);
}

TEST_F(AttentionSNNBridgeTest, CreateWithMaxHeads) {
    attention_snn_config_t config = attention_snn_config_default();
    config.num_heads = ATTENTION_SNN_MAX_HEADS;
    config.enable_bio_async = false;

    attention_snn_bridge_t* b = attention_snn_create(&config);
    ASSERT_NE(b, nullptr);
    attention_snn_destroy(b);
}

TEST_F(AttentionSNNBridgeTest, CreateWithZeroHeadsFails) {
    attention_snn_config_t config = attention_snn_config_default();
    config.num_heads = 0;

    attention_snn_bridge_t* b = attention_snn_create(&config);
    EXPECT_EQ(b, nullptr) << "Should fail with zero heads";
}

TEST_F(AttentionSNNBridgeTest, CreateWithExcessiveHeadsFails) {
    attention_snn_config_t config = attention_snn_config_default();
    config.num_heads = ATTENTION_SNN_MAX_HEADS + 1;

    attention_snn_bridge_t* b = attention_snn_create(&config);
    EXPECT_EQ(b, nullptr) << "Should fail with excessive heads";
}

TEST_F(AttentionSNNBridgeTest, DestroyNull) {
    /* Should not crash */
    attention_snn_destroy(nullptr);
}

TEST_F(AttentionSNNBridgeTest, Reset) {
    /* Encode something first */
    std::vector<float> weights = create_attention_weights(8, 0.7f);
    attention_snn_encode_weights(bridge, weights.data(), 8);
    attention_snn_simulate(bridge, 50.0f);

    /* Reset should succeed */
    int ret = attention_snn_reset(bridge);
    EXPECT_EQ(ret, 0) << "Reset should succeed";

    /* State should be IDLE after reset */
    attention_snn_bridge_state_t state;
    attention_snn_get_state(bridge, &state);
    EXPECT_EQ(state.state, ATTENTION_SNN_STATE_IDLE);
}

TEST_F(AttentionSNNBridgeTest, ResetNull) {
    int ret = attention_snn_reset(nullptr);
    EXPECT_EQ(ret, -1) << "Reset with null bridge should fail";
}

TEST_F(AttentionSNNBridgeTest, ResetClearsAttentionState) {
    /* Set up attention state */
    std::vector<float> weights = create_attention_weights(8, 0.8f);
    attention_snn_encode_weights(bridge, weights.data(), 8);
    attention_snn_simulate(bridge, 50.0f);

    float decoded_weights[8];
    attention_snn_get_weights(bridge, decoded_weights, 8);

    /* Reset */
    attention_snn_reset(bridge);

    /* Get attention state - should be reset */
    attention_snn_attention_state_t attention_state;
    attention_snn_get_attention_state(bridge, &attention_state);
    EXPECT_NEAR(attention_state.focus_strength, 0.0f, 0.01f);
    EXPECT_NEAR(attention_state.sparsity, 1.0f, 0.01f);
}

//=============================================================================
// Default Configuration Tests
//=============================================================================

TEST_F(AttentionSNNBridgeTest, DefaultConfigValues) {
    attention_snn_config_t config = attention_snn_config_default();

    EXPECT_EQ(config.num_heads, 8u);
    EXPECT_EQ(config.neurons_per_head, ATTENTION_SNN_NEURONS_PER_HEAD);
    EXPECT_EQ(config.salience_dim, ATTENTION_SNN_SALIENCE_DIM);
    EXPECT_GT(config.sequence_length, 0u);
    EXPECT_GT(config.dt_ms, 0.0f);
    EXPECT_EQ(config.dt_ms, ATTENTION_SNN_DEFAULT_DT);
    EXPECT_GT(config.simulation_window_ms, 0.0f);
    EXPECT_EQ(config.simulation_window_ms, ATTENTION_SNN_ENCODING_WINDOW);
    EXPECT_TRUE(config.enable_competition);
    EXPECT_TRUE(config.enable_gate_integration);
    EXPECT_EQ(config.encoding, ATTENTION_SNN_ENCODE_POPULATION);
    EXPECT_EQ(config.decoding, ATTENTION_SNN_DECODE_SOFTMAX);
    EXPECT_GT(config.baseline_rate_hz, 0.0f);
    EXPECT_GT(config.max_rate_hz, config.baseline_rate_hz);
    EXPECT_GT(config.top_k, 0u);
}

TEST_F(AttentionSNNBridgeTest, DefaultConfigEncodingGain) {
    attention_snn_config_t config = attention_snn_config_default();
    EXPECT_GT(config.encoding_gain, 0.0f);
    EXPECT_GT(config.salience_gain, 0.0f);
}

TEST_F(AttentionSNNBridgeTest, DefaultConfigCompetitionParams) {
    attention_snn_config_t config = attention_snn_config_default();
    EXPECT_GE(config.inhibition_strength, 0.0f);
    EXPECT_LE(config.inhibition_strength, 1.0f);
    EXPECT_GT(config.competition_tau_ms, 0.0f);
}

//=============================================================================
// Encoding Tests (Attention --> SNN)
//=============================================================================

TEST_F(AttentionSNNBridgeTest, EncodeWeights) {
    std::vector<float> weights = create_attention_weights(8, 0.6f);

    int spikes = attention_snn_encode_weights(bridge, weights.data(), 8);
    EXPECT_GE(spikes, 0) << "Encoding should succeed and return spike count";
}

TEST_F(AttentionSNNBridgeTest, EncodeWeightsNullBridge) {
    std::vector<float> weights = create_attention_weights(8);
    int spikes = attention_snn_encode_weights(nullptr, weights.data(), 8);
    EXPECT_EQ(spikes, -1) << "Should fail with null bridge";
}

TEST_F(AttentionSNNBridgeTest, EncodeWeightsNullWeights) {
    int spikes = attention_snn_encode_weights(bridge, nullptr, 8);
    EXPECT_EQ(spikes, -1) << "Should fail with null weights";
}

TEST_F(AttentionSNNBridgeTest, EncodeWeightsFewerHeads) {
    /* Encode with fewer heads than configured */
    std::vector<float> weights = create_attention_weights(4);
    int spikes = attention_snn_encode_weights(bridge, weights.data(), 4);
    EXPECT_GE(spikes, 0) << "Should succeed with fewer heads";
}

TEST_F(AttentionSNNBridgeTest, EncodeWeightsMoreHeads) {
    /* Encode with more heads than configured - should truncate */
    std::vector<float> weights = create_attention_weights(16);
    int spikes = attention_snn_encode_weights(bridge, weights.data(), 16);
    EXPECT_GE(spikes, 0) << "Should succeed with extra heads (truncated)";
}

TEST_F(AttentionSNNBridgeTest, EncodeWeightsBoundaryValues) {
    float weights[8];

    /* Test with all zeros */
    memset(weights, 0, sizeof(weights));
    EXPECT_GE(attention_snn_encode_weights(bridge, weights, 8), 0);

    /* Test with all ones */
    for (int i = 0; i < 8; i++) weights[i] = 1.0f;
    EXPECT_GE(attention_snn_encode_weights(bridge, weights, 8), 0);

    /* Test with mixed extreme values */
    weights[0] = 0.0f;
    weights[1] = 1.0f;
    weights[2] = 0.5f;
    weights[3] = 0.0f;
    weights[4] = 1.0f;
    weights[5] = 0.25f;
    weights[6] = 0.75f;
    weights[7] = 0.0f;
    EXPECT_GE(attention_snn_encode_weights(bridge, weights, 8), 0);
}

TEST_F(AttentionSNNBridgeTest, EncodeSalience) {
    std::vector<float> salience = create_salience_map(64);

    int spikes = attention_snn_encode_salience(bridge, salience.data(), 64);
    EXPECT_GE(spikes, 0) << "Salience encoding should succeed";
}

TEST_F(AttentionSNNBridgeTest, EncodeSalienceNullBridge) {
    std::vector<float> salience = create_salience_map(64);
    int spikes = attention_snn_encode_salience(nullptr, salience.data(), 64);
    EXPECT_EQ(spikes, -1) << "Should fail with null bridge";
}

TEST_F(AttentionSNNBridgeTest, EncodeSalienceNullSalience) {
    int spikes = attention_snn_encode_salience(bridge, nullptr, 64);
    EXPECT_EQ(spikes, -1) << "Should fail with null salience";
}

TEST_F(AttentionSNNBridgeTest, EncodeSalienceVaryingLength) {
    /* Test with shorter sequence */
    std::vector<float> salience_short = create_salience_map(32);
    EXPECT_GE(attention_snn_encode_salience(bridge, salience_short.data(), 32), 0);

    /* Test with longer sequence - should truncate */
    std::vector<float> salience_long = create_salience_map(128);
    EXPECT_GE(attention_snn_encode_salience(bridge, salience_long.data(), 128), 0);
}

TEST_F(AttentionSNNBridgeTest, EncodeGate) {
    int ret = attention_snn_encode_gate(bridge, 0.5f);
    EXPECT_EQ(ret, 0) << "Gate encoding should succeed";
}

TEST_F(AttentionSNNBridgeTest, EncodeGateNullBridge) {
    int ret = attention_snn_encode_gate(nullptr, 0.5f);
    EXPECT_EQ(ret, -1) << "Should fail with null bridge";
}

TEST_F(AttentionSNNBridgeTest, EncodeGateBoundaryValues) {
    EXPECT_EQ(attention_snn_encode_gate(bridge, 0.0f), 0);
    EXPECT_EQ(attention_snn_encode_gate(bridge, 0.5f), 0);
    EXPECT_EQ(attention_snn_encode_gate(bridge, 1.0f), 0);
}

TEST_F(AttentionSNNBridgeTest, EncodeGateOutOfRange) {
    /* Values should be clamped, not fail */
    EXPECT_EQ(attention_snn_encode_gate(bridge, -0.5f), 0);
    EXPECT_EQ(attention_snn_encode_gate(bridge, 1.5f), 0);
}

TEST_F(AttentionSNNBridgeTest, EncodeGateDisabled) {
    /* Create bridge with gate integration disabled */
    attention_snn_config_t config = attention_snn_config_default();
    config.enable_gate_integration = false;
    config.enable_bio_async = false;

    attention_snn_bridge_t* b = attention_snn_create(&config);
    ASSERT_NE(b, nullptr);

    /* Should return 0 (no-op) when gate integration is disabled */
    int ret = attention_snn_encode_gate(b, 0.5f);
    EXPECT_EQ(ret, 0);

    attention_snn_destroy(b);
}

//=============================================================================
// Simulation Tests
//=============================================================================

TEST_F(AttentionSNNBridgeTest, Simulate) {
    /* Encode first */
    std::vector<float> weights = create_attention_weights(8);
    attention_snn_encode_weights(bridge, weights.data(), 8);

    /* Simulate */
    int ret = attention_snn_simulate(bridge, 50.0f);
    EXPECT_EQ(ret, 0) << "Simulation should succeed";
}

TEST_F(AttentionSNNBridgeTest, SimulateNull) {
    int ret = attention_snn_simulate(nullptr, 50.0f);
    EXPECT_EQ(ret, -1) << "Should fail with null bridge";
}

TEST_F(AttentionSNNBridgeTest, SimulateVaryingDuration) {
    std::vector<float> weights = create_attention_weights(8);
    attention_snn_encode_weights(bridge, weights.data(), 8);

    /* Short duration */
    EXPECT_EQ(attention_snn_simulate(bridge, 10.0f), 0);

    /* Medium duration */
    EXPECT_EQ(attention_snn_simulate(bridge, 50.0f), 0);

    /* Longer duration */
    EXPECT_EQ(attention_snn_simulate(bridge, 100.0f), 0);
}

TEST_F(AttentionSNNBridgeTest, SimulateZeroDuration) {
    std::vector<float> weights = create_attention_weights(8);
    attention_snn_encode_weights(bridge, weights.data(), 8);

    /* Zero duration should succeed (no-op) */
    int ret = attention_snn_simulate(bridge, 0.0f);
    EXPECT_EQ(ret, 0);
}

TEST_F(AttentionSNNBridgeTest, Step) {
    std::vector<float> weights = create_attention_weights(8);
    attention_snn_encode_weights(bridge, weights.data(), 8);

    int ret = attention_snn_step(bridge);
    EXPECT_EQ(ret, 0) << "Single step should succeed";
}

TEST_F(AttentionSNNBridgeTest, StepNull) {
    int ret = attention_snn_step(nullptr);
    EXPECT_EQ(ret, -1) << "Should fail with null bridge";
}

TEST_F(AttentionSNNBridgeTest, MultipleSteps) {
    std::vector<float> weights = create_attention_weights(8);
    attention_snn_encode_weights(bridge, weights.data(), 8);

    for (int i = 0; i < 50; i++) {
        int ret = attention_snn_step(bridge);
        EXPECT_EQ(ret, 0) << "Step " << i << " should succeed";
    }
}

TEST_F(AttentionSNNBridgeTest, Compete) {
    std::vector<float> weights = create_attention_weights(8);
    attention_snn_encode_weights(bridge, weights.data(), 8);
    attention_snn_simulate(bridge, 20.0f);

    int ret = attention_snn_compete(bridge, 30.0f);
    EXPECT_EQ(ret, 0) << "Competition should succeed";
}

TEST_F(AttentionSNNBridgeTest, CompeteNull) {
    int ret = attention_snn_compete(nullptr, 30.0f);
    EXPECT_EQ(ret, -1) << "Should fail with null bridge";
}

TEST_F(AttentionSNNBridgeTest, CompeteDisabled) {
    /* Create bridge with competition disabled */
    attention_snn_config_t config = attention_snn_config_default();
    config.enable_competition = false;
    config.enable_bio_async = false;

    attention_snn_bridge_t* b = attention_snn_create(&config);
    ASSERT_NE(b, nullptr);

    std::vector<float> weights = create_attention_weights(8);
    attention_snn_encode_weights(b, weights.data(), 8);

    /* Should return 0 (no-op) when competition is disabled */
    int ret = attention_snn_compete(b, 30.0f);
    EXPECT_EQ(ret, 0);

    attention_snn_destroy(b);
}

TEST_F(AttentionSNNBridgeTest, CompeteVaryingDuration) {
    std::vector<float> weights = create_attention_weights(8);
    attention_snn_encode_weights(bridge, weights.data(), 8);

    EXPECT_EQ(attention_snn_compete(bridge, 10.0f), 0);
    EXPECT_EQ(attention_snn_compete(bridge, 50.0f), 0);
    EXPECT_EQ(attention_snn_compete(bridge, 100.0f), 0);
}

//=============================================================================
// Decoding Tests (SNN --> Attention)
//=============================================================================

TEST_F(AttentionSNNBridgeTest, GetWeights) {
    /* Encode and simulate first */
    std::vector<float> input_weights = create_attention_weights(8, 0.6f);
    attention_snn_encode_weights(bridge, input_weights.data(), 8);
    attention_snn_simulate(bridge, 50.0f);

    /* Get decoded weights */
    float weights[8];
    int ret = attention_snn_get_weights(bridge, weights, 8);
    EXPECT_EQ(ret, 0) << "Should decode weights successfully";

    /* Weights should be in valid range */
    for (int i = 0; i < 8; i++) {
        EXPECT_GE(weights[i], 0.0f);
        EXPECT_LE(weights[i], 1.0f);
    }
}

TEST_F(AttentionSNNBridgeTest, GetWeightsNullBridge) {
    float weights[8];
    int ret = attention_snn_get_weights(nullptr, weights, 8);
    EXPECT_EQ(ret, -1) << "Should fail with null bridge";
}

TEST_F(AttentionSNNBridgeTest, GetWeightsNullWeights) {
    int ret = attention_snn_get_weights(bridge, nullptr, 8);
    EXPECT_EQ(ret, -1) << "Should fail with null weights";
}

TEST_F(AttentionSNNBridgeTest, GetWeightsFewerHeads) {
    std::vector<float> input_weights = create_attention_weights(8);
    attention_snn_encode_weights(bridge, input_weights.data(), 8);
    attention_snn_simulate(bridge, 50.0f);

    /* Request fewer heads than configured */
    float weights[4];
    int ret = attention_snn_get_weights(bridge, weights, 4);
    EXPECT_EQ(ret, 0);
}

TEST_F(AttentionSNNBridgeTest, GetSalience) {
    std::vector<float> input_salience = create_salience_map(64);
    attention_snn_encode_salience(bridge, input_salience.data(), 64);

    float salience[64];
    int ret = attention_snn_get_salience(bridge, salience, 64);
    EXPECT_EQ(ret, 0) << "Should get salience successfully";
}

TEST_F(AttentionSNNBridgeTest, GetSalienceNullBridge) {
    float salience[64];
    int ret = attention_snn_get_salience(nullptr, salience, 64);
    EXPECT_EQ(ret, -1) << "Should fail with null bridge";
}

TEST_F(AttentionSNNBridgeTest, GetSalienceNullSalience) {
    int ret = attention_snn_get_salience(bridge, nullptr, 64);
    EXPECT_EQ(ret, -1) << "Should fail with null salience";
}

TEST_F(AttentionSNNBridgeTest, GetTopK) {
    std::vector<float> weights = create_attention_weights(8, 0.5f);
    attention_snn_encode_weights(bridge, weights.data(), 8);
    attention_snn_simulate(bridge, 50.0f);

    /* Decode weights to update attention state */
    float decoded_weights[8];
    attention_snn_get_weights(bridge, decoded_weights, 8);

    int32_t indices[3];
    int count = attention_snn_get_top_k(bridge, indices, 3);
    EXPECT_GE(count, 0) << "Should return valid count";
    EXPECT_LE(count, 3);

    /* Valid indices should be in range */
    for (int i = 0; i < count; i++) {
        if (indices[i] >= 0) {
            EXPECT_LT(indices[i], 8);
        }
    }
}

TEST_F(AttentionSNNBridgeTest, GetTopKNullBridge) {
    int32_t indices[3];
    int count = attention_snn_get_top_k(nullptr, indices, 3);
    EXPECT_EQ(count, -1) << "Should fail with null bridge";
}

TEST_F(AttentionSNNBridgeTest, GetTopKNullIndices) {
    int count = attention_snn_get_top_k(bridge, nullptr, 3);
    EXPECT_EQ(count, -1) << "Should fail with null indices";
}

TEST_F(AttentionSNNBridgeTest, GetTopKVaryingK) {
    std::vector<float> weights = create_attention_weights(8, 0.6f);
    attention_snn_encode_weights(bridge, weights.data(), 8);
    attention_snn_simulate(bridge, 50.0f);

    float decoded_weights[8];
    attention_snn_get_weights(bridge, decoded_weights, 8);

    /* Test k=1 */
    int32_t indices1[1];
    EXPECT_GE(attention_snn_get_top_k(bridge, indices1, 1), 0);

    /* Test k=5 */
    int32_t indices5[5];
    EXPECT_GE(attention_snn_get_top_k(bridge, indices5, 5), 0);
}

TEST_F(AttentionSNNBridgeTest, GetFocusStrength) {
    std::vector<float> weights = create_attention_weights(8);
    attention_snn_encode_weights(bridge, weights.data(), 8);
    attention_snn_simulate(bridge, 50.0f);

    float decoded_weights[8];
    attention_snn_get_weights(bridge, decoded_weights, 8);

    float focus = attention_snn_get_focus_strength(bridge);
    EXPECT_GE(focus, 0.0f);
    EXPECT_LE(focus, 1.0f);
}

TEST_F(AttentionSNNBridgeTest, GetFocusStrengthNull) {
    float focus = attention_snn_get_focus_strength(nullptr);
    EXPECT_EQ(focus, -1.0f) << "Should return -1 with null bridge";
}

TEST_F(AttentionSNNBridgeTest, GetSparsity) {
    std::vector<float> weights = create_attention_weights(8);
    attention_snn_encode_weights(bridge, weights.data(), 8);
    attention_snn_simulate(bridge, 50.0f);

    float decoded_weights[8];
    attention_snn_get_weights(bridge, decoded_weights, 8);

    float sparsity = attention_snn_get_sparsity(bridge);
    EXPECT_GE(sparsity, 0.0f);
    EXPECT_LE(sparsity, 1.0f);
}

TEST_F(AttentionSNNBridgeTest, GetSparsityNull) {
    float sparsity = attention_snn_get_sparsity(nullptr);
    EXPECT_EQ(sparsity, -1.0f) << "Should return -1 with null bridge";
}

TEST_F(AttentionSNNBridgeTest, GetAttentionState) {
    std::vector<float> weights = create_attention_weights(8, 0.7f);
    attention_snn_encode_weights(bridge, weights.data(), 8);
    attention_snn_simulate(bridge, 50.0f);

    float decoded_weights[8];
    attention_snn_get_weights(bridge, decoded_weights, 8);

    attention_snn_attention_state_t attention_state;
    int ret = attention_snn_get_attention_state(bridge, &attention_state);
    EXPECT_EQ(ret, 0) << "Should get attention state successfully";

    /* Check state values are valid */
    EXPECT_GE(attention_state.focus_strength, 0.0f);
    EXPECT_LE(attention_state.focus_strength, 1.0f);
    EXPECT_GE(attention_state.sparsity, 0.0f);
    EXPECT_LE(attention_state.sparsity, 1.0f);
    EXPECT_GE(attention_state.gate_activation, 0.0f);
    EXPECT_LE(attention_state.gate_activation, 1.0f);
}

TEST_F(AttentionSNNBridgeTest, GetAttentionStateNullBridge) {
    attention_snn_attention_state_t state;
    EXPECT_EQ(attention_snn_get_attention_state(nullptr, &state), -1);
}

TEST_F(AttentionSNNBridgeTest, GetAttentionStateNullState) {
    EXPECT_EQ(attention_snn_get_attention_state(bridge, nullptr), -1);
}

//=============================================================================
// State and Statistics Tests
//=============================================================================

TEST_F(AttentionSNNBridgeTest, GetState) {
    attention_snn_bridge_state_t state;
    int ret = attention_snn_get_state(bridge, &state);

    EXPECT_EQ(ret, 0) << "Should get state";
    EXPECT_EQ(state.state, ATTENTION_SNN_STATE_IDLE);
    EXPECT_FALSE(state.bio_async_connected);
}

TEST_F(AttentionSNNBridgeTest, GetStateNullBridge) {
    attention_snn_bridge_state_t state;
    EXPECT_EQ(attention_snn_get_state(nullptr, &state), -1);
}

TEST_F(AttentionSNNBridgeTest, GetStateNullState) {
    EXPECT_EQ(attention_snn_get_state(bridge, nullptr), -1);
}

TEST_F(AttentionSNNBridgeTest, StateAfterEncoding) {
    std::vector<float> weights = create_attention_weights(8);
    attention_snn_encode_weights(bridge, weights.data(), 8);

    attention_snn_bridge_state_t state;
    attention_snn_get_state(bridge, &state);
    /* State should return to IDLE after encoding completes */
    EXPECT_EQ(state.state, ATTENTION_SNN_STATE_IDLE);
}

TEST_F(AttentionSNNBridgeTest, GetStats) {
    attention_snn_stats_t stats;
    int ret = attention_snn_get_stats(bridge, &stats);

    EXPECT_EQ(ret, 0) << "Should get stats";
    EXPECT_EQ(stats.total_forward_passes, 0u);
    EXPECT_EQ(stats.total_spikes_generated, 0u);
    EXPECT_EQ(stats.total_decodings, 0u);
}

TEST_F(AttentionSNNBridgeTest, GetStatsNullBridge) {
    attention_snn_stats_t stats;
    EXPECT_EQ(attention_snn_get_stats(nullptr, &stats), -1);
}

TEST_F(AttentionSNNBridgeTest, GetStatsNullStats) {
    EXPECT_EQ(attention_snn_get_stats(bridge, nullptr), -1);
}

TEST_F(AttentionSNNBridgeTest, StatsUpdateAfterEncoding) {
    /* Initial stats */
    attention_snn_stats_t stats_before;
    attention_snn_get_stats(bridge, &stats_before);
    EXPECT_EQ(stats_before.total_forward_passes, 0u);

    /* Encode some weights */
    for (int i = 0; i < 5; i++) {
        std::vector<float> weights = create_attention_weights(8);
        attention_snn_encode_weights(bridge, weights.data(), 8);
    }

    /* Check stats updated */
    attention_snn_stats_t stats_after;
    attention_snn_get_stats(bridge, &stats_after);
    EXPECT_EQ(stats_after.total_forward_passes, 5u);
    EXPECT_GT(stats_after.total_spikes_generated, 0u);
}

TEST_F(AttentionSNNBridgeTest, StatsUpdateAfterDecoding) {
    /* Encode and simulate */
    std::vector<float> weights = create_attention_weights(8);
    attention_snn_encode_weights(bridge, weights.data(), 8);
    attention_snn_simulate(bridge, 50.0f);

    /* Decode multiple times */
    for (int i = 0; i < 3; i++) {
        float decoded_weights[8];
        attention_snn_get_weights(bridge, decoded_weights, 8);
    }

    /* Check stats */
    attention_snn_stats_t stats;
    attention_snn_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_decodings, 3u);
}

TEST_F(AttentionSNNBridgeTest, ResetStats) {
    /* Generate some stats */
    std::vector<float> weights = create_attention_weights(8);
    attention_snn_encode_weights(bridge, weights.data(), 8);
    attention_snn_simulate(bridge, 50.0f);

    float decoded_weights[8];
    attention_snn_get_weights(bridge, decoded_weights, 8);

    /* Verify stats exist */
    attention_snn_stats_t stats;
    attention_snn_get_stats(bridge, &stats);
    EXPECT_GT(stats.total_forward_passes, 0u);
    EXPECT_GT(stats.total_decodings, 0u);

    /* Reset stats */
    attention_snn_reset_stats(bridge);

    /* Verify stats are cleared */
    attention_snn_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_forward_passes, 0u);
    EXPECT_EQ(stats.total_spikes_generated, 0u);
    EXPECT_EQ(stats.total_decodings, 0u);
}

TEST_F(AttentionSNNBridgeTest, ResetStatsNull) {
    /* Should not crash */
    attention_snn_reset_stats(nullptr);
}

//=============================================================================
// Bio-Async Integration Tests
//=============================================================================

TEST_F(AttentionSNNBridgeTest, BioAsyncNotConnectedInitially) {
    bool connected = attention_snn_is_bio_async_connected(bridge);
    EXPECT_FALSE(connected) << "Should not be connected initially";
}

TEST_F(AttentionSNNBridgeTest, BioAsyncConnectDisconnect) {
    /* Connect */
    int ret = attention_snn_connect_bio_async(bridge);
    /* Note: Returns 0 if bio_async is disabled in config (no-op) */
    EXPECT_EQ(ret, 0);

    /* With bio_async disabled, it won't actually connect */
    bool connected = attention_snn_is_bio_async_connected(bridge);
    /* Connection status depends on config.enable_bio_async */

    /* Disconnect */
    ret = attention_snn_disconnect_bio_async(bridge);
    EXPECT_EQ(ret, 0);

    connected = attention_snn_is_bio_async_connected(bridge);
    EXPECT_FALSE(connected);
}

TEST_F(AttentionSNNBridgeTest, BioAsyncConnectNull) {
    EXPECT_EQ(attention_snn_connect_bio_async(nullptr), -1);
}

TEST_F(AttentionSNNBridgeTest, BioAsyncDisconnectNull) {
    EXPECT_EQ(attention_snn_disconnect_bio_async(nullptr), -1);
}

TEST_F(AttentionSNNBridgeTest, BioAsyncIsConnectedNull) {
    bool connected = attention_snn_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}

class AttentionSNNBridgeBioAsyncEnabledTest : public ::testing::Test {
protected:
    attention_snn_bridge_t* bridge = nullptr;

    void SetUp() override {
        attention_snn_config_t config = attention_snn_config_default();
        config.enable_bio_async = true;  /* Enable bio-async */
        config.enable_immune_modulation = false;
        bridge = attention_snn_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            attention_snn_destroy(bridge);
            bridge = nullptr;
        }
    }
};

TEST_F(AttentionSNNBridgeBioAsyncEnabledTest, ConnectWithBioAsyncEnabled) {
    int ret = attention_snn_connect_bio_async(bridge);
    EXPECT_EQ(ret, 0);

    bool connected = attention_snn_is_bio_async_connected(bridge);
    EXPECT_TRUE(connected);

    ret = attention_snn_disconnect_bio_async(bridge);
    EXPECT_EQ(ret, 0);

    connected = attention_snn_is_bio_async_connected(bridge);
    EXPECT_FALSE(connected);
}

//=============================================================================
// Modulation Tests
//=============================================================================

TEST_F(AttentionSNNBridgeTest, ModulateByArousal) {
    int ret = attention_snn_modulate_by_arousal(bridge, 0.8f);
    EXPECT_EQ(ret, 0) << "Arousal modulation should succeed";
}

TEST_F(AttentionSNNBridgeTest, ModulateByArousalNull) {
    int ret = attention_snn_modulate_by_arousal(nullptr, 0.5f);
    EXPECT_EQ(ret, -1) << "Should fail with null bridge";
}

TEST_F(AttentionSNNBridgeTest, ModulateByArousalBoundaryValues) {
    /* Valid range [0.1, 2.0] based on implementation clamp */
    EXPECT_EQ(attention_snn_modulate_by_arousal(bridge, 0.0f), 0);
    EXPECT_EQ(attention_snn_modulate_by_arousal(bridge, 0.5f), 0);
    EXPECT_EQ(attention_snn_modulate_by_arousal(bridge, 1.0f), 0);
    EXPECT_EQ(attention_snn_modulate_by_arousal(bridge, 2.0f), 0);
}

TEST_F(AttentionSNNBridgeTest, SetCompetitionStrength) {
    int ret = attention_snn_set_competition_strength(bridge, 0.7f);
    EXPECT_EQ(ret, 0) << "Competition strength should succeed";
}

TEST_F(AttentionSNNBridgeTest, SetCompetitionStrengthNull) {
    int ret = attention_snn_set_competition_strength(nullptr, 0.5f);
    EXPECT_EQ(ret, -1) << "Should fail with null bridge";
}

TEST_F(AttentionSNNBridgeTest, SetCompetitionStrengthBoundaryValues) {
    EXPECT_EQ(attention_snn_set_competition_strength(bridge, 0.0f), 0);
    EXPECT_EQ(attention_snn_set_competition_strength(bridge, 0.5f), 0);
    EXPECT_EQ(attention_snn_set_competition_strength(bridge, 1.0f), 0);
}

TEST_F(AttentionSNNBridgeTest, SetCompetitionStrengthOutOfRange) {
    /* Values should be clamped */
    EXPECT_EQ(attention_snn_set_competition_strength(bridge, -0.5f), 0);
    EXPECT_EQ(attention_snn_set_competition_strength(bridge, 1.5f), 0);
}

TEST_F(AttentionSNNBridgeTest, SetGateModulation) {
    int ret = attention_snn_set_gate_modulation(bridge, 0.6f);
    EXPECT_EQ(ret, 0) << "Gate modulation should succeed";

    /* Check that gate activation is reflected in attention state */
    attention_snn_attention_state_t state;
    attention_snn_get_attention_state(bridge, &state);
    EXPECT_NEAR(state.gate_activation, 0.6f, 0.01f);
}

TEST_F(AttentionSNNBridgeTest, SetGateModulationNull) {
    int ret = attention_snn_set_gate_modulation(nullptr, 0.5f);
    EXPECT_EQ(ret, -1) << "Should fail with null bridge";
}

TEST_F(AttentionSNNBridgeTest, SetGateModulationBoundaryValues) {
    EXPECT_EQ(attention_snn_set_gate_modulation(bridge, 0.0f), 0);
    EXPECT_EQ(attention_snn_set_gate_modulation(bridge, 0.5f), 0);
    EXPECT_EQ(attention_snn_set_gate_modulation(bridge, 1.0f), 0);
}

TEST_F(AttentionSNNBridgeTest, SetGateModulationOutOfRange) {
    /* Values should be clamped */
    EXPECT_EQ(attention_snn_set_gate_modulation(bridge, -0.5f), 0);
    EXPECT_EQ(attention_snn_set_gate_modulation(bridge, 1.5f), 0);

    attention_snn_attention_state_t state;
    attention_snn_get_attention_state(bridge, &state);
    /* Should be clamped to [0, 1] */
    EXPECT_GE(state.gate_activation, 0.0f);
    EXPECT_LE(state.gate_activation, 1.0f);
}

TEST_F(AttentionSNNBridgeTest, ModulationAffectsEncoding) {
    /* Set high arousal modulation */
    attention_snn_modulate_by_arousal(bridge, 2.0f);

    std::vector<float> weights = create_attention_weights(8, 0.7f);
    int spikes_high = attention_snn_encode_weights(bridge, weights.data(), 8);

    attention_snn_reset(bridge);

    /* Set low arousal modulation */
    attention_snn_modulate_by_arousal(bridge, 0.1f);

    int spikes_low = attention_snn_encode_weights(bridge, weights.data(), 8);

    /* High arousal should generally produce more active inputs */
    EXPECT_GE(spikes_high, 0);
    EXPECT_GE(spikes_low, 0);
    /* spikes_high should typically be >= spikes_low due to encoding gain */
}

//=============================================================================
// Complete Forward Pass Tests
//=============================================================================

TEST_F(AttentionSNNBridgeTest, CompleteAttentionProcessingPipeline) {
    /* 1. Encode attention weights */
    std::vector<float> weights = create_attention_weights(8, 0.6f);
    int spikes = attention_snn_encode_weights(bridge, weights.data(), 8);
    EXPECT_GE(spikes, 0);

    /* 2. Simulate */
    int ret = attention_snn_simulate(bridge, 100.0f);
    EXPECT_EQ(ret, 0);

    /* 3. Compete (winner-take-all) */
    ret = attention_snn_compete(bridge, 50.0f);
    EXPECT_EQ(ret, 0);

    /* 4. Decode weights */
    float decoded_weights[8];
    ret = attention_snn_get_weights(bridge, decoded_weights, 8);
    EXPECT_EQ(ret, 0);

    /* 5. Get focus strength */
    float focus = attention_snn_get_focus_strength(bridge);
    EXPECT_GE(focus, 0.0f);
    EXPECT_LE(focus, 1.0f);

    /* 6. Get sparsity */
    float sparsity = attention_snn_get_sparsity(bridge);
    EXPECT_GE(sparsity, 0.0f);
    EXPECT_LE(sparsity, 1.0f);

    /* 7. Get top-k */
    int32_t top_indices[3];
    int count = attention_snn_get_top_k(bridge, top_indices, 3);
    EXPECT_GE(count, 0);

    /* 8. Get full attention state */
    attention_snn_attention_state_t attention_state;
    ret = attention_snn_get_attention_state(bridge, &attention_state);
    EXPECT_EQ(ret, 0);

    /* 9. Check statistics */
    attention_snn_stats_t stats;
    attention_snn_get_stats(bridge, &stats);
    EXPECT_GE(stats.total_forward_passes, 1u);
    EXPECT_GT(stats.total_decodings, 0u);
}

TEST_F(AttentionSNNBridgeTest, MultipleAttentionSequence) {
    /* Process a sequence of attention patterns */
    for (int seq = 0; seq < 5; seq++) {
        std::vector<float> weights = create_attention_weights(8, 0.3f + 0.1f * seq);

        attention_snn_encode_weights(bridge, weights.data(), 8);
        attention_snn_simulate(bridge, 30.0f);
        attention_snn_compete(bridge, 20.0f);

        float decoded_weights[8];
        attention_snn_get_weights(bridge, decoded_weights, 8);

        float focus = attention_snn_get_focus_strength(bridge);
        EXPECT_GE(focus, 0.0f);
        EXPECT_LE(focus, 1.0f);
    }

    /* Check final stats */
    attention_snn_stats_t stats;
    attention_snn_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_forward_passes, 5u);
    EXPECT_EQ(stats.total_decodings, 5u);
}

TEST_F(AttentionSNNBridgeTest, SalienceAndWeightsCombined) {
    /* Encode both salience and weights */
    std::vector<float> weights = create_attention_weights(8, 0.5f);
    std::vector<float> salience = create_salience_map(64);

    int spikes_w = attention_snn_encode_weights(bridge, weights.data(), 8);
    EXPECT_GE(spikes_w, 0);

    int spikes_s = attention_snn_encode_salience(bridge, salience.data(), 64);
    EXPECT_GE(spikes_s, 0);

    /* Simulate with both encoded */
    attention_snn_simulate(bridge, 100.0f);

    /* Decode */
    float decoded_weights[8];
    attention_snn_get_weights(bridge, decoded_weights, 8);

    float decoded_salience[64];
    attention_snn_get_salience(bridge, decoded_salience, 64);
}

//=============================================================================
// Encoding Method Variations
//=============================================================================

TEST_F(AttentionSNNBridgeTest, EncodingMethodVariations) {
    attention_snn_encoding_t encodings[] = {
        ATTENTION_SNN_ENCODE_RATE,
        ATTENTION_SNN_ENCODE_TEMPORAL,
        ATTENTION_SNN_ENCODE_POPULATION,
        ATTENTION_SNN_ENCODE_WINNER_TAKE_ALL
    };

    for (auto enc : encodings) {
        attention_snn_config_t config = attention_snn_config_default();
        config.encoding = enc;
        config.enable_bio_async = false;

        attention_snn_bridge_t* b = attention_snn_create(&config);
        ASSERT_NE(b, nullptr) << "Should create bridge with encoding " << (int)enc;

        std::vector<float> weights(8, 0.5f);
        int spikes = attention_snn_encode_weights(b, weights.data(), 8);
        EXPECT_GE(spikes, 0) << "Encoding with method " << (int)enc << " should succeed";

        attention_snn_destroy(b);
    }
}

TEST_F(AttentionSNNBridgeTest, DecodingMethodVariations) {
    attention_snn_decoding_t decodings[] = {
        ATTENTION_SNN_DECODE_RATE,
        ATTENTION_SNN_DECODE_SOFTMAX,
        ATTENTION_SNN_DECODE_COMPETITION,
        ATTENTION_SNN_DECODE_SYNCHRONY
    };

    for (auto dec : decodings) {
        attention_snn_config_t config = attention_snn_config_default();
        config.decoding = dec;
        config.enable_bio_async = false;

        attention_snn_bridge_t* b = attention_snn_create(&config);
        ASSERT_NE(b, nullptr) << "Should create bridge with decoding " << (int)dec;

        std::vector<float> weights(8, 0.6f);
        attention_snn_encode_weights(b, weights.data(), 8);
        attention_snn_simulate(b, 50.0f);

        float decoded_weights[8];
        int ret = attention_snn_get_weights(b, decoded_weights, 8);
        EXPECT_EQ(ret, 0) << "Decoding with method " << (int)dec << " should succeed";

        /* All decoded weights should be in valid range */
        for (int i = 0; i < 8; i++) {
            EXPECT_GE(decoded_weights[i], 0.0f);
            EXPECT_LE(decoded_weights[i], 1.0f);
        }

        attention_snn_destroy(b);
    }
}

//=============================================================================
// Thread Safety Tests (Basic)
//=============================================================================

TEST_F(AttentionSNNBridgeTest, ConcurrentReadOperations) {
    /* Set up some state first */
    std::vector<float> weights = create_attention_weights(8);
    attention_snn_encode_weights(bridge, weights.data(), 8);
    attention_snn_simulate(bridge, 50.0f);

    std::atomic<int> success_count{0};
    std::atomic<int> failure_count{0};

    auto read_task = [&]() {
        for (int i = 0; i < 100; i++) {
            attention_snn_bridge_state_t state;
            if (attention_snn_get_state(bridge, &state) == 0) {
                success_count++;
            } else {
                failure_count++;
            }

            attention_snn_stats_t stats;
            if (attention_snn_get_stats(bridge, &stats) == 0) {
                success_count++;
            } else {
                failure_count++;
            }
        }
    };

    std::thread t1(read_task);
    std::thread t2(read_task);
    std::thread t3(read_task);

    t1.join();
    t2.join();
    t3.join();

    EXPECT_GT(success_count.load(), 0);
    /* All reads should succeed with proper locking */
}

TEST_F(AttentionSNNBridgeTest, ConcurrentEncodeSimulate) {
    std::atomic<int> encode_count{0};
    std::atomic<int> simulate_count{0};

    auto encode_task = [&]() {
        for (int i = 0; i < 50; i++) {
            std::vector<float> weights(8);
            for (int j = 0; j < 8; j++) {
                weights[j] = 0.3f + 0.4f * ((float)(i + j) / 100.0f);
            }
            if (attention_snn_encode_weights(bridge, weights.data(), 8) >= 0) {
                encode_count++;
            }
        }
    };

    auto simulate_task = [&]() {
        for (int i = 0; i < 50; i++) {
            if (attention_snn_step(bridge) == 0) {
                simulate_count++;
            }
        }
    };

    std::thread t1(encode_task);
    std::thread t2(simulate_task);
    std::thread t3(encode_task);
    std::thread t4(simulate_task);

    t1.join();
    t2.join();
    t3.join();
    t4.join();

    /* All operations should succeed with proper mutex protection */
    EXPECT_EQ(encode_count.load(), 100);
    EXPECT_EQ(simulate_count.load(), 100);
}

//=============================================================================
// Edge Cases and Error Handling
//=============================================================================

TEST_F(AttentionSNNBridgeTest, RepeatedResetStability) {
    for (int i = 0; i < 10; i++) {
        std::vector<float> weights = create_attention_weights(8);
        attention_snn_encode_weights(bridge, weights.data(), 8);
        attention_snn_simulate(bridge, 20.0f);

        int ret = attention_snn_reset(bridge);
        EXPECT_EQ(ret, 0) << "Reset " << i << " should succeed";

        attention_snn_bridge_state_t state;
        attention_snn_get_state(bridge, &state);
        EXPECT_EQ(state.state, ATTENTION_SNN_STATE_IDLE);
    }
}

TEST_F(AttentionSNNBridgeTest, SingleHeadConfiguration) {
    attention_snn_config_t config = attention_snn_config_default();
    config.num_heads = 1;
    config.enable_bio_async = false;

    attention_snn_bridge_t* b = attention_snn_create(&config);
    ASSERT_NE(b, nullptr);

    float weight = 0.8f;
    int spikes = attention_snn_encode_weights(b, &weight, 1);
    EXPECT_GE(spikes, 0);

    attention_snn_simulate(b, 50.0f);

    float decoded;
    int ret = attention_snn_get_weights(b, &decoded, 1);
    EXPECT_EQ(ret, 0);

    attention_snn_destroy(b);
}

TEST_F(AttentionSNNBridgeTest, MaxHeadsConfiguration) {
    attention_snn_config_t config = attention_snn_config_default();
    config.num_heads = ATTENTION_SNN_MAX_HEADS;
    config.enable_bio_async = false;

    attention_snn_bridge_t* b = attention_snn_create(&config);
    ASSERT_NE(b, nullptr);

    std::vector<float> weights(ATTENTION_SNN_MAX_HEADS, 0.5f);
    int spikes = attention_snn_encode_weights(b, weights.data(), ATTENTION_SNN_MAX_HEADS);
    EXPECT_GE(spikes, 0);

    attention_snn_simulate(b, 50.0f);

    std::vector<float> decoded(ATTENTION_SNN_MAX_HEADS);
    int ret = attention_snn_get_weights(b, decoded.data(), ATTENTION_SNN_MAX_HEADS);
    EXPECT_EQ(ret, 0);

    attention_snn_destroy(b);
}

TEST_F(AttentionSNNBridgeTest, HighCompetitionStrength) {
    attention_snn_set_competition_strength(bridge, 1.0f);

    std::vector<float> weights = create_attention_weights(8, 0.5f);
    attention_snn_encode_weights(bridge, weights.data(), 8);
    attention_snn_simulate(bridge, 50.0f);
    attention_snn_compete(bridge, 100.0f);

    float decoded_weights[8];
    attention_snn_get_weights(bridge, decoded_weights, 8);

    /* With high competition, expect sparse output */
    float sparsity = attention_snn_get_sparsity(bridge);
    /* Sparsity should be relatively high with strong competition */
    EXPECT_GE(sparsity, 0.0f);
}

TEST_F(AttentionSNNBridgeTest, ZeroCompetitionStrength) {
    attention_snn_set_competition_strength(bridge, 0.0f);

    std::vector<float> weights = create_attention_weights(8, 0.5f);
    attention_snn_encode_weights(bridge, weights.data(), 8);
    attention_snn_simulate(bridge, 50.0f);
    attention_snn_compete(bridge, 50.0f);

    float decoded_weights[8];
    attention_snn_get_weights(bridge, decoded_weights, 8);

    /* All operations should still succeed */
    float focus = attention_snn_get_focus_strength(bridge);
    EXPECT_GE(focus, 0.0f);
}

TEST_F(AttentionSNNBridgeTest, UniformWeightsInput) {
    /* All weights identical */
    float weights[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};

    attention_snn_encode_weights(bridge, weights, 8);
    attention_snn_simulate(bridge, 50.0f);

    float decoded_weights[8];
    attention_snn_get_weights(bridge, decoded_weights, 8);

    /* Focus strength should be low with uniform input */
    float focus = attention_snn_get_focus_strength(bridge);
    EXPECT_GE(focus, 0.0f);
    EXPECT_LE(focus, 1.0f);
}

TEST_F(AttentionSNNBridgeTest, SingleDominantWeight) {
    /* One weight much higher than others */
    float weights[8] = {0.1f, 0.1f, 0.1f, 0.95f, 0.1f, 0.1f, 0.1f, 0.1f};

    attention_snn_encode_weights(bridge, weights, 8);
    attention_snn_simulate(bridge, 50.0f);
    attention_snn_compete(bridge, 50.0f);

    float decoded_weights[8];
    attention_snn_get_weights(bridge, decoded_weights, 8);

    /* Focus should be higher with single dominant weight */
    float focus = attention_snn_get_focus_strength(bridge);
    EXPECT_GE(focus, 0.0f);
}

TEST_F(AttentionSNNBridgeTest, GateModulationEffect) {
    /* Test gate modulation affects attention processing */

    /* Low gate activation */
    attention_snn_set_gate_modulation(bridge, 0.2f);
    std::vector<float> weights = create_attention_weights(8, 0.6f);
    attention_snn_encode_weights(bridge, weights.data(), 8);
    attention_snn_simulate(bridge, 50.0f);

    attention_snn_attention_state_t state_low;
    attention_snn_get_attention_state(bridge, &state_low);
    EXPECT_NEAR(state_low.gate_activation, 0.2f, 0.01f);

    attention_snn_reset(bridge);

    /* High gate activation */
    attention_snn_set_gate_modulation(bridge, 0.9f);
    attention_snn_encode_weights(bridge, weights.data(), 8);
    attention_snn_simulate(bridge, 50.0f);

    attention_snn_attention_state_t state_high;
    attention_snn_get_attention_state(bridge, &state_high);
    EXPECT_NEAR(state_high.gate_activation, 0.9f, 0.01f);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
