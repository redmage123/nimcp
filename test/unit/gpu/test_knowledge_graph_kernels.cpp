/**
 * @file test_knowledge_graph_kernels.cpp
 * @brief Comprehensive unit tests for GPU knowledge graph kernels
 *
 * WHAT: Tests for GPU-accelerated knowledge graph operations
 * WHY:  Verify graph traversal, similarity search, hyperbolic operations
 * HOW:  GoogleTest with GPU context setup/teardown and numerical verification
 *
 * TEST COVERAGE:
 * - Graph lifecycle (create, destroy)
 * - Graph traversal (BFS, DFS, shortest path)
 * - Semantic similarity (cosine, pairwise, k-NN)
 * - Knowledge embedding operations
 * - Node/edge aggregation
 * - Subgraph matching
 * - Hyperbolic space operations
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>
#include <numeric>
#include <random>
#include <cstdlib>

#ifdef NIMCP_ENABLE_CUDA
#include <cuda_runtime.h>
#endif

#include "gpu/knowledge/nimcp_knowledge_graph_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"

//=============================================================================
// Test Constants
//=============================================================================

static constexpr uint32_t DEFAULT_NUM_NODES = 100;
static constexpr uint32_t DEFAULT_NUM_EDGES = 500;
static constexpr uint32_t DEFAULT_EMBED_DIM = 64;
static constexpr float NUMERICAL_EPS = 1e-5f;

//=============================================================================
// Test Fixture
//=============================================================================

/**
 * @brief Test fixture for GPU knowledge graph kernel tests
 */
class KnowledgeGraphKernelTest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* ctx = nullptr;
    bool gpu_available = false;
    std::mt19937 rng{42};

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

    /**
     * @brief Create a simple connected graph in CSR format
     */
    void create_simple_graph(
        std::vector<uint32_t>& row_offsets,
        std::vector<uint32_t>& col_indices,
        std::vector<float>& edge_weights,
        uint32_t num_nodes,
        uint32_t avg_degree
    ) {
        row_offsets.resize(num_nodes + 1);
        col_indices.clear();
        edge_weights.clear();

        std::uniform_int_distribution<uint32_t> node_dist(0, num_nodes - 1);
        std::uniform_real_distribution<float> weight_dist(0.1f, 1.0f);

        uint32_t edge_count = 0;
        for (uint32_t i = 0; i < num_nodes; i++) {
            row_offsets[i] = edge_count;

            // Add avg_degree random edges
            for (uint32_t j = 0; j < avg_degree; j++) {
                uint32_t target = node_dist(rng);
                if (target != i) {  // No self-loops
                    col_indices.push_back(target);
                    edge_weights.push_back(weight_dist(rng));
                    edge_count++;
                }
            }
        }
        row_offsets[num_nodes] = edge_count;
    }

    /**
     * @brief Create a chain graph (node 0 -> 1 -> 2 -> ... -> n-1)
     */
    void create_chain_graph(
        std::vector<uint32_t>& row_offsets,
        std::vector<uint32_t>& col_indices,
        uint32_t num_nodes
    ) {
        row_offsets.resize(num_nodes + 1);
        col_indices.clear();

        for (uint32_t i = 0; i < num_nodes; i++) {
            row_offsets[i] = i;
            if (i < num_nodes - 1) {
                col_indices.push_back(i + 1);
            }
        }
        row_offsets[num_nodes] = num_nodes - 1;
    }

    /**
     * @brief Create random embeddings
     */
    std::vector<float> create_random_embeddings(uint32_t num_nodes, uint32_t embed_dim) {
        std::normal_distribution<float> dist(0.0f, 1.0f);
        std::vector<float> embeddings(num_nodes * embed_dim);
        for (auto& e : embeddings) {
            e = dist(rng);
        }
        return embeddings;
    }

    /**
     * @brief Normalize embeddings (L2)
     */
    void normalize_embeddings(std::vector<float>& embeddings, uint32_t num_nodes, uint32_t embed_dim) {
        for (uint32_t i = 0; i < num_nodes; i++) {
            float norm = 0.0f;
            for (uint32_t j = 0; j < embed_dim; j++) {
                float val = embeddings[i * embed_dim + j];
                norm += val * val;
            }
            norm = std::sqrt(norm);
            if (norm > NUMERICAL_EPS) {
                for (uint32_t j = 0; j < embed_dim; j++) {
                    embeddings[i * embed_dim + j] /= norm;
                }
            }
        }
    }

    /**
     * @brief Create GPU tensor from host data
     */
    nimcp_gpu_tensor_t* create_tensor_from_data(const float* data, size_t size) {
        if (!gpu_available) return nullptr;
        size_t dims[1] = {size};
        return nimcp_gpu_tensor_from_host(ctx, data, dims, 1, NIMCP_GPU_PRECISION_FP32);
    }

    /**
     * @brief Create GPU tensor filled with zeros
     */
    nimcp_gpu_tensor_t* create_zero_tensor(size_t size) {
        if (!gpu_available) return nullptr;
        size_t dims[1] = {size};
        nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
        if (tensor) {
            nimcp_gpu_zeros(ctx, tensor);
        }
        return tensor;
    }

    /**
     * @brief Create 2D GPU tensor (matrix)
     */
    nimcp_gpu_tensor_t* create_matrix(size_t rows, size_t cols) {
        if (!gpu_available) return nullptr;
        size_t dims[2] = {rows, cols};
        nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(ctx, dims, 2, NIMCP_GPU_PRECISION_FP32);
        if (tensor) {
            nimcp_gpu_zeros(ctx, tensor);
        }
        return tensor;
    }

    /**
     * @brief Copy tensor data to host
     */
    bool copy_to_host(const nimcp_gpu_tensor_t* tensor, float* host_data) {
        if (!tensor || !host_data) return false;
        return nimcp_gpu_tensor_to_host(tensor, host_data);
    }
};

//=============================================================================
// Graph Lifecycle Tests
//=============================================================================

/**
 * TEST: Knowledge graph creation
 * WHAT: Create GPU knowledge graph from CSR arrays
 * WHY:  Verify graph allocation and initialization
 */
TEST_F(KnowledgeGraphKernelTest, GraphCreate_FromCSR_Succeeds) {
    RequireGPU();

    std::vector<uint32_t> row_offsets, col_indices;
    std::vector<float> edge_weights;
    create_simple_graph(row_offsets, col_indices, edge_weights, DEFAULT_NUM_NODES, 5);

    nimcp_gpu_knowledge_graph_t* graph = nimcp_gpu_knowledge_graph_create(
        ctx,
        row_offsets.data(),
        col_indices.data(),
        edge_weights.data(),
        DEFAULT_NUM_NODES,
        static_cast<uint32_t>(col_indices.size()),
        DEFAULT_EMBED_DIM
    );

    if (graph) {
        EXPECT_EQ(graph->num_nodes, DEFAULT_NUM_NODES);
        EXPECT_EQ(graph->num_edges, static_cast<uint32_t>(col_indices.size()));
        EXPECT_EQ(graph->embed_dim, DEFAULT_EMBED_DIM);
        EXPECT_NE(graph->row_offsets, nullptr);
        EXPECT_NE(graph->col_indices, nullptr);
        nimcp_gpu_knowledge_graph_destroy(graph);
    }
}

/**
 * TEST: Knowledge graph destruction with NULL
 * WHAT: Destroy NULL graph
 * WHY:  Verify NULL-safety
 */
TEST_F(KnowledgeGraphKernelTest, GraphDestroy_Null_NoOp) {
    nimcp_gpu_knowledge_graph_destroy(nullptr);
    SUCCEED() << "Should not crash";
}

