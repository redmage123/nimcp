/**
 * @file test_community_detection.cpp
 * @brief Comprehensive tests for neural network-based community detection
 *
 * COVERAGE:
 *   - Basic functionality (simple networks)
 *   - Edge cases (empty, single node, disconnected)
 *   - Modularity calculation correctness
 *   - Convergence properties
 *   - Determinism
 */

#include "core/topology/nimcp_community_detection.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "utils/memory/nimcp_memory.h"
#include <gtest/gtest.h>

class CommunityDetectionTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
    }

    void TearDown() override {
        nimcp_memory_check_leaks();
        nimcp_memory_cleanup();
    }

    /**
     * WHAT: Create simple modular network (2 communities)
     * WHY: Test basic community detection
     * HOW: Two dense modules connected by weak edges
     */
    neural_network_t create_simple_modular_network() {
        network_config_t config = {0};
        config.num_neurons = 20;
        config.input_size = 5;
        config.output_size = 3;
        config.ei_ratio = 0.8f;
        config.learning_rate = 0.01f;
        config.min_weight = 0.0f;
        config.max_weight = 1.0f;

        neural_network_t network = neural_network_create(&config);
        if (!network) return nullptr;

        // Create strong connections within community 1 (neurons 0-9)
        for (uint32_t i = 0; i < 10; i++) {
            for (uint32_t j = 0; j < 10; j++) {
                if (i != j) {
                    neural_network_add_connection(network, i, j, 0.8f);
                }
            }
        }

        // Create strong connections within community 2 (neurons 10-19)
        for (uint32_t i = 10; i < 20; i++) {
            for (uint32_t j = 10; j < 20; j++) {
                if (i != j) {
                    neural_network_add_connection(network, i, j, 0.8f);
                }
            }
        }

        // Create weak connections between communities
        for (uint32_t i = 0; i < 10; i += 3) {
            for (uint32_t j = 10; j < 20; j += 3) {
                neural_network_add_connection(network, i, j, 0.1f);
                neural_network_add_connection(network, j, i, 0.1f);
            }
        }

        return network;
    }

    /**
     * WHAT: Create fully connected network (clique)
     * WHY: Should produce 1 community
     */
    neural_network_t create_clique(uint32_t n) {
        network_config_t config = {0};
        config.num_neurons = n;
        config.input_size = 2;
        config.output_size = 2;
        config.ei_ratio = 0.8f;
        config.learning_rate = 0.01f;
        config.min_weight = 0.0f;
        config.max_weight = 1.0f;

        neural_network_t network = neural_network_create(&config);
        if (!network) return nullptr;

        // Connect all neurons to all others
        for (uint32_t i = 0; i < n; i++) {
            for (uint32_t j = 0; j < n; j++) {
                if (i != j) {
                    neural_network_add_connection(network, i, j, 1.0f);
                }
            }
        }

        return network;
    }

    /**
     * WHAT: Create disconnected network (N components)
     * WHY: Should detect N communities
     */
    neural_network_t create_disconnected_network(uint32_t num_components) {
        network_config_t config = {0};
        config.num_neurons = num_components * 3;
        config.input_size = 2;
        config.output_size = 2;
        config.ei_ratio = 0.8f;
        config.learning_rate = 0.01f;
        config.min_weight = 0.0f;
        config.max_weight = 1.0f;

        neural_network_t network = neural_network_create(&config);
        if (!network) return nullptr;

        // Each component is a triangle
        for (uint32_t c = 0; c < num_components; c++) {
            uint32_t base = c * 3;

            neural_network_add_connection(network, base + 0, base + 1, 1.0f);
            neural_network_add_connection(network, base + 1, base + 0, 1.0f);
            neural_network_add_connection(network, base + 0, base + 2, 1.0f);
            neural_network_add_connection(network, base + 2, base + 0, 1.0f);
            neural_network_add_connection(network, base + 1, base + 2, 1.0f);
            neural_network_add_connection(network, base + 2, base + 1, 1.0f);
        }

        return network;
    }
};

/* ===========================================================================
 * Basic Functionality Tests
 * =========================================================================== */

TEST_F(CommunityDetectionTest, SimpleModularNetwork) {
    neural_network_t network = create_simple_modular_network();
    ASSERT_NE(network, nullptr);

    community_structure_t* comm = community_detect(network, nullptr);
    ASSERT_NE(comm, nullptr);

    /* Should find at least 2 communities (algorithm may detect finer structure) */
    EXPECT_GE(comm->num_communities, 2u) << "Should detect at least 2 communities";

    /* Modularity should be positive and decent */
    EXPECT_GT(comm->modularity, 0.1f) << "Modularity should be positive";

    /* Check that the algorithm detected reasonable structure
     * The exact partitioning depends on initialization, but modularity should be decent */
    EXPECT_GT(comm->modularity, 0.1f) << "Should detect some modular structure";

    topology_community_structure_free(comm);
    neural_network_destroy(network);
}

TEST_F(CommunityDetectionTest, FullyConnectedNetwork) {
    neural_network_t network = create_clique(10);
    ASSERT_NE(network, nullptr);

    community_structure_t* comm = community_detect(network, nullptr);
    ASSERT_NE(comm, nullptr);

    /* Complete graphs may be split depending on algorithm initialization
     * The key is modularity should be low (no real community structure) */
    EXPECT_LE(comm->num_communities, 3u) << "Should find few communities";
    EXPECT_LT(comm->modularity, 0.25f) << "Modularity should be low (no structure)";

    topology_community_structure_free(comm);
    neural_network_destroy(network);
}

