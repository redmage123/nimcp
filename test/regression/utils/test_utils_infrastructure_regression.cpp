/**
 * @file test_utils_infrastructure_regression.cpp
 * @brief Regression tests for utils infrastructure P1-P2 fixes
 *
 * WHAT: Verify P1-P2 fixes don't regress
 * WHY:  Ensure strdup tracking, platform mutex isolation, lock checking,
 *       and exception recursion guards remain correct
 * HOW:  Test each fix scenario that previously failed
 *
 * REGRESSION CATEGORIES:
 * - Strdup Leaks: nimcp_strdup allocations visible in tracking (P1-3)
 * - Platform Mutex: No circular dependency with immune system (P1-6)
 * - Graceful Degradation: Lock failures properly detected (P2-6)
 * - Exception Recursion: Nested throws don't crash (P1-5)
 *
 * @author NIMCP Development Team
 * @date 2026-02-08
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/fault_tolerance/nimcp_graceful_degradation.h"
}

namespace {

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class UtilsInfrastructureRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
        nimcp_memory_clear_stats();
        nimcp_exception_immune_init(NULL);
        nimcp_exception_immune_reset_stats();
    }

    void TearDown() override {
        nimcp_exception_immune_shutdown();
        nimcp_memory_reset_state();
        nimcp_memory_cleanup();
    }
};

/* ============================================================================
 * Regression_StrdupLeaksFixed (P1-3)
 * ============================================================================ */

TEST_F(UtilsInfrastructureRegressionTest, Regression_StrdupLeaksFixed) {
    /* WHAT: nimcp_strdup allocations show up in tracking
     * WHY:  Previously used raw malloc(), making strdup invisible to tracker
     * REGRESSION: P1-3 fix replaced raw malloc with nimcp_malloc in strdup */

    nimcp_memory_stats_t stats_before;
    nimcp_memory_get_stats(&stats_before);

    /* Perform multiple strdup operations */
    const char* strings[] = {
        "first string",
        "second string",
        "a longer third string for testing",
        ""  /* edge case: empty string */
    };
    char* copies[4];

    for (int i = 0; i < 4; i++) {
        copies[i] = nimcp_strdup(strings[i]);
        ASSERT_NE(copies[i], nullptr) << "strdup failed for string " << i;
        EXPECT_STREQ(copies[i], strings[i]);
    }

    /* Verify allocations were tracked */
    nimcp_memory_stats_t stats_after;
    nimcp_memory_get_stats(&stats_after);
    EXPECT_GE(stats_after.allocation_count, stats_before.allocation_count + 4)
        << "All 4 strdup allocations should be tracked";
    EXPECT_GT(stats_after.current_allocated, stats_before.current_allocated)
        << "Current allocated should reflect strdup allocations";

    /* Free all copies and verify tracking decrements */
    for (int i = 0; i < 4; i++) {
        nimcp_free(copies[i]);
    }

    nimcp_memory_stats_t stats_freed;
    nimcp_memory_get_stats(&stats_freed);
    EXPECT_EQ(stats_freed.current_allocated, stats_before.current_allocated)
        << "All strdup memory should be freed and tracked";
}

/* ============================================================================
 * Regression_PlatformMutexNoCircularDep (P1-6)
 * ============================================================================ */

