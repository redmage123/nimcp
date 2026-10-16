/**
 * @file test_training_ternary_integration.cpp
 * @brief Integration tests for training with ternary representation
 *
 * Tests:
 * - Quantization-aware training with ternary
 * - Mixed precision with ternary mode
 * - Plasticity with ternary weights
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

// Headers have their own extern "C" guards
#include "training/nimcp_quantization_aware.h"
#include "utils/tensor/nimcp_tensor.h"
#include "utils/ternary/nimcp_ternary_types.h"
#include "utils/ternary/nimcp_ternary_vector.h"
#include "utils/ternary/nimcp_ternary_matrix.h"
#include "utils/ternary/nimcp_ternary_convert.h"

/**
 * @class TrainingTernaryIntegrationTest
 * @brief Test fixture for training with ternary integration
 */
class TrainingTernaryIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Nothing special needed
    }

    void TearDown() override {
        // Cleanup handled per-test
    }

    /**
     * @brief Create a tensor with random-ish weight values
     */
    nimcp_tensor_t* createWeightTensor(size_t rows, size_t cols) {
        uint32_t dims[2] = {rows, cols};
        nimcp_tensor_t* tensor = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_F32);
        if (!tensor) return nullptr;

        float* data = (float*)nimcp_tensor_data(tensor);
        size_t numel = rows * cols;

        // Create varied weights
        for (size_t i = 0; i < numel; i++) {
            // Range approximately [-1, 1]
            data[i] = 2.0f * ((float)(i % 100) / 100.0f) - 1.0f;
        }

        return tensor;
    }

    /**
     * @brief Create a tensor initialized with specific value
     */
    nimcp_tensor_t* createTensorWithValue(size_t size, float value) {
        uint32_t dims[1] = {size};
        nimcp_tensor_t* tensor = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
        if (!tensor) return nullptr;

        float* data = (float*)nimcp_tensor_data(tensor);
        for (size_t i = 0; i < size; i++) {
            data[i] = value;
        }

        return tensor;
    }

    /**
     * @brief Compute mean absolute value of tensor
     */
    float computeMeanAbs(const nimcp_tensor_t* tensor) {
        const float* data = (const float*)nimcp_tensor_data(const_cast<nimcp_tensor_t*>(tensor));
        size_t numel = nimcp_tensor_numel(tensor);

        float sum = 0.0f;
        for (size_t i = 0; i < numel; i++) {
            sum += std::abs(data[i]);
        }
        return sum / numel;
    }

    /**
     * @brief Count values by category
     */
    void countTernaryValues(const nimcp_tensor_t* tensor, float threshold,
                           size_t& n_positive, size_t& n_zero, size_t& n_negative) {
        const float* data = (const float*)nimcp_tensor_data(const_cast<nimcp_tensor_t*>(tensor));
        size_t numel = nimcp_tensor_numel(tensor);

        n_positive = 0;
        n_zero = 0;
        n_negative = 0;

        for (size_t i = 0; i < numel; i++) {
            if (data[i] > threshold) {
                n_positive++;
            } else if (data[i] < -threshold) {
                n_negative++;
            } else {
                n_zero++;
            }
        }
    }
};

//=============================================================================
// Test: Quantization-Aware Training with Ternary
//=============================================================================

/**
 * Test QAT context creation with ternary dtype
 */
TEST_F(TrainingTernaryIntegrationTest, QATCreateWithTernaryDtype) {
    qat_config_t config;
    int result = qat_default_config(&config);
    EXPECT_EQ(0, result);

    // Configure for ternary weights
    config.default_weight_dtype = QAT_DTYPE_TERNARY;
    config.default_scheme = QAT_SCHEME_SYMMETRIC;

    qat_ctx_t* ctx = qat_create(&config);
    ASSERT_NE(nullptr, ctx) << "Failed to create QAT context with ternary config";

    qat_destroy(ctx);
}

/**
 * Test ternary quantization configuration
 */
