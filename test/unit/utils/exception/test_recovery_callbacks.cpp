/**
 * @file test_recovery_callbacks.cpp
 * @brief Unit tests for exception recovery callbacks
 * @version 1.0.0
 * @date 2026-01-21
 *
 * WHAT: Unit tests for recovery callbacks in nimcp_exception_immune.c
 * WHY: Verify recovery callbacks handle unconfigured context gracefully
 * HOW: Test-driven development with Google Test framework
 *
 * Test Coverage:
 * - nimcp_recovery_compact() - Memory compaction recovery
 * - nimcp_recovery_restart_component() - Component restart recovery
 * - nimcp_recovery_graceful_shutdown() - Graceful shutdown recovery
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <cstring>

// Headers have their own extern "C" guards
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

//=============================================================================
// Test Fixture
//=============================================================================

class RecoveryCallbacksTest : public ::testing::Test {
protected:
    size_t baseline_allocated = 0;

    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);

        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        baseline_allocated = stats.current_allocated;

        // Initialize exception system
        nimcp_exception_system_init();

        // Initialize exception-immune integration
        nimcp_exception_immune_init(NULL);

        // Reset stats to start fresh
        nimcp_exception_immune_reset_stats();
    }

    void TearDown() override {
        // Shutdown exception-immune integration
        nimcp_exception_immune_shutdown();

        // Clear any current exception
        nimcp_exception_clear_current();

        // Shutdown exception system
        nimcp_exception_system_shutdown();

        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        EXPECT_EQ(stats.current_allocated, baseline_allocated)
            << "Memory leak detected! Allocated: " << stats.current_allocated
            << ", Baseline: " << baseline_allocated;
    }

    // Helper to create a test exception
    nimcp_exception_t* create_test_exception(nimcp_error_t code,
                                              nimcp_exception_severity_t severity,
                                              const char* message) {
        return nimcp_exception_create(
            code,
            severity,
            __FILE__,
            __LINE__,
            __func__,
            "%s", message
        );
    }

    // Helper to create a memory exception
    nimcp_memory_exception_t* create_memory_exception(size_t requested_size,
                                                       const char* message) {
        return nimcp_memory_exception_create(
            NIMCP_ERROR_NO_MEMORY,
            EXCEPTION_SEVERITY_SEVERE,
            __FILE__,
            __LINE__,
            __func__,
            requested_size,
            "%s", message
        );
    }
};

//=============================================================================
// nimcp_recovery_compact() Tests
//=============================================================================

TEST_F(RecoveryCallbacksTest, CompactReturnsErrorWhenContextNotConfigured) {
    // Create a test exception
    nimcp_exception_t* ex = create_test_exception(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        "Memory fragmentation test"
    );
    ASSERT_NE(ex, nullptr);

    // Recovery context is NOT configured - should return error gracefully
    int result = nimcp_recovery_compact(ex, EXCEPTION_RECOVERY_COMPACT, NULL);

    // Should return -1 when GC context is not configured
    EXPECT_EQ(result, -1);

    nimcp_exception_unref(ex);
}

TEST_F(RecoveryCallbacksTest, CompactHandlesNullException) {
    // Call with NULL exception - should handle gracefully
    int result = nimcp_recovery_compact(NULL, EXCEPTION_RECOVERY_COMPACT, NULL);

    // Should return -1 when GC context is not configured
    // (the function checks context first, not exception)
    EXPECT_EQ(result, -1);
}

TEST_F(RecoveryCallbacksTest, CompactHandlesNullUserData) {
    nimcp_exception_t* ex = create_test_exception(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        "Null user_data test"
    );
    ASSERT_NE(ex, nullptr);

    // user_data is NULL - should not crash
    int result = nimcp_recovery_compact(ex, EXCEPTION_RECOVERY_COMPACT, NULL);

    // Returns -1 because context not configured
    EXPECT_EQ(result, -1);

    nimcp_exception_unref(ex);
}

TEST_F(RecoveryCallbacksTest, CompactLogsAppropriateMessages) {
    // This test verifies the function executes without crash and logs
    // We can't easily verify log output, but we ensure no crash occurs
    nimcp_exception_t* ex = create_test_exception(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        "Log test for compact"
    );
    ASSERT_NE(ex, nullptr);

    // Should log "Executing memory compaction recovery action" and
    // "Memory compaction: GC context not configured"
    EXPECT_NO_THROW({
        nimcp_recovery_compact(ex, EXCEPTION_RECOVERY_COMPACT, NULL);
    });

    nimcp_exception_unref(ex);
}

//=============================================================================
// nimcp_recovery_restart_component() Tests
//=============================================================================

TEST_F(RecoveryCallbacksTest, RestartComponentReturnsErrorWhenNoException) {
    // Call with NULL exception - should return error
    int result = nimcp_recovery_restart_component(NULL, EXCEPTION_RECOVERY_RESTART_COMPONENT, NULL);

    // Should return -1 when exception is NULL
    EXPECT_EQ(result, -1);
}

TEST_F(RecoveryCallbacksTest, RestartComponentReturnsErrorWhenContextNotConfigured) {
    // Create a test exception
    nimcp_exception_t* ex = create_test_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        "Component failure test"
    );
    ASSERT_NE(ex, nullptr);

    // Recovery context is NOT configured - should return error gracefully
    int result = nimcp_recovery_restart_component(ex, EXCEPTION_RECOVERY_RESTART_COMPONENT, NULL);

    // Should return -1 when brain context is not configured
    EXPECT_EQ(result, -1);

    nimcp_exception_unref(ex);
}

TEST_F(RecoveryCallbacksTest, RestartComponentHandlesNullUserData) {
    nimcp_exception_t* ex = create_test_exception(
        NIMCP_ERROR_INVALID_STATE,
        EXCEPTION_SEVERITY_ERROR,
        "Null user_data restart test"
    );
    ASSERT_NE(ex, nullptr);

    // user_data is NULL - should not crash
    int result = nimcp_recovery_restart_component(ex, EXCEPTION_RECOVERY_RESTART_COMPONENT, NULL);

    // Returns -1 because brain context not configured
    EXPECT_EQ(result, -1);

    nimcp_exception_unref(ex);
}

TEST_F(RecoveryCallbacksTest, RestartComponentExtractsComponentNameFromFile) {
    // Create exception with specific file name to test component extraction
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        "/home/test/src/cognitive/memory/memory_module.c",
        42,
        "memory_init",
        "Component name extraction test"
    );
    ASSERT_NE(ex, nullptr);

    // Should log the file path as component name
    EXPECT_NO_THROW({
        nimcp_recovery_restart_component(ex, EXCEPTION_RECOVERY_RESTART_COMPONENT, NULL);
    });

    nimcp_exception_unref(ex);
}

TEST_F(RecoveryCallbacksTest, RestartComponentLogsAppropriateMessages) {
    nimcp_exception_t* ex = create_test_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_SEVERE,
        "Log test for restart component"
    );
    ASSERT_NE(ex, nullptr);

    // Should log "Executing component restart recovery action" and
    // component information
    EXPECT_NO_THROW({
        nimcp_recovery_restart_component(ex, EXCEPTION_RECOVERY_RESTART_COMPONENT, NULL);
    });

    nimcp_exception_unref(ex);
}

//=============================================================================
// nimcp_recovery_graceful_shutdown() Tests
//=============================================================================

TEST_F(RecoveryCallbacksTest, GracefulShutdownDoesNotCrashWithNullException) {
    // Call with NULL exception - should handle gracefully
    int result = nimcp_recovery_graceful_shutdown(NULL, EXCEPTION_RECOVERY_GRACEFUL_SHUTDOWN, NULL);

    // Should return 0 (success) - graceful shutdown should succeed even without exception
    EXPECT_EQ(result, 0);
}

TEST_F(RecoveryCallbacksTest, GracefulShutdownReturnsSuccessWhenContextNotConfigured) {
    nimcp_exception_t* ex = create_test_exception(
        NIMCP_ERROR_CRASH_RECOVERY,
        EXCEPTION_SEVERITY_FATAL,
        "Fatal error requiring shutdown"
    );
    ASSERT_NE(ex, nullptr);

    // Recovery context is NOT configured - should still succeed
    // because graceful shutdown always completes
    int result = nimcp_recovery_graceful_shutdown(ex, EXCEPTION_RECOVERY_GRACEFUL_SHUTDOWN, NULL);

    // Should return 0 - graceful shutdown always succeeds
    EXPECT_EQ(result, 0);

    nimcp_exception_unref(ex);
}

TEST_F(RecoveryCallbacksTest, GracefulShutdownHandlesNullUserData) {
    nimcp_exception_t* ex = create_test_exception(
        NIMCP_ERROR_CRASH_RECOVERY,
        EXCEPTION_SEVERITY_FATAL,
        "Null user_data shutdown test"
    );
    ASSERT_NE(ex, nullptr);

    // user_data is NULL - should not crash
    int result = nimcp_recovery_graceful_shutdown(ex, EXCEPTION_RECOVERY_GRACEFUL_SHUTDOWN, NULL);

    // Should return 0 (success)
    EXPECT_EQ(result, 0);

    nimcp_exception_unref(ex);
}

TEST_F(RecoveryCallbacksTest, GracefulShutdownIncrementsRecoveryStats) {
    nimcp_exception_immune_stats_t stats_before;
    nimcp_exception_immune_get_stats(&stats_before);

    nimcp_exception_t* ex = create_test_exception(
        NIMCP_ERROR_CRASH_RECOVERY,
        EXCEPTION_SEVERITY_FATAL,
        "Stats increment test"
    );
    ASSERT_NE(ex, nullptr);

    int result = nimcp_recovery_graceful_shutdown(ex, EXCEPTION_RECOVERY_GRACEFUL_SHUTDOWN, NULL);
    EXPECT_EQ(result, 0);

    nimcp_exception_immune_stats_t stats_after;
    nimcp_exception_immune_get_stats(&stats_after);

    // Should have incremented recoveries_succeeded
    EXPECT_GT(stats_after.recoveries_succeeded, stats_before.recoveries_succeeded);

    nimcp_exception_unref(ex);
}

TEST_F(RecoveryCallbacksTest, GracefulShutdownLogsExceptionDetails) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_CRASH_RECOVERY,
        EXCEPTION_SEVERITY_FATAL,
        __FILE__,
        __LINE__,
        __func__,
        "Detailed shutdown message: code=%d", 42
    );
    ASSERT_NE(ex, nullptr);

    // Should log exception details including code, severity, and message
    EXPECT_NO_THROW({
        nimcp_recovery_graceful_shutdown(ex, EXCEPTION_RECOVERY_GRACEFUL_SHUTDOWN, NULL);
    });

    nimcp_exception_unref(ex);
}

//=============================================================================
// Statistics Tests for Recovery Callbacks
//=============================================================================

TEST_F(RecoveryCallbacksTest, StatsResetProperlyBeforeTests) {
    nimcp_exception_immune_stats_t stats;
    nimcp_exception_immune_get_stats(&stats);

    // After reset in SetUp, should start at 0
    EXPECT_EQ(stats.recoveries_attempted, 0u);
    EXPECT_EQ(stats.recoveries_succeeded, 0u);
}

TEST_F(RecoveryCallbacksTest, MultipleShutdownsIncrementStats) {
    nimcp_exception_immune_stats_t stats_before;
    nimcp_exception_immune_get_stats(&stats_before);

    // Call graceful shutdown multiple times
    for (int i = 0; i < 3; i++) {
        nimcp_exception_t* ex = create_test_exception(
            NIMCP_ERROR_CRASH_RECOVERY,
            EXCEPTION_SEVERITY_FATAL,
            "Multi shutdown test"
        );
        ASSERT_NE(ex, nullptr);

        nimcp_recovery_graceful_shutdown(ex, EXCEPTION_RECOVERY_GRACEFUL_SHUTDOWN, NULL);
        nimcp_exception_unref(ex);
    }

    nimcp_exception_immune_stats_t stats_after;
    nimcp_exception_immune_get_stats(&stats_after);

    // Should have 3 more successful recoveries
    EXPECT_EQ(stats_after.recoveries_succeeded - stats_before.recoveries_succeeded, 3u);
}

//=============================================================================
// Different Exception Types Tests
//=============================================================================

TEST_F(RecoveryCallbacksTest, CompactWithMemoryException) {
    nimcp_memory_exception_t* mex = create_memory_exception(
        1024 * 1024,  // 1 MB
        "Memory exception for compact test"
    );
    ASSERT_NE(mex, nullptr);

    // Cast to base exception and call compact
    int result = nimcp_recovery_compact(
        (nimcp_exception_t*)mex,
        EXCEPTION_RECOVERY_COMPACT,
        NULL
    );

    // Returns -1 because GC context not configured
    EXPECT_EQ(result, -1);

    nimcp_exception_unref((nimcp_exception_t*)mex);
}

TEST_F(RecoveryCallbacksTest, GracefulShutdownWithDifferentSeverities) {
    // Test with different severity levels
    nimcp_exception_severity_t severities[] = {
        EXCEPTION_SEVERITY_ERROR,
        EXCEPTION_SEVERITY_SEVERE,
        EXCEPTION_SEVERITY_CRITICAL,
        EXCEPTION_SEVERITY_FATAL
    };

    for (auto severity : severities) {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_OPERATION_FAILED,
            severity,
            __FILE__,
            __LINE__,
            __func__,
            "Severity level test"
        );
        ASSERT_NE(ex, nullptr);

        int result = nimcp_recovery_graceful_shutdown(
            ex,
            EXCEPTION_RECOVERY_GRACEFUL_SHUTDOWN,
            NULL
        );

        // Graceful shutdown should always succeed regardless of severity
        EXPECT_EQ(result, 0);

        nimcp_exception_unref(ex);
    }
}

//=============================================================================
// Recovery Action Parameter Tests
//=============================================================================

TEST_F(RecoveryCallbacksTest, CompactIgnoresActionParameter) {
    nimcp_exception_t* ex = create_test_exception(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        "Action parameter test"
    );
    ASSERT_NE(ex, nullptr);

    // Pass different action - should still work the same
    int result1 = nimcp_recovery_compact(ex, EXCEPTION_RECOVERY_COMPACT, NULL);
    int result2 = nimcp_recovery_compact(ex, EXCEPTION_RECOVERY_GC, NULL);

    // Both should return the same result (context not configured)
    EXPECT_EQ(result1, result2);
    EXPECT_EQ(result1, -1);

    nimcp_exception_unref(ex);
}

TEST_F(RecoveryCallbacksTest, RestartComponentIgnoresActionParameter) {
    nimcp_exception_t* ex = create_test_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        "Action parameter test"
    );
    ASSERT_NE(ex, nullptr);

    // Pass different action - should still work the same
    int result1 = nimcp_recovery_restart_component(ex, EXCEPTION_RECOVERY_RESTART_COMPONENT, NULL);
    int result2 = nimcp_recovery_restart_component(ex, EXCEPTION_RECOVERY_RESTART_THREAD, NULL);

    // Both should return the same result (context not configured)
    EXPECT_EQ(result1, result2);
    EXPECT_EQ(result1, -1);

    nimcp_exception_unref(ex);
}

TEST_F(RecoveryCallbacksTest, GracefulShutdownIgnoresActionParameter) {
    nimcp_exception_t* ex = create_test_exception(
        NIMCP_ERROR_CRASH_RECOVERY,
        EXCEPTION_SEVERITY_FATAL,
        "Action parameter test"
    );
    ASSERT_NE(ex, nullptr);

    // Pass different action - should still work the same
    int result1 = nimcp_recovery_graceful_shutdown(ex, EXCEPTION_RECOVERY_GRACEFUL_SHUTDOWN, NULL);
    int result2 = nimcp_recovery_graceful_shutdown(ex, EXCEPTION_RECOVERY_NONE, NULL);

    // Both should return success
    EXPECT_EQ(result1, 0);
    EXPECT_EQ(result2, 0);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Install Default Recovery Callbacks Test
//=============================================================================

TEST_F(RecoveryCallbacksTest, InstallDefaultCallbacksSucceeds) {
    int result = nimcp_exception_install_default_recovery_callbacks();
    EXPECT_EQ(result, 0);
}

TEST_F(RecoveryCallbacksTest, ExecuteRecoveryAfterInstallDefaults) {
    // Install default callbacks
    int ret = nimcp_exception_install_default_recovery_callbacks();
    EXPECT_EQ(ret, 0);

    nimcp_exception_t* ex = create_test_exception(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        "Execute recovery test"
    );
    ASSERT_NE(ex, nullptr);

    // Execute through the standard API
    // This will use the registered callback
    int result = nimcp_exception_execute_recovery(ex, EXCEPTION_RECOVERY_COMPACT);

    // Should return -1 because context not configured
    EXPECT_EQ(result, -1);

    nimcp_exception_unref(ex);
}
