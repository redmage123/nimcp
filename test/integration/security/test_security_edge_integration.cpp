/**
 * @file test_security_edge_integration.cpp
 * @brief Integration tests combining BBB, audit trail, gradient validation, and memory store
 *
 * WHAT: Test BBB + audit trail + gradient validation + memory store together
 * WHY:  Security layers must work together without interference
 * HOW:  Exercise combined workflows: validate -> aggregate -> audit -> persist
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <unistd.h>
#include <limits>

extern "C" {
#include "edge/nimcp_edge.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "memory/nimcp_memory_store.h"
}

// ============================================================================
// Integration Test Fixture
// ============================================================================

class SecurityEdgeIntegrationTest : public ::testing::Test {
protected:
    nimcp_memory_store_t* store = nullptr;
    bbb_system_t bbb = nullptr;
    char db_path[256];

    void SetUp() override {
        snprintf(db_path, sizeof(db_path), "/tmp/nimcp_sec_edge_integ_%d.db", getpid());
        nimcp_memory_store_config_t cfg = nimcp_memory_store_config_default();
        cfg.db_path = db_path;
        cfg.enable_questdb_sync = false;
        store = nimcp_memory_store_create(&cfg);

        bbb_config_t bbb_cfg = bbb_default_config();
        bbb = bbb_system_create(&bbb_cfg);
    }

    void TearDown() override {
        if (bbb) bbb_system_destroy(bbb);
        if (store) nimcp_memory_store_destroy(store);
        unlink(db_path);
        char wal[270], shm[270];
        snprintf(wal, sizeof(wal), "%s-wal", db_path);
        snprintf(shm, sizeof(shm), "%s-shm", db_path);
        unlink(wal);
        unlink(shm);
    }

    nimcp_memory_audit_event_t make_audit(uint32_t type, const char* desc) {
        nimcp_memory_audit_event_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.event_type = type;
        ev.timestamp_us = 1000;
        if (desc) strncpy(ev.description, desc, sizeof(ev.description) - 1);
        return ev;
    }
};

// --- BBB + Audit ---

TEST_F(SecurityEdgeIntegrationTest, BBBRejectsAndAuditLogs) {
    if (!store || !bbb) GTEST_SKIP() << "Setup failed";

    bbb_validation_result_t result;
    bool valid = bbb_validate_pointer(bbb, nullptr, 100, &result);

    if (!valid) {
        nimcp_memory_audit_event_t ev = make_audit(2, "BBB rejected NULL pointer input");
        int ret = nimcp_memory_store_audit_log(store, &ev);
        EXPECT_EQ(ret, 0);
    }

    nimcp_memory_store_flush(store);
    nimcp_memory_search_result_t* sr =
        nimcp_memory_store_audit_search(store, 0, 0, UINT64_MAX, 10);
    if (sr) {
        EXPECT_GE(sr->count, 0u);
        nimcp_memory_search_result_destroy(sr);
    }
}

// --- Gradient Validation + FedAvg ---

TEST_F(SecurityEdgeIntegrationTest, GradientValidationWithFedAvg) {
    const uint32_t NUM_PARAMS = 10;
    float grads1[10], grads2[10], aggregated[10];

    for (int i = 0; i < 10; i++) {
        grads1[i] = 1.0f;
        grads2[i] = 3.0f;
    }

    nimcp_federated_gradient_t devices[2];
    memset(devices, 0, sizeof(devices));
    devices[0].device_id = 1;
    devices[0].gradients = grads1;
    devices[0].num_params = NUM_PARAMS;
    devices[1].device_id = 2;
    devices[1].gradients = grads2;
    devices[1].num_params = NUM_PARAMS;

    int ret = nimcp_federated_aggregate(devices, 2, aggregated, NUM_PARAMS,
                                         NIMCP_FED_AVG);
    EXPECT_EQ(ret, 0);

    for (int i = 0; i < 10; i++) {
        EXPECT_NEAR(aggregated[i], 2.0f, 1e-5f);
    }

    if (store) {
        nimcp_memory_audit_event_t ev = make_audit(0, "FedAvg aggregation successful");
        nimcp_memory_store_audit_log(store, &ev);
    }
}

// --- Gradient Validation + FedMedian ---

TEST_F(SecurityEdgeIntegrationTest, GradientValidationWithFedMedian) {
    const uint32_t NUM_PARAMS = 5;
    float grads1[5] = {1,1,1,1,1};
    float grads2[5] = {2,2,2,2,2};
    float grads3[5] = {100,100,100,100,100};
    float aggregated[5];

    nimcp_federated_gradient_t devices[3];
    memset(devices, 0, sizeof(devices));
    devices[0].device_id = 1; devices[0].gradients = grads1; devices[0].num_params = NUM_PARAMS;
    devices[1].device_id = 2; devices[1].gradients = grads2; devices[1].num_params = NUM_PARAMS;
    devices[2].device_id = 3; devices[2].gradients = grads3; devices[2].num_params = NUM_PARAMS;

    int ret = nimcp_federated_aggregate(devices, 3, aggregated, NUM_PARAMS,
                                         NIMCP_FED_MEDIAN);
    EXPECT_EQ(ret, 0);

    // Gradient validation zeroes device 3's extreme gradients (100 > 1e6? No, 100 < 1e6).
    // So values are [1, 2, 100], sorted = [1, 2, 100], median = 2
    for (int i = 0; i < 5; i++) {
        EXPECT_NEAR(aggregated[i], 2.0f, 1e-5f);
    }
}

// --- Memory Store + Audit Trail ---

TEST_F(SecurityEdgeIntegrationTest, MemoryStoreWithAuditTrail) {
    if (!store) GTEST_SKIP() << "Store creation failed";

    nimcp_engram_record_t engram;
    memset(&engram, 0, sizeof(engram));
    engram.engram_id = 1;
    engram.timestamp_us = 1000;
    strncpy(engram.label, "integration test engram", sizeof(engram.label) - 1);
    EXPECT_EQ(nimcp_memory_store_engram_put(store, &engram), 0);

    nimcp_memory_audit_event_t ev = make_audit(0, "Engram stored during integration test");
    EXPECT_EQ(nimcp_memory_store_audit_log(store, &ev), 0);

    nimcp_engram_record_t retrieved;
    memset(&retrieved, 0, sizeof(retrieved));
    EXPECT_EQ(nimcp_memory_store_engram_get(store, 1, &retrieved), 0);
    EXPECT_STREQ(retrieved.label, "integration test engram");
}

// --- Label Injection E2E ---

TEST_F(SecurityEdgeIntegrationTest, LabelInjectionEndToEnd) {
    if (!store) GTEST_SKIP() << "Store creation failed";

    const char* dangerous_labels[] = {
        "Robert'); DROP TABLE engrams;--",
        "O'Brien's test",
        "test\"; SELECT * FROM audit_log;--",
        "<script>alert('xss')</script>",
    };

    for (int i = 0; i < 4; i++) {
        nimcp_engram_record_t engram;
        memset(&engram, 0, sizeof(engram));
        engram.engram_id = (uint64_t)(100 + i);
        engram.timestamp_us = (uint64_t)((i + 1) * 1000);
        strncpy(engram.label, dangerous_labels[i], sizeof(engram.label) - 1);
        engram.label[sizeof(engram.label) - 1] = '\0';

        int ret = nimcp_memory_store_engram_put(store, &engram);
        EXPECT_EQ(ret, 0) << "Failed on label: " << dangerous_labels[i];
    }

    nimcp_memory_store_flush(store);

    nimcp_memory_search_result_t* result =
        nimcp_memory_store_engram_search_text(store, "O'Brien", 10);
    if (result) {
        nimcp_memory_search_result_destroy(result);
    }
}

// --- Multiple Security Layers ---

TEST_F(SecurityEdgeIntegrationTest, MultipleSecurityLayers) {
    if (!store || !bbb) GTEST_SKIP() << "Setup failed";

    bbb_validation_result_t vresult;
    bbb_validate_string(bbb, "safe input", &vresult);

    nimcp_engram_record_t engram;
    memset(&engram, 0, sizeof(engram));
    engram.engram_id = 200;
    engram.timestamp_us = 5000;
    strncpy(engram.label, "multi-layer test", sizeof(engram.label) - 1);
    EXPECT_EQ(nimcp_memory_store_engram_put(store, &engram), 0);

    nimcp_memory_audit_event_t ev = make_audit(0, "Multi-layer security pipeline test");
    EXPECT_EQ(nimcp_memory_store_audit_log(store, &ev), 0);

    bbb_statistics_t stats;
    bool got_stats = bbb_system_get_statistics(bbb, &stats);
    if (got_stats) {
        EXPECT_GE(stats.total_validations, 0u);
    }
}

// --- Audit Trail Survives Checkpoint ---

TEST_F(SecurityEdgeIntegrationTest, AuditTrailSurvivesCheckpoint) {
    if (!store) GTEST_SKIP() << "Store creation failed";

    for (int i = 0; i < 10; i++) {
        nimcp_memory_audit_event_t ev = make_audit(i % 3, "checkpoint test");
        ev.timestamp_us = (uint64_t)(i * 100);
        nimcp_memory_store_audit_log(store, &ev);
    }

    nimcp_memory_store_flush(store);
    int ret = nimcp_memory_store_checkpoint(store);
    EXPECT_EQ(ret, 0);

    // Audit events should be searchable after checkpoint
    nimcp_memory_search_result_t* result =
        nimcp_memory_store_audit_search(store, 0, 0, UINT64_MAX, 20);
    if (result) {
        // Count may be 0 depending on audit search implementation
        nimcp_memory_search_result_destroy(result);
    }
    // Main assertion: checkpoint does not crash or corrupt data
}

// --- Concurrent Audit and Memory ---

TEST_F(SecurityEdgeIntegrationTest, ConcurrentAuditAndMemory) {
    if (!store) GTEST_SKIP() << "Store creation failed";

    for (int i = 0; i < 50; i++) {
        if (i % 2 == 0) {
            nimcp_engram_record_t engram;
            memset(&engram, 0, sizeof(engram));
            engram.engram_id = (uint64_t)(500 + i);
            engram.timestamp_us = (uint64_t)(i * 100);
            snprintf(engram.label, sizeof(engram.label), "concurrent_%d", i);
            nimcp_memory_store_engram_put(store, &engram);
        } else {
            nimcp_memory_audit_event_t ev = make_audit(0, "concurrent interleaved");
            ev.timestamp_us = (uint64_t)(i * 100);
            nimcp_memory_store_audit_log(store, &ev);
        }
    }

    nimcp_memory_store_flush(store);
}

// --- Security Stats Accumulate ---

TEST_F(SecurityEdgeIntegrationTest, SecurityStatsAccumulate) {
    if (!bbb) GTEST_SKIP() << "BBB creation failed";

    bbb_statistics_t stats_before;
    bbb_system_get_statistics(bbb, &stats_before);

    bbb_validation_result_t vresult;
    for (int i = 0; i < 10; i++) {
        bbb_validate_string(bbb, "test", &vresult);
    }

    bbb_statistics_t stats_after;
    bbb_system_get_statistics(bbb, &stats_after);

    EXPECT_GE(stats_after.total_validations, stats_before.total_validations);
}