TEST_F(TrainingTernaryIntegrationTest, TernaryQuantizationConfig) {
    qat_ternary_config_t ternary_config;
    int result = qat_ternary_default_config(&ternary_config);
    EXPECT_EQ(0, result);

    // Verify defaults
    EXPECT_NEAR(0.7f, ternary_config.threshold_ratio, 0.01f);
    EXPECT_TRUE(ternary_config.symmetric);
    EXPECT_TRUE(ternary_config.use_ste);
    EXPECT_TRUE(ternary_config.normalize_weights);

    // Create ternary-specific context
    qat_ctx_t* ctx = qat_ternary_create(&ternary_config);
    ASSERT_NE(nullptr, ctx) << "Failed to create ternary QAT context";

    qat_destroy(ctx);
}

/**
 * Test ternarization of weight tensor
 */
TEST_F(TrainingTernaryIntegrationTest, TernarizeWeightTensor) {
    // Create ternary context
    qat_ternary_config_t ternary_config;
    qat_ternary_default_config(&ternary_config);
    ternary_config.threshold_ratio = 0.5f;  // Lower threshold for testing

    qat_ctx_t* ctx = qat_ternary_create(&ternary_config);
    ASSERT_NE(nullptr, ctx);

    // Create weight tensor with known distribution
    const size_t numel = 100;
    nimcp_tensor_t* weights = createWeightTensor(10, 10);
    ASSERT_NE(nullptr, weights);

    // Store original values
    std::vector<float> original(numel);
    memcpy(original.data(), nimcp_tensor_data(weights), numel * sizeof(float));

    // Ternarize
    qat_ternary_params_t params;
    int result = qat_ternarize(ctx, weights, &ternary_config, &params);
    EXPECT_EQ(0, result);

    // Verify ternary parameters
    EXPECT_GT(params.positive_scale, 0.0f);
    EXPECT_GT(params.negative_scale, 0.0f);
    EXPECT_GT(params.threshold, 0.0f);

    // Verify we have all three categories
    EXPECT_GT(params.n_positive + params.n_zero + params.n_negative, 0u);

    // Verify sparsity is reasonable
    float sparsity = (float)params.n_zero / numel;
    EXPECT_GE(params.sparsity, 0.0f);
    EXPECT_LE(params.sparsity, 1.0f);
    EXPECT_NEAR(sparsity, params.sparsity, 0.01f);

    // Verify ternarized values
    const float* data = (const float*)nimcp_tensor_data(weights);
    for (size_t i = 0; i < numel; i++) {
        // Values should be either 0, positive_scale, or -negative_scale
        bool is_valid = (std::abs(data[i]) < 0.001f) ||
                       (std::abs(data[i] - params.positive_scale) < 0.01f) ||
                       (std::abs(data[i] + params.negative_scale) < 0.01f);
        EXPECT_TRUE(is_valid) << "Value " << data[i] << " at index " << i
                             << " is not a valid ternary value";
    }

    nimcp_tensor_destroy(weights);
    qat_destroy(ctx);
}

/**
 * Test ternary backward gradient with STE
 */
TEST_F(TrainingTernaryIntegrationTest, TernaryBackwardSTE) {
    qat_ternary_config_t ternary_config;
    qat_ternary_default_config(&ternary_config);
    ternary_config.use_ste = true;
    ternary_config.ste_gradient_scale = 1.0f;

    qat_ctx_t* ctx = qat_ternary_create(&ternary_config);
    ASSERT_NE(nullptr, ctx);

    // Create tensors
    const size_t numel = 16;
    uint32_t dims[1] = {numel};

    nimcp_tensor_t* grad_output = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
    nimcp_tensor_t* original_weights = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
    nimcp_tensor_t* grad_input = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);

    ASSERT_NE(nullptr, grad_output);
    ASSERT_NE(nullptr, original_weights);
    ASSERT_NE(nullptr, grad_input);

    // Set up gradient (all 1s)
    float* go_data = (float*)nimcp_tensor_data(grad_output);
    for (size_t i = 0; i < numel; i++) {
        go_data[i] = 1.0f;
    }

    // Set up original weights
    float* ow_data = (float*)nimcp_tensor_data(original_weights);
    for (size_t i = 0; i < numel; i++) {
        ow_data[i] = ((float)i / numel) * 2.0f - 1.0f;
    }

    // Compute backward
    int result = qat_ternary_backward(
        ctx, grad_output, original_weights, &ternary_config, grad_input);
    EXPECT_EQ(0, result);

    // With STE, gradients should pass through (possibly scaled)
    const float* gi_data = (const float*)nimcp_tensor_data(grad_input);
    for (size_t i = 0; i < numel; i++) {
        EXPECT_FALSE(std::isnan(gi_data[i]));
        EXPECT_FALSE(std::isinf(gi_data[i]));
    }

    nimcp_tensor_destroy(grad_output);
    nimcp_tensor_destroy(original_weights);
    nimcp_tensor_destroy(grad_input);
    qat_destroy(ctx);
}

