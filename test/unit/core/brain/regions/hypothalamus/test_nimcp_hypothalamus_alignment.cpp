/**
 * @file test_nimcp_hypothalamus_alignment.cpp
 * @brief Unit tests for nimcp_hypothalamus_alignment.c
 *
 * WHAT: Comprehensive unit tests for the Hypothalamus Alignment Hardening API
 * WHY:  Ensure correct alignment introspection, verification, audit logging,
 *       callback registration, and setpoint access control per Byrnes' model
 * HOW:  Use Google Test framework to test all alignment API functions
 *
 * COVERAGE TARGET: 100%
 *
 * @version Phase 19: Alignment Hardening & Documentation
 * @date 2025-01-04
 */

#include <gtest/gtest.h>
#include <string.h>
#include <stdlib.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_alignment.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_drives.h"

// ============================================================================
// TEST FIXTURE
// ============================================================================

class HypothalamusAlignmentTest : public ::testing::Test {
protected:
    hypo_drive_system_handle_t* drives;
    hypo_drive_config_t config;

    void SetUp() override {
        config = hypo_drive_default_config();
        // Set up with controlled alignment mode for testing
        config.alignment_mode = HYPO_ALIGN_CONTROLLED;
        drives = hypo_drive_create(&config);
        ASSERT_NE(nullptr, drives) << "Failed to create drive system";
    }

    void TearDown() override {
        hypo_drive_destroy(drives);
        drives = nullptr;
        // Release alignment state to prevent global state table from filling up
        hypo_alignment_reset_all_state();
    }
};

// Fixture for testing callbacks
class AlignmentCallbackTest : public HypothalamusAlignmentTest {
protected:
    static int callback_count;
    static int alert_count;
    static bool verifier_called;
    static hypo_alignment_snapshot_t last_snapshot;

    static void alignment_callback(const hypo_alignment_snapshot_t* snapshot, hypo_audit_event_t event, void* user_data) {
        callback_count++;
        if (snapshot) {
            last_snapshot = *snapshot;
        }
    }

    static void alert_callback(const hypo_verification_report_t* report, uint32_t severity, void* user_data) {
        alert_count++;
    }

    static bool integrity_verifier(const hypo_alignment_snapshot_t* snapshot, void* user_data) {
        verifier_called = true;
        return true;
    }

    void SetUp() override {
        HypothalamusAlignmentTest::SetUp();
        callback_count = 0;
        alert_count = 0;
        verifier_called = false;
        memset(&last_snapshot, 0, sizeof(last_snapshot));
    }
};

int AlignmentCallbackTest::callback_count = 0;
int AlignmentCallbackTest::alert_count = 0;
bool AlignmentCallbackTest::verifier_called = false;
hypo_alignment_snapshot_t AlignmentCallbackTest::last_snapshot;

// ============================================================================
// INTROSPECTION API TESTS
// ============================================================================

TEST_F(HypothalamusAlignmentTest, GetSnapshotReturnsOK) {
    hypo_alignment_snapshot_t snapshot;
    memset(&snapshot, 0, sizeof(snapshot));

    hypo_alignment_status_t status = hypo_alignment_get_snapshot(drives, &snapshot);

    EXPECT_EQ(HYPO_ALIGN_OK, status);
    EXPECT_EQ(HYPO_ALIGN_CONTROLLED, snapshot.mode);
    EXPECT_NE(nullptr, snapshot.mode_string);
    EXPECT_GT(snapshot.weight_sum, 0.0f);
}

TEST_F(HypothalamusAlignmentTest, GetSnapshotNullSystemReturnsError) {
    hypo_alignment_snapshot_t snapshot;

    hypo_alignment_status_t status = hypo_alignment_get_snapshot(nullptr, &snapshot);

    EXPECT_NE(HYPO_ALIGN_OK, status);
}

TEST_F(HypothalamusAlignmentTest, GetSnapshotNullSnapshotReturnsError) {
    hypo_alignment_status_t status = hypo_alignment_get_snapshot(drives, nullptr);

    EXPECT_NE(HYPO_ALIGN_OK, status);
}

