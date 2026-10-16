/**
 * @file test_memory_store_regression.cpp
 * @brief Regression tests for NIMCP persistent memory store
 *
 * WHAT: Edge-case and boundary-condition tests that protect against known
 *       classes of bugs -- NULL embeddings, zero dimensions, overlong labels,
 *       concurrent flush, NaN vectors, and corrupt reopens.
 * WHY:  These tests codify failure modes discovered during development.
 *       Any future change that re-introduces one of these bugs will be
 *       caught immediately.
 * HOW:  Google Test with per-test temp databases and deliberate abuse of
 *       the API surface.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <unistd.h>
#include <thread>
#include <limits>

extern "C" {
#include "memory/nimcp_memory_store.h"
}

// ============================================================================
// Test Fixture
// ============================================================================

class MemoryStoreRegressionTest : public ::testing::Test {
protected:
    nimcp_memory_store_t* store = nullptr;
    char db_path[256];

    void SetUp() override {
        snprintf(db_path, sizeof(db_path), "/tmp/nimcp_regr_%d.db", getpid());
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
};

// ============================================================================
// Regression Tests
// ============================================================================

TEST_F(MemoryStoreRegressionTest, NullEmbeddingHandled) {
    nimcp_engram_record_t e = make_engram(1, "no embedding", 0.5f);
    e.embedding = nullptr;
    e.embedding_dim = 0;
    EXPECT_EQ(nimcp_memory_store_engram_put(store, &e), 0);

    nimcp_engram_record_t out = {0};
    EXPECT_EQ(nimcp_memory_store_engram_get(store, 1, &out), 0);
    EXPECT_EQ(out.embedding_dim, 0u);
}

TEST_F(MemoryStoreRegressionTest, ZeroDimEmbedding) {
    nimcp_engram_record_t e = make_engram(1, "zero dim", 0.5f);
    float dummy = 1.0f;
    e.embedding = &dummy;
    e.embedding_dim = 0;
    int rc = nimcp_memory_store_engram_put(store, &e);
    // Must NOT crash
    (void)rc;
}

TEST_F(MemoryStoreRegressionTest, VeryLongLabel) {
    nimcp_engram_record_t e = {0};
    e.engram_id = 1;
    e.timestamp_us = 1000;
    e.importance = 0.5f;
    memset(e.label, 'A', sizeof(e.label) - 1);
    e.label[sizeof(e.label) - 1] = '\0';
    EXPECT_EQ(nimcp_memory_store_engram_put(store, &e), 0);

    nimcp_engram_record_t out = {0};
    EXPECT_EQ(nimcp_memory_store_engram_get(store, 1, &out), 0);
    EXPECT_GT(strlen(out.label), 0u);
}

TEST_F(MemoryStoreRegressionTest, EmptyLabel) {
    nimcp_engram_record_t e = make_engram(1, "", 0.5f);
    EXPECT_EQ(nimcp_memory_store_engram_put(store, &e), 0);
    nimcp_memory_store_flush(store);

    // FTS search on empty label -- should not crash
    nimcp_memory_search_result_t* results =
        nimcp_memory_store_engram_search_text(store, "", 10);
    // May return NULL or valid result -- must not crash
    if (results) nimcp_memory_search_result_destroy(results);
}

TEST_F(MemoryStoreRegressionTest, NegativeImportance) {
    nimcp_engram_record_t e = make_engram(1, "negative importance", -1.0f);
    int rc = nimcp_memory_store_engram_put(store, &e);
    if (rc == 0) {
        nimcp_engram_record_t out = {0};
        EXPECT_EQ(nimcp_memory_store_engram_get(store, 1, &out), 0);
    }
}

TEST_F(MemoryStoreRegressionTest, MaxUint64Timestamp) {
    nimcp_engram_record_t e = make_engram(1, "max timestamp", 0.5f);
    e.timestamp_us = UINT64_MAX;
    EXPECT_EQ(nimcp_memory_store_engram_put(store, &e), 0);

    nimcp_engram_record_t out = {0};
    EXPECT_EQ(nimcp_memory_store_engram_get(store, 1, &out), 0);
    EXPECT_EQ(out.timestamp_us, UINT64_MAX);
}

TEST_F(MemoryStoreRegressionTest, ConcurrentFlush) {
    for (int i = 0; i < 100; i++) {
        nimcp_engram_record_t e = make_engram(i + 1, "concurrent flush", 0.5f);
        nimcp_memory_store_engram_put(store, &e);
    }

    std::atomic<int> errors{0};
    auto flusher = [&]() {
        if (nimcp_memory_store_flush(store) != 0) errors++;
    };

    std::thread t1(flusher);
    std::thread t2(flusher);
    t1.join();
    t2.join();

    EXPECT_EQ(errors.load(), 0);
}

TEST_F(MemoryStoreRegressionTest, DatabaseCorruptionRecovery) {
    for (int i = 0; i < 10; i++) {
        nimcp_engram_record_t e = make_engram(i + 1, "valid", 0.5f);
        ASSERT_EQ(nimcp_memory_store_engram_put(store, &e), 0);
    }
    nimcp_memory_store_checkpoint(store);
    nimcp_memory_store_destroy(store);
    store = nullptr;

    // Write garbage to WAL file to simulate partial write
    char wal[270];
    snprintf(wal, sizeof(wal), "%s-wal", db_path);
    FILE* f = fopen(wal, "ab");
    if (f) {
        const char garbage[] = "THIS IS GARBAGE DATA";
        fwrite(garbage, 1, sizeof(garbage), f);
        fclose(f);
    }

    nimcp_memory_store_config_t config = nimcp_memory_store_config_default();
    config.db_path = db_path;
    config.enable_questdb_sync = false;
    store = nimcp_memory_store_create(&config);
    if (store) {
        nimcp_engram_record_t out = {0};
        EXPECT_EQ(nimcp_memory_store_engram_get(store, 1, &out), 0);
    }
}

TEST_F(MemoryStoreRegressionTest, ZeroCapacityConfig) {
    nimcp_memory_store_destroy(store);
    store = nullptr;

    char path2[256];
    snprintf(path2, sizeof(path2), "/tmp/nimcp_regr_zero_%d.db", getpid());

    nimcp_memory_store_config_t config = nimcp_memory_store_config_default();
    config.db_path = path2;
    config.enable_questdb_sync = false;
    config.hot_cache_size = 0;

    store = nimcp_memory_store_create(&config);
    if (store) {
        nimcp_engram_record_t e = make_engram(1, "no cache", 0.5f);
        EXPECT_EQ(nimcp_memory_store_engram_put(store, &e), 0);
        nimcp_memory_store_flush(store);

        nimcp_engram_record_t out = {0};
        EXPECT_EQ(nimcp_memory_store_engram_get(store, 1, &out), 0);

        nimcp_memory_store_destroy(store);
        store = nullptr;
    }
    unlink(path2);
    char wal2[270], shm2[270];
    snprintf(wal2, sizeof(wal2), "%s-wal", path2);
    snprintf(shm2, sizeof(shm2), "%s-shm", path2);
    unlink(wal2);
    unlink(shm2);
}

TEST_F(MemoryStoreRegressionTest, VectorSearchWithNaN) {
    float valid_emb[] = {1.0f, 0.0f, 0.0f, 0.0f};
    nimcp_engram_record_t e = make_engram(1, "valid", 0.5f);
    e.embedding = valid_emb;
    e.embedding_dim = 4;
    ASSERT_EQ(nimcp_memory_store_engram_put(store, &e), 0);
    nimcp_memory_store_flush(store);

    float nan_query[] = {NAN, 0.0f, 0.0f, 0.0f};
    nimcp_memory_search_result_t* results =
        nimcp_memory_store_engram_search_similar(store, nan_query, 4, 1, 0.0f);
    // Should handle gracefully -- either return NULL or empty results
    // Must NOT crash
    if (results) {
        for (uint32_t i = 0; i < results->count; i++) {
            EXPECT_FALSE(std::isnan(results->distances[i]));
        }
        nimcp_memory_search_result_destroy(results);
    }
}

TEST_F(MemoryStoreRegressionTest, DestroyNullStore) {
    nimcp_memory_store_destroy(nullptr);
}

TEST_F(MemoryStoreRegressionTest, FlushNullStore) {
    EXPECT_NE(nimcp_memory_store_flush(nullptr), 0);
}

TEST_F(MemoryStoreRegressionTest, GetStatsNullStore) {
    nimcp_memory_store_stats_t stats = {0};
    EXPECT_NE(nimcp_memory_store_get_stats(nullptr, &stats), 0);
}

TEST_F(MemoryStoreRegressionTest, SearchTextNullQuery) {
    nimcp_memory_search_result_t* results =
        nimcp_memory_store_engram_search_text(store, nullptr, 10);
    // Should return NULL for null query
    EXPECT_EQ(results, nullptr);
}

TEST_F(MemoryStoreRegressionTest, SearchSimilarNullQuery) {
    nimcp_memory_search_result_t* results =
        nimcp_memory_store_engram_search_similar(store, nullptr, 4, 1, 0.0f);
    // Should return NULL for null query
    EXPECT_EQ(results, nullptr);
}
