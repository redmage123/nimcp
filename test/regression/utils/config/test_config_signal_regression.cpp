//=============================================================================
// test_config_signal_regression.cpp - Regression Tests for Config Signal Module
//=============================================================================
/**
 * @file test_config_signal_regression.cpp
 * @brief Regression tests for snapshot/restore roundtrip, multiple snapshots,
 *        history management, and deadlock prevention
 *
 * WHAT: Validate config signal correctness across complex usage patterns
 * WHY:  Prevent regressions in snapshot, restore, compare, and history operations
 * HOW:  GoogleTest with multi-step operations and cross-verification
 *
 * @author NIMCP Development Team
 * @date 2026-02-12
 */

#include <gtest/gtest.h>
#include <fstream>
#include <string>
#include <cstring>

// Headers have their own extern "C" guards
#include "utils/config/nimcp_dynamic_config.h"
#include "utils/config/nimcp_config_signal.h"

//=============================================================================
// Test Fixture
//=============================================================================

class ConfigSignalRegressionTest : public ::testing::Test {
protected:
    std::string config_path_;

    void SetUp() override {
        config_shutdown();

        config_path_ = "/tmp/nimcp_config_signal_regr.ini";
        WriteConfigFile(config_path_.c_str(),
            "alpha = 0.01\n"
            "beta = 100\n"
            "gamma = true\n"
            "delta = hello_world\n"
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
// Snapshot/Restore Roundtrip Preserves All Value Types
//=============================================================================

TEST_F(ConfigSignalRegressionTest, Roundtrip_PreservesFloat) {
    config_snapshot_t snap = config_create_snapshot();
    ASSERT_NE(snap, nullptr);

    ASSERT_TRUE(config_set_float("alpha", 99.99));
    ASSERT_TRUE(config_restore_snapshot(snap));
    EXPECT_DOUBLE_EQ(config_get_float("alpha", 0.0), 0.01);

    config_destroy_snapshot(snap);
}

TEST_F(ConfigSignalRegressionTest, Roundtrip_PreservesInt) {
    config_snapshot_t snap = config_create_snapshot();
    ASSERT_NE(snap, nullptr);

    ASSERT_TRUE(config_set_int("beta", 999));
    ASSERT_TRUE(config_restore_snapshot(snap));
    EXPECT_EQ(config_get_int("beta", 0), 100);

    config_destroy_snapshot(snap);
}

TEST_F(ConfigSignalRegressionTest, Roundtrip_PreservesBool) {
    config_snapshot_t snap = config_create_snapshot();
    ASSERT_NE(snap, nullptr);

    ASSERT_TRUE(config_set_bool("gamma", false));
    ASSERT_TRUE(config_restore_snapshot(snap));
    EXPECT_TRUE(config_get_bool("gamma", false));

    config_destroy_snapshot(snap);
}

TEST_F(ConfigSignalRegressionTest, Roundtrip_PreservesString) {
    config_snapshot_t snap = config_create_snapshot();
    ASSERT_NE(snap, nullptr);

    ASSERT_TRUE(config_set_string("delta", "changed"));
    ASSERT_TRUE(config_restore_snapshot(snap));
    EXPECT_STREQ(config_get_string("delta", ""), "hello_world");

    config_destroy_snapshot(snap);
}

//=============================================================================
// Multiple Snapshots Don't Interfere
//=============================================================================

TEST_F(ConfigSignalRegressionTest, MultipleSnapshots_Independent) {
    // Snapshot 1: original state
    config_snapshot_t snap1 = config_create_snapshot();
    ASSERT_NE(snap1, nullptr);

    // Mutate and take snapshot 2
    ASSERT_TRUE(config_set_int("beta", 200));
    config_snapshot_t snap2 = config_create_snapshot();
    ASSERT_NE(snap2, nullptr);

    // Mutate again
    ASSERT_TRUE(config_set_int("beta", 300));

    // Restore snap2 -> should get 200
    ASSERT_TRUE(config_restore_snapshot(snap2));
    EXPECT_EQ(config_get_int("beta", 0), 200);

    // Restore snap1 -> should get original 100
    ASSERT_TRUE(config_restore_snapshot(snap1));
    EXPECT_EQ(config_get_int("beta", 0), 100);

    config_destroy_snapshot(snap1);
    config_destroy_snapshot(snap2);
}

TEST_F(ConfigSignalRegressionTest, MultipleSnapshots_DifferentVersions) {
    config_snapshot_t snap1 = config_create_snapshot();
    ASSERT_NE(snap1, nullptr);

    config_force_version_increment();

    config_snapshot_t snap2 = config_create_snapshot();
    ASSERT_NE(snap2, nullptr);

    EXPECT_NE(config_snapshot_get_version(snap1),
              config_snapshot_get_version(snap2));

    config_destroy_snapshot(snap1);
    config_destroy_snapshot(snap2);
}

//=============================================================================
// Snapshot After Config Changes Captures New Values
//=============================================================================

TEST_F(ConfigSignalRegressionTest, SnapshotAfterChange_CapturesNewValues) {
    // Mutate config
    ASSERT_TRUE(config_set_int("beta", 777));
    ASSERT_TRUE(config_set_float("alpha", 3.14));

    // Take snapshot of mutated state
    config_snapshot_t snap = config_create_snapshot();
    ASSERT_NE(snap, nullptr);

    // Mutate further
    ASSERT_TRUE(config_set_int("beta", 888));
    ASSERT_TRUE(config_set_float("alpha", 2.71));

    // Restore to the snapshot (should get 777 and 3.14)
    ASSERT_TRUE(config_restore_snapshot(snap));
    EXPECT_EQ(config_get_int("beta", 0), 777);
    EXPECT_DOUBLE_EQ(config_get_float("alpha", 0.0), 3.14);

    config_destroy_snapshot(snap);
}

//=============================================================================
// Compare After Restore Shows Zero Differences
//=============================================================================

TEST_F(ConfigSignalRegressionTest, CompareAfterRestore_ZeroDifferences) {
    config_snapshot_t snap = config_create_snapshot();
    ASSERT_NE(snap, nullptr);

    // Mutate
    ASSERT_TRUE(config_set_int("beta", 999));
    size_t diffs_before_restore = config_compare_with_snapshot(snap);
    EXPECT_GE(diffs_before_restore, 1u);

    // Restore
    ASSERT_TRUE(config_restore_snapshot(snap));
    size_t diffs_after_restore = config_compare_with_snapshot(snap);
    EXPECT_EQ(diffs_after_restore, 0u);

    config_destroy_snapshot(snap);
}

//=============================================================================
// Snapshot + Modify + Compare Detects Exact Change Count
//=============================================================================

TEST_F(ConfigSignalRegressionTest, Compare_DetectsExactChangeCount) {
    config_snapshot_t snap = config_create_snapshot();
    ASSERT_NE(snap, nullptr);

    // Change one value
    ASSERT_TRUE(config_set_int("beta", 42));
    EXPECT_EQ(config_compare_with_snapshot(snap), 1u);

    // Change a second value
    ASSERT_TRUE(config_set_float("alpha", 9.99));
    EXPECT_EQ(config_compare_with_snapshot(snap), 2u);

    // Change a third value
    ASSERT_TRUE(config_set_bool("gamma", false));
    EXPECT_EQ(config_compare_with_snapshot(snap), 3u);

    // Change a fourth value
    ASSERT_TRUE(config_set_string("delta", "goodbye"));
    EXPECT_EQ(config_compare_with_snapshot(snap), 4u);

    config_destroy_snapshot(snap);
}

//=============================================================================
// History Management Regression
//=============================================================================

TEST_F(ConfigSignalRegressionTest, HistorySize_CanBeChanged) {
    config_set_history_size(5);
    EXPECT_EQ(config_get_history_size(), 5u);

    config_set_history_size(50);
    EXPECT_EQ(config_get_history_size(), 50u);
}

TEST_F(ConfigSignalRegressionTest, ClearHistory_ThenGetVersionHistory) {
    config_set_history_size(10);
    config_clear_history();

    uint32_t versions[10];
    size_t count = config_get_version_history(versions, 10);
    EXPECT_EQ(count, 0u);
}

//=============================================================================
// Stats Consistency
//=============================================================================

TEST_F(ConfigSignalRegressionTest, Stats_SnapshotCountsAccurate) {
    config_reset_atomic_stats();

    config_snapshot_t s1 = config_create_snapshot();
    config_snapshot_t s2 = config_create_snapshot();
    config_snapshot_t s3 = config_create_snapshot();
    ASSERT_NE(s1, nullptr);
    ASSERT_NE(s2, nullptr);
    ASSERT_NE(s3, nullptr);

    config_atomic_stats_t stats;
    ASSERT_TRUE(config_get_atomic_stats(&stats));
    EXPECT_GE(stats.snapshots_created, 3ULL);

    config_destroy_snapshot(s1);
    config_destroy_snapshot(s2);
    config_destroy_snapshot(s3);

    ASSERT_TRUE(config_get_atomic_stats(&stats));
    EXPECT_GE(stats.snapshots_destroyed, 3ULL);
}

//=============================================================================
// Deadlock Prevention: Snapshot + Restore Within Same Lock Context
//=============================================================================

TEST_F(ConfigSignalRegressionTest, DeadlockPrevention_CreateRestoreDestroy) {
    // This tests that create -> modify -> restore -> destroy completes
    // without deadlocking. The implementation acquires g_atomic_lock in
    // both create_snapshot and restore_snapshot, so this verifies
    // that the lock protocol is correct.

    for (int i = 0; i < 10; i++) {
        config_snapshot_t snap = config_create_snapshot();
        ASSERT_NE(snap, nullptr) << "Failed on iteration " << i;

        ASSERT_TRUE(config_set_int("beta", 100 + i));
        ASSERT_TRUE(config_restore_snapshot(snap));
        EXPECT_EQ(config_get_int("beta", 0), 100);

        config_destroy_snapshot(snap);
    }
}

TEST_F(ConfigSignalRegressionTest, DeadlockPrevention_CloneAndRestore) {
    config_snapshot_t orig = config_create_snapshot();
    ASSERT_NE(orig, nullptr);

    config_snapshot_t clone = config_clone_snapshot(orig);
    ASSERT_NE(clone, nullptr);

    ASSERT_TRUE(config_set_int("beta", 555));
    ASSERT_TRUE(config_restore_snapshot(clone));
    EXPECT_EQ(config_get_int("beta", 0), 100);

    config_destroy_snapshot(orig);
    config_destroy_snapshot(clone);
}

//=============================================================================
// Validator Registration/Unregistration Regression
//=============================================================================

static bool counting_validator(void* user_data) {
    int* count = (int*)user_data;
    (*count)++;
    return true;
}

TEST_F(ConfigSignalRegressionTest, ValidatorRegistration_MultipleSlots) {
    uint32_t ids[5];
    for (int i = 0; i < 5; i++) {
        ids[i] = config_register_reload_validator(counting_validator, nullptr);
        EXPECT_GT(ids[i], 0u) << "Failed to register validator " << i;
    }

    // Unregister all
    for (int i = 0; i < 5; i++) {
        config_unregister_reload_validator(ids[i]);
    }
}

//=============================================================================
// Callback Registration Regression
//=============================================================================

static void counting_pre_callback(uint32_t version_before, void* user_data) {
    (void)version_before;
    int* count = (int*)user_data;
    (*count)++;
}

static void counting_post_callback(uint32_t before, uint32_t after,
                                    bool success, void* user_data) {
    (void)before;
    (void)after;
    (void)success;
    int* count = (int*)user_data;
    (*count)++;
}

TEST_F(ConfigSignalRegressionTest, CallbackRegistration_MultipleSlots) {
    uint32_t pre_ids[3];
    uint32_t post_ids[3];

    for (int i = 0; i < 3; i++) {
        pre_ids[i] = config_register_pre_reload_callback(counting_pre_callback, nullptr);
        EXPECT_GT(pre_ids[i], 0u);

        post_ids[i] = config_register_post_reload_callback(counting_post_callback, nullptr);
        EXPECT_GT(post_ids[i], 0u);
    }

    for (int i = 0; i < 3; i++) {
        config_unregister_pre_reload_callback(pre_ids[i]);
        config_unregister_post_reload_callback(post_ids[i]);
    }
}

//=============================================================================
// Version Increment + Snapshot Consistency
//=============================================================================

TEST_F(ConfigSignalRegressionTest, VersionIncrement_SnapshotCapturesCorrectVersion) {
    uint32_t v1 = config_get_version();

    config_force_version_increment();
    uint32_t v2 = config_get_version();
    EXPECT_EQ(v2, v1 + 1);

    config_snapshot_t snap = config_create_snapshot();
    ASSERT_NE(snap, nullptr);
    EXPECT_EQ(config_snapshot_get_version(snap), v2);

    config_force_version_increment();
    uint32_t v3 = config_get_version();
    EXPECT_EQ(v3, v2 + 1);

    // Snapshot should still have v2
    EXPECT_EQ(config_snapshot_get_version(snap), v2);

    config_destroy_snapshot(snap);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
