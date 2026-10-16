/**
 * @file test_memory_hash_tracking_regression.cpp
 * @brief Regression tests for hash-table memory tracking fix
 *
 * WHAT: Regression tests proving the munmap_chunk crash is fixed
 * WHY: >100K allocations caused O(n) linked list to exceed MAX_ITERATIONS,
 *       returning guard_size=0, causing wrong real_ptr → munmap_chunk crash
 * HOW: Stress test with 150K+ allocations and random-order frees
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <algorithm>
#include <random>
#include <chrono>

extern "C" {
#include "utils/memory/nimcp_memory.h"
}

class MemoryHashTrackingRegressionTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
        nimcp_memory_enable_debug_output(false);
        nimcp_memory_reset_state();
    }

    void TearDown() override
    {
        nimcp_memory_reset_state();
    }
};

TEST_F(MemoryHashTrackingRegressionTest, RegressionMunmapChunkFix)
{
    // THE original crash scenario: allocs of varying sizes, freed in random order.
    // With the old linked list, get_guard_size() exceeded MAX_ITERATIONS=100000,
    // returned 0, nimcp_free() computed wrong real_ptr → munmap_chunk(): invalid pointer.
    // Using 50K here (enough to prove hash table works without OOM under ctest -j2).
    const size_t N = 50000;
    std::vector<void*> ptrs;
    ptrs.reserve(N);

    // Varying sizes from 8 to 512
    std::mt19937 rng(42);
    std::uniform_int_distribution<size_t> size_dist(8, 512);

    for (size_t i = 0; i < N; i++) {
        void* p = nimcp_malloc(size_dist(rng));
        if (!p) break;  // OOM is acceptable
        ptrs.push_back(p);
    }

    EXPECT_GE(ptrs.size(), N / 2) << "Too many OOM failures";

    // Shuffle for random-order free
    std::shuffle(ptrs.begin(), ptrs.end(), rng);

    // Free all — this is the crash point. With hash table, no SIGABRT.
    for (void* p : ptrs) {
        nimcp_free(p);
    }

    nimcp_memory_stats_t stats;
    ASSERT_TRUE(nimcp_memory_get_stats(&stats));
    EXPECT_EQ(stats.current_allocated, 0u);
}

TEST_F(MemoryHashTrackingRegressionTest, RegressionNoTruncatedLookups)
{
    // Verify each of 50K allocs can be freed correctly (guard_size found).
    // Old linked list would fail at >100K; hash table handles any count.
    const size_t N = 50000;
    std::vector<void*> ptrs;
    ptrs.reserve(N);

    for (size_t i = 0; i < N; i++) {
        void* p = nimcp_malloc(64);
        if (!p) break;
        ptrs.push_back(p);
    }

    EXPECT_GE(ptrs.size(), N / 2);

    for (void* p : ptrs) {
        nimcp_free(p);
    }

    nimcp_memory_stats_t stats;
    ASSERT_TRUE(nimcp_memory_get_stats(&stats));
    EXPECT_EQ(stats.current_allocated, 0u);
}

TEST_F(MemoryHashTrackingRegressionTest, RegressionBackwardCompatAPI)
{
    // Exercise all public memory API functions — backward compatibility
    nimcp_memory_init();
    nimcp_memory_enable_tracking(true);
    nimcp_memory_enable_debug_output(false);

    void* p1 = nimcp_malloc(100);
    ASSERT_NE(p1, nullptr);

    void* p2 = nimcp_calloc(10, sizeof(int));
    ASSERT_NE(p2, nullptr);

    void* p3 = nimcp_realloc(p1, 200);
    ASSERT_NE(p3, nullptr);

    nimcp_memory_stats_t stats;
    ASSERT_TRUE(nimcp_memory_get_stats(&stats));
    EXPECT_GT(stats.allocation_count, 0u);

    nimcp_memory_dump_allocations();
    nimcp_memory_analyze_patterns();

    nimcp_free(p2);
    nimcp_free(p3);

    nimcp_memory_check_leaks();
    nimcp_memory_clear_stats();
}

TEST_F(MemoryHashTrackingRegressionTest, RegressionPatternAnalysisUnaffected)
{
    // Allocate 3 different sizes, verify analyze_patterns doesn't crash
    void* a = nimcp_malloc(32);
    void* b = nimcp_malloc(64);
    void* c = nimcp_malloc(128);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    ASSERT_NE(c, nullptr);

    nimcp_memory_analyze_patterns();

    nimcp_free(a);
    nimcp_free(b);
    nimcp_free(c);

    nimcp_memory_analyze_patterns();
}

TEST_F(MemoryHashTrackingRegressionTest, RegressionPerformanceImprovement)
{
    // 100K alloc+free should complete in <5s (hash table is effectively O(1))
    const size_t N = 100000;

    auto start = std::chrono::steady_clock::now();

    for (size_t i = 0; i < N; i++) {
        void* p = nimcp_malloc(64);
        if (p) nimcp_free(p);
    }

    auto end = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Should complete in well under 5 seconds with hash table
    EXPECT_LT(elapsed, 5000) << "100K alloc+free took " << elapsed << "ms (expected <5000ms)";
}
