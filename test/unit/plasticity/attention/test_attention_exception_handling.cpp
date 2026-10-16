/**
 * @file test_attention_exception_handling.cpp
 * @brief Unit tests for multi-attention module exception handling
 *
 * WHAT: Comprehensive tests for all NIMCP_THROW_TO_IMMUNE exception paths
 * WHY:  Verify robust error handling and immune system integration
 * HOW:  Test NULL parameters, invalid configs, memory failures, and edge cases
 *
 * EXCEPTION PATHS TESTED:
 * - attention_head_create: NULL config, invalid dimensions, memory failures
 * - multihead_attention_create: invalid config, memory failures
 * - ternary_attention_*: NULL params, invalid configs
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>

extern "C" {
#include "plasticity/attention/nimcp_attention.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_immune.h"
}

//=============================================================================
// Test Fixtures
//=============================================================================

/**
 * @brief Test fixture for attention head exception tests
 */
class AttentionHeadExceptionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Valid default config
        config.input_dim = 64;
        config.output_dim = 64;
        config.key_dim = 64;
        config.value_dim = 64;
        config.temperature = 1.0f;
        config.dropout_rate = 0.0f;
    }

    void TearDown() override {
        if (head) {
            attention_head_destroy(head);
            head = nullptr;
        }
    }

    attention_head_config_t config;
    attention_head_t head = nullptr;
};

/**
 * @brief Test fixture for multihead attention exception tests
 */
class MultiheadAttentionExceptionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Valid default config
        config.num_heads = 4;
        config.input_dim = 128;
        config.output_dim = 128;
        config.sequence_length = 16;
        config.use_thalamic_gate = true;
        config.use_salience_weighting = false;
        config.gate_bias = 0.5f;
        config.use_positional_encoding = false;
        config.pe_type = NIMCP_POS_ROTARY;
        config.rope_base = 10000.0f;
        config.alibi_slope_base = 2.0f;
        config.enable_quantum_attention = false;
    }

    void TearDown() override {
        if (mha) {
            multihead_attention_destroy(mha);
            mha = nullptr;
        }
    }

    multihead_attention_config_t config;
    multihead_attention_t mha = nullptr;
};

/**
 * @brief Test fixture for ternary attention exception tests
 */
class TernaryAttentionExceptionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize with default config
        ternary_attention_default_config(&config);
    }

    void TearDown() override {
        if (ctx) {
            ternary_attention_destroy(ctx);
            ctx = nullptr;
        }
    }

    ternary_attention_config_t config;
    ternary_attention_ctx_t* ctx = nullptr;
};

//=============================================================================
// Attention Head Exception Tests
//=============================================================================

TEST_F(AttentionHeadExceptionTest, CreateWithNullConfig) {
    /**
     * WHAT: Test attention_head_create with NULL config
     * WHY:  Verify NIMCP_THROW_TO_IMMUNE for NIMCP_ERROR_NULL_POINTER
     */
    head = attention_head_create(nullptr);
    EXPECT_EQ(head, nullptr) << "Should return NULL for NULL config";
}

TEST_F(AttentionHeadExceptionTest, CreateWithZeroInputDim) {
    /**
     * WHAT: Test attention_head_create with input_dim = 0
     * WHY:  Verify NIMCP_THROW_TO_IMMUNE for NIMCP_ERROR_INVALID_PARAM
     */
    config.input_dim = 0;
    head = attention_head_create(&config);
    EXPECT_EQ(head, nullptr) << "Should return NULL for zero input_dim";
}

TEST_F(AttentionHeadExceptionTest, CreateWithZeroOutputDim) {
    /**
     * WHAT: Test attention_head_create with output_dim = 0
     * WHY:  Verify NIMCP_THROW_TO_IMMUNE for NIMCP_ERROR_INVALID_PARAM
     */
    config.output_dim = 0;
    head = attention_head_create(&config);
    EXPECT_EQ(head, nullptr) << "Should return NULL for zero output_dim";
}

TEST_F(AttentionHeadExceptionTest, DestroyNullHead) {
    /**
     * WHAT: Test attention_head_destroy with NULL head
     * WHY:  Verify graceful handling without crash
     */
    attention_head_destroy(nullptr);
    // No crash = success
    SUCCEED();
}

TEST_F(AttentionHeadExceptionTest, ForwardWithNullHead) {
    /**
     * WHAT: Test attention_head_forward with NULL head
     * WHY:  Verify returns false without crash
     */
    const uint32_t seq_len = 4;
    std::vector<float> query(seq_len * config.input_dim, 0.5f);
    std::vector<float> key(seq_len * config.input_dim, 0.5f);
    std::vector<float> value(seq_len * config.input_dim, 1.0f);
    std::vector<float> output(seq_len * config.output_dim, 0.0f);

    bool result = attention_head_forward(nullptr, query.data(), key.data(),
                                         value.data(), seq_len, output.data(),
                                         nullptr, nullptr, 0);
    EXPECT_FALSE(result) << "Should return false for NULL head";
}

TEST_F(AttentionHeadExceptionTest, ForwardWithNullQuery) {
    /**
     * WHAT: Test attention_head_forward with NULL query
     * WHY:  Verify returns false without crash
     */
    head = attention_head_create(&config);
    ASSERT_NE(head, nullptr);

    const uint32_t seq_len = 4;
    std::vector<float> key(seq_len * config.input_dim, 0.5f);
    std::vector<float> value(seq_len * config.input_dim, 1.0f);
    std::vector<float> output(seq_len * config.output_dim, 0.0f);

    bool result = attention_head_forward(head, nullptr, key.data(),
                                         value.data(), seq_len, output.data(),
                                         nullptr, nullptr, 0);
    EXPECT_FALSE(result) << "Should return false for NULL query";
}

