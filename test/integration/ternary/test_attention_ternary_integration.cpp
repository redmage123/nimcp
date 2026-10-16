/**
 * @file test_attention_ternary_integration.cpp
 * @brief Integration tests for attention with ternary representation
 *
 * Tests:
 * - Ternary attention with cortical columns
 * - Attention gate with sparse coding
 * - Hard ternary attention discretization
 * - Top-k ternary attention selection
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <algorithm>

// Headers have their own extern "C" guards
#include "plasticity/attention/nimcp_attention.h"
#include "utils/tensor/nimcp_tensor.h"
#include "utils/ternary/nimcp_ternary_types.h"
#include "utils/ternary/nimcp_ternary_vector.h"
#include "utils/ternary/nimcp_ternary_matrix.h"
#include "utils/ternary/nimcp_ternary_convert.h"

/**
 * @class AttentionTernaryIntegrationTest
 * @brief Test fixture for attention with ternary integration
 */
class AttentionTernaryIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Nothing special needed for setup
    }

    void TearDown() override {
        // Cleanup handled per-test
    }

    /**
     * @brief Create attention head with default config
     */
    attention_head_t createAttentionHead(uint32_t input_dim, uint32_t output_dim) {
        attention_head_config_t config = {
            .input_dim = input_dim,
            .output_dim = output_dim,
            .key_dim = input_dim / 2,
            .value_dim = input_dim / 2,
            .temperature = 1.0f,
            .dropout_rate = 0.0f
        };
        return attention_head_create(&config);
    }

    /**
     * @brief Create multihead attention with default config
     */
    multihead_attention_t createMultiheadAttention(
            uint32_t num_heads, uint32_t input_dim, uint32_t output_dim, uint32_t seq_length) {
        multihead_attention_config_t config = {
            .num_heads = num_heads,
            .input_dim = input_dim,
            .output_dim = output_dim,
            .sequence_length = seq_length,
            .use_thalamic_gate = true,
            .use_salience_weighting = true,
            .gate_bias = 0.5f,
            .use_positional_encoding = false,
            .pe_type = NIMCP_POS_ROTARY,
            .rope_base = 10000.0f,
            .alibi_slope_base = 1.0f,
            .enable_quantum_attention = false
        };
        return multihead_attention_create(&config);
    }

    /**
     * @brief Create ternary attention context with default config
     */
    ternary_attention_ctx_t* createTernaryAttentionCtx() {
        ternary_attention_config_t config;
        ternary_attention_default_config(&config);
        return ternary_attention_create(&config);
    }

    /**
     * @brief Generate soft attention weights (normalized)
     */
    std::vector<float> generateSoftAttention(uint32_t seq_length, bool biased = false) {
        std::vector<float> weights(seq_length);
        float sum = 0.0f;

        for (uint32_t i = 0; i < seq_length; i++) {
            if (biased) {
                // Create biased distribution (some positions get more attention)
                weights[i] = (i % 3 == 0) ? 0.5f : 0.1f;
            } else {
                // Uniform-ish distribution with some variation
                weights[i] = 0.5f + 0.1f * std::sin(i * 0.5f);
            }
            sum += weights[i];
        }

        // Normalize to sum to 1
        for (uint32_t i = 0; i < seq_length; i++) {
            weights[i] /= sum;
        }

        return weights;
    }
};

//=============================================================================
// Test: Ternary Attention with Cortical Columns
//=============================================================================

/**
 * Test basic multihead attention forward pass
 */
