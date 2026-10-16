/**
 * @file test_memory_store_e2e.cpp
 * @brief End-to-end tests for NIMCP persistent memory store
 *
 * WHAT: Full-lifecycle scenarios that exercise the store under realistic
 *       workloads -- 10K training steps, persistence across restarts,
 *       consolidation pipelines, edge-device simulation, and search precision.
 * WHY:  Unit and integration tests verify individual and pairwise behavior.
 *       E2E tests confirm the store behaves correctly when all pieces run
 *       together under production-like loads and sequences.
 * HOW:  Google Test with temp databases, large insert counts, timed
 *       assertions, and multi-stage store lifecycle.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <unistd.h>
#include <vector>
#include <algorithm>
#include <chrono>
#include <random>

extern "C" {
#include "memory/nimcp_memory_store.h"
}

// ============================================================================
// Test Fixture
// ============================================================================

class MemoryStoreE2ETest : public ::testing::Test {
protected:
    nimcp_memory_store_t* store = nullptr;
    char db_path[256];

    void SetUp() override {
        snprintf(db_path, sizeof(db_path), "/tmp/nimcp_e2e_%d.db", getpid());
        nimcp_memory_store_config_t config = nimcp_memory_store_config_default();
        config.db_path = db_path;
        config.enable_questdb_sync = false;
        store = nimcp_memory_store_create(&config);
        ASSERT_NE(store, nullptr);
    }

    void TearDown() override {
        if (store) nimcp_memory_store_destroy(store);
        unlink(db_path);
        char wal[270], shm[270];
        snprintf(wal, sizeof(wal), "%s-wal", db_path);
        snprintf(shm, sizeof(shm), "%s-shm", db_path);
        unlink(wal);
        unlink(shm);
    }

    nimcp_engram_record_t make_engram(uint64_t id, const char* label, float importance) {
        nimcp_engram_record_t r = {0};
        r.engram_id = id;
        r.timestamp_us = id * 1000;
        r.memory_type = 0;
        r.state = 0;
        r.importance = importance;
        r.valence = 0.5f;
        r.arousal = 0.3f;
        if (label) strncpy(r.label, label, sizeof(r.label) - 1);
        return r;
    }

    nimcp_concept_record_t make_concept(uint64_t id, const char* label, float activation) {
        nimcp_concept_record_t c = {0};
        c.concept_id = id;
        c.timestamp_us = id * 1000;
        c.base_activation = activation;
        c.access_count = 1;
        if (label) strncpy(c.label, label, sizeof(c.label) - 1);
        return c;
    }

    nimcp_autobio_record_t make_autobio(uint64_t id, const char* narrative) {
        nimcp_autobio_record_t a = {0};
        a.memory_id = id;
        a.timestamp_us = id * 1000;
        a.emotional_intensity = 0.5f;
        if (narrative) strncpy(a.what_happened, narrative, sizeof(a.what_happened) - 1);
        return a;
    }
};

// ============================================================================
// End-to-End Tests
// ============================================================================

TEST_F(MemoryStoreE2ETest, FullBrainMemorySimulation) {
    const int TOTAL_STEPS = 10000;

    for (int step = 0; step < TOTAL_STEPS; step++) {
        char label[64];
        snprintf(label, sizeof(label), "step_%d_engram", step);
        nimcp_engram_record_t e = make_engram(step + 1, label, (float)(step % 100) / 100.0f);
        ASSERT_EQ(nimcp_memory_store_engram_put(store, &e), 0);

        if (step % 50 == 0) {
            snprintf(label, sizeof(label), "concept_step_%d", step);
            nimcp_concept_record_t c = make_concept(step / 50 + 1, label, 0.7f);
            ASSERT_EQ(nimcp_memory_store_concept_put(store, &c), 0);
        }

        if (step % 100 == 0) {
            snprintf(label, sizeof(label), "learned something at step %d", step);
            nimcp_autobio_record_t a = make_autobio(step / 100 + 1, label);
            ASSERT_EQ(nimcp_memory_store_autobio_put(store, &a), 0);
        }

        if (step % 1000 == 999) {
            ASSERT_EQ(nimcp_memory_store_flush(store), 0);
        }
    }
    nimcp_memory_store_flush(store);

    auto start = std::chrono::high_resolution_clock::now();
    for (int q = 0; q < 100; q++) {
        nimcp_engram_record_t out = {0};
        uint64_t id = (uint64_t)(q * 100 + 1);
        nimcp_memory_store_engram_get(store, id, &out);
    }
    auto end = std::chrono::high_resolution_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    EXPECT_LT(elapsed_ms, 1000.0)
        << "100 recall queries took " << elapsed_ms << "ms (should be < 1000ms)";
}

TEST_F(MemoryStoreE2ETest, MemoryPersistenceAcrossRestarts) {
    const int N = 1000;
    for (int i = 0; i < N; i++) {
        char label[64];
        snprintf(label, sizeof(label), "persist_%d", i);
        nimcp_engram_record_t e = make_engram(i + 1, label, (float)i / N);
        ASSERT_EQ(nimcp_memory_store_engram_put(store, &e), 0);
    }
    nimcp_memory_store_checkpoint(store);

    nimcp_memory_store_destroy(store);
    store = nullptr;

    nimcp_memory_store_config_t config = nimcp_memory_store_config_default();
    config.db_path = db_path;
    config.enable_questdb_sync = false;
    store = nimcp_memory_store_create(&config);
    ASSERT_NE(store, nullptr);

    int found = 0;
    for (int i = 0; i < N; i++) {
        nimcp_engram_record_t out = {0};
        if (nimcp_memory_store_engram_get(store, i + 1, &out) == 0) {
            char expected[64];
            snprintf(expected, sizeof(expected), "persist_%d", i);
            EXPECT_STREQ(out.label, expected);
            found++;
        }
    }
    EXPECT_EQ(found, N);
}

TEST_F(MemoryStoreE2ETest, ConsolidationEndToEnd) {
    std::mt19937 rng(42);
    std::normal_distribution<float> dist(0.0f, 1.0f);
    const int dim = 8;

    for (int i = 0; i < 500; i++) {
        char label[64];
        snprintf(label, sizeof(label), "experience_%d", i);
        nimcp_engram_record_t e = make_engram(i + 1, label, (float)(i % 50) / 50.0f);
        e.timestamp_us = (uint64_t)i * 10000;
        if (e.importance > 0.8f) {
            e.state = 2;  // CONSOLIDATED
        }

        std::vector<float> emb(dim);
        for (int j = 0; j < dim; j++) emb[j] = dist(rng);
        float norm = 0.0f;
        for (int j = 0; j < dim; j++) norm += emb[j] * emb[j];
        norm = sqrtf(norm);
        if (norm > 0) {
            for (int j = 0; j < dim; j++) emb[j] /= norm;
        }
        e.embedding = emb.data();
        e.embedding_dim = dim;

        ASSERT_EQ(nimcp_memory_store_engram_put(store, &e), 0);
    }
    nimcp_memory_store_flush(store);

    EXPECT_EQ(nimcp_memory_store_consolidate(store), 0);
    /* gc() returns the number of deleted rows, not 0/error */
    int deleted = nimcp_memory_store_gc(store, 0.3f, 1);
    EXPECT_GE(deleted, 0);

    int important_survived = 0;
    for (int i = 0; i < 500; i++) {
        nimcp_engram_record_t out = {0};
        if (nimcp_memory_store_engram_get(store, i + 1, &out) == 0) {
            important_survived++;
        }
    }
    EXPECT_GT(important_survived, 0);
}