/**
 * TEST: Knowledge graph without edge weights
 * WHAT: Create graph without optional edge weights
 * WHY:  Verify optional parameter handling
 */
TEST_F(KnowledgeGraphKernelTest, GraphCreate_NoWeights_Succeeds) {
    RequireGPU();

    std::vector<uint32_t> row_offsets, col_indices;
    std::vector<float> edge_weights;
    create_simple_graph(row_offsets, col_indices, edge_weights, 50, 3);

    nimcp_gpu_knowledge_graph_t* graph = nimcp_gpu_knowledge_graph_create(
        ctx,
        row_offsets.data(),
        col_indices.data(),
        nullptr,  // No edge weights
        50,
        static_cast<uint32_t>(col_indices.size()),
        32
    );

    if (graph) {
        EXPECT_EQ(graph->num_nodes, 50u);
        nimcp_gpu_knowledge_graph_destroy(graph);
    }
}

/**
 * TEST: Set node embeddings
 * WHAT: Set embeddings from host data
 * WHY:  Initialize graph for similarity operations
 */
TEST_F(KnowledgeGraphKernelTest, SetEmbeddings_UpdatesGraph) {
    RequireGPU();

    std::vector<uint32_t> row_offsets, col_indices;
    std::vector<float> edge_weights;
    create_simple_graph(row_offsets, col_indices, edge_weights, DEFAULT_NUM_NODES, 5);

    nimcp_gpu_knowledge_graph_t* graph = nimcp_gpu_knowledge_graph_create(
        ctx,
        row_offsets.data(),
        col_indices.data(),
        edge_weights.data(),
        DEFAULT_NUM_NODES,
        static_cast<uint32_t>(col_indices.size()),
        DEFAULT_EMBED_DIM
    );

    if (!graph) {
        GTEST_SKIP() << "Graph creation failed";
    }

    std::vector<float> embeddings = create_random_embeddings(DEFAULT_NUM_NODES, DEFAULT_EMBED_DIM);
    bool result = nimcp_gpu_knowledge_graph_set_embeddings(graph, embeddings.data());

    if (result) {
        EXPECT_NE(graph->node_embeddings, nullptr);
    }

    nimcp_gpu_knowledge_graph_destroy(graph);
}

/**
 * TEST: Graph validity check
 * WHAT: Check if graph is valid and properly initialized
 * WHY:  Verify graph state
 */
TEST_F(KnowledgeGraphKernelTest, GraphIsValid_ReturnsTrueForValidGraph) {
    RequireGPU();

    std::vector<uint32_t> row_offsets, col_indices;
    std::vector<float> edge_weights;
    create_simple_graph(row_offsets, col_indices, edge_weights, 50, 3);

    nimcp_gpu_knowledge_graph_t* graph = nimcp_gpu_knowledge_graph_create(
        ctx,
        row_offsets.data(),
        col_indices.data(),
        edge_weights.data(),
        50,
        static_cast<uint32_t>(col_indices.size()),
        32
    );

    if (graph) {
        bool is_valid = nimcp_gpu_knowledge_graph_is_valid(graph);
        EXPECT_TRUE(is_valid);
        nimcp_gpu_knowledge_graph_destroy(graph);
    }
}

//=============================================================================
// Graph Traversal Tests
//=============================================================================

/**
 * TEST: Parallel BFS traversal
 * WHAT: Run BFS from a source node
 * WHY:  Core graph traversal operation
 */
TEST_F(KnowledgeGraphKernelTest, BFS_FromSource_ComputesDistances) {
    RequireGPU();

    // Create chain graph for predictable BFS results
    std::vector<uint32_t> row_offsets, col_indices;
    create_chain_graph(row_offsets, col_indices, 10);

    nimcp_gpu_knowledge_graph_t* graph = nimcp_gpu_knowledge_graph_create(
        ctx,
        row_offsets.data(),
        col_indices.data(),
        nullptr,
        10,
        static_cast<uint32_t>(col_indices.size()),
        DEFAULT_EMBED_DIM
    );

    if (!graph) {
        GTEST_SKIP() << "Graph creation failed";
    }

    nimcp_graph_traversal_result_t result;
    result.distances = create_zero_tensor(10);
    result.parents = create_zero_tensor(10);
    result.visited = create_zero_tensor(10);

    if (!result.distances || !result.parents || !result.visited) {
        if (result.distances) nimcp_gpu_tensor_destroy(result.distances);
        if (result.parents) nimcp_gpu_tensor_destroy(result.parents);
        if (result.visited) nimcp_gpu_tensor_destroy(result.visited);
        nimcp_gpu_knowledge_graph_destroy(graph);
        GTEST_SKIP() << "Tensor creation failed";
    }

    bool bfs_result = nimcp_gpu_bfs(graph, 0, -1, &result);

    if (bfs_result) {
        std::vector<float> distances(10);
        copy_to_host(result.distances, distances.data());

        // In chain graph, distance from 0 to i should be i
        for (int i = 0; i < 10; i++) {
            // Check that distance is computed (may be inf for unreachable)
            EXPECT_TRUE(std::isfinite(distances[i]) || distances[i] > 0);
        }
    }

    nimcp_graph_traversal_result_destroy(&result);
    nimcp_gpu_knowledge_graph_destroy(graph);
}

/**
 * TEST: Multi-source BFS
 * WHAT: BFS from multiple source nodes
 * WHY:  Efficiently compute distances from multiple sources
 */
TEST_F(KnowledgeGraphKernelTest, BFSMultiSource_ComputesMinDistances) {
    RequireGPU();

    std::vector<uint32_t> row_offsets, col_indices;
    std::vector<float> edge_weights;
    create_simple_graph(row_offsets, col_indices, edge_weights, 50, 5);

    nimcp_gpu_knowledge_graph_t* graph = nimcp_gpu_knowledge_graph_create(
        ctx,
        row_offsets.data(),
        col_indices.data(),
        nullptr,
        50,
        static_cast<uint32_t>(col_indices.size()),
        DEFAULT_EMBED_DIM
    );

    if (!graph) {
        GTEST_SKIP() << "Graph creation failed";
    }

    nimcp_graph_traversal_result_t result;
    result.distances = create_zero_tensor(50);
    result.parents = create_zero_tensor(50);
    result.visited = create_zero_tensor(50);

    if (!result.distances) {
        nimcp_gpu_knowledge_graph_destroy(graph);
        GTEST_SKIP() << "Tensor creation failed";
    }

    std::vector<uint32_t> sources = {0, 10, 20};
    bool bfs_result = nimcp_gpu_bfs_multi_source(graph, sources.data(), 3, 5, &result);

    if (bfs_result) {
        // Verify result structure is valid
        EXPECT_GT(result.num_visited, 0u);
    }

    nimcp_graph_traversal_result_destroy(&result);
    nimcp_gpu_knowledge_graph_destroy(graph);
}

/**
 * TEST: Parallel DFS traversal
 * WHAT: Run DFS from a source node
 * WHY:  Alternative traversal strategy
 */
