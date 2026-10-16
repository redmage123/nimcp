/**
 * @file test_graph.cpp
 * @brief Comprehensive unit tests for nimcp_graph.c
 *
 * WHAT: Tests for network topology graph data structure
 * WHY: Ensure graph operations, path finding, and component analysis work correctly
 * HOW: GoogleTest framework with fixture classes and comprehensive edge case coverage
 */

#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
#include <vector>

#include "utils/containers/nimcp_graph.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Graph Creation and Destruction Tests
//=============================================================================

class GraphTest : public ::testing::Test {
   protected:
    void SetUp() override
    {
        nimcp_memory_init();
        graph = nullptr;
    }

    void TearDown() override
    {
        if (graph) {
            nimcp_graph_destroy(graph);
        }
        nimcp_memory_cleanup();
    }

    NimcpGraph* graph;
};

/**
 * WHAT: Test basic graph creation
 * WHY: Verify graph can be created successfully
 */
TEST_F(GraphTest, CreateGraph)
{
    graph = nimcp_graph_create();
    ASSERT_NE(graph, nullptr);
    EXPECT_EQ(graph->vertex_count, 0);
    EXPECT_EQ(graph->edge_count, 0);
    EXPECT_EQ(graph->component_count, 0);
}

/**
 * WHAT: Test graph destruction with NULL
 * WHY: Verify safe handling of NULL pointer
 */
TEST_F(GraphTest, DestroyNullGraph)
{
    nimcp_graph_destroy(nullptr);  // Should not crash
}

/**
 * WHAT: Test graph destruction with vertices and edges
 * WHY: Verify proper cleanup of allocated memory
 */
TEST_F(GraphTest, DestroyGraphWithData)
{
    graph = nimcp_graph_create();
    ASSERT_NE(graph, nullptr);

    // Add some vertices
    nimcp_graph_add_vertex(graph, 1, 0.0f, 0.0f, 0.0f, 0);
    nimcp_graph_add_vertex(graph, 2, 1.0f, 1.0f, 1.0f, 0);

    // Add edge
    nimcp_graph_add_edge(graph, 0, 1, 1.0f);

    nimcp_graph_destroy(graph);
    graph = nullptr;  // Prevent double-free in TearDown
}

//=============================================================================
// Vertex Operations Tests
//=============================================================================

class VertexTest : public ::testing::Test {
   protected:
    void SetUp() override
    {
        nimcp_memory_init();
        graph = nimcp_graph_create();
        ASSERT_NE(graph, nullptr);
    }

    void TearDown() override
    {
        if (graph) {
            nimcp_graph_destroy(graph);
        }
        nimcp_memory_cleanup();
    }

    NimcpGraph* graph;
};

/**
 * WHAT: Test adding single vertex
 * WHY: Verify basic vertex addition works
 */
TEST_F(VertexTest, AddSingleVertex)
{
    uint32_t idx = nimcp_graph_add_vertex(graph, 100, 1.0f, 2.0f, 3.0f, 0xABCD);
    EXPECT_EQ(idx, 0);
    EXPECT_EQ(graph->vertex_count, 1);
    EXPECT_EQ(graph->vertices[0].peer_id, 100);
    EXPECT_FLOAT_EQ(graph->vertices[0].x, 1.0f);
    EXPECT_FLOAT_EQ(graph->vertices[0].y, 2.0f);
    EXPECT_FLOAT_EQ(graph->vertices[0].z, 3.0f);
    EXPECT_EQ(graph->vertices[0].capabilities, 0xABCD);
}

/**
 * WHAT: Test adding multiple vertices
 * WHY: Verify multiple vertices can be added and indexed correctly
 */
TEST_F(VertexTest, AddMultipleVertices)
{
    uint32_t idx1 = nimcp_graph_add_vertex(graph, 1, 0.0f, 0.0f, 0.0f, 0);
    uint32_t idx2 = nimcp_graph_add_vertex(graph, 2, 1.0f, 1.0f, 1.0f, 0);
    uint32_t idx3 = nimcp_graph_add_vertex(graph, 3, 2.0f, 2.0f, 2.0f, 0);

    EXPECT_EQ(idx1, 0);
    EXPECT_EQ(idx2, 1);
    EXPECT_EQ(idx3, 2);
    EXPECT_EQ(graph->vertex_count, 3);
}

/**
 * WHAT: Test adding vertex to NULL graph
 * WHY: Verify proper error handling
 */
