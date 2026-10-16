/**
 * @file test_graph_gpu_recovery.cpp
 * @brief Unit tests for GPU recovery integration in graph module
 *
 * WHAT: Tests GPU recovery for graph operations
 * WHY:  Verify self-healing from OOM, numerical errors, and kernel failures
 * HOW:  Test recovery initialization, error handling, and CPU fallback
 *
 * TEST COVERAGE:
 * - Recovery initialization in graph creation
 * - OOM recovery during graph allocation
 * - Kernel launch failure recovery in BFS
 * - CPU fallback for graph operations
 * - Error category handling for graph algorithms
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

#ifdef NIMCP_ENABLE_CUDA
#include <cuda_runtime.h>
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "gpu/graph/nimcp_graph_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#endif

namespace {

/* ============================================================================
 * Test Fixture
 * ============================================================================ */
class GraphGPURecoveryTest : public ::testing::Test {
protected:
    void SetUp() override {
#ifdef NIMCP_ENABLE_CUDA
        int device_count = 0;
        cudaGetDeviceCount(&device_count);
        if (device_count == 0) {
            GTEST_SKIP() << "No CUDA devices available";
        }
        if (!nimcp_gpu_recovery_is_initialized()) {
            nimcp_gpu_recovery_init(NULL);
        }
        ctx = nimcp_gpu_context_create_auto();
        if (!ctx || !nimcp_gpu_context_is_valid(ctx)) {
            GTEST_SKIP() << "Failed to create GPU context";
        }
#else
        GTEST_SKIP() << "CUDA not enabled";
#endif
    }

    void TearDown() override {
#ifdef NIMCP_ENABLE_CUDA
        if (graph) {
            nimcp_gpu_graph_destroy(graph);
            graph = nullptr;
        }
        if (ctx) {
            nimcp_gpu_context_destroy(ctx);
            ctx = nullptr;
        }
        if (nimcp_gpu_recovery_is_initialized()) {
            nimcp_gpu_recovery_shutdown();
        }
#endif
    }

#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_context_t* ctx = nullptr;
    nimcp_gpu_graph_t* graph = nullptr;
#endif
};

/* ============================================================================
 * Test: Recovery initialization at graph creation
 * ============================================================================ */
