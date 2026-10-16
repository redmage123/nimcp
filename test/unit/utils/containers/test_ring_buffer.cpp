/**
 * @file test_ring_buffer.cpp
 * @brief Comprehensive unit tests for fixed-size ring buffer container
 *
 * WHAT: 100% test coverage for nimcp_ring_buffer.c
 * WHY:  Ring buffer is used for trajectory history - must be bulletproof
 * HOW:  Test all operations, edge cases, memory management, and API contracts
 *
 * TEST COVERAGE:
 * 1. Creation and destruction (valid params, NULL handling)
 * 2. Push operations with wraparound
 * 3. Pop operations (front, back)
 * 4. Random access (at, front, back, peek_from_back)
 * 5. Overwrite behavior when full
 * 6. Iteration (foreach, foreach_reverse)
 * 7. Custom destructor behavior
 * 8. Utility functions (copy_last_n)
 * 9. Edge cases (empty, single element, full)
 *
 * @version Unit Testing Framework v1.0
 * @date 2026-01-02
 */

#include <gtest/gtest.h>
#include <vector>
#include <cstring>

// Headers have their own extern "C" guards
#include "utils/containers/nimcp_ring_buffer.h"

//=============================================================================
// Test Fixture
//=============================================================================

class RingBufferTest : public ::testing::Test {
protected:
    nimcp_ring_buffer_t* rb;

    void SetUp() override {
        rb = nullptr;
    }

    void TearDown() override {
        if (rb) {
            nimcp_ring_buffer_destroy(rb);
            rb = nullptr;
        }
    }

    // Helper: Create ring buffer of integers
    nimcp_ring_buffer_t* create_int_buffer(size_t capacity = 8) {
        return nimcp_ring_buffer_create(sizeof(int), capacity);
    }

    // Helper: Push integer values
    void push_int_values(nimcp_ring_buffer_t* r, const std::vector<int>& values) {
        for (int v : values) {
            nimcp_ring_buffer_push(r, &v);
        }
    }

    // Helper: Get integer at index
    int get_int_at(nimcp_ring_buffer_t* r, size_t idx) {
        int* ptr = (int*)nimcp_ring_buffer_at(r, idx);
        return ptr ? *ptr : -999999;
    }
};

//=============================================================================
// Test struct for complex element types
//=============================================================================

struct TestStruct {
    int id;
    float value;
    char name[32];
};

// Counter for destructor calls
static int g_destructor_calls = 0;

static void test_destructor(void* element) {
    (void)element;
    g_destructor_calls++;
}

// Iterator context for testing
struct IteratorContext {
    std::vector<int> collected;
    int stop_at_index;
};

static bool test_iterator(void* element, size_t index, void* context) {
    IteratorContext* ctx = (IteratorContext*)context;
    int val = *(int*)element;
    ctx->collected.push_back(val);
    if (ctx->stop_at_index >= 0 && (int)index == ctx->stop_at_index) {
        return false;  // Stop iteration
    }
    return true;  // Continue
}

//=============================================================================
// Unit Test 1: Creation with valid capacity
//=============================================================================

TEST_F(RingBufferTest, Create_ValidCapacity) {
    // WHAT: Verify ring buffer creation with valid capacity
    // WHY:  Must allocate and initialize properly
    // HOW:  Create ring buffer and verify state

    rb = create_int_buffer(16);
    ASSERT_NE(rb, nullptr);
    EXPECT_EQ(nimcp_ring_buffer_size(rb), 0u);
    EXPECT_EQ(nimcp_ring_buffer_capacity(rb), 16u);
    EXPECT_EQ(nimcp_ring_buffer_element_size(rb), sizeof(int));
    EXPECT_TRUE(nimcp_ring_buffer_is_empty(rb));
    EXPECT_FALSE(nimcp_ring_buffer_is_full(rb));
}

//=============================================================================
// Unit Test 2: Creation with zero capacity uses default
//=============================================================================

TEST_F(RingBufferTest, Create_ZeroCapacity_UsesDefault) {
    // WHAT: Zero capacity should use default
    // WHY:  Provide sensible default behavior
    // HOW:  Create with 0, verify capacity is default (64)

    rb = nimcp_ring_buffer_create(sizeof(int), 0);
    ASSERT_NE(rb, nullptr);
    EXPECT_EQ(nimcp_ring_buffer_capacity(rb), NIMCP_RING_BUFFER_DEFAULT_CAPACITY);
}

//=============================================================================
// Unit Test 3: Creation with zero element size fails
//=============================================================================

