/**
 * @file test_community_detection_regression.cpp
 * @brief Regression tests for community detection algorithms
 *
 * WHAT: Baseline tests ensuring algorithm behavior doesn't change
 * WHY: Prevent unintended algorithm modifications
 * HOW: Store baseline results, compare against current implementation
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
// Test Constants - Baseline Values
//=============================================================================

// Expected modularity baseline for modular network
static const double BASELINE_MODULAR_Q = 0.35;
static const double Q_TOLERANCE = 0.1;  // Allow ±0.1 variation

// Expected hub count for star graph
static const uint32_t BASELINE_STAR_HUBS = 1;

// Expected degree centrality for star hub
static const double BASELINE_STAR_HUB_CENTRALITY = 1.0;  // Normalized to 1.0

// Expected iterations to convergence
static const uint32_t BASELINE_MAX_ITERATIONS = 10;

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * WHAT: Create modular test network (consistent across runs)
 * WHY: Ensure baseline uses same network structure
 * HOW: Deterministic network generation
 */
static NimcpGraph* create_regression_network(void)
{
    NimcpGraph* graph = nimcp_graph_create();
    if (!graph) return nullptr;

    // Community 1: vertices 0-4
    for (uint32_t i = 0; i < 5; i++) {
        nimcp_graph_add_vertex(graph, i, 0.0f, 0.0f, 0.0f, 0);
    }
    for (uint32_t i = 0; i < 5; i++) {
        for (uint32_t j = i + 1; j < 5; j++) {
            nimcp_graph_add_edge(graph, i, j, 1.0f);
            nimcp_graph_add_edge(graph, j, i, 1.0f);
        }
    }

    // Community 2: vertices 5-9
    for (uint32_t i = 5; i < 10; i++) {
        nimcp_graph_add_vertex(graph, i, 1.0f, 0.0f, 0.0f, 0);
    }
    for (uint32_t i = 5; i < 10; i++) {
        for (uint32_t j = i + 1; j < 10; j++) {
            nimcp_graph_add_edge(graph, i, j, 1.0f);
            nimcp_graph_add_edge(graph, j, i, 1.0f);
        }
    }

    // Community 3: vertices 10-14
    for (uint32_t i = 10; i < 15; i++) {
        nimcp_graph_add_vertex(graph, i, 2.0f, 0.0f, 0.0f, 0);
    }
    for (uint32_t i = 10; i < 15; i++) {
        for (uint32_t j = i + 1; j < 15; j++) {
            nimcp_graph_add_edge(graph, i, j, 1.0f);
            nimcp_graph_add_edge(graph, j, i, 1.0f);
        }
    }

    // Inter-community edges (sparse)
    nimcp_graph_add_edge(graph, 0, 5, 1.0f);
    nimcp_graph_add_edge(graph, 5, 0, 1.0f);
    nimcp_graph_add_edge(graph, 5, 10, 1.0f);
    nimcp_graph_add_edge(graph, 10, 5, 1.0f);

    return graph;
}

/**
 * WHAT: Create star graph (hub with leaves)
 * WHY: Simple baseline for centrality tests
 * HOW: One central vertex connected to all others
 */
static NimcpGraph* create_regression_star(uint32_t num_leaves)
{
    NimcpGraph* graph = nimcp_graph_create();
    if (!graph) return nullptr;

    nimcp_graph_add_vertex(graph, 0, 0.0f, 0.0f, 0.0f, 0);

    for (uint32_t i = 1; i <= num_leaves; i++) {
        nimcp_graph_add_vertex(graph, i, float(i), 0.0f, 0.0f, 0);
        nimcp_graph_add_edge(graph, 0, i, 1.0f);
        nimcp_graph_add_edge(graph, i, 0, 1.0f);
    }

    return graph;
}

//=============================================================================
// Test Fixtures
//=============================================================================

class CommunityDetectionRegressionTest : public ::testing::Test {
protected:
    void TearDown() override
    {
        // Cleanup if needed
    }
};

//=============================================================================
// Regression Tests: Louvain Algorithm
//=============================================================================