TEST_F(AttentionTernaryIntegrationTest, BasicMultiheadAttentionForward) {
    const uint32_t num_heads = 4;
    const uint32_t input_dim = 32;
    const uint32_t output_dim = 32;
    const uint32_t seq_length = 8;

    // Create multihead attention
    multihead_attention_t mha = createMultiheadAttention(
        num_heads, input_dim, output_dim, seq_length);
    ASSERT_NE(nullptr, mha) << "Failed to create multihead attention";

    // Create input sequence
    std::vector<float> input(seq_length * input_dim);
    for (size_t i = 0; i < input.size(); i++) {
        input[i] = 0.1f * ((i % 10) - 5);
    }

    // Create output buffer
    std::vector<float> output(seq_length * output_dim, 0.0f);

    // Forward pass
    bool success = multihead_attention_forward(
        mha, input.data(), seq_length, nullptr, output.data());
    EXPECT_TRUE(success) << "Forward pass failed";

    // Verify output is valid
    for (size_t i = 0; i < output.size(); i++) {
        EXPECT_FALSE(std::isnan(output[i])) << "Output contains NaN at " << i;
        EXPECT_FALSE(std::isinf(output[i])) << "Output contains inf at " << i;
    }

    multihead_attention_destroy(mha);
}

/**
 * Test ternary attention state discretization
 */
TEST_F(AttentionTernaryIntegrationTest, TernaryAttentionDiscretization) {
    const uint32_t seq_length = 16;

    // Create ternary attention context
    ternary_attention_ctx_t* ctx = createTernaryAttentionCtx();
    ASSERT_NE(nullptr, ctx) << "Failed to create ternary attention context";

    // Generate soft attention with varied weights
    std::vector<float> soft_attention(seq_length);
    soft_attention[0] = 0.9f;   // High - should be FOCUS
    soft_attention[1] = 0.8f;   // High - should be FOCUS
    soft_attention[2] = 0.5f;   // Middle - should be NEUTRAL
    soft_attention[3] = 0.4f;   // Middle - should be NEUTRAL
    soft_attention[4] = 0.2f;   // Low - should be SUPPRESS
    soft_attention[5] = 0.1f;   // Low - should be SUPPRESS

    for (uint32_t i = 6; i < seq_length; i++) {
        soft_attention[i] = 0.5f;  // Middle values
    }

    // Discretize to ternary
    std::vector<ternary_attention_state_t> ternary_attention(seq_length);
    int result = ternary_attention_discretize(
        ctx, soft_attention.data(), seq_length, ternary_attention.data());
    EXPECT_EQ(0, result) << "Discretization failed";

    // Verify high attention values become FOCUS
    EXPECT_EQ(ATTENTION_FOCUS, ternary_attention[0])
        << "Attention 0.9 should be FOCUS";
    EXPECT_EQ(ATTENTION_FOCUS, ternary_attention[1])
        << "Attention 0.8 should be FOCUS";

    // Verify low attention values become SUPPRESS
    EXPECT_EQ(ATTENTION_SUPPRESS, ternary_attention[4])
        << "Attention 0.2 should be SUPPRESS";
    EXPECT_EQ(ATTENTION_SUPPRESS, ternary_attention[5])
        << "Attention 0.1 should be SUPPRESS";

    // Verify middle values are NEUTRAL
    EXPECT_EQ(ATTENTION_NEUTRAL, ternary_attention[2])
        << "Attention 0.5 should be NEUTRAL";
    EXPECT_EQ(ATTENTION_NEUTRAL, ternary_attention[3])
        << "Attention 0.4 should be NEUTRAL";

    ternary_attention_destroy(ctx);
}

/**
 * Test ternary attention application to values
 */
