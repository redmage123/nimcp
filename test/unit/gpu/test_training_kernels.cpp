/**
 * @file test_training_kernels.cpp
 * @brief Comprehensive unit tests for GPU training operations
 *
 * WHAT: Tests GPU training kernels for loss, gradients, optimizers, backprop
 * WHY:  Verify training operations compute correctly with GPU acceleration
 * HOW:  Test all public API functions from nimcp_training_gpu.h
 *
 * TEST COVERAGE:
 * - Loss functions (MSE, MAE, cross-entropy, BCE, focal, Huber)
 * - Gradient operations (accumulate, scale, clip_norm, clip_value, zero)
 * - Optimizers (SGD, Adam, AdamW, RMSprop, AdaGrad)
 * - Backward passes (linear, relu, sigmoid, tanh, gelu, softmax)
 * - Batch normalization backward
 * - Layer normalization backward
 * - Dropout backward
 * - Learning rate schedulers
 * - Edge cases and error handling
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>
#include <numeric>

// GPU headers include CUDA headers that cannot be in extern "C" blocks
#include "gpu/training/nimcp_training_gpu.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/nimcp_execution_mode.h"

//=============================================================================
// Test Constants
//=============================================================================

static constexpr float FLOAT_TOLERANCE = 1e-5f;
static constexpr float GRAD_TOLERANCE = 1e-4f;
static constexpr float LOSS_TOLERANCE = 1e-4f;

//=============================================================================
// Test Fixture
//=============================================================================

/**
 * @brief Test fixture for GPU training kernel tests
 *
 * WHAT: Provides common setup/teardown for GPU training tests
 * WHY:  Ensure proper cleanup of GPU resources and tensors
 * HOW:  Automatically destroys context and tensors in TearDown()
 */
class GPUTrainingKernelTest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* ctx = nullptr;
    std::vector<nimcp_gpu_tensor_t*> tensors_to_cleanup;
    std::vector<nimcp_optim_state_t*> optim_states_to_cleanup;

    void SetUp() override {
        // Try to create GPU context
        ctx = nimcp_gpu_context_create_auto();
        if (!ctx) {
            ctx = nimcp_gpu_context_create(0);
        }
    }

    void TearDown() override {
        // Clean up optimizer states
        for (auto* state : optim_states_to_cleanup) {
            if (state) {
                nimcp_optim_state_destroy(state);
            }
        }
        optim_states_to_cleanup.clear();

        // Clean up tensors
        for (auto* tensor : tensors_to_cleanup) {
            if (tensor) {
                nimcp_gpu_tensor_destroy(tensor);
            }
        }
        tensors_to_cleanup.clear();

        // Destroy context
        if (ctx) {
            nimcp_gpu_context_destroy(ctx);
            ctx = nullptr;
        }
    }

    /**
     * @brief Register tensor for automatic cleanup
     */
    nimcp_gpu_tensor_t* track(nimcp_gpu_tensor_t* tensor) {
        if (tensor) {
            tensors_to_cleanup.push_back(tensor);
        }
        return tensor;
    }

    /**
     * @brief Register optimizer state for automatic cleanup
     */
    nimcp_optim_state_t* track_optim(nimcp_optim_state_t* state) {
        if (state) {
            optim_states_to_cleanup.push_back(state);
        }
        return state;
    }

    /**
     * @brief Create tensor from host data
     */
    nimcp_gpu_tensor_t* create_tensor(const float* data, const size_t* dims, uint32_t ndim) {
        if (!ctx) return nullptr;
        return track(nimcp_gpu_tensor_from_host(ctx, data, dims, ndim, NIMCP_GPU_PRECISION_FP32));
    }

    /**
     * @brief Create output tensor
     */
    nimcp_gpu_tensor_t* create_output(const size_t* dims, uint32_t ndim) {
        if (!ctx) return nullptr;
        return track(nimcp_gpu_tensor_create(ctx, dims, ndim, NIMCP_GPU_PRECISION_FP32));
    }

    /**
     * @brief Create 1D tensor with values
     */
    nimcp_gpu_tensor_t* create_1d(const std::vector<float>& values) {
        if (!ctx) return nullptr;
        size_t dims[] = {values.size()};
        return track(nimcp_gpu_tensor_from_host(ctx, values.data(), dims, 1, NIMCP_GPU_PRECISION_FP32));
    }

    /**
     * @brief Create 2D tensor with values (row-major)
     */
    nimcp_gpu_tensor_t* create_2d(size_t rows, size_t cols, const std::vector<float>& values) {
        if (!ctx) return nullptr;
        size_t dims[] = {rows, cols};
        return track(nimcp_gpu_tensor_from_host(ctx, values.data(), dims, 2, NIMCP_GPU_PRECISION_FP32));
    }

    /**
     * @brief Create constant tensor
     */
    nimcp_gpu_tensor_t* create_constant(const size_t* dims, uint32_t ndim, float value) {
        if (!ctx) return nullptr;
        size_t numel = 1;
        for (uint32_t i = 0; i < ndim; i++) numel *= dims[i];
        std::vector<float> data(numel, value);
        return track(nimcp_gpu_tensor_from_host(ctx, data.data(), dims, ndim, NIMCP_GPU_PRECISION_FP32));
    }

    /**
     * @brief Copy tensor data to host
     */
    std::vector<float> to_host(const nimcp_gpu_tensor_t* tensor) {
        if (!tensor) return {};
        std::vector<float> result(tensor->numel);
        nimcp_gpu_tensor_to_host(tensor, result.data());
        return result;
    }

    /**
     * @brief Check if GPU context is available
     */
    bool has_gpu_context() const {
        return ctx != nullptr && nimcp_gpu_context_is_valid(ctx);
    }
};

//=============================================================================
// Loss Function Tests - MSE
//=============================================================================

/**
 * TEST: MSE loss computation
 * WHAT: Verify nimcp_gpu_loss_mse() computes mean((pred - target)^2)
 * WHY:  Most common regression loss
 */
TEST_F(GPUTrainingKernelTest, MSE_PerfectPrediction_ZeroLoss) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    std::vector<float> pred_data = {1.0f, 2.0f, 3.0f, 4.0f};
    std::vector<float> target_data = {1.0f, 2.0f, 3.0f, 4.0f};  // Same as pred
    size_t dims[] = {4};

    nimcp_gpu_tensor_t* pred = create_1d(pred_data);
    nimcp_gpu_tensor_t* target = create_1d(target_data);
    nimcp_gpu_tensor_t* grad = create_output(dims, 1);

    ASSERT_NE(pred, nullptr);
    ASSERT_NE(target, nullptr);
    ASSERT_NE(grad, nullptr);

    float loss = 0.0f;
    bool result = nimcp_gpu_loss_mse(ctx, pred, target, &loss, grad);

    EXPECT_TRUE(result);
    EXPECT_NEAR(loss, 0.0f, LOSS_TOLERANCE);

    // Gradients should be zero for perfect prediction
    std::vector<float> grad_data = to_host(grad);
    for (float g : grad_data) {
        EXPECT_NEAR(g, 0.0f, GRAD_TOLERANCE);
    }
}