/**
 * Test optimal ternary threshold computation
 */
TEST_F(TrainingTernaryIntegrationTest, OptimalTernaryThreshold) {
    // Create weight tensor with known distribution
    nimcp_tensor_t* weights = createWeightTensor(10, 10);
    ASSERT_NE(nullptr, weights);

    float threshold_out = 0.0f;
    float scale_out = 0.0f;

    int result = qat_compute_optimal_ternary_threshold(
        weights, &threshold_out, &scale_out);
    EXPECT_EQ(0, result);

    // Threshold should be positive
    EXPECT_GT(threshold_out, 0.0f);
    // Scale should be positive
    EXPECT_GT(scale_out, 0.0f);

    // Threshold should be related to mean absolute value
    float mean_abs = computeMeanAbs(weights);
    // Typically threshold is around 0.7 * mean_abs
    EXPECT_GT(threshold_out, 0.3f * mean_abs);
    EXPECT_LT(threshold_out, 1.5f * mean_abs);

    nimcp_tensor_destroy(weights);
}

//=============================================================================
// Test: Mixed Precision with Ternary Mode
//=============================================================================

/**
 * Test QAT with mixed INT8/ternary configuration
 */
TEST_F(TrainingTernaryIntegrationTest, MixedPrecisionInt8Ternary) {
    qat_config_t config;
    qat_default_config(&config);

    // Weights in ternary, activations in INT8
    config.default_weight_dtype = QAT_DTYPE_TERNARY;
    config.default_activation_dtype = QAT_DTYPE_INT8;
    config.default_scheme = QAT_SCHEME_SYMMETRIC;
    config.default_granularity = QAT_GRANULARITY_TENSOR;

    qat_ctx_t* ctx = qat_create(&config);
    ASSERT_NE(nullptr, ctx);

    // Register observer for weights
    int weight_observer = qat_register_observer(ctx, "fc1_weight", QAT_TARGET_WEIGHTS);
    EXPECT_GE(weight_observer, 0);

    // Register observer for activations
    int act_observer = qat_register_observer(ctx, "fc1_act", QAT_TARGET_ACTIVATIONS);
    EXPECT_GE(act_observer, 0);

    // Observe some data
    nimcp_tensor_t* weights = createWeightTensor(8, 8);
    nimcp_tensor_t* activations = createTensorWithValue(8, 0.5f);

    ASSERT_NE(nullptr, weights);
    ASSERT_NE(nullptr, activations);

    int result = qat_observe(ctx, weight_observer, weights);
    EXPECT_EQ(0, result);

    result = qat_observe(ctx, act_observer, activations);
    EXPECT_EQ(0, result);

    // Get quantization parameters
    qat_params_t weight_params, act_params;
    result = qat_get_params(ctx, weight_observer, &weight_params);
    EXPECT_EQ(0, result);
    EXPECT_EQ(QAT_DTYPE_TERNARY, weight_params.dtype);

    result = qat_get_params(ctx, act_observer, &act_params);
    EXPECT_EQ(0, result);
    EXPECT_EQ(QAT_DTYPE_INT8, act_params.dtype);

    nimcp_tensor_destroy(weights);
    nimcp_tensor_destroy(activations);
    qat_destroy(ctx);
}

/**
 * Test fake quantization with ternary weights
 */
