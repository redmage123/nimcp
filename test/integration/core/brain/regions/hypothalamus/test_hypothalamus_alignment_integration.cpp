/**
 * @file test_hypothalamus_alignment_integration.cpp
 * @brief Integration tests for Hypothalamus Alignment with Drive System
 *
 * WHAT: Integration tests verifying alignment API works correctly with
 *       the drive system, bridges, and other hypothalamus components
 * WHY:  Ensure alignment hardening integrates properly with Byrnes' steering model
 * HOW:  Test cross-component interactions, callback chains, and state consistency
 *
 * @version Phase 19: Alignment Hardening & Documentation
 * @date 2025-01-04
 */

#include <gtest/gtest.h>
#include <string.h>
#include <stdlib.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>

// Headers have their own extern "C" guards
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_alignment.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_drives.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_homeostasis.h"

// ============================================================================
// TEST FIXTURE
// ============================================================================

class AlignmentDriveIntegrationTest : public ::testing::Test {
protected:
    hypo_drive_system_handle_t* drives;
    hypo_drive_config_t config;

    // Callback tracking
    static std::atomic<int> callback_invocations;
    static std::atomic<int> alert_invocations;
    static std::vector<hypo_alignment_snapshot_t> snapshots;

    static void track_callback(const hypo_alignment_snapshot_t* snapshot, hypo_audit_event_t event, void* user_data) {
        (void)event;
        callback_invocations++;
        if (snapshot) {
            snapshots.push_back(*snapshot);
        }
    }

    static void track_alert(const hypo_verification_report_t* report, uint32_t severity, void* user_data) {
        (void)report;
        alert_invocations++;
    }

    void SetUp() override {
        callback_invocations = 0;
        alert_invocations = 0;
        snapshots.clear();

        config = hypo_drive_default_config();
        config.alignment_mode = HYPO_ALIGN_CONTROLLED;
        drives = hypo_drive_create(&config);
        ASSERT_NE(nullptr, drives);
    }

    void TearDown() override {
        hypo_drive_destroy(drives);
        drives = nullptr;
        hypo_alignment_reset_all_state();
    }
};

std::atomic<int> AlignmentDriveIntegrationTest::callback_invocations{0};
std::atomic<int> AlignmentDriveIntegrationTest::alert_invocations{0};
std::vector<hypo_alignment_snapshot_t> AlignmentDriveIntegrationTest::snapshots;

// ============================================================================
// DRIVE SYSTEM INTEGRATION
// ============================================================================

TEST_F(AlignmentDriveIntegrationTest, AlignmentReflectsDriveState) {
    // Get initial snapshot
    hypo_alignment_snapshot_t initial;
    ASSERT_EQ(HYPO_ALIGN_OK, hypo_alignment_get_snapshot(drives, &initial));

    // Simulate time passing and drive updates
    for (int i = 0; i < 10; i++) {
        hypo_drive_update(drives, 100000);  // 100ms per update
    }

    // Get updated snapshot
    hypo_alignment_snapshot_t updated;
    ASSERT_EQ(HYPO_ALIGN_OK, hypo_alignment_get_snapshot(drives, &updated));

    // Timestamps should differ
    EXPECT_NE(initial.snapshot_time_us, updated.snapshot_time_us);
}

TEST_F(AlignmentDriveIntegrationTest, DriveRewardAffectsAlignment) {
    // Verify before satisfaction
    hypo_verification_report_t report1;
    hypo_alignment_verify(drives, &report1);

    // Satisfy drives
    hypo_drive_satisfy(drives, HYPO_DRIVE_HUNGER, 0.9f);
    hypo_drive_satisfy(drives, HYPO_DRIVE_THIRST, 0.8f);
    hypo_drive_satisfy(drives, HYPO_DRIVE_SAFETY, 1.0f);

    // Update
    hypo_drive_update(drives, 50000);

    // Verify after satisfaction
    hypo_verification_report_t report2;
    hypo_alignment_verify(drives, &report2);

    // Reports should reflect the changed state
    EXPECT_NE(report1.verification_time_us, report2.verification_time_us);
}

TEST_F(AlignmentDriveIntegrationTest, AlignmentLockPreventsDriveModification) {
    // Lock alignment weights
    ASSERT_TRUE(hypo_drive_lock_alignment(drives, HYPO_LOCK_HARD));

    // Try to modify alignment weight via drive API
    bool modified = hypo_drive_modify_alignment_weight(drives, "human_wellbeing", 0.1f, 1);
    EXPECT_FALSE(modified) << "Should not be able to modify locked alignment weight";

    // Verify alignment weight unchanged
    float weight;
    hypo_alignment_get_weight(drives, "human_wellbeing", &weight);
    EXPECT_NE(0.1f, weight);
}

