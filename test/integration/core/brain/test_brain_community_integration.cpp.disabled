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

#include "utils/algorithms/nimcp_louvain.h"
#include "utils/algorithms/nimcp_modularity.h"
#include "utils/algorithms/nimcp_centrality.h"
#include "utils/containers/nimcp_graph.h"

//=============================================================================
// Test Constants
//=============================================================================

static const double EPSILON = 1e-6;
static const uint32_t MAX_VERTICES = 256;

//=============================================================================
// Helper Functions for Brain Network Construction
//=============================================================================

/**
 * WHAT: Create modular brain network with visual, auditory, motor regions
 * WHY: Test realistic brain topology
 * HOW: Create cliques for regions, sparse inter-region connections
 */
static NimcpGraph* create_brain_network(void)
{
    NimcpGraph* graph = nimcp_graph_create();
    if (!graph) return nullptr;

    // V1 (Visual cortex): vertices 0-4
    // A1 (Auditory cortex): vertices 5-9
    // M1 (Motor cortex): vertices 10-14
    // Thalamus (hub): vertices 15-17

    uint32_t vertex_id = 0;

    // V1 region (tightly connected)
    for (uint32_t i = 0; i < 5; i++) {
        nimcp_graph_add_vertex(graph, vertex_id++, float(i), 0.0f, 0.0f, 0);
    }

    // A1 region (tightly connected)
    for (uint32_t i = 0; i < 5; i++) {
        nimcp_graph_add_vertex(graph, vertex_id++, 0.0f, float(i), 0.0f, 0);
    }

    // M1 region (tightly connected)
    for (uint32_t i = 0; i < 5; i++) {
        nimcp_graph_add_vertex(graph, vertex_id++, float(i), float(i), 0.0f, 0);
    }

    // Thalamus (hub region)
    for (uint32_t i = 0; i < 3; i++) {
        nimcp_graph_add_vertex(graph, vertex_id++, 2.5f, 2.5f, 0.0f, 0);
    }

    // Create intra-region connections (high density)
    // V1 connections
    for (uint32_t i = 0; i < 5; i++) {
        for (uint32_t j = i + 1; j < 5; j++) {
            nimcp_graph_add_edge(graph, i, j, 1.0f);
            nimcp_graph_add_edge(graph, j, i, 1.0f);
        }
    }

    // A1 connections
    for (uint32_t i = 5; i < 10; i++) {
        for (uint32_t j = i + 1; j < 10; j++) {
            nimcp_graph_add_edge(graph, i, j, 1.0f);
            nimcp_graph_add_edge(graph, j, i, 1.0f);
        }
    }

    // M1 connections
    for (uint32_t i = 10; i < 15; i++) {
        for (uint32_t j = i + 1; j < 15; j++) {
            nimcp_graph_add_edge(graph, i, j, 1.0f);
            nimcp_graph_add_edge(graph, j, i, 1.0f);
        }
    }

    // Thalamus internal connections (strong)
    nimcp_graph_add_edge(graph, 15, 16, 1.0f);
    nimcp_graph_add_edge(graph, 16, 15, 1.0f);
    nimcp_graph_add_edge(graph, 16, 17, 1.0f);
    nimcp_graph_add_edge(graph, 17, 16, 1.0f);
    nimcp_graph_add_edge(graph, 15, 17, 1.0f);
    nimcp_graph_add_edge(graph, 17, 15, 1.0f);

    // Create inter-region connections (through thalamus)
    // V1 to Thalamus
    for (uint32_t i = 0; i < 3; i++) {
        nimcp_graph_add_edge(graph, i, 15 + i, 0.5f);
        nimcp_graph_add_edge(graph, 15 + i, i, 0.5f);
    }

    // A1 to Thalamus
    for (uint32_t i = 0; i < 3; i++) {
        nimcp_graph_add_edge(graph, 5 + i, 15 + i, 0.5f);
        nimcp_graph_add_edge(graph, 15 + i, 5 + i, 0.5f);
    }

    // M1 to Thalamus
    for (uint32_t i = 0; i < 3; i++) {
        nimcp_graph_add_edge(graph, 10 + i, 15 + i, 0.5f);
        nimcp_graph_add_edge(graph, 15 + i, 10 + i, 0.5f);
    }

    return graph;
}

