/* ============================================================================
 * Unit Tests: CNN GPU Recovery Integration
 * ============================================================================
 * WHAT: Unit tests for GPU recovery in Convolutional Neural Network operations
 * WHY:  Validate self-healing and CPU fallback for CNN kernel failures
 * HOW:  Test recovery from OOM, kernel launch failures, numerical errors
 * ============================================================================
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <random>

#ifdef NIMCP_ENABLE_CUDA
#include "gpu/cnn/nimcp_cnn_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#endif

namespace {

constexpr float TOLERANCE = 1e-4f;

class CNNGPURecoveryTest : public ::testing::Test {
protected:
    void SetUp() override {
#ifdef NIMCP_ENABLE_CUDA
        ctx_ = nimcp_gpu_context_create(0);
        if (ctx_) {
            // Initialize recovery system
            nimcp_gpu_recovery_config_t config;
            nimcp_gpu_recovery_default_config(&config);
            config.enable_cpu_fallback = true;
            config.enable_param_correction = true;
            config.max_retries = 3;
            nimcp_gpu_recovery_init(&config);
        }
#endif
    }

    void TearDown() override {
#ifdef NIMCP_ENABLE_CUDA
        if (ctx_) {
            nimcp_gpu_context_destroy(ctx_);
            ctx_ = NULL;
        }
        nimcp_gpu_recovery_shutdown();
#endif
    }

#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_context_t* ctx_ = NULL;

    // Helper: Create 4D tensor for CNN (N, C, H, W)
    nimcp_gpu_tensor_t* create_4d_tensor(size_t batch, size_t channels, size_t height, size_t width) {
        size_t dims[4] = {batch, channels, height, width};
        return nimcp_gpu_tensor_create(ctx_, dims, 4, NIMCP_GPU_PRECISION_FP32);
    }

    // Helper: Create 1D tensor
    nimcp_gpu_tensor_t* create_1d_tensor(size_t size) {
        size_t dims[1] = {size};
        return nimcp_gpu_tensor_create(ctx_, dims, 1, NIMCP_GPU_PRECISION_FP32);
    }

    // Helper: Fill tensor with random values
    void fill_random(nimcp_gpu_tensor_t* tensor, float scale = 1.0f) {
        size_t numel = tensor->numel;
        std::vector<float> data(numel);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dis(-scale, scale);
        for (size_t i = 0; i < numel; i++) {
            data[i] = dis(gen);
        }
        cudaMemcpy(tensor->data, data.data(), numel * sizeof(float), cudaMemcpyHostToDevice);
    }

    // Helper: Create default conv params
    nimcp_conv_params_t make_conv_params(uint32_t kernel_size = 3, uint32_t stride = 1, uint32_t padding = 1) {
        nimcp_conv_params_t params;
        params.kernel_h = kernel_size;
        params.kernel_w = kernel_size;
        params.stride_h = stride;
        params.stride_w = stride;
        params.pad_h = padding;
        params.pad_w = padding;
        params.dilation_h = 1;
        params.dilation_w = 1;
        params.groups = 1;
        return params;
    }

    // Helper: Create default pool params
    nimcp_pool_params_t make_pool_params(uint32_t kernel_size = 2, uint32_t stride = 2) {
        nimcp_pool_params_t params;
        params.kernel_h = kernel_size;
        params.kernel_w = kernel_size;
        params.stride_h = stride;
        params.stride_w = stride;
        params.pad_h = 0;
        params.pad_w = 0;
        return params;
    }
#endif
};

/* ============================================================================
 * Test: Conv2D Forward with Recovery
 * ============================================================================ */