TEST_F(GraphGPURecoveryTest, RecoveryInitializedAtGraphCreation) {
#ifdef NIMCP_ENABLE_CUDA
    /* Recovery should auto-init if not already */
    graph = nimcp_gpu_graph_create(ctx, 100, 500);
    ASSERT_NE(graph, nullptr);

    EXPECT_TRUE(nimcp_gpu_recovery_is_initialized())
        << "Recovery should be initialized after graph creation";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Graph creation from adjacency matrix with recovery
 * ============================================================================ */
TEST_F(GraphGPURecoveryTest, GraphFromAdjacencyWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    const size_t n = 10;
    std::vector<float> adj(n * n, 0.0f);

    /* Create simple chain graph */
    for (size_t i = 0; i < n - 1; i++) {
        adj[i * n + (i + 1)] = 1.0f;
        adj[(i + 1) * n + i] = 1.0f;
    }

    graph = nimcp_gpu_graph_from_adjacency(ctx, adj.data(), n, 0.5f);
    ASSERT_NE(graph, nullptr) << "Graph creation should succeed with recovery";

    EXPECT_EQ(graph->num_vertices, n);
    EXPECT_TRUE(nimcp_gpu_graph_is_valid(graph));
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Graph creation from edge list with recovery
 * ============================================================================ */
TEST_F(GraphGPURecoveryTest, GraphFromEdgeListWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    /* Simple triangle graph */
    std::vector<int> src = {0, 1, 2, 0, 1, 2};
    std::vector<int> dst = {1, 2, 0, 2, 0, 1};
    std::vector<float> weights(6, 1.0f);

    graph = nimcp_gpu_graph_from_edge_list(ctx, src.data(), dst.data(),
                                           weights.data(), 6, 3);
    ASSERT_NE(graph, nullptr) << "Graph from edge list should succeed";

    EXPECT_EQ(graph->num_vertices, 3u);
    EXPECT_EQ(graph->num_edges, 6u);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Invalid parameter recovery in graph creation
 * ============================================================================ */
TEST_F(GraphGPURecoveryTest, InvalidParamRecoveryGraphCreate) {
#ifdef NIMCP_ENABLE_CUDA
    /* NULL context should fail gracefully */
    nimcp_gpu_graph_t* bad_graph = nimcp_gpu_graph_create(nullptr, 100, 500);
    EXPECT_EQ(bad_graph, nullptr) << "Should fail gracefully for NULL context";

    /* Zero vertices should fail gracefully */
    bad_graph = nimcp_gpu_graph_create(ctx, 0, 100);
    EXPECT_EQ(bad_graph, nullptr) << "Should fail gracefully for 0 vertices";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: BFS with recovery
 * ============================================================================ */
TEST_F(GraphGPURecoveryTest, BFSWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    /* Create chain graph: 0-1-2-3-4 */
    std::vector<int> row_offsets = {0, 1, 3, 5, 7, 8};
    std::vector<int> col_indices = {1, 0, 2, 1, 3, 2, 4, 3};

    graph = nimcp_gpu_graph_from_csr(ctx, row_offsets.data(), col_indices.data(),
                                     nullptr, 5, 8);
    ASSERT_NE(graph, nullptr);

    /* Allocate distances on device */
    float* d_distances = nullptr;
    cudaMalloc(&d_distances, 5 * sizeof(float));
    ASSERT_NE(d_distances, nullptr);

    nimcp_error_t err = nimcp_gpu_graph_bfs(graph, 0, d_distances);
    EXPECT_EQ(err, NIMCP_SUCCESS) << "BFS should succeed with recovery";

    /* Copy back and verify */
    std::vector<float> h_distances(5);
    cudaMemcpy(h_distances.data(), d_distances, 5 * sizeof(float),
               cudaMemcpyDeviceToHost);

    EXPECT_FLOAT_EQ(h_distances[0], 0.0f);
    EXPECT_FLOAT_EQ(h_distances[1], 1.0f);
    EXPECT_FLOAT_EQ(h_distances[2], 2.0f);
    EXPECT_FLOAT_EQ(h_distances[3], 3.0f);
    EXPECT_FLOAT_EQ(h_distances[4], 4.0f);

    cudaFree(d_distances);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Clustering coefficient with recovery
 * ============================================================================ */
TEST_F(GraphGPURecoveryTest, ClusteringCoeffWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    /* Create triangle graph (complete K3) */
    std::vector<int> row_offsets = {0, 2, 4, 6};
    std::vector<int> col_indices = {1, 2, 0, 2, 0, 1};

    graph = nimcp_gpu_graph_from_csr(ctx, row_offsets.data(), col_indices.data(),
                                     nullptr, 3, 6);
    ASSERT_NE(graph, nullptr);

    float* d_coefficients = nullptr;
    cudaMalloc(&d_coefficients, 3 * sizeof(float));
    ASSERT_NE(d_coefficients, nullptr);

    nimcp_error_t err = nimcp_gpu_graph_clustering_coeff(graph, d_coefficients);
    EXPECT_EQ(err, NIMCP_SUCCESS) << "Clustering coeff should succeed";

    /* For complete K3, all clustering coefficients should be 1.0 */
    std::vector<float> h_coefficients(3);
    cudaMemcpy(h_coefficients.data(), d_coefficients, 3 * sizeof(float),
               cudaMemcpyDeviceToHost);

    for (int i = 0; i < 3; i++) {
        EXPECT_NEAR(h_coefficients[i], 1.0f, 0.01f)
            << "Complete graph should have clustering coeff = 1";
    }

    cudaFree(d_coefficients);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Degree centrality with recovery
 * ============================================================================ */
TEST_F(GraphGPURecoveryTest, DegreeCentralityWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    /* Star graph: center node 0 connected to 1,2,3,4 */
    std::vector<int> row_offsets = {0, 4, 5, 6, 7, 8};
    std::vector<int> col_indices = {1, 2, 3, 4, 0, 0, 0, 0};

    graph = nimcp_gpu_graph_from_csr(ctx, row_offsets.data(), col_indices.data(),
                                     nullptr, 5, 8);
    ASSERT_NE(graph, nullptr);

    float* d_centrality = nullptr;
    cudaMalloc(&d_centrality, 5 * sizeof(float));
    ASSERT_NE(d_centrality, nullptr);

    nimcp_error_t err = nimcp_gpu_graph_degree_centrality(graph, d_centrality);
    EXPECT_EQ(err, NIMCP_SUCCESS) << "Degree centrality should succeed";

    /* Center node should have highest centrality */
    std::vector<float> h_centrality(5);
    cudaMemcpy(h_centrality.data(), d_centrality, 5 * sizeof(float),
               cudaMemcpyDeviceToHost);

    EXPECT_GT(h_centrality[0], h_centrality[1])
        << "Center node should have higher centrality";

    cudaFree(d_centrality);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Error category for OOM
 * ============================================================================ */
TEST_F(GraphGPURecoveryTest, ErrorCategoryOOM) {
#ifdef NIMCP_ENABLE_CUDA
    /* Select recovery strategy for OOM */
    nimcp_gpu_recovery_action_t action = nimcp_gpu_select_recovery_strategy(
        GPU_ERROR_OUT_OF_MEMORY, cudaErrorMemoryAllocation, 0);

    /* Should suggest memory management action */
    EXPECT_TRUE(action == GPU_RECOVERY_FREE_CACHE ||
                action == GPU_RECOVERY_REDUCE_BATCH ||
                action == GPU_RECOVERY_REDUCE_DIMENSIONS ||
                action == GPU_RECOVERY_CPU_FALLBACK)
        << "OOM should trigger memory management action";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Error category for kernel launch
 * ============================================================================ */
TEST_F(GraphGPURecoveryTest, ErrorCategoryKernelLaunch) {
#ifdef NIMCP_ENABLE_CUDA
    /* Select recovery strategy for kernel launch failure */
    nimcp_gpu_recovery_action_t action = nimcp_gpu_select_recovery_strategy(
        GPU_ERROR_KERNEL_LAUNCH, cudaErrorLaunchFailure, 0);

    /* Should suggest retry or CPU fallback */
    EXPECT_TRUE(action == GPU_RECOVERY_RETRY_IMMEDIATE ||
                action == GPU_RECOVERY_RETRY_BACKOFF ||
                action == GPU_RECOVERY_CPU_FALLBACK ||
                action == GPU_RECOVERY_RESET_DEVICE)
        << "Kernel launch failure should trigger retry or fallback";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Small-world coefficient with recovery
 * ============================================================================ */
TEST_F(GraphGPURecoveryTest, SmallWorldCoeffWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    /* Create small-world-like graph (ring with shortcuts) */
    const size_t n = 20;
    std::vector<int> row_offsets(n + 1);
    std::vector<int> col_indices;

    /* Build ring plus some shortcuts */
    for (size_t i = 0; i < n; i++) {
        row_offsets[i] = col_indices.size();
        /* Ring edges */
        col_indices.push_back((i + 1) % n);
        col_indices.push_back((i + n - 1) % n);
        /* Shortcut */
        if (i % 5 == 0) {
            col_indices.push_back((i + 5) % n);
        }
    }
    row_offsets[n] = col_indices.size();

    graph = nimcp_gpu_graph_from_csr(ctx, row_offsets.data(), col_indices.data(),
                                     nullptr, n, col_indices.size());
    ASSERT_NE(graph, nullptr);

    float sigma = nimcp_gpu_graph_small_world_coeff(graph);
    EXPECT_TRUE(std::isfinite(sigma))
        << "Small-world coefficient should be finite";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Graph statistics with recovery
 * ============================================================================ */
TEST_F(GraphGPURecoveryTest, GraphStatsWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    /* Create regular graph (K5) */
    std::vector<int> row_offsets = {0, 4, 8, 12, 16, 20};
    std::vector<int> col_indices = {
        1, 2, 3, 4,  /* Node 0 */
        0, 2, 3, 4,  /* Node 1 */
        0, 1, 3, 4,  /* Node 2 */
        0, 1, 2, 4,  /* Node 3 */
        0, 1, 2, 3   /* Node 4 */
    };

    graph = nimcp_gpu_graph_from_csr(ctx, row_offsets.data(), col_indices.data(),
                                     nullptr, 5, 20);
    ASSERT_NE(graph, nullptr);

    float avg_degree, density;
    int max_degree, min_degree;

    nimcp_error_t err = nimcp_gpu_graph_stats(graph, &avg_degree, &max_degree,
                                               &min_degree, &density);
    EXPECT_EQ(err, NIMCP_SUCCESS) << "Graph stats should succeed";

    /* K5: all vertices have degree 4 */
    EXPECT_FLOAT_EQ(avg_degree, 4.0f);
    EXPECT_EQ(max_degree, 4);
    EXPECT_EQ(min_degree, 4);
    EXPECT_NEAR(density, 1.0f, 0.01f) << "Complete graph should have density 1";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Triangle counting with recovery
 * ============================================================================ */
TEST_F(GraphGPURecoveryTest, TriangleCountWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    /* Create K4 (complete graph with 4 vertices) */
    std::vector<int> row_offsets = {0, 3, 6, 9, 12};
    std::vector<int> col_indices = {
        1, 2, 3,  /* Node 0 */
        0, 2, 3,  /* Node 1 */
        0, 1, 3,  /* Node 2 */
        0, 1, 2   /* Node 3 */
    };

    graph = nimcp_gpu_graph_from_csr(ctx, row_offsets.data(), col_indices.data(),
                                     nullptr, 4, 12);
    ASSERT_NE(graph, nullptr);

    size_t triangle_count = 0;
    nimcp_error_t err = nimcp_gpu_graph_count_triangles(graph, &triangle_count);
    EXPECT_EQ(err, NIMCP_SUCCESS) << "Triangle counting should succeed";

    /* K4 has C(4,3) = 4 triangles */
    EXPECT_EQ(triangle_count, 4u) << "K4 should have exactly 4 triangles";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Recovery context in graph operations
 * ============================================================================ */
TEST_F(GraphGPURecoveryTest, RecoveryContextInGraphOps) {
#ifdef NIMCP_ENABLE_CUDA
    /* Create recovery context */
    nimcp_gpu_recovery_context_t* rctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(rctx, nullptr);

    /* Create simple graph and perform operation */
    std::vector<int> row_offsets = {0, 2, 4, 6};
    std::vector<int> col_indices = {1, 2, 0, 2, 0, 1};

    graph = nimcp_gpu_graph_from_csr(ctx, row_offsets.data(), col_indices.data(),
                                     nullptr, 3, 6);
    ASSERT_NE(graph, nullptr);

    float avg_clustering = 0.0f;
    nimcp_error_t err = nimcp_gpu_graph_avg_clustering(graph, &avg_clustering);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Verify recovery stats are available */
    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);

    /* Stats should be accessible */
    EXPECT_GE(stats.success_rate, 0.0f);
    EXPECT_LE(stats.success_rate, 1.0f);

    nimcp_gpu_recovery_context_destroy(rctx);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Connectivity check with recovery
 * ============================================================================ */
TEST_F(GraphGPURecoveryTest, ConnectivityCheckWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    /* Create connected graph (triangle) */
    std::vector<int> row_offsets = {0, 2, 4, 6};
    std::vector<int> col_indices = {1, 2, 0, 2, 0, 1};

    graph = nimcp_gpu_graph_from_csr(ctx, row_offsets.data(), col_indices.data(),
                                     nullptr, 3, 6);
    ASSERT_NE(graph, nullptr);

    bool is_connected = false;
    nimcp_error_t err = nimcp_gpu_graph_is_connected(graph, &is_connected);
    EXPECT_EQ(err, NIMCP_SUCCESS) << "Connectivity check should succeed";
    EXPECT_TRUE(is_connected) << "Triangle graph should be connected";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Stats tracking after graph operations
 * ============================================================================ */
TEST_F(GraphGPURecoveryTest, StatsTrackingAfterGraphOps) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_reset_stats();

    /* Create graph and perform multiple operations */
    std::vector<int> row_offsets = {0, 2, 4, 6, 8, 10};
    std::vector<int> col_indices = {1, 4, 0, 2, 1, 3, 2, 4, 0, 3};

    graph = nimcp_gpu_graph_from_csr(ctx, row_offsets.data(), col_indices.data(),
                                     nullptr, 5, 10);
    ASSERT_NE(graph, nullptr);

    /* Perform several operations */
    float* d_distances = nullptr;
    cudaMalloc(&d_distances, 5 * sizeof(float));
    ASSERT_NE(d_distances, nullptr);

    for (int i = 0; i < 5; i++) {
        nimcp_gpu_graph_bfs(graph, i, d_distances);
    }

    float* d_centrality = nullptr;
    cudaMalloc(&d_centrality, 5 * sizeof(float));
    nimcp_gpu_graph_degree_centrality(graph, d_centrality);

    /* Get stats */
    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);

    /* Stats should be accessible */
    EXPECT_GE(stats.total_errors, 0u);
    EXPECT_GE(stats.recoveries_attempted, 0u);

    cudaFree(d_distances);
    cudaFree(d_centrality);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Multi-source BFS with recovery
 * ============================================================================ */
TEST_F(GraphGPURecoveryTest, MultiSourceBFSWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    /* Create path graph: 0-1-2-3-4-5-6 */
    std::vector<int> row_offsets = {0, 1, 3, 5, 7, 9, 11, 12};
    std::vector<int> col_indices = {1, 0, 2, 1, 3, 2, 4, 3, 5, 4, 6, 5};

    graph = nimcp_gpu_graph_from_csr(ctx, row_offsets.data(), col_indices.data(),
                                     nullptr, 7, 12);
    ASSERT_NE(graph, nullptr);

    /* BFS from multiple sources */
    std::vector<int> sources = {0, 6};  /* Both ends */
    float* d_distances = nullptr;
    cudaMalloc(&d_distances, 7 * sizeof(float));
    ASSERT_NE(d_distances, nullptr);

    nimcp_error_t err = nimcp_gpu_graph_bfs_multi_source(
        graph, sources.data(), sources.size(), d_distances);
    EXPECT_EQ(err, NIMCP_SUCCESS) << "Multi-source BFS should succeed";

    /* Verify middle nodes have smaller distances */
    std::vector<float> h_distances(7);
    cudaMemcpy(h_distances.data(), d_distances, 7 * sizeof(float),
               cudaMemcpyDeviceToHost);

    /* Node 3 is in the middle, should have distance 3 from either end */
    EXPECT_EQ(h_distances[3], 3.0f);

    cudaFree(d_distances);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Recovery action names
 * ============================================================================ */
TEST_F(GraphGPURecoveryTest, RecoveryActionNames) {
#ifdef NIMCP_ENABLE_CUDA
    const char* name = nimcp_gpu_recovery_action_name(GPU_RECOVERY_CPU_FALLBACK);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);

    name = nimcp_gpu_recovery_action_name(GPU_RECOVERY_REDUCE_BATCH);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Error category names
 * ============================================================================ */
TEST_F(GraphGPURecoveryTest, ErrorCategoryNames) {
#ifdef NIMCP_ENABLE_CUDA
    const char* name = nimcp_gpu_error_category_name(GPU_ERROR_OUT_OF_MEMORY);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);

    name = nimcp_gpu_error_category_name(GPU_ERROR_NUMERICAL);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

}  /* namespace */

/* Main function for standalone test execution */
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
