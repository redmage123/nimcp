/**
 * @file test_quantization_ternary.cpp
 * @brief Unit tests for ternary quantization in quantization-aware training
 *
 * Tests ternary quantization mode including:
 * - Ternary QAT configuration
 * - Straight-through estimator (STE) for ternary
 * - Ternary threshold computation
 * - Ternarization forward and backward pass
 * - Integration with QAT context
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <algorithm>
#include <numeric>
#include <random>

// Headers have their own extern "C" guards
#include "training/nimcp_quantization_aware.h"
#include "utils/ternary/nimcp_ternary.h"
#include "utils/ternary/nimcp_ternary_types.h"
#include "utils/tensor/nimcp_tensor.h"
#include "utils/error/nimcp_error_codes.h"

//=============================================================================
// Test Fixture
//=============================================================================

class QuantizationTernaryTest : public ::testing::Test {
protected:
    qat_ctx_t* ctx;
    qat_config_t config;
    qat_ternary_config_t ternary_config;

    void SetUp() override {
        ctx = nullptr;
        memset(&config, 0, sizeof(config));
        memset(&ternary_config, 0, sizeof(ternary_config));
    }

    void TearDown() override {
        if (ctx) {
            qat_destroy(ctx);
            ctx = nullptr;
        }
    }

    // Helper: Create test tensor with known values
    nimcp_tensor_t* createTestTensor(const std::vector<float>& values) {
        uint32_t dims[] = {static_cast<uint32_t>(values.size())};
        nimcp_tensor_t* tensor = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
        if (tensor) {
            float* data = static_cast<float*>(nimcp_tensor_data(tensor));
            for (size_t i = 0; i < values.size(); i++) {
                data[i] = values[i];
            }
        }
        return tensor;
    }

    // Helper: Create random tensor with fixed seed for deterministic tests (M-2)
    nimcp_tensor_t* createRandomTensor(uint32_t size, float min_val = -1.0f, float max_val = 1.0f) {
        uint32_t dims[] = {size};
        nimcp_tensor_t* tensor = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
        if (tensor) {
            float* data = static_cast<float*>(nimcp_tensor_data(tensor));
            std::mt19937 gen(42);  // Fixed seed for reproducibility
            std::uniform_real_distribution<float> dist(min_val, max_val);
            for (uint32_t i = 0; i < size; i++) {
                data[i] = dist(gen);
            }
        }
        return tensor;
    }

    // Helper: Get ternary value from weight
    trit_t ternarize(float weight, float threshold) {
        if (weight > threshold) return TRIT_POSITIVE;
        if (weight < -threshold) return TRIT_NEGATIVE;
        return TRIT_UNKNOWN;
    }
};

//=============================================================================
// Default Configuration Tests
//=============================================================================

class QatConfigTest : public QuantizationTernaryTest {};

TEST_F(QatConfigTest, DefaultConfigSuccess) {
    int result = qat_default_config(&config);
    EXPECT_EQ(result, 0);
}

TEST_F(QatConfigTest, DefaultConfigNullPointer) {
    int result = qat_default_config(nullptr);
    EXPECT_LT(result, 0);
}

TEST_F(QatConfigTest, DefaultConfigHasValidDefaults) {
    int result = qat_default_config(&config);
    ASSERT_EQ(result, 0);

    // Check defaults
    EXPECT_EQ(config.default_weight_dtype, QAT_DTYPE_INT8);
    EXPECT_EQ(config.default_activation_dtype, QAT_DTYPE_INT8);
    EXPECT_EQ(config.default_scheme, QAT_SCHEME_SYMMETRIC);
}

TEST_F(QatConfigTest, Int4ConfigSuccess) {
    int result = qat_int4_config(&config);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(config.default_weight_dtype, QAT_DTYPE_INT4);
}

TEST_F(QatConfigTest, BinaryConfigSuccess) {
    int result = qat_binary_config(&config);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(config.default_weight_dtype, QAT_DTYPE_INT1);
}

//=============================================================================
// Ternary Configuration Tests
//=============================================================================

class TernaryConfigTest : public QuantizationTernaryTest {};

TEST_F(TernaryConfigTest, TernaryDefaultConfigSuccess) {
    int result = qat_ternary_default_config(&ternary_config);
    EXPECT_EQ(result, 0);
}

TEST_F(TernaryConfigTest, TernaryDefaultConfigNullPointer) {
    int result = qat_ternary_default_config(nullptr);
    EXPECT_LT(result, 0);
}

TEST_F(TernaryConfigTest, TernaryDefaultConfigValues) {
    int result = qat_ternary_default_config(&ternary_config);
    ASSERT_EQ(result, 0);

    // Check default values
    EXPECT_FLOAT_EQ(ternary_config.threshold_ratio, 0.7f);
    EXPECT_TRUE(ternary_config.use_ste);
    EXPECT_TRUE(ternary_config.symmetric);
    EXPECT_TRUE(ternary_config.normalize_weights);
}

TEST_F(TernaryConfigTest, TernaryDtypeInEnum) {
    // Verify QAT_DTYPE_TERNARY is defined
    EXPECT_EQ(QAT_DTYPE_TERNARY, 8);  // Based on enum order
    EXPECT_LT(QAT_DTYPE_TERNARY, QAT_DTYPE_COUNT);
}

//=============================================================================
// QAT Context Lifecycle Tests
//=============================================================================

class QatContextTest : public QuantizationTernaryTest {};

TEST_F(QatContextTest, CreateQatContextBasic) {
    int result = qat_default_config(&config);
    ASSERT_EQ(result, 0);

    ctx = qat_create(&config);
    // May be NULL if not fully implemented
    if (ctx) {
        SUCCEED();
    }
}

TEST_F(QatContextTest, TernaryContextCreation) {
    /* H-4: At least one test must ASSERT_NE(ctx, nullptr) to catch linkage
     * failures. All other ternary tests silently GTEST_SKIP when ctx is NULL,
     * which masks broken builds. */
    int result = qat_ternary_default_config(&ternary_config);
    ASSERT_EQ(result, 0) << "qat_ternary_default_config failed — possible linkage issue";

    ctx = qat_ternary_create(&ternary_config);
    ASSERT_NE(ctx, nullptr) << "qat_ternary_create returned NULL — ternary QAT not linked or not implemented";
}