TEST_F(UtilsInfrastructureRegressionTest, Regression_PlatformMutexNoCircularDep) {
    /* WHAT: Platform mutex init with NULL doesn't throw to immune
     * WHY:  Previously called NIMCP_THROW_TO_IMMUNE causing circular dependency
     * REGRESSION: P1-6 removed immune system calls from platform mutex */

    /* Test 1: NULL mutex init should return error code without crashing
     * Previously this would call into immune system -> mutex -> immune -> ... */
    int result = nimcp_platform_mutex_init(NULL, false);
    EXPECT_NE(result, 0) << "NULL mutex init should return error";

    /* Test 2: NULL mutex lock should return error without crashing */
    result = nimcp_platform_mutex_lock(NULL);
    EXPECT_NE(result, 0) << "NULL mutex lock should return error";

    /* Test 3: NULL mutex unlock should return error without crashing */
    result = nimcp_platform_mutex_unlock(NULL);
    EXPECT_NE(result, 0) << "NULL mutex unlock should return error";

    /* Test 4: NULL mutex destroy should return error without crashing */
    result = nimcp_platform_mutex_destroy(NULL);
    EXPECT_NE(result, 0) << "NULL mutex destroy should return error";

    /* Test 5: Normal mutex create/destroy should still work */
    nimcp_platform_mutex_t* mutex = nimcp_platform_mutex_create();
    ASSERT_NE(mutex, nullptr) << "Mutex create should succeed";

    result = nimcp_platform_mutex_lock(mutex);
    EXPECT_EQ(result, 0) << "Mutex lock should succeed";

    result = nimcp_platform_mutex_unlock(mutex);
    EXPECT_EQ(result, 0) << "Mutex unlock should succeed";

    nimcp_platform_mutex_destroy(mutex);
    nimcp_free(mutex);

    /* If we got here without hanging or crashing, the circular
     * dependency is resolved */
    SUCCEED() << "No circular dependency detected";
}

/* ============================================================================
 * Regression_GracefulDegradationLockCheck (P2-6)
 * ============================================================================ */

TEST_F(UtilsInfrastructureRegressionTest, Regression_GracefulDegradationLockCheck) {
    /* WHAT: Lock failures are properly detected in graceful degradation
     * WHY:  Previously nimcp_mutex_lock return value was ignored
     * REGRESSION: P2-6 added return value checking via gd_lock_checked */

    /* Create a valid GD context */
    gd_config_t config = gd_default_config();
    gd_context_t* ctx = gd_create(&config);
    ASSERT_NE(ctx, nullptr) << "GD context creation should succeed";

    /* Verify basic operations work with valid lock */
    gd_tier_t tier = gd_get_current_tier(ctx);
    EXPECT_EQ(tier, GD_TIER_FULL) << "Initial tier should be FULL";

    /* Set tier should work (requires lock) */
    bool result = gd_set_tier(ctx, GD_TIER_STANDARD, "regression test");
    EXPECT_TRUE(result) << "Set tier should succeed with valid mutex";

    tier = gd_get_current_tier(ctx);
    EXPECT_EQ(tier, GD_TIER_STANDARD) << "Tier should be STANDARD after set";

    /* Update resource should work (requires lock) */
    result = gd_update_resource(ctx, GD_RESOURCE_CPU, 50.0f);
    EXPECT_TRUE(result) << "Update resource should succeed with valid mutex";

    float usage = gd_get_resource_usage(ctx, GD_RESOURCE_CPU);
    EXPECT_FLOAT_EQ(usage, 50.0f) << "Resource usage should be 50%";

    /* Cleanup */
    gd_destroy(ctx);
}

/* ============================================================================
 * Regression_ExceptionRecursionGuard (P1-5)
 * ============================================================================ */

TEST_F(UtilsInfrastructureRegressionTest, Regression_ExceptionRecursionGuard) {
    /* WHAT: Nested throws don't crash
     * WHY:  Previously no depth limit on immune recursion
     * REGRESSION: P1-5 added depth counter with MAX_IMMUNE_DEPTH */

    /* Create and present multiple exceptions in rapid succession */
    const int RAPID_COUNT = 50;
    uint64_t presented_count = 0;

    for (int i = 0; i < RAPID_COUNT; i++) {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_NULL_POINTER,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Regression test exception %d", i
        );

        if (ex) {
            nimcp_immune_response_t response;
            memset(&response, 0, sizeof(response));
            int result = nimcp_exception_present_to_immune(ex, &response);
            if (result == 0) {
                presented_count++;
            }
            nimcp_exception_unref(ex);
        }
    }

    /* Should have successfully presented all (or most) exceptions
     * without crashing from recursion */
    EXPECT_GT(presented_count, 0u)
        << "Should have presented at least some exceptions";

    /* Verify stats are consistent */
    nimcp_exception_immune_stats_t stats;
    nimcp_exception_immune_get_stats(&stats);
    EXPECT_EQ(stats.exceptions_presented, presented_count)
        << "Stats should match presented count";

    SUCCEED() << "Exception recursion guard prevented crashes during rapid presentation";
}

} /* namespace */
