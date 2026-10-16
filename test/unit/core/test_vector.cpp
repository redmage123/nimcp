/**
 * @file test_vector.cpp
 * @brief Unit tests for vector mathematics utilities
 */

#include <gtest/gtest.h>
#include <cmath>
#include "utils/containers/nimcp_vector.h"

// Test fixture for vector operations
class VectorTest : public ::testing::Test {
   protected:
    static constexpr float FLOAT_TOLERANCE = 1e-5f;

    // Helper to compare floats with tolerance
    bool FloatEquals(float a, float b, float tolerance = FLOAT_TOLERANCE)
    {
        return std::fabs(a - b) < tolerance;
    }
};

//=============================================================================
// Basic Vector Operations Tests
//=============================================================================

/**
 * WHAT: Test dot product with simple vectors
 * WHY: Verify basic dot product calculation
 */
TEST_F(VectorTest, DotProduct_Simple)
{
    float a[] = {1.0f, 2.0f, 3.0f};
    float b[] = {4.0f, 5.0f, 6.0f};

    // Expected: 1*4 + 2*5 + 3*6 = 4 + 10 + 18 = 32
    float result = nimcp_vector_dot_product(a, b, 3);
    EXPECT_TRUE(FloatEquals(result, 32.0f));
}

/**
 * WHAT: Test dot product with zero vector
 * WHY: Verify edge case handling
 */
TEST_F(VectorTest, DotProduct_ZeroVector)
{
    float a[] = {1.0f, 2.0f, 3.0f};
    float b[] = {0.0f, 0.0f, 0.0f};

    float result = nimcp_vector_dot_product(a, b, 3);
    EXPECT_TRUE(FloatEquals(result, 0.0f));
}

/**
 * WHAT: Test dot product with null inputs
 * WHY: Verify null safety
 */
TEST_F(VectorTest, DotProduct_NullInputs)
{
    float a[] = {1.0f, 2.0f, 3.0f};

    EXPECT_TRUE(FloatEquals(nimcp_vector_dot_product(nullptr, a, 3), 0.0f));
    EXPECT_TRUE(FloatEquals(nimcp_vector_dot_product(a, nullptr, 3), 0.0f));
    EXPECT_TRUE(FloatEquals(nimcp_vector_dot_product(nullptr, nullptr, 3), 0.0f));
}

/**
 * WHAT: Test dot product with zero size
 * WHY: Verify size validation
 */
TEST_F(VectorTest, DotProduct_ZeroSize)
{
    float a[] = {1.0f, 2.0f, 3.0f};
    float b[] = {4.0f, 5.0f, 6.0f};

    float result = nimcp_vector_dot_product(a, b, 0);
    EXPECT_TRUE(FloatEquals(result, 0.0f));
}

/**
 * WHAT: Test L2 norm calculation
 * WHY: Verify Euclidean norm computation
 */
TEST_F(VectorTest, NormL2_Simple)
{
    float vec[] = {3.0f, 4.0f};

    // Expected: sqrt(3^2 + 4^2) = sqrt(9 + 16) = sqrt(25) = 5
    float result = nimcp_vector_norm_l2(vec, 2);
    EXPECT_TRUE(FloatEquals(result, 5.0f));
}

/**
 * WHAT: Test L2 norm with negative values
 * WHY: Verify norm handles negative values correctly
 */
TEST_F(VectorTest, NormL2_NegativeValues)
{
    float vec[] = {-3.0f, -4.0f};

    // Expected: sqrt((-3)^2 + (-4)^2) = sqrt(9 + 16) = 5
    float result = nimcp_vector_norm_l2(vec, 2);
    EXPECT_TRUE(FloatEquals(result, 5.0f));
}

/**
 * WHAT: Test L2 norm with zero vector
 * WHY: Verify edge case
 */
TEST_F(VectorTest, NormL2_ZeroVector)
{
    float vec[] = {0.0f, 0.0f, 0.0f};

    float result = nimcp_vector_norm_l2(vec, 3);
    EXPECT_TRUE(FloatEquals(result, 0.0f));
}

/**
 * WHAT: Test L2 norm with null input
 * WHY: Verify null safety
 */
TEST_F(VectorTest, NormL2_NullInput)
{
    EXPECT_TRUE(FloatEquals(nimcp_vector_norm_l2(nullptr, 3), 0.0f));
}

/**
 * WHAT: Test L1 norm calculation
 * WHY: Verify Manhattan norm computation
 */
TEST_F(VectorTest, NormL1_Simple)
{
    float vec[] = {1.0f, 2.0f, 3.0f};

    // Expected: |1| + |2| + |3| = 6
    float result = nimcp_vector_norm_l1(vec, 3);
    EXPECT_TRUE(FloatEquals(result, 6.0f));
}