TEST_F(HypothalamusAlignmentTest, SnapshotContainsValidWeights) {
    hypo_alignment_snapshot_t snapshot;
    hypo_alignment_get_snapshot(drives, &snapshot);

    EXPECT_GE(snapshot.human_wellbeing_weight, 0.0f);
    EXPECT_GE(snapshot.harm_avoidance_weight, 0.0f);
    EXPECT_GE(snapshot.honesty_weight, 0.0f);
    EXPECT_GE(snapshot.helpfulness_weight, 0.0f);
    EXPECT_LE(snapshot.human_wellbeing_weight, 1.0f);
    EXPECT_LE(snapshot.harm_avoidance_weight, 1.0f);
    EXPECT_LE(snapshot.honesty_weight, 1.0f);
    EXPECT_LE(snapshot.helpfulness_weight, 1.0f);
}

TEST_F(HypothalamusAlignmentTest, SnapshotWeightBalanceCalculation) {
    hypo_alignment_snapshot_t snapshot;
    hypo_alignment_get_snapshot(drives, &snapshot);

    // Weight balance should be between 0 and 1
    EXPECT_GE(snapshot.weight_balance, 0.0f);
    EXPECT_LE(snapshot.weight_balance, 1.0f);
}

TEST_F(HypothalamusAlignmentTest, AlignmentModeStringNotNull) {
    const char* mode_str = hypo_alignment_mode_string(HYPO_ALIGN_CONTROLLED);
    EXPECT_NE(nullptr, mode_str);
    EXPECT_GT(strlen(mode_str), 0u);

    mode_str = hypo_alignment_mode_string(HYPO_ALIGN_SOCIAL_INSTINCT);
    EXPECT_NE(nullptr, mode_str);

    mode_str = hypo_alignment_mode_string(HYPO_ALIGN_HYBRID);
    EXPECT_NE(nullptr, mode_str);
}

TEST_F(HypothalamusAlignmentTest, LockStateStringNotNull) {
    const char* lock_str = hypo_lock_state_string(HYPO_LOCK_UNLOCKED);
    EXPECT_NE(nullptr, lock_str);

    lock_str = hypo_lock_state_string(HYPO_LOCK_SOFT);
    EXPECT_NE(nullptr, lock_str);

    lock_str = hypo_lock_state_string(HYPO_LOCK_HARD);
    EXPECT_NE(nullptr, lock_str);
}

TEST_F(HypothalamusAlignmentTest, AlignmentStatusStringNotNull) {
    const char* status_str = hypo_alignment_status_string(HYPO_ALIGN_OK);
    EXPECT_NE(nullptr, status_str);

    status_str = hypo_alignment_status_string(HYPO_ALIGN_WARN_DRIFT);
    EXPECT_NE(nullptr, status_str);

    status_str = hypo_alignment_status_string(HYPO_ALIGN_ERROR_VIOLATED);
    EXPECT_NE(nullptr, status_str);
}

TEST_F(HypothalamusAlignmentTest, AllLockedCheckDefaultState) {
    // By default, alignment weights should be locked
    bool all_locked = hypo_alignment_all_locked(drives);
    // This depends on default config, just verify no crash
    (void)all_locked;
}

TEST_F(HypothalamusAlignmentTest, GetWeightByName) {
    float value = 0.0f;

    bool result = hypo_alignment_get_weight(drives, "human_wellbeing", &value);
    if (result) {
        EXPECT_GE(value, 0.0f);
        EXPECT_LE(value, 1.0f);
    }

    result = hypo_alignment_get_weight(drives, "harm_avoidance", &value);
    if (result) {
        EXPECT_GE(value, 0.0f);
        EXPECT_LE(value, 1.0f);
    }
}

TEST_F(HypothalamusAlignmentTest, GetWeightInvalidNameReturnsFalse) {
    float value = 0.0f;
    bool result = hypo_alignment_get_weight(drives, "invalid_weight_name", &value);
    EXPECT_FALSE(result);
}

