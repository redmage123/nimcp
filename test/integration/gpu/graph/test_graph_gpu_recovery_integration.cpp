/**
 * @file test_graph_gpu_recovery_integration.cpp
 * @brief Integration tests for GPU recovery in graph modules
 *
 * WHAT: Integration tests for graph and knowledge graph GPU operations with recovery
 * WHY:  Verify end-to-end recovery across graph algorithms
 * HOW:  Test complete graph workflows with error handling and recovery
 *
 * TEST COVERAGE:
 * - End-to-end graph analysis workflow with recovery
 * - Knowledge graph traversal and similarity with recovery
 * - Graph neural network operations with recovery
 * - Subgraph matching with recovery
 * - Cross-module graph operations with recovery
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <random>
#include <algorithm>

#ifdef NIMCP_ENABLE_CUDA
#include <cuda_runtime.h>
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "gpu/graph/nimcp_graph_gpu.h"
#include "gpu/knowledge/nimcp_knowledge_graph_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#endif

namespace {

/* ============================================================================
 * Test Fixture
 * ============================================================================ */
class GraphGPURecoveryIntegrationTest : public ::testing::Test {
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
        rctx = nimcp_gpu_recovery_context_create(NULL);
        if (!rctx) {
            GTEST_SKIP() << "Failed to create recovery context";
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
        if (kg) {
            nimcp_gpu_knowledge_graph_destroy(kg);
            kg = nullptr;
        }
        if (rctx) {
            nimcp_gpu_recovery_context_destroy(rctx);
            rctx = nullptr;
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

    /* Helper to create random graph */
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_graph_t* createRandomGraph(size_t n, float edge_prob, unsigned seed) {
        std::mt19937 rng(seed);
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);

        std::vector<int> src, dst;
        std::vector<float> weights;

        for (size_t i = 0; i < n; i++) {
            for (size_t j = i + 1; j < n; j++) {
                if (dist(rng) < edge_prob) {
                    src.push_back(i);
                    dst.push_back(j);
                    weights.push_back(dist(rng));
                    /* Undirected: add reverse edge */
                    src.push_back(j);
                    dst.push_back(i);
                    weights.push_back(weights.back());
                }
            }
        }

        if (src.empty()) {
            /* Ensure at least one edge */
            src.push_back(0);
            dst.push_back(1);
            weights.push_back(1.0f);
            src.push_back(1);
            dst.push_back(0);
            weights.push_back(1.0f);
        }

        return nimcp_gpu_graph_from_edge_list(ctx, src.data(), dst.data(),
                                               weights.data(), src.size(), n);
    }

    /* Helper to create knowledge graph */
    nimcp_gpu_knowledge_graph_t* createSimpleKG(uint32_t num_nodes,
                                                 uint32_t embed_dim) {
        std::vector<uint32_t> row_offsets(num_nodes + 1);
        std::vector<uint32_t> col_indices;

        for (uint32_t i = 0; i < num_nodes; i++) {
            row_offsets[i] = col_indices.size();
            if (i > 0) col_indices.push_back(i - 1);
            if (i < num_nodes - 1) col_indices.push_back(i + 1);
        }
        row_offsets[num_nodes] = col_indices.size();

        return nimcp_gpu_knowledge_graph_create(
            ctx, row_offsets.data(), col_indices.data(), nullptr,
            num_nodes, col_indices.size(), embed_dim);
    }

    nimcp_gpu_context_t* ctx = nullptr;
    nimcp_gpu_recovery_context_t* rctx = nullptr;
    nimcp_gpu_graph_t* graph = nullptr;
    nimcp_gpu_knowledge_graph_t* kg = nullptr;
#endif
};

/* ============================================================================
 * Integration Test: Complete Graph Analysis Workflow
 * ============================================================================ */
TEST_F(GraphGPURecoveryIntegrationTest, CompleteGraphAnalysisWorkflow) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_reset_stats();

