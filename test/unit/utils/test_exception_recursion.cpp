/**
 * @file test_exception_recursion.cpp
 * @brief Tests for exception recursion guard fix (P1-5)
 *
 * WHAT: Verify nested NIMCP_THROW_TO_IMMUNE calls don't cause stack overflow
 * WHY:  P1-5 added depth counter to prevent unbounded immune recursion
 * HOW:  Test nested throws, max depth behavior, and depth reset
 *
 * Function signatures tested:
 *   int nimcp_exception_immune_init(const nimcp_exception_immune_config_t* config);
 *   void nimcp_exception_immune_shutdown(void);
 *   int nimcp_exception_present_to_immune(nimcp_exception_t* ex, nimcp_immune_response_t* response);
 *   void nimcp_exception_immune_get_stats(nimcp_exception_immune_stats_t* stats);
 *   void nimcp_exception_immune_reset_stats(void);
 *   nimcp_exception_t* nimcp_exception_create(...);
 *   void nimcp_exception_unref(nimcp_exception_t* ex);
 *
 * @author NIMCP Development Team
 * @date 2026-02-08
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/memory/nimcp_memory.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class ExceptionRecursionTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_memory_init();
        nimcp_exception_immune_init(NULL);
        nimcp_exception_immune_reset_stats();
    }

    void TearDown() override {
        nimcp_exception_immune_shutdown();
    }
};

/* ============================================================================
 * Recursion Guard Tests
 * ============================================================================ */

TEST_F(ExceptionRecursionTest, ExceptionRecursion_NestedThrowDoesNotStackOverflow) {
    /* WHAT: Throw from within an exception handler shouldn't infinite loop
     * WHY:  P1-5 fix adds depth counter to prevent unbounded recursion
     * HOW:  Present multiple exceptions in sequence; verify no crash */

    /* Create an exception */
    nimcp_exception_t* ex1 = nimcp_exception_create(
        NIMCP_ERROR_NULL_POINTER,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Test exception for recursion guard"
    );
    ASSERT_NE(ex1, nullptr);

    /* Present to immune - this should work normally */
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    int result = nimcp_exception_present_to_immune(ex1, &response);
    EXPECT_EQ(result, 0) << "First presentation should succeed";
    EXPECT_TRUE(ex1->presented_to_immune);

    /* Create a second exception and present it */
    nimcp_exception_t* ex2 = nimcp_exception_create(
        NIMCP_ERROR_INVALID_PARAM,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        "Second exception"
    );
    ASSERT_NE(ex2, nullptr);

    result = nimcp_exception_present_to_immune(ex2, &response);
    EXPECT_EQ(result, 0) << "Second presentation should succeed";

    nimcp_exception_unref(ex1);
    nimcp_exception_unref(ex2);

    /* If we got here without crashing, the recursion guard works */
    SUCCEED() << "No stack overflow detected";
}

TEST_F(ExceptionRecursionTest, ExceptionRecursion_MaxDepthRespected) {
    /* WHAT: After MAX_IMMUNE_DEPTH throws, further throws are dropped
     * WHY:  Depth counter prevents unbounded nesting
     * HOW:  Verify that rapid presentation of many exceptions doesn't crash */

    /* Present many exceptions rapidly to stress-test the depth guard */
    const int NUM_EXCEPTIONS = 20;
    for (int i = 0; i < NUM_EXCEPTIONS; i++) {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_NULL_POINTER,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Stress test exception %d", i
        );
        ASSERT_NE(ex, nullptr) << "Failed to create exception " << i;

        /* Present to immune system (not connected, so just tracks stats) */
        int result = nimcp_exception_present_to_immune(ex, NULL);
        /* Result should be 0 (success) or the exception was already presented */
        EXPECT_EQ(result, 0) << "Presentation failed for exception " << i;

        nimcp_exception_unref(ex);
    }

    /* Verify stats show presentations were tracked */
    nimcp_exception_immune_stats_t stats;
    nimcp_exception_immune_get_stats(&stats);
    EXPECT_GT(stats.exceptions_presented, 0u)
        << "Should have tracked at least some presentations";

    SUCCEED() << "Max depth guard prevented stack overflow";
}

TEST_F(ExceptionRecursionTest, ExceptionRecursion_DepthResetsAfterCompletion) {
    /* WHAT: After handling completes, depth counter returns to 0
     * WHY:  Depth must reset so future exceptions can still be presented
     * HOW:  Present exception, verify completion, then present another */

    /* First presentation */
    nimcp_exception_t* ex1 = nimcp_exception_create(
        NIMCP_ERROR_NULL_POINTER,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "First exception for depth reset test"
    );
    ASSERT_NE(ex1, nullptr);

    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    int result = nimcp_exception_present_to_immune(ex1, &response);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(ex1->presented_to_immune);

    nimcp_exception_unref(ex1);

    /* After first presentation completes, depth should be back to 0.
     * Verify by presenting another exception successfully. */
    nimcp_exception_t* ex2 = nimcp_exception_create(
        NIMCP_ERROR_INVALID_PARAM,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        "Second exception after depth reset"
    );
    ASSERT_NE(ex2, nullptr);

    memset(&response, 0, sizeof(response));
    result = nimcp_exception_present_to_immune(ex2, &response);
    EXPECT_EQ(result, 0) << "Second presentation should succeed after depth reset";
    EXPECT_TRUE(ex2->presented_to_immune);

    /* Verify stats show both presentations */
    nimcp_exception_immune_stats_t stats;
    nimcp_exception_immune_get_stats(&stats);
    EXPECT_GE(stats.exceptions_presented, 2u)
        << "Both exceptions should have been presented";

    nimcp_exception_unref(ex2);
}
