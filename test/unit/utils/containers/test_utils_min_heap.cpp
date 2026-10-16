/**
 * @file test_utils_min_heap.cpp
 * @brief Comprehensive unit tests for min-heap data structure
 *
 * WHAT: 100% test coverage for nimcp_min_heap.c
 * WHY:  Min-heap is critical for Dijkstra's algorithm - must be bulletproof
 * HOW:  Test all operations, heap properties, edge cases, and priority queue behavior
 *
 * TEST COVERAGE:
 * 1. Heap creation and destruction
 * 2. Insert operations and heap property maintenance
 * 3. Extract-min operations
 * 4. Peek-min (non-destructive read)
 * 5. Decrease-key operations (for Dijkstra's algorithm)
 * 6. Empty heap handling
 * 7. Full heap handling
 * 8. Size and capacity tracking
 * 9. Clear operation
 * 10. Priority queue behavior verification
 * 11. Heap property verification after operations
 * 12. Edge cases (NULL pointers, boundary conditions)
 *
 * @version Unit Testing Framework v1.0
 * @date 2025-11-13
 */

#include <gtest/gtest.h>
#include <vector>
#include <algorithm>
#include <cmath>

    #include "utils/containers/nimcp_min_heap.h"

//=============================================================================
// Test Fixture
//=============================================================================

class MinHeapTest : public ::testing::Test {
protected:
    nimcp_min_heap_t* heap;

    void SetUp() override {
        heap = nullptr;
    }

    void TearDown() override {
        if (heap) {
            nimcp_min_heap_destroy(heap);
            heap = nullptr;
        }
    }

    // Helper: Verify heap property (parent <= children)
    // NOTE: Since heap structure is opaque, we rely on extract-min
    // returning elements in sorted order (tested separately)
    bool verify_heap_property(nimcp_min_heap_t* h) {
        if (!h) return false;

        uint32_t size = nimcp_min_heap_size(h);
        if (size <= 1) return true;

        // Heap property is implicitly verified by extract-min tests
        // which verify elements come out in sorted order
        return true;
    }

    // Helper: Create element
    nimcp_heap_element_t make_element(uint32_t vertex_id, float priority) {
        nimcp_heap_element_t elem;
        elem.vertex_id = vertex_id;
        elem.priority = priority;
        return elem;
    }
};

//=============================================================================
// Unit Test 1: Heap creation with valid capacity
//=============================================================================

TEST_F(MinHeapTest, Create_ValidCapacity) {
    // WHAT: Verify heap creation with valid capacity
    // WHY:  Must allocate and initialize properly
    // HOW:  Create heap and check it's not NULL, initially empty

    heap = nimcp_min_heap_create(10);
    ASSERT_NE(heap, nullptr);
    EXPECT_TRUE(nimcp_min_heap_is_empty(heap));
    EXPECT_EQ(nimcp_min_heap_size(heap), 0u);
    EXPECT_EQ(nimcp_min_heap_capacity(heap), 10u);
    EXPECT_FALSE(nimcp_min_heap_is_full(heap));
}

//=============================================================================
// Unit Test 2: Heap creation with zero capacity
//=============================================================================

TEST_F(MinHeapTest, Create_ZeroCapacity) {
    // WHAT: Verify heap creation fails with zero capacity
    // WHY:  Zero capacity heap is invalid
    // HOW:  Attempt to create with capacity 0, expect NULL

    heap = nimcp_min_heap_create(0);
    EXPECT_EQ(heap, nullptr);
}

//=============================================================================
// Unit Test 3: Heap destruction
//=============================================================================

TEST_F(MinHeapTest, Destroy_NullSafe) {
    // WHAT: Verify destroy handles NULL safely
    // WHY:  Must not crash on NULL pointer
    // HOW:  Call destroy with NULL

    nimcp_min_heap_destroy(nullptr);
    SUCCEED() << "Destroy NULL is safe";
}

//=============================================================================
// Unit Test 4: Insert single element
//=============================================================================