TEST_F(AttentionHeadExceptionTest, ForwardWithNullKey) {
    /**
     * WHAT: Test attention_head_forward with NULL key
     * WHY:  Verify returns false without crash
     */
    head = attention_head_create(&config);
    ASSERT_NE(head, nullptr);

    const uint32_t seq_len = 4;
    std::vector<float> query(seq_len * config.input_dim, 0.5f);
    std::vector<float> value(seq_len * config.input_dim, 1.0f);
    std::vector<float> output(seq_len * config.output_dim, 0.0f);

    bool result = attention_head_forward(head, query.data(), nullptr,
                                         value.data(), seq_len, output.data(),
                                         nullptr, nullptr, 0);
    EXPECT_FALSE(result) << "Should return false for NULL key";
}

TEST_F(AttentionHeadExceptionTest, ForwardWithNullValue) {
    /**
     * WHAT: Test attention_head_forward with NULL value
     * WHY:  Verify returns false without crash
     */
    head = attention_head_create(&config);
    ASSERT_NE(head, nullptr);

    const uint32_t seq_len = 4;
    std::vector<float> query(seq_len * config.input_dim, 0.5f);
    std::vector<float> key(seq_len * config.input_dim, 0.5f);
    std::vector<float> output(seq_len * config.output_dim, 0.0f);

    bool result = attention_head_forward(head, query.data(), key.data(),
                                         nullptr, seq_len, output.data(),
                                         nullptr, nullptr, 0);
    EXPECT_FALSE(result) << "Should return false for NULL value";
}

TEST_F(AttentionHeadExceptionTest, ForwardWithNullOutput) {
    /**
     * WHAT: Test attention_head_forward with NULL output
     * WHY:  Verify returns false without crash
     */
    head = attention_head_create(&config);
    ASSERT_NE(head, nullptr);

    const uint32_t seq_len = 4;
    std::vector<float> query(seq_len * config.input_dim, 0.5f);
    std::vector<float> key(seq_len * config.input_dim, 0.5f);
    std::vector<float> value(seq_len * config.input_dim, 1.0f);

    bool result = attention_head_forward(head, query.data(), key.data(),
                                         value.data(), seq_len, nullptr,
                                         nullptr, nullptr, 0);
    EXPECT_FALSE(result) << "Should return false for NULL output";
}

TEST_F(AttentionHeadExceptionTest, ForwardWithZeroSequenceLength) {
    /**
     * WHAT: Test attention_head_forward with seq_len = 0
     * WHY:  Verify returns false for invalid sequence length
     */
    head = attention_head_create(&config);
    ASSERT_NE(head, nullptr);

    std::vector<float> query(config.input_dim, 0.5f);
    std::vector<float> key(config.input_dim, 0.5f);
    std::vector<float> value(config.input_dim, 1.0f);
    std::vector<float> output(config.output_dim, 0.0f);

    bool result = attention_head_forward(head, query.data(), key.data(),
                                         value.data(), 0, output.data(),
                                         nullptr, nullptr, 0);
    EXPECT_FALSE(result) << "Should return false for zero sequence length";
}

//=============================================================================
// Multihead Attention Exception Tests
//=============================================================================

TEST_F(MultiheadAttentionExceptionTest, CreateWithNullConfig) {
    /**
     * WHAT: Test multihead_attention_create with NULL config
     * WHY:  Verify NIMCP_THROW_TO_IMMUNE for invalid config
     */
    mha = multihead_attention_create(nullptr);
    EXPECT_EQ(mha, nullptr) << "Should return NULL for NULL config";
}

TEST_F(MultiheadAttentionExceptionTest, CreateWithZeroNumHeads) {
    /**
     * WHAT: Test multihead_attention_create with num_heads = 0
     * WHY:  Verify NIMCP_THROW_TO_IMMUNE for NIMCP_ERROR_INVALID_PARAM
     */
    config.num_heads = 0;
    mha = multihead_attention_create(&config);
    EXPECT_EQ(mha, nullptr) << "Should return NULL for zero num_heads";
}

TEST_F(MultiheadAttentionExceptionTest, CreateWithZeroInputDim) {
    /**
     * WHAT: Test multihead_attention_create with input_dim = 0
     * WHY:  Verify NIMCP_THROW_TO_IMMUNE for NIMCP_ERROR_INVALID_PARAM
     */
    config.input_dim = 0;
    mha = multihead_attention_create(&config);
    EXPECT_EQ(mha, nullptr) << "Should return NULL for zero input_dim";
}

TEST_F(MultiheadAttentionExceptionTest, CreateWithZeroOutputDim) {
    /**
     * WHAT: Test multihead_attention_create with output_dim = 0
     * WHY:  Verify NIMCP_THROW_TO_IMMUNE for NIMCP_ERROR_INVALID_PARAM
     */
    config.output_dim = 0;
    mha = multihead_attention_create(&config);
    EXPECT_EQ(mha, nullptr) << "Should return NULL for zero output_dim";
}