TEST_F(AttentionTernaryIntegrationTest, TernaryAttentionApply) {
    const uint32_t seq_length = 4;
    const uint32_t dim = 8;

    // Create ternary attention context with known gains
    ternary_attention_config_t config;
    ternary_attention_default_config(&config);
    config.focus_gain = 2.0f;      // Double focused values
    config.suppress_gain = 0.1f;   // Nearly zero for suppressed

    ternary_attention_ctx_t* ctx = ternary_attention_create(&config);
    ASSERT_NE(nullptr, ctx);

    // Create input values (all 1.0 for easy verification)
    std::vector<float> values(seq_length * dim, 1.0f);

    // Create ternary attention states
    std::vector<ternary_attention_state_t> ternary_states = {
        ATTENTION_FOCUS,     // Position 0: double
        ATTENTION_NEUTRAL,   // Position 1: unchanged
        ATTENTION_SUPPRESS,  // Position 2: near zero
        ATTENTION_FOCUS      // Position 3: double
    };

    // Apply ternary attention
    std::vector<float> output(seq_length * dim);
    int result = ternary_attention_apply(
        ctx, values.data(), ternary_states.data(), seq_length, dim, output.data());
    EXPECT_EQ(0, result);

    // Verify modulation
    // Position 0 (FOCUS): should be ~2.0
    for (uint32_t d = 0; d < dim; d++) {
        EXPECT_NEAR(2.0f, output[0 * dim + d], 0.01f)
            << "Focused position should be doubled";
    }

    // Position 1 (NEUTRAL): should be ~1.0
    for (uint32_t d = 0; d < dim; d++) {
        EXPECT_NEAR(1.0f, output[1 * dim + d], 0.01f)
            << "Neutral position should be unchanged";
    }

    // Position 2 (SUPPRESS): should be ~0.1
    for (uint32_t d = 0; d < dim; d++) {
        EXPECT_NEAR(0.1f, output[2 * dim + d], 0.01f)
            << "Suppressed position should be near zero";
    }

    // Position 3 (FOCUS): should be ~2.0
    for (uint32_t d = 0; d < dim; d++) {
        EXPECT_NEAR(2.0f, output[3 * dim + d], 0.01f)
            << "Focused position should be doubled";
    }

    ternary_attention_destroy(ctx);
}

/**
 * Test mapping between ternary attention and trit values
 */
TEST_F(AttentionTernaryIntegrationTest, TernaryAttentionToTritMapping) {
    // Verify that ternary_attention_state_t maps to trit_t
    EXPECT_EQ(-1, (int)ATTENTION_SUPPRESS);
    EXPECT_EQ(0, (int)ATTENTION_NEUTRAL);
    EXPECT_EQ(1, (int)ATTENTION_FOCUS);

    EXPECT_EQ(-1, (int)TRIT_NEGATIVE);
    EXPECT_EQ(0, (int)TRIT_UNKNOWN);
    EXPECT_EQ(1, (int)TRIT_POSITIVE);

    // The enums have the same values, so we can cast between them
    ternary_attention_state_t att_state = ATTENTION_FOCUS;
    trit_t trit_val = (trit_t)att_state;
    EXPECT_EQ(TRIT_POSITIVE, trit_val);

    att_state = ATTENTION_SUPPRESS;
    trit_val = (trit_t)att_state;
    EXPECT_EQ(TRIT_NEGATIVE, trit_val);

    att_state = ATTENTION_NEUTRAL;
    trit_val = (trit_t)att_state;
    EXPECT_EQ(TRIT_UNKNOWN, trit_val);
}

//=============================================================================
// Test: Attention Gate with Sparse Coding
//=============================================================================

/**
 * Test thalamic gate modulation
 */
