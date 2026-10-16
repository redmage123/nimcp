//=============================================================================
// test_config_signal.cpp - Unit Tests for Config Signal Module
//=============================================================================
/**
 * @file test_config_signal.cpp
 * @brief Unit tests for atomic config reload with snapshot/rollback
 *
 * WHAT: Test snapshot creation, restore, compare, version management, callbacks
 * WHY:  Zero tests existed for nimcp_config_signal.c - full coverage needed
 * HOW:  GoogleTest with config_init/shutdown lifecycle per test
 *
 * TESTED FUNCTIONS:
 * - config_create_snapshot / config_destroy_snapshot
 * - config_restore_snapshot
 * - config_compare_with_snapshot
 * - config_snapshot_get_version / config_snapshot_get_timestamp
 * - config_clone_snapshot
 * - config_is_reload_pending / config_check_pending_reload
 * - config_force_version_increment / config_get_version
 * - config_get_atomic_stats / config_reset_atomic_stats
 * - config_set_history_size / config_get_history_size / config_clear_history
 * - config_register_reload_validator / config_unregister_reload_validator
 * - config_register_pre_reload_callback / config_unregister_pre_reload_callback
 * - config_register_post_reload_callback / config_unregister_post_reload_callback
 * - config_install_sighup_handler / config_uninstall_sighup_handler
 * - config_is_sighup_handler_installed
 * - Error handling for NULL parameters
 *
 * @author NIMCP Development Team
 * @date 2026-02-12
 */

#include <gtest/gtest.h>
#include <fstream>
#include <string>
#include <cstdio>
#include <cstring>

// Headers have their own extern "C" guards
#include "utils/config/nimcp_dynamic_config.h"
#include "utils/config/nimcp_config_signal.h"

//=============================================================================
// Test Fixture
//=============================================================================

class ConfigSignalTest : public ::testing::Test {
protected:
    std::string config_path_;

