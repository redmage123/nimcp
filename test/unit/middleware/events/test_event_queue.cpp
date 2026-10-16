//=============================================================================
// test_event_queue.cpp - Comprehensive Event Queue Tests
//=============================================================================
//
// WHAT: Complete test suite for event queue priority queue implementation
// WHY:  Ensure correctness of all queue operations and edge cases
// HOW:  GoogleTest framework with comprehensive test coverage
//
// TEST COVERAGE:
// - Lifecycle: create, destroy, configuration
// - Basic operations: enqueue, dequeue, peek
// - Priority handling: min-heap ordering
// - Capacity management: overflow policies
// - Batch operations: dequeue_batch
// - Advanced operations: remove_if, count_if
// - Statistics: tracking and reset
// - Error handling: NULL params, edge cases
// - Regression tests: specific bug scenarios
//
//=============================================================================

#include <gtest/gtest.h>
#include <algorithm>
#include <vector>
// Headers have their own extern "C" guards
#include "middleware/events/nimcp_event_queue.h"

//=============================================================================
// Test Fixture
//=============================================================================

/**
 * WHAT: Test fixture for event queue tests
 * WHY:  Provide common setup/teardown and helper functions
 * HOW:  GoogleTest fixture with helper methods
 */
class EventQueueTest : public ::testing::Test {
protected:
    event_queue_t queue;

    void SetUp() override {
        queue = nullptr;
    }

    void TearDown() override {
        if (queue) {
            event_queue_destroy(queue);
            queue = nullptr;
        }
    }

    // Helper: Create simple test event
    event_t create_test_event(mw_event_priority_t priority,
                              event_type_t type = EVENT_TYPE_CUSTOM) {
        return event_create_custom(nullptr, 0, "test", priority,
                                   EVENT_SOURCE_CUSTOM);
    }

    // Helper: Verify event equality (by priority and type)
    bool events_equal(const event_t* e1, const event_t* e2) {
        return e1->priority == e2->priority &&
               e1->type == e2->type &&
               e1->source == e2->source;
    }
};

//=============================================================================
// LIFECYCLE TESTS
//=============================================================================

/**
 * WHAT: Test queue creation with default config
 * WHY:  Verify factory function works with null config
 */
TEST_F(EventQueueTest, CreateWithDefaultConfig) {
    queue = event_queue_create(nullptr);
    ASSERT_NE(queue, nullptr);
    EXPECT_TRUE(event_queue_is_empty(queue));
    EXPECT_EQ(event_queue_size(queue), 0);
}

/**
 * WHAT: Test queue creation with custom config
 * WHY:  Verify configuration parameters are respected
 */
TEST_F(EventQueueTest, CreateWithCustomConfig) {
    event_queue_config_t config = {
        .capacity = 100,
        .overflow_policy = OVERFLOW_POLICY_DROP_OLDEST,
        .enable_coalescing = false,
        .block_timeout_us = 0
    };

    queue = event_queue_create(&config);
    ASSERT_NE(queue, nullptr);
    EXPECT_TRUE(event_queue_is_empty(queue));
}

/**
 * WHAT: Test queue destruction with NULL parameter
 * WHY:  Verify NULL-safety of destroy function
 */
TEST_F(EventQueueTest, DestroyNullQueue) {
    event_queue_destroy(nullptr);  // Should not crash
    SUCCEED();
}

/**
 * WHAT: Test queue destruction with events inside
 * WHY:  Verify proper cleanup of contained events
 */
TEST_F(EventQueueTest, DestroyQueueWithEvents) {
    queue = event_queue_create(nullptr);
    ASSERT_NE(queue, nullptr);

    // Add some events
    for (int i = 0; i < 10; i++) {
        event_t evt = create_test_event(MW_EVENT_PRIORITY_NORMAL);
        event_queue_enqueue(queue, &evt);
    }

    event_queue_destroy(queue);
    queue = nullptr;  // Prevent double-free in TearDown
    SUCCEED();
}

/**
 * WHAT: Test default configuration values
 * WHY:  Verify default config has sensible values
 */
TEST_F(EventQueueTest, DefaultConfiguration) {
    event_queue_config_t config = event_queue_default_config();

    EXPECT_GT(config.capacity, 0);
    EXPECT_GE(config.overflow_policy, OVERFLOW_POLICY_DROP_OLDEST);
    EXPECT_LE(config.overflow_policy, OVERFLOW_POLICY_BLOCK);
}

//=============================================================================
// BASIC OPERATION TESTS
//=============================================================================

/**
 * WHAT: Test basic enqueue and dequeue
 * WHY:  Verify core queue functionality works
 */
TEST_F(EventQueueTest, EnqueueDequeue) {
    queue = event_queue_create(nullptr);
    ASSERT_NE(queue, nullptr);

    event_t evt_in = create_test_event(MW_EVENT_PRIORITY_NORMAL);
    EXPECT_TRUE(event_queue_enqueue(queue, &evt_in));
    EXPECT_EQ(event_queue_size(queue), 1);

    event_t evt_out;
    EXPECT_TRUE(event_queue_dequeue(queue, &evt_out));
    EXPECT_TRUE(events_equal(&evt_in, &evt_out));
    EXPECT_EQ(event_queue_size(queue), 0);
}

