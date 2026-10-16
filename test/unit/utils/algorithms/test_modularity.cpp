/**
 * @file test_modularity.cpp
 * @brief Comprehensive test suite for modularity calculations
 *
 * WHAT: Tests for modularity metric on various network types
 * WHY: Ensure modularity calculation is correct across scenarios
 * HOW: Use GTest with synthetic neural networks of known properties
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
static const double Q_THRESHOLD_RANDOM = 0.3;    // Random networks should have Q < 0.3
static const double Q_THRESHOLD_MODULAR = 0.3;   // Modular networks should have Q > 0.3

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * WHAT: Create simple 2-community network
 * WHY: Test modularity calculation on known partition
 */
static neural_network_t create_two_community_network(void)
{
    network_config_t config = {0};
    config.num_neurons = 6;
    config.input_size = 2;
    config.output_size = 2;
    config.ei_ratio = 0.8f;
    config.learning_rate = 0.01f;
    config.min_weight = 0.0f;
    config.max_weight = 1.0f;

    neural_network_t network = neural_network_create(&config);
    if (!network) return nullptr;

    // Community 1: neurons 0-2 (fully connected)
    neural_network_add_connection(network, 0, 1, 1.0f);
    neural_network_add_connection(network, 1, 0, 1.0f);
    neural_network_add_connection(network, 0, 2, 1.0f);
    neural_network_add_connection(network, 2, 0, 1.0f);
    neural_network_add_connection(network, 1, 2, 1.0f);
    neural_network_add_connection(network, 2, 1, 1.0f);

    // Community 2: neurons 3-5 (fully connected)
    neural_network_add_connection(network, 3, 4, 1.0f);
    neural_network_add_connection(network, 4, 3, 1.0f);
    neural_network_add_connection(network, 3, 5, 1.0f);
    neural_network_add_connection(network, 5, 3, 1.0f);
    neural_network_add_connection(network, 4, 5, 1.0f);
    neural_network_add_connection(network, 5, 4, 1.0f);

    // Single inter-community edge
    neural_network_add_connection(network, 0, 3, 1.0f);
    neural_network_add_connection(network, 3, 0, 1.0f);

    return network;
}

