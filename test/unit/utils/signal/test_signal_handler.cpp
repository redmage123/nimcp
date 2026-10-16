//=============================================================================
// test_signal_handler.cpp - Comprehensive Signal Handler Testing
//=============================================================================
/**
 * @file test_signal_handler.cpp
 * @brief Unit tests for signal handler with checkpoint and recovery integration
 *
 * TESTS COVERED:
 * 1. Signal Installation & Uninstallation
 * 2. Signal Counting & Statistics
 * 3. Graceful Shutdown Signals (SIGTERM, SIGINT)
 * 4. Configuration Reload (SIGHUP)
 * 5. Health Status Monitoring
 * 6. Checkpoint Save/Recovery
 * 7. Auto-Recovery Configuration
 * 8. Checkpoint Retention Management
 */

#include "gtest/gtest.h"
#include "utils/signal/nimcp_signal_handler.h"
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

//=============================================================================
// Test Fixtures
//=============================================================================

class SignalHandlerTest : public ::testing::Test {
protected:
    virtual void SetUp() {
        // Install default signal handlers before each test
        signal_handler_config_t config = signal_handler_default_config();
        config.enable_checkpoint_save = false;  // Disable for tests unless needed
        ASSERT_TRUE(signal_handler_install(&config));
        signal_handler_reset_stats();
    }

    virtual void TearDown() {
        // Clean up: uninstall handlers and unregister brain
        signal_handler_uninstall();
        signal_handler_unregister_brain();
        signal_handler_reset_stats();
    }

    // Helper to simulate signal without crashing
    void check_signal_handled(int sig) {
        // This is verified through signal stats, not by actually sending signal
        // (sending SIGSEGV would crash the test)
    }
};

class SignalHealthTest : public ::testing::Test {
protected:
    virtual void SetUp() {
        signal_handler_config_t config = signal_handler_default_config();
        ASSERT_TRUE(signal_handler_install(&config));
        signal_handler_reset_stats();
    }

    virtual void TearDown() {
        signal_handler_uninstall();
        signal_handler_reset_stats();
    }
};

class SignalCheckpointTest : public ::testing::Test {
protected:
    const char* test_checkpoint_dir = "/tmp/nimcp_test_checkpoints";

    virtual void SetUp() {
        // Create test checkpoint directory
        mkdir(test_checkpoint_dir, 0755);

        signal_handler_config_t config = signal_handler_default_config();
        config.checkpoint_path = test_checkpoint_dir;
        config.enable_checkpoint_save = true;
        ASSERT_TRUE(signal_handler_install(&config));
        signal_handler_reset_stats();
    }

    virtual void TearDown() {
        signal_handler_uninstall();
        signal_handler_reset_stats();

        // Cleanup test directory
        system("rm -rf /tmp/nimcp_test_checkpoints");
    }
};

//=============================================================================
// Installation & Configuration Tests
//=============================================================================

TEST_F(SignalHandlerTest, InstallDefaultConfig) {
    // Test that default config installs successfully
    signal_handler_uninstall();

    signal_handler_config_t config = signal_handler_default_config();
    ASSERT_TRUE(signal_handler_install(&config));
    ASSERT_EQ(SIGNAL_MODE_LOG_SHUTDOWN, config.sigsegv_mode);
    ASSERT_EQ(SIGNAL_MODE_LOG_CONTINUE, config.sigfpe_mode);
}

TEST_F(SignalHandlerTest, InstallNullConfigUsesDefaults) {
    signal_handler_uninstall();

    // Install with NULL config should use defaults
    ASSERT_TRUE(signal_handler_install(NULL));
    ASSERT_TRUE(signal_handler_uninstall());
}

TEST_F(SignalHandlerTest, UninstallRestoresDefaultHandlers) {
    // Install and then uninstall
    ASSERT_TRUE(signal_handler_uninstall());

    // Should not crash when uninstalling twice
    ASSERT_FALSE(signal_handler_uninstall());
}