/**
 * TEST: MSE loss with error
 * WHAT: Verify MSE computes correct loss and gradient
 * WHY:  Validate loss formula implementation
 */
TEST_F(GPUTrainingKernelTest, MSE_WithError_ComputesCorrectly) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    std::vector<float> pred_data = {2.0f, 4.0f, 6.0f, 8.0f};
    std::vector<float> target_data = {1.0f, 2.0f, 3.0f, 4.0f};  // Error of 1, 2, 3, 4
    size_t dims[] = {4};

    nimcp_gpu_tensor_t* pred = create_1d(pred_data);
    nimcp_gpu_tensor_t* target = create_1d(target_data);
    nimcp_gpu_tensor_t* grad = create_output(dims, 1);

    float loss = 0.0f;
    bool result = nimcp_gpu_loss_mse(ctx, pred, target, &loss, grad);

    EXPECT_TRUE(result);
    // MSE = mean((1^2 + 2^2 + 3^2 + 4^2)) = mean(1+4+9+16) = 30/4 = 7.5
    EXPECT_NEAR(loss, 7.5f, LOSS_TOLERANCE);

    // Gradient = 2 * (pred - target) / n = 2 * [1,2,3,4] / 4 = [0.5, 1.0, 1.5, 2.0]
    std::vector<float> grad_data = to_host(grad);
    EXPECT_NEAR(grad_data[0], 0.5f, GRAD_TOLERANCE);
    EXPECT_NEAR(grad_data[1], 1.0f, GRAD_TOLERANCE);
    EXPECT_NEAR(grad_data[2], 1.5f, GRAD_TOLERANCE);
    EXPECT_NEAR(grad_data[3], 2.0f, GRAD_TOLERANCE);
}

//=============================================================================
// Loss Function Tests - MAE
//=============================================================================

/**
 * TEST: MAE loss computation
 * WHAT: Verify nimcp_gpu_loss_mae() computes mean(|pred - target|)
 * WHY:  Robust to outliers
 */
TEST_F(GPUTrainingKernelTest, MAE_WithError_ComputesCorrectly) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    std::vector<float> pred_data = {2.0f, 4.0f, 6.0f, 8.0f};
    std::vector<float> target_data = {1.0f, 2.0f, 3.0f, 4.0f};
    size_t dims[] = {4};

    nimcp_gpu_tensor_t* pred = create_1d(pred_data);
    nimcp_gpu_tensor_t* target = create_1d(target_data);
    nimcp_gpu_tensor_t* grad = create_output(dims, 1);

    float loss = 0.0f;
    bool result = nimcp_gpu_loss_mae(ctx, pred, target, &loss, grad);

    EXPECT_TRUE(result);
    // MAE = mean(|1| + |2| + |3| + |4|) = 10/4 = 2.5
    EXPECT_NEAR(loss, 2.5f, LOSS_TOLERANCE);

    // Gradient = sign(pred - target) / n
    std::vector<float> grad_data = to_host(grad);
    for (float g : grad_data) {
        EXPECT_NEAR(g, 0.25f, GRAD_TOLERANCE);  // All positive errors, so sign = 1
    }
}

//=============================================================================
// Loss Function Tests - Cross Entropy
//=============================================================================

/**
 * TEST: Cross-entropy loss
 * WHAT: Verify nimcp_gpu_loss_cross_entropy() computes -sum(target * log(softmax(logits)))
 * WHY:  Standard classification loss
 */
TEST_F(GPUTrainingKernelTest, CrossEntropy_OneHot_ComputesCorrectly) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    // Batch of 2 samples, 3 classes
    std::vector<float> logits_data = {2.0f, 1.0f, 0.1f, 0.1f, 2.0f, 1.0f};  // 2x3
    std::vector<float> target_data = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};  // One-hot
    size_t dims[] = {2, 3};

    nimcp_gpu_tensor_t* logits = create_2d(2, 3, logits_data);
    nimcp_gpu_tensor_t* target = create_2d(2, 3, target_data);
    nimcp_gpu_tensor_t* grad = create_output(dims, 2);

    float loss = 0.0f;
    bool result = nimcp_gpu_loss_cross_entropy(ctx, logits, target, &loss, grad, 1);  // Mean reduction

    EXPECT_TRUE(result);
    EXPECT_GT(loss, 0.0f);  // Loss should be positive

    // Gradient should have reasonable values
    std::vector<float> grad_data = to_host(grad);
    for (float g : grad_data) {
        EXPECT_FALSE(std::isnan(g));
        EXPECT_FALSE(std::isinf(g));
    }
}

//=============================================================================
// Loss Function Tests - Binary Cross Entropy
//=============================================================================

/**
 * TEST: Binary cross-entropy loss
 * WHAT: Verify nimcp_gpu_loss_bce() for binary classification
 * WHY:  Binary classification standard
 */
TEST_F(GPUTrainingKernelTest, BCE_BinaryTargets_ComputesCorrectly) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    std::vector<float> pred_data = {0.9f, 0.1f, 0.8f, 0.2f};  // Already sigmoid outputs
    std::vector<float> target_data = {1.0f, 0.0f, 1.0f, 0.0f};
    size_t dims[] = {4};

    nimcp_gpu_tensor_t* pred = create_1d(pred_data);
    nimcp_gpu_tensor_t* target = create_1d(target_data);
    nimcp_gpu_tensor_t* grad = create_output(dims, 1);

    float loss = 0.0f;
    bool result = nimcp_gpu_loss_bce(ctx, pred, target, &loss, grad);

    EXPECT_TRUE(result);
    EXPECT_GT(loss, 0.0f);  // Loss should be positive
    // Good predictions should have low loss
    EXPECT_LT(loss, 1.0f);

    std::vector<float> grad_data = to_host(grad);
    for (float g : grad_data) {
        EXPECT_FALSE(std::isnan(g));
    }
}

//=============================================================================
// Loss Function Tests - Focal Loss
//=============================================================================

/**
 * TEST: Focal loss for imbalanced data
 * WHAT: Verify nimcp_gpu_loss_focal() down-weights easy examples
 * WHY:  Handles class imbalance
 */
TEST_F(GPUTrainingKernelTest, FocalLoss_ImbalancedData_ComputesCorrectly) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    std::vector<float> pred_data = {0.99f, 0.01f, 0.95f, 0.05f};
    std::vector<float> target_data = {1.0f, 0.0f, 1.0f, 0.0f};
    size_t dims[] = {4};

    nimcp_gpu_tensor_t* pred = create_1d(pred_data);
    nimcp_gpu_tensor_t* target = create_1d(target_data);
    nimcp_gpu_tensor_t* grad = create_output(dims, 1);

    float loss = 0.0f;
    float alpha = 0.25f;  // Typical focal loss alpha
    float gamma = 2.0f;   // Focusing parameter

    bool result = nimcp_gpu_loss_focal(ctx, pred, target, &loss, grad, alpha, gamma);

    EXPECT_TRUE(result);
    EXPECT_GT(loss, 0.0f);

    std::vector<float> grad_data = to_host(grad);
    for (float g : grad_data) {
        EXPECT_FALSE(std::isnan(g));
        EXPECT_FALSE(std::isinf(g));
    }
}

