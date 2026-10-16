/**
 * @file test_modularity.cpp
 * @brief Comprehensive test suite for modularity calculations
 *
 * WHAT: Tests for modularity metric on various network types
 * WHY: Ensure modularity calculation is correct across scenarios
 * HOW: Use GTest with synthetic networks of known properties
 */

#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
#include <vector>

#include "utils/algorithms/nimcp_modularity.h"
#include "utils/containers/nimcp_graph.h"

//=============================================================================
// Test Constants
//=============================================================================

static const double EPSILON = 1e-6;
static const double Q_THRESHOLD_RANDOM = 0.3;    // Random graphs should have Q < 0.3
static const double Q_THRESHOLD_MODULAR = 0.3;   // Modular networks should have Q > 0.3

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * WHAT: Create simple 2-community network
 * WHY: Test modularity calculation on known partition
 */
static NimcpGraph* create_two_community_graph(void)
{
    NimcpGraph* graph = nimcp_graph_create();
    if (!graph) return nullptr;

    // Community 1: vertices 0-2 (fully connected)
    for (uint32_t i = 0; i < 3; i++) {
        nimcp_graph_add_vertex(graph, i, 0.0f, 0.0f, 0.0f, 0);
    }
    nimcp_graph_add_edge(graph, 0, 1, 1.0f);
    nimcp_graph_add_edge(graph, 1, 0, 1.0f);
    nimcp_graph_add_edge(graph, 0, 2, 1.0f);
    nimcp_graph_add_edge(graph, 2, 0, 1.0f);
    nimcp_graph_add_edge(graph, 1, 2, 1.0f);
    nimcp_graph_add_edge(graph, 2, 1, 1.0f);

    // Community 2: vertices 3-5 (fully connected)
    for (uint32_t i = 3; i < 6; i++) {
        nimcp_graph_add_vertex(graph, i, 1.0f, 0.0f, 0.0f, 0);
    }
    nimcp_graph_add_edge(graph, 3, 4, 1.0f);
    nimcp_graph_add_edge(graph, 4, 3, 1.0f);
    nimcp_graph_add_edge(graph, 3, 5, 1.0f);
    nimcp_graph_add_edge(graph, 5, 3, 1.0f);
    nimcp_graph_add_edge(graph, 4, 5, 1.0f);
    nimcp_graph_add_edge(graph, 5, 4, 1.0f);

    // Single inter-community edge
    nimcp_graph_add_edge(graph, 0, 3, 1.0f);
    nimcp_graph_add_edge(graph, 3, 0, 1.0f);

    return graph;
}

/**
 * WHAT: Create random graph with no community structure
 * WHY: Test that random partition has low modularity
 */
static NimcpGraph* create_random_sparse_graph(uint32_t num_vertices, uint32_t num_edges, uint32_t seed)
{
    NimcpGraph* graph = nimcp_graph_create();
    if (!graph) return nullptr;

    srand(seed);

    // Add vertices
    for (uint32_t i = 0; i < num_vertices; i++) {
        nimcp_graph_add_vertex(graph, i, 0.0f, 0.0f, 0.0f, 0);
    }

    // Add random edges
    uint32_t edges_added = 0;
    for (uint32_t i = 0; i < num_vertices && edges_added < num_edges; i++) {
        for (uint32_t j = i + 1; j < num_vertices && edges_added < num_edges; j++) {
            if ((rand() % 100) < 30) {  // 30% chance
                nimcp_graph_add_edge(graph, i, j, 1.0f);
                nimcp_graph_add_edge(graph, j, i, 1.0f);
                edges_added++;
            }
        }
    }

    return graph;
}