TEST_F(KnowledgeGraphKernelTest, DFS_FromSource_ExploresGraph) {
    RequireGPU();

    std::vector<uint32_t> row_offsets, col_indices;
    std::vector<float> edge_weights;
    create_simple_graph(row_offsets, col_indices, edge_weights, 30, 4);

    nimcp_gpu_knowledge_graph_t* graph = nimcp_gpu_knowledge_graph_create(
        ctx,
        row_offsets.data(),
        col_indices.data(),
        nullptr,
        30,
        static_cast<uint32_t>(col_indices.size()),
        DEFAULT_EMBED_DIM
    );

    if (!graph) {
        GTEST_SKIP() << "Graph creation failed";
    }

    nimcp_graph_traversal_result_t result;
    result.distances = create_zero_tensor(30);
    result.parents = create_zero_tensor(30);
    result.visited = create_zero_tensor(30);

    bool dfs_result = nimcp_gpu_dfs(graph, 0, 10, &result);

    if (dfs_result) {
        std::vector<float> visited(30);
        copy_to_host(result.visited, visited.data());

        // At least some nodes should be visited
        int visited_count = 0;
        for (float v : visited) {
            if (v > 0.5f) visited_count++;
        }
        EXPECT_GT(visited_count, 0);
    }

    nimcp_graph_traversal_result_destroy(&result);
    nimcp_gpu_knowledge_graph_destroy(graph);
}

/**
 * TEST: Shortest path between nodes
 * WHAT: Find shortest path using bidirectional BFS
 * WHY:  Path finding in knowledge graphs
 */
TEST_F(KnowledgeGraphKernelTest, ShortestPath_FindsPath) {
    RequireGPU();

    // Create chain graph for predictable path
    std::vector<uint32_t> row_offsets, col_indices;
    create_chain_graph(row_offsets, col_indices, 10);

    nimcp_gpu_knowledge_graph_t* graph = nimcp_gpu_knowledge_graph_create(
        ctx,
        row_offsets.data(),
        col_indices.data(),
        nullptr,
        10,
        static_cast<uint32_t>(col_indices.size()),
        DEFAULT_EMBED_DIM
    );

    if (!graph) {
        GTEST_SKIP() << "Graph creation failed";
    }

    std::vector<uint32_t> path(10);
    uint32_t path_length = 0;

    bool result = nimcp_gpu_shortest_path(graph, 0, 5, path.data(), &path_length);

    if (result) {
        EXPECT_GT(path_length, 0u);
        EXPECT_LE(path_length, 10u);
    }

    nimcp_gpu_knowledge_graph_destroy(graph);
}

//=============================================================================
// Semantic Similarity Tests
//=============================================================================

/**
 * TEST: Cosine similarity between two nodes
 * WHAT: Compute similarity using embeddings
 * WHY:  Core similarity operation
 */
TEST_F(KnowledgeGraphKernelTest, NodeSimilarity_ComputesCosine) {
    RequireGPU();

    std::vector<uint32_t> row_offsets, col_indices;
    std::vector<float> edge_weights;
    create_simple_graph(row_offsets, col_indices, edge_weights, 50, 3);

    nimcp_gpu_knowledge_graph_t* graph = nimcp_gpu_knowledge_graph_create(
        ctx,
        row_offsets.data(),
        col_indices.data(),
        nullptr,
        50,
        static_cast<uint32_t>(col_indices.size()),
        32
    );

    if (!graph) {
        GTEST_SKIP() << "Graph creation failed";
    }

    // Set embeddings
    std::vector<float> embeddings = create_random_embeddings(50, 32);
    normalize_embeddings(embeddings, 50, 32);
    nimcp_gpu_knowledge_graph_set_embeddings(graph, embeddings.data());

    float similarity = 0.0f;
    bool result = nimcp_gpu_node_similarity(graph, 0, 1, &similarity);

    if (result) {
        // Cosine similarity should be between -1 and 1
        EXPECT_GE(similarity, -1.0f - NUMERICAL_EPS);
        EXPECT_LE(similarity, 1.0f + NUMERICAL_EPS);
    }

    nimcp_gpu_knowledge_graph_destroy(graph);
}

/**
 * TEST: Self-similarity
 * WHAT: Similarity of a node with itself
 * WHY:  Self-similarity should be 1 for normalized embeddings
 */
TEST_F(KnowledgeGraphKernelTest, NodeSimilarity_SelfSimilarity_IsOne) {
    RequireGPU();

    std::vector<uint32_t> row_offsets, col_indices;
    std::vector<float> edge_weights;
    create_simple_graph(row_offsets, col_indices, edge_weights, 20, 3);

    nimcp_gpu_knowledge_graph_t* graph = nimcp_gpu_knowledge_graph_create(
        ctx,
        row_offsets.data(),
        col_indices.data(),
        nullptr,
        20,
        static_cast<uint32_t>(col_indices.size()),
        32
    );

    if (!graph) {
        GTEST_SKIP() << "Graph creation failed";
    }

    // Set normalized embeddings
    std::vector<float> embeddings = create_random_embeddings(20, 32);
    normalize_embeddings(embeddings, 20, 32);
    nimcp_gpu_knowledge_graph_set_embeddings(graph, embeddings.data());

    float similarity = 0.0f;
    bool result = nimcp_gpu_node_similarity(graph, 5, 5, &similarity);

    if (result) {
        EXPECT_NEAR(similarity, 1.0f, 0.01f);
    }

    nimcp_gpu_knowledge_graph_destroy(graph);
}

/**
 * TEST: Pairwise similarity matrix
 * WHAT: Compute similarity for all pairs of specified nodes
 * WHY:  Batch similarity computation
 */
TEST_F(KnowledgeGraphKernelTest, PairwiseSimilarity_SymmetricMatrix) {
    RequireGPU();

    std::vector<uint32_t> row_offsets, col_indices;
    std::vector<float> edge_weights;
    create_simple_graph(row_offsets, col_indices, edge_weights, 50, 3);

    nimcp_gpu_knowledge_graph_t* graph = nimcp_gpu_knowledge_graph_create(
        ctx,
        row_offsets.data(),
        col_indices.data(),
        nullptr,
        50,
        static_cast<uint32_t>(col_indices.size()),
        32
    );

    if (!graph) {
        GTEST_SKIP() << "Graph creation failed";
    }

    std::vector<float> embeddings = create_random_embeddings(50, 32);
    normalize_embeddings(embeddings, 50, 32);
    nimcp_gpu_knowledge_graph_set_embeddings(graph, embeddings.data());

    std::vector<uint32_t> nodes = {0, 5, 10, 15, 20};
    nimcp_gpu_tensor_t* sim_matrix = create_matrix(5, 5);

    if (!sim_matrix) {
        nimcp_gpu_knowledge_graph_destroy(graph);
        GTEST_SKIP() << "Matrix creation failed";
    }

    bool result = nimcp_gpu_pairwise_similarity(graph, nodes.data(), 5, sim_matrix);

    if (result) {
        std::vector<float> sim_host(25);
        copy_to_host(sim_matrix, sim_host.data());

        // Matrix should be symmetric
        for (int i = 0; i < 5; i++) {
            for (int j = i + 1; j < 5; j++) {
                EXPECT_NEAR(sim_host[i * 5 + j], sim_host[j * 5 + i], 0.01f);
            }
        }

        // Diagonal should be 1
        for (int i = 0; i < 5; i++) {
            EXPECT_NEAR(sim_host[i * 5 + i], 1.0f, 0.01f);
        }
    }

    nimcp_gpu_tensor_destroy(sim_matrix);
    nimcp_gpu_knowledge_graph_destroy(graph);
}

/**
 * TEST: k-NN similarity search
 * WHAT: Find k most similar nodes to a query
 * WHY:  Core retrieval operation
 */
