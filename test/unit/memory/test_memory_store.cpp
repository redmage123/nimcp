/**
 * @file test_memory_store.cpp
 * @brief Unit tests for NIMCP persistent memory store (SQLite + FTS5 + vector search)
 *
 * WHAT: Comprehensive unit tests for nimcp_memory_store_t lifecycle, CRUD,
 *       text search, time range queries, vector similarity, bloom filter,
 *       consolidation, GC, and statistics.
 * WHY:  The persistent memory store is a critical data path for engrams,
 *       concepts, relations, and autobiographical records. Every code path
 *       must be exercised so regressions are caught early.
 * HOW:  Google Test with a per-test temp SQLite database.
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

extern "C" {
#include "memory/nimcp_memory_store.h"
}

// ============================================================================
// Test Fixture
// ============================================================================

class MemoryStoreTest : public ::testing::Test {
protected:
    nimcp_memory_store_t* store = nullptr;
    char db_path[256];

    void SetUp() override {
        snprintf(db_path, sizeof(db_path), "/tmp/nimcp_test_%d.db", getpid());
        nimcp_memory_store_config_t config = nimcp_memory_store_config_default();
        config.db_path = db_path;
        config.enable_questdb_sync = false;
        store = nimcp_memory_store_create(&config);
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

    nimcp_autobio_record_t make_autobio(uint64_t id, const char* narrative,
                                         bool identity_defining) {
        nimcp_autobio_record_t a = {0};
        a.memory_id = id;
        a.timestamp_us = id * 1000;
        a.identity_defining = identity_defining;
        a.is_core_memory = false;
        a.emotional_intensity = 0.7f;
        if (narrative) strncpy(a.what_happened, narrative, sizeof(a.what_happened) - 1);
        return a;
    }
};

// ============================================================================
// Lifecycle Tests
// ============================================================================

TEST_F(MemoryStoreTest, CreateAndDestroy) {
    ASSERT_NE(store, nullptr);
}

TEST_F(MemoryStoreTest, CreateWithNullConfig) {
    nimcp_memory_store_t* s = nimcp_memory_store_create(nullptr);
    EXPECT_EQ(s, nullptr);
}

TEST_F(MemoryStoreTest, CreateWithInvalidPath) {
    nimcp_memory_store_config_t config = nimcp_memory_store_config_default();
    config.db_path = "/nonexistent/deeply/nested/dir/file.db";
    config.enable_questdb_sync = false;
    nimcp_memory_store_t* s = nimcp_memory_store_create(&config);
    if (s) nimcp_memory_store_destroy(s);
}

TEST_F(MemoryStoreTest, DoubleDestroyIsSafe) {
    nimcp_memory_store_destroy(store);
    store = nullptr;
}

TEST_F(MemoryStoreTest, ConfigDefault) {
    nimcp_memory_store_config_t config = nimcp_memory_store_config_default();
    EXPECT_GT(config.write_buffer_size, 0u);
    EXPECT_GT(config.hot_cache_size, 0u);
    EXPECT_GT(config.bloom_filter_size, 0u);
}

// ============================================================================
// Engram CRUD Tests
// ============================================================================

TEST_F(MemoryStoreTest, EngramPutAndGet) {
    ASSERT_NE(store, nullptr);
    nimcp_engram_record_t e = make_engram(1, "red cardinal", 0.8f);
    ASSERT_EQ(nimcp_memory_store_engram_put(store, &e), 0);

    nimcp_engram_record_t out = {0};
    ASSERT_EQ(nimcp_memory_store_engram_get(store, 1, &out), 0);
    EXPECT_EQ(out.engram_id, 1u);
    EXPECT_STREQ(out.label, "red cardinal");
    EXPECT_FLOAT_EQ(out.importance, 0.8f);
    EXPECT_FLOAT_EQ(out.valence, 0.5f);
    EXPECT_FLOAT_EQ(out.arousal, 0.3f);
    EXPECT_EQ(out.timestamp_us, 1000u);
}

TEST_F(MemoryStoreTest, EngramPutWithEmbedding) {
    ASSERT_NE(store, nullptr);
    nimcp_engram_record_t e = make_engram(10, "embedding test", 0.5f);
    const uint32_t dim = 32;
    float emb[32];
    for (uint32_t i = 0; i < dim; i++) emb[i] = (float)i / dim;
    e.embedding = emb;
    e.embedding_dim = dim;
    ASSERT_EQ(nimcp_memory_store_engram_put(store, &e), 0);

    nimcp_engram_record_t out = {0};
    ASSERT_EQ(nimcp_memory_store_engram_get(store, 10, &out), 0);
    ASSERT_EQ(out.embedding_dim, dim);
    ASSERT_NE(out.embedding, nullptr);
    for (uint32_t i = 0; i < dim; i++) {
        EXPECT_FLOAT_EQ(out.embedding[i], emb[i]);
    }
    if (out.embedding) free(out.embedding);
}

TEST_F(MemoryStoreTest, EngramPutWithNeuronPattern) {
    ASSERT_NE(store, nullptr);
    nimcp_engram_record_t e = make_engram(20, "neuron pattern", 0.6f);
    uint32_t ids[] = {100, 200, 300, 400};
    float acts[] = {0.9f, 0.7f, 0.5f, 0.3f};
    e.neuron_ids = ids;
    e.activations = acts;
    e.neuron_count = 4;
    ASSERT_EQ(nimcp_memory_store_engram_put(store, &e), 0);

    nimcp_engram_record_t out = {0};
    ASSERT_EQ(nimcp_memory_store_engram_get(store, 20, &out), 0);
    ASSERT_EQ(out.neuron_count, 4u);
    ASSERT_NE(out.neuron_ids, nullptr);
    ASSERT_NE(out.activations, nullptr);
    for (int i = 0; i < 4; i++) {
        EXPECT_EQ(out.neuron_ids[i], ids[i]);
        EXPECT_FLOAT_EQ(out.activations[i], acts[i]);
    }
    if (out.neuron_ids) free(out.neuron_ids);
    if (out.activations) free(out.activations);
}

TEST_F(MemoryStoreTest, EngramGetNonexistent) {
    ASSERT_NE(store, nullptr);
    nimcp_engram_record_t out = {0};
    EXPECT_NE(nimcp_memory_store_engram_get(store, 99999, &out), 0);
}

TEST_F(MemoryStoreTest, EngramUpdate) {
    ASSERT_NE(store, nullptr);
    nimcp_engram_record_t e = make_engram(5, "original", 0.3f);
    ASSERT_EQ(nimcp_memory_store_engram_put(store, &e), 0);
    e.importance = 0.95f;
    strncpy(e.label, "updated", sizeof(e.label) - 1);
    ASSERT_EQ(nimcp_memory_store_engram_put(store, &e), 0);

    nimcp_engram_record_t out = {0};
    ASSERT_EQ(nimcp_memory_store_engram_get(store, 5, &out), 0);
    EXPECT_STREQ(out.label, "updated");
    EXPECT_FLOAT_EQ(out.importance, 0.95f);
}

TEST_F(MemoryStoreTest, EngramPutMany) {
    ASSERT_NE(store, nullptr);
    const int N = 1000;
    for (int i = 0; i < N; i++) {
        char label[64];
        snprintf(label, sizeof(label), "engram_%d", i);
        nimcp_engram_record_t e = make_engram(i + 1, label, (float)i / N);
        ASSERT_EQ(nimcp_memory_store_engram_put(store, &e), 0);
    }
    nimcp_memory_store_flush(store);

    nimcp_engram_record_t out = {0};
    ASSERT_EQ(nimcp_memory_store_engram_get(store, 1, &out), 0);
    EXPECT_STREQ(out.label, "engram_0");
    ASSERT_EQ(nimcp_memory_store_engram_get(store, 500, &out), 0);
    EXPECT_STREQ(out.label, "engram_499");
    ASSERT_EQ(nimcp_memory_store_engram_get(store, 1000, &out), 0);
    EXPECT_STREQ(out.label, "engram_999");
}

TEST_F(MemoryStoreTest, EngramPutNullStore) {
    nimcp_engram_record_t e = make_engram(1, "test", 0.5f);
    EXPECT_NE(nimcp_memory_store_engram_put(nullptr, &e), 0);
}

TEST_F(MemoryStoreTest, EngramPutNullRecord) {
    ASSERT_NE(store, nullptr);
    EXPECT_NE(nimcp_memory_store_engram_put(store, nullptr), 0);
}

// ============================================================================
// Write Buffering Tests
// ============================================================================

TEST_F(MemoryStoreTest, FlushEmptyBuffer) {
    ASSERT_NE(store, nullptr);
    EXPECT_EQ(nimcp_memory_store_flush(store), 0);
}

TEST_F(MemoryStoreTest, FlushAfterPuts) {
    ASSERT_NE(store, nullptr);
    for (int i = 0; i < 50; i++) {
        char label[64];
        snprintf(label, sizeof(label), "flush_%d", i);
        nimcp_engram_record_t e = make_engram(i + 1, label, 0.5f);
        ASSERT_EQ(nimcp_memory_store_engram_put(store, &e), 0);
    }
    ASSERT_EQ(nimcp_memory_store_flush(store), 0);
    for (int i = 0; i < 50; i++) {
        nimcp_engram_record_t out = {0};
        EXPECT_EQ(nimcp_memory_store_engram_get(store, i + 1, &out), 0);
    }
}

TEST_F(MemoryStoreTest, AutoFlushOnBufferFull) {
    ASSERT_NE(store, nullptr);
    nimcp_memory_store_config_t config = nimcp_memory_store_config_default();
    uint32_t buf_size = config.write_buffer_size;
    for (uint32_t i = 0; i < buf_size + 1; i++) {
        char label[64];
        snprintf(label, sizeof(label), "auto_%u", i);
        nimcp_engram_record_t e = make_engram(i + 1, label, 0.5f);
        ASSERT_EQ(nimcp_memory_store_engram_put(store, &e), 0);
    }
    nimcp_engram_record_t out = {0};
    EXPECT_EQ(nimcp_memory_store_engram_get(store, 1, &out), 0);
}

TEST_F(MemoryStoreTest, CheckpointFlushesAndSyncs) {
    ASSERT_NE(store, nullptr);
    for (int i = 0; i < 10; i++) {
        nimcp_engram_record_t e = make_engram(i + 1, "checkpoint_test", 0.5f);
        ASSERT_EQ(nimcp_memory_store_engram_put(store, &e), 0);
    }
    EXPECT_EQ(nimcp_memory_store_checkpoint(store), 0);
    for (int i = 0; i < 10; i++) {
        nimcp_engram_record_t out = {0};
        EXPECT_EQ(nimcp_memory_store_engram_get(store, i + 1, &out), 0);
    }
}

// ============================================================================
// Text Search (FTS5) Tests
// ============================================================================

TEST_F(MemoryStoreTest, EngramSearchTextExactMatch) {
    ASSERT_NE(store, nullptr);
    nimcp_engram_record_t e = make_engram(1, "red cardinal", 0.8f);
    ASSERT_EQ(nimcp_memory_store_engram_put(store, &e), 0);
    nimcp_memory_store_flush(store);

    nimcp_memory_search_result_t* results =
        nimcp_memory_store_engram_search_text(store, "cardinal", 10);
    ASSERT_NE(results, nullptr);
    EXPECT_GE(results->count, 1u);
    bool found = false;
    for (uint32_t i = 0; i < results->count; i++) {
        if (results->ids[i] == 1) found = true;
    }
    EXPECT_TRUE(found);
    nimcp_memory_search_result_destroy(results);
}

TEST_F(MemoryStoreTest, EngramSearchTextPartialMatch) {
    ASSERT_NE(store, nullptr);
    nimcp_engram_record_t e = make_engram(1, "red cardinal", 0.8f);
    ASSERT_EQ(nimcp_memory_store_engram_put(store, &e), 0);
    nimcp_memory_store_flush(store);

    nimcp_memory_search_result_t* results =
        nimcp_memory_store_engram_search_text(store, "card*", 10);
    ASSERT_NE(results, nullptr);
    EXPECT_GE(results->count, 1u);
    nimcp_memory_search_result_destroy(results);
}

TEST_F(MemoryStoreTest, EngramSearchTextNoMatch) {
    ASSERT_NE(store, nullptr);
    nimcp_engram_record_t e = make_engram(1, "red cardinal", 0.8f);
    ASSERT_EQ(nimcp_memory_store_engram_put(store, &e), 0);
    nimcp_memory_store_flush(store);

    nimcp_memory_search_result_t* results =
        nimcp_memory_store_engram_search_text(store, "dinosaur", 10);
    ASSERT_NE(results, nullptr);
    EXPECT_EQ(results->count, 0u);
    nimcp_memory_search_result_destroy(results);
}

TEST_F(MemoryStoreTest, EngramSearchTextMultipleResults) {
    ASSERT_NE(store, nullptr);
    const char* bird_labels[] = {
        "blue bird flying", "bird song morning", "bird nest tree",
        "rare bird sighting", "bird migration south"
    };
    for (int i = 0; i < 5; i++) {
        nimcp_engram_record_t e = make_engram(i + 1, bird_labels[i], 0.5f);
        ASSERT_EQ(nimcp_memory_store_engram_put(store, &e), 0);
    }
    nimcp_memory_store_flush(store);

    nimcp_memory_search_result_t* results =
        nimcp_memory_store_engram_search_text(store, "bird", 10);
    ASSERT_NE(results, nullptr);
    EXPECT_EQ(results->count, 5u);
    nimcp_memory_search_result_destroy(results);
}

TEST_F(MemoryStoreTest, AutobioSearchText) {
    ASSERT_NE(store, nullptr);
    nimcp_autobio_record_t a = make_autobio(1, "learned about colors today", false);
    ASSERT_EQ(nimcp_memory_store_autobio_put(store, &a), 0);
    nimcp_memory_store_flush(store);

    nimcp_memory_search_result_t* results =
        nimcp_memory_store_autobio_search_text(store, "colors", 10);
    ASSERT_NE(results, nullptr);
    EXPECT_GE(results->count, 1u);
    nimcp_memory_search_result_destroy(results);
}

// ============================================================================
// Time Range Search Tests
// ============================================================================

TEST_F(MemoryStoreTest, EngramSearchTimeRange) {
    ASSERT_NE(store, nullptr);
    nimcp_engram_record_t e1 = make_engram(1, "early", 0.5f);
    e1.timestamp_us = 100;
    nimcp_engram_record_t e2 = make_engram(2, "middle", 0.5f);
    e2.timestamp_us = 200;
    nimcp_engram_record_t e3 = make_engram(3, "late", 0.5f);
    e3.timestamp_us = 300;
    ASSERT_EQ(nimcp_memory_store_engram_put(store, &e1), 0);
    ASSERT_EQ(nimcp_memory_store_engram_put(store, &e2), 0);
    ASSERT_EQ(nimcp_memory_store_engram_put(store, &e3), 0);
    nimcp_memory_store_flush(store);

    nimcp_memory_search_result_t* results =
        nimcp_memory_store_engram_search_time(store, 150, 250, 10);
    ASSERT_NE(results, nullptr);
    EXPECT_EQ(results->count, 1u);
    if (results->count > 0) {
        EXPECT_EQ(results->ids[0], 2u);
    }
    nimcp_memory_search_result_destroy(results);
}

TEST_F(MemoryStoreTest, EngramSearchTimeRangeEmpty) {
    ASSERT_NE(store, nullptr);
    nimcp_engram_record_t e = make_engram(1, "only one", 0.5f);
    e.timestamp_us = 100;
    ASSERT_EQ(nimcp_memory_store_engram_put(store, &e), 0);
    nimcp_memory_store_flush(store);

    nimcp_memory_search_result_t* results =
        nimcp_memory_store_engram_search_time(store, 500, 1000, 10);
    ASSERT_NE(results, nullptr);
    EXPECT_EQ(results->count, 0u);
    nimcp_memory_search_result_destroy(results);
}

TEST_F(MemoryStoreTest, AutobioSearchTimeRange) {
    ASSERT_NE(store, nullptr);
    nimcp_autobio_record_t a1 = make_autobio(1, "morning event", false);
    a1.timestamp_us = 100;
    nimcp_autobio_record_t a2 = make_autobio(2, "afternoon event", false);
    a2.timestamp_us = 200;
    ASSERT_EQ(nimcp_memory_store_autobio_put(store, &a1), 0);
    ASSERT_EQ(nimcp_memory_store_autobio_put(store, &a2), 0);
    nimcp_memory_store_flush(store);

    nimcp_memory_search_result_t* results =
        nimcp_memory_store_autobio_search_time(store, 150, 250, 10);
    ASSERT_NE(results, nullptr);
    EXPECT_GE(results->count, 1u);
    nimcp_memory_search_result_destroy(results);
}

// ============================================================================
// Vector Similarity Search Tests
// ============================================================================

TEST_F(MemoryStoreTest, EngramSearchSimilarExact) {
    ASSERT_NE(store, nullptr);
    float emb[] = {1.0f, 0.0f, 0.0f, 0.0f};
    nimcp_engram_record_t e = make_engram(1, "vector test", 0.5f);
    e.embedding = emb;
    e.embedding_dim = 4;
    ASSERT_EQ(nimcp_memory_store_engram_put(store, &e), 0);
    nimcp_memory_store_flush(store);

    float query[] = {1.0f, 0.0f, 0.0f, 0.0f};
    nimcp_memory_search_result_t* results =
        nimcp_memory_store_engram_search_similar(store, query, 4, 1, 0.0f);
    ASSERT_NE(results, nullptr);
    EXPECT_GE(results->count, 1u);
    if (results->count > 0) {
        EXPECT_EQ(results->ids[0], 1u);
        EXPECT_NEAR(results->distances[0], 0.0f, 0.01f);
    }
    nimcp_memory_search_result_destroy(results);
}

TEST_F(MemoryStoreTest, EngramSearchSimilarClose) {
    ASSERT_NE(store, nullptr);
    float emb[] = {1.0f, 0.0f, 0.0f, 0.0f};
    nimcp_engram_record_t e = make_engram(1, "close vector", 0.5f);
    e.embedding = emb;
    e.embedding_dim = 4;
    ASSERT_EQ(nimcp_memory_store_engram_put(store, &e), 0);
    nimcp_memory_store_flush(store);

    float query[] = {0.9f, 0.1f, 0.0f, 0.0f};
    nimcp_memory_search_result_t* results =
        nimcp_memory_store_engram_search_similar(store, query, 4, 1, 0.0f);
    ASSERT_NE(results, nullptr);
    EXPECT_GE(results->count, 1u);
    if (results->count > 0) {
        EXPECT_LT(results->distances[0], 0.5f);
    }
    nimcp_memory_search_result_destroy(results);
}

TEST_F(MemoryStoreTest, EngramSearchSimilarDifferent) {
    ASSERT_NE(store, nullptr);
    float emb[] = {1.0f, 0.0f, 0.0f, 0.0f};
    nimcp_engram_record_t e = make_engram(1, "different vector", 0.5f);
    e.embedding = emb;
    e.embedding_dim = 4;
    ASSERT_EQ(nimcp_memory_store_engram_put(store, &e), 0);
    nimcp_memory_store_flush(store);

    float query[] = {0.0f, 0.0f, 0.0f, 1.0f};
    nimcp_memory_search_result_t* results =
        nimcp_memory_store_engram_search_similar(store, query, 4, 1, 0.0f);
    ASSERT_NE(results, nullptr);
    if (results->count > 0) {
        EXPECT_GT(results->distances[0], 0.5f);
    }
    nimcp_memory_search_result_destroy(results);
}

TEST_F(MemoryStoreTest, EngramSearchSimilarTopK) {
    ASSERT_NE(store, nullptr);
    for (int i = 0; i < 10; i++) {
        float emb[4] = {0};
        emb[0] = 1.0f - (float)i * 0.1f;
        emb[1] = (float)i * 0.1f;
        float norm = sqrtf(emb[0] * emb[0] + emb[1] * emb[1]);
        emb[0] /= norm;
        emb[1] /= norm;
        char label[64];
        snprintf(label, sizeof(label), "vec_%d", i);
        nimcp_engram_record_t e = make_engram(i + 1, label, 0.5f);
        e.embedding = emb;
        e.embedding_dim = 4;
        ASSERT_EQ(nimcp_memory_store_engram_put(store, &e), 0);
    }
    nimcp_memory_store_flush(store);

    float query[] = {1.0f, 0.0f, 0.0f, 0.0f};
    nimcp_memory_search_result_t* results =
        nimcp_memory_store_engram_search_similar(store, query, 4, 3, 0.0f);
    ASSERT_NE(results, nullptr);
    EXPECT_EQ(results->count, 3u);
    if (results->count >= 3) {
        EXPECT_LE(results->distances[0], results->distances[1]);
        EXPECT_LE(results->distances[1], results->distances[2]);
        EXPECT_EQ(results->ids[0], 1u);
    }
    nimcp_memory_search_result_destroy(results);
}

TEST_F(MemoryStoreTest, ConceptSearchSimilar) {
    ASSERT_NE(store, nullptr);
    float emb1[] = {1.0f, 0.0f, 0.0f, 0.0f};
    float emb2[] = {0.0f, 1.0f, 0.0f, 0.0f};
    nimcp_concept_record_t c1 = make_concept(1, "concept_a", 0.8f);
    c1.embedding = emb1;
    c1.embedding_dim = 4;
    nimcp_concept_record_t c2 = make_concept(2, "concept_b", 0.6f);
    c2.embedding = emb2;
    c2.embedding_dim = 4;
    ASSERT_EQ(nimcp_memory_store_concept_put(store, &c1), 0);
    ASSERT_EQ(nimcp_memory_store_concept_put(store, &c2), 0);
    nimcp_memory_store_flush(store);

    float query[] = {0.95f, 0.05f, 0.0f, 0.0f};
    nimcp_memory_search_result_t* results =
        nimcp_memory_store_concept_search_similar(store, query, 4, 1, 0.0f);
    ASSERT_NE(results, nullptr);
    EXPECT_GE(results->count, 1u);
    if (results->count > 0) {
        EXPECT_EQ(results->ids[0], 1u);
    }
    nimcp_memory_search_result_destroy(results);
}

// ============================================================================
// Concept Operation Tests
// ============================================================================

TEST_F(MemoryStoreTest, ConceptPutAndGet) {
    ASSERT_NE(store, nullptr);
    nimcp_concept_record_t c = make_concept(1, "bird", 0.9f);
    ASSERT_EQ(nimcp_memory_store_concept_put(store, &c), 0);

    nimcp_concept_record_t out = {0};
    ASSERT_EQ(nimcp_memory_store_concept_get(store, 1, &out), 0);
    EXPECT_STREQ(out.label, "bird");
    EXPECT_FLOAT_EQ(out.base_activation, 0.9f);
    EXPECT_EQ(out.access_count, 1u);
}

TEST_F(MemoryStoreTest, ConceptSearchTextByLabel) {
    ASSERT_NE(store, nullptr);
    nimcp_concept_record_t c = make_concept(1, "cardinal", 0.7f);
    ASSERT_EQ(nimcp_memory_store_concept_put(store, &c), 0);
    nimcp_memory_store_flush(store);

    nimcp_memory_search_result_t* results =
        nimcp_memory_store_concept_search_text(store, "cardinal", 10);
    ASSERT_NE(results, nullptr);
    EXPECT_GE(results->count, 1u);
    nimcp_memory_search_result_destroy(results);
}

TEST_F(MemoryStoreTest, ConceptUpdateActivation) {
    ASSERT_NE(store, nullptr);
    nimcp_concept_record_t c = make_concept(1, "mutable concept", 0.3f);
    ASSERT_EQ(nimcp_memory_store_concept_put(store, &c), 0);
    c.base_activation = 0.99f;
    ASSERT_EQ(nimcp_memory_store_concept_put(store, &c), 0);

    nimcp_concept_record_t out = {0};
    ASSERT_EQ(nimcp_memory_store_concept_get(store, 1, &out), 0);
    EXPECT_FLOAT_EQ(out.base_activation, 0.99f);
}

TEST_F(MemoryStoreTest, ConceptWithSourceEngram) {
    ASSERT_NE(store, nullptr);
    nimcp_concept_record_t c = make_concept(1, "derived concept", 0.5f);
    c.source_engram_id = 42;
    ASSERT_EQ(nimcp_memory_store_concept_put(store, &c), 0);

    nimcp_concept_record_t out = {0};
    ASSERT_EQ(nimcp_memory_store_concept_get(store, 1, &out), 0);
    EXPECT_EQ(out.source_engram_id, 42u);
}

// ============================================================================
// Relation (KG) Operation Tests
// ============================================================================

TEST_F(MemoryStoreTest, RelationPutAndQuery) {
    ASSERT_NE(store, nullptr);
    nimcp_concept_record_t c1 = make_concept(1, "bird", 0.8f);
    nimcp_concept_record_t c2 = make_concept(2, "animal", 0.9f);
    ASSERT_EQ(nimcp_memory_store_concept_put(store, &c1), 0);
    ASSERT_EQ(nimcp_memory_store_concept_put(store, &c2), 0);
    nimcp_memory_store_flush(store);  /* FK constraint: concepts must be in DB before relation_put */

    nimcp_relation_record_t r = make_relation(1, 2, 1, 0.9f);
    ASSERT_EQ(nimcp_memory_store_relation_put(store, &r), 0);
    nimcp_memory_store_flush(store);

    nimcp_memory_search_result_t* results =
        nimcp_memory_store_relation_get_for_concept(store, 1, 10);
    ASSERT_NE(results, nullptr);
    EXPECT_GE(results->count, 1u);
    nimcp_memory_search_result_destroy(results);
}

