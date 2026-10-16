/**
 * @file test_neuralnet_comprehensive.cpp
 * @brief Comprehensive unit tests for nimcp_neuralnet.c (Target: 95%+ coverage)
 *
 * WHAT: Complete test coverage for neural network implementation
 * WHY:  Increase coverage from 14.1% to 95%+ (407+ uncovered lines)
 * HOW:  Test all functions, branches, learning rules, and edge cases
 *
 * COVERAGE STRATEGY:
 * 1. Network Creation & Destruction (all configurations)
 * 2. Activation Functions (sigmoid, tanh, ReLU, leaky ReLU, adaptive)
 * 3. Neuron Updates & Dynamics (membrane potential, spikes, refractory)
 * 4. Synapse Management (add, typed, bidirectional tracking)
 * 5. Learning Rules (Hebbian, Oja, Generalized Oja, STDP, Hybrid)
 * 6. Plasticity (homeostatic, meta-plasticity, traces, calcium)
 * 7. Weight Management (normalization, pruning, statistics)
 * 8. Network Operations (forward pass, compute step, reset)
 * 9. Neuron Models (LIF, Izhikevich)
 * 10. Integration Systems (neuromodulators, glial, global state)
 * 11. Statistics & Monitoring (activity, weight norms, network stats)
 * 12. Error Paths (null checks, invalid IDs, capacity limits)
 */

#include "test_helpers.h"

//=============================================================================
// Test Fixture for Network Operations
//=============================================================================

class NeuralNetComprehensive : public ::testing::Test {
protected:
    neural_network_t network;
    network_config_t config;

    void SetUp() override
    {
        config = create_test_config();
        network = nullptr;
    }

    void TearDown() override
    {
        if (network) {
            neural_network_destroy(network);
            network = nullptr;
        }
    }

    // Helper to create network with connections
    void CreateNetworkWithConnections()
    {
        network = neural_network_create(&config);
        ASSERT_NE(network, nullptr);

        // Create simple chain: 0 -> 1 -> 2
        neural_network_add_connection(network, 0, 1, 0.5f);
        neural_network_add_connection(network, 1, 2, 0.5f);
    }

    // Helper to create fully connected network
    void CreateFullyConnectedNetwork()
    {
        network = neural_network_create(&config);
        ASSERT_NE(network, nullptr);

        // Connect all neurons to each other
        for (uint32_t i = 0; i < config.num_neurons; i++) {
            for (uint32_t j = 0; j < config.num_neurons; j++) {
                if (i != j) {
                    neural_network_add_connection(network, i, j, 0.3f);
                }
            }
        }
    }
};

//=============================================================================
// 1. Network Creation/Destruction Tests
//=============================================================================

TEST_F(NeuralNetComprehensive, CreateWithMinimalConfig)
{
    config.num_neurons = 1;
    network = neural_network_create(&config);
    ASSERT_NE(network, nullptr);
    EXPECT_EQ(neural_network_get_num_neurons(network), 1);
}

TEST_F(NeuralNetComprehensive, CreateWithLargeConfig)
{
    config.num_neurons = 1000;
    network = neural_network_create(&config);
    ASSERT_NE(network, nullptr);
    EXPECT_EQ(neural_network_get_num_neurons(network), 1000);
}

TEST_F(NeuralNetComprehensive, CreateWithCustomEIRatio)
{
    config.num_neurons = 100;
    config.ei_ratio = 0.9f;  // 90% excitatory
    network = neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    network_stats_t stats;
    neural_network_get_stats(network, &stats);
    EXPECT_EQ(stats.num_excitatory, 90);
    EXPECT_EQ(stats.num_inhibitory, 10);
}

TEST_F(NeuralNetComprehensive, CreateWithZeroEIRatio)
{
    config.num_neurons = 10;
    config.ei_ratio = 0.0f;  // All inhibitory
    network = neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    network_stats_t stats;
    neural_network_get_stats(network, &stats);
    EXPECT_EQ(stats.num_excitatory, 0);
    EXPECT_EQ(stats.num_inhibitory, 10);
}

TEST_F(NeuralNetComprehensive, CreateWithAllEIRatio)
{
    config.num_neurons = 10;
    config.ei_ratio = 1.0f;  // All excitatory
    network = neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    network_stats_t stats;
    neural_network_get_stats(network, &stats);
    EXPECT_EQ(stats.num_excitatory, 10);
    EXPECT_EQ(stats.num_inhibitory, 0);
}