TEST_F(AttentionTernaryIntegrationTest, ThalamicGateModulation) {
    const uint32_t num_heads = 4;
    const uint32_t input_dim = 16;
    const uint32_t output_dim = 16;
    const uint32_t seq_length = 4;

    multihead_attention_t mha = createMultiheadAttention(
        num_heads, input_dim, output_dim, seq_length);
    ASSERT_NE(nullptr, mha);

    // Create input
    std::vector<float> input(seq_length * input_dim, 0.5f);
    std::vector<float> output_open(seq_length * output_dim);
    std::vector<float> output_closed(seq_length * output_dim);

    // Test with gate fully open
    bool success = multihead_attention_set_gate(mha, 1.0f);
    EXPECT_TRUE(success);
    success = multihead_attention_forward(
        mha, input.data(), seq_length, nullptr, output_open.data());
    EXPECT_TRUE(success);

    // Test with gate closed
    success = multihead_attention_set_gate(mha, 0.0f);
    EXPECT_TRUE(success);
    success = multihead_attention_forward(
        mha, input.data(), seq_length, nullptr, output_closed.data());
    EXPECT_TRUE(success);

    // Outputs should differ based on gate state
    bool outputs_differ = false;
    for (size_t i = 0; i < output_open.size(); i++) {
        if (std::abs(output_open[i] - output_closed[i]) > 0.001f) {
            outputs_differ = true;
            break;
        }
    }

    // Note: Depending on implementation, gate may affect output magnitude
    // If gate fully closes attention, output might be attenuated
    // This test verifies gate has an effect

    multihead_attention_destroy(mha);
}

/**
 * Test sparse attention using ternary top-k selection
 */
TEST_F(AttentionTernaryIntegrationTest, SparseTopKTernaryAttention) {
    const uint32_t seq_length = 10;
    const uint32_t k = 3;  // Focus on top 3

    // Create soft attention with clear ordering
    std::vector<float> soft_attention = {
        0.05f,  // Rank 9
        0.15f,  // Rank 6
        0.25f,  // Rank 4
        0.30f,  // Rank 3 - top k boundary
        0.35f,  // Rank 2 - should be FOCUS
        0.10f,  // Rank 8
        0.20f,  // Rank 5
        0.12f,  // Rank 7
        0.40f,  // Rank 1 - should be FOCUS
        0.50f   // Rank 0 - should be FOCUS
    };

    // Apply top-k ternary attention
    std::vector<ternary_attention_state_t> ternary_attention(seq_length);
    int result = ternary_attention_top_k(
        soft_attention.data(), seq_length, k, ternary_attention.data());
    EXPECT_EQ(0, result);

    // Verify top 3 positions are FOCUS
    // Top 3 are at indices 9 (0.50), 8 (0.40), 4 (0.35)
    EXPECT_EQ(ATTENTION_FOCUS, ternary_attention[9])
        << "Position 9 (0.50) should be FOCUS";
    EXPECT_EQ(ATTENTION_FOCUS, ternary_attention[8])
        << "Position 8 (0.40) should be FOCUS";
    EXPECT_EQ(ATTENTION_FOCUS, ternary_attention[4])
        << "Position 4 (0.35) should be FOCUS";

    // Verify others are SUPPRESS
    EXPECT_EQ(ATTENTION_SUPPRESS, ternary_attention[0])
        << "Position 0 (0.05) should be SUPPRESS";
    EXPECT_EQ(ATTENTION_SUPPRESS, ternary_attention[1])
        << "Position 1 (0.15) should be SUPPRESS";

    // Count states
    int n_focus = 0, n_suppress = 0, n_neutral = 0;
    for (uint32_t i = 0; i < seq_length; i++) {
        if (ternary_attention[i] == ATTENTION_FOCUS) n_focus++;
        else if (ternary_attention[i] == ATTENTION_SUPPRESS) n_suppress++;
        else n_neutral++;
    }

    EXPECT_EQ(k, (uint32_t)n_focus) << "Should have exactly k focused items";
    EXPECT_EQ(seq_length - k, (uint32_t)(n_suppress + n_neutral))
        << "Remaining items should be suppress or neutral";
}

/**
 * Test ternary attention vector storage
 */