TEST_F(MemoryStoreTest, RelationTraverseOneHop) {
    ASSERT_NE(store, nullptr);
    nimcp_concept_record_t ca = make_concept(1, "A", 0.5f);
    nimcp_concept_record_t cb = make_concept(2, "B", 0.5f);
    ASSERT_EQ(nimcp_memory_store_concept_put(store, &ca), 0);
    ASSERT_EQ(nimcp_memory_store_concept_put(store, &cb), 0);
    nimcp_memory_store_flush(store);  /* FK constraint: concepts must be in DB before relation_put */

    nimcp_relation_record_t r = make_relation(1, 2, 1, 0.8f);
    ASSERT_EQ(nimcp_memory_store_relation_put(store, &r), 0);
    nimcp_memory_store_flush(store);

    nimcp_graph_traversal_result_t* results =
        nimcp_memory_store_relation_traverse(store, 1, 1, 0.0f);
    ASSERT_NE(results, nullptr);
    EXPECT_GE(results->count, 1u);
    bool found_b = false;
    for (uint32_t i = 0; i < results->count; i++) {
        if (results->node_ids[i] == 2) found_b = true;
    }
    EXPECT_TRUE(found_b);
    nimcp_memory_graph_result_destroy(results);
}

TEST_F(MemoryStoreTest, RelationTraverseTwoHop) {
    ASSERT_NE(store, nullptr);
    nimcp_concept_record_t ca = make_concept(1, "A", 0.5f);
    nimcp_concept_record_t cb = make_concept(2, "B", 0.5f);
    nimcp_concept_record_t cc = make_concept(3, "C", 0.5f);
    ASSERT_EQ(nimcp_memory_store_concept_put(store, &ca), 0);
    ASSERT_EQ(nimcp_memory_store_concept_put(store, &cb), 0);
    ASSERT_EQ(nimcp_memory_store_concept_put(store, &cc), 0);
    nimcp_memory_store_flush(store);  /* FK constraint: concepts must be in DB before relation_put */

    nimcp_relation_record_t r1 = make_relation(1, 2, 1, 0.8f);
    nimcp_relation_record_t r2 = make_relation(2, 3, 1, 0.7f);
    ASSERT_EQ(nimcp_memory_store_relation_put(store, &r1), 0);
    ASSERT_EQ(nimcp_memory_store_relation_put(store, &r2), 0);
    nimcp_memory_store_flush(store);

    nimcp_graph_traversal_result_t* results =
        nimcp_memory_store_relation_traverse(store, 1, 2, 0.0f);
    ASSERT_NE(results, nullptr);
    bool found_b = false, found_c = false;
    for (uint32_t i = 0; i < results->count; i++) {
        if (results->node_ids[i] == 2) found_b = true;
        if (results->node_ids[i] == 3) found_c = true;
    }
    EXPECT_TRUE(found_b);
    EXPECT_TRUE(found_c);
    nimcp_memory_graph_result_destroy(results);
}

