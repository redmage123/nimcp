/* ============================================================================
 * Unit Tests: GPU Gradient Kernels with Recovery Integration
 * ============================================================================
 * WHAT: Unit tests for GPU gradient computation with recovery support
 * WHY:  Validate loss functions, gradient operations, and recovery from errors
 * HOW:  Test each function with valid inputs and error recovery scenarios
 * ============================================================================
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <random>

#ifdef NIMCP_ENABLE_CUDA
#include "gpu/training/nimcp_training_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#endif

namespace {

constexpr float TOLERANCE = 1e-4f;
constexpr float RELAXED_TOLERANCE = 1e-3f;

class GradientKernelsTest : public ::testing::Test {
protected:
    void SetUp() override {
#ifdef NIMCP_ENABLE_CUDA
        ctx_ = nimcp_gpu_context_create(0);
        if (!ctx_) {
            GTEST_SKIP() << "No GPU available - skipping test";
        }
        // Initialize recovery system
        if (!nimcp_gpu_recovery_is_initialized()) {
            nimcp_gpu_recovery_init(NULL);
        }
#endif
    }

    void TearDown() override {
#ifdef NIMCP_ENABLE_CUDA
        if (ctx_) {
            nimcp_gpu_context_destroy(ctx_);
            ctx_ = NULL;
        }
#endif
    }

#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_context_t* ctx_ = NULL;

    // Helper: Create a tensor with random data
    nimcp_gpu_tensor_t* create_random_tensor(const std::vector<size_t>& dims,
                                              float min_val = -1.0f, float max_val = 1.0f,
                                              unsigned int seed = 0) {
        nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(
            ctx_, dims.data(), static_cast<uint32_t>(dims.size()), NIMCP_GPU_PRECISION_FP32);
        if (!tensor) return nullptr;

        // Use provided seed, or auto-increment to ensure different tensors get different data
        static unsigned int auto_seed = 42;
        std::mt19937 gen(seed != 0 ? seed : auto_seed++);
        std::vector<float> data(tensor->numel);
        std::uniform_real_distribution<float> dist(min_val, max_val);
        for (auto& v : data) v = dist(gen);

        cudaMemcpy(tensor->data, data.data(), data.size() * sizeof(float),
                   cudaMemcpyHostToDevice);
        return tensor;
    }

    // Helper: Create a tensor with specific values
    nimcp_gpu_tensor_t* create_tensor_with_values(const std::vector<size_t>& dims,
                                                   const std::vector<float>& values) {
        nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(
            ctx_, dims.data(), static_cast<uint32_t>(dims.size()), NIMCP_GPU_PRECISION_FP32);
        if (!tensor) return nullptr;

        cudaMemcpy(tensor->data, values.data(), values.size() * sizeof(float),
                   cudaMemcpyHostToDevice);
        return tensor;
    }

    // Helper: Create a zeros tensor
    nimcp_gpu_tensor_t* create_zeros_tensor(const std::vector<size_t>& dims) {
        nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(
            ctx_, dims.data(), static_cast<uint32_t>(dims.size()), NIMCP_GPU_PRECISION_FP32);
        if (!tensor) return nullptr;
        nimcp_gpu_zeros(ctx_, tensor);
        return tensor;
    }

    // Helper: Download tensor to host
    std::vector<float> download_tensor(nimcp_gpu_tensor_t* tensor) {
        std::vector<float> data(tensor->numel);
        cudaMemcpy(data.data(), tensor->data, tensor->numel * sizeof(float),
                   cudaMemcpyDeviceToHost);
        return data;
    }

    // Helper: Compute CPU MSE loss
    float cpu_mse_loss(const std::vector<float>& pred, const std::vector<float>& target) {
        float sum = 0.0f;
        for (size_t i = 0; i < pred.size(); i++) {
            float diff = pred[i] - target[i];
            sum += diff * diff;
        }
        return sum / static_cast<float>(pred.size());
    }

    // Helper: Compute CPU MAE loss
    float cpu_mae_loss(const std::vector<float>& pred, const std::vector<float>& target) {
        float sum = 0.0f;
        for (size_t i = 0; i < pred.size(); i++) {
            sum += std::abs(pred[i] - target[i]);
        }
        return sum / static_cast<float>(pred.size());
    }
#endif
};

/* ============================================================================
 * Test: MSE Loss Computation
 * ============================================================================ */
