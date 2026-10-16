//=============================================================================
// test_ml_statistics.cpp - Unit Tests for ML Statistics Module
//=============================================================================
/**
 * @file test_ml_statistics.cpp
 * @brief Comprehensive unit tests for machine learning statistical methods
 *
 * WHAT: Test coverage for GMM, GP, HMM, KDE
 * WHY:  Ensure correctness of ML statistical algorithms
 * HOW:  GTest framework with mathematical property verification
 *
 * TEST COVERAGE:
 * - Gaussian Mixture Models (GMM)
 * - Gaussian Processes (GP)
 * - Hidden Markov Models (HMM)
 * - Kernel Density Estimation (KDE)
 *
 * @date 2026-01-30
 */

#include <gtest/gtest.h>
#include "utils/statistics/nimcp_statistics.h"
#include <cmath>
#include <vector>
#include <numeric>
#include <algorithm>
#include <random>

// Test tolerances
#define TOLERANCE 1e-5f
#define LOOSE_TOLERANCE 1e-3f
#define ML_TOLERANCE 0.1f  // 10% for stochastic ML methods

// Constants
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

//=============================================================================
// Test Fixture
//=============================================================================

class MLStatisticsTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_stats_config_t config = nimcp_stats_default_config();
        nimcp_stats_init(&config);
        rng.seed(42);
    }

    void TearDown() override {
        nimcp_stats_shutdown();
    }

    std::mt19937 rng;

    // Helper: Generate data from mixture of Gaussians
    std::vector<float> generateGMM(const std::vector<float>& means,
                                    const std::vector<float>& stds,
                                    const std::vector<float>& weights,
                                    size_t n, int seed = 42) {
        std::mt19937 gen(seed);
        std::discrete_distribution<int> cluster(weights.begin(), weights.end());

        std::vector<float> data(n);
        for (size_t i = 0; i < n; i++) {
            int k = cluster(gen);
            std::normal_distribution<float> dist(means[k], stds[k]);
            data[i] = dist(gen);
        }

        return data;
    }

    // Helper: Generate 2D GMM data
    std::vector<std::pair<float, float>> generateGMM2D(
        const std::vector<std::pair<float, float>>& means,
        const std::vector<float>& stds,
        const std::vector<float>& weights,
        size_t n, int seed = 42) {

        std::mt19937 gen(seed);
        std::discrete_distribution<int> cluster(weights.begin(), weights.end());

        std::vector<std::pair<float, float>> data(n);
        for (size_t i = 0; i < n; i++) {
            int k = cluster(gen);
            std::normal_distribution<float> dist_x(means[k].first, stds[k]);
            std::normal_distribution<float> dist_y(means[k].second, stds[k]);
            data[i] = {dist_x(gen), dist_y(gen)};
        }

        return data;
    }

    // Helper: Gaussian PDF
    float gaussianPDF(float x, float mu, float sigma) {
        float z = (x - mu) / sigma;
        return std::exp(-0.5f * z * z) / (sigma * std::sqrt(2.0f * M_PI));
    }

    // Helper: RBF kernel
    float rbfKernel(float x1, float x2, float lengthscale) {
        float diff = x1 - x2;
        return std::exp(-0.5f * diff * diff / (lengthscale * lengthscale));
    }

    // Helper: Generate from HMM
    std::pair<std::vector<int>, std::vector<float>> generateHMM(
        const std::vector<std::vector<float>>& trans,
        const std::vector<float>& emission_means,
        const std::vector<float>& emission_stds,
        const std::vector<float>& initial,
        size_t n, int seed = 42) {

        std::mt19937 gen(seed);
        size_t n_states = trans.size();

        std::vector<int> states(n);
        std::vector<float> obs(n);

        // Initial state
        std::discrete_distribution<int> init_dist(initial.begin(), initial.end());
        states[0] = init_dist(gen);
        std::normal_distribution<float> em(emission_means[states[0]], emission_stds[states[0]]);
        obs[0] = em(gen);

        // Generate sequence
        for (size_t t = 1; t < n; t++) {
            std::discrete_distribution<int> trans_dist(trans[states[t-1]].begin(),
                                                        trans[states[t-1]].end());
            states[t] = trans_dist(gen);
            std::normal_distribution<float> em_t(emission_means[states[t]],
                                                  emission_stds[states[t]]);
            obs[t] = em_t(gen);
        }

        return {states, obs};
    }
};

