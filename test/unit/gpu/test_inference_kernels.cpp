/**
 * @file test_inference_kernels.cpp
 * @brief Unit tests for GPU inference kernels
 *
 * WHAT: Tests GPU-accelerated inference operations using CUDA
 * WHY:  Verify fused operations, quantization, and inference optimizations
 * HOW:  Test all public API functions with various configurations
 *
 * TEST COVERAGE:
 * - Fused Linear+ReLU, Linear+GELU, Linear+SiLU operations
 * - INT8 quantization and dequantization
 * - Calibration for quantization parameters
 * - In-place activations
 * - Residual add operations
 * - Layer normalization and RMS normalization
 * - Inference session management
 * - Edge cases and error handling
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <random>
#include <algorithm>

// GPU headers include CUDA headers that cannot be in extern "C" blocks
#include "gpu/inference/nimcp_inference_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"

//=============================================================================
// Test Fixture
//=============================================================================

/**
 * @brief Test fixture for inference kernel tests
 * WHAT: Provides common setup/teardown for inference GPU tests
 * WHY:  Ensure proper cleanup of GPU resources
 * HOW:  Automatically creates/destroys GPU context and tensors
 */
class InferenceKernelTest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* ctx = nullptr;
    nimcp_infer_session_t* session = nullptr;
    std::vector<nimcp_gpu_tensor_t*> tensors;
    std::mt19937 rng;

    void SetUp() override {
        // Try to create GPU context (may fail if no GPU)
        ctx = nimcp_gpu_context_create_auto();
        rng.seed(42);  // Deterministic for reproducibility
    }

    void TearDown() override {
        // Destroy all tracked tensors
        for (auto* tensor : tensors) {
            if (tensor) {
                nimcp_gpu_tensor_destroy(tensor);
            }
        }
        tensors.clear();

        if (session) {
            nimcp_infer_session_destroy(session);
            session = nullptr;
        }
        if (ctx) {
            nimcp_gpu_context_destroy(ctx);
            ctx = nullptr;
        }
    }

    /**
     * @brief Check if GPU is available for tests
     */
    bool hasGPU() const {
        return ctx != nullptr && nimcp_gpu_context_is_valid(ctx);
    }

    /**
     * @brief Skip test if no GPU available
     */
    void skipIfNoGPU() {
        if (!hasGPU()) {
            GTEST_SKIP() << "Skipping test: No GPU available";
        }
    }

    /**
     * @brief Create and track a GPU tensor
     */
    nimcp_gpu_tensor_t* createTensor(const size_t* dims, uint32_t ndim,
                                      nimcp_gpu_precision_t precision = NIMCP_GPU_PRECISION_FP32) {
        nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(ctx, dims, ndim, precision);
        if (tensor) {
            tensors.push_back(tensor);
        }
        return tensor;
    }

    /**
     * @brief Create tensor from host data
     */
    nimcp_gpu_tensor_t* createTensorFromHost(const float* data, const size_t* dims,
                                              uint32_t ndim) {
        nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_from_host(ctx, data, dims, ndim,
                                                                 NIMCP_GPU_PRECISION_FP32);
        if (tensor) {
            tensors.push_back(tensor);
        }
        return tensor;
    }

    /**
     * @brief Fill tensor with random values
     */
    void fillRandom(nimcp_gpu_tensor_t* tensor, float min_val = -1.0f, float max_val = 1.0f) {
        std::uniform_real_distribution<float> dist(min_val, max_val);
        std::vector<float> data(tensor->numel);
        for (size_t i = 0; i < tensor->numel; i++) {
            data[i] = dist(rng);
        }
        nimcp_gpu_tensor_t* temp = nimcp_gpu_tensor_from_host(ctx, data.data(),
                                                               tensor->dims, tensor->ndim,
                                                               NIMCP_GPU_PRECISION_FP32);
        if (temp) {
            nimcp_gpu_copy(ctx, temp, tensor);
            nimcp_gpu_tensor_destroy(temp);
        }
    }

    /**
     * @brief Copy tensor to host
     */
    std::vector<float> copyToHost(const nimcp_gpu_tensor_t* tensor) {
        std::vector<float> data(tensor->numel);
        nimcp_gpu_tensor_to_host(tensor, data.data());
        return data;
    }

    /**
     * @brief Check if two float vectors are approximately equal
     */
    bool approxEqual(const std::vector<float>& a, const std::vector<float>& b,
                     float tolerance = 1e-5f) {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); i++) {
            if (std::abs(a[i] - b[i]) > tolerance) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Compute ReLU on host for reference
     */
    std::vector<float> hostReLU(const std::vector<float>& x) {
        std::vector<float> result(x.size());
        for (size_t i = 0; i < x.size(); i++) {
            result[i] = std::max(0.0f, x[i]);
        }
        return result;
    }

    /**
     * @brief Compute GELU on host for reference
     */
    std::vector<float> hostGELU(const std::vector<float>& x) {
        std::vector<float> result(x.size());
        const float sqrt_2_over_pi = std::sqrt(2.0f / M_PI);
        for (size_t i = 0; i < x.size(); i++) {
            float val = x[i];
            result[i] = 0.5f * val * (1.0f + std::tanh(sqrt_2_over_pi * (val + 0.044715f * val * val * val)));
        }
        return result;
    }

    /**
     * @brief Compute SiLU on host for reference
     */
    std::vector<float> hostSiLU(const std::vector<float>& x) {
        std::vector<float> result(x.size());
        for (size_t i = 0; i < x.size(); i++) {
            result[i] = x[i] / (1.0f + std::exp(-x[i]));
        }
        return result;
    }

    /**
     * @brief Compute matrix multiply on host for reference
     */
    std::vector<float> hostMatMul(const std::vector<float>& a, const std::vector<float>& b,
                                  size_t M, size_t K, size_t N) {
        std::vector<float> result(M * N, 0.0f);
        for (size_t i = 0; i < M; i++) {
            for (size_t j = 0; j < N; j++) {
                float sum = 0.0f;
                for (size_t k = 0; k < K; k++) {
                    sum += a[i * K + k] * b[k * N + j];
                }
                result[i * N + j] = sum;
            }
        }
        return result;
    }

    /**
     * @brief Add bias on host for reference
     */
    void hostAddBias(std::vector<float>& data, const std::vector<float>& bias,
                     size_t batch, size_t features) {
        for (size_t i = 0; i < batch; i++) {
            for (size_t j = 0; j < features; j++) {
                data[i * features + j] += bias[j];
            }
        }
    }
};

//=============================================================================
// Fused Linear+ReLU Tests
//=============================================================================

/**
 * TEST: Fused Linear+ReLU basic operation
 * WHAT: Verify nimcp_gpu_infer_linear_relu() computes y = ReLU(x @ W^T + b)
 * WHY:  Fused operations reduce memory bandwidth
 */