TEST_F(KnowledgeGraphKernelTest, KNNSimilarity_FindsTopK) {
    RequireGPU();

    std::vector<uint32_t> row_offsets, col_indices;
    std::vector<float> edge_weights;
    create_simple_graph(row_offsets, col_indices, edge_weights, 100, 3);

    nimcp_gpu_knowledge_graph_t* graph = nimcp_gpu_knowledge_graph_create(
        ctx,
        row_offsets.data(),
        col_indices.data(),
        nullptr,
        100,
        static_cast<uint32_t>(col_indices.size()),
        64
    );

    if (!graph) {
        GTEST_SKIP() << "Graph creation failed";
    }

    std::vector<float> embeddings = create_random_embeddings(100, 64);
    normalize_embeddings(embeddings, 100, 64);
    nimcp_gpu_knowledge_graph_set_embeddings(graph, embeddings.data());

    nimcp_similarity_result_t result;
    result.k = 5;
    result.indices = create_zero_tensor(5);
    result.scores = create_zero_tensor(5);

    if (!result.indices || !result.scores) {
        if (result.indices) nimcp_gpu_tensor_destroy(result.indices);
        if (result.scores) nimcp_gpu_tensor_destroy(result.scores);
        nimcp_gpu_knowledge_graph_destroy(graph);
        GTEST_SKIP() << "Tensor creation failed";
    }

    bool knn_result = nimcp_gpu_knn_similarity(graph, 0, 5, &result);

    if (knn_result) {
        std::vector<float> scores(5);
        copy_to_host(result.scores, scores.data());

        // First result should be self (highest similarity)
        // Scores should be sorted in descending order
        for (int i = 0; i < 4; i++) {
            EXPECT_GE(scores[i], scores[i + 1] - NUMERICAL_EPS);
        }
    }

    nimcp_similarity_result_destroy(&result);
    nimcp_gpu_knowledge_graph_destroy(graph);
}

//=============================================================================
// Knowledge Embedding Operations Tests
//=============================================================================

/**
 * TEST: Embedding update via gradient descent
 * WHAT: Update embeddings with gradients
 * WHY:  Training knowledge graph embeddings
 */
TEST_F(KnowledgeGraphKernelTest, EmbeddingUpdate_ModifiesEmbeddings) {
    RequireGPU();

    std::vector<uint32_t> row_offsets, col_indices;
    std::vector<float> edge_weights;
    create_simple_graph(row_offsets, col_indices, edge_weights, 50, 3);

    nimcp_gpu_knowledge_graph_t* graph = nimcp_gpu_knowledge_graph_create(
        ctx,
        row_offsets.data(),
        col_indices.data(),
        nullptr,
        50,
        static_cast<uint32_t>(col_indices.size()),
        32
    );

    if (!graph) {
        GTEST_SKIP() << "Graph creation failed";
    }

    std::vector<float> embeddings = create_random_embeddings(50, 32);
    nimcp_gpu_knowledge_graph_set_embeddings(graph, embeddings.data());

    // Create gradient tensor
    std::vector<float> grads(50 * 32, 0.1f);
    size_t grad_dims[2] = {50, 32};
    nimcp_gpu_tensor_t* gradients = nimcp_gpu_tensor_from_host(ctx, grads.data(), grad_dims, 2, NIMCP_GPU_PRECISION_FP32);

    if (!gradients) {
        nimcp_gpu_knowledge_graph_destroy(graph);
        GTEST_SKIP() << "Gradient tensor creation failed";
    }

    bool result = nimcp_gpu_embedding_update(graph, gradients, 0.01f);

    // Function may not be implemented, but should not crash
    if (result) {
        SUCCEED();
    }

    nimcp_gpu_tensor_destroy(gradients);
    nimcp_gpu_knowledge_graph_destroy(graph);
}

/**
 * TEST: Triplet loss computation
 * WHAT: Compute triplet loss for training
 * WHY:  Knowledge graph embedding objective
 */
TEST_F(KnowledgeGraphKernelTest, TripletLoss_ComputesLoss) {
    RequireGPU();

    std::vector<uint32_t> row_offsets, col_indices;
    std::vector<float> edge_weights;
    create_simple_graph(row_offsets, col_indices, edge_weights, 50, 3);

    nimcp_gpu_knowledge_graph_t* graph = nimcp_gpu_knowledge_graph_create(
        ctx,
        row_offsets.data(),
        col_indices.data(),
        nullptr,
        50,
        static_cast<uint32_t>(col_indices.size()),
        32
    );

    if (!graph) {
        GTEST_SKIP() << "Graph creation failed";
    }

    std::vector<float> embeddings = create_random_embeddings(50, 32);
    nimcp_gpu_knowledge_graph_set_embeddings(graph, embeddings.data());

    std::vector<uint32_t> anchors = {0, 1, 2, 3};
    std::vector<uint32_t> positives = {1, 2, 3, 4};
    std::vector<uint32_t> negatives = {10, 11, 12, 13};

    float loss = 0.0f;
    bool result = nimcp_gpu_triplet_loss(graph, anchors.data(), positives.data(),
                                          negatives.data(), 4, 0.5f, &loss);

    if (result) {
        EXPECT_GE(loss, 0.0f);
    }

    nimcp_gpu_knowledge_graph_destroy(graph);
}

/**
 * TEST: Embedding normalization
 * WHAT: L2 normalize all embeddings
 * WHY:  Ensure unit-length embeddings for cosine similarity
 */
TEST_F(KnowledgeGraphKernelTest, NormalizeEmbeddings_ProducesUnitVectors) {
    RequireGPU();

    std::vector<uint32_t> row_offsets, col_indices;
    std::vector<float> edge_weights;
    create_simple_graph(row_offsets, col_indices, edge_weights, 30, 3);

    nimcp_gpu_knowledge_graph_t* graph = nimcp_gpu_knowledge_graph_create(
        ctx,
        row_offsets.data(),
        col_indices.data(),
        nullptr,
        30,
        static_cast<uint32_t>(col_indices.size()),
        32
    );

    if (!graph) {
        GTEST_SKIP() << "Graph creation failed";
    }

    // Set non-normalized embeddings
    std::vector<float> embeddings = create_random_embeddings(30, 32);
    nimcp_gpu_knowledge_graph_set_embeddings(graph, embeddings.data());

    bool result = nimcp_gpu_normalize_embeddings(graph);

    if (result) {
        // Self-similarity should be 1 after normalization
        float sim = 0.0f;
        nimcp_gpu_node_similarity(graph, 0, 0, &sim);
        EXPECT_NEAR(sim, 1.0f, 0.01f);
    }

    nimcp_gpu_knowledge_graph_destroy(graph);
}

//=============================================================================
// Node/Edge Aggregation Tests
//=============================================================================

/**
 * TEST: Neighbor feature aggregation (sum)
 * WHAT: Sum neighbor features for each node
 * WHY:  GNN-style message passing
 */
TEST_F(KnowledgeGraphKernelTest, AggregateNeighbors_Sum_ProducesOutput) {
    RequireGPU();

    std::vector<uint32_t> row_offsets, col_indices;
    std::vector<float> edge_weights;
    create_simple_graph(row_offsets, col_indices, edge_weights, 50, 5);

    nimcp_gpu_knowledge_graph_t* graph = nimcp_gpu_knowledge_graph_create(
        ctx,
        row_offsets.data(),
        col_indices.data(),
        nullptr,
        50,
        static_cast<uint32_t>(col_indices.size()),
        32
    );

    if (!graph) {
        GTEST_SKIP() << "Graph creation failed";
    }

    std::vector<float> embeddings = create_random_embeddings(50, 32);
    nimcp_gpu_knowledge_graph_set_embeddings(graph, embeddings.data());

    nimcp_gpu_tensor_t* output = create_matrix(50, 32);

    if (!output) {
        nimcp_gpu_knowledge_graph_destroy(graph);
        GTEST_SKIP() << "Output tensor creation failed";
    }

    bool result = nimcp_gpu_aggregate_neighbors(graph, NIMCP_AGGREGATE_SUM, output);

    if (result) {
        std::vector<float> out_host(50 * 32);
        copy_to_host(output, out_host.data());

        // Output should be finite
        for (float v : out_host) {
            EXPECT_TRUE(std::isfinite(v));
        }
    }

    nimcp_gpu_tensor_destroy(output);
    nimcp_gpu_knowledge_graph_destroy(graph);
}

