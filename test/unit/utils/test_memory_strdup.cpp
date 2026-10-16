/**
 * @file test_memory_strdup.cpp
 * @brief Tests for nimcp_strdup() tracking fix (P1-3)
 *
 * WHAT: Verify nimcp_strdup() allocations are properly tracked
 * WHY:  P1-3 fixed nimcp_strdup() to use nimcp_malloc() instead of raw malloc()
 * HOW:  Test that strdup results are tracked, freeable, and handle edge cases
 *
 * Function signatures tested (from include/utils/memory/nimcp_memory.h):
 *   char* nimcp_strdup(const char* str);
 *   void  nimcp_free(void* ptr);
 *   void  nimcp_memory_init(void);
 *   void  nimcp_memory_cleanup(void);
 *   void  nimcp_memory_enable_tracking(bool enable);
 *   bool  nimcp_memory_get_stats(nimcp_memory_stats_t* stats);
 *   void  nimcp_memory_clear_stats(void);
 *   void  nimcp_memory_reset_state(void);
 *
 * @author NIMCP Development Team
 * @date 2026-02-08
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "utils/memory/nimcp_memory.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class MemoryStrdupTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
        nimcp_memory_clear_stats();
    }

    void TearDown() override {
        nimcp_memory_reset_state();
        nimcp_memory_cleanup();
    }
};

/* ============================================================================
 * StrdupTracking Tests
 * ============================================================================ */

TEST_F(MemoryStrdupTest, StrdupTracking_AllocatedPointerIsTracked) {
    /* WHAT: nimcp_strdup result should be freeable with nimcp_free without warnings
     * WHY:  P1-3 fix ensures strdup uses nimcp_malloc so the pointer is tracked
     * HOW:  Strdup a string, verify stats show allocation, free it, verify stats */

    nimcp_memory_stats_t stats_before;
    nimcp_memory_get_stats(&stats_before);
    size_t alloc_before = stats_before.allocation_count;

    const char* original = "Hello, NIMCP world!";
    char* copy = nimcp_strdup(original);
    ASSERT_NE(copy, nullptr) << "nimcp_strdup returned NULL for valid string";

    /* Verify the copy matches the original */
    EXPECT_STREQ(copy, original);

    /* Verify the allocation was tracked */
    nimcp_memory_stats_t stats_after;
    nimcp_memory_get_stats(&stats_after);
    EXPECT_GT(stats_after.allocation_count, alloc_before)
        << "Allocation count should increase after nimcp_strdup";
    EXPECT_GT(stats_after.current_allocated, stats_before.current_allocated)
        << "Current allocated should increase after nimcp_strdup";

    /* Free should succeed without warnings (pointer is tracked) */
    nimcp_free(copy);

    /* Verify the free was tracked */
    nimcp_memory_stats_t stats_freed;
    nimcp_memory_get_stats(&stats_freed);
    EXPECT_EQ(stats_freed.current_allocated, stats_before.current_allocated)
        << "Current allocated should return to original after free";
}

TEST_F(MemoryStrdupTest, StrdupTracking_NullInputHandled) {
    /* WHAT: nimcp_strdup(NULL) should handle gracefully
     * WHY:  NULL input should not crash, should return NULL
     * HOW:  Call with NULL, verify NULL return */

    char* result = nimcp_strdup(NULL);
    EXPECT_EQ(result, nullptr) << "nimcp_strdup(NULL) should return NULL";
}

TEST_F(MemoryStrdupTest, StrdupTracking_EmptyString) {
    /* WHAT: nimcp_strdup("") should work correctly
     * WHY:  Empty string is a valid input, should produce tracked 1-byte allocation
     * HOW:  Strdup empty string, verify content and tracking */

    char* copy = nimcp_strdup("");
    ASSERT_NE(copy, nullptr) << "nimcp_strdup(\"\") should not return NULL";

    /* Verify it's an empty string (just null terminator) */
    EXPECT_EQ(copy[0], '\0');
    EXPECT_EQ(strlen(copy), 0u);

    /* Verify it was tracked */
    nimcp_memory_stats_t stats;
    nimcp_memory_get_stats(&stats);
    EXPECT_GT(stats.allocation_count, 0u)
        << "Empty string strdup should still be tracked";

    nimcp_free(copy);
}

TEST_F(MemoryStrdupTest, StrdupTracking_LargeString) {
    /* WHAT: nimcp_strdup with a large string works
     * WHY:  Verify no issues with large allocations through strdup path
     * HOW:  Create large string, strdup it, verify content integrity */

    const size_t large_size = 100000;
    char* large_str = (char*)nimcp_malloc(large_size + 1);
    ASSERT_NE(large_str, nullptr);

    /* Fill with a pattern */
    for (size_t i = 0; i < large_size; i++) {
        large_str[i] = 'A' + (char)(i % 26);
    }
    large_str[large_size] = '\0';

    /* Strdup the large string */
    char* copy = nimcp_strdup(large_str);
    ASSERT_NE(copy, nullptr) << "nimcp_strdup failed for large string";

    /* Verify content matches */
    EXPECT_EQ(strlen(copy), large_size);
    EXPECT_EQ(memcmp(copy, large_str, large_size + 1), 0)
        << "Large string copy should match original";

    /* Verify both are tracked */
    nimcp_memory_stats_t stats;
    nimcp_memory_get_stats(&stats);
    EXPECT_GE(stats.allocation_count, 2u)
        << "Both original and copy should be tracked";

    nimcp_free(copy);
    nimcp_free(large_str);
}