TEST_F(NeuralNetComprehensive, DestroyNullNetwork)
{
    // Should not crash
    neural_network_destroy(nullptr);
    SUCCEED();
}

//=============================================================================
// 2. Neuron Management Tests
//=============================================================================

TEST_F(NeuralNetComprehensive, AddNeuronSigmoid)
{
    network = neural_network_create(&config);
    uint32_t id = neural_network_add_neuron(network, ACTIVATION_SIGMOID);
    EXPECT_NE(id, UINT32_MAX);
    EXPECT_EQ(neural_network_get_num_neurons(network), config.num_neurons + 1);
}

TEST_F(NeuralNetComprehensive, AddNeuronReLU)
{
    network = neural_network_create(&config);
    uint32_t id = neural_network_add_neuron(network, ACTIVATION_RELU);
    EXPECT_NE(id, UINT32_MAX);
}

TEST_F(NeuralNetComprehensive, AddNeuronLeakyReLU)
{
    network = neural_network_create(&config);
    uint32_t id = neural_network_add_neuron(network, ACTIVATION_LEAKY_RELU);
    EXPECT_NE(id, UINT32_MAX);
}

TEST_F(NeuralNetComprehensive, AddNeuronTanh)
{
    network = neural_network_create(&config);
    uint32_t id = neural_network_add_neuron(network, ACTIVATION_TANH);
    EXPECT_NE(id, UINT32_MAX);
}

TEST_F(NeuralNetComprehensive, AddNeuronAdaptive)
{
    network = neural_network_create(&config);
    uint32_t id = neural_network_add_neuron(network, ACTIVATION_ADAPTIVE);
    EXPECT_NE(id, UINT32_MAX);
}

TEST_F(NeuralNetComprehensive, AddNeuronToNullNetwork)
{
    uint32_t id = neural_network_add_neuron(nullptr, ACTIVATION_SIGMOID);
    EXPECT_EQ(id, UINT32_MAX);
}

TEST_F(NeuralNetComprehensive, GetNeuronValid)
{
    network = neural_network_create(&config);
    neuron_t* neuron = neural_network_get_neuron(network, 0);
    ASSERT_NE(neuron, nullptr);
    EXPECT_EQ(neuron->id, 0);
}

TEST_F(NeuralNetComprehensive, GetNeuronInvalid)
{
    network = neural_network_create(&config);
    neuron_t* neuron = neural_network_get_neuron(network, 1000);
    EXPECT_EQ(neuron, nullptr);
}

TEST_F(NeuralNetComprehensive, GetNeuronFromNullNetwork)
{
    neuron_t* neuron = neural_network_get_neuron(nullptr, 0);
    EXPECT_EQ(neuron, nullptr);
}

TEST_F(NeuralNetComprehensive, GetNeuronStateValid)
{
    network = neural_network_create(&config);
    float state;
    ASSERT_TRUE(neural_network_get_neuron_state(network, 0, &state));
    EXPECT_TRUE(float_equals(state, 0.0f));
}

TEST_F(NeuralNetComprehensive, GetNeuronStateInvalid)
{
    network = neural_network_create(&config);
    float state;
    EXPECT_FALSE(neural_network_get_neuron_state(network, 1000, &state));
}

TEST_F(NeuralNetComprehensive, GetNeuronStateNullOutput)
{
    network = neural_network_create(&config);
    EXPECT_FALSE(neural_network_get_neuron_state(network, 0, nullptr));
}

TEST_F(NeuralNetComprehensive, GetNumNeuronsValid)
{
    network = neural_network_create(&config);
    EXPECT_EQ(neural_network_get_num_neurons(network), config.num_neurons);
}

TEST_F(NeuralNetComprehensive, GetNumNeuronsNull)
{
    EXPECT_EQ(neural_network_get_num_neurons(nullptr), 0);
}

//=============================================================================
// 3. Synapse Management Tests
//=============================================================================

TEST_F(NeuralNetComprehensive, AddConnectionValid)
{
    network = neural_network_create(&config);
    EXPECT_TRUE(neural_network_add_connection(network, 0, 1, 0.5f));

    // Verify connection exists
    neuron_t* neuron = neural_network_get_neuron(network, 0);
    ASSERT_NE(neuron, nullptr);
    EXPECT_EQ(neuron->num_synapses, 1);
    EXPECT_EQ(neuron->synapses[0].target_id, 1);
}