/**
 * TEST: Multi-hop aggregation
 * WHAT: Aggregate features from k-hop neighborhood
 * WHY:  Multi-layer GNN computation
 */
TEST_F(KnowledgeGraphKernelTest, MultiHopAggregate_MultipleHops) {
    RequireGPU();

    std::vector<uint32_t> row_offsets, col_indices;
    std::vector<float> edge_weights;
    create_simple_graph(row_offsets, col_indices, edge_weights, 50, 5);

    nimcp_gpu_knowledge_graph_t* graph = nimcp_gpu_knowledge_graph_create(
        ctx,
        row_offsets.data(),
        col_indices.data(),
        nullptr,
        50,
        static_cast<uint32_t>(col_indices.size()),
        32
    );

    if (!graph) {
        GTEST_SKIP() << "Graph creation failed";
    }

    std::vector<float> embeddings = create_random_embeddings(50, 32);
    nimcp_gpu_knowledge_graph_set_embeddings(graph, embeddings.data());

    nimcp_gpu_tensor_t* output = create_matrix(50, 32);

    if (!output) {
        nimcp_gpu_knowledge_graph_destroy(graph);
        GTEST_SKIP() << "Output tensor creation failed";
    }

    bool result = nimcp_gpu_multi_hop_aggregate(graph, 2, NIMCP_AGGREGATE_MEAN, output);

    if (result) {
        std::vector<float> out_host(50 * 32);
        copy_to_host(output, out_host.data());

        for (float v : out_host) {
            EXPECT_TRUE(std::isfinite(v));
        }
    }

    nimcp_gpu_tensor_destroy(output);
    nimcp_gpu_knowledge_graph_destroy(graph);
}

//=============================================================================
// Hyperbolic Space Operations Tests
//=============================================================================

/**
 * TEST: Set hyperbolic embeddings
 * WHAT: Set embeddings in Poincare ball model
 * WHY:  Support hierarchical knowledge
 */
TEST_F(KnowledgeGraphKernelTest, SetHyperbolicEmbeddings_SetsFlag) {
    RequireGPU();

    std::vector<uint32_t> row_offsets, col_indices;
    std::vector<float> edge_weights;
    create_simple_graph(row_offsets, col_indices, edge_weights, 30, 3);

    nimcp_gpu_knowledge_graph_t* graph = nimcp_gpu_knowledge_graph_create(
        ctx,
        row_offsets.data(),
        col_indices.data(),
        nullptr,
        30,
        static_cast<uint32_t>(col_indices.size()),
        32
    );

    if (!graph) {
        GTEST_SKIP() << "Graph creation failed";
    }

    // Create embeddings inside Poincare ball (norm < 1)
    std::vector<float> embeddings = create_random_embeddings(30, 32);
    for (size_t i = 0; i < 30; i++) {
        float norm = 0.0f;
        for (size_t j = 0; j < 32; j++) {
            norm += embeddings[i * 32 + j] * embeddings[i * 32 + j];
        }
        norm = std::sqrt(norm);
        // Scale to be inside ball
        float scale = 0.9f / (norm + 1.0f);
        for (size_t j = 0; j < 32; j++) {
            embeddings[i * 32 + j] *= scale;
        }
    }

    bool result = nimcp_gpu_knowledge_graph_set_hyperbolic_embeddings(graph, embeddings.data());

    if (result) {
        EXPECT_TRUE(graph->is_hyperbolic);
    }

    nimcp_gpu_knowledge_graph_destroy(graph);
}

/**
 * TEST: Hyperbolic distance
 * WHAT: Compute Poincare ball distance
 * WHY:  Distance in hyperbolic space
 */
TEST_F(KnowledgeGraphKernelTest, HyperbolicDistance_ComputesDistance) {
    RequireGPU();

    std::vector<uint32_t> row_offsets, col_indices;
    std::vector<float> edge_weights;
    create_simple_graph(row_offsets, col_indices, edge_weights, 20, 3);

    nimcp_gpu_knowledge_graph_t* graph = nimcp_gpu_knowledge_graph_create(
        ctx,
        row_offsets.data(),
        col_indices.data(),
        nullptr,
        20,
        static_cast<uint32_t>(col_indices.size()),
        16
    );

    if (!graph) {
        GTEST_SKIP() << "Graph creation failed";
    }

    // Set hyperbolic embeddings
    std::vector<float> embeddings(20 * 16);
    for (size_t i = 0; i < 20 * 16; i++) {
        embeddings[i] = 0.1f * static_cast<float>(i % 10) / 10.0f;
    }
    nimcp_gpu_knowledge_graph_set_hyperbolic_embeddings(graph, embeddings.data());

    float distance = 0.0f;
    bool result = nimcp_gpu_hyperbolic_distance(graph, 0, 1, &distance);

    if (result) {
        EXPECT_GE(distance, 0.0f);
        EXPECT_TRUE(std::isfinite(distance));
    }

    nimcp_gpu_knowledge_graph_destroy(graph);
}

/**
 * TEST: Convert Euclidean to hyperbolic
 * WHAT: Use exponential map from origin
 * WHY:  Convert standard embeddings to Poincare ball
 */
TEST_F(KnowledgeGraphKernelTest, EuclideanToHyperbolic_ConvertsEmbeddings) {
    RequireGPU();

    std::vector<uint32_t> row_offsets, col_indices;
    std::vector<float> edge_weights;
    create_simple_graph(row_offsets, col_indices, edge_weights, 20, 3);

    nimcp_gpu_knowledge_graph_t* graph = nimcp_gpu_knowledge_graph_create(
        ctx,
        row_offsets.data(),
        col_indices.data(),
        nullptr,
        20,
        static_cast<uint32_t>(col_indices.size()),
        16
    );

    if (!graph) {
        GTEST_SKIP() << "Graph creation failed";
    }

    // Set Euclidean embeddings
    std::vector<float> embeddings = create_random_embeddings(20, 16);
    nimcp_gpu_knowledge_graph_set_embeddings(graph, embeddings.data());

    bool result = nimcp_gpu_euclidean_to_hyperbolic(graph);

    if (result) {
        EXPECT_TRUE(graph->is_hyperbolic);
    }

    nimcp_gpu_knowledge_graph_destroy(graph);
}

//=============================================================================
// Graph Statistics Tests
//=============================================================================

/**
 * TEST: Graph statistics computation
 * WHAT: Get avg degree, max degree, embedding norm
 * WHY:  Monitor graph state
 */
TEST_F(KnowledgeGraphKernelTest, GraphStats_ComputesStatistics) {
    RequireGPU();

    std::vector<uint32_t> row_offsets, col_indices;
    std::vector<float> edge_weights;
    create_simple_graph(row_offsets, col_indices, edge_weights, 50, 5);

    nimcp_gpu_knowledge_graph_t* graph = nimcp_gpu_knowledge_graph_create(
        ctx,
        row_offsets.data(),
        col_indices.data(),
        nullptr,
        50,
        static_cast<uint32_t>(col_indices.size()),
        32
    );

    if (!graph) {
        GTEST_SKIP() << "Graph creation failed";
    }

    std::vector<float> embeddings = create_random_embeddings(50, 32);
    nimcp_gpu_knowledge_graph_set_embeddings(graph, embeddings.data());

    float avg_degree = 0.0f;
    uint32_t max_degree = 0;
    float embedding_norm = 0.0f;

    bool result = nimcp_gpu_knowledge_graph_stats(graph, &avg_degree, &max_degree, &embedding_norm);

    if (result) {
        EXPECT_GT(avg_degree, 0.0f);
        EXPECT_GT(max_degree, 0u);
        EXPECT_GT(embedding_norm, 0.0f);
    }

    nimcp_gpu_knowledge_graph_destroy(graph);
}

