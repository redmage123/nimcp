/* ============================================================================
 * Integration Tests: GPU Inference with Recovery
 * ============================================================================
 * WHAT: Integration tests for end-to-end inference pipelines with recovery
 * WHY:  Validate complete inference workflows handle errors gracefully
 * HOW:  Test multi-layer inference, calibration, and export with recovery
 * ============================================================================
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <random>
#include <chrono>

#ifdef NIMCP_ENABLE_CUDA
#include "gpu/inference/nimcp_inference_gpu.h"
#include "gpu/inference/nimcp_int8_inference.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#endif

#ifdef NIMCP_ENABLE_TENSORRT
#include "gpu/inference/nimcp_tensorrt_export.h"
#endif

namespace {

constexpr float TOLERANCE = 1e-4f;
constexpr float RELAXED_TOLERANCE = 1e-3f;

/* ============================================================================
 * Test Fixture: Inference Integration with Recovery
 * ============================================================================ */
class InferenceIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
#ifdef NIMCP_ENABLE_CUDA
        // Initialize recovery system with aggressive settings for testing
        nimcp_gpu_recovery_config_t config;
        nimcp_gpu_recovery_default_config(&config);
        config.enable_cpu_fallback = true;
        config.enable_param_correction = true;
        config.enable_batch_reduction = true;
        config.max_retries = 3;
        config.batch_reduction_factor = 0.5f;
        nimcp_gpu_recovery_init(&config);

        ctx_ = nimcp_gpu_context_create(0);
        nimcp_gpu_recovery_reset_stats();
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
    nimcp_gpu_tensor_t* create_tensor(const std::vector<size_t>& dims) {
        return nimcp_gpu_tensor_create(ctx_, dims.data(), dims.size(), NIMCP_GPU_PRECISION_FP32);
    }

    // Helper: Fill tensor with random data
    void fill_random(nimcp_gpu_tensor_t* tensor, float min_val = -1.0f, float max_val = 1.0f) {
        if (!tensor) return;
        size_t numel = nimcp_gpu_tensor_numel(tensor);
        std::vector<float> data(numel);
        std::mt19937 gen(42);
        std::uniform_real_distribution<float> dist(min_val, max_val);
        for (auto& x : data) x = dist(gen);
        nimcp_gpu_tensor_upload(tensor, data.data(), numel * sizeof(float));
    }

    // Helper: Initialize quant params
    void init_quant_params(nimcp_int8_quant_params_t* params, float scale = 0.01f) {
        nimcp_int8_params_init(params);
        params->scale = scale;
        params->zero_point = 0;
        params->min_val = -128.0f * scale;
        params->max_val = 127.0f * scale;
        params->symmetric = true;
    }
#endif
};

/* ============================================================================
 * Test: Multi-Layer Inference Pipeline
 * ============================================================================ */