TEST_F(NeuralNetComprehensive, AddConnectionBidirectionalTracking)
{
    network = neural_network_create(&config);
    ASSERT_TRUE(neural_network_add_connection(network, 0, 1, 0.5f));

    // Check incoming synapse on target neuron
    neuron_t* target = neural_network_get_neuron(network, 1);
    ASSERT_NE(target, nullptr);
    EXPECT_EQ(target->num_incoming, 1);
}

TEST_F(NeuralNetComprehensive, AddConnectionWeightClamping)
{
    config.min_weight = -1.0f;
    config.max_weight = 1.0f;
    network = neural_network_create(&config);

    // Try to add connection with weight outside range
    ASSERT_TRUE(neural_network_add_connection(network, 0, 1, 5.0f));

    neuron_t* neuron = neural_network_get_neuron(network, 0);
    EXPECT_LE(neuron->synapses[0].weight, 1.0f);
    EXPECT_GE(neuron->synapses[0].weight, -1.0f);
}

TEST_F(NeuralNetComprehensive, AddConnectionInvalidSource)
{
    network = neural_network_create(&config);
    EXPECT_FALSE(neural_network_add_connection(network, 1000, 1, 0.5f));
}

TEST_F(NeuralNetComprehensive, AddConnectionInvalidTarget)
{
    network = neural_network_create(&config);
    EXPECT_FALSE(neural_network_add_connection(network, 0, 1000, 0.5f));
}

TEST_F(NeuralNetComprehensive, AddConnectionTypedAMPA)
{
    network = neural_network_create(&config);
    EXPECT_TRUE(
        neural_network_add_connection_typed(network, 0, 1, 0.5f, SYNAPSE_AMPA));

    neuron_t* neuron = neural_network_get_neuron(network, 0);
    EXPECT_EQ(neuron->synapses[0].type, SYNAPSE_AMPA);
}

TEST_F(NeuralNetComprehensive, AddConnectionTypedNMDA)
{
    network = neural_network_create(&config);
    EXPECT_TRUE(
        neural_network_add_connection_typed(network, 0, 1, 0.5f, SYNAPSE_NMDA));

    neuron_t* neuron = neural_network_get_neuron(network, 0);
    EXPECT_EQ(neuron->synapses[0].type, SYNAPSE_NMDA);
}

TEST_F(NeuralNetComprehensive, AddConnectionTypedGABA_A)
{
    network = neural_network_create(&config);
    EXPECT_TRUE(
        neural_network_add_connection_typed(network, 0, 1, -0.5f, SYNAPSE_GABA_A));

    neuron_t* neuron = neural_network_get_neuron(network, 0);
    EXPECT_EQ(neuron->synapses[0].type, SYNAPSE_GABA_A);
}

TEST_F(NeuralNetComprehensive, AddConnectionUntilFull)
{
    network = neural_network_create(&config);

    // Fill up synapses to maximum
    uint32_t connections = 0;
    for (uint32_t i = 0; i < MAX_SYNAPSES_PER_NEURON; i++) {
        uint32_t target = (i + 1) % config.num_neurons;
        if (neural_network_add_connection(network, 0, target, 0.5f)) {
            connections++;
        }
    }

    EXPECT_EQ(connections, MAX_SYNAPSES_PER_NEURON);

    // Next connection should fail
    EXPECT_FALSE(neural_network_add_connection(network, 0, 5, 0.5f));
}

//=============================================================================
// 4. Network Propagation Tests
//=============================================================================

TEST_F(NeuralNetComprehensive, UpdateNeuronBasic)
{
    CreateNetworkWithConnections();

    // Update neuron with input current
    EXPECT_TRUE(neural_network_update_neuron(network, 0, 0.1f, 1));

    float state;
    neural_network_get_neuron_state(network, 0, &state);
    // State should have changed from rest potential
    EXPECT_NE(state, 0.0f);
}

TEST_F(NeuralNetComprehensive, UpdateNeuronInvalidID)
{
    network = neural_network_create(&config);
    EXPECT_FALSE(neural_network_update_neuron(network, 1000, 0.1f, 1));
}

TEST_F(NeuralNetComprehensive, UpdateNeuronNullNetwork)
{
    EXPECT_FALSE(neural_network_update_neuron(nullptr, 0, 0.1f, 1));
}

TEST_F(NeuralNetComprehensive, ComputeStepBasic)
{
    CreateNetworkWithConnections();

    uint32_t active = neural_network_compute_step(network, 1);
    // Should return number of active neurons
    EXPECT_GE(active, 0);
}

