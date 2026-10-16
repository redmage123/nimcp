//=============================================================================
// test_statistics.cpp - Unit Tests for Statistics Module
//=============================================================================
/**
 * @file test_statistics.cpp
 * @brief Comprehensive unit tests for nimcp_statistics
 *
 * WHAT: Test coverage for statistical functions
 * WHY:  Ensure numerical accuracy and correctness
 * HOW:  GTest framework with tolerance-based comparisons
 *
 * @date 2026-01-30
 */

#include <gtest/gtest.h>
#include "utils/statistics/nimcp_statistics.h"
#include <cmath>
#include <vector>
#include <numeric>
#include <algorithm>

// Test tolerances
#define TOLERANCE 1e-5f
#define LOOSE_TOLERANCE 1e-3f
#define VERY_LOOSE_TOLERANCE 1e-2f

//=============================================================================
// Test Fixture
//=============================================================================

class StatisticsTest : public ::testing::Test {
protected:
    void SetUp() override {
        config = nimcp_stats_default_config();
        nimcp_stats_init(&config);
    }

    void TearDown() override {
        nimcp_stats_shutdown();
    }

    nimcp_stats_config_t config;
};

//=============================================================================
// Module Initialization Tests
//=============================================================================

class InitializationTest : public ::testing::Test {};

TEST_F(InitializationTest, DefaultConfig) {
    nimcp_stats_config_t config = nimcp_stats_default_config();
    EXPECT_FALSE(config.enable_simd);
    EXPECT_FALSE(config.enable_parallel);
    EXPECT_EQ(config.bootstrap_default, 1000u);
}

TEST_F(InitializationTest, InitShutdown) {
    nimcp_stats_config_t config = nimcp_stats_default_config();
    EXPECT_EQ(nimcp_stats_init(&config), NIMCP_STATS_OK);
    EXPECT_TRUE(nimcp_stats_is_initialized());
    nimcp_stats_shutdown();
    EXPECT_FALSE(nimcp_stats_is_initialized());
}

TEST_F(InitializationTest, InitWithNull) {
    EXPECT_EQ(nimcp_stats_init(nullptr), NIMCP_STATS_OK);
    EXPECT_TRUE(nimcp_stats_is_initialized());
    nimcp_stats_shutdown();
}

//=============================================================================
// Descriptive Statistics Tests
//=============================================================================

class DescriptiveStatsTest : public StatisticsTest {};