TEST_F(GradientKernelsTest, MSELossForward) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    std::vector<size_t> dims = {64, 10};  // batch=64, features=10
    std::vector<float> pred_data(640), target_data(640);

    std::mt19937 gen(42);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    for (size_t i = 0; i < 640; i++) {
        pred_data[i] = dist(gen);
        target_data[i] = dist(gen);
    }

    nimcp_gpu_tensor_t* pred = create_tensor_with_values(dims, pred_data);
    nimcp_gpu_tensor_t* target = create_tensor_with_values(dims, target_data);
    ASSERT_NE(pred, nullptr);
    ASSERT_NE(target, nullptr);

    float loss = 0.0f;
    bool success = nimcp_gpu_loss_mse(ctx_, pred, target, &loss, nullptr);
    ASSERT_TRUE(success) << "MSE loss computation failed";

    // Compare with CPU reference
    float expected_loss = cpu_mse_loss(pred_data, target_data);
    EXPECT_NEAR(loss, expected_loss, TOLERANCE)
        << "MSE loss mismatch: GPU=" << loss << ", CPU=" << expected_loss;

    nimcp_gpu_tensor_destroy(pred);
    nimcp_gpu_tensor_destroy(target);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

TEST_F(GradientKernelsTest, MSELossBackward) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    std::vector<size_t> dims = {32, 8};
    std::vector<float> pred_data(256), target_data(256);

    for (size_t i = 0; i < 256; i++) {
        pred_data[i] = static_cast<float>(i) / 256.0f;
        target_data[i] = static_cast<float>(i + 10) / 256.0f;
    }

    nimcp_gpu_tensor_t* pred = create_tensor_with_values(dims, pred_data);
    nimcp_gpu_tensor_t* target = create_tensor_with_values(dims, target_data);
    nimcp_gpu_tensor_t* grad = create_zeros_tensor(dims);
    ASSERT_NE(pred, nullptr);
    ASSERT_NE(target, nullptr);
    ASSERT_NE(grad, nullptr);

    float loss = 0.0f;
    bool success = nimcp_gpu_loss_mse(ctx_, pred, target, &loss, grad);
    ASSERT_TRUE(success) << "MSE loss+grad computation failed";

    // Verify gradients are computed
    std::vector<float> grad_data = download_tensor(grad);
    float grad_sum = 0.0f;
    for (const auto& g : grad_data) grad_sum += std::abs(g);
    EXPECT_GT(grad_sum, 0.0f) << "Gradients should be non-zero";

    // MSE gradient = 2 * (pred - target) / n
    float scale = 2.0f / static_cast<float>(256);
    for (size_t i = 0; i < 256; i++) {
        float expected_grad = scale * (pred_data[i] - target_data[i]);
        EXPECT_NEAR(grad_data[i], expected_grad, TOLERANCE)
            << "Gradient mismatch at index " << i;
    }

    nimcp_gpu_tensor_destroy(pred);
    nimcp_gpu_tensor_destroy(target);
    nimcp_gpu_tensor_destroy(grad);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: MAE Loss Computation
 * ============================================================================ */
TEST_F(GradientKernelsTest, MAELossForward) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    std::vector<size_t> dims = {64, 10};
    std::vector<float> pred_data(640), target_data(640);

    std::mt19937 gen(123);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    for (size_t i = 0; i < 640; i++) {
        pred_data[i] = dist(gen);
        target_data[i] = dist(gen);
    }

    nimcp_gpu_tensor_t* pred = create_tensor_with_values(dims, pred_data);
    nimcp_gpu_tensor_t* target = create_tensor_with_values(dims, target_data);

    float loss = 0.0f;
    bool success = nimcp_gpu_loss_mae(ctx_, pred, target, &loss, nullptr);
    ASSERT_TRUE(success) << "MAE loss computation failed";

    float expected_loss = cpu_mae_loss(pred_data, target_data);
    EXPECT_NEAR(loss, expected_loss, TOLERANCE)
        << "MAE loss mismatch: GPU=" << loss << ", CPU=" << expected_loss;

    nimcp_gpu_tensor_destroy(pred);
    nimcp_gpu_tensor_destroy(target);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Cross Entropy Loss
 * ============================================================================ */