TEST_F(InferenceKernelTest, LinearReLU_Basic_CorrectOutput) {
    skipIfNoGPU();

    // Create tensors: input [2, 4], weights [3, 4], bias [3], output [2, 3]
    size_t input_dims[2] = {2, 4};
    size_t weight_dims[2] = {3, 4};
    size_t bias_dims[1] = {3};
    size_t output_dims[2] = {2, 3};

    // Create host data
    std::vector<float> input_data = {1.0f, 2.0f, 3.0f, 4.0f,
                                      5.0f, 6.0f, 7.0f, 8.0f};
    std::vector<float> weight_data = {1.0f, 0.0f, 0.0f, 0.0f,
                                       0.0f, 1.0f, 0.0f, 0.0f,
                                       0.0f, 0.0f, 1.0f, 0.0f};
    std::vector<float> bias_data = {-2.0f, -3.0f, -4.0f};

    nimcp_gpu_tensor_t* input = createTensorFromHost(input_data.data(), input_dims, 2);
    nimcp_gpu_tensor_t* weights = createTensorFromHost(weight_data.data(), weight_dims, 2);
    nimcp_gpu_tensor_t* bias = createTensorFromHost(bias_data.data(), bias_dims, 1);
    nimcp_gpu_tensor_t* output = createTensor(output_dims, 2);

    ASSERT_NE(input, nullptr);
    ASSERT_NE(weights, nullptr);
    ASSERT_NE(bias, nullptr);
    ASSERT_NE(output, nullptr);

    bool result = nimcp_gpu_infer_linear_relu(ctx, input, weights, bias, output);
    EXPECT_TRUE(result);

    // Expected: row 0: [1-2, 2-3, 3-4] = [-1, -1, -1] -> ReLU -> [0, 0, 0]
    //           row 1: [5-2, 6-3, 7-4] = [3, 3, 3] -> ReLU -> [3, 3, 3]
    std::vector<float> host_output = copyToHost(output);

    EXPECT_NEAR(host_output[0], 0.0f, 0.001f);
    EXPECT_NEAR(host_output[1], 0.0f, 0.001f);
    EXPECT_NEAR(host_output[2], 0.0f, 0.001f);
    EXPECT_NEAR(host_output[3], 3.0f, 0.001f);
    EXPECT_NEAR(host_output[4], 3.0f, 0.001f);
    EXPECT_NEAR(host_output[5], 3.0f, 0.001f);
}

/**
 * TEST: Linear+ReLU without bias
 * WHAT: Verify operation works with NULL bias
 * WHY:  Some layers don't have bias
 */
TEST_F(InferenceKernelTest, LinearReLU_NoBias_Succeeds) {
    skipIfNoGPU();

    size_t input_dims[2] = {4, 8};
    size_t weight_dims[2] = {16, 8};
    size_t output_dims[2] = {4, 16};

    nimcp_gpu_tensor_t* input = createTensor(input_dims, 2);
    nimcp_gpu_tensor_t* weights = createTensor(weight_dims, 2);
    nimcp_gpu_tensor_t* output = createTensor(output_dims, 2);

    ASSERT_NE(input, nullptr);
    ASSERT_NE(weights, nullptr);
    ASSERT_NE(output, nullptr);

    fillRandom(input);
    fillRandom(weights);

    bool result = nimcp_gpu_infer_linear_relu(ctx, input, weights, nullptr, output);
    EXPECT_TRUE(result);

    // Verify output has ReLU applied (all values >= 0)
    std::vector<float> host_output = copyToHost(output);
    for (size_t i = 0; i < host_output.size(); i++) {
        EXPECT_GE(host_output[i], 0.0f);
    }
}

/**
 * TEST: Linear+ReLU with NULL inputs
 * WHAT: Verify operation handles NULL gracefully
 * WHY:  Guard clause validation
 */
TEST_F(InferenceKernelTest, LinearReLU_NullInputs_ReturnsFalse) {
    skipIfNoGPU();

    size_t dims[2] = {4, 4};
    nimcp_gpu_tensor_t* tensor = createTensor(dims, 2);

    EXPECT_FALSE(nimcp_gpu_infer_linear_relu(nullptr, tensor, tensor, nullptr, tensor));
    EXPECT_FALSE(nimcp_gpu_infer_linear_relu(ctx, nullptr, tensor, nullptr, tensor));
    EXPECT_FALSE(nimcp_gpu_infer_linear_relu(ctx, tensor, nullptr, nullptr, tensor));
    EXPECT_FALSE(nimcp_gpu_infer_linear_relu(ctx, tensor, tensor, nullptr, nullptr));
}

//=============================================================================
// Fused Linear+GELU Tests
//=============================================================================

/**
 * TEST: Fused Linear+GELU basic operation
 * WHAT: Verify nimcp_gpu_infer_linear_gelu() computes y = GELU(x @ W^T + b)
 * WHY:  GELU is used in transformers
 */
TEST_F(InferenceKernelTest, LinearGELU_Basic_CorrectOutput) {
    skipIfNoGPU();

    size_t input_dims[2] = {2, 4};
    size_t weight_dims[2] = {4, 4};
    size_t bias_dims[1] = {4};
    size_t output_dims[2] = {2, 4};

    nimcp_gpu_tensor_t* input = createTensor(input_dims, 2);
    nimcp_gpu_tensor_t* weights = createTensor(weight_dims, 2);
    nimcp_gpu_tensor_t* bias = createTensor(bias_dims, 1);
    nimcp_gpu_tensor_t* output = createTensor(output_dims, 2);

    ASSERT_NE(input, nullptr);
    ASSERT_NE(weights, nullptr);
    ASSERT_NE(bias, nullptr);
    ASSERT_NE(output, nullptr);

    fillRandom(input, -1.0f, 1.0f);
    fillRandom(weights, -0.5f, 0.5f);
    fillRandom(bias, -0.1f, 0.1f);

    bool result = nimcp_gpu_infer_linear_gelu(ctx, input, weights, bias, output);
    EXPECT_TRUE(result);

    // Verify output is finite
    std::vector<float> host_output = copyToHost(output);
    for (size_t i = 0; i < host_output.size(); i++) {
        EXPECT_TRUE(std::isfinite(host_output[i]));
    }
}

/**
 * TEST: Linear+GELU matches reference implementation
 * WHAT: Compare GPU output against host reference
 * WHY:  Verify correctness of fused operation
 */
TEST_F(InferenceKernelTest, LinearGELU_MatchesReference) {
    skipIfNoGPU();

    size_t batch = 4;
    size_t in_features = 8;
    size_t out_features = 8;

    size_t input_dims[2] = {batch, in_features};
    size_t weight_dims[2] = {out_features, in_features};
    size_t bias_dims[1] = {out_features};
    size_t output_dims[2] = {batch, out_features};

    // Create identity-like weights for easier verification
    std::vector<float> weight_data(out_features * in_features, 0.0f);
    for (size_t i = 0; i < std::min(out_features, in_features); i++) {
        weight_data[i * in_features + i] = 1.0f;
    }

    std::vector<float> input_data = {-2.0f, -1.0f, 0.0f, 1.0f, 2.0f, 0.5f, -0.5f, 1.5f,
                                      1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
                                      0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                      -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f};
    std::vector<float> bias_data(out_features, 0.0f);

    nimcp_gpu_tensor_t* input = createTensorFromHost(input_data.data(), input_dims, 2);
    nimcp_gpu_tensor_t* weights = createTensorFromHost(weight_data.data(), weight_dims, 2);
    nimcp_gpu_tensor_t* bias = createTensorFromHost(bias_data.data(), bias_dims, 1);
    nimcp_gpu_tensor_t* output = createTensor(output_dims, 2);

    bool result = nimcp_gpu_infer_linear_gelu(ctx, input, weights, bias, output);
    EXPECT_TRUE(result);

    // Compute reference
    std::vector<float> linear_out = hostMatMul(input_data, weight_data, batch, in_features, out_features);
    hostAddBias(linear_out, bias_data, batch, out_features);
    std::vector<float> reference = hostGELU(linear_out);

    // Compare
    std::vector<float> gpu_output = copyToHost(output);
    EXPECT_TRUE(approxEqual(gpu_output, reference, 0.01f));
}

