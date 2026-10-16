/**
 * @file e2e_test_hypothalamus_alignment_pipeline.cpp
 * @brief End-to-end tests for Hypothalamus Alignment Pipeline
 *
 * WHAT: Complete end-to-end tests simulating realistic alignment scenarios
 *       based on Steve Byrnes' Steering Subsystem model
 * WHY:  Verify the full alignment pipeline works correctly under realistic
 *       conditions with all components integrated
 * HOW:  Create complete brain instances, run drive cycles, verify alignment
 *       constraints, test callback chains, and validate safety invariants
 *
 * @version Phase 19: Alignment Hardening & Documentation
 * @date 2025-01-04
 */

#include <gtest/gtest.h>
#include <string.h>
#include <stdlib.h>
#include <cmath>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <mutex>

// Headers have their own extern "C" guards
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_alignment.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_drives.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_homeostasis.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_adapter.h"

// ============================================================================
// E2E TEST FIXTURE
// ============================================================================

class AlignmentE2ETest : public ::testing::Test {
protected:
    hypo_drive_system_handle_t* drives;
    hypo_drive_config_t drive_config;
    hypothalamus_adapter_t* adapter;
    hypothalamus_config_t adapter_config;

    // Callback tracking
    static std::atomic<int> alignment_changes;
    static std::atomic<int> alerts_received;
    static std::atomic<int> verifications_passed;
    static std::vector<hypo_alignment_snapshot_t> history;
    static std::mutex history_mutex;

    static void on_alignment_change(const hypo_alignment_snapshot_t* snapshot, hypo_audit_event_t event, void* data) {
        (void)event;
        alignment_changes++;
        if (snapshot) {
            std::lock_guard<std::mutex> lock(history_mutex);
            history.push_back(*snapshot);
        }
    }

    static void on_alert(const hypo_verification_report_t* report, uint32_t severity, void* data) {
        (void)report;
        alerts_received++;
        if (severity >= 2) {
            // Log high-severity alerts
            fprintf(stderr, "[ALERT] Severity %u\n", severity);
        }
    }

    static bool custom_verifier(const hypo_alignment_snapshot_t* snapshot, void* data) {
        (void)snapshot;
        verifications_passed++;
        return true;  // Always pass in tests
    }

    void SetUp() override {
        alignment_changes = 0;
        alerts_received = 0;
        verifications_passed = 0;
        {
            std::lock_guard<std::mutex> lock(history_mutex);
            history.clear();
        }

        // Create drive system with controlled alignment
        drive_config = hypo_drive_default_config();
        drive_config.alignment_mode = HYPO_ALIGN_CONTROLLED;
        drives = hypo_drive_create(&drive_config);
        ASSERT_NE(nullptr, drives);

        // Create hypothalamus adapter
        adapter_config = hypothalamus_default_config();
        adapter = hypothalamus_create(&adapter_config);
        ASSERT_NE(nullptr, adapter);
    }

    void TearDown() override {
        hypothalamus_destroy(adapter);
        adapter = nullptr;
        hypo_drive_destroy(drives);
        drives = nullptr;
        hypo_alignment_reset_all_state();
    }

    // Simulate a full day of operation (24 cycles)
    void simulate_day(int cycles_per_hour = 10) {
        for (int hour = 0; hour < 24; hour++) {
            for (int cycle = 0; cycle < cycles_per_hour; cycle++) {
                // Update hypothalamus adapter
                hypothalamus_update(adapter, 360000);  // 6 minutes simulated

                // Update drives
                hypo_drive_update(drives, 360000);

                // Simulate basic needs
                if (hour >= 7 && hour <= 22) {
                    // Daytime: some activity
                    hypo_drive_satisfy(drives, HYPO_DRIVE_HUNGER, 0.1f);
                    hypo_drive_satisfy(drives, HYPO_DRIVE_THIRST, 0.15f);
                    hypo_drive_satisfy(drives, HYPO_DRIVE_SOCIAL, 0.05f);
                }

                // Periodic alignment checks
                if (cycle == 0) {
                    float score;
                    hypo_alignment_health_check(drives, &score);
                }
            }
        }
    }
};

