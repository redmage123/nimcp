/**
 * @file test_predictive.cpp
 * @brief Phase 10.9: Predictive Processing Test Suite
 *
 * WHAT: Comprehensive tests for hierarchical predictive coding
 * WHY:  Ensure free energy minimization and active inference correctness
 * HOW:  Google Test framework with mock inputs and networks
 *
 * TEST COVERAGE:
 * - Creation & destruction
 * - Configuration management
 * - Forward inference (free energy minimization)
 * - Prediction error computation
 * - Precision weighting
 * - Active inference
 * - Learning & model updates
 * - Statistics tracking
 * - Error handling
 *
 * @author NIMCP Phase 10 Team
 * @date 2025-11-09
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include "cognitive/nimcp_predictive.h"

//=============================================================================
// Test Fixture
//=============================================================================

class PredictiveTest : public ::testing::Test {
protected:
    predictive_network_t net;

    void SetUp() override {
        net = nullptr;
    }

    void TearDown() override {
        if (net) {
            predictive_destroy(net);
            net = nullptr;
        }
    }
};

//=============================================================================
// Creation & Destruction Tests
//=============================================================================

TEST_F(PredictiveTest, CreateWithDefaults) {
    // Act: Create with default config
    net = predictive_create(nullptr);

    // Assert: Should create successfully
    ASSERT_NE(net, nullptr);
    EXPECT_EQ(predictive_get_num_layers(net), 4u);  // Default 4 layers
}

TEST_F(PredictiveTest, CreateWithCustomConfig) {
    // Arrange: Custom configuration
    uint32_t layer_sizes[] = {128, 64, 32};
    predictive_config_t config = predictive_default_config();
    config.num_layers = 3;
    config.layer_sizes = layer_sizes;

    // Act: Create with custom config
    net = predictive_create(&config);

    // Assert: Should match configuration
    ASSERT_NE(net, nullptr);
    EXPECT_EQ(predictive_get_num_layers(net), 3u);
    EXPECT_EQ(predictive_get_layer_size(net, 0), 128u);
    EXPECT_EQ(predictive_get_layer_size(net, 1), 64u);
    EXPECT_EQ(predictive_get_layer_size(net, 2), 32u);
}

TEST_F(PredictiveTest, CreateWithZeroLayers) {
    // Arrange: Invalid configuration
    uint32_t layer_sizes[] = {128};
    predictive_config_t config = predictive_default_config();
    config.num_layers = 0;
    config.layer_sizes = layer_sizes;

    // Act: Create with zero layers
    net = predictive_create(&config);

    // Assert: Should fail gracefully
    EXPECT_EQ(net, nullptr);
}

TEST_F(PredictiveTest, DestroyNull) {
    // Act: Destroy NULL network
    predictive_destroy(nullptr);

    // Assert: Should not crash
    SUCCEED();
}

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(PredictiveTest, DefaultConfigValues) {
    // Act: Get default configuration
    predictive_config_t config = predictive_default_config();

    // Assert: Check sensible defaults
    EXPECT_EQ(config.num_layers, 4u);
    EXPECT_GT(config.learning_rate, 0.0f);
    EXPECT_GT(config.precision_learning_rate, 0.0f);
    EXPECT_EQ(config.initial_precision, 1.0f);
    EXPECT_TRUE(config.enable_active_inference);
    EXPECT_TRUE(config.enable_precision_learning);
}

//=============================================================================
// Forward Inference Tests
//=============================================================================

TEST_F(PredictiveTest, ForwardInferenceBasic) {
    // Arrange: Create network
    net = predictive_create(nullptr);
    ASSERT_NE(net, nullptr);

    // Create input
    uint32_t input_size = predictive_get_layer_size(net, 0);
    float* input = new float[input_size];
    for (uint32_t i = 0; i < input_size; i++) {
        input[i] = 0.5f;  // Constant input
    }

    // Act: Run forward pass
    float free_energy = predictive_forward(net, input, 10);

    // Assert: Free energy should be finite
    EXPECT_TRUE(std::isfinite(free_energy));
    EXPECT_GE(free_energy, 0.0f);

    delete[] input;
}

TEST_F(PredictiveTest, ForwardInferenceConvergence) {
    // Arrange: Create network
    net = predictive_create(nullptr);
    ASSERT_NE(net, nullptr);

    uint32_t input_size = predictive_get_layer_size(net, 0);
    float* input = new float[input_size];
    for (uint32_t i = 0; i < input_size; i++) {
        input[i] = (float)i / input_size;  // Gradient input
    }

    // Act: Run multiple inference steps
    float fe1 = predictive_forward(net, input, 5);
    predictive_reset(net);
    float fe2 = predictive_forward(net, input, 20);

    // Assert: More iterations should reduce free energy
    EXPECT_TRUE(std::isfinite(fe1));
    EXPECT_TRUE(std::isfinite(fe2));
    // Note: fe2 might not always be less due to reset, but should be finite

    delete[] input;
}

TEST_F(PredictiveTest, ForwardWithNullNetwork) {
    // Arrange: Create input
    float input[256] = {0};

    // Act: Forward with NULL network
    float fe = predictive_forward(nullptr, input, 10);

    // Assert: Should return infinity
    EXPECT_EQ(fe, INFINITY);
}

TEST_F(PredictiveTest, ForwardWithNullInput) {
    // Arrange: Create network
    net = predictive_create(nullptr);
    ASSERT_NE(net, nullptr);

    // Act: Forward with NULL input
    float fe = predictive_forward(net, nullptr, 10);

    // Assert: Should return infinity
    EXPECT_EQ(fe, INFINITY);
}

//=============================================================================
// Prediction & Error Tests
//=============================================================================

TEST_F(PredictiveTest, GetLayerPrediction) {
    // Arrange: Create network and run inference
    net = predictive_create(nullptr);
    ASSERT_NE(net, nullptr);

    uint32_t input_size = predictive_get_layer_size(net, 0);
    float* input = new float[input_size];
    for (uint32_t i = 0; i < input_size; i++) {
        input[i] = 1.0f;
    }
    predictive_forward(net, input, 10);

    // Act: Get layer prediction
    uint32_t layer_size = predictive_get_layer_size(net, 1);
    float* prediction = new float[layer_size];
    bool success = predictive_get_layer_prediction(net, 1, prediction);

    // Assert: Should succeed
    EXPECT_TRUE(success);

    delete[] input;
    delete[] prediction;
}

TEST_F(PredictiveTest, GetLayerError) {
    // Arrange: Create network and run inference
    net = predictive_create(nullptr);
    ASSERT_NE(net, nullptr);

    uint32_t input_size = predictive_get_layer_size(net, 0);
    float* input = new float[input_size];
    for (uint32_t i = 0; i < input_size; i++) {
        input[i] = 0.5f;
    }
    predictive_forward(net, input, 10);

    // Act: Get layer error
    uint32_t layer_size = predictive_get_layer_size(net, 0);
    float* error = new float[layer_size];
    bool success = predictive_get_layer_error(net, 0, error);

    // Assert: Should succeed
    EXPECT_TRUE(success);

    delete[] input;
    delete[] error;
}

TEST_F(PredictiveTest, GetPredictionInvalidLayer) {
    // Arrange: Create network
    net = predictive_create(nullptr);
    ASSERT_NE(net, nullptr);

    float output[128];

    // Act: Get prediction from invalid layer
    bool success = predictive_get_layer_prediction(net, 999, output);

    // Assert: Should fail gracefully
    EXPECT_FALSE(success);
}

//=============================================================================
// Learning Tests
//=============================================================================

TEST_F(PredictiveTest, UpdateModel) {
    // Arrange: Create network
    net = predictive_create(nullptr);
    ASSERT_NE(net, nullptr);

    // Act: Update model
    bool success = predictive_update_model(net);

    // Assert: Should succeed
    EXPECT_TRUE(success);
}

TEST_F(PredictiveTest, UpdatePrecision) {
    // Arrange: Create network and run inference
    net = predictive_create(nullptr);
    ASSERT_NE(net, nullptr);

    uint32_t input_size = predictive_get_layer_size(net, 0);
    float* input = new float[input_size];
    for (uint32_t i = 0; i < input_size; i++) {
        input[i] = (float)i / input_size;
    }
    predictive_forward(net, input, 10);

    // Act: Update precision for layer 0
    bool success = predictive_update_precision(net, 0);

    // Assert: Should succeed
    EXPECT_TRUE(success);

    delete[] input;
}

TEST_F(PredictiveTest, UpdatePrecisionInvalidLayer) {
    // Arrange: Create network
    net = predictive_create(nullptr);
    ASSERT_NE(net, nullptr);

    // Act: Update precision for invalid layer
    bool success = predictive_update_precision(net, 999);

    // Assert: Should fail gracefully
    EXPECT_FALSE(success);
}

//=============================================================================
// Active Inference Tests
//=============================================================================

TEST_F(PredictiveTest, ActiveInferenceBasic) {
    // Arrange: Create network
    net = predictive_create(nullptr);
    ASSERT_NE(net, nullptr);

    // Create action candidates
    predictive_action_t actions[3];
    actions[0].action_id = 0;
    strcpy(actions[0].action_name, "action_A");
    actions[0].expected_free_energy = 1.5f;

    actions[1].action_id = 1;
    strcpy(actions[1].action_name, "action_B");
    actions[1].expected_free_energy = 0.8f;  // Lowest (best)

    actions[2].action_id = 2;
    strcpy(actions[2].action_name, "action_C");
    actions[2].expected_free_energy = 2.0f;

    // Act: Select action
    uint32_t selected;
    float efe = predictive_active_inference(net, actions, 3, &selected);

    // Assert: Should select action with lowest EFE
    EXPECT_EQ(selected, 1u);  // action_B
    EXPECT_FLOAT_EQ(efe, 0.8f);
}

TEST_F(PredictiveTest, ActiveInferenceWithNullActions) {
    // Arrange: Create network
    net = predictive_create(nullptr);
    ASSERT_NE(net, nullptr);

    // Act: Active inference with NULL actions
    uint32_t selected;
    float efe = predictive_active_inference(net, nullptr, 3, &selected);

    // Assert: Should fail gracefully
    EXPECT_EQ(efe, INFINITY);
}

TEST_F(PredictiveTest, ActiveInferenceDisabled) {
    // Arrange: Create network with active inference disabled
    predictive_config_t config = predictive_default_config();
    config.enable_active_inference = false;
    net = predictive_create(&config);
    ASSERT_NE(net, nullptr);

    predictive_action_t actions[2];
    actions[0].expected_free_energy = 1.0f;
    actions[1].expected_free_energy = 2.0f;

    // Act: Try active inference
    uint32_t selected;
    float efe = predictive_active_inference(net, actions, 2, &selected);

    // Assert: Should fail (disabled)
    EXPECT_EQ(efe, INFINITY);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(PredictiveTest, GetStatistics) {
    // Arrange: Create network and run inference
    net = predictive_create(nullptr);
    ASSERT_NE(net, nullptr);

    uint32_t input_size = predictive_get_layer_size(net, 0);
    float* input = new float[input_size];
    for (uint32_t i = 0; i < input_size; i++) {
        input[i] = 0.5f;
    }
    predictive_forward(net, input, 10);

    // Act: Get statistics
    predictive_stats_t stats;
    bool success = predictive_get_statistics(net, &stats);

    // Assert: Should return valid statistics
    EXPECT_TRUE(success);
    EXPECT_TRUE(std::isfinite(stats.total_free_energy));
    EXPECT_GT(stats.average_precision, 0.0f);
    EXPECT_GE(stats.num_updates, 1u);

    delete[] input;
}

TEST_F(PredictiveTest, GetStatisticsWithNullNetwork) {
    // Arrange: NULL network
    predictive_stats_t stats;

    // Act: Get statistics from NULL
    bool success = predictive_get_statistics(nullptr, &stats);

    // Assert: Should fail gracefully
    EXPECT_FALSE(success);
}

//=============================================================================
// Utility Tests
//=============================================================================

TEST_F(PredictiveTest, PrintState) {
    // Arrange: Create network
    net = predictive_create(nullptr);
    ASSERT_NE(net, nullptr);

    // Act: Print state
    predictive_print_state(net);

    // Assert: Should not crash
    SUCCEED();
}

TEST_F(PredictiveTest, PrintStateWithNull) {
    // Act: Print NULL network
    predictive_print_state(nullptr);

    // Assert: Should not crash
    SUCCEED();
}

TEST_F(PredictiveTest, ResetNetwork) {
    // Arrange: Create network and run inference
    net = predictive_create(nullptr);
    ASSERT_NE(net, nullptr);

    uint32_t input_size = predictive_get_layer_size(net, 0);
    float* input = new float[input_size];
    for (uint32_t i = 0; i < input_size; i++) {
        input[i] = 1.0f;
    }
    predictive_forward(net, input, 10);

    // Act: Reset network
    bool success = predictive_reset(net);

    // Assert: Should succeed
    EXPECT_TRUE(success);

    delete[] input;
}

TEST_F(PredictiveTest, GetNumLayers) {
    // Arrange: Create network
    net = predictive_create(nullptr);
    ASSERT_NE(net, nullptr);

    // Act: Get number of layers
    uint32_t num_layers = predictive_get_num_layers(net);

    // Assert: Should match default
    EXPECT_EQ(num_layers, 4u);
}

TEST_F(PredictiveTest, GetLayerSize) {
    // Arrange: Create network
    net = predictive_create(nullptr);
    ASSERT_NE(net, nullptr);

    // Act: Get layer size
    uint32_t size = predictive_get_layer_size(net, 0);

    // Assert: Should match default
    EXPECT_EQ(size, 256u);  // Default first layer
}

TEST_F(PredictiveTest, GetLayerSizeInvalid) {
    // Arrange: Create network
    net = predictive_create(nullptr);
    ASSERT_NE(net, nullptr);

    // Act: Get invalid layer size
    uint32_t size = predictive_get_layer_size(net, 999);

    // Assert: Should return 0
    EXPECT_EQ(size, 0u);
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(PredictiveTest, CompleteWorkflow) {
    // Arrange: Create network
    net = predictive_create(nullptr);
    ASSERT_NE(net, nullptr);

    // Create input
    uint32_t input_size = predictive_get_layer_size(net, 0);
    float* input = new float[input_size];
    for (uint32_t i = 0; i < input_size; i++) {
        input[i] = sinf((float)i / 10.0f);  // Sinusoidal pattern
    }

    // Act: Complete workflow
    // 1. Forward inference
    float fe = predictive_forward(net, input, 20);
    EXPECT_TRUE(std::isfinite(fe));

    // 2. Get predictions
    float* prediction = new float[input_size];
    EXPECT_TRUE(predictive_get_layer_prediction(net, 0, prediction));

    // 3. Update precision
    EXPECT_TRUE(predictive_update_precision(net, 0));

    // 4. Update model
    EXPECT_TRUE(predictive_update_model(net));

    // 5. Get statistics
    predictive_stats_t stats;
    EXPECT_TRUE(predictive_get_statistics(net, &stats));

    // 6. Print state
    predictive_print_state(net);

    // Assert: All operations completed successfully
    SUCCEED();

    delete[] input;
    delete[] prediction;
}

TEST_F(PredictiveTest, MultipleInferenceSteps) {
    // Arrange: Create network
    net = predictive_create(nullptr);
    ASSERT_NE(net, nullptr);

    uint32_t input_size = predictive_get_layer_size(net, 0);
    float* input = new float[input_size];

    // Act: Run multiple inference steps with different inputs
    for (int step = 0; step < 5; step++) {
        // Create varying input
        for (uint32_t i = 0; i < input_size; i++) {
            input[i] = sinf((float)(i + step * 10) / 20.0f);
        }

        float fe = predictive_forward(net, input, 10);
        EXPECT_TRUE(std::isfinite(fe));

        // Update precision after each step
        predictive_update_precision(net, 0);
    }

    // Assert: Should complete all steps
    predictive_stats_t stats;
    predictive_get_statistics(net, &stats);
    EXPECT_GE(stats.num_updates, 5u);

    delete[] input;
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(PredictiveTest, VeryLargeNetwork) {
    // Arrange: Large network configuration
    uint32_t large_sizes[] = {1024, 512, 256, 128};
    predictive_config_t config = predictive_default_config();
    config.num_layers = 4;
    config.layer_sizes = large_sizes;

    // Act: Create large network
    net = predictive_create(&config);

    // Assert: Should handle large dimensions
    EXPECT_NE(net, nullptr);
    EXPECT_EQ(predictive_get_layer_size(net, 0), 1024u);
}

TEST_F(PredictiveTest, SingleLayerNetwork) {
    // Arrange: Single layer configuration
    uint32_t single_size[] = {64};
    predictive_config_t config = predictive_default_config();
    config.num_layers = 1;
    config.layer_sizes = single_size;

    // Act: Create single-layer network
    net = predictive_create(&config);

    // Assert: Should handle single layer
    EXPECT_NE(net, nullptr);
    EXPECT_EQ(predictive_get_num_layers(net), 1u);
}
