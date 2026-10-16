/**
 * @file test_loss_functions_integration.cpp
 * @brief Integration tests for loss functions module
 *
 * Tests loss functions integration with:
 * - Security module (module registration, validation)
 * - Memory management (allocation patterns, cleanup)
 * - Training pipeline (forward/backward passes)
 * - Other training components (optimizers, regularization)
 *
 * Phase TM-2: Loss Functions Integration Testing
 */

#include <gtest/gtest.h>
#include <cmath>
#include <random>
#include <thread>
#include <vector>
#include <atomic>

// Headers have their own extern "C" guards
#include "middleware/training/nimcp_loss_functions.h"
#include "security/nimcp_security_integration.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/validation/nimcp_common.h"

namespace {

constexpr float EPSILON = 1e-4f;
constexpr size_t BATCH_SIZE = 32;
constexpr size_t NUM_CLASSES = 10;
constexpr size_t INTEGRATION_ITERATIONS = 100;

/**
 * @brief Integration test fixture for loss functions
 */
class LossFunctionsIntegrationTest : public ::testing::Test {
protected:
    nimcp_sec_integration_t* security_ctx = nullptr;
    unified_mem_manager_t memory_mgr = nullptr;
    std::mt19937 rng{42};  // Fixed seed for reproducibility

    void SetUp() override {
        // Initialize security context
        security_ctx = nimcp_sec_integration_create();

        // Memory manager can be null - will use malloc/free
        memory_mgr = nullptr;
    }

    void TearDown() override {
        if (security_ctx) {
            nimcp_sec_integration_destroy(security_ctx);
            security_ctx = nullptr;
        }
    }

    // Helper: Generate random predictions (softmax-like)
    std::vector<float> generate_softmax_predictions(size_t batch_size, size_t num_classes) {
        std::vector<float> preds(batch_size * num_classes);
        std::uniform_real_distribution<float> dist(0.1f, 0.9f);

        for (size_t b = 0; b < batch_size; b++) {
            float sum = 0.0f;
            for (size_t c = 0; c < num_classes; c++) {
                preds[b * num_classes + c] = dist(rng);
                sum += preds[b * num_classes + c];
            }
            // Normalize to sum to 1
            for (size_t c = 0; c < num_classes; c++) {
                preds[b * num_classes + c] /= sum;
            }
        }
        return preds;
    }

    // Helper: Generate one-hot targets
    std::vector<float> generate_onehot_targets(size_t batch_size, size_t num_classes) {
        std::vector<float> targets(batch_size * num_classes, 0.0f);
        std::uniform_int_distribution<size_t> dist(0, num_classes - 1);

        for (size_t b = 0; b < batch_size; b++) {
            size_t class_idx = dist(rng);
            targets[b * num_classes + class_idx] = 1.0f;
        }
        return targets;
    }

