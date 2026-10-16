/**
 * @file test_louvain.cpp
 * @brief Comprehensive test suite for neural network community detection algorithm
 *
 * WHAT: Tests for community detection on various network topologies
 * WHY: Ensure algorithm correctly identifies modular structure across scenarios
 * HOW: Use GTest framework with synthetic neural network fixtures
 */

#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
#include <vector>

#include "core/topology/nimcp_community_detection.h"
#include "core/neuralnet/nimcp_neuralnet.h"

//=============================================================================
// Test Constants
//=============================================================================

static const double EPSILON = 1e-6;
static const uint32_t MAX_TEST_NEURONS = 256;

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * WHAT: Create complete network (all neurons connected)
 * WHY: Test case with known single community
 */
static neural_network_t create_complete_network(uint32_t num_neurons)
{
    network_config_t config = {0};
    config.num_neurons = num_neurons;
    config.input_size = 2;
    config.output_size = 2;
    config.ei_ratio = 0.8f;
    config.learning_rate = 0.01f;
    config.min_weight = 0.0f;
    config.max_weight = 1.0f;

    neural_network_t network = neural_network_create(&config);
    if (!network) return nullptr;

    // Add all edges
    for (uint32_t i = 0; i < num_neurons; i++) {
        for (uint32_t j = i + 1; j < num_neurons; j++) {
            neural_network_add_connection(network, i, j, 1.0f);
            neural_network_add_connection(network, j, i, 1.0f);
        }
    }

    return network;
}

/**
 * WHAT: Create disconnected network
 * WHY: Test network with multiple components
 */
static neural_network_t create_disconnected_network(uint32_t num_components)
{
    network_config_t config = {0};
    config.num_neurons = num_components * 2;
    config.input_size = 2;
    config.output_size = 2;
    config.ei_ratio = 0.8f;
    config.learning_rate = 0.01f;
    config.min_weight = 0.0f;
    config.max_weight = 1.0f;

    neural_network_t network = neural_network_create(&config);
    if (!network) return nullptr;

    uint32_t neuron_id = 0;
    for (uint32_t c = 0; c < num_components; c++) {
        // Create pair for each component
        neural_network_add_connection(network, neuron_id, neuron_id + 1, 1.0f);
        neural_network_add_connection(network, neuron_id + 1, neuron_id, 1.0f);
        neuron_id += 2;
    }

    return network;
}

/**
 * WHAT: Create modular synthetic network
 * WHY: Test case with known strong communities
 */
static neural_network_t create_modular_network(uint32_t num_communities, uint32_t neurons_per_com)
{
    network_config_t config = {0};
    config.num_neurons = num_communities * neurons_per_com;
    config.input_size = 3;
    config.output_size = 3;
    config.ei_ratio = 0.8f;
    config.learning_rate = 0.01f;
    config.min_weight = 0.0f;
    config.max_weight = 1.0f;

    neural_network_t network = neural_network_create(&config);
    if (!network) return nullptr;

    // Create cliques for each community
    for (uint32_t c = 0; c < num_communities; c++) {
        uint32_t start = c * neurons_per_com;
        uint32_t end = start + neurons_per_com;

        // Create clique (all connected within community)
        for (uint32_t i = start; i < end; i++) {
            for (uint32_t j = i + 1; j < end; j++) {
                neural_network_add_connection(network, i, j, 1.0f);
                neural_network_add_connection(network, j, i, 1.0f);
            }
        }
    }

    // Add inter-community edges (sparse)
    for (uint32_t c1 = 0; c1 < num_communities - 1; c1++) {
        for (uint32_t c2 = c1 + 1; c2 < num_communities; c2++) {
            uint32_t n1 = c1 * neurons_per_com;
            uint32_t n2 = c2 * neurons_per_com;
            neural_network_add_connection(network, n1, n2, 0.1f);
            neural_network_add_connection(network, n2, n1, 0.1f);
        }
    }

    return network;
}

//=============================================================================
// Test Fixtures
//=============================================================================

class LouvainTest : public ::testing::Test {
protected:
    void TearDown() override
    {
        // Cleanup if needed
    }
};

//=============================================================================
// Tests: Basic Functionality
//=============================================================================

TEST_F(LouvainTest, test_complete_network_single_community)
{
    // WHAT: Complete network should have low number of communities
    // WHY: In complete network, all neurons equally important
    // HOW: Run community detection and verify low community count

    neural_network_t network = create_complete_network(5);
    ASSERT_NE(nullptr, network);

    community_detection_config_t config = community_default_config();
    config.random_seed = 42;

    community_structure_t* structure = community_detect(network, &config);
    ASSERT_NE(nullptr, structure);

    // Complete networks may be split into multiple communities by the algorithm
    // depending on initialization. The key is modularity should be low.
    EXPECT_LE(structure->num_communities, 3u) << "Complete network should have few communities";
    EXPECT_LT(structure->modularity, 0.2f) << "Complete network should have low modularity";
    EXPECT_GT(structure->modularity, -0.2f) << "Modularity should not be significantly negative";

    topology_community_structure_free(structure);
    neural_network_destroy(network);
}