    void SetUp() override {
        // Clean any prior state
        config_shutdown();

        // Create a config file with various types
        config_path_ = "/tmp/nimcp_config_signal_test.ini";
        WriteConfigFile(config_path_.c_str(),
            "learning_rate = 0.001\n"
            "batch_size = 32\n"
            "enable_cache = true\n"
            "model_name = test_model\n"
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
// Snapshot Creation Tests
//=============================================================================

TEST_F(ConfigSignalTest, CreateSnapshot_ReturnsNonNull) {
    config_snapshot_t snap = config_create_snapshot();
    ASSERT_NE(snap, nullptr);
    config_destroy_snapshot(snap);
}

TEST_F(ConfigSignalTest, CreateSnapshot_CapturesCurrentVersion) {
    uint32_t version_before = config_get_version();
    config_snapshot_t snap = config_create_snapshot();
    ASSERT_NE(snap, nullptr);

    EXPECT_EQ(config_snapshot_get_version(snap), version_before);
    config_destroy_snapshot(snap);
}

TEST_F(ConfigSignalTest, CreateSnapshot_HasTimestamp) {
    config_snapshot_t snap = config_create_snapshot();
    ASSERT_NE(snap, nullptr);

    uint64_t ts = config_snapshot_get_timestamp(snap);
    EXPECT_GT(ts, 0ULL) << "Snapshot timestamp should be non-zero";
    config_destroy_snapshot(snap);
}

TEST_F(ConfigSignalTest, CreateSnapshot_StatsIncremented) {
    config_reset_atomic_stats();

    config_snapshot_t snap = config_create_snapshot();
    ASSERT_NE(snap, nullptr);

    config_atomic_stats_t stats;
    ASSERT_TRUE(config_get_atomic_stats(&stats));
    EXPECT_GE(stats.snapshots_created, 1ULL);

    config_destroy_snapshot(snap);
}

//=============================================================================
// Snapshot Destroy Tests
//=============================================================================

TEST_F(ConfigSignalTest, DestroySnapshot_NullIsSafe) {
    // Should not crash
    config_destroy_snapshot(nullptr);
}

TEST_F(ConfigSignalTest, DestroySnapshot_StatsIncremented) {
    config_reset_atomic_stats();

    config_snapshot_t snap = config_create_snapshot();
    ASSERT_NE(snap, nullptr);
    config_destroy_snapshot(snap);

    config_atomic_stats_t stats;
    ASSERT_TRUE(config_get_atomic_stats(&stats));
    EXPECT_GE(stats.snapshots_destroyed, 1ULL);
}

//=============================================================================
// Snapshot Restore Tests
//=============================================================================

TEST_F(ConfigSignalTest, RestoreSnapshot_RevertsMutatedInt) {
    // Take snapshot of initial state
    config_snapshot_t snap = config_create_snapshot();
    ASSERT_NE(snap, nullptr);

    // Mutate config
    ASSERT_TRUE(config_set_int("batch_size", 64));
    EXPECT_EQ(config_get_int("batch_size", 0), 64);

    // Restore snapshot
    ASSERT_TRUE(config_restore_snapshot(snap));
    EXPECT_EQ(config_get_int("batch_size", 0), 32);

    config_destroy_snapshot(snap);
}

TEST_F(ConfigSignalTest, RestoreSnapshot_RevertsMutatedFloat) {
    config_snapshot_t snap = config_create_snapshot();
    ASSERT_NE(snap, nullptr);

    ASSERT_TRUE(config_set_float("learning_rate", 0.1));
    EXPECT_DOUBLE_EQ(config_get_float("learning_rate", 0.0), 0.1);

    ASSERT_TRUE(config_restore_snapshot(snap));
    EXPECT_DOUBLE_EQ(config_get_float("learning_rate", 0.0), 0.001);

    config_destroy_snapshot(snap);
}

TEST_F(ConfigSignalTest, RestoreSnapshot_RevertsMutatedBool) {
    config_snapshot_t snap = config_create_snapshot();
    ASSERT_NE(snap, nullptr);

    ASSERT_TRUE(config_set_bool("enable_cache", false));
    EXPECT_FALSE(config_get_bool("enable_cache", true));

    ASSERT_TRUE(config_restore_snapshot(snap));
    EXPECT_TRUE(config_get_bool("enable_cache", false));

    config_destroy_snapshot(snap);
}

TEST_F(ConfigSignalTest, RestoreSnapshot_RevertsMutatedString) {
    config_snapshot_t snap = config_create_snapshot();
    ASSERT_NE(snap, nullptr);

    ASSERT_TRUE(config_set_string("model_name", "changed_model"));
    EXPECT_STREQ(config_get_string("model_name", ""), "changed_model");

    ASSERT_TRUE(config_restore_snapshot(snap));
    EXPECT_STREQ(config_get_string("model_name", ""), "test_model");

    config_destroy_snapshot(snap);
}

TEST_F(ConfigSignalTest, RestoreSnapshot_NullReturnsFalse) {
    EXPECT_FALSE(config_restore_snapshot(nullptr));
}

//=============================================================================
// Snapshot Compare Tests
//=============================================================================

TEST_F(ConfigSignalTest, CompareWithSnapshot_NoDifferencesWhenUnchanged) {
    config_snapshot_t snap = config_create_snapshot();
    ASSERT_NE(snap, nullptr);

    size_t diffs = config_compare_with_snapshot(snap);
    EXPECT_EQ(diffs, 0u) << "No changes made, should be zero differences";

    config_destroy_snapshot(snap);
}

TEST_F(ConfigSignalTest, CompareWithSnapshot_DetectsIntChange) {
    config_snapshot_t snap = config_create_snapshot();
    ASSERT_NE(snap, nullptr);

    ASSERT_TRUE(config_set_int("batch_size", 128));

    size_t diffs = config_compare_with_snapshot(snap);
    EXPECT_GE(diffs, 1u) << "Should detect at least one difference";

    config_destroy_snapshot(snap);
}

TEST_F(ConfigSignalTest, CompareWithSnapshot_DetectsFloatChange) {
    config_snapshot_t snap = config_create_snapshot();
    ASSERT_NE(snap, nullptr);

    ASSERT_TRUE(config_set_float("learning_rate", 0.5));

    size_t diffs = config_compare_with_snapshot(snap);
    EXPECT_GE(diffs, 1u);

    config_destroy_snapshot(snap);
}

TEST_F(ConfigSignalTest, CompareWithSnapshot_DetectsBoolChange) {
    config_snapshot_t snap = config_create_snapshot();
    ASSERT_NE(snap, nullptr);

    ASSERT_TRUE(config_set_bool("enable_cache", false));

    size_t diffs = config_compare_with_snapshot(snap);
    EXPECT_GE(diffs, 1u);

    config_destroy_snapshot(snap);
}

TEST_F(ConfigSignalTest, CompareWithSnapshot_DetectsStringChange) {
    config_snapshot_t snap = config_create_snapshot();
    ASSERT_NE(snap, nullptr);

    ASSERT_TRUE(config_set_string("model_name", "different_model"));

    size_t diffs = config_compare_with_snapshot(snap);
    EXPECT_GE(diffs, 1u);

    config_destroy_snapshot(snap);
}

TEST_F(ConfigSignalTest, CompareWithSnapshot_DetectsMultipleChanges) {
    config_snapshot_t snap = config_create_snapshot();
    ASSERT_NE(snap, nullptr);

    ASSERT_TRUE(config_set_int("batch_size", 128));
    ASSERT_TRUE(config_set_float("learning_rate", 0.5));
    ASSERT_TRUE(config_set_bool("enable_cache", false));

    size_t diffs = config_compare_with_snapshot(snap);
    EXPECT_GE(diffs, 3u) << "Should detect at least 3 differences";

    config_destroy_snapshot(snap);
}

TEST_F(ConfigSignalTest, CompareWithSnapshot_NullReturnsZero) {
    EXPECT_EQ(config_compare_with_snapshot(nullptr), 0u);
}

//=============================================================================
// Clone Snapshot Tests
//=============================================================================

TEST_F(ConfigSignalTest, CloneSnapshot_ProducesValidCopy) {
    config_snapshot_t orig = config_create_snapshot();
    ASSERT_NE(orig, nullptr);

    config_snapshot_t clone = config_clone_snapshot(orig);
    ASSERT_NE(clone, nullptr);

    EXPECT_EQ(config_snapshot_get_version(clone),
              config_snapshot_get_version(orig));
    EXPECT_EQ(config_snapshot_get_timestamp(clone),
              config_snapshot_get_timestamp(orig));

    config_destroy_snapshot(clone);
    config_destroy_snapshot(orig);
}

TEST_F(ConfigSignalTest, CloneSnapshot_NullReturnsNull) {
    EXPECT_EQ(config_clone_snapshot(nullptr), nullptr);
}

TEST_F(ConfigSignalTest, CloneSnapshot_IndependentOfOriginal) {
    config_snapshot_t orig = config_create_snapshot();
    ASSERT_NE(orig, nullptr);

    config_snapshot_t clone = config_clone_snapshot(orig);
    ASSERT_NE(clone, nullptr);

    // Destroy original -- clone should still be valid
    config_destroy_snapshot(orig);

    // Clone should still work for restore
    ASSERT_TRUE(config_set_int("batch_size", 999));
    ASSERT_TRUE(config_restore_snapshot(clone));
    EXPECT_EQ(config_get_int("batch_size", 0), 32);

    config_destroy_snapshot(clone);
}

//=============================================================================
// Version Management Tests
//=============================================================================

TEST_F(ConfigSignalTest, GetVersion_ReturnsNonZero) {
    uint32_t ver = config_get_version();
    EXPECT_GT(ver, 0u);
}

TEST_F(ConfigSignalTest, ForceVersionIncrement_IncreasesVersion) {
    uint32_t before = config_get_version();
    uint32_t after = config_force_version_increment();
    EXPECT_EQ(after, before + 1);
    EXPECT_EQ(config_get_version(), after);
}

TEST_F(ConfigSignalTest, ForceVersionIncrement_MultipleIncrements) {
    uint32_t v0 = config_get_version();
    config_force_version_increment();
    config_force_version_increment();
    uint32_t v2 = config_force_version_increment();
    EXPECT_EQ(v2, v0 + 3);
}

//=============================================================================
// History Configuration Tests
//=============================================================================

TEST_F(ConfigSignalTest, GetHistorySize_ReturnsDefault) {
    uint32_t size = config_get_history_size();
    EXPECT_EQ(size, CONFIG_DEFAULT_HISTORY_SIZE);
}

TEST_F(ConfigSignalTest, SetHistorySize_ValidValue) {
    config_set_history_size(20);
    EXPECT_EQ(config_get_history_size(), 20u);
}

TEST_F(ConfigSignalTest, SetHistorySize_MaxBoundary) {
    config_set_history_size(CONFIG_MAX_HISTORY_SIZE);
    EXPECT_EQ(config_get_history_size(), (uint32_t)CONFIG_MAX_HISTORY_SIZE);
}

TEST_F(ConfigSignalTest, SetHistorySize_MinBoundary) {
    config_set_history_size(1);
    EXPECT_EQ(config_get_history_size(), 1u);
}

TEST_F(ConfigSignalTest, SetHistorySize_ZeroIgnored) {
    uint32_t before = config_get_history_size();
    config_set_history_size(0);
    EXPECT_EQ(config_get_history_size(), before);
}

TEST_F(ConfigSignalTest, SetHistorySize_OverMaxIgnored) {
    uint32_t before = config_get_history_size();
    config_set_history_size(CONFIG_MAX_HISTORY_SIZE + 1);
    EXPECT_EQ(config_get_history_size(), before);
}

TEST_F(ConfigSignalTest, ClearHistory_DoesNotCrash) {
    // Set up some history via version increments
    config_set_history_size(10);
    config_clear_history();
    // Should not crash and history count should be zero
    uint32_t versions[10];
    size_t count = config_get_version_history(versions, 10);
    EXPECT_EQ(count, 0u);
}

//=============================================================================
// Version History Tests
//=============================================================================

TEST_F(ConfigSignalTest, GetVersionHistory_EmptyInitially) {
    uint32_t versions[10];
    size_t count = config_get_version_history(versions, 10);
    EXPECT_EQ(count, 0u);
}

TEST_F(ConfigSignalTest, GetVersionHistory_NullReturnZero) {
    EXPECT_EQ(config_get_version_history(nullptr, 10), 0u);
}

TEST_F(ConfigSignalTest, GetVersionHistory_ZeroMaxReturnZero) {
    uint32_t versions[10];
    EXPECT_EQ(config_get_version_history(versions, 0), 0u);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(ConfigSignalTest, GetAtomicStats_ReturnsTrue) {
    config_atomic_stats_t stats;
    ASSERT_TRUE(config_get_atomic_stats(&stats));
    EXPECT_GT(stats.current_version, 0u);
}

TEST_F(ConfigSignalTest, GetAtomicStats_NullReturnsFalse) {
    EXPECT_FALSE(config_get_atomic_stats(nullptr));
}

TEST_F(ConfigSignalTest, ResetAtomicStats_ClearsCounters) {
    // Create a snapshot to bump the counter
    config_snapshot_t snap = config_create_snapshot();
    ASSERT_NE(snap, nullptr);
    config_destroy_snapshot(snap);

    config_reset_atomic_stats();

    config_atomic_stats_t stats;
    ASSERT_TRUE(config_get_atomic_stats(&stats));
    EXPECT_EQ(stats.atomic_reloads, 0ULL);
    EXPECT_EQ(stats.atomic_reload_failures, 0ULL);
    EXPECT_EQ(stats.rollbacks, 0ULL);
    EXPECT_EQ(stats.rollback_failures, 0ULL);
    EXPECT_EQ(stats.snapshots_created, 0ULL);
    EXPECT_EQ(stats.snapshots_destroyed, 0ULL);
    EXPECT_EQ(stats.validation_failures, 0ULL);
}

//=============================================================================
// Validator Registration Tests
//=============================================================================

static bool always_true_validator(void* user_data) {
    (void)user_data;
    return true;
}

static bool always_false_validator(void* user_data) {
    (void)user_data;
    return false;
}

TEST_F(ConfigSignalTest, RegisterValidator_ReturnsNonZeroId) {
    uint32_t id = config_register_reload_validator(always_true_validator, nullptr);
    EXPECT_GT(id, 0u);
    config_unregister_reload_validator(id);
}

TEST_F(ConfigSignalTest, RegisterValidator_NullReturnsZero) {
    uint32_t id = config_register_reload_validator(nullptr, nullptr);
    EXPECT_EQ(id, 0u);
}

TEST_F(ConfigSignalTest, UnregisterValidator_DoesNotCrash) {
    uint32_t id = config_register_reload_validator(always_true_validator, nullptr);
    EXPECT_GT(id, 0u);
    config_unregister_reload_validator(id);
    // Unregistering again should be harmless
    config_unregister_reload_validator(id);
}

//=============================================================================
// Pre-Reload Callback Tests
//=============================================================================

static void pre_reload_noop(uint32_t version_before, void* user_data) {
    (void)version_before;
    (void)user_data;
}

TEST_F(ConfigSignalTest, RegisterPreReloadCallback_ReturnsNonZeroId) {
    uint32_t id = config_register_pre_reload_callback(pre_reload_noop, nullptr);
    EXPECT_GT(id, 0u);
    config_unregister_pre_reload_callback(id);
}

TEST_F(ConfigSignalTest, RegisterPreReloadCallback_NullReturnsZero) {
    uint32_t id = config_register_pre_reload_callback(nullptr, nullptr);
    EXPECT_EQ(id, 0u);
}

//=============================================================================
// Post-Reload Callback Tests
//=============================================================================

static void post_reload_noop(uint32_t before, uint32_t after, bool success,
                              void* user_data) {
    (void)before;
    (void)after;
    (void)success;
    (void)user_data;
}

TEST_F(ConfigSignalTest, RegisterPostReloadCallback_ReturnsNonZeroId) {
    uint32_t id = config_register_post_reload_callback(post_reload_noop, nullptr);
    EXPECT_GT(id, 0u);
    config_unregister_post_reload_callback(id);
}

TEST_F(ConfigSignalTest, RegisterPostReloadCallback_NullReturnsZero) {
    uint32_t id = config_register_post_reload_callback(nullptr, nullptr);
    EXPECT_EQ(id, 0u);
}

//=============================================================================
// SIGHUP Handler Tests
//=============================================================================

TEST_F(ConfigSignalTest, InstallSighupHandler_ReturnsTrue) {
    EXPECT_TRUE(config_install_sighup_handler());
    EXPECT_TRUE(config_is_sighup_handler_installed());
}

TEST_F(ConfigSignalTest, InstallSighupHandler_DoubleInstallStillTrue) {
    EXPECT_TRUE(config_install_sighup_handler());
    EXPECT_TRUE(config_install_sighup_handler());
    EXPECT_TRUE(config_is_sighup_handler_installed());
}

TEST_F(ConfigSignalTest, UninstallSighupHandler_ReturnsTrue) {
    ASSERT_TRUE(config_install_sighup_handler());
    EXPECT_TRUE(config_uninstall_sighup_handler());
    EXPECT_FALSE(config_is_sighup_handler_installed());
}

TEST_F(ConfigSignalTest, UninstallSighupHandler_WhenNotInstalled) {
    EXPECT_TRUE(config_uninstall_sighup_handler());
}

TEST_F(ConfigSignalTest, IsSighupHandlerInstalled_FalseInitially) {
    EXPECT_FALSE(config_is_sighup_handler_installed());
}

//=============================================================================
// Snapshot with Various Config Types (Roundtrip)
//=============================================================================

TEST_F(ConfigSignalTest, SnapshotRoundtrip_AllTypes) {
    // Verify initial values
    EXPECT_DOUBLE_EQ(config_get_float("learning_rate", 0.0), 0.001);
    EXPECT_EQ(config_get_int("batch_size", 0), 32);
    EXPECT_TRUE(config_get_bool("enable_cache", false));
    EXPECT_STREQ(config_get_string("model_name", ""), "test_model");

    // Take snapshot
    config_snapshot_t snap = config_create_snapshot();
    ASSERT_NE(snap, nullptr);

    // Mutate all types
    ASSERT_TRUE(config_set_float("learning_rate", 0.999));
    ASSERT_TRUE(config_set_int("batch_size", 256));
    ASSERT_TRUE(config_set_bool("enable_cache", false));
    ASSERT_TRUE(config_set_string("model_name", "mutated"));

    // Verify mutations took effect
    EXPECT_DOUBLE_EQ(config_get_float("learning_rate", 0.0), 0.999);
    EXPECT_EQ(config_get_int("batch_size", 0), 256);
    EXPECT_FALSE(config_get_bool("enable_cache", true));
    EXPECT_STREQ(config_get_string("model_name", ""), "mutated");

    // Restore snapshot
    ASSERT_TRUE(config_restore_snapshot(snap));

    // Verify all original values are restored
    EXPECT_DOUBLE_EQ(config_get_float("learning_rate", 0.0), 0.001);
    EXPECT_EQ(config_get_int("batch_size", 0), 32);
    EXPECT_TRUE(config_get_bool("enable_cache", false));
    EXPECT_STREQ(config_get_string("model_name", ""), "test_model");

    config_destroy_snapshot(snap);
}

//=============================================================================
// Dump Version History (smoke test)
//=============================================================================

TEST_F(ConfigSignalTest, DumpVersionHistory_DoesNotCrash) {
    config_dump_version_history();
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
