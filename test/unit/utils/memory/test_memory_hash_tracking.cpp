/**
 * @file test_memory_hash_tracking.cpp
 * @brief Unit tests for hash-table-based memory tracking
 *
 * WHAT: Validate the hash table tracking in nimcp_memory.c
 * WHY: The linked list was replaced with a hash table to handle >100K allocations
 * HOW: Exercise alloc/free paths and verify stats remain consistent
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>
#include <thread>
#include <atomic>

extern "C" {
#include "utils/memory/nimcp_memory.h"
}

class MemoryHashTrackingTest : public ::testing::Test {
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

TEST_F(MemoryHashTrackingTest, InsertAndLookup)
{
    std::vector<void*> ptrs;
    for (int i = 0; i < 10; i++) {
        void* p = nimcp_malloc(64);
        ASSERT_NE(p, nullptr);
        ptrs.push_back(p);
    }

    nimcp_memory_stats_t stats;
    ASSERT_TRUE(nimcp_memory_get_stats(&stats));
    EXPECT_GE(stats.allocation_count, 10u);

    for (void* p : ptrs) {
        nimcp_free(p);
    }
}

TEST_F(MemoryHashTrackingTest, RemoveAndVerify)
{
    std::vector<void*> ptrs;
    for (int i = 0; i < 10; i++) {
        void* p = nimcp_malloc(128);
        ASSERT_NE(p, nullptr);
        ptrs.push_back(p);
    }

    // Free first 5
    for (int i = 0; i < 5; i++) {
        nimcp_free(ptrs[i]);
    }

    nimcp_memory_stats_t stats;
    ASSERT_TRUE(nimcp_memory_get_stats(&stats));
    EXPECT_EQ(stats.free_count, 5u);

    // Free remaining
    for (int i = 5; i < 10; i++) {
        nimcp_free(ptrs[i]);
    }
}

TEST_F(MemoryHashTrackingTest, LargeScaleInsert)
{
    const size_t N = 50000;
    std::vector<void*> ptrs;
    ptrs.reserve(N);

    for (size_t i = 0; i < N; i++) {
        void* p = nimcp_malloc(32);
        ASSERT_NE(p, nullptr) << "Failed at allocation " << i;
        ptrs.push_back(p);
    }

    nimcp_memory_stats_t stats;
    ASSERT_TRUE(nimcp_memory_get_stats(&stats));
    EXPECT_GE(stats.allocation_count, N);

    for (void* p : ptrs) {
        nimcp_free(p);
    }

    ASSERT_TRUE(nimcp_memory_get_stats(&stats));
    EXPECT_EQ(stats.current_allocated, 0u);
}

TEST_F(MemoryHashTrackingTest, Over100KAllocations)
{
    // The original crash scenario: >100K allocations caused MAX_ITERATIONS overflow
    const size_t N = 120000;
    std::vector<void*> ptrs;
    ptrs.reserve(N);

    for (size_t i = 0; i < N; i++) {
        void* p = nimcp_malloc(16);
        if (!p) {
            // OOM is acceptable for this test — just verify no crash
            break;
        }
        ptrs.push_back(p);
    }

    // Free all — the critical test: no munmap_chunk crash
    for (void* p : ptrs) {
        nimcp_free(p);
    }

    nimcp_memory_stats_t stats;
    ASSERT_TRUE(nimcp_memory_get_stats(&stats));
    EXPECT_EQ(stats.current_allocated, 0u);
}

TEST_F(MemoryHashTrackingTest, ResetStateClearsAll)
{
    for (int i = 0; i < 100; i++) {
        void* p = nimcp_malloc(64);
        ASSERT_NE(p, nullptr);
        // Intentionally leak — reset_state clears tracking
    }

    nimcp_memory_stats_t stats;
    ASSERT_TRUE(nimcp_memory_get_stats(&stats));
    EXPECT_GE(stats.allocation_count, 100u);

    nimcp_memory_reset_state();

    ASSERT_TRUE(nimcp_memory_get_stats(&stats));
    EXPECT_EQ(stats.allocation_count, 0u);
    EXPECT_EQ(stats.current_allocated, 0u);
}

TEST_F(MemoryHashTrackingTest, ReInitAfterCleanup)
{
    void* p1 = nimcp_malloc(64);
    ASSERT_NE(p1, nullptr);
    nimcp_free(p1);

    nimcp_memory_cleanup();

    // Re-initialize and use again — should not crash
    nimcp_memory_init();
    nimcp_memory_enable_tracking(true);

    void* p2 = nimcp_malloc(128);
    ASSERT_NE(p2, nullptr);
    nimcp_free(p2);
}

TEST_F(MemoryHashTrackingTest, StatsAccuracyUnderChurn)
{
    const size_t N = 10000;

    for (size_t i = 0; i < N; i++) {
        void* p = nimcp_malloc(32);
        ASSERT_NE(p, nullptr);
        nimcp_free(p);
    }

    nimcp_memory_stats_t stats;
    ASSERT_TRUE(nimcp_memory_get_stats(&stats));
    EXPECT_GE(stats.allocation_count, N);
    EXPECT_GE(stats.free_count, N);
    EXPECT_EQ(stats.current_allocated, 0u);
}

TEST_F(MemoryHashTrackingTest, MultiThreadSafety)
{
    const int NUM_THREADS = 4;
    const int OPS_PER_THREAD = 1000;
    std::atomic<int> errors{0};

    auto worker = [&]() {
        for (int i = 0; i < OPS_PER_THREAD; i++) {
            void* p = nimcp_malloc(64);
            if (!p) {
                errors++;
                continue;
            }
            memset(p, 0xAB, 64);
            nimcp_free(p);
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(worker);
    }
    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(errors.load(), 0);

    nimcp_memory_stats_t stats;
    ASSERT_TRUE(nimcp_memory_get_stats(&stats));
    EXPECT_EQ(stats.current_allocated, 0u);
}

TEST_F(MemoryHashTrackingTest, GuardSizePreserved)
{
    // Allocate and free — guard_size lookup must succeed (no "ptr NOT FOUND" warnings)
    void* p = nimcp_malloc(256);
    ASSERT_NE(p, nullptr);
    memset(p, 0xCC, 256);
    nimcp_free(p);  // This calls get_guard_size internally — must not print warning
}

TEST_F(MemoryHashTrackingTest, CallocAndReallocTracked)
{
    // calloc
    int* arr = static_cast<int*>(nimcp_calloc(10, sizeof(int)));
    ASSERT_NE(arr, nullptr);
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(arr[i], 0);
    }

    // realloc
    arr = static_cast<int*>(nimcp_realloc(arr, 20 * sizeof(int)));
    ASSERT_NE(arr, nullptr);

    nimcp_memory_stats_t stats;
    ASSERT_TRUE(nimcp_memory_get_stats(&stats));
    EXPECT_GT(stats.allocation_count, 0u);

    nimcp_free(arr);
}