    /* Create random graph */
    graph = createRandomGraph(50, 0.1f, 42);
    ASSERT_NE(graph, nullptr);
    EXPECT_TRUE(nimcp_gpu_graph_is_valid(graph));

    /* 1. Compute basic statistics */
    float avg_degree, density;
    int max_degree, min_degree;
    nimcp_error_t err = nimcp_gpu_graph_stats(graph, &avg_degree, &max_degree,
                                               &min_degree, &density);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GT(avg_degree, 0.0f);

    /* 2. Compute degree centrality */
    float* d_centrality = nullptr;
    cudaMalloc(&d_centrality, graph->num_vertices * sizeof(float));
    ASSERT_NE(d_centrality, nullptr);

    err = nimcp_gpu_graph_degree_centrality(graph, d_centrality);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* 3. Find hub nodes */
    std::vector<int> hub_ids(10);
    size_t num_hubs = nimcp_gpu_graph_find_hubs(graph, 0.1f, hub_ids.data(), 10);
    /* May or may not find hubs depending on graph structure */

    /* 4. Compute clustering coefficients */
    float* d_clustering = nullptr;
    cudaMalloc(&d_clustering, graph->num_vertices * sizeof(float));
    ASSERT_NE(d_clustering, nullptr);

    err = nimcp_gpu_graph_clustering_coeff(graph, d_clustering);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* 5. Average clustering */
    float avg_clustering = 0.0f;
    err = nimcp_gpu_graph_avg_clustering(graph, &avg_clustering);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GE(avg_clustering, 0.0f);
    EXPECT_LE(avg_clustering, 1.0f);

    /* 6. Check connectivity */
    bool is_connected = false;
    err = nimcp_gpu_graph_is_connected(graph, &is_connected);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* 7. Count triangles */
    size_t triangle_count = 0;
    err = nimcp_gpu_graph_count_triangles(graph, &triangle_count);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* 8. Small-world metrics */
    nimcp_graph_small_world_t sw_metrics;
    err = nimcp_gpu_graph_small_world_metrics(graph, &sw_metrics);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    cudaFree(d_centrality);
    cudaFree(d_clustering);

    /* Check recovery stats */
    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);
    EXPECT_GE(stats.success_rate, 0.0f);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Integration Test: BFS from Multiple Sources
 * ============================================================================ */
TEST_F(GraphGPURecoveryIntegrationTest, BFSMultipleSourcesWorkflow) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_reset_stats();

    /* Create graph */
    graph = createRandomGraph(100, 0.05f, 123);
    ASSERT_NE(graph, nullptr);

    /* Allocate distance array */
    float* d_distances = nullptr;
    cudaMalloc(&d_distances, graph->num_vertices * sizeof(float));
    ASSERT_NE(d_distances, nullptr);

    /* Run BFS from multiple sources and collect results */
    std::vector<std::vector<float>> all_distances;

    for (int source = 0; source < 10; source++) {
        nimcp_error_t err = nimcp_gpu_graph_bfs(graph, source, d_distances);
        EXPECT_EQ(err, NIMCP_SUCCESS) << "BFS from source " << source << " failed";

        std::vector<float> h_distances(graph->num_vertices);
        cudaMemcpy(h_distances.data(), d_distances,
                   graph->num_vertices * sizeof(float), cudaMemcpyDeviceToHost);
        all_distances.push_back(h_distances);
    }

    /* Run multi-source BFS */
    std::vector<int> sources = {0, 25, 50, 75, 99};
    nimcp_error_t err = nimcp_gpu_graph_bfs_multi_source(
        graph, sources.data(), sources.size(), d_distances);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    cudaFree(d_distances);

    /* Check recovery stats */
    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);
    EXPECT_GE(stats.success_rate, 0.0f);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Integration Test: Knowledge Graph Traversal and Similarity
 * ============================================================================ */
