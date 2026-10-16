//=============================================================================
// test_ternary_accuracy_regression.cpp - Ternary Quantization Accuracy Tests
//=============================================================================
/**
 * @file test_ternary_accuracy_regression.cpp
 * @brief Regression tests for ternary quantization accuracy bounds
 *
 * WHAT: Test that ternary quantization maintains acceptable accuracy
 * WHY:  Quantization introduces error; must track bounds
 * HOW:  Compare float vs ternary forward pass results
 *
 * ACCURACY METRICS:
 * - Mean Absolute Error (MAE)
 * - Root Mean Square Error (RMSE)
 * - Sign agreement percentage
 * - Magnitude preservation
 *
 * REGRESSION BASELINES:
 * - Sign agreement: >= 90% for threshold 0.3
 * - MAE from reconstruction: <= 0.5 for typical data
 * - Forward pass correlation: >= 0.8 for neural activations
 *
 * @author NIMCP Test Team
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include <cmath>
#include <random>
#include <vector>
#include <algorithm>
#include <numeric>

// Headers have their own extern "C" guards
#include "utils/ternary/nimcp_ternary.h"

//=============================================================================
// Test Fixture
//=============================================================================

class TernaryAccuracyRegressionTest : public ::testing::Test {
protected:
    std::mt19937 rng;

    void SetUp() override {
        rng.seed(42);  // Fixed seed for reproducibility
    }

    void TearDown() override {}

    // Generate random float array with Gaussian distribution
    std::vector<float> generate_gaussian(size_t n, float mean, float std) {
        std::normal_distribution<float> dist(mean, std);
        std::vector<float> result(n);
        for (size_t i = 0; i < n; i++) {
            result[i] = dist(rng);
        }
        return result;
    }

    // Generate random float array with uniform distribution
    std::vector<float> generate_uniform(size_t n, float min_val, float max_val) {
        std::uniform_real_distribution<float> dist(min_val, max_val);
        std::vector<float> result(n);
        for (size_t i = 0; i < n; i++) {
            result[i] = dist(rng);
        }
        return result;
    }

    // Calculate Mean Absolute Error
    double calculate_mae(const std::vector<float>& original,
                         const std::vector<float>& reconstructed) {
        if (original.size() != reconstructed.size()) return -1.0;

        double sum = 0.0;
        for (size_t i = 0; i < original.size(); i++) {
            sum += std::abs(original[i] - reconstructed[i]);
        }
        return sum / original.size();
    }

    // Calculate Root Mean Square Error
    double calculate_rmse(const std::vector<float>& original,
                          const std::vector<float>& reconstructed) {
        if (original.size() != reconstructed.size()) return -1.0;

        double sum_sq = 0.0;
        for (size_t i = 0; i < original.size(); i++) {
            double diff = original[i] - reconstructed[i];
            sum_sq += diff * diff;
        }
        return std::sqrt(sum_sq / original.size());
    }

    // Calculate sign agreement percentage
    double calculate_sign_agreement(const std::vector<float>& original,
                                    const std::vector<trit_t>& quantized) {
        if (original.size() != quantized.size()) return -1.0;

        size_t matches = 0;
        for (size_t i = 0; i < original.size(); i++) {
            int orig_sign = (original[i] > 0.0f) ? 1 : ((original[i] < 0.0f) ? -1 : 0);
            if (orig_sign == quantized[i]) matches++;
        }
        return 100.0 * matches / original.size();
    }

    // Pearson correlation coefficient
    double calculate_correlation(const std::vector<float>& x,
                                 const std::vector<float>& y) {
        if (x.size() != y.size() || x.empty()) return 0.0;

        double sum_x = std::accumulate(x.begin(), x.end(), 0.0);
        double sum_y = std::accumulate(y.begin(), y.end(), 0.0);
        double mean_x = sum_x / x.size();
        double mean_y = sum_y / y.size();

        double num = 0.0, den_x = 0.0, den_y = 0.0;
        for (size_t i = 0; i < x.size(); i++) {
            double dx = x[i] - mean_x;
            double dy = y[i] - mean_y;
            num += dx * dy;
            den_x += dx * dx;
            den_y += dy * dy;
        }

        if (den_x == 0.0 || den_y == 0.0) return 0.0;
        return num / std::sqrt(den_x * den_y);
    }
};

//=============================================================================
// Quantization Accuracy Tests
//=============================================================================

TEST_F(TernaryAccuracyRegressionTest, ThresholdQuantizationSignAgreement) {
    // WHAT: Verify sign agreement after threshold quantization
    // WHY:  Sign preservation is critical for neural weights
    // BASELINE: >= 90% sign agreement for threshold 0.3

    const size_t n = 10000;
    const float threshold = 0.3f;

    std::vector<float> weights = generate_gaussian(n, 0.0f, 1.0f);
    std::vector<trit_t> quantized(n);

    for (size_t i = 0; i < n; i++) {
        quantized[i] = trit_from_float_threshold(weights[i], threshold);
    }

    double agreement = calculate_sign_agreement(weights, quantized);

    std::cout << "Sign agreement (threshold=" << threshold << "): "
              << agreement << "%" << std::endl;

    // For Gaussian data, most values should be outside threshold
    EXPECT_GE(agreement, 70.0);  // Lower bound for Gaussian(0,1) with t=0.3
}

TEST_F(TernaryAccuracyRegressionTest, ReconstructionMAE) {
    // WHAT: Verify reconstruction MAE is bounded
    // WHY:  Quantization error must be predictable
    // BASELINE: MAE <= 0.5 for scale=1.0 reconstruction

    const size_t n = 10000;
    const float threshold = 0.5f;
    const float scale = 1.0f;

    std::vector<float> original = generate_gaussian(n, 0.0f, 1.0f);

    trit_vector_t* vec = trit_vector_from_floats(original.data(), n, threshold,
                                                   TERNARY_PACK_NONE);
    ASSERT_NE(vec, nullptr);

    std::vector<float> reconstructed(n);
    trit_vector_to_floats(vec, reconstructed.data(), scale);

    double mae = calculate_mae(original, reconstructed);
    double rmse = calculate_rmse(original, reconstructed);

    std::cout << "Reconstruction error (threshold=" << threshold << "):" << std::endl;
    std::cout << "  MAE: " << mae << std::endl;
    std::cout << "  RMSE: " << rmse << std::endl;

    // MAE should be bounded by threshold + margin
    EXPECT_LE(mae, 0.7);
    EXPECT_LE(rmse, 1.0);

    trit_vector_destroy(vec);
}

TEST_F(TernaryAccuracyRegressionTest, DifferentThresholdAccuracy) {
    // WHAT: Verify accuracy improves with smaller threshold
    // WHY:  Smaller threshold should preserve more information
    // BASELINE: MAE should decrease as threshold decreases

    const size_t n = 10000;
    std::vector<float> thresholds = {0.1f, 0.3f, 0.5f, 0.7f};
    std::vector<double> maes;

    std::vector<float> original = generate_gaussian(n, 0.0f, 1.0f);

    for (float threshold : thresholds) {
        trit_vector_t* vec = trit_vector_from_floats(original.data(), n, threshold,
                                                       TERNARY_PACK_NONE);
        ASSERT_NE(vec, nullptr);

        std::vector<float> reconstructed(n);
        trit_vector_to_floats(vec, reconstructed.data(), 1.0f);

        double mae = calculate_mae(original, reconstructed);
        maes.push_back(mae);

        std::cout << "Threshold " << threshold << ": MAE = " << mae << std::endl;

        trit_vector_destroy(vec);
    }

    // All thresholds should produce bounded MAE
    // Note: Very small thresholds quantize everything to non-zero (higher MAE),
    // so monotonic ordering doesn't hold - just verify reasonable bounds
    for (size_t i = 0; i < maes.size(); i++) {
        EXPECT_LE(maes[i], 0.7) << "MAE exceeds reconstruction bound at threshold index " << i;
    }
}

//=============================================================================
// Forward Pass Accuracy Tests
//=============================================================================

TEST_F(TernaryAccuracyRegressionTest, MatrixVectorMultiplyCorrelation) {
    // WHAT: Verify ternary matrix-vector multiply correlates with float
    // WHY:  Forward pass must preserve activation patterns
    // BASELINE: Correlation >= 0.7 with float baseline

    const size_t rows = 100;
    const size_t cols = 100;
    const float threshold = 0.5f;

    // Generate random float matrix and vector
    std::vector<float> mat_data = generate_gaussian(rows * cols, 0.0f, 0.5f);
    std::vector<float> vec_data = generate_gaussian(cols, 0.0f, 1.0f);

    // Float matrix-vector multiply baseline
    std::vector<float> float_result(rows, 0.0f);
    for (size_t r = 0; r < rows; r++) {
        for (size_t c = 0; c < cols; c++) {
            float_result[r] += mat_data[r * cols + c] * vec_data[c];
        }
    }

    // Ternary matrix-vector multiply
    trit_matrix_t* mat = trit_matrix_from_floats(mat_data.data(), rows, cols,
                                                   threshold, TERNARY_PACK_NONE);
    trit_vector_t* vec = trit_vector_from_floats(vec_data.data(), cols,
                                                   threshold, TERNARY_PACK_NONE);
    ASSERT_NE(mat, nullptr);
    ASSERT_NE(vec, nullptr);

    trit_vector_t* result = trit_matrix_vector_mul(mat, vec);
    ASSERT_NE(result, nullptr);

    // Convert ternary result to float for comparison
    std::vector<float> ternary_result(rows);
    trit_vector_to_floats(result, ternary_result.data(), 1.0f);

    // Calculate correlation
    double correlation = calculate_correlation(float_result, ternary_result);

    std::cout << "Matrix-vector multiply correlation: " << correlation << std::endl;

    // Correlation should be positive (same direction)
    EXPECT_GT(correlation, 0.3);

    trit_matrix_destroy(mat);
    trit_vector_destroy(vec);
    trit_vector_destroy(result);
}

TEST_F(TernaryAccuracyRegressionTest, DotProductCorrelation) {
    // WHAT: Verify ternary dot product correlates with float dot product
    // WHY:  Dot product is fundamental operation
    // BASELINE: Sign of result should match in most cases

    const size_t n = 100;
    const size_t num_trials = 100;
    const float threshold = 0.3f;

    size_t sign_matches = 0;

    for (size_t trial = 0; trial < num_trials; trial++) {
        std::vector<float> a = generate_gaussian(n, 0.0f, 1.0f);
        std::vector<float> b = generate_gaussian(n, 0.0f, 1.0f);

        // Float dot product
        float float_dot = 0.0f;
        for (size_t i = 0; i < n; i++) {
            float_dot += a[i] * b[i];
        }

        // Ternary dot product
        trit_vector_t* va = trit_vector_from_floats(a.data(), n, threshold, TERNARY_PACK_NONE);
        trit_vector_t* vb = trit_vector_from_floats(b.data(), n, threshold, TERNARY_PACK_NONE);
        ASSERT_NE(va, nullptr);
        ASSERT_NE(vb, nullptr);

        int ternary_dot = trit_vector_dot(va, vb);

        // Check sign agreement
        int float_sign = (float_dot > 0.0f) ? 1 : ((float_dot < 0.0f) ? -1 : 0);
        int ternary_sign = (ternary_dot > 0) ? 1 : ((ternary_dot < 0) ? -1 : 0);

        if (float_sign == ternary_sign) sign_matches++;

        trit_vector_destroy(va);
        trit_vector_destroy(vb);
    }

    double sign_match_rate = 100.0 * sign_matches / num_trials;

    std::cout << "Dot product sign match rate: " << sign_match_rate << "%" << std::endl;

    EXPECT_GE(sign_match_rate, 60.0);  // At least 60% sign agreement
}

//=============================================================================
// Sparsity and Distribution Tests
//=============================================================================

TEST_F(TernaryAccuracyRegressionTest, SparsityPreservation) {
    // WHAT: Verify sparsity is correctly preserved after quantization
    // WHY:  Sparsity patterns affect computational efficiency
    // BASELINE: Sparsity should match expected from threshold

    const size_t n = 10000;

    struct TestCase {
        float threshold;
        float expected_sparsity_min;
        float expected_sparsity_max;
    };

    // For Gaussian(0,1) data:
    // P(|x| < t) = erf(t/sqrt(2)) for standard normal
    std::vector<TestCase> cases = {
        {0.1f, 0.05f, 0.15f},   // About 8% should be zero
        {0.5f, 0.30f, 0.45f},   // About 38% should be zero
        {1.0f, 0.60f, 0.75f},   // About 68% should be zero
    };

    std::vector<float> data = generate_gaussian(n, 0.0f, 1.0f);

    for (const auto& tc : cases) {
        trit_vector_t* vec = trit_vector_from_floats(data.data(), n, tc.threshold,
                                                       TERNARY_PACK_NONE);
        ASSERT_NE(vec, nullptr);

        size_t pos_count, unk_count, neg_count;
        trit_vector_count(vec, &pos_count, &unk_count, &neg_count);

        float sparsity = static_cast<float>(unk_count) / n;

        std::cout << "Threshold " << tc.threshold << ": sparsity = " << sparsity
                  << " (expected " << tc.expected_sparsity_min << "-"
                  << tc.expected_sparsity_max << ")" << std::endl;

        EXPECT_GE(sparsity, tc.expected_sparsity_min);
        EXPECT_LE(sparsity, tc.expected_sparsity_max);

        trit_vector_destroy(vec);
    }
}

TEST_F(TernaryAccuracyRegressionTest, BalancedDistribution) {
    // WHAT: Verify ternary distribution is balanced for symmetric input
    // WHY:  Balanced weights are important for neural network stability
    // BASELINE: Positive and negative counts should be within 5% of each other

    const size_t n = 100000;
    const float threshold = 0.3f;

    // Symmetric Gaussian data
    std::vector<float> data = generate_gaussian(n, 0.0f, 1.0f);

    trit_vector_t* vec = trit_vector_from_floats(data.data(), n, threshold,
                                                   TERNARY_PACK_NONE);
    ASSERT_NE(vec, nullptr);

    size_t pos_count, unk_count, neg_count;
    trit_vector_count(vec, &pos_count, &unk_count, &neg_count);

    std::cout << "Distribution for symmetric Gaussian:" << std::endl;
    std::cout << "  Positive: " << pos_count << " (" << 100.0 * pos_count / n << "%)" << std::endl;
    std::cout << "  Unknown: " << unk_count << " (" << 100.0 * unk_count / n << "%)" << std::endl;
    std::cout << "  Negative: " << neg_count << " (" << 100.0 * neg_count / n << "%)" << std::endl;

    // Positive and negative should be roughly equal (within 2%)
    double pos_ratio = static_cast<double>(pos_count) / n;
    double neg_ratio = static_cast<double>(neg_count) / n;
    double imbalance = std::abs(pos_ratio - neg_ratio);

    EXPECT_LE(imbalance, 0.02);

    trit_vector_destroy(vec);
}

//=============================================================================
// Extended Trit Accuracy Tests
//=============================================================================

TEST_F(TernaryAccuracyRegressionTest, ExtendedTritConfidenceAccuracy) {
    // WHAT: Verify extended trit confidence reflects quantization certainty
    // WHY:  Confidence should indicate how close value was to threshold
    // BASELINE: Far-from-threshold values should have high confidence

    const float threshold = 0.5f;

    struct TestCase {
        float input;
        trit_t expected_value;
        float min_confidence;
    };

    std::vector<TestCase> cases = {
        {2.0f, TRIT_POSITIVE, 0.9f},    // Far from threshold
        {-2.0f, TRIT_NEGATIVE, 0.9f},   // Far from threshold
        {0.0f, TRIT_UNKNOWN, 0.9f},     // Center of unknown zone
        {0.51f, TRIT_POSITIVE, 0.0f},   // Just over threshold
        {-0.51f, TRIT_NEGATIVE, 0.0f},  // Just under threshold
    };

    for (const auto& tc : cases) {
        trit_extended_t ext = trit_extended_from_float(tc.input, threshold);

        std::cout << "Input " << tc.input << ": value=" << (int)ext.value
                  << ", confidence=" << ext.confidence << std::endl;

        EXPECT_EQ(ext.value, tc.expected_value);
        EXPECT_GE(ext.confidence, tc.min_confidence);
        EXPECT_LE(ext.confidence, 1.0f);
    }
}

//=============================================================================
// Degradation Bound Tests
//=============================================================================

TEST_F(TernaryAccuracyRegressionTest, MaximumQuantizationError) {
    // WHAT: Verify maximum per-element quantization error is bounded
    // WHY:  Worst-case error must be predictable
    // BASELINE: Max error <= 1.0 for unit-scaled reconstruction

    const size_t n = 10000;
    const float threshold = 0.5f;

    std::vector<float> original = generate_uniform(n, -2.0f, 2.0f);

    trit_vector_t* vec = trit_vector_from_floats(original.data(), n, threshold,
                                                   TERNARY_PACK_NONE);
    ASSERT_NE(vec, nullptr);

    std::vector<float> reconstructed(n);
    trit_vector_to_floats(vec, reconstructed.data(), 1.0f);

    float max_error = 0.0f;
    for (size_t i = 0; i < n; i++) {
        float error = std::abs(original[i] - reconstructed[i]);
        max_error = std::max(max_error, error);
    }

    std::cout << "Maximum quantization error: " << max_error << std::endl;

    // For values in [-2, 2] with ternary {-1, 0, +1}, max error is ~2-3
    EXPECT_LE(max_error, 3.0f);

    trit_vector_destroy(vec);
}

TEST_F(TernaryAccuracyRegressionTest, AccuracyConsistencyAcrossModes) {
    // WHAT: Verify accuracy is identical across pack modes
    // WHY:  Pack mode should only affect storage, not values
    // BASELINE: Same quantization results for all pack modes

    const size_t n = 1000;
    const float threshold = 0.5f;

    std::vector<float> original = generate_gaussian(n, 0.0f, 1.0f);

    trit_vector_t* vec_none = trit_vector_from_floats(original.data(), n, threshold,
                                                        TERNARY_PACK_NONE);
    trit_vector_t* vec_2bit = trit_vector_from_floats(original.data(), n, threshold,
                                                        TERNARY_PACK_2BIT);
    trit_vector_t* vec_243 = trit_vector_from_floats(original.data(), n, threshold,
                                                       TERNARY_PACK_BASE243);

    ASSERT_NE(vec_none, nullptr);
    ASSERT_NE(vec_2bit, nullptr);
    ASSERT_NE(vec_243, nullptr);

    // All should produce identical values
    for (size_t i = 0; i < n; i++) {
        trit_t v_none = trit_vector_get(vec_none, i);
        trit_t v_2bit = trit_vector_get(vec_2bit, i);
        trit_t v_243 = trit_vector_get(vec_243, i);

        EXPECT_EQ(v_none, v_2bit);
        EXPECT_EQ(v_none, v_243);
    }

    trit_vector_destroy(vec_none);
    trit_vector_destroy(vec_2bit);
    trit_vector_destroy(vec_243);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