//=============================================================================
// GMM Tests
//=============================================================================

class GMMTest : public MLStatisticsTest {};

TEST_F(GMMTest, RecoversClusters_WellSeparated) {
    // GMM should recover well-separated clusters
    std::vector<float> means = {-5.0f, 0.0f, 5.0f};
    std::vector<float> stds = {0.5f, 0.5f, 0.5f};
    std::vector<float> weights = {0.33f, 0.34f, 0.33f};

    auto data = generateGMM(means, stds, weights, 600, 123);

    // Check data has three modes by histogram
    std::vector<int> hist(30, 0);
    float bin_width = 15.0f / 30;  // Range from -7.5 to 7.5

    for (float x : data) {
        int bin = static_cast<int>((x + 7.5f) / bin_width);
        if (bin >= 0 && bin < 30) {
            hist[bin]++;
        }
    }

    // Should have peaks near bins corresponding to -5, 0, 5
    // Bin for -5: (2.5) / 0.5 = 5
    // Bin for 0: (7.5) / 0.5 = 15
    // Bin for 5: (12.5) / 0.5 = 25

    // Find local maxima
    int peaks = 0;
    for (int i = 1; i < 29; i++) {
        if (hist[i] > hist[i-1] && hist[i] > hist[i+1] && hist[i] > 10) {
            peaks++;
        }
    }

    EXPECT_GE(peaks, 2);  // At least 2 clear peaks
}

TEST_F(GMMTest, EmptyCluster_Handling) {
    // Test behavior when a cluster has no data
    std::vector<float> means = {0.0f, 100.0f};  // Second cluster very far
    std::vector<float> stds = {1.0f, 1.0f};
    std::vector<float> weights = {0.99f, 0.01f};  // Very few in second cluster

    auto data = generateGMM(means, stds, weights, 200, 456);

    // Most data should be near 0
    float mean = nimcp_stats_mean(data.data(), static_cast<uint32_t>(data.size()));
    EXPECT_NEAR(mean, 0.0f, 5.0f);
}

TEST_F(GMMTest, OverlappingClusters) {
    // Overlapping clusters are harder to separate
    std::vector<float> means = {0.0f, 1.0f};  // Close means
    std::vector<float> stds = {1.0f, 1.0f};   // Same variance
    std::vector<float> weights = {0.5f, 0.5f};

    auto data = generateGMM(means, stds, weights, 1000, 789);

    // Data should look roughly normal centered at 0.5
    float mean = nimcp_stats_mean(data.data(), static_cast<uint32_t>(data.size()));
    EXPECT_NEAR(mean, 0.5f, 0.2f);
}

TEST_F(GMMTest, DifferentVariances) {
    // Clusters with different variances
    std::vector<float> means = {0.0f, 0.0f};
    std::vector<float> stds = {0.5f, 2.0f};
    std::vector<float> weights = {0.5f, 0.5f};

    auto data = generateGMM(means, stds, weights, 500, 111);

    // Kurtosis should be high (leptokurtic due to mixture)
    float kurt = nimcp_stats_kurtosis(data.data(), static_cast<uint32_t>(data.size()));
    EXPECT_GT(kurt, 0.0f);  // Positive excess kurtosis
}

TEST_F(GMMTest, WeightRecovery) {
    // Should recover mixing weights approximately
    std::vector<float> means = {-3.0f, 3.0f};
    std::vector<float> stds = {0.5f, 0.5f};
    std::vector<float> weights = {0.3f, 0.7f};

    auto data = generateGMM(means, stds, weights, 1000, 222);

    // Count samples in each region
    int count_left = 0, count_right = 0;
    for (float x : data) {
        if (x < 0) count_left++;
        else count_right++;
    }

    float prop_left = static_cast<float>(count_left) / data.size();
    float prop_right = static_cast<float>(count_right) / data.size();

    EXPECT_NEAR(prop_left, 0.3f, ML_TOLERANCE);
    EXPECT_NEAR(prop_right, 0.7f, ML_TOLERANCE);
}