/**
 * WHAT: Create complete graph
 * WHY: Test complete graph has modularity = 0
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
    // HOW: Create 2-community graph, calculate Q with correct partition

    NimcpGraph* graph = create_two_community_graph();
    ASSERT_NE(nullptr, graph);

    uint32_t assignments[6] = {0, 0, 0, 1, 1, 1};  // Correct partition

    double modularity = nimcp_calculate_modularity(graph, assignments, 6);

    EXPECT_GT(modularity, 0.0) << "Correct partition should have positive modularity";
    EXPECT_LT(modularity, 1.0) << "Modularity should be < 1.0";

    nimcp_graph_destroy(graph);
}

TEST_F(ModularityTest, test_modularity_random_partition)
{
    // WHAT: Random partition should have lower modularity
    // WHY: Incorrect partitions have worse modularity
    // HOW: Create partition that doesn't match structure

    NimcpGraph* graph = create_two_community_graph();
    ASSERT_NE(nullptr, graph);

    uint32_t correct_assignments[6] = {0, 0, 0, 1, 1, 1};
    uint32_t random_assignments[6] = {0, 1, 0, 1, 0, 1};

    double q_correct = nimcp_calculate_modularity(graph, correct_assignments, 6);
    double q_random = nimcp_calculate_modularity(graph, random_assignments, 6);

    EXPECT_GT(q_correct, q_random)
        << "Correct partition should have higher modularity than random partition";

    nimcp_graph_destroy(graph);
}

TEST_F(ModularityTest, test_modularity_all_one_community)
{
    // WHAT: All vertices in one community
    // WHY: Edge case for modularity calculation
    // HOW: Assign all to community 0

    NimcpGraph* graph = create_two_community_graph();
    ASSERT_NE(nullptr, graph);

    uint32_t assignments[6] = {0, 0, 0, 0, 0, 0};

    double modularity = nimcp_calculate_modularity(graph, assignments, 6);

    EXPECT_LE(modularity, 0.0)
        << "All-in-one partition should have non-positive modularity";

    nimcp_graph_destroy(graph);
}

TEST_F(ModularityTest, test_modularity_each_vertex_own_community)
{
    // WHAT: Each vertex in separate community
    // WHY: Edge case for modularity calculation
    // HOW: Assign each to unique community

    NimcpGraph* graph = create_two_community_graph();
    ASSERT_NE(nullptr, graph);

    uint32_t assignments[6] = {0, 1, 2, 3, 4, 5};

    double modularity = nimcp_calculate_modularity(graph, assignments, 6);

    EXPECT_LE(modularity, 0.3)
        << "Each-vertex-separate should have low modularity";

    nimcp_graph_destroy(graph);
}

//=============================================================================
// Tests: Modularity with Resolution Parameter
//=============================================================================

TEST_F(ModularityTest, test_modularity_with_resolution_default)
{
    // WHAT: Default resolution (1.0) should match standard modularity
    // WHY: Verify resolution parameter works correctly
    // HOW: Compare with and without resolution=1.0

    NimcpGraph* graph = create_two_community_graph();
    ASSERT_NE(nullptr, graph);

    uint32_t assignments[6] = {0, 0, 0, 1, 1, 1};

    double q1 = nimcp_calculate_modularity(graph, assignments, 6);
    double q2 = nimcp_calculate_modularity_with_resolution(graph, assignments, 6, 1.0);

    EXPECT_NEAR(q1, q2, EPSILON) << "Default and resolution=1.0 should match";

    nimcp_graph_destroy(graph);
}

TEST_F(ModularityTest, test_modularity_with_resolution_higher)
{
    // WHAT: Higher resolution finds finer structures
    // WHY: Resolution parameter controls hierarchical level
    // HOW: Compare Q with different resolution values

    NimcpGraph* graph = create_two_community_graph();
    ASSERT_NE(nullptr, graph);

    uint32_t assignments[6] = {0, 0, 0, 1, 1, 1};

    double q_low = nimcp_calculate_modularity_with_resolution(graph, assignments, 6, 0.5);
    double q_high = nimcp_calculate_modularity_with_resolution(graph, assignments, 6, 2.0);

    // Higher resolution should reduce modularity for same partition
    EXPECT_LE(q_high, q_low + EPSILON)
        << "Higher resolution should not increase modularity for fixed partition";

    nimcp_graph_destroy(graph);
}

//=============================================================================
// Tests: Edge Cases
//=============================================================================

TEST_F(ModularityTest, test_modularity_empty_graph)
{
    // WHAT: Empty graph should have zero modularity
    // WHY: No edges means no community structure
    // HOW: Create empty graph and calculate

    NimcpGraph* graph = nimcp_graph_create();
    ASSERT_NE(nullptr, graph);

    uint32_t assignments[1] = {0};

    double modularity = nimcp_calculate_modularity(graph, assignments, 1);

    EXPECT_EQ(0.0, modularity) << "Empty graph should have zero modularity";

    nimcp_graph_destroy(graph);
}

TEST_F(ModularityTest, test_modularity_single_vertex)
{
    // WHAT: Single vertex should have zero modularity
    // WHY: No edges possible
    // HOW: Create 1-vertex graph

    NimcpGraph* graph = nimcp_graph_create();
    ASSERT_NE(nullptr, graph);
    nimcp_graph_add_vertex(graph, 0, 0.0f, 0.0f, 0.0f, 0);

    uint32_t assignments[1] = {0};

    double modularity = nimcp_calculate_modularity(graph, assignments, 1);

    EXPECT_EQ(0.0, modularity) << "Single vertex should have zero modularity";

    nimcp_graph_destroy(graph);
}

TEST_F(ModularityTest, test_modularity_complete_graph)
{
    // WHAT: Complete graph with one community
    // WHY: All vertices equally connected
    // HOW: Test Q for complete graph

    NimcpGraph* graph = create_complete_graph(5);
    ASSERT_NE(nullptr, graph);

    uint32_t assignments[5] = {0, 0, 0, 0, 0};

    double modularity = nimcp_calculate_modularity(graph, assignments, 5);

    EXPECT_LE(modularity, 0.1) << "Complete graph one-community should have low Q";

    nimcp_graph_destroy(graph);
}

//=============================================================================
// Tests: Partition Validation
//=============================================================================

TEST_F(ModularityTest, test_validate_partition_valid)
{
    // WHAT: Validate correct partition
    // WHY: Ensure validation catches errors
    // HOW: Check valid partition returns true

    uint32_t assignments[6] = {0, 0, 1, 1, 2, 2};

    bool valid = nimcp_validate_partition(assignments, 6, 3);

    EXPECT_TRUE(valid) << "Valid partition should pass validation";
}

TEST_F(ModularityTest, test_validate_partition_out_of_range)
{
    // WHAT: Partition with community ID out of range
    // WHY: Catch invalid assignments
    // HOW: Check community ID exceeds num_communities

    uint32_t assignments[6] = {0, 0, 1, 1, 5, 5};  // 5 >= 3

    bool valid = nimcp_validate_partition(assignments, 6, 3);

    EXPECT_FALSE(valid) << "Out-of-range community should fail validation";
}

TEST_F(ModularityTest, test_validate_partition_gaps)
{
    // WHAT: Partition with unused community IDs
    // WHY: All communities should have members
    // HOW: Create partition with gap (0,1,3 but not 2)

    uint32_t assignments[6] = {0, 0, 0, 1, 1, 1};  // No community 2

    bool valid = nimcp_validate_partition(assignments, 6, 3);

    EXPECT_FALSE(valid) << "Partition with gaps should fail validation";
}

TEST_F(ModularityTest, test_validate_partition_empty_num_vertices)
{
    // WHAT: Empty partition
    // WHY: Edge case
    // HOW: Zero vertices

    uint32_t assignments[1] = {0};

    bool valid = nimcp_validate_partition(assignments, 0, 1);

    EXPECT_FALSE(valid) << "Empty partition should fail";
}

//=============================================================================
// Tests: Community Counting
//=============================================================================

TEST_F(ModularityTest, test_count_communities_correct)
{
    // WHAT: Count unique communities
    // WHY: Verify community counting
    // HOW: Create partition and count

    uint32_t assignments[10] = {0, 0, 0, 1, 1, 2, 2, 2, 2, 1};

    uint32_t count = nimcp_count_communities(assignments, 10);

    EXPECT_EQ(3u, count) << "Should count 3 unique communities";
}

TEST_F(ModularityTest, test_count_communities_single)
{
    // WHAT: Count when all in one community
    // WHY: Edge case
    // HOW: All same community ID

    uint32_t assignments[5] = {0, 0, 0, 0, 0};

    uint32_t count = nimcp_count_communities(assignments, 5);

    EXPECT_EQ(1u, count) << "Should count 1 community";
}

TEST_F(ModularityTest, test_count_communities_all_separate)
{
    // WHAT: Count when each vertex separate
    // WHY: Edge case
    // HOW: Each vertex in unique community

    uint32_t assignments[5] = {0, 1, 2, 3, 4};

    uint32_t count = nimcp_count_communities(assignments, 5);

    EXPECT_EQ(5u, count) << "Should count 5 unique communities";
}

//=============================================================================
// Tests: Modularity Properties
//=============================================================================

TEST_F(ModularityTest, test_modularity_bounds)
{
    // WHAT: Modularity should be in [-0.5, 1.0]
    // WHY: Theoretical bounds on modularity
    // HOW: Generate various partitions and check bounds

    NimcpGraph* graph = create_two_community_graph();
    ASSERT_NE(nullptr, graph);

    // Test multiple partitions
    uint32_t partitions[3][6] = {
        {0, 0, 0, 1, 1, 1},   // Good partition
        {0, 1, 0, 1, 0, 1},   // Random partition
        {0, 0, 0, 0, 0, 0},   // All same
    };

    for (int i = 0; i < 3; i++) {
        double q = nimcp_calculate_modularity(graph, partitions[i], 6);
        EXPECT_GE(q, -0.6) << "Modularity should be >= -0.5";
        EXPECT_LE(q, 1.0) << "Modularity should be <= 1.0";
    }

    nimcp_graph_destroy(graph);
}

TEST_F(ModularityTest, test_modularity_symmetry)
{
    // WHAT: Swapping community labels shouldn't change Q
    // WHY: Modularity is invariant to relabeling
    // HOW: Calculate Q with swapped labels

    NimcpGraph* graph = create_two_community_graph();
    ASSERT_NE(nullptr, graph);

    uint32_t assignments1[6] = {0, 0, 0, 1, 1, 1};
    uint32_t assignments2[6] = {1, 1, 1, 0, 0, 0};  // Labels swapped

    double q1 = nimcp_calculate_modularity(graph, assignments1, 6);
    double q2 = nimcp_calculate_modularity(graph, assignments2, 6);

    EXPECT_NEAR(q1, q2, EPSILON) << "Modularity should be invariant to relabeling";

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