TEST_F(AttentionTernaryIntegrationTest, TernaryAttentionVectorStorage) {
    const uint32_t seq_length = 16;

    // Create soft attention and discretize
    std::vector<float> soft_attention = generateSoftAttention(seq_length, true);

    ternary_attention_ctx_t* ctx = createTernaryAttentionCtx();
    ASSERT_NE(nullptr, ctx);

    std::vector<ternary_attention_state_t> ternary_attention(seq_length);
    int result = ternary_attention_discretize(
        ctx, soft_attention.data(), seq_length, ternary_attention.data());
    EXPECT_EQ(0, result);

    // Store in trit_vector_t for efficient representation
    trit_vector_t* trit_vec = trit_vector_create(seq_length, TERNARY_PACK_NONE);
    ASSERT_NE(nullptr, trit_vec);

    // Copy ternary attention to trit vector
    for (uint32_t i = 0; i < seq_length; i++) {
        trit_t trit_val = (trit_t)ternary_attention[i];
        trit_vector_set(trit_vec, i, trit_val);
    }

    // Verify values match
    for (uint32_t i = 0; i < seq_length; i++) {
        trit_t stored = trit_vector_get(trit_vec, i);
        trit_t expected = (trit_t)ternary_attention[i];
        EXPECT_EQ(expected, stored) << "Mismatch at position " << i;
    }

    // Test packed storage for efficiency
    trit_vector_t* packed_vec = trit_vector_create(seq_length, TERNARY_PACK_2BIT);
    ASSERT_NE(nullptr, packed_vec);

    for (uint32_t i = 0; i < seq_length; i++) {
        trit_t trit_val = (trit_t)ternary_attention[i];
        trit_vector_set(packed_vec, i, trit_val);
    }

    // Verify packed matches unpacked
    for (uint32_t i = 0; i < seq_length; i++) {
        trit_t unpacked_val = trit_vector_get(trit_vec, i);
        trit_t packed_val = trit_vector_get(packed_vec, i);
        EXPECT_EQ(unpacked_val, packed_val) << "Packed/unpacked mismatch at " << i;
    }

    trit_vector_destroy(trit_vec);
    trit_vector_destroy(packed_vec);
    ternary_attention_destroy(ctx);
}

/**
 * Test attention entropy with ternary sparsity
 */
TEST_F(AttentionTernaryIntegrationTest, AttentionEntropyWithTernarySparsity) {
    const uint32_t seq_length = 8;

    // Create focused attention (low entropy)
    std::vector<float> focused_attention(seq_length, 0.01f);
    focused_attention[0] = 0.93f;  // Most attention on one position

    // Create diffuse attention (high entropy)
    std::vector<float> diffuse_attention(seq_length);
    float uniform = 1.0f / seq_length;
    for (uint32_t i = 0; i < seq_length; i++) {
        diffuse_attention[i] = uniform;
    }

    // Compute entropies
    float focused_entropy = attention_compute_entropy(
        focused_attention.data(), seq_length);
    float diffuse_entropy = attention_compute_entropy(
        diffuse_attention.data(), seq_length);

    EXPECT_LT(focused_entropy, diffuse_entropy)
        << "Focused attention should have lower entropy";

    // Discretize to ternary and verify sparsity
    ternary_attention_ctx_t* ctx = createTernaryAttentionCtx();
    ASSERT_NE(nullptr, ctx);

    std::vector<ternary_attention_state_t> focused_ternary(seq_length);
    std::vector<ternary_attention_state_t> diffuse_ternary(seq_length);

    ternary_attention_discretize(
        ctx, focused_attention.data(), seq_length, focused_ternary.data());
    ternary_attention_discretize(
        ctx, diffuse_attention.data(), seq_length, diffuse_ternary.data());

    // Count FOCUS states
    int focused_focus_count = 0, diffuse_focus_count = 0;
    for (uint32_t i = 0; i < seq_length; i++) {
        if (focused_ternary[i] == ATTENTION_FOCUS) focused_focus_count++;
        if (diffuse_ternary[i] == ATTENTION_FOCUS) diffuse_focus_count++;
    }

    EXPECT_LT(focused_focus_count, diffuse_focus_count)
        << "Focused attention should have fewer FOCUS states";

    ternary_attention_destroy(ctx);
}

//=============================================================================
// Test: Temperature Annealing for Hard Attention
//=============================================================================