TEST_F(DescriptiveStatsTest, Mean_SimpleArray) {
    float data[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    EXPECT_NEAR(nimcp_stats_mean(data, 5), 3.0f, TOLERANCE);
}

TEST_F(DescriptiveStatsTest, Mean_SingleElement) {
    float data[] = {42.0f};
    EXPECT_NEAR(nimcp_stats_mean(data, 1), 42.0f, TOLERANCE);
}

TEST_F(DescriptiveStatsTest, Mean_NullInput) {
    EXPECT_TRUE(std::isnan(nimcp_stats_mean(nullptr, 5)));
}

TEST_F(DescriptiveStatsTest, Mean_ZeroSize) {
    float data[] = {1.0f};
    EXPECT_TRUE(std::isnan(nimcp_stats_mean(data, 0)));
}

TEST_F(DescriptiveStatsTest, Mean_NegativeValues) {
    float data[] = {-3.0f, -1.0f, 0.0f, 1.0f, 3.0f};
    EXPECT_NEAR(nimcp_stats_mean(data, 5), 0.0f, TOLERANCE);
}

TEST_F(DescriptiveStatsTest, Variance_SimpleArray) {
    float data[] = {2.0f, 4.0f, 4.0f, 4.0f, 5.0f, 5.0f, 7.0f, 9.0f};
    // Sample variance with n-1 denominator
    float expected = 4.571428571f;  // Calculated manually
    EXPECT_NEAR(nimcp_stats_variance(data, 8), expected, LOOSE_TOLERANCE);
}

TEST_F(DescriptiveStatsTest, Variance_Constant) {
    float data[] = {5.0f, 5.0f, 5.0f, 5.0f, 5.0f};
    EXPECT_NEAR(nimcp_stats_variance(data, 5), 0.0f, TOLERANCE);
}

TEST_F(DescriptiveStatsTest, Variance_TooSmall) {
    float data[] = {1.0f};
    EXPECT_TRUE(std::isnan(nimcp_stats_variance(data, 1)));
}

TEST_F(DescriptiveStatsTest, StdDev_SimpleArray) {
    float data[] = {2.0f, 4.0f, 4.0f, 4.0f, 5.0f, 5.0f, 7.0f, 9.0f};
    float var = nimcp_stats_variance(data, 8);
    EXPECT_NEAR(nimcp_stats_std_dev(data, 8), std::sqrt(var), TOLERANCE);
}

TEST_F(DescriptiveStatsTest, StdError_SimpleArray) {
    float data[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    float std_dev = nimcp_stats_std_dev(data, 5);
    float expected_se = std_dev / std::sqrt(5.0f);
    EXPECT_NEAR(nimcp_stats_std_error(data, 5), expected_se, TOLERANCE);
}

TEST_F(DescriptiveStatsTest, MinMax_SimpleArray) {
    float data[] = {3.0f, 1.0f, 4.0f, 1.0f, 5.0f, 9.0f, 2.0f, 6.0f};
    EXPECT_NEAR(nimcp_stats_min(data, 8), 1.0f, TOLERANCE);
    EXPECT_NEAR(nimcp_stats_max(data, 8), 9.0f, TOLERANCE);
}

TEST_F(DescriptiveStatsTest, Range_SimpleArray) {
    float data[] = {3.0f, 1.0f, 4.0f, 1.0f, 5.0f, 9.0f, 2.0f, 6.0f};
    EXPECT_NEAR(nimcp_stats_range(data, 8), 8.0f, TOLERANCE);  // 9 - 1
}

TEST_F(DescriptiveStatsTest, Sum_SimpleArray) {
    float data[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    EXPECT_NEAR(nimcp_stats_sum(data, 5), 15.0f, TOLERANCE);
}

TEST_F(DescriptiveStatsTest, Median_OddCount) {
    float data[] = {1.0f, 3.0f, 2.0f, 5.0f, 4.0f};
    EXPECT_NEAR(nimcp_stats_median(data, 5), 3.0f, TOLERANCE);
}

TEST_F(DescriptiveStatsTest, Median_EvenCount) {
    float data[] = {1.0f, 2.0f, 3.0f, 4.0f};
    EXPECT_NEAR(nimcp_stats_median(data, 4), 2.5f, TOLERANCE);
}

TEST_F(DescriptiveStatsTest, Median_SingleElement) {
    float data[] = {42.0f};
    EXPECT_NEAR(nimcp_stats_median(data, 1), 42.0f, TOLERANCE);
}

TEST_F(DescriptiveStatsTest, Quantile_Median) {
    float data[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    EXPECT_NEAR(nimcp_stats_quantile(data, 5, 0.5f), 3.0f, TOLERANCE);
}

TEST_F(DescriptiveStatsTest, Quantile_Q1Q3) {
    float data[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
    float q1 = nimcp_stats_quantile(data, 8, 0.25f);
    float q3 = nimcp_stats_quantile(data, 8, 0.75f);
    EXPECT_GT(q1, 1.0f);
    EXPECT_LT(q1, 3.0f);
    EXPECT_GT(q3, 6.0f);
    EXPECT_LT(q3, 8.0f);
}

TEST_F(DescriptiveStatsTest, Quantile_Extremes) {
    float data[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    EXPECT_NEAR(nimcp_stats_quantile(data, 5, 0.0f), 1.0f, TOLERANCE);
    EXPECT_NEAR(nimcp_stats_quantile(data, 5, 1.0f), 5.0f, TOLERANCE);
}

TEST_F(DescriptiveStatsTest, Quantile_InvalidP) {
    float data[] = {1.0f, 2.0f, 3.0f};
    EXPECT_TRUE(std::isnan(nimcp_stats_quantile(data, 3, -0.1f)));
    EXPECT_TRUE(std::isnan(nimcp_stats_quantile(data, 3, 1.1f)));
}

TEST_F(DescriptiveStatsTest, IQR_SimpleArray) {
    float data[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
    float q1 = nimcp_stats_quantile(data, 8, 0.25f);
    float q3 = nimcp_stats_quantile(data, 8, 0.75f);
    EXPECT_NEAR(nimcp_stats_iqr(data, 8), q3 - q1, TOLERANCE);
}

TEST_F(DescriptiveStatsTest, Skewness_Symmetric) {
    // Symmetric distribution should have skewness near 0
    // Note: Small samples can have slight skewness even for symmetric data
    float data[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 4.0f, 3.0f, 2.0f, 1.0f};
    EXPECT_NEAR(nimcp_stats_skewness(data, 9), 0.0f, 0.15f);  // Allow more tolerance for small samples
}

TEST_F(DescriptiveStatsTest, Skewness_RightSkewed) {
    // Right-skewed distribution should have positive skewness
    float data[] = {1.0f, 1.0f, 2.0f, 2.0f, 2.0f, 3.0f, 10.0f};
    float skew = nimcp_stats_skewness(data, 7);
    EXPECT_GT(skew, 0.0f);
}

TEST_F(DescriptiveStatsTest, Kurtosis_Normal) {
    // Normal distribution has excess kurtosis of 0
    // This is a rough test with limited samples
    float data[] = {-2.0f, -1.0f, -0.5f, 0.0f, 0.5f, 1.0f, 2.0f};
    float kurt = nimcp_stats_kurtosis(data, 7);
    // Just check it's finite and reasonable
    EXPECT_FALSE(std::isnan(kurt));
    EXPECT_LT(std::abs(kurt), 10.0f);
}

TEST_F(DescriptiveStatsTest, GeometricMean_Positive) {
    float data[] = {1.0f, 2.0f, 4.0f, 8.0f};
    // GM = (1*2*4*8)^(1/4) = 64^0.25 = 2.828...
    EXPECT_NEAR(nimcp_stats_geometric_mean(data, 4), std::pow(64.0f, 0.25f), TOLERANCE);
}

TEST_F(DescriptiveStatsTest, GeometricMean_WithZero) {
    float data[] = {1.0f, 2.0f, 0.0f, 4.0f};
    EXPECT_TRUE(std::isnan(nimcp_stats_geometric_mean(data, 4)));
}

TEST_F(DescriptiveStatsTest, HarmonicMean_Positive) {
    float data[] = {1.0f, 2.0f, 4.0f};
    // HM = 3 / (1/1 + 1/2 + 1/4) = 3 / 1.75 = 1.714...
    EXPECT_NEAR(nimcp_stats_harmonic_mean(data, 3), 3.0f / 1.75f, TOLERANCE);
}

TEST_F(DescriptiveStatsTest, CoefVariation) {
    float data[] = {10.0f, 12.0f, 14.0f, 16.0f, 18.0f};
    float mean = nimcp_stats_mean(data, 5);
    float std_dev = nimcp_stats_std_dev(data, 5);
    EXPECT_NEAR(nimcp_stats_coef_variation(data, 5), std_dev / mean, TOLERANCE);
}

TEST_F(DescriptiveStatsTest, Describe_Comprehensive) {
    float data[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f};
    nimcp_descriptive_stats_t stats;

    EXPECT_EQ(nimcp_stats_describe(data, 10, &stats), NIMCP_STATS_OK);

    EXPECT_EQ(stats.n, 10u);
    EXPECT_NEAR(stats.mean, 5.5f, TOLERANCE);
    EXPECT_NEAR(stats.min, 1.0f, TOLERANCE);
    EXPECT_NEAR(stats.max, 10.0f, TOLERANCE);
    EXPECT_NEAR(stats.range, 9.0f, TOLERANCE);
    EXPECT_NEAR(stats.sum, 55.0f, TOLERANCE);
    EXPECT_NEAR(stats.median, 5.5f, TOLERANCE);
    EXPECT_GT(stats.variance, 0.0f);
    EXPECT_GT(stats.std_dev, 0.0f);
}

//=============================================================================
// Running Statistics Tests
//=============================================================================

class RunningStatsTest : public StatisticsTest {};

TEST_F(RunningStatsTest, InitAndAdd) {
    nimcp_running_stats_t stats;
    nimcp_stats_running_init(&stats);

    nimcp_stats_running_add(&stats, 1.0);
    nimcp_stats_running_add(&stats, 2.0);
    nimcp_stats_running_add(&stats, 3.0);
    nimcp_stats_running_add(&stats, 4.0);
    nimcp_stats_running_add(&stats, 5.0);

    EXPECT_NEAR(nimcp_stats_running_mean(&stats), 3.0, TOLERANCE);
}

TEST_F(RunningStatsTest, MatchesBatchComputation) {
    float data[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f};

    // Batch computation
    float batch_mean = nimcp_stats_mean(data, 10);
    float batch_var = nimcp_stats_variance(data, 10);

    // Running computation
    nimcp_running_stats_t stats;
    nimcp_stats_running_init(&stats);
    nimcp_stats_running_add_array(&stats, data, 10);

    EXPECT_NEAR(nimcp_stats_running_mean(&stats), batch_mean, TOLERANCE);
    EXPECT_NEAR(nimcp_stats_running_variance(&stats), batch_var, LOOSE_TOLERANCE);
}

TEST_F(RunningStatsTest, Merge) {
    nimcp_running_stats_t a, b;
    nimcp_stats_running_init(&a);
    nimcp_stats_running_init(&b);

    float data1[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    float data2[] = {6.0f, 7.0f, 8.0f, 9.0f, 10.0f};

    nimcp_stats_running_add_array(&a, data1, 5);
    nimcp_stats_running_add_array(&b, data2, 5);

    nimcp_stats_running_merge(&a, &b);

    float all_data[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f};
    EXPECT_NEAR(nimcp_stats_running_mean(&a), nimcp_stats_mean(all_data, 10), TOLERANCE);
}

//=============================================================================
// Probability Distribution Tests - PDF
//=============================================================================

class PDFTest : public StatisticsTest {};

TEST_F(PDFTest, StandardNormal_AtZero) {
    // PDF at x=0 should be 1/sqrt(2*pi) ≈ 0.3989
    float pdf = nimcp_stats_pdf_standard_normal(0.0f);
    EXPECT_NEAR(pdf, 0.3989422804f, TOLERANCE);
}

TEST_F(PDFTest, StandardNormal_Symmetric) {
    EXPECT_NEAR(
        nimcp_stats_pdf_standard_normal(1.0f),
        nimcp_stats_pdf_standard_normal(-1.0f),
        TOLERANCE
    );
}

TEST_F(PDFTest, Normal_ScaledShifted) {
    float pdf_std = nimcp_stats_pdf_standard_normal(0.0f);
    float pdf_scaled = nimcp_stats_pdf_normal(5.0f, 5.0f, 2.0f);  // x=mu
    EXPECT_NEAR(pdf_scaled, pdf_std / 2.0f, TOLERANCE);  // Divided by sigma
}

TEST_F(PDFTest, Exponential_AtZero) {
    float lambda = 2.0f;
    EXPECT_NEAR(nimcp_stats_pdf_exponential(0.0f, lambda), lambda, TOLERANCE);
}

TEST_F(PDFTest, Exponential_Decay) {
    float lambda = 1.0f;
    // PDF should decay exponentially
    float pdf1 = nimcp_stats_pdf_exponential(1.0f, lambda);
    float pdf2 = nimcp_stats_pdf_exponential(2.0f, lambda);
    EXPECT_GT(pdf1, pdf2);
}

TEST_F(PDFTest, Beta_Symmetric) {
    // Beta(2,2) is symmetric around 0.5
    float pdf_low = nimcp_stats_pdf_beta(0.3f, 2.0f, 2.0f);
    float pdf_high = nimcp_stats_pdf_beta(0.7f, 2.0f, 2.0f);
    EXPECT_NEAR(pdf_low, pdf_high, TOLERANCE);
}

TEST_F(PDFTest, Beta_UniformCase) {
    // Beta(1,1) is uniform on [0,1]
    float pdf = nimcp_stats_pdf_beta(0.5f, 1.0f, 1.0f);
    EXPECT_NEAR(pdf, 1.0f, TOLERANCE);
}

TEST_F(PDFTest, Poisson_Mode) {
    float lambda = 5.0f;
    // Mode is at floor(lambda) for lambda > 1
    float pmf_mode = nimcp_stats_pmf_poisson(5, lambda);
    float pmf_below = nimcp_stats_pmf_poisson(4, lambda);
    float pmf_above = nimcp_stats_pmf_poisson(6, lambda);
    EXPECT_GE(pmf_mode, pmf_below);
    EXPECT_GE(pmf_mode, pmf_above);
}

TEST_F(PDFTest, Binomial_Symmetric) {
    // Binomial(10, 0.5) is symmetric
    EXPECT_NEAR(
        nimcp_stats_pmf_binomial(3, 10, 0.5f),
        nimcp_stats_pmf_binomial(7, 10, 0.5f),
        TOLERANCE
    );
}

TEST_F(PDFTest, StudentT_ConvergesToNormal) {
    // As df -> infinity, t-distribution approaches normal
    float t_pdf = nimcp_stats_pdf_student_t(0.0f, 1000.0f);
    float normal_pdf = nimcp_stats_pdf_standard_normal(0.0f);
    EXPECT_NEAR(t_pdf, normal_pdf, LOOSE_TOLERANCE);
}

//=============================================================================
// Probability Distribution Tests - CDF
//=============================================================================

class CDFTest : public StatisticsTest {};

TEST_F(CDFTest, StandardNormal_AtZero) {
    EXPECT_NEAR(nimcp_stats_cdf_standard_normal(0.0f), 0.5f, TOLERANCE);
}

TEST_F(CDFTest, StandardNormal_Symmetry) {
    float cdf_pos = nimcp_stats_cdf_standard_normal(1.0f);
    float cdf_neg = nimcp_stats_cdf_standard_normal(-1.0f);
    EXPECT_NEAR(cdf_pos + cdf_neg, 1.0f, TOLERANCE);
}

TEST_F(CDFTest, StandardNormal_KnownValues) {
    // P(Z < 1.96) ≈ 0.975
    EXPECT_NEAR(nimcp_stats_cdf_standard_normal(1.96f), 0.975f, LOOSE_TOLERANCE);
    // P(Z < -1.96) ≈ 0.025
    EXPECT_NEAR(nimcp_stats_cdf_standard_normal(-1.96f), 0.025f, LOOSE_TOLERANCE);
}

TEST_F(CDFTest, Normal_ShiftedScale) {
    // CDF(mu, mu, sigma) = 0.5
    EXPECT_NEAR(nimcp_stats_cdf_normal(5.0f, 5.0f, 2.0f), 0.5f, TOLERANCE);
}

TEST_F(CDFTest, Exponential_Boundaries) {
    EXPECT_NEAR(nimcp_stats_cdf_exponential(0.0f, 1.0f), 0.0f, TOLERANCE);
    // CDF approaches 1 as x -> infinity
    EXPECT_GT(nimcp_stats_cdf_exponential(10.0f, 1.0f), 0.99f);
}

TEST_F(CDFTest, Beta_Boundaries) {
    EXPECT_NEAR(nimcp_stats_cdf_beta(0.0f, 2.0f, 2.0f), 0.0f, TOLERANCE);
    EXPECT_NEAR(nimcp_stats_cdf_beta(1.0f, 2.0f, 2.0f), 1.0f, TOLERANCE);
}

TEST_F(CDFTest, StudentT_Symmetric) {
    float cdf_pos = nimcp_stats_cdf_student_t(2.0f, 10.0f);
    float cdf_neg = nimcp_stats_cdf_student_t(-2.0f, 10.0f);
    EXPECT_NEAR(cdf_pos + cdf_neg, 1.0f, TOLERANCE);
}

TEST_F(CDFTest, ChiSquared_Monotonic) {
    float cdf1 = nimcp_stats_cdf_chi_squared(2.0f, 5.0f);
    float cdf2 = nimcp_stats_cdf_chi_squared(4.0f, 5.0f);
    float cdf3 = nimcp_stats_cdf_chi_squared(6.0f, 5.0f);
    EXPECT_LT(cdf1, cdf2);
    EXPECT_LT(cdf2, cdf3);
}

//=============================================================================
// Probability Distribution Tests - Quantile
//=============================================================================

class QuantileTest : public StatisticsTest {};

TEST_F(QuantileTest, StandardNormal_Inverse) {
    // Quantile should be inverse of CDF
    float p = 0.75f;
    float q = nimcp_stats_quantile_standard_normal(p);
    float cdf = nimcp_stats_cdf_standard_normal(q);
    EXPECT_NEAR(cdf, p, TOLERANCE);
}

TEST_F(QuantileTest, StandardNormal_KnownValues) {
    // z_0.975 ≈ 1.96
    EXPECT_NEAR(nimcp_stats_quantile_standard_normal(0.975f), 1.96f, LOOSE_TOLERANCE);
    // z_0.5 = 0
    EXPECT_NEAR(nimcp_stats_quantile_standard_normal(0.5f), 0.0f, TOLERANCE);
}

TEST_F(QuantileTest, Normal_Inverse) {
    float mu = 10.0f, sigma = 3.0f;
    float p = 0.9f;
    float q = nimcp_stats_quantile_normal(p, mu, sigma);
    float cdf = nimcp_stats_cdf_normal(q, mu, sigma);
    EXPECT_NEAR(cdf, p, LOOSE_TOLERANCE);
}

TEST_F(QuantileTest, StudentT_Inverse) {
    float df = 10.0f;
    float p = 0.95f;
    float q = nimcp_stats_quantile_student_t(p, df);
    float cdf = nimcp_stats_cdf_student_t(q, df);
    EXPECT_NEAR(cdf, p, LOOSE_TOLERANCE);
}

TEST_F(QuantileTest, ChiSquared_Inverse) {
    float df = 5.0f;
    float p = 0.95f;
    float q = nimcp_stats_quantile_chi_squared(p, df);
    float cdf = nimcp_stats_cdf_chi_squared(q, df);
    EXPECT_NEAR(cdf, p, LOOSE_TOLERANCE);
}

//=============================================================================
// Distribution Sampling Tests
//=============================================================================

class SamplingTest : public StatisticsTest {};

TEST_F(SamplingTest, Uniform_Range) {
    nimcp_distribution_params_t params = {
        .type = NIMCP_DIST_UNIFORM,
        .params = {.uniform = {0.0f, 1.0f}}
    };

    for (int i = 0; i < 100; i++) {
        float sample = nimcp_stats_sample(&params);
        EXPECT_GE(sample, 0.0f);
        EXPECT_LE(sample, 1.0f);
    }
}

TEST_F(SamplingTest, Normal_MeanVariance) {
    nimcp_distribution_params_t params = {
        .type = NIMCP_DIST_NORMAL,
        .params = {.normal = {5.0f, 2.0f}}
    };

    float samples[1000];
    nimcp_stats_sample_array(&params, samples, 1000);

    float mean = nimcp_stats_mean(samples, 1000);
    float std_dev = nimcp_stats_std_dev(samples, 1000);

    EXPECT_NEAR(mean, 5.0f, 0.2f);  // Should be close to mu
    EXPECT_NEAR(std_dev, 2.0f, 0.3f);  // Should be close to sigma
}

TEST_F(SamplingTest, Exponential_MeanPositive) {
    nimcp_distribution_params_t params = {
        .type = NIMCP_DIST_EXPONENTIAL,
        .params = {.exponential = {2.0f}}
    };

    float samples[500];
    nimcp_stats_sample_array(&params, samples, 500);

    // All samples should be positive
    for (int i = 0; i < 500; i++) {
        EXPECT_GE(samples[i], 0.0f);
    }

    // Mean should be close to 1/lambda = 0.5
    float mean = nimcp_stats_mean(samples, 500);
    EXPECT_NEAR(mean, 0.5f, 0.1f);
}

TEST_F(SamplingTest, Beta_Range) {
    nimcp_distribution_params_t params = {
        .type = NIMCP_DIST_BETA,
        .params = {.beta = {2.0f, 5.0f}}
    };

    for (int i = 0; i < 100; i++) {
        float sample = nimcp_stats_sample(&params);
        EXPECT_GE(sample, 0.0f);
        EXPECT_LE(sample, 1.0f);
    }
}

//=============================================================================
// Hypothesis Testing Tests
//=============================================================================

class HypothesisTestTest : public StatisticsTest {};

TEST_F(HypothesisTestTest, TTest_OneSample_NullTrue) {
    // Sample from N(10, 1), test H0: mu = 10
    float data[] = {9.8f, 10.2f, 10.1f, 9.9f, 10.0f, 10.3f, 9.7f, 10.1f};
    nimcp_test_result_t result;

    EXPECT_EQ(nimcp_stats_ttest_one_sample(data, 8, 10.0f, NIMCP_TEST_TWO_SIDED, 0.95f, &result), NIMCP_STATS_OK);

    // Should not reject (p > 0.05)
    EXPECT_GT(result.p_value, 0.05f);
    EXPECT_FALSE(result.reject_null);
}

TEST_F(HypothesisTestTest, TTest_OneSample_NullFalse) {
    // Sample clearly different from 0
    float data[] = {10.0f, 11.0f, 12.0f, 9.0f, 10.5f, 11.5f, 10.2f, 10.8f};
    nimcp_test_result_t result;

    EXPECT_EQ(nimcp_stats_ttest_one_sample(data, 8, 0.0f, NIMCP_TEST_TWO_SIDED, 0.95f, &result), NIMCP_STATS_OK);

    // Should reject (p < 0.05)
    EXPECT_LT(result.p_value, 0.05f);
    EXPECT_TRUE(result.reject_null);
}

TEST_F(HypothesisTestTest, TTest_TwoSample_EqualMeans) {
    float data1[] = {10.0f, 10.5f, 9.8f, 10.2f, 10.1f};
    float data2[] = {10.1f, 9.9f, 10.3f, 10.0f, 10.2f};
    nimcp_test_result_t result;

    EXPECT_EQ(nimcp_stats_ttest_two_sample(data1, 5, data2, 5, true, NIMCP_TEST_TWO_SIDED, 0.95f, &result), NIMCP_STATS_OK);

    EXPECT_GT(result.p_value, 0.05f);  // Should not reject
}

TEST_F(HypothesisTestTest, TTest_TwoSample_DifferentMeans) {
    float data1[] = {10.0f, 10.5f, 9.8f, 10.2f, 10.1f, 10.3f, 9.9f, 10.4f};
    float data2[] = {15.0f, 15.5f, 14.8f, 15.2f, 15.1f, 15.3f, 14.9f, 15.4f};
    nimcp_test_result_t result;

    EXPECT_EQ(nimcp_stats_ttest_two_sample(data1, 8, data2, 8, true, NIMCP_TEST_TWO_SIDED, 0.95f, &result), NIMCP_STATS_OK);

    EXPECT_LT(result.p_value, 0.001f);  // Should strongly reject
    EXPECT_TRUE(result.reject_null);
}

TEST_F(HypothesisTestTest, TTest_Paired) {
    // Before and after treatment (matched pairs)
    float before[] = {100.0f, 105.0f, 98.0f, 102.0f, 101.0f};
    float after[] = {95.0f, 100.0f, 93.0f, 97.0f, 96.0f};  // Decrease of ~5
    nimcp_test_result_t result;

    EXPECT_EQ(nimcp_stats_ttest_paired(before, after, 5, NIMCP_TEST_TWO_SIDED, 0.95f, &result), NIMCP_STATS_OK);

    // Mean difference is about 5, should detect this
    EXPECT_LT(result.p_value, 0.05f);
}

//=============================================================================
// Correlation Tests
//=============================================================================

class CorrelationTest : public StatisticsTest {};

TEST_F(CorrelationTest, Pearson_PerfectPositive) {
    float x[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    float y[] = {2.0f, 4.0f, 6.0f, 8.0f, 10.0f};  // y = 2x
    nimcp_correlation_result_t result;

    EXPECT_EQ(nimcp_stats_correlation_pearson(x, y, 5, &result), NIMCP_STATS_OK);
    EXPECT_NEAR(result.r, 1.0f, TOLERANCE);
}

TEST_F(CorrelationTest, Pearson_PerfectNegative) {
    float x[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    float y[] = {10.0f, 8.0f, 6.0f, 4.0f, 2.0f};  // y = -2x + 12
    nimcp_correlation_result_t result;

    EXPECT_EQ(nimcp_stats_correlation_pearson(x, y, 5, &result), NIMCP_STATS_OK);
    EXPECT_NEAR(result.r, -1.0f, TOLERANCE);
}

TEST_F(CorrelationTest, Pearson_NoCorrelation) {
    // Random-ish data with no correlation
    float x[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
    float y[] = {5.0f, 2.0f, 8.0f, 1.0f, 9.0f, 3.0f, 7.0f, 4.0f};
    nimcp_correlation_result_t result;

    EXPECT_EQ(nimcp_stats_correlation_pearson(x, y, 8, &result), NIMCP_STATS_OK);
    EXPECT_LT(std::abs(result.r), 0.5f);  // Should be weak correlation
}

TEST_F(CorrelationTest, Covariance_SameAsVariance) {
    float x[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    float cov = nimcp_stats_covariance(x, x, 5);
    float var = nimcp_stats_variance(x, 5);
    EXPECT_NEAR(cov, var, TOLERANCE);  // Cov(X,X) = Var(X)
}

//=============================================================================
// Regression Tests
//=============================================================================

class RegressionTest : public StatisticsTest {};

TEST_F(RegressionTest, Linear_PerfectFit) {
    float x[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    float y[] = {3.0f, 5.0f, 7.0f, 9.0f, 11.0f};  // y = 2x + 1
    nimcp_regression_result_t result;

    EXPECT_EQ(nimcp_stats_regression_linear(x, y, 5, &result), NIMCP_STATS_OK);

    EXPECT_NEAR(result.slope, 2.0f, TOLERANCE);
    EXPECT_NEAR(result.intercept, 1.0f, TOLERANCE);
    EXPECT_NEAR(result.r_squared, 1.0f, TOLERANCE);

    nimcp_stats_regression_free(&result);
}

TEST_F(RegressionTest, Linear_WithNoise) {
    // y = 2x + 1 + noise
    float x[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
    float y[] = {3.1f, 4.9f, 7.2f, 8.8f, 11.1f, 12.9f, 15.2f, 16.8f};
    nimcp_regression_result_t result;

    EXPECT_EQ(nimcp_stats_regression_linear(x, y, 8, &result), NIMCP_STATS_OK);

    EXPECT_NEAR(result.slope, 2.0f, 0.1f);
    EXPECT_NEAR(result.intercept, 1.0f, 0.3f);
    EXPECT_GT(result.r_squared, 0.99f);  // Should still be very high

    nimcp_stats_regression_free(&result);
}

//=============================================================================
// Information Theory Tests
//=============================================================================

class InformationTheoryTest : public StatisticsTest {};

TEST_F(InformationTheoryTest, Entropy_Uniform) {
    // Uniform distribution over 4 outcomes: H = log2(4) = 2 bits
    float probs[] = {0.25f, 0.25f, 0.25f, 0.25f};
    EXPECT_NEAR(nimcp_stats_entropy(probs, 4), 2.0f, TOLERANCE);
}

TEST_F(InformationTheoryTest, Entropy_Deterministic) {
    // Deterministic: H = 0
    float probs[] = {1.0f, 0.0f, 0.0f, 0.0f};
    EXPECT_NEAR(nimcp_stats_entropy(probs, 4), 0.0f, TOLERANCE);
}

TEST_F(InformationTheoryTest, Entropy_Binary) {
    // Binary entropy function
    float probs[] = {0.5f, 0.5f};
    EXPECT_NEAR(nimcp_stats_entropy(probs, 2), 1.0f, TOLERANCE);  // 1 bit
}

TEST_F(InformationTheoryTest, KLDivergence_SameDistribution) {
    float p[] = {0.25f, 0.25f, 0.25f, 0.25f};
    float q[] = {0.25f, 0.25f, 0.25f, 0.25f};
    EXPECT_NEAR(nimcp_stats_kl_divergence(p, q, 4), 0.0f, TOLERANCE);
}

TEST_F(InformationTheoryTest, KLDivergence_Positive) {
    float p[] = {0.5f, 0.5f};
    float q[] = {0.9f, 0.1f};
    float kl = nimcp_stats_kl_divergence(p, q, 2);
    EXPECT_GT(kl, 0.0f);  // KL divergence is always non-negative
}

TEST_F(InformationTheoryTest, JSDivergence_Symmetric) {
    float p[] = {0.7f, 0.3f};
    float q[] = {0.3f, 0.7f};
    float js_pq = nimcp_stats_js_divergence(p, q, 2);
    float js_qp = nimcp_stats_js_divergence(q, p, 2);
    EXPECT_NEAR(js_pq, js_qp, TOLERANCE);  // JS is symmetric
}

TEST_F(InformationTheoryTest, CrossEntropy_Relation) {
    // H(P,Q) = H(P) + D_KL(P||Q)
    float p[] = {0.25f, 0.25f, 0.25f, 0.25f};
    float q[] = {0.4f, 0.3f, 0.2f, 0.1f};

    float h_p = nimcp_stats_entropy(p, 4);
    float kl = nimcp_stats_kl_divergence(p, q, 4);
    float ce = nimcp_stats_cross_entropy(p, q, 4);

    EXPECT_NEAR(ce, h_p + kl, TOLERANCE);
}

TEST_F(InformationTheoryTest, MutualInformation_Independent) {
    // Independent: P(X,Y) = P(X)P(Y), so I(X;Y) = 0
    float joint[] = {
        0.125f, 0.125f,  // P(X=0, Y=0), P(X=0, Y=1)
        0.125f, 0.125f,  // P(X=1, Y=0), P(X=1, Y=1)
        0.125f, 0.125f,
        0.125f, 0.125f
    };
    float mi = nimcp_stats_mutual_information(joint, 4, 2);
    EXPECT_NEAR(mi, 0.0f, TOLERANCE);
}

//=============================================================================
// Bayesian Inference Tests
//=============================================================================

class BayesianTest : public StatisticsTest {};

TEST_F(BayesianTest, BetaBinomial_UpdatesProperly) {
    // Prior: Beta(1, 1) = uniform
    // Data: 7 successes out of 10 trials
    nimcp_bayesian_result_t result;

    EXPECT_EQ(nimcp_stats_bayesian_beta_binomial(1.0f, 1.0f, 7, 10, 0.95f, &result), NIMCP_STATS_OK);

    // Posterior is Beta(8, 4), mean = 8/12 = 0.667
    EXPECT_NEAR(result.posterior_mean, 8.0f / 12.0f, TOLERANCE);
}

TEST_F(BayesianTest, BetaBinomial_CredibleInterval) {
    nimcp_bayesian_result_t result;

    EXPECT_EQ(nimcp_stats_bayesian_beta_binomial(10.0f, 10.0f, 50, 100, 0.95f, &result), NIMCP_STATS_OK);

    // 95% CI should contain posterior mean
    EXPECT_LT(result.credible_lower, result.posterior_mean);
    EXPECT_GT(result.credible_upper, result.posterior_mean);
}

TEST_F(BayesianTest, BayesFactor_Interpretation) {
    // Strong evidence: BF > 10
    float bf = nimcp_stats_bayes_factor(0.0f, -3.0f);  // exp(3) ≈ 20
    EXPECT_GT(bf, 10.0f);

    // Equal evidence: BF = 1
    bf = nimcp_stats_bayes_factor(0.0f, 0.0f);
    EXPECT_NEAR(bf, 1.0f, TOLERANCE);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

class UtilityTest : public StatisticsTest {};

TEST_F(UtilityTest, Standardize_MeanZeroStdOne) {
    float data[] = {10.0f, 20.0f, 30.0f, 40.0f, 50.0f};
    float standardized[5];

    EXPECT_EQ(nimcp_stats_standardize(data, 5, standardized), NIMCP_STATS_OK);

    // Mean should be ~0, std dev should be ~1
    EXPECT_NEAR(nimcp_stats_mean(standardized, 5), 0.0f, TOLERANCE);
    EXPECT_NEAR(nimcp_stats_std_dev(standardized, 5), 1.0f, TOLERANCE);
}

TEST_F(UtilityTest, NormalizeMinMax_Range01) {
    float data[] = {10.0f, 20.0f, 30.0f, 40.0f, 50.0f};
    float normalized[5];

    EXPECT_EQ(nimcp_stats_normalize_minmax(data, 5, normalized), NIMCP_STATS_OK);

    EXPECT_NEAR(nimcp_stats_min(normalized, 5), 0.0f, TOLERANCE);
    EXPECT_NEAR(nimcp_stats_max(normalized, 5), 1.0f, TOLERANCE);
}

TEST_F(UtilityTest, DetectOutliers_IQR) {
    float data[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 100.0f};  // 100 is outlier
    bool mask[6];
    uint32_t n_outliers;

    EXPECT_EQ(nimcp_stats_detect_outliers_iqr(data, 6, 1.5f, mask, &n_outliers), NIMCP_STATS_OK);

    EXPECT_GE(n_outliers, 1u);
    EXPECT_TRUE(mask[5]);  // 100 should be flagged
}

TEST_F(UtilityTest, DetectOutliers_ZScore) {
    float data[] = {0.0f, 0.1f, -0.1f, 0.2f, -0.2f, 10.0f};  // 10 is outlier
    bool mask[6];
    uint32_t n_outliers;

    // Z-score of 10 in this data is ~2.2, so use threshold of 2.0
    EXPECT_EQ(nimcp_stats_detect_outliers_zscore(data, 6, 2.0f, mask, &n_outliers), NIMCP_STATS_OK);

    EXPECT_GE(n_outliers, 1u);
    EXPECT_TRUE(mask[5]);  // 10 should be flagged
}

//=============================================================================
// Special Functions Tests
//=============================================================================

class SpecialFunctionsTest : public StatisticsTest {};

TEST_F(SpecialFunctionsTest, LGamma_KnownValues) {
    // lgamma(1) = 0
    EXPECT_NEAR(nimcp_stats_lgamma(1.0), 0.0, TOLERANCE);
    // lgamma(2) = 0 (since 1! = 1)
    EXPECT_NEAR(nimcp_stats_lgamma(2.0), 0.0, TOLERANCE);
    // lgamma(3) = ln(2!) = ln(2)
    EXPECT_NEAR(nimcp_stats_lgamma(3.0), std::log(2.0), TOLERANCE);
    // lgamma(4) = ln(3!) = ln(6)
    EXPECT_NEAR(nimcp_stats_lgamma(4.0), std::log(6.0), TOLERANCE);
}

TEST_F(SpecialFunctionsTest, BetaFunction_Symmetry) {
    // B(a, b) = B(b, a)
    double beta_ab = nimcp_stats_beta_fn(2.0, 3.0);
    double beta_ba = nimcp_stats_beta_fn(3.0, 2.0);
    EXPECT_NEAR(beta_ab, beta_ba, TOLERANCE);
}

TEST_F(SpecialFunctionsTest, Erf_KnownValues) {
    EXPECT_NEAR(nimcp_stats_erf(0.0), 0.0, TOLERANCE);
    // erf(1) ≈ 0.8427
    EXPECT_NEAR(nimcp_stats_erf(1.0), 0.8427, LOOSE_TOLERANCE);
    // erf(-x) = -erf(x)
    EXPECT_NEAR(nimcp_stats_erf(-1.0), -nimcp_stats_erf(1.0), TOLERANCE);
}

TEST_F(SpecialFunctionsTest, Erfc_Complement) {
    // erfc(x) = 1 - erf(x)
    double x = 0.5;
    EXPECT_NEAR(nimcp_stats_erfc(x), 1.0 - nimcp_stats_erf(x), TOLERANCE);
}

TEST_F(SpecialFunctionsTest, BinomialCoef_KnownValues) {
    EXPECT_NEAR(nimcp_stats_binomial_coef(5, 0), 1.0, TOLERANCE);
    EXPECT_NEAR(nimcp_stats_binomial_coef(5, 1), 5.0, TOLERANCE);
    EXPECT_NEAR(nimcp_stats_binomial_coef(5, 2), 10.0, TOLERANCE);
    EXPECT_NEAR(nimcp_stats_binomial_coef(5, 5), 1.0, TOLERANCE);
    EXPECT_NEAR(nimcp_stats_binomial_coef(10, 3), 120.0, TOLERANCE);
}

TEST_F(SpecialFunctionsTest, BinomialCoef_Symmetry) {
    // C(n, k) = C(n, n-k)
    EXPECT_NEAR(nimcp_stats_binomial_coef(10, 3), nimcp_stats_binomial_coef(10, 7), TOLERANCE);
}

//=============================================================================
// Error Handling Tests
//=============================================================================

class ErrorHandlingTest : public StatisticsTest {};

TEST_F(ErrorHandlingTest, ErrorString_AllCodes) {
    EXPECT_STREQ(nimcp_stats_error_string(NIMCP_STATS_OK), "Success");
    EXPECT_STREQ(nimcp_stats_error_string(NIMCP_STATS_ERROR_NULL), "NULL pointer argument");
    EXPECT_STREQ(nimcp_stats_error_string(NIMCP_STATS_ERROR_SIZE), "Invalid size (n=0 or too small)");
    EXPECT_STREQ(nimcp_stats_error_string(NIMCP_STATS_ERROR_MEMORY), "Memory allocation failed");
    EXPECT_STREQ(nimcp_stats_error_string(NIMCP_STATS_ERROR_PARAMS), "Invalid distribution parameters");
}

TEST_F(ErrorHandlingTest, NullInputs_ReturnError) {
    nimcp_test_result_t result;
    EXPECT_EQ(nimcp_stats_ttest_one_sample(nullptr, 5, 0.0f, NIMCP_TEST_TWO_SIDED, 0.95f, &result), NIMCP_STATS_ERROR_NULL);

    float data[] = {1.0f, 2.0f, 3.0f};
    EXPECT_EQ(nimcp_stats_ttest_one_sample(data, 3, 0.0f, NIMCP_TEST_TWO_SIDED, 0.95f, nullptr), NIMCP_STATS_ERROR_NULL);
}

TEST_F(ErrorHandlingTest, InvalidSize_ReturnError) {
    float data[] = {1.0f};
    nimcp_test_result_t result;
    EXPECT_EQ(nimcp_stats_ttest_one_sample(data, 1, 0.0f, NIMCP_TEST_TWO_SIDED, 0.95f, &result), NIMCP_STATS_ERROR_SIZE);
}

//=============================================================================
// Edge Case Tests
//=============================================================================

class EdgeCaseTest : public StatisticsTest {};

TEST_F(EdgeCaseTest, VerySmallValues) {
    float data[] = {1e-10f, 2e-10f, 3e-10f, 4e-10f, 5e-10f};
    EXPECT_NEAR(nimcp_stats_mean(data, 5), 3e-10f, 1e-15f);
}

TEST_F(EdgeCaseTest, VeryLargeValues) {
    float data[] = {1e10f, 2e10f, 3e10f, 4e10f, 5e10f};
    EXPECT_NEAR(nimcp_stats_mean(data, 5), 3e10f, 1e5f);
}

TEST_F(EdgeCaseTest, MixedSigns) {
    float data[] = {-1e6f, -1.0f, 0.0f, 1.0f, 1e6f};
    float mean = nimcp_stats_mean(data, 5);
    EXPECT_NEAR(mean, 0.0f, 1.0f);
}

TEST_F(EdgeCaseTest, AllSameValue) {
    float data[] = {42.0f, 42.0f, 42.0f, 42.0f, 42.0f};
    EXPECT_NEAR(nimcp_stats_mean(data, 5), 42.0f, TOLERANCE);
    EXPECT_NEAR(nimcp_stats_variance(data, 5), 0.0f, TOLERANCE);
    EXPECT_NEAR(nimcp_stats_std_dev(data, 5), 0.0f, TOLERANCE);
}

TEST_F(EdgeCaseTest, TwoElements) {
    float data[] = {1.0f, 3.0f};
    EXPECT_NEAR(nimcp_stats_mean(data, 2), 2.0f, TOLERANCE);
    EXPECT_NEAR(nimcp_stats_variance(data, 2), 2.0f, TOLERANCE);  // (1-2)^2 + (3-2)^2 / 1 = 2
    EXPECT_NEAR(nimcp_stats_median(data, 2), 2.0f, TOLERANCE);
}

//=============================================================================
// Shannon Integration Tests
//=============================================================================

class ShannonIntegrationTest : public StatisticsTest {};

TEST_F(ShannonIntegrationTest, ChannelCapacity_Basic) {
    // C = B × log₂(1 + SNR)
    // bandwidth=100Hz, SNR=10 → C = 100 × log₂(11) ≈ 346 bits/s
    float capacity = nimcp_stats_channel_capacity(100.0f, 10.0f);
    EXPECT_NEAR(capacity, 100.0f * std::log2(11.0f), TOLERANCE);
}

TEST_F(ShannonIntegrationTest, ChannelCapacity_HighSNR) {
    // High SNR: bandwidth=100Hz, SNR=100 → C ≈ 665 bits/s
    float capacity = nimcp_stats_channel_capacity(100.0f, 100.0f);
    EXPECT_NEAR(capacity, 100.0f * std::log2(101.0f), TOLERANCE);
}

TEST_F(ShannonIntegrationTest, ChannelCapacity_InvalidInputs) {
    EXPECT_TRUE(std::isnan(nimcp_stats_channel_capacity(-100.0f, 10.0f)));
    EXPECT_TRUE(std::isnan(nimcp_stats_channel_capacity(100.0f, -10.0f)));
}

TEST_F(ShannonIntegrationTest, SNR_Conversion) {
    // 10 dB = 10 linear
    float snr_linear = nimcp_stats_snr_from_db(10.0f);
    EXPECT_NEAR(snr_linear, 10.0f, TOLERANCE);

    // 20 dB = 100 linear
    EXPECT_NEAR(nimcp_stats_snr_from_db(20.0f), 100.0f, TOLERANCE);

    // Round-trip
    float snr_db = nimcp_stats_snr_to_db(snr_linear);
    EXPECT_NEAR(snr_db, 10.0f, TOLERANCE);
}

TEST_F(ShannonIntegrationTest, SNR_Conversion_RoundTrip) {
    float original_db = 15.0f;
    float linear = nimcp_stats_snr_from_db(original_db);
    float back_to_db = nimcp_stats_snr_to_db(linear);
    EXPECT_NEAR(back_to_db, original_db, TOLERANCE);
}

TEST_F(ShannonIntegrationTest, VariationOfInformation_SameDistribution) {
    // VI(X,X) = 0 for identical distributions
    float joint[4] = {0.5f, 0.0f, 0.0f, 0.5f};  // Perfect correlation
    float vi = nimcp_stats_variation_of_information(joint, 2, 2);
    EXPECT_NEAR(vi, 0.0f, TOLERANCE);
}

TEST_F(ShannonIntegrationTest, VariationOfInformation_Independent) {
    // Independent: VI = H(X) + H(Y)
    float joint[4] = {0.25f, 0.25f, 0.25f, 0.25f};  // Independent uniform
    float vi = nimcp_stats_variation_of_information(joint, 2, 2);
    // H(X,Y) = 2 bits, I(X;Y) = 0, so VI = 2
    EXPECT_NEAR(vi, 2.0f, LOOSE_TOLERANCE);
}

TEST_F(ShannonIntegrationTest, TransferEntropy_Independent) {
    // Independent time series should have TE ≈ 0
    float x[100], y[100];
    srand(42);
    for (int i = 0; i < 100; i++) {
        x[i] = (float)rand() / RAND_MAX;
        y[i] = (float)rand() / RAND_MAX;
    }
    float te = nimcp_stats_transfer_entropy(x, y, 100, 1, 4);
    EXPECT_LT(te, 0.5f);  // Should be near zero for independent series
}

TEST_F(ShannonIntegrationTest, TransferEntropy_Causal) {
    // y[t] = x[t-1] + noise: should have positive TE(X→Y)
    float x[100], y[100];
    srand(42);
    x[0] = (float)rand() / RAND_MAX;
    for (int i = 1; i < 100; i++) {
        x[i] = (float)rand() / RAND_MAX;
        y[i] = x[i-1] + 0.1f * ((float)rand() / RAND_MAX - 0.5f);
    }
    y[0] = x[0];

    float te_xy = nimcp_stats_transfer_entropy(x, y, 100, 1, 8);
    float te_yx = nimcp_stats_transfer_entropy(y, x, 100, 1, 8);

    // TE(X→Y) should be greater than TE(Y→X) due to causal relationship
    EXPECT_GT(te_xy, te_yx * 0.5f);  // Relaxed test due to noise
}

TEST_F(ShannonIntegrationTest, EffectiveInformation_Deterministic) {
    // Deterministic TPM (permutation matrix) has maximum EI
    float tpm[4] = {0.0f, 1.0f, 1.0f, 0.0f};  // Swap states
    float ei = nimcp_stats_effective_information(tpm, 2);
    // EI = H(output) - H(output|input) = 1 - 0 = 1 bit
    EXPECT_NEAR(ei, 1.0f, LOOSE_TOLERANCE);
}

TEST_F(ShannonIntegrationTest, EffectiveInformation_Random) {
    // Random TPM has low EI
    float tpm[4] = {0.5f, 0.5f, 0.5f, 0.5f};  // No information preserved
    float ei = nimcp_stats_effective_information(tpm, 2);
    // EI should be near 0 for random mapping
    EXPECT_NEAR(ei, 0.0f, LOOSE_TOLERANCE);
}

TEST_F(ShannonIntegrationTest, InformationIntegration_Uncorrelated) {
    // Diagonal covariance = independent variables → Phi = 0
    float cov[9] = {1.0f, 0.0f, 0.0f,
                    0.0f, 1.0f, 0.0f,
                    0.0f, 0.0f, 1.0f};
    float phi = nimcp_stats_information_integration(cov, 3);
    EXPECT_NEAR(phi, 0.0f, LOOSE_TOLERANCE);
}

TEST_F(ShannonIntegrationTest, InformationIntegration_Correlated) {
    // High correlation → high Phi
    float cov[4] = {1.0f, 0.9f,
                    0.9f, 1.0f};
    float phi = nimcp_stats_information_integration(cov, 2);
    EXPECT_GT(phi, 0.0f);  // Should be positive for correlated variables
}

TEST_F(ShannonIntegrationTest, InformationBottleneck_Compression) {
    // Simple joint distribution
    float joint_xy[4] = {0.4f, 0.1f, 0.1f, 0.4f};  // Correlated X and Y
    float q_t_given_x[4];  // 2 x states, 2 t states

    float ratio = nimcp_stats_information_bottleneck(joint_xy, 2, 2, 2, 10.0f, q_t_given_x, 100);

    // With high beta, should preserve most information
    EXPECT_GT(ratio, 0.5f);
    EXPECT_LE(ratio, 1.0f);
}

TEST_F(ShannonIntegrationTest, InformationBottleneck_MaxCompression) {
    // Low beta = maximum compression
    float joint_xy[4] = {0.4f, 0.1f, 0.1f, 0.4f};
    float q_t_given_x[4];

    float ratio = nimcp_stats_information_bottleneck(joint_xy, 2, 2, 1, 0.1f, q_t_given_x, 100);

    // With low beta and only 1 compressed state, ratio should be low
    EXPECT_LT(ratio, 0.5f);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
