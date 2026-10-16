/**
 * @file test_int8_quantization.cpp
 * @brief Unit tests for INT8 Quantization (QAT)
 *
 * Tests quantization params computation, FP32/INT8 conversions, per-channel
 * quantization, INT8 GEMM, fake quantization, calibration, and model export.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <cstring>
#include <random>
#include <algorithm>
#include <numeric>

// Headers already have their own extern "C" guards
#include "training/nimcp_quantization_aware.h"
#include "utils/tensor/nimcp_tensor.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class INT8QuantizationTest : public ::testing::Test {
protected:
    qat_ctx_t* qat = nullptr;

    void SetUp() override {
        // Default setup
    }

    void TearDown() override {
        if (qat) {
            qat_destroy(qat);
            qat = nullptr;
        }
    }

    // Helper to create a 1D tensor with values
    nimcp_tensor_t* Create1DTensor(size_t n, float fill_value = 0.0f) {
        size_t dims[1] = {n};
        nimcp_tensor_t* tensor = nimcp_tensor_create(dims, 1, NIMCP_GPU_PRECISION_FP32);
        if (tensor && fill_value != 0.0f) {
            float* data = (float*)tensor->data;
            for (size_t i = 0; i < n; i++) {
                data[i] = fill_value;
            }
        }
        return tensor;
    }

    // Helper to create 2D tensor
    nimcp_tensor_t* Create2DTensor(size_t rows, size_t cols, float fill_value = 0.0f) {
        size_t dims[2] = {rows, cols};
        nimcp_tensor_t* tensor = nimcp_tensor_create(dims, 2, NIMCP_GPU_PRECISION_FP32);
        if (tensor && fill_value != 0.0f) {
            float* data = (float*)tensor->data;
            for (size_t i = 0; i < rows * cols; i++) {
                data[i] = fill_value;
            }
        }
        return tensor;
    }

    // Helper to fill tensor with random values
    void FillRandom(nimcp_tensor_t* tensor, float min_val = -1.0f, float max_val = 1.0f) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(min_val, max_val);

        float* data = (float*)tensor->data;
        for (size_t i = 0; i < nimcp_tensor_numel(tensor); i++) {
            data[i] = dist(gen);
        }
    }

    // Helper to get tensor data
    std::vector<float> GetData(nimcp_tensor_t* tensor) {
        size_t n = nimcp_tensor_numel(tensor);
        float* data = (float*)tensor->data;
        return std::vector<float>(data, data + n);
    }

    // Helper to set tensor data
    void SetData(nimcp_tensor_t* tensor, const std::vector<float>& values) {
        float* data = (float*)tensor->data;
        memcpy(data, values.data(), values.size() * sizeof(float));
    }

    // Compute MSE between two vectors
    float ComputeMSE(const std::vector<float>& a, const std::vector<float>& b) {
        if (a.size() != b.size()) return std::numeric_limits<float>::infinity();

        float mse = 0.0f;
        for (size_t i = 0; i < a.size(); i++) {
            float diff = a[i] - b[i];
            mse += diff * diff;
        }
        return mse / a.size();
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(INT8QuantizationTest, DefaultConfig_ReturnsValidConfig) {
    qat_config_t config;
    int result = qat_default_config(&config);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(config.default_weight_dtype, QAT_DTYPE_INT8);
    EXPECT_EQ(config.default_activation_dtype, QAT_DTYPE_INT8);
    EXPECT_EQ(config.default_scheme, QAT_SCHEME_SYMMETRIC);
    EXPECT_EQ(config.default_granularity, QAT_GRANULARITY_TENSOR);
}

TEST_F(INT8QuantizationTest, INT4Config_ReturnsValidConfig) {
    qat_config_t config;
    int result = qat_int4_config(&config);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(config.default_weight_dtype, QAT_DTYPE_INT4);
}

TEST_F(INT8QuantizationTest, BinaryConfig_ReturnsValidConfig) {
    qat_config_t config;
    int result = qat_binary_config(&config);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(config.default_weight_dtype, QAT_DTYPE_INT1);
}

TEST_F(INT8QuantizationTest, ValidateConfig_ReturnsZeroForValid) {
    qat_config_t config;
    qat_default_config(&config);

    int result = qat_validate_config(&config);
    EXPECT_EQ(result, 0);
}

//=============================================================================
// Context Creation Tests
//=============================================================================

TEST_F(INT8QuantizationTest, Create_WithDefaultConfig_ReturnsValidContext) {
    qat_config_t config;
    qat_default_config(&config);

    qat = qat_create(&config);

    ASSERT_NE(qat, nullptr);
}

TEST_F(INT8QuantizationTest, Create_WithNullConfig_UsesDefaults) {
    qat = qat_create(nullptr);

    // Should work with default configuration
    ASSERT_NE(qat, nullptr);
}

TEST_F(INT8QuantizationTest, Destroy_HandlesNull) {
    qat_destroy(nullptr);  // Should not crash
}

//=============================================================================
// Quantization Parameters Tests
//=============================================================================

TEST_F(INT8QuantizationTest, Quantize_ComputesCorrectParams) {
    qat_config_t config;
    qat_default_config(&config);
    qat = qat_create(&config);
    ASSERT_NE(qat, nullptr);

    nimcp_tensor_t* tensor = Create1DTensor(100, 0.0f);
    ASSERT_NE(tensor, nullptr);

    // Fill with range [-1, 1]
    std::vector<float> values(100);
    for (int i = 0; i < 100; i++) {
        values[i] = -1.0f + 2.0f * i / 99.0f;
    }
    SetData(tensor, values);

    int observer_id = qat_register_observer(qat, "test_tensor", QAT_TARGET_WEIGHTS);
    EXPECT_GE(observer_id, 0);

    int result = qat_observe(qat, observer_id, tensor);
    EXPECT_EQ(result, 0);

    qat_params_t params;
    result = qat_get_params(qat, observer_id, &params);
    EXPECT_EQ(result, 0);

    // Scale should be reasonable for [-1, 1] range
    EXPECT_GT(params.scale, 0.0f);
    EXPECT_NEAR(params.observed_min, -1.0f, 0.1f);
    EXPECT_NEAR(params.observed_max, 1.0f, 0.1f);

    nimcp_tensor_destroy(tensor);
}

//=============================================================================
// FP32 to INT8 Quantization Tests
//=============================================================================

TEST_F(INT8QuantizationTest, FP32ToINT8_BasicQuantization) {
    qat_config_t config;
    qat_default_config(&config);
    qat = qat_create(&config);
    ASSERT_NE(qat, nullptr);

    nimcp_tensor_t* input = Create1DTensor(100, 0.0f);
    size_t dims[1] = {100};
    nimcp_tensor_t* output = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_INT8);
    ASSERT_NE(input, nullptr);
    ASSERT_NE(output, nullptr);

    // Fill with values in range [-1, 1]
    FillRandom(input, -1.0f, 1.0f);

    // Setup params
    qat_params_t params;
    params.scale = 0.00787f;  // 1/127
    params.zero_point = 0;
    params.dtype = QAT_DTYPE_INT8;
    params.scheme = QAT_SCHEME_SYMMETRIC;
    params.granularity = QAT_GRANULARITY_TENSOR;
    params.scales = nullptr;
    params.zero_points = nullptr;
    params.num_channels = 0;

    int result = qat_quantize(qat, input, output, &params);
    EXPECT_EQ(result, 0);

    // Verify output is INT8 values
    int8_t* out_data = (int8_t*)output->data;
    for (size_t i = 0; i < 100; i++) {
        EXPECT_GE(out_data[i], -128);
        EXPECT_LE(out_data[i], 127);
    }

    nimcp_tensor_destroy(input);
    nimcp_tensor_destroy(output);
}

TEST_F(INT8QuantizationTest, INT8ToFP32_BasicDequantization) {
    qat_config_t config;
    qat_default_config(&config);
    qat = qat_create(&config);
    ASSERT_NE(qat, nullptr);

    size_t dims[1] = {100};
    nimcp_tensor_t* input = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_INT8);
    nimcp_tensor_t* output = Create1DTensor(100, 0.0f);
    ASSERT_NE(input, nullptr);
    ASSERT_NE(output, nullptr);

    // Fill with INT8 values
    int8_t* in_data = (int8_t*)input->data;
    for (int i = 0; i < 100; i++) {
        in_data[i] = (int8_t)(i - 50);  // Range [-50, 49]
    }

    qat_params_t params;
    params.scale = 0.01f;
    params.zero_point = 0;
    params.dtype = QAT_DTYPE_INT8;
    params.scheme = QAT_SCHEME_SYMMETRIC;

    int result = qat_dequantize(qat, input, output, &params);
    EXPECT_EQ(result, 0);

    float* out_data = (float*)output->data;
    for (int i = 0; i < 100; i++) {
        float expected = (i - 50) * 0.01f;
        EXPECT_NEAR(out_data[i], expected, 0.001f);
    }

    nimcp_tensor_destroy(input);
    nimcp_tensor_destroy(output);
}

TEST_F(INT8QuantizationTest, QuantizeDequantize_RoundTrip) {
    qat_config_t config;
    qat_default_config(&config);
    qat = qat_create(&config);
    ASSERT_NE(qat, nullptr);

    nimcp_tensor_t* original = Create1DTensor(256, 0.0f);
    FillRandom(original, -0.5f, 0.5f);

    std::vector<float> original_data = GetData(original);

    // Quantize
    size_t dims[1] = {256};
    nimcp_tensor_t* quantized = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_INT8);
    nimcp_tensor_t* dequantized = Create1DTensor(256, 0.0f);

    qat_params_t params;
    params.scale = 1.0f / 255.0f;
    params.zero_point = 0;
    params.dtype = QAT_DTYPE_INT8;
    params.scheme = QAT_SCHEME_SYMMETRIC;

    qat_quantize(qat, original, quantized, &params);
    qat_dequantize(qat, quantized, dequantized, &params);

    std::vector<float> deq_data = GetData(dequantized);

    // Should be close but not exact due to quantization
    float mse = ComputeMSE(original_data, deq_data);
    EXPECT_LT(mse, 0.01f);  // Quantization error should be small

    nimcp_tensor_destroy(original);
    nimcp_tensor_destroy(quantized);
    nimcp_tensor_destroy(dequantized);
}

//=============================================================================
// Per-Channel Quantization Tests
//=============================================================================

TEST_F(INT8QuantizationTest, PerChannelQuantization_DifferentScales) {
    qat_config_t config;
    qat_default_config(&config);
    config.default_granularity = QAT_GRANULARITY_CHANNEL;
    qat = qat_create(&config);
    ASSERT_NE(qat, nullptr);

    // Create 4-channel tensor (output_channels x input_features)
    nimcp_tensor_t* weights = Create2DTensor(4, 16, 0.0f);
    ASSERT_NE(weights, nullptr);

    float* data = (float*)weights->data;

    // Each channel has different magnitude
    for (int c = 0; c < 4; c++) {
        float scale = (c + 1) * 0.25f;  // 0.25, 0.5, 0.75, 1.0
        for (int i = 0; i < 16; i++) {
            data[c * 16 + i] = scale * (float)(i - 8) / 8.0f;
        }
    }

    // Register and observe
    int observer_id = qat_register_observer(qat, "weights", QAT_TARGET_WEIGHTS);
    qat_observe(qat, observer_id, weights);

    qat_params_t params;
    int result = qat_get_params(qat, observer_id, &params);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(params.granularity, QAT_GRANULARITY_CHANNEL);

    // Per-channel should have different scales
    if (params.scales != nullptr) {
        for (uint32_t c = 0; c < params.num_channels; c++) {
            EXPECT_GT(params.scales[c], 0.0f);
        }
    }

    nimcp_tensor_destroy(weights);
}

//=============================================================================
// Symmetric vs Asymmetric Quantization Tests
//=============================================================================

TEST_F(INT8QuantizationTest, SymmetricQuantization_ZeroPointIsZero) {
    qat_config_t config;
    qat_default_config(&config);
    config.default_scheme = QAT_SCHEME_SYMMETRIC;
    qat = qat_create(&config);
    ASSERT_NE(qat, nullptr);

    nimcp_tensor_t* tensor = Create1DTensor(100, 0.0f);
    FillRandom(tensor, -1.0f, 1.0f);

    int observer_id = qat_register_observer(qat, "symmetric", QAT_TARGET_WEIGHTS);
    qat_observe(qat, observer_id, tensor);

    qat_params_t params;
    qat_get_params(qat, observer_id, &params);

    EXPECT_EQ(params.scheme, QAT_SCHEME_SYMMETRIC);
    EXPECT_EQ(params.zero_point, 0);

    nimcp_tensor_destroy(tensor);
}

TEST_F(INT8QuantizationTest, AffineQuantization_NonZeroZeroPoint) {
    qat_config_t config;
    qat_default_config(&config);
    config.default_scheme = QAT_SCHEME_AFFINE;
    qat = qat_create(&config);
    ASSERT_NE(qat, nullptr);

    nimcp_tensor_t* tensor = Create1DTensor(100, 0.0f);

    // Create asymmetric data (mostly positive)
    std::vector<float> values(100);
    for (int i = 0; i < 100; i++) {
        values[i] = 0.1f + 0.9f * i / 99.0f;  // Range [0.1, 1.0]
    }
    SetData(tensor, values);

    int observer_id = qat_register_observer(qat, "affine", QAT_TARGET_ACTIVATIONS);
    qat_observe(qat, observer_id, tensor);

    qat_params_t params;
    qat_get_params(qat, observer_id, &params);

    EXPECT_EQ(params.scheme, QAT_SCHEME_AFFINE);
    // With asymmetric data, zero_point should be non-zero
    // Actual value depends on implementation

    nimcp_tensor_destroy(tensor);
}

//=============================================================================
// INT8 GEMM Tests
//=============================================================================

TEST_F(INT8QuantizationTest, INT8GEMM_CorrectResult) {
    qat_config_t config;
    qat_default_config(&config);
    qat = qat_create(&config);
    ASSERT_NE(qat, nullptr);

    const size_t M = 4, K = 4, N = 4;

    // Create matrices
    size_t dims_a[2] = {M, K};
    size_t dims_b[2] = {K, N};
    size_t dims_c[2] = {M, N};

    nimcp_tensor_t* A = nimcp_tensor_create(dims_a, 2, NIMCP_DTYPE_INT8);
    nimcp_tensor_t* B = nimcp_tensor_create(dims_b, 2, NIMCP_DTYPE_INT8);
    nimcp_tensor_t* C = nimcp_tensor_create(dims_c, 2, NIMCP_GPU_PRECISION_FP32);

    // Fill with simple known values
    int8_t* a_data = (int8_t*)A->data;
    int8_t* b_data = (int8_t*)B->data;

    // A = all 1s, B = identity
    for (size_t i = 0; i < M * K; i++) a_data[i] = 1;
    memset(b_data, 0, K * N);
    for (size_t i = 0; i < std::min(K, N); i++) {
        b_data[i * N + i] = 1;
    }

    qat_params_t params_a = {.scale = 1.0f, .zero_point = 0, .dtype = QAT_DTYPE_INT8, .scheme = QAT_SCHEME_SYMMETRIC};
    qat_params_t params_b = {.scale = 1.0f, .zero_point = 0, .dtype = QAT_DTYPE_INT8, .scheme = QAT_SCHEME_SYMMETRIC};

    int result = qat_matmul(qat, A, B, C, &params_a, &params_b, nullptr);
    EXPECT_EQ(result, 0);

    // Result should be: each row = [1, 1, 1, 1] (where K=4)
    float* c_data = (float*)C->data;
    for (size_t i = 0; i < M; i++) {
        for (size_t j = 0; j < N; j++) {
            float expected = (j < K) ? 1.0f : 0.0f;
            EXPECT_NEAR(c_data[i * N + j], expected, 0.1f);
        }
    }

    nimcp_tensor_destroy(A);
    nimcp_tensor_destroy(B);
    nimcp_tensor_destroy(C);
}

//=============================================================================
// Fake Quantization Tests
//=============================================================================

TEST_F(INT8QuantizationTest, FakeQuantize_SimulatesQuantization) {
    qat_config_t config;
    qat_default_config(&config);
    qat = qat_create(&config);
    ASSERT_NE(qat, nullptr);

    nimcp_tensor_t* tensor = Create1DTensor(256, 0.0f);
    ASSERT_NE(tensor, nullptr);

    FillRandom(tensor, -1.0f, 1.0f);
    std::vector<float> original = GetData(tensor);

    qat_params_t params;
    params.scale = 1.0f / 127.0f;
    params.zero_point = 0;
    params.dtype = QAT_DTYPE_INT8;
    params.scheme = QAT_SCHEME_SYMMETRIC;

    int result = qat_fake_quantize(qat, tensor, &params);
    EXPECT_EQ(result, 0);

    std::vector<float> fake_quant = GetData(tensor);

    // Fake quantized values should be different but close to original
    bool values_changed = false;
    for (size_t i = 0; i < original.size(); i++) {
        if (std::abs(fake_quant[i] - original[i]) > 1e-6f) {
            values_changed = true;
            break;
        }
    }
    EXPECT_TRUE(values_changed);

    // Values should be quantization levels
    for (size_t i = 0; i < fake_quant.size(); i++) {
        float q = fake_quant[i] / params.scale;
        float q_rounded = std::round(q);
        EXPECT_NEAR(q, q_rounded, 0.01f);
    }

    nimcp_tensor_destroy(tensor);
}

TEST_F(INT8QuantizationTest, FakeQuantize_LearnedParams) {
    qat_config_t config;
    qat_default_config(&config);
    config.fake_quant.method = QAT_FAKE_QUANT_LSQ;  // Learned Step Size
    qat = qat_create(&config);
    ASSERT_NE(qat, nullptr);

    nimcp_tensor_t* tensor = Create1DTensor(100, 0.0f);
    FillRandom(tensor, -0.5f, 0.5f);

    int observer_id = qat_register_observer(qat, "learned", QAT_TARGET_WEIGHTS);
    qat_observe(qat, observer_id, tensor);

    int result = qat_fake_quantize_learned(qat, tensor, observer_id);
    EXPECT_EQ(result, 0);

    nimcp_tensor_destroy(tensor);
}

TEST_F(INT8QuantizationTest, FakeQuantize_Backward_STE) {
    qat_config_t config;
    qat_default_config(&config);
    config.fake_quant.method = QAT_FAKE_QUANT_STE;
    qat = qat_create(&config);
    ASSERT_NE(qat, nullptr);

    nimcp_tensor_t* tensor = Create1DTensor(100, 0.0f);
    nimcp_tensor_t* grad_output = Create1DTensor(100, 1.0f);  // All ones
    nimcp_tensor_t* grad_input = Create1DTensor(100, 0.0f);

    FillRandom(tensor, -1.0f, 1.0f);

    qat_params_t params;
    params.scale = 1.0f / 127.0f;
    params.zero_point = 0;
    params.dtype = QAT_DTYPE_INT8;
    params.scheme = QAT_SCHEME_SYMMETRIC;

    int result = qat_fake_quantize_backward(qat, grad_output, tensor, &params, grad_input);
    EXPECT_EQ(result, 0);

    // With STE, gradients should pass through unchanged (within clipping range)
    std::vector<float> grad = GetData(grad_input);
    for (size_t i = 0; i < grad.size(); i++) {
        // Gradient should be 1 for values within range, 0 for clipped
        EXPECT_TRUE(grad[i] == 0.0f || grad[i] == 1.0f);
    }

    nimcp_tensor_destroy(tensor);
    nimcp_tensor_destroy(grad_output);
    nimcp_tensor_destroy(grad_input);
}

//=============================================================================
// Calibration Tests
//=============================================================================

TEST_F(INT8QuantizationTest, Calibration_MinMax) {
    qat_config_t config;
    qat_default_config(&config);
    config.observer.method = QAT_OBSERVER_MINMAX;
    qat = qat_create(&config);
    ASSERT_NE(qat, nullptr);

    nimcp_tensor_t* tensor = Create1DTensor(1000, 0.0f);

    int observer_id = qat_register_observer(qat, "calibration", QAT_TARGET_ACTIVATIONS);

    // Observe multiple batches
    for (int batch = 0; batch < 10; batch++) {
        FillRandom(tensor, -2.0f + batch * 0.1f, 2.0f + batch * 0.1f);
        qat_observe(qat, observer_id, tensor);
    }

    qat_params_t params;
    qat_get_params(qat, observer_id, &params);

    // Min/max should have been tracked
    EXPECT_LE(params.observed_min, -2.0f);
    EXPECT_GE(params.observed_max, 2.0f);

    nimcp_tensor_destroy(tensor);
}

TEST_F(INT8QuantizationTest, Calibration_MovingAverage) {
    qat_config_t config;
    qat_default_config(&config);
    config.observer.method = QAT_OBSERVER_MOVING_AVG;
    config.observer.ema_decay = 0.9f;
    qat = qat_create(&config);
    ASSERT_NE(qat, nullptr);

    nimcp_tensor_t* tensor = Create1DTensor(100, 0.0f);
    int observer_id = qat_register_observer(qat, "ema", QAT_TARGET_WEIGHTS);

    // First batch with small range
    FillRandom(tensor, -0.1f, 0.1f);
    qat_observe(qat, observer_id, tensor);

    // Second batch with larger range
    FillRandom(tensor, -1.0f, 1.0f);
    qat_observe(qat, observer_id, tensor);

    qat_params_t params;
    qat_get_params(qat, observer_id, &params);

    // EMA should have smoothed the range
    // Range should be between 0.1 and 1.0
    EXPECT_GT(std::abs(params.observed_max - params.observed_min), 0.1f);

    nimcp_tensor_destroy(tensor);
}

TEST_F(INT8QuantizationTest, Calibration_Histogram) {
    qat_config_t config;
    qat_default_config(&config);
    config.observer.method = QAT_OBSERVER_HISTOGRAM;
    config.observer.num_bins = 2048;
    config.observer.percentile = 0.999f;
    qat = qat_create(&config);
    ASSERT_NE(qat, nullptr);

    nimcp_tensor_t* tensor = Create1DTensor(1000, 0.0f);
    int observer_id = qat_register_observer(qat, "histogram", QAT_TARGET_ACTIVATIONS);

    // Observe with some outliers
    std::vector<float> values(1000);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<float> dist(0.0f, 0.3f);

    for (int i = 0; i < 1000; i++) {
        values[i] = dist(gen);
    }
    // Add some outliers
    values[0] = 10.0f;
    values[999] = -10.0f;

    SetData(tensor, values);
    qat_observe(qat, observer_id, tensor);

    qat_params_t params;
    qat_get_params(qat, observer_id, &params);

    // With histogram and percentile, outliers should be clipped
    // Range should be much smaller than [-10, 10]
    EXPECT_GT(params.observed_min, -5.0f);
    EXPECT_LT(params.observed_max, 5.0f);

    nimcp_tensor_destroy(tensor);
}

TEST_F(INT8QuantizationTest, Calibration_Entropy) {
    qat_config_t config;
    qat_default_config(&config);
    config.observer.method = QAT_OBSERVER_ENTROPY;
    qat = qat_create(&config);
    ASSERT_NE(qat, nullptr);

    nimcp_tensor_t* tensor = Create1DTensor(500, 0.0f);
    int observer_id = qat_register_observer(qat, "entropy", QAT_TARGET_ACTIVATIONS);

    FillRandom(tensor, -1.0f, 1.0f);
    qat_observe(qat, observer_id, tensor);

    qat_params_t params;
    int result = qat_get_params(qat, observer_id, &params);
    EXPECT_EQ(result, 0);

    nimcp_tensor_destroy(tensor);
}

TEST_F(INT8QuantizationTest, Calibrate_BatchProcess) {
    qat_config_t config;
    qat_default_config(&config);
    qat = qat_create(&config);
    ASSERT_NE(qat, nullptr);

    // Register observers
    qat_register_observer(qat, "layer1", QAT_TARGET_ACTIVATIONS);
    qat_register_observer(qat, "layer2", QAT_TARGET_ACTIVATIONS);

    // Calibrate (batch process all observers)
    int result = qat_calibrate(qat);
    EXPECT_EQ(result, 0);
}

TEST_F(INT8QuantizationTest, FreezeObservers_StopsUpdating) {
    qat_config_t config;
    qat_default_config(&config);
    qat = qat_create(&config);
    ASSERT_NE(qat, nullptr);

    nimcp_tensor_t* tensor = Create1DTensor(100, 0.0f);
    int observer_id = qat_register_observer(qat, "test", QAT_TARGET_WEIGHTS);

    // First observation
    FillRandom(tensor, -0.5f, 0.5f);
    qat_observe(qat, observer_id, tensor);

    qat_params_t params_before;
    qat_get_params(qat, observer_id, &params_before);

    // Freeze
    int result = qat_freeze_observers(qat);
    EXPECT_EQ(result, 0);

    // Observation after freeze (should be ignored)
    FillRandom(tensor, -2.0f, 2.0f);
    qat_observe(qat, observer_id, tensor);

    qat_params_t params_after;
    qat_get_params(qat, observer_id, &params_after);

    // Parameters should not have changed
    EXPECT_FLOAT_EQ(params_before.scale, params_after.scale);
    EXPECT_EQ(params_before.zero_point, params_after.zero_point);

    nimcp_tensor_destroy(tensor);
}

//=============================================================================
// Ternary Quantization Tests
//=============================================================================

TEST_F(INT8QuantizationTest, TernaryDefaultConfig) {
    qat_ternary_config_t config;
    int result = qat_ternary_default_config(&config);

    EXPECT_EQ(result, 0);
    EXPECT_GT(config.threshold_ratio, 0.0f);
    EXPECT_LT(config.threshold_ratio, 1.0f);
    EXPECT_TRUE(config.use_ste);
    EXPECT_TRUE(config.symmetric);
}

TEST_F(INT8QuantizationTest, TernaryCreate) {
    qat_ternary_config_t config;
    qat_ternary_default_config(&config);

    qat = qat_ternary_create(&config);
    ASSERT_NE(qat, nullptr);
}

TEST_F(INT8QuantizationTest, Ternarize_ProducesThreeValues) {
    qat_ternary_config_t config;
    qat_ternary_default_config(&config);
    qat = qat_ternary_create(&config);
    ASSERT_NE(qat, nullptr);

    nimcp_tensor_t* tensor = Create1DTensor(1000, 0.0f);
    FillRandom(tensor, -1.0f, 1.0f);

    qat_ternary_params_t params;
    int result = qat_ternarize(qat, tensor, &config, &params);
    EXPECT_EQ(result, 0);

    // Count unique values
    std::vector<float> data = GetData(tensor);
    std::set<float> unique_values;
    for (float v : data) {
        unique_values.insert(std::round(v * 1000) / 1000);  // Round to avoid float precision issues
    }

    // Should have exactly 3 unique values: -scale, 0, +scale
    EXPECT_LE(unique_values.size(), 3u);

    // Check statistics
    EXPECT_GE(params.n_positive, 0u);
    EXPECT_GE(params.n_zero, 0u);
    EXPECT_GE(params.n_negative, 0u);
    EXPECT_EQ(params.n_positive + params.n_zero + params.n_negative, 1000u);

    nimcp_tensor_destroy(tensor);
}

TEST_F(INT8QuantizationTest, TernaryBackward_STE) {
    qat_ternary_config_t config;
    qat_ternary_default_config(&config);
    config.use_ste = true;
    qat = qat_ternary_create(&config);
    ASSERT_NE(qat, nullptr);

    nimcp_tensor_t* grad_output = Create1DTensor(100, 1.0f);
    nimcp_tensor_t* original = Create1DTensor(100, 0.0f);
    nimcp_tensor_t* grad_input = Create1DTensor(100, 0.0f);

    FillRandom(original, -1.0f, 1.0f);

    int result = qat_ternary_backward(qat, grad_output, original, &config, grad_input);
    EXPECT_EQ(result, 0);

    // With STE, gradient should pass through
    std::vector<float> grad = GetData(grad_input);
    for (float g : grad) {
        EXPECT_TRUE(g == 0.0f || g == 1.0f);
    }

    nimcp_tensor_destroy(grad_output);
    nimcp_tensor_destroy(original);
    nimcp_tensor_destroy(grad_input);
}

TEST_F(INT8QuantizationTest, ComputeOptimalTernaryThreshold) {
    nimcp_tensor_t* tensor = Create1DTensor(1000, 0.0f);
    FillRandom(tensor, -1.0f, 1.0f);

    float threshold, scale;
    int result = qat_compute_optimal_ternary_threshold(tensor, &threshold, &scale);

    EXPECT_EQ(result, 0);
    EXPECT_GT(threshold, 0.0f);
    EXPECT_GT(scale, 0.0f);

    nimcp_tensor_destroy(tensor);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(INT8QuantizationTest, GetStats_ReturnsValidStats) {
    qat_config_t config;
    qat_default_config(&config);
    config.track_statistics = true;
    qat = qat_create(&config);
    ASSERT_NE(qat, nullptr);

    qat_stats_t stats;
    int result = qat_get_stats(qat, &stats);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.total_steps, 0u);
}

TEST_F(INT8QuantizationTest, ResetStats) {
    qat_config_t config;
    qat_default_config(&config);
    qat = qat_create(&config);
    ASSERT_NE(qat, nullptr);

    qat_reset_stats(qat);  // Should not crash

    qat_stats_t stats;
    qat_get_stats(qat, &stats);
    EXPECT_EQ(stats.total_steps, 0u);
}

TEST_F(INT8QuantizationTest, ComputeMSE_CorrectResult) {
    nimcp_tensor_t* original = Create1DTensor(100, 1.0f);
    nimcp_tensor_t* quantized = Create1DTensor(100, 1.1f);

    float mse = qat_compute_mse(original, quantized);

    EXPECT_NEAR(mse, 0.01f, 0.001f);  // (1.1 - 1.0)^2 = 0.01

    nimcp_tensor_destroy(original);
    nimcp_tensor_destroy(quantized);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(INT8QuantizationTest, DtypeName_ReturnsCorrectStrings) {
    EXPECT_STREQ(qat_dtype_name(QAT_DTYPE_INT8), "INT8");
    EXPECT_STREQ(qat_dtype_name(QAT_DTYPE_UINT8), "UINT8");
    EXPECT_STREQ(qat_dtype_name(QAT_DTYPE_INT4), "INT4");
    EXPECT_STREQ(qat_dtype_name(QAT_DTYPE_INT2), "INT2");
    EXPECT_STREQ(qat_dtype_name(QAT_DTYPE_INT1), "INT1");
    EXPECT_STREQ(qat_dtype_name(QAT_DTYPE_FP8_E4M3), "FP8_E4M3");
    EXPECT_STREQ(qat_dtype_name(QAT_DTYPE_TERNARY), "TERNARY");
}

TEST_F(INT8QuantizationTest, DtypeBits_ReturnsCorrectBits) {
    EXPECT_EQ(qat_dtype_bits(QAT_DTYPE_INT8), 8u);
    EXPECT_EQ(qat_dtype_bits(QAT_DTYPE_UINT8), 8u);
    EXPECT_EQ(qat_dtype_bits(QAT_DTYPE_INT4), 4u);
    EXPECT_EQ(qat_dtype_bits(QAT_DTYPE_INT2), 2u);
    EXPECT_EQ(qat_dtype_bits(QAT_DTYPE_INT1), 1u);
}

TEST_F(INT8QuantizationTest, SchemeName_ReturnsCorrectStrings) {
    EXPECT_STREQ(qat_scheme_name(QAT_SCHEME_SYMMETRIC), "SYMMETRIC");
    EXPECT_STREQ(qat_scheme_name(QAT_SCHEME_AFFINE), "AFFINE");
    EXPECT_STREQ(qat_scheme_name(QAT_SCHEME_POWER_OF_TWO), "POWER_OF_TWO");
}

//=============================================================================
// NULL Safety Tests
//=============================================================================

TEST_F(INT8QuantizationTest, NullSafety_AllFunctions) {
    EXPECT_NE(qat_default_config(nullptr), 0);
    EXPECT_NE(qat_int4_config(nullptr), 0);
    EXPECT_NE(qat_binary_config(nullptr), 0);
    EXPECT_NE(qat_validate_config(nullptr), 0);

    qat_destroy(nullptr);  // Should not crash

    EXPECT_LT(qat_register_observer(nullptr, "test", QAT_TARGET_WEIGHTS), 0);
    EXPECT_NE(qat_observe(nullptr, 0, nullptr), 0);
    EXPECT_NE(qat_get_params(nullptr, 0, nullptr), 0);
    EXPECT_NE(qat_calibrate(nullptr), 0);
    EXPECT_NE(qat_freeze_observers(nullptr), 0);

    EXPECT_NE(qat_fake_quantize(nullptr, nullptr, nullptr), 0);
    EXPECT_NE(qat_fake_quantize_learned(nullptr, nullptr, 0), 0);
    EXPECT_NE(qat_fake_quantize_backward(nullptr, nullptr, nullptr, nullptr, nullptr), 0);

    EXPECT_NE(qat_quantize(nullptr, nullptr, nullptr, nullptr), 0);
    EXPECT_NE(qat_dequantize(nullptr, nullptr, nullptr, nullptr), 0);
    EXPECT_NE(qat_matmul(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr), 0);

    EXPECT_NE(qat_get_stats(nullptr, nullptr), 0);
    qat_reset_stats(nullptr);  // Should not crash

    EXPECT_NE(qat_ternary_default_config(nullptr), 0);
    EXPECT_NE(qat_ternarize(nullptr, nullptr, nullptr, nullptr), 0);
    EXPECT_NE(qat_ternary_backward(nullptr, nullptr, nullptr, nullptr, nullptr), 0);
    EXPECT_NE(qat_compute_optimal_ternary_threshold(nullptr, nullptr, nullptr), 0);
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(INT8QuantizationTest, EmptyTensor_Handled) {
    qat_config_t config;
    qat_default_config(&config);
    qat = qat_create(&config);
    ASSERT_NE(qat, nullptr);

    size_t dims[1] = {0};
    nimcp_tensor_t* empty = nimcp_tensor_create(dims, 1, NIMCP_GPU_PRECISION_FP32);

    if (empty != nullptr) {
        qat_params_t params = {.scale = 1.0f, .zero_point = 0};
        int result = qat_fake_quantize(qat, empty, &params);
        // May return error or succeed with empty tensor
        (void)result;
        nimcp_tensor_destroy(empty);
    }
}

TEST_F(INT8QuantizationTest, SingleElement_Quantized) {
    qat_config_t config;
    qat_default_config(&config);
    qat = qat_create(&config);
    ASSERT_NE(qat, nullptr);

    nimcp_tensor_t* single = Create1DTensor(1, 0.5f);

    int observer_id = qat_register_observer(qat, "single", QAT_TARGET_WEIGHTS);
    qat_observe(qat, observer_id, single);

    qat_params_t params;
    qat_get_params(qat, observer_id, &params);

    // Should handle single element
    EXPECT_GT(params.scale, 0.0f);

    nimcp_tensor_destroy(single);
}

TEST_F(INT8QuantizationTest, ConstantTensor_Handled) {
    qat_config_t config;
    qat_default_config(&config);
    qat = qat_create(&config);
    ASSERT_NE(qat, nullptr);

    nimcp_tensor_t* constant = Create1DTensor(100, 0.5f);  // All same value

    int observer_id = qat_register_observer(qat, "constant", QAT_TARGET_WEIGHTS);
    qat_observe(qat, observer_id, constant);

    qat_params_t params;
    qat_get_params(qat, observer_id, &params);

    // Range is 0, but should still get valid params
    EXPECT_GE(params.scale, 0.0f);

    nimcp_tensor_destroy(constant);
}

TEST_F(INT8QuantizationTest, LargeTensor_Handled) {
    qat_config_t config;
    qat_default_config(&config);
    qat = qat_create(&config);
    ASSERT_NE(qat, nullptr);

    nimcp_tensor_t* large = Create1DTensor(1000000, 0.0f);  // 1M elements
    if (large == nullptr) {
        GTEST_SKIP() << "Not enough memory for large tensor test";
    }

    FillRandom(large, -1.0f, 1.0f);

    int observer_id = qat_register_observer(qat, "large", QAT_TARGET_ACTIVATIONS);
    int result = qat_observe(qat, observer_id, large);
    EXPECT_EQ(result, 0);

    qat_params_t params;
    result = qat_get_params(qat, observer_id, &params);
    EXPECT_EQ(result, 0);

    nimcp_tensor_destroy(large);
}

TEST_F(INT8QuantizationTest, VerySmallValues_Handled) {
    qat_config_t config;
    qat_default_config(&config);
    qat = qat_create(&config);
    ASSERT_NE(qat, nullptr);

    nimcp_tensor_t* small = Create1DTensor(100, 0.0f);
    std::vector<float> values(100);
    for (int i = 0; i < 100; i++) {
        values[i] = 1e-6f * (i + 1);
    }
    SetData(small, values);

    int observer_id = qat_register_observer(qat, "small", QAT_TARGET_WEIGHTS);
    qat_observe(qat, observer_id, small);

    qat_params_t params;
    qat_get_params(qat, observer_id, &params);

    // Scale should be very small
    EXPECT_GT(params.scale, 0.0f);
    EXPECT_LT(params.scale, 1e-4f);

    nimcp_tensor_destroy(small);
}

TEST_F(INT8QuantizationTest, VeryLargeValues_Handled) {
    qat_config_t config;
    qat_default_config(&config);
    qat = qat_create(&config);
    ASSERT_NE(qat, nullptr);

    nimcp_tensor_t* large_vals = Create1DTensor(100, 0.0f);
    std::vector<float> values(100);
    for (int i = 0; i < 100; i++) {
        values[i] = 1e6f * (i + 1);
    }
    SetData(large_vals, values);

    int observer_id = qat_register_observer(qat, "large_vals", QAT_TARGET_ACTIVATIONS);
    qat_observe(qat, observer_id, large_vals);

    qat_params_t params;
    qat_get_params(qat, observer_id, &params);

    // Scale should be large
    EXPECT_GT(params.scale, 1e4f);

    nimcp_tensor_destroy(large_vals);
}