    // Helper: Generate continuous targets for regression
    std::vector<float> generate_regression_targets(size_t count) {
        std::vector<float> targets(count);
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        for (size_t i = 0; i < count; i++) {
            targets[i] = dist(rng);
        }
        return targets;
    }
};

// ============================================================================
// Context Creation and Basic Integration Tests
// ============================================================================

TEST_F(LossFunctionsIntegrationTest, ContextCreation_AllLossTypes) {
    nimcp_loss_type_t types[] = {
        NIMCP_LOSS_MSE, NIMCP_LOSS_MAE, NIMCP_LOSS_CROSS_ENTROPY,
        NIMCP_LOSS_BINARY_CROSS_ENTROPY, NIMCP_LOSS_KL_DIVERGENCE
    };

    for (auto type : types) {
        nimcp_loss_config_t config = nimcp_loss_default_config(type);
        nimcp_loss_context_t* ctx = nimcp_loss_create(&config, security_ctx, memory_mgr);
        ASSERT_NE(ctx, nullptr) << "Failed to create context for loss type " << static_cast<int>(type);
        nimcp_loss_destroy(ctx);
    }
}

TEST_F(LossFunctionsIntegrationTest, BasicComputation_MSE) {
    nimcp_loss_config_t config = nimcp_loss_default_config(NIMCP_LOSS_MSE);
    nimcp_loss_context_t* ctx = nimcp_loss_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float predictions[] = {0.5f, 0.7f, 0.9f};
    float targets[] = {0.0f, 1.0f, 1.0f};
    nimcp_loss_result_t result;

    nimcp_result_t err = nimcp_loss_forward(ctx, predictions, targets, 1, 3, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GT(result.loss_value, 0.0f);
    EXPECT_FALSE(std::isnan(result.loss_value));

    nimcp_loss_destroy(ctx);
}

// ============================================================================
// Training Pipeline Integration Tests
// ============================================================================

TEST_F(LossFunctionsIntegrationTest, TrainingLoop_MSE_Convergence) {
    nimcp_loss_config_t config = nimcp_loss_default_config(NIMCP_LOSS_MSE);
    nimcp_loss_context_t* ctx = nimcp_loss_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    // Simple target
    float target = 1.0f;
    float prediction = 0.0f;
    float learning_rate = 0.1f;

    float initial_loss = 0.0f;
    float final_loss = 0.0f;

    // Training loop
    for (size_t i = 0; i < INTEGRATION_ITERATIONS; i++) {
        nimcp_loss_result_t result;
        nimcp_result_t err = nimcp_loss_forward_backward(ctx, &prediction, &target, 1, 1, &result);
        ASSERT_EQ(err, NIMCP_SUCCESS);

        if (i == 0) initial_loss = result.loss_value;
        final_loss = result.loss_value;

        // Gradient descent update
        prediction -= learning_rate * result.gradients[0];

        nimcp_loss_result_free(&result);
    }

    // Verify convergence
    EXPECT_LT(final_loss, initial_loss);
    EXPECT_NEAR(prediction, target, 0.1f);

    nimcp_loss_destroy(ctx);
}

TEST_F(LossFunctionsIntegrationTest, TrainingLoop_CrossEntropy_BatchProcessing) {
    nimcp_loss_config_t config = nimcp_loss_default_config(NIMCP_LOSS_CROSS_ENTROPY);
    nimcp_loss_context_t* ctx = nimcp_loss_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    auto predictions = generate_softmax_predictions(BATCH_SIZE, NUM_CLASSES);
    auto targets = generate_onehot_targets(BATCH_SIZE, NUM_CLASSES);

    float prev_loss = std::numeric_limits<float>::max();

    // Simulate multiple training iterations
    for (size_t iter = 0; iter < 10; iter++) {
        nimcp_loss_result_t result;
        nimcp_result_t err = nimcp_loss_forward_backward(
            ctx, predictions.data(), targets.data(), BATCH_SIZE, NUM_CLASSES, &result);
        ASSERT_EQ(err, NIMCP_SUCCESS);

        // Verify valid loss
        EXPECT_FALSE(std::isnan(result.loss_value));
        EXPECT_FALSE(std::isinf(result.loss_value));
        EXPECT_GT(result.loss_value, 0.0f);

        // Apply gradients
        for (size_t i = 0; i < predictions.size(); i++) {
            predictions[i] -= 0.01f * result.gradients[i];
            // Clamp to valid range
            predictions[i] = std::max(0.01f, std::min(0.99f, predictions[i]));
        }

        // Re-normalize to maintain softmax property
        for (size_t b = 0; b < BATCH_SIZE; b++) {
            float sum = 0.0f;
            for (size_t c = 0; c < NUM_CLASSES; c++) {
                sum += predictions[b * NUM_CLASSES + c];
            }
            for (size_t c = 0; c < NUM_CLASSES; c++) {
                predictions[b * NUM_CLASSES + c] /= sum;
            }
        }

        prev_loss = result.loss_value;
        nimcp_loss_result_free(&result);
    }

    // Verify stats tracking
    nimcp_loss_stats_t stats;
    nimcp_loss_get_stats(ctx, &stats);
    EXPECT_EQ(stats.forward_count, 10u);
    EXPECT_EQ(stats.backward_count, 10u);

    nimcp_loss_destroy(ctx);
}

TEST_F(LossFunctionsIntegrationTest, TrainingLoop_Huber_RobustRegression) {
    nimcp_loss_config_t config = nimcp_loss_default_config(NIMCP_LOSS_HUBER);
    config.params.huber.delta = 1.0f;

    nimcp_loss_context_t* ctx = nimcp_loss_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    // Data with outliers
    std::vector<float> predictions = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    std::vector<float> targets = {1.0f, 1.1f, 0.9f, 1.0f, 100.0f};  // Last is outlier

    float learning_rate = 0.1f;

    for (size_t iter = 0; iter < 50; iter++) {
        nimcp_loss_result_t result;
        nimcp_result_t err = nimcp_loss_forward_backward(
            ctx, predictions.data(), targets.data(), 5, 1, &result);
        ASSERT_EQ(err, NIMCP_SUCCESS);

        // Update all except the outlier position
        for (size_t i = 0; i < 4; i++) {
            predictions[i] -= learning_rate * result.gradients[i];
        }

        nimcp_loss_result_free(&result);
    }

    // Non-outlier predictions should converge toward 1.0 (within 0.5 tolerance)
    for (size_t i = 0; i < 4; i++) {
        EXPECT_NEAR(predictions[i], 1.0f, 0.5f);
    }

    nimcp_loss_destroy(ctx);
}

// ============================================================================
// Memory Integration Tests
// ============================================================================

TEST_F(LossFunctionsIntegrationTest, Memory_LargeAllocation) {
    nimcp_loss_config_t config = nimcp_loss_default_config(NIMCP_LOSS_MSE);
    config.use_memory_pool = true;

    nimcp_loss_context_t* ctx = nimcp_loss_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    // Large batch
    constexpr size_t LARGE_BATCH = 1000;
    constexpr size_t LARGE_DIM = 1000;

    std::vector<float> predictions(LARGE_BATCH * LARGE_DIM, 0.5f);
    std::vector<float> targets(LARGE_BATCH * LARGE_DIM, 0.7f);

    nimcp_loss_result_t result;
    nimcp_result_t err = nimcp_loss_forward_backward(
        ctx, predictions.data(), targets.data(), LARGE_BATCH, LARGE_DIM, &result);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    EXPECT_NE(result.gradients, nullptr);
    EXPECT_GT(result.loss_value, 0.0f);

    nimcp_loss_result_free(&result);
    nimcp_loss_destroy(ctx);
}

TEST_F(LossFunctionsIntegrationTest, Memory_RepeatedAllocations) {
    nimcp_loss_config_t config = nimcp_loss_default_config(NIMCP_LOSS_MSE);
    nimcp_loss_context_t* ctx = nimcp_loss_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float predictions[] = {0.5f};
    float targets[] = {1.0f};

    // Repeated forward/backward calls should not leak memory
    for (size_t i = 0; i < 1000; i++) {
        nimcp_loss_result_t result;
        nimcp_result_t err = nimcp_loss_forward_backward(
            ctx, predictions, targets, 1, 1, &result);
        ASSERT_EQ(err, NIMCP_SUCCESS);
        nimcp_loss_result_free(&result);
    }

    nimcp_loss_destroy(ctx);
}

// ============================================================================
// Cross-Loss Function Integration Tests
// ============================================================================

TEST_F(LossFunctionsIntegrationTest, MultiLoss_CombinedObjective) {
    // Create multiple loss contexts
    nimcp_loss_config_t mse_config = nimcp_loss_default_config(NIMCP_LOSS_MSE);
    nimcp_loss_config_t kl_config = nimcp_loss_default_config(NIMCP_LOSS_KL_DIVERGENCE);

    nimcp_loss_context_t* mse_ctx = nimcp_loss_create(&mse_config, security_ctx, memory_mgr);
    nimcp_loss_context_t* kl_ctx = nimcp_loss_create(&kl_config, security_ctx, memory_mgr);

    ASSERT_NE(mse_ctx, nullptr);
    ASSERT_NE(kl_ctx, nullptr);

    // Predictions and targets
    std::vector<float> predictions = {0.3f, 0.5f, 0.2f};
    std::vector<float> targets = {0.25f, 0.5f, 0.25f};

    nimcp_loss_result_t mse_result, kl_result;

    // Compute both losses
    nimcp_result_t err = nimcp_loss_forward_backward(
        mse_ctx, predictions.data(), targets.data(), 1, 3, &mse_result);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    err = nimcp_loss_forward_backward(
        kl_ctx, predictions.data(), targets.data(), 1, 3, &kl_result);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Combined loss
    float alpha = 0.5f;
    float combined_loss = alpha * mse_result.loss_value + (1.0f - alpha) * kl_result.loss_value;

    EXPECT_GT(combined_loss, 0.0f);
    EXPECT_FALSE(std::isnan(combined_loss));

    // Combined gradients
    std::vector<float> combined_grads(3);
    for (size_t i = 0; i < 3; i++) {
        combined_grads[i] = alpha * mse_result.gradients[i] +
                           (1.0f - alpha) * kl_result.gradients[i];
    }

    nimcp_loss_result_free(&mse_result);
    nimcp_loss_result_free(&kl_result);
    nimcp_loss_destroy(mse_ctx);
    nimcp_loss_destroy(kl_ctx);
}

// ============================================================================
// Concurrent Access Tests
// ============================================================================

TEST_F(LossFunctionsIntegrationTest, Concurrent_IndependentContexts) {
    constexpr size_t NUM_THREADS = 4;
    std::atomic<size_t> success_count{0};
    std::vector<std::thread> threads;

    for (size_t t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([this, &success_count, t]() {
            // Each thread creates its own context
            nimcp_loss_config_t config = nimcp_loss_default_config(
                static_cast<nimcp_loss_type_t>(t % 5));  // Vary loss types

            nimcp_loss_context_t* ctx = nimcp_loss_create(&config, security_ctx, memory_mgr);
            if (!ctx) return;

            std::mt19937 local_rng{static_cast<unsigned>(42 + t)};
            std::uniform_real_distribution<float> dist(0.0f, 1.0f);

            bool all_ok = true;
            for (size_t i = 0; i < 100 && all_ok; i++) {
                float pred = dist(local_rng);
                float tgt = dist(local_rng);

                nimcp_loss_result_t result;
                nimcp_result_t err = nimcp_loss_forward_backward(ctx, &pred, &tgt, 1, 1, &result);

                if (err != NIMCP_SUCCESS || std::isnan(result.loss_value)) {
                    all_ok = false;
                }

                nimcp_loss_result_free(&result);
            }

            nimcp_loss_destroy(ctx);

            if (all_ok) {
                success_count++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), NUM_THREADS);
}

// ============================================================================
// Gradient Flow Tests
// ============================================================================

TEST_F(LossFunctionsIntegrationTest, GradientFlow_NumericalGradientCheck) {
    nimcp_loss_config_t config = nimcp_loss_default_config(NIMCP_LOSS_MSE);
    nimcp_loss_context_t* ctx = nimcp_loss_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float prediction = 0.7f;
    float target = 0.3f;
    float eps = 1e-4f;

    // Compute analytical gradient
    nimcp_loss_result_t result;
    nimcp_result_t err = nimcp_loss_forward_backward(ctx, &prediction, &target, 1, 1, &result);
    ASSERT_EQ(err, NIMCP_SUCCESS);
    float analytical_grad = result.gradients[0];
    nimcp_loss_result_free(&result);

    // Compute numerical gradient
    float pred_plus = prediction + eps;
    float pred_minus = prediction - eps;

    nimcp_loss_result_t result_plus, result_minus;
    nimcp_loss_forward(ctx, &pred_plus, &target, 1, 1, &result_plus);
    nimcp_loss_forward(ctx, &pred_minus, &target, 1, 1, &result_minus);

    float numerical_grad = (result_plus.loss_value - result_minus.loss_value) / (2.0f * eps);

    EXPECT_NEAR(analytical_grad, numerical_grad, 1e-3f);

    nimcp_loss_destroy(ctx);
}

TEST_F(LossFunctionsIntegrationTest, GradientClipping_IntegratedWithTraining) {
    nimcp_loss_config_t config = nimcp_loss_default_config(NIMCP_LOSS_MSE);
    config.clip_gradients = true;
    config.gradient_clip_value = 1.0f;

    nimcp_loss_context_t* ctx = nimcp_loss_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    // Large difference to cause large gradient
    float prediction = 0.0f;
    float target = 100.0f;

    nimcp_loss_result_t result;
    nimcp_result_t err = nimcp_loss_forward_backward(ctx, &prediction, &target, 1, 1, &result);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Gradient should be clipped
    EXPECT_LE(std::abs(result.gradients[0]), config.gradient_clip_value + EPSILON);

    // Stats should track clips
    nimcp_loss_stats_t stats;
    nimcp_loss_get_stats(ctx, &stats);
    EXPECT_GT(stats.gradient_clips, 0u);

    nimcp_loss_result_free(&result);
    nimcp_loss_destroy(ctx);
}

// ============================================================================
// Edge Case Integration Tests
// ============================================================================

TEST_F(LossFunctionsIntegrationTest, EdgeCase_VerySmallValues) {
    nimcp_loss_config_t config = nimcp_loss_default_config(NIMCP_LOSS_BINARY_CROSS_ENTROPY);
    nimcp_loss_context_t* ctx = nimcp_loss_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    // Very small probabilities
    float predictions[] = {1e-7f, 1.0f - 1e-7f};
    float targets[] = {0.0f, 1.0f};

    nimcp_loss_result_t result;
    nimcp_result_t err = nimcp_loss_forward_backward(ctx, predictions, targets, 2, 1, &result);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Loss should be finite
    EXPECT_FALSE(std::isnan(result.loss_value));
    EXPECT_FALSE(std::isinf(result.loss_value));

    // Gradients should be finite
    EXPECT_FALSE(std::isnan(result.gradients[0]));
    EXPECT_FALSE(std::isnan(result.gradients[1]));

    nimcp_loss_result_free(&result);
    nimcp_loss_destroy(ctx);
}

TEST_F(LossFunctionsIntegrationTest, EdgeCase_IdenticalInputOutput) {
    nimcp_loss_config_t config = nimcp_loss_default_config(NIMCP_LOSS_MSE);
    nimcp_loss_context_t* ctx = nimcp_loss_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    // Perfect prediction
    float predictions[] = {0.5f, 0.3f, 0.8f};
    float targets[] = {0.5f, 0.3f, 0.8f};

    nimcp_loss_result_t result;
    nimcp_result_t err = nimcp_loss_forward_backward(ctx, predictions, targets, 1, 3, &result);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Loss should be zero or very close
    EXPECT_NEAR(result.loss_value, 0.0f, EPSILON);

    // Gradients should be zero
    for (size_t i = 0; i < 3; i++) {
        EXPECT_NEAR(result.gradients[i], 0.0f, EPSILON);
    }

    nimcp_loss_result_free(&result);
    nimcp_loss_destroy(ctx);
}

// ============================================================================
// Stress Tests
// ============================================================================

TEST_F(LossFunctionsIntegrationTest, Stress_RapidContextCreation) {
    // Use only common loss types (MSE, MAE, BCE, KL, Huber) that don't need special data
    nimcp_loss_type_t common_types[] = {
        NIMCP_LOSS_MSE, NIMCP_LOSS_MAE, NIMCP_LOSS_BINARY_CROSS_ENTROPY,
        NIMCP_LOSS_KL_DIVERGENCE, NIMCP_LOSS_HUBER
    };
    constexpr size_t num_types = sizeof(common_types) / sizeof(common_types[0]);

    for (size_t i = 0; i < 100; i++) {
        nimcp_loss_config_t config = nimcp_loss_default_config(common_types[i % num_types]);

        nimcp_loss_context_t* ctx = nimcp_loss_create(&config, security_ctx, memory_mgr);
        ASSERT_NE(ctx, nullptr) << "Failed at iteration " << i;

        float pred = 0.5f, tgt = 0.7f;
        nimcp_loss_result_t result;
        nimcp_result_t err = nimcp_loss_forward(ctx, &pred, &tgt, 1, 1, &result);
        EXPECT_EQ(err, NIMCP_SUCCESS);

        nimcp_loss_destroy(ctx);
    }
}

TEST_F(LossFunctionsIntegrationTest, Stress_LargeSequentialOperations) {
    nimcp_loss_config_t config = nimcp_loss_default_config(NIMCP_LOSS_MSE);
    nimcp_loss_context_t* ctx = nimcp_loss_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    constexpr size_t NUM_OPERATIONS = 10000;

    for (size_t i = 0; i < NUM_OPERATIONS; i++) {
        float pred = static_cast<float>(i % 100) / 100.0f;
        float tgt = static_cast<float>((i + 50) % 100) / 100.0f;

        nimcp_loss_result_t result;
        nimcp_result_t err = nimcp_loss_forward_backward(ctx, &pred, &tgt, 1, 1, &result);
        ASSERT_EQ(err, NIMCP_SUCCESS);
        nimcp_loss_result_free(&result);
    }

    nimcp_loss_stats_t stats;
    nimcp_loss_get_stats(ctx, &stats);
    EXPECT_EQ(stats.forward_count, NUM_OPERATIONS);
    EXPECT_EQ(stats.backward_count, NUM_OPERATIONS);

    nimcp_loss_destroy(ctx);
}

}  // namespace
