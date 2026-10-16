/**
 * @file test_memory_store_integration.cpp
 * @brief Integration tests for NIMCP persistent memory store
 *
 * WHAT: Integration tests covering multi-table workflows, vector accuracy,
 *       graph traversal, consolidation pipelines, concurrency, and persistence.
 * WHY:  Unit tests exercise individual operations; integration tests verify
 *       that the subsystems compose correctly under realistic conditions.
 * HOW:  Google Test with temp SQLite databases, multi-threaded writes,
 *       large embeddings, and store reopen verification.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <unistd.h>
#include <vector>
#include <algorithm>
#include <thread>
#include <atomic>
#include <chrono>
#include <random>
#include <ctime>

extern "C" {
#include "memory/nimcp_memory_store.h"
}

// ============================================================================
// Test Fixture
// ============================================================================

class MemoryStoreIntegrationTest : public ::testing::Test {
protected:
    nimcp_memory_store_t* store = nullptr;
    char db_path[256];

    void SetUp() override {
        snprintf(db_path, sizeof(db_path), "/tmp/nimcp_integ_%d.db", getpid());
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

    uint64_t next_relation_id = 1;

    nimcp_relation_record_t make_relation(uint64_t src, uint64_t dst,
                                           uint32_t type, float strength) {
        nimcp_relation_record_t r = {0};
        r.relation_id = next_relation_id++;
        r.source_concept_id = src;
        r.target_concept_id = dst;
        r.relation_type = type;
        r.strength = strength;
        r.timestamp_us = (src + dst) * 1000;
        return r;
    }

    nimcp_autobio_record_t make_autobio(uint64_t id, const char* narrative, bool identity) {
        nimcp_autobio_record_t a = {0};
        a.memory_id = id;
        a.timestamp_us = id * 1000;
        a.identity_defining = identity;
        a.is_core_memory = false;
        a.emotional_intensity = 0.7f;
        if (narrative) strncpy(a.what_happened, narrative, sizeof(a.what_happened) - 1);
        return a;
    }
};

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(MemoryStoreIntegrationTest, FullWriteReadCycle) {
    for (int i = 0; i < 100; i++) {
        char label[64];
        snprintf(label, sizeof(label), "engram_%d", i);
        nimcp_engram_record_t e = make_engram(i + 1, label, (float)i / 100.0f);
        ASSERT_EQ(nimcp_memory_store_engram_put(store, &e), 0);
    }

    for (int i = 0; i < 20; i++) {
        char label[64];
        snprintf(label, sizeof(label), "concept_%d", i);
        nimcp_concept_record_t c = make_concept(i + 1, label, (float)i / 20.0f);
        ASSERT_EQ(nimcp_memory_store_concept_put(store, &c), 0);
    }

    nimcp_memory_store_flush(store);  /* FK constraint: concepts must be in DB before relation_put */

    for (int i = 0; i < 50; i++) {
        nimcp_relation_record_t r = make_relation(
            (i % 20) + 1, ((i + 1) % 20) + 1, 1, 0.5f + (float)i / 100.0f);
        ASSERT_EQ(nimcp_memory_store_relation_put(store, &r), 0);
    }

    for (int i = 0; i < 10; i++) {
        char narr[128];
        snprintf(narr, sizeof(narr), "autobio entry number %d", i);
        nimcp_autobio_record_t a = make_autobio(i + 1, narr, i % 3 == 0);
        ASSERT_EQ(nimcp_memory_store_autobio_put(store, &a), 0);
    }

    ASSERT_EQ(nimcp_memory_store_flush(store), 0);

    for (int i = 0; i < 100; i++) {
        nimcp_engram_record_t out = {0};
        EXPECT_EQ(nimcp_memory_store_engram_get(store, i + 1, &out), 0);
    }
    for (int i = 0; i < 20; i++) {
        nimcp_concept_record_t out = {0};
        EXPECT_EQ(nimcp_memory_store_concept_get(store, i + 1, &out), 0);
    }
    // Autobio verified via text search (no autobio_get in API)
    nimcp_memory_search_result_t* results =
        nimcp_memory_store_autobio_search_text(store, "autobio", 20);
    ASSERT_NE(results, nullptr);
    EXPECT_GE(results->count, 1u);
    nimcp_memory_search_result_destroy(results);
}

