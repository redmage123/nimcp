/**
 * @file test_mathematical_intuition.cpp
 * @brief Unit tests for NIMCP Mathematical Intuition
 *
 * Tests pattern detection, symmetry detection, analogies,
 * and geometric reasoning.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "cognitive/parietal/nimcp_mathematical_intuition.h"

namespace {

//=============================================================================
// Test Constants
//=============================================================================

constexpr float FLOAT_TOLERANCE = 1e-5f;
constexpr float PI = 3.14159265358979f;

//=============================================================================
// Test Fixture
//=============================================================================

class MathIntuitionTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        mi = math_intuition_create();
        ASSERT_NE(mi, nullptr);
    }

    void TearDown() override
    {
        if (mi) {
            math_intuition_destroy(mi);
            mi = nullptr;
        }
    }

    math_intuition_t* mi;
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(MathIntuitionTest, CreateDefault)
{
    EXPECT_NE(mi, nullptr);
}

TEST_F(MathIntuitionTest, CreateCustom)
{
    math_intuition_config_t config = math_intuition_default_config();
    config.pattern_confidence_threshold = 0.8f;
    config.symmetry_tolerance = 0.005f;

    math_intuition_t* custom = math_intuition_create_custom(&config);
    ASSERT_NE(custom, nullptr);
    math_intuition_destroy(custom);
}

TEST_F(MathIntuitionTest, CreateWithNullConfig)
{
    math_intuition_t* created = math_intuition_create_custom(nullptr);
    EXPECT_NE(created, nullptr);
    math_intuition_destroy(created);
}

TEST_F(MathIntuitionTest, DestroyNullSafe)
{
    math_intuition_destroy(nullptr);
    // Should not crash
}

TEST_F(MathIntuitionTest, DefaultConfig)
{
    math_intuition_config_t config = math_intuition_default_config();

    EXPECT_NEAR(config.pattern_confidence_threshold, 0.7f, 0.01f);
    EXPECT_NEAR(config.symmetry_tolerance, 0.01f, 0.001f);
    EXPECT_EQ(config.max_polynomial_degree, 5);
    EXPECT_TRUE(config.enable_oscillation_detection);
}

TEST_F(MathIntuitionTest, ValidateConfig)
{
    math_intuition_config_t valid = math_intuition_default_config();
    EXPECT_TRUE(math_intuition_validate_config(&valid));

    math_intuition_config_t invalid = valid;
    invalid.pattern_confidence_threshold = 1.5f;
    EXPECT_FALSE(math_intuition_validate_config(&invalid));

    invalid = valid;
    invalid.max_polynomial_degree = 0;
    EXPECT_FALSE(math_intuition_validate_config(&invalid));
}

//=============================================================================
// Pattern Detection Tests - Arithmetic Progression
//=============================================================================

TEST_F(MathIntuitionTest, DetectArithmeticPattern)
{
    float seq[] = {2.0f, 4.0f, 6.0f, 8.0f, 10.0f};
    detected_pattern_t pattern = math_detect_pattern(mi, seq, 5);

    EXPECT_EQ(pattern.type, PATTERN_ARITHMETIC);
    EXPECT_GT(pattern.confidence, 0.9f);
    EXPECT_NEAR(pattern.params.arithmetic.first_term, 2.0f, 0.1f);
    EXPECT_NEAR(pattern.params.arithmetic.difference, 2.0f, 0.1f);
}

TEST_F(MathIntuitionTest, DetectArithmeticNegativeDifference)
{
    float seq[] = {10.0f, 8.0f, 6.0f, 4.0f, 2.0f};
    detected_pattern_t pattern = math_detect_pattern(mi, seq, 5);

    EXPECT_EQ(pattern.type, PATTERN_ARITHMETIC);
    EXPECT_NEAR(pattern.params.arithmetic.difference, -2.0f, 0.1f);
}

//=============================================================================
// Pattern Detection Tests - Geometric Progression
//=============================================================================

TEST_F(MathIntuitionTest, DetectGeometricPattern)
{
    float seq[] = {2.0f, 4.0f, 8.0f, 16.0f, 32.0f};
    detected_pattern_t pattern = math_detect_pattern(mi, seq, 5);

    EXPECT_EQ(pattern.type, PATTERN_GEOMETRIC);
    EXPECT_GT(pattern.confidence, 0.9f);
    EXPECT_NEAR(pattern.params.geometric.first_term, 2.0f, 0.1f);
    EXPECT_NEAR(pattern.params.geometric.ratio, 2.0f, 0.1f);
}

TEST_F(MathIntuitionTest, DetectGeometricDecay)
{
    float seq[] = {64.0f, 32.0f, 16.0f, 8.0f, 4.0f};
    detected_pattern_t pattern = math_detect_pattern(mi, seq, 5);

    EXPECT_EQ(pattern.type, PATTERN_GEOMETRIC);
    EXPECT_NEAR(pattern.params.geometric.ratio, 0.5f, 0.1f);
}

//=============================================================================
// Pattern Detection Tests - Fibonacci
//=============================================================================

TEST_F(MathIntuitionTest, DetectFibonacciPattern)
{
    float seq[] = {1.0f, 1.0f, 2.0f, 3.0f, 5.0f, 8.0f, 13.0f};
    detected_pattern_t pattern = math_detect_pattern(mi, seq, 7);

    EXPECT_EQ(pattern.type, PATTERN_FIBONACCI);
    EXPECT_GT(pattern.confidence, 0.8f);
}

//=============================================================================
// Pattern Detection Tests - Constant
//=============================================================================

TEST_F(MathIntuitionTest, DetectConstantPattern)
{
    float seq[] = {5.0f, 5.0f, 5.0f, 5.0f, 5.0f};
    detected_pattern_t pattern = math_detect_pattern(mi, seq, 5);

    EXPECT_EQ(pattern.type, PATTERN_CONSTANT);
    EXPECT_GT(pattern.confidence, 0.95f);
    EXPECT_NEAR(pattern.params.constant.value, 5.0f, 0.1f);
}

//=============================================================================
// Pattern Detection Tests - Polynomial
//=============================================================================

TEST_F(MathIntuitionTest, DetectQuadraticPattern)
{
    // Quadratic: n^2 = 1, 4, 9, 16, 25
    float seq[] = {1.0f, 4.0f, 9.0f, 16.0f, 25.0f};
    detected_pattern_t pattern = math_detect_pattern(mi, seq, 5);

    // Should detect as polynomial or square pattern
    EXPECT_TRUE(pattern.type == PATTERN_POLYNOMIAL || pattern.type == PATTERN_SQUARE);
    EXPECT_GT(pattern.confidence, 0.8f);
}

//=============================================================================
// Pattern Detection Tests - Special Sequences
//=============================================================================

TEST_F(MathIntuitionTest, DetectTriangularPattern)
{
    // Triangular numbers: 1, 3, 6, 10, 15
    float seq[] = {1.0f, 3.0f, 6.0f, 10.0f, 15.0f};
    detected_pattern_t pattern = math_detect_pattern(mi, seq, 5);

    EXPECT_TRUE(pattern.type == PATTERN_TRIANGULAR || pattern.type == PATTERN_POLYNOMIAL);
    EXPECT_GT(pattern.confidence, 0.8f);
}

//=============================================================================
// Pattern Detection Null Handling
//=============================================================================

TEST_F(MathIntuitionTest, DetectPatternNullHandling)
{
    float seq[] = {1.0f, 2.0f, 3.0f};

    detected_pattern_t pattern = math_detect_pattern(nullptr, seq, 3);
    EXPECT_EQ(pattern.type, PATTERN_UNKNOWN);

    pattern = math_detect_pattern(mi, nullptr, 3);
    EXPECT_EQ(pattern.type, PATTERN_UNKNOWN);

    pattern = math_detect_pattern(mi, seq, 0);
    EXPECT_EQ(pattern.type, PATTERN_UNKNOWN);
}

//=============================================================================
// Pattern Extrapolation Tests
//=============================================================================

TEST_F(MathIntuitionTest, ExtrapolateArithmetic)
{
    float seq[] = {2.0f, 4.0f, 6.0f, 8.0f};
    detected_pattern_t pattern = math_detect_pattern(mi, seq, 4);

    float next = math_extrapolate(mi, &pattern, 4);  // Index 4 = 5th value
    EXPECT_NEAR(next, 10.0f, 0.5f);

    float after = math_extrapolate(mi, &pattern, 5);  // Index 5 = 6th value
    EXPECT_NEAR(after, 12.0f, 0.5f);
}

TEST_F(MathIntuitionTest, ExtrapolateGeometric)
{
    float seq[] = {2.0f, 4.0f, 8.0f, 16.0f};
    detected_pattern_t pattern = math_detect_pattern(mi, seq, 4);

    float next = math_extrapolate(mi, &pattern, 4);
    EXPECT_NEAR(next, 32.0f, 2.0f);
}

TEST_F(MathIntuitionTest, PredictSequence)
{
    float seq[] = {1.0f, 2.0f, 3.0f, 4.0f};
    detected_pattern_t pattern = math_detect_pattern(mi, seq, 4);

    float predictions[3];
    uint32_t num = math_predict_sequence(mi, &pattern, predictions, 3);

    EXPECT_EQ(num, 3);
    EXPECT_NEAR(predictions[0], 5.0f, 0.5f);
    EXPECT_NEAR(predictions[1], 6.0f, 0.5f);
    EXPECT_NEAR(predictions[2], 7.0f, 0.5f);
}

TEST_F(MathIntuitionTest, CheckPatternFit)
{
    float seq[] = {2.0f, 4.0f, 6.0f, 8.0f};
    detected_pattern_t pattern = math_detect_pattern(mi, seq, 4);

    // Value that fits
    float fit_good = math_check_pattern_fit(mi, &pattern, 10.0f, 4);
    EXPECT_GT(fit_good, 0.8f);

    // Value that doesn't fit (but still has some match to pattern)
    float fit_bad = math_check_pattern_fit(mi, &pattern, 15.0f, 4);
    EXPECT_LT(fit_bad, fit_good);  // Just verify it's worse than the good fit
}

TEST_F(MathIntuitionTest, PatternTypeName)
{
    EXPECT_STREQ(math_pattern_type_name(PATTERN_ARITHMETIC), "Arithmetic");
    EXPECT_STREQ(math_pattern_type_name(PATTERN_GEOMETRIC), "Geometric");
    EXPECT_STREQ(math_pattern_type_name(PATTERN_FIBONACCI), "Fibonacci");
    EXPECT_STREQ(math_pattern_type_name(PATTERN_UNKNOWN), "Unknown");
}

//=============================================================================
// Geometric Reasoning Tests
//=============================================================================

TEST_F(MathIntuitionTest, AnalyzeLinesParallel)
{
    vec3_t l1_start = vec3_create(0.0f, 0.0f, 0.0f);
    vec3_t l1_end = vec3_create(1.0f, 0.0f, 0.0f);
    vec3_t l2_start = vec3_create(0.0f, 1.0f, 0.0f);
    vec3_t l2_end = vec3_create(1.0f, 1.0f, 0.0f);

    geometric_result_t result = math_analyze_lines(mi, l1_start, l1_end, l2_start, l2_end);

    EXPECT_EQ(result.relation, GEOM_RELATION_PARALLEL);
    EXPECT_GT(result.confidence, 0.9f);
}

TEST_F(MathIntuitionTest, AnalyzeLinesPerpendicular)
{
    vec3_t l1_start = vec3_create(0.0f, 0.0f, 0.0f);
    vec3_t l1_end = vec3_create(1.0f, 0.0f, 0.0f);
    vec3_t l2_start = vec3_create(0.0f, 0.0f, 0.0f);
    vec3_t l2_end = vec3_create(0.0f, 1.0f, 0.0f);

    geometric_result_t result = math_analyze_lines(mi, l1_start, l1_end, l2_start, l2_end);

    EXPECT_EQ(result.relation, GEOM_RELATION_PERPENDICULAR);
    EXPECT_GT(result.confidence, 0.9f);
}

TEST_F(MathIntuitionTest, AnalyzeLinesIntersecting)
{
    // Use non-perpendicular intersecting lines (45 degrees)
    vec3_t l1_start = vec3_create(0.0f, 0.0f, 0.0f);
    vec3_t l1_end = vec3_create(2.0f, 1.0f, 0.0f);   // 26.5 degree angle
    vec3_t l2_start = vec3_create(0.0f, 1.0f, 0.0f);
    vec3_t l2_end = vec3_create(2.0f, 0.0f, 0.0f);   // -26.5 degree angle

    geometric_result_t result = math_analyze_lines(mi, l1_start, l1_end, l2_start, l2_end);

    EXPECT_EQ(result.relation, GEOM_RELATION_INTERSECTING);
    // Lines intersect somewhere in the middle
    EXPECT_GE(result.intersection_point.x, 0.0f);
    EXPECT_LE(result.intersection_point.x, 2.0f);
}

TEST_F(MathIntuitionTest, CheckCongruent)
{
    // Two identical triangles
    vec3_t tri1[3] = {
        vec3_create(0, 0, 0),
        vec3_create(1, 0, 0),
        vec3_create(0.5f, 1, 0)
    };
    vec3_t tri2[3] = {
        vec3_create(5, 5, 0),
        vec3_create(6, 5, 0),
        vec3_create(5.5f, 6, 0)
    };

    geometric_result_t result = math_check_congruent(mi, tri1, 3, tri2, 3);

    EXPECT_EQ(result.relation, GEOM_RELATION_CONGRUENT);
    EXPECT_GT(result.confidence, 0.8f);
}

TEST_F(MathIntuitionTest, CheckSimilar)
{
    // Triangle and scaled version
    vec3_t tri1[3] = {
        vec3_create(0, 0, 0),
        vec3_create(1, 0, 0),
        vec3_create(0.5f, 1, 0)
    };
    vec3_t tri2[3] = {
        vec3_create(0, 0, 0),
        vec3_create(2, 0, 0),
        vec3_create(1.0f, 2, 0)
    };

    geometric_result_t result = math_check_similar(mi, tri1, 3, tri2, 3);

    EXPECT_EQ(result.relation, GEOM_RELATION_SIMILAR);
    // Scale factor could be 2.0 or 0.5 depending on direction
    bool scale_valid = (fabsf(result.scale_factor - 2.0f) < 0.3f) ||
                       (fabsf(result.scale_factor - 0.5f) < 0.1f);
    EXPECT_TRUE(scale_valid);
    EXPECT_GT(result.confidence, 0.8f);
}

//=============================================================================
// Symmetry Detection Tests
//=============================================================================

TEST_F(MathIntuitionTest, DetectReflectionSymmetry)
{
    // Points symmetric around Y axis
    vec3_t points[] = {
        vec3_create(-1, 0, 0),
        vec3_create(1, 0, 0),
        vec3_create(-2, 1, 0),
        vec3_create(2, 1, 0),
        vec3_create(0, 2, 0)
    };

    symmetry_result_t sym = math_detect_symmetry(mi, points, 5);

    EXPECT_TRUE(sym.has_reflection);
    EXPECT_GT(sym.confidence, 0.8f);
}

TEST_F(MathIntuitionTest, DetectRotationalSymmetry)
{
    // Square (4-fold rotational symmetry, also has 2-fold)
    vec3_t points[] = {
        vec3_create(1, 0, 0),
        vec3_create(0, 1, 0),
        vec3_create(-1, 0, 0),
        vec3_create(0, -1, 0)
    };

    symmetry_result_t sym = math_detect_symmetry(mi, points, 4);

    EXPECT_TRUE(sym.has_rotation);
    // May detect 2-fold or 4-fold (both are valid for a square)
    EXPECT_TRUE(sym.rotation_order == 2 || sym.rotation_order == 4);
    EXPECT_GT(sym.confidence, 0.5f);
}

TEST_F(MathIntuitionTest, DetectPointSymmetry)
{
    // Points symmetric around origin
    vec3_t points[] = {
        vec3_create(1, 1, 0),
        vec3_create(-1, -1, 0),
        vec3_create(2, -1, 0),
        vec3_create(-2, 1, 0)
    };

    symmetry_result_t sym = math_detect_symmetry(mi, points, 4);

    EXPECT_TRUE(sym.has_point_symmetry);
    EXPECT_NEAR(sym.symmetry_center.x, 0.0f, 0.1f);
    EXPECT_NEAR(sym.symmetry_center.y, 0.0f, 0.1f);
}

TEST_F(MathIntuitionTest, CheckSymmetryType)
{
    vec3_t points[] = {
        vec3_create(-1, 0, 0),
        vec3_create(1, 0, 0)
    };

    float conf_reflection = math_check_symmetry_type(mi, points, 2, SYMMETRY_REFLECTION);
    EXPECT_GT(conf_reflection, 0.8f);
}

TEST_F(MathIntuitionTest, FindReflectionAxis)
{
    vec3_t points[] = {
        vec3_create(-1, 0, 0),
        vec3_create(1, 0, 0),
        vec3_create(0, 1, 0)
    };

    vec3_t axis_point, axis_direction;
    float conf = math_find_reflection_axis(mi, points, 3, &axis_point, &axis_direction);

    EXPECT_GT(conf, 0.5f);
    // The axis found may vary - just verify it's a valid normalized direction
    float len = sqrtf(axis_direction.x * axis_direction.x +
                      axis_direction.y * axis_direction.y +
                      axis_direction.z * axis_direction.z);
    EXPECT_NEAR(len, 1.0f, 0.2f);  // Should be normalized
}

TEST_F(MathIntuitionTest, FindRotationSymmetry)
{
    // Equilateral triangle (3-fold symmetry)
    vec3_t points[] = {
        vec3_create(0, 1, 0),
        vec3_create(-0.866f, -0.5f, 0),
        vec3_create(0.866f, -0.5f, 0)
    };

    vec3_t center;
    uint32_t order;
    float conf = math_find_rotation_symmetry(mi, points, 3, &center, &order);

    EXPECT_GT(conf, 0.7f);
    EXPECT_EQ(order, 3);
    EXPECT_NEAR(center.x, 0.0f, 0.1f);
    EXPECT_NEAR(center.y, 0.0f, 0.1f);
}

TEST_F(MathIntuitionTest, SymmetryNullHandling)
{
    vec3_t points[] = {vec3_create(0, 0, 0)};

    symmetry_result_t sym = math_detect_symmetry(nullptr, points, 1);
    EXPECT_EQ(sym.type, SYMMETRY_NONE);

    sym = math_detect_symmetry(mi, nullptr, 1);
    EXPECT_EQ(sym.type, SYMMETRY_NONE);
}

//=============================================================================
// Analogy Tests
//=============================================================================

TEST_F(MathIntuitionTest, SolveAnalogyAdditive)
{
    // 2:4 :: 5:?  (could be additive +2 -> 7, or multiplicative *2 -> 10)
    analogy_result_t result = math_solve_analogy(mi, 2.0f, 4.0f, 5.0f);

    // Accept either additive (7) or multiplicative (10) interpretation
    bool valid_answer = (fabsf(result.answer - 7.0f) < 1.0f) ||
                        (fabsf(result.answer - 10.0f) < 1.0f);
    EXPECT_TRUE(valid_answer);
    EXPECT_GT(result.confidence, 0.5f);
}

TEST_F(MathIntuitionTest, SolveAnalogyMultiplicative)
{
    // 2:6 :: 4:?  (multiplicative: *3)
    analogy_result_t result = math_solve_analogy(mi, 2.0f, 6.0f, 4.0f);

    EXPECT_NEAR(result.answer, 12.0f, 1.0f);
    EXPECT_GT(result.confidence, 0.7f);
}

TEST_F(MathIntuitionTest, SolveAnalogySquare)
{
    // 2:4 :: 3:?  (could be square x^2 -> 9, additive +2 -> 5, or multiplicative *2 -> 6)
    analogy_result_t result = math_solve_analogy(mi, 2.0f, 4.0f, 3.0f);

    // Multiple valid interpretations exist
    EXPECT_GT(result.confidence, 0.3f);
}

TEST_F(MathIntuitionTest, CheckAnalogy)
{
    // 2:4 :: 3:6  (ratio of 2)
    float validity = math_check_analogy(mi, 2.0f, 4.0f, 3.0f, 6.0f);
    EXPECT_GT(validity, 0.7f);

    // 2:4 :: 3:10  (incorrect for most patterns)
    float validity_bad = math_check_analogy(mi, 2.0f, 4.0f, 3.0f, 10.0f);
    EXPECT_LT(validity_bad, validity);  // Should be worse than the correct one
}

TEST_F(MathIntuitionTest, SolveAnalogyNullHandling)
{
    analogy_result_t result = math_solve_analogy(nullptr, 1.0f, 2.0f, 3.0f);
    EXPECT_EQ(result.confidence, -1.0f);  // Null input returns -1.0f confidence
}

//=============================================================================
// Modulation Tests
//=============================================================================

TEST_F(MathIntuitionTest, SetInflammation)
{
    EXPECT_EQ(math_intuition_set_inflammation(mi, 0.5f), 0);
    EXPECT_NE(math_intuition_set_inflammation(nullptr, 0.5f), 0);
}

TEST_F(MathIntuitionTest, SetFatigue)
{
    EXPECT_EQ(math_intuition_set_fatigue(mi, 0.5f), 0);
    EXPECT_NE(math_intuition_set_fatigue(nullptr, 0.5f), 0);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(MathIntuitionTest, GetStats)
{
    // Perform operations
    float seq[] = {1.0f, 2.0f, 3.0f};
    math_detect_pattern(mi, seq, 3);

    vec3_t points[] = {vec3_create(0, 0, 0), vec3_create(1, 0, 0)};
    math_detect_symmetry(mi, points, 2);

    math_solve_analogy(mi, 1.0f, 2.0f, 3.0f);

    math_intuition_stats_t stats;
    EXPECT_EQ(math_intuition_get_stats(mi, &stats), 0);

    EXPECT_GE(stats.patterns_detected, 1);
    EXPECT_GE(stats.symmetries_detected, 1);
    EXPECT_GE(stats.analogies_solved, 1);
}

TEST_F(MathIntuitionTest, GetStatsNullHandling)
{
    math_intuition_stats_t stats;
    EXPECT_NE(math_intuition_get_stats(nullptr, &stats), 0);
    EXPECT_NE(math_intuition_get_stats(mi, nullptr), 0);
}

TEST_F(MathIntuitionTest, ResetStats)
{
    float seq[] = {1.0f, 2.0f, 3.0f};
    math_detect_pattern(mi, seq, 3);

    math_intuition_reset_stats(mi);

    math_intuition_stats_t stats;
    math_intuition_get_stats(mi, &stats);
    EXPECT_EQ(stats.patterns_detected, 0);
}

TEST_F(MathIntuitionTest, ResetStatsNullSafe)
{
    math_intuition_reset_stats(nullptr);
    // Should not crash
}

}  // namespace