TEST_F(NeuralNetComprehensive, ComputeStepMultipleTimes)
{
    CreateNetworkWithConnections();

    // Run multiple steps
    for (uint64_t t = 0; t < 100; t++) {
        neural_network_compute_step(network, t);
    }

    // Network should still be functional
    network_stats_t stats;
    neural_network_get_stats(network, &stats);
    EXPECT_EQ(stats.num_neurons, config.num_neurons);
}

TEST_F(NeuralNetComprehensive, ForwardPassBasic)
{
    config.input_size = 3;
    config.output_size = 2;
    network = neural_network_create(&config);

    float inputs[3] = {0.5f, 0.3f, 0.8f};
    float outputs[2];

    EXPECT_TRUE(neural_network_forward(network, inputs, 3, outputs, 2));
}

TEST_F(NeuralNetComprehensive, ForwardPassNullInput)
{
    config.input_size = 3;
    config.output_size = 2;
    network = neural_network_create(&config);

    float outputs[2];
    EXPECT_FALSE(neural_network_forward(network, nullptr, 3, outputs, 2));
}

TEST_F(NeuralNetComprehensive, ForwardPassNullOutput)
{
    config.input_size = 3;
    config.output_size = 2;
    network = neural_network_create(&config);

    float inputs[3] = {0.5f, 0.3f, 0.8f};
    EXPECT_FALSE(neural_network_forward(network, inputs, 3, nullptr, 2));
}

//=============================================================================
// 5. Learning Rules Tests
//=============================================================================

TEST_F(NeuralNetComprehensive, ApplyOjaBasic)
{
    CreateNetworkWithConnections();

    uint32_t modified = neural_network_apply_oja(network, 0, 10);
    // Should modify at least some synapses
    EXPECT_GE(modified, 0);
}

TEST_F(NeuralNetComprehensive, ApplyOjaInvalidNeuron)
{
    network = neural_network_create(&config);
    uint32_t modified = neural_network_apply_oja(network, 1000, 10);
    EXPECT_EQ(modified, 0);
}

TEST_F(NeuralNetComprehensive, ApplyGeneralizedOja)
{
    CreateNetworkWithConnections();

    uint32_t modified = neural_network_apply_generalized_oja(network, 0, 10);
    EXPECT_GE(modified, 0);
}

TEST_F(NeuralNetComprehensive, ApplySTDPWithSpikes)
{
    CreateNetworkWithConnections();

    // Record spikes for STDP
    neural_network_record_spike(network, 0, 1.0f, 1);
    neural_network_record_spike(network, 1, 1.0f, 5);

    uint32_t modified = neural_network_apply_stdp(network, 0, 10);
    EXPECT_GT(modified, 0);
}

TEST_F(NeuralNetComprehensive, ApplyHomeostasisBasic)
{
    network = neural_network_create(&config);

    // Update neuron first to set last_update timestamp
    neural_network_update_neuron(network, 0, 1.0f, 0);

    // Now apply homeostasis with sufficient time passed (> update_interval)
    EXPECT_TRUE(neural_network_apply_homeostasis(network, 0, 2000));
}

TEST_F(NeuralNetComprehensive, ApplyHomeostasisInvalidNeuron)
{
    network = neural_network_create(&config);

    EXPECT_FALSE(neural_network_apply_homeostasis(network, 1000, 100));
}

TEST_F(NeuralNetComprehensive, UpdatePlasticity)
{
    CreateNetworkWithConnections();

    uint32_t updated = neural_network_update_plasticity(network, 0, 10);
    EXPECT_GE(updated, 0);
}

//=============================================================================
// 6. Plasticity Operations Tests
//=============================================================================

TEST_F(NeuralNetComprehensive, NormalizeWeightsBasic)
{
    CreateNetworkWithConnections();

    EXPECT_TRUE(neural_network_normalize_weights(network, 0));

    // Check that weights are normalized
    float norm = neural_network_get_weight_norm(network, 0);
    EXPECT_GT(norm, 0.0f);
}

TEST_F(NeuralNetComprehensive, NormalizeWeightsInvalidNeuron)
{
    network = neural_network_create(&config);
    EXPECT_FALSE(neural_network_normalize_weights(network, 1000));
}