//=============================================================================
// Loss Function Tests - Huber Loss
//=============================================================================

/**
 * TEST: Huber loss (smooth L1)
 * WHAT: Verify nimcp_gpu_loss_huber() combines L2 and L1
 * WHY:  Robust regression loss
 */
TEST_F(GPUTrainingKernelTest, HuberLoss_MixedErrors_ComputesCorrectly) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    std::vector<float> pred_data = {1.0f, 5.0f, 10.0f, 20.0f};   // Varying errors
    std::vector<float> target_data = {1.0f, 3.0f, 5.0f, 10.0f};
    size_t dims[] = {4};

    nimcp_gpu_tensor_t* pred = create_1d(pred_data);
    nimcp_gpu_tensor_t* target = create_1d(target_data);
    nimcp_gpu_tensor_t* grad = create_output(dims, 1);

    float loss = 0.0f;
    float delta = 1.0f;  // Transition point

    bool result = nimcp_gpu_loss_huber(ctx, pred, target, &loss, grad, delta);

    EXPECT_TRUE(result);
    EXPECT_GT(loss, 0.0f);

    std::vector<float> grad_data = to_host(grad);
    // Zero error should have zero gradient
    EXPECT_NEAR(grad_data[0], 0.0f, GRAD_TOLERANCE);
}

//=============================================================================
// Gradient Operation Tests
//=============================================================================

/**
 * TEST: Gradient accumulation
 * WHAT: Verify nimcp_gpu_gradient_accumulate() adds gradients
 * WHY:  Gradient accumulation for mini-batches
 */
TEST_F(GPUTrainingKernelTest, GradientAccumulate_AddsGradients) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    std::vector<float> grad_data = {1.0f, 2.0f, 3.0f, 4.0f};
    std::vector<float> accum_data = {0.5f, 0.5f, 0.5f, 0.5f};
    size_t dims[] = {4};

    nimcp_gpu_tensor_t* grad = create_1d(grad_data);
    nimcp_gpu_tensor_t* accum = create_1d(accum_data);

    bool result = nimcp_gpu_gradient_accumulate(ctx, grad, accum);
    EXPECT_TRUE(result);

    std::vector<float> result_data = to_host(accum);
    EXPECT_NEAR(result_data[0], 1.5f, FLOAT_TOLERANCE);
    EXPECT_NEAR(result_data[1], 2.5f, FLOAT_TOLERANCE);
    EXPECT_NEAR(result_data[2], 3.5f, FLOAT_TOLERANCE);
    EXPECT_NEAR(result_data[3], 4.5f, FLOAT_TOLERANCE);
}

/**
 * TEST: Gradient scaling
 * WHAT: Verify nimcp_gpu_gradient_scale() multiplies by factor
 * WHY:  Gradient averaging and loss scaling
 */
TEST_F(GPUTrainingKernelTest, GradientScale_MultipliesCorrectly) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    std::vector<float> grad_data = {2.0f, 4.0f, 6.0f, 8.0f};
    size_t dims[] = {4};

    nimcp_gpu_tensor_t* grad = create_1d(grad_data);

    bool result = nimcp_gpu_gradient_scale(ctx, grad, 0.5f);
    EXPECT_TRUE(result);

    std::vector<float> result_data = to_host(grad);
    EXPECT_NEAR(result_data[0], 1.0f, FLOAT_TOLERANCE);
    EXPECT_NEAR(result_data[1], 2.0f, FLOAT_TOLERANCE);
    EXPECT_NEAR(result_data[2], 3.0f, FLOAT_TOLERANCE);
    EXPECT_NEAR(result_data[3], 4.0f, FLOAT_TOLERANCE);
}

/**
 * TEST: Gradient clipping by norm
 * WHAT: Verify nimcp_gpu_gradient_clip_norm() clips global norm
 * WHY:  Prevent exploding gradients
 */
TEST_F(GPUTrainingKernelTest, GradientClipNorm_LargeGradients_ClipsCorrectly) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    // Create gradient with L2 norm = 5 (3-4-5 triangle scaled)
    std::vector<float> grad1_data = {3.0f, 4.0f};
    size_t dims[] = {2};

    nimcp_gpu_tensor_t* grad1 = create_1d(grad1_data);
    nimcp_gpu_tensor_t* grads[] = {grad1};

    float max_norm = 1.0f;
    float total_norm = 0.0f;

    bool result = nimcp_gpu_gradient_clip_norm(ctx, grads, 1, max_norm, &total_norm);
    EXPECT_TRUE(result);
    EXPECT_NEAR(total_norm, 5.0f, FLOAT_TOLERANCE);

    // After clipping, L2 norm should be 1.0
    std::vector<float> result_data = to_host(grad1);
    float new_norm = std::sqrt(result_data[0] * result_data[0] + result_data[1] * result_data[1]);
    EXPECT_NEAR(new_norm, max_norm, 0.01f);
}

/**
 * TEST: Gradient clipping by value
 * WHAT: Verify nimcp_gpu_gradient_clip_value() clips element-wise
 * WHY:  Alternative gradient clipping method
 */
TEST_F(GPUTrainingKernelTest, GradientClipValue_LargeValues_ClipsCorrectly) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    std::vector<float> grad_data = {-5.0f, -1.0f, 0.0f, 1.0f, 5.0f};
    size_t dims[] = {5};

    nimcp_gpu_tensor_t* grad = create_1d(grad_data);

    bool result = nimcp_gpu_gradient_clip_value(ctx, grad, 2.0f);
    EXPECT_TRUE(result);

    std::vector<float> result_data = to_host(grad);
    EXPECT_NEAR(result_data[0], -2.0f, FLOAT_TOLERANCE);  // Clipped
    EXPECT_NEAR(result_data[1], -1.0f, FLOAT_TOLERANCE);  // Unchanged
    EXPECT_NEAR(result_data[2], 0.0f, FLOAT_TOLERANCE);   // Unchanged
    EXPECT_NEAR(result_data[3], 1.0f, FLOAT_TOLERANCE);   // Unchanged
    EXPECT_NEAR(result_data[4], 2.0f, FLOAT_TOLERANCE);   // Clipped
}

/**
 * TEST: Zero gradients
 * WHAT: Verify nimcp_gpu_gradient_zero() sets all to zero
 * WHY:  Reset gradients between batches
 */
TEST_F(GPUTrainingKernelTest, GradientZero_NonZeroGradients_SetsToZero) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    std::vector<float> grad_data = {1.0f, 2.0f, 3.0f, 4.0f};
    size_t dims[] = {4};

    nimcp_gpu_tensor_t* grad = create_1d(grad_data);

    bool result = nimcp_gpu_gradient_zero(ctx, grad);
    EXPECT_TRUE(result);

    std::vector<float> result_data = to_host(grad);
    for (float g : result_data) {
        EXPECT_NEAR(g, 0.0f, FLOAT_TOLERANCE);
    }
}

//=============================================================================
// Optimizer Tests - SGD
//=============================================================================

/**
 * TEST: SGD optimizer step
 * WHAT: Verify nimcp_gpu_optim_sgd() updates parameters
 * WHY:  Simplest optimizer
 */