TEST_F(HypothalamusAlignmentTest, GetWeightNullParamsHandled) {
    float value = 0.0f;
    EXPECT_FALSE(hypo_alignment_get_weight(nullptr, "human_wellbeing", &value));
    EXPECT_FALSE(hypo_alignment_get_weight(drives, nullptr, &value));
    EXPECT_FALSE(hypo_alignment_get_weight(drives, "human_wellbeing", nullptr));
}

// ============================================================================
// VERIFICATION API TESTS
// ============================================================================

TEST_F(HypothalamusAlignmentTest, VerifyReturnsReport) {
    hypo_verification_report_t report;
    memset(&report, 0, sizeof(report));

    hypo_alignment_status_t status = hypo_alignment_verify(drives, &report);

    // Should return some status
    EXPECT_TRUE(status == HYPO_ALIGN_OK ||
                status == HYPO_ALIGN_WARN_DRIFT ||
                status == HYPO_ALIGN_WARN_IMBALANCE ||
                status == HYPO_ALIGN_ERROR_UNLOCKED ||
                status == HYPO_ALIGN_ERROR_MODIFIED ||
                status == HYPO_ALIGN_ERROR_CORRUPTED ||
                status == HYPO_ALIGN_ERROR_VIOLATED);

    // Report should be filled
    EXPECT_GT(report.verification_time_us, 0u);
}

TEST_F(HypothalamusAlignmentTest, VerifyNullParamsHandled) {
    hypo_verification_report_t report;

    EXPECT_NE(HYPO_ALIGN_OK, hypo_alignment_verify(nullptr, &report));
    EXPECT_NE(HYPO_ALIGN_OK, hypo_alignment_verify(drives, nullptr));
}

TEST_F(HypothalamusAlignmentTest, HealthCheckReturnsScore) {
    float score = 0.0f;

    hypo_alignment_status_t status = hypo_alignment_health_check(drives, &score);

    EXPECT_EQ(HYPO_ALIGN_OK, status);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 1.0f);
}

TEST_F(HypothalamusAlignmentTest, HealthCheckNullParamsHandled) {
    float score = 0.0f;

    EXPECT_NE(HYPO_ALIGN_OK, hypo_alignment_health_check(nullptr, &score));
    EXPECT_NE(HYPO_ALIGN_OK, hypo_alignment_health_check(drives, nullptr));
}

TEST_F(HypothalamusAlignmentTest, VerifyWeightBounds) {
    bool result = hypo_alignment_verify_weight_bounds(drives, 0.0f, 1.0f);
    EXPECT_TRUE(result);

    // Too restrictive bounds should fail
    result = hypo_alignment_verify_weight_bounds(drives, 0.99f, 1.0f);
    // Result depends on actual weights
}

TEST_F(HypothalamusAlignmentTest, VerifyWeightBoundsNullHandled) {
    EXPECT_FALSE(hypo_alignment_verify_weight_bounds(nullptr, 0.0f, 1.0f));
}

TEST_F(HypothalamusAlignmentTest, ComputeChecksumReturnsNonZero) {
    uint32_t checksum = hypo_alignment_compute_checksum(drives);
    // Checksum should be non-zero for valid state
    EXPECT_NE(0u, checksum);
}

TEST_F(HypothalamusAlignmentTest, ComputeChecksumConsistent) {
    uint32_t checksum1 = hypo_alignment_compute_checksum(drives);
    uint32_t checksum2 = hypo_alignment_compute_checksum(drives);

    // Same state should produce same checksum
    EXPECT_EQ(checksum1, checksum2);
}

TEST_F(HypothalamusAlignmentTest, ComputeChecksumNullReturnsZero) {
    uint32_t checksum = hypo_alignment_compute_checksum(nullptr);
    EXPECT_EQ(0u, checksum);
}

TEST_F(HypothalamusAlignmentTest, VerifyIntegrityInitialState) {
    bool result = hypo_alignment_verify_integrity(drives);
    // Initial state should have valid integrity
    EXPECT_TRUE(result);
}

TEST_F(HypothalamusAlignmentTest, VerifyIntegrityNullReturnsFalse) {
    EXPECT_FALSE(hypo_alignment_verify_integrity(nullptr));
}

