/**
 * @file test_memory_tracking_e2e.cpp
 * @brief End-to-End Tests for Memory Tracking System
 *
 * WHAT: Full workflow E2E tests for memory tracking, strdup, and exception safety
 * WHY:  Verify nimcp_strdup tracking, leak detection, exception recursion safety,
 *       and platform mutex independence from exception system
 * HOW:  Test memory operations through complete lifecycle scenarios
 *
 * TEST PIPELINES:
 * - MemoryTrackingE2E_StrdupIsTracked: nimcp_strdup result is tracked
 * - MemoryTrackingE2E_StrdupLeakDetection: Leak detection for strdup
 * - MemoryTrackingE2E_ExceptionRecursionSafety: Exception handler recursion guard
 * - MemoryTrackingE2E_PlatformMutexNoRecursion: Platform mutex independence
 *
 * @author NIMCP Development Team
 * @date 2026-02-08
 * @version 1.0.0
 */

#include "e2e_test_framework.h"

extern "C" {
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/error/nimcp_error_codes.h"
}

#include <thread>
#include <atomic>
#include <chrono>
#include <cstring>
#include <cstdlib>

//=============================================================================
// Test Fixture
//=============================================================================

class MemoryTrackingE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        // Ensure memory tracking is initialized
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
    }

    void TearDown() override {
        nimcp_memory_enable_tracking(false);
        nimcp_memory_cleanup();
    }
};

//=============================================================================
// Test 1: nimcp_strdup Is Tracked
//=============================================================================

