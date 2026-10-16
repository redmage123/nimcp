/**
 * @file test_loss_functions.cpp
 * @brief Unit tests for Loss Functions Module (Phase TM-2)
 *
 * Tests cover:
 * - All loss function types (MSE, MAE, BCE, CE, KL, Huber, etc.)
 * - Forward and backward passes
 * - Gradient computation correctness
 * - Reduction modes (mean, sum, none)
 * - Security integration
 * - Memory pool integration
 * - Numerical stability
 * - Edge cases
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <random>
#include <limits>
#include <algorithm>

// Headers have their own extern "C" guards
#include "middleware/training/nimcp_loss_functions.h"
#include "security/nimcp_security_integration.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/validation/nimcp_common.h"

namespace {

constexpr float EPSILON = 1e-5f;
constexpr float GRADIENT_TOL = 1e-4f;

/**
 * @brief Test fixture for loss function unit tests
 */
class LossFunctionsTest : public ::testing::Test {
protected:
    nimcp_sec_integration_t* security_ctx = nullptr;
    unified_mem_manager_t memory_mgr = nullptr;

    void SetUp() override {
        // Initialize security context
        security_ctx = nimcp_sec_integration_create();
        if (security_ctx) {
            nimcp_sec_integration_config_t sec_cfg = nimcp_sec_integration_default_config();
            nimcp_sec_integration_init(security_ctx, &sec_cfg);
        }

        // Memory manager not needed - loss functions use malloc/free internally
        // Can be added later if unified memory integration is required
        memory_mgr = nullptr;
    }

    void TearDown() override {
        if (security_ctx) {
            nimcp_sec_integration_destroy(security_ctx);
            security_ctx = nullptr;
        }
    }

    // Helper to check gradients numerically
    bool checkGradientNumerically(
        nimcp_loss_type_t loss_type,
        const float* predictions,
        const float* targets,
        const float* analytic_grad,
        size_t count,
        float delta = 1e-4f)
    {
        std::vector<float> pred_copy(predictions, predictions + count);
        float eps = 1e-3f;

        for (size_t i = 0; i < count; i++) {
            // Compute numerical gradient
            pred_copy[i] = predictions[i] + delta;
            float loss_plus = nimcp_loss_mse(pred_copy.data(), targets, count, NIMCP_LOSS_REDUCE_MEAN);

            pred_copy[i] = predictions[i] - delta;
            float loss_minus = nimcp_loss_mse(pred_copy.data(), targets, count, NIMCP_LOSS_REDUCE_MEAN);

            pred_copy[i] = predictions[i]; // Restore

            float numerical_grad = (loss_plus - loss_minus) / (2.0f * delta);

            if (std::abs(analytic_grad[i] - numerical_grad) > eps) {
                return false;
            }
        }
        return true;
    }
};

// ============================================================================
// MSE Loss Tests
// ============================================================================

TEST_F(LossFunctionsTest, MSE_BasicComputation) {
    float predictions[] = {1.0f, 2.0f, 3.0f, 4.0f};
    float targets[] = {1.5f, 2.0f, 2.5f, 4.5f};
    size_t count = 4;

    // Expected: ((0.5)^2 + (0)^2 + (0.5)^2 + (0.5)^2) / 4 = 0.75 / 4 = 0.1875
    float expected = 0.1875f;

    float result = nimcp_loss_mse(predictions, targets, count, NIMCP_LOSS_REDUCE_MEAN);
    EXPECT_NEAR(result, expected, EPSILON);
}

TEST_F(LossFunctionsTest, MSE_SumReduction) {
    float predictions[] = {1.0f, 2.0f, 3.0f};
    float targets[] = {2.0f, 2.0f, 2.0f};
    size_t count = 3;

    // Expected: (1 + 0 + 1) = 2.0
    float result = nimcp_loss_mse(predictions, targets, count, NIMCP_LOSS_REDUCE_SUM);
    EXPECT_NEAR(result, 2.0f, EPSILON);
}

TEST_F(LossFunctionsTest, MSE_ZeroLoss) {
    float values[] = {1.0f, 2.0f, 3.0f};
    float result = nimcp_loss_mse(values, values, 3, NIMCP_LOSS_REDUCE_MEAN);
    EXPECT_NEAR(result, 0.0f, EPSILON);
}