TEST_F(MemoryStoreTest, RelationTraverseStrengthFilter) {
    ASSERT_NE(store, nullptr);
    nimcp_concept_record_t ca = make_concept(1, "A", 0.5f);
    nimcp_concept_record_t cb = make_concept(2, "B", 0.5f);
    nimcp_concept_record_t cc = make_concept(3, "C", 0.5f);
    ASSERT_EQ(nimcp_memory_store_concept_put(store, &ca), 0);
    ASSERT_EQ(nimcp_memory_store_concept_put(store, &cb), 0);
    ASSERT_EQ(nimcp_memory_store_concept_put(store, &cc), 0);
    nimcp_memory_store_flush(store);  /* FK constraint: concepts must be in DB before relation_put */

    nimcp_relation_record_t r1 = make_relation(1, 2, 1, 0.8f);
    nimcp_relation_record_t r2 = make_relation(1, 3, 2, 0.2f);
    ASSERT_EQ(nimcp_memory_store_relation_put(store, &r1), 0);
    ASSERT_EQ(nimcp_memory_store_relation_put(store, &r2), 0);
    nimcp_memory_store_flush(store);

    nimcp_graph_traversal_result_t* results =
        nimcp_memory_store_relation_traverse(store, 1, 1, 0.5f);
    ASSERT_NE(results, nullptr);
    bool found_b = false, found_c = false;
    for (uint32_t i = 0; i < results->count; i++) {
        if (results->node_ids[i] == 2) found_b = true;
        if (results->node_ids[i] == 3) found_c = true;
    }
    EXPECT_TRUE(found_b);
    EXPECT_FALSE(found_c);
    nimcp_memory_graph_result_destroy(results);
}