TEST_F(MemoryStoreIntegrationTest, VectorSearchAccuracy) {
    std::mt19937 rng(42);
    std::normal_distribution<float> dist(0.0f, 1.0f);
    const int N = 100;
    const int dim = 16;

    std::vector<std::vector<float>> embeddings(N);
    for (int i = 0; i < N; i++) {
        embeddings[i].resize(dim);
        for (int j = 0; j < dim; j++) {
            embeddings[i][j] = dist(rng);
        }
        float norm = 0.0f;
        for (int j = 0; j < dim; j++) norm += embeddings[i][j] * embeddings[i][j];
        norm = sqrtf(norm);
        for (int j = 0; j < dim; j++) embeddings[i][j] /= norm;

        char label[64];
        snprintf(label, sizeof(label), "vec_%d", i);
        nimcp_engram_record_t e = make_engram(i + 1, label, 0.5f);
        e.embedding = embeddings[i].data();
        e.embedding_dim = dim;
        ASSERT_EQ(nimcp_memory_store_engram_put(store, &e), 0);
    }
    nimcp_memory_store_flush(store);

    // Find ground-truth nearest neighbor for engram 0
    int true_nn = -1;
    float best_sim = -2.0f;
    for (int i = 1; i < N; i++) {
        float sim = 0.0f;
        for (int j = 0; j < dim; j++) {
            sim += embeddings[0][j] * embeddings[i][j];
        }
        if (sim > best_sim) {
            best_sim = sim;
            true_nn = i;
        }
    }

    nimcp_memory_search_result_t* results =
        nimcp_memory_store_engram_search_similar(
            store, embeddings[0].data(), dim, 1, 0.0f);
    ASSERT_NE(results, nullptr);
    EXPECT_GE(results->count, 1u);
    if (results->count > 0) {
        uint64_t top_id = results->ids[0];
        EXPECT_TRUE(top_id == (uint64_t)(true_nn + 1) || top_id == 1u);
    }
    nimcp_memory_search_result_destroy(results);
}

TEST_F(MemoryStoreIntegrationTest, FTSAcrossAllTables) {
    nimcp_engram_record_t e = make_engram(1, "quantum physics experiment", 0.8f);
    ASSERT_EQ(nimcp_memory_store_engram_put(store, &e), 0);

    nimcp_concept_record_t c = make_concept(1, "quantum entanglement", 0.9f);
    ASSERT_EQ(nimcp_memory_store_concept_put(store, &c), 0);

    nimcp_autobio_record_t a = {0};
    a.memory_id = 1;
    a.timestamp_us = 1000;
    a.emotional_intensity = 0.5f;
    strncpy(a.what_happened, "studied quantum mechanics today", sizeof(a.what_happened) - 1);
    ASSERT_EQ(nimcp_memory_store_autobio_put(store, &a), 0);
    nimcp_memory_store_flush(store);

    nimcp_memory_search_result_t* r1 =
        nimcp_memory_store_engram_search_text(store, "quantum", 10);
    nimcp_memory_search_result_t* r2 =
        nimcp_memory_store_concept_search_text(store, "quantum", 10);
    nimcp_memory_search_result_t* r3 =
        nimcp_memory_store_autobio_search_text(store, "quantum", 10);

    ASSERT_NE(r1, nullptr);
    ASSERT_NE(r2, nullptr);
    ASSERT_NE(r3, nullptr);
    EXPECT_GE(r1->count, 1u);
    EXPECT_GE(r2->count, 1u);
    EXPECT_GE(r3->count, 1u);

    nimcp_memory_search_result_destroy(r1);
    nimcp_memory_search_result_destroy(r2);
    nimcp_memory_search_result_destroy(r3);
}

TEST_F(MemoryStoreIntegrationTest, GraphTraversalWithRealData) {
    for (int i = 0; i < 10; i++) {
        char label[64];
        snprintf(label, sizeof(label), "node_%d", i);
        nimcp_concept_record_t c = make_concept(i + 1, label, 0.5f);
        ASSERT_EQ(nimcp_memory_store_concept_put(store, &c), 0);
    }
    nimcp_memory_store_flush(store);  /* FK constraint: concepts must be in DB before relation_put */

    for (int i = 0; i < 10; i++) {
        for (int j = 1; j <= 2; j++) {
            int target = ((i + j) % 10) + 1;
            nimcp_relation_record_t r = make_relation(i + 1, target, 1, 0.7f);
            ASSERT_EQ(nimcp_memory_store_relation_put(store, &r), 0);
        }
    }
    nimcp_memory_store_flush(store);

    nimcp_graph_traversal_result_t* results =
        nimcp_memory_store_relation_traverse(store, 1, 2, 0.0f);
    ASSERT_NE(results, nullptr);
    EXPECT_GE(results->count, 2u);
    nimcp_memory_graph_result_destroy(results);
}

