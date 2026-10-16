//=============================================================================
// test_ml_statistics_e2e.cpp - Machine Learning Statistics E2E Tests
//=============================================================================
/**
 * @file test_ml_statistics_e2e.cpp
 * @brief End-to-end tests for ML-focused statistical workflows
 *
 * WHAT: Complete ML pipelines using statistical primitives
 * WHY:  Verify statistics module handles ML-specific scenarios
 * HOW:  Cross-validation, model selection, feature analysis, evaluation metrics
 *
 * TEST SCENARIOS:
 * 1. Cross-validation framework for model evaluation
 * 2. Bootstrap model confidence intervals
 * 3. Feature importance via statistical tests
 * 4. Hyperparameter selection with statistical validation
 * 5. Model comparison using hypothesis tests
 * 6. Learning curve analysis
 * 7. Bias-variance decomposition estimation
 * 8. Confusion matrix derived statistics
 * 9. ROC/AUC analysis
 * 10. Calibration curve analysis
 * 11. Ensemble model statistics
 * 12. Regularization path analysis
 * 13. Feature selection via mutual information
 * 14. Class imbalance metrics
 * 15. Multi-model statistical comparison
 *
 * @date 2026-01-30
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <random>
#include <algorithm>
#include <numeric>
#include <chrono>

extern "C" {
#include "utils/statistics/nimcp_statistics.h"
}

// Test tolerances
#define TOLERANCE 1e-5f
#define LOOSE_TOLERANCE 1e-2f
#define VERY_LOOSE_TOLERANCE 0.1f

// Timing macros
#define START_TIMER() auto _start_time = std::chrono::high_resolution_clock::now()
#define STOP_TIMER_MS() std::chrono::duration<double, std::milli>( \
    std::chrono::high_resolution_clock::now() - _start_time).count()

//=============================================================================
// Test Fixture
//=============================================================================

class MLStatisticsE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        config = nimcp_stats_default_config();
        ASSERT_EQ(nimcp_stats_init(&config), NIMCP_STATS_OK);
        rng.seed(42);  // Reproducible tests
    }

    void TearDown() override {
        nimcp_stats_shutdown();
    }

    nimcp_stats_config_t config;
    std::mt19937 rng;

    // Generate classification data
    struct ClassificationData {
        std::vector<float> features;  // n_samples x n_features (row-major)
        std::vector<uint8_t> labels;
        size_t n_samples;
        size_t n_features;
    };

    ClassificationData generate_binary_classification(
        size_t n_samples, size_t n_features, float separation = 2.0f) {
        ClassificationData data;
        data.n_samples = n_samples;
        data.n_features = n_features;
        data.features.resize(n_samples * n_features);
        data.labels.resize(n_samples);

        std::normal_distribution<float> noise(0.0f, 1.0f);

        for (size_t i = 0; i < n_samples; i++) {
            data.labels[i] = (i < n_samples / 2) ? 0 : 1;
            float class_offset = data.labels[i] * separation;
            for (size_t j = 0; j < n_features; j++) {
                data.features[i * n_features + j] = class_offset + noise(rng);
            }
        }

        // Shuffle data
        std::vector<size_t> indices(n_samples);
        std::iota(indices.begin(), indices.end(), 0);
        std::shuffle(indices.begin(), indices.end(), rng);

        std::vector<float> shuffled_features(n_samples * n_features);
        std::vector<uint8_t> shuffled_labels(n_samples);
        for (size_t i = 0; i < n_samples; i++) {
            shuffled_labels[i] = data.labels[indices[i]];
            for (size_t j = 0; j < n_features; j++) {
                shuffled_features[i * n_features + j] =
                    data.features[indices[i] * n_features + j];
            }
        }
        data.features = shuffled_features;
        data.labels = shuffled_labels;

        return data;
    }

    // Generate regression data
    struct RegressionData {
        std::vector<float> X;  // n_samples x n_features
        std::vector<float> y;
        std::vector<float> true_coeffs;
        size_t n_samples;
        size_t n_features;
    };

    RegressionData generate_regression(size_t n_samples, size_t n_features,
                                       float noise_std = 0.5f) {
        RegressionData data;
        data.n_samples = n_samples;
        data.n_features = n_features;
        data.X.resize(n_samples * n_features);
        data.y.resize(n_samples);
        data.true_coeffs.resize(n_features);

        std::normal_distribution<float> feature_dist(0.0f, 1.0f);
        std::normal_distribution<float> noise_dist(0.0f, noise_std);
        std::uniform_real_distribution<float> coeff_dist(-2.0f, 2.0f);

        // Generate true coefficients
        for (size_t j = 0; j < n_features; j++) {
            data.true_coeffs[j] = coeff_dist(rng);
        }

        // Generate features and targets
        for (size_t i = 0; i < n_samples; i++) {
            data.y[i] = noise_dist(rng);  // intercept = 0
            for (size_t j = 0; j < n_features; j++) {
                data.X[i * n_features + j] = feature_dist(rng);
                data.y[i] += data.true_coeffs[j] * data.X[i * n_features + j];
            }
        }

        return data;
    }

    // Simple logistic prediction
    float logistic_predict(const float* x, const float* coeffs, size_t n_features) {
        float logit = 0.0f;
        for (size_t j = 0; j < n_features; j++) {
            logit += coeffs[j] * x[j];
        }
        return 1.0f / (1.0f + std::exp(-logit));
    }

    // Compute accuracy
    float compute_accuracy(const std::vector<uint8_t>& true_labels,
                          const std::vector<uint8_t>& pred_labels) {
        size_t correct = 0;
        for (size_t i = 0; i < true_labels.size(); i++) {
            if (true_labels[i] == pred_labels[i]) correct++;
        }
        return static_cast<float>(correct) / true_labels.size();
    }

    // Generate normal samples
    std::vector<float> generate_normal(size_t n, float mean, float stddev) {
        std::normal_distribution<float> dist(mean, stddev);
        std::vector<float> data(n);
        for (auto& x : data) x = dist(rng);
        return data;
    }

    // Generate uniform samples
    std::vector<float> generate_uniform(size_t n, float low, float high) {
        std::uniform_real_distribution<float> dist(low, high);
        std::vector<float> data(n);
        for (auto& x : data) x = dist(rng);
        return data;
    }
};

//=============================================================================
// E2E Test 1: Cross-Validation Framework
//=============================================================================

TEST_F(MLStatisticsE2ETest, CrossValidationFramework) {
    START_TIMER();

    // Generate dataset
    const size_t n_samples = 1000;
    const size_t n_features = 10;
    const size_t k_folds = 5;
    auto data = generate_binary_classification(n_samples, n_features, 2.0f);

    // Perform k-fold cross-validation
    std::vector<float> fold_accuracies(k_folds);
    size_t fold_size = n_samples / k_folds;

    for (size_t fold = 0; fold < k_folds; fold++) {
        // Split into train/test
        std::vector<float> train_X, test_X;
        std::vector<uint8_t> train_y, test_y;

        for (size_t i = 0; i < n_samples; i++) {
            bool is_test = (i >= fold * fold_size && i < (fold + 1) * fold_size);
            if (is_test) {
                for (size_t j = 0; j < n_features; j++) {
                    test_X.push_back(data.features[i * n_features + j]);
                }
                test_y.push_back(data.labels[i]);
            } else {
                for (size_t j = 0; j < n_features; j++) {
                    train_X.push_back(data.features[i * n_features + j]);
                }
                train_y.push_back(data.labels[i]);
            }
        }

        // Simple threshold classifier using feature means
        std::vector<float> class0_means(n_features, 0.0f);
        std::vector<float> class1_means(n_features, 0.0f);
        size_t class0_count = 0, class1_count = 0;

        size_t train_samples = train_y.size();
        for (size_t i = 0; i < train_samples; i++) {
            if (train_y[i] == 0) {
                for (size_t j = 0; j < n_features; j++) {
                    class0_means[j] += train_X[i * n_features + j];
                }
                class0_count++;
            } else {
                for (size_t j = 0; j < n_features; j++) {
                    class1_means[j] += train_X[i * n_features + j];
                }
                class1_count++;
            }
        }

        for (size_t j = 0; j < n_features; j++) {
            class0_means[j] /= class0_count;
            class1_means[j] /= class1_count;
        }

        // Predict on test set using distance to class means
        std::vector<uint8_t> predictions(test_y.size());
        for (size_t i = 0; i < test_y.size(); i++) {
            float dist0 = 0.0f, dist1 = 0.0f;
            for (size_t j = 0; j < n_features; j++) {
                float diff0 = test_X[i * n_features + j] - class0_means[j];
                float diff1 = test_X[i * n_features + j] - class1_means[j];
                dist0 += diff0 * diff0;
                dist1 += diff1 * diff1;
            }
            predictions[i] = (dist0 < dist1) ? 0 : 1;
        }

        fold_accuracies[fold] = compute_accuracy(test_y, predictions);
    }

    // Compute CV statistics
    nimcp_descriptive_stats_t cv_stats;
    ASSERT_EQ(nimcp_stats_describe(fold_accuracies.data(), k_folds, &cv_stats),
              NIMCP_STATS_OK);

    // Verify reasonable accuracy and low variance across folds
    EXPECT_GT(cv_stats.mean, 0.7f) << "Mean CV accuracy should be > 70%";
    EXPECT_LT(cv_stats.std_dev, 0.1f) << "CV std dev should be < 10%";

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 60000.0) << "Test should complete in under 60 seconds";
    std::cout << "Cross-validation completed in " << elapsed << " ms\n";
    std::cout << "Mean accuracy: " << cv_stats.mean << " +/- " << cv_stats.std_dev << "\n";
}

//=============================================================================
// E2E Test 2: Bootstrap Model Confidence Intervals
//=============================================================================

TEST_F(MLStatisticsE2ETest, BootstrapModelConfidence) {
    START_TIMER();

    // Generate regression data
    const size_t n_samples = 500;
    auto data = generate_regression(n_samples, 1, 1.0f);

    // Fit simple linear regression
    nimcp_regression_result_t reg_result;
    ASSERT_EQ(nimcp_stats_regression_linear(data.X.data(), data.y.data(),
                                            n_samples, &reg_result),
              NIMCP_STATS_OK);

    // Bootstrap for slope confidence interval
    nimcp_bootstrap_result_t bootstrap_result;
    const uint32_t n_replicates = 1000;

    // Create paired data for bootstrap
    std::vector<float> slopes(n_replicates);
    for (uint32_t b = 0; b < n_replicates; b++) {
        // Resample with replacement
        std::vector<float> boot_x(n_samples);
        std::vector<float> boot_y(n_samples);
        std::uniform_int_distribution<size_t> idx_dist(0, n_samples - 1);

        for (size_t i = 0; i < n_samples; i++) {
            size_t idx = idx_dist(rng);
            boot_x[i] = data.X[idx];
            boot_y[i] = data.y[idx];
        }

        nimcp_regression_result_t boot_reg;
        if (nimcp_stats_regression_linear(boot_x.data(), boot_y.data(),
                                          n_samples, &boot_reg) == NIMCP_STATS_OK) {
            slopes[b] = boot_reg.slope;
        }
    }

    // Compute bootstrap statistics
    nimcp_descriptive_stats_t slope_stats;
    ASSERT_EQ(nimcp_stats_describe(slopes.data(), n_replicates, &slope_stats),
              NIMCP_STATS_OK);

    // Compute percentile CI
    float ci_lower = nimcp_stats_quantile(slopes.data(), n_replicates, 0.025f);
    float ci_upper = nimcp_stats_quantile(slopes.data(), n_replicates, 0.975f);

    // True slope should be within CI
    EXPECT_GT(data.true_coeffs[0], ci_lower - 0.5f);
    EXPECT_LT(data.true_coeffs[0], ci_upper + 0.5f);

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 60000.0);
    std::cout << "Bootstrap CI completed in " << elapsed << " ms\n";
    std::cout << "True slope: " << data.true_coeffs[0] << ", CI: ["
              << ci_lower << ", " << ci_upper << "]\n";
}

//=============================================================================
// E2E Test 3: Feature Importance via Statistical Tests
//=============================================================================

TEST_F(MLStatisticsE2ETest, FeatureImportanceStatisticalTests) {
    START_TIMER();

    // Generate data with varying feature importance
    const size_t n_samples = 500;
    const size_t n_features = 10;
    const size_t n_important = 3;  // Only first 3 features are important

    auto data = generate_binary_classification(n_samples, n_features, 3.0f);

    // Add noise features (last n_features - n_important are pure noise)
    std::normal_distribution<float> noise_dist(0.0f, 1.0f);
    for (size_t i = 0; i < n_samples; i++) {
        for (size_t j = n_important; j < n_features; j++) {
            data.features[i * n_features + j] = noise_dist(rng);
        }
    }

    // Compute t-test for each feature between classes
    std::vector<float> feature_pvalues(n_features);
    std::vector<float> feature_effect_sizes(n_features);

    for (size_t j = 0; j < n_features; j++) {
        std::vector<float> class0_vals, class1_vals;
        for (size_t i = 0; i < n_samples; i++) {
            if (data.labels[i] == 0) {
                class0_vals.push_back(data.features[i * n_features + j]);
            } else {
                class1_vals.push_back(data.features[i * n_features + j]);
            }
        }

        nimcp_test_result_t t_result;
        ASSERT_EQ(nimcp_stats_ttest_two_sample(
            class0_vals.data(), class0_vals.size(),
            class1_vals.data(), class1_vals.size(),
            false,  // Welch's t-test
            NIMCP_TEST_TWO_SIDED,
            0.95f,
            &t_result), NIMCP_STATS_OK);

        feature_pvalues[j] = t_result.p_value;
        feature_effect_sizes[j] = std::abs(t_result.effect_size);
    }

    // Important features should have low p-values
    for (size_t j = 0; j < n_important; j++) {
        EXPECT_LT(feature_pvalues[j], 0.05f)
            << "Important feature " << j << " should be significant";
    }

    // Some noise features should have high p-values
    int non_significant = 0;
    for (size_t j = n_important; j < n_features; j++) {
        if (feature_pvalues[j] > 0.05f) non_significant++;
    }
    EXPECT_GT(non_significant, 0) << "Some noise features should be non-significant";

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 60000.0);
    std::cout << "Feature importance test completed in " << elapsed << " ms\n";
}

//=============================================================================
// E2E Test 4: Hyperparameter Selection with Statistical Validation
//=============================================================================

TEST_F(MLStatisticsE2ETest, HyperparameterStatisticalValidation) {
    START_TIMER();

    // Simulate grid search with different regularization values
    const size_t n_configs = 5;
    const size_t n_trials = 10;  // Repeated trials per config

    // Simulated accuracy for different lambda values
    std::vector<std::vector<float>> trial_accuracies(n_configs);
    std::vector<float> config_means(n_configs);
    std::vector<float> config_stds(n_configs);

    // Simulate: optimal config is index 2
    float base_accuracies[] = {0.75f, 0.80f, 0.85f, 0.82f, 0.78f};

    for (size_t c = 0; c < n_configs; c++) {
        trial_accuracies[c].resize(n_trials);
        std::normal_distribution<float> acc_dist(base_accuracies[c], 0.03f);
        for (size_t t = 0; t < n_trials; t++) {
            trial_accuracies[c][t] = std::min(1.0f, std::max(0.0f, acc_dist(rng)));
        }

        nimcp_descriptive_stats_t stats;
        nimcp_stats_describe(trial_accuracies[c].data(), n_trials, &stats);
        config_means[c] = stats.mean;
        config_stds[c] = stats.std_dev;
    }

    // Find best config
    size_t best_config = 0;
    for (size_t c = 1; c < n_configs; c++) {
        if (config_means[c] > config_means[best_config]) {
            best_config = c;
        }
    }

    // Test if best is significantly better than second best
    size_t second_best = (best_config == 0) ? 1 : 0;
    for (size_t c = 0; c < n_configs; c++) {
        if (c != best_config && config_means[c] > config_means[second_best]) {
            second_best = c;
        }
    }

    nimcp_test_result_t comparison;
    ASSERT_EQ(nimcp_stats_ttest_two_sample(
        trial_accuracies[best_config].data(), n_trials,
        trial_accuracies[second_best].data(), n_trials,
        false, NIMCP_TEST_RIGHT_TAIL, 0.95f, &comparison),
        NIMCP_STATS_OK);

    EXPECT_EQ(best_config, 2u) << "Config 2 should have highest mean accuracy";

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 60000.0);
    std::cout << "Hyperparameter selection completed in " << elapsed << " ms\n";
    std::cout << "Best config: " << best_config << " with mean accuracy "
              << config_means[best_config] << "\n";
}

//=============================================================================
// E2E Test 5: Model Comparison Using Hypothesis Tests
//=============================================================================

TEST_F(MLStatisticsE2ETest, ModelComparisonHypothesisTests) {
    START_TIMER();

    // Simulate results from 3 different models on same cross-validation folds
    const size_t k_folds = 10;
    const size_t n_models = 3;

    std::vector<std::vector<float>> model_accuracies(n_models);
    float model_base_acc[] = {0.82f, 0.85f, 0.84f};  // Model 1 is best

    for (size_t m = 0; m < n_models; m++) {
        model_accuracies[m].resize(k_folds);
        std::normal_distribution<float> acc_dist(model_base_acc[m], 0.02f);
        for (size_t k = 0; k < k_folds; k++) {
            model_accuracies[m][k] = acc_dist(rng);
        }
    }

    // Paired t-test: Model 1 vs Model 0
    nimcp_test_result_t paired_result;
    ASSERT_EQ(nimcp_stats_ttest_paired(
        model_accuracies[1].data(),
        model_accuracies[0].data(),
        k_folds,
        NIMCP_TEST_TWO_SIDED,
        0.95f,
        &paired_result), NIMCP_STATS_OK);

    // Model 1 should be significantly better than Model 0
    EXPECT_LT(paired_result.p_value, 0.10f)
        << "Model 1 should be significantly better than Model 0";

    // ANOVA-style comparison (using one-way ANOVA)
    const float* groups[] = {
        model_accuracies[0].data(),
        model_accuracies[1].data(),
        model_accuracies[2].data()
    };
    uint32_t sizes[] = {(uint32_t)k_folds, (uint32_t)k_folds, (uint32_t)k_folds};

    nimcp_anova_result_t anova_result;
    ASSERT_EQ(nimcp_stats_anova_one_way(groups, sizes, n_models, 0.05f, &anova_result),
              NIMCP_STATS_OK);

    // Should detect difference between models
    std::cout << "ANOVA F-statistic: " << anova_result.f_statistic
              << ", p-value: " << anova_result.p_value << "\n";

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 60000.0);
    std::cout << "Model comparison completed in " << elapsed << " ms\n";
}

//=============================================================================
// E2E Test 6: Learning Curve Analysis
//=============================================================================

TEST_F(MLStatisticsE2ETest, LearningCurveAnalysis) {
    START_TIMER();

    // Simulate learning curves for training and validation
    std::vector<size_t> train_sizes = {100, 200, 500, 1000, 2000, 5000};
    std::vector<float> train_scores;
    std::vector<float> val_scores;

    // Typical learning curve pattern: train decreases, val increases
    for (size_t size : train_sizes) {
        // Training score typically high and decreasing
        float train_base = 0.95f - 0.05f * std::log10((float)size / 100.0f);
        std::normal_distribution<float> train_dist(train_base, 0.01f);
        train_scores.push_back(train_dist(rng));

        // Validation score typically lower but increasing
        float val_base = 0.70f + 0.08f * std::log10((float)size / 100.0f);
        std::normal_distribution<float> val_dist(val_base, 0.02f);
        val_scores.push_back(val_dist(rng));
    }

    // Analyze gap (overfitting measure)
    std::vector<float> gaps(train_sizes.size());
    for (size_t i = 0; i < train_sizes.size(); i++) {
        gaps[i] = train_scores[i] - val_scores[i];
    }

    // Gap should decrease with more training data
    nimcp_correlation_result_t gap_trend;
    std::vector<float> sizes_float(train_sizes.begin(), train_sizes.end());
    ASSERT_EQ(nimcp_stats_correlation_pearson(
        sizes_float.data(), gaps.data(), train_sizes.size(), &gap_trend),
        NIMCP_STATS_OK);

    // Negative correlation: gap decreases with more data
    EXPECT_LT(gap_trend.r, 0.0f)
        << "Overfitting gap should decrease with more training data";

    // Validation score should increase with more data
    nimcp_correlation_result_t val_trend;
    ASSERT_EQ(nimcp_stats_correlation_pearson(
        sizes_float.data(), val_scores.data(), train_sizes.size(), &val_trend),
        NIMCP_STATS_OK);

    EXPECT_GT(val_trend.r, 0.0f)
        << "Validation score should increase with more data";

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 60000.0);
    std::cout << "Learning curve analysis completed in " << elapsed << " ms\n";
    std::cout << "Gap-size correlation: " << gap_trend.r << "\n";
    std::cout << "Val-size correlation: " << val_trend.r << "\n";
}

//=============================================================================
// E2E Test 7: Bias-Variance Decomposition Estimation
//=============================================================================

TEST_F(MLStatisticsE2ETest, BiasVarianceDecomposition) {
    START_TIMER();

    // Simulate multiple model fits with bootstrapping
    const size_t n_bootstrap = 50;
    const size_t n_test_points = 100;

    // True function: y = x^2
    auto true_function = [](float x) { return x * x; };

    // Generate predictions from multiple bootstrap models
    std::vector<std::vector<float>> predictions(n_test_points);
    for (size_t i = 0; i < n_test_points; i++) {
        predictions[i].resize(n_bootstrap);
    }

    std::vector<float> test_x(n_test_points);
    std::vector<float> true_y(n_test_points);
    for (size_t i = 0; i < n_test_points; i++) {
        test_x[i] = -2.0f + 4.0f * i / (n_test_points - 1);
        true_y[i] = true_function(test_x[i]);
    }

    // Simulate predictions from linear models (inherent bias)
    for (size_t b = 0; b < n_bootstrap; b++) {
        // Each bootstrap gives slightly different linear fit
        std::normal_distribution<float> slope_dist(0.5f, 0.1f);
        std::normal_distribution<float> intercept_dist(1.0f, 0.2f);
        float slope = slope_dist(rng);
        float intercept = intercept_dist(rng);

        for (size_t i = 0; i < n_test_points; i++) {
            predictions[i][b] = slope * test_x[i] + intercept;
        }
    }

    // Compute bias, variance, and MSE at each test point
    std::vector<float> point_bias(n_test_points);
    std::vector<float> point_variance(n_test_points);
    std::vector<float> point_mse(n_test_points);

    for (size_t i = 0; i < n_test_points; i++) {
        float mean_pred = nimcp_stats_mean(predictions[i].data(), n_bootstrap);
        float var_pred = nimcp_stats_variance(predictions[i].data(), n_bootstrap);

        point_bias[i] = mean_pred - true_y[i];
        point_variance[i] = var_pred;
        point_mse[i] = point_bias[i] * point_bias[i] + var_pred;
    }

    // Aggregate statistics
    float total_bias_sq = 0.0f, total_variance = 0.0f, total_mse = 0.0f;
    for (size_t i = 0; i < n_test_points; i++) {
        total_bias_sq += point_bias[i] * point_bias[i];
        total_variance += point_variance[i];
        total_mse += point_mse[i];
    }
    total_bias_sq /= n_test_points;
    total_variance /= n_test_points;
    total_mse /= n_test_points;

    // MSE should approximately equal Bias^2 + Variance
    float expected_mse = total_bias_sq + total_variance;
    EXPECT_NEAR(total_mse, expected_mse, 0.1f);

    // Linear model on quadratic function should have high bias
    EXPECT_GT(total_bias_sq, 0.1f) << "Linear model should have bias on quadratic data";

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 60000.0);
    std::cout << "Bias-variance decomposition completed in " << elapsed << " ms\n";
    std::cout << "Bias^2: " << total_bias_sq << ", Variance: " << total_variance
              << ", MSE: " << total_mse << "\n";
}

//=============================================================================
// E2E Test 8: Confusion Matrix Derived Statistics
//=============================================================================

TEST_F(MLStatisticsE2ETest, ConfusionMatrixStatistics) {
    START_TIMER();

    // Simulate classification results
    const size_t n_samples = 1000;
    const size_t n_classes = 3;

    // Generate predictions with known confusion pattern
    std::vector<uint8_t> true_labels(n_samples);
    std::vector<uint8_t> pred_labels(n_samples);

    // Class distribution: roughly balanced
    for (size_t i = 0; i < n_samples; i++) {
        true_labels[i] = i % n_classes;

        // Simulate 85% accuracy with some confusion
        std::uniform_real_distribution<float> uni_dist(0.0f, 1.0f);
        if (uni_dist(rng) < 0.85f) {
            pred_labels[i] = true_labels[i];  // Correct
        } else {
            // Random incorrect class
            pred_labels[i] = (true_labels[i] + 1 + (rng() % (n_classes - 1))) % n_classes;
        }
    }

    // Compute confusion matrix
    std::vector<float> confusion(n_classes * n_classes, 0.0f);
    for (size_t i = 0; i < n_samples; i++) {
        confusion[true_labels[i] * n_classes + pred_labels[i]] += 1.0f;
    }

    // Compute per-class statistics
    std::vector<float> precision(n_classes), recall(n_classes), f1(n_classes);

    for (size_t c = 0; c < n_classes; c++) {
        float tp = confusion[c * n_classes + c];
        float fp = 0.0f, fn = 0.0f;
        for (size_t i = 0; i < n_classes; i++) {
            if (i != c) {
                fp += confusion[i * n_classes + c];  // Predicted c but was i
                fn += confusion[c * n_classes + i];  // Was c but predicted i
            }
        }

        precision[c] = (tp + fp > 0) ? tp / (tp + fp) : 0.0f;
        recall[c] = (tp + fn > 0) ? tp / (tp + fn) : 0.0f;
        f1[c] = (precision[c] + recall[c] > 0)
            ? 2 * precision[c] * recall[c] / (precision[c] + recall[c]) : 0.0f;
    }

    // Compute overall accuracy
    float correct = 0.0f;
    for (size_t c = 0; c < n_classes; c++) {
        correct += confusion[c * n_classes + c];
    }
    float accuracy = correct / n_samples;

    // Compute macro-average F1
    nimcp_descriptive_stats_t f1_stats;
    ASSERT_EQ(nimcp_stats_describe(f1.data(), n_classes, &f1_stats), NIMCP_STATS_OK);

    EXPECT_GT(accuracy, 0.80f) << "Overall accuracy should be > 80%";
    EXPECT_GT(f1_stats.mean, 0.75f) << "Macro F1 should be > 75%";

    // Cohen's Kappa (inter-rater agreement)
    float pe = 0.0f;  // Expected agreement by chance
    for (size_t c = 0; c < n_classes; c++) {
        float row_sum = 0.0f, col_sum = 0.0f;
        for (size_t i = 0; i < n_classes; i++) {
            row_sum += confusion[c * n_classes + i];
            col_sum += confusion[i * n_classes + c];
        }
        pe += (row_sum / n_samples) * (col_sum / n_samples);
    }
    float kappa = (accuracy - pe) / (1.0f - pe);
    EXPECT_GT(kappa, 0.70f) << "Cohen's kappa should indicate substantial agreement";

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 60000.0);
    std::cout << "Confusion matrix analysis completed in " << elapsed << " ms\n";
    std::cout << "Accuracy: " << accuracy << ", Macro-F1: " << f1_stats.mean
              << ", Kappa: " << kappa << "\n";
}

//=============================================================================
// E2E Test 9: ROC/AUC Analysis
//=============================================================================

TEST_F(MLStatisticsE2ETest, ROCAUCAnalysis) {
    START_TIMER();

    // Generate probabilistic predictions
    const size_t n_samples = 1000;
    std::vector<float> scores(n_samples);
    std::vector<uint8_t> labels(n_samples);

    // Positive class centered at higher scores
    std::normal_distribution<float> pos_dist(0.7f, 0.15f);
    std::normal_distribution<float> neg_dist(0.3f, 0.15f);

    for (size_t i = 0; i < n_samples; i++) {
        labels[i] = (i < n_samples / 2) ? 0 : 1;
        scores[i] = (labels[i] == 1) ? pos_dist(rng) : neg_dist(rng);
        scores[i] = std::max(0.0f, std::min(1.0f, scores[i]));
    }

    // Compute ROC curve points
    std::vector<float> thresholds;
    for (float t = 0.0f; t <= 1.0f; t += 0.01f) {
        thresholds.push_back(t);
    }

    std::vector<float> tpr(thresholds.size()), fpr(thresholds.size());

    for (size_t t = 0; t < thresholds.size(); t++) {
        size_t tp = 0, fp = 0, tn = 0, fn = 0;
        for (size_t i = 0; i < n_samples; i++) {
            uint8_t pred = (scores[i] >= thresholds[t]) ? 1 : 0;
            if (pred == 1 && labels[i] == 1) tp++;
            else if (pred == 1 && labels[i] == 0) fp++;
            else if (pred == 0 && labels[i] == 0) tn++;
            else fn++;
        }
        tpr[t] = (tp + fn > 0) ? (float)tp / (tp + fn) : 0.0f;
        fpr[t] = (fp + tn > 0) ? (float)fp / (fp + tn) : 0.0f;
    }

    // Compute AUC using trapezoidal rule
    float auc = 0.0f;
    for (size_t i = 1; i < thresholds.size(); i++) {
        auc += 0.5f * (tpr[i] + tpr[i-1]) * std::abs(fpr[i] - fpr[i-1]);
    }

    // Good classifier should have AUC > 0.8
    EXPECT_GT(auc, 0.8f) << "AUC should be > 0.8 for well-separated classes";

    // Compute Youden's J statistic to find optimal threshold
    float max_j = 0.0f;
    size_t best_thresh_idx = 0;
    for (size_t t = 0; t < thresholds.size(); t++) {
        float j = tpr[t] - fpr[t];
        if (j > max_j) {
            max_j = j;
            best_thresh_idx = t;
        }
    }

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 60000.0);
    std::cout << "ROC/AUC analysis completed in " << elapsed << " ms\n";
    std::cout << "AUC: " << auc << ", Optimal threshold: "
              << thresholds[best_thresh_idx] << "\n";
}

//=============================================================================
// E2E Test 10: Calibration Curve Analysis
//=============================================================================

TEST_F(MLStatisticsE2ETest, CalibrationCurveAnalysis) {
    START_TIMER();

    // Generate predictions from a well-calibrated model
    const size_t n_samples = 2000;
    const size_t n_bins = 10;

    std::vector<float> predicted_probs(n_samples);
    std::vector<uint8_t> true_labels(n_samples);

    // Generate well-calibrated predictions
    std::uniform_real_distribution<float> prob_dist(0.0f, 1.0f);
    for (size_t i = 0; i < n_samples; i++) {
        predicted_probs[i] = prob_dist(rng);
        // Label is 1 with probability equal to predicted probability (calibrated)
        std::bernoulli_distribution label_dist(predicted_probs[i]);
        true_labels[i] = label_dist(rng) ? 1 : 0;
    }

    // Compute calibration bins
    std::vector<float> bin_means(n_bins), bin_fractions(n_bins);
    std::vector<size_t> bin_counts(n_bins, 0);
    std::vector<size_t> bin_positives(n_bins, 0);

    for (size_t i = 0; i < n_samples; i++) {
        size_t bin = std::min((size_t)(predicted_probs[i] * n_bins), n_bins - 1);
        bin_counts[bin]++;
        if (true_labels[i] == 1) bin_positives[bin]++;
    }

    for (size_t b = 0; b < n_bins; b++) {
        bin_means[b] = (b + 0.5f) / n_bins;  // Midpoint of bin
        bin_fractions[b] = (bin_counts[b] > 0)
            ? (float)bin_positives[b] / bin_counts[b] : 0.0f;
    }

    // Compute Expected Calibration Error (ECE)
    float ece = 0.0f;
    for (size_t b = 0; b < n_bins; b++) {
        if (bin_counts[b] > 0) {
            ece += (float)bin_counts[b] / n_samples
                 * std::abs(bin_fractions[b] - bin_means[b]);
        }
    }

    // Well-calibrated model should have low ECE
    EXPECT_LT(ece, 0.1f) << "Expected Calibration Error should be < 10%";

    // Calibration curve should correlate strongly with diagonal
    nimcp_correlation_result_t calib_corr;
    ASSERT_EQ(nimcp_stats_correlation_pearson(
        bin_means.data(), bin_fractions.data(), n_bins, &calib_corr),
        NIMCP_STATS_OK);

    EXPECT_GT(calib_corr.r, 0.9f) << "Calibration should be near-perfect";

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 60000.0);
    std::cout << "Calibration analysis completed in " << elapsed << " ms\n";
    std::cout << "ECE: " << ece << ", Calibration correlation: " << calib_corr.r << "\n";
}

//=============================================================================
// E2E Test 11: Ensemble Model Statistics
//=============================================================================

TEST_F(MLStatisticsE2ETest, EnsembleModelStatistics) {
    START_TIMER();

    // Simulate ensemble of models
    const size_t n_models = 10;
    const size_t n_samples = 500;

    std::vector<std::vector<float>> model_predictions(n_models);
    std::vector<float> true_values(n_samples);

    // Generate true values
    std::normal_distribution<float> true_dist(0.0f, 1.0f);
    for (size_t i = 0; i < n_samples; i++) {
        true_values[i] = true_dist(rng);
    }

    // Each model has slightly different bias and variance
    for (size_t m = 0; m < n_models; m++) {
        model_predictions[m].resize(n_samples);
        std::normal_distribution<float> bias_dist(0.0f, 0.1f);
        std::normal_distribution<float> noise_dist(0.0f, 0.3f);
        float model_bias = bias_dist(rng);

        for (size_t i = 0; i < n_samples; i++) {
            model_predictions[m][i] = true_values[i] + model_bias + noise_dist(rng);
        }
    }

    // Compute ensemble prediction (mean)
    std::vector<float> ensemble_pred(n_samples, 0.0f);
    for (size_t i = 0; i < n_samples; i++) {
        for (size_t m = 0; m < n_models; m++) {
            ensemble_pred[i] += model_predictions[m][i];
        }
        ensemble_pred[i] /= n_models;
    }

    // Compute MSE for each model and ensemble
    std::vector<float> model_mse(n_models);
    for (size_t m = 0; m < n_models; m++) {
        float mse = 0.0f;
        for (size_t i = 0; i < n_samples; i++) {
            float diff = model_predictions[m][i] - true_values[i];
            mse += diff * diff;
        }
        model_mse[m] = mse / n_samples;
    }

    float ensemble_mse = 0.0f;
    for (size_t i = 0; i < n_samples; i++) {
        float diff = ensemble_pred[i] - true_values[i];
        ensemble_mse += diff * diff;
    }
    ensemble_mse /= n_samples;

    // Ensemble should typically have lower MSE
    nimcp_descriptive_stats_t mse_stats;
    ASSERT_EQ(nimcp_stats_describe(model_mse.data(), n_models, &mse_stats),
              NIMCP_STATS_OK);

    EXPECT_LT(ensemble_mse, mse_stats.mean)
        << "Ensemble MSE should be lower than average model MSE";

    // Compute model diversity (disagreement)
    std::vector<float> prediction_stds(n_samples);
    for (size_t i = 0; i < n_samples; i++) {
        std::vector<float> preds_at_i(n_models);
        for (size_t m = 0; m < n_models; m++) {
            preds_at_i[m] = model_predictions[m][i];
        }
        prediction_stds[i] = nimcp_stats_std_dev(preds_at_i.data(), n_models);
    }

    float mean_diversity = nimcp_stats_mean(prediction_stds.data(), n_samples);
    EXPECT_GT(mean_diversity, 0.0f) << "Ensemble should have some diversity";

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 60000.0);
    std::cout << "Ensemble analysis completed in " << elapsed << " ms\n";
    std::cout << "Mean model MSE: " << mse_stats.mean << ", Ensemble MSE: "
              << ensemble_mse << ", Diversity: " << mean_diversity << "\n";
}

//=============================================================================
// E2E Test 12: Regularization Path Analysis
//=============================================================================

TEST_F(MLStatisticsE2ETest, RegularizationPathAnalysis) {
    START_TIMER();

    // Simulate regularization path for ridge regression
    const size_t n_lambdas = 20;
    std::vector<float> lambdas(n_lambdas);
    std::vector<float> train_errors(n_lambdas);
    std::vector<float> val_errors(n_lambdas);
    std::vector<float> coef_norms(n_lambdas);

    // Generate lambda values (log-spaced)
    for (size_t i = 0; i < n_lambdas; i++) {
        lambdas[i] = std::pow(10.0f, -4.0f + 6.0f * i / (n_lambdas - 1));
    }

    // Simulate typical regularization behavior
    for (size_t i = 0; i < n_lambdas; i++) {
        float log_lambda = std::log10(lambdas[i]);

        // Train error increases with regularization
        std::normal_distribution<float> train_noise(0.0f, 0.01f);
        train_errors[i] = 0.05f + 0.02f * (log_lambda + 4) / 6 + train_noise(rng);

        // Validation error is U-shaped
        float optimal_log_lambda = 0.0f;  // Optimal around lambda = 1
        std::normal_distribution<float> val_noise(0.0f, 0.02f);
        val_errors[i] = 0.10f + 0.03f * std::pow(log_lambda - optimal_log_lambda, 2)
                      + val_noise(rng);

        // Coefficient norm decreases with regularization
        coef_norms[i] = 10.0f * std::exp(-0.5f * (log_lambda + 4));
    }

    // Find optimal lambda (minimum validation error)
    size_t best_idx = 0;
    for (size_t i = 1; i < n_lambdas; i++) {
        if (val_errors[i] < val_errors[best_idx]) {
            best_idx = i;
        }
    }

    // Correlation between lambda and train error (should be positive)
    nimcp_correlation_result_t lambda_train_corr;
    std::vector<float> log_lambdas(n_lambdas);
    for (size_t i = 0; i < n_lambdas; i++) {
        log_lambdas[i] = std::log10(lambdas[i]);
    }
    ASSERT_EQ(nimcp_stats_correlation_pearson(
        log_lambdas.data(), train_errors.data(), n_lambdas, &lambda_train_corr),
        NIMCP_STATS_OK);

    EXPECT_GT(lambda_train_corr.r, 0.8f)
        << "Train error should increase with regularization";

    // Correlation between lambda and coef norm (should be negative)
    nimcp_correlation_result_t lambda_coef_corr;
    ASSERT_EQ(nimcp_stats_correlation_pearson(
        log_lambdas.data(), coef_norms.data(), n_lambdas, &lambda_coef_corr),
        NIMCP_STATS_OK);

    EXPECT_LT(lambda_coef_corr.r, -0.8f)
        << "Coefficient norm should decrease with regularization";

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 60000.0);
    std::cout << "Regularization path analysis completed in " << elapsed << " ms\n";
    std::cout << "Optimal lambda: " << lambdas[best_idx]
              << " at index " << best_idx << "\n";
}

//=============================================================================
// E2E Test 13: Feature Selection via Mutual Information
//=============================================================================

TEST_F(MLStatisticsE2ETest, FeatureSelectionMutualInformation) {
    START_TIMER();

    // Generate features with varying MI with target
    const size_t n_samples = 1000;
    const size_t n_features = 8;
    const size_t n_bins = 10;

    std::vector<float> features(n_samples * n_features);
    std::vector<float> target(n_samples);

    // Generate target
    std::normal_distribution<float> target_dist(0.0f, 1.0f);
    for (size_t i = 0; i < n_samples; i++) {
        target[i] = target_dist(rng);
    }

    // Generate features with different relationships to target
    for (size_t j = 0; j < n_features; j++) {
        float relationship_strength = (j < 3) ? 0.8f : 0.1f;  // First 3 are important
        std::normal_distribution<float> noise_dist(0.0f, 1.0f - relationship_strength);

        for (size_t i = 0; i < n_samples; i++) {
            features[i * n_features + j] = relationship_strength * target[i]
                                         + noise_dist(rng);
        }
    }

    // Compute MI for each feature using discretization
    std::vector<float> feature_mi(n_features);

    for (size_t j = 0; j < n_features; j++) {
        // Extract feature column
        std::vector<float> feat_col(n_samples);
        for (size_t i = 0; i < n_samples; i++) {
            feat_col[i] = features[i * n_features + j];
        }

        // Discretize both feature and target
        float feat_min = nimcp_stats_min(feat_col.data(), n_samples);
        float feat_max = nimcp_stats_max(feat_col.data(), n_samples);
        float target_min = nimcp_stats_min(target.data(), n_samples);
        float target_max = nimcp_stats_max(target.data(), n_samples);

        // Build joint histogram
        std::vector<float> joint_hist(n_bins * n_bins, 0.0f);
        for (size_t i = 0; i < n_samples; i++) {
            size_t feat_bin = std::min((size_t)((feat_col[i] - feat_min)
                            / (feat_max - feat_min + 1e-10f) * n_bins), n_bins - 1);
            size_t target_bin = std::min((size_t)((target[i] - target_min)
                              / (target_max - target_min + 1e-10f) * n_bins), n_bins - 1);
            joint_hist[feat_bin * n_bins + target_bin] += 1.0f;
        }

        // Normalize to joint probability
        for (size_t k = 0; k < n_bins * n_bins; k++) {
            joint_hist[k] /= n_samples;
        }

        feature_mi[j] = nimcp_stats_mutual_information(joint_hist.data(), n_bins, n_bins);
    }

    // Important features should have higher MI
    float important_mi_avg = (feature_mi[0] + feature_mi[1] + feature_mi[2]) / 3.0f;
    float unimportant_mi_avg = 0.0f;
    for (size_t j = 3; j < n_features; j++) {
        unimportant_mi_avg += feature_mi[j];
    }
    unimportant_mi_avg /= (n_features - 3);

    EXPECT_GT(important_mi_avg, unimportant_mi_avg)
        << "Important features should have higher MI";

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 60000.0);
    std::cout << "Feature selection via MI completed in " << elapsed << " ms\n";
    std::cout << "Important features avg MI: " << important_mi_avg
              << ", Unimportant avg MI: " << unimportant_mi_avg << "\n";
}

//=============================================================================
// E2E Test 14: Class Imbalance Metrics
//=============================================================================

TEST_F(MLStatisticsE2ETest, ClassImbalanceMetrics) {
    START_TIMER();

    // Simulate highly imbalanced dataset (1:10 ratio)
    const size_t n_minority = 100;
    const size_t n_majority = 1000;
    const size_t n_total = n_minority + n_majority;

    std::vector<float> scores(n_total);
    std::vector<uint8_t> labels(n_total);

    // Minority class (positive)
    std::normal_distribution<float> minority_dist(0.6f, 0.2f);
    for (size_t i = 0; i < n_minority; i++) {
        labels[i] = 1;
        scores[i] = std::max(0.0f, std::min(1.0f, minority_dist(rng)));
    }

    // Majority class (negative)
    std::normal_distribution<float> majority_dist(0.3f, 0.2f);
    for (size_t i = n_minority; i < n_total; i++) {
        labels[i] = 0;
        scores[i] = std::max(0.0f, std::min(1.0f, majority_dist(rng)));
    }

    // Compute at threshold 0.5
    float threshold = 0.5f;
    size_t tp = 0, fp = 0, tn = 0, fn = 0;
    for (size_t i = 0; i < n_total; i++) {
        uint8_t pred = (scores[i] >= threshold) ? 1 : 0;
        if (pred == 1 && labels[i] == 1) tp++;
        else if (pred == 1 && labels[i] == 0) fp++;
        else if (pred == 0 && labels[i] == 0) tn++;
        else fn++;
    }

    float accuracy = (float)(tp + tn) / n_total;
    float precision = (tp + fp > 0) ? (float)tp / (tp + fp) : 0.0f;
    float recall = (tp + fn > 0) ? (float)tp / (tp + fn) : 0.0f;
    float f1 = (precision + recall > 0)
             ? 2 * precision * recall / (precision + recall) : 0.0f;

    // Balanced accuracy
    float tpr = (tp + fn > 0) ? (float)tp / (tp + fn) : 0.0f;
    float tnr = (tn + fp > 0) ? (float)tn / (tn + fp) : 0.0f;
    float balanced_accuracy = (tpr + tnr) / 2.0f;

    // Matthews Correlation Coefficient (MCC)
    float mcc_num = (float)(tp * tn) - (float)(fp * fn);
    float mcc_den = std::sqrt((float)(tp + fp) * (tp + fn) * (tn + fp) * (tn + fn));
    float mcc = (mcc_den > 0) ? mcc_num / mcc_den : 0.0f;

    // G-mean
    float g_mean = std::sqrt(tpr * tnr);

    // Accuracy can be misleading with imbalance
    EXPECT_GT(accuracy, 0.7f);  // High due to majority class
    EXPECT_GT(balanced_accuracy, 0.5f);  // More realistic
    EXPECT_GT(mcc, 0.2f);  // Indicates some predictive power

    // Imbalance ratio
    float imbalance_ratio = (float)n_majority / n_minority;
    EXPECT_NEAR(imbalance_ratio, 10.0f, 0.1f);

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 60000.0);
    std::cout << "Class imbalance analysis completed in " << elapsed << " ms\n";
    std::cout << "Accuracy: " << accuracy << ", Balanced: " << balanced_accuracy
              << ", MCC: " << mcc << ", G-mean: " << g_mean << "\n";
}

//=============================================================================
// E2E Test 15: Multi-Model Statistical Comparison
//=============================================================================

TEST_F(MLStatisticsE2ETest, MultiModelStatisticalComparison) {
    START_TIMER();

    // Compare 5 models across 20 datasets
    const size_t n_models = 5;
    const size_t n_datasets = 20;

    std::vector<std::vector<float>> results(n_models);
    for (size_t m = 0; m < n_models; m++) {
        results[m].resize(n_datasets);
    }

    // Simulate performance with model 2 being best
    float base_performance[] = {0.78f, 0.80f, 0.85f, 0.82f, 0.79f};

    for (size_t d = 0; d < n_datasets; d++) {
        for (size_t m = 0; m < n_models; m++) {
            std::normal_distribution<float> perf_dist(base_performance[m], 0.03f);
            results[m][d] = perf_dist(rng);
        }
    }

    // Compute mean ranks (Friedman test preparation)
    std::vector<float> mean_ranks(n_models, 0.0f);
    for (size_t d = 0; d < n_datasets; d++) {
        // Get ranks for this dataset
        std::vector<std::pair<float, size_t>> perf_idx(n_models);
        for (size_t m = 0; m < n_models; m++) {
            perf_idx[m] = {results[m][d], m};
        }
        std::sort(perf_idx.begin(), perf_idx.end(), std::greater<>());

        for (size_t rank = 0; rank < n_models; rank++) {
            mean_ranks[perf_idx[rank].second] += (rank + 1);
        }
    }
    for (size_t m = 0; m < n_models; m++) {
        mean_ranks[m] /= n_datasets;
    }

    // Model 2 should have best (lowest) mean rank
    size_t best_model = 0;
    for (size_t m = 1; m < n_models; m++) {
        if (mean_ranks[m] < mean_ranks[best_model]) {
            best_model = m;
        }
    }
    EXPECT_EQ(best_model, 2u) << "Model 2 should have lowest mean rank";

    // Friedman statistic (simplified)
    float friedman = 12.0f * n_datasets / (n_models * (n_models + 1));
    float rank_sum_sq = 0.0f;
    for (size_t m = 0; m < n_models; m++) {
        rank_sum_sq += mean_ranks[m] * mean_ranks[m];
    }
    friedman *= rank_sum_sq;
    friedman -= 3.0f * n_datasets * (n_models + 1);

    // Compute overall statistics
    std::vector<float> model_means(n_models);
    for (size_t m = 0; m < n_models; m++) {
        model_means[m] = nimcp_stats_mean(results[m].data(), n_datasets);
    }

    nimcp_descriptive_stats_t mean_stats;
    ASSERT_EQ(nimcp_stats_describe(model_means.data(), n_models, &mean_stats),
              NIMCP_STATS_OK);

    // Pairwise comparison: best model vs others
    size_t significant_wins = 0;
    for (size_t m = 0; m < n_models; m++) {
        if (m == best_model) continue;

        nimcp_test_result_t paired;
        nimcp_stats_ttest_paired(results[best_model].data(), results[m].data(),
                                n_datasets, NIMCP_TEST_TWO_SIDED, 0.95f, &paired);
        if (paired.p_value < 0.05f &&
            nimcp_stats_mean(results[best_model].data(), n_datasets) >
            nimcp_stats_mean(results[m].data(), n_datasets)) {
            significant_wins++;
        }
    }

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 60000.0);
    std::cout << "Multi-model comparison completed in " << elapsed << " ms\n";
    std::cout << "Best model: " << best_model << " with mean rank "
              << mean_ranks[best_model] << "\n";
    std::cout << "Significant wins over other models: " << significant_wins << "\n";
    std::cout << "Friedman statistic: " << friedman << "\n";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