TEST_F(CommunityDetectionRegressionTest, test_louvain_modular_network_baseline)
{
    // WHAT: Louvain on modular network should produce consistent results
    // WHY: Detect algorithm regressions
    // HOW: Compare modularity against baseline

    NimcpGraph* graph = create_regression_network();
    ASSERT_NE(nullptr, graph);

    NimcpCommunityPartition* partition = nimcp_louvain_detect(graph, 1.0, 42);
    ASSERT_NE(nullptr, partition);

    // Check modularity is in expected range (±tolerance)
    EXPECT_GE(partition->modularity, BASELINE_MODULAR_Q - Q_TOLERANCE)
        << "Modularity decreased below acceptable range";
    EXPECT_LE(partition->modularity, BASELINE_MODULAR_Q + Q_TOLERANCE)
        << "Modularity changed unexpectedly";

    // Should find at least 3 communities
    EXPECT_GE(partition->num_communities, 3u) << "Community count regression";

    // Convergence should be reasonable
    EXPECT_LE(partition->iterations, BASELINE_MAX_ITERATIONS)
        << "Convergence degraded (more iterations needed)";

    nimcp_community_partition_destroy(partition);
    nimcp_graph_destroy(graph);
}

TEST_F(CommunityDetectionRegressionTest, test_louvain_determinism_regression)
{
    // WHAT: Same seed always produces same results
    // WHY: Detect non-determinism bugs
    // HOW: Run 3 times with same seed, verify identical

    NimcpGraph* graph1 = create_regression_network();
    NimcpGraph* graph2 = create_regression_network();
    NimcpGraph* graph3 = create_regression_network();
    ASSERT_NE(nullptr, graph1);
    ASSERT_NE(nullptr, graph2);
    ASSERT_NE(nullptr, graph3);

    NimcpCommunityPartition* p1 = nimcp_louvain_detect(graph1, 1.0, 42);
    NimcpCommunityPartition* p2 = nimcp_louvain_detect(graph2, 1.0, 42);
    NimcpCommunityPartition* p3 = nimcp_louvain_detect(graph3, 1.0, 42);
    ASSERT_NE(nullptr, p1);
    ASSERT_NE(nullptr, p2);
    ASSERT_NE(nullptr, p3);

    // All should have same community count
    EXPECT_EQ(p1->num_communities, p2->num_communities)
        << "Determinism regression: different community counts";
    EXPECT_EQ(p2->num_communities, p3->num_communities)
        << "Determinism regression: inconsistent results";

    // All should have very close modularity
    EXPECT_NEAR(p1->modularity, p2->modularity, 1e-10)
        << "Determinism regression: modularity differs";
    EXPECT_NEAR(p2->modularity, p3->modularity, 1e-10)
        << "Determinism regression: inconsistent modularity";

    nimcp_community_partition_destroy(p1);
    nimcp_community_partition_destroy(p2);
    nimcp_community_partition_destroy(p3);
    nimcp_graph_destroy(graph1);
    nimcp_graph_destroy(graph2);
    nimcp_graph_destroy(graph3);
}

//=============================================================================
// Regression Tests: Centrality Algorithms
//=============================================================================

TEST_F(CommunityDetectionRegressionTest, test_degree_centrality_star_baseline)
{
    // WHAT: Degree centrality on star graph baseline
    // WHY: Detect centrality calculation regressions
    // HOW: Verify hub has expected centrality

    NimcpGraph* graph = create_regression_star(10);
    ASSERT_NE(nullptr, graph);

    NimcpCentralityScores* scores = nimcp_degree_centrality(graph);
    ASSERT_NE(nullptr, scores);

    // Hub should have highest centrality
    double hub_centrality = scores->scores[0];
    EXPECT_GE(hub_centrality, 0.99) << "Hub centrality regression";

    // All leaves should have same centrality
    double leaf_centrality = scores->scores[1];
    for (uint32_t i = 2; i <= 10; i++) {
        EXPECT_NEAR(scores->scores[i], leaf_centrality, 1e-10)
            << "Leaf centrality regression at vertex " << i;
    }

    nimcp_centrality_scores_destroy(scores);
    nimcp_graph_destroy(graph);
}

TEST_F(CommunityDetectionRegressionTest, test_hub_detection_star_baseline)
{
    // WHAT: Hub detection on star graph should identify hub
    // WHY: Detect hub detection regressions
    // HOW: Verify exactly 1 hub detected

    NimcpGraph* graph = create_regression_star(10);
    ASSERT_NE(nullptr, graph);

    NimcpCentralityScores* scores = nimcp_degree_centrality(graph);
    ASSERT_NE(nullptr, scores);

    uint32_t hubs[256];
    uint32_t num_hubs = nimcp_detect_hubs(scores, 1.0, hubs, 256);

    // Should detect exactly the hub
    EXPECT_EQ(1u, num_hubs) << "Hub detection count regression";
    EXPECT_EQ(0u, hubs[0]) << "Should detect vertex 0 as hub";

    nimcp_centrality_scores_destroy(scores);
    nimcp_graph_destroy(graph);
}

//=============================================================================
// Regression Tests: Modularity Calculation
//=============================================================================