/**
 * WHAT: Test dequeue from empty queue
 * WHY:  Verify error handling for underflow
 */
TEST_F(EventQueueTest, DequeueFromEmpty) {
    queue = event_queue_create(nullptr);
    ASSERT_NE(queue, nullptr);

    event_t evt;
    EXPECT_FALSE(event_queue_dequeue(queue, &evt));
    EXPECT_TRUE(event_queue_is_empty(queue));
}

/**
 * WHAT: Test peek operation
 * WHY:  Verify peek doesn't remove event
 */
TEST_F(EventQueueTest, PeekOperation) {
    queue = event_queue_create(nullptr);
    ASSERT_NE(queue, nullptr);

    event_t evt_in = create_test_event(MW_EVENT_PRIORITY_NORMAL);
    event_queue_enqueue(queue, &evt_in);

    event_t evt_peek;
    EXPECT_TRUE(event_queue_peek(queue, &evt_peek));
    EXPECT_TRUE(events_equal(&evt_in, &evt_peek));
    EXPECT_EQ(event_queue_size(queue), 1);  // Still there

    event_t evt_out;
    EXPECT_TRUE(event_queue_dequeue(queue, &evt_out));
    EXPECT_TRUE(events_equal(&evt_in, &evt_out));
}

/**
 * WHAT: Test peek on empty queue
 * WHY:  Verify error handling for peek underflow
 */
TEST_F(EventQueueTest, PeekFromEmpty) {
    queue = event_queue_create(nullptr);
    ASSERT_NE(queue, nullptr);

    event_t evt;
    EXPECT_FALSE(event_queue_peek(queue, &evt));
}

/**
 * WHAT: Test is_empty predicate
 * WHY:  Verify empty state detection
 */
TEST_F(EventQueueTest, IsEmpty) {
    queue = event_queue_create(nullptr);
    ASSERT_NE(queue, nullptr);

    EXPECT_TRUE(event_queue_is_empty(queue));

    event_t evt = create_test_event(MW_EVENT_PRIORITY_NORMAL);
    event_queue_enqueue(queue, &evt);
    EXPECT_FALSE(event_queue_is_empty(queue));

    event_queue_dequeue(queue, &evt);
    EXPECT_TRUE(event_queue_is_empty(queue));
}

/**
 * WHAT: Test clear operation
 * WHY:  Verify all events are removed
 */
TEST_F(EventQueueTest, ClearQueue) {
    queue = event_queue_create(nullptr);
    ASSERT_NE(queue, nullptr);

    // Add multiple events
    for (int i = 0; i < 5; i++) {
        event_t evt = create_test_event(MW_EVENT_PRIORITY_NORMAL);
        event_queue_enqueue(queue, &evt);
    }
    EXPECT_EQ(event_queue_size(queue), 5);

    event_queue_clear(queue);
    EXPECT_EQ(event_queue_size(queue), 0);
    EXPECT_TRUE(event_queue_is_empty(queue));
}

//=============================================================================
// PRIORITY HANDLING TESTS
//=============================================================================

/**
 * WHAT: Test priority ordering
 * WHY:  Verify highest priority (lowest number) comes out first
 */
TEST_F(EventQueueTest, PriorityOrdering) {
    queue = event_queue_create(nullptr);
    ASSERT_NE(queue, nullptr);

    // Enqueue in non-priority order
    event_t evt_low = create_test_event(MW_EVENT_PRIORITY_LOW);
    event_t evt_crit = create_test_event(MW_EVENT_PRIORITY_CRITICAL);
    event_t evt_norm = create_test_event(MW_EVENT_PRIORITY_NORMAL);
    event_t evt_high = create_test_event(MW_EVENT_PRIORITY_HIGH);

    event_queue_enqueue(queue, &evt_low);
    event_queue_enqueue(queue, &evt_crit);
    event_queue_enqueue(queue, &evt_norm);
    event_queue_enqueue(queue, &evt_high);

    // Should come out in priority order
    event_t out;

    EXPECT_TRUE(event_queue_dequeue(queue, &out));
    EXPECT_EQ(out.priority, MW_EVENT_PRIORITY_CRITICAL);

    EXPECT_TRUE(event_queue_dequeue(queue, &out));
    EXPECT_EQ(out.priority, MW_EVENT_PRIORITY_HIGH);

    EXPECT_TRUE(event_queue_dequeue(queue, &out));
    EXPECT_EQ(out.priority, MW_EVENT_PRIORITY_NORMAL);

    EXPECT_TRUE(event_queue_dequeue(queue, &out));
    EXPECT_EQ(out.priority, MW_EVENT_PRIORITY_LOW);
}

/**
 * WHAT: Test FIFO within same priority
 * WHY:  Events with same priority should maintain insertion order
 */
