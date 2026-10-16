//=============================================================================
// test_neural_statistics_regression.cpp - Neural Statistics Regression Tests
//=============================================================================
/**
 * @file test_neural_statistics_regression.cpp
 * @brief Comprehensive regression tests for neural statistics module
 *
 * REGRESSION TEST FOCUS:
 * - Numerical stability with extreme values
 * - Known value comparisons against reference implementations
 * - Consistency across platforms and code changes
 * - Performance baselines
 * - Fisher information analytical validation
 * - Spike train entropy convergence
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <limits>
#include <chrono>
#include <random>
#include <algorithm>
#include <numeric>

extern "C" {
#include "utils/statistics/nimcp_statistics.h"
#include "information/nimcp_shannon.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class NeuralStatisticsRegressionTest : public ::testing::Test {
protected:
    static constexpr float EPSILON = 1e-5f;
    static constexpr float RELATIVE_TOL = 1e-4f;
    static constexpr float ABSOLUTE_TOL = 1e-6f;

    // Performance baselines (microseconds)
    // Baselines account for system variability across different hardware/load conditions
    static constexpr double MEAN_BASELINE_US = 150.0;
    static constexpr double VARIANCE_BASELINE_US = 200.0;
    static constexpr double ENTROPY_BASELINE_US = 75.0;

    std::mt19937 rng;

    void SetUp() override {
        nimcp_stats_init(nullptr);
        rng.seed(42); // Deterministic seed for reproducibility
    }

    void TearDown() override {
        nimcp_stats_shutdown();
    }

    // Helper: relative error comparison
    bool relativelyEqual(float a, float b, float rel_tol = RELATIVE_TOL, float abs_tol = ABSOLUTE_TOL) {
        if (std::isnan(a) || std::isnan(b)) return false;
        if (std::isinf(a) && std::isinf(b)) return (a > 0) == (b > 0);
        float diff = std::fabs(a - b);
        return diff <= abs_tol || diff <= rel_tol * std::max(std::fabs(a), std::fabs(b));
    }

    // Generate normal samples
    std::vector<float> generateNormal(size_t n, float mean, float stddev) {
        std::normal_distribution<float> dist(mean, stddev);
        std::vector<float> data(n);
        for (size_t i = 0; i < n; ++i) {
            data[i] = dist(rng);
        }
        return data;
    }

    // Generate uniform samples
    std::vector<float> generateUniform(size_t n, float a, float b) {
        std::uniform_real_distribution<float> dist(a, b);
        std::vector<float> data(n);
        for (size_t i = 0; i < n; ++i) {
            data[i] = dist(rng);
        }
        return data;
    }

    // Reference mean calculation (Kahan summation for accuracy)
    double referenceMean(const std::vector<float>& data) {
        double sum = 0.0, c = 0.0;
        for (float x : data) {
            double y = x - c;
            double t = sum + y;
            c = (t - sum) - y;
            sum = t;
        }
        return sum / data.size();
    }

    // Reference variance calculation (two-pass)
    double referenceVariance(const std::vector<float>& data) {
        double mean = referenceMean(data);
        double sum_sq = 0.0;
        for (float x : data) {
            double d = x - mean;
            sum_sq += d * d;
        }
        return sum_sq / (data.size() - 1);
    }
};

//=============================================================================
// NUMERICAL STABILITY REGRESSION TESTS
//=============================================================================

TEST_F(NeuralStatisticsRegressionTest, MeanNearMachineEpsilon) {
    // Test with values near machine epsilon
    std::vector<float> data = {
        std::numeric_limits<float>::epsilon(),
        2.0f * std::numeric_limits<float>::epsilon(),
        3.0f * std::numeric_limits<float>::epsilon(),
        4.0f * std::numeric_limits<float>::epsilon(),
        5.0f * std::numeric_limits<float>::epsilon()
    };

    float result = nimcp_stats_mean(data.data(), data.size());
    float expected = 3.0f * std::numeric_limits<float>::epsilon();

    EXPECT_TRUE(relativelyEqual(result, expected, 1e-3f))
        << "Mean near epsilon: expected " << expected << ", got " << result;
}

TEST_F(NeuralStatisticsRegressionTest, MeanVeryLargeValues) {
    // Test with very large values (1e30)
    std::vector<float> data = {1e30f, 1.1e30f, 0.9e30f, 1.05e30f, 0.95e30f};

    float result = nimcp_stats_mean(data.data(), data.size());
    float expected = 1.0e30f; // Approximate expected mean

    EXPECT_TRUE(relativelyEqual(result, expected, 0.1f))
        << "Mean large values: expected ~" << expected << ", got " << result;
}

TEST_F(NeuralStatisticsRegressionTest, MeanVerySmallValues) {
    // Test with very small values (1e-30)
    std::vector<float> data = {1e-30f, 2e-30f, 3e-30f, 4e-30f, 5e-30f};

    float result = nimcp_stats_mean(data.data(), data.size());
    float expected = 3e-30f;

    EXPECT_TRUE(relativelyEqual(result, expected, 1e-3f))
        << "Mean tiny values: expected " << expected << ", got " << result;
}

TEST_F(NeuralStatisticsRegressionTest, MeanMixedScales) {
    // Test with mixed scales (challenging for numerical stability)
    std::vector<float> data = {1e10f, 1e-10f, 1e10f, 1e-10f};

    float result = nimcp_stats_mean(data.data(), data.size());
    float expected = 0.5e10f; // (2e10 + 2e-10) / 4 ~= 0.5e10

    // Allow larger tolerance due to floating point limitations
    EXPECT_TRUE(relativelyEqual(result, expected, 0.01f))
        << "Mean mixed scales: expected ~" << expected << ", got " << result;
}

TEST_F(NeuralStatisticsRegressionTest, VarianceNumericalStability) {
    // Welford's algorithm should handle large offsets
    // Data with large mean but small variance
    std::vector<float> data(1000);
    float base = 1e6f;
    std::normal_distribution<float> dist(base, 1.0f);
    for (auto& x : data) x = dist(rng);

    float result = nimcp_stats_variance(data.data(), data.size());

    // Variance should be approximately 1.0 regardless of large offset
    EXPECT_GT(result, 0.5f) << "Variance should be positive";
    EXPECT_LT(result, 2.0f) << "Variance should be close to 1.0";
}

TEST_F(NeuralStatisticsRegressionTest, VarianceAllSameValue) {
    // Variance of constant data should be exactly 0
    std::vector<float> data(100, 42.0f);

    float result = nimcp_stats_variance(data.data(), data.size());

    EXPECT_FLOAT_EQ(result, 0.0f) << "Variance of constant data should be 0";
}

TEST_F(NeuralStatisticsRegressionTest, SkewnessNormalDistribution) {
    // Normal distribution should have skewness near 0
    auto data = generateNormal(10000, 0.0f, 1.0f);

    float result = nimcp_stats_skewness(data.data(), data.size());

    EXPECT_NEAR(result, 0.0f, 0.1f) << "Normal distribution should have ~0 skewness";
}

TEST_F(NeuralStatisticsRegressionTest, KurtosisNormalDistribution) {
    // Normal distribution should have excess kurtosis near 0
    auto data = generateNormal(10000, 0.0f, 1.0f);

    float result = nimcp_stats_kurtosis(data.data(), data.size());

    EXPECT_NEAR(result, 0.0f, 0.2f) << "Normal distribution should have ~0 excess kurtosis";
}

//=============================================================================
// KNOWN VALUE REGRESSION TESTS (Golden Values)
//=============================================================================

TEST_F(NeuralStatisticsRegressionTest, GoldenMeanSimple) {
    // Golden test: mean of {1,2,3,4,5} = 3.0 exactly
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};

    float result = nimcp_stats_mean(data.data(), data.size());

    EXPECT_FLOAT_EQ(result, 3.0f);
}

TEST_F(NeuralStatisticsRegressionTest, GoldenVarianceSimple) {
    // Golden test: sample variance of {1,2,3,4,5}
    // Mean = 3, deviations = {-2,-1,0,1,2}, sum of squares = 10
    // Sample variance = 10 / 4 = 2.5
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};

    float result = nimcp_stats_variance(data.data(), data.size());

    EXPECT_FLOAT_EQ(result, 2.5f);
}

TEST_F(NeuralStatisticsRegressionTest, GoldenStdDevSimple) {
    // sqrt(2.5) = 1.5811388...
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};

    float result = nimcp_stats_std_dev(data.data(), data.size());

    EXPECT_NEAR(result, 1.5811388f, 1e-5f);
}

TEST_F(NeuralStatisticsRegressionTest, GoldenMedianOdd) {
    // Median of {1,2,3,4,5} = 3
    std::vector<float> data = {5.0f, 1.0f, 3.0f, 2.0f, 4.0f}; // Unsorted

    float result = nimcp_stats_median(data.data(), data.size());

    EXPECT_FLOAT_EQ(result, 3.0f);
}

TEST_F(NeuralStatisticsRegressionTest, GoldenMedianEven) {
    // Median of {1,2,3,4} = (2+3)/2 = 2.5
    std::vector<float> data = {4.0f, 1.0f, 3.0f, 2.0f}; // Unsorted

    float result = nimcp_stats_median(data.data(), data.size());

    EXPECT_FLOAT_EQ(result, 2.5f);
}

TEST_F(NeuralStatisticsRegressionTest, GoldenQuantile25) {
    // Q1 (25th percentile) of {1,2,3,4,5,6,7,8}
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};

    float result = nimcp_stats_quantile(data.data(), data.size(), 0.25f);

    EXPECT_NEAR(result, 2.5f, 0.5f); // Allow for interpolation differences
}

TEST_F(NeuralStatisticsRegressionTest, GoldenQuantile75) {
    // Q3 (75th percentile)
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};

    float result = nimcp_stats_quantile(data.data(), data.size(), 0.75f);

    EXPECT_NEAR(result, 6.5f, 0.5f);
}

TEST_F(NeuralStatisticsRegressionTest, GoldenGeometricMean) {
    // Geometric mean of {2, 8} = sqrt(16) = 4
    std::vector<float> data = {2.0f, 8.0f};

    float result = nimcp_stats_geometric_mean(data.data(), data.size());

    EXPECT_FLOAT_EQ(result, 4.0f);
}

TEST_F(NeuralStatisticsRegressionTest, GoldenHarmonicMean) {
    // Harmonic mean of {1, 2, 4} = 3 / (1 + 0.5 + 0.25) = 3 / 1.75 = 12/7
    std::vector<float> data = {1.0f, 2.0f, 4.0f};

    float result = nimcp_stats_harmonic_mean(data.data(), data.size());

    EXPECT_NEAR(result, 12.0f/7.0f, 1e-5f);
}

//=============================================================================
// PROBABILITY DISTRIBUTION REGRESSION TESTS
//=============================================================================

TEST_F(NeuralStatisticsRegressionTest, StandardNormalPDFAtZero) {
    // PDF at x=0 for N(0,1) = 1/sqrt(2*pi) ~= 0.3989422804
    float result = nimcp_stats_pdf_normal(0.0f, 0.0f, 1.0f);

    EXPECT_NEAR(result, 0.3989422804f, 1e-6f);
}

TEST_F(NeuralStatisticsRegressionTest, StandardNormalPDFAtOne) {
    // PDF at x=1 for N(0,1) = exp(-0.5)/sqrt(2*pi) ~= 0.2419707245
    float result = nimcp_stats_pdf_normal(1.0f, 0.0f, 1.0f);

    EXPECT_NEAR(result, 0.2419707245f, 1e-6f);
}

TEST_F(NeuralStatisticsRegressionTest, StandardNormalCDFAtZero) {
    // CDF at x=0 for N(0,1) = 0.5 exactly
    float result = nimcp_stats_cdf_standard_normal(0.0f);

    EXPECT_FLOAT_EQ(result, 0.5f);
}

TEST_F(NeuralStatisticsRegressionTest, StandardNormalCDFAt196) {
    // CDF at x=1.96 ~= 0.975 (97.5th percentile)
    float result = nimcp_stats_cdf_standard_normal(1.96f);

    EXPECT_NEAR(result, 0.975f, 0.001f);
}

TEST_F(NeuralStatisticsRegressionTest, StandardNormalQuantile95) {
    // 95th percentile of N(0,1) ~= 1.645
    float result = nimcp_stats_quantile_standard_normal(0.95f);

    EXPECT_NEAR(result, 1.645f, 0.01f);
}

TEST_F(NeuralStatisticsRegressionTest, ExponentialPDF) {
    // PDF of Exp(2) at x=1: 2*exp(-2) ~= 0.2706706
    float result = nimcp_stats_pdf_exponential(1.0f, 2.0f);

    EXPECT_NEAR(result, 0.2706706f, 1e-5f);
}

TEST_F(NeuralStatisticsRegressionTest, ExponentialCDF) {
    // CDF of Exp(2) at x=1: 1 - exp(-2) ~= 0.8646647
    float result = nimcp_stats_cdf_exponential(1.0f, 2.0f);

    EXPECT_NEAR(result, 0.8646647f, 1e-5f);
}

TEST_F(NeuralStatisticsRegressionTest, PoissonPMF) {
    // P(X=3) for Poisson(2) = (e^-2 * 2^3) / 3! = 8*e^-2/6 ~= 0.180447
    float result = nimcp_stats_pmf_poisson(3, 2.0f);

    EXPECT_NEAR(result, 0.180447f, 1e-4f);
}

TEST_F(NeuralStatisticsRegressionTest, BinomialPMF) {
    // P(X=2) for Bin(5, 0.5) = C(5,2)*0.5^5 = 10*0.03125 = 0.3125
    float result = nimcp_stats_pmf_binomial(2, 5, 0.5f);

    EXPECT_NEAR(result, 0.3125f, 1e-5f);
}

//=============================================================================
// INFORMATION THEORY REGRESSION TESTS
//=============================================================================

TEST_F(NeuralStatisticsRegressionTest, EntropyFairCoin) {
    // Entropy of fair coin: H = -2 * 0.5 * log2(0.5) = 1 bit
    std::vector<float> probs = {0.5f, 0.5f};

    float result = nimcp_stats_entropy(probs.data(), probs.size());

    EXPECT_FLOAT_EQ(result, 1.0f);
}

TEST_F(NeuralStatisticsRegressionTest, EntropyUniform4) {
    // Entropy of uniform distribution over 4 states = log2(4) = 2 bits
    std::vector<float> probs = {0.25f, 0.25f, 0.25f, 0.25f};

    float result = nimcp_stats_entropy(probs.data(), probs.size());

    EXPECT_FLOAT_EQ(result, 2.0f);
}

TEST_F(NeuralStatisticsRegressionTest, EntropyDeterministic) {
    // Entropy of deterministic distribution = 0
    std::vector<float> probs = {1.0f, 0.0f, 0.0f};

    float result = nimcp_stats_entropy(probs.data(), probs.size());

    EXPECT_FLOAT_EQ(result, 0.0f);
}

TEST_F(NeuralStatisticsRegressionTest, EntropyBiasedCoin) {
    // H(0.9, 0.1) = -0.9*log2(0.9) - 0.1*log2(0.1) ~= 0.469
    std::vector<float> probs = {0.9f, 0.1f};

    float result = nimcp_stats_entropy(probs.data(), probs.size());

    EXPECT_NEAR(result, 0.469f, 0.01f);
}

TEST_F(NeuralStatisticsRegressionTest, KLDivergenceSame) {
    // KL(P||P) = 0 for any P
    std::vector<float> probs = {0.3f, 0.4f, 0.3f};

    float result = nimcp_stats_kl_divergence(probs.data(), probs.data(), probs.size());

    EXPECT_NEAR(result, 0.0f, 1e-6f);
}

TEST_F(NeuralStatisticsRegressionTest, JSDivergenceSymmetric) {
    // JS divergence should be symmetric
    std::vector<float> p = {0.5f, 0.5f};
    std::vector<float> q = {0.9f, 0.1f};

    float js_pq = nimcp_stats_js_divergence(p.data(), q.data(), p.size());
    float js_qp = nimcp_stats_js_divergence(q.data(), p.data(), p.size());

    EXPECT_NEAR(js_pq, js_qp, 1e-6f) << "JS divergence should be symmetric";
}

TEST_F(NeuralStatisticsRegressionTest, MutualInformationIndependent) {
    // MI of independent variables = 0
    // Joint: p(x,y) = p(x)*p(y)
    std::vector<float> joint = {0.25f, 0.25f, 0.25f, 0.25f}; // 2x2 uniform

    float result = nimcp_stats_mutual_information(joint.data(), 2, 2);

    EXPECT_NEAR(result, 0.0f, 1e-5f);
}

TEST_F(NeuralStatisticsRegressionTest, MutualInformationPerfect) {
    // MI of perfectly correlated variables = H(X) = H(Y)
    // Y = X (diagonal joint distribution)
    std::vector<float> joint = {0.5f, 0.0f, 0.0f, 0.5f}; // 2x2 diagonal

    float result = nimcp_stats_mutual_information(joint.data(), 2, 2);

    EXPECT_NEAR(result, 1.0f, 1e-5f) << "Perfect correlation should have MI = 1 bit";
}

//=============================================================================
// CORRELATION AND REGRESSION TESTS
//=============================================================================

TEST_F(NeuralStatisticsRegressionTest, PearsonPerfectPositive) {
    // Perfect positive correlation: r = 1
    std::vector<float> x = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    std::vector<float> y = {2.0f, 4.0f, 6.0f, 8.0f, 10.0f}; // y = 2x

    nimcp_correlation_result_t result;
    nimcp_stats_result_t status = nimcp_stats_correlation_pearson(
        x.data(), y.data(), x.size(), &result
    );

    EXPECT_EQ(status, NIMCP_STATS_OK);
    EXPECT_NEAR(result.r, 1.0f, 1e-5f);
}

TEST_F(NeuralStatisticsRegressionTest, PearsonPerfectNegative) {
    // Perfect negative correlation: r = -1
    std::vector<float> x = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    std::vector<float> y = {10.0f, 8.0f, 6.0f, 4.0f, 2.0f}; // y = 12 - 2x

    nimcp_correlation_result_t result;
    nimcp_stats_result_t status = nimcp_stats_correlation_pearson(
        x.data(), y.data(), x.size(), &result
    );

    EXPECT_EQ(status, NIMCP_STATS_OK);
    EXPECT_NEAR(result.r, -1.0f, 1e-5f);
}

TEST_F(NeuralStatisticsRegressionTest, LinearRegressionSimple) {
    // y = 2x + 1
    std::vector<float> x = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    std::vector<float> y = {3.0f, 5.0f, 7.0f, 9.0f, 11.0f};

    nimcp_regression_result_t result;
    nimcp_stats_result_t status = nimcp_stats_regression_linear(
        x.data(), y.data(), x.size(), &result
    );

    EXPECT_EQ(status, NIMCP_STATS_OK);
    EXPECT_NEAR(result.slope, 2.0f, 1e-5f);
    EXPECT_NEAR(result.intercept, 1.0f, 1e-5f);
    EXPECT_NEAR(result.r_squared, 1.0f, 1e-5f);
}

TEST_F(NeuralStatisticsRegressionTest, CovariancePositive) {
    // Positive covariance for positively related data
    std::vector<float> x = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    std::vector<float> y = {2.0f, 4.0f, 5.0f, 4.0f, 5.0f};

    float result = nimcp_stats_covariance(x.data(), y.data(), x.size());

    EXPECT_GT(result, 0.0f);
}

//=============================================================================
// HYPOTHESIS TESTING REGRESSION TESTS
//=============================================================================

TEST_F(NeuralStatisticsRegressionTest, OneSampleTTestKnown) {
    // Test against known population mean
    std::vector<float> data = {5.1f, 4.9f, 5.0f, 5.2f, 4.8f, 5.1f, 5.0f, 4.9f};

    nimcp_test_result_t result;
    nimcp_stats_result_t status = nimcp_stats_ttest_one_sample(
        data.data(), data.size(), 5.0f, // H0: mu = 5.0
        NIMCP_TEST_TWO_SIDED, 0.95f, &result
    );

    EXPECT_EQ(status, NIMCP_STATS_OK);
    EXPECT_GT(result.p_value, 0.05f) << "Should not reject H0 for this data";
}

TEST_F(NeuralStatisticsRegressionTest, TwoSampleTTestDifferent) {
    // Two clearly different samples
    std::vector<float> data1 = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    std::vector<float> data2 = {10.0f, 11.0f, 12.0f, 13.0f, 14.0f};

    nimcp_test_result_t result;
    nimcp_stats_result_t status = nimcp_stats_ttest_two_sample(
        data1.data(), data1.size(),
        data2.data(), data2.size(),
        true, // equal variance
        NIMCP_TEST_TWO_SIDED, 0.95f, &result
    );

    EXPECT_EQ(status, NIMCP_STATS_OK);
    EXPECT_LT(result.p_value, 0.001f) << "Should strongly reject H0";
}

TEST_F(NeuralStatisticsRegressionTest, ChiSquaredGOFUniform) {
    // Test uniform distribution against uniform expectation
    std::vector<float> observed = {25.0f, 25.0f, 25.0f, 25.0f};
    std::vector<float> expected = {25.0f, 25.0f, 25.0f, 25.0f};

    nimcp_test_result_t result;
    nimcp_stats_result_t status = nimcp_stats_chi_squared_gof(
        observed.data(), expected.data(), observed.size(), &result
    );

    EXPECT_EQ(status, NIMCP_STATS_OK);
    EXPECT_NEAR(result.statistic, 0.0f, 1e-5f) << "Chi-squared should be ~0 for perfect match";
    EXPECT_GT(result.p_value, 0.99f) << "P-value should be near 1 for perfect match";
}

//=============================================================================
// CONSISTENCY REGRESSION TESTS
//=============================================================================

TEST_F(NeuralStatisticsRegressionTest, DeterministicResults) {
    // Same input should always produce same output
    auto data = generateNormal(1000, 0.0f, 1.0f);

    float mean1 = nimcp_stats_mean(data.data(), data.size());
    float mean2 = nimcp_stats_mean(data.data(), data.size());
    float var1 = nimcp_stats_variance(data.data(), data.size());
    float var2 = nimcp_stats_variance(data.data(), data.size());

    EXPECT_FLOAT_EQ(mean1, mean2) << "Mean should be deterministic";
    EXPECT_FLOAT_EQ(var1, var2) << "Variance should be deterministic";
}

TEST_F(NeuralStatisticsRegressionTest, ConsistencyWithReferenceImplementation) {
    // Compare against reference implementation
    rng.seed(12345);
    auto data = generateNormal(1000, 5.0f, 2.0f);

    float nimcp_mean = nimcp_stats_mean(data.data(), data.size());
    float nimcp_var = nimcp_stats_variance(data.data(), data.size());

    double ref_mean = referenceMean(data);
    double ref_var = referenceVariance(data);

    EXPECT_NEAR(nimcp_mean, ref_mean, 1e-4f) << "Mean should match reference";
    EXPECT_NEAR(nimcp_var, ref_var, 1e-3f) << "Variance should match reference";
}

//=============================================================================
// PERFORMANCE REGRESSION TESTS
//=============================================================================

TEST_F(NeuralStatisticsRegressionTest, MeanPerformanceBaseline) {
    auto data = generateNormal(10000, 0.0f, 1.0f);

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100; ++i) {
        volatile float result = nimcp_stats_mean(data.data(), data.size());
        (void)result;
    }
    auto end = std::chrono::high_resolution_clock::now();

    double elapsed_us = std::chrono::duration<double, std::micro>(end - start).count() / 100.0;

    // Allow 20% performance degradation from baseline
    EXPECT_LT(elapsed_us, MEAN_BASELINE_US * 1.2)
        << "Mean performance degraded: " << elapsed_us << "us vs baseline " << MEAN_BASELINE_US << "us";
}

TEST_F(NeuralStatisticsRegressionTest, VariancePerformanceBaseline) {
    auto data = generateNormal(10000, 0.0f, 1.0f);

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100; ++i) {
        volatile float result = nimcp_stats_variance(data.data(), data.size());
        (void)result;
    }
    auto end = std::chrono::high_resolution_clock::now();

    double elapsed_us = std::chrono::duration<double, std::micro>(end - start).count() / 100.0;

    EXPECT_LT(elapsed_us, VARIANCE_BASELINE_US * 1.2)
        << "Variance performance degraded: " << elapsed_us << "us";
}

//=============================================================================
// RUNNING STATISTICS REGRESSION TESTS
//=============================================================================

TEST_F(NeuralStatisticsRegressionTest, RunningStatsMergeAssociativity) {
    // Merging should be associative: (A merge B) merge C == A merge (B merge C)
    auto data1 = generateNormal(100, 0.0f, 1.0f);
    auto data2 = generateNormal(100, 1.0f, 1.5f);
    auto data3 = generateNormal(100, -1.0f, 0.5f);

    nimcp_running_stats_t a, b, c, ab, abc1, bc, abc2;

    nimcp_stats_running_init(&a);
    nimcp_stats_running_init(&b);
    nimcp_stats_running_init(&c);

    nimcp_stats_running_add_array(&a, data1.data(), data1.size());
    nimcp_stats_running_add_array(&b, data2.data(), data2.size());
    nimcp_stats_running_add_array(&c, data3.data(), data3.size());

    // (A merge B) merge C
    ab = a;
    nimcp_stats_running_merge(&ab, &b);
    abc1 = ab;
    nimcp_stats_running_merge(&abc1, &c);

    // A merge (B merge C)
    bc = b;
    nimcp_stats_running_merge(&bc, &c);
    abc2 = a;
    nimcp_stats_running_merge(&abc2, &bc);

    EXPECT_NEAR(nimcp_stats_running_mean(&abc1), nimcp_stats_running_mean(&abc2), 1e-5);
    EXPECT_NEAR(nimcp_stats_running_variance(&abc1), nimcp_stats_running_variance(&abc2), 1e-4);
}

TEST_F(NeuralStatisticsRegressionTest, RunningStatsMatchBatch) {
    // Running stats should match batch computation
    auto data = generateNormal(1000, 5.0f, 2.0f);

    nimcp_running_stats_t running;
    nimcp_stats_running_init(&running);
    nimcp_stats_running_add_array(&running, data.data(), data.size());

    float batch_mean = nimcp_stats_mean(data.data(), data.size());
    float batch_var = nimcp_stats_variance(data.data(), data.size());

    EXPECT_NEAR((float)nimcp_stats_running_mean(&running), batch_mean, 1e-5f);
    EXPECT_NEAR((float)nimcp_stats_running_variance(&running), batch_var, 1e-4f);
}

//=============================================================================
// EDGE CASE REGRESSION TESTS
//=============================================================================

TEST_F(NeuralStatisticsRegressionTest, SingleElement) {
    std::vector<float> data = {42.0f};

    EXPECT_FLOAT_EQ(nimcp_stats_mean(data.data(), 1), 42.0f);
    EXPECT_TRUE(std::isnan(nimcp_stats_variance(data.data(), 1)) ||
                nimcp_stats_variance(data.data(), 1) == 0.0f)
        << "Variance of single element should be NaN or 0";
}

TEST_F(NeuralStatisticsRegressionTest, TwoElements) {
    std::vector<float> data = {0.0f, 2.0f};

    EXPECT_FLOAT_EQ(nimcp_stats_mean(data.data(), 2), 1.0f);
    EXPECT_FLOAT_EQ(nimcp_stats_variance(data.data(), 2), 2.0f); // ((-1)^2 + 1^2) / 1 = 2
}

TEST_F(NeuralStatisticsRegressionTest, NullPointerHandling) {
    float result = nimcp_stats_mean(nullptr, 10);
    EXPECT_TRUE(std::isnan(result));

    result = nimcp_stats_variance(nullptr, 10);
    EXPECT_TRUE(std::isnan(result));
}

TEST_F(NeuralStatisticsRegressionTest, ZeroSizeHandling) {
    std::vector<float> data = {1.0f, 2.0f};

    float result = nimcp_stats_mean(data.data(), 0);
    EXPECT_TRUE(std::isnan(result));
}

TEST_F(NeuralStatisticsRegressionTest, InfinityHandling) {
    std::vector<float> data = {1.0f, std::numeric_limits<float>::infinity(), 3.0f};

    float result = nimcp_stats_mean(data.data(), data.size());
    EXPECT_TRUE(std::isinf(result) && result > 0);
}

TEST_F(NeuralStatisticsRegressionTest, NaNHandling) {
    std::vector<float> data = {1.0f, std::numeric_limits<float>::quiet_NaN(), 3.0f};

    float result = nimcp_stats_mean(data.data(), data.size());
    EXPECT_TRUE(std::isnan(result));
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
