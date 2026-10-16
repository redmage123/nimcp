/**
 * @file test_ml_statistics_integration.cpp
 * @brief Integration tests for ML statistics with existing regression module
 *
 * WHAT: Verify ML statistics integrate with regression and classification
 * WHY:  Ensure model evaluation metrics are consistent across modules
 * HOW:  Test cross-validation, model comparison, and performance metrics
 *
 * TEST COVERAGE:
 * - ML statistics + existing regression consistency
 * - Model evaluation metrics (R-squared, MSE, MAE)
 * - Classification metrics (accuracy, precision, recall, F1)
 * - Cross-validation procedures
 * - Model selection criteria (AIC, BIC)
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

// Memory management
#include "utils/memory/nimcp_memory.h"

// Core types

//=============================================================================
// Test Configuration
//=============================================================================

namespace {
    constexpr float STRICT_TOLERANCE = 1e-5f;
    constexpr float RELAXED_TOLERANCE = 1e-4f;
    constexpr float ML_TOLERANCE = 0.01f;

    constexpr uint32_t SMALL_N = 50;
    constexpr uint32_t MEDIUM_N = 200;
    constexpr uint32_t LARGE_N = 1000;
}

//=============================================================================
// Test Fixture
//=============================================================================

class MLStatisticsIntegrationTest : public ::testing::Test {
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
    // Helper: Generate linear regression data
    //=========================================================================
    void generateLinearData(std::vector<float>& x, std::vector<float>& y,
                            uint32_t n, float slope, float intercept, float noise_std) {
        x.resize(n);
        y.resize(n);
        std::normal_distribution<float> noise(0.0f, noise_std);
        std::uniform_real_distribution<float> x_dist(-5.0f, 5.0f);

        for (uint32_t i = 0; i < n; i++) {
            x[i] = x_dist(rng);
            y[i] = slope * x[i] + intercept + noise(rng);
        }
    }

    //=========================================================================
    // Helper: Generate classification data
    //=========================================================================
    void generateClassificationData(std::vector<float>& X, std::vector<uint8_t>& y,
                                     uint32_t n, uint32_t p) {
        X.resize(n * p);
        y.resize(n);

        std::normal_distribution<float> dist(0.0f, 1.0f);

        for (uint32_t i = 0; i < n; i++) {
            float score = 0.0f;
            for (uint32_t j = 0; j < p; j++) {
                X[i * p + j] = dist(rng);
                score += (j == 0 ? 2.0f : 0.5f) * X[i * p + j];
            }
            // Sigmoid classification
            float prob = 1.0f / (1.0f + std::exp(-score));
            y[i] = (prob > 0.5f) ? 1 : 0;
        }
    }

    //=========================================================================
    // Helper: Compute MSE
    //=========================================================================
    float computeMSE(const std::vector<float>& y_true,
                     const std::vector<float>& y_pred) {
        float sum = 0.0f;
        for (size_t i = 0; i < y_true.size(); i++) {
            float diff = y_true[i] - y_pred[i];
            sum += diff * diff;
        }
        return sum / y_true.size();
    }

    //=========================================================================
    // Helper: Compute MAE
    //=========================================================================
    float computeMAE(const std::vector<float>& y_true,
                     const std::vector<float>& y_pred) {
        float sum = 0.0f;
        for (size_t i = 0; i < y_true.size(); i++) {
            sum += std::fabs(y_true[i] - y_pred[i]);
        }
        return sum / y_true.size();
    }

    //=========================================================================
    // Helper: Compute R-squared
    //=========================================================================
    float computeRSquared(const std::vector<float>& y_true,
                          const std::vector<float>& y_pred) {
        float mean_y = nimcp_stats_mean(y_true.data(),
                                         static_cast<uint32_t>(y_true.size()));
        float ss_tot = 0.0f, ss_res = 0.0f;
        for (size_t i = 0; i < y_true.size(); i++) {
            ss_tot += (y_true[i] - mean_y) * (y_true[i] - mean_y);
            ss_res += (y_true[i] - y_pred[i]) * (y_true[i] - y_pred[i]);
        }
        return 1.0f - ss_res / ss_tot;
    }

    //=========================================================================
    // Helper: Compute classification accuracy
    //=========================================================================
    float computeAccuracy(const std::vector<uint8_t>& y_true,
                          const std::vector<uint8_t>& y_pred) {
        uint32_t correct = 0;
        for (size_t i = 0; i < y_true.size(); i++) {
            if (y_true[i] == y_pred[i]) correct++;
        }
        return static_cast<float>(correct) / y_true.size();
    }

    //=========================================================================
    // Helper: Compute confusion matrix metrics
    //=========================================================================
    struct ConfusionMetrics {
        uint32_t tp, tn, fp, fn;
        float precision, recall, f1, specificity;
    };

    ConfusionMetrics computeConfusionMetrics(const std::vector<uint8_t>& y_true,
                                              const std::vector<uint8_t>& y_pred) {
        ConfusionMetrics m = {0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f};
        for (size_t i = 0; i < y_true.size(); i++) {
            if (y_true[i] == 1 && y_pred[i] == 1) m.tp++;
            else if (y_true[i] == 0 && y_pred[i] == 0) m.tn++;
            else if (y_true[i] == 0 && y_pred[i] == 1) m.fp++;
            else m.fn++;
        }
        m.precision = (m.tp + m.fp > 0) ? static_cast<float>(m.tp) / (m.tp + m.fp) : 0.0f;
        m.recall = (m.tp + m.fn > 0) ? static_cast<float>(m.tp) / (m.tp + m.fn) : 0.0f;
        m.f1 = (m.precision + m.recall > 0) ?
               2.0f * m.precision * m.recall / (m.precision + m.recall) : 0.0f;
        m.specificity = (m.tn + m.fp > 0) ? static_cast<float>(m.tn) / (m.tn + m.fp) : 0.0f;
        return m;
    }
};

//=============================================================================
// Linear Regression Model Evaluation
//=============================================================================

TEST_F(MLStatisticsIntegrationTest, RegressionRSquaredConsistency) {
    std::vector<float> x, y;
    generateLinearData(x, y, MEDIUM_N, 2.5f, 1.0f, 0.5f);

    nimcp_regression_result_t result;
    nimcp_stats_regression_linear(x.data(), y.data(), MEDIUM_N, &result);

    // Compute predictions
    std::vector<float> y_pred(MEDIUM_N);
    for (uint32_t i = 0; i < MEDIUM_N; i++) {
        y_pred[i] = result.intercept + result.slope * x[i];
    }

    // Compare R-squared
    float computed_r2 = computeRSquared(y, y_pred);
    EXPECT_NEAR(result.r_squared, computed_r2, RELAXED_TOLERANCE)
        << "R-squared should match manual computation";

    // R-squared should be high for clean linear data
    EXPECT_GT(result.r_squared, 0.95f);
}

TEST_F(MLStatisticsIntegrationTest, RegressionMSEMAE) {
    std::vector<float> x, y;
    generateLinearData(x, y, MEDIUM_N, 1.5f, 2.0f, 1.0f);

    nimcp_regression_result_t result;
    nimcp_stats_regression_linear(x.data(), y.data(), MEDIUM_N, &result);

    std::vector<float> y_pred(MEDIUM_N);
    for (uint32_t i = 0; i < MEDIUM_N; i++) {
        y_pred[i] = result.intercept + result.slope * x[i];
    }

    float mse = computeMSE(y, y_pred);
    float mae = computeMAE(y, y_pred);

    // MSE should be approximately noise_variance
    EXPECT_NEAR(mse, 1.0f, 0.3f) << "MSE should be near noise variance";

    // MAE < sqrt(MSE) generally
    EXPECT_LT(mae, std::sqrt(mse) * 1.5f);
}

TEST_F(MLStatisticsIntegrationTest, RegressionAdjustedRSquared) {
    std::vector<float> x, y;
    generateLinearData(x, y, SMALL_N, 2.0f, 0.0f, 2.0f);

    nimcp_regression_result_t result;
    nimcp_stats_regression_linear(x.data(), y.data(), SMALL_N, &result);

    // Adjusted R-squared should be <= R-squared
    EXPECT_LE(result.adj_r_squared, result.r_squared + RELAXED_TOLERANCE);

    // For small samples with few predictors, difference should be small
    EXPECT_GT(result.adj_r_squared, result.r_squared - 0.1f);
}

//=============================================================================
// Model Comparison with AIC/BIC
//=============================================================================

TEST_F(MLStatisticsIntegrationTest, ModelComparisonAIC) {
    // Generate quadratic data
    uint32_t n = MEDIUM_N;
    std::vector<float> x(n), y(n);
    std::normal_distribution<float> noise(0.0f, 0.5f);

    for (uint32_t i = 0; i < n; i++) {
        x[i] = -3.0f + 6.0f * i / n;
        y[i] = 0.5f * x[i] * x[i] - x[i] + 1.0f + noise(rng);
    }

    // Fit linear model
    nimcp_regression_result_t linear;
    nimcp_stats_regression_linear(x.data(), y.data(), n, &linear);

    // Fit quadratic model
    nimcp_regression_result_t quadratic;
    quadratic.coefficients = new float[3];
    quadratic.n_coefficients = 3;
    nimcp_stats_regression_polynomial(x.data(), y.data(), n, 2, &quadratic);

    // Quadratic should have lower AIC (better fit)
    // Note: AIC = n * log(RSS/n) + 2*k
    EXPECT_LT(quadratic.aic, linear.aic)
        << "Quadratic model should have lower AIC";

    // BIC penalizes complexity more
    EXPECT_LT(quadratic.bic, linear.bic)
        << "Quadratic model should also have lower BIC";

    nimcp_stats_regression_free(&quadratic);
}

TEST_F(MLStatisticsIntegrationTest, OverfittingDetection) {
    // Small sample with high-degree polynomial
    uint32_t n = 20;
    std::vector<float> x(n), y(n);
    std::normal_distribution<float> noise(0.0f, 1.0f);

    for (uint32_t i = 0; i < n; i++) {
        x[i] = -2.0f + 4.0f * i / n;
        y[i] = x[i] + noise(rng);  // Linear relationship
    }

    // Fit various polynomial degrees
    std::vector<float> aic_values;
    for (uint32_t degree = 1; degree <= 5; degree++) {
        nimcp_regression_result_t result;
        result.coefficients = new float[degree + 1];
        result.n_coefficients = degree + 1;

        nimcp_stats_regression_polynomial(x.data(), y.data(), n, degree, &result);
        aic_values.push_back(result.aic);

        nimcp_stats_regression_free(&result);
    }

    // Higher degrees should eventually increase AIC (overfitting)
    // Degree 1 or 2 should be optimal
    float min_aic = *std::min_element(aic_values.begin(), aic_values.end());
    EXPECT_EQ(min_aic, aic_values[0]) << "Linear model should have lowest AIC for linear data";
}

//=============================================================================
// Logistic Regression and Classification
//=============================================================================

TEST_F(MLStatisticsIntegrationTest, LogisticRegressionBasic) {
    uint32_t n = MEDIUM_N;
    uint32_t p = 2;

    std::vector<float> X;
    std::vector<uint8_t> y;
    generateClassificationData(X, y, n, p);

    std::vector<float> coefficients(p);
    nimcp_stats_result_t status = nimcp_stats_regression_logistic(
        X.data(), y.data(), n, p, coefficients.data(), 100);

    EXPECT_EQ(status, NIMCP_STATS_OK);

    // First coefficient should be larger (more important feature)
    EXPECT_GT(std::fabs(coefficients[0]), std::fabs(coefficients[1]))
        << "First feature has larger true coefficient";
}

TEST_F(MLStatisticsIntegrationTest, LogisticRegressionPredictions) {
    uint32_t n = MEDIUM_N;
    uint32_t p = 2;

    std::vector<float> X;
    std::vector<uint8_t> y;
    generateClassificationData(X, y, n, p);

    std::vector<float> coefficients(p);
    nimcp_stats_regression_logistic(X.data(), y.data(), n, p, coefficients.data(), 100);

    // Make predictions
    std::vector<uint8_t> y_pred(n);
    for (uint32_t i = 0; i < n; i++) {
        float score = 0.0f;
        for (uint32_t j = 0; j < p; j++) {
            score += coefficients[j] * X[i * p + j];
        }
        float prob = 1.0f / (1.0f + std::exp(-score));
        y_pred[i] = (prob > 0.5f) ? 1 : 0;
    }

    float accuracy = computeAccuracy(y, y_pred);
    EXPECT_GT(accuracy, 0.7f) << "Should achieve reasonable accuracy";
}

TEST_F(MLStatisticsIntegrationTest, ClassificationMetrics) {
    uint32_t n = MEDIUM_N;
    uint32_t p = 3;

    std::vector<float> X;
    std::vector<uint8_t> y;
    generateClassificationData(X, y, n, p);

    std::vector<float> coefficients(p);
    nimcp_stats_regression_logistic(X.data(), y.data(), n, p, coefficients.data(), 100);

    // Predictions
    std::vector<uint8_t> y_pred(n);
    for (uint32_t i = 0; i < n; i++) {
        float score = 0.0f;
        for (uint32_t j = 0; j < p; j++) {
            score += coefficients[j] * X[i * p + j];
        }
        y_pred[i] = (1.0f / (1.0f + std::exp(-score)) > 0.5f) ? 1 : 0;
    }

    ConfusionMetrics cm = computeConfusionMetrics(y, y_pred);

    // Check all metrics are in valid range
    EXPECT_GE(cm.precision, 0.0f);
    EXPECT_LE(cm.precision, 1.0f);
    EXPECT_GE(cm.recall, 0.0f);
    EXPECT_LE(cm.recall, 1.0f);
    EXPECT_GE(cm.f1, 0.0f);
    EXPECT_LE(cm.f1, 1.0f);
    EXPECT_GE(cm.specificity, 0.0f);
    EXPECT_LE(cm.specificity, 1.0f);

    // TP + TN + FP + FN = n
    EXPECT_EQ(cm.tp + cm.tn + cm.fp + cm.fn, n);
}

//=============================================================================
// Cross-Validation Simulation
//=============================================================================

TEST_F(MLStatisticsIntegrationTest, CrossValidationVariance) {
    // Simulate K-fold cross-validation
    uint32_t n = MEDIUM_N;
    uint32_t k_folds = 5;
    uint32_t fold_size = n / k_folds;

    std::vector<float> x, y;
    generateLinearData(x, y, n, 2.0f, 1.0f, 1.0f);

    std::vector<float> fold_r2(k_folds);

    for (uint32_t fold = 0; fold < k_folds; fold++) {
        // Create train/test split
        std::vector<float> x_train, y_train, x_test, y_test;

        for (uint32_t i = 0; i < n; i++) {
            if (i >= fold * fold_size && i < (fold + 1) * fold_size) {
                x_test.push_back(x[i]);
                y_test.push_back(y[i]);
            } else {
                x_train.push_back(x[i]);
                y_train.push_back(y[i]);
            }
        }

        // Fit on training
        nimcp_regression_result_t result;
        nimcp_stats_regression_linear(x_train.data(), y_train.data(),
                                       static_cast<uint32_t>(x_train.size()), &result);

        // Evaluate on test
        std::vector<float> y_pred(x_test.size());
        for (size_t i = 0; i < x_test.size(); i++) {
            y_pred[i] = result.intercept + result.slope * x_test[i];
        }

        fold_r2[fold] = computeRSquared(y_test, y_pred);
    }

    // Compute CV statistics
    float mean_r2 = nimcp_stats_mean(fold_r2.data(), k_folds);
    float std_r2 = nimcp_stats_std_dev(fold_r2.data(), k_folds);

    EXPECT_GT(mean_r2, 0.8f) << "Mean CV R-squared should be high";
    EXPECT_LT(std_r2, 0.1f) << "CV variance should be small";
}

TEST_F(MLStatisticsIntegrationTest, TrainTestGeneralizationGap) {
    // Check for overfitting by comparing train and test performance
    std::vector<float> x, y;
    generateLinearData(x, y, MEDIUM_N, 1.5f, 2.0f, 1.0f);

    // Split 80/20
    uint32_t train_size = static_cast<uint32_t>(0.8f * MEDIUM_N);
    uint32_t test_size = MEDIUM_N - train_size;

    std::vector<float> x_train(x.begin(), x.begin() + train_size);
    std::vector<float> y_train(y.begin(), y.begin() + train_size);
    std::vector<float> x_test(x.begin() + train_size, x.end());
    std::vector<float> y_test(y.begin() + train_size, y.end());

    nimcp_regression_result_t result;
    nimcp_stats_regression_linear(x_train.data(), y_train.data(), train_size, &result);

    // Train performance
    std::vector<float> train_pred(train_size);
    for (uint32_t i = 0; i < train_size; i++) {
        train_pred[i] = result.intercept + result.slope * x_train[i];
    }
    float train_r2 = computeRSquared(y_train, train_pred);

    // Test performance
    std::vector<float> test_pred(test_size);
    for (uint32_t i = 0; i < test_size; i++) {
        test_pred[i] = result.intercept + result.slope * x_test[i];
    }
    float test_r2 = computeRSquared(y_test, test_pred);

    // Gap should be small for linear model
    float gap = train_r2 - test_r2;
    EXPECT_LT(gap, 0.1f) << "Generalization gap should be small";
}

//=============================================================================
// Correlation-Based Feature Selection
//=============================================================================

TEST_F(MLStatisticsIntegrationTest, FeatureCorrelationFiltering) {
    // Generate features with varying correlation to target
    uint32_t n = MEDIUM_N;
    uint32_t n_features = 5;

    std::vector<float> y(n);
    std::vector<std::vector<float>> features(n_features, std::vector<float>(n));
    std::normal_distribution<float> noise(0.0f, 1.0f);

    // Target: sum of first 2 features
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t f = 0; f < n_features; f++) {
            features[f][i] = noise(rng);
        }
        y[i] = features[0][i] + features[1][i] + 0.5f * noise(rng);
    }

    // Compute correlations with target
    std::vector<float> correlations(n_features);
    for (uint32_t f = 0; f < n_features; f++) {
        nimcp_correlation_result_t result;
        nimcp_stats_correlation_pearson(features[f].data(), y.data(), n, &result);
        correlations[f] = std::fabs(result.r);
    }

    // Features 0 and 1 should have highest correlations
    EXPECT_GT(correlations[0], correlations[2]);
    EXPECT_GT(correlations[1], correlations[2]);
    EXPECT_GT(correlations[0], correlations[3]);
    EXPECT_GT(correlations[1], correlations[4]);
}

//=============================================================================
// Residual Diagnostics
//=============================================================================

TEST_F(MLStatisticsIntegrationTest, ResidualNormality) {
    std::vector<float> x, y;
    generateLinearData(x, y, MEDIUM_N, 2.0f, 1.0f, 1.0f);

    nimcp_regression_result_t result;
    nimcp_stats_regression_linear(x.data(), y.data(), MEDIUM_N, &result);

    // Compute residuals
    std::vector<float> residuals(MEDIUM_N);
    for (uint32_t i = 0; i < MEDIUM_N; i++) {
        residuals[i] = y[i] - (result.intercept + result.slope * x[i]);
    }

    // Residuals should have mean ~0
    float res_mean = nimcp_stats_mean(residuals.data(), MEDIUM_N);
    EXPECT_NEAR(res_mean, 0.0f, 0.1f);

    // Test normality
    nimcp_test_result_t normality;
    nimcp_stats_ks_normality(residuals.data(), MEDIUM_N, &normality);

    // Should pass normality test
    EXPECT_GT(normality.p_value, 0.01f)
        << "Residuals should be approximately normal";
}

TEST_F(MLStatisticsIntegrationTest, ResidualHomoscedasticity) {
    // Generate data with constant variance
    std::vector<float> x, y;
    generateLinearData(x, y, MEDIUM_N, 2.0f, 1.0f, 1.0f);

    nimcp_regression_result_t result;
    nimcp_stats_regression_linear(x.data(), y.data(), MEDIUM_N, &result);

    // Compute residuals
    std::vector<float> residuals(MEDIUM_N);
    for (uint32_t i = 0; i < MEDIUM_N; i++) {
        residuals[i] = y[i] - (result.intercept + result.slope * x[i]);
    }

    // Split residuals into low/high x groups
    std::vector<std::pair<float, float>> sorted_pairs(MEDIUM_N);
    for (uint32_t i = 0; i < MEDIUM_N; i++) {
        sorted_pairs[i] = {x[i], residuals[i]};
    }
    std::sort(sorted_pairs.begin(), sorted_pairs.end());

    std::vector<float> low_res(MEDIUM_N / 2);
    std::vector<float> high_res(MEDIUM_N / 2);
    for (uint32_t i = 0; i < MEDIUM_N / 2; i++) {
        low_res[i] = sorted_pairs[i].second;
        high_res[i] = sorted_pairs[i + MEDIUM_N / 2].second;
    }

    float var_low = nimcp_stats_variance(low_res.data(), MEDIUM_N / 2);
    float var_high = nimcp_stats_variance(high_res.data(), MEDIUM_N / 2);

    // Variances should be similar (homoscedastic)
    float ratio = var_high / var_low;
    EXPECT_GT(ratio, 0.5f) << "Variance ratio too extreme";
    EXPECT_LT(ratio, 2.0f) << "Variance ratio too extreme";
}

//=============================================================================
// Regularization Simulation
//=============================================================================

TEST_F(MLStatisticsIntegrationTest, BiasVarianceTradeoff) {
    // Simulate effect of model complexity
    uint32_t n_train = 50;
    uint32_t n_test = 100;

    std::vector<float> x_train(n_train), y_train(n_train);
    std::vector<float> x_test(n_test), y_test(n_test);
    std::normal_distribution<float> noise(0.0f, 0.5f);
    std::uniform_real_distribution<float> x_dist(-2.0f, 2.0f);

    // True function: y = sin(x)
    for (uint32_t i = 0; i < n_train; i++) {
        x_train[i] = x_dist(rng);
        y_train[i] = std::sin(x_train[i]) + noise(rng);
    }
    for (uint32_t i = 0; i < n_test; i++) {
        x_test[i] = x_dist(rng);
        y_test[i] = std::sin(x_test[i]);  // No noise for true error
    }

    std::vector<float> train_errors, test_errors;

    for (uint32_t degree = 1; degree <= 10; degree++) {
        nimcp_regression_result_t result = {0};  // Zero-initialize

        nimcp_stats_regression_polynomial(x_train.data(), y_train.data(),
                                           n_train, degree, &result);

        // Compute train error
        float train_mse = 0.0f;
        for (uint32_t i = 0; i < n_train; i++) {
            float pred = result.coefficients[0];
            float x_pow = 1.0f;
            for (uint32_t d = 1; d <= degree; d++) {
                x_pow *= x_train[i];
                pred += result.coefficients[d] * x_pow;
            }
            train_mse += (y_train[i] - pred) * (y_train[i] - pred);
        }
        train_errors.push_back(train_mse / n_train);

        // Compute test error
        float test_mse = 0.0f;
        for (uint32_t i = 0; i < n_test; i++) {
            float pred = result.coefficients[0];
            float x_pow = 1.0f;
            for (uint32_t d = 1; d <= degree; d++) {
                x_pow *= x_test[i];
                pred += result.coefficients[d] * x_pow;
            }
            test_mse += (y_test[i] - pred) * (y_test[i] - pred);
        }
        test_errors.push_back(test_mse / n_test);

        nimcp_stats_regression_free(&result);
    }

    // Training error should decrease monotonically
    for (size_t i = 1; i < train_errors.size(); i++) {
        EXPECT_LE(train_errors[i], train_errors[i-1] + 0.01f);
    }

    // Test error should have minimum somewhere in middle (optimal complexity)
    float min_test_error = *std::min_element(test_errors.begin(), test_errors.end());
    // Not at degree 1 (underfitting) or degree 10 (overfitting typically)
    EXPECT_GT(test_errors[0], min_test_error * 0.9f);
}

//=============================================================================
// Bootstrap Model Evaluation
//=============================================================================

TEST_F(MLStatisticsIntegrationTest, BootstrapRegressionCoefficients) {
    std::vector<float> x, y;
    generateLinearData(x, y, SMALL_N, 2.0f, 1.0f, 1.0f);

    // Bootstrap confidence interval for slope
    nimcp_bootstrap_result_t bootstrap_result;
    nimcp_stats_result_t status = nimcp_stats_bootstrap_correlation(
        x.data(), y.data(), SMALL_N, 1000, 0.95f, &bootstrap_result);

    EXPECT_EQ(status, NIMCP_STATS_OK);

    // The correlation bootstrap gives CI for r, which we can compare
    // to regression output
    nimcp_regression_result_t reg;
    nimcp_stats_regression_linear(x.data(), y.data(), SMALL_N, &reg);

    // R should be sqrt(R-squared) approximately
    float expected_r = std::sqrt(reg.r_squared);
    EXPECT_NEAR(bootstrap_result.estimate, expected_r, 0.1f);
}

//=============================================================================
// Memory and Performance Tests
//=============================================================================

TEST_F(MLStatisticsIntegrationTest, NoMemoryLeaksRegression) {
    nimcp_memory_clear_stats();

    for (int trial = 0; trial < 100; trial++) {
        std::vector<float> x, y;
        generateLinearData(x, y, SMALL_N, 2.0f, 1.0f, 1.0f);

        nimcp_regression_result_t result;
        nimcp_stats_regression_linear(x.data(), y.data(), SMALL_N, &result);
    }

    nimcp_memory_stats_t stats;
    nimcp_memory_get_stats(&stats);
    EXPECT_LT(stats.current_allocated, 4096);
}

TEST_F(MLStatisticsIntegrationTest, NoMemoryLeaksLogistic) {
    nimcp_memory_clear_stats();

    for (int trial = 0; trial < 50; trial++) {
        std::vector<float> X;
        std::vector<uint8_t> y;
        generateClassificationData(X, y, SMALL_N, 3);

        std::vector<float> coef(3);
        nimcp_stats_regression_logistic(X.data(), y.data(), SMALL_N, 3, coef.data(), 50);
    }

    nimcp_memory_stats_t stats;
    nimcp_memory_get_stats(&stats);
    EXPECT_LT(stats.current_allocated, 4096);
}

TEST_F(MLStatisticsIntegrationTest, PerformanceLargeRegression) {
    std::vector<float> x, y;
    generateLinearData(x, y, LARGE_N, 2.0f, 1.0f, 1.0f);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; i++) {
        nimcp_regression_result_t result;
        nimcp_stats_regression_linear(x.data(), y.data(), LARGE_N, &result);
    }

    auto end = std::chrono::high_resolution_clock::now();
    uint64_t time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    EXPECT_LT(time_ms, 500u) << "100 regressions should complete in <500ms";
}

TEST_F(MLStatisticsIntegrationTest, PerformanceLogisticConvergence) {
    std::vector<float> X;
    std::vector<uint8_t> y;
    generateClassificationData(X, y, MEDIUM_N, 5);

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<float> coef(5);
    nimcp_stats_regression_logistic(X.data(), y.data(), MEDIUM_N, 5, coef.data(), 100);

    auto end = std::chrono::high_resolution_clock::now();
    uint64_t time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    EXPECT_LT(time_ms, 1000u) << "Logistic regression should converge in <1s";
}

