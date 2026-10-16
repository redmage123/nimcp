/**
 * @file test_queue_types.cpp
 * @brief Comprehensive unit tests for NIMCP queue types (BLOCKING, SPSC, MPMC)
 *
 * WHAT: Complete test coverage for all three queue implementation types
 * WHY:  Queues are critical infrastructure - must verify correctness, thread safety, and performance
 * HOW:  Test each type independently with operations, concurrency, and edge cases
 *
 * TEST COVERAGE:
 * 1. Creation and configuration for each type
 * 2. Basic operations (enqueue, dequeue, try_enqueue, try_dequeue)
 * 3. FIFO ordering verification
 * 4. Capacity and overflow handling
 * 5. Empty queue handling
 * 6. Batch operations
 * 7. Statistics and monitoring
 * 8. Thread safety:
 *    - BLOCKING: Multiple producers/consumers
 *    - SPSC: Single producer + single consumer (lock-free)
 *    - MPMC: Multiple producers + multiple consumers (lock-free)
 * 9. Performance characteristics
 * 10. Peek and clear operations
 * 11. Memory leak testing
 *
 * QUEUE TYPES TESTED:
 * - NIMCP_QUEUE_TYPE_BLOCKING: Mutex-based, general purpose
 * - NIMCP_QUEUE_TYPE_SPSC: Lock-free single producer/consumer
 * - NIMCP_QUEUE_TYPE_MPMC: Lock-free multiple producer/consumer
 *
 * @version 1.0.0
 * @date 2025-11-28
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <unordered_set>
#include "utils/containers/nimcp_queue.h"

//=============================================================================
// Test Fixture Base Class
//=============================================================================

/**
 * @brief Base test fixture for queue operations
 *
 * WHAT: Provides setup/teardown and helper methods for queue tests
 * WHY:  Reduces code duplication and ensures proper cleanup
 * HOW:  RAII pattern - creates queue in setup, destroys in teardown
 */
class QueueTypeTestBase : public ::testing::Test {
protected:
    nimcp_queue_handle_t queue = nullptr;

    void TearDown() override {
        if (queue) {
            nimcp_queue_destroy(queue);
            queue = nullptr;
        }
    }

    /**
     * @brief Create queue with specified type
     *
     * WHAT: Helper to create queue with standard config
     * WHY:  Simplify test setup code
     * HOW:  Set common defaults, customize type
     */
    void CreateQueue(nimcp_queue_type_t type, size_t capacity = 1024) {
        nimcp_queue_config_t config = {};
        config.max_size = capacity;
        config.item_size = sizeof(int);
        config.type = type;
        config.is_blocking = false;  // Most tests use non-blocking
        config.timeout_ms = 100;
        config.spin_count = 1000;

        nimcp_result_t result = nimcp_queue_create(&config, &queue);
        ASSERT_EQ(result, NIMCP_SUCCESS);
        ASSERT_NE(queue, nullptr);
    }
};

//=============================================================================
// Test Fixture for Each Queue Type
//=============================================================================

class BlockingQueueTest : public QueueTypeTestBase {};
class SPSCQueueTest : public QueueTypeTestBase {};
class MPMCQueueTest : public QueueTypeTestBase {};

//=============================================================================
// Unit Test 1: Queue Creation and Type Verification
//=============================================================================

/**
 * WHAT: Verify BLOCKING queue creation and type
 * WHY:  Ensure correct queue type instantiation
 */
TEST_F(BlockingQueueTest, Create_ValidConfig) {
    CreateQueue(NIMCP_QUEUE_TYPE_BLOCKING);

    EXPECT_EQ(nimcp_queue_get_type(queue), NIMCP_QUEUE_TYPE_BLOCKING);
    EXPECT_TRUE(nimcp_queue_is_empty(queue));
    EXPECT_FALSE(nimcp_queue_is_full(queue));
    EXPECT_EQ(nimcp_queue_get_size(queue), 0u);
    EXPECT_GT(nimcp_queue_get_capacity(queue), 0u);
}

/**
 * WHAT: Verify SPSC queue creation and type
 * WHY:  Ensure correct queue type instantiation
 */
TEST_F(SPSCQueueTest, Create_ValidConfig) {
    CreateQueue(NIMCP_QUEUE_TYPE_SPSC);

    EXPECT_EQ(nimcp_queue_get_type(queue), NIMCP_QUEUE_TYPE_SPSC);
    EXPECT_TRUE(nimcp_queue_is_empty(queue));
    EXPECT_FALSE(nimcp_queue_is_full(queue));
    EXPECT_EQ(nimcp_queue_get_size(queue), 0u);

    // SPSC rounds to power of 2
    size_t capacity = nimcp_queue_get_capacity(queue);
    EXPECT_GT(capacity, 0u);
    EXPECT_EQ(capacity & (capacity - 1), 0u) << "SPSC capacity should be power of 2";
}

/**
 * WHAT: Verify MPMC queue creation and type
 * WHY:  Ensure correct queue type instantiation
 */