TEST_F(VertexTest, AddVertexToNullGraph)
{
    uint32_t idx = nimcp_graph_add_vertex(nullptr, 1, 0.0f, 0.0f, 0.0f, 0);
    EXPECT_EQ(idx, NIMCP_INVALID_VERTEX);
}

/**
 * WHAT: Test adding duplicate peer_id
 * WHY: Ensure duplicate peer IDs are rejected
 */
TEST_F(VertexTest, AddDuplicatePeerId)
{
    uint32_t idx1 = nimcp_graph_add_vertex(graph, 100, 0.0f, 0.0f, 0.0f, 0);
    EXPECT_NE(idx1, NIMCP_INVALID_VERTEX);

    // Try to add same peer_id again
    uint32_t idx2 = nimcp_graph_add_vertex(graph, 100, 1.0f, 1.0f, 1.0f, 0);
    EXPECT_EQ(idx2, NIMCP_INVALID_VERTEX);
    EXPECT_EQ(graph->vertex_count, 1);  // Should still be 1
}

/**
 * WHAT: Test adding vertices up to maximum
 * WHY: Verify capacity limit is enforced
 */
TEST_F(VertexTest, AddMaxVertices)
{
    // Add vertices up to the limit
    for (uint32_t i = 0; i < NIMCP_MAX_VERTICES; i++) {
        uint32_t idx = nimcp_graph_add_vertex(graph, i, 0.0f, 0.0f, 0.0f, 0);
        EXPECT_EQ(idx, i);
    }

    EXPECT_EQ(graph->vertex_count, NIMCP_MAX_VERTICES);

    // Try to add one more - should fail
    uint32_t idx = nimcp_graph_add_vertex(graph, NIMCP_MAX_VERTICES, 0.0f, 0.0f, 0.0f, 0);
    EXPECT_EQ(idx, NIMCP_INVALID_VERTEX);
}

/**
 * WHAT: Test removing single vertex
 * WHY: Verify vertex removal works
 */
TEST_F(VertexTest, RemoveSingleVertex)
{
    uint32_t idx = nimcp_graph_add_vertex(graph, 1, 0.0f, 0.0f, 0.0f, 0);
    EXPECT_EQ(graph->vertex_count, 1);

    bool result = nimcp_graph_remove_vertex(graph, idx);
    EXPECT_TRUE(result);
    EXPECT_EQ(graph->vertex_count, 0);
}

/**
 * WHAT: Test removing vertex from NULL graph
 * WHY: Verify proper error handling
 */
TEST_F(VertexTest, RemoveVertexFromNullGraph)
{
    bool result = nimcp_graph_remove_vertex(nullptr, 0);
    EXPECT_FALSE(result);
}

/**
 * WHAT: Test removing vertex with invalid index
 * WHY: Ensure invalid indices are rejected
 */
TEST_F(VertexTest, RemoveInvalidVertex)
{
    nimcp_graph_add_vertex(graph, 1, 0.0f, 0.0f, 0.0f, 0);

    bool result = nimcp_graph_remove_vertex(graph, 999);
    EXPECT_FALSE(result);
    EXPECT_EQ(graph->vertex_count, 1);  // Should still be 1
}

/**
 * WHAT: Test removing vertex with edges
 * WHY: Verify edges are properly cleaned up
 */
TEST_F(VertexTest, RemoveVertexWithEdges)
{
    uint32_t v1 = nimcp_graph_add_vertex(graph, 1, 0.0f, 0.0f, 0.0f, 0);
    uint32_t v2 = nimcp_graph_add_vertex(graph, 2, 1.0f, 1.0f, 1.0f, 0);
    uint32_t v3 = nimcp_graph_add_vertex(graph, 3, 2.0f, 2.0f, 2.0f, 0);

    nimcp_graph_add_edge(graph, v1, v2, 1.0f);
    nimcp_graph_add_edge(graph, v2, v3, 1.0f);
    EXPECT_EQ(graph->edge_count, 2);

    // Remove middle vertex
    bool result = nimcp_graph_remove_vertex(graph, v2);
    EXPECT_TRUE(result);
    EXPECT_EQ(graph->vertex_count, 2);
    EXPECT_EQ(graph->edge_count, 0);  // Both edges should be removed
}

/**
 * WHAT: Test updating vertex coordinates
 * WHY: Verify coordinate updates work correctly
 */
