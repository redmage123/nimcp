/**
 * @file test_learning_sparse_synapse.cpp
 * @brief Tests for learning rules with sparse synapse storage
 *
 * WHAT: Verify that learning rules (STDP, Oja, weight normalization, reward learning)
 *       work correctly with the sparse synapse storage API (NIMCP 2.11)
 *
 * WHY:  Bug fix validation — learning rules previously used dense array access
 *       (neuron->synapses[i]) which is incompatible with sparse synapse storage.
 *       The migration to sparse_synapse_iterator/handle API must preserve
 *       learning behavior while supporting the new storage format.
 *
 * HOW:  Create networks with sparse synapse storage (the default since NIMCP 2.11),
 *       run learning rules, and verify weights change correctly without crashes.
 */

#include "test_helpers.h"
#include <cmath>

extern "C" {
#include "core/neuralnet/nimcp_neuralnet.h"
#include "core/neuralnet/nimcp_neuralnet_learning.h"
#include "core/neuralnet/nimcp_neuron_synapse_access.h"
}

//=============================================================================
// Helper: Create network with sparse synapses and connections
//=============================================================================

static neural_network_t create_sparse_test_network(uint32_t num_neurons) {
    network_config_t config = create_test_config();
    config.num_neurons = num_neurons;
    config.input_size = num_neurons / 2;
    config.output_size = num_neurons - config.input_size;
    config.min_weight = -1.0f;
    config.max_weight = 1.0f;
    config.learning_rate = 0.01f;
    config.stdp_window = 20.0f;
    config.enable_stdp = true;
    config.enable_oja = true;

    return neural_network_create(&config);
}

//=============================================================================
// STDP with Sparse Storage
//=============================================================================

TEST(LearningSparse, StdpUpdateWithSparseStorage) {
    // Create network with sparse synapses (default since NIMCP 2.11)
    neural_network_t network = create_sparse_test_network(10);
    ASSERT_NE(network, nullptr);

    // Add connections (these go into sparse synapse storage)
    ASSERT_TRUE(neural_network_add_connection(network, 0, 5, 0.5f));
    ASSERT_TRUE(neural_network_add_connection(network, 1, 6, 0.4f));
    ASSERT_TRUE(neural_network_add_connection(network, 2, 7, 0.3f));

    // Verify synapses were added via sparse API
    neuron_t* n0 = neural_network_get_neuron(network, 0);
    ASSERT_NE(n0, nullptr);
    EXPECT_GE(NEURON_OUT_COUNT(n0), 1u);

    // Record spikes with causal timing (pre before post -> potentiation)
    neural_network_record_spike(network, 0, 1.0f, 1);
    neural_network_record_spike(network, 5, 1.0f, 5);

    // Apply STDP - should not crash and should modify weights
    uint32_t modified = neural_network_apply_stdp(network, 0, 5);

    // STDP should modify at least one synapse (neuron 0 -> 5)
    EXPECT_GT(modified, 0u)
        << "STDP should modify synapses with causal spike timing";

    neural_network_destroy(network);
}

TEST(LearningSparse, StdpAntiCausalWithSparse) {
    neural_network_t network = create_sparse_test_network(10);
    ASSERT_NE(network, nullptr);

    ASSERT_TRUE(neural_network_add_connection(network, 0, 5, 0.5f));

    // Anti-causal: post before pre -> depression
    neural_network_record_spike(network, 5, 1.0f, 1);
    neural_network_record_spike(network, 0, 1.0f, 5);

    uint32_t modified = neural_network_apply_stdp(network, 0, 5);
    EXPECT_GT(modified, 0u)
        << "STDP should modify synapses with anti-causal timing";

    neural_network_destroy(network);
}

TEST(LearningSparse, StdpMultipleSynapsesSparse) {
    neural_network_t network = create_sparse_test_network(10);
    ASSERT_NE(network, nullptr);

    // Add multiple outgoing connections from neuron 0
    ASSERT_TRUE(neural_network_add_connection(network, 0, 3, 0.5f));
    ASSERT_TRUE(neural_network_add_connection(network, 0, 4, 0.4f));
    ASSERT_TRUE(neural_network_add_connection(network, 0, 5, 0.3f));

    // Record spikes
    neural_network_record_spike(network, 0, 1.0f, 1);
    neural_network_record_spike(network, 3, 1.0f, 3);
    neural_network_record_spike(network, 4, 1.0f, 4);
    neural_network_record_spike(network, 5, 1.0f, 5);

    uint32_t modified = neural_network_apply_stdp(network, 0, 5);
    EXPECT_GT(modified, 0u)
        << "STDP with multiple sparse synapses should modify at least one";

    neural_network_destroy(network);
}

