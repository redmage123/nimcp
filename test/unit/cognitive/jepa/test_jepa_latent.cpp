/**
 * @file test_jepa_latent.cpp
 * @brief Comprehensive unit tests for JEPA Latent Space Representation Module
 *
 * WHAT: Tests for JEPA latent embedding operations
 * WHY:  Latent space is core to JEPA - must be correct for prediction/comparison
 * HOW:  Unit tests for creation, similarity, interpolation, normalization, pooling
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

// Headers have their own extern "C" guards
#include "cognitive/jepa/nimcp_jepa_latent.h"
#include "utils/error/nimcp_error_codes.h"

// Tolerance for floating point comparisons
static constexpr float FLOAT_TOLERANCE = 1e-5f;

// Helper to compare floats
static bool float_equals(float a, float b, float tol = FLOAT_TOLERANCE)
{
    return std::fabs(a - b) < tol;
}

// =============================================================================
// Test Fixture
// =============================================================================

class JepaLatentTest : public ::testing::Test {
   protected:
    void SetUp() override
    {
        // Reset stats at start of each test
        jepa_latent_reset_stats();
    }

    void TearDown() override
    {
        // Nothing to clean up - tests manage their own latents
    }

    // Helper to create latent with test values
    jepa_latent_t* create_test_latent(uint32_t dim, float fill_value)
    {
        jepa_latent_t* latent = jepa_latent_create_dim(dim);
        if (latent && latent->embedding) {
            for (uint32_t i = 0; i < dim; i++) {
                latent->embedding[i] = fill_value + (float)i * 0.01f;
            }
        }
        return latent;
    }

    // Helper to create normalized latent (unit vector)
    jepa_latent_t* create_unit_latent(uint32_t dim, float* values)
    {
        jepa_latent_t* latent = jepa_latent_create_dim(dim);
        if (latent && latent->embedding && values) {
            float norm = 0.0f;
            for (uint32_t i = 0; i < dim; i++) {
                norm += values[i] * values[i];
            }
            norm = std::sqrt(norm);
            if (norm > 0.0f) {
                for (uint32_t i = 0; i < dim; i++) {
                    latent->embedding[i] = values[i] / norm;
                }
            }
            latent->is_normalized = true;
            latent->norm_type = JEPA_NORM_L2;
        }
        return latent;
    }
};

// =============================================================================
// Configuration Tests
// =============================================================================

TEST_F(JepaLatentTest, DefaultConfigSetsReasonableValues)
{
    jepa_latent_config_t config;
    int result = jepa_latent_default_config(&config);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GE(config.latent_dim, JEPA_LATENT_MIN_DIM);
    EXPECT_LE(config.latent_dim, JEPA_LATENT_MAX_DIM);
    EXPECT_GT(config.initial_precision, 0.0f);
}

TEST_F(JepaLatentTest, DefaultConfigNullPointer)
{
    int result = jepa_latent_default_config(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// Creation/Destruction Tests
// =============================================================================

TEST_F(JepaLatentTest, CreateWithDefaultConfig)
{
    jepa_latent_t* latent = jepa_latent_create(nullptr);

    ASSERT_NE(latent, nullptr);
    EXPECT_NE(latent->embedding, nullptr);
    EXPECT_GE(latent->latent_dim, JEPA_LATENT_MIN_DIM);
    EXPECT_EQ(latent->ref_count, 1u);

    jepa_latent_destroy(latent);
}

TEST_F(JepaLatentTest, CreateWithCustomConfig)
{
    jepa_latent_config_t config;
    jepa_latent_default_config(&config);
    config.latent_dim = 128;
    config.enable_variance = true;
    config.modality = JEPA_MODALITY_VISUAL;
    config.initial_precision = 2.0f;

    jepa_latent_t* latent = jepa_latent_create(&config);

    ASSERT_NE(latent, nullptr);
    EXPECT_EQ(latent->latent_dim, 128u);
    EXPECT_NE(latent->variance, nullptr);  // Variance enabled
    EXPECT_EQ(latent->modality, JEPA_MODALITY_VISUAL);
    EXPECT_FLOAT_EQ(latent->precision, 2.0f);

    jepa_latent_destroy(latent);
}

TEST_F(JepaLatentTest, CreateDimBasic)
{
    jepa_latent_t* latent = jepa_latent_create_dim(64);

    ASSERT_NE(latent, nullptr);
    EXPECT_EQ(latent->latent_dim, 64u);
    EXPECT_NE(latent->embedding, nullptr);

    jepa_latent_destroy(latent);
}

TEST_F(JepaLatentTest, CreateDimMinimum)
{
    jepa_latent_t* latent = jepa_latent_create_dim(JEPA_LATENT_MIN_DIM);

    ASSERT_NE(latent, nullptr);
    EXPECT_EQ(latent->latent_dim, JEPA_LATENT_MIN_DIM);

    jepa_latent_destroy(latent);
}

TEST_F(JepaLatentTest, CreateDimMaximum)
{
    jepa_latent_t* latent = jepa_latent_create_dim(JEPA_LATENT_MAX_DIM);

    ASSERT_NE(latent, nullptr);
    EXPECT_EQ(latent->latent_dim, JEPA_LATENT_MAX_DIM);

    jepa_latent_destroy(latent);
}

TEST_F(JepaLatentTest, CreateDimZeroFails)
{
    jepa_latent_t* latent = jepa_latent_create_dim(0);
    EXPECT_EQ(latent, nullptr);
}

TEST_F(JepaLatentTest, CreateDimBelowMinimumFails)
{
    jepa_latent_t* latent = jepa_latent_create_dim(JEPA_LATENT_MIN_DIM - 1);
    EXPECT_EQ(latent, nullptr);
}

TEST_F(JepaLatentTest, CreateDimAboveMaximumFails)
{
    jepa_latent_t* latent = jepa_latent_create_dim(JEPA_LATENT_MAX_DIM + 1);
    EXPECT_EQ(latent, nullptr);
}

TEST_F(JepaLatentTest, DestroyNullSafe)
{
    // Should not crash
    jepa_latent_destroy(nullptr);
}

TEST_F(JepaLatentTest, CloneCreatesDeepCopy)
{
    jepa_latent_t* original = create_test_latent(32, 1.0f);
    ASSERT_NE(original, nullptr);

    jepa_latent_t* clone = jepa_latent_clone(original);
    ASSERT_NE(clone, nullptr);

    // Should have same dimensions
    EXPECT_EQ(clone->latent_dim, original->latent_dim);

    // Should have same values
    for (uint32_t i = 0; i < original->latent_dim; i++) {
        EXPECT_FLOAT_EQ(clone->embedding[i], original->embedding[i]);
    }

    // But different memory
    EXPECT_NE(clone->embedding, original->embedding);

    // Modifying clone should not affect original
    clone->embedding[0] = 999.0f;
    EXPECT_NE(original->embedding[0], 999.0f);

    jepa_latent_destroy(original);
    jepa_latent_destroy(clone);
}

TEST_F(JepaLatentTest, CloneNullReturnsNull)
{
    jepa_latent_t* clone = jepa_latent_clone(nullptr);
    EXPECT_EQ(clone, nullptr);
}

TEST_F(JepaLatentTest, ResetClearsEmbedding)
{
    jepa_latent_t* latent = create_test_latent(32, 5.0f);
    ASSERT_NE(latent, nullptr);

    // Verify non-zero values
    bool has_nonzero = false;
    for (uint32_t i = 0; i < latent->latent_dim; i++) {
        if (latent->embedding[i] != 0.0f) {
            has_nonzero = true;
            break;
        }
    }
    EXPECT_TRUE(has_nonzero);

    // Reset
    int result = jepa_latent_reset(latent);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // All should be zero now
    for (uint32_t i = 0; i < latent->latent_dim; i++) {
        EXPECT_FLOAT_EQ(latent->embedding[i], 0.0f);
    }

    jepa_latent_destroy(latent);
}

TEST_F(JepaLatentTest, ResetNullFails)
{
    int result = jepa_latent_reset(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// Data Access Tests
// =============================================================================

TEST_F(JepaLatentTest, SetEmbeddingCopiesValues)
{
    jepa_latent_t* latent = jepa_latent_create_dim(32);
    ASSERT_NE(latent, nullptr);

    float values[32];
    for (int i = 0; i < 32; i++) {
        values[i] = (float)i * 0.5f;
    }

    int result = jepa_latent_set_embedding(latent, values, 32);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    for (int i = 0; i < 32; i++) {
        EXPECT_FLOAT_EQ(latent->embedding[i], values[i]);
    }

    jepa_latent_destroy(latent);
}

TEST_F(JepaLatentTest, SetEmbeddingNullLatentFails)
{
    float values[32] = {0};
    int result = jepa_latent_set_embedding(nullptr, values, 32);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(JepaLatentTest, SetEmbeddingNullValuesFails)
{
    jepa_latent_t* latent = jepa_latent_create_dim(32);
    ASSERT_NE(latent, nullptr);

    int result = jepa_latent_set_embedding(latent, nullptr, 32);
    EXPECT_NE(result, NIMCP_SUCCESS);

    jepa_latent_destroy(latent);
}

TEST_F(JepaLatentTest, SetEmbeddingDimensionMismatchFails)
{
    jepa_latent_t* latent = jepa_latent_create_dim(32);
    ASSERT_NE(latent, nullptr);

    float values[64] = {0};
    int result = jepa_latent_set_embedding(latent, values, 64);
    EXPECT_NE(result, NIMCP_SUCCESS);

    jepa_latent_destroy(latent);
}

TEST_F(JepaLatentTest, GetEmbeddingCopiesValues)
{
    jepa_latent_t* latent = create_test_latent(32, 2.0f);
    ASSERT_NE(latent, nullptr);

    float values[32];
    int copied = jepa_latent_get_embedding(latent, values, 32);

    EXPECT_EQ(copied, 32);
    for (int i = 0; i < 32; i++) {
        EXPECT_FLOAT_EQ(values[i], latent->embedding[i]);
    }

    jepa_latent_destroy(latent);
}

TEST_F(JepaLatentTest, GetEmbeddingPartial)
{
    jepa_latent_t* latent = create_test_latent(32, 1.0f);
    ASSERT_NE(latent, nullptr);

    float values[16];
    int copied = jepa_latent_get_embedding(latent, values, 16);

    EXPECT_EQ(copied, 16);

    jepa_latent_destroy(latent);
}

TEST_F(JepaLatentTest, GetEmbeddingNullFails)
{
    float values[32];
    int copied = jepa_latent_get_embedding(nullptr, values, 32);
    EXPECT_EQ(copied, -1);
}

TEST_F(JepaLatentTest, SetVarianceUpdatesValues)
{
    jepa_latent_config_t config;
    jepa_latent_default_config(&config);
    config.latent_dim = 32;
    config.enable_variance = true;

    jepa_latent_t* latent = jepa_latent_create(&config);
    ASSERT_NE(latent, nullptr);
    ASSERT_NE(latent->variance, nullptr);

    float variance[32];
    for (int i = 0; i < 32; i++) {
        variance[i] = 0.1f + (float)i * 0.01f;
    }

    int result = jepa_latent_set_variance(latent, variance, 32);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    for (int i = 0; i < 32; i++) {
        EXPECT_FLOAT_EQ(latent->variance[i], variance[i]);
    }

    jepa_latent_destroy(latent);
}

TEST_F(JepaLatentTest, UpdatePrecisionFromVariance)
{
    jepa_latent_config_t config;
    jepa_latent_default_config(&config);
    config.latent_dim = 32;
    config.enable_variance = true;

    jepa_latent_t* latent = jepa_latent_create(&config);
    ASSERT_NE(latent, nullptr);

    // Set uniform variance of 0.5
    float variance[32];
    for (int i = 0; i < 32; i++) {
        variance[i] = 0.5f;
    }
    jepa_latent_set_variance(latent, variance, 32);

    int result = jepa_latent_update_precision(latent);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Precision should be 1 / mean(variance) = 1 / 0.5 = 2.0
    EXPECT_NEAR(latent->precision, 2.0f, 0.01f);

    jepa_latent_destroy(latent);
}

// =============================================================================
// Normalization Tests
// =============================================================================

TEST_F(JepaLatentTest, NormalizeCreatesUnitVector)
{
    jepa_latent_t* latent = create_test_latent(32, 3.0f);
    ASSERT_NE(latent, nullptr);

    int result = jepa_latent_normalize(latent);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Compute L2 norm
    float norm = 0.0f;
    for (uint32_t i = 0; i < latent->latent_dim; i++) {
        norm += latent->embedding[i] * latent->embedding[i];
    }
    norm = std::sqrt(norm);

    EXPECT_NEAR(norm, 1.0f, FLOAT_TOLERANCE);
    EXPECT_TRUE(latent->is_normalized);
    EXPECT_EQ(latent->norm_type, JEPA_NORM_L2);

    jepa_latent_destroy(latent);
}

TEST_F(JepaLatentTest, NormalizeNullFails)
{
    int result = jepa_latent_normalize(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(JepaLatentTest, LayerNormalizeZeroMeanUnitVar)
{
    jepa_latent_t* latent = create_test_latent(64, 5.0f);
    ASSERT_NE(latent, nullptr);

    int result = jepa_latent_layer_normalize(latent);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Compute mean
    float mean = 0.0f;
    for (uint32_t i = 0; i < latent->latent_dim; i++) {
        mean += latent->embedding[i];
    }
    mean /= latent->latent_dim;

    // Mean should be approximately 0
    EXPECT_NEAR(mean, 0.0f, FLOAT_TOLERANCE);

    // Compute variance
    float var = 0.0f;
    for (uint32_t i = 0; i < latent->latent_dim; i++) {
        float diff = latent->embedding[i] - mean;
        var += diff * diff;
    }
    var /= latent->latent_dim;

    // Variance should be approximately 1
    EXPECT_NEAR(var, 1.0f, 0.01f);

    EXPECT_EQ(latent->norm_type, JEPA_NORM_LAYERNORM);

    jepa_latent_destroy(latent);
}

TEST_F(JepaLatentTest, NormReturnsL2Norm)
{
    jepa_latent_t* latent = jepa_latent_create_dim(32);
    ASSERT_NE(latent, nullptr);

    // Set known values
    for (uint32_t i = 0; i < 32; i++) {
        latent->embedding[i] = 1.0f;
    }

    float norm = jepa_latent_norm(latent);

    // ||[1,1,...,1]|| = sqrt(32)
    EXPECT_NEAR(norm, std::sqrt(32.0f), FLOAT_TOLERANCE);

    jepa_latent_destroy(latent);
}

TEST_F(JepaLatentTest, NormNullReturnsNegative)
{
    float norm = jepa_latent_norm(nullptr);
    EXPECT_LT(norm, 0.0f);
}

// =============================================================================
// Similarity Tests
// =============================================================================

TEST_F(JepaLatentTest, CosineSimilarityIdentical)
{
    jepa_latent_t* a = create_test_latent(32, 1.0f);
    ASSERT_NE(a, nullptr);

    float sim = jepa_latent_cosine_similarity(a, a);

    // Self-similarity should be 1.0
    EXPECT_NEAR(sim, 1.0f, FLOAT_TOLERANCE);

    jepa_latent_destroy(a);
}

TEST_F(JepaLatentTest, CosineSimilarityOrthogonal)
{
    jepa_latent_t* a = jepa_latent_create_dim(32);
    jepa_latent_t* b = jepa_latent_create_dim(32);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    // Create orthogonal vectors
    // a = [1, 0, 0, 0, ...]
    // b = [0, 1, 0, 0, ...]
    memset(a->embedding, 0, 32 * sizeof(float));
    memset(b->embedding, 0, 32 * sizeof(float));
    a->embedding[0] = 1.0f;
    b->embedding[1] = 1.0f;

    float sim = jepa_latent_cosine_similarity(a, b);

    // Orthogonal vectors have 0 similarity
    EXPECT_NEAR(sim, 0.0f, FLOAT_TOLERANCE);

    jepa_latent_destroy(a);
    jepa_latent_destroy(b);
}

TEST_F(JepaLatentTest, CosineSimilarityOpposite)
{
    jepa_latent_t* a = create_test_latent(32, 1.0f);
    jepa_latent_t* b = jepa_latent_clone(a);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    // Negate b
    for (uint32_t i = 0; i < b->latent_dim; i++) {
        b->embedding[i] = -b->embedding[i];
    }

    float sim = jepa_latent_cosine_similarity(a, b);

    // Opposite vectors have -1 similarity
    EXPECT_NEAR(sim, -1.0f, FLOAT_TOLERANCE);

    jepa_latent_destroy(a);
    jepa_latent_destroy(b);
}

TEST_F(JepaLatentTest, CosineSimilarityNullReturnsNaN)
{
    jepa_latent_t* a = create_test_latent(32, 1.0f);

    float sim1 = jepa_latent_cosine_similarity(nullptr, a);
    float sim2 = jepa_latent_cosine_similarity(a, nullptr);
    float sim3 = jepa_latent_cosine_similarity(nullptr, nullptr);

    EXPECT_TRUE(std::isnan(sim1));
    EXPECT_TRUE(std::isnan(sim2));
    EXPECT_TRUE(std::isnan(sim3));

    jepa_latent_destroy(a);
}

TEST_F(JepaLatentTest, SimilarityWithDifferentMetrics)
{
    jepa_latent_t* a = create_test_latent(32, 1.0f);
    jepa_latent_t* b = create_test_latent(32, 1.5f);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    // All metrics should return valid values
    float cosine = jepa_latent_similarity(a, b, JEPA_SIM_COSINE);
    float dot = jepa_latent_similarity(a, b, JEPA_SIM_DOT_PRODUCT);
    float euclidean = jepa_latent_similarity(a, b, JEPA_SIM_EUCLIDEAN);

    EXPECT_FALSE(std::isnan(cosine));
    EXPECT_FALSE(std::isnan(dot));
    EXPECT_FALSE(std::isnan(euclidean));

    // Cosine should be in [-1, 1]
    EXPECT_GE(cosine, -1.0f);
    EXPECT_LE(cosine, 1.0f);

    // Euclidean similarity is negative distance, so <= 0
    EXPECT_LE(euclidean, 0.0f);

    jepa_latent_destroy(a);
    jepa_latent_destroy(b);
}

TEST_F(JepaLatentTest, SimilarityDimensionMismatchReturnsNaN)
{
    jepa_latent_t* a = jepa_latent_create_dim(32);
    jepa_latent_t* b = jepa_latent_create_dim(64);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    float sim = jepa_latent_similarity(a, b, JEPA_SIM_COSINE);
    EXPECT_TRUE(std::isnan(sim));

    jepa_latent_destroy(a);
    jepa_latent_destroy(b);
}

TEST_F(JepaLatentTest, PrecisionWeightedSimilarity)
{
    jepa_latent_config_t config;
    jepa_latent_default_config(&config);
    config.latent_dim = 32;
    config.enable_variance = true;

    jepa_latent_t* a = jepa_latent_create(&config);
    jepa_latent_t* b = jepa_latent_create(&config);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    // Set embedding values
    for (uint32_t i = 0; i < 32; i++) {
        a->embedding[i] = 1.0f;
        b->embedding[i] = 1.0f;
        a->variance[i] = 0.5f;
        b->variance[i] = 0.5f;
    }

    float sim = jepa_latent_precision_similarity(a, b);

    // Identical vectors with uniform precision should have high similarity
    EXPECT_FALSE(std::isnan(sim));

    jepa_latent_destroy(a);
    jepa_latent_destroy(b);
}

TEST_F(JepaLatentTest, DistanceIdenticalIsZero)
{
    jepa_latent_t* a = create_test_latent(32, 1.0f);
    ASSERT_NE(a, nullptr);

    float dist = jepa_latent_distance(a, a);
    EXPECT_NEAR(dist, 0.0f, FLOAT_TOLERANCE);

    jepa_latent_destroy(a);
}

TEST_F(JepaLatentTest, DistanceSymmetric)
{
    jepa_latent_t* a = create_test_latent(32, 1.0f);
    jepa_latent_t* b = create_test_latent(32, 2.0f);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    float dist_ab = jepa_latent_distance(a, b);
    float dist_ba = jepa_latent_distance(b, a);

    EXPECT_NEAR(dist_ab, dist_ba, FLOAT_TOLERANCE);

    jepa_latent_destroy(a);
    jepa_latent_destroy(b);
}

TEST_F(JepaLatentTest, DistanceNullReturnsNegative)
{
    jepa_latent_t* a = create_test_latent(32, 1.0f);

    float dist = jepa_latent_distance(nullptr, a);
    EXPECT_LT(dist, 0.0f);

    dist = jepa_latent_distance(a, nullptr);
    EXPECT_LT(dist, 0.0f);

    jepa_latent_destroy(a);
}

// =============================================================================
// Interpolation Tests
// =============================================================================

TEST_F(JepaLatentTest, InterpolateAlphaZeroGivesFirst)
{
    jepa_latent_t* a = create_test_latent(32, 1.0f);
    jepa_latent_t* b = create_test_latent(32, 5.0f);
    jepa_latent_t* result = jepa_latent_create_dim(32);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    ASSERT_NE(result, nullptr);

    int ret = jepa_latent_interpolate(a, b, 0.0f, result);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    // Result should equal a
    for (uint32_t i = 0; i < 32; i++) {
        EXPECT_NEAR(result->embedding[i], a->embedding[i], FLOAT_TOLERANCE);
    }

    jepa_latent_destroy(a);
    jepa_latent_destroy(b);
    jepa_latent_destroy(result);
}

TEST_F(JepaLatentTest, InterpolateAlphaOneGivesSecond)
{
    jepa_latent_t* a = create_test_latent(32, 1.0f);
    jepa_latent_t* b = create_test_latent(32, 5.0f);
    jepa_latent_t* result = jepa_latent_create_dim(32);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    ASSERT_NE(result, nullptr);

    int ret = jepa_latent_interpolate(a, b, 1.0f, result);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    // Result should equal b
    for (uint32_t i = 0; i < 32; i++) {
        EXPECT_NEAR(result->embedding[i], b->embedding[i], FLOAT_TOLERANCE);
    }

    jepa_latent_destroy(a);
    jepa_latent_destroy(b);
    jepa_latent_destroy(result);
}

TEST_F(JepaLatentTest, InterpolateAlphaHalfGivesMidpoint)
{
    jepa_latent_t* a = jepa_latent_create_dim(32);
    jepa_latent_t* b = jepa_latent_create_dim(32);
    jepa_latent_t* result = jepa_latent_create_dim(32);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    ASSERT_NE(result, nullptr);

    // a = [0, 0, ...], b = [2, 2, ...]
    for (uint32_t i = 0; i < 32; i++) {
        a->embedding[i] = 0.0f;
        b->embedding[i] = 2.0f;
    }

    int ret = jepa_latent_interpolate(a, b, 0.5f, result);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    // Result should be [1, 1, ...]
    for (uint32_t i = 0; i < 32; i++) {
        EXPECT_NEAR(result->embedding[i], 1.0f, FLOAT_TOLERANCE);
    }

    jepa_latent_destroy(a);
    jepa_latent_destroy(b);
    jepa_latent_destroy(result);
}

TEST_F(JepaLatentTest, InterpolateNullFails)
{
    jepa_latent_t* a = create_test_latent(32, 1.0f);
    jepa_latent_t* result = jepa_latent_create_dim(32);

    int ret = jepa_latent_interpolate(nullptr, a, 0.5f, result);
    EXPECT_NE(ret, NIMCP_SUCCESS);

    ret = jepa_latent_interpolate(a, nullptr, 0.5f, result);
    EXPECT_NE(ret, NIMCP_SUCCESS);

    ret = jepa_latent_interpolate(a, a, 0.5f, nullptr);
    EXPECT_NE(ret, NIMCP_SUCCESS);

    jepa_latent_destroy(a);
    jepa_latent_destroy(result);
}

TEST_F(JepaLatentTest, InterpolateDimensionMismatchFails)
{
    jepa_latent_t* a = jepa_latent_create_dim(32);
    jepa_latent_t* b = jepa_latent_create_dim(64);
    jepa_latent_t* result = jepa_latent_create_dim(32);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    ASSERT_NE(result, nullptr);

    int ret = jepa_latent_interpolate(a, b, 0.5f, result);
    EXPECT_NE(ret, NIMCP_SUCCESS);

    jepa_latent_destroy(a);
    jepa_latent_destroy(b);
    jepa_latent_destroy(result);
}

TEST_F(JepaLatentTest, SlerpPreservesNorm)
{
    // Create two normalized vectors
    float vals_a[32], vals_b[32];
    for (int i = 0; i < 32; i++) {
        vals_a[i] = (float)(i + 1);
        vals_b[i] = (float)(32 - i);
    }

    jepa_latent_t* a = create_unit_latent(32, vals_a);
    jepa_latent_t* b = create_unit_latent(32, vals_b);
    jepa_latent_t* result = jepa_latent_create_dim(32);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    ASSERT_NE(result, nullptr);

    int ret = jepa_latent_slerp(a, b, 0.5f, result);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    // Result should be approximately unit length
    float norm = jepa_latent_norm(result);
    EXPECT_NEAR(norm, 1.0f, 0.01f);

    jepa_latent_destroy(a);
    jepa_latent_destroy(b);
    jepa_latent_destroy(result);
}

// =============================================================================
// Arithmetic Tests
// =============================================================================

TEST_F(JepaLatentTest, AddElementwise)
{
    jepa_latent_t* a = jepa_latent_create_dim(32);
    jepa_latent_t* b = jepa_latent_create_dim(32);
    jepa_latent_t* result = jepa_latent_create_dim(32);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    ASSERT_NE(result, nullptr);

    for (uint32_t i = 0; i < 32; i++) {
        a->embedding[i] = 1.0f;
        b->embedding[i] = 2.0f;
    }

    int ret = jepa_latent_add(a, b, result);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    for (uint32_t i = 0; i < 32; i++) {
        EXPECT_NEAR(result->embedding[i], 3.0f, FLOAT_TOLERANCE);
    }

    jepa_latent_destroy(a);
    jepa_latent_destroy(b);
    jepa_latent_destroy(result);
}

TEST_F(JepaLatentTest, SubtractElementwise)
{
    jepa_latent_t* a = jepa_latent_create_dim(32);
    jepa_latent_t* b = jepa_latent_create_dim(32);
    jepa_latent_t* result = jepa_latent_create_dim(32);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    ASSERT_NE(result, nullptr);

    for (uint32_t i = 0; i < 32; i++) {
        a->embedding[i] = 5.0f;
        b->embedding[i] = 2.0f;
    }

    int ret = jepa_latent_subtract(a, b, result);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    for (uint32_t i = 0; i < 32; i++) {
        EXPECT_NEAR(result->embedding[i], 3.0f, FLOAT_TOLERANCE);
    }

    jepa_latent_destroy(a);
    jepa_latent_destroy(b);
    jepa_latent_destroy(result);
}

TEST_F(JepaLatentTest, ScaleInPlace)
{
    jepa_latent_t* latent = jepa_latent_create_dim(32);
    ASSERT_NE(latent, nullptr);

    for (uint32_t i = 0; i < 32; i++) {
        latent->embedding[i] = 2.0f;
    }

    int ret = jepa_latent_scale(latent, 3.0f);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    for (uint32_t i = 0; i < 32; i++) {
        EXPECT_NEAR(latent->embedding[i], 6.0f, FLOAT_TOLERANCE);
    }

    jepa_latent_destroy(latent);
}

TEST_F(JepaLatentTest, ScaleNullFails)
{
    int ret = jepa_latent_scale(nullptr, 2.0f);
    EXPECT_NE(ret, NIMCP_SUCCESS);
}

TEST_F(JepaLatentTest, DotProductComputed)
{
    jepa_latent_t* a = jepa_latent_create_dim(32);
    jepa_latent_t* b = jepa_latent_create_dim(32);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    for (uint32_t i = 0; i < 32; i++) {
        a->embedding[i] = 1.0f;
        b->embedding[i] = 2.0f;
    }

    float dot = jepa_latent_dot(a, b);

    // dot([1,...], [2,...]) = 32 * 1 * 2 = 64
    EXPECT_NEAR(dot, 64.0f, FLOAT_TOLERANCE);

    jepa_latent_destroy(a);
    jepa_latent_destroy(b);
}

TEST_F(JepaLatentTest, DotProductNullReturnsNaN)
{
    jepa_latent_t* a = create_test_latent(32, 1.0f);

    float dot = jepa_latent_dot(nullptr, a);
    EXPECT_TRUE(std::isnan(dot));

    dot = jepa_latent_dot(a, nullptr);
    EXPECT_TRUE(std::isnan(dot));

    jepa_latent_destroy(a);
}

// =============================================================================
// Projection Tests
// =============================================================================

TEST_F(JepaLatentTest, ProjectToLowerDim)
{
    jepa_latent_t* src = jepa_latent_create_dim(64);
    jepa_latent_t* result = jepa_latent_create_dim(32);
    ASSERT_NE(src, nullptr);
    ASSERT_NE(result, nullptr);

    // Set source values
    for (uint32_t i = 0; i < 64; i++) {
        src->embedding[i] = 1.0f;
    }

    // Identity-like projection (sum pairs)
    std::vector<float> projection(32 * 64, 0.0f);
    for (uint32_t i = 0; i < 32; i++) {
        projection[i * 64 + i] = 1.0f;
    }

    int ret = jepa_latent_project(src, projection.data(), nullptr, 32, result);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    jepa_latent_destroy(src);
    jepa_latent_destroy(result);
}

TEST_F(JepaLatentTest, ProjectWithBias)
{
    jepa_latent_t* src = jepa_latent_create_dim(32);
    jepa_latent_t* result = jepa_latent_create_dim(32);
    ASSERT_NE(src, nullptr);
    ASSERT_NE(result, nullptr);

    for (uint32_t i = 0; i < 32; i++) {
        src->embedding[i] = 1.0f;
    }

    // Identity projection
    std::vector<float> projection(32 * 32, 0.0f);
    for (uint32_t i = 0; i < 32; i++) {
        projection[i * 32 + i] = 1.0f;
    }

    // Bias of 0.5
    std::vector<float> bias(32, 0.5f);

    int ret = jepa_latent_project(src, projection.data(), bias.data(), 32, result);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    // Result should be 1.0 + 0.5 = 1.5
    for (uint32_t i = 0; i < 32; i++) {
        EXPECT_NEAR(result->embedding[i], 1.5f, FLOAT_TOLERANCE);
    }

    jepa_latent_destroy(src);
    jepa_latent_destroy(result);
}

TEST_F(JepaLatentTest, ProjectNullFails)
{
    jepa_latent_t* src = jepa_latent_create_dim(32);
    jepa_latent_t* result = jepa_latent_create_dim(32);
    std::vector<float> projection(32 * 32, 0.0f);

    int ret = jepa_latent_project(nullptr, projection.data(), nullptr, 32, result);
    EXPECT_NE(ret, NIMCP_SUCCESS);

    ret = jepa_latent_project(src, nullptr, nullptr, 32, result);
    EXPECT_NE(ret, NIMCP_SUCCESS);

    ret = jepa_latent_project(src, projection.data(), nullptr, 32, nullptr);
    EXPECT_NE(ret, NIMCP_SUCCESS);

    jepa_latent_destroy(src);
    jepa_latent_destroy(result);
}

// =============================================================================
// Pooling Tests
// =============================================================================

TEST_F(JepaLatentTest, MeanPoolMultipleLatents)
{
    const uint32_t dim = 32;
    const uint32_t num_latents = 4;

    std::vector<jepa_latent_t*> latents(num_latents);
    for (uint32_t i = 0; i < num_latents; i++) {
        latents[i] = jepa_latent_create_dim(dim);
        ASSERT_NE(latents[i], nullptr);
        for (uint32_t j = 0; j < dim; j++) {
            latents[i]->embedding[j] = (float)(i + 1);  // 1, 2, 3, 4
        }
    }

    jepa_latent_t* result = jepa_latent_create_dim(dim);
    ASSERT_NE(result, nullptr);

    int ret = jepa_latent_mean_pool(
        const_cast<const jepa_latent_t**>(latents.data()), num_latents, result);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    // Mean of 1, 2, 3, 4 = 2.5
    for (uint32_t i = 0; i < dim; i++) {
        EXPECT_NEAR(result->embedding[i], 2.5f, FLOAT_TOLERANCE);
    }

    for (auto* l : latents) {
        jepa_latent_destroy(l);
    }
    jepa_latent_destroy(result);
}

TEST_F(JepaLatentTest, MeanPoolSingleLatent)
{
    jepa_latent_t* latent = create_test_latent(32, 5.0f);
    jepa_latent_t* result = jepa_latent_create_dim(32);
    ASSERT_NE(latent, nullptr);
    ASSERT_NE(result, nullptr);

    const jepa_latent_t* latents[] = {latent};

    int ret = jepa_latent_mean_pool(latents, 1, result);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    // Single latent mean equals itself
    for (uint32_t i = 0; i < 32; i++) {
        EXPECT_NEAR(result->embedding[i], latent->embedding[i], FLOAT_TOLERANCE);
    }

    jepa_latent_destroy(latent);
    jepa_latent_destroy(result);
}

TEST_F(JepaLatentTest, MeanPoolNullFails)
{
    jepa_latent_t* latent = create_test_latent(32, 1.0f);
    jepa_latent_t* result = jepa_latent_create_dim(32);
    const jepa_latent_t* latents[] = {latent};

    int ret = jepa_latent_mean_pool(nullptr, 1, result);
    EXPECT_NE(ret, NIMCP_SUCCESS);

    ret = jepa_latent_mean_pool(latents, 0, result);
    EXPECT_NE(ret, NIMCP_SUCCESS);

    ret = jepa_latent_mean_pool(latents, 1, nullptr);
    EXPECT_NE(ret, NIMCP_SUCCESS);

    jepa_latent_destroy(latent);
    jepa_latent_destroy(result);
}

TEST_F(JepaLatentTest, MaxPoolMultipleLatents)
{
    const uint32_t dim = 32;
    const uint32_t num_latents = 3;

    std::vector<jepa_latent_t*> latents(num_latents);
    for (uint32_t i = 0; i < num_latents; i++) {
        latents[i] = jepa_latent_create_dim(dim);
        ASSERT_NE(latents[i], nullptr);
    }

    // Set up so different latents have max at different positions
    // latent[0]: [3, 0, 0, ...]
    // latent[1]: [0, 5, 0, ...]
    // latent[2]: [0, 0, 7, ...]
    for (uint32_t i = 0; i < num_latents; i++) {
        for (uint32_t j = 0; j < dim; j++) {
            latents[i]->embedding[j] = 0.0f;
        }
        latents[i]->embedding[i] = (float)(i + 1) * 2.0f + 1.0f;  // 3, 5, 7
    }

    jepa_latent_t* result = jepa_latent_create_dim(dim);
    ASSERT_NE(result, nullptr);

    int ret = jepa_latent_max_pool(
        const_cast<const jepa_latent_t**>(latents.data()), num_latents, result);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    EXPECT_NEAR(result->embedding[0], 3.0f, FLOAT_TOLERANCE);
    EXPECT_NEAR(result->embedding[1], 5.0f, FLOAT_TOLERANCE);
    EXPECT_NEAR(result->embedding[2], 7.0f, FLOAT_TOLERANCE);

    for (auto* l : latents) {
        jepa_latent_destroy(l);
    }
    jepa_latent_destroy(result);
}

// =============================================================================
// Statistics Tests
// =============================================================================

TEST_F(JepaLatentTest, GetStatsAfterOperations)
{
    jepa_latent_reset_stats();

    // Create some latents with non-zero values so normalization can succeed
    jepa_latent_t* a = create_test_latent(32, 1.0f);
    jepa_latent_t* b = create_test_latent(32, 2.0f);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    // Perform some operations
    int norm_ret = jepa_latent_normalize(a);
    EXPECT_EQ(norm_ret, NIMCP_SUCCESS);
    jepa_latent_cosine_similarity(a, b);

    jepa_latent_stats_t stats;
    int ret = jepa_latent_get_stats(&stats);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    EXPECT_GE(stats.latents_created, 2u);
    EXPECT_GE(stats.normalizations, 1u);
    EXPECT_GE(stats.similarity_ops, 1u);

    jepa_latent_destroy(a);
    jepa_latent_destroy(b);

    // Check destroyed count updated
    ret = jepa_latent_get_stats(&stats);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_GE(stats.latents_destroyed, 2u);
}

TEST_F(JepaLatentTest, GetStatsNullFails)
{
    int ret = jepa_latent_get_stats(nullptr);
    EXPECT_NE(ret, NIMCP_SUCCESS);
}

TEST_F(JepaLatentTest, ResetStatsClears)
{
    // Create and destroy some latents
    jepa_latent_t* latent = jepa_latent_create_dim(32);
    jepa_latent_destroy(latent);

    // Reset
    int ret = jepa_latent_reset_stats();
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    jepa_latent_stats_t stats;
    jepa_latent_get_stats(&stats);

    EXPECT_EQ(stats.latents_created, 0u);
    EXPECT_EQ(stats.latents_destroyed, 0u);
    EXPECT_EQ(stats.normalizations, 0u);
    EXPECT_EQ(stats.similarity_ops, 0u);
    EXPECT_EQ(stats.interpolations, 0u);
}

// =============================================================================
// String Conversion Tests
// =============================================================================

TEST_F(JepaLatentTest, ModalityToString)
{
    EXPECT_NE(jepa_modality_to_string(JEPA_MODALITY_UNKNOWN), nullptr);
    EXPECT_NE(jepa_modality_to_string(JEPA_MODALITY_VISUAL), nullptr);
    EXPECT_NE(jepa_modality_to_string(JEPA_MODALITY_SPEECH), nullptr);
    EXPECT_NE(jepa_modality_to_string(JEPA_MODALITY_TEXT), nullptr);
    EXPECT_NE(jepa_modality_to_string(JEPA_MODALITY_MOTOR), nullptr);
    EXPECT_NE(jepa_modality_to_string(JEPA_MODALITY_MULTIMODAL), nullptr);

    // Verify they are distinct strings
    EXPECT_STRNE(jepa_modality_to_string(JEPA_MODALITY_VISUAL),
                 jepa_modality_to_string(JEPA_MODALITY_TEXT));
}

TEST_F(JepaLatentTest, NormTypeToString)
{
    EXPECT_NE(jepa_norm_type_to_string(JEPA_NORM_NONE), nullptr);
    EXPECT_NE(jepa_norm_type_to_string(JEPA_NORM_L2), nullptr);
    EXPECT_NE(jepa_norm_type_to_string(JEPA_NORM_LAYERNORM), nullptr);
    EXPECT_NE(jepa_norm_type_to_string(JEPA_NORM_BATCHNORM), nullptr);
}

TEST_F(JepaLatentTest, SimilarityMetricToString)
{
    EXPECT_NE(jepa_similarity_to_string(JEPA_SIM_COSINE), nullptr);
    EXPECT_NE(jepa_similarity_to_string(JEPA_SIM_DOT_PRODUCT), nullptr);
    EXPECT_NE(jepa_similarity_to_string(JEPA_SIM_EUCLIDEAN), nullptr);
    EXPECT_NE(jepa_similarity_to_string(JEPA_SIM_PRECISION_WEIGHTED), nullptr);
}

// =============================================================================
// Edge Cases and Stress Tests
// =============================================================================

TEST_F(JepaLatentTest, MultipleCloneChain)
{
    jepa_latent_t* original = create_test_latent(32, 1.0f);
    ASSERT_NE(original, nullptr);

    jepa_latent_t* clone1 = jepa_latent_clone(original);
    jepa_latent_t* clone2 = jepa_latent_clone(clone1);
    jepa_latent_t* clone3 = jepa_latent_clone(clone2);

    ASSERT_NE(clone1, nullptr);
    ASSERT_NE(clone2, nullptr);
    ASSERT_NE(clone3, nullptr);

    // All should have same values
    for (uint32_t i = 0; i < 32; i++) {
        EXPECT_FLOAT_EQ(original->embedding[i], clone3->embedding[i]);
    }

    // All should have independent memory
    EXPECT_NE(original->embedding, clone1->embedding);
    EXPECT_NE(clone1->embedding, clone2->embedding);
    EXPECT_NE(clone2->embedding, clone3->embedding);

    jepa_latent_destroy(original);
    jepa_latent_destroy(clone1);
    jepa_latent_destroy(clone2);
    jepa_latent_destroy(clone3);
}

TEST_F(JepaLatentTest, ZeroEmbeddingNormalization)
{
    jepa_latent_t* latent = jepa_latent_create_dim(32);
    ASSERT_NE(latent, nullptr);

    // All zeros
    for (uint32_t i = 0; i < 32; i++) {
        latent->embedding[i] = 0.0f;
    }

    // Normalizing zero vector should handle gracefully
    int ret = jepa_latent_normalize(latent);
    // Implementation may return error or keep as-is
    // Just verify no crash and embedding is still valid
    EXPECT_NE(latent->embedding, nullptr);

    jepa_latent_destroy(latent);
}

TEST_F(JepaLatentTest, VerySmallEmbeddingValues)
{
    jepa_latent_t* a = jepa_latent_create_dim(32);
    jepa_latent_t* b = jepa_latent_create_dim(32);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    // Very small values (near epsilon)
    for (uint32_t i = 0; i < 32; i++) {
        a->embedding[i] = JEPA_LATENT_EPSILON * (float)(i + 1);
        b->embedding[i] = JEPA_LATENT_EPSILON * (float)(i + 1);
    }

    // Operations should handle without NaN/Inf
    float sim = jepa_latent_cosine_similarity(a, b);
    EXPECT_FALSE(std::isnan(sim));
    EXPECT_FALSE(std::isinf(sim));

    float dist = jepa_latent_distance(a, b);
    EXPECT_FALSE(std::isnan(dist));
    EXPECT_FALSE(std::isinf(dist));

    jepa_latent_destroy(a);
    jepa_latent_destroy(b);
}

TEST_F(JepaLatentTest, VeryLargeEmbeddingValues)
{
    jepa_latent_t* a = jepa_latent_create_dim(32);
    jepa_latent_t* b = jepa_latent_create_dim(32);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    // Large values
    for (uint32_t i = 0; i < 32; i++) {
        a->embedding[i] = 1e6f * (float)(i + 1);
        b->embedding[i] = 1e6f * (float)(i + 1);
    }

    // Operations should handle without overflow
    float sim = jepa_latent_cosine_similarity(a, b);
    EXPECT_FALSE(std::isnan(sim));

    jepa_latent_destroy(a);
    jepa_latent_destroy(b);
}

TEST_F(JepaLatentTest, InPlaceOperationSameAsInput)
{
    jepa_latent_t* a = jepa_latent_create_dim(32);
    jepa_latent_t* b = jepa_latent_create_dim(32);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    for (uint32_t i = 0; i < 32; i++) {
        a->embedding[i] = 1.0f;
        b->embedding[i] = 2.0f;
    }

    // Add a + b into a (in-place)
    int ret = jepa_latent_add(a, b, a);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    for (uint32_t i = 0; i < 32; i++) {
        EXPECT_NEAR(a->embedding[i], 3.0f, FLOAT_TOLERANCE);
    }

    jepa_latent_destroy(a);
    jepa_latent_destroy(b);
}

TEST_F(JepaLatentTest, MetadataPreservedOnClone)
{
    jepa_latent_config_t config;
    jepa_latent_default_config(&config);
    config.latent_dim = 64;
    config.modality = JEPA_MODALITY_SPEECH;

    jepa_latent_t* original = jepa_latent_create(&config);
    ASSERT_NE(original, nullptr);

    original->timestamp_ms = 12345678;
    original->sequence_position = 42;
    original->is_normalized = true;
    original->norm_type = JEPA_NORM_L2;

    jepa_latent_t* clone = jepa_latent_clone(original);
    ASSERT_NE(clone, nullptr);

    EXPECT_EQ(clone->modality, JEPA_MODALITY_SPEECH);
    EXPECT_EQ(clone->timestamp_ms, 12345678u);
    EXPECT_EQ(clone->sequence_position, 42u);
    EXPECT_EQ(clone->is_normalized, true);
    EXPECT_EQ(clone->norm_type, JEPA_NORM_L2);

    jepa_latent_destroy(original);
    jepa_latent_destroy(clone);
}

// =============================================================================
// Tier-Specific Configuration Tests
// =============================================================================

TEST_F(JepaLatentTest, DefaultConfigRespectsTierLimits)
{
    jepa_latent_config_t config;
    int result = jepa_latent_default_config(&config);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Default dim should be within bounds regardless of tier
    EXPECT_GE(config.latent_dim, JEPA_LATENT_MIN_DIM);
    EXPECT_LE(config.latent_dim, JEPA_LATENT_MAX_DIM);

    // Precision should be positive and within reasonable bounds
    EXPECT_GE(config.initial_precision, JEPA_LATENT_MIN_PRECISION);
    EXPECT_LE(config.initial_precision, JEPA_LATENT_MAX_PRECISION);
}

TEST_F(JepaLatentTest, CreateWithMinimalTierDimension)
{
    // Test with minimum valid dimension (simulates constrained tier)
    jepa_latent_config_t config;
    jepa_latent_default_config(&config);
    config.latent_dim = JEPA_LATENT_MIN_DIM;  // 16 - minimal tier size

    jepa_latent_t* latent = jepa_latent_create(&config);
    ASSERT_NE(latent, nullptr);
    EXPECT_EQ(latent->latent_dim, JEPA_LATENT_MIN_DIM);

    // Set non-zero values so normalization can succeed
    for (uint32_t i = 0; i < JEPA_LATENT_MIN_DIM; i++) {
        latent->embedding[i] = 1.0f + (float)i * 0.1f;
    }

    // Verify operations work at minimal dimension
    int ret = jepa_latent_normalize(latent);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    jepa_latent_destroy(latent);
}

TEST_F(JepaLatentTest, CreateWithFullTierDimension)
{
    // Test with maximum valid dimension (full tier)
    jepa_latent_config_t config;
    jepa_latent_default_config(&config);
    config.latent_dim = JEPA_LATENT_MAX_DIM;  // 1024 - full tier size

    jepa_latent_t* latent = jepa_latent_create(&config);
    ASSERT_NE(latent, nullptr);
    EXPECT_EQ(latent->latent_dim, JEPA_LATENT_MAX_DIM);

    // Verify operations work at maximum dimension
    float norm = jepa_latent_norm(latent);
    EXPECT_GE(norm, 0.0f);

    jepa_latent_destroy(latent);
}

TEST_F(JepaLatentTest, SimilarityAcrossDifferentTierDimensions)
{
    // Verify dimension mismatch is properly handled (simulates cross-tier comparison)
    jepa_latent_t* minimal = jepa_latent_create_dim(JEPA_LATENT_MIN_DIM);  // 16
    jepa_latent_t* medium = jepa_latent_create_dim(128);                    // Medium tier
    jepa_latent_t* full = jepa_latent_create_dim(JEPA_LATENT_MAX_DIM);     // 1024

    ASSERT_NE(minimal, nullptr);
    ASSERT_NE(medium, nullptr);
    ASSERT_NE(full, nullptr);

    // Cross-tier comparisons should return NaN
    float sim_min_med = jepa_latent_cosine_similarity(minimal, medium);
    float sim_min_full = jepa_latent_cosine_similarity(minimal, full);
    float sim_med_full = jepa_latent_cosine_similarity(medium, full);

    EXPECT_TRUE(std::isnan(sim_min_med));
    EXPECT_TRUE(std::isnan(sim_min_full));
    EXPECT_TRUE(std::isnan(sim_med_full));

    jepa_latent_destroy(minimal);
    jepa_latent_destroy(medium);
    jepa_latent_destroy(full);
}

TEST_F(JepaLatentTest, PoolingAcrossDifferentTierDimensions)
{
    // Mean pooling should fail with mismatched dimensions
    jepa_latent_t* latent_16 = jepa_latent_create_dim(16);
    jepa_latent_t* latent_32 = jepa_latent_create_dim(32);
    jepa_latent_t* result = jepa_latent_create_dim(16);

    ASSERT_NE(latent_16, nullptr);
    ASSERT_NE(latent_32, nullptr);
    ASSERT_NE(result, nullptr);

    const jepa_latent_t* mixed[] = {latent_16, latent_32};
    int ret = jepa_latent_mean_pool(mixed, 2, result);
    EXPECT_NE(ret, NIMCP_SUCCESS);

    jepa_latent_destroy(latent_16);
    jepa_latent_destroy(latent_32);
    jepa_latent_destroy(result);
}

// =============================================================================
// Additional Edge Case Tests
// =============================================================================

TEST_F(JepaLatentTest, NegativeEmbeddingValues)
{
    jepa_latent_t* latent = jepa_latent_create_dim(32);
    ASSERT_NE(latent, nullptr);

    // Set all negative values
    for (uint32_t i = 0; i < 32; i++) {
        latent->embedding[i] = -1.0f * (float)(i + 1);
    }

    // Normalization should work
    int ret = jepa_latent_normalize(latent);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    float norm = jepa_latent_norm(latent);
    EXPECT_NEAR(norm, 1.0f, FLOAT_TOLERANCE);

    jepa_latent_destroy(latent);
}

TEST_F(JepaLatentTest, MixedSignEmbeddingValues)
{
    jepa_latent_t* a = jepa_latent_create_dim(32);
    jepa_latent_t* b = jepa_latent_create_dim(32);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    // Alternating positive/negative
    for (uint32_t i = 0; i < 32; i++) {
        a->embedding[i] = (i % 2 == 0) ? 1.0f : -1.0f;
        b->embedding[i] = (i % 2 == 0) ? -1.0f : 1.0f;
    }

    // These vectors should have negative similarity
    float sim = jepa_latent_cosine_similarity(a, b);
    EXPECT_LT(sim, 0.0f);
    EXPECT_NEAR(sim, -1.0f, FLOAT_TOLERANCE);

    jepa_latent_destroy(a);
    jepa_latent_destroy(b);
}

TEST_F(JepaLatentTest, InterpolateOutOfBoundsAlpha)
{
    jepa_latent_t* a = jepa_latent_create_dim(32);
    jepa_latent_t* b = jepa_latent_create_dim(32);
    jepa_latent_t* result = jepa_latent_create_dim(32);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    ASSERT_NE(result, nullptr);

    for (uint32_t i = 0; i < 32; i++) {
        a->embedding[i] = 0.0f;
        b->embedding[i] = 10.0f;
    }

    // Alpha < 0 (extrapolation)
    int ret = jepa_latent_interpolate(a, b, -0.5f, result);
    // Should either clamp or extrapolate - check no crash
    (void)ret;

    // Alpha > 1 (extrapolation)
    ret = jepa_latent_interpolate(a, b, 1.5f, result);
    // Should either clamp or extrapolate - check no crash
    (void)ret;

    jepa_latent_destroy(a);
    jepa_latent_destroy(b);
    jepa_latent_destroy(result);
}

TEST_F(JepaLatentTest, ScaleByZero)
{
    jepa_latent_t* latent = create_test_latent(32, 5.0f);
    ASSERT_NE(latent, nullptr);

    int ret = jepa_latent_scale(latent, 0.0f);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    // All values should be zero
    for (uint32_t i = 0; i < 32; i++) {
        EXPECT_FLOAT_EQ(latent->embedding[i], 0.0f);
    }

    jepa_latent_destroy(latent);
}

TEST_F(JepaLatentTest, ScaleByNegative)
{
    jepa_latent_t* latent = jepa_latent_create_dim(32);
    ASSERT_NE(latent, nullptr);

    for (uint32_t i = 0; i < 32; i++) {
        latent->embedding[i] = 2.0f;
    }

    int ret = jepa_latent_scale(latent, -1.5f);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    for (uint32_t i = 0; i < 32; i++) {
        EXPECT_NEAR(latent->embedding[i], -3.0f, FLOAT_TOLERANCE);
    }

    jepa_latent_destroy(latent);
}

TEST_F(JepaLatentTest, VarianceWithZeroValues)
{
    jepa_latent_config_t config;
    jepa_latent_default_config(&config);
    config.latent_dim = 32;
    config.enable_variance = true;

    jepa_latent_t* latent = jepa_latent_create(&config);
    ASSERT_NE(latent, nullptr);
    ASSERT_NE(latent->variance, nullptr);

    // Set variance to very small values (near zero)
    float variance[32];
    for (int i = 0; i < 32; i++) {
        variance[i] = JEPA_LATENT_EPSILON;
    }
    jepa_latent_set_variance(latent, variance, 32);

    int ret = jepa_latent_update_precision(latent);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    // Precision should be very high but finite
    EXPECT_FALSE(std::isinf(latent->precision));
    EXPECT_GT(latent->precision, 0.0f);

    jepa_latent_destroy(latent);
}

TEST_F(JepaLatentTest, AllModalitiesSupported)
{
    // Test that all modalities can be used
    jepa_modality_t modalities[] = {
        JEPA_MODALITY_UNKNOWN,
        JEPA_MODALITY_VISUAL,
        JEPA_MODALITY_SPEECH,
        JEPA_MODALITY_TEXT,
        JEPA_MODALITY_MOTOR,
        JEPA_MODALITY_MULTIMODAL
    };

    for (auto modality : modalities) {
        jepa_latent_config_t config;
        jepa_latent_default_config(&config);
        config.modality = modality;

        jepa_latent_t* latent = jepa_latent_create(&config);
        ASSERT_NE(latent, nullptr);
        EXPECT_EQ(latent->modality, modality);

        // Verify string conversion works
        const char* name = jepa_modality_to_string(modality);
        EXPECT_NE(name, nullptr);
        EXPECT_GT(strlen(name), 0u);

        jepa_latent_destroy(latent);
    }
}

TEST_F(JepaLatentTest, AllNormTypesSupported)
{
    jepa_norm_type_t norm_types[] = {
        JEPA_NORM_NONE,
        JEPA_NORM_L2,
        JEPA_NORM_LAYERNORM,
        JEPA_NORM_BATCHNORM
    };

    for (auto norm : norm_types) {
        jepa_latent_config_t config;
        jepa_latent_default_config(&config);
        config.norm_type = norm;

        jepa_latent_t* latent = jepa_latent_create(&config);
        ASSERT_NE(latent, nullptr);
        EXPECT_EQ(latent->norm_type, norm);

        // Verify string conversion works
        const char* name = jepa_norm_type_to_string(norm);
        EXPECT_NE(name, nullptr);
        EXPECT_GT(strlen(name), 0u);

        jepa_latent_destroy(latent);
    }
}

TEST_F(JepaLatentTest, AllSimilarityMetricsSupported)
{
    jepa_similarity_t metrics[] = {
        JEPA_SIM_COSINE,
        JEPA_SIM_DOT_PRODUCT,
        JEPA_SIM_EUCLIDEAN,
        JEPA_SIM_PRECISION_WEIGHTED
    };

    jepa_latent_config_t config;
    jepa_latent_default_config(&config);
    config.latent_dim = 32;
    config.enable_variance = true;

    jepa_latent_t* a = jepa_latent_create(&config);
    jepa_latent_t* b = jepa_latent_create(&config);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    // Set values
    for (uint32_t i = 0; i < 32; i++) {
        a->embedding[i] = 1.0f;
        b->embedding[i] = 2.0f;
        if (a->variance) a->variance[i] = 0.5f;
        if (b->variance) b->variance[i] = 0.5f;
    }

    for (auto metric : metrics) {
        float sim = jepa_latent_similarity(a, b, metric);
        EXPECT_FALSE(std::isnan(sim)) << "Metric " << (int)metric << " returned NaN";

        // Verify string conversion works
        const char* name = jepa_similarity_to_string(metric);
        EXPECT_NE(name, nullptr);
        EXPECT_GT(strlen(name), 0u);
    }

    jepa_latent_destroy(a);
    jepa_latent_destroy(b);
}

TEST_F(JepaLatentTest, ProjectToHigherDimension)
{
    jepa_latent_t* src = jepa_latent_create_dim(32);
    jepa_latent_t* result = jepa_latent_create_dim(64);
    ASSERT_NE(src, nullptr);
    ASSERT_NE(result, nullptr);

    for (uint32_t i = 0; i < 32; i++) {
        src->embedding[i] = 1.0f;
    }

    // Project 32 -> 64 (expand)
    std::vector<float> projection(64 * 32, 0.0f);
    for (uint32_t i = 0; i < 32; i++) {
        projection[i * 32 + i] = 1.0f;           // Copy to first half
        projection[(i + 32) * 32 + i] = 1.0f;    // Copy to second half
    }

    int ret = jepa_latent_project(src, projection.data(), nullptr, 64, result);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    jepa_latent_destroy(src);
    jepa_latent_destroy(result);
}

TEST_F(JepaLatentTest, SlerpWithNonNormalizedVectors)
{
    jepa_latent_t* a = create_test_latent(32, 1.0f);
    jepa_latent_t* b = create_test_latent(32, 5.0f);
    jepa_latent_t* result = jepa_latent_create_dim(32);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    ASSERT_NE(result, nullptr);

    // Vectors are not normalized - SLERP should handle this
    a->is_normalized = false;
    b->is_normalized = false;

    int ret = jepa_latent_slerp(a, b, 0.5f, result);
    // Should either normalize internally or return error
    // Either way, should not crash
    (void)ret;

    jepa_latent_destroy(a);
    jepa_latent_destroy(b);
    jepa_latent_destroy(result);
}

TEST_F(JepaLatentTest, ReferenceCountInitialized)
{
    jepa_latent_t* latent = jepa_latent_create_dim(32);
    ASSERT_NE(latent, nullptr);

    // Reference count should be initialized to 1
    EXPECT_EQ(latent->ref_count, 1u);

    jepa_latent_t* clone = jepa_latent_clone(latent);
    ASSERT_NE(clone, nullptr);

    // Clone should also have ref_count of 1 (independent copy)
    EXPECT_EQ(clone->ref_count, 1u);

    jepa_latent_destroy(latent);
    jepa_latent_destroy(clone);
}

TEST_F(JepaLatentTest, TimestampAndSequencePosition)
{
    jepa_latent_t* latent = jepa_latent_create_dim(32);
    ASSERT_NE(latent, nullptr);

    // Set metadata
    latent->timestamp_ms = 1703548800000ULL;  // Example timestamp
    latent->sequence_position = 42;

    // Clone should preserve metadata
    jepa_latent_t* clone = jepa_latent_clone(latent);
    ASSERT_NE(clone, nullptr);

    EXPECT_EQ(clone->timestamp_ms, 1703548800000ULL);
    EXPECT_EQ(clone->sequence_position, 42u);

    jepa_latent_destroy(latent);
    jepa_latent_destroy(clone);
}

TEST_F(JepaLatentTest, ConstantsAreValid)
{
    // Verify constants are sensible
    EXPECT_GT(JEPA_LATENT_MAX_DIM, JEPA_LATENT_MIN_DIM);
    EXPECT_GT(JEPA_LATENT_MIN_DIM, 0u);
    EXPECT_GT(JEPA_LATENT_DEFAULT_PRECISION, 0.0f);
    EXPECT_GT(JEPA_LATENT_MIN_PRECISION, 0.0f);
    EXPECT_GT(JEPA_LATENT_MAX_PRECISION, JEPA_LATENT_MIN_PRECISION);
    EXPECT_GT(JEPA_LATENT_SIMILARITY_THRESHOLD, 0.0f);
    EXPECT_LE(JEPA_LATENT_SIMILARITY_THRESHOLD, 1.0f);
    EXPECT_GT(JEPA_LATENT_EPSILON, 0.0f);
    EXPECT_LT(JEPA_LATENT_EPSILON, 0.001f);
}

TEST_F(JepaLatentTest, BiologicalModuleIdDefined)
{
    // Verify bio-async module ID is defined
    EXPECT_NE(BIO_MODULE_JEPA_LATENT, 0u);
}

// Main function for running tests
int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