TEST_F(LossFunctionsTest, MSE_Gradient) {
    float predictions[] = {1.0f, 2.0f, 3.0f, 4.0f};
    float targets[] = {0.0f, 2.0f, 4.0f, 3.0f};
    float gradients[4];
    size_t count = 4;

    nimcp_loss_mse_grad(predictions, targets, gradients, count);

    // gradient = 2 * (pred - target) / n
    float scale = 2.0f / count;
    EXPECT_NEAR(gradients[0], scale * 1.0f, EPSILON);   // 1 - 0 = 1
    EXPECT_NEAR(gradients[1], scale * 0.0f, EPSILON);   // 2 - 2 = 0
    EXPECT_NEAR(gradients[2], scale * -1.0f, EPSILON);  // 3 - 4 = -1
    EXPECT_NEAR(gradients[3], scale * 1.0f, EPSILON);   // 4 - 3 = 1
}

// ============================================================================
// MAE Loss Tests
// ============================================================================

TEST_F(LossFunctionsTest, MAE_BasicComputation) {
    float predictions[] = {1.0f, 2.0f, 3.0f, 4.0f};
    float targets[] = {0.0f, 2.0f, 5.0f, 3.0f};
    size_t count = 4;

    // Expected: (|1| + |0| + |-2| + |1|) / 4 = 4 / 4 = 1.0
    float expected = 1.0f;

    float result = nimcp_loss_mae(predictions, targets, count, NIMCP_LOSS_REDUCE_MEAN);
    EXPECT_NEAR(result, expected, EPSILON);
}

TEST_F(LossFunctionsTest, MAE_Gradient) {
    float predictions[] = {1.0f, 2.0f, 3.0f};
    float targets[] = {0.0f, 2.0f, 5.0f};
    float gradients[3];

    nimcp_loss_mae_grad(predictions, targets, gradients, 3);

    float scale = 1.0f / 3.0f;
    EXPECT_NEAR(gradients[0], scale, EPSILON);    // 1-0 > 0
    EXPECT_NEAR(gradients[1], 0.0f, EPSILON);     // 2-2 = 0
    EXPECT_NEAR(gradients[2], -scale, EPSILON);   // 3-5 < 0
}

// ============================================================================
// Binary Cross-Entropy Tests
// ============================================================================

TEST_F(LossFunctionsTest, BCE_BasicComputation) {
    float predictions[] = {0.9f, 0.1f, 0.8f, 0.2f};
    float targets[] = {1.0f, 0.0f, 1.0f, 0.0f};
    size_t count = 4;

    float result = nimcp_loss_binary_cross_entropy(predictions, targets, count,
                                                    NIMCP_LOSS_REDUCE_MEAN, EPSILON);

    // All predictions are "correct" (high for 1, low for 0), so loss should be low
    EXPECT_LT(result, 0.5f);
    EXPECT_GT(result, 0.0f);
}

TEST_F(LossFunctionsTest, BCE_PerfectPrediction) {
    float predictions[] = {0.999f, 0.001f};
    float targets[] = {1.0f, 0.0f};

    float result = nimcp_loss_binary_cross_entropy(predictions, targets, 2,
                                                    NIMCP_LOSS_REDUCE_MEAN, 1e-7f);

    // Near-perfect predictions should have very low loss
    EXPECT_LT(result, 0.01f);
}

TEST_F(LossFunctionsTest, BCE_WorstPrediction) {
    float predictions[] = {0.001f, 0.999f};
    float targets[] = {1.0f, 0.0f};

    float result = nimcp_loss_binary_cross_entropy(predictions, targets, 2,
                                                    NIMCP_LOSS_REDUCE_MEAN, 1e-7f);

    // Worst predictions should have high loss
    EXPECT_GT(result, 5.0f);
}

TEST_F(LossFunctionsTest, BCE_Gradient) {
    float predictions[] = {0.8f, 0.2f};
    float targets[] = {1.0f, 0.0f};
    float gradients[2];

    nimcp_loss_binary_cross_entropy_grad(predictions, targets, gradients, 2, 1e-7f);

    // For correct predictions, gradients should push predictions toward targets
    // gradient = (p - t) / (p * (1-p))
    // For p=0.8, t=1: (0.8-1)/(0.8*0.2) = -0.2/0.16 = -1.25 (scaled by 1/n)
    EXPECT_LT(gradients[0], 0.0f);  // Should be negative to increase prediction
    EXPECT_GT(gradients[1], 0.0f);  // Should be positive to decrease prediction
}