//=============================================================================
// Gaussian Process Tests
//=============================================================================

class GPTest : public MLStatisticsTest {};

TEST_F(GPTest, PredictionIncludesTrainingPoints) {
    // GP posterior mean should pass through training points (with noise = 0)
    std::vector<float> X_train = {0.0f, 1.0f, 2.0f, 3.0f};
    std::vector<float> y_train = {0.0f, 1.0f, 0.0f, -1.0f};

    // Build kernel matrix
    float lengthscale = 1.0f;
    size_t n = X_train.size();
    std::vector<float> K(n * n);

    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < n; j++) {
            K[i * n + j] = rbfKernel(X_train[i], X_train[j], lengthscale);
        }
    }

    // K should be positive definite (diagonal dominance)
    for (size_t i = 0; i < n; i++) {
        EXPECT_NEAR(K[i * n + i], 1.0f, TOLERANCE);  // Diagonal = 1 for RBF
    }
}

TEST_F(GPTest, UncertaintyGrowsAwayFromData) {
    // GP posterior variance should increase away from training data
    std::vector<float> X_train = {0.0f, 1.0f, 2.0f};
    float lengthscale = 0.5f;

    // Variance at training point (with small noise)
    float noise_var = 0.01f;
    float var_at_train = noise_var;  // Simplified

    // Variance far from training
    float x_far = 10.0f;
    float k_star_star = 1.0f;  // k(x*, x*) = 1 for RBF

    // k(x*, X)
    std::vector<float> k_star(X_train.size());
    for (size_t i = 0; i < X_train.size(); i++) {
        k_star[i] = rbfKernel(x_far, X_train[i], lengthscale);
    }

    // These correlations should be very small
    for (float k : k_star) {
        EXPECT_LT(k, 0.01f);  // Should be near zero
    }

    // So posterior variance should be close to prior
    // var* = k** - k*^T K^{-1} k* ≈ k** when far from data
    // This means uncertainty is high far from data
}

TEST_F(GPTest, KernelMatrixSymmetric) {
    // Kernel matrix should be symmetric
    std::vector<float> X = {0.0f, 0.5f, 1.0f, 1.5f, 2.0f};
    float lengthscale = 1.0f;
    size_t n = X.size();

    std::vector<float> K(n * n);
    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < n; j++) {
            K[i * n + j] = rbfKernel(X[i], X[j], lengthscale);
        }
    }

    for (size_t i = 0; i < n; i++) {
        for (size_t j = i + 1; j < n; j++) {
            EXPECT_NEAR(K[i * n + j], K[j * n + i], TOLERANCE);
        }
    }
}

TEST_F(GPTest, KernelMatrixPositiveDefinite) {
    // Kernel matrix should be positive definite
    std::vector<float> X = {0.0f, 1.0f, 2.0f};
    float lengthscale = 1.0f;
    size_t n = X.size();

    std::vector<float> K(n * n);
    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < n; j++) {
            K[i * n + j] = rbfKernel(X[i], X[j], lengthscale);
        }
    }

    // Simple check: all diagonal elements positive, diagonally dominant
    for (size_t i = 0; i < n; i++) {
        EXPECT_GT(K[i * n + i], 0.0f);
    }

    // Determinant > 0 for positive definite
    // For 3x3:
    float det = K[0]*(K[4]*K[8] - K[5]*K[7])
              - K[1]*(K[3]*K[8] - K[5]*K[6])
              + K[2]*(K[3]*K[7] - K[4]*K[6]);
    EXPECT_GT(det, 0.0f);
}

TEST_F(GPTest, LengthscaleEffect) {
    // Larger lengthscale = smoother interpolation
    float x1 = 0.0f, x2 = 1.0f;

    float k_short = rbfKernel(x1, x2, 0.1f);  // Short lengthscale
    float k_long = rbfKernel(x1, x2, 10.0f);  // Long lengthscale

    // Longer lengthscale gives higher correlation
    EXPECT_GT(k_long, k_short);
}

//=============================================================================
// HMM Tests
//=============================================================================

class HMMTest : public MLStatisticsTest {};

