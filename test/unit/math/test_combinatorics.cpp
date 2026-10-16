/**
 * @file test_combinatorics.cpp
 * @brief Tests for the combinatorics engine (Google Test)
 */

#include <gtest/gtest.h>

extern "C" {
#include "cognitive/math/nimcp_combinatorics.h"
}

/* ---------- lifecycle ---------- */

TEST(CombinatoricsTest, CreateDestroy) {
    combinatorics_t *ctx = combinatorics_create();
    ASSERT_NE(ctx, nullptr);
    combinatorics_destroy(ctx);
}

/* ---------- fixture for tests needing context ---------- */

class CombinatoricsFixture : public ::testing::Test {
protected:
    void SetUp() override {
        ctx = combinatorics_create();
        ASSERT_NE(ctx, nullptr);
    }
    void TearDown() override {
        combinatorics_destroy(ctx);
    }
    combinatorics_t *ctx;
};

/* ---------- factorial ---------- */

TEST_F(CombinatoricsFixture, FactorialKnown) {
    EXPECT_EQ(comb_factorial(ctx, 0), 1u);
    EXPECT_EQ(comb_factorial(ctx, 1), 1u);
    EXPECT_EQ(comb_factorial(ctx, 5), 120u);
    EXPECT_EQ(comb_factorial(ctx, 10), 3628800u);
    EXPECT_EQ(comb_factorial(ctx, 20), (uint64_t)2432902008176640000ULL);
}

/* ---------- binomial coefficients ---------- */

TEST_F(CombinatoricsFixture, BinomialKnown) {
    EXPECT_EQ(comb_binomial(ctx, 10, 3), 120u);
    EXPECT_EQ(comb_binomial(ctx, 10, 0), 1u);
    EXPECT_EQ(comb_binomial(ctx, 10, 10), 1u);
    EXPECT_EQ(comb_binomial(ctx, 6, 3), 20u);
    EXPECT_EQ(comb_binomial(ctx, 20, 10), 184756u);
}

/* ---------- Catalan numbers ---------- */

TEST_F(CombinatoricsFixture, CatalanKnown) {
    EXPECT_EQ(comb_catalan(ctx, 0), 1u);
    EXPECT_EQ(comb_catalan(ctx, 1), 1u);
    EXPECT_EQ(comb_catalan(ctx, 2), 2u);
    EXPECT_EQ(comb_catalan(ctx, 3), 5u);
    EXPECT_EQ(comb_catalan(ctx, 4), 14u);
    EXPECT_EQ(comb_catalan(ctx, 5), 42u);
}

/* ---------- Fibonacci ---------- */

TEST(CombinatoricsTest, FibonacciKnown) {
    EXPECT_EQ(comb_fibonacci(0), 0u);
    EXPECT_EQ(comb_fibonacci(1), 1u);
    EXPECT_EQ(comb_fibonacci(2), 1u);
    EXPECT_EQ(comb_fibonacci(5), 5u);
    EXPECT_EQ(comb_fibonacci(10), 55u);
    EXPECT_EQ(comb_fibonacci(20), 6765u);
}

/* ---------- derangements ---------- */

TEST_F(CombinatoricsFixture, DerangementKnown) {
    /* D(0)=1, D(1)=0, D(2)=1, D(3)=2, D(4)=9, D(5)=44 */
    EXPECT_EQ(comb_derangement(ctx, 0), 1u);
    EXPECT_EQ(comb_derangement(ctx, 1), 0u);
    EXPECT_EQ(comb_derangement(ctx, 2), 1u);
    EXPECT_EQ(comb_derangement(ctx, 3), 2u);
    EXPECT_EQ(comb_derangement(ctx, 4), 9u);
    EXPECT_EQ(comb_derangement(ctx, 5), 44u);
}

/* ---------- Stirling numbers (second kind) ---------- */

TEST(CombinatoricsTest, StirlingSecondKnown) {
    /* S(4,2)=7, S(4,1)=1, S(4,4)=1, S(5,3)=25 */
    EXPECT_EQ(comb_stirling_second(4, 2), 7u);
    EXPECT_EQ(comb_stirling_second(4, 1), 1u);
    EXPECT_EQ(comb_stirling_second(4, 4), 1u);
    EXPECT_EQ(comb_stirling_second(5, 3), 25u);
    EXPECT_EQ(comb_stirling_second(3, 2), 3u);
}

/* ---------- Stirling numbers (first kind) ---------- */

TEST(CombinatoricsTest, StirlingFirstKnown) {
    /* |s(3,1)|=2, |s(3,2)|=3, |s(3,3)|=1, |s(4,2)|=11 */
    EXPECT_EQ(comb_stirling_first(3, 1), 2u);
    EXPECT_EQ(comb_stirling_first(3, 2), 3u);
    EXPECT_EQ(comb_stirling_first(3, 3), 1u);
    EXPECT_EQ(comb_stirling_first(4, 2), 11u);
}

/* ---------- Bell numbers ---------- */

TEST(CombinatoricsTest, BellKnown) {
    /* B(0)=1, B(1)=1, B(2)=2, B(3)=5, B(4)=15, B(5)=52 */
    EXPECT_EQ(comb_bell(0), 1u);
    EXPECT_EQ(comb_bell(1), 1u);
    EXPECT_EQ(comb_bell(2), 2u);
    EXPECT_EQ(comb_bell(3), 5u);
    EXPECT_EQ(comb_bell(4), 15u);
    EXPECT_EQ(comb_bell(5), 52u);
}

/* ---------- permutation ---------- */

TEST_F(CombinatoricsFixture, PermutationKnown) {
    /* P(5,2) = 20, P(10,3) = 720 */
    EXPECT_EQ(comb_permutation(ctx, 5, 2), 20u);
    EXPECT_EQ(comb_permutation(ctx, 10, 3), 720u);
    EXPECT_EQ(comb_permutation(ctx, 5, 5), 120u);  /* = 5! */
}

/* ---------- partition function ---------- */

TEST(CombinatoricsTest, PartitionKnown) {
    /* p(0)=1, p(1)=1, p(2)=2, p(3)=3, p(4)=5, p(5)=7, p(10)=42 */
    EXPECT_EQ(comb_partition(0), 1u);
    EXPECT_EQ(comb_partition(1), 1u);
    EXPECT_EQ(comb_partition(4), 5u);
    EXPECT_EQ(comb_partition(5), 7u);
    EXPECT_EQ(comb_partition(10), 42u);
}

/* ---------- generating function ---------- */

TEST(CombinatoricsTest, OgfEvaluate) {
    /* 1 + 2x + 3x^2 at x=2 => 1+4+12 = 17 */
    double c[] = {1.0, 2.0, 3.0};
    ogf_t gf = comb_ogf_create(c, 3);
    EXPECT_NEAR(comb_ogf_evaluate(&gf, 2.0), 17.0, 1e-9);
    EXPECT_NEAR(comb_ogf_evaluate(&gf, 0.0), 1.0, 1e-9);
}