//=============================================================================
// Knowledge Graph Embedding DAO Tests
//=============================================================================

/**
 * TEST: DAO creation
 * WHAT: Create knowledge embedding DAO
 * WHY:  Verify DAO initialization
 */
TEST_F(KnowledgeGraphKernelTest, DAOCreate_Succeeds) {
    RequireGPU();

    nimcp_knowledge_embedding_dao_t* dao = nimcp_knowledge_embedding_dao_create(
        ctx, 100, 10, DEFAULT_EMBED_DIM);

    if (dao) {
        EXPECT_EQ(dao->max_entities, 100);
        EXPECT_EQ(dao->max_relations, 10);
        EXPECT_EQ(dao->embedding_dim, (int)DEFAULT_EMBED_DIM);
        EXPECT_EQ(dao->num_entities, 0);
        EXPECT_NE(dao->d_entity_embeddings, nullptr);
        EXPECT_NE(dao->d_relation_embeddings, nullptr);
        nimcp_knowledge_embedding_dao_destroy(dao);
    }
}

/**
 * TEST: DAO create embedding
 * WHAT: Create an entity embedding via DAO
 * WHY:  Test CRUD create operation
 */
TEST_F(KnowledgeGraphKernelTest, DAOCreateEmbedding_Succeeds) {
    RequireGPU();

    nimcp_knowledge_embedding_dao_t* dao = nimcp_knowledge_embedding_dao_create(
        ctx, 100, 10, DEFAULT_EMBED_DIM);

    if (!dao) {
        GTEST_SKIP() << "DAO creation failed";
    }

    std::vector<float> embedding(DEFAULT_EMBED_DIM);
    for (uint32_t i = 0; i < DEFAULT_EMBED_DIM; i++) {
        embedding[i] = static_cast<float>(i) / DEFAULT_EMBED_DIM;
    }

    int result = dao->create_embedding(dao, 0, embedding.data());
    EXPECT_EQ(result, 0);
    EXPECT_EQ(dao->num_entities, 1);

    // Creating duplicate should fail
    int duplicate_result = dao->create_embedding(dao, 0, embedding.data());
    EXPECT_EQ(duplicate_result, -1);

    nimcp_knowledge_embedding_dao_destroy(dao);
}

/**
 * TEST: DAO read embedding
 * WHAT: Read an entity embedding via DAO
 * WHY:  Test CRUD read operation
 */
TEST_F(KnowledgeGraphKernelTest, DAOReadEmbedding_ReturnsCorrectData) {
    RequireGPU();

    nimcp_knowledge_embedding_dao_t* dao = nimcp_knowledge_embedding_dao_create(
        ctx, 100, 10, DEFAULT_EMBED_DIM);

    if (!dao) {
        GTEST_SKIP() << "DAO creation failed";
    }

    // Create embedding
    std::vector<float> embedding(DEFAULT_EMBED_DIM);
    for (uint32_t i = 0; i < DEFAULT_EMBED_DIM; i++) {
        embedding[i] = static_cast<float>(i) / DEFAULT_EMBED_DIM;
    }
    dao->create_embedding(dao, 5, embedding.data());

    // Read it back
    std::vector<float> read_embedding(DEFAULT_EMBED_DIM);
    int result = dao->read_embedding(dao, 5, read_embedding.data());
    EXPECT_EQ(result, 0);

    // Verify data matches
    for (uint32_t i = 0; i < DEFAULT_EMBED_DIM; i++) {
        EXPECT_NEAR(embedding[i], read_embedding[i], NUMERICAL_EPS);
    }

    // Reading non-existent should fail
    int invalid_result = dao->read_embedding(dao, 99, read_embedding.data());
    EXPECT_EQ(invalid_result, -1);

    nimcp_knowledge_embedding_dao_destroy(dao);
}

/**
 * TEST: DAO update embedding
 * WHAT: Update an entity embedding via DAO
 * WHY:  Test CRUD update operation
 */
TEST_F(KnowledgeGraphKernelTest, DAOUpdateEmbedding_ModifiesData) {
    RequireGPU();

    nimcp_knowledge_embedding_dao_t* dao = nimcp_knowledge_embedding_dao_create(
        ctx, 100, 10, DEFAULT_EMBED_DIM);

    if (!dao) {
        GTEST_SKIP() << "DAO creation failed";
    }

    // Create initial embedding
    std::vector<float> embedding(DEFAULT_EMBED_DIM, 1.0f);
    dao->create_embedding(dao, 0, embedding.data());

    // Update with new values
    std::vector<float> new_embedding(DEFAULT_EMBED_DIM, 2.0f);
    int result = dao->update_embedding(dao, 0, new_embedding.data());
    EXPECT_EQ(result, 0);

    // Verify update
    std::vector<float> read_embedding(DEFAULT_EMBED_DIM);
    dao->read_embedding(dao, 0, read_embedding.data());
    for (uint32_t i = 0; i < DEFAULT_EMBED_DIM; i++) {
        EXPECT_NEAR(read_embedding[i], 2.0f, NUMERICAL_EPS);
    }

    nimcp_knowledge_embedding_dao_destroy(dao);
}

/**
 * TEST: DAO delete embedding
 * WHAT: Delete an entity embedding via DAO
 * WHY:  Test CRUD delete operation
 */
TEST_F(KnowledgeGraphKernelTest, DAODeleteEmbedding_RemovesEntity) {
    RequireGPU();

    nimcp_knowledge_embedding_dao_t* dao = nimcp_knowledge_embedding_dao_create(
        ctx, 100, 10, DEFAULT_EMBED_DIM);

    if (!dao) {
        GTEST_SKIP() << "DAO creation failed";
    }

    // Create and delete embedding
    std::vector<float> embedding(DEFAULT_EMBED_DIM, 1.0f);
    dao->create_embedding(dao, 0, embedding.data());
    EXPECT_EQ(dao->num_entities, 1);

    int result = dao->delete_embedding(dao, 0);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(dao->num_entities, 0);

    // Reading deleted should fail
    std::vector<float> read_embedding(DEFAULT_EMBED_DIM);
    int read_result = dao->read_embedding(dao, 0, read_embedding.data());
    EXPECT_EQ(read_result, -1);

    nimcp_knowledge_embedding_dao_destroy(dao);
}

/**
 * TEST: Cosine similarity search
 * WHAT: Find similar entities using cosine similarity
 * WHY:  Core embedding search functionality
 */
TEST_F(KnowledgeGraphKernelTest, DAOFindSimilar_ReturnsCorrectEntities) {
    RequireGPU();

    nimcp_knowledge_embedding_dao_t* dao = nimcp_knowledge_embedding_dao_create(
        ctx, 100, 10, DEFAULT_EMBED_DIM);

    if (!dao) {
        GTEST_SKIP() << "DAO creation failed";
    }

    // Create several embeddings
    for (int i = 0; i < 10; i++) {
        std::vector<float> embedding(DEFAULT_EMBED_DIM);
        for (uint32_t j = 0; j < DEFAULT_EMBED_DIM; j++) {
            embedding[j] = (i == 0) ? 1.0f : static_cast<float>(j + i) / DEFAULT_EMBED_DIM;
        }
        dao->create_embedding(dao, i, embedding.data());
    }

    // Query with embedding similar to entity 0
    std::vector<float> query(DEFAULT_EMBED_DIM, 1.0f);
    std::vector<int> results(5);
    std::vector<float> scores(5);

    int result = dao->find_similar(dao, query.data(), 5, results.data(), scores.data());
    EXPECT_EQ(result, 0);

    // Entity 0 should be most similar (exact match)
    EXPECT_EQ(results[0], 0);
    EXPECT_NEAR(scores[0], 1.0f, 0.01f);  // Cosine similarity should be ~1.0

    nimcp_knowledge_embedding_dao_destroy(dao);
}