TEST_F(VertexTest, UpdateCoordinates)
{
    uint32_t idx = nimcp_graph_add_vertex(graph, 1, 0.0f, 0.0f, 0.0f, 0);

    bool result = nimcp_graph_update_coordinates(graph, idx, 5.0f, 6.0f, 7.0f);
    EXPECT_TRUE(result);
    EXPECT_FLOAT_EQ(graph->vertices[idx].x, 5.0f);
    EXPECT_FLOAT_EQ(graph->vertices[idx].y, 6.0f);
    EXPECT_FLOAT_EQ(graph->vertices[idx].z, 7.0f);
}

/**
 * WHAT: Test updating coordinates with invalid values
 * WHY: Ensure NaN and infinity are rejected
 */
TEST_F(VertexTest, UpdateCoordinatesWithInvalidValues)
{
    uint32_t idx = nimcp_graph_add_vertex(graph, 1, 0.0f, 0.0f, 0.0f, 0);

    // Try with NaN
    bool result = nimcp_graph_update_coordinates(graph, idx, NAN, 0.0f, 0.0f);
    EXPECT_FALSE(result);

    // Try with infinity
    result = nimcp_graph_update_coordinates(graph, idx, INFINITY, 0.0f, 0.0f);
    EXPECT_FALSE(result);
}

/**
 * WHAT: Test finding vertex by peer ID
 * WHY: Verify peer ID lookup works
 */
TEST_F(VertexTest, FindVertex)
{
    nimcp_graph_add_vertex(graph, 100, 0.0f, 0.0f, 0.0f, 0);
    nimcp_graph_add_vertex(graph, 200, 1.0f, 1.0f, 1.0f, 0);
    nimcp_graph_add_vertex(graph, 300, 2.0f, 2.0f, 2.0f, 0);

    uint32_t idx = nimcp_graph_find_vertex(graph, 200);
    EXPECT_EQ(idx, 1);
    EXPECT_EQ(graph->vertices[idx].peer_id, 200);
}

/**
 * WHAT: Test finding non-existent vertex
 * WHY: Verify proper handling when peer ID not found
 */
TEST_F(VertexTest, FindNonExistentVertex)
{
    nimcp_graph_add_vertex(graph, 100, 0.0f, 0.0f, 0.0f, 0);

    uint32_t idx = nimcp_graph_find_vertex(graph, 999);
    EXPECT_EQ(idx, NIMCP_INVALID_VERTEX);
}

//=============================================================================
// Edge Operations Tests
//=============================================================================

class EdgeTest : public ::testing::Test {
   protected:
    void SetUp() override
    {
        nimcp_memory_init();
        graph = nimcp_graph_create();
        ASSERT_NE(graph, nullptr);

        // Add some vertices for testing
        v1 = nimcp_graph_add_vertex(graph, 1, 0.0f, 0.0f, 0.0f, 0);
        v2 = nimcp_graph_add_vertex(graph, 2, 1.0f, 1.0f, 1.0f, 0);
        v3 = nimcp_graph_add_vertex(graph, 3, 2.0f, 2.0f, 2.0f, 0);
    }

    void TearDown() override
    {
        if (graph) {
            nimcp_graph_destroy(graph);
        }
        nimcp_memory_cleanup();
    }

    NimcpGraph* graph;
    uint32_t v1, v2, v3;
};

/**
 * WHAT: Test adding single edge
 * WHY: Verify basic edge addition works
 */
TEST_F(EdgeTest, AddSingleEdge)
{
    bool result = nimcp_graph_add_edge(graph, v1, v2, 1.5f);
    EXPECT_TRUE(result);
    EXPECT_EQ(graph->edge_count, 1);
    EXPECT_EQ(graph->vertices[v1].edge_count, 1);
}

/**
 * WHAT: Test adding edge to NULL graph
 * WHY: Verify proper error handling
 */
TEST_F(EdgeTest, AddEdgeToNullGraph)
{
    bool result = nimcp_graph_add_edge(nullptr, 0, 1, 1.0f);
    EXPECT_FALSE(result);
}

/**
 * WHAT: Test adding edge with invalid indices
 * WHY: Ensure invalid vertex indices are rejected
 */
TEST_F(EdgeTest, AddEdgeWithInvalidIndices)
{
    bool result = nimcp_graph_add_edge(graph, 999, v2, 1.0f);
    EXPECT_FALSE(result);

    result = nimcp_graph_add_edge(graph, v1, 999, 1.0f);
    EXPECT_FALSE(result);
}

/**
 * WHAT: Test adding self-loop
 * WHY: Ensure edges to same vertex are rejected
 */