TEST_F(InferenceIntegrationTest, MultiLayerInferencePipeline) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    // Define a 3-layer MLP: 128 -> 64 -> 32 -> 16
    const size_t batch = 32;
    const size_t layers[] = {128, 64, 32, 16};

    // Create tensors for all layers
    nimcp_gpu_tensor_t* input = create_tensor({batch, layers[0]});
    nimcp_gpu_tensor_t* hidden1 = create_tensor({batch, layers[1]});
    nimcp_gpu_tensor_t* hidden2 = create_tensor({batch, layers[2]});
    nimcp_gpu_tensor_t* output = create_tensor({batch, layers[3]});

    // Create weights
    nimcp_gpu_tensor_t* w1 = create_tensor({layers[1], layers[0]});
    nimcp_gpu_tensor_t* w2 = create_tensor({layers[2], layers[1]});
    nimcp_gpu_tensor_t* w3 = create_tensor({layers[3], layers[2]});

    // Create biases
    nimcp_gpu_tensor_t* b1 = create_tensor({layers[1]});
    nimcp_gpu_tensor_t* b2 = create_tensor({layers[2]});
    nimcp_gpu_tensor_t* b3 = create_tensor({layers[3]});

    ASSERT_NE(input, nullptr);
    ASSERT_NE(w1, nullptr);

    // Initialize with random data
    fill_random(input);
    fill_random(w1, -0.1f, 0.1f);
    fill_random(w2, -0.1f, 0.1f);
    fill_random(w3, -0.1f, 0.1f);
    fill_random(b1, -0.01f, 0.01f);
    fill_random(b2, -0.01f, 0.01f);
    fill_random(b3, -0.01f, 0.01f);

    // Forward pass through all layers
    EXPECT_TRUE(nimcp_gpu_infer_linear_relu(ctx_, input, w1, b1, hidden1))
        << "Layer 1 should succeed";
    EXPECT_TRUE(nimcp_gpu_infer_linear_relu(ctx_, hidden1, w2, b2, hidden2))
        << "Layer 2 should succeed";
    EXPECT_TRUE(nimcp_gpu_infer_linear_relu(ctx_, hidden2, w3, b3, output))
        << "Layer 3 should succeed";

    // Verify recovery stats
    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);
    // Should have no errors in successful run
    EXPECT_EQ(stats.recoveries_attempted, 0u)
        << "No recovery should be needed for valid operations";

    // Cleanup
    nimcp_gpu_tensor_destroy(input);
    nimcp_gpu_tensor_destroy(hidden1);
    nimcp_gpu_tensor_destroy(hidden2);
    nimcp_gpu_tensor_destroy(output);
    nimcp_gpu_tensor_destroy(w1);
    nimcp_gpu_tensor_destroy(w2);
    nimcp_gpu_tensor_destroy(w3);
    nimcp_gpu_tensor_destroy(b1);
    nimcp_gpu_tensor_destroy(b2);
    nimcp_gpu_tensor_destroy(b3);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: INT8 Calibration Pipeline
 * ============================================================================ */
TEST_F(InferenceIntegrationTest, INT8CalibrationPipeline) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    // Create calibrator
    nimcp_int8_calibrator_t* cal = nimcp_int8_calibrator_create(
        ctx_,
        INT8_CALIB_MINMAX,
        false,  // per-tensor
        1,
        1024
    );
    ASSERT_NE(cal, nullptr) << "Calibrator creation should succeed";

    // Generate calibration data
    const size_t numel = 4096;
    const int num_batches = 10;
    std::vector<float> host_data(numel);
    std::mt19937 gen(42);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    float* d_data = nullptr;
    cudaMalloc(&d_data, numel * sizeof(float));

    // Feed calibration batches
    for (int i = 0; i < num_batches; i++) {
        // Generate new random data for each batch
        for (auto& x : host_data) x = dist(gen);
        cudaMemcpy(d_data, host_data.data(), numel * sizeof(float), cudaMemcpyHostToDevice);

        int ret = nimcp_int8_calibrator_observe(cal, d_data, numel);
        EXPECT_EQ(ret, 0) << "Calibration observation " << i << " should succeed";
    }

    // Compute final parameters
    nimcp_int8_quant_params_t params;
    nimcp_int8_params_init(&params);

    int ret = nimcp_int8_calibrator_compute_params(cal, &params);
    EXPECT_EQ(ret, 0) << "Computing calibration params should succeed";

    // Verify reasonable parameters
    EXPECT_GT(params.scale, 0.0f) << "Scale should be positive";
    EXPECT_GE(params.min_val, -2.0f) << "Min should be reasonable";
    EXPECT_LE(params.max_val, 2.0f) << "Max should be reasonable";

    cudaFree(d_data);
    nimcp_int8_calibrator_destroy(cal);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: INT8 Full Quantization Pipeline
 * ============================================================================ */