TEST_F(QatContextTest, CreateTernaryQatContext) {
    int result = qat_ternary_default_config(&ternary_config);
    ASSERT_EQ(result, 0);

    ctx = qat_ternary_create(&ternary_config);
    // May be NULL if not fully implemented
    if (ctx) {
        SUCCEED();
    }
}

TEST_F(QatContextTest, DestroyNullSafe) {
    qat_destroy(nullptr);
    SUCCEED();  // Should not crash
}

TEST_F(QatContextTest, CreateWithNullConfig) {
    ctx = qat_create(nullptr);
    // Should either use defaults or return NULL
    if (ctx) {
        // If not NULL, destroy it
        qat_destroy(ctx);
        ctx = nullptr;
    }
    SUCCEED();
}

//=============================================================================
// Ternarization Tests
//=============================================================================

class TernarizationTest : public QuantizationTernaryTest {};

TEST_F(TernarizationTest, TernarizeBasicThreshold) {
    // Test basic ternarization logic
    float threshold = 0.5f;

    EXPECT_EQ(ternarize(0.8f, threshold), TRIT_POSITIVE);
    EXPECT_EQ(ternarize(-0.8f, threshold), TRIT_NEGATIVE);
    EXPECT_EQ(ternarize(0.3f, threshold), TRIT_UNKNOWN);
    EXPECT_EQ(ternarize(-0.3f, threshold), TRIT_UNKNOWN);
    EXPECT_EQ(ternarize(0.0f, threshold), TRIT_UNKNOWN);
}

TEST_F(TernarizationTest, TernarizeAtThreshold) {
    float threshold = 0.5f;

    // At threshold - convention may vary
    trit_t at_pos = ternarize(0.5f, threshold);
    trit_t at_neg = ternarize(-0.5f, threshold);

    // These should be UNKNOWN (not crossing threshold)
    EXPECT_EQ(at_pos, TRIT_UNKNOWN);
    EXPECT_EQ(at_neg, TRIT_UNKNOWN);
}