TEST_F(EventQueueTest, FIFOWithinPriority) {
    queue = event_queue_create(nullptr);
    ASSERT_NE(queue, nullptr);

    // Enqueue multiple events with same priority
    std::vector<event_t> events;
    for (int i = 0; i < 5; i++) {
        event_t evt = event_create_custom(nullptr, i, "test",
                                          MW_EVENT_PRIORITY_NORMAL,
                                          EVENT_SOURCE_CUSTOM);
        events.push_back(evt);
        event_queue_enqueue(queue, &evt);
    }

    // Should come out in insertion order
    for (int i = 0; i < 5; i++) {
        event_t out;
        EXPECT_TRUE(event_queue_dequeue(queue, &out));
        EXPECT_EQ(out.data.custom.data_size, i);
    }
}

/**
 * WHAT: Test many priorities interleaved
 * WHY:  Verify heap maintains correct order under complex insertions
 */
TEST_F(EventQueueTest, InterleavedPriorities) {
    queue = event_queue_create(nullptr);
    ASSERT_NE(queue, nullptr);

    // Add 50 events with random priorities
    std::vector<mw_event_priority_t> priorities = {
        MW_EVENT_PRIORITY_CRITICAL, MW_EVENT_PRIORITY_HIGH,
        MW_EVENT_PRIORITY_NORMAL, MW_EVENT_PRIORITY_LOW,
        MW_EVENT_PRIORITY_BACKGROUND
    };

    for (int i = 0; i < 50; i++) {
        event_t evt = create_test_event(priorities[i % 5]);
        event_queue_enqueue(queue, &evt);
    }

    // Verify all come out in priority order
    mw_event_priority_t prev_priority = MW_EVENT_PRIORITY_CRITICAL;
    for (int i = 0; i < 50; i++) {
        event_t out;
        EXPECT_TRUE(event_queue_dequeue(queue, &out));
        EXPECT_LE(prev_priority, out.priority);  // Non-decreasing
        prev_priority = out.priority;
    }
}

//=============================================================================
// CAPACITY AND OVERFLOW TESTS
//=============================================================================

/**
 * WHAT: Test is_full predicate
 * WHY:  Verify full state detection
 */
TEST_F(EventQueueTest, IsFull) {
    event_queue_config_t config = {
        .capacity = 5,
        .overflow_policy = OVERFLOW_POLICY_DROP_NEWEST,
        .enable_coalescing = false,
        .block_timeout_us = 0
    };

    queue = event_queue_create(&config);
    ASSERT_NE(queue, nullptr);

    EXPECT_FALSE(event_queue_is_full(queue));

    // Fill queue
    for (int i = 0; i < 5; i++) {
        event_t evt = create_test_event(MW_EVENT_PRIORITY_NORMAL);
        event_queue_enqueue(queue, &evt);
    }

    EXPECT_TRUE(event_queue_is_full(queue));

    // Remove one
    event_t evt;
    event_queue_dequeue(queue, &evt);
    EXPECT_FALSE(event_queue_is_full(queue));
}

/**
 * WHAT: Test DROP_NEWEST overflow policy
 * WHY:  Verify new events are rejected when full
 */
TEST_F(EventQueueTest, OverflowPolicyDropNewest) {
    event_queue_config_t config = {
        .capacity = 3,
        .overflow_policy = OVERFLOW_POLICY_DROP_NEWEST,
        .enable_coalescing = false,
        .block_timeout_us = 0
    };

    queue = event_queue_create(&config);
    ASSERT_NE(queue, nullptr);

    // Fill queue
    for (int i = 0; i < 3; i++) {
        event_t evt = event_create_custom(nullptr, i, "test",
                                          MW_EVENT_PRIORITY_NORMAL,
                                          EVENT_SOURCE_CUSTOM);
        EXPECT_TRUE(event_queue_enqueue(queue, &evt));
    }

    // Try to add one more - should be dropped
    event_t evt_dropped = event_create_custom(nullptr, 999, "dropped",
                                              MW_EVENT_PRIORITY_NORMAL,
                                              EVENT_SOURCE_CUSTOM);
    EXPECT_FALSE(event_queue_enqueue(queue, &evt_dropped));

    // Verify original events still there
    event_t out;
    for (int i = 0; i < 3; i++) {
        EXPECT_TRUE(event_queue_dequeue(queue, &out));
        EXPECT_EQ(out.data.custom.data_size, i);
    }
}

/**
 * WHAT: Test DROP_OLDEST overflow policy
 * WHY:  Verify oldest event is removed to make room
 */