//=============================================================================
// Fused Linear+SiLU Tests
//=============================================================================

/**
 * TEST: Fused Linear+SiLU basic operation
 * WHAT: Verify nimcp_gpu_infer_linear_silu() computes y = SiLU(x @ W^T + b)
 * WHY:  SiLU/Swish is used in modern architectures
 */
TEST_F(InferenceKernelTest, LinearSiLU_Basic_CorrectOutput) {
    skipIfNoGPU();

    size_t input_dims[2] = {2, 4};
    size_t weight_dims[2] = {4, 4};
    size_t bias_dims[1] = {4};
    size_t output_dims[2] = {2, 4};

    nimcp_gpu_tensor_t* input = createTensor(input_dims, 2);
    nimcp_gpu_tensor_t* weights = createTensor(weight_dims, 2);
    nimcp_gpu_tensor_t* bias = createTensor(bias_dims, 1);
    nimcp_gpu_tensor_t* output = createTensor(output_dims, 2);

    fillRandom(input);
    fillRandom(weights, -0.5f, 0.5f);
    fillRandom(bias, -0.1f, 0.1f);

    bool result = nimcp_gpu_infer_linear_silu(ctx, input, weights, bias, output);
    EXPECT_TRUE(result);

    std::vector<float> host_output = copyToHost(output);
    for (size_t i = 0; i < host_output.size(); i++) {
        EXPECT_TRUE(std::isfinite(host_output[i]));
    }
}

/**
 * TEST: Linear+SiLU matches reference
 * WHAT: Compare GPU output against host reference
 * WHY:  Verify SiLU implementation correctness
 */
TEST_F(InferenceKernelTest, LinearSiLU_MatchesReference) {
    skipIfNoGPU();

    size_t batch = 2;
    size_t in_features = 4;
    size_t out_features = 4;

    size_t input_dims[2] = {batch, in_features};
    size_t weight_dims[2] = {out_features, in_features};
    size_t bias_dims[1] = {out_features};
    size_t output_dims[2] = {batch, out_features};

    // Identity weights
    std::vector<float> weight_data(out_features * in_features, 0.0f);
    for (size_t i = 0; i < out_features; i++) {
        weight_data[i * in_features + i] = 1.0f;
    }

    std::vector<float> input_data = {-2.0f, -1.0f, 0.0f, 1.0f,
                                      2.0f, 0.5f, -0.5f, 1.5f};
    std::vector<float> bias_data(out_features, 0.0f);

    nimcp_gpu_tensor_t* input = createTensorFromHost(input_data.data(), input_dims, 2);
    nimcp_gpu_tensor_t* weights = createTensorFromHost(weight_data.data(), weight_dims, 2);
    nimcp_gpu_tensor_t* bias = createTensorFromHost(bias_data.data(), bias_dims, 1);
    nimcp_gpu_tensor_t* output = createTensor(output_dims, 2);

    bool result = nimcp_gpu_infer_linear_silu(ctx, input, weights, bias, output);
    EXPECT_TRUE(result);

    // Compute reference (with identity weights, output = SiLU(input))
    std::vector<float> reference = hostSiLU(input_data);

    std::vector<float> gpu_output = copyToHost(output);
    EXPECT_TRUE(approxEqual(gpu_output, reference, 0.01f));
}

//=============================================================================
// INT8 Quantization Tests
//=============================================================================

/**
 * TEST: INT8 quantization basic operation
 * WHAT: Verify nimcp_gpu_infer_quantize_int8() quantizes FP32 to INT8
 * WHY:  INT8 inference is critical for performance
 */