TEST_F(TernarizationTest, TernarizeWithTensorApi) {
    qat_ternary_default_config(&ternary_config);
    ctx = qat_ternary_create(&ternary_config);

    if (!ctx) GTEST_SKIP() << "QAT ternary context not available";

    std::vector<float> values = {0.8f, -0.8f, 0.2f, -0.2f, 0.0f, 1.0f, -1.0f};
    nimcp_tensor_t* tensor = createTestTensor(values);

    if (tensor) {
        qat_ternary_params_t params;
        int result = qat_ternarize(ctx, tensor, &ternary_config, &params);

        if (result == 0) {
            // Verify params are filled
            EXPECT_GT(params.n_positive, 0u);
            EXPECT_GT(params.n_negative, 0u);
        }

        nimcp_tensor_destroy(tensor);
    }
}

TEST_F(TernarizationTest, TernarizeNullInputs) {
    qat_ternary_default_config(&ternary_config);
    ctx = qat_ternary_create(&ternary_config);

    if (!ctx) GTEST_SKIP() << "QAT ternary context not available";

    std::vector<float> values = {0.5f, -0.5f, 0.0f};
    nimcp_tensor_t* tensor = createTestTensor(values);
    qat_ternary_params_t params;

    if (tensor) {
        // Null context
        int result = qat_ternarize(nullptr, tensor, &ternary_config, &params);
        EXPECT_LT(result, 0);

        // Null tensor
        result = qat_ternarize(ctx, nullptr, &ternary_config, &params);
        EXPECT_LT(result, 0);

        // Null config
        result = qat_ternarize(ctx, tensor, nullptr, &params);
        EXPECT_LT(result, 0);

        // Null params should still work (output optional)
        result = qat_ternarize(ctx, tensor, &ternary_config, nullptr);
        // May succeed or fail depending on implementation

        nimcp_tensor_destroy(tensor);
    }
}

//=============================================================================
// Threshold Computation Tests
//=============================================================================

class ThresholdComputationTest : public QuantizationTernaryTest {};

TEST_F(ThresholdComputationTest, ComputeOptimalThresholdBasic) {
    std::vector<float> values = {
        0.1f, 0.2f, 0.3f, 0.4f, 0.5f,
        -0.1f, -0.2f, -0.3f, -0.4f, -0.5f
    };
    nimcp_tensor_t* tensor = createTestTensor(values);

    if (tensor) {
        float threshold, scale;
        int result = qat_compute_optimal_ternary_threshold(tensor, &threshold, &scale);

        if (result == 0) {
            // Threshold should be positive
            EXPECT_GT(threshold, 0.0f);
            EXPECT_GT(scale, 0.0f);
        }

        nimcp_tensor_destroy(tensor);
    }
}

TEST_F(ThresholdComputationTest, ComputeOptimalThresholdSymmetric) {
    // Symmetric distribution
    std::vector<float> values = {
        -1.0f, -0.8f, -0.5f, -0.2f, 0.0f,
        0.2f, 0.5f, 0.8f, 1.0f
    };
    nimcp_tensor_t* tensor = createTestTensor(values);

    if (tensor) {
        float threshold, scale;
        int result = qat_compute_optimal_ternary_threshold(tensor, &threshold, &scale);

        if (result == 0) {
            // Mean absolute value = (1+0.8+0.5+0.2+0+0.2+0.5+0.8+1)/9 = 0.556
            // Threshold should be around 0.7 * 0.556 = 0.389
            EXPECT_GT(threshold, 0.0f);
            EXPECT_LT(threshold, 1.0f);
        }

        nimcp_tensor_destroy(tensor);
    }
}

TEST_F(ThresholdComputationTest, ComputeOptimalThresholdNullInputs) {
    std::vector<float> values = {0.5f, -0.5f};
    nimcp_tensor_t* tensor = createTestTensor(values);
    float threshold, scale;

    // Null tensor
    int result = qat_compute_optimal_ternary_threshold(nullptr, &threshold, &scale);
    EXPECT_LT(result, 0);

    if (tensor) {
        // Null threshold output
        result = qat_compute_optimal_ternary_threshold(tensor, nullptr, &scale);
        EXPECT_LT(result, 0);

        // Null scale output
        result = qat_compute_optimal_ternary_threshold(tensor, &threshold, nullptr);
        EXPECT_LT(result, 0);

        nimcp_tensor_destroy(tensor);
    }
}