TEST_F(CommunityDetectionRegressionTest, test_modularity_calculation_baseline)
{
    // WHAT: Modularity calculation should match baseline
    // WHY: Detect modularity calculation regressions
    // HOW: Calculate Q for modular network with known partition

    NimcpGraph* graph = create_regression_network();
    ASSERT_NE(nullptr, graph);

    uint32_t assignments[15] = {0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2};

    double q = nimcp_calculate_modularity(graph, assignments, 15);

    // Modularity should be in expected range
    EXPECT_GE(q, BASELINE_MODULAR_Q - Q_TOLERANCE) << "Modularity calculation regression (too low)";
    EXPECT_LE(q, BASELINE_MODULAR_Q + Q_TOLERANCE) << "Modularity calculation regression (too high)";

    nimcp_graph_destroy(graph);
}

TEST_F(CommunityDetectionRegressionTest, test_modularity_validation_baseline)
{
    // WHAT: Partition validation should work correctly
    // WHY: Detect validation logic regressions
    // HOW: Test known valid and invalid partitions

    uint32_t valid_partition[15] = {0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2};
    uint32_t invalid_partition[15] = {0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 5, 5, 5, 5, 5};

    bool valid = nimcp_validate_partition(valid_partition, 15, 3);
    bool invalid = nimcp_validate_partition(invalid_partition, 15, 3);

    EXPECT_TRUE(valid) << "Valid partition validation regression";
    EXPECT_FALSE(invalid) << "Invalid partition validation regression";
}

//=============================================================================
// Regression Tests: Performance
//=============================================================================

TEST_F(CommunityDetectionRegressionTest, test_louvain_convergence_performance)
{
    // WHAT: Louvain should converge in reasonable time
    // WHY: Detect performance regressions
    // HOW: Measure iterations required

    NimcpGraph* graph = create_regression_network();
    ASSERT_NE(nullptr, graph);

    auto start = std::chrono::high_resolution_clock::now();
    NimcpCommunityPartition* partition = nimcp_louvain_detect(graph, 1.0, 42);
    auto end = std::chrono::high_resolution_clock::now();

    ASSERT_NE(nullptr, partition);

    uint32_t iterations = partition->iterations;
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Should converge quickly (less than 10 iterations)
    EXPECT_LE(iterations, BASELINE_MAX_ITERATIONS)
        << "Convergence speed regression: too many iterations";

    // Should be fast (reasonable time bound)
    EXPECT_LT(elapsed, 1000) << "Performance regression: took > 1 second";

    nimcp_community_partition_destroy(partition);
    nimcp_graph_destroy(graph);
}

TEST_F(CommunityDetectionRegressionTest, test_degree_centrality_performance)
{
    // WHAT: Degree centrality should compute quickly
    // WHY: Detect performance regressions
    // HOW: Measure computation time

    NimcpGraph* graph = create_regression_network();
    ASSERT_NE(nullptr, graph);

    auto start = std::chrono::high_resolution_clock::now();
    NimcpCentralityScores* scores = nimcp_degree_centrality(graph);
    auto end = std::chrono::high_resolution_clock::now();

    ASSERT_NE(nullptr, scores);

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Should be very fast (< 100ms for small graph)
    EXPECT_LT(elapsed, 100) << "Degree centrality performance regression";

    nimcp_centrality_scores_destroy(scores);
    nimcp_graph_destroy(graph);
}

//=============================================================================
// Regression Tests: Edge Cases
//=============================================================================

TEST_F(CommunityDetectionRegressionTest, test_single_vertex_behavior)
{
    // WHAT: Single vertex should produce consistent results
    // WHY: Ensure edge cases don't regress
    // HOW: Test single-vertex network

    NimcpGraph* graph = nimcp_graph_create();
    ASSERT_NE(nullptr, graph);
    nimcp_graph_add_vertex(graph, 0, 0.0f, 0.0f, 0.0f, 0);

    NimcpCommunityPartition* partition = nimcp_louvain_detect(graph, 1.0, 42);
    ASSERT_NE(nullptr, partition);

    EXPECT_EQ(1u, partition->num_communities) << "Single vertex edge case regression";

    nimcp_community_partition_destroy(partition);
    nimcp_graph_destroy(graph);
}

TEST_F(CommunityDetectionRegressionTest, test_empty_graph_behavior)
{
    // WHAT: Empty graph should return NULL
    // WHY: Edge case behavior should be consistent
    // HOW: Test empty graph handling

    NimcpGraph* graph = nimcp_graph_create();
    ASSERT_NE(nullptr, graph);

    NimcpCommunityPartition* partition = nimcp_louvain_detect(graph, 1.0, 42);

    EXPECT_EQ(nullptr, partition) << "Empty graph handling regression";

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
