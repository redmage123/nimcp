/**
 * @file test_topology_gpu.cpp
 * @brief Unit tests for GPU topology and community detection operations
 *
 * Tests graph creation, community detection (Louvain, Label Propagation),
 * graph metrics, shortest paths, and network generation algorithms.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <cstring>
#include <algorithm>
#include <numeric>
#include <random>

// Headers already have their own extern "C" guards
#include "gpu/topology/nimcp_topology_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class TopologyGPUTest : public ::testing::Test {
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

    // Helper to create a tensor filled with a constant value
    nimcp_gpu_tensor_t* CreateFilledTensor(size_t* dims, size_t rank, float value) {
        if (!ctx) return nullptr;
        nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(ctx, dims, rank, NIMCP_GPU_PRECISION_FP32);
        if (tensor) {
            nimcp_gpu_fill(ctx, tensor, value);
        }
        return tensor;
    }

    // Helper to create 1D tensor
    nimcp_gpu_tensor_t* Create1DTensor(size_t n, float value = 0.0f) {
        size_t dims[1] = {n};
        return CreateFilledTensor(dims, 1, value);
    }

    // Helper to create 2D tensor
    nimcp_gpu_tensor_t* Create2DTensor(size_t rows, size_t cols, float value = 0.0f) {
        size_t dims[2] = {rows, cols};
        return CreateFilledTensor(dims, 2, value);
    }

    // Helper to copy tensor to host
    std::vector<float> CopyToHost(nimcp_gpu_tensor_t* tensor) {
        size_t n = tensor->numel;
        std::vector<float> host_data(n);
        nimcp_gpu_tensor_to_host(tensor, host_data.data());
        return host_data;
    }

    // Helper to set tensor from host
    nimcp_gpu_tensor_t* SetFromHost(nimcp_gpu_tensor_t* tensor, const std::vector<float>& data) {
        if (tensor) nimcp_gpu_tensor_destroy(tensor);
        size_t dims[1] = {data.size()};
        return nimcp_gpu_tensor_from_host(ctx, data.data(), dims, 1, NIMCP_GPU_PRECISION_FP32);
    }

    // Create a complete graph (all nodes connected)
    std::vector<float> CreateCompleteGraphAdjacency(int num_nodes) {
        std::vector<float> adj(num_nodes * num_nodes, 1.0f);
        // Remove self-loops
        for (int i = 0; i < num_nodes; i++) {
            adj[i * num_nodes + i] = 0.0f;
        }
        return adj;
    }

    // Create a ring graph (each node connected to neighbors)
    std::vector<float> CreateRingGraphAdjacency(int num_nodes, int k = 1) {
        std::vector<float> adj(num_nodes * num_nodes, 0.0f);
        for (int i = 0; i < num_nodes; i++) {
            for (int j = 1; j <= k; j++) {
                int next = (i + j) % num_nodes;
                int prev = (i - j + num_nodes) % num_nodes;
                adj[i * num_nodes + next] = 1.0f;
                adj[i * num_nodes + prev] = 1.0f;
            }
        }
        return adj;
    }

    // Create a star graph (one hub connected to all others)
    std::vector<float> CreateStarGraphAdjacency(int num_nodes) {
        std::vector<float> adj(num_nodes * num_nodes, 0.0f);
        // Node 0 is the hub
        for (int i = 1; i < num_nodes; i++) {
            adj[0 * num_nodes + i] = 1.0f;  // hub -> others
            adj[i * num_nodes + 0] = 1.0f;  // others -> hub
        }
        return adj;
    }

    // Create edge list from adjacency matrix
    void CreateEdgeListFromAdjacency(const std::vector<float>& adj, int num_nodes,
                                      std::vector<int>& src, std::vector<int>& dst,
                                      std::vector<float>& weights) {
        src.clear();
        dst.clear();
        weights.clear();
        for (int i = 0; i < num_nodes; i++) {
            for (int j = 0; j < num_nodes; j++) {
                if (adj[i * num_nodes + j] > 0) {
                    src.push_back(i);
                    dst.push_back(j);
                    weights.push_back(adj[i * num_nodes + j]);
                }
            }
        }
    }

    // Create a graph with clear community structure
    std::vector<float> CreateCommunityGraphAdjacency(int num_nodes, int num_communities) {
        std::vector<float> adj(num_nodes * num_nodes, 0.0f);
        int nodes_per_community = num_nodes / num_communities;

        for (int c = 0; c < num_communities; c++) {
            int start = c * nodes_per_community;
            int end = (c == num_communities - 1) ? num_nodes : (c + 1) * nodes_per_community;

            // Dense intra-community connections
            for (int i = start; i < end; i++) {
                for (int j = start; j < end; j++) {
                    if (i != j) {
                        adj[i * num_nodes + j] = 1.0f;
                    }
                }
            }
        }

        // Sparse inter-community connections
        for (int c = 0; c < num_communities - 1; c++) {
            int node1 = c * nodes_per_community;
            int node2 = (c + 1) * nodes_per_community;
            adj[node1 * num_nodes + node2] = 0.1f;
            adj[node2 * num_nodes + node1] = 0.1f;
        }

        return adj;
    }

    // Create CSR representation from edge list
    void CreateCSRFromEdges(const std::vector<int>& src, const std::vector<int>& dst,
                            int num_nodes, std::vector<int>& row_ptrs,
                            std::vector<int>& col_indices) {
        row_ptrs.resize(num_nodes + 1, 0);
        col_indices = dst;

        // Count edges per source node
        for (int s : src) {
            row_ptrs[s + 1]++;
        }

        // Cumulative sum
        for (int i = 0; i < num_nodes; i++) {
            row_ptrs[i + 1] += row_ptrs[i];
        }
    }
};

//=============================================================================
// Graph Creation Tests
//=============================================================================

TEST_F(TopologyGPUTest, GraphCreate_ReturnsValidGraph) {
    RequireGPU();

    const int num_nodes = 100;
    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_create(ctx, num_nodes, false);
    ASSERT_NE(graph, nullptr);

    EXPECT_EQ(graph->num_nodes, num_nodes);
    EXPECT_FALSE(graph->is_sparse);
    EXPECT_NE(graph->ctx, nullptr);

    nimcp_graph_gpu_destroy(graph);
}

TEST_F(TopologyGPUTest, GraphCreate_SparseMode) {
    RequireGPU();

    const int num_nodes = 100;
    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_create(ctx, num_nodes, true);
    ASSERT_NE(graph, nullptr);

    EXPECT_EQ(graph->num_nodes, num_nodes);
    EXPECT_TRUE(graph->is_sparse);

    nimcp_graph_gpu_destroy(graph);
}

TEST_F(TopologyGPUTest, GraphFromDense_CreatesValidGraph) {
    RequireGPU();

    const int num_nodes = 10;
    auto adj = CreateCompleteGraphAdjacency(num_nodes);

    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(ctx, adj.data(), num_nodes);
    ASSERT_NE(graph, nullptr);

    EXPECT_EQ(graph->num_nodes, num_nodes);
    EXPECT_NE(graph->adjacency, nullptr);

    // Complete graph should have n*(n-1) edges
    EXPECT_EQ(graph->num_edges, num_nodes * (num_nodes - 1));

    nimcp_graph_gpu_destroy(graph);
}

TEST_F(TopologyGPUTest, GraphFromEdges_CreatesValidGraph) {
    RequireGPU();

    const int num_nodes = 5;
    std::vector<int> src = {0, 0, 1, 2, 3};
    std::vector<int> dst = {1, 2, 2, 3, 4};
    std::vector<float> weights = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    int num_edges = src.size();

    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_edges(
        ctx, src.data(), dst.data(), weights.data(), num_edges, num_nodes);
    ASSERT_NE(graph, nullptr);

    EXPECT_EQ(graph->num_nodes, num_nodes);
    EXPECT_EQ(graph->num_edges, num_edges);

    nimcp_graph_gpu_destroy(graph);
}

TEST_F(TopologyGPUTest, GraphFromCSR_CreatesValidGraph) {
    RequireGPU();

    const int num_nodes = 4;
    // Create a simple path graph: 0->1->2->3
    std::vector<int> row_ptrs = {0, 1, 2, 3, 3};  // 3 edges total
    std::vector<int> col_indices = {1, 2, 3};
    std::vector<float> weights = {1.0f, 1.0f, 1.0f};
    int num_edges = 3;

    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_csr(
        ctx, row_ptrs.data(), col_indices.data(), weights.data(), num_nodes, num_edges);
    ASSERT_NE(graph, nullptr);

    EXPECT_EQ(graph->num_nodes, num_nodes);
    EXPECT_EQ(graph->num_edges, num_edges);
    EXPECT_TRUE(graph->is_sparse);

    nimcp_graph_gpu_destroy(graph);
}

TEST_F(TopologyGPUTest, GraphToCSR_ConvertsSuccessfully) {
    RequireGPU();

    const int num_nodes = 10;
    auto adj = CreateRingGraphAdjacency(num_nodes, 2);

    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(ctx, adj.data(), num_nodes);
    ASSERT_NE(graph, nullptr);
    EXPECT_FALSE(graph->is_sparse);

    bool result = nimcp_graph_gpu_to_csr(graph, 0.5f);
    EXPECT_TRUE(result);
    EXPECT_TRUE(graph->is_sparse);
    EXPECT_NE(graph->row_ptrs, nullptr);
    EXPECT_NE(graph->col_indices, nullptr);

    nimcp_graph_gpu_destroy(graph);
}

TEST_F(TopologyGPUTest, GraphToDense_ConvertsSuccessfully) {
    RequireGPU();

    const int num_nodes = 5;
    std::vector<int> row_ptrs = {0, 2, 4, 6, 8, 10};
    std::vector<int> col_indices = {1, 4, 0, 2, 1, 3, 2, 4, 0, 3};
    std::vector<float> weights(10, 1.0f);
    int num_edges = 10;

    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_csr(
        ctx, row_ptrs.data(), col_indices.data(), weights.data(), num_nodes, num_edges);
    ASSERT_NE(graph, nullptr);
    EXPECT_TRUE(graph->is_sparse);

    bool result = nimcp_graph_gpu_to_dense(graph);
    EXPECT_TRUE(result);
    EXPECT_FALSE(graph->is_sparse);
    EXPECT_NE(graph->adjacency, nullptr);

    nimcp_graph_gpu_destroy(graph);
}

TEST_F(TopologyGPUTest, GraphSetFeatures_SetsNodeFeatures) {
    RequireGPU();

    const int num_nodes = 10;
    const int feature_dim = 5;
    std::vector<float> features(num_nodes * feature_dim);
    for (int i = 0; i < num_nodes * feature_dim; i++) {
        features[i] = static_cast<float>(i) * 0.1f;
    }

    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_create(ctx, num_nodes, false);
    ASSERT_NE(graph, nullptr);

    bool result = nimcp_graph_gpu_set_features(graph, features.data(), feature_dim);
    EXPECT_TRUE(result);
    EXPECT_NE(graph->node_features, nullptr);

    nimcp_graph_gpu_destroy(graph);
}

TEST_F(TopologyGPUTest, GraphDestroy_HandlesNull) {
    nimcp_graph_gpu_destroy(nullptr);  // Should not crash
}

//=============================================================================
// Degree Computation Tests
//=============================================================================

TEST_F(TopologyGPUTest, ComputeDegree_CompleteGraph) {
    RequireGPU();

    const int num_nodes = 10;
    auto adj = CreateCompleteGraphAdjacency(num_nodes);

    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(ctx, adj.data(), num_nodes);
    ASSERT_NE(graph, nullptr);

    nimcp_gpu_tensor_t* degree = Create1DTensor(num_nodes, 0.0f);
    bool result = nimcp_topology_compute_degree(ctx, graph, degree);
    EXPECT_TRUE(result);

    auto degree_data = CopyToHost(degree);

    // In complete graph, each node has degree = n-1
    for (int i = 0; i < num_nodes; i++) {
        EXPECT_NEAR(degree_data[i], static_cast<float>(num_nodes - 1), 0.01f);
    }

    nimcp_gpu_tensor_destroy(degree);
    nimcp_graph_gpu_destroy(graph);
}

TEST_F(TopologyGPUTest, ComputeDegree_StarGraph) {
    RequireGPU();

    const int num_nodes = 10;
    auto adj = CreateStarGraphAdjacency(num_nodes);

    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(ctx, adj.data(), num_nodes);
    ASSERT_NE(graph, nullptr);

    nimcp_gpu_tensor_t* degree = Create1DTensor(num_nodes, 0.0f);
    bool result = nimcp_topology_compute_degree(ctx, graph, degree);
    EXPECT_TRUE(result);

    auto degree_data = CopyToHost(degree);

    // Hub (node 0) has degree = n-1, others have degree = 1
    EXPECT_NEAR(degree_data[0], static_cast<float>(num_nodes - 1), 0.01f);
    for (int i = 1; i < num_nodes; i++) {
        EXPECT_NEAR(degree_data[i], 1.0f, 0.01f);
    }

    nimcp_gpu_tensor_destroy(degree);
    nimcp_graph_gpu_destroy(graph);
}

TEST_F(TopologyGPUTest, ComputeWeightedDegree_VariableWeights) {
    RequireGPU();

    const int num_nodes = 4;
    std::vector<float> adj = {
        0.0f, 1.0f, 2.0f, 0.0f,
        1.0f, 0.0f, 0.0f, 3.0f,
        2.0f, 0.0f, 0.0f, 0.5f,
        0.0f, 3.0f, 0.5f, 0.0f
    };

    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(ctx, adj.data(), num_nodes);
    ASSERT_NE(graph, nullptr);

    nimcp_gpu_tensor_t* weighted_degree = Create1DTensor(num_nodes, 0.0f);
    bool result = nimcp_topology_compute_weighted_degree(ctx, graph, weighted_degree);
    EXPECT_TRUE(result);

    auto wd_data = CopyToHost(weighted_degree);

    // Node 0: 1 + 2 = 3
    EXPECT_NEAR(wd_data[0], 3.0f, 0.01f);
    // Node 1: 1 + 3 = 4
    EXPECT_NEAR(wd_data[1], 4.0f, 0.01f);
    // Node 2: 2 + 0.5 = 2.5
    EXPECT_NEAR(wd_data[2], 2.5f, 0.01f);
    // Node 3: 3 + 0.5 = 3.5
    EXPECT_NEAR(wd_data[3], 3.5f, 0.01f);

    nimcp_gpu_tensor_destroy(weighted_degree);
    nimcp_graph_gpu_destroy(graph);
}

//=============================================================================
// Clustering Coefficient Tests
//=============================================================================

TEST_F(TopologyGPUTest, ComputeClustering_CompleteGraph) {
    RequireGPU();

    const int num_nodes = 5;
    auto adj = CreateCompleteGraphAdjacency(num_nodes);

    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(ctx, adj.data(), num_nodes);
    ASSERT_NE(graph, nullptr);

    nimcp_gpu_tensor_t* clustering = Create1DTensor(num_nodes, 0.0f);
    bool result = nimcp_topology_compute_clustering(ctx, graph, clustering);
    EXPECT_TRUE(result);

    auto cc_data = CopyToHost(clustering);

    // Complete graph has clustering coefficient = 1.0 for all nodes
    for (int i = 0; i < num_nodes; i++) {
        EXPECT_NEAR(cc_data[i], 1.0f, 0.01f);
    }

    nimcp_gpu_tensor_destroy(clustering);
    nimcp_graph_gpu_destroy(graph);
}

TEST_F(TopologyGPUTest, ComputeClustering_StarGraph) {
    RequireGPU();

    const int num_nodes = 5;
    auto adj = CreateStarGraphAdjacency(num_nodes);

    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(ctx, adj.data(), num_nodes);
    ASSERT_NE(graph, nullptr);

    nimcp_gpu_tensor_t* clustering = Create1DTensor(num_nodes, 0.0f);
    bool result = nimcp_topology_compute_clustering(ctx, graph, clustering);
    EXPECT_TRUE(result);

    auto cc_data = CopyToHost(clustering);

    // Star graph has 0 clustering for all nodes (no triangles)
    for (int i = 0; i < num_nodes; i++) {
        EXPECT_NEAR(cc_data[i], 0.0f, 0.01f);
    }

    nimcp_gpu_tensor_destroy(clustering);
    nimcp_graph_gpu_destroy(graph);
}

TEST_F(TopologyGPUTest, CountTriangles_CompleteGraph) {
    RequireGPU();

    const int num_nodes = 4;
    auto adj = CreateCompleteGraphAdjacency(num_nodes);

    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(ctx, adj.data(), num_nodes);
    ASSERT_NE(graph, nullptr);

    int64_t count = 0;
    bool result = nimcp_topology_count_triangles(ctx, graph, &count);
    EXPECT_TRUE(result);

    // Complete graph with n nodes has C(n,3) = n*(n-1)*(n-2)/6 triangles
    int64_t expected = (int64_t)num_nodes * (num_nodes - 1) * (num_nodes - 2) / 6;
    EXPECT_EQ(count, expected);

    nimcp_graph_gpu_destroy(graph);
}

TEST_F(TopologyGPUTest, CountTriangles_RingGraph) {
    RequireGPU();

    const int num_nodes = 6;
    auto adj = CreateRingGraphAdjacency(num_nodes, 1);  // Simple ring

    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(ctx, adj.data(), num_nodes);
    ASSERT_NE(graph, nullptr);

    int64_t count = 0;
    bool result = nimcp_topology_count_triangles(ctx, graph, &count);
    EXPECT_TRUE(result);

    // Ring graph with k=1 has no triangles
    EXPECT_EQ(count, 0);

    nimcp_graph_gpu_destroy(graph);
}

//=============================================================================
// PageRank Tests
//=============================================================================

TEST_F(TopologyGPUTest, ComputePageRank_UniformGraph) {
    RequireGPU();

    const int num_nodes = 10;
    auto adj = CreateCompleteGraphAdjacency(num_nodes);

    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(ctx, adj.data(), num_nodes);
    ASSERT_NE(graph, nullptr);

    nimcp_gpu_tensor_t* pagerank = Create1DTensor(num_nodes, 0.0f);
    bool result = nimcp_topology_compute_pagerank(ctx, graph, 0.85f, 100, 1e-6f, pagerank);
    EXPECT_TRUE(result);

    auto pr_data = CopyToHost(pagerank);

    // In uniform graph, all nodes should have equal PageRank (1/n)
    float expected_pr = 1.0f / num_nodes;
    for (int i = 0; i < num_nodes; i++) {
        EXPECT_NEAR(pr_data[i], expected_pr, 0.05f);
    }

    // Sum should be 1.0
    float sum = 0.0f;
    for (float pr : pr_data) sum += pr;
    EXPECT_NEAR(sum, 1.0f, 0.01f);

    nimcp_gpu_tensor_destroy(pagerank);
    nimcp_graph_gpu_destroy(graph);
}

TEST_F(TopologyGPUTest, ComputePageRank_StarGraph) {
    RequireGPU();

    const int num_nodes = 10;
    auto adj = CreateStarGraphAdjacency(num_nodes);

    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(ctx, adj.data(), num_nodes);
    ASSERT_NE(graph, nullptr);

    nimcp_gpu_tensor_t* pagerank = Create1DTensor(num_nodes, 0.0f);
    bool result = nimcp_topology_compute_pagerank(ctx, graph, 0.85f, 100, 1e-6f, pagerank);
    EXPECT_TRUE(result);

    auto pr_data = CopyToHost(pagerank);

    // Hub should have higher PageRank than leaves
    float hub_pr = pr_data[0];
    float avg_leaf_pr = 0.0f;
    for (int i = 1; i < num_nodes; i++) {
        avg_leaf_pr += pr_data[i];
    }
    avg_leaf_pr /= (num_nodes - 1);

    EXPECT_GT(hub_pr, avg_leaf_pr);

    nimcp_gpu_tensor_destroy(pagerank);
    nimcp_graph_gpu_destroy(graph);
}

//=============================================================================
// Shortest Path Tests
//=============================================================================

TEST_F(TopologyGPUTest, ShortestPathBFS_PathGraph) {
    RequireGPU();

    const int num_nodes = 5;
    // Create path graph: 0 - 1 - 2 - 3 - 4
    std::vector<float> adj(num_nodes * num_nodes, 0.0f);
    for (int i = 0; i < num_nodes - 1; i++) {
        adj[i * num_nodes + (i + 1)] = 1.0f;
        adj[(i + 1) * num_nodes + i] = 1.0f;
    }

    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(ctx, adj.data(), num_nodes);
    ASSERT_NE(graph, nullptr);

    nimcp_shortest_path_result_gpu_t result;
    result.distances = Create1DTensor(num_nodes, 0.0f);
    result.predecessors = Create1DTensor(num_nodes, -1.0f);

    bool success = nimcp_shortest_path_bfs(ctx, graph, 0, &result);
    EXPECT_TRUE(success);

    auto dist_data = CopyToHost(result.distances);

    // Distances from node 0 should be 0, 1, 2, 3, 4
    for (int i = 0; i < num_nodes; i++) {
        EXPECT_NEAR(dist_data[i], static_cast<float>(i), 0.01f);
    }

    nimcp_gpu_tensor_destroy(result.distances);
    nimcp_gpu_tensor_destroy(result.predecessors);
    nimcp_graph_gpu_destroy(graph);
}

TEST_F(TopologyGPUTest, ShortestPathBFS_CompleteGraph) {
    RequireGPU();

    const int num_nodes = 10;
    auto adj = CreateCompleteGraphAdjacency(num_nodes);

    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(ctx, adj.data(), num_nodes);
    ASSERT_NE(graph, nullptr);

    nimcp_shortest_path_result_gpu_t result;
    result.distances = Create1DTensor(num_nodes, 0.0f);
    result.predecessors = Create1DTensor(num_nodes, -1.0f);

    bool success = nimcp_shortest_path_bfs(ctx, graph, 0, &result);
    EXPECT_TRUE(success);

    auto dist_data = CopyToHost(result.distances);

    // In complete graph, all distances are 1 (except from source which is 0)
    EXPECT_NEAR(dist_data[0], 0.0f, 0.01f);
    for (int i = 1; i < num_nodes; i++) {
        EXPECT_NEAR(dist_data[i], 1.0f, 0.01f);
    }

    nimcp_gpu_tensor_destroy(result.distances);
    nimcp_gpu_tensor_destroy(result.predecessors);
    nimcp_graph_gpu_destroy(graph);
}

TEST_F(TopologyGPUTest, ShortestPathDijkstra_WeightedGraph) {
    RequireGPU();

    const int num_nodes = 4;
    // Weighted graph where direct path is not shortest
    //   0 --1-- 1
    //   |       |
    //   5       1
    //   |       |
    //   3 --1-- 2
    std::vector<float> adj = {
        0.0f, 1.0f, 0.0f, 5.0f,
        1.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 1.0f,
        5.0f, 0.0f, 1.0f, 0.0f
    };

    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(ctx, adj.data(), num_nodes);
    ASSERT_NE(graph, nullptr);

    nimcp_shortest_path_result_gpu_t result;
    result.distances = Create1DTensor(num_nodes, 0.0f);
    result.predecessors = Create1DTensor(num_nodes, -1.0f);

    bool success = nimcp_shortest_path_dijkstra(ctx, graph, 0, &result);
    EXPECT_TRUE(success);

    auto dist_data = CopyToHost(result.distances);

    // Shortest paths from 0:
    // to 0: 0
    // to 1: 1
    // to 2: 2 (0->1->2)
    // to 3: 3 (0->1->2->3, not 0->3 which is 5)
    EXPECT_NEAR(dist_data[0], 0.0f, 0.01f);
    EXPECT_NEAR(dist_data[1], 1.0f, 0.01f);
    EXPECT_NEAR(dist_data[2], 2.0f, 0.01f);
    EXPECT_NEAR(dist_data[3], 3.0f, 0.01f);

    nimcp_gpu_tensor_destroy(result.distances);
    nimcp_gpu_tensor_destroy(result.predecessors);
    nimcp_graph_gpu_destroy(graph);
}

TEST_F(TopologyGPUTest, FloydWarshall_AllPairsShortestPaths) {
    RequireGPU();

    const int num_nodes = 4;
    auto adj = CreateCompleteGraphAdjacency(num_nodes);

    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(ctx, adj.data(), num_nodes);
    ASSERT_NE(graph, nullptr);

    nimcp_apsp_result_gpu_t result;
    result.distances = Create2DTensor(num_nodes, num_nodes, 0.0f);

    bool success = nimcp_shortest_path_floyd_warshall(ctx, graph, &result);
    EXPECT_TRUE(success);

    auto dist_data = CopyToHost(result.distances);

    // In complete graph, all distances are 0 (self) or 1 (others)
    for (int i = 0; i < num_nodes; i++) {
        for (int j = 0; j < num_nodes; j++) {
            float expected = (i == j) ? 0.0f : 1.0f;
            EXPECT_NEAR(dist_data[i * num_nodes + j], expected, 0.01f);
        }
    }

    // Diameter should be 1
    EXPECT_NEAR(result.diameter, 1.0f, 0.01f);
    // Average path length should be 1.0 (all non-self paths are 1)
    EXPECT_NEAR(result.avg_path_length, 1.0f, 0.01f);

    nimcp_gpu_tensor_destroy(result.distances);
    nimcp_graph_gpu_destroy(graph);
}

TEST_F(TopologyGPUTest, MultiSourceBFS_ComputesCorrectly) {
    RequireGPU();

    const int num_nodes = 5;
    auto adj = CreateRingGraphAdjacency(num_nodes, 1);

    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(ctx, adj.data(), num_nodes);
    ASSERT_NE(graph, nullptr);

    std::vector<int> sources = {0, 2};
    int num_sources = sources.size();

    nimcp_gpu_tensor_t* distances = Create2DTensor(num_sources, num_nodes, 0.0f);

    bool success = nimcp_shortest_path_multi_source_bfs(
        ctx, graph, sources.data(), num_sources, distances);
    EXPECT_TRUE(success);

    auto dist_data = CopyToHost(distances);

    // Verify distances from source 0
    EXPECT_NEAR(dist_data[0 * num_nodes + 0], 0.0f, 0.01f);  // 0 to 0
    EXPECT_NEAR(dist_data[0 * num_nodes + 1], 1.0f, 0.01f);  // 0 to 1
    EXPECT_NEAR(dist_data[0 * num_nodes + 2], 2.0f, 0.01f);  // 0 to 2

    // Verify distances from source 2
    EXPECT_NEAR(dist_data[1 * num_nodes + 2], 0.0f, 0.01f);  // 2 to 2
    EXPECT_NEAR(dist_data[1 * num_nodes + 0], 2.0f, 0.01f);  // 2 to 0

    nimcp_gpu_tensor_destroy(distances);
    nimcp_graph_gpu_destroy(graph);
}

//=============================================================================
// Community Detection Tests
//=============================================================================

TEST_F(TopologyGPUTest, LouvainCommunityDetection_FindsCommunities) {
    RequireGPU();

    const int num_nodes = 20;
    const int num_communities = 4;
    auto adj = CreateCommunityGraphAdjacency(num_nodes, num_communities);

    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(ctx, adj.data(), num_nodes);
    ASSERT_NE(graph, nullptr);

    nimcp_community_result_gpu_t* result = nimcp_community_detect_louvain(
        ctx, graph, 1.0f, 100, 1e-5f);
    ASSERT_NE(result, nullptr);

    // Should find approximately the planted number of communities
    EXPECT_GE(result->num_communities, 2);
    EXPECT_LE(result->num_communities, num_communities + 2);

    // Modularity should be positive for structured graph
    EXPECT_GT(result->modularity, 0.0f);

    nimcp_community_result_gpu_destroy(result);
    nimcp_graph_gpu_destroy(graph);
}

TEST_F(TopologyGPUTest, LabelPropagation_FindsCommunities) {
    RequireGPU();

    const int num_nodes = 20;
    const int num_communities = 4;
    auto adj = CreateCommunityGraphAdjacency(num_nodes, num_communities);

    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(ctx, adj.data(), num_nodes);
    ASSERT_NE(graph, nullptr);

    nimcp_community_result_gpu_t* result = nimcp_community_detect_label_prop(
        ctx, graph, 100);
    ASSERT_NE(result, nullptr);

    // Should find some communities
    EXPECT_GE(result->num_communities, 1);
    EXPECT_NE(result->node_communities, nullptr);

    nimcp_community_result_gpu_destroy(result);
    nimcp_graph_gpu_destroy(graph);
}

TEST_F(TopologyGPUTest, ComputeModularity_ReturnsValidScore) {
    RequireGPU();

    const int num_nodes = 12;
    const int num_communities = 3;
    auto adj = CreateCommunityGraphAdjacency(num_nodes, num_communities);

    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(ctx, adj.data(), num_nodes);
    ASSERT_NE(graph, nullptr);

    // Perfect community assignment
    std::vector<int> communities(num_nodes);
    for (int i = 0; i < num_nodes; i++) {
        communities[i] = i / (num_nodes / num_communities);
    }

    float modularity = nimcp_community_compute_modularity(
        ctx, graph, communities.data(), num_communities, 1.0f);

    // Good community structure should have positive modularity
    EXPECT_GT(modularity, 0.0f);
    EXPECT_LE(modularity, 1.0f);

    nimcp_graph_gpu_destroy(graph);
}

TEST_F(TopologyGPUTest, ComputeModularity_RandomAssignment) {
    RequireGPU();

    const int num_nodes = 20;
    auto adj = CreateCompleteGraphAdjacency(num_nodes);

    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(ctx, adj.data(), num_nodes);
    ASSERT_NE(graph, nullptr);

    // Random community assignment
    std::vector<int> communities(num_nodes);
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, 3);
    for (int i = 0; i < num_nodes; i++) {
        communities[i] = dist(rng);
    }

    float modularity = nimcp_community_compute_modularity(
        ctx, graph, communities.data(), 4, 1.0f);

    // Random assignment on complete graph should have near-zero modularity
    EXPECT_NEAR(modularity, 0.0f, 0.2f);

    nimcp_graph_gpu_destroy(graph);
}

TEST_F(TopologyGPUTest, CommunityResultDestroy_HandlesNull) {
    nimcp_community_result_gpu_destroy(nullptr);  // Should not crash
}

//=============================================================================
// Network Generation Tests
//=============================================================================

TEST_F(TopologyGPUTest, GenerateErdosRenyi_ValidGraph) {
    RequireGPU();

    const int num_nodes = 50;
    const float edge_prob = 0.3f;

    nimcp_graph_gpu_t* graph = nimcp_graph_generate_erdos_renyi(
        ctx, num_nodes, edge_prob, 12345);
    ASSERT_NE(graph, nullptr);

    EXPECT_EQ(graph->num_nodes, num_nodes);
    EXPECT_GT(graph->num_edges, 0);

    // Edge density should be approximately edge_prob
    float expected_edges = num_nodes * (num_nodes - 1) * edge_prob;
    float actual_edges = static_cast<float>(graph->num_edges);
    EXPECT_NEAR(actual_edges, expected_edges, expected_edges * 0.3f);  // 30% tolerance

    nimcp_graph_gpu_destroy(graph);
}

TEST_F(TopologyGPUTest, GenerateErdosRenyi_Reproducible) {
    RequireGPU();

    const int num_nodes = 30;
    const float edge_prob = 0.2f;
    const uint32_t seed = 42;

    nimcp_graph_gpu_t* graph1 = nimcp_graph_generate_erdos_renyi(
        ctx, num_nodes, edge_prob, seed);
    nimcp_graph_gpu_t* graph2 = nimcp_graph_generate_erdos_renyi(
        ctx, num_nodes, edge_prob, seed);

    ASSERT_NE(graph1, nullptr);
    ASSERT_NE(graph2, nullptr);

    // Same seed should produce same number of edges
    EXPECT_EQ(graph1->num_edges, graph2->num_edges);

    nimcp_graph_gpu_destroy(graph1);
    nimcp_graph_gpu_destroy(graph2);
}

TEST_F(TopologyGPUTest, GenerateBarabasiAlbert_ScaleFreeProperties) {
    RequireGPU();

    const int num_nodes = 100;
    const int m = 3;  // Each new node connects to 3 existing nodes

    nimcp_graph_gpu_t* graph = nimcp_graph_generate_barabasi_albert(
        ctx, num_nodes, m, 12345);
    ASSERT_NE(graph, nullptr);

    EXPECT_EQ(graph->num_nodes, num_nodes);
    EXPECT_GT(graph->num_edges, 0);

    // Compute degree distribution
    nimcp_gpu_tensor_t* degree = Create1DTensor(num_nodes, 0.0f);
    nimcp_topology_compute_degree(ctx, graph, degree);
    auto degree_data = CopyToHost(degree);

    // Find max degree (should be higher than average due to preferential attachment)
    float max_degree = *std::max_element(degree_data.begin(), degree_data.end());
    float avg_degree = std::accumulate(degree_data.begin(), degree_data.end(), 0.0f) / num_nodes;

    // Max degree should be significantly higher than average (hub nodes)
    EXPECT_GT(max_degree, avg_degree * 2);

    nimcp_gpu_tensor_destroy(degree);
    nimcp_graph_gpu_destroy(graph);
}

TEST_F(TopologyGPUTest, GenerateWattsStrogatz_SmallWorldProperties) {
    RequireGPU();

    const int num_nodes = 50;
    const int k = 4;  // Each node connected to k nearest neighbors
    const float rewire_prob = 0.1f;

    nimcp_graph_gpu_t* graph = nimcp_graph_generate_watts_strogatz(
        ctx, num_nodes, k, rewire_prob, 12345);
    ASSERT_NE(graph, nullptr);

    EXPECT_EQ(graph->num_nodes, num_nodes);
    EXPECT_GT(graph->num_edges, 0);

    // Compute clustering coefficient
    nimcp_gpu_tensor_t* clustering = Create1DTensor(num_nodes, 0.0f);
    nimcp_topology_compute_clustering(ctx, graph, clustering);
    auto cc_data = CopyToHost(clustering);

    // Average clustering should be relatively high for small-world
    float avg_clustering = std::accumulate(cc_data.begin(), cc_data.end(), 0.0f) / num_nodes;
    EXPECT_GT(avg_clustering, 0.1f);

    nimcp_gpu_tensor_destroy(clustering);
    nimcp_graph_gpu_destroy(graph);
}

TEST_F(TopologyGPUTest, GenerateFractal_ValidGraph) {
    RequireGPU();

    const int num_nodes = 64;
    const float fractal_dim = 2.0f;
    const float cluster_scale = 0.5f;

    nimcp_graph_gpu_t* graph = nimcp_graph_generate_fractal(
        ctx, num_nodes, fractal_dim, cluster_scale, 12345);
    ASSERT_NE(graph, nullptr);

    EXPECT_EQ(graph->num_nodes, num_nodes);
    EXPECT_GT(graph->num_edges, 0);

    nimcp_graph_gpu_destroy(graph);
}

TEST_F(TopologyGPUTest, GeneratePowerLaw_ValidDegreeDistribution) {
    RequireGPU();

    const int num_nodes = 100;
    const float gamma = -2.5f;
    const int min_degree = 2;

    nimcp_graph_gpu_t* graph = nimcp_graph_generate_power_law(
        ctx, num_nodes, gamma, min_degree, 12345);
    ASSERT_NE(graph, nullptr);

    EXPECT_EQ(graph->num_nodes, num_nodes);
    EXPECT_GT(graph->num_edges, 0);

    // Compute degree
    nimcp_gpu_tensor_t* degree = Create1DTensor(num_nodes, 0.0f);
    nimcp_topology_compute_degree(ctx, graph, degree);
    auto degree_data = CopyToHost(degree);

    // All degrees should be at least min_degree
    float min_found = *std::min_element(degree_data.begin(), degree_data.end());
    EXPECT_GE(min_found, static_cast<float>(min_degree - 1));  // Allow for small variance

    nimcp_gpu_tensor_destroy(degree);
    nimcp_graph_gpu_destroy(graph);
}

//=============================================================================
// Betweenness Centrality Tests
//=============================================================================

TEST_F(TopologyGPUTest, ComputeBetweenness_PathGraph) {
    RequireGPU();

    const int num_nodes = 5;
    // Path graph: 0 - 1 - 2 - 3 - 4
    std::vector<float> adj(num_nodes * num_nodes, 0.0f);
    for (int i = 0; i < num_nodes - 1; i++) {
        adj[i * num_nodes + (i + 1)] = 1.0f;
        adj[(i + 1) * num_nodes + i] = 1.0f;
    }

    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(ctx, adj.data(), num_nodes);
    ASSERT_NE(graph, nullptr);

    nimcp_gpu_tensor_t* betweenness = Create1DTensor(num_nodes, 0.0f);
    bool result = nimcp_topology_compute_betweenness(ctx, graph, true, betweenness);
    EXPECT_TRUE(result);

    auto bc_data = CopyToHost(betweenness);

    // Middle node (2) should have highest betweenness
    float max_bc = *std::max_element(bc_data.begin(), bc_data.end());
    EXPECT_EQ(bc_data[2], max_bc);

    // End nodes (0, 4) should have lowest betweenness
    EXPECT_LT(bc_data[0], bc_data[2]);
    EXPECT_LT(bc_data[4], bc_data[2]);

    nimcp_gpu_tensor_destroy(betweenness);
    nimcp_graph_gpu_destroy(graph);
}

TEST_F(TopologyGPUTest, ComputeBetweenness_StarGraph) {
    RequireGPU();

    const int num_nodes = 5;
    auto adj = CreateStarGraphAdjacency(num_nodes);

    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(ctx, adj.data(), num_nodes);
    ASSERT_NE(graph, nullptr);

    nimcp_gpu_tensor_t* betweenness = Create1DTensor(num_nodes, 0.0f);
    bool result = nimcp_topology_compute_betweenness(ctx, graph, true, betweenness);
    EXPECT_TRUE(result);

    auto bc_data = CopyToHost(betweenness);

    // Hub (node 0) should have highest betweenness
    float hub_bc = bc_data[0];
    for (int i = 1; i < num_nodes; i++) {
        EXPECT_GT(hub_bc, bc_data[i]);
    }

    nimcp_gpu_tensor_destroy(betweenness);
    nimcp_graph_gpu_destroy(graph);
}

//=============================================================================
// Topology Metrics Tests
//=============================================================================

TEST_F(TopologyGPUTest, ComputeMetrics_ReturnsAllMetrics) {
    RequireGPU();

    const int num_nodes = 20;
    auto adj = CreateRingGraphAdjacency(num_nodes, 2);

    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(ctx, adj.data(), num_nodes);
    ASSERT_NE(graph, nullptr);

    nimcp_topology_metrics_gpu_t* metrics = nimcp_topology_compute_metrics(ctx, graph);
    ASSERT_NE(metrics, nullptr);

    // All tensor fields should be allocated
    EXPECT_NE(metrics->degree, nullptr);
    EXPECT_NE(metrics->weighted_degree, nullptr);
    EXPECT_NE(metrics->clustering_coeff, nullptr);
    EXPECT_NE(metrics->pagerank, nullptr);

    // Global metrics should be valid
    EXPECT_GE(metrics->avg_path_length, 0.0f);
    EXPECT_GE(metrics->global_clustering, 0.0f);
    EXPECT_LE(metrics->global_clustering, 1.0f);
    EXPECT_GE(metrics->density, 0.0f);
    EXPECT_LE(metrics->density, 1.0f);

    nimcp_topology_metrics_gpu_destroy(metrics);
    nimcp_graph_gpu_destroy(graph);
}

TEST_F(TopologyGPUTest, TopologyMetricsDestroy_HandlesNull) {
    nimcp_topology_metrics_gpu_destroy(nullptr);  // Should not crash
}

//=============================================================================
// Graph Utility Tests
//=============================================================================

TEST_F(TopologyGPUTest, GraphIsValid_ReturnsTrueForValidGraph) {
    RequireGPU();

    const int num_nodes = 10;
    auto adj = CreateCompleteGraphAdjacency(num_nodes);

    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(ctx, adj.data(), num_nodes);
    ASSERT_NE(graph, nullptr);

    EXPECT_TRUE(nimcp_graph_gpu_is_valid(graph));

    nimcp_graph_gpu_destroy(graph);
}

TEST_F(TopologyGPUTest, GraphIsValid_ReturnsFalseForNull) {
    EXPECT_FALSE(nimcp_graph_gpu_is_valid(nullptr));
}

TEST_F(TopologyGPUTest, GraphStats_ReturnsCorrectValues) {
    RequireGPU();

    const int num_nodes = 10;
    auto adj = CreateCompleteGraphAdjacency(num_nodes);

    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(ctx, adj.data(), num_nodes);
    ASSERT_NE(graph, nullptr);

    float avg_degree = 0.0f;
    int max_degree = 0;
    float density = 0.0f;

    bool result = nimcp_graph_gpu_stats(graph, &avg_degree, &max_degree, &density);
    EXPECT_TRUE(result);

    // Complete graph: avg degree = n-1, max degree = n-1, density = 1.0
    EXPECT_NEAR(avg_degree, static_cast<float>(num_nodes - 1), 0.01f);
    EXPECT_EQ(max_degree, num_nodes - 1);
    EXPECT_NEAR(density, 1.0f, 0.01f);

    nimcp_graph_gpu_destroy(graph);
}

TEST_F(TopologyGPUTest, GraphToHost_CopiesCorrectly) {
    RequireGPU();

    const int num_nodes = 5;
    auto adj = CreateCompleteGraphAdjacency(num_nodes);

    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(ctx, adj.data(), num_nodes);
    ASSERT_NE(graph, nullptr);

    std::vector<float> host_adj(num_nodes * num_nodes, 0.0f);
    bool result = nimcp_graph_gpu_to_host(graph, host_adj.data());
    EXPECT_TRUE(result);

    // Verify adjacency matches original
    for (int i = 0; i < num_nodes * num_nodes; i++) {
        EXPECT_NEAR(host_adj[i], adj[i], 0.01f);
    }

    nimcp_graph_gpu_destroy(graph);
}

TEST_F(TopologyGPUTest, GraphSymmetrize_MakesUndirected) {
    RequireGPU();

    const int num_nodes = 4;
    // Directed graph
    std::vector<float> adj = {
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 0.0f, 0.0f
    };

    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(ctx, adj.data(), num_nodes);
    ASSERT_NE(graph, nullptr);

    bool result = nimcp_graph_gpu_symmetrize(graph);
    EXPECT_TRUE(result);

    std::vector<float> sym_adj(num_nodes * num_nodes, 0.0f);
    nimcp_graph_gpu_to_host(graph, sym_adj.data());

    // Check symmetry
    for (int i = 0; i < num_nodes; i++) {
        for (int j = 0; j < num_nodes; j++) {
            EXPECT_NEAR(sym_adj[i * num_nodes + j], sym_adj[j * num_nodes + i], 0.01f);
        }
    }

    nimcp_graph_gpu_destroy(graph);
}

TEST_F(TopologyGPUTest, GraphRemoveSelfLoops_RemovesLoops) {
    RequireGPU();

    const int num_nodes = 4;
    // Graph with self-loops
    std::vector<float> adj = {
        1.0f, 1.0f, 0.0f, 0.0f,
        1.0f, 1.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 1.0f, 1.0f,
        0.0f, 0.0f, 1.0f, 1.0f
    };

    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(ctx, adj.data(), num_nodes);
    ASSERT_NE(graph, nullptr);

    bool result = nimcp_graph_gpu_remove_self_loops(graph);
    EXPECT_TRUE(result);

    std::vector<float> clean_adj(num_nodes * num_nodes, 0.0f);
    nimcp_graph_gpu_to_host(graph, clean_adj.data());

    // Diagonal should be zero
    for (int i = 0; i < num_nodes; i++) {
        EXPECT_NEAR(clean_adj[i * num_nodes + i], 0.0f, 0.01f);
    }

    nimcp_graph_gpu_destroy(graph);
}

//=============================================================================
// NULL Safety Tests
//=============================================================================

TEST_F(TopologyGPUTest, GraphCreate_NullContext) {
    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_create(nullptr, 10, false);
    EXPECT_EQ(graph, nullptr);
}

TEST_F(TopologyGPUTest, GraphFromDense_NullInputs) {
    RequireGPU();

    EXPECT_EQ(nimcp_graph_gpu_from_dense(nullptr, nullptr, 10), nullptr);
    EXPECT_EQ(nimcp_graph_gpu_from_dense(ctx, nullptr, 10), nullptr);
}

TEST_F(TopologyGPUTest, GraphFromEdges_NullInputs) {
    RequireGPU();

    std::vector<int> src = {0, 1};
    std::vector<int> dst = {1, 2};

    EXPECT_EQ(nimcp_graph_gpu_from_edges(nullptr, src.data(), dst.data(), nullptr, 2, 3), nullptr);
    EXPECT_EQ(nimcp_graph_gpu_from_edges(ctx, nullptr, dst.data(), nullptr, 2, 3), nullptr);
    EXPECT_EQ(nimcp_graph_gpu_from_edges(ctx, src.data(), nullptr, nullptr, 2, 3), nullptr);
}

TEST_F(TopologyGPUTest, ComputeDegree_NullInputs) {
    RequireGPU();

    const int num_nodes = 10;
    auto adj = CreateCompleteGraphAdjacency(num_nodes);
    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(ctx, adj.data(), num_nodes);
    nimcp_gpu_tensor_t* degree = Create1DTensor(num_nodes, 0.0f);

    EXPECT_FALSE(nimcp_topology_compute_degree(nullptr, graph, degree));
    EXPECT_FALSE(nimcp_topology_compute_degree(ctx, nullptr, degree));
    EXPECT_FALSE(nimcp_topology_compute_degree(ctx, graph, nullptr));

    nimcp_gpu_tensor_destroy(degree);
    nimcp_graph_gpu_destroy(graph);
}

TEST_F(TopologyGPUTest, ComputePageRank_NullInputs) {
    RequireGPU();

    const int num_nodes = 10;
    auto adj = CreateCompleteGraphAdjacency(num_nodes);
    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(ctx, adj.data(), num_nodes);
    nimcp_gpu_tensor_t* pagerank = Create1DTensor(num_nodes, 0.0f);

    EXPECT_FALSE(nimcp_topology_compute_pagerank(nullptr, graph, 0.85f, 100, 1e-6f, pagerank));
    EXPECT_FALSE(nimcp_topology_compute_pagerank(ctx, nullptr, 0.85f, 100, 1e-6f, pagerank));
    EXPECT_FALSE(nimcp_topology_compute_pagerank(ctx, graph, 0.85f, 100, 1e-6f, nullptr));

    nimcp_gpu_tensor_destroy(pagerank);
    nimcp_graph_gpu_destroy(graph);
}

TEST_F(TopologyGPUTest, LouvainDetect_NullInputs) {
    RequireGPU();

    const int num_nodes = 10;
    auto adj = CreateCompleteGraphAdjacency(num_nodes);
    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(ctx, adj.data(), num_nodes);

    EXPECT_EQ(nimcp_community_detect_louvain(nullptr, graph, 1.0f, 100, 1e-5f), nullptr);
    EXPECT_EQ(nimcp_community_detect_louvain(ctx, nullptr, 1.0f, 100, 1e-5f), nullptr);

    nimcp_graph_gpu_destroy(graph);
}

TEST_F(TopologyGPUTest, ShortestPathBFS_NullInputs) {
    RequireGPU();

    const int num_nodes = 10;
    auto adj = CreateCompleteGraphAdjacency(num_nodes);
    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(ctx, adj.data(), num_nodes);

    nimcp_shortest_path_result_gpu_t result;
    result.distances = Create1DTensor(num_nodes, 0.0f);
    result.predecessors = Create1DTensor(num_nodes, -1.0f);

    EXPECT_FALSE(nimcp_shortest_path_bfs(nullptr, graph, 0, &result));
    EXPECT_FALSE(nimcp_shortest_path_bfs(ctx, nullptr, 0, &result));
    EXPECT_FALSE(nimcp_shortest_path_bfs(ctx, graph, 0, nullptr));

    nimcp_gpu_tensor_destroy(result.distances);
    nimcp_gpu_tensor_destroy(result.predecessors);
    nimcp_graph_gpu_destroy(graph);
}

TEST_F(TopologyGPUTest, GenerateErdosRenyi_NullContext) {
    EXPECT_EQ(nimcp_graph_generate_erdos_renyi(nullptr, 10, 0.5f, 0), nullptr);
}

TEST_F(TopologyGPUTest, GenerateBarabasiAlbert_NullContext) {
    EXPECT_EQ(nimcp_graph_generate_barabasi_albert(nullptr, 10, 3, 0), nullptr);
}

TEST_F(TopologyGPUTest, GenerateWattsStrogatz_NullContext) {
    EXPECT_EQ(nimcp_graph_generate_watts_strogatz(nullptr, 10, 4, 0.1f, 0), nullptr);
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(TopologyGPUTest, Integration_CommunityDetectionPipeline) {
    RequireGPU();

    const int num_nodes = 40;
    const int num_communities = 4;

    // Generate graph with community structure
    auto adj = CreateCommunityGraphAdjacency(num_nodes, num_communities);
    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(ctx, adj.data(), num_nodes);
    ASSERT_NE(graph, nullptr);

    // Compute initial metrics
    nimcp_topology_metrics_gpu_t* initial_metrics = nimcp_topology_compute_metrics(ctx, graph);
    ASSERT_NE(initial_metrics, nullptr);

    // Detect communities
    nimcp_community_result_gpu_t* louvain_result = nimcp_community_detect_louvain(
        ctx, graph, 1.0f, 100, 1e-5f);
    ASSERT_NE(louvain_result, nullptr);

    nimcp_community_result_gpu_t* label_prop_result = nimcp_community_detect_label_prop(
        ctx, graph, 100);
    ASSERT_NE(label_prop_result, nullptr);

    // Both methods should find communities
    EXPECT_GE(louvain_result->num_communities, 2);
    EXPECT_GE(label_prop_result->num_communities, 1);

    // Louvain typically gives higher modularity
    EXPECT_GE(louvain_result->modularity, 0.0f);

    nimcp_community_result_gpu_destroy(louvain_result);
    nimcp_community_result_gpu_destroy(label_prop_result);
    nimcp_topology_metrics_gpu_destroy(initial_metrics);
    nimcp_graph_gpu_destroy(graph);
}

TEST_F(TopologyGPUTest, Integration_NetworkAnalysisPipeline) {
    RequireGPU();

    // Generate scale-free network
    const int num_nodes = 50;
    nimcp_graph_gpu_t* graph = nimcp_graph_generate_barabasi_albert(ctx, num_nodes, 3, 42);
    ASSERT_NE(graph, nullptr);

    // Compute all metrics
    nimcp_topology_metrics_gpu_t* metrics = nimcp_topology_compute_metrics(ctx, graph);
    ASSERT_NE(metrics, nullptr);

    // Extract degrees
    auto degree_data = CopyToHost(metrics->degree);
    float max_degree = *std::max_element(degree_data.begin(), degree_data.end());
    float avg_degree = std::accumulate(degree_data.begin(), degree_data.end(), 0.0f) / num_nodes;

    // Scale-free network properties
    EXPECT_GT(max_degree, avg_degree * 2);  // Hubs exist

    // Compute shortest paths
    nimcp_apsp_result_gpu_t apsp;
    apsp.distances = Create2DTensor(num_nodes, num_nodes, 0.0f);
    bool apsp_result = nimcp_shortest_path_floyd_warshall(ctx, graph, &apsp);
    EXPECT_TRUE(apsp_result);

    // Small-world property: low diameter
    EXPECT_LT(apsp.diameter, num_nodes / 2);

    nimcp_gpu_tensor_destroy(apsp.distances);
    nimcp_topology_metrics_gpu_destroy(metrics);
    nimcp_graph_gpu_destroy(graph);
}

TEST_F(TopologyGPUTest, Integration_GraphGenerationComparison) {
    RequireGPU();

    const int num_nodes = 50;

    // Generate different graph types
    nimcp_graph_gpu_t* er_graph = nimcp_graph_generate_erdos_renyi(ctx, num_nodes, 0.2f, 42);
    nimcp_graph_gpu_t* ba_graph = nimcp_graph_generate_barabasi_albert(ctx, num_nodes, 3, 42);
    nimcp_graph_gpu_t* ws_graph = nimcp_graph_generate_watts_strogatz(ctx, num_nodes, 6, 0.1f, 42);

    ASSERT_NE(er_graph, nullptr);
    ASSERT_NE(ba_graph, nullptr);
    ASSERT_NE(ws_graph, nullptr);

    // Compute clustering for each
    nimcp_gpu_tensor_t* er_clustering = Create1DTensor(num_nodes, 0.0f);
    nimcp_gpu_tensor_t* ba_clustering = Create1DTensor(num_nodes, 0.0f);
    nimcp_gpu_tensor_t* ws_clustering = Create1DTensor(num_nodes, 0.0f);

    nimcp_topology_compute_clustering(ctx, er_graph, er_clustering);
    nimcp_topology_compute_clustering(ctx, ba_graph, ba_clustering);
    nimcp_topology_compute_clustering(ctx, ws_graph, ws_clustering);

    auto er_cc = CopyToHost(er_clustering);
    auto ba_cc = CopyToHost(ba_clustering);
    auto ws_cc = CopyToHost(ws_clustering);

    float er_avg = std::accumulate(er_cc.begin(), er_cc.end(), 0.0f) / num_nodes;
    float ba_avg = std::accumulate(ba_cc.begin(), ba_cc.end(), 0.0f) / num_nodes;
    float ws_avg = std::accumulate(ws_cc.begin(), ws_cc.end(), 0.0f) / num_nodes;

    // Watts-Strogatz should have highest clustering (small-world property)
    EXPECT_GT(ws_avg, er_avg);

    nimcp_gpu_tensor_destroy(er_clustering);
    nimcp_gpu_tensor_destroy(ba_clustering);
    nimcp_gpu_tensor_destroy(ws_clustering);
    nimcp_graph_gpu_destroy(er_graph);
    nimcp_graph_gpu_destroy(ba_graph);
    nimcp_graph_gpu_destroy(ws_graph);
}

TEST_F(TopologyGPUTest, Integration_LargeScaleGraph) {
    RequireGPU();

    const int num_nodes = 200;
    const float edge_prob = 0.05f;

    // Generate large sparse graph
    nimcp_graph_gpu_t* graph = nimcp_graph_generate_erdos_renyi(
        ctx, num_nodes, edge_prob, 12345);
    ASSERT_NE(graph, nullptr);

    // Convert to CSR for efficiency
    bool csr_result = nimcp_graph_gpu_to_csr(graph, 0.0f);
    EXPECT_TRUE(csr_result);
    EXPECT_TRUE(graph->is_sparse);

    // Compute degree on sparse graph
    nimcp_gpu_tensor_t* degree = Create1DTensor(num_nodes, 0.0f);
    bool degree_result = nimcp_topology_compute_degree(ctx, graph, degree);
    EXPECT_TRUE(degree_result);

    auto degree_data = CopyToHost(degree);
    float total_degree = std::accumulate(degree_data.begin(), degree_data.end(), 0.0f);

    // Total degree should be approximately 2 * expected_edges
    float expected_edges = num_nodes * (num_nodes - 1) * edge_prob;
    EXPECT_NEAR(total_degree / 2, expected_edges, expected_edges * 0.3f);

    // Run PageRank on sparse graph
    nimcp_gpu_tensor_t* pagerank = Create1DTensor(num_nodes, 0.0f);
    bool pr_result = nimcp_topology_compute_pagerank(ctx, graph, 0.85f, 100, 1e-6f, pagerank);
    EXPECT_TRUE(pr_result);

    auto pr_data = CopyToHost(pagerank);
    float pr_sum = std::accumulate(pr_data.begin(), pr_data.end(), 0.0f);
    EXPECT_NEAR(pr_sum, 1.0f, 0.01f);

    nimcp_gpu_tensor_destroy(degree);
    nimcp_gpu_tensor_destroy(pagerank);
    nimcp_graph_gpu_destroy(graph);
}

TEST_F(TopologyGPUTest, Integration_ConnectedComponentsAnalysis) {
    RequireGPU();

    const int num_nodes = 30;

    // Create two disconnected cliques
    std::vector<float> adj(num_nodes * num_nodes, 0.0f);
    int clique_size = num_nodes / 2;

    // First clique (nodes 0 to 14)
    for (int i = 0; i < clique_size; i++) {
        for (int j = 0; j < clique_size; j++) {
            if (i != j) {
                adj[i * num_nodes + j] = 1.0f;
            }
        }
    }

    // Second clique (nodes 15 to 29)
    for (int i = clique_size; i < num_nodes; i++) {
        for (int j = clique_size; j < num_nodes; j++) {
            if (i != j) {
                adj[i * num_nodes + j] = 1.0f;
            }
        }
    }

    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(ctx, adj.data(), num_nodes);
    ASSERT_NE(graph, nullptr);

    // Detect communities - should find exactly 2
    nimcp_community_result_gpu_t* result = nimcp_community_detect_louvain(
        ctx, graph, 1.0f, 100, 1e-5f);
    ASSERT_NE(result, nullptr);

    // Should find 2 communities (the two cliques)
    EXPECT_EQ(result->num_communities, 2);

    // Very high modularity for disconnected cliques
    EXPECT_GT(result->modularity, 0.4f);

    nimcp_community_result_gpu_destroy(result);
    nimcp_graph_gpu_destroy(graph);
}
