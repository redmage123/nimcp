/**
 * @file regression_gpu_test_topology.cpp
 * @brief Regression tests for GPU topology operations
 *
 * WHAT: Ensure GPU topology operations remain stable and accurate
 * WHY:  Prevent reintroduction of bugs, verify numerical correctness
 * HOW:  Compare GPU results against CPU reference, test edge cases
 *
 * TEST CATEGORIES:
 * - Floyd-Warshall accuracy vs CPU reference
 * - Louvain modularity correctness
 * - PageRank convergence behavior
 * - Numerical stability under various conditions
 * - Backward compatibility verification
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2026
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <algorithm>
#include <numeric>
#include <random>
#include <limits>

#include "gpu/topology/nimcp_topology_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"

// Headers already have their own extern "C" guards
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Constants
//=============================================================================

namespace {
    constexpr int SMALL_GRAPH = 10;
    constexpr int MEDIUM_GRAPH = 50;
    constexpr int STABILITY_ITERATIONS = 100;
    constexpr float FLOAT_TOLERANCE = 1e-4f;
    constexpr float LOOSE_TOLERANCE = 1e-2f;
    constexpr uint32_t RANDOM_SEED = 12345;

//=============================================================================
// Test Fixture
//=============================================================================

class TopologyGPURegressionTest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* gpu_ctx = nullptr;
    std::mt19937 rng;

    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
        nimcp_memory_clear_stats();

        gpu_ctx = nimcp_gpu_context_create_auto();
        rng.seed(RANDOM_SEED);
    }

    void TearDown() override {
        if (gpu_ctx) {
            nimcp_gpu_context_destroy(gpu_ctx);
            gpu_ctx = nullptr;
        }

        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        EXPECT_LT(stats.current_allocated, 4096)
            << "Memory leak detected: " << stats.current_allocated << " bytes";
    }

    bool HasGPU() const { return gpu_ctx != nullptr; }

    // CPU reference: Floyd-Warshall algorithm
    void FloydWarshallCPU(const std::vector<float>& adj, int n,
                          std::vector<float>& dist) {
        const float INF = std::numeric_limits<float>::max() / 2;
        dist.resize(n * n);

        // Initialize distances
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                if (i == j) {
                    dist[i * n + j] = 0.0f;
                } else if (adj[i * n + j] > 0) {
                    dist[i * n + j] = adj[i * n + j];
                } else {
                    dist[i * n + j] = INF;
                }
            }
        }

        // Floyd-Warshall iterations
        for (int k = 0; k < n; k++) {
            for (int i = 0; i < n; i++) {
                for (int j = 0; j < n; j++) {
                    if (dist[i * n + k] + dist[k * n + j] < dist[i * n + j]) {
                        dist[i * n + j] = dist[i * n + k] + dist[k * n + j];
                    }
                }
            }
        }
    }

    // CPU reference: PageRank
    void PageRankCPU(const std::vector<float>& adj, int n, float damping,
                     int max_iter, float tol, std::vector<float>& pr) {
        pr.resize(n, 1.0f / n);
        std::vector<float> new_pr(n);
        std::vector<float> out_degree(n, 0.0f);

        // Compute out-degrees
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                out_degree[i] += adj[i * n + j];
            }
        }

        // Power iteration
        for (int iter = 0; iter < max_iter; iter++) {
            float diff = 0.0f;

            for (int i = 0; i < n; i++) {
                float sum = 0.0f;
                for (int j = 0; j < n; j++) {
                    if (adj[j * n + i] > 0 && out_degree[j] > 0) {
                        sum += pr[j] / out_degree[j];
                    }
                }
                new_pr[i] = (1.0f - damping) / n + damping * sum;
                diff += std::abs(new_pr[i] - pr[i]);
            }

            pr = new_pr;

            if (diff < tol) break;
        }

        // Normalize
        float sum = std::accumulate(pr.begin(), pr.end(), 0.0f);
        for (float& p : pr) p /= sum;
    }

    // CPU reference: Modularity computation
    float ModularityCPU(const std::vector<float>& adj, int n,
                        const std::vector<int>& communities, int num_comm) {
        float m = 0.0f;  // Total edge weight
        std::vector<float> degree(n, 0.0f);

        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                m += adj[i * n + j];
                degree[i] += adj[i * n + j];
            }
        }
        m /= 2.0f;

        if (m == 0.0f) return 0.0f;

        float Q = 0.0f;
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                if (communities[i] == communities[j]) {
                    Q += adj[i * n + j] - (degree[i] * degree[j]) / (2.0f * m);
                }
            }
        }

        return Q / (2.0f * m);
    }

    // CPU reference: BFS distances
    void BFSCPU(const std::vector<float>& adj, int n, int source,
                std::vector<float>& dist) {
        const float INF = std::numeric_limits<float>::max();
        dist.resize(n, INF);
        dist[source] = 0.0f;

        std::vector<bool> visited(n, false);
        std::vector<int> queue;
        queue.push_back(source);
        visited[source] = true;

        int front = 0;
        while (front < (int)queue.size()) {
            int u = queue[front++];
            for (int v = 0; v < n; v++) {
                if (adj[u * n + v] > 0 && !visited[v]) {
                    visited[v] = true;
                    dist[v] = dist[u] + 1.0f;
                    queue.push_back(v);
                }
            }
        }
    }

    // Helper: Create random adjacency matrix
    std::vector<float> CreateRandomGraph(int n, float prob) {
        std::vector<float> adj(n * n, 0.0f);
        std::bernoulli_distribution dist(prob);

        for (int i = 0; i < n; i++) {
            for (int j = i + 1; j < n; j++) {
                if (dist(rng)) {
                    adj[i * n + j] = 1.0f;
                    adj[j * n + i] = 1.0f;
                }
            }
        }
        return adj;
    }

    // Helper: Create weighted random graph
    std::vector<float> CreateWeightedGraph(int n, float prob) {
        std::vector<float> adj(n * n, 0.0f);
        std::bernoulli_distribution edge_dist(prob);
        std::uniform_real_distribution<float> weight_dist(0.1f, 2.0f);

        for (int i = 0; i < n; i++) {
            for (int j = i + 1; j < n; j++) {
                if (edge_dist(rng)) {
                    float w = weight_dist(rng);
                    adj[i * n + j] = w;
                    adj[j * n + i] = w;
                }
            }
        }
        return adj;
    }

    // Helper: Create graph with planted communities
    std::vector<float> CreateCommunityGraph(int n, int num_comm,
                                             float p_in, float p_out) {
        std::vector<float> adj(n * n, 0.0f);
        int comm_size = n / num_comm;
        std::bernoulli_distribution dist_in(p_in);
        std::bernoulli_distribution dist_out(p_out);

        for (int i = 0; i < n; i++) {
            for (int j = i + 1; j < n; j++) {
                int ci = i / comm_size;
                int cj = j / comm_size;
                bool edge = (ci == cj) ? dist_in(rng) : dist_out(rng);
                if (edge) {
                    adj[i * n + j] = 1.0f;
                    adj[j * n + i] = 1.0f;
                }
            }
        }
        return adj;
    }

    // Helper: Copy tensor to host
    std::vector<float> TensorToHost(nimcp_gpu_tensor_t* tensor, size_t n) {
        std::vector<float> host_data(n);
        nimcp_gpu_memcpy(gpu_ctx, host_data.data(), tensor->data,
                         n * sizeof(float), GPU_MEMCPY_DEVICE_TO_HOST);
        return host_data;
    }

    std::vector<int32_t> TensorToHostInt(nimcp_gpu_tensor_t* tensor, size_t n) {
        std::vector<int32_t> host_data(n);
        nimcp_gpu_memcpy(gpu_ctx, host_data.data(), tensor->data,
                         n * sizeof(int32_t), GPU_MEMCPY_DEVICE_TO_HOST);
        return host_data;
    }

    // Helper: Create GPU tensor
    nimcp_gpu_tensor_t* Create1DTensor(size_t n, nimcp_gpu_precision_t dtype) {
        size_t dims[] = {n};
        return nimcp_gpu_tensor_create(gpu_ctx, dims, 1, dtype);
    }

    nimcp_gpu_tensor_t* Create2DTensor(size_t rows, size_t cols, nimcp_gpu_precision_t dtype) {
        size_t dims[] = {rows, cols};
        return nimcp_gpu_tensor_create(gpu_ctx, dims, 2, dtype);
    }
};

//=============================================================================
// Floyd-Warshall Accuracy Tests
//=============================================================================

TEST_F(TopologyGPURegressionTest, FloydWarshall_MatchesCPU_SmallGraph) {
    if (!HasGPU()) GTEST_SKIP() << "GPU not available";

    auto adj = CreateRandomGraph(SMALL_GRAPH, 0.5f);
    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(gpu_ctx, adj.data(), SMALL_GRAPH);
    ASSERT_NE(graph, nullptr);

    // GPU computation
    nimcp_apsp_result_gpu_t gpu_result;
    gpu_result.distances = Create2DTensor(SMALL_GRAPH, SMALL_GRAPH, NIMCP_GPU_PRECISION_FP32);
    bool gpu_ok = nimcp_shortest_path_floyd_warshall(gpu_ctx, graph, &gpu_result);
    ASSERT_TRUE(gpu_ok);

    auto gpu_dist = TensorToHost(gpu_result.distances, SMALL_GRAPH * SMALL_GRAPH);

    // CPU reference
    std::vector<float> cpu_dist;
    FloydWarshallCPU(adj, SMALL_GRAPH, cpu_dist);

    // Compare results
    int matches = 0;
    int total = 0;
    float max_diff = 0.0f;
    const float INF = std::numeric_limits<float>::max() / 2;

    for (int i = 0; i < SMALL_GRAPH * SMALL_GRAPH; i++) {
        // Skip unreachable pairs
        if (cpu_dist[i] >= INF || gpu_dist[i] >= INF) continue;

        total++;
        float diff = std::abs(gpu_dist[i] - cpu_dist[i]);
        max_diff = std::max(max_diff, diff);

        if (diff < FLOAT_TOLERANCE) matches++;
    }

    float accuracy = (float)matches / total;
    std::cout << "Floyd-Warshall accuracy: " << (accuracy * 100) << "% "
              << "(max diff: " << max_diff << ")" << std::endl;

    EXPECT_GT(accuracy, 0.99f) << "GPU Floyd-Warshall differs from CPU reference";
    EXPECT_LT(max_diff, LOOSE_TOLERANCE);

    nimcp_gpu_tensor_destroy(gpu_result.distances);
    nimcp_graph_gpu_destroy(graph);
}

TEST_F(TopologyGPURegressionTest, FloydWarshall_MatchesCPU_WeightedGraph) {
    if (!HasGPU()) GTEST_SKIP() << "GPU not available";

    auto adj = CreateWeightedGraph(SMALL_GRAPH, 0.4f);
    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(gpu_ctx, adj.data(), SMALL_GRAPH);
    ASSERT_NE(graph, nullptr);

    // GPU computation
    nimcp_apsp_result_gpu_t gpu_result;
    gpu_result.distances = Create2DTensor(SMALL_GRAPH, SMALL_GRAPH, NIMCP_GPU_PRECISION_FP32);
    bool gpu_ok = nimcp_shortest_path_floyd_warshall(gpu_ctx, graph, &gpu_result);
    ASSERT_TRUE(gpu_ok);

    auto gpu_dist = TensorToHost(gpu_result.distances, SMALL_GRAPH * SMALL_GRAPH);

    // CPU reference
    std::vector<float> cpu_dist;
    FloydWarshallCPU(adj, SMALL_GRAPH, cpu_dist);

    // Compare
    const float INF = std::numeric_limits<float>::max() / 2;
    int errors = 0;

    for (int i = 0; i < SMALL_GRAPH; i++) {
        for (int j = 0; j < SMALL_GRAPH; j++) {
            float cpu_d = cpu_dist[i * SMALL_GRAPH + j];
            float gpu_d = gpu_dist[i * SMALL_GRAPH + j];

            if (cpu_d >= INF && gpu_d >= INF) continue;
            if (cpu_d < INF && gpu_d >= INF) errors++;
            else if (cpu_d >= INF && gpu_d < INF) errors++;
            else if (std::abs(cpu_d - gpu_d) > LOOSE_TOLERANCE) errors++;
        }
    }

    std::cout << "Floyd-Warshall weighted graph errors: " << errors << "/"
              << (SMALL_GRAPH * SMALL_GRAPH) << std::endl;
    EXPECT_EQ(errors, 0) << "GPU Floyd-Warshall has errors on weighted graph";

    nimcp_gpu_tensor_destroy(gpu_result.distances);
    nimcp_graph_gpu_destroy(graph);
}

TEST_F(TopologyGPURegressionTest, FloydWarshall_DiameterCorrect) {
    if (!HasGPU()) GTEST_SKIP() << "GPU not available";

    // Create path graph: 0-1-2-3-4 (diameter should be 4)
    int n = 5;
    std::vector<float> adj(n * n, 0.0f);
    for (int i = 0; i < n - 1; i++) {
        adj[i * n + (i + 1)] = 1.0f;
        adj[(i + 1) * n + i] = 1.0f;
    }

    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(gpu_ctx, adj.data(), n);
    ASSERT_NE(graph, nullptr);

    nimcp_apsp_result_gpu_t gpu_result;
    gpu_result.distances = Create2DTensor(n, n, NIMCP_GPU_PRECISION_FP32);
    bool ok = nimcp_shortest_path_floyd_warshall(gpu_ctx, graph, &gpu_result);
    ASSERT_TRUE(ok);

    EXPECT_NEAR(gpu_result.diameter, 4.0f, FLOAT_TOLERANCE)
        << "Diameter of path graph should be 4";
    EXPECT_NEAR(gpu_result.avg_path_length, 2.0f, LOOSE_TOLERANCE)
        << "Average path length should be 2 for path graph";

    nimcp_gpu_tensor_destroy(gpu_result.distances);
    nimcp_graph_gpu_destroy(graph);
}

//=============================================================================
// Louvain Modularity Correctness Tests
//=============================================================================

TEST_F(TopologyGPURegressionTest, Louvain_ModularityMatchesCPU) {
    if (!HasGPU()) GTEST_SKIP() << "GPU not available";

    auto adj = CreateCommunityGraph(MEDIUM_GRAPH, 4, 0.7f, 0.05f);
    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(gpu_ctx, adj.data(), MEDIUM_GRAPH);
    ASSERT_NE(graph, nullptr);

    // Run Louvain
    nimcp_community_result_gpu_t* result = nimcp_community_detect_louvain(
        gpu_ctx, graph, 1.0f, 100, 1e-6f
    );
    ASSERT_NE(result, nullptr);

    // Get GPU communities
    auto communities = TensorToHostInt(result->node_communities, MEDIUM_GRAPH);
    std::vector<int> comm_vec(communities.begin(), communities.end());

    // Compute modularity using CPU reference
    float cpu_modularity = ModularityCPU(adj, MEDIUM_GRAPH, comm_vec, result->num_communities);

    std::cout << "Louvain GPU modularity: " << result->modularity << std::endl;
    std::cout << "CPU computed modularity: " << cpu_modularity << std::endl;

    // Should match closely
    EXPECT_NEAR(result->modularity, cpu_modularity, LOOSE_TOLERANCE)
        << "GPU modularity differs from CPU computation";

    // Modularity should be positive for community-structured graph
    EXPECT_GT(result->modularity, 0.3f)
        << "Modularity too low for planted partition graph";

    nimcp_community_result_gpu_destroy(result);
    nimcp_graph_gpu_destroy(graph);
}

TEST_F(TopologyGPURegressionTest, Louvain_FindsPlantedCommunities) {
    if (!HasGPU()) GTEST_SKIP() << "GPU not available";

    const int n = 40;
    const int num_comm = 4;

    auto adj = CreateCommunityGraph(n, num_comm, 0.8f, 0.02f);
    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(gpu_ctx, adj.data(), n);
    ASSERT_NE(graph, nullptr);

    nimcp_community_result_gpu_t* result = nimcp_community_detect_louvain(
        gpu_ctx, graph, 1.0f, 100, 1e-6f
    );
    ASSERT_NE(result, nullptr);

    // Should find approximately the planted number
    std::cout << "Planted communities: " << num_comm << std::endl;
    std::cout << "Found communities: " << result->num_communities << std::endl;

    EXPECT_GE(result->num_communities, num_comm - 1);
    EXPECT_LE(result->num_communities, num_comm + 2);

    // Check community sizes are reasonable
    auto sizes = TensorToHostInt(result->community_sizes, result->num_communities);
    int total_assigned = std::accumulate(sizes.begin(), sizes.end(), 0);
    EXPECT_EQ(total_assigned, n) << "All nodes should be assigned to communities";

    nimcp_community_result_gpu_destroy(result);
    nimcp_graph_gpu_destroy(graph);
}

TEST_F(TopologyGPURegressionTest, Louvain_Reproducible) {
    if (!HasGPU()) GTEST_SKIP() << "GPU not available";

    auto adj = CreateCommunityGraph(MEDIUM_GRAPH, 4, 0.7f, 0.05f);
    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(gpu_ctx, adj.data(), MEDIUM_GRAPH);
    ASSERT_NE(graph, nullptr);

    // Run twice with same parameters
    nimcp_community_result_gpu_t* result1 = nimcp_community_detect_louvain(
        gpu_ctx, graph, 1.0f, 100, 1e-6f
    );
    nimcp_community_result_gpu_t* result2 = nimcp_community_detect_louvain(
        gpu_ctx, graph, 1.0f, 100, 1e-6f
    );

    ASSERT_NE(result1, nullptr);
    ASSERT_NE(result2, nullptr);

    // Results should be similar (deterministic algorithm)
    EXPECT_EQ(result1->num_communities, result2->num_communities);
    EXPECT_NEAR(result1->modularity, result2->modularity, FLOAT_TOLERANCE);

    nimcp_community_result_gpu_destroy(result1);
    nimcp_community_result_gpu_destroy(result2);
    nimcp_graph_gpu_destroy(graph);
}

//=============================================================================
// PageRank Convergence Tests
//=============================================================================

TEST_F(TopologyGPURegressionTest, PageRank_MatchesCPU) {
    if (!HasGPU()) GTEST_SKIP() << "GPU not available";

    auto adj = CreateRandomGraph(SMALL_GRAPH, 0.4f);
    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(gpu_ctx, adj.data(), SMALL_GRAPH);
    ASSERT_NE(graph, nullptr);

    // GPU computation
    nimcp_gpu_tensor_t* gpu_pr = Create1DTensor(SMALL_GRAPH, NIMCP_GPU_PRECISION_FP32);
    bool ok = nimcp_topology_compute_pagerank(gpu_ctx, graph, 0.85f, 100, 1e-6f, gpu_pr);
    ASSERT_TRUE(ok);
    auto gpu_pr_host = TensorToHost(gpu_pr, SMALL_GRAPH);

    // CPU reference
    std::vector<float> cpu_pr;
    PageRankCPU(adj, SMALL_GRAPH, 0.85f, 100, 1e-6f, cpu_pr);

    // Compare
    float max_diff = 0.0f;
    for (int i = 0; i < SMALL_GRAPH; i++) {
        float diff = std::abs(gpu_pr_host[i] - cpu_pr[i]);
        max_diff = std::max(max_diff, diff);
    }

    std::cout << "PageRank max difference from CPU: " << max_diff << std::endl;
    EXPECT_LT(max_diff, LOOSE_TOLERANCE)
        << "GPU PageRank differs significantly from CPU reference";

    // Both should sum to 1
    float gpu_sum = std::accumulate(gpu_pr_host.begin(), gpu_pr_host.end(), 0.0f);
    float cpu_sum = std::accumulate(cpu_pr.begin(), cpu_pr.end(), 0.0f);

    EXPECT_NEAR(gpu_sum, 1.0f, FLOAT_TOLERANCE);
    EXPECT_NEAR(cpu_sum, 1.0f, FLOAT_TOLERANCE);

    nimcp_gpu_tensor_destroy(gpu_pr);
    nimcp_graph_gpu_destroy(graph);
}

TEST_F(TopologyGPURegressionTest, PageRank_Converges) {
    if (!HasGPU()) GTEST_SKIP() << "GPU not available";

    auto adj = CreateRandomGraph(MEDIUM_GRAPH, 0.1f);
    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(gpu_ctx, adj.data(), MEDIUM_GRAPH);
    ASSERT_NE(graph, nullptr);

    nimcp_gpu_tensor_t* pr = Create1DTensor(MEDIUM_GRAPH, NIMCP_GPU_PRECISION_FP32);

    // Run with very low tolerance to test convergence
    bool ok = nimcp_topology_compute_pagerank(gpu_ctx, graph, 0.85f, 1000, 1e-8f, pr);
    ASSERT_TRUE(ok);

    auto pr_host = TensorToHost(pr, MEDIUM_GRAPH);

    // Verify stochastic property: sum to 1
    float sum = std::accumulate(pr_host.begin(), pr_host.end(), 0.0f);
    EXPECT_NEAR(sum, 1.0f, FLOAT_TOLERANCE) << "PageRank should sum to 1";

    // All values should be positive
    for (int i = 0; i < MEDIUM_GRAPH; i++) {
        EXPECT_GT(pr_host[i], 0.0f) << "PageRank values should be positive";
    }

    nimcp_gpu_tensor_destroy(pr);
    nimcp_graph_gpu_destroy(graph);
}

TEST_F(TopologyGPURegressionTest, PageRank_DampingFactorEffect) {
    if (!HasGPU()) GTEST_SKIP() << "GPU not available";

    // Star graph where hub importance depends on damping
    std::vector<float> adj(SMALL_GRAPH * SMALL_GRAPH, 0.0f);
    for (int i = 1; i < SMALL_GRAPH; i++) {
        adj[0 * SMALL_GRAPH + i] = 1.0f;
        adj[i * SMALL_GRAPH + 0] = 1.0f;
    }

    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(gpu_ctx, adj.data(), SMALL_GRAPH);
    ASSERT_NE(graph, nullptr);

    nimcp_gpu_tensor_t* pr_low = Create1DTensor(SMALL_GRAPH, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* pr_high = Create1DTensor(SMALL_GRAPH, NIMCP_GPU_PRECISION_FP32);

    // Low damping (more uniform)
    nimcp_topology_compute_pagerank(gpu_ctx, graph, 0.5f, 100, 1e-6f, pr_low);
    // High damping (more link-based)
    nimcp_topology_compute_pagerank(gpu_ctx, graph, 0.95f, 100, 1e-6f, pr_high);

    auto pr_low_host = TensorToHost(pr_low, SMALL_GRAPH);
    auto pr_high_host = TensorToHost(pr_high, SMALL_GRAPH);

    // Hub (node 0) should have higher relative importance with high damping
    float hub_ratio_low = pr_low_host[0] / (1.0f / SMALL_GRAPH);
    float hub_ratio_high = pr_high_host[0] / (1.0f / SMALL_GRAPH);

    std::cout << "Hub importance ratio (d=0.5): " << hub_ratio_low << std::endl;
    std::cout << "Hub importance ratio (d=0.95): " << hub_ratio_high << std::endl;

    EXPECT_GT(hub_ratio_high, hub_ratio_low)
        << "Higher damping should increase hub importance";

    nimcp_gpu_tensor_destroy(pr_low);
    nimcp_gpu_tensor_destroy(pr_high);
    nimcp_graph_gpu_destroy(graph);
}

//=============================================================================
// BFS Accuracy Tests
//=============================================================================

TEST_F(TopologyGPURegressionTest, BFS_MatchesCPU) {
    if (!HasGPU()) GTEST_SKIP() << "GPU not available";

    auto adj = CreateRandomGraph(SMALL_GRAPH, 0.4f);
    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(gpu_ctx, adj.data(), SMALL_GRAPH);
    ASSERT_NE(graph, nullptr);

    // GPU BFS
    nimcp_shortest_path_result_gpu_t gpu_result;
    gpu_result.distances = Create1DTensor(SMALL_GRAPH, NIMCP_GPU_PRECISION_FP32);
    gpu_result.predecessors = Create1DTensor(SMALL_GRAPH, NIMCP_GPU_PRECISION_INT32);

    bool ok = nimcp_shortest_path_bfs(gpu_ctx, graph, 0, &gpu_result);
    ASSERT_TRUE(ok);

    auto gpu_dist = TensorToHost(gpu_result.distances, SMALL_GRAPH);

    // CPU reference
    std::vector<float> cpu_dist;
    BFSCPU(adj, SMALL_GRAPH, 0, cpu_dist);

    // Compare
    const float INF = std::numeric_limits<float>::max();
    int errors = 0;

    for (int i = 0; i < SMALL_GRAPH; i++) {
        bool cpu_inf = cpu_dist[i] >= INF;
        bool gpu_inf = gpu_dist[i] >= INF || gpu_dist[i] < 0;

        if (cpu_inf && gpu_inf) continue;
        if (cpu_inf != gpu_inf) errors++;
        else if (std::abs(cpu_dist[i] - gpu_dist[i]) > FLOAT_TOLERANCE) errors++;
    }

    std::cout << "BFS errors: " << errors << "/" << SMALL_GRAPH << std::endl;
    EXPECT_EQ(errors, 0) << "GPU BFS differs from CPU reference";

    nimcp_gpu_tensor_destroy(gpu_result.distances);
    nimcp_gpu_tensor_destroy(gpu_result.predecessors);
    nimcp_graph_gpu_destroy(graph);
}

//=============================================================================
// Numerical Stability Tests
//=============================================================================

TEST_F(TopologyGPURegressionTest, Stability_RepeatedComputation) {
    if (!HasGPU()) GTEST_SKIP() << "GPU not available";

    auto adj = CreateRandomGraph(SMALL_GRAPH, 0.5f);
    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(gpu_ctx, adj.data(), SMALL_GRAPH);
    ASSERT_NE(graph, nullptr);

    std::vector<float> first_pr;
    nimcp_gpu_tensor_t* pr = Create1DTensor(SMALL_GRAPH, NIMCP_GPU_PRECISION_FP32);

    // Run PageRank multiple times
    for (int i = 0; i < STABILITY_ITERATIONS; i++) {
        bool ok = nimcp_topology_compute_pagerank(gpu_ctx, graph, 0.85f, 100, 1e-6f, pr);
        ASSERT_TRUE(ok) << "PageRank failed at iteration " << i;

        auto pr_host = TensorToHost(pr, SMALL_GRAPH);

        if (i == 0) {
            first_pr = pr_host;
        } else {
            // All results should be identical
            for (int j = 0; j < SMALL_GRAPH; j++) {
                EXPECT_NEAR(pr_host[j], first_pr[j], FLOAT_TOLERANCE)
                    << "PageRank not stable at iteration " << i;
            }
        }
    }

    nimcp_gpu_tensor_destroy(pr);
    nimcp_graph_gpu_destroy(graph);
}

TEST_F(TopologyGPURegressionTest, Stability_SmallWeights) {
    if (!HasGPU()) GTEST_SKIP() << "GPU not available";

    // Graph with very small weights
    std::vector<float> adj(SMALL_GRAPH * SMALL_GRAPH, 0.0f);
    std::uniform_real_distribution<float> dist(1e-6f, 1e-4f);

    for (int i = 0; i < SMALL_GRAPH; i++) {
        for (int j = i + 1; j < SMALL_GRAPH; j++) {
            if (rng() % 2 == 0) {
                float w = dist(rng);
                adj[i * SMALL_GRAPH + j] = w;
                adj[j * SMALL_GRAPH + i] = w;
            }
        }
    }

    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(gpu_ctx, adj.data(), SMALL_GRAPH);
    ASSERT_NE(graph, nullptr);

    // Compute metrics - should not produce NaN or Inf
    nimcp_topology_metrics_gpu_t* metrics = nimcp_topology_compute_metrics(gpu_ctx, graph);
    ASSERT_NE(metrics, nullptr);

    EXPECT_FALSE(std::isnan(metrics->global_clustering));
    EXPECT_FALSE(std::isinf(metrics->global_clustering));
    EXPECT_FALSE(std::isnan(metrics->avg_path_length));
    EXPECT_FALSE(std::isnan(metrics->density));

    nimcp_topology_metrics_gpu_destroy(metrics);
    nimcp_graph_gpu_destroy(graph);
}

TEST_F(TopologyGPURegressionTest, Stability_LargeWeights) {
    if (!HasGPU()) GTEST_SKIP() << "GPU not available";

    // Graph with large weights
    std::vector<float> adj(SMALL_GRAPH * SMALL_GRAPH, 0.0f);
    std::uniform_real_distribution<float> dist(1e4f, 1e6f);

    for (int i = 0; i < SMALL_GRAPH; i++) {
        for (int j = i + 1; j < SMALL_GRAPH; j++) {
            if (rng() % 2 == 0) {
                float w = dist(rng);
                adj[i * SMALL_GRAPH + j] = w;
                adj[j * SMALL_GRAPH + i] = w;
            }
        }
    }

    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(gpu_ctx, adj.data(), SMALL_GRAPH);
    ASSERT_NE(graph, nullptr);

    // Should handle large weights without overflow
    nimcp_gpu_tensor_t* wd = Create1DTensor(SMALL_GRAPH, NIMCP_GPU_PRECISION_FP32);
    bool ok = nimcp_topology_compute_weighted_degree(gpu_ctx, graph, wd);
    ASSERT_TRUE(ok);

    auto wd_host = TensorToHost(wd, SMALL_GRAPH);

    for (int i = 0; i < SMALL_GRAPH; i++) {
        EXPECT_FALSE(std::isnan(wd_host[i])) << "NaN in weighted degree";
        EXPECT_FALSE(std::isinf(wd_host[i])) << "Inf in weighted degree";
    }

    nimcp_gpu_tensor_destroy(wd);
    nimcp_graph_gpu_destroy(graph);
}

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(TopologyGPURegressionTest, EdgeCase_EmptyGraph) {
    if (!HasGPU()) GTEST_SKIP() << "GPU not available";

    std::vector<float> adj(SMALL_GRAPH * SMALL_GRAPH, 0.0f);
    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(gpu_ctx, adj.data(), SMALL_GRAPH);
    ASSERT_NE(graph, nullptr);

    nimcp_topology_metrics_gpu_t* metrics = nimcp_topology_compute_metrics(gpu_ctx, graph);
    ASSERT_NE(metrics, nullptr);

    EXPECT_NEAR(metrics->density, 0.0f, FLOAT_TOLERANCE);
    EXPECT_NEAR(metrics->global_clustering, 0.0f, FLOAT_TOLERANCE);

    nimcp_topology_metrics_gpu_destroy(metrics);
    nimcp_graph_gpu_destroy(graph);
}

TEST_F(TopologyGPURegressionTest, EdgeCase_CompleteGraph) {
    if (!HasGPU()) GTEST_SKIP() << "GPU not available";

    std::vector<float> adj(SMALL_GRAPH * SMALL_GRAPH, 1.0f);
    for (int i = 0; i < SMALL_GRAPH; i++) {
        adj[i * SMALL_GRAPH + i] = 0.0f;  // No self-loops
    }

    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(gpu_ctx, adj.data(), SMALL_GRAPH);
    ASSERT_NE(graph, nullptr);

    nimcp_topology_metrics_gpu_t* metrics = nimcp_topology_compute_metrics(gpu_ctx, graph);
    ASSERT_NE(metrics, nullptr);

    EXPECT_NEAR(metrics->density, 1.0f, FLOAT_TOLERANCE);
    EXPECT_NEAR(metrics->global_clustering, 1.0f, FLOAT_TOLERANCE);
    EXPECT_NEAR(metrics->diameter, 1.0f, FLOAT_TOLERANCE);

    nimcp_topology_metrics_gpu_destroy(metrics);
    nimcp_graph_gpu_destroy(graph);
}

TEST_F(TopologyGPURegressionTest, EdgeCase_SingleNode) {
    if (!HasGPU()) GTEST_SKIP() << "GPU not available";

    std::vector<float> adj = {0.0f};
    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(gpu_ctx, adj.data(), 1);
    ASSERT_NE(graph, nullptr);

    nimcp_gpu_tensor_t* degree = Create1DTensor(1, NIMCP_GPU_PRECISION_INT32);
    bool ok = nimcp_topology_compute_degree(gpu_ctx, graph, degree);
    EXPECT_TRUE(ok);

    auto deg_host = TensorToHostInt(degree, 1);
    EXPECT_EQ(deg_host[0], 0);

    nimcp_gpu_tensor_destroy(degree);
    nimcp_graph_gpu_destroy(graph);
}

TEST_F(TopologyGPURegressionTest, EdgeCase_DisconnectedGraph) {
    if (!HasGPU()) GTEST_SKIP() << "GPU not available";

    // Two disconnected cliques
    int n = 10;
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

    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(gpu_ctx, adj.data(), n);
    ASSERT_NE(graph, nullptr);

    // BFS from node 0 should not reach nodes 5-9
    nimcp_shortest_path_result_gpu_t result;
    result.distances = Create1DTensor(n, NIMCP_GPU_PRECISION_FP32);
    result.predecessors = Create1DTensor(n, NIMCP_GPU_PRECISION_INT32);

    bool ok = nimcp_shortest_path_bfs(gpu_ctx, graph, 0, &result);
    ASSERT_TRUE(ok);

    auto dist = TensorToHost(result.distances, n);

    // Nodes 0-4 should be reachable
    for (int i = 0; i < 5; i++) {
        EXPECT_LT(dist[i], n) << "Node " << i << " should be reachable from 0";
    }

    // Nodes 5-9 should be unreachable (infinite distance)
    for (int i = 5; i < 10; i++) {
        EXPECT_GT(dist[i], n) << "Node " << i << " should be unreachable from 0";
    }

    // Community detection should find 2 communities
    nimcp_community_result_gpu_t* comm = nimcp_community_detect_louvain(
        gpu_ctx, graph, 1.0f, 100, 1e-6f
    );
    ASSERT_NE(comm, nullptr);
    EXPECT_EQ(comm->num_communities, 2);

    nimcp_community_result_gpu_destroy(comm);
    nimcp_gpu_tensor_destroy(result.distances);
    nimcp_gpu_tensor_destroy(result.predecessors);
    nimcp_graph_gpu_destroy(graph);
}

//=============================================================================
// Memory Safety Tests
//=============================================================================

TEST_F(TopologyGPURegressionTest, MemorySafety_RepeatedAllocation) {
    if (!HasGPU()) GTEST_SKIP() << "GPU not available";

    nimcp_memory_stats_t initial_stats;
    nimcp_memory_get_stats(&initial_stats);

    for (int i = 0; i < 50; i++) {
        auto adj = CreateRandomGraph(SMALL_GRAPH, 0.3f);
        nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(gpu_ctx, adj.data(), SMALL_GRAPH);
        ASSERT_NE(graph, nullptr);

        nimcp_topology_metrics_gpu_t* metrics = nimcp_topology_compute_metrics(gpu_ctx, graph);
        ASSERT_NE(metrics, nullptr);

        nimcp_community_result_gpu_t* comm = nimcp_community_detect_louvain(
            gpu_ctx, graph, 1.0f, 50, 1e-4f
        );
        ASSERT_NE(comm, nullptr);

        nimcp_community_result_gpu_destroy(comm);
        nimcp_topology_metrics_gpu_destroy(metrics);
        nimcp_graph_gpu_destroy(graph);
    }

    nimcp_memory_stats_t final_stats;
    nimcp_memory_get_stats(&final_stats);

    int64_t leaked = final_stats.current_allocated - initial_stats.current_allocated;
    EXPECT_LT(leaked, 1024) << "Memory leak detected: " << leaked << " bytes";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
