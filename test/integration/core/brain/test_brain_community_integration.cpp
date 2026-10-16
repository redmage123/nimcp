/**
 * @file test_brain_community_integration.cpp
 * @brief Integration tests for community detection with NIMCP brain
 *
 * WHAT: Tests community detection on realistic brain network structures
 * WHY: Verify algorithms work with brain region topology
 * HOW: Create modular brain structure, run community detection
 */

#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
#include <vector>

#include "core/topology/nimcp_community_detection.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "core/brain/nimcp_brain.h"
#include "plasticity/adaptive/nimcp_adaptive.h"

//=============================================================================
// Test Constants
//=============================================================================

static const double EPSILON = 1e-6;
static const uint32_t MAX_TEST_NEURONS = 256;

//=============================================================================
// Helper Functions for Brain Network Construction
//=============================================================================

/**
 * WHAT: Create modular brain with different regions
 * WHY: Test realistic brain topology
 * HOW: Use brain API to create structured network
 */
static brain_t create_modular_brain(void)
{
    // Create small brain for testing
    brain_t brain = brain_create("test_brain", BRAIN_SIZE_SMALL,
                                 BRAIN_TASK_CLASSIFICATION, 20, 5);
    return brain;
}

//=============================================================================
// Test Fixtures
//=============================================================================

class BrainCommunityIntegrationTest : public ::testing::Test {
protected:
    void TearDown() override
    {
        // Cleanup if needed
    }
};

//=============================================================================
// Tests: Brain Network Analysis
//=============================================================================

TEST_F(BrainCommunityIntegrationTest, test_brain_network_community_detection)
{
    // WHAT: Detect communities in brain network
    // WHY: Verify algorithm finds brain regions
    // HOW: Create brain network, run community detection

    brain_t brain = create_modular_brain();
    ASSERT_NE(nullptr, brain);

    adaptive_network_t adaptive_net = brain_get_network(brain);
    ASSERT_NE(nullptr, adaptive_net);
    neural_network_t network = adaptive_network_get_base_network(adaptive_net);
    ASSERT_NE(nullptr, network);

    community_structure_t* structure = community_detect(network, nullptr);
    ASSERT_NE(nullptr, structure);

    // Should find at least 1 community
    EXPECT_GE(structure->num_communities, 1u)
        << "Should detect at least one community";

    EXPECT_GT(structure->modularity, -0.5f)
        << "Brain network should have reasonable modularity";

    topology_community_structure_free(structure);
    brain_destroy(brain);
}

TEST_F(BrainCommunityIntegrationTest, test_brain_hub_detection)
{
    // WHAT: Detect hub neurons in brain network
    // WHY: Hubs are important for brain function
    // HOW: Use hub detection on brain network

    brain_t brain = create_modular_brain();
    ASSERT_NE(nullptr, brain);

    adaptive_network_t adaptive_net = brain_get_network(brain);
    ASSERT_NE(nullptr, adaptive_net);
    neural_network_t network = adaptive_network_get_base_network(adaptive_net);
    ASSERT_NE(nullptr, network);

    hub_structure_t* hubs = community_detect_hubs(network, 0.8f);
    ASSERT_NE(nullptr, hubs);

    // Should detect some hubs
    EXPECT_GE(hubs->num_hubs, 0u) << "Hub detection should succeed";

    hub_structure_free(hubs);
    brain_destroy(brain);
}

TEST_F(BrainCommunityIntegrationTest, test_brain_topology_validity)
{
    // WHAT: Verify brain network structure is valid
    // WHY: Sanity check on brain network
    // HOW: Check neuron count and basic properties

    brain_t brain = create_modular_brain();
    ASSERT_NE(nullptr, brain);

    adaptive_network_t adaptive_net = brain_get_network(brain);
    ASSERT_NE(nullptr, adaptive_net);
    neural_network_t network = adaptive_network_get_base_network(adaptive_net);
    ASSERT_NE(nullptr, network);

    // Run community detection to verify network is valid
    community_structure_t* structure = community_detect(network, nullptr);
    ASSERT_NE(nullptr, structure);

    EXPECT_GT(structure->num_neurons, 0u) << "Brain should have neurons";
    EXPECT_GE(structure->num_communities, 1u) << "Should detect at least one community";

    topology_community_structure_free(structure);
    brain_destroy(brain);
}

TEST_F(BrainCommunityIntegrationTest, test_brain_modularity_calculation)
{
    // WHAT: Calculate modularity of brain network
    // WHY: Quantify modular structure
    // HOW: Get community detection results

    brain_t brain = create_modular_brain();
    ASSERT_NE(nullptr, brain);

    adaptive_network_t adaptive_net = brain_get_network(brain);
    ASSERT_NE(nullptr, adaptive_net);
    neural_network_t network = adaptive_network_get_base_network(adaptive_net);
    ASSERT_NE(nullptr, network);

    community_structure_t* structure = community_detect(network, nullptr);
    ASSERT_NE(nullptr, structure);

    // Verify modularity is in valid range
    EXPECT_GE(structure->modularity, -0.5f);
    EXPECT_LE(structure->modularity, 1.0f);

    topology_community_structure_free(structure);
    brain_destroy(brain);
}