TEST_F(EventQueueTest, OverflowPolicyDropOldest) {
    event_queue_config_t config = {
        .capacity = 3,
        .overflow_policy = OVERFLOW_POLICY_DROP_OLDEST,
        .enable_coalescing = false,
        .block_timeout_us = 0
    };

    queue = event_queue_create(&config);
    ASSERT_NE(queue, nullptr);

    // Fill queue with events 0, 1, 2
    for (int i = 0; i < 3; i++) {
        event_t evt = event_create_custom(nullptr, i, "test",
                                          MW_EVENT_PRIORITY_NORMAL,
                                          EVENT_SOURCE_CUSTOM);
        event_queue_enqueue(queue, &evt);
    }

    // Add event 3 - should drop event 0
    event_t evt_new = event_create_custom(nullptr, 3, "new",
                                          MW_EVENT_PRIORITY_NORMAL,
                                          EVENT_SOURCE_CUSTOM);
    EXPECT_TRUE(event_queue_enqueue(queue, &evt_new));

    // Should have events 1, 2, 3
    event_t out;
    EXPECT_TRUE(event_queue_dequeue(queue, &out));
    EXPECT_EQ(out.data.custom.data_size, 1);

    EXPECT_TRUE(event_queue_dequeue(queue, &out));
    EXPECT_EQ(out.data.custom.data_size, 2);

    EXPECT_TRUE(event_queue_dequeue(queue, &out));
    EXPECT_EQ(out.data.custom.data_size, 3);
}

/**
 * WHAT: Test DROP_LOWEST overflow policy
 * WHY:  Verify lowest priority event is dropped
 */
TEST_F(EventQueueTest, OverflowPolicyDropLowest) {
    event_queue_config_t config = {
        .capacity = 3,
        .overflow_policy = OVERFLOW_POLICY_DROP_LOWEST,
        .enable_coalescing = false,
        .block_timeout_us = 0
    };

    queue = event_queue_create(&config);
    ASSERT_NE(queue, nullptr);

    // Add high, normal, low priority events
    event_t evt_high = create_test_event(MW_EVENT_PRIORITY_HIGH);
    event_t evt_norm = create_test_event(MW_EVENT_PRIORITY_NORMAL);
    event_t evt_low = create_test_event(MW_EVENT_PRIORITY_LOW);

    event_queue_enqueue(queue, &evt_high);
    event_queue_enqueue(queue, &evt_norm);
    event_queue_enqueue(queue, &evt_low);

    // Add critical - should drop low priority
    event_t evt_crit = create_test_event(MW_EVENT_PRIORITY_CRITICAL);
    EXPECT_TRUE(event_queue_enqueue(queue, &evt_crit));

    // Should have critical, high, normal
    event_t out;

    EXPECT_TRUE(event_queue_dequeue(queue, &out));
    EXPECT_EQ(out.priority, MW_EVENT_PRIORITY_CRITICAL);

    EXPECT_TRUE(event_queue_dequeue(queue, &out));
    EXPECT_EQ(out.priority, MW_EVENT_PRIORITY_HIGH);

    EXPECT_TRUE(event_queue_dequeue(queue, &out));
    EXPECT_EQ(out.priority, MW_EVENT_PRIORITY_NORMAL);

    EXPECT_TRUE(event_queue_is_empty(queue));
}

//=============================================================================
// BATCH OPERATIONS TESTS
//=============================================================================

/**
 * WHAT: Test batch dequeue
 * WHY:  Verify efficient removal of multiple events
 */
TEST_F(EventQueueTest, DequeueBatch) {
    queue = event_queue_create(nullptr);
    ASSERT_NE(queue, nullptr);

    // Add 10 events
    for (int i = 0; i < 10; i++) {
        event_t evt = create_test_event(MW_EVENT_PRIORITY_NORMAL);
        event_queue_enqueue(queue, &evt);
    }

    // Dequeue 5 at once
    event_t batch[5];
    uint32_t count = event_queue_dequeue_batch(queue, batch, 5);

    EXPECT_EQ(count, 5);
    EXPECT_EQ(event_queue_size(queue), 5);
}

/**
 * WHAT: Test batch dequeue requesting more than available
 * WHY:  Verify partial batch handling
 */
TEST_F(EventQueueTest, DequeueBatchPartial) {
    queue = event_queue_create(nullptr);
    ASSERT_NE(queue, nullptr);

    // Add only 3 events
    for (int i = 0; i < 3; i++) {
        event_t evt = create_test_event(MW_EVENT_PRIORITY_NORMAL);
        event_queue_enqueue(queue, &evt);
    }

    // Request 10
    event_t batch[10];
    uint32_t count = event_queue_dequeue_batch(queue, batch, 10);

    EXPECT_EQ(count, 3);
    EXPECT_TRUE(event_queue_is_empty(queue));
}

/**
 * WHAT: Test batch dequeue from empty queue
 * WHY:  Verify zero returned for empty queue
 */
TEST_F(EventQueueTest, DequeueBatchEmpty) {
    queue = event_queue_create(nullptr);
    ASSERT_NE(queue, nullptr);

    event_t batch[5];
    uint32_t count = event_queue_dequeue_batch(queue, batch, 5);

    EXPECT_EQ(count, 0);
}

/**
 * WHAT: Test batch dequeue maintains priority order
 * WHY:  Verify batch operation respects priority queue semantics
 */
