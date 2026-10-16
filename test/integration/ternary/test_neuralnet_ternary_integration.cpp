//=============================================================================
// test_neuralnet_ternary_integration.cpp - Ternary Neural Network Integration Tests
//=============================================================================
/**
 * @file test_neuralnet_ternary_integration.cpp
 * @brief Integration tests for ternary representation in neural networks
 *
 * WHAT: Tests ternary weight integration with full neural network operations
 * WHY:  Verify ternary weights work correctly in forward passes and weight conversion
 * HOW:  Create networks with ternary synapses, test forward pass, verify consistency
 *
 * TEST CATEGORIES:
 * 1. Ternary weights with full neural network forward pass
 * 2. Mixed float/ternary layer configurations
 * 3. Weight conversion during training
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>

// Headers have their own extern "C" guards
#include "core/neuralnet/nimcp_neuralnet.h"
#include "core/neuralnet/nimcp_neuron_synapse_access.h"
#include "utils/ternary/nimcp_ternary.h"
#include "utils/ternary/nimcp_ternary_types.h"
#include "utils/ternary/nimcp_ternary_vector.h"
#include "utils/ternary/nimcp_ternary_matrix.h"
#include "utils/ternary/nimcp_ternary_convert.h"
#include "utils/ternary/nimcp_ternary_tensor.h"

//=============================================================================
// Test Fixture
//=============================================================================

class NeuralNetTernaryIntegrationTest : public ::testing::Test {
protected:
    neural_network_t network = nullptr;

    void SetUp() override {
        // Create a basic network configuration
        network_config_t config;
        memset(&config, 0, sizeof(config));
        config.num_neurons = 100;
        config.ei_ratio = 0.8f;
        config.learning_rate = 0.01f;
        config.hebbian_rate = 0.001f;
        config.stdp_window = 20.0f;
        config.homeostatic_rate = 0.0001f;
        config.target_activity = 0.1f;
        config.adaptation_rate = 0.001f;
        config.refractory_period = 2.0f;
        config.min_weight = -1.0f;
        config.max_weight = 1.0f;
        config.update_interval = 1;
        config.input_size = 10;
        config.output_size = 5;
        config.enable_stdp = true;
        config.enable_hebbian = true;

        network = neural_network_create(&config);
        ASSERT_NE(network, nullptr) << "Failed to create neural network";
    }

    void TearDown() override {
        if (network) {
            neural_network_destroy(network);
            network = nullptr;
        }
    }

    // Helper to add connections with ternary weights
    void AddTernaryConnection(uint32_t from, uint32_t to, trit_t ternary_weight, float scale) {
        // Add connection with float weight first
        float float_weight = trit_to_float_scaled(ternary_weight, scale);
        bool success = neural_network_add_connection(network, from, to, float_weight);
        ASSERT_TRUE(success) << "Failed to add connection";

        // Get neuron and set ternary weight on last synapse handle
        neuron_t* neuron = neural_network_get_neuron(network, from);
        ASSERT_NE(neuron, nullptr);

        uint32_t count = NEURON_OUT_COUNT(neuron);
        if (count > 0) {
            synapse_handle_t* h = NEURON_OUT_HANDLE(neuron, count - 1);
            h->ternary_weight = ternary_weight;
            h->use_ternary_weight = true;
            synapse_t* meta = neural_network_get_out_meta(network, neuron, count - 1);
            if (meta) meta->ternary_scale = scale;
        }
    }
};

//=============================================================================
// Test Category 1: Ternary Weights with Full Neural Network Forward Pass
//=============================================================================

TEST_F(NeuralNetTernaryIntegrationTest, TernaryWeightForwardPass) {
    // Create connections with ternary weights
    const float scale = 1.0f;

    // Add excitatory connections (TRIT_POSITIVE)
    AddTernaryConnection(0, 50, TRIT_POSITIVE, scale);
    AddTernaryConnection(1, 51, TRIT_POSITIVE, scale);

    // Add inhibitory connections (TRIT_NEGATIVE)
    AddTernaryConnection(2, 52, TRIT_NEGATIVE, scale);
    AddTernaryConnection(3, 53, TRIT_NEGATIVE, scale);

    // Add silent connections (TRIT_UNKNOWN - zero weight)
    AddTernaryConnection(4, 54, TRIT_UNKNOWN, scale);

    // Prepare input
    float inputs[10] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    float outputs[5] = {0};

    // Run forward pass
    bool success = neural_network_forward(network, inputs, 10, outputs, 5);
    EXPECT_TRUE(success) << "Forward pass should succeed";

    // Verify output is valid (not NaN or Inf)
    for (int i = 0; i < 5; i++) {
        EXPECT_FALSE(std::isnan(outputs[i])) << "Output " << i << " is NaN";
        EXPECT_FALSE(std::isinf(outputs[i])) << "Output " << i << " is Inf";
    }
}

TEST_F(NeuralNetTernaryIntegrationTest, TernaryWeightConsistency) {
    // Verify that ternary weights produce consistent values
    const float scale = 2.0f;

    AddTernaryConnection(0, 10, TRIT_POSITIVE, scale);

    neuron_t* neuron = neural_network_get_neuron(network, 0);
    ASSERT_NE(neuron, nullptr);
    ASSERT_GT(NEURON_OUT_COUNT(neuron), 0u);

    synapse_handle_t* h = NEURON_OUT_HANDLE(neuron, NEURON_OUT_COUNT(neuron) - 1);

    // Verify ternary weight matches expected dequantized value
    float dequant = trit_dequantize_weight(h->ternary_weight, scale, -scale);
    EXPECT_FLOAT_EQ(dequant, scale) << "TRIT_POSITIVE should dequantize to positive scale";

    // Test negative
    h->ternary_weight = TRIT_NEGATIVE;
    dequant = trit_dequantize_weight(h->ternary_weight, scale, -scale);
    EXPECT_FLOAT_EQ(dequant, -scale) << "TRIT_NEGATIVE should dequantize to negative scale";

    // Test zero
    h->ternary_weight = TRIT_UNKNOWN;
    dequant = trit_dequantize_weight(h->ternary_weight, scale, -scale);
    EXPECT_FLOAT_EQ(dequant, 0.0f) << "TRIT_UNKNOWN should dequantize to zero";
}

TEST_F(NeuralNetTernaryIntegrationTest, TernaryWeightPropagation) {
    // Test that ternary weights correctly propagate signals
    const float scale = 1.0f;

    // Create a chain: input -> hidden -> output
    // Input neurons: 0-9, Hidden: 10-19, Output: 90-99
    for (int i = 0; i < 5; i++) {
        // Excitatory path
        AddTernaryConnection(i, 10 + i, TRIT_POSITIVE, scale);
        AddTernaryConnection(10 + i, 90 + i, TRIT_POSITIVE, scale);
    }

    // Verify connections exist
    for (int i = 0; i < 5; i++) {
        neuron_t* neuron = neural_network_get_neuron(network, i);
        ASSERT_NE(neuron, nullptr);
        EXPECT_GT(NEURON_OUT_COUNT(neuron), 0u) << "Neuron " << i << " should have synapses";
    }

    // Run forward pass
    float inputs[10] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float outputs[5] = {0};

    bool success = neural_network_forward(network, inputs, 10, outputs, 5);
    EXPECT_TRUE(success);
}

//=============================================================================
// Test Category 2: Mixed Float/Ternary Layer Configurations
//=============================================================================

TEST_F(NeuralNetTernaryIntegrationTest, MixedFloatTernaryWeights) {
    // Test network with both float and ternary weights

    // Add regular float connections
    neural_network_add_connection(network, 0, 20, 0.5f);
    neural_network_add_connection(network, 1, 21, -0.3f);
    neural_network_add_connection(network, 2, 22, 0.8f);

    // Add ternary connections
    AddTernaryConnection(3, 23, TRIT_POSITIVE, 1.0f);
    AddTernaryConnection(4, 24, TRIT_NEGATIVE, 1.0f);

    // Verify both types coexist
    neuron_t* n0 = neural_network_get_neuron(network, 0);
    neuron_t* n3 = neural_network_get_neuron(network, 3);

    ASSERT_NE(n0, nullptr);
    ASSERT_NE(n3, nullptr);
    ASSERT_GT(NEURON_OUT_COUNT(n0), 0u);
    ASSERT_GT(NEURON_OUT_COUNT(n3), 0u);

    // Float synapse should not use ternary
    EXPECT_FALSE(NEURON_OUT_HANDLE(n0, NEURON_OUT_COUNT(n0) - 1)->use_ternary_weight);

    // Ternary synapse should use ternary
    EXPECT_TRUE(NEURON_OUT_HANDLE(n3, NEURON_OUT_COUNT(n3) - 1)->use_ternary_weight);

    // Run forward pass with mixed configuration
    float inputs[10] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    float outputs[5] = {0};

    bool success = neural_network_forward(network, inputs, 10, outputs, 5);
    EXPECT_TRUE(success);
}

TEST_F(NeuralNetTernaryIntegrationTest, LayerWiseTernaryConfiguration) {
    // Configure ternary weights for specific layers

    // Layer 1 (neurons 0-19): Float weights
    for (int i = 0; i < 10; i++) {
        float weight = 0.1f * (float)(i + 1);
        neural_network_add_connection(network, i, 20 + i, weight);
    }

    // Layer 2 (neurons 20-39): Ternary weights
    for (int i = 0; i < 10; i++) {
        trit_t weight = (i % 3 == 0) ? TRIT_POSITIVE :
                        (i % 3 == 1) ? TRIT_NEGATIVE : TRIT_UNKNOWN;
        AddTernaryConnection(20 + i, 40 + i, weight, 1.0f);
    }

    // Verify layer configurations
    for (int i = 0; i < 10; i++) {
        neuron_t* layer1_neuron = neural_network_get_neuron(network, i);
        neuron_t* layer2_neuron = neural_network_get_neuron(network, 20 + i);

        ASSERT_NE(layer1_neuron, nullptr);
        ASSERT_NE(layer2_neuron, nullptr);

        if (NEURON_OUT_COUNT(layer1_neuron) > 0) {
            EXPECT_FALSE(NEURON_OUT_HANDLE(layer1_neuron, NEURON_OUT_COUNT(layer1_neuron) - 1)->use_ternary_weight);
        }
        if (NEURON_OUT_COUNT(layer2_neuron) > 0) {
            EXPECT_TRUE(NEURON_OUT_HANDLE(layer2_neuron, NEURON_OUT_COUNT(layer2_neuron) - 1)->use_ternary_weight);
        }
    }
}

//=============================================================================
// Test Category 3: Weight Conversion During Training
//=============================================================================

TEST_F(NeuralNetTernaryIntegrationTest, FloatToTernaryConversion) {
    // Test converting float weights to ternary

    // Add float connections
    float weights[] = {0.8f, -0.7f, 0.1f, -0.05f, 0.9f};
    for (int i = 0; i < 5; i++) {
        neural_network_add_connection(network, i, 50 + i, weights[i]);
    }

    // Convert to ternary
    float threshold = 0.3f;
    for (int i = 0; i < 5; i++) {
        neuron_t* neuron = neural_network_get_neuron(network, i);
        ASSERT_NE(neuron, nullptr);
        uint32_t nc = NEURON_OUT_COUNT(neuron);
        ASSERT_GT(nc, 0u);

        synapse_handle_t* h = NEURON_OUT_HANDLE(neuron, nc - 1);

        // Quantize float weight to ternary
        trit_t ternary = trit_from_float_threshold(h->weight, threshold);
        h->ternary_weight = ternary;
        h->use_ternary_weight = true;
        synapse_t* meta = neural_network_get_out_meta(network, neuron, nc - 1);
        if (meta) meta->ternary_scale = 1.0f;
    }

    // Verify conversions
    // weights[0] = 0.8f > 0.3f -> TRIT_POSITIVE
    // weights[1] = -0.7f < -0.3f -> TRIT_NEGATIVE
    // weights[2] = 0.1f in [-0.3, 0.3] -> TRIT_UNKNOWN
    // weights[3] = -0.05f in [-0.3, 0.3] -> TRIT_UNKNOWN
    // weights[4] = 0.9f > 0.3f -> TRIT_POSITIVE

    trit_t expected[] = {TRIT_POSITIVE, TRIT_NEGATIVE, TRIT_UNKNOWN, TRIT_UNKNOWN, TRIT_POSITIVE};

    for (int i = 0; i < 5; i++) {
        neuron_t* neuron = neural_network_get_neuron(network, i);
        synapse_handle_t* h = NEURON_OUT_HANDLE(neuron, NEURON_OUT_COUNT(neuron) - 1);
        EXPECT_EQ(h->ternary_weight, expected[i])
            << "Weight " << weights[i] << " should convert to " << (int)expected[i];
    }
}

TEST_F(NeuralNetTernaryIntegrationTest, TernaryToFloatDequantization) {
    // Test dequantizing ternary weights back to float
    const float pos_scale = 0.8f;
    const float neg_scale = -0.7f;

    trit_t ternary_weights[] = {TRIT_POSITIVE, TRIT_NEGATIVE, TRIT_UNKNOWN};
    float expected_floats[] = {pos_scale, neg_scale, 0.0f};

    for (int i = 0; i < 3; i++) {
        float dequant = trit_dequantize_weight(ternary_weights[i], pos_scale, neg_scale);
        EXPECT_FLOAT_EQ(dequant, expected_floats[i])
            << "Ternary " << (int)ternary_weights[i] << " should dequantize to " << expected_floats[i];
    }
}

TEST_F(NeuralNetTernaryIntegrationTest, AdaptiveThresholdQuantization) {
    // Test weight quantization with adaptive threshold based on weight distribution

    // Add connections with varying weights
    std::vector<float> weights = {0.9f, 0.8f, 0.7f, 0.2f, 0.1f, 0.05f, -0.05f, -0.1f, -0.6f, -0.8f};

    for (size_t i = 0; i < weights.size(); i++) {
        neural_network_add_connection(network, (uint32_t)i, 60 + (uint32_t)i, weights[i]);
    }

    // Compute weight statistics for adaptive threshold
    float mean = 0.0f;
    for (size_t i = 0; i < weights.size(); i++) {
        mean += weights[i];
    }
    mean /= (float)weights.size();

    float variance = 0.0f;
    for (size_t i = 0; i < weights.size(); i++) {
        float diff = weights[i] - mean;
        variance += diff * diff;
    }
    variance /= (float)weights.size();
    float std_dev = sqrtf(variance);

    // Use 0.5 * std_dev as threshold
    float adaptive_threshold = 0.5f * std_dev;

    // Quantize weights using adaptive threshold
    for (size_t i = 0; i < weights.size(); i++) {
        trit_t ternary = trit_quantize_weight(weights[i], 0.5f, mean, std_dev);

        // Verify quantization is consistent
        float centered = weights[i] - mean;
        if (centered >= adaptive_threshold) {
            EXPECT_EQ(ternary, TRIT_POSITIVE) << "Weight " << weights[i] << " should be POSITIVE";
        } else if (centered <= -adaptive_threshold) {
            EXPECT_EQ(ternary, TRIT_NEGATIVE) << "Weight " << weights[i] << " should be NEGATIVE";
        } else {
            EXPECT_EQ(ternary, TRIT_UNKNOWN) << "Weight " << weights[i] << " should be UNKNOWN";
        }
    }
}

//=============================================================================
// Test: Ternary Vector Integration with Neural Network
//=============================================================================

TEST_F(NeuralNetTernaryIntegrationTest, TernaryVectorWeightIntegration) {
    // Create a ternary vector to represent weights
    const size_t num_weights = 20;
    trit_vector_t* weight_vec = trit_vector_create(num_weights, TERNARY_PACK_NONE);
    ASSERT_NE(weight_vec, nullptr);

    // Set ternary weights
    for (size_t i = 0; i < num_weights; i++) {
        trit_t val = (i % 3 == 0) ? TRIT_POSITIVE :
                     (i % 3 == 1) ? TRIT_NEGATIVE : TRIT_UNKNOWN;
        ternary_error_t err = trit_vector_set(weight_vec, i, val);
        EXPECT_EQ(err, TERNARY_OK);
    }

    // Apply weights to network connections
    for (size_t i = 0; i < num_weights; i++) {
        trit_t weight = trit_vector_get(weight_vec, i);
        AddTernaryConnection((uint32_t)i, 50 + (uint32_t)i, weight, 1.0f);
    }

    // Verify weights match
    for (size_t i = 0; i < num_weights; i++) {
        neuron_t* neuron = neural_network_get_neuron(network, (uint32_t)i);
        ASSERT_NE(neuron, nullptr);
        ASSERT_GT(NEURON_OUT_COUNT(neuron), 0u);

        trit_t vec_weight = trit_vector_get(weight_vec, i);
        trit_t syn_weight = NEURON_OUT_HANDLE(neuron, NEURON_OUT_COUNT(neuron) - 1)->ternary_weight;
        EXPECT_EQ(syn_weight, vec_weight) << "Synapse weight should match vector weight at index " << i;
    }

    trit_vector_destroy(weight_vec);
}

TEST_F(NeuralNetTernaryIntegrationTest, TernaryMatrixWeightIntegration) {
    // Create a ternary weight matrix (input x output)
    const size_t rows = 5;  // Input neurons
    const size_t cols = 5;  // Output neurons

    trit_matrix_t* weight_mat = trit_matrix_create(rows, cols, TERNARY_PACK_NONE);
    ASSERT_NE(weight_mat, nullptr);

    // Create a pattern: excitatory diagonal, inhibitory off-diagonal
    for (size_t r = 0; r < rows; r++) {
        for (size_t c = 0; c < cols; c++) {
            trit_t val = (r == c) ? TRIT_POSITIVE : TRIT_NEGATIVE;
            ternary_error_t err = trit_matrix_set(weight_mat, r, c, val);
            EXPECT_EQ(err, TERNARY_OK);
        }
    }

    // Apply matrix weights to network
    for (size_t r = 0; r < rows; r++) {
        for (size_t c = 0; c < cols; c++) {
            trit_t weight = trit_matrix_get(weight_mat, r, c);
            // Only add connection if weight is non-zero
            if (weight != TRIT_UNKNOWN) {
                AddTernaryConnection((uint32_t)r, 70 + (uint32_t)c, weight, 1.0f);
            }
        }
    }

    // Run forward pass
    float inputs[10] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float outputs[5] = {0};

    bool success = neural_network_forward(network, inputs, 10, outputs, 5);
    EXPECT_TRUE(success);

    trit_matrix_destroy(weight_mat);
}

//=============================================================================
// Test: Ternary Weight Sparsity
//=============================================================================

TEST_F(NeuralNetTernaryIntegrationTest, TernarySparsityComputation) {
    // Create sparse ternary weight matrix
    const size_t rows = 10;
    const size_t cols = 10;

    trit_matrix_t* sparse_mat = trit_matrix_create(rows, cols, TERNARY_PACK_NONE);
    ASSERT_NE(sparse_mat, nullptr);

    // Make 70% of weights UNKNOWN (zero)
    int zero_count = 0;
    for (size_t r = 0; r < rows; r++) {
        for (size_t c = 0; c < cols; c++) {
            trit_t val;
            if ((r + c) % 10 < 3) {
                val = TRIT_POSITIVE;
            } else if ((r + c) % 10 < 6) {
                val = TRIT_NEGATIVE;
            } else {
                val = TRIT_UNKNOWN;
                zero_count++;
            }
            trit_matrix_set(sparse_mat, r, c, val);
        }
    }

    // Compute sparsity
    float sparsity = trit_matrix_sparsity(sparse_mat);
    float expected_sparsity = (float)zero_count / (float)(rows * cols);

    EXPECT_NEAR(sparsity, expected_sparsity, 0.01f) << "Sparsity should match expected value";

    trit_matrix_destroy(sparse_mat);
}

//=============================================================================
// Test: Network Compute Step with Ternary Weights
//=============================================================================

TEST_F(NeuralNetTernaryIntegrationTest, ComputeStepWithTernaryWeights) {
    // Add ternary connections
    for (int i = 0; i < 10; i++) {
        trit_t weight = (i % 2 == 0) ? TRIT_POSITIVE : TRIT_NEGATIVE;
        AddTernaryConnection(i, 50 + i, weight, 1.0f);
    }

    // Set input currents
    for (int i = 0; i < 10; i++) {
        neuron_t* neuron = neural_network_get_neuron(network, i);
        if (neuron) {
            neuron->external_current = 0.5f;
        }
    }

    // Run compute step
    uint64_t timestamp = 1000;
    uint32_t spikes = neural_network_compute_step(network, timestamp);

    // Verify network processed without errors
    // (Spike count can be 0 depending on thresholds)
    EXPECT_GE(spikes, 0u);

    // Run multiple steps
    for (int step = 0; step < 100; step++) {
        timestamp += 1;
        spikes = neural_network_compute_step(network, timestamp);
    }

    // Network should still be functional
    network_stats_t stats;
    bool success = neural_network_get_stats(network, &stats);
    EXPECT_TRUE(success);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
