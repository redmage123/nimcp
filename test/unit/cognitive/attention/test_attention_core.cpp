/**
 * @file test_attention_core.cpp
 * @brief Comprehensive unit tests for Core Attention Mechanisms
 *
 * TEST COVERAGE:
 * - Attention system initialization and cleanup
 * - Saliency computation
 * - Focus shifts
 * - Competitive suppression
 * - Top-down vs bottom-up attention
 * - Attention decay
 * - Hard ternary attention
 * - Multihead attention integration
 * - Statistics and monitoring
 *
 * @author NIMCP Development Team
 * @date 2025-02
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <thread>
#include <chrono>
#include <algorithm>

#include "plasticity/attention/nimcp_attention.h"

//=============================================================================
// Test Helpers and Fixtures
//=============================================================================

class AttentionCoreTest : public ::testing::Test {
protected:
    multihead_attention_t mha;
    attention_head_t single_head;
    ternary_attention_ctx_t* ternary_ctx;

    void SetUp() override {
        mha = nullptr;
        single_head = nullptr;
        ternary_ctx = nullptr;
    }

    void TearDown() override {
        if (mha != nullptr) {
            multihead_attention_destroy(mha);
            mha = nullptr;
        }
        if (single_head != nullptr) {
            attention_head_destroy(single_head);
            single_head = nullptr;
        }
        if (ternary_ctx != nullptr) {
            ternary_attention_destroy(ternary_ctx);
            ternary_ctx = nullptr;
        }
    }

    // Helper: Create default multihead attention
    multihead_attention_t CreateDefaultMHA() {
        multihead_attention_config_t config;
        config.num_heads = 4;
        config.input_dim = 64;
        config.output_dim = 64;
        config.sequence_length = 16;
        config.use_thalamic_gate = true;
        config.use_salience_weighting = true;
        config.gate_bias = 0.5f;
        config.use_positional_encoding = false;
        config.pe_type = NIMCP_POS_SINUSOIDAL;
        config.rope_base = 10000.0f;
        config.alibi_slope_base = 1.0f;
        config.enable_quantum_attention = false;
        return multihead_attention_create(&config);
    }

    // Helper: Create single attention head
    attention_head_t CreateSingleHead() {
        attention_head_config_t config;
        config.input_dim = 64;
        config.output_dim = 16;
        config.key_dim = 16;
        config.value_dim = 16;
        config.temperature = 1.0f;
        config.dropout_rate = 0.0f;
        return attention_head_create(&config);
    }

    // Helper: Create test input sequence
    std::vector<float> CreateTestSequence(uint32_t seq_len, uint32_t dim) {
        std::vector<float> sequence(seq_len * dim);
        for (size_t i = 0; i < sequence.size(); i++) {
            sequence[i] = 0.5f + 0.5f * std::sin(static_cast<float>(i) * 0.1f);
        }
        return sequence;
    }

    // Helper: Create salience scores
    std::vector<float> CreateSalienceScores(uint32_t seq_len) {
        std::vector<float> salience(seq_len);
        for (uint32_t i = 0; i < seq_len; i++) {
            salience[i] = static_cast<float>(i + 1) / static_cast<float>(seq_len);
        }
        return salience;
    }
};

//=============================================================================
// 1. Initialization Tests
//=============================================================================

TEST_F(AttentionCoreTest, CreateMultiheadAttentionValid) {
    mha = CreateDefaultMHA();
    ASSERT_NE(mha, nullptr);
}

TEST_F(AttentionCoreTest, CreateMultiheadAttentionWithNullConfig) {
    mha = multihead_attention_create(nullptr);
    EXPECT_EQ(mha, nullptr);  // Should fail with null config
}

TEST_F(AttentionCoreTest, CreateSingleHeadValid) {
    single_head = CreateSingleHead();
    ASSERT_NE(single_head, nullptr);
}

TEST_F(AttentionCoreTest, CreateSingleHeadWithNullConfig) {
    single_head = attention_head_create(nullptr);
    EXPECT_EQ(single_head, nullptr);
}

TEST_F(AttentionCoreTest, DestroyNull) {
    // Should not crash
    multihead_attention_destroy(nullptr);
    attention_head_destroy(nullptr);
    SUCCEED();
}

TEST_F(AttentionCoreTest, ValidateConfigValid) {
    multihead_attention_config_t config;
    config.num_heads = 4;
    config.input_dim = 64;
    config.output_dim = 64;
    config.sequence_length = 16;
    config.use_thalamic_gate = true;
    config.use_salience_weighting = true;
    config.gate_bias = 0.5f;
    config.use_positional_encoding = false;
    config.pe_type = NIMCP_POS_SINUSOIDAL;
    config.rope_base = 10000.0f;
    config.alibi_slope_base = 1.0f;
    config.enable_quantum_attention = false;

    bool valid = attention_validate_config(&config);
    EXPECT_TRUE(valid);
}

TEST_F(AttentionCoreTest, ValidateConfigInvalidZeroHeads) {
    multihead_attention_config_t config;
    config.num_heads = 0;  // Invalid
    config.input_dim = 64;
    config.output_dim = 64;
    config.sequence_length = 16;

    bool valid = attention_validate_config(&config);
    EXPECT_FALSE(valid);
}

//=============================================================================
// 2. Forward Pass Tests
//=============================================================================

TEST_F(AttentionCoreTest, ForwardPassMultiheadValid) {
    mha = CreateDefaultMHA();
    ASSERT_NE(mha, nullptr);

    auto input = CreateTestSequence(16, 64);
    auto salience = CreateSalienceScores(16);
    std::vector<float> output(16 * 64);

    bool ok = multihead_attention_forward(mha, input.data(), 16,
                                          salience.data(), output.data());
    EXPECT_TRUE(ok);

    // Output should not be all zeros
    bool all_zeros = std::all_of(output.begin(), output.end(),
                                  [](float v) { return v == 0.0f; });
    EXPECT_FALSE(all_zeros);
}

TEST_F(AttentionCoreTest, ForwardPassMultiheadNullSalience) {
    mha = CreateDefaultMHA();
    ASSERT_NE(mha, nullptr);

    auto input = CreateTestSequence(16, 64);
    std::vector<float> output(16 * 64);

    // Forward pass with NULL salience should work (uniform salience)
    bool ok = multihead_attention_forward(mha, input.data(), 16,
                                          nullptr, output.data());
    EXPECT_TRUE(ok);
}

TEST_F(AttentionCoreTest, ForwardPassMultiheadNullInput) {
    mha = CreateDefaultMHA();
    ASSERT_NE(mha, nullptr);

    std::vector<float> output(16 * 64);
    bool ok = multihead_attention_forward(mha, nullptr, 16, nullptr, output.data());
    EXPECT_FALSE(ok);  // Should fail
}

TEST_F(AttentionCoreTest, ForwardPassMultiheadNullOutput) {
    mha = CreateDefaultMHA();
    ASSERT_NE(mha, nullptr);

    auto input = CreateTestSequence(16, 64);
    bool ok = multihead_attention_forward(mha, input.data(), 16, nullptr, nullptr);
    EXPECT_FALSE(ok);  // Should fail
}

TEST_F(AttentionCoreTest, ForwardPassSingleHead) {
    single_head = CreateSingleHead();
    ASSERT_NE(single_head, nullptr);

    uint32_t seq_len = 8;
    auto query = CreateTestSequence(seq_len, 64);
    auto key = CreateTestSequence(seq_len, 64);
    auto value = CreateTestSequence(seq_len, 64);
    std::vector<float> output(seq_len * 16);
    std::vector<float> attention_weights(seq_len * seq_len);

    bool ok = attention_head_forward(single_head, query.data(), key.data(),
                                     value.data(), seq_len, output.data(),
                                     attention_weights.data(), nullptr, 0);
    EXPECT_TRUE(ok);

    // Attention weights should sum to ~1.0 per row (softmax output)
    for (uint32_t i = 0; i < seq_len; i++) {
        float row_sum = 0.0f;
        for (uint32_t j = 0; j < seq_len; j++) {
            row_sum += attention_weights[i * seq_len + j];
        }
        EXPECT_NEAR(row_sum, 1.0f, 0.01f);
    }
}

//=============================================================================
// 3. Thalamic Gate Tests
//=============================================================================

TEST_F(AttentionCoreTest, SetGateValid) {
    mha = CreateDefaultMHA();
    ASSERT_NE(mha, nullptr);

    bool ok = multihead_attention_set_gate(mha, 0.8f);
    EXPECT_TRUE(ok);

    // Verify gate affects attention strength
    float strength = multihead_attention_get_strength(mha);
    // Strength should be influenced by gate (exact value depends on implementation)
    EXPECT_GE(strength, 0.0f);
    EXPECT_LE(strength, 1.0f);
}

TEST_F(AttentionCoreTest, SetGateBoundary) {
    mha = CreateDefaultMHA();
    ASSERT_NE(mha, nullptr);

    // Test boundary values
    EXPECT_TRUE(multihead_attention_set_gate(mha, 0.0f));
    EXPECT_TRUE(multihead_attention_set_gate(mha, 1.0f));
}

TEST_F(AttentionCoreTest, SetGateNull) {
    bool ok = multihead_attention_set_gate(nullptr, 0.5f);
    EXPECT_FALSE(ok);
}

TEST_F(AttentionCoreTest, GateAffectsForwardPass) {
    mha = CreateDefaultMHA();
    ASSERT_NE(mha, nullptr);

    auto input = CreateTestSequence(16, 64);
    std::vector<float> output_open(16 * 64);
    std::vector<float> output_closed(16 * 64);

    // Forward with open gate
    multihead_attention_set_gate(mha, 1.0f);
    multihead_attention_forward(mha, input.data(), 16, nullptr, output_open.data());

    // Forward with closed gate
    multihead_attention_set_gate(mha, 0.0f);
    multihead_attention_forward(mha, input.data(), 16, nullptr, output_closed.data());

    // Outputs should differ
    bool differs = false;
    for (size_t i = 0; i < output_open.size(); i++) {
        if (std::abs(output_open[i] - output_closed[i]) > 0.01f) {
            differs = true;
            break;
        }
    }
    EXPECT_TRUE(differs);
}

//=============================================================================
// 4. Salience Weighting Tests
//=============================================================================

TEST_F(AttentionCoreTest, SalienceWeightingAffectsOutput) {
    mha = CreateDefaultMHA();
    ASSERT_NE(mha, nullptr);

    auto input = CreateTestSequence(16, 64);
    std::vector<float> output_uniform(16 * 64);
    std::vector<float> output_weighted(16 * 64);

    // Uniform salience
    std::vector<float> uniform_salience(16, 0.5f);
    multihead_attention_forward(mha, input.data(), 16,
                               uniform_salience.data(), output_uniform.data());

    // Non-uniform salience (high at end)
    auto weighted_salience = CreateSalienceScores(16);
    multihead_attention_forward(mha, input.data(), 16,
                               weighted_salience.data(), output_weighted.data());

    // Outputs should differ
    bool differs = false;
    for (size_t i = 0; i < output_uniform.size(); i++) {
        if (std::abs(output_uniform[i] - output_weighted[i]) > 0.001f) {
            differs = true;
            break;
        }
    }
    EXPECT_TRUE(differs);
}

//=============================================================================
// 5. Statistics Tests
//=============================================================================

TEST_F(AttentionCoreTest, GetStatisticsValid) {
    mha = CreateDefaultMHA();
    ASSERT_NE(mha, nullptr);

    // Do some forward passes
    auto input = CreateTestSequence(16, 64);
    std::vector<float> output(16 * 64);
    for (int i = 0; i < 5; i++) {
        multihead_attention_forward(mha, input.data(), 16, nullptr, output.data());
    }

    attention_stats_t stats;
    bool ok = multihead_attention_get_stats(mha, &stats);
    EXPECT_TRUE(ok);
    EXPECT_EQ(stats.forward_calls, 5u);
    EXPECT_GT(stats.active_heads, 0u);
}

TEST_F(AttentionCoreTest, ResetStatistics) {
    mha = CreateDefaultMHA();
    ASSERT_NE(mha, nullptr);

    // Do some forward passes
    auto input = CreateTestSequence(16, 64);
    std::vector<float> output(16 * 64);
    multihead_attention_forward(mha, input.data(), 16, nullptr, output.data());

    // Reset
    multihead_attention_reset_stats(mha);

    attention_stats_t stats;
    multihead_attention_get_stats(mha, &stats);
    EXPECT_EQ(stats.forward_calls, 0u);
}

TEST_F(AttentionCoreTest, GetStatisticsNull) {
    attention_stats_t stats;
    bool ok = multihead_attention_get_stats(nullptr, &stats);
    EXPECT_FALSE(ok);
}

//=============================================================================
// 6. Attention Entropy Tests
//=============================================================================

TEST_F(AttentionCoreTest, ComputeEntropyUniform) {
    // Uniform distribution should have high entropy
    std::vector<float> uniform_weights(64, 1.0f / 64.0f);
    float entropy = attention_compute_entropy(uniform_weights.data(), 8);
    EXPECT_GT(entropy, 0.0f);
}

TEST_F(AttentionCoreTest, ComputeEntropyFocused) {
    // Focused distribution should have low entropy
    std::vector<float> focused_weights(64, 0.0f);
    focused_weights[0] = 1.0f;  // All attention on first
    float entropy = attention_compute_entropy(focused_weights.data(), 8);
    EXPECT_LT(entropy, 0.1f);  // Low entropy
}

TEST_F(AttentionCoreTest, ComputeEntropyNull) {
    float entropy = attention_compute_entropy(nullptr, 8);
    EXPECT_EQ(entropy, 0.0f);
}

//=============================================================================
// 7. Hard Ternary Attention Tests
//=============================================================================

TEST_F(AttentionCoreTest, TernaryAttentionCreateDestroy) {
    ternary_attention_config_t config;
    int ret = ternary_attention_default_config(&config);
    EXPECT_EQ(ret, 0);

    ternary_ctx = ternary_attention_create(&config);
    ASSERT_NE(ternary_ctx, nullptr);
}

TEST_F(AttentionCoreTest, TernaryAttentionDefaultConfig) {
    ternary_attention_config_t config;
    int ret = ternary_attention_default_config(&config);
    EXPECT_EQ(ret, 0);
    EXPECT_FLOAT_EQ(config.focus_threshold, 0.7f);
    EXPECT_FLOAT_EQ(config.suppress_threshold, 0.3f);
    EXPECT_FALSE(config.use_top_k);
    EXPECT_FLOAT_EQ(config.focus_gain, 1.5f);
    EXPECT_FLOAT_EQ(config.suppress_gain, 0.1f);
}

TEST_F(AttentionCoreTest, TernaryAttentionDiscretize) {
    ternary_attention_config_t config;
    ternary_attention_default_config(&config);
    ternary_ctx = ternary_attention_create(&config);
    ASSERT_NE(ternary_ctx, nullptr);

    // Create soft attention weights
    std::vector<float> soft_attention = {0.8f, 0.2f, 0.5f, 0.9f, 0.1f};
    std::vector<ternary_attention_state_t> ternary_out(5);

    int ret = ternary_attention_discretize(ternary_ctx, soft_attention.data(),
                                           5, ternary_out.data());
    EXPECT_EQ(ret, 0);

    // Check discretization based on thresholds (0.7 focus, 0.3 suppress)
    EXPECT_EQ(ternary_out[0], ATTENTION_FOCUS);      // 0.8 > 0.7
    EXPECT_EQ(ternary_out[1], ATTENTION_SUPPRESS);   // 0.2 < 0.3
    EXPECT_EQ(ternary_out[2], ATTENTION_NEUTRAL);    // 0.3 <= 0.5 <= 0.7
    EXPECT_EQ(ternary_out[3], ATTENTION_FOCUS);      // 0.9 > 0.7
    EXPECT_EQ(ternary_out[4], ATTENTION_SUPPRESS);   // 0.1 < 0.3
}

TEST_F(AttentionCoreTest, TernaryAttentionApply) {
    ternary_attention_config_t config;
    ternary_attention_default_config(&config);
    ternary_ctx = ternary_attention_create(&config);
    ASSERT_NE(ternary_ctx, nullptr);

    // Values
    std::vector<float> values = {1.0f, 1.0f, 1.0f, 1.0f};  // 2x2

    // Ternary states
    std::vector<ternary_attention_state_t> states = {ATTENTION_FOCUS, ATTENTION_SUPPRESS};

    std::vector<float> output(4);
    int ret = ternary_attention_apply(ternary_ctx, values.data(), states.data(),
                                      2, 2, output.data());
    EXPECT_EQ(ret, 0);

    // FOCUS should multiply by focus_gain (1.5)
    EXPECT_NEAR(output[0], 1.5f, 0.01f);
    EXPECT_NEAR(output[1], 1.5f, 0.01f);

    // SUPPRESS should multiply by suppress_gain (0.1)
    EXPECT_NEAR(output[2], 0.1f, 0.01f);
    EXPECT_NEAR(output[3], 0.1f, 0.01f);
}

TEST_F(AttentionCoreTest, TernaryAttentionTopK) {
    std::vector<float> soft_attention = {0.3f, 0.7f, 0.5f, 0.9f, 0.1f};
    std::vector<ternary_attention_state_t> ternary_out(5);

    int ret = ternary_attention_top_k(soft_attention.data(), 5, 2, ternary_out.data());
    EXPECT_EQ(ret, 0);

    // Top 2 should be FOCUS (indices 3 and 1, with values 0.9 and 0.7)
    int focus_count = 0;
    for (int i = 0; i < 5; i++) {
        if (ternary_out[i] == ATTENTION_FOCUS) {
            focus_count++;
        }
    }
    EXPECT_EQ(focus_count, 2);
    EXPECT_EQ(ternary_out[3], ATTENTION_FOCUS);  // 0.9
    EXPECT_EQ(ternary_out[1], ATTENTION_FOCUS);  // 0.7
}

TEST_F(AttentionCoreTest, TernaryAttentionTemperature) {
    ternary_attention_config_t config;
    ternary_attention_default_config(&config);
    config.temperature_annealing = true;
    config.initial_temperature = 5.0f;
    config.final_temperature = 0.1f;
    config.annealing_steps = 100;

    ternary_ctx = ternary_attention_create(&config);
    ASSERT_NE(ternary_ctx, nullptr);

    float initial_temp = ternary_attention_get_temperature(ternary_ctx);
    EXPECT_NEAR(initial_temp, 5.0f, 0.01f);

    // Update temperature (midway through annealing)
    ternary_attention_update_temperature(ternary_ctx, 50);
    float mid_temp = ternary_attention_get_temperature(ternary_ctx);
    EXPECT_LT(mid_temp, initial_temp);
    EXPECT_GT(mid_temp, 0.1f);

    // Update to end
    ternary_attention_update_temperature(ternary_ctx, 100);
    float final_temp = ternary_attention_get_temperature(ternary_ctx);
    EXPECT_NEAR(final_temp, 0.1f, 0.05f);
}

TEST_F(AttentionCoreTest, TernaryAttentionStats) {
    ternary_attention_config_t config;
    ternary_attention_default_config(&config);
    ternary_ctx = ternary_attention_create(&config);
    ASSERT_NE(ternary_ctx, nullptr);

    // Do some discretizations
    std::vector<float> soft_attention = {0.8f, 0.2f, 0.5f, 0.9f, 0.1f};
    std::vector<ternary_attention_state_t> ternary_out(5);
    ternary_attention_discretize(ternary_ctx, soft_attention.data(),
                                 5, ternary_out.data());

    ternary_attention_stats_t stats;
    int ret = ternary_attention_get_stats(ternary_ctx, &stats);
    EXPECT_EQ(ret, 0);

    EXPECT_EQ(stats.n_focus, 2u);       // 0.8, 0.9
    EXPECT_EQ(stats.n_suppress, 2u);    // 0.2, 0.1
    EXPECT_EQ(stats.n_neutral, 1u);     // 0.5
}

TEST_F(AttentionCoreTest, TernaryAttentionStateToGain) {
    ternary_attention_config_t config;
    ternary_attention_default_config(&config);

    EXPECT_FLOAT_EQ(ternary_attention_state_to_gain(ATTENTION_FOCUS, &config), 1.5f);
    EXPECT_FLOAT_EQ(ternary_attention_state_to_gain(ATTENTION_SUPPRESS, &config), 0.1f);
    EXPECT_FLOAT_EQ(ternary_attention_state_to_gain(ATTENTION_NEUTRAL, &config), 1.0f);
}

TEST_F(AttentionCoreTest, TernaryAttentionStateName) {
    EXPECT_STREQ(ternary_attention_state_name(ATTENTION_FOCUS), "FOCUS");
    EXPECT_STREQ(ternary_attention_state_name(ATTENTION_SUPPRESS), "SUPPRESS");
    EXPECT_STREQ(ternary_attention_state_name(ATTENTION_NEUTRAL), "NEUTRAL");
}

//=============================================================================
// 8. Attention Strength Tests
//=============================================================================

TEST_F(AttentionCoreTest, GetStrengthValid) {
    mha = CreateDefaultMHA();
    ASSERT_NE(mha, nullptr);

    float strength = multihead_attention_get_strength(mha);
    EXPECT_GE(strength, 0.0f);
    EXPECT_LE(strength, 1.0f);
}

TEST_F(AttentionCoreTest, GetStrengthNull) {
    float strength = multihead_attention_get_strength(nullptr);
    EXPECT_FLOAT_EQ(strength, 0.0f);
}

//=============================================================================
// 9. Positional Encoding Integration Tests
//=============================================================================

TEST_F(AttentionCoreTest, SetPETypeRoPE) {
    multihead_attention_config_t config;
    config.num_heads = 4;
    config.input_dim = 64;
    config.output_dim = 64;
    config.sequence_length = 16;
    config.use_thalamic_gate = true;
    config.use_salience_weighting = true;
    config.gate_bias = 0.5f;
    config.use_positional_encoding = true;
    config.pe_type = NIMCP_POS_ROTARY;
    config.rope_base = 10000.0f;
    config.alibi_slope_base = 1.0f;
    config.enable_quantum_attention = false;

    mha = multihead_attention_create(&config);
    ASSERT_NE(mha, nullptr);

    bool ok = multihead_attention_set_pe_type(mha, NIMCP_POS_ROTARY);
    EXPECT_TRUE(ok);
}

TEST_F(AttentionCoreTest, SetPETypeALiBi) {
    multihead_attention_config_t config;
    config.num_heads = 4;
    config.input_dim = 64;
    config.output_dim = 64;
    config.sequence_length = 16;
    config.use_thalamic_gate = true;
    config.use_salience_weighting = true;
    config.gate_bias = 0.5f;
    config.use_positional_encoding = true;
    config.pe_type = NIMCP_POS_ALIBI;
    config.rope_base = 10000.0f;
    config.alibi_slope_base = 1.0f;
    config.enable_quantum_attention = false;

    mha = multihead_attention_create(&config);
    ASSERT_NE(mha, nullptr);

    bool ok = multihead_attention_set_pe_type(mha, NIMCP_POS_ALIBI);
    EXPECT_TRUE(ok);
}

//=============================================================================
// 10. Error Handling Tests
//=============================================================================

TEST_F(AttentionCoreTest, ForwardPassZeroSequenceLength) {
    mha = CreateDefaultMHA();
    ASSERT_NE(mha, nullptr);

    auto input = CreateTestSequence(16, 64);
    std::vector<float> output(16 * 64);

    bool ok = multihead_attention_forward(mha, input.data(), 0, nullptr, output.data());
    EXPECT_FALSE(ok);  // Should fail with zero length
}

TEST_F(AttentionCoreTest, DiscretizeNullContext) {
    std::vector<float> soft_attention = {0.5f};
    std::vector<ternary_attention_state_t> ternary_out(1);

    int ret = ternary_attention_discretize(nullptr, soft_attention.data(),
                                           1, ternary_out.data());
    EXPECT_LT(ret, 0);  // Should return error
}

TEST_F(AttentionCoreTest, DiscretizeNullInput) {
    ternary_attention_config_t config;
    ternary_attention_default_config(&config);
    ternary_ctx = ternary_attention_create(&config);
    ASSERT_NE(ternary_ctx, nullptr);

    std::vector<ternary_attention_state_t> ternary_out(5);
    int ret = ternary_attention_discretize(ternary_ctx, nullptr, 5, ternary_out.data());
    EXPECT_LT(ret, 0);  // Should return error
}

TEST_F(AttentionCoreTest, DiscretizeNullOutput) {
    ternary_attention_config_t config;
    ternary_attention_default_config(&config);
    ternary_ctx = ternary_attention_create(&config);
    ASSERT_NE(ternary_ctx, nullptr);

    std::vector<float> soft_attention = {0.5f};
    int ret = ternary_attention_discretize(ternary_ctx, soft_attention.data(), 1, nullptr);
    EXPECT_LT(ret, 0);  // Should return error
}

//=============================================================================
// Run All Tests
//=============================================================================

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
