/**
 * @file test_e2e_inference_pipeline_gpu.cpp
 * @brief E2E Tests for GPU Inference Pipeline
 *
 * WHAT: End-to-end testing for GPU inference workflows
 * WHY:  Verify inference correctness, performance, and fused operations
 * HOW:  Test batched inference, fused operations, normalization
 *
 * TEST PIPELINES:
 * - SessionCreation: Create inference session
 * - FusedLinearReLU: Test fused linear+ReLU operation
 * - LayerNormalization: Test layer normalization
 * - BatchedInference: Run inference on multiple samples
 * - InferenceSession: Test inference session workflow
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 * @version 1.0.0
 */

#include "e2e_test_framework.h"

// Headers have their own extern "C" guards
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/inference/nimcp_inference_gpu.h"

#include <vector>
#include <cmath>
#include <cstring>
#include <random>
#include <chrono>
#include <iostream>
#include <algorithm>
#include <numeric>

//=============================================================================
// Test Data Helpers
//=============================================================================

/**
 * @brief Generate random input data for inference
 */
static void generate_inference_input(
    float* data,
    size_t batch_size,
    size_t input_dim)
{
    std::mt19937 gen(42);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    for (size_t i = 0; i < batch_size * input_dim; i++) {
        data[i] = dist(gen);
    }
}

/**
 * @brief Initialize weights with small values
 */
static void initialize_model_weights(
    float* weights,
    size_t fan_in,
    size_t fan_out)
{
    std::mt19937 gen(42);
    float stddev = std::sqrt(2.0f / static_cast<float>(fan_in + fan_out));
    std::normal_distribution<float> dist(0.0f, stddev);

    for (size_t i = 0; i < fan_in * fan_out; i++) {
        weights[i] = dist(gen);
    }
}

//=============================================================================
// Test Fixture
//=============================================================================

class GPUInferencePipelineE2ETest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* ctx_ = nullptr;
    bool gpu_available_ = false;

    void SetUp() override {
        ctx_ = nimcp_gpu_context_create(0);
        gpu_available_ = (ctx_ != nullptr);

        if (!gpu_available_) {
            std::cout << "GPU not available - some tests will be skipped" << std::endl;
        }
    }

    void TearDown() override {
        if (ctx_) {
            nimcp_gpu_context_destroy(ctx_);
            ctx_ = nullptr;
        }
    }

    bool HasGPU() const { return gpu_available_; }

    void SkipIfNoGPU() {
        if (!gpu_available_) {
            GTEST_SKIP() << "GPU not available";
        }
    }
};

//=============================================================================
// Pipeline 1: Session Creation
//=============================================================================