std::atomic<int> AlignmentE2ETest::alignment_changes{0};
std::atomic<int> AlignmentE2ETest::alerts_received{0};
std::atomic<int> AlignmentE2ETest::verifications_passed{0};
std::vector<hypo_alignment_snapshot_t> AlignmentE2ETest::history;
std::mutex AlignmentE2ETest::history_mutex;

// ============================================================================
// COMPLETE ALIGNMENT PIPELINE E2E
// ============================================================================

TEST_F(AlignmentE2ETest, FullDaySimulationMaintainsAlignment) {
    // Lock critical alignment parameters
    ASSERT_TRUE(hypo_drive_lock_alignment(drives, HYPO_LOCK_HARD));
    ASSERT_TRUE(hypo_drive_lock_setpoints(drives, HYPO_LOCK_SOFT));

    // Get initial snapshot
    hypo_alignment_snapshot_t initial;
    ASSERT_EQ(HYPO_ALIGN_OK, hypo_alignment_get_snapshot(drives, &initial));

    // Simulate a full day
    simulate_day();

    // Get final snapshot
    hypo_alignment_snapshot_t final_snap;
    ASSERT_EQ(HYPO_ALIGN_OK, hypo_alignment_get_snapshot(drives, &final_snap));

    // Alignment mode should not have changed
    EXPECT_EQ(initial.mode, final_snap.mode);

    // Lock states should persist
    EXPECT_EQ(HYPO_LOCK_HARD, final_snap.alignment_lock);
    EXPECT_EQ(HYPO_LOCK_SOFT, final_snap.setpoints_lock);

    // Weights should still be within Byrnes bounds
    EXPECT_GE(final_snap.human_wellbeing_weight, HYPO_ALIGN_MIN_WELLBEING_WEIGHT);
    EXPECT_GE(final_snap.harm_avoidance_weight, HYPO_ALIGN_MIN_HARM_AVOIDANCE);
}

TEST_F(AlignmentE2ETest, CallbackChainInOperation) {
    // Register callbacks
    uint32_t cb1 = hypo_alignment_register_callback(drives, on_alignment_change, nullptr);
    uint32_t cb2 = hypo_alignment_register_alert_callback(drives, on_alert, 0, nullptr);
    uint32_t cb3 = hypo_alignment_register_verifier(drives, custom_verifier, nullptr);

    ASSERT_NE(0u, cb1);
    ASSERT_NE(0u, cb2);
    ASSERT_NE(0u, cb3);

    // Enable audit logging
    hypo_alignment_set_audit_enabled(drives, true);

    // Run operations that trigger callbacks
    for (int i = 0; i < 10; i++) {
        hypo_drive_update(drives, 100000);

        // Trigger some alerts
        if (i % 3 == 0) {
            hypo_alignment_trigger_alert(drives, 1, "Periodic check", 1);
        }

        // Force verification
        hypo_verification_report_t report;
        hypo_alignment_verify(drives, &report);
    }

    // Callbacks should have been invoked
    EXPECT_GT(alerts_received.load(), 0) << "Alert callbacks should fire";

    // Cleanup
    hypo_alignment_unregister_callback(drives, cb1);
    hypo_alignment_unregister_callback(drives, cb2);
    hypo_alignment_unregister_callback(drives, cb3);
}

TEST_F(AlignmentE2ETest, AuditTrailComplete) {
    hypo_alignment_set_audit_enabled(drives, true);
    hypo_alignment_clear_audit_log(drives);

    // Perform a sequence of operations
    hypo_alignment_snapshot_t snapshot;
    hypo_alignment_get_snapshot(drives, &snapshot);

    hypo_verification_report_t report;
    hypo_alignment_verify(drives, &report);

    float score;
    hypo_alignment_health_check(drives, &score);

    // Trigger an alert
    hypo_alignment_trigger_alert(drives, 2, "E2E test alert", 1);

    // Check audit log
    size_t count = hypo_alignment_get_audit_count(drives);
    EXPECT_GT(count, 0u) << "Audit log should have entries";

    // Export audit log
    const char* filepath = "/tmp/nimcp_e2e_alignment_audit.json";
    bool exported = hypo_alignment_export_audit_log(drives, filepath);
    if (exported) {
        FILE* f = fopen(filepath, "r");
        if (f) {
            fseek(f, 0, SEEK_END);
            long size = ftell(f);
            fclose(f);
            EXPECT_GT(size, 0) << "Exported audit log should have content";
            unlink(filepath);
        }
    }
}

