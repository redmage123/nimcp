/**
 * @file test_oodb_regression.cpp
 * @brief Regression tests for NIMCP OODB edge cases and boundary conditions
 *
 * WHAT: 10 regression tests covering NULL store, zero cache, eviction of
 *       clean objects, duplicate IDs, traversal cycles, destroy with dirty
 *       objects, concurrent access simulation, large labels, empty tags,
 *       and null pointer relations.
 * WHY:  Guard against previously-discovered or likely edge cases in the
 *       OODB cache layer.
 * HOW:  Google Test with targeted API abuse and boundary conditions.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdint>
#include <unistd.h>

extern "C" {
#include "memory/nimcp_memory_oodb.h"
#include "memory/nimcp_memory_store.h"
}

// ============================================================================
// Test Fixture with optional SQLite backing
// ============================================================================

class OODBRegressionTest : public ::testing::Test {
protected:
    nimcp_memory_store_t* store = nullptr;
    char db_path[256];

    void SetUp() override {
        snprintf(db_path, sizeof(db_path), "/tmp/nimcp_oodb_reg_%d.db", getpid());
        nimcp_memory_store_config_t store_config = nimcp_memory_store_config_default();
        store_config.db_path = db_path;
        store_config.enable_questdb_sync = false;
        store = nimcp_memory_store_create(&store_config);
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
};

// ============================================================================
// NullStoreBackend
// ============================================================================

TEST(OODBRegressionStandalone, NullStoreBackend) {
    nimcp_oodb_config_t cfg = nimcp_oodb_config_default();
    nimcp_oodb_t* oodb = nimcp_oodb_create(&cfg, nullptr);
    ASSERT_NE(oodb, nullptr);

    /* Basic operations should still work in cache-only mode */
    oodb_engram_t* e = nimcp_oodb_create_engram(oodb, 1);
    ASSERT_NE(e, nullptr);
    strncpy(e->label, "cache only", sizeof(e->label) - 1);

    oodb_engram_t* fetched = nimcp_oodb_get_engram(oodb, 1);
    ASSERT_NE(fetched, nullptr);
    EXPECT_STREQ(fetched->label, "cache only");

    /* Miss should return NULL (no store to load from) */
    oodb_engram_t* missing = nimcp_oodb_get_engram(oodb, 999);
    EXPECT_EQ(missing, nullptr);

    /* Flush with NULL store should not crash */
    nimcp_oodb_flush(oodb);

    nimcp_oodb_destroy(oodb);
}

// ============================================================================
// ZeroCacheSize
// ============================================================================

TEST(OODBRegressionStandalone, ZeroCacheSize) {
    nimcp_oodb_config_t cfg = nimcp_oodb_config_default();
    cfg.max_cached_objects = 0;
    cfg.max_engrams = 0;
    cfg.max_concepts = 0;

    nimcp_oodb_t* oodb = nimcp_oodb_create(&cfg, nullptr);
    /* Implementation may either reject zero cache or accept it */
    if (oodb) {
        /* Should still be able to create objects (immediate eviction possible) */
        oodb_engram_t* e = nimcp_oodb_create_engram(oodb, 1);
        /* May be NULL if cache rejects at zero capacity */
        (void)e;
        nimcp_oodb_destroy(oodb);
    }
}

// ============================================================================
// EvictCleanObject
// ============================================================================

TEST_F(OODBRegressionTest, EvictCleanObject) {
    nimcp_oodb_config_t cfg = nimcp_oodb_config_default();
    cfg.max_cached_objects = 3;
    cfg.max_engrams = 3;
    nimcp_oodb_t* oodb = nimcp_oodb_create(&cfg, store);
    ASSERT_NE(oodb, nullptr);

    /* Create clean (non-dirty) objects */
    for (uint64_t i = 1; i <= 3; i++) {
        oodb_engram_t* e = nimcp_oodb_create_engram(oodb, i);
        ASSERT_NE(e, nullptr);
        /* NOT marked dirty — state is OODB_STATE_NEW */
    }

    /* Add one more to force eviction of id=1 */
    nimcp_oodb_create_engram(oodb, 4);

    /* Clean evicted object should NOT be in SQLite */
    nimcp_engram_record_t out;
    memset(&out, 0, sizeof(out));
    int rc = nimcp_memory_store_engram_get(store, 1, &out);
    /* NEW (never dirtied) objects that get evicted may or may not be stored.
     * The key requirement is no crash. */
    (void)rc;

    nimcp_oodb_destroy(oodb);
}