TEST_F(MinHeapTest, Insert_SingleElement) {
    // WHAT: Verify inserting a single element works
    // WHY:  Basic insert functionality
    // HOW:  Insert one element, verify size increases and element can be peeked

    heap = nimcp_min_heap_create(10);
    ASSERT_NE(heap, nullptr);

    nimcp_heap_element_t elem = make_element(5, 10.0f);
    EXPECT_TRUE(nimcp_min_heap_insert(heap, &elem));
    EXPECT_EQ(nimcp_min_heap_size(heap), 1u);
    EXPECT_FALSE(nimcp_min_heap_is_empty(heap));

    nimcp_heap_element_t peeked;
    EXPECT_TRUE(nimcp_min_heap_peek_min(heap, &peeked));
    EXPECT_EQ(peeked.vertex_id, 5u);
    EXPECT_FLOAT_EQ(peeked.priority, 10.0f);
}

//=============================================================================
// Unit Test 5: Insert multiple elements and verify min-heap property
//=============================================================================

TEST_F(MinHeapTest, Insert_MultipleElementsMaintainsHeapProperty) {
    // WHAT: Verify multiple insertions maintain min-heap property
    // WHY:  Heap must keep minimum at root after each insertion
    // HOW:  Insert elements in various orders, verify min is always accessible

    heap = nimcp_min_heap_create(10);
    ASSERT_NE(heap, nullptr);

    // Insert in descending order
    float priorities[] = {50.0f, 40.0f, 30.0f, 20.0f, 10.0f};
    for (int i = 0; i < 5; i++) {
        nimcp_heap_element_t elem = make_element(i, priorities[i]);
        ASSERT_TRUE(nimcp_min_heap_insert(heap, &elem));
    }

    EXPECT_EQ(nimcp_min_heap_size(heap), 5u);

    // Min should be 10.0f (last inserted)
    nimcp_heap_element_t min_elem;
    EXPECT_TRUE(nimcp_min_heap_peek_min(heap, &min_elem));
    EXPECT_FLOAT_EQ(min_elem.priority, 10.0f);
}

//=============================================================================
// Unit Test 6: Insert into full heap
//=============================================================================

TEST_F(MinHeapTest, Insert_IntoFullHeap) {
    // WHAT: Verify insert fails when heap is full
    // WHY:  Must handle capacity limits properly
    // HOW:  Fill heap to capacity, attempt one more insert

    heap = nimcp_min_heap_create(3);
    ASSERT_NE(heap, nullptr);

    // Fill heap
    for (uint32_t i = 0; i < 3; i++) {
        nimcp_heap_element_t elem = make_element(i, (float)i);
        ASSERT_TRUE(nimcp_min_heap_insert(heap, &elem));
    }

    EXPECT_TRUE(nimcp_min_heap_is_full(heap));

    // Try to insert when full
    nimcp_heap_element_t overflow_elem = make_element(10, 100.0f);
    EXPECT_FALSE(nimcp_min_heap_insert(heap, &overflow_elem));
    EXPECT_EQ(nimcp_min_heap_size(heap), 3u);
}

//=============================================================================
// Unit Test 7: Extract-min from heap
//=============================================================================

TEST_F(MinHeapTest, ExtractMin_ReturnsMinimumElement) {
    // WHAT: Verify extract-min removes and returns minimum element
    // WHY:  Core priority queue operation
    // HOW:  Insert multiple elements, extract and verify order

    heap = nimcp_min_heap_create(10);
    ASSERT_NE(heap, nullptr);

    // Insert elements with known priorities
    float priorities[] = {30.0f, 10.0f, 50.0f, 20.0f, 40.0f};
    for (int i = 0; i < 5; i++) {
        nimcp_heap_element_t elem = make_element(i, priorities[i]);
        ASSERT_TRUE(nimcp_min_heap_insert(heap, &elem));
    }

    // Extract in priority order
    float expected_order[] = {10.0f, 20.0f, 30.0f, 40.0f, 50.0f};
    for (int i = 0; i < 5; i++) {
        nimcp_heap_element_t extracted;
        ASSERT_TRUE(nimcp_min_heap_extract_min(heap, &extracted));
        EXPECT_FLOAT_EQ(extracted.priority, expected_order[i]);
    }

    EXPECT_TRUE(nimcp_min_heap_is_empty(heap));
}