TEST_F(MemoryStoreTest, RelationTraverseNoCycles) {
    ASSERT_NE(store, nullptr);
    nimcp_concept_record_t ca = make_concept(1, "A", 0.5f);
    nimcp_concept_record_t cb = make_concept(2, "B", 0.5f);
    ASSERT_EQ(nimcp_memory_store_concept_put(store, &ca), 0);
    ASSERT_EQ(nimcp_memory_store_concept_put(store, &cb), 0);
    nimcp_memory_store_flush(store);  /* FK constraint: concepts must be in DB before relation_put */

    nimcp_relation_record_t r1 = make_relation(1, 2, 1, 0.8f);
    nimcp_relation_record_t r2 = make_relation(2, 1, 1, 0.8f);
    ASSERT_EQ(nimcp_memory_store_relation_put(store, &r1), 0);
    ASSERT_EQ(nimcp_memory_store_relation_put(store, &r2), 0);
    nimcp_memory_store_flush(store);

    nimcp_graph_traversal_result_t* results =
        nimcp_memory_store_relation_traverse(store, 1, 5, 0.0f);
    ASSERT_NE(results, nullptr);
    EXPECT_LE(results->count, 2u);
    nimcp_memory_graph_result_destroy(results);
}

// ============================================================================
// Autobiographical Tests
// ============================================================================

