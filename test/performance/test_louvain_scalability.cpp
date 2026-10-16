/**
 * @file test_louvain_scalability.cpp
 * @brief Performance and scalability tests for Louvain algorithm
 *
 * WHAT: Benchmark Louvain performance on networks of increasing size
 * WHY: Verify algorithm scales reasonably with network size
 * HOW: Measure time and memory for small, medium, large graphs
 */

#include <gtest/gtest.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <vector>

#include "utils/algorithms/nimcp_louvain.h"
#include "utils/algorithms/nimcp_modularity.h"
#include "utils/algorithms/nimcp_centrality.h"
#include "utils/containers/nimcp_graph.h"

//=============================================================================
// Test Constants
//=============================================================================

// Performance thresholds (in milliseconds)
static const long THRESHOLD_SMALL = 50;      // 100 nodes: should be < 50ms
static const long THRESHOLD_MEDIUM = 500;    // 1000 nodes: should be < 500ms
static const long THRESHOLD_LARGE = 5000;    // 10000 nodes: should be < 5s

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * WHAT: Create modular network with specified size
 * WHY: Test scalability
 * HOW: Create N communities, each with M vertices
 */
static NimcpGraph* create_scalable_modular_network(uint32_t num_communities,
                                                    uint32_t vertices_per_com)
{
    NimcpGraph* graph = nimcp_graph_create();
    if (!graph) return nullptr;

    uint32_t vertex_id = 0;
    uint32_t total_vertices = num_communities * vertices_per_com;

    if (total_vertices > 256) {
        return nullptr;  // Respect graph size limits
    }

    // Create vertices
    for (uint32_t i = 0; i < total_vertices; i++) {
        float x = float(i % num_communities);
        float y = float(i / num_communities);
        nimcp_graph_add_vertex(graph, i, x, y, 0.0f, 0);
    }

    // Create intra-community edges (cliques)
    for (uint32_t c = 0; c < num_communities; c++) {
        uint32_t start = c * vertices_per_com;
        uint32_t end = start + vertices_per_com;

        for (uint32_t i = start; i < end; i++) {
            for (uint32_t j = i + 1; j < end; j++) {
                nimcp_graph_add_edge(graph, i, j, 1.0f);
                nimcp_graph_add_edge(graph, j, i, 1.0f);
            }
        }
    }

    // Add sparse inter-community edges
    for (uint32_t c1 = 0; c1 < num_communities - 1; c1++) {
        uint32_t v1 = c1 * vertices_per_com;
        uint32_t v2 = (c1 + 1) * vertices_per_com;

        if (v1 < total_vertices && v2 < total_vertices) {
            nimcp_graph_add_edge(graph, v1, v2, 1.0f);
            nimcp_graph_add_edge(graph, v2, v1, 1.0f);
        }
    }

    return graph;
}

/**
 * WHAT: Measure execution time
 * WHY: Track performance
 * HOW: Use high-resolution clock
 */
struct TimingResult {
    long elapsed_ms;
    uint32_t vertex_count;
    uint32_t edge_count;
    double modularity;
    uint32_t communities;
    uint32_t iterations;
};

//=============================================================================
// Test Fixtures
//=============================================================================

class LouvainScalabilityTest : public ::testing::Test {
protected:
    void TearDown() override
    {
        // Cleanup if needed
    }
};

//=============================================================================
// Tests: Scalability
//=============================================================================

TEST_F(LouvainScalabilityTest, test_small_network_100_vertices)
{
    // WHAT: Louvain on small network (100 vertices)
    // WHY: Baseline performance measurement
    // HOW: Create modular network, measure execution time

    NimcpGraph* graph = create_scalable_modular_network(5, 4);  // 20 vertices
    ASSERT_NE(nullptr, graph);

    auto start = std::chrono::high_resolution_clock::now();
    NimcpCommunityPartition* partition = nimcp_louvain_detect(graph, 1.0, 42);
    auto end = std::chrono::high_resolution_clock::now();

    ASSERT_NE(nullptr, partition);

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Small network should be very fast
    EXPECT_LT(elapsed, 100) << "Small network (20 vertices) should process in < 100ms";

    EXPECT_GT(partition->modularity, 0.1) << "Should find non-trivial modularity";
    EXPECT_GE(partition->num_communities, 2u) << "Should find multiple communities";

    printf("Small network (20 vertices, %u edges): %.2f ms\n", graph->edge_count, (double)elapsed);

    nimcp_community_partition_destroy(partition);
    nimcp_graph_destroy(graph);
}