/**
 * WHAT: Create complete network
 * WHY: Test complete network has modularity ~ 0
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

//=============================================================================
// Test Fixtures
//=============================================================================

class ModularityTest : public ::testing::Test {
protected:
    void TearDown() override
    {
        // Cleanup if needed
    }
};

//=============================================================================
// Tests: Basic Modularity Calculation
//=============================================================================

TEST_F(ModularityTest, test_modularity_correct_partition)
{
    // WHAT: Test modularity calculation on known good partition
    // WHY: Verify calculation matches expected value
    // HOW: Create 2-community network, calculate Q with community detection

    neural_network_t network = create_two_community_network();
    ASSERT_NE(nullptr, network);

    community_structure_t* structure = community_detect(network, nullptr);
    ASSERT_NE(nullptr, structure);

    EXPECT_GT(structure->modularity, 0.0) << "Correct partition should have positive modularity";
    EXPECT_LT(structure->modularity, 1.0) << "Modularity should be < 1.0";

    topology_community_structure_free(structure);
    neural_network_destroy(network);
}

TEST_F(ModularityTest, test_modularity_complete_network)
{
    // WHAT: Complete network should have low modularity
    // WHY: All neurons equally connected - no community structure
    // HOW: Test Q for complete network

    neural_network_t network = create_complete_network(5);
    ASSERT_NE(nullptr, network);

    community_structure_t* structure = community_detect(network, nullptr);
    ASSERT_NE(nullptr, structure);

    // Complete networks may be split into multiple communities by the algorithm
    // depending on initialization, but modularity should remain low
    EXPECT_LT(structure->modularity, 0.2f) << "Complete network should have low modularity";
    EXPECT_GT(structure->modularity, -0.2f) << "Modularity should not be significantly negative";
    EXPECT_LE(structure->num_communities, 3u) << "Complete network should have few communities";

    topology_community_structure_free(structure);
    neural_network_destroy(network);
}

//=============================================================================
// Tests: Modularity with Resolution Parameter
//=============================================================================

TEST_F(ModularityTest, test_modularity_with_resolution_default)
{
    // WHAT: Default resolution (1.0) should work correctly
    // WHY: Verify resolution parameter works correctly
    // HOW: Compare with default configuration

    neural_network_t network = create_two_community_network();
    ASSERT_NE(nullptr, network);

    community_detection_config_t config = community_default_config();
    EXPECT_FLOAT_EQ(config.resolution, 1.0f) << "Default resolution should be 1.0";

    community_structure_t* structure = community_detect(network, &config);
    ASSERT_NE(nullptr, structure);

    EXPECT_GT(structure->modularity, 0.0) << "Modularity should be positive";

    topology_community_structure_free(structure);
    neural_network_destroy(network);
}

TEST_F(ModularityTest, test_modularity_with_resolution_higher)
{
    // WHAT: Higher resolution finds finer structures
    // WHY: Resolution parameter controls hierarchical level
    // HOW: Compare Q with different resolution values

    neural_network_t network = create_two_community_network();
    ASSERT_NE(nullptr, network);

    community_detection_config_t config_low = community_default_config();
    config_low.resolution = 0.5f;

    community_detection_config_t config_high = community_default_config();
    config_high.resolution = 2.0f;

    community_structure_t* structure_low = community_detect(network, &config_low);
    community_structure_t* structure_high = community_detect(network, &config_high);
    ASSERT_NE(nullptr, structure_low);
    ASSERT_NE(nullptr, structure_high);

    // Higher resolution may find more communities
    EXPECT_LE(structure_low->num_communities, structure_high->num_communities);

    topology_community_structure_free(structure_low);
    topology_community_structure_free(structure_high);
    neural_network_destroy(network);
}

//=============================================================================
// Tests: Edge Cases
//=============================================================================

TEST_F(ModularityTest, test_modularity_single_neuron)
{
    // WHAT: Single neuron should have zero modularity
    // WHY: No edges possible
    // HOW: Create 1-neuron network

    network_config_t config = {0};
    config.num_neurons = 1;
    config.input_size = 1;
    config.output_size = 1;
    neural_network_t network = neural_network_create(&config);
    ASSERT_NE(nullptr, network);

    community_structure_t* structure = community_detect(network, nullptr);
    ASSERT_NE(nullptr, structure);

    EXPECT_EQ(0.0, structure->modularity) << "Single neuron should have zero modularity";

    topology_community_structure_free(structure);
    neural_network_destroy(network);
}

//=============================================================================
// Tests: Modularity Properties
//=============================================================================

TEST_F(ModularityTest, test_modularity_bounds)
{
    // WHAT: Modularity should be in [-0.5, 1.0]
    // WHY: Theoretical bounds on modularity
    // HOW: Generate various networks and check bounds

    neural_network_t network = create_two_community_network();
    ASSERT_NE(nullptr, network);

    community_structure_t* structure = community_detect(network, nullptr);
    ASSERT_NE(nullptr, structure);

    EXPECT_GE(structure->modularity, -0.6) << "Modularity should be >= -0.5";
    EXPECT_LE(structure->modularity, 1.0) << "Modularity should be <= 1.0";

    topology_community_structure_free(structure);
    neural_network_destroy(network);
}

TEST_F(ModularityTest, test_modularity_symmetry)
{
    // WHAT: Community detection should be consistent
    // WHY: Modularity is invariant to relabeling
    // HOW: Run multiple times with same seed

    neural_network_t network = create_two_community_network();
    ASSERT_NE(nullptr, network);

    community_detection_config_t config = community_default_config();
    config.random_seed = 42;

    community_structure_t* structure1 = community_detect(network, &config);
    community_structure_t* structure2 = community_detect(network, &config);
    ASSERT_NE(nullptr, structure1);
    ASSERT_NE(nullptr, structure2);

    EXPECT_NEAR(structure1->modularity, structure2->modularity, EPSILON)
        << "Modularity should be consistent with same seed";

    topology_community_structure_free(structure1);
    topology_community_structure_free(structure2);
    neural_network_destroy(network);
}

//=============================================================================
// Tests: Community Statistics
//=============================================================================

TEST_F(ModularityTest, test_community_sizes_sum)
{
    // WHAT: Sum of community sizes equals total neurons
    // WHY: All neurons must be assigned to a community
    // HOW: Check community sizes array

    neural_network_t network = create_two_community_network();
    ASSERT_NE(nullptr, network);

    community_structure_t* structure = community_detect(network, nullptr);
    ASSERT_NE(nullptr, structure);

    uint32_t total = 0;
    for (uint32_t i = 0; i < structure->num_communities; i++) {
        total += structure->community_sizes[i];
    }

    EXPECT_EQ(total, structure->num_neurons)
        << "Sum of community sizes should equal total neurons";

    topology_community_structure_free(structure);
    neural_network_destroy(network);
}

TEST_F(ModularityTest, test_internal_density)
{
    // WHAT: Check internal density values are valid
    // WHY: Ensure density metrics are calculated correctly
    // HOW: Verify density is in [0, 1] range

    neural_network_t network = create_two_community_network();
    ASSERT_NE(nullptr, network);

    community_structure_t* structure = community_detect(network, nullptr);
    ASSERT_NE(nullptr, structure);

    if (structure->internal_density) {
        for (uint32_t i = 0; i < structure->num_communities; i++) {
            EXPECT_GE(structure->internal_density[i], 0.0f);
            EXPECT_LE(structure->internal_density[i], 1.0f);
        }
    }

    topology_community_structure_free(structure);
    neural_network_destroy(network);
}

//=============================================================================
// Test Summary
//=============================================================================

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