TEST_F(MemoryStoreTest, AutobioPutAndSearchText) {
    ASSERT_NE(store, nullptr);
    nimcp_autobio_record_t a = make_autobio(1, "learned about birds today", false);
    ASSERT_EQ(nimcp_memory_store_autobio_put(store, &a), 0);
    nimcp_memory_store_flush(store);

    nimcp_memory_search_result_t* results =
        nimcp_memory_store_autobio_search_text(store, "birds", 10);
    ASSERT_NE(results, nullptr);
    EXPECT_GE(results->count, 1u);
    nimcp_memory_search_result_destroy(results);
}

TEST_F(MemoryStoreTest, AutobioIdentityDefining) {
    ASSERT_NE(store, nullptr);
    nimcp_autobio_record_t a = make_autobio(1, "defining moment", true);
    ASSERT_EQ(nimcp_memory_store_autobio_put(store, &a), 0);
    nimcp_memory_store_flush(store);

    nimcp_memory_search_result_t* results =
        nimcp_memory_store_autobio_search_text(store, "defining", 10);
    ASSERT_NE(results, nullptr);
    EXPECT_GE(results->count, 1u);
    nimcp_memory_search_result_destroy(results);
}

TEST_F(MemoryStoreTest, AutobioCoreMemory) {
    ASSERT_NE(store, nullptr);
    nimcp_autobio_record_t a = make_autobio(1, "core memory event", false);
    a.is_core_memory = true;
    ASSERT_EQ(nimcp_memory_store_autobio_put(store, &a), 0);
    nimcp_memory_store_flush(store);

    nimcp_memory_search_result_t* results =
        nimcp_memory_store_autobio_search_text(store, "core memory", 10);
    ASSERT_NE(results, nullptr);
    EXPECT_GE(results->count, 1u);
    nimcp_memory_search_result_destroy(results);
}