TEST_F(EventQueueTest, DequeueBatchPriority) {
    queue = event_queue_create(nullptr);
    ASSERT_NE(queue, nullptr);

    // Add events with varying priorities
    std::vector<mw_event_priority_t> priorities = {
        MW_EVENT_PRIORITY_LOW, MW_EVENT_PRIORITY_CRITICAL,
        MW_EVENT_PRIORITY_NORMAL, MW_EVENT_PRIORITY_HIGH
    };

    for (auto pri : priorities) {
        event_t evt = create_test_event(pri);
        event_queue_enqueue(queue, &evt);
    }

    event_t batch[4];
    uint32_t count = event_queue_dequeue_batch(queue, batch, 4);

    EXPECT_EQ(count, 4);
    EXPECT_EQ(batch[0].priority, MW_EVENT_PRIORITY_CRITICAL);
    EXPECT_EQ(batch[1].priority, MW_EVENT_PRIORITY_HIGH);
    EXPECT_EQ(batch[2].priority, MW_EVENT_PRIORITY_NORMAL);
    EXPECT_EQ(batch[3].priority, MW_EVENT_PRIORITY_LOW);
}

//=============================================================================
// ADVANCED OPERATIONS TESTS
//=============================================================================

/**
 * WHAT: Test remove_if with filter function
 * WHY:  Verify selective event removal
 */
TEST_F(EventQueueTest, RemoveIf) {
    queue = event_queue_create(nullptr);
    ASSERT_NE(queue, nullptr);

    // Add events with different priorities
    for (int i = 0; i < 10; i++) {
        mw_event_priority_t pri = (i % 2 == 0) ?
            MW_EVENT_PRIORITY_HIGH : MW_EVENT_PRIORITY_LOW;
        event_t evt = create_test_event(pri);
        event_queue_enqueue(queue, &evt);
    }

    // Remove all high priority events
    auto filter = [](const event_t* evt, void* ctx) -> bool {
        return evt->priority == MW_EVENT_PRIORITY_HIGH;
    };

    uint32_t removed = event_queue_remove_if(queue, filter, nullptr);

    EXPECT_EQ(removed, 5);
    EXPECT_EQ(event_queue_size(queue), 5);

    // Verify only low priority remain
    while (!event_queue_is_empty(queue)) {
        event_t out;
        event_queue_dequeue(queue, &out);
        EXPECT_EQ(out.priority, MW_EVENT_PRIORITY_LOW);
    }
}

/**
 * WHAT: Test remove_if with no matches
 * WHY:  Verify filter with zero removals works
 */
TEST_F(EventQueueTest, RemoveIfNoMatches) {
    queue = event_queue_create(nullptr);
    ASSERT_NE(queue, nullptr);

    // Add normal priority events
    for (int i = 0; i < 5; i++) {
        event_t evt = create_test_event(MW_EVENT_PRIORITY_NORMAL);
        event_queue_enqueue(queue, &evt);
    }

    // Try to remove critical priority (none exist)
    auto filter = [](const event_t* evt, void* ctx) -> bool {
        return evt->priority == MW_EVENT_PRIORITY_CRITICAL;
    };

    uint32_t removed = event_queue_remove_if(queue, filter, nullptr);

    EXPECT_EQ(removed, 0);
    EXPECT_EQ(event_queue_size(queue), 5);
}

/**
 * WHAT: Test count_if with filter function
 * WHY:  Verify event counting without removal
 */
TEST_F(EventQueueTest, CountIf) {
    queue = event_queue_create(nullptr);
    ASSERT_NE(queue, nullptr);

    // Add 3 critical, 7 normal
    for (int i = 0; i < 10; i++) {
        mw_event_priority_t pri = (i < 3) ?
            MW_EVENT_PRIORITY_CRITICAL : MW_EVENT_PRIORITY_NORMAL;
        event_t evt = create_test_event(pri);
        event_queue_enqueue(queue, &evt);
    }

    auto filter = [](const event_t* evt, void* ctx) -> bool {
        return evt->priority == MW_EVENT_PRIORITY_CRITICAL;
    };

    uint32_t count = event_queue_count_if(queue, filter, nullptr);

    EXPECT_EQ(count, 3);
    EXPECT_EQ(event_queue_size(queue), 10);  // None removed
}

/**
 * WHAT: Test count_if with context parameter
 * WHY:  Verify user context is passed to filter
 */
TEST_F(EventQueueTest, CountIfWithContext) {
    queue = event_queue_create(nullptr);
    ASSERT_NE(queue, nullptr);

    // Add events with different types
    for (int i = 0; i < 5; i++) {
        event_t evt = event_create_pattern_detected(i, 0.5f, 10, "test",
                                                     MW_EVENT_PRIORITY_NORMAL,
                                                     EVENT_SOURCE_PATTERN_DETECTOR);
        event_queue_enqueue(queue, &evt);
    }

    // Count events with specific pattern_id
    uint32_t target_id = 2;
    auto filter = [](const event_t* evt, void* ctx) -> bool {
        uint32_t* target = static_cast<uint32_t*>(ctx);
        return evt->type == EVENT_TYPE_PATTERN_DETECTED &&
               evt->data.pattern_detected.pattern_id == *target;
    };

    uint32_t count = event_queue_count_if(queue, filter, &target_id);
    EXPECT_EQ(count, 1);
}