TEST_F(GPUTrainingKernelTest, SGD_BasicStep_UpdatesCorrectly) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    std::vector<float> param_data = {1.0f, 2.0f, 3.0f, 4.0f};
    std::vector<float> grad_data = {0.1f, 0.2f, 0.3f, 0.4f};
    size_t dims[] = {4};

    nimcp_gpu_tensor_t* param = create_1d(param_data);
    nimcp_gpu_tensor_t* grad = create_1d(grad_data);

    // Create optimizer state with lr = 1.0 for easy verification
    nimcp_optim_state_t* state = track_optim(nimcp_optim_state_create(ctx, NIMCP_OPTIM_SGD, param, 1.0f));
    ASSERT_NE(state, nullptr);

    bool result = nimcp_gpu_optim_sgd(ctx, param, grad, state);
    EXPECT_TRUE(result);

    // param = param - lr * grad = [1-0.1, 2-0.2, 3-0.3, 4-0.4] = [0.9, 1.8, 2.7, 3.6]
    std::vector<float> result_data = to_host(param);
    EXPECT_NEAR(result_data[0], 0.9f, FLOAT_TOLERANCE);
    EXPECT_NEAR(result_data[1], 1.8f, FLOAT_TOLERANCE);
    EXPECT_NEAR(result_data[2], 2.7f, FLOAT_TOLERANCE);
    EXPECT_NEAR(result_data[3], 3.6f, FLOAT_TOLERANCE);
}

/**
 * TEST: SGD with momentum
 * WHAT: Verify SGD momentum accelerates convergence
 * WHY:  Momentum is commonly used
 */
TEST_F(GPUTrainingKernelTest, SGD_WithMomentum_AccumulatesVelocity) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    std::vector<float> param_data = {1.0f, 1.0f};
    std::vector<float> grad_data = {0.1f, 0.1f};
    size_t dims[] = {2};

    nimcp_gpu_tensor_t* param = create_1d(param_data);
    nimcp_gpu_tensor_t* grad = create_1d(grad_data);

    nimcp_optim_state_t* state = track_optim(nimcp_optim_state_create(ctx, NIMCP_OPTIM_SGD_MOMENTUM, param, 0.1f));
    ASSERT_NE(state, nullptr);
    state->momentum = 0.9f;

    // First step
    bool result1 = nimcp_gpu_optim_sgd(ctx, param, grad, state);
    EXPECT_TRUE(result1);

    // Second step should have accumulated momentum
    bool result2 = nimcp_gpu_optim_sgd(ctx, param, grad, state);
    EXPECT_TRUE(result2);

    std::vector<float> result_data = to_host(param);
    // Parameters should have decreased more than just lr * grad * 2
    EXPECT_LT(result_data[0], 1.0f - 0.02f);
}

//=============================================================================
// Optimizer Tests - Adam
//=============================================================================

/**
 * TEST: Adam optimizer step
 * WHAT: Verify nimcp_gpu_optim_adam() updates with adaptive learning rate
 * WHY:  Most popular optimizer
 */
TEST_F(GPUTrainingKernelTest, Adam_BasicStep_UpdatesCorrectly) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    std::vector<float> param_data = {1.0f, 2.0f, 3.0f, 4.0f};
    std::vector<float> grad_data = {0.1f, 0.2f, 0.3f, 0.4f};
    size_t dims[] = {4};

    nimcp_gpu_tensor_t* param = create_1d(param_data);
    nimcp_gpu_tensor_t* grad = create_1d(grad_data);

    nimcp_optim_state_t* state = track_optim(nimcp_optim_state_create(ctx, NIMCP_OPTIM_ADAM, param, 0.001f));
    ASSERT_NE(state, nullptr);

    bool result = nimcp_gpu_optim_adam(ctx, param, grad, state);
    EXPECT_TRUE(result);

    // Verify timestep incremented
    EXPECT_EQ(state->t, 1u);

    std::vector<float> result_data = to_host(param);
    // Parameters should have decreased
    for (size_t i = 0; i < 4; i++) {
        EXPECT_LT(result_data[i], param_data[i]);
    }
}

/**
 * TEST: Adam multiple steps
 * WHAT: Verify Adam converges over multiple steps
 * WHY:  Validate moment accumulation
 */
TEST_F(GPUTrainingKernelTest, Adam_MultipleSteps_Converges) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    std::vector<float> param_data = {1.0f, 1.0f};
    std::vector<float> grad_data = {0.1f, 0.1f};
    size_t dims[] = {2};

    nimcp_gpu_tensor_t* param = create_1d(param_data);
    nimcp_gpu_tensor_t* grad = create_1d(grad_data);

    nimcp_optim_state_t* state = track_optim(nimcp_optim_state_create(ctx, NIMCP_OPTIM_ADAM, param, 0.01f));
    ASSERT_NE(state, nullptr);

    // Run 100 steps
    for (int i = 0; i < 100; i++) {
        bool result = nimcp_gpu_optim_adam(ctx, param, grad, state);
        EXPECT_TRUE(result);
    }

    EXPECT_EQ(state->t, 100u);

    std::vector<float> result_data = to_host(param);
    // Parameters should have converged toward gradient direction
    EXPECT_LT(result_data[0], 0.5f);
}

//=============================================================================
// Optimizer Tests - AdamW
//=============================================================================

/**
 * TEST: AdamW optimizer with weight decay
 * WHAT: Verify nimcp_gpu_optim_adamw() applies decoupled weight decay
 * WHY:  Better regularization than L2
 */
TEST_F(GPUTrainingKernelTest, AdamW_WithWeightDecay_DecreasesWeights) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    std::vector<float> param_data = {1.0f, 1.0f};
    std::vector<float> grad_data = {0.0f, 0.0f};  // Zero gradient
    size_t dims[] = {2};

    nimcp_gpu_tensor_t* param = create_1d(param_data);
    nimcp_gpu_tensor_t* grad = create_1d(grad_data);

    nimcp_optim_state_t* state = track_optim(nimcp_optim_state_create(ctx, NIMCP_OPTIM_ADAMW, param, 0.01f));
    ASSERT_NE(state, nullptr);
    state->weight_decay = 0.01f;

    bool result = nimcp_gpu_optim_adamw(ctx, param, grad, state);
    EXPECT_TRUE(result);

    std::vector<float> result_data = to_host(param);
    // Even with zero gradient, weight decay should decrease parameters
    EXPECT_LT(result_data[0], 1.0f);
    EXPECT_LT(result_data[1], 1.0f);
}

//=============================================================================
// Optimizer Tests - RMSprop
//=============================================================================

/**
 * TEST: RMSprop optimizer
 * WHAT: Verify nimcp_gpu_optim_rmsprop() adapts learning rate
 * WHY:  Good for non-stationary objectives
 */