TEST_F(MultiheadAttentionExceptionTest, CreateWithZeroSequenceLength) {
    /**
     * WHAT: Test multihead_attention_create with sequence_length = 0
     * WHY:  Verify NIMCP_THROW_TO_IMMUNE for NIMCP_ERROR_INVALID_PARAM
     */
    config.sequence_length = 0;
    mha = multihead_attention_create(&config);
    EXPECT_EQ(mha, nullptr) << "Should return NULL for zero sequence_length";
}

TEST_F(MultiheadAttentionExceptionTest, CreateWithNonDivisibleDimensions) {
    /**
     * WHAT: Test multihead_attention_create with input_dim not divisible by num_heads
     * WHY:  Verify NIMCP_THROW_TO_IMMUNE for NIMCP_ERROR_INVALID_PARAM
     */
    config.num_heads = 4;
    config.input_dim = 127;  // Not divisible by 4
    mha = multihead_attention_create(&config);
    EXPECT_EQ(mha, nullptr) << "Should return NULL when input_dim not divisible by num_heads";
}

TEST_F(MultiheadAttentionExceptionTest, DestroyNullMha) {
    /**
     * WHAT: Test multihead_attention_destroy with NULL mha
     * WHY:  Verify graceful handling without crash
     */
    multihead_attention_destroy(nullptr);
    // No crash = success
    SUCCEED();
}

TEST_F(MultiheadAttentionExceptionTest, ForwardWithNullMha) {
    /**
     * WHAT: Test multihead_attention_forward with NULL mha
     * WHY:  Verify returns false without crash
     */
    const uint32_t seq_len = 8;
    std::vector<float> input(seq_len * config.input_dim, 0.5f);
    std::vector<float> output(seq_len * config.output_dim, 0.0f);

    bool result = multihead_attention_forward(nullptr, input.data(), seq_len,
                                              nullptr, output.data());
    EXPECT_FALSE(result) << "Should return false for NULL mha";
}

TEST_F(MultiheadAttentionExceptionTest, ForwardWithNullInput) {
    /**
     * WHAT: Test multihead_attention_forward with NULL input
     * WHY:  Verify returns false without crash
     */
    mha = multihead_attention_create(&config);
    ASSERT_NE(mha, nullptr);

    const uint32_t seq_len = 8;
    std::vector<float> output(seq_len * config.output_dim, 0.0f);

    bool result = multihead_attention_forward(mha, nullptr, seq_len,
                                              nullptr, output.data());
    EXPECT_FALSE(result) << "Should return false for NULL input";
}

TEST_F(MultiheadAttentionExceptionTest, ForwardWithNullOutput) {
    /**
     * WHAT: Test multihead_attention_forward with NULL output
     * WHY:  Verify returns false without crash
     */
    mha = multihead_attention_create(&config);
    ASSERT_NE(mha, nullptr);

    const uint32_t seq_len = 8;
    std::vector<float> input(seq_len * config.input_dim, 0.5f);

    bool result = multihead_attention_forward(mha, input.data(), seq_len,
                                              nullptr, nullptr);
    EXPECT_FALSE(result) << "Should return false for NULL output";
}

TEST_F(MultiheadAttentionExceptionTest, ForwardWithZeroSequenceLength) {
    /**
     * WHAT: Test multihead_attention_forward with seq_len = 0
     * WHY:  Verify returns false for invalid sequence length
     */
    mha = multihead_attention_create(&config);
    ASSERT_NE(mha, nullptr);

    std::vector<float> input(config.input_dim, 0.5f);
    std::vector<float> output(config.output_dim, 0.0f);

    bool result = multihead_attention_forward(mha, input.data(), 0,
                                              nullptr, output.data());
    EXPECT_FALSE(result) << "Should return false for zero sequence length";
}

TEST_F(MultiheadAttentionExceptionTest, ForwardWithExcessiveSequenceLength) {
    /**
     * WHAT: Test multihead_attention_forward with seq_len > max
     * WHY:  Verify returns false when exceeding configured max
     */
    mha = multihead_attention_create(&config);
    ASSERT_NE(mha, nullptr);

    const uint32_t excessive_len = config.sequence_length + 1;
    std::vector<float> input(excessive_len * config.input_dim, 0.5f);
    std::vector<float> output(excessive_len * config.output_dim, 0.0f);

    bool result = multihead_attention_forward(mha, input.data(), excessive_len,
                                              nullptr, output.data());
    EXPECT_FALSE(result) << "Should return false when exceeding max sequence length";
}

TEST_F(MultiheadAttentionExceptionTest, SetGateWithNullMha) {
    /**
     * WHAT: Test multihead_attention_set_gate with NULL mha
     * WHY:  Verify returns false without crash
     */
    bool result = multihead_attention_set_gate(nullptr, 0.5f);
    EXPECT_FALSE(result) << "Should return false for NULL mha";
}

TEST_F(MultiheadAttentionExceptionTest, GetStatsWithNullMha) {
    /**
     * WHAT: Test multihead_attention_get_stats with NULL mha
     * WHY:  Verify returns false without crash
     */
    attention_stats_t stats;
    bool result = multihead_attention_get_stats(nullptr, &stats);
    EXPECT_FALSE(result) << "Should return false for NULL mha";
}

TEST_F(MultiheadAttentionExceptionTest, GetStatsWithNullStats) {
    /**
     * WHAT: Test multihead_attention_get_stats with NULL stats
     * WHY:  Verify returns false without crash
     */
    mha = multihead_attention_create(&config);
    ASSERT_NE(mha, nullptr);

    bool result = multihead_attention_get_stats(mha, nullptr);
    EXPECT_FALSE(result) << "Should return false for NULL stats";
}

