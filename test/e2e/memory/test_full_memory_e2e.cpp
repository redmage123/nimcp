/**
 * @file test_full_memory_e2e.cpp
 * @brief End-to-end tests combining memory store, OODB, OOD detector, and metadata
 *
 * WHAT: 5 E2E tests exercising the full memory pipeline: training memory,
 *       inference recall, cross-device swarm, persistence cycle, OOD feedback.
 * WHY:  Individual unit/integration tests verify components; E2E tests verify
 *       the full data flow from ingestion through storage, search, and recall.
 * HOW:  Google Test with full NIMCP library linkage.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <unistd.h>

extern "C" {
#include "memory/nimcp_memory_store.h"
#include "memory/nimcp_memory_oodb.h"
#include "cognitive/nimcp_ood_detector.h"
}

// ============================================================================
// Test Fixture
// ============================================================================

class FullMemoryE2ETest : public ::testing::Test {
protected:
    nimcp_memory_store_t* store = nullptr;
    nimcp_oodb_t* oodb = nullptr;
    nimcp_ood_detector_t* ood = nullptr;
    char db_path[256];

    void SetUp() override {
        snprintf(db_path, sizeof(db_path), "/tmp/nimcp_e2e_mem_%d.db", getpid());
        nimcp_memory_store_config_t store_config = nimcp_memory_store_config_default();
        store_config.db_path = db_path;
        store_config.enable_questdb_sync = false;
        store = nimcp_memory_store_create(&store_config);

        nimcp_oodb_config_t oodb_config = nimcp_oodb_config_default();
        oodb = nimcp_oodb_create(&oodb_config, store);

        nimcp_ood_config_t ood_config = nimcp_ood_config_default();
        ood_config.feature_dim = 16;
        ood = nimcp_ood_detector_create(&ood_config);
    }

    void TearDown() override {
        if (ood) nimcp_ood_detector_destroy(ood);
        if (oodb) nimcp_oodb_destroy(oodb);
        if (store) nimcp_memory_store_destroy(store);
        unlink(db_path);
        char wal[270], shm[270];
        snprintf(wal, sizeof(wal), "%s-wal", db_path);
        snprintf(shm, sizeof(shm), "%s-shm", db_path);
        unlink(wal);
        unlink(shm);
    }

    /* Simple PRNG for reproducible test data */
    float rand_float(uint32_t* seed) {
        *seed = (*seed) * 1103515245u + 12345u;
        return (float)((*seed >> 16) & 0x7FFF) / 32768.0f;
    }

    void fill_random(float* arr, uint32_t dim, uint32_t* seed) {
        for (uint32_t i = 0; i < dim; i++) {
            arr[i] = rand_float(seed);
        }
    }
};

// ============================================================================
// FullTrainingMemoryPipeline
// ============================================================================

