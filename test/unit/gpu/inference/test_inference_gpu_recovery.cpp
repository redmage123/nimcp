/* ============================================================================
 * Unit Tests: GPU Inference Recovery System
 * ============================================================================
 * WHAT: Unit tests for GPU inference operations with self-healing recovery
 * WHY:  Validate recovery from OOM, quantization errors, and CPU fallback
 * HOW:  Test recovery mechanisms for inference kernels and INT8 operations
 * ============================================================================
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <random>

#ifdef NIMCP_ENABLE_CUDA
#include "gpu/inference/nimcp_inference_gpu.h"
#include "gpu/inference/nimcp_int8_inference.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#endif

namespace {

constexpr float TOLERANCE = 1e-4f;
constexpr float RELAXED_TOLERANCE = 1e-3f;

/* ============================================================================
 * Test Fixture: Inference GPU Recovery
 * ============================================================================ */
class InferenceGPURecoveryTest : public ::testing::Test {
protected:
    void SetUp() override {
#ifdef NIMCP_ENABLE_CUDA
        // Initialize recovery system
        nimcp_gpu_recovery_config_t config;
        nimcp_gpu_recovery_default_config(&config);
        config.enable_cpu_fallback = true;
        config.enable_param_correction = true;
        config.enable_batch_reduction = true;
        config.max_retries = 3;
        nimcp_gpu_recovery_init(&config);

        // Create GPU context
        ctx_ = nimcp_gpu_context_create(0);
#endif
    }

    void TearDown() override {
#ifdef NIMCP_ENABLE_CUDA
        if (ctx_) {
            nimcp_gpu_context_destroy(ctx_);
            ctx_ = nullptr;
        }
        nimcp_gpu_recovery_shutdown();
#endif
    }

#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_context_t* ctx_ = nullptr;

    // Helper: Create test GPU tensor
    nimcp_gpu_tensor_t* create_test_tensor(const std::vector<size_t>& dims) {
        return nimcp_gpu_tensor_create(ctx_, dims.data(), dims.size(), NIMCP_GPU_PRECISION_FP32);
    }

    // Helper: Fill tensor with random data
    void fill_random(nimcp_gpu_tensor_t* tensor, float min_val = -1.0f, float max_val = 1.0f) {
        if (!tensor) return;
        size_t numel = nimcp_gpu_tensor_numel(tensor);
        std::vector<float> host_data(numel);
        std::mt19937 gen(42);
        std::uniform_real_distribution<float> dist(min_val, max_val);
        for (auto& x : host_data) x = dist(gen);
        nimcp_gpu_tensor_upload(tensor, host_data.data(), numel * sizeof(float));
    }

    // Helper: Create and fill INT8 quantization parameters
    void init_quant_params(nimcp_int8_quant_params_t* params, float scale = 0.01f, int32_t zp = 0) {
        nimcp_int8_params_init(params);
        params->scale = scale;
        params->zero_point = zp;
        params->min_val = -128.0f * scale;
        params->max_val = 127.0f * scale;
        params->symmetric = true;
    }
#endif
};

/* ============================================================================
 * Test: Recovery System Initialization
 * ============================================================================ */
TEST_F(InferenceGPURecoveryTest, RecoveryInitialization) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_TRUE(nimcp_gpu_recovery_is_initialized())
        << "Recovery system should be initialized";

    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);
    EXPECT_EQ(stats.total_errors, 0u)
        << "No errors should be recorded initially";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Invalid Parameter Recovery for Linear Layer
 * ============================================================================ */
TEST_F(InferenceGPURecoveryTest, InvalidParamRecovery_LinearLayer) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    // Test with NULL parameters - should be handled gracefully
    bool result = nimcp_gpu_infer_linear_relu(ctx_, nullptr, nullptr, nullptr, nullptr);
    EXPECT_FALSE(result) << "Should fail with NULL parameters";

    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);
    EXPECT_GT(stats.total_errors, 0u)
        << "Error should be recorded for invalid params";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Valid Linear+ReLU Inference
 * ============================================================================ */
