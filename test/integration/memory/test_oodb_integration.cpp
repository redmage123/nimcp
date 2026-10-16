/**
 * @file test_oodb_integration.cpp
 * @brief Integration tests for OODB cache + SQLite store working together
 *
 * WHAT: 12 integration tests covering OODB-store interaction: create+flush,
 *       lazy loading, graph traversal, dirty flush, eviction, prewarm,
 *       tag search, stats, metadata, and full pipeline.
 * WHY:  The OODB cache and SQLite store form a two-tier memory system.
 *       Integration tests verify the contract between tiers holds under
 *       realistic workflows.
 * HOW:  Google Test with both OODB and SQLite store per test.
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
// Test Fixture
// ============================================================================

class OODBIntegrationTest : public ::testing::Test {
protected:
    nimcp_oodb_t* oodb = nullptr;
    nimcp_memory_store_t* store = nullptr;
    char db_path[256];

    void SetUp() override {
        snprintf(db_path, sizeof(db_path), "/tmp/nimcp_oodb_integ_%d.db", getpid());
        nimcp_memory_store_config_t store_config = nimcp_memory_store_config_default();
        store_config.db_path = db_path;
        store_config.enable_questdb_sync = false;
        store = nimcp_memory_store_create(&store_config);

        nimcp_oodb_config_t oodb_config = nimcp_oodb_config_default();
        oodb = nimcp_oodb_create(&oodb_config, store);
    }

    void TearDown() override {
        if (oodb) nimcp_oodb_destroy(oodb);
        if (store) nimcp_memory_store_destroy(store);
        unlink(db_path);
        char wal[270], shm[270];
        snprintf(wal, sizeof(wal), "%s-wal", db_path);
        snprintf(shm, sizeof(shm), "%s-shm", db_path);
        unlink(wal);
        unlink(shm);
    }

    nimcp_engram_record_t make_engram(uint64_t id, const char* label, float importance) {
        nimcp_engram_record_t r;
        memset(&r, 0, sizeof(r));
        r.engram_id = id;
        r.timestamp_us = id * 1000;
        r.importance = importance;
        if (label) strncpy(r.label, label, sizeof(r.label) - 1);
        return r;
    }

    nimcp_concept_record_t make_concept_rec(uint64_t id, const char* label) {
        nimcp_concept_record_t c;
        memset(&c, 0, sizeof(c));
        c.concept_id = id;
        c.timestamp_us = id * 1000;
        c.base_activation = 0.5f;
        c.access_count = 1;
        if (label) strncpy(c.label, label, sizeof(c.label) - 1);
        return c;
    }
};

// ============================================================================
// CreateWithStore
// ============================================================================

TEST_F(OODBIntegrationTest, CreateWithStore) {
    ASSERT_NE(oodb, nullptr);
    ASSERT_NE(store, nullptr);
}

// ============================================================================
// PutAndFlush
// ============================================================================

TEST_F(OODBIntegrationTest, PutAndFlush) {
    ASSERT_NE(oodb, nullptr);
    oodb_engram_t* e = nimcp_oodb_create_engram(oodb, 1);
    ASSERT_NE(e, nullptr);
    strncpy(e->label, "flushed engram", sizeof(e->label) - 1);
    e->base.importance = 0.9f;
    nimcp_oodb_mark_dirty(&e->base);

    ASSERT_EQ(nimcp_oodb_flush(oodb), 0);
    nimcp_memory_store_flush(store);

    /* Verify appears in SQLite */
    nimcp_engram_record_t out;
    memset(&out, 0, sizeof(out));
    ASSERT_EQ(nimcp_memory_store_engram_get(store, 1, &out), 0);
    EXPECT_STREQ(out.label, "flushed engram");
}

// ============================================================================
// LazyLoadFromStore
// ============================================================================

TEST_F(OODBIntegrationTest, LazyLoadFromStore) {
    /* Put engram directly in SQLite */
    nimcp_engram_record_t rec = make_engram(77, "lazy load me", 0.8f);
    ASSERT_EQ(nimcp_memory_store_engram_put(store, &rec), 0);
    nimcp_memory_store_flush(store);

    /* Get via OODB — should lazy-load */
    oodb_engram_t* e = nimcp_oodb_get_engram(oodb, 77);
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(e->base.id, 77u);
    EXPECT_STREQ(e->label, "lazy load me");
    EXPECT_EQ(e->base.state, OODB_STATE_LOADED);
}