TEST_F(MemoryTrackingE2ETest, MemoryTrackingE2E_StrdupIsTracked) {
    E2E_PIPELINE_START("Memory Tracking - strdup Tracked");

    // Stage 1: Get baseline stats
    E2E_STAGE_BEGIN("Get baseline stats", 100);
    nimcp_memory_stats_t baseline;
    nimcp_memory_get_stats(&baseline);
    size_t baseline_allocs = baseline.allocation_count;
    E2E_STAGE_END();

    // Stage 2: Perform strdup
    E2E_STAGE_BEGIN("Perform strdup", 100);
    const char* original = "Hello, NIMCP Memory Tracking!";
    char* duplicated = nimcp_strdup(original);
    E2E_ASSERT_NOT_NULL(duplicated, "nimcp_strdup returned NULL");
    EXPECT_STREQ(duplicated, original);
    E2E_STAGE_END();

    // Stage 3: Verify allocation was tracked
    E2E_STAGE_BEGIN("Verify allocation tracked", 100);
    nimcp_memory_stats_t after_strdup;
    nimcp_memory_get_stats(&after_strdup);
    EXPECT_GT(after_strdup.allocation_count, baseline_allocs)
        << "nimcp_strdup allocation was not tracked";
    size_t after_alloc_current = after_strdup.current_allocated;
    EXPECT_GT(after_alloc_current, baseline.current_allocated)
        << "Current allocated should increase after strdup";
    E2E_STAGE_END();

    // Stage 4: Free with nimcp_free and verify deallocation tracked
    E2E_STAGE_BEGIN("Free and verify deallocation", 100);
    nimcp_free(duplicated);
    nimcp_memory_stats_t after_free;
    nimcp_memory_get_stats(&after_free);
    EXPECT_GT(after_free.free_count, baseline.free_count)
        << "nimcp_free of strdup result was not tracked";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test 2: nimcp_strdup Leak Detection
//=============================================================================

TEST_F(MemoryTrackingE2ETest, MemoryTrackingE2E_StrdupLeakDetection) {
    E2E_PIPELINE_START("Memory Tracking - strdup Leak Detection");

    // Stage 1: Get baseline
    E2E_STAGE_BEGIN("Get baseline", 100);
    nimcp_memory_stats_t baseline;
    nimcp_memory_get_stats(&baseline);
    size_t baseline_current = baseline.current_allocated;
    E2E_STAGE_END();

    // Stage 2: Perform multiple strdups without freeing
    E2E_STAGE_BEGIN("Perform strdups", 200);
    const int NUM_STRDUPS = 5;
    char* leaked_ptrs[NUM_STRDUPS];
    for (int i = 0; i < NUM_STRDUPS; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Leaked string %d", i);
        leaked_ptrs[i] = nimcp_strdup(buf);
        ASSERT_NE(leaked_ptrs[i], nullptr) << "strdup " << i << " returned NULL";
    }
    E2E_STAGE_END();

    // Stage 3: Verify memory increased (leaks present)
    E2E_STAGE_BEGIN("Verify memory increased", 100);
    nimcp_memory_stats_t after_strdups;
    nimcp_memory_get_stats(&after_strdups);
    EXPECT_GT(after_strdups.current_allocated, baseline_current)
        << "Expected current_allocated to increase with leaked strdups";
    EXPECT_GE(after_strdups.allocation_count,
              baseline.allocation_count + NUM_STRDUPS)
        << "Expected at least " << NUM_STRDUPS << " new allocations";
    E2E_STAGE_END();

    // Stage 4: Run leak check (should detect leaks)
    // Note: nimcp_memory_check_leaks() logs leaks but does not return a count.
    // We verify via stats that allocations > frees.
    E2E_STAGE_BEGIN("Check leak detection", 200);
    nimcp_memory_check_leaks();
    nimcp_memory_stats_t leak_check;
    nimcp_memory_get_stats(&leak_check);
    EXPECT_GT(leak_check.allocation_count, leak_check.free_count)
        << "Leak detection: allocations should exceed frees";
    E2E_STAGE_END();

    // Stage 5: Clean up the "leaks" so test teardown is clean
    E2E_STAGE_BEGIN("Clean up leaked strings", 100);
    for (int i = 0; i < NUM_STRDUPS; i++) {
        nimcp_free(leaked_ptrs[i]);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test 3: Exception Recursion Safety
//=============================================================================

TEST_F(MemoryTrackingE2ETest, MemoryTrackingE2E_ExceptionRecursionSafety) {
    E2E_PIPELINE_START("Memory Tracking - Exception Recursion Safety");

    // Stage 1: Initialize exception system
    E2E_STAGE_BEGIN("Initialize exception system", 200);
    nimcp_exception_system_init();
    E2E_STAGE_END();

    // Stage 2: Create and throw an exception (should not recurse into memory system)
    E2E_STAGE_BEGIN("Create exception", 200);
    nimcp_exception_t* ex1 = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY, EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__, "Test memory error");
    // Creating an exception involves memory allocation (nimcp_calloc/nimcp_malloc)
    // which must NOT trigger another exception in an infinite loop
    ASSERT_NE(ex1, nullptr) << "Exception creation failed (possible recursion)";
    E2E_STAGE_END();

    // Stage 3: Create a second exception while the first is active
    E2E_STAGE_BEGIN("Nested exception creation", 200);
    nimcp_exception_t* ex2 = nimcp_exception_create(
        NIMCP_ERROR_NULL_POINTER, EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__, "Nested exception test");
    ASSERT_NE(ex2, nullptr) << "Nested exception creation failed";
    E2E_STAGE_END();

    // Stage 4: Present to immune (which may trigger memory allocation internally)
    E2E_STAGE_BEGIN("Present to immune", 200);
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    int rc = nimcp_exception_present_to_immune(ex1, &response);
    // May or may not succeed (immune system may not be connected), but should not crash
    (void)rc;
    SUCCEED() << "Immune presentation did not cause recursion";
    E2E_STAGE_END();

    // Stage 5: Clean up
    E2E_STAGE_BEGIN("Clean up", 100);
    nimcp_exception_unref(ex2);
    nimcp_exception_unref(ex1);
    nimcp_exception_system_shutdown();
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test 4: Platform Mutex No Recursion
//=============================================================================

TEST_F(MemoryTrackingE2ETest, MemoryTrackingE2E_PlatformMutexNoRecursion) {
    E2E_PIPELINE_START("Memory Tracking - Platform Mutex No Recursion");

    // Stage 1: Create platform mutex directly (no exception system involvement)
    E2E_STAGE_BEGIN("Create platform mutex", 100);
    nimcp_platform_mutex_t mutex;
    int rc = nimcp_platform_mutex_init(&mutex, false);
    EXPECT_EQ(rc, 0) << "Platform mutex init failed";
    E2E_STAGE_END();

    // Stage 2: Lock/unlock without exception system initialized
    // This verifies platform layer is independent of exception system
    E2E_STAGE_BEGIN("Lock/unlock without exception system", 200);
    rc = nimcp_platform_mutex_lock(&mutex);
    EXPECT_EQ(rc, 0) << "Platform mutex lock failed";
    rc = nimcp_platform_mutex_unlock(&mutex);
    EXPECT_EQ(rc, 0) << "Platform mutex unlock failed";
    E2E_STAGE_END();

    // Stage 3: Multiple lock/unlock cycles
    E2E_STAGE_BEGIN("Multiple lock/unlock cycles", 200);
    for (int i = 0; i < 100; i++) {
        rc = nimcp_platform_mutex_lock(&mutex);
        EXPECT_EQ(rc, 0) << "Lock failed at iteration " << i;
        rc = nimcp_platform_mutex_unlock(&mutex);
        EXPECT_EQ(rc, 0) << "Unlock failed at iteration " << i;
    }
    E2E_STAGE_END();

    // Stage 4: Concurrent lock/unlock from multiple threads
    E2E_STAGE_BEGIN("Concurrent mutex operations", 1000);
    std::atomic<int> counter{0};
    const int NUM_THREADS = 4;
    const int OPS_PER_THREAD = 100;
    std::vector<std::thread> threads;

    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([&]() {
            for (int i = 0; i < OPS_PER_THREAD; i++) {
                nimcp_platform_mutex_lock(&mutex);
                counter.fetch_add(1);
                nimcp_platform_mutex_unlock(&mutex);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(counter.load(), NUM_THREADS * OPS_PER_THREAD);
    E2E_STAGE_END();

    // Stage 5: Destroy
    E2E_STAGE_BEGIN("Destroy mutex", 100);
    rc = nimcp_platform_mutex_destroy(&mutex);
    EXPECT_EQ(rc, 0) << "Platform mutex destroy failed";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}