TEST_F(MemoryStoreIntegrationTest, ConsolidationPipeline) {
    for (int i = 0; i < 20; i++) {
        char label[64];
        snprintf(label, sizeof(label), "experience_%d", i);
        nimcp_engram_record_t e = make_engram(i + 1, label, (float)(i + 1) / 20.0f);
        e.state = (i >= 15) ? 2 : 0;
        float emb[4] = {(float)i / 20.0f, 0.5f, 0.3f, 0.1f};
        e.embedding = emb;
        e.embedding_dim = 4;
        ASSERT_EQ(nimcp_memory_store_engram_put(store, &e), 0);
    }
    nimcp_memory_store_flush(store);

    EXPECT_EQ(nimcp_memory_store_consolidate(store), 0);

    for (int i = 0; i < 20; i++) {
        nimcp_engram_record_t out = {0};
        EXPECT_EQ(nimcp_memory_store_engram_get(store, i + 1, &out), 0);
    }
}

TEST_F(MemoryStoreIntegrationTest, GCPipeline) {
    for (int i = 0; i < 500; i++) {
        char label[64];
        snprintf(label, sizeof(label), "gc_%d", i);
        float importance = (i % 5 == 0) ? 0.95f : 0.01f;
        nimcp_engram_record_t e = make_engram(i + 1, label, importance);
        e.timestamp_us = 1;
        ASSERT_EQ(nimcp_memory_store_engram_put(store, &e), 0);
    }
    nimcp_memory_store_flush(store);

    /* gc() returns the number of deleted rows (not 0/error).
     * 500 engrams: 100 have importance=0.95 (every 5th), 400 have importance=0.01.
     * GC deletes where importance < 0.5 AND recall_count=0 AND timestamp < cutoff.
     * With max_age_days=0, cutoff=now_us, so all with timestamp=1 qualify. */
    int deleted = nimcp_memory_store_gc(store, 0.5f, 0);
    EXPECT_GE(deleted, 0);  /* non-negative = success */

    int survivors = 0;
    for (int i = 0; i < 500; i++) {
        nimcp_engram_record_t out = {0};
        if (nimcp_memory_store_engram_get(store, i + 1, &out) == 0) {
            survivors++;
        }
    }
    EXPECT_GE(survivors, 90);
    EXPECT_LE(survivors, 110);
}

TEST_F(MemoryStoreIntegrationTest, ConcurrentReadsAndWrites) {
    for (int i = 0; i < 100; i++) {
        nimcp_engram_record_t e = make_engram(i + 1, "concurrent", 0.5f);
        ASSERT_EQ(nimcp_memory_store_engram_put(store, &e), 0);
    }
    nimcp_memory_store_flush(store);

    std::atomic<int> errors{0};
    std::atomic<int> writes_done{0};
    std::atomic<int> reads_done{0};

    auto writer = [&](int thread_id) {
        for (int i = 0; i < 50; i++) {
            uint64_t id = 1000 + thread_id * 100 + i;
            nimcp_engram_record_t e = {0};
            e.engram_id = id;
            e.timestamp_us = id * 1000;
            e.importance = 0.5f;
            snprintf(e.label, sizeof(e.label), "writer_%d_%d", thread_id, i);
            if (nimcp_memory_store_engram_put(store, &e) != 0) {
                errors++;
            }
            writes_done++;
        }
    };

    auto reader = [&](int /*thread_id*/) {
        for (int i = 0; i < 50; i++) {
            uint64_t id = (i % 100) + 1;
            nimcp_engram_record_t out = {0};
            nimcp_memory_store_engram_get(store, id, &out);
            reads_done++;
        }
    };

    std::thread w1(writer, 0), w2(writer, 1);
    std::thread r1(reader, 0), r2(reader, 1);

    w1.join(); w2.join();
    r1.join(); r2.join();

    EXPECT_EQ(errors.load(), 0);
    EXPECT_EQ(writes_done.load(), 100);
    EXPECT_EQ(reads_done.load(), 100);
}

TEST_F(MemoryStoreIntegrationTest, LargeEmbeddings) {
    const int dim = 1024;
    std::vector<float> emb(dim);
    for (int i = 0; i < dim; i++) emb[i] = sinf((float)i * 0.01f);
    float norm = 0.0f;
    for (int i = 0; i < dim; i++) norm += emb[i] * emb[i];
    norm = sqrtf(norm);
    for (int i = 0; i < dim; i++) emb[i] /= norm;

    nimcp_engram_record_t e = make_engram(1, "large embedding", 0.5f);
    e.embedding = emb.data();
    e.embedding_dim = dim;
    ASSERT_EQ(nimcp_memory_store_engram_put(store, &e), 0);
    nimcp_memory_store_flush(store);

    nimcp_memory_search_result_t* results =
        nimcp_memory_store_engram_search_similar(store, emb.data(), dim, 1, 0.0f);
    ASSERT_NE(results, nullptr);
    EXPECT_GE(results->count, 1u);
    if (results->count > 0) {
        EXPECT_NEAR(results->distances[0], 0.0f, 0.01f);
    }
    nimcp_memory_search_result_destroy(results);
}