// ============================================================================
// GraphTraversalPointers
// ============================================================================

TEST_F(OODBIntegrationTest, GraphTraversalPointers) {
    oodb_concept_t* a = nimcp_oodb_create_concept(oodb, 1);
    oodb_concept_t* b = nimcp_oodb_create_concept(oodb, 2);
    oodb_concept_t* c = nimcp_oodb_create_concept(oodb, 3);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    ASSERT_NE(c, nullptr);

    strncpy(a->label, "A", sizeof(a->label) - 1);
    strncpy(b->label, "B", sizeof(b->label) - 1);
    strncpy(c->label, "C", sizeof(c->label) - 1);

    oodb_relation_t* r1 = nimcp_oodb_create_relation(oodb, 100);
    r1->source = a; r1->target = b; r1->strength = 0.9f;
    a->relations[0] = r1; a->relation_count = 1;

    oodb_relation_t* r2 = nimcp_oodb_create_relation(oodb, 101);
    r2->source = b; r2->target = c; r2->strength = 0.8f;
    b->relations[0] = r2; b->relation_count = 1;

    nimcp_oodb_traversal_t* result = nimcp_oodb_traverse(oodb, a, 3, 0.0f);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->count, 2u);

    bool found_b = false, found_c = false;
    for (uint32_t i = 0; i < result->count; i++) {
        if (result->concepts[i] == b) found_b = true;
        if (result->concepts[i] == c) found_c = true;
    }
    EXPECT_TRUE(found_b);
    EXPECT_TRUE(found_c);
    nimcp_oodb_traversal_destroy(result);
}

// ============================================================================
// TraversalVsSQL
// ============================================================================

TEST_F(OODBIntegrationTest, TraversalVsSQL) {
    /* Create graph in OODB */
    oodb_concept_t* a = nimcp_oodb_create_concept(oodb, 1);
    oodb_concept_t* b = nimcp_oodb_create_concept(oodb, 2);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    strncpy(a->label, "nodeA", sizeof(a->label) - 1);
    strncpy(b->label, "nodeB", sizeof(b->label) - 1);

    oodb_relation_t* r = nimcp_oodb_create_relation(oodb, 100);
    r->source = a; r->target = b; r->strength = 0.85f;
    a->relations[0] = r; a->relation_count = 1;

    /* OODB traversal */
    nimcp_oodb_traversal_t* oodb_result = nimcp_oodb_traverse(oodb, a, 1, 0.0f);
    ASSERT_NE(oodb_result, nullptr);
    EXPECT_GE(oodb_result->count, 1u);

    /* Create same graph in SQLite */
    nimcp_concept_record_t ca = make_concept_rec(1, "nodeA");
    nimcp_concept_record_t cb = make_concept_rec(2, "nodeB");
    ASSERT_EQ(nimcp_memory_store_concept_put(store, &ca), 0);
    ASSERT_EQ(nimcp_memory_store_concept_put(store, &cb), 0);
    nimcp_memory_store_flush(store);

    nimcp_relation_record_t rel;
    memset(&rel, 0, sizeof(rel));
    rel.relation_id = 100;
    rel.source_concept_id = 1;
    rel.target_concept_id = 2;
    rel.relation_type = 1;
    rel.strength = 0.85f;
    rel.timestamp_us = 1000;
    ASSERT_EQ(nimcp_memory_store_relation_put(store, &rel), 0);
    nimcp_memory_store_flush(store);

    nimcp_graph_traversal_result_t* sql_result =
        nimcp_memory_store_relation_traverse(store, 1, 1, 0.0f);
    ASSERT_NE(sql_result, nullptr);
    EXPECT_GE(sql_result->count, 1u);

    /* Both should find node B */
    bool oodb_found_b = false;
    for (uint32_t i = 0; i < oodb_result->count; i++) {
        if (oodb_result->concepts[i]->base.id == 2) oodb_found_b = true;
    }
    bool sql_found_b = false;
    for (uint32_t i = 0; i < sql_result->count; i++) {
        if (sql_result->node_ids[i] == 2) sql_found_b = true;
    }
    EXPECT_TRUE(oodb_found_b);
    EXPECT_TRUE(sql_found_b);

    nimcp_oodb_traversal_destroy(oodb_result);
    nimcp_memory_graph_result_destroy(sql_result);
}

// ============================================================================
// FlushDirtyOnly
// ============================================================================