TEST_F(InferenceGPURecoveryTest, ValidLinearReLU) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    const size_t batch = 16;
    const size_t in_features = 64;
    const size_t out_features = 32;

    // Create tensors
    nimcp_gpu_tensor_t* input = create_test_tensor({batch, in_features});
    nimcp_gpu_tensor_t* weights = create_test_tensor({out_features, in_features});
    nimcp_gpu_tensor_t* bias = create_test_tensor({out_features});
    nimcp_gpu_tensor_t* output = create_test_tensor({batch, out_features});

    ASSERT_NE(input, nullptr);
    ASSERT_NE(weights, nullptr);
    ASSERT_NE(bias, nullptr);
    ASSERT_NE(output, nullptr);

    fill_random(input);
    fill_random(weights, -0.1f, 0.1f);
    fill_random(bias, -0.01f, 0.01f);

    bool result = nimcp_gpu_infer_linear_relu(ctx_, input, weights, bias, output);
    EXPECT_TRUE(result) << "Linear+ReLU should succeed with valid inputs";

    // Cleanup
    nimcp_gpu_tensor_destroy(input);
    nimcp_gpu_tensor_destroy(weights);
    nimcp_gpu_tensor_destroy(bias);
    nimcp_gpu_tensor_destroy(output);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: INT8 Quantization Parameter Initialization
 * ============================================================================ */
TEST_F(InferenceGPURecoveryTest, INT8QuantParamsInit) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_int8_quant_params_t params;
    int ret = nimcp_int8_params_init(&params);
    EXPECT_EQ(ret, 0) << "Parameter init should succeed";
    EXPECT_GT(params.scale, 0.0f) << "Scale should be positive";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: INT8 Quantization with Valid Data
 * ============================================================================ */
TEST_F(InferenceGPURecoveryTest, INT8QuantizeValid) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    const size_t numel = 1024;

    // Create host data
    std::vector<float> host_input(numel);
    std::mt19937 gen(42);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    for (auto& x : host_input) x = dist(gen);

    // Allocate GPU memory
    float* d_input = nullptr;
    int8_t* d_output = nullptr;
    cudaMalloc(&d_input, numel * sizeof(float));
    cudaMalloc(&d_output, numel * sizeof(int8_t));
    cudaMemcpy(d_input, host_input.data(), numel * sizeof(float), cudaMemcpyHostToDevice);

    // Set up quantization parameters
    nimcp_int8_quant_params_t params;
    init_quant_params(&params, 1.0f / 127.0f, 0);

    int ret = nimcp_int8_quantize(ctx_, d_input, d_output, numel, &params);
    EXPECT_EQ(ret, 0) << "INT8 quantization should succeed";

    // Verify output
    std::vector<int8_t> host_output(numel);
    cudaMemcpy(host_output.data(), d_output, numel * sizeof(int8_t), cudaMemcpyDeviceToHost);

    // Check a few values
    for (size_t i = 0; i < 10; i++) {
        int8_t expected = (int8_t)std::max(-128.0f, std::min(127.0f,
            std::round(host_input[i] / params.scale + params.zero_point)));
        EXPECT_NEAR(host_output[i], expected, 1)
            << "Quantized value mismatch at index " << i;
    }

    cudaFree(d_input);
    cudaFree(d_output);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: INT8 Dequantization Roundtrip
 * ============================================================================ */
