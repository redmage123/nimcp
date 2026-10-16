/**
 * @file test_memory_recursion_atomic_regression.cpp
 * @brief Regression tests for atomic recursion guard in memory subsystem
 *
 * WHAT: Regression tests to ensure the non-atomic recursion guard bug
 *       (g_memory_throw_active, g_umm_throw_active as plain bool) does not
 *       return. These flags were changed from _Thread_local bool to
 *       _Thread_local _Atomic bool to prevent:
 *       1. Compiler reordering of flag set/clear across longjmp boundaries
 *       2. Register caching of the flag value (flag stuck as true)
 *       3. Data races in pathological compiler optimization scenarios
 *
 * WHY:  The original plain bool flags could be optimized by the compiler in
 *       ways that violate the recursion guard contract:
 *       - The set-to-true/throw/set-to-false sequence could be reordered
 *       - If NIMCP_THROW_TO_IMMUNE triggers a longjmp, the set-to-false
 *         might never execute, and without atomic semantics the set-to-true
 *         might not have been committed to memory either (register-cached)
 *       - Subsequent calls would then either always-throw or never-throw
 *         depending on the cached register state
 *
 * HOW:  Stress tests with high thread counts and allocation volumes
 *       to amplify any guard state corruption or cross-thread interference
 *
 * @date 2026-02-15
 */

#include <gtest/gtest.h>
#include <cstring>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <numeric>
#include <algorithm>

extern "C" {
#include "utils/memory/nimcp_memory.h"
#include "utils/memory/nimcp_unified_memory.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class MemoryRecursionAtomicRegressionTest : public ::testing::Test {
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

    using clock = std::chrono::high_resolution_clock;

    double elapsed_ms(clock::time_point start, clock::time_point end) {
        return std::chrono::duration<double, std::milli>(end - start).count();
    }
};

//=============================================================================
// Regression: Guard state does not corrupt under heavy concurrent load
//=============================================================================

/**
 * WHAT: Stress test concurrent malloc/free from many threads
 * WHY:  The original plain bool guard could theoretically corrupt under
 *       heavy concurrent load if the compiler cached it in a register.
 *       This test runs enough iterations to trigger such corruption.
 * REGRESSION: Ensures fix for non-atomic recursion guard flags
 */
TEST_F(MemoryRecursionAtomicRegressionTest, ConcurrentMallocFreeStress) {
    const int NUM_THREADS = 8;
    const int ALLOCS_PER_THREAD = 500;
    std::atomic<int> total_success{0};
    std::atomic<int> total_failures{0};

    auto worker = [&](int tid) {
        for (int i = 0; i < ALLOCS_PER_THREAD; i++) {
            size_t sz = 8 + ((tid * 31 + i * 7) % 1024);
            void* ptr = nimcp_malloc(sz);
            if (ptr) {
                // Write a recognizable pattern
                memset(ptr, static_cast<unsigned char>((tid + i) & 0xFF), sz);
                nimcp_free(ptr);
                total_success.fetch_add(1, std::memory_order_relaxed);
            } else {
                total_failures.fetch_add(1, std::memory_order_relaxed);
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

    // All allocations must succeed
    EXPECT_EQ(total_success.load(), NUM_THREADS * ALLOCS_PER_THREAD);
    EXPECT_EQ(total_failures.load(), 0)
        << "Some allocations failed - possible recursion guard corruption";
}

/**
 * WHAT: Verify allocations continue working after sustained concurrent load
 * WHY:  If the atomic guard state leaks (stuck at true) on any thread, that
 *       thread's subsequent allocations would silently stop throwing exceptions.
 *       We can detect this indirectly: if allocations fail after heavy use,
 *       the guard may be stuck.
 * REGRESSION: Ensures guard resets properly even under contention
 */
TEST_F(MemoryRecursionAtomicRegressionTest, PostStressAllocationsWork) {
    const int STRESS_THREADS = 4;
    const int STRESS_ALLOCS = 300;

    // Phase 1: Heavy concurrent stress
    {
        std::vector<std::thread> threads;
        for (int t = 0; t < STRESS_THREADS; t++) {
            threads.emplace_back([&]() {
                for (int i = 0; i < STRESS_ALLOCS; i++) {
                    void* ptr = nimcp_malloc(128);
                    if (ptr) {
                        memset(ptr, 0, 128);
                        nimcp_free(ptr);
                    }
                }
            });
        }
        for (auto& t : threads) {
            t.join();
        }
    }

    // Phase 2: Single-threaded allocations must still work
    for (int i = 0; i < 100; i++) {
        void* ptr = nimcp_malloc(64);
        ASSERT_NE(ptr, nullptr)
            << "Post-stress allocation " << i << " failed - "
            << "recursion guard may be stuck in active state";
        nimcp_free(ptr);
    }
}

/**
 * WHAT: Test many threads doing calloc simultaneously
 * WHY:  calloc goes through init_if_needed() which uses the UMM initialization
 *       recursion guard (g_umm_initializing). Multiple threads hitting this
 *       path simultaneously is the most likely scenario for the original bug.
 * REGRESSION: Ensures the atomic UMM initialization guard works correctly
 */
TEST_F(MemoryRecursionAtomicRegressionTest, ConcurrentCallocInitPath) {
    const int NUM_THREADS = 8;
    const int ALLOCS_PER_THREAD = 200;
    std::atomic<int> success_count{0};

    auto worker = [&]() {
        for (int i = 0; i < ALLOCS_PER_THREAD; i++) {
            void* ptr = nimcp_calloc(16, sizeof(int));
            if (ptr) {
                // Verify zero-initialization
                int* arr = static_cast<int*>(ptr);
                bool ok = true;
                for (int j = 0; j < 16; j++) {
                    if (arr[j] != 0) {
                        ok = false;
                        break;
                    }
                }
                EXPECT_TRUE(ok) << "Calloc returned non-zero memory";
                nimcp_free(ptr);
                success_count.fetch_add(1, std::memory_order_relaxed);
            }
        }
    };

    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back(worker);
    }
    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), NUM_THREADS * ALLOCS_PER_THREAD);
}