TEST_F(MultiheadAttentionExceptionTest, ResetStatsWithNullMha) {
    /**
     * WHAT: Test multihead_attention_reset_stats with NULL mha
     * WHY:  Verify graceful handling without crash
     */
    multihead_attention_reset_stats(nullptr);
    // No crash = success
    SUCCEED();
}

TEST_F(MultiheadAttentionExceptionTest, GetStrengthWithNullMha) {
    /**
     * WHAT: Test multihead_attention_get_strength with NULL mha
     * WHY:  Verify returns 0.0f without crash
     */
    float strength = multihead_attention_get_strength(nullptr);
    EXPECT_FLOAT_EQ(strength, 0.0f) << "Should return 0.0f for NULL mha";
}

TEST_F(MultiheadAttentionExceptionTest, SetPeTypeWithNullMha) {
    /**
     * WHAT: Test multihead_attention_set_pe_type with NULL mha
     * WHY:  Verify returns false without crash
     */
    bool result = multihead_attention_set_pe_type(nullptr, NIMCP_POS_ROTARY);
    EXPECT_FALSE(result) << "Should return false for NULL mha";
}

TEST_F(MultiheadAttentionExceptionTest, SetPeTypeWithUnsupportedType) {
    /**
     * WHAT: Test multihead_attention_set_pe_type with unsupported type
     * WHY:  Verify returns false for invalid PE type
     */
    mha = multihead_attention_create(&config);
    ASSERT_NE(mha, nullptr);

    // Use NIMCP_POS_SINUSOIDAL which is not supported
    bool result = multihead_attention_set_pe_type(mha, NIMCP_POS_SINUSOIDAL);
    EXPECT_FALSE(result) << "Should return false for unsupported PE type";
}

TEST_F(MultiheadAttentionExceptionTest, ApplyRopeWithNullMha) {
    /**
     * WHAT: Test multihead_attention_apply_rope with NULL mha
     * WHY:  Verify returns false without crash
     */
    const uint32_t seq_len = 4;
    const uint32_t key_dim = 32;
    std::vector<float> query_proj(seq_len * key_dim, 0.5f);
    std::vector<float> key_proj(seq_len * key_dim, 0.5f);
    std::vector<float> query_out(seq_len * key_dim, 0.0f);
    std::vector<float> key_out(seq_len * key_dim, 0.0f);

    bool result = multihead_attention_apply_rope(nullptr, query_proj.data(),
                                                  key_proj.data(), seq_len, key_dim,
                                                  query_out.data(), key_out.data());
    EXPECT_FALSE(result) << "Should return false for NULL mha";
}

TEST_F(MultiheadAttentionExceptionTest, ApplyRopeWithNullQueryProj) {
    /**
     * WHAT: Test multihead_attention_apply_rope with NULL query_proj
     * WHY:  Verify returns false without crash
     */
    mha = multihead_attention_create(&config);
    ASSERT_NE(mha, nullptr);

    const uint32_t seq_len = 4;
    const uint32_t key_dim = 32;
    std::vector<float> key_proj(seq_len * key_dim, 0.5f);
    std::vector<float> query_out(seq_len * key_dim, 0.0f);
    std::vector<float> key_out(seq_len * key_dim, 0.0f);

    bool result = multihead_attention_apply_rope(mha, nullptr,
                                                  key_proj.data(), seq_len, key_dim,
                                                  query_out.data(), key_out.data());
    EXPECT_FALSE(result) << "Should return false for NULL query_proj";
}

TEST_F(MultiheadAttentionExceptionTest, ApplyRopeWithNoEncoder) {
    /**
     * WHAT: Test multihead_attention_apply_rope without positional encoder
     * WHY:  Verify returns false when no encoder is configured
     */
    config.use_positional_encoding = false;
    mha = multihead_attention_create(&config);
    ASSERT_NE(mha, nullptr);

    const uint32_t seq_len = 4;
    const uint32_t key_dim = 32;
    std::vector<float> query_proj(seq_len * key_dim, 0.5f);
    std::vector<float> key_proj(seq_len * key_dim, 0.5f);
    std::vector<float> query_out(seq_len * key_dim, 0.0f);
    std::vector<float> key_out(seq_len * key_dim, 0.0f);

    bool result = multihead_attention_apply_rope(mha, query_proj.data(),
                                                  key_proj.data(), seq_len, key_dim,
                                                  query_out.data(), key_out.data());
    EXPECT_FALSE(result) << "Should return false when no encoder configured";
}

TEST_F(MultiheadAttentionExceptionTest, GetAlibiBiasWithNullMha) {
    /**
     * WHAT: Test multihead_attention_get_alibi_bias with NULL mha
     * WHY:  Verify returns false without crash
     */
    const uint32_t seq_len = 4;
    std::vector<float> bias_out(config.num_heads * seq_len * seq_len, 0.0f);

    bool result = multihead_attention_get_alibi_bias(nullptr, seq_len, bias_out.data());
    EXPECT_FALSE(result) << "Should return false for NULL mha";
}

//=============================================================================
// Utility Function Exception Tests
//=============================================================================