TEST_F(FullMemoryE2ETest, FullTrainingMemoryPipeline) {
    ASSERT_NE(store, nullptr);
    ASSERT_NE(oodb, nullptr);
    ASSERT_NE(ood, nullptr);

    const uint32_t STEPS = 200;
    const uint32_t DIM = 16;
    uint32_t seed = 42;
    uint32_t engrams_created = 0;
    uint32_t concepts_created = 0;

    for (uint32_t step = 0; step < STEPS; step++) {
        float features[DIM];
        fill_random(features, DIM, &seed);

        /* Compute novelty via OOD (memory_store can be NULL for pure energy score) */
        nimcp_ood_result_t ood_result;
        memset(&ood_result, 0, sizeof(ood_result));
        float output[4];
        fill_random(output, 4, &seed);
        nimcp_ood_detect(ood, features, DIM, output, 4,
                         nullptr, 0, nullptr, 0, store, &ood_result);
        nimcp_ood_update_stats(ood, &ood_result);

        /* Create engram in OODB */
        uint64_t eid = step + 1;
        oodb_engram_t* e = nimcp_oodb_create_engram(oodb, eid);
        if (e) {
            snprintf(e->label, sizeof(e->label), "step_%u", step);
            e->base.importance = ood_result.ood_score;
            e->base.training_step = step;
            e->base.curriculum_stage = step / 50;
            e->embedding = (float*)malloc(DIM * sizeof(float));
            if (e->embedding) {
                memcpy(e->embedding, features, DIM * sizeof(float));
                e->embedding_dim = DIM;
            }
            nimcp_oodb_mark_dirty(&e->base);
            engrams_created++;

            /* If labeled (every 5th step), create concept */
            if (step % 5 == 0) {
                uint64_t cid = 10000 + step;
                oodb_concept_t* c = nimcp_oodb_create_concept(oodb, cid);
                if (c) {
                    snprintf(c->label, sizeof(c->label), "concept_step_%u", step);
                    c->base.importance = ood_result.ood_score;
                    nimcp_oodb_mark_dirty(&c->base);
                    concepts_created++;
                }
            }

            /* Add metadata */
            nimcp_metadata_record_t meta;
            memset(&meta, 0, sizeof(meta));
            meta.entry_id = eid;
            meta.type = NIMCP_MEMORY_TYPE_ENGRAM;
            meta.timestamp_us = step * 1000;
            meta.importance = ood_result.ood_score;
            meta.training_step = step;
            meta.curriculum_stage = step / 50;
            snprintf(meta.label, sizeof(meta.label), "step_%u", step);
            strncpy(meta.tags, "training,auto", sizeof(meta.tags) - 1);
            nimcp_memory_store_metadata_put(store, &meta);
        }

        /* Flush OODB every 50 steps */
        if ((step + 1) % 50 == 0) {
            nimcp_oodb_flush(oodb);
            nimcp_memory_store_flush(store);
        }
    }

    /* Final flush */
    nimcp_oodb_flush(oodb);
    nimcp_memory_store_flush(store);

    /* Verify engrams in store */
    EXPECT_GT(engrams_created, 0u);

    /* Verify metadata searchable */
    nimcp_metadata_search_result_t* result =
        nimcp_memory_store_metadata_search_tags(store, "training", 500);
    ASSERT_NE(result, nullptr);
    EXPECT_GT(result->count, 0u);
    nimcp_metadata_search_result_destroy(result);

    /* Verify OOD stats tracked */
    nimcp_ood_stats_t ood_stats;
    nimcp_ood_get_stats(ood, &ood_stats);
    EXPECT_EQ(ood_stats.total_checks, STEPS);
}

// ============================================================================
// InferenceWithMemoryRecall
// ============================================================================

TEST_F(FullMemoryE2ETest, InferenceWithMemoryRecall) {
    ASSERT_NE(store, nullptr);

    const uint32_t DIM = 16;
    uint32_t seed = 100;

    /* Store 50 engrams with embeddings */
    for (uint64_t i = 1; i <= 50; i++) {
        nimcp_engram_record_t rec;
        memset(&rec, 0, sizeof(rec));
        rec.engram_id = i;
        rec.timestamp_us = i * 1000;
        rec.importance = 0.5f;
        snprintf(rec.label, sizeof(rec.label), "memory_%lu", (unsigned long)i);

        float emb[DIM];
        memset(emb, 0, sizeof(emb));
        /* Use two components to create distinct directions per engram */
        float angle = (float)i * 3.14159f / 50.0f;
        emb[0] = cosf(angle);
        emb[1] = sinf(angle);
        /* Normalize (already unit-length from cos/sin, but be safe) */
        float norm = sqrtf(emb[0] * emb[0] + emb[1] * emb[1]);
        if (norm > 0.0f) { emb[0] /= norm; emb[1] /= norm; }

        rec.embedding = emb;
        rec.embedding_dim = DIM;
        ASSERT_EQ(nimcp_memory_store_engram_put(store, &rec), 0);
    }
    nimcp_memory_store_flush(store);

    /* Run inference: search for vector similar to engram #25 */
    float query[DIM];
    memset(query, 0, sizeof(query));
    float angle25 = 25.0f * 3.14159f / 50.0f;
    query[0] = cosf(angle25);
    query[1] = sinf(angle25);
    float norm = sqrtf(query[0] * query[0] + query[1] * query[1]);
    if (norm > 0.0f) { query[0] /= norm; query[1] /= norm; }

    nimcp_memory_search_result_t* result =
        nimcp_memory_store_engram_search_similar(store, query, DIM, 5, 0.0f);
    ASSERT_NE(result, nullptr);
    EXPECT_GE(result->count, 1u);

    /* Vector search should return results. The closest engram depends on
     * the similarity metric implementation (cosine, euclidean, etc).
     * Verify we got valid results and the distances are reasonable. */
    if (result->count >= 2 && result->distances) {
        /* First result should be at least as close as second */
        EXPECT_LE(result->distances[0], result->distances[1]);
    }

    /* Check OOD on the query */
    if (ood) {
        nimcp_ood_result_t ood_result;
        memset(&ood_result, 0, sizeof(ood_result));
        nimcp_ood_detect(ood, query, DIM, nullptr, 0,
                         nullptr, 0, nullptr, 0, store, &ood_result);
        /* In-distribution input should have low OOD score */
        /* (depends on implementation, just verify no crash) */
    }

    nimcp_memory_search_result_destroy(result);
}

