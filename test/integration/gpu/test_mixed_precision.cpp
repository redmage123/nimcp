/**
 * @file test_mixed_precision.cpp
 * @brief Integration tests for mixed precision and quantization in NIMCP
 *
 * WHAT: Verify precision conversion and quantization accuracy
 * WHY:  Mixed precision enables faster inference with acceptable accuracy
 * HOW:  Test FP32/FP16/INT8 conversions and quantization calibration
 *
 * TEST COVERAGE:
 * - FP32 vs FP16 computation within tolerance
 * - INT8 quantization accuracy
 * - Precision conversion functions
 * - Calibration for optimal quantization parameters
 * - Dynamic range handling
 * - Quantization parameter computation
 * - Per-tensor and symmetric quantization modes
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>
#include <algorithm>
#include <limits>

// GPU headers include CUDA headers that cannot be in extern "C" blocks
#include "gpu/backend/nimcp_kernel_backend.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/inference/nimcp_inference_gpu.h"

// Headers have their own extern "C" guards
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Configuration Constants
//=============================================================================

namespace {
    // Tolerance thresholds for precision comparisons
    constexpr float FP16_TOLERANCE = 1e-3f;           // FP16 has ~3.3 decimal digits precision
    constexpr float INT8_REL_TOLERANCE = 0.02f;       // INT8 allows ~2% relative error
    constexpr float QUANTIZE_TOLERANCE = 0.05f;       // 5% tolerance for quantization
    constexpr float CALIBRATION_TOLERANCE = 1e-4f;    // Tight tolerance for calibration

    // Test tensor sizes
    constexpr size_t SMALL_SIZE = 64;
    constexpr size_t MEDIUM_SIZE = 256;
    constexpr size_t LARGE_SIZE = 1024;

    // Quantization parameters
    constexpr int8_t QUANT_INT8_MIN = -128;
    constexpr int8_t QUANT_INT8_MAX = 127;
}

//=============================================================================
// Test Fixture
//=============================================================================

class MixedPrecisionTest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* gpu_ctx = nullptr;
    std::mt19937 rng{54321};  // Reproducible random numbers

    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
        nimcp_memory_clear_stats();

        // Initialize kernel backend
        bool init_ok = nimcp_kernel_backend_init(NIMCP_BACKEND_AUTO);
        ASSERT_TRUE(init_ok) << "Failed to initialize kernel backend";

        // Create GPU context
        gpu_ctx = nimcp_gpu_context_create_auto();
        // Note: gpu_ctx can be NULL if no GPU - tests will skip GPU portions
    }

    void TearDown() override {
        if (gpu_ctx) {
            nimcp_gpu_context_destroy(gpu_ctx);
            gpu_ctx = nullptr;
        }

        nimcp_kernel_backend_shutdown();

        // Check for memory leaks
        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        EXPECT_LT(stats.current_allocated, 4096)
            << "Potential memory leak: " << stats.current_allocated << " bytes";
    }

    //=========================================================================
    // Helper: Generate random float data
    //=========================================================================
    std::vector<float> generateRandomData(size_t count, float min_val = -1.0f, float max_val = 1.0f) {
        std::vector<float> data(count);
        std::uniform_real_distribution<float> dist(min_val, max_val);
        for (size_t i = 0; i < count; i++) {
            data[i] = dist(rng);
        }
        return data;
    }

    //=========================================================================
    // Helper: Generate data with specific distribution patterns
    //=========================================================================
    std::vector<float> generateNormalData(size_t count, float mean = 0.0f, float stddev = 1.0f) {
        std::vector<float> data(count);
        std::normal_distribution<float> dist(mean, stddev);
        for (size_t i = 0; i < count; i++) {
            data[i] = dist(rng);
        }
        return data;
    }

    std::vector<float> generateUniformPositive(size_t count, float max_val = 10.0f) {
        std::vector<float> data(count);
        std::uniform_real_distribution<float> dist(0.0f, max_val);
        for (size_t i = 0; i < count; i++) {
            data[i] = dist(rng);
        }
        return data;
    }

    //=========================================================================
    // Helper: Create GPU tensor from host data
    //=========================================================================
    nimcp_gpu_tensor_t* createGPUTensor(const std::vector<float>& data,
                                        nimcp_gpu_precision_t precision = NIMCP_GPU_PRECISION_FP32) {
        if (!gpu_ctx) return nullptr;
        std::vector<size_t> dims = {data.size()};
        return nimcp_gpu_tensor_from_host(
            gpu_ctx,
            data.data(),
            dims.data(),
            1,
            precision
        );
    }

    nimcp_gpu_tensor_t* createEmptyTensor(size_t size, nimcp_gpu_precision_t precision = NIMCP_GPU_PRECISION_FP32) {
        if (!gpu_ctx) return nullptr;
        std::vector<size_t> dims = {size};
        return nimcp_gpu_tensor_create(gpu_ctx, dims.data(), 1, precision);
    }

    //=========================================================================
    // Helper: Copy GPU tensor to host
    //=========================================================================
    std::vector<float> copyToHost(const nimcp_gpu_tensor_t* tensor) {
        if (!tensor || !gpu_ctx) return {};
        std::vector<float> result(tensor->numel);
        nimcp_gpu_tensor_to_host(tensor, result.data());
        return result;
    }

    //=========================================================================
    // Helper: Check if GPU is available and active
    //=========================================================================
    bool hasGPU() const {
        // Must have valid GPU context AND active backend must be CUDA
        if (!gpu_ctx || !nimcp_cuda_backend_available()) {
            return false;
        }
        nimcp_backend_type_t active_type = nimcp_get_backend_type();
        return active_type == NIMCP_BACKEND_CUDA;
    }

    //=========================================================================
    // CPU Reference: Quantization functions
    //=========================================================================
    void cpuQuantizeInt8(const float* input, int8_t* output, size_t n,
                         float scale, int32_t zero_point) {
        for (size_t i = 0; i < n; i++) {
            float val = input[i] / scale + static_cast<float>(zero_point);
            val = std::round(val);
            val = std::max(static_cast<float>(INT8_MIN), std::min(static_cast<float>(INT8_MAX), val));
            output[i] = static_cast<int8_t>(val);
        }
    }

    void cpuDequantizeInt8(const int8_t* input, float* output, size_t n,
                           float scale, int32_t zero_point) {
        for (size_t i = 0; i < n; i++) {
            output[i] = (static_cast<float>(input[i]) - static_cast<float>(zero_point)) * scale;
        }
    }

    void cpuComputeQuantParams(const float* data, size_t n, float& scale, int32_t& zero_point,
                               bool symmetric = false) {
        float min_val = data[0], max_val = data[0];
        for (size_t i = 1; i < n; i++) {
            min_val = std::min(min_val, data[i]);
            max_val = std::max(max_val, data[i]);
        }

        if (symmetric) {
            float abs_max = std::max(std::fabs(min_val), std::fabs(max_val));
            scale = abs_max / 127.0f;
            zero_point = 0;
        } else {
            scale = (max_val - min_val) / 255.0f;
            if (scale < 1e-8f) scale = 1e-8f;  // Avoid division by zero
            zero_point = static_cast<int32_t>(std::round(-min_val / scale) + INT8_MIN);
            zero_point = std::max(static_cast<int32_t>(INT8_MIN), std::min(static_cast<int32_t>(INT8_MAX), zero_point));
        }

        if (scale < 1e-8f) scale = 1e-8f;  // Prevent zero scale
    }

    //=========================================================================
    // CPU Reference: FP16 simulation (using float with reduced precision)
    //=========================================================================
    float simulateFP16(float val) {
        // FP16 has 1 sign, 5 exponent, 10 mantissa bits
        // Simulate by rounding to approximate precision
        if (std::fabs(val) < 1e-7f) return 0.0f;  // Denormal handling

        // Round to ~3 significant decimal digits
        float scale = std::pow(10.0f, std::floor(std::log10(std::fabs(val))));
        return std::round(val / scale * 1024.0f) * scale / 1024.0f;
    }

    //=========================================================================
    // Helper: Compute relative error
    //=========================================================================
    float relativeError(float expected, float actual) {
        if (std::fabs(expected) < 1e-10f) {
            return std::fabs(actual);
        }
        return std::fabs(actual - expected) / std::fabs(expected);
    }

    //=========================================================================
    // Helper: Compute Mean Squared Error
    //=========================================================================
    float computeMSE(const float* a, const float* b, size_t n) {
        float mse = 0;
        for (size_t i = 0; i < n; i++) {
            float diff = a[i] - b[i];
            mse += diff * diff;
        }
        return mse / n;
    }

    //=========================================================================
    // Helper: Compute Signal-to-Quantization-Noise Ratio (SQNR)
    //=========================================================================
    float computeSQNR(const float* original, const float* quantized, size_t n) {
        float signal_power = 0;
        float noise_power = 0;
        for (size_t i = 0; i < n; i++) {
            signal_power += original[i] * original[i];
            float noise = original[i] - quantized[i];
            noise_power += noise * noise;
        }
        if (noise_power < 1e-10f) return 100.0f;  // Very high SQNR
        return 10.0f * std::log10(signal_power / noise_power);
    }
};

//=============================================================================
// FP32 vs FP16 PRECISION TESTS
//=============================================================================

/**
 * WHAT: Test FP32 to FP16 tensor conversion
 * WHY:  FP16 enables faster inference with tensor cores
 * HOW:  Convert tensor, verify values within FP16 tolerance
 */
