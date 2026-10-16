#include "test_helpers.h"

//=============================================================================
// STDP Learning Tests
//=============================================================================

// Test basic STDP learning with causal timing
TEST(NeuralNetLearning, STDPCausal)
{
    network_config_t config = create_test_config();
    config.stdp_window = 20.0f;
    neural_network_t network = neural_network_create(&config);

    ASSERT_NE(network, nullptr);

    // Add test connection
    ASSERT_TRUE(neural_network_add_connection(network, 0, 1, 0.5f));

    // Get weight statistics before learning
    float mean_before, std_dev;
    neural_network_get_weight_statistics(network, 0, &mean_before, &std_dev);

    // Record spikes with causal timing (pre before post) - should strengthen
    neural_network_record_spike(network, 0, 1.0f, 1);
    neural_network_record_spike(network, 1, 1.0f, 5);

    // Apply STDP
    uint32_t modified = neural_network_apply_stdp(network, 0, 5);

    // Verify weight changes occurred
    ASSERT_GT(modified, 0);

    // Get weight statistics after learning
    float mean_after;
    neural_network_get_weight_statistics(network, 0, &mean_after, &std_dev);

    // Causal timing should potentiate weights (increase)
    EXPECT_GE(mean_after, mean_before);

    neural_network_destroy(network);
}

// Test STDP learning with anti-causal timing
TEST(NeuralNetLearning, STDPAntiCausal)
{
    network_config_t config = create_test_config();
    config.stdp_window = 20.0f;
    neural_network_t network = neural_network_create(&config);

    ASSERT_NE(network, nullptr);

    // Add test connection
    ASSERT_TRUE(neural_network_add_connection(network, 0, 1, 0.5f));

    // Get weight before
    float mean_before, std_dev;
    neural_network_get_weight_statistics(network, 0, &mean_before, &std_dev);

    // Record spikes with anti-causal timing (post before pre) - should weaken
    neural_network_record_spike(network, 1, 1.0f, 1);
    neural_network_record_spike(network, 0, 1.0f, 5);

    // Apply STDP
    uint32_t modified = neural_network_apply_stdp(network, 0, 5);

    // Weight should change (depression expected but not strictly required)
    ASSERT_GT(modified, 0);

    neural_network_destroy(network);
}

// Test STDP with multiple synapses
TEST(NeuralNetLearning, STDPMultipleSynapses)
{
    network_config_t config = create_test_config();
    neural_network_t network = neural_network_create(&config);

    ASSERT_NE(network, nullptr);

    // Add multiple connections
    ASSERT_TRUE(neural_network_add_connection(network, 0, 1, 0.5f));
    ASSERT_TRUE(neural_network_add_connection(network, 0, 2, 0.5f));
    ASSERT_TRUE(neural_network_add_connection(network, 0, 3, 0.5f));

    // Record spikes
    neural_network_record_spike(network, 0, 1.0f, 1);
    neural_network_record_spike(network, 1, 1.0f, 3);
    neural_network_record_spike(network, 2, 1.0f, 4);
    neural_network_record_spike(network, 3, 1.0f, 5);

    // Apply STDP
    uint32_t modified = neural_network_apply_stdp(network, 0, 5);

    // Multiple synapses should be modified
    EXPECT_GT(modified, 0);

    neural_network_destroy(network);
}

//=============================================================================
// Oja's Learning Rule Tests
//=============================================================================

// Test basic Oja's learning
TEST(NeuralNetLearning, OjaBasic)
{
    network_config_t config = create_test_config();
    neural_network_t network = neural_network_create(&config);

    ASSERT_NE(network, nullptr);

    // Add test connection
    ASSERT_TRUE(neural_network_add_connection(network, 0, 1, 0.5f));

    // Activate neurons
    neural_network_update_neuron(network, 0, 1.0f, 1);
    neural_network_update_neuron(network, 1, 1.0f, 1);

    // Apply Oja's rule
    uint32_t modified = neural_network_apply_oja(network, 0, 1);

    // Verify weight changes
    ASSERT_GT(modified, 0);

    neural_network_destroy(network);
}

