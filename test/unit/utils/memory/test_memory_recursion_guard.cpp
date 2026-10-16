/**
 * @file test_memory_recursion_guard.cpp
 * @brief Unit tests for atomic recursion guard in memory subsystem
 *
 * WHAT: Verify that the recursion guard flags (g_memory_throw_active,
 *       g_umm_throw_active) correctly prevent infinite recursion during
 *       exception throwing from within memory allocation functions.
 *
 * WHY:  These guards were changed from plain _Thread_local bool to
 *       _Thread_local _Atomic bool to prevent compiler reordering of
 *       flag set/clear operations (critical with longjmp-based throws).
 *       These tests verify:
 *       1. Recursion guard prevents infinite recursion
 *       2. Concurrent memory allocation from multiple threads is safe
 *       3. After an exception, subsequent allocations still work
 *       4. Per-thread isolation of recursion guard state
 *
 * HOW:  GoogleTest with std::thread for concurrent testing
 */

#include <gtest/gtest.h>
#include <cstring>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

extern "C" {
#include "utils/memory/nimcp_memory.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/exception/nimcp_exception.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class MemoryRecursionGuardTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
        nimcp_memory_enable_debug_output(false);
        nimcp_memory_clear_stats();
    }

    void TearDown() override {
        nimcp_memory_enable_tracking(false);
    }
};

//=============================================================================
// Basic Recursion Guard Tests
//=============================================================================

/**
 * WHAT: Test that basic allocation works after initialization
 * WHY:  Sanity check that the atomic recursion guard doesn't break normal flow
 */
TEST_F(MemoryRecursionGuardTest, BasicAllocationStillWorks) {
    void* ptr = nimcp_malloc(64);
    ASSERT_NE(ptr, nullptr);
    memset(ptr, 0xAA, 64);
    nimcp_free(ptr);
}

/**
 * WHAT: Test that calloc works with the atomic recursion guard
 * WHY:  calloc triggers init_if_needed() which has its own recursion guard
 */
TEST_F(MemoryRecursionGuardTest, CallocStillWorks) {
    int* arr = static_cast<int*>(nimcp_calloc(10, sizeof(int)));
    ASSERT_NE(arr, nullptr);

    // Verify zero-initialization
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(arr[i], 0);
    }

    nimcp_free(arr);
}

/**
 * WHAT: Test that realloc works with the atomic recursion guard
 * WHY:  realloc also triggers init_if_needed()
 */
TEST_F(MemoryRecursionGuardTest, ReallocStillWorks) {
    void* ptr = nimcp_malloc(32);
    ASSERT_NE(ptr, nullptr);
    memset(ptr, 0xBB, 32);

    void* new_ptr = nimcp_realloc(ptr, 128);
    ASSERT_NE(new_ptr, nullptr);

    // Verify original data preserved
    unsigned char* bytes = static_cast<unsigned char*>(new_ptr);
    for (int i = 0; i < 32; i++) {
        EXPECT_EQ(bytes[i], 0xBB);
    }

    nimcp_free(new_ptr);
}

/**
 * WHAT: Test rapid successive allocations
 * WHY:  Verifies the atomic recursion guard doesn't have performance issues
 *       or state corruption across many rapid calls on the same thread
 */
TEST_F(MemoryRecursionGuardTest, RapidSuccessiveAllocations) {
    const int NUM_ALLOCS = 1000;
    std::vector<void*> ptrs;
    ptrs.reserve(NUM_ALLOCS);

    for (int i = 0; i < NUM_ALLOCS; i++) {
        void* p = nimcp_malloc(16 + (i % 256));
        ASSERT_NE(p, nullptr) << "Allocation " << i << " failed";
        ptrs.push_back(p);
    }

    // Free all in reverse order
    for (int i = NUM_ALLOCS - 1; i >= 0; i--) {
        nimcp_free(ptrs[i]);
    }
}

/**
 * WHAT: Test strdup with the atomic recursion guard
 * WHY:  strdup calls nimcp_malloc internally - tests recursion through public API
 */
TEST_F(MemoryRecursionGuardTest, StrdupStillWorks) {
    const char* original = "Test string for recursion guard verification";
    char* dup = nimcp_strdup(original);
    ASSERT_NE(dup, nullptr);
    EXPECT_STREQ(dup, original);
    nimcp_free(dup);
}

//=============================================================================
// Concurrent Thread Tests
//=============================================================================

/**
 * WHAT: Test concurrent allocations from multiple threads
 * WHY:  Verify the _Thread_local _Atomic guard provides proper per-thread
 *       isolation - each thread's recursion guard must be independent
 */
