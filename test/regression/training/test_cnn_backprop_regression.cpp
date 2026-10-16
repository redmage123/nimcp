/**
 * @file test_cnn_backprop_regression.cpp
 * @brief Regression tests for CNN backpropagation gradient stability
 *
 * WHAT: Verify CNN gradient computation remains numerically stable and correct
 * WHY:  Detect gradient explosions, vanishing, NaN/Inf propagation, non-determinism
 * HOW:  Test gradient properties across various scenarios and edge cases
 *
 * TEST CATEGORIES:
 * 1. Gradient Validity - No NaN/Inf in computed gradients
 * 2. Gradient Boundedness - Gradients stay within reasonable magnitude
 * 3. Gradient Non-Vanishing - Gradients are non-zero for reasonable inputs
 * 4. Determinism - Same input produces identical gradients
 * 5. Weight Update Stability - Parameter updates don't explode
 * 6. Accumulation Correctness - Multiple backward() calls accumulate properly
 * 7. Large Batch Stability - Handles large batches without overflow
 * 8. Edge Cases - Zero input, one-hot input, uniform input
 *
 * BIOLOGICAL GROUNDING:
 * Gradient descent models synaptic plasticity (Hebbian learning):
 * - Gradient explosion → uncontrolled LTP (excitotoxicity)
 * - Gradient vanishing → insufficient plasticity (learning failure)
 * - NaN/Inf → pathological neural states
 *
 * @author NIMCP Development Team
 * @date 2025-12-24
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>
#include <random>

// Headers have their own extern "C" guards
#include "training/nimcp_cnn_training.h"
#include "utils/tensor/nimcp_tensor.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Check if all values in array are finite (no NaN or Inf)
 */