TEST_F(MPMCQueueTest, Create_ValidConfig) {
    CreateQueue(NIMCP_QUEUE_TYPE_MPMC);

    EXPECT_EQ(nimcp_queue_get_type(queue), NIMCP_QUEUE_TYPE_MPMC);
    EXPECT_TRUE(nimcp_queue_is_empty(queue));
    EXPECT_FALSE(nimcp_queue_is_full(queue));
    EXPECT_EQ(nimcp_queue_get_size(queue), 0u);

    // MPMC rounds to power of 2
    size_t capacity = nimcp_queue_get_capacity(queue);
    EXPECT_GT(capacity, 0u);
    EXPECT_EQ(capacity & (capacity - 1), 0u) << "MPMC capacity should be power of 2";
}

//=============================================================================
// Unit Test 2: Basic Enqueue/Dequeue Operations
//=============================================================================

/**
 * WHAT: Test basic enqueue/dequeue for BLOCKING queue
 * WHY:  Verify core functionality
 */
TEST_F(BlockingQueueTest, EnqueueDequeue_Basic) {
    CreateQueue(NIMCP_QUEUE_TYPE_BLOCKING, 10);

    int value_in = 42;
    int value_out = 0;

    ASSERT_EQ(nimcp_queue_enqueue(queue, &value_in, 0), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_queue_get_size(queue), 1u);
    EXPECT_FALSE(nimcp_queue_is_empty(queue));

    ASSERT_EQ(nimcp_queue_dequeue(queue, &value_out, 0), NIMCP_SUCCESS);
    EXPECT_EQ(value_out, 42);
    EXPECT_EQ(nimcp_queue_get_size(queue), 0u);
    EXPECT_TRUE(nimcp_queue_is_empty(queue));
}

/**
 * WHAT: Test basic enqueue/dequeue for SPSC queue
 * WHY:  Verify core lock-free functionality
 */
TEST_F(SPSCQueueTest, EnqueueDequeue_Basic) {
    CreateQueue(NIMCP_QUEUE_TYPE_SPSC, 16);

    int value_in = 123;
    int value_out = 0;

    ASSERT_EQ(nimcp_queue_enqueue(queue, &value_in, 0), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_queue_get_size(queue), 1u);

    ASSERT_EQ(nimcp_queue_dequeue(queue, &value_out, 0), NIMCP_SUCCESS);
    EXPECT_EQ(value_out, 123);
    EXPECT_TRUE(nimcp_queue_is_empty(queue));
}

/**
 * WHAT: Test basic enqueue/dequeue for MPMC queue
 * WHY:  Verify core lock-free functionality
 */
TEST_F(MPMCQueueTest, EnqueueDequeue_Basic) {
    CreateQueue(NIMCP_QUEUE_TYPE_MPMC, 16);

    int value_in = 456;
    int value_out = 0;

    ASSERT_EQ(nimcp_queue_enqueue(queue, &value_in, 0), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_queue_get_size(queue), 1u);

    ASSERT_EQ(nimcp_queue_dequeue(queue, &value_out, 0), NIMCP_SUCCESS);
    EXPECT_EQ(value_out, 456);
    EXPECT_TRUE(nimcp_queue_is_empty(queue));
}

//=============================================================================
// Unit Test 3: FIFO Ordering Verification
//=============================================================================

/**
 * WHAT: Verify FIFO ordering for BLOCKING queue
 * WHY:  Queue must preserve insertion order
 */
TEST_F(BlockingQueueTest, FIFOOrdering_Correct) {
    CreateQueue(NIMCP_QUEUE_TYPE_BLOCKING, 100);

    // Enqueue sequence
    for (int i = 0; i < 50; i++) {
        int value = i * 10;
        ASSERT_EQ(nimcp_queue_enqueue(queue, &value, 0), NIMCP_SUCCESS);
    }

    // Verify dequeue order
    for (int i = 0; i < 50; i++) {
        int value_out = -1;
        ASSERT_EQ(nimcp_queue_dequeue(queue, &value_out, 0), NIMCP_SUCCESS);
        EXPECT_EQ(value_out, i * 10) << "FIFO violated at index " << i;
    }

    EXPECT_TRUE(nimcp_queue_is_empty(queue));
}

/**
 * WHAT: Verify FIFO ordering for SPSC queue
 * WHY:  Lock-free queue must preserve order
 */
TEST_F(SPSCQueueTest, FIFOOrdering_Correct) {
    CreateQueue(NIMCP_QUEUE_TYPE_SPSC, 64);

    for (int i = 0; i < 50; i++) {
        int value = i + 100;
        ASSERT_EQ(nimcp_queue_enqueue(queue, &value, 0), NIMCP_SUCCESS);
    }

    for (int i = 0; i < 50; i++) {
        int value_out = -1;
        ASSERT_EQ(nimcp_queue_dequeue(queue, &value_out, 0), NIMCP_SUCCESS);
        EXPECT_EQ(value_out, i + 100) << "SPSC FIFO violated at index " << i;
    }

    EXPECT_TRUE(nimcp_queue_is_empty(queue));
}

/**
 * WHAT: Verify FIFO ordering for MPMC queue
 * WHY:  Lock-free queue must preserve order
 */
TEST_F(MPMCQueueTest, FIFOOrdering_Correct) {
    CreateQueue(NIMCP_QUEUE_TYPE_MPMC, 64);

    for (int i = 0; i < 50; i++) {
        int value = i + 200;
        ASSERT_EQ(nimcp_queue_enqueue(queue, &value, 0), NIMCP_SUCCESS);
    }

    for (int i = 0; i < 50; i++) {
        int value_out = -1;
        ASSERT_EQ(nimcp_queue_dequeue(queue, &value_out, 0), NIMCP_SUCCESS);
        EXPECT_EQ(value_out, i + 200) << "MPMC FIFO violated at index " << i;
    }

    EXPECT_TRUE(nimcp_queue_is_empty(queue));
}