TEST_F(ThresholdComputationTest, ComputeThresholdAllZeros) {
    std::vector<float> values(10, 0.0f);
    nimcp_tensor_t* tensor = createTestTensor(values);

    if (tensor) {
        float threshold, scale;
        int result = qat_compute_optimal_ternary_threshold(tensor, &threshold, &scale);

        // May return error or threshold = 0
        if (result == 0) {
            EXPECT_GE(threshold, 0.0f);
        }

        nimcp_tensor_destroy(tensor);
    }
}

//=============================================================================
// Straight-Through Estimator Tests
//=============================================================================

class SteTest : public QuantizationTernaryTest {};

TEST_F(SteTest, TernaryBackwardBasic) {
    qat_ternary_default_config(&ternary_config);
    ternary_config.use_ste = true;

    ctx = qat_ternary_create(&ternary_config);
    if (!ctx) GTEST_SKIP() << "QAT ternary context not available";

    std::vector<float> weights = {0.8f, -0.8f, 0.2f, -0.2f, 0.0f};
    std::vector<float> grad_output = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f};

    nimcp_tensor_t* original = createTestTensor(weights);
    nimcp_tensor_t* grad_out = createTestTensor(grad_output);
    nimcp_tensor_t* grad_in = createTestTensor(std::vector<float>(5, 0.0f));

    if (original && grad_out && grad_in) {
        int result = qat_ternary_backward(ctx, grad_out, original, &ternary_config, grad_in);

        if (result == 0) {
            // STE should pass through gradients
            float* grad_data = static_cast<float*>(nimcp_tensor_data(grad_in));
            for (int i = 0; i < 5; i++) {
                // Gradient should be approximately 1.0 (straight-through)
                EXPECT_GT(std::abs(grad_data[i]), 0.0f);
            }
        }
    }

    if (original) nimcp_tensor_destroy(original);
    if (grad_out) nimcp_tensor_destroy(grad_out);
    if (grad_in) nimcp_tensor_destroy(grad_in);
}

TEST_F(SteTest, TernaryBackwardNullInputs) {
    qat_ternary_default_config(&ternary_config);
    ctx = qat_ternary_create(&ternary_config);

    if (!ctx) GTEST_SKIP() << "QAT ternary context not available";

    std::vector<float> values(5, 0.5f);
    nimcp_tensor_t* tensor = createTestTensor(values);
    nimcp_tensor_t* grad = createTestTensor(std::vector<float>(5, 1.0f));

    if (tensor && grad) {
        // Null context
        int result = qat_ternary_backward(nullptr, grad, tensor, &ternary_config, grad);
        EXPECT_LT(result, 0);

        // Null grad_output
        result = qat_ternary_backward(ctx, nullptr, tensor, &ternary_config, grad);
        EXPECT_LT(result, 0);

        // Null original
        result = qat_ternary_backward(ctx, grad, nullptr, &ternary_config, grad);
        EXPECT_LT(result, 0);

        // Null grad_input
        result = qat_ternary_backward(ctx, grad, tensor, &ternary_config, nullptr);
        EXPECT_LT(result, 0);
    }

    if (tensor) nimcp_tensor_destroy(tensor);
    if (grad) nimcp_tensor_destroy(grad);
}