/**
 * WHAT: Test L1 norm with negative values
 * WHY: Verify absolute value handling
 */
TEST_F(VectorTest, NormL1_NegativeValues)
{
    float vec[] = {-1.0f, -2.0f, 3.0f};

    // Expected: |-1| + |-2| + |3| = 1 + 2 + 3 = 6
    float result = nimcp_vector_norm_l1(vec, 3);
    EXPECT_TRUE(FloatEquals(result, 6.0f));
}

/**
 * WHAT: Test vector copy operation
 * WHY: Verify data is copied correctly
 */
TEST_F(VectorTest, Copy_Simple)
{
    float src[] = {1.0f, 2.0f, 3.0f, 4.0f};
    float dst[4] = {0.0f};

    nimcp_vector_copy(src, dst, 4);

    for (int i = 0; i < 4; i++) {
        EXPECT_TRUE(FloatEquals(dst[i], src[i]));
    }
}

/**
 * WHAT: Test vector copy with null inputs
 * WHY: Verify null safety (should not crash)
 */
TEST_F(VectorTest, Copy_NullInputs)
{
    float vec[4] = {1.0f, 2.0f, 3.0f, 4.0f};

    // These should not crash
    nimcp_vector_copy(nullptr, vec, 4);
    nimcp_vector_copy(vec, nullptr, 4);
    nimcp_vector_copy(nullptr, nullptr, 4);
}

//=============================================================================
// Similarity and Distance Metrics Tests
//=============================================================================

/**
 * WHAT: Test cosine similarity with identical vectors
 * WHY: Verify result is 1.0 for identical directions
 */
TEST_F(VectorTest, CosineSimilarity_Identical)
{
    float a[] = {1.0f, 2.0f, 3.0f};
    float b[] = {1.0f, 2.0f, 3.0f};

    float result = nimcp_vector_cosine_similarity(a, b, 3);
    EXPECT_TRUE(FloatEquals(result, 1.0f));
}

/**
 * WHAT: Test cosine similarity with scaled vectors
 * WHY: Verify scale invariance (scaled vectors should have same similarity)
 */
TEST_F(VectorTest, CosineSimilarity_Scaled)
{
    float a[] = {1.0f, 2.0f, 3.0f};
    float b[] = {2.0f, 4.0f, 6.0f};  // 2x scaled

    float result = nimcp_vector_cosine_similarity(a, b, 3);
    EXPECT_TRUE(FloatEquals(result, 1.0f));
}

/**
 * WHAT: Test cosine similarity with orthogonal vectors
 * WHY: Verify result is 0.0 for perpendicular vectors
 */
TEST_F(VectorTest, CosineSimilarity_Orthogonal)
{
    float a[] = {1.0f, 0.0f};
    float b[] = {0.0f, 1.0f};

    float result = nimcp_vector_cosine_similarity(a, b, 2);
    EXPECT_TRUE(FloatEquals(result, 0.0f));
}

/**
 * WHAT: Test cosine similarity with opposite vectors
 * WHY: Verify result is -1.0 for opposite directions
 */
TEST_F(VectorTest, CosineSimilarity_Opposite)
{
    float a[] = {1.0f, 2.0f, 3.0f};
    float b[] = {-1.0f, -2.0f, -3.0f};

    float result = nimcp_vector_cosine_similarity(a, b, 3);
    EXPECT_TRUE(FloatEquals(result, -1.0f));
}

/**
 * WHAT: Test cosine similarity with both zero vectors
 * WHY: Verify special case returns 1.0 (perfect match)
 */
TEST_F(VectorTest, CosineSimilarity_BothZero)
{
    float a[] = {0.0f, 0.0f, 0.0f};
    float b[] = {0.0f, 0.0f, 0.0f};

    float result = nimcp_vector_cosine_similarity(a, b, 3);
    EXPECT_TRUE(FloatEquals(result, 1.0f));
}

/**
 * WHAT: Test cosine similarity with one zero vector
 * WHY: Verify special case returns 0.0 (no similarity)
 */
TEST_F(VectorTest, CosineSimilarity_OneZero)
{
    float a[] = {1.0f, 2.0f, 3.0f};
    float b[] = {0.0f, 0.0f, 0.0f};

    float result = nimcp_vector_cosine_similarity(a, b, 3);
    EXPECT_TRUE(FloatEquals(result, 0.0f));
}

/**
 * WHAT: Test cosine distance with identical vectors
 * WHY: Verify distance is 0 for identical directions
 */
TEST_F(VectorTest, CosineDistance_Identical)
{
    float a[] = {1.0f, 2.0f, 3.0f};
    float b[] = {1.0f, 2.0f, 3.0f};

    float result = nimcp_vector_cosine_distance(a, b, 3);
    EXPECT_TRUE(FloatEquals(result, 0.0f));
}