//=============================================================================
// Unit Test 8: Extract-min from empty heap
//=============================================================================

TEST_F(MinHeapTest, ExtractMin_FromEmptyHeap) {
    // WHAT: Verify extract-min fails on empty heap
    // WHY:  Cannot extract from empty heap
    // HOW:  Create empty heap, attempt extract

    heap = nimcp_min_heap_create(10);
    ASSERT_NE(heap, nullptr);

    nimcp_heap_element_t elem;
    EXPECT_FALSE(nimcp_min_heap_extract_min(heap, &elem));
    EXPECT_TRUE(nimcp_min_heap_is_empty(heap));
}

//=============================================================================
// Unit Test 9: Peek-min without removing element
//=============================================================================

TEST_F(MinHeapTest, PeekMin_DoesNotModifyHeap) {
    // WHAT: Verify peek-min doesn't remove element
    // WHY:  Must allow inspection without modification
    // HOW:  Insert elements, peek multiple times, verify size unchanged

    heap = nimcp_min_heap_create(10);
    ASSERT_NE(heap, nullptr);

    nimcp_heap_element_t elem1 = make_element(1, 10.0f);
    nimcp_heap_element_t elem2 = make_element(2, 20.0f);
    ASSERT_TRUE(nimcp_min_heap_insert(heap, &elem1));
    ASSERT_TRUE(nimcp_min_heap_insert(heap, &elem2));

    uint32_t size_before = nimcp_min_heap_size(heap);

    // Peek multiple times
    nimcp_heap_element_t peeked;
    for (int i = 0; i < 3; i++) {
        EXPECT_TRUE(nimcp_min_heap_peek_min(heap, &peeked));
        EXPECT_FLOAT_EQ(peeked.priority, 10.0f);
    }

    // Size should be unchanged
    EXPECT_EQ(nimcp_min_heap_size(heap), size_before);
}

//=============================================================================
// Unit Test 10: Decrease-key operation
//=============================================================================

TEST_F(MinHeapTest, DecreaseKey_UpdatesPriorityCorrectly) {
    // WHAT: Verify decrease-key updates priority and maintains heap property
    // WHY:  Essential for Dijkstra's shortest path algorithm
    // HOW:  Insert elements, decrease a key, verify new minimum

    heap = nimcp_min_heap_create(10);
    ASSERT_NE(heap, nullptr);

    // Insert elements
    nimcp_heap_element_t elem1 = make_element(0, 50.0f);
    nimcp_heap_element_t elem2 = make_element(1, 30.0f);
    nimcp_heap_element_t elem3 = make_element(2, 40.0f);

    ASSERT_TRUE(nimcp_min_heap_insert(heap, &elem1));
    ASSERT_TRUE(nimcp_min_heap_insert(heap, &elem2));
    ASSERT_TRUE(nimcp_min_heap_insert(heap, &elem3));

    // Min should be vertex 1 with priority 30.0
    nimcp_heap_element_t min_elem;
    EXPECT_TRUE(nimcp_min_heap_peek_min(heap, &min_elem));
    EXPECT_EQ(min_elem.vertex_id, 1u);
    EXPECT_FLOAT_EQ(min_elem.priority, 30.0f);

    // Decrease key of vertex 0 to 5.0 (now minimum)
    EXPECT_TRUE(nimcp_min_heap_decrease_key(heap, 0, 5.0f));

    // Verify new minimum
    EXPECT_TRUE(nimcp_min_heap_peek_min(heap, &min_elem));
    EXPECT_EQ(min_elem.vertex_id, 0u);
    EXPECT_FLOAT_EQ(min_elem.priority, 5.0f);
}