TEST_F(InferenceGPURecoveryTest, INT8DequantizeRoundtrip) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    const size_t numel = 512;

    // Create host data in quantizable range
    std::vector<float> host_input(numel);
    std::vector<float> host_output(numel);
    std::mt19937 gen(42);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    for (auto& x : host_input) x = dist(gen);

    // Allocate GPU memory
    float* d_input = nullptr;
    int8_t* d_quant = nullptr;
    float* d_output = nullptr;
    cudaMalloc(&d_input, numel * sizeof(float));
    cudaMalloc(&d_quant, numel * sizeof(int8_t));
    cudaMalloc(&d_output, numel * sizeof(float));
    cudaMemcpy(d_input, host_input.data(), numel * sizeof(float), cudaMemcpyHostToDevice);

    nimcp_int8_quant_params_t params;
    init_quant_params(&params, 1.0f / 127.0f, 0);

    // Quantize
    int ret = nimcp_int8_quantize(ctx_, d_input, d_quant, numel, &params);
    EXPECT_EQ(ret, 0) << "Quantization should succeed";

    // Dequantize
    ret = nimcp_int8_dequantize(ctx_, d_quant, d_output, numel, &params);
    EXPECT_EQ(ret, 0) << "Dequantization should succeed";

    // Copy back and verify
    cudaMemcpy(host_output.data(), d_output, numel * sizeof(float), cudaMemcpyDeviceToHost);

    // Check quantization error is bounded
    float max_error = 0.0f;
    for (size_t i = 0; i < numel; i++) {
        float error = std::abs(host_output[i] - host_input[i]);
        max_error = std::max(max_error, error);
    }
    EXPECT_LT(max_error, params.scale * 1.5f)
        << "Roundtrip error should be bounded by quantization step";

    cudaFree(d_input);
    cudaFree(d_quant);
    cudaFree(d_output);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: INT8 Quantization with Invalid Parameters
 * ============================================================================ */
TEST_F(InferenceGPURecoveryTest, INT8QuantizeInvalidParams) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    // Test with NULL parameters
    int ret = nimcp_int8_quantize(ctx_, nullptr, nullptr, 100, nullptr);
    EXPECT_NE(ret, 0) << "Should fail with NULL parameters";

    // Error should be recorded
    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);
    EXPECT_GT(stats.total_errors, 0u)
        << "Error should be recorded for invalid quantization params";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: INT8 GEMM Operation
 * ============================================================================ */
TEST_F(InferenceGPURecoveryTest, INT8GEMMValid) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    const int M = 16, N = 32, K = 64;

    // Allocate INT8 matrices
    int8_t* d_A = nullptr;
    int8_t* d_B = nullptr;
    int32_t* d_C = nullptr;
    cudaMalloc(&d_A, M * K * sizeof(int8_t));
    cudaMalloc(&d_B, K * N * sizeof(int8_t));
    cudaMalloc(&d_C, M * N * sizeof(int32_t));

    // Fill with simple test data
    std::vector<int8_t> h_A(M * K, 1);
    std::vector<int8_t> h_B(K * N, 1);
    cudaMemcpy(d_A, h_A.data(), M * K * sizeof(int8_t), cudaMemcpyHostToDevice);
    cudaMemcpy(d_B, h_B.data(), K * N * sizeof(int8_t), cudaMemcpyHostToDevice);

    nimcp_int8_quant_params_t params_a, params_b;
    init_quant_params(&params_a);
    init_quant_params(&params_b);

    int ret = nimcp_int8_gemm(ctx_, d_A, d_B, d_C, M, N, K, &params_a, &params_b);
    EXPECT_EQ(ret, 0) << "INT8 GEMM should succeed";

    // Verify output: C[i,j] = sum(A[i,:] * B[:,j]) = K * 1 * 1 = K
    std::vector<int32_t> h_C(M * N);
    cudaMemcpy(h_C.data(), d_C, M * N * sizeof(int32_t), cudaMemcpyDeviceToHost);

    for (int i = 0; i < M * N; i++) {
        EXPECT_EQ(h_C[i], K) << "GEMM result mismatch at index " << i;
    }

    cudaFree(d_A);
    cudaFree(d_B);
    cudaFree(d_C);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: INT8 ReLU Activation
 * ============================================================================ */
