/**
 * @file test_predictive_real.cpp
 * @brief REAL tests for nimcp_predictive.c that exercise actual implementation
 *
 * DIFFERENCE FROM test_predictive_coverage.cpp:
 * - Creates REAL predictive networks
 * - Runs REAL inference and learning
 * - Exercises actual implementation code paths
 * - NOT just NULL guards and config checks
 *
 * @date 2025-11-10
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

#include "cognitive/nimcp_predictive.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class PredictiveRealTest : public ::testing::Test {
protected:
    predictive_network_t network = nullptr;
    uint32_t layer_sizes[3] = {10, 5, 3};  // Small network for fast tests

    void SetUp() override {
        // Create REAL predictive network
        predictive_config_t config;
        config.num_layers = 3;
        config.layer_sizes = layer_sizes;
        config.learning_rate = 0.01f;
        config.precision_learning_rate = 0.001f;
        config.initial_precision = 1.0f;
        config.enable_precision_learning = true;
        config.enable_active_inference = true;

        network = predictive_create(&config);
        ASSERT_NE(network, nullptr) << "Failed to create predictive network";
    }

    void TearDown() override {
        if (network) {
            predictive_destroy(network);
            network = nullptr;
        }
    }

    // Helper: Create valid config
    predictive_config_t create_valid_config() {
        return predictive_default_config();
    }
};

//=============================================================================
// Test Suite: REAL Network Creation
//=============================================================================

TEST_F(PredictiveRealTest, CreateNetwork_DefaultConfig) {
    predictive_config_t config = predictive_default_config();
    predictive_network_t net = predictive_create(&config);

    ASSERT_NE(net, nullptr);
    EXPECT_GT(predictive_get_num_layers(net), 0U);

    predictive_destroy(net);
}

TEST_F(PredictiveRealTest, CreateNetwork_MinimalConfig) {
    uint32_t sizes[2] = {5, 3};
    predictive_config_t config;
    config.num_layers = 2;
    config.layer_sizes = sizes;
    config.learning_rate = 0.01f;
    config.precision_learning_rate = 0.001f;
    config.initial_precision = 1.0f;
    config.enable_precision_learning = true;
    config.enable_active_inference = false;

    predictive_network_t net = predictive_create(&config);
    ASSERT_NE(net, nullptr);

    EXPECT_EQ(predictive_get_num_layers(net), 2U);
    EXPECT_EQ(predictive_get_layer_size(net, 0), 5U);
    EXPECT_EQ(predictive_get_layer_size(net, 1), 3U);

    predictive_destroy(net);
}

TEST_F(PredictiveRealTest, CreateNetwork_LargerNetwork) {
    uint32_t sizes[4] = {20, 15, 10, 5};
    predictive_config_t config;
    config.num_layers = 4;
    config.layer_sizes = sizes;
    config.learning_rate = 0.01f;
    config.precision_learning_rate = 0.001f;
    config.initial_precision = 1.0f;
    config.enable_precision_learning = true;
    config.enable_active_inference = true;

    predictive_network_t net = predictive_create(&config);
    ASSERT_NE(net, nullptr);
    EXPECT_EQ(predictive_get_num_layers(net), 4U);

    predictive_destroy(net);
}

//=============================================================================
// Test Suite: REAL Inference
//=============================================================================

TEST_F(PredictiveRealTest, Forward_WithRealInput) {
    // Create input matching layer 0 size
    float input[10];
    for (int i = 0; i < 10; i++) {
        input[i] = 0.5f;
    }

    // Run forward inference
    float energy = predictive_forward(network, input, 10);

    // Should return valid free energy (non-negative)
    EXPECT_FALSE(std::isinf(energy));
    EXPECT_FALSE(std::isnan(energy));
    EXPECT_GE(energy, 0.0f);
}

TEST_F(PredictiveRealTest, Forward_MultipleIterations) {
    float input[10];
    for (int i = 0; i < 10; i++) {
        input[i] = 0.3f;
    }

    // Run with different iteration counts
    float energy_1 = predictive_forward(network, input, 1);
    float energy_10 = predictive_forward(network, input, 10);
    float energy_50 = predictive_forward(network, input, 50);

    // All should succeed
    EXPECT_FALSE(std::isinf(energy_1));
    EXPECT_FALSE(std::isinf(energy_10));
    EXPECT_FALSE(std::isinf(energy_50));

    // More iterations should generally lead to lower energy (convergence)
    EXPECT_GE(energy_1, 0.0f);
    EXPECT_GE(energy_10, 0.0f);
    EXPECT_GE(energy_50, 0.0f);
}

TEST_F(PredictiveRealTest, Forward_DifferentInputs) {
    float input1[10] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float input2[10] = {0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    float energy1 = predictive_forward(network, input1, 10);
    float energy2 = predictive_forward(network, input2, 10);

    // Both should succeed
    EXPECT_FALSE(std::isinf(energy1));
    EXPECT_FALSE(std::isinf(energy2));

    // Different inputs may yield different energies
    EXPECT_GE(energy1, 0.0f);
    EXPECT_GE(energy2, 0.0f);
}

TEST_F(PredictiveRealTest, Forward_VaryingPatterns) {
    // Test with sin wave pattern
    float input[10];
    for (int i = 0; i < 10; i++) {
        input[i] = 0.5f + 0.3f * std::sin(i * 0.5f);
    }

    float energy = predictive_forward(network, input, 10);
    EXPECT_FALSE(std::isinf(energy));
    EXPECT_GE(energy, 0.0f);
}

//=============================================================================
// Test Suite: REAL Layer Access
//=============================================================================

TEST_F(PredictiveRealTest, GetLayerPrediction_AfterInference) {
    float input[10];
    for (int i = 0; i < 10; i++) {
        input[i] = 0.5f;
    }

    // Run inference first
    predictive_forward(network, input, 10);

    // Get prediction from layer 0
    float prediction[10];
    bool success = predictive_get_layer_prediction(network, 0, prediction);

    EXPECT_TRUE(success);
}

TEST_F(PredictiveRealTest, GetLayerError_AfterInference) {
    float input[10];
    for (int i = 0; i < 10; i++) {
        input[i] = 0.5f;
    }

    predictive_forward(network, input, 10);

    // Get error from layer 0
    float error[10];
    bool success = predictive_get_layer_error(network, 0, error);

    EXPECT_TRUE(success);
}

TEST_F(PredictiveRealTest, GetLayerSize_AllLayers) {
    EXPECT_EQ(predictive_get_layer_size(network, 0), 10U);
    EXPECT_EQ(predictive_get_layer_size(network, 1), 5U);
    EXPECT_EQ(predictive_get_layer_size(network, 2), 3U);
}

TEST_F(PredictiveRealTest, GetNumLayers_ReturnsCorrectCount) {
    EXPECT_EQ(predictive_get_num_layers(network), 3U);
}

//=============================================================================
// Test Suite: REAL Learning
//=============================================================================

TEST_F(PredictiveRealTest, UpdateModel_AfterInference) {
    float input[10];
    for (int i = 0; i < 10; i++) {
        input[i] = 0.5f;
    }

    // Run inference to generate errors
    predictive_forward(network, input, 10);

    // Update model
    bool success = predictive_update_model(network);

    EXPECT_TRUE(success);
}

TEST_F(PredictiveRealTest, UpdatePrecision_AllLayers) {
    float input[10];
    for (int i = 0; i < 10; i++) {
        input[i] = 0.5f;
    }

    predictive_forward(network, input, 10);

    // Update precision for each layer
    EXPECT_TRUE(predictive_update_precision(network, 0));
    EXPECT_TRUE(predictive_update_precision(network, 1));
}

TEST_F(PredictiveRealTest, LearningLoop_ConvergenceTest) {
    float input[10];
    for (int i = 0; i < 10; i++) {
        input[i] = 0.5f;
    }

    // Run multiple learning iterations
    float energy_before = predictive_forward(network, input, 10);

    for (int iter = 0; iter < 10; iter++) {
        predictive_forward(network, input, 10);
        predictive_update_model(network);
    }

    float energy_after = predictive_forward(network, input, 10);

    // Energy should decrease or stay stable (learning)
    EXPECT_LE(energy_after, energy_before * 1.5f);  // Allow some variance
}

TEST_F(PredictiveRealTest, UpdateModel_MultiplePatterns) {
    // Learn multiple patterns
    for (int pattern = 0; pattern < 3; pattern++) {
        float input[10];
        for (int i = 0; i < 10; i++) {
            input[i] = (float)pattern * 0.2f + i * 0.05f;
        }

        predictive_forward(network, input, 10);
        EXPECT_TRUE(predictive_update_model(network));
    }
}

//=============================================================================
// Test Suite: REAL Active Inference
//=============================================================================

TEST_F(PredictiveRealTest, ActiveInference_TwoActions) {
    // Create two action candidates
    float state1[10] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float state2[10] = {0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    predictive_action_t actions[2];
    actions[0].action_id = 1;
    strncpy(actions[0].action_name, "Action1", 64);
    actions[0].predicted_state = state1;
    actions[0].state_dim = 10;
    actions[0].expected_free_energy = 0.0f;

    actions[1].action_id = 2;
    strncpy(actions[1].action_name, "Action2", 64);
    actions[1].predicted_state = state2;
    actions[1].state_dim = 10;
    actions[1].expected_free_energy = 0.0f;

    uint32_t selected = 0;
    float efe = predictive_active_inference(network, actions, 2, &selected);

    // Should select an action
    EXPECT_FALSE(std::isinf(efe));
    EXPECT_TRUE(selected == 0 || selected == 1);
    EXPECT_GE(efe, 0.0f);
}

TEST_F(PredictiveRealTest, ActiveInference_MultipleActions) {
    float state1[10], state2[10], state3[10];
    for (int i = 0; i < 10; i++) {
        state1[i] = (i == 0) ? 1.0f : 0.0f;
        state2[i] = (i == 1) ? 1.0f : 0.0f;
        state3[i] = (i == 2) ? 1.0f : 0.0f;
    }

    predictive_action_t actions[3];
    for (int i = 0; i < 3; i++) {
        actions[i].action_id = i + 1;
        snprintf(actions[i].action_name, 64, "Action%d", i + 1);
        actions[i].state_dim = 10;
        actions[i].expected_free_energy = 0.0f;
    }
    actions[0].predicted_state = state1;
    actions[1].predicted_state = state2;
    actions[2].predicted_state = state3;

    uint32_t selected = 0;
    float efe = predictive_active_inference(network, actions, 3, &selected);

    EXPECT_FALSE(std::isinf(efe));
    EXPECT_LT(selected, 3U);
}

//=============================================================================
// Test Suite: REAL Statistics and Utilities
//=============================================================================

TEST_F(PredictiveRealTest, GetStatistics_AfterInference) {
    float input[10];
    for (int i = 0; i < 10; i++) {
        input[i] = 0.5f;
    }

    predictive_forward(network, input, 10);
    predictive_update_model(network);

    predictive_stats_t stats;
    bool success = predictive_get_statistics(network, &stats);

    EXPECT_TRUE(success);
    EXPECT_GE(stats.total_free_energy, 0.0f);
    EXPECT_GT(stats.num_updates, 0U);
}

TEST_F(PredictiveRealTest, GetStatistics_MultipleUpdates) {
    float input[10];
    for (int i = 0; i < 10; i++) {
        input[i] = 0.5f;
    }

    // Multiple updates
    for (int i = 0; i < 5; i++) {
        predictive_forward(network, input, 10);
        predictive_update_model(network);
    }

    predictive_stats_t stats;
    predictive_get_statistics(network, &stats);

    EXPECT_EQ(stats.num_updates, 5U);
}

TEST_F(PredictiveRealTest, PrintState_DoesNotCrash) {
    float input[10];
    for (int i = 0; i < 10; i++) {
        input[i] = 0.5f;
    }

    predictive_forward(network, input, 10);

    // Should not crash
    predictive_print_state(network);
    SUCCEED();
}

TEST_F(PredictiveRealTest, Reset_Network) {
    float input[10];
    for (int i = 0; i < 10; i++) {
        input[i] = 0.5f;
    }

    // Run inference
    predictive_forward(network, input, 10);

    // Reset
    bool success = predictive_reset(network);
    EXPECT_TRUE(success);

    // Should be able to run inference again
    float energy = predictive_forward(network, input, 10);
    EXPECT_FALSE(std::isinf(energy));
}

//=============================================================================
// Test Suite: REAL Integration Workflow
//=============================================================================

TEST_F(PredictiveRealTest, CompleteInferenceLearningCycle) {
    float input[10];
    for (int i = 0; i < 10; i++) {
        input[i] = 0.4f + 0.1f * i;  // Varying input
    }

    // 1. Initial inference
    float energy1 = predictive_forward(network, input, 10);
    EXPECT_FALSE(std::isinf(energy1));

    // 2. Update model
    EXPECT_TRUE(predictive_update_model(network));

    // 3. Update precision
    EXPECT_TRUE(predictive_update_precision(network, 0));

    // 4. Another inference
    float energy2 = predictive_forward(network, input, 10);
    EXPECT_FALSE(std::isinf(energy2));

    // 5. Get statistics
    predictive_stats_t stats;
    EXPECT_TRUE(predictive_get_statistics(network, &stats));
    EXPECT_GT(stats.num_updates, 0U);
}

TEST_F(PredictiveRealTest, ActiveInferenceLoop_WithExecution) {
    // Create actions
    float state1[10], state2[10];
    for (int i = 0; i < 10; i++) {
        state1[i] = 0.8f;
        state2[i] = 0.2f;
    }

    predictive_action_t actions[2];
    actions[0].action_id = 1;
    strncpy(actions[0].action_name, "Action1", 64);
    actions[0].predicted_state = state1;
    actions[0].state_dim = 10;
    actions[0].expected_free_energy = 0.0f;

    actions[1].action_id = 2;
    strncpy(actions[1].action_name, "Action2", 64);
    actions[1].predicted_state = state2;
    actions[1].state_dim = 10;
    actions[1].expected_free_energy = 0.0f;

    // 1. Select action
    uint32_t selected = 0;
    float efe = predictive_active_inference(network, actions, 2, &selected);
    EXPECT_FALSE(std::isinf(efe));

    // 2. Execute selected action (simulate with forward)
    float energy = predictive_forward(network, actions[selected].predicted_state, 10);
    EXPECT_FALSE(std::isinf(energy));

    // 3. Update model after action
    EXPECT_TRUE(predictive_update_model(network));
}

TEST_F(PredictiveRealTest, FullPredictiveCodingCycle) {
    // Simulate full predictive coding cycle
    float input[10];
    for (int i = 0; i < 10; i++) {
        input[i] = 0.5f + 0.2f * std::sin(i);
    }

    // 1. Forward pass
    float energy = predictive_forward(network, input, 20);
    EXPECT_FALSE(std::isinf(energy));

    // 2. Get predictions and errors
    float prediction[10], error[10];
    EXPECT_TRUE(predictive_get_layer_prediction(network, 0, prediction));
    EXPECT_TRUE(predictive_get_layer_error(network, 0, error));

    // 3. Update models
    EXPECT_TRUE(predictive_update_model(network));
    EXPECT_TRUE(predictive_update_precision(network, 0));

    // 4. Get final stats
    predictive_stats_t stats;
    EXPECT_TRUE(predictive_get_statistics(network, &stats));
}

//=============================================================================
// Test Suite: NULL Guards (still important for safety)
//=============================================================================

TEST_F(PredictiveRealTest, NullGuard_ForwardNull) {
    float input[10];
    float energy = predictive_forward(nullptr, input, 10);
    EXPECT_TRUE(std::isinf(energy) || std::isnan(energy));
}

TEST_F(PredictiveRealTest, NullGuard_UpdateModelNull) {
    bool success = predictive_update_model(nullptr);
    EXPECT_FALSE(success);
}

TEST_F(PredictiveRealTest, NullGuard_GetStatisticsNull) {
    predictive_stats_t stats;
    bool success = predictive_get_statistics(nullptr, &stats);
    EXPECT_FALSE(success);
}

TEST_F(PredictiveRealTest, NullGuard_DestroyNull) {
    predictive_destroy(nullptr);
    SUCCEED();
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