TEST_F(GradientKernelsTest, CrossEntropyLoss) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    // batch=8, num_classes=4
    std::vector<size_t> dims = {8, 4};

    // Create logits (unnormalized)
    std::vector<float> logits_data = {
        1.0f, 2.0f, 0.5f, 0.1f,
        0.1f, 0.2f, 3.0f, 0.5f,
        2.0f, 1.0f, 0.1f, 0.1f,
        0.5f, 0.5f, 0.5f, 2.5f,
        1.0f, 1.0f, 1.0f, 1.0f,
        0.0f, 0.0f, 0.0f, 3.0f,
        2.0f, 2.0f, 0.0f, 0.0f,
        1.5f, 0.5f, 0.5f, 0.5f
    };

    // Create one-hot targets
    std::vector<float> target_data = {
        0.0f, 1.0f, 0.0f, 0.0f,  // class 1
        0.0f, 0.0f, 1.0f, 0.0f,  // class 2
        1.0f, 0.0f, 0.0f, 0.0f,  // class 0
        0.0f, 0.0f, 0.0f, 1.0f,  // class 3
        1.0f, 0.0f, 0.0f, 0.0f,  // class 0
        0.0f, 0.0f, 0.0f, 1.0f,  // class 3
        0.0f, 1.0f, 0.0f, 0.0f,  // class 1
        1.0f, 0.0f, 0.0f, 0.0f   // class 0
    };

    nimcp_gpu_tensor_t* logits = create_tensor_with_values(dims, logits_data);
    nimcp_gpu_tensor_t* target = create_tensor_with_values(dims, target_data);
    nimcp_gpu_tensor_t* grad = create_zeros_tensor(dims);

    float loss = 0.0f;
    bool success = nimcp_gpu_loss_cross_entropy(ctx_, logits, target, &loss, grad, 1);  // mean
    ASSERT_TRUE(success) << "Cross entropy loss computation failed";

    // Loss should be positive
    EXPECT_GT(loss, 0.0f) << "Cross entropy loss should be positive";

    // Verify gradients exist
    std::vector<float> grad_data = download_tensor(grad);
    float grad_sum = 0.0f;
    for (const auto& g : grad_data) grad_sum += std::abs(g);
    EXPECT_GT(grad_sum, 0.0f) << "Gradients should be non-zero";

    nimcp_gpu_tensor_destroy(logits);
    nimcp_gpu_tensor_destroy(target);
    nimcp_gpu_tensor_destroy(grad);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Binary Cross Entropy Loss
 * ============================================================================ */
TEST_F(GradientKernelsTest, BCELoss) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    std::vector<size_t> dims = {64};

    // Predictions (sigmoid output, in [0,1])
    std::vector<float> pred_data(64), target_data(64);
    std::mt19937 gen(42);
    std::uniform_real_distribution<float> dist(0.1f, 0.9f);
    for (size_t i = 0; i < 64; i++) {
        pred_data[i] = dist(gen);
        target_data[i] = (i % 2 == 0) ? 1.0f : 0.0f;
    }

    nimcp_gpu_tensor_t* pred = create_tensor_with_values(dims, pred_data);
    nimcp_gpu_tensor_t* target = create_tensor_with_values(dims, target_data);

    float loss = 0.0f;
    bool success = nimcp_gpu_loss_bce(ctx_, pred, target, &loss, nullptr);
    ASSERT_TRUE(success) << "BCE loss computation failed";

    // Loss should be positive
    EXPECT_GT(loss, 0.0f) << "BCE loss should be positive";

    nimcp_gpu_tensor_destroy(pred);
    nimcp_gpu_tensor_destroy(target);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Huber Loss
 * ============================================================================ */
TEST_F(GradientKernelsTest, HuberLoss) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    std::vector<size_t> dims = {100};
    std::vector<float> pred_data(100), target_data(100);

    // Mix of small and large errors
    for (size_t i = 0; i < 100; i++) {
        pred_data[i] = static_cast<float>(i) / 10.0f;
        target_data[i] = (i < 50) ?
            pred_data[i] + 0.1f :   // Small error (quadratic region)
            pred_data[i] + 5.0f;    // Large error (linear region)
    }

    nimcp_gpu_tensor_t* pred = create_tensor_with_values(dims, pred_data);
    nimcp_gpu_tensor_t* target = create_tensor_with_values(dims, target_data);
    nimcp_gpu_tensor_t* grad = create_zeros_tensor(dims);

    float loss = 0.0f;
    float delta = 1.0f;
    bool success = nimcp_gpu_loss_huber(ctx_, pred, target, &loss, grad, delta);
    ASSERT_TRUE(success) << "Huber loss computation failed";

    EXPECT_GT(loss, 0.0f) << "Huber loss should be positive";

    // Verify gradient behavior
    std::vector<float> grad_data = download_tensor(grad);

    // For large errors, gradient should be clipped to delta
    for (size_t i = 50; i < 100; i++) {
        EXPECT_NEAR(std::abs(grad_data[i]), delta / 100.0f, RELAXED_TOLERANCE)
            << "Large error gradient should be clipped at index " << i;
    }

    nimcp_gpu_tensor_destroy(pred);
    nimcp_gpu_tensor_destroy(target);
    nimcp_gpu_tensor_destroy(grad);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Focal Loss
 * ============================================================================ */