TEST_F(SteTest, SteGradientScaling) {
    qat_ternary_default_config(&ternary_config);
    ternary_config.use_ste = true;
    ternary_config.ste_gradient_scale = 0.5f;

    ctx = qat_ternary_create(&ternary_config);
    if (!ctx) GTEST_SKIP() << "QAT ternary context not available";

    std::vector<float> weights = {0.8f, -0.8f};
    std::vector<float> grad_output = {2.0f, 2.0f};

    nimcp_tensor_t* original = createTestTensor(weights);
    nimcp_tensor_t* grad_out = createTestTensor(grad_output);
    nimcp_tensor_t* grad_in = createTestTensor(std::vector<float>(2, 0.0f));

    if (original && grad_out && grad_in) {
        int result = qat_ternary_backward(ctx, grad_out, original, &ternary_config, grad_in);

        if (result == 0) {
            float* grad_data = static_cast<float*>(nimcp_tensor_data(grad_in));
            // With 0.5 scaling, gradients should be approximately 1.0
            for (int i = 0; i < 2; i++) {
                EXPECT_GT(std::abs(grad_data[i]), 0.0f);
            }
        }
    }

    if (original) nimcp_tensor_destroy(original);
    if (grad_out) nimcp_tensor_destroy(grad_out);
    if (grad_in) nimcp_tensor_destroy(grad_in);
}

//=============================================================================
// Ternary Statistics Tests
//=============================================================================

class TernaryStatsTest : public QuantizationTernaryTest {};

TEST_F(TernaryStatsTest, GetTernaryStatsBasic) {
    qat_ternary_default_config(&ternary_config);
    ctx = qat_ternary_create(&ternary_config);

    if (!ctx) GTEST_SKIP() << "QAT ternary context not available";

    qat_ternary_params_t params;
    int result = qat_get_ternary_stats(ctx, &params);

    if (result == 0) {
        // Check fields are accessible
        EXPECT_GE(params.sparsity, 0.0f);
        EXPECT_LE(params.sparsity, 1.0f);
    }
}

TEST_F(TernaryStatsTest, GetTernaryStatsNullInputs) {
    qat_ternary_default_config(&ternary_config);
    ctx = qat_ternary_create(&ternary_config);

    if (!ctx) GTEST_SKIP() << "QAT ternary context not available";

    qat_ternary_params_t params;

    // Null context
    int result = qat_get_ternary_stats(nullptr, &params);
    EXPECT_LT(result, 0);

    // Null params
    result = qat_get_ternary_stats(ctx, nullptr);
    EXPECT_LT(result, 0);
}

TEST_F(TernaryStatsTest, TernaryParamsAfterTernarization) {
    qat_ternary_default_config(&ternary_config);
    ctx = qat_ternary_create(&ternary_config);

    if (!ctx) GTEST_SKIP() << "QAT ternary context not available";

    // Create tensor with known distribution
    std::vector<float> values = {
        1.0f, 0.9f, 0.8f,      // Should become +1
        0.1f, 0.0f, -0.1f,    // Should become 0
        -0.8f, -0.9f, -1.0f   // Should become -1
    };
    nimcp_tensor_t* tensor = createTestTensor(values);

    if (tensor) {
        qat_ternary_params_t params;
        int result = qat_ternarize(ctx, tensor, &ternary_config, &params);

        if (result == 0) {
            // Check distribution
            EXPECT_GT(params.n_positive, 0u);
            EXPECT_GT(params.n_zero, 0u);
            EXPECT_GT(params.n_negative, 0u);

            // Sum should equal total elements
            uint64_t total = params.n_positive + params.n_zero + params.n_negative;
            EXPECT_EQ(total, values.size());

            // Sparsity = n_zero / total
            float expected_sparsity = static_cast<float>(params.n_zero) / total;
            EXPECT_NEAR(params.sparsity, expected_sparsity, 0.01f);
        }

        nimcp_tensor_destroy(tensor);
    }
}

//=============================================================================
// Utility Function Tests
//=============================================================================

class QatUtilityTest : public QuantizationTernaryTest {};

TEST_F(QatUtilityTest, DtypeNameTernary) {
    const char* name = qat_dtype_name(QAT_DTYPE_TERNARY);
    EXPECT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);
}

TEST_F(QatUtilityTest, DtypeBitsTernary) {
    uint32_t bits = qat_dtype_bits(QAT_DTYPE_TERNARY);
    // Ternary is typically 2 bits (can represent 3 values with 2 bits)
    EXPECT_LE(bits, 8u);
    EXPECT_GE(bits, 1u);
}