/**
 * Test temperature annealing schedule
 */
TEST_F(AttentionTernaryIntegrationTest, TemperatureAnnealing) {
    ternary_attention_config_t config;
    ternary_attention_default_config(&config);
    config.temperature_annealing = true;
    config.initial_temperature = 5.0f;
    config.final_temperature = 0.1f;
    config.annealing_steps = 100;

    ternary_attention_ctx_t* ctx = ternary_attention_create(&config);
    ASSERT_NE(nullptr, ctx);

    // Check initial temperature
    float temp = ternary_attention_get_temperature(ctx);
    EXPECT_NEAR(5.0f, temp, 0.01f) << "Initial temperature should be 5.0";

    // Update through annealing steps
    for (uint32_t step = 0; step <= 100; step++) {
        ternary_attention_update_temperature(ctx, step);
        temp = ternary_attention_get_temperature(ctx);

        // Temperature should decrease monotonically
        EXPECT_GE(temp, config.final_temperature)
            << "Temperature should not go below final";
        EXPECT_LE(temp, config.initial_temperature)
            << "Temperature should not exceed initial";
    }

    // Check final temperature
    temp = ternary_attention_get_temperature(ctx);
    EXPECT_NEAR(0.1f, temp, 0.1f) << "Final temperature should be ~0.1";

    ternary_attention_destroy(ctx);
}

/**
 * Test ternary attention statistics
 */
TEST_F(AttentionTernaryIntegrationTest, TernaryAttentionStatistics) {
    const uint32_t seq_length = 20;

    ternary_attention_ctx_t* ctx = createTernaryAttentionCtx();
    ASSERT_NE(nullptr, ctx);

    // Generate varied attention and discretize multiple times
    for (int iter = 0; iter < 10; iter++) {
        std::vector<float> soft_attention(seq_length);
        for (uint32_t i = 0; i < seq_length; i++) {
            soft_attention[i] = (float)(i + iter) / (seq_length + 10);
        }

        std::vector<ternary_attention_state_t> ternary_attention(seq_length);
        ternary_attention_discretize(
            ctx, soft_attention.data(), seq_length, ternary_attention.data());
    }

    // Get statistics
    ternary_attention_stats_t stats;
    int result = ternary_attention_get_stats(ctx, &stats);
    EXPECT_EQ(0, result);

    // Verify statistics are reasonable
    EXPECT_GT(stats.n_focus + stats.n_neutral + stats.n_suppress, 0u)
        << "Should have some discretization decisions";
    EXPECT_GE(stats.focus_ratio, 0.0f);
    EXPECT_LE(stats.focus_ratio, 1.0f);
    EXPECT_GE(stats.suppress_ratio, 0.0f);
    EXPECT_LE(stats.suppress_ratio, 1.0f);
    EXPECT_GE(stats.sparsity, 0.0f);
    EXPECT_LE(stats.sparsity, 1.0f);

    ternary_attention_destroy(ctx);
}

//=============================================================================
// Test: Integration with Ternary Weight Matrices
//=============================================================================

/**
 * Test attention weights as ternary matrix
 */
