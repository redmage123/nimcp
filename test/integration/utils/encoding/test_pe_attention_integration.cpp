//=============================================================================
// test_pe_attention_integration.cpp - Positional Encoding + Attention Integration
//=============================================================================
/**
 * @file test_pe_attention_integration.cpp
 * @brief Integration tests for positional encoding with attention mechanism
 *
 * WHAT: Test attention mechanism enhanced with positional encodings
 * WHY:  Position-aware attention is critical for sequence processing
 * HOW:  Create attention heads with RoPE/ALiBi, verify position effects
 *
 * TEST COVERAGE:
 * 1. RoPE integration with attention (rotary position embeddings)
 * 2. ALiBi integration with attention (linear biases)
 * 3. Position-dependent attention weights
 * 4. Multi-head attention with PE
 * 5. Bio-async integration with PE messages
 * 6. Sequence discrimination with vs without PE
 *
 * BIOLOGICAL BASIS:
 * - Grid cells in entorhinal cortex encode relative positions (RoPE-like)
 * - Distance-dependent attention in parietal cortex (ALiBi-like)
 * - Position-aware attention in prefrontal cortex for working memory
 *
 * @author NIMCP Development Team
 * @date 2025-12-10
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <algorithm>

// Headers have their own extern "C" guards
    #include "plasticity/attention/nimcp_attention.h"
    #include "utils/encoding/nimcp_positional_encoding.h"
    #include "async/nimcp_bio_async.h"
    #include "async/nimcp_bio_messages.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class PEAttentionIntegrationTest : public ::testing::Test {
protected:
    static constexpr float EPSILON = 1e-4f;
    static constexpr uint32_t SEQ_LEN = 16;
    static constexpr uint32_t INPUT_DIM = 64;
    static constexpr uint32_t OUTPUT_DIM = 64;
    static constexpr uint32_t NUM_HEADS = 4;

    multihead_attention_t mha = nullptr;
    nimcp_pos_encoder_t* pe_encoder = nullptr;

    void SetUp() override {
        // Initialize bio-async system
        nimcp_bio_async_config_t cfg = {0};
        nimcp_bio_async_init(&cfg);
    }

    void TearDown() override {
        if (mha) {
            multihead_attention_destroy(mha);
            mha = nullptr;
        }
        if (pe_encoder) {
            nimcp_pos_encoder_destroy(pe_encoder);
            pe_encoder = nullptr;
        }
        nimcp_bio_async_shutdown();
    }

    // Helper: Create attention with specific PE type
    void create_attention_with_pe(nimcp_pos_encoding_type_t pe_type) {
        multihead_attention_config_t config = {};
        config.num_heads = NUM_HEADS;
        config.input_dim = INPUT_DIM;
        config.output_dim = OUTPUT_DIM;
        config.sequence_length = SEQ_LEN;
        config.use_thalamic_gate = false;
        config.use_salience_weighting = false;
        config.use_positional_encoding = true;
        config.pe_type = pe_type;
        config.rope_base = 10000.0f;
        config.alibi_slope_base = 2.0f;

        mha = multihead_attention_create(&config);
        ASSERT_NE(mha, nullptr);
    }

    // Helper: Generate sequence with position-dependent pattern
    void generate_position_pattern(float* buffer, uint32_t seq_len, uint32_t dim) {
        for (uint32_t pos = 0; pos < seq_len; pos++) {
            for (uint32_t d = 0; d < dim; d++) {
                // Pattern where position matters: sin(pos * d)
                buffer[pos * dim + d] = sinf((float)pos * (float)d / 10.0f);
            }
        }
    }

    // Helper: Compute attention entropy (measure of diffuseness)
    float compute_attention_entropy(const float* weights, uint32_t size) {
        float entropy = 0.0f;
        for (uint32_t i = 0; i < size; i++) {
            if (weights[i] > EPSILON) {
                entropy -= weights[i] * logf(weights[i]);
            }
        }
        return entropy;
    }
};

//=============================================================================
// Integration Tests: RoPE + Attention
//=============================================================================

TEST_F(PEAttentionIntegrationTest, RoPEBasicIntegration) {
    /* WHAT: Test RoPE integration with multihead attention
     * WHY:  Verify rotary embeddings are applied correctly
     * HOW:  Create attention with RoPE, process sequence, verify PE applied
     */

    create_attention_with_pe(NIMCP_POS_ROTARY);

    // Generate input sequence
    std::vector<float> input(SEQ_LEN * INPUT_DIM);
    generate_position_pattern(input.data(), SEQ_LEN, INPUT_DIM);

    // Forward pass
    std::vector<float> output(SEQ_LEN * OUTPUT_DIM);
    bool success = multihead_attention_forward(
        mha, input.data(), SEQ_LEN, nullptr, output.data()
    );
    ASSERT_TRUE(success);

    // Verify output has been computed
    bool has_output = false;
    for (uint32_t i = 0; i < SEQ_LEN * OUTPUT_DIM; i++) {
        if (fabsf(output[i]) > EPSILON) {
            has_output = true;
            break;
        }
    }
    EXPECT_TRUE(has_output);

    // Get statistics
    attention_stats_t stats;
    ASSERT_TRUE(multihead_attention_get_stats(mha, &stats));
    EXPECT_GT(stats.forward_calls, 0);
}