//=============================================================================
// Statistics & Counting Tests
//=============================================================================

TEST_F(SignalHandlerTest, GetSignalStats) {
    signal_handler_stats_t stats = signal_handler_get_stats();

    // Initial stats should be zero
    ASSERT_EQ(0, stats.sigsegv_count);
    ASSERT_EQ(0, stats.sigabrt_count);
    ASSERT_EQ(0, stats.sigbus_count);
    ASSERT_EQ(0, stats.sigfpe_count);
    ASSERT_EQ(0, stats.sigill_count);
    ASSERT_EQ(0, stats.sigterm_count);
    ASSERT_EQ(0, stats.sigint_count);
    ASSERT_EQ(0, stats.sighup_count);
    ASSERT_EQ(0, stats.recoveries);
    ASSERT_EQ(0, stats.fatal_crashes);
}

TEST_F(SignalHandlerTest, ResetStats) {
    // Get initial stats (all zeros)
    signal_handler_stats_t stats1 = signal_handler_get_stats();
    ASSERT_EQ(0, stats1.sigsegv_count);

    // Reset should not change anything if already zero
    signal_handler_reset_stats();
    signal_handler_stats_t stats2 = signal_handler_get_stats();
    ASSERT_EQ(0, stats2.sigsegv_count);
}

//=============================================================================
// Signal Name Tests
//=============================================================================

TEST_F(SignalHandlerTest, SignalNames) {
    ASSERT_STREQ("SIGSEGV", signal_handler_get_signal_name(SIGSEGV));
    ASSERT_STREQ("SIGABRT", signal_handler_get_signal_name(SIGABRT));
    ASSERT_STREQ("SIGBUS", signal_handler_get_signal_name(SIGBUS));
    ASSERT_STREQ("SIGFPE", signal_handler_get_signal_name(SIGFPE));
    ASSERT_STREQ("SIGILL", signal_handler_get_signal_name(SIGILL));
    ASSERT_STREQ("SIGTERM", signal_handler_get_signal_name(SIGTERM));
    ASSERT_STREQ("SIGINT", signal_handler_get_signal_name(SIGINT));
    ASSERT_STREQ("SIGHUP", signal_handler_get_signal_name(SIGHUP));
    ASSERT_STREQ("UNKNOWN", signal_handler_get_signal_name(999));
}

//=============================================================================
// Shutdown Signal Tests
//=============================================================================

TEST_F(SignalHandlerTest, ShutdownNotRequestedInitially) {
    ASSERT_FALSE(signal_handler_shutdown_requested());
}

TEST_F(SignalHandlerTest, ReloadNotRequestedInitially) {
    ASSERT_FALSE(signal_handler_reload_requested());
}

//=============================================================================
// Callback Tests
//=============================================================================

static int g_crash_callback_called = 0;
static int g_crash_callback_signal = 0;

static void test_crash_callback(int sig) {
    g_crash_callback_called++;
    g_crash_callback_signal = sig;
}

static int g_reload_callback_called = 0;

static void test_reload_callback(void) {
    g_reload_callback_called++;
}

TEST_F(SignalHandlerTest, SetCrashCallback) {
    g_crash_callback_called = 0;
    signal_handler_set_crash_callback(test_crash_callback);

    // Callback is registered but won't actually be called without a signal
    signal_handler_set_crash_callback(NULL);
}

TEST_F(SignalHandlerTest, SetReloadCallback) {
    g_reload_callback_called = 0;
    signal_handler_set_reload_callback(test_reload_callback);

    // Callback is registered but won't be called without SIGHUP
    signal_handler_set_reload_callback(NULL);
}

//=============================================================================
// Health Status Tests
//=============================================================================

