//=============================================================================
// test_memory_alignment.cpp - Unit tests for nimcp_aligned_alloc
//=============================================================================
// WHAT: Comprehensive tests for aligned memory allocation
// WHY: Ensure AddressSanitizer compatibility and correctness
// HOW: Test alignment, guards, tracking, and edge cases
//
// COVERAGE:
// - Unit: Alignment validation, guard placement, zero size
// - Integration: Integration with malloc/calloc/free
// - Regression: Ensure existing allocations still work
//=============================================================================

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "utils/memory/nimcp_memory.h"

#include <cstdint>
#include <cstring>

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * WHAT: Check if pointer is aligned to boundary
 * WHY: Validate alignment guarantees
 */
static bool is_aligned(void* ptr, size_t alignment)
{
    return ((uintptr_t)ptr % alignment) == 0;
}

//=============================================================================
// Unit Tests - Alignment Validation
//=============================================================================

TEST(MemoryAlignment, AlignedAlloc_8Byte)
{
    void* ptr = nimcp_aligned_alloc(8, 128);
    ASSERT_NE(ptr, nullptr);
    EXPECT_TRUE(is_aligned(ptr, 8));
    nimcp_free(ptr);
}

TEST(MemoryAlignment, AlignedAlloc_16Byte)
{
    void* ptr = nimcp_aligned_alloc(16, 256);
    ASSERT_NE(ptr, nullptr);
    EXPECT_TRUE(is_aligned(ptr, 16));
    nimcp_free(ptr);
}

TEST(MemoryAlignment, AlignedAlloc_32Byte)
{
    void* ptr = nimcp_aligned_alloc(32, 512);
    ASSERT_NE(ptr, nullptr);
    EXPECT_TRUE(is_aligned(ptr, 32));
    nimcp_free(ptr);
}

TEST(MemoryAlignment, AlignedAlloc_64Byte)
{
    void* ptr = nimcp_aligned_alloc(64, 1024);
    ASSERT_NE(ptr, nullptr);
    EXPECT_TRUE(is_aligned(ptr, 64));
    nimcp_free(ptr);
}

//=============================================================================
// Unit Tests - Error Handling
//=============================================================================

TEST(MemoryAlignment, AlignedAlloc_ZeroSize)
{
    void* ptr = nimcp_aligned_alloc(8, 0);
    EXPECT_EQ(ptr, nullptr);
}

TEST(MemoryAlignment, AlignedAlloc_InvalidAlignment_NotPowerOf2)
{
    void* ptr = nimcp_aligned_alloc(3, 128);  // 3 is not power of 2
    EXPECT_EQ(ptr, nullptr);
}

TEST(MemoryAlignment, AlignedAlloc_InvalidAlignment_Zero)
{
    void* ptr = nimcp_aligned_alloc(0, 128);
    EXPECT_EQ(ptr, nullptr);
}

//=============================================================================
// Unit Tests - Data Integrity
//=============================================================================

TEST(MemoryAlignment, AlignedAlloc_DataWriteRead)
{
    const size_t size = 256;
    uint8_t* ptr = (uint8_t*)nimcp_aligned_alloc(16, size);
    ASSERT_NE(ptr, nullptr);

    // Write pattern
    for (size_t i = 0; i < size; i++) {
        ptr[i] = (uint8_t)(i & 0xFF);
    }

    // Verify pattern
    for (size_t i = 0; i < size; i++) {
        EXPECT_EQ(ptr[i], (uint8_t)(i & 0xFF));
    }

    nimcp_free(ptr);
}

TEST(MemoryAlignment, AlignedAlloc_StructAlignment)
{
    // Test struct requiring 8-byte alignment
    struct alignas(8) TestStruct {
        uint64_t a;
        double b;
        uint32_t c;
    };

    TestStruct* ptr = (TestStruct*)nimcp_aligned_alloc(8, sizeof(TestStruct));
    ASSERT_NE(ptr, nullptr);
    EXPECT_TRUE(is_aligned(ptr, 8));

    // Write and verify
    ptr->a = 0xDEADBEEFCAFEBABE;
    ptr->b = 3.14159;
    ptr->c = 42;

    EXPECT_EQ(ptr->a, 0xDEADBEEFCAFEBABE);
    EXPECT_DOUBLE_EQ(ptr->b, 3.14159);
    EXPECT_EQ(ptr->c, 42);

    nimcp_free(ptr);
}

//=============================================================================
// Integration Tests - Interaction with malloc/calloc
//=============================================================================