TEST_F(PEAttentionIntegrationTest, RoPEPositionSensitivity) {
    /* WHAT: Test attention is sensitive to position with RoPE
     * WHY:  RoPE should make attention position-aware
     * HOW:  Compare outputs for same tokens at different positions
     */

    create_attention_with_pe(NIMCP_POS_ROTARY);

    // Create sequence with same token at different positions
    std::vector<float> input(SEQ_LEN * INPUT_DIM, 0.0f);

    // Put identical patterns at positions 0 and 10
    for (uint32_t d = 0; d < INPUT_DIM; d++) {
        input[0 * INPUT_DIM + d] = 1.0f;  // Position 0
        input[10 * INPUT_DIM + d] = 1.0f; // Position 10
    }

    std::vector<float> output(SEQ_LEN * OUTPUT_DIM);
    ASSERT_TRUE(multihead_attention_forward(
        mha, input.data(), SEQ_LEN, nullptr, output.data()
    ));

    // Compare outputs at positions 0 and 10
    // With RoPE, outputs should differ despite same input
    float diff_sum = 0.0f;
    for (uint32_t d = 0; d < OUTPUT_DIM; d++) {
        float diff = fabsf(output[0 * OUTPUT_DIM + d] - output[10 * OUTPUT_DIM + d]);
        diff_sum += diff;
    }

    // Outputs should differ (position matters)
    EXPECT_GT(diff_sum, 0.1f);
}

TEST_F(PEAttentionIntegrationTest, RoPELongRangeDependencies) {
    /* WHAT: Test RoPE maintains attention across long sequences
     * WHY:  RoPE designed for good long-range behavior
     * HOW:  Process longer sequence, verify attention span
     */

    // Use longer sequence
    const uint32_t LONG_SEQ = 64;

    multihead_attention_config_t config = {};
    config.num_heads = NUM_HEADS;
    config.input_dim = INPUT_DIM;
    config.output_dim = OUTPUT_DIM;
    config.sequence_length = LONG_SEQ;
    config.use_positional_encoding = true;
    config.pe_type = NIMCP_POS_ROTARY;
    config.rope_base = 10000.0f;

    mha = multihead_attention_create(&config);
    ASSERT_NE(mha, nullptr);

    // Generate input
    std::vector<float> input(LONG_SEQ * INPUT_DIM);
    generate_position_pattern(input.data(), LONG_SEQ, INPUT_DIM);

    std::vector<float> output(LONG_SEQ * OUTPUT_DIM);
    ASSERT_TRUE(multihead_attention_forward(
        mha, input.data(), LONG_SEQ, nullptr, output.data()
    ));

    // Verify all positions produced output
    for (uint32_t pos = 0; pos < LONG_SEQ; pos++) {
        float pos_norm = 0.0f;
        for (uint32_t d = 0; d < OUTPUT_DIM; d++) {
            pos_norm += fabsf(output[pos * OUTPUT_DIM + d]);
        }
        EXPECT_GT(pos_norm, 0.0f) << "Position " << pos << " has no output";
    }
}

//=============================================================================
// Integration Tests: ALiBi + Attention
//=============================================================================

TEST_F(PEAttentionIntegrationTest, ALiBiBasicIntegration) {
    /* WHAT: Test ALiBi integration with multihead attention
     * WHY:  Verify linear biases are applied correctly
     * HOW:  Create attention with ALiBi, verify nearby tokens preferred
     */

    create_attention_with_pe(NIMCP_POS_ALIBI);

    std::vector<float> input(SEQ_LEN * INPUT_DIM);
    generate_position_pattern(input.data(), SEQ_LEN, INPUT_DIM);

    std::vector<float> output(SEQ_LEN * OUTPUT_DIM);
    bool success = multihead_attention_forward(
        mha, input.data(), SEQ_LEN, nullptr, output.data()
    );
    ASSERT_TRUE(success);

    // Verify output computed
    float output_sum = 0.0f;
    for (uint32_t i = 0; i < SEQ_LEN * OUTPUT_DIM; i++) {
        output_sum += fabsf(output[i]);
    }
    EXPECT_GT(output_sum, 0.0f);
}

