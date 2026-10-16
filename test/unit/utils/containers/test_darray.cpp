/**
 * @file test_darray.cpp
 * @brief Comprehensive unit tests for dynamic array container
 *
 * WHAT: 100% test coverage for nimcp_darray.c
 * WHY:  Dynamic array is foundational for higher-level modules - must be bulletproof
 * HOW:  Test all operations, edge cases, memory management, and API contracts
 *
 * TEST COVERAGE:
 * 1. Creation and destruction (valid params, NULL handling)
 * 2. Push/pop operations (append, remove, boundary conditions)
 * 3. Random access (at, set, front, back)
 * 4. Insert/remove at arbitrary positions
 * 5. Size/capacity management (reserve, shrink, resize)
 * 6. Custom destructor behavior
 * 7. Type safety with different element sizes
 * 8. Memory allocation stress tests
 * 9. Edge cases (empty array, single element, NULL parameters)
 * 10. Swap operation
 *
 * @version Unit Testing Framework v1.0
 * @date 2025-12-08
 */

#include <gtest/gtest.h>
#include <vector>
#include <string>
#include <cstring>

// Headers have their own extern "C" guards
#include "utils/containers/nimcp_darray.h"

//=============================================================================
// Test Fixture
//=============================================================================

class DArrayTest : public ::testing::Test {
protected:
    nimcp_darray_t* arr;

    void SetUp() override {
        arr = nullptr;
    }

    void TearDown() override {
        if (arr) {
            nimcp_darray_destroy(arr);
            arr = nullptr;
        }
    }

    // Helper: Create array of integers
    nimcp_darray_t* create_int_array(size_t capacity = 16) {
        return nimcp_darray_create(sizeof(int), capacity);
    }

    // Helper: Create array of doubles
    nimcp_darray_t* create_double_array(size_t capacity = 16) {
        return nimcp_darray_create(sizeof(double), capacity);
    }

    // Helper: Push integer values
    void push_int_values(nimcp_darray_t* a, const std::vector<int>& values) {
        for (int v : values) {
            nimcp_darray_push_back(a, &v);
        }
    }