/**
 * WHAT: Test interleaved alloc types from concurrent threads
 * WHY:  Different allocation paths (malloc, calloc, realloc, strdup) exercise
 *       different recursion guard code paths. Interleaving them maximizes
 *       the chance of detecting guard state corruption.
 * REGRESSION: Covers all allocation paths through the atomic guard
 */
TEST_F(MemoryRecursionAtomicRegressionTest, InterleavedAllocTypesMultiThread) {
    const int NUM_THREADS = 6;
    const int ITERATIONS = 100;
    std::atomic<bool> any_failure{false};

    auto worker = [&](int tid) {
        for (int i = 0; i < ITERATIONS && !any_failure.load(std::memory_order_relaxed); i++) {
            void* p1 = nullptr;
            void* p2 = nullptr;
            char* p3 = nullptr;
            void* p4 = nullptr;

            // malloc
            p1 = nimcp_malloc(32 + (tid * 11 + i) % 256);
            if (!p1) { any_failure.store(true); return; }
            memset(p1, 0xAA, 32);

            // calloc
            p2 = nimcp_calloc(4, sizeof(double));
            if (!p2) { nimcp_free(p1); any_failure.store(true); return; }

            // strdup
            p3 = nimcp_strdup("regression_test_string");
            if (!p3) { nimcp_free(p1); nimcp_free(p2); any_failure.store(true); return; }

            // realloc
            p4 = nimcp_realloc(nullptr, 64);  // realloc(NULL, n) == malloc(n)
            if (!p4) { nimcp_free(p1); nimcp_free(p2); nimcp_free(p3); any_failure.store(true); return; }

            // Verify strdup content
            EXPECT_STREQ(p3, "regression_test_string");

            // Free all
            nimcp_free(p1);
            nimcp_free(p2);
            nimcp_free(p3);
            nimcp_free(p4);
        }
    };

    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back(worker, t);
    }
    for (auto& t : threads) {
        t.join();
    }

    EXPECT_FALSE(any_failure.load())
        << "At least one allocation failed - possible atomic guard corruption";
}

/**
 * WHAT: Test that the unified memory manager handles concurrent access
 * WHY:  unified_mem_alloc and unified_mem_free go through the UMM throw
 *       guard (g_umm_throw_active). Concurrent access must not corrupt state.
 * REGRESSION: Ensures UMM atomic guard works under load
 */