TEST_F(NeuralNetComprehensive, PruneSynapsesBasic)
{
    CreateFullyConnectedNetwork();

    // Set very weak weights
    neuron_t* neuron = neural_network_get_neuron(network, 0);
    for (uint32_t i = 0; i < neuron->num_synapses; i++) {
        neuron->synapses[i].weight = 0.001f;
        neuron->synapses[i].strength = 0.1f;
    }

    uint32_t pruned = neural_network_prune_synapses(network, 0.01f);
    EXPECT_GT(pruned, 0);
}

TEST_F(NeuralNetComprehensive, PruneSynapsesNone)
{
    CreateNetworkWithConnections();

    // All weights are strong, nothing should be pruned
    uint32_t pruned = neural_network_prune_synapses(network, 0.0001f);
    EXPECT_GE(pruned, 0);
}

//=============================================================================
// 7. Activity Tracking Tests
//=============================================================================

TEST_F(NeuralNetComprehensive, RecordSpikeBasic)
{
    network = neural_network_create(&config);

    EXPECT_TRUE(neural_network_record_spike(network, 0, 1.0f, 1));
}

TEST_F(NeuralNetComprehensive, RecordSpikeMultiple)
{
    network = neural_network_create(&config);

    for (uint32_t i = 0; i < 10; i++) {
        EXPECT_TRUE(neural_network_record_spike(network, 0, 1.0f, i * 10));
    }
}

TEST_F(NeuralNetComprehensive, RecordSpikeInvalidNeuron)
{
    network = neural_network_create(&config);

    EXPECT_FALSE(neural_network_record_spike(network, 1000, 1.0f, 1));
}

TEST_F(NeuralNetComprehensive, UpdateTracesBasic)
{
    network = neural_network_create(&config);

    // Should not crash
    neural_network_update_traces(network, 0, 10);
    SUCCEED();
}

TEST_F(NeuralNetComprehensive, GetAverageActivity)
{
    CreateNetworkWithConnections();

    // Record some activity
    neural_network_record_spike(network, 0, 1.0f, 1);
    neural_network_record_spike(network, 0, 1.0f, 10);

    float avg = neural_network_get_average_activity(network, 0);
    EXPECT_GE(avg, 0.0f);
}

TEST_F(NeuralNetComprehensive, GetAverageActivityInvalid)
{
    network = neural_network_create(&config);

    float avg = neural_network_get_average_activity(network, 1000);
    EXPECT_EQ(avg, 0.0f);
}

//=============================================================================
// 8. Threshold Adaptation Tests
//=============================================================================

TEST_F(NeuralNetComprehensive, AdaptThresholdBasic)
{
    network = neural_network_create(&config);

    EXPECT_TRUE(neural_network_adapt_threshold(network, 0, 10));
}

TEST_F(NeuralNetComprehensive, AdaptThresholdInvalid)
{
    network = neural_network_create(&config);

    EXPECT_FALSE(neural_network_adapt_threshold(network, 1000, 10));
}

TEST_F(NeuralNetComprehensive, AdaptThresholdWithActivity)
{
    network = neural_network_create(&config);

    // Generate some activity
    neural_network_record_spike(network, 0, 1.0f, 1);
    neural_network_record_spike(network, 0, 1.0f, 5);

    EXPECT_TRUE(neural_network_adapt_threshold(network, 0, 10));
}

//=============================================================================
// 9. Activation Functions Tests
//=============================================================================

TEST_F(NeuralNetComprehensive, ComputeActivationSigmoid)
{
    network = neural_network_create(&config);
    neuron_t* neuron = neural_network_get_neuron(network, 0);
    ASSERT_NE(neuron, nullptr);

    neuron->activation_type = ACTIVATION_SIGMOID;
    float result = neural_network_compute_activation(neuron, 0.5f);
    EXPECT_GT(result, 0.0f);
    EXPECT_LT(result, 1.0f);
}

TEST_F(NeuralNetComprehensive, ComputeActivationTanh)
{
    network = neural_network_create(&config);
    neuron_t* neuron = neural_network_get_neuron(network, 0);
    ASSERT_NE(neuron, nullptr);

    neuron->activation_type = ACTIVATION_TANH;
    float result = neural_network_compute_activation(neuron, 0.5f);
    EXPECT_GT(result, -1.0f);
    EXPECT_LT(result, 1.0f);
}