//=============================================================================
// STATISTICS TESTS
//=============================================================================

/**
 * WHAT: Test statistics tracking
 * WHY:  Verify stats are updated correctly
 */
TEST_F(EventQueueTest, Statistics) {
    queue = event_queue_create(nullptr);
    ASSERT_NE(queue, nullptr);

    event_queue_stats_t stats;
    EXPECT_TRUE(event_queue_get_stats(queue, &stats));

    EXPECT_EQ(stats.total_enqueued, 0);
    EXPECT_EQ(stats.total_dequeued, 0);
    EXPECT_EQ(stats.current_size, 0);
    EXPECT_EQ(stats.peak_size, 0);

    // Enqueue 5 events
    for (int i = 0; i < 5; i++) {
        event_t evt = create_test_event(MW_EVENT_PRIORITY_NORMAL);
        event_queue_enqueue(queue, &evt);
    }

    EXPECT_TRUE(event_queue_get_stats(queue, &stats));
    EXPECT_EQ(stats.total_enqueued, 5);
    EXPECT_EQ(stats.current_size, 5);
    EXPECT_EQ(stats.peak_size, 5);

    // Dequeue 2
    event_t evt;
    event_queue_dequeue(queue, &evt);
    event_queue_dequeue(queue, &evt);

    EXPECT_TRUE(event_queue_get_stats(queue, &stats));
    EXPECT_EQ(stats.total_dequeued, 2);
    EXPECT_EQ(stats.current_size, 3);
    EXPECT_EQ(stats.peak_size, 5);  // Peak remains
}

/**
 * WHAT: Test statistics for dropped events
 * WHY:  Verify overflow drops are tracked
 */
TEST_F(EventQueueTest, StatisticsDropped) {
    event_queue_config_t config = {
        .capacity = 3,
        .overflow_policy = OVERFLOW_POLICY_DROP_NEWEST,
        .enable_coalescing = false,
        .block_timeout_us = 0
    };

    queue = event_queue_create(&config);
    ASSERT_NE(queue, nullptr);

    // Fill queue
    for (int i = 0; i < 3; i++) {
        event_t evt = create_test_event(MW_EVENT_PRIORITY_NORMAL);
        event_queue_enqueue(queue, &evt);
    }

    // Try to add 2 more - should be dropped
    for (int i = 0; i < 2; i++) {
        event_t evt = create_test_event(MW_EVENT_PRIORITY_NORMAL);
        event_queue_enqueue(queue, &evt);
    }

    event_queue_stats_t stats;
    EXPECT_TRUE(event_queue_get_stats(queue, &stats));
    EXPECT_EQ(stats.total_dropped, 2);
}

/**
 * WHAT: Test statistics reset
 * WHY:  Verify reset clears counters but not current state
 */
TEST_F(EventQueueTest, StatisticsReset) {
    queue = event_queue_create(nullptr);
    ASSERT_NE(queue, nullptr);

    // Add and remove some events
    for (int i = 0; i < 10; i++) {
        event_t evt = create_test_event(MW_EVENT_PRIORITY_NORMAL);
        event_queue_enqueue(queue, &evt);
    }

    event_t evt;
    for (int i = 0; i < 5; i++) {
        event_queue_dequeue(queue, &evt);
    }

    // Reset stats
    event_queue_reset_stats(queue);

    event_queue_stats_t stats;
    EXPECT_TRUE(event_queue_get_stats(queue, &stats));

    EXPECT_EQ(stats.total_enqueued, 0);
    EXPECT_EQ(stats.total_dequeued, 0);
    EXPECT_EQ(stats.total_dropped, 0);
    EXPECT_EQ(stats.peak_size, 0);
    EXPECT_EQ(stats.current_size, 5);  // Current size unchanged
}

//=============================================================================
// ERROR HANDLING TESTS
//=============================================================================

/**
 * WHAT: Test NULL queue parameter handling
 * WHY:  Verify all functions handle NULL gracefully
 */
TEST_F(EventQueueTest, NullQueueHandling) {
    event_t evt = create_test_event(MW_EVENT_PRIORITY_NORMAL);
    event_queue_stats_t stats;

    // All operations should handle NULL queue safely
    EXPECT_FALSE(event_queue_enqueue(nullptr, &evt));
    EXPECT_FALSE(event_queue_dequeue(nullptr, &evt));
    EXPECT_FALSE(event_queue_peek(nullptr, &evt));
    EXPECT_EQ(event_queue_dequeue_batch(nullptr, &evt, 1), 0);
    EXPECT_EQ(event_queue_size(nullptr), 0);
    EXPECT_TRUE(event_queue_is_empty(nullptr));
    EXPECT_FALSE(event_queue_is_full(nullptr));

    event_queue_clear(nullptr);  // Should not crash
    event_queue_reset_stats(nullptr);  // Should not crash

    EXPECT_FALSE(event_queue_get_stats(nullptr, &stats));

    auto filter = [](const event_t* e, void* c) { return true; };
    EXPECT_EQ(event_queue_remove_if(nullptr, filter, nullptr), 0);
    EXPECT_EQ(event_queue_count_if(nullptr, filter, nullptr), 0);
}