// ============================================================================
// Cross-Entropy Loss Tests
// ============================================================================

TEST_F(LossFunctionsTest, CrossEntropy_BasicComputation) {
    // Softmax outputs (already normalized)
    float predictions[] = {0.7f, 0.2f, 0.1f,   // Sample 1: class 0
                           0.1f, 0.8f, 0.1f};  // Sample 2: class 1
    float targets[] = {1.0f, 0.0f, 0.0f,       // Sample 1: true class 0
                       0.0f, 1.0f, 0.0f};      // Sample 2: true class 1

    float result = nimcp_loss_cross_entropy(predictions, targets, 2, 3,
                                             NIMCP_LOSS_REDUCE_MEAN, 1e-7f);

    // Loss = -mean(log(0.7), log(0.8)) ≈ 0.277
    EXPECT_LT(result, 0.5f);
    EXPECT_GT(result, 0.1f);
}

TEST_F(LossFunctionsTest, CrossEntropy_Gradient) {
    float predictions[] = {0.7f, 0.2f, 0.1f};
    float targets[] = {1.0f, 0.0f, 0.0f};
    float gradients[3];

    nimcp_loss_cross_entropy_grad(predictions, targets, gradients, 1, 3);

    // For softmax + CE, gradient = p - t
    EXPECT_NEAR(gradients[0], -0.3f, 0.01f);  // 0.7 - 1.0
    EXPECT_NEAR(gradients[1], 0.2f, 0.01f);   // 0.2 - 0.0
    EXPECT_NEAR(gradients[2], 0.1f, 0.01f);   // 0.1 - 0.0
}

// ============================================================================
// KL Divergence Tests
// ============================================================================

TEST_F(LossFunctionsTest, KL_IdenticalDistributions) {
    float p[] = {0.25f, 0.25f, 0.25f, 0.25f};
    float q[] = {0.25f, 0.25f, 0.25f, 0.25f};

    float result = nimcp_loss_kl_divergence(p, q, 4, NIMCP_LOSS_REDUCE_SUM, 1e-7f);

    // KL divergence of identical distributions is 0
    EXPECT_NEAR(result, 0.0f, 1e-5f);
}

TEST_F(LossFunctionsTest, KL_DifferentDistributions) {
    float p[] = {0.5f, 0.5f};
    float q[] = {0.1f, 0.9f};

    float result = nimcp_loss_kl_divergence(p, q, 2, NIMCP_LOSS_REDUCE_SUM, 1e-7f);

    // KL divergence should be positive for different distributions
    EXPECT_GT(result, 0.0f);
}

TEST_F(LossFunctionsTest, KL_Asymmetry) {
    float p[] = {0.8f, 0.2f};
    float q[] = {0.2f, 0.8f};

    float kl_pq = nimcp_loss_kl_divergence(p, q, 2, NIMCP_LOSS_REDUCE_SUM, 1e-7f);
    float kl_qp = nimcp_loss_kl_divergence(q, p, 2, NIMCP_LOSS_REDUCE_SUM, 1e-7f);

    // KL divergence is asymmetric: KL(p||q) != KL(q||p)
    // But for symmetric distributions, they should be equal
    EXPECT_NEAR(kl_pq, kl_qp, EPSILON);
}

// ============================================================================
// Huber Loss Tests
// ============================================================================

TEST_F(LossFunctionsTest, Huber_SmallErrors) {
    // Small errors should behave like MSE
    float predictions[] = {1.0f, 2.0f, 3.0f};
    float targets[] = {1.1f, 2.1f, 3.1f};
    float delta = 1.0f;

    float huber = nimcp_loss_huber(predictions, targets, 3, delta, NIMCP_LOSS_REDUCE_MEAN);
    float mse = nimcp_loss_mse(predictions, targets, 3, NIMCP_LOSS_REDUCE_MEAN);

    // For small errors (< delta), Huber ≈ 0.5 * MSE
    EXPECT_NEAR(huber, 0.5f * mse, 1e-4f);
}

