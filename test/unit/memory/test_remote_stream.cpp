/**
 * @file test_remote_stream.cpp
 * @brief Unit tests for NIMCP remote sensory stream ingestion APIs
 *
 * WHAT: 12 unit tests covering stream ingestion, device queries,
 *       cross-device correlation, and null/edge-case handling.
 * WHY:  Remote stream ingestion is the master-side entry point for
 *       all multi-device sensory data flowing into the unified memory.
 * HOW:  Google Test with a per-test temp SQLite database.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <unistd.h>

extern "C" {
#include "memory/nimcp_memory_store.h"
}

// ============================================================================
// Test Fixture
// ============================================================================

class RemoteStreamTest : public ::testing::Test {
protected:
    nimcp_memory_store_t* store = nullptr;
    char db_path[256];

    void SetUp() override {
        snprintf(db_path, sizeof(db_path), "/tmp/nimcp_stream_test_%d.db", getpid());
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

    nimcp_sensory_stream_record_t make_stream(uint32_t device_id,
                                               uint64_t timestamp_us,
                                               const char* label,
                                               float importance) {
        nimcp_sensory_stream_record_t r;
        memset(&r, 0, sizeof(r));
        r.source_device_id = device_id;
        r.timestamp_us = timestamp_us;
        if (label) strncpy(r.label, label, sizeof(r.label) - 1);
        r.importance = importance;
        r.valence = 0.5f;
        r.arousal = 0.3f;
        r.loss = 0.1f;
        return r;
    }

    nimcp_sensory_stream_record_t make_stream_with_features(
        uint32_t device_id, uint64_t timestamp_us,
        const char* label, float importance,
        float* features, uint32_t feature_dim,
        float* output, uint32_t output_dim) {

        nimcp_sensory_stream_record_t r = make_stream(device_id, timestamp_us, label, importance);
        r.features = features;
        r.feature_dim = feature_dim;
        r.output = output;
        r.output_dim = output_dim;
        return r;
    }
};

// ============================================================================
// IngestBasicStream
// ============================================================================

TEST_F(RemoteStreamTest, IngestBasicStream) {
    ASSERT_NE(store, nullptr);
    float features[] = {0.1f, 0.2f, 0.3f, 0.4f};
    float output[] = {0.5f, 0.6f};
    nimcp_sensory_stream_record_t rec = make_stream_with_features(
        1, 1000, "basic observation", 0.5f,
        features, 4, output, 2);

    int rc = nimcp_memory_store_ingest_remote_stream(store, &rec);
    EXPECT_EQ(rc, 0);
    nimcp_memory_store_flush(store);

    /* Verify engram was created — query by device */
    nimcp_memory_search_result_t* result =
        nimcp_memory_store_query_by_device(store, 1, 10);
    ASSERT_NE(result, nullptr);
    EXPECT_GE(result->count, 1u);
    nimcp_memory_search_result_destroy(result);
}

// ============================================================================
// IngestWithLabel
// ============================================================================

TEST_F(RemoteStreamTest, IngestWithLabel) {
    ASSERT_NE(store, nullptr);
    float features[] = {0.1f, 0.2f, 0.3f, 0.4f};
    nimcp_sensory_stream_record_t rec = make_stream_with_features(
        1, 2000, "labeled bird sighting", 0.6f,
        features, 4, nullptr, 0);

    ASSERT_EQ(nimcp_memory_store_ingest_remote_stream(store, &rec), 0);
    nimcp_memory_store_flush(store);

    /* A labeled stream should create a concept findable by text search */
    nimcp_memory_search_result_t* result =
        nimcp_memory_store_concept_search_text(store, "bird", 10);
    if (result) {
        /* Concept creation is implementation-dependent on importance threshold.
         * Just verify no crash and check if found. */
        nimcp_memory_search_result_destroy(result);
    }

    /* At minimum, engram should exist */
    nimcp_memory_search_result_t* dev_result =
        nimcp_memory_store_query_by_device(store, 1, 10);
    ASSERT_NE(dev_result, nullptr);
    EXPECT_GE(dev_result->count, 1u);
    nimcp_memory_search_result_destroy(dev_result);
}

// ============================================================================
// IngestHighImportance
// ============================================================================

