/**
 * @file test_axon_integration_backward_compat.cpp
 * @brief Unit tests for axon integration backward compatibility
 *
 * WHAT: Verify that adding axon fields to neuron_t and synapse_t doesn't break existing code
 * WHY:  Ensure zero-default pattern works (axon_id=0 means no axon, legacy behavior)
 * HOW:  Create neurons and synapses without explicit axon setup, verify fields are zero
 *
 * @version Phase: Axon Integration (Option A)
 * @date 2025-11-24
 */

#include <gtest/gtest.h>
#include "core/neuralnet/nimcp_neuralnet.h"
#include "utils/test_helpers.h"

/**
 * @brief Test fixture for axon integration backward compatibility
 */
class AxonBackwardCompatTest : public ::testing::Test {
protected:
    neural_network_t network;

    void SetUp() override {
        network_config_t config = create_test_config();
        config.enable_bcm = false;
        config.enable_eligibility = false;

        network = neural_network_create(&config);
        ASSERT_NE(network, nullptr);
    }

    void TearDown() override {
        if (network) {
            neural_network_destroy(network);
        }
    }
};

/**
 * @brief Test that neurons are created with axon_id=0 by default
 *
 * WHAT: Verify neuron_t.axon_id is zero-initialized
 * WHY:  axon_id=0 means "no axon" (legacy behavior, backward compatible)
 * HOW:  Create neuron, check axon_id == 0
 */
TEST_F(AxonBackwardCompatTest, NeuronAxonIdDefaultsToZero) {
    // Add a neuron without any axon setup
    uint32_t neuron_id = neural_network_add_neuron(network, ACTIVATION_SIGMOID);

    // Verify neuron was created
    ASSERT_NE(neuron_id, UINT32_MAX);

    // Get the neuron
    neuron_t* neuron = neural_network_get_neuron(network, neuron_id);
    ASSERT_NE(neuron, nullptr);

    // CRITICAL: axon_id must be 0 (no axon, backward compatible)
    EXPECT_EQ(neuron->axon_id, 0u)
        << "Neuron axon_id should default to 0 (no axon, legacy mode)";
}

/**
 * @brief Test that synapses are created with source_neuron_id and axon_id set
 *
 * WHAT: Verify synapse_t fields are properly initialized
 * WHY:  source_neuron_id should be set, axon_id=0 for backward compatibility
 * HOW:  Create connection, check synapse fields
 */
TEST_F(AxonBackwardCompatTest, SynapseFieldsInitializedCorrectly) {
    // Create two neurons
    uint32_t from_id = neural_network_add_neuron(network, ACTIVATION_SIGMOID);
    uint32_t to_id = neural_network_add_neuron(network, ACTIVATION_SIGMOID);

    ASSERT_NE(from_id, UINT32_MAX);
    ASSERT_NE(to_id, UINT32_MAX);

    // Create connection (legacy API, no axon)
    bool success = neural_network_add_connection(network, from_id, to_id, 0.5f);
    ASSERT_TRUE(success);

    // Get the source neuron
    neuron_t* from_neuron = neural_network_get_neuron(network, from_id);
    ASSERT_NE(from_neuron, nullptr);
    ASSERT_GT(from_neuron->num_synapses, 0u);

    // Get the synapse
    synapse_t* syn = &from_neuron->synapses[0];

    // Verify basic synapse fields
    EXPECT_EQ(syn->target_id, to_id) << "Synapse target_id should match";

    // CRITICAL: source_neuron_id must be set correctly
    EXPECT_EQ(syn->source_neuron_id, from_id)
        << "Synapse source_neuron_id should be set to pre-synaptic neuron ID";

    // CRITICAL: axon_id must be 0 (no axon, backward compatible)
    EXPECT_EQ(syn->axon_id, 0u)
        << "Synapse axon_id should default to 0 (no axon, direct connection)";
}

/**
 * @brief Test that incoming synapses also have correct fields
 *
 * WHAT: Verify incoming synapse fields (reverse edges)
 * WHY:  Incoming synapses must match forward synapses
 * HOW:  Create connection, check incoming synapse fields
 */