TEST_F(RingBufferTest, Create_ZeroElementSize_Fails) {
    // WHAT: Zero element size should fail
    // WHY:  Cannot have elements of size 0
    // HOW:  Attempt creation with element_size=0

    rb = nimcp_ring_buffer_create(0, 16);
    EXPECT_EQ(rb, nullptr);
}

//=============================================================================
// Unit Test 4: Creation with excessive capacity fails
//=============================================================================

TEST_F(RingBufferTest, Create_ExcessiveCapacity_Fails) {
    // WHAT: Capacity beyond max should fail
    // WHY:  Prevent unreasonable memory allocation
    // HOW:  Create with capacity > max

    rb = nimcp_ring_buffer_create(sizeof(int), NIMCP_RING_BUFFER_MAX_CAPACITY + 1);
    EXPECT_EQ(rb, nullptr);
}

//=============================================================================
// Unit Test 5: Destroy NULL is safe
//=============================================================================

TEST_F(RingBufferTest, Destroy_Null_Safe) {
    // WHAT: Destroying NULL should not crash
    // WHY:  Safety for cleanup code paths
    // HOW:  Call destroy with NULL

    nimcp_ring_buffer_destroy(nullptr);  // Should not crash
    SUCCEED();
}

//=============================================================================
// Unit Test 6: Push increases size
//=============================================================================

TEST_F(RingBufferTest, Push_IncreasesSize) {
    // WHAT: Push should increase size
    // WHY:  Core append functionality
    // HOW:  Push elements and verify size increases

    rb = create_int_buffer(8);
    ASSERT_NE(rb, nullptr);

    int val1 = 10, val2 = 20, val3 = 30;

    EXPECT_TRUE(nimcp_ring_buffer_push(rb, &val1));
    EXPECT_EQ(nimcp_ring_buffer_size(rb), 1u);
    EXPECT_FALSE(nimcp_ring_buffer_is_empty(rb));

    EXPECT_TRUE(nimcp_ring_buffer_push(rb, &val2));
    EXPECT_EQ(nimcp_ring_buffer_size(rb), 2u);

    EXPECT_TRUE(nimcp_ring_buffer_push(rb, &val3));
    EXPECT_EQ(nimcp_ring_buffer_size(rb), 3u);
}

//=============================================================================
// Unit Test 7: Push stores correct values
//=============================================================================

TEST_F(RingBufferTest, Push_StoresCorrectValues) {
    // WHAT: Values should be retrievable after push
    // WHY:  Core data storage functionality
    // HOW:  Push values, verify via at()

    rb = create_int_buffer(8);
    push_int_values(rb, {100, 200, 300, 400, 500});

    EXPECT_EQ(get_int_at(rb, 0), 100);  // Oldest
    EXPECT_EQ(get_int_at(rb, 1), 200);
    EXPECT_EQ(get_int_at(rb, 2), 300);
    EXPECT_EQ(get_int_at(rb, 3), 400);
    EXPECT_EQ(get_int_at(rb, 4), 500);  // Newest
}

//=============================================================================
// Unit Test 8: Push NULL element fails
//=============================================================================

TEST_F(RingBufferTest, Push_NullElement_Fails) {
    // WHAT: Push NULL element should fail
    // WHY:  Cannot copy from NULL
    // HOW:  Attempt push with NULL element

    rb = create_int_buffer();
    EXPECT_FALSE(nimcp_ring_buffer_push(rb, nullptr));
    EXPECT_EQ(nimcp_ring_buffer_size(rb), 0u);
}

//=============================================================================
// Unit Test 9: Push NULL buffer fails
//=============================================================================

TEST_F(RingBufferTest, Push_NullBuffer_Fails) {
    // WHAT: Push to NULL buffer should fail
    // WHY:  Safety for invalid parameters
    // HOW:  Call push on NULL buffer

    int val = 42;
    EXPECT_FALSE(nimcp_ring_buffer_push(nullptr, &val));
}

//=============================================================================
// Unit Test 10: Push overwrites oldest when full
//=============================================================================