TEST(MemoryAlignment, MallocUsesAlignedAlloc)
{
    // Verify malloc provides 8-byte alignment
    void* ptr = nimcp_malloc(100);
    ASSERT_NE(ptr, nullptr);
    EXPECT_TRUE(is_aligned(ptr, 8));
    nimcp_free(ptr);
}

TEST(MemoryAlignment, CallocUsesAlignedAlloc)
{
    // Verify calloc provides 8-byte alignment
    void* ptr = nimcp_calloc(10, 10);
    ASSERT_NE(ptr, nullptr);
    EXPECT_TRUE(is_aligned(ptr, 8));

    // Verify zero-initialization
    uint8_t* bytes = (uint8_t*)ptr;
    for (size_t i = 0; i < 100; i++) {
        EXPECT_EQ(bytes[i], 0);
    }

    nimcp_free(ptr);
}

TEST(MemoryAlignment, MixedAllocations)
{
    // Allocate with different methods
    void* ptr1 = nimcp_malloc(64);
    void* ptr2 = nimcp_aligned_alloc(16, 128);
    void* ptr3 = nimcp_calloc(8, 16);

    ASSERT_NE(ptr1, nullptr);
    ASSERT_NE(ptr2, nullptr);
    ASSERT_NE(ptr3, nullptr);

    EXPECT_TRUE(is_aligned(ptr1, 8));
    EXPECT_TRUE(is_aligned(ptr2, 16));
    EXPECT_TRUE(is_aligned(ptr3, 8));

    // Free in different order
    nimcp_free(ptr2);
    nimcp_free(ptr1);
    nimcp_free(ptr3);
}

//=============================================================================
// Integration Tests - Memory Statistics
//=============================================================================

TEST(MemoryAlignment, AlignedAlloc_UpdatesStatistics)
{
    nimcp_memory_stats_t stats_before, stats_after;
    nimcp_memory_get_stats(&stats_before);

    void* ptr = nimcp_aligned_alloc(32, 512);
    ASSERT_NE(ptr, nullptr);

    nimcp_memory_get_stats(&stats_after);

    EXPECT_EQ(stats_after.allocation_count, stats_before.allocation_count + 1);
    EXPECT_GT(stats_after.current_allocated, stats_before.current_allocated);

    nimcp_free(ptr);
}

//=============================================================================
// Regression Tests - Backward Compatibility
//=============================================================================

TEST(MemoryAlignment, ExistingMallocStillWorks)
{
    // Ensure existing code using malloc still works
    for (size_t size : {1, 16, 64, 256, 1024, 4096}) {
        void* ptr = nimcp_malloc(size);
        ASSERT_NE(ptr, nullptr) << "malloc(" << size << ") failed";

        // Write some data
        memset(ptr, 0xAB, size);

        nimcp_free(ptr);
    }
}

TEST(MemoryAlignment, ExistingCallocStillWorks)
{
    // Ensure existing code using calloc still works
    for (size_t count : {1, 8, 64, 256}) {
        void* ptr = nimcp_calloc(count, 16);
        ASSERT_NE(ptr, nullptr) << "calloc(" << count << ", 16) failed";

        // Verify zeros
        uint8_t* bytes = (uint8_t*)ptr;
        for (size_t i = 0; i < count * 16; i++) {
            EXPECT_EQ(bytes[i], 0) << "calloc not zeroed at index " << i;
        }

        nimcp_free(ptr);
    }
}

TEST(MemoryAlignment, LargeAlignments)
{
    // Test alignments up to cache line size
    for (size_t alignment : {8, 16, 32, 64, 128}) {
        void* ptr = nimcp_aligned_alloc(alignment, 1024);
        ASSERT_NE(ptr, nullptr) << "aligned_alloc(" << alignment << ", 1024) failed";
        EXPECT_TRUE(is_aligned(ptr, alignment)) << "Not aligned to " << alignment;
        nimcp_free(ptr);
    }
}

//=============================================================================
// Stress Tests
//=============================================================================

TEST(MemoryAlignment, ManyAlignedAllocations)
{
    const size_t num_allocs = 1000;
    void* ptrs[num_allocs];

    // Allocate many aligned pointers
    for (size_t i = 0; i < num_allocs; i++) {
        size_t alignment = 8 << (i % 4);  // 8, 16, 32, 64
        size_t size = 64 + (i % 512);
        ptrs[i] = nimcp_aligned_alloc(alignment, size);
        ASSERT_NE(ptrs[i], nullptr) << "Allocation " << i << " failed";
        EXPECT_TRUE(is_aligned(ptrs[i], alignment));
    }

    // Free all
    for (size_t i = 0; i < num_allocs; i++) {
        nimcp_free(ptrs[i]);
    }
}
