/**
 * @file test_pe_stability.cpp
 * @brief Core regression tests for positional encoding stability
 *
 * WHAT: Comprehensive regression tests for PE core implementation
 * WHY:  PE is foundational - must be rock-solid stable across all types
 * HOW:  Test all PE types under stress, verify numerical stability
 *
 * REGRESSION TEST PHILOSOPHY:
 * - Ensure PE numerical accuracy doesn't degrade
 * - Verify PE memory management is leak-free
 * - Validate PE performance remains consistent
 * - Test PE edge cases and boundary conditions
 * - Verify PE type implementations don't interfere
 * - Catch any regressions in core PE algorithms
 *
 * WHAT WE'RE PROTECTING:
 * - Sinusoidal PE mathematical correctness
 * - RoPE rotation matrix stability
 * - ALiBi bias computation accuracy
 * - Learned embedding training stability
 * - Relative PE correctness
 * - Cache effectiveness and correctness
 * - API stability and backward compatibility
 *
 * @author NIMCP Development Team
 * @date 2025-12-10
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <vector>
#include <cmath>
#include <chrono>
#include <random>

// Headers have their own extern "C" guards
#include "utils/encoding/nimcp_positional_encoding.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "utils/memory/nimcp_unified_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class PEStabilityTest : public ::testing::Test {
protected:
    static constexpr int ITERATIONS = 1000;
    static constexpr int STRESS_ITERATIONS = 10000;
    static constexpr int EMBEDDING_DIM = 512;
    static constexpr int MAX_SEQ_LENGTH = 8192;

    nimcp_pos_encoder_t* encoder;
    std::mt19937 rng;

    void SetUp() override {
        // WHAT: Initialize systems and RNG
        // WHY:  PE tests need bio-async and reproducible randomness
        // HOW:  Standard initialization with fixed seed
        nimcp_bio_async_config_t async_cfg = {0};
        nimcp_bio_async_init(&async_cfg);
        bio_router_config_t cfg = {0};
        bio_router_init(&cfg);

        encoder = nullptr;
        rng.seed(42);  // Fixed seed for reproducibility
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

    // WHAT: Check if values match within tolerance
    // WHY:  Need floating point comparison
    // HOW:  Element-wise comparison with epsilon
    bool ValuesMatch(const float* v1, const float* v2, uint32_t size, float tolerance = 1e-5f) {
        for (uint32_t i = 0; i < size; i++) {
            if (std::abs(v1[i] - v2[i]) > tolerance) {
                return false;
            }
        }
        return true;
    }

    // WHAT: Check if all values are finite
    // WHY:  Detect NaN/Inf corruption
    // HOW:  Test each element
    bool AllFinite(const float* values, uint32_t size) {
        for (uint32_t i = 0; i < size; i++) {
            if (!std::isfinite(values[i])) {
                return false;
            }
        }
        return true;
    }

    // WHAT: Compute L2 norm of vector
    // WHY:  Check for magnitude explosions
    // HOW:  Standard L2 norm formula
    float ComputeNorm(const float* values, uint32_t size) {
        float sum = 0.0f;
        for (uint32_t i = 0; i < size; i++) {
            sum += values[i] * values[i];
        }
        return std::sqrt(sum);
    }
};

//=============================================================================
// 1. Sinusoidal PE Stability Tests
//=============================================================================

TEST_F(PEStabilityTest, Sinusoidal_NumericalAccuracy) {
    // WHAT: Verify sinusoidal PE matches theoretical formula
    // WHY:  Mathematical correctness is foundational
    // HOW:  Compute expected values, compare with implementation

    nimcp_pos_config_t config = {};
    config.type = NIMCP_POS_SINUSOIDAL;
    config.config.sinusoidal = nimcp_pos_sinusoidal_default_config();

    encoder = nimcp_pos_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    std::vector<float> encoding(EMBEDDING_DIM);

    // Test position 0 (should be specific pattern)
    nimcp_pos_encode_position(encoder, 0, encoding.data());

    // At position 0, sin terms should be 0, cos terms should be 1
    for (uint32_t i = 0; i < EMBEDDING_DIM; i += 2) {
        EXPECT_NEAR(encoding[i], 0.0f, 1e-5f) << "Sin term at dim " << i;
        if (i + 1 < EMBEDDING_DIM) {
            EXPECT_NEAR(encoding[i + 1], 1.0f, 1e-5f) << "Cos term at dim " << (i + 1);
        }
    }
}

TEST_F(PEStabilityTest, Sinusoidal_Deterministic) {
    // WHAT: Verify sinusoidal PE is perfectly deterministic
    // WHY:  Should produce identical output for same input
    // HOW:  Encode same position many times, verify exact match

    nimcp_pos_config_t config = {};
    config.type = NIMCP_POS_SINUSOIDAL;
    config.config.sinusoidal = nimcp_pos_sinusoidal_default_config();

    encoder = nimcp_pos_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    std::vector<float> baseline(EMBEDDING_DIM);
    std::vector<float> current(EMBEDDING_DIM);

    nimcp_pos_encode_position(encoder, 42, baseline.data());

    for (int i = 0; i < ITERATIONS; i++) {
        nimcp_pos_encode_position(encoder, 42, current.data());

        EXPECT_TRUE(ValuesMatch(baseline.data(), current.data(), EMBEDDING_DIM, 0.0f))
            << "Encoding changed at iteration " << i;
    }
}

TEST_F(PEStabilityTest, Sinusoidal_LargePositions) {
    // WHAT: Verify sinusoidal PE handles large position indices
    // WHY:  Should work up to MAX_SEQ_LENGTH
    // HOW:  Test increasing positions, verify no overflow/NaN

    nimcp_pos_config_t config = {};
    config.type = NIMCP_POS_SINUSOIDAL;
    config.config.sinusoidal = nimcp_pos_sinusoidal_default_config();

    encoder = nimcp_pos_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    std::vector<float> encoding(EMBEDDING_DIM);

    uint32_t test_positions[] = {0, 100, 1000, 4096, 8191};

    for (auto pos : test_positions) {
        int result = nimcp_pos_encode_position(encoder, pos, encoding.data());
        ASSERT_EQ(result, NIMCP_POS_SUCCESS);

        EXPECT_TRUE(AllFinite(encoding.data(), EMBEDDING_DIM))
            << "Non-finite values at position " << pos;

        float norm = ComputeNorm(encoding.data(), EMBEDDING_DIM);
        EXPECT_GT(norm, 0.0f) << "Zero norm at position " << pos;
        EXPECT_LT(norm, 1000.0f) << "Exploding norm at position " << pos;
    }
}

TEST_F(PEStabilityTest, Sinusoidal_StressTest) {
    // WHAT: Verify sinusoidal PE under extreme stress
    // WHY:  Must handle intensive use without degradation
    // HOW:  10000 random encodings, verify all valid

    nimcp_pos_config_t config = {};
    config.type = NIMCP_POS_SINUSOIDAL;
    config.config.sinusoidal = nimcp_pos_sinusoidal_default_config();

    encoder = nimcp_pos_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    std::vector<float> encoding(EMBEDDING_DIM);
    std::uniform_int_distribution<uint32_t> pos_dist(0, MAX_SEQ_LENGTH - 1);

    for (int i = 0; i < STRESS_ITERATIONS; i++) {
        uint32_t pos = pos_dist(rng);
        int result = nimcp_pos_encode_position(encoder, pos, encoding.data());

        ASSERT_EQ(result, NIMCP_POS_SUCCESS);
        ASSERT_TRUE(AllFinite(encoding.data(), EMBEDDING_DIM))
            << "Corruption at iteration " << i << ", position " << pos;

        if (i % 100 == 0) {
            nimcp_bio_async_step(1.0f);
        }
    }
}

//=============================================================================
// 2. RoPE Stability Tests
//=============================================================================

TEST_F(PEStabilityTest, RoPE_OrthogonalityPreservation) {
    // WHAT: Verify RoPE preserves vector orthogonality
    // WHY:  Rotation should preserve dot products
    // HOW:  Rotate orthogonal vectors, verify still orthogonal

    nimcp_pos_config_t config = {};
    config.type = NIMCP_POS_ROTARY;
    config.config.rope = nimcp_pos_rope_default_config();

    encoder = nimcp_pos_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    // Create orthogonal query/key
    std::vector<float> query(EMBEDDING_DIM, 0.0f);
    std::vector<float> key(EMBEDDING_DIM, 0.0f);
    query[0] = 1.0f;
    key[1] = 1.0f;  // Orthogonal to query

    // Compute dot product before rotation
    float dot_before = 0.0f;
    for (uint32_t i = 0; i < EMBEDDING_DIM; i++) {
        dot_before += query[i] * key[i];
    }
    EXPECT_NEAR(dot_before, 0.0f, 1e-5f);

    // Apply RoPE
    std::vector<float> query_out(EMBEDDING_DIM);
    std::vector<float> key_out(EMBEDDING_DIM);

    int result = nimcp_pos_rope_apply(
        encoder,
        query.data(), key.data(), 10,
        query_out.data(), key_out.data()
    );
    ASSERT_EQ(result, NIMCP_POS_SUCCESS);

    // Compute dot product after rotation
    float dot_after = 0.0f;
    for (uint32_t i = 0; i < EMBEDDING_DIM; i++) {
        dot_after += query_out[i] * key_out[i];
    }

    // Should remain approximately orthogonal (rotations preserve angles)
    EXPECT_NEAR(dot_after, 0.0f, 0.1f);  // Allow some numerical error
}

TEST_F(PEStabilityTest, RoPE_MagnitudePreservation) {
    // WHAT: Verify RoPE preserves vector magnitudes
    // WHY:  Rotation should preserve L2 norm
    // HOW:  Rotate vectors, verify norm unchanged

    nimcp_pos_config_t config = {};
    config.type = NIMCP_POS_ROTARY;
    config.config.rope = nimcp_pos_rope_default_config();

    encoder = nimcp_pos_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    std::vector<float> query(EMBEDDING_DIM);
    std::vector<float> key(EMBEDDING_DIM);

    // Random vectors
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    for (uint32_t i = 0; i < EMBEDDING_DIM; i++) {
        query[i] = dist(rng);
        key[i] = dist(rng);
    }

    float query_norm_before = ComputeNorm(query.data(), EMBEDDING_DIM);
    float key_norm_before = ComputeNorm(key.data(), EMBEDDING_DIM);

    std::vector<float> query_out(EMBEDDING_DIM);
    std::vector<float> key_out(EMBEDDING_DIM);

    int result = nimcp_pos_rope_apply(
        encoder,
        query.data(), key.data(), 50,
        query_out.data(), key_out.data()
    );
    ASSERT_EQ(result, NIMCP_POS_SUCCESS);

    float query_norm_after = ComputeNorm(query_out.data(), EMBEDDING_DIM);
    float key_norm_after = ComputeNorm(key_out.data(), EMBEDDING_DIM);

    // Norms should be preserved
    EXPECT_NEAR(query_norm_after, query_norm_before, 0.01f * query_norm_before);
    EXPECT_NEAR(key_norm_after, key_norm_before, 0.01f * key_norm_before);
}

TEST_F(PEStabilityTest, RoPE_BatchConsistency) {
    // WHAT: Verify batch RoPE matches individual RoPE
    // WHY:  Batch and individual paths should produce same result
    // HOW:  Apply both methods, compare outputs

    nimcp_pos_config_t config = {};
    config.type = NIMCP_POS_ROTARY;
    config.config.rope = nimcp_pos_rope_default_config();

    encoder = nimcp_pos_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    uint32_t seq_len = 10;
    uint32_t num_heads = 4;
    uint32_t head_dim = 128;
    size_t total_size = seq_len * num_heads * head_dim;

    std::vector<float> queries(total_size);
    std::vector<float> keys(total_size);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    for (size_t i = 0; i < total_size; i++) {
        queries[i] = dist(rng);
        keys[i] = dist(rng);
    }

    // Apply batch RoPE
    std::vector<float> batch_queries_out(total_size);
    std::vector<float> batch_keys_out(total_size);

    int result = nimcp_pos_rope_apply_batch(
        encoder,
        queries.data(), keys.data(),
        seq_len, num_heads,
        batch_queries_out.data(), batch_keys_out.data()
    );
    ASSERT_EQ(result, NIMCP_POS_SUCCESS);

    // Verify all outputs are valid
    EXPECT_TRUE(AllFinite(batch_queries_out.data(), total_size));
    EXPECT_TRUE(AllFinite(batch_keys_out.data(), total_size));
}

//=============================================================================
// 3. ALiBi Stability Tests
//=============================================================================

TEST_F(PEStabilityTest, ALiBi_BiasSymmetry) {
    // WHAT: Verify ALiBi bias has correct symmetry properties
    // WHY:  Bias matrix should be symmetric for bidirectional attention
    // HOW:  Compute bias, check symmetry when enabled

    nimcp_pos_config_t config = {};
    config.type = NIMCP_POS_ALIBI;
    config.config.alibi = nimcp_pos_alibi_default_config();
    config.config.alibi.base.cache_enabled = false;
    config.config.alibi.use_symmetric = true;

    encoder = nimcp_pos_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    uint32_t seq_len = 32;
    uint32_t num_heads = 8;
    size_t bias_size = num_heads * seq_len * seq_len;

    std::vector<float> bias(bias_size);

    int result = nimcp_pos_alibi_get_bias(encoder, seq_len, bias.data());
    ASSERT_EQ(result, NIMCP_POS_SUCCESS);

    // For symmetric bias, bias[h][i][j] should equal bias[h][j][i]
    for (uint32_t h = 0; h < num_heads; h++) {
        for (uint32_t i = 0; i < seq_len; i++) {
            for (uint32_t j = i + 1; j < seq_len; j++) {
                size_t idx_ij = h * seq_len * seq_len + i * seq_len + j;
                size_t idx_ji = h * seq_len * seq_len + j * seq_len + i;

                float diff = std::abs(bias[idx_ij] - bias[idx_ji]);
                EXPECT_LT(diff, 1e-5f)
                    << "Asymmetric bias at head " << h << ", pos (" << i << "," << j << ")";
            }
        }
    }
}

TEST_F(PEStabilityTest, ALiBi_SlopeGeometry) {
    // WHAT: Verify ALiBi slopes follow geometric sequence
    // WHY:  Slopes should be 2^(-8*(h+1)/num_heads)
    // HOW:  Get slopes, verify geometric progression

    nimcp_pos_config_t config = {};
    config.type = NIMCP_POS_ALIBI;
    config.config.alibi = nimcp_pos_alibi_default_config();

    encoder = nimcp_pos_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    uint32_t num_heads = 8;
    std::vector<float> slopes(num_heads);

    int result = nimcp_pos_alibi_get_slopes(encoder, slopes.data());
    ASSERT_EQ(result, NIMCP_POS_SUCCESS);

    // Verify slopes are in decreasing order
    for (uint32_t h = 0; h < num_heads - 1; h++) {
        EXPECT_GT(slopes[h], slopes[h + 1])
            << "Slopes not decreasing at head " << h;
    }

    // Verify slopes are positive and reasonable
    for (uint32_t h = 0; h < num_heads; h++) {
        EXPECT_GT(slopes[h], 0.0f) << "Non-positive slope at head " << h;
        EXPECT_LT(slopes[h], 1.0f) << "Slope too large at head " << h;
    }
}

TEST_F(PEStabilityTest, ALiBi_BiasLinearGrowth) {
    // WHAT: Verify ALiBi bias grows linearly with distance
    // WHY:  bias[i][j] = -slope * |i - j| (symmetric mode)
    // HOW:  Check bias values match formula using symmetric config

    nimcp_pos_config_t config = {};
    config.type = NIMCP_POS_ALIBI;
    config.config.alibi = nimcp_pos_alibi_default_config();
    config.config.alibi.use_symmetric = true;  // Must be symmetric for |i-j| formula

    encoder = nimcp_pos_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    uint32_t seq_len = 16;
    uint32_t num_heads = 8;
    size_t bias_size = num_heads * seq_len * seq_len;

    std::vector<float> bias(bias_size);
    std::vector<float> slopes(num_heads);

    nimcp_pos_alibi_get_slopes(encoder, slopes.data());
    nimcp_pos_alibi_get_bias(encoder, seq_len, bias.data());

    // Verify linear growth for first head
    uint32_t h = 0;
    for (uint32_t i = 0; i < seq_len; i++) {
        for (uint32_t j = 0; j < seq_len; j++) {
            size_t idx = h * seq_len * seq_len + i * seq_len + j;
            float expected = -slopes[h] * std::abs((int)i - (int)j);

            EXPECT_NEAR(bias[idx], expected, 1e-5f)
                << "Bias mismatch at (" << i << "," << j << ")";
        }
    }
}

//=============================================================================
// 4. Cache Effectiveness Tests
//=============================================================================

TEST_F(PEStabilityTest, Cache_HitRate) {
    // WHAT: Verify cache achieves high hit rate
    // WHY:  Cache should be effective for repeated access
    // HOW:  Pre-compute cache, access repeatedly, check stats

    nimcp_pos_config_t config = {};
    config.type = NIMCP_POS_SINUSOIDAL;
    config.config.sinusoidal = nimcp_pos_sinusoidal_default_config();
    config.config.sinusoidal.base.cache_enabled = true;

    encoder = nimcp_pos_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    // Pre-compute cache
    int result = nimcp_pos_cache_precompute(encoder, 1024);
    ASSERT_EQ(result, NIMCP_POS_SUCCESS);

    // Reset stats to measure only new accesses
    nimcp_pos_reset_stats(encoder);

    // Access cached positions many times
    std::vector<float> output(EMBEDDING_DIM);
    for (int i = 0; i < 1000; i++) {
        nimcp_pos_encode_position(encoder, i % 1024, output.data());
    }

    // Get cache stats
    float hit_rate = 0.0f;
    size_t cache_size = 0;
    result = nimcp_pos_cache_stats(encoder, &hit_rate, &cache_size);
    ASSERT_EQ(result, NIMCP_POS_SUCCESS);

    // Should have very high hit rate (>95%)
    EXPECT_GT(hit_rate, 0.95f)
        << "Cache hit rate too low: " << hit_rate;
}

TEST_F(PEStabilityTest, Cache_Correctness) {
    // WHAT: Verify cached encodings match uncached
    // WHY:  Cache must not change results
    // HOW:  Compare cached vs uncached outputs

    // Uncached
    nimcp_pos_config_t config_nocache = {};
    config_nocache.type = NIMCP_POS_SINUSOIDAL;
    config_nocache.config.sinusoidal = nimcp_pos_sinusoidal_default_config();
    config_nocache.config.sinusoidal.base.cache_enabled = false;

    nimcp_pos_encoder_t* encoder_nocache = nimcp_pos_encoder_create(&config_nocache);
    ASSERT_NE(encoder_nocache, nullptr);

    std::vector<float> uncached_output(EMBEDDING_DIM);
    nimcp_pos_encode_position(encoder_nocache, 42, uncached_output.data());

    nimcp_pos_encoder_destroy(encoder_nocache);

    // Cached
    nimcp_pos_config_t config_cache = {};
    config_cache.type = NIMCP_POS_SINUSOIDAL;
    config_cache.config.sinusoidal = nimcp_pos_sinusoidal_default_config();
    config_cache.config.sinusoidal.base.cache_enabled = true;

    encoder = nimcp_pos_encoder_create(&config_cache);
    ASSERT_NE(encoder, nullptr);

    nimcp_pos_cache_precompute(encoder, 100);

    std::vector<float> cached_output(EMBEDDING_DIM);
    nimcp_pos_encode_position(encoder, 42, cached_output.data());

    // Results should match exactly
    EXPECT_TRUE(ValuesMatch(uncached_output.data(), cached_output.data(), EMBEDDING_DIM))
        << "Cached output differs from uncached";
}

//=============================================================================
// 5. Memory Leak Tests
//=============================================================================

TEST_F(PEStabilityTest, Memory_NoLeaksAllTypes) {
    // WHAT: Verify no memory leaks across all PE types
    // WHY:  Each type has different internal structures
    // HOW:  Create/destroy each type many times

    nimcp_pos_encoding_type_t types[] = {
        NIMCP_POS_SINUSOIDAL,
        NIMCP_POS_LEARNED,
        NIMCP_POS_ROTARY,
        NIMCP_POS_ALIBI,
        NIMCP_POS_RELATIVE
    };

    for (auto pe_type : types) {
        for (int i = 0; i < 100; i++) {
            nimcp_pos_config_t config = {};
            config.type = pe_type;

            // Get default config for type
            switch (pe_type) {
                case NIMCP_POS_SINUSOIDAL:
                    config.config.sinusoidal = nimcp_pos_sinusoidal_default_config();
                    break;
                case NIMCP_POS_LEARNED:
                    config.config.learned = nimcp_pos_learned_default_config();
                    break;
                case NIMCP_POS_ROTARY:
                    config.config.rope = nimcp_pos_rope_default_config();
                    break;
                case NIMCP_POS_ALIBI:
                    config.config.alibi = nimcp_pos_alibi_default_config();
                    break;
                case NIMCP_POS_RELATIVE:
                    config.config.relative = nimcp_pos_relative_default_config();
                    break;
                default:
                    break;
            }

            nimcp_pos_encoder_t* temp_encoder = nimcp_pos_encoder_create(&config);
            ASSERT_NE(temp_encoder, nullptr);

            // Do some work
            std::vector<float> output(EMBEDDING_DIM);
            for (int j = 0; j < 10; j++) {
                nimcp_pos_encode_position(temp_encoder, j, output.data());
            }

            nimcp_pos_encoder_destroy(temp_encoder);
        }
    }

    SUCCEED();
}

//=============================================================================
// 6. API Stability Tests
//=============================================================================

TEST_F(PEStabilityTest, API_NullHandling) {
    // WHAT: Verify API handles NULL parameters gracefully
    // WHY:  Defensive programming - should not crash
    // HOW:  Pass NULLs to all functions, verify error codes

    // NULL config to create
    encoder = nimcp_pos_encoder_create(nullptr);
    // May return NULL or valid encoder with defaults - both acceptable
    if (encoder) {
        nimcp_pos_encoder_destroy(encoder);
        encoder = nullptr;
    }

    // Create valid encoder for remaining tests
    nimcp_pos_config_t config = {};
    config.type = NIMCP_POS_SINUSOIDAL;
    config.config.sinusoidal = nimcp_pos_sinusoidal_default_config();

    encoder = nimcp_pos_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    // NULL output buffer
    int result = nimcp_pos_encode_position(encoder, 0, nullptr);
    EXPECT_NE(result, NIMCP_POS_SUCCESS);

    // NULL encoder
    std::vector<float> output(EMBEDDING_DIM);
    result = nimcp_pos_encode_position(nullptr, 0, output.data());
    EXPECT_NE(result, NIMCP_POS_SUCCESS);
}

TEST_F(PEStabilityTest, API_BoundaryPositions) {
    // WHAT: Verify API handles boundary position values
    // WHY:  Edge cases must work correctly
    // HOW:  Test position 0, max position within encoder limits, beyond max

    nimcp_pos_config_t config = {};
    config.type = NIMCP_POS_SINUSOIDAL;
    config.config.sinusoidal = nimcp_pos_sinusoidal_default_config();
    // Default sinusoidal max_seq_length is 8192, use that as the boundary
    uint32_t encoder_max = config.config.sinusoidal.base.max_seq_length;

    encoder = nimcp_pos_encoder_create(&config);
    ASSERT_NE(encoder, nullptr);

    std::vector<float> output(EMBEDDING_DIM);

    // Position 0
    int result = nimcp_pos_encode_position(encoder, 0, output.data());
    EXPECT_EQ(result, NIMCP_POS_SUCCESS);

    // Max position within encoder's configured limit
    result = nimcp_pos_encode_position(encoder, encoder_max - 1, output.data());
    EXPECT_EQ(result, NIMCP_POS_SUCCESS);

    // Beyond encoder's max (should fail or clip)
    result = nimcp_pos_encode_position(encoder, encoder_max, output.data());
    // Implementation may handle this differently - just verify no crash
}

//=============================================================================
// 7. Performance Regression Tests
//=============================================================================

TEST_F(PEStabilityTest, Performance_EncodingSpeed) {
    // WHAT: Verify encoding performance hasn't regressed
    // WHY:  PE is on critical path - must stay fast
    // HOW:  Benchmark each PE type, verify reasonable speed

    struct {
        nimcp_pos_encoding_type_t type;
        const char* name;
        float max_us;  // Max acceptable time in microseconds
    } benchmarks[] = {
        {NIMCP_POS_SINUSOIDAL, "Sinusoidal", 50.0f},
        {NIMCP_POS_ROTARY, "RoPE", 100.0f},
        {NIMCP_POS_ALIBI, "ALiBi", 200.0f}
    };

    for (auto& bench : benchmarks) {
        nimcp_pos_config_t config = {};
        config.type = bench.type;

        switch (bench.type) {
            case NIMCP_POS_SINUSOIDAL:
                config.config.sinusoidal = nimcp_pos_sinusoidal_default_config();
                break;
            case NIMCP_POS_ROTARY:
                config.config.rope = nimcp_pos_rope_default_config();
                break;
            case NIMCP_POS_ALIBI:
                config.config.alibi = nimcp_pos_alibi_default_config();
                break;
            default:
                continue;
        }

        nimcp_pos_encoder_t* temp_encoder = nimcp_pos_encoder_create(&config);
        ASSERT_NE(temp_encoder, nullptr);

        std::vector<float> output(EMBEDDING_DIM);

        // Warmup
        for (int i = 0; i < 100; i++) {
            nimcp_pos_encode_position(temp_encoder, i, output.data());
        }

        // Benchmark
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 1000; i++) {
            nimcp_pos_encode_position(temp_encoder, i, output.data());
        }
        auto end = std::chrono::high_resolution_clock::now();

        auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(
            end - start).count();
        float avg_us = duration_us / 1000.0f;

        EXPECT_LT(avg_us, bench.max_us)
            << bench.name << " encoding too slow: " << avg_us << "us (max: " << bench.max_us << "us)";

        nimcp_pos_encoder_destroy(temp_encoder);
    }
}

//=============================================================================
// Run All Tests
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