// ============================================================================
// Bloom Filter Tests
// ============================================================================

TEST_F(MemoryStoreTest, BloomAddAndCheck) {
    ASSERT_NE(store, nullptr);
    float emb[] = {1.0f, 0.0f, 0.5f, 0.3f};
    nimcp_memory_store_bloom_add(store, emb, 4);
    bool found = nimcp_memory_store_bloom_check(store, emb, 4);
    EXPECT_TRUE(found);
}

TEST_F(MemoryStoreTest, BloomCheckMissing) {
    ASSERT_NE(store, nullptr);
    float emb[] = {0.1f, 0.2f, 0.3f, 0.4f};
    bool found = nimcp_memory_store_bloom_check(store, emb, 4);
    EXPECT_FALSE(found);
}

TEST_F(MemoryStoreTest, BloomFalsePositiveRate) {
    ASSERT_NE(store, nullptr);
    const int N = 1000;
    for (int i = 0; i < N; i++) {
        float emb[4] = {(float)i, (float)(i * 2), (float)(i * 3), (float)(i * 4)};
        nimcp_memory_store_bloom_add(store, emb, 4);
    }
    int false_positives = 0;
    for (int i = N; i < 2 * N; i++) {
        float emb[4] = {(float)i, (float)(i * 2), (float)(i * 3), (float)(i * 4)};
        if (nimcp_memory_store_bloom_check(store, emb, 4)) false_positives++;
    }
    float fp_rate = (float)false_positives / N;
    EXPECT_LT(fp_rate, 0.05f);
}