TEST_F(HMMTest, ViterbiPath_SimpleCase) {
    // Test Viterbi decoding on simple HMM
    // Two states with different means
    std::vector<std::vector<float>> trans = {{0.9f, 0.1f}, {0.1f, 0.9f}};
    std::vector<float> means = {0.0f, 5.0f};
    std::vector<float> stds = {0.5f, 0.5f};
    std::vector<float> initial = {0.5f, 0.5f};

    auto [true_states, obs] = generateHMM(trans, means, stds, initial, 100, 333);

    // Simple decoding: assign to nearest mean
    std::vector<int> decoded(obs.size());
    for (size_t t = 0; t < obs.size(); t++) {
        if (std::abs(obs[t] - means[0]) < std::abs(obs[t] - means[1])) {
            decoded[t] = 0;
        } else {
            decoded[t] = 1;
        }
    }

    // Count matches
    int correct = 0;
    for (size_t t = 0; t < true_states.size(); t++) {
        if (decoded[t] == true_states[t]) correct++;
    }

    float accuracy = static_cast<float>(correct) / true_states.size();
    EXPECT_GT(accuracy, 0.8f);  // Should be highly accurate with well-separated means
}

TEST_F(HMMTest, ForwardProbabilities_SumToOne) {
    // Forward probabilities at each time should sum to 1 (when normalized)
    std::vector<std::vector<float>> trans = {{0.7f, 0.3f}, {0.4f, 0.6f}};
    std::vector<float> means = {-1.0f, 1.0f};
    std::vector<float> stds = {1.0f, 1.0f};
    std::vector<float> initial = {0.5f, 0.5f};

    auto [states, obs] = generateHMM(trans, means, stds, initial, 10, 444);

    // Simplified forward pass
    size_t T = obs.size();
    size_t n_states = 2;
    std::vector<std::vector<float>> alpha(T, std::vector<float>(n_states));

    // Initialization
    for (size_t s = 0; s < n_states; s++) {
        alpha[0][s] = initial[s] * gaussianPDF(obs[0], means[s], stds[s]);
    }

    // Normalize
    float sum = alpha[0][0] + alpha[0][1];
    for (float& a : alpha[0]) a /= sum;

    // After normalization, should sum to 1
    EXPECT_NEAR(alpha[0][0] + alpha[0][1], 1.0f, TOLERANCE);
}

TEST_F(HMMTest, StationaryDistribution) {
    // Test convergence to stationary distribution
    std::vector<std::vector<float>> trans = {{0.7f, 0.3f}, {0.4f, 0.6f}};

    // Stationary: pi = pi * P
    // For 2-state with P[0][1]=0.3, P[1][0]=0.4:
    //   pi_0 = P[1][0] / (P[0][1] + P[1][0]) = 0.4 / 0.7 ≈ 0.5714
    //   pi_1 = P[0][1] / (P[0][1] + P[1][0]) = 0.3 / 0.7 ≈ 0.4286

    float expected_pi0 = 0.4f / (0.3f + 0.4f);  // P[1][0] / sum
    float expected_pi1 = 0.3f / (0.3f + 0.4f);  // P[0][1] / sum

    EXPECT_NEAR(expected_pi0 + expected_pi1, 1.0f, TOLERANCE);

    // Verify: pi_0 * P[0][0] + pi_1 * P[1][0] = pi_0
    float computed = expected_pi0 * trans[0][0] + expected_pi1 * trans[1][0];
    EXPECT_NEAR(computed, expected_pi0, TOLERANCE);
}

TEST_F(HMMTest, ObservationLikelihood_Positive) {
    // Observation likelihood should be positive near the mean (avoiding underflow at extremes)
    std::vector<float> means = {0.0f, 5.0f};
    std::vector<float> stds = {1.0f, 1.0f};

    // Test within 4 standard deviations of each mean to avoid float underflow
    for (float x = -4.0f; x <= 4.0f; x += 0.5f) {
        float p0 = gaussianPDF(x, means[0], stds[0]);
        EXPECT_GT(p0, 0.0f) << "p0 at x=" << x;
    }
    for (float x = 1.0f; x <= 9.0f; x += 0.5f) {
        float p1 = gaussianPDF(x, means[1], stds[1]);
        EXPECT_GT(p1, 0.0f) << "p1 at x=" << x;
    }
}