TEST_F(InferenceGPURecoveryTest, INT8ReLU) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    const size_t numel = 256;

    // Create test data with positive and negative values
    std::vector<int8_t> h_input(numel);
    for (size_t i = 0; i < numel; i++) {
        h_input[i] = static_cast<int8_t>(i % 256 - 128);  // -128 to 127
    }

    int8_t* d_input = nullptr;
    int8_t* d_output = nullptr;
    cudaMalloc(&d_input, numel * sizeof(int8_t));
    cudaMalloc(&d_output, numel * sizeof(int8_t));
    cudaMemcpy(d_input, h_input.data(), numel * sizeof(int8_t), cudaMemcpyHostToDevice);

    int32_t zero_point = 0;  // Symmetric quantization
    int ret = nimcp_int8_relu(ctx_, d_input, d_output, numel, zero_point);
    EXPECT_EQ(ret, 0) << "INT8 ReLU should succeed";

    // Verify: output = max(input, zero_point)
    std::vector<int8_t> h_output(numel);
    cudaMemcpy(h_output.data(), d_output, numel * sizeof(int8_t), cudaMemcpyDeviceToHost);

    for (size_t i = 0; i < numel; i++) {
        int8_t expected = std::max(h_input[i], (int8_t)zero_point);
        EXPECT_EQ(h_output[i], expected)
            << "ReLU mismatch at index " << i;
    }

    cudaFree(d_input);
    cudaFree(d_output);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Recovery Statistics Tracking
 * ============================================================================ */
TEST_F(InferenceGPURecoveryTest, RecoveryStatsTracking) {
#ifdef NIMCP_ENABLE_CUDA
    // Reset stats
    nimcp_gpu_recovery_reset_stats();

    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);
    EXPECT_EQ(stats.total_errors, 0u);
    EXPECT_EQ(stats.recoveries_attempted, 0u);

    // Trigger some invalid operations to generate stats
    nimcp_int8_quantize(ctx_, nullptr, nullptr, 100, nullptr);
    nimcp_int8_dequantize(ctx_, nullptr, nullptr, 100, nullptr);

    nimcp_gpu_recovery_get_stats(&stats);
    EXPECT_GT(stats.total_errors, 0u)
        << "Errors should be recorded";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Calibrator Creation and Destruction
 * ============================================================================ */
TEST_F(InferenceGPURecoveryTest, CalibratorLifecycle) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    nimcp_int8_calibrator_t* cal = nimcp_int8_calibrator_create(
        ctx_,
        INT8_CALIB_MINMAX,
        false,  // per_channel
        1,      // num_channels
        1024    // num_bins
    );
    ASSERT_NE(cal, nullptr) << "Calibrator creation should succeed";

    // Reset calibrator
    nimcp_int8_calibrator_reset(cal);

    nimcp_int8_calibrator_destroy(cal);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Calibrator with Invalid Parameters
 * ============================================================================ */
