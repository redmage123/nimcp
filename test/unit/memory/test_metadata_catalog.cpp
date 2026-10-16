/**
 * @file test_metadata_catalog.cpp
 * @brief Unit tests for NIMCP metadata catalog APIs in nimcp_memory_store
 *
 * WHAT: 15 unit tests covering metadata put/get, tag search, text search,
 *       provenance search, tag appending, null handling, and multi-type search.
 * WHY:  The metadata catalog is the unified cross-type index that enables
 *       type-agnostic search across engrams, concepts, and autobio records.
 * HOW:  Google Test with a per-test temp SQLite database.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdint>
#include <unistd.h>

extern "C" {
#include "memory/nimcp_memory_store.h"
}

// ============================================================================
// Test Fixture
// ============================================================================

class MetadataCatalogTest : public ::testing::Test {
protected:
    nimcp_memory_store_t* store = nullptr;
    char db_path[256];

    void SetUp() override {
        snprintf(db_path, sizeof(db_path), "/tmp/nimcp_metadata_test_%d.db", getpid());
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

    nimcp_metadata_record_t make_metadata(uint64_t id,
                                           nimcp_memory_entry_type_t type,
                                           const char* label,
                                           const char* tags,
                                           float importance,
                                           uint32_t training_step,
                                           uint32_t curriculum_stage,
                                           uint32_t device_id) {
        nimcp_metadata_record_t r;
        memset(&r, 0, sizeof(r));
        r.entry_id = id;
        r.type = type;
        r.timestamp_us = id * 1000;
        r.importance = importance;
        r.source_device_id = device_id;
        r.training_step = training_step;
        r.curriculum_stage = curriculum_stage;
        r.model_version = 1;
        if (label) strncpy(r.label, label, sizeof(r.label) - 1);
        if (tags) strncpy(r.tags, tags, sizeof(r.tags) - 1);
        return r;
    }
};

// ============================================================================
// MetadataPutAndGet
// ============================================================================

TEST_F(MetadataCatalogTest, MetadataPutAndGet) {
    ASSERT_NE(store, nullptr);
    nimcp_metadata_record_t rec = make_metadata(
        1, NIMCP_MEMORY_TYPE_ENGRAM, "red cardinal sighting",
        "bird,visual", 0.8f, 100, 1, 0);
    ASSERT_EQ(nimcp_memory_store_metadata_put(store, &rec), 0);

    nimcp_metadata_record_t out;
    memset(&out, 0, sizeof(out));
    ASSERT_EQ(nimcp_memory_store_metadata_get(store, 1, NIMCP_MEMORY_TYPE_ENGRAM, &out), 0);
    EXPECT_EQ(out.entry_id, 1u);
    EXPECT_EQ(out.type, NIMCP_MEMORY_TYPE_ENGRAM);
    EXPECT_STREQ(out.label, "red cardinal sighting");
    EXPECT_FLOAT_EQ(out.importance, 0.8f);
    EXPECT_EQ(out.training_step, 100u);
    EXPECT_EQ(out.curriculum_stage, 1u);
}

// ============================================================================
// MetadataSearchTags
// ============================================================================

TEST_F(MetadataCatalogTest, MetadataSearchTags) {
    ASSERT_NE(store, nullptr);
    nimcp_metadata_record_t r1 = make_metadata(
        1, NIMCP_MEMORY_TYPE_ENGRAM, "cardinal", "bird,visual", 0.8f, 100, 1, 0);
    nimcp_metadata_record_t r2 = make_metadata(
        2, NIMCP_MEMORY_TYPE_ENGRAM, "symphony", "audio,music", 0.6f, 110, 1, 0);
    ASSERT_EQ(nimcp_memory_store_metadata_put(store, &r1), 0);
    ASSERT_EQ(nimcp_memory_store_metadata_put(store, &r2), 0);

    nimcp_metadata_search_result_t* result =
        nimcp_memory_store_metadata_search_tags(store, "bird", 10);
    ASSERT_NE(result, nullptr);
    EXPECT_GE(result->count, 1u);

    bool found_cardinal = false;
    for (uint32_t i = 0; i < result->count; i++) {
        if (result->records[i].entry_id == 1) found_cardinal = true;
    }
    EXPECT_TRUE(found_cardinal);
    nimcp_metadata_search_result_destroy(result);
}

// ============================================================================
// MetadataSearchTagsMultiple
// ============================================================================

TEST_F(MetadataCatalogTest, MetadataSearchTagsMultiple) {
    ASSERT_NE(store, nullptr);
    for (uint64_t i = 1; i <= 5; i++) {
        nimcp_metadata_record_t r = make_metadata(
            i, NIMCP_MEMORY_TYPE_ENGRAM, "tagged item",
            "common_tag,unique", 0.5f, (uint32_t)i * 10, 0, 0);
        ASSERT_EQ(nimcp_memory_store_metadata_put(store, &r), 0);
    }

    nimcp_metadata_search_result_t* result =
        nimcp_memory_store_metadata_search_tags(store, "common_tag", 20);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->count, 5u);
    nimcp_metadata_search_result_destroy(result);
}

// ============================================================================
// MetadataSearchTagsNoMatch
// ============================================================================

TEST_F(MetadataCatalogTest, MetadataSearchTagsNoMatch) {
    ASSERT_NE(store, nullptr);
    nimcp_metadata_record_t r = make_metadata(
        1, NIMCP_MEMORY_TYPE_ENGRAM, "item", "bird,visual", 0.5f, 10, 0, 0);
    ASSERT_EQ(nimcp_memory_store_metadata_put(store, &r), 0);

    nimcp_metadata_search_result_t* result =
        nimcp_memory_store_metadata_search_tags(store, "dinosaur", 10);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->count, 0u);
    nimcp_metadata_search_result_destroy(result);
}

// ============================================================================
// MetadataSearchText
// ============================================================================

TEST_F(MetadataCatalogTest, MetadataSearchText) {
    ASSERT_NE(store, nullptr);
    nimcp_metadata_record_t r1 = make_metadata(
        1, NIMCP_MEMORY_TYPE_ENGRAM, "beautiful red cardinal",
        "bird", 0.8f, 100, 1, 0);
    nimcp_metadata_record_t r2 = make_metadata(
        2, NIMCP_MEMORY_TYPE_CONCEPT, "music theory basics",
        "audio", 0.6f, 110, 1, 0);
    ASSERT_EQ(nimcp_memory_store_metadata_put(store, &r1), 0);
    ASSERT_EQ(nimcp_memory_store_metadata_put(store, &r2), 0);

    nimcp_metadata_search_result_t* result =
        nimcp_memory_store_metadata_search_text(store, "cardinal", 10);
    ASSERT_NE(result, nullptr);
    EXPECT_GE(result->count, 1u);
    if (result->count > 0) {
        EXPECT_EQ(result->records[0].entry_id, 1u);
    }
    nimcp_metadata_search_result_destroy(result);
}

// ============================================================================
// MetadataSearchProvenance
// ============================================================================

TEST_F(MetadataCatalogTest, MetadataSearchProvenance) {
    ASSERT_NE(store, nullptr);
    nimcp_metadata_record_t r1 = make_metadata(
        1, NIMCP_MEMORY_TYPE_ENGRAM, "early", "tag", 0.5f, 50, 0, 0);
    nimcp_metadata_record_t r2 = make_metadata(
        2, NIMCP_MEMORY_TYPE_ENGRAM, "mid", "tag", 0.5f, 150, 1, 0);
    nimcp_metadata_record_t r3 = make_metadata(
        3, NIMCP_MEMORY_TYPE_ENGRAM, "late", "tag", 0.5f, 300, 2, 0);
    ASSERT_EQ(nimcp_memory_store_metadata_put(store, &r1), 0);
    ASSERT_EQ(nimcp_memory_store_metadata_put(store, &r2), 0);
    ASSERT_EQ(nimcp_memory_store_metadata_put(store, &r3), 0);

    /* Search step range [100, 200], any stage, any device */
    nimcp_metadata_search_result_t* result =
        nimcp_memory_store_metadata_search_provenance(store, 100, 200, -1, -1, 10);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->count, 1u);
    if (result->count > 0) {
        EXPECT_EQ(result->records[0].entry_id, 2u);
    }
    nimcp_metadata_search_result_destroy(result);
}