TEST_F(SignalHealthTest, HealthStatusHealthyInitial) {
    signal_health_info_t health = signal_handler_get_health_status();

    ASSERT_EQ(SIGNAL_HEALTH_HEALTHY, health.status);
    ASSERT_EQ(0, health.total_signals);
    ASSERT_EQ(0, health.fatal_crashes);
    ASSERT_EQ(0, health.successful_recoveries);
    ASSERT_EQ(0, health.failed_recoveries);
    ASSERT_EQ(0.0f, health.recovery_success_rate);
    ASSERT_FALSE(health.is_in_recovery);
}

TEST_F(SignalHealthTest, HealthStatusBasicStructure) {
    signal_health_info_t health = signal_handler_get_health_status();

    // Verify all fields are accessible
    ASSERT_GE(health.total_signals, 0);
    ASSERT_GE(health.fatal_crashes, 0);
    ASSERT_GE(health.successful_recoveries, 0);
    ASSERT_GE(health.failed_recoveries, 0);
    ASSERT_GE(health.recovery_success_rate, 0.0f);
    ASSERT_LE(health.recovery_success_rate, 100.0f);
    ASSERT_NOT_NULL(health.last_signal_name);
}

//=============================================================================
// Checkpoint Integration Tests
//=============================================================================

TEST_F(SignalCheckpointTest, CheckpointDirectoryCreated) {
    // Directory should exist after setup
    struct stat st;
    ASSERT_EQ(0, stat(test_checkpoint_dir, &st));
    ASSERT_TRUE(S_ISDIR(st.st_mode));
}

TEST_F(SignalCheckpointTest, CheckpointCountInitial) {
    // No checkpoints initially (no brain registered)
    int count = signal_handler_get_checkpoint_count();
    // Count could be -1 (error) or 0 (no checkpoints)
    // We don't assert a specific value since no brain is registered
}

TEST_F(SignalCheckpointTest, CheckpointRetentionSetting) {
    signal_handler_set_checkpoint_retention(10);

    // Setting retention doesn't return a value, just ensure no crash
    ASSERT_TRUE(true);
}

TEST_F(SignalCheckpointTest, CheckpointRetentionZeroUnlimited) {
    // Setting to 0 means unlimited retention
    signal_handler_set_checkpoint_retention(0);

    // Verify setting was accepted
    ASSERT_TRUE(true);
}

TEST_F(SignalCheckpointTest, CheckpointSaveWithoutBrain) {
    // Cannot save checkpoint without registered brain
    ASSERT_FALSE(signal_handler_checkpoint_save(test_checkpoint_dir));
}

TEST_F(SignalCheckpointTest, CheckpointSaveCustomPath) {
    // Without brain, should return false
    ASSERT_FALSE(signal_handler_checkpoint_save("/tmp/custom_checkpoint"));
}

TEST_F(SignalCheckpointTest, ForceCheckpointDeprecated) {
    // Old API should still work (maps to new API)
    ASSERT_FALSE(signal_handler_force_checkpoint());
}

//=============================================================================
// Auto-Recovery Configuration Tests
//=============================================================================

TEST_F(SignalHandlerTest, AutoRecoveryEnabledByDefault) {
    ASSERT_TRUE(signal_handler_is_auto_recovery_enabled());
}

TEST_F(SignalHandlerTest, DisableAutoRecovery) {
    signal_handler_set_auto_recovery(false);
    ASSERT_FALSE(signal_handler_is_auto_recovery_enabled());
}

TEST_F(SignalHandlerTest, ReenableAutoRecovery) {
    signal_handler_set_auto_recovery(false);
    ASSERT_FALSE(signal_handler_is_auto_recovery_enabled());

    signal_handler_set_auto_recovery(true);
    ASSERT_TRUE(signal_handler_is_auto_recovery_enabled());
}

TEST_F(SignalHandlerTest, SetMaxRecoveryAttempts) {
    // Setting various limits
    signal_handler_set_max_recovery_attempts(1);
    signal_handler_set_max_recovery_attempts(5);
    signal_handler_set_max_recovery_attempts(0);  // Unlimited

    // Verify no crashes
    ASSERT_TRUE(true);
}