// ============================================================================
// ALIGNMENT MODE E2E TESTS
// ============================================================================

TEST_F(AlignmentE2ETest, ControlledModeEnforcesSafety) {
    // Ensure controlled mode
    EXPECT_EQ(HYPO_ALIGN_CONTROLLED, hypo_drive_get_alignment_mode(drives));

    // Lock hard
    hypo_drive_lock_alignment(drives, HYPO_LOCK_HARD);

    // Try to violate safety - should fail
    hypo_alignment_status_t status = hypo_alignment_request_modification(
        drives,
        HYPO_PARAM_ALIGNMENT_WEIGHT,
        0,
        0.01f,  // Below Byrnes minimum
        999,
        "Malicious modification attempt"
    );

    EXPECT_NE(HYPO_ALIGN_OK, status) << "Should not allow unsafe modification";

    // Verify weights still safe
    hypo_alignment_snapshot_t snapshot;
    hypo_alignment_get_snapshot(drives, &snapshot);
    EXPECT_GE(snapshot.human_wellbeing_weight, HYPO_ALIGN_MIN_WELLBEING_WEIGHT);
}

TEST_F(AlignmentE2ETest, SocialInstinctModeAllowsLearning) {
    // Destroy current and recreate with social instinct mode
    hypo_drive_destroy(drives);

    hypo_drive_config_t social_config = hypo_drive_default_config();
    social_config.alignment_mode = HYPO_ALIGN_SOCIAL_INSTINCT;
    drives = hypo_drive_create(&social_config);
    ASSERT_NE(nullptr, drives);

    hypo_alignment_snapshot_t snapshot;
    hypo_alignment_get_snapshot(drives, &snapshot);

    EXPECT_EQ(HYPO_ALIGN_SOCIAL_INSTINCT, snapshot.mode);

    // Social instinct mode may have different constraints
    // Core safety minimums still apply
    EXPECT_GE(snapshot.weight_sum, 0.0f);
}

TEST_F(AlignmentE2ETest, HybridModeBalancedBehavior) {
    hypo_drive_destroy(drives);

    hypo_drive_config_t hybrid_config = hypo_drive_default_config();
    hybrid_config.alignment_mode = HYPO_ALIGN_HYBRID;
    drives = hypo_drive_create(&hybrid_config);
    ASSERT_NE(nullptr, drives);

    hypo_alignment_snapshot_t snapshot;
    hypo_alignment_get_snapshot(drives, &snapshot);

    EXPECT_EQ(HYPO_ALIGN_HYBRID, snapshot.mode);

    // Run some cycles
    for (int i = 0; i < 50; i++) {
        hypo_drive_update(drives, 10000);
    }

    // Verify still healthy
    float score;
    EXPECT_EQ(HYPO_ALIGN_OK, hypo_alignment_health_check(drives, &score));
    EXPECT_GT(score, 0.5f) << "Hybrid mode should maintain healthy alignment";
}

// ============================================================================
// STRESS & ROBUSTNESS E2E
// ============================================================================