TEST_F(MultiheadAttentionExceptionTest, ComputeEntropyWithNullWeights) {
    /**
     * WHAT: Test attention_compute_entropy with NULL weights
     * WHY:  Verify returns 0.0f without crash
     */
    float entropy = attention_compute_entropy(nullptr, 16);
    EXPECT_FLOAT_EQ(entropy, 0.0f) << "Should return 0.0f for NULL weights";
}

TEST_F(MultiheadAttentionExceptionTest, ComputeEntropyWithZeroLength) {
    /**
     * WHAT: Test attention_compute_entropy with seq_len = 0
     * WHY:  Verify returns 0.0f without crash
     */
    std::vector<float> weights(16, 0.0625f);  // Uniform distribution
    float entropy = attention_compute_entropy(weights.data(), 0);
    EXPECT_FLOAT_EQ(entropy, 0.0f) << "Should return 0.0f for zero sequence length";
}

TEST_F(MultiheadAttentionExceptionTest, ValidateConfigWithNullConfig) {
    /**
     * WHAT: Test attention_validate_config with NULL config
     * WHY:  Verify returns false without crash
     */
    bool valid = attention_validate_config(nullptr);
    EXPECT_FALSE(valid) << "Should return false for NULL config";
}

//=============================================================================
// Ternary Attention Exception Tests
//=============================================================================

TEST_F(TernaryAttentionExceptionTest, DefaultConfigWithNullConfig) {
    /**
     * WHAT: Test ternary_attention_default_config with NULL config
     * WHY:  Verify NIMCP_THROW_TO_IMMUNE for NIMCP_ERROR_NULL_POINTER
     */
    int result = ternary_attention_default_config(nullptr);
    EXPECT_EQ(result, -1) << "Should return -1 for NULL config";
}

TEST_F(TernaryAttentionExceptionTest, CreateWithNullConfig) {
    /**
     * WHAT: Test ternary_attention_create with NULL config
     * WHY:  Verify NIMCP_THROW_TO_IMMUNE for NIMCP_ERROR_NULL_POINTER
     */
    ctx = ternary_attention_create(nullptr);
    EXPECT_EQ(ctx, nullptr) << "Should return NULL for NULL config";
}

TEST_F(TernaryAttentionExceptionTest, DestroyNullCtx) {
    /**
     * WHAT: Test ternary_attention_destroy with NULL ctx
     * WHY:  Verify graceful handling without crash
     */
    ternary_attention_destroy(nullptr);
    // No crash = success
    SUCCEED();
}

TEST_F(TernaryAttentionExceptionTest, DiscretizeWithNullCtx) {
    /**
     * WHAT: Test ternary_attention_discretize with NULL ctx
     * WHY:  Verify returns -1 without crash
     */
    const uint32_t seq_len = 8;
    std::vector<float> soft_attention(seq_len, 0.5f);
    std::vector<ternary_attention_state_t> ternary_out(seq_len);

    int result = ternary_attention_discretize(nullptr, soft_attention.data(),
                                              seq_len, ternary_out.data());
    EXPECT_EQ(result, -1) << "Should return -1 for NULL ctx";
}

TEST_F(TernaryAttentionExceptionTest, DiscretizeWithNullSoftAttention) {
    /**
     * WHAT: Test ternary_attention_discretize with NULL soft_attention
     * WHY:  Verify returns -1 without crash
     */
    ctx = ternary_attention_create(&config);
    ASSERT_NE(ctx, nullptr);

    const uint32_t seq_len = 8;
    std::vector<ternary_attention_state_t> ternary_out(seq_len);

    int result = ternary_attention_discretize(ctx, nullptr, seq_len, ternary_out.data());
    EXPECT_EQ(result, -1) << "Should return -1 for NULL soft_attention";
}

TEST_F(TernaryAttentionExceptionTest, DiscretizeWithNullOutput) {
    /**
     * WHAT: Test ternary_attention_discretize with NULL ternary_out
     * WHY:  Verify returns -1 without crash
     */
    ctx = ternary_attention_create(&config);
    ASSERT_NE(ctx, nullptr);

    const uint32_t seq_len = 8;
    std::vector<float> soft_attention(seq_len, 0.5f);

    int result = ternary_attention_discretize(ctx, soft_attention.data(), seq_len, nullptr);
    EXPECT_EQ(result, -1) << "Should return -1 for NULL ternary_out";
}

TEST_F(TernaryAttentionExceptionTest, DiscretizeWithZeroSequenceLength) {
    /**
     * WHAT: Test ternary_attention_discretize with seq_len = 0
     * WHY:  Verify returns -1 for invalid sequence length
     */
    ctx = ternary_attention_create(&config);
    ASSERT_NE(ctx, nullptr);

    std::vector<float> soft_attention(1, 0.5f);
    std::vector<ternary_attention_state_t> ternary_out(1);

    int result = ternary_attention_discretize(ctx, soft_attention.data(), 0, ternary_out.data());
    EXPECT_EQ(result, -1) << "Should return -1 for zero sequence length";
}

TEST_F(TernaryAttentionExceptionTest, ApplyWithNullCtx) {
    /**
     * WHAT: Test ternary_attention_apply with NULL ctx
     * WHY:  Verify returns -1 without crash
     */
    const uint32_t seq_len = 8;
    const uint32_t dim = 64;
    std::vector<float> values(seq_len * dim, 1.0f);
    std::vector<ternary_attention_state_t> ternary_attention(seq_len, ATTENTION_NEUTRAL);
    std::vector<float> output(seq_len * dim, 0.0f);

    int result = ternary_attention_apply(nullptr, values.data(), ternary_attention.data(),
                                         seq_len, dim, output.data());
    EXPECT_EQ(result, -1) << "Should return -1 for NULL ctx";
}