//=============================================================================
// Oja's Rule with Sparse Storage
//=============================================================================

TEST(LearningSparse, OjaRuleWithSparseStorage) {
    neural_network_t network = create_sparse_test_network(10);
    ASSERT_NE(network, nullptr);

    // Add connection
    ASSERT_TRUE(neural_network_add_connection(network, 0, 5, 0.5f));

    // Activate neurons to generate activity
    neural_network_update_neuron(network, 0, 1.0f, 1);
    neural_network_update_neuron(network, 5, 1.0f, 1);

    // Apply Oja's rule
    uint32_t modified = neural_network_apply_oja(network, 0, 1);

    // Should modify weights based on activity
    EXPECT_GT(modified, 0u)
        << "Oja's rule should modify weights with active neurons";

    neural_network_destroy(network);
}

TEST(LearningSparse, OjaMultipleIterations) {
    neural_network_t network = create_sparse_test_network(10);
    ASSERT_NE(network, nullptr);

    ASSERT_TRUE(neural_network_add_connection(network, 0, 5, 0.5f));
    ASSERT_TRUE(neural_network_add_connection(network, 0, 6, 0.4f));

    // Run multiple iterations of Oja's rule
    for (uint64_t t = 1; t <= 50; t++) {
        neural_network_update_neuron(network, 0, 1.0f, t);
        neural_network_update_neuron(network, 5, 0.8f, t);
        neural_network_update_neuron(network, 6, 0.3f, t);
        neural_network_apply_oja(network, 0, t);
    }

    // Weights should have adapted — verify via weight statistics
    float mean, std_dev;
    neural_network_get_weight_statistics(network, 0, &mean, &std_dev);
    EXPECT_GT(mean, 0.0f) << "Oja-updated weights should remain positive";

    neural_network_destroy(network);
}

//=============================================================================
// Weight Normalization with Sparse Storage
//=============================================================================

TEST(LearningSparse, WeightNormalizationSparse) {
    neural_network_t network = create_sparse_test_network(10);
    ASSERT_NE(network, nullptr);

    // Add multiple connections with high weights
    ASSERT_TRUE(neural_network_add_connection(network, 0, 5, 0.9f));
    ASSERT_TRUE(neural_network_add_connection(network, 0, 6, 0.8f));
    ASSERT_TRUE(neural_network_add_connection(network, 0, 7, 0.7f));

    // Normalize weights - should not crash with sparse storage
    bool result = neural_network_normalize_weights(network, 0);
    EXPECT_TRUE(result) << "Weight normalization should succeed";

    // Get weight norm after normalization
    float norm = neural_network_get_weight_norm(network, 0);
    EXPECT_GT(norm, 0.0f) << "Weight norm should be positive after normalization";

    neural_network_destroy(network);
}

TEST(LearningSparse, WeightStatisticsSparse) {
    neural_network_t network = create_sparse_test_network(10);
    ASSERT_NE(network, nullptr);

    ASSERT_TRUE(neural_network_add_connection(network, 0, 5, 0.2f));
    ASSERT_TRUE(neural_network_add_connection(network, 0, 6, 0.8f));

    float mean = 0.0f, std_dev = 0.0f;
    neural_network_get_weight_statistics(network, 0, &mean, &std_dev);

    // Mean should be (0.2 + 0.8) / 2 = 0.5
    EXPECT_NEAR(mean, 0.5f, 0.1f)
        << "Mean weight should be approximately 0.5";

    // Std dev should be non-zero since weights differ
    EXPECT_GT(std_dev, 0.0f)
        << "Std dev should be positive with different weights";

    neural_network_destroy(network);
}

//=============================================================================
// Trace Updates with Sparse Storage
//=============================================================================

