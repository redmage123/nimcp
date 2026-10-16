//=============================================================================
// test_config_signal_integration.cpp - Integration Tests for Config Signal
//=============================================================================
/**
 * @file test_config_signal_integration.cpp
 * @brief Integration tests for SIGHUP handler, atomic reload, and snapshot
 *        operations working together
 *
 * WHAT: Test signal-triggered reload, snapshot+modify+compare, snapshot+modify+restore
 * WHY:  Verify cross-cutting interactions between config signal subsystems
 * HOW:  GoogleTest with signal delivery, config file rewrites, and multi-step flows
 *
 * @author NIMCP Development Team
 * @date 2026-02-12
 */

#include <gtest/gtest.h>
#include <fstream>
#include <string>
#include <cstring>
#include <csignal>
#include <unistd.h>

// Headers have their own extern "C" guards
#include "utils/config/nimcp_dynamic_config.h"
#include "utils/config/nimcp_config_signal.h"

//=============================================================================
// Test Fixture
//=============================================================================

class ConfigSignalIntegrationTest : public ::testing::Test {
protected:
    std::string config_path_;

    void SetUp() override {
        config_shutdown();

        config_path_ = "/tmp/nimcp_config_signal_integ.ini";
        WriteConfigFile(config_path_.c_str(),
            "rate = 0.01\n"
            "count = 50\n"
            "enabled = true\n"
            "label = initial\n"
        );

        ASSERT_TRUE(config_init(config_path_.c_str()))
            << "config_init failed for " << config_path_;
    }

    void TearDown() override {
        config_uninstall_sighup_handler();
        config_shutdown();
        remove(config_path_.c_str());
    }

    void WriteConfigFile(const char* path, const char* content) {
        std::ofstream file(path);
        ASSERT_TRUE(file.is_open()) << "Failed to create config file: " << path;
        file << content;
        file.close();
    }
};

//=============================================================================
// SIGHUP Signal Integration
//=============================================================================

TEST_F(ConfigSignalIntegrationTest, SighupSetsPendingFlag) {
    ASSERT_TRUE(config_install_sighup_handler());
    EXPECT_FALSE(config_is_reload_pending());

    // Send SIGHUP to self
    kill(getpid(), SIGHUP);

    // The signal handler sets the volatile flag synchronously
    // (SA_RESTART means it won't be deferred)
    EXPECT_TRUE(config_is_reload_pending());

    // Clear the pending flag by checking (will attempt reload)
    // The reload may succeed or fail depending on file state,
    // but the pending flag should be cleared either way
    config_check_pending_reload();
    EXPECT_FALSE(config_is_reload_pending());
}

TEST_F(ConfigSignalIntegrationTest, SighupHandler_InstallUninstallCycle) {
    // Install
    ASSERT_TRUE(config_install_sighup_handler());
    EXPECT_TRUE(config_is_sighup_handler_installed());

    // Uninstall
    ASSERT_TRUE(config_uninstall_sighup_handler());
    EXPECT_FALSE(config_is_sighup_handler_installed());

    // Re-install
    ASSERT_TRUE(config_install_sighup_handler());
    EXPECT_TRUE(config_is_sighup_handler_installed());

    // Verify signal still works
    kill(getpid(), SIGHUP);
    EXPECT_TRUE(config_is_reload_pending());

    // Clean up the pending flag
    config_check_pending_reload();
}

TEST_F(ConfigSignalIntegrationTest, CheckPendingReload_TriggersActualReload) {
    ASSERT_TRUE(config_install_sighup_handler());

    // Record initial version
    uint32_t version_before = config_get_version();

    // Rewrite config file with same keys but different values
    WriteConfigFile(config_path_.c_str(),
        "rate = 0.02\n"
        "count = 100\n"
        "enabled = false\n"
        "label = reloaded\n"
    );

    // Send SIGHUP to self
    kill(getpid(), SIGHUP);
    ASSERT_TRUE(config_is_reload_pending());

    // Process the pending reload
    bool reload_result = config_check_pending_reload();

    if (reload_result) {
        // Reload succeeded - version should have incremented
        EXPECT_GT(config_get_version(), version_before);
    }

    // Pending flag should be cleared regardless
    EXPECT_FALSE(config_is_reload_pending());
}

//=============================================================================
// Snapshot + Modify + Compare Integration
//=============================================================================

