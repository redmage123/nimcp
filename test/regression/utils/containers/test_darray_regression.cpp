/**
 * @file test_darray_regression.cpp
 * @brief Regression tests for dynamic array - performance and edge case verification
 *
 * WHAT: Performance regression tests and edge case validation for darray
 * WHY:  Ensure performance characteristics remain stable and edge cases stay fixed
 * HOW:  Benchmark operations, test known edge cases, verify memory bounds
 *
 * REGRESSION COVERAGE:
 * 1. Performance: O(1) amortized push_back
 * 2. Performance: O(1) access by index
 * 3. Memory: Growth factor efficiency (2x)
 * 4. Edge case: Empty array operations
 * 5. Edge case: Single element array
 * 6. Edge case: Maximum index boundaries
 * 7. Edge case: Zero-size operations
 * 8. Memory: Destructor call ordering
 * 9. Memory: No double-free on clear+destroy
 * 10. Stability: Rapid resize cycles
 *
 * @version Regression Testing Framework v1.0
 * @date 2025-12-08
 */

#include <gtest/gtest.h>
#include <chrono>
#include <vector>
#include <algorithm>
#include <numeric>
#include <cmath>

// Headers have their own extern "C" guards
#include "utils/containers/nimcp_darray.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class DArrayRegressionTest : public ::testing::Test {
protected:
    using Clock = std::chrono::high_resolution_clock;
    using Duration = std::chrono::duration<double, std::micro>;

    // Performance measurement helper
    template<typename Func>
    double measure_time_us(Func&& func) {
        auto start = Clock::now();
        func();
        auto end = Clock::now();
        return Duration(end - start).count();
    }

    // Statistical helper
    double calculate_std_dev(const std::vector<double>& values) {
        if (values.empty()) return 0.0;
        double mean = std::accumulate(values.begin(), values.end(), 0.0) / values.size();
        double sq_sum = 0.0;
        for (double v : values) {
            sq_sum += (v - mean) * (v - mean);
        }
        return std::sqrt(sq_sum / values.size());
    }
};

// Destructor tracking
static std::vector<int> g_destructor_order;
static void tracking_destructor(void* element) {
    int* val = (int*)element;
    g_destructor_order.push_back(*val);
}

//=============================================================================
// Regression Test 1: Amortized O(1) push_back performance
//=============================================================================

TEST_F(DArrayRegressionTest, Performance_AmortizedConstantPushBack) {
    // WHAT: Verify push_back remains O(1) amortized
    // WHY:  Performance regression would indicate broken resize logic
    // HOW:  Measure average time per push across different sizes

    const int BATCH_SIZE = 1000;
    const int NUM_BATCHES = 10;

    nimcp_darray_t* arr = nimcp_darray_create(sizeof(int), 16);
    ASSERT_NE(arr, nullptr);

    std::vector<double> batch_times;

    for (int batch = 0; batch < NUM_BATCHES; batch++) {
        auto start = Clock::now();
        for (int i = 0; i < BATCH_SIZE; i++) {
            int val = batch * BATCH_SIZE + i;
            nimcp_darray_push_back(arr, &val);
        }
        auto end = Clock::now();

        double time_per_op = Duration(end - start).count() / BATCH_SIZE;
        batch_times.push_back(time_per_op);
    }

    // Calculate statistics
    double mean = std::accumulate(batch_times.begin(), batch_times.end(), 0.0) / batch_times.size();
    double std_dev = calculate_std_dev(batch_times);

    // Amortized O(1): later batches should not be significantly slower
    // Allow 3x standard deviation variance (accounting for resize costs)
    double max_allowed_ratio = 3.0;
    double first_batch = batch_times[0];
    double last_batch = batch_times.back();

    // Last batch should not be dramatically slower than first
    // (would indicate O(n) growth)
    EXPECT_LT(last_batch / first_batch, max_allowed_ratio)
        << "Push_back performance degraded: first=" << first_batch
        << "us, last=" << last_batch << "us";

    // Standard deviation should be reasonable (not wildly inconsistent)
    EXPECT_LT(std_dev / mean, 1.0)
        << "Push_back performance inconsistent: mean=" << mean
        << "us, std_dev=" << std_dev << "us";

    nimcp_darray_destroy(arr);
}

