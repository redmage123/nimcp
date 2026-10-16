/**
 * @file test_graph_gpu.cpp
 * @brief Unit tests for GPU Graph module with DAO pattern and GraphQL utilities
 *
 * Tests cover:
 * - CSR conversion from dense matrix
 * - BFS traversal correctness
 * - Clustering coefficient on known graphs (clique, star, path)
 * - Hub identification on scale-free networks
 * - DAO CRUD operations
 * - GraphQL query parsing and execution
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <cstring>
#include <algorithm>
#include <numeric>
#include <random>
#include <set>

// Headers with extern "C" guards
#include "gpu/graph/nimcp_graph_gpu.h"
#include "gpu/graph/nimcp_graph_dao.h"
#include "gpu/graphql/nimcp_graphql_utils.h"
#include "gpu/context/nimcp_gpu_context.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class GraphGPUTest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* ctx = nullptr;
    bool gpu_available = false;

    void SetUp() override {
        ctx = nimcp_gpu_context_create_auto();
        gpu_available = (ctx != nullptr && nimcp_gpu_context_is_valid(ctx));
    }

    void TearDown() override {
        if (ctx) {
            nimcp_gpu_context_destroy(ctx);
            ctx = nullptr;
        }
    }

    void RequireGPU() {
        if (!gpu_available) {
            GTEST_SKIP() << "GPU not available, skipping test";
        }
    }

    // Create a complete graph (all nodes connected)
    std::vector<float> CreateCompleteGraph(int n) {
        std::vector<float> adj(n * n, 1.0f);
        // Remove self-loops
        for (int i = 0; i < n; i++) {
            adj[i * n + i] = 0.0f;
        }
        return adj;
    }

    // Create a star graph (node 0 is hub)
    std::vector<float> CreateStarGraph(int n) {
        std::vector<float> adj(n * n, 0.0f);
        for (int i = 1; i < n; i++) {
            adj[0 * n + i] = 1.0f;  // hub -> leaves
            adj[i * n + 0] = 1.0f;  // leaves -> hub
        }
        return adj;
    }

    // Create a path graph (linear chain)
    std::vector<float> CreatePathGraph(int n) {
        std::vector<float> adj(n * n, 0.0f);
        for (int i = 0; i < n - 1; i++) {
            adj[i * n + (i + 1)] = 1.0f;
            adj[(i + 1) * n + i] = 1.0f;
        }
        return adj;
    }

    // Create a ring graph
    std::vector<float> CreateRingGraph(int n) {
        std::vector<float> adj(n * n, 0.0f);
        for (int i = 0; i < n; i++) {
            int next = (i + 1) % n;
            adj[i * n + next] = 1.0f;
            adj[next * n + i] = 1.0f;
        }
        return adj;
    }

    // Create CSR from adjacency matrix
    void AdjacencyToCSR(const std::vector<float>& adj, int n, float threshold,
                        std::vector<int>& row_offsets,
                        std::vector<int>& col_indices,
                        std::vector<float>& weights) {
        row_offsets.resize(n + 1);
        col_indices.clear();
        weights.clear();

        row_offsets[0] = 0;
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                float val = adj[i * n + j];
                if (std::abs(val) > threshold && i != j) {
                    col_indices.push_back(j);
                    weights.push_back(val);
                }
            }
            row_offsets[i + 1] = col_indices.size();
        }
    }

    // Create edge list from adjacency matrix
    void AdjacencyToEdgeList(const std::vector<float>& adj, int n,
                             std::vector<int>& src, std::vector<int>& dst,
                             std::vector<float>& weights) {
        src.clear();
        dst.clear();
        weights.clear();

        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                if (adj[i * n + j] > 0 && i != j) {
                    src.push_back(i);
                    dst.push_back(j);
                    weights.push_back(adj[i * n + j]);
                }
            }
        }
    }
};

//=============================================================================
// Graph Creation Tests
//=============================================================================

TEST_F(GraphGPUTest, CreateEmptyGraph_ReturnsValidGraph) {
    RequireGPU();

    nimcp_gpu_graph_t* graph = nimcp_gpu_graph_create(ctx, 100, 500);
    ASSERT_NE(graph, nullptr);

    EXPECT_EQ(graph->num_vertices, 100);
    EXPECT_EQ(graph->num_edges, 500);
    EXPECT_TRUE(nimcp_gpu_graph_is_valid(graph));

    nimcp_gpu_graph_destroy(graph);
}

TEST_F(GraphGPUTest, CreateFromAdjacency_CompleteGraph) {
    RequireGPU();

    const int n = 10;
    auto adj = CreateCompleteGraph(n);

    nimcp_gpu_graph_t* graph = nimcp_gpu_graph_from_adjacency(ctx, adj.data(), n, 0.5f);
    ASSERT_NE(graph, nullptr);

    EXPECT_EQ(graph->num_vertices, n);
    // Complete graph has n*(n-1) directed edges
    EXPECT_EQ(graph->num_edges, n * (n - 1));

    nimcp_gpu_graph_destroy(graph);
}

TEST_F(GraphGPUTest, CreateFromCSR_PathGraph) {
    RequireGPU();

    const int n = 5;
    auto adj = CreatePathGraph(n);

    std::vector<int> row_offsets, col_indices;
    std::vector<float> weights;
    AdjacencyToCSR(adj, n, 0.0f, row_offsets, col_indices, weights);

    nimcp_gpu_graph_t* graph = nimcp_gpu_graph_from_csr(
        ctx, row_offsets.data(), col_indices.data(), weights.data(),
        n, col_indices.size());

    ASSERT_NE(graph, nullptr);
    EXPECT_EQ(graph->num_vertices, n);
    EXPECT_EQ(graph->num_edges, col_indices.size());

    nimcp_gpu_graph_destroy(graph);
}

TEST_F(GraphGPUTest, CreateFromEdgeList_StarGraph) {
    RequireGPU();

    const int n = 5;
    auto adj = CreateStarGraph(n);

    std::vector<int> src, dst;
    std::vector<float> weights;
    AdjacencyToEdgeList(adj, n, src, dst, weights);

    nimcp_gpu_graph_t* graph = nimcp_gpu_graph_from_edge_list(
        ctx, src.data(), dst.data(), weights.data(), src.size(), n);

    ASSERT_NE(graph, nullptr);
    EXPECT_EQ(graph->num_vertices, n);
    // Star graph has 2*(n-1) directed edges
    EXPECT_EQ(graph->num_edges, 2 * (n - 1));

    nimcp_gpu_graph_destroy(graph);
}

TEST_F(GraphGPUTest, CloneGraph_CreatesIdenticalCopy) {
    RequireGPU();

    const int n = 10;
    auto adj = CreateCompleteGraph(n);

    nimcp_gpu_graph_t* original = nimcp_gpu_graph_from_adjacency(ctx, adj.data(), n, 0.5f);
    ASSERT_NE(original, nullptr);

    nimcp_gpu_graph_t* clone = nimcp_gpu_graph_clone(original);
    ASSERT_NE(clone, nullptr);

    EXPECT_EQ(clone->num_vertices, original->num_vertices);
    EXPECT_EQ(clone->num_edges, original->num_edges);

    nimcp_gpu_graph_destroy(original);
    nimcp_gpu_graph_destroy(clone);
}

//=============================================================================
// BFS Traversal Tests
//=============================================================================

TEST_F(GraphGPUTest, BFS_PathGraph_CorrectDistances) {
    RequireGPU();

    const int n = 5;
    auto adj = CreatePathGraph(n);

    nimcp_gpu_graph_t* graph = nimcp_gpu_graph_from_adjacency(ctx, adj.data(), n, 0.0f);
    ASSERT_NE(graph, nullptr);

    // Allocate distances on device
    float* d_distances = nullptr;
    cudaMalloc(&d_distances, n * sizeof(float));

    nimcp_error_t err = nimcp_gpu_graph_bfs(graph, 0, d_distances);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Copy distances to host
    std::vector<float> h_distances(n);
    cudaMemcpy(h_distances.data(), d_distances, n * sizeof(float), cudaMemcpyDeviceToHost);

    // In a path graph from node 0: distances should be 0, 1, 2, 3, 4
    for (int i = 0; i < n; i++) {
        EXPECT_FLOAT_EQ(h_distances[i], (float)i) << "Distance to node " << i;
    }

    cudaFree(d_distances);
    nimcp_gpu_graph_destroy(graph);
}

TEST_F(GraphGPUTest, BFS_CompleteGraph_AllDistanceOne) {
    RequireGPU();

    const int n = 10;
    auto adj = CreateCompleteGraph(n);

    nimcp_gpu_graph_t* graph = nimcp_gpu_graph_from_adjacency(ctx, adj.data(), n, 0.5f);
    ASSERT_NE(graph, nullptr);

    float* d_distances = nullptr;
    cudaMalloc(&d_distances, n * sizeof(float));

    nimcp_error_t err = nimcp_gpu_graph_bfs(graph, 0, d_distances);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    std::vector<float> h_distances(n);
    cudaMemcpy(h_distances.data(), d_distances, n * sizeof(float), cudaMemcpyDeviceToHost);

    // In complete graph, source has distance 0, all others have distance 1
    EXPECT_FLOAT_EQ(h_distances[0], 0.0f);
    for (int i = 1; i < n; i++) {
        EXPECT_FLOAT_EQ(h_distances[i], 1.0f) << "Distance to node " << i;
    }

    cudaFree(d_distances);
    nimcp_gpu_graph_destroy(graph);
}

TEST_F(GraphGPUTest, BFS_StarGraph_HubConnectsAll) {
    RequireGPU();

    const int n = 6;
    auto adj = CreateStarGraph(n);

    nimcp_gpu_graph_t* graph = nimcp_gpu_graph_from_adjacency(ctx, adj.data(), n, 0.0f);
    ASSERT_NE(graph, nullptr);

    float* d_distances = nullptr;
    cudaMalloc(&d_distances, n * sizeof(float));

    // BFS from hub (node 0)
    nimcp_error_t err = nimcp_gpu_graph_bfs(graph, 0, d_distances);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    std::vector<float> h_distances(n);
    cudaMemcpy(h_distances.data(), d_distances, n * sizeof(float), cudaMemcpyDeviceToHost);

    // All leaves should be distance 1 from hub
    EXPECT_FLOAT_EQ(h_distances[0], 0.0f);
    for (int i = 1; i < n; i++) {
        EXPECT_FLOAT_EQ(h_distances[i], 1.0f);
    }

    // BFS from leaf (node 1) - all other leaves should be distance 2
    err = nimcp_gpu_graph_bfs(graph, 1, d_distances);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    cudaMemcpy(h_distances.data(), d_distances, n * sizeof(float), cudaMemcpyDeviceToHost);

    EXPECT_FLOAT_EQ(h_distances[1], 0.0f);  // Source
    EXPECT_FLOAT_EQ(h_distances[0], 1.0f);  // Hub
    for (int i = 2; i < n; i++) {
        EXPECT_FLOAT_EQ(h_distances[i], 2.0f);  // Other leaves
    }

    cudaFree(d_distances);
    nimcp_gpu_graph_destroy(graph);
}

//=============================================================================
// Clustering Coefficient Tests
//=============================================================================

TEST_F(GraphGPUTest, ClusteringCoeff_CompleteGraph_AllOnes) {
    RequireGPU();

    const int n = 5;
    auto adj = CreateCompleteGraph(n);

    nimcp_gpu_graph_t* graph = nimcp_gpu_graph_from_adjacency(ctx, adj.data(), n, 0.5f);
    ASSERT_NE(graph, nullptr);

    float* d_coeffs = nullptr;
    cudaMalloc(&d_coeffs, n * sizeof(float));

    nimcp_error_t err = nimcp_gpu_graph_clustering_coeff(graph, d_coeffs);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    std::vector<float> h_coeffs(n);
    cudaMemcpy(h_coeffs.data(), d_coeffs, n * sizeof(float), cudaMemcpyDeviceToHost);

    // Complete graph has clustering coefficient of 1 for all vertices
    for (int i = 0; i < n; i++) {
        EXPECT_NEAR(h_coeffs[i], 1.0f, 0.01f) << "Clustering at node " << i;
    }

    cudaFree(d_coeffs);
    nimcp_gpu_graph_destroy(graph);
}

TEST_F(GraphGPUTest, ClusteringCoeff_StarGraph_ZeroForHub) {
    RequireGPU();

    const int n = 5;
    auto adj = CreateStarGraph(n);

    nimcp_gpu_graph_t* graph = nimcp_gpu_graph_from_adjacency(ctx, adj.data(), n, 0.0f);
    ASSERT_NE(graph, nullptr);

    float* d_coeffs = nullptr;
    cudaMalloc(&d_coeffs, n * sizeof(float));

    nimcp_error_t err = nimcp_gpu_graph_clustering_coeff(graph, d_coeffs);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    std::vector<float> h_coeffs(n);
    cudaMemcpy(h_coeffs.data(), d_coeffs, n * sizeof(float), cudaMemcpyDeviceToHost);

    // Hub has 0 clustering (leaves are not connected to each other)
    EXPECT_NEAR(h_coeffs[0], 0.0f, 0.01f);

    // Leaves have degree 1, so clustering is 0 (undefined, treated as 0)
    for (int i = 1; i < n; i++) {
        EXPECT_NEAR(h_coeffs[i], 0.0f, 0.01f);
    }

    cudaFree(d_coeffs);
    nimcp_gpu_graph_destroy(graph);
}

TEST_F(GraphGPUTest, ClusteringCoeff_PathGraph_ZeroForAll) {
    RequireGPU();

    const int n = 5;
    auto adj = CreatePathGraph(n);

    nimcp_gpu_graph_t* graph = nimcp_gpu_graph_from_adjacency(ctx, adj.data(), n, 0.0f);
    ASSERT_NE(graph, nullptr);

    float* d_coeffs = nullptr;
    cudaMalloc(&d_coeffs, n * sizeof(float));

    nimcp_error_t err = nimcp_gpu_graph_clustering_coeff(graph, d_coeffs);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    std::vector<float> h_coeffs(n);
    cudaMemcpy(h_coeffs.data(), d_coeffs, n * sizeof(float), cudaMemcpyDeviceToHost);

    // Path graph has no triangles, so clustering is 0 for all
    for (int i = 0; i < n; i++) {
        EXPECT_NEAR(h_coeffs[i], 0.0f, 0.01f);
    }

    cudaFree(d_coeffs);
    nimcp_gpu_graph_destroy(graph);
}

TEST_F(GraphGPUTest, AvgClustering_CompleteGraph_IsOne) {
    RequireGPU();

    const int n = 6;
    auto adj = CreateCompleteGraph(n);

    nimcp_gpu_graph_t* graph = nimcp_gpu_graph_from_adjacency(ctx, adj.data(), n, 0.5f);
    ASSERT_NE(graph, nullptr);

    float avg_clustering = 0.0f;
    nimcp_error_t err = nimcp_gpu_graph_avg_clustering(graph, &avg_clustering);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_NEAR(avg_clustering, 1.0f, 0.01f);

    nimcp_gpu_graph_destroy(graph);
}

//=============================================================================
// Hub Identification Tests
//=============================================================================

TEST_F(GraphGPUTest, FindHubs_StarGraph_HubIdentified) {
    RequireGPU();

    const int n = 10;
    auto adj = CreateStarGraph(n);

    nimcp_gpu_graph_t* graph = nimcp_gpu_graph_from_adjacency(ctx, adj.data(), n, 0.0f);
    ASSERT_NE(graph, nullptr);

    std::vector<int> hub_ids(10);
    size_t num_hubs = nimcp_gpu_graph_find_hubs(graph, 0.5f, hub_ids.data(), 10);

    // Hub (node 0) should be the only high-centrality vertex
    EXPECT_GE(num_hubs, 1);
    bool found_hub = false;
    for (size_t i = 0; i < num_hubs; i++) {
        if (hub_ids[i] == 0) {
            found_hub = true;
            break;
        }
    }
    EXPECT_TRUE(found_hub) << "Hub node 0 should be identified";

    nimcp_gpu_graph_destroy(graph);
}

TEST_F(GraphGPUTest, DegreeCentrality_StarGraph_HubHasHighest) {
    RequireGPU();

    const int n = 5;
    auto adj = CreateStarGraph(n);

    nimcp_gpu_graph_t* graph = nimcp_gpu_graph_from_adjacency(ctx, adj.data(), n, 0.0f);
    ASSERT_NE(graph, nullptr);

    float* d_centrality = nullptr;
    cudaMalloc(&d_centrality, n * sizeof(float));

    nimcp_error_t err = nimcp_gpu_graph_degree_centrality(graph, d_centrality);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    std::vector<float> h_centrality(n);
    cudaMemcpy(h_centrality.data(), d_centrality, n * sizeof(float), cudaMemcpyDeviceToHost);

    // Hub should have highest centrality
    float max_centrality = *std::max_element(h_centrality.begin(), h_centrality.end());
    EXPECT_EQ(h_centrality[0], max_centrality);

    // Hub centrality should be (n-1)/(n-1) = 1.0
    EXPECT_NEAR(h_centrality[0], 1.0f, 0.01f);

    cudaFree(d_centrality);
    nimcp_gpu_graph_destroy(graph);
}

//=============================================================================
// Small-World Metrics Tests
//=============================================================================

TEST_F(GraphGPUTest, SmallWorldCoeff_CompleteGraph_ReturnsValue) {
    RequireGPU();

    const int n = 10;
    auto adj = CreateCompleteGraph(n);

    nimcp_gpu_graph_t* graph = nimcp_gpu_graph_from_adjacency(ctx, adj.data(), n, 0.5f);
    ASSERT_NE(graph, nullptr);

    float sigma = nimcp_gpu_graph_small_world_coeff(graph);
    // Complete graphs are not particularly small-world, but should compute
    EXPECT_GE(sigma, 0.0f);

    nimcp_gpu_graph_destroy(graph);
}

TEST_F(GraphGPUTest, AvgPathLength_PathGraph_Computed) {
    RequireGPU();

    const int n = 5;
    auto adj = CreatePathGraph(n);

    nimcp_gpu_graph_t* graph = nimcp_gpu_graph_from_adjacency(ctx, adj.data(), n, 0.0f);
    ASSERT_NE(graph, nullptr);

    float avg_path = 0.0f;
    nimcp_error_t err = nimcp_gpu_graph_avg_path_length(graph, 5, &avg_path);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GT(avg_path, 0.0f);

    nimcp_gpu_graph_destroy(graph);
}

//=============================================================================
// Graph Statistics Tests
//=============================================================================

TEST_F(GraphGPUTest, GraphStats_CompleteGraph_Correct) {
    RequireGPU();

    const int n = 5;
    auto adj = CreateCompleteGraph(n);

    nimcp_gpu_graph_t* graph = nimcp_gpu_graph_from_adjacency(ctx, adj.data(), n, 0.5f);
    ASSERT_NE(graph, nullptr);

    float avg_degree = 0.0f;
    int max_degree = 0, min_degree = 0;
    float density = 0.0f;

    nimcp_error_t err = nimcp_gpu_graph_stats(graph, &avg_degree, &max_degree,
                                               &min_degree, &density);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Complete graph: all vertices have degree n-1
    EXPECT_NEAR(avg_degree, (float)(n - 1), 0.01f);
    EXPECT_EQ(max_degree, n - 1);
    EXPECT_EQ(min_degree, n - 1);
    EXPECT_NEAR(density, 1.0f, 0.01f);

    nimcp_gpu_graph_destroy(graph);
}

//=============================================================================
// DAO CRUD Tests
//=============================================================================

TEST_F(GraphGPUTest, DAO_Create_ReturnsValidDAO) {
    RequireGPU();

    nimcp_gpu_graph_dao_t* dao = nimcp_graph_dao_create_gpu(ctx, 16);
    ASSERT_NE(dao, nullptr);

    nimcp_graph_dao_destroy(dao);
}

TEST_F(GraphGPUTest, DAO_CreateAndRead_ReturnsGraph) {
    RequireGPU();

    nimcp_gpu_graph_dao_t* dao = nimcp_graph_dao_create_gpu(ctx, 16);
    ASSERT_NE(dao, nullptr);

    // Create a graph
    const int n = 5;
    auto adj = CreateCompleteGraph(n);
    nimcp_gpu_graph_t* graph = nimcp_gpu_graph_from_adjacency(ctx, adj.data(), n, 0.5f);
    ASSERT_NE(graph, nullptr);

    // Store in DAO
    int id = nimcp_graph_dao_create(dao, graph);
    EXPECT_GT(id, 0);

    // Read back
    nimcp_gpu_graph_t* read_graph = nullptr;
    nimcp_error_t err = nimcp_graph_dao_read(dao, id, &read_graph);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_NE(read_graph, nullptr);
    EXPECT_EQ(read_graph->num_vertices, n);

    nimcp_graph_dao_destroy(dao);
}

TEST_F(GraphGPUTest, DAO_Delete_RemovesGraph) {
    RequireGPU();

    nimcp_gpu_graph_dao_t* dao = nimcp_graph_dao_create_gpu(ctx, 16);
    ASSERT_NE(dao, nullptr);

    // Create and store
    const int n = 5;
    auto adj = CreateCompleteGraph(n);
    nimcp_gpu_graph_t* graph = nimcp_gpu_graph_from_adjacency(ctx, adj.data(), n, 0.5f);
    int id = nimcp_graph_dao_create(dao, graph);
    EXPECT_GT(id, 0);

    // Verify exists
    EXPECT_TRUE(nimcp_graph_dao_exists(dao, id));

    // Delete
    nimcp_error_t err = nimcp_graph_dao_delete(dao, id);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Verify deleted
    EXPECT_FALSE(nimcp_graph_dao_exists(dao, id));

    nimcp_graph_dao_destroy(dao);
}

TEST_F(GraphGPUTest, DAO_BatchInsert_InsertsMultiple) {
    RequireGPU();

    nimcp_gpu_graph_dao_t* dao = nimcp_graph_dao_create_gpu(ctx, 16);
    ASSERT_NE(dao, nullptr);

    // Create multiple graphs
    const int num_graphs = 5;
    std::vector<nimcp_gpu_graph_t*> graphs(num_graphs);
    for (int i = 0; i < num_graphs; i++) {
        auto adj = CreateRingGraph(5 + i);
        graphs[i] = nimcp_gpu_graph_from_adjacency(ctx, adj.data(), 5 + i, 0.0f);
        ASSERT_NE(graphs[i], nullptr);
    }

    // Batch insert
    std::vector<int> ids(num_graphs);
    size_t inserted = nimcp_graph_dao_batch_insert(dao, graphs.data(), num_graphs, ids.data());
    EXPECT_EQ(inserted, num_graphs);

    // Verify all inserted
    for (int i = 0; i < num_graphs; i++) {
        EXPECT_TRUE(nimcp_graph_dao_exists(dao, ids[i]));
    }

    nimcp_graph_dao_destroy(dao);
}

TEST_F(GraphGPUTest, DAO_CacheStats_TracksHitsMisses) {
    RequireGPU();

    nimcp_gpu_graph_dao_t* dao = nimcp_graph_dao_create_gpu(ctx, 16);
    ASSERT_NE(dao, nullptr);

    // Create and store
    const int n = 5;
    auto adj = CreateCompleteGraph(n);
    nimcp_gpu_graph_t* graph = nimcp_gpu_graph_from_adjacency(ctx, adj.data(), n, 0.5f);
    int id = nimcp_graph_dao_create(dao, graph);

    // Read multiple times to generate hits
    nimcp_gpu_graph_t* read_graph = nullptr;
    for (int i = 0; i < 5; i++) {
        nimcp_graph_dao_read(dao, id, &read_graph);
    }

    // Check cache stats
    uint64_t hits = 0, misses = 0;
    size_t count = 0;
    nimcp_graph_dao_cache_stats(dao, &hits, &misses, &count);

    // After first read (miss), subsequent reads should be hits
    EXPECT_GE(hits, 4);

    nimcp_graph_dao_destroy(dao);
}

//=============================================================================
// GraphQL Utils Tests
//=============================================================================

TEST_F(GraphGPUTest, GraphQL_ExecutorCreate_ReturnsValid) {
    RequireGPU();

    nimcp_graphql_executor_t* exec = nimcp_graphql_executor_create(ctx);
    ASSERT_NE(exec, nullptr);

    nimcp_graphql_executor_destroy(exec);
}

TEST_F(GraphGPUTest, GraphQL_QueryCreate_ReturnsValid) {
    nimcp_graph_query_t* query = nimcp_graph_query_create();
    ASSERT_NE(query, nullptr);

    nimcp_error_t err = nimcp_graph_query_set_type(query, "vertices");
    EXPECT_EQ(err, NIMCP_SUCCESS);

    err = nimcp_graph_query_set_filter(query, "degree > 5");
    EXPECT_EQ(err, NIMCP_SUCCESS);

    std::vector<int> vertices = {1, 2, 3};
    err = nimcp_graph_query_set_vertices(query, vertices.data(), vertices.size());
    EXPECT_EQ(err, NIMCP_SUCCESS);

    nimcp_graph_query_destroy(query);
}

TEST_F(GraphGPUTest, GraphQL_FilterParsing_SimpleComparison) {
    nimcp_graphql_filter_node_t* filter = nimcp_graphql_parse_filter("degree > 5");
    ASSERT_NE(filter, nullptr);

    EXPECT_EQ(filter->field, NIMCP_GRAPHQL_FIELD_DEGREE);
    EXPECT_EQ(filter->op, NIMCP_GRAPHQL_OP_GT);
    EXPECT_FLOAT_EQ(filter->value, 5.0f);

    nimcp_graphql_filter_destroy(filter);
}

TEST_F(GraphGPUTest, GraphQL_FilterParsing_LogicalAnd) {
    nimcp_graphql_filter_node_t* filter =
        nimcp_graphql_parse_filter("degree > 5 AND clustering > 0.5");
    ASSERT_NE(filter, nullptr);

    EXPECT_EQ(filter->op, NIMCP_GRAPHQL_OP_AND);
    EXPECT_NE(filter->left, nullptr);
    EXPECT_NE(filter->right, nullptr);

    nimcp_graphql_filter_destroy(filter);
}

TEST_F(GraphGPUTest, GraphQL_FilterEvaluate_PassesCorrectly) {
    nimcp_graphql_filter_node_t* filter = nimcp_graphql_parse_filter("degree > 5");
    ASSERT_NE(filter, nullptr);

    // Degree 10 should pass (10 > 5)
    bool passes = nimcp_graphql_filter_evaluate(filter, 10, 0.0f, 0.0f, 0.0f, 0.0f, nullptr, 0);
    EXPECT_TRUE(passes);

    // Degree 3 should fail (3 > 5 is false)
    passes = nimcp_graphql_filter_evaluate(filter, 3, 0.0f, 0.0f, 0.0f, 0.0f, nullptr, 0);
    EXPECT_FALSE(passes);

    nimcp_graphql_filter_destroy(filter);
}

TEST_F(GraphGPUTest, GraphQL_FilterEvaluate_LogicalOperators) {
    nimcp_graphql_filter_node_t* filter =
        nimcp_graphql_parse_filter("degree > 5 AND clustering > 0.5");
    ASSERT_NE(filter, nullptr);

    // Both conditions pass
    bool passes = nimcp_graphql_filter_evaluate(filter, 10, 0.0f, 0.0f, 0.8f, 0.0f, nullptr, 0);
    EXPECT_TRUE(passes);

    // Degree fails
    passes = nimcp_graphql_filter_evaluate(filter, 3, 0.0f, 0.0f, 0.8f, 0.0f, nullptr, 0);
    EXPECT_FALSE(passes);

    // Clustering fails
    passes = nimcp_graphql_filter_evaluate(filter, 10, 0.0f, 0.0f, 0.3f, 0.0f, nullptr, 0);
    EXPECT_FALSE(passes);

    nimcp_graphql_filter_destroy(filter);
}

TEST_F(GraphGPUTest, GraphQL_QueryTypeConversion_Works) {
    EXPECT_EQ(nimcp_graphql_query_type_from_string("vertices"),
              NIMCP_GRAPHQL_QUERY_VERTICES);
    EXPECT_EQ(nimcp_graphql_query_type_from_string("edges"),
              NIMCP_GRAPHQL_QUERY_EDGES);
    EXPECT_EQ(nimcp_graphql_query_type_from_string("neighbors"),
              NIMCP_GRAPHQL_QUERY_NEIGHBORS);

    EXPECT_STREQ(nimcp_graphql_query_type_to_string(NIMCP_GRAPHQL_QUERY_VERTICES),
                 "vertices");
}

TEST_F(GraphGPUTest, GraphQL_ValidateQuery_AcceptsValid) {
    char error_msg[256];

    bool valid = nimcp_graphql_validate_query("{ vertices() }", error_msg, sizeof(error_msg));
    EXPECT_TRUE(valid);

    valid = nimcp_graphql_validate_query("{ neighbors(id: 0) }", error_msg, sizeof(error_msg));
    EXPECT_TRUE(valid);
}

TEST_F(GraphGPUTest, GraphQL_ValidateQuery_RejectsInvalid) {
    char error_msg[256];

    bool valid = nimcp_graphql_validate_query("", error_msg, sizeof(error_msg));
    EXPECT_FALSE(valid);

    valid = nimcp_graphql_validate_query("{ unknown() }", error_msg, sizeof(error_msg));
    EXPECT_FALSE(valid);

    valid = nimcp_graphql_validate_query("{ vertices(", error_msg, sizeof(error_msg));
    EXPECT_FALSE(valid);
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(GraphGPUTest, Integration_DAOWithGraphQL_QueryWorks) {
    RequireGPU();

    // Create DAO and populate with graphs
    nimcp_gpu_graph_dao_t* dao = nimcp_graph_dao_create_gpu(ctx, 16);
    ASSERT_NE(dao, nullptr);

    const int n = 10;
    auto adj = CreateCompleteGraph(n);
    nimcp_gpu_graph_t* graph = nimcp_gpu_graph_from_adjacency(ctx, adj.data(), n, 0.5f);
    int id = nimcp_graph_dao_create(dao, graph);
    EXPECT_GT(id, 0);

    // Create GraphQL executor and set graph
    nimcp_graphql_executor_t* exec = nimcp_graphql_executor_create(ctx);
    ASSERT_NE(exec, nullptr);
    nimcp_graphql_executor_set_graph(exec, graph);

    // Execute query
    nimcp_graph_query_t* query = nimcp_graph_query_create();
    nimcp_graph_query_set_type(query, "vertices");
    nimcp_graph_query_set_filter(query, "degree > 5");

    nimcp_error_t err = nimcp_graphql_execute_query(exec, query);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    nimcp_graph_query_destroy(query);
    nimcp_graphql_executor_destroy(exec);
    nimcp_graph_dao_destroy(dao);
}

//=============================================================================
// Modularity Tests
//=============================================================================

TEST_F(GraphGPUTest, Modularity_TwoCommunities_PositiveScore) {
    RequireGPU();

    // Create two disconnected cliques (clear community structure)
    const int n = 10;
    std::vector<float> adj(n * n, 0.0f);

    // First clique: nodes 0-4
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 5; j++) {
            if (i != j) adj[i * n + j] = 1.0f;
        }
    }
    // Second clique: nodes 5-9
    for (int i = 5; i < 10; i++) {
        for (int j = 5; j < 10; j++) {
            if (i != j) adj[i * n + j] = 1.0f;
        }
    }
    // One edge between cliques
    adj[4 * n + 5] = 1.0f;
    adj[5 * n + 4] = 1.0f;

    nimcp_gpu_graph_t* graph = nimcp_gpu_graph_from_adjacency(ctx, adj.data(), n, 0.0f);
    ASSERT_NE(graph, nullptr);

    // Correct community assignment
    std::vector<int> labels = {0, 0, 0, 0, 0, 1, 1, 1, 1, 1};
    float modularity = 0.0f;

    nimcp_error_t err = nimcp_gpu_graph_modularity(graph, labels.data(), &modularity);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Modularity should be positive for good community detection
    EXPECT_GT(modularity, 0.0f);

    nimcp_gpu_graph_destroy(graph);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