TEST_F(GPUInferencePipelineE2ETest, SessionCreation) {
    SkipIfNoGPU();

    E2E_PIPELINE_START("Session Creation");

    // Stage 1: Create session with FP32
    E2E_STAGE_BEGIN("Create FP32 session", 500);

    nimcp_infer_session_t* session_fp32 = nimcp_infer_session_create(
        ctx_, NIMCP_INFER_FP32, 0);
    E2E_ASSERT_NOT_NULL(session_fp32, "Failed to create FP32 session");

    std::cout << "\n  Created FP32 inference session" << std::endl;

    E2E_STAGE_END();

    // Stage 2: Get recommended precision
    E2E_STAGE_BEGIN("Check recommended precision", 300);

    nimcp_infer_precision_t recommended = nimcp_gpu_infer_recommended_precision(ctx_);
    const char* prec_names[] = {"FP32", "FP16", "BF16", "INT8", "INT4", "TF32"};

    std::cout << "  Recommended precision: " << prec_names[recommended] << std::endl;

    E2E_STAGE_END();

    // Stage 3: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 300);

    nimcp_infer_session_destroy(session_fp32);

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 2: Fused Linear + ReLU
//=============================================================================

TEST_F(GPUInferencePipelineE2ETest, FusedLinearReLU) {
    SkipIfNoGPU();

    E2E_PIPELINE_START("Fused Linear + ReLU");

    const size_t batch_size = 16;
    const size_t in_features = 64;
    const size_t out_features = 32;

    // Stage 1: Create tensors
    E2E_STAGE_BEGIN("Create tensors", 500);

    // Input tensor
    std::vector<float> input_data(batch_size * in_features);
    generate_inference_input(input_data.data(), batch_size, in_features);

    size_t input_dims[] = {batch_size, in_features};
    nimcp_gpu_tensor_t* input = nimcp_gpu_tensor_from_host(
        ctx_, input_data.data(), input_dims, 2, NIMCP_GPU_PRECISION_FP32);
    E2E_ASSERT_NOT_NULL(input, "Failed to create input tensor");

    // Weights tensor
    std::vector<float> weight_data(out_features * in_features);
    initialize_model_weights(weight_data.data(), in_features, out_features);

    size_t weight_dims[] = {out_features, in_features};
    nimcp_gpu_tensor_t* weights = nimcp_gpu_tensor_from_host(
        ctx_, weight_data.data(), weight_dims, 2, NIMCP_GPU_PRECISION_FP32);
    E2E_ASSERT_NOT_NULL(weights, "Failed to create weights tensor");

    // Bias tensor
    std::vector<float> bias_data(out_features, 0.1f);
    size_t bias_dims[] = {out_features};
    nimcp_gpu_tensor_t* bias = nimcp_gpu_tensor_from_host(
        ctx_, bias_data.data(), bias_dims, 1, NIMCP_GPU_PRECISION_FP32);
    E2E_ASSERT_NOT_NULL(bias, "Failed to create bias tensor");

    // Output tensor
    size_t output_dims[] = {batch_size, out_features};
    nimcp_gpu_tensor_t* output = nimcp_gpu_tensor_create(
        ctx_, output_dims, 2, NIMCP_GPU_PRECISION_FP32);
    E2E_ASSERT_NOT_NULL(output, "Failed to create output tensor");

    std::cout << "\n  Input: [" << batch_size << ", " << in_features << "]" << std::endl;
    std::cout << "  Weights: [" << out_features << ", " << in_features << "]" << std::endl;
    std::cout << "  Output: [" << batch_size << ", " << out_features << "]" << std::endl;

    E2E_STAGE_END();

    // Stage 2: Run fused Linear + ReLU
    E2E_STAGE_BEGIN("Run fused Linear+ReLU", 1000);

    bool ok = nimcp_gpu_infer_linear_relu(ctx_, input, weights, bias, output);
    EXPECT_TRUE(ok) << "Fused Linear+ReLU failed";

    nimcp_gpu_context_synchronize(ctx_);

    E2E_STAGE_END();

    // Stage 3: Verify output
    E2E_STAGE_BEGIN("Verify output", 500);

    std::vector<float> output_host(batch_size * out_features);
    nimcp_gpu_tensor_to_host(output, output_host.data());

    // Check for NaN/Inf
    bool valid = true;
    size_t positive_count = 0;
    size_t zero_count = 0;

    for (size_t i = 0; i < output_host.size(); i++) {
        if (std::isnan(output_host[i]) || std::isinf(output_host[i])) {
            valid = false;
            break;
        }
        if (output_host[i] > 0) positive_count++;
        if (output_host[i] == 0) zero_count++;
    }

    EXPECT_TRUE(valid) << "Output contains NaN/Inf";

    // ReLU should produce some zeros and some positive values
    std::cout << "  Positive values: " << positive_count << "/" << output_host.size() << std::endl;
    std::cout << "  Zero values: " << zero_count << "/" << output_host.size() << std::endl;

    // Check ReLU: all values should be >= 0
    for (auto v : output_host) {
        EXPECT_GE(v, 0.0f) << "ReLU output should be non-negative";
    }

    E2E_STAGE_END();

    // Stage 4: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 300);

    nimcp_gpu_tensor_destroy(input);
    nimcp_gpu_tensor_destroy(weights);
    nimcp_gpu_tensor_destroy(bias);
    nimcp_gpu_tensor_destroy(output);

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 3: Layer Normalization
//=============================================================================

TEST_F(GPUInferencePipelineE2ETest, LayerNormalization) {
    SkipIfNoGPU();

    E2E_PIPELINE_START("Layer Normalization");

    const size_t batch_size = 8;
    const size_t hidden_dim = 64;

    // Stage 1: Create tensors
    E2E_STAGE_BEGIN("Create tensors", 500);

    // Input tensor with some variance
    std::vector<float> input_data(batch_size * hidden_dim);
    std::mt19937 gen(42);
    std::normal_distribution<float> dist(0.0f, 2.0f);

    for (auto& v : input_data) {
        v = dist(gen);
    }

    size_t input_dims[] = {batch_size, hidden_dim};
    nimcp_gpu_tensor_t* input = nimcp_gpu_tensor_from_host(
        ctx_, input_data.data(), input_dims, 2, NIMCP_GPU_PRECISION_FP32);
    E2E_ASSERT_NOT_NULL(input, "Failed to create input tensor");

    // Gamma (scale) - initialize to 1
    std::vector<float> gamma_data(hidden_dim, 1.0f);
    size_t param_dims[] = {hidden_dim};
    nimcp_gpu_tensor_t* gamma = nimcp_gpu_tensor_from_host(
        ctx_, gamma_data.data(), param_dims, 1, NIMCP_GPU_PRECISION_FP32);

    // Beta (bias) - initialize to 0
    std::vector<float> beta_data(hidden_dim, 0.0f);
    nimcp_gpu_tensor_t* beta = nimcp_gpu_tensor_from_host(
        ctx_, beta_data.data(), param_dims, 1, NIMCP_GPU_PRECISION_FP32);

    // Output
    nimcp_gpu_tensor_t* output = nimcp_gpu_tensor_create(
        ctx_, input_dims, 2, NIMCP_GPU_PRECISION_FP32);

    E2E_STAGE_END();

    // Stage 2: Run layer normalization
    E2E_STAGE_BEGIN("Run layer normalization", 1000);

    float eps = 1e-5f;
    bool ok = nimcp_gpu_infer_layernorm(ctx_, input, gamma, beta, output, eps);
    EXPECT_TRUE(ok) << "Layer normalization failed";

    nimcp_gpu_context_synchronize(ctx_);

    E2E_STAGE_END();

    // Stage 3: Verify normalization
    E2E_STAGE_BEGIN("Verify normalization", 500);

    std::vector<float> output_host(batch_size * hidden_dim);
    nimcp_gpu_tensor_to_host(output, output_host.data());

    // Check that each sample is normalized (mean ~0, std ~1)
    for (size_t b = 0; b < batch_size; b++) {
        float sum = 0.0f, sum_sq = 0.0f;
        for (size_t d = 0; d < hidden_dim; d++) {
            float v = output_host[b * hidden_dim + d];
            sum += v;
            sum_sq += v * v;
        }
        float mean = sum / hidden_dim;
        float var = sum_sq / hidden_dim - mean * mean;
        float std_dev = std::sqrt(var);

        if (b == 0) {
            std::cout << "\n  Sample 0 after LayerNorm:" << std::endl;
            std::cout << "    Mean: " << mean << " (expected ~0)" << std::endl;
            std::cout << "    Std:  " << std_dev << " (expected ~1)" << std::endl;
        }

        EXPECT_NEAR(mean, 0.0f, 0.1f) << "Mean should be close to 0";
        EXPECT_NEAR(std_dev, 1.0f, 0.1f) << "Std dev should be close to 1";
    }

    E2E_STAGE_END();

    // Stage 4: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 300);

    nimcp_gpu_tensor_destroy(input);
    nimcp_gpu_tensor_destroy(gamma);
    nimcp_gpu_tensor_destroy(beta);
    nimcp_gpu_tensor_destroy(output);

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 4: Batched Inference
//=============================================================================

TEST_F(GPUInferencePipelineE2ETest, BatchedInference) {
    SkipIfNoGPU();

    E2E_PIPELINE_START("Batched Inference");

    const size_t batch_size = 32;
    const size_t in_features = 128;
    const size_t out_features = 64;

    // Stage 1: Create shared weights
    E2E_STAGE_BEGIN("Create model weights", 500);

    std::vector<float> weight_data(out_features * in_features);
    initialize_model_weights(weight_data.data(), in_features, out_features);

    size_t weight_dims[] = {out_features, in_features};
    nimcp_gpu_tensor_t* weights = nimcp_gpu_tensor_from_host(
        ctx_, weight_data.data(), weight_dims, 2, NIMCP_GPU_PRECISION_FP32);

    std::vector<float> bias_data(out_features, 0.0f);
    size_t bias_dims[] = {out_features};
    nimcp_gpu_tensor_t* bias = nimcp_gpu_tensor_from_host(
        ctx_, bias_data.data(), bias_dims, 1, NIMCP_GPU_PRECISION_FP32);

    std::cout << "\n  Model: [" << in_features << "] -> [" << out_features << "]" << std::endl;

    E2E_STAGE_END();

    // Stage 2: Create batch input
    E2E_STAGE_BEGIN("Create batch input", 500);

    std::vector<float> batch_input(batch_size * in_features);
    generate_inference_input(batch_input.data(), batch_size, in_features);

    size_t input_dims[] = {batch_size, in_features};
    nimcp_gpu_tensor_t* input = nimcp_gpu_tensor_from_host(
        ctx_, batch_input.data(), input_dims, 2, NIMCP_GPU_PRECISION_FP32);

    size_t output_dims[] = {batch_size, out_features};
    nimcp_gpu_tensor_t* output = nimcp_gpu_tensor_create(
        ctx_, output_dims, 2, NIMCP_GPU_PRECISION_FP32);

    std::cout << "  Batch size: " << batch_size << std::endl;

    E2E_STAGE_END();

    // Stage 3: Run inference multiple times and measure throughput
    E2E_STAGE_BEGIN("Measure throughput", 2000);

    const int warmup_iters = 10;
    const int measure_iters = 100;

    // Warmup
    for (int i = 0; i < warmup_iters; i++) {
        nimcp_gpu_infer_linear_relu(ctx_, input, weights, bias, output);
    }
    nimcp_gpu_context_synchronize(ctx_);

    // Measure
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < measure_iters; i++) {
        nimcp_gpu_infer_linear_relu(ctx_, input, weights, bias, output);
    }
    nimcp_gpu_context_synchronize(ctx_);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    float samples_per_sec = (measure_iters * batch_size * 1e6f) / duration_us;

    std::cout << "  Iterations: " << measure_iters << std::endl;
    std::cout << "  Total time: " << duration_us / 1000.0f << " ms" << std::endl;
    std::cout << "  Throughput: " << samples_per_sec << " samples/sec" << std::endl;

    E2E_STAGE_END();

    // Stage 4: Verify output
    E2E_STAGE_BEGIN("Verify output", 500);

    std::vector<float> output_host(batch_size * out_features);
    nimcp_gpu_tensor_to_host(output, output_host.data());

    bool valid = true;
    for (auto v : output_host) {
        if (std::isnan(v) || std::isinf(v)) {
            valid = false;
            break;
        }
    }

    EXPECT_TRUE(valid) << "Output contains invalid values";

    E2E_STAGE_END();

    // Stage 5: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 300);

    nimcp_gpu_tensor_destroy(input);
    nimcp_gpu_tensor_destroy(output);
    nimcp_gpu_tensor_destroy(weights);
    nimcp_gpu_tensor_destroy(bias);

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 5: Inference Session
//=============================================================================

TEST_F(GPUInferencePipelineE2ETest, InferenceSession) {
    SkipIfNoGPU();

    E2E_PIPELINE_START("Inference Session");

    // Stage 1: Create session
    E2E_STAGE_BEGIN("Create inference session", 500);

    nimcp_infer_session_t* session = nimcp_infer_session_create(
        ctx_, NIMCP_INFER_FP32, 1024 * 1024);  // 1MB workspace
    E2E_ASSERT_NOT_NULL(session, "Failed to create session");

    std::cout << "\n  Created session with 1MB workspace" << std::endl;

    E2E_STAGE_END();

    // Stage 2: Warmup kernels
    E2E_STAGE_BEGIN("Warmup kernels", 1000);

    bool warmup_ok = nimcp_gpu_infer_warmup(ctx_, NIMCP_INFER_FP32);
    std::cout << "  Warmup: " << (warmup_ok ? "OK" : "FAILED") << std::endl;

    E2E_STAGE_END();

    // Stage 3: Test in-place activation
    E2E_STAGE_BEGIN("Test in-place activation", 1000);

    const size_t size = 1024;
    std::vector<float> data(size);
    std::mt19937 gen(42);
    std::normal_distribution<float> dist(0.0f, 1.0f);

    for (auto& v : data) {
        v = dist(gen);
    }

    size_t dims[] = {size};
    nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_from_host(
        ctx_, data.data(), dims, 1, NIMCP_GPU_PRECISION_FP32);

    // Apply ReLU in-place
    bool relu_ok = nimcp_gpu_infer_activation_inplace(ctx_, tensor, 0);  // 0 = ReLU
    EXPECT_TRUE(relu_ok);

    nimcp_gpu_context_synchronize(ctx_);

    // Verify
    std::vector<float> result(size);
    nimcp_gpu_tensor_to_host(tensor, result.data());

    size_t positive = 0, zero = 0;
    for (auto v : result) {
        if (v > 0) positive++;
        else if (v == 0) zero++;
    }

    std::cout << "  After ReLU: " << positive << " positive, " << zero << " zero" << std::endl;

    nimcp_gpu_tensor_destroy(tensor);

    E2E_STAGE_END();

    // Stage 4: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 300);

    nimcp_infer_session_destroy(session);

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