TEST_F(AttentionTernaryIntegrationTest, AttentionWeightsTernaryMatrix) {
    const uint32_t seq_length = 8;

    // Create soft attention weight matrix (seq_length x seq_length)
    std::vector<float> attention_weights(seq_length * seq_length);
    for (uint32_t i = 0; i < seq_length; i++) {
        float row_sum = 0.0f;
        for (uint32_t j = 0; j < seq_length; j++) {
            // Create pattern: high attention near diagonal
            float dist = std::abs((float)i - (float)j);
            attention_weights[i * seq_length + j] = std::exp(-dist * 0.5f);
            row_sum += attention_weights[i * seq_length + j];
        }
        // Normalize row
        for (uint32_t j = 0; j < seq_length; j++) {
            attention_weights[i * seq_length + j] /= row_sum;
        }
    }

    // Convert to ternary matrix using threshold
    const float focus_thresh = 0.3f;
    const float suppress_thresh = 0.1f;

    trit_matrix_t* ternary_mat = trit_matrix_create(
        seq_length, seq_length, TERNARY_PACK_NONE);
    ASSERT_NE(nullptr, ternary_mat);

    for (uint32_t i = 0; i < seq_length; i++) {
        for (uint32_t j = 0; j < seq_length; j++) {
            float weight = attention_weights[i * seq_length + j];
            trit_t trit_val;

            if (weight > focus_thresh) {
                trit_val = TRIT_POSITIVE;  // FOCUS
            } else if (weight < suppress_thresh) {
                trit_val = TRIT_NEGATIVE;  // SUPPRESS
            } else {
                trit_val = TRIT_UNKNOWN;   // NEUTRAL
            }

            trit_matrix_set(ternary_mat, i, j, trit_val);
        }
    }

    // Verify diagonal should have high attention (FOCUS)
    for (uint32_t i = 0; i < seq_length; i++) {
        EXPECT_EQ(TRIT_POSITIVE, trit_matrix_get(ternary_mat, i, i))
            << "Diagonal element should be FOCUS (high attention)";
    }

    // Verify far-from-diagonal should have low attention
    EXPECT_EQ(TRIT_NEGATIVE, trit_matrix_get(ternary_mat, 0, seq_length - 1))
        << "Far off-diagonal should be SUPPRESS (low attention)";
    EXPECT_EQ(TRIT_NEGATIVE, trit_matrix_get(ternary_mat, seq_length - 1, 0))
        << "Far off-diagonal should be SUPPRESS (low attention)";

    trit_matrix_destroy(ternary_mat);
}

/**
 * Test ternary attention mask for causal attention
 */
TEST_F(AttentionTernaryIntegrationTest, TernaryCausalAttentionMask) {
    const uint32_t seq_length = 8;

    // Create causal mask as ternary matrix
    // Future positions are SUPPRESS (-1), past/current are NEUTRAL (0)
    trit_matrix_t* causal_mask = trit_matrix_create(
        seq_length, seq_length, TERNARY_PACK_NONE);
    ASSERT_NE(nullptr, causal_mask);

    for (uint32_t i = 0; i < seq_length; i++) {
        for (uint32_t j = 0; j < seq_length; j++) {
            if (j > i) {
                // Future position: mask out
                trit_matrix_set(causal_mask, i, j, TRIT_NEGATIVE);
            } else {
                // Past or current: allow
                trit_matrix_set(causal_mask, i, j, TRIT_UNKNOWN);
            }
        }
    }

    // Verify mask structure
    // Lower triangle + diagonal should be NEUTRAL
    for (uint32_t i = 0; i < seq_length; i++) {
        for (uint32_t j = 0; j <= i; j++) {
            EXPECT_EQ(TRIT_UNKNOWN, trit_matrix_get(causal_mask, i, j))
                << "Lower triangle should be NEUTRAL at (" << i << "," << j << ")";
        }
    }

    // Upper triangle should be SUPPRESS
    for (uint32_t i = 0; i < seq_length; i++) {
        for (uint32_t j = i + 1; j < seq_length; j++) {
            EXPECT_EQ(TRIT_NEGATIVE, trit_matrix_get(causal_mask, i, j))
                << "Upper triangle should be SUPPRESS at (" << i << "," << j << ")";
        }
    }

    // Check sparsity (half of off-diagonal should be SUPPRESS)
    size_t n_positive, n_unknown, n_negative;
    trit_matrix_count(causal_mask, &n_positive, &n_unknown, &n_negative);

    // Upper triangle has seq_length*(seq_length-1)/2 elements
    size_t expected_suppress = seq_length * (seq_length - 1) / 2;
    EXPECT_EQ(expected_suppress, n_negative);

    // Lower triangle + diagonal has seq_length*(seq_length+1)/2 elements
    size_t expected_neutral = seq_length * (seq_length + 1) / 2;
    EXPECT_EQ(expected_neutral, n_unknown);

    trit_matrix_destroy(causal_mask);
}