TEST_F(GradientKernelsTest, FocalLoss) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    std::vector<size_t> dims = {64};
    std::vector<float> pred_data(64), target_data(64);

    // Simulate imbalanced classification
    for (size_t i = 0; i < 64; i++) {
        pred_data[i] = (i < 60) ? 0.9f : 0.1f;  // Easy vs hard
        target_data[i] = 1.0f;
    }

    nimcp_gpu_tensor_t* pred = create_tensor_with_values(dims, pred_data);
    nimcp_gpu_tensor_t* target = create_tensor_with_values(dims, target_data);

    float loss = 0.0f;
    float alpha = 0.25f;
    float gamma = 2.0f;
    bool success = nimcp_gpu_loss_focal(ctx_, pred, target, &loss, nullptr, alpha, gamma);
    ASSERT_TRUE(success) << "Focal loss computation failed";

    EXPECT_GT(loss, 0.0f) << "Focal loss should be positive";

    nimcp_gpu_tensor_destroy(pred);
    nimcp_gpu_tensor_destroy(target);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Gradient Accumulation
 * ============================================================================ */
TEST_F(GradientKernelsTest, GradientAccumulation) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    std::vector<size_t> dims = {100};
    std::vector<float> grad1_data(100), grad2_data(100);
    for (size_t i = 0; i < 100; i++) {
        grad1_data[i] = 1.0f;
        grad2_data[i] = 2.0f;
    }

    nimcp_gpu_tensor_t* grad = create_tensor_with_values(dims, grad1_data);
    nimcp_gpu_tensor_t* accum = create_tensor_with_values(dims, grad2_data);

    bool success = nimcp_gpu_gradient_accumulate(ctx_, grad, accum);
    ASSERT_TRUE(success) << "Gradient accumulation failed";

    std::vector<float> result = download_tensor(accum);
    for (size_t i = 0; i < 100; i++) {
        EXPECT_NEAR(result[i], 3.0f, TOLERANCE)
            << "Accumulation mismatch at index " << i;
    }

    nimcp_gpu_tensor_destroy(grad);
    nimcp_gpu_tensor_destroy(accum);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Gradient Scaling
 * ============================================================================ */
TEST_F(GradientKernelsTest, GradientScaling) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    std::vector<size_t> dims = {100};
    std::vector<float> grad_data(100);
    for (size_t i = 0; i < 100; i++) {
        grad_data[i] = 4.0f;
    }

    nimcp_gpu_tensor_t* grad = create_tensor_with_values(dims, grad_data);

    float scale = 0.25f;
    bool success = nimcp_gpu_gradient_scale(ctx_, grad, scale);
    ASSERT_TRUE(success) << "Gradient scaling failed";

    std::vector<float> result = download_tensor(grad);
    for (size_t i = 0; i < 100; i++) {
        EXPECT_NEAR(result[i], 1.0f, TOLERANCE)
            << "Scaling mismatch at index " << i;
    }

    nimcp_gpu_tensor_destroy(grad);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Gradient Clipping by Value
 * ============================================================================ */
TEST_F(GradientKernelsTest, GradientClipValue) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    std::vector<size_t> dims = {100};
    std::vector<float> grad_data(100);
    for (size_t i = 0; i < 100; i++) {
        grad_data[i] = static_cast<float>(i) - 50.0f;  // Range: -50 to 49
    }

    nimcp_gpu_tensor_t* grad = create_tensor_with_values(dims, grad_data);

    float clip_value = 10.0f;
    bool success = nimcp_gpu_gradient_clip_value(ctx_, grad, clip_value);
    ASSERT_TRUE(success) << "Gradient clipping by value failed";

    std::vector<float> result = download_tensor(grad);
    for (size_t i = 0; i < 100; i++) {
        EXPECT_GE(result[i], -clip_value) << "Value below clip at index " << i;
        EXPECT_LE(result[i], clip_value) << "Value above clip at index " << i;
    }

    nimcp_gpu_tensor_destroy(grad);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Gradient Zeroing
 * ============================================================================ */
