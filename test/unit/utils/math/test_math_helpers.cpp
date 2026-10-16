//=============================================================================
// test_math_helpers.cpp - Math Helper Functions Unit Tests
//=============================================================================
/**
 * @file test_math_helpers.cpp
 * @brief Tests for the consolidated math helper functions in nimcp_math_helpers.h
 *
 * WHAT: Verifies nimcp_clampf, nimcp_clamp01, nimcp_safe_logf,
 *       nimcp_entropy, and nimcp_sigmoid.
 * WHY:  These replace dozens of duplicate static definitions across the codebase.
 *
 * @author NIMCP Development Team
 * @date 2026-02-26
 */

#include <gtest/gtest.h>
#include <cmath>

extern "C" {
#include "utils/math/nimcp_math_helpers.h"
}

//=============================================================================
// nimcp_clampf Tests
//=============================================================================

TEST(MathHelpers, ClampfBasic) {
    EXPECT_FLOAT_EQ(nimcp_clampf(0.5f, 0.0f, 1.0f), 0.5f);
    EXPECT_FLOAT_EQ(nimcp_clampf(-1.0f, 0.0f, 1.0f), 0.0f);
    EXPECT_FLOAT_EQ(nimcp_clampf(2.0f, 0.0f, 1.0f), 1.0f);
}

TEST(MathHelpers, ClampfBoundary) {
    EXPECT_FLOAT_EQ(nimcp_clampf(0.0f, 0.0f, 1.0f), 0.0f);
    EXPECT_FLOAT_EQ(nimcp_clampf(1.0f, 0.0f, 1.0f), 1.0f);
}

TEST(MathHelpers, ClampfNegativeRange) {
    EXPECT_FLOAT_EQ(nimcp_clampf(0.0f, -1.0f, -0.5f), -0.5f);
    EXPECT_FLOAT_EQ(nimcp_clampf(-2.0f, -1.0f, -0.5f), -1.0f);
    EXPECT_FLOAT_EQ(nimcp_clampf(-0.7f, -1.0f, -0.5f), -0.7f);
}

//=============================================================================
// nimcp_clamp01 Tests
//=============================================================================

TEST(MathHelpers, Clamp01InRange) {
    EXPECT_FLOAT_EQ(nimcp_clamp01(0.5f), 0.5f);
    EXPECT_FLOAT_EQ(nimcp_clamp01(0.0f), 0.0f);
    EXPECT_FLOAT_EQ(nimcp_clamp01(1.0f), 1.0f);
}

TEST(MathHelpers, Clamp01OutOfRange) {
    EXPECT_FLOAT_EQ(nimcp_clamp01(-0.5f), 0.0f);
    EXPECT_FLOAT_EQ(nimcp_clamp01(1.5f), 1.0f);
    EXPECT_FLOAT_EQ(nimcp_clamp01(-100.0f), 0.0f);
    EXPECT_FLOAT_EQ(nimcp_clamp01(100.0f), 1.0f);
}

//=============================================================================
// nimcp_safe_logf Tests
//=============================================================================

TEST(MathHelpers, SafeLogNoNaN) {
    EXPECT_FALSE(std::isnan(nimcp_safe_logf(0.0f)));
    EXPECT_FALSE(std::isnan(nimcp_safe_logf(-1.0f)));
    EXPECT_FALSE(std::isinf(nimcp_safe_logf(0.0f)));
}

TEST(MathHelpers, SafeLogPositive) {
    // log(1) should be close to 0
    EXPECT_NEAR(nimcp_safe_logf(1.0f), 0.0f, 0.001f);

    // log(e) should be close to 1
    EXPECT_NEAR(nimcp_safe_logf(expf(1.0f)), 1.0f, 0.001f);
}

TEST(MathHelpers, SafeLogSmallValues) {
    // Very small positive values should not produce -inf
    float result = nimcp_safe_logf(1e-20f);
    EXPECT_FALSE(std::isinf(result));

    // Zero should produce log(1e-10) which is finite and negative
    float zero_result = nimcp_safe_logf(0.0f);
    EXPECT_LT(zero_result, 0.0f);
    EXPECT_FALSE(std::isinf(zero_result));
}

//=============================================================================
// nimcp_sigmoid Tests
//=============================================================================

TEST(MathHelpers, SigmoidMidpoint) {
    EXPECT_GT(nimcp_sigmoid(0.0f), 0.499f);
    EXPECT_LT(nimcp_sigmoid(0.0f), 0.501f);
}

TEST(MathHelpers, SigmoidBounds) {
    EXPECT_GT(nimcp_sigmoid(100.0f), 0.99f);
    EXPECT_LT(nimcp_sigmoid(-100.0f), 0.01f);
}

TEST(MathHelpers, SigmoidMonotonic) {
    // sigmoid should be monotonically increasing
    float prev = nimcp_sigmoid(-10.0f);
    for (float x = -9.0f; x <= 10.0f; x += 1.0f) {
        float curr = nimcp_sigmoid(x);
        EXPECT_GE(curr, prev) << "sigmoid not monotonic at x=" << x;
        prev = curr;
    }
}

TEST(MathHelpers, SigmoidSymmetry) {
    // sigmoid(-x) = 1 - sigmoid(x)
    for (float x = 0.1f; x <= 5.0f; x += 0.5f) {
        EXPECT_NEAR(nimcp_sigmoid(-x), 1.0f - nimcp_sigmoid(x), 1e-5f)
            << "sigmoid symmetry violated at x=" << x;
    }
}

//=============================================================================
// nimcp_entropy Tests
//=============================================================================

TEST(MathHelpers, EntropyUniform) {
    float probs[] = {0.25f, 0.25f, 0.25f, 0.25f};
    float h = nimcp_entropy(probs, 4);
    // Max entropy for 4 classes = log(4)
    EXPECT_NEAR(h, logf(4.0f), 0.01f);
}

TEST(MathHelpers, EntropyDeterministic) {
    float probs[] = {1.0f, 0.0f, 0.0f, 0.0f};
    float h = nimcp_entropy(probs, 4);
    // Deterministic distribution has 0 entropy
    EXPECT_NEAR(h, 0.0f, 0.001f);
}

TEST(MathHelpers, EntropyBinary) {
    // Binary uniform distribution: entropy = log(2)
    float probs[] = {0.5f, 0.5f};
    float h = nimcp_entropy(probs, 2);
    EXPECT_NEAR(h, logf(2.0f), 0.01f);
}

TEST(MathHelpers, EntropyNonNegative) {
    // Entropy should always be >= 0 for valid distributions
    float probs[] = {0.1f, 0.2f, 0.3f, 0.4f};
    float h = nimcp_entropy(probs, 4);
    EXPECT_GE(h, 0.0f);
}

TEST(MathHelpers, EntropyAllZero) {
    // Degenerate case: all zero probabilities
    float probs[] = {0.0f, 0.0f, 0.0f, 0.0f};
    float h = nimcp_entropy(probs, 4);
    EXPECT_FLOAT_EQ(h, 0.0f);
    EXPECT_FALSE(std::isnan(h));
}