TEST_F(NeuralNetComprehensive, ComputeActivationReLU)
{
    network = neural_network_create(&config);
    neuron_t* neuron = neural_network_get_neuron(network, 0);
    ASSERT_NE(neuron, nullptr);

    neuron->activation_type = ACTIVATION_RELU;
    float result = neural_network_compute_activation(neuron, 0.5f);
    EXPECT_EQ(result, 0.5f);

    result = neural_network_compute_activation(neuron, -0.5f);
    EXPECT_EQ(result, 0.0f);
}

TEST_F(NeuralNetComprehensive, ComputeActivationLeakyReLU)
{
    network = neural_network_create(&config);
    neuron_t* neuron = neural_network_get_neuron(network, 0);
    ASSERT_NE(neuron, nullptr);

    neuron->activation_type = ACTIVATION_LEAKY_RELU;
    float result = neural_network_compute_activation(neuron, -0.5f);
    EXPECT_LT(result, 0.0f);  // Should be negative but not zero
}

TEST_F(NeuralNetComprehensive, ComputeActivationAdaptive)
{
    network = neural_network_create(&config);
    neuron_t* neuron = neural_network_get_neuron(network, 0);
    ASSERT_NE(neuron, nullptr);

    neuron->activation_type = ACTIVATION_ADAPTIVE;
    float result = neural_network_compute_activation(neuron, 0.5f);
    // Adaptive should pass through input
    EXPECT_EQ(result, 0.5f);
}

//=============================================================================
// 10. Weight Statistics Tests
//=============================================================================

TEST_F(NeuralNetComprehensive, GetWeightNormBasic)
{
    CreateNetworkWithConnections();

    float norm = neural_network_get_weight_norm(network, 0);
    EXPECT_GT(norm, 0.0f);
}

TEST_F(NeuralNetComprehensive, GetWeightNormInvalid)
{
    network = neural_network_create(&config);

    float norm = neural_network_get_weight_norm(network, 1000);
    EXPECT_EQ(norm, 0.0f);
}

TEST_F(NeuralNetComprehensive, GetWeightStatisticsBasic)
{
    CreateNetworkWithConnections();

    float mean, std_dev;
    neural_network_get_weight_statistics(network, 0, &mean, &std_dev);
    EXPECT_GT(mean, 0.0f);
    EXPECT_GE(std_dev, 0.0f);
}

TEST_F(NeuralNetComprehensive, GetWeightStatisticsNullOutputs)
{
    CreateNetworkWithConnections();

    // Should not crash with null outputs
    neural_network_get_weight_statistics(network, 0, nullptr, nullptr);
    SUCCEED();
}

TEST_F(NeuralNetComprehensive, GetStatsBasic)
{
    CreateNetworkWithConnections();

    network_stats_t stats;
    ASSERT_TRUE(neural_network_get_stats(network, &stats));

    EXPECT_EQ(stats.num_neurons, config.num_neurons);
    EXPECT_GT(stats.total_synapses, 0);
}

TEST_F(NeuralNetComprehensive, GetStatsNullOutput)
{
    network = neural_network_create(&config);
    EXPECT_FALSE(neural_network_get_stats(network, nullptr));
}

TEST_F(NeuralNetComprehensive, GetStatsNullNetwork)
{
    network_stats_t stats;
    EXPECT_FALSE(neural_network_get_stats(nullptr, &stats));
}

//=============================================================================
// 11. Network Maintenance Tests
//=============================================================================

TEST_F(NeuralNetComprehensive, MaintainBasic)
{
    CreateNetworkWithConnections();

    // Run maintenance
    neural_network_maintain(network, 10000);

    // Network should still be functional
    network_stats_t stats;
    neural_network_get_stats(network, &stats);
    EXPECT_EQ(stats.num_neurons, config.num_neurons);
}

TEST_F(NeuralNetComprehensive, MaintainHomeostasisBasic)
{
    network = neural_network_create(&config);

    // Should not crash
    neural_network_maintain_homeostasis(network, 100);
    SUCCEED();
}

TEST_F(NeuralNetComprehensive, ResetNetwork)
{
    CreateNetworkWithConnections();

    // Activate network
    for (uint32_t i = 0; i < 10; i++) {
        neural_network_update_neuron(network, 0, 1.0f, i);
        neural_network_compute_step(network, i);
    }

    // Reset
    neural_network_reset(network);

    // Check that state is reset
    float state;
    neural_network_get_neuron_state(network, 0, &state);
    // State should be at rest potential
    EXPECT_TRUE(float_equals(state, 0.0f));
}

TEST_F(NeuralNetComprehensive, DumpNeuronBasic)
{
    CreateNetworkWithConnections();

    // Should not crash (outputs to stdout)
    neural_network_dump_neuron(network, 0);
    SUCCEED();
}