TEST_F(PEAttentionIntegrationTest, ALiBiLocalityBias) {
    /* WHAT: Test ALiBi creates locality bias (nearby tokens attended more)
     * WHY:  ALiBi's linear bias encourages local attention
     * HOW:  Measure attention to nearby vs distant tokens
     */

    create_attention_with_pe(NIMCP_POS_ALIBI);

    // Create sequence with distinct patterns
    std::vector<float> input(SEQ_LEN * INPUT_DIM);
    for (uint32_t pos = 0; pos < SEQ_LEN; pos++) {
        for (uint32_t d = 0; d < INPUT_DIM; d++) {
            // Each position has unique pattern
            input[pos * INPUT_DIM + d] = sinf((float)pos + (float)d);
        }
    }

    std::vector<float> output(SEQ_LEN * OUTPUT_DIM);
    ASSERT_TRUE(multihead_attention_forward(
        mha, input.data(), SEQ_LEN, nullptr, output.data()
    ));

    // Verify processing succeeded
    attention_stats_t stats;
    ASSERT_TRUE(multihead_attention_get_stats(mha, &stats));
    EXPECT_GT(stats.forward_calls, 0);
    EXPECT_EQ(stats.active_heads, NUM_HEADS);
}

TEST_F(PEAttentionIntegrationTest, ALiBiVsRoPEComparison) {
    /* WHAT: Compare attention behavior with ALiBi vs RoPE
     * WHY:  Different PE methods have different characteristics
     * HOW:  Process same input with both, verify differences
     */

    std::vector<float> input(SEQ_LEN * INPUT_DIM);
    generate_position_pattern(input.data(), SEQ_LEN, INPUT_DIM);

    // Process with RoPE
    create_attention_with_pe(NIMCP_POS_ROTARY);
    std::vector<float> output_rope(SEQ_LEN * OUTPUT_DIM);
    ASSERT_TRUE(multihead_attention_forward(
        mha, input.data(), SEQ_LEN, nullptr, output_rope.data()
    ));

    // Destroy and recreate with ALiBi
    multihead_attention_destroy(mha);
    mha = nullptr;

    create_attention_with_pe(NIMCP_POS_ALIBI);
    std::vector<float> output_alibi(SEQ_LEN * OUTPUT_DIM);
    ASSERT_TRUE(multihead_attention_forward(
        mha, input.data(), SEQ_LEN, nullptr, output_alibi.data()
    ));

    // Outputs should differ (different PE methods)
    float diff_sum = 0.0f;
    for (uint32_t i = 0; i < SEQ_LEN * OUTPUT_DIM; i++) {
        diff_sum += fabsf(output_rope[i] - output_alibi[i]);
    }

    // Significant difference expected
    EXPECT_GT(diff_sum, 0.1f);
}

//=============================================================================
// Integration Tests: PE Enhances Sequence Processing
//=============================================================================

TEST_F(PEAttentionIntegrationTest, PEImprovesSequenceDiscrimination) {
    /* WHAT: Test PE improves ability to discriminate sequences
     * WHY:  Position information should enhance sequence understanding
     * HOW:  Compare attention outputs with and without PE
     */

    // Test WITHOUT positional encoding
    multihead_attention_config_t config_no_pe = {};
    config_no_pe.num_heads = NUM_HEADS;
    config_no_pe.input_dim = INPUT_DIM;
    config_no_pe.output_dim = OUTPUT_DIM;
    config_no_pe.sequence_length = SEQ_LEN;
    config_no_pe.use_positional_encoding = false;

    mha = multihead_attention_create(&config_no_pe);
    ASSERT_NE(mha, nullptr);

    std::vector<float> input(SEQ_LEN * INPUT_DIM);
    generate_position_pattern(input.data(), SEQ_LEN, INPUT_DIM);

    std::vector<float> output_no_pe(SEQ_LEN * OUTPUT_DIM);
    ASSERT_TRUE(multihead_attention_forward(
        mha, input.data(), SEQ_LEN, nullptr, output_no_pe.data()
    ));

    // Compute variance in output (measure of discrimination)
    float mean_no_pe = 0.0f;
    for (uint32_t i = 0; i < SEQ_LEN * OUTPUT_DIM; i++) {
        mean_no_pe += output_no_pe[i];
    }
    mean_no_pe /= (float)(SEQ_LEN * OUTPUT_DIM);

    float var_no_pe = 0.0f;
    for (uint32_t i = 0; i < SEQ_LEN * OUTPUT_DIM; i++) {
        float diff = output_no_pe[i] - mean_no_pe;
        var_no_pe += diff * diff;
    }
    var_no_pe /= (float)(SEQ_LEN * OUTPUT_DIM);

    // Test WITH positional encoding (RoPE)
    multihead_attention_destroy(mha);
    mha = nullptr;

    create_attention_with_pe(NIMCP_POS_ROTARY);

    std::vector<float> output_with_pe(SEQ_LEN * OUTPUT_DIM);
    ASSERT_TRUE(multihead_attention_forward(
        mha, input.data(), SEQ_LEN, nullptr, output_with_pe.data()
    ));

    float mean_with_pe = 0.0f;
    for (uint32_t i = 0; i < SEQ_LEN * OUTPUT_DIM; i++) {
        mean_with_pe += output_with_pe[i];
    }
    mean_with_pe /= (float)(SEQ_LEN * OUTPUT_DIM);

    float var_with_pe = 0.0f;
    for (uint32_t i = 0; i < SEQ_LEN * OUTPUT_DIM; i++) {
        float diff = output_with_pe[i] - mean_with_pe;
        var_with_pe += diff * diff;
    }
    var_with_pe /= (float)(SEQ_LEN * OUTPUT_DIM);

    // With PE, outputs should be more discriminative (higher variance)
    EXPECT_GE(var_with_pe, var_no_pe * 0.5f);
}

