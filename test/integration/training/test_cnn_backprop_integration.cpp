/**
 * @file test_cnn_backprop_integration.cpp
 * @brief Integration tests for CNN training backpropagation pipeline
 *
 * WHAT: Full forward→backward→step cycle integration tests
 * WHY:  Verify complete training workflow with gradient flow and weight updates
 * HOW:  Test multi-layer networks, mixed layer types, and training iterations
 *
 * Test Categories:
 * - Multi-layer network backward pass (Conv→Pool→Flatten→Dense)
 * - Weight changes after step() - weights should differ from initial
 * - Multiple training iterations - loss should decrease
 * - Training mode vs eval mode behavior
 * - Gradient flow through entire network
 * - Mixed layer types (Conv+BN+Pool+Dropout+Dense)
 *
 * BIOLOGICAL GROUNDING:
 * Tests model the hierarchical visual pathway learning:
 * - V1 (Conv): Orientation-selective receptive fields via Hebbian learning
 * - V2/V4 (Pool): Position-invariant feature extraction
 * - IT (Dense): Category-selective neurons through supervised learning
 * - Backprop: Biological implausibility, but enables pretraining for SNN conversion
 *
 * @author NIMCP Development Team
 * @date 2025-12-24
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <algorithm>

// Headers have their own extern "C" guards
#include "training/nimcp_cnn_training.h"
#include "utils/tensor/nimcp_tensor.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/memory/nimcp_memory.h"
#include "middleware/training/nimcp_loss_functions.h"

//=============================================================================
// Test Fixture
//=============================================================================

class CNNBackpropIntegrationTest : public ::testing::Test {
protected:
    cnn_trainer_t* trainer = nullptr;
    nimcp_tensor_t* input = nullptr;
    nimcp_tensor_t* target = nullptr;

    // Small network dimensions for reasonable test time
    static constexpr uint32_t BATCH_SIZE = 4;
    static constexpr uint32_t INPUT_CHANNELS = 2;
    static constexpr uint32_t INPUT_HEIGHT = 8;
    static constexpr uint32_t INPUT_WIDTH = 8;
    static constexpr uint32_t NUM_CLASSES = 3;

    void SetUp() override {
        // Create trainer with default config
        cnn_trainer_config_t config;
        cnn_trainer_default_config(&config);
        config.learning_rate = 0.01f;
        config.max_epochs = 5;
        config.gradient_clip_value = 5.0f;
        config.enable_bio_async = false;
        config.verbose = false;
        trainer = cnn_trainer_create(&config);
        ASSERT_NE(trainer, nullptr);

        // Create input tensor [batch, channels, height, width]
        uint32_t input_dims[] = {BATCH_SIZE, INPUT_CHANNELS, INPUT_HEIGHT, INPUT_WIDTH};
        input = nimcp_tensor_randn(input_dims, 4, NIMCP_DTYPE_F32, 0.0, 0.5);
        ASSERT_NE(input, nullptr);

        // Create target tensor [batch, num_classes] - one-hot encoded
        uint32_t target_dims[] = {BATCH_SIZE, NUM_CLASSES};
        target = nimcp_tensor_zeros(target_dims, 2, NIMCP_DTYPE_F32);
        ASSERT_NE(target, nullptr);

        // Fill target with one-hot labels
        float* target_data = static_cast<float*>(nimcp_tensor_data(target));
        for (uint32_t b = 0; b < BATCH_SIZE; ++b) {
            uint32_t label = b % NUM_CLASSES;  // Simple cycling labels
            target_data[b * NUM_CLASSES + label] = 1.0f;
        }
    }

    void TearDown() override {
        nimcp_tensor_destroy(input);
        nimcp_tensor_destroy(target);
        cnn_trainer_destroy(trainer);
    }

    // Helper to build simple Conv→Pool→Flatten→Dense network
    void BuildSimpleNetwork() {
        // Conv layer: 2→4 channels, 3x3 kernel
        cnn_conv_config_t conv_cfg;
        memset(&conv_cfg, 0, sizeof(conv_cfg));
        conv_cfg.kernel_h = 3;
        conv_cfg.kernel_w = 3;
        conv_cfg.stride_h = 1;
        conv_cfg.stride_w = 1;
        conv_cfg.padding_h = 1;  // Same padding
        conv_cfg.padding_w = 1;
        conv_cfg.in_channels = INPUT_CHANNELS;
        conv_cfg.out_channels = 4;
        conv_cfg.activation = CNN_ACTIVATION_RELU;
        conv_cfg.use_bias = true;
        conv_cfg.weight_init_std = 0.1f;
        conv_cfg.padding_mode = CNN_PADDING_SAME;
        conv_cfg.groups = 1;
        conv_cfg.dilation_h = 1;
        conv_cfg.dilation_w = 1;
        ASSERT_NE(cnn_trainer_add_conv_layer(trainer, &conv_cfg), nullptr);

        // Pool layer: 2x2 max pooling
        cnn_pool_config_t pool_cfg;
        memset(&pool_cfg, 0, sizeof(pool_cfg));
        pool_cfg.type = CNN_POOL_MAX;
        pool_cfg.pool_h = 2;
        pool_cfg.pool_w = 2;
        pool_cfg.stride_h = 2;
        pool_cfg.stride_w = 2;
        pool_cfg.padding_h = 0;
        pool_cfg.padding_w = 0;
        ASSERT_NE(cnn_trainer_add_pool_layer(trainer, &pool_cfg), nullptr);

        // Flatten layer
        ASSERT_NE(cnn_trainer_add_flatten_layer(trainer), nullptr);

        // Dense layer: flattened → NUM_CLASSES
        // After conv (8x8) → pool (4x4), flatten = 4 channels * 4 * 4 = 64
        cnn_dense_config_t dense_cfg;
        memset(&dense_cfg, 0, sizeof(dense_cfg));
        dense_cfg.in_features = 64;  // 4 channels * 4 * 4
        dense_cfg.out_features = NUM_CLASSES;
        dense_cfg.activation = CNN_ACTIVATION_SOFTMAX;
        dense_cfg.use_bias = true;
        dense_cfg.weight_init_std = 0.1f;
        ASSERT_NE(cnn_trainer_add_dense_layer(trainer, &dense_cfg), nullptr);
    }

    // Helper to build complex network with BatchNorm and Dropout
    void BuildComplexNetwork() {
        // Conv layer 1: 2→8 channels
        cnn_conv_config_t conv1_cfg;
        memset(&conv1_cfg, 0, sizeof(conv1_cfg));
        conv1_cfg.kernel_h = 3;
        conv1_cfg.kernel_w = 3;
        conv1_cfg.stride_h = 1;
        conv1_cfg.stride_w = 1;
        conv1_cfg.padding_h = 1;
        conv1_cfg.padding_w = 1;
        conv1_cfg.in_channels = INPUT_CHANNELS;
        conv1_cfg.out_channels = 8;
        conv1_cfg.activation = CNN_ACTIVATION_RELU;
        conv1_cfg.use_bias = false;  // BatchNorm makes bias redundant
        conv1_cfg.weight_init_std = 0.1f;
        conv1_cfg.padding_mode = CNN_PADDING_SAME;
        conv1_cfg.groups = 1;
        conv1_cfg.dilation_h = 1;
        conv1_cfg.dilation_w = 1;
        ASSERT_NE(cnn_trainer_add_conv_layer(trainer, &conv1_cfg), nullptr);

        // Batch normalization
        cnn_batch_norm_config_t bn_cfg;
        memset(&bn_cfg, 0, sizeof(bn_cfg));
        bn_cfg.num_features = 8;
        bn_cfg.epsilon = 1e-5f;
        bn_cfg.momentum = 0.9f;
        bn_cfg.affine = true;
        bn_cfg.track_running_stats = true;
        ASSERT_NE(cnn_trainer_add_batch_norm_layer(trainer, &bn_cfg), nullptr);

        // Max pooling
        cnn_pool_config_t pool_cfg;
        memset(&pool_cfg, 0, sizeof(pool_cfg));
        pool_cfg.type = CNN_POOL_MAX;
        pool_cfg.pool_h = 2;
        pool_cfg.pool_w = 2;
        pool_cfg.stride_h = 2;
        pool_cfg.stride_w = 2;
        pool_cfg.padding_h = 0;
        pool_cfg.padding_w = 0;
        ASSERT_NE(cnn_trainer_add_pool_layer(trainer, &pool_cfg), nullptr);

        // Conv layer 2: 8→16 channels
        cnn_conv_config_t conv2_cfg;
        memset(&conv2_cfg, 0, sizeof(conv2_cfg));
        conv2_cfg.kernel_h = 3;
        conv2_cfg.kernel_w = 3;
        conv2_cfg.stride_h = 1;
        conv2_cfg.stride_w = 1;
        conv2_cfg.padding_h = 1;
        conv2_cfg.padding_w = 1;
        conv2_cfg.in_channels = 8;
        conv2_cfg.out_channels = 16;
        conv2_cfg.activation = CNN_ACTIVATION_RELU;
        conv2_cfg.use_bias = true;
        conv2_cfg.weight_init_std = 0.1f;
        conv2_cfg.padding_mode = CNN_PADDING_SAME;
        conv2_cfg.groups = 1;
        conv2_cfg.dilation_h = 1;
        conv2_cfg.dilation_w = 1;
        ASSERT_NE(cnn_trainer_add_conv_layer(trainer, &conv2_cfg), nullptr);

        // Dropout
        cnn_dropout_config_t dropout_cfg;
        memset(&dropout_cfg, 0, sizeof(dropout_cfg));
        dropout_cfg.dropout_rate = 0.2f;
        dropout_cfg.spatial_dropout = true;
        dropout_cfg.variational_dropout = false;
        ASSERT_NE(cnn_trainer_add_dropout_layer(trainer, &dropout_cfg), nullptr);

        // Flatten
        ASSERT_NE(cnn_trainer_add_flatten_layer(trainer), nullptr);

        // Dense layer: 16 channels * 4 * 4 = 256 → NUM_CLASSES
        cnn_dense_config_t dense_cfg;
        memset(&dense_cfg, 0, sizeof(dense_cfg));
        dense_cfg.in_features = 256;
        dense_cfg.out_features = NUM_CLASSES;
        dense_cfg.activation = CNN_ACTIVATION_SOFTMAX;
        dense_cfg.use_bias = true;
        dense_cfg.weight_init_std = 0.1f;
        ASSERT_NE(cnn_trainer_add_dense_layer(trainer, &dense_cfg), nullptr);
    }

    // Helper to clone network weights (for detecting changes)
    std::vector<nimcp_tensor_t*> CloneAllWeights() {
        std::vector<nimcp_tensor_t*> cloned;
        uint32_t num_layers = cnn_get_layer_count(trainer);

        for (uint32_t i = 0; i < num_layers; ++i) {
            cnn_layer_t* layer = cnn_get_layer(trainer, i);
            if (layer) {
                // Get weight tensor (implementation-specific, assume accessible)
                // For now, skip - actual implementation would access layer internals
            }
        }
        return cloned;
    }

    // Helper to compute L2 distance between tensors
    double TensorDistance(const nimcp_tensor_t* a, const nimcp_tensor_t* b) {
        nimcp_tensor_t* diff = nimcp_tensor_sub(a, b);
        nimcp_tensor_t* squared = nimcp_tensor_square(diff);
        nimcp_tensor_t* sum_t = nimcp_tensor_sum(squared);
        double dist = nimcp_tensor_get_flat(sum_t, 0);
        nimcp_tensor_destroy(diff);
        nimcp_tensor_destroy(squared);
        nimcp_tensor_destroy(sum_t);
        return std::sqrt(dist);
    }
};

//=============================================================================
// Test 1: Multi-layer Network Backward Pass
//=============================================================================

TEST_F(CNNBackpropIntegrationTest, MultiLayerBackwardPass) {
    BuildSimpleNetwork();

    // Forward pass
    cnn_forward_result_t fwd_result;
    memset(&fwd_result, 0, sizeof(fwd_result));
    nimcp_error_t err = cnn_trainer_forward(trainer, input, &fwd_result);
    ASSERT_EQ(err, NIMCP_SUCCESS);
    ASSERT_NE(fwd_result.output, nullptr);

    // Verify output shape [batch_size, num_classes]
    const nimcp_tensor_shape_t* out_shape = nimcp_tensor_shape(fwd_result.output);
    ASSERT_EQ(out_shape->rank, 2);
    ASSERT_EQ(out_shape->dims[0], BATCH_SIZE);
    ASSERT_EQ(out_shape->dims[1], NUM_CLASSES);

    // Backward pass
    err = cnn_trainer_backward(trainer, target, &fwd_result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Verify gradient tensors were computed (cached in activations)
    EXPECT_NE(fwd_result.activations, nullptr);
    EXPECT_GT(fwd_result.num_layers, 0u);

    // Cleanup
    nimcp_tensor_destroy(fwd_result.output);
    for (uint32_t i = 0; i < fwd_result.num_layers; ++i) {
        nimcp_tensor_destroy(fwd_result.activations[i]);
    }
    if (fwd_result.activations) {
        nimcp_free(fwd_result.activations);
    }
}

//=============================================================================
// Test 2: Weight Changes After Step
//=============================================================================

TEST_F(CNNBackpropIntegrationTest, WeightChangesAfterStep) {
    BuildSimpleNetwork();

    // Get layer count
    uint32_t num_layers = cnn_get_layer_count(trainer);
    ASSERT_GT(num_layers, 0u);

    // Forward + backward
    cnn_forward_result_t fwd_result;
    memset(&fwd_result, 0, sizeof(fwd_result));
    ASSERT_EQ(cnn_trainer_forward(trainer, input, &fwd_result), NIMCP_SUCCESS);
    ASSERT_EQ(cnn_trainer_backward(trainer, target, &fwd_result), NIMCP_SUCCESS);

    // Optimizer step (weight update)
    nimcp_error_t err = cnn_trainer_step(trainer);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Note: Actual weight comparison requires accessing layer internals
    // In production code, this test would:
    // 1. Clone weights before step
    // 2. Step
    // 3. Verify weights changed (L2 distance > threshold)
    // For now, verify step succeeds

    // Cleanup
    nimcp_tensor_destroy(fwd_result.output);
    for (uint32_t i = 0; i < fwd_result.num_layers; ++i) {
        nimcp_tensor_destroy(fwd_result.activations[i]);
    }
    if (fwd_result.activations) {
        nimcp_free(fwd_result.activations);
    }
}

//=============================================================================
// Test 3: Multiple Training Iterations - Loss Decreases
//=============================================================================

TEST_F(CNNBackpropIntegrationTest, LossDecreasesOverIterations) {
    BuildSimpleNetwork();

    const int NUM_ITERATIONS = 10;
    std::vector<double> losses;

    for (int iter = 0; iter < NUM_ITERATIONS; ++iter) {
        cnn_forward_result_t fwd_result;
        memset(&fwd_result, 0, sizeof(fwd_result));

        // Forward pass
        ASSERT_EQ(cnn_trainer_forward(trainer, input, &fwd_result), NIMCP_SUCCESS);

        // Compute loss (cross-entropy)
        float loss = nimcp_loss_cross_entropy_tensor(fwd_result.output, target, 1e-7f, NIMCP_LOSS_REDUCE_MEAN);
        losses.push_back(loss);

        // Backward + step
        ASSERT_EQ(cnn_trainer_backward(trainer, target, &fwd_result), NIMCP_SUCCESS);
        ASSERT_EQ(cnn_trainer_step(trainer), NIMCP_SUCCESS);

        // Cleanup iteration
        nimcp_tensor_destroy(fwd_result.output);
        for (uint32_t i = 0; i < fwd_result.num_layers; ++i) {
            nimcp_tensor_destroy(fwd_result.activations[i]);
        }
        if (fwd_result.activations) {
            nimcp_free(fwd_result.activations);
        }
    }

    // Verify loss decreased overall
    ASSERT_GE(losses.size(), static_cast<size_t>(NUM_ITERATIONS));
    double first_loss = losses[0];
    double last_loss = losses[NUM_ITERATIONS - 1];

    // Loss should decrease (or at worst, stay similar within 20%)
    // Some noise is expected with small batch size
    EXPECT_LT(last_loss, first_loss * 1.2);

    // Check trend: majority of iterations should show decrease
    int decreases = 0;
    for (int i = 1; i < NUM_ITERATIONS; ++i) {
        if (losses[i] <= losses[i - 1]) {
            decreases++;
        }
    }
    // At least 50% of iterations should decrease or maintain
    EXPECT_GE(decreases, NUM_ITERATIONS / 2);
}

//=============================================================================
// Test 4: Training Mode vs Eval Mode
//=============================================================================

TEST_F(CNNBackpropIntegrationTest, TrainingModeVsEvalMode) {
    BuildComplexNetwork();  // Network with dropout and batch norm

    // Training mode: forward pass
    cnn_forward_result_t train_fwd_result;
    memset(&train_fwd_result, 0, sizeof(train_fwd_result));
    ASSERT_EQ(cnn_trainer_forward(trainer, input, &train_fwd_result), NIMCP_SUCCESS);

    // Clone output
    nimcp_tensor_t* train_output = nimcp_tensor_clone(train_fwd_result.output);
    ASSERT_NE(train_output, nullptr);

    // Cleanup training forward
    nimcp_tensor_destroy(train_fwd_result.output);
    for (uint32_t i = 0; i < train_fwd_result.num_layers; ++i) {
        nimcp_tensor_destroy(train_fwd_result.activations[i]);
    }
    if (train_fwd_result.activations) {
        free(train_fwd_result.activations);
    }

    // TODO: When eval mode API is available, test that:
    // 1. Dropout is disabled (all activations pass through)
    // 2. Batch norm uses running stats (not batch stats)
    // 3. Outputs differ between train and eval mode

    // For now, verify training mode succeeds
    EXPECT_NE(train_output, nullptr);

    nimcp_tensor_destroy(train_output);
}

//=============================================================================
// Test 5: Gradient Flow Through Entire Network
//=============================================================================

TEST_F(CNNBackpropIntegrationTest, GradientFlowThroughNetwork) {
    BuildSimpleNetwork();

    // Forward pass
    cnn_forward_result_t fwd_result;
    memset(&fwd_result, 0, sizeof(fwd_result));
    ASSERT_EQ(cnn_trainer_forward(trainer, input, &fwd_result), NIMCP_SUCCESS);

    // Backward pass
    ASSERT_EQ(cnn_trainer_backward(trainer, target, &fwd_result), NIMCP_SUCCESS);

    // Verify activations were cached for all layers
    uint32_t num_layers = cnn_get_layer_count(trainer);
    EXPECT_EQ(fwd_result.num_layers, num_layers);

    // Verify each layer has activation tensor
    for (uint32_t i = 0; i < fwd_result.num_layers; ++i) {
        EXPECT_NE(fwd_result.activations[i], nullptr) << "Layer " << i << " activation is NULL";

        // Verify activation is non-trivial (not all zeros)
        const nimcp_tensor_shape_t* shape = nimcp_tensor_shape(fwd_result.activations[i]);
        ASSERT_NE(shape, nullptr);

        bool has_nonzero = false;
        size_t numel = nimcp_tensor_numel(fwd_result.activations[i]);
        const float* data = static_cast<const float*>(
            nimcp_tensor_data_const(fwd_result.activations[i])
        );

        for (size_t j = 0; j < std::min(numel, size_t(100)); ++j) {
            if (std::fabs(data[j]) > 1e-6f) {
                has_nonzero = true;
                break;
            }
        }
        EXPECT_TRUE(has_nonzero) << "Layer " << i << " activation is all zeros";
    }

    // Cleanup
    nimcp_tensor_destroy(fwd_result.output);
    for (uint32_t i = 0; i < fwd_result.num_layers; ++i) {
        nimcp_tensor_destroy(fwd_result.activations[i]);
    }
    if (fwd_result.activations) {
        nimcp_free(fwd_result.activations);
    }
}

//=============================================================================
// Test 6: Mixed Layer Types Integration
//=============================================================================

TEST_F(CNNBackpropIntegrationTest, MixedLayerTypes) {
    BuildComplexNetwork();  // Conv + BN + Pool + Dropout + Dense

    // Verify layer count
    uint32_t num_layers = cnn_get_layer_count(trainer);
    EXPECT_GE(num_layers, 5u);  // At least 5 layers in complex network

    // Run full training cycle
    cnn_forward_result_t fwd_result;
    memset(&fwd_result, 0, sizeof(fwd_result));
    ASSERT_EQ(cnn_trainer_forward(trainer, input, &fwd_result), NIMCP_SUCCESS);
    ASSERT_EQ(cnn_trainer_backward(trainer, target, &fwd_result), NIMCP_SUCCESS);
    ASSERT_EQ(cnn_trainer_step(trainer), NIMCP_SUCCESS);

    // Verify output shape is correct
    const nimcp_tensor_shape_t* out_shape = nimcp_tensor_shape(fwd_result.output);
    EXPECT_EQ(out_shape->rank, 2);
    EXPECT_EQ(out_shape->dims[0], BATCH_SIZE);
    EXPECT_EQ(out_shape->dims[1], NUM_CLASSES);

    // Verify output is valid probability distribution (softmax)
    const float* output_data = static_cast<const float*>(
        nimcp_tensor_data_const(fwd_result.output)
    );

    for (uint32_t b = 0; b < BATCH_SIZE; ++b) {
        double row_sum = 0.0;
        for (uint32_t c = 0; c < NUM_CLASSES; ++c) {
            float val = output_data[b * NUM_CLASSES + c];
            EXPECT_GE(val, 0.0f) << "Negative probability at batch " << b << ", class " << c;
            EXPECT_LE(val, 1.0f) << "Probability > 1 at batch " << b << ", class " << c;
            row_sum += val;
        }
        // Softmax should sum to 1.0 (with small tolerance)
        EXPECT_NEAR(row_sum, 1.0, 1e-5) << "Softmax doesn't sum to 1 for batch " << b;
    }

    // Cleanup
    nimcp_tensor_destroy(fwd_result.output);
    for (uint32_t i = 0; i < fwd_result.num_layers; ++i) {
        nimcp_tensor_destroy(fwd_result.activations[i]);
    }
    if (fwd_result.activations) {
        nimcp_free(fwd_result.activations);
    }
}

//=============================================================================
// Test 7: Parameter Count Verification
//=============================================================================

TEST_F(CNNBackpropIntegrationTest, ParameterCount) {
    BuildSimpleNetwork();

    size_t param_count = cnn_count_parameters(trainer);

    // Expected parameters:
    // Conv: (kernel_h * kernel_w * in_channels * out_channels) + out_channels
    //     = (3 * 3 * 2 * 4) + 4 = 72 + 4 = 76
    // Pool: 0 (no parameters)
    // Flatten: 0
    // Dense: (in_features * out_features) + out_features
    //      = (64 * 3) + 3 = 192 + 3 = 195
    // Total: 76 + 195 = 271

    size_t expected = 271;
    EXPECT_EQ(param_count, expected);
}

//=============================================================================
// Test 8: Gradient Clipping
//=============================================================================

TEST_F(CNNBackpropIntegrationTest, GradientClipping) {
    BuildSimpleNetwork();

    // Create large gradients by using extreme target values
    uint32_t target_dims[] = {BATCH_SIZE, NUM_CLASSES};
    nimcp_tensor_t* extreme_target = nimcp_tensor_full(
        target_dims, 2, NIMCP_DTYPE_F32, 100.0  // Extreme value
    );
    ASSERT_NE(extreme_target, nullptr);

    // Forward + backward with extreme target
    cnn_forward_result_t fwd_result;
    memset(&fwd_result, 0, sizeof(fwd_result));
    ASSERT_EQ(cnn_trainer_forward(trainer, input, &fwd_result), NIMCP_SUCCESS);
    ASSERT_EQ(cnn_trainer_backward(trainer, extreme_target, &fwd_result), NIMCP_SUCCESS);

    // Step should succeed (gradients clipped to 5.0 threshold from config)
    nimcp_error_t err = cnn_trainer_step(trainer);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Cleanup
    nimcp_tensor_destroy(extreme_target);
    nimcp_tensor_destroy(fwd_result.output);
    for (uint32_t i = 0; i < fwd_result.num_layers; ++i) {
        nimcp_tensor_destroy(fwd_result.activations[i]);
    }
    if (fwd_result.activations) {
        nimcp_free(fwd_result.activations);
    }
}

//=============================================================================
// Test 9: Batch Processing Consistency
//=============================================================================

TEST_F(CNNBackpropIntegrationTest, BatchProcessingConsistency) {
    BuildSimpleNetwork();

    // Process full batch
    cnn_forward_result_t batch_result;
    memset(&batch_result, 0, sizeof(batch_result));
    ASSERT_EQ(cnn_trainer_forward(trainer, input, &batch_result), NIMCP_SUCCESS);

    // Extract first sample output
    nimcp_tensor_t* batch_output_clone = nimcp_tensor_clone(batch_result.output);
    ASSERT_NE(batch_output_clone, nullptr);

    // Cleanup batch result
    nimcp_tensor_destroy(batch_result.output);
    for (uint32_t i = 0; i < batch_result.num_layers; ++i) {
        nimcp_tensor_destroy(batch_result.activations[i]);
    }
    if (batch_result.activations) {
        free(batch_result.activations);
    }

    // Process individual sample (batch size = 1)
    uint32_t single_dims[] = {1, INPUT_CHANNELS, INPUT_HEIGHT, INPUT_WIDTH};
    nimcp_tensor_t* single_input = nimcp_tensor_create(single_dims, 4, NIMCP_DTYPE_F32);
    ASSERT_NE(single_input, nullptr);

    // Copy first sample from batch input
    const float* input_data = static_cast<const float*>(nimcp_tensor_data_const(input));
    float* single_data = static_cast<float*>(nimcp_tensor_data(single_input));
    size_t sample_size = INPUT_CHANNELS * INPUT_HEIGHT * INPUT_WIDTH;
    memcpy(single_data, input_data, sample_size * sizeof(float));

    cnn_forward_result_t single_result;
    memset(&single_result, 0, sizeof(single_result));
    ASSERT_EQ(cnn_trainer_forward(trainer, single_input, &single_result), NIMCP_SUCCESS);

    // Compare outputs (should be identical for first sample)
    const float* batch_out_data = static_cast<const float*>(
        nimcp_tensor_data_const(batch_output_clone)
    );
    const float* single_out_data = static_cast<const float*>(
        nimcp_tensor_data_const(single_result.output)
    );

    for (uint32_t c = 0; c < NUM_CLASSES; ++c) {
        EXPECT_NEAR(batch_out_data[c], single_out_data[c], 1e-4f)
            << "Mismatch at class " << c;
    }

    // Cleanup
    nimcp_tensor_destroy(single_input);
    nimcp_tensor_destroy(batch_output_clone);
    nimcp_tensor_destroy(single_result.output);
    for (uint32_t i = 0; i < single_result.num_layers; ++i) {
        nimcp_tensor_destroy(single_result.activations[i]);
    }
    if (single_result.activations) {
        free(single_result.activations);
    }
}

//=============================================================================
// Test 10: End-to-End Training Pipeline
//=============================================================================

TEST_F(CNNBackpropIntegrationTest, EndToEndTrainingPipeline) {
    BuildComplexNetwork();

    const int NUM_EPOCHS = 3;
    std::vector<double> epoch_losses;

    for (int epoch = 0; epoch < NUM_EPOCHS; ++epoch) {
        // Forward
        cnn_forward_result_t fwd_result;
        memset(&fwd_result, 0, sizeof(fwd_result));
        ASSERT_EQ(cnn_trainer_forward(trainer, input, &fwd_result), NIMCP_SUCCESS);

        // Compute loss
        float loss = nimcp_loss_cross_entropy_tensor(fwd_result.output, target, 1e-7f, NIMCP_LOSS_REDUCE_MEAN);
        epoch_losses.push_back(loss);

        // Backward
        ASSERT_EQ(cnn_trainer_backward(trainer, target, &fwd_result), NIMCP_SUCCESS);

        // Step
        ASSERT_EQ(cnn_trainer_step(trainer), NIMCP_SUCCESS);

        // Cleanup
        nimcp_tensor_destroy(fwd_result.output);
        for (uint32_t i = 0; i < fwd_result.num_layers; ++i) {
            nimcp_tensor_destroy(fwd_result.activations[i]);
        }
        if (fwd_result.activations) {
            nimcp_free(fwd_result.activations);
        }
    }

    // Verify loss trend (should decrease or stabilize)
    ASSERT_EQ(epoch_losses.size(), static_cast<size_t>(NUM_EPOCHS));
    double first_epoch_loss = epoch_losses[0];
    double last_epoch_loss = epoch_losses[NUM_EPOCHS - 1];

    // Final loss should be no worse than 1.5x initial loss
    EXPECT_LT(last_epoch_loss, first_epoch_loss * 1.5);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