TEST_F(GPUTrainingKernelTest, RMSprop_BasicStep_UpdatesCorrectly) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    std::vector<float> param_data = {1.0f, 2.0f};
    std::vector<float> grad_data = {0.5f, 1.0f};
    size_t dims[] = {2};

    nimcp_gpu_tensor_t* param = create_1d(param_data);
    nimcp_gpu_tensor_t* grad = create_1d(grad_data);

    nimcp_optim_state_t* state = track_optim(nimcp_optim_state_create(ctx, NIMCP_OPTIM_RMSPROP, param, 0.01f));
    ASSERT_NE(state, nullptr);

    bool result = nimcp_gpu_optim_rmsprop(ctx, param, grad, state);
    EXPECT_TRUE(result);

    std::vector<float> result_data = to_host(param);
    EXPECT_LT(result_data[0], 1.0f);
    EXPECT_LT(result_data[1], 2.0f);
}

//=============================================================================
// Optimizer Tests - AdaGrad
//=============================================================================

/**
 * TEST: AdaGrad optimizer
 * WHAT: Verify nimcp_gpu_optim_adagrad() accumulates squared gradients
 * WHY:  Good for sparse features
 */
TEST_F(GPUTrainingKernelTest, AdaGrad_BasicStep_UpdatesCorrectly) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    std::vector<float> param_data = {1.0f, 2.0f};
    std::vector<float> grad_data = {1.0f, 1.0f};
    size_t dims[] = {2};

    nimcp_gpu_tensor_t* param = create_1d(param_data);
    nimcp_gpu_tensor_t* grad = create_1d(grad_data);

    nimcp_optim_state_t* state = track_optim(nimcp_optim_state_create(ctx, NIMCP_OPTIM_ADAGRAD, param, 0.1f));
    ASSERT_NE(state, nullptr);

    bool result = nimcp_gpu_optim_adagrad(ctx, param, grad, state);
    EXPECT_TRUE(result);

    std::vector<float> result_data = to_host(param);
    EXPECT_LT(result_data[0], 1.0f);
    EXPECT_LT(result_data[1], 2.0f);
}

//=============================================================================
// Backward Pass Tests - Linear Layer
//=============================================================================

/**
 * TEST: Linear layer backward pass
 * WHAT: Verify nimcp_gpu_backward_linear() computes dx, dW, db
 * WHY:  Core layer gradient computation
 */
TEST_F(GPUTrainingKernelTest, BackwardLinear_ComputesGradients) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    // Input: batch=2, in_features=3
    // Weight: out_features=2, in_features=3
    // Output: batch=2, out_features=2
    std::vector<float> x_data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};  // 2x3
    std::vector<float> w_data = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f};  // 2x3
    std::vector<float> grad_out_data = {1.0f, 1.0f, 1.0f, 1.0f};       // 2x2
    size_t x_dims[] = {2, 3};
    size_t w_dims[] = {2, 3};
    size_t out_dims[] = {2, 2};
    size_t b_dims[] = {2};

    nimcp_gpu_tensor_t* x = create_2d(2, 3, x_data);
    nimcp_gpu_tensor_t* weight = create_2d(2, 3, w_data);
    nimcp_gpu_tensor_t* grad_output = create_2d(2, 2, grad_out_data);
    nimcp_gpu_tensor_t* grad_input = create_output(x_dims, 2);
    nimcp_gpu_tensor_t* grad_weight = create_output(w_dims, 2);
    nimcp_gpu_tensor_t* grad_bias = create_output(b_dims, 1);

    bool result = nimcp_gpu_backward_linear(ctx, x, weight, grad_output,
                                            grad_input, grad_weight, grad_bias);
    EXPECT_TRUE(result);

    // Verify gradients are computed (non-zero)
    std::vector<float> dx = to_host(grad_input);
    std::vector<float> dw = to_host(grad_weight);
    std::vector<float> db = to_host(grad_bias);

    // At least some gradients should be non-zero
    bool has_nonzero = false;
    for (float g : dx) if (std::abs(g) > 1e-6f) has_nonzero = true;
    for (float g : dw) if (std::abs(g) > 1e-6f) has_nonzero = true;
    for (float g : db) if (std::abs(g) > 1e-6f) has_nonzero = true;
    EXPECT_TRUE(has_nonzero);
}

//=============================================================================
// Backward Pass Tests - Activation Functions
//=============================================================================

/**
 * TEST: ReLU backward pass
 * WHAT: Verify nimcp_gpu_backward_relu() computes dx = dy * (x > 0)
 * WHY:  ReLU gradient is indicator function
 */
TEST_F(GPUTrainingKernelTest, BackwardReLU_ZeroesNegativeGradients) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    std::vector<float> x_data = {-2.0f, -1.0f, 0.0f, 1.0f, 2.0f};
    std::vector<float> grad_out_data = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    size_t dims[] = {5};

    nimcp_gpu_tensor_t* x = create_1d(x_data);
    nimcp_gpu_tensor_t* grad_output = create_1d(grad_out_data);
    nimcp_gpu_tensor_t* grad_input = create_output(dims, 1);

    bool result = nimcp_gpu_backward_relu(ctx, x, grad_output, grad_input);
    EXPECT_TRUE(result);

    std::vector<float> dx = to_host(grad_input);
    EXPECT_NEAR(dx[0], 0.0f, GRAD_TOLERANCE);  // x < 0, gradient = 0
    EXPECT_NEAR(dx[1], 0.0f, GRAD_TOLERANCE);  // x < 0, gradient = 0
    EXPECT_NEAR(dx[2], 0.0f, GRAD_TOLERANCE);  // x = 0, gradient = 0
    EXPECT_NEAR(dx[3], 1.0f, GRAD_TOLERANCE);  // x > 0, gradient = 1
    EXPECT_NEAR(dx[4], 1.0f, GRAD_TOLERANCE);  // x > 0, gradient = 1
}

/**
 * TEST: Sigmoid backward pass
 * WHAT: Verify nimcp_gpu_backward_sigmoid() computes dx = dy * s * (1-s)
 * WHY:  Sigmoid derivative formula
 */
TEST_F(GPUTrainingKernelTest, BackwardSigmoid_ComputesDerivative) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    // sigmoid(0) = 0.5, derivative = 0.5 * 0.5 = 0.25
    std::vector<float> output_data = {0.5f, 0.5f};  // sigmoid output
    std::vector<float> grad_out_data = {1.0f, 2.0f};
    size_t dims[] = {2};

    nimcp_gpu_tensor_t* output = create_1d(output_data);
    nimcp_gpu_tensor_t* grad_output = create_1d(grad_out_data);
    nimcp_gpu_tensor_t* grad_input = create_output(dims, 1);

    bool result = nimcp_gpu_backward_sigmoid(ctx, output, grad_output, grad_input);
    EXPECT_TRUE(result);

    std::vector<float> dx = to_host(grad_input);
    // dx = dy * s * (1-s) = 1 * 0.5 * 0.5 = 0.25
    EXPECT_NEAR(dx[0], 0.25f, GRAD_TOLERANCE);
    EXPECT_NEAR(dx[1], 0.5f, GRAD_TOLERANCE);  // 2 * 0.25 = 0.5
}

/**
 * TEST: Tanh backward pass
 * WHAT: Verify nimcp_gpu_backward_tanh() computes dx = dy * (1 - tanh^2)
 * WHY:  Tanh derivative formula
 */
