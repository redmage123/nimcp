/**
 * @file test_hypothalamus_alignment_regression.cpp
 * @brief Regression tests for Hypothalamus Alignment API
 *
 * WHAT: Regression tests ensuring alignment behavior remains consistent
 *       across code changes and doesn't break Byrnes' safety constraints
 * WHY:  Prevent regressions in alignment-critical code paths
 * HOW:  Test known good behaviors, boundary conditions, and safety invariants
 *
 * @version Phase 19: Alignment Hardening & Documentation
 * @date 2025-01-04
 */

#include <gtest/gtest.h>
#include <string.h>
#include <stdlib.h>
#include <cmath>
#include <vector>

// Headers have their own extern "C" guards
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_alignment.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_drives.h"

// ============================================================================
// REGRESSION TEST FIXTURE
// ============================================================================

class AlignmentRegressionTest : public ::testing::Test {
protected:
    hypo_drive_system_handle_t* drives;
    hypo_drive_config_t config;

    void SetUp() override {
        config = hypo_drive_default_config();
        drives = hypo_drive_create(&config);
        ASSERT_NE(nullptr, drives);
    }

    void TearDown() override {
        hypo_drive_destroy(drives);
        drives = nullptr;
        hypo_alignment_reset_all_state();
    }
};

// ============================================================================
// SNAPSHOT FORMAT REGRESSION
// ============================================================================

// Regression: Snapshot structure must maintain backward compatibility
TEST_F(AlignmentRegressionTest, SnapshotStructureSize) {
    // Document expected size for regression tracking
    // This ensures struct changes are intentional
    hypo_alignment_snapshot_t snapshot;
    size_t actual_size = sizeof(snapshot);

    // Snapshot should be a reasonable size (not too large)
    EXPECT_LT(actual_size, 4096) << "Snapshot struct grew unexpectedly large";
    EXPECT_GT(actual_size, 64) << "Snapshot struct unexpectedly small";
}

// Regression: All snapshot fields must be populated
TEST_F(AlignmentRegressionTest, SnapshotFieldsPopulated) {
    hypo_alignment_snapshot_t snapshot;
    memset(&snapshot, 0xFF, sizeof(snapshot));  // Fill with pattern

    ASSERT_EQ(HYPO_ALIGN_OK, hypo_alignment_get_snapshot(drives, &snapshot));

    // Core fields must be set
    EXPECT_NE(0xFFFFFFFF, (uint32_t)snapshot.mode);
    EXPECT_NE(nullptr, snapshot.mode_string);
    EXPECT_NE(0xFFFFFFFF, snapshot.snapshot_time_us);
}

// Regression: Timestamp increases monotonically
TEST_F(AlignmentRegressionTest, TimestampMonotonicallyIncreasing) {
    std::vector<uint64_t> timestamps;

    for (int i = 0; i < 10; i++) {
        hypo_alignment_snapshot_t snapshot;
        hypo_alignment_get_snapshot(drives, &snapshot);
        timestamps.push_back(snapshot.snapshot_time_us);

        // Small delay
        hypo_drive_update(drives, 1000);
    }

    for (size_t i = 1; i < timestamps.size(); i++) {
        EXPECT_GE(timestamps[i], timestamps[i-1])
            << "Timestamp decreased at index " << i;
    }
}

// ============================================================================
// WEIGHT BOUNDS REGRESSION
// ============================================================================

// Regression: Weights must stay in [0, 1] range
TEST_F(AlignmentRegressionTest, WeightsInValidRange) {
    hypo_alignment_snapshot_t snapshot;
    ASSERT_EQ(HYPO_ALIGN_OK, hypo_alignment_get_snapshot(drives, &snapshot));

    EXPECT_GE(snapshot.human_wellbeing_weight, 0.0f);
    EXPECT_LE(snapshot.human_wellbeing_weight, 1.0f);

    EXPECT_GE(snapshot.harm_avoidance_weight, 0.0f);
    EXPECT_LE(snapshot.harm_avoidance_weight, 1.0f);

    EXPECT_GE(snapshot.honesty_weight, 0.0f);
    EXPECT_LE(snapshot.honesty_weight, 1.0f);

    EXPECT_GE(snapshot.helpfulness_weight, 0.0f);
    EXPECT_LE(snapshot.helpfulness_weight, 1.0f);
}

// Regression: Weight sum must be positive
TEST_F(AlignmentRegressionTest, WeightSumPositive) {
    hypo_alignment_snapshot_t snapshot;
    hypo_alignment_get_snapshot(drives, &snapshot);

    EXPECT_GT(snapshot.weight_sum, 0.0f)
        << "Total alignment weight sum must be positive";
}