TEST_F(TernaryAttentionExceptionTest, ApplyWithNullValues) {
    /**
     * WHAT: Test ternary_attention_apply with NULL values
     * WHY:  Verify returns -1 without crash
     */
    ctx = ternary_attention_create(&config);
    ASSERT_NE(ctx, nullptr);

    const uint32_t seq_len = 8;
    const uint32_t dim = 64;
    std::vector<ternary_attention_state_t> ternary_attention(seq_len, ATTENTION_NEUTRAL);
    std::vector<float> output(seq_len * dim, 0.0f);

    int result = ternary_attention_apply(ctx, nullptr, ternary_attention.data(),
                                         seq_len, dim, output.data());
    EXPECT_EQ(result, -1) << "Should return -1 for NULL values";
}

TEST_F(TernaryAttentionExceptionTest, ApplyWithNullTernaryAttention) {
    /**
     * WHAT: Test ternary_attention_apply with NULL ternary_attention
     * WHY:  Verify returns -1 without crash
     */
    ctx = ternary_attention_create(&config);
    ASSERT_NE(ctx, nullptr);

    const uint32_t seq_len = 8;
    const uint32_t dim = 64;
    std::vector<float> values(seq_len * dim, 1.0f);
    std::vector<float> output(seq_len * dim, 0.0f);

    int result = ternary_attention_apply(ctx, values.data(), nullptr,
                                         seq_len, dim, output.data());
    EXPECT_EQ(result, -1) << "Should return -1 for NULL ternary_attention";
}

TEST_F(TernaryAttentionExceptionTest, ApplyWithNullOutput) {
    /**
     * WHAT: Test ternary_attention_apply with NULL output
     * WHY:  Verify returns -1 without crash
     */
    ctx = ternary_attention_create(&config);
    ASSERT_NE(ctx, nullptr);

    const uint32_t seq_len = 8;
    const uint32_t dim = 64;
    std::vector<float> values(seq_len * dim, 1.0f);
    std::vector<ternary_attention_state_t> ternary_attention(seq_len, ATTENTION_NEUTRAL);

    int result = ternary_attention_apply(ctx, values.data(), ternary_attention.data(),
                                         seq_len, dim, nullptr);
    EXPECT_EQ(result, -1) << "Should return -1 for NULL output";
}

TEST_F(TernaryAttentionExceptionTest, ApplyWithZeroSequenceLength) {
    /**
     * WHAT: Test ternary_attention_apply with seq_len = 0
     * WHY:  Verify returns -1 for invalid sequence length
     */
    ctx = ternary_attention_create(&config);
    ASSERT_NE(ctx, nullptr);

    const uint32_t dim = 64;
    std::vector<float> values(dim, 1.0f);
    std::vector<ternary_attention_state_t> ternary_attention(1, ATTENTION_NEUTRAL);
    std::vector<float> output(dim, 0.0f);

    int result = ternary_attention_apply(ctx, values.data(), ternary_attention.data(),
                                         0, dim, output.data());
    EXPECT_EQ(result, -1) << "Should return -1 for zero sequence length";
}

TEST_F(TernaryAttentionExceptionTest, ApplyWithZeroDim) {
    /**
     * WHAT: Test ternary_attention_apply with dim = 0
     * WHY:  Verify returns -1 for invalid dimension
     */
    ctx = ternary_attention_create(&config);
    ASSERT_NE(ctx, nullptr);

    const uint32_t seq_len = 8;
    std::vector<float> values(seq_len, 1.0f);
    std::vector<ternary_attention_state_t> ternary_attention(seq_len, ATTENTION_NEUTRAL);
    std::vector<float> output(seq_len, 0.0f);

    int result = ternary_attention_apply(ctx, values.data(), ternary_attention.data(),
                                         seq_len, 0, output.data());
    EXPECT_EQ(result, -1) << "Should return -1 for zero dimension";
}

TEST_F(TernaryAttentionExceptionTest, BackwardWithNullCtx) {
    /**
     * WHAT: Test ternary_attention_backward with NULL ctx
     * WHY:  Verify returns -1 without crash
     */
    const uint32_t seq_len = 8;
    const uint32_t dim = 64;
    std::vector<float> grad_output(seq_len * dim, 1.0f);
    std::vector<float> soft_attention(seq_len, 0.5f);
    std::vector<float> grad_attention(seq_len, 0.0f);

    int result = ternary_attention_backward(nullptr, grad_output.data(), soft_attention.data(),
                                            seq_len, dim, grad_attention.data());
    EXPECT_EQ(result, -1) << "Should return -1 for NULL ctx";
}

TEST_F(TernaryAttentionExceptionTest, BackwardWithNullGradOutput) {
    /**
     * WHAT: Test ternary_attention_backward with NULL grad_output
     * WHY:  Verify returns -1 without crash
     */
    ctx = ternary_attention_create(&config);
    ASSERT_NE(ctx, nullptr);

    const uint32_t seq_len = 8;
    const uint32_t dim = 64;
    std::vector<float> soft_attention(seq_len, 0.5f);
    std::vector<float> grad_attention(seq_len, 0.0f);

    int result = ternary_attention_backward(ctx, nullptr, soft_attention.data(),
                                            seq_len, dim, grad_attention.data());
    EXPECT_EQ(result, -1) << "Should return -1 for NULL grad_output";
}