TEST_F(MemoryStoreE2ETest, EdgeDeviceSimulation) {
    nimcp_memory_store_destroy(store);
    store = nullptr;

    char edge_path[256];
    snprintf(edge_path, sizeof(edge_path), "/tmp/nimcp_edge_%d.db", getpid());

    nimcp_memory_store_config_t config = nimcp_memory_store_config_default();
    config.db_path = edge_path;
    config.enable_questdb_sync = false;
    config.hot_cache_size = 64;
    config.write_buffer_size = 10;

    store = nimcp_memory_store_create(&config);
    ASSERT_NE(store, nullptr);

    for (int i = 0; i < 1000; i++) {
        char label[64];
        snprintf(label, sizeof(label), "edge_%d", i);
        nimcp_engram_record_t e = make_engram(i + 1, label, 0.5f);
        ASSERT_EQ(nimcp_memory_store_engram_put(store, &e), 0);
    }
    nimcp_memory_store_flush(store);

    int found = 0;
    for (int i = 0; i < 1000; i++) {
        nimcp_engram_record_t out = {0};
        if (nimcp_memory_store_engram_get(store, i + 1, &out) == 0) found++;
    }
    EXPECT_EQ(found, 1000);

    nimcp_memory_store_destroy(store);
    store = nullptr;
    unlink(edge_path);
    char wal[270], shm[270];
    snprintf(wal, sizeof(wal), "%s-wal", edge_path);
    snprintf(shm, sizeof(shm), "%s-shm", edge_path);
    unlink(wal);
    unlink(shm);
}