TEST_F(LossFunctionsTest, Huber_LargeErrors) {
    // Large errors should behave like MAE
    float predictions[] = {0.0f, 0.0f};
    float targets[] = {10.0f, -10.0f};
    float delta = 1.0f;

    float huber = nimcp_loss_huber(predictions, targets, 2, delta, NIMCP_LOSS_REDUCE_MEAN);

    // For large errors, Huber grows linearly
    // Huber = delta * (|error| - 0.5*delta)
    float expected = delta * (10.0f - 0.5f * delta);
    EXPECT_NEAR(huber, expected, 0.1f);
}

TEST_F(LossFunctionsTest, Huber_Gradient_SmallError) {
    float predictions[] = {1.0f};
    float targets[] = {1.5f};
    float gradients[1];
    float delta = 1.0f;

    nimcp_loss_huber_grad(predictions, targets, gradients, 1, delta);

    // For |error| < delta, gradient = error / n
    EXPECT_NEAR(gradients[0], -0.5f, EPSILON);
}

TEST_F(LossFunctionsTest, Huber_Gradient_LargeError) {
    float predictions[] = {0.0f};
    float targets[] = {5.0f};
    float gradients[1];
    float delta = 1.0f;

    nimcp_loss_huber_grad(predictions, targets, gradients, 1, delta);

    // For |error| > delta, gradient = ±delta / n
    EXPECT_NEAR(gradients[0], -delta, EPSILON);
}

// ============================================================================
// Hinge Loss Tests
// ============================================================================

TEST_F(LossFunctionsTest, Hinge_CorrectClassification) {
    // Correct classification with margin
    float predictions[] = {2.0f, -2.0f};  // Confident predictions
    float targets[] = {1.0f, -1.0f};       // True labels

    float result = nimcp_loss_hinge(predictions, targets, 2, NIMCP_LOSS_REDUCE_MEAN);

    // margin = 1 - y*f(x) = 1 - 2 = -1 (< 0, so loss = 0)
    EXPECT_NEAR(result, 0.0f, EPSILON);
}

TEST_F(LossFunctionsTest, Hinge_WrongClassification) {
    float predictions[] = {-1.0f};  // Wrong prediction
    float targets[] = {1.0f};        // True label

    float result = nimcp_loss_hinge(predictions, targets, 1, NIMCP_LOSS_REDUCE_MEAN);

    // margin = 1 - (-1)*1 = 1 + 1 = 2
    EXPECT_NEAR(result, 2.0f, EPSILON);
}

// ============================================================================
// Focal Loss Tests
// ============================================================================

TEST_F(LossFunctionsTest, Focal_BasicComputation) {
    float predictions[] = {0.9f, 0.1f};
    float targets[] = {1.0f, 0.0f};
    float gamma = 2.0f;
    float alpha = 0.25f;

    float focal = nimcp_loss_focal(predictions, targets, 2, gamma, alpha,
                                    NIMCP_LOSS_REDUCE_MEAN, 1e-7f);
    float bce = nimcp_loss_binary_cross_entropy(predictions, targets, 2,
                                                 NIMCP_LOSS_REDUCE_MEAN, 1e-7f);

    // Focal loss should be less than BCE for easy examples
    EXPECT_LT(focal, bce);
}

TEST_F(LossFunctionsTest, Focal_HardExamples) {
    float predictions[] = {0.5f, 0.5f};  // Uncertain predictions
    float targets[] = {1.0f, 0.0f};
    float gamma = 2.0f;
    float alpha = 0.25f;

    float focal = nimcp_loss_focal(predictions, targets, 2, gamma, alpha,
                                    NIMCP_LOSS_REDUCE_MEAN, 1e-7f);

    // For hard examples (p close to 0.5), focal loss is higher
    EXPECT_GT(focal, 0.0f);
}

// ============================================================================
// Contrastive Loss Tests
// ============================================================================

TEST_F(LossFunctionsTest, Contrastive_SimilarPairs) {
    // Two identical embeddings
    float embed1[] = {1.0f, 0.0f, 0.0f};
    float embed2[] = {1.0f, 0.0f, 0.0f};
    float labels[] = {1.0f};  // Similar pair

    float result = nimcp_loss_contrastive(embed1, embed2, labels, 1, 3, 1.0f,
                                           NIMCP_LOSS_REDUCE_MEAN);

    // Distance is 0, so loss should be 0 for similar pairs
    EXPECT_NEAR(result, 0.0f, EPSILON);
}

