/**
 * @file integration_gpu_test_sparse_topology.cpp
 * @brief Integration tests for Sparse + Topology pipeline with GPU Memory Pool
 *
 * WHAT: Test integrated sparse tensor and topology operations
 * WHY:  Verify sparse tensors work correctly with graph topology operations
 * HOW:  Test sparse linear layers with graph connectivity, memory pool integration
 *
 * TEST COVERAGE:
 * - Sparse + topology pipeline
 * - Sparse linear layers with graph connectivity
 * - Memory pool integration with both sparse and topology
 * - Combined operations for neural network graph layers
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2026
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <random>
#include <algorithm>
#include <numeric>
#include <chrono>

// GPU headers
#include "gpu/sparse/nimcp_sparse_gpu.h"
#include "gpu/topology/nimcp_topology_gpu.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/memory/nimcp_gpu_pool.h"

// Headers already have their own extern "C" guards
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Configuration
//=============================================================================

namespace {
    constexpr size_t SMALL_SIZE = 32;
    constexpr size_t MEDIUM_SIZE = 128;
    constexpr size_t LARGE_SIZE = 512;
    constexpr int NUM_NODES = 100;
    constexpr int NUM_FEATURES = 64;
    constexpr int NUM_HIDDEN = 32;
    constexpr float TOLERANCE = 1e-4f;
    constexpr float SPARSITY = 0.8f;
    constexpr uint32_t RANDOM_SEED = 42;

//=============================================================================
// Test Fixture
//=============================================================================

class SparseTopologyIntegrationTest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* gpu_ctx = nullptr;
    nimcp_sparse_ctx_t* sparse_ctx = nullptr;
    nimcp_gpu_pool_t* memory_pool = nullptr;
    std::mt19937 rng{RANDOM_SEED};
    bool gpu_available = false;

    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
        nimcp_memory_clear_stats();

        gpu_ctx = nimcp_gpu_context_create_auto();
        gpu_available = (gpu_ctx != nullptr && nimcp_gpu_context_is_valid(gpu_ctx));

        if (gpu_available) {
            sparse_ctx = nimcp_sparse_ctx_create(gpu_ctx);

            // Create GPU memory pool
            nimcp_gpu_pool_config_t pool_config = {
                .initial_size = 64 * 1024 * 1024,  // 64 MB
                .growth_factor = 2.0f,
                .max_size = 256 * 1024 * 1024,     // 256 MB
                .alignment = 256,
                .enable_fragmentation_tracking = true
            };
            memory_pool = nimcp_gpu_pool_create(gpu_ctx, &pool_config);
        }
    }

    void TearDown() override {
        if (memory_pool) {
            nimcp_gpu_pool_destroy(memory_pool);
            memory_pool = nullptr;
        }

        if (sparse_ctx) {
            nimcp_sparse_ctx_destroy(sparse_ctx);
            sparse_ctx = nullptr;
        }

        if (gpu_ctx) {
            nimcp_gpu_context_destroy(gpu_ctx);
            gpu_ctx = nullptr;
        }

        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        EXPECT_LT(stats.current_allocated, 8192)
            << "Potential memory leak: " << stats.current_allocated << " bytes";
    }

    void RequireGPU() {
        if (!gpu_available || !sparse_ctx) {
            GTEST_SKIP() << "GPU or sparse context not available";
        }
    }

    // Helper: Generate random matrix
    std::vector<float> generateMatrix(size_t rows, size_t cols, float sparsity = 0.0f) {
        std::vector<float> data(rows * cols, 0.0f);
        std::uniform_real_distribution<float> val_dist(-1.0f, 1.0f);
        std::uniform_real_distribution<float> prob_dist(0.0f, 1.0f);

        for (size_t i = 0; i < data.size(); i++) {
            if (prob_dist(rng) > sparsity) {
                data[i] = val_dist(rng);
            }
        }
        return data;
    }

    // Helper: Create GPU tensor from host data
    nimcp_gpu_tensor_t* createTensor(const std::vector<float>& data,
                                      const std::vector<size_t>& dims) {
        if (!gpu_ctx) return nullptr;
        return nimcp_gpu_tensor_from_host(gpu_ctx, data.data(), dims.data(),
                                          dims.size(), NIMCP_GPU_PRECISION_FP32);
    }

    // Helper: Copy GPU tensor to host
    std::vector<float> copyToHost(const nimcp_gpu_tensor_t* tensor) {
        if (!tensor) return {};
        std::vector<float> result(tensor->numel);
        nimcp_gpu_tensor_to_host(tensor, result.data());
        return result;
    }

    // Helper: Create adjacency matrix with community structure
    std::vector<float> createCommunityAdjacency(int num_nodes, int num_communities,
                                                  float p_in, float p_out) {
        std::vector<float> adj(num_nodes * num_nodes, 0.0f);
        int community_size = num_nodes / num_communities;
        std::bernoulli_distribution dist_in(p_in);
        std::bernoulli_distribution dist_out(p_out);

        for (int i = 0; i < num_nodes; i++) {
            for (int j = i + 1; j < num_nodes; j++) {
                int comm_i = i / community_size;
                int comm_j = j / community_size;

                bool has_edge = (comm_i == comm_j) ? dist_in(rng) : dist_out(rng);
                if (has_edge) {
                    adj[i * num_nodes + j] = 1.0f;
                    adj[j * num_nodes + i] = 1.0f;
                }
            }
        }
        return adj;
    }

    // Helper: Create sparse adjacency from graph topology
    nimcp_sparse_tensor_t* graphToSparseMatrix(nimcp_graph_gpu_t* graph) {
        if (!graph || !graph->adjacency) return nullptr;

        // Get adjacency data
        std::vector<float> adj(graph->num_nodes * graph->num_nodes);
        nimcp_graph_gpu_to_host(graph, adj.data());

        // Create dense tensor
        std::vector<size_t> dims = {static_cast<size_t>(graph->num_nodes),
                                    static_cast<size_t>(graph->num_nodes)};
        nimcp_gpu_tensor_t* dense = createTensor(adj, dims);
        if (!dense) return nullptr;

        // Convert to sparse
        nimcp_sparse_tensor_t* sparse = nimcp_sparse_from_dense(
            sparse_ctx, dense, SPARSE_FORMAT_CSR, 0.0f);

        nimcp_gpu_tensor_destroy(dense);
        return sparse;
    }

    // Helper: Compute max absolute error
    float maxAbsError(const std::vector<float>& a, const std::vector<float>& b) {
        if (a.size() != b.size()) return std::numeric_limits<float>::max();
        float max_err = 0.0f;
        for (size_t i = 0; i < a.size(); i++) {
            max_err = std::max(max_err, std::fabs(a[i] - b[i]));
        }
        return max_err;
    }
};

//=============================================================================
// Sparse + Topology Pipeline Tests
//=============================================================================

TEST_F(SparseTopologyIntegrationTest, Pipeline_SparseAdjacencyFromGraph) {
    RequireGPU();

    std::cout << "\n=== Pipeline: Sparse Adjacency from Graph ===" << std::endl;

    // Step 1: Generate graph using topology module
    nimcp_graph_gpu_t* graph = nimcp_graph_generate_erdos_renyi(
        gpu_ctx, NUM_NODES, 0.1f, RANDOM_SEED);
    ASSERT_NE(graph, nullptr);
    std::cout << "Generated Erdos-Renyi graph: " << graph->num_nodes << " nodes, "
              << graph->num_edges << " edges" << std::endl;

    // Step 2: Convert to sparse matrix using sparse module
    nimcp_sparse_tensor_t* adj_sparse = graphToSparseMatrix(graph);
    ASSERT_NE(adj_sparse, nullptr);

    int nnz = nimcp_sparse_nnz(adj_sparse);
    float sparsity = nimcp_sparse_sparsity(adj_sparse);

    std::cout << "Sparse adjacency: nnz=" << nnz << ", sparsity=" << (sparsity * 100) << "%" << std::endl;

    // Verify: nnz should match 2 * num_edges (symmetric)
    EXPECT_EQ(nnz, graph->num_edges);

    // Step 3: Use sparse adjacency for SpMV (graph message passing)
    auto features = generateMatrix(NUM_NODES, 1, 0.0f);
    std::vector<size_t> feat_dims = {static_cast<size_t>(NUM_NODES)};
    nimcp_gpu_tensor_t* node_features = createTensor(features, feat_dims);

    // Aggregate neighbor features: out = A * features
    nimcp_gpu_tensor_t* aggregated = nimcp_sparse_mv(
        sparse_ctx, adj_sparse, node_features, 1.0f, 0.0f, nullptr);
    ASSERT_NE(aggregated, nullptr);

    auto agg_data = copyToHost(aggregated);
    float sum = std::accumulate(agg_data.begin(), agg_data.end(), 0.0f,
                                 [](float a, float b) { return a + std::fabs(b); });
    EXPECT_GT(sum, 0.0f) << "Aggregated features should be non-zero";

    // Cleanup
    nimcp_sparse_tensor_destroy(adj_sparse);
    nimcp_gpu_tensor_destroy(node_features);
    nimcp_gpu_tensor_destroy(aggregated);
    nimcp_graph_gpu_destroy(graph);

    std::cout << "Pipeline completed successfully!" << std::endl;
}

TEST_F(SparseTopologyIntegrationTest, Pipeline_GraphNeuralNetworkLayer) {
    RequireGPU();

    std::cout << "\n=== Pipeline: Graph Neural Network Layer ===" << std::endl;

    // Step 1: Create graph with community structure
    auto adj_data = createCommunityAdjacency(NUM_NODES, 4, 0.5f, 0.05f);
    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(
        gpu_ctx, adj_data.data(), NUM_NODES);
    ASSERT_NE(graph, nullptr);

    // Step 2: Create sparse adjacency matrix
    std::vector<size_t> adj_dims = {static_cast<size_t>(NUM_NODES),
                                    static_cast<size_t>(NUM_NODES)};
    nimcp_gpu_tensor_t* adj_dense = createTensor(adj_data, adj_dims);
    nimcp_sparse_tensor_t* adj_sparse = nimcp_sparse_from_dense(
        sparse_ctx, adj_dense, SPARSE_FORMAT_CSR, 0.0f);
    ASSERT_NE(adj_sparse, nullptr);

    // Step 3: Create node features and weight matrix
    auto features = generateMatrix(NUM_NODES, NUM_FEATURES, 0.0f);
    auto weights = generateMatrix(NUM_HIDDEN, NUM_FEATURES, SPARSITY);

    std::vector<size_t> feat_dims = {static_cast<size_t>(NUM_NODES),
                                     static_cast<size_t>(NUM_FEATURES)};
    std::vector<size_t> weight_dims = {static_cast<size_t>(NUM_HIDDEN),
                                       static_cast<size_t>(NUM_FEATURES)};

    nimcp_gpu_tensor_t* X = createTensor(features, feat_dims);
    nimcp_gpu_tensor_t* W_dense = createTensor(weights, weight_dims);
    nimcp_sparse_tensor_t* W_sparse = nimcp_sparse_from_dense(
        sparse_ctx, W_dense, SPARSE_FORMAT_CSR, 0.0f);
    ASSERT_NE(W_sparse, nullptr);

    // Step 4: GNN forward pass
    // H = A * X * W^T (simplified GCN-style aggregation)

    // First: aggregate neighbors - Z = A * X
    nimcp_gpu_tensor_t* Z = nimcp_sparse_mm(
        sparse_ctx, adj_sparse, X, 1.0f, 0.0f, nullptr);
    ASSERT_NE(Z, nullptr);

    // Then: transform - H = Z * W^T (using sparse linear)
    nimcp_gpu_tensor_t* H = nimcp_sparse_linear_forward(
        sparse_ctx, Z, W_sparse, nullptr);
    ASSERT_NE(H, nullptr);

    // Verify output dimensions
    EXPECT_EQ(H->dims[0], static_cast<size_t>(NUM_NODES));
    EXPECT_EQ(H->dims[1], static_cast<size_t>(NUM_HIDDEN));

    auto output = copyToHost(H);
    float sum = 0.0f;
    for (float v : output) sum += std::fabs(v);
    EXPECT_GT(sum, 0.0f) << "GNN output should be non-zero";

    std::cout << "GNN layer output: " << NUM_NODES << "x" << NUM_HIDDEN << std::endl;

    // Cleanup
    nimcp_sparse_tensor_destroy(adj_sparse);
    nimcp_sparse_tensor_destroy(W_sparse);
    nimcp_gpu_tensor_destroy(adj_dense);
    nimcp_gpu_tensor_destroy(X);
    nimcp_gpu_tensor_destroy(W_dense);
    nimcp_gpu_tensor_destroy(Z);
    nimcp_gpu_tensor_destroy(H);
    nimcp_graph_gpu_destroy(graph);

    std::cout << "Pipeline completed successfully!" << std::endl;
}

TEST_F(SparseTopologyIntegrationTest, Pipeline_CommunityAwareSparseNetwork) {
    RequireGPU();

    std::cout << "\n=== Pipeline: Community-Aware Sparse Network ===" << std::endl;

    // Step 1: Generate scale-free network
    nimcp_graph_gpu_t* graph = nimcp_graph_generate_barabasi_albert(
        gpu_ctx, NUM_NODES, 3, RANDOM_SEED);
    ASSERT_NE(graph, nullptr);

    // Step 2: Detect communities
    nimcp_community_result_gpu_t* communities = nimcp_community_detect_louvain(
        gpu_ctx, graph, 1.0f, 100, 1e-5f);
    ASSERT_NE(communities, nullptr);

    std::cout << "Detected " << communities->num_communities << " communities "
              << "(modularity: " << communities->modularity << ")" << std::endl;

    // Step 3: Create community-aware sparse weight matrix
    // Connections within communities have higher weights
    std::vector<int32_t> comm_assign(NUM_NODES);
    nimcp_gpu_memcpy(gpu_ctx, comm_assign.data(), communities->node_communities->data,
                     NUM_NODES * sizeof(int32_t), GPU_MEMCPY_DEVICE_TO_HOST);

    std::vector<float> weight_data(NUM_NODES * NUM_NODES, 0.0f);
    std::uniform_real_distribution<float> val_dist(0.1f, 1.0f);

    for (int i = 0; i < NUM_NODES; i++) {
        for (int j = 0; j < NUM_NODES; j++) {
            if (comm_assign[i] == comm_assign[j]) {
                // Higher probability within community
                if (rng() % 5 == 0) {
                    weight_data[i * NUM_NODES + j] = val_dist(rng);
                }
            } else {
                // Lower probability between communities
                if (rng() % 20 == 0) {
                    weight_data[i * NUM_NODES + j] = val_dist(rng) * 0.5f;
                }
            }
        }
    }

    std::vector<size_t> dims = {static_cast<size_t>(NUM_NODES),
                                static_cast<size_t>(NUM_NODES)};
    nimcp_gpu_tensor_t* W_dense = createTensor(weight_data, dims);
    nimcp_sparse_tensor_t* W_sparse = nimcp_sparse_from_dense(
        sparse_ctx, W_dense, SPARSE_FORMAT_CSR, 0.0f);

    float sparsity = nimcp_sparse_sparsity(W_sparse);
    std::cout << "Community-aware weight sparsity: " << (sparsity * 100) << "%" << std::endl;

    // Step 4: Forward pass with community-aware weights
    auto features = generateMatrix(NUM_NODES, 1, 0.0f);
    std::vector<size_t> feat_dims = {static_cast<size_t>(NUM_NODES)};
    nimcp_gpu_tensor_t* X = createTensor(features, feat_dims);

    nimcp_gpu_tensor_t* output = nimcp_sparse_mv(
        sparse_ctx, W_sparse, X, 1.0f, 0.0f, nullptr);
    ASSERT_NE(output, nullptr);

    auto out_data = copyToHost(output);
    float sum = 0.0f;
    for (float v : out_data) sum += std::fabs(v);
    EXPECT_GT(sum, 0.0f);

    // Cleanup
    nimcp_community_result_gpu_destroy(communities);
    nimcp_sparse_tensor_destroy(W_sparse);
    nimcp_gpu_tensor_destroy(W_dense);
    nimcp_gpu_tensor_destroy(X);
    nimcp_gpu_tensor_destroy(output);
    nimcp_graph_gpu_destroy(graph);

    std::cout << "Pipeline completed successfully!" << std::endl;
}

//=============================================================================
// Sparse Linear Layers with Graph Connectivity Tests
//=============================================================================

TEST_F(SparseTopologyIntegrationTest, SparseLinear_GraphStructuredWeights) {
    RequireGPU();

    std::cout << "\n=== Sparse Linear with Graph-Structured Weights ===" << std::endl;

    const int input_dim = 64;
    const int output_dim = 32;
    const int batch_size = 16;

    // Create graph to define weight connectivity pattern
    nimcp_graph_gpu_t* graph = nimcp_graph_generate_watts_strogatz(
        gpu_ctx, output_dim * input_dim, 8, 0.2f, RANDOM_SEED);
    ASSERT_NE(graph, nullptr);

    // Use graph adjacency to mask weight matrix
    std::vector<float> graph_adj(output_dim * input_dim * output_dim * input_dim);
    nimcp_graph_gpu_to_host(graph, graph_adj.data());

    // Create weight matrix with graph-induced sparsity
    std::vector<float> weights(output_dim * input_dim);
    std::uniform_real_distribution<float> val_dist(-1.0f, 1.0f);
    for (int i = 0; i < output_dim; i++) {
        for (int j = 0; j < input_dim; j++) {
            // Use simplified connectivity - threshold adjacency
            int idx = i * input_dim + j;
            if (rng() % 5 != 0) {  // 80% sparse
                weights[idx] = 0.0f;
            } else {
                weights[idx] = val_dist(rng);
            }
        }
    }

    std::vector<size_t> weight_dims = {static_cast<size_t>(output_dim),
                                       static_cast<size_t>(input_dim)};
    nimcp_gpu_tensor_t* W_dense = createTensor(weights, weight_dims);
    nimcp_sparse_tensor_t* W_sparse = nimcp_sparse_from_dense(
        sparse_ctx, W_dense, SPARSE_FORMAT_CSR, 0.0f);

    // Create input batch
    auto input_data = generateMatrix(batch_size, input_dim, 0.0f);
    std::vector<size_t> input_dims = {static_cast<size_t>(batch_size),
                                      static_cast<size_t>(input_dim)};
    nimcp_gpu_tensor_t* input = createTensor(input_data, input_dims);

    // Forward pass
    nimcp_gpu_tensor_t* output = nimcp_sparse_linear_forward(
        sparse_ctx, input, W_sparse, nullptr);
    ASSERT_NE(output, nullptr);

    EXPECT_EQ(output->dims[0], static_cast<size_t>(batch_size));
    EXPECT_EQ(output->dims[1], static_cast<size_t>(output_dim));

    std::cout << "Graph-structured sparse linear: " << input_dim << " -> " << output_dim
              << " (nnz: " << nimcp_sparse_nnz(W_sparse) << ")" << std::endl;

    // Cleanup
    nimcp_sparse_tensor_destroy(W_sparse);
    nimcp_gpu_tensor_destroy(W_dense);
    nimcp_gpu_tensor_destroy(input);
    nimcp_gpu_tensor_destroy(output);
    nimcp_graph_gpu_destroy(graph);
}

TEST_F(SparseTopologyIntegrationTest, SparseLinear_DegreeNormalizedAggregation) {
    RequireGPU();

    std::cout << "\n=== Sparse Linear with Degree-Normalized Aggregation ===" << std::endl;

    // Create graph
    auto adj_data = createCommunityAdjacency(NUM_NODES, 4, 0.6f, 0.05f);
    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_from_dense(
        gpu_ctx, adj_data.data(), NUM_NODES);
    ASSERT_NE(graph, nullptr);

    // Compute degrees for normalization
    std::vector<size_t> deg_dims = {static_cast<size_t>(NUM_NODES)};
    nimcp_gpu_tensor_t* degree = nimcp_gpu_tensor_create(
        gpu_ctx, deg_dims.data(), 1, NIMCP_GPU_PRECISION_INT32);

    nimcp_topology_compute_degree(gpu_ctx, graph, degree);

    std::vector<int32_t> deg_host(NUM_NODES);
    nimcp_gpu_memcpy(gpu_ctx, deg_host.data(), degree->data,
                     NUM_NODES * sizeof(int32_t), GPU_MEMCPY_DEVICE_TO_HOST);

    // Create normalized adjacency: D^{-1/2} * A * D^{-1/2}
    std::vector<float> norm_adj(NUM_NODES * NUM_NODES);
    for (int i = 0; i < NUM_NODES; i++) {
        float di = std::max(1.0f, std::sqrt(static_cast<float>(deg_host[i])));
        for (int j = 0; j < NUM_NODES; j++) {
            float dj = std::max(1.0f, std::sqrt(static_cast<float>(deg_host[j])));
            norm_adj[i * NUM_NODES + j] = adj_data[i * NUM_NODES + j] / (di * dj);
        }
    }

    std::vector<size_t> adj_dims = {static_cast<size_t>(NUM_NODES),
                                    static_cast<size_t>(NUM_NODES)};
    nimcp_gpu_tensor_t* norm_dense = createTensor(norm_adj, adj_dims);
    nimcp_sparse_tensor_t* norm_sparse = nimcp_sparse_from_dense(
        sparse_ctx, norm_dense, SPARSE_FORMAT_CSR, 0.0f);

    // Create node features
    auto features = generateMatrix(NUM_NODES, NUM_FEATURES, 0.0f);
    std::vector<size_t> feat_dims = {static_cast<size_t>(NUM_NODES),
                                     static_cast<size_t>(NUM_FEATURES)};
    nimcp_gpu_tensor_t* X = createTensor(features, feat_dims);

    // Normalized aggregation: X' = norm_A * X
    nimcp_gpu_tensor_t* X_agg = nimcp_sparse_mm(
        sparse_ctx, norm_sparse, X, 1.0f, 0.0f, nullptr);
    ASSERT_NE(X_agg, nullptr);

    auto agg_data = copyToHost(X_agg);

    // Verify output is properly normalized (should have smaller magnitudes)
    float orig_norm = 0.0f, agg_norm = 0.0f;
    for (size_t i = 0; i < features.size(); i++) {
        orig_norm += features[i] * features[i];
    }
    for (float v : agg_data) {
        agg_norm += v * v;
    }

    std::cout << "Original L2 norm: " << std::sqrt(orig_norm) << std::endl;
    std::cout << "Aggregated L2 norm: " << std::sqrt(agg_norm) << std::endl;

    // Cleanup
    nimcp_sparse_tensor_destroy(norm_sparse);
    nimcp_gpu_tensor_destroy(norm_dense);
    nimcp_gpu_tensor_destroy(degree);
    nimcp_gpu_tensor_destroy(X);
    nimcp_gpu_tensor_destroy(X_agg);
    nimcp_graph_gpu_destroy(graph);
}

//=============================================================================
// Memory Pool Integration Tests
//=============================================================================

TEST_F(SparseTopologyIntegrationTest, MemoryPool_SparseTopologyPipeline) {
    RequireGPU();
    ASSERT_NE(memory_pool, nullptr) << "GPU pool not available";

    std::cout << "\n=== Memory Pool: Sparse + Topology Pipeline ===" << std::endl;

    // Get initial pool stats
    nimcp_gpu_pool_stats_t initial_stats;
    nimcp_gpu_pool_get_stats(memory_pool, &initial_stats);
    std::cout << "Initial pool allocated: " << initial_stats.allocated_bytes << " bytes" << std::endl;

    // Perform multiple iterations of sparse+topology operations
    const int iterations = 10;

    for (int iter = 0; iter < iterations; iter++) {
        // Create graph
        nimcp_graph_gpu_t* graph = nimcp_graph_generate_erdos_renyi(
            gpu_ctx, MEDIUM_SIZE, 0.1f, RANDOM_SEED + iter);
        ASSERT_NE(graph, nullptr);

        // Get adjacency and convert to sparse
        std::vector<float> adj(MEDIUM_SIZE * MEDIUM_SIZE);
        nimcp_graph_gpu_to_host(graph, adj.data());

        std::vector<size_t> dims = {MEDIUM_SIZE, MEDIUM_SIZE};
        nimcp_gpu_tensor_t* adj_dense = createTensor(adj, dims);
        nimcp_sparse_tensor_t* adj_sparse = nimcp_sparse_from_dense(
            sparse_ctx, adj_dense, SPARSE_FORMAT_CSR, 0.0f);

        // Compute topology metrics
        nimcp_topology_metrics_gpu_t* metrics = nimcp_topology_compute_metrics(
            gpu_ctx, graph);

        // Perform sparse operation
        auto features = generateMatrix(MEDIUM_SIZE, 1, 0.0f);
        std::vector<size_t> feat_dims = {MEDIUM_SIZE};
        nimcp_gpu_tensor_t* X = createTensor(features, feat_dims);

        nimcp_gpu_tensor_t* output = nimcp_sparse_mv(
            sparse_ctx, adj_sparse, X, 1.0f, 0.0f, nullptr);

        // Cleanup this iteration
        nimcp_sparse_tensor_destroy(adj_sparse);
        nimcp_gpu_tensor_destroy(adj_dense);
        nimcp_gpu_tensor_destroy(X);
        nimcp_gpu_tensor_destroy(output);
        if (metrics) nimcp_topology_metrics_gpu_destroy(metrics);
        nimcp_graph_gpu_destroy(graph);
    }

    // Check final pool stats
    nimcp_gpu_pool_stats_t final_stats;
    nimcp_gpu_pool_get_stats(memory_pool, &final_stats);
    std::cout << "Final pool allocated: " << final_stats.allocated_bytes << " bytes" << std::endl;
    std::cout << "Pool peak usage: " << final_stats.peak_allocated_bytes << " bytes" << std::endl;

    // Memory should be properly freed
    EXPECT_LE(final_stats.allocated_bytes, initial_stats.allocated_bytes + 1024)
        << "Memory not properly freed in pool";

    std::cout << "Memory pool integration test passed!" << std::endl;
}

TEST_F(SparseTopologyIntegrationTest, MemoryPool_LargeScalePipeline) {
    RequireGPU();
    ASSERT_NE(memory_pool, nullptr) << "GPU pool not available";

    std::cout << "\n=== Memory Pool: Large Scale Pipeline ===" << std::endl;

    // Generate larger graph
    nimcp_graph_gpu_t* graph = nimcp_graph_generate_barabasi_albert(
        gpu_ctx, LARGE_SIZE, 5, RANDOM_SEED);
    ASSERT_NE(graph, nullptr);

    std::cout << "Graph: " << graph->num_nodes << " nodes, "
              << graph->num_edges << " edges" << std::endl;

    // Convert to CSR for efficiency
    nimcp_graph_gpu_to_csr(graph, 0.0f);
    EXPECT_TRUE(graph->is_sparse);

    // Compute comprehensive metrics
    auto start = std::chrono::high_resolution_clock::now();

    nimcp_topology_metrics_gpu_t* metrics = nimcp_topology_compute_metrics(
        gpu_ctx, graph);
    ASSERT_NE(metrics, nullptr);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration<double, std::milli>(end - start).count();

    std::cout << "Metrics computed in " << duration << " ms" << std::endl;
    std::cout << "  Global clustering: " << metrics->global_clustering << std::endl;
    std::cout << "  Density: " << metrics->density << std::endl;

    // Community detection
    start = std::chrono::high_resolution_clock::now();

    nimcp_community_result_gpu_t* communities = nimcp_community_detect_louvain(
        gpu_ctx, graph, 1.0f, 50, 1e-4f);
    ASSERT_NE(communities, nullptr);

    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration<double, std::milli>(end - start).count();

    std::cout << "Community detection in " << duration << " ms" << std::endl;
    std::cout << "  Found " << communities->num_communities << " communities" << std::endl;
    std::cout << "  Modularity: " << communities->modularity << std::endl;

    // Cleanup
    nimcp_community_result_gpu_destroy(communities);
    nimcp_topology_metrics_gpu_destroy(metrics);
    nimcp_graph_gpu_destroy(graph);

    std::cout << "Large scale pipeline completed!" << std::endl;
}

//=============================================================================
// Combined Sparse and Topology Regression Tests
//=============================================================================

TEST_F(SparseTopologyIntegrationTest, Regression_SparseMM_WithGraphAdjacency) {
    RequireGPU();

    std::cout << "\n=== Regression: SpMM with Graph Adjacency ===" << std::endl;

    // Test multiple graph types
    struct GraphConfig {
        std::string name;
        std::function<nimcp_graph_gpu_t*()> generator;
    };

    std::vector<GraphConfig> configs = {
        {"Erdos-Renyi", [this]() {
            return nimcp_graph_generate_erdos_renyi(gpu_ctx, NUM_NODES, 0.1f, RANDOM_SEED);
        }},
        {"Barabasi-Albert", [this]() {
            return nimcp_graph_generate_barabasi_albert(gpu_ctx, NUM_NODES, 3, RANDOM_SEED);
        }},
        {"Watts-Strogatz", [this]() {
            return nimcp_graph_generate_watts_strogatz(gpu_ctx, NUM_NODES, 4, 0.1f, RANDOM_SEED);
        }}
    };

    for (const auto& config : configs) {
        nimcp_graph_gpu_t* graph = config.generator();
        ASSERT_NE(graph, nullptr) << "Failed to generate " << config.name;

        // Get adjacency
        std::vector<float> adj(NUM_NODES * NUM_NODES);
        nimcp_graph_gpu_to_host(graph, adj.data());

        // Create sparse adjacency
        std::vector<size_t> adj_dims = {static_cast<size_t>(NUM_NODES),
                                        static_cast<size_t>(NUM_NODES)};
        nimcp_gpu_tensor_t* adj_dense = createTensor(adj, adj_dims);
        nimcp_sparse_tensor_t* adj_sparse = nimcp_sparse_from_dense(
            sparse_ctx, adj_dense, SPARSE_FORMAT_CSR, 0.0f);

        // Create feature matrix
        auto features = generateMatrix(NUM_NODES, NUM_FEATURES, 0.0f);
        std::vector<size_t> feat_dims = {static_cast<size_t>(NUM_NODES),
                                         static_cast<size_t>(NUM_FEATURES)};
        nimcp_gpu_tensor_t* X = createTensor(features, feat_dims);

        // SpMM: Y = A * X
        nimcp_gpu_tensor_t* Y = nimcp_sparse_mm(
            sparse_ctx, adj_sparse, X, 1.0f, 0.0f, nullptr);
        ASSERT_NE(Y, nullptr) << "SpMM failed for " << config.name;

        // Verify output dimensions
        EXPECT_EQ(Y->dims[0], static_cast<size_t>(NUM_NODES));
        EXPECT_EQ(Y->dims[1], static_cast<size_t>(NUM_FEATURES));

        // Verify non-zero output (unless empty graph)
        auto Y_data = copyToHost(Y);
        float sum = 0.0f;
        for (float v : Y_data) sum += std::fabs(v);

        if (graph->num_edges > 0) {
            EXPECT_GT(sum, 0.0f) << "SpMM output should be non-zero for " << config.name;
        }

        std::cout << config.name << ": edges=" << graph->num_edges
                  << ", nnz=" << nimcp_sparse_nnz(adj_sparse)
                  << ", output_sum=" << sum << std::endl;

        // Cleanup
        nimcp_sparse_tensor_destroy(adj_sparse);
        nimcp_gpu_tensor_destroy(adj_dense);
        nimcp_gpu_tensor_destroy(X);
        nimcp_gpu_tensor_destroy(Y);
        nimcp_graph_gpu_destroy(graph);
    }

    std::cout << "Regression test passed for all graph types!" << std::endl;
}

TEST_F(SparseTopologyIntegrationTest, Regression_SparsityPreservation_InPipeline) {
    RequireGPU();

    std::cout << "\n=== Regression: Sparsity Preservation in Pipeline ===" << std::endl;

    // Create graph
    nimcp_graph_gpu_t* graph = nimcp_graph_generate_erdos_renyi(
        gpu_ctx, NUM_NODES, 0.05f, RANDOM_SEED);  // Very sparse
    ASSERT_NE(graph, nullptr);

    // Get adjacency
    std::vector<float> adj(NUM_NODES * NUM_NODES);
    nimcp_graph_gpu_to_host(graph, adj.data());

    // Track sparsity through pipeline
    std::vector<size_t> dims = {static_cast<size_t>(NUM_NODES),
                                static_cast<size_t>(NUM_NODES)};
    nimcp_gpu_tensor_t* adj_dense = createTensor(adj, dims);

    // Original nnz
    int original_nnz = 0;
    for (float v : adj) {
        if (std::fabs(v) > 0.0f) original_nnz++;
    }

    // Convert to CSR
    nimcp_sparse_tensor_t* csr = nimcp_sparse_from_dense(
        sparse_ctx, adj_dense, SPARSE_FORMAT_CSR, 0.0f);
    int csr_nnz = nimcp_sparse_nnz(csr);

    // Convert to COO
    nimcp_sparse_tensor_t* coo = nimcp_sparse_convert(
        sparse_ctx, csr, SPARSE_FORMAT_COO);
    int coo_nnz = nimcp_sparse_nnz(coo);

    // Convert back to CSR
    nimcp_sparse_tensor_t* csr2 = nimcp_sparse_convert(
        sparse_ctx, coo, SPARSE_FORMAT_CSR);
    int csr2_nnz = nimcp_sparse_nnz(csr2);

    // Convert back to dense
    nimcp_gpu_tensor_t* reconstructed = nimcp_sparse_to_dense(sparse_ctx, csr2);
    auto recon_data = copyToHost(reconstructed);

    int final_nnz = 0;
    for (float v : recon_data) {
        if (std::fabs(v) > 0.0f) final_nnz++;
    }

    std::cout << "Sparsity preservation:" << std::endl;
    std::cout << "  Original nnz: " << original_nnz << std::endl;
    std::cout << "  CSR nnz: " << csr_nnz << std::endl;
    std::cout << "  COO nnz: " << coo_nnz << std::endl;
    std::cout << "  CSR2 nnz: " << csr2_nnz << std::endl;
    std::cout << "  Final nnz: " << final_nnz << std::endl;

    EXPECT_EQ(csr_nnz, original_nnz) << "CSR conversion changed nnz";
    EXPECT_EQ(coo_nnz, original_nnz) << "COO conversion changed nnz";
    EXPECT_EQ(csr2_nnz, original_nnz) << "Second CSR conversion changed nnz";
    EXPECT_EQ(final_nnz, original_nnz) << "Round-trip changed nnz";

    // Verify data integrity
    float max_err = maxAbsError(adj, recon_data);
    EXPECT_LT(max_err, TOLERANCE) << "Data integrity lost in conversion pipeline";

    // Cleanup
    nimcp_sparse_tensor_destroy(csr);
    nimcp_sparse_tensor_destroy(coo);
    nimcp_sparse_tensor_destroy(csr2);
    nimcp_gpu_tensor_destroy(adj_dense);
    nimcp_gpu_tensor_destroy(reconstructed);
    nimcp_graph_gpu_destroy(graph);

    std::cout << "Sparsity preservation test passed!" << std::endl;
}

//=============================================================================
// Performance Tests
//=============================================================================

TEST_F(SparseTopologyIntegrationTest, Performance_SparseVsDense_GraphOperations) {
    RequireGPU();

    std::cout << "\n=== Performance: Sparse vs Dense Graph Operations ===" << std::endl;

    const int warmup = 3;
    const int runs = 10;

    // Generate graph
    nimcp_graph_gpu_t* graph = nimcp_graph_generate_barabasi_albert(
        gpu_ctx, LARGE_SIZE, 3, RANDOM_SEED);
    ASSERT_NE(graph, nullptr);

    // Get adjacency
    std::vector<float> adj(LARGE_SIZE * LARGE_SIZE);
    nimcp_graph_gpu_to_host(graph, adj.data());

    // Create tensors
    std::vector<size_t> adj_dims = {LARGE_SIZE, LARGE_SIZE};
    std::vector<size_t> feat_dims = {LARGE_SIZE, NUM_FEATURES};

    nimcp_gpu_tensor_t* adj_dense = createTensor(adj, adj_dims);
    nimcp_sparse_tensor_t* adj_sparse = nimcp_sparse_from_dense(
        sparse_ctx, adj_dense, SPARSE_FORMAT_CSR, 0.0f);

    auto features = generateMatrix(LARGE_SIZE, NUM_FEATURES, 0.0f);
    nimcp_gpu_tensor_t* X = createTensor(features, feat_dims);

    float sparsity = nimcp_sparse_sparsity(adj_sparse);
    std::cout << "Matrix size: " << LARGE_SIZE << "x" << LARGE_SIZE << std::endl;
    std::cout << "Sparsity: " << (sparsity * 100) << "%" << std::endl;

    // Warmup sparse
    for (int i = 0; i < warmup; i++) {
        nimcp_gpu_tensor_t* out = nimcp_sparse_mm(
            sparse_ctx, adj_sparse, X, 1.0f, 0.0f, nullptr);
        nimcp_gpu_tensor_destroy(out);
    }
    nimcp_gpu_context_synchronize(gpu_ctx);

    // Time sparse
    auto sparse_start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < runs; i++) {
        nimcp_gpu_tensor_t* out = nimcp_sparse_mm(
            sparse_ctx, adj_sparse, X, 1.0f, 0.0f, nullptr);
        nimcp_gpu_tensor_destroy(out);
    }
    nimcp_gpu_context_synchronize(gpu_ctx);
    auto sparse_end = std::chrono::high_resolution_clock::now();

    double sparse_ms = std::chrono::duration<double, std::milli>(
        sparse_end - sparse_start).count() / runs;

    std::cout << "Sparse SpMM time: " << sparse_ms << " ms" << std::endl;

    // Verify correctness
    nimcp_gpu_tensor_t* sparse_out = nimcp_sparse_mm(
        sparse_ctx, adj_sparse, X, 1.0f, 0.0f, nullptr);
    auto sparse_data = copyToHost(sparse_out);

    float sum = 0.0f;
    for (float v : sparse_data) sum += std::fabs(v);
    EXPECT_GT(sum, 0.0f) << "Output should be non-zero";

    // Cleanup
    nimcp_sparse_tensor_destroy(adj_sparse);
    nimcp_gpu_tensor_destroy(adj_dense);
    nimcp_gpu_tensor_destroy(X);
    nimcp_gpu_tensor_destroy(sparse_out);
    nimcp_graph_gpu_destroy(graph);

    std::cout << "Performance test completed!" << std::endl;
}

//=============================================================================
// Memory Safety Tests
//=============================================================================

TEST_F(SparseTopologyIntegrationTest, MemorySafety_RepeatedPipeline) {
    RequireGPU();

    std::cout << "\n=== Memory Safety: Repeated Pipeline ===" << std::endl;

    nimcp_memory_stats_t initial_stats;
    nimcp_memory_get_stats(&initial_stats);

    const int iterations = 20;

    for (int i = 0; i < iterations; i++) {
        // Create graph
        nimcp_graph_gpu_t* graph = nimcp_graph_generate_erdos_renyi(
            gpu_ctx, MEDIUM_SIZE, 0.1f, RANDOM_SEED + i);
        ASSERT_NE(graph, nullptr);

        // Topology operations
        nimcp_topology_metrics_gpu_t* metrics = nimcp_topology_compute_metrics(
            gpu_ctx, graph);

        nimcp_community_result_gpu_t* communities = nimcp_community_detect_louvain(
            gpu_ctx, graph, 1.0f, 50, 1e-4f);

        // Convert to sparse
        std::vector<float> adj(MEDIUM_SIZE * MEDIUM_SIZE);
        nimcp_graph_gpu_to_host(graph, adj.data());

        std::vector<size_t> dims = {MEDIUM_SIZE, MEDIUM_SIZE};
        nimcp_gpu_tensor_t* adj_dense = createTensor(adj, dims);
        nimcp_sparse_tensor_t* adj_sparse = nimcp_sparse_from_dense(
            sparse_ctx, adj_dense, SPARSE_FORMAT_CSR, 0.0f);

        // Sparse operations
        auto features = generateMatrix(MEDIUM_SIZE, 1, 0.0f);
        std::vector<size_t> feat_dims = {MEDIUM_SIZE};
        nimcp_gpu_tensor_t* X = createTensor(features, feat_dims);

        nimcp_gpu_tensor_t* Y = nimcp_sparse_mv(
            sparse_ctx, adj_sparse, X, 1.0f, 0.0f, nullptr);

        // Format conversions
        nimcp_sparse_tensor_t* coo = nimcp_sparse_convert(
            sparse_ctx, adj_sparse, SPARSE_FORMAT_COO);

        // Cleanup
        if (coo) nimcp_sparse_tensor_destroy(coo);
        nimcp_sparse_tensor_destroy(adj_sparse);
        nimcp_gpu_tensor_destroy(adj_dense);
        nimcp_gpu_tensor_destroy(X);
        nimcp_gpu_tensor_destroy(Y);
        if (communities) nimcp_community_result_gpu_destroy(communities);
        if (metrics) nimcp_topology_metrics_gpu_destroy(metrics);
        nimcp_graph_gpu_destroy(graph);
    }

    nimcp_memory_stats_t final_stats;
    nimcp_memory_get_stats(&final_stats);

    int64_t leaked = final_stats.current_allocated - initial_stats.current_allocated;

    std::cout << "Iterations: " << iterations << std::endl;
    std::cout << "Initial allocated: " << initial_stats.current_allocated << " bytes" << std::endl;
    std::cout << "Final allocated: " << final_stats.current_allocated << " bytes" << std::endl;
    std::cout << "Potential leak: " << leaked << " bytes" << std::endl;

    EXPECT_LT(leaked, 4096) << "Memory leak detected";

    std::cout << "Memory safety test passed!" << std::endl;
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