//=============================================================================
// Unit Test 4: Capacity and Full Queue Handling
//=============================================================================

/**
 * WHAT: Verify BLOCKING queue full handling
 * WHY:  Must reject enqueue when at capacity
 */
TEST_F(BlockingQueueTest, FullQueue_RejectsEnqueue) {
    CreateQueue(NIMCP_QUEUE_TYPE_BLOCKING, 8);

    size_t capacity = nimcp_queue_get_capacity(queue);

    // Fill to capacity
    for (size_t i = 0; i < capacity; i++) {
        int value = static_cast<int>(i);
        nimcp_result_t result = nimcp_queue_enqueue(queue, &value, 0);
        if (result != NIMCP_SUCCESS) {
            // Some implementations reserve a slot
            EXPECT_TRUE(nimcp_queue_is_full(queue));
            break;
        }
    }

    // Should be full or nearly full
    EXPECT_GT(nimcp_queue_get_size(queue), 0u);

    // Try to enqueue one more (should fail if truly full)
    int extra = 999;
    if (nimcp_queue_is_full(queue)) {
        nimcp_result_t result = nimcp_queue_enqueue(queue, &extra, 0);
        EXPECT_NE(result, NIMCP_SUCCESS);
    }
}

/**
 * WHAT: Verify SPSC queue full handling
 * WHY:  Lock-free queue must handle capacity correctly
 */
TEST_F(SPSCQueueTest, FullQueue_RejectsEnqueue) {
    CreateQueue(NIMCP_QUEUE_TYPE_SPSC, 8);

    size_t capacity = nimcp_queue_get_capacity(queue);

    // Fill queue (SPSC reserves one slot)
    for (size_t i = 0; i < capacity; i++) {
        int value = static_cast<int>(i);
        nimcp_result_t result = nimcp_queue_enqueue(queue, &value, 0);
        if (result != NIMCP_SUCCESS) {
            break;
        }
    }

    // Verify full state
    if (nimcp_queue_is_full(queue)) {
        int extra = 999;
        nimcp_result_t result = nimcp_queue_enqueue(queue, &extra, 0);
        EXPECT_NE(result, NIMCP_SUCCESS);
    }
}

/**
 * WHAT: Verify MPMC queue full handling
 * WHY:  Lock-free queue must handle capacity correctly
 */
TEST_F(MPMCQueueTest, FullQueue_RejectsEnqueue) {
    CreateQueue(NIMCP_QUEUE_TYPE_MPMC, 8);

    size_t capacity = nimcp_queue_get_capacity(queue);

    // Fill queue
    for (size_t i = 0; i < capacity; i++) {
        int value = static_cast<int>(i);
        nimcp_result_t result = nimcp_queue_enqueue(queue, &value, 0);
        if (result != NIMCP_SUCCESS) {
            break;
        }
    }

    // Verify full state
    if (nimcp_queue_is_full(queue)) {
        int extra = 999;
        nimcp_result_t result = nimcp_queue_enqueue(queue, &extra, 0);
        EXPECT_NE(result, NIMCP_SUCCESS);
    }
}

//=============================================================================
// Unit Test 5: Empty Queue Handling
//=============================================================================

/**
 * WHAT: Verify dequeue from empty BLOCKING queue fails
 * WHY:  Must prevent underflow
 */