TEST_F(LossFunctionsTest, Contrastive_DissimilarPairs) {
    // Two different embeddings
    float embed1[] = {1.0f, 0.0f, 0.0f};
    float embed2[] = {0.0f, 1.0f, 0.0f};
    float labels[] = {0.0f};  // Dissimilar pair
    float margin = 2.0f;

    float result = nimcp_loss_contrastive(embed1, embed2, labels, 1, 3, margin,
                                           NIMCP_LOSS_REDUCE_MEAN);

    // Distance = sqrt(2), margin = 2, so max(0, 2-sqrt(2))^2
    float dist = std::sqrt(2.0f);
    float expected = 0.5f * (margin - dist) * (margin - dist);
    EXPECT_NEAR(result, expected, EPSILON);
}

// ============================================================================
// Triplet Loss Tests
// ============================================================================

TEST_F(LossFunctionsTest, Triplet_CorrectOrdering) {
    // Anchor closer to positive than negative
    float anchors[] = {0.0f, 0.0f};
    float positives[] = {0.1f, 0.0f};  // Close to anchor
    float negatives[] = {1.0f, 0.0f};  // Far from anchor
    float margin = 0.5f;

    float result = nimcp_loss_triplet(anchors, positives, negatives, 1, 2, margin,
                                       NIMCP_LOSS_REDUCE_MEAN);

    // d(a,p) < d(a,n) - margin, so loss should be 0 or small
    EXPECT_LE(result, margin);
}

TEST_F(LossFunctionsTest, Triplet_WrongOrdering) {
    // Anchor closer to negative than positive
    float anchors[] = {0.0f, 0.0f};
    float positives[] = {1.0f, 0.0f};  // Far from anchor
    float negatives[] = {0.1f, 0.0f};  // Close to anchor
    float margin = 0.5f;

    float result = nimcp_loss_triplet(anchors, positives, negatives, 1, 2, margin,
                                       NIMCP_LOSS_REDUCE_MEAN);

    // d(a,p) > d(a,n), so loss should be positive
    EXPECT_GT(result, 0.0f);
}

// ============================================================================
// Context-Based Tests
// ============================================================================

TEST_F(LossFunctionsTest, Context_CreateDestroy) {
    nimcp_loss_config_t config = nimcp_loss_default_config(NIMCP_LOSS_MSE);

    nimcp_loss_context_t* ctx = nimcp_loss_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    nimcp_result_t err = nimcp_loss_init(ctx);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    nimcp_loss_destroy(ctx);
}

TEST_F(LossFunctionsTest, Context_ForwardPass) {
    nimcp_loss_config_t config = nimcp_loss_default_config(NIMCP_LOSS_MSE);
    nimcp_loss_context_t* ctx = nimcp_loss_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float predictions[] = {1.0f, 2.0f, 3.0f, 4.0f};
    float targets[] = {1.0f, 2.0f, 3.0f, 4.0f};

    nimcp_loss_result_t result;
    nimcp_result_t err = nimcp_loss_forward(ctx, predictions, targets, 1, 4, &result);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_NEAR(result.loss_value, 0.0f, EPSILON);

    nimcp_loss_destroy(ctx);
}

TEST_F(LossFunctionsTest, Context_BackwardPass) {
    nimcp_loss_config_t config = nimcp_loss_default_config(NIMCP_LOSS_MSE);
    nimcp_loss_context_t* ctx = nimcp_loss_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float predictions[] = {1.0f, 2.0f, 3.0f, 4.0f};
    float targets[] = {0.0f, 2.0f, 4.0f, 4.0f};
    float gradients[4];

    nimcp_result_t err = nimcp_loss_backward(ctx, predictions, targets, 1, 4, gradients);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Check gradients are non-zero where there's error
    EXPECT_NE(gradients[0], 0.0f);
    EXPECT_NEAR(gradients[1], 0.0f, EPSILON);
    EXPECT_NE(gradients[2], 0.0f);
    EXPECT_NEAR(gradients[3], 0.0f, EPSILON);

    nimcp_loss_destroy(ctx);
}