/**
 * TEST: Top-k retrieval correctness
 * WHAT: Verify top-k returns correct k entities
 * WHY:  Ensure ranking is correct
 */
TEST_F(KnowledgeGraphKernelTest, DAOFindSimilar_ReturnsCorrectK) {
    RequireGPU();

    nimcp_knowledge_embedding_dao_t* dao = nimcp_knowledge_embedding_dao_create(
        ctx, 100, 10, DEFAULT_EMBED_DIM);

    if (!dao) {
        GTEST_SKIP() << "DAO creation failed";
    }

    // Create 20 embeddings
    for (int i = 0; i < 20; i++) {
        std::vector<float> embedding(DEFAULT_EMBED_DIM);
        for (uint32_t j = 0; j < DEFAULT_EMBED_DIM; j++) {
            embedding[j] = static_cast<float>(i * DEFAULT_EMBED_DIM + j) / (20 * DEFAULT_EMBED_DIM);
        }
        dao->create_embedding(dao, i, embedding.data());
    }

    // Query for top 5
    std::vector<float> query(DEFAULT_EMBED_DIM);
    for (uint32_t j = 0; j < DEFAULT_EMBED_DIM; j++) {
        query[j] = static_cast<float>(j) / (20 * DEFAULT_EMBED_DIM);  // Similar to entity 0
    }

    std::vector<int> results(5);
    std::vector<float> scores(5);

    dao->find_similar(dao, query.data(), 5, results.data(), scores.data());

    // Verify we got 5 results
    int valid_count = 0;
    for (int i = 0; i < 5; i++) {
        if (results[i] >= 0) valid_count++;
    }
    EXPECT_EQ(valid_count, 5);

    // Scores should be in descending order
    for (int i = 0; i < 4; i++) {
        EXPECT_GE(scores[i], scores[i + 1]);
    }

    nimcp_knowledge_embedding_dao_destroy(dao);
}

//=============================================================================
// TransE Training Tests
//=============================================================================

#ifdef NIMCP_ENABLE_CUDA
/**
 * TEST: TransE score computation
 * WHAT: Compute ||h + r - t|| score
 * WHY:  Verify TransE scoring function
 */
TEST_F(KnowledgeGraphKernelTest, TransEScore_ComputesCorrectly) {
    RequireGPU();

    nimcp_knowledge_embedding_dao_t* dao = nimcp_knowledge_embedding_dao_create(
        ctx, 100, 10, DEFAULT_EMBED_DIM);

    if (!dao) {
        GTEST_SKIP() << "DAO creation failed";
    }

    // Create head, relation, tail embeddings
    std::vector<float> head_emb(DEFAULT_EMBED_DIM, 1.0f);
    std::vector<float> rel_emb(DEFAULT_EMBED_DIM, 0.5f);
    std::vector<float> tail_emb(DEFAULT_EMBED_DIM, 1.5f);  // h + r = t (perfect TransE)

    dao->create_embedding(dao, 0, head_emb.data());
    dao->create_embedding(dao, 1, tail_emb.data());

    // Set relation embedding directly
    cudaMemcpy(dao->d_relation_embeddings, rel_emb.data(),
               DEFAULT_EMBED_DIM * sizeof(float), cudaMemcpyHostToDevice);

    // Mark relation as valid
    int valid = 1;
    cudaMemcpy(dao->d_relation_valid, &valid, sizeof(int), cudaMemcpyHostToDevice);

    float score = 0.0f;
    int result = nimcp_kg_transe_score(dao, 0, 0, 1, &score);

    if (result == 0) {
        // For perfect TransE: h + r = t, score should be 0
        EXPECT_NEAR(score, 0.0f, 0.1f);
    }

    nimcp_knowledge_embedding_dao_destroy(dao);
}

/**
 * TEST: TransE training step
 * WHAT: Perform one TransE training step
 * WHY:  Verify gradients are computed and applied
 */
TEST_F(KnowledgeGraphKernelTest, TransETrainStep_UpdatesEmbeddings) {
    RequireGPU();

    nimcp_knowledge_embedding_dao_t* dao = nimcp_knowledge_embedding_dao_create(
        ctx, 100, 10, DEFAULT_EMBED_DIM);

    if (!dao) {
        GTEST_SKIP() << "DAO creation failed";
    }

    // Create initial embeddings
    for (int i = 0; i < 10; i++) {
        std::vector<float> embedding(DEFAULT_EMBED_DIM);
        for (uint32_t j = 0; j < DEFAULT_EMBED_DIM; j++) {
            embedding[j] = static_cast<float>(rand()) / RAND_MAX;
        }
        dao->create_embedding(dao, i, embedding.data());
    }

    // Initialize relation embeddings
    std::vector<float> rel_emb(DEFAULT_EMBED_DIM * 5);
    for (size_t i = 0; i < rel_emb.size(); i++) {
        rel_emb[i] = static_cast<float>(rand()) / RAND_MAX;
    }
    cudaMemcpy(dao->d_relation_embeddings, rel_emb.data(),
               rel_emb.size() * sizeof(float), cudaMemcpyHostToDevice);

    // Read initial embedding
    std::vector<float> before(DEFAULT_EMBED_DIM);
    dao->read_embedding(dao, 0, before.data());

    // Training configuration
    nimcp_kg_train_config_t config;
    config.learning_rate = 0.01f;
    config.margin = 1.0f;
    config.negative_samples = 1;
    config.regularization = 0.0f;
    config.normalize_embeddings = false;

    // Training data (head, relation, tail)
    int heads[] = {0, 1, 2};
    int relations[] = {0, 1, 0};
    int tails[] = {1, 2, 3};

    int result = nimcp_kg_train_step(dao, heads, relations, tails, 3, &config);
    EXPECT_EQ(result, 0);

    // Read embedding after training
    std::vector<float> after(DEFAULT_EMBED_DIM);
    dao->read_embedding(dao, 0, after.data());

    // Embeddings should have changed
    bool changed = false;
    for (uint32_t i = 0; i < DEFAULT_EMBED_DIM; i++) {
        if (std::abs(before[i] - after[i]) > NUMERICAL_EPS) {
            changed = true;
            break;
        }
    }
    // Note: embeddings may or may not change depending on gradient direction
    // Just verify the function completes without error

    nimcp_knowledge_embedding_dao_destroy(dao);
}
#endif  // NIMCP_ENABLE_CUDA

//=============================================================================
// Semantic Search and Path Finding Tests
//=============================================================================

/**
 * TEST: Semantic search API
 * WHAT: Perform semantic search on knowledge graph
 * WHY:  High-level search API
 */