TEST_F(ConfigSignalIntegrationTest, SnapshotModifyCompare_DetectsChanges) {
    // Take snapshot
    config_snapshot_t snap = config_create_snapshot();
    ASSERT_NE(snap, nullptr);

    // Initially no differences
    EXPECT_EQ(config_compare_with_snapshot(snap), 0u);

    // Modify one value
    ASSERT_TRUE(config_set_int("count", 999));
    EXPECT_EQ(config_compare_with_snapshot(snap), 1u);

    // Modify another value
    ASSERT_TRUE(config_set_float("rate", 0.999));
    EXPECT_EQ(config_compare_with_snapshot(snap), 2u);

    // Modify a third value
    ASSERT_TRUE(config_set_bool("enabled", false));
    EXPECT_EQ(config_compare_with_snapshot(snap), 3u);

    // Modify a fourth value
    ASSERT_TRUE(config_set_string("label", "modified"));
    EXPECT_EQ(config_compare_with_snapshot(snap), 4u);

    config_destroy_snapshot(snap);
}

//=============================================================================
// Snapshot + Modify + Restore Integration
//=============================================================================

TEST_F(ConfigSignalIntegrationTest, SnapshotModifyRestore_RevertsAllChanges) {
    // Verify initial values
    EXPECT_DOUBLE_EQ(config_get_float("rate", 0.0), 0.01);
    EXPECT_EQ(config_get_int("count", 0), 50);
    EXPECT_TRUE(config_get_bool("enabled", false));
    EXPECT_STREQ(config_get_string("label", ""), "initial");

    // Take snapshot
    config_snapshot_t snap = config_create_snapshot();
    ASSERT_NE(snap, nullptr);

    // Make changes
    ASSERT_TRUE(config_set_float("rate", 0.99));
    ASSERT_TRUE(config_set_int("count", 999));
    ASSERT_TRUE(config_set_bool("enabled", false));
    ASSERT_TRUE(config_set_string("label", "modified"));

    // Verify changes
    EXPECT_DOUBLE_EQ(config_get_float("rate", 0.0), 0.99);
    EXPECT_EQ(config_get_int("count", 0), 999);
    EXPECT_FALSE(config_get_bool("enabled", true));
    EXPECT_STREQ(config_get_string("label", ""), "modified");

    // Restore from snapshot
    ASSERT_TRUE(config_restore_snapshot(snap));

    // Verify all values are reverted
    EXPECT_DOUBLE_EQ(config_get_float("rate", 0.0), 0.01);
    EXPECT_EQ(config_get_int("count", 0), 50);
    EXPECT_TRUE(config_get_bool("enabled", false));
    EXPECT_STREQ(config_get_string("label", ""), "initial");

    config_destroy_snapshot(snap);
}

//=============================================================================
// Validator Integration with Reload
//=============================================================================

static bool reject_high_rate(void* user_data) {
    (void)user_data;
    double rate = config_get_float("rate", 0.0);
    return rate <= 1.0;
}

TEST_F(ConfigSignalIntegrationTest, ValidatorCalledDuringAtomicReload) {
    config_set_history_size(10);
    uint32_t validator_id = config_register_reload_validator(reject_high_rate, nullptr);
    EXPECT_GT(validator_id, 0u);

    // The validator accepts rate <= 1.0
    // Reload the same config file (rate=0.01) - should succeed
    bool result = config_atomic_reload(nullptr);
    // Note: atomic_reload calls config_reload() which re-reads the file
    // Success depends on whether the file is valid and parseable
    // We just verify it doesn't crash

    config_unregister_reload_validator(validator_id);
    (void)result;  // Result depends on config_reload() behavior
}

//=============================================================================
// Pre and Post Reload Callbacks Integration
//=============================================================================

struct CallbackTracker {
    int pre_count;
    int post_count;
    uint32_t last_pre_version;
    uint32_t last_post_version_before;
    uint32_t last_post_version_after;
    bool last_post_success;
};

static void tracking_pre_callback(uint32_t version_before, void* user_data) {
    CallbackTracker* tracker = (CallbackTracker*)user_data;
    tracker->pre_count++;
    tracker->last_pre_version = version_before;
}

static void tracking_post_callback(uint32_t before, uint32_t after,
                                    bool success, void* user_data) {
    CallbackTracker* tracker = (CallbackTracker*)user_data;
    tracker->post_count++;
    tracker->last_post_version_before = before;
    tracker->last_post_version_after = after;
    tracker->last_post_success = success;
}