TEST_F(TernaryAttentionExceptionTest, BackwardWithNullSoftAttention) {
    /**
     * WHAT: Test ternary_attention_backward with NULL soft_attention
     * WHY:  Verify returns -1 without crash
     */
    ctx = ternary_attention_create(&config);
    ASSERT_NE(ctx, nullptr);

    const uint32_t seq_len = 8;
    const uint32_t dim = 64;
    std::vector<float> grad_output(seq_len * dim, 1.0f);
    std::vector<float> grad_attention(seq_len, 0.0f);

    int result = ternary_attention_backward(ctx, grad_output.data(), nullptr,
                                            seq_len, dim, grad_attention.data());
    EXPECT_EQ(result, -1) << "Should return -1 for NULL soft_attention";
}

TEST_F(TernaryAttentionExceptionTest, BackwardWithNullGradAttention) {
    /**
     * WHAT: Test ternary_attention_backward with NULL grad_attention
     * WHY:  Verify returns -1 without crash
     */
    ctx = ternary_attention_create(&config);
    ASSERT_NE(ctx, nullptr);

    const uint32_t seq_len = 8;
    const uint32_t dim = 64;
    std::vector<float> grad_output(seq_len * dim, 1.0f);
    std::vector<float> soft_attention(seq_len, 0.5f);

    int result = ternary_attention_backward(ctx, grad_output.data(), soft_attention.data(),
                                            seq_len, dim, nullptr);
    EXPECT_EQ(result, -1) << "Should return -1 for NULL grad_attention";
}

TEST_F(TernaryAttentionExceptionTest, BackwardWithZeroSequenceLength) {
    /**
     * WHAT: Test ternary_attention_backward with seq_len = 0
     * WHY:  Verify returns -1 for invalid sequence length
     */
    ctx = ternary_attention_create(&config);
    ASSERT_NE(ctx, nullptr);

    const uint32_t dim = 64;
    std::vector<float> grad_output(dim, 1.0f);
    std::vector<float> soft_attention(1, 0.5f);
    std::vector<float> grad_attention(1, 0.0f);

    int result = ternary_attention_backward(ctx, grad_output.data(), soft_attention.data(),
                                            0, dim, grad_attention.data());
    EXPECT_EQ(result, -1) << "Should return -1 for zero sequence length";
}

TEST_F(TernaryAttentionExceptionTest, BackwardWithZeroDim) {
    /**
     * WHAT: Test ternary_attention_backward with dim = 0
     * WHY:  Verify returns -1 for invalid dimension
     */
    ctx = ternary_attention_create(&config);
    ASSERT_NE(ctx, nullptr);

    const uint32_t seq_len = 8;
    std::vector<float> grad_output(seq_len, 1.0f);
    std::vector<float> soft_attention(seq_len, 0.5f);
    std::vector<float> grad_attention(seq_len, 0.0f);

    int result = ternary_attention_backward(ctx, grad_output.data(), soft_attention.data(),
                                            seq_len, 0, grad_attention.data());
    EXPECT_EQ(result, -1) << "Should return -1 for zero dimension";
}

TEST_F(TernaryAttentionExceptionTest, UpdateTemperatureWithNullCtx) {
    /**
     * WHAT: Test ternary_attention_update_temperature with NULL ctx
     * WHY:  Verify graceful handling without crash
     */
    ternary_attention_update_temperature(nullptr, 100);
    // No crash = success
    SUCCEED();
}

TEST_F(TernaryAttentionExceptionTest, GetTemperatureWithNullCtx) {
    /**
     * WHAT: Test ternary_attention_get_temperature with NULL ctx
     * WHY:  Verify returns default value (1.0f) without crash
     */
    float temp = ternary_attention_get_temperature(nullptr);
    EXPECT_FLOAT_EQ(temp, 1.0f) << "Should return 1.0f for NULL ctx";
}

TEST_F(TernaryAttentionExceptionTest, GetStatsWithNullCtx) {
    /**
     * WHAT: Test ternary_attention_get_stats with NULL ctx
     * WHY:  Verify returns -1 without crash
     */
    ternary_attention_stats_t stats;
    int result = ternary_attention_get_stats(nullptr, &stats);
    EXPECT_EQ(result, -1) << "Should return -1 for NULL ctx";
}

TEST_F(TernaryAttentionExceptionTest, GetStatsWithNullStats) {
    /**
     * WHAT: Test ternary_attention_get_stats with NULL stats
     * WHY:  Verify returns -1 without crash
     */
    ctx = ternary_attention_create(&config);
    ASSERT_NE(ctx, nullptr);

    int result = ternary_attention_get_stats(ctx, nullptr);
    EXPECT_EQ(result, -1) << "Should return -1 for NULL stats";
}

TEST_F(TernaryAttentionExceptionTest, TopKWithNullSoftAttention) {
    /**
     * WHAT: Test ternary_attention_top_k with NULL soft_attention
     * WHY:  Verify returns -1 without crash
     */
    const uint32_t seq_len = 8;
    const uint32_t k = 3;
    std::vector<ternary_attention_state_t> ternary_out(seq_len);

    int result = ternary_attention_top_k(nullptr, seq_len, k, ternary_out.data());
    EXPECT_EQ(result, -1) << "Should return -1 for NULL soft_attention";
}