TEST_F(MemoryStoreE2ETest, SearchAccuracyBenchmark) {
    std::mt19937 rng(123);
    std::normal_distribution<float> noise(0.0f, 0.1f);
    const int dim = 16;
    const int N = 10000;
    const int NUM_CLUSTERS = 10;
    const int PER_CLUSTER = N / NUM_CLUSTERS;

    std::vector<std::vector<float>> centroids(NUM_CLUSTERS);
    for (int c = 0; c < NUM_CLUSTERS; c++) {
        centroids[c].resize(dim, 0.0f);
        centroids[c][c % dim] = 1.0f;
        centroids[c][(c + 1) % dim] = 0.5f;
        float norm = 0.0f;
        for (int j = 0; j < dim; j++) norm += centroids[c][j] * centroids[c][j];
        norm = sqrtf(norm);
        for (int j = 0; j < dim; j++) centroids[c][j] /= norm;
    }

    std::vector<int> cluster_assignment(N);
    for (int i = 0; i < N; i++) {
        int cluster = i / PER_CLUSTER;
        cluster_assignment[i] = cluster;

        std::vector<float> emb(dim);
        for (int j = 0; j < dim; j++) {
            emb[j] = centroids[cluster][j] + noise(rng);
        }
        float norm = 0.0f;
        for (int j = 0; j < dim; j++) norm += emb[j] * emb[j];
        norm = sqrtf(norm);
        for (int j = 0; j < dim; j++) emb[j] /= norm;

        char label[64];
        snprintf(label, sizeof(label), "cluster%d_item%d", cluster, i % PER_CLUSTER);
        nimcp_engram_record_t e = make_engram(i + 1, label, 0.5f);
        e.embedding = emb.data();
        e.embedding_dim = dim;
        ASSERT_EQ(nimcp_memory_store_engram_put(store, &e), 0);

        if (i % 2000 == 1999) nimcp_memory_store_flush(store);
    }
    nimcp_memory_store_flush(store);

    int total_correct = 0;
    int total_queries = 0;

    for (int c = 0; c < NUM_CLUSTERS; c++) {
        nimcp_memory_search_result_t* results =
            nimcp_memory_store_engram_search_similar(
                store, centroids[c].data(), dim, 10, 0.0f);
        ASSERT_NE(results, nullptr);

        int correct = 0;
        for (uint32_t r = 0; r < results->count && r < 10; r++) {
            uint64_t id = results->ids[r];
            if (id >= 1 && id <= (uint64_t)N) {
                int assigned_cluster = cluster_assignment[id - 1];
                if (assigned_cluster == c) correct++;
            }
        }

        total_correct += correct;
        total_queries++;
        nimcp_memory_search_result_destroy(results);
    }

    float precision_at_10 = (float)total_correct / (total_queries * 10);
    EXPECT_GT(precision_at_10, 0.8f)
        << "precision@10 = " << precision_at_10 << " (expected > 0.8)";
}