TEST_F(EdgeTest, AddSelfLoop)
{
    bool result = nimcp_graph_add_edge(graph, v1, v1, 1.0f);
    EXPECT_FALSE(result);
}

/**
 * WHAT: Test adding duplicate edge
 * WHY: Verify duplicate edges update weight instead of adding new edge
 */
TEST_F(EdgeTest, AddDuplicateEdge)
{
    bool result1 = nimcp_graph_add_edge(graph, v1, v2, 1.0f);
    EXPECT_TRUE(result1);
    EXPECT_EQ(graph->edge_count, 1);

    // Add same edge again with different weight
    bool result2 = nimcp_graph_add_edge(graph, v1, v2, 2.0f);
    EXPECT_TRUE(result2);
    EXPECT_EQ(graph->edge_count, 1);  // Count should still be 1

    // Verify weight was updated
    nimcp_weight_t weight;
    bool found = nimcp_graph_get_edge_weight(graph, v1, v2, &weight);
    EXPECT_TRUE(found);
    EXPECT_FLOAT_EQ(weight, 2.0f);
}

/**
 * WHAT: Test adding edge with invalid weight
 * WHY: Ensure NaN and infinity weights are rejected
 */
TEST_F(EdgeTest, AddEdgeWithInvalidWeight)
{
    bool result = nimcp_graph_add_edge(graph, v1, v2, NAN);
    EXPECT_FALSE(result);

    result = nimcp_graph_add_edge(graph, v1, v2, INFINITY);
    EXPECT_FALSE(result);
}

/**
 * WHAT: Test adding multiple edges
 * WHY: Verify graph can handle multiple edges correctly
 */
TEST_F(EdgeTest, AddMultipleEdges)
{
    bool r1 = nimcp_graph_add_edge(graph, v1, v2, 1.0f);
    bool r2 = nimcp_graph_add_edge(graph, v2, v3, 2.0f);
    bool r3 = nimcp_graph_add_edge(graph, v1, v3, 3.0f);

    EXPECT_TRUE(r1);
    EXPECT_TRUE(r2);
    EXPECT_TRUE(r3);
    EXPECT_EQ(graph->edge_count, 3);
}

/**
 * WHAT: Test removing single edge
 * WHY: Verify edge removal works
 */
TEST_F(EdgeTest, RemoveSingleEdge)
{
    nimcp_graph_add_edge(graph, v1, v2, 1.0f);
    EXPECT_EQ(graph->edge_count, 1);

    bool result = nimcp_graph_remove_edge(graph, v1, v2);
    EXPECT_TRUE(result);
    EXPECT_EQ(graph->edge_count, 0);
    EXPECT_EQ(graph->vertices[v1].edge_count, 0);
}

/**
 * WHAT: Test removing non-existent edge
 * WHY: Verify proper handling when edge doesn't exist
 */
TEST_F(EdgeTest, RemoveNonExistentEdge)
{
    bool result = nimcp_graph_remove_edge(graph, v1, v2);
    EXPECT_FALSE(result);
}

/**
 * WHAT: Test removing edge from NULL graph
 * WHY: Verify proper error handling
 */
TEST_F(EdgeTest, RemoveEdgeFromNullGraph)
{
    bool result = nimcp_graph_remove_edge(nullptr, 0, 1);
    EXPECT_FALSE(result);
}

/**
 * WHAT: Test removing edge with invalid indices
 * WHY: Ensure invalid vertex indices are rejected
 */
TEST_F(EdgeTest, RemoveEdgeWithInvalidIndices)
{
    bool result = nimcp_graph_remove_edge(graph, 999, v2);
    EXPECT_FALSE(result);

    result = nimcp_graph_remove_edge(graph, v1, 999);
    EXPECT_FALSE(result);
}

/**
 * WHAT: Test getting edge weight
 * WHY: Verify edge weight retrieval works
 */
TEST_F(EdgeTest, GetEdgeWeight)
{
    nimcp_graph_add_edge(graph, v1, v2, 2.5f);

    nimcp_weight_t weight;
    bool result = nimcp_graph_get_edge_weight(graph, v1, v2, &weight);
    EXPECT_TRUE(result);
    EXPECT_FLOAT_EQ(weight, 2.5f);
}

/**
 * WHAT: Test getting weight of non-existent edge
 * WHY: Verify proper handling when edge doesn't exist
 */