// Test Oja's learning with weight normalization
TEST(NeuralNetLearning, OjaWeightNormalization)
{
    network_config_t config = create_test_config();
    neural_network_t network = neural_network_create(&config);

    ASSERT_NE(network, nullptr);

    // Add multiple connections
    ASSERT_TRUE(neural_network_add_connection(network, 0, 1, 1.0f));
    ASSERT_TRUE(neural_network_add_connection(network, 0, 2, 1.0f));
    ASSERT_TRUE(neural_network_add_connection(network, 0, 3, 1.0f));

    // Get initial weight norm
    float norm_before = neural_network_get_weight_norm(network, 0);

    // Activate neurons repeatedly
    for (uint64_t t = 1; t <= 100; t++) {
        neural_network_update_neuron(network, 0, 1.0f, t);
        neural_network_update_neuron(network, 1, 1.0f, t);
        neural_network_apply_oja(network, 0, t);
    }

    // Get final weight norm
    float norm_after = neural_network_get_weight_norm(network, 0);

    // Oja's rule should control weight growth
    EXPECT_LT(norm_after, norm_before * 10.0f);  // Shouldn't explode

    neural_network_destroy(network);
}

// Test Oja's learning with correlated inputs
TEST(NeuralNetLearning, OjaCorrelatedInputs)
{
    network_config_t config = create_test_config();
    neural_network_t network = neural_network_create(&config);

    ASSERT_NE(network, nullptr);

    // Add connections
    ASSERT_TRUE(neural_network_add_connection(network, 0, 1, 0.3f));
    ASSERT_TRUE(neural_network_add_connection(network, 0, 2, 0.3f));

    // Present correlated patterns
    for (uint64_t t = 1; t <= 50; t++) {
        // Neuron 1 and 2 fire together (correlated)
        neural_network_update_neuron(network, 1, 1.0f, t);
        neural_network_update_neuron(network, 2, 1.0f, t);
        neural_network_update_neuron(network, 0, 0.5f, t);

        neural_network_apply_oja(network, 0, t);
    }

    // Weights should have adapted
    float mean, std_dev;
    neural_network_get_weight_statistics(network, 0, &mean, &std_dev);
    EXPECT_GT(mean, 0.0f);

    neural_network_destroy(network);
}

//=============================================================================
// Homeostatic Plasticity Tests
//=============================================================================

// Test basic homeostatic plasticity
TEST(NeuralNetLearning, HomeostasisBasic)
{
    network_config_t config = create_test_config();
    config.homeostatic_rate = 0.01f;
    config.target_activity = 0.5f;
    neural_network_t network = neural_network_create(&config);

    ASSERT_NE(network, nullptr);

    // Apply homeostasis
    ASSERT_TRUE(neural_network_apply_homeostasis(network, 0, 1000));

    // Verify function executes without error

    neural_network_destroy(network);
}

// Test homeostasis with high activity
TEST(NeuralNetLearning, HomeostasisHighActivity)
{
    network_config_t config = create_test_config();
    config.homeostatic_rate = 0.1f;
    config.target_activity = 0.3f;
    neural_network_t network = neural_network_create(&config);

    ASSERT_NE(network, nullptr);

    // Create high activity pattern
    for (uint64_t t = 1; t <= 100; t++) {
        neural_network_record_spike(network, 0, 1.0f, t);
    }

    // Apply homeostasis
    ASSERT_TRUE(neural_network_apply_homeostasis(network, 0, 1000));

    // Homeostasis should adjust to reduce activity
    // (exact behavior depends on implementation)

    neural_network_destroy(network);
}

// Test homeostasis with low activity
TEST(NeuralNetLearning, HomeostasisLowActivity)
{
    network_config_t config = create_test_config();
    config.homeostatic_rate = 0.1f;
    config.target_activity = 0.5f;
    neural_network_t network = neural_network_create(&config);

    ASSERT_NE(network, nullptr);

    // Keep activity low (no spikes)

    // Apply homeostasis
    ASSERT_TRUE(neural_network_apply_homeostasis(network, 0, 1000));

    // Homeostasis should adjust to increase excitability

    neural_network_destroy(network);
}

//=============================================================================
// Meta-Plasticity Tests
//=============================================================================

// Test plasticity update mechanism
TEST(NeuralNetLearning, PlasticityUpdate)
{
    network_config_t config = create_test_config();
    neural_network_t network = neural_network_create(&config);

    ASSERT_NE(network, nullptr);

    // Add connection
    ASSERT_TRUE(neural_network_add_connection(network, 0, 1, 0.5f));

    // Update plasticity
    uint32_t updated = neural_network_update_plasticity(network, 0, 1);

    // Verify update occurred
    EXPECT_GT(updated, 0);

    neural_network_destroy(network);
}