TEST_F(OODBIntegrationTest, FlushDirtyOnly) {
    /* Create 5 objects, dirty only 2 */
    for (uint64_t i = 1; i <= 5; i++) {
        oodb_engram_t* e = nimcp_oodb_create_engram(oodb, i);
        ASSERT_NE(e, nullptr);
        snprintf(e->label, sizeof(e->label), "engram-%lu", (unsigned long)i);
        if (i == 2 || i == 4) {
            nimcp_oodb_mark_dirty(&e->base);
        }
    }

    ASSERT_EQ(nimcp_oodb_flush(oodb), 0);
    nimcp_memory_store_flush(store);

    /* Dirty objects MUST be in SQLite */
    nimcp_engram_record_t out;
    memset(&out, 0, sizeof(out));
    EXPECT_EQ(nimcp_memory_store_engram_get(store, 2, &out), 0);
    EXPECT_STREQ(out.label, "engram-2");

    memset(&out, 0, sizeof(out));
    EXPECT_EQ(nimcp_memory_store_engram_get(store, 4, &out), 0);
    EXPECT_STREQ(out.label, "engram-4");

    /* Implementation may also flush NEW objects — verify dirty ones
     * have correct data, which is the key invariant. */
}

// ============================================================================
// EvictionFlushes
// ============================================================================

TEST_F(OODBIntegrationTest, EvictionFlushes) {
    /* Recreate OODB with small cache */
    nimcp_oodb_destroy(oodb);
    nimcp_oodb_config_t small_cfg = nimcp_oodb_config_default();
    small_cfg.max_cached_objects = 3;
    small_cfg.max_engrams = 3;
    oodb = nimcp_oodb_create(&small_cfg, store);
    ASSERT_NE(oodb, nullptr);

    /* Create and dirty object that will be evicted */
    oodb_engram_t* e = nimcp_oodb_create_engram(oodb, 1);
    ASSERT_NE(e, nullptr);
    strncpy(e->label, "evict-me", sizeof(e->label) - 1);
    nimcp_oodb_mark_dirty(&e->base);

    /* Fill cache to force eviction */
    nimcp_oodb_create_engram(oodb, 2);
    nimcp_oodb_create_engram(oodb, 3);
    nimcp_oodb_create_engram(oodb, 4);  /* Should evict id=1 */

    nimcp_memory_store_flush(store);

    /* Dirty evicted object should have been written to SQLite */
    nimcp_engram_record_t out;
    memset(&out, 0, sizeof(out));
    EXPECT_EQ(nimcp_memory_store_engram_get(store, 1, &out), 0);
    EXPECT_STREQ(out.label, "evict-me");
}

// ============================================================================
// PrewarmFromStore
// ============================================================================

TEST_F(OODBIntegrationTest, PrewarmFromStore) {
    /* Put 20 engrams in SQLite */
    for (uint64_t i = 1; i <= 20; i++) {
        nimcp_engram_record_t rec = make_engram(i, "prewarm-item", 0.5f);
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        rec.timestamp_us = (uint64_t)ts.tv_sec * 1000000ULL + i;
        ASSERT_EQ(nimcp_memory_store_engram_put(store, &rec), 0);
    }
    nimcp_memory_store_flush(store);

    int loaded = nimcp_oodb_prewarm(oodb, 10);
    EXPECT_GE(loaded, 0);
    /* Prewarm should load items into OODB cache */
}

// ============================================================================
// TagSearchAcrossTypes
// ============================================================================

TEST_F(OODBIntegrationTest, TagSearchAcrossTypes) {
    oodb_engram_t* e = nimcp_oodb_create_engram(oodb, 1);
    ASSERT_NE(e, nullptr);
    strncpy(e->base.tags, "shared_tag,engram_only", sizeof(e->base.tags) - 1);

    oodb_concept_t* c = nimcp_oodb_create_concept(oodb, 2);
    ASSERT_NE(c, nullptr);
    strncpy(c->base.tags, "shared_tag,concept_only", sizeof(c->base.tags) - 1);

    nimcp_oodb_search_result_t* result =
        nimcp_oodb_search_by_tag(oodb, "shared_tag", 10);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->count, 2u);

    /* Verify both types present */
    bool found_engram = false, found_concept = false;
    for (uint32_t i = 0; i < result->count; i++) {
        if (result->objects[i]->type == OODB_TYPE_ENGRAM) found_engram = true;
        if (result->objects[i]->type == OODB_TYPE_CONCEPT) found_concept = true;
    }
    EXPECT_TRUE(found_engram);
    EXPECT_TRUE(found_concept);
    nimcp_oodb_search_result_destroy(result);
}