TEST_F(LossFunctionsTest, Context_ForwardBackward) {
    nimcp_loss_config_t config = nimcp_loss_default_config(NIMCP_LOSS_CROSS_ENTROPY);
    nimcp_loss_context_t* ctx = nimcp_loss_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float predictions[] = {0.7f, 0.2f, 0.1f};
    float targets[] = {1.0f, 0.0f, 0.0f};

    nimcp_loss_result_t result;
    memset(&result, 0, sizeof(result));

    nimcp_result_t err = nimcp_loss_forward_backward(ctx, predictions, targets, 1, 3, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GT(result.loss_value, 0.0f);
    EXPECT_NE(result.gradients, nullptr);
    EXPECT_EQ(result.gradient_count, 3u);

    nimcp_loss_result_free(&result);
    nimcp_loss_destroy(ctx);
}

TEST_F(LossFunctionsTest, Context_Statistics) {
    nimcp_loss_config_t config = nimcp_loss_default_config(NIMCP_LOSS_MSE);
    nimcp_loss_context_t* ctx = nimcp_loss_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float predictions[] = {1.0f, 2.0f, 3.0f};
    float targets[] = {0.0f, 2.0f, 4.0f};
    nimcp_loss_result_t result;

    // Run multiple forward passes
    for (int i = 0; i < 10; i++) {
        nimcp_loss_forward(ctx, predictions, targets, 1, 3, &result);
    }

    nimcp_loss_stats_t stats;
    nimcp_result_t err = nimcp_loss_get_stats(ctx, &stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(stats.forward_count, 10u);
    EXPECT_GT(stats.total_loss, 0.0);
    EXPECT_GT(stats.avg_loss, 0.0);

    nimcp_loss_destroy(ctx);
}

TEST_F(LossFunctionsTest, Context_Reset) {
    nimcp_loss_config_t config = nimcp_loss_default_config(NIMCP_LOSS_MSE);
    nimcp_loss_context_t* ctx = nimcp_loss_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float predictions[] = {1.0f, 2.0f};
    float targets[] = {0.0f, 2.0f};
    nimcp_loss_result_t result;

    nimcp_loss_forward(ctx, predictions, targets, 1, 2, &result);

    nimcp_loss_stats_t stats;
    nimcp_loss_get_stats(ctx, &stats);
    EXPECT_EQ(stats.forward_count, 1u);

    nimcp_result_t err = nimcp_loss_reset(ctx);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    nimcp_loss_get_stats(ctx, &stats);
    EXPECT_EQ(stats.forward_count, 0u);

    nimcp_loss_destroy(ctx);
}

// ============================================================================
// Utility Function Tests
// ============================================================================

TEST_F(LossFunctionsTest, Softmax_Normalization) {
    float logits[] = {1.0f, 2.0f, 3.0f};
    float probs[3];

    nimcp_loss_softmax(logits, probs, 1, 3);

    // Softmax should sum to 1
    float sum = probs[0] + probs[1] + probs[2];
    EXPECT_NEAR(sum, 1.0f, EPSILON);

    // Higher logit should have higher probability
    EXPECT_LT(probs[0], probs[1]);
    EXPECT_LT(probs[1], probs[2]);
}

TEST_F(LossFunctionsTest, Softmax_NumericalStability) {
    // Large values that could cause overflow
    float logits[] = {1000.0f, 1001.0f, 1002.0f};
    float probs[3];

    nimcp_loss_softmax(logits, probs, 1, 3);

    // Should still sum to 1 without NaN/Inf
    float sum = probs[0] + probs[1] + probs[2];
    EXPECT_TRUE(std::isfinite(sum));
    EXPECT_NEAR(sum, 1.0f, EPSILON);
}

TEST_F(LossFunctionsTest, Sigmoid_Range) {
    float inputs[] = {-10.0f, -1.0f, 0.0f, 1.0f, 10.0f};
    float outputs[5];

    nimcp_loss_sigmoid(inputs, outputs, 5);

    // Sigmoid output should be in (0, 1)
    for (int i = 0; i < 5; i++) {
        EXPECT_GT(outputs[i], 0.0f);
        EXPECT_LT(outputs[i], 1.0f);
    }

    // sigmoid(0) = 0.5
    EXPECT_NEAR(outputs[2], 0.5f, EPSILON);
}

TEST_F(LossFunctionsTest, GradientClipByValue) {
    float gradients[] = {0.5f, 2.0f, -3.0f, 0.1f};
    float max_value = 1.0f;

    size_t clipped = nimcp_loss_clip_gradients(gradients, 4, max_value);

    EXPECT_EQ(clipped, 2u);  // Two values clipped
    EXPECT_EQ(gradients[0], 0.5f);
    EXPECT_EQ(gradients[1], max_value);
    EXPECT_EQ(gradients[2], -max_value);
    EXPECT_EQ(gradients[3], 0.1f);
}

TEST_F(LossFunctionsTest, GradientClipByNorm) {
    float gradients[] = {3.0f, 4.0f};  // norm = 5
    float max_norm = 1.0f;

    float original_norm = nimcp_loss_clip_gradients_norm(gradients, 2, max_norm);

    EXPECT_NEAR(original_norm, 5.0f, EPSILON);

    // After clipping, norm should be max_norm
    float new_norm = std::sqrt(gradients[0] * gradients[0] + gradients[1] * gradients[1]);
    EXPECT_NEAR(new_norm, max_norm, EPSILON);

    // Direction should be preserved
    EXPECT_NEAR(gradients[1] / gradients[0], 4.0f / 3.0f, EPSILON);
}

// ============================================================================
// Edge Cases and Error Handling
// ============================================================================

TEST_F(LossFunctionsTest, NullInputHandling) {
    // Should handle null inputs gracefully
    float result = nimcp_loss_mse(nullptr, nullptr, 0, NIMCP_LOSS_REDUCE_MEAN);
    EXPECT_EQ(result, 0.0f);

    nimcp_loss_mse_grad(nullptr, nullptr, nullptr, 0);
    // Should not crash
}

TEST_F(LossFunctionsTest, EmptyInputHandling) {
    float predictions[1] = {0.0f};
    float targets[1] = {0.0f};

    float result = nimcp_loss_mse(predictions, targets, 0, NIMCP_LOSS_REDUCE_MEAN);
    EXPECT_EQ(result, 0.0f);
}

TEST_F(LossFunctionsTest, ConfigValidation_ValidConfig) {
    nimcp_loss_config_t config = nimcp_loss_default_config(NIMCP_LOSS_MSE);
    nimcp_result_t err = nimcp_loss_validate_config(&config);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(LossFunctionsTest, ConfigValidation_InvalidType) {
    nimcp_loss_config_t config = nimcp_loss_default_config(NIMCP_LOSS_MSE);
    config.type = (nimcp_loss_type_t)999;  // Invalid type

    nimcp_result_t err = nimcp_loss_validate_config(&config);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(LossFunctionsTest, ConfigValidation_NullConfig) {
    nimcp_result_t err = nimcp_loss_validate_config(nullptr);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(LossFunctionsTest, TypeName) {
    EXPECT_STREQ(nimcp_loss_type_name(NIMCP_LOSS_MSE), "MSE");
    EXPECT_STREQ(nimcp_loss_type_name(NIMCP_LOSS_CROSS_ENTROPY), "CrossEntropy");
    EXPECT_STREQ(nimcp_loss_type_name(NIMCP_LOSS_KL_DIVERGENCE), "KLDivergence");
    EXPECT_STREQ(nimcp_loss_type_name(NIMCP_LOSS_HUBER), "Huber");
}

// ============================================================================
// Memory Pool Integration Tests
// ============================================================================

TEST_F(LossFunctionsTest, MemoryPool_GradientAllocation) {
    nimcp_loss_config_t config = nimcp_loss_default_config(NIMCP_LOSS_MSE);
    config.use_memory_pool = true;
    config.cow_strategy = UNIFIED_STRATEGY_POOL_DIRECT;

    nimcp_loss_context_t* ctx = nimcp_loss_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float predictions[] = {1.0f, 2.0f, 3.0f, 4.0f};
    float targets[] = {0.0f, 2.0f, 4.0f, 3.0f};

    nimcp_loss_result_t result;
    nimcp_result_t err = nimcp_loss_forward_backward(ctx, predictions, targets, 1, 4, &result);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_NE(result.gradients, nullptr);

    nimcp_loss_result_free(&result);
    nimcp_loss_destroy(ctx);
}

// ============================================================================
// Batch Processing Tests
// ============================================================================

TEST_F(LossFunctionsTest, BatchProcessing_CrossEntropy) {
    nimcp_loss_config_t config = nimcp_loss_default_config(NIMCP_LOSS_CROSS_ENTROPY);
    nimcp_loss_context_t* ctx = nimcp_loss_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    // Batch of 4 samples, 3 classes each
    float predictions[12] = {
        0.7f, 0.2f, 0.1f,  // Sample 0: predicts class 0
        0.1f, 0.8f, 0.1f,  // Sample 1: predicts class 1
        0.1f, 0.1f, 0.8f,  // Sample 2: predicts class 2
        0.4f, 0.4f, 0.2f   // Sample 3: uncertain
    };
    float targets[12] = {
        1.0f, 0.0f, 0.0f,  // True class 0
        0.0f, 1.0f, 0.0f,  // True class 1
        0.0f, 0.0f, 1.0f,  // True class 2
        1.0f, 0.0f, 0.0f   // True class 0 (mismatch)
    };

    nimcp_loss_result_t result;
    nimcp_result_t err = nimcp_loss_forward(ctx, predictions, targets, 4, 3, &result);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GT(result.loss_value, 0.0f);

    nimcp_loss_destroy(ctx);
}

// ============================================================================
// Custom Loss Function Tests
// ============================================================================

static float custom_loss_forward(const float* pred, const float* tgt, size_t count, void* user_data) {
    (void)user_data;
    float sum = 0.0f;
    for (size_t i = 0; i < count; i++) {
        sum += (pred[i] - tgt[i]) * (pred[i] - tgt[i]) * (pred[i] - tgt[i]) * (pred[i] - tgt[i]);
    }
    return sum / (float)count;
}

static void custom_loss_backward(const float* pred, const float* tgt, float* grad, size_t count, void* user_data) {
    (void)user_data;
    for (size_t i = 0; i < count; i++) {
        float diff = pred[i] - tgt[i];
        grad[i] = 4.0f * diff * diff * diff / (float)count;
    }
}

TEST_F(LossFunctionsTest, CustomLoss_Integration) {
    nimcp_loss_config_t config = nimcp_loss_default_config(NIMCP_LOSS_CUSTOM);
    config.params.custom.forward_fn = custom_loss_forward;
    config.params.custom.backward_fn = custom_loss_backward;
    config.params.custom.user_data = nullptr;
    config.params.custom.name = "L4Loss";

    nimcp_loss_context_t* ctx = nimcp_loss_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float predictions[] = {2.0f};
    float targets[] = {1.0f};

    nimcp_loss_result_t result;
    nimcp_result_t err = nimcp_loss_forward_backward(ctx, predictions, targets, 1, 1, &result);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_NEAR(result.loss_value, 1.0f, EPSILON);  // (2-1)^4 = 1
    EXPECT_NEAR(result.gradients[0], 4.0f, EPSILON);  // 4*(2-1)^3 = 4

    nimcp_loss_result_free(&result);
    nimcp_loss_destroy(ctx);
}

// ============================================================================
// Gradient Clipping Integration Tests
// ============================================================================

TEST_F(LossFunctionsTest, GradientClipping_Enabled) {
    nimcp_loss_config_t config = nimcp_loss_default_config(NIMCP_LOSS_MSE);
    config.clip_gradients = true;
    config.gradient_clip_value = 0.5f;

    nimcp_loss_context_t* ctx = nimcp_loss_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    // Large prediction error to cause large gradients
    float predictions[] = {100.0f};
    float targets[] = {0.0f};
    float gradients[1];

    nimcp_loss_backward(ctx, predictions, targets, 1, 1, gradients);

    // Gradient should be clipped to max value
    EXPECT_LE(std::abs(gradients[0]), config.gradient_clip_value);

    nimcp_loss_stats_t stats;
    nimcp_loss_get_stats(ctx, &stats);
    EXPECT_GT(stats.gradient_clips, 0u);

    nimcp_loss_destroy(ctx);
}

}  // namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