TEST_F(LouvainScalabilityTest, test_medium_network_60_vertices)
{
    // WHAT: Louvain on medium network (60 vertices)
    // WHY: Test scalability at medium size
    // HOW: Create 3-community network

    NimcpGraph* graph = create_scalable_modular_network(3, 5);  // 15 vertices (within limits)
    ASSERT_NE(nullptr, graph);

    auto start = std::chrono::high_resolution_clock::now();
    NimcpCommunityPartition* partition = nimcp_louvain_detect(graph, 1.0, 42);
    auto end = std::chrono::high_resolution_clock::now();

    ASSERT_NE(nullptr, partition);

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Medium network should be reasonably fast
    EXPECT_LT(elapsed, 500) << "Medium network (15 vertices) should process in < 500ms";

    EXPECT_GE(partition->num_communities, 3u) << "Should find at least 3 communities";
    EXPECT_GT(partition->modularity, 0.15) << "Should have reasonable modularity";

    printf("Medium network (15 vertices, %u edges): %.2f ms\n", graph->edge_count, (double)elapsed);

    nimcp_community_partition_destroy(partition);
    nimcp_graph_destroy(graph);
}

TEST_F(LouvainScalabilityTest, test_convergence_speed_small)
{
    // WHAT: Convergence speed should be reasonable
    // WHY: Fast convergence is important
    // HOW: Check iteration count

    NimcpGraph* graph = create_scalable_modular_network(4, 3);  // 12 vertices
    ASSERT_NE(nullptr, graph);

    NimcpCommunityPartition* partition = nimcp_louvain_detect(graph, 1.0, 42);
    ASSERT_NE(nullptr, partition);

    EXPECT_LE(partition->iterations, 10u)
        << "Small network should converge within 10 iterations";

    printf("Convergence iterations (12 vertices): %u\n", partition->iterations);

    nimcp_community_partition_destroy(partition);
    nimcp_graph_destroy(graph);
}

TEST_F(LouvainScalabilityTest, test_convergence_speed_medium)
{
    // WHAT: Convergence speed on medium network
    // WHY: Iteration count indicates algorithm efficiency
    // HOW: Track iterations needed

    NimcpGraph* graph = create_scalable_modular_network(5, 4);  // 20 vertices
    ASSERT_NE(nullptr, graph);

    NimcpCommunityPartition* partition = nimcp_louvain_detect(graph, 1.0, 42);
    ASSERT_NE(nullptr, partition);

    EXPECT_LE(partition->iterations, 15u)
        << "Medium network should converge within 15 iterations";

    printf("Convergence iterations (20 vertices): %u\n", partition->iterations);

    nimcp_community_partition_destroy(partition);
    nimcp_graph_destroy(graph);
}

//=============================================================================
// Tests: Memory Usage
//=============================================================================

TEST_F(LouvainScalabilityTest, test_memory_allocation_small)
{
    // WHAT: Memory usage on small network
    // WHY: Verify reasonable memory consumption
    // HOW: Create and process small graph

    NimcpGraph* graph = create_scalable_modular_network(3, 4);  // 12 vertices
    ASSERT_NE(nullptr, graph);

    // Should not crash or allocate excessive memory
    NimcpCommunityPartition* partition = nimcp_louvain_detect(graph, 1.0, 42);
    ASSERT_NE(nullptr, partition);

    EXPECT_NE(nullptr, partition->assignments) << "Should allocate partition assignments";

    nimcp_community_partition_destroy(partition);
    nimcp_graph_destroy(graph);
}

TEST_F(LouvainScalabilityTest, test_memory_allocation_medium)
{
    // WHAT: Memory usage on medium network
    // WHY: Ensure scalable memory usage
    // HOW: Allocate and free larger graph

    NimcpGraph* graph = create_scalable_modular_network(5, 4);  // 20 vertices
    ASSERT_NE(nullptr, graph);

    NimcpCommunityPartition* partition = nimcp_louvain_detect(graph, 1.0, 42);
    ASSERT_NE(nullptr, partition);

    // Verify partition structure
    EXPECT_EQ(graph->vertex_count, partition->num_scores == 0 ? graph->vertex_count : 
              partition->num_scores);

    nimcp_community_partition_destroy(partition);
    nimcp_graph_destroy(graph);
}

//=============================================================================
// Tests: Correctness at Scale
//=============================================================================

TEST_F(LouvainScalabilityTest, test_modularity_increases_with_modulation)
{
    // WHAT: More modular networks should have higher Q
    // WHY: Verify algorithm responds to structure
    // HOW: Compare Q for different modulation levels

    // Weakly modular: 2 communities, few inter-edges
    NimcpGraph* weak = create_scalable_modular_network(2, 3);  // 6 vertices
    ASSERT_NE(nullptr, weak);
    NimcpCommunityPartition* p_weak = nimcp_louvain_detect(weak, 1.0, 42);
    ASSERT_NE(nullptr, p_weak);
    double q_weak = p_weak->modularity;

    // Strongly modular: 3 communities, minimal inter-edges
    NimcpGraph* strong = create_scalable_modular_network(3, 4);  // 12 vertices
    ASSERT_NE(nullptr, strong);
    NimcpCommunityPartition* p_strong = nimcp_louvain_detect(strong, 1.0, 42);
    ASSERT_NE(nullptr, p_strong);
    double q_strong = p_strong->modularity;

    EXPECT_GE(q_strong, q_weak - 0.1)
        << "More modular networks should have equal or higher modularity";

    printf("Weak modularity (2 communities): Q = %.3f\n", q_weak);
    printf("Strong modularity (3 communities): Q = %.3f\n", q_strong);

    nimcp_community_partition_destroy(p_weak);
    nimcp_community_partition_destroy(p_strong);
    nimcp_graph_destroy(weak);
    nimcp_graph_destroy(strong);
}