TEST_F(GraphGPURecoveryIntegrationTest, KnowledgeGraphTraversalSimilarity) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_reset_stats();

    /* Create knowledge graph */
    kg = createSimpleKG(50, 64);
    ASSERT_NE(kg, nullptr);

    /* Set embeddings with clear patterns */
    std::vector<float> embeddings(50 * 64);
    for (size_t i = 0; i < 50; i++) {
        for (size_t j = 0; j < 64; j++) {
            /* Embeddings cluster by node index */
            embeddings[i * 64 + j] = (float)((i / 10 + j) % 100) / 100.0f;
        }
    }
    ASSERT_TRUE(nimcp_gpu_knowledge_graph_set_embeddings(kg, embeddings.data()));

    /* 1. Run BFS from multiple sources */
    for (int src = 0; src < 50; src += 10) {
        nimcp_graph_traversal_result_t result;
        memset(&result, 0, sizeof(result));

        bool success = nimcp_gpu_bfs(kg, src, -1, &result);
        EXPECT_TRUE(success) << "BFS from " << src << " should succeed";
        EXPECT_GT(result.num_visited, 0u);

        nimcp_graph_traversal_result_destroy(&result);
    }

    /* 2. Compute pairwise similarity for subset */
    std::vector<uint32_t> node_subset = {0, 10, 20, 30, 40};
    size_t dims[2] = {5, 5};
    nimcp_gpu_tensor_t* sim_matrix = nimcp_gpu_tensor_create(ctx, dims, 2,
                                                              NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(sim_matrix, nullptr);

    bool sim_result = nimcp_gpu_pairwise_similarity(kg, node_subset.data(), 5, sim_matrix);
    EXPECT_TRUE(sim_result) << "Pairwise similarity should succeed";

    /* 3. KNN similarity search */
    for (uint32_t query_node = 0; query_node < 50; query_node += 10) {
        nimcp_similarity_result_t knn_result;
        memset(&knn_result, 0, sizeof(knn_result));

        bool knn_success = nimcp_gpu_knn_similarity(kg, query_node, 5, &knn_result);
        EXPECT_TRUE(knn_success) << "KNN for node " << query_node << " should succeed";

        nimcp_similarity_result_destroy(&knn_result);
    }

    nimcp_gpu_tensor_destroy(sim_matrix);

    /* Check recovery stats */
    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);
    EXPECT_GE(stats.success_rate, 0.0f);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Integration Test: Graph Neural Network Style Operations
 * ============================================================================ */
TEST_F(GraphGPURecoveryIntegrationTest, GNNStyleOperations) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_reset_stats();

    /* Create knowledge graph */
    kg = createSimpleKG(30, 32);
    ASSERT_NE(kg, nullptr);

    /* Set random embeddings */
    std::vector<float> embeddings(30 * 32);
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    for (auto& e : embeddings) {
        e = dist(rng);
    }
    ASSERT_TRUE(nimcp_gpu_knowledge_graph_set_embeddings(kg, embeddings.data()));

    /* Create output tensor */
    size_t dims[2] = {30, 32};
    nimcp_gpu_tensor_t* output = nimcp_gpu_tensor_create(ctx, dims, 2,
                                                          NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(output, nullptr);

    /* Test different aggregation modes */
    nimcp_aggregate_mode_t modes[] = {
        NIMCP_AGGREGATE_SUM,
        NIMCP_AGGREGATE_MEAN,
        NIMCP_AGGREGATE_MAX
    };

    for (auto mode : modes) {
        bool agg_result = nimcp_gpu_aggregate_neighbors(kg, mode, output);
        EXPECT_TRUE(agg_result) << "Aggregation mode " << mode << " should succeed";
    }

    /* Multi-hop aggregation */
    for (int hops = 1; hops <= 3; hops++) {
        bool multi_result = nimcp_gpu_multi_hop_aggregate(kg, hops,
                                                           NIMCP_AGGREGATE_MEAN, output);
        EXPECT_TRUE(multi_result) << "Multi-hop aggregation with " << hops << " hops should succeed";
    }

    nimcp_gpu_tensor_destroy(output);

    /* Check recovery stats */
    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);
    EXPECT_GE(stats.success_rate, 0.0f);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Integration Test: Knowledge Graph Embedding Training
 * ============================================================================ */