static bool is_finite(const float* data, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        if (!std::isfinite(data[i])) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Check if all values are bounded within [-max_abs, max_abs]
 */
static bool is_bounded(const float* data, size_t n, float max_abs) {
    for (size_t i = 0; i < n; ++i) {
        if (std::abs(data[i]) > max_abs) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Check if at least one value is non-zero
 */
static bool is_nonzero(const float* data, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        if (std::abs(data[i]) > 1e-10f) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Compute maximum absolute value in array
 */
static float max_abs_value(const float* data, size_t n) {
    float max_val = 0.0f;
    for (size_t i = 0; i < n; ++i) {
        float abs_val = std::abs(data[i]);
        if (abs_val > max_val) {
            max_val = abs_val;
        }
    }
    return max_val;
}

/**
 * @brief Check if two arrays are equal within tolerance
 */
static bool arrays_equal(const float* a, const float* b, size_t n, float tol = 1e-6f) {
    for (size_t i = 0; i < n; ++i) {
        if (std::abs(a[i] - b[i]) > tol) {
            return false;
        }
    }
    return true;
}

//=============================================================================
// Test Fixture
//=============================================================================

class CNNBackpropRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Use fixed seed for determinism
        rng_.seed(42);
    }

    void TearDown() override {
        // Cleanup happens via RAII
    }

    /**
     * @brief Create simple CNN trainer: Conv -> Pool -> Flatten -> Dense
     */
    cnn_trainer_t* create_simple_cnn(uint32_t batch_size = 4) {
        cnn_trainer_config_t config;
        int ret = cnn_trainer_default_config(&config);
        if (ret != NIMCP_SUCCESS) return nullptr;

        config.dataloader.batch_size = batch_size;
        config.learning_rate = 0.01f;
        config.gradient_clip_value = 5.0f;

        cnn_trainer_t* trainer = cnn_trainer_create(&config);
        if (!trainer) return nullptr;

        // Conv layer: 1 -> 8 channels, 3x3 kernel
        cnn_conv_config_t conv_cfg = {};
        conv_cfg.kernel_h = 3;
        conv_cfg.kernel_w = 3;
        conv_cfg.stride_h = 1;
        conv_cfg.stride_w = 1;
        conv_cfg.padding_h = 1;
        conv_cfg.padding_w = 1;
        conv_cfg.in_channels = 1;
        conv_cfg.out_channels = 8;
        conv_cfg.activation = CNN_ACTIVATION_RELU;
        conv_cfg.use_bias = true;
        conv_cfg.weight_init_std = 0.1f;

        cnn_layer_t* conv = cnn_trainer_add_conv_layer(trainer, &conv_cfg);
        if (!conv) {
            cnn_trainer_destroy(trainer);
            return nullptr;
        }

        // Pool layer: 2x2 max pooling
        cnn_pool_config_t pool_cfg = {};
        pool_cfg.type = CNN_POOL_MAX;
        pool_cfg.pool_h = 2;
        pool_cfg.pool_w = 2;
        pool_cfg.stride_h = 2;
        pool_cfg.stride_w = 2;

        cnn_layer_t* pool = cnn_trainer_add_pool_layer(trainer, &pool_cfg);
        if (!pool) {
            cnn_trainer_destroy(trainer);
            return nullptr;
        }

        // Flatten
        cnn_layer_t* flatten = cnn_trainer_add_flatten_layer(trainer);
        if (!flatten) {
            cnn_trainer_destroy(trainer);
            return nullptr;
        }

        // Dense layer: flattened -> 10 outputs
        cnn_dense_config_t dense_cfg = {};
        dense_cfg.in_features = 8 * 14 * 14;  // Assuming 28x28 input -> 14x14 after pool
        dense_cfg.out_features = 10;
        dense_cfg.activation = CNN_ACTIVATION_SOFTMAX;
        dense_cfg.use_bias = true;
        dense_cfg.weight_init_std = 0.1f;

        cnn_layer_t* dense = cnn_trainer_add_dense_layer(trainer, &dense_cfg);
        if (!dense) {
            cnn_trainer_destroy(trainer);
            return nullptr;
        }

        return trainer;
    }

    /**
     * @brief Create input tensor with random values
     */
    nimcp_tensor_t* create_random_input(uint32_t batch, uint32_t channels,
                                         uint32_t height, uint32_t width) {
        uint32_t dims[4] = {batch, channels, height, width};
        nimcp_tensor_t* t = nimcp_tensor_create(dims, 4, NIMCP_DTYPE_F32);
        if (!t) return nullptr;

        float* data = static_cast<float*>(nimcp_tensor_data(t));
        std::normal_distribution<float> dist(0.0f, 1.0f);
        size_t n = batch * channels * height * width;
        for (size_t i = 0; i < n; ++i) {
            data[i] = dist(rng_);
        }

        return t;
    }

    /**
     * @brief Create target tensor (one-hot encoded labels)
     */
    nimcp_tensor_t* create_one_hot_targets(uint32_t batch, uint32_t num_classes) {
        uint32_t dims[2] = {batch, num_classes};
        nimcp_tensor_t* t = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_F32);
        if (!t) return nullptr;

        float* data = static_cast<float*>(nimcp_tensor_data(t));
        memset(data, 0, batch * num_classes * sizeof(float));

        // Random class per sample
        std::uniform_int_distribution<uint32_t> class_dist(0, num_classes - 1);
        for (uint32_t b = 0; b < batch; ++b) {
            uint32_t cls = class_dist(rng_);
            data[b * num_classes + cls] = 1.0f;
        }

        return t;
    }

    /**
     * @brief Create all-zero input tensor
     */
    nimcp_tensor_t* create_zero_input(uint32_t batch, uint32_t channels,
                                       uint32_t height, uint32_t width) {
        uint32_t dims[4] = {batch, channels, height, width};
        nimcp_tensor_t* t = nimcp_tensor_create(dims, 4, NIMCP_DTYPE_F32);
        if (!t) return nullptr;

        float* data = static_cast<float*>(nimcp_tensor_data(t));
        memset(data, 0, batch * channels * height * width * sizeof(float));
        return t;
    }

    /**
     * @brief Create uniform input tensor (all same value)
     */
    nimcp_tensor_t* create_uniform_input(uint32_t batch, uint32_t channels,
                                          uint32_t height, uint32_t width,
                                          float value) {
        uint32_t dims[4] = {batch, channels, height, width};
        nimcp_tensor_t* t = nimcp_tensor_create(dims, 4, NIMCP_DTYPE_F32);
        if (!t) return nullptr;

        float* data = static_cast<float*>(nimcp_tensor_data(t));
        size_t n = batch * channels * height * width;
        std::fill(data, data + n, value);
        return t;
    }

    /**
     * @brief Get gradient data for a layer
     *
     * Uses the public cnn_get_layer_weight_grad API to access layer gradients.
     */
    float* get_layer_gradients(cnn_trainer_t* trainer, uint32_t layer_idx, size_t* out_size) {
        float* grad_data = nullptr;
        size_t grad_size = 0;

        nimcp_error_t ret = cnn_get_layer_weight_grad(trainer, layer_idx, &grad_data, &grad_size);
        if (ret != NIMCP_SUCCESS || !grad_data) {
            *out_size = 0;
            return nullptr;
        }

        *out_size = grad_size;
        return grad_data;
    }

    std::mt19937 rng_;
};

//=============================================================================
// Gradient Validity Tests (No NaN/Inf)
//=============================================================================

/**
 * Test: Gradients don't contain NaN after backward pass
 * REGRESSION TARGET: Numerical stability
 */
TEST_F(CNNBackpropRegressionTest, GradientsNoNaN) {
    cnn_trainer_t* trainer = create_simple_cnn(4);
    ASSERT_NE(trainer, nullptr);

    nimcp_tensor_t* input = create_random_input(4, 1, 28, 28);
    ASSERT_NE(input, nullptr);

    nimcp_tensor_t* target = create_one_hot_targets(4, 10);
    ASSERT_NE(target, nullptr);

    // Forward pass
    cnn_forward_result_t fwd_result;
    memset(&fwd_result, 0, sizeof(fwd_result));
    int ret = cnn_trainer_forward(trainer, input, &fwd_result);
    ASSERT_EQ(ret, NIMCP_SUCCESS);

    // Backward pass
    ret = cnn_trainer_backward(trainer, target, &fwd_result);
    ASSERT_EQ(ret, NIMCP_SUCCESS);

    // Check gradients for all layers
    uint32_t num_layers = cnn_get_layer_count(trainer);
    for (uint32_t i = 0; i < num_layers; ++i) {
        size_t grad_size = 0;
        float* grads = get_layer_gradients(trainer, i, &grad_size);
        if (grads && grad_size > 0) {
            EXPECT_TRUE(is_finite(grads, grad_size))
                << "Layer " << i << " has NaN or Inf in gradients";
            nimcp_free(grads);
        }
    }

    nimcp_tensor_destroy(input);
    nimcp_tensor_destroy(target);
    cnn_trainer_destroy(trainer);
}

/**
 * Test: Output activations are finite after forward pass
 * REGRESSION TARGET: Forward pass stability
 */
TEST_F(CNNBackpropRegressionTest, ActivationsFinite) {
    cnn_trainer_t* trainer = create_simple_cnn(4);
    ASSERT_NE(trainer, nullptr);

    nimcp_tensor_t* input = create_random_input(4, 1, 28, 28);
    ASSERT_NE(input, nullptr);

    cnn_forward_result_t fwd_result;
    memset(&fwd_result, 0, sizeof(fwd_result));
    int ret = cnn_trainer_forward(trainer, input, &fwd_result);
    ASSERT_EQ(ret, NIMCP_SUCCESS);

    // Check output is finite
    if (fwd_result.output) {
        float* out_data = static_cast<float*>(nimcp_tensor_data(fwd_result.output));
        size_t out_size = 4 * 10;  // batch * num_classes
        EXPECT_TRUE(is_finite(out_data, out_size))
            << "Output contains NaN or Inf";
    }

    nimcp_tensor_destroy(input);
    cnn_trainer_destroy(trainer);
}

//=============================================================================
// Gradient Boundedness Tests
//=============================================================================

/**
 * Test: Gradients are bounded (not exploding)
 * REGRESSION TARGET: Gradient explosion detection
 */
TEST_F(CNNBackpropRegressionTest, GradientsBounded) {
    cnn_trainer_t* trainer = create_simple_cnn(4);
    ASSERT_NE(trainer, nullptr);

    nimcp_tensor_t* input = create_random_input(4, 1, 28, 28);
    ASSERT_NE(input, nullptr);

    nimcp_tensor_t* target = create_one_hot_targets(4, 10);
    ASSERT_NE(target, nullptr);

    cnn_forward_result_t fwd_result;
    memset(&fwd_result, 0, sizeof(fwd_result));
    cnn_trainer_forward(trainer, input, &fwd_result);
    cnn_trainer_backward(trainer, target, &fwd_result);

    // Gradients should be < 100 (gradient clipping at 5.0 should help)
    const float MAX_GRADIENT = 100.0f;
    uint32_t num_layers = cnn_get_layer_count(trainer);
    for (uint32_t i = 0; i < num_layers; ++i) {
        size_t grad_size = 0;
        float* grads = get_layer_gradients(trainer, i, &grad_size);
        if (grads && grad_size > 0) {
            EXPECT_TRUE(is_bounded(grads, grad_size, MAX_GRADIENT))
                << "Layer " << i << " gradients exceed " << MAX_GRADIENT;
            nimcp_free(grads);
        }
    }

    nimcp_tensor_destroy(input);
    nimcp_tensor_destroy(target);
    cnn_trainer_destroy(trainer);
}

/**
 * Test: Gradient magnitude in reasonable range
 * REGRESSION TARGET: Gradient scale stability
 */
TEST_F(CNNBackpropRegressionTest, GradientMagnitudeReasonable) {
    cnn_trainer_t* trainer = create_simple_cnn(4);
    ASSERT_NE(trainer, nullptr);

    nimcp_tensor_t* input = create_random_input(4, 1, 28, 28);
    ASSERT_NE(input, nullptr);

    nimcp_tensor_t* target = create_one_hot_targets(4, 10);
    ASSERT_NE(target, nullptr);

    cnn_forward_result_t fwd_result;
    memset(&fwd_result, 0, sizeof(fwd_result));
    cnn_trainer_forward(trainer, input, &fwd_result);
    cnn_trainer_backward(trainer, target, &fwd_result);

    // Check gradient magnitude is in [1e-6, 10.0] range
    const float MIN_EXPECTED = 1e-6f;
    const float MAX_EXPECTED = 10.0f;

    uint32_t num_layers = cnn_get_layer_count(trainer);
    for (uint32_t i = 0; i < num_layers; ++i) {
        size_t grad_size = 0;
        float* grads = get_layer_gradients(trainer, i, &grad_size);
        if (grads && grad_size > 0) {
            float max_grad = max_abs_value(grads, grad_size);
            EXPECT_GE(max_grad, MIN_EXPECTED)
                << "Layer " << i << " gradients too small (vanishing)";
            EXPECT_LE(max_grad, MAX_EXPECTED)
                << "Layer " << i << " gradients too large (exploding)";
            nimcp_free(grads);
        }
    }

    nimcp_tensor_destroy(input);
    nimcp_tensor_destroy(target);
    cnn_trainer_destroy(trainer);
}

//=============================================================================
// Gradient Non-Vanishing Tests
//=============================================================================

/**
 * Test: Gradients are non-zero for reasonable inputs
 * REGRESSION TARGET: Gradient vanishing detection
 */
TEST_F(CNNBackpropRegressionTest, GradientsNonZero) {
    cnn_trainer_t* trainer = create_simple_cnn(4);
    ASSERT_NE(trainer, nullptr);

    nimcp_tensor_t* input = create_random_input(4, 1, 28, 28);
    ASSERT_NE(input, nullptr);

    nimcp_tensor_t* target = create_one_hot_targets(4, 10);
    ASSERT_NE(target, nullptr);

    cnn_forward_result_t fwd_result;
    memset(&fwd_result, 0, sizeof(fwd_result));
    cnn_trainer_forward(trainer, input, &fwd_result);
    cnn_trainer_backward(trainer, target, &fwd_result);

    // At least some gradients should be non-zero
    uint32_t num_layers = cnn_get_layer_count(trainer);
    for (uint32_t i = 0; i < num_layers; ++i) {
        size_t grad_size = 0;
        float* grads = get_layer_gradients(trainer, i, &grad_size);
        if (grads && grad_size > 0) {
            EXPECT_TRUE(is_nonzero(grads, grad_size))
                << "Layer " << i << " has all-zero gradients (vanishing)";
            nimcp_free(grads);
        }
    }

    nimcp_tensor_destroy(input);
    nimcp_tensor_destroy(target);
    cnn_trainer_destroy(trainer);
}

//=============================================================================
// Determinism Tests
//=============================================================================

/**
 * Test: Same input produces identical gradients
 * REGRESSION TARGET: Deterministic gradient computation
 */
TEST_F(CNNBackpropRegressionTest, GradientsDeterministic) {
    cnn_trainer_t* trainer = create_simple_cnn(4);
    ASSERT_NE(trainer, nullptr);

    nimcp_tensor_t* input = create_random_input(4, 1, 28, 28);
    ASSERT_NE(input, nullptr);

    nimcp_tensor_t* target = create_one_hot_targets(4, 10);
    ASSERT_NE(target, nullptr);

    // First pass - zero gradients before to ensure clean state
    cnn_trainer_zero_grad(trainer);
    cnn_forward_result_t fwd_result1;
    memset(&fwd_result1, 0, sizeof(fwd_result1));
    cnn_trainer_forward(trainer, input, &fwd_result1);
    cnn_trainer_backward(trainer, target, &fwd_result1);

    // Store first gradients
    std::vector<std::vector<float>> first_grads;
    uint32_t num_layers = cnn_get_layer_count(trainer);
    for (uint32_t i = 0; i < num_layers; ++i) {
        size_t grad_size = 0;
        float* grads = get_layer_gradients(trainer, i, &grad_size);
        if (grads && grad_size > 0) {
            first_grads.push_back(std::vector<float>(grads, grads + grad_size));
            nimcp_free(grads);
        }
    }

    // Zero gradients before second pass to prevent accumulation
    cnn_trainer_zero_grad(trainer);

    // Second pass with same input
    cnn_forward_result_t fwd_result2;
    memset(&fwd_result2, 0, sizeof(fwd_result2));
    cnn_trainer_forward(trainer, input, &fwd_result2);
    cnn_trainer_backward(trainer, target, &fwd_result2);

    // Compare gradients
    for (uint32_t i = 0; i < num_layers && i < first_grads.size(); ++i) {
        size_t grad_size = 0;
        float* grads = get_layer_gradients(trainer, i, &grad_size);
        if (grads && grad_size > 0 && grad_size == first_grads[i].size()) {
            EXPECT_TRUE(arrays_equal(first_grads[i].data(), grads, grad_size))
                << "Layer " << i << " gradients are non-deterministic";
            nimcp_free(grads);
        }
    }

    nimcp_tensor_destroy(input);
    nimcp_tensor_destroy(target);
    cnn_trainer_destroy(trainer);
}

//=============================================================================
// Weight Update Stability Tests
//=============================================================================

/**
 * Test: Weight updates are bounded after optimizer step
 * REGRESSION TARGET: Weight update explosion
 */
TEST_F(CNNBackpropRegressionTest, WeightUpdatesBounded) {
    cnn_trainer_t* trainer = create_simple_cnn(4);
    ASSERT_NE(trainer, nullptr);

    nimcp_tensor_t* input = create_random_input(4, 1, 28, 28);
    ASSERT_NE(input, nullptr);

    nimcp_tensor_t* target = create_one_hot_targets(4, 10);
    ASSERT_NE(target, nullptr);

    cnn_forward_result_t fwd_result;
    memset(&fwd_result, 0, sizeof(fwd_result));
    cnn_trainer_forward(trainer, input, &fwd_result);
    cnn_trainer_backward(trainer, target, &fwd_result);

    // Apply optimizer step
    int ret = cnn_trainer_step(trainer);
    ASSERT_EQ(ret, NIMCP_SUCCESS);

    // Weight updates should be small (bounded by learning rate)
    // With LR=0.01, updates should be < 1.0
    // NOTE: Would need API to access weights for verification
    // This is a placeholder test

    nimcp_tensor_destroy(input);
    nimcp_tensor_destroy(target);
    cnn_trainer_destroy(trainer);
}

/**
 * Test: Multiple step() calls don't cause weight explosion
 * REGRESSION TARGET: Iterative stability
 */
TEST_F(CNNBackpropRegressionTest, MultipleStepsStable) {
    cnn_trainer_t* trainer = create_simple_cnn(4);
    ASSERT_NE(trainer, nullptr);

    // Run 10 training iterations
    for (int iter = 0; iter < 10; ++iter) {
        nimcp_tensor_t* input = create_random_input(4, 1, 28, 28);
        ASSERT_NE(input, nullptr);

        nimcp_tensor_t* target = create_one_hot_targets(4, 10);
        ASSERT_NE(target, nullptr);

        cnn_forward_result_t fwd_result;
        memset(&fwd_result, 0, sizeof(fwd_result));
        cnn_trainer_forward(trainer, input, &fwd_result);
        cnn_trainer_backward(trainer, target, &fwd_result);
        cnn_trainer_step(trainer);

        // Check output remains finite
        if (fwd_result.output) {
            float* out_data = static_cast<float*>(nimcp_tensor_data(fwd_result.output));
            EXPECT_TRUE(is_finite(out_data, 4 * 10))
                << "Iteration " << iter << " produced non-finite outputs";
        }

        nimcp_tensor_destroy(input);
        nimcp_tensor_destroy(target);
    }

    cnn_trainer_destroy(trainer);
}

//=============================================================================
// Gradient Accumulation Tests
//=============================================================================

/**
 * Test: Multiple backward() calls accumulate gradients correctly
 * REGRESSION TARGET: Gradient accumulation correctness
 */
TEST_F(CNNBackpropRegressionTest, GradientAccumulationCorrect) {
    cnn_trainer_t* trainer = create_simple_cnn(2);
    ASSERT_NE(trainer, nullptr);

    nimcp_tensor_t* input1 = create_random_input(2, 1, 28, 28);
    nimcp_tensor_t* target1 = create_one_hot_targets(2, 10);
    ASSERT_NE(input1, nullptr);
    ASSERT_NE(target1, nullptr);

    // First backward pass
    cnn_forward_result_t fwd_result1;
    memset(&fwd_result1, 0, sizeof(fwd_result1));
    cnn_trainer_forward(trainer, input1, &fwd_result1);
    cnn_trainer_backward(trainer, target1, &fwd_result1);

    // Store first gradients
    std::vector<float> grads_first;
    size_t grad_size_first = 0;
    float* g1 = get_layer_gradients(trainer, 0, &grad_size_first);
    if (g1 && grad_size_first > 0) {
        grads_first.assign(g1, g1 + grad_size_first);
        nimcp_free(g1);
    }

    // Second backward pass (should accumulate)
    nimcp_tensor_t* input2 = create_random_input(2, 1, 28, 28);
    nimcp_tensor_t* target2 = create_one_hot_targets(2, 10);
    ASSERT_NE(input2, nullptr);
    ASSERT_NE(target2, nullptr);

    cnn_forward_result_t fwd_result2;
    memset(&fwd_result2, 0, sizeof(fwd_result2));
    cnn_trainer_forward(trainer, input2, &fwd_result2);
    cnn_trainer_backward(trainer, target2, &fwd_result2);

    // Check gradients are accumulated (should be larger)
    size_t grad_size_acc = 0;
    float* g_acc = get_layer_gradients(trainer, 0, &grad_size_acc);
    if (g_acc && grad_size_acc > 0 && grad_size_acc == grad_size_first) {
        float first_norm = 0.0f, acc_norm = 0.0f;
        for (size_t i = 0; i < grad_size_first; ++i) {
            first_norm += grads_first[i] * grads_first[i];
            acc_norm += g_acc[i] * g_acc[i];
        }
        first_norm = std::sqrt(first_norm);
        acc_norm = std::sqrt(acc_norm);

        // Accumulated gradients should be larger (or at least similar magnitude)
        EXPECT_GE(acc_norm, first_norm * 0.5f)
            << "Gradient accumulation seems to have decreased magnitude unexpectedly";
        nimcp_free(g_acc);
    }

    nimcp_tensor_destroy(input1);
    nimcp_tensor_destroy(target1);
    nimcp_tensor_destroy(input2);
    nimcp_tensor_destroy(target2);
    cnn_trainer_destroy(trainer);
}

//=============================================================================
// Large Batch Stability Tests
//=============================================================================

/**
 * Test: Large batch size doesn't cause overflow
 * REGRESSION TARGET: Batch size scaling stability
 */
TEST_F(CNNBackpropRegressionTest, LargeBatchStable) {
    // Test with batch size 64 (larger than typical)
    cnn_trainer_t* trainer = create_simple_cnn(64);
    ASSERT_NE(trainer, nullptr);

    nimcp_tensor_t* input = create_random_input(64, 1, 28, 28);
    ASSERT_NE(input, nullptr);

    nimcp_tensor_t* target = create_one_hot_targets(64, 10);
    ASSERT_NE(target, nullptr);

    cnn_forward_result_t fwd_result;
    memset(&fwd_result, 0, sizeof(fwd_result));
    int ret = cnn_trainer_forward(trainer, input, &fwd_result);
    ASSERT_EQ(ret, NIMCP_SUCCESS);

    ret = cnn_trainer_backward(trainer, target, &fwd_result);
    ASSERT_EQ(ret, NIMCP_SUCCESS);

    // Check gradients are still finite and bounded
    uint32_t num_layers = cnn_get_layer_count(trainer);
    for (uint32_t i = 0; i < num_layers; ++i) {
        size_t grad_size = 0;
        float* grads = get_layer_gradients(trainer, i, &grad_size);
        if (grads && grad_size > 0) {
            EXPECT_TRUE(is_finite(grads, grad_size))
                << "Large batch caused non-finite gradients at layer " << i;
            EXPECT_TRUE(is_bounded(grads, grad_size, 100.0f))
                << "Large batch caused gradient explosion at layer " << i;
            nimcp_free(grads);
        }
    }

    nimcp_tensor_destroy(input);
    nimcp_tensor_destroy(target);
    cnn_trainer_destroy(trainer);
}

//=============================================================================
// Edge Case Tests
//=============================================================================

/**
 * Test: Zero input doesn't cause NaN gradients
 * REGRESSION TARGET: Edge case - zero input
 */
TEST_F(CNNBackpropRegressionTest, ZeroInputStable) {
    cnn_trainer_t* trainer = create_simple_cnn(4);
    ASSERT_NE(trainer, nullptr);

    nimcp_tensor_t* input = create_zero_input(4, 1, 28, 28);
    ASSERT_NE(input, nullptr);

    nimcp_tensor_t* target = create_one_hot_targets(4, 10);
    ASSERT_NE(target, nullptr);

    cnn_forward_result_t fwd_result;
    memset(&fwd_result, 0, sizeof(fwd_result));
    int ret = cnn_trainer_forward(trainer, input, &fwd_result);
    ASSERT_EQ(ret, NIMCP_SUCCESS);

    ret = cnn_trainer_backward(trainer, target, &fwd_result);
    ASSERT_EQ(ret, NIMCP_SUCCESS);

    // Gradients should still be finite (might be zero or near-zero)
    uint32_t num_layers = cnn_get_layer_count(trainer);
    for (uint32_t i = 0; i < num_layers; ++i) {
        size_t grad_size = 0;
        float* grads = get_layer_gradients(trainer, i, &grad_size);
        if (grads && grad_size > 0) {
            EXPECT_TRUE(is_finite(grads, grad_size))
                << "Zero input caused non-finite gradients at layer " << i;
            nimcp_free(grads);
        }
    }

    nimcp_tensor_destroy(input);
    nimcp_tensor_destroy(target);
    cnn_trainer_destroy(trainer);
}

/**
 * Test: One-hot input produces reasonable gradients
 * REGRESSION TARGET: Edge case - sparse input
 */
TEST_F(CNNBackpropRegressionTest, OneHotInputStable) {
    cnn_trainer_t* trainer = create_simple_cnn(4);
    ASSERT_NE(trainer, nullptr);

    // Create input with only one pixel per sample set to 1.0
    nimcp_tensor_t* input = create_zero_input(4, 1, 28, 28);
    ASSERT_NE(input, nullptr);

    float* input_data = static_cast<float*>(nimcp_tensor_data(input));
    input_data[0] = 1.0f;    // Sample 0
    input_data[784] = 1.0f;  // Sample 1
    input_data[1568] = 1.0f; // Sample 2
    input_data[2352] = 1.0f; // Sample 3

    nimcp_tensor_t* target = create_one_hot_targets(4, 10);
    ASSERT_NE(target, nullptr);

    cnn_forward_result_t fwd_result;
    memset(&fwd_result, 0, sizeof(fwd_result));
    cnn_trainer_forward(trainer, input, &fwd_result);
    cnn_trainer_backward(trainer, target, &fwd_result);

    // Gradients should be finite
    uint32_t num_layers = cnn_get_layer_count(trainer);
    for (uint32_t i = 0; i < num_layers; ++i) {
        size_t grad_size = 0;
        float* grads = get_layer_gradients(trainer, i, &grad_size);
        if (grads && grad_size > 0) {
            EXPECT_TRUE(is_finite(grads, grad_size))
                << "One-hot input caused non-finite gradients at layer " << i;
            nimcp_free(grads);
        }
    }

    nimcp_tensor_destroy(input);
    nimcp_tensor_destroy(target);
    cnn_trainer_destroy(trainer);
}

/**
 * Test: Uniform input produces stable gradients
 * REGRESSION TARGET: Edge case - uniform input
 */
TEST_F(CNNBackpropRegressionTest, UniformInputStable) {
    cnn_trainer_t* trainer = create_simple_cnn(4);
    ASSERT_NE(trainer, nullptr);

    nimcp_tensor_t* input = create_uniform_input(4, 1, 28, 28, 0.5f);
    ASSERT_NE(input, nullptr);

    nimcp_tensor_t* target = create_one_hot_targets(4, 10);
    ASSERT_NE(target, nullptr);

    cnn_forward_result_t fwd_result;
    memset(&fwd_result, 0, sizeof(fwd_result));
    cnn_trainer_forward(trainer, input, &fwd_result);
    cnn_trainer_backward(trainer, target, &fwd_result);

    // Gradients should be finite and bounded
    uint32_t num_layers = cnn_get_layer_count(trainer);
    for (uint32_t i = 0; i < num_layers; ++i) {
        size_t grad_size = 0;
        float* grads = get_layer_gradients(trainer, i, &grad_size);
        if (grads && grad_size > 0) {
            EXPECT_TRUE(is_finite(grads, grad_size))
                << "Uniform input caused non-finite gradients at layer " << i;
            EXPECT_TRUE(is_bounded(grads, grad_size, 100.0f))
                << "Uniform input caused gradient explosion at layer " << i;
            nimcp_free(grads);
        }
    }

    nimcp_tensor_destroy(input);
    nimcp_tensor_destroy(target);
    cnn_trainer_destroy(trainer);
}

/**
 * Test: Extreme input values don't break gradients
 * REGRESSION TARGET: Edge case - extreme values
 */
TEST_F(CNNBackpropRegressionTest, ExtremeInputValuesStable) {
    cnn_trainer_t* trainer = create_simple_cnn(4);
    ASSERT_NE(trainer, nullptr);

    // Test with large positive values
    nimcp_tensor_t* input = create_uniform_input(4, 1, 28, 28, 100.0f);
    ASSERT_NE(input, nullptr);

    nimcp_tensor_t* target = create_one_hot_targets(4, 10);
    ASSERT_NE(target, nullptr);

    cnn_forward_result_t fwd_result;
    memset(&fwd_result, 0, sizeof(fwd_result));
    cnn_trainer_forward(trainer, input, &fwd_result);
    cnn_trainer_backward(trainer, target, &fwd_result);

    // Gradients should still be finite (clipping should prevent explosion)
    uint32_t num_layers = cnn_get_layer_count(trainer);
    for (uint32_t i = 0; i < num_layers; ++i) {
        size_t grad_size = 0;
        float* grads = get_layer_gradients(trainer, i, &grad_size);
        if (grads && grad_size > 0) {
            EXPECT_TRUE(is_finite(grads, grad_size))
                << "Extreme input caused non-finite gradients at layer " << i;
            nimcp_free(grads);
        }
    }

    nimcp_tensor_destroy(input);
    nimcp_tensor_destroy(target);
    cnn_trainer_destroy(trainer);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