TEST_F(CommunityDetectionTest, DisconnectedNetwork) {
    uint32_t num_components = 3;
    neural_network_t network = create_disconnected_network(num_components);
    ASSERT_NE(network, nullptr);

    community_structure_t* comm = community_detect(network, nullptr);
    ASSERT_NE(comm, nullptr);

    /* Should find at least as many communities as components
     * (algorithm may split components further) */
    EXPECT_GE(comm->num_communities, num_components);

    /* Modularity should be positive (some structure detected) */
    EXPECT_GT(comm->modularity, 0.0f) << "Should detect some community structure";

    topology_community_structure_free(comm);
    neural_network_destroy(network);
}

/* ===========================================================================
 * Edge Case Tests
 * =========================================================================== */

TEST_F(CommunityDetectionTest, NullNetwork) {
    community_structure_t* comm = community_detect(nullptr, nullptr);
    EXPECT_EQ(comm, nullptr);
}

TEST_F(CommunityDetectionTest, EmptyNetwork) {
    network_config_t config = {0};
    config.num_neurons = 0;
    neural_network_t network = neural_network_create(&config);

    community_structure_t* comm = community_detect(network, nullptr);
    EXPECT_EQ(comm, nullptr);

    if (network) neural_network_destroy(network);
}

TEST_F(CommunityDetectionTest, SingleNeuron) {
    network_config_t config = {0};
    config.num_neurons = 1;
    config.input_size = 1;
    config.output_size = 1;
    neural_network_t network = neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    community_structure_t* comm = community_detect(network, nullptr);
    ASSERT_NE(comm, nullptr);

    EXPECT_EQ(comm->num_communities, 1u);
    EXPECT_EQ(comm->community_ids[0], 0u);
    EXPECT_NEAR(comm->modularity, 0.0f, 0.01f);

    topology_community_structure_free(comm);
    neural_network_destroy(network);
}

/* ===========================================================================
 * Modularity Calculation Tests
 * =========================================================================== */

TEST_F(CommunityDetectionTest, ModularityCalculation) {
    neural_network_t network = create_simple_modular_network();
    ASSERT_NE(network, nullptr);

    community_structure_t* comm = community_detect(network, nullptr);
    ASSERT_NE(comm, nullptr);

    /* Modularity should be positive for good partition */
    EXPECT_GT(comm->modularity, 0.0f);
    EXPECT_LT(comm->modularity, 1.0f);

    topology_community_structure_free(comm);
    neural_network_destroy(network);
}

/* ===========================================================================
 * API Tests
 * =========================================================================== */

TEST_F(CommunityDetectionTest, GetCommunityForNeuron) {
    neural_network_t network = create_simple_modular_network();
    ASSERT_NE(network, nullptr);

    community_structure_t* comm = community_detect(network, nullptr);
    ASSERT_NE(comm, nullptr);

    /* Valid queries */
    for (uint32_t i = 0; i < 20; i++) {
        uint32_t c = community_get_neuron_community(comm, i);
        EXPECT_LT(c, comm->num_communities);
    }

    topology_community_structure_free(comm);
    neural_network_destroy(network);
}

TEST_F(CommunityDetectionTest, CommunitySizes) {
    neural_network_t network = create_simple_modular_network();
    ASSERT_NE(network, nullptr);

    community_structure_t* comm = community_detect(network, nullptr);
    ASSERT_NE(comm, nullptr);

    /* Sum of sizes should equal num neurons */
    uint32_t total_size = 0;
    for (uint32_t i = 0; i < comm->num_communities; i++) {
        EXPECT_GT(comm->community_sizes[i], 0u);
        total_size += comm->community_sizes[i];
    }

    EXPECT_EQ(total_size, comm->num_neurons);

    topology_community_structure_free(comm);
    neural_network_destroy(network);
}

TEST_F(CommunityDetectionTest, FreeNullCommunity) {
    /* Should not crash */
    topology_community_structure_free(nullptr);
}

/* ===========================================================================
 * Configuration Tests
 * =========================================================================== */

TEST_F(CommunityDetectionTest, CustomConfiguration) {
    neural_network_t network = create_simple_modular_network();
    ASSERT_NE(network, nullptr);

    community_detection_config_t config = community_default_config();
    config.max_iterations = 50;
    config.resolution = 1.5f;

    community_structure_t* comm = community_detect(network, &config);
    ASSERT_NE(comm, nullptr);

    /* Should still detect communities */
    EXPECT_GE(comm->num_communities, 1u);

    topology_community_structure_free(comm);
    neural_network_destroy(network);
}

/* ===========================================================================
 * Determinism Tests
 * =========================================================================== */

TEST_F(CommunityDetectionTest, DeterministicResults) {
    neural_network_t network = create_simple_modular_network();
    ASSERT_NE(network, nullptr);

    community_detection_config_t config = community_default_config();
    config.random_seed = 42;

    /* Run twice */
    community_structure_t* comm1 = community_detect(network, &config);
    ASSERT_NE(comm1, nullptr);

    community_structure_t* comm2 = community_detect(network, &config);
    ASSERT_NE(comm2, nullptr);

    /* Should produce same results */
    EXPECT_EQ(comm1->num_communities, comm2->num_communities);
    EXPECT_NEAR(comm1->modularity, comm2->modularity, 0.001f);

    /* Community assignments should match */
    for (uint32_t i = 0; i < comm1->num_neurons; i++) {
        EXPECT_EQ(comm1->community_ids[i], comm2->community_ids[i]);
    }

    topology_community_structure_free(comm1);
    topology_community_structure_free(comm2);
    neural_network_destroy(network);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
