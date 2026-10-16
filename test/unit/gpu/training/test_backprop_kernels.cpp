//=============================================================================
// test_backprop_kernels.cpp - Unit Tests for Backpropagation GPU Kernels
//=============================================================================
/**
 * @file test_backprop_kernels.cpp
 * @brief GTest unit tests for backpropagation GPU kernels with recovery integration
 *
 * WHAT: Tests backpropagation kernels (linear, activations, normalization, dropout)
 * WHY:  Ensure GPU backprop operations work correctly with recovery integration
 * HOW:  Uses GTest framework with GPU tensor operations
 *
 * TEST CATEGORIES:
 * - Linear layer backward pass
 * - Activation function backward passes (ReLU, Sigmoid, Tanh, GELU, Softmax)
 * - Normalization backward passes (BatchNorm, LayerNorm)
 * - Dropout backward pass
 * - Recovery scenarios (OOM, numerical errors)
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

class BackpropKernelsTest : public ::testing::Test {
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
        std::mt19937 gen(rd());
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

    // Check if tensor is approximately zero
    bool is_approximately_zero(nimcp_gpu_tensor_t* tensor, float eps = 1e-5f) {
        auto data = get_tensor_data(tensor);
        for (float v : data) {
            if (std::abs(v) > eps) return false;
        }
        return true;
    }
};

//=============================================================================
// Linear Layer Backward Tests
//=============================================================================

TEST_F(BackpropKernelsTest, BackwardLinearBasic) {
    // Simple linear layer: y = x @ W^T + b
    // x: [2, 4] (batch=2, in_features=4)
    // W: [3, 4] (out_features=3, in_features=4)
    // y: [2, 3] (batch=2, out_features=3)

    nimcp_gpu_tensor_t* x = create_random_tensor({2, 4});
    nimcp_gpu_tensor_t* weight = create_random_tensor({3, 4});
    nimcp_gpu_tensor_t* grad_output = create_random_tensor({2, 3});

    nimcp_gpu_tensor_t* grad_input = create_zero_tensor({2, 4});
    nimcp_gpu_tensor_t* grad_weight = create_zero_tensor({3, 4});
    nimcp_gpu_tensor_t* grad_bias = create_zero_tensor({3});

    ASSERT_NE(x, nullptr);
    ASSERT_NE(weight, nullptr);
    ASSERT_NE(grad_output, nullptr);
    ASSERT_NE(grad_input, nullptr);
    ASSERT_NE(grad_weight, nullptr);
    ASSERT_NE(grad_bias, nullptr);

    bool result = nimcp_gpu_backward_linear(ctx, x, weight, grad_output,
                                            grad_input, grad_weight, grad_bias);
    EXPECT_TRUE(result);

    // Gradients should be non-zero
    EXPECT_FALSE(is_approximately_zero(grad_input));
    EXPECT_FALSE(is_approximately_zero(grad_weight));
    EXPECT_FALSE(is_approximately_zero(grad_bias));

    // No NaN/Inf in gradients
    EXPECT_FALSE(has_nan_or_inf(grad_input));
    EXPECT_FALSE(has_nan_or_inf(grad_weight));
    EXPECT_FALSE(has_nan_or_inf(grad_bias));

    nimcp_gpu_tensor_destroy(ctx, x);
    nimcp_gpu_tensor_destroy(ctx, weight);
    nimcp_gpu_tensor_destroy(ctx, grad_output);
    nimcp_gpu_tensor_destroy(ctx, grad_input);
    nimcp_gpu_tensor_destroy(ctx, grad_weight);
    nimcp_gpu_tensor_destroy(ctx, grad_bias);
}

TEST_F(BackpropKernelsTest, BackwardLinearNullBias) {
    // Linear backward without bias gradient
    nimcp_gpu_tensor_t* x = create_random_tensor({4, 8});
    nimcp_gpu_tensor_t* weight = create_random_tensor({6, 8});
    nimcp_gpu_tensor_t* grad_output = create_random_tensor({4, 6});

    nimcp_gpu_tensor_t* grad_input = create_zero_tensor({4, 8});
    nimcp_gpu_tensor_t* grad_weight = create_zero_tensor({6, 8});

    bool result = nimcp_gpu_backward_linear(ctx, x, weight, grad_output,
                                            grad_input, grad_weight, nullptr);
    EXPECT_TRUE(result);

    EXPECT_FALSE(is_approximately_zero(grad_input));
    EXPECT_FALSE(is_approximately_zero(grad_weight));

    nimcp_gpu_tensor_destroy(ctx, x);
    nimcp_gpu_tensor_destroy(ctx, weight);
    nimcp_gpu_tensor_destroy(ctx, grad_output);
    nimcp_gpu_tensor_destroy(ctx, grad_input);
    nimcp_gpu_tensor_destroy(ctx, grad_weight);
}

TEST_F(BackpropKernelsTest, BackwardLinearLargerBatch) {
    // Test with larger batch size
    nimcp_gpu_tensor_t* x = create_random_tensor({32, 128});
    nimcp_gpu_tensor_t* weight = create_random_tensor({64, 128});
    nimcp_gpu_tensor_t* grad_output = create_random_tensor({32, 64});

    nimcp_gpu_tensor_t* grad_input = create_zero_tensor({32, 128});
    nimcp_gpu_tensor_t* grad_weight = create_zero_tensor({64, 128});
    nimcp_gpu_tensor_t* grad_bias = create_zero_tensor({64});

    bool result = nimcp_gpu_backward_linear(ctx, x, weight, grad_output,
                                            grad_input, grad_weight, grad_bias);
    EXPECT_TRUE(result);

    EXPECT_FALSE(has_nan_or_inf(grad_input));
    EXPECT_FALSE(has_nan_or_inf(grad_weight));
    EXPECT_FALSE(has_nan_or_inf(grad_bias));

    nimcp_gpu_tensor_destroy(ctx, x);
    nimcp_gpu_tensor_destroy(ctx, weight);
    nimcp_gpu_tensor_destroy(ctx, grad_output);
    nimcp_gpu_tensor_destroy(ctx, grad_input);
    nimcp_gpu_tensor_destroy(ctx, grad_weight);
    nimcp_gpu_tensor_destroy(ctx, grad_bias);
}

//=============================================================================
// ReLU Backward Tests
//=============================================================================

TEST_F(BackpropKernelsTest, BackwardReluBasic) {
    // ReLU backward: dx = dy * (x > 0)
    std::vector<float> x_data = {-2.0f, -1.0f, 0.0f, 1.0f, 2.0f, 3.0f};
    std::vector<float> grad_out_data = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};

    nimcp_gpu_tensor_t* x = create_test_tensor({2, 3}, x_data);
    nimcp_gpu_tensor_t* grad_output = create_test_tensor({2, 3}, grad_out_data);
    nimcp_gpu_tensor_t* grad_input = create_zero_tensor({2, 3});

    bool result = nimcp_gpu_backward_relu(ctx, x, grad_output, grad_input);
    EXPECT_TRUE(result);

    auto grad_data = get_tensor_data(grad_input);
    // For x <= 0, gradient should be 0; for x > 0, gradient should be 1
    EXPECT_FLOAT_EQ(grad_data[0], 0.0f);  // x = -2
    EXPECT_FLOAT_EQ(grad_data[1], 0.0f);  // x = -1
    EXPECT_FLOAT_EQ(grad_data[2], 0.0f);  // x = 0 (ReLU derivative at 0 is typically 0)
    EXPECT_FLOAT_EQ(grad_data[3], 1.0f);  // x = 1
    EXPECT_FLOAT_EQ(grad_data[4], 1.0f);  // x = 2
    EXPECT_FLOAT_EQ(grad_data[5], 1.0f);  // x = 3

    nimcp_gpu_tensor_destroy(ctx, x);
    nimcp_gpu_tensor_destroy(ctx, grad_output);
    nimcp_gpu_tensor_destroy(ctx, grad_input);
}

TEST_F(BackpropKernelsTest, BackwardReluLarge) {
    // Test with larger tensor
    nimcp_gpu_tensor_t* x = create_random_tensor({64, 256});
    nimcp_gpu_tensor_t* grad_output = create_random_tensor({64, 256});
    nimcp_gpu_tensor_t* grad_input = create_zero_tensor({64, 256});

    bool result = nimcp_gpu_backward_relu(ctx, x, grad_output, grad_input);
    EXPECT_TRUE(result);

    EXPECT_FALSE(has_nan_or_inf(grad_input));

    nimcp_gpu_tensor_destroy(ctx, x);
    nimcp_gpu_tensor_destroy(ctx, grad_output);
    nimcp_gpu_tensor_destroy(ctx, grad_input);
}

//=============================================================================
// Sigmoid Backward Tests
//=============================================================================

TEST_F(BackpropKernelsTest, BackwardSigmoidBasic) {
    // Sigmoid backward: dx = dy * sigmoid(x) * (1 - sigmoid(x))
    // Note: API takes sigmoid output, not input
    std::vector<float> output_data = {0.1f, 0.5f, 0.9f, 0.3f, 0.7f, 0.2f};
    std::vector<float> grad_out_data = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};

    nimcp_gpu_tensor_t* output = create_test_tensor({2, 3}, output_data);
    nimcp_gpu_tensor_t* grad_output = create_test_tensor({2, 3}, grad_out_data);
    nimcp_gpu_tensor_t* grad_input = create_zero_tensor({2, 3});

    bool result = nimcp_gpu_backward_sigmoid(ctx, output, grad_output, grad_input);
    EXPECT_TRUE(result);

    auto grad_data = get_tensor_data(grad_input);
    // Expected: grad = sigmoid * (1 - sigmoid)
    EXPECT_NEAR(grad_data[0], 0.1f * 0.9f, 1e-5f);   // 0.09
    EXPECT_NEAR(grad_data[1], 0.5f * 0.5f, 1e-5f);   // 0.25 (max derivative)
    EXPECT_NEAR(grad_data[2], 0.9f * 0.1f, 1e-5f);   // 0.09

    nimcp_gpu_tensor_destroy(ctx, output);
    nimcp_gpu_tensor_destroy(ctx, grad_output);
    nimcp_gpu_tensor_destroy(ctx, grad_input);
}

TEST_F(BackpropKernelsTest, BackwardSigmoidLarge) {
    nimcp_gpu_tensor_t* output = create_random_tensor({32, 128}, 0.01f, 0.99f);
    nimcp_gpu_tensor_t* grad_output = create_random_tensor({32, 128});
    nimcp_gpu_tensor_t* grad_input = create_zero_tensor({32, 128});

    bool result = nimcp_gpu_backward_sigmoid(ctx, output, grad_output, grad_input);
    EXPECT_TRUE(result);

    EXPECT_FALSE(has_nan_or_inf(grad_input));

    nimcp_gpu_tensor_destroy(ctx, output);
    nimcp_gpu_tensor_destroy(ctx, grad_output);
    nimcp_gpu_tensor_destroy(ctx, grad_input);
}

//=============================================================================
// Tanh Backward Tests
//=============================================================================

TEST_F(BackpropKernelsTest, BackwardTanhBasic) {
    // Tanh backward: dx = dy * (1 - tanh(x)^2)
    // Note: API takes tanh output, not input
    std::vector<float> output_data = {-0.8f, -0.5f, 0.0f, 0.5f, 0.8f, 0.95f};
    std::vector<float> grad_out_data = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};

    nimcp_gpu_tensor_t* output = create_test_tensor({2, 3}, output_data);
    nimcp_gpu_tensor_t* grad_output = create_test_tensor({2, 3}, grad_out_data);
    nimcp_gpu_tensor_t* grad_input = create_zero_tensor({2, 3});

    bool result = nimcp_gpu_backward_tanh(ctx, output, grad_output, grad_input);
    EXPECT_TRUE(result);

    auto grad_data = get_tensor_data(grad_input);
    // Expected: grad = 1 - output^2
    EXPECT_NEAR(grad_data[0], 1.0f - 0.64f, 1e-5f);   // 0.36
    EXPECT_NEAR(grad_data[1], 1.0f - 0.25f, 1e-5f);   // 0.75
    EXPECT_NEAR(grad_data[2], 1.0f - 0.0f, 1e-5f);    // 1.0 (max derivative at 0)

    nimcp_gpu_tensor_destroy(ctx, output);
    nimcp_gpu_tensor_destroy(ctx, grad_output);
    nimcp_gpu_tensor_destroy(ctx, grad_input);
}

TEST_F(BackpropKernelsTest, BackwardTanhLarge) {
    nimcp_gpu_tensor_t* output = create_random_tensor({64, 64}, -0.99f, 0.99f);
    nimcp_gpu_tensor_t* grad_output = create_random_tensor({64, 64});
    nimcp_gpu_tensor_t* grad_input = create_zero_tensor({64, 64});

    bool result = nimcp_gpu_backward_tanh(ctx, output, grad_output, grad_input);
    EXPECT_TRUE(result);

    EXPECT_FALSE(has_nan_or_inf(grad_input));

    nimcp_gpu_tensor_destroy(ctx, output);
    nimcp_gpu_tensor_destroy(ctx, grad_output);
    nimcp_gpu_tensor_destroy(ctx, grad_input);
}

//=============================================================================
// GELU Backward Tests
//=============================================================================

TEST_F(BackpropKernelsTest, BackwardGeluBasic) {
    // GELU backward takes x (input), not output
    nimcp_gpu_tensor_t* x = create_random_tensor({4, 8});
    nimcp_gpu_tensor_t* grad_output = create_random_tensor({4, 8});
    nimcp_gpu_tensor_t* grad_input = create_zero_tensor({4, 8});

    bool result = nimcp_gpu_backward_gelu(ctx, x, grad_output, grad_input);
    EXPECT_TRUE(result);

    EXPECT_FALSE(is_approximately_zero(grad_input));
    EXPECT_FALSE(has_nan_or_inf(grad_input));

    nimcp_gpu_tensor_destroy(ctx, x);
    nimcp_gpu_tensor_destroy(ctx, grad_output);
    nimcp_gpu_tensor_destroy(ctx, grad_input);
}

TEST_F(BackpropKernelsTest, BackwardGeluLarge) {
    nimcp_gpu_tensor_t* x = create_random_tensor({32, 256}, -3.0f, 3.0f);
    nimcp_gpu_tensor_t* grad_output = create_random_tensor({32, 256});
    nimcp_gpu_tensor_t* grad_input = create_zero_tensor({32, 256});

    bool result = nimcp_gpu_backward_gelu(ctx, x, grad_output, grad_input);
    EXPECT_TRUE(result);

    EXPECT_FALSE(has_nan_or_inf(grad_input));

    nimcp_gpu_tensor_destroy(ctx, x);
    nimcp_gpu_tensor_destroy(ctx, grad_output);
    nimcp_gpu_tensor_destroy(ctx, grad_input);
}

//=============================================================================
// Softmax Backward Tests
//=============================================================================

TEST_F(BackpropKernelsTest, BackwardSoftmaxBasic) {
    // Softmax output sums to 1
    std::vector<float> output_data = {0.1f, 0.2f, 0.7f, 0.3f, 0.4f, 0.3f};
    std::vector<float> grad_out_data = {0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f};

    nimcp_gpu_tensor_t* output = create_test_tensor({2, 3}, output_data);
    nimcp_gpu_tensor_t* grad_output = create_test_tensor({2, 3}, grad_out_data);
    nimcp_gpu_tensor_t* grad_input = create_zero_tensor({2, 3});

    bool result = nimcp_gpu_backward_softmax(ctx, output, grad_output, grad_input);
    EXPECT_TRUE(result);

    // Softmax gradient should not have NaN/Inf
    EXPECT_FALSE(has_nan_or_inf(grad_input));

    nimcp_gpu_tensor_destroy(ctx, output);
    nimcp_gpu_tensor_destroy(ctx, grad_output);
    nimcp_gpu_tensor_destroy(ctx, grad_input);
}

TEST_F(BackpropKernelsTest, BackwardSoftmaxLargeVocab) {
    // Test with larger vocabulary (common in language models)
    nimcp_gpu_tensor_t* output = create_random_tensor({16, 512}, 0.0001f, 0.1f);
    nimcp_gpu_tensor_t* grad_output = create_random_tensor({16, 512});
    nimcp_gpu_tensor_t* grad_input = create_zero_tensor({16, 512});

    bool result = nimcp_gpu_backward_softmax(ctx, output, grad_output, grad_input);
    EXPECT_TRUE(result);

    EXPECT_FALSE(has_nan_or_inf(grad_input));

    nimcp_gpu_tensor_destroy(ctx, output);
    nimcp_gpu_tensor_destroy(ctx, grad_output);
    nimcp_gpu_tensor_destroy(ctx, grad_input);
}

//=============================================================================
// BatchNorm Backward Tests
//=============================================================================

TEST_F(BackpropKernelsTest, BackwardBatchNormBasic) {
    // BatchNorm: y = gamma * (x - mean) / sqrt(var + eps) + beta
    size_t batch_size = 4;
    size_t num_features = 8;

    nimcp_gpu_tensor_t* x = create_random_tensor({batch_size, num_features});
    nimcp_gpu_tensor_t* gamma = create_random_tensor({num_features}, 0.5f, 2.0f);
    nimcp_gpu_tensor_t* mean = create_random_tensor({num_features}, -0.5f, 0.5f);
    nimcp_gpu_tensor_t* var = create_random_tensor({num_features}, 0.1f, 2.0f);
    nimcp_gpu_tensor_t* grad_output = create_random_tensor({batch_size, num_features});

    nimcp_gpu_tensor_t* grad_input = create_zero_tensor({batch_size, num_features});
    nimcp_gpu_tensor_t* grad_gamma = create_zero_tensor({num_features});
    nimcp_gpu_tensor_t* grad_beta = create_zero_tensor({num_features});

    bool result = nimcp_gpu_backward_batchnorm(ctx, x, gamma, mean, var,
                                               grad_output, grad_input,
                                               grad_gamma, grad_beta, 1e-5f);
    EXPECT_TRUE(result);

    EXPECT_FALSE(has_nan_or_inf(grad_input));
    EXPECT_FALSE(has_nan_or_inf(grad_gamma));
    EXPECT_FALSE(has_nan_or_inf(grad_beta));

    nimcp_gpu_tensor_destroy(ctx, x);
    nimcp_gpu_tensor_destroy(ctx, gamma);
    nimcp_gpu_tensor_destroy(ctx, mean);
    nimcp_gpu_tensor_destroy(ctx, var);
    nimcp_gpu_tensor_destroy(ctx, grad_output);
    nimcp_gpu_tensor_destroy(ctx, grad_input);
    nimcp_gpu_tensor_destroy(ctx, grad_gamma);
    nimcp_gpu_tensor_destroy(ctx, grad_beta);
}

TEST_F(BackpropKernelsTest, BackwardBatchNormLarge) {
    size_t batch_size = 32;
    size_t num_features = 256;

    nimcp_gpu_tensor_t* x = create_random_tensor({batch_size, num_features});
    nimcp_gpu_tensor_t* gamma = create_random_tensor({num_features}, 0.5f, 2.0f);
    nimcp_gpu_tensor_t* mean = create_random_tensor({num_features}, -0.5f, 0.5f);
    nimcp_gpu_tensor_t* var = create_random_tensor({num_features}, 0.1f, 2.0f);
    nimcp_gpu_tensor_t* grad_output = create_random_tensor({batch_size, num_features});

    nimcp_gpu_tensor_t* grad_input = create_zero_tensor({batch_size, num_features});
    nimcp_gpu_tensor_t* grad_gamma = create_zero_tensor({num_features});
    nimcp_gpu_tensor_t* grad_beta = create_zero_tensor({num_features});

    bool result = nimcp_gpu_backward_batchnorm(ctx, x, gamma, mean, var,
                                               grad_output, grad_input,
                                               grad_gamma, grad_beta, 1e-5f);
    EXPECT_TRUE(result);

    EXPECT_FALSE(has_nan_or_inf(grad_input));
    EXPECT_FALSE(has_nan_or_inf(grad_gamma));
    EXPECT_FALSE(has_nan_or_inf(grad_beta));

    nimcp_gpu_tensor_destroy(ctx, x);
    nimcp_gpu_tensor_destroy(ctx, gamma);
    nimcp_gpu_tensor_destroy(ctx, mean);
    nimcp_gpu_tensor_destroy(ctx, var);
    nimcp_gpu_tensor_destroy(ctx, grad_output);
    nimcp_gpu_tensor_destroy(ctx, grad_input);
    nimcp_gpu_tensor_destroy(ctx, grad_gamma);
    nimcp_gpu_tensor_destroy(ctx, grad_beta);
}

//=============================================================================
// LayerNorm Backward Tests
//=============================================================================

TEST_F(BackpropKernelsTest, BackwardLayerNormBasic) {
    // LayerNorm normalizes across features (not batch)
    size_t batch_size = 4;
    size_t num_features = 16;

    nimcp_gpu_tensor_t* x = create_random_tensor({batch_size, num_features});
    nimcp_gpu_tensor_t* gamma = create_random_tensor({num_features}, 0.5f, 2.0f);
    nimcp_gpu_tensor_t* mean = create_random_tensor({batch_size});
    nimcp_gpu_tensor_t* var = create_random_tensor({batch_size}, 0.1f, 2.0f);
    nimcp_gpu_tensor_t* grad_output = create_random_tensor({batch_size, num_features});

    nimcp_gpu_tensor_t* grad_input = create_zero_tensor({batch_size, num_features});
    nimcp_gpu_tensor_t* grad_gamma = create_zero_tensor({num_features});
    nimcp_gpu_tensor_t* grad_beta = create_zero_tensor({num_features});

    bool result = nimcp_gpu_backward_layernorm(ctx, x, gamma, mean, var,
                                               grad_output, grad_input,
                                               grad_gamma, grad_beta, 1e-5f);
    EXPECT_TRUE(result);

    EXPECT_FALSE(has_nan_or_inf(grad_input));
    EXPECT_FALSE(has_nan_or_inf(grad_gamma));
    EXPECT_FALSE(has_nan_or_inf(grad_beta));

    nimcp_gpu_tensor_destroy(ctx, x);
    nimcp_gpu_tensor_destroy(ctx, gamma);
    nimcp_gpu_tensor_destroy(ctx, mean);
    nimcp_gpu_tensor_destroy(ctx, var);
    nimcp_gpu_tensor_destroy(ctx, grad_output);
    nimcp_gpu_tensor_destroy(ctx, grad_input);
    nimcp_gpu_tensor_destroy(ctx, grad_gamma);
    nimcp_gpu_tensor_destroy(ctx, grad_beta);
}

TEST_F(BackpropKernelsTest, BackwardLayerNormTransformer) {
    // Test with typical transformer dimensions
    size_t batch_size = 16;
    size_t seq_len = 128;
    size_t hidden_dim = 256;

    // Reshape as 2D: [batch * seq, hidden]
    nimcp_gpu_tensor_t* x = create_random_tensor({batch_size * seq_len, hidden_dim});
    nimcp_gpu_tensor_t* gamma = create_random_tensor({hidden_dim}, 0.5f, 2.0f);
    nimcp_gpu_tensor_t* mean = create_random_tensor({batch_size * seq_len});
    nimcp_gpu_tensor_t* var = create_random_tensor({batch_size * seq_len}, 0.1f, 2.0f);
    nimcp_gpu_tensor_t* grad_output = create_random_tensor({batch_size * seq_len, hidden_dim});

    nimcp_gpu_tensor_t* grad_input = create_zero_tensor({batch_size * seq_len, hidden_dim});
    nimcp_gpu_tensor_t* grad_gamma = create_zero_tensor({hidden_dim});
    nimcp_gpu_tensor_t* grad_beta = create_zero_tensor({hidden_dim});

    bool result = nimcp_gpu_backward_layernorm(ctx, x, gamma, mean, var,
                                               grad_output, grad_input,
                                               grad_gamma, grad_beta, 1e-5f);
    EXPECT_TRUE(result);

    EXPECT_FALSE(has_nan_or_inf(grad_input));

    nimcp_gpu_tensor_destroy(ctx, x);
    nimcp_gpu_tensor_destroy(ctx, gamma);
    nimcp_gpu_tensor_destroy(ctx, mean);
    nimcp_gpu_tensor_destroy(ctx, var);
    nimcp_gpu_tensor_destroy(ctx, grad_output);
    nimcp_gpu_tensor_destroy(ctx, grad_input);
    nimcp_gpu_tensor_destroy(ctx, grad_gamma);
    nimcp_gpu_tensor_destroy(ctx, grad_beta);
}

//=============================================================================
// Dropout Backward Tests
//=============================================================================

TEST_F(BackpropKernelsTest, BackwardDropoutBasic) {
    // Dropout backward: dx = dy * mask / (1 - p)
    // Using p = 0.5
    std::vector<float> mask_data = {1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f};
    std::vector<float> grad_out_data = {2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f};

    nimcp_gpu_tensor_t* mask = create_test_tensor({2, 3}, mask_data);
    nimcp_gpu_tensor_t* grad_output = create_test_tensor({2, 3}, grad_out_data);
    nimcp_gpu_tensor_t* grad_input = create_zero_tensor({2, 3});

    float p = 0.5f;
    bool result = nimcp_gpu_backward_dropout(ctx, mask, grad_output, grad_input, p);
    EXPECT_TRUE(result);

    auto grad_data = get_tensor_data(grad_input);
    // Where mask = 1, grad = grad_out / (1 - p) = 2.0 / 0.5 = 4.0
    // Where mask = 0, grad = 0
    EXPECT_NEAR(grad_data[0], 4.0f, 1e-5f);  // mask = 1
    EXPECT_NEAR(grad_data[1], 0.0f, 1e-5f);  // mask = 0
    EXPECT_NEAR(grad_data[2], 4.0f, 1e-5f);  // mask = 1
    EXPECT_NEAR(grad_data[3], 0.0f, 1e-5f);  // mask = 0
    EXPECT_NEAR(grad_data[4], 4.0f, 1e-5f);  // mask = 1
    EXPECT_NEAR(grad_data[5], 4.0f, 1e-5f);  // mask = 1

    nimcp_gpu_tensor_destroy(ctx, mask);
    nimcp_gpu_tensor_destroy(ctx, grad_output);
    nimcp_gpu_tensor_destroy(ctx, grad_input);
}

TEST_F(BackpropKernelsTest, BackwardDropoutDifferentRates) {
    nimcp_gpu_tensor_t* mask = create_random_tensor({32, 64}, 0.0f, 1.0f);
    nimcp_gpu_tensor_t* grad_output = create_random_tensor({32, 64});
    nimcp_gpu_tensor_t* grad_input = create_zero_tensor({32, 64});

    // Test with different dropout rates
    float dropout_rates[] = {0.1f, 0.3f, 0.5f, 0.7f};

    for (float p : dropout_rates) {
        bool result = nimcp_gpu_backward_dropout(ctx, mask, grad_output, grad_input, p);
        EXPECT_TRUE(result) << "Failed with dropout rate: " << p;
        EXPECT_FALSE(has_nan_or_inf(grad_input)) << "NaN/Inf with dropout rate: " << p;
    }

    nimcp_gpu_tensor_destroy(ctx, mask);
    nimcp_gpu_tensor_destroy(ctx, grad_output);
    nimcp_gpu_tensor_destroy(ctx, grad_input);
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(BackpropKernelsTest, BackwardNullInputs) {
    nimcp_gpu_tensor_t* valid_tensor = create_random_tensor({4, 8});
    nimcp_gpu_tensor_t* grad_input = create_zero_tensor({4, 8});

    // Test null context
    EXPECT_FALSE(nimcp_gpu_backward_relu(nullptr, valid_tensor, valid_tensor, grad_input));

    // Test null input
    EXPECT_FALSE(nimcp_gpu_backward_relu(ctx, nullptr, valid_tensor, grad_input));

    // Test null grad_output
    EXPECT_FALSE(nimcp_gpu_backward_relu(ctx, valid_tensor, nullptr, grad_input));

    // Test null grad_input
    EXPECT_FALSE(nimcp_gpu_backward_relu(ctx, valid_tensor, valid_tensor, nullptr));

    nimcp_gpu_tensor_destroy(ctx, valid_tensor);
    nimcp_gpu_tensor_destroy(ctx, grad_input);
}

TEST_F(BackpropKernelsTest, BackwardLinearNullInputs) {
    nimcp_gpu_tensor_t* x = create_random_tensor({4, 8});
    nimcp_gpu_tensor_t* weight = create_random_tensor({6, 8});
    nimcp_gpu_tensor_t* grad_output = create_random_tensor({4, 6});
    nimcp_gpu_tensor_t* grad_input = create_zero_tensor({4, 8});
    nimcp_gpu_tensor_t* grad_weight = create_zero_tensor({6, 8});

    // Test null x
    EXPECT_FALSE(nimcp_gpu_backward_linear(ctx, nullptr, weight, grad_output,
                                           grad_input, grad_weight, nullptr));

    // Test null weight
    EXPECT_FALSE(nimcp_gpu_backward_linear(ctx, x, nullptr, grad_output,
                                           grad_input, grad_weight, nullptr));

    // Test null grad_output
    EXPECT_FALSE(nimcp_gpu_backward_linear(ctx, x, weight, nullptr,
                                           grad_input, grad_weight, nullptr));

    nimcp_gpu_tensor_destroy(ctx, x);
    nimcp_gpu_tensor_destroy(ctx, weight);
    nimcp_gpu_tensor_destroy(ctx, grad_output);
    nimcp_gpu_tensor_destroy(ctx, grad_input);
    nimcp_gpu_tensor_destroy(ctx, grad_weight);
}

//=============================================================================
// Recovery Integration Tests
//=============================================================================

TEST_F(BackpropKernelsTest, RecoveryAfterNumericalError) {
    // Create tensor with extreme values that might cause numerical issues
    std::vector<float> extreme_data = {1e30f, -1e30f, 1e-30f, -1e-30f};
    nimcp_gpu_tensor_t* output = create_test_tensor({2, 2}, extreme_data);
    nimcp_gpu_tensor_t* grad_output = create_random_tensor({2, 2});
    nimcp_gpu_tensor_t* grad_input = create_zero_tensor({2, 2});

    // This may trigger numerical warnings but should recover
    bool result = nimcp_gpu_backward_sigmoid(ctx, output, grad_output, grad_input);

    // The function should either succeed or gracefully fail
    // Recovery system should handle any CUDA errors
    if (result) {
        // If it succeeded, check for valid output
        EXPECT_FALSE(has_nan_or_inf(grad_input));
    }

    nimcp_gpu_tensor_destroy(ctx, output);
    nimcp_gpu_tensor_destroy(ctx, grad_output);
    nimcp_gpu_tensor_destroy(ctx, grad_input);
}

TEST_F(BackpropKernelsTest, RecoveryStateCheck) {
    // Verify recovery system is initialized and functional
    EXPECT_TRUE(nimcp_gpu_recovery_is_initialized());

    // Perform a successful operation
    nimcp_gpu_tensor_t* x = create_random_tensor({8, 16});
    nimcp_gpu_tensor_t* grad_output = create_random_tensor({8, 16});
    nimcp_gpu_tensor_t* grad_input = create_zero_tensor({8, 16});

    bool result = nimcp_gpu_backward_relu(ctx, x, grad_output, grad_input);
    EXPECT_TRUE(result);

    // Recovery system should still be operational
    EXPECT_TRUE(nimcp_gpu_recovery_is_initialized());

    nimcp_gpu_tensor_destroy(ctx, x);
    nimcp_gpu_tensor_destroy(ctx, grad_output);
    nimcp_gpu_tensor_destroy(ctx, grad_input);
}

//=============================================================================
// Gradient Numerical Validation Tests
//=============================================================================

TEST_F(BackpropKernelsTest, BackwardReluGradientCheck) {
    // Verify ReLU gradient is correct by checking specific values
    std::vector<float> x_data = {-1.0f, 0.5f, 2.0f, -0.1f};
    std::vector<float> grad_out_data = {0.5f, 0.5f, 0.5f, 0.5f};

    nimcp_gpu_tensor_t* x = create_test_tensor({1, 4}, x_data);
    nimcp_gpu_tensor_t* grad_output = create_test_tensor({1, 4}, grad_out_data);
    nimcp_gpu_tensor_t* grad_input = create_zero_tensor({1, 4});

    nimcp_gpu_backward_relu(ctx, x, grad_output, grad_input);

    auto grad_data = get_tensor_data(grad_input);
    EXPECT_NEAR(grad_data[0], 0.0f, 1e-5f);   // x=-1 < 0, grad=0
    EXPECT_NEAR(grad_data[1], 0.5f, 1e-5f);   // x=0.5 > 0, grad=0.5
    EXPECT_NEAR(grad_data[2], 0.5f, 1e-5f);   // x=2.0 > 0, grad=0.5
    EXPECT_NEAR(grad_data[3], 0.0f, 1e-5f);   // x=-0.1 < 0, grad=0

    nimcp_gpu_tensor_destroy(ctx, x);
    nimcp_gpu_tensor_destroy(ctx, grad_output);
    nimcp_gpu_tensor_destroy(ctx, grad_input);
}

TEST_F(BackpropKernelsTest, BackwardTanhGradientCheck) {
    // Verify tanh gradient at specific points
    // tanh'(x) = 1 - tanh(x)^2
    std::vector<float> output_data = {0.0f, 0.5f, -0.5f};  // tanh outputs
    std::vector<float> grad_out_data = {1.0f, 1.0f, 1.0f};

    nimcp_gpu_tensor_t* output = create_test_tensor({1, 3}, output_data);
    nimcp_gpu_tensor_t* grad_output = create_test_tensor({1, 3}, grad_out_data);
    nimcp_gpu_tensor_t* grad_input = create_zero_tensor({1, 3});

    nimcp_gpu_backward_tanh(ctx, output, grad_output, grad_input);

    auto grad_data = get_tensor_data(grad_input);
    EXPECT_NEAR(grad_data[0], 1.0f, 1e-5f);          // 1 - 0^2 = 1
    EXPECT_NEAR(grad_data[1], 0.75f, 1e-5f);         // 1 - 0.5^2 = 0.75
    EXPECT_NEAR(grad_data[2], 0.75f, 1e-5f);         // 1 - (-0.5)^2 = 0.75

    nimcp_gpu_tensor_destroy(ctx, output);
    nimcp_gpu_tensor_destroy(ctx, grad_output);
    nimcp_gpu_tensor_destroy(ctx, grad_input);
}