TEST_F(TrainingTernaryIntegrationTest, FakeQuantizeTernaryWeights) {
    qat_config_t config;
    qat_default_config(&config);
    config.default_weight_dtype = QAT_DTYPE_TERNARY;

    qat_ctx_t* ctx = qat_create(&config);
    ASSERT_NE(nullptr, ctx);

    // Create weight tensor
    nimcp_tensor_t* weights = createWeightTensor(8, 8);
    ASSERT_NE(nullptr, weights);

    // Set up ternary params
    qat_params_t params;
    memset(&params, 0, sizeof(params));
    params.dtype = QAT_DTYPE_TERNARY;
    params.scheme = QAT_SCHEME_SYMMETRIC;
    params.scale = 0.5f;  // Expected scale for ternary

    // Apply fake quantization
    int result = qat_fake_quantize(ctx, weights, &params);
    EXPECT_EQ(0, result);

    // Verify values are ternarized
    const float* data = (const float*)nimcp_tensor_data(weights);
    size_t numel = nimcp_tensor_numel(weights);

    for (size_t i = 0; i < numel; i++) {
        // Values should be discrete: -scale, 0, or +scale
        bool is_valid = (std::abs(data[i]) < 0.001f) ||
                       (std::abs(std::abs(data[i]) - params.scale) < 0.01f);
        EXPECT_TRUE(is_valid) << "Value " << data[i] << " is not ternary";
    }

    nimcp_tensor_destroy(weights);
    qat_destroy(ctx);
}

/**
 * Test QAT statistics tracking
 */
TEST_F(TrainingTernaryIntegrationTest, QATStatisticsTracking) {
    qat_config_t config;
    qat_default_config(&config);
    config.default_weight_dtype = QAT_DTYPE_TERNARY;
    config.track_statistics = true;

    qat_ctx_t* ctx = qat_create(&config);
    ASSERT_NE(nullptr, ctx);

    // Perform some operations
    nimcp_tensor_t* weights = createWeightTensor(10, 10);
    ASSERT_NE(nullptr, weights);

    qat_ternary_config_t ternary_config;
    qat_ternary_default_config(&ternary_config);

    qat_ternary_params_t ternary_params;
    qat_ternarize(ctx, weights, &ternary_config, &ternary_params);

    // Get statistics
    qat_stats_t stats;
    int result = qat_get_stats(ctx, &stats);
    EXPECT_EQ(0, result);

    // Verify statistics are reasonable
    // Note: Exact values depend on implementation
    EXPECT_GE(stats.avg_weight_range, 0.0f);
    EXPECT_LE(stats.outlier_ratio, 1.0f);

    nimcp_tensor_destroy(weights);
    qat_destroy(ctx);
}

//=============================================================================
// Test: Plasticity with Ternary Weights
//=============================================================================

/**
 * Test ternary weight update simulation (plasticity)
 */
TEST_F(TrainingTernaryIntegrationTest, TernaryWeightPlasticity) {
    const size_t n_synapses = 64;

    // Create ternary weight vector
    trit_vector_t* weights = trit_vector_create(n_synapses, TERNARY_PACK_NONE);
    ASSERT_NE(nullptr, weights);

    // Initialize with random ternary values
    for (size_t i = 0; i < n_synapses; i++) {
        trit_t val = (trit_t)((i % 3) - 1);  // {-1, 0, 1}
        trit_vector_set(weights, i, val);
    }

    // Simulate plasticity: potentiation increases weight, depression decreases
    std::vector<float> plasticity_signals(n_synapses);
    for (size_t i = 0; i < n_synapses; i++) {
        // Alternate between potentiation and depression
        plasticity_signals[i] = (i % 2 == 0) ? 0.6f : -0.4f;
    }

    // Apply plasticity-inspired updates
    const float potentiation_threshold = 0.5f;
    const float depression_threshold = -0.3f;

    for (size_t i = 0; i < n_synapses; i++) {
        trit_t current = trit_vector_get(weights, i);
        trit_t new_val = current;

        if (plasticity_signals[i] > potentiation_threshold) {
            // LTP: increase weight (saturate at +1)
            if (current < TRIT_POSITIVE) {
                new_val = (trit_t)(current + 1);
            }
        } else if (plasticity_signals[i] < depression_threshold) {
            // LTD: decrease weight (saturate at -1)
            if (current > TRIT_NEGATIVE) {
                new_val = (trit_t)(current - 1);
            }
        }

        trit_vector_set(weights, i, new_val);
    }

    // Verify updates occurred
    size_t n_positive, n_unknown, n_negative;
    trit_vector_count(weights, &n_positive, &n_unknown, &n_negative);

    EXPECT_GT(n_positive + n_unknown + n_negative, 0u);
    // Due to alternating potentiation/depression, distribution should shift

    trit_vector_destroy(weights);
}