TEST_F(RingBufferTest, Push_OverwritesOldest_WhenFull) {
    // WHAT: Push to full buffer should overwrite oldest
    // WHY:  Ring buffer semantics - fixed size, circular
    // HOW:  Fill buffer, push more, verify oldest replaced

    rb = create_int_buffer(4);
    push_int_values(rb, {10, 20, 30, 40});

    EXPECT_TRUE(nimcp_ring_buffer_is_full(rb));
    EXPECT_EQ(nimcp_ring_buffer_size(rb), 4u);

    // Push one more - should overwrite 10
    int val = 50;
    EXPECT_TRUE(nimcp_ring_buffer_push(rb, &val));

    EXPECT_EQ(nimcp_ring_buffer_size(rb), 4u);  // Still 4
    EXPECT_EQ(get_int_at(rb, 0), 20);  // Now oldest
    EXPECT_EQ(get_int_at(rb, 1), 30);
    EXPECT_EQ(get_int_at(rb, 2), 40);
    EXPECT_EQ(get_int_at(rb, 3), 50);  // Newest

    // Push more - should overwrite 20
    val = 60;
    nimcp_ring_buffer_push(rb, &val);
    EXPECT_EQ(get_int_at(rb, 0), 30);  // Now oldest
    EXPECT_EQ(get_int_at(rb, 3), 60);  // Newest
}

//=============================================================================
// Unit Test 11: Front returns oldest element
//=============================================================================

TEST_F(RingBufferTest, Front_ReturnsOldestElement) {
    // WHAT: front() should return oldest element
    // WHY:  FIFO access pattern
    // HOW:  Push values, verify front()

    rb = create_int_buffer(8);
    push_int_values(rb, {100, 200, 300});

    int* front = (int*)nimcp_ring_buffer_front(rb);
    ASSERT_NE(front, nullptr);
    EXPECT_EQ(*front, 100);
}

//=============================================================================
// Unit Test 12: Back returns newest element
//=============================================================================

TEST_F(RingBufferTest, Back_ReturnsNewestElement) {
    // WHAT: back() should return newest element
    // WHY:  Stack-like access
    // HOW:  Push values, verify back()

    rb = create_int_buffer(8);
    push_int_values(rb, {100, 200, 300});

    int* back = (int*)nimcp_ring_buffer_back(rb);
    ASSERT_NE(back, nullptr);
    EXPECT_EQ(*back, 300);

    // Push more, verify back updates
    int val = 400;
    nimcp_ring_buffer_push(rb, &val);
    back = (int*)nimcp_ring_buffer_back(rb);
    EXPECT_EQ(*back, 400);
}

//=============================================================================
// Unit Test 13: Front/back on empty returns NULL
//=============================================================================

TEST_F(RingBufferTest, FrontBack_Empty_ReturnsNull) {
    // WHAT: front()/back() on empty should return NULL
    // WHY:  No elements to access
    // HOW:  Call on empty buffer

    rb = create_int_buffer();
    EXPECT_EQ(nimcp_ring_buffer_front(rb), nullptr);
    EXPECT_EQ(nimcp_ring_buffer_back(rb), nullptr);
}

//=============================================================================
// Unit Test 14: At returns correct pointer
//=============================================================================

TEST_F(RingBufferTest, At_ReturnsCorrectPointer) {
    // WHAT: at() should return valid pointer to element
    // WHY:  Random access is core functionality
    // HOW:  Push values, access via at()

    rb = create_int_buffer(8);
    push_int_values(rb, {100, 200, 300});

    int* ptr0 = (int*)nimcp_ring_buffer_at(rb, 0);
    int* ptr1 = (int*)nimcp_ring_buffer_at(rb, 1);
    int* ptr2 = (int*)nimcp_ring_buffer_at(rb, 2);

    ASSERT_NE(ptr0, nullptr);
    ASSERT_NE(ptr1, nullptr);
    ASSERT_NE(ptr2, nullptr);

    EXPECT_EQ(*ptr0, 100);
    EXPECT_EQ(*ptr1, 200);
    EXPECT_EQ(*ptr2, 300);
}

//=============================================================================
// Unit Test 15: At out of bounds returns NULL
//=============================================================================

TEST_F(RingBufferTest, At_OutOfBounds_ReturnsNull) {
    // WHAT: at() with invalid index should return NULL
    // WHY:  Bounds checking prevents crashes
    // HOW:  Access with invalid indices

    rb = create_int_buffer(8);
    push_int_values(rb, {10, 20, 30});

    EXPECT_EQ(nimcp_ring_buffer_at(rb, 3), nullptr);
    EXPECT_EQ(nimcp_ring_buffer_at(rb, 100), nullptr);
    EXPECT_EQ(nimcp_ring_buffer_at(rb, SIZE_MAX), nullptr);
}

//=============================================================================
// Unit Test 16: At on empty buffer returns NULL
//=============================================================================

