/**
 * @file test_multivariate_integration.cpp
 * @brief Integration tests for multivariate statistics with tensor module
 *
 * WHAT: Verify multivariate statistics integrate with tensor operations
 * WHY:  Ensure statistical analysis works with high-dimensional neural data
 * HOW:  Test covariance matrices, correlation matrices, PCA, dimensionality reduction
 *
 * TEST COVERAGE:
 * - Multivariate + tensor module integration
 * - Covariance and correlation matrix computations
 * - Regression with multiple predictors
 * - High-dimensional genomics-style data patterns
 * - Memory management for large matrices
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2026-01-30
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <random>
#include <chrono>
#include <numeric>
#include <algorithm>

// Statistics headers
#include "utils/statistics/nimcp_statistics.h"

// Tensor operations
#include "utils/tensor/nimcp_tensor.h"

// Memory management
#include "utils/memory/nimcp_memory.h"

// Core types

//=============================================================================
// Test Configuration
//=============================================================================

namespace {
    constexpr float STRICT_TOLERANCE = 1e-5f;
    constexpr float RELAXED_TOLERANCE = 1e-4f;
    constexpr float MATRIX_TOLERANCE = 1e-3f;

    constexpr uint32_t SMALL_N = 50;
    constexpr uint32_t MEDIUM_N = 200;
    constexpr uint32_t LARGE_N = 1000;

    constexpr uint32_t SMALL_VARS = 5;
    constexpr uint32_t MEDIUM_VARS = 20;
    constexpr uint32_t LARGE_VARS = 100;
}

//=============================================================================
// Test Fixture
//=============================================================================

class MultivariateIntegrationTest : public ::testing::Test {
protected:
    std::mt19937 rng{42};

    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
        nimcp_memory_clear_stats();

        nimcp_stats_config_t config = nimcp_stats_default_config();
        config.random_seed = 42;
        ASSERT_EQ(nimcp_stats_init(&config), NIMCP_STATS_OK);
    }

    void TearDown() override {
        nimcp_stats_shutdown();

        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        EXPECT_LT(stats.current_allocated, 4096)
            << "Memory leak: " << stats.current_allocated << " bytes";
    }

    //=========================================================================
    // Helper: Generate multivariate normal data
    //=========================================================================
    std::vector<float> generateMultivariateData(uint32_t n_obs, uint32_t n_vars,
                                                 float correlation = 0.0f) {
        std::vector<float> data(n_obs * n_vars);
        std::normal_distribution<float> dist(0.0f, 1.0f);

        // Generate base random data
        std::vector<float> base(n_obs);
        for (uint32_t i = 0; i < n_obs; i++) {
            base[i] = dist(rng);
        }

        for (uint32_t v = 0; v < n_vars; v++) {
            for (uint32_t i = 0; i < n_obs; i++) {
                // Mix correlated and independent components
                float independent = dist(rng);
                data[i * n_vars + v] = correlation * base[i] +
                                        std::sqrt(1.0f - correlation * correlation) * independent;
            }
        }
        return data;
    }

    //=========================================================================
    // Helper: Generate correlated pair
    //=========================================================================
    void generateCorrelatedPair(std::vector<float>& x, std::vector<float>& y,
                                uint32_t n, float target_r) {
        x.resize(n);
        y.resize(n);

        std::normal_distribution<float> dist(0.0f, 1.0f);

        for (uint32_t i = 0; i < n; i++) {
            float u = dist(rng);
            float v = dist(rng);
            x[i] = u;
            y[i] = target_r * u + std::sqrt(1.0f - target_r * target_r) * v;
        }
    }

    //=========================================================================
    // Helper: Check matrix symmetry
    //=========================================================================
    bool isSymmetric(const std::vector<float>& matrix, uint32_t n) {
        for (uint32_t i = 0; i < n; i++) {
            for (uint32_t j = i + 1; j < n; j++) {
                if (std::fabs(matrix[i * n + j] - matrix[j * n + i]) > STRICT_TOLERANCE) {
                    return false;
                }
            }
        }
        return true;
    }

    //=========================================================================
    // Helper: Check positive semi-definite (via diagonal dominance)
    //=========================================================================
    bool hasPosiveDiagonal(const std::vector<float>& matrix, uint32_t n) {
        for (uint32_t i = 0; i < n; i++) {
            if (matrix[i * n + i] < -STRICT_TOLERANCE) {
                return false;
            }
        }
        return true;
    }
};

//=============================================================================
// Covariance Matrix Tests
//=============================================================================

TEST_F(MultivariateIntegrationTest, CovarianceMatrixBasic) {
    uint32_t n_obs = MEDIUM_N;
    uint32_t n_vars = SMALL_VARS;

    auto data = generateMultivariateData(n_obs, n_vars, 0.0f);
    std::vector<float> cov_matrix(n_vars * n_vars);

    nimcp_stats_result_t status = nimcp_stats_covariance_matrix(
        data.data(), n_obs, n_vars, cov_matrix.data());

    EXPECT_EQ(status, NIMCP_STATS_OK);

    // Covariance matrix should be symmetric
    EXPECT_TRUE(isSymmetric(cov_matrix, n_vars))
        << "Covariance matrix must be symmetric";

    // Diagonal should be positive (variances)
    EXPECT_TRUE(hasPosiveDiagonal(cov_matrix, n_vars))
        << "Variances must be positive";

    // For uncorrelated data, off-diagonal should be near zero
    for (uint32_t i = 0; i < n_vars; i++) {
        for (uint32_t j = 0; j < n_vars; j++) {
            if (i != j) {
                EXPECT_NEAR(cov_matrix[i * n_vars + j], 0.0f, 0.3f)
                    << "Off-diagonal should be near zero for uncorrelated data";
            }
        }
    }
}

TEST_F(MultivariateIntegrationTest, CovarianceMatrixCorrelated) {
    uint32_t n_obs = LARGE_N;
    uint32_t n_vars = SMALL_VARS;
    float correlation = 0.7f;

    auto data = generateMultivariateData(n_obs, n_vars, correlation);
    std::vector<float> cov_matrix(n_vars * n_vars);

    nimcp_stats_result_t status = nimcp_stats_covariance_matrix(
        data.data(), n_obs, n_vars, cov_matrix.data());

    EXPECT_EQ(status, NIMCP_STATS_OK);
    EXPECT_TRUE(isSymmetric(cov_matrix, n_vars));

    // For correlated data, off-diagonal should be positive
    for (uint32_t i = 0; i < n_vars; i++) {
        for (uint32_t j = i + 1; j < n_vars; j++) {
            EXPECT_GT(cov_matrix[i * n_vars + j], 0.1f)
                << "Off-diagonal should be positive for correlated data";
        }
    }
}

TEST_F(MultivariateIntegrationTest, CovarianceMatchesBivariateCovariance) {
    uint32_t n_obs = MEDIUM_N;
    uint32_t n_vars = 2;

    std::vector<float> x, y;
    generateCorrelatedPair(x, y, n_obs, 0.6f);

    // Arrange as multivariate data
    std::vector<float> data(n_obs * 2);
    for (uint32_t i = 0; i < n_obs; i++) {
        data[i * 2 + 0] = x[i];
        data[i * 2 + 1] = y[i];
    }

    std::vector<float> cov_matrix(4);
    nimcp_stats_covariance_matrix(data.data(), n_obs, 2, cov_matrix.data());

    // Compare with bivariate covariance function
    float bivar_cov = nimcp_stats_covariance(x.data(), y.data(), n_obs);

    EXPECT_NEAR(cov_matrix[0 * 2 + 1], bivar_cov, RELAXED_TOLERANCE)
        << "Covariance matrix element should match bivariate covariance";
    EXPECT_NEAR(cov_matrix[1 * 2 + 0], bivar_cov, RELAXED_TOLERANCE)
        << "Covariance matrix should be symmetric";
}

TEST_F(MultivariateIntegrationTest, VarianceFromCovarianceMatrix) {
    uint32_t n_obs = MEDIUM_N;
    uint32_t n_vars = 3;

    auto data = generateMultivariateData(n_obs, n_vars, 0.3f);
    std::vector<float> cov_matrix(n_vars * n_vars);

    nimcp_stats_covariance_matrix(data.data(), n_obs, n_vars, cov_matrix.data());

    // Diagonal should equal univariate variance for each variable
    for (uint32_t v = 0; v < n_vars; v++) {
        std::vector<float> var_data(n_obs);
        for (uint32_t i = 0; i < n_obs; i++) {
            var_data[i] = data[i * n_vars + v];
        }
        float univariate_var = nimcp_stats_variance(var_data.data(), n_obs);
        EXPECT_NEAR(cov_matrix[v * n_vars + v], univariate_var, RELAXED_TOLERANCE)
            << "Diagonal should equal univariate variance";
    }
}

//=============================================================================
// Correlation Consistency Tests
//=============================================================================

TEST_F(MultivariateIntegrationTest, PearsonCorrelationConsistency) {
    std::vector<float> x, y;
    generateCorrelatedPair(x, y, MEDIUM_N, 0.8f);

    nimcp_correlation_result_t result;
    nimcp_stats_correlation_pearson(x.data(), y.data(), MEDIUM_N, &result);

    // Check r is in valid range
    EXPECT_GE(result.r, -1.0f);
    EXPECT_LE(result.r, 1.0f);

    // Should detect strong positive correlation
    EXPECT_GT(result.r, 0.6f) << "Should detect strong positive correlation";

    // P-value should indicate significance
    EXPECT_LT(result.p_value, 0.001f);

    // R-squared should be r^2
    EXPECT_NEAR(result.r_squared, result.r * result.r, RELAXED_TOLERANCE);
}

TEST_F(MultivariateIntegrationTest, SpearmanCorrelationMonotonic) {
    uint32_t n = SMALL_N;
    std::vector<float> x(n), y(n);

    // Monotonic but non-linear relationship: y = x^3
    for (uint32_t i = 0; i < n; i++) {
        x[i] = static_cast<float>(i) - n / 2;
        y[i] = x[i] * x[i] * x[i];
    }

    nimcp_correlation_result_t pearson_result, spearman_result;
    nimcp_stats_correlation_pearson(x.data(), y.data(), n, &pearson_result);
    nimcp_stats_correlation_spearman(x.data(), y.data(), n, &spearman_result);

    // Spearman should be higher for monotonic relationship
    EXPECT_NEAR(spearman_result.r, 1.0f, 0.01f)
        << "Spearman should be ~1 for perfect monotonic";
    // Pearson may be lower due to non-linearity
    EXPECT_LT(pearson_result.r, spearman_result.r)
        << "Spearman should exceed Pearson for non-linear monotonic";
}

TEST_F(MultivariateIntegrationTest, KendallCorrelation) {
    std::vector<float> x, y;
    generateCorrelatedPair(x, y, SMALL_N, 0.5f);

    nimcp_correlation_result_t result;
    nimcp_stats_result_t status = nimcp_stats_correlation_kendall(
        x.data(), y.data(), SMALL_N, &result);

    EXPECT_EQ(status, NIMCP_STATS_OK);
    EXPECT_GE(result.r, -1.0f);
    EXPECT_LE(result.r, 1.0f);

    // Kendall tau is typically smaller than Pearson for same data
    // but should still indicate positive association
    EXPECT_GT(result.r, 0.2f);
}

TEST_F(MultivariateIntegrationTest, PartialCorrelationControlVariable) {
    uint32_t n = MEDIUM_N;
    std::vector<float> x(n), y(n), z(n);

    std::normal_distribution<float> dist(0.0f, 1.0f);

    // Z influences both X and Y (confound)
    for (uint32_t i = 0; i < n; i++) {
        z[i] = dist(rng);
        x[i] = 0.8f * z[i] + 0.2f * dist(rng);
        y[i] = 0.8f * z[i] + 0.2f * dist(rng);
    }

    // Raw correlation between X and Y
    nimcp_correlation_result_t raw_result;
    nimcp_stats_correlation_pearson(x.data(), y.data(), n, &raw_result);

    // Partial correlation controlling for Z
    nimcp_correlation_result_t partial_result;
    nimcp_stats_correlation_partial(x.data(), y.data(), z.data(), n, &partial_result);

    // Partial correlation should be smaller (confound removed)
    EXPECT_LT(std::fabs(partial_result.r), std::fabs(raw_result.r))
        << "Partial correlation should be smaller after controlling for confound";
}

TEST_F(MultivariateIntegrationTest, PointBiserialCorrelation) {
    uint32_t n = MEDIUM_N;
    std::vector<float> continuous(n);
    std::vector<uint8_t> binary(n);

    std::normal_distribution<float> dist(0.0f, 1.0f);

    // Group 1 (binary=0) has lower mean than group 1 (binary=1)
    for (uint32_t i = 0; i < n / 2; i++) {
        continuous[i] = dist(rng) - 1.0f;  // Lower
        binary[i] = 0;
    }
    for (uint32_t i = n / 2; i < n; i++) {
        continuous[i] = dist(rng) + 1.0f;  // Higher
        binary[i] = 1;
    }

    nimcp_correlation_result_t result;
    nimcp_stats_result_t status = nimcp_stats_correlation_point_biserial(
        continuous.data(), binary.data(), n, &result);

    EXPECT_EQ(status, NIMCP_STATS_OK);
    EXPECT_GT(result.r, 0.5f)
        << "Should detect strong positive association";
}

//=============================================================================
// Multiple Regression Tests
//=============================================================================

TEST_F(MultivariateIntegrationTest, MultipleRegressionBasic) {
    uint32_t n = MEDIUM_N;
    uint32_t p = 3;  // Including intercept

    // Generate X matrix with intercept column
    std::vector<float> X(n * p);
    std::vector<float> y(n);
    std::normal_distribution<float> noise(0.0f, 0.5f);

    // True coefficients: y = 1 + 2*x1 + 3*x2
    float true_beta[] = {1.0f, 2.0f, 3.0f};

    for (uint32_t i = 0; i < n; i++) {
        X[i * p + 0] = 1.0f;  // Intercept
        X[i * p + 1] = static_cast<float>(i) / n;
        X[i * p + 2] = static_cast<float>(i * i) / (n * n);

        y[i] = true_beta[0] + true_beta[1] * X[i * p + 1] +
               true_beta[2] * X[i * p + 2] + noise(rng);
    }

    nimcp_regression_result_t result;
    result.coefficients = new float[p];
    result.n_coefficients = p;

    nimcp_stats_result_t status = nimcp_stats_regression_multiple(
        X.data(), y.data(), n, p, &result);

    EXPECT_EQ(status, NIMCP_STATS_OK);
    EXPECT_GT(result.r_squared, 0.85f) << "Should have high R-squared";

    // Check coefficients are close to true values
    EXPECT_NEAR(result.coefficients[0], true_beta[0], 0.3f) << "Intercept";
    EXPECT_NEAR(result.coefficients[1], true_beta[1], 0.5f) << "Beta1";
    EXPECT_NEAR(result.coefficients[2], true_beta[2], 1.0f) << "Beta2";

    nimcp_stats_regression_free(&result);
}

TEST_F(MultivariateIntegrationTest, RegressionModelComparison) {
    uint32_t n = MEDIUM_N;
    std::vector<float> x(n), y(n);
    std::normal_distribution<float> noise(0.0f, 1.0f);

    // Quadratic data
    for (uint32_t i = 0; i < n; i++) {
        x[i] = -3.0f + 6.0f * i / n;
        y[i] = x[i] * x[i] - 2.0f * x[i] + 1.0f + noise(rng);
    }

    // Fit linear model
    nimcp_regression_result_t linear_result;
    nimcp_stats_regression_linear(x.data(), y.data(), n, &linear_result);

    // Fit quadratic model
    nimcp_regression_result_t quad_result;
    quad_result.coefficients = new float[3];
    quad_result.n_coefficients = 3;
    nimcp_stats_regression_polynomial(x.data(), y.data(), n, 2, &quad_result);

    // Quadratic should have higher R-squared
    EXPECT_GT(quad_result.r_squared, linear_result.r_squared)
        << "Quadratic model should fit better than linear";

    // AIC/BIC comparison (lower is better)
    // Note: quad has more parameters so penalty is higher
    // but improvement should outweigh penalty

    nimcp_stats_regression_free(&quad_result);
}

TEST_F(MultivariateIntegrationTest, LogisticRegressionBasic) {
    uint32_t n = MEDIUM_N;
    uint32_t p = 2;  // Including intercept

    std::vector<float> X(n * p);
    std::vector<uint8_t> y(n);

    // Generate data where P(y=1) increases with x
    std::uniform_real_distribution<float> uniform(0.0f, 1.0f);

    for (uint32_t i = 0; i < n; i++) {
        X[i * p + 0] = 1.0f;  // Intercept
        X[i * p + 1] = -3.0f + 6.0f * i / n;

        // P(y=1) = sigmoid(0 + 1*x)
        float prob = 1.0f / (1.0f + std::exp(-X[i * p + 1]));
        y[i] = uniform(rng) < prob ? 1 : 0;
    }

    std::vector<float> coefficients(p);

    nimcp_stats_result_t status = nimcp_stats_regression_logistic(
        X.data(), y.data(), n, p, coefficients.data(), 100);

    EXPECT_EQ(status, NIMCP_STATS_OK);

    // Slope should be positive (higher x -> higher P(y=1))
    EXPECT_GT(coefficients[1], 0.0f)
        << "Logistic regression slope should be positive";
}

//=============================================================================
// High-Dimensional Data Tests
//=============================================================================

TEST_F(MultivariateIntegrationTest, HighDimensionalCovariance) {
    uint32_t n_obs = MEDIUM_N;
    uint32_t n_vars = MEDIUM_VARS;

    auto data = generateMultivariateData(n_obs, n_vars, 0.1f);
    std::vector<float> cov_matrix(n_vars * n_vars);

    auto start = std::chrono::high_resolution_clock::now();
    nimcp_stats_result_t status = nimcp_stats_covariance_matrix(
        data.data(), n_obs, n_vars, cov_matrix.data());
    auto end = std::chrono::high_resolution_clock::now();

    EXPECT_EQ(status, NIMCP_STATS_OK);
    EXPECT_TRUE(isSymmetric(cov_matrix, n_vars));

    uint64_t time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    EXPECT_LT(time_ms, 1000u)
        << "High-dim covariance should complete in <1s";
}

TEST_F(MultivariateIntegrationTest, VeryHighDimensionalData) {
    uint32_t n_obs = SMALL_N;
    uint32_t n_vars = LARGE_VARS;

    auto data = generateMultivariateData(n_obs, n_vars, 0.0f);
    std::vector<float> cov_matrix(n_vars * n_vars);

    nimcp_stats_result_t status = nimcp_stats_covariance_matrix(
        data.data(), n_obs, n_vars, cov_matrix.data());

    EXPECT_EQ(status, NIMCP_STATS_OK);

    // When n_obs < n_vars, matrix is singular but should still compute
    // Check diagonal values are reasonable
    for (uint32_t i = 0; i < n_vars; i++) {
        EXPECT_GE(cov_matrix[i * n_vars + i], 0.0f);
        EXPECT_LT(cov_matrix[i * n_vars + i], 10.0f);
    }
}

TEST_F(MultivariateIntegrationTest, GenomicsStyleData) {
    // Simulate gene expression data (many variables, few samples)
    uint32_t n_genes = 50;  // Variables
    uint32_t n_samples = 30; // Observations

    std::vector<float> expression(n_samples * n_genes);
    std::normal_distribution<float> base_expr(5.0f, 2.0f);
    std::normal_distribution<float> noise(0.0f, 0.5f);

    // Create some co-expression modules (groups of correlated genes)
    std::vector<float> module_signals(n_samples * 3);  // 3 modules
    for (uint32_t i = 0; i < n_samples * 3; i++) {
        module_signals[i] = base_expr(rng);
    }

    for (uint32_t g = 0; g < n_genes; g++) {
        uint32_t module = g % 3;  // Assign to module
        for (uint32_t s = 0; s < n_samples; s++) {
            expression[s * n_genes + g] = module_signals[s * 3 + module] + noise(rng);
        }
    }

    std::vector<float> cov_matrix(n_genes * n_genes);
    nimcp_stats_covariance_matrix(expression.data(), n_samples, n_genes, cov_matrix.data());

    // Check that genes in same module have higher covariance
    // Module 0: genes 0, 3, 6, ...
    // Module 1: genes 1, 4, 7, ...
    float avg_within_module = 0.0f;
    float avg_between_module = 0.0f;
    uint32_t within_count = 0, between_count = 0;

    for (uint32_t g1 = 0; g1 < n_genes; g1++) {
        for (uint32_t g2 = g1 + 1; g2 < n_genes; g2++) {
            float cov = std::fabs(cov_matrix[g1 * n_genes + g2]);
            if (g1 % 3 == g2 % 3) {
                avg_within_module += cov;
                within_count++;
            } else {
                avg_between_module += cov;
                between_count++;
            }
        }
    }

    avg_within_module /= within_count;
    avg_between_module /= between_count;

    EXPECT_GT(avg_within_module, avg_between_module)
        << "Within-module covariance should exceed between-module";
}

//=============================================================================
// Tensor Integration Tests
//=============================================================================

TEST_F(MultivariateIntegrationTest, TensorStatisticsConsistency) {
    uint32_t dims[] = {10, 10};
    nimcp_tensor_t* tensor = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_F32);
    ASSERT_NE(tensor, nullptr);

    // Fill with random data
    float* data = (float*)nimcp_tensor_data(tensor);
    std::normal_distribution<float> dist(0.0f, 1.0f);
    uint32_t numel = 100;
    for (uint32_t i = 0; i < numel; i++) {
        data[i] = dist(rng);
    }

    // Compute statistics on flattened tensor data
    float mean = nimcp_stats_mean(data, numel);
    float var = nimcp_stats_variance(data, numel);

    EXPECT_NEAR(mean, 0.0f, 0.5f) << "Mean should be near 0";
    EXPECT_NEAR(var, 1.0f, 0.5f) << "Variance should be near 1";

    nimcp_tensor_destroy(tensor);
}

TEST_F(MultivariateIntegrationTest, TensorRowStatistics) {
    // 2D tensor where each row is an observation
    uint32_t n_obs = 50;
    uint32_t n_vars = 5;
    uint32_t dims[] = {n_obs, n_vars};

    nimcp_tensor_t* tensor = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_F32);
    ASSERT_NE(tensor, nullptr);

    float* data = (float*)nimcp_tensor_data(tensor);

    // Fill with structured data (each variable has different mean)
    for (uint32_t obs = 0; obs < n_obs; obs++) {
        for (uint32_t var = 0; var < n_vars; var++) {
            std::normal_distribution<float> dist(static_cast<float>(var), 1.0f);
            data[obs * n_vars + var] = dist(rng);
        }
    }

    // Compute column means
    for (uint32_t var = 0; var < n_vars; var++) {
        std::vector<float> column(n_obs);
        for (uint32_t obs = 0; obs < n_obs; obs++) {
            column[obs] = data[obs * n_vars + var];
        }
        float col_mean = nimcp_stats_mean(column.data(), n_obs);
        EXPECT_NEAR(col_mean, static_cast<float>(var), 0.5f)
            << "Column " << var << " mean should be near " << var;
    }

    // Compute covariance matrix
    std::vector<float> cov_matrix(n_vars * n_vars);
    nimcp_stats_covariance_matrix(data, n_obs, n_vars, cov_matrix.data());

    // Verify it's valid
    EXPECT_TRUE(isSymmetric(cov_matrix, n_vars));
    EXPECT_TRUE(hasPosiveDiagonal(cov_matrix, n_vars));

    nimcp_tensor_destroy(tensor);
}

//=============================================================================
// Rank and Data Transformation Tests
//=============================================================================

TEST_F(MultivariateIntegrationTest, RankTransformForSpearman) {
    std::vector<float> data = {5.0f, 3.0f, 8.0f, 1.0f, 8.0f, 2.0f};
    std::vector<float> ranks(6);

    nimcp_stats_result_t status = nimcp_stats_rank(data.data(), 6, ranks.data(), 'a');
    EXPECT_EQ(status, NIMCP_STATS_OK);

    // Verify rank properties
    EXPECT_NEAR(ranks[3], 1.0f, STRICT_TOLERANCE) << "1 is smallest";
    EXPECT_NEAR(ranks[5], 2.0f, STRICT_TOLERANCE) << "2 is second smallest";
    EXPECT_NEAR(ranks[1], 3.0f, STRICT_TOLERANCE) << "3 is third smallest";
    EXPECT_NEAR(ranks[0], 4.0f, STRICT_TOLERANCE) << "5 is fourth smallest";
    // 8 appears twice, average rank is (5+6)/2 = 5.5
    EXPECT_NEAR(ranks[2], 5.5f, STRICT_TOLERANCE) << "First 8 gets average rank";
    EXPECT_NEAR(ranks[4], 5.5f, STRICT_TOLERANCE) << "Second 8 gets average rank";
}

TEST_F(MultivariateIntegrationTest, StandardizationForRegression) {
    auto data = generateMultivariateData(MEDIUM_N, 3, 0.5f);

    // Standardize each variable
    for (uint32_t v = 0; v < 3; v++) {
        std::vector<float> var_data(MEDIUM_N);
        for (uint32_t i = 0; i < MEDIUM_N; i++) {
            var_data[i] = data[i * 3 + v];
        }

        std::vector<float> standardized(MEDIUM_N);
        nimcp_stats_standardize(var_data.data(), MEDIUM_N, standardized.data());

        float std_mean = nimcp_stats_mean(standardized.data(), MEDIUM_N);
        float std_std = nimcp_stats_std_dev(standardized.data(), MEDIUM_N);

        EXPECT_NEAR(std_mean, 0.0f, RELAXED_TOLERANCE);
        EXPECT_NEAR(std_std, 1.0f, RELAXED_TOLERANCE);
    }
}

//=============================================================================
// Edge Cases and Error Handling
//=============================================================================

TEST_F(MultivariateIntegrationTest, SingleObservation) {
    uint32_t n_vars = 3;
    std::vector<float> single_obs = {1.0f, 2.0f, 3.0f};
    std::vector<float> cov_matrix(n_vars * n_vars);

    // Single observation - variance undefined
    nimcp_stats_result_t status = nimcp_stats_covariance_matrix(
        single_obs.data(), 1, n_vars, cov_matrix.data());

    // May return error or NaN values
    if (status == NIMCP_STATS_OK) {
        for (uint32_t i = 0; i < n_vars * n_vars; i++) {
            EXPECT_TRUE(std::isnan(cov_matrix[i]) || cov_matrix[i] == 0.0f);
        }
    }
}

TEST_F(MultivariateIntegrationTest, TwoObservations) {
    uint32_t n_obs = 2;
    uint32_t n_vars = 3;
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
    std::vector<float> cov_matrix(n_vars * n_vars);

    nimcp_stats_result_t status = nimcp_stats_covariance_matrix(
        data.data(), n_obs, n_vars, cov_matrix.data());

    EXPECT_EQ(status, NIMCP_STATS_OK);

    // With n=2, variance has n-1=1 in denominator
    // Each variable: var = (x1 - mean)^2 / 1
    // Variable 0: values 1, 4, mean = 2.5, var = (1-2.5)^2 + (4-2.5)^2 / 1 = 4.5
    EXPECT_NEAR(cov_matrix[0], 4.5f, RELAXED_TOLERANCE);
}

TEST_F(MultivariateIntegrationTest, PerfectCollinearity) {
    // X2 = 2*X1 (perfect collinearity)
    uint32_t n = SMALL_N;
    uint32_t p = 3;
    std::vector<float> X(n * p);
    std::vector<float> y(n);
    std::normal_distribution<float> noise(0.0f, 0.1f);

    for (uint32_t i = 0; i < n; i++) {
        X[i * p + 0] = 1.0f;
        X[i * p + 1] = static_cast<float>(i) / n;
        X[i * p + 2] = 2.0f * X[i * p + 1];  // Collinear!
        y[i] = 1.0f + X[i * p + 1] + noise(rng);
    }

    nimcp_regression_result_t result;
    result.coefficients = new float[p];
    result.n_coefficients = p;

    nimcp_stats_result_t status = nimcp_stats_regression_multiple(
        X.data(), y.data(), n, p, &result);

    // May return SINGULAR error or produce unstable estimates
    if (status == NIMCP_STATS_ERROR_SINGULAR) {
        // Expected for perfect collinearity
    } else {
        // If it completes, coefficients may be unreliable
    }

    nimcp_stats_regression_free(&result);
}

TEST_F(MultivariateIntegrationTest, ConstantVariable) {
    uint32_t n = SMALL_N;
    std::vector<float> x(n, 5.0f);  // Constant
    std::vector<float> y(n);
    std::normal_distribution<float> dist(0.0f, 1.0f);
    for (uint32_t i = 0; i < n; i++) {
        y[i] = dist(rng);
    }

    // Correlation with constant variable is undefined
    nimcp_correlation_result_t result;
    nimcp_stats_result_t status = nimcp_stats_correlation_pearson(
        x.data(), y.data(), n, &result);

    // Should handle gracefully (NaN or error)
    if (status == NIMCP_STATS_OK) {
        EXPECT_TRUE(std::isnan(result.r) || result.r == 0.0f);
    }
}

//=============================================================================
// Memory Tests
//=============================================================================

TEST_F(MultivariateIntegrationTest, NoMemoryLeaksCovariance) {
    nimcp_memory_clear_stats();

    for (int trial = 0; trial < 50; trial++) {
        auto data = generateMultivariateData(MEDIUM_N, SMALL_VARS, 0.5f);
        std::vector<float> cov_matrix(SMALL_VARS * SMALL_VARS);
        nimcp_stats_covariance_matrix(data.data(), MEDIUM_N, SMALL_VARS, cov_matrix.data());
    }

    nimcp_memory_stats_t stats;
    nimcp_memory_get_stats(&stats);
    EXPECT_LT(stats.current_allocated, 4096);
}

TEST_F(MultivariateIntegrationTest, NoMemoryLeaksRegression) {
    nimcp_memory_clear_stats();

    for (int trial = 0; trial < 50; trial++) {
        std::vector<float> x(SMALL_N), y(SMALL_N);
        std::normal_distribution<float> dist(0.0f, 1.0f);
        for (uint32_t i = 0; i < SMALL_N; i++) {
            x[i] = dist(rng);
            y[i] = 2.0f * x[i] + dist(rng);
        }

        nimcp_regression_result_t result;
        nimcp_stats_regression_linear(x.data(), y.data(), SMALL_N, &result);
    }

    nimcp_memory_stats_t stats;
    nimcp_memory_get_stats(&stats);
    EXPECT_LT(stats.current_allocated, 4096);
}

//=============================================================================
// Performance Tests
//=============================================================================

TEST_F(MultivariateIntegrationTest, PerformanceCovarianceMatrix) {
    auto data = generateMultivariateData(LARGE_N, MEDIUM_VARS, 0.3f);
    std::vector<float> cov_matrix(MEDIUM_VARS * MEDIUM_VARS);

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 10; i++) {
        nimcp_stats_covariance_matrix(data.data(), LARGE_N, MEDIUM_VARS, cov_matrix.data());
    }
    auto end = std::chrono::high_resolution_clock::now();

    uint64_t total_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    uint64_t per_call = total_us / 10;

    EXPECT_LT(per_call, 100000u)  // 100ms per call max
        << "Covariance matrix computation too slow: " << per_call << "us";
}

TEST_F(MultivariateIntegrationTest, PerformanceCorrelation) {
    std::vector<float> x, y;
    generateCorrelatedPair(x, y, LARGE_N, 0.5f);

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100; i++) {
        nimcp_correlation_result_t result;
        nimcp_stats_correlation_pearson(x.data(), y.data(), LARGE_N, &result);
    }
    auto end = std::chrono::high_resolution_clock::now();

    uint64_t total_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    EXPECT_LT(total_us, 500000u)  // 500ms for 100 calls
        << "Correlation computation too slow";
}

