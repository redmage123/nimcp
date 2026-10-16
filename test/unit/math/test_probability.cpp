/**
 * @file test_probability.cpp
 * @brief Tests for the probability and statistics engine (Google Test)
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

extern "C" {
#include "cognitive/math/nimcp_probability.h"
}

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const double TOL = 1e-6;

/* ---------- lifecycle ---------- */

TEST(ProbabilityTest, CreateDestroy) {
    probability_engine_t *eng = probability_create(42);
    ASSERT_NE(eng, nullptr);
    probability_destroy(eng);
}

/* ---------- normal PDF at mean is maximum ---------- */

TEST(ProbabilityTest, NormalPdfAtMean) {
    dist_params_t d = {0};
    d.type = DIST_NORMAL;
    d.mu = 5.0;
    d.sigma = 2.0;

    double pdf_at_mean = prob_pdf(&d, 5.0);
    /* PDF of normal at mean = 1/(sigma*sqrt(2*pi)) */
    double expected = 1.0 / (2.0 * sqrt(2.0 * M_PI));
    EXPECT_NEAR(pdf_at_mean, expected, TOL);

    /* PDF slightly off-mean should be less */
    double pdf_off = prob_pdf(&d, 5.5);
    EXPECT_LT(pdf_off, pdf_at_mean);
}

/* ---------- uniform CDF ---------- */

TEST(ProbabilityTest, UniformCdfMidpoint) {
    dist_params_t d = {0};
    d.type = DIST_UNIFORM;
    d.a = 0.0;
    d.b = 1.0;

    EXPECT_NEAR(prob_cdf(&d, 0.5), 0.5, TOL);
    EXPECT_NEAR(prob_cdf(&d, 0.0), 0.0, TOL);
    EXPECT_NEAR(prob_cdf(&d, 1.0), 1.0, TOL);
    EXPECT_NEAR(prob_cdf(&d, 0.25), 0.25, TOL);
}

/* ---------- Poisson PMF ---------- */

TEST(ProbabilityTest, PoissonPmf) {
    /* P(k=0 | lambda=1) = e^{-1} */
    dist_params_t d = {0};
    d.type = DIST_POISSON;
    d.lambda = 1.0;

    double p0 = prob_pdf(&d, 0.0);
    EXPECT_NEAR(p0, exp(-1.0), TOL);

    /* P(k=1 | lambda=1) = e^{-1} */
    double p1 = prob_pdf(&d, 1.0);
    EXPECT_NEAR(p1, exp(-1.0), TOL);
}

/* ---------- Pearson correlation ---------- */

TEST(ProbabilityTest, PearsonIdentical) {
    double x[] = {1.0, 2.0, 3.0, 4.0, 5.0};
    double r = prob_pearson(x, x, 5);
    EXPECT_NEAR(r, 1.0, TOL);
}

TEST(ProbabilityTest, PearsonNegativelyCorrelated) {
    double x[] = {1.0, 2.0, 3.0, 4.0, 5.0};
    double y[] = {5.0, 4.0, 3.0, 2.0, 1.0};
    double r = prob_pearson(x, y, 5);
    EXPECT_NEAR(r, -1.0, TOL);
}

/* ---------- z-test ---------- */

TEST(ProbabilityTest, ZTestRejectsLargeShift) {
    /* Data from N(10, 1): test H0: mu=5 with known sigma=1 => reject */
    double data[20];
    for (int i = 0; i < 20; i++) data[i] = 10.0 + 0.1 * (i - 10);

    test_result_t res = prob_z_test(data, 20, 5.0, 1.0);
    EXPECT_TRUE(res.reject_null);
    EXPECT_GT(fabs(res.statistic), 3.0);
}

TEST(ProbabilityTest, ZTestFailsToRejectCorrectMean) {
    /* Data centered around 5 with sigma=1 => do not reject mu=5 */
    double data[30];
    for (int i = 0; i < 30; i++) data[i] = 5.0 + 0.05 * (i - 15);

    test_result_t res = prob_z_test(data, 30, 5.0, 1.0);
    EXPECT_FALSE(res.reject_null);
}

/* ---------- descriptive statistics ---------- */

TEST(ProbabilityTest, DescriptiveStats) {
    double data[] = {2.0, 4.0, 4.0, 4.0, 5.0, 5.0, 7.0, 9.0};
    descriptive_stats_t s = prob_descriptive(data, 8);

    EXPECT_EQ(s.n, 8);
    EXPECT_NEAR(s.mean, 5.0, TOL);
    EXPECT_NEAR(s.min, 2.0, TOL);
    EXPECT_NEAR(s.max, 9.0, TOL);
    EXPECT_GT(s.variance, 0.0);
    EXPECT_GT(s.std_dev, 0.0);
}

/* ---------- normal CDF ---------- */

TEST(ProbabilityTest, NormalCdfSymmetry) {
    dist_params_t d = {0};
    d.type = DIST_NORMAL;
    d.mu = 0.0;
    d.sigma = 1.0;

    /* CDF at mean = 0.5 */
    EXPECT_NEAR(prob_cdf(&d, 0.0), 0.5, 1e-3);
    /* CDF(-x) + CDF(x) = 1 */
    double cdf_neg = prob_cdf(&d, -1.96);
    double cdf_pos = prob_cdf(&d, 1.96);
    EXPECT_NEAR(cdf_neg + cdf_pos, 1.0, 1e-3);
}

/* ---------- Spearman correlation ---------- */

TEST(ProbabilityTest, SpearmanMonotonic) {
    double x[] = {1.0, 2.0, 3.0, 4.0, 5.0};
    double y[] = {2.0, 4.0, 8.0, 16.0, 32.0}; /* monotonically increasing */
    double r = prob_spearman(x, y, 5);
    EXPECT_NEAR(r, 1.0, TOL);
}
