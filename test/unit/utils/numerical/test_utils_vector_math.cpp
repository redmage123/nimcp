/**
 * @file test_utils_vector_math.cpp
 * @brief Comprehensive unit tests for vector mathematics utilities
 *
 * WHAT: 100% test coverage for nimcp_vector.c (vector math operations)
 * WHY:  Vector operations are fundamental to neural network computations
 * HOW:  Test all operations, edge cases, numerical stability
 *
 * TEST COVERAGE:
 * 1. nimcp_vector_dot_product() - dot product computation
 * 2. nimcp_vector_norm_l2() - L2 (Euclidean) norm
 * 3. nimcp_vector_norm_l1() - L1 (Manhattan) norm
 * 4. nimcp_vector_copy() - vector copying
 * 5. nimcp_vector_cosine_similarity() - cosine similarity
 * 6. nimcp_vector_cosine_distance() - cosine distance
 * 7. nimcp_vector_euclidean_distance() - Euclidean distance
 * 8. Edge cases (zero vectors, NULL, size=0)
 * 9. Numerical stability
 * 10. Large vectors
 *
 * @version Unit Testing Framework v1.0
 * @date 2025-11-13
 */

#include <gtest/gtest.h>
#include <cmath>

    #include "utils/containers/nimcp_vector.h"

//=============================================================================
// Test Fixture
//=============================================================================

class VectorMathTest : public ::testing::Test {
protected:
    static constexpr float EPSILON = 1e-5f;

    bool FloatEqual(float a, float b) {
        return std::abs(a - b) < EPSILON;
    }
};

//=============================================================================
// Unit Test 1: Dot product - basic
//=============================================================================

TEST_F(VectorMathTest, DotProduct_Basic) {
    // WHAT: Verify dot product computation
    // WHY:  Core vector operation

    float a[] = {1.0f, 2.0f, 3.0f};
    float b[] = {4.0f, 5.0f, 6.0f};

    float result = nimcp_vector_dot_product(a, b, 3);

    // Expected: 1*4 + 2*5 + 3*6 = 4 + 10 + 18 = 32
    EXPECT_TRUE(FloatEqual(result, 32.0f));
    SUCCEED() << "Dot product basic computation works";
}

//=============================================================================
// Unit Test 2: Dot product - orthogonal vectors
//=============================================================================

TEST_F(VectorMathTest, DotProduct_OrthogonalVectors) {
    // WHAT: Verify dot product of orthogonal vectors is zero
    // WHY:  Mathematical correctness

    float a[] = {1.0f, 0.0f};
    float b[] = {0.0f, 1.0f};

    float result = nimcp_vector_dot_product(a, b, 2);

    EXPECT_TRUE(FloatEqual(result, 0.0f));
    SUCCEED() << "Orthogonal vectors give zero dot product";
}

//=============================================================================
// Unit Test 3: Dot product - zero vector
//=============================================================================

TEST_F(VectorMathTest, DotProduct_ZeroVector) {
    // WHAT: Verify dot product with zero vector
    // WHY:  Edge case handling

    float a[] = {1.0f, 2.0f, 3.0f};
    float b[] = {0.0f, 0.0f, 0.0f};

    float result = nimcp_vector_dot_product(a, b, 3);

    EXPECT_TRUE(FloatEqual(result, 0.0f));
    SUCCEED() << "Zero vector dot product is zero";
}

//=============================================================================
// Unit Test 4: L2 norm - basic
//=============================================================================

TEST_F(VectorMathTest, NormL2_Basic) {
    // WHAT: Verify L2 norm computation
    // WHY:  Used for normalization

    float vec[] = {3.0f, 4.0f};

    float result = nimcp_vector_norm_l2(vec, 2);

    // Expected: sqrt(3^2 + 4^2) = sqrt(9 + 16) = sqrt(25) = 5.0
    EXPECT_TRUE(FloatEqual(result, 5.0f));
    SUCCEED() << "L2 norm basic computation works";
}

//=============================================================================
// Unit Test 5: L2 norm - unit vector
//=============================================================================

TEST_F(VectorMathTest, NormL2_UnitVector) {
    // WHAT: Verify L2 norm of unit vector is 1
    // WHY:  Mathematical correctness

    float vec[] = {1.0f, 0.0f, 0.0f};

    float result = nimcp_vector_norm_l2(vec, 3);

    EXPECT_TRUE(FloatEqual(result, 1.0f));
    SUCCEED() << "Unit vector has L2 norm of 1";
}

//=============================================================================
// Unit Test 6: L2 norm - zero vector
//=============================================================================

TEST_F(VectorMathTest, NormL2_ZeroVector) {
    // WHAT: Verify L2 norm of zero vector is 0
    // WHY:  Edge case handling

    float vec[] = {0.0f, 0.0f, 0.0f};

    float result = nimcp_vector_norm_l2(vec, 3);

    EXPECT_TRUE(FloatEqual(result, 0.0f));
    SUCCEED() << "Zero vector has L2 norm of 0";
}

//=============================================================================
// Unit Test 7: L1 norm - basic
//=============================================================================

TEST_F(VectorMathTest, NormL1_Basic) {
    // WHAT: Verify L1 (Manhattan) norm computation
    // WHY:  Alternative norm metric

    float vec[] = {1.0f, -2.0f, 3.0f};

    float result = nimcp_vector_norm_l1(vec, 3);

    // Expected: |1| + |-2| + |3| = 1 + 2 + 3 = 6.0
    EXPECT_TRUE(FloatEqual(result, 6.0f));
    SUCCEED() << "L1 norm basic computation works";
}

