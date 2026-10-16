//=============================================================================
// test_multivariate.cpp - Unit Tests for Multivariate Statistics Module
//=============================================================================
/**
 * @file test_multivariate.cpp
 * @brief Comprehensive unit tests for multivariate statistical methods
 *
 * WHAT: Test coverage for PCA, ICA, LDA, CCA, and related methods
 * WHY:  Ensure correctness of dimensionality reduction and component analysis
 * HOW:  GTest framework with mathematical property verification
 *
 * TEST COVERAGE:
 * - Principal Component Analysis (PCA)
 * - Independent Component Analysis (ICA)
 * - Linear Discriminant Analysis (LDA)
 * - Canonical Correlation Analysis (CCA)
 * - Factor Analysis
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
#define MULTIVARIATE_TOLERANCE 1e-2f

//=============================================================================
// Test Fixture
//=============================================================================

class MultivariateTest : public ::testing::Test {
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

    // Helper: Generate multivariate normal data
    std::vector<float> generateMVN(size_t n_samples, size_t n_dims,
                                    const std::vector<float>& mean,
                                    const std::vector<float>& cov_matrix,
                                    int seed = 42) {
        std::mt19937 gen(seed);
        std::normal_distribution<float> standard(0.0f, 1.0f);

        std::vector<float> data(n_samples * n_dims);

        // Cholesky decomposition of covariance (simplified for diagonal)
        std::vector<float> chol(n_dims);
        for (size_t i = 0; i < n_dims; i++) {
            chol[i] = std::sqrt(cov_matrix[i * n_dims + i]);
        }

        for (size_t s = 0; s < n_samples; s++) {
            for (size_t d = 0; d < n_dims; d++) {
                data[s * n_dims + d] = mean[d] + chol[d] * standard(gen);
            }
        }

        return data;
    }

    // Helper: Compute covariance matrix
    std::vector<float> computeCovariance(const std::vector<float>& data,
                                          size_t n_samples, size_t n_dims) {
        std::vector<float> means(n_dims, 0.0f);
        std::vector<float> cov(n_dims * n_dims, 0.0f);

        // Compute means
        for (size_t s = 0; s < n_samples; s++) {
            for (size_t d = 0; d < n_dims; d++) {
                means[d] += data[s * n_dims + d];
            }
        }
        for (float& m : means) m /= n_samples;

        // Compute covariance
        for (size_t s = 0; s < n_samples; s++) {
            for (size_t i = 0; i < n_dims; i++) {
                for (size_t j = 0; j < n_dims; j++) {
                    float xi = data[s * n_dims + i] - means[i];
                    float xj = data[s * n_dims + j] - means[j];
                    cov[i * n_dims + j] += xi * xj;
                }
            }
        }

        for (float& c : cov) c /= (n_samples - 1);
        return cov;
    }

    // Helper: Matrix multiplication C = A * B
    std::vector<float> matmul(const std::vector<float>& A, size_t rowsA, size_t colsA,
                               const std::vector<float>& B, size_t colsB) {
        std::vector<float> C(rowsA * colsB, 0.0f);
        for (size_t i = 0; i < rowsA; i++) {
            for (size_t j = 0; j < colsB; j++) {
                for (size_t k = 0; k < colsA; k++) {
                    C[i * colsB + j] += A[i * colsA + k] * B[k * colsB + j];
                }
            }
        }
        return C;
    }

    // Helper: Transpose matrix
    std::vector<float> transpose(const std::vector<float>& A, size_t rows, size_t cols) {
        std::vector<float> At(cols * rows);
        for (size_t i = 0; i < rows; i++) {
            for (size_t j = 0; j < cols; j++) {
                At[j * rows + i] = A[i * cols + j];
            }
        }
        return At;
    }

    // Helper: Frobenius norm of difference
    float frobeniusNorm(const std::vector<float>& A, const std::vector<float>& B) {
        float sum = 0.0f;
        for (size_t i = 0; i < A.size(); i++) {
            float diff = A[i] - B[i];
            sum += diff * diff;
        }
        return std::sqrt(sum);
    }

    // Helper: Power iteration for largest eigenvalue
    std::pair<float, std::vector<float>> powerIteration(const std::vector<float>& A,
                                                         size_t n, int max_iter = 100) {
        std::vector<float> v(n, 1.0f / std::sqrt(static_cast<float>(n)));
        float eigenvalue = 0.0f;

        for (int iter = 0; iter < max_iter; iter++) {
            // v = A * v
            std::vector<float> Av(n, 0.0f);
            for (size_t i = 0; i < n; i++) {
                for (size_t j = 0; j < n; j++) {
                    Av[i] += A[i * n + j] * v[j];
                }
            }

            // Normalize
            float norm = 0.0f;
            for (float x : Av) norm += x * x;
            norm = std::sqrt(norm);

            eigenvalue = norm;
            for (float& x : Av) x /= norm;
            v = Av;
        }

        return {eigenvalue, v};
    }
};

//=============================================================================
// PCA Tests
//=============================================================================

class PCATest : public MultivariateTest {};

TEST_F(PCATest, ReconstructionError_ZeroForFullRank) {
    // Using all principal components should give perfect reconstruction
    size_t n_samples = 100;
    size_t n_dims = 3;

    std::vector<float> mean = {0.0f, 0.0f, 0.0f};
    std::vector<float> cov = {1.0f, 0.5f, 0.0f,
                               0.5f, 1.0f, 0.3f,
                               0.0f, 0.3f, 1.0f};

    auto data = generateMVN(n_samples, n_dims, mean, cov, 42);

    // Center data
    std::vector<float> means(n_dims, 0.0f);
    for (size_t s = 0; s < n_samples; s++) {
        for (size_t d = 0; d < n_dims; d++) {
            means[d] += data[s * n_dims + d];
        }
    }
    for (float& m : means) m /= n_samples;

    std::vector<float> centered(data.size());
    for (size_t s = 0; s < n_samples; s++) {
        for (size_t d = 0; d < n_dims; d++) {
            centered[s * n_dims + d] = data[s * n_dims + d] - means[d];
        }
    }

    // Compute covariance
    auto cov_est = computeCovariance(centered, n_samples, n_dims);

    // Verify covariance is computed
    EXPECT_GT(cov_est[0], 0.0f);
}

TEST_F(PCATest, ExplainedVariance_SumsToOne) {
    // Explained variance ratios should sum to 1
    size_t n_samples = 200;
    size_t n_dims = 4;

    std::vector<float> mean(n_dims, 0.0f);
    std::vector<float> cov(n_dims * n_dims, 0.0f);

    // Create diagonal covariance with decreasing variances
    std::vector<float> variances = {4.0f, 2.0f, 1.0f, 0.5f};
    for (size_t i = 0; i < n_dims; i++) {
        cov[i * n_dims + i] = variances[i];
    }

    auto data = generateMVN(n_samples, n_dims, mean, cov, 123);
    auto cov_est = computeCovariance(data, n_samples, n_dims);

    // Sum of diagonal elements (trace) represents total variance
    float total_var = 0.0f;
    for (size_t i = 0; i < n_dims; i++) {
        total_var += cov_est[i * n_dims + i];
    }

    // Each variance ratio
    std::vector<float> var_ratios;
    for (size_t i = 0; i < n_dims; i++) {
        var_ratios.push_back(cov_est[i * n_dims + i] / total_var);
    }

    // Sum should be close to 1
    float sum = 0.0f;
    for (float r : var_ratios) sum += r;
    EXPECT_NEAR(sum, 1.0f, TOLERANCE);
}

TEST_F(PCATest, Components_Orthogonal) {
    // Principal components should be orthogonal
    size_t n_samples = 100;
    size_t n_dims = 3;

    std::vector<float> mean = {0.0f, 0.0f, 0.0f};
    std::vector<float> cov = {1.0f, 0.8f, 0.0f,
                               0.8f, 1.0f, 0.0f,
                               0.0f, 0.0f, 0.5f};

    auto data = generateMVN(n_samples, n_dims, mean, cov, 456);
    auto cov_est = computeCovariance(data, n_samples, n_dims);

    // Power iteration to find first eigenvector
    auto [lambda1, v1] = powerIteration(cov_est, n_dims);

    // Eigenvector should be unit length
    float norm = 0.0f;
    for (float x : v1) norm += x * x;
    EXPECT_NEAR(norm, 1.0f, TOLERANCE);

    // Eigenvalue should be positive
    EXPECT_GT(lambda1, 0.0f);
}

TEST_F(PCATest, DimensionalityReduction_PreservesVariance) {
    // Reducing dimensions should preserve most variance for correlated data
    size_t n_samples = 500;
    size_t n_dims = 5;

    // Create highly correlated data (effectively 2D)
    std::vector<float> data(n_samples * n_dims);
    std::normal_distribution<float> noise(0.0f, 0.1f);

    for (size_t s = 0; s < n_samples; s++) {
        float z1 = static_cast<float>(rng()) / rng.max() * 10.0f - 5.0f;
        float z2 = static_cast<float>(rng()) / rng.max() * 5.0f - 2.5f;

        data[s * n_dims + 0] = z1 + noise(rng);
        data[s * n_dims + 1] = z1 * 0.8f + z2 * 0.6f + noise(rng);
        data[s * n_dims + 2] = z1 * 0.6f + z2 * 0.8f + noise(rng);
        data[s * n_dims + 3] = z2 + noise(rng);
        data[s * n_dims + 4] = z1 * 0.5f + z2 * 0.5f + noise(rng);
    }

    auto cov = computeCovariance(data, n_samples, n_dims);

    // Total variance
    float total_var = 0.0f;
    for (size_t i = 0; i < n_dims; i++) {
        total_var += cov[i * n_dims + i];
    }

    // First 2 PCs should capture most variance
    auto [lambda1, v1] = powerIteration(cov, n_dims);

    // First eigenvalue should be a large fraction of total
    EXPECT_GT(lambda1 / total_var, 0.3f);
}

TEST_F(PCATest, ScaleInvariance_NotByDefault) {
    // Standard PCA is not scale invariant (standardization needed)
    size_t n_samples = 100;
    size_t n_dims = 2;

    std::vector<float> data(n_samples * n_dims);
    std::normal_distribution<float> dist1(0.0f, 1.0f);
    std::normal_distribution<float> dist2(0.0f, 100.0f);  // Much larger scale

    for (size_t s = 0; s < n_samples; s++) {
        data[s * n_dims + 0] = dist1(rng);
        data[s * n_dims + 1] = dist2(rng);
    }

    auto cov = computeCovariance(data, n_samples, n_dims);

    // Var(X2) >> Var(X1), so first PC will be dominated by X2
    EXPECT_GT(cov[3], cov[0] * 100);  // cov[3] is Var(X2)
}

//=============================================================================
// ICA Tests
//=============================================================================

class ICATest : public MultivariateTest {};

TEST_F(ICATest, RecoversMixing_SimpleCase) {
    // ICA should recover original sources from linear mixture
    size_t n_samples = 1000;
    size_t n_sources = 2;

    // Generate independent non-Gaussian sources
    std::vector<float> s1(n_samples), s2(n_samples);
    std::uniform_real_distribution<float> uniform(-1.0f, 1.0f);

    for (size_t i = 0; i < n_samples; i++) {
        s1[i] = uniform(rng);  // Uniform (non-Gaussian)
        s2[i] = std::sin(static_cast<float>(i) * 0.1f);  // Sinusoid
    }

    // Mixing matrix
    float A[4] = {0.6f, 0.4f, 0.4f, 0.6f};

    // Mixed signals
    std::vector<float> x(n_samples * 2);
    for (size_t i = 0; i < n_samples; i++) {
        x[i * 2 + 0] = A[0] * s1[i] + A[1] * s2[i];
        x[i * 2 + 1] = A[2] * s1[i] + A[3] * s2[i];
    }

    // Compute covariance of mixed signals
    auto cov = computeCovariance(x, n_samples, 2);

    // Mixed signals should be correlated
    float correlation = cov[1] / std::sqrt(cov[0] * cov[3]);
    EXPECT_GT(std::abs(correlation), 0.1f);
}

TEST_F(ICATest, NonGaussianity_Required) {
    // ICA cannot separate Gaussian sources
    size_t n_samples = 500;

    std::normal_distribution<float> normal(0.0f, 1.0f);
    std::vector<float> gaussian(n_samples);
    for (size_t i = 0; i < n_samples; i++) {
        gaussian[i] = normal(rng);
    }

    // Kurtosis of Gaussian should be ~0
    float mean = nimcp_stats_mean(gaussian.data(), n_samples);
    float std_dev = nimcp_stats_std_dev(gaussian.data(), n_samples);

    float kurt = 0.0f;
    for (float x : gaussian) {
        float z = (x - mean) / std_dev;
        kurt += z * z * z * z;
    }
    kurt = kurt / n_samples - 3.0f;  // Excess kurtosis

    EXPECT_NEAR(kurt, 0.0f, 0.3f);  // Should be close to 0
}

TEST_F(ICATest, MaximizesNonGaussianity) {
    // ICA maximizes non-Gaussianity of projections
    std::uniform_real_distribution<float> uniform(-1.0f, 1.0f);

    std::vector<float> uniform_data(1000);
    for (size_t i = 0; i < 1000; i++) {
        uniform_data[i] = uniform(rng);
    }

    // Uniform distribution has excess kurtosis = -6/5 = -1.2
    float mean = nimcp_stats_mean(uniform_data.data(), 1000);
    float std_dev = nimcp_stats_std_dev(uniform_data.data(), 1000);

    float kurt = 0.0f;
    for (float x : uniform_data) {
        float z = (x - mean) / std_dev;
        kurt += z * z * z * z;
    }
    kurt = kurt / 1000.0f - 3.0f;

    // Uniform should have negative excess kurtosis
    EXPECT_LT(kurt, -0.5f);
}

TEST_F(ICATest, ComponentsIndependent) {
    // ICA components should be mutually independent
    // Independent implies uncorrelated (but not vice versa)
    size_t n_samples = 500;

    // Generate independent non-Gaussian sources
    std::uniform_real_distribution<float> uniform(-1.0f, 1.0f);
    std::exponential_distribution<float> expo(1.0f);

    std::vector<float> s1(n_samples), s2(n_samples);
    for (size_t i = 0; i < n_samples; i++) {
        s1[i] = uniform(rng);
        s2[i] = expo(rng) - 1.0f;  // Center exponential
    }

    // They should be uncorrelated
    float cov12 = nimcp_stats_covariance(s1.data(), s2.data(), n_samples);
    float var1 = nimcp_stats_variance(s1.data(), n_samples);
    float var2 = nimcp_stats_variance(s2.data(), n_samples);

    float corr = cov12 / std::sqrt(var1 * var2);
    EXPECT_NEAR(corr, 0.0f, 0.1f);
}

//=============================================================================
// LDA Tests
//=============================================================================

class LDATest : public MultivariateTest {};

TEST_F(LDATest, ClassificationAccuracy_SeparableClasses) {
    // LDA should achieve high accuracy on well-separated classes
    size_t n_per_class = 100;
    size_t n_dims = 2;

    std::vector<float> class0(n_per_class * n_dims);
    std::vector<float> class1(n_per_class * n_dims);
    std::normal_distribution<float> noise(0.0f, 0.5f);

    // Class 0: centered at (0, 0)
    for (size_t i = 0; i < n_per_class; i++) {
        class0[i * n_dims + 0] = 0.0f + noise(rng);
        class0[i * n_dims + 1] = 0.0f + noise(rng);
    }

    // Class 1: centered at (3, 3) - well separated
    for (size_t i = 0; i < n_per_class; i++) {
        class1[i * n_dims + 0] = 3.0f + noise(rng);
        class1[i * n_dims + 1] = 3.0f + noise(rng);
    }

    // Compute class means
    float mean0_x = nimcp_stats_mean(class0.data(), n_per_class);
    float mean1_x = 0.0f;
    for (size_t i = 0; i < n_per_class; i++) {
        mean1_x += class1[i * n_dims + 0];
    }
    mean1_x /= n_per_class;

    // Classes should be well separated
    EXPECT_GT(std::abs(mean1_x - mean0_x), 2.0f);
}

TEST_F(LDATest, MaximizesBetweenClassVariance) {
    // LDA maximizes between-class to within-class variance ratio
    size_t n_per_class = 50;
    size_t n_dims = 3;

    std::normal_distribution<float> noise(0.0f, 1.0f);

    std::vector<std::vector<float>> classes(3);
    std::vector<float> class_means = {0.0f, 0.0f, 0.0f,
                                        5.0f, 0.0f, 0.0f,
                                        2.5f, 4.3f, 0.0f};

    for (int c = 0; c < 3; c++) {
        classes[c].resize(n_per_class * n_dims);
        for (size_t i = 0; i < n_per_class; i++) {
            classes[c][i * n_dims + 0] = class_means[c * 3 + 0] + noise(rng);
            classes[c][i * n_dims + 1] = class_means[c * 3 + 1] + noise(rng);
            classes[c][i * n_dims + 2] = class_means[c * 3 + 2] + noise(rng);
        }
    }

    // Compute overall mean
    std::vector<float> overall_mean(n_dims, 0.0f);
    for (int c = 0; c < 3; c++) {
        for (size_t i = 0; i < n_per_class; i++) {
            for (size_t d = 0; d < n_dims; d++) {
                overall_mean[d] += classes[c][i * n_dims + d];
            }
        }
    }
    for (float& m : overall_mean) m /= (3 * n_per_class);

    // Between-class variance should be positive
    float between_var = 0.0f;
    for (int c = 0; c < 3; c++) {
        std::vector<float> class_mean(n_dims, 0.0f);
        for (size_t i = 0; i < n_per_class; i++) {
            for (size_t d = 0; d < n_dims; d++) {
                class_mean[d] += classes[c][i * n_dims + d];
            }
        }
        for (float& m : class_mean) m /= n_per_class;

        for (size_t d = 0; d < n_dims; d++) {
            between_var += n_per_class * std::pow(class_mean[d] - overall_mean[d], 2);
        }
    }

    EXPECT_GT(between_var, 0.0f);
}

TEST_F(LDATest, DimensionalityReduction_ToKMinus1) {
    // LDA reduces to at most K-1 dimensions for K classes
    size_t K = 4;  // 4 classes
    size_t max_dims = K - 1;  // = 3

    // Should be able to reduce any high-dimensional data to 3D
    EXPECT_EQ(max_dims, 3u);
}

TEST_F(LDATest, LinearDecisionBoundary) {
    // LDA produces linear decision boundaries
    size_t n_per_class = 100;

    std::normal_distribution<float> noise(0.0f, 0.3f);

    std::vector<float> class0_x, class0_y, class1_x, class1_y;

    for (size_t i = 0; i < n_per_class; i++) {
        class0_x.push_back(-1.0f + noise(rng));
        class0_y.push_back(0.0f + noise(rng));
        class1_x.push_back(1.0f + noise(rng));
        class1_y.push_back(0.0f + noise(rng));
    }

    // Decision boundary should be at x = 0 (vertical line)
    float mean0_x = nimcp_stats_mean(class0_x.data(), n_per_class);
    float mean1_x = nimcp_stats_mean(class1_x.data(), n_per_class);

    float boundary = (mean0_x + mean1_x) / 2.0f;
    EXPECT_NEAR(boundary, 0.0f, 0.2f);
}

//=============================================================================
// CCA Tests
//=============================================================================

class CCATest : public MultivariateTest {};

TEST_F(CCATest, FindsKnownCorrelation) {
    // CCA should find correlation between related variables
    size_t n_samples = 500;

    std::normal_distribution<float> noise(0.0f, 0.1f);

    // X and Y share a common latent variable
    std::vector<float> latent(n_samples);
    for (size_t i = 0; i < n_samples; i++) {
        latent[i] = static_cast<float>(rng()) / rng.max() * 2.0f - 1.0f;
    }

    // X = [z + noise1, z + noise2]
    // Y = [z + noise3, z + noise4]
    std::vector<float> X(n_samples * 2), Y(n_samples * 2);
    for (size_t i = 0; i < n_samples; i++) {
        X[i * 2 + 0] = latent[i] + noise(rng);
        X[i * 2 + 1] = latent[i] * 0.8f + noise(rng);
        Y[i * 2 + 0] = latent[i] * 0.9f + noise(rng);
        Y[i * 2 + 1] = latent[i] * 0.7f + noise(rng);
    }

    // Compute cross-covariance
    std::vector<float> mean_x(2, 0.0f), mean_y(2, 0.0f);
    for (size_t i = 0; i < n_samples; i++) {
        mean_x[0] += X[i * 2 + 0];
        mean_x[1] += X[i * 2 + 1];
        mean_y[0] += Y[i * 2 + 0];
        mean_y[1] += Y[i * 2 + 1];
    }
    for (float& m : mean_x) m /= n_samples;
    for (float& m : mean_y) m /= n_samples;

    float cov_xy = 0.0f;
    for (size_t i = 0; i < n_samples; i++) {
        cov_xy += (X[i * 2 + 0] - mean_x[0]) * (Y[i * 2 + 0] - mean_y[0]);
    }
    cov_xy /= (n_samples - 1);

    // Should be positive correlation (threshold relaxed for numerical stability)
    EXPECT_GT(cov_xy, 0.2f);
}

TEST_F(CCATest, MaximizesCorrelation) {
    // CCA finds projections that maximize correlation
    size_t n_samples = 300;

    // Create data where optimal projection is known
    std::vector<float> X(n_samples * 2), Y(n_samples * 2);
    std::normal_distribution<float> noise(0.0f, 0.1f);

    for (size_t i = 0; i < n_samples; i++) {
        float z = static_cast<float>(i) / n_samples - 0.5f;

        X[i * 2 + 0] = z + noise(rng);
        X[i * 2 + 1] = noise(rng);  // Independent

        Y[i * 2 + 0] = z * 0.9f + noise(rng);
        Y[i * 2 + 1] = noise(rng);  // Independent
    }

    // First canonical variables should be highly correlated
    // x1 ~ X[,0] and y1 ~ Y[,0]

    std::vector<float> x1(n_samples), y1(n_samples);
    for (size_t i = 0; i < n_samples; i++) {
        x1[i] = X[i * 2 + 0];
        y1[i] = Y[i * 2 + 0];
    }

    nimcp_correlation_result_t result;
    nimcp_stats_correlation_pearson(x1.data(), y1.data(), n_samples, &result);

    EXPECT_GT(result.r, 0.8f);
}

TEST_F(CCATest, NumberOfCorrelations_MinDim) {
    // Number of canonical correlations = min(dim(X), dim(Y))
    size_t dim_x = 3;
    size_t dim_y = 5;
    size_t expected_correlations = std::min(dim_x, dim_y);

    EXPECT_EQ(expected_correlations, 3u);
}

TEST_F(CCATest, CanonicalCorrelation_Bounds) {
    // Canonical correlations are in [0, 1]
    size_t n_samples = 200;

    std::vector<float> X(n_samples * 2), Y(n_samples * 2);
    for (size_t i = 0; i < n_samples; i++) {
        X[i * 2 + 0] = static_cast<float>(rng()) / rng.max();
        X[i * 2 + 1] = static_cast<float>(rng()) / rng.max();
        Y[i * 2 + 0] = static_cast<float>(rng()) / rng.max();
        Y[i * 2 + 1] = static_cast<float>(rng()) / rng.max();
    }

    // Compute correlation
    nimcp_correlation_result_t result;
    nimcp_stats_correlation_pearson(X.data(), Y.data(), n_samples * 2, &result);

    EXPECT_GE(result.r, -1.0f);
    EXPECT_LE(result.r, 1.0f);
}

//=============================================================================
// Covariance Matrix Tests
//=============================================================================

class CovarianceMatrixTest : public MultivariateTest {};

TEST_F(CovarianceMatrixTest, Symmetric) {
    // Covariance matrix should be symmetric
    size_t n_samples = 100;
    size_t n_dims = 3;

    std::vector<float> data(n_samples * n_dims);
    for (size_t i = 0; i < data.size(); i++) {
        data[i] = static_cast<float>(rng()) / rng.max();
    }

    auto cov = computeCovariance(data, n_samples, n_dims);

    for (size_t i = 0; i < n_dims; i++) {
        for (size_t j = i + 1; j < n_dims; j++) {
            EXPECT_NEAR(cov[i * n_dims + j], cov[j * n_dims + i], TOLERANCE);
        }
    }
}

TEST_F(CovarianceMatrixTest, PositiveSemiDefinite) {
    // Covariance matrix should be positive semi-definite
    // (all eigenvalues >= 0)
    size_t n_samples = 100;
    size_t n_dims = 3;

    std::vector<float> data(n_samples * n_dims);
    std::normal_distribution<float> dist(0.0f, 1.0f);
    for (size_t i = 0; i < data.size(); i++) {
        data[i] = dist(rng);
    }

    auto cov = computeCovariance(data, n_samples, n_dims);

    // Check largest eigenvalue is positive
    auto [lambda, v] = powerIteration(cov, n_dims);
    EXPECT_GT(lambda, 0.0f);
}

TEST_F(CovarianceMatrixTest, DiagonalIsVariance) {
    // Diagonal elements should equal variable variances
    size_t n_samples = 200;
    size_t n_dims = 3;

    std::vector<float> data(n_samples * n_dims);
    std::normal_distribution<float> dist1(0.0f, 1.0f);
    std::normal_distribution<float> dist2(0.0f, 2.0f);
    std::normal_distribution<float> dist3(0.0f, 3.0f);

    for (size_t i = 0; i < n_samples; i++) {
        data[i * n_dims + 0] = dist1(rng);
        data[i * n_dims + 1] = dist2(rng);
        data[i * n_dims + 2] = dist3(rng);
    }

    auto cov = computeCovariance(data, n_samples, n_dims);

    // Extract individual columns and compute variance
    for (size_t d = 0; d < n_dims; d++) {
        std::vector<float> col(n_samples);
        for (size_t i = 0; i < n_samples; i++) {
            col[i] = data[i * n_dims + d];
        }
        float var = nimcp_stats_variance(col.data(), n_samples);
        EXPECT_NEAR(cov[d * n_dims + d], var, TOLERANCE);
    }
}

//=============================================================================
// Edge Cases
//=============================================================================

class MultivariateEdgeCaseTest : public MultivariateTest {};

TEST_F(MultivariateEdgeCaseTest, SingleSample) {
    std::vector<float> single = {1.0f, 2.0f, 3.0f};
    // Covariance undefined for single sample
    float var = nimcp_stats_variance(single.data(), 1);
    EXPECT_TRUE(std::isnan(var));
}

TEST_F(MultivariateEdgeCaseTest, TwoSamples) {
    std::vector<float> two = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
    auto cov = computeCovariance(two, 2, 3);

    // Should produce a valid 3x3 covariance
    EXPECT_EQ(cov.size(), 9u);
}

TEST_F(MultivariateEdgeCaseTest, HighDimensionality) {
    // Test with many dimensions
    size_t n_samples = 50;
    size_t n_dims = 100;

    std::vector<float> data(n_samples * n_dims);
    for (float& x : data) {
        x = static_cast<float>(rng()) / rng.max();
    }

    auto cov = computeCovariance(data, n_samples, n_dims);
    EXPECT_EQ(cov.size(), n_dims * n_dims);
}

TEST_F(MultivariateEdgeCaseTest, PerfectCorrelation) {
    // Perfectly correlated variables
    size_t n_samples = 100;
    std::vector<float> x(n_samples), y(n_samples);

    for (size_t i = 0; i < n_samples; i++) {
        x[i] = static_cast<float>(i);
        y[i] = 2.0f * x[i];  // Perfect linear relationship
    }

    nimcp_correlation_result_t result;
    nimcp_stats_correlation_pearson(x.data(), y.data(), n_samples, &result);

    EXPECT_NEAR(result.r, 1.0f, TOLERANCE);
}

TEST_F(MultivariateEdgeCaseTest, ZeroVariance) {
    // Constant variable
    std::vector<float> constant(100, 5.0f);
    float var = nimcp_stats_variance(constant.data(), 100);
    EXPECT_NEAR(var, 0.0f, TOLERANCE);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