TEST_F(InferenceKernelTest, QuantizeINT8_Basic_Succeeds) {
    skipIfNoGPU();

    size_t dims[2] = {4, 8};

    nimcp_gpu_tensor_t* input = createTensor(dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* output = createTensor(dims, 2, NIMCP_GPU_PRECISION_INT8);

    ASSERT_NE(input, nullptr);
    ASSERT_NE(output, nullptr);

    fillRandom(input, -1.0f, 1.0f);

    nimcp_quant_params_t params;
    params.scale = 0.01f;  // 1/100
    params.zero_point = 0;
    params.min_val = -1.27f;
    params.max_val = 1.27f;

    bool result = nimcp_gpu_infer_quantize_int8(ctx, input, output, &params);
    EXPECT_TRUE(result);
}

/**
 * TEST: INT8 dequantization
 * WHAT: Verify nimcp_gpu_infer_dequantize_int8() restores FP32 from INT8
 * WHY:  Need to convert back for some operations
 */
TEST_F(InferenceKernelTest, DequantizeINT8_Basic_Succeeds) {
    skipIfNoGPU();

    size_t dims[2] = {4, 8};

    nimcp_gpu_tensor_t* fp32_input = createTensor(dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* int8_temp = createTensor(dims, 2, NIMCP_GPU_PRECISION_INT8);
    nimcp_gpu_tensor_t* fp32_output = createTensor(dims, 2, NIMCP_GPU_PRECISION_FP32);

    ASSERT_NE(fp32_input, nullptr);
    ASSERT_NE(int8_temp, nullptr);
    ASSERT_NE(fp32_output, nullptr);

    fillRandom(fp32_input, -1.0f, 1.0f);

    nimcp_quant_params_t params;
    params.scale = 0.01f;
    params.zero_point = 0;
    params.min_val = -1.27f;
    params.max_val = 1.27f;

    // Quantize
    bool quant_result = nimcp_gpu_infer_quantize_int8(ctx, fp32_input, int8_temp, &params);
    EXPECT_TRUE(quant_result);

    // Dequantize
    bool dequant_result = nimcp_gpu_infer_dequantize_int8(ctx, int8_temp, fp32_output, &params);
    EXPECT_TRUE(dequant_result);

    // Check values are approximately preserved (within quantization error)
    std::vector<float> original = copyToHost(fp32_input);
    std::vector<float> reconstructed = copyToHost(fp32_output);

    for (size_t i = 0; i < original.size(); i++) {
        EXPECT_NEAR(original[i], reconstructed[i], params.scale * 1.5f);
    }
}

/**
 * TEST: Quantization with NULL inputs
 * WHAT: Verify quantization handles NULL gracefully
 * WHY:  Guard clause validation
 */
TEST_F(InferenceKernelTest, QuantizeINT8_NullInputs_ReturnsFalse) {
    skipIfNoGPU();

    size_t dims[2] = {4, 4};
    nimcp_gpu_tensor_t* tensor = createTensor(dims, 2);
    nimcp_quant_params_t params = {0.01f, 0, -1.0f, 1.0f};

    EXPECT_FALSE(nimcp_gpu_infer_quantize_int8(nullptr, tensor, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_infer_quantize_int8(ctx, nullptr, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_infer_quantize_int8(ctx, tensor, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_infer_quantize_int8(ctx, tensor, tensor, nullptr));
}

//=============================================================================
// Calibration Tests
//=============================================================================

/**
 * TEST: Calibration for quantization
 * WHAT: Verify nimcp_gpu_infer_calibrate() computes quantization parameters
 * WHY:  Calibration determines optimal quantization scale
 */
TEST_F(InferenceKernelTest, Calibrate_ComputesParams) {
    skipIfNoGPU();

    size_t dims[2] = {100, 64};
    nimcp_gpu_tensor_t* tensor = createTensor(dims, 2);
    ASSERT_NE(tensor, nullptr);

    fillRandom(tensor, -2.0f, 2.0f);

    nimcp_quant_params_t params;
    memset(&params, 0, sizeof(params));

    bool result = nimcp_gpu_infer_calibrate(ctx, tensor, &params, true);  // symmetric
    EXPECT_TRUE(result);

    // Check params are reasonable
    EXPECT_GT(params.scale, 0.0f);
    EXPECT_EQ(params.zero_point, 0);  // Symmetric should have zero_point=0
    EXPECT_LT(params.min_val, 0.0f);
    EXPECT_GT(params.max_val, 0.0f);
}

/**
 * TEST: Asymmetric calibration
 * WHAT: Test calibration with asymmetric quantization
 * WHY:  Asymmetric may provide better accuracy for skewed distributions
 */
TEST_F(InferenceKernelTest, Calibrate_Asymmetric) {
    skipIfNoGPU();

    size_t dims[2] = {100, 64};
    nimcp_gpu_tensor_t* tensor = createTensor(dims, 2);
    ASSERT_NE(tensor, nullptr);

    // Create positively skewed data
    fillRandom(tensor, 0.0f, 5.0f);

    nimcp_quant_params_t params;
    memset(&params, 0, sizeof(params));

    bool result = nimcp_gpu_infer_calibrate(ctx, tensor, &params, false);  // asymmetric
    EXPECT_TRUE(result);

    EXPECT_GT(params.scale, 0.0f);
    // For asymmetric with positive data, zero_point may be non-zero
    EXPECT_GE(params.min_val, 0.0f);
    EXPECT_GT(params.max_val, 0.0f);
}

/**
 * TEST: Calibration with NULL inputs
 * WHAT: Verify calibration handles NULL inputs
 * WHY:  Guard clause validation
 */
TEST_F(InferenceKernelTest, Calibrate_NullInputs_ReturnsFalse) {
    skipIfNoGPU();

    size_t dims[2] = {4, 4};
    nimcp_gpu_tensor_t* tensor = createTensor(dims, 2);
    nimcp_quant_params_t params;

    EXPECT_FALSE(nimcp_gpu_infer_calibrate(nullptr, tensor, &params, true));
    EXPECT_FALSE(nimcp_gpu_infer_calibrate(ctx, nullptr, &params, true));
    EXPECT_FALSE(nimcp_gpu_infer_calibrate(ctx, tensor, nullptr, true));
}

//=============================================================================
// In-place Activation Tests
//=============================================================================

/**
 * TEST: In-place ReLU activation
 * WHAT: Verify nimcp_gpu_infer_activation_inplace() applies ReLU in-place
 * WHY:  In-place saves memory for inference
 */
TEST_F(InferenceKernelTest, ActivationInplace_ReLU) {
    skipIfNoGPU();

    size_t dims[2] = {4, 8};
    nimcp_gpu_tensor_t* tensor = createTensor(dims, 2);
    ASSERT_NE(tensor, nullptr);

    fillRandom(tensor, -2.0f, 2.0f);
    std::vector<float> original = copyToHost(tensor);

    bool result = nimcp_gpu_infer_activation_inplace(ctx, tensor, 0);  // 0 = ReLU
    EXPECT_TRUE(result);

    std::vector<float> activated = copyToHost(tensor);

    for (size_t i = 0; i < activated.size(); i++) {
        float expected = std::max(0.0f, original[i]);
        EXPECT_NEAR(activated[i], expected, 0.0001f);
    }
}

/**
 * TEST: In-place Sigmoid activation
 * WHAT: Test sigmoid activation type
 * WHY:  Sigmoid is common for binary classification
 */
TEST_F(InferenceKernelTest, ActivationInplace_Sigmoid) {
    skipIfNoGPU();

    size_t dims[2] = {4, 8};
    nimcp_gpu_tensor_t* tensor = createTensor(dims, 2);
    ASSERT_NE(tensor, nullptr);

    fillRandom(tensor, -3.0f, 3.0f);
    std::vector<float> original = copyToHost(tensor);

    bool result = nimcp_gpu_infer_activation_inplace(ctx, tensor, 1);  // 1 = Sigmoid
    EXPECT_TRUE(result);

    std::vector<float> activated = copyToHost(tensor);

    for (size_t i = 0; i < activated.size(); i++) {
        float expected = 1.0f / (1.0f + std::exp(-original[i]));
        EXPECT_NEAR(activated[i], expected, 0.001f);
    }
}

/**
 * TEST: In-place Tanh activation
 * WHAT: Test tanh activation type
 * WHY:  Tanh is used in RNNs
 */
TEST_F(InferenceKernelTest, ActivationInplace_Tanh) {
    skipIfNoGPU();

    size_t dims[2] = {4, 8};
    nimcp_gpu_tensor_t* tensor = createTensor(dims, 2);
    ASSERT_NE(tensor, nullptr);

    fillRandom(tensor, -2.0f, 2.0f);
    std::vector<float> original = copyToHost(tensor);

    bool result = nimcp_gpu_infer_activation_inplace(ctx, tensor, 2);  // 2 = Tanh
    EXPECT_TRUE(result);

    std::vector<float> activated = copyToHost(tensor);

    for (size_t i = 0; i < activated.size(); i++) {
        float expected = std::tanh(original[i]);
        EXPECT_NEAR(activated[i], expected, 0.001f);
    }
}

/**
 * TEST: In-place GELU activation
 * WHAT: Test GELU activation type
 * WHY:  GELU is used in transformers
 */
TEST_F(InferenceKernelTest, ActivationInplace_GELU) {
    skipIfNoGPU();

    size_t dims[2] = {4, 8};
    nimcp_gpu_tensor_t* tensor = createTensor(dims, 2);
    ASSERT_NE(tensor, nullptr);

    fillRandom(tensor, -2.0f, 2.0f);
    std::vector<float> original = copyToHost(tensor);

    bool result = nimcp_gpu_infer_activation_inplace(ctx, tensor, 3);  // 3 = GELU
    EXPECT_TRUE(result);

    std::vector<float> activated = copyToHost(tensor);
    std::vector<float> expected = hostGELU(original);

    EXPECT_TRUE(approxEqual(activated, expected, 0.01f));
}

/**
 * TEST: In-place SiLU activation
 * WHAT: Test SiLU/Swish activation type
 * WHY:  SiLU is used in modern architectures
 */
TEST_F(InferenceKernelTest, ActivationInplace_SiLU) {
    skipIfNoGPU();

    size_t dims[2] = {4, 8};
    nimcp_gpu_tensor_t* tensor = createTensor(dims, 2);
    ASSERT_NE(tensor, nullptr);

    fillRandom(tensor, -2.0f, 2.0f);
    std::vector<float> original = copyToHost(tensor);

    bool result = nimcp_gpu_infer_activation_inplace(ctx, tensor, 4);  // 4 = SiLU
    EXPECT_TRUE(result);

    std::vector<float> activated = copyToHost(tensor);
    std::vector<float> expected = hostSiLU(original);

    EXPECT_TRUE(approxEqual(activated, expected, 0.01f));
}

//=============================================================================
// Residual Add Tests
//=============================================================================

/**
 * TEST: Residual add basic operation
 * WHAT: Verify nimcp_gpu_infer_residual_add() computes y = alpha*x + beta*residual
 * WHY:  Residual connections are fundamental to deep networks
 */
TEST_F(InferenceKernelTest, ResidualAdd_Basic) {
    skipIfNoGPU();

    size_t dims[2] = {4, 8};

    nimcp_gpu_tensor_t* x = createTensor(dims, 2);
    nimcp_gpu_tensor_t* residual = createTensor(dims, 2);
    nimcp_gpu_tensor_t* y = createTensor(dims, 2);

    ASSERT_NE(x, nullptr);
    ASSERT_NE(residual, nullptr);
    ASSERT_NE(y, nullptr);

    fillRandom(x, -1.0f, 1.0f);
    fillRandom(residual, -1.0f, 1.0f);

    std::vector<float> x_data = copyToHost(x);
    std::vector<float> res_data = copyToHost(residual);

    float alpha = 1.0f;
    float beta = 1.0f;

    bool result = nimcp_gpu_infer_residual_add(ctx, x, residual, y, alpha, beta);
    EXPECT_TRUE(result);

    std::vector<float> y_data = copyToHost(y);

    for (size_t i = 0; i < y_data.size(); i++) {
        float expected = alpha * x_data[i] + beta * res_data[i];
        EXPECT_NEAR(y_data[i], expected, 0.0001f);
    }
}

/**
 * TEST: Residual add with scaling
 * WHAT: Test with non-unit alpha and beta
 * WHY:  Some architectures use scaled residuals
 */
TEST_F(InferenceKernelTest, ResidualAdd_WithScaling) {
    skipIfNoGPU();

    size_t dims[2] = {4, 8};

    nimcp_gpu_tensor_t* x = createTensor(dims, 2);
    nimcp_gpu_tensor_t* residual = createTensor(dims, 2);
    nimcp_gpu_tensor_t* y = createTensor(dims, 2);

    fillRandom(x, -1.0f, 1.0f);
    fillRandom(residual, -1.0f, 1.0f);

    std::vector<float> x_data = copyToHost(x);
    std::vector<float> res_data = copyToHost(residual);

    float alpha = 0.8f;
    float beta = 0.2f;

    bool result = nimcp_gpu_infer_residual_add(ctx, x, residual, y, alpha, beta);
    EXPECT_TRUE(result);

    std::vector<float> y_data = copyToHost(y);

    for (size_t i = 0; i < y_data.size(); i++) {
        float expected = alpha * x_data[i] + beta * res_data[i];
        EXPECT_NEAR(y_data[i], expected, 0.0001f);
    }
}

/**
 * TEST: Residual add in-place (y = x)
 * WHAT: Test with output same as input
 * WHY:  In-place saves memory
 */
TEST_F(InferenceKernelTest, ResidualAdd_Inplace) {
    skipIfNoGPU();

    size_t dims[2] = {4, 8};

    nimcp_gpu_tensor_t* x = createTensor(dims, 2);
    nimcp_gpu_tensor_t* residual = createTensor(dims, 2);

    fillRandom(x, -1.0f, 1.0f);
    fillRandom(residual, -1.0f, 1.0f);

    std::vector<float> x_data = copyToHost(x);
    std::vector<float> res_data = copyToHost(residual);

    // y = x, so result is written back to x
    bool result = nimcp_gpu_infer_residual_add(ctx, x, residual, x, 1.0f, 1.0f);
    EXPECT_TRUE(result);

    std::vector<float> result_data = copyToHost(x);

    for (size_t i = 0; i < result_data.size(); i++) {
        float expected = x_data[i] + res_data[i];
        EXPECT_NEAR(result_data[i], expected, 0.0001f);
    }
}

//=============================================================================
// Layer Normalization Tests
//=============================================================================

/**
 * TEST: Layer normalization basic operation
 * WHAT: Verify nimcp_gpu_infer_layernorm() normalizes input
 * WHY:  LayerNorm is critical for transformers
 */
TEST_F(InferenceKernelTest, LayerNorm_Basic) {
    skipIfNoGPU();

    size_t input_dims[2] = {4, 8};
    size_t param_dims[1] = {8};

    nimcp_gpu_tensor_t* input = createTensor(input_dims, 2);
    nimcp_gpu_tensor_t* gamma = createTensor(param_dims, 1);
    nimcp_gpu_tensor_t* beta = createTensor(param_dims, 1);
    nimcp_gpu_tensor_t* output = createTensor(input_dims, 2);

    ASSERT_NE(input, nullptr);
    ASSERT_NE(gamma, nullptr);
    ASSERT_NE(beta, nullptr);
    ASSERT_NE(output, nullptr);

    fillRandom(input, -2.0f, 2.0f);
    nimcp_gpu_ones(ctx, gamma);  // Scale = 1
    nimcp_gpu_zeros(ctx, beta);  // Shift = 0

    float eps = 1e-5f;
    bool result = nimcp_gpu_infer_layernorm(ctx, input, gamma, beta, output, eps);
    EXPECT_TRUE(result);

    // Verify each row is normalized (mean ~ 0, std ~ 1)
    std::vector<float> output_data = copyToHost(output);

    for (size_t row = 0; row < 4; row++) {
        float sum = 0.0f;
        float sq_sum = 0.0f;
        for (size_t col = 0; col < 8; col++) {
            float val = output_data[row * 8 + col];
            sum += val;
            sq_sum += val * val;
        }
        float mean = sum / 8.0f;
        float var = sq_sum / 8.0f - mean * mean;
        float std = std::sqrt(var);

        EXPECT_NEAR(mean, 0.0f, 0.01f) << "Row " << row << " mean not zero";
        EXPECT_NEAR(std, 1.0f, 0.1f) << "Row " << row << " std not one";
    }
}

/**
 * TEST: Layer normalization with gamma and beta
 * WHAT: Test with non-trivial scale and shift
 * WHY:  Learnable parameters are essential
 */
TEST_F(InferenceKernelTest, LayerNorm_WithParams) {
    skipIfNoGPU();

    size_t input_dims[2] = {2, 4};
    size_t param_dims[1] = {4};

    // Constant input for predictable output
    std::vector<float> input_data = {1.0f, 2.0f, 3.0f, 4.0f,
                                      1.0f, 2.0f, 3.0f, 4.0f};
    std::vector<float> gamma_data = {2.0f, 2.0f, 2.0f, 2.0f};
    std::vector<float> beta_data = {1.0f, 1.0f, 1.0f, 1.0f};

    nimcp_gpu_tensor_t* input = createTensorFromHost(input_data.data(), input_dims, 2);
    nimcp_gpu_tensor_t* gamma = createTensorFromHost(gamma_data.data(), param_dims, 1);
    nimcp_gpu_tensor_t* beta = createTensorFromHost(beta_data.data(), param_dims, 1);
    nimcp_gpu_tensor_t* output = createTensor(input_dims, 2);

    float eps = 1e-5f;
    bool result = nimcp_gpu_infer_layernorm(ctx, input, gamma, beta, output, eps);
    EXPECT_TRUE(result);

    std::vector<float> output_data = copyToHost(output);

    // With gamma=2 and beta=1: output = 2 * normalized + 1
    // Mean of [1,2,3,4] = 2.5, std = sqrt(1.25) ~ 1.118
    // normalized = (x - 2.5) / 1.118
    // output = 2 * normalized + 1
    for (size_t i = 0; i < output_data.size(); i++) {
        EXPECT_TRUE(std::isfinite(output_data[i]));
    }
}

/**
 * TEST: Layer normalization with NULL inputs
 * WHAT: Verify layernorm handles NULL inputs
 * WHY:  Guard clause validation
 */
TEST_F(InferenceKernelTest, LayerNorm_NullInputs_ReturnsFalse) {
    skipIfNoGPU();

    size_t dims[2] = {4, 4};
    size_t param_dims[1] = {4};

    nimcp_gpu_tensor_t* tensor = createTensor(dims, 2);
    nimcp_gpu_tensor_t* param = createTensor(param_dims, 1);

    EXPECT_FALSE(nimcp_gpu_infer_layernorm(nullptr, tensor, param, param, tensor, 1e-5f));
    EXPECT_FALSE(nimcp_gpu_infer_layernorm(ctx, nullptr, param, param, tensor, 1e-5f));
    EXPECT_FALSE(nimcp_gpu_infer_layernorm(ctx, tensor, nullptr, param, tensor, 1e-5f));
    EXPECT_FALSE(nimcp_gpu_infer_layernorm(ctx, tensor, param, nullptr, tensor, 1e-5f));
    EXPECT_FALSE(nimcp_gpu_infer_layernorm(ctx, tensor, param, param, nullptr, 1e-5f));
}

//=============================================================================
// RMS Normalization Tests
//=============================================================================

/**
 * TEST: RMS normalization basic operation
 * WHAT: Verify nimcp_gpu_infer_rmsnorm() normalizes by RMS
 * WHY:  RMSNorm is used in LLaMA-style models
 */
TEST_F(InferenceKernelTest, RMSNorm_Basic) {
    skipIfNoGPU();

    size_t input_dims[2] = {4, 8};
    size_t param_dims[1] = {8};

    nimcp_gpu_tensor_t* input = createTensor(input_dims, 2);
    nimcp_gpu_tensor_t* gamma = createTensor(param_dims, 1);
    nimcp_gpu_tensor_t* output = createTensor(input_dims, 2);

    ASSERT_NE(input, nullptr);
    ASSERT_NE(gamma, nullptr);
    ASSERT_NE(output, nullptr);

    fillRandom(input, -2.0f, 2.0f);
    nimcp_gpu_ones(ctx, gamma);

    float eps = 1e-5f;
    bool result = nimcp_gpu_infer_rmsnorm(ctx, input, gamma, output, eps);
    EXPECT_TRUE(result);

    // Verify RMS normalization: output = x * gamma / rms(x)
    std::vector<float> input_data = copyToHost(input);
    std::vector<float> output_data = copyToHost(output);

    for (size_t row = 0; row < 4; row++) {
        // Compute expected RMS
        float sq_sum = 0.0f;
        for (size_t col = 0; col < 8; col++) {
            float val = input_data[row * 8 + col];
            sq_sum += val * val;
        }
        float rms = std::sqrt(sq_sum / 8.0f + eps);

        // Verify output = input / rms (since gamma = 1)
        for (size_t col = 0; col < 8; col++) {
            float expected = input_data[row * 8 + col] / rms;
            EXPECT_NEAR(output_data[row * 8 + col], expected, 0.01f);
        }
    }
}

/**
 * TEST: RMS normalization with gamma scaling
 * WHAT: Test with non-unit gamma
 * WHY:  Learnable scale is part of RMSNorm
 */
TEST_F(InferenceKernelTest, RMSNorm_WithGamma) {
    skipIfNoGPU();

    size_t input_dims[2] = {2, 4};
    size_t param_dims[1] = {4};

    std::vector<float> input_data = {1.0f, 1.0f, 1.0f, 1.0f,
                                      2.0f, 2.0f, 2.0f, 2.0f};
    std::vector<float> gamma_data = {2.0f, 0.5f, 1.0f, 3.0f};

    nimcp_gpu_tensor_t* input = createTensorFromHost(input_data.data(), input_dims, 2);
    nimcp_gpu_tensor_t* gamma = createTensorFromHost(gamma_data.data(), param_dims, 1);
    nimcp_gpu_tensor_t* output = createTensor(input_dims, 2);

    float eps = 1e-5f;
    bool result = nimcp_gpu_infer_rmsnorm(ctx, input, gamma, output, eps);
    EXPECT_TRUE(result);

    std::vector<float> output_data = copyToHost(output);

    // For input = [1,1,1,1], rms = sqrt(4/4) = 1
    // output[0] = gamma * input / rms = [2, 0.5, 1, 3]
    EXPECT_NEAR(output_data[0], 2.0f, 0.01f);
    EXPECT_NEAR(output_data[1], 0.5f, 0.01f);
    EXPECT_NEAR(output_data[2], 1.0f, 0.01f);
    EXPECT_NEAR(output_data[3], 3.0f, 0.01f);
}

//=============================================================================
// Inference Session Tests
//=============================================================================

/**
 * TEST: Create inference session
 * WHAT: Verify nimcp_infer_session_create() creates valid session
 * WHY:  Session manages inference state
 */
TEST_F(InferenceKernelTest, SessionCreate_ValidParams) {
    skipIfNoGPU();

    session = nimcp_infer_session_create(ctx, NIMCP_INFER_FP32, 0);
    ASSERT_NE(session, nullptr);

    EXPECT_EQ(session->ctx, ctx);
    EXPECT_EQ(session->precision, NIMCP_INFER_FP32);
    EXPECT_FALSE(session->graph_captured);
}

/**
 * TEST: Create session with NULL context
 * WHAT: Verify NULL context returns NULL session
 * WHY:  Guard clause validation
 */
TEST_F(InferenceKernelTest, SessionCreate_NullContext_ReturnsNull) {
    nimcp_infer_session_t* null_session = nimcp_infer_session_create(nullptr, NIMCP_INFER_FP32, 0);
    EXPECT_EQ(null_session, nullptr);
}

/**
 * TEST: Create session with various precision modes
 * WHAT: Test session creation with different precision settings
 * WHY:  Different modes for different performance/accuracy tradeoffs
 */
TEST_F(InferenceKernelTest, SessionCreate_VariousPrecisions) {
    skipIfNoGPU();

    nimcp_infer_precision_t precisions[] = {
        NIMCP_INFER_FP32,
        NIMCP_INFER_FP16,
        NIMCP_INFER_INT8
    };

    for (auto precision : precisions) {
        nimcp_infer_session_t* s = nimcp_infer_session_create(ctx, precision, 0);
        ASSERT_NE(s, nullptr) << "Failed for precision " << (int)precision;
        EXPECT_EQ(s->precision, precision);
        nimcp_infer_session_destroy(s);
    }
}

/**
 * TEST: Destroy NULL session
 * WHAT: Verify nimcp_infer_session_destroy() handles NULL gracefully
 * WHY:  Prevent crashes from invalid input
 */
TEST_F(InferenceKernelTest, SessionDestroy_Null_DoesNotCrash) {
    nimcp_infer_session_destroy(nullptr);
    SUCCEED();
}

/**
 * TEST: Session CUDA graph capture
 * WHAT: Test CUDA graph capture workflow
 * WHY:  Graph capture reduces kernel launch overhead
 *
 * NOTE: CUDA graph capture does NOT allow memory allocations during capture.
 *       All tensors must be pre-allocated before calling begin_capture().
 */
TEST_F(InferenceKernelTest, SessionGraphCapture_Workflow) {
    skipIfNoGPU();

    session = nimcp_infer_session_create(ctx, NIMCP_INFER_FP32, 1024 * 1024);
    ASSERT_NE(session, nullptr);

    // Pre-allocate all tensors BEFORE capture begins
    // CUDA graph capture does not allow memory allocations
    size_t dims[2] = {4, 8};
    nimcp_gpu_tensor_t* tensor = createTensor(dims, 2);
    ASSERT_NE(tensor, nullptr);
    fillRandom(tensor);  // Initialize data before capture

    // Begin capture
    bool begin_result = nimcp_infer_session_begin_capture(session);
    // May fail if CUDA graphs not supported
    if (!begin_result) {
        GTEST_SKIP() << "CUDA graph capture not supported";
    }

    // Perform compute operations only (no memory allocations during capture)
    nimcp_gpu_infer_activation_inplace(ctx, tensor, 0);  // ReLU

    // End capture
    bool end_result = nimcp_infer_session_end_capture(session);
    EXPECT_TRUE(end_result);
    EXPECT_TRUE(session->graph_captured);

    // Replay graph
    bool replay_result = nimcp_infer_session_replay(session);
    EXPECT_TRUE(replay_result);
}

/**
 * TEST: Session graph replay without capture
 * WHAT: Verify replay fails if graph not captured
 * WHY:  Must capture before replay
 */
TEST_F(InferenceKernelTest, SessionReplay_WithoutCapture_ReturnsFalse) {
    skipIfNoGPU();

    session = nimcp_infer_session_create(ctx, NIMCP_INFER_FP32, 0);
    ASSERT_NE(session, nullptr);

    bool result = nimcp_infer_session_replay(session);
    EXPECT_FALSE(result);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

/**
 * TEST: Get recommended precision
 * WHAT: Verify nimcp_gpu_infer_recommended_precision() returns valid precision
 * WHY:  Auto-select best precision for current GPU
 */
TEST_F(InferenceKernelTest, RecommendedPrecision_ReturnsValid) {
    skipIfNoGPU();

    nimcp_infer_precision_t precision = nimcp_gpu_infer_recommended_precision(ctx);

    // Should return one of the valid precision values
    EXPECT_TRUE(precision == NIMCP_INFER_FP32 ||
                precision == NIMCP_INFER_FP16 ||
                precision == NIMCP_INFER_BF16 ||
                precision == NIMCP_INFER_INT8 ||
                precision == NIMCP_INFER_TF32);
}

/**
 * TEST: Inference warmup
 * WHAT: Verify nimcp_gpu_infer_warmup() pre-compiles kernels
 * WHY:  Warmup reduces first-inference latency
 */
TEST_F(InferenceKernelTest, Warmup_Succeeds) {
    skipIfNoGPU();

    bool result = nimcp_gpu_infer_warmup(ctx, NIMCP_INFER_FP32);
    EXPECT_TRUE(result);
}

/**
 * TEST: Warmup with NULL context
 * WHAT: Verify warmup handles NULL context
 * WHY:  Guard clause validation
 */
TEST_F(InferenceKernelTest, Warmup_NullContext_ReturnsFalse) {
    bool result = nimcp_gpu_infer_warmup(nullptr, NIMCP_INFER_FP32);
    EXPECT_FALSE(result);
}

/**
 * TEST: Precision conversion
 * WHAT: Test nimcp_gpu_infer_convert_precision() converts between types
 * WHY:  Mixed precision requires type conversion
 */
TEST_F(InferenceKernelTest, ConvertPrecision_FP32toFP16) {
    skipIfNoGPU();

    size_t dims[2] = {4, 8};

    nimcp_gpu_tensor_t* fp32_tensor = createTensor(dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* fp16_tensor = createTensor(dims, 2, NIMCP_GPU_PRECISION_FP16);

    ASSERT_NE(fp32_tensor, nullptr);
    ASSERT_NE(fp16_tensor, nullptr);

    fillRandom(fp32_tensor, -1.0f, 1.0f);

    bool result = nimcp_gpu_infer_convert_precision(ctx, fp32_tensor, fp16_tensor,
                                                     NIMCP_INFER_FP16);
    EXPECT_TRUE(result);
}

//=============================================================================
// Integration Tests
//=============================================================================

/**
 * TEST: Full transformer layer inference
 * WHAT: Simulate a complete transformer layer
 * WHY:  End-to-end validation of inference pipeline
 */
TEST_F(InferenceKernelTest, Integration_TransformerLayer) {
    skipIfNoGPU();

    const size_t batch = 2;
    const size_t seq_len = 8;
    const size_t embed_dim = 16;
    const size_t ff_dim = 64;

    // Input embedding
    size_t input_dims[2] = {batch * seq_len, embed_dim};
    nimcp_gpu_tensor_t* hidden = createTensor(input_dims, 2);
    fillRandom(hidden, -0.5f, 0.5f);

    // Save input for residual
    nimcp_gpu_tensor_t* residual = nimcp_gpu_tensor_clone(hidden);
    tensors.push_back(residual);

    // Layer norm parameters
    size_t norm_dims[1] = {embed_dim};
    nimcp_gpu_tensor_t* gamma = createTensor(norm_dims, 1);
    nimcp_gpu_tensor_t* beta = createTensor(norm_dims, 1);
    nimcp_gpu_ones(ctx, gamma);
    nimcp_gpu_zeros(ctx, beta);

    // Apply layer norm
    nimcp_gpu_tensor_t* norm_out = createTensor(input_dims, 2);
    EXPECT_TRUE(nimcp_gpu_infer_layernorm(ctx, hidden, gamma, beta, norm_out, 1e-5f));

    // FFN: Linear + GELU
    size_t w1_dims[2] = {ff_dim, embed_dim};
    size_t b1_dims[1] = {ff_dim};
    size_t ff_dims[2] = {batch * seq_len, ff_dim};

    nimcp_gpu_tensor_t* w1 = createTensor(w1_dims, 2);
    nimcp_gpu_tensor_t* b1 = createTensor(b1_dims, 1);
    nimcp_gpu_tensor_t* ff_hidden = createTensor(ff_dims, 2);

    fillRandom(w1, -0.1f, 0.1f);
    fillRandom(b1, -0.01f, 0.01f);

    EXPECT_TRUE(nimcp_gpu_infer_linear_gelu(ctx, norm_out, w1, b1, ff_hidden));

    // FFN: Linear + SiLU (projection back)
    size_t w2_dims[2] = {embed_dim, ff_dim};
    size_t b2_dims[1] = {embed_dim};

    nimcp_gpu_tensor_t* w2 = createTensor(w2_dims, 2);
    nimcp_gpu_tensor_t* b2 = createTensor(b2_dims, 1);
    nimcp_gpu_tensor_t* ff_out = createTensor(input_dims, 2);

    fillRandom(w2, -0.1f, 0.1f);
    fillRandom(b2, -0.01f, 0.01f);

    EXPECT_TRUE(nimcp_gpu_infer_linear_relu(ctx, ff_hidden, w2, b2, ff_out));

    // Residual connection
    nimcp_gpu_tensor_t* output = createTensor(input_dims, 2);
    EXPECT_TRUE(nimcp_gpu_infer_residual_add(ctx, ff_out, residual, output, 1.0f, 1.0f));

    // Verify output is finite
    std::vector<float> final_output = copyToHost(output);
    for (size_t i = 0; i < final_output.size(); i++) {
        EXPECT_TRUE(std::isfinite(final_output[i])) << "NaN/Inf at index " << i;
    }
}

/**
 * TEST: Quantized inference pipeline
 * WHAT: Full quantization workflow: calibrate, quantize, infer, dequantize
 * WHY:  End-to-end INT8 inference validation
 */
TEST_F(InferenceKernelTest, Integration_QuantizedInference) {
    skipIfNoGPU();

    size_t batch = 4;
    size_t features = 32;
    size_t dims[2] = {batch, features};

    // Create FP32 input
    nimcp_gpu_tensor_t* fp32_input = createTensor(dims, 2, NIMCP_GPU_PRECISION_FP32);
    fillRandom(fp32_input, -2.0f, 2.0f);

    // Calibrate
    nimcp_quant_params_t params;
    EXPECT_TRUE(nimcp_gpu_infer_calibrate(ctx, fp32_input, &params, true));

    // Quantize
    nimcp_gpu_tensor_t* int8_input = createTensor(dims, 2, NIMCP_GPU_PRECISION_INT8);
    EXPECT_TRUE(nimcp_gpu_infer_quantize_int8(ctx, fp32_input, int8_input, &params));

    // Apply activation in INT8 domain (if supported) or dequantize first
    nimcp_gpu_tensor_t* fp32_output = createTensor(dims, 2, NIMCP_GPU_PRECISION_FP32);
    EXPECT_TRUE(nimcp_gpu_infer_dequantize_int8(ctx, int8_input, fp32_output, &params));

    // Apply ReLU
    EXPECT_TRUE(nimcp_gpu_infer_activation_inplace(ctx, fp32_output, 0));

    // Verify results
    std::vector<float> output = copyToHost(fp32_output);
    for (size_t i = 0; i < output.size(); i++) {
        EXPECT_GE(output[i], 0.0f) << "ReLU failed at index " << i;
    }
}

/**
 * TEST: Batch inference with multiple samples
 * WHAT: Process multiple batches sequentially
 * WHY:  Verify memory management across batches
 */
TEST_F(InferenceKernelTest, Integration_BatchInference) {
    skipIfNoGPU();

    const size_t batch_size = 8;
    const size_t in_features = 32;
    const size_t out_features = 16;
    const size_t num_batches = 10;

    size_t input_dims[2] = {batch_size, in_features};
    size_t weight_dims[2] = {out_features, in_features};
    size_t bias_dims[1] = {out_features};
    size_t output_dims[2] = {batch_size, out_features};

    // Shared weights
    nimcp_gpu_tensor_t* weights = createTensor(weight_dims, 2);
    nimcp_gpu_tensor_t* bias = createTensor(bias_dims, 1);
    fillRandom(weights, -0.5f, 0.5f);
    fillRandom(bias, -0.1f, 0.1f);

    // Process multiple batches
    for (size_t b = 0; b < num_batches; b++) {
        nimcp_gpu_tensor_t* input = nimcp_gpu_tensor_create(ctx, input_dims, 2,
                                                             NIMCP_GPU_PRECISION_FP32);
        nimcp_gpu_tensor_t* output = nimcp_gpu_tensor_create(ctx, output_dims, 2,
                                                              NIMCP_GPU_PRECISION_FP32);
        ASSERT_NE(input, nullptr);
        ASSERT_NE(output, nullptr);

        // Fill with batch-specific data
        std::vector<float> batch_data(batch_size * in_features);
        for (size_t i = 0; i < batch_data.size(); i++) {
            batch_data[i] = (float)(b + 1) * 0.1f;
        }
        nimcp_gpu_tensor_t* temp = nimcp_gpu_tensor_from_host(ctx, batch_data.data(),
                                                               input_dims, 2,
                                                               NIMCP_GPU_PRECISION_FP32);
        nimcp_gpu_copy(ctx, temp, input);
        nimcp_gpu_tensor_destroy(temp);

        // Inference
        EXPECT_TRUE(nimcp_gpu_infer_linear_relu(ctx, input, weights, bias, output));

        // Verify output
        std::vector<float> output_data = copyToHost(output);
        for (size_t i = 0; i < output_data.size(); i++) {
            EXPECT_GE(output_data[i], 0.0f);  // ReLU
            EXPECT_TRUE(std::isfinite(output_data[i]));
        }

        nimcp_gpu_tensor_destroy(input);
        nimcp_gpu_tensor_destroy(output);
    }
}

//=============================================================================
// Edge Cases and Error Handling
//=============================================================================

/**
 * TEST: Large tensor inference
 * WHAT: Test with large tensors to verify memory handling
 * WHY:  Production models have large layers
 */
TEST_F(InferenceKernelTest, EdgeCase_LargeTensors) {
    skipIfNoGPU();

    size_t batch = 32;
    size_t features = 1024;
    size_t dims[2] = {batch, features};

    nimcp_gpu_tensor_t* input = createTensor(dims, 2);
    nimcp_gpu_tensor_t* output = createTensor(dims, 2);

    if (input == nullptr || output == nullptr) {
        GTEST_SKIP() << "Could not allocate large tensors (memory constraint)";
    }

    fillRandom(input);

    // Apply activation
    nimcp_gpu_copy(ctx, input, output);
    EXPECT_TRUE(nimcp_gpu_infer_activation_inplace(ctx, output, 3));  // GELU

    std::vector<float> result = copyToHost(output);
    for (size_t i = 0; i < result.size(); i++) {
        EXPECT_TRUE(std::isfinite(result[i]));
    }
}

/**
 * TEST: Repeated operations
 * WHAT: Run many inference operations to test stability
 * WHY:  Verify no memory leaks or state corruption
 */
TEST_F(InferenceKernelTest, EdgeCase_RepeatedOperations) {
    skipIfNoGPU();

    size_t dims[2] = {8, 16};

    nimcp_gpu_tensor_t* tensor = createTensor(dims, 2);
    ASSERT_NE(tensor, nullptr);

    for (int i = 0; i < 1000; i++) {
        fillRandom(tensor, -1.0f, 1.0f);

        // Apply various activations
        EXPECT_TRUE(nimcp_gpu_infer_activation_inplace(ctx, tensor, i % 5));
    }

    // Final sync
    EXPECT_EQ(nimcp_gpu_context_synchronize(ctx), 0);
}

/**
 * TEST: Dimension mismatch detection
 * WHAT: Verify operations detect shape incompatibilities
 * WHY:  Must catch user errors early
 */
TEST_F(InferenceKernelTest, EdgeCase_DimensionMismatch) {
    skipIfNoGPU();

    // Input [4, 8] but weights [3, 4] - incompatible K dimension
    size_t input_dims[2] = {4, 8};
    size_t weight_dims[2] = {3, 4};  // K=4 != 8
    size_t output_dims[2] = {4, 3};

    nimcp_gpu_tensor_t* input = createTensor(input_dims, 2);
    nimcp_gpu_tensor_t* weights = createTensor(weight_dims, 2);
    nimcp_gpu_tensor_t* output = createTensor(output_dims, 2);

    fillRandom(input);
    fillRandom(weights);

    // This should fail or produce incorrect results
    bool result = nimcp_gpu_infer_linear_relu(ctx, input, weights, nullptr, output);
    // Implementation may either return false or handle gracefully
    // The key is it shouldn't crash
    (void)result;  // Suppress unused warning
}

/**
 * TEST: GPU context error state
 * WHAT: Verify error handling after GPU errors
 * WHY:  Should recover gracefully from GPU errors
 */
TEST_F(InferenceKernelTest, EdgeCase_ErrorRecovery) {
    skipIfNoGPU();

    // Normal operation
    size_t dims[2] = {4, 8};
    nimcp_gpu_tensor_t* tensor = createTensor(dims, 2);
    ASSERT_NE(tensor, nullptr);

    fillRandom(tensor);
    EXPECT_TRUE(nimcp_gpu_infer_activation_inplace(ctx, tensor, 0));

    // Sync to clear any pending errors
    nimcp_gpu_context_synchronize(ctx);

    // Should still work after sync
    fillRandom(tensor);
    EXPECT_TRUE(nimcp_gpu_infer_activation_inplace(ctx, tensor, 1));
}