TEST_F(MemoryStoreIntegrationTest, BloomFilterIntegration) {
    int duplicates_detected = 0;
    for (int i = 0; i < 10000; i++) {
        float emb[4] = {(float)i, (float)(i * 2 + 1), (float)(i * 3 + 2), (float)(i * 5 + 3)};
        if (nimcp_memory_store_bloom_check(store, emb, 4)) {
            duplicates_detected++;
        }
        nimcp_memory_store_bloom_add(store, emb, 4);
    }
    EXPECT_LT(duplicates_detected, 500);
}

TEST_F(MemoryStoreIntegrationTest, StoreCheckpointAndReopen) {
    for (int i = 0; i < 50; i++) {
        char label[64];
        snprintf(label, sizeof(label), "persist_%d", i);
        nimcp_engram_record_t e = make_engram(i + 1, label, 0.5f);
        ASSERT_EQ(nimcp_memory_store_engram_put(store, &e), 0);
    }

    ASSERT_EQ(nimcp_memory_store_checkpoint(store), 0);
    nimcp_memory_store_destroy(store);
    store = nullptr;

    nimcp_memory_store_config_t config = nimcp_memory_store_config_default();
    config.db_path = db_path;
    config.enable_questdb_sync = false;
    store = nimcp_memory_store_create(&config);
    ASSERT_NE(store, nullptr);

    for (int i = 0; i < 50; i++) {
        nimcp_engram_record_t out = {0};
        EXPECT_EQ(nimcp_memory_store_engram_get(store, i + 1, &out), 0)
            << "Failed to read engram " << i + 1 << " after reopen";
    }
}

TEST_F(MemoryStoreIntegrationTest, WALModePerformance) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10000; i++) {
        char label[64];
        snprintf(label, sizeof(label), "perf_%d", i);
        nimcp_engram_record_t e = make_engram(i + 1, label, 0.5f);
        ASSERT_EQ(nimcp_memory_store_engram_put(store, &e), 0);
    }
    nimcp_memory_store_flush(store);

    auto end = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(end - start).count();
    EXPECT_LT(elapsed, 5.0) << "WAL write performance too slow: "
                             << elapsed << " seconds for 10000 engrams";
}

TEST_F(MemoryStoreIntegrationTest, MultiTableTextSearchRelevance) {
    const char* topics[] = {"photosynthesis", "chloroplast", "mitochondria",
                            "ATP synthesis", "electron transport"};
    for (int i = 0; i < 5; i++) {
        nimcp_engram_record_t e = make_engram(i + 1, topics[i], 0.8f);
        ASSERT_EQ(nimcp_memory_store_engram_put(store, &e), 0);
        nimcp_concept_record_t c = make_concept(i + 1, topics[i], 0.7f);
        ASSERT_EQ(nimcp_memory_store_concept_put(store, &c), 0);
    }
    nimcp_memory_store_flush(store);

    nimcp_memory_search_result_t* results =
        nimcp_memory_store_engram_search_text(store, "photosynthesis", 10);
    ASSERT_NE(results, nullptr);
    EXPECT_EQ(results->count, 1u);
    if (results->count > 0) {
        EXPECT_EQ(results->ids[0], 1u);
    }
    nimcp_memory_search_result_destroy(results);
}

TEST_F(MemoryStoreIntegrationTest, ConsolidateAndGCTogether) {
    /* Get current time for realistic timestamps */
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    uint64_t now_us = (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;

    for (int i = 0; i < 100; i++) {
        nimcp_engram_record_t e = make_engram(i + 1, "mixed", 0.5f);
        /* Old engrams (0-49) get ancient timestamps; new ones (50-99) get current time */
        e.timestamp_us = (i < 50) ? 1 : now_us;
        e.importance = (i % 10 == 0) ? 0.95f : 0.02f;
        e.state = (i % 10 == 0) ? 2 : 0;
        ASSERT_EQ(nimcp_memory_store_engram_put(store, &e), 0);
    }
    nimcp_memory_store_flush(store);

    EXPECT_EQ(nimcp_memory_store_consolidate(store), 0);
    /* gc() returns deleted count, not 0/error. max_age_days=1 means
     * cutoff = now - 1 day. Old engrams (timestamp=1) are ancient and
     * will be deleted if importance < 0.5; new ones (timestamp=now) survive. */
    int deleted = nimcp_memory_store_gc(store, 0.5f, 1);
    EXPECT_GE(deleted, 0);

    /* New engrams (51-100) should survive: their timestamp is current (within 1 day) */
    for (int i = 50; i < 100; i++) {
        nimcp_engram_record_t out = {0};
        EXPECT_EQ(nimcp_memory_store_engram_get(store, i + 1, &out), 0);
    }
}