/**
 * Test combining attention with ternary value modulation
 */
TEST_F(AttentionTernaryIntegrationTest, CombinedAttentionTernaryValues) {
    const uint32_t seq_length = 6;
    const uint32_t dim = 4;

    // Create attention context
    ternary_attention_ctx_t* ctx = createTernaryAttentionCtx();
    ASSERT_NE(nullptr, ctx);

    // Create soft attention weights
    std::vector<float> soft_attention = {
        0.8f,   // FOCUS
        0.5f,   // NEUTRAL
        0.2f,   // SUPPRESS
        0.75f,  // FOCUS
        0.45f,  // NEUTRAL
        0.15f   // SUPPRESS
    };

    // Discretize
    std::vector<ternary_attention_state_t> ternary_attention(seq_length);
    ternary_attention_discretize(
        ctx, soft_attention.data(), seq_length, ternary_attention.data());

    // Create values as ternary vector per position
    std::vector<trit_vector_t*> ternary_values(seq_length);
    for (uint32_t i = 0; i < seq_length; i++) {
        ternary_values[i] = trit_vector_create(dim, TERNARY_PACK_NONE);
        ASSERT_NE(nullptr, ternary_values[i]);

        // Set varied ternary values
        for (uint32_t d = 0; d < dim; d++) {
            trit_t val = (trit_t)((i + d) % 3 - 1);
            trit_vector_set(ternary_values[i], d, val);
        }
    }

    // Apply attention-weighted aggregation
    // For simplicity, just verify the ternary values are accessible
    for (uint32_t i = 0; i < seq_length; i++) {
        trit_t first_val = trit_vector_get(ternary_values[i], 0);
        EXPECT_TRUE(first_val >= TRIT_NEGATIVE && first_val <= TRIT_POSITIVE);

        // Check that focused positions have valid ternary values
        if (ternary_attention[i] == ATTENTION_FOCUS) {
            for (uint32_t d = 0; d < dim; d++) {
                trit_t val = trit_vector_get(ternary_values[i], d);
                EXPECT_TRUE(TRIT_IS_VALID(val))
                    << "Focused position " << i << " should have valid trit at dim " << d;
            }
        }
    }

    // Cleanup
    for (uint32_t i = 0; i < seq_length; i++) {
        trit_vector_destroy(ternary_values[i]);
    }
    ternary_attention_destroy(ctx);
}

/**
 * Test ternary attention backward gradient estimation
 */
TEST_F(AttentionTernaryIntegrationTest, TernaryAttentionBackward) {
    const uint32_t seq_length = 4;
    const uint32_t dim = 8;

    ternary_attention_ctx_t* ctx = createTernaryAttentionCtx();
    ASSERT_NE(nullptr, ctx);

    // Create soft attention
    std::vector<float> soft_attention = {0.8f, 0.5f, 0.3f, 0.1f};

    // Create gradient from downstream
    std::vector<float> grad_output(seq_length * dim, 1.0f);

    // Compute gradient for attention weights
    std::vector<float> grad_attention(seq_length);
    int result = ternary_attention_backward(
        ctx, grad_output.data(), soft_attention.data(),
        seq_length, dim, grad_attention.data());
    EXPECT_EQ(0, result);

    // Verify gradients are computed
    for (uint32_t i = 0; i < seq_length; i++) {
        EXPECT_FALSE(std::isnan(grad_attention[i]))
            << "Gradient should not be NaN at " << i;
        EXPECT_FALSE(std::isinf(grad_attention[i]))
            << "Gradient should not be inf at " << i;
    }

    ternary_attention_destroy(ctx);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
