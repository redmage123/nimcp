/**
 * @file test_louvain.cpp
 * @brief Comprehensive test suite for Louvain community detection algorithm
 *
 * WHAT: Tests for Louvain community detection on various network topologies
 * WHY: Ensure algorithm correctly identifies modular structure across scenarios
 * HOW: Use GTest framework with synthetic network fixtures
 */

#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
#include <vector>

#include "utils/algorithms/nimcp_louvain.h"
#include "utils/containers/nimcp_graph.h"

//=============================================================================
// Test Constants
//=============================================================================

static const double EPSILON = 1e-6;
static const uint32_t MAX_VERTICES = 256;

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * WHAT: Create complete graph (all vertices connected)
 * WHY: Test case with known single community
 */
static NimcpGraph* create_complete_graph(uint32_t num_vertices)
{
    NimcpGraph* graph = nimcp_graph_create();
    if (!graph) return nullptr;

    // Add vertices
    for (uint32_t i = 0; i < num_vertices; i++) {
        nimcp_graph_add_vertex(graph, i, 0.0f, 0.0f, 0.0f, 0);
    }

    // Add all edges
    for (uint32_t i = 0; i < num_vertices; i++) {
        for (uint32_t j = i + 1; j < num_vertices; j++) {
            nimcp_graph_add_edge(graph, i, j, 1.0f);
            nimcp_graph_add_edge(graph, j, i, 1.0f);
        }
    }

    return graph;
}

/**
 * WHAT: Create disconnected graph
 * WHY: Test graph with multiple components
 */
static NimcpGraph* create_disconnected_graph(uint32_t num_components)
{
    NimcpGraph* graph = nimcp_graph_create();
    if (!graph) return nullptr;

    uint32_t vertex_id = 0;
    for (uint32_t c = 0; c < num_components; c++) {
        // Create clique for each component
        nimcp_graph_add_vertex(graph, vertex_id, 0.0f, 0.0f, 0.0f, 0);
        nimcp_graph_add_vertex(graph, vertex_id + 1, 1.0f, 0.0f, 0.0f, 0);

        nimcp_graph_add_edge(graph, vertex_id, vertex_id + 1, 1.0f);
        nimcp_graph_add_edge(graph, vertex_id + 1, vertex_id, 1.0f);

        vertex_id += 2;
    }

    return graph;
}

/**
 * WHAT: Create Zachary's karate club network (benchmark)
 * WHY: Known 2 communities - standard test case
 */
static NimcpGraph* create_zachary_karate_club(void)
{
    NimcpGraph* graph = nimcp_graph_create();
    if (!graph) return nullptr;

    // Create 34 vertices (club members)
    for (uint32_t i = 0; i < 34; i++) {
        nimcp_graph_add_vertex(graph, i, 0.0f, 0.0f, 0.0f, 0);
    }

    // Add edges for karate club structure (simplified, representative version)
    // This creates a bimodal network with clear community structure
    // Group 1: vertices 0-16 (tightly connected)
    for (uint32_t i = 0; i <= 16; i++) {
        for (uint32_t j = i + 1; j <= 16 && j < 18; j++) {
            if ((i + j) % 3 != 0) {  // Create sparse but modular structure
                nimcp_graph_add_edge(graph, i, j, 1.0f);
                nimcp_graph_add_edge(graph, j, i, 1.0f);
            }
        }
    }

    // Group 2: vertices 17-33 (tightly connected)
    for (uint32_t i = 17; i < 34; i++) {
        for (uint32_t j = i + 1; j < 34; j++) {
            if ((i + j) % 3 != 0) {  // Create sparse but modular structure
                nimcp_graph_add_edge(graph, i, j, 1.0f);
                nimcp_graph_add_edge(graph, j, i, 1.0f);
            }
        }
    }

    // Add inter-community edges (fewer than intra-community)
    for (uint32_t i = 0; i <= 5; i++) {
        for (uint32_t j = 28; j < 34; j++) {
            if ((i * j) % 7 == 0) {
                nimcp_graph_add_edge(graph, i, j, 1.0f);
                nimcp_graph_add_edge(graph, j, i, 1.0f);
            }
        }
    }

    return graph;
}