//=============================================================================
// Regression Test 2: O(1) random access
//=============================================================================

TEST_F(DArrayRegressionTest, Performance_ConstantTimeAccess) {
    // WHAT: Verify at() is O(1) regardless of array size
    // WHY:  Access must remain constant time
    // HOW:  Measure access time at various indices

    const int SIZE = 100000;
    nimcp_darray_t* arr = nimcp_darray_create(sizeof(int), SIZE);
    ASSERT_NE(arr, nullptr);

    // Fill array
    for (int i = 0; i < SIZE; i++) {
        nimcp_darray_push_back(arr, &i);
    }

    // Access at different positions
    std::vector<size_t> indices = {0, SIZE/4, SIZE/2, SIZE*3/4, SIZE-1};
    std::vector<double> access_times;

    for (size_t idx : indices) {
        double total = 0;
        const int ITERATIONS = 10000;

        auto start = Clock::now();
        for (int i = 0; i < ITERATIONS; i++) {
            volatile int* ptr = (int*)nimcp_darray_at(arr, idx);
            (void)ptr;
        }
        auto end = Clock::now();

        double time_per_access = Duration(end - start).count() / ITERATIONS;
        access_times.push_back(time_per_access);
    }

    // All access times should be similar (within 2x)
    double min_time = *std::min_element(access_times.begin(), access_times.end());
    double max_time = *std::max_element(access_times.begin(), access_times.end());

    EXPECT_LT(max_time / min_time, 2.0)
        << "Access time varies with index: min=" << min_time
        << "us, max=" << max_time << "us";

    nimcp_darray_destroy(arr);
}

//=============================================================================
// Regression Test 3: Growth factor efficiency
//=============================================================================

TEST_F(DArrayRegressionTest, Memory_GrowthFactorEfficiency) {
    // WHAT: Verify 2x growth factor is applied correctly
    // WHY:  Incorrect growth causes O(n^2) total copy operations
    // HOW:  Track capacity changes during growth

    nimcp_darray_t* arr = nimcp_darray_create(sizeof(int), 4);
    ASSERT_NE(arr, nullptr);

    std::vector<size_t> capacities;
    capacities.push_back(nimcp_darray_capacity(arr));

    size_t prev_capacity = 4;

    // Push until we've resized several times
    for (int i = 0; i < 1000; i++) {
        nimcp_darray_push_back(arr, &i);

        size_t curr_capacity = nimcp_darray_capacity(arr);
        if (curr_capacity != prev_capacity) {
            capacities.push_back(curr_capacity);
            prev_capacity = curr_capacity;
        }
    }

    // Verify growth factor is approximately 2x
    for (size_t i = 1; i < capacities.size(); i++) {
        double ratio = (double)capacities[i] / capacities[i-1];
        EXPECT_GE(ratio, 1.5) << "Growth factor too small at step " << i;
        EXPECT_LE(ratio, 2.5) << "Growth factor too large at step " << i;
    }

    nimcp_darray_destroy(arr);
}

//=============================================================================
// Regression Test 4: Empty array edge cases
//=============================================================================

TEST_F(DArrayRegressionTest, EdgeCase_EmptyArray) {
    // WHAT: All operations handle empty array correctly
    // WHY:  Prevents crashes/undefined behavior
    // HOW:  Test all operations on empty array

    nimcp_darray_t* arr = nimcp_darray_create(sizeof(int), 16);
    ASSERT_NE(arr, nullptr);

    // All these should be safe on empty array
    EXPECT_TRUE(nimcp_darray_is_empty(arr));
    EXPECT_EQ(nimcp_darray_size(arr), 0u);
    EXPECT_EQ(nimcp_darray_at(arr, 0), nullptr);
    EXPECT_EQ(nimcp_darray_front(arr), nullptr);
    EXPECT_EQ(nimcp_darray_back(arr), nullptr);
    EXPECT_EQ(nimcp_darray_data(arr), nullptr);

    int out;
    EXPECT_FALSE(nimcp_darray_pop_back(arr, &out));
    EXPECT_FALSE(nimcp_darray_remove_at(arr, 0, &out));

    // Clear empty array should be safe
    nimcp_darray_clear(arr);
    EXPECT_TRUE(nimcp_darray_is_empty(arr));

    // Shrink empty array
    EXPECT_TRUE(nimcp_darray_shrink_to_fit(arr));

    // Resize to 0 should work
    EXPECT_TRUE(nimcp_darray_resize(arr, 0));

    nimcp_darray_destroy(arr);
}