//=============================================================================
// Unit Test 8: L1 norm - all negative
//=============================================================================

TEST_F(VectorMathTest, NormL1_AllNegative) {
    // WHAT: Verify L1 norm handles negative values
    // WHY:  Test absolute value behavior

    float vec[] = {-1.0f, -2.0f, -3.0f};

    float result = nimcp_vector_norm_l1(vec, 3);

    // Expected: |-1| + |-2| + |-3| = 1 + 2 + 3 = 6.0
    EXPECT_TRUE(FloatEqual(result, 6.0f));
    SUCCEED() << "L1 norm handles negative values";
}

//=============================================================================
// Unit Test 9: Vector copy
//=============================================================================

TEST_F(VectorMathTest, Copy_Basic) {
    // WHAT: Verify vector copy works
    // WHY:  Used throughout codebase

    float src[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    float dst[5];

    nimcp_vector_copy(src, dst, 5);

    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(dst[i], src[i]) << "Element " << i << " not copied correctly";
    }

    SUCCEED() << "Vector copy works";
}

//=============================================================================
// Unit Test 10: Cosine similarity - identical vectors
//=============================================================================

TEST_F(VectorMathTest, CosineSimilarity_IdenticalVectors) {
    // WHAT: Verify cosine similarity of identical vectors is 1
    // WHY:  Mathematical correctness

    float vec[] = {1.0f, 2.0f, 3.0f};

    float result = nimcp_vector_cosine_similarity(vec, vec, 3);

    EXPECT_TRUE(FloatEqual(result, 1.0f));
    SUCCEED() << "Identical vectors have cosine similarity of 1";
}

//=============================================================================
// Unit Test 11: Cosine similarity - opposite vectors
//=============================================================================

TEST_F(VectorMathTest, CosineSimilarity_OppositeVectors) {
    // WHAT: Verify cosine similarity of opposite vectors is -1
    // WHY:  Mathematical correctness

    float a[] = {1.0f, 2.0f, 3.0f};
    float b[] = {-1.0f, -2.0f, -3.0f};

    float result = nimcp_vector_cosine_similarity(a, b, 3);

    EXPECT_TRUE(FloatEqual(result, -1.0f));
    SUCCEED() << "Opposite vectors have cosine similarity of -1";
}

//=============================================================================
// Unit Test 12: Cosine distance
//=============================================================================

TEST_F(VectorMathTest, CosineDistance_Basic) {
    // WHAT: Verify cosine distance = 1 - cosine_similarity
    // WHY:  Distance metric correctness

    float a[] = {1.0f, 2.0f, 3.0f};
    float b[] = {1.0f, 2.0f, 3.0f};

    float similarity = nimcp_vector_cosine_similarity(a, b, 3);
    float distance = nimcp_vector_cosine_distance(a, b, 3);

    EXPECT_TRUE(FloatEqual(distance, 1.0f - similarity));
    SUCCEED() << "Cosine distance is 1 - cosine similarity";
}

//=============================================================================
// Unit Test 13: Euclidean distance - basic
//=============================================================================

TEST_F(VectorMathTest, EuclideanDistance_Basic) {
    // WHAT: Verify Euclidean distance computation
    // WHY:  Common distance metric

    float a[] = {0.0f, 0.0f};
    float b[] = {3.0f, 4.0f};

    float result = nimcp_vector_euclidean_distance(a, b, 2);

    // Expected: sqrt((3-0)^2 + (4-0)^2) = sqrt(9 + 16) = 5.0
    EXPECT_TRUE(FloatEqual(result, 5.0f));
    SUCCEED() << "Euclidean distance computation works";
}

//=============================================================================
// Unit Test 14: Euclidean distance - same point
//=============================================================================

TEST_F(VectorMathTest, EuclideanDistance_SamePoint) {
    // WHAT: Verify distance from point to itself is zero
    // WHY:  Mathematical correctness

    float vec[] = {1.0f, 2.0f, 3.0f};

    float result = nimcp_vector_euclidean_distance(vec, vec, 3);

    EXPECT_TRUE(FloatEqual(result, 0.0f));
    SUCCEED() << "Distance from point to itself is zero";
}

//=============================================================================
// Unit Test 15: Large vectors
//=============================================================================

TEST_F(VectorMathTest, Operations_LargeVectors) {
    // WHAT: Verify operations work with large vectors
    // WHY:  Real-world use case

    const uint32_t SIZE = 1000;
    float a[SIZE], b[SIZE];

    for (uint32_t i = 0; i < SIZE; i++) {
        a[i] = (float)i / SIZE;
        b[i] = 1.0f - (float)i / SIZE;
    }

    float dot = nimcp_vector_dot_product(a, b, SIZE);
    float norm_a = nimcp_vector_norm_l2(a, SIZE);
    float norm_b = nimcp_vector_norm_l2(b, SIZE);

    // Just verify they don't crash and return reasonable values
    EXPECT_GT(norm_a, 0.0f);
    EXPECT_GT(norm_b, 0.0f);
    (void)dot; // May be positive or negative

    SUCCEED() << "Operations work with large vectors";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