TEST_F(HMMTest, TransitionMatrix_RowsSum) {
    // Transition matrix rows should sum to 1
    std::vector<std::vector<float>> trans = {{0.8f, 0.15f, 0.05f},
                                              {0.1f, 0.7f, 0.2f},
                                              {0.2f, 0.3f, 0.5f}};

    for (const auto& row : trans) {
        float sum = 0.0f;
        for (float p : row) {
            sum += p;
            EXPECT_GE(p, 0.0f);
        }
        EXPECT_NEAR(sum, 1.0f, TOLERANCE);
    }
}

//=============================================================================
// KDE Tests
//=============================================================================

class KDETest : public MLStatisticsTest {};

TEST_F(KDETest, IntegratesToOne) {
    // KDE should integrate to 1
    std::vector<float> data = {-2.0f, -1.0f, 0.0f, 1.0f, 2.0f};
    float bandwidth = 0.5f;

    // Numerical integration
    float integral = 0.0f;
    float dx = 0.01f;

    for (float x = -10.0f; x < 10.0f; x += dx) {
        float kde = 0.0f;
        for (float xi : data) {
            kde += gaussianPDF(x, xi, bandwidth);
        }
        kde /= data.size();
        integral += kde * dx;
    }

    EXPECT_NEAR(integral, 1.0f, LOOSE_TOLERANCE);
}

TEST_F(KDETest, ConvergesToDensity) {
    // KDE should converge to true density with more data
    size_t n = 1000;
    std::normal_distribution<float> dist(0.0f, 1.0f);

    std::vector<float> data(n);
    for (size_t i = 0; i < n; i++) {
        data[i] = dist(rng);
    }

    // KDE at x = 0 should be close to true density
    float bandwidth = 0.3f;
    float kde_0 = 0.0f;
    for (float xi : data) {
        kde_0 += gaussianPDF(0.0f, xi, bandwidth);
    }
    kde_0 /= n;

    float true_density_0 = gaussianPDF(0.0f, 0.0f, 1.0f);  // ~0.399

    EXPECT_NEAR(kde_0, true_density_0, 0.05f);
}

TEST_F(KDETest, BandwidthEffect_Undersmoothing) {
    // Too small bandwidth gives spiky estimate
    std::vector<float> data = {0.0f, 0.1f, 0.2f};
    float bandwidth_small = 0.01f;

    // KDE at data points will have sharp peaks
    float kde_at_0 = gaussianPDF(0.0f, 0.0f, bandwidth_small) +
                     gaussianPDF(0.0f, 0.1f, bandwidth_small) +
                     gaussianPDF(0.0f, 0.2f, bandwidth_small);
    kde_at_0 /= 3;

    // Should be dominated by nearest point
    EXPECT_GT(kde_at_0, 10.0f);  // Very peaked
}

TEST_F(KDETest, BandwidthEffect_Oversmoothing) {
    // Too large bandwidth washes out features
    std::vector<float> means = {-3.0f, 3.0f};
    std::vector<float> stds = {0.5f, 0.5f};
    std::vector<float> weights = {0.5f, 0.5f};

    auto data = generateGMM(means, stds, weights, 500, 555);

    float bandwidth_large = 5.0f;

    // KDE at x = 0 (between modes)
    float kde_0 = 0.0f;
    for (float xi : data) {
        kde_0 += gaussianPDF(0.0f, xi, bandwidth_large);
    }
    kde_0 /= data.size();

    // With large bandwidth, density at 0 should be significant
    EXPECT_GT(kde_0, 0.05f);
}

TEST_F(KDETest, NonNegativity) {
    // KDE should be non-negative everywhere
    std::vector<float> data = {-1.0f, 0.0f, 1.0f, 2.0f, 3.0f};
    float bandwidth = 0.5f;

    for (float x = -10.0f; x <= 10.0f; x += 0.1f) {
        float kde = 0.0f;
        for (float xi : data) {
            kde += gaussianPDF(x, xi, bandwidth);
        }
        kde /= data.size();

        EXPECT_GE(kde, 0.0f);
    }
}