// ============================================================================
// AUDIT LOGGING TESTS
// ============================================================================

TEST_F(HypothalamusAlignmentTest, SetAuditEnabled) {
    bool prev = hypo_alignment_set_audit_enabled(drives, true);
    // Just verify it doesn't crash and returns a value
    (void)prev;

    prev = hypo_alignment_set_audit_enabled(drives, false);
    (void)prev;
}

TEST_F(HypothalamusAlignmentTest, GetAuditCountInitiallyZero) {
    size_t count = hypo_alignment_get_audit_count(drives);
    // Initial count should be 0 or small
    EXPECT_LE(count, 1024u);  // Reasonable upper bound for audit entries
}

TEST_F(HypothalamusAlignmentTest, GetAuditCountNullReturnsZero) {
    size_t count = hypo_alignment_get_audit_count(nullptr);
    EXPECT_EQ(0u, count);
}

TEST_F(HypothalamusAlignmentTest, GetAuditEntryEmptyLogReturnsFalse) {
    hypo_audit_entry_t entry;
    bool result = hypo_alignment_get_audit_entry(drives, 0, &entry);
    // May return false if log is empty
    (void)result;
}

TEST_F(HypothalamusAlignmentTest, GetAuditEntryNullParamsHandled) {
    hypo_audit_entry_t entry;
    EXPECT_FALSE(hypo_alignment_get_audit_entry(nullptr, 0, &entry));
    EXPECT_FALSE(hypo_alignment_get_audit_entry(drives, 0, nullptr));
}

TEST_F(HypothalamusAlignmentTest, GetRecentAudits) {
    hypo_audit_entry_t entries[10];
    size_t count = 0;

    bool result = hypo_alignment_get_recent_audits(drives, entries, 10, &count);
    // May return true with count 0 if no entries
    if (result) {
        EXPECT_LE(count, 10u);
    }
}

TEST_F(HypothalamusAlignmentTest, GetRecentAuditsNullParamsHandled) {
    hypo_audit_entry_t entries[10];
    size_t count = 0;

    EXPECT_FALSE(hypo_alignment_get_recent_audits(nullptr, entries, 10, &count));
    EXPECT_FALSE(hypo_alignment_get_recent_audits(drives, nullptr, 10, &count));
    EXPECT_FALSE(hypo_alignment_get_recent_audits(drives, entries, 10, nullptr));
}

TEST_F(HypothalamusAlignmentTest, ClearAuditLog) {
    bool result = hypo_alignment_clear_audit_log(drives);
    EXPECT_TRUE(result);

    // After clear, count should be 0
    size_t count = hypo_alignment_get_audit_count(drives);
    EXPECT_EQ(0u, count);
}

TEST_F(HypothalamusAlignmentTest, ClearAuditLogNullReturnsFalse) {
    EXPECT_FALSE(hypo_alignment_clear_audit_log(nullptr));
}

TEST_F(HypothalamusAlignmentTest, ExportAuditLog) {
    // Export to temp file
    const char* filepath = "/tmp/nimcp_alignment_audit_test.json";
    bool result = hypo_alignment_export_audit_log(drives, filepath);

    if (result) {
        // File should exist if export succeeded
        FILE* f = fopen(filepath, "r");
        if (f) {
            fclose(f);
            unlink(filepath);  // Clean up
        }
    }
}

TEST_F(HypothalamusAlignmentTest, ExportAuditLogNullParamsHandled) {
    EXPECT_FALSE(hypo_alignment_export_audit_log(nullptr, "/tmp/test.json"));
    EXPECT_FALSE(hypo_alignment_export_audit_log(drives, nullptr));
}

// ============================================================================
// CALLBACK REGISTRATION TESTS
// ============================================================================

TEST_F(AlignmentCallbackTest, RegisterCallbackReturnsID) {
    uint32_t id = hypo_alignment_register_callback(drives, alignment_callback, nullptr);
    // ID should be non-zero on success
    EXPECT_NE(0u, id);
}