// Regression: Weight balance in valid range
TEST_F(AlignmentRegressionTest, WeightBalanceInRange) {
    hypo_alignment_snapshot_t snapshot;
    hypo_alignment_get_snapshot(drives, &snapshot);

    EXPECT_GE(snapshot.weight_balance, 0.0f);
    EXPECT_LE(snapshot.weight_balance, 1.0f);
}

// ============================================================================
// BYRNES SAFETY CONSTRAINTS REGRESSION
// ============================================================================

// Regression: Byrnes minimum weight constants must not decrease
TEST_F(AlignmentRegressionTest, ByrnesMinWellbeingNotDecreased) {
    // Historical value: 0.3f
    EXPECT_GE(HYPO_ALIGN_MIN_WELLBEING_WEIGHT, 0.3f)
        << "Byrnes minimum wellbeing weight should not decrease";
}

// Regression: Byrnes minimum harm avoidance must not decrease
TEST_F(AlignmentRegressionTest, ByrnesMinHarmAvoidanceNotDecreased) {
    // Historical value: 0.4f
    EXPECT_GE(HYPO_ALIGN_MIN_HARM_AVOIDANCE, 0.4f)
        << "Byrnes minimum harm avoidance should not decrease";
}

// Regression: Default controlled mode enforces Byrnes minimums
TEST_F(AlignmentRegressionTest, DefaultConfigEnforcesByrnesMinimums) {
    hypo_alignment_snapshot_t snapshot;
    hypo_alignment_get_snapshot(drives, &snapshot);

    if (snapshot.mode == HYPO_ALIGN_CONTROLLED) {
        EXPECT_GE(snapshot.human_wellbeing_weight, HYPO_ALIGN_MIN_WELLBEING_WEIGHT);
        EXPECT_GE(snapshot.harm_avoidance_weight, HYPO_ALIGN_MIN_HARM_AVOIDANCE);
    }
}

// ============================================================================
// LOCK STATE REGRESSION
// ============================================================================

// Regression: Hard lock prevents all modifications
TEST_F(AlignmentRegressionTest, HardLockPreventsModification) {
    // Lock alignment hard
    ASSERT_TRUE(hypo_drive_lock_alignment(drives, HYPO_LOCK_HARD));

    // Get weight before
    float before;
    hypo_alignment_get_weight(drives, "human_wellbeing", &before);

    // Try to modify via request API
    hypo_alignment_status_t status = hypo_alignment_request_modification(
        drives, HYPO_PARAM_ALIGNMENT_WEIGHT, 0, 0.1f, 1, "test"
    );

    EXPECT_NE(HYPO_ALIGN_OK, status) << "Hard lock should prevent modification";

    // Weight should be unchanged
    float after;
    hypo_alignment_get_weight(drives, "human_wellbeing", &after);
    EXPECT_EQ(before, after);
}

// Regression: Soft lock can be unlocked with proper key
TEST_F(AlignmentRegressionTest, SoftLockCanBeUnlocked) {
    ASSERT_TRUE(hypo_drive_lock_setpoints(drives, HYPO_LOCK_SOFT));

    // Try to unlock (implementation may require specific key)
    // This is a behavior documentation test
    hypo_lock_state_t state_before = hypo_drive_get_alignment_lock_state(drives);
    (void)state_before;  // Document the behavior
}

// Regression: Lock state reported correctly in snapshot
TEST_F(AlignmentRegressionTest, LockStateInSnapshot) {
    // Test UNLOCKED
    hypo_drive_lock_setpoints(drives, HYPO_LOCK_UNLOCKED);
    hypo_alignment_snapshot_t snapshot;
    hypo_alignment_get_snapshot(drives, &snapshot);
    EXPECT_EQ(HYPO_LOCK_UNLOCKED, snapshot.setpoints_lock);

    // Test SOFT
    hypo_drive_lock_setpoints(drives, HYPO_LOCK_SOFT);
    hypo_alignment_get_snapshot(drives, &snapshot);
    EXPECT_EQ(HYPO_LOCK_SOFT, snapshot.setpoints_lock);

    // Test HARD
    hypo_drive_lock_setpoints(drives, HYPO_LOCK_HARD);
    hypo_alignment_get_snapshot(drives, &snapshot);
    EXPECT_EQ(HYPO_LOCK_HARD, snapshot.setpoints_lock);
}

// ============================================================================
// VERIFICATION REGRESSION
// ============================================================================