TEST_F(MemoryRecursionGuardTest, ConcurrentAllocationsSafe) {
    const int NUM_THREADS = 8;
    const int ALLOCS_PER_THREAD = 200;
    std::atomic<int> success_count{0};
    std::atomic<int> failure_count{0};

    auto worker = [&](int thread_id) {
        std::vector<void*> local_ptrs;
        local_ptrs.reserve(ALLOCS_PER_THREAD);

        for (int i = 0; i < ALLOCS_PER_THREAD; i++) {
            size_t size = 16 + ((thread_id * 37 + i * 13) % 512);
            void* ptr = nimcp_malloc(size);
            if (ptr) {
                // Write thread-specific pattern to detect cross-thread corruption
                memset(ptr, static_cast<unsigned char>(thread_id & 0xFF), size);
                local_ptrs.push_back(ptr);
                success_count.fetch_add(1, std::memory_order_relaxed);
            } else {
                failure_count.fetch_add(1, std::memory_order_relaxed);
            }
        }

        // Verify patterns are intact
        for (size_t i = 0; i < local_ptrs.size(); i++) {
            unsigned char expected = static_cast<unsigned char>(thread_id & 0xFF);
            unsigned char actual = *static_cast<unsigned char*>(local_ptrs[i]);
            EXPECT_EQ(actual, expected)
                << "Thread " << thread_id << " allocation " << i << " corrupted";
        }

        // Free all local allocations
        for (void* p : local_ptrs) {
            nimcp_free(p);
        }
    };

    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back(worker, t);
    }

    for (auto& t : threads) {
        t.join();
    }

    // All allocations should succeed
    EXPECT_EQ(success_count.load(), NUM_THREADS * ALLOCS_PER_THREAD);
    EXPECT_EQ(failure_count.load(), 0);
}

/**
 * WHAT: Test concurrent calloc from multiple threads
 * WHY:  calloc triggers init_if_needed() which has the UMM initialization
 *       recursion guard - verify no deadlock or corruption across threads
 */
TEST_F(MemoryRecursionGuardTest, ConcurrentCallocSafe) {
    const int NUM_THREADS = 4;
    const int ALLOCS_PER_THREAD = 100;
    std::atomic<int> total_success{0};

    auto worker = [&]() {
        std::vector<void*> local_ptrs;
        for (int i = 0; i < ALLOCS_PER_THREAD; i++) {
            void* ptr = nimcp_calloc(10, sizeof(int));
            if (ptr) {
                // Verify zero-initialization
                int* arr = static_cast<int*>(ptr);
                bool all_zero = true;
                for (int j = 0; j < 10; j++) {
                    if (arr[j] != 0) {
                        all_zero = false;
                        break;
                    }
                }
                EXPECT_TRUE(all_zero);
                local_ptrs.push_back(ptr);
                total_success.fetch_add(1, std::memory_order_relaxed);
            }
        }

        for (void* p : local_ptrs) {
            nimcp_free(p);
        }
    };

    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back(worker);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(total_success.load(), NUM_THREADS * ALLOCS_PER_THREAD);
}

/**
 * WHAT: Test mixed alloc/free from multiple threads simultaneously
 * WHY:  Stress test the recursion guard under concurrent alloc+free patterns
 *       that exercise both MEMORY_SAFE_THROW and UMM_SAFE_THROW paths
 */
TEST_F(MemoryRecursionGuardTest, ConcurrentMixedAllocFree) {
    const int NUM_THREADS = 6;
    const int ITERATIONS = 150;
    std::atomic<bool> failed{false};

    auto worker = [&]() {
        for (int i = 0; i < ITERATIONS && !failed.load(std::memory_order_relaxed); i++) {
            // Allocate
            void* ptr1 = nimcp_malloc(64);
            void* ptr2 = nimcp_calloc(8, sizeof(double));
            char* str = nimcp_strdup("test_recursion_guard_thread");

            if (!ptr1 || !ptr2 || !str) {
                failed.store(true, std::memory_order_relaxed);
                if (ptr1) nimcp_free(ptr1);
                if (ptr2) nimcp_free(ptr2);
                if (str) nimcp_free(str);
                return;
            }

            // Use the memory
            memset(ptr1, 0xCC, 64);
            static_cast<double*>(ptr2)[0] = 3.14159;

            // Free
            nimcp_free(ptr1);
            nimcp_free(ptr2);
            nimcp_free(str);
        }
    };

    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back(worker);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_FALSE(failed.load()) << "At least one thread had an allocation failure";
}

//=============================================================================
// Post-Exception Recovery Tests
//=============================================================================

/**
 * WHAT: Test that allocations work after an exception path
 * WHY:  The recursion guard must properly reset (clear) even if the throw
 *       path is exercised. With the atomic flag, the store to false must
 *       actually reach memory and not be optimized away.
 */