TEST_F(RingBufferTest, At_EmptyBuffer_ReturnsNull) {
    // WHAT: at() on empty buffer should return NULL
    // WHY:  No elements to access
    // HOW:  Create empty buffer, try at(0)

    rb = create_int_buffer();
    EXPECT_EQ(nimcp_ring_buffer_at(rb, 0), nullptr);
}

//=============================================================================
// Unit Test 17: Pop front removes oldest
//=============================================================================

TEST_F(RingBufferTest, PopFront_RemovesOldest) {
    // WHAT: pop_front should remove oldest element
    // WHY:  FIFO removal
    // HOW:  Push elements, pop front, verify

    rb = create_int_buffer(8);
    push_int_values(rb, {10, 20, 30});
    EXPECT_EQ(nimcp_ring_buffer_size(rb), 3u);

    int out;
    EXPECT_TRUE(nimcp_ring_buffer_pop_front(rb, &out));
    EXPECT_EQ(out, 10);
    EXPECT_EQ(nimcp_ring_buffer_size(rb), 2u);

    EXPECT_TRUE(nimcp_ring_buffer_pop_front(rb, &out));
    EXPECT_EQ(out, 20);
    EXPECT_EQ(nimcp_ring_buffer_size(rb), 1u);

    EXPECT_TRUE(nimcp_ring_buffer_pop_front(rb, &out));
    EXPECT_EQ(out, 30);
    EXPECT_EQ(nimcp_ring_buffer_size(rb), 0u);
    EXPECT_TRUE(nimcp_ring_buffer_is_empty(rb));
}

//=============================================================================
// Unit Test 18: Pop back removes newest
//=============================================================================

TEST_F(RingBufferTest, PopBack_RemovesNewest) {
    // WHAT: pop_back should remove newest element
    // WHY:  LIFO removal
    // HOW:  Push elements, pop back, verify

    rb = create_int_buffer(8);
    push_int_values(rb, {10, 20, 30});

    int out;
    EXPECT_TRUE(nimcp_ring_buffer_pop_back(rb, &out));
    EXPECT_EQ(out, 30);
    EXPECT_EQ(nimcp_ring_buffer_size(rb), 2u);

    EXPECT_TRUE(nimcp_ring_buffer_pop_back(rb, &out));
    EXPECT_EQ(out, 20);
    EXPECT_EQ(nimcp_ring_buffer_size(rb), 1u);
}

//=============================================================================
// Unit Test 19: Pop from empty fails
//=============================================================================

TEST_F(RingBufferTest, Pop_EmptyBuffer_Fails) {
    // WHAT: Pop from empty buffer should fail
    // WHY:  Cannot pop when no elements exist
    // HOW:  Pop from empty buffer

    rb = create_int_buffer();
    int out;
    EXPECT_FALSE(nimcp_ring_buffer_pop_front(rb, &out));
    EXPECT_FALSE(nimcp_ring_buffer_pop_back(rb, &out));
}

//=============================================================================
// Unit Test 20: Pop with NULL output succeeds
//=============================================================================

TEST_F(RingBufferTest, Pop_NullOutput_Succeeds) {
    // WHAT: Pop with NULL output should still remove element
    // WHY:  Allow discard of element without copying
    // HOW:  Pop with NULL output

    rb = create_int_buffer(8);
    push_int_values(rb, {10, 20, 30});

    EXPECT_TRUE(nimcp_ring_buffer_pop_front(rb, nullptr));
    EXPECT_EQ(nimcp_ring_buffer_size(rb), 2u);

    EXPECT_TRUE(nimcp_ring_buffer_pop_back(rb, nullptr));
    EXPECT_EQ(nimcp_ring_buffer_size(rb), 1u);
}

//=============================================================================
// Unit Test 21: Peek from back
//=============================================================================

TEST_F(RingBufferTest, PeekFromBack_ReturnsCorrect) {
    // WHAT: peek_from_back should return N-th newest element
    // WHY:  Access recent history
    // HOW:  Push values, peek from back

    rb = create_int_buffer(8);
    push_int_values(rb, {10, 20, 30, 40, 50});

    // 0 = newest (50), 1 = second newest (40), etc.
    int* newest = (int*)nimcp_ring_buffer_peek_from_back(rb, 0);
    ASSERT_NE(newest, nullptr);
    EXPECT_EQ(*newest, 50);

    int* second = (int*)nimcp_ring_buffer_peek_from_back(rb, 1);
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(*second, 40);

    int* oldest = (int*)nimcp_ring_buffer_peek_from_back(rb, 4);
    ASSERT_NE(oldest, nullptr);
    EXPECT_EQ(*oldest, 10);

    // Out of bounds
    EXPECT_EQ(nimcp_ring_buffer_peek_from_back(rb, 5), nullptr);
}