TEST(LearningSparse, TraceUpdatesSparse) {
    neural_network_t network = create_sparse_test_network(10);
    ASSERT_NE(network, nullptr);

    ASSERT_TRUE(neural_network_add_connection(network, 0, 5, 0.5f));

    // Update traces - should not crash with sparse storage
    neural_network_update_traces(network, 0, 100);

    // Verify neuron is still accessible and valid
    float state = 0.0f;
    EXPECT_TRUE(neural_network_get_neuron_state(network, 0, &state));

    neural_network_destroy(network);
}

//=============================================================================
// Reward Learning with Sparse Storage
//=============================================================================

TEST(LearningSparse, RewardLearningWithSparseStorage) {
    neural_network_t network = create_sparse_test_network(10);
    ASSERT_NE(network, nullptr);

    // Add connections
    ASSERT_TRUE(neural_network_add_connection(network, 0, 5, 0.5f));
    ASSERT_TRUE(neural_network_add_connection(network, 1, 6, 0.4f));

    // Activate some neurons
    neural_network_update_neuron(network, 0, 1.0f, 1);
    neural_network_update_neuron(network, 1, 0.8f, 1);
    neural_network_update_neuron(network, 5, 0.5f, 1);
    neural_network_update_neuron(network, 6, 0.3f, 1);

    // Apply reward learning - should not crash
    uint32_t modified = neural_network_apply_reward_learning(network, 0.8f, 0.01f, 1);

    // Reward learning may or may not modify weights depending on eligibility trace state,
    // but it should NOT crash
    (void)modified;

    neural_network_destroy(network);
}

//=============================================================================
// Lateral Inhibition (doesn't use synapses directly, but verify it still works)
//=============================================================================

TEST(LearningSparse, LateralInhibitionWithSparse) {
    neural_network_t network = create_sparse_test_network(10);
    ASSERT_NE(network, nullptr);

    // Set varying activations on output neurons
    neural_network_update_neuron(network, 5, 1.0f, 1);
    neural_network_update_neuron(network, 6, 0.5f, 1);
    neural_network_update_neuron(network, 7, 0.3f, 1);

    // Apply lateral inhibition
    uint32_t modified = neural_network_apply_lateral_inhibition(network, 5, 3, 0.5f);

    // Should modify non-winning neurons
    EXPECT_GT(modified, 0u)
        << "Lateral inhibition should modify non-winning neurons";

    neural_network_destroy(network);
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST(LearningSparse, StdpNoSynapses) {
    neural_network_t network = create_sparse_test_network(10);
    ASSERT_NE(network, nullptr);

    // Don't add any connections - STDP should handle gracefully
    uint32_t modified = neural_network_apply_stdp(network, 0, 1);
    EXPECT_EQ(modified, 0u)
        << "STDP with no synapses should modify zero";

    neural_network_destroy(network);
}

TEST(LearningSparse, OjaNoActivity) {
    neural_network_t network = create_sparse_test_network(10);
    ASSERT_NE(network, nullptr);

    ASSERT_TRUE(neural_network_add_connection(network, 0, 5, 0.5f));

    // Don't activate neurons - Oja should skip due to low activity
    uint32_t modified = neural_network_apply_oja(network, 0, 1);
    EXPECT_EQ(modified, 0u)
        << "Oja with no activity should modify zero synapses";

    neural_network_destroy(network);
}

TEST(LearningSparse, NormalizeZeroWeights) {
    neural_network_t network = create_sparse_test_network(10);
    ASSERT_NE(network, nullptr);

    // Add connection with zero weight
    ASSERT_TRUE(neural_network_add_connection(network, 0, 5, 0.0f));

    // Normalize should handle zero weights gracefully
    bool result = neural_network_normalize_weights(network, 0);
    EXPECT_TRUE(result);

    neural_network_destroy(network);
}

// NOTE: Invalid neuron ID tests omitted here because the error path
// invokes NIMCP_THROW_TO_IMMUNE which requires the immune system to
// be initialized (not available in unit test context).
// These boundary conditions are covered by the existing
// test_neuralnet_learning.cpp tests that run in a full test harness.

// NOTE: NULL network tests omitted because NIMCP_THROW_TO_IMMUNE on
// the NULL check path requires the immune system to be initialized.
// The NULL guard clauses themselves work correctly (verified by
// code inspection and the existing regression suite).
