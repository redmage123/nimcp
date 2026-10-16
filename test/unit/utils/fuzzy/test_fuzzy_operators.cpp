/**
 * @file test_fuzzy_operators.cpp
 * @brief Unit tests for fuzzy logic operators module
 *
 * WHAT: ~45 tests for t-norms, t-conorms, complements, implications,
 *       aggregation, weighted ops, relations, similarity/distance, set ops
 * WHY:  Verify correct algebraic behavior of all 28+ operator variants
 * HOW:  GTest C++17, extern "C" headers, EXPECT_NEAR for float comparisons
 *
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <algorithm>

extern "C" {
#include "utils/fuzzy/nimcp_fuzzy_types.h"
#include "utils/fuzzy/nimcp_fuzzy_operators.h"
}

// ============================================================================
// Test Constants
// ============================================================================

namespace {
    constexpr float TOL = 1e-4f;
    constexpr float RELAXED = 1e-2f;
}

// ============================================================================
// 1. T-Norm Tests (Fuzzy AND)
// ============================================================================

TEST(FuzzyTNorm, Min_BasicValues) {
    EXPECT_NEAR(fuzzy_tnorm(0.3f, 0.7f, FUZZY_TNORM_MIN), 0.3f, TOL);
}

TEST(FuzzyTNorm, Min_BothOne) {
    EXPECT_NEAR(fuzzy_tnorm(1.0f, 1.0f, FUZZY_TNORM_MIN), 1.0f, TOL);
}

TEST(FuzzyTNorm, Min_OneZero) {
    EXPECT_NEAR(fuzzy_tnorm(0.5f, 0.0f, FUZZY_TNORM_MIN), 0.0f, TOL);
}

TEST(FuzzyTNorm, AlgebraicProduct_BasicValues) {
    EXPECT_NEAR(fuzzy_tnorm(0.4f, 0.5f, FUZZY_TNORM_ALGEBRAIC_PRODUCT), 0.2f, TOL);
}

TEST(FuzzyTNorm, Lukasiewicz_BasicValues) {
    // max(0, 0.6 + 0.7 - 1) = max(0, 0.3) = 0.3
    EXPECT_NEAR(fuzzy_tnorm(0.6f, 0.7f, FUZZY_TNORM_LUKASIEWICZ), 0.3f, TOL);
}

TEST(FuzzyTNorm, Lukasiewicz_SmallValues) {
    // max(0, 0.2 + 0.3 - 1) = max(0, -0.5) = 0
    EXPECT_NEAR(fuzzy_tnorm(0.2f, 0.3f, FUZZY_TNORM_LUKASIEWICZ), 0.0f, TOL);
}

TEST(FuzzyTNorm, Drastic_OneMax) {
    // drastic: min(a,b) if max(a,b)=1, else 0
    EXPECT_NEAR(fuzzy_tnorm(0.5f, 1.0f, FUZZY_TNORM_DRASTIC), 0.5f, TOL);
}

TEST(FuzzyTNorm, Drastic_NeitherMax) {
    EXPECT_NEAR(fuzzy_tnorm(0.5f, 0.5f, FUZZY_TNORM_DRASTIC), 0.0f, TOL);
}

TEST(FuzzyTNorm, Einstein_BasicValues) {
    // (a*b)/(2-(a+b-a*b)) = (0.5*0.5)/(2-(0.5+0.5-0.25)) = 0.25/(2-0.75) = 0.25/1.25 = 0.2
    float result = fuzzy_tnorm(0.5f, 0.5f, FUZZY_TNORM_EINSTEIN);
    EXPECT_NEAR(result, 0.2f, TOL);
}

TEST(FuzzyTNorm, Hamacher_BasicValues) {
    // (a*b)/(a+b-a*b) = (0.5*0.5)/(0.5+0.5-0.25) = 0.25/0.75 = 1/3
    float result = fuzzy_tnorm(0.5f, 0.5f, FUZZY_TNORM_HAMACHER);
    EXPECT_NEAR(result, 1.0f / 3.0f, TOL);
}

TEST(FuzzyTNorm, NilpotentMin_SumGreaterThanOne) {
    // min(a,b) if a+b > 1, else 0
    EXPECT_NEAR(fuzzy_tnorm(0.6f, 0.7f, FUZZY_TNORM_NILPOTENT_MIN), 0.6f, TOL);
}

TEST(FuzzyTNorm, NilpotentMin_SumLessOrEqual) {
    EXPECT_NEAR(fuzzy_tnorm(0.3f, 0.4f, FUZZY_TNORM_NILPOTENT_MIN), 0.0f, TOL);
}

// ============================================================================
// 2. T-Conorm Tests (Fuzzy OR)
// ============================================================================

TEST(FuzzyTConorm, Max_BasicValues) {
    EXPECT_NEAR(fuzzy_tconorm(0.3f, 0.7f, FUZZY_TCONORM_MAX), 0.7f, TOL);
}

TEST(FuzzyTConorm, Max_BothZero) {
    EXPECT_NEAR(fuzzy_tconorm(0.0f, 0.0f, FUZZY_TCONORM_MAX), 0.0f, TOL);
}

TEST(FuzzyTConorm, AlgebraicSum_BasicValues) {
    // a + b - a*b = 0.4 + 0.5 - 0.2 = 0.7
    EXPECT_NEAR(fuzzy_tconorm(0.4f, 0.5f, FUZZY_TCONORM_ALGEBRAIC_SUM), 0.7f, TOL);
}

TEST(FuzzyTConorm, Lukasiewicz_BasicValues) {
    // min(1, a+b) = min(1, 0.6+0.7) = min(1, 1.3) = 1.0
    EXPECT_NEAR(fuzzy_tconorm(0.6f, 0.7f, FUZZY_TCONORM_LUKASIEWICZ), 1.0f, TOL);
}

TEST(FuzzyTConorm, Lukasiewicz_SmallValues) {
    // min(1, 0.2+0.3) = 0.5
    EXPECT_NEAR(fuzzy_tconorm(0.2f, 0.3f, FUZZY_TCONORM_LUKASIEWICZ), 0.5f, TOL);
}

TEST(FuzzyTConorm, Drastic_OneMin) {
    // drastic: max(a,b) if min(a,b)=0, else 1
    EXPECT_NEAR(fuzzy_tconorm(0.5f, 0.0f, FUZZY_TCONORM_DRASTIC), 0.5f, TOL);
}

TEST(FuzzyTConorm, Drastic_NeitherMin) {
    EXPECT_NEAR(fuzzy_tconorm(0.5f, 0.5f, FUZZY_TCONORM_DRASTIC), 1.0f, TOL);
}

TEST(FuzzyTConorm, Einstein_BasicValues) {
    // (a+b)/(1+a*b) = (0.5+0.5)/(1+0.25) = 1.0/1.25 = 0.8
    float result = fuzzy_tconorm(0.5f, 0.5f, FUZZY_TCONORM_EINSTEIN);
    EXPECT_NEAR(result, 0.8f, TOL);
}

TEST(FuzzyTConorm, NilpotentMax_SumLessThanOne) {
    // max(a,b) if a+b < 1, else 1
    EXPECT_NEAR(fuzzy_tconorm(0.3f, 0.4f, FUZZY_TCONORM_NILPOTENT_MAX), 0.4f, TOL);
}

TEST(FuzzyTConorm, NilpotentMax_SumGreaterOrEqual) {
    EXPECT_NEAR(fuzzy_tconorm(0.6f, 0.7f, FUZZY_TCONORM_NILPOTENT_MAX), 1.0f, TOL);
}

// ============================================================================
// 3. Complement Tests (Fuzzy NOT)
// ============================================================================

TEST(FuzzyComplement, Standard) {
    // 1 - a
    EXPECT_NEAR(fuzzy_complement(0.3f, FUZZY_COMPLEMENT_STANDARD, 0.0f), 0.7f, TOL);
}

TEST(FuzzyComplement, Standard_Zero) {
    EXPECT_NEAR(fuzzy_complement(0.0f, FUZZY_COMPLEMENT_STANDARD, 0.0f), 1.0f, TOL);
}

TEST(FuzzyComplement, Standard_One) {
    EXPECT_NEAR(fuzzy_complement(1.0f, FUZZY_COMPLEMENT_STANDARD, 0.0f), 0.0f, TOL);
}

TEST(FuzzyComplement, Sugeno) {
    // (1-a)/(1+lambda*a), lambda=0.5, a=0.4: (0.6)/(1+0.2) = 0.6/1.2 = 0.5
    float result = fuzzy_complement(0.4f, FUZZY_COMPLEMENT_SUGENO, 0.5f);
    EXPECT_NEAR(result, 0.5f, TOL);
}

TEST(FuzzyComplement, Yager) {
    // (1 - a^w)^(1/w), w=2, a=0.5: (1 - 0.25)^(0.5) = sqrt(0.75)
    float result = fuzzy_complement(0.5f, FUZZY_COMPLEMENT_YAGER, 2.0f);
    float expected = std::sqrt(0.75f);
    EXPECT_NEAR(result, expected, TOL);
}

// ============================================================================
// 4. Implication Tests
// ============================================================================

TEST(FuzzyImplication, Mamdani) {
    // min(a, b)
    EXPECT_NEAR(fuzzy_implication(0.3f, 0.7f, FUZZY_IMPL_MAMDANI), 0.3f, TOL);
}

TEST(FuzzyImplication, Larsen) {
    // a * b
    EXPECT_NEAR(fuzzy_implication(0.3f, 0.7f, FUZZY_IMPL_LARSEN), 0.21f, TOL);
}

TEST(FuzzyImplication, Lukasiewicz) {
    // min(1, 1-a+b) = min(1, 1-0.3+0.7) = min(1, 1.4) = 1.0
    EXPECT_NEAR(fuzzy_implication(0.3f, 0.7f, FUZZY_IMPL_LUKASIEWICZ), 1.0f, TOL);
}

TEST(FuzzyImplication, KleeneDienes) {
    // max(1-a, b) = max(0.7, 0.7) = 0.7
    EXPECT_NEAR(fuzzy_implication(0.3f, 0.7f, FUZZY_IMPL_KLEENE_DIENES), 0.7f, TOL);
}

TEST(FuzzyImplication, Zadeh) {
    // max(min(a,b), 1-a) = max(min(0.3,0.7), 0.7) = max(0.3, 0.7) = 0.7
    EXPECT_NEAR(fuzzy_implication(0.3f, 0.7f, FUZZY_IMPL_ZADEH), 0.7f, TOL);
}

TEST(FuzzyImplication, Godel) {
    // b if a <= b, else 1 => a=0.3, b=0.7, a<=b, result=1.0
    // Actually Godel: if a<=b return 1.0, else return b
    EXPECT_NEAR(fuzzy_implication(0.3f, 0.7f, FUZZY_IMPL_GODEL), 1.0f, TOL);
}

// ============================================================================
// 5. Aggregation Tests
// ============================================================================

TEST(FuzzyAggregation, Max) {
    EXPECT_NEAR(fuzzy_aggregate(0.3f, 0.7f, FUZZY_AGG_MAX), 0.7f, TOL);
}

TEST(FuzzyAggregation, AlgebraicSum) {
    // a + b - a*b = 0.3 + 0.7 - 0.21 = 0.79
    EXPECT_NEAR(fuzzy_aggregate(0.3f, 0.7f, FUZZY_AGG_ALGEBRAIC_SUM), 0.79f, TOL);
}

TEST(FuzzyAggregation, BoundedSum) {
    // min(1, a+b) = min(1, 1.0) = 1.0
    EXPECT_NEAR(fuzzy_aggregate(0.3f, 0.7f, FUZZY_AGG_BOUNDED_SUM), 1.0f, TOL);
}

TEST(FuzzyAggregation, EinsteinSum) {
    // (a+b)/(1+a*b) = (0.3+0.7)/(1+0.21) = 1.0/1.21
    float expected = 1.0f / 1.21f;
    EXPECT_NEAR(fuzzy_aggregate(0.3f, 0.7f, FUZZY_AGG_EINSTEIN_SUM), expected, TOL);
}

TEST(FuzzyAggregation, NormalizedSum) {
    // (a+b)/max_sum normalized -- at least the sum should be reasonable
    float result = fuzzy_aggregate(0.3f, 0.7f, FUZZY_AGG_NORMALIZED_SUM);
    EXPECT_GE(result, 0.0f);
    EXPECT_LE(result, 1.0f);
}

// ============================================================================
// 6. Array Operations
// ============================================================================

TEST(FuzzyArrayOps, TNormArray_Min) {
    float vals[] = {0.3f, 0.7f, 0.5f, 0.9f};
    float result = fuzzy_tnorm_array(vals, 4, FUZZY_TNORM_MIN);
    EXPECT_NEAR(result, 0.3f, TOL);
}

TEST(FuzzyArrayOps, TConormArray_Max) {
    float vals[] = {0.3f, 0.7f, 0.5f, 0.9f};
    float result = fuzzy_tconorm_array(vals, 4, FUZZY_TCONORM_MAX);
    EXPECT_NEAR(result, 0.9f, TOL);
}

TEST(FuzzyArrayOps, AggregateArray_Max) {
    float vals[] = {0.2f, 0.8f, 0.4f};
    float result = fuzzy_aggregate_array(vals, 3, FUZZY_AGG_MAX);
    EXPECT_NEAR(result, 0.8f, TOL);
}

TEST(FuzzyArrayOps, TNormArray_AlgebraicProduct) {
    float vals[] = {0.5f, 0.5f, 0.5f};
    float result = fuzzy_tnorm_array(vals, 3, FUZZY_TNORM_ALGEBRAIC_PRODUCT);
    EXPECT_NEAR(result, 0.125f, TOL);
}

// ============================================================================
// 7. Weighted Operations
// ============================================================================

TEST(FuzzyWeighted, WeightedAverage) {
    float vals[] = {0.2f, 0.8f};
    float wts[]  = {1.0f, 1.0f};
    float result = fuzzy_weighted_average(vals, wts, 2);
    EXPECT_NEAR(result, 0.5f, TOL);
}

TEST(FuzzyWeighted, WeightedAverage_Unequal) {
    float vals[] = {0.0f, 1.0f};
    float wts[]  = {1.0f, 3.0f};
    float result = fuzzy_weighted_average(vals, wts, 2);
    EXPECT_NEAR(result, 0.75f, TOL);
}

TEST(FuzzyWeighted, WeightedTNorm) {
    float vals[] = {0.5f, 0.5f};
    float wts[]  = {1.0f, 1.0f};
    float result = fuzzy_weighted_tnorm(vals, wts, 2, FUZZY_TNORM_MIN);
    // With equal weights, behavior depends on implementation
    EXPECT_GE(result, 0.0f);
    EXPECT_LE(result, 1.0f);
}

TEST(FuzzyWeighted, WeightedTConorm) {
    float vals[] = {0.3f, 0.7f};
    float wts[]  = {1.0f, 1.0f};
    float result = fuzzy_weighted_tconorm(vals, wts, 2, FUZZY_TCONORM_MAX);
    EXPECT_GE(result, 0.0f);
    EXPECT_LE(result, 1.0f);
}

// ============================================================================
// 8. Fuzzy Relations
// ============================================================================

TEST(FuzzyRelation, Compose_2x2) {
    // rel_a: 2x2, rel_b: 2x2 -> out: 2x2
    float rel_a[] = {0.5f, 0.8f, 0.3f, 0.6f};
    float rel_b[] = {0.7f, 0.4f, 0.9f, 0.2f};
    float out[4] = {0};

    int rc = fuzzy_relation_compose(rel_a, 2, 2, rel_b, 2, 2, out,
                                     FUZZY_TNORM_MIN, FUZZY_TCONORM_MAX);
    EXPECT_EQ(rc, 0);
    // out[0] = max(min(0.5,0.7), min(0.8,0.9)) = max(0.5, 0.8) = 0.8
    EXPECT_NEAR(out[0], 0.8f, TOL);
}

TEST(FuzzyRelation, Compose_NullInputs) {
    float out[4] = {0};
    float rel[] = {0.5f, 0.5f, 0.5f, 0.5f};
    int rc = fuzzy_relation_compose(nullptr, 2, 2, rel, 2, 2, out,
                                     FUZZY_TNORM_MIN, FUZZY_TCONORM_MAX);
    EXPECT_NE(rc, 0);
}

// ============================================================================
// 9. Set-Level Operations (fuzzy_value_t)
// ============================================================================

TEST(FuzzyValueOps, And_MinTermwise) {
    fuzzy_value_t a = {};
    fuzzy_value_t b = {};
    fuzzy_value_t out = {};
    a.num_terms = 3;
    b.num_terms = 3;
    a.memberships[0] = 0.3f; a.memberships[1] = 0.7f; a.memberships[2] = 0.5f;
    b.memberships[0] = 0.6f; b.memberships[1] = 0.4f; b.memberships[2] = 0.9f;

    int rc = fuzzy_value_and(&a, &b, FUZZY_TNORM_MIN, &out);
    EXPECT_EQ(rc, 0);
    EXPECT_NEAR(out.memberships[0], 0.3f, TOL);
    EXPECT_NEAR(out.memberships[1], 0.4f, TOL);
    EXPECT_NEAR(out.memberships[2], 0.5f, TOL);
}

TEST(FuzzyValueOps, Or_MaxTermwise) {
    fuzzy_value_t a = {};
    fuzzy_value_t b = {};
    fuzzy_value_t out = {};
    a.num_terms = 3;
    b.num_terms = 3;
    a.memberships[0] = 0.3f; a.memberships[1] = 0.7f; a.memberships[2] = 0.5f;
    b.memberships[0] = 0.6f; b.memberships[1] = 0.4f; b.memberships[2] = 0.9f;

    int rc = fuzzy_value_or(&a, &b, FUZZY_TCONORM_MAX, &out);
    EXPECT_EQ(rc, 0);
    EXPECT_NEAR(out.memberships[0], 0.6f, TOL);
    EXPECT_NEAR(out.memberships[1], 0.7f, TOL);
    EXPECT_NEAR(out.memberships[2], 0.9f, TOL);
}

TEST(FuzzyValueOps, Not_Standard) {
    fuzzy_value_t a = {};
    fuzzy_value_t out = {};
    a.num_terms = 2;
    a.memberships[0] = 0.3f;
    a.memberships[1] = 0.8f;

    int rc = fuzzy_value_not(&a, FUZZY_COMPLEMENT_STANDARD, 0.0f, &out);
    EXPECT_EQ(rc, 0);
    EXPECT_NEAR(out.memberships[0], 0.7f, TOL);
    EXPECT_NEAR(out.memberships[1], 0.2f, TOL);
}

TEST(FuzzyValueOps, And_NullInput) {
    fuzzy_value_t b = {};
    fuzzy_value_t out = {};
    b.num_terms = 1;
    int rc = fuzzy_value_and(nullptr, &b, FUZZY_TNORM_MIN, &out);
    EXPECT_NE(rc, 0);
}

// ============================================================================
// 10. Similarity & Distance
// ============================================================================

TEST(FuzzySimilarity, IdenticalSets_MaxSimilarity) {
    float set[] = {0.2f, 0.5f, 0.8f};
    float sim = fuzzy_set_similarity(set, set, 3);
    EXPECT_NEAR(sim, 1.0f, TOL);
}

TEST(FuzzySimilarity, DifferentSets) {
    float a[] = {1.0f, 0.0f, 0.0f};
    float b[] = {0.0f, 0.0f, 1.0f};
    float sim = fuzzy_set_similarity(a, b, 3);
    EXPECT_LT(sim, 0.5f);
}

TEST(FuzzyDistance, IdenticalSets_ZeroDistance) {
    float set[] = {0.5f, 0.5f, 0.5f};
    float dist = fuzzy_set_distance(set, set, 3);
    EXPECT_NEAR(dist, 0.0f, TOL);
}

TEST(FuzzyDistance, DifferentSets_Positive) {
    float a[] = {1.0f, 0.0f};
    float b[] = {0.0f, 1.0f};
    float dist = fuzzy_set_distance(a, b, 2);
    EXPECT_GT(dist, 0.0f);
}

TEST(FuzzyInclusion, SubsetOfItself) {
    float set[] = {0.3f, 0.7f};
    float inc = fuzzy_set_inclusion(set, set, 2);
    EXPECT_NEAR(inc, 1.0f, TOL);
}

// ============================================================================
// 11. Statistics
// ============================================================================

TEST(FuzzyOperatorStats, GetStats) {
    fuzzy_operator_stats_t stats;
    int rc = fuzzy_operator_get_stats(&stats);
    EXPECT_EQ(rc, 0);
}

TEST(FuzzyOperatorStats, GetStats_Null) {
    int rc = fuzzy_operator_get_stats(nullptr);
    EXPECT_NE(rc, 0);
}

TEST(FuzzyOperatorStats, ResetStats_NoOp) {
    fuzzy_operator_reset_stats();  // should not crash
}

// ============================================================================
// 12. Edge Cases
// ============================================================================

TEST(FuzzyEdgeCases, TNorm_BothZero) {
    EXPECT_NEAR(fuzzy_tnorm(0.0f, 0.0f, FUZZY_TNORM_MIN), 0.0f, TOL);
}

TEST(FuzzyEdgeCases, TConorm_BothOne) {
    EXPECT_NEAR(fuzzy_tconorm(1.0f, 1.0f, FUZZY_TCONORM_MAX), 1.0f, TOL);
}

TEST(FuzzyEdgeCases, TNorm_BothOne) {
    EXPECT_NEAR(fuzzy_tnorm(1.0f, 1.0f, FUZZY_TNORM_ALGEBRAIC_PRODUCT), 1.0f, TOL);
}

TEST(FuzzyEdgeCases, TConorm_BothZero) {
    EXPECT_NEAR(fuzzy_tconorm(0.0f, 0.0f, FUZZY_TCONORM_ALGEBRAIC_SUM), 0.0f, TOL);
}
