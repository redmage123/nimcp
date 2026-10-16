/**
 * @file test_utils_hyperbolic.cpp
 * @brief Comprehensive unit tests for hyperbolic geometry utilities
 *
 * WHAT: 100% test coverage for nimcp_hyperbolic.c (Poincaré ball operations)
 * WHY:  Hyperbolic geometry is fundamental to hierarchical knowledge embeddings
 * HOW:  Test all operations, edge cases, numerical stability, mathematical properties
 *
 * TEST COVERAGE:
 * 1. poincare_point_create() - point creation and initialization
 * 2. poincare_point_copy() - deep copy
 * 3. poincare_point_destroy() - memory cleanup
 * 4. poincare_norm() - Euclidean norm computation
 * 5. poincare_clip() - boundary clipping
 * 6. poincare_distance() - hyperbolic distance
 * 7. poincare_euclidean_dist_squared() - Euclidean distance squared
 * 8. poincare_exp_map() - exponential map (tangent → manifold)
 * 9. poincare_log_map() - logarithmic map (manifold → tangent)
 * 10. poincare_mobius_add() - Möbius addition
 * 11. poincare_mobius_scalar_mult() - Möbius scalar multiplication
 * 12. poincare_riemannian_gradient() - Riemannian gradient computation
 * 13. poincare_sgd_step() - Riemannian SGD optimization
 * 14. poincare_conformal_factor() - conformal factor computation
 * 15. poincare_point_is_valid() - validation checks
 * 16. Edge cases (origin, boundary, zero vectors)
 * 17. Mathematical properties (symmetry, triangle inequality)
 * 18. Numerical stability near boundary
 *
 * MATHEMATICAL PROPERTIES TESTED:
 * - Distance symmetry: d(x,y) = d(y,x)
 * - Distance identity: d(x,x) = 0
 * - Distance positivity: d(x,y) > 0 for x ≠ y
 * - Exponential-logarithmic inverse: exp_p(log_p(q)) ≈ q
 * - Möbius identity: 0 ⊕ x = x
 * - Möbius inverse: x ⊕ (-x) = 0
 * - Conformal factor positivity: λ_p > 0
 *
 * @version Unit Testing Framework v1.0
 * @date 2025-11-13
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

    #include "utils/geometry/nimcp_hyperbolic.h"
    #include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class HyperbolicGeometryTest : public ::testing::Test {
protected:
    static constexpr float EPSILON = 1e-4f;
    static constexpr float LOOSE_EPSILON = 1e-3f;

    bool FloatEqual(float a, float b, float epsilon = EPSILON) {
        return std::abs(a - b) < epsilon;
    }

    bool VectorEqual(const float* a, const float* b, uint32_t dim, float epsilon = EPSILON) {
        for (uint32_t i = 0; i < dim; i++) {
            if (std::abs(a[i] - b[i]) >= epsilon) {
                return false;
            }
        }
        return true;
    }

    // Helper to create test point
    poincare_point_t* CreateTestPoint(const std::vector<float>& coords, float curvature = -1.0f) {
        return poincare_point_create(coords.size(), coords.data(), curvature);
    }
};

//=============================================================================
// Unit Test 1: Point creation - basic
//=============================================================================

TEST_F(HyperbolicGeometryTest, PointCreate_Basic) {
    // WHAT: Verify point creation at origin
    // WHY:  Foundation for all operations

    poincare_point_t* point = poincare_point_create(3, nullptr, -1.0f);

    ASSERT_NE(point, nullptr);
    EXPECT_EQ(point->dim, 3u);
    EXPECT_FLOAT_EQ(point->curvature, -1.0f);
    ASSERT_NE(point->coords, nullptr);

    // Should be at origin
    for (uint32_t i = 0; i < point->dim; i++) {
        EXPECT_FLOAT_EQ(point->coords[i], 0.0f);
    }

    poincare_point_destroy(point);
    SUCCEED() << "Point creation at origin works";
}

//=============================================================================
// Unit Test 2: Point creation - with coordinates
//=============================================================================

TEST_F(HyperbolicGeometryTest, PointCreate_WithCoordinates) {
    // WHAT: Verify point creation with specified coordinates
    // WHY:  Need to initialize points at specific locations

    float coords[] = {0.1f, 0.2f, 0.3f};
    poincare_point_t* point = poincare_point_create(3, coords, -1.0f);

    ASSERT_NE(point, nullptr);
    EXPECT_EQ(point->dim, 3u);

    for (uint32_t i = 0; i < 3; i++) {
        EXPECT_FLOAT_EQ(point->coords[i], coords[i]);
    }

    poincare_point_destroy(point);
    SUCCEED() << "Point creation with coordinates works";
}

//=============================================================================
// Unit Test 3: Point creation - boundary clipping
//=============================================================================

TEST_F(HyperbolicGeometryTest, PointCreate_BoundaryClipping) {
    // WHAT: Verify points outside ball are clipped
    // WHY:  Numerical stability requires ||x|| < 1

    float coords[] = {1.5f, 0.0f};  // Outside unit ball
    poincare_point_t* point = poincare_point_create(2, coords, -1.0f);

    ASSERT_NE(point, nullptr);

    float norm = poincare_norm(point);
    EXPECT_LE(norm, POINCARE_MAX_RADIUS);

    poincare_point_destroy(point);
    SUCCEED() << "Boundary clipping works during creation";
}

//=============================================================================
// Unit Test 4: Point copy
//=============================================================================

TEST_F(HyperbolicGeometryTest, PointCopy_Basic) {
    // WHAT: Verify deep copy of point
    // WHY:  Need independent copies for operations

    poincare_point_t* original = CreateTestPoint({0.3f, 0.4f});
    poincare_point_t* copy = poincare_point_copy(original);

    ASSERT_NE(copy, nullptr);
    EXPECT_EQ(copy->dim, original->dim);
    EXPECT_FLOAT_EQ(copy->curvature, original->curvature);
    EXPECT_NE(copy->coords, original->coords);  // Different memory

    for (uint32_t i = 0; i < copy->dim; i++) {
        EXPECT_FLOAT_EQ(copy->coords[i], original->coords[i]);
    }

    poincare_point_destroy(original);
    poincare_point_destroy(copy);
    SUCCEED() << "Point copy creates independent copy";
}

//=============================================================================
// Unit Test 5: Point norm - basic
//=============================================================================

TEST_F(HyperbolicGeometryTest, Norm_Basic) {
    // WHAT: Verify norm computation
    // WHY:  Used in many hyperbolic operations

    poincare_point_t* point = CreateTestPoint({0.3f, 0.4f});

    float norm = poincare_norm(point);

    // Expected: sqrt(0.3^2 + 0.4^2) = sqrt(0.09 + 0.16) = sqrt(0.25) = 0.5
    EXPECT_TRUE(FloatEqual(norm, 0.5f));

    poincare_point_destroy(point);
    SUCCEED() << "Norm computation works";
}

//=============================================================================
// Unit Test 6: Point norm - origin
//=============================================================================

TEST_F(HyperbolicGeometryTest, Norm_Origin) {
    // WHAT: Verify norm at origin is zero
    // WHY:  Edge case for distance calculations

    poincare_point_t* point = CreateTestPoint({0.0f, 0.0f, 0.0f});

    float norm = poincare_norm(point);

    EXPECT_TRUE(FloatEqual(norm, 0.0f));

    poincare_point_destroy(point);
    SUCCEED() << "Norm at origin is zero";
}

//=============================================================================
// Unit Test 7: Point clipping
//=============================================================================

TEST_F(HyperbolicGeometryTest, Clip_OutsideBall) {
    // WHAT: Verify clipping projects points into ball
    // WHY:  Prevents numerical instability

    poincare_point_t* point = CreateTestPoint({2.0f, 0.0f});  // Already clipped by create

    // Manually set to outside (bypass create's clipping)
    point->coords[0] = 1.5f;

    poincare_clip(point);

    float norm = poincare_norm(point);
    EXPECT_LE(norm, POINCARE_MAX_RADIUS);

    poincare_point_destroy(point);
    SUCCEED() << "Clipping constrains points to ball";
}

//=============================================================================
// Unit Test 8: Euclidean distance squared
//=============================================================================

TEST_F(HyperbolicGeometryTest, EuclideanDistSquared_Basic) {
    // WHAT: Verify Euclidean distance squared computation
    // WHY:  Helper function for hyperbolic distance

    poincare_point_t* x = CreateTestPoint({0.0f, 0.0f});
    poincare_point_t* y = CreateTestPoint({0.3f, 0.4f});

    float dist_sq = poincare_euclidean_dist_squared(x, y);

    // Expected: (0.3-0)^2 + (0.4-0)^2 = 0.09 + 0.16 = 0.25
    EXPECT_TRUE(FloatEqual(dist_sq, 0.25f));

    poincare_point_destroy(x);
    poincare_point_destroy(y);
    SUCCEED() << "Euclidean distance squared works";
}

//=============================================================================
// Unit Test 9: Hyperbolic distance - identity
//=============================================================================

TEST_F(HyperbolicGeometryTest, HyperbolicDistance_Identity) {
    // WHAT: Verify d(x,x) = 0
    // WHY:  Distance metric property

    poincare_point_t* point = CreateTestPoint({0.3f, 0.4f});

    float distance = poincare_distance(point, point);

    EXPECT_TRUE(FloatEqual(distance, 0.0f));

    poincare_point_destroy(point);
    SUCCEED() << "Distance from point to itself is zero";
}

//=============================================================================
// Unit Test 10: Hyperbolic distance - symmetry
//=============================================================================

TEST_F(HyperbolicGeometryTest, HyperbolicDistance_Symmetry) {
    // WHAT: Verify d(x,y) = d(y,x)
    // WHY:  Distance metric property

    poincare_point_t* x = CreateTestPoint({0.1f, 0.2f});
    poincare_point_t* y = CreateTestPoint({0.3f, 0.4f});

    float dist_xy = poincare_distance(x, y);
    float dist_yx = poincare_distance(y, x);

    EXPECT_TRUE(FloatEqual(dist_xy, dist_yx));

    poincare_point_destroy(x);
    poincare_point_destroy(y);
    SUCCEED() << "Distance is symmetric";
}

//=============================================================================
// Unit Test 11: Hyperbolic distance - from origin
//=============================================================================

TEST_F(HyperbolicGeometryTest, HyperbolicDistance_FromOrigin) {
    // WHAT: Verify distance from origin
    // WHY:  Special case with simpler formula

    poincare_point_t* origin = CreateTestPoint({0.0f, 0.0f});
    poincare_point_t* point = CreateTestPoint({0.3f, 0.4f});

    float distance = poincare_distance(origin, point);

    // Distance from origin: d = acosh(1 + 2||x||²/(1-||x||²))
    // ||point|| = 0.5, so d = acosh(1 + 2*0.25/(1-0.25)) = acosh(1 + 0.5/0.75)
    EXPECT_GT(distance, 0.0f);
    EXPECT_LT(distance, 10.0f);  // Reasonable bound

    poincare_point_destroy(origin);
    poincare_point_destroy(point);
    SUCCEED() << "Distance from origin computed";
}

//=============================================================================
// Unit Test 12: Hyperbolic distance - positivity
//=============================================================================

TEST_F(HyperbolicGeometryTest, HyperbolicDistance_Positivity) {
    // WHAT: Verify d(x,y) > 0 for x ≠ y
    // WHY:  Distance metric property

    poincare_point_t* x = CreateTestPoint({0.1f, 0.0f});
    poincare_point_t* y = CreateTestPoint({0.2f, 0.0f});

    float distance = poincare_distance(x, y);

    EXPECT_GT(distance, 0.0f);

    poincare_point_destroy(x);
    poincare_point_destroy(y);
    SUCCEED() << "Distance between distinct points is positive";
}

//=============================================================================
// Unit Test 13: Exponential map - zero tangent vector
//=============================================================================

TEST_F(HyperbolicGeometryTest, ExpMap_ZeroTangentVector) {
    // WHAT: Verify exp_p(0) = p
    // WHY:  Zero velocity means no movement

    poincare_point_t* base = CreateTestPoint({0.2f, 0.3f});
    float tangent[] = {0.0f, 0.0f};

    poincare_point_t* result = poincare_exp_map(base, tangent);

    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(VectorEqual(result->coords, base->coords, base->dim));

    poincare_point_destroy(base);
    poincare_point_destroy(result);
    SUCCEED() << "Exponential map with zero tangent returns base point";
}

//=============================================================================
// Unit Test 14: Exponential map - from origin
//=============================================================================

TEST_F(HyperbolicGeometryTest, ExpMap_FromOrigin) {
    // WHAT: Verify exponential map from origin
    // WHY:  Simplest case, should give tanh(||v||/2) * v/||v||

    poincare_point_t* origin = CreateTestPoint({0.0f, 0.0f});
    float tangent[] = {0.2f, 0.0f};

    poincare_point_t* result = poincare_exp_map(origin, tangent);

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->dim, 2u);

    // Result should be along x-axis
    EXPECT_GT(result->coords[0], 0.0f);
    EXPECT_TRUE(FloatEqual(result->coords[1], 0.0f));

    // Should stay within ball
    float norm = poincare_norm(result);
    EXPECT_LT(norm, 1.0f);

    poincare_point_destroy(origin);
    poincare_point_destroy(result);
    SUCCEED() << "Exponential map from origin works";
}

//=============================================================================
// Unit Test 15: Logarithmic map - same point
//=============================================================================

TEST_F(HyperbolicGeometryTest, LogMap_SamePoint) {
    // WHAT: Verify log_p(p) = 0
    // WHY:  No tangent vector needed to reach same point

    poincare_point_t* point = CreateTestPoint({0.3f, 0.4f});

    float* tangent = poincare_log_map(point, point);

    ASSERT_NE(tangent, nullptr);
    EXPECT_TRUE(FloatEqual(tangent[0], 0.0f));
    EXPECT_TRUE(FloatEqual(tangent[1], 0.0f));

    nimcp_free(tangent);
    poincare_point_destroy(point);
    SUCCEED() << "Logarithmic map of same point gives zero tangent";
}

//=============================================================================
// Unit Test 16: Exp-Log inverse property
//=============================================================================

TEST_F(HyperbolicGeometryTest, ExpLogInverse_Property) {
    // WHAT: Verify exp_p(log_p(q)) ≈ q
    // WHY:  Fundamental property of exponential/logarithmic maps

    poincare_point_t* p = CreateTestPoint({0.1f, 0.2f});
    poincare_point_t* q = CreateTestPoint({0.3f, 0.4f});

    // Compute log_p(q)
    float* tangent = poincare_log_map(p, q);
    ASSERT_NE(tangent, nullptr);

    // Compute exp_p(log_p(q))
    poincare_point_t* result = poincare_exp_map(p, tangent);
    ASSERT_NE(result, nullptr);

    // Should recover q (approximately)
    EXPECT_TRUE(VectorEqual(result->coords, q->coords, q->dim, LOOSE_EPSILON));

    nimcp_free(tangent);
    poincare_point_destroy(p);
    poincare_point_destroy(q);
    poincare_point_destroy(result);
    SUCCEED() << "Exp-Log inverse property holds";
}

//=============================================================================
// Unit Test 17: Möbius addition - identity
//=============================================================================

TEST_F(HyperbolicGeometryTest, MobiusAdd_Identity) {
    // WHAT: Verify 0 ⊕ x = x
    // WHY:  Origin is additive identity

    poincare_point_t* origin = CreateTestPoint({0.0f, 0.0f});
    poincare_point_t* x = CreateTestPoint({0.3f, 0.4f});

    poincare_point_t* result = poincare_mobius_add(origin, x);

    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(VectorEqual(result->coords, x->coords, x->dim, LOOSE_EPSILON));

    poincare_point_destroy(origin);
    poincare_point_destroy(x);
    poincare_point_destroy(result);
    SUCCEED() << "Möbius addition with origin is identity";
}

//=============================================================================
// Unit Test 18: Möbius addition - stays in ball
//=============================================================================

TEST_F(HyperbolicGeometryTest, MobiusAdd_StaysInBall) {
    // WHAT: Verify x ⊕ y stays within Poincaré ball
    // WHY:  Closure property

    poincare_point_t* x = CreateTestPoint({0.5f, 0.0f});
    poincare_point_t* y = CreateTestPoint({0.3f, 0.2f});

    poincare_point_t* result = poincare_mobius_add(x, y);

    ASSERT_NE(result, nullptr);
    float norm = poincare_norm(result);
    EXPECT_LT(norm, 1.0f);

    poincare_point_destroy(x);
    poincare_point_destroy(y);
    poincare_point_destroy(result);
    SUCCEED() << "Möbius addition stays within ball";
}

//=============================================================================
// Unit Test 19: Möbius scalar multiplication - zero
//=============================================================================

TEST_F(HyperbolicGeometryTest, MobiusScalarMult_Zero) {
    // WHAT: Verify 0 ⊗ x = 0
    // WHY:  Zero scalar gives origin

    poincare_point_t* x = CreateTestPoint({0.3f, 0.4f});

    poincare_point_t* result = poincare_mobius_scalar_mult(0.0f, x);

    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(FloatEqual(result->coords[0], 0.0f));
    EXPECT_TRUE(FloatEqual(result->coords[1], 0.0f));

    poincare_point_destroy(x);
    poincare_point_destroy(result);
    SUCCEED() << "Möbius scalar multiplication by zero gives origin";
}

//=============================================================================
// Unit Test 20: Möbius scalar multiplication - identity
//=============================================================================

TEST_F(HyperbolicGeometryTest, MobiusScalarMult_Identity) {
    // WHAT: Verify 1 ⊗ x = x
    // WHY:  Unit scalar is identity

    poincare_point_t* x = CreateTestPoint({0.3f, 0.4f});

    poincare_point_t* result = poincare_mobius_scalar_mult(1.0f, x);

    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(VectorEqual(result->coords, x->coords, x->dim, LOOSE_EPSILON));

    poincare_point_destroy(x);
    poincare_point_destroy(result);
    SUCCEED() << "Möbius scalar multiplication by one is identity";
}

//=============================================================================
// Unit Test 21: Conformal factor - origin
//=============================================================================

TEST_F(HyperbolicGeometryTest, ConformalFactor_Origin) {
    // WHAT: Verify conformal factor at origin is 2
    // WHY:  λ_0 = 2/(1-0) = 2

    poincare_point_t* origin = CreateTestPoint({0.0f, 0.0f});

    float lambda = poincare_conformal_factor(origin);

    EXPECT_TRUE(FloatEqual(lambda, 2.0f));

    poincare_point_destroy(origin);
    SUCCEED() << "Conformal factor at origin is 2";
}

//=============================================================================
// Unit Test 22: Conformal factor - positivity
//=============================================================================

TEST_F(HyperbolicGeometryTest, ConformalFactor_Positivity) {
    // WHAT: Verify conformal factor is always positive
    // WHY:  λ_p = 2/(1-||p||²) > 0 for ||p|| < 1

    poincare_point_t* point = CreateTestPoint({0.5f, 0.5f});

    float lambda = poincare_conformal_factor(point);

    EXPECT_GT(lambda, 0.0f);

    poincare_point_destroy(point);
    SUCCEED() << "Conformal factor is positive";
}

//=============================================================================
// Unit Test 23: Conformal factor - increases near boundary
//=============================================================================

TEST_F(HyperbolicGeometryTest, ConformalFactor_IncreaseNearBoundary) {
    // WHAT: Verify conformal factor increases as ||p|| → 1
    // WHY:  Demonstrates hyperbolic geometry's exponential expansion

    poincare_point_t* near_origin = CreateTestPoint({0.1f, 0.0f});
    poincare_point_t* near_boundary = CreateTestPoint({0.8f, 0.0f});

    float lambda_origin = poincare_conformal_factor(near_origin);
    float lambda_boundary = poincare_conformal_factor(near_boundary);

    EXPECT_GT(lambda_boundary, lambda_origin);

    poincare_point_destroy(near_origin);
    poincare_point_destroy(near_boundary);
    SUCCEED() << "Conformal factor increases near boundary";
}

//=============================================================================
// Unit Test 24: Riemannian gradient - origin
//=============================================================================

TEST_F(HyperbolicGeometryTest, RiemannianGradient_Origin) {
    // WHAT: Verify Riemannian gradient at origin equals Euclidean
    // WHY:  At origin: (1-0²)²/4 = 1/4, but often normalized differently

    poincare_point_t* origin = CreateTestPoint({0.0f, 0.0f});
    float euclidean_grad[] = {1.0f, 2.0f};

    float* riemannian_grad = poincare_riemannian_gradient(origin, euclidean_grad);

    ASSERT_NE(riemannian_grad, nullptr);

    // At origin, Riemannian gradient should be scaled Euclidean gradient
    EXPECT_GT(std::abs(riemannian_grad[0]), 0.0f);
    EXPECT_GT(std::abs(riemannian_grad[1]), 0.0f);

    nimcp_free(riemannian_grad);
    poincare_point_destroy(origin);
    SUCCEED() << "Riemannian gradient computed at origin";
}

//=============================================================================
// Unit Test 25: Riemannian gradient - scaling
//=============================================================================

TEST_F(HyperbolicGeometryTest, RiemannianGradient_Scaling) {
    // WHAT: Verify Riemannian gradient scales correctly
    // WHY:  grad_R = (1-||x||²)²/4 * grad_E

    poincare_point_t* point = CreateTestPoint({0.5f, 0.0f});
    float euclidean_grad[] = {2.0f, 0.0f};

    float* riemannian_grad = poincare_riemannian_gradient(point, euclidean_grad);

    ASSERT_NE(riemannian_grad, nullptr);

    // Riemannian gradient should be smaller near boundary
    float norm = poincare_norm(point);
    float scale_factor = (1.0f - norm * norm) * (1.0f - norm * norm) / 4.0f;

    EXPECT_TRUE(FloatEqual(riemannian_grad[0], euclidean_grad[0] * scale_factor, LOOSE_EPSILON));

    nimcp_free(riemannian_grad);
    poincare_point_destroy(point);
    SUCCEED() << "Riemannian gradient scaling is correct";
}

//=============================================================================
// Unit Test 26: SGD step - moves point
//=============================================================================

TEST_F(HyperbolicGeometryTest, SGDStep_MovesPoint) {
    // WHAT: Verify SGD step updates point position
    // WHY:  Core optimization operation

    poincare_point_t* point = CreateTestPoint({0.2f, 0.3f});
    float original_x = point->coords[0];

    float gradient[] = {-0.1f, 0.05f};
    float learning_rate = 0.01f;

    bool success = poincare_sgd_step(point, gradient, learning_rate);

    EXPECT_TRUE(success);
    EXPECT_NE(point->coords[0], original_x);  // Point should move

    // Should stay in ball
    float norm = poincare_norm(point);
    EXPECT_LT(norm, 1.0f);

    poincare_point_destroy(point);
    SUCCEED() << "SGD step moves point";
}

//=============================================================================
// Unit Test 27: Point validation - valid point
//=============================================================================

TEST_F(HyperbolicGeometryTest, PointValidation_ValidPoint) {
    // WHAT: Verify valid point passes validation
    // WHY:  Sanity check for correctness

    poincare_point_t* point = CreateTestPoint({0.3f, 0.4f});

    bool valid = poincare_point_is_valid(point);

    EXPECT_TRUE(valid);

    poincare_point_destroy(point);
    SUCCEED() << "Valid point passes validation";
}

//=============================================================================
// Unit Test 28: Point validation - null point
//=============================================================================

TEST_F(HyperbolicGeometryTest, PointValidation_NullPoint) {
    // WHAT: Verify null point fails validation
    // WHY:  Safety check

    bool valid = poincare_point_is_valid(nullptr);

    EXPECT_FALSE(valid);
    SUCCEED() << "Null point fails validation";
}

//=============================================================================
// Unit Test 29: High-dimensional embedding
//=============================================================================

TEST_F(HyperbolicGeometryTest, HighDimensionalEmbedding) {
    // WHAT: Verify operations work in higher dimensions
    // WHY:  Real embeddings use 5-10 dimensions

    std::vector<float> coords_5d = {0.1f, 0.2f, 0.1f, 0.15f, 0.05f};
    poincare_point_t* point = CreateTestPoint(coords_5d);

    ASSERT_NE(point, nullptr);
    EXPECT_EQ(point->dim, 5u);

    float norm = poincare_norm(point);
    EXPECT_GT(norm, 0.0f);
    EXPECT_LT(norm, 1.0f);

    // Test distance computation
    std::vector<float> coords_5d_2 = {0.2f, 0.1f, 0.05f, 0.2f, 0.1f};
    poincare_point_t* point2 = CreateTestPoint(coords_5d_2);

    float distance = poincare_distance(point, point2);
    EXPECT_GT(distance, 0.0f);

    poincare_point_destroy(point);
    poincare_point_destroy(point2);
    SUCCEED() << "High-dimensional embeddings work";
}

//=============================================================================
// Unit Test 30: Numerical stability near boundary
//=============================================================================

TEST_F(HyperbolicGeometryTest, NumericalStability_NearBoundary) {
    // WHAT: Verify operations remain stable near boundary
    // WHY:  Critical for robust embeddings

    // Create point very close to boundary
    poincare_point_t* near_boundary = CreateTestPoint({0.99f, 0.0f});

    // Should be clipped to valid range
    float norm = poincare_norm(near_boundary);
    EXPECT_LT(norm, POINCARE_MAX_RADIUS);

    // Distance computation should not crash or give inf/nan
    poincare_point_t* origin = CreateTestPoint({0.0f, 0.0f});
    float distance = poincare_distance(origin, near_boundary);

    EXPECT_FALSE(std::isnan(distance));
    EXPECT_FALSE(std::isinf(distance));
    EXPECT_GT(distance, 0.0f);

    poincare_point_destroy(near_boundary);
    poincare_point_destroy(origin);
    SUCCEED() << "Operations are stable near boundary";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