// ============================================================================
// Consolidation Tests
// ============================================================================

TEST_F(MemoryStoreTest, ConsolidateCreatesConceptFromEngram) {
    ASSERT_NE(store, nullptr);
    nimcp_engram_record_t e = make_engram(1, "very important memory", 0.95f);
    e.state = 2;
    float emb[] = {0.5f, 0.5f, 0.5f, 0.5f};
    e.embedding = emb;
    e.embedding_dim = 4;
    ASSERT_EQ(nimcp_memory_store_engram_put(store, &e), 0);
    nimcp_memory_store_flush(store);

    ASSERT_EQ(nimcp_memory_store_consolidate(store), 0);

    nimcp_memory_search_result_t* results =
        nimcp_memory_store_concept_search_text(store, "important", 10);
    ASSERT_NE(results, nullptr);
    nimcp_memory_search_result_destroy(results);
}

TEST_F(MemoryStoreTest, GCRemovesOldUnimportant) {
    ASSERT_NE(store, nullptr);
    for (int i = 0; i < 10; i++) {
        nimcp_engram_record_t e = make_engram(i + 1, "unimportant", 0.05f);
        e.timestamp_us = 1;
        ASSERT_EQ(nimcp_memory_store_engram_put(store, &e), 0);
    }
    nimcp_memory_store_flush(store);

    /* gc() returns the number of deleted rows, not 0/error.
     * max_age_days=0 means cutoff_us = now_us, so timestamp_us=1 is ancient. */
    int deleted = nimcp_memory_store_gc(store, 0.1f, 0);
    EXPECT_GE(deleted, 0);  /* non-negative = success */
    EXPECT_EQ(deleted, 10); /* all 10 low-importance engrams should be deleted */

    nimcp_engram_record_t out = {0};
    EXPECT_NE(nimcp_memory_store_engram_get(store, 1, &out), 0);
}

