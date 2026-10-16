/**
 * @file test_security_edge_regression.cpp
 * @brief Regression tests for security edge cases and boundary conditions
 *
 * WHAT: Test NULL pointers, empty strings, max-length strings, overflow, NaN
 * WHY:  Prevent regressions in security input validation and error handling
 * HOW:  Exercise boundary conditions across all security-related APIs
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <cfloat>
#include <unistd.h>
#include <limits>

extern "C" {
#include "edge/nimcp_edge.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "memory/nimcp_memory_store.h"
}

// ============================================================================
// Regression Test Fixture
// ============================================================================

class SecurityEdgeRegressionTest : public ::testing::Test {
protected:
    nimcp_memory_store_t* store = nullptr;
    bbb_system_t bbb = nullptr;
    char db_path[256];

    void SetUp() override {
        snprintf(db_path, sizeof(db_path), "/tmp/nimcp_sec_regress_%d.db", getpid());
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
};

// --- NULL Pointer Safety ---

TEST_F(SecurityEdgeRegressionTest, NullPointerSafety) {
    // Edge APIs with NULL brain
    EXPECT_EQ(nimcp_edge_brain_resize(nullptr, nullptr), -1);

    nimcp_resize_config_t rcfg = nimcp_resize_config_default();
    EXPECT_EQ(nimcp_edge_brain_resize(nullptr, &rcfg), -1);

    nimcp_resize_report_t rreport;
    EXPECT_EQ(nimcp_edge_brain_resize_check(nullptr, nullptr, &rreport), -1);

    EXPECT_EQ(nimcp_edge_score_neuron_importance(nullptr, nullptr, 0), -1);

    EXPECT_EQ(nimcp_brain_distill(nullptr, nullptr, nullptr, nullptr), -1);
    EXPECT_EQ(nimcp_brain_quantize(nullptr, nullptr), -1);

    // Federated with NULL
    EXPECT_EQ(nimcp_federated_aggregate(nullptr, 0, nullptr, 0, NIMCP_FED_AVG), -1);
    EXPECT_EQ(nimcp_federated_blend(nullptr, nullptr, 0, 0.5f, nullptr), -1);

    // Memory store with NULL
    EXPECT_EQ(nimcp_memory_store_engram_put(nullptr, nullptr), -1);
    EXPECT_EQ(nimcp_memory_store_audit_log(nullptr, nullptr), -1);
    EXPECT_EQ(nimcp_memory_store_flush(nullptr), -1);
    EXPECT_EQ(nimcp_memory_store_checkpoint(nullptr), -1);

    // BBB with NULL
    bbb_system_destroy(nullptr);  // Should not crash

    bbb_validation_result_t vresult;
    bbb_validate_string(nullptr, "test", &vresult);  // Should not crash
    bbb_validate_pointer(nullptr, nullptr, 0, &vresult);  // Should not crash

    bbb_statistics_t stats;
    bbb_system_get_statistics(nullptr, &stats);  // Should not crash
}

// --- Empty String Labels ---

TEST_F(SecurityEdgeRegressionTest, EmptyStringLabels) {
    if (!store) GTEST_SKIP() << "Store creation failed";

    nimcp_engram_record_t engram;
    memset(&engram, 0, sizeof(engram));
    engram.engram_id = 1;
    engram.timestamp_us = 1000;
    engram.label[0] = '\0';  // Empty label

    int ret = nimcp_memory_store_engram_put(store, &engram);
    EXPECT_EQ(ret, 0);

    nimcp_autobio_record_t autobio;
    memset(&autobio, 0, sizeof(autobio));
    autobio.memory_id = 1;
    autobio.timestamp_us = 1000;
    autobio.what_happened[0] = '\0';

    ret = nimcp_memory_store_autobio_put(store, &autobio);
    EXPECT_EQ(ret, 0);
}

// --- Max Length Strings ---

TEST_F(SecurityEdgeRegressionTest, MaxLengthStrings) {
    if (!store) GTEST_SKIP() << "Store creation failed";

    nimcp_engram_record_t engram;
    memset(&engram, 0, sizeof(engram));
    engram.engram_id = 2;
    engram.timestamp_us = 2000;
    memset(engram.label, 'X', 255);
    engram.label[255] = '\0';

    int ret = nimcp_memory_store_engram_put(store, &engram);
    EXPECT_EQ(ret, 0);

    nimcp_metadata_record_t meta;
    memset(&meta, 0, sizeof(meta));
    meta.entry_id = 2;
    meta.type = NIMCP_MEMORY_TYPE_ENGRAM;
    meta.timestamp_us = 2000;
    memset(meta.tags, 'Y', 511);
    meta.tags[511] = '\0';
    memset(meta.label, 'Z', 255);
    meta.label[255] = '\0';

    ret = nimcp_memory_store_metadata_put(store, &meta);
    EXPECT_EQ(ret, 0);
}

// --- Gradient Edge Cases ---

TEST_F(SecurityEdgeRegressionTest, GradientOverflow) {
    const uint32_t N = 5;
    float grads[5] = {FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX};
    float aggregated[5];

    nimcp_federated_gradient_t device;
    memset(&device, 0, sizeof(device));
    device.device_id = 1;
    device.gradients = grads;
    device.num_params = N;

    int ret = nimcp_federated_aggregate(&device, 1, aggregated, N, NIMCP_FED_AVG);
    EXPECT_EQ(ret, 0);

    // FLT_MAX > 1e6 threshold, so gradient validation zeroes out the device
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(aggregated[i], 0.0f);
    }
}

TEST_F(SecurityEdgeRegressionTest, NaNInGradients) {
    const uint32_t N = 5;
    float grads[5];
    for (int i = 0; i < 5; i++) grads[i] = std::numeric_limits<float>::quiet_NaN();
    float aggregated[5];

    nimcp_federated_gradient_t device;
    memset(&device, 0, sizeof(device));
    device.device_id = 1;
    device.gradients = grads;
    device.num_params = N;

    int ret = nimcp_federated_aggregate(&device, 1, aggregated, N, NIMCP_FED_AVG);
    EXPECT_EQ(ret, 0);
    // No crash
}

// --- Audit Trail Edge Cases ---

TEST_F(SecurityEdgeRegressionTest, AuditLogOverflow) {
    if (!store) GTEST_SKIP() << "Store creation failed";

    nimcp_memory_audit_event_t ev;
    memset(&ev, 0, sizeof(ev));
    ev.event_type = 0;
    ev.timestamp_us = 1000;

    // 255-char description
    memset(ev.description, 'D', 255);
    ev.description[255] = '\0';

    // 511-char details
    memset(ev.details, 'T', 511);
    ev.details[511] = '\0';

    int ret = nimcp_memory_store_audit_log(store, &ev);
    EXPECT_EQ(ret, 0);
}

TEST_F(SecurityEdgeRegressionTest, ConcurrentGradientValidation) {
    const uint32_t N = 100;
    const uint32_t DEVICES = 10;

    std::vector<std::vector<float>> grad_arrays(DEVICES, std::vector<float>(N));
    for (uint32_t d = 0; d < DEVICES; d++) {
        for (uint32_t i = 0; i < N; i++) {
            grad_arrays[d][i] = (float)(d + i) * 0.001f;
        }
    }

    std::vector<nimcp_federated_gradient_t> devices(DEVICES);
    for (uint32_t d = 0; d < DEVICES; d++) {
        memset(&devices[d], 0, sizeof(nimcp_federated_gradient_t));
        devices[d].device_id = d + 1;
        devices[d].gradients = grad_arrays[d].data();
        devices[d].num_params = N;
    }

    std::vector<float> aggregated(N);
    int ret = nimcp_federated_aggregate(devices.data(), DEVICES, aggregated.data(),
                                         N, NIMCP_FED_AVG);
    EXPECT_EQ(ret, 0);
}

// --- BBB with disabled immune ---

TEST_F(SecurityEdgeRegressionTest, BBBWithDisabledImmune) {
    if (!bbb) GTEST_SKIP() << "BBB creation failed";

    // Connect with NULL immune system - should handle gracefully
    bool ret = bbb_connect_immune(bbb, nullptr);
    // May return false (rejected) or true (accepted NULL) - either is fine
    (void)ret;

    // BBB should still work
    bbb_validation_result_t vresult;
    bbb_validate_string(bbb, "test after null immune", &vresult);
}

// --- Double destroy safety ---

TEST_F(SecurityEdgeRegressionTest, DoubleDestroyBBB) {
    bbb_system_t local_bbb = bbb_system_create(nullptr);
    if (local_bbb) {
        bbb_system_destroy(local_bbb);
    }
    // Second destroy of NULL should be safe
    bbb_system_destroy(nullptr);
}

TEST_F(SecurityEdgeRegressionTest, DoubleDestroyMemoryStore) {
    if (!store) GTEST_SKIP() << "Store creation failed";

    nimcp_memory_store_destroy(store);
    store = nullptr;

    nimcp_memory_store_destroy(nullptr);
    // No crash = pass
}

// --- Memory Store After Operations ---

TEST_F(SecurityEdgeRegressionTest, StoreOperationsAfterFlush) {
    if (!store) GTEST_SKIP() << "Store creation failed";

    nimcp_engram_record_t engram;
    memset(&engram, 0, sizeof(engram));
    engram.engram_id = 99;
    engram.timestamp_us = 99000;
    strncpy(engram.label, "pre-flush", sizeof(engram.label) - 1);
    EXPECT_EQ(nimcp_memory_store_engram_put(store, &engram), 0);

    nimcp_memory_store_flush(store);

    // Operations after flush should still work
    engram.engram_id = 100;
    strncpy(engram.label, "post-flush", sizeof(engram.label) - 1);
    EXPECT_EQ(nimcp_memory_store_engram_put(store, &engram), 0);

    nimcp_engram_record_t retrieved;
    memset(&retrieved, 0, sizeof(retrieved));
    EXPECT_EQ(nimcp_memory_store_engram_get(store, 100, &retrieved), 0);
}
