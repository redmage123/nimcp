/**
 * @file test_memory_safety.cpp
 * @brief Tests for nimcp_malloc/nimcp_free migration (P2-1)
 *
 * WHAT: Verify memory allocation wrappers work correctly
 * WHY:  P2-1 migrated raw malloc/free to nimcp_malloc/nimcp_free
 * HOW:  Test allocation, deallocation, zeroing, resizing, and statistics
 *
 * Function signatures tested (from include/utils/memory/nimcp_memory.h):
 *   void* nimcp_malloc(size_t size);
 *   void* nimcp_calloc(size_t count, size_t size);
 *   void* nimcp_realloc(void* ptr, size_t new_size);
 *   void  nimcp_free(void* ptr);
 *   void  nimcp_memory_init(void);
 *   void  nimcp_memory_cleanup(void);
 *   void  nimcp_memory_enable_tracking(bool enable);
 *   bool  nimcp_memory_get_stats(nimcp_memory_stats_t* stats);
 *   void  nimcp_memory_clear_stats(void);
 *   void  nimcp_memory_reset_state(void);
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "utils/memory/nimcp_memory.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class MemorySafetyTest : public ::testing::Test {
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
 * nimcp_malloc Tests
 * ============================================================================ */

TEST_F(MemorySafetyTest, MallocReturnsNonNullForValidSize) {
    void* ptr = nimcp_malloc(256);
    ASSERT_NE(ptr, nullptr);

    // Verify the memory is writable
    memset(ptr, 0xAB, 256);

    nimcp_free(ptr);
}

TEST_F(MemorySafetyTest, MallocSmallAllocation) {
    void* ptr = nimcp_malloc(1);
    ASSERT_NE(ptr, nullptr);

    unsigned char* byte = static_cast<unsigned char*>(ptr);
    *byte = 0xFF;
    EXPECT_EQ(*byte, 0xFF);

    nimcp_free(ptr);
}

TEST_F(MemorySafetyTest, MallocLargeAllocation) {
    size_t large_size = 1024 * 1024; // 1 MB
    void* ptr = nimcp_malloc(large_size);
    ASSERT_NE(ptr, nullptr);

    // Write to first and last byte to verify full range
    unsigned char* bytes = static_cast<unsigned char*>(ptr);
    bytes[0] = 0xAA;
    bytes[large_size - 1] = 0xBB;
    EXPECT_EQ(bytes[0], 0xAA);
    EXPECT_EQ(bytes[large_size - 1], 0xBB);

    nimcp_free(ptr);
}

/* ============================================================================
 * nimcp_free Tests
 * ============================================================================ */

TEST_F(MemorySafetyTest, FreeNullDoesNotCrash) {
    // This should be a no-op, not crash
    nimcp_free(nullptr);
    SUCCEED() << "nimcp_free(NULL) did not crash";
}

TEST_F(MemorySafetyTest, FreeValidPointer) {
    void* ptr = nimcp_malloc(64);
    ASSERT_NE(ptr, nullptr);

    nimcp_free(ptr);
    SUCCEED() << "nimcp_free on valid pointer succeeded";
}

/* ============================================================================
 * nimcp_calloc Tests
 * ============================================================================ */

TEST_F(MemorySafetyTest, CallocZeroesMemory) {
    size_t count = 100;
    size_t elem_size = sizeof(int);

    int* arr = static_cast<int*>(nimcp_calloc(count, elem_size));
    ASSERT_NE(arr, nullptr);

    // Verify all elements are zero
    for (size_t i = 0; i < count; i++) {
        EXPECT_EQ(arr[i], 0) << "Element " << i << " was not zero-initialized";
    }

    nimcp_free(arr);
}

TEST_F(MemorySafetyTest, CallocSingleElement) {
    double* val = static_cast<double*>(nimcp_calloc(1, sizeof(double)));
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, 0.0);

    nimcp_free(val);
}

TEST_F(MemorySafetyTest, CallocLargeArray) {
    size_t count = 10000;
    unsigned char* buf = static_cast<unsigned char*>(nimcp_calloc(count, 1));
    ASSERT_NE(buf, nullptr);

    // Spot-check zeroing
    EXPECT_EQ(buf[0], 0);
    EXPECT_EQ(buf[count / 2], 0);
    EXPECT_EQ(buf[count - 1], 0);

    nimcp_free(buf);
}