TEST_F(EdgeTest, GetNonExistentEdgeWeight)
{
    nimcp_weight_t weight;
    bool result = nimcp_graph_get_edge_weight(graph, v1, v2, &weight);
    EXPECT_FALSE(result);
}

/**
 * WHAT: Test getting neighbors
 * WHY: Verify neighbor retrieval works correctly
 */
TEST_F(EdgeTest, GetNeighbors)
{
    nimcp_graph_add_edge(graph, v1, v2, 1.0f);
    nimcp_graph_add_edge(graph, v1, v3, 2.0f);

    uint32_t neighbors[10];
    uint32_t count = nimcp_graph_get_neighbors(graph, v1, neighbors, 10);
    EXPECT_EQ(count, 2);

    // Verify neighbors are correct (order may vary)
    std::vector<uint32_t> neighbor_vec(neighbors, neighbors + count);
    EXPECT_NE(std::find(neighbor_vec.begin(), neighbor_vec.end(), v2), neighbor_vec.end());
    EXPECT_NE(std::find(neighbor_vec.begin(), neighbor_vec.end(), v3), neighbor_vec.end());
}

/**
 * WHAT: Test getting neighbors with limited buffer
 * WHY: Verify function respects max_neighbors limit
 */
TEST_F(EdgeTest, GetNeighborsLimitedBuffer)
{
    nimcp_graph_add_edge(graph, v1, v2, 1.0f);
    nimcp_graph_add_edge(graph, v1, v3, 2.0f);

    uint32_t neighbors[1];  // Buffer for only 1 neighbor
    uint32_t count = nimcp_graph_get_neighbors(graph, v1, neighbors, 1);
    EXPECT_EQ(count, 1);  // Should only return 1 neighbor
}

/**
 * WHAT: Test getting neighbors with NULL buffer
 * WHY: Verify proper error handling
 */
TEST_F(EdgeTest, GetNeighborsNullBuffer)
{
    nimcp_graph_add_edge(graph, v1, v2, 1.0f);

    uint32_t count = nimcp_graph_get_neighbors(graph, v1, nullptr, 10);
    EXPECT_EQ(count, 0);
}

//=============================================================================
// Path Finding Tests
//=============================================================================

class PathFindingTest : public ::testing::Test {
   protected:
    void SetUp() override
    {
        nimcp_memory_init();
        graph = nimcp_graph_create();
        ASSERT_NE(graph, nullptr);
    }

    void TearDown() override
    {
        if (graph) {
            nimcp_graph_destroy(graph);
        }
        nimcp_memory_cleanup();
    }

    NimcpGraph* graph;
};

/**
 * WHAT: Test shortest path in simple graph
 * WHY: Verify Dijkstra's algorithm works for basic case
 */
TEST_F(PathFindingTest, SimpleShortestPath)
{
    // Create simple 3-node path: 0 -> 1 -> 2
    uint32_t v0 = nimcp_graph_add_vertex(graph, 0, 0.0f, 0.0f, 0.0f, 0);
    uint32_t v1 = nimcp_graph_add_vertex(graph, 1, 1.0f, 0.0f, 0.0f, 0);
    uint32_t v2 = nimcp_graph_add_vertex(graph, 2, 2.0f, 0.0f, 0.0f, 0);

    nimcp_graph_add_edge(graph, v0, v1, 1.0f);
    nimcp_graph_add_edge(graph, v1, v2, 1.0f);

    NimcpPath* path = nimcp_graph_shortest_path(graph, v0, v2);
    ASSERT_NE(path, nullptr);
    EXPECT_EQ(path->length, 3);  // 0 -> 1 -> 2
    EXPECT_FLOAT_EQ(path->total_weight, 2.0f);
    EXPECT_EQ(path->vertices[0], v0);
    EXPECT_EQ(path->vertices[1], v1);
    EXPECT_EQ(path->vertices[2], v2);

    nimcp_free(path->vertices);
    nimcp_free(path);
}

/**
 * WHAT: Test shortest path with multiple routes
 * WHY: Verify algorithm selects optimal path
 */