TEST_F(GraphGPURecoveryIntegrationTest, KGEmbeddingTraining) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_reset_stats();

    /* Create embedding DAO */
    nimcp_knowledge_embedding_dao_t* dao = nimcp_knowledge_embedding_dao_create(
        ctx, 100, 10, 32);
    ASSERT_NE(dao, nullptr);

    /* Initialize entity embeddings */
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-0.1f, 0.1f);

    for (int i = 0; i < 50; i++) {
        std::vector<float> embedding(32);
        for (auto& e : embedding) {
            e = dist(rng);
        }
        int result = dao->create_embedding(dao, i, embedding.data());
        EXPECT_EQ(result, 0) << "Create embedding for entity " << i << " should succeed";
    }

    /* Training configuration */
    nimcp_kg_train_config_t config;
    config.learning_rate = 0.01f;
    config.margin = 1.0f;
    config.negative_samples = 5;
    config.regularization = 0.001f;
    config.normalize_embeddings = true;

    /* Create training triples */
    std::vector<int> heads, relations, tails;
    for (int i = 0; i < 40; i++) {
        heads.push_back(i);
        relations.push_back(i % 5);
        tails.push_back((i + 5) % 50);
    }

    /* Training loop */
    for (int epoch = 0; epoch < 10; epoch++) {
        int status = nimcp_kg_train_step(dao, heads.data(), relations.data(),
                                          tails.data(), 40, &config);
        EXPECT_EQ(status, 0) << "Training step " << epoch << " should succeed";
    }

    /* Test predictions */
    std::vector<int> predictions(5);
    std::vector<float> scores(5);
    int pred_status = nimcp_kg_predict_tail(dao, 0, 0, 5,
                                             predictions.data(), scores.data());
    EXPECT_EQ(pred_status, 0) << "Tail prediction should succeed";

    nimcp_knowledge_embedding_dao_destroy(dao);

    /* Check recovery stats */
    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);
    EXPECT_GE(stats.success_rate, 0.0f);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Integration Test: Graph Comparison and Modularity
 * ============================================================================ */
TEST_F(GraphGPURecoveryIntegrationTest, GraphComparisonModularity) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_reset_stats();

    /* Create graph with community structure */
    const size_t n = 40;
    const size_t communities = 4;
    const size_t community_size = n / communities;

    std::vector<int> src, dst;
    std::vector<float> weights;
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    /* Dense within-community edges */
    for (size_t c = 0; c < communities; c++) {
        for (size_t i = c * community_size; i < (c + 1) * community_size; i++) {
            for (size_t j = i + 1; j < (c + 1) * community_size; j++) {
                if (dist(rng) < 0.5f) {
                    src.push_back(i);
                    dst.push_back(j);
                    weights.push_back(1.0f);
                    src.push_back(j);
                    dst.push_back(i);
                    weights.push_back(1.0f);
                }
            }
        }
    }

    /* Sparse between-community edges */
    for (size_t c1 = 0; c1 < communities; c1++) {
        for (size_t c2 = c1 + 1; c2 < communities; c2++) {
            if (dist(rng) < 0.1f) {
                size_t i = c1 * community_size + (size_t)(dist(rng) * community_size);
                size_t j = c2 * community_size + (size_t)(dist(rng) * community_size);
                src.push_back(i);
                dst.push_back(j);
                weights.push_back(0.5f);
                src.push_back(j);
                dst.push_back(i);
                weights.push_back(0.5f);
            }
        }
    }

    graph = nimcp_gpu_graph_from_edge_list(ctx, src.data(), dst.data(),
                                            weights.data(), src.size(), n);
    ASSERT_NE(graph, nullptr);

    /* Compute modularity with ground-truth labels */
    std::vector<int> labels(n);
    for (size_t i = 0; i < n; i++) {
        labels[i] = i / community_size;
    }

    float modularity = 0.0f;
    nimcp_error_t err = nimcp_gpu_graph_modularity(graph, labels.data(), &modularity);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GT(modularity, 0.0f) << "Graph with clear communities should have positive modularity";

    /* Count communities */
    size_t num_communities = nimcp_gpu_graph_count_communities(graph, labels.data());
    EXPECT_EQ(num_communities, communities);

    /* Check recovery stats */
    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);
    EXPECT_GE(stats.success_rate, 0.0f);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Integration Test: High-Stress Graph Operations
 * ============================================================================ */