//=============================================================================
// Regression Test 5: Single element array
//=============================================================================

TEST_F(DArrayRegressionTest, EdgeCase_SingleElement) {
    // WHAT: All operations work correctly with single element
    // WHY:  Single element is boundary case
    // HOW:  Test operations with exactly one element

    nimcp_darray_t* arr = nimcp_darray_create(sizeof(int), 16);
    int val = 42;
    nimcp_darray_push_back(arr, &val);

    EXPECT_FALSE(nimcp_darray_is_empty(arr));
    EXPECT_EQ(nimcp_darray_size(arr), 1u);

    // Front and back should be same
    EXPECT_EQ(nimcp_darray_front(arr), nimcp_darray_back(arr));
    EXPECT_EQ(*(int*)nimcp_darray_front(arr), 42);

    // At(0) should work, at(1) should not
    EXPECT_NE(nimcp_darray_at(arr, 0), nullptr);
    EXPECT_EQ(nimcp_darray_at(arr, 1), nullptr);

    // Set should work
    int new_val = 99;
    EXPECT_TRUE(nimcp_darray_set(arr, 0, &new_val));
    EXPECT_EQ(*(int*)nimcp_darray_at(arr, 0), 99);

    // Insert at 0 should prepend
    int prepend_val = 1;
    EXPECT_TRUE(nimcp_darray_insert(arr, 0, &prepend_val));
    EXPECT_EQ(nimcp_darray_size(arr), 2u);
    EXPECT_EQ(*(int*)nimcp_darray_at(arr, 0), 1);
    EXPECT_EQ(*(int*)nimcp_darray_at(arr, 1), 99);

    // Remove first
    nimcp_darray_remove_at(arr, 0, nullptr);
    EXPECT_EQ(nimcp_darray_size(arr), 1u);
    EXPECT_EQ(*(int*)nimcp_darray_at(arr, 0), 99);

    // Pop last element
    int out;
    EXPECT_TRUE(nimcp_darray_pop_back(arr, &out));
    EXPECT_EQ(out, 99);
    EXPECT_TRUE(nimcp_darray_is_empty(arr));

    nimcp_darray_destroy(arr);
}

//=============================================================================
// Regression Test 6: Maximum index boundaries
//=============================================================================

TEST_F(DArrayRegressionTest, EdgeCase_MaximumBoundaries) {
    // WHAT: Large indices handled correctly
    // WHY:  Prevent overflow/underflow issues
    // HOW:  Test with SIZE_MAX and near-max values

    nimcp_darray_t* arr = nimcp_darray_create(sizeof(int), 16);
    int val = 42;
    nimcp_darray_push_back(arr, &val);

    // SIZE_MAX should not cause crash
    EXPECT_EQ(nimcp_darray_at(arr, SIZE_MAX), nullptr);
    EXPECT_FALSE(nimcp_darray_set(arr, SIZE_MAX, &val));
    EXPECT_FALSE(nimcp_darray_remove_at(arr, SIZE_MAX, nullptr));

    // SIZE_MAX - 1 should also be safe
    EXPECT_EQ(nimcp_darray_at(arr, SIZE_MAX - 1), nullptr);

    nimcp_darray_destroy(arr);
}

//=============================================================================
// Regression Test 7: Destructor call ordering
//=============================================================================