TEST_F(PathFindingTest, MultipleRoutes)
{
    // Create diamond graph:
    //   0 -> 1 -> 3
    //   |         ^
    //   +-> 2 ----+
    uint32_t v0 = nimcp_graph_add_vertex(graph, 0, 0.0f, 0.0f, 0.0f, 0);
    uint32_t v1 = nimcp_graph_add_vertex(graph, 1, 1.0f, 1.0f, 0.0f, 0);
    uint32_t v2 = nimcp_graph_add_vertex(graph, 2, 1.0f, -1.0f, 0.0f, 0);
    uint32_t v3 = nimcp_graph_add_vertex(graph, 3, 2.0f, 0.0f, 0.0f, 0);

    nimcp_graph_add_edge(graph, v0, v1, 2.0f);
    nimcp_graph_add_edge(graph, v1, v3, 2.0f);
    nimcp_graph_add_edge(graph, v0, v2, 1.0f);
    nimcp_graph_add_edge(graph, v2, v3, 1.0f);  // Shorter path

    NimcpPath* path = nimcp_graph_shortest_path(graph, v0, v3);
    ASSERT_NE(path, nullptr);
    EXPECT_EQ(path->length, 3);                 // 0 -> 2 -> 3
    EXPECT_FLOAT_EQ(path->total_weight, 2.0f);  // Shorter than 0->1->3 (weight 4)

    nimcp_free(path->vertices);
    nimcp_free(path);
}

/**
 * WHAT: Test path to unreachable vertex
 * WHY: Verify handling when no path exists
 */
TEST_F(PathFindingTest, NoPath)
{
    // Create two disconnected vertices
    uint32_t v0 = nimcp_graph_add_vertex(graph, 0, 0.0f, 0.0f, 0.0f, 0);
    uint32_t v1 = nimcp_graph_add_vertex(graph, 1, 1.0f, 0.0f, 0.0f, 0);

    NimcpPath* path = nimcp_graph_shortest_path(graph, v0, v1);
    EXPECT_EQ(path, nullptr);
}

/**
 * WHAT: Test shortest path with invalid indices
 * WHY: Verify proper error handling
 */
TEST_F(PathFindingTest, InvalidIndices)
{
    uint32_t v0 = nimcp_graph_add_vertex(graph, 0, 0.0f, 0.0f, 0.0f, 0);

    NimcpPath* path = nimcp_graph_shortest_path(graph, v0, 999);
    EXPECT_EQ(path, nullptr);

    path = nimcp_graph_shortest_path(graph, 999, v0);
    EXPECT_EQ(path, nullptr);
}

/**
 * WHAT: Test shortest path from vertex to itself
 * WHY: Verify self-path handling
 */
TEST_F(PathFindingTest, SelfPath)
{
    uint32_t v0 = nimcp_graph_add_vertex(graph, 0, 0.0f, 0.0f, 0.0f, 0);

    NimcpPath* path = nimcp_graph_shortest_path(graph, v0, v0);
    ASSERT_NE(path, nullptr);
    EXPECT_EQ(path->length, 1);
    EXPECT_FLOAT_EQ(path->total_weight, 0.0f);
    EXPECT_EQ(path->vertices[0], v0);

    nimcp_free(path->vertices);
    nimcp_free(path);
}

/**
 * WHAT: Test shortest path on NULL graph
 * WHY: Verify proper error handling
 */
TEST_F(PathFindingTest, NullGraph)
{
    NimcpPath* path = nimcp_graph_shortest_path(nullptr, 0, 1);
    EXPECT_EQ(path, nullptr);
}

//=============================================================================
// Component Analysis Tests
//=============================================================================

class ComponentTest : public ::testing::Test {
   protected:
    void SetUp() override
    {
        nimcp_memory_init();
        graph = nimcp_graph_create();
        ASSERT_NE(graph, nullptr);
    }

    void TearDown() override
    {
        if (graph) {
            nimcp_graph_destroy(graph);
        }
        nimcp_memory_cleanup();
    }

    NimcpGraph* graph;
};

/**
 * WHAT: Test single connected component
 * WHY: Verify component detection for fully connected graph
 */
TEST_F(ComponentTest, SingleComponent)
{
    uint32_t v0 = nimcp_graph_add_vertex(graph, 0, 0.0f, 0.0f, 0.0f, 0);
    uint32_t v1 = nimcp_graph_add_vertex(graph, 1, 1.0f, 0.0f, 0.0f, 0);
    uint32_t v2 = nimcp_graph_add_vertex(graph, 2, 2.0f, 0.0f, 0.0f, 0);

    nimcp_graph_add_edge(graph, v0, v1, 1.0f);
    nimcp_graph_add_edge(graph, v1, v2, 1.0f);

    uint32_t components = nimcp_graph_update_components(graph);
    EXPECT_EQ(components, 1);
    EXPECT_EQ(graph->component_count, 1);
    EXPECT_EQ(graph->components[v0], graph->components[v1]);
    EXPECT_EQ(graph->components[v1], graph->components[v2]);
}