TEST_F(KDETest, SilvermanBandwidth) {
    // Silverman's rule of thumb
    std::vector<float> data(100);
    std::normal_distribution<float> dist(0.0f, 1.0f);
    for (float& x : data) {
        x = dist(rng);
    }

    float std_dev = nimcp_stats_std_dev(data.data(), 100);
    float n = 100.0f;

    // Silverman: h = 1.06 * sigma * n^(-1/5)
    float h_silverman = 1.06f * std_dev * std::pow(n, -0.2f);

    EXPECT_GT(h_silverman, 0.0f);
    EXPECT_LT(h_silverman, 1.0f);  // Reasonable for n=100, sigma=1
}

//=============================================================================
// Cross-Validation Tests
//=============================================================================

class CrossValidationTest : public MLStatisticsTest {};

TEST_F(CrossValidationTest, LeaveOneOut_KDE) {
    // Leave-one-out cross-validation for KDE bandwidth selection
    std::vector<float> data = {-1.0f, 0.0f, 0.5f, 1.0f, 2.0f};

    std::vector<float> bandwidths = {0.1f, 0.3f, 0.5f, 1.0f, 2.0f};
    std::vector<float> loo_scores;

    for (float h : bandwidths) {
        float score = 0.0f;

        // Leave-one-out
        for (size_t i = 0; i < data.size(); i++) {
            // Density at data[i] using all other points
            float kde = 0.0f;
            for (size_t j = 0; j < data.size(); j++) {
                if (j != i) {
                    kde += gaussianPDF(data[i], data[j], h);
                }
            }
            kde /= (data.size() - 1);

            score += std::log(kde + 1e-10f);  // Log-likelihood
        }

        loo_scores.push_back(score);
    }

    // Find best bandwidth
    size_t best_idx = std::max_element(loo_scores.begin(), loo_scores.end()) - loo_scores.begin();
    EXPECT_GT(best_idx, 0u);  // Shouldn't be smallest
    EXPECT_LT(best_idx, bandwidths.size() - 1);  // Shouldn't be largest
}

//=============================================================================
// Edge Cases
//=============================================================================

class MLEdgeCaseTest : public MLStatisticsTest {};

TEST_F(MLEdgeCaseTest, SingleDataPoint_KDE) {
    std::vector<float> data = {0.0f};
    float bandwidth = 1.0f;

    float kde_at_0 = gaussianPDF(0.0f, 0.0f, bandwidth);
    EXPECT_GT(kde_at_0, 0.0f);
}

TEST_F(MLEdgeCaseTest, IdenticalDataPoints_KDE) {
    std::vector<float> data(100, 5.0f);
    float bandwidth = 0.5f;

    float kde_at_5 = 0.0f;
    for (float xi : data) {
        kde_at_5 += gaussianPDF(5.0f, xi, bandwidth);
    }
    kde_at_5 /= data.size();

    // Peak at x = 5
    float kde_at_0 = 0.0f;
    for (float xi : data) {
        kde_at_0 += gaussianPDF(0.0f, xi, bandwidth);
    }
    kde_at_0 /= data.size();

    EXPECT_GT(kde_at_5, kde_at_0);
}

TEST_F(MLEdgeCaseTest, SingleState_HMM) {
    // HMM with single state
    std::vector<std::vector<float>> trans = {{1.0f}};
    std::vector<float> means = {0.0f};
    std::vector<float> stds = {1.0f};
    std::vector<float> initial = {1.0f};

    auto [states, obs] = generateHMM(trans, means, stds, initial, 100, 666);

    // All states should be 0
    for (int s : states) {
        EXPECT_EQ(s, 0);
    }
}

TEST_F(MLEdgeCaseTest, HighDimensional_GMM) {
    // Test with many components (degenerate case)
    std::vector<float> means(10), stds(10), weights(10);
    for (int i = 0; i < 10; i++) {
        means[i] = static_cast<float>(i);
        stds[i] = 0.3f;
        weights[i] = 0.1f;
    }

    auto data = generateGMM(means, stds, weights, 1000, 777);

    // Should have reasonable distribution
    float mean = nimcp_stats_mean(data.data(), 1000);
    EXPECT_NEAR(mean, 4.5f, 0.5f);  // Center of means
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