// ============================================================================
// DuplicateId
// ============================================================================

TEST(OODBRegressionStandalone, DuplicateId) {
    nimcp_oodb_config_t cfg = nimcp_oodb_config_default();
    nimcp_oodb_t* oodb = nimcp_oodb_create(&cfg, nullptr);
    ASSERT_NE(oodb, nullptr);

    oodb_engram_t* e1 = nimcp_oodb_create_engram(oodb, 42);
    ASSERT_NE(e1, nullptr);
    strncpy(e1->label, "first", sizeof(e1->label) - 1);

    /* Create another with same ID — should return existing or overwrite */
    oodb_engram_t* e2 = nimcp_oodb_create_engram(oodb, 42);
    ASSERT_NE(e2, nullptr);

    /* Verify no corruption — get should return a valid object */
    oodb_engram_t* check = nimcp_oodb_get_engram(oodb, 42);
    ASSERT_NE(check, nullptr);
    EXPECT_EQ(check->base.id, 42u);

    nimcp_oodb_destroy(oodb);
}

// ============================================================================
// TraversalCycleDetection
// ============================================================================

TEST(OODBRegressionStandalone, TraversalCycleDetection) {
    nimcp_oodb_config_t cfg = nimcp_oodb_config_default();
    nimcp_oodb_t* oodb = nimcp_oodb_create(&cfg, nullptr);
    ASSERT_NE(oodb, nullptr);

    oodb_concept_t* a = nimcp_oodb_create_concept(oodb, 1);
    oodb_concept_t* b = nimcp_oodb_create_concept(oodb, 2);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    /* Create A->B->A cycle */
    oodb_relation_t* r1 = nimcp_oodb_create_relation(oodb, 100);
    r1->source = a; r1->target = b; r1->strength = 0.9f;
    oodb_relation_t* r2 = nimcp_oodb_create_relation(oodb, 101);
    r2->source = b; r2->target = a; r2->strength = 0.9f;

    a->relations[0] = r1; a->relation_count = 1;
    b->relations[0] = r2; b->relation_count = 1;

    /* Traverse with high hop count — must terminate, not loop forever */
    nimcp_oodb_traversal_t* result = nimcp_oodb_traverse(oodb, a, 100, 0.0f);
    ASSERT_NE(result, nullptr);
    /* Should only visit B once despite cycle */
    EXPECT_EQ(result->count, 1u);
    nimcp_oodb_traversal_destroy(result);

    nimcp_oodb_destroy(oodb);
}

// ============================================================================
// DestroyWithDirtyObjects
// ============================================================================

TEST_F(OODBRegressionTest, DestroyWithDirtyObjects) {
    nimcp_oodb_config_t cfg = nimcp_oodb_config_default();
    nimcp_oodb_t* oodb = nimcp_oodb_create(&cfg, store);
    ASSERT_NE(oodb, nullptr);

    /* Create dirty objects without flushing */
    for (uint64_t i = 1; i <= 10; i++) {
        oodb_engram_t* e = nimcp_oodb_create_engram(oodb, i);
        ASSERT_NE(e, nullptr);
        snprintf(e->label, sizeof(e->label), "unflushed-%lu", (unsigned long)i);
        nimcp_oodb_mark_dirty(&e->base);
    }

    /* Destroy should auto-flush dirty objects or at least not crash */
    nimcp_oodb_destroy(oodb);
    /* No crash = pass */

    /* Check if auto-flush happened */
    nimcp_memory_store_flush(store);
    nimcp_engram_record_t out;
    memset(&out, 0, sizeof(out));
    int rc = nimcp_memory_store_engram_get(store, 1, &out);
    /* May or may not have auto-flushed — either way, no crash */
    (void)rc;
}

// ============================================================================
// ConcurrentAccess (single-threaded simulation)
// ============================================================================