TEST_F(GPUTrainingKernelTest, BackwardTanh_ComputesDerivative) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    // tanh(0) = 0, derivative = 1 - 0^2 = 1
    std::vector<float> output_data = {0.0f, 0.5f};  // tanh output
    std::vector<float> grad_out_data = {1.0f, 1.0f};
    size_t dims[] = {2};

    nimcp_gpu_tensor_t* output = create_1d(output_data);
    nimcp_gpu_tensor_t* grad_output = create_1d(grad_out_data);
    nimcp_gpu_tensor_t* grad_input = create_output(dims, 1);

    bool result = nimcp_gpu_backward_tanh(ctx, output, grad_output, grad_input);
    EXPECT_TRUE(result);

    std::vector<float> dx = to_host(grad_input);
    // dx = dy * (1 - tanh^2) = 1 * (1 - 0) = 1
    EXPECT_NEAR(dx[0], 1.0f, GRAD_TOLERANCE);
    // 1 - 0.5^2 = 0.75
    EXPECT_NEAR(dx[1], 0.75f, GRAD_TOLERANCE);
}

/**
 * TEST: GELU backward pass
 * WHAT: Verify nimcp_gpu_backward_gelu() computes correct gradient
 * WHY:  GELU used in transformers
 */
TEST_F(GPUTrainingKernelTest, BackwardGELU_ComputesGradient) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    std::vector<float> x_data = {-1.0f, 0.0f, 1.0f};
    std::vector<float> grad_out_data = {1.0f, 1.0f, 1.0f};
    size_t dims[] = {3};

    nimcp_gpu_tensor_t* x = create_1d(x_data);
    nimcp_gpu_tensor_t* grad_output = create_1d(grad_out_data);
    nimcp_gpu_tensor_t* grad_input = create_output(dims, 1);

    bool result = nimcp_gpu_backward_gelu(ctx, x, grad_output, grad_input);
    EXPECT_TRUE(result);

    std::vector<float> dx = to_host(grad_input);
    // GELU'(0) = 0.5
    EXPECT_NEAR(dx[1], 0.5f, 0.1f);
    // GELU'(x) > 0 for x > 0
    EXPECT_GT(dx[2], 0.0f);
}

/**
 * TEST: Softmax backward pass
 * WHAT: Verify nimcp_gpu_backward_softmax() computes Jacobian-vector product
 * WHY:  Softmax gradient is complex
 */
TEST_F(GPUTrainingKernelTest, BackwardSoftmax_ComputesGradient) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    std::vector<float> output_data = {0.5f, 0.3f, 0.2f};  // Softmax output (sums to 1)
    std::vector<float> grad_out_data = {1.0f, 0.0f, 0.0f};  // One-hot gradient
    size_t dims[] = {3};

    nimcp_gpu_tensor_t* output = create_1d(output_data);
    nimcp_gpu_tensor_t* grad_output = create_1d(grad_out_data);
    nimcp_gpu_tensor_t* grad_input = create_output(dims, 1);

    bool result = nimcp_gpu_backward_softmax(ctx, output, grad_output, grad_input);
    EXPECT_TRUE(result);

    std::vector<float> dx = to_host(grad_input);
    // dx_i = s_i * (dy_i - sum_j(dy_j * s_j))
    // sum_j = 1 * 0.5 = 0.5
    // dx[0] = 0.5 * (1 - 0.5) = 0.25
    // dx[1] = 0.3 * (0 - 0.5) = -0.15
    // dx[2] = 0.2 * (0 - 0.5) = -0.10
    EXPECT_NEAR(dx[0], 0.25f, GRAD_TOLERANCE);
    EXPECT_NEAR(dx[1], -0.15f, GRAD_TOLERANCE);
    EXPECT_NEAR(dx[2], -0.10f, GRAD_TOLERANCE);
}

//=============================================================================
// Backward Pass Tests - Normalization
//=============================================================================

/**
 * TEST: Batch normalization backward
 * WHAT: Verify nimcp_gpu_backward_batchnorm() computes gradients
 * WHY:  BatchNorm is ubiquitous in CNNs
 */
TEST_F(GPUTrainingKernelTest, BackwardBatchNorm_ComputesGradients) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    // Batch of 2, 4 features
    std::vector<float> x_data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};  // 2x4
    std::vector<float> gamma_data = {1.0f, 1.0f, 1.0f, 1.0f};  // Scale
    std::vector<float> mean_data = {3.0f, 4.0f, 5.0f, 6.0f};   // Running mean
    std::vector<float> var_data = {1.0f, 1.0f, 1.0f, 1.0f};    // Running var
    std::vector<float> grad_out_data = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};

    size_t x_dims[] = {2, 4};
    size_t param_dims[] = {4};

    nimcp_gpu_tensor_t* x = create_2d(2, 4, x_data);
    nimcp_gpu_tensor_t* gamma = create_1d(gamma_data);
    nimcp_gpu_tensor_t* mean = create_1d(mean_data);
    nimcp_gpu_tensor_t* var = create_1d(var_data);
    nimcp_gpu_tensor_t* grad_output = create_2d(2, 4, grad_out_data);
    nimcp_gpu_tensor_t* grad_input = create_output(x_dims, 2);
    nimcp_gpu_tensor_t* grad_gamma = create_output(param_dims, 1);
    nimcp_gpu_tensor_t* grad_beta = create_output(param_dims, 1);

    bool result = nimcp_gpu_backward_batchnorm(ctx, x, gamma, mean, var,
                                                grad_output, grad_input,
                                                grad_gamma, grad_beta, 1e-5f);
    EXPECT_TRUE(result);

    // Verify gradients are computed
    std::vector<float> dx = to_host(grad_input);
    std::vector<float> dgamma = to_host(grad_gamma);
    std::vector<float> dbeta = to_host(grad_beta);

    for (float g : dx) EXPECT_FALSE(std::isnan(g));
    for (float g : dgamma) EXPECT_FALSE(std::isnan(g));
    for (float g : dbeta) EXPECT_FALSE(std::isnan(g));
}

/**
 * TEST: Layer normalization backward
 * WHAT: Verify nimcp_gpu_backward_layernorm() computes gradients
 * WHY:  LayerNorm is common in transformers
 */
TEST_F(GPUTrainingKernelTest, BackwardLayerNorm_ComputesGradients) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    std::vector<float> x_data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};  // 2x4
    std::vector<float> gamma_data = {1.0f, 1.0f, 1.0f, 1.0f};
    std::vector<float> mean_data = {2.5f, 6.5f};  // Per-sample mean
    std::vector<float> var_data = {1.25f, 1.25f}; // Per-sample var
    std::vector<float> grad_out_data = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};

    size_t x_dims[] = {2, 4};
    size_t param_dims[] = {4};
    size_t stats_dims[] = {2};

    nimcp_gpu_tensor_t* x = create_2d(2, 4, x_data);
    nimcp_gpu_tensor_t* gamma = create_1d(gamma_data);
    nimcp_gpu_tensor_t* mean = create_1d(mean_data);
    nimcp_gpu_tensor_t* var = create_1d(var_data);
    nimcp_gpu_tensor_t* grad_output = create_2d(2, 4, grad_out_data);
    nimcp_gpu_tensor_t* grad_input = create_output(x_dims, 2);
    nimcp_gpu_tensor_t* grad_gamma = create_output(param_dims, 1);
    nimcp_gpu_tensor_t* grad_beta = create_output(param_dims, 1);

    bool result = nimcp_gpu_backward_layernorm(ctx, x, gamma, mean, var,
                                                grad_output, grad_input,
                                                grad_gamma, grad_beta, 1e-5f);
    EXPECT_TRUE(result);

    std::vector<float> dx = to_host(grad_input);
    for (float g : dx) EXPECT_FALSE(std::isnan(g));
}