TEST_F(MemoryStoreE2ETest, HighFrequencyWriteAndSearch) {
    const int N = 5000;

    for (int i = 0; i < N; i++) {
        char label[64];
        snprintf(label, sizeof(label), "rapid_%d_topic_%d", i, i % 10);
        nimcp_engram_record_t e = make_engram(i + 1, label, 0.5f);
        ASSERT_EQ(nimcp_memory_store_engram_put(store, &e), 0);

        if (i % 100 == 99) {
            nimcp_memory_store_flush(store);
            char query[32];
            snprintf(query, sizeof(query), "topic_%d", i % 10);
            nimcp_memory_search_result_t* results =
                nimcp_memory_store_engram_search_text(store, query, 100);
            ASSERT_NE(results, nullptr);
            EXPECT_GT(results->count, 0u);
            nimcp_memory_search_result_destroy(results);
        }
    }
}

TEST_F(MemoryStoreE2ETest, KnowledgeGraphEndToEnd) {
    const char* animals[] = {"animal", "mammal", "bird", "reptile",
                              "dog", "cat", "eagle", "parrot",
                              "snake", "lizard"};
    for (int i = 0; i < 10; i++) {
        nimcp_concept_record_t c = make_concept(i + 1, animals[i], 0.8f);
        ASSERT_EQ(nimcp_memory_store_concept_put(store, &c), 0);
    }

    nimcp_memory_store_flush(store);  /* FK constraint: concepts must be in DB before relation_put */

    // Define taxonomy relations (using uint32_t relation_type)
    uint64_t rel_id = 1;
    struct { int src; int dst; uint32_t type; } rels[] = {
        {2, 1, 1},   // mammal is_a animal
        {3, 1, 1},   // bird is_a animal
        {4, 1, 1},   // reptile is_a animal
        {5, 2, 1},   // dog is_a mammal
        {6, 2, 1},   // cat is_a mammal
        {7, 3, 1},   // eagle is_a bird
        {8, 3, 1},   // parrot is_a bird
        {9, 4, 1},   // snake is_a reptile
        {10, 4, 1},  // lizard is_a reptile
    };

    for (auto& rel : rels) {
        nimcp_relation_record_t r = {0};
        r.relation_id = rel_id++;
        r.source_concept_id = rel.src;
        r.target_concept_id = rel.dst;
        r.relation_type = rel.type;
        r.strength = 1.0f;
        r.timestamp_us = 1000;
        ASSERT_EQ(nimcp_memory_store_relation_put(store, &r), 0);
    }
    nimcp_memory_store_flush(store);

    // Traverse from "dog" (5) upward -- should reach "mammal" (2) and "animal" (1)
    nimcp_graph_traversal_result_t* results =
        nimcp_memory_store_relation_traverse(store, 5, 3, 0.0f);
    ASSERT_NE(results, nullptr);

    bool found_mammal = false, found_animal = false;
    for (uint32_t i = 0; i < results->count; i++) {
        if (results->node_ids[i] == 2) found_mammal = true;
        if (results->node_ids[i] == 1) found_animal = true;
    }
    EXPECT_TRUE(found_mammal);
    EXPECT_TRUE(found_animal);
    nimcp_memory_graph_result_destroy(results);

    // Text search for "eagle"
    nimcp_memory_search_result_t* text_results =
        nimcp_memory_store_concept_search_text(store, "eagle", 10);
    ASSERT_NE(text_results, nullptr);
    EXPECT_GE(text_results->count, 1u);
    nimcp_memory_search_result_destroy(text_results);
}