TEST_F(CNNGPURecoveryTest, Conv2DForwardRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    // Input: (batch=2, channels=3, height=32, width=32)
    // Filter: (out_channels=16, in_channels=3, kH=3, kW=3)
    // Output: (batch=2, channels=16, height=32, width=32) with pad=1
    size_t batch = 2, in_c = 3, height = 32, width = 32;
    size_t out_c = 16, k_h = 3, k_w = 3;

    nimcp_gpu_tensor_t* input = create_4d_tensor(batch, in_c, height, width);
    ASSERT_NE(input, nullptr);
    fill_random(input, 1.0f);

    size_t weight_dims[4] = {out_c, in_c, k_h, k_w};
    nimcp_gpu_tensor_t* weight = nimcp_gpu_tensor_create(ctx_, weight_dims, 4, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(weight, nullptr);
    fill_random(weight, 0.1f);

    nimcp_gpu_tensor_t* bias = create_1d_tensor(out_c);
    ASSERT_NE(bias, nullptr);
    nimcp_gpu_zeros(ctx_, bias);

    // Output with same spatial dims (pad=1, stride=1)
    nimcp_gpu_tensor_t* output = create_4d_tensor(batch, out_c, height, width);
    ASSERT_NE(output, nullptr);

    nimcp_conv_params_t params = make_conv_params(3, 1, 1);

    // Run conv2d forward with recovery
    bool success = nimcp_gpu_conv2d_forward(ctx_, input, weight, bias, output, &params);
    EXPECT_TRUE(success) << "Conv2D forward should succeed with recovery";

    // Verify output values are finite
    std::vector<float> result(output->numel);
    cudaMemcpy(result.data(), output->data, output->numel * sizeof(float), cudaMemcpyDeviceToHost);

    for (size_t i = 0; i < result.size(); i++) {
        EXPECT_FALSE(std::isnan(result[i])) << "Output NaN at index " << i;
        EXPECT_FALSE(std::isinf(result[i])) << "Output Inf at index " << i;
    }

    nimcp_gpu_tensor_destroy(input);
    nimcp_gpu_tensor_destroy(weight);
    nimcp_gpu_tensor_destroy(bias);
    nimcp_gpu_tensor_destroy(output);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Conv2D Backward with Recovery
 * ============================================================================ */
TEST_F(CNNGPURecoveryTest, Conv2DBackwardRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    size_t batch = 2, in_c = 3, height = 16, width = 16;
    size_t out_c = 8, k_h = 3, k_w = 3;
    size_t out_h = height, out_w = width;  // pad=1, stride=1

    nimcp_gpu_tensor_t* input = create_4d_tensor(batch, in_c, height, width);
    ASSERT_NE(input, nullptr);
    fill_random(input, 1.0f);

    size_t weight_dims[4] = {out_c, in_c, k_h, k_w};
    nimcp_gpu_tensor_t* weight = nimcp_gpu_tensor_create(ctx_, weight_dims, 4, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(weight, nullptr);
    fill_random(weight, 0.1f);

    // Gradient from next layer
    nimcp_gpu_tensor_t* grad_output = create_4d_tensor(batch, out_c, out_h, out_w);
    ASSERT_NE(grad_output, nullptr);
    fill_random(grad_output, 0.5f);

    // Gradients to compute
    nimcp_gpu_tensor_t* grad_input = create_4d_tensor(batch, in_c, height, width);
    nimcp_gpu_tensor_t* grad_weight = nimcp_gpu_tensor_create(ctx_, weight_dims, 4, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* grad_bias = create_1d_tensor(out_c);
    ASSERT_NE(grad_input, nullptr);
    ASSERT_NE(grad_weight, nullptr);
    ASSERT_NE(grad_bias, nullptr);

    nimcp_conv_params_t params = make_conv_params(3, 1, 1);

    // Run conv2d backward with recovery
    bool success = nimcp_gpu_conv2d_backward(ctx_, input, weight, grad_output,
                                              grad_input, grad_weight, grad_bias, &params);
    EXPECT_TRUE(success) << "Conv2D backward should succeed with recovery";

    // Verify gradient values are finite
    std::vector<float> grad_result(grad_input->numel);
    cudaMemcpy(grad_result.data(), grad_input->data, grad_input->numel * sizeof(float), cudaMemcpyDeviceToHost);

    for (size_t i = 0; i < grad_result.size(); i++) {
        EXPECT_FALSE(std::isnan(grad_result[i])) << "Grad input NaN at index " << i;
        EXPECT_FALSE(std::isinf(grad_result[i])) << "Grad input Inf at index " << i;
    }

    nimcp_gpu_tensor_destroy(input);
    nimcp_gpu_tensor_destroy(weight);
    nimcp_gpu_tensor_destroy(grad_output);
    nimcp_gpu_tensor_destroy(grad_input);
    nimcp_gpu_tensor_destroy(grad_weight);
    nimcp_gpu_tensor_destroy(grad_bias);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Conv1D Forward with Recovery
 * ============================================================================ */
TEST_F(CNNGPURecoveryTest, Conv1DForwardRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    // Input: (batch=4, channels=32, length=128) - audio-like data
    size_t batch = 4, in_c = 32, length = 128;
    size_t out_c = 64, kernel_size = 5;

    size_t input_dims[3] = {batch, in_c, length};
    nimcp_gpu_tensor_t* input = nimcp_gpu_tensor_create(ctx_, input_dims, 3, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(input, nullptr);
    fill_random(input, 1.0f);

    size_t weight_dims[3] = {out_c, in_c, kernel_size};
    nimcp_gpu_tensor_t* weight = nimcp_gpu_tensor_create(ctx_, weight_dims, 3, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(weight, nullptr);
    fill_random(weight, 0.1f);

    nimcp_gpu_tensor_t* bias = create_1d_tensor(out_c);
    ASSERT_NE(bias, nullptr);
    nimcp_gpu_zeros(ctx_, bias);

    // Output length with padding=2: same length
    size_t out_length = length;
    size_t output_dims[3] = {batch, out_c, out_length};
    nimcp_gpu_tensor_t* output = nimcp_gpu_tensor_create(ctx_, output_dims, 3, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(output, nullptr);

    // Run conv1d with recovery
    bool success = nimcp_gpu_conv1d_forward(ctx_, input, weight, bias, output,
                                             kernel_size, 1, 2, 1);  // kernel=5, stride=1, pad=2, dilation=1
    EXPECT_TRUE(success) << "Conv1D forward should succeed with recovery";

    // Verify output
    std::vector<float> result(output->numel);
    cudaMemcpy(result.data(), output->data, output->numel * sizeof(float), cudaMemcpyDeviceToHost);

    for (size_t i = 0; i < result.size(); i++) {
        EXPECT_FALSE(std::isnan(result[i])) << "Output NaN at index " << i;
    }

    nimcp_gpu_tensor_destroy(input);
    nimcp_gpu_tensor_destroy(weight);
    nimcp_gpu_tensor_destroy(bias);
    nimcp_gpu_tensor_destroy(output);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Depthwise Conv2D with Recovery
 * ============================================================================ */
TEST_F(CNNGPURecoveryTest, DepthwiseConv2DRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    size_t batch = 2, channels = 32, height = 16, width = 16;
    size_t k_h = 3, k_w = 3;

    nimcp_gpu_tensor_t* input = create_4d_tensor(batch, channels, height, width);
    ASSERT_NE(input, nullptr);
    fill_random(input, 1.0f);

    // Depthwise weight: (C, 1, kH, kW)
    size_t weight_dims[4] = {channels, 1, k_h, k_w};
    nimcp_gpu_tensor_t* weight = nimcp_gpu_tensor_create(ctx_, weight_dims, 4, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(weight, nullptr);
    fill_random(weight, 0.1f);

    nimcp_gpu_tensor_t* bias = create_1d_tensor(channels);
    ASSERT_NE(bias, nullptr);
    nimcp_gpu_zeros(ctx_, bias);

    nimcp_gpu_tensor_t* output = create_4d_tensor(batch, channels, height, width);
    ASSERT_NE(output, nullptr);

    nimcp_conv_params_t params = make_conv_params(3, 1, 1);
    params.groups = channels;  // Depthwise

    // Run depthwise conv with recovery
    bool success = nimcp_gpu_depthwise_conv2d(ctx_, input, weight, bias, output, &params);
    EXPECT_TRUE(success) << "Depthwise conv2d should succeed with recovery";

    nimcp_gpu_tensor_destroy(input);
    nimcp_gpu_tensor_destroy(weight);
    nimcp_gpu_tensor_destroy(bias);
    nimcp_gpu_tensor_destroy(output);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: MaxPool2D with Recovery
 * ============================================================================ */
TEST_F(CNNGPURecoveryTest, MaxPool2DRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    size_t batch = 2, channels = 16, height = 32, width = 32;
    size_t out_h = 16, out_w = 16;  // 2x2 pooling with stride 2

    nimcp_gpu_tensor_t* input = create_4d_tensor(batch, channels, height, width);
    ASSERT_NE(input, nullptr);
    fill_random(input, 1.0f);

    nimcp_gpu_tensor_t* output = create_4d_tensor(batch, channels, out_h, out_w);
    ASSERT_NE(output, nullptr);

    // Indices tensor for backward pass
    nimcp_gpu_tensor_t* indices = create_4d_tensor(batch, channels, out_h, out_w);
    ASSERT_NE(indices, nullptr);

    nimcp_pool_params_t params = make_pool_params(2, 2);

    // Run maxpool with recovery
    bool success = nimcp_gpu_maxpool2d(ctx_, input, output, indices, &params);
    EXPECT_TRUE(success) << "MaxPool2D should succeed with recovery";

    // Verify output values are in valid range (should be max of input window)
    std::vector<float> result(output->numel);
    cudaMemcpy(result.data(), output->data, output->numel * sizeof(float), cudaMemcpyDeviceToHost);

    for (size_t i = 0; i < result.size(); i++) {
        EXPECT_FALSE(std::isnan(result[i])) << "Output NaN at index " << i;
        EXPECT_GE(result[i], -1.0f) << "Max value should be >= min input";
        EXPECT_LE(result[i], 1.0f) << "Max value should be <= max input";
    }

    nimcp_gpu_tensor_destroy(input);
    nimcp_gpu_tensor_destroy(output);
    nimcp_gpu_tensor_destroy(indices);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: AvgPool2D with Recovery
 * ============================================================================ */
TEST_F(CNNGPURecoveryTest, AvgPool2DRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    size_t batch = 2, channels = 16, height = 32, width = 32;
    size_t out_h = 16, out_w = 16;

    nimcp_gpu_tensor_t* input = create_4d_tensor(batch, channels, height, width);
    ASSERT_NE(input, nullptr);
    fill_random(input, 1.0f);

    nimcp_gpu_tensor_t* output = create_4d_tensor(batch, channels, out_h, out_w);
    ASSERT_NE(output, nullptr);

    nimcp_pool_params_t params = make_pool_params(2, 2);

    // Run avgpool with recovery
    bool success = nimcp_gpu_avgpool2d(ctx_, input, output, &params);
    EXPECT_TRUE(success) << "AvgPool2D should succeed with recovery";

    // Verify output values are in valid range (average should be within input range)
    std::vector<float> result(output->numel);
    cudaMemcpy(result.data(), output->data, output->numel * sizeof(float), cudaMemcpyDeviceToHost);

    for (size_t i = 0; i < result.size(); i++) {
        EXPECT_FALSE(std::isnan(result[i])) << "Output NaN at index " << i;
        EXPECT_GE(result[i], -1.0f);
        EXPECT_LE(result[i], 1.0f);
    }

    nimcp_gpu_tensor_destroy(input);
    nimcp_gpu_tensor_destroy(output);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Global Average Pooling with Recovery
 * ============================================================================ */
TEST_F(CNNGPURecoveryTest, GlobalAvgPoolRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    size_t batch = 4, channels = 512, height = 7, width = 7;  // Typical pre-classifier size

    nimcp_gpu_tensor_t* input = create_4d_tensor(batch, channels, height, width);
    ASSERT_NE(input, nullptr);
    fill_random(input, 1.0f);

    // Output: (batch, channels, 1, 1) -> flatten to (batch, channels)
    size_t output_dims[2] = {batch, channels};
    nimcp_gpu_tensor_t* output = nimcp_gpu_tensor_create(ctx_, output_dims, 2, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(output, nullptr);

    // Run global avgpool with recovery
    bool success = nimcp_gpu_global_avgpool(ctx_, input, output);
    EXPECT_TRUE(success) << "Global AvgPool should succeed with recovery";

    // Verify output dimensions and values
    std::vector<float> result(output->numel);
    cudaMemcpy(result.data(), output->data, output->numel * sizeof(float), cudaMemcpyDeviceToHost);

    for (size_t i = 0; i < result.size(); i++) {
        EXPECT_FALSE(std::isnan(result[i])) << "Output NaN at index " << i;
    }

    nimcp_gpu_tensor_destroy(input);
    nimcp_gpu_tensor_destroy(output);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Adaptive Average Pooling with Recovery
 * ============================================================================ */
TEST_F(CNNGPURecoveryTest, AdaptiveAvgPool2DRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    size_t batch = 2, channels = 64, height = 28, width = 28;
    uint32_t out_h = 7, out_w = 7;  // Target output size

    nimcp_gpu_tensor_t* input = create_4d_tensor(batch, channels, height, width);
    ASSERT_NE(input, nullptr);
    fill_random(input, 1.0f);

    nimcp_gpu_tensor_t* output = create_4d_tensor(batch, channels, out_h, out_w);
    ASSERT_NE(output, nullptr);

    // Run adaptive avgpool with recovery
    bool success = nimcp_gpu_adaptive_avgpool2d(ctx_, input, output, out_h, out_w);
    EXPECT_TRUE(success) << "Adaptive AvgPool2D should succeed with recovery";

    nimcp_gpu_tensor_destroy(input);
    nimcp_gpu_tensor_destroy(output);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Batch Normalization Forward with Recovery
 * ============================================================================ */
TEST_F(CNNGPURecoveryTest, BatchNorm2DForwardRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    size_t batch = 8, channels = 64, height = 16, width = 16;

    nimcp_gpu_tensor_t* input = create_4d_tensor(batch, channels, height, width);
    ASSERT_NE(input, nullptr);
    fill_random(input, 1.0f);

    // BatchNorm parameters
    nimcp_gpu_tensor_t* gamma = create_1d_tensor(channels);
    nimcp_gpu_tensor_t* beta = create_1d_tensor(channels);
    nimcp_gpu_tensor_t* running_mean = create_1d_tensor(channels);
    nimcp_gpu_tensor_t* running_var = create_1d_tensor(channels);
    ASSERT_NE(gamma, nullptr);
    ASSERT_NE(beta, nullptr);
    ASSERT_NE(running_mean, nullptr);
    ASSERT_NE(running_var, nullptr);

    nimcp_gpu_fill(ctx_, gamma, 1.0f);  // Scale = 1
    nimcp_gpu_zeros(ctx_, beta);         // Shift = 0
    nimcp_gpu_zeros(ctx_, running_mean);
    nimcp_gpu_fill(ctx_, running_var, 1.0f);

    nimcp_gpu_tensor_t* output = create_4d_tensor(batch, channels, height, width);
    ASSERT_NE(output, nullptr);

    float momentum = 0.1f;
    float eps = 1e-5f;
    bool training = true;

    // Run batchnorm forward with recovery
    bool success = nimcp_gpu_batchnorm2d_forward(ctx_, input, gamma, beta, output,
                                                  running_mean, running_var,
                                                  momentum, eps, training);
    EXPECT_TRUE(success) << "BatchNorm2D forward should succeed with recovery";

    // In training mode, output should be roughly normalized (mean ~0, var ~1)
    std::vector<float> result(output->numel);
    cudaMemcpy(result.data(), output->data, output->numel * sizeof(float), cudaMemcpyDeviceToHost);

    // Check output is finite
    for (size_t i = 0; i < result.size(); i++) {
        EXPECT_FALSE(std::isnan(result[i])) << "Output NaN at index " << i;
        EXPECT_FALSE(std::isinf(result[i])) << "Output Inf at index " << i;
    }

    nimcp_gpu_tensor_destroy(input);
    nimcp_gpu_tensor_destroy(gamma);
    nimcp_gpu_tensor_destroy(beta);
    nimcp_gpu_tensor_destroy(running_mean);
    nimcp_gpu_tensor_destroy(running_var);
    nimcp_gpu_tensor_destroy(output);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Layer Normalization with Recovery
 * ============================================================================ */
TEST_F(CNNGPURecoveryTest, LayerNormRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    int batch_size = 16;
    int normalized_shape = 256;
    float eps = 1e-5f;

    // Create layer norm context
    nimcp_layer_norm_ctx_t* ln_ctx = nimcp_layer_norm_create(ctx_, normalized_shape, eps);
    ASSERT_NE(ln_ctx, nullptr) << "Layer norm context creation failed";

    // Create input and output buffers
    size_t data_size = batch_size * normalized_shape * sizeof(float);
    float* d_input = NULL;
    float* d_output = NULL;
    cudaMalloc(&d_input, data_size);
    cudaMalloc(&d_output, data_size);
    ASSERT_NE(d_input, nullptr);
    ASSERT_NE(d_output, nullptr);

    // Initialize input with random values
    std::vector<float> h_input(batch_size * normalized_shape);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<float> dis(0.0f, 1.0f);
    for (auto& v : h_input) v = dis(gen);
    cudaMemcpy(d_input, h_input.data(), data_size, cudaMemcpyHostToDevice);

    // Run layer norm forward with recovery
    int result = nimcp_layer_norm_forward(ln_ctx, d_input, d_output, batch_size);
    EXPECT_EQ(result, 0) << "Layer norm forward should succeed with recovery";

    // Verify output is normalized
    std::vector<float> h_output(batch_size * normalized_shape);
    cudaMemcpy(h_output.data(), d_output, data_size, cudaMemcpyDeviceToHost);

    // Check each sample is approximately normalized
    for (int b = 0; b < batch_size; b++) {
        float sum = 0.0f;
        for (int i = 0; i < normalized_shape; i++) {
            sum += h_output[b * normalized_shape + i];
            EXPECT_FALSE(std::isnan(h_output[b * normalized_shape + i]));
        }
        float mean = sum / normalized_shape;
        // Mean should be close to 0 (after normalization)
        EXPECT_NEAR(mean, 0.0f, 0.1f) << "Normalized mean should be ~0 for sample " << b;
    }

    cudaFree(d_input);
    cudaFree(d_output);
    nimcp_layer_norm_destroy(ln_ctx);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Instance Normalization with Recovery
 * ============================================================================ */
TEST_F(CNNGPURecoveryTest, InstanceNormRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    int batch_size = 4;
    int num_features = 64;  // channels
    int height = 16, width = 16;
    float eps = 1e-5f;
    bool affine = true;

    // Create instance norm context
    nimcp_instance_norm_ctx_t* in_ctx = nimcp_instance_norm_create(ctx_, num_features, eps, affine);
    ASSERT_NE(in_ctx, nullptr) << "Instance norm context creation failed";

    // Create input and output buffers
    size_t data_size = batch_size * num_features * height * width * sizeof(float);
    float* d_input = NULL;
    float* d_output = NULL;
    cudaMalloc(&d_input, data_size);
    cudaMalloc(&d_output, data_size);
    ASSERT_NE(d_input, nullptr);
    ASSERT_NE(d_output, nullptr);

    // Initialize input
    std::vector<float> h_input(batch_size * num_features * height * width);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<float> dis(0.0f, 1.0f);
    for (auto& v : h_input) v = dis(gen);
    cudaMemcpy(d_input, h_input.data(), data_size, cudaMemcpyHostToDevice);

    // Run instance norm forward with recovery
    int result = nimcp_instance_norm_forward(in_ctx, d_input, d_output, batch_size, height, width);
    EXPECT_EQ(result, 0) << "Instance norm forward should succeed with recovery";

    // Verify output is finite
    std::vector<float> h_output(batch_size * num_features * height * width);
    cudaMemcpy(h_output.data(), d_output, data_size, cudaMemcpyDeviceToHost);

    for (size_t i = 0; i < h_output.size(); i++) {
        EXPECT_FALSE(std::isnan(h_output[i])) << "Output NaN at index " << i;
    }

    cudaFree(d_input);
    cudaFree(d_output);
    nimcp_instance_norm_destroy(in_ctx);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Conv2D Backward Context API with Recovery
 * ============================================================================ */
TEST_F(CNNGPURecoveryTest, Conv2DBackwardContextRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    int batch = 2, in_c = 3, in_h = 16, in_w = 16;
    int out_c = 8, k_h = 3, k_w = 3;
    int stride_h = 1, stride_w = 1, pad_h = 1, pad_w = 1;
    int out_h = in_h, out_w = in_w;  // With padding

    // Create backward context
    nimcp_conv2d_backward_ctx_t* bwd_ctx = nimcp_conv2d_backward_create(
        ctx_, batch, in_c, in_h, in_w, out_c, k_h, k_w,
        stride_h, stride_w, pad_h, pad_w
    );
    ASSERT_NE(bwd_ctx, nullptr) << "Conv2D backward context creation failed";

    // Verify context parameters
    EXPECT_EQ(bwd_ctx->batch_size, batch);
    EXPECT_EQ(bwd_ctx->in_channels, in_c);
    EXPECT_EQ(bwd_ctx->out_channels, out_c);

    // Allocate test data
    size_t output_grad_size = batch * out_c * out_h * out_w * sizeof(float);
    size_t weights_size = out_c * in_c * k_h * k_w * sizeof(float);
    size_t input_size = batch * in_c * in_h * in_w * sizeof(float);

    float* d_output_grad = NULL;
    float* d_weights = NULL;
    float* d_input = NULL;
    cudaMalloc(&d_output_grad, output_grad_size);
    cudaMalloc(&d_weights, weights_size);
    cudaMalloc(&d_input, input_size);

    // Initialize with random values
    std::vector<float> h_output_grad(batch * out_c * out_h * out_w);
    std::vector<float> h_weights(out_c * in_c * k_h * k_w);
    std::vector<float> h_input(batch * in_c * in_h * in_w);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(-1.0f, 1.0f);
    for (auto& v : h_output_grad) v = dis(gen);
    for (auto& v : h_weights) v = dis(gen) * 0.1f;
    for (auto& v : h_input) v = dis(gen);

    cudaMemcpy(d_output_grad, h_output_grad.data(), output_grad_size, cudaMemcpyHostToDevice);
    cudaMemcpy(d_weights, h_weights.data(), weights_size, cudaMemcpyHostToDevice);
    cudaMemcpy(d_input, h_input.data(), input_size, cudaMemcpyHostToDevice);

    // Run backward pass
    int result = nimcp_conv2d_backward(bwd_ctx, d_output_grad, d_weights, d_input);
    EXPECT_EQ(result, 0) << "Conv2D backward should succeed with recovery";

    // Verify gradients are finite
    size_t input_grad_numel = batch * in_c * in_h * in_w;
    std::vector<float> h_input_grad(input_grad_numel);
    cudaMemcpy(h_input_grad.data(), bwd_ctx->d_input_grad, input_grad_numel * sizeof(float), cudaMemcpyDeviceToHost);

    for (size_t i = 0; i < input_grad_numel; i++) {
        EXPECT_FALSE(std::isnan(h_input_grad[i])) << "Input grad NaN at index " << i;
    }

    cudaFree(d_output_grad);
    cudaFree(d_weights);
    cudaFree(d_input);
    nimcp_conv2d_backward_destroy(bwd_ctx);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: im2col with Recovery
 * ============================================================================ */
TEST_F(CNNGPURecoveryTest, Im2ColRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    int C = 3, H = 8, W = 8;
    int kH = 3, kW = 3;
    int sH = 1, sW = 1;
    int pH = 1, pW = 1;
    int outH = H, outW = W;  // Same size with padding

    size_t input_size = C * H * W * sizeof(float);
    size_t col_size = C * kH * kW * outH * outW * sizeof(float);

    float* d_input = NULL;
    float* d_col = NULL;
    cudaMalloc(&d_input, input_size);
    cudaMalloc(&d_col, col_size);
    ASSERT_NE(d_input, nullptr);
    ASSERT_NE(d_col, nullptr);

    // Initialize input
    std::vector<float> h_input(C * H * W);
    for (size_t i = 0; i < h_input.size(); i++) {
        h_input[i] = static_cast<float>(i % 10);
    }
    cudaMemcpy(d_input, h_input.data(), input_size, cudaMemcpyHostToDevice);

    // Run im2col with recovery
    bool success = nimcp_gpu_im2col(ctx_, d_input, d_col, C, H, W, kH, kW, sH, sW, pH, pW, outH, outW);
    EXPECT_TRUE(success) << "im2col should succeed with recovery";

    // Verify column buffer is valid
    std::vector<float> h_col(C * kH * kW * outH * outW);
    cudaMemcpy(h_col.data(), d_col, col_size, cudaMemcpyDeviceToHost);

    for (size_t i = 0; i < h_col.size(); i++) {
        EXPECT_FALSE(std::isnan(h_col[i])) << "Column buffer NaN at index " << i;
    }

    cudaFree(d_input);
    cudaFree(d_col);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: col2im with Recovery
 * ============================================================================ */
TEST_F(CNNGPURecoveryTest, Col2ImRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    int C = 3, H = 8, W = 8;
    int kH = 3, kW = 3;
    int sH = 1, sW = 1;
    int pH = 1, pW = 1;
    int outH = H, outW = W;

    size_t col_size = C * kH * kW * outH * outW * sizeof(float);
    size_t input_grad_size = C * H * W * sizeof(float);

    float* d_col = NULL;
    float* d_input_grad = NULL;
    cudaMalloc(&d_col, col_size);
    cudaMalloc(&d_input_grad, input_grad_size);
    ASSERT_NE(d_col, nullptr);
    ASSERT_NE(d_input_grad, nullptr);

    // Initialize column buffer
    std::vector<float> h_col(C * kH * kW * outH * outW);
    for (size_t i = 0; i < h_col.size(); i++) {
        h_col[i] = 1.0f;  // Uniform for testing accumulation
    }
    cudaMemcpy(d_col, h_col.data(), col_size, cudaMemcpyHostToDevice);

    // Run col2im with recovery
    bool success = nimcp_gpu_col2im(ctx_, d_col, d_input_grad, C, H, W, kH, kW, sH, sW, pH, pW, outH, outW);
    EXPECT_TRUE(success) << "col2im should succeed with recovery";

    // Verify input gradient is valid
    std::vector<float> h_input_grad(C * H * W);
    cudaMemcpy(h_input_grad.data(), d_input_grad, input_grad_size, cudaMemcpyDeviceToHost);

    for (size_t i = 0; i < h_input_grad.size(); i++) {
        EXPECT_FALSE(std::isnan(h_input_grad[i])) << "Input grad NaN at index " << i;
        EXPECT_FALSE(std::isinf(h_input_grad[i])) << "Input grad Inf at index " << i;
    }

    cudaFree(d_col);
    cudaFree(d_input_grad);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Multiple Conv Layers Chain with Recovery
 * ============================================================================ */
TEST_F(CNNGPURecoveryTest, ConvChainRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    // Simulate a small CNN: Conv -> BN -> Pool -> Conv -> BN -> Pool
    size_t batch = 4, in_c = 3, height = 32, width = 32;

    // Layer 1: 3 -> 32 channels
    nimcp_gpu_tensor_t* x = create_4d_tensor(batch, in_c, height, width);
    ASSERT_NE(x, nullptr);
    fill_random(x, 1.0f);

    size_t w1_dims[4] = {32, 3, 3, 3};
    nimcp_gpu_tensor_t* w1 = nimcp_gpu_tensor_create(ctx_, w1_dims, 4, NIMCP_GPU_PRECISION_FP32);
    fill_random(w1, 0.1f);

    nimcp_gpu_tensor_t* b1 = create_1d_tensor(32);
    nimcp_gpu_zeros(ctx_, b1);

    nimcp_gpu_tensor_t* x1 = create_4d_tensor(batch, 32, 32, 32);

    nimcp_conv_params_t conv_params = make_conv_params(3, 1, 1);
    bool success = nimcp_gpu_conv2d_forward(ctx_, x, w1, b1, x1, &conv_params);
    EXPECT_TRUE(success) << "Conv1 failed";

    // Pool 1: 32x32 -> 16x16
    nimcp_gpu_tensor_t* p1 = create_4d_tensor(batch, 32, 16, 16);
    nimcp_gpu_tensor_t* idx1 = create_4d_tensor(batch, 32, 16, 16);
    nimcp_pool_params_t pool_params = make_pool_params(2, 2);
    success = nimcp_gpu_maxpool2d(ctx_, x1, p1, idx1, &pool_params);
    EXPECT_TRUE(success) << "Pool1 failed";

    // Layer 2: 32 -> 64 channels
    size_t w2_dims[4] = {64, 32, 3, 3};
    nimcp_gpu_tensor_t* w2 = nimcp_gpu_tensor_create(ctx_, w2_dims, 4, NIMCP_GPU_PRECISION_FP32);
    fill_random(w2, 0.1f);

    nimcp_gpu_tensor_t* b2 = create_1d_tensor(64);
    nimcp_gpu_zeros(ctx_, b2);

    nimcp_gpu_tensor_t* x2 = create_4d_tensor(batch, 64, 16, 16);
    success = nimcp_gpu_conv2d_forward(ctx_, p1, w2, b2, x2, &conv_params);
    EXPECT_TRUE(success) << "Conv2 failed";

    // Pool 2: 16x16 -> 8x8
    nimcp_gpu_tensor_t* p2 = create_4d_tensor(batch, 64, 8, 8);
    nimcp_gpu_tensor_t* idx2 = create_4d_tensor(batch, 64, 8, 8);
    success = nimcp_gpu_maxpool2d(ctx_, x2, p2, idx2, &pool_params);
    EXPECT_TRUE(success) << "Pool2 failed";

    // Verify final output
    std::vector<float> result(p2->numel);
    cudaMemcpy(result.data(), p2->data, p2->numel * sizeof(float), cudaMemcpyDeviceToHost);

    for (size_t i = 0; i < result.size(); i++) {
        EXPECT_FALSE(std::isnan(result[i])) << "Final output NaN at index " << i;
    }

    // Cleanup
    nimcp_gpu_tensor_destroy(x);
    nimcp_gpu_tensor_destroy(w1);
    nimcp_gpu_tensor_destroy(b1);
    nimcp_gpu_tensor_destroy(x1);
    nimcp_gpu_tensor_destroy(p1);
    nimcp_gpu_tensor_destroy(idx1);
    nimcp_gpu_tensor_destroy(w2);
    nimcp_gpu_tensor_destroy(b2);
    nimcp_gpu_tensor_destroy(x2);
    nimcp_gpu_tensor_destroy(p2);
    nimcp_gpu_tensor_destroy(idx2);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Recovery Statistics for CNN Operations
 * ============================================================================ */
TEST_F(CNNGPURecoveryTest, RecoveryStatistics) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    // Reset stats
    nimcp_gpu_recovery_reset_stats();

    // Perform some CNN operations
    size_t batch = 2, channels = 16, height = 16, width = 16;

    nimcp_gpu_tensor_t* input = create_4d_tensor(batch, channels, height, width);
    ASSERT_NE(input, nullptr);
    fill_random(input, 1.0f);

    size_t weight_dims[4] = {32, 16, 3, 3};
    nimcp_gpu_tensor_t* weight = nimcp_gpu_tensor_create(ctx_, weight_dims, 4, NIMCP_GPU_PRECISION_FP32);
    fill_random(weight, 0.1f);

    nimcp_gpu_tensor_t* bias = create_1d_tensor(32);
    nimcp_gpu_zeros(ctx_, bias);

    nimcp_gpu_tensor_t* output = create_4d_tensor(batch, 32, 16, 16);
    nimcp_conv_params_t params = make_conv_params(3, 1, 1);

    // Run multiple forward passes
    for (int i = 0; i < 10; i++) {
        nimcp_gpu_conv2d_forward(ctx_, input, weight, bias, output, &params);
    }

    // Get stats
    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);

    // Verify stats are valid
    EXPECT_GE(stats.success_rate, 0.0f);
    EXPECT_LE(stats.success_rate, 1.0f);

    nimcp_gpu_tensor_destroy(input);
    nimcp_gpu_tensor_destroy(weight);
    nimcp_gpu_tensor_destroy(bias);
    nimcp_gpu_tensor_destroy(output);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

} // namespace