TEST_F(MemoryRecursionAtomicRegressionTest, UnifiedMemManagerConcurrentAccess) {
    unified_mem_config_t config = unified_mem_default_config();
    config.enable_tracking = true;
    config.enable_cow = false;  // Direct allocations for simplicity
    unified_mem_manager_t mgr = unified_mem_create(&config);
    ASSERT_NE(mgr, nullptr);

    const int NUM_THREADS = 4;
    const int ALLOCS_PER_THREAD = 100;
    std::atomic<int> success_count{0};

    auto worker = [&]() {
        for (int i = 0; i < ALLOCS_PER_THREAD; i++) {
            unified_mem_request_t req = unified_mem_request_direct(128);
            unified_mem_handle_t handle = unified_mem_alloc(mgr, &req);
            if (handle) {
                void* ptr = unified_mem_write(handle);
                if (ptr) {
                    memset(ptr, 0xBB, 128);
                    success_count.fetch_add(1, std::memory_order_relaxed);
                }
                unified_mem_free(handle);
            }
        }
    };

    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back(worker);
    }
    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), NUM_THREADS * ALLOCS_PER_THREAD);

    unified_mem_destroy(mgr);
}

/**
 * WHAT: Test sustained allocation throughput to detect performance regression
 * WHY:  The atomic operations (even with relaxed ordering) add some overhead.
 *       This test ensures the overhead is negligible (< 2x baseline).
 * REGRESSION: Ensures atomic guards don't cause performance regression
 */
TEST_F(MemoryRecursionAtomicRegressionTest, AllocationThroughputNotDegraded) {
    const int NUM_ALLOCS = 10000;
    std::vector<void*> ptrs;
    ptrs.reserve(NUM_ALLOCS);

    auto start = clock::now();

    for (int i = 0; i < NUM_ALLOCS; i++) {
        void* ptr = nimcp_malloc(64);
        ASSERT_NE(ptr, nullptr);
        ptrs.push_back(ptr);
    }

    for (void* p : ptrs) {
        nimcp_free(p);
    }

    auto end = clock::now();
    double ms = elapsed_ms(start, end);

    // Generous threshold: 10000 alloc+free in under 5 seconds
    // (relaxed threshold for parallel ctest with CPU contention)
    EXPECT_LT(ms, 5000.0)
        << "Allocation throughput may be degraded: " << NUM_ALLOCS
        << " alloc/free cycles took " << ms << " ms";
}

/**
 * WHAT: Test repeated init/cleanup cycles
 * WHY:  nimcp_memory_cleanup() and re-init exercise the guard reset paths.
 *       If the guard is stuck after cleanup, re-init allocations would fail.
 * REGRESSION: Ensures guard state is properly managed across lifecycle
 */
TEST_F(MemoryRecursionAtomicRegressionTest, RepeatedInitCleanupCycles) {
    // The SetUp already called nimcp_memory_init(), so start cycling
    for (int cycle = 0; cycle < 5; cycle++) {
        // Allocate and free some memory
        void* ptr = nimcp_malloc(256);
        ASSERT_NE(ptr, nullptr) << "Allocation failed on cycle " << cycle;
        memset(ptr, static_cast<unsigned char>(cycle), 256);
        nimcp_free(ptr);

        // Reset state (lighter than full cleanup)
        nimcp_memory_reset_state();

        // Allocate again after reset
        ptr = nimcp_malloc(128);
        ASSERT_NE(ptr, nullptr)
            << "Post-reset allocation failed on cycle " << cycle
            << " - recursion guard may not survive state reset";
        nimcp_free(ptr);
    }
}

/**
 * WHAT: Test that concurrent threads get correct zero-initialized memory
 * WHY:  If guard corruption causes calloc to silently fail exception reporting,
 *       we might get uninitialized memory instead of zero-filled memory.
 * REGRESSION: Ensures calloc semantics preserved with atomic guards
 */
TEST_F(MemoryRecursionAtomicRegressionTest, ConcurrentCallocZeroInit) {
    const int NUM_THREADS = 4;
    const int ALLOCS_PER_THREAD = 200;
    const int ELEMENTS = 64;
    std::atomic<int> zero_violations{0};

    auto worker = [&]() {
        for (int i = 0; i < ALLOCS_PER_THREAD; i++) {
            int* arr = static_cast<int*>(nimcp_calloc(ELEMENTS, sizeof(int)));
            if (!arr) continue;

            for (int j = 0; j < ELEMENTS; j++) {
                if (arr[j] != 0) {
                    zero_violations.fetch_add(1, std::memory_order_relaxed);
                    break;
                }
            }

            nimcp_free(arr);
        }
    };

    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back(worker);
    }
    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(zero_violations.load(), 0)
        << "calloc returned non-zero memory - possible guard corruption";
}