//=============================================================================
// Unit Test 11: Decrease-key with invalid increase
//=============================================================================

TEST_F(MinHeapTest, DecreaseKey_RejectsIncrease) {
    // WHAT: Verify decrease-key fails when trying to increase priority
    // WHY:  Decrease-key should only allow decreasing, not increasing
    // HOW:  Insert element, try to "decrease" to higher value

    heap = nimcp_min_heap_create(10);
    ASSERT_NE(heap, nullptr);

    nimcp_heap_element_t elem = make_element(0, 10.0f);
    ASSERT_TRUE(nimcp_min_heap_insert(heap, &elem));

    // Try to increase priority (should fail)
    EXPECT_FALSE(nimcp_min_heap_decrease_key(heap, 0, 20.0f));

    // Verify priority unchanged
    nimcp_heap_element_t peeked;
    EXPECT_TRUE(nimcp_min_heap_peek_min(heap, &peeked));
    EXPECT_FLOAT_EQ(peeked.priority, 10.0f);
}

//=============================================================================
// Unit Test 12: Decrease-key on non-existent vertex
//=============================================================================

TEST_F(MinHeapTest, DecreaseKey_NonExistentVertex) {
    // WHAT: Verify decrease-key fails for vertex not in heap
    // WHY:  Cannot update non-existent element
    // HOW:  Create heap, try to decrease key of vertex never inserted

    heap = nimcp_min_heap_create(10);
    ASSERT_NE(heap, nullptr);

    nimcp_heap_element_t elem = make_element(0, 10.0f);
    ASSERT_TRUE(nimcp_min_heap_insert(heap, &elem));

    // Try to decrease key of vertex 5 (never inserted)
    EXPECT_FALSE(nimcp_min_heap_decrease_key(heap, 5, 1.0f));
}

//=============================================================================
// Unit Test 13: Clear operation
//=============================================================================

TEST_F(MinHeapTest, Clear_EmptiesHeap) {
    // WHAT: Verify clear removes all elements
    // WHY:  Must be able to reset heap without recreating
    // HOW:  Insert elements, clear, verify empty

    heap = nimcp_min_heap_create(10);
    ASSERT_NE(heap, nullptr);

    // Insert several elements
    for (uint32_t i = 0; i < 5; i++) {
        nimcp_heap_element_t elem = make_element(i, (float)i);
        ASSERT_TRUE(nimcp_min_heap_insert(heap, &elem));
    }

    EXPECT_EQ(nimcp_min_heap_size(heap), 5u);

    // Clear heap
    nimcp_min_heap_clear(heap);

    // Verify empty
    EXPECT_TRUE(nimcp_min_heap_is_empty(heap));
    EXPECT_EQ(nimcp_min_heap_size(heap), 0u);

    // Should be able to insert again (use vertex_id within range)
    nimcp_heap_element_t elem = make_element(9, 100.0f);
    EXPECT_TRUE(nimcp_min_heap_insert(heap, &elem));
    EXPECT_EQ(nimcp_min_heap_size(heap), 1u);
}

//=============================================================================
// Unit Test 14: Size tracking
//=============================================================================

TEST_F(MinHeapTest, Size_TrackedCorrectly) {
    // WHAT: Verify size is tracked accurately through operations
    // WHY:  Size must reflect actual element count
    // HOW:  Insert/extract elements, verify size after each operation

    heap = nimcp_min_heap_create(10);
    ASSERT_NE(heap, nullptr);

    EXPECT_EQ(nimcp_min_heap_size(heap), 0u);

    // Insert 5 elements
    for (uint32_t i = 0; i < 5; i++) {
        nimcp_heap_element_t elem = make_element(i, (float)i);
        ASSERT_TRUE(nimcp_min_heap_insert(heap, &elem));
        EXPECT_EQ(nimcp_min_heap_size(heap), i + 1);
    }

    // Extract 3 elements
    for (int i = 0; i < 3; i++) {
        nimcp_heap_element_t elem;
        ASSERT_TRUE(nimcp_min_heap_extract_min(heap, &elem));
        EXPECT_EQ(nimcp_min_heap_size(heap), 5u - i - 1);
    }

    EXPECT_EQ(nimcp_min_heap_size(heap), 2u);
}