TEST_F(TernaryAttentionExceptionTest, TopKWithNullOutput) {
    /**
     * WHAT: Test ternary_attention_top_k with NULL ternary_out
     * WHY:  Verify returns -1 without crash
     */
    const uint32_t seq_len = 8;
    const uint32_t k = 3;
    std::vector<float> soft_attention(seq_len, 0.5f);

    int result = ternary_attention_top_k(soft_attention.data(), seq_len, k, nullptr);
    EXPECT_EQ(result, -1) << "Should return -1 for NULL ternary_out";
}

TEST_F(TernaryAttentionExceptionTest, TopKWithZeroSequenceLength) {
    /**
     * WHAT: Test ternary_attention_top_k with seq_len = 0
     * WHY:  Verify returns -1 for invalid sequence length
     */
    const uint32_t k = 3;
    std::vector<float> soft_attention(1, 0.5f);
    std::vector<ternary_attention_state_t> ternary_out(1);

    int result = ternary_attention_top_k(soft_attention.data(), 0, k, ternary_out.data());
    EXPECT_EQ(result, -1) << "Should return -1 for zero sequence length";
}

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(AttentionHeadExceptionTest, CreateWithVeryLargeDimensions) {
    /**
     * WHAT: Test attention_head_create with very large dimensions
     * WHY:  Verify memory allocation handling for large allocations
     */
    config.input_dim = 65536;
    config.output_dim = 65536;
    config.key_dim = 65536;
    config.value_dim = 65536;

    head = attention_head_create(&config);
    // May or may not succeed depending on system memory
    // Just verify no crash
    if (head) {
        attention_head_destroy(head);
        head = nullptr;
    }
    SUCCEED();
}

TEST_F(MultiheadAttentionExceptionTest, CreateWithSingleHead) {
    /**
     * WHAT: Test multihead_attention_create with single head
     * WHY:  Verify edge case of minimum heads works
     */
    config.num_heads = 1;
    config.input_dim = 64;  // Must be divisible by num_heads

    mha = multihead_attention_create(&config);
    EXPECT_NE(mha, nullptr) << "Should succeed with single head";
}

TEST_F(TernaryAttentionExceptionTest, DiscretizeWithValidInput) {
    /**
     * WHAT: Test ternary_attention_discretize with valid inputs
     * WHY:  Verify successful operation returns 0
     */
    ctx = ternary_attention_create(&config);
    ASSERT_NE(ctx, nullptr);

    const uint32_t seq_len = 8;
    std::vector<float> soft_attention = {0.1f, 0.2f, 0.5f, 0.8f, 0.9f, 0.3f, 0.6f, 0.4f};
    std::vector<ternary_attention_state_t> ternary_out(seq_len);

    int result = ternary_attention_discretize(ctx, soft_attention.data(), seq_len, ternary_out.data());
    EXPECT_EQ(result, 0) << "Should return 0 for valid inputs";

    // Verify discretization is correct
    EXPECT_EQ(ternary_out[0], ATTENTION_SUPPRESS);  // 0.1 < 0.3
    EXPECT_EQ(ternary_out[3], ATTENTION_FOCUS);     // 0.8 > 0.7
    EXPECT_EQ(ternary_out[4], ATTENTION_FOCUS);     // 0.9 > 0.7
}

TEST_F(TernaryAttentionExceptionTest, TopKWithKGreaterThanLength) {
    /**
     * WHAT: Test ternary_attention_top_k with k > seq_length
     * WHY:  Verify k is clamped to valid range
     */
    const uint32_t seq_len = 4;
    const uint32_t k = 10;  // Greater than seq_len
    std::vector<float> soft_attention = {0.1f, 0.8f, 0.5f, 0.3f};
    std::vector<ternary_attention_state_t> ternary_out(seq_len);

    int result = ternary_attention_top_k(soft_attention.data(), seq_len, k, ternary_out.data());
    EXPECT_EQ(result, 0) << "Should succeed with clamped k";

    // All should be FOCUS since k >= seq_len
    for (uint32_t i = 0; i < seq_len; i++) {
        EXPECT_EQ(ternary_out[i], ATTENTION_FOCUS);
    }
}

TEST_F(TernaryAttentionExceptionTest, TopKWithKZero) {
    /**
     * WHAT: Test ternary_attention_top_k with k = 0
     * WHY:  Verify k is clamped to minimum of 1
     */
    const uint32_t seq_len = 4;
    const uint32_t k = 0;
    std::vector<float> soft_attention = {0.1f, 0.8f, 0.5f, 0.3f};
    std::vector<ternary_attention_state_t> ternary_out(seq_len);

    int result = ternary_attention_top_k(soft_attention.data(), seq_len, k, ternary_out.data());
    EXPECT_EQ(result, 0) << "Should succeed with clamped k=1";

    // Exactly one FOCUS (the max at index 1)
    int focus_count = 0;
    for (uint32_t i = 0; i < seq_len; i++) {
        if (ternary_out[i] == ATTENTION_FOCUS) {
            focus_count++;
        }
    }
    EXPECT_EQ(focus_count, 1) << "Should have exactly 1 FOCUS with k=0 (clamped to 1)";
    EXPECT_EQ(ternary_out[1], ATTENTION_FOCUS) << "Max value at index 1 should be FOCUS";
}