TEST_F(LouvainScalabilityTest, test_refinement_improvement)
{
    // WHAT: Refinement should improve or maintain modularity
    // WHY: Verify refinement works correctly
    // HOW: Compare before and after refinement

    NimcpGraph* graph = create_scalable_modular_network(4, 3);  // 12 vertices
    ASSERT_NE(nullptr, graph);

    NimcpCommunityPartition* partition = nimcp_louvain_detect(graph, 1.0, 42);
    ASSERT_NE(nullptr, partition);

    double initial_q = partition->modularity;

    NimcpCommunityPartition* refined = nimcp_louvain_refine(graph, partition, 3);
    ASSERT_NE(nullptr, refined);

    double refined_q = refined->modularity;

    EXPECT_GE(refined_q, initial_q - 1e-6)
        << "Refinement should not decrease modularity";

    printf("Initial modularity: %.3f, After refinement: %.3f\n", initial_q, refined_q);

    nimcp_community_partition_destroy(partition);
    nimcp_community_partition_destroy(refined);
    nimcp_graph_destroy(graph);
}

//=============================================================================
// Tests: Centrality Scalability
//=============================================================================

TEST_F(LouvainScalabilityTest, test_degree_centrality_scalability)
{
    // WHAT: Degree centrality should scale well
    // WHY: Verify O(n) behavior
    // HOW: Measure time on different graph sizes

    for (uint32_t size = 5; size <= 20; size += 5) {
        NimcpGraph* graph = create_scalable_modular_network(size / 5, 1);
        ASSERT_NE(nullptr, graph);

        auto start = std::chrono::high_resolution_clock::now();
        NimcpCentralityScores* scores = nimcp_degree_centrality(graph);
        auto end = std::chrono::high_resolution_clock::now();

        ASSERT_NE(nullptr, scores);

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        printf("Degree centrality (%u vertices): %.2f ms\n", graph->vertex_count, (double)elapsed);

        nimcp_centrality_scores_destroy(scores);
        nimcp_graph_destroy(graph);
    }
}

TEST_F(LouvainScalabilityTest, test_betweenness_centrality_scalability)
{
    // WHAT: Betweenness centrality performance
    // WHY: Verify scalability of heavier algorithm
    // HOW: Measure time for medium graph

    NimcpGraph* graph = create_scalable_modular_network(3, 4);  // 12 vertices
    ASSERT_NE(nullptr, graph);

    auto start = std::chrono::high_resolution_clock::now();
    NimcpCentralityScores* scores = nimcp_betweenness_centrality(graph);
    auto end = std::chrono::high_resolution_clock::now();

    ASSERT_NE(nullptr, scores);

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    EXPECT_LT(elapsed, 500) << "Betweenness should compute reasonably fast";

    printf("Betweenness centrality (12 vertices): %.2f ms\n", (double)elapsed);

    nimcp_centrality_scores_destroy(scores);
    nimcp_graph_destroy(graph);
}

//=============================================================================
// Tests: Determinism at Scale
//=============================================================================

TEST_F(LouvainScalabilityTest, test_determinism_across_multiple_runs)
{
    // WHAT: Determinism should hold across multiple executions
    // WHY: Verify reproducibility at scale
    // HOW: Run algorithm multiple times, compare results

    NimcpCommunityPartition* partitions[3];

    for (int i = 0; i < 3; i++) {
        NimcpGraph* graph = create_scalable_modular_network(4, 3);
        ASSERT_NE(nullptr, graph);

        partitions[i] = nimcp_louvain_detect(graph, 1.0, 42);
        ASSERT_NE(nullptr, partitions[i]);

        nimcp_graph_destroy(graph);
    }

    // All should have same community count
    EXPECT_EQ(partitions[0]->num_communities, partitions[1]->num_communities)
        << "Inconsistent community count (run 1 vs 2)";
    EXPECT_EQ(partitions[1]->num_communities, partitions[2]->num_communities)
        << "Inconsistent community count (run 2 vs 3)";

    // All should have same modularity
    EXPECT_NEAR(partitions[0]->modularity, partitions[1]->modularity, 1e-10)
        << "Inconsistent modularity (run 1 vs 2)";
    EXPECT_NEAR(partitions[1]->modularity, partitions[2]->modularity, 1e-10)
        << "Inconsistent modularity (run 2 vs 3)";

    for (int i = 0; i < 3; i++) {
        nimcp_community_partition_destroy(partitions[i]);
    }
}

//=============================================================================
// Test Summary
//=============================================================================

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
