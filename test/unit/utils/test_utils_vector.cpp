/**
 * @file test_utils_vector.cpp
 * @brief Comprehensive unit tests for vector mathematics utilities
 *
 * WHAT: 100% test coverage for nimcp_vector.c
 * WHY:  Vector operations are critical for neural network computations
 * HOW:  Test all operations, edge cases, numerical stability, and correctness
 *
 * TEST COVERAGE:
 * 1. nimcp_vector_dot_product() - basic and edge cases
 * 2. nimcp_vector_norm_l2() - L2 (Euclidean) norm
 * 3. nimcp_vector_norm_l1() - L1 (Manhattan) norm
 * 4. nimcp_vector_copy() - vector copying
 * 5. nimcp_vector_cosine_similarity() - similarity metric
 * 6. nimcp_vector_cosine_distance() - distance metric
 * 7. nimcp_vector_euclidean_distance() - Euclidean distance
 * 8. nimcp_vector_normalize_l2() - L2 normalization
 * 9. nimcp_vector_normalize_l1() - L1 normalization
 * 10. Edge cases (NULL pointers, zero size, zero vectors)
 * 11. Numerical stability (epsilon guards)
 * 12. Orthogonal and parallel vectors
 * 13. Negative values handling
 * 14. Large vector stress test
 * 15. Mixed positive/negative values
 *
 * @version Unit Testing Framework v1.0
 * @date 2025-11-13
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

    #include "utils/containers/nimcp_vector.h"

//=============================================================================
// Test Fixture
//=============================================================================

class VectorTest : public ::testing::Test {
protected:
    static constexpr float EPSILON = 1e-5f;

    // Helper function to compare floats with tolerance
    void ExpectFloatNear(float actual, float expected, const char* msg = "") {
        EXPECT_NEAR(actual, expected, EPSILON) << msg;
    }

    // Helper to verify vector contents
    void ExpectVectorNear(const float* actual, const float* expected, uint32_t size, const char* msg = "") {
        for (uint32_t i = 0; i < size; i++) {
            EXPECT_NEAR(actual[i], expected[i], EPSILON)
                << msg << " at index " << i;
        }
    }
};

//=============================================================================
// Unit Test 1: Dot product - basic operation
//=============================================================================

TEST_F(VectorTest, DotProduct_BasicOperation) {
    // WHAT: Compute dot product of two vectors
    // WHY:  Fundamental vector operation
    // HOW:  [1,2,3] · [4,5,6] = 1*4 + 2*5 + 3*6 = 32

    float a[] = {1.0f, 2.0f, 3.0f};
    float b[] = {4.0f, 5.0f, 6.0f};

    float result = nimcp_vector_dot_product(a, b, 3);
    ExpectFloatNear(result, 32.0f, "Dot product calculation");

    SUCCEED() << "Basic dot product works";
}

//=============================================================================
// Unit Test 2: Dot product - orthogonal vectors
//=============================================================================

TEST_F(VectorTest, DotProduct_OrthogonalVectors) {
    // WHAT: Dot product of orthogonal vectors should be zero
    // WHY:  Test perpendicular vectors
    // HOW:  [1,0,0] · [0,1,0] = 0

    float a[] = {1.0f, 0.0f, 0.0f};
    float b[] = {0.0f, 1.0f, 0.0f};

    float result = nimcp_vector_dot_product(a, b, 3);
    ExpectFloatNear(result, 0.0f, "Orthogonal vectors dot product");

    SUCCEED() << "Orthogonal vectors have zero dot product";
}

//=============================================================================
// Unit Test 3: Dot product - NULL and edge cases
//=============================================================================

TEST_F(VectorTest, DotProduct_NullAndEdgeCases) {
    // WHAT: Test NULL pointers and zero size
    // WHY:  Defensive programming
    // HOW:  Should return 0.0f for invalid inputs

    float a[] = {1.0f, 2.0f, 3.0f};
    float b[] = {4.0f, 5.0f, 6.0f};

    // NULL first vector
    EXPECT_FLOAT_EQ(nimcp_vector_dot_product(nullptr, b, 3), 0.0f);

    // NULL second vector
    EXPECT_FLOAT_EQ(nimcp_vector_dot_product(a, nullptr, 3), 0.0f);

    // Both NULL
    EXPECT_FLOAT_EQ(nimcp_vector_dot_product(nullptr, nullptr, 3), 0.0f);

    // Zero size
    EXPECT_FLOAT_EQ(nimcp_vector_dot_product(a, b, 0), 0.0f);

    SUCCEED() << "Dot product handles NULL and edge cases";
}

//=============================================================================
// Unit Test 4: L2 norm - basic operation
//=============================================================================

TEST_F(VectorTest, NormL2_BasicOperation) {
    // WHAT: Compute L2 (Euclidean) norm
    // WHY:  Used for magnitude calculation
    // HOW:  ||[3,4]|| = sqrt(9+16) = 5

    float vec[] = {3.0f, 4.0f};
    float result = nimcp_vector_norm_l2(vec, 2);
    ExpectFloatNear(result, 5.0f, "L2 norm calculation");

    SUCCEED() << "L2 norm works correctly";
}

//=============================================================================
// Unit Test 5: L2 norm - unit vector
//=============================================================================

TEST_F(VectorTest, NormL2_UnitVector) {
    // WHAT: L2 norm of unit vector should be 1.0
    // WHY:  Verify normalized vectors
    // HOW:  ||[1/sqrt(2), 1/sqrt(2)]|| = 1

    float vec[] = {0.707106781f, 0.707106781f};  // 1/sqrt(2)
    float result = nimcp_vector_norm_l2(vec, 2);
    ExpectFloatNear(result, 1.0f, "Unit vector L2 norm");

    SUCCEED() << "Unit vector has norm 1.0";
}

//=============================================================================
// Unit Test 6: L2 norm - zero vector
//=============================================================================

TEST_F(VectorTest, NormL2_ZeroVector) {
    // WHAT: L2 norm of zero vector should be 0.0
    // WHY:  Edge case handling
    // HOW:  ||[0,0,0]|| = 0

    float vec[] = {0.0f, 0.0f, 0.0f};
    float result = nimcp_vector_norm_l2(vec, 3);
    ExpectFloatNear(result, 0.0f, "Zero vector L2 norm");

    SUCCEED() << "Zero vector has norm 0.0";
}

//=============================================================================
// Unit Test 7: L2 norm - NULL and edge cases
//=============================================================================

TEST_F(VectorTest, NormL2_NullAndEdgeCases) {
    // WHAT: Test NULL pointer and zero size
    // WHY:  Defensive programming
    // HOW:  Should return 0.0f for invalid inputs

    float vec[] = {1.0f, 2.0f, 3.0f};

    // NULL vector
    EXPECT_FLOAT_EQ(nimcp_vector_norm_l2(nullptr, 3), 0.0f);

    // Zero size
    EXPECT_FLOAT_EQ(nimcp_vector_norm_l2(vec, 0), 0.0f);

    SUCCEED() << "L2 norm handles NULL and edge cases";
}

//=============================================================================
// Unit Test 8: L1 norm - basic operation
//=============================================================================

TEST_F(VectorTest, NormL1_BasicOperation) {
    // WHAT: Compute L1 (Manhattan) norm
    // WHY:  Sum of absolute values
    // HOW:  ||[1,2,3]||_1 = |1| + |2| + |3| = 6

    float vec[] = {1.0f, 2.0f, 3.0f};
    float result = nimcp_vector_norm_l1(vec, 3);
    ExpectFloatNear(result, 6.0f, "L1 norm calculation");

    SUCCEED() << "L1 norm works correctly";
}

//=============================================================================
// Unit Test 9: L1 norm - negative values
//=============================================================================

TEST_F(VectorTest, NormL1_NegativeValues) {
    // WHAT: L1 norm with negative values
    // WHY:  Test absolute value handling
    // HOW:  ||[-1,2,-3]||_1 = |-1| + |2| + |-3| = 6

    float vec[] = {-1.0f, 2.0f, -3.0f};
    float result = nimcp_vector_norm_l1(vec, 3);
    ExpectFloatNear(result, 6.0f, "L1 norm with negative values");

    SUCCEED() << "L1 norm handles negative values correctly";
}

//=============================================================================
// Unit Test 10: L1 norm - NULL and edge cases
//=============================================================================

TEST_F(VectorTest, NormL1_NullAndEdgeCases) {
    // WHAT: Test NULL pointer and zero size
    // WHY:  Defensive programming
    // HOW:  Should return 0.0f for invalid inputs

    float vec[] = {1.0f, 2.0f, 3.0f};

    // NULL vector
    EXPECT_FLOAT_EQ(nimcp_vector_norm_l1(nullptr, 3), 0.0f);

    // Zero size
    EXPECT_FLOAT_EQ(nimcp_vector_norm_l1(vec, 0), 0.0f);

    SUCCEED() << "L1 norm handles NULL and edge cases";
}

//=============================================================================
// Unit Test 11: Vector copy - basic operation
//=============================================================================

TEST_F(VectorTest, Copy_BasicOperation) {
    // WHAT: Copy vector from source to destination
    // WHY:  Test vector duplication
    // HOW:  Copy and verify contents match

    float src[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    float dst[5] = {0.0f};

    nimcp_vector_copy(src, dst, 5);

    ExpectVectorNear(dst, src, 5, "Vector copy");

    SUCCEED() << "Vector copy works correctly";
}

//=============================================================================
// Unit Test 12: Vector copy - NULL and edge cases
//=============================================================================

TEST_F(VectorTest, Copy_NullAndEdgeCases) {
    // WHAT: Test NULL pointers and zero size
    // WHY:  Defensive programming
    // HOW:  Should handle gracefully without crash

    float src[] = {1.0f, 2.0f, 3.0f};
    float dst[3] = {0.0f};

    // NULL source (should not crash)
    nimcp_vector_copy(nullptr, dst, 3);

    // NULL destination (should not crash)
    nimcp_vector_copy(src, nullptr, 3);

    // Zero size (should not crash)
    nimcp_vector_copy(src, dst, 0);

    SUCCEED() << "Vector copy handles NULL and edge cases";
}

//=============================================================================
// Unit Test 13: Cosine similarity - identical vectors
//=============================================================================

TEST_F(VectorTest, CosineSimilarity_IdenticalVectors) {
    // WHAT: Cosine similarity of identical vectors should be 1.0
    // WHY:  Test perfect match
    // HOW:  Same vector compared to itself

    float a[] = {1.0f, 2.0f, 3.0f};
    float b[] = {1.0f, 2.0f, 3.0f};

    float result = nimcp_vector_cosine_similarity(a, b, 3);
    ExpectFloatNear(result, 1.0f, "Identical vectors similarity");

    SUCCEED() << "Identical vectors have similarity 1.0";
}

//=============================================================================
// Unit Test 14: Cosine similarity - orthogonal vectors
//=============================================================================

TEST_F(VectorTest, CosineSimilarity_OrthogonalVectors) {
    // WHAT: Cosine similarity of orthogonal vectors should be 0.0
    // WHY:  Test perpendicular vectors
    // HOW:  [1,0,0] vs [0,1,0]

    float a[] = {1.0f, 0.0f, 0.0f};
    float b[] = {0.0f, 1.0f, 0.0f};

    float result = nimcp_vector_cosine_similarity(a, b, 3);
    ExpectFloatNear(result, 0.0f, "Orthogonal vectors similarity");

    SUCCEED() << "Orthogonal vectors have similarity 0.0";
}

//=============================================================================
// Unit Test 15: Cosine similarity - opposite vectors
//=============================================================================

TEST_F(VectorTest, CosineSimilarity_OppositeVectors) {
    // WHAT: Cosine similarity of opposite vectors should be -1.0
    // WHY:  Test anti-parallel vectors
    // HOW:  [1,2,3] vs [-1,-2,-3]

    float a[] = {1.0f, 2.0f, 3.0f};
    float b[] = {-1.0f, -2.0f, -3.0f};

    float result = nimcp_vector_cosine_similarity(a, b, 3);
    ExpectFloatNear(result, -1.0f, "Opposite vectors similarity");

    SUCCEED() << "Opposite vectors have similarity -1.0";
}

//=============================================================================
// Unit Test 16: Cosine similarity - zero vectors
//=============================================================================

TEST_F(VectorTest, CosineSimilarity_ZeroVectors) {
    // WHAT: Test zero vector handling
    // WHY:  Numerical stability edge case
    // HOW:  Both zero = 1.0, one zero = 0.0 (per spec)

    float zero[] = {0.0f, 0.0f, 0.0f};
    float nonzero[] = {1.0f, 2.0f, 3.0f};

    // Both zero = perfect match
    float result1 = nimcp_vector_cosine_similarity(zero, zero, 3);
    ExpectFloatNear(result1, 1.0f, "Both vectors zero");

    // One zero = no similarity
    float result2 = nimcp_vector_cosine_similarity(zero, nonzero, 3);
    ExpectFloatNear(result2, 0.0f, "One vector zero");

    float result3 = nimcp_vector_cosine_similarity(nonzero, zero, 3);
    ExpectFloatNear(result3, 0.0f, "One vector zero (reversed)");

    SUCCEED() << "Zero vector edge cases handled correctly";
}

//=============================================================================
// Unit Test 17: Cosine similarity - NULL and edge cases
//=============================================================================

TEST_F(VectorTest, CosineSimilarity_NullAndEdgeCases) {
    // WHAT: Test NULL pointers and zero size
    // WHY:  Defensive programming
    // HOW:  Should return 0.0f for invalid inputs

    float a[] = {1.0f, 2.0f, 3.0f};
    float b[] = {4.0f, 5.0f, 6.0f};

    // NULL first vector
    EXPECT_FLOAT_EQ(nimcp_vector_cosine_similarity(nullptr, b, 3), 0.0f);

    // NULL second vector
    EXPECT_FLOAT_EQ(nimcp_vector_cosine_similarity(a, nullptr, 3), 0.0f);

    // Zero size
    EXPECT_FLOAT_EQ(nimcp_vector_cosine_similarity(a, b, 0), 0.0f);

    SUCCEED() << "Cosine similarity handles NULL and edge cases";
}

//=============================================================================
// Unit Test 18: Cosine distance - identical vectors
//=============================================================================

TEST_F(VectorTest, CosineDistance_IdenticalVectors) {
    // WHAT: Cosine distance = 1.0 - similarity
    // WHY:  Test distance metric
    // HOW:  Identical vectors should have distance 0.0

    float a[] = {1.0f, 2.0f, 3.0f};
    float b[] = {1.0f, 2.0f, 3.0f};

    float result = nimcp_vector_cosine_distance(a, b, 3);
    ExpectFloatNear(result, 0.0f, "Identical vectors distance");

    SUCCEED() << "Identical vectors have distance 0.0";
}

//=============================================================================
// Unit Test 19: Cosine distance - orthogonal vectors
//=============================================================================

TEST_F(VectorTest, CosineDistance_OrthogonalVectors) {
    // WHAT: Cosine distance of orthogonal vectors
    // WHY:  Should be 1.0 (since similarity is 0.0)
    // HOW:  [1,0] vs [0,1]

    float a[] = {1.0f, 0.0f};
    float b[] = {0.0f, 1.0f};

    float result = nimcp_vector_cosine_distance(a, b, 2);
    ExpectFloatNear(result, 1.0f, "Orthogonal vectors distance");

    SUCCEED() << "Orthogonal vectors have distance 1.0";
}

//=============================================================================
// Unit Test 20: Cosine distance - opposite vectors
//=============================================================================

TEST_F(VectorTest, CosineDistance_OppositeVectors) {
    // WHAT: Cosine distance of opposite vectors
    // WHY:  Should be 2.0 (since similarity is -1.0)
    // HOW:  [1,2,3] vs [-1,-2,-3]

    float a[] = {1.0f, 2.0f, 3.0f};
    float b[] = {-1.0f, -2.0f, -3.0f};

    float result = nimcp_vector_cosine_distance(a, b, 3);
    ExpectFloatNear(result, 2.0f, "Opposite vectors distance");

    SUCCEED() << "Opposite vectors have distance 2.0";
}

//=============================================================================
// Unit Test 21: Euclidean distance - basic operation
//=============================================================================

TEST_F(VectorTest, EuclideanDistance_BasicOperation) {
    // WHAT: Compute Euclidean distance
    // WHY:  Standard distance metric
    // HOW:  dist([0,0], [3,4]) = sqrt((3-0)^2 + (4-0)^2) = 5

    float a[] = {0.0f, 0.0f};
    float b[] = {3.0f, 4.0f};

    float result = nimcp_vector_euclidean_distance(a, b, 2);
    ExpectFloatNear(result, 5.0f, "Euclidean distance calculation");

    SUCCEED() << "Euclidean distance works correctly";
}

//=============================================================================
// Unit Test 22: Euclidean distance - identical vectors
//=============================================================================

TEST_F(VectorTest, EuclideanDistance_IdenticalVectors) {
    // WHAT: Distance between identical vectors should be 0.0
    // WHY:  Test zero distance case
    // HOW:  Same vector compared to itself

    float a[] = {1.0f, 2.0f, 3.0f};
    float b[] = {1.0f, 2.0f, 3.0f};

    float result = nimcp_vector_euclidean_distance(a, b, 3);
    ExpectFloatNear(result, 0.0f, "Identical vectors Euclidean distance");

    SUCCEED() << "Identical vectors have Euclidean distance 0.0";
}

//=============================================================================
// Unit Test 23: Euclidean distance - NULL and edge cases
//=============================================================================

TEST_F(VectorTest, EuclideanDistance_NullAndEdgeCases) {
    // WHAT: Test NULL pointers and zero size
    // WHY:  Defensive programming
    // HOW:  Should return 0.0f for invalid inputs

    float a[] = {1.0f, 2.0f, 3.0f};
    float b[] = {4.0f, 5.0f, 6.0f};

    // NULL first vector
    EXPECT_FLOAT_EQ(nimcp_vector_euclidean_distance(nullptr, b, 3), 0.0f);

    // NULL second vector
    EXPECT_FLOAT_EQ(nimcp_vector_euclidean_distance(a, nullptr, 3), 0.0f);

    // Zero size
    EXPECT_FLOAT_EQ(nimcp_vector_euclidean_distance(a, b, 0), 0.0f);

    SUCCEED() << "Euclidean distance handles NULL and edge cases";
}

//=============================================================================
// Unit Test 24: L2 normalization - basic operation
//=============================================================================

TEST_F(VectorTest, NormalizeL2_BasicOperation) {
    // WHAT: Normalize vector to unit length
    // WHY:  Common operation for neural networks
    // HOW:  [3,4] -> [0.6, 0.8]

    float vec[] = {3.0f, 4.0f};
    float expected[] = {0.6f, 0.8f};

    float original_norm = nimcp_vector_normalize_l2(vec, 2, 1.0f);

    ExpectFloatNear(original_norm, 5.0f, "Original norm");
    ExpectVectorNear(vec, expected, 2, "Normalized vector");

    // Verify new norm is 1.0
    float new_norm = nimcp_vector_norm_l2(vec, 2);
    ExpectFloatNear(new_norm, 1.0f, "New norm after normalization");

    SUCCEED() << "L2 normalization works correctly";
}

//=============================================================================
// Unit Test 25: L2 normalization - custom target norm
//=============================================================================

TEST_F(VectorTest, NormalizeL2_CustomTargetNorm) {
    // WHAT: Normalize to custom target norm
    // WHY:  Test non-unit normalization
    // HOW:  Normalize to norm 10.0

    float vec[] = {3.0f, 4.0f};

    nimcp_vector_normalize_l2(vec, 2, 10.0f);

    float new_norm = nimcp_vector_norm_l2(vec, 2);
    ExpectFloatNear(new_norm, 10.0f, "Custom target norm");

    SUCCEED() << "L2 normalization with custom target works";
}

//=============================================================================
// Unit Test 26: L2 normalization - zero vector
//=============================================================================

TEST_F(VectorTest, NormalizeL2_ZeroVector) {
    // WHAT: Normalizing zero vector should be no-op
    // WHY:  Edge case - can't normalize zero vector
    // HOW:  Returns 0.0, leaves vector unchanged

    float vec[] = {0.0f, 0.0f, 0.0f};
    float original[] = {0.0f, 0.0f, 0.0f};

    float result = nimcp_vector_normalize_l2(vec, 3, 1.0f);

    ExpectFloatNear(result, 0.0f, "Zero vector normalization return value");
    ExpectVectorNear(vec, original, 3, "Zero vector unchanged");

    SUCCEED() << "Zero vector normalization handled correctly";
}

//=============================================================================
// Unit Test 27: L2 normalization - NULL and edge cases
//=============================================================================

TEST_F(VectorTest, NormalizeL2_NullAndEdgeCases) {
    // WHAT: Test NULL pointer and zero size
    // WHY:  Defensive programming
    // HOW:  Should return 0.0f for invalid inputs

    // NULL vector
    EXPECT_FLOAT_EQ(nimcp_vector_normalize_l2(nullptr, 3, 1.0f), 0.0f);

    // Zero size
    float vec[] = {1.0f, 2.0f, 3.0f};
    EXPECT_FLOAT_EQ(nimcp_vector_normalize_l2(vec, 0, 1.0f), 0.0f);

    SUCCEED() << "L2 normalization handles NULL and edge cases";
}

//=============================================================================
// Unit Test 28: L1 normalization - basic operation
//=============================================================================

TEST_F(VectorTest, NormalizeL1_BasicOperation) {
    // WHAT: Normalize vector to L1 norm
    // WHY:  Sum of absolute values normalization
    // HOW:  [1,2,3] (L1=6) -> [0.166..., 0.333..., 0.5]

    float vec[] = {1.0f, 2.0f, 3.0f};
    float expected[] = {1.0f/6.0f, 2.0f/6.0f, 3.0f/6.0f};

    float original_norm = nimcp_vector_normalize_l1(vec, 3, 1.0f);

    ExpectFloatNear(original_norm, 6.0f, "Original L1 norm");
    ExpectVectorNear(vec, expected, 3, "Normalized vector");

    // Verify new L1 norm is 1.0
    float new_norm = nimcp_vector_norm_l1(vec, 3);
    ExpectFloatNear(new_norm, 1.0f, "New L1 norm after normalization");

    SUCCEED() << "L1 normalization works correctly";
}

//=============================================================================
// Unit Test 29: L1 normalization - negative values
//=============================================================================

TEST_F(VectorTest, NormalizeL1_NegativeValues) {
    // WHAT: L1 normalization with negative values
    // WHY:  Test sign preservation
    // HOW:  [-2, 4] (L1=6) -> [-0.333..., 0.666...]

    float vec[] = {-2.0f, 4.0f};

    float original_norm = nimcp_vector_normalize_l1(vec, 2, 1.0f);

    ExpectFloatNear(original_norm, 6.0f, "Original L1 norm with negatives");

    // Check signs preserved
    EXPECT_LT(vec[0], 0.0f);
    EXPECT_GT(vec[1], 0.0f);

    // Verify new L1 norm is 1.0
    float new_norm = nimcp_vector_norm_l1(vec, 2);
    ExpectFloatNear(new_norm, 1.0f, "New L1 norm after normalization");

    SUCCEED() << "L1 normalization preserves signs";
}

//=============================================================================
// Unit Test 30: L1 normalization - zero vector
//=============================================================================

TEST_F(VectorTest, NormalizeL1_ZeroVector) {
    // WHAT: Normalizing zero vector should be no-op
    // WHY:  Edge case - can't normalize zero vector
    // HOW:  Returns 0.0, leaves vector unchanged

    float vec[] = {0.0f, 0.0f, 0.0f};
    float original[] = {0.0f, 0.0f, 0.0f};

    float result = nimcp_vector_normalize_l1(vec, 3, 1.0f);

    ExpectFloatNear(result, 0.0f, "Zero vector L1 normalization return value");
    ExpectVectorNear(vec, original, 3, "Zero vector unchanged");

    SUCCEED() << "Zero vector L1 normalization handled correctly";
}

//=============================================================================
// Unit Test 31: L1 normalization - NULL and edge cases
//=============================================================================

TEST_F(VectorTest, NormalizeL1_NullAndEdgeCases) {
    // WHAT: Test NULL pointer and zero size
    // WHY:  Defensive programming
    // HOW:  Should return 0.0f for invalid inputs

    // NULL vector
    EXPECT_FLOAT_EQ(nimcp_vector_normalize_l1(nullptr, 3, 1.0f), 0.0f);

    // Zero size
    float vec[] = {1.0f, 2.0f, 3.0f};
    EXPECT_FLOAT_EQ(nimcp_vector_normalize_l1(vec, 0, 1.0f), 0.0f);

    SUCCEED() << "L1 normalization handles NULL and edge cases";
}

//=============================================================================
// Unit Test 32: Stress test - large vectors
//=============================================================================

TEST_F(VectorTest, StressTest_LargeVectors) {
    // WHAT: Test operations on large vectors
    // WHY:  Verify stability and performance
    // HOW:  1000-element vectors

    const uint32_t SIZE = 1000;
    std::vector<float> a(SIZE);
    std::vector<float> b(SIZE);

    // Initialize with known pattern
    for (uint32_t i = 0; i < SIZE; i++) {
        a[i] = (float)i;
        b[i] = (float)(SIZE - i);
    }

    // Test dot product doesn't crash
    float dot = nimcp_vector_dot_product(a.data(), b.data(), SIZE);
    EXPECT_TRUE(std::isfinite(dot));

    // Test norms don't crash
    float norm_a = nimcp_vector_norm_l2(a.data(), SIZE);
    float norm_b = nimcp_vector_norm_l1(b.data(), SIZE);
    EXPECT_TRUE(std::isfinite(norm_a));
    EXPECT_TRUE(std::isfinite(norm_b));

    // Test similarity doesn't crash
    float sim = nimcp_vector_cosine_similarity(a.data(), b.data(), SIZE);
    EXPECT_TRUE(std::isfinite(sim));
    EXPECT_GE(sim, -1.0f);
    EXPECT_LE(sim, 1.0f);

    SUCCEED() << "Stress test with large vectors passed";
}

//=============================================================================
// Unit Test 33: Mixed positive and negative values
//=============================================================================

TEST_F(VectorTest, MixedValues_CorrectCalculations) {
    // WHAT: Test with mixed positive/negative values
    // WHY:  Real-world data often has mixed signs
    // HOW:  Verify all operations handle signs correctly

    float a[] = {-1.0f, 2.0f, -3.0f, 4.0f};
    float b[] = {5.0f, -6.0f, 7.0f, -8.0f};

    // Dot product
    float dot = nimcp_vector_dot_product(a, b, 4);
    // -1*5 + 2*(-6) + (-3)*7 + 4*(-8) = -5 - 12 - 21 - 32 = -70
    ExpectFloatNear(dot, -70.0f, "Dot product with mixed signs");

    // L2 norm
    float norm_l2 = nimcp_vector_norm_l2(a, 4);
    // sqrt(1 + 4 + 9 + 16) = sqrt(30)
    ExpectFloatNear(norm_l2, sqrtf(30.0f), "L2 norm with mixed signs");

    // L1 norm
    float norm_l1 = nimcp_vector_norm_l1(a, 4);
    // |−1| + |2| + |−3| + |4| = 10
    ExpectFloatNear(norm_l1, 10.0f, "L1 norm with mixed signs");

    SUCCEED() << "Mixed positive/negative values handled correctly";
}

//=============================================================================
// Unit Test 34: Numerical stability - very small values
//=============================================================================

TEST_F(VectorTest, NumericalStability_SmallValues) {
    // WHAT: Test with very small values
    // WHY:  Verify epsilon guards work
    // HOW:  Use values near zero

    float a[] = {1e-20f, 1e-20f, 1e-20f};
    float b[] = {1.0f, 2.0f, 3.0f};

    // Cosine similarity with tiny vector
    float sim = nimcp_vector_cosine_similarity(a, b, 3);
    EXPECT_TRUE(std::isfinite(sim));
    ExpectFloatNear(sim, 0.0f, "Small vector treated as zero");

    // Normalization of tiny vector
    float tiny[] = {1e-20f, 1e-20f};
    float result = nimcp_vector_normalize_l2(tiny, 2, 1.0f);
    ExpectFloatNear(result, 0.0f, "Tiny vector normalization returns 0");

    SUCCEED() << "Numerical stability with small values verified";
}

//=============================================================================
// Unit Test 35: Comprehensive integration test
//=============================================================================

TEST_F(VectorTest, Integration_CombinedOperations) {
    // WHAT: Test multiple operations in sequence
    // WHY:  Verify operations work together correctly
    // HOW:  Copy, normalize, compute similarity

    float original[] = {3.0f, 4.0f, 5.0f};
    float vec1[3];
    float vec2[3];

    // Copy to two vectors
    nimcp_vector_copy(original, vec1, 3);
    nimcp_vector_copy(original, vec2, 3);

    // Normalize both
    nimcp_vector_normalize_l2(vec1, 3, 1.0f);
    nimcp_vector_normalize_l2(vec2, 3, 1.0f);

    // Should be identical after same normalization
    float sim = nimcp_vector_cosine_similarity(vec1, vec2, 3);
    ExpectFloatNear(sim, 1.0f, "Normalized copies are identical");

    // Distance should be zero
    float dist = nimcp_vector_euclidean_distance(vec1, vec2, 3);
    ExpectFloatNear(dist, 0.0f, "Normalized copies have zero distance");

    SUCCEED() << "Integration test passed";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