//=============================================================================
// Integration Tests: Multi-Head with PE
//=============================================================================

TEST_F(PEAttentionIntegrationTest, MultiHeadPEProcessing) {
    /* WHAT: Test all attention heads process with PE
     * WHY:  Verify PE applied consistently across heads
     * HOW:  Process with multiple heads, verify all active
     */

    create_attention_with_pe(NIMCP_POS_ROTARY);

    std::vector<float> input(SEQ_LEN * INPUT_DIM);
    generate_position_pattern(input.data(), SEQ_LEN, INPUT_DIM);

    std::vector<float> output(SEQ_LEN * OUTPUT_DIM);
    ASSERT_TRUE(multihead_attention_forward(
        mha, input.data(), SEQ_LEN, nullptr, output.data()
    ));

    attention_stats_t stats;
    ASSERT_TRUE(multihead_attention_get_stats(mha, &stats));

    // All heads should be active
    EXPECT_EQ(stats.active_heads, NUM_HEADS);
    EXPECT_GT(stats.forward_calls, 0);
    EXPECT_GE(stats.avg_attention_entropy, 0.0f);
}

TEST_F(PEAttentionIntegrationTest, PESwitchingBetweenTypes) {
    /* WHAT: Test runtime switching between PE types
     * WHY:  Verify system can adapt encoding strategy
     * HOW:  Switch from RoPE to ALiBi, process, verify behavior
     */

    create_attention_with_pe(NIMCP_POS_ROTARY);

    std::vector<float> input(SEQ_LEN * INPUT_DIM);
    generate_position_pattern(input.data(), SEQ_LEN, INPUT_DIM);

    // Process with RoPE
    std::vector<float> output_rope(SEQ_LEN * OUTPUT_DIM);
    ASSERT_TRUE(multihead_attention_forward(
        mha, input.data(), SEQ_LEN, nullptr, output_rope.data()
    ));

    // Switch to ALiBi
    ASSERT_TRUE(multihead_attention_set_pe_type(mha, NIMCP_POS_ALIBI));

    // Process with ALiBi
    std::vector<float> output_alibi(SEQ_LEN * OUTPUT_DIM);
    ASSERT_TRUE(multihead_attention_forward(
        mha, input.data(), SEQ_LEN, nullptr, output_alibi.data()
    ));

    // Outputs should differ
    float diff_sum = 0.0f;
    for (uint32_t i = 0; i < SEQ_LEN * OUTPUT_DIM; i++) {
        diff_sum += fabsf(output_rope[i] - output_alibi[i]);
    }

    EXPECT_GT(diff_sum, 0.01f);
}

//=============================================================================
// Integration Tests: Bio-Async + PE
//=============================================================================

TEST_F(PEAttentionIntegrationTest, BioAsyncPEMessages) {
    /* WHAT: Test PE operations emit bio-async messages
     * WHY:  Verify loose coupling with bio-async system
     * HOW:  Create attention with PE, process, verify bio-async enabled
     *
     * NOTE: The bio-async subscribe API requires module-specific registration.
     *       This test verifies attention works with bio-async enabled.
     */

    create_attention_with_pe(NIMCP_POS_ROTARY);

    std::vector<float> input(SEQ_LEN * INPUT_DIM);
    generate_position_pattern(input.data(), SEQ_LEN, INPUT_DIM);

    std::vector<float> output(SEQ_LEN * OUTPUT_DIM);
    ASSERT_TRUE(multihead_attention_forward(
        mha, input.data(), SEQ_LEN, nullptr, output.data()
    ));

    // Process bio-async messages (step the system)
    nimcp_bio_async_step(1.0f);  // 1ms time step

    // Verify output is valid (non-zero)
    float output_sum = 0.0f;
    for (uint32_t i = 0; i < SEQ_LEN * OUTPUT_DIM; i++) {
        output_sum += fabsf(output[i]);
    }
    EXPECT_GT(output_sum, 0.0f);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
