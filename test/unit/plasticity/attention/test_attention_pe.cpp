//=============================================================================
// test_attention_pe.cpp - Unit Tests for Attention Positional Encoding
//=============================================================================
/**
 * @file test_attention_pe.cpp
 * @brief Unit tests for positional encoding integration in multihead attention
 *
 * WHAT: Test PE configuration, RoPE application, and ALiBi bias in attention
 * WHY:  Positional encoding is critical for temporal attention mechanisms
 * HOW:  Test RoPE and ALiBi with multihead_attention API
 *
 * API VERIFIED FROM: include/plasticity/attention/nimcp_attention.h
 * - multihead_attention_t, multihead_attention_config_t
 * - multihead_attention_create(), multihead_attention_destroy()
 * - multihead_attention_forward(), multihead_attention_get_stats()
 * - multihead_attention_set_pe_type()
 * - multihead_attention_apply_rope(), multihead_attention_get_alibi_bias()
 *
 * @author NIMCP Development Team
 * @date 2025-12-10
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdlib>
#include <cstring>

// Headers have their own extern "C" guards
    #include "plasticity/attention/nimcp_attention.h"
    #include "utils/encoding/nimcp_positional_encoding.h"
    #include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class AttentionPETest : public ::testing::Test {
protected:
    static constexpr float EPSILON = 1e-4f;
    static constexpr uint32_t TEST_SEQ_LEN = 16;
    static constexpr uint32_t TEST_EMBED_DIM = 64;
    static constexpr uint32_t TEST_NUM_HEADS = 4;

    multihead_attention_t mha = nullptr;

    void SetUp() override {
        srand(42);
        nimcp_memory_init();
    }

    void TearDown() override {
        if (mha) {
            multihead_attention_destroy(mha);
            mha = nullptr;
        }
    }

    bool FloatEqual(float a, float b, float eps = EPSILON) {
        return std::abs(a - b) < eps;
    }

    // Helper to create multihead attention with PE
    multihead_attention_t CreateMHAWithPE(nimcp_pos_encoding_type_t pe_type) {
        multihead_attention_config_t config = {};
        config.num_heads = TEST_NUM_HEADS;
        config.input_dim = TEST_EMBED_DIM;
        config.output_dim = TEST_EMBED_DIM;
        config.sequence_length = TEST_SEQ_LEN;
        config.use_thalamic_gate = false;
        config.use_salience_weighting = false;
        config.gate_bias = 0.5f;
        config.use_positional_encoding = true;
        config.pe_type = pe_type;
        config.rope_base = 10000.0f;
        config.alibi_slope_base = 1.0f;
        config.enable_quantum_attention = false;

        return multihead_attention_create(&config);
    }

    // Helper to create multihead attention without PE
    multihead_attention_t CreateMHAWithoutPE() {
        multihead_attention_config_t config = {};
        config.num_heads = TEST_NUM_HEADS;
        config.input_dim = TEST_EMBED_DIM;
        config.output_dim = TEST_EMBED_DIM;
        config.sequence_length = TEST_SEQ_LEN;
        config.use_thalamic_gate = false;
        config.use_salience_weighting = false;
        config.gate_bias = 0.5f;
        config.use_positional_encoding = false;
        config.enable_quantum_attention = false;

        return multihead_attention_create(&config);
    }
};

//=============================================================================
// Unit Tests: PE Configuration
//=============================================================================

TEST_F(AttentionPETest, CreateWithRoPE) {
    // WHAT: Create multihead attention with RoPE
    // WHY:  Modern transformer position encoding

    mha = CreateMHAWithPE(NIMCP_POS_ROTARY);
    ASSERT_NE(mha, nullptr) << "MHA with RoPE should be created";

    attention_stats_t stats;
    bool result = multihead_attention_get_stats(mha, &stats);
    EXPECT_TRUE(result) << "Should get stats";
}

TEST_F(AttentionPETest, CreateWithALiBi) {
    // WHAT: Create multihead attention with ALiBi
    // WHY:  Efficient attention with linear biases

    mha = CreateMHAWithPE(NIMCP_POS_ALIBI);
    ASSERT_NE(mha, nullptr) << "MHA with ALiBi should be created";

    attention_stats_t stats;
    bool result = multihead_attention_get_stats(mha, &stats);
    EXPECT_TRUE(result) << "Should get stats";
}

TEST_F(AttentionPETest, CreateWithoutPE) {
    // WHAT: Create multihead attention without PE
    // WHY:  Baseline comparison

    mha = CreateMHAWithoutPE();
    ASSERT_NE(mha, nullptr) << "MHA without PE should be created";

    attention_stats_t stats;
    bool result = multihead_attention_get_stats(mha, &stats);
    EXPECT_TRUE(result) << "Should get stats";
}

TEST_F(AttentionPETest, ValidateConfig) {
    // WHAT: Validate attention configuration
    // WHY:  Catch configuration errors early

    multihead_attention_config_t config = {};
    config.num_heads = TEST_NUM_HEADS;
    config.input_dim = TEST_EMBED_DIM;
    config.output_dim = TEST_EMBED_DIM;
    config.sequence_length = TEST_SEQ_LEN;
    config.use_positional_encoding = true;
    config.pe_type = NIMCP_POS_ROTARY;

    bool valid = attention_validate_config(&config);
    EXPECT_TRUE(valid) << "Valid config should pass validation";
}

//=============================================================================
// Unit Tests: RoPE Application
//=============================================================================

TEST_F(AttentionPETest, RoPE_ApplyToQueryKey) {
    // WHAT: Apply RoPE rotation to query/key vectors
    // WHY:  RoPE modifies vectors via rotation

    mha = CreateMHAWithPE(NIMCP_POS_ROTARY);
    ASSERT_NE(mha, nullptr);

    uint32_t seq_len = 8;
    uint32_t key_dim = TEST_EMBED_DIM / TEST_NUM_HEADS;

    // Create test query and key projections
    float* query_proj = (float*)nimcp_malloc(seq_len * key_dim * sizeof(float));
    float* key_proj = (float*)nimcp_malloc(seq_len * key_dim * sizeof(float));
    float* query_out = (float*)nimcp_malloc(seq_len * key_dim * sizeof(float));
    float* key_out = (float*)nimcp_malloc(seq_len * key_dim * sizeof(float));

    ASSERT_NE(query_proj, nullptr);
    ASSERT_NE(key_proj, nullptr);
    ASSERT_NE(query_out, nullptr);
    ASSERT_NE(key_out, nullptr);

    for (uint32_t i = 0; i < seq_len * key_dim; i++) {
        query_proj[i] = (i % 2 == 0) ? 1.0f : 0.0f;
        key_proj[i] = (i % 2 == 0) ? 0.0f : 1.0f;
    }

    bool result = multihead_attention_apply_rope(mha, query_proj, key_proj,
                                                  seq_len, key_dim,
                                                  query_out, key_out);
    EXPECT_TRUE(result) << "RoPE application should succeed";

    // Verify vectors were rotated (changed)
    bool query_changed = false;
    bool key_changed = false;

    for (uint32_t i = 0; i < seq_len * key_dim; i++) {
        if (std::abs(query_out[i] - query_proj[i]) > EPSILON) query_changed = true;
        if (std::abs(key_out[i] - key_proj[i]) > EPSILON) key_changed = true;
    }

    EXPECT_TRUE(query_changed) << "RoPE should modify query";
    EXPECT_TRUE(key_changed) << "RoPE should modify key";

    nimcp_free(query_proj);
    nimcp_free(key_proj);
    nimcp_free(query_out);
    nimcp_free(key_out);
}

//=============================================================================
// Unit Tests: ALiBi Bias
//=============================================================================

TEST_F(AttentionPETest, ALiBi_GetBias) {
    // WHAT: Retrieve ALiBi bias matrix
    // WHY:  ALiBi adds linear bias to attention scores

    mha = CreateMHAWithPE(NIMCP_POS_ALIBI);
    ASSERT_NE(mha, nullptr);

    uint32_t seq_len = 8;
    float* bias = (float*)nimcp_malloc(TEST_NUM_HEADS * seq_len * seq_len * sizeof(float));
    ASSERT_NE(bias, nullptr);

    bool result = multihead_attention_get_alibi_bias(mha, seq_len, bias);
    EXPECT_TRUE(result) << "ALiBi bias retrieval should succeed";

    // Verify bias is non-zero
    bool has_nonzero = false;
    for (uint32_t i = 0; i < TEST_NUM_HEADS * seq_len * seq_len; i++) {
        if (std::abs(bias[i]) > EPSILON) {
            has_nonzero = true;
            break;
        }
    }

    EXPECT_TRUE(has_nonzero) << "ALiBi bias should contain non-zero values";

    nimcp_free(bias);
}

TEST_F(AttentionPETest, ALiBi_DistanceDecay) {
    // WHAT: Verify ALiBi bias increases with distance
    // WHY:  ALiBi penalizes distant positions

    mha = CreateMHAWithPE(NIMCP_POS_ALIBI);
    ASSERT_NE(mha, nullptr);

    uint32_t seq_len = 8;
    float* bias = (float*)nimcp_malloc(TEST_NUM_HEADS * seq_len * seq_len * sizeof(float));
    ASSERT_NE(bias, nullptr);

    multihead_attention_get_alibi_bias(mha, seq_len, bias);

    // For first head, check that bias increases with distance
    // bias[h][i][j] = -slope[h] * |i - j|
    uint32_t head = 0;
    uint32_t i = 3;  // Position i

    // Get bias at different distances
    float bias_dist_0 = bias[head * seq_len * seq_len + i * seq_len + i];      // Same position
    float bias_dist_1 = bias[head * seq_len * seq_len + i * seq_len + (i+1)];  // Distance 1
    float bias_dist_2 = bias[head * seq_len * seq_len + i * seq_len + (i+2)];  // Distance 2

    // Bias should become more negative as distance increases
    EXPECT_NEAR(bias_dist_0, 0.0f, EPSILON) << "Self-attention bias should be zero";
    EXPECT_LT(bias_dist_1, bias_dist_0) << "Distance 1 bias should be more negative";
    EXPECT_LT(bias_dist_2, bias_dist_1) << "Distance 2 bias should be more negative";

    nimcp_free(bias);
}

//=============================================================================
// Unit Tests: PE Type Switching
//=============================================================================

TEST_F(AttentionPETest, SwitchPEType_RoPEToALiBi) {
    // WHAT: Switch from RoPE to ALiBi at runtime
    // WHY:  Allow dynamic PE reconfiguration

    mha = CreateMHAWithPE(NIMCP_POS_ROTARY);
    ASSERT_NE(mha, nullptr);

    // Switch to ALiBi
    bool result = multihead_attention_set_pe_type(mha, NIMCP_POS_ALIBI);
    EXPECT_TRUE(result) << "PE type switch should succeed";

    // Verify ALiBi is now active
    uint32_t seq_len = 8;
    float* bias = (float*)nimcp_malloc(TEST_NUM_HEADS * seq_len * seq_len * sizeof(float));
    ASSERT_NE(bias, nullptr);

    result = multihead_attention_get_alibi_bias(mha, seq_len, bias);
    EXPECT_TRUE(result) << "ALiBi should work after switch";

    nimcp_free(bias);
}

TEST_F(AttentionPETest, SwitchPEType_ALiBiToRoPE) {
    // WHAT: Switch from ALiBi to RoPE
    // WHY:  Test different PE type transitions

    mha = CreateMHAWithPE(NIMCP_POS_ALIBI);
    ASSERT_NE(mha, nullptr);

    // Switch to RoPE
    bool result = multihead_attention_set_pe_type(mha, NIMCP_POS_ROTARY);
    EXPECT_TRUE(result) << "PE type switch should succeed";

    // Verify RoPE is now active
    uint32_t seq_len = 4;
    uint32_t key_dim = TEST_EMBED_DIM / TEST_NUM_HEADS;

    float* query_proj = (float*)nimcp_malloc(seq_len * key_dim * sizeof(float));
    float* key_proj = (float*)nimcp_malloc(seq_len * key_dim * sizeof(float));
    float* query_out = (float*)nimcp_malloc(seq_len * key_dim * sizeof(float));
    float* key_out = (float*)nimcp_malloc(seq_len * key_dim * sizeof(float));

    for (uint32_t i = 0; i < seq_len * key_dim; i++) {
        query_proj[i] = key_proj[i] = 1.0f;
    }

    result = multihead_attention_apply_rope(mha, query_proj, key_proj,
                                             seq_len, key_dim,
                                             query_out, key_out);
    EXPECT_TRUE(result) << "RoPE should work after switch";

    nimcp_free(query_proj);
    nimcp_free(key_proj);
    nimcp_free(query_out);
    nimcp_free(key_out);
}

//=============================================================================
// Unit Tests: Forward Pass with PE
//=============================================================================

TEST_F(AttentionPETest, Forward_WithRoPE) {
    // WHAT: Run forward pass with RoPE encoding
    // WHY:  End-to-end integration test

    mha = CreateMHAWithPE(NIMCP_POS_ROTARY);
    ASSERT_NE(mha, nullptr);

    uint32_t seq_len = 8;

    float* input = (float*)nimcp_malloc(seq_len * TEST_EMBED_DIM * sizeof(float));
    float* output = (float*)nimcp_malloc(seq_len * TEST_EMBED_DIM * sizeof(float));

    ASSERT_NE(input, nullptr);
    ASSERT_NE(output, nullptr);

    for (uint32_t i = 0; i < seq_len * TEST_EMBED_DIM; i++) {
        input[i] = (float)(rand() % 100) / 100.0f;
    }

    bool result = multihead_attention_forward(mha, input, seq_len, nullptr, output);
    EXPECT_TRUE(result) << "Forward with RoPE should succeed";

    // Verify output is non-zero
    bool has_output = false;
    for (uint32_t i = 0; i < seq_len * TEST_EMBED_DIM; i++) {
        if (std::abs(output[i]) > EPSILON) {
            has_output = true;
            break;
        }
    }
    EXPECT_TRUE(has_output) << "Should produce non-zero output";

    nimcp_free(input);
    nimcp_free(output);
}

TEST_F(AttentionPETest, Forward_WithALiBi) {
    // WHAT: Run forward pass with ALiBi encoding
    // WHY:  End-to-end integration test

    mha = CreateMHAWithPE(NIMCP_POS_ALIBI);
    ASSERT_NE(mha, nullptr);

    uint32_t seq_len = 8;

    float* input = (float*)nimcp_malloc(seq_len * TEST_EMBED_DIM * sizeof(float));
    float* output = (float*)nimcp_malloc(seq_len * TEST_EMBED_DIM * sizeof(float));

    ASSERT_NE(input, nullptr);
    ASSERT_NE(output, nullptr);

    for (uint32_t i = 0; i < seq_len * TEST_EMBED_DIM; i++) {
        input[i] = (float)(rand() % 100) / 100.0f;
    }

    bool result = multihead_attention_forward(mha, input, seq_len, nullptr, output);
    EXPECT_TRUE(result) << "Forward with ALiBi should succeed";

    // Verify output is non-zero
    bool has_output = false;
    for (uint32_t i = 0; i < seq_len * TEST_EMBED_DIM; i++) {
        if (std::abs(output[i]) > EPSILON) {
            has_output = true;
            break;
        }
    }
    EXPECT_TRUE(has_output) << "Should produce non-zero output";

    nimcp_free(input);
    nimcp_free(output);
}

//=============================================================================
// Unit Tests: Edge Cases
//=============================================================================

TEST_F(AttentionPETest, EdgeCase_NullMHA) {
    // WHAT: Handle NULL MHA gracefully
    // WHY:  Robustness testing

    attention_stats_t stats;
    bool result = multihead_attention_get_stats(nullptr, &stats);
    EXPECT_FALSE(result) << "NULL mha should fail";
}

TEST_F(AttentionPETest, EdgeCase_NullOutput) {
    // WHAT: Handle NULL output buffer
    // WHY:  Robustness testing

    mha = CreateMHAWithPE(NIMCP_POS_ROTARY);
    ASSERT_NE(mha, nullptr);

    uint32_t seq_len = 8;
    float* input = (float*)nimcp_malloc(seq_len * TEST_EMBED_DIM * sizeof(float));
    ASSERT_NE(input, nullptr);

    bool result = multihead_attention_forward(mha, input, seq_len, nullptr, nullptr);
    EXPECT_FALSE(result) << "NULL output should fail";

    nimcp_free(input);
}

TEST_F(AttentionPETest, DestroyNull) {
    // WHAT: Destroy NULL mha
    // WHY:  Should not crash

    multihead_attention_destroy(nullptr);
    SUCCEED() << "Destroying NULL should not crash";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