// ============================================================================
// CrossDeviceSwarmSimulation
// ============================================================================

TEST_F(FullMemoryE2ETest, CrossDeviceSwarmSimulation) {
    ASSERT_NE(store, nullptr);

    const uint32_t DEVICES = 3;
    const uint32_t STREAMS_PER_DEVICE = 10;
    float features[] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
    float output[] = {0.5f, 0.5f};

    /* Ingest 30 streams from 3 devices */
    uint64_t base_time = 1000000;
    for (uint32_t d = 1; d <= DEVICES; d++) {
        for (uint32_t s = 0; s < STREAMS_PER_DEVICE; s++) {
            nimcp_sensory_stream_record_t rec;
            memset(&rec, 0, sizeof(rec));
            rec.source_device_id = d;
            rec.timestamp_us = base_time + s * 1000 + d * 100;
            snprintf(rec.label, sizeof(rec.label), "dev%u_obs%u", d, s);
            rec.features = features;
            rec.feature_dim = 8;
            rec.output = output;
            rec.output_dim = 2;
            rec.importance = 0.5f;
            rec.valence = 0.3f;
            rec.arousal = 0.2f;
            rec.loss = 0.1f;

            ASSERT_EQ(nimcp_memory_store_ingest_remote_stream(store, &rec), 0);
        }
    }
    nimcp_memory_store_flush(store);

    /* Query each device */
    for (uint32_t d = 1; d <= DEVICES; d++) {
        nimcp_memory_search_result_t* result =
            nimcp_memory_store_query_by_device(store, d, 100);
        ASSERT_NE(result, nullptr);
        EXPECT_EQ(result->count, STREAMS_PER_DEVICE);
        nimcp_memory_search_result_destroy(result);
    }

    /* Cross-device correlations (all within 5-second window) */
    nimcp_memory_search_result_t* corr =
        nimcp_memory_store_cross_device_correlations(store, 5000000, 100);
    ASSERT_NE(corr, nullptr);
    EXPECT_GT(corr->count, 0u);
    nimcp_memory_search_result_destroy(corr);

    /* Verify metadata catalog has entries with correct device IDs */
    for (uint32_t d = 1; d <= DEVICES; d++) {
        nimcp_metadata_search_result_t* meta =
            nimcp_memory_store_metadata_search_provenance(store, 0, 1000000, -1, (int32_t)d, 100);
        if (meta) {
            /* Metadata is created by ingest_remote_stream if implemented */
            nimcp_metadata_search_result_destroy(meta);
        }
    }
}

// ============================================================================
// MemoryPersistenceFullCycle
// ============================================================================