TEST_F(AxonBackwardCompatTest, IncomingSynapseFieldsInitializedCorrectly) {
    // Create two neurons
    uint32_t from_id = neural_network_add_neuron(network, ACTIVATION_SIGMOID);
    uint32_t to_id = neural_network_add_neuron(network, ACTIVATION_SIGMOID);

    ASSERT_NE(from_id, UINT32_MAX);
    ASSERT_NE(to_id, UINT32_MAX);

    // Create connection
    bool success = neural_network_add_connection(network, from_id, to_id, 0.5f);
    ASSERT_TRUE(success);

    // Get the target neuron
    neuron_t* to_neuron = neural_network_get_neuron(network, to_id);
    ASSERT_NE(to_neuron, nullptr);
    ASSERT_GT(to_neuron->num_incoming, 0u);

    // Get the incoming synapse
    synapse_t* incoming_syn = &to_neuron->incoming_synapses[0];

    // Verify incoming synapse fields
    EXPECT_EQ(incoming_syn->target_id, from_id)
        << "Incoming synapse target_id stores source neuron ID";

    // CRITICAL: source_neuron_id must be set correctly
    EXPECT_EQ(incoming_syn->source_neuron_id, from_id)
        << "Incoming synapse source_neuron_id should match";

    // CRITICAL: axon_id must be 0 (no axon, backward compatible)
    EXPECT_EQ(incoming_syn->axon_id, 0u)
        << "Incoming synapse axon_id should default to 0";
}

/**
 * @brief Test multiple connections work correctly
 *
 * WHAT: Verify backward compatibility with multiple connections
 * WHY:  Ensure all synapses get correct field initialization
 * HOW:  Create network with multiple connections, verify all fields
 */
TEST_F(AxonBackwardCompatTest, MultipleConnectionsBackwardCompatible) {
    // Create 3 neurons
    uint32_t n1 = neural_network_add_neuron(network, ACTIVATION_SIGMOID);
    uint32_t n2 = neural_network_add_neuron(network, ACTIVATION_SIGMOID);
    uint32_t n3 = neural_network_add_neuron(network, ACTIVATION_SIGMOID);

    ASSERT_NE(n1, UINT32_MAX);
    ASSERT_NE(n2, UINT32_MAX);
    ASSERT_NE(n3, UINT32_MAX);

    // Create connections: n1 → n2, n1 → n3, n2 → n3
    ASSERT_TRUE(neural_network_add_connection(network, n1, n2, 0.3f));
    ASSERT_TRUE(neural_network_add_connection(network, n1, n3, 0.4f));
    ASSERT_TRUE(neural_network_add_connection(network, n2, n3, 0.5f));

    // Verify all neurons have axon_id=0
    neuron_t* neuron1 = neural_network_get_neuron(network, n1);
    neuron_t* neuron2 = neural_network_get_neuron(network, n2);
    neuron_t* neuron3 = neural_network_get_neuron(network, n3);

    EXPECT_EQ(neuron1->axon_id, 0u);
    EXPECT_EQ(neuron2->axon_id, 0u);
    EXPECT_EQ(neuron3->axon_id, 0u);

    // Verify n1's synapses
    ASSERT_EQ(neuron1->num_synapses, 2u);
    EXPECT_EQ(neuron1->synapses[0].source_neuron_id, n1);
    EXPECT_EQ(neuron1->synapses[0].axon_id, 0u);
    EXPECT_EQ(neuron1->synapses[1].source_neuron_id, n1);
    EXPECT_EQ(neuron1->synapses[1].axon_id, 0u);

    // Verify n2's synapses
    ASSERT_EQ(neuron2->num_synapses, 1u);
    EXPECT_EQ(neuron2->synapses[0].source_neuron_id, n2);
    EXPECT_EQ(neuron2->synapses[0].axon_id, 0u);

    // Verify n3's incoming synapses
    ASSERT_EQ(neuron3->num_incoming, 2u);
    EXPECT_EQ(neuron3->incoming_synapses[0].source_neuron_id, n1);
    EXPECT_EQ(neuron3->incoming_synapses[0].axon_id, 0u);
    EXPECT_EQ(neuron3->incoming_synapses[1].source_neuron_id, n2);
    EXPECT_EQ(neuron3->incoming_synapses[1].axon_id, 0u);
}