//=============================================================================
// Unit Test 22: Clear removes all elements
//=============================================================================

TEST_F(RingBufferTest, Clear_RemovesAllElements) {
    // WHAT: Clear should set size to 0
    // WHY:  Reset buffer without deallocation
    // HOW:  Push elements, clear, verify empty

    rb = create_int_buffer(8);
    push_int_values(rb, {10, 20, 30, 40, 50});
    EXPECT_EQ(nimcp_ring_buffer_size(rb), 5u);

    nimcp_ring_buffer_clear(rb);
    EXPECT_EQ(nimcp_ring_buffer_size(rb), 0u);
    EXPECT_TRUE(nimcp_ring_buffer_is_empty(rb));

    // Capacity remains
    EXPECT_EQ(nimcp_ring_buffer_capacity(rb), 8u);
}

//=============================================================================
// Unit Test 23: Foreach iterates oldest to newest
//=============================================================================

TEST_F(RingBufferTest, Foreach_OldestToNewest) {
    // WHAT: foreach should iterate from oldest to newest
    // WHY:  Forward iteration
    // HOW:  Collect values, verify order

    rb = create_int_buffer(8);
    push_int_values(rb, {10, 20, 30, 40, 50});

    IteratorContext ctx;
    ctx.stop_at_index = -1;  // Don't stop early

    nimcp_ring_buffer_foreach(rb, test_iterator, &ctx);

    ASSERT_EQ(ctx.collected.size(), 5u);
    EXPECT_EQ(ctx.collected[0], 10);
    EXPECT_EQ(ctx.collected[1], 20);
    EXPECT_EQ(ctx.collected[2], 30);
    EXPECT_EQ(ctx.collected[3], 40);
    EXPECT_EQ(ctx.collected[4], 50);
}

//=============================================================================
// Unit Test 24: Foreach reverse iterates newest to oldest
//=============================================================================

TEST_F(RingBufferTest, ForeachReverse_NewestToOldest) {
    // WHAT: foreach_reverse should iterate from newest to oldest
    // WHY:  Backward iteration
    // HOW:  Collect values, verify order

    rb = create_int_buffer(8);
    push_int_values(rb, {10, 20, 30, 40, 50});

    IteratorContext ctx;
    ctx.stop_at_index = -1;

    nimcp_ring_buffer_foreach_reverse(rb, test_iterator, &ctx);

    ASSERT_EQ(ctx.collected.size(), 5u);
    EXPECT_EQ(ctx.collected[0], 50);  // Newest first
    EXPECT_EQ(ctx.collected[1], 40);
    EXPECT_EQ(ctx.collected[2], 30);
    EXPECT_EQ(ctx.collected[3], 20);
    EXPECT_EQ(ctx.collected[4], 10);  // Oldest last
}

//=============================================================================
// Unit Test 25: Foreach early termination
//=============================================================================

TEST_F(RingBufferTest, Foreach_EarlyTermination) {
    // WHAT: foreach should stop when iterator returns false
    // WHY:  Allow search patterns
    // HOW:  Stop at specific index

    rb = create_int_buffer(8);
    push_int_values(rb, {10, 20, 30, 40, 50});

    IteratorContext ctx;
    ctx.stop_at_index = 2;  // Stop at index 2

    nimcp_ring_buffer_foreach(rb, test_iterator, &ctx);

    ASSERT_EQ(ctx.collected.size(), 3u);  // 0, 1, 2
    EXPECT_EQ(ctx.collected[0], 10);
    EXPECT_EQ(ctx.collected[1], 20);
    EXPECT_EQ(ctx.collected[2], 30);
}

//=============================================================================
// Unit Test 26: Copy last N elements
//=============================================================================

TEST_F(RingBufferTest, CopyLastN_CopiesCorrectly) {
    // WHAT: copy_last_n should copy N newest elements
    // WHY:  Extract recent history
    // HOW:  Copy last N, verify values

    rb = create_int_buffer(8);
    push_int_values(rb, {10, 20, 30, 40, 50});

    int out[3];
    size_t copied = nimcp_ring_buffer_copy_last_n(rb, out, 3);

    EXPECT_EQ(copied, 3u);
    // Copied from newest to oldest
    EXPECT_EQ(out[0], 50);  // Newest
    EXPECT_EQ(out[1], 40);
    EXPECT_EQ(out[2], 30);
}

//=============================================================================
// Unit Test 27: Copy last N more than available
//=============================================================================