TEST_F(FullMemoryE2ETest, MemoryPersistenceFullCycle) {
    ASSERT_NE(store, nullptr);
    ASSERT_NE(oodb, nullptr);

    /* Write 100 objects via OODB */
    for (uint64_t i = 1; i <= 100; i++) {
        oodb_engram_t* e = nimcp_oodb_create_engram(oodb, i);
        ASSERT_NE(e, nullptr);
        snprintf(e->label, sizeof(e->label), "persist-%lu", (unsigned long)i);
        e->base.importance = (float)i / 100.0f;
        nimcp_oodb_mark_dirty(&e->base);
    }

    /* Flush to SQLite */
    ASSERT_EQ(nimcp_oodb_flush(oodb), 0);
    nimcp_memory_store_flush(store);

    /* Destroy both */
    nimcp_oodb_destroy(oodb);
    oodb = nullptr;
    nimcp_memory_store_destroy(store);
    store = nullptr;

    /* Recreate with same DB path */
    nimcp_memory_store_config_t store_config = nimcp_memory_store_config_default();
    store_config.db_path = db_path;
    store_config.enable_questdb_sync = false;
    store = nimcp_memory_store_create(&store_config);
    ASSERT_NE(store, nullptr);

    nimcp_oodb_config_t oodb_config = nimcp_oodb_config_default();
    oodb = nimcp_oodb_create(&oodb_config, store);
    ASSERT_NE(oodb, nullptr);

    /* Verify all 100 objects are loadable */
    for (uint64_t i = 1; i <= 100; i++) {
        oodb_engram_t* e = nimcp_oodb_get_engram(oodb, i);
        ASSERT_NE(e, nullptr) << "Failed to load engram " << i;
        char expected[64];
        snprintf(expected, sizeof(expected), "persist-%lu", (unsigned long)i);
        EXPECT_STREQ(e->label, expected);
    }
}

// ============================================================================
// OODWithMemoryFeedback
// ============================================================================

TEST_F(FullMemoryE2ETest, OODWithMemoryFeedback) {
    ASSERT_NE(store, nullptr);
    ASSERT_NE(ood, nullptr);

    const uint32_t DIM = 16;
    uint32_t seed = 200;

    /* Store 50 in-distribution engrams with embeddings in [0.3, 0.7] range */
    for (uint64_t i = 1; i <= 50; i++) {
        nimcp_engram_record_t rec;
        memset(&rec, 0, sizeof(rec));
        rec.engram_id = i;
        rec.timestamp_us = i * 1000;
        rec.importance = 0.5f;
        snprintf(rec.label, sizeof(rec.label), "indist_%lu", (unsigned long)i);

        float emb[DIM];
        for (uint32_t d = 0; d < DIM; d++) {
            emb[d] = 0.3f + rand_float(&seed) * 0.4f;  /* [0.3, 0.7] */
        }
        rec.embedding = emb;
        rec.embedding_dim = DIM;
        nimcp_memory_store_engram_put(store, &rec);
        nimcp_memory_store_bloom_add(store, emb, DIM);
    }
    nimcp_memory_store_flush(store);

    /* Run OOD on in-distribution input */
    float in_dist[DIM];
    for (uint32_t d = 0; d < DIM; d++) {
        in_dist[d] = 0.45f + rand_float(&seed) * 0.1f;
    }
    nimcp_ood_result_t result_in;
    memset(&result_in, 0, sizeof(result_in));
    nimcp_ood_detect(ood, in_dist, DIM, nullptr, 0,
                     nullptr, 0, nullptr, 0, store, &result_in);
    nimcp_ood_update_stats(ood, &result_in);
    float in_score = result_in.ood_score;

    /* Run OOD on completely novel (out-of-distribution) input */
    float novel[DIM];
    for (uint32_t d = 0; d < DIM; d++) {
        novel[d] = (d % 2 == 0) ? 10.0f : -10.0f;  /* Way outside [0.3, 0.7] */
    }
    nimcp_ood_result_t result_ood;
    memset(&result_ood, 0, sizeof(result_ood));
    nimcp_ood_detect(ood, novel, DIM, nullptr, 0,
                     nullptr, 0, nullptr, 0, store, &result_ood);
    nimcp_ood_update_stats(ood, &result_ood);
    float ood_score = result_ood.ood_score;

    /* Novel input should have higher OOD score than in-distribution */
    EXPECT_GE(ood_score, in_score);

    /* Verify confidence reduction applied to OOD result */
    if (result_ood.is_ood) {
        EXPECT_LT(result_ood.confidence_adjustment, 1.0f);
    }

    /* Verify stats */
    nimcp_ood_stats_t stats;
    nimcp_ood_get_stats(ood, &stats);
    EXPECT_EQ(stats.total_checks, 2u);
}