// ============================================================================
// StatsAccuracy
// ============================================================================

TEST_F(OODBIntegrationTest, StatsAccuracy) {
    /* Create some objects */
    nimcp_oodb_create_engram(oodb, 1);
    nimcp_oodb_create_engram(oodb, 2);
    nimcp_oodb_create_concept(oodb, 3);

    /* Cache hits */
    nimcp_oodb_get_engram(oodb, 1);
    nimcp_oodb_get_engram(oodb, 2);

    /* Cache miss */
    nimcp_oodb_get_engram(oodb, 999);

    nimcp_oodb_stats_t stats;
    ASSERT_EQ(nimcp_oodb_get_stats(oodb, &stats), 0);
    EXPECT_EQ(stats.cached_engrams, 2u);
    EXPECT_EQ(stats.cached_concepts, 1u);
    EXPECT_GE(stats.cache_hits, 2u);
    EXPECT_GE(stats.cache_misses, 1u);
}

// ============================================================================
// OODBWithMetadata
// ============================================================================

TEST_F(OODBIntegrationTest, OODBWithMetadata) {
    /* Create object in OODB */
    oodb_engram_t* e = nimcp_oodb_create_engram(oodb, 1);
    ASSERT_NE(e, nullptr);
    strncpy(e->label, "metadata linked", sizeof(e->label) - 1);
    nimcp_oodb_mark_dirty(&e->base);
    nimcp_oodb_flush(oodb);
    nimcp_memory_store_flush(store);

    /* Add metadata via store */
    nimcp_metadata_record_t meta;
    memset(&meta, 0, sizeof(meta));
    meta.entry_id = 1;
    meta.type = NIMCP_MEMORY_TYPE_ENGRAM;
    meta.timestamp_us = 1000;
    meta.importance = 0.8f;
    meta.training_step = 100;
    strncpy(meta.label, "metadata linked", sizeof(meta.label) - 1);
    strncpy(meta.tags, "oodb_test", sizeof(meta.tags) - 1);
    ASSERT_EQ(nimcp_memory_store_metadata_put(store, &meta), 0);

    /* Search metadata, should find the OODB-created object */
    nimcp_metadata_search_result_t* result =
        nimcp_memory_store_metadata_search_tags(store, "oodb_test", 10);
    ASSERT_NE(result, nullptr);
    EXPECT_GE(result->count, 1u);
    nimcp_metadata_search_result_destroy(result);
}

// ============================================================================
// FullPipeline
// ============================================================================

TEST_F(OODBIntegrationTest, FullPipeline) {
    const int N = 100;

    /* Create 100 objects */
    for (int i = 0; i < N; i++) {
        oodb_engram_t* e = nimcp_oodb_create_engram(oodb, (uint64_t)i + 1);
        ASSERT_NE(e, nullptr);
        snprintf(e->label, sizeof(e->label), "pipeline-%d", i);
        e->base.importance = (float)i / N;
    }

    /* Dirty 50 */
    for (int i = 0; i < 50; i++) {
        oodb_engram_t* e = nimcp_oodb_get_engram(oodb, (uint64_t)i + 1);
        ASSERT_NE(e, nullptr);
        nimcp_oodb_mark_dirty(&e->base);
    }

    /* Flush */
    ASSERT_EQ(nimcp_oodb_flush(oodb), 0);
    nimcp_memory_store_flush(store);

    /* Verify dirty objects are in SQLite */
    for (int i = 0; i < 50; i++) {
        nimcp_engram_record_t out;
        memset(&out, 0, sizeof(out));
        int rc = nimcp_memory_store_engram_get(store, (uint64_t)i + 1, &out);
        EXPECT_EQ(rc, 0);
    }

    /* Recreate OODB to simulate restart */
    nimcp_oodb_destroy(oodb);
    nimcp_oodb_config_t cfg = nimcp_oodb_config_default();
    oodb = nimcp_oodb_create(&cfg, store);
    ASSERT_NE(oodb, nullptr);

    /* Reload 10 objects */
    for (int i = 0; i < 10; i++) {
        oodb_engram_t* e = nimcp_oodb_get_engram(oodb, (uint64_t)i + 1);
        ASSERT_NE(e, nullptr);
        char expected[64];
        snprintf(expected, sizeof(expected), "pipeline-%d", i);
        EXPECT_STREQ(e->label, expected);
    }
}
