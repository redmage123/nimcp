/**
 * @file test_quantum_semantic.cpp
 * @brief Unit tests for quantum-enhanced semantic memory retrieval
 *
 * Tests:
 * - Lifecycle (create/destroy)
 * - Grover-inspired search
 * - Quantum walk spreading activation
 * - Ternary feature indexing
 * - Statistics tracking
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdlib>
#include <cstring>

// Headers have their own extern "C" guards
#include "cognitive/memory/nimcp_quantum_semantic.h"
#include "cognitive/memory/nimcp_semantic_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class QuantumSemanticTest : public ::testing::Test {
protected:
    quantum_semantic_t ctx;
    quantum_semantic_config_t config;
    semantic_memory_system_t* sem_mem;

    static constexpr uint32_t MAX_CONCEPTS = 64;
    static constexpr uint32_t FEATURE_DIM = 8;

    void SetUp() override {
        config = quantum_semantic_default_config();
        ctx = quantum_semantic_create(&config, MAX_CONCEPTS);

        // Create semantic memory with some test concepts
        sem_mem = semantic_memory_create();
        if (sem_mem) {
            populateTestConcepts();
            quantum_semantic_set_memory(ctx, sem_mem);
        }
    }

    void TearDown() override {
        if (ctx) {
            quantum_semantic_destroy(ctx);
            ctx = nullptr;
        }
        if (sem_mem) {
            semantic_memory_destroy(sem_mem);
            sem_mem = nullptr;
        }
    }

    // Helper to populate test concepts
    void populateTestConcepts() {
        if (!sem_mem) return;

        // Create some test concepts with known features
        float features[FEATURE_DIM];

        // Concept 1: Strong positive features
        for (uint32_t i = 0; i < FEATURE_DIM; i++) features[i] = 1.0f;
        semantic_memory_create_concept(sem_mem, features, FEATURE_DIM, "positive", CONCEPT_OBJECT);

        // Concept 2: Strong negative features
        for (uint32_t i = 0; i < FEATURE_DIM; i++) features[i] = -1.0f;
        semantic_memory_create_concept(sem_mem, features, FEATURE_DIM, "negative", CONCEPT_OBJECT);

        // Concept 3: Mixed features
        for (uint32_t i = 0; i < FEATURE_DIM; i++) features[i] = (i % 2 == 0) ? 0.5f : -0.5f;
        semantic_memory_create_concept(sem_mem, features, FEATURE_DIM, "mixed", CONCEPT_OBJECT);

        // Concept 4-10: Random features
        for (int c = 0; c < 7; c++) {
            for (uint32_t i = 0; i < FEATURE_DIM; i++) {
                features[i] = ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;
            }
            semantic_memory_create_concept(sem_mem, features, FEATURE_DIM, "random", CONCEPT_ABSTRACT);
        }
    }

    // Helper to generate query matching first concept
    void makePositiveQuery(float* query, uint32_t dim) {
        for (uint32_t i = 0; i < dim; i++) query[i] = 1.0f;
    }

    // Helper to generate random query
    void makeRandomQuery(float* query, uint32_t dim) {
        for (uint32_t i = 0; i < dim; i++) {
            query[i] = ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(QuantumSemanticTest, CreateWithValidConfig) {
    ASSERT_NE(ctx, nullptr);
    EXPECT_EQ(ctx->magic, QUANTUM_SEMANTIC_MAGIC);
}

TEST_F(QuantumSemanticTest, CreateWithNullConfig) {
    quantum_semantic_t null_ctx = quantum_semantic_create(nullptr, MAX_CONCEPTS);
    EXPECT_EQ(null_ctx, nullptr);
}

TEST_F(QuantumSemanticTest, CreateWithZeroConcepts) {
    quantum_semantic_t null_ctx = quantum_semantic_create(&config, 0);
    EXPECT_EQ(null_ctx, nullptr);
}

TEST_F(QuantumSemanticTest, DestroyNull) {
    // Should not crash
    quantum_semantic_destroy(nullptr);
}

TEST_F(QuantumSemanticTest, DestroyInvalidMagic) {
    ctx->magic = 0xDEADBEEF;
    quantum_semantic_destroy(ctx);
    ctx = nullptr;  // Prevent double-free
}

//=============================================================================
// Default Configuration Tests
//=============================================================================

TEST_F(QuantumSemanticTest, DefaultConfigValues) {
    quantum_semantic_config_t def = quantum_semantic_default_config();

    EXPECT_EQ(def.mode, QUANTUM_SEM_MODE_GROVER);
    EXPECT_EQ(def.grover_iterations, 0U);  // Auto
    EXPECT_FLOAT_EQ(def.similarity_threshold, 0.7f);
    EXPECT_FLOAT_EQ(def.dissimilarity_threshold, 0.2f);
    EXPECT_EQ(def.walk_steps, 5U);
    EXPECT_FLOAT_EQ(def.decay_per_hop, 0.8f);
    EXPECT_EQ(def.max_results, 10U);
    EXPECT_FLOAT_EQ(def.activation_threshold, 0.3f);
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(QuantumSemanticTest, SetMemoryValid) {
    EXPECT_EQ(ctx->sem_mem, sem_mem);
    EXPECT_GT(ctx->index_size, 0U);
}

TEST_F(QuantumSemanticTest, SetMemoryNull) {
    quantum_semantic_set_memory(ctx, nullptr);
    EXPECT_EQ(ctx->sem_mem, nullptr);
}

TEST_F(QuantumSemanticTest, SetMemoryNullContext) {
    // Should not crash
    quantum_semantic_set_memory(nullptr, sem_mem);
}

//=============================================================================
// Cosine Similarity Tests
//=============================================================================

TEST_F(QuantumSemanticTest, CosineSimilarityIdentical) {
    float a[FEATURE_DIM], b[FEATURE_DIM];
    for (uint32_t i = 0; i < FEATURE_DIM; i++) {
        a[i] = b[i] = 1.0f;
    }

    float sim = quantum_semantic_cosine(a, b, FEATURE_DIM);
    EXPECT_NEAR(sim, 1.0f, 0.001f);
}

TEST_F(QuantumSemanticTest, CosineSimilarityOpposite) {
    float a[FEATURE_DIM], b[FEATURE_DIM];
    for (uint32_t i = 0; i < FEATURE_DIM; i++) {
        a[i] = 1.0f;
        b[i] = -1.0f;
    }

    float sim = quantum_semantic_cosine(a, b, FEATURE_DIM);
    EXPECT_NEAR(sim, -1.0f, 0.001f);
}

TEST_F(QuantumSemanticTest, CosineSimilarityOrthogonal) {
    float a[4] = {1, 0, 0, 0};
    float b[4] = {0, 1, 0, 0};

    float sim = quantum_semantic_cosine(a, b, 4);
    EXPECT_NEAR(sim, 0.0f, 0.001f);
}

TEST_F(QuantumSemanticTest, CosineSimilarityZeroVector) {
    float a[FEATURE_DIM] = {0};
    float b[FEATURE_DIM];
    for (uint32_t i = 0; i < FEATURE_DIM; i++) b[i] = 1.0f;

    float sim = quantum_semantic_cosine(a, b, FEATURE_DIM);
    EXPECT_FLOAT_EQ(sim, 0.0f);
}

//=============================================================================
// Grover Search Tests
//=============================================================================

TEST_F(QuantumSemanticTest, GroverSearchPositiveQuery) {
    float query[FEATURE_DIM];
    makePositiveQuery(query, FEATURE_DIM);

    quantum_semantic_result_t result;
    memset(&result, 0, sizeof(result));

    int err = quantum_semantic_grover_search(ctx, query, FEATURE_DIM, &result);

    EXPECT_EQ(err, 0);
    EXPECT_TRUE(result.success);
    EXPECT_GT(result.count, 0U);

    // First concept should be most similar (all positive features)
    if (result.count > 0) {
        EXPECT_GT(result.similarities[0], 0.5f);
    }

    quantum_semantic_free_result(&result);
}

TEST_F(QuantumSemanticTest, GroverSearchNullContext) {
    float query[FEATURE_DIM];
    quantum_semantic_result_t result;

    int err = quantum_semantic_grover_search(nullptr, query, FEATURE_DIM, &result);
    EXPECT_NE(err, 0);
}

TEST_F(QuantumSemanticTest, GroverSearchNullQuery) {
    quantum_semantic_result_t result;

    int err = quantum_semantic_grover_search(ctx, nullptr, FEATURE_DIM, &result);
    EXPECT_NE(err, 0);
}

TEST_F(QuantumSemanticTest, GroverSearchNullResult) {
    float query[FEATURE_DIM];

    int err = quantum_semantic_grover_search(ctx, query, FEATURE_DIM, nullptr);
    EXPECT_NE(err, 0);
}

TEST_F(QuantumSemanticTest, GroverSearchEmptyMemory) {
    semantic_memory_reset(sem_mem);

    float query[FEATURE_DIM];
    makeRandomQuery(query, FEATURE_DIM);

    quantum_semantic_result_t result;
    memset(&result, 0, sizeof(result));

    int err = quantum_semantic_grover_search(ctx, query, FEATURE_DIM, &result);

    // Should handle empty memory gracefully
    EXPECT_NE(err, 0);  // Error due to no concepts
}

TEST_F(QuantumSemanticTest, GroverSearchMaxResults) {
    // Add many concepts
    float features[FEATURE_DIM];
    for (int i = 0; i < 50; i++) {
        for (uint32_t j = 0; j < FEATURE_DIM; j++) {
            features[j] = 1.0f - 0.01f * i;  // Gradually decreasing similarity
        }
        semantic_memory_create_concept(sem_mem, features, FEATURE_DIM, "test", CONCEPT_OBJECT);
    }

    // Update index
    quantum_semantic_set_memory(ctx, sem_mem);

    float query[FEATURE_DIM];
    makePositiveQuery(query, FEATURE_DIM);

    config.max_results = 5;
    ctx->config = config;

    quantum_semantic_result_t result;
    memset(&result, 0, sizeof(result));

    int err = quantum_semantic_grover_search(ctx, query, FEATURE_DIM, &result);

    EXPECT_EQ(err, 0);
    EXPECT_LE(result.count, 5U);

    quantum_semantic_free_result(&result);
}

TEST_F(QuantumSemanticTest, GroverSearchResultsSorted) {
    float query[FEATURE_DIM];
    makePositiveQuery(query, FEATURE_DIM);

    quantum_semantic_result_t result;
    memset(&result, 0, sizeof(result));

    int err = quantum_semantic_grover_search(ctx, query, FEATURE_DIM, &result);

    EXPECT_EQ(err, 0);

    // Results should be sorted by similarity (descending)
    for (uint32_t i = 1; i < result.count; i++) {
        EXPECT_GE(result.similarities[i-1], result.similarities[i]);
    }

    quantum_semantic_free_result(&result);
}

//=============================================================================
// Quantum Walk Tests
//=============================================================================

TEST_F(QuantumSemanticTest, WalkActivateValid) {
    // Get first concept ID
    ASSERT_GT(sem_mem->concept_count, 0U);
    uint64_t start_id = sem_mem->concepts[0]->id;

    quantum_semantic_result_t result;
    memset(&result, 0, sizeof(result));

    int err = quantum_semantic_walk_activate(ctx, start_id, 1.0f, &result);

    EXPECT_EQ(err, 0);
    EXPECT_TRUE(result.success);
    EXPECT_GT(result.count, 0U);

    // Starting concept should be activated
    bool found_start = false;
    for (uint32_t i = 0; i < result.count; i++) {
        if (result.concept_ids[i] == start_id) {
            found_start = true;
            EXPECT_GT(result.activations[i], 0.0f);
        }
    }
    EXPECT_TRUE(found_start);

    quantum_semantic_free_result(&result);
}

TEST_F(QuantumSemanticTest, WalkActivateNullContext) {
    quantum_semantic_result_t result;

    int err = quantum_semantic_walk_activate(nullptr, 1, 1.0f, &result);
    EXPECT_NE(err, 0);
}

TEST_F(QuantumSemanticTest, WalkActivateInvalidConcept) {
    quantum_semantic_result_t result;
    memset(&result, 0, sizeof(result));

    // Non-existent concept ID
    int err = quantum_semantic_walk_activate(ctx, 99999, 1.0f, &result);
    EXPECT_NE(err, 0);
}

TEST_F(QuantumSemanticTest, WalkActivateZeroActivation) {
    uint64_t start_id = sem_mem->concepts[0]->id;

    quantum_semantic_result_t result;
    memset(&result, 0, sizeof(result));

    int err = quantum_semantic_walk_activate(ctx, start_id, 0.0f, &result);

    EXPECT_EQ(err, 0);
    // With zero initial activation, results may be empty or have very low values
}

//=============================================================================
// Combined Query Tests
//=============================================================================

TEST_F(QuantumSemanticTest, QueryGroverMode) {
    ctx->config.mode = QUANTUM_SEM_MODE_GROVER;

    float query[FEATURE_DIM];
    makePositiveQuery(query, FEATURE_DIM);

    quantum_semantic_result_t result;
    memset(&result, 0, sizeof(result));

    int err = quantum_semantic_query(ctx, query, FEATURE_DIM, &result);

    EXPECT_EQ(err, 0);
    EXPECT_TRUE(result.success);

    quantum_semantic_free_result(&result);
}

TEST_F(QuantumSemanticTest, QueryWalkMode) {
    ctx->config.mode = QUANTUM_SEM_MODE_WALK;

    float query[FEATURE_DIM];
    makePositiveQuery(query, FEATURE_DIM);

    quantum_semantic_result_t result;
    memset(&result, 0, sizeof(result));

    int err = quantum_semantic_query(ctx, query, FEATURE_DIM, &result);

    EXPECT_EQ(err, 0);
    EXPECT_TRUE(result.success);

    quantum_semantic_free_result(&result);
}

TEST_F(QuantumSemanticTest, QueryHybridMode) {
    ctx->config.mode = QUANTUM_SEM_MODE_HYBRID;

    float query[FEATURE_DIM];
    makePositiveQuery(query, FEATURE_DIM);

    quantum_semantic_result_t result;
    memset(&result, 0, sizeof(result));

    int err = quantum_semantic_query(ctx, query, FEATURE_DIM, &result);

    EXPECT_EQ(err, 0);
    EXPECT_TRUE(result.success);

    quantum_semantic_free_result(&result);
}

TEST_F(QuantumSemanticTest, QueryAnnealMode) {
    ctx->config.mode = QUANTUM_SEM_MODE_ANNEAL;

    float query[FEATURE_DIM];
    makePositiveQuery(query, FEATURE_DIM);

    quantum_semantic_result_t result;
    memset(&result, 0, sizeof(result));

    int err = quantum_semantic_query(ctx, query, FEATURE_DIM, &result);

    // Falls back to Grover
    EXPECT_EQ(err, 0);

    quantum_semantic_free_result(&result);
}

//=============================================================================
// Feature Index Tests
//=============================================================================

TEST_F(QuantumSemanticTest, BuildIndex) {
    uint32_t indexed = quantum_semantic_build_index(ctx);

    EXPECT_GT(indexed, 0U);
    EXPECT_NE(ctx->feature_index, nullptr);
}

TEST_F(QuantumSemanticTest, BuildIndexNoMemory) {
    quantum_semantic_set_memory(ctx, nullptr);

    uint32_t indexed = quantum_semantic_build_index(ctx);
    EXPECT_EQ(indexed, 0U);
}

TEST_F(QuantumSemanticTest, BuildIndexNullContext) {
    uint32_t indexed = quantum_semantic_build_index(nullptr);
    EXPECT_EQ(indexed, 0U);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(QuantumSemanticTest, GetStatsInitial) {
    quantum_semantic_stats_t stats;
    quantum_semantic_get_stats(ctx, &stats);

    EXPECT_EQ(stats.total_queries, 0U);
    EXPECT_EQ(stats.concepts_evaluated, 0U);
    EXPECT_EQ(stats.concepts_skipped, 0U);
}

TEST_F(QuantumSemanticTest, GetStatsAfterQuery) {
    float query[FEATURE_DIM];
    makePositiveQuery(query, FEATURE_DIM);

    quantum_semantic_result_t result;
    memset(&result, 0, sizeof(result));
    quantum_semantic_query(ctx, query, FEATURE_DIM, &result);
    quantum_semantic_free_result(&result);

    quantum_semantic_stats_t stats;
    quantum_semantic_get_stats(ctx, &stats);

    EXPECT_EQ(stats.total_queries, 1U);
    EXPECT_GT(stats.concepts_evaluated, 0U);
}

TEST_F(QuantumSemanticTest, ResetStats) {
    // Generate some stats
    float query[FEATURE_DIM];
    makePositiveQuery(query, FEATURE_DIM);

    quantum_semantic_result_t result;
    memset(&result, 0, sizeof(result));
    quantum_semantic_query(ctx, query, FEATURE_DIM, &result);
    quantum_semantic_free_result(&result);

    // Reset
    quantum_semantic_reset_stats(ctx);

    quantum_semantic_stats_t stats;
    quantum_semantic_get_stats(ctx, &stats);

    EXPECT_EQ(stats.total_queries, 0U);
    EXPECT_EQ(stats.concepts_evaluated, 0U);
}

TEST_F(QuantumSemanticTest, SpeedupFactor) {
    // Run multiple queries to accumulate stats
    float query[FEATURE_DIM];
    for (int i = 0; i < 5; i++) {
        makeRandomQuery(query, FEATURE_DIM);

        quantum_semantic_result_t result;
        memset(&result, 0, sizeof(result));
        quantum_semantic_query(ctx, query, FEATURE_DIM, &result);
        quantum_semantic_free_result(&result);
    }

    quantum_semantic_stats_t stats;
    quantum_semantic_get_stats(ctx, &stats);

    // Speedup should be >= 1.0
    EXPECT_GE(stats.avg_speedup, 1.0f);
}

//=============================================================================
// Result Management Tests
//=============================================================================

TEST_F(QuantumSemanticTest, FreeResultNull) {
    // Should not crash
    quantum_semantic_free_result(nullptr);
}

TEST_F(QuantumSemanticTest, FreeResultEmptyFields) {
    quantum_semantic_result_t result;
    memset(&result, 0, sizeof(result));

    // Should not crash
    quantum_semantic_free_result(&result);
}

TEST_F(QuantumSemanticTest, FreeResultClearsFields) {
    float query[FEATURE_DIM];
    makePositiveQuery(query, FEATURE_DIM);

    quantum_semantic_result_t result;
    memset(&result, 0, sizeof(result));
    quantum_semantic_query(ctx, query, FEATURE_DIM, &result);

    EXPECT_NE(result.concept_ids, nullptr);

    quantum_semantic_free_result(&result);

    EXPECT_EQ(result.concept_ids, nullptr);
    EXPECT_EQ(result.similarities, nullptr);
    EXPECT_EQ(result.count, 0U);
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(QuantumSemanticTest, VeryHighThreshold) {
    ctx->config.similarity_threshold = 0.99f;

    float query[FEATURE_DIM];
    makeRandomQuery(query, FEATURE_DIM);

    quantum_semantic_result_t result;
    memset(&result, 0, sizeof(result));
    int err = quantum_semantic_query(ctx, query, FEATURE_DIM, &result);

    EXPECT_EQ(err, 0);
    // May have few or no results due to high threshold

    quantum_semantic_free_result(&result);
}

TEST_F(QuantumSemanticTest, VeryLowThreshold) {
    ctx->config.similarity_threshold = 0.0f;
    ctx->config.dissimilarity_threshold = -1.0f;

    float query[FEATURE_DIM];
    makeRandomQuery(query, FEATURE_DIM);

    quantum_semantic_result_t result;
    memset(&result, 0, sizeof(result));
    int err = quantum_semantic_query(ctx, query, FEATURE_DIM, &result);

    EXPECT_EQ(err, 0);
    // Should match many concepts

    quantum_semantic_free_result(&result);
}

TEST_F(QuantumSemanticTest, SingleConcept) {
    // Reset and add only one concept
    semantic_memory_reset(sem_mem);
    float features[FEATURE_DIM];
    for (uint32_t i = 0; i < FEATURE_DIM; i++) features[i] = 1.0f;
    semantic_memory_create_concept(sem_mem, features, FEATURE_DIM, "only", CONCEPT_OBJECT);

    quantum_semantic_set_memory(ctx, sem_mem);

    float query[FEATURE_DIM];
    makePositiveQuery(query, FEATURE_DIM);

    quantum_semantic_result_t result;
    memset(&result, 0, sizeof(result));
    int err = quantum_semantic_query(ctx, query, FEATURE_DIM, &result);

    EXPECT_EQ(err, 0);
    EXPECT_LE(result.count, 1U);

    quantum_semantic_free_result(&result);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    srand(42);  // Fixed seed for reproducibility
    return RUN_ALL_TESTS();
}