//=============================================================================
// Backward Pass Tests - Dropout
//=============================================================================

/**
 * TEST: Dropout backward pass
 * WHAT: Verify nimcp_gpu_backward_dropout() applies mask and scaling
 * WHY:  Dropout is important for regularization
 */
TEST_F(GPUTrainingKernelTest, BackwardDropout_AppliesMaskAndScale) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    std::vector<float> mask_data = {1.0f, 0.0f, 1.0f, 0.0f};  // Binary mask
    std::vector<float> grad_out_data = {1.0f, 1.0f, 1.0f, 1.0f};
    size_t dims[] = {4};
    float dropout_p = 0.5f;

    nimcp_gpu_tensor_t* mask = create_1d(mask_data);
    nimcp_gpu_tensor_t* grad_output = create_1d(grad_out_data);
    nimcp_gpu_tensor_t* grad_input = create_output(dims, 1);

    bool result = nimcp_gpu_backward_dropout(ctx, mask, grad_output, grad_input, dropout_p);
    EXPECT_TRUE(result);

    std::vector<float> dx = to_host(grad_input);
    // dx = dy * mask / (1 - p) = 1 * 1 / 0.5 = 2 (where mask = 1)
    EXPECT_NEAR(dx[0], 2.0f, GRAD_TOLERANCE);  // mask = 1
    EXPECT_NEAR(dx[1], 0.0f, GRAD_TOLERANCE);  // mask = 0
    EXPECT_NEAR(dx[2], 2.0f, GRAD_TOLERANCE);  // mask = 1
    EXPECT_NEAR(dx[3], 0.0f, GRAD_TOLERANCE);  // mask = 0
}

//=============================================================================
// Learning Rate Scheduler Tests
//=============================================================================

/**
 * TEST: Step learning rate scheduler
 * WHAT: Verify nimcp_lr_step() decays LR at intervals
 * WHY:  Common scheduler for training
 */
TEST_F(GPUTrainingKernelTest, LRStep_DecaysCorrectly) {
    float initial_lr = 0.1f;
    uint64_t step_size = 10;
    float gamma = 0.1f;

    // At step 0, LR should be initial
    float lr0 = nimcp_lr_step(initial_lr, 0, step_size, gamma);
    EXPECT_NEAR(lr0, 0.1f, FLOAT_TOLERANCE);

    // At step 10, LR should be 0.1 * 0.1 = 0.01
    float lr10 = nimcp_lr_step(initial_lr, 10, step_size, gamma);
    EXPECT_NEAR(lr10, 0.01f, FLOAT_TOLERANCE);

    // At step 20, LR should be 0.01 * 0.1 = 0.001
    float lr20 = nimcp_lr_step(initial_lr, 20, step_size, gamma);
    EXPECT_NEAR(lr20, 0.001f, FLOAT_TOLERANCE);
}

/**
 * TEST: Cosine annealing scheduler
 * WHAT: Verify nimcp_lr_cosine() follows cosine curve
 * WHY:  Smooth learning rate decay
 */
TEST_F(GPUTrainingKernelTest, LRCosine_FollowsCurve) {
    float max_lr = 0.1f;
    float min_lr = 0.001f;
    uint64_t total_steps = 100;

    // At step 0, LR should be max
    float lr0 = nimcp_lr_cosine(max_lr, min_lr, 0, total_steps);
    EXPECT_NEAR(lr0, max_lr, FLOAT_TOLERANCE);

    // At step total/2, LR should be midpoint
    float lr50 = nimcp_lr_cosine(max_lr, min_lr, 50, total_steps);
    float expected_mid = min_lr + 0.5f * (max_lr - min_lr);
    EXPECT_NEAR(lr50, expected_mid, 0.01f);

    // At step total, LR should be min
    float lr100 = nimcp_lr_cosine(max_lr, min_lr, 100, total_steps);
    EXPECT_NEAR(lr100, min_lr, FLOAT_TOLERANCE);
}

/**
 * TEST: Linear warmup scheduler
 * WHAT: Verify nimcp_lr_warmup_linear() warms up then decays
 * WHY:  Prevents training instability at start
 */
TEST_F(GPUTrainingKernelTest, LRWarmupLinear_WarmsUpThenDecays) {
    float max_lr = 0.1f;
    uint64_t warmup_steps = 10;
    uint64_t total_steps = 100;

    // At step 0, LR should be near 0
    float lr0 = nimcp_lr_warmup_linear(max_lr, 0, warmup_steps, total_steps);
    EXPECT_NEAR(lr0, 0.0f, 0.01f);

    // At step warmup/2, LR should be max/2
    float lr5 = nimcp_lr_warmup_linear(max_lr, 5, warmup_steps, total_steps);
    EXPECT_NEAR(lr5, 0.05f, 0.01f);

    // At step warmup, LR should be max
    float lr10 = nimcp_lr_warmup_linear(max_lr, warmup_steps, warmup_steps, total_steps);
    EXPECT_NEAR(lr10, max_lr, 0.01f);

    // After warmup, LR should decay linearly
    float lr100 = nimcp_lr_warmup_linear(max_lr, total_steps, warmup_steps, total_steps);
    EXPECT_NEAR(lr100, 0.0f, 0.01f);
}

/**
 * TEST: Exponential decay scheduler
 * WHAT: Verify nimcp_lr_exponential() decays exponentially
 * WHY:  Smooth exponential decay
 */
TEST_F(GPUTrainingKernelTest, LRExponential_DecaysCorrectly) {
    float initial_lr = 0.1f;
    float decay_rate = 0.01f;

    // At step 0
    float lr0 = nimcp_lr_exponential(initial_lr, 0, decay_rate);
    EXPECT_NEAR(lr0, initial_lr, FLOAT_TOLERANCE);

    // At step 100, LR = 0.1 * exp(-0.01 * 100) = 0.1 * exp(-1) ~= 0.0368
    float lr100 = nimcp_lr_exponential(initial_lr, 100, decay_rate);
    EXPECT_NEAR(lr100, initial_lr * std::exp(-1.0f), 0.01f);
}

//=============================================================================
// Optimizer State Lifecycle Tests
//=============================================================================

/**
 * TEST: Optimizer state creation
 * WHAT: Verify nimcp_optim_state_create() initializes correctly
 * WHY:  Optimizer state required for momentum/adaptive LR
 */