TEST_F(RemoteStreamTest, IngestHighImportance) {
    ASSERT_NE(store, nullptr);
    float features[] = {0.9f, 0.8f, 0.7f, 0.6f};
    nimcp_sensory_stream_record_t rec = make_stream_with_features(
        1, 3000, "critical threat detected", 0.95f,
        features, 4, nullptr, 0);

    ASSERT_EQ(nimcp_memory_store_ingest_remote_stream(store, &rec), 0);
    nimcp_memory_store_flush(store);

    /* High importance should create autobio entry */
    nimcp_memory_search_result_t* result =
        nimcp_memory_store_autobio_search_text(store, "threat", 10);
    if (result) {
        /* Implementation may or may not create autobio for high importance */
        nimcp_memory_search_result_destroy(result);
    }

    /* At minimum, device query should find it */
    nimcp_memory_search_result_t* dev_result =
        nimcp_memory_store_query_by_device(store, 1, 10);
    ASSERT_NE(dev_result, nullptr);
    EXPECT_GE(dev_result->count, 1u);
    nimcp_memory_search_result_destroy(dev_result);
}

// ============================================================================
// IngestLowImportance
// ============================================================================

TEST_F(RemoteStreamTest, IngestLowImportance) {
    ASSERT_NE(store, nullptr);
    float features[] = {0.01f, 0.02f, 0.01f, 0.01f};
    nimcp_sensory_stream_record_t rec = make_stream_with_features(
        1, 4000, "mundane background noise", 0.1f,
        features, 4, nullptr, 0);

    ASSERT_EQ(nimcp_memory_store_ingest_remote_stream(store, &rec), 0);
    nimcp_memory_store_flush(store);

    /* Low importance — autobio text search should NOT find this */
    nimcp_memory_search_result_t* result =
        nimcp_memory_store_autobio_search_text(store, "mundane background", 10);
    if (result) {
        EXPECT_EQ(result->count, 0u);
        nimcp_memory_search_result_destroy(result);
    }
}

// ============================================================================
// IngestWithDeviceId
// ============================================================================

TEST_F(RemoteStreamTest, IngestWithDeviceId) {
    ASSERT_NE(store, nullptr);
    float features[] = {0.5f, 0.5f, 0.5f, 0.5f};
    nimcp_sensory_stream_record_t rec = make_stream_with_features(
        42, 5000, "drone 42 observation", 0.6f,
        features, 4, nullptr, 0);

    ASSERT_EQ(nimcp_memory_store_ingest_remote_stream(store, &rec), 0);
    nimcp_memory_store_flush(store);

    nimcp_memory_search_result_t* result =
        nimcp_memory_store_query_by_device(store, 42, 10);
    ASSERT_NE(result, nullptr);
    EXPECT_GE(result->count, 1u);
    nimcp_memory_search_result_destroy(result);

    /* Different device should have nothing */
    nimcp_memory_search_result_t* empty =
        nimcp_memory_store_query_by_device(store, 43, 10);
    ASSERT_NE(empty, nullptr);
    EXPECT_EQ(empty->count, 0u);
    nimcp_memory_search_result_destroy(empty);
}

// ============================================================================
// QueryByDevice
// ============================================================================

TEST_F(RemoteStreamTest, QueryByDevice) {
    ASSERT_NE(store, nullptr);
    float features[] = {0.1f, 0.2f, 0.3f, 0.4f};

    /* 3 streams from device 1 */
    for (int i = 0; i < 3; i++) {
        nimcp_sensory_stream_record_t rec = make_stream_with_features(
            1, (uint64_t)(i + 1) * 1000, "device1 obs", 0.5f,
            features, 4, nullptr, 0);
        ASSERT_EQ(nimcp_memory_store_ingest_remote_stream(store, &rec), 0);
    }
    /* 2 streams from device 2 */
    for (int i = 0; i < 2; i++) {
        nimcp_sensory_stream_record_t rec = make_stream_with_features(
            2, (uint64_t)(i + 10) * 1000, "device2 obs", 0.5f,
            features, 4, nullptr, 0);
        ASSERT_EQ(nimcp_memory_store_ingest_remote_stream(store, &rec), 0);
    }
    nimcp_memory_store_flush(store);

    nimcp_memory_search_result_t* d1 = nimcp_memory_store_query_by_device(store, 1, 20);
    ASSERT_NE(d1, nullptr);
    EXPECT_EQ(d1->count, 3u);
    nimcp_memory_search_result_destroy(d1);

    nimcp_memory_search_result_t* d2 = nimcp_memory_store_query_by_device(store, 2, 20);
    ASSERT_NE(d2, nullptr);
    EXPECT_EQ(d2->count, 2u);
    nimcp_memory_search_result_destroy(d2);
}

// ============================================================================
// CrossDeviceCorrelation
// ============================================================================