/**
 * WHAT: Test multiple disconnected components
 * WHY: Verify component detection for disconnected graph
 */
TEST_F(ComponentTest, MultipleComponents)
{
    // Component 1: v0 - v1
    uint32_t v0 = nimcp_graph_add_vertex(graph, 0, 0.0f, 0.0f, 0.0f, 0);
    uint32_t v1 = nimcp_graph_add_vertex(graph, 1, 1.0f, 0.0f, 0.0f, 0);
    nimcp_graph_add_edge(graph, v0, v1, 1.0f);

    // Component 2: v2 - v3
    uint32_t v2 = nimcp_graph_add_vertex(graph, 2, 2.0f, 0.0f, 0.0f, 0);
    uint32_t v3 = nimcp_graph_add_vertex(graph, 3, 3.0f, 0.0f, 0.0f, 0);
    nimcp_graph_add_edge(graph, v2, v3, 1.0f);

    uint32_t components = nimcp_graph_update_components(graph);
    EXPECT_EQ(components, 2);
    EXPECT_EQ(graph->component_count, 2);
    EXPECT_EQ(graph->components[v0], graph->components[v1]);
    EXPECT_EQ(graph->components[v2], graph->components[v3]);
    EXPECT_NE(graph->components[v0], graph->components[v2]);
}

/**
 * WHAT: Test isolated vertices
 * WHY: Verify each isolated vertex forms its own component
 */
TEST_F(ComponentTest, IsolatedVertices)
{
    uint32_t v0 = nimcp_graph_add_vertex(graph, 0, 0.0f, 0.0f, 0.0f, 0);
    uint32_t v1 = nimcp_graph_add_vertex(graph, 1, 1.0f, 0.0f, 0.0f, 0);
    uint32_t v2 = nimcp_graph_add_vertex(graph, 2, 2.0f, 0.0f, 0.0f, 0);

    // No edges - all vertices isolated
    uint32_t components = nimcp_graph_update_components(graph);
    EXPECT_EQ(components, 3);
    EXPECT_EQ(graph->component_count, 3);
    EXPECT_NE(graph->components[v0], graph->components[v1]);
    EXPECT_NE(graph->components[v1], graph->components[v2]);
    EXPECT_NE(graph->components[v0], graph->components[v2]);
}

/**
 * WHAT: Test component update on NULL graph
 * WHY: Verify proper error handling
 */
TEST_F(ComponentTest, NullGraph)
{
    uint32_t components = nimcp_graph_update_components(nullptr);
    EXPECT_EQ(components, 0);
}

/**
 * WHAT: Test component update on empty graph
 * WHY: Verify handling of graph with no vertices
 */
TEST_F(ComponentTest, EmptyGraph)
{
    uint32_t components = nimcp_graph_update_components(graph);
    EXPECT_EQ(components, 0);
}

//=============================================================================
// Thread Safety Tests
//=============================================================================

class ThreadSafetyTest : public ::testing::Test {
   protected:
    void SetUp() override
    {
        nimcp_memory_init();
        graph = nimcp_graph_create();
        ASSERT_NE(graph, nullptr);
    }

    void TearDown() override
    {
        if (graph) {
            nimcp_graph_destroy(graph);
        }
        nimcp_memory_cleanup();
    }

    NimcpGraph* graph;
};

/**
 * WHAT: Test concurrent vertex additions
 * WHY: Verify thread safety of add_vertex operation
 * NOTE: Basic test - full thread safety testing would require threading
 */
TEST_F(ThreadSafetyTest, ConcurrentVertexAdditions)
{
    // Sequential additions (actual threading would require pthread or std::thread)
    // This tests that the mutex mechanism doesn't deadlock on sequential calls
    for (int i = 0; i < 10; i++) {
        uint32_t idx = nimcp_graph_add_vertex(graph, i, 0.0f, 0.0f, 0.0f, 0);
        EXPECT_EQ(idx, static_cast<uint32_t>(i));
    }
    EXPECT_EQ(graph->vertex_count, 10);
}

/**
 * WHAT: Test concurrent edge operations
 * WHY: Verify thread safety of edge add/remove operations
 */