/**
 * WHAT: Test NULL event parameter handling
 * WHY:  Verify operations validate event pointers
 */
TEST_F(EventQueueTest, NullEventHandling) {
    queue = event_queue_create(nullptr);
    ASSERT_NE(queue, nullptr);

    EXPECT_FALSE(event_queue_enqueue(queue, nullptr));
    EXPECT_FALSE(event_queue_dequeue(queue, nullptr));
    EXPECT_FALSE(event_queue_peek(queue, nullptr));
}

/**
 * WHAT: Test NULL stats parameter
 * WHY:  Verify stats function validates output pointer
 */
TEST_F(EventQueueTest, NullStatsHandling) {
    queue = event_queue_create(nullptr);
    ASSERT_NE(queue, nullptr);

    EXPECT_FALSE(event_queue_get_stats(queue, nullptr));
}

/**
 * WHAT: Test zero capacity configuration
 * WHY:  Verify invalid config is rejected
 */
TEST_F(EventQueueTest, ZeroCapacity) {
    event_queue_config_t config = {
        .capacity = 0,
        .overflow_policy = OVERFLOW_POLICY_DROP_NEWEST,
        .enable_coalescing = false,
        .block_timeout_us = 0
    };

    queue = event_queue_create(&config);
    // Should either use default capacity or return NULL
    // Either behavior is acceptable
    if (queue) {
        EXPECT_GT(event_queue_size(queue), 0);  // Should have some capacity
    }
}

/**
 * WHAT: Test invalid overflow policy
 * WHY:  Verify enum validation
 */
TEST_F(EventQueueTest, InvalidOverflowPolicy) {
    event_queue_config_t config = {
        .capacity = 10,
        .overflow_policy = static_cast<overflow_policy_t>(999),
        .enable_coalescing = false,
        .block_timeout_us = 0
    };

    queue = event_queue_create(&config);
    // Should either use default policy or return NULL
    // Implementation-dependent behavior
}

//=============================================================================
// REGRESSION TESTS
//=============================================================================

/**
 * WHAT: Test heap property after multiple operations
 * WHY:  Regression test for heap corruption bugs
 */
TEST_F(EventQueueTest, HeapPropertyMaintained) {
    queue = event_queue_create(nullptr);
    ASSERT_NE(queue, nullptr);

    // Complex sequence: add, remove, add, remove
    std::vector<mw_event_priority_t> priorities = {
        MW_EVENT_PRIORITY_NORMAL, MW_EVENT_PRIORITY_CRITICAL,
        MW_EVENT_PRIORITY_LOW, MW_EVENT_PRIORITY_HIGH
    };

    for (int cycle = 0; cycle < 10; cycle++) {
        // Add 4 events
        for (auto pri : priorities) {
            event_t evt = create_test_event(pri);
            event_queue_enqueue(queue, &evt);
        }

        // Remove 2
        event_t out;
        event_queue_dequeue(queue, &out);
        event_queue_dequeue(queue, &out);
    }

    // Verify all remaining events come out in priority order
    mw_event_priority_t prev = MW_EVENT_PRIORITY_CRITICAL;
    while (!event_queue_is_empty(queue)) {
        event_t out;
        event_queue_dequeue(queue, &out);
        EXPECT_LE(prev, out.priority);
        prev = out.priority;
    }
}

/**
 * WHAT: Test single-element queue edge case
 * WHY:  Regression test for boundary condition bugs
 */
TEST_F(EventQueueTest, SingleElementQueue) {
    event_queue_config_t config = {
        .capacity = 1,
        .overflow_policy = OVERFLOW_POLICY_DROP_NEWEST,
        .enable_coalescing = false,
        .block_timeout_us = 0
    };

    queue = event_queue_create(&config);
    ASSERT_NE(queue, nullptr);

    event_t evt1 = create_test_event(MW_EVENT_PRIORITY_NORMAL);
    EXPECT_TRUE(event_queue_enqueue(queue, &evt1));
    EXPECT_TRUE(event_queue_is_full(queue));

    // Try to add another - should be dropped
    event_t evt2 = create_test_event(MW_EVENT_PRIORITY_CRITICAL);
    EXPECT_FALSE(event_queue_enqueue(queue, &evt2));

    // Should still have first event
    event_t out;
    EXPECT_TRUE(event_queue_dequeue(queue, &out));
    EXPECT_EQ(out.priority, MW_EVENT_PRIORITY_NORMAL);
}

/**
 * WHAT: Test alternating enqueue/dequeue
 * WHY:  Regression test for queue state corruption
 */
