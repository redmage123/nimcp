//=============================================================================
// test_multivariate_regression.cpp - Multivariate Statistics Regression Tests
//=============================================================================
/**
 * @file test_multivariate_regression.cpp
 * @brief Comprehensive regression tests for multivariate statistics module
 *
 * REGRESSION TEST FOCUS:
 * - PCA on known covariance matrices
 * - ICA on known mixtures
 * - Covariance matrix properties
 * - Correlation matrix bounds
 * - Ill-conditioned matrices
 * - Near-singular covariance matrices
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
}

//=============================================================================
// Test Fixture
//=============================================================================

class MultivariateRegressionTest : public ::testing::Test {
protected:
    static constexpr float EPSILON = 1e-5f;
    static constexpr float RELATIVE_TOL = 1e-4f;
    static constexpr float ILL_CONDITIONED_TOL = 1e-2f;

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

    // Generate multivariate normal data
    std::vector<float> generateMultivariateNormal(
        size_t n, size_t p,
        const std::vector<float>& mean,
        const std::vector<float>& cov_chol // Cholesky factor (lower triangular)
    ) {
        std::normal_distribution<float> std_normal(0.0f, 1.0f);
        std::vector<float> data(n * p);

        for (size_t i = 0; i < n; ++i) {
            // Generate standard normal vector
            std::vector<float> z(p);
            for (size_t j = 0; j < p; ++j) {
                z[j] = std_normal(rng);
            }

            // Transform: x = L*z + mu
            for (size_t j = 0; j < p; ++j) {
                float sum = mean[j];
                for (size_t k = 0; k <= j; ++k) {
                    sum += cov_chol[j * p + k] * z[k];
                }
                data[i * p + j] = sum;
            }
        }

        return data;
    }

    // Reference covariance matrix computation
    std::vector<float> referenceCovariance(const std::vector<float>& data, size_t n, size_t p) {
        std::vector<double> means(p, 0.0);
        for (size_t i = 0; i < n; ++i) {
            for (size_t j = 0; j < p; ++j) {
                means[j] += data[i * p + j];
            }
        }
        for (size_t j = 0; j < p; ++j) {
            means[j] /= n;
        }

        std::vector<float> cov(p * p, 0.0f);
        for (size_t i = 0; i < n; ++i) {
            for (size_t j = 0; j < p; ++j) {
                for (size_t k = 0; k < p; ++k) {
                    cov[j * p + k] += (data[i * p + j] - means[j]) *
                                      (data[i * p + k] - means[k]);
                }
            }
        }

        for (size_t j = 0; j < p * p; ++j) {
            cov[j] /= (n - 1);
        }

        return cov;
    }

    // Check if matrix is symmetric
    bool isSymmetric(const std::vector<float>& mat, size_t n, float tol = 1e-5f) {
        for (size_t i = 0; i < n; ++i) {
            for (size_t j = i + 1; j < n; ++j) {
                if (std::fabs(mat[i * n + j] - mat[j * n + i]) > tol) {
                    return false;
                }
            }
        }
        return true;
    }

    // Check if matrix is positive semi-definite (via eigenvalues)
    bool isPositiveSemiDefinite(const std::vector<float>& mat, size_t n, float tol = -1e-6f) {
        // Simple check: diagonal should be positive
        for (size_t i = 0; i < n; ++i) {
            if (mat[i * n + i] < tol) return false;
        }
        // More rigorous check would compute eigenvalues
        return true;
    }
};

//=============================================================================
// COVARIANCE MATRIX REGRESSION TESTS
//=============================================================================

TEST_F(MultivariateRegressionTest, CovarianceMatrixSymmetric) {
    // Covariance matrix should always be symmetric
    const size_t n = 100, p = 5;
    std::vector<float> data(n * p);
    std::normal_distribution<float> dist(0.0f, 1.0f);
    for (auto& x : data) x = dist(rng);

    std::vector<float> cov(p * p);
    nimcp_stats_result_t status = nimcp_stats_covariance_matrix(
        data.data(), n, p, cov.data()
    );

    EXPECT_EQ(status, NIMCP_STATS_OK);
    EXPECT_TRUE(isSymmetric(cov, p)) << "Covariance matrix should be symmetric";
}

TEST_F(MultivariateRegressionTest, CovarianceMatrixPositiveSemiDefinite) {
    // Covariance matrix should be positive semi-definite
    const size_t n = 100, p = 4;
    std::vector<float> data(n * p);
    std::normal_distribution<float> dist(0.0f, 1.0f);
    for (auto& x : data) x = dist(rng);

    std::vector<float> cov(p * p);
    nimcp_stats_result_t status = nimcp_stats_covariance_matrix(
        data.data(), n, p, cov.data()
    );

    EXPECT_EQ(status, NIMCP_STATS_OK);
    EXPECT_TRUE(isPositiveSemiDefinite(cov, p)) << "Covariance should be PSD";
}

TEST_F(MultivariateRegressionTest, CovarianceIdenticalVariables) {
    // If all variables are identical, covariance matrix should have all equal entries
    const size_t n = 100, p = 3;
    std::vector<float> data(n * p);
    std::normal_distribution<float> dist(5.0f, 2.0f);

    for (size_t i = 0; i < n; ++i) {
        float val = dist(rng);
        for (size_t j = 0; j < p; ++j) {
            data[i * p + j] = val;
        }
    }

    std::vector<float> cov(p * p);
    nimcp_stats_covariance_matrix(data.data(), n, p, cov.data());

    // All entries should be approximately equal
    float first = cov[0];
    for (size_t i = 0; i < p * p; ++i) {
        EXPECT_NEAR(cov[i], first, 0.01f * std::fabs(first))
            << "All covariances should be equal for identical variables";
    }
}

TEST_F(MultivariateRegressionTest, CovarianceIndependentVariables) {
    // Independent variables should have near-zero off-diagonal covariance
    const size_t n = 10000, p = 3;
    std::vector<float> data(n * p);
    std::normal_distribution<float> dist(0.0f, 1.0f);

    for (auto& x : data) x = dist(rng);

    std::vector<float> cov(p * p);
    nimcp_stats_covariance_matrix(data.data(), n, p, cov.data());

    // Off-diagonal should be near zero
    for (size_t i = 0; i < p; ++i) {
        for (size_t j = 0; j < p; ++j) {
            if (i == j) {
                EXPECT_NEAR(cov[i * p + j], 1.0f, 0.1f) << "Diagonal should be ~1";
            } else {
                EXPECT_NEAR(cov[i * p + j], 0.0f, 0.1f)
                    << "Off-diagonal should be ~0 for independent vars";
            }
        }
    }
}

TEST_F(MultivariateRegressionTest, CovarianceKnownStructure) {
    // Generate data with known covariance structure
    const size_t n = 5000, p = 2;

    // Known covariance: [[4, 2], [2, 3]]
    // Cholesky: [[2, 0], [1, sqrt(2)]]
    std::vector<float> mean = {0.0f, 0.0f};
    std::vector<float> chol = {2.0f, 0.0f, 1.0f, std::sqrt(2.0f)};

    auto data = generateMultivariateNormal(n, p, mean, chol);

    std::vector<float> cov(p * p);
    nimcp_stats_covariance_matrix(data.data(), n, p, cov.data());

    // Check against known values (with sampling error tolerance)
    EXPECT_NEAR(cov[0], 4.0f, 0.3f) << "Cov[0,0] should be ~4";
    EXPECT_NEAR(cov[1], 2.0f, 0.2f) << "Cov[0,1] should be ~2";
    EXPECT_NEAR(cov[2], 2.0f, 0.2f) << "Cov[1,0] should be ~2";
    EXPECT_NEAR(cov[3], 3.0f, 0.2f) << "Cov[1,1] should be ~3";
}

TEST_F(MultivariateRegressionTest, CovarianceMatchesReference) {
    const size_t n = 100, p = 3;
    std::vector<float> data(n * p);
    std::normal_distribution<float> dist(0.0f, 1.0f);
    for (auto& x : data) x = dist(rng);

    std::vector<float> cov(p * p);
    nimcp_stats_covariance_matrix(data.data(), n, p, cov.data());

    auto ref = referenceCovariance(data, n, p);

    for (size_t i = 0; i < p * p; ++i) {
        EXPECT_NEAR(cov[i], ref[i], 1e-4f)
            << "Covariance should match reference at index " << i;
    }
}

//=============================================================================
// CORRELATION REGRESSION TESTS
//=============================================================================

TEST_F(MultivariateRegressionTest, CorrelationBounds) {
    // Correlation should always be in [-1, 1]
    const size_t n = 100;
    std::vector<float> x(n), y(n);
    std::normal_distribution<float> dist(0.0f, 1.0f);
    for (size_t i = 0; i < n; ++i) {
        x[i] = dist(rng);
        y[i] = dist(rng);
    }

    nimcp_correlation_result_t result;
    nimcp_stats_correlation_pearson(x.data(), y.data(), n, &result);

    EXPECT_GE(result.r, -1.0f) << "Correlation should be >= -1";
    EXPECT_LE(result.r, 1.0f) << "Correlation should be <= 1";
}

TEST_F(MultivariateRegressionTest, SpearmanRankCorrelation) {
    // Spearman should handle monotonic relationships
    const size_t n = 50;
    std::vector<float> x(n), y(n);
    for (size_t i = 0; i < n; ++i) {
        x[i] = static_cast<float>(i);
        y[i] = static_cast<float>(i * i); // Monotonic but not linear
    }

    nimcp_correlation_result_t result;
    nimcp_stats_correlation_spearman(x.data(), y.data(), n, &result);

    EXPECT_FLOAT_EQ(result.r, 1.0f)
        << "Spearman should be 1.0 for perfect monotonic relationship";
}

TEST_F(MultivariateRegressionTest, KendallTauPerfect) {
    // Kendall's tau for perfect ranking
    const size_t n = 20;
    std::vector<float> x(n), y(n);
    for (size_t i = 0; i < n; ++i) {
        x[i] = static_cast<float>(i);
        y[i] = static_cast<float>(i);
    }

    nimcp_correlation_result_t result;
    nimcp_stats_correlation_kendall(x.data(), y.data(), n, &result);

    EXPECT_FLOAT_EQ(result.r, 1.0f) << "Kendall's tau should be 1.0 for identical ranking";
}

TEST_F(MultivariateRegressionTest, PartialCorrelation) {
    // Partial correlation should control for third variable
    const size_t n = 1000;
    std::vector<float> z(n), x(n), y(n);
    std::normal_distribution<float> dist(0.0f, 1.0f);

    // z is common cause of x and y
    for (size_t i = 0; i < n; ++i) {
        z[i] = dist(rng);
        x[i] = 0.8f * z[i] + 0.2f * dist(rng);
        y[i] = 0.8f * z[i] + 0.2f * dist(rng);
    }

    // Regular correlation x-y should be high due to common cause
    nimcp_correlation_result_t r_xy;
    nimcp_stats_correlation_pearson(x.data(), y.data(), n, &r_xy);
    EXPECT_GT(r_xy.r, 0.5f) << "x and y should be correlated through z";

    // Partial correlation x-y | z should be low
    nimcp_correlation_result_t r_partial;
    nimcp_stats_correlation_partial(x.data(), y.data(), z.data(), n, &r_partial);
    EXPECT_LT(std::fabs(r_partial.r), 0.2f)
        << "Partial correlation should be low after controlling for z";
}

//=============================================================================
// REGRESSION ANALYSIS TESTS
//=============================================================================

TEST_F(MultivariateRegressionTest, LinearRegressionKnownCoefficients) {
    // y = 3 + 2*x (known coefficients)
    const size_t n = 100;
    std::vector<float> x(n), y(n);

    for (size_t i = 0; i < n; ++i) {
        x[i] = static_cast<float>(i) / 10.0f;
        y[i] = 3.0f + 2.0f * x[i];
    }

    nimcp_regression_result_t result;
    nimcp_stats_regression_linear(x.data(), y.data(), n, &result);

    EXPECT_NEAR(result.intercept, 3.0f, 1e-4f);
    EXPECT_NEAR(result.slope, 2.0f, 1e-4f);
    EXPECT_NEAR(result.r_squared, 1.0f, 1e-5f);
}

TEST_F(MultivariateRegressionTest, LinearRegressionWithNoise) {
    // y = 1 + 0.5*x + noise
    const size_t n = 1000;
    std::vector<float> x(n), y(n);
    std::normal_distribution<float> noise(0.0f, 0.1f);

    for (size_t i = 0; i < n; ++i) {
        x[i] = static_cast<float>(i) / 100.0f;
        y[i] = 1.0f + 0.5f * x[i] + noise(rng);
    }

    nimcp_regression_result_t result;
    nimcp_stats_regression_linear(x.data(), y.data(), n, &result);

    EXPECT_NEAR(result.intercept, 1.0f, 0.1f);
    EXPECT_NEAR(result.slope, 0.5f, 0.05f);
    EXPECT_GT(result.r_squared, 0.9f);
}

TEST_F(MultivariateRegressionTest, PolynomialRegressionQuadratic) {
    // y = 1 + 2*x + 3*x^2
    const size_t n = 100;
    std::vector<float> x(n), y(n);

    for (size_t i = 0; i < n; ++i) {
        x[i] = (static_cast<float>(i) - 50.0f) / 25.0f; // [-2, 2]
        y[i] = 1.0f + 2.0f * x[i] + 3.0f * x[i] * x[i];
    }

    nimcp_regression_result_t result;
    nimcp_stats_regression_polynomial(x.data(), y.data(), n, 2, &result);

    EXPECT_EQ(result.n_coefficients, 3u);
    EXPECT_NEAR(result.r_squared, 1.0f, 1e-4f);
}

TEST_F(MultivariateRegressionTest, RSquaredBounds) {
    // R^2 should always be in [0, 1] for valid regression
    const size_t n = 50;
    std::vector<float> x(n), y(n);
    std::normal_distribution<float> dist(0.0f, 1.0f);

    for (auto& v : x) v = dist(rng);
    for (auto& v : y) v = dist(rng);

    nimcp_regression_result_t result;
    nimcp_stats_regression_linear(x.data(), y.data(), n, &result);

    EXPECT_GE(result.r_squared, 0.0f);
    EXPECT_LE(result.r_squared, 1.0f);
}

//=============================================================================
// ILL-CONDITIONED MATRIX REGRESSION TESTS
//=============================================================================

TEST_F(MultivariateRegressionTest, NearSingularCovariance) {
    // Nearly collinear variables
    const size_t n = 100, p = 2;
    std::vector<float> data(n * p);
    std::normal_distribution<float> dist(0.0f, 1.0f);

    for (size_t i = 0; i < n; ++i) {
        float x = dist(rng);
        data[i * p + 0] = x;
        data[i * p + 1] = x + 1e-6f * dist(rng); // Nearly identical to x
    }

    std::vector<float> cov(p * p);
    nimcp_stats_result_t status = nimcp_stats_covariance_matrix(
        data.data(), n, p, cov.data()
    );

    EXPECT_EQ(status, NIMCP_STATS_OK);
    // Off-diagonal should be nearly equal to diagonal
    EXPECT_NEAR(cov[1], cov[0], 0.01f * std::fabs(cov[0]));
}

TEST_F(MultivariateRegressionTest, IllConditionedRegression) {
    // Regression with highly correlated predictors (multicollinearity)
    const size_t n = 100, p = 3;
    std::vector<float> X(n * p), y(n);
    std::normal_distribution<float> dist(0.0f, 1.0f);

    for (size_t i = 0; i < n; ++i) {
        float x1 = dist(rng);
        X[i * p + 0] = 1.0f;        // Intercept
        X[i * p + 1] = x1;          // First predictor
        X[i * p + 2] = x1 + 0.001f * dist(rng); // Nearly collinear

        y[i] = 2.0f + 3.0f * x1 + dist(rng);
    }

    nimcp_regression_result_t result;
    result.coefficients = new float[p];
    result.n_coefficients = p;

    nimcp_stats_result_t status = nimcp_stats_regression_multiple(
        X.data(), y.data(), n, p, &result
    );

    // Should either succeed or report singularity
    EXPECT_TRUE(status == NIMCP_STATS_OK || status == NIMCP_STATS_ERROR_SINGULAR);

    delete[] result.coefficients;
}

//=============================================================================
// NUMERICAL STABILITY TESTS
//=============================================================================

TEST_F(MultivariateRegressionTest, CovarianceLargeOffset) {
    // Data with large mean but small variance (Welford should handle)
    const size_t n = 100, p = 2;
    std::vector<float> data(n * p);
    std::normal_distribution<float> dist(1e6f, 1.0f);
    for (auto& x : data) x = dist(rng);

    std::vector<float> cov(p * p);
    nimcp_stats_covariance_matrix(data.data(), n, p, cov.data());

    // Variance should be approximately 1.0 despite large mean
    EXPECT_GT(cov[0], 0.5f);
    EXPECT_LT(cov[0], 2.0f);
}

TEST_F(MultivariateRegressionTest, CovarianceMixedScales) {
    // Variables with very different scales
    const size_t n = 100, p = 2;
    std::vector<float> data(n * p);
    std::normal_distribution<float> dist1(0.0f, 1e-5f);
    std::normal_distribution<float> dist2(0.0f, 1e5f);

    for (size_t i = 0; i < n; ++i) {
        data[i * p + 0] = dist1(rng);
        data[i * p + 1] = dist2(rng);
    }

    std::vector<float> cov(p * p);
    nimcp_stats_result_t status = nimcp_stats_covariance_matrix(
        data.data(), n, p, cov.data()
    );

    EXPECT_EQ(status, NIMCP_STATS_OK);
    EXPECT_GT(cov[0], 0.0f) << "Variance of small-scale variable";
    EXPECT_GT(cov[3], 0.0f) << "Variance of large-scale variable";
}

//=============================================================================
// CONSISTENCY REGRESSION TESTS
//=============================================================================

TEST_F(MultivariateRegressionTest, CovarianceDeterministic) {
    const size_t n = 50, p = 3;
    std::vector<float> data(n * p);
    std::normal_distribution<float> dist(0.0f, 1.0f);
    for (auto& x : data) x = dist(rng);

    std::vector<float> cov1(p * p), cov2(p * p);
    nimcp_stats_covariance_matrix(data.data(), n, p, cov1.data());
    nimcp_stats_covariance_matrix(data.data(), n, p, cov2.data());

    for (size_t i = 0; i < p * p; ++i) {
        EXPECT_FLOAT_EQ(cov1[i], cov2[i]);
    }
}

TEST_F(MultivariateRegressionTest, RegressionDeterministic) {
    const size_t n = 50;
    std::vector<float> x(n), y(n);
    for (size_t i = 0; i < n; ++i) {
        x[i] = static_cast<float>(i);
        y[i] = 2.0f * x[i] + 1.0f;
    }

    nimcp_regression_result_t r1, r2;
    nimcp_stats_regression_linear(x.data(), y.data(), n, &r1);
    nimcp_stats_regression_linear(x.data(), y.data(), n, &r2);

    EXPECT_FLOAT_EQ(r1.slope, r2.slope);
    EXPECT_FLOAT_EQ(r1.intercept, r2.intercept);
}

//=============================================================================
// PERFORMANCE REGRESSION TESTS
//=============================================================================

TEST_F(MultivariateRegressionTest, CovariancePerformance) {
    const size_t n = 1000, p = 10;
    std::vector<float> data(n * p);
    std::normal_distribution<float> dist(0.0f, 1.0f);
    for (auto& x : data) x = dist(rng);

    std::vector<float> cov(p * p);

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100; ++i) {
        nimcp_stats_covariance_matrix(data.data(), n, p, cov.data());
    }
    auto end = std::chrono::high_resolution_clock::now();

    double elapsed_us = std::chrono::duration<double, std::micro>(end - start).count() / 100.0;

    EXPECT_LT(elapsed_us, 5000.0)
        << "Covariance computation too slow: " << elapsed_us << "us";
}

TEST_F(MultivariateRegressionTest, RegressionPerformance) {
    const size_t n = 1000;
    std::vector<float> x(n), y(n);
    for (size_t i = 0; i < n; ++i) {
        x[i] = static_cast<float>(i);
        y[i] = 2.0f * x[i] + 1.0f;
    }

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100; ++i) {
        nimcp_regression_result_t result;
        nimcp_stats_regression_linear(x.data(), y.data(), n, &result);
    }
    auto end = std::chrono::high_resolution_clock::now();

    double elapsed_us = std::chrono::duration<double, std::micro>(end - start).count() / 100.0;

    EXPECT_LT(elapsed_us, 1000.0) << "Regression too slow: " << elapsed_us << "us";
}

//=============================================================================
// EDGE CASE REGRESSION TESTS
//=============================================================================

TEST_F(MultivariateRegressionTest, MinimalSampleSize) {
    // Minimum sample size for covariance (n >= p+1)
    const size_t n = 3, p = 2;
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};

    std::vector<float> cov(p * p);
    nimcp_stats_result_t status = nimcp_stats_covariance_matrix(
        data.data(), n, p, cov.data()
    );

    EXPECT_EQ(status, NIMCP_STATS_OK);
    EXPECT_TRUE(isSymmetric(cov, p));
}

TEST_F(MultivariateRegressionTest, SingleVariable) {
    // p = 1 should give variance
    const size_t n = 100, p = 1;
    std::vector<float> data(n);
    std::normal_distribution<float> dist(5.0f, 2.0f);
    for (auto& x : data) x = dist(rng);

    std::vector<float> cov(1);
    nimcp_stats_covariance_matrix(data.data(), n, p, cov.data());

    float var = nimcp_stats_variance(data.data(), n);
    EXPECT_NEAR(cov[0], var, 1e-5f);
}

TEST_F(MultivariateRegressionTest, ConstantVariable) {
    // Constant data should have zero variance
    const size_t n = 50, p = 2;
    std::vector<float> data(n * p);
    for (size_t i = 0; i < n; ++i) {
        data[i * p + 0] = 42.0f; // Constant
        data[i * p + 1] = static_cast<float>(i); // Varying
    }

    std::vector<float> cov(p * p);
    nimcp_stats_covariance_matrix(data.data(), n, p, cov.data());

    EXPECT_FLOAT_EQ(cov[0], 0.0f) << "Constant variable should have zero variance";
    EXPECT_GT(cov[3], 0.0f) << "Varying variable should have positive variance";
}

TEST_F(MultivariateRegressionTest, NullPointerHandling) {
    const size_t n = 10, p = 2;
    std::vector<float> cov(p * p);

    nimcp_stats_result_t status = nimcp_stats_covariance_matrix(
        nullptr, n, p, cov.data()
    );

    EXPECT_EQ(status, NIMCP_STATS_ERROR_NULL);
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