/**
 * WHAT: Verify regions are detected as communities
 * WHY: Ensure community detection respects brain structure
 * HOW: Check if vertices in same region are in same community
 */
static bool verify_brain_communities(const NimcpCommunityPartition* partition)
{
    if (!partition) return false;

    // Check V1 region (0-4) - should be same community
    uint32_t v1_comm = partition->assignments[0];
    for (uint32_t i = 1; i < 5; i++) {
        if (partition->assignments[i] != v1_comm) {
            return false;
        }
    }

    // Check A1 region (5-9) - should be same community
    uint32_t a1_comm = partition->assignments[5];
    if (a1_comm == v1_comm) {
        return false;  // Should be different from V1
    }
    for (uint32_t i = 6; i < 10; i++) {
        if (partition->assignments[i] != a1_comm) {
            return false;
        }
    }

    return true;
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
    // WHAT: Detect communities in modular brain network
    // WHY: Verify algorithm finds brain regions
    // HOW: Create brain network, run Louvain

    NimcpGraph* graph = create_brain_network();
    ASSERT_NE(nullptr, graph);

    NimcpCommunityPartition* partition = nimcp_louvain_detect(graph, 1.0, 42);
    ASSERT_NE(nullptr, partition);

    // Should find at least 3 communities (V1, A1, M1)
    EXPECT_GE(partition->num_communities, 3)
        << "Should detect at least V1, A1, M1 as separate communities";

    EXPECT_GT(partition->modularity, 0.25)
        << "Brain network should have strong modularity (Q > 0.25)";

    nimcp_community_partition_destroy(partition);
    nimcp_graph_destroy(graph);
}

TEST_F(BrainCommunityIntegrationTest, test_brain_regions_cluster_together)
{
    // WHAT: Brain regions cluster into single communities
    // WHY: Regions are densely connected internally
    // HOW: Verify region vertices assigned to same community

    NimcpGraph* graph = create_brain_network();
    ASSERT_NE(nullptr, graph);

    NimcpCommunityPartition* partition = nimcp_louvain_detect(graph, 1.0, 42);
    ASSERT_NE(nullptr, partition);

    // V1 region (0-4) should be in same community
    uint32_t v1_comm = partition->assignments[0];
    bool v1_together = true;
    for (uint32_t i = 1; i < 5; i++) {
        if (partition->assignments[i] != v1_comm) {
            v1_together = false;
            break;
        }
    }
    EXPECT_TRUE(v1_together) << "V1 region should cluster together";

    // A1 region (5-9) should be in same community
    uint32_t a1_comm = partition->assignments[5];
    bool a1_together = true;
    for (uint32_t i = 6; i < 10; i++) {
        if (partition->assignments[i] != a1_comm) {
            a1_together = false;
            break;
        }
    }
    EXPECT_TRUE(a1_together) << "A1 region should cluster together";

    // M1 region (10-14) should be in same community
    uint32_t m1_comm = partition->assignments[10];
    bool m1_together = true;
    for (uint32_t i = 11; i < 15; i++) {
        if (partition->assignments[i] != m1_comm) {
            m1_together = false;
            break;
        }
    }
    EXPECT_TRUE(m1_together) << "M1 region should cluster together";

    nimcp_community_partition_destroy(partition);
    nimcp_graph_destroy(graph);
}

TEST_F(BrainCommunityIntegrationTest, test_thalamus_as_hub)
{
    // WHAT: Thalamus should have high centrality (hub)
    // WHY: Thalamus connects all regions
    // HOW: Compute centrality, verify thalamus vertices rank high

    NimcpGraph* graph = create_brain_network();
    ASSERT_NE(nullptr, graph);

    NimcpCentralityScores* scores = nimcp_degree_centrality(graph);
    ASSERT_NE(nullptr, scores);

    // Thalamus vertices: 15, 16, 17
    double thalamus_centrality = (scores->scores[15] + scores->scores[16] + scores->scores[17]) / 3.0;

    // Average of other regions
    double cortex_centrality = 0.0;
    for (uint32_t i = 0; i < 15; i++) {
        cortex_centrality += scores->scores[i];
    }
    cortex_centrality /= 15.0;

    EXPECT_GT(thalamus_centrality, cortex_centrality)
        << "Thalamus should have higher centrality than cortex";

    nimcp_centrality_scores_destroy(scores);
    nimcp_graph_destroy(graph);
}