TEST_F(RingBufferTest, CopyLastN_MoreThanAvailable) {
    // WHAT: copy_last_n with N > size should copy all
    // WHY:  Handle edge case gracefully
    // HOW:  Request more than exists

    rb = create_int_buffer(8);
    push_int_values(rb, {10, 20, 30});

    int out[10];
    size_t copied = nimcp_ring_buffer_copy_last_n(rb, out, 10);

    EXPECT_EQ(copied, 3u);  // Only 3 available
    EXPECT_EQ(out[0], 30);
    EXPECT_EQ(out[1], 20);
    EXPECT_EQ(out[2], 10);
}

//=============================================================================
// Unit Test 28: Destructor called on overwrite
//=============================================================================

TEST_F(RingBufferTest, Destructor_CalledOnOverwrite) {
    // WHAT: Destructor should be called when element is overwritten
    // WHY:  Resource cleanup on overwrite
    // HOW:  Fill buffer, push more, count destructor calls

    g_destructor_calls = 0;

    rb = nimcp_ring_buffer_create_with_destructor(sizeof(int), 3, test_destructor);
    ASSERT_NE(rb, nullptr);

    push_int_values(rb, {10, 20, 30});  // Full
    EXPECT_EQ(g_destructor_calls, 0);   // No overwrites yet

    int val = 40;
    nimcp_ring_buffer_push(rb, &val);   // Overwrites 10
    EXPECT_EQ(g_destructor_calls, 1);

    val = 50;
    nimcp_ring_buffer_push(rb, &val);   // Overwrites 20
    EXPECT_EQ(g_destructor_calls, 2);
}

//=============================================================================
// Unit Test 29: Destructor called on destroy
//=============================================================================

TEST_F(RingBufferTest, Destructor_CalledOnDestroy) {
    // WHAT: Destructor should be called for each element on destroy
    // WHY:  Resource cleanup on destruction
    // HOW:  Create, fill, destroy, count calls

    g_destructor_calls = 0;

    nimcp_ring_buffer_t* local = nimcp_ring_buffer_create_with_destructor(
        sizeof(int), 8, test_destructor);
    ASSERT_NE(local, nullptr);

    int vals[] = {1, 2, 3, 4, 5};
    for (int i = 0; i < 5; i++) {
        nimcp_ring_buffer_push(local, &vals[i]);
    }

    nimcp_ring_buffer_destroy(local);
    EXPECT_EQ(g_destructor_calls, 5);
}

//=============================================================================
// Unit Test 30: Destructor called on clear
//=============================================================================

TEST_F(RingBufferTest, Destructor_CalledOnClear) {
    // WHAT: Destructor should be called on clear
    // WHY:  Resource cleanup on clear
    // HOW:  Fill, clear, count destructor calls

    g_destructor_calls = 0;

    rb = nimcp_ring_buffer_create_with_destructor(sizeof(int), 8, test_destructor);
    push_int_values(rb, {1, 2, 3, 4});

    nimcp_ring_buffer_clear(rb);
    EXPECT_EQ(g_destructor_calls, 4);
    EXPECT_EQ(nimcp_ring_buffer_size(rb), 0u);
}

//=============================================================================
// Unit Test 31: Wraparound access works correctly
//=============================================================================

TEST_F(RingBufferTest, Wraparound_AccessWorksCorrectly) {
    // WHAT: After wraparound, access should still work
    // WHY:  Core ring buffer functionality
    // HOW:  Fill, overflow multiple times, verify access

    rb = create_int_buffer(4);

    // Fill and overflow several times
    for (int i = 1; i <= 20; i++) {
        nimcp_ring_buffer_push(rb, &i);
    }

    // Should contain 17, 18, 19, 20
    EXPECT_EQ(nimcp_ring_buffer_size(rb), 4u);
    EXPECT_EQ(get_int_at(rb, 0), 17);
    EXPECT_EQ(get_int_at(rb, 1), 18);
    EXPECT_EQ(get_int_at(rb, 2), 19);
    EXPECT_EQ(get_int_at(rb, 3), 20);

    int* front = (int*)nimcp_ring_buffer_front(rb);
    int* back = (int*)nimcp_ring_buffer_back(rb);
    EXPECT_EQ(*front, 17);
    EXPECT_EQ(*back, 20);
}

//=============================================================================
// Unit Test 32: Works with struct elements
//=============================================================================

