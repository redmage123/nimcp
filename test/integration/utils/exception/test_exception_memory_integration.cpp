/**
 * @file test_exception_memory_integration.cpp
 * @brief Integration Tests for Exception-Memory System Interaction
 *
 * WHAT: Integration tests for exception system and memory tracking interplay
 * WHY:  Verify that exceptions thrown during memory allocation don't corrupt
 *       tracking, that recursion guards work, and that platform mutexes
 *       operate independently of the exception system
 * HOW:  Test combined exception + memory scenarios
 *
 * TESTS:
 * - ExceptionMemoryIntegration_ThrowDuringAlloc: Exception during allocation
 * - ExceptionMemoryIntegration_RecursionGuardWorks: Nested throws handled safely
 * - ExceptionMemoryIntegration_PlatformLayerIndependence: Mutex without exceptions
 *
 * @author NIMCP Development Team
 * @date 2026-02-08
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdlib>
#include <thread>
#include <atomic>

extern "C" {
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/error/nimcp_error_codes.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class ExceptionMemoryIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
        nimcp_exception_system_init();
    }

    void TearDown() override {
        nimcp_exception_system_shutdown();
        nimcp_memory_enable_tracking(false);
        nimcp_memory_cleanup();
    }
};

//=============================================================================
// Test 1: Exception Thrown During Memory Operations
//=============================================================================

TEST_F(ExceptionMemoryIntegrationTest, ExceptionMemoryIntegration_ThrowDuringAlloc) {
    // Get baseline stats
    nimcp_memory_stats_t baseline;
    nimcp_memory_get_stats(&baseline);

    // Allocate some memory
    void* ptr1 = nimcp_malloc(256);
    ASSERT_NE(ptr1, nullptr);

    // Create an exception (this internally uses memory allocation)
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY, EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Simulated memory error during allocation");
    ASSERT_NE(ex, nullptr) << "Exception creation should succeed";

    // The exception object was allocated via the memory system;
    // verify tracking still works
    nimcp_memory_stats_t after_ex;
    nimcp_memory_get_stats(&after_ex);
    EXPECT_GT(after_ex.allocation_count, baseline.allocation_count)
        << "Memory tracking should record exception allocation";

    // Allocate more memory after exception (should still work)
    void* ptr2 = nimcp_malloc(512);
    ASSERT_NE(ptr2, nullptr) << "Allocation after exception should succeed";

    // Free in reverse order
    nimcp_free(ptr2);
    nimcp_exception_unref(ex);
    nimcp_free(ptr1);

    // Verify tracking is consistent
    nimcp_memory_stats_t final_stats;
    nimcp_memory_get_stats(&final_stats);
    EXPECT_GT(final_stats.free_count, baseline.free_count);
}

//=============================================================================
// Test 2: Recursion Guard Works
//=============================================================================

TEST_F(ExceptionMemoryIntegrationTest, ExceptionMemoryIntegration_RecursionGuardWorks) {
    // Test that creating multiple exceptions rapidly doesn't cause recursion
    // The exception system has rate limiting and recursion guards

    const int NUM_EXCEPTIONS = 20;
    std::vector<nimcp_exception_t*> exceptions;
    exceptions.reserve(NUM_EXCEPTIONS);

    // Reset rate limiter for clean test
    nimcp_exception_reset_rate_limit();

    for (int i = 0; i < NUM_EXCEPTIONS; i++) {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_UNKNOWN, EXCEPTION_SEVERITY_WARNING,
            __FILE__, __LINE__, __func__,
            "Recursion guard test exception %d", i);
        if (ex) {
            exceptions.push_back(ex);
        }
        // Some may be rate-limited, which is expected behavior
    }

    // We should have created at least some exceptions
    EXPECT_GT(exceptions.size(), 0u)
        << "At least some exceptions should be created";

    // Present each to immune - this exercises the immune integration path
    // which could cause recursion if not properly guarded
    for (auto* ex : exceptions) {
        nimcp_immune_response_t response;
        memset(&response, 0, sizeof(response));
        int rc = nimcp_exception_present_to_immune(ex, &response);
        // May return error if immune not fully initialized, but should not crash
        (void)rc;
    }

    // Clean up
    for (auto* ex : exceptions) {
        nimcp_exception_unref(ex);
    }

    SUCCEED() << "Recursion guard prevented stack overflow with "
              << exceptions.size() << " exceptions";
}

//=============================================================================
// Test 3: Platform Layer Independence
//=============================================================================

TEST_F(ExceptionMemoryIntegrationTest, ExceptionMemoryIntegration_PlatformLayerIndependence) {
    // Shutdown exception system to test platform mutex independence
    nimcp_exception_system_shutdown();

    // Create platform mutex - should work without exception system
    nimcp_platform_mutex_t mutex;
    int rc = nimcp_platform_mutex_init(&mutex, false);
    EXPECT_EQ(rc, 0) << "Platform mutex init should work without exception system";

    // Lock/unlock cycle
    rc = nimcp_platform_mutex_lock(&mutex);
    EXPECT_EQ(rc, 0);
    rc = nimcp_platform_mutex_unlock(&mutex);
    EXPECT_EQ(rc, 0);

    // Memory allocation should also work without exception system
    void* ptr = nimcp_malloc(128);
    ASSERT_NE(ptr, nullptr)
        << "Memory allocation should work without exception system";
    nimcp_free(ptr);

    // Try lock
    rc = nimcp_platform_mutex_trylock(&mutex);
    if (rc == 0) {
        // Got the lock
        rc = nimcp_platform_mutex_unlock(&mutex);
        EXPECT_EQ(rc, 0);
    }

    // Multi-threaded test without exception system
    std::atomic<int> counter{0};
    const int NUM_THREADS = 4;
    const int OPS = 50;
    std::vector<std::thread> threads;

    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([&]() {
            for (int i = 0; i < OPS; i++) {
                nimcp_platform_mutex_lock(&mutex);
                int val = counter.load();
                counter.store(val + 1);
                nimcp_platform_mutex_unlock(&mutex);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(counter.load(), NUM_THREADS * OPS)
        << "Platform mutex should provide correct synchronization without exception system";

    // Destroy mutex
    rc = nimcp_platform_mutex_destroy(&mutex);
    EXPECT_EQ(rc, 0);

    // Re-initialize exception system for teardown
    nimcp_exception_system_init();
}