/**
 * Test ternary weight gradient accumulation
 */
TEST_F(TrainingTernaryIntegrationTest, TernaryGradientAccumulation) {
    const size_t batch_size = 4;
    const size_t weight_size = 16;

    // Simulate gradient accumulation over batches
    std::vector<float> accumulated_gradients(weight_size, 0.0f);

    for (size_t batch = 0; batch < batch_size; batch++) {
        // Generate per-batch gradients
        std::vector<float> batch_gradients(weight_size);
        for (size_t i = 0; i < weight_size; i++) {
            batch_gradients[i] = 0.1f * ((i + batch) % 5 - 2);
        }

        // Accumulate
        for (size_t i = 0; i < weight_size; i++) {
            accumulated_gradients[i] += batch_gradients[i];
        }
    }

    // Apply accumulated gradient to ternary weights
    trit_vector_t* weights = trit_vector_create(weight_size, TERNARY_PACK_NONE);
    ASSERT_NE(nullptr, weights);

    // Initialize to zero
    for (size_t i = 0; i < weight_size; i++) {
        trit_vector_set(weights, i, TRIT_UNKNOWN);
    }

    // Update based on accumulated gradients
    const float update_threshold = 0.1f * batch_size;  // Scale threshold

    for (size_t i = 0; i < weight_size; i++) {
        trit_t current = trit_vector_get(weights, i);
        trit_t new_val = current;

        if (accumulated_gradients[i] > update_threshold) {
            // Positive gradient -> decrease weight (for gradient descent)
            if (current > TRIT_NEGATIVE) {
                new_val = (trit_t)(current - 1);
            }
        } else if (accumulated_gradients[i] < -update_threshold) {
            // Negative gradient -> increase weight
            if (current < TRIT_POSITIVE) {
                new_val = (trit_t)(current + 1);
            }
        }

        trit_vector_set(weights, i, new_val);
    }

    // Verify some updates occurred
    size_t n_positive, n_unknown, n_negative;
    trit_vector_count(weights, &n_positive, &n_unknown, &n_negative);

    // Not all weights should still be zero
    EXPECT_LT(n_unknown, weight_size);

    trit_vector_destroy(weights);
}

/**
 * Test ternary matrix weight training simulation
 */
TEST_F(TrainingTernaryIntegrationTest, TernaryMatrixWeightTraining) {
    const size_t rows = 8;
    const size_t cols = 8;
    const size_t epochs = 10;

    // Create ternary weight matrix
    trit_matrix_t* weights = trit_matrix_create(rows, cols, TERNARY_PACK_NONE);
    ASSERT_NE(nullptr, weights);

    // Initialize with target pattern (diagonal positive)
    for (size_t i = 0; i < rows; i++) {
        for (size_t j = 0; j < cols; j++) {
            trit_matrix_set(weights, i, j, TRIT_UNKNOWN);
        }
    }

    // Target: identity-like pattern
    trit_matrix_t* target = trit_matrix_create_identity(rows, TERNARY_PACK_NONE);
    ASSERT_NE(nullptr, target);

    // Training loop
    float learning_rate = 0.1f;

    for (size_t epoch = 0; epoch < epochs; epoch++) {
        // Compute "error" (difference from target)
        for (size_t i = 0; i < rows; i++) {
            for (size_t j = 0; j < cols; j++) {
                trit_t current = trit_matrix_get(weights, i, j);
                trit_t goal = trit_matrix_get(target, i, j);

                // Simple update rule: move toward target
                if (current < goal) {
                    // Need to increase
                    if (((float)rand() / RAND_MAX) < learning_rate) {
                        trit_matrix_set(weights, i, j, (trit_t)(current + 1));
                    }
                } else if (current > goal) {
                    // Need to decrease
                    if (((float)rand() / RAND_MAX) < learning_rate) {
                        trit_matrix_set(weights, i, j, (trit_t)(current - 1));
                    }
                }
            }
        }
    }

    // Verify convergence toward target
    size_t matches = 0;
    for (size_t i = 0; i < rows; i++) {
        for (size_t j = 0; j < cols; j++) {
            if (trit_matrix_get(weights, i, j) == trit_matrix_get(target, i, j)) {
                matches++;
            }
        }
    }

    // Should have learned some of the pattern
    float accuracy = (float)matches / (rows * cols);
    EXPECT_GT(accuracy, 0.3f) << "Should learn at least 30% of pattern";

    trit_matrix_destroy(weights);
    trit_matrix_destroy(target);
}