TEST_F(InferenceGPURecoveryTest, CalibratorInvalidParams) {
#ifdef NIMCP_ENABLE_CUDA
    // NULL context
    nimcp_int8_calibrator_t* cal = nimcp_int8_calibrator_create(
        nullptr,
        INT8_CALIB_MINMAX,
        false, 1, 1024
    );
    EXPECT_EQ(cal, nullptr) << "Should fail with NULL context";

    // Invalid number of channels
    cal = nimcp_int8_calibrator_create(
        ctx_,
        INT8_CALIB_MINMAX,
        true,   // per_channel
        0,      // invalid num_channels
        1024
    );
    EXPECT_EQ(cal, nullptr) << "Should fail with zero channels";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: INT8 Model Creation
 * ============================================================================ */
TEST_F(InferenceGPURecoveryTest, INT8ModelLifecycle) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    nimcp_int8_model_t* model = nimcp_int8_model_create(ctx_, 4, "test_model");
    ASSERT_NE(model, nullptr) << "Model creation should succeed";

    nimcp_int8_model_destroy(model);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: INT8 Tensor Creation and Destruction
 * ============================================================================ */
TEST_F(InferenceGPURecoveryTest, INT8TensorLifecycle) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    size_t dims[] = {32, 64};
    nimcp_int8_tensor_t* tensor = nimcp_int8_tensor_create(ctx_, dims, 2);
    ASSERT_NE(tensor, nullptr) << "INT8 tensor creation should succeed";

    EXPECT_EQ(tensor->rank, 2u);
    EXPECT_EQ(tensor->numel, 32u * 64u);

    nimcp_int8_tensor_destroy(tensor);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Memory Information
 * ============================================================================ */
TEST_F(InferenceGPURecoveryTest, MemoryInfo) {
#ifdef NIMCP_ENABLE_CUDA
    size_t free_bytes = 0, total_bytes = 0;
    bool result = nimcp_gpu_get_memory_info(&free_bytes, &total_bytes);
    EXPECT_TRUE(result) << "Memory info should be available";
    EXPECT_GT(total_bytes, 0u) << "Total memory should be positive";
    EXPECT_LE(free_bytes, total_bytes) << "Free should be <= total";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: CPU Fallback Availability Check
 * ============================================================================ */
TEST_F(InferenceGPURecoveryTest, CPUFallbackAvailable) {
#ifdef NIMCP_ENABLE_CUDA
    bool available = nimcp_gpu_cpu_fallback_available();
    // Just verify the function doesn't crash - result depends on config
    (void)available;
    SUCCEED();
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Error Category Names
 * ============================================================================ */
TEST_F(InferenceGPURecoveryTest, ErrorCategoryNames) {
#ifdef NIMCP_ENABLE_CUDA
    const char* name;

    name = nimcp_gpu_error_category_name(GPU_ERROR_OUT_OF_MEMORY);
    EXPECT_NE(name, nullptr);
    EXPECT_STRNE(name, "");

    name = nimcp_gpu_error_category_name(GPU_ERROR_INVALID_PARAMS);
    EXPECT_NE(name, nullptr);

    name = nimcp_gpu_error_category_name(GPU_ERROR_NUMERICAL);
    EXPECT_NE(name, nullptr);

    name = nimcp_gpu_error_category_name(GPU_ERROR_KERNEL_LAUNCH);
    EXPECT_NE(name, nullptr);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Recovery Action Names
 * ============================================================================ */
TEST_F(InferenceGPURecoveryTest, RecoveryActionNames) {
#ifdef NIMCP_ENABLE_CUDA
    const char* name;

    name = nimcp_gpu_recovery_action_name(GPU_RECOVERY_CPU_FALLBACK);
    EXPECT_NE(name, nullptr);
    EXPECT_STRNE(name, "");

    name = nimcp_gpu_recovery_action_name(GPU_RECOVERY_REDUCE_BATCH);
    EXPECT_NE(name, nullptr);

    name = nimcp_gpu_recovery_action_name(GPU_RECOVERY_CLAMP_PARAMS);
    EXPECT_NE(name, nullptr);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: LayerNorm Inference
 * ============================================================================ */
TEST_F(InferenceGPURecoveryTest, LayerNormInference) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    const size_t batch = 8;
    const size_t features = 64;

    nimcp_gpu_tensor_t* input = create_test_tensor({batch, features});
    nimcp_gpu_tensor_t* gamma = create_test_tensor({features});
    nimcp_gpu_tensor_t* beta = create_test_tensor({features});
    nimcp_gpu_tensor_t* output = create_test_tensor({batch, features});

    ASSERT_NE(input, nullptr);
    ASSERT_NE(gamma, nullptr);
    ASSERT_NE(beta, nullptr);
    ASSERT_NE(output, nullptr);

    fill_random(input);

    // Fill gamma with 1s and beta with 0s (identity transform)
    std::vector<float> ones(features, 1.0f);
    std::vector<float> zeros(features, 0.0f);
    nimcp_gpu_tensor_upload(gamma, ones.data(), features * sizeof(float));
    nimcp_gpu_tensor_upload(beta, zeros.data(), features * sizeof(float));

    bool result = nimcp_gpu_infer_layernorm(ctx_, input, gamma, beta, output, 1e-5f);
    EXPECT_TRUE(result) << "LayerNorm should succeed";

    nimcp_gpu_tensor_destroy(input);
    nimcp_gpu_tensor_destroy(gamma);
    nimcp_gpu_tensor_destroy(beta);
    nimcp_gpu_tensor_destroy(output);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: RMSNorm Inference
 * ============================================================================ */
TEST_F(InferenceGPURecoveryTest, RMSNormInference) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    const size_t batch = 8;
    const size_t features = 64;

    nimcp_gpu_tensor_t* input = create_test_tensor({batch, features});
    nimcp_gpu_tensor_t* gamma = create_test_tensor({features});
    nimcp_gpu_tensor_t* output = create_test_tensor({batch, features});

    ASSERT_NE(input, nullptr);
    ASSERT_NE(gamma, nullptr);
    ASSERT_NE(output, nullptr);

    fill_random(input);

    std::vector<float> ones(features, 1.0f);
    nimcp_gpu_tensor_upload(gamma, ones.data(), features * sizeof(float));

    bool result = nimcp_gpu_infer_rmsnorm(ctx_, input, gamma, output, 1e-5f);
    EXPECT_TRUE(result) << "RMSNorm should succeed";

    nimcp_gpu_tensor_destroy(input);
    nimcp_gpu_tensor_destroy(gamma);
    nimcp_gpu_tensor_destroy(output);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Inference Session Lifecycle
 * ============================================================================ */
TEST_F(InferenceGPURecoveryTest, InferenceSessionLifecycle) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    nimcp_infer_session_t* session = nimcp_infer_session_create(
        ctx_,
        NIMCP_INFER_FP32,
        0  // auto workspace
    );
    ASSERT_NE(session, nullptr) << "Session creation should succeed";

    EXPECT_EQ(session->precision, NIMCP_INFER_FP32);
    EXPECT_FALSE(session->graph_captured);

    nimcp_infer_session_destroy(session);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: INT8 Tensor Cores Availability Check
 * ============================================================================ */
TEST_F(InferenceGPURecoveryTest, INT8TensorCoresCheck) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    bool available = nimcp_int8_tensor_cores_available(ctx_);
    // Just check it doesn't crash - availability depends on GPU
    (void)available;
    SUCCEED();
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: INT8 Recommended Settings
 * ============================================================================ */
TEST_F(InferenceGPURecoveryTest, INT8RecommendedSettings) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    nimcp_int8_scheme_t scheme;
    nimcp_int8_granularity_t granularity;

    int ret = nimcp_int8_get_recommended_settings(ctx_, &scheme, &granularity);
    EXPECT_EQ(ret, 0) << "Getting recommended settings should succeed";
    EXPECT_LT((int)scheme, INT8_SCHEME_COUNT);
    EXPECT_LT((int)granularity, INT8_GRANULARITY_COUNT);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Utility String Functions
 * ============================================================================ */
TEST_F(InferenceGPURecoveryTest, UtilityStringFunctions) {
#ifdef NIMCP_ENABLE_CUDA
    // Test scheme names
    EXPECT_STREQ(nimcp_int8_scheme_name(INT8_SCHEME_SYMMETRIC), "symmetric");
    EXPECT_STREQ(nimcp_int8_scheme_name(INT8_SCHEME_ASYMMETRIC), "asymmetric");

    // Test calibration method names
    EXPECT_STREQ(nimcp_int8_calib_method_name(INT8_CALIB_MINMAX), "minmax");
    EXPECT_STREQ(nimcp_int8_calib_method_name(INT8_CALIB_ENTROPY), "entropy");
    EXPECT_STREQ(nimcp_int8_calib_method_name(INT8_CALIB_PERCENTILE), "percentile");

    // Test mode names
    EXPECT_STREQ(nimcp_int8_mode_name(INT8_QUANT_MODE_DYNAMIC), "dynamic");
    EXPECT_STREQ(nimcp_int8_mode_name(INT8_QUANT_MODE_STATIC), "static");
    EXPECT_STREQ(nimcp_int8_mode_name(INT8_QUANT_MODE_QAT), "qat");
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Memory Savings Calculation
 * ============================================================================ */
TEST_F(InferenceGPURecoveryTest, MemorySavingsCalculation) {
#ifdef NIMCP_ENABLE_CUDA
    size_t fp32_size = 1024 * 1024;  // 1MB in FP32
    size_t int8_size = nimcp_int8_memory_savings(fp32_size);

    // INT8 should be ~4x smaller
    EXPECT_LT(int8_size, fp32_size);
    EXPECT_GE(int8_size, fp32_size / 4 - 1024);
    EXPECT_LE(int8_size, fp32_size / 4 + 1024);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

}  // namespace