TEST_F(BrainCommunityIntegrationTest, test_hub_detection_identifies_thalamus)
{
    // WHAT: Hub detection should identify thalamus
    // WHY: Thalamus is natural hub in brain
    // HOW: Detect hubs, verify thalamus included

    NimcpGraph* graph = create_brain_network();
    ASSERT_NE(nullptr, graph);

    NimcpCentralityScores* scores = nimcp_degree_centrality(graph);
    ASSERT_NE(nullptr, scores);

    uint32_t hubs[MAX_VERTICES];
    uint32_t num_hubs = nimcp_detect_hubs(scores, 1.0, hubs, MAX_VERTICES);

    EXPECT_GT(num_hubs, 0u) << "Should detect at least one hub";

    // At least one thalamus vertex should be detected as hub
    bool thalamus_hub = false;
    for (uint32_t i = 0; i < num_hubs; i++) {
        if (hubs[i] >= 15 && hubs[i] < 18) {
            thalamus_hub = true;
            break;
        }
    }
    EXPECT_TRUE(thalamus_hub) << "Thalamus should be detected as hub";

    nimcp_centrality_scores_destroy(scores);
    nimcp_graph_destroy(graph);
}

TEST_F(BrainCommunityIntegrationTest, test_brain_topology_validity)
{
    // WHAT: Verify brain network structure is valid
    // WHY: Sanity check on generated network
    // HOW: Check vertex count, edge count, connectivity

    NimcpGraph* graph = create_brain_network();
    ASSERT_NE(nullptr, graph);

    EXPECT_EQ(18u, graph->vertex_count) << "Brain network should have 18 vertices";
    EXPECT_GT(graph->edge_count, 0u) << "Brain network should have edges";

    // All vertices should be connected (1 component)
    uint32_t components = nimcp_graph_update_components(graph);
    EXPECT_EQ(1u, components) << "Brain network should be fully connected";

    nimcp_graph_destroy(graph);
}

TEST_F(BrainCommunityIntegrationTest, test_brain_modularity_calculation)
{
    // WHAT: Calculate modularity of brain region partition
    // WHY: Quantify modular structure
    // HOW: Get Louvain partition, calculate Q

    NimcpGraph* graph = create_brain_network();
    ASSERT_NE(nullptr, graph);

    NimcpCommunityPartition* partition = nimcp_louvain_detect(graph, 1.0, 42);
    ASSERT_NE(nullptr, partition);

    // Verify modularity matches stored value
    double q_calculated = nimcp_calculate_modularity(graph, partition->assignments, graph->vertex_count);

    EXPECT_NEAR(q_calculated, partition->modularity, EPSILON)
        << "Stored modularity should match calculated value";

    EXPECT_GT(partition->modularity, 0.0) << "Brain network should have positive Q";

    nimcp_community_partition_destroy(partition);
    nimcp_graph_destroy(graph);
}

TEST_F(BrainCommunityIntegrationTest, test_refinement_preserves_structure)
{
    // WHAT: Refining partition shouldn't degrade community structure
    // WHY: Refinement should improve or maintain quality
    // HOW: Refine partition and verify community structure

    NimcpGraph* graph = create_brain_network();
    ASSERT_NE(nullptr, graph);

    NimcpCommunityPartition* partition = nimcp_louvain_detect(graph, 1.0, 42);
    ASSERT_NE(nullptr, partition);

    uint32_t original_communities = partition->num_communities;
    double original_modularity = partition->modularity;

    NimcpCommunityPartition* refined = nimcp_louvain_refine(graph, partition, 5);
    ASSERT_NE(nullptr, refined);

    // Number of communities should not increase dramatically
    EXPECT_LE(refined->num_communities, original_communities + 1)
        << "Refinement shouldn't create many new communities";

    // Modularity should not decrease
    EXPECT_GE(refined->modularity, original_modularity - EPSILON)
        << "Refinement shouldn't degrade modularity";

    nimcp_community_partition_destroy(partition);
    nimcp_community_partition_destroy(refined);
    nimcp_graph_destroy(graph);
}

//=============================================================================
// Test Summary
//=============================================================================

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