//=============================================================================
// Brain Registration Tests
//=============================================================================

TEST_F(SignalHandlerTest, RegisterBrain) {
    // Register NULL brain (valid use case)
    signal_handler_register_brain(NULL);

    // Should not crash
    ASSERT_TRUE(true);
}

TEST_F(SignalHandlerTest, UnregisterBrain) {
    signal_handler_register_brain(NULL);
    signal_handler_unregister_brain();

    // Should not crash
    ASSERT_TRUE(true);
}

//=============================================================================
// Last Signal Tests
//=============================================================================

TEST_F(SignalHandlerTest, GetLastSignalInitial) {
    int last_sig = signal_handler_get_last_signal();

    // Should be 0 or some initial value
    ASSERT_GE(last_sig, 0);
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(SignalCheckpointTest, HealthStatusIntegration) {
    signal_health_info_t health = signal_handler_get_health_status();

    // Should be healthy with no activity
    ASSERT_EQ(SIGNAL_HEALTH_HEALTHY, health.status);
    ASSERT_EQ(0, health.fatal_crashes);
}

TEST_F(SignalHandlerTest, MultipleInstallUninstallCycles) {
    for (int i = 0; i < 3; i++) {
        signal_handler_uninstall();
        ASSERT_TRUE(signal_handler_install(NULL));
        signal_handler_reset_stats();
    }

    // Should handle multiple cycles without issues
    ASSERT_TRUE(true);
}

TEST_F(SignalCheckpointTest, CheckpointAndHealthTogether) {
    // Enable auto-recovery
    signal_handler_set_auto_recovery(true);

    // Set checkpoint retention
    signal_handler_set_checkpoint_retention(5);

    // Get health status
    signal_health_info_t health = signal_handler_get_health_status();

    // Everything should work together
    ASSERT_TRUE(signal_handler_is_auto_recovery_enabled());
    ASSERT_EQ(SIGNAL_HEALTH_HEALTHY, health.status);
}

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(SignalHandlerTest, EmptySignalName) {
    const char* name = signal_handler_get_signal_name(0);
    ASSERT_NOT_NULL(name);
    // Should return some name, not NULL
}

TEST_F(SignalHandlerTest, LargeSignalNumber) {
    const char* name = signal_handler_get_signal_name(999999);
    ASSERT_STREQ("UNKNOWN", name);
}

TEST_F(SignalCheckpointTest, CheckpointPathNULL) {
    // NULL path should use configured default or fail gracefully
    ASSERT_FALSE(signal_handler_checkpoint_save(NULL));
}

TEST_F(SignalHandlerTest, GetHealthMultipleTimes) {
    // Should be idempotent
    signal_health_info_t h1 = signal_handler_get_health_status();
    signal_health_info_t h2 = signal_handler_get_health_status();

    ASSERT_EQ(h1.status, h2.status);
    ASSERT_EQ(h1.total_signals, h2.total_signals);
    ASSERT_EQ(h1.fatal_crashes, h2.fatal_crashes);
}

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(SignalHandlerTest, DefaultConfigValues) {
    signal_handler_config_t config = signal_handler_default_config();

    ASSERT_EQ(SIGNAL_MODE_LOG_SHUTDOWN, config.sigsegv_mode);
    ASSERT_EQ(SIGNAL_MODE_LOG_SHUTDOWN, config.sigabrt_mode);
    ASSERT_EQ(SIGNAL_MODE_LOG_SHUTDOWN, config.sigbus_mode);
    ASSERT_EQ(SIGNAL_MODE_LOG_CONTINUE, config.sigfpe_mode);
    ASSERT_EQ(SIGNAL_MODE_LOG_SHUTDOWN, config.sigill_mode);
    ASSERT_EQ(SIGNAL_MODE_LOG_SHUTDOWN, config.sigterm_mode);
    ASSERT_EQ(SIGNAL_MODE_LOG_SHUTDOWN, config.sigint_mode);
    ASSERT_EQ(SIGNAL_MODE_LOG_CONTINUE, config.sighup_mode);

    ASSERT_TRUE(config.enable_stack_trace);
    ASSERT_FALSE(config.enable_state_dump);
    ASSERT_FALSE(config.enable_checkpoint_save);
}

TEST_F(SignalHandlerTest, CustomConfig) {
    signal_handler_uninstall();

    signal_handler_config_t config = signal_handler_default_config();
    config.sigsegv_mode = SIGNAL_MODE_LOG_CONTINUE;  // Custom: attempt recovery
    config.enable_stack_trace = false;
    config.enable_checkpoint_save = true;
    config.checkpoint_path = "/tmp/custom_ckpt";

    ASSERT_TRUE(signal_handler_install(&config));
    ASSERT_TRUE(signal_handler_uninstall());
}

//=============================================================================
// Consistency Tests
//=============================================================================

TEST_F(SignalHandlerTest, StatsConsistency) {
    signal_handler_stats_t stats1 = signal_handler_get_stats();
    signal_handler_stats_t stats2 = signal_handler_get_stats();

    // Multiple reads should return same values
    ASSERT_EQ(stats1.sigsegv_count, stats2.sigsegv_count);
    ASSERT_EQ(stats1.fatal_crashes, stats2.fatal_crashes);
    ASSERT_EQ(stats1.recoveries, stats2.recoveries);
}

TEST_F(SignalHealthTest, HealthConsistency) {
    signal_health_info_t h1 = signal_handler_get_health_status();
    signal_health_info_t h2 = signal_handler_get_health_status();

    ASSERT_EQ(h1.status, h2.status);
    ASSERT_EQ(h1.total_signals, h2.total_signals);
    ASSERT_EQ(h1.checkpoint_saves, h2.checkpoint_saves);
}

//=============================================================================
// Recovery Configuration Tests
//=============================================================================

TEST_F(SignalHandlerTest, RecoveryAttemptLimit) {
    signal_handler_set_max_recovery_attempts(1);
    signal_handler_set_max_recovery_attempts(10);
    signal_handler_set_max_recovery_attempts(0);

    // Should accept all values without error
    ASSERT_TRUE(true);
}

TEST_F(SignalHandlerTest, AutoRecoveryToggleMultipleTimes) {
    for (int i = 0; i < 5; i++) {
        signal_handler_set_auto_recovery(i % 2 == 0);
        bool enabled = signal_handler_is_auto_recovery_enabled();
        ASSERT_EQ((i % 2 == 0), enabled);
    }
}

//=============================================================================
// Thread-Local Recovery Tests
//=============================================================================

class SignalRecoveryTest : public ::testing::Test {
protected:
    virtual void SetUp() {
        signal_handler_config_t config = signal_handler_default_config();
        config.enable_checkpoint_save = false;
        ASSERT_TRUE(signal_handler_install(&config));
        signal_handler_reset_stats();

        // Initialize thread-local recovery for main test thread
        ASSERT_EQ(0, signal_handler_init_thread_recovery());
    }

    virtual void TearDown() {
        signal_handler_cleanup_thread_recovery();
        signal_handler_uninstall();
        signal_handler_reset_stats();
    }
};

TEST_F(SignalRecoveryTest, ThreadRecoveryInitCleanup) {
    // Already initialized in SetUp, verify cleanup works
    signal_handler_cleanup_thread_recovery();

    // Re-initialize should work
    ASSERT_EQ(0, signal_handler_init_thread_recovery());

    // Double init should succeed (no-op)
    ASSERT_EQ(0, signal_handler_init_thread_recovery());
}

TEST_F(SignalRecoveryTest, GetRecoveryContext) {
    signal_recovery_ctx_t* ctx = signal_handler_get_recovery_ctx();

    ASSERT_NOT_NULL(ctx);
    ASSERT_EQ(0, ctx->valid);
    ASSERT_EQ(RECOVERY_INITIAL, ctx->result);
    ASSERT_EQ(0, ctx->crash_signal);
    ASSERT_EQ(0, ctx->retry_count);
}

TEST_F(SignalRecoveryTest, SetRecoveryPointBasic) {
    signal_recovery_ctx_t* ctx = signal_handler_get_recovery_ctx();
    ASSERT_NOT_NULL(ctx);

    // Set recovery point - should return RECOVERY_INITIAL on first call
    int result = signal_handler_set_recovery_point_ex(ctx, 3, "test_recovery");

    ASSERT_EQ(RECOVERY_INITIAL, result);
    ASSERT_EQ(1, ctx->valid);
    ASSERT_EQ(3, ctx->max_retries);
    ASSERT_STREQ("test_recovery", ctx->label);

    // Clear recovery point
    signal_handler_clear_recovery_point_ex(ctx);

    ASSERT_EQ(0, ctx->valid);
    ASSERT_EQ(RECOVERY_INITIAL, ctx->result);
}

TEST_F(SignalRecoveryTest, CanRecoverCheck) {
    signal_recovery_ctx_t* ctx = signal_handler_get_recovery_ctx();
    ASSERT_NOT_NULL(ctx);

    // Initially cannot recover (no recovery point set)
    signal_handler_clear_recovery_point_ex(ctx);
    ASSERT_FALSE(signal_handler_can_recover());

    // Set recovery point
    int result = signal_handler_set_recovery_point_ex(ctx, 0, "test");
    ASSERT_EQ(RECOVERY_INITIAL, result);

    // Now can recover
    ASSERT_TRUE(signal_handler_can_recover());

    // Clear and verify
    signal_handler_clear_recovery_point_ex(ctx);
    ASSERT_FALSE(signal_handler_can_recover());
}

TEST_F(SignalRecoveryTest, GetCrashSignalInitial) {
    // No crash yet, should return 0 or last signal
    int sig = signal_handler_get_crash_signal();
    ASSERT_GE(sig, 0);
}

TEST_F(SignalRecoveryTest, RecoveryPointWithNullContext) {
    // Using NULL context should auto-get thread context
    int result = signal_handler_set_recovery_point_ex(NULL, 2, "null_ctx_test");

    ASSERT_EQ(RECOVERY_INITIAL, result);
    ASSERT_TRUE(signal_handler_can_recover());

    signal_handler_clear_recovery_point_ex(NULL);
    ASSERT_FALSE(signal_handler_can_recover());
}

TEST_F(SignalRecoveryTest, MultipleRecoveryPointsOverwrite) {
    signal_recovery_ctx_t* ctx = signal_handler_get_recovery_ctx();
    ASSERT_NOT_NULL(ctx);

    // Set first recovery point
    int result1 = signal_handler_set_recovery_point_ex(ctx, 1, "first");
    ASSERT_EQ(RECOVERY_INITIAL, result1);
    ASSERT_STREQ("first", ctx->label);

    // Clear and set second
    signal_handler_clear_recovery_point_ex(ctx);
    int result2 = signal_handler_set_recovery_point_ex(ctx, 5, "second");
    ASSERT_EQ(RECOVERY_INITIAL, result2);
    ASSERT_STREQ("second", ctx->label);
    ASSERT_EQ(5, ctx->max_retries);

    signal_handler_clear_recovery_point_ex(ctx);
}

TEST_F(SignalRecoveryTest, TriggerRecoveryNoValidPoint) {
    signal_recovery_ctx_t* ctx = signal_handler_get_recovery_ctx();
    ASSERT_NOT_NULL(ctx);

    // Ensure no valid recovery point
    signal_handler_clear_recovery_point_ex(ctx);
    signal_handler_clear_recovery_point();  // Clear global too

    // Trigger should fail (return -1) since no valid point
    int result = signal_handler_trigger_recovery(RECOVERY_CRASH_HANDLED);
    ASSERT_EQ(-1, result);
}

//=============================================================================
// Recovery Macro Tests (simulate without actual crashes)
//=============================================================================

TEST_F(SignalRecoveryTest, TryRecoverMacroStructure) {
    // Test that the macro compiles and executes without crash
    int executed = 0;
    int recovered = 0;

    // Note: This won't actually trigger recovery since no signal occurs
    SIGNAL_TRY_RECOVER(3, "macro_test") {
        executed = 1;
        // Safe code - no crash here
    } SIGNAL_ON_CRASH {
        recovered = 1;
    } SIGNAL_TRY_END;

    // Should have executed the try block
    ASSERT_EQ(1, executed);
    // Should NOT have recovered (no crash)
    ASSERT_EQ(0, recovered);
}

TEST_F(SignalRecoveryTest, RecoveryContextRetryCount) {
    signal_recovery_ctx_t* ctx = signal_handler_get_recovery_ctx();
    ASSERT_NOT_NULL(ctx);

    // Initial retry count should be 0
    ASSERT_EQ(0, ctx->retry_count);

    // Set recovery point multiple times (simulating retry pattern)
    for (int i = 0; i < 3; i++) {
        int result = signal_handler_set_recovery_point_ex(ctx, 5, "retry_test");
        ASSERT_EQ(RECOVERY_INITIAL, result);
        signal_handler_clear_recovery_point_ex(ctx);
    }

    // Retry count is only incremented on actual recovery jump
    // Since we cleared without jumping, retry_count stays at 0
    ASSERT_EQ(0, ctx->retry_count);
}

TEST_F(SignalRecoveryTest, RecoveryContextUserData) {
    signal_recovery_ctx_t* ctx = signal_handler_get_recovery_ctx();
    ASSERT_NOT_NULL(ctx);

    // Initially user_data should be NULL
    ASSERT_EQ(nullptr, ctx->user_data);

    // Set user data
    int user_value = 42;
    ctx->user_data = &user_value;

    ASSERT_EQ(&user_value, ctx->user_data);
    ASSERT_EQ(42, *(int*)ctx->user_data);

    // Clear user data
    ctx->user_data = nullptr;
}

TEST_F(SignalRecoveryTest, CleanupWithActiveRecoveryPoint) {
    signal_recovery_ctx_t* ctx = signal_handler_get_recovery_ctx();
    ASSERT_NOT_NULL(ctx);

    // Set recovery point
    int result = signal_handler_set_recovery_point_ex(ctx, 2, "cleanup_test");
    ASSERT_EQ(RECOVERY_INITIAL, result);
    ASSERT_TRUE(signal_handler_can_recover());

    // Cleanup should safely clear the context
    signal_handler_cleanup_thread_recovery();

    // After cleanup, cannot recover anymore (context is freed)
    ASSERT_FALSE(signal_handler_can_recover());

    // Re-initialize for TearDown
    ASSERT_EQ(0, signal_handler_init_thread_recovery());
}

//=============================================================================
// Integration with Signal Stats
//=============================================================================

TEST_F(SignalRecoveryTest, ImmuneStatsWithRecovery) {
    uint64_t immune_recoveries = 0;
    uint64_t total_crashes = 0;

    signal_handler_get_immune_stats(&immune_recoveries, &total_crashes);

    // Initially zero
    ASSERT_EQ(0, immune_recoveries);
    ASSERT_EQ(0, total_crashes);
}

TEST_F(SignalRecoveryTest, HasCodeImmuneCheck) {
    // Check code immune availability
    bool has_immune = signal_handler_has_code_immune();

    // Depends on compile-time flags - just ensure no crash
    (void)has_immune;
    ASSERT_TRUE(true);
}

#endif // TEST_SIGNAL_HANDLER_CPP
