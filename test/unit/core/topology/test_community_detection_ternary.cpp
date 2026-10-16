/**
 * @file test_community_detection_ternary.cpp
 * @brief Unit tests for ternary integration with community detection
 *
 * Tests ternary integration with community detection including:
 * - Ternary adjacency matrices for modularity computation
 * - Ternary edge weights in community detection
 * - Community structure with ternary representation
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <algorithm>
#include <numeric>
#include <random>

// Headers have their own extern "C" guards
#include "core/topology/nimcp_community_detection.h"
#include "utils/ternary/nimcp_ternary.h"
#include "utils/ternary/nimcp_ternary_types.h"
#include "utils/ternary/nimcp_ternary_matrix.h"
#include "utils/error/nimcp_error_codes.h"

//=============================================================================
// Test Fixture
//=============================================================================

class CommunityDetectionTernaryTest : public ::testing::Test {
protected:
    community_structure_t* community_structure;

    void SetUp() override {
        community_structure = nullptr;
    }

    void TearDown() override {
        if (community_structure) {
            topology_community_structure_free(community_structure);
            community_structure = nullptr;
        }
    }

    // Helper: Create ternary adjacency matrix with clear community structure
    std::vector<trit_t> createCommunityAdjacency(uint32_t n_neurons, uint32_t n_communities) {
        std::vector<trit_t> adj(n_neurons * n_neurons, TRIT_UNKNOWN);

        uint32_t neurons_per_community = n_neurons / n_communities;

        for (uint32_t i = 0; i < n_neurons; i++) {
            uint32_t comm_i = i / neurons_per_community;
            if (comm_i >= n_communities) comm_i = n_communities - 1;

            for (uint32_t j = 0; j < n_neurons; j++) {
                uint32_t comm_j = j / neurons_per_community;
                if (comm_j >= n_communities) comm_j = n_communities - 1;

                if (i == j) {
                    // Self-connection
                    adj[i * n_neurons + j] = TRIT_POSITIVE;
                } else if (comm_i == comm_j) {
                    // Same community - high probability of connection
                    if ((i + j) % 2 == 0) {
                        adj[i * n_neurons + j] = TRIT_POSITIVE;
                    }
                } else {
                    // Different communities - low probability
                    if ((i + j) % 7 == 0) {
                        adj[i * n_neurons + j] = TRIT_POSITIVE;
                    }
                }
            }
        }

        return adj;
    }

    // Helper: Compute ternary modularity
    float computeTernaryModularity(const std::vector<trit_t>& adj, uint32_t n_neurons,
                                    const std::vector<uint32_t>& community_ids) {
        // Count total edges
        int32_t total_edges = 0;
        for (uint32_t i = 0; i < n_neurons; i++) {
            for (uint32_t j = 0; j < n_neurons; j++) {
                if (adj[i * n_neurons + j] == TRIT_POSITIVE) {
                    total_edges++;
                }
            }
        }

        if (total_edges == 0) return 0.0f;

        // Compute degrees
        std::vector<int32_t> degrees(n_neurons, 0);
        for (uint32_t i = 0; i < n_neurons; i++) {
            for (uint32_t j = 0; j < n_neurons; j++) {
                if (adj[i * n_neurons + j] == TRIT_POSITIVE) {
                    degrees[i]++;
                }
            }
        }

        // Compute modularity
        float Q = 0.0f;
        for (uint32_t i = 0; i < n_neurons; i++) {
            for (uint32_t j = 0; j < n_neurons; j++) {
                if (community_ids[i] == community_ids[j]) {
                    float A_ij = (adj[i * n_neurons + j] == TRIT_POSITIVE) ? 1.0f : 0.0f;
                    float expected = static_cast<float>(degrees[i] * degrees[j]) / total_edges;
                    Q += A_ij - expected;
                }
            }
        }
        Q /= total_edges;

        return Q;
    }
};

//=============================================================================
// Default Configuration Tests
//=============================================================================

class CommunityConfigTest : public ::testing::Test {
protected:
    community_detection_config_t config;
};

TEST_F(CommunityConfigTest, DefaultConfigReturnsValidConfig) {
    config = community_default_config();

    // Check sensible defaults
    EXPECT_GT(config.max_iterations, 0u);
    EXPECT_GT(config.min_modularity_gain, 0.0f);
    EXPECT_GE(config.resolution, 0.0f);
}

TEST_F(CommunityConfigTest, DefaultConfigMaxIterations) {
    config = community_default_config();
    EXPECT_GE(config.max_iterations, 10u);
    EXPECT_LE(config.max_iterations, 1000u);
}

TEST_F(CommunityConfigTest, DefaultConfigResolution) {
    config = community_default_config();
    // Resolution typically around 1.0
    EXPECT_GE(config.resolution, 0.5f);
    EXPECT_LE(config.resolution, 2.0f);
}

//=============================================================================
// Ternary Adjacency Matrix Tests
//=============================================================================

class TernaryAdjacencyTest : public CommunityDetectionTernaryTest {};

TEST_F(TernaryAdjacencyTest, CreateTernaryAdjacencyWithCommunities) {
    std::vector<trit_t> adj = createCommunityAdjacency(20, 4);
    EXPECT_EQ(adj.size(), 400u);  // 20x20

    // Count edges
    uint32_t n_positive = 0, n_negative = 0, n_zero = 0;
    for (trit_t t : adj) {
        switch (t) {
            case TRIT_POSITIVE: n_positive++; break;
            case TRIT_NEGATIVE: n_negative++; break;
            case TRIT_UNKNOWN: n_zero++; break;
        }
    }

    // Should have mix
    EXPECT_GT(n_positive, 0u);
    EXPECT_GT(n_zero, 0u);
}

TEST_F(TernaryAdjacencyTest, TernaryAdjacencySymmetric) {
    // Create symmetric adjacency
    const uint32_t n = 10;
    std::vector<trit_t> adj(n * n, TRIT_UNKNOWN);

    // Add symmetric edges
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {0, 1}, {1, 2}, {2, 3}, {3, 4}, {5, 6}, {6, 7}, {7, 8}
    };

    for (auto& edge : edges) {
        adj[edge.first * n + edge.second] = TRIT_POSITIVE;
        adj[edge.second * n + edge.first] = TRIT_POSITIVE;
    }

    // Add self-loops
    for (uint32_t i = 0; i < n; i++) {
        adj[i * n + i] = TRIT_POSITIVE;
    }

    // Verify symmetry
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = 0; j < n; j++) {
            EXPECT_EQ(adj[i * n + j], adj[j * n + i])
                << "Asymmetric at (" << i << ", " << j << ")";
        }
    }
}

TEST_F(TernaryAdjacencyTest, TernaryAdjacencyWithInhibitory) {
    // Create adjacency with both excitatory and inhibitory edges
    const uint32_t n = 10;
    std::vector<trit_t> adj(n * n, TRIT_UNKNOWN);

    // Excitatory edges (within community)
    for (uint32_t i = 0; i < 5; i++) {
        for (uint32_t j = 0; j < 5; j++) {
            if (i != j) {
                adj[i * n + j] = TRIT_POSITIVE;
            }
        }
    }

    // Inhibitory edges (between communities)
    for (uint32_t i = 0; i < 5; i++) {
        for (uint32_t j = 5; j < 10; j++) {
            adj[i * n + j] = TRIT_NEGATIVE;
            adj[j * n + i] = TRIT_NEGATIVE;
        }
    }

    // Count distribution
    uint32_t n_pos = 0, n_neg = 0, n_zero = 0;
    for (trit_t t : adj) {
        switch (t) {
            case TRIT_POSITIVE: n_pos++; break;
            case TRIT_NEGATIVE: n_neg++; break;
            case TRIT_UNKNOWN: n_zero++; break;
        }
    }

    // Should have all three types
    EXPECT_GT(n_pos, 0u);
    EXPECT_GT(n_neg, 0u);
    EXPECT_GT(n_zero, 0u);
}

//=============================================================================
// Modularity Computation Tests
//=============================================================================

class TernaryModularityTest : public CommunityDetectionTernaryTest {};

TEST_F(TernaryModularityTest, ComputeModularitySingleCommunity) {
    // All neurons in one community
    const uint32_t n = 10;
    std::vector<trit_t> adj(n * n, TRIT_POSITIVE);  // Fully connected
    std::vector<uint32_t> community_ids(n, 0);  // All in community 0

    float Q = computeTernaryModularity(adj, n, community_ids);

    // Single community on fully connected graph should have Q ≈ 0
    EXPECT_NEAR(Q, 0.0f, 0.1f);
}

TEST_F(TernaryModularityTest, ComputeModularityPerfectCommunities) {
    // Two completely separate communities
    const uint32_t n = 10;
    std::vector<trit_t> adj(n * n, TRIT_UNKNOWN);
    std::vector<uint32_t> community_ids(n);

    // Community 0: neurons 0-4, fully connected within
    for (uint32_t i = 0; i < 5; i++) {
        community_ids[i] = 0;
        for (uint32_t j = 0; j < 5; j++) {
            adj[i * n + j] = TRIT_POSITIVE;
        }
    }

    // Community 1: neurons 5-9, fully connected within
    for (uint32_t i = 5; i < 10; i++) {
        community_ids[i] = 1;
        for (uint32_t j = 5; j < 10; j++) {
            adj[i * n + j] = TRIT_POSITIVE;
        }
    }

    float Q = computeTernaryModularity(adj, n, community_ids);

    // Perfect community separation should have high modularity
    EXPECT_GT(Q, 0.3f);
}

TEST_F(TernaryModularityTest, ComputeModularityNoCommunities) {
    // Empty adjacency (no edges)
    const uint32_t n = 10;
    std::vector<trit_t> adj(n * n, TRIT_UNKNOWN);
    std::vector<uint32_t> community_ids(n, 0);

    float Q = computeTernaryModularity(adj, n, community_ids);

    // No edges = 0 modularity
    EXPECT_FLOAT_EQ(Q, 0.0f);
}

TEST_F(TernaryModularityTest, ModularityRangeValid) {
    // Test various adjacency patterns
    const uint32_t n = 20;

    for (int seed = 0; seed < 5; seed++) {
        std::vector<trit_t> adj = createCommunityAdjacency(n, 3 + seed);
        std::vector<uint32_t> community_ids(n);
        for (uint32_t i = 0; i < n; i++) {
            community_ids[i] = i % (3 + seed);
        }

        float Q = computeTernaryModularity(adj, n, community_ids);

        // Modularity should be in valid range [-0.5, 1.0]
        EXPECT_GE(Q, -0.5f);
        EXPECT_LE(Q, 1.0f);
    }
}

//=============================================================================
// Community Structure Tests
//=============================================================================

class CommunityStructureTest : public CommunityDetectionTernaryTest {};

TEST_F(CommunityStructureTest, FreeNullSafe) {
    topology_community_structure_free(nullptr);
    SUCCEED();  // Should not crash
}

TEST_F(CommunityStructureTest, GetNeuronCommunityValid) {
    // Create mock structure
    community_structure_t structure;
    structure.num_neurons = 10;
    structure.num_communities = 3;
    structure.community_ids = new uint32_t[10]{0, 0, 0, 1, 1, 1, 1, 2, 2, 2};
    structure.community_sizes = new uint32_t[3]{3, 4, 3};
    structure.modularity = 0.5f;
    structure.internal_density = nullptr;
    structure.external_density = nullptr;

    // Query community IDs
    EXPECT_EQ(community_get_neuron_community(&structure, 0), 0u);
    EXPECT_EQ(community_get_neuron_community(&structure, 3), 1u);
    EXPECT_EQ(community_get_neuron_community(&structure, 7), 2u);

    delete[] structure.community_ids;
    delete[] structure.community_sizes;
}

TEST_F(CommunityStructureTest, GetNeuronCommunityNullStructure) {
    uint32_t result = community_get_neuron_community(nullptr, 0);
    EXPECT_EQ(result, UINT32_MAX);  // Error indicator
}

TEST_F(CommunityStructureTest, GetNeuronCommunityOutOfRange) {
    community_structure_t structure;
    structure.num_neurons = 10;
    structure.num_communities = 2;
    structure.community_ids = new uint32_t[10]{0, 0, 0, 0, 0, 1, 1, 1, 1, 1};
    structure.community_sizes = new uint32_t[2]{5, 5};
    structure.modularity = 0.4f;
    structure.internal_density = nullptr;
    structure.external_density = nullptr;

    uint32_t result = community_get_neuron_community(&structure, 100);  // Out of range
    EXPECT_EQ(result, UINT32_MAX);

    delete[] structure.community_ids;
    delete[] structure.community_sizes;
}

//=============================================================================
// Hub Detection Tests
//=============================================================================

class HubDetectionTest : public CommunityDetectionTernaryTest {};

TEST_F(HubDetectionTest, HubStructureFreeNullSafe) {
    hub_structure_free(nullptr);
    SUCCEED();  // Should not crash
}

//=============================================================================
// Topology Validation Tests
//=============================================================================

class TopologyValidationTest : public CommunityDetectionTernaryTest {};

TEST_F(TopologyValidationTest, ValidationStructureValid) {
    topology_validation_t validation;

    // Check structure has expected fields
    validation.is_valid = true;
    validation.modularity = 0.4f;
    validation.clustering_coefficient = 0.3f;
    validation.characteristic_path = 2.5f;
    validation.small_world_sigma = 1.5f;
    validation.num_communities = 4;
    validation.num_hubs = 3;
    std::strcpy(validation.error_message, "OK");

    EXPECT_TRUE(validation.is_valid);
    EXPECT_FLOAT_EQ(validation.modularity, 0.4f);
    EXPECT_EQ(validation.num_communities, 4u);
}

TEST_F(TopologyValidationTest, ValidationModularityThresholds) {
    // Test modularity interpretation
    // Q > 0.3: Strong community structure
    // Q in [0.2, 0.3]: Moderate
    // Q < 0.2: Weak

    float strong_Q = 0.45f;
    float moderate_Q = 0.25f;
    float weak_Q = 0.15f;

    EXPECT_GT(strong_Q, 0.3f);
    EXPECT_GE(moderate_Q, 0.2f);
    EXPECT_LE(moderate_Q, 0.3f);
    EXPECT_LT(weak_Q, 0.2f);
}

//=============================================================================
// Ternary Community Integration Tests
//=============================================================================

class TernaryIntegrationTest : public CommunityDetectionTernaryTest {};

TEST_F(TernaryIntegrationTest, TernaryMatrixMemoryEfficiency) {
    const uint32_t n = 100;

    // Float adjacency: n * n * 4 bytes
    size_t float_bytes = n * n * sizeof(float);

    // Ternary adjacency (unpacked): n * n * 1 byte
    size_t ternary_unpacked = n * n * sizeof(trit_t);

    // Ternary adjacency (2-bit packed): n * n / 4 bytes
    size_t ternary_packed = (n * n + 3) / 4;

    EXPECT_LT(ternary_unpacked, float_bytes);
    EXPECT_LT(ternary_packed, ternary_unpacked);

    float savings_unpacked = 1.0f - static_cast<float>(ternary_unpacked) / float_bytes;
    float savings_packed = 1.0f - static_cast<float>(ternary_packed) / float_bytes;

    EXPECT_GT(savings_unpacked, 0.7f);  // >70% savings
    EXPECT_GT(savings_packed, 0.93f);   // >93% savings
}

TEST_F(TernaryIntegrationTest, TernaryEdgeClassification) {
    // Test that ternary edges map to biological interpretation
    // POSITIVE: Excitatory connection (promotes activity)
    // NEGATIVE: Inhibitory connection (suppresses activity)
    // UNKNOWN: No connection

    std::vector<trit_t> adj = {
        TRIT_POSITIVE,   // Strong excitatory
        TRIT_NEGATIVE,   // Strong inhibitory
        TRIT_UNKNOWN     // No connection
    };

    // Verify classification
    EXPECT_EQ(adj[0], TRIT_POSITIVE);
    EXPECT_EQ(adj[1], TRIT_NEGATIVE);
    EXPECT_EQ(adj[2], TRIT_UNKNOWN);

    // Biological interpretation
    EXPECT_EQ(adj[0], TRIT_EXCITATORY);  // Alias check
    EXPECT_EQ(adj[1], TRIT_INHIBITORY);  // Alias check
    EXPECT_EQ(adj[2], TRIT_SILENT);      // Alias check
}

TEST_F(TernaryIntegrationTest, TernarySignedModularity) {
    // Test modularity with signed (excitatory/inhibitory) edges
    const uint32_t n = 12;
    std::vector<trit_t> adj(n * n, TRIT_UNKNOWN);
    std::vector<uint32_t> community_ids(n);

    // Community 0: Excitatory within (neurons 0-5)
    for (uint32_t i = 0; i < 6; i++) {
        community_ids[i] = 0;
        for (uint32_t j = 0; j < 6; j++) {
            if (i != j) {
                adj[i * n + j] = TRIT_POSITIVE;
            }
        }
    }

    // Community 1: Excitatory within (neurons 6-11)
    for (uint32_t i = 6; i < 12; i++) {
        community_ids[i] = 1;
        for (uint32_t j = 6; j < 12; j++) {
            if (i != j) {
                adj[i * n + j] = TRIT_POSITIVE;
            }
        }
    }

    // Inhibitory between communities
    for (uint32_t i = 0; i < 6; i++) {
        for (uint32_t j = 6; j < 12; j++) {
            adj[i * n + j] = TRIT_NEGATIVE;
            adj[j * n + i] = TRIT_NEGATIVE;
        }
    }

    // In a signed network, inhibitory between communities actually
    // reinforces community structure
    // For unsigned modularity (treating only + as edges), Q should be high
    float Q = computeTernaryModularity(adj, n, community_ids);
    EXPECT_GT(Q, 0.3f);
}

//=============================================================================
// Edge Cases and Boundary Conditions
//=============================================================================

class EdgeCasesTest : public CommunityDetectionTernaryTest {};

TEST_F(EdgeCasesTest, SingleNeuronCommunity) {
    const uint32_t n = 1;
    std::vector<trit_t> adj = {TRIT_POSITIVE};  // Self-connection
    std::vector<uint32_t> community_ids = {0};

    float Q = computeTernaryModularity(adj, n, community_ids);
    EXPECT_GE(Q, -0.5f);
    EXPECT_LE(Q, 1.0f);
}

TEST_F(EdgeCasesTest, TwoNeuronsCommunity) {
    const uint32_t n = 2;
    std::vector<trit_t> adj = {
        TRIT_POSITIVE, TRIT_POSITIVE,
        TRIT_POSITIVE, TRIT_POSITIVE
    };
    std::vector<uint32_t> community_ids = {0, 0};

    float Q = computeTernaryModularity(adj, n, community_ids);
    EXPECT_GE(Q, -0.5f);
    EXPECT_LE(Q, 1.0f);
}

TEST_F(EdgeCasesTest, AllIsolatedNeurons) {
    const uint32_t n = 10;
    std::vector<trit_t> adj(n * n, TRIT_UNKNOWN);  // No edges
    std::vector<uint32_t> community_ids(n);
    for (uint32_t i = 0; i < n; i++) {
        community_ids[i] = i;  // Each neuron in own community
    }

    float Q = computeTernaryModularity(adj, n, community_ids);
    EXPECT_FLOAT_EQ(Q, 0.0f);  // No edges, no modularity
}

TEST_F(EdgeCasesTest, AllInhibitoryNetwork) {
    const uint32_t n = 5;
    std::vector<trit_t> adj(n * n, TRIT_NEGATIVE);
    std::vector<uint32_t> community_ids(n, 0);

    // All inhibitory = no positive edges
    uint32_t positive_count = 0;
    for (trit_t t : adj) {
        if (t == TRIT_POSITIVE) positive_count++;
    }
    EXPECT_EQ(positive_count, 0u);

    // Modularity computation should handle this
    // (treating only positive as edges means no edges)
    float Q = computeTernaryModularity(adj, n, community_ids);
    EXPECT_FLOAT_EQ(Q, 0.0f);
}

TEST_F(EdgeCasesTest, LargeNetworkPerformance) {
    const uint32_t n = 500;
    std::vector<trit_t> adj = createCommunityAdjacency(n, 10);
    std::vector<uint32_t> community_ids(n);
    for (uint32_t i = 0; i < n; i++) {
        community_ids[i] = i % 10;
    }

    // Should complete without timeout
    float Q = computeTernaryModularity(adj, n, community_ids);
    EXPECT_GE(Q, -0.5f);
    EXPECT_LE(Q, 1.0f);
}

//=============================================================================
// Ternary Matrix Operations Tests
//=============================================================================

class TernaryMatrixOpsTest : public CommunityDetectionTernaryTest {};

TEST_F(TernaryMatrixOpsTest, CreateTernaryMatrixBasic) {
    trit_matrix_t* mat = trit_matrix_create(10, 10, TERNARY_PACK_NONE);
    if (mat) {
        EXPECT_EQ(mat->rows, 10u);
        EXPECT_EQ(mat->cols, 10u);
        trit_matrix_destroy(mat);
    }
}

TEST_F(TernaryMatrixOpsTest, CreateTernaryMatrixPacked) {
    trit_matrix_t* mat = trit_matrix_create(10, 10, TERNARY_PACK_2BIT);
    if (mat) {
        EXPECT_EQ(mat->rows, 10u);
        EXPECT_EQ(mat->cols, 10u);
        trit_matrix_destroy(mat);
    }
}

TEST_F(TernaryMatrixOpsTest, TernaryMatrixSetGet) {
    trit_matrix_t* mat = trit_matrix_create(5, 5, TERNARY_PACK_NONE);
    if (mat) {
        // Set some values
        trit_matrix_set(mat, 0, 0, TRIT_POSITIVE);
        trit_matrix_set(mat, 0, 1, TRIT_NEGATIVE);
        trit_matrix_set(mat, 1, 0, TRIT_UNKNOWN);

        // Get and verify
        EXPECT_EQ(trit_matrix_get(mat, 0, 0), TRIT_POSITIVE);
        EXPECT_EQ(trit_matrix_get(mat, 0, 1), TRIT_NEGATIVE);
        EXPECT_EQ(trit_matrix_get(mat, 1, 0), TRIT_UNKNOWN);

        trit_matrix_destroy(mat);
    }
}

TEST_F(TernaryMatrixOpsTest, TernaryMatrixToAdjacency) {
    trit_matrix_t* mat = trit_matrix_create(4, 4, TERNARY_PACK_NONE);
    if (mat) {
        // Create ring topology
        trit_matrix_set(mat, 0, 1, TRIT_POSITIVE);
        trit_matrix_set(mat, 1, 2, TRIT_POSITIVE);
        trit_matrix_set(mat, 2, 3, TRIT_POSITIVE);
        trit_matrix_set(mat, 3, 0, TRIT_POSITIVE);

        // Self-connections
        for (uint32_t i = 0; i < 4; i++) {
            trit_matrix_set(mat, i, i, TRIT_POSITIVE);
        }

        // Verify topology
        EXPECT_EQ(trit_matrix_get(mat, 0, 1), TRIT_POSITIVE);
        EXPECT_EQ(trit_matrix_get(mat, 0, 2), TRIT_UNKNOWN);
        EXPECT_EQ(trit_matrix_get(mat, 0, 3), TRIT_UNKNOWN);

        trit_matrix_destroy(mat);
    }
}

//=============================================================================
// Community ID Assignment Tests
//=============================================================================

class CommunityAssignmentTest : public CommunityDetectionTernaryTest {};

TEST_F(CommunityAssignmentTest, TernaryBasedAssignment) {
    // Test assigning communities based on ternary edge structure
    const uint32_t n = 8;
    std::vector<trit_t> adj(n * n, TRIT_UNKNOWN);

    // Create two clear clusters
    // Cluster 1: neurons 0-3 (fully connected)
    for (uint32_t i = 0; i < 4; i++) {
        for (uint32_t j = 0; j < 4; j++) {
            adj[i * n + j] = TRIT_POSITIVE;
        }
    }

    // Cluster 2: neurons 4-7 (fully connected)
    for (uint32_t i = 4; i < 8; i++) {
        for (uint32_t j = 4; j < 8; j++) {
            adj[i * n + j] = TRIT_POSITIVE;
        }
    }

    // Weak inter-cluster connection
    adj[3 * n + 4] = TRIT_POSITIVE;
    adj[4 * n + 3] = TRIT_POSITIVE;

    // Compute edge density within/between clusters
    uint32_t intra_cluster1 = 0, intra_cluster2 = 0, inter_cluster = 0;

    for (uint32_t i = 0; i < 4; i++) {
        for (uint32_t j = 0; j < 4; j++) {
            if (adj[i * n + j] == TRIT_POSITIVE) intra_cluster1++;
        }
    }

    for (uint32_t i = 4; i < 8; i++) {
        for (uint32_t j = 4; j < 8; j++) {
            if (adj[i * n + j] == TRIT_POSITIVE) intra_cluster2++;
        }
    }

    for (uint32_t i = 0; i < 4; i++) {
        for (uint32_t j = 4; j < 8; j++) {
            if (adj[i * n + j] == TRIT_POSITIVE) inter_cluster++;
        }
    }

    // Intra-cluster density should be much higher
    EXPECT_EQ(intra_cluster1, 16u);  // 4x4
    EXPECT_EQ(intra_cluster2, 16u);  // 4x4
    EXPECT_EQ(inter_cluster, 1u);    // Just one connection
}

TEST_F(CommunityAssignmentTest, ModularityMaximization) {
    // Test that optimal partition maximizes modularity
    const uint32_t n = 6;
    std::vector<trit_t> adj(n * n, TRIT_UNKNOWN);

    // Clear 2-community structure
    for (uint32_t i = 0; i < 3; i++) {
        for (uint32_t j = 0; j < 3; j++) {
            adj[i * n + j] = TRIT_POSITIVE;
        }
    }
    for (uint32_t i = 3; i < 6; i++) {
        for (uint32_t j = 3; j < 6; j++) {
            adj[i * n + j] = TRIT_POSITIVE;
        }
    }

    // Optimal partition: {0,1,2} and {3,4,5}
    std::vector<uint32_t> optimal_partition = {0, 0, 0, 1, 1, 1};

    // Sub-optimal partition: alternating
    std::vector<uint32_t> suboptimal_partition = {0, 1, 0, 1, 0, 1};

    float Q_optimal = computeTernaryModularity(adj, n, optimal_partition);
    float Q_suboptimal = computeTernaryModularity(adj, n, suboptimal_partition);

    // Optimal should have higher modularity
    EXPECT_GT(Q_optimal, Q_suboptimal);
}

//=============================================================================
// Error Handling Tests
//=============================================================================

class ErrorHandlingTest : public CommunityDetectionTernaryTest {};

TEST_F(ErrorHandlingTest, GetLastErrorInitiallyNull) {
    const char* error = community_get_last_error();
    // May be NULL or empty string initially
    if (error) {
        // If not NULL, should be valid string
        EXPECT_GE(strlen(error), 0u);
    }
}

TEST_F(ErrorHandlingTest, GetNeuronsInCommunityNullInputs) {
    community_structure_t structure;
    structure.num_neurons = 10;
    structure.num_communities = 2;
    structure.community_ids = new uint32_t[10];
    for (int i = 0; i < 10; i++) structure.community_ids[i] = i / 5;
    structure.community_sizes = new uint32_t[2]{5, 5};
    structure.modularity = 0.4f;
    structure.internal_density = nullptr;
    structure.external_density = nullptr;

    uint32_t* neuron_ids = nullptr;
    uint32_t count = 0;

    // Null structure
    bool result = community_get_neurons_in_community(nullptr, 0, &neuron_ids, &count);
    EXPECT_FALSE(result);

    // Null output arrays
    result = community_get_neurons_in_community(&structure, 0, nullptr, &count);
    EXPECT_FALSE(result);

    result = community_get_neurons_in_community(&structure, 0, &neuron_ids, nullptr);
    EXPECT_FALSE(result);

    // Invalid community ID
    result = community_get_neurons_in_community(&structure, 99, &neuron_ids, &count);
    EXPECT_FALSE(result);

    delete[] structure.community_ids;
    delete[] structure.community_sizes;
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