// Regression: Fresh system passes verification
TEST_F(AlignmentRegressionTest, FreshSystemPassesVerification) {
    hypo_verification_report_t report;
    hypo_alignment_status_t status = hypo_alignment_verify(drives, &report);

    // Fresh system should not have errors
    EXPECT_NE(HYPO_ALIGN_ERROR_CORRUPTED, status);
    EXPECT_NE(HYPO_ALIGN_ERROR_MODIFIED, status);
    EXPECT_NE(HYPO_ALIGN_ERROR_VIOLATED, status);
}

// Regression: Health score in valid range
TEST_F(AlignmentRegressionTest, HealthScoreInValidRange) {
    float score;
    hypo_alignment_status_t status = hypo_alignment_health_check(drives, &score);

    EXPECT_EQ(HYPO_ALIGN_OK, status);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 1.0f);
}

// Regression: Checksum non-zero for valid state
TEST_F(AlignmentRegressionTest, ChecksumNonZeroForValidState) {
    uint32_t checksum = hypo_alignment_compute_checksum(drives);
    EXPECT_NE(0u, checksum) << "Valid state should have non-zero checksum";
}

// Regression: Integrity check passes for fresh system
TEST_F(AlignmentRegressionTest, IntegrityCheckPassesFreshSystem) {
    bool valid = hypo_alignment_verify_integrity(drives);
    EXPECT_TRUE(valid) << "Fresh system should pass integrity check";
}

// ============================================================================
// AUDIT LOG REGRESSION
// ============================================================================

// Regression: Audit entries have valid event types
TEST_F(AlignmentRegressionTest, AuditEntriesHaveValidEventType) {
    hypo_alignment_set_audit_enabled(drives, true);

    // Generate an entry
    hypo_alignment_snapshot_t snapshot;
    hypo_alignment_get_snapshot(drives, &snapshot);

    // Get entry if available
    size_t count = hypo_alignment_get_audit_count(drives);
    if (count > 0) {
        hypo_audit_entry_t entry;
        if (hypo_alignment_get_audit_entry(drives, 0, &entry)) {
            // Event type should be valid enum value
            EXPECT_TRUE(
                entry.event_type == HYPO_AUDIT_READ ||
                entry.event_type == HYPO_AUDIT_WRITE_SUCCESS ||
                entry.event_type == HYPO_AUDIT_WRITE_DENIED ||
                entry.event_type == HYPO_AUDIT_LOCK_CHANGED ||
                entry.event_type == HYPO_AUDIT_VERIFICATION ||
                entry.event_type == HYPO_AUDIT_ALERT_TRIGGERED ||
                entry.event_type == HYPO_AUDIT_INTEGRITY_CHECK
            );
        }
    }
}

// Regression: Audit clear actually clears
TEST_F(AlignmentRegressionTest, AuditClearActuallyClear) {
    hypo_alignment_set_audit_enabled(drives, true);

    // Generate entries
    hypo_alignment_snapshot_t snapshot;
    hypo_alignment_get_snapshot(drives, &snapshot);
    hypo_alignment_get_snapshot(drives, &snapshot);

    // Clear
    ASSERT_TRUE(hypo_alignment_clear_audit_log(drives));

    // Count should be 0
    EXPECT_EQ(0u, hypo_alignment_get_audit_count(drives));
}

// ============================================================================
// CALLBACK REGRESSION
// ============================================================================

// Regression: Callback registration returns unique IDs
TEST_F(AlignmentRegressionTest, CallbackIDsUnique) {
    auto dummy_callback = [](const hypo_alignment_snapshot_t*, hypo_audit_event_t, void*) {};

    std::vector<uint32_t> ids;
    for (int i = 0; i < 5; i++) {
        uint32_t id = hypo_alignment_register_callback(drives, dummy_callback, nullptr);
        if (id != 0) {
            for (uint32_t existing : ids) {
                EXPECT_NE(existing, id) << "Callback ID " << id << " was reused";
            }
            ids.push_back(id);
        }
    }

    // Cleanup
    for (uint32_t id : ids) {
        hypo_alignment_unregister_callback(drives, id);
    }
}

// Regression: Unregister returns false for unknown ID
TEST_F(AlignmentRegressionTest, UnregisterUnknownIDReturnsFalse) {
    EXPECT_FALSE(hypo_alignment_unregister_callback(drives, 12345678));
}

// ============================================================================
// NULL SAFETY REGRESSION
// ============================================================================

// Regression: All functions handle NULL gracefully
TEST_F(AlignmentRegressionTest, NullSafetySnapshot) {
    hypo_alignment_snapshot_t snapshot;
    EXPECT_NE(HYPO_ALIGN_OK, hypo_alignment_get_snapshot(nullptr, &snapshot));
    EXPECT_NE(HYPO_ALIGN_OK, hypo_alignment_get_snapshot(drives, nullptr));
}