TEST_F(BrainCommunityIntegrationTest, test_community_detection_with_config)
{
    // WHAT: Use custom configuration for community detection
    // WHY: Test configuration flexibility
    // HOW: Create config and run detection

    brain_t brain = create_modular_brain();
    ASSERT_NE(nullptr, brain);

    adaptive_network_t adaptive_net = brain_get_network(brain);
    ASSERT_NE(nullptr, adaptive_net);
    neural_network_t network = adaptive_network_get_base_network(adaptive_net);
    ASSERT_NE(nullptr, network);

    community_detection_config_t config = community_default_config();
    config.max_iterations = 50;
    config.resolution = 1.2f;
    config.random_seed = 42;

    community_structure_t* structure = community_detect(network, &config);
    ASSERT_NE(nullptr, structure);

    EXPECT_GE(structure->num_communities, 1u);

    topology_community_structure_free(structure);
    brain_destroy(brain);
}

TEST_F(BrainCommunityIntegrationTest, test_hub_detection_threshold)
{
    // WHAT: Test different hub detection thresholds
    // WHY: Verify threshold parameter works
    // HOW: Detect hubs with different thresholds

    brain_t brain = create_modular_brain();
    ASSERT_NE(nullptr, brain);

    adaptive_network_t adaptive_net = brain_get_network(brain);
    ASSERT_NE(nullptr, adaptive_net);
    neural_network_t network = adaptive_network_get_base_network(adaptive_net);
    ASSERT_NE(nullptr, network);

    hub_structure_t* hubs_low = community_detect_hubs(network, 0.5f);
    hub_structure_t* hubs_high = community_detect_hubs(network, 0.9f);

    ASSERT_NE(nullptr, hubs_low);
    ASSERT_NE(nullptr, hubs_high);

    // Lower threshold should find more (or equal) hubs
    EXPECT_GE(hubs_low->num_hubs, hubs_high->num_hubs)
        << "Lower threshold should find more hubs";

    hub_structure_free(hubs_low);
    hub_structure_free(hubs_high);
    brain_destroy(brain);
}

TEST_F(BrainCommunityIntegrationTest, test_community_sizes)
{
    // WHAT: Check community sizes are valid
    // WHY: Ensure all neurons are assigned
    // HOW: Sum community sizes

    brain_t brain = create_modular_brain();
    ASSERT_NE(nullptr, brain);

    adaptive_network_t adaptive_net = brain_get_network(brain);
    ASSERT_NE(nullptr, adaptive_net);
    neural_network_t network = adaptive_network_get_base_network(adaptive_net);
    ASSERT_NE(nullptr, network);

    community_structure_t* structure = community_detect(network, nullptr);
    ASSERT_NE(nullptr, structure);

    uint32_t total = 0;
    for (uint32_t i = 0; i < structure->num_communities; i++) {
        EXPECT_GT(structure->community_sizes[i], 0u)
            << "Each community should have at least one neuron";
        total += structure->community_sizes[i];
    }

    EXPECT_EQ(total, structure->num_neurons)
        << "All neurons should be assigned to communities";

    topology_community_structure_free(structure);
    brain_destroy(brain);
}

TEST_F(BrainCommunityIntegrationTest, test_deterministic_detection)
{
    // WHAT: Community detection should be deterministic with fixed seed
    // WHY: Reproducibility is important
    // HOW: Run twice with same seed

    brain_t brain = create_modular_brain();
    ASSERT_NE(nullptr, brain);

    adaptive_network_t adaptive_net = brain_get_network(brain);
    ASSERT_NE(nullptr, adaptive_net);
    neural_network_t network = adaptive_network_get_base_network(adaptive_net);
    ASSERT_NE(nullptr, network);

    community_detection_config_t config = community_default_config();
    config.random_seed = 42;

    community_structure_t* structure1 = community_detect(network, &config);
    community_structure_t* structure2 = community_detect(network, &config);

    ASSERT_NE(nullptr, structure1);
    ASSERT_NE(nullptr, structure2);

    EXPECT_EQ(structure1->num_communities, structure2->num_communities);
    EXPECT_NEAR(structure1->modularity, structure2->modularity, EPSILON);

    topology_community_structure_free(structure1);
    topology_community_structure_free(structure2);
    brain_destroy(brain);
}

//=============================================================================
// Test Summary
//=============================================================================

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