TEST_F(DArrayRegressionTest, Memory_DestructorOrdering) {
    // WHAT: Destructors called in correct order during operations
    // WHY:  Order matters for dependent resources
    // HOW:  Track destructor calls during various operations

    g_destructor_order.clear();

    nimcp_darray_t* arr = nimcp_darray_create_with_destructor(
        sizeof(int), 16, tracking_destructor);

    int vals[] = {10, 20, 30, 40, 50};
    for (int v : vals) {
        nimcp_darray_push_back(arr, &v);
    }

    // Pop should call destructor for removed element
    g_destructor_order.clear();
    nimcp_darray_pop_back(arr, nullptr);  // Remove 50
    ASSERT_EQ(g_destructor_order.size(), 1u);
    EXPECT_EQ(g_destructor_order[0], 50);

    // Remove at should call destructor
    g_destructor_order.clear();
    nimcp_darray_remove_at(arr, 1, nullptr);  // Remove 20
    ASSERT_EQ(g_destructor_order.size(), 1u);
    EXPECT_EQ(g_destructor_order[0], 20);

    // Resize down should call destructors for removed elements
    g_destructor_order.clear();
    nimcp_darray_resize(arr, 1);  // Keep only 10, remove 30, 40
    ASSERT_EQ(g_destructor_order.size(), 2u);

    // Clear should call all remaining destructors
    g_destructor_order.clear();
    nimcp_darray_clear(arr);
    ASSERT_EQ(g_destructor_order.size(), 1u);
    EXPECT_EQ(g_destructor_order[0], 10);

    nimcp_darray_destroy(arr);
}

//=============================================================================
// Regression Test 8: No double-free on clear+destroy
//=============================================================================

TEST_F(DArrayRegressionTest, Memory_NoDoubleFree) {
    // WHAT: Clear followed by destroy doesn't double-free
    // WHY:  Common usage pattern must be safe
    // HOW:  Clear then destroy, count destructor calls

    g_destructor_order.clear();

    nimcp_darray_t* arr = nimcp_darray_create_with_destructor(
        sizeof(int), 16, tracking_destructor);

    int vals[] = {1, 2, 3, 4, 5};
    for (int v : vals) {
        nimcp_darray_push_back(arr, &v);
    }

    // Clear calls destructors
    g_destructor_order.clear();
    nimcp_darray_clear(arr);
    EXPECT_EQ(g_destructor_order.size(), 5u);

    // Destroy should not call destructors again (array is empty)
    g_destructor_order.clear();
    nimcp_darray_destroy(arr);
    EXPECT_EQ(g_destructor_order.size(), 0u);
}

//=============================================================================
// Regression Test 9: Rapid resize cycles
//=============================================================================

TEST_F(DArrayRegressionTest, Stability_RapidResizeCycles) {
    // WHAT: Repeated grow/shrink cycles are stable
    // WHY:  Memory fragmentation and leak prevention
    // HOW:  Cycle through sizes multiple times

    nimcp_darray_t* arr = nimcp_darray_create(sizeof(int), 8);
    ASSERT_NE(arr, nullptr);

    for (int cycle = 0; cycle < 100; cycle++) {
        // Grow
        for (int i = 0; i < 100; i++) {
            int val = cycle * 1000 + i;
            EXPECT_TRUE(nimcp_darray_push_back(arr, &val));
        }
        EXPECT_EQ(nimcp_darray_size(arr), 100u);

        // Verify data integrity
        for (int i = 0; i < 100; i++) {
            int expected = cycle * 1000 + i;
            EXPECT_EQ(*(int*)nimcp_darray_at(arr, i), expected);
        }

        // Shrink partially
        nimcp_darray_resize(arr, 50);
        EXPECT_EQ(nimcp_darray_size(arr), 50u);

        // Verify remaining data
        for (int i = 0; i < 50; i++) {
            int expected = cycle * 1000 + i;
            EXPECT_EQ(*(int*)nimcp_darray_at(arr, i), expected);
        }

        // Clear completely
        nimcp_darray_clear(arr);
        EXPECT_EQ(nimcp_darray_size(arr), 0u);
    }

    nimcp_darray_destroy(arr);
}

//=============================================================================
// Regression Test 10: Insert/remove at boundaries
//=============================================================================