    // Helper: Get integer at index
    int get_int_at(nimcp_darray_t* a, size_t idx) {
        int* ptr = (int*)nimcp_darray_at(a, idx);
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

//=============================================================================
// Unit Test 1: Creation with valid capacity
//=============================================================================

TEST_F(DArrayTest, Create_ValidCapacity) {
    // WHAT: Verify array creation with valid capacity
    // WHY:  Must allocate and initialize properly
    // HOW:  Create array and verify state

    arr = create_int_array(32);
    ASSERT_NE(arr, nullptr);
    EXPECT_EQ(nimcp_darray_size(arr), 0u);
    EXPECT_EQ(nimcp_darray_capacity(arr), 32u);
    EXPECT_EQ(nimcp_darray_element_size(arr), sizeof(int));
    EXPECT_TRUE(nimcp_darray_is_empty(arr));
}

//=============================================================================
// Unit Test 2: Creation with zero capacity uses default
//=============================================================================

TEST_F(DArrayTest, Create_ZeroCapacity_UsesDefault) {
    // WHAT: Zero capacity should use default
    // WHY:  Provide sensible default behavior
    // HOW:  Create with 0, verify capacity is default (16)

    arr = nimcp_darray_create(sizeof(int), 0);
    ASSERT_NE(arr, nullptr);
    EXPECT_EQ(nimcp_darray_capacity(arr), NIMCP_DARRAY_DEFAULT_CAPACITY);
}

//=============================================================================
// Unit Test 3: Creation with zero element size fails
//=============================================================================

TEST_F(DArrayTest, Create_ZeroElementSize_Fails) {
    // WHAT: Zero element size should fail
    // WHY:  Cannot have elements of size 0
    // HOW:  Attempt creation with element_size=0

    arr = nimcp_darray_create(0, 16);
    EXPECT_EQ(arr, nullptr);
}

//=============================================================================
// Unit Test 4: Destroy NULL is safe
//=============================================================================

TEST_F(DArrayTest, Destroy_Null_Safe) {
    // WHAT: Destroying NULL should not crash
    // WHY:  Safety for cleanup code paths
    // HOW:  Call destroy with NULL

    nimcp_darray_destroy(nullptr);  // Should not crash
    SUCCEED();
}

//=============================================================================
// Unit Test 5: Push back increases size
//=============================================================================

TEST_F(DArrayTest, PushBack_IncreasesSize) {
    // WHAT: Push back should increase size
    // WHY:  Core append functionality
    // HOW:  Push elements and verify size increases

    arr = create_int_array();
    ASSERT_NE(arr, nullptr);

    int val1 = 10, val2 = 20, val3 = 30;

    EXPECT_TRUE(nimcp_darray_push_back(arr, &val1));
    EXPECT_EQ(nimcp_darray_size(arr), 1u);
    EXPECT_FALSE(nimcp_darray_is_empty(arr));

    EXPECT_TRUE(nimcp_darray_push_back(arr, &val2));
    EXPECT_EQ(nimcp_darray_size(arr), 2u);

    EXPECT_TRUE(nimcp_darray_push_back(arr, &val3));
    EXPECT_EQ(nimcp_darray_size(arr), 3u);
}

//=============================================================================
// Unit Test 6: Push back stores correct values
//=============================================================================

TEST_F(DArrayTest, PushBack_StoresCorrectValues) {
    // WHAT: Values should be retrievable after push
    // WHY:  Core data storage functionality
    // HOW:  Push values, verify via at()

    arr = create_int_array();
    push_int_values(arr, {100, 200, 300, 400, 500});

    EXPECT_EQ(get_int_at(arr, 0), 100);
    EXPECT_EQ(get_int_at(arr, 1), 200);
    EXPECT_EQ(get_int_at(arr, 2), 300);
    EXPECT_EQ(get_int_at(arr, 3), 400);
    EXPECT_EQ(get_int_at(arr, 4), 500);
}

//=============================================================================
// Unit Test 7: Push back NULL element fails
//=============================================================================

TEST_F(DArrayTest, PushBack_NullElement_Fails) {
    // WHAT: Push NULL element should fail
    // WHY:  Cannot copy from NULL
    // HOW:  Attempt push_back with NULL element

    arr = create_int_array();
    EXPECT_FALSE(nimcp_darray_push_back(arr, nullptr));
    EXPECT_EQ(nimcp_darray_size(arr), 0u);
}

//=============================================================================
// Unit Test 8: Push back NULL array fails
//=============================================================================

TEST_F(DArrayTest, PushBack_NullArray_Fails) {
    // WHAT: Push to NULL array should fail
    // WHY:  Safety for invalid parameters
    // HOW:  Call push_back on NULL array

    int val = 42;
    EXPECT_FALSE(nimcp_darray_push_back(nullptr, &val));
}

//=============================================================================
// Unit Test 9: Auto-resize on push
//=============================================================================

TEST_F(DArrayTest, PushBack_AutoResize) {
    // WHAT: Array should grow when full
    // WHY:  Dynamic sizing is core feature
    // HOW:  Push more elements than initial capacity

    arr = nimcp_darray_create(sizeof(int), 4);
    ASSERT_NE(arr, nullptr);
    EXPECT_EQ(nimcp_darray_capacity(arr), 4u);

    // Push 10 elements (more than capacity 4)
    for (int i = 0; i < 10; i++) {
        EXPECT_TRUE(nimcp_darray_push_back(arr, &i));
    }

    EXPECT_EQ(nimcp_darray_size(arr), 10u);
    EXPECT_GE(nimcp_darray_capacity(arr), 10u);

    // Verify all values
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(get_int_at(arr, i), i);
    }
}

//=============================================================================
// Unit Test 10: Pop back decreases size
//=============================================================================

TEST_F(DArrayTest, PopBack_DecreasesSize) {
    // WHAT: Pop back should decrease size
    // WHY:  Core removal functionality
    // HOW:  Push elements, pop, verify size

    arr = create_int_array();
    push_int_values(arr, {10, 20, 30});
    EXPECT_EQ(nimcp_darray_size(arr), 3u);

    int out;
    EXPECT_TRUE(nimcp_darray_pop_back(arr, &out));
    EXPECT_EQ(out, 30);
    EXPECT_EQ(nimcp_darray_size(arr), 2u);

    EXPECT_TRUE(nimcp_darray_pop_back(arr, &out));
    EXPECT_EQ(out, 20);
    EXPECT_EQ(nimcp_darray_size(arr), 1u);

    EXPECT_TRUE(nimcp_darray_pop_back(arr, &out));
    EXPECT_EQ(out, 10);
    EXPECT_EQ(nimcp_darray_size(arr), 0u);
    EXPECT_TRUE(nimcp_darray_is_empty(arr));
}

//=============================================================================
// Unit Test 11: Pop back from empty fails
//=============================================================================

TEST_F(DArrayTest, PopBack_EmptyArray_Fails) {
    // WHAT: Pop from empty array should fail
    // WHY:  Cannot pop when no elements exist
    // HOW:  Pop from empty array

    arr = create_int_array();
    int out;
    EXPECT_FALSE(nimcp_darray_pop_back(arr, &out));
}

//=============================================================================
// Unit Test 12: Pop back with NULL output buffer
//=============================================================================

TEST_F(DArrayTest, PopBack_NullOutput_Succeeds) {
    // WHAT: Pop with NULL output should still remove element
    // WHY:  Allow discard of element without copying
    // HOW:  Pop with NULL output

    arr = create_int_array();
    push_int_values(arr, {10, 20, 30});

    EXPECT_TRUE(nimcp_darray_pop_back(arr, nullptr));
    EXPECT_EQ(nimcp_darray_size(arr), 2u);
}

//=============================================================================
// Unit Test 13: At returns correct pointer
//=============================================================================

TEST_F(DArrayTest, At_ReturnsCorrectPointer) {
    // WHAT: at() should return valid pointer to element
    // WHY:  Random access is core functionality
    // HOW:  Push values, access via at()

    arr = create_int_array();
    push_int_values(arr, {100, 200, 300});

    int* ptr0 = (int*)nimcp_darray_at(arr, 0);
    int* ptr1 = (int*)nimcp_darray_at(arr, 1);
    int* ptr2 = (int*)nimcp_darray_at(arr, 2);

    ASSERT_NE(ptr0, nullptr);
    ASSERT_NE(ptr1, nullptr);
    ASSERT_NE(ptr2, nullptr);

    EXPECT_EQ(*ptr0, 100);
    EXPECT_EQ(*ptr1, 200);
    EXPECT_EQ(*ptr2, 300);
}

//=============================================================================
// Unit Test 14: At out of bounds returns NULL
//=============================================================================

TEST_F(DArrayTest, At_OutOfBounds_ReturnsNull) {
    // WHAT: at() with invalid index should return NULL
    // WHY:  Bounds checking prevents crashes
    // HOW:  Access with invalid indices

    arr = create_int_array();
    push_int_values(arr, {10, 20, 30});

    EXPECT_EQ(nimcp_darray_at(arr, 3), nullptr);
    EXPECT_EQ(nimcp_darray_at(arr, 100), nullptr);
    EXPECT_EQ(nimcp_darray_at(arr, SIZE_MAX), nullptr);
}

//=============================================================================
// Unit Test 15: At on empty array returns NULL
//=============================================================================

TEST_F(DArrayTest, At_EmptyArray_ReturnsNull) {
    // WHAT: at() on empty array should return NULL
    // WHY:  No elements to access
    // HOW:  Create empty array, try at(0)

    arr = create_int_array();
    EXPECT_EQ(nimcp_darray_at(arr, 0), nullptr);
}

//=============================================================================
// Unit Test 16: Set modifies element
//=============================================================================

TEST_F(DArrayTest, Set_ModifiesElement) {
    // WHAT: set() should modify element at index
    // WHY:  Allow in-place modification
    // HOW:  Push values, set new value, verify

    arr = create_int_array();
    push_int_values(arr, {10, 20, 30});

    int new_val = 999;
    EXPECT_TRUE(nimcp_darray_set(arr, 1, &new_val));

    EXPECT_EQ(get_int_at(arr, 0), 10);
    EXPECT_EQ(get_int_at(arr, 1), 999);
    EXPECT_EQ(get_int_at(arr, 2), 30);
}

//=============================================================================
// Unit Test 17: Set out of bounds fails
//=============================================================================

TEST_F(DArrayTest, Set_OutOfBounds_Fails) {
    // WHAT: set() with invalid index should fail
    // WHY:  Cannot set non-existent element
    // HOW:  Try to set beyond size

    arr = create_int_array();
    push_int_values(arr, {10, 20, 30});

    int val = 999;
    EXPECT_FALSE(nimcp_darray_set(arr, 3, &val));
    EXPECT_FALSE(nimcp_darray_set(arr, 100, &val));
}

//=============================================================================
// Unit Test 18: Front returns first element
//=============================================================================

TEST_F(DArrayTest, Front_ReturnsFirstElement) {
    // WHAT: front() should return first element
    // WHY:  Convenience accessor
    // HOW:  Push values, verify front()

    arr = create_int_array();
    push_int_values(arr, {100, 200, 300});

    int* front = (int*)nimcp_darray_front(arr);
    ASSERT_NE(front, nullptr);
    EXPECT_EQ(*front, 100);
}

//=============================================================================
// Unit Test 19: Back returns last element
//=============================================================================

TEST_F(DArrayTest, Back_ReturnsLastElement) {
    // WHAT: back() should return last element
    // WHY:  Convenience accessor
    // HOW:  Push values, verify back()

    arr = create_int_array();
    push_int_values(arr, {100, 200, 300});

    int* back = (int*)nimcp_darray_back(arr);
    ASSERT_NE(back, nullptr);
    EXPECT_EQ(*back, 300);

    // Push more, verify back updates
    int val = 400;
    nimcp_darray_push_back(arr, &val);
    back = (int*)nimcp_darray_back(arr);
    EXPECT_EQ(*back, 400);
}

//=============================================================================
// Unit Test 20: Front/back on empty returns NULL
//=============================================================================

TEST_F(DArrayTest, FrontBack_Empty_ReturnsNull) {
    // WHAT: front()/back() on empty should return NULL
    // WHY:  No elements to access
    // HOW:  Call on empty array

    arr = create_int_array();
    EXPECT_EQ(nimcp_darray_front(arr), nullptr);
    EXPECT_EQ(nimcp_darray_back(arr), nullptr);
}

//=============================================================================
// Unit Test 21: Insert at beginning
//=============================================================================

TEST_F(DArrayTest, Insert_AtBeginning) {
    // WHAT: Insert at index 0 should prepend
    // WHY:  Support insertion at any position
    // HOW:  Insert at 0, verify order

    arr = create_int_array();
    push_int_values(arr, {20, 30});

    int val = 10;
    EXPECT_TRUE(nimcp_darray_insert(arr, 0, &val));

    EXPECT_EQ(nimcp_darray_size(arr), 3u);
    EXPECT_EQ(get_int_at(arr, 0), 10);
    EXPECT_EQ(get_int_at(arr, 1), 20);
    EXPECT_EQ(get_int_at(arr, 2), 30);
}

//=============================================================================
// Unit Test 22: Insert in middle
//=============================================================================

TEST_F(DArrayTest, Insert_InMiddle) {
    // WHAT: Insert in middle shifts elements right
    // WHY:  Support insertion at any position
    // HOW:  Insert at middle index

    arr = create_int_array();
    push_int_values(arr, {10, 30, 40});

    int val = 20;
    EXPECT_TRUE(nimcp_darray_insert(arr, 1, &val));

    EXPECT_EQ(nimcp_darray_size(arr), 4u);
    EXPECT_EQ(get_int_at(arr, 0), 10);
    EXPECT_EQ(get_int_at(arr, 1), 20);
    EXPECT_EQ(get_int_at(arr, 2), 30);
    EXPECT_EQ(get_int_at(arr, 3), 40);
}

//=============================================================================
// Unit Test 23: Insert at end (equals push_back)
//=============================================================================

TEST_F(DArrayTest, Insert_AtEnd) {
    // WHAT: Insert at size should append
    // WHY:  Valid edge case
    // HOW:  Insert at index == size

    arr = create_int_array();
    push_int_values(arr, {10, 20});

    int val = 30;
    EXPECT_TRUE(nimcp_darray_insert(arr, 2, &val));

    EXPECT_EQ(nimcp_darray_size(arr), 3u);
    EXPECT_EQ(get_int_at(arr, 2), 30);
}

//=============================================================================
// Unit Test 24: Insert beyond size fails
//=============================================================================

TEST_F(DArrayTest, Insert_BeyondSize_Fails) {
    // WHAT: Insert at index > size should fail
    // WHY:  Would create gap in array
    // HOW:  Try insert at invalid index

    arr = create_int_array();
    push_int_values(arr, {10, 20});

    int val = 30;
    EXPECT_FALSE(nimcp_darray_insert(arr, 3, &val));  // size is 2, so 3 is invalid
}

//=============================================================================
// Unit Test 25: Remove at index
//=============================================================================

TEST_F(DArrayTest, RemoveAt_Middle) {
    // WHAT: Remove at index should shift elements left
    // WHY:  Support removal at any position
    // HOW:  Remove from middle, verify order

    arr = create_int_array();
    push_int_values(arr, {10, 20, 30, 40});

    int out;
    EXPECT_TRUE(nimcp_darray_remove_at(arr, 1, &out));
    EXPECT_EQ(out, 20);

    EXPECT_EQ(nimcp_darray_size(arr), 3u);
    EXPECT_EQ(get_int_at(arr, 0), 10);
    EXPECT_EQ(get_int_at(arr, 1), 30);
    EXPECT_EQ(get_int_at(arr, 2), 40);
}

//=============================================================================
// Unit Test 26: Clear removes all elements
//=============================================================================

TEST_F(DArrayTest, Clear_RemovesAllElements) {
    // WHAT: Clear should set size to 0
    // WHY:  Reset array without deallocation
    // HOW:  Push elements, clear, verify empty

    arr = create_int_array();
    push_int_values(arr, {10, 20, 30, 40, 50});
    EXPECT_EQ(nimcp_darray_size(arr), 5u);

    nimcp_darray_clear(arr);
    EXPECT_EQ(nimcp_darray_size(arr), 0u);
    EXPECT_TRUE(nimcp_darray_is_empty(arr));

    // Capacity should remain
    EXPECT_GT(nimcp_darray_capacity(arr), 0u);
}

//=============================================================================
// Unit Test 27: Reserve increases capacity
//=============================================================================

TEST_F(DArrayTest, Reserve_IncreasesCapacity) {
    // WHAT: Reserve should increase capacity
    // WHY:  Pre-allocate to avoid resizes
    // HOW:  Reserve larger capacity, verify

    arr = nimcp_darray_create(sizeof(int), 8);
    EXPECT_EQ(nimcp_darray_capacity(arr), 8u);

    EXPECT_TRUE(nimcp_darray_reserve(arr, 100));
    EXPECT_GE(nimcp_darray_capacity(arr), 100u);
    EXPECT_EQ(nimcp_darray_size(arr), 0u);  // Size unchanged
}

//=============================================================================
// Unit Test 28: Reserve with smaller capacity does nothing
//=============================================================================

TEST_F(DArrayTest, Reserve_Smaller_NoChange) {
    // WHAT: Reserve with smaller value should not shrink
    // WHY:  Reserve only grows, never shrinks
    // HOW:  Reserve smaller, verify no change

    arr = nimcp_darray_create(sizeof(int), 32);
    EXPECT_TRUE(nimcp_darray_reserve(arr, 8));
    EXPECT_EQ(nimcp_darray_capacity(arr), 32u);  // Unchanged
}

//=============================================================================
// Unit Test 29: Shrink to fit reduces capacity
//=============================================================================

TEST_F(DArrayTest, ShrinkToFit_ReducesCapacity) {
    // WHAT: Shrink to fit should reduce capacity to size
    // WHY:  Free unused memory
    // HOW:  Push few elements into large array, shrink

    arr = nimcp_darray_create(sizeof(int), 100);
    push_int_values(arr, {10, 20, 30});
    EXPECT_EQ(nimcp_darray_size(arr), 3u);
    EXPECT_EQ(nimcp_darray_capacity(arr), 100u);

    EXPECT_TRUE(nimcp_darray_shrink_to_fit(arr));
    EXPECT_EQ(nimcp_darray_capacity(arr), 3u);

    // Values should be preserved
    EXPECT_EQ(get_int_at(arr, 0), 10);
    EXPECT_EQ(get_int_at(arr, 1), 20);
    EXPECT_EQ(get_int_at(arr, 2), 30);
}

//=============================================================================
// Unit Test 30: Resize grows array
//=============================================================================

TEST_F(DArrayTest, Resize_Grows) {
    // WHAT: Resize larger should grow with zero-init
    // WHY:  Change size explicitly
    // HOW:  Resize larger, verify new elements are zero

    arr = create_int_array();
    push_int_values(arr, {10, 20});

    EXPECT_TRUE(nimcp_darray_resize(arr, 5));
    EXPECT_EQ(nimcp_darray_size(arr), 5u);

    // Original values preserved
    EXPECT_EQ(get_int_at(arr, 0), 10);
    EXPECT_EQ(get_int_at(arr, 1), 20);

    // New elements should be zero-initialized
    EXPECT_EQ(get_int_at(arr, 2), 0);
    EXPECT_EQ(get_int_at(arr, 3), 0);
    EXPECT_EQ(get_int_at(arr, 4), 0);
}

//=============================================================================
// Unit Test 31: Resize shrinks array
//=============================================================================

TEST_F(DArrayTest, Resize_Shrinks) {
    // WHAT: Resize smaller should truncate
    // WHY:  Explicit size reduction
    // HOW:  Resize smaller, verify size

    arr = create_int_array();
    push_int_values(arr, {10, 20, 30, 40, 50});

    EXPECT_TRUE(nimcp_darray_resize(arr, 2));
    EXPECT_EQ(nimcp_darray_size(arr), 2u);

    // First two values preserved
    EXPECT_EQ(get_int_at(arr, 0), 10);
    EXPECT_EQ(get_int_at(arr, 1), 20);
}

//=============================================================================
// Unit Test 32: Data returns contiguous storage
//=============================================================================

TEST_F(DArrayTest, Data_ReturnsContiguousStorage) {
    // WHAT: data() should return contiguous array
    // WHY:  Allow raw array access for C APIs
    // HOW:  Get data pointer, iterate

    arr = create_int_array();
    push_int_values(arr, {100, 200, 300, 400});

    int* data = (int*)nimcp_darray_data(arr);
    ASSERT_NE(data, nullptr);

    EXPECT_EQ(data[0], 100);
    EXPECT_EQ(data[1], 200);
    EXPECT_EQ(data[2], 300);
    EXPECT_EQ(data[3], 400);
}

//=============================================================================
// Unit Test 33: Destructor called on destroy
//=============================================================================

TEST_F(DArrayTest, Destructor_CalledOnDestroy) {
    // WHAT: Destructor should be called for each element on destroy
    // WHY:  Allow cleanup of nested resources
    // HOW:  Create with destructor, destroy, count calls

    g_destructor_calls = 0;

    nimcp_darray_t* local = nimcp_darray_create_with_destructor(
        sizeof(int), 16, test_destructor);
    ASSERT_NE(local, nullptr);

    int vals[] = {1, 2, 3, 4, 5};
    for (int i = 0; i < 5; i++) {
        nimcp_darray_push_back(local, &vals[i]);
    }

    nimcp_darray_destroy(local);
    EXPECT_EQ(g_destructor_calls, 5);
}

//=============================================================================
// Unit Test 34: Destructor called on clear
//=============================================================================

TEST_F(DArrayTest, Destructor_CalledOnClear) {
    // WHAT: Destructor should be called on clear
    // WHY:  Clear should cleanup elements
    // HOW:  Clear array, count destructor calls

    g_destructor_calls = 0;

    arr = nimcp_darray_create_with_destructor(sizeof(int), 16, test_destructor);
    int vals[] = {1, 2, 3};
    for (int i = 0; i < 3; i++) {
        nimcp_darray_push_back(arr, &vals[i]);
    }

    nimcp_darray_clear(arr);
    EXPECT_EQ(g_destructor_calls, 3);
    EXPECT_EQ(nimcp_darray_size(arr), 0u);
}

//=============================================================================
// Unit Test 35: Destructor called on resize shrink
//=============================================================================

TEST_F(DArrayTest, Destructor_CalledOnResizeShrink) {
    // WHAT: Destructor called for removed elements on shrink
    // WHY:  Resize down should cleanup excess
    // HOW:  Resize smaller, count calls

    g_destructor_calls = 0;

    arr = nimcp_darray_create_with_destructor(sizeof(int), 16, test_destructor);
    int vals[] = {1, 2, 3, 4, 5};
    for (int i = 0; i < 5; i++) {
        nimcp_darray_push_back(arr, &vals[i]);
    }

    nimcp_darray_resize(arr, 2);  // Remove 3 elements
    EXPECT_EQ(g_destructor_calls, 3);
}

//=============================================================================
// Unit Test 36: Swap arrays
//=============================================================================

TEST_F(DArrayTest, Swap_ExchangesContents) {
    // WHAT: Swap should exchange array contents
    // WHY:  Efficient content exchange without copying
    // HOW:  Create two arrays, swap, verify

    arr = create_int_array();
    push_int_values(arr, {1, 2, 3});

    nimcp_darray_t* arr2 = create_int_array();
    push_int_values(arr2, {100, 200, 300, 400, 500});

    EXPECT_TRUE(nimcp_darray_swap(arr, arr2));

    // arr now has arr2's old contents
    EXPECT_EQ(nimcp_darray_size(arr), 5u);
    EXPECT_EQ(get_int_at(arr, 0), 100);

    // arr2 now has arr's old contents
    EXPECT_EQ(nimcp_darray_size(arr2), 3u);
    EXPECT_EQ(*(int*)nimcp_darray_at(arr2, 0), 1);

    nimcp_darray_destroy(arr2);
}

//=============================================================================
// Unit Test 37: Swap different element sizes fails
//=============================================================================

TEST_F(DArrayTest, Swap_DifferentSizes_Fails) {
    // WHAT: Swap arrays with different element sizes should fail
    // WHY:  Type safety
    // HOW:  Create arrays with different element sizes, try swap

    arr = create_int_array();  // sizeof(int)
    nimcp_darray_t* arr2 = create_double_array();  // sizeof(double)

    EXPECT_FALSE(nimcp_darray_swap(arr, arr2));

    nimcp_darray_destroy(arr2);
}

//=============================================================================
// Unit Test 38: Works with struct elements
//=============================================================================

TEST_F(DArrayTest, WorksWithStructs) {
    // WHAT: Should work with complex struct types
    // WHY:  Generic container support
    // HOW:  Create array of structs, push/access

    arr = nimcp_darray_create(sizeof(TestStruct), 16);
    ASSERT_NE(arr, nullptr);

    TestStruct s1 = {.id = 1, .value = 1.5f, .name = "first"};
    TestStruct s2 = {.id = 2, .value = 2.5f, .name = "second"};
    TestStruct s3 = {.id = 3, .value = 3.5f, .name = "third"};

    EXPECT_TRUE(nimcp_darray_push_back(arr, &s1));
    EXPECT_TRUE(nimcp_darray_push_back(arr, &s2));
    EXPECT_TRUE(nimcp_darray_push_back(arr, &s3));

    TestStruct* ptr = (TestStruct*)nimcp_darray_at(arr, 1);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(ptr->id, 2);
    EXPECT_FLOAT_EQ(ptr->value, 2.5f);
    EXPECT_STREQ(ptr->name, "second");
}

//=============================================================================
// Unit Test 39: Large array stress test
//=============================================================================

TEST_F(DArrayTest, StressTest_LargeArray) {
    // WHAT: Should handle large number of elements
    // WHY:  Verify scaling behavior
    // HOW:  Push many elements, verify all

    arr = create_int_array(4);  // Start small
    const int COUNT = 10000;

    for (int i = 0; i < COUNT; i++) {
        EXPECT_TRUE(nimcp_darray_push_back(arr, &i));
    }

    EXPECT_EQ(nimcp_darray_size(arr), (size_t)COUNT);

    // Verify all values
    for (int i = 0; i < COUNT; i++) {
        EXPECT_EQ(get_int_at(arr, i), i);
    }
}

//=============================================================================
// Unit Test 40: At const returns same as at
//=============================================================================

TEST_F(DArrayTest, AtConst_ReturnsSameAsAt) {
    // WHAT: at_const should return same value as at
    // WHY:  Const correctness
    // HOW:  Compare at and at_const

    arr = create_int_array();
    push_int_values(arr, {10, 20, 30});

    const nimcp_darray_t* const_arr = arr;
    const int* const_ptr = (const int*)nimcp_darray_at_const(const_arr, 1);
    int* ptr = (int*)nimcp_darray_at(arr, 1);

    EXPECT_EQ(const_ptr, ptr);
    EXPECT_EQ(*const_ptr, 20);
}

//=============================================================================
// Unit Test 41: Element size accessor
//=============================================================================

TEST_F(DArrayTest, ElementSize_ReturnsCorrectSize) {
    // WHAT: element_size should return configured size
    // WHY:  Allow introspection
    // HOW:  Create arrays of different types, verify sizes

    nimcp_darray_t* arr_int = create_int_array();
    nimcp_darray_t* arr_double = create_double_array();
    nimcp_darray_t* arr_struct = nimcp_darray_create(sizeof(TestStruct), 16);

    EXPECT_EQ(nimcp_darray_element_size(arr_int), sizeof(int));
    EXPECT_EQ(nimcp_darray_element_size(arr_double), sizeof(double));
    EXPECT_EQ(nimcp_darray_element_size(arr_struct), sizeof(TestStruct));

    nimcp_darray_destroy(arr_int);
    nimcp_darray_destroy(arr_double);
    nimcp_darray_destroy(arr_struct);

    arr = nullptr;  // Prevent double-free in TearDown
}

//=============================================================================
// Unit Test 42: NULL parameters return safe defaults
//=============================================================================

TEST_F(DArrayTest, NullParams_SafeDefaults) {
    // WHAT: NULL array should return safe defaults
    // WHY:  Prevent crashes on invalid input
    // HOW:  Call all accessors with NULL

    EXPECT_EQ(nimcp_darray_size(nullptr), 0u);
    EXPECT_EQ(nimcp_darray_capacity(nullptr), 0u);
    EXPECT_EQ(nimcp_darray_element_size(nullptr), 0u);
    EXPECT_TRUE(nimcp_darray_is_empty(nullptr));
    EXPECT_EQ(nimcp_darray_at(nullptr, 0), nullptr);
    EXPECT_EQ(nimcp_darray_at_const(nullptr, 0), nullptr);
    EXPECT_EQ(nimcp_darray_front(nullptr), nullptr);
    EXPECT_EQ(nimcp_darray_back(nullptr), nullptr);
    EXPECT_EQ(nimcp_darray_data(nullptr), nullptr);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