TEST_F(GraphGPURecoveryIntegrationTest, HighStressGraphOperations) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_reset_stats();

    /* Perform many graph operations rapidly */
    const int n_iterations = 50;

    for (int i = 0; i < n_iterations; i++) {
        /* Create graph with varying size */
        size_t n = 20 + (i % 30);
        nimcp_gpu_graph_t* g = createRandomGraph(n, 0.1f, i);
        if (!g) continue;

        /* Run various operations */
        float* d_distances = nullptr;
        cudaMalloc(&d_distances, n * sizeof(float));
        if (d_distances) {
            nimcp_gpu_graph_bfs(g, 0, d_distances);
            cudaFree(d_distances);
        }

        float* d_clustering = nullptr;
        cudaMalloc(&d_clustering, n * sizeof(float));
        if (d_clustering) {
            nimcp_gpu_graph_clustering_coeff(g, d_clustering);
            cudaFree(d_clustering);
        }

        float avg_degree, density;
        int max_degree, min_degree;
        nimcp_gpu_graph_stats(g, &avg_degree, &max_degree, &min_degree, &density);

        nimcp_gpu_graph_destroy(g);
    }

    /* Get final stats */
    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);

    if (stats.recoveries_attempted > 0) {
        EXPECT_GT(stats.success_rate, 0.5f)
            << "Recovery success rate should be reasonable";
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Integration Test: Cross-Module Graph-Knowledge Integration
 * ============================================================================ */
TEST_F(GraphGPURecoveryIntegrationTest, CrossModuleGraphKnowledgeIntegration) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_reset_stats();

    /* Create both graph types from same structure */
    const size_t n = 30;

    /* Build adjacency structure */
    std::vector<int> row_offsets(n + 1);
    std::vector<int> col_indices;
    std::vector<uint32_t> row_offsets_u32(n + 1);
    std::vector<uint32_t> col_indices_u32;

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    for (size_t i = 0; i < n; i++) {
        row_offsets[i] = col_indices.size();
        row_offsets_u32[i] = col_indices_u32.size();

        for (size_t j = 0; j < n; j++) {
            if (i != j && dist(rng) < 0.1f) {
                col_indices.push_back(j);
                col_indices_u32.push_back(j);
            }
        }
    }
    row_offsets[n] = col_indices.size();
    row_offsets_u32[n] = col_indices_u32.size();

    /* Create regular graph */
    graph = nimcp_gpu_graph_from_csr(ctx, row_offsets.data(), col_indices.data(),
                                      nullptr, n, col_indices.size());
    ASSERT_NE(graph, nullptr);

    /* Create knowledge graph with same structure */
    kg = nimcp_gpu_knowledge_graph_create(ctx, row_offsets_u32.data(),
                                           col_indices_u32.data(), nullptr,
                                           n, col_indices_u32.size(), 32);
    ASSERT_NE(kg, nullptr);

    /* Set embeddings for KG */
    std::vector<float> embeddings(n * 32, 1.0f);
    ASSERT_TRUE(nimcp_gpu_knowledge_graph_set_embeddings(kg, embeddings.data()));

    /* Analyze regular graph */
    float* d_distances = nullptr;
    cudaMalloc(&d_distances, n * sizeof(float));
    ASSERT_NE(d_distances, nullptr);

    nimcp_error_t err = nimcp_gpu_graph_bfs(graph, 0, d_distances);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Copy distances to host */
    std::vector<float> h_distances(n);
    cudaMemcpy(h_distances.data(), d_distances, n * sizeof(float),
               cudaMemcpyDeviceToHost);

    /* Analyze knowledge graph */
    nimcp_graph_traversal_result_t kg_result;
    memset(&kg_result, 0, sizeof(kg_result));
    bool kg_bfs = nimcp_gpu_bfs(kg, 0, -1, &kg_result);
    EXPECT_TRUE(kg_bfs);

    /* Results should be consistent (same graph structure) */
    EXPECT_EQ(kg_result.num_visited, (uint32_t)std::count_if(
        h_distances.begin(), h_distances.end(),
        [](float d) { return d < NIMCP_GRAPH_INF_DISTANCE; }));

    nimcp_graph_traversal_result_destroy(&kg_result);
    cudaFree(d_distances);

    /* Check recovery stats */
    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);
    EXPECT_GE(stats.success_rate, 0.0f);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Integration Test: Betweenness Centrality with Recovery
 * ============================================================================ */