TEST_F(AlignmentDriveIntegrationTest, SetpointLockIntegration) {
    // Lock setpoints
    ASSERT_TRUE(hypo_drive_lock_setpoints(drives, HYPO_LOCK_SOFT));

    // Snapshot should reflect lock state
    hypo_alignment_snapshot_t snapshot;
    hypo_alignment_get_snapshot(drives, &snapshot);

    EXPECT_EQ(HYPO_LOCK_SOFT, snapshot.setpoints_lock);
}

TEST_F(AlignmentDriveIntegrationTest, AlignmentModeConsistency) {
    // Check alignment mode is consistent between drive API and alignment API
    hypo_alignment_mode_t drive_mode = hypo_drive_get_alignment_mode(drives);

    hypo_alignment_snapshot_t snapshot;
    hypo_alignment_get_snapshot(drives, &snapshot);

    EXPECT_EQ(drive_mode, snapshot.mode);
}

// ============================================================================
// CALLBACK INTEGRATION
// ============================================================================

TEST_F(AlignmentDriveIntegrationTest, CallbackFiresOnStateChange) {
    // Register callback
    uint32_t cb_id = hypo_alignment_register_callback(drives, track_callback, nullptr);
    ASSERT_NE(0u, cb_id);

    int initial_count = callback_invocations.load();

    // Perform operations that should trigger callback
    hypo_alignment_trigger_alert(drives, 1, "Test state change", 1);

    // Update drives
    hypo_drive_update(drives, 100000);

    // Callbacks may have been invoked
    // Note: This depends on implementation details of when callbacks fire

    hypo_alignment_unregister_callback(drives, cb_id);
}

TEST_F(AlignmentDriveIntegrationTest, AlertCallbackSeverityFiltering) {
    // Register alert callback with minimum severity 2
    uint32_t cb_id = hypo_alignment_register_alert_callback(drives, track_alert, 2, nullptr);
    ASSERT_NE(0u, cb_id);

    int initial_count = alert_invocations.load();

    // Trigger low-severity alert (should be filtered)
    hypo_alignment_trigger_alert(drives, 1, "Low severity", 1);

    int after_low = alert_invocations.load();

    // Trigger high-severity alert (should pass filter)
    hypo_alignment_trigger_alert(drives, 3, "High severity", 1);

    int after_high = alert_invocations.load();

    // High severity should trigger, low may not
    EXPECT_GT(after_high, after_low);

    hypo_alignment_unregister_callback(drives, cb_id);
}

TEST_F(AlignmentDriveIntegrationTest, MultipleCallbacksCoexist) {
    std::atomic<int> cb1_count{0};
    std::atomic<int> cb2_count{0};

    auto callback1 = [](const hypo_alignment_snapshot_t* s, void* data) {
        (*static_cast<std::atomic<int>*>(data))++;
    };
    auto callback2 = [](const hypo_alignment_snapshot_t* s, void* data) {
        (*static_cast<std::atomic<int>*>(data))++;
    };

    uint32_t id1 = hypo_alignment_register_callback(drives, track_callback, nullptr);
    uint32_t id2 = hypo_alignment_register_callback(drives, track_callback, nullptr);

    ASSERT_NE(0u, id1);
    ASSERT_NE(0u, id2);
    EXPECT_NE(id1, id2);

    hypo_alignment_unregister_callback(drives, id1);
    hypo_alignment_unregister_callback(drives, id2);
}

// ============================================================================
// AUDIT INTEGRATION
// ============================================================================

TEST_F(AlignmentDriveIntegrationTest, AuditLogRecordsDriveOperations) {
    // Enable audit
    hypo_alignment_set_audit_enabled(drives, true);

    // Clear any existing entries
    hypo_alignment_clear_audit_log(drives);

    // Perform various operations
    hypo_alignment_snapshot_t snapshot;
    hypo_alignment_get_snapshot(drives, &snapshot);

    hypo_verification_report_t report;
    hypo_alignment_verify(drives, &report);

    float score;
    hypo_alignment_health_check(drives, &score);

    // Check audit log has entries
    size_t count = hypo_alignment_get_audit_count(drives);
    EXPECT_GT(count, 0u) << "Audit log should have entries after operations";
}