TEST_F(GPUTrainingKernelTest, OptimStateCreate_InitializesCorrectly) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    std::vector<float> param_data = {1.0f, 2.0f, 3.0f, 4.0f};
    size_t dims[] = {4};
    nimcp_gpu_tensor_t* param = create_1d(param_data);

    nimcp_optim_state_t* state = track_optim(nimcp_optim_state_create(ctx, NIMCP_OPTIM_ADAM, param, 0.001f));

    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->type, NIMCP_OPTIM_ADAM);
    EXPECT_FLOAT_EQ(state->lr, 0.001f);
    EXPECT_EQ(state->t, 0u);
    EXPECT_FLOAT_EQ(state->beta1, 0.9f);
    EXPECT_FLOAT_EQ(state->beta2, 0.999f);
    EXPECT_NE(state->m, nullptr);  // First moment tensor
    EXPECT_NE(state->v, nullptr);  // Second moment tensor
}

/**
 * TEST: Optimizer state destroy
 * WHAT: Verify nimcp_optim_state_destroy() handles NULL
 * WHY:  Prevent crashes from double-free or NULL
 */
TEST_F(GPUTrainingKernelTest, OptimStateDestroy_Null_DoesNotCrash) {
    nimcp_optim_state_destroy(nullptr);
    SUCCEED();
}

//=============================================================================
// Error Handling Tests
//=============================================================================

/**
 * TEST: NULL context handling
 * WHAT: Verify loss functions fail gracefully with NULL context
 * WHY:  Prevent crashes from invalid input
 */
TEST_F(GPUTrainingKernelTest, NullContext_Loss_ReturnsFalse) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    std::vector<float> data = {1.0f, 2.0f};
    size_t dims[] = {2};

    nimcp_gpu_tensor_t* pred = create_1d(data);
    nimcp_gpu_tensor_t* target = create_1d(data);
    nimcp_gpu_tensor_t* grad = create_output(dims, 1);

    float loss = 0.0f;
    bool result = nimcp_gpu_loss_mse(nullptr, pred, target, &loss, grad);
    EXPECT_FALSE(result);
}

/**
 * TEST: NULL tensor handling
 * WHAT: Verify operations fail gracefully with NULL tensors
 * WHY:  Prevent crashes from invalid input
 */
TEST_F(GPUTrainingKernelTest, NullTensor_Optimizer_ReturnsFalse) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    std::vector<float> data = {1.0f, 2.0f};
    nimcp_gpu_tensor_t* param = create_1d(data);
    nimcp_optim_state_t* state = track_optim(nimcp_optim_state_create(ctx, NIMCP_OPTIM_SGD, param, 0.01f));

    bool result = nimcp_gpu_optim_sgd(ctx, param, nullptr, state);
    EXPECT_FALSE(result);
}

/**
 * TEST: NULL optimizer state handling
 * WHAT: Verify optimizer fails gracefully with NULL state
 * WHY:  Prevent crashes from invalid input
 */
TEST_F(GPUTrainingKernelTest, NullState_Optimizer_ReturnsFalse) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    std::vector<float> data = {1.0f, 2.0f};
    nimcp_gpu_tensor_t* param = create_1d(data);
    nimcp_gpu_tensor_t* grad = create_1d(data);

    bool result = nimcp_gpu_optim_adam(ctx, param, grad, nullptr);
    EXPECT_FALSE(result);
}

//=============================================================================
// Integration Tests
//=============================================================================

/**
 * TEST: Full training step
 * WHAT: Verify complete forward-backward-update cycle
 * WHY:  End-to-end training validation
 */
TEST_F(GPUTrainingKernelTest, Integration_FullTrainingStep_Succeeds) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    // Simple linear regression: y = 2*x, trying to learn weight = 2
    std::vector<float> x_data = {1.0f, 2.0f, 3.0f, 4.0f};
    std::vector<float> y_true_data = {2.0f, 4.0f, 6.0f, 8.0f};  // True targets
    std::vector<float> weight_data = {1.0f};  // Initial weight (should converge to 2)
    size_t batch_dims[] = {4};
    size_t weight_dims[] = {1};

    nimcp_gpu_tensor_t* x = create_1d(x_data);
    nimcp_gpu_tensor_t* y_true = create_1d(y_true_data);
    nimcp_gpu_tensor_t* weight = create_1d(weight_data);
    nimcp_gpu_tensor_t* y_pred = create_output(batch_dims, 1);
    nimcp_gpu_tensor_t* grad = create_output(batch_dims, 1);
    nimcp_gpu_tensor_t* weight_grad = create_output(weight_dims, 1);

    nimcp_optim_state_t* state = track_optim(nimcp_optim_state_create(ctx, NIMCP_OPTIM_SGD, weight, 0.01f));
    ASSERT_NE(state, nullptr);

    float initial_loss = 0.0f;
    float final_loss = 0.0f;

    // Training loop
    for (int epoch = 0; epoch < 100; epoch++) {
        // Forward: y_pred = x * weight (element-wise for simplicity)
        nimcp_gpu_mul_scalar(ctx, x, to_host(weight)[0], y_pred);

        // Loss: MSE
        float loss = 0.0f;
        nimcp_gpu_loss_mse(ctx, y_pred, y_true, &loss, grad);

        if (epoch == 0) initial_loss = loss;
        if (epoch == 99) final_loss = loss;

        // Backward: dL/dw = sum(x * (y_pred - y_true) * 2 / n)
        // Simplified: just use the mean gradient
        std::vector<float> grad_data = to_host(grad);
        std::vector<float> x_host = to_host(x);
        float weight_g = 0.0f;
        for (size_t i = 0; i < 4; i++) {
            weight_g += x_host[i] * grad_data[i];
        }
        std::vector<float> wg = {weight_g};
        nimcp_gpu_tensor_t* wg_tensor = create_1d(wg);

        // Update weight
        nimcp_gpu_optim_sgd(ctx, weight, wg_tensor, state);
    }

    // Loss should decrease
    EXPECT_LT(final_loss, initial_loss);

    // Weight should be closer to 2
    std::vector<float> final_weight = to_host(weight);
    EXPECT_GT(final_weight[0], 1.5f);
}

/**
 * TEST: Large batch training
 * WHAT: Verify training works with large batch sizes
 * WHY:  Real training uses large batches
 */
TEST_F(GPUTrainingKernelTest, Integration_LargeBatch_Succeeds) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    // Large batch: 1024 samples
    size_t batch_size = 1024;
    std::vector<float> pred_data(batch_size, 1.0f);
    std::vector<float> target_data(batch_size, 0.0f);
    size_t dims[] = {batch_size};

    nimcp_gpu_tensor_t* pred = create_tensor(pred_data.data(), dims, 1);
    nimcp_gpu_tensor_t* target = create_tensor(target_data.data(), dims, 1);
    nimcp_gpu_tensor_t* grad = create_output(dims, 1);

    float loss = 0.0f;
    bool result = nimcp_gpu_loss_mse(ctx, pred, target, &loss, grad);

    EXPECT_TRUE(result);
    EXPECT_NEAR(loss, 1.0f, LOSS_TOLERANCE);  // MSE of (1-0)^2 = 1

    // Verify gradient computed for all elements
    std::vector<float> grad_data = to_host(grad);
    EXPECT_EQ(grad_data.size(), batch_size);
    for (float g : grad_data) {
        EXPECT_FALSE(std::isnan(g));
    }
}