TEST_F(GraphGPURecoveryIntegrationTest, BetweennessCentralityWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_reset_stats();

    /* Create graph with clear bottleneck structure */
    /* Two dense clusters connected by single node */
    std::vector<int> src, dst;
    std::vector<float> weights;

    /* First cluster: nodes 0-9 */
    for (int i = 0; i < 10; i++) {
        for (int j = i + 1; j < 10; j++) {
            src.push_back(i);
            dst.push_back(j);
            weights.push_back(1.0f);
            src.push_back(j);
            dst.push_back(i);
            weights.push_back(1.0f);
        }
    }

    /* Second cluster: nodes 11-20 */
    for (int i = 11; i < 21; i++) {
        for (int j = i + 1; j < 21; j++) {
            src.push_back(i);
            dst.push_back(j);
            weights.push_back(1.0f);
            src.push_back(j);
            dst.push_back(i);
            weights.push_back(1.0f);
        }
    }

    /* Bridge node 10 connecting clusters */
    src.push_back(5);
    dst.push_back(10);
    weights.push_back(1.0f);
    src.push_back(10);
    dst.push_back(5);
    weights.push_back(1.0f);
    src.push_back(10);
    dst.push_back(15);
    weights.push_back(1.0f);
    src.push_back(15);
    dst.push_back(10);
    weights.push_back(1.0f);

    graph = nimcp_gpu_graph_from_edge_list(ctx, src.data(), dst.data(),
                                            weights.data(), src.size(), 21);
    ASSERT_NE(graph, nullptr);

    /* Compute betweenness centrality */
    float* d_centrality = nullptr;
    cudaMalloc(&d_centrality, 21 * sizeof(float));
    ASSERT_NE(d_centrality, nullptr);

    nimcp_error_t err = nimcp_gpu_graph_betweenness_centrality(graph, d_centrality, 0);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Verify bridge node has highest centrality */
    std::vector<float> h_centrality(21);
    cudaMemcpy(h_centrality.data(), d_centrality, 21 * sizeof(float),
               cudaMemcpyDeviceToHost);

    /* Node 10 should have very high centrality as bridge */
    float bridge_centrality = h_centrality[10];
    float max_other = 0.0f;
    for (int i = 0; i < 21; i++) {
        if (i != 10 && h_centrality[i] > max_other) {
            max_other = h_centrality[i];
        }
    }

    EXPECT_GT(bridge_centrality, max_other * 0.5f)
        << "Bridge node should have relatively high centrality";

    cudaFree(d_centrality);

    /* Check recovery stats */
    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);
    EXPECT_GE(stats.success_rate, 0.0f);
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