TEST_F(GradientKernelsTest, GradientZero) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    std::vector<size_t> dims = {100};
    std::vector<float> grad_data(100);
    for (size_t i = 0; i < 100; i++) {
        grad_data[i] = static_cast<float>(i);
    }

    nimcp_gpu_tensor_t* grad = create_tensor_with_values(dims, grad_data);

    bool success = nimcp_gpu_gradient_zero(ctx_, grad);
    ASSERT_TRUE(success) << "Gradient zeroing failed";

    std::vector<float> result = download_tensor(grad);
    for (size_t i = 0; i < 100; i++) {
        EXPECT_NEAR(result[i], 0.0f, TOLERANCE)
            << "Non-zero value at index " << i;
    }

    nimcp_gpu_tensor_destroy(grad);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Large Batch Loss Computation
 * ============================================================================ */
TEST_F(GradientKernelsTest, LargeBatchMSE) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    std::vector<size_t> dims = {1024, 128};  // Large batch
    nimcp_gpu_tensor_t* pred = create_random_tensor(dims);
    nimcp_gpu_tensor_t* target = create_random_tensor(dims);
    nimcp_gpu_tensor_t* grad = create_zeros_tensor(dims);
    ASSERT_NE(pred, nullptr);
    ASSERT_NE(target, nullptr);
    ASSERT_NE(grad, nullptr);

    float loss = 0.0f;
    bool success = nimcp_gpu_loss_mse(ctx_, pred, target, &loss, grad);
    ASSERT_TRUE(success) << "Large batch MSE computation failed";

    EXPECT_GT(loss, 0.0f) << "Loss should be positive";

    // Verify gradients were computed
    std::vector<float> grad_data = download_tensor(grad);
    float grad_sum = 0.0f;
    for (const auto& g : grad_data) grad_sum += std::abs(g);
    EXPECT_GT(grad_sum, 0.0f) << "Gradients should be non-zero";

    nimcp_gpu_tensor_destroy(pred);
    nimcp_gpu_tensor_destroy(target);
    nimcp_gpu_tensor_destroy(grad);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Recovery System Initialization
 * ============================================================================ */
TEST_F(GradientKernelsTest, RecoverySystemInitialized) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    // Recovery system should be initialized
    EXPECT_TRUE(nimcp_gpu_recovery_is_initialized())
        << "Recovery system should be initialized after gradient operation";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Null Parameter Handling
 * ============================================================================ */
TEST_F(GradientKernelsTest, NullParameterHandling) {
#ifdef NIMCP_ENABLE_CUDA
    std::vector<size_t> dims = {10};
    nimcp_gpu_tensor_t* tensor = create_random_tensor(dims);

    float loss;

    // NULL context
    EXPECT_FALSE(nimcp_gpu_loss_mse(nullptr, tensor, tensor, &loss, nullptr));

    // NULL pred
    EXPECT_FALSE(nimcp_gpu_loss_mse(ctx_, nullptr, tensor, &loss, nullptr));

    // NULL target
    EXPECT_FALSE(nimcp_gpu_loss_mse(ctx_, tensor, nullptr, &loss, nullptr));

    // NULL loss output
    EXPECT_FALSE(nimcp_gpu_loss_mse(ctx_, tensor, tensor, nullptr, nullptr));

    // Gradient operations with NULL
    EXPECT_FALSE(nimcp_gpu_gradient_accumulate(ctx_, nullptr, tensor));
    EXPECT_FALSE(nimcp_gpu_gradient_accumulate(ctx_, tensor, nullptr));
    EXPECT_FALSE(nimcp_gpu_gradient_scale(ctx_, nullptr, 1.0f));
    EXPECT_FALSE(nimcp_gpu_gradient_zero(ctx_, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Recovery Statistics After Operations
 * ============================================================================ */
TEST_F(GradientKernelsTest, RecoveryStatistics) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    // Reset stats
    nimcp_gpu_recovery_reset_stats();

    // Run several operations
    std::vector<size_t> dims = {64};
    nimcp_gpu_tensor_t* pred = create_random_tensor(dims);
    nimcp_gpu_tensor_t* target = create_random_tensor(dims);

    float loss;
    for (int i = 0; i < 10; i++) {
        nimcp_gpu_loss_mse(ctx_, pred, target, &loss, nullptr);
    }

    // Get stats
    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);

    // Should have recorded some operations (may or may not have errors)
    // This is mainly to ensure stats API works
    EXPECT_GE(stats.success_rate, 0.0f);
    EXPECT_LE(stats.success_rate, 1.0f);

    nimcp_gpu_tensor_destroy(pred);
    nimcp_gpu_tensor_destroy(target);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

}  // namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