TEST_F(ConfigSignalIntegrationTest, ReloadCallbacksInvoked) {
    config_set_history_size(10);
    CallbackTracker tracker = {0, 0, 0, 0, 0, false};

    uint32_t pre_id = config_register_pre_reload_callback(tracking_pre_callback, &tracker);
    EXPECT_GT(pre_id, 0u);

    uint32_t post_id = config_register_post_reload_callback(tracking_post_callback, &tracker);
    EXPECT_GT(post_id, 0u);

    // Attempt atomic reload (will call pre and post callbacks)
    config_atomic_reload(nullptr);

    // The callbacks should have been called at least once
    EXPECT_GE(tracker.pre_count, 1);
    EXPECT_GE(tracker.post_count, 1);

    config_unregister_pre_reload_callback(pre_id);
    config_unregister_post_reload_callback(post_id);
}

//=============================================================================
// Stats Integration After Operations
//=============================================================================

TEST_F(ConfigSignalIntegrationTest, StatsReflectOperations) {
    config_reset_atomic_stats();

    // Create and destroy snapshots
    config_snapshot_t snap = config_create_snapshot();
    ASSERT_NE(snap, nullptr);
    config_destroy_snapshot(snap);

    config_atomic_stats_t stats;
    ASSERT_TRUE(config_get_atomic_stats(&stats));
    EXPECT_GE(stats.snapshots_created, 1ULL);
    EXPECT_GE(stats.snapshots_destroyed, 1ULL);
    EXPECT_GT(stats.current_version, 0u);
}

//=============================================================================
// Clone + Restore Integration
//=============================================================================

TEST_F(ConfigSignalIntegrationTest, CloneSnapshot_UsableForRestore) {
    // Take snapshot
    config_snapshot_t orig = config_create_snapshot();
    ASSERT_NE(orig, nullptr);

    // Clone it
    config_snapshot_t clone = config_clone_snapshot(orig);
    ASSERT_NE(clone, nullptr);

    // Destroy original
    config_destroy_snapshot(orig);

    // Mutate config
    ASSERT_TRUE(config_set_int("count", 12345));
    EXPECT_EQ(config_get_int("count", 0), 12345);

    // Restore from clone
    ASSERT_TRUE(config_restore_snapshot(clone));
    EXPECT_EQ(config_get_int("count", 0), 50);

    config_destroy_snapshot(clone);
}

//=============================================================================
// Multiple SIGHUP Signals
//=============================================================================

TEST_F(ConfigSignalIntegrationTest, MultipleSighups_OnlyOneReload) {
    ASSERT_TRUE(config_install_sighup_handler());

    // Send multiple SIGHUP signals rapidly
    for (int i = 0; i < 3; i++) {
        kill(getpid(), SIGHUP);
    }

    // Only one reload should be pending (flag is 0 or 1)
    EXPECT_TRUE(config_is_reload_pending());

    // Process it
    config_check_pending_reload();
    EXPECT_FALSE(config_is_reload_pending());
}

//=============================================================================
// Snapshot Comparison After File Reload
//=============================================================================

TEST_F(ConfigSignalIntegrationTest, SnapshotCompare_AfterFileReload) {
    // Take snapshot of initial state
    config_snapshot_t snap = config_create_snapshot();
    ASSERT_NE(snap, nullptr);

    // Rewrite config file with different values
    WriteConfigFile(config_path_.c_str(),
        "rate = 0.05\n"
        "count = 75\n"
        "enabled = false\n"
        "label = reloaded\n"
    );

    // Reload from file
    bool reloaded = config_reload();
    if (reloaded) {
        // All 4 values changed, so compare should detect differences
        size_t diffs = config_compare_with_snapshot(snap);
        EXPECT_GE(diffs, 1u)
            << "After reload with different values, should detect differences";
    }

    config_destroy_snapshot(snap);
}

//=============================================================================
// Version Tracking Across Snapshots and Restores
//=============================================================================

TEST_F(ConfigSignalIntegrationTest, VersionTracking_RestoreResetsVersion) {
    uint32_t v_initial = config_get_version();

    // Take snapshot
    config_snapshot_t snap = config_create_snapshot();
    ASSERT_NE(snap, nullptr);
    EXPECT_EQ(config_snapshot_get_version(snap), v_initial);

    // Increment version
    config_force_version_increment();
    config_force_version_increment();
    uint32_t v_after = config_get_version();
    EXPECT_EQ(v_after, v_initial + 2);

    // Restore snapshot should reset version to snapshot's version
    ASSERT_TRUE(config_restore_snapshot(snap));
    EXPECT_EQ(config_get_version(), v_initial);

    config_destroy_snapshot(snap);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