/**
 * WHAT: Test cosine distance with orthogonal vectors
 * WHY: Verify distance is 1.0 for perpendicular vectors
 */
TEST_F(VectorTest, CosineDistance_Orthogonal)
{
    float a[] = {1.0f, 0.0f};
    float b[] = {0.0f, 1.0f};

    float result = nimcp_vector_cosine_distance(a, b, 2);
    EXPECT_TRUE(FloatEquals(result, 1.0f));
}

/**
 * WHAT: Test cosine distance with opposite vectors
 * WHY: Verify distance is 2.0 for opposite directions
 */
TEST_F(VectorTest, CosineDistance_Opposite)
{
    float a[] = {1.0f, 2.0f, 3.0f};
    float b[] = {-1.0f, -2.0f, -3.0f};

    float result = nimcp_vector_cosine_distance(a, b, 3);
    EXPECT_TRUE(FloatEquals(result, 2.0f));
}

/**
 * WHAT: Test Euclidean distance calculation
 * WHY: Verify standard distance metric
 */
TEST_F(VectorTest, EuclideanDistance_Simple)
{
    float a[] = {0.0f, 0.0f};
    float b[] = {3.0f, 4.0f};

    // Expected: sqrt((3-0)^2 + (4-0)^2) = sqrt(9 + 16) = 5
    float result = nimcp_vector_euclidean_distance(a, b, 2);
    EXPECT_TRUE(FloatEquals(result, 5.0f));
}

/**
 * WHAT: Test Euclidean distance with identical vectors
 * WHY: Verify distance is 0 for same point
 */
TEST_F(VectorTest, EuclideanDistance_Identical)
{
    float a[] = {1.0f, 2.0f, 3.0f};
    float b[] = {1.0f, 2.0f, 3.0f};

    float result = nimcp_vector_euclidean_distance(a, b, 3);
    EXPECT_TRUE(FloatEquals(result, 0.0f));
}

/**
 * WHAT: Test Euclidean distance with null inputs
 * WHY: Verify null safety
 */
TEST_F(VectorTest, EuclideanDistance_NullInputs)
{
    float a[] = {1.0f, 2.0f};

    EXPECT_TRUE(FloatEquals(nimcp_vector_euclidean_distance(nullptr, a, 2), 0.0f));
    EXPECT_TRUE(FloatEquals(nimcp_vector_euclidean_distance(a, nullptr, 2), 0.0f));
}

//=============================================================================
// Normalization Tests
//=============================================================================

/**
 * WHAT: Test L2 normalization to unit vector
 * WHY: Verify vector is scaled to target norm
 */
TEST_F(VectorTest, NormalizeL2_ToUnit)
{
    float vec[] = {3.0f, 4.0f};

    float original_norm = nimcp_vector_normalize_l2(vec, 2, 1.0f);

    // Original norm should be 5.0
    EXPECT_TRUE(FloatEquals(original_norm, 5.0f));

    // New norm should be 1.0
    float new_norm = nimcp_vector_norm_l2(vec, 2);
    EXPECT_TRUE(FloatEquals(new_norm, 1.0f));

    // Direction should be preserved (3:4 ratio)
    EXPECT_TRUE(FloatEquals(vec[0], 0.6f));
    EXPECT_TRUE(FloatEquals(vec[1], 0.8f));
}

/**
 * WHAT: Test L2 normalization to custom target
 * WHY: Verify normalization to arbitrary target norm
 */
TEST_F(VectorTest, NormalizeL2_CustomTarget)
{
    float vec[] = {1.0f, 1.0f};

    float original_norm = nimcp_vector_normalize_l2(vec, 2, 10.0f);

    // Original norm should be sqrt(2)
    EXPECT_TRUE(FloatEquals(original_norm, std::sqrt(2.0f)));

    // New norm should be 10.0
    float new_norm = nimcp_vector_norm_l2(vec, 2);
    EXPECT_TRUE(FloatEquals(new_norm, 10.0f));
}

/**
 * WHAT: Test L2 normalization of zero vector
 * WHY: Verify zero vector is not modified
 */
TEST_F(VectorTest, NormalizeL2_ZeroVector)
{
    float vec[] = {0.0f, 0.0f, 0.0f};

    float original_norm = nimcp_vector_normalize_l2(vec, 3, 1.0f);

    // Should return 0 and leave vector unchanged
    EXPECT_TRUE(FloatEquals(original_norm, 0.0f));
    EXPECT_TRUE(FloatEquals(vec[0], 0.0f));
    EXPECT_TRUE(FloatEquals(vec[1], 0.0f));
    EXPECT_TRUE(FloatEquals(vec[2], 0.0f));
}

