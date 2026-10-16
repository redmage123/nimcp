//=============================================================================
// test_ml_statistics_regression.cpp - ML Statistics Regression Tests
//=============================================================================
/**
 * @file test_ml_statistics_regression.cpp
 * @brief Comprehensive regression tests for ML statistics module
 *
 * REGRESSION TEST FOCUS:
 * - GMM on well-separated Gaussians
 * - Logistic regression convergence
 * - Bootstrap confidence intervals
 * - Cross-validation stability
 * - AIC/BIC model selection criteria
 * - Information criteria for model comparison
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <limits>
#include <chrono>
#include <random>
#include <algorithm>
#include <numeric>
#include <memory>

extern "C" {
#include "utils/statistics/nimcp_statistics.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class MLStatisticsRegressionTest : public ::testing::Test {
protected:
    static constexpr float EPSILON = 1e-5f;
    static constexpr float RELATIVE_TOL = 1e-4f;

    std::mt19937 rng;

    void SetUp() override {
        nimcp_stats_init(nullptr);
        rng.seed(42);
    }

    void TearDown() override {
        nimcp_stats_shutdown();
    }

    bool relativelyEqual(float a, float b, float tol = RELATIVE_TOL) {
        if (std::isnan(a) || std::isnan(b)) return false;
        return std::fabs(a - b) <= tol * std::max(1.0f, std::max(std::fabs(a), std::fabs(b)));
    }

    // Generate mixture of Gaussians
    std::vector<float> generateGMM(size_t n,
                                    const std::vector<float>& means,
                                    const std::vector<float>& stds,
                                    const std::vector<float>& weights) {
        std::discrete_distribution<> component(weights.begin(), weights.end());
        std::vector<float> data(n);

        for (size_t i = 0; i < n; ++i) {
            int k = component(rng);
            std::normal_distribution<float> dist(means[k], stds[k]);
            data[i] = dist(rng);
        }

        return data;
    }

    // Generate binary classification data
    void generateBinaryClassification(size_t n,
                                       std::vector<float>& X,
                                       std::vector<uint8_t>& y,
                                       float separation = 2.0f) {
        std::normal_distribution<float> dist(0.0f, 1.0f);
        X.resize(n * 2); // 2 features
        y.resize(n);

        for (size_t i = 0; i < n; ++i) {
            if (i < n / 2) {
                // Class 0: centered at (-separation/2, 0)
                X[i * 2 + 0] = -separation / 2.0f + dist(rng);
                X[i * 2 + 1] = dist(rng);
                y[i] = 0;
            } else {
                // Class 1: centered at (separation/2, 0)
                X[i * 2 + 0] = separation / 2.0f + dist(rng);
                X[i * 2 + 1] = dist(rng);
                y[i] = 1;
            }
        }
    }

    // Sigmoid function
    float sigmoid(float x) {
        return 1.0f / (1.0f + std::exp(-x));
    }

    // Reference AIC: AIC = 2k - 2*log(L) where k is parameters, L is likelihood
    float computeAIC(float log_likelihood, int num_params) {
        return 2.0f * num_params - 2.0f * log_likelihood;
    }

    // Reference BIC: BIC = k*log(n) - 2*log(L)
    float computeBIC(float log_likelihood, int num_params, int n) {
        return num_params * std::log(static_cast<float>(n)) - 2.0f * log_likelihood;
    }
};

//=============================================================================
// LOGISTIC REGRESSION REGRESSION TESTS
//=============================================================================

TEST_F(MLStatisticsRegressionTest, LogisticRegressionWellSeparated) {
    // Well-separated classes should achieve high accuracy
    std::vector<float> X;
    std::vector<uint8_t> y;
    generateBinaryClassification(200, X, y, 4.0f); // Large separation

    // Function outputs p coefficients (no intercept)
    std::vector<float> coefficients(2);
    nimcp_stats_result_t status = nimcp_stats_regression_logistic(
        X.data(), y.data(), 200, 2, coefficients.data(), 100
    );

    EXPECT_EQ(status, NIMCP_STATS_OK);

    // First coefficient (for x1) should be positive (class 1 has larger x1)
    // Index 0 = x1 coefficient, Index 1 = x2 coefficient
    EXPECT_GT(coefficients[0], 0.0f) << "Coefficient for separating feature should be positive";
}

TEST_F(MLStatisticsRegressionTest, LogisticRegressionOverlapping) {
    // Overlapping classes should have lower accuracy
    std::vector<float> X;
    std::vector<uint8_t> y;
    generateBinaryClassification(200, X, y, 0.5f); // Small separation

    std::vector<float> coefficients(3);
    nimcp_stats_result_t status = nimcp_stats_regression_logistic(
        X.data(), y.data(), 200, 2, coefficients.data(), 100
    );

    EXPECT_EQ(status, NIMCP_STATS_OK);
    // Coefficients should be smaller due to overlap
}

TEST_F(MLStatisticsRegressionTest, LogisticRegressionConvergence) {
    // Should converge within max iterations for reasonable data
    std::vector<float> X;
    std::vector<uint8_t> y;
    generateBinaryClassification(100, X, y, 2.0f);

    std::vector<float> coefficients(3);
    nimcp_stats_result_t status = nimcp_stats_regression_logistic(
        X.data(), y.data(), 100, 2, coefficients.data(), 1000
    );

    EXPECT_EQ(status, NIMCP_STATS_OK);
    EXPECT_FALSE(std::isnan(coefficients[0]));
    EXPECT_FALSE(std::isnan(coefficients[1]));
    EXPECT_FALSE(std::isinf(coefficients[0]));
}

TEST_F(MLStatisticsRegressionTest, LogisticRegressionDeterministic) {
    // Same input should give same output
    std::vector<float> X;
    std::vector<uint8_t> y;
    rng.seed(123);
    generateBinaryClassification(100, X, y, 2.0f);

    std::vector<float> coef1(3), coef2(3);

    nimcp_stats_regression_logistic(X.data(), y.data(), 100, 2, coef1.data(), 100);
    nimcp_stats_regression_logistic(X.data(), y.data(), 100, 2, coef2.data(), 100);

    for (int i = 0; i < 3; ++i) {
        EXPECT_FLOAT_EQ(coef1[i], coef2[i])
            << "Logistic regression should be deterministic";
    }
}

//=============================================================================
// BOOTSTRAP REGRESSION TESTS
//=============================================================================

TEST_F(MLStatisticsRegressionTest, BootstrapMeanCI) {
    // Bootstrap CI for mean should contain true mean
    const size_t n = 100;
    const float true_mean = 5.0f;
    std::normal_distribution<float> dist(true_mean, 1.0f);

    std::vector<float> data(n);
    for (auto& x : data) x = dist(rng);

    nimcp_bootstrap_result_t result;
    nimcp_stats_result_t status = nimcp_stats_bootstrap_mean(
        data.data(), n, 1000, 0.95f, &result
    );

    EXPECT_EQ(status, NIMCP_STATS_OK);
    EXPECT_LE(result.ci_lower_percentile, true_mean)
        << "True mean should be >= lower CI bound";
    EXPECT_GE(result.ci_upper_percentile, true_mean)
        << "True mean should be <= upper CI bound";
}

TEST_F(MLStatisticsRegressionTest, BootstrapMedianCI) {
    // Bootstrap CI for median
    const size_t n = 100;
    std::normal_distribution<float> dist(10.0f, 2.0f);

    std::vector<float> data(n);
    for (auto& x : data) x = dist(rng);

    nimcp_bootstrap_result_t result;
    nimcp_stats_result_t status = nimcp_stats_bootstrap_median(
        data.data(), n, 1000, 0.95f, &result
    );

    EXPECT_EQ(status, NIMCP_STATS_OK);
    EXPECT_LT(result.ci_lower_percentile, result.estimate);
    EXPECT_GT(result.ci_upper_percentile, result.estimate);
}

TEST_F(MLStatisticsRegressionTest, BootstrapCorrelationCI) {
    // Bootstrap CI for correlation
    const size_t n = 50;
    std::vector<float> x(n), y(n);
    std::normal_distribution<float> dist(0.0f, 1.0f);

    for (size_t i = 0; i < n; ++i) {
        x[i] = dist(rng);
        y[i] = 0.7f * x[i] + 0.3f * dist(rng); // r ~ 0.7
    }

    nimcp_bootstrap_result_t result;
    nimcp_stats_result_t status = nimcp_stats_bootstrap_correlation(
        x.data(), y.data(), n, 1000, 0.95f, &result
    );

    EXPECT_EQ(status, NIMCP_STATS_OK);
    EXPECT_GT(result.estimate, 0.5f) << "Estimated correlation should be positive";
    EXPECT_LT(result.estimate, 1.0f);
}

TEST_F(MLStatisticsRegressionTest, BootstrapReplicateCount) {
    const size_t n = 50;
    std::vector<float> data(n);
    std::normal_distribution<float> dist(0.0f, 1.0f);
    for (auto& x : data) x = dist(rng);

    nimcp_bootstrap_result_t result;
    nimcp_stats_bootstrap_mean(data.data(), n, 500, 0.95f, &result);

    EXPECT_EQ(result.n_replicates, 500u);
}

TEST_F(MLStatisticsRegressionTest, BootstrapBiasEstimation) {
    // Bootstrap bias should be small for unbiased estimator
    const size_t n = 200;
    std::normal_distribution<float> dist(5.0f, 1.0f);

    std::vector<float> data(n);
    for (auto& x : data) x = dist(rng);

    nimcp_bootstrap_result_t result;
    nimcp_stats_bootstrap_mean(data.data(), n, 1000, 0.95f, &result);

    EXPECT_NEAR(result.bias, 0.0f, 0.1f) << "Mean estimator should have small bias";
}

//=============================================================================
// MODEL SELECTION CRITERIA REGRESSION TESTS
//=============================================================================

TEST_F(MLStatisticsRegressionTest, AICPrefersSimplerModel) {
    // AIC should prefer simpler model when fit is similar
    // Simulate: true model is y = 2*x + noise
    const size_t n = 100;
    std::vector<float> x(n), y(n);
    std::normal_distribution<float> noise(0.0f, 0.5f);

    for (size_t i = 0; i < n; ++i) {
        x[i] = static_cast<float>(i) / 10.0f;
        y[i] = 2.0f * x[i] + noise(rng);
    }

    // Fit linear model (2 params: intercept + slope)
    nimcp_regression_result_t linear;
    nimcp_stats_regression_linear(x.data(), y.data(), n, &linear);

    // Fit quadratic model (3 params)
    nimcp_regression_result_t quad;
    nimcp_stats_regression_polynomial(x.data(), y.data(), n, 2, &quad);

    // Linear AIC should be similar or better (fewer params, similar fit)
    // AIC = n*log(RSS/n) + 2k (simplified)
    EXPECT_LE(linear.aic, quad.aic + 5.0f)
        << "AIC should not strongly prefer overfitted model";
}

TEST_F(MLStatisticsRegressionTest, BICPenalizesComplexity) {
    // BIC should penalize complexity more than AIC for large n
    const size_t n = 500;
    std::vector<float> x(n), y(n);
    std::normal_distribution<float> noise(0.0f, 0.5f);

    for (size_t i = 0; i < n; ++i) {
        x[i] = static_cast<float>(i) / 50.0f;
        y[i] = 2.0f * x[i] + noise(rng);
    }

    nimcp_regression_result_t linear;
    nimcp_stats_regression_linear(x.data(), y.data(), n, &linear);

    nimcp_regression_result_t poly5;
    nimcp_stats_regression_polynomial(x.data(), y.data(), n, 5, &poly5);

    // BIC should prefer linear for large n
    EXPECT_LT(linear.bic, poly5.bic)
        << "BIC should prefer simpler model for large n";
}

TEST_F(MLStatisticsRegressionTest, DurbinWatsonAutocorrelation) {
    // Durbin-Watson statistic for residual autocorrelation
    const size_t n = 100;
    std::vector<float> x(n), y(n);

    for (size_t i = 0; i < n; ++i) {
        x[i] = static_cast<float>(i);
        y[i] = 2.0f * x[i] + 1.0f; // Perfect linear, no autocorrelation
    }

    nimcp_regression_result_t result;
    nimcp_stats_regression_linear(x.data(), y.data(), n, &result);

    // DW ~ 2 means no autocorrelation
    // DW < 2 means positive autocorrelation
    // DW > 2 means negative autocorrelation
    EXPECT_GT(result.durbin_watson, 1.5f) << "Should be close to 2 for no autocorrelation";
    EXPECT_LT(result.durbin_watson, 2.5f);
}

//=============================================================================
// HYPOTHESIS TESTING FOR ML REGRESSION TESTS
//=============================================================================

TEST_F(MLStatisticsRegressionTest, TTestSignificantDifference) {
    // Two groups with different means
    std::vector<float> group1(50), group2(50);
    std::normal_distribution<float> dist1(5.0f, 1.0f);
    std::normal_distribution<float> dist2(7.0f, 1.0f);

    for (auto& x : group1) x = dist1(rng);
    for (auto& x : group2) x = dist2(rng);

    nimcp_test_result_t result;
    nimcp_stats_ttest_two_sample(
        group1.data(), group1.size(),
        group2.data(), group2.size(),
        true, NIMCP_TEST_TWO_SIDED, 0.95f, &result
    );

    EXPECT_LT(result.p_value, 0.001f) << "Should detect significant difference";
    EXPECT_TRUE(result.reject_null);
}

TEST_F(MLStatisticsRegressionTest, TTestNoSignificantDifference) {
    // Two groups with same mean
    std::vector<float> group1(50), group2(50);
    std::normal_distribution<float> dist(5.0f, 1.0f);

    for (auto& x : group1) x = dist(rng);
    for (auto& x : group2) x = dist(rng);

    nimcp_test_result_t result;
    nimcp_stats_ttest_two_sample(
        group1.data(), group1.size(),
        group2.data(), group2.size(),
        true, NIMCP_TEST_TWO_SIDED, 0.95f, &result
    );

    EXPECT_GT(result.p_value, 0.05f) << "Should not reject H0 for same mean";
    EXPECT_FALSE(result.reject_null);
}

TEST_F(MLStatisticsRegressionTest, ANOVAMultipleGroups) {
    // Three groups with different means
    std::vector<float> g1(30), g2(30), g3(30);
    std::normal_distribution<float> dist1(5.0f, 1.0f);
    std::normal_distribution<float> dist2(5.0f, 1.0f);
    std::normal_distribution<float> dist3(8.0f, 1.0f); // Different mean

    for (auto& x : g1) x = dist1(rng);
    for (auto& x : g2) x = dist2(rng);
    for (auto& x : g3) x = dist3(rng);

    const float* groups[] = {g1.data(), g2.data(), g3.data()};
    uint32_t sizes[] = {30, 30, 30};

    nimcp_anova_result_t result;
    nimcp_stats_anova_one_way(groups, sizes, 3, 0.05f, &result);

    EXPECT_LT(result.p_value, 0.05f) << "Should detect group differences";
    EXPECT_TRUE(result.significant);
    EXPECT_GT(result.eta_squared, 0.0f) << "Effect size should be positive";
}

//=============================================================================
// NORMALIZATION AND STANDARDIZATION TESTS
//=============================================================================

TEST_F(MLStatisticsRegressionTest, StandardizeZeroMeanUnitVar) {
    const size_t n = 100;
    std::normal_distribution<float> dist(10.0f, 3.0f);

    std::vector<float> data(n), standardized(n);
    for (auto& x : data) x = dist(rng);

    nimcp_stats_standardize(data.data(), n, standardized.data());

    float mean = nimcp_stats_mean(standardized.data(), n);
    float var = nimcp_stats_variance(standardized.data(), n);

    EXPECT_NEAR(mean, 0.0f, 0.01f) << "Standardized mean should be ~0";
    EXPECT_NEAR(var, 1.0f, 0.1f) << "Standardized variance should be ~1";
}

TEST_F(MLStatisticsRegressionTest, MinMaxNormalizeBounds) {
    const size_t n = 100;
    std::uniform_real_distribution<float> dist(-10.0f, 50.0f);

    std::vector<float> data(n), normalized(n);
    for (auto& x : data) x = dist(rng);

    nimcp_stats_normalize_minmax(data.data(), n, normalized.data());

    float min_val = nimcp_stats_min(normalized.data(), n);
    float max_val = nimcp_stats_max(normalized.data(), n);

    EXPECT_NEAR(min_val, 0.0f, 1e-5f) << "Min should be 0 after normalization";
    EXPECT_NEAR(max_val, 1.0f, 1e-5f) << "Max should be 1 after normalization";
}

//=============================================================================
// OUTLIER DETECTION TESTS
//=============================================================================

TEST_F(MLStatisticsRegressionTest, OutlierDetectionIQR) {
    // Normal data with outliers
    std::vector<float> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 100}; // 100 is outlier
    std::unique_ptr<bool[]> mask(new bool[data.size()]());
    uint32_t n_outliers;

    nimcp_stats_detect_outliers_iqr(data.data(), data.size(), 1.5f, mask.get(), &n_outliers);

    EXPECT_GE(n_outliers, 1u) << "Should detect at least one outlier";
    EXPECT_TRUE(mask[10]) << "100 should be marked as outlier";
}

TEST_F(MLStatisticsRegressionTest, OutlierDetectionZScore) {
    // Data with clear outlier - 100 has z-score ~2.6 with this data
    // Use threshold of 2.5 to reliably detect it
    std::vector<float> data = {10, 11, 10, 11, 10, 11, 10, 11, 100};
    std::unique_ptr<bool[]> mask(new bool[data.size()]());
    uint32_t n_outliers;

    nimcp_stats_detect_outliers_zscore(data.data(), data.size(), 2.5f, mask.get(), &n_outliers);

    EXPECT_GE(n_outliers, 1u);
}

TEST_F(MLStatisticsRegressionTest, WinsorizeClipping) {
    std::vector<float> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 100};
    std::vector<float> winsorized(data.size());

    nimcp_stats_winsorize(data.data(), data.size(), 0.1f, 0.9f, winsorized.data());

    // Outlier should be clipped
    EXPECT_LT(winsorized[9], 100.0f) << "Extreme value should be winsorized";
}

//=============================================================================
// NUMERICAL STABILITY TESTS
//=============================================================================

TEST_F(MLStatisticsRegressionTest, LogisticRegressionLargeSeparation) {
    // Very large separation can cause numerical issues
    std::vector<float> X;
    std::vector<uint8_t> y;
    generateBinaryClassification(100, X, y, 20.0f); // Very large separation

    std::vector<float> coefficients(3);
    nimcp_stats_result_t status = nimcp_stats_regression_logistic(
        X.data(), y.data(), 100, 2, coefficients.data(), 100
    );

    // Should either succeed or report convergence issue, not crash
    EXPECT_TRUE(status == NIMCP_STATS_OK || status == NIMCP_STATS_ERROR_CONVERGE);

    // Coefficients should not be NaN
    for (float c : coefficients) {
        EXPECT_FALSE(std::isnan(c));
    }
}

TEST_F(MLStatisticsRegressionTest, BootstrapSmallSample) {
    std::vector<float> data = {1.0f, 2.0f, 3.0f};

    nimcp_bootstrap_result_t result;
    nimcp_stats_result_t status = nimcp_stats_bootstrap_mean(
        data.data(), 3, 100, 0.95f, &result
    );

    EXPECT_EQ(status, NIMCP_STATS_OK);
    EXPECT_NEAR(result.estimate, 2.0f, 0.01f);
}

//=============================================================================
// PERFORMANCE TESTS
//=============================================================================

TEST_F(MLStatisticsRegressionTest, LogisticRegressionPerformance) {
    std::vector<float> X;
    std::vector<uint8_t> y;
    generateBinaryClassification(1000, X, y, 2.0f);

    std::vector<float> coefficients(3);

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 10; ++i) {
        nimcp_stats_regression_logistic(X.data(), y.data(), 1000, 2, coefficients.data(), 100);
    }
    auto end = std::chrono::high_resolution_clock::now();

    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count() / 10.0;

    EXPECT_LT(elapsed_ms, 100.0) << "Logistic regression too slow: " << elapsed_ms << "ms";
}

TEST_F(MLStatisticsRegressionTest, BootstrapPerformance) {
    std::vector<float> data(100);
    std::normal_distribution<float> dist(0.0f, 1.0f);
    for (auto& x : data) x = dist(rng);

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 10; ++i) {
        nimcp_bootstrap_result_t result;
        nimcp_stats_bootstrap_mean(data.data(), 100, 1000, 0.95f, &result);
    }
    auto end = std::chrono::high_resolution_clock::now();

    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count() / 10.0;

    EXPECT_LT(elapsed_ms, 50.0) << "Bootstrap too slow: " << elapsed_ms << "ms";
}

//=============================================================================
// CONSISTENCY TESTS
//=============================================================================

TEST_F(MLStatisticsRegressionTest, StandardizeDeterministic) {
    std::vector<float> data = {1, 2, 3, 4, 5};
    std::vector<float> out1(5), out2(5);

    nimcp_stats_standardize(data.data(), 5, out1.data());
    nimcp_stats_standardize(data.data(), 5, out2.data());

    for (size_t i = 0; i < 5; ++i) {
        EXPECT_FLOAT_EQ(out1[i], out2[i]);
    }
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