TEST_F(RingBufferTest, WorksWithStructs) {
    // WHAT: Should work with complex struct types
    // WHY:  Generic container support
    // HOW:  Create buffer of structs, push/access

    rb = nimcp_ring_buffer_create(sizeof(TestStruct), 8);
    ASSERT_NE(rb, nullptr);

    TestStruct s1 = {.id = 1, .value = 1.5f, .name = "first"};
    TestStruct s2 = {.id = 2, .value = 2.5f, .name = "second"};
    TestStruct s3 = {.id = 3, .value = 3.5f, .name = "third"};

    EXPECT_TRUE(nimcp_ring_buffer_push(rb, &s1));
    EXPECT_TRUE(nimcp_ring_buffer_push(rb, &s2));
    EXPECT_TRUE(nimcp_ring_buffer_push(rb, &s3));

    TestStruct* ptr = (TestStruct*)nimcp_ring_buffer_at(rb, 1);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(ptr->id, 2);
    EXPECT_FLOAT_EQ(ptr->value, 2.5f);
    EXPECT_STREQ(ptr->name, "second");
}

//=============================================================================
// Unit Test 33: Single element buffer works
//=============================================================================

TEST_F(RingBufferTest, SingleElementBuffer) {
    // WHAT: Buffer with capacity 1 should work
    // WHY:  Edge case
    // HOW:  Create size-1 buffer, push multiple

    rb = create_int_buffer(1);
    ASSERT_NE(rb, nullptr);

    int val = 10;
    EXPECT_TRUE(nimcp_ring_buffer_push(rb, &val));
    EXPECT_EQ(nimcp_ring_buffer_size(rb), 1u);
    EXPECT_TRUE(nimcp_ring_buffer_is_full(rb));
    EXPECT_EQ(get_int_at(rb, 0), 10);

    val = 20;
    EXPECT_TRUE(nimcp_ring_buffer_push(rb, &val));
    EXPECT_EQ(nimcp_ring_buffer_size(rb), 1u);
    EXPECT_EQ(get_int_at(rb, 0), 20);

    val = 30;
    EXPECT_TRUE(nimcp_ring_buffer_push(rb, &val));
    EXPECT_EQ(get_int_at(rb, 0), 30);
}

//=============================================================================
// Unit Test 34: At const returns same as at
//=============================================================================

TEST_F(RingBufferTest, AtConst_ReturnsSameAsAt) {
    // WHAT: at_const should return same value as at
    // WHY:  Const correctness
    // HOW:  Compare at and at_const

    rb = create_int_buffer(8);
    push_int_values(rb, {10, 20, 30});

    const nimcp_ring_buffer_t* const_rb = rb;
    const int* const_ptr = (const int*)nimcp_ring_buffer_at_const(const_rb, 1);
    int* ptr = (int*)nimcp_ring_buffer_at(rb, 1);

    EXPECT_EQ(const_ptr, ptr);
    EXPECT_EQ(*const_ptr, 20);
}

//=============================================================================
// Unit Test 35: Front/back const versions work
//=============================================================================

TEST_F(RingBufferTest, FrontBackConst_Work) {
    // WHAT: front_const and back_const should work
    // WHY:  Const correctness
    // HOW:  Use const versions

    rb = create_int_buffer(8);
    push_int_values(rb, {10, 20, 30});

    const nimcp_ring_buffer_t* const_rb = rb;

    const int* front = (const int*)nimcp_ring_buffer_front_const(const_rb);
    const int* back = (const int*)nimcp_ring_buffer_back_const(const_rb);

    ASSERT_NE(front, nullptr);
    ASSERT_NE(back, nullptr);
    EXPECT_EQ(*front, 10);
    EXPECT_EQ(*back, 30);
}

//=============================================================================
// Unit Test 36: Raw data pointer
//=============================================================================

TEST_F(RingBufferTest, RawData_ReturnsPointer) {
    // WHAT: raw_data should return data pointer
    // WHY:  Low-level access for debugging
    // HOW:  Get raw data, verify non-null

    rb = create_int_buffer(8);
    push_int_values(rb, {10, 20, 30});

    void* data = nimcp_ring_buffer_raw_data(rb);
    EXPECT_NE(data, nullptr);

    // Empty buffer should still have data
    nimcp_ring_buffer_clear(rb);
    data = nimcp_ring_buffer_raw_data(rb);
    EXPECT_NE(data, nullptr);
}

//=============================================================================
// Unit Test 37: NULL parameters return safe defaults
//=============================================================================

