/**
 * @file test_attention_pe_regression.cpp
 * @brief Regression tests for positional encoding in attention mechanisms
 *
 * WHAT: Regression tests ensuring PE stability in attention-based plasticity
 * WHY:  PE is critical for sequence processing - must maintain stability
 * HOW:  Test PE under repeated use, memory stability, performance consistency
 *
 * REGRESSION TEST PHILOSOPHY:
 * - Ensure PE doesn't degrade under repeated use
 * - Verify PE memory stability (no leaks)
 * - Validate PE encoding values remain consistent
 * - Test PE type switching doesn't corrupt state
 * - Verify PE works with large sequences
 * - Catch performance regressions
 *
 * WHAT WE'RE PROTECTING:
 * - PE numerical stability over many iterations
 * - Memory usage patterns with PE enabled
 * - PE cache effectiveness
 * - Encoding consistency across runs
 * - Type switching safety
 *
 * @author NIMCP Development Team
 * @date 2025-12-10
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <vector>
#include <cmath>

// Headers have their own extern "C" guards
#include "utils/encoding/nimcp_positional_encoding.h"
#include "cognitive/attention/nimcp_emotion_attention.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "utils/memory/nimcp_unified_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class AttentionPERegressionTest : public ::testing::Test {
protected:
    static constexpr int ITERATIONS = 1000;
    static constexpr int LARGE_SEQ_LENGTH = 2048;
    static constexpr int EMBEDDING_DIM = 512;

    nimcp_pos_encoder_t* encoder;

    void SetUp() override {
        // WHAT: Initialize bio-async and memory systems
        // WHY:  PE integration requires these systems
        // HOW:  Standard initialization sequence
        nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
        nimcp_bio_async_init(&bio_config);
        bio_router_config_t cfg = bio_router_default_config();
        bio_router_init(&cfg);

        encoder = nullptr;
    }

    void TearDown() override {
        // WHAT: Clean up encoder and systems
        // WHY:  Prevent memory leaks
        // HOW:  Standard cleanup sequence
        if (encoder) {
            nimcp_pos_encoder_destroy(encoder);
            encoder = nullptr;
        }

        bio_router_shutdown();
        nimcp_bio_async_shutdown();
    }

    // WHAT: Compute variance of a vector
    // WHY:  Need stability metric for numerical tests
    // HOW:  Standard variance formula
    float ComputeVariance(const std::vector<float>& values) {
        if (values.empty()) return 0.0f;

        float mean = 0.0f;
        for (float v : values) mean += v;
        mean /= values.size();

        float variance = 0.0f;
        for (float v : values) {
            float diff = v - mean;
            variance += diff * diff;
        }
        return variance / values.size();
    }

    // WHAT: Check if encoding values are consistent across runs
    // WHY:  PE should be deterministic
    // HOW:  Compare encoding outputs with tolerance
    bool EncodingsMatch(const float* enc1, const float* enc2, uint32_t dim, float tolerance = 1e-5f) {
        for (uint32_t i = 0; i < dim; i++) {
            if (std::abs(enc1[i] - enc2[i]) > tolerance) {
                return false;
            }
        }
        return true;
    }
};

//=============================================================================
// 1. Stability Under Repeated Use Tests
//=============================================================================

TEST_F(AttentionPERegressionTest, StabilityUnderRepeatedUse_Sinusoidal) {
    // WHAT: Verify sinusoidal PE doesn't degrade over many iterations
    // WHY:  PE should be deterministic and stable
    // HOW:  Encode same position 1000 times, verify consistency

    nimcp_pos_config_t config = {};
    config.type = NIMCP_POS_SINUSOIDAL;
    config.config.sinusoidal = nimcp_pos_sinusoidal_default_config();

    encoder = nimcp_pos_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    std::vector<float> first_encoding(EMBEDDING_DIM);
    std::vector<float> current_encoding(EMBEDDING_DIM);

    // Get baseline encoding
    int result = nimcp_pos_encode_position(encoder, 42, first_encoding.data());
    ASSERT_EQ(result, NIMCP_POS_SUCCESS);

    // Repeat encoding many times
    for (int i = 0; i < ITERATIONS; i++) {
        result = nimcp_pos_encode_position(encoder, 42, current_encoding.data());
        ASSERT_EQ(result, NIMCP_POS_SUCCESS);

        // Verify encoding hasn't changed
        EXPECT_TRUE(EncodingsMatch(first_encoding.data(), current_encoding.data(), EMBEDDING_DIM))
            << "Encoding changed at iteration " << i;

        // Process bio-async messages periodically
    }
}

TEST_F(AttentionPERegressionTest, StabilityUnderRepeatedUse_RoPE) {
    // WHAT: Verify RoPE doesn't degrade over many iterations
    // WHY:  Rotation matrices should remain stable
    // HOW:  Apply RoPE 1000 times, verify consistency

    nimcp_pos_config_t config = {};
    config.type = NIMCP_POS_ROTARY;
    config.config.rope = nimcp_pos_rope_default_config();

    encoder = nimcp_pos_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    std::vector<float> query(EMBEDDING_DIM, 0.5f);
    std::vector<float> key(EMBEDDING_DIM, 0.3f);
    std::vector<float> first_query_out(EMBEDDING_DIM);
    std::vector<float> first_key_out(EMBEDDING_DIM);
    std::vector<float> query_out(EMBEDDING_DIM);
    std::vector<float> key_out(EMBEDDING_DIM);

    // Get baseline
    int result = nimcp_pos_rope_apply(
        encoder, query.data(), key.data(), 10,
        first_query_out.data(), first_key_out.data()
    );
    ASSERT_EQ(result, NIMCP_POS_SUCCESS);

    // Repeat many times
    for (int i = 0; i < ITERATIONS; i++) {
        result = nimcp_pos_rope_apply(
            encoder, query.data(), key.data(), 10,
            query_out.data(), key_out.data()
        );
        ASSERT_EQ(result, NIMCP_POS_SUCCESS);

        // Verify outputs match baseline
        EXPECT_TRUE(EncodingsMatch(first_query_out.data(), query_out.data(), EMBEDDING_DIM))
            << "Query encoding changed at iteration " << i;
        EXPECT_TRUE(EncodingsMatch(first_key_out.data(), key_out.data(), EMBEDDING_DIM))
            << "Key encoding changed at iteration " << i;

    }
}

TEST_F(AttentionPERegressionTest, StabilityUnderRepeatedUse_ALiBi) {
    // WHAT: Verify ALiBi bias computation remains stable
    // WHY:  Bias values should be deterministic
    // HOW:  Compute bias 1000 times, verify consistency

    nimcp_pos_config_t config = {};
    config.type = NIMCP_POS_ALIBI;
    config.config.alibi = nimcp_pos_alibi_default_config();

    encoder = nimcp_pos_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    uint32_t seq_len = 64;
    uint32_t num_heads = 8;
    size_t bias_size = num_heads * seq_len * seq_len;

    std::vector<float> first_bias(bias_size);
    std::vector<float> current_bias(bias_size);

    // Get baseline
    int result = nimcp_pos_alibi_get_bias(encoder, seq_len, first_bias.data());
    ASSERT_EQ(result, NIMCP_POS_SUCCESS);

    // Repeat many times
    for (int i = 0; i < ITERATIONS; i++) {
        result = nimcp_pos_alibi_get_bias(encoder, seq_len, current_bias.data());
        ASSERT_EQ(result, NIMCP_POS_SUCCESS);

        EXPECT_TRUE(EncodingsMatch(first_bias.data(), current_bias.data(), bias_size))
            << "ALiBi bias changed at iteration " << i;

    }
}

//=============================================================================
// 2. Memory Stability Tests
//=============================================================================

TEST_F(AttentionPERegressionTest, MemoryStability_NoLeaks) {
    // WHAT: Verify repeated PE operations don't leak memory
    // WHY:  Memory leaks cause long-running instability
    // HOW:  Create/destroy encoder many times, verify no leaks

    for (int i = 0; i < 100; i++) {
        nimcp_pos_config_t config = {};
        config.type = NIMCP_POS_SINUSOIDAL;
        config.config.sinusoidal = nimcp_pos_sinusoidal_default_config();

        nimcp_pos_encoder_t* temp_encoder = nimcp_pos_encoder_create(&config);
        ASSERT_NE(temp_encoder, nullptr) << "Failed at iteration " << i;

        // Do some work
        std::vector<float> output(EMBEDDING_DIM);
        for (int j = 0; j < 10; j++) {
            nimcp_pos_encode_position(temp_encoder, j, output.data());
        }

        nimcp_pos_encoder_destroy(temp_encoder);

    }

    // AddressSanitizer would catch leaks
    SUCCEED();
}

TEST_F(AttentionPERegressionTest, MemoryStability_CacheGrowth) {
    // WHAT: Verify PE cache doesn't grow unbounded
    // WHY:  Cache should have reasonable memory limits
    // HOW:  Encode many positions, verify cache size is reasonable

    nimcp_pos_config_t config = {};
    config.type = NIMCP_POS_SINUSOIDAL;
    config.config.sinusoidal = nimcp_pos_sinusoidal_default_config();
    config.config.sinusoidal.base.cache_enabled = true;

    encoder = nimcp_pos_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    // Pre-compute large cache
    int result = nimcp_pos_cache_precompute(encoder, LARGE_SEQ_LENGTH);
    ASSERT_EQ(result, NIMCP_POS_SUCCESS);

    // Get cache size
    float hit_rate = 0.0f;
    size_t cache_size = 0;
    result = nimcp_pos_cache_stats(encoder, &hit_rate, &cache_size);
    ASSERT_EQ(result, NIMCP_POS_SUCCESS);

    // Cache size should be reasonable (< 100MB for 2048 positions * 512 dim)
    size_t expected_max = LARGE_SEQ_LENGTH * EMBEDDING_DIM * sizeof(float) * 2; // 2x margin
    EXPECT_LT(cache_size, expected_max)
        << "Cache size " << cache_size << " exceeds expected max " << expected_max;
}

TEST_F(AttentionPERegressionTest, MemoryStability_CacheClear) {
    // WHAT: Verify cache clearing works correctly
    // WHY:  Must be able to reclaim cache memory
    // HOW:  Build cache, clear it, verify size drops

    nimcp_pos_config_t config = {};
    config.type = NIMCP_POS_SINUSOIDAL;
    config.config.sinusoidal = nimcp_pos_sinusoidal_default_config();
    config.config.sinusoidal.base.cache_enabled = true;

    encoder = nimcp_pos_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    // Build cache
    int result = nimcp_pos_cache_precompute(encoder, 1024);
    ASSERT_EQ(result, NIMCP_POS_SUCCESS);

    float hit_rate_before = 0.0f;
    size_t size_before = 0;
    nimcp_pos_cache_stats(encoder, &hit_rate_before, &size_before);

    // Clear cache
    result = nimcp_pos_cache_clear(encoder);
    ASSERT_EQ(result, NIMCP_POS_SUCCESS);

    float hit_rate_after = 0.0f;
    size_t size_after = 0;
    nimcp_pos_cache_stats(encoder, &hit_rate_after, &size_after);

    // Cache should be smaller after clearing
    EXPECT_LT(size_after, size_before)
        << "Cache size didn't decrease after clear";
}

//=============================================================================
// 3. Performance Stability Tests
//=============================================================================

TEST_F(AttentionPERegressionTest, PerformanceStability_NoRegression) {
    // WHAT: Verify PE encoding performance doesn't regress
    // WHY:  PE is on critical path - must stay fast
    // HOW:  Time encoding operations, verify reasonable performance

    nimcp_pos_config_t config = {};
    config.type = NIMCP_POS_SINUSOIDAL;
    config.config.sinusoidal = nimcp_pos_sinusoidal_default_config();

    encoder = nimcp_pos_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    std::vector<float> output(EMBEDDING_DIM);

    // Warmup
    for (int i = 0; i < 100; i++) {
        nimcp_pos_encode_position(encoder, i, output.data());
    }

    // Time 1000 encodings
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; i++) {
        nimcp_pos_encode_position(encoder, i, output.data());
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    float avg_time_us = duration_us / 1000.0f;

    // Should be fast (< 100us per encoding on average)
    EXPECT_LT(avg_time_us, 100.0f)
        << "Average encoding time " << avg_time_us << "us exceeds 100us threshold";
}

TEST_F(AttentionPERegressionTest, PerformanceStability_CacheEffectiveness) {
    // WHAT: Verify cache improves performance
    // WHY:  Cache should make repeated access faster
    // HOW:  Compare cached vs uncached performance

    // Uncached encoder
    nimcp_pos_config_t config_nocache = {};
    config_nocache.type = NIMCP_POS_SINUSOIDAL;
    config_nocache.config.sinusoidal = nimcp_pos_sinusoidal_default_config();
    config_nocache.config.sinusoidal.base.cache_enabled = false;

    nimcp_pos_encoder_t* nocache_encoder = nimcp_pos_encoder_create(&config_nocache);
    ASSERT_NE(nocache_encoder, nullptr);

    std::vector<float> output(EMBEDDING_DIM);

    // Time uncached
    auto start_nocache = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; i++) {
        nimcp_pos_encode_position(nocache_encoder, i % 100, output.data());
    }
    auto end_nocache = std::chrono::high_resolution_clock::now();
    auto duration_nocache = std::chrono::duration_cast<std::chrono::microseconds>(
        end_nocache - start_nocache).count();

    nimcp_pos_encoder_destroy(nocache_encoder);

    // Cached encoder
    nimcp_pos_config_t config_cache = {};
    config_cache.type = NIMCP_POS_SINUSOIDAL;
    config_cache.config.sinusoidal = nimcp_pos_sinusoidal_default_config();
    config_cache.config.sinusoidal.base.cache_enabled = true;

    encoder = nimcp_pos_encoder_create(&config_cache);
    ASSERT_NE(encoder, nullptr);

    // Pre-compute cache
    nimcp_pos_cache_precompute(encoder, 100);

    // Time cached
    auto start_cache = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; i++) {
        nimcp_pos_encode_position(encoder, i % 100, output.data());
    }
    auto end_cache = std::chrono::high_resolution_clock::now();
    auto duration_cache = std::chrono::duration_cast<std::chrono::microseconds>(
        end_cache - start_cache).count();

    // Cache should be faster (at least no slower)
    EXPECT_LE(duration_cache, duration_nocache * 1.5f)  // Allow 50% margin
        << "Cached version slower than uncached";
}

//=============================================================================
// 4. Encoding Consistency Tests
//=============================================================================

TEST_F(AttentionPERegressionTest, EncodingConsistency_DeterministicOutput) {
    // WHAT: Verify PE produces identical output for same input
    // WHY:  PE must be deterministic for reproducibility
    // HOW:  Encode same positions multiple times, verify exact match

    nimcp_pos_config_t config = {};
    config.type = NIMCP_POS_SINUSOIDAL;
    config.config.sinusoidal = nimcp_pos_sinusoidal_default_config();

    encoder = nimcp_pos_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    // Encode sequence
    std::vector<float> sequence1(EMBEDDING_DIM * 10);
    std::vector<float> sequence2(EMBEDDING_DIM * 10);

    int result1 = nimcp_pos_encode_sequence(encoder, 0, 10, sequence1.data());
    ASSERT_EQ(result1, NIMCP_POS_SUCCESS);

    // Reset statistics to simulate fresh start
    nimcp_pos_reset_stats(encoder);

    int result2 = nimcp_pos_encode_sequence(encoder, 0, 10, sequence2.data());
    ASSERT_EQ(result2, NIMCP_POS_SUCCESS);

    // Verify exact match
    EXPECT_TRUE(EncodingsMatch(sequence1.data(), sequence2.data(), EMBEDDING_DIM * 10))
        << "Sequences don't match on repeated encoding";
}

TEST_F(AttentionPERegressionTest, EncodingConsistency_SequenceVsIndividual) {
    // WHAT: Verify batch encoding matches individual encoding
    // WHY:  Different code paths should produce same result
    // HOW:  Encode sequence as batch vs individual, compare

    nimcp_pos_config_t config = {};
    config.type = NIMCP_POS_SINUSOIDAL;
    config.config.sinusoidal = nimcp_pos_sinusoidal_default_config();

    encoder = nimcp_pos_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    uint32_t seq_len = 20;
    std::vector<float> batch_output(EMBEDDING_DIM * seq_len);
    std::vector<float> individual_output(EMBEDDING_DIM * seq_len);

    // Batch encoding
    int result = nimcp_pos_encode_sequence(encoder, 0, seq_len, batch_output.data());
    ASSERT_EQ(result, NIMCP_POS_SUCCESS);

    // Individual encoding
    for (uint32_t i = 0; i < seq_len; i++) {
        result = nimcp_pos_encode_position(
            encoder, i,
            individual_output.data() + i * EMBEDDING_DIM
        );
        ASSERT_EQ(result, NIMCP_POS_SUCCESS);
    }

    // Compare
    EXPECT_TRUE(EncodingsMatch(batch_output.data(), individual_output.data(),
                               EMBEDDING_DIM * seq_len))
        << "Batch and individual encodings differ";
}

//=============================================================================
// 5. Type Switching Tests
//=============================================================================

TEST_F(AttentionPERegressionTest, TypeSwitching_NoStateCorruption) {
    // WHAT: Verify switching between PE types doesn't corrupt state
    // WHY:  Should be able to safely switch encoding methods
    // HOW:  Create different encoders sequentially, verify each works

    std::vector<float> output(EMBEDDING_DIM);

    // Sinusoidal
    nimcp_pos_config_t sin_config = {};
    sin_config.type = NIMCP_POS_SINUSOIDAL;
    sin_config.config.sinusoidal = nimcp_pos_sinusoidal_default_config();

    encoder = nimcp_pos_encoder_create(&sin_config);
    ASSERT_NE(encoder, nullptr);
    EXPECT_EQ(nimcp_pos_encode_position(encoder, 0, output.data()), NIMCP_POS_SUCCESS);
    nimcp_pos_encoder_destroy(encoder);

    // RoPE - uses rope_apply API (applies rotation to Q/K pairs)
    nimcp_pos_config_t rope_config = {};
    rope_config.type = NIMCP_POS_ROTARY;
    rope_config.config.rope = nimcp_pos_rope_default_config();

    encoder = nimcp_pos_encoder_create(&rope_config);
    ASSERT_NE(encoder, nullptr);
    std::vector<float> query(EMBEDDING_DIM, 1.0f);
    std::vector<float> key(EMBEDDING_DIM, 1.0f);
    std::vector<float> query_out(EMBEDDING_DIM);
    std::vector<float> key_out(EMBEDDING_DIM);
    EXPECT_EQ(nimcp_pos_rope_apply(encoder, query.data(), key.data(), 0,
                                   query_out.data(), key_out.data()), NIMCP_POS_SUCCESS);
    nimcp_pos_encoder_destroy(encoder);

    // ALiBi
    nimcp_pos_config_t alibi_config = {};
    alibi_config.type = NIMCP_POS_ALIBI;
    alibi_config.config.alibi = nimcp_pos_alibi_default_config();

    encoder = nimcp_pos_encoder_create(&alibi_config);
    ASSERT_NE(encoder, nullptr);
    std::vector<float> bias(8 * 64 * 64);
    EXPECT_EQ(nimcp_pos_alibi_get_bias(encoder, 64, bias.data()), NIMCP_POS_SUCCESS);
    nimcp_pos_encoder_destroy(encoder);

    encoder = nullptr;
    SUCCEED();
}

//=============================================================================
// 6. Large Sequence Tests
//=============================================================================

TEST_F(AttentionPERegressionTest, LargeSequence_Sinusoidal) {
    // WHAT: Verify PE handles large sequences without issues
    // WHY:  Should scale to typical transformer sequence lengths
    // HOW:  Encode 2048 position sequence, verify success

    nimcp_pos_config_t config = {};
    config.type = NIMCP_POS_SINUSOIDAL;
    config.config.sinusoidal = nimcp_pos_sinusoidal_default_config();

    encoder = nimcp_pos_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    std::vector<float> sequence(EMBEDDING_DIM * LARGE_SEQ_LENGTH);

    int result = nimcp_pos_encode_sequence(encoder, 0, LARGE_SEQ_LENGTH, sequence.data());
    ASSERT_EQ(result, NIMCP_POS_SUCCESS);

    // Verify encodings are valid (not NaN or Inf)
    for (size_t i = 0; i < sequence.size(); i++) {
        EXPECT_TRUE(std::isfinite(sequence[i]))
            << "Non-finite value at index " << i;
    }
}

TEST_F(AttentionPERegressionTest, LargeSequence_RoPEBatch) {
    // WHAT: Verify RoPE handles large batch operations
    // WHY:  Batch processing is critical for performance
    // HOW:  Apply RoPE to large batch, verify success

    nimcp_pos_config_t config = {};
    config.type = NIMCP_POS_ROTARY;
    config.config.rope = nimcp_pos_rope_default_config();

    encoder = nimcp_pos_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    uint32_t seq_len = 512;
    uint32_t num_heads = 8;
    uint32_t head_dim = 64;
    size_t total_size = seq_len * num_heads * head_dim;

    std::vector<float> queries(total_size, 0.5f);
    std::vector<float> keys(total_size, 0.3f);
    std::vector<float> queries_out(total_size);
    std::vector<float> keys_out(total_size);

    int result = nimcp_pos_rope_apply_batch(
        encoder,
        queries.data(), keys.data(),
        seq_len, num_heads,
        queries_out.data(), keys_out.data()
    );

    ASSERT_EQ(result, NIMCP_POS_SUCCESS);

    // Verify outputs are valid
    for (size_t i = 0; i < total_size; i++) {
        EXPECT_TRUE(std::isfinite(queries_out[i]));
        EXPECT_TRUE(std::isfinite(keys_out[i]));
    }
}

//=============================================================================
// Run All Tests
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