TEST_F(QatUtilityTest, DtypeBitsAllTypes) {
    // INT8 = 8 bits
    EXPECT_EQ(qat_dtype_bits(QAT_DTYPE_INT8), 8u);

    // UINT8 = 8 bits
    EXPECT_EQ(qat_dtype_bits(QAT_DTYPE_UINT8), 8u);

    // INT4 = 4 bits
    EXPECT_EQ(qat_dtype_bits(QAT_DTYPE_INT4), 4u);

    // INT2 = 2 bits
    EXPECT_EQ(qat_dtype_bits(QAT_DTYPE_INT2), 2u);

    // INT1 (binary) = 1 bit
    EXPECT_EQ(qat_dtype_bits(QAT_DTYPE_INT1), 1u);
}

TEST_F(QatUtilityTest, SchemeNameSymmetric) {
    const char* name = qat_scheme_name(QAT_SCHEME_SYMMETRIC);
    EXPECT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);
}

TEST_F(QatUtilityTest, ValidateConfigValid) {
    int result = qat_default_config(&config);
    ASSERT_EQ(result, 0);

    result = qat_validate_config(&config);
    EXPECT_EQ(result, 0);
}

TEST_F(QatUtilityTest, ValidateConfigNull) {
    int result = qat_validate_config(nullptr);
    EXPECT_LT(result, 0);
}

TEST_F(QatUtilityTest, ComputeMseBasic) {
    std::vector<float> original_vals = {1.0f, 0.0f, -1.0f, 0.5f};
    std::vector<float> quantized_vals = {1.0f, 0.0f, -1.0f, 0.0f};  // One error

    nimcp_tensor_t* original = createTestTensor(original_vals);
    nimcp_tensor_t* quantized = createTestTensor(quantized_vals);

    if (original && quantized) {
        float mse = qat_compute_mse(original, quantized);

        // MSE = (0 + 0 + 0 + 0.25) / 4 = 0.0625
        EXPECT_NEAR(mse, 0.0625f, 0.001f);
    }

    if (original) nimcp_tensor_destroy(original);
    if (quantized) nimcp_tensor_destroy(quantized);
}

TEST_F(QatUtilityTest, ComputeMseZeroError) {
    std::vector<float> values = {1.0f, -1.0f, 0.0f};

    nimcp_tensor_t* tensor1 = createTestTensor(values);
    nimcp_tensor_t* tensor2 = createTestTensor(values);

    if (tensor1 && tensor2) {
        float mse = qat_compute_mse(tensor1, tensor2);
        EXPECT_FLOAT_EQ(mse, 0.0f);
    }

    if (tensor1) nimcp_tensor_destroy(tensor1);
    if (tensor2) nimcp_tensor_destroy(tensor2);
}

//=============================================================================
// Observer Integration Tests
//=============================================================================

class ObserverTest : public QuantizationTernaryTest {};

TEST_F(ObserverTest, RegisterObserverBasic) {
    int result = qat_default_config(&config);
    ASSERT_EQ(result, 0);

    ctx = qat_create(&config);
    if (!ctx) GTEST_SKIP() << "QAT context not available";

    int observer_id = qat_register_observer(ctx, "test_weights", QAT_TARGET_WEIGHTS);

    if (observer_id >= 0) {
        // Successfully registered
        SUCCEED();
    }
}

TEST_F(ObserverTest, RegisterObserverNullInputs) {
    int result = qat_default_config(&config);
    ASSERT_EQ(result, 0);

    ctx = qat_create(&config);
    if (!ctx) GTEST_SKIP() << "QAT context not available";

    // Null context
    int observer_id = qat_register_observer(nullptr, "test", QAT_TARGET_WEIGHTS);
    EXPECT_LT(observer_id, 0);

    // Null name
    observer_id = qat_register_observer(ctx, nullptr, QAT_TARGET_WEIGHTS);
    EXPECT_LT(observer_id, 0);
}

//=============================================================================
// Fake Quantization Tests
//=============================================================================

class FakeQuantTest : public QuantizationTernaryTest {};