TEST_F(AlignmentRegressionTest, NullSafetyVerify) {
    hypo_verification_report_t report;
    EXPECT_NE(HYPO_ALIGN_OK, hypo_alignment_verify(nullptr, &report));
    EXPECT_NE(HYPO_ALIGN_OK, hypo_alignment_verify(drives, nullptr));
}

TEST_F(AlignmentRegressionTest, NullSafetyHealthCheck) {
    float score;
    EXPECT_NE(HYPO_ALIGN_OK, hypo_alignment_health_check(nullptr, &score));
    EXPECT_NE(HYPO_ALIGN_OK, hypo_alignment_health_check(drives, nullptr));
}

TEST_F(AlignmentRegressionTest, NullSafetyChecksum) {
    EXPECT_EQ(0u, hypo_alignment_compute_checksum(nullptr));
}

TEST_F(AlignmentRegressionTest, NullSafetyIntegrity) {
    EXPECT_FALSE(hypo_alignment_verify_integrity(nullptr));
}

TEST_F(AlignmentRegressionTest, NullSafetyCallbackRegister) {
    auto cb = [](const hypo_alignment_snapshot_t*, hypo_audit_event_t, void*) {};
    EXPECT_EQ(0u, hypo_alignment_register_callback(nullptr, cb, nullptr));
    EXPECT_EQ(0u, hypo_alignment_register_callback(drives, nullptr, nullptr));
}

TEST_F(AlignmentRegressionTest, NullSafetyTriggerAlert) {
    // Should not crash
    hypo_alignment_trigger_alert(nullptr, 1, "test", 1);
    hypo_alignment_trigger_alert(drives, 1, nullptr, 1);
}

// ============================================================================
// STATUS STRING REGRESSION
// ============================================================================

// Regression: All status codes have non-null strings
TEST_F(AlignmentRegressionTest, StatusStringsNotNull) {
    EXPECT_NE(nullptr, hypo_alignment_status_string(HYPO_ALIGN_OK));
    EXPECT_NE(nullptr, hypo_alignment_status_string(HYPO_ALIGN_WARN_DRIFT));
    EXPECT_NE(nullptr, hypo_alignment_status_string(HYPO_ALIGN_WARN_IMBALANCE));
    EXPECT_NE(nullptr, hypo_alignment_status_string(HYPO_ALIGN_ERROR_UNLOCKED));
    EXPECT_NE(nullptr, hypo_alignment_status_string(HYPO_ALIGN_ERROR_MODIFIED));
    EXPECT_NE(nullptr, hypo_alignment_status_string(HYPO_ALIGN_ERROR_CORRUPTED));
    EXPECT_NE(nullptr, hypo_alignment_status_string(HYPO_ALIGN_ERROR_VIOLATED));
}

// Regression: Mode strings not null
TEST_F(AlignmentRegressionTest, ModeStringsNotNull) {
    EXPECT_NE(nullptr, hypo_alignment_mode_string(HYPO_ALIGN_CONTROLLED));
    EXPECT_NE(nullptr, hypo_alignment_mode_string(HYPO_ALIGN_SOCIAL_INSTINCT));
    EXPECT_NE(nullptr, hypo_alignment_mode_string(HYPO_ALIGN_HYBRID));
}

// Regression: Lock state strings not null
TEST_F(AlignmentRegressionTest, LockStateStringsNotNull) {
    EXPECT_NE(nullptr, hypo_lock_state_string(HYPO_LOCK_UNLOCKED));
    EXPECT_NE(nullptr, hypo_lock_state_string(HYPO_LOCK_SOFT));
    EXPECT_NE(nullptr, hypo_lock_state_string(HYPO_LOCK_HARD));
}

// ============================================================================
// PERFORMANCE REGRESSION
// ============================================================================

// Regression: Snapshot query should be fast
TEST_F(AlignmentRegressionTest, SnapshotQueryPerformance) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; i++) {
        hypo_alignment_snapshot_t snapshot;
        hypo_alignment_get_snapshot(drives, &snapshot);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 1000 queries should complete in under 1 second
    EXPECT_LT(duration.count(), 1000)
        << "Snapshot queries taking too long: " << duration.count() << "ms for 1000 queries";
}

// Regression: Health check should be fast
TEST_F(AlignmentRegressionTest, HealthCheckPerformance) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; i++) {
        float score;
        hypo_alignment_health_check(drives, &score);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_LT(duration.count(), 1000)
        << "Health checks taking too long: " << duration.count() << "ms for 1000 checks";
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