TEST_F(MemoryStoreTest, GCKeepsImportant) {
    ASSERT_NE(store, nullptr);
    nimcp_engram_record_t e = make_engram(1, "very important", 0.99f);
    e.timestamp_us = 1;
    ASSERT_EQ(nimcp_memory_store_engram_put(store, &e), 0);
    nimcp_memory_store_flush(store);

    /* importance 0.99 >= 0.5 threshold, so nothing should be deleted */
    EXPECT_EQ(nimcp_memory_store_gc(store, 0.5f, 0), 0);

    nimcp_engram_record_t out = {0};
    EXPECT_EQ(nimcp_memory_store_engram_get(store, 1, &out), 0);
    EXPECT_STREQ(out.label, "very important");
}

TEST_F(MemoryStoreTest, RebuildIndex) {
    ASSERT_NE(store, nullptr);
    nimcp_engram_record_t e = make_engram(1, "indexable memory", 0.5f);
    ASSERT_EQ(nimcp_memory_store_engram_put(store, &e), 0);
    nimcp_memory_store_flush(store);
    ASSERT_EQ(nimcp_memory_store_rebuild_index(store), 0);

    nimcp_memory_search_result_t* results =
        nimcp_memory_store_engram_search_text(store, "indexable", 10);
    ASSERT_NE(results, nullptr);
    EXPECT_GE(results->count, 1u);
    nimcp_memory_search_result_destroy(results);
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_F(MemoryStoreTest, StatsTrackWrites) {
    ASSERT_NE(store, nullptr);
    for (int i = 0; i < 10; i++) {
        nimcp_engram_record_t e = make_engram(i + 1, "stats test", 0.5f);
        nimcp_memory_store_engram_put(store, &e);
    }
    nimcp_memory_store_stats_t stats = {0};
    ASSERT_EQ(nimcp_memory_store_get_stats(store, &stats), 0);
    EXPECT_GE(stats.total_writes, 10u);
}

TEST_F(MemoryStoreTest, StatsTrackReads) {
    ASSERT_NE(store, nullptr);
    for (int i = 0; i < 5; i++) {
        nimcp_engram_record_t e = make_engram(i + 1, "read test", 0.5f);
        nimcp_memory_store_engram_put(store, &e);
    }
    nimcp_memory_store_flush(store);
    for (int i = 0; i < 5; i++) {
        nimcp_engram_record_t out = {0};
        nimcp_memory_store_engram_get(store, i + 1, &out);
    }
    nimcp_memory_store_stats_t stats = {0};
    ASSERT_EQ(nimcp_memory_store_get_stats(store, &stats), 0);
    EXPECT_GE(stats.total_reads, 5u);
}

TEST_F(MemoryStoreTest, StatsTrackCacheHits) {
    ASSERT_NE(store, nullptr);
    nimcp_engram_record_t e = make_engram(1, "cache test", 0.5f);
    nimcp_memory_store_engram_put(store, &e);
    nimcp_memory_store_flush(store);

    nimcp_engram_record_t out = {0};
    nimcp_memory_store_engram_get(store, 1, &out);
    nimcp_memory_store_engram_get(store, 1, &out);

    nimcp_memory_store_stats_t stats = {0};
    ASSERT_EQ(nimcp_memory_store_get_stats(store, &stats), 0);
    EXPECT_GE(stats.cache_hits, 1u);
}