TEST_F(InferenceIntegrationTest, INT8FullQuantizationPipeline) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    const size_t numel = 2048;

    // Step 1: Generate FP32 data
    std::vector<float> fp32_input(numel);
    std::mt19937 gen(42);
    std::uniform_real_distribution<float> dist(-0.5f, 0.5f);
    for (auto& x : fp32_input) x = dist(gen);

    // Step 2: Calibrate
    nimcp_int8_calibrator_t* cal = nimcp_int8_calibrator_create(
        ctx_, INT8_CALIB_MINMAX, false, 1, 1024
    );
    ASSERT_NE(cal, nullptr);

    float* d_fp32 = nullptr;
    cudaMalloc(&d_fp32, numel * sizeof(float));
    cudaMemcpy(d_fp32, fp32_input.data(), numel * sizeof(float), cudaMemcpyHostToDevice);

    EXPECT_EQ(nimcp_int8_calibrator_observe(cal, d_fp32, numel), 0);

    nimcp_int8_quant_params_t params;
    nimcp_int8_params_init(&params);
    EXPECT_EQ(nimcp_int8_calibrator_compute_params(cal, &params), 0);
    nimcp_int8_calibrator_destroy(cal);

    // Step 3: Quantize
    int8_t* d_int8 = nullptr;
    cudaMalloc(&d_int8, numel * sizeof(int8_t));
    EXPECT_EQ(nimcp_int8_quantize(ctx_, d_fp32, d_int8, numel, &params), 0);

    // Step 4: Dequantize
    float* d_dequant = nullptr;
    cudaMalloc(&d_dequant, numel * sizeof(float));
    EXPECT_EQ(nimcp_int8_dequantize(ctx_, d_int8, d_dequant, numel, &params), 0);

    // Step 5: Verify roundtrip error
    std::vector<float> dequant_output(numel);
    cudaMemcpy(dequant_output.data(), d_dequant, numel * sizeof(float), cudaMemcpyDeviceToHost);

    double total_error = 0.0;
    for (size_t i = 0; i < numel; i++) {
        double error = std::abs(dequant_output[i] - fp32_input[i]);
        total_error += error;
    }
    double avg_error = total_error / numel;

    // Average error should be small relative to scale
    EXPECT_LT(avg_error, params.scale * 0.75)
        << "Average quantization error should be bounded";

    cudaFree(d_fp32);
    cudaFree(d_int8);
    cudaFree(d_dequant);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: INT8 GEMM Pipeline
 * ============================================================================ */
TEST_F(InferenceIntegrationTest, INT8GEMMPipeline) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    const int M = 64, K = 128, N = 32;

    // Create FP32 matrices
    std::vector<float> h_A_fp32(M * K);
    std::vector<float> h_B_fp32(K * N);
    std::mt19937 gen(42);
    std::uniform_real_distribution<float> dist(-0.5f, 0.5f);
    for (auto& x : h_A_fp32) x = dist(gen);
    for (auto& x : h_B_fp32) x = dist(gen);

    // Quantization parameters
    nimcp_int8_quant_params_t params_a, params_b;
    init_quant_params(&params_a, 0.5f / 127.0f);
    init_quant_params(&params_b, 0.5f / 127.0f);

    // Quantize to INT8
    std::vector<int8_t> h_A_int8(M * K);
    std::vector<int8_t> h_B_int8(K * N);

    for (size_t i = 0; i < h_A_fp32.size(); i++) {
        float val = h_A_fp32[i] / params_a.scale + params_a.zero_point;
        h_A_int8[i] = (int8_t)std::max(-128.0f, std::min(127.0f, std::round(val)));
    }
    for (size_t i = 0; i < h_B_fp32.size(); i++) {
        float val = h_B_fp32[i] / params_b.scale + params_b.zero_point;
        h_B_int8[i] = (int8_t)std::max(-128.0f, std::min(127.0f, std::round(val)));
    }

    // Allocate device memory
    int8_t* d_A = nullptr;
    int8_t* d_B = nullptr;
    int32_t* d_C = nullptr;
    cudaMalloc(&d_A, M * K * sizeof(int8_t));
    cudaMalloc(&d_B, K * N * sizeof(int8_t));
    cudaMalloc(&d_C, M * N * sizeof(int32_t));
    cudaMemcpy(d_A, h_A_int8.data(), M * K * sizeof(int8_t), cudaMemcpyHostToDevice);
    cudaMemcpy(d_B, h_B_int8.data(), K * N * sizeof(int8_t), cudaMemcpyHostToDevice);

    // Execute INT8 GEMM
    int ret = nimcp_int8_gemm(ctx_, d_A, d_B, d_C, M, N, K, &params_a, &params_b);
    EXPECT_EQ(ret, 0) << "INT8 GEMM should succeed";

    // Copy result back
    std::vector<int32_t> h_C(M * N);
    cudaMemcpy(h_C.data(), d_C, M * N * sizeof(int32_t), cudaMemcpyDeviceToHost);

    // Verify result (spot check)
    // C[0,0] = sum(A[0,:] * B[:,0])
    int32_t expected_00 = 0;
    for (int k = 0; k < K; k++) {
        expected_00 += (int32_t)h_A_int8[k] * (int32_t)h_B_int8[k * N];
    }
    EXPECT_EQ(h_C[0], expected_00) << "GEMM result[0,0] mismatch";

    cudaFree(d_A);
    cudaFree(d_B);
    cudaFree(d_C);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: INT8 Model Creation and Layer Addition
 * ============================================================================ */