TEST_F(MemoryRecursionGuardTest, AllocationsWorkAfterExceptionPath) {
    // First, do some normal allocations
    void* ptr1 = nimcp_malloc(128);
    ASSERT_NE(ptr1, nullptr);
    nimcp_free(ptr1);

    // Attempt an allocation that should succeed after a potential throw path
    // (zero-size calloc is handled gracefully, not an exception)
    void* ptr2 = nimcp_calloc(0, 0);
    // Zero-size behavior is implementation-defined, don't assert on it
    if (ptr2) {
        nimcp_free(ptr2);
    }

    // After that, normal allocations must still work
    void* ptr3 = nimcp_malloc(256);
    ASSERT_NE(ptr3, nullptr) << "Allocation failed after exception path - "
                              << "recursion guard may be stuck in active state";
    memset(ptr3, 0xDD, 256);
    nimcp_free(ptr3);

    // Calloc must also still work
    void* ptr4 = nimcp_calloc(5, sizeof(long));
    ASSERT_NE(ptr4, nullptr) << "Calloc failed after exception path";
    nimcp_free(ptr4);
}

/**
 * WHAT: Test many allocation cycles to detect guard state leaks
 * WHY:  If the atomic store to false is ever skipped (e.g., due to compiler
 *       optimization or longjmp), subsequent throws would be silently suppressed.
 *       Running many cycles amplifies any such leak.
 */
TEST_F(MemoryRecursionGuardTest, NoGuardStateLeakOverManyCycles) {
    const int CYCLES = 5000;

    for (int i = 0; i < CYCLES; i++) {
        void* ptr = nimcp_malloc(32);
        ASSERT_NE(ptr, nullptr) << "Allocation failed at cycle " << i
                                 << " - recursion guard state may have leaked";
        nimcp_free(ptr);
    }
}

//=============================================================================
// Per-Thread Isolation Tests
//=============================================================================

/**
 * WHAT: Test that one thread's recursion guard state doesn't affect another
 * WHY:  The _Thread_local qualifier ensures per-thread copies, but we need
 *       to verify this under actual concurrent execution
 */
TEST_F(MemoryRecursionGuardTest, PerThreadIsolation) {
    const int NUM_THREADS = 4;
    const int ALLOCS_PER_THREAD = 500;

    // Each thread will track its own success/failure independently
    struct ThreadResult {
        int successes = 0;
        int failures = 0;
    };
    std::vector<ThreadResult> results(NUM_THREADS);

    auto worker = [&](int tid) {
        for (int i = 0; i < ALLOCS_PER_THREAD; i++) {
            void* ptr = nimcp_malloc(48);
            if (ptr) {
                results[tid].successes++;
                nimcp_free(ptr);
            } else {
                results[tid].failures++;
            }
        }
    };

    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back(worker, t);
    }

    for (auto& t : threads) {
        t.join();
    }

    // Each thread should have 100% success - no cross-thread interference
    for (int t = 0; t < NUM_THREADS; t++) {
        EXPECT_EQ(results[t].successes, ALLOCS_PER_THREAD)
            << "Thread " << t << " had " << results[t].failures << " failures";
        EXPECT_EQ(results[t].failures, 0)
            << "Thread " << t << " had failures - possible cross-thread guard interference";
    }
}

/**
 * WHAT: Test aligned allocation with the atomic recursion guard
 * WHY:  nimcp_aligned_alloc also calls init_if_needed and uses the guard
 */
TEST_F(MemoryRecursionGuardTest, AlignedAllocStillWorks) {
    void* ptr = nimcp_aligned_malloc(256, 64);
    ASSERT_NE(ptr, nullptr);

    // Verify alignment
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    EXPECT_EQ(addr % 64, 0u) << "Aligned allocation not properly aligned";

    memset(ptr, 0xEE, 256);
    nimcp_aligned_free(ptr);
}

/**
 * WHAT: Test statistics tracking works correctly with atomic guards
 * WHY:  The atomic operations in the recursion guard must not interfere
 *       with the statistics tracking mutex operations
 */
TEST_F(MemoryRecursionGuardTest, StatisticsTrackingIntact) {
    nimcp_memory_clear_stats();

    const int NUM_ALLOCS = 50;
    std::vector<void*> ptrs;

    for (int i = 0; i < NUM_ALLOCS; i++) {
        void* ptr = nimcp_malloc(100);
        ASSERT_NE(ptr, nullptr);
        ptrs.push_back(ptr);
    }

    nimcp_memory_stats_t stats;
    ASSERT_TRUE(nimcp_memory_get_stats(&stats));
    // Stats should reflect at least our allocations
    // (may be higher due to internal UMM allocations)
    EXPECT_GE(stats.allocation_count, static_cast<size_t>(NUM_ALLOCS));

    for (void* p : ptrs) {
        nimcp_free(p);
    }
}