TEST_F(RingBufferTest, NullParams_SafeDefaults) {
    // WHAT: NULL buffer should return safe defaults
    // WHY:  Prevent crashes on invalid input
    // HOW:  Call all accessors with NULL

    EXPECT_EQ(nimcp_ring_buffer_size(nullptr), 0u);
    EXPECT_EQ(nimcp_ring_buffer_capacity(nullptr), 0u);
    EXPECT_EQ(nimcp_ring_buffer_element_size(nullptr), 0u);
    EXPECT_TRUE(nimcp_ring_buffer_is_empty(nullptr));
    EXPECT_FALSE(nimcp_ring_buffer_is_full(nullptr));
    EXPECT_EQ(nimcp_ring_buffer_at(nullptr, 0), nullptr);
    EXPECT_EQ(nimcp_ring_buffer_at_const(nullptr, 0), nullptr);
    EXPECT_EQ(nimcp_ring_buffer_front(nullptr), nullptr);
    EXPECT_EQ(nimcp_ring_buffer_back(nullptr), nullptr);
    EXPECT_EQ(nimcp_ring_buffer_front_const(nullptr), nullptr);
    EXPECT_EQ(nimcp_ring_buffer_back_const(nullptr), nullptr);
    EXPECT_EQ(nimcp_ring_buffer_peek_from_back(nullptr, 0), nullptr);
    EXPECT_EQ(nimcp_ring_buffer_raw_data(nullptr), nullptr);
}

//=============================================================================
// Unit Test 38: Copy last N with NULL params
//=============================================================================

TEST_F(RingBufferTest, CopyLastN_NullParams) {
    // WHAT: copy_last_n with NULL should return 0
    // WHY:  Safety for invalid input
    // HOW:  Call with NULL buffer or output

    rb = create_int_buffer(8);
    push_int_values(rb, {10, 20, 30});

    int out[3];
    EXPECT_EQ(nimcp_ring_buffer_copy_last_n(nullptr, out, 3), 0u);
    EXPECT_EQ(nimcp_ring_buffer_copy_last_n(rb, nullptr, 3), 0u);
    EXPECT_EQ(nimcp_ring_buffer_copy_last_n(rb, out, 0), 0u);
}

//=============================================================================
// Unit Test 39: Pop after wraparound
//=============================================================================

TEST_F(RingBufferTest, Pop_AfterWraparound) {
    // WHAT: Pop should work correctly after wraparound
    // WHY:  Verify index handling
    // HOW:  Fill, overflow, pop

    rb = create_int_buffer(4);
    push_int_values(rb, {10, 20, 30, 40, 50, 60});  // Contains 30, 40, 50, 60

    int out;
    EXPECT_TRUE(nimcp_ring_buffer_pop_front(rb, &out));
    EXPECT_EQ(out, 30);

    EXPECT_TRUE(nimcp_ring_buffer_pop_back(rb, &out));
    EXPECT_EQ(out, 60);

    EXPECT_EQ(nimcp_ring_buffer_size(rb), 2u);
    EXPECT_EQ(get_int_at(rb, 0), 40);
    EXPECT_EQ(get_int_at(rb, 1), 50);
}

//=============================================================================
// Unit Test 40: Mixed push/pop operations
//=============================================================================

TEST_F(RingBufferTest, MixedOperations) {
    // WHAT: Mixed push/pop should maintain integrity
    // WHY:  Real-world usage pattern
    // HOW:  Interleave push and pop operations

    rb = create_int_buffer(4);

    int val, out;

    // Push 3
    val = 10; nimcp_ring_buffer_push(rb, &val);
    val = 20; nimcp_ring_buffer_push(rb, &val);
    val = 30; nimcp_ring_buffer_push(rb, &val);

    // Pop 1 from front
    nimcp_ring_buffer_pop_front(rb, &out);
    EXPECT_EQ(out, 10);

    // Push 2 more
    val = 40; nimcp_ring_buffer_push(rb, &val);
    val = 50; nimcp_ring_buffer_push(rb, &val);

    // Now have: 20, 30, 40, 50
    EXPECT_EQ(nimcp_ring_buffer_size(rb), 4u);
    EXPECT_EQ(get_int_at(rb, 0), 20);
    EXPECT_EQ(get_int_at(rb, 3), 50);

    // Pop from back
    nimcp_ring_buffer_pop_back(rb, &out);
    EXPECT_EQ(out, 50);

    // Push causes wraparound
    val = 60; nimcp_ring_buffer_push(rb, &val);

    // Now have: 20, 30, 40, 60
    EXPECT_EQ(get_int_at(rb, 0), 20);
    EXPECT_EQ(get_int_at(rb, 3), 60);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