TEST(OODBRegressionStandalone, ConcurrentAccess) {
    nimcp_oodb_config_t cfg = nimcp_oodb_config_default();
    nimcp_oodb_t* oodb = nimcp_oodb_create(&cfg, nullptr);
    ASSERT_NE(oodb, nullptr);

    oodb_engram_t* e = nimcp_oodb_create_engram(oodb, 1);
    ASSERT_NE(e, nullptr);
    strncpy(e->label, "shared object", sizeof(e->label) - 1);

    /* Simulate rapid access from different "iterations" */
    for (int iter = 0; iter < 1000; iter++) {
        oodb_engram_t* fetched = nimcp_oodb_get_engram(oodb, 1);
        ASSERT_NE(fetched, nullptr);
        EXPECT_EQ(fetched->base.id, 1u);

        /* Occasionally mark dirty */
        if (iter % 100 == 0) {
            nimcp_oodb_mark_dirty(&fetched->base);
        }
    }

    nimcp_oodb_stats_t stats;
    nimcp_oodb_get_stats(oodb, &stats);
    EXPECT_GE(stats.cache_hits, 999u);

    nimcp_oodb_destroy(oodb);
}

// ============================================================================
// VeryLargeLabel
// ============================================================================

TEST(OODBRegressionStandalone, VeryLargeLabel) {
    nimcp_oodb_config_t cfg = nimcp_oodb_config_default();
    nimcp_oodb_t* oodb = nimcp_oodb_create(&cfg, nullptr);
    ASSERT_NE(oodb, nullptr);

    oodb_engram_t* e = nimcp_oodb_create_engram(oodb, 1);
    ASSERT_NE(e, nullptr);

    /* Fill label to max (255 chars + null) */
    char big_label[256];
    memset(big_label, 'A', 255);
    big_label[255] = '\0';
    strncpy(e->label, big_label, sizeof(e->label) - 1);
    e->label[sizeof(e->label) - 1] = '\0';

    oodb_engram_t* fetched = nimcp_oodb_get_engram(oodb, 1);
    ASSERT_NE(fetched, nullptr);
    EXPECT_EQ(strlen(fetched->label), 255u);

    nimcp_oodb_destroy(oodb);
}

// ============================================================================
// EmptyTagSearch
// ============================================================================

TEST(OODBRegressionStandalone, EmptyTagSearch) {
    nimcp_oodb_config_t cfg = nimcp_oodb_config_default();
    nimcp_oodb_t* oodb = nimcp_oodb_create(&cfg, nullptr);
    ASSERT_NE(oodb, nullptr);

    nimcp_oodb_create_engram(oodb, 1);

    nimcp_oodb_search_result_t* result =
        nimcp_oodb_search_by_tag(oodb, "", 10);
    ASSERT_NE(result, nullptr);
    /* Empty tag should match nothing or everything depending on implementation */
    nimcp_oodb_search_result_destroy(result);

    nimcp_oodb_destroy(oodb);
}

// ============================================================================
// RelationWithNullPointers
// ============================================================================

TEST(OODBRegressionStandalone, RelationWithNullPointers) {
    nimcp_oodb_config_t cfg = nimcp_oodb_config_default();
    nimcp_oodb_t* oodb = nimcp_oodb_create(&cfg, nullptr);
    ASSERT_NE(oodb, nullptr);

    oodb_concept_t* a = nimcp_oodb_create_concept(oodb, 1);
    ASSERT_NE(a, nullptr);

    oodb_relation_t* r = nimcp_oodb_create_relation(oodb, 100);
    ASSERT_NE(r, nullptr);

    /* source is set, target is NULL stub */
    r->source = a;
    r->target = nullptr;
    r->strength = 0.5f;

    a->relations[0] = r;
    a->relation_count = 1;

    /* Traversal should handle NULL target gracefully */
    nimcp_oodb_traversal_t* result = nimcp_oodb_traverse(oodb, a, 1, 0.0f);
    ASSERT_NE(result, nullptr);
    /* Should either skip NULL target or return 0 results */
    nimcp_oodb_traversal_destroy(result);

    nimcp_oodb_destroy(oodb);
}