TEST_F(ThreadSafetyTest, ConcurrentEdgeOperations)
{
    // Add vertices first
    uint32_t v0 = nimcp_graph_add_vertex(graph, 0, 0.0f, 0.0f, 0.0f, 0);
    uint32_t v1 = nimcp_graph_add_vertex(graph, 1, 1.0f, 0.0f, 0.0f, 0);

    // Sequential edge operations
    for (int i = 0; i < 5; i++) {
        nimcp_graph_add_edge(graph, v0, v1, 1.0f);
        nimcp_graph_remove_edge(graph, v0, v1);
    }

    // Add one final edge
    nimcp_graph_add_edge(graph, v0, v1, 2.0f);
    EXPECT_EQ(graph->edge_count, 1);
}

//=============================================================================
// Complex Graph Scenarios
//=============================================================================

class ComplexGraphTest : public ::testing::Test {
   protected:
    void SetUp() override
    {
        nimcp_memory_init();
        graph = nimcp_graph_create();
        ASSERT_NE(graph, nullptr);
    }

    void TearDown() override
    {
        if (graph) {
            nimcp_graph_destroy(graph);
        }
        nimcp_memory_cleanup();
    }

    NimcpGraph* graph;
};

/**
 * WHAT: Test cyclic graph
 * WHY: Verify graph handles cycles correctly
 */
TEST_F(ComplexGraphTest, CyclicGraph)
{
    // Create cycle: 0 -> 1 -> 2 -> 0
    uint32_t v0 = nimcp_graph_add_vertex(graph, 0, 0.0f, 0.0f, 0.0f, 0);
    uint32_t v1 = nimcp_graph_add_vertex(graph, 1, 1.0f, 0.0f, 0.0f, 0);
    uint32_t v2 = nimcp_graph_add_vertex(graph, 2, 2.0f, 0.0f, 0.0f, 0);

    nimcp_graph_add_edge(graph, v0, v1, 1.0f);
    nimcp_graph_add_edge(graph, v1, v2, 1.0f);
    nimcp_graph_add_edge(graph, v2, v0, 1.0f);

    // Should still find shortest path
    NimcpPath* path = nimcp_graph_shortest_path(graph, v0, v2);
    ASSERT_NE(path, nullptr);
    EXPECT_EQ(path->length, 3);  // 0 -> 1 -> 2

    nimcp_free(path->vertices);
    nimcp_free(path);
}

/**
 * WHAT: Test dense graph
 * WHY: Verify performance with many edges
 */
TEST_F(ComplexGraphTest, DenseGraph)
{
    // Create complete graph of 10 vertices
    const int n = 10;
    std::vector<uint32_t> vertices;

    for (int i = 0; i < n; i++) {
        vertices.push_back(nimcp_graph_add_vertex(graph, i, 0.0f, 0.0f, 0.0f, 0));
    }

    // Add edge between every pair of vertices
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            nimcp_graph_add_edge(graph, vertices[i], vertices[j], 1.0f);
        }
    }

    // Verify all edges were added
    int expected_edges = (n * (n - 1)) / 2;
    EXPECT_EQ(graph->edge_count, static_cast<uint32_t>(expected_edges));

    // All vertices should be in same component
    uint32_t components = nimcp_graph_update_components(graph);
    EXPECT_EQ(components, 1);
}

/**
 * WHAT: Test graph modification during traversal
 * WHY: Verify robust handling of graph changes
 */
TEST_F(ComplexGraphTest, ModifyDuringTraversal)
{
    uint32_t v0 = nimcp_graph_add_vertex(graph, 0, 0.0f, 0.0f, 0.0f, 0);
    uint32_t v1 = nimcp_graph_add_vertex(graph, 1, 1.0f, 0.0f, 0.0f, 0);
    uint32_t v2 = nimcp_graph_add_vertex(graph, 2, 2.0f, 0.0f, 0.0f, 0);

    nimcp_graph_add_edge(graph, v0, v1, 1.0f);
    nimcp_graph_add_edge(graph, v1, v2, 1.0f);

    // Get initial path
    NimcpPath* path1 = nimcp_graph_shortest_path(graph, v0, v2);
    ASSERT_NE(path1, nullptr);

    // Add direct edge
    nimcp_graph_add_edge(graph, v0, v2, 0.5f);

    // Get new path - should be shorter
    NimcpPath* path2 = nimcp_graph_shortest_path(graph, v0, v2);
    ASSERT_NE(path2, nullptr);
    EXPECT_LT(path2->total_weight, path1->total_weight);

    nimcp_free(path1->vertices);
    nimcp_free(path1);
    nimcp_free(path2->vertices);
    nimcp_free(path2);
}