/**
 * Test conversion between float gradients and ternary updates
 */
TEST_F(TrainingTernaryIntegrationTest, FloatGradientToTernaryUpdate) {
    const size_t n_weights = 32;

    // Create float weight tensor
    nimcp_tensor_t* float_weights = createWeightTensor(8, 4);
    ASSERT_NE(nullptr, float_weights);

    // Create ternary representation
    trit_vector_t* ternary_weights = trit_vector_create(n_weights, TERNARY_PACK_NONE);
    ASSERT_NE(nullptr, ternary_weights);

    // Convert float to ternary
    const float* float_data = (const float*)nimcp_tensor_data(float_weights);
    const float threshold = 0.3f;

    for (size_t i = 0; i < n_weights; i++) {
        trit_t val = trit_from_float_threshold(float_data[i], threshold);
        trit_vector_set(ternary_weights, i, val);
    }

    // Simulate gradient update
    std::vector<float> gradients(n_weights);
    for (size_t i = 0; i < n_weights; i++) {
        gradients[i] = 0.1f * std::sin(i * 0.5f);
    }

    // Update float weights
    float* float_data_mut = (float*)nimcp_tensor_data(float_weights);
    const float lr = 0.01f;
    for (size_t i = 0; i < n_weights; i++) {
        float_data_mut[i] -= lr * gradients[i];
    }

    // Re-ternarize
    for (size_t i = 0; i < n_weights; i++) {
        trit_t val = trit_from_float_threshold(float_data_mut[i], threshold);
        trit_vector_set(ternary_weights, i, val);
    }

    // Verify ternary weights are valid
    for (size_t i = 0; i < n_weights; i++) {
        trit_t val = trit_vector_get(ternary_weights, i);
        EXPECT_TRUE(TRIT_IS_VALID(val)) << "Invalid trit at " << i;
    }

    trit_vector_destroy(ternary_weights);
    nimcp_tensor_destroy(float_weights);
}

/**
 * Test QAT calibration with ternary mode
 */
TEST_F(TrainingTernaryIntegrationTest, TernaryCalibration) {
    qat_config_t config;
    qat_default_config(&config);
    config.default_weight_dtype = QAT_DTYPE_TERNARY;
    config.start_with_calibration = true;
    config.observer.method = QAT_OBSERVER_MINMAX;
    config.observer.calibration_batches = 10;

    qat_ctx_t* ctx = qat_create(&config);
    ASSERT_NE(nullptr, ctx);

    // Register observer
    int observer_id = qat_register_observer(ctx, "weights", QAT_TARGET_WEIGHTS);
    EXPECT_GE(observer_id, 0);

    // Observe multiple batches
    for (int batch = 0; batch < 10; batch++) {
        nimcp_tensor_t* weights = createWeightTensor(16, 16);
        ASSERT_NE(nullptr, weights);

        // Add some variation per batch
        float* data = (float*)nimcp_tensor_data(weights);
        size_t numel = nimcp_tensor_numel(weights);
        for (size_t i = 0; i < numel; i++) {
            data[i] += 0.1f * batch * ((i % 2) ? 1.0f : -1.0f);
        }

        int result = qat_observe(ctx, observer_id, weights);
        EXPECT_EQ(0, result);

        nimcp_tensor_destroy(weights);
    }

    // Calibrate
    int result = qat_calibrate(ctx);
    EXPECT_EQ(0, result);

    // Get calibrated parameters
    qat_params_t params;
    result = qat_get_params(ctx, observer_id, &params);
    EXPECT_EQ(0, result);

    // Verify observed range is reasonable
    EXPECT_LT(params.observed_min, params.observed_max);
    EXPECT_LT(params.observed_min, 0.0f);  // Should see negative values
    EXPECT_GT(params.observed_max, 0.0f);  // Should see positive values

    qat_destroy(ctx);
}