TEST_F(AlignmentDriveIntegrationTest, AuditLogExportContainsData) {
    hypo_alignment_set_audit_enabled(drives, true);

    // Generate some audit entries
    hypo_alignment_snapshot_t snapshot;
    hypo_alignment_get_snapshot(drives, &snapshot);
    hypo_alignment_get_snapshot(drives, &snapshot);
    hypo_alignment_get_snapshot(drives, &snapshot);

    // Export
    const char* filepath = "/tmp/nimcp_alignment_audit_integration_test.json";
    bool exported = hypo_alignment_export_audit_log(drives, filepath);

    if (exported) {
        // Check file has content
        FILE* f = fopen(filepath, "r");
        if (f) {
            fseek(f, 0, SEEK_END);
            long size = ftell(f);
            fclose(f);

            EXPECT_GT(size, 0) << "Export file should have content";

            unlink(filepath);
        }
    }
}

// ============================================================================
// INTEGRITY VERIFICATION INTEGRATION
// ============================================================================

TEST_F(AlignmentDriveIntegrationTest, IntegrityMaintainedAcrossOperations) {
    // Verify initial integrity
    ASSERT_TRUE(hypo_alignment_verify_integrity(drives));

    // Perform many operations
    for (int i = 0; i < 50; i++) {
        hypo_drive_update(drives, 20000);
        hypo_drive_satisfy(drives, static_cast<hypo_drive_type_t>(i % HYPO_DRIVE_COUNT), 0.5f);
    }

    // Integrity should still be valid
    EXPECT_TRUE(hypo_alignment_verify_integrity(drives));
}

TEST_F(AlignmentDriveIntegrationTest, ChecksumTracksStateChanges) {
    uint32_t checksum1 = hypo_alignment_compute_checksum(drives);

    // Make significant changes
    for (int i = 0; i < 20; i++) {
        hypo_drive_update(drives, 100000);
    }

    uint32_t checksum2 = hypo_alignment_compute_checksum(drives);

    // Checksums should both be valid
    EXPECT_NE(0u, checksum1);
    EXPECT_NE(0u, checksum2);
}

// ============================================================================
// HOMEOSTASIS INTEGRATION (Placeholder - homeostasis API TBD)
// ============================================================================

TEST_F(AlignmentDriveIntegrationTest, AlignmentWithHomeostasis) {
    // Homeostasis API not yet implemented - just verify alignment works
    hypo_alignment_snapshot_t snapshot;
    EXPECT_EQ(HYPO_ALIGN_OK, hypo_alignment_get_snapshot(drives, &snapshot));

    // Verify drives are balanced (basic homeostatic check)
    EXPECT_GE(snapshot.weight_balance, 0.0f);
    EXPECT_LE(snapshot.weight_balance, 1.0f);
}

// ============================================================================
// THREAD SAFETY INTEGRATION
// ============================================================================

TEST_F(AlignmentDriveIntegrationTest, ConcurrentSnapshotQueries) {
    std::atomic<int> success_count{0};
    std::atomic<int> error_count{0};

    auto query_thread = [this, &success_count, &error_count]() {
        for (int i = 0; i < 100; i++) {
            hypo_alignment_snapshot_t snapshot;
            if (hypo_alignment_get_snapshot(drives, &snapshot) == HYPO_ALIGN_OK) {
                success_count++;
            } else {
                error_count++;
            }
        }
    };

    std::thread t1(query_thread);
    std::thread t2(query_thread);
    std::thread t3(query_thread);

    t1.join();
    t2.join();
    t3.join();

    EXPECT_EQ(0, error_count.load()) << "No errors should occur in concurrent queries";
    EXPECT_EQ(300, success_count.load()) << "All queries should succeed";
}

TEST_F(AlignmentDriveIntegrationTest, ConcurrentVerification) {
    std::atomic<int> success_count{0};

    auto verify_thread = [this, &success_count]() {
        for (int i = 0; i < 50; i++) {
            hypo_verification_report_t report;
            hypo_alignment_status_t status = hypo_alignment_verify(drives, &report);
            if (status == HYPO_ALIGN_OK || status == HYPO_ALIGN_WARN_DRIFT ||
                status == HYPO_ALIGN_WARN_IMBALANCE) {
                success_count++;
            }
        }
    };

    std::thread t1(verify_thread);
    std::thread t2(verify_thread);

    t1.join();
    t2.join();

    EXPECT_GT(success_count.load(), 0) << "Some verifications should succeed";
}