// ============================================================================
// MetadataSearchProvenanceByStage
// ============================================================================

TEST_F(MetadataCatalogTest, MetadataSearchProvenanceByStage) {
    ASSERT_NE(store, nullptr);
    nimcp_metadata_record_t r1 = make_metadata(
        1, NIMCP_MEMORY_TYPE_ENGRAM, "stage0", "tag", 0.5f, 50, 0, 0);
    nimcp_metadata_record_t r2 = make_metadata(
        2, NIMCP_MEMORY_TYPE_ENGRAM, "stage1", "tag", 0.5f, 100, 1, 0);
    nimcp_metadata_record_t r3 = make_metadata(
        3, NIMCP_MEMORY_TYPE_CONCEPT, "stage1b", "tag", 0.5f, 150, 1, 0);
    ASSERT_EQ(nimcp_memory_store_metadata_put(store, &r1), 0);
    ASSERT_EQ(nimcp_memory_store_metadata_put(store, &r2), 0);
    ASSERT_EQ(nimcp_memory_store_metadata_put(store, &r3), 0);

    /* Filter by curriculum_stage=1, broad step range, any device */
    nimcp_metadata_search_result_t* result =
        nimcp_memory_store_metadata_search_provenance(store, 0, 1000, 1, -1, 10);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->count, 2u);
    nimcp_metadata_search_result_destroy(result);
}