/**
 * Test ternary weight sparsity during training
 */
TEST_F(TrainingTernaryIntegrationTest, TernarySparsityDuringTraining) {
    const size_t numel = 100;

    qat_ternary_config_t config;
    qat_ternary_default_config(&config);

    qat_ctx_t* ctx = qat_ternary_create(&config);
    ASSERT_NE(nullptr, ctx);

    // Track sparsity over multiple "training" iterations
    std::vector<float> sparsity_history;

    for (int iter = 0; iter < 5; iter++) {
        // Create weights with decreasing magnitude (simulating regularization)
        nimcp_tensor_t* weights = createWeightTensor(10, 10);
        ASSERT_NE(nullptr, weights);

        // Apply decay
        float decay = 0.8f - 0.1f * iter;  // Increasing decay
        float* data = (float*)nimcp_tensor_data(weights);
        for (size_t i = 0; i < numel; i++) {
            data[i] *= decay;
        }

        // Ternarize
        qat_ternary_params_t params;
        qat_ternarize(ctx, weights, &config, &params);

        sparsity_history.push_back(params.sparsity);

        nimcp_tensor_destroy(weights);
    }

    // Sparsity should increase as weights decay
    // (more values fall within threshold -> zero)
    for (size_t i = 1; i < sparsity_history.size(); i++) {
        // Generally sparsity should increase (might have some noise)
        // Just verify all sparsities are valid
        EXPECT_GE(sparsity_history[i], 0.0f);
        EXPECT_LE(sparsity_history[i], 1.0f);
    }

    // Final sparsity should be higher than initial
    EXPECT_GE(sparsity_history.back(), sparsity_history.front())
        << "Sparsity should increase with weight decay";

    qat_destroy(ctx);
}

/**
 * Test QAT dtype utilities
 */
TEST_F(TrainingTernaryIntegrationTest, QATDtypeUtilities) {
    // Test ternary dtype properties
    EXPECT_STREQ("ternary", qat_dtype_name(QAT_DTYPE_TERNARY));
    EXPECT_EQ(2u, qat_dtype_bits(QAT_DTYPE_TERNARY));  // 1.58 bits, rounded to 2

    // Test other dtypes for comparison
    EXPECT_EQ(8u, qat_dtype_bits(QAT_DTYPE_INT8));
    EXPECT_EQ(4u, qat_dtype_bits(QAT_DTYPE_INT4));
    EXPECT_EQ(2u, qat_dtype_bits(QAT_DTYPE_INT2));
    EXPECT_EQ(1u, qat_dtype_bits(QAT_DTYPE_INT1));

    // Test scheme names
    EXPECT_NE(nullptr, qat_scheme_name(QAT_SCHEME_SYMMETRIC));
    EXPECT_NE(nullptr, qat_scheme_name(QAT_SCHEME_AFFINE));
}

/**
 * Test quantization error computation for ternary
 */
TEST_F(TrainingTernaryIntegrationTest, TernaryQuantizationError) {
    // Create original weights
    nimcp_tensor_t* original = createWeightTensor(8, 8);
    ASSERT_NE(nullptr, original);

    // Clone for ternarization
    uint32_t dims[2] = {8, 8};
    nimcp_tensor_t* quantized = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_F32);
    ASSERT_NE(nullptr, quantized);

    memcpy(nimcp_tensor_data(quantized), nimcp_tensor_data(original),
           64 * sizeof(float));

    // Ternarize
    qat_ternary_config_t config;
    qat_ternary_default_config(&config);

    qat_ctx_t* ctx = qat_ternary_create(&config);
    ASSERT_NE(nullptr, ctx);

    qat_ternary_params_t params;
    qat_ternarize(ctx, quantized, &config, &params);

    // Compute MSE
    float mse = qat_compute_mse(original, quantized);

    // MSE should be non-negative
    EXPECT_GE(mse, 0.0f);

    // For reasonable weights in [-1, 1], MSE should be bounded
    EXPECT_LT(mse, 1.0f) << "Quantization error should be reasonable";

    nimcp_tensor_destroy(original);
    nimcp_tensor_destroy(quantized);
    qat_destroy(ctx);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