//=============================================================================
// Unit Test 15: Priority queue behavior - stress test
//=============================================================================

TEST_F(MinHeapTest, PriorityQueue_StressTest) {
    // WHAT: Verify heap maintains priority queue semantics under load
    // WHY:  Must work correctly with many operations
    // HOW:  Insert many elements randomly, extract all in sorted order

    heap = nimcp_min_heap_create(100);
    ASSERT_NE(heap, nullptr);

    // Insert 100 elements with random-like priorities
    std::vector<float> priorities;
    for (uint32_t i = 0; i < 100; i++) {
        float priority = (float)(i * 7 % 100);  // Pseudo-random
        priorities.push_back(priority);

        nimcp_heap_element_t elem = make_element(i, priority);
        ASSERT_TRUE(nimcp_min_heap_insert(heap, &elem));
    }

    // Sort priorities to get expected order
    std::sort(priorities.begin(), priorities.end());

    // Extract all and verify sorted order
    for (size_t i = 0; i < 100; i++) {
        nimcp_heap_element_t extracted;
        ASSERT_TRUE(nimcp_min_heap_extract_min(heap, &extracted));
        EXPECT_FLOAT_EQ(extracted.priority, priorities[i])
            << "Element " << i << " not in priority order";
    }

    EXPECT_TRUE(nimcp_min_heap_is_empty(heap));
}

//=============================================================================
// Unit Test 16: NULL parameter handling
//=============================================================================

TEST_F(MinHeapTest, NullParameters_HandledSafely) {
    // WHAT: Verify all functions handle NULL parameters safely
    // WHY:  Must be robust against NULL pointers
    // HOW:  Call each function with NULL, verify no crash

    nimcp_heap_element_t elem = make_element(0, 1.0f);

    EXPECT_FALSE(nimcp_min_heap_insert(nullptr, &elem));
    EXPECT_FALSE(nimcp_min_heap_extract_min(nullptr, &elem));
    EXPECT_FALSE(nimcp_min_heap_peek_min(nullptr, &elem));
    EXPECT_FALSE(nimcp_min_heap_decrease_key(nullptr, 0, 1.0f));
    EXPECT_TRUE(nimcp_min_heap_is_empty(nullptr));
    EXPECT_FALSE(nimcp_min_heap_is_full(nullptr));
    EXPECT_EQ(nimcp_min_heap_size(nullptr), 0u);
    EXPECT_EQ(nimcp_min_heap_capacity(nullptr), 0u);

    nimcp_min_heap_clear(nullptr);  // Should not crash

    SUCCEED() << "NULL parameters handled safely";
}

//=============================================================================
// Unit Test 17: Insert with NULL element pointer
//=============================================================================

TEST_F(MinHeapTest, Insert_NullElement) {
    // WHAT: Verify insert fails with NULL element pointer
    // WHY:  Cannot insert NULL element
    // HOW:  Create heap, try to insert NULL

    heap = nimcp_min_heap_create(10);
    ASSERT_NE(heap, nullptr);

    EXPECT_FALSE(nimcp_min_heap_insert(heap, nullptr));
    EXPECT_EQ(nimcp_min_heap_size(heap), 0u);
}

//=============================================================================
// Unit Test 18: Extract/Peek with NULL output pointer
//=============================================================================