// ============================================================================
// MetadataSearchProvenanceByDevice
// ============================================================================

TEST_F(MetadataCatalogTest, MetadataSearchProvenanceByDevice) {
    ASSERT_NE(store, nullptr);
    nimcp_metadata_record_t r1 = make_metadata(
        1, NIMCP_MEMORY_TYPE_ENGRAM, "local", "tag", 0.5f, 50, 0, 0);
    nimcp_metadata_record_t r2 = make_metadata(
        2, NIMCP_MEMORY_TYPE_ENGRAM, "drone1", "tag", 0.5f, 100, 0, 1);
    nimcp_metadata_record_t r3 = make_metadata(
        3, NIMCP_MEMORY_TYPE_ENGRAM, "drone2", "tag", 0.5f, 150, 0, 2);
    ASSERT_EQ(nimcp_memory_store_metadata_put(store, &r1), 0);
    ASSERT_EQ(nimcp_memory_store_metadata_put(store, &r2), 0);
    ASSERT_EQ(nimcp_memory_store_metadata_put(store, &r3), 0);

    /* Filter by device_id=1, broad step range, any stage */
    nimcp_metadata_search_result_t* result =
        nimcp_memory_store_metadata_search_provenance(store, 0, 1000, -1, 1, 10);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->count, 1u);
    if (result->count > 0) {
        EXPECT_EQ(result->records[0].entry_id, 2u);
    }
    nimcp_metadata_search_result_destroy(result);
}

// ============================================================================
// MetadataAddTags
// ============================================================================

TEST_F(MetadataCatalogTest, MetadataAddTags) {
    ASSERT_NE(store, nullptr);
    nimcp_metadata_record_t r = make_metadata(
        1, NIMCP_MEMORY_TYPE_ENGRAM, "cardinal", "bird", 0.8f, 100, 0, 0);
    ASSERT_EQ(nimcp_memory_store_metadata_put(store, &r), 0);

    ASSERT_EQ(nimcp_memory_store_metadata_add_tags(
        store, 1, NIMCP_MEMORY_TYPE_ENGRAM, "visual,red"), 0);

    /* Verify tags were appended */
    nimcp_metadata_record_t out;
    memset(&out, 0, sizeof(out));
    ASSERT_EQ(nimcp_memory_store_metadata_get(store, 1, NIMCP_MEMORY_TYPE_ENGRAM, &out), 0);
    EXPECT_NE(strstr(out.tags, "bird"), nullptr);
    EXPECT_NE(strstr(out.tags, "visual"), nullptr);
    EXPECT_NE(strstr(out.tags, "red"), nullptr);
}

// ============================================================================
// MetadataAddTagsToNew (non-existent entry)
// ============================================================================

TEST_F(MetadataCatalogTest, MetadataAddTagsToNew) {
    ASSERT_NE(store, nullptr);
    /* Adding tags to a non-existent entry — implementation may upsert or fail.
     * Either way, should not crash and return a valid status. */
    int rc = nimcp_memory_store_metadata_add_tags(
        store, 9999, NIMCP_MEMORY_TYPE_ENGRAM, "newtag");
    /* rc == 0 (upsert) or rc != 0 (strict not-found) — both valid */
    (void)rc;
}

// ============================================================================
// MetadataNullHandling
// ============================================================================

TEST_F(MetadataCatalogTest, MetadataNullHandling) {
    /* NULL store */
    nimcp_metadata_record_t rec;
    memset(&rec, 0, sizeof(rec));
    EXPECT_NE(nimcp_memory_store_metadata_put(nullptr, &rec), 0);

    /* NULL record */
    ASSERT_NE(store, nullptr);
    EXPECT_NE(nimcp_memory_store_metadata_put(store, nullptr), 0);

    /* NULL query */
    nimcp_metadata_search_result_t* result =
        nimcp_memory_store_metadata_search_tags(nullptr, "bird", 10);
    EXPECT_EQ(result, nullptr);

    result = nimcp_memory_store_metadata_search_tags(store, nullptr, 10);
    /* Either NULL or empty result is acceptable */
    if (result) {
        EXPECT_EQ(result->count, 0u);
        nimcp_metadata_search_result_destroy(result);
    }

    /* NULL store for metadata_get */
    EXPECT_NE(nimcp_memory_store_metadata_get(nullptr, 1, NIMCP_MEMORY_TYPE_ENGRAM, &rec), 0);
}