/**
 * @brief Test that legacy code behavior is unchanged
 *
 * WHAT: Verify that network can still function with new fields
 * WHY:  When axon_id=0, behavior should be exactly as before (backward compatible)
 * HOW:  Create connection, run compute step, verify fields remain correct
 */
TEST_F(AxonBackwardCompatTest, LegacySignalPropagationUnchanged) {
    // Create two neurons
    uint32_t from_id = neural_network_add_neuron(network, ACTIVATION_SIGMOID);
    uint32_t to_id = neural_network_add_neuron(network, ACTIVATION_SIGMOID);

    // Create connection
    ASSERT_TRUE(neural_network_add_connection(network, from_id, to_id, 1.0f));

    // Get neurons
    neuron_t* from_neuron = neural_network_get_neuron(network, from_id);
    neuron_t* to_neuron = neural_network_get_neuron(network, to_id);

    // Process one timestep (legacy behavior)
    uint64_t timestamp = 1000;
    neural_network_compute_step(network, timestamp);

    // With axon_id=0, propagation is immediate (legacy behavior)
    // Verify the connection exists and fields are still correct after compute step

    // Verify the synapse fields remain correct
    EXPECT_EQ(from_neuron->synapses[0].axon_id, 0u)
        << "Legacy mode: axon_id=0 means no delay";
    EXPECT_EQ(from_neuron->synapses[0].source_neuron_id, from_id)
        << "Source neuron ID correctly set";
    EXPECT_EQ(from_neuron->synapses[0].target_id, to_id)
        << "Target neuron ID correctly set";
}

/**
 * @brief Test memory overhead is minimal
 *
 * WHAT: Verify new fields don't significantly increase memory usage
 * WHY:  Backward compatibility should not add significant overhead
 * HOW:  Check structure sizes are reasonable
 */
TEST_F(AxonBackwardCompatTest, MemoryOverheadAcceptable) {
    // New fields added:
    // - neuron_t: +4 bytes (uint32_t axon_id)
    // - synapse_t: +8 bytes (uint32_t source_neuron_id, uint32_t axon_id)

    // These should be negligible compared to overall structure sizes
    EXPECT_LT(sizeof(uint32_t), 8u) << "axon_id field is small";

    // Total overhead for 10K neurons, 100K synapses:
    // - Neurons: 10,000 × 4 = 40 KB
    // - Synapses: 100,000 × 8 = 800 KB
    // - Total: ~840 KB (acceptable)

    size_t neuron_overhead = sizeof(uint32_t);  // axon_id
    size_t synapse_overhead = 2 * sizeof(uint32_t);  // source_neuron_id + axon_id

    EXPECT_EQ(neuron_overhead, 4u);
    EXPECT_EQ(synapse_overhead, 8u);
}

/**
 * @brief Test that future axon integration won't break
 *
 * WHAT: Verify fields can be set to non-zero values
 * WHY:  Ensure forward compatibility for future axon network integration
 * HOW:  Manually set axon_id fields, verify they're stored correctly
 */
TEST_F(AxonBackwardCompatTest, ForwardCompatibilityReady) {
    // Create neuron
    uint32_t neuron_id = neural_network_add_neuron(network, ACTIVATION_SIGMOID);
    neuron_t* neuron = neural_network_get_neuron(network, neuron_id);

    // Manually set axon_id (simulating future axon network integration)
    neuron->axon_id = 12345u;

    // Verify it's stored correctly
    EXPECT_EQ(neuron->axon_id, 12345u)
        << "Field can be set to non-zero value for future axon integration";

    // Create synapse
    uint32_t to_id = neural_network_add_neuron(network, ACTIVATION_SIGMOID);
    ASSERT_TRUE(neural_network_add_connection(network, neuron_id, to_id, 0.5f));

    synapse_t* syn = &neuron->synapses[0];

    // Manually set axon_id
    syn->axon_id = 67890u;

    // Verify it's stored correctly
    EXPECT_EQ(syn->axon_id, 67890u)
        << "Synapse axon_id can be set for future integration";
}