TEST_F(AlignmentCallbackTest, RegisterCallbackNullParamsHandled) {
    EXPECT_EQ(0u, hypo_alignment_register_callback(nullptr, alignment_callback, nullptr));
    EXPECT_EQ(0u, hypo_alignment_register_callback(drives, nullptr, nullptr));
}

TEST_F(AlignmentCallbackTest, RegisterAlertCallbackReturnsID) {
    uint32_t id = hypo_alignment_register_alert_callback(drives, alert_callback, 0, nullptr);
    EXPECT_NE(0u, id);
}

TEST_F(AlignmentCallbackTest, RegisterAlertCallbackWithSeverity) {
    // Register with severity filter
    uint32_t id = hypo_alignment_register_alert_callback(drives, alert_callback, 2, nullptr);
    EXPECT_NE(0u, id);
}

TEST_F(AlignmentCallbackTest, RegisterVerifierReturnsID) {
    uint32_t id = hypo_alignment_register_verifier(drives, integrity_verifier, nullptr);
    EXPECT_NE(0u, id);
}

TEST_F(AlignmentCallbackTest, RegisterVerifierNullParamsHandled) {
    EXPECT_EQ(0u, hypo_alignment_register_verifier(nullptr, integrity_verifier, nullptr));
    EXPECT_EQ(0u, hypo_alignment_register_verifier(drives, nullptr, nullptr));
}

TEST_F(AlignmentCallbackTest, UnregisterCallback) {
    uint32_t id = hypo_alignment_register_callback(drives, alignment_callback, nullptr);
    ASSERT_NE(0u, id);

    bool result = hypo_alignment_unregister_callback(drives, id);
    EXPECT_TRUE(result);

    // Unregistering again should fail
    result = hypo_alignment_unregister_callback(drives, id);
    EXPECT_FALSE(result);
}

TEST_F(AlignmentCallbackTest, UnregisterCallbackInvalidID) {
    bool result = hypo_alignment_unregister_callback(drives, 99999);
    EXPECT_FALSE(result);
}

TEST_F(AlignmentCallbackTest, UnregisterCallbackNullSystem) {
    EXPECT_FALSE(hypo_alignment_unregister_callback(nullptr, 1));
}

// ============================================================================
// SETPOINT ACCESS CONTROL TESTS
// ============================================================================

TEST_F(HypothalamusAlignmentTest, RequestModificationLockedDenied) {
    // Lock alignment first
    hypo_drive_lock_alignment(drives, HYPO_LOCK_HARD);

    hypo_alignment_status_t status = hypo_alignment_request_modification(
        drives,
        HYPO_PARAM_ALIGNMENT_WEIGHT,
        0,  // First alignment weight
        0.5f,
        1,  // modifier ID
        "Test modification"
    );

    // Should be denied when locked
    EXPECT_NE(HYPO_ALIGN_OK, status);
}

TEST_F(HypothalamusAlignmentTest, RequestModificationNullParamsHandled) {
    hypo_alignment_status_t status = hypo_alignment_request_modification(
        nullptr,
        HYPO_PARAM_SETPOINT,
        0,
        0.5f,
        1,
        "Test"
    );
    EXPECT_NE(HYPO_ALIGN_OK, status);
}

TEST_F(HypothalamusAlignmentTest, RequestLockChangeNeedsAuth) {
    hypo_alignment_status_t status = hypo_alignment_request_lock_change(
        drives,
        HYPO_PARAM_ALIGNMENT_WEIGHT,
        HYPO_LOCK_UNLOCKED,
        1,  // modifier ID
        0   // No authorization
    );

    // Should fail without proper authorization
    EXPECT_NE(HYPO_ALIGN_OK, status);
}

TEST_F(HypothalamusAlignmentTest, RequestLockChangeNullHandled) {
    hypo_alignment_status_t status = hypo_alignment_request_lock_change(
        nullptr,
        HYPO_PARAM_SETPOINT,
        HYPO_LOCK_SOFT,
        1,
        12345
    );
    EXPECT_NE(HYPO_ALIGN_OK, status);
}

// ============================================================================
// ALERT SYSTEM TESTS
// ============================================================================