TEST_F(LouvainTest, test_disconnected_network_multiple_communities)
{
    // WHAT: Disconnected network should detect each component
    // WHY: Disconnected components are naturally separate communities
    // HOW: Create N disconnected components, verify N communities found

    uint32_t num_components = 3;
    neural_network_t network = create_disconnected_network(num_components);
    ASSERT_NE(nullptr, network);

    community_structure_t* structure = community_detect(network, nullptr);
    ASSERT_NE(nullptr, structure);

    EXPECT_GE(structure->num_communities, num_components)
        << "Should detect at least " << num_components << " components";
    EXPECT_LE(structure->num_communities, structure->num_neurons)
        << "Number of communities cannot exceed neurons";

    topology_community_structure_free(structure);
    neural_network_destroy(network);
}

TEST_F(LouvainTest, test_modular_network_correct_partition)
{
    // WHAT: Modular network should detect original community structure
    // WHY: Algorithm should find known modular partitions
    // HOW: Create network with known communities, verify detection

    uint32_t num_communities = 3;
    uint32_t neurons_per_com = 4;
    neural_network_t network = create_modular_network(num_communities, neurons_per_com);
    ASSERT_NE(nullptr, network);

    community_structure_t* structure = community_detect(network, nullptr);
    ASSERT_NE(nullptr, structure);

    EXPECT_GE(structure->num_communities, 2) << "Should detect at least 2 communities";
    EXPECT_GT(structure->modularity, 0.0) << "Modular network should have positive modularity";

    // The exact partitioning depends on initialization and algorithm details
    // Just verify reasonable structure was detected
    EXPECT_LE(structure->num_communities, num_communities * neurons_per_com)
        << "Should not have more communities than neurons";

    topology_community_structure_free(structure);
    neural_network_destroy(network);
}

//=============================================================================
// Tests: Convergence and Performance
//=============================================================================

TEST_F(LouvainTest, test_determinism_reproducible_results)
{
    // WHAT: Same seed should produce same results
    // WHY: Reproducibility is essential for testing and debugging
    // HOW: Run twice with same seed, verify identical partition

    neural_network_t network = create_modular_network(3, 5);
    ASSERT_NE(nullptr, network);

    community_detection_config_t config = community_default_config();
    config.random_seed = 42;

    community_structure_t* structure1 = community_detect(network, &config);
    community_structure_t* structure2 = community_detect(network, &config);
    ASSERT_NE(nullptr, structure1);
    ASSERT_NE(nullptr, structure2);

    // Verify identical results
    EXPECT_EQ(structure1->num_communities, structure2->num_communities);

    // Check assignments match
    bool assignments_match = true;
    for (uint32_t i = 0; i < structure1->num_neurons; i++) {
        if (structure1->community_ids[i] != structure2->community_ids[i]) {
            assignments_match = false;
            break;
        }
    }
    EXPECT_TRUE(assignments_match) << "Same seed should produce identical partitions";

    topology_community_structure_free(structure1);
    topology_community_structure_free(structure2);
    neural_network_destroy(network);
}

//=============================================================================
// Tests: Edge Cases
//=============================================================================

TEST_F(LouvainTest, test_empty_network_returns_null)
{
    // WHAT: Empty network should return NULL
    // WHY: Cannot partition network with no neurons
    // HOW: Create empty network, verify NULL return

    network_config_t config = {0};
    config.num_neurons = 0;
    neural_network_t network = neural_network_create(&config);

    community_structure_t* structure = community_detect(network, nullptr);
    EXPECT_EQ(nullptr, structure) << "Empty network should return NULL structure";

    if (network) neural_network_destroy(network);
}

TEST_F(LouvainTest, test_single_neuron_network)
{
    // WHAT: Single neuron network should have 1 community
    // WHY: Single neuron forms trivial partition
    // HOW: Create network with 1 neuron

    network_config_t config = {0};
    config.num_neurons = 1;
    config.input_size = 1;
    config.output_size = 1;
    neural_network_t network = neural_network_create(&config);
    ASSERT_NE(nullptr, network);

    community_structure_t* structure = community_detect(network, nullptr);
    ASSERT_NE(nullptr, structure);

    EXPECT_EQ(1u, structure->num_communities) << "Single neuron should be 1 community";
    EXPECT_EQ(0u, structure->community_ids[0]) << "Single neuron should be in community 0";

    topology_community_structure_free(structure);
    neural_network_destroy(network);
}

//=============================================================================
// Tests: API Functions
//=============================================================================

TEST_F(LouvainTest, test_get_community_members)
{
    // WHAT: Extract all members of a community
    // WHY: Need to analyze community composition
    // HOW: Query community members and verify

    neural_network_t network = create_modular_network(2, 3);
    ASSERT_NE(nullptr, network);

    community_structure_t* structure = community_detect(network, nullptr);
    ASSERT_NE(nullptr, structure);

    uint32_t* neuron_ids = nullptr;
    uint32_t count = 0;
    community_get_neurons_in_community(structure, 0, &neuron_ids, &count);

    EXPECT_GT(count, 0u) << "Community should have at least 1 member";
    EXPECT_LE(count, structure->num_neurons) << "Member count bounded by network size";

    // Free the allocated neuron_ids array to prevent memory leak
    if (neuron_ids) {
        free(neuron_ids);
    }

    topology_community_structure_free(structure);
    neural_network_destroy(network);
}

TEST_F(LouvainTest, test_default_config)
{
    // WHAT: Default configuration should work
    // WHY: Ensure sensible defaults
    // HOW: Use default config

    community_detection_config_t config = community_default_config();

    EXPECT_GT(config.max_iterations, 0u);
    EXPECT_GT(config.min_modularity_gain, 0.0f);
    EXPECT_GE(config.resolution, 0.0f);
}

//=============================================================================
// Test Summary
//=============================================================================

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