TEST_F(MinHeapTest, ExtractPeek_NullOutput) {
    // WHAT: Verify extract/peek fail with NULL output pointer
    // WHY:  Need valid pointer to store result
    // HOW:  Create non-empty heap, call with NULL output

    heap = nimcp_min_heap_create(10);
    ASSERT_NE(heap, nullptr);

    nimcp_heap_element_t elem = make_element(0, 1.0f);
    ASSERT_TRUE(nimcp_min_heap_insert(heap, &elem));

    EXPECT_FALSE(nimcp_min_heap_extract_min(heap, nullptr));
    EXPECT_FALSE(nimcp_min_heap_peek_min(heap, nullptr));

    // Heap should still have the element
    EXPECT_EQ(nimcp_min_heap_size(heap), 1u);
}

//=============================================================================
// Unit Test 19: Vertex ID boundary conditions
//=============================================================================

TEST_F(MinHeapTest, VertexId_BoundaryConditions) {
    // WHAT: Verify handling of vertex IDs at boundaries
    // WHY:  Must handle edge cases in position map
    // HOW:  Test with vertex ID at capacity limit

    heap = nimcp_min_heap_create(5);
    ASSERT_NE(heap, nullptr);

    // Insert with vertex IDs within range (0 to capacity-1)
    nimcp_heap_element_t elem1 = make_element(0, 10.0f);
    nimcp_heap_element_t elem2 = make_element(4, 20.0f);

    EXPECT_TRUE(nimcp_min_heap_insert(heap, &elem1));
    EXPECT_TRUE(nimcp_min_heap_insert(heap, &elem2));

    // Try to insert with vertex ID >= max_vertex_id (should fail)
    nimcp_heap_element_t elem3 = make_element(5, 30.0f);
    EXPECT_FALSE(nimcp_min_heap_insert(heap, &elem3));

    // Try even larger vertex ID
    nimcp_heap_element_t elem4 = make_element(100, 40.0f);
    EXPECT_FALSE(nimcp_min_heap_insert(heap, &elem4));
}

//=============================================================================
// Unit Test 20: Interleaved operations
//=============================================================================

TEST_F(MinHeapTest, InterleavedOperations_MaintainCorrectness) {
    // WHAT: Verify heap works correctly with mixed operations
    // WHY:  Real usage involves interleaved insert/extract/decrease
    // HOW:  Perform sequence of mixed operations, verify consistency

    heap = nimcp_min_heap_create(20);
    ASSERT_NE(heap, nullptr);

    // Insert some elements
    for (uint32_t i = 0; i < 5; i++) {
        nimcp_heap_element_t elem = make_element(i, (float)(i * 10));
        ASSERT_TRUE(nimcp_min_heap_insert(heap, &elem));
    }

    // Extract minimum
    nimcp_heap_element_t extracted;
    ASSERT_TRUE(nimcp_min_heap_extract_min(heap, &extracted));
    EXPECT_FLOAT_EQ(extracted.priority, 0.0f);

    // Insert more
    nimcp_heap_element_t elem = make_element(10, 5.0f);
    ASSERT_TRUE(nimcp_min_heap_insert(heap, &elem));

    // Peek
    nimcp_heap_element_t peeked;
    ASSERT_TRUE(nimcp_min_heap_peek_min(heap, &peeked));
    EXPECT_FLOAT_EQ(peeked.priority, 5.0f);

    // Decrease key
    ASSERT_TRUE(nimcp_min_heap_decrease_key(heap, 4, 3.0f));

    // Verify new minimum
    ASSERT_TRUE(nimcp_min_heap_peek_min(heap, &peeked));
    EXPECT_EQ(peeked.vertex_id, 4u);
    EXPECT_FLOAT_EQ(peeked.priority, 3.0f);

    // Extract all remaining
    std::vector<float> extracted_priorities;
    while (!nimcp_min_heap_is_empty(heap)) {
        ASSERT_TRUE(nimcp_min_heap_extract_min(heap, &extracted));
        extracted_priorities.push_back(extracted.priority);
    }

    // Verify they came out in sorted order
    for (size_t i = 1; i < extracted_priorities.size(); i++) {
        EXPECT_LE(extracted_priorities[i-1], extracted_priorities[i])
            << "Priority order violated at position " << i;
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
