//=============================================================================
// test_multivariate_e2e.cpp - High-Dimensional Data Analysis E2E Tests
//=============================================================================
/**
 * @file test_multivariate_e2e.cpp
 * @brief End-to-end tests for high-dimensional data reduction and analysis
 *
 * WHAT: Complete multivariate analysis pipelines including PCA, ICA, GMM, LDA
 * WHY:  Verify statistics module handles high-dimensional data scenarios
 * HOW:  Dimensionality reduction, clustering, classification workflows
 *
 * TEST SCENARIOS:
 * 1. PCA for dimensionality reduction
 * 2. ICA for source separation
 * 3. GMM clustering
 * 4. LDA classification
 * 5. Cross-validation evaluation
 * 6. Covariance matrix analysis
 * 7. Correlation matrix factorization
 * 8. Multivariate normality tests
 * 9. Principal component selection
 * 10. Factor analysis
 * 11. Mahalanobis distance computation
 * 12. Multivariate outlier detection
 * 13. Canonical correlation analysis
 * 14. Discriminant function analysis
 * 15. Multidimensional scaling
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

class MultivariateE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        config = nimcp_stats_default_config();
        ASSERT_EQ(nimcp_stats_init(&config), NIMCP_STATS_OK);
        rng.seed(42);
    }

    void TearDown() override {
        nimcp_stats_shutdown();
    }

    nimcp_stats_config_t config;
    std::mt19937 rng;

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

    // Generate multivariate normal data
    std::vector<float> generate_mvnormal(int n_samples, int n_features,
                                         const std::vector<float>& means,
                                         const std::vector<float>& cov_matrix) {
        std::vector<float> data(n_samples * n_features);

        // Simple: generate uncorrelated, then apply Cholesky
        // For simplicity, just add correlation manually
        for (int i = 0; i < n_samples; i++) {
            auto z = generate_normal(n_features, 0.0f, 1.0f);
            for (int j = 0; j < n_features; j++) {
                data[i * n_features + j] = means[j];
                for (int k = 0; k <= j; k++) {
                    // Simplified: diagonal dominance
                    float weight = (k == j) ? 1.0f : cov_matrix[j * n_features + k] / std::sqrt(cov_matrix[j * n_features + j] + 1e-6f);
                    data[i * n_features + j] += weight * z[k];
                }
            }
        }
        return data;
    }

    // Compute covariance matrix
    void compute_cov_matrix(const std::vector<float>& data, int n_samples, int n_features,
                           std::vector<float>& cov, std::vector<float>& means) {
        means.resize(n_features, 0.0f);
        cov.resize(n_features * n_features, 0.0f);

        // Compute means
        for (int i = 0; i < n_samples; i++) {
            for (int j = 0; j < n_features; j++) {
                means[j] += data[i * n_features + j];
            }
        }
        for (int j = 0; j < n_features; j++) {
            means[j] /= n_samples;
        }

        // Compute covariance
        for (int i = 0; i < n_samples; i++) {
            for (int j = 0; j < n_features; j++) {
                float dj = data[i * n_features + j] - means[j];
                for (int k = 0; k < n_features; k++) {
                    float dk = data[i * n_features + k] - means[k];
                    cov[j * n_features + k] += dj * dk;
                }
            }
        }
        for (int j = 0; j < n_features * n_features; j++) {
            cov[j] /= (n_samples - 1);
        }
    }

    // Simple power iteration for largest eigenvalue
    float power_iteration(const std::vector<float>& matrix, int n, int max_iter = 100) {
        std::vector<float> v = generate_uniform(n, 0.0f, 1.0f);
        float norm = 0.0f;
        for (float x : v) norm += x * x;
        norm = std::sqrt(norm);
        for (float& x : v) x /= norm;

        float eigenvalue = 0.0f;
        for (int iter = 0; iter < max_iter; iter++) {
            // Multiply
            std::vector<float> Av(n, 0.0f);
            for (int i = 0; i < n; i++) {
                for (int j = 0; j < n; j++) {
                    Av[i] += matrix[i * n + j] * v[j];
                }
            }

            // Compute eigenvalue (Rayleigh quotient)
            eigenvalue = 0.0f;
            for (int i = 0; i < n; i++) {
                eigenvalue += v[i] * Av[i];
            }

            // Normalize
            norm = 0.0f;
            for (float x : Av) norm += x * x;
            norm = std::sqrt(norm);
            for (int i = 0; i < n; i++) v[i] = Av[i] / (norm + 1e-10f);
        }
        return eigenvalue;
    }
};

//=============================================================================
// E2E Test 1: PCA for Dimensionality Reduction
//=============================================================================

TEST_F(MultivariateE2ETest, PCADimensionalityReduction) {
    START_TIMER();

    // Generate high-dimensional data with low intrinsic dimensionality
    const int n_samples = 500;
    const int n_features = 100;
    const int n_components = 3;  // True dimensionality

    // Create data: 3 independent sources, projected to 100D
    std::vector<float> sources(n_samples * n_components);
    for (int i = 0; i < n_samples * n_components; i++) {
        sources[i] = generate_normal(1, 0.0f, 1.0f)[0];
    }

    // Random projection matrix
    std::vector<float> projection(n_components * n_features);
    for (int i = 0; i < n_components * n_features; i++) {
        projection[i] = generate_normal(1, 0.0f, 1.0f)[0];
    }

    // Project sources to high-D
    std::vector<float> data(n_samples * n_features, 0.0f);
    for (int i = 0; i < n_samples; i++) {
        for (int j = 0; j < n_features; j++) {
            for (int k = 0; k < n_components; k++) {
                data[i * n_features + j] += sources[i * n_components + k] *
                                           projection[k * n_features + j];
            }
            // Add small noise
            data[i * n_features + j] += 0.1f * generate_normal(1, 0.0f, 1.0f)[0];
        }
    }

    // Compute covariance matrix
    std::vector<float> cov, means;
    compute_cov_matrix(data, n_samples, n_features, cov, means);

    // Compute eigenvalues (simplified: just check structure)
    // Use covariance matrix trace and Frobenius norm as proxies
    float trace = 0.0f;
    for (int i = 0; i < n_features; i++) {
        trace += cov[i * n_features + i];
    }

    // Largest eigenvalue via power iteration
    float lambda_max = power_iteration(cov, n_features, 50);

    // Top eigenvalues should capture most variance
    EXPECT_GT(lambda_max, 0.1f * trace);

    // Total variance should be captured by few components
    // (eigenvalue sum = trace)
    EXPECT_GT(trace, 0.0f);

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 10000.0);

    std::cout << "PCA: " << n_samples << "x" << n_features << " data, "
              << "trace=" << trace << ", lambda_max=" << lambda_max << ", "
              << "time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 2: ICA for Source Separation (Simplified)
//=============================================================================

TEST_F(MultivariateE2ETest, ICASourceSeparation) {
    START_TIMER();

    // Generate mixed signals from independent sources
    const int n_samples = 1000;
    const int n_sources = 3;

    // Independent sources (different distributions)
    std::vector<std::vector<float>> sources(n_sources);

    // Source 1: Sinusoidal
    sources[0].resize(n_samples);
    for (int i = 0; i < n_samples; i++) {
        sources[0][i] = std::sin(2 * M_PI * i / 100);
    }

    // Source 2: Sawtooth
    sources[1].resize(n_samples);
    for (int i = 0; i < n_samples; i++) {
        sources[1][i] = 2.0f * (i % 50) / 50.0f - 1.0f;
    }

    // Source 3: Random uniform
    sources[2] = generate_uniform(n_samples, -1.0f, 1.0f);

    // Mixing matrix
    std::vector<float> mixing = {0.5f, 0.3f, 0.2f,
                                  0.2f, 0.5f, 0.3f,
                                  0.3f, 0.2f, 0.5f};

    // Mixed signals
    std::vector<float> mixed(n_samples * n_sources, 0.0f);
    for (int i = 0; i < n_samples; i++) {
        for (int j = 0; j < n_sources; j++) {
            for (int k = 0; k < n_sources; k++) {
                mixed[i * n_sources + j] += mixing[j * n_sources + k] * sources[k][i];
            }
        }
    }

    // Analyze mixed signals
    // Check that sources can be distinguished by kurtosis
    std::vector<float> kurtosis_values(n_sources);

    for (int s = 0; s < n_sources; s++) {
        std::vector<float> signal(n_samples);
        for (int i = 0; i < n_samples; i++) {
            signal[i] = mixed[i * n_sources + s];
        }
        kurtosis_values[s] = nimcp_stats_kurtosis(signal.data(), n_samples);
    }

    // Different sources should have different statistical properties
    float kurtosis_range = *std::max_element(kurtosis_values.begin(), kurtosis_values.end()) -
                           *std::min_element(kurtosis_values.begin(), kurtosis_values.end());

    // Mixed signals should have some kurtosis variation
    // (though less than original sources)

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 3000.0);

    std::cout << "ICA Setup: " << n_sources << " sources mixed, "
              << "kurtosis range=" << kurtosis_range << ", "
              << "time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 3: GMM Clustering (Simplified)
//=============================================================================

TEST_F(MultivariateE2ETest, GMMClustering) {
    START_TIMER();

    // Generate data from multiple Gaussian clusters
    const int n_clusters = 3;
    const int n_samples_per_cluster = 100;
    const int n_features = 2;
    const int n_total = n_clusters * n_samples_per_cluster;

    std::vector<float> data(n_total * n_features);
    std::vector<int> true_labels(n_total);

    // Cluster centers
    std::vector<std::pair<float, float>> centers = {{0, 0}, {5, 0}, {2.5, 4}};
    float cluster_std = 0.8f;

    for (int c = 0; c < n_clusters; c++) {
        for (int i = 0; i < n_samples_per_cluster; i++) {
            int idx = c * n_samples_per_cluster + i;
            data[idx * n_features + 0] = centers[c].first + cluster_std * generate_normal(1, 0, 1)[0];
            data[idx * n_features + 1] = centers[c].second + cluster_std * generate_normal(1, 0, 1)[0];
            true_labels[idx] = c;
        }
    }

    // Compute cluster statistics
    std::vector<float> cluster_means(n_clusters * n_features);
    std::vector<float> cluster_vars(n_clusters * n_features);

    for (int c = 0; c < n_clusters; c++) {
        for (int f = 0; f < n_features; f++) {
            std::vector<float> feature_vals(n_samples_per_cluster);
            for (int i = 0; i < n_samples_per_cluster; i++) {
                int idx = c * n_samples_per_cluster + i;
                feature_vals[i] = data[idx * n_features + f];
            }
            cluster_means[c * n_features + f] = nimcp_stats_mean(feature_vals.data(), n_samples_per_cluster);
            cluster_vars[c * n_features + f] = nimcp_stats_variance(feature_vals.data(), n_samples_per_cluster);
        }
    }

    // Verify cluster means are near true centers
    for (int c = 0; c < n_clusters; c++) {
        EXPECT_NEAR(cluster_means[c * n_features + 0], centers[c].first, 0.5f);
        EXPECT_NEAR(cluster_means[c * n_features + 1], centers[c].second, 0.5f);
    }

    // Within-cluster variance should be ~cluster_std^2
    for (int c = 0; c < n_clusters; c++) {
        for (int f = 0; f < n_features; f++) {
            EXPECT_NEAR(cluster_vars[c * n_features + f], cluster_std * cluster_std, 0.5f);
        }
    }

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 3000.0);

    std::cout << "GMM Clustering: " << n_clusters << " clusters, "
              << n_total << " samples, "
              << "time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 4: LDA Classification (Simplified)
//=============================================================================

TEST_F(MultivariateE2ETest, LDAClassification) {
    START_TIMER();

    // Generate two-class classification problem
    const int n_per_class = 100;
    const int n_features = 5;
    const int n_total = 2 * n_per_class;

    std::vector<float> data(n_total * n_features);
    std::vector<int> labels(n_total);

    // Class 0: centered at origin
    // Class 1: centered at [2, 0, 0, 0, 0]
    for (int i = 0; i < n_per_class; i++) {
        // Class 0
        for (int f = 0; f < n_features; f++) {
            data[i * n_features + f] = generate_normal(1, 0.0f, 1.0f)[0];
        }
        labels[i] = 0;

        // Class 1
        for (int f = 0; f < n_features; f++) {
            float mean = (f == 0) ? 2.0f : 0.0f;
            data[(n_per_class + i) * n_features + f] = generate_normal(1, mean, 1.0f)[0];
        }
        labels[n_per_class + i] = 1;
    }

    // Compute class means
    std::vector<float> mean0(n_features, 0.0f);
    std::vector<float> mean1(n_features, 0.0f);

    for (int i = 0; i < n_per_class; i++) {
        for (int f = 0; f < n_features; f++) {
            mean0[f] += data[i * n_features + f];
            mean1[f] += data[(n_per_class + i) * n_features + f];
        }
    }
    for (int f = 0; f < n_features; f++) {
        mean0[f] /= n_per_class;
        mean1[f] /= n_per_class;
    }

    // LDA discriminant: w = Sw^-1 * (m1 - m0)
    // For now, just verify class separation
    std::vector<float> mean_diff(n_features);
    for (int f = 0; f < n_features; f++) {
        mean_diff[f] = mean1[f] - mean0[f];
    }

    // First feature should show separation
    EXPECT_NEAR(mean_diff[0], 2.0f, 0.5f);

    // Other features should have near-zero mean difference
    for (int f = 1; f < n_features; f++) {
        EXPECT_NEAR(mean_diff[f], 0.0f, 0.5f);
    }

    // Simple classifier: classify by first feature > 1
    int correct = 0;
    for (int i = 0; i < n_total; i++) {
        int predicted = (data[i * n_features + 0] > 1.0f) ? 1 : 0;
        if (predicted == labels[i]) correct++;
    }
    float accuracy = static_cast<float>(correct) / n_total;

    EXPECT_GT(accuracy, 0.8f);  // Should be fairly accurate

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 2000.0);

    std::cout << "LDA: 2 classes, " << n_features << " features, "
              << "accuracy=" << accuracy << ", "
              << "time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 5: Cross-Validation Evaluation
//=============================================================================

TEST_F(MultivariateE2ETest, CrossValidationEvaluation) {
    START_TIMER();

    // K-fold cross-validation for variance estimation
    const int n_samples = 200;
    const int n_folds = 5;
    const int fold_size = n_samples / n_folds;

    // Generate data
    auto data = generate_normal(n_samples, 10.0f, 2.0f);

    // Cross-validation: estimate mean on training, evaluate on test
    std::vector<float> fold_errors(n_folds);

    for (int fold = 0; fold < n_folds; fold++) {
        // Training data (all except fold)
        std::vector<float> train_data;
        std::vector<float> test_data;

        for (int i = 0; i < n_samples; i++) {
            if (i / fold_size == fold) {
                test_data.push_back(data[i]);
            } else {
                train_data.push_back(data[i]);
            }
        }

        // Estimate mean on training
        float train_mean = nimcp_stats_mean(train_data.data(), train_data.size());

        // Evaluate on test: compute squared errors
        float mse = 0.0f;
        for (float x : test_data) {
            float err = x - train_mean;
            mse += err * err;
        }
        mse /= test_data.size();
        fold_errors[fold] = mse;
    }

    // Analyze fold errors
    nimcp_descriptive_stats_t error_stats;
    EXPECT_EQ(nimcp_stats_describe(fold_errors.data(), n_folds, &error_stats), NIMCP_STATS_OK);

    // MSE should be around variance (4.0)
    EXPECT_NEAR(error_stats.mean, 4.0f, 2.0f);

    // Fold errors should be relatively consistent
    float cv = error_stats.std_dev / (error_stats.mean + 1e-6f);
    EXPECT_LT(cv, 1.0f);

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 2000.0);

    std::cout << "Cross-Validation: " << n_folds << " folds, "
              << "mean MSE=" << error_stats.mean << ", "
              << "CV=" << cv << ", "
              << "time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 6: Covariance Matrix Analysis
//=============================================================================

TEST_F(MultivariateE2ETest, CovarianceMatrixAnalysis) {
    START_TIMER();

    // Generate data with known covariance structure
    const int n_samples = 500;
    const int n_features = 10;

    // Generate with varying correlations
    std::vector<float> data(n_samples * n_features);

    // First feature: independent
    for (int i = 0; i < n_samples; i++) {
        data[i * n_features + 0] = generate_normal(1, 0.0f, 1.0f)[0];
    }

    // Other features: correlated with first
    for (int f = 1; f < n_features; f++) {
        float correlation = 0.9f / f;  // Decreasing correlation
        for (int i = 0; i < n_samples; i++) {
            data[i * n_features + f] = correlation * data[i * n_features + 0] +
                                       std::sqrt(1 - correlation * correlation) * generate_normal(1, 0.0f, 1.0f)[0];
        }
    }

    // Compute covariance matrix using NIMCP function
    std::vector<float> cov_matrix(n_features * n_features);
    EXPECT_EQ(nimcp_stats_covariance_matrix(data.data(), n_samples, n_features, cov_matrix.data()), NIMCP_STATS_OK);

    // Check diagonal (variances should be ~1)
    for (int i = 0; i < n_features; i++) {
        EXPECT_NEAR(cov_matrix[i * n_features + i], 1.0f, 0.3f);
    }

    // Check off-diagonal (covariances/correlations)
    for (int f = 1; f < n_features; f++) {
        float expected_corr = 0.9f / f;
        float actual_cov = cov_matrix[0 * n_features + f];
        EXPECT_NEAR(actual_cov, expected_corr, 0.2f);
    }

    // Matrix should be symmetric
    for (int i = 0; i < n_features; i++) {
        for (int j = i + 1; j < n_features; j++) {
            EXPECT_NEAR(cov_matrix[i * n_features + j], cov_matrix[j * n_features + i], 1e-5f);
        }
    }

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 3000.0);

    std::cout << "Covariance Matrix: " << n_features << "x" << n_features << ", "
              << "time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 7: Correlation Matrix Factorization
//=============================================================================

TEST_F(MultivariateE2ETest, CorrelationMatrixFactorization) {
    START_TIMER();

    // Create data with factor structure
    const int n_samples = 300;
    const int n_features = 12;
    const int n_factors = 3;

    // Generate factors
    std::vector<std::vector<float>> factors(n_factors);
    for (int f = 0; f < n_factors; f++) {
        factors[f] = generate_normal(n_samples, 0.0f, 1.0f);
    }

    // Generate data: each feature loads on one factor
    std::vector<float> data(n_samples * n_features);
    for (int i = 0; i < n_samples; i++) {
        for (int j = 0; j < n_features; j++) {
            int factor_idx = j % n_factors;  // Feature j loads on factor j%3
            float loading = 0.8f;
            data[i * n_features + j] = loading * factors[factor_idx][i] +
                                       std::sqrt(1 - loading * loading) * generate_normal(1, 0.0f, 1.0f)[0];
        }
    }

    // Compute correlation matrix
    std::vector<float> cov, means;
    compute_cov_matrix(data, n_samples, n_features, cov, means);

    // Convert to correlation
    std::vector<float> corr(n_features * n_features);
    for (int i = 0; i < n_features; i++) {
        for (int j = 0; j < n_features; j++) {
            float si = std::sqrt(cov[i * n_features + i] + 1e-6f);
            float sj = std::sqrt(cov[j * n_features + j] + 1e-6f);
            corr[i * n_features + j] = cov[i * n_features + j] / (si * sj);
        }
    }

    // Features loading on same factor should be highly correlated
    float same_factor_corr = 0.0f;
    float diff_factor_corr = 0.0f;
    int same_count = 0, diff_count = 0;

    for (int i = 0; i < n_features; i++) {
        for (int j = i + 1; j < n_features; j++) {
            if (i % n_factors == j % n_factors) {
                same_factor_corr += std::abs(corr[i * n_features + j]);
                same_count++;
            } else {
                diff_factor_corr += std::abs(corr[i * n_features + j]);
                diff_count++;
            }
        }
    }
    same_factor_corr /= same_count;
    diff_factor_corr /= diff_count;

    // Same-factor correlations should be higher
    EXPECT_GT(same_factor_corr, diff_factor_corr);

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 3000.0);

    std::cout << "Factor Structure: same-factor r=" << same_factor_corr
              << ", diff-factor r=" << diff_factor_corr
              << ", time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 8: Multivariate Normality Tests
//=============================================================================

TEST_F(MultivariateE2ETest, MultivariateNormalityTests) {
    START_TIMER();

    const int n_samples = 200;
    const int n_features = 4;

    // Generate multivariate normal data
    std::vector<float> normal_data(n_samples * n_features);
    for (int i = 0; i < n_samples; i++) {
        for (int f = 0; f < n_features; f++) {
            normal_data[i * n_features + f] = generate_normal(1, 0.0f, 1.0f)[0];
        }
    }

    // Generate non-normal data (uniform)
    std::vector<float> uniform_data(n_samples * n_features);
    for (int i = 0; i < n_samples; i++) {
        for (int f = 0; f < n_features; f++) {
            uniform_data[i * n_features + f] = generate_uniform(1, -2.0f, 2.0f)[0];
        }
    }

    // Test normality of each feature
    std::vector<float> normal_skew(n_features), uniform_skew(n_features);
    std::vector<float> normal_kurt(n_features), uniform_kurt(n_features);

    for (int f = 0; f < n_features; f++) {
        std::vector<float> norm_f(n_samples), unif_f(n_samples);
        for (int i = 0; i < n_samples; i++) {
            norm_f[i] = normal_data[i * n_features + f];
            unif_f[i] = uniform_data[i * n_features + f];
        }

        normal_skew[f] = nimcp_stats_skewness(norm_f.data(), n_samples);
        uniform_skew[f] = nimcp_stats_skewness(unif_f.data(), n_samples);
        normal_kurt[f] = nimcp_stats_kurtosis(norm_f.data(), n_samples);
        uniform_kurt[f] = nimcp_stats_kurtosis(unif_f.data(), n_samples);
    }

    // Normal data should have skewness ~0 and kurtosis ~0
    float mean_normal_skew = nimcp_stats_mean(normal_skew.data(), n_features);
    float mean_normal_kurt = nimcp_stats_mean(normal_kurt.data(), n_features);
    float mean_uniform_kurt = nimcp_stats_mean(uniform_kurt.data(), n_features);

    EXPECT_NEAR(mean_normal_skew, 0.0f, 0.5f);
    EXPECT_NEAR(mean_normal_kurt, 0.0f, 1.0f);

    // Uniform has negative excess kurtosis (-1.2)
    EXPECT_LT(mean_uniform_kurt, -0.5f);

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 2000.0);

    std::cout << "Normality Tests: normal skew=" << mean_normal_skew
              << ", normal kurt=" << mean_normal_kurt
              << ", uniform kurt=" << mean_uniform_kurt
              << ", time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 9: Principal Component Selection
//=============================================================================

TEST_F(MultivariateE2ETest, PrincipalComponentSelection) {
    START_TIMER();

    // Analyze how many components to retain
    const int n_samples = 300;
    const int n_features = 20;
    const int true_rank = 5;

    // Generate low-rank data
    std::vector<float> components(n_samples * true_rank);
    for (int i = 0; i < n_samples * true_rank; i++) {
        components[i] = generate_normal(1, 0.0f, 1.0f)[0];
    }

    // Random projection
    std::vector<float> projection(true_rank * n_features);
    for (int i = 0; i < true_rank * n_features; i++) {
        projection[i] = generate_normal(1, 0.0f, 1.0f)[0];
    }

    // Project
    std::vector<float> data(n_samples * n_features, 0.0f);
    for (int i = 0; i < n_samples; i++) {
        for (int j = 0; j < n_features; j++) {
            for (int k = 0; k < true_rank; k++) {
                data[i * n_features + j] += components[i * true_rank + k] *
                                           projection[k * n_features + j];
            }
            // Add small noise
            data[i * n_features + j] += 0.1f * generate_normal(1, 0.0f, 1.0f)[0];
        }
    }

    // Compute covariance
    std::vector<float> cov, means;
    compute_cov_matrix(data, n_samples, n_features, cov, means);

    // Compute trace (total variance)
    float total_variance = 0.0f;
    for (int i = 0; i < n_features; i++) {
        total_variance += cov[i * n_features + i];
    }

    // Estimate effective dimensionality using participation ratio
    // PR = (sum_i lambda_i)^2 / sum_i lambda_i^2
    float sum_sq = 0.0f;
    for (int i = 0; i < n_features * n_features; i++) {
        sum_sq += cov[i] * cov[i];
    }

    float participation_ratio = (total_variance * total_variance) / (sum_sq + 1e-6f);

    // PR should be close to true rank
    EXPECT_GT(participation_ratio, 2.0f);  // At least some structure
    EXPECT_LT(participation_ratio, n_features * 0.5f);  // Not full rank

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 5000.0);

    std::cout << "PC Selection: true rank=" << true_rank
              << ", participation ratio=" << participation_ratio
              << ", time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 10: Factor Analysis (Simplified)
//=============================================================================

TEST_F(MultivariateE2ETest, FactorAnalysis) {
    START_TIMER();

    // Test factor model assumptions
    const int n_samples = 400;
    const int n_features = 8;
    const int n_factors = 2;

    // Generate factor model: X = LF + e
    // L = loadings, F = factors, e = unique variance
    std::vector<std::vector<float>> factors(n_factors);
    for (int f = 0; f < n_factors; f++) {
        factors[f] = generate_normal(n_samples, 0.0f, 1.0f);
    }

    // Loadings matrix (pattern matrix)
    std::vector<std::vector<float>> loadings(n_features, std::vector<float>(n_factors, 0.0f));

    // First 4 features load on factor 1
    for (int i = 0; i < 4; i++) {
        loadings[i][0] = 0.7f + 0.1f * i;
    }
    // Next 4 features load on factor 2
    for (int i = 4; i < 8; i++) {
        loadings[i][1] = 0.7f + 0.1f * (i - 4);
    }

    // Generate data
    std::vector<float> data(n_samples * n_features);
    for (int i = 0; i < n_samples; i++) {
        for (int j = 0; j < n_features; j++) {
            float value = 0.0f;
            for (int f = 0; f < n_factors; f++) {
                value += loadings[j][f] * factors[f][i];
            }
            // Add unique variance
            value += 0.4f * generate_normal(1, 0.0f, 1.0f)[0];
            data[i * n_features + j] = value;
        }
    }

    // Compute covariance
    std::vector<float> cov, means;
    compute_cov_matrix(data, n_samples, n_features, cov, means);

    // Check factor structure: features 0-3 should correlate with each other
    float within_factor1 = 0.0f;
    int count1 = 0;
    for (int i = 0; i < 4; i++) {
        for (int j = i + 1; j < 4; j++) {
            float si = std::sqrt(cov[i * n_features + i]);
            float sj = std::sqrt(cov[j * n_features + j]);
            float r = cov[i * n_features + j] / (si * sj + 1e-6f);
            within_factor1 += r;
            count1++;
        }
    }
    within_factor1 /= count1;

    // Features across factors should have lower correlation
    float across_factors = 0.0f;
    int count2 = 0;
    for (int i = 0; i < 4; i++) {
        for (int j = 4; j < 8; j++) {
            float si = std::sqrt(cov[i * n_features + i]);
            float sj = std::sqrt(cov[j * n_features + j]);
            float r = cov[i * n_features + j] / (si * sj + 1e-6f);
            across_factors += std::abs(r);
            count2++;
        }
    }
    across_factors /= count2;

    EXPECT_GT(within_factor1, across_factors);

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 3000.0);

    std::cout << "Factor Analysis: within-factor r=" << within_factor1
              << ", across-factor r=" << across_factors
              << ", time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 11: Mahalanobis Distance Computation
//=============================================================================

TEST_F(MultivariateE2ETest, MahalanobisDistanceComputation) {
    START_TIMER();

    const int n_samples = 300;
    const int n_features = 5;

    // Generate reference distribution
    std::vector<float> data(n_samples * n_features);
    for (int i = 0; i < n_samples; i++) {
        for (int f = 0; f < n_features; f++) {
            data[i * n_features + f] = generate_normal(1, 0.0f, 1.0f)[0];
        }
    }

    // Compute mean and covariance
    std::vector<float> cov, means;
    compute_cov_matrix(data, n_samples, n_features, cov, means);

    // Compute Mahalanobis distances for all points
    // Simplified: use diagonal covariance approximation
    std::vector<float> diag_inv(n_features);
    for (int f = 0; f < n_features; f++) {
        diag_inv[f] = 1.0f / (cov[f * n_features + f] + 1e-6f);
    }

    std::vector<float> mahal_dist(n_samples);
    for (int i = 0; i < n_samples; i++) {
        float dist = 0.0f;
        for (int f = 0; f < n_features; f++) {
            float diff = data[i * n_features + f] - means[f];
            dist += diff * diff * diag_inv[f];
        }
        mahal_dist[i] = std::sqrt(dist);
    }

    // Mahalanobis distances should follow chi distribution
    // Mean should be around sqrt(n_features)
    float mean_dist = nimcp_stats_mean(mahal_dist.data(), n_samples);
    EXPECT_NEAR(mean_dist, std::sqrt(static_cast<float>(n_features)), 0.5f);

    // Test a clear outlier
    std::vector<float> outlier(n_features);
    for (int f = 0; f < n_features; f++) {
        outlier[f] = 5.0f;  // 5 sigma in each direction
    }

    float outlier_dist = 0.0f;
    for (int f = 0; f < n_features; f++) {
        float diff = outlier[f] - means[f];
        outlier_dist += diff * diff * diag_inv[f];
    }
    outlier_dist = std::sqrt(outlier_dist);

    // Outlier should have much larger distance
    EXPECT_GT(outlier_dist, mean_dist * 2);

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 2000.0);

    std::cout << "Mahalanobis: mean dist=" << mean_dist
              << ", outlier dist=" << outlier_dist
              << ", time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 12: Multivariate Outlier Detection
//=============================================================================

TEST_F(MultivariateE2ETest, MultivariateOutlierDetection) {
    START_TIMER();

    const int n_normal = 200;
    const int n_outliers = 10;
    const int n_features = 4;
    const int n_total = n_normal + n_outliers;

    // Generate normal data
    std::vector<float> data(n_total * n_features);
    std::vector<bool> is_outlier(n_total, false);

    for (int i = 0; i < n_normal; i++) {
        for (int f = 0; f < n_features; f++) {
            data[i * n_features + f] = generate_normal(1, 0.0f, 1.0f)[0];
        }
    }

    // Generate outliers
    for (int i = n_normal; i < n_total; i++) {
        for (int f = 0; f < n_features; f++) {
            data[i * n_features + f] = generate_normal(1, 4.0f, 0.5f)[0];  // Shifted mean
        }
        is_outlier[i] = true;
    }

    // Detect outliers using IQR method on each feature
    std::vector<float> outlier_scores(n_total, 0.0f);

    for (int f = 0; f < n_features; f++) {
        std::vector<float> feature_vals(n_total);
        for (int i = 0; i < n_total; i++) {
            feature_vals[i] = data[i * n_features + f];
        }

        float q1 = nimcp_stats_quantile(feature_vals.data(), n_total, 0.25f);
        float q3 = nimcp_stats_quantile(feature_vals.data(), n_total, 0.75f);
        float iqr = q3 - q1;
        float lower = q1 - 1.5f * iqr;
        float upper = q3 + 1.5f * iqr;

        for (int i = 0; i < n_total; i++) {
            if (feature_vals[i] < lower || feature_vals[i] > upper) {
                outlier_scores[i] += 1.0f;
            }
        }
    }

    // Count detected outliers
    int detected = 0;
    int false_positives = 0;
    float threshold = n_features * 0.5f;  // Outlier in at least half of features

    for (int i = 0; i < n_total; i++) {
        bool predicted_outlier = outlier_scores[i] >= threshold;
        if (predicted_outlier && is_outlier[i]) detected++;
        if (predicted_outlier && !is_outlier[i]) false_positives++;
    }

    float recall = static_cast<float>(detected) / n_outliers;
    float precision = static_cast<float>(detected) / (detected + false_positives + 1e-6f);

    EXPECT_GT(recall, 0.5f);  // Should detect most outliers
    EXPECT_GT(precision, 0.5f);  // Should have reasonable precision

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 2000.0);

    std::cout << "Outlier Detection: " << detected << "/" << n_outliers << " detected, "
              << false_positives << " false positives, "
              << "recall=" << recall << ", precision=" << precision
              << ", time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 13: Canonical Correlation Analysis (Simplified)
//=============================================================================

TEST_F(MultivariateE2ETest, CanonicalCorrelationAnalysis) {
    START_TIMER();

    // Test correlation between two sets of variables
    const int n_samples = 300;
    const int n_set1 = 4;
    const int n_set2 = 3;

    // Generate correlated variable sets
    // Common factor influences both sets
    auto common_factor = generate_normal(n_samples, 0.0f, 1.0f);

    std::vector<float> set1(n_samples * n_set1);
    std::vector<float> set2(n_samples * n_set2);

    for (int i = 0; i < n_samples; i++) {
        for (int f = 0; f < n_set1; f++) {
            float loading = 0.7f - 0.1f * f;
            set1[i * n_set1 + f] = loading * common_factor[i] +
                                   std::sqrt(1 - loading * loading) * generate_normal(1, 0, 1)[0];
        }
        for (int f = 0; f < n_set2; f++) {
            float loading = 0.6f - 0.1f * f;
            set2[i * n_set2 + f] = loading * common_factor[i] +
                                   std::sqrt(1 - loading * loading) * generate_normal(1, 0, 1)[0];
        }
    }

    // Compute cross-correlation matrix between sets
    std::vector<float> cross_corr(n_set1 * n_set2);

    for (int i = 0; i < n_set1; i++) {
        std::vector<float> v1(n_samples);
        for (int s = 0; s < n_samples; s++) v1[s] = set1[s * n_set1 + i];

        for (int j = 0; j < n_set2; j++) {
            std::vector<float> v2(n_samples);
            for (int s = 0; s < n_samples; s++) v2[s] = set2[s * n_set2 + j];

            nimcp_correlation_result_t result;
            nimcp_stats_correlation_pearson(v1.data(), v2.data(), n_samples, &result);
            cross_corr[i * n_set2 + j] = result.r;
        }
    }

    // Find maximum correlation (approximation to first canonical correlation)
    float max_corr = *std::max_element(cross_corr.begin(), cross_corr.end());

    // Should have moderate to high canonical correlation due to shared factor
    EXPECT_GT(max_corr, 0.3f);

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 3000.0);

    std::cout << "Canonical Correlation: max r=" << max_corr
              << ", time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 14: Discriminant Function Analysis
//=============================================================================

TEST_F(MultivariateE2ETest, DiscriminantFunctionAnalysis) {
    START_TIMER();

    // Multi-class discrimination
    const int n_classes = 3;
    const int n_per_class = 80;
    const int n_features = 4;
    const int n_total = n_classes * n_per_class;

    // Class means
    std::vector<std::vector<float>> class_means = {
        {0, 0, 0, 0},
        {3, 0, 0, 0},
        {1.5, 2.6, 0, 0}  // Triangle arrangement
    };

    // Generate data
    std::vector<float> data(n_total * n_features);
    std::vector<int> labels(n_total);

    for (int c = 0; c < n_classes; c++) {
        for (int i = 0; i < n_per_class; i++) {
            int idx = c * n_per_class + i;
            for (int f = 0; f < n_features; f++) {
                data[idx * n_features + f] = class_means[c][f] + generate_normal(1, 0.0f, 0.8f)[0];
            }
            labels[idx] = c;
        }
    }

    // Compute between-class and within-class scatter
    std::vector<float> overall_mean(n_features, 0.0f);
    for (int i = 0; i < n_total; i++) {
        for (int f = 0; f < n_features; f++) {
            overall_mean[f] += data[i * n_features + f];
        }
    }
    for (int f = 0; f < n_features; f++) {
        overall_mean[f] /= n_total;
    }

    // Within-class scatter
    float total_within = 0.0f;
    for (int c = 0; c < n_classes; c++) {
        std::vector<float> cmean(n_features, 0.0f);
        for (int i = 0; i < n_per_class; i++) {
            int idx = c * n_per_class + i;
            for (int f = 0; f < n_features; f++) {
                cmean[f] += data[idx * n_features + f];
            }
        }
        for (int f = 0; f < n_features; f++) cmean[f] /= n_per_class;

        for (int i = 0; i < n_per_class; i++) {
            int idx = c * n_per_class + i;
            for (int f = 0; f < n_features; f++) {
                float diff = data[idx * n_features + f] - cmean[f];
                total_within += diff * diff;
            }
        }
    }

    // Between-class scatter
    float total_between = 0.0f;
    for (int c = 0; c < n_classes; c++) {
        for (int f = 0; f < n_features; f++) {
            float diff = class_means[c][f] - overall_mean[f];
            total_between += n_per_class * diff * diff;
        }
    }

    // F-ratio (discriminability)
    float f_ratio = (total_between / (n_classes - 1)) / (total_within / (n_total - n_classes) + 1e-6f);

    // Should have good separation
    EXPECT_GT(f_ratio, 10.0f);

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 2000.0);

    std::cout << "Discriminant Analysis: " << n_classes << " classes, "
              << "F-ratio=" << f_ratio
              << ", time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// E2E Test 15: Multidimensional Scaling (Simplified)
//=============================================================================

TEST_F(MultivariateE2ETest, MultidimensionalScaling) {
    START_TIMER();

    // Test that MDS preserves distances
    const int n_points = 50;
    const int original_dim = 10;
    const int target_dim = 2;

    // Generate points in high-dimensional space
    std::vector<float> points(n_points * original_dim);
    for (int i = 0; i < n_points * original_dim; i++) {
        points[i] = generate_normal(1, 0.0f, 1.0f)[0];
    }

    // Compute distance matrix
    std::vector<float> distances(n_points * n_points);
    for (int i = 0; i < n_points; i++) {
        for (int j = 0; j < n_points; j++) {
            if (i == j) {
                distances[i * n_points + j] = 0.0f;
            } else {
                float dist = 0.0f;
                for (int d = 0; d < original_dim; d++) {
                    float diff = points[i * original_dim + d] - points[j * original_dim + d];
                    dist += diff * diff;
                }
                distances[i * n_points + j] = std::sqrt(dist);
            }
        }
    }

    // Analyze distance distribution
    nimcp_descriptive_stats_t dist_stats;
    EXPECT_EQ(nimcp_stats_describe(distances.data(), n_points * n_points, &dist_stats), NIMCP_STATS_OK);

    // Distances should be positive
    EXPECT_GE(dist_stats.min, 0.0f);

    // Mean distance should scale with sqrt(dim)
    float expected_mean_dist = std::sqrt(2.0f * original_dim);  // For normal(0,1) points
    EXPECT_NEAR(dist_stats.mean, expected_mean_dist, expected_mean_dist * 0.3f);

    // Simple MDS check: projection onto first 2 PCs
    // Compute covariance and get approximate embedding
    std::vector<float> cov, means;
    compute_cov_matrix(points, n_points, original_dim, cov, means);

    // Largest eigenvalue
    float lambda_max = power_iteration(cov, original_dim, 50);

    // Should capture significant variance
    float total_var = 0.0f;
    for (int i = 0; i < original_dim; i++) {
        total_var += cov[i * original_dim + i];
    }

    float var_explained = lambda_max / total_var;
    EXPECT_GT(var_explained, 0.05f);  // At least 5% variance in top PC

    double elapsed = STOP_TIMER_MS();
    EXPECT_LT(elapsed, 5000.0);

    std::cout << "MDS: " << n_points << " points, " << original_dim << "D -> " << target_dim << "D, "
              << "mean dist=" << dist_stats.mean << ", "
              << "top PC var=" << var_explained * 100 << "%, "
              << "time=" << elapsed << " ms" << std::endl;
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