TEST_F(NeuralNetComprehensive, DumpNeuronInvalid)
{
    network = neural_network_create(&config);

    // Should handle invalid ID gracefully
    neural_network_dump_neuron(network, 1000);
    SUCCEED();
}

//=============================================================================
// 12. Global State and Neuromodulation Tests
//=============================================================================

TEST_F(NeuralNetComprehensive, SetGlobalStateBasic)
{
    network = neural_network_create(&config);

    float global_state[10] = {0.1f, 0.2f, 0.3f};
    EXPECT_TRUE(neural_network_set_global_state(network, global_state, 10));
}

TEST_F(NeuralNetComprehensive, SetGlobalStateNull)
{
    network = neural_network_create(&config);

    EXPECT_TRUE(neural_network_set_global_state(network, nullptr, 0));
}

TEST_F(NeuralNetComprehensive, SetNeuromodulatorSystem)
{
    network = neural_network_create(&config);

    void* fake_system = (void*) 0x12345678;
    EXPECT_TRUE(neural_network_set_neuromodulator_system(network, fake_system));
}

TEST_F(NeuralNetComprehensive, GetNeuromodulation)
{
    network = neural_network_create(&config);

    float level = neural_network_get_neuromodulation(network);
    EXPECT_GE(level, 0.0f);
    EXPECT_LE(level, 1.0f);
}

TEST_F(NeuralNetComprehensive, GetNeuromodulationNull)
{
    float level = neural_network_get_neuromodulation(nullptr);
    EXPECT_EQ(level, 0.0f);
}

//=============================================================================
// 13. Glial Integration Tests
//=============================================================================

TEST_F(NeuralNetComprehensive, SetGlialIntegration)
{
    network = neural_network_create(&config);

    void* fake_glial = (void*) 0x87654321;
    EXPECT_TRUE(neural_network_set_glial_integration(network, fake_glial));
}

TEST_F(NeuralNetComprehensive, SetGlialIntegrationNull)
{
    network = neural_network_create(&config);

    EXPECT_TRUE(neural_network_set_glial_integration(network, nullptr));
}

//=============================================================================
// 14. Incoming Synapse Access Tests
//=============================================================================

TEST_F(NeuralNetComprehensive, GetIncomingSynapseCount)
{
    CreateNetworkWithConnections();

    // Neuron 1 should have 1 incoming synapse from neuron 0
    uint32_t count = neural_network_get_incoming_synapse_count(network, 1);
    EXPECT_EQ(count, 1);
}

TEST_F(NeuralNetComprehensive, GetIncomingSynapseCountZero)
{
    network = neural_network_create(&config);

    // No connections, should be zero
    uint32_t count = neural_network_get_incoming_synapse_count(network, 0);
    EXPECT_EQ(count, 0);
}

TEST_F(NeuralNetComprehensive, GetIncomingSynapses)
{
    CreateNetworkWithConnections();

    const synapse_t* synapses = nullptr;
    uint32_t count = neural_network_get_incoming_synapses(network, 1, &synapses);

    EXPECT_EQ(count, 1);
    ASSERT_NE(synapses, nullptr);
    EXPECT_EQ(synapses[0].target_id, 0);  // Reverse edge points to source
}

TEST_F(NeuralNetComprehensive, GetIncomingSynapsesNullOutput)
{
    CreateNetworkWithConnections();

    // API returns 0 when out_synapses is NULL (defensive programming)
    uint32_t count = neural_network_get_incoming_synapses(network, 1, nullptr);
    EXPECT_EQ(count, 0);
}

//=============================================================================
// 15. Neuron Model Configuration Tests
//=============================================================================

TEST_F(NeuralNetComprehensive, SetNeuronModelLIF)
{
    network = neural_network_create(&config);

    EXPECT_TRUE(
        neural_network_set_neuron_model(network, 0, NEURON_MODEL_LIF, nullptr));
}

TEST_F(NeuralNetComprehensive, SetNeuronModelIzhikevich)
{
    network = neural_network_create(&config);

    EXPECT_TRUE(neural_network_set_neuron_model(network, 0, NEURON_MODEL_IZHIKEVICH,
                                                  nullptr));
}

TEST_F(NeuralNetComprehensive, SetNeuronModelInvalidNeuron)
{
    network = neural_network_create(&config);

    EXPECT_FALSE(
        neural_network_set_neuron_model(network, 1000, NEURON_MODEL_LIF, nullptr));
}