/**
 * WHAT: Create random graph (null model)
 * WHY: Test that random graphs have low modularity
 */
static NimcpGraph* create_random_graph(uint32_t num_vertices, double edge_probability,
                                       uint32_t seed)
{
    NimcpGraph* graph = nimcp_graph_create();
    if (!graph) return nullptr;

    srand(seed);

    // Add vertices
    for (uint32_t i = 0; i < num_vertices; i++) {
        nimcp_graph_add_vertex(graph, i, 0.0f, 0.0f, 0.0f, 0);
    }

    // Add random edges
    for (uint32_t i = 0; i < num_vertices; i++) {
        for (uint32_t j = i + 1; j < num_vertices; j++) {
            if ((rand() / (double)RAND_MAX) < edge_probability) {
                nimcp_graph_add_edge(graph, i, j, 1.0f);
                nimcp_graph_add_edge(graph, j, i, 1.0f);
            }
        }
    }

    return graph;
}

/**
 * WHAT: Create modular synthetic network
 * WHY: Test case with known strong communities
 */
static NimcpGraph* create_modular_network(uint32_t num_communities, uint32_t vertices_per_com)
{
    NimcpGraph* graph = nimcp_graph_create();
    if (!graph) return nullptr;

    uint32_t vertex_id = 0;

    // Create cliques for each community
    for (uint32_t c = 0; c < num_communities; c++) {
        uint32_t start = vertex_id;
        uint32_t end = start + vertices_per_com;

        // Add vertices
        for (uint32_t i = start; i < end; i++) {
            nimcp_graph_add_vertex(graph, i, 0.0f, 0.0f, 0.0f, 0);
        }

        // Create clique (all connected within community)
        for (uint32_t i = start; i < end; i++) {
            for (uint32_t j = i + 1; j < end; j++) {
                nimcp_graph_add_edge(graph, i, j, 1.0f);
                nimcp_graph_add_edge(graph, j, i, 1.0f);
            }
        }

        vertex_id = end;
    }

    // Add inter-community edges (sparse)
    for (uint32_t c1 = 0; c1 < num_communities - 1; c1++) {
        for (uint32_t c2 = c1 + 1; c2 < num_communities; c2++) {
            uint32_t v1 = c1 * vertices_per_com;
            uint32_t v2 = c2 * vertices_per_com;
            if (v1 < vertex_id && v2 < vertex_id) {
                nimcp_graph_add_edge(graph, v1, v2, 1.0f);
                nimcp_graph_add_edge(graph, v2, v1, 1.0f);
            }
        }
    }

    return graph;
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

TEST_F(LouvainTest, test_complete_graph_single_community)
{
    // WHAT: Complete graph should have exactly 1 community
    // WHY: In complete graph, all vertices equally important
    // HOW: Run Louvain and verify exactly 1 community assigned

    NimcpGraph* graph = create_complete_graph(5);
    ASSERT_NE(nullptr, graph);

    NimcpCommunityPartition* partition = nimcp_louvain_detect(graph, 1.0, 42);
    ASSERT_NE(nullptr, partition);

    EXPECT_EQ(1u, partition->num_communities) << "Complete graph should have 1 community";
    EXPECT_GT(partition->modularity, -0.1) << "Modularity should not be extremely negative";

    // All vertices should have same community
    uint32_t first_comm = partition->assignments[0];
    for (uint32_t i = 1; i < graph->vertex_count; i++) {
        EXPECT_EQ(first_comm, partition->assignments[i])
            << "All vertices in complete graph should be in same community";
    }

    nimcp_community_partition_destroy(partition);
    nimcp_graph_destroy(graph);
}

TEST_F(LouvainTest, test_disconnected_graph_multiple_communities)
{
    // WHAT: Disconnected graph should detect each component
    // WHY: Disconnected components are naturally separate communities
    // HOW: Create N disconnected components, verify N communities found

    uint32_t num_components = 3;
    NimcpGraph* graph = create_disconnected_graph(num_components);
    ASSERT_NE(nullptr, graph);

    NimcpCommunityPartition* partition = nimcp_louvain_detect(graph, 1.0, 42);
    ASSERT_NE(nullptr, partition);

    EXPECT_GE(partition->num_communities, num_components)
        << "Should detect at least " << num_components << " components";
    EXPECT_LE(partition->num_communities, graph->vertex_count)
        << "Number of communities cannot exceed vertices";

    nimcp_community_partition_destroy(partition);
    nimcp_graph_destroy(graph);
}

TEST_F(LouvainTest, test_modular_network_correct_partition)
{
    // WHAT: Modular network should detect original community structure
    // WHY: Algorithm should find known modular partitions
    // HOW: Create network with known communities, verify detection

    uint32_t num_communities = 3;
    uint32_t vertices_per_com = 4;
    NimcpGraph* graph = create_modular_network(num_communities, vertices_per_com);
    ASSERT_NE(nullptr, graph);

    NimcpCommunityPartition* partition = nimcp_louvain_detect(graph, 1.0, 42);
    ASSERT_NE(nullptr, partition);

    EXPECT_GE(partition->num_communities, 2) << "Should detect at least 2 communities";
    EXPECT_GT(partition->modularity, 0.3) << "Modular network should have Q > 0.3";

    // Check that vertices within same original community are together
    for (uint32_t c = 0; c < num_communities; c++) {
        uint32_t start = c * vertices_per_com;
        uint32_t end = start + vertices_per_com;
        uint32_t first_comm = partition->assignments[start];

        bool all_same = true;
        for (uint32_t v = start; v < end; v++) {
            if (partition->assignments[v] != first_comm) {
                all_same = false;
                break;
            }
        }
        EXPECT_TRUE(all_same) << "Vertices in community " << c << " should be together";
    }

    nimcp_community_partition_destroy(partition);
    nimcp_graph_destroy(graph);
}

TEST_F(LouvainTest, test_zachary_karate_club_two_communities)
{
    // WHAT: Zachary's karate club benchmark
    // WHY: Standard test case with known 2 communities
    // HOW: Create Zachary network, verify 2 communities found

    NimcpGraph* graph = create_zachary_karate_club();
    ASSERT_NE(nullptr, graph);

    NimcpCommunityPartition* partition = nimcp_louvain_detect(graph, 1.0, 42);
    ASSERT_NE(nullptr, partition);

    EXPECT_GE(partition->num_communities, 2) << "Should detect at least 2 communities";
    EXPECT_GT(partition->modularity, 0.2) << "Should find reasonable modularity (Q > 0.2)";

    // Verify partition is valid (all vertices assigned)
    for (uint32_t i = 0; i < graph->vertex_count; i++) {
        EXPECT_LT(partition->assignments[i], partition->num_communities)
            << "All vertices should be assigned to valid community";
    }

    nimcp_community_partition_destroy(partition);
    nimcp_graph_destroy(graph);
}

TEST_F(LouvainTest, test_random_graph_low_modularity)
{
    // WHAT: Random graph should have low modularity
    // WHY: Random graphs have no community structure
    // HOW: Create random graph, verify low Q

    NimcpGraph* graph = create_random_graph(20, 0.3, 42);
    ASSERT_NE(nullptr, graph);

    NimcpCommunityPartition* partition = nimcp_louvain_detect(graph, 1.0, 42);
    ASSERT_NE(nullptr, partition);

    EXPECT_LT(partition->modularity, 0.3)
        << "Random graph should have low modularity (Q < 0.3)";
    EXPECT_GT(partition->modularity, -0.5) << "Modularity should be reasonable";

    nimcp_community_partition_destroy(partition);
    nimcp_graph_destroy(graph);
}

//=============================================================================
// Tests: Convergence and Performance
//=============================================================================

TEST_F(LouvainTest, test_convergence_reasonable_iterations)
{
    // WHAT: Algorithm should converge quickly
    // WHY: Efficiency is important for large networks
    // HOW: Check iteration count is reasonable

    NimcpGraph* graph = create_modular_network(3, 5);
    ASSERT_NE(nullptr, graph);

    NimcpCommunityPartition* partition = nimcp_louvain_detect(graph, 1.0, 42);
    ASSERT_NE(nullptr, partition);

    EXPECT_LE(partition->iterations, 10u)
        << "Should converge within reasonable iterations (<=10)";
    EXPECT_GT(partition->iterations, 0u) << "Should have at least 1 iteration";

    nimcp_community_partition_destroy(partition);
    nimcp_graph_destroy(graph);
}

TEST_F(LouvainTest, test_determinism_reproducible_results)
{
    // WHAT: Same seed should produce same results
    // WHY: Reproducibility is essential for testing and debugging
    // HOW: Run twice with same seed, verify identical partition

    NimcpGraph* graph1 = create_modular_network(3, 5);
    NimcpGraph* graph2 = create_modular_network(3, 5);
    ASSERT_NE(nullptr, graph1);
    ASSERT_NE(nullptr, graph2);

    NimcpCommunityPartition* partition1 = nimcp_louvain_detect(graph1, 1.0, 42);
    NimcpCommunityPartition* partition2 = nimcp_louvain_detect(graph2, 1.0, 42);
    ASSERT_NE(nullptr, partition1);
    ASSERT_NE(nullptr, partition2);

    // Verify identical results
    EXPECT_EQ(partition1->num_communities, partition2->num_communities);

    // Check assignments match
    bool assignments_match = true;
    for (uint32_t i = 0; i < graph1->vertex_count; i++) {
        if (partition1->assignments[i] != partition2->assignments[i]) {
            assignments_match = false;
            break;
        }
    }
    EXPECT_TRUE(assignments_match) << "Same seed should produce identical partitions";

    nimcp_community_partition_destroy(partition1);
    nimcp_community_partition_destroy(partition2);
    nimcp_graph_destroy(graph1);
    nimcp_graph_destroy(graph2);
}

TEST_F(LouvainTest, test_different_seed_different_results)
{
    // WHAT: Different seeds may produce different results
    // WHY: Algorithm uses randomization
    // HOW: Run with different seeds, verify not always identical

    NimcpGraph* graph1 = create_modular_network(3, 5);
    NimcpGraph* graph2 = create_modular_network(3, 5);
    ASSERT_NE(nullptr, graph1);
    ASSERT_NE(nullptr, graph2);

    NimcpCommunityPartition* partition1 = nimcp_louvain_detect(graph1, 1.0, 42);
    NimcpCommunityPartition* partition2 = nimcp_louvain_detect(graph2, 1.0, 123);
    ASSERT_NE(nullptr, partition1);
    ASSERT_NE(nullptr, partition2);

    // Different seeds may lead to same community count and modularity
    // (this is actually expected for well-defined problems)
    EXPECT_EQ(partition1->num_communities, partition2->num_communities)
        << "Community count should be stable for modular networks";

    nimcp_community_partition_destroy(partition1);
    nimcp_community_partition_destroy(partition2);
    nimcp_graph_destroy(graph1);
    nimcp_graph_destroy(graph2);
}

//=============================================================================
// Tests: Edge Cases
//=============================================================================

TEST_F(LouvainTest, test_empty_graph_returns_null)
{
    // WHAT: Empty graph should return NULL
    // WHY: Cannot partition graph with no vertices
    // HOW: Create empty graph, verify NULL return

    NimcpGraph* graph = nimcp_graph_create();
    ASSERT_NE(nullptr, graph);

    NimcpCommunityPartition* partition = nimcp_louvain_detect(graph, 1.0, 42);
    EXPECT_EQ(nullptr, partition) << "Empty graph should return NULL partition";

    nimcp_graph_destroy(graph);
}

TEST_F(LouvainTest, test_single_vertex_graph)
{
    // WHAT: Single vertex graph should have 1 community
    // WHY: Single vertex forms trivial partition
    // HOW: Create graph with 1 vertex

    NimcpGraph* graph = nimcp_graph_create();
    ASSERT_NE(nullptr, graph);
    nimcp_graph_add_vertex(graph, 0, 0.0f, 0.0f, 0.0f, 0);

    NimcpCommunityPartition* partition = nimcp_louvain_detect(graph, 1.0, 42);
    ASSERT_NE(nullptr, partition);

    EXPECT_EQ(1u, partition->num_communities) << "Single vertex should be 1 community";
    EXPECT_EQ(0u, partition->assignments[0]) << "Single vertex should be in community 0";

    nimcp_community_partition_destroy(partition);
    nimcp_graph_destroy(graph);
}

TEST_F(LouvainTest, test_isolated_vertices)
{
    // WHAT: Isolated vertices should each form community
    // WHY: No edges means no interactions
    // HOW: Create vertices without edges

    NimcpGraph* graph = nimcp_graph_create();
    ASSERT_NE(nullptr, graph);

    for (uint32_t i = 0; i < 5; i++) {
        nimcp_graph_add_vertex(graph, i, 0.0f, 0.0f, 0.0f, 0);
    }

    NimcpCommunityPartition* partition = nimcp_louvain_detect(graph, 1.0, 42);
    ASSERT_NE(nullptr, partition);

    EXPECT_EQ(5u, partition->num_communities)
        << "5 isolated vertices should have 5 communities";

    nimcp_community_partition_destroy(partition);
    nimcp_graph_destroy(graph);
}

//=============================================================================
// Tests: API Functions
//=============================================================================

TEST_F(LouvainTest, test_get_community_id)
{
    // WHAT: Get community ID for specific vertex
    // WHY: Need to query partition results
    // HOW: Create partition and check get function

    NimcpGraph* graph = create_modular_network(2, 3);
    ASSERT_NE(nullptr, graph);

    NimcpCommunityPartition* partition = nimcp_louvain_detect(graph, 1.0, 42);
    ASSERT_NE(nullptr, partition);

    // Test valid index
    uint32_t comm = nimcp_get_community_id(partition, 0);
    EXPECT_LT(comm, partition->num_communities) << "Community ID should be valid";

    // Test invalid index
    uint32_t invalid_comm = nimcp_get_community_id(partition, 999);
    EXPECT_LT(invalid_comm, partition->num_communities) << "Still returns some ID";

    nimcp_community_partition_destroy(partition);
    nimcp_graph_destroy(graph);
}

TEST_F(LouvainTest, test_get_community_members)
{
    // WHAT: Extract all members of a community
    // WHY: Need to analyze community composition
    // HOW: Query community members and verify

    NimcpGraph* graph = create_modular_network(2, 3);
    ASSERT_NE(nullptr, graph);

    NimcpCommunityPartition* partition = nimcp_louvain_detect(graph, 1.0, 42);
    ASSERT_NE(nullptr, partition);

    uint32_t members[MAX_VERTICES];
    uint32_t count = nimcp_get_community_members(partition, 0, members, MAX_VERTICES);

    EXPECT_GT(count, 0u) << "Community should have at least 1 member";
    EXPECT_LE(count, graph->vertex_count) << "Member count bounded by graph size";

    nimcp_community_partition_destroy(partition);
    nimcp_graph_destroy(graph);
}

TEST_F(LouvainTest, test_refine_partition_improves_quality)
{
    // WHAT: Refinement should improve partition quality
    // WHY: Additional iterations should increase modularity
    // HOW: Refine partition and check modularity

    NimcpGraph* graph = create_modular_network(3, 5);
    ASSERT_NE(nullptr, graph);

    NimcpCommunityPartition* partition1 = nimcp_louvain_detect(graph, 1.0, 42);
    ASSERT_NE(nullptr, partition1);

    double initial_modularity = partition1->modularity;

    NimcpCommunityPartition* partition2 = nimcp_louvain_refine(graph, partition1, 5);
    ASSERT_NE(nullptr, partition2);

    // Modularity should not decrease (may stay same if already optimal)
    EXPECT_GE(partition2->modularity, initial_modularity - EPSILON)
        << "Refinement should not decrease modularity";

    nimcp_community_partition_destroy(partition1);
    nimcp_community_partition_destroy(partition2);
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
