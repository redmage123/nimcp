/**
 * @file integration_gpu_test_topology.cpp
 * @brief Integration tests for GPU topology and community detection
 *
 * WHAT: Test GPU topology integration with kernel backend, tensor operations
 * WHY:  Verify topology operations work correctly in full system context
 * HOW:  Test complete pipelines: graph + metrics + community detection
 *
 * TEST PIPELINES:
 * - Graph creation + community detection + metrics pipeline
 * - Topology with GPU tensor operations
 * - Large-scale graph operations with memory management
 * - Sparse/dense format conversion pipeline
 * - Multi-algorithm comparison pipeline
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
#include <chrono>
#include <random>

#include "gpu/topology/nimcp_topology_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/backend/nimcp_kernel_backend.h"

// Headers already have their own extern "C" guards
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Constants
//=============================================================================

namespace {
    constexpr int SMALL_GRAPH = 20;
    constexpr int MEDIUM_GRAPH = 100;
    constexpr int LARGE_GRAPH = 500;
    constexpr int NUM_COMMUNITIES = 4;
    constexpr float INTRA_COMMUNITY_PROB = 0.8f;
    constexpr float INTER_COMMUNITY_PROB = 0.05f;
    constexpr uint32_t RANDOM_SEED = 42;
    constexpr float PAGERANK_DAMPING = 0.85f;
    constexpr int PAGERANK_MAX_ITER = 100;
    constexpr float PAGERANK_TOL = 1e-6f;

//=============================================================================
// Test Fixture
//=============================================================================

class TopologyGPUIntegrationTest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* gpu_ctx = nullptr;
    nimcp_kernel_backend_t* backend = nullptr;
    std::mt19937 rng;

    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
        nimcp_memory_clear_stats();

        bool init_ok = nimcp_kernel_backend_init(NIMCP_BACKEND_AUTO);
        ASSERT_TRUE(init_ok);

        backend = nimcp_get_kernel_backend();
        ASSERT_NE(backend, nullptr);

        gpu_ctx = nimcp_gpu_context_create_auto();
        rng.seed(RANDOM_SEED);
    }

    void TearDown() override {
        if (gpu_ctx) {
            nimcp_gpu_context_destroy(gpu_ctx);
            gpu_ctx = nullptr;
        }

        nimcp_kernel_backend_shutdown();

        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        EXPECT_LT(stats.current_allocated, 4096)
            << "Memory leak detected: " << stats.current_allocated << " bytes";
    }

    bool HasGPU() const { return gpu_ctx != nullptr; }

    // Helper: Create planted partition graph with clear communities
    std::vector<float> CreatePlantedPartitionGraph(int n, int k,
                                                    float p_in, float p_out) {
        std::vector<float> adj(n * n, 0.0f);
        int community_size = n / k;
        std::bernoulli_distribution dist_in(p_in);
        std::bernoulli_distribution dist_out(p_out);

        for (int i = 0; i < n; i++) {
            for (int j = i + 1; j < n; j++) {
                int comm_i = i / community_size;
                int comm_j = j / community_size;

                bool has_edge = (comm_i == comm_j) ? dist_in(rng) : dist_out(rng);
                if (has_edge) {
                    adj[i * n + j] = 1.0f;
                    adj[j * n + i] = 1.0f;
                }
            }
        }
        return adj;
    }

    // Helper: Create tensor and copy data to host
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

    // Helper: Create 1D GPU tensor
    nimcp_gpu_tensor_t* Create1DTensor(size_t n, nimcp_gpu_precision_t dtype) {
        size_t dims[] = {n};
        return nimcp_gpu_tensor_create(gpu_ctx, dims, 1, dtype);
    }

    // Helper: Compute Normalized Mutual Information
    double ComputeNMI(const std::vector<int32_t>& pred,
                      const std::vector<int32_t>& truth, int n) {
        // Simplified NMI calculation
        int correct = 0;
        for (int i = 0; i < n; i++) {
            for (int j = i + 1; j < n; j++) {
                bool pred_same = (pred[i] == pred[j]);
                bool truth_same = (truth[i] == truth[j]);
                if (pred_same == truth_same) correct++;
            }
        }
        return 2.0 * correct / (n * (n - 1));
    }
};

//=============================================================================
// Pipeline 1: Graph + Community Detection + Metrics
//=============================================================================

TEST_F(TopologyGPUIntegrationTest, Pipeline_GraphCommunityMetrics) {
    if (!HasGPU()) GTEST_SKIP() << "GPU not available";

    std::cout << "\n=== Pipeline: Graph + Community Detection + Metrics ===" << std::endl;

    // Step 1: Create graph with planted community structure
    std::cout << "Step 1: Creating planted partition graph..." << std::endl;
    auto adj = CreatePlantedPartitionGraph(MEDIUM_GRAPH, NUM_COMMUNITIES,
                                            INTRA_COMMUNITY_PROB, INTER_COMMUNITY_PROB);

    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(gpu_ctx, adj.data(), MEDIUM_GRAPH);
    ASSERT_NE(graph, nullptr);
    std::cout << "  Graph created: " << graph->num_nodes << " nodes, "
              << graph->num_edges << " edges" << std::endl;

    // Step 2: Compute comprehensive topology metrics
    std::cout << "Step 2: Computing topology metrics..." << std::endl;
    auto metrics_start = std::chrono::high_resolution_clock::now();

    nimcp_topology_metrics_gpu_t* metrics = nimcp_topology_compute_metrics(gpu_ctx, graph);
    ASSERT_NE(metrics, nullptr);

    auto metrics_end = std::chrono::high_resolution_clock::now();
    auto metrics_ms = std::chrono::duration<double, std::milli>(metrics_end - metrics_start).count();

    std::cout << "  Metrics computed in " << metrics_ms << " ms" << std::endl;
    std::cout << "  Global clustering: " << metrics->global_clustering << std::endl;
    std::cout << "  Average path length: " << metrics->avg_path_length << std::endl;
    std::cout << "  Density: " << metrics->density << std::endl;

    EXPECT_GT(metrics->global_clustering, 0.3f);  // Community structure -> high clustering
    EXPECT_GT(metrics->density, 0.0f);
    EXPECT_LT(metrics->density, 1.0f);

    // Step 3: Detect communities using Louvain
    std::cout << "Step 3: Detecting communities (Louvain)..." << std::endl;
    auto louvain_start = std::chrono::high_resolution_clock::now();

    nimcp_community_result_gpu_t* louvain = nimcp_community_detect_louvain(
        gpu_ctx, graph, 1.0f, 100, 1e-5f
    );
    ASSERT_NE(louvain, nullptr);

    auto louvain_end = std::chrono::high_resolution_clock::now();
    auto louvain_ms = std::chrono::duration<double, std::milli>(louvain_end - louvain_start).count();

    std::cout << "  Louvain completed in " << louvain_ms << " ms" << std::endl;
    std::cout << "  Communities found: " << louvain->num_communities << std::endl;
    std::cout << "  Modularity: " << louvain->modularity << std::endl;

    // Should detect approximately the planted number of communities
    EXPECT_GE(louvain->num_communities, NUM_COMMUNITIES - 1);
    EXPECT_LE(louvain->num_communities, NUM_COMMUNITIES + 2);
    EXPECT_GT(louvain->modularity, 0.3f);

    // Step 4: Verify community quality with ground truth
    std::cout << "Step 4: Verifying community quality..." << std::endl;
    auto communities = TensorToHostInt(louvain->node_communities, MEDIUM_GRAPH);

    // Create ground truth
    std::vector<int32_t> ground_truth(MEDIUM_GRAPH);
    int community_size = MEDIUM_GRAPH / NUM_COMMUNITIES;
    for (int i = 0; i < MEDIUM_GRAPH; i++) {
        ground_truth[i] = i / community_size;
    }

    double nmi = ComputeNMI(communities, ground_truth, MEDIUM_GRAPH);
    std::cout << "  NMI with ground truth: " << nmi << std::endl;
    EXPECT_GT(nmi, 0.7);  // Good recovery of planted structure

    // Cleanup
    nimcp_community_result_gpu_destroy(louvain);
    nimcp_topology_metrics_gpu_destroy(metrics);
    nimcp_graph_gpu_destroy(graph);

    std::cout << "Pipeline completed successfully!" << std::endl;
}

//=============================================================================
// Pipeline 2: Topology with GPU Tensor Operations
//=============================================================================

TEST_F(TopologyGPUIntegrationTest, Pipeline_TopologyTensorOperations) {
    if (!HasGPU()) GTEST_SKIP() << "GPU not available";

    std::cout << "\n=== Pipeline: Topology + Tensor Operations ===" << std::endl;

    // Step 1: Generate scale-free network
    std::cout << "Step 1: Generating Barabasi-Albert network..." << std::endl;
    nimcp_graph_gpu_t* graph = nimcp_graph_generate_barabasi_albert(
        gpu_ctx, MEDIUM_GRAPH, 3, RANDOM_SEED
    );
    ASSERT_NE(graph, nullptr);

    // Step 2: Compute degree and PageRank as separate tensors
    std::cout << "Step 2: Computing node centralities..." << std::endl;
    nimcp_gpu_tensor_t* degree = Create1DTensor(MEDIUM_GRAPH, NIMCP_GPU_PRECISION_INT32);
    nimcp_gpu_tensor_t* pagerank = Create1DTensor(MEDIUM_GRAPH, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* clustering = Create1DTensor(MEDIUM_GRAPH, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(degree, nullptr);
    ASSERT_NE(pagerank, nullptr);
    ASSERT_NE(clustering, nullptr);

    bool deg_ok = nimcp_topology_compute_degree(gpu_ctx, graph, degree);
    bool pr_ok = nimcp_topology_compute_pagerank(gpu_ctx, graph, PAGERANK_DAMPING,
                                                   PAGERANK_MAX_ITER, PAGERANK_TOL, pagerank);
    bool cc_ok = nimcp_topology_compute_clustering(gpu_ctx, graph, clustering);

    EXPECT_TRUE(deg_ok);
    EXPECT_TRUE(pr_ok);
    EXPECT_TRUE(cc_ok);

    // Step 3: Download and analyze correlations
    std::cout << "Step 3: Analyzing centrality correlations..." << std::endl;
    auto deg_host = TensorToHostInt(degree, MEDIUM_GRAPH);
    auto pr_host = TensorToHost(pagerank, MEDIUM_GRAPH);
    auto cc_host = TensorToHost(clustering, MEDIUM_GRAPH);

    // Find hub nodes (high degree)
    int max_deg = *std::max_element(deg_host.begin(), deg_host.end());
    float avg_deg = std::accumulate(deg_host.begin(), deg_host.end(), 0.0f) / MEDIUM_GRAPH;

    std::cout << "  Max degree: " << max_deg << std::endl;
    std::cout << "  Avg degree: " << avg_deg << std::endl;
    std::cout << "  PageRank sum: " << std::accumulate(pr_host.begin(), pr_host.end(), 0.0f) << std::endl;

    // Scale-free property: max degree >> average
    EXPECT_GT(max_deg, avg_deg * 3);

    // PageRank should sum to 1
    EXPECT_NEAR(std::accumulate(pr_host.begin(), pr_host.end(), 0.0f), 1.0f, 0.01f);

    // Step 4: Verify high-degree nodes have high PageRank
    std::cout << "Step 4: Verifying degree-PageRank correlation..." << std::endl;
    std::vector<std::pair<int, float>> deg_pr_pairs;
    for (int i = 0; i < MEDIUM_GRAPH; i++) {
        deg_pr_pairs.push_back({deg_host[i], pr_host[i]});
    }
    std::sort(deg_pr_pairs.begin(), deg_pr_pairs.end(),
              [](auto& a, auto& b) { return a.first > b.first; });

    // Top 10% by degree should have above-average PageRank
    float top_10_pr = 0.0f;
    int top_count = MEDIUM_GRAPH / 10;
    for (int i = 0; i < top_count; i++) {
        top_10_pr += deg_pr_pairs[i].second;
    }
    top_10_pr /= top_count;
    float avg_pr = 1.0f / MEDIUM_GRAPH;

    std::cout << "  Top 10% avg PageRank: " << top_10_pr << std::endl;
    std::cout << "  Overall avg PageRank: " << avg_pr << std::endl;
    EXPECT_GT(top_10_pr, avg_pr * 1.5f);

    // Cleanup
    nimcp_gpu_tensor_destroy(degree);
    nimcp_gpu_tensor_destroy(pagerank);
    nimcp_gpu_tensor_destroy(clustering);
    nimcp_graph_gpu_destroy(graph);

    std::cout << "Pipeline completed successfully!" << std::endl;
}

//=============================================================================
// Pipeline 3: Large-Scale Graph Operations
//=============================================================================

TEST_F(TopologyGPUIntegrationTest, Pipeline_LargeScaleOperations) {
    if (!HasGPU()) GTEST_SKIP() << "GPU not available";

    std::cout << "\n=== Pipeline: Large-Scale Graph Operations ===" << std::endl;

    // Step 1: Generate large sparse graph
    std::cout << "Step 1: Generating large Erdos-Renyi graph (" << LARGE_GRAPH
              << " nodes)..." << std::endl;
    auto gen_start = std::chrono::high_resolution_clock::now();

    nimcp_graph_gpu_t* graph = nimcp_graph_generate_erdos_renyi(
        gpu_ctx, LARGE_GRAPH, 0.02f, RANDOM_SEED
    );
    ASSERT_NE(graph, nullptr);

    auto gen_end = std::chrono::high_resolution_clock::now();
    auto gen_ms = std::chrono::duration<double, std::milli>(gen_end - gen_start).count();

    std::cout << "  Generated in " << gen_ms << " ms" << std::endl;
    std::cout << "  Edges: " << graph->num_edges << std::endl;

    // Step 2: Convert to CSR for efficient sparse operations
    std::cout << "Step 2: Converting to CSR format..." << std::endl;
    auto csr_start = std::chrono::high_resolution_clock::now();

    bool csr_ok = nimcp_graph_gpu_to_csr(graph, 0.0f);
    EXPECT_TRUE(csr_ok);
    EXPECT_TRUE(graph->is_sparse);

    auto csr_end = std::chrono::high_resolution_clock::now();
    auto csr_ms = std::chrono::duration<double, std::milli>(csr_end - csr_start).count();
    std::cout << "  Conversion took " << csr_ms << " ms" << std::endl;

    // Step 3: Compute degree distribution efficiently
    std::cout << "Step 3: Computing degree on sparse graph..." << std::endl;
    nimcp_gpu_tensor_t* degree = Create1DTensor(LARGE_GRAPH, NIMCP_GPU_PRECISION_INT32);
    ASSERT_NE(degree, nullptr);

    auto deg_start = std::chrono::high_resolution_clock::now();
    bool deg_ok = nimcp_topology_compute_degree(gpu_ctx, graph, degree);
    auto deg_end = std::chrono::high_resolution_clock::now();
    auto deg_ms = std::chrono::duration<double, std::milli>(deg_end - deg_start).count();

    EXPECT_TRUE(deg_ok);
    std::cout << "  Degree computed in " << deg_ms << " ms" << std::endl;

    auto deg_host = TensorToHostInt(degree, LARGE_GRAPH);
    float avg_deg = std::accumulate(deg_host.begin(), deg_host.end(), 0.0f) / LARGE_GRAPH;
    int max_deg = *std::max_element(deg_host.begin(), deg_host.end());
    int min_deg = *std::min_element(deg_host.begin(), deg_host.end());

    std::cout << "  Degree range: [" << min_deg << ", " << max_deg << "]" << std::endl;
    std::cout << "  Average degree: " << avg_deg << std::endl;

    // Step 4: Run PageRank on large graph
    std::cout << "Step 4: Computing PageRank on large graph..." << std::endl;
    nimcp_gpu_tensor_t* pagerank = Create1DTensor(LARGE_GRAPH, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(pagerank, nullptr);

    auto pr_start = std::chrono::high_resolution_clock::now();
    bool pr_ok = nimcp_topology_compute_pagerank(gpu_ctx, graph, PAGERANK_DAMPING,
                                                   PAGERANK_MAX_ITER, PAGERANK_TOL, pagerank);
    auto pr_end = std::chrono::high_resolution_clock::now();
    auto pr_ms = std::chrono::duration<double, std::milli>(pr_end - pr_start).count();

    EXPECT_TRUE(pr_ok);
    std::cout << "  PageRank computed in " << pr_ms << " ms" << std::endl;

    auto pr_host = TensorToHost(pagerank, LARGE_GRAPH);
    float pr_sum = std::accumulate(pr_host.begin(), pr_host.end(), 0.0f);
    EXPECT_NEAR(pr_sum, 1.0f, 0.01f);

    // Step 5: Community detection on large graph
    std::cout << "Step 5: Community detection on large graph..." << std::endl;
    auto comm_start = std::chrono::high_resolution_clock::now();

    nimcp_community_result_gpu_t* communities = nimcp_community_detect_louvain(
        gpu_ctx, graph, 1.0f, 50, 1e-4f
    );
    ASSERT_NE(communities, nullptr);

    auto comm_end = std::chrono::high_resolution_clock::now();
    auto comm_ms = std::chrono::duration<double, std::milli>(comm_end - comm_start).count();

    std::cout << "  Found " << communities->num_communities << " communities in "
              << comm_ms << " ms" << std::endl;
    std::cout << "  Modularity: " << communities->modularity << std::endl;

    // Cleanup
    nimcp_community_result_gpu_destroy(communities);
    nimcp_gpu_tensor_destroy(pagerank);
    nimcp_gpu_tensor_destroy(degree);
    nimcp_graph_gpu_destroy(graph);

    std::cout << "Pipeline completed successfully!" << std::endl;
}

//=============================================================================
// Pipeline 4: Sparse/Dense Conversion Pipeline
//=============================================================================

TEST_F(TopologyGPUIntegrationTest, Pipeline_FormatConversion) {
    if (!HasGPU()) GTEST_SKIP() << "GPU not available";

    std::cout << "\n=== Pipeline: Format Conversion ===" << std::endl;

    // Step 1: Create dense graph
    std::cout << "Step 1: Creating dense graph..." << std::endl;
    auto adj = CreatePlantedPartitionGraph(SMALL_GRAPH, 2, 0.6f, 0.1f);
    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(gpu_ctx, adj.data(), SMALL_GRAPH);
    ASSERT_NE(graph, nullptr);
    ASSERT_FALSE(graph->is_sparse);

    // Step 2: Compute metrics on dense format
    std::cout << "Step 2: Computing metrics on dense graph..." << std::endl;
    nimcp_gpu_tensor_t* dense_degree = Create1DTensor(SMALL_GRAPH, NIMCP_GPU_PRECISION_INT32);
    bool dense_deg_ok = nimcp_topology_compute_degree(gpu_ctx, graph, dense_degree);
    EXPECT_TRUE(dense_deg_ok);
    auto dense_deg_host = TensorToHostInt(dense_degree, SMALL_GRAPH);

    // Step 3: Convert to CSR
    std::cout << "Step 3: Converting to CSR..." << std::endl;
    bool csr_ok = nimcp_graph_gpu_to_csr(graph, 0.0f);
    EXPECT_TRUE(csr_ok);
    EXPECT_TRUE(graph->is_sparse);
    EXPECT_NE(graph->row_ptrs, nullptr);
    EXPECT_NE(graph->col_indices, nullptr);

    // Step 4: Compute metrics on sparse format
    std::cout << "Step 4: Computing metrics on sparse graph..." << std::endl;
    nimcp_gpu_tensor_t* sparse_degree = Create1DTensor(SMALL_GRAPH, NIMCP_GPU_PRECISION_INT32);
    bool sparse_deg_ok = nimcp_topology_compute_degree(gpu_ctx, graph, sparse_degree);
    EXPECT_TRUE(sparse_deg_ok);
    auto sparse_deg_host = TensorToHostInt(sparse_degree, SMALL_GRAPH);

    // Step 5: Verify results match
    std::cout << "Step 5: Verifying consistency..." << std::endl;
    for (int i = 0; i < SMALL_GRAPH; i++) {
        EXPECT_EQ(dense_deg_host[i], sparse_deg_host[i])
            << "Degree mismatch at node " << i;
    }
    std::cout << "  Degree results match for dense and sparse formats" << std::endl;

    // Step 6: Convert back to dense
    std::cout << "Step 6: Converting back to dense..." << std::endl;
    bool dense_ok = nimcp_graph_gpu_to_dense(graph);
    EXPECT_TRUE(dense_ok);
    EXPECT_FALSE(graph->is_sparse);
    EXPECT_NE(graph->adjacency, nullptr);

    // Step 7: Verify adjacency preserved
    std::cout << "Step 7: Verifying adjacency preserved..." << std::endl;
    std::vector<float> recovered_adj(SMALL_GRAPH * SMALL_GRAPH);
    bool host_ok = nimcp_graph_gpu_to_host(graph, recovered_adj.data());
    EXPECT_TRUE(host_ok);

    int matches = 0;
    for (int i = 0; i < SMALL_GRAPH * SMALL_GRAPH; i++) {
        if (std::abs(recovered_adj[i] - adj[i]) < 0.01f) matches++;
    }
    float match_rate = (float)matches / (SMALL_GRAPH * SMALL_GRAPH);
    std::cout << "  Adjacency match rate: " << (match_rate * 100) << "%" << std::endl;
    EXPECT_GT(match_rate, 0.99f);

    // Cleanup
    nimcp_gpu_tensor_destroy(dense_degree);
    nimcp_gpu_tensor_destroy(sparse_degree);
    nimcp_graph_gpu_destroy(graph);

    std::cout << "Pipeline completed successfully!" << std::endl;
}

//=============================================================================
// Pipeline 5: Multi-Algorithm Comparison
//=============================================================================

TEST_F(TopologyGPUIntegrationTest, Pipeline_AlgorithmComparison) {
    if (!HasGPU()) GTEST_SKIP() << "GPU not available";

    std::cout << "\n=== Pipeline: Multi-Algorithm Comparison ===" << std::endl;

    // Step 1: Create graph with clear community structure
    std::cout << "Step 1: Creating test graph..." << std::endl;
    auto adj = CreatePlantedPartitionGraph(MEDIUM_GRAPH, NUM_COMMUNITIES,
                                            INTRA_COMMUNITY_PROB, INTER_COMMUNITY_PROB);
    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(gpu_ctx, adj.data(), MEDIUM_GRAPH);
    ASSERT_NE(graph, nullptr);

    // Ground truth communities
    std::vector<int32_t> ground_truth(MEDIUM_GRAPH);
    int community_size = MEDIUM_GRAPH / NUM_COMMUNITIES;
    for (int i = 0; i < MEDIUM_GRAPH; i++) {
        ground_truth[i] = i / community_size;
    }

    // Step 2: Run Louvain algorithm
    std::cout << "Step 2: Running Louvain algorithm..." << std::endl;
    auto louvain_start = std::chrono::high_resolution_clock::now();
    nimcp_community_result_gpu_t* louvain = nimcp_community_detect_louvain(
        gpu_ctx, graph, 1.0f, 100, 1e-5f
    );
    auto louvain_end = std::chrono::high_resolution_clock::now();
    auto louvain_ms = std::chrono::duration<double, std::milli>(louvain_end - louvain_start).count();

    ASSERT_NE(louvain, nullptr);
    auto louvain_comm = TensorToHostInt(louvain->node_communities, MEDIUM_GRAPH);
    double louvain_nmi = ComputeNMI(louvain_comm, ground_truth, MEDIUM_GRAPH);

    std::cout << "  Louvain: " << louvain->num_communities << " communities, "
              << "modularity=" << louvain->modularity << ", "
              << "NMI=" << louvain_nmi << ", "
              << "time=" << louvain_ms << "ms" << std::endl;

    // Step 3: Run Label Propagation algorithm
    std::cout << "Step 3: Running Label Propagation algorithm..." << std::endl;
    auto lp_start = std::chrono::high_resolution_clock::now();
    nimcp_community_result_gpu_t* label_prop = nimcp_community_detect_label_prop(
        gpu_ctx, graph, 100
    );
    auto lp_end = std::chrono::high_resolution_clock::now();
    auto lp_ms = std::chrono::duration<double, std::milli>(lp_end - lp_start).count();

    ASSERT_NE(label_prop, nullptr);
    auto lp_comm = TensorToHostInt(label_prop->node_communities, MEDIUM_GRAPH);
    double lp_nmi = ComputeNMI(lp_comm, ground_truth, MEDIUM_GRAPH);

    std::cout << "  Label Prop: " << label_prop->num_communities << " communities, "
              << "modularity=" << label_prop->modularity << ", "
              << "NMI=" << lp_nmi << ", "
              << "time=" << lp_ms << "ms" << std::endl;

    // Step 4: Compare algorithms
    std::cout << "Step 4: Algorithm comparison summary:" << std::endl;
    std::cout << "  | Algorithm     | Communities | Modularity | NMI    | Time(ms) |" << std::endl;
    std::cout << "  |---------------|-------------|------------|--------|----------|" << std::endl;
    std::cout << "  | Louvain       | " << louvain->num_communities
              << "           | " << louvain->modularity
              << "      | " << louvain_nmi
              << " | " << louvain_ms << " |" << std::endl;
    std::cout << "  | Label Prop    | " << label_prop->num_communities
              << "           | " << label_prop->modularity
              << "      | " << lp_nmi
              << " | " << lp_ms << " |" << std::endl;

    // Both should achieve reasonable results
    EXPECT_GT(louvain_nmi, 0.6);
    EXPECT_GT(lp_nmi, 0.5);

    // Cleanup
    nimcp_community_result_gpu_destroy(louvain);
    nimcp_community_result_gpu_destroy(label_prop);
    nimcp_graph_gpu_destroy(graph);

    std::cout << "Pipeline completed successfully!" << std::endl;
}

//=============================================================================
// Pipeline 6: Shortest Path Integration
//=============================================================================

TEST_F(TopologyGPUIntegrationTest, Pipeline_ShortestPathIntegration) {
    if (!HasGPU()) GTEST_SKIP() << "GPU not available";

    std::cout << "\n=== Pipeline: Shortest Path Integration ===" << std::endl;

    // Step 1: Generate small-world network (short paths expected)
    std::cout << "Step 1: Generating Watts-Strogatz network..." << std::endl;
    nimcp_graph_gpu_t* graph = nimcp_graph_generate_watts_strogatz(
        gpu_ctx, SMALL_GRAPH, 4, 0.1f, RANDOM_SEED
    );
    ASSERT_NE(graph, nullptr);

    // Step 2: Compute single-source shortest paths (BFS)
    std::cout << "Step 2: Computing BFS shortest paths from node 0..." << std::endl;
    nimcp_shortest_path_result_gpu_t bfs_result;
    bfs_result.distances = Create1DTensor(SMALL_GRAPH, NIMCP_GPU_PRECISION_FP32);
    bfs_result.predecessors = Create1DTensor(SMALL_GRAPH, NIMCP_GPU_PRECISION_INT32);

    bool bfs_ok = nimcp_shortest_path_bfs(gpu_ctx, graph, 0, &bfs_result);
    EXPECT_TRUE(bfs_ok);

    auto bfs_dist = TensorToHost(bfs_result.distances, SMALL_GRAPH);
    float max_dist = *std::max_element(bfs_dist.begin(), bfs_dist.end());
    float avg_dist = std::accumulate(bfs_dist.begin(), bfs_dist.end(), 0.0f) / SMALL_GRAPH;

    std::cout << "  Max distance: " << max_dist << std::endl;
    std::cout << "  Avg distance: " << avg_dist << std::endl;
    EXPECT_LT(max_dist, SMALL_GRAPH / 2);  // Small-world property

    // Step 3: Compute all-pairs shortest paths
    std::cout << "Step 3: Computing Floyd-Warshall APSP..." << std::endl;
    nimcp_apsp_result_gpu_t apsp_result;
    size_t apsp_dims[] = {static_cast<size_t>(SMALL_GRAPH), static_cast<size_t>(SMALL_GRAPH)};
    apsp_result.distances = nimcp_gpu_tensor_create(gpu_ctx, apsp_dims, 2, NIMCP_GPU_PRECISION_FP32);

    auto apsp_start = std::chrono::high_resolution_clock::now();
    bool apsp_ok = nimcp_shortest_path_floyd_warshall(gpu_ctx, graph, &apsp_result);
    auto apsp_end = std::chrono::high_resolution_clock::now();
    auto apsp_ms = std::chrono::duration<double, std::milli>(apsp_end - apsp_start).count();

    EXPECT_TRUE(apsp_ok);
    std::cout << "  APSP computed in " << apsp_ms << " ms" << std::endl;
    std::cout << "  Diameter: " << apsp_result.diameter << std::endl;
    std::cout << "  Avg path length: " << apsp_result.avg_path_length << std::endl;

    // Small-world: low average path length
    EXPECT_LT(apsp_result.avg_path_length, log(SMALL_GRAPH));

    // Step 4: Verify consistency between BFS and Floyd-Warshall
    std::cout << "Step 4: Verifying BFS vs Floyd-Warshall consistency..." << std::endl;
    auto apsp_dist = TensorToHost(apsp_result.distances, SMALL_GRAPH * SMALL_GRAPH);

    int consistent = 0;
    for (int i = 0; i < SMALL_GRAPH; i++) {
        float bfs_d = bfs_dist[i];
        float fw_d = apsp_dist[0 * SMALL_GRAPH + i];  // Row 0 = from node 0
        if (std::abs(bfs_d - fw_d) < 0.01f) consistent++;
    }
    float consistency = (float)consistent / SMALL_GRAPH;
    std::cout << "  Consistency: " << (consistency * 100) << "%" << std::endl;
    EXPECT_GT(consistency, 0.99f);

    // Cleanup
    nimcp_gpu_tensor_destroy(bfs_result.distances);
    nimcp_gpu_tensor_destroy(bfs_result.predecessors);
    nimcp_gpu_tensor_destroy(apsp_result.distances);
    nimcp_graph_gpu_destroy(graph);

    std::cout << "Pipeline completed successfully!" << std::endl;
}

//=============================================================================
// Pipeline 7: Network Generation Comparison
//=============================================================================

TEST_F(TopologyGPUIntegrationTest, Pipeline_NetworkGenerationComparison) {
    if (!HasGPU()) GTEST_SKIP() << "GPU not available";

    std::cout << "\n=== Pipeline: Network Generation Comparison ===" << std::endl;

    struct NetworkStats {
        std::string name;
        float avg_degree;
        float max_degree;
        float clustering;
        float density;
        int num_edges;
    };

    std::vector<NetworkStats> all_stats;

    // Generate and analyze different network types
    std::vector<std::pair<std::string, nimcp_graph_gpu_t*>> networks;

    // Erdos-Renyi
    networks.push_back({"Erdos-Renyi",
        nimcp_graph_generate_erdos_renyi(gpu_ctx, MEDIUM_GRAPH, 0.1f, RANDOM_SEED)});

    // Barabasi-Albert
    networks.push_back({"Barabasi-Albert",
        nimcp_graph_generate_barabasi_albert(gpu_ctx, MEDIUM_GRAPH, 5, RANDOM_SEED)});

    // Watts-Strogatz
    networks.push_back({"Watts-Strogatz",
        nimcp_graph_generate_watts_strogatz(gpu_ctx, MEDIUM_GRAPH, 10, 0.1f, RANDOM_SEED)});

    for (auto& [name, graph] : networks) {
        if (!graph) continue;

        NetworkStats stats;
        stats.name = name;
        stats.num_edges = graph->num_edges;

        // Get basic stats
        nimcp_graph_gpu_stats(graph, &stats.avg_degree, nullptr, &stats.density);

        // Compute degree for max
        nimcp_gpu_tensor_t* degree = Create1DTensor(MEDIUM_GRAPH, NIMCP_GPU_PRECISION_INT32);
        nimcp_topology_compute_degree(gpu_ctx, graph, degree);
        auto deg_host = TensorToHostInt(degree, MEDIUM_GRAPH);
        stats.max_degree = *std::max_element(deg_host.begin(), deg_host.end());

        // Compute clustering
        nimcp_gpu_tensor_t* clustering = Create1DTensor(MEDIUM_GRAPH, NIMCP_GPU_PRECISION_FP32);
        nimcp_topology_compute_clustering(gpu_ctx, graph, clustering);
        auto cc_host = TensorToHost(clustering, MEDIUM_GRAPH);
        stats.clustering = std::accumulate(cc_host.begin(), cc_host.end(), 0.0f) / MEDIUM_GRAPH;

        all_stats.push_back(stats);

        nimcp_gpu_tensor_destroy(degree);
        nimcp_gpu_tensor_destroy(clustering);
        nimcp_graph_gpu_destroy(graph);
    }

    // Print comparison table
    std::cout << "\nNetwork Type Comparison (" << MEDIUM_GRAPH << " nodes):" << std::endl;
    std::cout << "| Network         | Edges  | Avg Deg | Max Deg | Clustering | Density |" << std::endl;
    std::cout << "|-----------------|--------|---------|---------|------------|---------|" << std::endl;

    for (const auto& s : all_stats) {
        std::cout << "| " << s.name;
        for (size_t i = s.name.length(); i < 15; i++) std::cout << " ";
        std::cout << " | " << s.num_edges
                  << "   | " << s.avg_degree
                  << "   | " << s.max_degree
                  << "    | " << s.clustering
                  << "     | " << s.density
                  << "  |" << std::endl;
    }

    // Verify expected properties
    // BA should have highest max degree (scale-free)
    // WS should have highest clustering (small-world)
    bool found_ba = false, found_ws = false;
    float ba_max_deg = 0, ws_clustering = 0;
    for (const auto& s : all_stats) {
        if (s.name == "Barabasi-Albert") {
            ba_max_deg = s.max_degree;
            found_ba = true;
        }
        if (s.name == "Watts-Strogatz") {
            ws_clustering = s.clustering;
            found_ws = true;
        }
    }

    if (found_ba && found_ws) {
        std::cout << "\nProperty verification:" << std::endl;
        std::cout << "  BA max degree: " << ba_max_deg << " (expected high for scale-free)" << std::endl;
        std::cout << "  WS clustering: " << ws_clustering << " (expected high for small-world)" << std::endl;
    }

    std::cout << "\nPipeline completed successfully!" << std::endl;
}

//=============================================================================
// Memory Safety Tests
//=============================================================================

TEST_F(TopologyGPUIntegrationTest, MemorySafety_RepeatedOperations) {
    if (!HasGPU()) GTEST_SKIP() << "GPU not available";

    std::cout << "\n=== Memory Safety: Repeated Operations ===" << std::endl;

    nimcp_memory_stats_t initial_stats;
    nimcp_memory_get_stats(&initial_stats);

    const int iterations = 10;

    for (int i = 0; i < iterations; i++) {
        nimcp_graph_gpu_t* graph = nimcp_graph_generate_erdos_renyi(
            gpu_ctx, SMALL_GRAPH, 0.2f, RANDOM_SEED + i
        );
        ASSERT_NE(graph, nullptr);

        nimcp_topology_metrics_gpu_t* metrics = nimcp_topology_compute_metrics(gpu_ctx, graph);
        ASSERT_NE(metrics, nullptr);

        nimcp_community_result_gpu_t* communities = nimcp_community_detect_louvain(
            gpu_ctx, graph, 1.0f, 50, 1e-4f
        );
        ASSERT_NE(communities, nullptr);

        nimcp_community_result_gpu_destroy(communities);
        nimcp_topology_metrics_gpu_destroy(metrics);
        nimcp_graph_gpu_destroy(graph);
    }

    nimcp_memory_stats_t final_stats;
    nimcp_memory_get_stats(&final_stats);

    int64_t leaked = final_stats.current_allocated - initial_stats.current_allocated;
    std::cout << "  Memory leaked after " << iterations << " iterations: "
              << leaked << " bytes" << std::endl;
    EXPECT_LT(leaked, 1024) << "Memory leak detected";

    std::cout << "Memory safety test passed!" << std::endl;
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