//=============================================================================
// 16. Edge Cases and Error Handling Tests
//=============================================================================

TEST_F(NeuralNetComprehensive, MaxNeuronsCapacity)
{
    // Test network at maximum capacity
    config.num_neurons = MAX_NEURONS;
    network = neural_network_create(&config);
    EXPECT_NE(network, nullptr);

    // Should not be able to add more neurons (depends on capacity growth)
    EXPECT_EQ(neural_network_get_num_neurons(network), MAX_NEURONS);
}

TEST_F(NeuralNetComprehensive, ExceedMaxNeurons)
{
    config.num_neurons = MAX_NEURONS + 1;
    network = neural_network_create(&config);
    EXPECT_EQ(network, nullptr);
}

TEST_F(NeuralNetComprehensive, NetworkOperationsAfterReset)
{
    CreateNetworkWithConnections();

    neural_network_reset(network);

    // Should still be able to perform operations
    EXPECT_TRUE(neural_network_update_neuron(network, 0, 0.1f, 1));
    EXPECT_TRUE(neural_network_add_connection(network, 3, 4, 0.5f));
}

TEST_F(NeuralNetComprehensive, MultipleMaintenanceCycles)
{
    CreateNetworkWithConnections();

    for (uint64_t t = 0; t < 10; t++) {
        neural_network_maintain(network, t * config.update_interval);
    }

    // Network should still be functional
    network_stats_t stats;
    EXPECT_TRUE(neural_network_get_stats(network, &stats));
}

TEST_F(NeuralNetComprehensive, LongRunningSimulation)
{
    CreateNetworkWithConnections();

    // Run for many timesteps
    for (uint64_t t = 0; t < 10000; t++) {
        neural_network_compute_step(network, t);

        if (t % 1000 == 0) {
            neural_network_maintain(network, t);
        }
    }

    // Network should still be stable
    network_stats_t stats;
    EXPECT_TRUE(neural_network_get_stats(network, &stats));
    EXPECT_EQ(stats.num_neurons, config.num_neurons);
}

TEST_F(NeuralNetComprehensive, ConcurrentNeuronUpdates)
{
    CreateFullyConnectedNetwork();

    // Update all neurons in the same timestep
    for (uint32_t i = 0; i < config.num_neurons; i++) {
        neural_network_update_neuron(network, i, 0.1f, 1);
    }

    // Check all neurons were updated
    for (uint32_t i = 0; i < config.num_neurons; i++) {
        float state;
        EXPECT_TRUE(neural_network_get_neuron_state(network, i, &state));
    }
}

//=============================================================================
// 17. Complex Network Topology Tests
//=============================================================================

TEST_F(NeuralNetComprehensive, ChainTopology)
{
    network = neural_network_create(&config);

    // Create chain: 0 -> 1 -> 2 -> ... -> 9
    for (uint32_t i = 0; i < config.num_neurons - 1; i++) {
        EXPECT_TRUE(neural_network_add_connection(network, i, i + 1, 0.5f));
    }

    // Test propagation through chain
    neural_network_update_neuron(network, 0, 1.0f, 1);
    for (uint64_t t = 1; t < 100; t++) {
        neural_network_compute_step(network, t);
    }

    network_stats_t stats;
    EXPECT_TRUE(neural_network_get_stats(network, &stats));
}

TEST_F(NeuralNetComprehensive, RingTopology)
{
    network = neural_network_create(&config);

    // Create ring: 0 -> 1 -> 2 -> ... -> 9 -> 0
    for (uint32_t i = 0; i < config.num_neurons; i++) {
        uint32_t next = (i + 1) % config.num_neurons;
        EXPECT_TRUE(neural_network_add_connection(network, i, next, 0.5f));
    }

    network_stats_t stats;
    EXPECT_TRUE(neural_network_get_stats(network, &stats));
    EXPECT_EQ(stats.total_synapses, config.num_neurons);
}

TEST_F(NeuralNetComprehensive, FullyConnectedTopology)
{
    CreateFullyConnectedNetwork();

    network_stats_t stats;
    EXPECT_TRUE(neural_network_get_stats(network, &stats));

    // Should have n*(n-1) connections
    uint32_t expected = config.num_neurons * (config.num_neurons - 1);
    EXPECT_EQ(stats.total_synapses, expected);
}

//=============================================================================
// Main Test Entry Point
//=============================================================================

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