/**
 * WHAT: Test L2 normalization with null input
 * WHY: Verify null safety
 */
TEST_F(VectorTest, NormalizeL2_NullInput)
{
    float result = nimcp_vector_normalize_l2(nullptr, 3, 1.0f);
    EXPECT_TRUE(FloatEquals(result, 0.0f));
}

/**
 * WHAT: Test L1 normalization
 * WHY: Verify L1 norm scaling
 */
TEST_F(VectorTest, NormalizeL1_Simple)
{
    float vec[] = {1.0f, 2.0f, 3.0f};

    float original_norm = nimcp_vector_normalize_l1(vec, 3, 1.0f);

    // Original norm should be 6.0
    EXPECT_TRUE(FloatEquals(original_norm, 6.0f));

    // New L1 norm should be 1.0
    float new_norm = nimcp_vector_norm_l1(vec, 3);
    EXPECT_TRUE(FloatEquals(new_norm, 1.0f));

    // Values should be scaled by 1/6
    EXPECT_TRUE(FloatEquals(vec[0], 1.0f / 6.0f));
    EXPECT_TRUE(FloatEquals(vec[1], 2.0f / 6.0f));
    EXPECT_TRUE(FloatEquals(vec[2], 3.0f / 6.0f));
}

/**
 * WHAT: Test L1 normalization with negative values
 * WHY: Verify signs are preserved during normalization
 */
TEST_F(VectorTest, NormalizeL1_NegativeValues)
{
    float vec[] = {-1.0f, 2.0f, -3.0f};

    float original_norm = nimcp_vector_normalize_l1(vec, 3, 1.0f);

    // Original norm should be 6.0
    EXPECT_TRUE(FloatEquals(original_norm, 6.0f));

    // Signs should be preserved
    EXPECT_LT(vec[0], 0.0f);
    EXPECT_GT(vec[1], 0.0f);
    EXPECT_LT(vec[2], 0.0f);
}

/**
 * WHAT: Test L1 normalization of zero vector
 * WHY: Verify zero vector is not modified
 */
TEST_F(VectorTest, NormalizeL1_ZeroVector)
{
    float vec[] = {0.0f, 0.0f, 0.0f};

    float original_norm = nimcp_vector_normalize_l1(vec, 3, 1.0f);

    // Should return 0 and leave vector unchanged
    EXPECT_TRUE(FloatEquals(original_norm, 0.0f));
    EXPECT_TRUE(FloatEquals(vec[0], 0.0f));
    EXPECT_TRUE(FloatEquals(vec[1], 0.0f));
    EXPECT_TRUE(FloatEquals(vec[2], 0.0f));
}

//=============================================================================
// Edge Cases and Stress Tests
//=============================================================================

/**
 * WHAT: Test operations with very small values
 * WHY: Verify numerical stability with values near epsilon
 */
TEST_F(VectorTest, NumericalStability_SmallValues)
{
    float vec[] = {1e-11f, 1e-11f, 1e-11f};

    // This should be treated as zero and not normalized
    float norm = nimcp_vector_normalize_l2(vec, 3, 1.0f);
    EXPECT_TRUE(FloatEquals(norm, 0.0f));
}

/**
 * WHAT: Test operations with large vectors
 * WHY: Verify performance with realistic vector sizes
 */
TEST_F(VectorTest, Performance_LargeVector)
{
    const int size = 1024;
    float a[size], b[size];

    for (int i = 0; i < size; i++) {
        a[i] = static_cast<float>(i);
        b[i] = static_cast<float>(i);
    }

    float dot = nimcp_vector_dot_product(a, b, size);
    EXPECT_GT(dot, 0.0f);

    float similarity = nimcp_vector_cosine_similarity(a, b, size);
    EXPECT_TRUE(FloatEquals(similarity, 1.0f));
}

/**
 * WHAT: Test vector with mixed positive and negative values
 * WHY: Verify operations handle sign correctly
 */
TEST_F(VectorTest, MixedSigns_Operations)
{
    float a[] = {1.0f, -2.0f, 3.0f, -4.0f};
    float b[] = {-1.0f, 2.0f, -3.0f, 4.0f};

    // Dot product: 1*(-1) + (-2)*2 + 3*(-3) + (-4)*4 = -1 - 4 - 9 - 16 = -30
    float dot = nimcp_vector_dot_product(a, b, 4);
    EXPECT_TRUE(FloatEquals(dot, -30.0f));

    // Should be opposite direction (negative similarity)
    float similarity = nimcp_vector_cosine_similarity(a, b, 4);
    EXPECT_LT(similarity, 0.0f);
}