TEST_F(InferenceIntegrationTest, INT8ModelPipeline) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    // Create INT8 model
    nimcp_int8_model_t* model = nimcp_int8_model_create(ctx_, 3, "test_mlp");
    ASSERT_NE(model, nullptr) << "Model creation should succeed";

    // Create weight tensors
    nimcp_gpu_tensor_t* w1 = create_tensor({64, 128});
    nimcp_gpu_tensor_t* w2 = create_tensor({32, 64});
    nimcp_gpu_tensor_t* w3 = create_tensor({16, 32});

    ASSERT_NE(w1, nullptr);
    ASSERT_NE(w2, nullptr);
    ASSERT_NE(w3, nullptr);

    fill_random(w1, -0.1f, 0.1f);
    fill_random(w2, -0.1f, 0.1f);
    fill_random(w3, -0.1f, 0.1f);

    // Add layers to model
    EXPECT_EQ(nimcp_int8_model_add_layer(model, 0, "layer1", w1, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_int8_model_add_layer(model, 1, "layer2", w2, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_int8_model_add_layer(model, 2, "layer3", w3, nullptr, nullptr), 0);

    // Verify model structure
    EXPECT_EQ(model->num_layers, 3);

    // Allocate workspace
    EXPECT_EQ(nimcp_int8_model_allocate_workspace(model, 32), 0);

    // Cleanup
    nimcp_gpu_tensor_destroy(w1);
    nimcp_gpu_tensor_destroy(w2);
    nimcp_gpu_tensor_destroy(w3);
    nimcp_int8_model_destroy(model);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Recovery From Simulated Error
 * ============================================================================ */
TEST_F(InferenceIntegrationTest, RecoveryFromSimulatedError) {
#ifdef NIMCP_ENABLE_CUDA
    // Create recovery context
    nimcp_gpu_recovery_context_t* rec_ctx = nimcp_gpu_recovery_context_create(nullptr);
    ASSERT_NE(rec_ctx, nullptr);

    // Simulate an OOM error and attempt recovery
    nimcp_gpu_recovery_result_t result;
    bool recovered = nimcp_gpu_try_recover(
        rec_ctx,
        GPU_ERROR_OUT_OF_MEMORY,
        cudaErrorMemoryAllocation,
        &result
    );

    // Recovery should be attempted
    EXPECT_TRUE(result.success || rec_ctx->retry_count > 0)
        << "Recovery should be attempted";

    // Verify stats updated
    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);
    EXPECT_GT(stats.recoveries_attempted, 0u)
        << "Recovery attempt should be recorded";

    nimcp_gpu_recovery_context_destroy(rec_ctx);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Parameter Correction
 * ============================================================================ */
TEST_F(InferenceIntegrationTest, ParameterCorrection) {
#ifdef NIMCP_ENABLE_CUDA
    // Test integer parameter correction
    int batch_size = -5;  // Invalid
    bool corrected = nimcp_gpu_correct_param_int(&batch_size, 1, 1024, 32, "batch_size");
    EXPECT_TRUE(corrected) << "Should correct invalid batch size";
    EXPECT_GE(batch_size, 1) << "Corrected batch size should be valid";

    // Test float parameter correction
    float learning_rate = -0.1f;  // Invalid
    nimcp_gpu_param_range_t lr_range = {1e-6f, 1.0f, 0.001f, true};
    corrected = nimcp_gpu_correct_param_float(&learning_rate, &lr_range, "learning_rate");
    EXPECT_TRUE(corrected) << "Should correct invalid learning rate";
    EXPECT_GE(learning_rate, lr_range.min_value) << "Corrected LR should be valid";

    // Test valid parameter (should not be corrected)
    int valid_batch = 64;
    corrected = nimcp_gpu_correct_param_int(&valid_batch, 1, 1024, 32, "batch_size");
    EXPECT_FALSE(corrected) << "Should not correct valid parameter";
    EXPECT_EQ(valid_batch, 64);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Batch Reduction for Memory
 * ============================================================================ */
TEST_F(InferenceIntegrationTest, BatchReductionForMemory) {
#ifdef NIMCP_ENABLE_CUDA
    // Test batch size reduction
    size_t batch_size = 1024;
    size_t element_size = sizeof(float);
    size_t memory_per_element = 1024 * 1024;  // 1MB per element (artificially high)

    bool reduced = nimcp_gpu_correct_batch_for_memory(
        &batch_size,
        element_size,
        memory_per_element
    );

    // Whether reduced depends on available memory
    // Just verify function doesn't crash and batch is still valid
    EXPECT_GT(batch_size, 0u) << "Batch size should remain positive";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Inference Session with Graph Capture
 * ============================================================================ */
TEST_F(InferenceIntegrationTest, InferenceSessionGraphCapture) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    nimcp_infer_session_t* session = nimcp_infer_session_create(
        ctx_,
        NIMCP_INFER_FP32,
        1024 * 1024  // 1MB workspace
    );
    ASSERT_NE(session, nullptr);

    EXPECT_FALSE(session->graph_captured);

    // Begin capture
    bool capture_started = nimcp_infer_session_begin_capture(session);
    if (capture_started) {
        // Do some operations that would be captured
        // (In a real test, we'd run inference operations here)

        // End capture
        bool capture_ended = nimcp_infer_session_end_capture(session);
        if (capture_ended) {
            EXPECT_TRUE(session->graph_captured);

            // Replay the graph
            bool replayed = nimcp_infer_session_replay(session);
            EXPECT_TRUE(replayed) << "Graph replay should succeed";
        }
    }

    nimcp_infer_session_destroy(session);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Entropy Calibration
 * ============================================================================ */
TEST_F(InferenceIntegrationTest, EntropyCalibration) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    // Create entropy calibrator
    nimcp_int8_calibrator_t* cal = nimcp_int8_calibrator_create(
        ctx_,
        INT8_CALIB_ENTROPY,
        false,
        1,
        2048  // More bins for entropy calibration
    );
    ASSERT_NE(cal, nullptr);

    // Generate calibration data with normal distribution
    const size_t numel = 8192;
    std::vector<float> host_data(numel);
    std::mt19937 gen(42);
    std::normal_distribution<float> dist(0.0f, 0.3f);

    float* d_data = nullptr;
    cudaMalloc(&d_data, numel * sizeof(float));

    // Feed multiple batches
    for (int i = 0; i < 5; i++) {
        for (auto& x : host_data) x = dist(gen);
        cudaMemcpy(d_data, host_data.data(), numel * sizeof(float), cudaMemcpyHostToDevice);
        EXPECT_EQ(nimcp_int8_calibrator_observe(cal, d_data, numel), 0);
    }

    // Compute entropy-based parameters
    nimcp_int8_quant_params_t params;
    nimcp_int8_params_init(&params);

    int ret = nimcp_int8_calibrator_compute_entropy(cal, &params);
    EXPECT_EQ(ret, 0) << "Entropy calibration should succeed";

    // Entropy calibration should find reasonable clip values
    EXPECT_GT(params.scale, 0.0f);

    cudaFree(d_data);
    nimcp_int8_calibrator_destroy(cal);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Percentile Calibration
 * ============================================================================ */
TEST_F(InferenceIntegrationTest, PercentileCalibration) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    // Create percentile calibrator
    nimcp_int8_calibrator_t* cal = nimcp_int8_calibrator_create(
        ctx_,
        INT8_CALIB_PERCENTILE,
        false,
        1,
        1024
    );
    ASSERT_NE(cal, nullptr);

    // Generate calibration data with some outliers
    const size_t numel = 4096;
    std::vector<float> host_data(numel);
    std::mt19937 gen(42);
    std::normal_distribution<float> dist(0.0f, 0.5f);

    float* d_data = nullptr;
    cudaMalloc(&d_data, numel * sizeof(float));

    for (auto& x : host_data) {
        x = dist(gen);
        // Add some outliers
        if (std::abs(x) > 1.5f) x *= 3.0f;
    }
    cudaMemcpy(d_data, host_data.data(), numel * sizeof(float), cudaMemcpyHostToDevice);

    EXPECT_EQ(nimcp_int8_calibrator_observe(cal, d_data, numel), 0);

    // Compute percentile-based parameters (99.9 percentile)
    nimcp_int8_quant_params_t params;
    nimcp_int8_params_init(&params);

    int ret = nimcp_int8_calibrator_compute_percentile(cal, 99.9f, &params);
    EXPECT_EQ(ret, 0) << "Percentile calibration should succeed";

    // Percentile should clip outliers
    EXPECT_GT(params.scale, 0.0f);

    cudaFree(d_data);
    nimcp_int8_calibrator_destroy(cal);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: INT8 Tensor Clone
 * ============================================================================ */
TEST_F(InferenceIntegrationTest, INT8TensorClone) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    size_t dims[] = {16, 32, 64};
    nimcp_int8_tensor_t* original = nimcp_int8_tensor_create(ctx_, dims, 3);
    ASSERT_NE(original, nullptr);

    // Set some quantization parameters
    original->params.scale = 0.02f;
    original->params.zero_point = 5;

    // Clone the tensor
    nimcp_int8_tensor_t* clone = nimcp_int8_tensor_clone(original);
    ASSERT_NE(clone, nullptr);

    // Verify clone properties
    EXPECT_EQ(clone->rank, original->rank);
    EXPECT_EQ(clone->numel, original->numel);
    EXPECT_FLOAT_EQ(clone->params.scale, original->params.scale);
    EXPECT_EQ(clone->params.zero_point, original->params.zero_point);

    // Verify they are independent
    EXPECT_NE(clone->data, original->data);

    nimcp_int8_tensor_destroy(original);
    nimcp_int8_tensor_destroy(clone);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Per-Channel Quantization
 * ============================================================================ */
TEST_F(InferenceIntegrationTest, PerChannelQuantization) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    const int N = 4, C = 8, HW = 16;  // Batch, Channels, spatial
    const size_t numel = N * C * HW;

    // Create per-channel parameters
    nimcp_int8_quant_params_t* params = nimcp_int8_params_create_per_channel(C);
    ASSERT_NE(params, nullptr);

    // Set different scales per channel
    for (int c = 0; c < C; c++) {
        params->channel_scales[c] = 0.01f * (c + 1);
        params->channel_zero_points[c] = 0;
    }

    // Create test data
    std::vector<float> host_input(numel);
    std::mt19937 gen(42);
    std::uniform_real_distribution<float> dist(-0.5f, 0.5f);
    for (auto& x : host_input) x = dist(gen);

    float* d_input = nullptr;
    int8_t* d_output = nullptr;
    cudaMalloc(&d_input, numel * sizeof(float));
    cudaMalloc(&d_output, numel * sizeof(int8_t));
    cudaMemcpy(d_input, host_input.data(), numel * sizeof(float), cudaMemcpyHostToDevice);

    // Quantize per-channel
    int ret = nimcp_int8_quantize_per_channel(ctx_, d_input, d_output, N, C, HW, params);
    EXPECT_EQ(ret, 0) << "Per-channel quantization should succeed";

    // Dequantize per-channel
    float* d_dequant = nullptr;
    cudaMalloc(&d_dequant, numel * sizeof(float));
    ret = nimcp_int8_dequantize_per_channel(ctx_, d_output, d_dequant, N, C, HW, params);
    EXPECT_EQ(ret, 0) << "Per-channel dequantization should succeed";

    cudaFree(d_input);
    cudaFree(d_output);
    cudaFree(d_dequant);
    nimcp_int8_params_destroy(params);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: TensorRT Config API (if available)
 * ============================================================================ */
#ifdef NIMCP_ENABLE_TENSORRT
TEST_F(InferenceIntegrationTest, TensorRTConfigAPI) {
#ifdef NIMCP_ENABLE_CUDA
    // Test default config
    nimcp_trt_config_t config;
    int ret = nimcp_trt_default_config(&config);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(config.precision, TRT_PRECISION_INT8);
    EXPECT_GT(config.max_batch_size, 0);
    EXPECT_GT(config.max_workspace_size, 0u);

    // Test FP16 config
    ret = nimcp_trt_fp16_config(&config);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(config.precision, TRT_PRECISION_FP16);

    // Test INT8 strict config
    ret = nimcp_trt_int8_strict_config(&config);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(config.use_strict_types);

    // Test validation
    ret = nimcp_trt_validate_config(&config);
    EXPECT_EQ(ret, 0);

    // Test name functions
    EXPECT_STREQ(nimcp_trt_precision_name(TRT_PRECISION_INT8), "int8");
    EXPECT_STREQ(nimcp_trt_precision_name(TRT_PRECISION_FP16), "fp16");
    EXPECT_STREQ(nimcp_trt_format_name(TRT_FORMAT_ENGINE), "engine");
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}
#endif

/* ============================================================================
 * Test: Fake Quantization for QAT
 * ============================================================================ */
TEST_F(InferenceIntegrationTest, FakeQuantization) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    const size_t numel = 1024;

    std::vector<float> host_input(numel);
    std::mt19937 gen(42);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    for (auto& x : host_input) x = dist(gen);

    float* d_input = nullptr;
    float* d_output = nullptr;
    cudaMalloc(&d_input, numel * sizeof(float));
    cudaMalloc(&d_output, numel * sizeof(float));
    cudaMemcpy(d_input, host_input.data(), numel * sizeof(float), cudaMemcpyHostToDevice);

    nimcp_int8_quant_params_t params;
    init_quant_params(&params, 1.0f / 127.0f);

    // Fake quantize (quantize + dequantize in one step)
    int ret = nimcp_int8_fake_quantize(ctx_, d_input, d_output, numel, &params);
    EXPECT_EQ(ret, 0) << "Fake quantization should succeed";

    // Output should be similar to input but with quantization noise
    std::vector<float> host_output(numel);
    cudaMemcpy(host_output.data(), d_output, numel * sizeof(float), cudaMemcpyDeviceToHost);

    float max_error = 0.0f;
    for (size_t i = 0; i < numel; i++) {
        float error = std::abs(host_output[i] - host_input[i]);
        max_error = std::max(max_error, error);
    }
    EXPECT_LT(max_error, params.scale * 1.5f)
        << "Fake quantization error should be bounded";

    cudaFree(d_input);
    cudaFree(d_output);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Recovery Statistics Accuracy
 * ============================================================================ */
TEST_F(InferenceIntegrationTest, RecoveryStatisticsAccuracy) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_reset_stats();

    nimcp_gpu_recovery_stats_t initial_stats;
    nimcp_gpu_recovery_get_stats(&initial_stats);
    EXPECT_EQ(initial_stats.total_errors, 0u);

    // Trigger multiple errors
    for (int i = 0; i < 5; i++) {
        nimcp_int8_quantize(ctx_, nullptr, nullptr, 100, nullptr);
    }

    nimcp_gpu_recovery_stats_t final_stats;
    nimcp_gpu_recovery_get_stats(&final_stats);
    EXPECT_GE(final_stats.total_errors, 5u)
        << "Should track all error occurrences";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

}  // namespace