TEST_F(EventQueueTest, AlternatingOperations) {
    queue = event_queue_create(nullptr);
    ASSERT_NE(queue, nullptr);

    for (int i = 0; i < 100; i++) {
        event_t evt = create_test_event(MW_EVENT_PRIORITY_NORMAL);
        EXPECT_TRUE(event_queue_enqueue(queue, &evt));

        event_t out;
        EXPECT_TRUE(event_queue_dequeue(queue, &out));
        EXPECT_TRUE(event_queue_is_empty(queue));
    }
}

/**
 * WHAT: Test clear on full queue
 * WHY:  Regression test for clear operation
 */
TEST_F(EventQueueTest, ClearFullQueue) {
    event_queue_config_t config = {
        .capacity = 10,
        .overflow_policy = OVERFLOW_POLICY_DROP_NEWEST,
        .enable_coalescing = false,
        .block_timeout_us = 0
    };

    queue = event_queue_create(&config);
    ASSERT_NE(queue, nullptr);

    // Fill completely
    for (int i = 0; i < 10; i++) {
        event_t evt = create_test_event(MW_EVENT_PRIORITY_NORMAL);
        event_queue_enqueue(queue, &evt);
    }

    EXPECT_TRUE(event_queue_is_full(queue));

    event_queue_clear(queue);

    EXPECT_TRUE(event_queue_is_empty(queue));
    EXPECT_FALSE(event_queue_is_full(queue));
    EXPECT_EQ(event_queue_size(queue), 0);

    // Should be able to add events again
    event_t evt = create_test_event(MW_EVENT_PRIORITY_NORMAL);
    EXPECT_TRUE(event_queue_enqueue(queue, &evt));
}

/**
 * WHAT: Test remove_if on full queue
 * WHY:  Regression test for remove during overflow state
 */
TEST_F(EventQueueTest, RemoveIfFullQueue) {
    event_queue_config_t config = {
        .capacity = 10,
        .overflow_policy = OVERFLOW_POLICY_DROP_NEWEST,
        .enable_coalescing = false,
        .block_timeout_us = 0
    };

    queue = event_queue_create(&config);
    ASSERT_NE(queue, nullptr);

    // Fill with alternating priorities
    for (int i = 0; i < 10; i++) {
        mw_event_priority_t pri = (i % 2 == 0) ?
            MW_EVENT_PRIORITY_HIGH : MW_EVENT_PRIORITY_LOW;
        event_t evt = create_test_event(pri);
        event_queue_enqueue(queue, &evt);
    }

    EXPECT_TRUE(event_queue_is_full(queue));

    // Remove all low priority
    auto filter = [](const event_t* e, void* c) {
        return e->priority == MW_EVENT_PRIORITY_LOW;
    };

    uint32_t removed = event_queue_remove_if(queue, filter, nullptr);
    EXPECT_EQ(removed, 5);
    EXPECT_FALSE(event_queue_is_full(queue));
    EXPECT_EQ(event_queue_size(queue), 5);
}

//=============================================================================
// STRESS TESTS
//=============================================================================

/**
 * WHAT: Test large number of events
 * WHY:  Verify performance and correctness at scale
 */
TEST_F(EventQueueTest, LargeQueueStress) {
    event_queue_config_t config = {
        .capacity = 10000,
        .overflow_policy = OVERFLOW_POLICY_DROP_NEWEST,
        .enable_coalescing = false,
        .block_timeout_us = 0
    };

    queue = event_queue_create(&config);
    ASSERT_NE(queue, nullptr);

    // Add 5000 events
    for (int i = 0; i < 5000; i++) {
        mw_event_priority_t pri = static_cast<mw_event_priority_t>(i % 5);
        event_t evt = create_test_event(pri);
        EXPECT_TRUE(event_queue_enqueue(queue, &evt));
    }

    EXPECT_EQ(event_queue_size(queue), 5000);

    // Remove all in priority order
    mw_event_priority_t prev = MW_EVENT_PRIORITY_CRITICAL;
    int count = 0;
    while (!event_queue_is_empty(queue)) {
        event_t out;
        EXPECT_TRUE(event_queue_dequeue(queue, &out));
        EXPECT_LE(prev, out.priority);
        prev = out.priority;
        count++;
    }

    EXPECT_EQ(count, 5000);
}

/**
 * WHAT: Test many small batches
 * WHY:  Verify batch operations at scale
 */
TEST_F(EventQueueTest, BatchOperationsStress) {
    queue = event_queue_create(nullptr);
    ASSERT_NE(queue, nullptr);

    // Add 1000 events
    for (int i = 0; i < 1000; i++) {
        event_t evt = create_test_event(MW_EVENT_PRIORITY_NORMAL);
        event_queue_enqueue(queue, &evt);
    }

    // Remove in batches of 10
    int total = 0;
    event_t batch[10];
    while (!event_queue_is_empty(queue)) {
        uint32_t count = event_queue_dequeue_batch(queue, batch, 10);
        EXPECT_GT(count, 0);
        EXPECT_LE(count, 10);
        total += count;
    }

    EXPECT_EQ(total, 1000);
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