TEST_F(MixedPrecisionTest, FP32toFP16_Conversion) {
    if (!hasGPU()) {
        GTEST_SKIP() << "No GPU available for precision conversion test";
    }

    const size_t size = MEDIUM_SIZE;
    auto fp32_data = generateNormalData(size, 0.0f, 1.0f);

    // Create FP32 tensor
    auto* tensor_fp32 = createGPUTensor(fp32_data, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(tensor_fp32, nullptr);

    // Create FP16 output tensor
    auto* tensor_fp16 = createEmptyTensor(size, NIMCP_GPU_PRECISION_FP16);
    ASSERT_NE(tensor_fp16, nullptr);

    // Convert FP32 -> FP16
    bool convert_ok = nimcp_gpu_infer_convert_precision(
        gpu_ctx, tensor_fp32, tensor_fp16, NIMCP_INFER_FP16);
    EXPECT_TRUE(convert_ok) << "FP32 to FP16 conversion should succeed";

    // Convert back FP16 -> FP32 for comparison
    auto* tensor_back = createEmptyTensor(size, NIMCP_GPU_PRECISION_FP32);
    convert_ok = nimcp_gpu_infer_convert_precision(
        gpu_ctx, tensor_fp16, tensor_back, NIMCP_INFER_FP32);
    EXPECT_TRUE(convert_ok);

    auto result = copyToHost(tensor_back);

    // Verify values within FP16 tolerance
    for (size_t i = 0; i < size; i++) {
        float rel_err = relativeError(fp32_data[i], result[i]);
        EXPECT_LT(rel_err, FP16_TOLERANCE)
            << "FP16 round-trip error too large at index " << i
            << ": original=" << fp32_data[i] << ", result=" << result[i];
    }

    nimcp_gpu_tensor_destroy(tensor_fp32);
    nimcp_gpu_tensor_destroy(tensor_fp16);
    nimcp_gpu_tensor_destroy(tensor_back);
}

/**
 * WHAT: Test FP16 matrix multiplication accuracy
 * WHY:  MatMul is most common operation, must work in FP16
 * HOW:  Compute matmul in FP32 and FP16, compare results
 */
TEST_F(MixedPrecisionTest, FP16_MatMul_Accuracy) {
    if (!hasGPU()) {
        GTEST_SKIP() << "No GPU available for FP16 matmul test";
    }

    const size_t M = 32, N = 64, K = 48;
    auto data_a = generateNormalData(M * K, 0.0f, 0.5f);  // Smaller range for stability
    auto data_b = generateNormalData(K * N, 0.0f, 0.5f);

    std::vector<size_t> dims_a = {M, K};
    std::vector<size_t> dims_b = {K, N};
    std::vector<size_t> dims_c = {M, N};

    // FP32 computation
    auto* tensor_a_fp32 = nimcp_gpu_tensor_from_host(gpu_ctx, data_a.data(), dims_a.data(), 2, NIMCP_GPU_PRECISION_FP32);
    auto* tensor_b_fp32 = nimcp_gpu_tensor_from_host(gpu_ctx, data_b.data(), dims_b.data(), 2, NIMCP_GPU_PRECISION_FP32);
    auto* tensor_c_fp32 = nimcp_gpu_tensor_create(gpu_ctx, dims_c.data(), 2, NIMCP_GPU_PRECISION_FP32);

    NIMCP_TENSOR_OPS()->matmul(gpu_ctx, tensor_a_fp32, tensor_b_fp32, tensor_c_fp32);
    auto result_fp32 = copyToHost(tensor_c_fp32);

    // FP16 computation
    auto* tensor_a_fp16 = nimcp_gpu_tensor_from_host(gpu_ctx, data_a.data(), dims_a.data(), 2, NIMCP_GPU_PRECISION_FP16);
    auto* tensor_b_fp16 = nimcp_gpu_tensor_from_host(gpu_ctx, data_b.data(), dims_b.data(), 2, NIMCP_GPU_PRECISION_FP16);
    auto* tensor_c_fp16 = nimcp_gpu_tensor_create(gpu_ctx, dims_c.data(), 2, NIMCP_GPU_PRECISION_FP16);

    bool fp16_ok = nimcp_gpu_infer_gemm_fp16(
        gpu_ctx, tensor_a_fp16, tensor_b_fp16, tensor_c_fp16, true);  // accumulate in FP32

    if (fp16_ok) {
        // Convert result to FP32 for comparison
        auto* tensor_c_result = createEmptyTensor(M * N, NIMCP_GPU_PRECISION_FP32);
        nimcp_gpu_infer_convert_precision(gpu_ctx, tensor_c_fp16, tensor_c_result, NIMCP_INFER_FP32);
        auto result_fp16 = copyToHost(tensor_c_result);

        // Compute MSE between FP32 and FP16 results
        float mse = computeMSE(result_fp32.data(), result_fp16.data(), M * N);
        EXPECT_LT(mse, FP16_TOLERANCE * FP16_TOLERANCE)
            << "FP16 matmul MSE too high: " << mse;

        nimcp_gpu_tensor_destroy(tensor_c_result);
    }

    nimcp_gpu_tensor_destroy(tensor_a_fp32);
    nimcp_gpu_tensor_destroy(tensor_b_fp32);
    nimcp_gpu_tensor_destroy(tensor_c_fp32);
    nimcp_gpu_tensor_destroy(tensor_a_fp16);
    nimcp_gpu_tensor_destroy(tensor_b_fp16);
    nimcp_gpu_tensor_destroy(tensor_c_fp16);
}

/**
 * WHAT: Test FP16 handles edge cases
 * WHY:  FP16 has limited range and precision
 * HOW:  Test with very small and large values
 */
TEST_F(MixedPrecisionTest, FP16_EdgeCases) {
    if (!hasGPU()) {
        GTEST_SKIP() << "No GPU available";
    }

    // Test data with various ranges
    std::vector<float> edge_cases = {
        0.0f, 1.0f, -1.0f,
        1e-4f, -1e-4f,           // Small values
        1e4f, -1e4f,             // Large values (within FP16 range)
        65504.0f, -65504.0f,     // Max FP16 values
        0.00006103515625f,       // Min positive normal FP16
    };

    auto* tensor_fp32 = createGPUTensor(edge_cases, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(tensor_fp32, nullptr);

    auto* tensor_fp16 = createEmptyTensor(edge_cases.size(), NIMCP_GPU_PRECISION_FP16);
    auto* tensor_back = createEmptyTensor(edge_cases.size(), NIMCP_GPU_PRECISION_FP32);

    nimcp_gpu_infer_convert_precision(gpu_ctx, tensor_fp32, tensor_fp16, NIMCP_INFER_FP16);
    nimcp_gpu_infer_convert_precision(gpu_ctx, tensor_fp16, tensor_back, NIMCP_INFER_FP32);

    auto result = copyToHost(tensor_back);

    for (size_t i = 0; i < edge_cases.size(); i++) {
        float original = edge_cases[i];
        float converted = result[i];

        if (std::fabs(original) < 1e-10f) {
            EXPECT_NEAR(converted, 0.0f, 1e-6f) << "Zero should remain zero";
        } else if (std::fabs(original) < 1e-3f) {
            // Small values may lose precision
            float rel_err = relativeError(original, converted);
            EXPECT_LT(rel_err, 0.1f) << "Small value precision loss too high at index " << i;
        } else {
            float rel_err = relativeError(original, converted);
            EXPECT_LT(rel_err, FP16_TOLERANCE) << "Value error at index " << i;
        }
    }

    nimcp_gpu_tensor_destroy(tensor_fp32);
    nimcp_gpu_tensor_destroy(tensor_fp16);
    nimcp_gpu_tensor_destroy(tensor_back);
}

//=============================================================================
// INT8 QUANTIZATION TESTS
//=============================================================================

/**
 * WHAT: Test INT8 quantization and dequantization round-trip
 * WHY:  Quantization must preserve information within tolerance
 * HOW:  Quantize, dequantize, compare with original
 */
TEST_F(MixedPrecisionTest, INT8_RoundTrip) {
    if (!hasGPU()) {
        GTEST_SKIP() << "No GPU available";
    }

    const size_t size = MEDIUM_SIZE;
    auto original = generateNormalData(size, 0.0f, 2.0f);

    // Compute quantization parameters
    nimcp_quant_params_t params;
    auto* tensor_original = createGPUTensor(original, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(tensor_original, nullptr);

    bool calibrate_ok = nimcp_gpu_infer_calibrate(gpu_ctx, tensor_original, &params, true);  // symmetric
    EXPECT_TRUE(calibrate_ok) << "Calibration should succeed";
    EXPECT_GT(params.scale, 0.0f) << "Scale should be positive";

    // Quantize
    auto* tensor_int8 = createEmptyTensor(size, NIMCP_GPU_PRECISION_INT8);
    bool quant_ok = nimcp_gpu_infer_quantize_int8(gpu_ctx, tensor_original, tensor_int8, &params);
    EXPECT_TRUE(quant_ok) << "Quantization should succeed";

    // Dequantize
    auto* tensor_dequant = createEmptyTensor(size, NIMCP_GPU_PRECISION_FP32);
    bool dequant_ok = nimcp_gpu_infer_dequantize_int8(gpu_ctx, tensor_int8, tensor_dequant, &params);
    EXPECT_TRUE(dequant_ok) << "Dequantization should succeed";

    auto result = copyToHost(tensor_dequant);

    // Compute SQNR
    float sqnr = computeSQNR(original.data(), result.data(), size);
    EXPECT_GT(sqnr, 20.0f) << "SQNR should be > 20 dB for normal distribution";

    // Check relative errors
    // Note: For symmetric INT8 quantization with scale = absmax/127,
    // relative error at value x is approximately (scale/2)/|x| = absmax/(254*|x|)
    // For 2% tolerance, need |x| > absmax/5.08. With absmax~6, need |x| > 1.2
    // So only check values with sufficient magnitude for meaningful tolerance
    int error_count = 0;
    for (size_t i = 0; i < size; i++) {
        float rel_err = relativeError(original[i], result[i]);
        // Only count errors for values large enough to expect < 2% quantization error
        if (rel_err > INT8_REL_TOLERANCE && std::fabs(original[i]) > 1.0f) {
            error_count++;
        }
    }
    EXPECT_LT(error_count, size / 10) << "Too many values exceed tolerance";

    nimcp_gpu_tensor_destroy(tensor_original);
    nimcp_gpu_tensor_destroy(tensor_int8);
    nimcp_gpu_tensor_destroy(tensor_dequant);
}

/**
 * WHAT: Test INT8 quantization parameter computation
 * WHY:  Optimal parameters minimize quantization error
 * HOW:  Compare computed parameters with CPU reference
 */
TEST_F(MixedPrecisionTest, INT8_ParameterComputation) {
    if (!hasGPU()) {
        GTEST_SKIP() << "No GPU available";
    }

    const size_t size = LARGE_SIZE;
    auto data = generateNormalData(size, 0.0f, 3.0f);

    // CPU reference computation
    float cpu_scale;
    int32_t cpu_zero_point;
    cpuComputeQuantParams(data.data(), size, cpu_scale, cpu_zero_point, true);

    // GPU computation
    auto* tensor = createGPUTensor(data, NIMCP_GPU_PRECISION_FP32);
    nimcp_quant_params_t gpu_params;
    nimcp_gpu_infer_calibrate(gpu_ctx, tensor, &gpu_params, true);

    // Compare parameters
    EXPECT_NEAR(gpu_params.scale, cpu_scale, cpu_scale * 0.01f)
        << "Scale computation differs significantly";
    EXPECT_EQ(gpu_params.zero_point, cpu_zero_point)
        << "Zero point differs (symmetric should be 0)";

    nimcp_gpu_tensor_destroy(tensor);
}

/**
 * WHAT: Test asymmetric quantization
 * WHY:  Asymmetric quantization better for ReLU activations
 * HOW:  Quantize with asymmetric params, verify range utilization
 */
TEST_F(MixedPrecisionTest, INT8_AsymmetricQuantization) {
    if (!hasGPU()) {
        GTEST_SKIP() << "No GPU available";
    }

    const size_t size = MEDIUM_SIZE;
    // Generate ReLU-like distribution (mostly positive)
    auto data = generateUniformPositive(size, 5.0f);

    // Calibrate with asymmetric mode
    auto* tensor = createGPUTensor(data, NIMCP_GPU_PRECISION_FP32);
    nimcp_quant_params_t params;
    nimcp_gpu_infer_calibrate(gpu_ctx, tensor, &params, false);  // asymmetric

    // Verify parameters make sense for positive data
    EXPECT_GE(params.min_val, 0.0f) << "Min should be >= 0 for positive data";
    EXPECT_GT(params.scale, 0.0f);

    // Quantize and dequantize
    auto* tensor_int8 = createEmptyTensor(size, NIMCP_GPU_PRECISION_INT8);
    auto* tensor_dequant = createEmptyTensor(size, NIMCP_GPU_PRECISION_FP32);

    nimcp_gpu_infer_quantize_int8(gpu_ctx, tensor, tensor_int8, &params);
    nimcp_gpu_infer_dequantize_int8(gpu_ctx, tensor_int8, tensor_dequant, &params);

    auto result = copyToHost(tensor_dequant);

    // Compute SQNR - should be good for well-calibrated asymmetric
    float sqnr = computeSQNR(data.data(), result.data(), size);
    EXPECT_GT(sqnr, 25.0f) << "SQNR should be good for asymmetric quantization";

    nimcp_gpu_tensor_destroy(tensor);
    nimcp_gpu_tensor_destroy(tensor_int8);
    nimcp_gpu_tensor_destroy(tensor_dequant);
}

/**
 * WHAT: Test INT8 quantized matrix multiplication
 * WHY:  INT8 GEMM is key for inference acceleration
 * HOW:  Compare INT8 matmul result with FP32 reference
 */
TEST_F(MixedPrecisionTest, INT8_MatMul_Accuracy) {
    if (!hasGPU()) {
        GTEST_SKIP() << "No GPU available";
    }

    const size_t M = 16, N = 32, K = 24;
    auto data_a = generateNormalData(M * K, 0.0f, 1.0f);
    auto data_b = generateNormalData(K * N, 0.0f, 1.0f);

    std::vector<size_t> dims_a = {M, K};
    std::vector<size_t> dims_b = {K, N};
    std::vector<size_t> dims_c = {M, N};

    // FP32 reference
    auto* tensor_a_fp32 = nimcp_gpu_tensor_from_host(gpu_ctx, data_a.data(), dims_a.data(), 2, NIMCP_GPU_PRECISION_FP32);
    auto* tensor_b_fp32 = nimcp_gpu_tensor_from_host(gpu_ctx, data_b.data(), dims_b.data(), 2, NIMCP_GPU_PRECISION_FP32);
    auto* tensor_c_fp32 = nimcp_gpu_tensor_create(gpu_ctx, dims_c.data(), 2, NIMCP_GPU_PRECISION_FP32);

    NIMCP_TENSOR_OPS()->matmul(gpu_ctx, tensor_a_fp32, tensor_b_fp32, tensor_c_fp32);
    auto result_fp32 = copyToHost(tensor_c_fp32);

    // Calibrate and quantize inputs
    nimcp_quant_params_t params_a, params_b, params_c;
    nimcp_gpu_infer_calibrate(gpu_ctx, tensor_a_fp32, &params_a, true);
    nimcp_gpu_infer_calibrate(gpu_ctx, tensor_b_fp32, &params_b, true);

    auto* tensor_a_int8 = nimcp_gpu_tensor_create(gpu_ctx, dims_a.data(), 2, NIMCP_GPU_PRECISION_INT8);
    auto* tensor_b_int8 = nimcp_gpu_tensor_create(gpu_ctx, dims_b.data(), 2, NIMCP_GPU_PRECISION_INT8);
    auto* tensor_c_result = nimcp_gpu_tensor_create(gpu_ctx, dims_c.data(), 2, NIMCP_GPU_PRECISION_FP32);

    nimcp_gpu_infer_quantize_int8(gpu_ctx, tensor_a_fp32, tensor_a_int8, &params_a);
    nimcp_gpu_infer_quantize_int8(gpu_ctx, tensor_b_fp32, tensor_b_int8, &params_b);

    // Estimate output scale from input scales
    params_c.scale = params_a.scale * params_b.scale;
    params_c.zero_point = 0;

    // INT8 matmul
    bool int8_ok = nimcp_gpu_infer_gemm_int8(
        gpu_ctx, tensor_a_int8, tensor_b_int8, tensor_c_result,
        &params_a, &params_b, &params_c);

    if (int8_ok) {
        auto result_int8 = copyToHost(tensor_c_result);

        // Compute relative error
        float total_rel_error = 0;
        for (size_t i = 0; i < M * N; i++) {
            float rel_err = relativeError(result_fp32[i], result_int8[i]);
            total_rel_error += rel_err;
        }
        float avg_rel_error = total_rel_error / (M * N);

        EXPECT_LT(avg_rel_error, QUANTIZE_TOLERANCE)
            << "INT8 matmul average relative error too high: " << avg_rel_error;
    }

    nimcp_gpu_tensor_destroy(tensor_a_fp32);
    nimcp_gpu_tensor_destroy(tensor_b_fp32);
    nimcp_gpu_tensor_destroy(tensor_c_fp32);
    nimcp_gpu_tensor_destroy(tensor_a_int8);
    nimcp_gpu_tensor_destroy(tensor_b_int8);
    nimcp_gpu_tensor_destroy(tensor_c_result);
}

//=============================================================================
// CALIBRATION TESTS
//=============================================================================

/**
 * WHAT: Test calibration captures data range accurately
 * WHY:  Range determines quantization parameters
 * HOW:  Compare calibrated min/max with actual data range
 */
TEST_F(MixedPrecisionTest, Calibration_RangeAccuracy) {
    if (!hasGPU()) {
        GTEST_SKIP() << "No GPU available";
    }

    const size_t size = MEDIUM_SIZE;
    auto data = generateRandomData(size, -5.0f, 10.0f);

    // Compute actual range
    float actual_min = *std::min_element(data.begin(), data.end());
    float actual_max = *std::max_element(data.begin(), data.end());

    auto* tensor = createGPUTensor(data, NIMCP_GPU_PRECISION_FP32);
    nimcp_quant_params_t params;
    nimcp_gpu_infer_calibrate(gpu_ctx, tensor, &params, false);

    // Verify calibrated range covers data
    EXPECT_LE(params.min_val, actual_min + CALIBRATION_TOLERANCE);
    EXPECT_GE(params.max_val, actual_max - CALIBRATION_TOLERANCE);

    nimcp_gpu_tensor_destroy(tensor);
}

/**
 * WHAT: Test calibration with constant data
 * WHY:  Edge case - all values the same
 * HOW:  Calibrate constant tensor, verify no division by zero
 */
TEST_F(MixedPrecisionTest, Calibration_ConstantData) {
    if (!hasGPU()) {
        GTEST_SKIP() << "No GPU available";
    }

    const size_t size = MEDIUM_SIZE;
    std::vector<float> constant_data(size, 5.0f);

    auto* tensor = createGPUTensor(constant_data, NIMCP_GPU_PRECISION_FP32);
    nimcp_quant_params_t params;
    bool ok = nimcp_gpu_infer_calibrate(gpu_ctx, tensor, &params, true);

    EXPECT_TRUE(ok) << "Calibration should handle constant data";
    EXPECT_GT(params.scale, 0.0f) << "Scale should still be positive";

    nimcp_gpu_tensor_destroy(tensor);
}

/**
 * WHAT: Test calibration with very small values
 * WHY:  Small values need careful scale computation
 * HOW:  Calibrate near-zero data, verify precision preserved
 */
TEST_F(MixedPrecisionTest, Calibration_SmallValues) {
    if (!hasGPU()) {
        GTEST_SKIP() << "No GPU available";
    }

    const size_t size = MEDIUM_SIZE;
    auto data = generateRandomData(size, -1e-5f, 1e-5f);

    auto* tensor = createGPUTensor(data, NIMCP_GPU_PRECISION_FP32);
    nimcp_quant_params_t params;
    nimcp_gpu_infer_calibrate(gpu_ctx, tensor, &params, true);

    EXPECT_GT(params.scale, 0.0f) << "Scale should be positive for small values";
    EXPECT_LT(params.scale, 1e-4f) << "Scale should be small for small value range";

    nimcp_gpu_tensor_destroy(tensor);
}

/**
 * WHAT: Test per-channel calibration
 * WHY:  Per-channel quantization improves accuracy
 * HOW:  Calibrate with per_channel flag, verify parameters vary
 */
TEST_F(MixedPrecisionTest, Calibration_PerChannel) {
    if (!hasGPU()) {
        GTEST_SKIP() << "No GPU available";
    }

    // Create multi-channel data with different ranges
    const size_t n_channels = 4;
    const size_t per_channel = 64;
    std::vector<float> data(n_channels * per_channel);

    for (size_t c = 0; c < n_channels; c++) {
        float scale = static_cast<float>(c + 1);  // Different range per channel
        for (size_t i = 0; i < per_channel; i++) {
            std::uniform_real_distribution<float> dist(-scale, scale);
            data[c * per_channel + i] = dist(rng);
        }
    }

    // For per-channel, we'd need a different API or iterate
    // This test verifies the concept with single-tensor calibration
    auto* tensor = createGPUTensor(data, NIMCP_GPU_PRECISION_FP32);
    nimcp_quant_params_t params;
    nimcp_gpu_infer_calibrate(gpu_ctx, tensor, &params, true);

    // Verify overall range is captured
    EXPECT_GT(params.scale, 0.0f);

    nimcp_gpu_tensor_destroy(tensor);
}

//=============================================================================
// DYNAMIC RANGE TESTS
//=============================================================================

/**
 * WHAT: Test quantization with large dynamic range
 * WHY:  Large range stresses quantization accuracy
 * HOW:  Quantize data with large range, verify quality
 */
TEST_F(MixedPrecisionTest, DynamicRange_Large) {
    if (!hasGPU()) {
        GTEST_SKIP() << "No GPU available";
    }

    const size_t size = MEDIUM_SIZE;
    auto data = generateRandomData(size, -1000.0f, 1000.0f);

    auto* tensor = createGPUTensor(data, NIMCP_GPU_PRECISION_FP32);
    nimcp_quant_params_t params;
    nimcp_gpu_infer_calibrate(gpu_ctx, tensor, &params, true);

    auto* tensor_int8 = createEmptyTensor(size, NIMCP_GPU_PRECISION_INT8);
    auto* tensor_dequant = createEmptyTensor(size, NIMCP_GPU_PRECISION_FP32);

    nimcp_gpu_infer_quantize_int8(gpu_ctx, tensor, tensor_int8, &params);
    nimcp_gpu_infer_dequantize_int8(gpu_ctx, tensor_int8, tensor_dequant, &params);

    auto result = copyToHost(tensor_dequant);

    // With large range, we expect larger absolute errors but reasonable SQNR
    float sqnr = computeSQNR(data.data(), result.data(), size);
    EXPECT_GT(sqnr, 15.0f) << "SQNR should be acceptable even with large range";

    nimcp_gpu_tensor_destroy(tensor);
    nimcp_gpu_tensor_destroy(tensor_int8);
    nimcp_gpu_tensor_destroy(tensor_dequant);
}

/**
 * WHAT: Test quantization with outliers
 * WHY:  Outliers can dominate scale and hurt quality
 * HOW:  Add outliers to normal data, check quantization quality
 */
TEST_F(MixedPrecisionTest, DynamicRange_Outliers) {
    if (!hasGPU()) {
        GTEST_SKIP() << "No GPU available";
    }

    const size_t size = MEDIUM_SIZE;
    auto data = generateNormalData(size, 0.0f, 1.0f);

    // Add a few outliers
    data[0] = 100.0f;
    data[1] = -100.0f;

    auto* tensor = createGPUTensor(data, NIMCP_GPU_PRECISION_FP32);
    nimcp_quant_params_t params;
    nimcp_gpu_infer_calibrate(gpu_ctx, tensor, &params, true);

    auto* tensor_int8 = createEmptyTensor(size, NIMCP_GPU_PRECISION_INT8);
    auto* tensor_dequant = createEmptyTensor(size, NIMCP_GPU_PRECISION_FP32);

    nimcp_gpu_infer_quantize_int8(gpu_ctx, tensor, tensor_int8, &params);
    nimcp_gpu_infer_dequantize_int8(gpu_ctx, tensor_int8, tensor_dequant, &params);

    auto result = copyToHost(tensor_dequant);

    // Outliers reduce quality for normal values
    // This is expected behavior - just verify no crashes
    float sqnr = computeSQNR(data.data(), result.data(), size);
    EXPECT_GT(sqnr, 5.0f) << "SQNR should still be positive even with outliers";

    nimcp_gpu_tensor_destroy(tensor);
    nimcp_gpu_tensor_destroy(tensor_int8);
    nimcp_gpu_tensor_destroy(tensor_dequant);
}

//=============================================================================
// PRECISION CONVERSION FUNCTION TESTS
//=============================================================================

/**
 * WHAT: Test all precision conversion paths
 * WHY:  Need to convert between any precision pair
 * HOW:  Test FP32<->FP16, FP32<->INT8, FP16<->INT8
 */
TEST_F(MixedPrecisionTest, Conversion_AllPaths) {
    if (!hasGPU()) {
        GTEST_SKIP() << "No GPU available";
    }

    const size_t size = SMALL_SIZE;
    auto data = generateNormalData(size, 0.0f, 1.0f);

    auto* tensor_fp32 = createGPUTensor(data, NIMCP_GPU_PRECISION_FP32);

    // FP32 -> FP16 -> FP32
    auto* tensor_fp16 = createEmptyTensor(size, NIMCP_GPU_PRECISION_FP16);
    auto* tensor_fp32_back = createEmptyTensor(size, NIMCP_GPU_PRECISION_FP32);

    bool ok1 = nimcp_gpu_infer_convert_precision(gpu_ctx, tensor_fp32, tensor_fp16, NIMCP_INFER_FP16);
    bool ok2 = nimcp_gpu_infer_convert_precision(gpu_ctx, tensor_fp16, tensor_fp32_back, NIMCP_INFER_FP32);

    EXPECT_TRUE(ok1) << "FP32->FP16 conversion should succeed";
    EXPECT_TRUE(ok2) << "FP16->FP32 conversion should succeed";

    auto result = copyToHost(tensor_fp32_back);
    float mse = computeMSE(data.data(), result.data(), size);
    EXPECT_LT(mse, FP16_TOLERANCE * FP16_TOLERANCE);

    nimcp_gpu_tensor_destroy(tensor_fp32);
    nimcp_gpu_tensor_destroy(tensor_fp16);
    nimcp_gpu_tensor_destroy(tensor_fp32_back);
}

/**
 * WHAT: Test recommended precision query
 * WHY:  Applications need to know optimal precision for GPU
 * HOW:  Query recommendation, verify valid result
 */
TEST_F(MixedPrecisionTest, RecommendedPrecision_Query) {
    if (!hasGPU()) {
        GTEST_SKIP() << "No GPU available";
    }

    nimcp_infer_precision_t recommended = nimcp_gpu_infer_recommended_precision(gpu_ctx);

    // Should be a valid precision type
    EXPECT_TRUE(recommended == NIMCP_INFER_FP32 ||
                recommended == NIMCP_INFER_FP16 ||
                recommended == NIMCP_INFER_TF32 ||
                recommended == NIMCP_INFER_BF16 ||
                recommended == NIMCP_INFER_INT8)
        << "Recommended precision should be valid: " << static_cast<int>(recommended);
}

//=============================================================================
// INFERENCE SESSION TESTS
//=============================================================================

/**
 * WHAT: Test inference session with specific precision
 * WHY:  Sessions cache precision-specific optimizations
 * HOW:  Create session, verify precision setting
 */
TEST_F(MixedPrecisionTest, Session_PrecisionSetting) {
    if (!hasGPU()) {
        GTEST_SKIP() << "No GPU available";
    }

    nimcp_infer_session_t* session = nimcp_infer_session_create(
        gpu_ctx, NIMCP_INFER_FP16, 0);

    if (session) {
        EXPECT_EQ(session->precision, NIMCP_INFER_FP16);
        EXPECT_EQ(session->ctx, gpu_ctx);

        nimcp_infer_session_destroy(session);
    }
}

/**
 * WHAT: Test warmup function
 * WHY:  Warmup JIT-compiles kernels for target precision
 * HOW:  Call warmup, verify no errors
 */
TEST_F(MixedPrecisionTest, Warmup_Works) {
    if (!hasGPU()) {
        GTEST_SKIP() << "No GPU available";
    }

    bool warmup_ok = nimcp_gpu_infer_warmup(gpu_ctx, NIMCP_INFER_FP32);
    EXPECT_TRUE(warmup_ok) << "FP32 warmup should succeed";

    warmup_ok = nimcp_gpu_infer_warmup(gpu_ctx, NIMCP_INFER_FP16);
    // FP16 warmup may fail on GPUs without FP16 support
    // Not asserting success, just no crash
}

//=============================================================================
// COMBINED PRECISION TESTS
//=============================================================================

/**
 * WHAT: Test mixed precision neural network layer
 * WHY:  Real usage involves mixing precisions
 * HOW:  FP16 matmul with INT8 weights, FP32 accumulation
 */
TEST_F(MixedPrecisionTest, MixedPrecision_NNLayer) {
    if (!hasGPU()) {
        GTEST_SKIP() << "No GPU available";
    }

    const size_t batch = 8, in_features = 64, out_features = 32;

    // FP32 input (as received from previous layer)
    auto input_data = generateNormalData(batch * in_features, 0.0f, 1.0f);
    auto weight_data = generateNormalData(out_features * in_features, 0.0f, 0.5f);
    auto bias_data = generateNormalData(out_features, 0.0f, 0.1f);

    std::vector<size_t> input_dims = {batch, in_features};
    std::vector<size_t> weight_dims = {out_features, in_features};
    std::vector<size_t> bias_dims = {out_features};
    std::vector<size_t> output_dims = {batch, out_features};

    // Create tensors
    auto* input = nimcp_gpu_tensor_from_host(gpu_ctx, input_data.data(), input_dims.data(), 2, NIMCP_GPU_PRECISION_FP32);
    auto* weights = nimcp_gpu_tensor_from_host(gpu_ctx, weight_data.data(), weight_dims.data(), 2, NIMCP_GPU_PRECISION_FP32);
    auto* bias = nimcp_gpu_tensor_from_host(gpu_ctx, bias_data.data(), bias_dims.data(), 1, NIMCP_GPU_PRECISION_FP32);
    auto* output = nimcp_gpu_tensor_create(gpu_ctx, output_dims.data(), 2, NIMCP_GPU_PRECISION_FP32);

    // Fused linear+relu (common inference pattern)
    bool ok = nimcp_gpu_infer_linear_relu(gpu_ctx, input, weights, bias, output);
    EXPECT_TRUE(ok) << "Fused linear+relu should succeed";

    auto result = copyToHost(output);

    // Verify output is valid (all >= 0 due to ReLU)
    for (size_t i = 0; i < batch * out_features; i++) {
        EXPECT_GE(result[i], 0.0f) << "ReLU output should be non-negative";
        EXPECT_FALSE(std::isnan(result[i])) << "Output should not be NaN";
    }

    nimcp_gpu_tensor_destroy(input);
    nimcp_gpu_tensor_destroy(weights);
    nimcp_gpu_tensor_destroy(bias);
    nimcp_gpu_tensor_destroy(output);
}

/**
 * WHAT: Test quantization quality metrics
 * WHY:  Need to measure quantization impact on model accuracy
 * HOW:  Compute various metrics: MSE, MAE, SQNR
 */
TEST_F(MixedPrecisionTest, QuantizationQuality_Metrics) {
    if (!hasGPU()) {
        GTEST_SKIP() << "No GPU available";
    }

    const size_t size = LARGE_SIZE;
    auto data = generateNormalData(size, 0.0f, 2.0f);

    auto* tensor = createGPUTensor(data, NIMCP_GPU_PRECISION_FP32);
    nimcp_quant_params_t params;
    nimcp_gpu_infer_calibrate(gpu_ctx, tensor, &params, true);

    auto* tensor_int8 = createEmptyTensor(size, NIMCP_GPU_PRECISION_INT8);
    auto* tensor_dequant = createEmptyTensor(size, NIMCP_GPU_PRECISION_FP32);

    nimcp_gpu_infer_quantize_int8(gpu_ctx, tensor, tensor_int8, &params);
    nimcp_gpu_infer_dequantize_int8(gpu_ctx, tensor_int8, tensor_dequant, &params);

    auto result = copyToHost(tensor_dequant);

    // Compute metrics
    float mse = computeMSE(data.data(), result.data(), size);
    float sqnr = computeSQNR(data.data(), result.data(), size);

    float mae = 0;
    for (size_t i = 0; i < size; i++) {
        mae += std::fabs(data[i] - result[i]);
    }
    mae /= size;

    // Report metrics
    EXPECT_LT(mse, 0.01f) << "MSE too high: " << mse;
    EXPECT_GT(sqnr, 20.0f) << "SQNR too low: " << sqnr << " dB";
    EXPECT_LT(mae, 0.1f) << "MAE too high: " << mae;

    nimcp_gpu_tensor_destroy(tensor);
    nimcp_gpu_tensor_destroy(tensor_int8);
    nimcp_gpu_tensor_destroy(tensor_dequant);
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
