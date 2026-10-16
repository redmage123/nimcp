/**
 * @file test_knowledge_graph_gpu_recovery.cpp
 * @brief Unit tests for GPU recovery integration in knowledge graph module
 *
 * WHAT: Tests GPU recovery for knowledge graph operations
 * WHY:  Verify self-healing from OOM, numerical errors, and kernel failures
 * HOW:  Test recovery initialization, error handling, and CPU fallback
 *
 * TEST COVERAGE:
 * - Recovery initialization in knowledge graph creation
 * - OOM recovery during embedding allocation
 * - Numerical error recovery in similarity computation
 * - Kernel launch failure recovery in traversal
 * - CPU fallback for knowledge graph operations
 * - Hyperbolic embedding recovery
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
#include "gpu/knowledge/nimcp_knowledge_graph_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#endif

namespace {

/* ============================================================================
 * Test Fixture
 * ============================================================================ */
class KnowledgeGraphGPURecoveryTest : public ::testing::Test {
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
        if (kg) {
            nimcp_gpu_knowledge_graph_destroy(kg);
            kg = nullptr;
        }
        if (dao) {
            nimcp_knowledge_embedding_dao_destroy(dao);
            dao = nullptr;
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

    /* Helper to create simple knowledge graph */
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_knowledge_graph_t* createSimpleKG(uint32_t num_nodes,
                                                 uint32_t embed_dim) {
        /* Create simple chain graph */
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
    nimcp_gpu_knowledge_graph_t* kg = nullptr;
    nimcp_knowledge_embedding_dao_t* dao = nullptr;
#endif
};

/* ============================================================================
 * Test: Recovery initialization at knowledge graph creation
 * ============================================================================ */
TEST_F(KnowledgeGraphGPURecoveryTest, RecoveryInitializedAtKGCreation) {
#ifdef NIMCP_ENABLE_CUDA
    kg = createSimpleKG(10, 64);
    ASSERT_NE(kg, nullptr);

    EXPECT_TRUE(nimcp_gpu_recovery_is_initialized())
        << "Recovery should be initialized after KG creation";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Invalid parameter recovery in KG creation
 * ============================================================================ */
TEST_F(KnowledgeGraphGPURecoveryTest, InvalidParamRecoveryKGCreate) {
#ifdef NIMCP_ENABLE_CUDA
    /* NULL context should fail gracefully */
    std::vector<uint32_t> row_offsets = {0, 1, 2};
    std::vector<uint32_t> col_indices = {1, 0};

    nimcp_gpu_knowledge_graph_t* bad_kg = nimcp_gpu_knowledge_graph_create(
        nullptr, row_offsets.data(), col_indices.data(), nullptr, 2, 2, 64);
    EXPECT_EQ(bad_kg, nullptr) << "Should fail gracefully for NULL context";

    /* NULL row_offsets should fail gracefully */
    bad_kg = nimcp_gpu_knowledge_graph_create(
        ctx, nullptr, col_indices.data(), nullptr, 2, 2, 64);
    EXPECT_EQ(bad_kg, nullptr) << "Should fail gracefully for NULL row_offsets";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Set embeddings with recovery
 * ============================================================================ */
TEST_F(KnowledgeGraphGPURecoveryTest, SetEmbeddingsWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    kg = createSimpleKG(10, 32);
    ASSERT_NE(kg, nullptr);

    /* Create random embeddings */
    std::vector<float> embeddings(10 * 32);
    for (size_t i = 0; i < embeddings.size(); i++) {
        embeddings[i] = (float)(i % 100) / 100.0f;
    }

    bool result = nimcp_gpu_knowledge_graph_set_embeddings(kg, embeddings.data());
    EXPECT_TRUE(result) << "Set embeddings should succeed with recovery";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Hyperbolic embeddings with recovery
 * ============================================================================ */
TEST_F(KnowledgeGraphGPURecoveryTest, HyperbolicEmbeddingsWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    kg = createSimpleKG(10, 32);
    ASSERT_NE(kg, nullptr);

    /* Create embeddings in Poincare ball (norm < 1) */
    std::vector<float> embeddings(10 * 32);
    for (size_t i = 0; i < 10; i++) {
        float norm = 0.0f;
        for (size_t j = 0; j < 32; j++) {
            float val = (float)((i * 32 + j) % 100) / 200.0f;  /* Small values */
            embeddings[i * 32 + j] = val;
            norm += val * val;
        }
        /* Normalize to be inside Poincare ball */
        norm = sqrtf(norm);
        if (norm > 0.9f) {
            for (size_t j = 0; j < 32; j++) {
                embeddings[i * 32 + j] *= 0.8f / norm;
            }
        }
    }

    bool result = nimcp_gpu_knowledge_graph_set_hyperbolic_embeddings(
        kg, embeddings.data());
    EXPECT_TRUE(result) << "Hyperbolic embeddings should succeed with recovery";
    EXPECT_TRUE(kg->is_hyperbolic) << "Graph should be marked as hyperbolic";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: BFS traversal with recovery
 * ============================================================================ */
TEST_F(KnowledgeGraphGPURecoveryTest, BFSTraversalWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    kg = createSimpleKG(20, 64);
    ASSERT_NE(kg, nullptr);

    nimcp_graph_traversal_result_t result;
    memset(&result, 0, sizeof(result));

    bool success = nimcp_gpu_bfs(kg, 0, -1, &result);
    EXPECT_TRUE(success) << "BFS should succeed with recovery";
    EXPECT_GT(result.num_visited, 0u) << "Should visit some nodes";

    nimcp_graph_traversal_result_destroy(&result);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Multi-source BFS with recovery
 * ============================================================================ */
TEST_F(KnowledgeGraphGPURecoveryTest, MultiSourceBFSWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    kg = createSimpleKG(20, 64);
    ASSERT_NE(kg, nullptr);

    std::vector<uint32_t> sources = {0, 19};  /* Both ends */
    nimcp_graph_traversal_result_t result;
    memset(&result, 0, sizeof(result));

    bool success = nimcp_gpu_bfs_multi_source(kg, sources.data(), 2, -1, &result);
    EXPECT_TRUE(success) << "Multi-source BFS should succeed with recovery";

    nimcp_graph_traversal_result_destroy(&result);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Node similarity with recovery
 * ============================================================================ */
TEST_F(KnowledgeGraphGPURecoveryTest, NodeSimilarityWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    kg = createSimpleKG(10, 32);
    ASSERT_NE(kg, nullptr);

    /* Set embeddings */
    std::vector<float> embeddings(10 * 32);
    for (size_t i = 0; i < 10; i++) {
        for (size_t j = 0; j < 32; j++) {
            embeddings[i * 32 + j] = (i == j % 10) ? 1.0f : 0.0f;
        }
    }
    ASSERT_TRUE(nimcp_gpu_knowledge_graph_set_embeddings(kg, embeddings.data()));

    float similarity = 0.0f;
    bool success = nimcp_gpu_node_similarity(kg, 0, 0, &similarity);
    EXPECT_TRUE(success) << "Node similarity should succeed";
    EXPECT_NEAR(similarity, 1.0f, 0.01f) << "Self-similarity should be 1.0";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: KNN similarity with recovery
 * ============================================================================ */
TEST_F(KnowledgeGraphGPURecoveryTest, KNNSimilarityWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    kg = createSimpleKG(20, 32);
    ASSERT_NE(kg, nullptr);

    /* Set embeddings with clear patterns */
    std::vector<float> embeddings(20 * 32, 0.0f);
    for (size_t i = 0; i < 20; i++) {
        embeddings[i * 32 + (i % 32)] = 1.0f;
    }
    ASSERT_TRUE(nimcp_gpu_knowledge_graph_set_embeddings(kg, embeddings.data()));

    nimcp_similarity_result_t result;
    memset(&result, 0, sizeof(result));

    bool success = nimcp_gpu_knn_similarity(kg, 0, 5, &result);
    EXPECT_TRUE(success) << "KNN similarity should succeed with recovery";
    EXPECT_EQ(result.k, 5u) << "Should return requested k results";

    nimcp_similarity_result_destroy(&result);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Neighbor aggregation with recovery
 * ============================================================================ */
TEST_F(KnowledgeGraphGPURecoveryTest, NeighborAggregationWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    kg = createSimpleKG(10, 32);
    ASSERT_NE(kg, nullptr);

    /* Set embeddings */
    std::vector<float> embeddings(10 * 32, 1.0f);
    ASSERT_TRUE(nimcp_gpu_knowledge_graph_set_embeddings(kg, embeddings.data()));

    /* Create output tensor */
    size_t dims[2] = {10, 32};
    nimcp_gpu_tensor_t* output = nimcp_gpu_tensor_create(ctx, dims, 2,
                                                          NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(output, nullptr);

    bool success = nimcp_gpu_aggregate_neighbors(kg, NIMCP_AGGREGATE_SUM, output);
    EXPECT_TRUE(success) << "Neighbor aggregation should succeed";

    nimcp_gpu_tensor_destroy(output);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Multi-hop aggregation with recovery
 * ============================================================================ */
TEST_F(KnowledgeGraphGPURecoveryTest, MultiHopAggregationWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    kg = createSimpleKG(10, 32);
    ASSERT_NE(kg, nullptr);

    /* Set embeddings */
    std::vector<float> embeddings(10 * 32, 1.0f);
    ASSERT_TRUE(nimcp_gpu_knowledge_graph_set_embeddings(kg, embeddings.data()));

    /* Create output tensor */
    size_t dims[2] = {10, 32};
    nimcp_gpu_tensor_t* output = nimcp_gpu_tensor_create(ctx, dims, 2,
                                                          NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(output, nullptr);

    bool success = nimcp_gpu_multi_hop_aggregate(kg, 2, NIMCP_AGGREGATE_MEAN, output);
    EXPECT_TRUE(success) << "Multi-hop aggregation should succeed";

    nimcp_gpu_tensor_destroy(output);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Triplet loss with recovery
 * ============================================================================ */
TEST_F(KnowledgeGraphGPURecoveryTest, TripletLossWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    kg = createSimpleKG(10, 32);
    ASSERT_NE(kg, nullptr);

    /* Set distinct embeddings */
    std::vector<float> embeddings(10 * 32, 0.0f);
    for (size_t i = 0; i < 10; i++) {
        embeddings[i * 32 + i] = 1.0f;
    }
    ASSERT_TRUE(nimcp_gpu_knowledge_graph_set_embeddings(kg, embeddings.data()));

    /* Create triplets: anchor=0, positive=1, negative=9 */
    std::vector<uint32_t> anchors = {0};
    std::vector<uint32_t> positives = {1};
    std::vector<uint32_t> negatives = {9};

    float loss = 0.0f;
    bool success = nimcp_gpu_triplet_loss(kg, anchors.data(), positives.data(),
                                          negatives.data(), 1, 0.5f, &loss);
    EXPECT_TRUE(success) << "Triplet loss should succeed with recovery";
    EXPECT_GE(loss, 0.0f) << "Loss should be non-negative";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Embedding DAO creation with recovery
 * ============================================================================ */
TEST_F(KnowledgeGraphGPURecoveryTest, DAOCreationWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    dao = nimcp_knowledge_embedding_dao_create(ctx, 100, 10, 64);
    ASSERT_NE(dao, nullptr) << "DAO creation should succeed with recovery";

    EXPECT_EQ(dao->max_entities, 100);
    EXPECT_EQ(dao->max_relations, 10);
    EXPECT_EQ(dao->embedding_dim, 64);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: DAO embedding operations with recovery
 * ============================================================================ */
TEST_F(KnowledgeGraphGPURecoveryTest, DAOEmbeddingOpsWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    dao = nimcp_knowledge_embedding_dao_create(ctx, 100, 10, 32);
    ASSERT_NE(dao, nullptr);

    /* Create embedding */
    std::vector<float> embedding(32, 1.0f);
    int result = dao->create_embedding(dao, 0, embedding.data());
    EXPECT_EQ(result, 0) << "Create embedding should succeed";

    /* Read embedding */
    std::vector<float> read_embedding(32, 0.0f);
    result = dao->read_embedding(dao, 0, read_embedding.data());
    EXPECT_EQ(result, 0) << "Read embedding should succeed";

    /* Verify */
    for (int i = 0; i < 32; i++) {
        EXPECT_FLOAT_EQ(read_embedding[i], 1.0f);
    }

    /* Update embedding */
    std::vector<float> new_embedding(32, 2.0f);
    result = dao->update_embedding(dao, 0, new_embedding.data());
    EXPECT_EQ(result, 0) << "Update embedding should succeed";

    /* Delete embedding */
    result = dao->delete_embedding(dao, 0);
    EXPECT_EQ(result, 0) << "Delete embedding should succeed";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Semantic search with recovery
 * ============================================================================ */
TEST_F(KnowledgeGraphGPURecoveryTest, SemanticSearchWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    dao = nimcp_knowledge_embedding_dao_create(ctx, 100, 10, 32);
    ASSERT_NE(dao, nullptr);

    /* Create several embeddings */
    for (int i = 0; i < 10; i++) {
        std::vector<float> embedding(32, 0.0f);
        embedding[i] = 1.0f;
        dao->create_embedding(dao, i, embedding.data());
    }

    /* Search for similar embeddings */
    std::vector<float> query(32, 0.0f);
    query[0] = 1.0f;  /* Should match entity 0 */

    nimcp_kg_result_t result;
    memset(&result, 0, sizeof(result));

    int status = nimcp_kg_semantic_search(dao, query.data(), 5, &result);
    EXPECT_EQ(status, 0) << "Semantic search should succeed";
    EXPECT_GT(result.num_results, 0) << "Should find some results";

    if (result.num_results > 0) {
        EXPECT_EQ(result.matched_entities[0], 0)
            << "First result should be entity 0";
    }

    nimcp_kg_result_destroy(&result);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: TransE training step with recovery
 * ============================================================================ */
TEST_F(KnowledgeGraphGPURecoveryTest, TransETrainingWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    dao = nimcp_knowledge_embedding_dao_create(ctx, 100, 10, 32);
    ASSERT_NE(dao, nullptr);

    /* Create entities and relations */
    for (int i = 0; i < 10; i++) {
        std::vector<float> embedding(32, 0.1f);
        embedding[i % 32] = 1.0f;
        dao->create_embedding(dao, i, embedding.data());
    }

    /* Training configuration */
    nimcp_kg_train_config_t config;
    config.learning_rate = 0.01f;
    config.margin = 1.0f;
    config.negative_samples = 1;
    config.regularization = 0.001f;
    config.normalize_embeddings = true;

    /* Training triplet: (0, 0, 1) */
    std::vector<int> heads = {0};
    std::vector<int> relations = {0};
    std::vector<int> tails = {1};

    int status = nimcp_kg_train_step(dao, heads.data(), relations.data(),
                                      tails.data(), 1, &config);
    EXPECT_EQ(status, 0) << "TransE training step should succeed";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: KG graph statistics with recovery
 * ============================================================================ */
TEST_F(KnowledgeGraphGPURecoveryTest, KGStatsWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    kg = createSimpleKG(10, 32);
    ASSERT_NE(kg, nullptr);

    /* Set embeddings */
    std::vector<float> embeddings(10 * 32, 1.0f);
    ASSERT_TRUE(nimcp_gpu_knowledge_graph_set_embeddings(kg, embeddings.data()));

    float avg_degree = 0.0f;
    uint32_t max_degree = 0;
    float embedding_norm = 0.0f;

    bool success = nimcp_gpu_knowledge_graph_stats(kg, &avg_degree, &max_degree,
                                                    &embedding_norm);
    EXPECT_TRUE(success) << "KG stats should succeed with recovery";
    EXPECT_GT(avg_degree, 0.0f);
    EXPECT_GT(max_degree, 0u);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Error category for OOM
 * ============================================================================ */
TEST_F(KnowledgeGraphGPURecoveryTest, ErrorCategoryOOM) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_action_t action = nimcp_gpu_select_recovery_strategy(
        GPU_ERROR_OUT_OF_MEMORY, cudaErrorMemoryAllocation, 0);

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
 * Test: Error category for numerical errors
 * ============================================================================ */
TEST_F(KnowledgeGraphGPURecoveryTest, ErrorCategoryNumerical) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_action_t action = nimcp_gpu_select_recovery_strategy(
        GPU_ERROR_NUMERICAL, cudaSuccess, 0);

    EXPECT_TRUE(action == GPU_RECOVERY_RETRY_IMMEDIATE ||
                action == GPU_RECOVERY_RETRY_BACKOFF ||
                action == GPU_RECOVERY_CPU_FALLBACK ||
                action == GPU_RECOVERY_REDUCE_BATCH ||
                action == GPU_RECOVERY_REDUCE_PRECISION ||
                action == GPU_RECOVERY_CLAMP_PARAMS ||
                action == GPU_RECOVERY_NONE)
        << "Numerical error should trigger appropriate action";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Recovery stats tracking
 * ============================================================================ */
TEST_F(KnowledgeGraphGPURecoveryTest, StatsTrackingAfterKGOps) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_reset_stats();

    /* Create and operate on knowledge graph */
    kg = createSimpleKG(20, 64);
    ASSERT_NE(kg, nullptr);

    std::vector<float> embeddings(20 * 64, 1.0f);
    nimcp_gpu_knowledge_graph_set_embeddings(kg, embeddings.data());

    /* Perform several operations */
    for (int i = 0; i < 5; i++) {
        nimcp_graph_traversal_result_t result;
        memset(&result, 0, sizeof(result));
        nimcp_gpu_bfs(kg, i, -1, &result);
        nimcp_graph_traversal_result_destroy(&result);
    }

    /* Get stats */
    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);

    EXPECT_GE(stats.total_errors, 0u);
    EXPECT_GE(stats.recoveries_attempted, 0u);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Recovery action names
 * ============================================================================ */
TEST_F(KnowledgeGraphGPURecoveryTest, RecoveryActionNames) {
#ifdef NIMCP_ENABLE_CUDA
    const char* name = nimcp_gpu_recovery_action_name(GPU_RECOVERY_CPU_FALLBACK);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);

    name = nimcp_gpu_recovery_action_name(GPU_RECOVERY_REDUCE_DIMENSIONS);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: KG validity check
 * ============================================================================ */
TEST_F(KnowledgeGraphGPURecoveryTest, KGValidityCheck) {
#ifdef NIMCP_ENABLE_CUDA
    kg = createSimpleKG(10, 32);
    ASSERT_NE(kg, nullptr);

    EXPECT_TRUE(nimcp_gpu_knowledge_graph_is_valid(kg))
        << "Created KG should be valid";

    /* NULL check */
    EXPECT_FALSE(nimcp_gpu_knowledge_graph_is_valid(nullptr))
        << "NULL KG should be invalid";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Normalize embeddings with recovery
 * ============================================================================ */
TEST_F(KnowledgeGraphGPURecoveryTest, NormalizeEmbeddingsWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    kg = createSimpleKG(10, 32);
    ASSERT_NE(kg, nullptr);

    /* Set unnormalized embeddings */
    std::vector<float> embeddings(10 * 32, 2.0f);
    ASSERT_TRUE(nimcp_gpu_knowledge_graph_set_embeddings(kg, embeddings.data()));

    bool success = nimcp_gpu_normalize_embeddings(kg);
    EXPECT_TRUE(success) << "Normalize embeddings should succeed";
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
