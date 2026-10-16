//=============================================================================
// test_training_pipeline_integration.cpp - Integration Tests for Training Pipeline
//=============================================================================
/**
 * @file test_training_pipeline_integration.cpp
 * @brief Integration tests for complete GPU training pipeline with recovery
 *
 * WHAT: End-to-end tests for training workflows
 * WHY:  Verify all training components work together correctly
 * HOW:  Tests complete forward->loss->backward->optimize cycles
 *
 * TEST SCENARIOS:
 * - Single training step (forward, loss, backward, optimize)
 * - Multiple training iterations
 * - Different loss functions with optimizers
 * - Gradient clipping integration
 * - Learning rate scheduling
 * - Recovery from errors during training
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>

extern "C" {
#include "gpu/training/nimcp_training_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class TrainingPipelineIntegrationTest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* ctx = nullptr;

    void SetUp() override {
        // Initialize GPU recovery system
        if (!nimcp_gpu_recovery_is_initialized()) {
            nimcp_gpu_recovery_init(NULL);
        }

        // Create GPU context
        ctx = nimcp_gpu_context_create(0);
        if (!ctx) {
            GTEST_SKIP() << "GPU context creation failed - skipping GPU tests";
        }
    }

    void TearDown() override {
        if (ctx) {
            nimcp_gpu_context_destroy(ctx);
            ctx = nullptr;
        }
    }

    // Helper to create test tensor with specific data
    nimcp_gpu_tensor_t* create_test_tensor(const std::vector<size_t>& dims,
                                           const std::vector<float>& data) {
        nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(
            ctx, dims.data(), dims.size(), NIMCP_DTYPE_FLOAT32);
        if (tensor && !data.empty()) {
            nimcp_gpu_tensor_copy_from_host(ctx, tensor, data.data(),
                                            data.size() * sizeof(float));
        }
        return tensor;
    }

    // Helper to create test tensor with random data
    nimcp_gpu_tensor_t* create_random_tensor(const std::vector<size_t>& dims,
                                              float min_val = -1.0f,
                                              float max_val = 1.0f) {
        size_t total_elements = 1;
        for (size_t d : dims) total_elements *= d;

        std::vector<float> data(total_elements);
        std::random_device rd;
        std::mt19937 gen(42);  // Fixed seed for reproducibility
        std::uniform_real_distribution<float> dist(min_val, max_val);
        for (auto& v : data) v = dist(gen);

        return create_test_tensor(dims, data);
    }

    // Helper to create zero tensor
    nimcp_gpu_tensor_t* create_zero_tensor(const std::vector<size_t>& dims) {
        size_t total_elements = 1;
        for (size_t d : dims) total_elements *= d;
        std::vector<float> zeros(total_elements, 0.0f);
        return create_test_tensor(dims, zeros);
    }

    // Helper to create tensor initialized with a constant
    nimcp_gpu_tensor_t* create_constant_tensor(const std::vector<size_t>& dims, float value) {
        size_t total_elements = 1;
        for (size_t d : dims) total_elements *= d;
        std::vector<float> data(total_elements, value);
        return create_test_tensor(dims, data);
    }

    // Helper to get tensor data from GPU
    std::vector<float> get_tensor_data(nimcp_gpu_tensor_t* tensor) {
        size_t total_elements = 1;
        for (uint32_t i = 0; i < tensor->ndim; i++) {
            total_elements *= tensor->dims[i];
        }
        std::vector<float> data(total_elements);
        nimcp_gpu_tensor_copy_to_host(ctx, tensor, data.data(),
                                      data.size() * sizeof(float));
        return data;
    }

    // Check if tensor contains NaN or Inf
    bool has_nan_or_inf(nimcp_gpu_tensor_t* tensor) {
        auto data = get_tensor_data(tensor);
        for (float v : data) {
            if (std::isnan(v) || std::isinf(v)) return true;
        }
        return false;
    }

    // Calculate L2 norm of tensor
    float tensor_norm(nimcp_gpu_tensor_t* tensor) {
        auto data = get_tensor_data(tensor);
        float sum = 0.0f;
        for (float v : data) sum += v * v;
        return std::sqrt(sum);
    }

    // Calculate mean of tensor
    float tensor_mean(nimcp_gpu_tensor_t* tensor) {
        auto data = get_tensor_data(tensor);
        float sum = 0.0f;
        for (float v : data) sum += v;
        return sum / data.size();
    }
};

//=============================================================================
// Single Training Step Tests
//=============================================================================

TEST_F(TrainingPipelineIntegrationTest, SingleStepMSEWithSGD) {
    // Simple training step: MSE loss + SGD optimizer
    // Simulates a linear layer output

    // Create "predictions" and "targets"
    nimcp_gpu_tensor_t* pred = create_random_tensor({8, 16}, 0.0f, 1.0f);
    nimcp_gpu_tensor_t* target = create_random_tensor({8, 16}, 0.0f, 1.0f);
    nimcp_gpu_tensor_t* grad = create_zero_tensor({8, 16});

    ASSERT_NE(pred, nullptr);
    ASSERT_NE(target, nullptr);
    ASSERT_NE(grad, nullptr);

    // Compute MSE loss and gradient
    float loss = 0.0f;
    bool result = nimcp_gpu_loss_mse(ctx, pred, target, &loss, grad);
    EXPECT_TRUE(result);
    EXPECT_GT(loss, 0.0f);  // Loss should be positive
    EXPECT_FALSE(has_nan_or_inf(grad));

    // Create optimizer state
    nimcp_optim_state_t* state = nimcp_optim_state_create(ctx, NIMCP_OPTIM_SGD, pred, 0.01f);
    ASSERT_NE(state, nullptr);

    // Save original prediction values
    float original_norm = tensor_norm(pred);

    // Apply SGD update
    result = nimcp_gpu_optim_sgd(ctx, pred, grad, state);
    EXPECT_TRUE(result);
    EXPECT_FALSE(has_nan_or_inf(pred));

    // Predictions should have changed
    float updated_norm = tensor_norm(pred);
    EXPECT_NE(original_norm, updated_norm);

    // Clean up
    nimcp_optim_state_destroy(state);
    nimcp_gpu_tensor_destroy(ctx, pred);
    nimcp_gpu_tensor_destroy(ctx, target);
    nimcp_gpu_tensor_destroy(ctx, grad);
}

TEST_F(TrainingPipelineIntegrationTest, SingleStepCrossEntropyWithAdam) {
    // Training step with cross entropy loss + Adam optimizer
    // Simulates classification output

    size_t batch_size = 16;
    size_t num_classes = 10;

    // Create logits and one-hot targets
    nimcp_gpu_tensor_t* logits = create_random_tensor({batch_size, num_classes}, -2.0f, 2.0f);
    nimcp_gpu_tensor_t* target = create_zero_tensor({batch_size, num_classes});
    nimcp_gpu_tensor_t* grad = create_zero_tensor({batch_size, num_classes});

    ASSERT_NE(logits, nullptr);
    ASSERT_NE(target, nullptr);
    ASSERT_NE(grad, nullptr);

    // Set random class labels (one-hot)
    std::vector<float> target_data(batch_size * num_classes, 0.0f);
    std::mt19937 gen(42);
    std::uniform_int_distribution<int> dist(0, num_classes - 1);
    for (size_t i = 0; i < batch_size; i++) {
        int label = dist(gen);
        target_data[i * num_classes + label] = 1.0f;
    }
    nimcp_gpu_tensor_copy_from_host(ctx, target, target_data.data(),
                                    target_data.size() * sizeof(float));

    // Compute cross entropy loss
    float loss = 0.0f;
    bool result = nimcp_gpu_loss_cross_entropy(ctx, logits, target, &loss, grad, 1);
    EXPECT_TRUE(result);
    EXPECT_GT(loss, 0.0f);
    EXPECT_FALSE(has_nan_or_inf(grad));

    // Create Adam optimizer state
    nimcp_optim_state_t* state = nimcp_optim_state_create(ctx, NIMCP_OPTIM_ADAM, logits, 0.001f);
    ASSERT_NE(state, nullptr);
    state->beta1 = 0.9f;
    state->beta2 = 0.999f;
    state->eps = 1e-8f;

    // Apply Adam update
    result = nimcp_gpu_optim_adam(ctx, logits, grad, state);
    EXPECT_TRUE(result);
    EXPECT_FALSE(has_nan_or_inf(logits));

    // Clean up
    nimcp_optim_state_destroy(state);
    nimcp_gpu_tensor_destroy(ctx, logits);
    nimcp_gpu_tensor_destroy(ctx, target);
    nimcp_gpu_tensor_destroy(ctx, grad);
}

//=============================================================================
// Multiple Training Iterations Tests
//=============================================================================

TEST_F(TrainingPipelineIntegrationTest, MultipleIterationsMSE) {
    // Train for multiple iterations and verify loss decreases

    nimcp_gpu_tensor_t* pred = create_random_tensor({4, 8}, -1.0f, 1.0f);
    nimcp_gpu_tensor_t* target = create_constant_tensor({4, 8}, 0.5f);
    nimcp_gpu_tensor_t* grad = create_zero_tensor({4, 8});

    nimcp_optim_state_t* state = nimcp_optim_state_create(ctx, NIMCP_OPTIM_ADAM, pred, 0.1f);
    state->beta1 = 0.9f;
    state->beta2 = 0.999f;
    state->eps = 1e-8f;

    float initial_loss = 0.0f;
    float final_loss = 0.0f;

    const int num_iterations = 100;
    for (int i = 0; i < num_iterations; i++) {
        float loss = 0.0f;

        // Zero gradients
        nimcp_gpu_gradient_zero(ctx, grad);

        // Compute loss and gradient
        bool result = nimcp_gpu_loss_mse(ctx, pred, target, &loss, grad);
        ASSERT_TRUE(result);
        ASSERT_FALSE(has_nan_or_inf(grad));

        if (i == 0) initial_loss = loss;
        if (i == num_iterations - 1) final_loss = loss;

        // Apply optimizer step
        result = nimcp_gpu_optim_adam(ctx, pred, grad, state);
        ASSERT_TRUE(result);
        ASSERT_FALSE(has_nan_or_inf(pred));
    }

    // Loss should decrease over training
    EXPECT_LT(final_loss, initial_loss);
    // Loss should be close to zero (target is constant 0.5)
    EXPECT_LT(final_loss, 0.01f);

    nimcp_optim_state_destroy(state);
    nimcp_gpu_tensor_destroy(ctx, pred);
    nimcp_gpu_tensor_destroy(ctx, target);
    nimcp_gpu_tensor_destroy(ctx, grad);
}

TEST_F(TrainingPipelineIntegrationTest, MultipleIterationsDifferentOptimizers) {
    // Compare convergence with different optimizers

    std::vector<nimcp_optim_type_t> optimizers = {
        NIMCP_OPTIM_SGD,
        NIMCP_OPTIM_ADAM,
        NIMCP_OPTIM_RMSPROP,
        NIMCP_OPTIM_ADAGRAD
    };

    for (auto opt_type : optimizers) {
        nimcp_gpu_tensor_t* pred = create_random_tensor({4, 8}, -1.0f, 1.0f);
        nimcp_gpu_tensor_t* target = create_constant_tensor({4, 8}, 0.0f);
        nimcp_gpu_tensor_t* grad = create_zero_tensor({4, 8});

        float lr = (opt_type == NIMCP_OPTIM_SGD) ? 0.5f : 0.1f;
        nimcp_optim_state_t* state = nimcp_optim_state_create(ctx, opt_type, pred, lr);

        float initial_loss = 0.0f;
        float final_loss = 0.0f;

        const int num_iterations = 50;
        for (int i = 0; i < num_iterations; i++) {
            float loss = 0.0f;

            nimcp_gpu_gradient_zero(ctx, grad);
            nimcp_gpu_loss_mse(ctx, pred, target, &loss, grad);

            if (i == 0) initial_loss = loss;
            if (i == num_iterations - 1) final_loss = loss;

            // Apply appropriate optimizer
            switch (opt_type) {
                case NIMCP_OPTIM_SGD:
                    nimcp_gpu_optim_sgd(ctx, pred, grad, state);
                    break;
                case NIMCP_OPTIM_ADAM:
                    nimcp_gpu_optim_adam(ctx, pred, grad, state);
                    break;
                case NIMCP_OPTIM_RMSPROP:
                    nimcp_gpu_optim_rmsprop(ctx, pred, grad, state);
                    break;
                case NIMCP_OPTIM_ADAGRAD:
                    nimcp_gpu_optim_adagrad(ctx, pred, grad, state);
                    break;
                default:
                    break;
            }
        }

        EXPECT_LT(final_loss, initial_loss) << "Optimizer " << opt_type << " failed to reduce loss";

        nimcp_optim_state_destroy(state);
        nimcp_gpu_tensor_destroy(ctx, pred);
        nimcp_gpu_tensor_destroy(ctx, target);
        nimcp_gpu_tensor_destroy(ctx, grad);
    }
}

//=============================================================================
// Gradient Clipping Integration Tests
//=============================================================================

TEST_F(TrainingPipelineIntegrationTest, GradientClippingNorm) {
    // Test gradient clipping by norm during training

    nimcp_gpu_tensor_t* pred = create_random_tensor({8, 32}, -10.0f, 10.0f);
    nimcp_gpu_tensor_t* target = create_random_tensor({8, 32}, -10.0f, 10.0f);
    nimcp_gpu_tensor_t* grad = create_zero_tensor({8, 32});

    // Compute loss (will produce large gradients due to large values)
    float loss = 0.0f;
    bool result = nimcp_gpu_loss_mse(ctx, pred, target, &loss, grad);
    EXPECT_TRUE(result);

    // Get gradient norm before clipping
    float grad_norm_before = tensor_norm(grad);

    // Clip gradients by norm
    float max_norm = 1.0f;
    float total_norm = 0.0f;
    nimcp_gpu_tensor_t* grads[] = {grad};
    result = nimcp_gpu_gradient_clip_norm(ctx, grads, 1, max_norm, &total_norm);
    EXPECT_TRUE(result);

    // Get gradient norm after clipping
    float grad_norm_after = tensor_norm(grad);

    // If gradient was larger than max_norm, it should be clipped
    if (grad_norm_before > max_norm) {
        EXPECT_LE(grad_norm_after, max_norm * 1.01f);  // Small tolerance for floating point
    }

    nimcp_gpu_tensor_destroy(ctx, pred);
    nimcp_gpu_tensor_destroy(ctx, target);
    nimcp_gpu_tensor_destroy(ctx, grad);
}

TEST_F(TrainingPipelineIntegrationTest, GradientClippingValue) {
    // Test gradient clipping by value

    // Create tensor with some extreme values
    std::vector<float> grad_data = {-10.0f, 5.0f, -3.0f, 100.0f, -50.0f, 0.5f};
    nimcp_gpu_tensor_t* grad = create_test_tensor({2, 3}, grad_data);

    float clip_value = 5.0f;
    bool result = nimcp_gpu_gradient_clip_value(ctx, grad, clip_value);
    EXPECT_TRUE(result);

    auto clipped_data = get_tensor_data(grad);
    for (float v : clipped_data) {
        EXPECT_GE(v, -clip_value);
        EXPECT_LE(v, clip_value);
    }

    nimcp_gpu_tensor_destroy(ctx, grad);
}

TEST_F(TrainingPipelineIntegrationTest, TrainingWithGradientClipping) {
    // Full training loop with gradient clipping

    nimcp_gpu_tensor_t* pred = create_random_tensor({8, 16}, -5.0f, 5.0f);
    nimcp_gpu_tensor_t* target = create_constant_tensor({8, 16}, 0.0f);
    nimcp_gpu_tensor_t* grad = create_zero_tensor({8, 16});

    nimcp_optim_state_t* state = nimcp_optim_state_create(ctx, NIMCP_OPTIM_ADAM, pred, 0.01f);

    float initial_loss = 0.0f;
    float final_loss = 0.0f;

    const int num_iterations = 50;
    float max_norm = 1.0f;

    for (int i = 0; i < num_iterations; i++) {
        float loss = 0.0f;

        // Zero gradients
        nimcp_gpu_gradient_zero(ctx, grad);

        // Forward + backward (compute loss and gradient)
        nimcp_gpu_loss_mse(ctx, pred, target, &loss, grad);

        if (i == 0) initial_loss = loss;
        if (i == num_iterations - 1) final_loss = loss;

        // Clip gradients
        float total_norm = 0.0f;
        nimcp_gpu_tensor_t* grads[] = {grad};
        nimcp_gpu_gradient_clip_norm(ctx, grads, 1, max_norm, &total_norm);

        // Optimizer step
        nimcp_gpu_optim_adam(ctx, pred, grad, state);
    }

    EXPECT_LT(final_loss, initial_loss);

    nimcp_optim_state_destroy(state);
    nimcp_gpu_tensor_destroy(ctx, pred);
    nimcp_gpu_tensor_destroy(ctx, target);
    nimcp_gpu_tensor_destroy(ctx, grad);
}

//=============================================================================
// Learning Rate Scheduling Tests
//=============================================================================

TEST_F(TrainingPipelineIntegrationTest, StepLearningRateScheduler) {
    float initial_lr = 0.1f;
    float gamma = 0.5f;
    uint64_t step_size = 10;

    // Test LR at various steps
    EXPECT_FLOAT_EQ(nimcp_lr_step(initial_lr, 0, step_size, gamma), 0.1f);
    EXPECT_FLOAT_EQ(nimcp_lr_step(initial_lr, 5, step_size, gamma), 0.1f);
    EXPECT_FLOAT_EQ(nimcp_lr_step(initial_lr, 10, step_size, gamma), 0.05f);
    EXPECT_FLOAT_EQ(nimcp_lr_step(initial_lr, 20, step_size, gamma), 0.025f);
    EXPECT_FLOAT_EQ(nimcp_lr_step(initial_lr, 30, step_size, gamma), 0.0125f);
}

TEST_F(TrainingPipelineIntegrationTest, CosineLearningRateScheduler) {
    float max_lr = 0.1f;
    float min_lr = 0.001f;
    uint64_t total_steps = 100;

    // At step 0, should be max_lr
    float lr_start = nimcp_lr_cosine(max_lr, min_lr, 0, total_steps);
    EXPECT_NEAR(lr_start, max_lr, 0.001f);

    // At step total_steps, should be min_lr
    float lr_end = nimcp_lr_cosine(max_lr, min_lr, total_steps, total_steps);
    EXPECT_NEAR(lr_end, min_lr, 0.001f);

    // At step total_steps/2, should be approximately (max_lr + min_lr) / 2
    float lr_mid = nimcp_lr_cosine(max_lr, min_lr, total_steps / 2, total_steps);
    float expected_mid = (max_lr + min_lr) / 2.0f;
    EXPECT_NEAR(lr_mid, expected_mid, 0.01f);
}

TEST_F(TrainingPipelineIntegrationTest, WarmupLearningRateScheduler) {
    float max_lr = 0.1f;
    uint64_t warmup_steps = 10;
    uint64_t total_steps = 100;

    // During warmup, LR should increase linearly
    float lr_warmup_start = nimcp_lr_warmup_linear(max_lr, 0, warmup_steps, total_steps);
    EXPECT_LT(lr_warmup_start, max_lr);

    float lr_warmup_mid = nimcp_lr_warmup_linear(max_lr, warmup_steps / 2, warmup_steps, total_steps);
    EXPECT_GT(lr_warmup_mid, lr_warmup_start);

    // At end of warmup, should be at max_lr
    float lr_warmup_end = nimcp_lr_warmup_linear(max_lr, warmup_steps, warmup_steps, total_steps);
    EXPECT_NEAR(lr_warmup_end, max_lr, 0.01f);

    // After warmup, should decay
    float lr_after_warmup = nimcp_lr_warmup_linear(max_lr, total_steps, warmup_steps, total_steps);
    EXPECT_LT(lr_after_warmup, max_lr);
}

TEST_F(TrainingPipelineIntegrationTest, ExponentialLearningRateScheduler) {
    float initial_lr = 0.1f;
    float decay_rate = 0.01f;

    // LR should decay exponentially
    float lr_0 = nimcp_lr_exponential(initial_lr, 0, decay_rate);
    EXPECT_FLOAT_EQ(lr_0, initial_lr);

    float lr_100 = nimcp_lr_exponential(initial_lr, 100, decay_rate);
    EXPECT_LT(lr_100, lr_0);

    float lr_200 = nimcp_lr_exponential(initial_lr, 200, decay_rate);
    EXPECT_LT(lr_200, lr_100);
}

TEST_F(TrainingPipelineIntegrationTest, TrainingWithLearningRateSchedule) {
    // Train with cosine LR schedule

    nimcp_gpu_tensor_t* pred = create_random_tensor({4, 8}, -1.0f, 1.0f);
    nimcp_gpu_tensor_t* target = create_constant_tensor({4, 8}, 0.0f);
    nimcp_gpu_tensor_t* grad = create_zero_tensor({4, 8});

    nimcp_optim_state_t* state = nimcp_optim_state_create(ctx, NIMCP_OPTIM_SGD, pred, 0.1f);

    float max_lr = 0.5f;
    float min_lr = 0.001f;
    const uint64_t total_steps = 100;

    float initial_loss = 0.0f;
    float final_loss = 0.0f;

    for (uint64_t step = 0; step < total_steps; step++) {
        // Update learning rate
        state->lr = nimcp_lr_cosine(max_lr, min_lr, step, total_steps);

        float loss = 0.0f;
        nimcp_gpu_gradient_zero(ctx, grad);
        nimcp_gpu_loss_mse(ctx, pred, target, &loss, grad);

        if (step == 0) initial_loss = loss;
        if (step == total_steps - 1) final_loss = loss;

        nimcp_gpu_optim_sgd(ctx, pred, grad, state);
    }

    EXPECT_LT(final_loss, initial_loss);

    nimcp_optim_state_destroy(state);
    nimcp_gpu_tensor_destroy(ctx, pred);
    nimcp_gpu_tensor_destroy(ctx, target);
    nimcp_gpu_tensor_destroy(ctx, grad);
}

//=============================================================================
// Gradient Accumulation Tests
//=============================================================================

TEST_F(TrainingPipelineIntegrationTest, GradientAccumulation) {
    // Test gradient accumulation for simulating larger batch sizes

    nimcp_gpu_tensor_t* pred1 = create_random_tensor({4, 8});
    nimcp_gpu_tensor_t* pred2 = create_random_tensor({4, 8});
    nimcp_gpu_tensor_t* target = create_constant_tensor({4, 8}, 0.5f);

    nimcp_gpu_tensor_t* grad = create_zero_tensor({4, 8});
    nimcp_gpu_tensor_t* grad_accum = create_zero_tensor({4, 8});

    float loss1 = 0.0f, loss2 = 0.0f;

    // Compute gradient for first mini-batch
    nimcp_gpu_loss_mse(ctx, pred1, target, &loss1, grad);

    // Accumulate
    bool result = nimcp_gpu_gradient_accumulate(ctx, grad, grad_accum);
    EXPECT_TRUE(result);

    // Zero grad for next mini-batch
    nimcp_gpu_gradient_zero(ctx, grad);

    // Compute gradient for second mini-batch
    nimcp_gpu_loss_mse(ctx, pred2, target, &loss2, grad);

    // Accumulate
    result = nimcp_gpu_gradient_accumulate(ctx, grad, grad_accum);
    EXPECT_TRUE(result);

    // Accumulated gradient should be sum of both
    float accum_norm = tensor_norm(grad_accum);
    EXPECT_GT(accum_norm, 0.0f);

    // Scale by 1/2 for averaging
    result = nimcp_gpu_gradient_scale(ctx, grad_accum, 0.5f);
    EXPECT_TRUE(result);

    nimcp_gpu_tensor_destroy(ctx, pred1);
    nimcp_gpu_tensor_destroy(ctx, pred2);
    nimcp_gpu_tensor_destroy(ctx, target);
    nimcp_gpu_tensor_destroy(ctx, grad);
    nimcp_gpu_tensor_destroy(ctx, grad_accum);
}

//=============================================================================
// Different Loss Functions Tests
//=============================================================================

TEST_F(TrainingPipelineIntegrationTest, TrainingWithFocalLoss) {
    // Focal loss for imbalanced classification

    size_t batch_size = 16;
    size_t num_classes = 5;

    nimcp_gpu_tensor_t* pred = create_random_tensor({batch_size, num_classes}, 0.01f, 0.99f);
    nimcp_gpu_tensor_t* target = create_zero_tensor({batch_size, num_classes});
    nimcp_gpu_tensor_t* grad = create_zero_tensor({batch_size, num_classes});

    // Set one class per sample
    std::vector<float> target_data(batch_size * num_classes, 0.0f);
    for (size_t i = 0; i < batch_size; i++) {
        target_data[i * num_classes] = 1.0f;  // All samples have class 0
    }
    nimcp_gpu_tensor_copy_from_host(ctx, target, target_data.data(),
                                    target_data.size() * sizeof(float));

    float alpha = 0.25f;
    float gamma = 2.0f;
    float loss = 0.0f;

    bool result = nimcp_gpu_loss_focal(ctx, pred, target, &loss, grad, alpha, gamma);
    EXPECT_TRUE(result);
    EXPECT_GT(loss, 0.0f);
    EXPECT_FALSE(has_nan_or_inf(grad));

    nimcp_gpu_tensor_destroy(ctx, pred);
    nimcp_gpu_tensor_destroy(ctx, target);
    nimcp_gpu_tensor_destroy(ctx, grad);
}

TEST_F(TrainingPipelineIntegrationTest, TrainingWithHuberLoss) {
    // Huber loss for robust regression

    nimcp_gpu_tensor_t* pred = create_random_tensor({8, 16}, -5.0f, 5.0f);
    nimcp_gpu_tensor_t* target = create_random_tensor({8, 16}, -5.0f, 5.0f);
    nimcp_gpu_tensor_t* grad = create_zero_tensor({8, 16});

    float delta = 1.0f;
    float loss = 0.0f;

    bool result = nimcp_gpu_loss_huber(ctx, pred, target, &loss, grad, delta);
    EXPECT_TRUE(result);
    EXPECT_GT(loss, 0.0f);
    EXPECT_FALSE(has_nan_or_inf(grad));

    // Compare with MSE - Huber should be more robust to outliers
    nimcp_gpu_tensor_destroy(ctx, pred);
    nimcp_gpu_tensor_destroy(ctx, target);
    nimcp_gpu_tensor_destroy(ctx, grad);
}

TEST_F(TrainingPipelineIntegrationTest, TrainingWithMAELoss) {
    // MAE loss (L1 loss)

    nimcp_gpu_tensor_t* pred = create_random_tensor({8, 16}, -1.0f, 1.0f);
    nimcp_gpu_tensor_t* target = create_constant_tensor({8, 16}, 0.0f);
    nimcp_gpu_tensor_t* grad = create_zero_tensor({8, 16});

    nimcp_optim_state_t* state = nimcp_optim_state_create(ctx, NIMCP_OPTIM_ADAM, pred, 0.1f);

    float initial_loss = 0.0f;
    float final_loss = 0.0f;

    for (int i = 0; i < 50; i++) {
        float loss = 0.0f;
        nimcp_gpu_gradient_zero(ctx, grad);
        nimcp_gpu_loss_mae(ctx, pred, target, &loss, grad);

        if (i == 0) initial_loss = loss;
        if (i == 49) final_loss = loss;

        nimcp_gpu_optim_adam(ctx, pred, grad, state);
    }

    EXPECT_LT(final_loss, initial_loss);

    nimcp_optim_state_destroy(state);
    nimcp_gpu_tensor_destroy(ctx, pred);
    nimcp_gpu_tensor_destroy(ctx, target);
    nimcp_gpu_tensor_destroy(ctx, grad);
}

TEST_F(TrainingPipelineIntegrationTest, TrainingWithBCELoss) {
    // Binary cross entropy loss

    nimcp_gpu_tensor_t* pred = create_random_tensor({8, 1}, 0.1f, 0.9f);
    nimcp_gpu_tensor_t* target = create_test_tensor({8, 1}, {1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f});
    nimcp_gpu_tensor_t* grad = create_zero_tensor({8, 1});

    float loss = 0.0f;
    bool result = nimcp_gpu_loss_bce(ctx, pred, target, &loss, grad);
    EXPECT_TRUE(result);
    EXPECT_GT(loss, 0.0f);
    EXPECT_FALSE(has_nan_or_inf(grad));

    nimcp_gpu_tensor_destroy(ctx, pred);
    nimcp_gpu_tensor_destroy(ctx, target);
    nimcp_gpu_tensor_destroy(ctx, grad);
}

//=============================================================================
// Recovery Integration Tests
//=============================================================================

TEST_F(TrainingPipelineIntegrationTest, RecoveryAfterNumericalError) {
    // Test recovery from numerical errors during training

    // Create predictions with potential for numerical issues
    std::vector<float> extreme_preds(32, 0.0f);
    for (int i = 0; i < 32; i++) {
        extreme_preds[i] = (i % 2 == 0) ? 1e-30f : 1.0f - 1e-30f;
    }

    nimcp_gpu_tensor_t* pred = create_test_tensor({4, 8}, extreme_preds);
    nimcp_gpu_tensor_t* target = create_constant_tensor({4, 8}, 0.5f);
    nimcp_gpu_tensor_t* grad = create_zero_tensor({4, 8});

    float loss = 0.0f;
    bool result = nimcp_gpu_loss_bce(ctx, pred, target, &loss, grad);

    // Should either succeed or gracefully fail with recovery
    if (result) {
        // If successful, verify no NaN/Inf propagated
        EXPECT_FALSE(has_nan_or_inf(grad));
    }

    // Recovery system should still be operational
    EXPECT_TRUE(nimcp_gpu_recovery_is_initialized());

    nimcp_gpu_tensor_destroy(ctx, pred);
    nimcp_gpu_tensor_destroy(ctx, target);
    nimcp_gpu_tensor_destroy(ctx, grad);
}

TEST_F(TrainingPipelineIntegrationTest, ContinueTrainingAfterRecovery) {
    // Verify training can continue after a potential recovery event

    nimcp_gpu_tensor_t* pred = create_random_tensor({8, 16});
    nimcp_gpu_tensor_t* target = create_constant_tensor({8, 16}, 0.0f);
    nimcp_gpu_tensor_t* grad = create_zero_tensor({8, 16});

    nimcp_optim_state_t* state = nimcp_optim_state_create(ctx, NIMCP_OPTIM_ADAM, pred, 0.01f);

    // Run several training steps
    for (int i = 0; i < 20; i++) {
        float loss = 0.0f;
        nimcp_gpu_gradient_zero(ctx, grad);
        bool loss_ok = nimcp_gpu_loss_mse(ctx, pred, target, &loss, grad);
        ASSERT_TRUE(loss_ok);

        bool optim_ok = nimcp_gpu_optim_adam(ctx, pred, grad, state);
        ASSERT_TRUE(optim_ok);
    }

    // All operations should have completed successfully
    EXPECT_FALSE(has_nan_or_inf(pred));
    EXPECT_TRUE(nimcp_gpu_recovery_is_initialized());

    nimcp_optim_state_destroy(state);
    nimcp_gpu_tensor_destroy(ctx, pred);
    nimcp_gpu_tensor_destroy(ctx, target);
    nimcp_gpu_tensor_destroy(ctx, grad);
}

//=============================================================================
// Weight Decay Tests
//=============================================================================

TEST_F(TrainingPipelineIntegrationTest, AdamWWeightDecay) {
    // Test AdamW with weight decay

    nimcp_gpu_tensor_t* param = create_random_tensor({8, 16}, -1.0f, 1.0f);
    nimcp_gpu_tensor_t* grad = create_constant_tensor({8, 16}, 0.0f);  // Zero gradient

    nimcp_optim_state_t* state = nimcp_optim_state_create(ctx, NIMCP_OPTIM_ADAMW, param, 0.1f);
    state->weight_decay = 0.1f;

    float initial_norm = tensor_norm(param);

    // With zero gradient, weight decay should still reduce weights
    bool result = nimcp_gpu_optim_adamw(ctx, param, grad, state);
    EXPECT_TRUE(result);

    float final_norm = tensor_norm(param);

    // Weights should have decreased due to weight decay
    EXPECT_LT(final_norm, initial_norm);

    nimcp_optim_state_destroy(state);
    nimcp_gpu_tensor_destroy(ctx, param);
    nimcp_gpu_tensor_destroy(ctx, grad);
}