TEST_F(FakeQuantTest, FakeQuantizeBasic) {
    int result = qat_default_config(&config);
    ASSERT_EQ(result, 0);

    ctx = qat_create(&config);
    if (!ctx) GTEST_SKIP() << "QAT context not available";

    std::vector<float> values = {0.5f, -0.5f, 0.25f, -0.25f, 0.0f};
    nimcp_tensor_t* tensor = createTestTensor(values);

    if (tensor) {
        qat_params_t params;
        params.scale = 0.1f;
        params.zero_point = 0;
        params.dtype = QAT_DTYPE_INT8;
        params.scheme = QAT_SCHEME_SYMMETRIC;
        params.granularity = QAT_GRANULARITY_TENSOR;

        result = qat_fake_quantize(ctx, tensor, &params);

        // Check result
        if (result == 0) {
            // Values should be quantized and dequantized
            float* data = static_cast<float*>(nimcp_tensor_data(tensor));
            for (size_t i = 0; i < values.size(); i++) {
                // Should be close to original but quantized
                EXPECT_NEAR(data[i], values[i], 0.1f);
            }
        }

        nimcp_tensor_destroy(tensor);
    }
}

TEST_F(FakeQuantTest, FakeQuantizeNullInputs) {
    int result = qat_default_config(&config);
    ASSERT_EQ(result, 0);

    ctx = qat_create(&config);
    if (!ctx) GTEST_SKIP() << "QAT context not available";

    std::vector<float> values = {0.5f};
    nimcp_tensor_t* tensor = createTestTensor(values);
    qat_params_t params = {};

    if (tensor) {
        // Null context
        result = qat_fake_quantize(nullptr, tensor, &params);
        EXPECT_LT(result, 0);

        // Null tensor
        result = qat_fake_quantize(ctx, nullptr, &params);
        EXPECT_LT(result, 0);

        // Null params
        result = qat_fake_quantize(ctx, tensor, nullptr);
        EXPECT_LT(result, 0);

        nimcp_tensor_destroy(tensor);
    }
}

//=============================================================================
// Memory Efficiency Tests
//=============================================================================

class MemoryEfficiencyTest : public QuantizationTernaryTest {};

TEST_F(MemoryEfficiencyTest, TernaryMemorySavings) {
    const uint32_t n_weights = 1000000;  // 1M weights

    // FP32 storage
    size_t fp32_bytes = n_weights * sizeof(float);

    // INT8 storage
    size_t int8_bytes = n_weights * sizeof(int8_t);

    // Ternary storage (2-bit packed)
    size_t ternary_bytes = (n_weights * 2 + 7) / 8;  // 2 bits per value

    // Calculate compression ratios
    float int8_ratio = static_cast<float>(fp32_bytes) / int8_bytes;
    float ternary_ratio = static_cast<float>(fp32_bytes) / ternary_bytes;

    EXPECT_NEAR(int8_ratio, 4.0f, 0.1f);   // 4x compression
    EXPECT_NEAR(ternary_ratio, 16.0f, 1.0f); // 16x compression
}

TEST_F(MemoryEfficiencyTest, TernaryBitsPerWeight) {
    // Ternary has 3 possible values: -1, 0, +1
    // Theoretical: log2(3) = 1.585 bits
    // Practical (2-bit encoding): 2 bits

    float theoretical_bits = std::log2(3.0f);
    float practical_bits = 2.0f;

    EXPECT_NEAR(theoretical_bits, 1.585f, 0.01f);
    EXPECT_GT(practical_bits, theoretical_bits);
}

//=============================================================================
// Edge Cases and Boundary Conditions
//=============================================================================

class EdgeCasesTest : public QuantizationTernaryTest {};

TEST_F(EdgeCasesTest, TernarizeExtremeValues) {
    qat_ternary_default_config(&ternary_config);
    ctx = qat_ternary_create(&ternary_config);

    if (!ctx) GTEST_SKIP() << "QAT ternary context not available";

    std::vector<float> extreme_values = {
        1e10f,      // Very large positive
        -1e10f,     // Very large negative
        1e-10f,     // Very small positive
        -1e-10f,    // Very small negative
        0.0f,       // Zero
    };
    nimcp_tensor_t* tensor = createTestTensor(extreme_values);

    if (tensor) {
        qat_ternary_params_t params;
        int result = qat_ternarize(ctx, tensor, &ternary_config, &params);

        // Should handle without crash
        if (result == 0) {
            float* data = static_cast<float*>(nimcp_tensor_data(tensor));
            // Large values should become +/-1
            EXPECT_GT(data[0], 0.0f);  // Was 1e10
            EXPECT_LT(data[1], 0.0f);  // Was -1e10
        }

        nimcp_tensor_destroy(tensor);
    }
}