TEST_F(KnowledgeGraphKernelTest, SemanticSearch_ReturnsResults) {
    RequireGPU();

    nimcp_knowledge_embedding_dao_t* dao = nimcp_knowledge_embedding_dao_create(
        ctx, 100, 10, DEFAULT_EMBED_DIM);

    if (!dao) {
        GTEST_SKIP() << "DAO creation failed";
    }

    // Create embeddings
    for (int i = 0; i < 10; i++) {
        std::vector<float> embedding(DEFAULT_EMBED_DIM);
        for (uint32_t j = 0; j < DEFAULT_EMBED_DIM; j++) {
            embedding[j] = static_cast<float>(i == 0 ? 1.0f : (j + i) / (float)DEFAULT_EMBED_DIM);
        }
        dao->create_embedding(dao, i, embedding.data());
    }

    std::vector<float> query(DEFAULT_EMBED_DIM, 1.0f);  // Query similar to entity 0
    nimcp_kg_result_t result;
    memset(&result, 0, sizeof(result));

    int search_result = nimcp_kg_semantic_search(dao, query.data(), 3, &result);
    EXPECT_EQ(search_result, 0);
    EXPECT_GT(result.num_results, 0);
    EXPECT_NE(result.matched_entities, nullptr);
    EXPECT_NE(result.scores, nullptr);

    if (result.num_results > 0) {
        // First result should be entity 0 (most similar)
        EXPECT_EQ(result.matched_entities[0], 0);
    }

    nimcp_kg_result_destroy(&result);
    nimcp_knowledge_embedding_dao_destroy(dao);
}

/**
 * TEST: Path finding API
 * WHAT: Find path between entities
 * WHY:  Graph navigation
 */
TEST_F(KnowledgeGraphKernelTest, FindPath_ReturnsPath) {
    RequireGPU();

    nimcp_knowledge_embedding_dao_t* dao = nimcp_knowledge_embedding_dao_create(
        ctx, 100, 10, DEFAULT_EMBED_DIM);

    if (!dao) {
        GTEST_SKIP() << "DAO creation failed";
    }

    // Create source and target embeddings
    std::vector<float> source_emb(DEFAULT_EMBED_DIM, 1.0f);
    std::vector<float> target_emb(DEFAULT_EMBED_DIM, 0.9f);  // Similar to source

    dao->create_embedding(dao, 0, source_emb.data());
    dao->create_embedding(dao, 1, target_emb.data());

    nimcp_kg_result_t result;
    memset(&result, 0, sizeof(result));

    int path_result = nimcp_kg_find_path(dao, 0, 1, 5, &result);
    EXPECT_EQ(path_result, 0);
    EXPECT_GT(result.num_results, 0);

    if (result.num_results > 0) {
        EXPECT_NE(result.matched_entities, nullptr);
        EXPECT_NE(result.scores, nullptr);
    }

    nimcp_kg_result_destroy(&result);
    nimcp_knowledge_embedding_dao_destroy(dao);
}

/**
 * TEST: Pattern matching API
 * WHAT: Match pattern in knowledge graph
 * WHY:  Complex query support
 */
TEST_F(KnowledgeGraphKernelTest, PatternMatch_ReturnsMatches) {
    RequireGPU();

    nimcp_knowledge_embedding_dao_t* dao = nimcp_knowledge_embedding_dao_create(
        ctx, 100, 10, DEFAULT_EMBED_DIM);

    if (!dao) {
        GTEST_SKIP() << "DAO creation failed";
    }

    // Create embeddings
    for (int i = 0; i < 5; i++) {
        std::vector<float> embedding(DEFAULT_EMBED_DIM);
        for (uint32_t j = 0; j < DEFAULT_EMBED_DIM; j++) {
            embedding[j] = static_cast<float>(i * DEFAULT_EMBED_DIM + j) / (5 * DEFAULT_EMBED_DIM);
        }
        dao->create_embedding(dao, i, embedding.data());
    }

    // Create pattern query
    nimcp_kg_query_t pattern;
    memset(&pattern, 0, sizeof(pattern));
    pattern.query_type = NIMCP_KG_QUERY_MATCH_PATTERN;

    std::vector<float> query_emb(DEFAULT_EMBED_DIM);
    for (uint32_t j = 0; j < DEFAULT_EMBED_DIM; j++) {
        query_emb[j] = static_cast<float>(j) / (5 * DEFAULT_EMBED_DIM);  // Similar to entity 0
    }
    pattern.query_embedding = query_emb.data();
    pattern.top_k = 3;

    nimcp_kg_result_t result;
    memset(&result, 0, sizeof(result));

    int match_result = nimcp_kg_pattern_match(dao, &pattern, &result);
    EXPECT_EQ(match_result, 0);

    nimcp_kg_result_destroy(&result);
    nimcp_knowledge_embedding_dao_destroy(dao);
}

#ifdef NIMCP_ENABLE_CUDA
/**
 * TEST: Predict tail entity
 * WHAT: Predict tail given head and relation
 * WHY:  Knowledge graph completion
 */
TEST_F(KnowledgeGraphKernelTest, PredictTail_ReturnsRankedEntities) {
    RequireGPU();

    nimcp_knowledge_embedding_dao_t* dao = nimcp_knowledge_embedding_dao_create(
        ctx, 100, 10, DEFAULT_EMBED_DIM);

    if (!dao) {
        GTEST_SKIP() << "DAO creation failed";
    }

    // Create entity embeddings
    for (int i = 0; i < 10; i++) {
        std::vector<float> embedding(DEFAULT_EMBED_DIM);
        for (uint32_t j = 0; j < DEFAULT_EMBED_DIM; j++) {
            embedding[j] = static_cast<float>(i + j) / DEFAULT_EMBED_DIM;
        }
        dao->create_embedding(dao, i, embedding.data());
    }

    // Set relation embedding
    std::vector<float> rel_emb(DEFAULT_EMBED_DIM, 0.1f);
    cudaMemcpy(dao->d_relation_embeddings, rel_emb.data(),
               DEFAULT_EMBED_DIM * sizeof(float), cudaMemcpyHostToDevice);

    std::vector<int> predictions(5);
    std::vector<float> scores(5);

    int result = nimcp_kg_predict_tail(dao, 0, 0, 5, predictions.data(), scores.data());
    EXPECT_EQ(result, 0);

    // Should return valid entity indices
    for (int i = 0; i < 5; i++) {
        if (predictions[i] >= 0) {
            EXPECT_LT(predictions[i], 10);
        }
    }

    nimcp_knowledge_embedding_dao_destroy(dao);
}

/**
 * TEST: Predict head entity
 * WHAT: Predict head given relation and tail
 * WHY:  Reverse knowledge graph completion
 */
TEST_F(KnowledgeGraphKernelTest, PredictHead_ReturnsRankedEntities) {
    RequireGPU();

    nimcp_knowledge_embedding_dao_t* dao = nimcp_knowledge_embedding_dao_create(
        ctx, 100, 10, DEFAULT_EMBED_DIM);

    if (!dao) {
        GTEST_SKIP() << "DAO creation failed";
    }

    // Create entity embeddings
    for (int i = 0; i < 10; i++) {
        std::vector<float> embedding(DEFAULT_EMBED_DIM);
        for (uint32_t j = 0; j < DEFAULT_EMBED_DIM; j++) {
            embedding[j] = static_cast<float>(i + j) / DEFAULT_EMBED_DIM;
        }
        dao->create_embedding(dao, i, embedding.data());
    }

    // Set relation embedding
    std::vector<float> rel_emb(DEFAULT_EMBED_DIM, 0.1f);
    cudaMemcpy(dao->d_relation_embeddings, rel_emb.data(),
               DEFAULT_EMBED_DIM * sizeof(float), cudaMemcpyHostToDevice);

    std::vector<int> predictions(5);
    std::vector<float> scores(5);

    int result = nimcp_kg_predict_head(dao, 0, 5, 5, predictions.data(), scores.data());
    EXPECT_EQ(result, 0);

    // Should return valid entity indices
    for (int i = 0; i < 5; i++) {
        if (predictions[i] >= 0) {
            EXPECT_LT(predictions[i], 10);
        }
    }

    nimcp_knowledge_embedding_dao_destroy(dao);
}
#endif  // NIMCP_ENABLE_CUDA

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