/* ============================================================================
 * nimcp_realloc Tests
 * ============================================================================ */

TEST_F(MemorySafetyTest, ReallocPreservesContent) {
    size_t initial_size = 64;
    size_t new_size = 256;

    unsigned char* ptr = static_cast<unsigned char*>(nimcp_malloc(initial_size));
    ASSERT_NE(ptr, nullptr);

    // Fill with known pattern
    for (size_t i = 0; i < initial_size; i++) {
        ptr[i] = static_cast<unsigned char>(i & 0xFF);
    }

    // Realloc to larger size
    unsigned char* new_ptr = static_cast<unsigned char*>(
        nimcp_realloc(ptr, new_size)
    );
    ASSERT_NE(new_ptr, nullptr);

    // Verify original content is preserved
    for (size_t i = 0; i < initial_size; i++) {
        EXPECT_EQ(new_ptr[i], static_cast<unsigned char>(i & 0xFF))
            << "Content at index " << i << " was not preserved after realloc";
    }

    nimcp_free(new_ptr);
}

TEST_F(MemorySafetyTest, ReallocNullActsAsMalloc) {
    // realloc(NULL, size) should behave like malloc(size)
    void* ptr = nimcp_realloc(nullptr, 128);
    ASSERT_NE(ptr, nullptr);

    memset(ptr, 0xCC, 128);
    nimcp_free(ptr);
}

TEST_F(MemorySafetyTest, ReallocToSmallerSize) {
    size_t initial_size = 256;
    size_t smaller_size = 32;

    unsigned char* ptr = static_cast<unsigned char*>(nimcp_malloc(initial_size));
    ASSERT_NE(ptr, nullptr);

    // Fill initial portion with pattern
    for (size_t i = 0; i < smaller_size; i++) {
        ptr[i] = static_cast<unsigned char>(i);
    }

    unsigned char* new_ptr = static_cast<unsigned char*>(
        nimcp_realloc(ptr, smaller_size)
    );
    ASSERT_NE(new_ptr, nullptr);

    // Verify content preserved in smaller region
    for (size_t i = 0; i < smaller_size; i++) {
        EXPECT_EQ(new_ptr[i], static_cast<unsigned char>(i));
    }

    nimcp_free(new_ptr);
}

/* ============================================================================
 * Allocation/Deallocation Cycle Tests
 * ============================================================================ */

TEST_F(MemorySafetyTest, AllocationDeallocationCycle) {
    const int num_iterations = 100;
    void* ptrs[100];

    // Allocate all
    for (int i = 0; i < num_iterations; i++) {
        ptrs[i] = nimcp_malloc(static_cast<size_t>(i + 1) * 16);
        ASSERT_NE(ptrs[i], nullptr) << "Allocation " << i << " failed";
    }

    // Free all
    for (int i = 0; i < num_iterations; i++) {
        nimcp_free(ptrs[i]);
    }

    SUCCEED() << "All allocations and deallocations completed";
}

TEST_F(MemorySafetyTest, InterleavedAllocFree) {
    // Allocate and free in interleaved pattern
    void* a = nimcp_malloc(100);
    void* b = nimcp_malloc(200);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    nimcp_free(a);

    void* c = nimcp_malloc(150);
    ASSERT_NE(c, nullptr);

    nimcp_free(b);
    nimcp_free(c);

    SUCCEED() << "Interleaved alloc/free pattern completed";
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(MemorySafetyTest, StatsTrackAllocations) {
    nimcp_memory_stats_t stats;

    void* ptr = nimcp_malloc(100);
    ASSERT_NE(ptr, nullptr);

    bool got_stats = nimcp_memory_get_stats(&stats);
    EXPECT_TRUE(got_stats);
    EXPECT_GE(stats.allocation_count, 1u);
    EXPECT_GE(stats.current_allocated, 100u);

    nimcp_free(ptr);

    got_stats = nimcp_memory_get_stats(&stats);
    EXPECT_TRUE(got_stats);
    EXPECT_GE(stats.free_count, 1u);
}

TEST_F(MemorySafetyTest, StatsNullReturnsError) {
    bool result = nimcp_memory_get_stats(nullptr);
    EXPECT_FALSE(result);
}