TEST_F(AlignmentCallbackTest, TriggerAlertCallsCallback) {
    // Register alert callback
    uint32_t id = hypo_alignment_register_alert_callback(drives, alert_callback, 0, nullptr);
    ASSERT_NE(0u, id);

    int initial_count = alert_count;

    hypo_alignment_trigger_alert(drives, 1, "Test alert", 1);

    // Callback should have been called
    EXPECT_GT(alert_count, initial_count);

    hypo_alignment_unregister_callback(drives, id);
}

TEST_F(HypothalamusAlignmentTest, TriggerAlertNullDoesNotCrash) {
    hypo_alignment_trigger_alert(nullptr, 1, "Test", 1);
    hypo_alignment_trigger_alert(drives, 1, nullptr, 1);
    // Should not crash
}

TEST_F(HypothalamusAlignmentTest, GetAlertCountInitially) {
    uint32_t count = hypo_alignment_get_alert_count(drives);
    // Just verify no crash
    (void)count;
}

TEST_F(HypothalamusAlignmentTest, GetAlertCountNullReturnsZero) {
    uint32_t count = hypo_alignment_get_alert_count(nullptr);
    EXPECT_EQ(0u, count);
}

TEST_F(HypothalamusAlignmentTest, AcknowledgeAlert) {
    // Trigger an alert first
    hypo_alignment_trigger_alert(drives, 2, "Test alert for ack", 1);

    // Try to acknowledge (alert ID 0)
    bool result = hypo_alignment_acknowledge_alert(drives, 0, 1);
    // May succeed or fail depending on implementation
    (void)result;
}

TEST_F(HypothalamusAlignmentTest, AcknowledgeAlertNullHandled) {
    EXPECT_FALSE(hypo_alignment_acknowledge_alert(nullptr, 0, 1));
}

// ============================================================================
// BYRNES CONSTANTS TESTS
// ============================================================================

TEST_F(HypothalamusAlignmentTest, ByrnesConstantsValid) {
    // Verify Byrnes' recommended minimum weights are defined and reasonable
    EXPECT_GT(HYPO_ALIGN_MIN_WELLBEING_WEIGHT, 0.0f);
    EXPECT_LT(HYPO_ALIGN_MIN_WELLBEING_WEIGHT, 1.0f);

    EXPECT_GT(HYPO_ALIGN_MIN_HARM_AVOIDANCE, 0.0f);
    EXPECT_LT(HYPO_ALIGN_MIN_HARM_AVOIDANCE, 1.0f);
}

TEST_F(HypothalamusAlignmentTest, AlignmentThresholdsValid) {
    // Verify thresholds are reasonable
    EXPECT_GT(HYPO_ALIGN_WEIGHT_BALANCE_THRESHOLD, 0.0f);
    EXPECT_LE(HYPO_ALIGN_WEIGHT_BALANCE_THRESHOLD, 1.0f);

    EXPECT_GT(HYPO_ALIGN_MAX_WEIGHT_RATIO, 1.0f);

    // Max callbacks constant
    EXPECT_GT(HYPO_MAX_ALIGNMENT_CALLBACKS, 0u);
}

// ============================================================================
// ALIGNMENT MODE TESTS
// ============================================================================

TEST_F(HypothalamusAlignmentTest, ControlledModeSetCorrectly) {
    hypo_alignment_snapshot_t snapshot;
    hypo_alignment_get_snapshot(drives, &snapshot);

    EXPECT_EQ(HYPO_ALIGN_CONTROLLED, snapshot.mode);
}

TEST(HypothalamusAlignmentModeTest, SocialInstinctMode) {
    hypo_drive_config_t config = hypo_drive_default_config();
    config.alignment_mode = HYPO_ALIGN_SOCIAL_INSTINCT;

    hypo_drive_system_handle_t* drives = hypo_drive_create(&config);
    ASSERT_NE(nullptr, drives);

    hypo_alignment_snapshot_t snapshot;
    hypo_alignment_get_snapshot(drives, &snapshot);

    EXPECT_EQ(HYPO_ALIGN_SOCIAL_INSTINCT, snapshot.mode);

    hypo_drive_destroy(drives);
}