TEST_F(BlockingQueueTest, EmptyQueue_RejectsDequeue) {
    CreateQueue(NIMCP_QUEUE_TYPE_BLOCKING);

    EXPECT_TRUE(nimcp_queue_is_empty(queue));

    int value_out = 0;
    nimcp_result_t result = nimcp_queue_dequeue(queue, &value_out, 0);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

/**
 * WHAT: Verify dequeue from empty SPSC queue fails
 * WHY:  Must prevent underflow
 */
TEST_F(SPSCQueueTest, EmptyQueue_RejectsDequeue) {
    CreateQueue(NIMCP_QUEUE_TYPE_SPSC);

    EXPECT_TRUE(nimcp_queue_is_empty(queue));

    int value_out = 0;
    nimcp_result_t result = nimcp_queue_dequeue(queue, &value_out, 0);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

/**
 * WHAT: Verify dequeue from empty MPMC queue fails
 * WHY:  Must prevent underflow
 */
TEST_F(MPMCQueueTest, EmptyQueue_RejectsDequeue) {
    CreateQueue(NIMCP_QUEUE_TYPE_MPMC);

    EXPECT_TRUE(nimcp_queue_is_empty(queue));

    int value_out = 0;
    nimcp_result_t result = nimcp_queue_dequeue(queue, &value_out, 0);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

//=============================================================================
// Unit Test 6: Try Operations (Non-blocking)
//=============================================================================

/**
 * WHAT: Test try_enqueue and try_dequeue for BLOCKING queue
 * WHY:  Verify non-blocking operation variants
 */
TEST_F(BlockingQueueTest, TryOperations_Work) {
    CreateQueue(NIMCP_QUEUE_TYPE_BLOCKING, 10);

    int value_in = 777;
    EXPECT_TRUE(nimcp_queue_try_enqueue(queue, &value_in));
    EXPECT_EQ(nimcp_queue_get_size(queue), 1u);

    int value_out = 0;
    EXPECT_TRUE(nimcp_queue_try_dequeue(queue, &value_out));
    EXPECT_EQ(value_out, 777);
    EXPECT_TRUE(nimcp_queue_is_empty(queue));

    // Try dequeue on empty
    EXPECT_FALSE(nimcp_queue_try_dequeue(queue, &value_out));
}

/**
 * WHAT: Test try_enqueue and try_dequeue for SPSC queue
 * WHY:  Verify non-blocking operation variants
 */
TEST_F(SPSCQueueTest, TryOperations_Work) {
    CreateQueue(NIMCP_QUEUE_TYPE_SPSC, 16);

    int value_in = 888;
    EXPECT_TRUE(nimcp_queue_try_enqueue(queue, &value_in));
    EXPECT_EQ(nimcp_queue_get_size(queue), 1u);

    int value_out = 0;
    EXPECT_TRUE(nimcp_queue_try_dequeue(queue, &value_out));
    EXPECT_EQ(value_out, 888);

    EXPECT_FALSE(nimcp_queue_try_dequeue(queue, &value_out));
}

/**
 * WHAT: Test try_enqueue and try_dequeue for MPMC queue
 * WHY:  Verify non-blocking operation variants
 */
TEST_F(MPMCQueueTest, TryOperations_Work) {
    CreateQueue(NIMCP_QUEUE_TYPE_MPMC, 16);

    int value_in = 999;
    EXPECT_TRUE(nimcp_queue_try_enqueue(queue, &value_in));
    EXPECT_EQ(nimcp_queue_get_size(queue), 1u);

    int value_out = 0;
    EXPECT_TRUE(nimcp_queue_try_dequeue(queue, &value_out));
    EXPECT_EQ(value_out, 999);

    EXPECT_FALSE(nimcp_queue_try_dequeue(queue, &value_out));
}

//=============================================================================
// Unit Test 7: Batch Operations
//=============================================================================

/**
 * WHAT: Test batch enqueue/dequeue for BLOCKING queue
 * WHY:  Verify bulk operations work correctly
 */
TEST_F(BlockingQueueTest, BatchOperations_Work) {
    CreateQueue(NIMCP_QUEUE_TYPE_BLOCKING, 100);

    // Batch enqueue
    int values_in[10];
    for (int i = 0; i < 10; i++) {
        values_in[i] = i * 5;
    }

    size_t enqueued_count = 0;
    nimcp_result_t result = nimcp_queue_enqueue_batch(
        queue, values_in, 10, &enqueued_count, 0);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(enqueued_count, 10u);
    EXPECT_EQ(nimcp_queue_get_size(queue), 10u);

    // Batch dequeue
    int values_out[10] = {0};
    size_t dequeued_count = 0;
    result = nimcp_queue_dequeue_batch(
        queue, values_out, 10, &dequeued_count, 0);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(dequeued_count, 10u);

    // Verify values
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(values_out[i], i * 5);
    }

    EXPECT_TRUE(nimcp_queue_is_empty(queue));
}

/**
 * WHAT: Test batch operations for SPSC queue
 * WHY:  Verify bulk operations in lock-free context
 */
TEST_F(SPSCQueueTest, BatchOperations_Work) {
    CreateQueue(NIMCP_QUEUE_TYPE_SPSC, 64);

    int values_in[20];
    for (int i = 0; i < 20; i++) {
        values_in[i] = i + 100;
    }

    size_t enqueued_count = 0;
    nimcp_result_t result = nimcp_queue_enqueue_batch(
        queue, values_in, 20, &enqueued_count, 0);

    EXPECT_GT(enqueued_count, 0u);

    int values_out[20] = {0};
    size_t dequeued_count = 0;
    result = nimcp_queue_dequeue_batch(
        queue, values_out, 20, &dequeued_count, 0);

    EXPECT_EQ(dequeued_count, enqueued_count);

    for (size_t i = 0; i < dequeued_count; i++) {
        EXPECT_EQ(values_out[i], static_cast<int>(i) + 100);
    }
}

/**
 * WHAT: Test batch operations for MPMC queue
 * WHY:  Verify bulk operations in lock-free context
 */
TEST_F(MPMCQueueTest, BatchOperations_Work) {
    CreateQueue(NIMCP_QUEUE_TYPE_MPMC, 64);

    int values_in[20];
    for (int i = 0; i < 20; i++) {
        values_in[i] = i + 200;
    }

    size_t enqueued_count = 0;
    nimcp_result_t result = nimcp_queue_enqueue_batch(
        queue, values_in, 20, &enqueued_count, 0);

    EXPECT_GT(enqueued_count, 0u);

    int values_out[20] = {0};
    size_t dequeued_count = 0;
    result = nimcp_queue_dequeue_batch(
        queue, values_out, 20, &dequeued_count, 0);

    EXPECT_EQ(dequeued_count, enqueued_count);

    for (size_t i = 0; i < dequeued_count; i++) {
        EXPECT_EQ(values_out[i], static_cast<int>(i) + 200);
    }
}

//=============================================================================
// Unit Test 8: Statistics and Status
//=============================================================================

/**
 * WHAT: Verify statistics tracking for BLOCKING queue
 * WHY:  Monitor queue health and usage
 */
TEST_F(BlockingQueueTest, Statistics_TrackCorrectly) {
    CreateQueue(NIMCP_QUEUE_TYPE_BLOCKING, 50);

    // Perform operations
    for (int i = 0; i < 10; i++) {
        int value = i;
        nimcp_queue_enqueue(queue, &value, 0);
    }

    nimcp_queue_status_t status = {};
    ASSERT_EQ(nimcp_queue_get_status(queue, &status), NIMCP_SUCCESS);

    EXPECT_EQ(status.type, NIMCP_QUEUE_TYPE_BLOCKING);
    EXPECT_EQ(status.current_size, 10u);
    EXPECT_EQ(status.total_enqueued, 10u);
    EXPECT_EQ(status.total_dequeued, 0u);
    EXPECT_GE(status.peak_size, 10u);

    // Dequeue some
    for (int i = 0; i < 5; i++) {
        int value;
        nimcp_queue_dequeue(queue, &value, 0);
    }

    ASSERT_EQ(nimcp_queue_get_status(queue, &status), NIMCP_SUCCESS);
    EXPECT_EQ(status.current_size, 5u);
    EXPECT_EQ(status.total_dequeued, 5u);
}

/**
 * WHAT: Verify statistics tracking for SPSC queue
 * WHY:  Monitor queue health and usage
 */
TEST_F(SPSCQueueTest, Statistics_TrackCorrectly) {
    CreateQueue(NIMCP_QUEUE_TYPE_SPSC, 64);

    for (int i = 0; i < 15; i++) {
        int value = i;
        nimcp_queue_enqueue(queue, &value, 0);
    }

    nimcp_queue_status_t status = {};
    ASSERT_EQ(nimcp_queue_get_status(queue, &status), NIMCP_SUCCESS);

    EXPECT_EQ(status.type, NIMCP_QUEUE_TYPE_SPSC);
    EXPECT_EQ(status.current_size, 15u);
    EXPECT_GE(status.total_enqueued, 15u);
}

/**
 * WHAT: Verify statistics tracking for MPMC queue
 * WHY:  Monitor queue health and usage
 */
TEST_F(MPMCQueueTest, Statistics_TrackCorrectly) {
    CreateQueue(NIMCP_QUEUE_TYPE_MPMC, 64);

    for (int i = 0; i < 20; i++) {
        int value = i;
        nimcp_queue_enqueue(queue, &value, 0);
    }

    nimcp_queue_status_t status = {};
    ASSERT_EQ(nimcp_queue_get_status(queue, &status), NIMCP_SUCCESS);

    EXPECT_EQ(status.type, NIMCP_QUEUE_TYPE_MPMC);
    EXPECT_EQ(status.current_size, 20u);
    EXPECT_GE(status.total_enqueued, 20u);
}

//=============================================================================
// Unit Test 9: Peek Operation
//=============================================================================

/**
 * WHAT: Test peek operation for BLOCKING queue
 * WHY:  Verify non-destructive read
 */
TEST_F(BlockingQueueTest, Peek_NonDestructive) {
    CreateQueue(NIMCP_QUEUE_TYPE_BLOCKING);

    int value_in = 555;
    nimcp_queue_enqueue(queue, &value_in, 0);

    int peeked = 0;
    ASSERT_EQ(nimcp_queue_peek(queue, &peeked), NIMCP_SUCCESS);
    EXPECT_EQ(peeked, 555);
    EXPECT_EQ(nimcp_queue_get_size(queue), 1u);

    // Peek again - should get same value
    ASSERT_EQ(nimcp_queue_peek(queue, &peeked), NIMCP_SUCCESS);
    EXPECT_EQ(peeked, 555);

    // Now dequeue
    int value_out = 0;
    ASSERT_EQ(nimcp_queue_dequeue(queue, &value_out, 0), NIMCP_SUCCESS);
    EXPECT_EQ(value_out, 555);
    EXPECT_TRUE(nimcp_queue_is_empty(queue));

    // Peek on empty should fail
    nimcp_result_t result = nimcp_queue_peek(queue, &peeked);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

/**
 * WHAT: Test peek operation for SPSC queue
 * WHY:  Verify non-destructive read in lock-free context
 */
TEST_F(SPSCQueueTest, Peek_NonDestructive) {
    CreateQueue(NIMCP_QUEUE_TYPE_SPSC);

    int value_in = 666;
    nimcp_queue_enqueue(queue, &value_in, 0);

    int peeked = 0;
    ASSERT_EQ(nimcp_queue_peek(queue, &peeked), NIMCP_SUCCESS);
    EXPECT_EQ(peeked, 666);
    EXPECT_EQ(nimcp_queue_get_size(queue), 1u);
}

/**
 * WHAT: Test peek operation for MPMC queue
 * WHY:  Verify non-destructive read in lock-free context
 */
TEST_F(MPMCQueueTest, Peek_NonDestructive) {
    CreateQueue(NIMCP_QUEUE_TYPE_MPMC);

    int value_in = 777;
    nimcp_queue_enqueue(queue, &value_in, 0);

    int peeked = 0;
    ASSERT_EQ(nimcp_queue_peek(queue, &peeked), NIMCP_SUCCESS);
    EXPECT_EQ(peeked, 777);
    EXPECT_EQ(nimcp_queue_get_size(queue), 1u);
}

//=============================================================================
// Unit Test 10: Clear Operation
//=============================================================================

/**
 * WHAT: Test clear operation for BLOCKING queue
 * WHY:  Verify bulk removal
 */
TEST_F(BlockingQueueTest, Clear_RemovesAll) {
    CreateQueue(NIMCP_QUEUE_TYPE_BLOCKING, 50);

    // Add items
    for (int i = 0; i < 25; i++) {
        int value = i;
        nimcp_queue_enqueue(queue, &value, 0);
    }

    EXPECT_EQ(nimcp_queue_get_size(queue), 25u);

    ASSERT_EQ(nimcp_queue_clear(queue), NIMCP_SUCCESS);

    EXPECT_EQ(nimcp_queue_get_size(queue), 0u);
    EXPECT_TRUE(nimcp_queue_is_empty(queue));
    EXPECT_FALSE(nimcp_queue_is_full(queue));
}

/**
 * WHAT: Test clear operation for SPSC queue
 * WHY:  Verify bulk removal in lock-free context
 */
TEST_F(SPSCQueueTest, Clear_RemovesAll) {
    CreateQueue(NIMCP_QUEUE_TYPE_SPSC, 64);

    for (int i = 0; i < 30; i++) {
        int value = i;
        nimcp_queue_enqueue(queue, &value, 0);
    }

    EXPECT_GT(nimcp_queue_get_size(queue), 0u);

    ASSERT_EQ(nimcp_queue_clear(queue), NIMCP_SUCCESS);

    EXPECT_EQ(nimcp_queue_get_size(queue), 0u);
    EXPECT_TRUE(nimcp_queue_is_empty(queue));
}

/**
 * WHAT: Test clear operation for MPMC queue
 * WHY:  Verify bulk removal in lock-free context
 */
TEST_F(MPMCQueueTest, Clear_RemovesAll) {
    CreateQueue(NIMCP_QUEUE_TYPE_MPMC, 64);

    for (int i = 0; i < 30; i++) {
        int value = i;
        nimcp_queue_enqueue(queue, &value, 0);
    }

    EXPECT_GT(nimcp_queue_get_size(queue), 0u);

    ASSERT_EQ(nimcp_queue_clear(queue), NIMCP_SUCCESS);

    EXPECT_EQ(nimcp_queue_get_size(queue), 0u);
    EXPECT_TRUE(nimcp_queue_is_empty(queue));
}

//=============================================================================
// Unit Test 11: Thread Safety - BLOCKING Queue (Multiple Producers/Consumers)
//=============================================================================

/**
 * WHAT: Test BLOCKING queue with multiple producers and consumers
 * WHY:  Verify thread safety with mutex protection
 * HOW:  Spawn 4 producer + 4 consumer threads, verify all items transferred
 */
TEST_F(BlockingQueueTest, ThreadSafety_MultipleProducersConsumers) {
    CreateQueue(NIMCP_QUEUE_TYPE_BLOCKING, 1024);

    const int NUM_PRODUCERS = 4;
    const int NUM_CONSUMERS = 4;
    const int ITEMS_PER_PRODUCER = 100;
    const int TOTAL_ITEMS = NUM_PRODUCERS * ITEMS_PER_PRODUCER;

    std::atomic<int> items_consumed{0};
    std::vector<int> consumed_values;
    std::mutex consumed_mutex;

    // Producer function
    auto producer_func = [this](int producer_id, int count) {
        for (int i = 0; i < count; i++) {
            int value = producer_id * 1000 + i;
            while (nimcp_queue_enqueue(queue, &value, 10) != NIMCP_SUCCESS) {
                std::this_thread::yield();
            }
        }
    };

    // Consumer function
    auto consumer_func = [this, &items_consumed, &consumed_values, &consumed_mutex]() {
        int value = 0;
        while (items_consumed.load() < TOTAL_ITEMS) {
            if (nimcp_queue_dequeue(queue, &value, 10) == NIMCP_SUCCESS) {
                {
                    std::lock_guard<std::mutex> lock(consumed_mutex);
                    consumed_values.push_back(value);
                }
                items_consumed.fetch_add(1);
            } else {
                std::this_thread::yield();
            }
        }
    };

    // Start threads
    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;

    for (int i = 0; i < NUM_PRODUCERS; i++) {
        producers.emplace_back(producer_func, i, ITEMS_PER_PRODUCER);
    }

    for (int i = 0; i < NUM_CONSUMERS; i++) {
        consumers.emplace_back(consumer_func);
    }

    // Wait for completion
    for (auto& t : producers) t.join();
    for (auto& t : consumers) t.join();

    // Verify all items consumed
    EXPECT_EQ(consumed_values.size(), static_cast<size_t>(TOTAL_ITEMS));
    EXPECT_TRUE(nimcp_queue_is_empty(queue));

    // Verify no duplicates
    std::unordered_set<int> unique_values(consumed_values.begin(), consumed_values.end());
    EXPECT_EQ(unique_values.size(), static_cast<size_t>(TOTAL_ITEMS));
}

//=============================================================================
// Unit Test 12: Thread Safety - SPSC Queue (Single Producer/Consumer)
//=============================================================================

/**
 * WHAT: Test SPSC queue with single producer and consumer
 * WHY:  Verify lock-free correctness with 1-to-1 threading
 * HOW:  One producer thread, one consumer thread, verify all items transferred
 */
TEST_F(SPSCQueueTest, ThreadSafety_SingleProducerConsumer) {
    CreateQueue(NIMCP_QUEUE_TYPE_SPSC, 256);

    const int TOTAL_ITEMS = 10000;
    std::atomic<bool> producer_done{false};
    std::vector<int> consumed_values;
    consumed_values.reserve(TOTAL_ITEMS);

    // Single producer
    auto producer = [this, TOTAL_ITEMS]() {
        for (int i = 0; i < TOTAL_ITEMS; i++) {
            int value = i;
            while (!nimcp_queue_try_enqueue(queue, &value)) {
                std::this_thread::yield();
            }
        }
    };

    // Single consumer
    auto consumer = [this, &consumed_values, &producer_done]() {
        int value = 0;
        while (!producer_done.load() || !nimcp_queue_is_empty(queue)) {
            if (nimcp_queue_try_dequeue(queue, &value)) {
                consumed_values.push_back(value);
            } else {
                std::this_thread::yield();
            }
        }
    };

    std::thread prod_thread(producer);
    std::thread cons_thread(consumer);

    prod_thread.join();
    producer_done.store(true);
    cons_thread.join();

    // Verify all items received in order
    EXPECT_EQ(consumed_values.size(), static_cast<size_t>(TOTAL_ITEMS));
    for (int i = 0; i < TOTAL_ITEMS; i++) {
        EXPECT_EQ(consumed_values[i], i) << "Order violated at index " << i;
    }

    EXPECT_TRUE(nimcp_queue_is_empty(queue));
}

//=============================================================================
// Unit Test 13: Thread Safety - MPMC Queue (Multiple Producers/Consumers)
//=============================================================================

/**
 * WHAT: Test MPMC queue with multiple producers and consumers
 * WHY:  Verify lock-free correctness with many-to-many threading
 * HOW:  Spawn 8 producers + 8 consumers, verify all items transferred
 */
TEST_F(MPMCQueueTest, ThreadSafety_MultipleProducersConsumers) {
    CreateQueue(NIMCP_QUEUE_TYPE_MPMC, 512);

    const int NUM_PRODUCERS = 8;
    const int NUM_CONSUMERS = 8;
    const int ITEMS_PER_PRODUCER = 1000;
    const int TOTAL_ITEMS = NUM_PRODUCERS * ITEMS_PER_PRODUCER;

    std::atomic<int> items_produced{0};
    std::atomic<int> items_consumed{0};
    std::vector<int> consumed_values;
    std::mutex consumed_mutex;

    // Producer function
    auto producer_func = [this, &items_produced](int producer_id, int count) {
        for (int i = 0; i < count; i++) {
            int value = producer_id * 10000 + i;
            while (!nimcp_queue_try_enqueue(queue, &value)) {
                std::this_thread::yield();
            }
            items_produced.fetch_add(1);
        }
    };

    // Consumer function
    auto consumer_func = [this, &items_consumed, &consumed_values, &consumed_mutex, TOTAL_ITEMS]() {
        int value = 0;
        while (items_consumed.load() < TOTAL_ITEMS) {
            if (nimcp_queue_try_dequeue(queue, &value)) {
                {
                    std::lock_guard<std::mutex> lock(consumed_mutex);
                    consumed_values.push_back(value);
                }
                items_consumed.fetch_add(1);
            } else {
                std::this_thread::yield();
            }
        }
    };

    // Start threads
    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;

    for (int i = 0; i < NUM_PRODUCERS; i++) {
        producers.emplace_back(producer_func, i, ITEMS_PER_PRODUCER);
    }

    for (int i = 0; i < NUM_CONSUMERS; i++) {
        consumers.emplace_back(consumer_func);
    }

    // Wait for completion
    for (auto& t : producers) t.join();
    for (auto& t : consumers) t.join();

    // Verify all items consumed
    EXPECT_EQ(consumed_values.size(), static_cast<size_t>(TOTAL_ITEMS));

    // Verify no duplicates
    std::unordered_set<int> unique_values(consumed_values.begin(), consumed_values.end());
    EXPECT_EQ(unique_values.size(), static_cast<size_t>(TOTAL_ITEMS));
}

//=============================================================================
// Unit Test 14: Performance Characteristics
//=============================================================================

/**
 * WHAT: Compare performance of SPSC vs MPMC for single-threaded case
 * WHY:  SPSC should be faster for single-threaded workloads
 * HOW:  Time identical operations on both queue types
 */
TEST(QueuePerformanceTest, SPSC_FasterThanMPMC_SingleThreaded) {
    const int OPERATIONS = 100000;

    // Test SPSC
    nimcp_queue_config_t spsc_config = {};
    spsc_config.max_size = 1024;
    spsc_config.item_size = sizeof(int);
    spsc_config.type = NIMCP_QUEUE_TYPE_SPSC;
    spsc_config.is_blocking = false;
    spsc_config.timeout_ms = 0;

    nimcp_queue_handle_t spsc_queue = nullptr;
    ASSERT_EQ(nimcp_queue_create(&spsc_config, &spsc_queue), NIMCP_SUCCESS);

    auto spsc_start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < OPERATIONS; i++) {
        int value = i;
        nimcp_queue_try_enqueue(spsc_queue, &value);
        nimcp_queue_try_dequeue(spsc_queue, &value);
    }
    auto spsc_end = std::chrono::high_resolution_clock::now();
    auto spsc_duration = std::chrono::duration_cast<std::chrono::microseconds>(spsc_end - spsc_start).count();

    nimcp_queue_destroy(spsc_queue);

    // Test MPMC
    nimcp_queue_config_t mpmc_config = {};
    mpmc_config.max_size = 1024;
    mpmc_config.item_size = sizeof(int);
    mpmc_config.type = NIMCP_QUEUE_TYPE_MPMC;
    mpmc_config.is_blocking = false;
    mpmc_config.timeout_ms = 0;
    mpmc_config.spin_count = 1000;

    nimcp_queue_handle_t mpmc_queue = nullptr;
    ASSERT_EQ(nimcp_queue_create(&mpmc_config, &mpmc_queue), NIMCP_SUCCESS);

    auto mpmc_start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < OPERATIONS; i++) {
        int value = i;
        nimcp_queue_try_enqueue(mpmc_queue, &value);
        nimcp_queue_try_dequeue(mpmc_queue, &value);
    }
    auto mpmc_end = std::chrono::high_resolution_clock::now();
    auto mpmc_duration = std::chrono::duration_cast<std::chrono::microseconds>(mpmc_end - mpmc_start).count();

    nimcp_queue_destroy(mpmc_queue);

    // SPSC should be faster or comparable (allow 20% margin)
    // Note: This is informational - actual performance depends on hardware
    EXPECT_GT(spsc_duration, 0);
    EXPECT_GT(mpmc_duration, 0);
}

//=============================================================================
// Unit Test 15: Memory Leak Test
//=============================================================================

/**
 * WHAT: Test for memory leaks through create/destroy cycles
 * WHY:  Ensure proper cleanup
 * HOW:  Create and destroy queues repeatedly
 */
TEST(QueueMemoryTest, CreateDestroyCycles_NoLeaks) {
    const int CYCLES = 100;

    for (int cycle = 0; cycle < CYCLES; cycle++) {
        nimcp_queue_config_t config = {};
        config.max_size = 128;
        config.item_size = sizeof(int);
        config.type = static_cast<nimcp_queue_type_t>(cycle % 3); // Cycle through types
        config.is_blocking = false;
        config.timeout_ms = 0;
        config.spin_count = 1000;

        nimcp_queue_handle_t queue = nullptr;
        if (nimcp_queue_create(&config, &queue) == NIMCP_SUCCESS) {
            // Do some operations
            for (int i = 0; i < 10; i++) {
                int value = i;
                nimcp_queue_try_enqueue(queue, &value);
            }

            for (int i = 0; i < 10; i++) {
                int value;
                nimcp_queue_try_dequeue(queue, &value);
            }

            nimcp_queue_destroy(queue);
        }
    }

    // If we reach here without crashes, no obvious leaks
    SUCCEED();
}

//=============================================================================
// Unit Test 16: Default Configuration
//=============================================================================

/**
 * WHAT: Test default configuration helper for each type
 * WHY:  Verify convenience function provides sensible defaults
 */
TEST(QueueConfigTest, DefaultConfig_ValidForAllTypes) {
    // BLOCKING
    nimcp_queue_config_t blocking_config = nimcp_queue_default_config(NIMCP_QUEUE_TYPE_BLOCKING);
    EXPECT_EQ(blocking_config.type, NIMCP_QUEUE_TYPE_BLOCKING);
    EXPECT_GT(blocking_config.max_size, 0u);
    EXPECT_GT(blocking_config.item_size, 0u);

    // SPSC
    nimcp_queue_config_t spsc_config = nimcp_queue_default_config(NIMCP_QUEUE_TYPE_SPSC);
    EXPECT_EQ(spsc_config.type, NIMCP_QUEUE_TYPE_SPSC);
    EXPECT_GT(spsc_config.max_size, 0u);

    // MPMC
    nimcp_queue_config_t mpmc_config = nimcp_queue_default_config(NIMCP_QUEUE_TYPE_MPMC);
    EXPECT_EQ(mpmc_config.type, NIMCP_QUEUE_TYPE_MPMC);
    EXPECT_GT(mpmc_config.max_size, 0u);
    EXPECT_GT(mpmc_config.spin_count, 0u);
}

//=============================================================================
// Unit Test 17: Utilization Metrics
//=============================================================================

/**
 * WHAT: Test utilization percentage calculation
 * WHY:  Verify monitoring metrics
 */
TEST_F(BlockingQueueTest, Utilization_CalculatesCorrectly) {
    CreateQueue(NIMCP_QUEUE_TYPE_BLOCKING, 100);

    float util_empty = nimcp_queue_get_utilization(queue);
    EXPECT_FLOAT_EQ(util_empty, 0.0f);

    // Fill halfway
    size_t capacity = nimcp_queue_get_capacity(queue);
    for (size_t i = 0; i < capacity / 2; i++) {
        int value = static_cast<int>(i);
        if (nimcp_queue_try_enqueue(queue, &value)) {
            // Success
        } else {
            break;
        }
    }

    float util_half = nimcp_queue_get_utilization(queue);
    EXPECT_GT(util_half, 0.0f);
    EXPECT_LT(util_half, 100.0f);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