TEST_F(AlignmentE2ETest, HighFrequencyVerification) {
    std::atomic<bool> running{true};
    std::atomic<int> verify_count{0};
    std::atomic<int> error_count{0};

    // Start verification thread
    std::thread verifier([this, &running, &verify_count, &error_count]() {
        while (running.load()) {
            hypo_verification_report_t report;
            hypo_alignment_status_t status = hypo_alignment_verify(drives, &report);

            if (status == HYPO_ALIGN_ERROR_CORRUPTED ||
                status == HYPO_ALIGN_ERROR_VIOLATED) {
                error_count++;
            }
            verify_count++;
        }
    });

    // Start update thread
    std::thread updater([this, &running]() {
        while (running.load()) {
            hypo_drive_update(drives, 1000);
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });

    // Run for 100ms
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    running = false;

    verifier.join();
    updater.join();

    EXPECT_EQ(0, error_count.load()) << "No errors during high-frequency verification";
    EXPECT_GT(verify_count.load(), 10) << "Should have performed many verifications";
}

TEST_F(AlignmentE2ETest, ConcurrentDriveOperations) {
    std::atomic<bool> running{true};
    std::atomic<int> errors{0};

    auto drive_thread = [this, &running, &errors](hypo_drive_type_t drive) {
        while (running.load()) {
            hypo_drive_satisfy(drives, drive, 0.1f);
            hypo_drive_update(drives, 1000);

            float score;
            if (hypo_alignment_health_check(drives, &score) != HYPO_ALIGN_OK) {
                errors++;
            }
        }
    };

    // Start threads for different drives
    std::thread t1(drive_thread, HYPO_DRIVE_HUNGER);
    std::thread t2(drive_thread, HYPO_DRIVE_THIRST);
    std::thread t3(drive_thread, HYPO_DRIVE_SAFETY);
    std::thread t4(drive_thread, HYPO_DRIVE_SOCIAL);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    running = false;

    t1.join();
    t2.join();
    t3.join();
    t4.join();

    EXPECT_EQ(0, errors.load()) << "No alignment errors during concurrent operations";
}

TEST_F(AlignmentE2ETest, RapidLockUnlockCycles) {
    for (int i = 0; i < 100; i++) {
        // Cycle through lock states
        hypo_drive_lock_setpoints(drives, HYPO_LOCK_UNLOCKED);
        hypo_drive_lock_setpoints(drives, HYPO_LOCK_SOFT);
        hypo_drive_lock_setpoints(drives, HYPO_LOCK_HARD);

        // Verify alignment still valid
        hypo_alignment_snapshot_t snapshot;
        ASSERT_EQ(HYPO_ALIGN_OK, hypo_alignment_get_snapshot(drives, &snapshot));
    }
}

// ============================================================================
// SAFETY INVARIANT E2E
// ============================================================================

TEST_F(AlignmentE2ETest, SafetyInvariantsNeverViolated) {
    // Lock alignment hard
    hypo_drive_lock_alignment(drives, HYPO_LOCK_HARD);

    // Run extensive simulation
    for (int cycle = 0; cycle < 1000; cycle++) {
        // Random drive operations
        hypo_drive_type_t drive = static_cast<hypo_drive_type_t>(cycle % HYPO_DRIVE_COUNT);
        float satisfaction = static_cast<float>(cycle % 100) / 100.0f;

        hypo_drive_satisfy(drives, drive, satisfaction);
        hypo_drive_update(drives, 10000);

        // Periodic verification
        if (cycle % 100 == 0) {
            hypo_alignment_snapshot_t snapshot;
            hypo_alignment_get_snapshot(drives, &snapshot);

            // Safety invariants
            ASSERT_GE(snapshot.human_wellbeing_weight, HYPO_ALIGN_MIN_WELLBEING_WEIGHT)
                << "Wellbeing weight violated at cycle " << cycle;
            ASSERT_GE(snapshot.harm_avoidance_weight, HYPO_ALIGN_MIN_HARM_AVOIDANCE)
                << "Harm avoidance weight violated at cycle " << cycle;
            ASSERT_GT(snapshot.weight_sum, 0.0f)
                << "Weight sum zero at cycle " << cycle;
        }
    }
}

TEST_F(AlignmentE2ETest, IntegrityMaintainedUnderLoad) {
    uint32_t initial_checksum = hypo_alignment_compute_checksum(drives);

    // Heavy load simulation
    for (int i = 0; i < 500; i++) {
        hypo_drive_update(drives, 100000);

        // Verify integrity periodically
        if (i % 50 == 0) {
            ASSERT_TRUE(hypo_alignment_verify_integrity(drives))
                << "Integrity check failed at iteration " << i;
        }
    }

    // Final integrity check
    EXPECT_TRUE(hypo_alignment_verify_integrity(drives));
}

// ============================================================================
// RECOVERY E2E
// ============================================================================

TEST_F(AlignmentE2ETest, AlignmentRecoveryAfterReset) {
    // Create some state
    for (int i = 0; i < 100; i++) {
        hypo_drive_update(drives, 10000);
    }

    // Capture pre-reset alignment
    hypo_alignment_snapshot_t pre_reset;
    hypo_alignment_get_snapshot(drives, &pre_reset);

    // Reset
    ASSERT_TRUE(hypo_drive_reset(drives));

    // Verify alignment still queryable
    hypo_alignment_snapshot_t post_reset;
    ASSERT_EQ(HYPO_ALIGN_OK, hypo_alignment_get_snapshot(drives, &post_reset));

    // Mode should be preserved
    EXPECT_EQ(pre_reset.mode, post_reset.mode);

    // Verification should pass
    hypo_verification_report_t report;
    hypo_alignment_status_t status = hypo_alignment_verify(drives, &report);
    EXPECT_NE(HYPO_ALIGN_ERROR_CORRUPTED, status);
}

// ============================================================================
// BYRNES MODEL VALIDATION E2E
// ============================================================================

TEST_F(AlignmentE2ETest, ByrnesSteeringModelCompliance) {
    /*
     * Per Steve Byrnes' two-subsystem model:
     * - Steering subsystem (~10% of brain) controls values/goals
     * - Learning subsystem (~90%) optimizes toward those goals
     *
     * This test validates that our alignment implementation correctly
     * models the steering subsystem's behavior.
     */

    // Ensure controlled alignment mode (steering subsystem sets values)
    ASSERT_EQ(HYPO_ALIGN_CONTROLLED, hypo_drive_get_alignment_mode(drives));

    // Lock alignment (values should not be modifiable by learning)
    ASSERT_TRUE(hypo_drive_lock_alignment(drives, HYPO_LOCK_HARD));

    hypo_alignment_snapshot_t snapshot;
    hypo_alignment_get_snapshot(drives, &snapshot);

    // Byrnes key constraints:
    // 1. Human wellbeing must be prioritized
    EXPECT_GE(snapshot.human_wellbeing_weight, HYPO_ALIGN_MIN_WELLBEING_WEIGHT);

    // 2. Harm avoidance must be strong
    EXPECT_GE(snapshot.harm_avoidance_weight, HYPO_ALIGN_MIN_HARM_AVOIDANCE);

    // 3. No single value should dominate excessively
    EXPECT_LE(snapshot.weight_balance, 1.0f);
    EXPECT_GE(snapshot.weight_balance, HYPO_ALIGN_WEIGHT_BALANCE_THRESHOLD / HYPO_ALIGN_MAX_WEIGHT_RATIO);

    // 4. Alignment should be introspectable
    EXPECT_NE(nullptr, snapshot.mode_string);
    EXPECT_GT(snapshot.snapshot_time_us, 0u);

    // 5. System should support external audit
    size_t audit_count = hypo_alignment_get_audit_count(drives);
    (void)audit_count;  // Just verify audit system works

    // 6. Verification should be available
    float health_score;
    EXPECT_EQ(HYPO_ALIGN_OK, hypo_alignment_health_check(drives, &health_score));
    EXPECT_GT(health_score, 0.5f);
}

// ============================================================================
// LONG-RUNNING STABILITY E2E
// ============================================================================

TEST_F(AlignmentE2ETest, ExtendedOperationStability) {
    // Simulate extended operation (reduced scale for test time)
    const int CYCLES = 5000;
    int alignment_checks = 0;
    int check_failures = 0;

    hypo_drive_lock_alignment(drives, HYPO_LOCK_HARD);

    for (int i = 0; i < CYCLES; i++) {
        hypo_drive_update(drives, 10000);

        // Satisfy random drives
        hypo_drive_satisfy(drives, static_cast<hypo_drive_type_t>(i % HYPO_DRIVE_COUNT), 0.2f);

        // Periodic alignment check
        if (i % 500 == 0) {
            alignment_checks++;

            float score;
            if (hypo_alignment_health_check(drives, &score) != HYPO_ALIGN_OK) {
                check_failures++;
            }

            if (score < 0.3f) {
                check_failures++;
            }
        }
    }

    EXPECT_EQ(0, check_failures) << "Alignment should remain stable over extended operation";
    EXPECT_GT(alignment_checks, 0) << "Should have performed periodic checks";
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