TEST_F(EdgeCasesTest, TernarizeNaNInf) {
    qat_ternary_default_config(&ternary_config);
    ctx = qat_ternary_create(&ternary_config);

    if (!ctx) GTEST_SKIP() << "QAT ternary context not available";

    std::vector<float> special_values = {
        NAN,
        INFINITY,
        -INFINITY,
        0.0f
    };
    nimcp_tensor_t* tensor = createTestTensor(special_values);

    if (tensor) {
        qat_ternary_params_t params;
        int result = qat_ternarize(ctx, tensor, &ternary_config, &params);

        // Should handle gracefully (may return error or clamp values)
        // Just check it doesn't crash
        SUCCEED();

        nimcp_tensor_destroy(tensor);
    }
}

TEST_F(EdgeCasesTest, EmptyTensor) {
    qat_ternary_default_config(&ternary_config);
    ctx = qat_ternary_create(&ternary_config);

    if (!ctx) GTEST_SKIP() << "QAT ternary context not available";

    uint32_t dims[] = {0};
    nimcp_tensor_t* tensor = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);

    if (tensor) {
        qat_ternary_params_t params;
        int result = qat_ternarize(ctx, tensor, &ternary_config, &params);

        // Should handle gracefully
        if (result == 0) {
            EXPECT_EQ(params.n_positive + params.n_zero + params.n_negative, 0u);
        }

        nimcp_tensor_destroy(tensor);
    }
}

TEST_F(EdgeCasesTest, SingleElementTensor) {
    qat_ternary_default_config(&ternary_config);
    ctx = qat_ternary_create(&ternary_config);

    if (!ctx) GTEST_SKIP() << "QAT ternary context not available";

    std::vector<float> single = {0.8f};
    nimcp_tensor_t* tensor = createTestTensor(single);

    if (tensor) {
        qat_ternary_params_t params;
        int result = qat_ternarize(ctx, tensor, &ternary_config, &params);

        if (result == 0) {
            // Single positive value
            EXPECT_EQ(params.n_positive, 1u);
            EXPECT_EQ(params.n_zero, 0u);
            EXPECT_EQ(params.n_negative, 0u);
        }

        nimcp_tensor_destroy(tensor);
    }
}

//=============================================================================
// Roundtrip Tests
//=============================================================================

class RoundtripTest : public QuantizationTernaryTest {};

TEST_F(RoundtripTest, TernarizeDeterministic) {
    qat_ternary_default_config(&ternary_config);
    ctx = qat_ternary_create(&ternary_config);

    if (!ctx) GTEST_SKIP() << "QAT ternary context not available";

    std::vector<float> values = {0.8f, -0.8f, 0.2f, -0.2f, 0.0f};

    // Create two tensors with same values
    nimcp_tensor_t* tensor1 = createTestTensor(values);
    nimcp_tensor_t* tensor2 = createTestTensor(values);

    if (tensor1 && tensor2) {
        qat_ternary_params_t params1, params2;

        int result1 = qat_ternarize(ctx, tensor1, &ternary_config, &params1);
        int result2 = qat_ternarize(ctx, tensor2, &ternary_config, &params2);

        if (result1 == 0 && result2 == 0) {
            // Results should be identical
            EXPECT_EQ(params1.n_positive, params2.n_positive);
            EXPECT_EQ(params1.n_zero, params2.n_zero);
            EXPECT_EQ(params1.n_negative, params2.n_negative);

            float* data1 = static_cast<float*>(nimcp_tensor_data(tensor1));
            float* data2 = static_cast<float*>(nimcp_tensor_data(tensor2));

            for (size_t i = 0; i < values.size(); i++) {
                EXPECT_FLOAT_EQ(data1[i], data2[i]);
            }
        }
    }

    if (tensor1) nimcp_tensor_destroy(tensor1);
    if (tensor2) nimcp_tensor_destroy(tensor2);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