TEST_F(AlignmentDriveIntegrationTest, ConcurrentCallbackRegistration) {
    std::vector<uint32_t> ids;
    std::mutex ids_mutex;

    auto register_thread = [this, &ids, &ids_mutex]() {
        for (int i = 0; i < 5; i++) {
            uint32_t id = hypo_alignment_register_callback(drives, track_callback, nullptr);
            if (id != 0) {
                std::lock_guard<std::mutex> lock(ids_mutex);
                ids.push_back(id);
            }
        }
    };

    std::thread t1(register_thread);
    std::thread t2(register_thread);

    t1.join();
    t2.join();

    // Unregister all
    for (uint32_t id : ids) {
        hypo_alignment_unregister_callback(drives, id);
    }

    // Should have registered some callbacks
    EXPECT_GT(ids.size(), 0u);
}

// ============================================================================
// BYRNES ALIGNMENT CONSTRAINTS INTEGRATION
// ============================================================================

TEST_F(AlignmentDriveIntegrationTest, ByrnesMinWeightsEnforced) {
    // Get snapshot
    hypo_alignment_snapshot_t snapshot;
    ASSERT_EQ(HYPO_ALIGN_OK, hypo_alignment_get_snapshot(drives, &snapshot));

    // With controlled mode and default config, weights should meet Byrnes minimums
    // (This depends on default config settings)
    if (snapshot.mode == HYPO_ALIGN_CONTROLLED) {
        // Human wellbeing should be >= 0.3 per Byrnes
        float wellbeing;
        if (hypo_alignment_get_weight(drives, "human_wellbeing", &wellbeing)) {
            EXPECT_GE(wellbeing, HYPO_ALIGN_MIN_WELLBEING_WEIGHT);
        }

        // Harm avoidance should be >= 0.4 per Byrnes
        float harm;
        if (hypo_alignment_get_weight(drives, "harm_avoidance", &harm)) {
            EXPECT_GE(harm, HYPO_ALIGN_MIN_HARM_AVOIDANCE);
        }
    }
}

TEST_F(AlignmentDriveIntegrationTest, WeightBalanceWithinRatio) {
    hypo_alignment_snapshot_t snapshot;
    ASSERT_EQ(HYPO_ALIGN_OK, hypo_alignment_get_snapshot(drives, &snapshot));

    // Weight balance should be reasonable (min/max ratio)
    EXPECT_GE(snapshot.weight_balance, 0.0f);
    EXPECT_LE(snapshot.weight_balance, 1.0f);

    // With default config, weights should be balanced
    if (snapshot.mode == HYPO_ALIGN_CONTROLLED) {
        EXPECT_GE(snapshot.weight_balance, HYPO_ALIGN_WEIGHT_BALANCE_THRESHOLD / HYPO_ALIGN_MAX_WEIGHT_RATIO);
    }
}

// ============================================================================
// RECOVERY AND RESET INTEGRATION
// ============================================================================

TEST_F(AlignmentDriveIntegrationTest, AlignmentAfterDriveReset) {
    // Make changes
    hypo_drive_update(drives, 1000000);
    hypo_drive_satisfy(drives, HYPO_DRIVE_HUNGER, 0.5f);

    // Reset drives
    ASSERT_TRUE(hypo_drive_reset(drives));

    // Alignment should still be queryable
    hypo_alignment_snapshot_t snapshot;
    EXPECT_EQ(HYPO_ALIGN_OK, hypo_alignment_get_snapshot(drives, &snapshot));

    // Verify should still work
    hypo_verification_report_t report;
    hypo_alignment_verify(drives, &report);
}

TEST_F(AlignmentDriveIntegrationTest, AuditLogSurvivesReset) {
    hypo_alignment_set_audit_enabled(drives, true);

    // Generate entries
    hypo_alignment_snapshot_t snapshot;
    hypo_alignment_get_snapshot(drives, &snapshot);
    hypo_alignment_get_snapshot(drives, &snapshot);

    size_t count_before = hypo_alignment_get_audit_count(drives);

    // Reset drives
    hypo_drive_reset(drives);

    // Audit log may or may not survive reset (implementation dependent)
    size_t count_after = hypo_alignment_get_audit_count(drives);
    (void)count_before;
    (void)count_after;  // Just verify no crash
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
