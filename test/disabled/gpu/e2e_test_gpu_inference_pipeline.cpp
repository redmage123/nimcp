/**
 * @file e2e_test_gpu_inference_pipeline.cpp
 * @brief E2E Tests for GPU Inference Pipeline
 *
 * WHAT: End-to-end testing for GPU inference workflows
 * WHY:  Verify inference correctness, performance, and quantization support
 * HOW:  Test batched inference, fused operations, INT8 quantization, CUDA graphs
 *
 * TEST PIPELINES:
 * - ModelWeightLoading: Load model weights onto GPU
 * - BatchedInference: Run inference on multiple samples
 * - FusedOperations: Test fused GEMM+ReLU operations
 * - QuantizedInferenceINT8: Run INT8 quantized inference
 * - InferenceSession: Test inference session with CUDA graph capture
 * - ThroughputMeasurement: Measure and report inference throughput
 * - DynamicBatching: Handle variable batch sizes
 * - MultiStreamInference: Parallel inference streams
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
    std::random_device rd;
    std::mt19937 gen(rd());
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
    std::random_device rd;
    std::mt19937 gen(42);  // Fixed seed for reproducibility
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
// Pipeline 1: Model Weight Loading
//=============================================================================

TEST_F(GPUInferencePipelineE2ETest, ModelWeightLoading) {
    SkipIfNoGPU();

    E2E_PIPELINE_START("Model Weight Loading");

    // Model architecture
    const size_t layer_dims[] = {64, 128, 64, 32, 10};  // 4 layers
    const size_t num_layers = 4;

    std::vector<nimcp_gpu_tensor_t*> weights(num_layers);
    std::vector<nimcp_gpu_tensor_t*> biases(num_layers);

    // Stage 1: Allocate weight tensors
    E2E_STAGE_BEGIN("Allocate weight tensors", 1000);

    for (size_t i = 0; i < num_layers; i++) {
        size_t in_dim = layer_dims[i];
        size_t out_dim = layer_dims[i + 1];

        size_t w_dims[] = {in_dim, out_dim};
        size_t b_dims[] = {out_dim};

        weights[i] = nimcp_gpu_tensor_create(ctx_, w_dims, 2, NIMCP_GPU_PRECISION_FP32);
        biases[i] = nimcp_gpu_tensor_create(ctx_, b_dims, 1, NIMCP_GPU_PRECISION_FP32);

        E2E_ASSERT_NOT_NULL(weights[i], "Failed to create weight tensor");
        E2E_ASSERT_NOT_NULL(biases[i], "Failed to create bias tensor");
    }

    E2E_STAGE_END();

    // Stage 2: Load weights from host
    E2E_STAGE_BEGIN("Load weights from host", 1000);

    for (size_t i = 0; i < num_layers; i++) {
        size_t in_dim = layer_dims[i];
        size_t out_dim = layer_dims[i + 1];

        std::vector<float> w_host(in_dim * out_dim);
        std::vector<float> b_host(out_dim, 0.0f);

        initialize_model_weights(w_host.data(), in_dim, out_dim);

        // Create tensors from host data
        size_t w_dims[] = {in_dim, out_dim};
        size_t b_dims[] = {out_dim};

        nimcp_gpu_tensor_t* w_new = nimcp_gpu_tensor_from_host(
            ctx_, w_host.data(), w_dims, 2, NIMCP_GPU_PRECISION_FP32);
        nimcp_gpu_tensor_t* b_new = nimcp_gpu_tensor_from_host(
            ctx_, b_host.data(), b_dims, 1, NIMCP_GPU_PRECISION_FP32);

        EXPECT_NE(w_new, nullptr);
        EXPECT_NE(b_new, nullptr);

        nimcp_gpu_tensor_destroy(weights[i]);
        nimcp_gpu_tensor_destroy(biases[i]);
        weights[i] = w_new;
        biases[i] = b_new;
    }

    nimcp_gpu_context_synchronize(ctx_);

    E2E_STAGE_END();

    // Stage 3: Verify loaded weights
    E2E_STAGE_BEGIN("Verify loaded weights", 500);

    for (size_t i = 0; i < num_layers; i++) {
        size_t in_dim = layer_dims[i];
        size_t out_dim = layer_dims[i + 1];
        size_t num_elements = in_dim * out_dim;

        std::vector<float> w_verify(num_elements);
        bool copy_ok = nimcp_gpu_tensor_to_host(weights[i], w_verify.data());
        EXPECT_TRUE(copy_ok);

        // Check for valid values
        bool valid = true;
        float sum = 0.0f;
        for (const auto& v : w_verify) {
            if (std::isnan(v) || std::isinf(v)) {
                valid = false;
                break;
            }
            sum += std::abs(v);
        }
        EXPECT_TRUE(valid) << "Layer " << i << " weights contain NaN/Inf";
        EXPECT_GT(sum, 0.0f) << "Layer " << i << " weights should be non-zero";
    }

    E2E_STAGE_END();

    // Stage 4: Report model stats
    E2E_STAGE_BEGIN("Report model statistics", 200);

    size_t total_params = 0;
    for (size_t i = 0; i < num_layers; i++) {
        size_t in_dim = layer_dims[i];
        size_t out_dim = layer_dims[i + 1];
        total_params += in_dim * out_dim + out_dim;
    }

    size_t memory_bytes = total_params * sizeof(float);

    std::cout << "\n  Model Statistics:" << std::endl;
    std::cout << "    Layers: " << num_layers << std::endl;
    std::cout << "    Total parameters: " << total_params << std::endl;
    std::cout << "    Memory (FP32): " << (memory_bytes / 1024.0) << " KB" << std::endl;

    E2E_STAGE_END();

    // Stage 5: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 300);

    for (size_t i = 0; i < num_layers; i++) {
        nimcp_gpu_tensor_destroy(weights[i]);
        nimcp_gpu_tensor_destroy(biases[i]);
    }

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 2: Batched Inference
//=============================================================================

TEST_F(GPUInferencePipelineE2ETest, BatchedInference) {
    SkipIfNoGPU();

    E2E_PIPELINE_START("Batched Inference");

    const size_t input_dim = 32;
    const size_t hidden_dim = 64;
    const size_t output_dim = 10;
    const size_t batch_sizes[] = {1, 8, 32, 64, 128};
    const size_t num_batch_sizes = sizeof(batch_sizes) / sizeof(batch_sizes[0]);

    // Stage 1: Create model weights
    E2E_STAGE_BEGIN("Create model weights", 500);

    size_t w1_dims[] = {input_dim, hidden_dim};
    size_t w2_dims[] = {hidden_dim, output_dim};

    nimcp_gpu_tensor_t* W1 = nimcp_gpu_tensor_create(ctx_, w1_dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* W2 = nimcp_gpu_tensor_create(ctx_, w2_dims, 2, NIMCP_GPU_PRECISION_FP32);

    E2E_ASSERT_NOT_NULL(W1, "Failed to create W1");
    E2E_ASSERT_NOT_NULL(W2, "Failed to create W2");

    // Initialize weights
    std::vector<float> w1_host(input_dim * hidden_dim);
    std::vector<float> w2_host(hidden_dim * output_dim);
    initialize_model_weights(w1_host.data(), input_dim, hidden_dim);
    initialize_model_weights(w2_host.data(), hidden_dim, output_dim);

    nimcp_gpu_tensor_t* w1_init = nimcp_gpu_tensor_from_host(ctx_, w1_host.data(), w1_dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* w2_init = nimcp_gpu_tensor_from_host(ctx_, w2_host.data(), w2_dims, 2, NIMCP_GPU_PRECISION_FP32);

    nimcp_gpu_tensor_destroy(W1);
    nimcp_gpu_tensor_destroy(W2);
    W1 = w1_init;
    W2 = w2_init;

    E2E_STAGE_END();

    // Stage 2: Run inference with different batch sizes
    E2E_STAGE_BEGIN("Run batched inference", 3000);

    std::cout << "\n  Batch Size | Samples/ms" << std::endl;
    std::cout << "  -----------|----------" << std::endl;

    for (size_t bi = 0; bi < num_batch_sizes; bi++) {
        size_t batch_size = batch_sizes[bi];

        // Create input/output tensors
        size_t input_dims[] = {batch_size, input_dim};
        size_t hidden_dims[] = {batch_size, hidden_dim};
        size_t output_dims[] = {batch_size, output_dim};

        nimcp_gpu_tensor_t* input = nimcp_gpu_tensor_create(ctx_, input_dims, 2, NIMCP_GPU_PRECISION_FP32);
        nimcp_gpu_tensor_t* hidden = nimcp_gpu_tensor_create(ctx_, hidden_dims, 2, NIMCP_GPU_PRECISION_FP32);
        nimcp_gpu_tensor_t* output = nimcp_gpu_tensor_create(ctx_, output_dims, 2, NIMCP_GPU_PRECISION_FP32);

        // Generate random input
        std::vector<float> input_host(batch_size * input_dim);
        generate_inference_input(input_host.data(), batch_size, input_dim);
        nimcp_gpu_tensor_t* input_init = nimcp_gpu_tensor_from_host(
            ctx_, input_host.data(), input_dims, 2, NIMCP_GPU_PRECISION_FP32);
        nimcp_gpu_tensor_destroy(input);
        input = input_init;

        // Warmup
        nimcp_gpu_gemm(ctx_, input, W1, hidden, 1.0f, 0.0f, false, false);
        nimcp_gpu_relu(ctx_, hidden, hidden);
        nimcp_gpu_gemm(ctx_, hidden, W2, output, 1.0f, 0.0f, false, false);
        nimcp_gpu_context_synchronize(ctx_);

        // Timed inference
        const int num_iterations = 100;
        auto start = std::chrono::high_resolution_clock::now();

        for (int iter = 0; iter < num_iterations; iter++) {
            nimcp_gpu_gemm(ctx_, input, W1, hidden, 1.0f, 0.0f, false, false);
            nimcp_gpu_relu(ctx_, hidden, hidden);
            nimcp_gpu_gemm(ctx_, hidden, W2, output, 1.0f, 0.0f, false, false);
        }
        nimcp_gpu_context_synchronize(ctx_);

        auto end = std::chrono::high_resolution_clock::now();
        double total_ms = std::chrono::duration<double, std::milli>(end - start).count();
        double samples_per_ms = (batch_size * num_iterations) / total_ms;

        std::cout << "  " << std::setw(10) << batch_size << " | "
                  << std::setw(9) << std::fixed << std::setprecision(1)
                  << samples_per_ms << std::endl;

        // Verify output
        std::vector<float> output_host(batch_size * output_dim);
        nimcp_gpu_tensor_to_host(output, output_host.data());

        bool valid = true;
        for (const auto& v : output_host) {
            if (std::isnan(v) || std::isinf(v)) {
                valid = false;
                break;
            }
        }
        EXPECT_TRUE(valid) << "Output contains NaN/Inf for batch size " << batch_size;

        nimcp_gpu_tensor_destroy(input);
        nimcp_gpu_tensor_destroy(hidden);
        nimcp_gpu_tensor_destroy(output);
    }

    E2E_STAGE_END();

    // Stage 3: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 300);

    nimcp_gpu_tensor_destroy(W1);
    nimcp_gpu_tensor_destroy(W2);

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 3: Fused Operations
//=============================================================================

TEST_F(GPUInferencePipelineE2ETest, FusedOperations) {
    SkipIfNoGPU();

    E2E_PIPELINE_START("Fused Operations");

    const size_t batch_size = 64;
    const size_t input_dim = 128;
    const size_t output_dim = 64;

    // Stage 1: Create tensors
    E2E_STAGE_BEGIN("Create tensors", 500);

    size_t x_dims[] = {batch_size, input_dim};
    size_t w_dims[] = {input_dim, output_dim};
    size_t y_dims[] = {batch_size, output_dim};
    size_t b_dims[] = {output_dim};

    nimcp_gpu_tensor_t* x = nimcp_gpu_tensor_create(ctx_, x_dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* W = nimcp_gpu_tensor_create(ctx_, w_dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* b = nimcp_gpu_tensor_create(ctx_, b_dims, 1, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* y_unfused = nimcp_gpu_tensor_create(ctx_, y_dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* y_fused = nimcp_gpu_tensor_create(ctx_, y_dims, 2, NIMCP_GPU_PRECISION_FP32);

    E2E_ASSERT_NOT_NULL(x, "Failed to create x");
    E2E_ASSERT_NOT_NULL(W, "Failed to create W");
    E2E_ASSERT_NOT_NULL(y_unfused, "Failed to create y_unfused");
    E2E_ASSERT_NOT_NULL(y_fused, "Failed to create y_fused");

    E2E_STAGE_END();

    // Stage 2: Initialize data
    E2E_STAGE_BEGIN("Initialize data", 500);

    std::vector<float> x_host(batch_size * input_dim);
    std::vector<float> w_host(input_dim * output_dim);
    std::vector<float> b_host(output_dim);

    generate_inference_input(x_host.data(), batch_size, input_dim);
    initialize_model_weights(w_host.data(), input_dim, output_dim);
    for (size_t i = 0; i < output_dim; i++) {
        b_host[i] = 0.1f * static_cast<float>(i) / output_dim;
    }

    nimcp_gpu_tensor_t* x_init = nimcp_gpu_tensor_from_host(ctx_, x_host.data(), x_dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* w_init = nimcp_gpu_tensor_from_host(ctx_, w_host.data(), w_dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* b_init = nimcp_gpu_tensor_from_host(ctx_, b_host.data(), b_dims, 1, NIMCP_GPU_PRECISION_FP32);

    nimcp_gpu_tensor_destroy(x);
    nimcp_gpu_tensor_destroy(W);
    nimcp_gpu_tensor_destroy(b);
    x = x_init;
    W = w_init;
    b = b_init;

    E2E_STAGE_END();

    // Stage 3: Unfused operations
    E2E_STAGE_BEGIN("Run unfused operations", 1000);

    nimcp_gpu_tensor_t* temp = nimcp_gpu_tensor_create(ctx_, y_dims, 2, NIMCP_GPU_PRECISION_FP32);

    const int num_iterations = 50;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_iterations; i++) {
        // GEMM
        nimcp_gpu_gemm(ctx_, x, W, temp, 1.0f, 0.0f, false, false);
        // Add bias
        nimcp_gpu_add_broadcast(ctx_, temp, b, temp);
        // ReLU
        nimcp_gpu_relu(ctx_, temp, y_unfused);
    }
    nimcp_gpu_context_synchronize(ctx_);

    auto end = std::chrono::high_resolution_clock::now();
    double unfused_time_ms = std::chrono::duration<double, std::milli>(end - start).count();

    std::cout << "\n  Unfused time: " << unfused_time_ms << " ms" << std::endl;

    nimcp_gpu_tensor_destroy(temp);

    E2E_STAGE_END();

    // Stage 4: Fused GEMM + bias + ReLU
    E2E_STAGE_BEGIN("Run fused operations", 1000);

    start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_iterations; i++) {
        // Fused: y = ReLU(x @ W + b)
        bool ok = nimcp_gpu_fused_linear_relu(ctx_, x, W, b, y_fused);
        EXPECT_TRUE(ok);
    }
    nimcp_gpu_context_synchronize(ctx_);

    end = std::chrono::high_resolution_clock::now();
    double fused_time_ms = std::chrono::duration<double, std::milli>(end - start).count();

    std::cout << "  Fused time:   " << fused_time_ms << " ms" << std::endl;
    std::cout << "  Speedup:      " << (unfused_time_ms / fused_time_ms) << "x" << std::endl;

    E2E_STAGE_END();

    // Stage 5: Verify results match
    E2E_STAGE_BEGIN("Verify results match", 500);

    std::vector<float> y_unfused_host(batch_size * output_dim);
    std::vector<float> y_fused_host(batch_size * output_dim);

    nimcp_gpu_tensor_to_host(y_unfused, y_unfused_host.data());
    nimcp_gpu_tensor_to_host(y_fused, y_fused_host.data());

    float max_diff = 0.0f;
    for (size_t i = 0; i < batch_size * output_dim; i++) {
        float diff = std::abs(y_unfused_host[i] - y_fused_host[i]);
        max_diff = std::max(max_diff, diff);
    }

    std::cout << "  Max difference: " << max_diff << std::endl;
    EXPECT_LT(max_diff, 1e-4f) << "Fused and unfused results should match";

    E2E_STAGE_END();

    // Stage 6: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 300);

    nimcp_gpu_tensor_destroy(x);
    nimcp_gpu_tensor_destroy(W);
    nimcp_gpu_tensor_destroy(b);
    nimcp_gpu_tensor_destroy(y_unfused);
    nimcp_gpu_tensor_destroy(y_fused);

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 4: Quantized Inference (INT8)
//=============================================================================

TEST_F(GPUInferencePipelineE2ETest, QuantizedInferenceINT8) {
    SkipIfNoGPU();

    E2E_PIPELINE_START("Quantized Inference INT8");

    const size_t batch_size = 32;
    const size_t input_dim = 64;
    const size_t output_dim = 32;

    // Stage 1: Create FP32 reference model
    E2E_STAGE_BEGIN("Create FP32 reference model", 500);

    size_t x_dims[] = {batch_size, input_dim};
    size_t w_dims[] = {input_dim, output_dim};
    size_t y_dims[] = {batch_size, output_dim};

    // Create and initialize FP32 tensors
    std::vector<float> x_host(batch_size * input_dim);
    std::vector<float> w_host(input_dim * output_dim);
    generate_inference_input(x_host.data(), batch_size, input_dim);
    initialize_model_weights(w_host.data(), input_dim, output_dim);

    nimcp_gpu_tensor_t* x_fp32 = nimcp_gpu_tensor_from_host(
        ctx_, x_host.data(), x_dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* w_fp32 = nimcp_gpu_tensor_from_host(
        ctx_, w_host.data(), w_dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* y_fp32 = nimcp_gpu_tensor_create(
        ctx_, y_dims, 2, NIMCP_GPU_PRECISION_FP32);

    E2E_ASSERT_NOT_NULL(x_fp32, "Failed to create x_fp32");
    E2E_ASSERT_NOT_NULL(w_fp32, "Failed to create w_fp32");
    E2E_ASSERT_NOT_NULL(y_fp32, "Failed to create y_fp32");

    E2E_STAGE_END();

    // Stage 2: Run FP32 inference
    E2E_STAGE_BEGIN("Run FP32 inference", 500);

    bool fp32_ok = nimcp_gpu_gemm(ctx_, x_fp32, w_fp32, y_fp32, 1.0f, 0.0f, false, false);
    EXPECT_TRUE(fp32_ok);
    nimcp_gpu_context_synchronize(ctx_);

    std::vector<float> y_fp32_host(batch_size * output_dim);
    nimcp_gpu_tensor_to_host(y_fp32, y_fp32_host.data());

    E2E_STAGE_END();

    // Stage 3: Quantize weights and input to INT8
    E2E_STAGE_BEGIN("Quantize to INT8", 500);

    // Create calibration context for finding scale factors
    nimcp_quant_calibration_t* calib = nimcp_quant_calibration_create(ctx_);
    E2E_ASSERT_NOT_NULL(calib, "Failed to create calibration context");

    // Find optimal scale factors
    nimcp_quant_calibrate(calib, x_fp32, "input");
    nimcp_quant_calibrate(calib, w_fp32, "weight");

    float input_scale = 0.0f, weight_scale = 0.0f;
    nimcp_quant_get_scale(calib, "input", &input_scale);
    nimcp_quant_get_scale(calib, "weight", &weight_scale);

    std::cout << "\n  Input scale:  " << input_scale << std::endl;
    std::cout << "  Weight scale: " << weight_scale << std::endl;

    // Quantize tensors
    nimcp_gpu_tensor_t* x_int8 = nimcp_gpu_tensor_create(
        ctx_, x_dims, 2, NIMCP_GPU_PRECISION_INT8);
    nimcp_gpu_tensor_t* w_int8 = nimcp_gpu_tensor_create(
        ctx_, w_dims, 2, NIMCP_GPU_PRECISION_INT8);
    nimcp_gpu_tensor_t* y_int32 = nimcp_gpu_tensor_create(
        ctx_, y_dims, 2, NIMCP_GPU_PRECISION_INT32);

    bool quant_x_ok = nimcp_gpu_quantize(ctx_, x_fp32, x_int8, input_scale);
    bool quant_w_ok = nimcp_gpu_quantize(ctx_, w_fp32, w_int8, weight_scale);

    EXPECT_TRUE(quant_x_ok);
    EXPECT_TRUE(quant_w_ok);

    E2E_STAGE_END();

    // Stage 4: Run INT8 inference
    E2E_STAGE_BEGIN("Run INT8 inference", 500);

    const int num_iterations = 100;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_iterations; i++) {
        nimcp_gpu_gemm_int8(ctx_, x_int8, w_int8, y_int32);
    }
    nimcp_gpu_context_synchronize(ctx_);

    auto end = std::chrono::high_resolution_clock::now();
    double int8_time_ms = std::chrono::duration<double, std::milli>(end - start).count();

    std::cout << "  INT8 inference time: " << int8_time_ms << " ms for "
              << num_iterations << " iterations" << std::endl;

    E2E_STAGE_END();

    // Stage 5: Dequantize and compare
    E2E_STAGE_BEGIN("Dequantize and compare", 500);

    nimcp_gpu_tensor_t* y_dequant = nimcp_gpu_tensor_create(
        ctx_, y_dims, 2, NIMCP_GPU_PRECISION_FP32);

    float output_scale = input_scale * weight_scale;
    bool dequant_ok = nimcp_gpu_dequantize(ctx_, y_int32, y_dequant, output_scale);
    EXPECT_TRUE(dequant_ok);

    std::vector<float> y_int8_host(batch_size * output_dim);
    nimcp_gpu_tensor_to_host(y_dequant, y_int8_host.data());

    // Compare with FP32 reference
    float max_diff = 0.0f;
    float total_diff = 0.0f;
    for (size_t i = 0; i < batch_size * output_dim; i++) {
        float diff = std::abs(y_fp32_host[i] - y_int8_host[i]);
        max_diff = std::max(max_diff, diff);
        total_diff += diff;
    }
    float avg_diff = total_diff / (batch_size * output_dim);

    std::cout << "  Max difference:  " << max_diff << std::endl;
    std::cout << "  Avg difference:  " << avg_diff << std::endl;

    // INT8 should be within reasonable error (typical ~1% of max value)
    EXPECT_LT(avg_diff, 1.0f) << "INT8 error should be small";

    nimcp_gpu_tensor_destroy(y_dequant);

    E2E_STAGE_END();

    // Stage 6: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 300);

    nimcp_quant_calibration_destroy(calib);
    nimcp_gpu_tensor_destroy(x_fp32);
    nimcp_gpu_tensor_destroy(w_fp32);
    nimcp_gpu_tensor_destroy(y_fp32);
    nimcp_gpu_tensor_destroy(x_int8);
    nimcp_gpu_tensor_destroy(w_int8);
    nimcp_gpu_tensor_destroy(y_int32);

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 5: Inference Session with CUDA Graph Capture
//=============================================================================

TEST_F(GPUInferencePipelineE2ETest, InferenceSession) {
    SkipIfNoGPU();

    E2E_PIPELINE_START("Inference Session with CUDA Graph");

    const size_t batch_size = 32;
    const size_t input_dim = 64;
    const size_t hidden_dim = 128;
    const size_t output_dim = 10;

    // Stage 1: Create inference session
    E2E_STAGE_BEGIN("Create inference session", 500);

    nimcp_inference_session_t* session = nimcp_inference_session_create(ctx_);
    E2E_ASSERT_NOT_NULL(session, "Failed to create inference session");

    E2E_STAGE_END();

    // Stage 2: Configure model layers
    E2E_STAGE_BEGIN("Configure model layers", 500);

    size_t w1_dims[] = {input_dim, hidden_dim};
    size_t w2_dims[] = {hidden_dim, output_dim};

    nimcp_gpu_tensor_t* W1 = nimcp_gpu_tensor_create(ctx_, w1_dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* W2 = nimcp_gpu_tensor_create(ctx_, w2_dims, 2, NIMCP_GPU_PRECISION_FP32);

    // Initialize weights
    std::vector<float> w1_host(input_dim * hidden_dim);
    std::vector<float> w2_host(hidden_dim * output_dim);
    initialize_model_weights(w1_host.data(), input_dim, hidden_dim);
    initialize_model_weights(w2_host.data(), hidden_dim, output_dim);

    nimcp_gpu_tensor_t* w1_init = nimcp_gpu_tensor_from_host(ctx_, w1_host.data(), w1_dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* w2_init = nimcp_gpu_tensor_from_host(ctx_, w2_host.data(), w2_dims, 2, NIMCP_GPU_PRECISION_FP32);

    nimcp_gpu_tensor_destroy(W1);
    nimcp_gpu_tensor_destroy(W2);
    W1 = w1_init;
    W2 = w2_init;

    // Add layers to session
    nimcp_inference_session_add_linear(session, W1, nullptr);  // No bias
    nimcp_inference_session_add_activation(session, NIMCP_ACTIVATION_RELU);
    nimcp_inference_session_add_linear(session, W2, nullptr);
    nimcp_inference_session_add_activation(session, NIMCP_ACTIVATION_SOFTMAX);

    E2E_STAGE_END();

    // Stage 3: Capture CUDA graph
    E2E_STAGE_BEGIN("Capture CUDA graph", 1000);

    // Prepare input/output tensors
    size_t input_dims[] = {batch_size, input_dim};
    size_t output_dims[] = {batch_size, output_dim};

    nimcp_gpu_tensor_t* input = nimcp_gpu_tensor_create(ctx_, input_dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* output = nimcp_gpu_tensor_create(ctx_, output_dims, 2, NIMCP_GPU_PRECISION_FP32);

    // Begin graph capture
    bool capture_begin = nimcp_inference_session_begin_capture(session);
    EXPECT_TRUE(capture_begin);

    // Run inference (will be captured)
    bool infer_ok = nimcp_inference_session_run(session, input, output);
    EXPECT_TRUE(infer_ok);

    // End capture
    bool capture_end = nimcp_inference_session_end_capture(session);
    EXPECT_TRUE(capture_end);

    std::cout << "\n  CUDA graph captured successfully" << std::endl;

    E2E_STAGE_END();

    // Stage 4: Run with captured graph (fast path)
    E2E_STAGE_BEGIN("Run with captured graph", 2000);

    // Fill input with random data
    std::vector<float> input_host(batch_size * input_dim);
    generate_inference_input(input_host.data(), batch_size, input_dim);
    nimcp_gpu_tensor_t* input_init = nimcp_gpu_tensor_from_host(
        ctx_, input_host.data(), input_dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_destroy(input);
    input = input_init;

    // Warmup
    nimcp_inference_session_run_graph(session, input, output);
    nimcp_gpu_context_synchronize(ctx_);

    // Timed runs
    const int num_iterations = 1000;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_iterations; i++) {
        nimcp_inference_session_run_graph(session, input, output);
    }
    nimcp_gpu_context_synchronize(ctx_);

    auto end = std::chrono::high_resolution_clock::now();
    double graph_time_ms = std::chrono::duration<double, std::milli>(end - start).count();

    double latency_us = (graph_time_ms / num_iterations) * 1000.0;
    double throughput = (batch_size * num_iterations) / (graph_time_ms / 1000.0);

    std::cout << "  CUDA Graph Statistics:" << std::endl;
    std::cout << "    Total time: " << graph_time_ms << " ms for " << num_iterations << " runs" << std::endl;
    std::cout << "    Latency:    " << latency_us << " us/inference" << std::endl;
    std::cout << "    Throughput: " << throughput << " samples/sec" << std::endl;

    E2E_STAGE_END();

    // Stage 5: Verify output
    E2E_STAGE_BEGIN("Verify output", 500);

    std::vector<float> output_host(batch_size * output_dim);
    nimcp_gpu_tensor_to_host(output, output_host.data());

    // Check softmax output (should sum to ~1 for each sample)
    for (size_t b = 0; b < batch_size; b++) {
        float sum = 0.0f;
        for (size_t c = 0; c < output_dim; c++) {
            float v = output_host[b * output_dim + c];
            EXPECT_GE(v, 0.0f) << "Softmax output should be >= 0";
            EXPECT_LE(v, 1.0f) << "Softmax output should be <= 1";
            sum += v;
        }
        EXPECT_NEAR(sum, 1.0f, 0.01f) << "Softmax should sum to 1";
    }

    E2E_STAGE_END();

    // Stage 6: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 300);

    nimcp_gpu_tensor_destroy(input);
    nimcp_gpu_tensor_destroy(output);
    nimcp_gpu_tensor_destroy(W1);
    nimcp_gpu_tensor_destroy(W2);
    nimcp_inference_session_destroy(session);

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 6: Throughput Measurement
//=============================================================================

TEST_F(GPUInferencePipelineE2ETest, ThroughputMeasurement) {
    SkipIfNoGPU();

    E2E_PIPELINE_START("Throughput Measurement");

    const size_t input_dim = 256;
    const size_t hidden_dim = 512;
    const size_t output_dim = 100;

    // Stage 1: Create model
    E2E_STAGE_BEGIN("Create model", 500);

    size_t w1_dims[] = {input_dim, hidden_dim};
    size_t w2_dims[] = {hidden_dim, output_dim};

    std::vector<float> w1_host(input_dim * hidden_dim);
    std::vector<float> w2_host(hidden_dim * output_dim);
    initialize_model_weights(w1_host.data(), input_dim, hidden_dim);
    initialize_model_weights(w2_host.data(), hidden_dim, output_dim);

    nimcp_gpu_tensor_t* W1 = nimcp_gpu_tensor_from_host(ctx_, w1_host.data(), w1_dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* W2 = nimcp_gpu_tensor_from_host(ctx_, w2_host.data(), w2_dims, 2, NIMCP_GPU_PRECISION_FP32);

    E2E_ASSERT_NOT_NULL(W1, "Failed to create W1");
    E2E_ASSERT_NOT_NULL(W2, "Failed to create W2");

    E2E_STAGE_END();

    // Stage 2: Measure throughput at different batch sizes
    E2E_STAGE_BEGIN("Measure throughput", 10000);

    std::cout << "\n  Batch | Latency (us) | Throughput (samples/s)" << std::endl;
    std::cout << "  ------|--------------|-----------------------" << std::endl;

    std::vector<size_t> batch_sizes = {1, 4, 16, 32, 64, 128, 256};
    std::vector<double> throughputs;

    for (size_t batch_size : batch_sizes) {
        size_t input_dims[] = {batch_size, input_dim};
        size_t hidden_dims[] = {batch_size, hidden_dim};
        size_t output_dims[] = {batch_size, output_dim};

        // Create tensors
        std::vector<float> input_host(batch_size * input_dim);
        generate_inference_input(input_host.data(), batch_size, input_dim);

        nimcp_gpu_tensor_t* input = nimcp_gpu_tensor_from_host(
            ctx_, input_host.data(), input_dims, 2, NIMCP_GPU_PRECISION_FP32);
        nimcp_gpu_tensor_t* hidden = nimcp_gpu_tensor_create(
            ctx_, hidden_dims, 2, NIMCP_GPU_PRECISION_FP32);
        nimcp_gpu_tensor_t* output = nimcp_gpu_tensor_create(
            ctx_, output_dims, 2, NIMCP_GPU_PRECISION_FP32);

        // Warmup
        for (int i = 0; i < 10; i++) {
            nimcp_gpu_gemm(ctx_, input, W1, hidden, 1.0f, 0.0f, false, false);
            nimcp_gpu_relu(ctx_, hidden, hidden);
            nimcp_gpu_gemm(ctx_, hidden, W2, output, 1.0f, 0.0f, false, false);
        }
        nimcp_gpu_context_synchronize(ctx_);

        // Timed runs
        const int num_iterations = 500;
        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < num_iterations; i++) {
            nimcp_gpu_gemm(ctx_, input, W1, hidden, 1.0f, 0.0f, false, false);
            nimcp_gpu_relu(ctx_, hidden, hidden);
            nimcp_gpu_gemm(ctx_, hidden, W2, output, 1.0f, 0.0f, false, false);
        }
        nimcp_gpu_context_synchronize(ctx_);

        auto end = std::chrono::high_resolution_clock::now();
        double total_ms = std::chrono::duration<double, std::milli>(end - start).count();

        double latency_us = (total_ms / num_iterations) * 1000.0;
        double throughput = (batch_size * num_iterations) / (total_ms / 1000.0);
        throughputs.push_back(throughput);

        std::cout << "  " << std::setw(5) << batch_size << " | "
                  << std::setw(12) << std::fixed << std::setprecision(1) << latency_us << " | "
                  << std::setw(21) << std::fixed << std::setprecision(0) << throughput << std::endl;

        nimcp_gpu_tensor_destroy(input);
        nimcp_gpu_tensor_destroy(hidden);
        nimcp_gpu_tensor_destroy(output);
    }

    E2E_STAGE_END();

    // Stage 3: Report peak throughput
    E2E_STAGE_BEGIN("Report peak throughput", 200);

    double peak_throughput = *std::max_element(throughputs.begin(), throughputs.end());
    size_t peak_batch_idx = std::distance(throughputs.begin(),
        std::max_element(throughputs.begin(), throughputs.end()));

    std::cout << "\n  Peak Throughput: " << peak_throughput << " samples/s "
              << "at batch size " << batch_sizes[peak_batch_idx] << std::endl;

    E2E_STAGE_END();

    // Stage 4: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 300);

    nimcp_gpu_tensor_destroy(W1);
    nimcp_gpu_tensor_destroy(W2);

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 7: Dynamic Batching
//=============================================================================

TEST_F(GPUInferencePipelineE2ETest, DynamicBatching) {
    SkipIfNoGPU();

    E2E_PIPELINE_START("Dynamic Batching");

    const size_t input_dim = 32;
    const size_t output_dim = 16;
    const size_t max_batch_size = 64;

    // Stage 1: Create model weights
    E2E_STAGE_BEGIN("Create model weights", 500);

    size_t w_dims[] = {input_dim, output_dim};

    std::vector<float> w_host(input_dim * output_dim);
    initialize_model_weights(w_host.data(), input_dim, output_dim);

    nimcp_gpu_tensor_t* W = nimcp_gpu_tensor_from_host(
        ctx_, w_host.data(), w_dims, 2, NIMCP_GPU_PRECISION_FP32);
    E2E_ASSERT_NOT_NULL(W, "Failed to create weight tensor");

    E2E_STAGE_END();

    // Stage 2: Create dynamic batcher
    E2E_STAGE_BEGIN("Create dynamic batcher", 500);

    nimcp_dynamic_batcher_t* batcher = nimcp_dynamic_batcher_create(
        ctx_, max_batch_size, input_dim, 10 /* timeout_ms */);
    E2E_ASSERT_NOT_NULL(batcher, "Failed to create dynamic batcher");

    E2E_STAGE_END();

    // Stage 3: Process requests with varying batch sizes
    E2E_STAGE_BEGIN("Process dynamic batches", 3000);

    std::vector<size_t> request_sizes = {1, 3, 7, 12, 5, 20, 8, 2, 15, 4};
    size_t total_samples = 0;

    for (size_t req_size : request_sizes) {
        // Generate request data
        std::vector<float> req_data(req_size * input_dim);
        generate_inference_input(req_data.data(), req_size, input_dim);

        // Add to batcher
        bool added = nimcp_dynamic_batcher_add_request(
            batcher, req_data.data(), req_size);
        EXPECT_TRUE(added);

        total_samples += req_size;
    }

    // Flush remaining requests
    bool flush_ok = nimcp_dynamic_batcher_flush(batcher);
    EXPECT_TRUE(flush_ok);

    std::cout << "\n  Processed " << total_samples << " samples in "
              << request_sizes.size() << " requests" << std::endl;

    E2E_STAGE_END();

    // Stage 4: Run batched inference
    E2E_STAGE_BEGIN("Run batched inference", 1000);

    size_t num_batches = 0;
    while (nimcp_dynamic_batcher_has_batch(batcher)) {
        size_t batch_size = 0;
        nimcp_gpu_tensor_t* batch_input = nullptr;
        nimcp_gpu_tensor_t* batch_output = nullptr;

        // Get next batch
        bool got_batch = nimcp_dynamic_batcher_get_batch(
            batcher, &batch_input, &batch_size);
        EXPECT_TRUE(got_batch);

        // Create output tensor
        size_t output_dims[] = {batch_size, output_dim};
        batch_output = nimcp_gpu_tensor_create(
            ctx_, output_dims, 2, NIMCP_GPU_PRECISION_FP32);

        // Run inference
        nimcp_gpu_gemm(ctx_, batch_input, W, batch_output, 1.0f, 0.0f, false, false);

        // Return results (in real system would dispatch to requesters)
        nimcp_dynamic_batcher_return_results(batcher, batch_output);

        nimcp_gpu_tensor_destroy(batch_output);
        num_batches++;
    }

    std::cout << "  Created " << num_batches << " batches" << std::endl;

    E2E_STAGE_END();

    // Stage 5: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 300);

    nimcp_dynamic_batcher_destroy(batcher);
    nimcp_gpu_tensor_destroy(W);

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 8: Multi-Stream Inference
//=============================================================================