TEST_F(DArrayRegressionTest, EdgeCase_InsertRemoveBoundaries) {
    // WHAT: Insert/remove at first and last positions
    // WHY:  Boundary conditions in shift operations
    // HOW:  Test insert/remove at 0 and size-1

    nimcp_darray_t* arr = nimcp_darray_create(sizeof(int), 16);

    // Build array [10, 20, 30, 40, 50]
    int vals[] = {10, 20, 30, 40, 50};
    for (int v : vals) {
        nimcp_darray_push_back(arr, &v);
    }

    // Insert at 0 -> [5, 10, 20, 30, 40, 50]
    int val = 5;
    EXPECT_TRUE(nimcp_darray_insert(arr, 0, &val));
    EXPECT_EQ(*(int*)nimcp_darray_at(arr, 0), 5);
    EXPECT_EQ(*(int*)nimcp_darray_at(arr, 1), 10);
    EXPECT_EQ(nimcp_darray_size(arr), 6u);

    // Insert at size -> [5, 10, 20, 30, 40, 50, 60]
    val = 60;
    EXPECT_TRUE(nimcp_darray_insert(arr, 6, &val));
    EXPECT_EQ(*(int*)nimcp_darray_at(arr, 6), 60);
    EXPECT_EQ(nimcp_darray_size(arr), 7u);

    // Remove at 0 -> [10, 20, 30, 40, 50, 60]
    int out;
    EXPECT_TRUE(nimcp_darray_remove_at(arr, 0, &out));
    EXPECT_EQ(out, 5);
    EXPECT_EQ(*(int*)nimcp_darray_at(arr, 0), 10);
    EXPECT_EQ(nimcp_darray_size(arr), 6u);

    // Remove at size-1 -> [10, 20, 30, 40, 50]
    EXPECT_TRUE(nimcp_darray_remove_at(arr, 5, &out));
    EXPECT_EQ(out, 60);
    EXPECT_EQ(nimcp_darray_size(arr), 5u);

    // Verify final array
    int expected[] = {10, 20, 30, 40, 50};
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(*(int*)nimcp_darray_at(arr, i), expected[i]);
    }

    nimcp_darray_destroy(arr);
}

//=============================================================================
// Regression Test 11: Memory layout consistency
//=============================================================================

TEST_F(DArrayRegressionTest, Memory_LayoutConsistency) {
    // WHAT: Data pointer returns contiguous memory
    // WHY:  External code may rely on contiguous layout
    // HOW:  Verify pointer arithmetic matches at()

    nimcp_darray_t* arr = nimcp_darray_create(sizeof(int), 32);

    for (int i = 0; i < 20; i++) {
        nimcp_darray_push_back(arr, &i);
    }

    int* data = (int*)nimcp_darray_data(arr);
    ASSERT_NE(data, nullptr);

    // Pointer arithmetic should match at()
    for (int i = 0; i < 20; i++) {
        int* via_at = (int*)nimcp_darray_at(arr, i);
        int* via_ptr = data + i;
        EXPECT_EQ(via_at, via_ptr)
            << "Memory layout mismatch at index " << i;
    }

    nimcp_darray_destroy(arr);
}

//=============================================================================
// Regression Test 12: Set operation calls destructor
//=============================================================================

TEST_F(DArrayRegressionTest, Memory_SetCallsDestructor) {
    // WHAT: Set should call destructor for replaced element
    // WHY:  Prevent memory leaks when replacing elements
    // HOW:  Set element, verify destructor called once

    g_destructor_order.clear();

    nimcp_darray_t* arr = nimcp_darray_create_with_destructor(
        sizeof(int), 16, tracking_destructor);

    int val = 100;
    nimcp_darray_push_back(arr, &val);
    val = 200;
    nimcp_darray_push_back(arr, &val);

    // Set should destroy old value
    g_destructor_order.clear();
    int new_val = 999;
    nimcp_darray_set(arr, 0, &new_val);

    ASSERT_EQ(g_destructor_order.size(), 1u);
    EXPECT_EQ(g_destructor_order[0], 100);

    // Verify new value is set
    EXPECT_EQ(*(int*)nimcp_darray_at(arr, 0), 999);

    nimcp_darray_destroy(arr);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