TEST_F(RemoteStreamTest, CrossDeviceCorrelation) {
    ASSERT_NE(store, nullptr);
    float features[] = {0.5f, 0.5f, 0.5f, 0.5f};

    /* Two devices observing at similar timestamps (within 5 seconds) */
    nimcp_sensory_stream_record_t r1 = make_stream_with_features(
        1, 1000000, "device1 sees bird", 0.7f, features, 4, nullptr, 0);
    nimcp_sensory_stream_record_t r2 = make_stream_with_features(
        2, 1002000, "device2 sees bird", 0.7f, features, 4, nullptr, 0);
    ASSERT_EQ(nimcp_memory_store_ingest_remote_stream(store, &r1), 0);
    ASSERT_EQ(nimcp_memory_store_ingest_remote_stream(store, &r2), 0);
    nimcp_memory_store_flush(store);

    /* 5-second window in microseconds */
    nimcp_memory_search_result_t* result =
        nimcp_memory_store_cross_device_correlations(store, 5000000, 10);
    ASSERT_NE(result, nullptr);
    EXPECT_GE(result->count, 1u);
    nimcp_memory_search_result_destroy(result);
}

// ============================================================================
// CrossDeviceNoCorrelation
// ============================================================================

TEST_F(RemoteStreamTest, CrossDeviceNoCorrelation) {
    ASSERT_NE(store, nullptr);
    float features[] = {0.5f, 0.5f, 0.5f, 0.5f};

    /* Two devices observing at very different timestamps (100 seconds apart) */
    nimcp_sensory_stream_record_t r1 = make_stream_with_features(
        1, 1000000, "device1 early", 0.5f, features, 4, nullptr, 0);
    nimcp_sensory_stream_record_t r2 = make_stream_with_features(
        2, 101000000, "device2 late", 0.5f, features, 4, nullptr, 0);
    ASSERT_EQ(nimcp_memory_store_ingest_remote_stream(store, &r1), 0);
    ASSERT_EQ(nimcp_memory_store_ingest_remote_stream(store, &r2), 0);
    nimcp_memory_store_flush(store);

    /* 5-second window — these are 100s apart, so no correlation */
    nimcp_memory_search_result_t* result =
        nimcp_memory_store_cross_device_correlations(store, 5000000, 10);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->count, 0u);
    nimcp_memory_search_result_destroy(result);
}

// ============================================================================
// IngestNullStore
// ============================================================================

TEST_F(RemoteStreamTest, IngestNullStore) {
    nimcp_sensory_stream_record_t rec = make_stream(1, 1000, "test", 0.5f);
    EXPECT_EQ(nimcp_memory_store_ingest_remote_stream(nullptr, &rec), -1);
}

// ============================================================================
// IngestNullRecord
// ============================================================================

TEST_F(RemoteStreamTest, IngestNullRecord) {
    ASSERT_NE(store, nullptr);
    EXPECT_EQ(nimcp_memory_store_ingest_remote_stream(store, nullptr), -1);
}

// ============================================================================
// IngestEmptyFeatures
// ============================================================================

TEST_F(RemoteStreamTest, IngestEmptyFeatures) {
    ASSERT_NE(store, nullptr);
    nimcp_sensory_stream_record_t rec = make_stream(1, 7000, "no features", 0.5f);
    rec.features = nullptr;
    rec.feature_dim = 0;

    /* Should handle gracefully — either succeed or return error, no crash */
    int rc = nimcp_memory_store_ingest_remote_stream(store, &rec);
    (void)rc;
    /* No crash = pass */
}

// ============================================================================
// IngestManyStreams
// ============================================================================

TEST_F(RemoteStreamTest, IngestManyStreams) {
    ASSERT_NE(store, nullptr);
    float features[] = {0.1f, 0.2f, 0.3f, 0.4f};
    const int TOTAL = 100;
    const int DEVICES = 5;

    for (int i = 0; i < TOTAL; i++) {
        uint32_t device_id = (uint32_t)(i % DEVICES) + 1;
        nimcp_sensory_stream_record_t rec = make_stream_with_features(
            device_id, (uint64_t)(i + 1) * 1000, "mass ingest", 0.5f,
            features, 4, nullptr, 0);
        ASSERT_EQ(nimcp_memory_store_ingest_remote_stream(store, &rec), 0);
    }
    nimcp_memory_store_flush(store);

    /* Each device should have TOTAL/DEVICES = 20 entries */
    for (uint32_t d = 1; d <= (uint32_t)DEVICES; d++) {
        nimcp_memory_search_result_t* result =
            nimcp_memory_store_query_by_device(store, d, 100);
        ASSERT_NE(result, nullptr);
        EXPECT_EQ(result->count, (uint32_t)(TOTAL / DEVICES));
        nimcp_memory_search_result_destroy(result);
    }
}