TEST_F(GPUInferencePipelineE2ETest, MultiStreamInference) {
    SkipIfNoGPU();

    E2E_PIPELINE_START("Multi-Stream Inference");

    const size_t batch_size = 16;
    const size_t input_dim = 64;
    const size_t output_dim = 32;
    const size_t num_streams = 4;

    // Stage 1: Create multiple streams
    E2E_STAGE_BEGIN("Create GPU streams", 500);

    std::vector<nimcp_gpu_stream_t*> streams(num_streams);
    for (size_t i = 0; i < num_streams; i++) {
        streams[i] = nimcp_gpu_stream_create(ctx_);
        E2E_ASSERT_NOT_NULL(streams[i], "Failed to create stream");
    }

    E2E_STAGE_END();

    // Stage 2: Create model weights (shared across streams)
    E2E_STAGE_BEGIN("Create shared model weights", 500);

    size_t w_dims[] = {input_dim, output_dim};

    std::vector<float> w_host(input_dim * output_dim);
    initialize_model_weights(w_host.data(), input_dim, output_dim);

    nimcp_gpu_tensor_t* W = nimcp_gpu_tensor_from_host(
        ctx_, w_host.data(), w_dims, 2, NIMCP_GPU_PRECISION_FP32);
    E2E_ASSERT_NOT_NULL(W, "Failed to create weight tensor");

    E2E_STAGE_END();

    // Stage 3: Create input/output tensors for each stream
    E2E_STAGE_BEGIN("Create per-stream tensors", 1000);

    std::vector<nimcp_gpu_tensor_t*> inputs(num_streams);
    std::vector<nimcp_gpu_tensor_t*> outputs(num_streams);

    size_t input_dims[] = {batch_size, input_dim};
    size_t output_dims[] = {batch_size, output_dim};

    for (size_t i = 0; i < num_streams; i++) {
        std::vector<float> input_host(batch_size * input_dim);
        generate_inference_input(input_host.data(), batch_size, input_dim);

        inputs[i] = nimcp_gpu_tensor_from_host(
            ctx_, input_host.data(), input_dims, 2, NIMCP_GPU_PRECISION_FP32);
        outputs[i] = nimcp_gpu_tensor_create(
            ctx_, output_dims, 2, NIMCP_GPU_PRECISION_FP32);

        EXPECT_NE(inputs[i], nullptr);
        EXPECT_NE(outputs[i], nullptr);
    }

    E2E_STAGE_END();

    // Stage 4: Run parallel inference
    E2E_STAGE_BEGIN("Run parallel inference", 2000);

    const int num_iterations = 100;
    auto start = std::chrono::high_resolution_clock::now();

    for (int iter = 0; iter < num_iterations; iter++) {
        // Launch inference on all streams
        for (size_t s = 0; s < num_streams; s++) {
            nimcp_gpu_gemm_async(ctx_, inputs[s], W, outputs[s],
                                 1.0f, 0.0f, false, false, streams[s]);
        }
    }

    // Synchronize all streams
    for (size_t s = 0; s < num_streams; s++) {
        nimcp_gpu_stream_synchronize(streams[s]);
    }

    auto end = std::chrono::high_resolution_clock::now();
    double total_ms = std::chrono::duration<double, std::milli>(end - start).count();

    size_t total_samples = batch_size * num_streams * num_iterations;
    double throughput = total_samples / (total_ms / 1000.0);

    std::cout << "\n  Multi-Stream Performance:" << std::endl;
    std::cout << "    Streams:     " << num_streams << std::endl;
    std::cout << "    Total time:  " << total_ms << " ms" << std::endl;
    std::cout << "    Throughput:  " << throughput << " samples/s" << std::endl;

    E2E_STAGE_END();

    // Stage 5: Verify all outputs
    E2E_STAGE_BEGIN("Verify outputs", 500);

    for (size_t s = 0; s < num_streams; s++) {
        std::vector<float> output_host(batch_size * output_dim);
        nimcp_gpu_tensor_to_host(outputs[s], output_host.data());

        bool valid = true;
        for (const auto& v : output_host) {
            if (std::isnan(v) || std::isinf(v)) {
                valid = false;
                break;
            }
        }
        EXPECT_TRUE(valid) << "Stream " << s << " output contains NaN/Inf";
    }

    E2E_STAGE_END();

    // Stage 6: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 500);

    for (size_t i = 0; i < num_streams; i++) {
        nimcp_gpu_tensor_destroy(inputs[i]);
        nimcp_gpu_tensor_destroy(outputs[i]);
        nimcp_gpu_stream_destroy(streams[i]);
    }
    nimcp_gpu_tensor_destroy(W);

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