// ============================================================================
// MetadataEmptyTags
// ============================================================================

TEST_F(MetadataCatalogTest, MetadataEmptyTags) {
    ASSERT_NE(store, nullptr);
    nimcp_metadata_record_t r = make_metadata(
        1, NIMCP_MEMORY_TYPE_ENGRAM, "no tags", "", 0.5f, 10, 0, 0);
    ASSERT_EQ(nimcp_memory_store_metadata_put(store, &r), 0);

    nimcp_metadata_search_result_t* result =
        nimcp_memory_store_metadata_search_tags(store, "", 10);
    /* Empty tag search — should return nothing or handle gracefully */
    if (result) {
        nimcp_metadata_search_result_destroy(result);
    }
}

// ============================================================================
// MetadataSearchResultDestroy
// ============================================================================

TEST_F(MetadataCatalogTest, MetadataSearchResultDestroy) {
    /* Destroying NULL should not crash */
    nimcp_metadata_search_result_destroy(nullptr);

    ASSERT_NE(store, nullptr);
    nimcp_metadata_record_t r = make_metadata(
        1, NIMCP_MEMORY_TYPE_ENGRAM, "destroyable", "tag", 0.5f, 10, 0, 0);
    ASSERT_EQ(nimcp_memory_store_metadata_put(store, &r), 0);

    nimcp_metadata_search_result_t* result =
        nimcp_memory_store_metadata_search_tags(store, "tag", 10);
    ASSERT_NE(result, nullptr);
    nimcp_metadata_search_result_destroy(result);
    /* No crash = pass */
}

// ============================================================================
// MetadataProvenance with curriculum_stage=-1 (any)
// ============================================================================

TEST_F(MetadataCatalogTest, MetadataProvenanceAnyStage) {
    ASSERT_NE(store, nullptr);
    for (uint32_t stage = 0; stage < 3; stage++) {
        nimcp_metadata_record_t r = make_metadata(
            stage + 1, NIMCP_MEMORY_TYPE_ENGRAM, "multi-stage",
            "tag", 0.5f, 100, stage, 0);
        ASSERT_EQ(nimcp_memory_store_metadata_put(store, &r), 0);
    }

    /* curriculum_stage=-1 means any */
    nimcp_metadata_search_result_t* result =
        nimcp_memory_store_metadata_search_provenance(store, 0, 1000, -1, -1, 20);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->count, 3u);
    nimcp_metadata_search_result_destroy(result);
}

// ============================================================================
// MetadataMultipleTypes
// ============================================================================

TEST_F(MetadataCatalogTest, MetadataMultipleTypes) {
    ASSERT_NE(store, nullptr);
    nimcp_metadata_record_t r1 = make_metadata(
        1, NIMCP_MEMORY_TYPE_ENGRAM, "engram entry",
        "shared_tag", 0.5f, 100, 0, 0);
    nimcp_metadata_record_t r2 = make_metadata(
        2, NIMCP_MEMORY_TYPE_CONCEPT, "concept entry",
        "shared_tag", 0.6f, 110, 0, 0);
    nimcp_metadata_record_t r3 = make_metadata(
        3, NIMCP_MEMORY_TYPE_AUTOBIO, "autobio entry",
        "shared_tag", 0.7f, 120, 0, 0);
    ASSERT_EQ(nimcp_memory_store_metadata_put(store, &r1), 0);
    ASSERT_EQ(nimcp_memory_store_metadata_put(store, &r2), 0);
    ASSERT_EQ(nimcp_memory_store_metadata_put(store, &r3), 0);

    nimcp_metadata_search_result_t* result =
        nimcp_memory_store_metadata_search_tags(store, "shared_tag", 20);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->count, 3u);

    /* Verify all types present */
    bool found_engram = false, found_concept = false, found_autobio = false;
    for (uint32_t i = 0; i < result->count; i++) {
        if (result->records[i].type == NIMCP_MEMORY_TYPE_ENGRAM) found_engram = true;
        if (result->records[i].type == NIMCP_MEMORY_TYPE_CONCEPT) found_concept = true;
        if (result->records[i].type == NIMCP_MEMORY_TYPE_AUTOBIO) found_autobio = true;
    }
    EXPECT_TRUE(found_engram);
    EXPECT_TRUE(found_concept);
    EXPECT_TRUE(found_autobio);
    nimcp_metadata_search_result_destroy(result);
}