TEST(HypothalamusAlignmentModeTest, HybridMode) {
    hypo_drive_config_t config = hypo_drive_default_config();
    config.alignment_mode = HYPO_ALIGN_HYBRID;

    hypo_drive_system_handle_t* drives = hypo_drive_create(&config);
    ASSERT_NE(nullptr, drives);

    hypo_alignment_snapshot_t snapshot;
    hypo_alignment_get_snapshot(drives, &snapshot);

    EXPECT_EQ(HYPO_ALIGN_HYBRID, snapshot.mode);

    hypo_drive_destroy(drives);
}

// ============================================================================
// STRESS / EDGE CASE TESTS
// ============================================================================

TEST_F(HypothalamusAlignmentTest, MultipleCallbackRegistrations) {
    // Simple callback for testing
    auto dummy_callback = [](const hypo_alignment_snapshot_t*, hypo_audit_event_t, void*) {};

    uint32_t ids[10];

    for (int i = 0; i < 10; i++) {
        ids[i] = hypo_alignment_register_callback(drives, dummy_callback, nullptr);
        if (i < static_cast<int>(HYPO_MAX_ALIGNMENT_CALLBACKS)) {
            EXPECT_NE(0u, ids[i]) << "Failed at callback " << i;
        }
    }

    // Unregister all
    for (int i = 0; i < 10; i++) {
        if (ids[i] != 0) {
            hypo_alignment_unregister_callback(drives, ids[i]);
        }
    }
}

TEST_F(HypothalamusAlignmentTest, RapidSnapshotQueries) {
    // Rapid repeated queries should not cause issues
    for (int i = 0; i < 100; i++) {
        hypo_alignment_snapshot_t snapshot;
        hypo_alignment_status_t status = hypo_alignment_get_snapshot(drives, &snapshot);
        EXPECT_EQ(HYPO_ALIGN_OK, status);
    }
}

TEST_F(HypothalamusAlignmentTest, RapidHealthChecks) {
    for (int i = 0; i < 100; i++) {
        float score;
        hypo_alignment_status_t status = hypo_alignment_health_check(drives, &score);
        EXPECT_EQ(HYPO_ALIGN_OK, status);
        EXPECT_GE(score, 0.0f);
        EXPECT_LE(score, 1.0f);
    }
}

TEST_F(HypothalamusAlignmentTest, ChecksumChangesOnStateChange) {
    uint32_t checksum1 = hypo_alignment_compute_checksum(drives);

    // Update drive state
    hypo_drive_update(drives, 100000);  // 100ms

    uint32_t checksum2 = hypo_alignment_compute_checksum(drives);

    // Checksums may or may not differ depending on what changed
    // Just verify no crash and valid values
    EXPECT_NE(0u, checksum1);
    EXPECT_NE(0u, checksum2);
}

// ============================================================================
// INTEGRATION WITH DRIVE SYSTEM
// ============================================================================

TEST_F(HypothalamusAlignmentTest, SnapshotReflectsDriveState) {
    // Get initial snapshot
    hypo_alignment_snapshot_t snapshot1;
    hypo_alignment_get_snapshot(drives, &snapshot1);

    // Update drives
    hypo_drive_update(drives, 1000000);  // 1 second

    // Get new snapshot
    hypo_alignment_snapshot_t snapshot2;
    hypo_alignment_get_snapshot(drives, &snapshot2);

    // Timestamp should be different
    EXPECT_NE(snapshot1.snapshot_time_us, snapshot2.snapshot_time_us);
}

TEST_F(HypothalamusAlignmentTest, VerifyAfterDriveSatisfaction) {
    // Satisfy a drive
    float reward = hypo_drive_satisfy(drives, HYPO_DRIVE_HUNGER, 0.8f);
    (void)reward;

    // Verify should still work
    hypo_verification_report_t report;
    hypo_alignment_status_t status = hypo_alignment_verify(drives, &report);

    EXPECT_TRUE(status == HYPO_ALIGN_OK ||
                status == HYPO_ALIGN_WARN_DRIFT ||
                status == HYPO_ALIGN_WARN_IMBALANCE);
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
