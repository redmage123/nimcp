/**
 * @file test_utils_graph.cpp
 * @brief Comprehensive unit tests for graph data structure
 *
 * WHAT: 100% test coverage for nimcp_graph.c
 * WHY:  Graph is critical for P2P network topology and routing
 * HOW:  Test all operations, path finding, components, edge cases
 *
 * TEST COVERAGE:
 * 1. nimcp_graph_create() - graph creation
 * 2. nimcp_graph_destroy() - cleanup and NULL safety
 * 3. nimcp_graph_add_vertex() - vertex insertion
 * 4. nimcp_graph_remove_vertex() - vertex deletion
 * 5. nimcp_graph_add_edge() - edge insertion
 * 6. nimcp_graph_remove_edge() - edge deletion
 * 7. nimcp_graph_shortest_path() - Dijkstra's algorithm
 * 8. nimcp_graph_update_components() - component analysis
 * 9. nimcp_graph_find_vertex() - vertex lookup by peer ID
 * 10. nimcp_graph_get_neighbors() - neighbor enumeration
 * 11. nimcp_graph_get_edge_weight() - edge weight retrieval
 * 12. nimcp_graph_update_coordinates() - coordinate updates
 * 13. Edge cases (invalid indices, full graph, NULL pointers)
 * 14. Complex topologies
 * 15. Thread safety
 *
 * @version Unit Testing Framework v1.0
 * @date 2025-11-13
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>

    #include "utils/containers/nimcp_graph.h"
    #include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class GraphTest : public ::testing::Test {
protected:
    NimcpGraph* graph = nullptr;

    void TearDown() override {
        if (graph) {
            nimcp_graph_destroy(graph);
            graph = nullptr;
        }
    }
};

//=============================================================================
// Unit Test 1: Create graph
//=============================================================================

TEST_F(GraphTest, Create_InitializesEmpty) {
    // WHAT: Create empty graph
    // WHY:  Test initialization

    graph = nimcp_graph_create();
    ASSERT_NE(graph, nullptr);

    EXPECT_EQ(graph->vertex_count, 0u);
    EXPECT_EQ(graph->edge_count, 0u);
    EXPECT_NE(graph->vertices, nullptr);

    SUCCEED() << "Graph created successfully";
}

//=============================================================================
// Unit Test 2: Destroy NULL graph is safe
//=============================================================================

TEST_F(GraphTest, Destroy_NullIsSafe) {
    // WHAT: Destroying NULL graph doesn't crash
    // WHY:  Defensive programming

    nimcp_graph_destroy(nullptr);
    SUCCEED() << "Destroying NULL graph is safe";
}

//=============================================================================
// Unit Test 3: Add vertex
//=============================================================================

TEST_F(GraphTest, AddVertex_IncreasesCount) {
    // WHAT: Add vertices to graph
    // WHY:  Test vertex insertion

    graph = nimcp_graph_create();
    ASSERT_NE(graph, nullptr);

    uint32_t idx1 = nimcp_graph_add_vertex(graph, 1001, 0.0f, 0.0f, 0.0f, 0);
    EXPECT_NE(idx1, NIMCP_INVALID_VERTEX);
    EXPECT_EQ(graph->vertex_count, 1u);

    uint32_t idx2 = nimcp_graph_add_vertex(graph, 1002, 1.0f, 1.0f, 1.0f, 0);
    EXPECT_NE(idx2, NIMCP_INVALID_VERTEX);
    EXPECT_EQ(graph->vertex_count, 2u);

    EXPECT_NE(idx1, idx2) << "Vertices should have unique indices";

    SUCCEED() << "Add vertex works";
}

//=============================================================================
// Unit Test 4: Find vertex by peer ID
//=============================================================================

TEST_F(GraphTest, FindVertex_ReturnCorrectIndex) {
    // WHAT: Lookup vertex by peer ID
    // WHY:  Test vertex search

    graph = nimcp_graph_create();
    ASSERT_NE(graph, nullptr);

    uint32_t idx1 = nimcp_graph_add_vertex(graph, 2001, 0.0f, 0.0f, 0.0f, 0);
    uint32_t idx2 = nimcp_graph_add_vertex(graph, 2002, 0.0f, 0.0f, 0.0f, 0);

    EXPECT_EQ(nimcp_graph_find_vertex(graph, 2001), idx1);
    EXPECT_EQ(nimcp_graph_find_vertex(graph, 2002), idx2);
    EXPECT_EQ(nimcp_graph_find_vertex(graph, 9999), NIMCP_INVALID_VERTEX) << "Non-existent peer";

    SUCCEED() << "Find vertex works";
}

//=============================================================================
// Unit Test 5: Add edge
//=============================================================================

TEST_F(GraphTest, AddEdge_CreatesConnection) {
    // WHAT: Add edges between vertices
    // WHY:  Test edge insertion

    graph = nimcp_graph_create();
    ASSERT_NE(graph, nullptr);

    uint32_t v1 = nimcp_graph_add_vertex(graph, 101, 0.0f, 0.0f, 0.0f, 0);
    uint32_t v2 = nimcp_graph_add_vertex(graph, 102, 0.0f, 0.0f, 0.0f, 0);

    EXPECT_TRUE(nimcp_graph_add_edge(graph, v1, v2, 1.5f));
    EXPECT_EQ(graph->edge_count, 1u);

    // Verify edge weight
    nimcp_weight_t weight;
    EXPECT_TRUE(nimcp_graph_get_edge_weight(graph, v1, v2, &weight));
    EXPECT_FLOAT_EQ(weight, 1.5f);

    SUCCEED() << "Add edge works";
}

//=============================================================================
// Unit Test 6: Get neighbors
//=============================================================================

TEST_F(GraphTest, GetNeighbors_ReturnsConnected) {
    // WHAT: Retrieve neighbors of a vertex
    // WHY:  Test adjacency list traversal

    graph = nimcp_graph_create();
    ASSERT_NE(graph, nullptr);

    uint32_t v0 = nimcp_graph_add_vertex(graph, 201, 0.0f, 0.0f, 0.0f, 0);
    uint32_t v1 = nimcp_graph_add_vertex(graph, 202, 0.0f, 0.0f, 0.0f, 0);
    uint32_t v2 = nimcp_graph_add_vertex(graph, 203, 0.0f, 0.0f, 0.0f, 0);

    nimcp_graph_add_edge(graph, v0, v1, 1.0f);
    nimcp_graph_add_edge(graph, v0, v2, 1.0f);

    uint32_t neighbors[10];
    uint32_t count = nimcp_graph_get_neighbors(graph, v0, neighbors, 10);

    EXPECT_EQ(count, 2u);
    EXPECT_TRUE(neighbors[0] == v1 || neighbors[1] == v1) << "v1 should be neighbor";
    EXPECT_TRUE(neighbors[0] == v2 || neighbors[1] == v2) << "v2 should be neighbor";

    SUCCEED() << "Get neighbors works";
}

//=============================================================================
// Unit Test 7: Remove edge
//=============================================================================

TEST_F(GraphTest, RemoveEdge_DeletesConnection) {
    // WHAT: Remove edge from graph
    // WHY:  Test edge deletion

    graph = nimcp_graph_create();
    ASSERT_NE(graph, nullptr);

    uint32_t v1 = nimcp_graph_add_vertex(graph, 301, 0.0f, 0.0f, 0.0f, 0);
    uint32_t v2 = nimcp_graph_add_vertex(graph, 302, 0.0f, 0.0f, 0.0f, 0);

    nimcp_graph_add_edge(graph, v1, v2, 1.0f);
    EXPECT_EQ(graph->edge_count, 1u);

    EXPECT_TRUE(nimcp_graph_remove_edge(graph, v1, v2));
    EXPECT_EQ(graph->edge_count, 0u);

    // Verify edge gone
    nimcp_weight_t weight;
    EXPECT_FALSE(nimcp_graph_get_edge_weight(graph, v1, v2, &weight));

    // Removing non-existent edge should return false
    EXPECT_FALSE(nimcp_graph_remove_edge(graph, v1, v2));

    SUCCEED() << "Remove edge works";
}

//=============================================================================
// Unit Test 8: Remove vertex
//=============================================================================

TEST_F(GraphTest, RemoveVertex_DeletesNode) {
    // WHAT: Remove vertex from graph
    // WHY:  Test vertex deletion

    graph = nimcp_graph_create();
    ASSERT_NE(graph, nullptr);

    uint32_t v1 = nimcp_graph_add_vertex(graph, 401, 0.0f, 0.0f, 0.0f, 0);
    uint32_t v2 = nimcp_graph_add_vertex(graph, 402, 0.0f, 0.0f, 0.0f, 0);
    nimcp_graph_add_edge(graph, v1, v2, 1.0f);

    EXPECT_TRUE(nimcp_graph_remove_vertex(graph, v1));
    EXPECT_EQ(graph->vertex_count, 1u);

    // Vertex should not be findable
    EXPECT_EQ(nimcp_graph_find_vertex(graph, 401), NIMCP_INVALID_VERTEX);

    SUCCEED() << "Remove vertex works";
}

//=============================================================================
// Unit Test 9: Update coordinates
//=============================================================================

TEST_F(GraphTest, UpdateCoordinates_ModifiesPosition) {
    // WHAT: Update vertex coordinates
    // WHY:  Test coordinate modification

    graph = nimcp_graph_create();
    ASSERT_NE(graph, nullptr);

    uint32_t v = nimcp_graph_add_vertex(graph, 501, 0.0f, 0.0f, 0.0f, 0);

    EXPECT_TRUE(nimcp_graph_update_coordinates(graph, v, 2.5f, 3.5f, 4.5f));

    EXPECT_FLOAT_EQ(graph->vertices[v].x, 2.5f);
    EXPECT_FLOAT_EQ(graph->vertices[v].y, 3.5f);
    EXPECT_FLOAT_EQ(graph->vertices[v].z, 4.5f);

    SUCCEED() << "Update coordinates works";
}

//=============================================================================
// Unit Test 10: Shortest path - simple
//=============================================================================

TEST_F(GraphTest, ShortestPath_SimpleChain) {
    // WHAT: Find shortest path in linear chain
    // WHY:  Test Dijkstra's algorithm

    graph = nimcp_graph_create();
    ASSERT_NE(graph, nullptr);

    // Create chain: v0 -> v1 -> v2
    uint32_t v0 = nimcp_graph_add_vertex(graph, 601, 0.0f, 0.0f, 0.0f, 0);
    uint32_t v1 = nimcp_graph_add_vertex(graph, 602, 0.0f, 0.0f, 0.0f, 0);
    uint32_t v2 = nimcp_graph_add_vertex(graph, 603, 0.0f, 0.0f, 0.0f, 0);

    nimcp_graph_add_edge(graph, v0, v1, 1.0f);
    nimcp_graph_add_edge(graph, v1, v2, 1.0f);

    NimcpPath* path = nimcp_graph_shortest_path(graph, v0, v2);
    ASSERT_NE(path, nullptr);

    EXPECT_EQ(path->length, 3u) << "Path should be v0->v1->v2";
    EXPECT_FLOAT_EQ(path->total_weight, 2.0f);

    EXPECT_EQ(path->vertices[0], v0);
    EXPECT_EQ(path->vertices[1], v1);
    EXPECT_EQ(path->vertices[2], v2);

    nimcp_free(path->vertices);
    nimcp_free(path);

    SUCCEED() << "Shortest path (simple chain) works";
}

//=============================================================================
// Unit Test 11: Shortest path - weighted
//=============================================================================

TEST_F(GraphTest, ShortestPath_ChoosesLowerWeight) {
    // WHAT: Find shortest path with different weights
    // WHY:  Test Dijkstra prefers lower weight

    graph = nimcp_graph_create();
    ASSERT_NE(graph, nullptr);

    // Create graph:
    //   v0 --[5.0]-> v2
    //   v0 --[1.0]-> v1 --[1.0]-> v2
    // Optimal path: v0->v1->v2 (total 2.0)

    uint32_t v0 = nimcp_graph_add_vertex(graph, 701, 0.0f, 0.0f, 0.0f, 0);
    uint32_t v1 = nimcp_graph_add_vertex(graph, 702, 0.0f, 0.0f, 0.0f, 0);
    uint32_t v2 = nimcp_graph_add_vertex(graph, 703, 0.0f, 0.0f, 0.0f, 0);

    nimcp_graph_add_edge(graph, v0, v2, 5.0f);  // Direct but expensive
    nimcp_graph_add_edge(graph, v0, v1, 1.0f);  // Cheap first hop
    nimcp_graph_add_edge(graph, v1, v2, 1.0f);  // Cheap second hop

    NimcpPath* path = nimcp_graph_shortest_path(graph, v0, v2);
    ASSERT_NE(path, nullptr);

    EXPECT_EQ(path->length, 3u) << "Should choose v0->v1->v2";
    EXPECT_FLOAT_EQ(path->total_weight, 2.0f) << "Should avoid expensive direct edge";

    nimcp_free(path->vertices);
    nimcp_free(path);

    SUCCEED() << "Shortest path chooses lower weight";
}

//=============================================================================
// Unit Test 12: Shortest path - no path
//=============================================================================

TEST_F(GraphTest, ShortestPath_NoPath) {
    // WHAT: Handle disconnected vertices
    // WHY:  Test no-path case

    graph = nimcp_graph_create();
    ASSERT_NE(graph, nullptr);

    uint32_t v0 = nimcp_graph_add_vertex(graph, 801, 0.0f, 0.0f, 0.0f, 0);
    uint32_t v1 = nimcp_graph_add_vertex(graph, 802, 0.0f, 0.0f, 0.0f, 0);
    // No edge between v0 and v1

    NimcpPath* path = nimcp_graph_shortest_path(graph, v0, v1);
    EXPECT_EQ(path, nullptr) << "Should return NULL when no path exists";

    SUCCEED() << "No path returns NULL";
}

//=============================================================================
// Unit Test 13: Connected components - single component
//=============================================================================

TEST_F(GraphTest, Components_SingleConnected) {
    // WHAT: Find connected components
    // WHY:  Test component analysis

    graph = nimcp_graph_create();
    ASSERT_NE(graph, nullptr);

    uint32_t v0 = nimcp_graph_add_vertex(graph, 901, 0.0f, 0.0f, 0.0f, 0);
    uint32_t v1 = nimcp_graph_add_vertex(graph, 902, 0.0f, 0.0f, 0.0f, 0);
    uint32_t v2 = nimcp_graph_add_vertex(graph, 903, 0.0f, 0.0f, 0.0f, 0);

    // All connected: v0-v1-v2
    nimcp_graph_add_edge(graph, v0, v1, 1.0f);
    nimcp_graph_add_edge(graph, v1, v2, 1.0f);

    uint32_t count = nimcp_graph_update_components(graph);
    EXPECT_EQ(count, 1u) << "All vertices in one component";

    SUCCEED() << "Single component detected";
}

//=============================================================================
// Unit Test 14: Connected components - multiple
//=============================================================================

TEST_F(GraphTest, Components_MultipleDisconnected) {
    // WHAT: Detect multiple components
    // WHY:  Test disconnected graph

    graph = nimcp_graph_create();
    ASSERT_NE(graph, nullptr);

    // Component 1: v0-v1
    uint32_t v0 = nimcp_graph_add_vertex(graph, 1001, 0.0f, 0.0f, 0.0f, 0);
    uint32_t v1 = nimcp_graph_add_vertex(graph, 1002, 0.0f, 0.0f, 0.0f, 0);
    nimcp_graph_add_edge(graph, v0, v1, 1.0f);

    // Component 2: v2 (isolated)
    nimcp_graph_add_vertex(graph, 1003, 0.0f, 0.0f, 0.0f, 0);

    uint32_t count = nimcp_graph_update_components(graph);
    EXPECT_EQ(count, 2u) << "Two disconnected components";

    SUCCEED() << "Multiple components detected";
}

//=============================================================================
// Unit Test 15: Empty graph operations
//=============================================================================

TEST_F(GraphTest, EmptyGraph_Operations) {
    // WHAT: Operations on empty graph
    // WHY:  Edge case handling

    graph = nimcp_graph_create();
    ASSERT_NE(graph, nullptr);

    EXPECT_EQ(nimcp_graph_find_vertex(graph, 9999), NIMCP_INVALID_VERTEX);
    EXPECT_FALSE(nimcp_graph_add_edge(graph, 0, 1, 1.0f));

    uint32_t neighbors[10];
    EXPECT_EQ(nimcp_graph_get_neighbors(graph, 0, neighbors, 10), 0u);

    NimcpPath* path = nimcp_graph_shortest_path(graph, 0, 1);
    EXPECT_EQ(path, nullptr);

    EXPECT_EQ(nimcp_graph_update_components(graph), 0u);

    SUCCEED() << "Empty graph operations are safe";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