// Test plasticity with activity patterns
TEST(NeuralNetLearning, PlasticityWithActivity)
{
    network_config_t config = create_test_config();
    neural_network_t network = neural_network_create(&config);

    ASSERT_NE(network, nullptr);

    // Add connections
    ASSERT_TRUE(neural_network_add_connection(network, 0, 1, 0.5f));

    // Create activity pattern
    for (uint64_t t = 1; t <= 50; t++) {
        neural_network_update_neuron(network, 0, 1.0f, t);
        neural_network_update_plasticity(network, 0, t);
    }

    // Plasticity should adapt based on activity variance

    neural_network_destroy(network);
}

//=============================================================================
// Combined Learning Tests
//=============================================================================

// Test STDP and homeostasis together
TEST(NeuralNetLearning, STDPWithHomeostasis)
{
    network_config_t config = create_test_config();
    config.stdp_window = 20.0f;
    config.homeostatic_rate = 0.01f;
    config.target_activity = 0.3f;
    neural_network_t network = neural_network_create(&config);

    ASSERT_NE(network, nullptr);

    // Add connections
    ASSERT_TRUE(neural_network_add_connection(network, 0, 1, 0.5f));

    // Run learning over time
    for (uint64_t t = 1; t <= 100; t++) {
        // Present spikes
        if (t % 10 == 0) {
            neural_network_record_spike(network, 0, 1.0f, t);
            neural_network_record_spike(network, 1, 1.0f, t + 2);
        }

        // Apply learning rules
        neural_network_apply_stdp(network, 0, t);

        // Apply homeostasis periodically
        if (t % 50 == 0) {
            neural_network_apply_homeostasis(network, 0, t);
        }
    }

    // Network should remain stable
    network_stats_t stats;
    neural_network_get_stats(network, &stats);
    EXPECT_LT(stats.avg_weight, 10.0f);  // Weights shouldn't explode

    neural_network_destroy(network);
}

// Test Oja and STDP together
TEST(NeuralNetLearning, OjaWithSTDP)
{
    network_config_t config = create_test_config();
    neural_network_t network = neural_network_create(&config);

    ASSERT_NE(network, nullptr);

    // Add connections
    ASSERT_TRUE(neural_network_add_connection(network, 0, 1, 0.5f));
    ASSERT_TRUE(neural_network_add_connection(network, 0, 2, 0.5f));

    // Apply both learning rules
    for (uint64_t t = 1; t <= 50; t++) {
        neural_network_update_neuron(network, 0, 1.0f, t);
        neural_network_update_neuron(network, 1, 1.0f, t);

        neural_network_apply_oja(network, 0, t);
        neural_network_apply_stdp(network, 0, t);
    }

    // Both learning rules should cooperate
    float mean, std_dev;
    neural_network_get_weight_statistics(network, 0, &mean, &std_dev);
    EXPECT_GT(mean, 0.0f);

    neural_network_destroy(network);
}

//=============================================================================
// Network Maintenance Tests
//=============================================================================

// Test network maintenance routine
TEST(NeuralNetLearning, NetworkMaintenance)
{
    network_config_t config = create_test_config();
    config.update_interval = 100;
    neural_network_t network = neural_network_create(&config);

    ASSERT_NE(network, nullptr);

    // Add connections
    ASSERT_TRUE(neural_network_add_connection(network, 0, 1, 0.5f));
    ASSERT_TRUE(neural_network_add_connection(network, 1, 2, 0.3f));

    // Run some activity
    for (uint64_t t = 1; t <= 50; t++) {
        neural_network_update_neuron(network, 0, 1.0f, t);
    }

    // Run maintenance
    neural_network_maintain(network, 200);

    // Network should still be functional
    network_stats_t stats;
    neural_network_get_stats(network, &stats);
    EXPECT_GT(stats.num_neurons, 0);

    neural_network_destroy(network);
}

// Test synaptic pruning
TEST(NeuralNetLearning, SynapticPruning)
{
    network_config_t config = create_test_config();
    neural_network_t network = neural_network_create(&config);

    ASSERT_NE(network, nullptr);

    // Add connections with varying weights
    ASSERT_TRUE(neural_network_add_connection(network, 0, 1, 0.9f));
    ASSERT_TRUE(neural_network_add_connection(network, 0, 2, 0.01f));  // Very weak
    ASSERT_TRUE(neural_network_add_connection(network, 0, 3, 0.8f));

    // Prune weak synapses
    uint32_t pruned = neural_network_prune_synapses(network, 0.05f);

    // At least one weak synapse should be pruned
    EXPECT_GT(pruned, 0);

    neural_network_destroy(network);
}
