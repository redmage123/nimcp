/**
 * @file test_spike_event.cpp
 * @brief Comprehensive unit tests for NIMCP spike event system
 *
 * WHAT: Tests for spike trains, event queues, and GPU synchronization
 * WHY:  Ensure biological spike communication works correctly and thread-safely
 * HOW:  Use GoogleTest with edge cases, lock-free operations, and performance validation
 *
 * COVERAGE:
 * - Spike train creation/destruction
 * - Spike event creation and storage
 * - Circular buffer operations
 * - Firing rate computation
 * - Lock-free queue operations
 * - GPU memory management
 * - Thread safety
 * - Performance baselines
 *
 * @author NIMCP Development Team
 * @date 2025
 * @version 2.6 (GPU P2P)
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <cstring>
#include <algorithm>

// Headers have their own extern "C" guards
#include "gpu/nimcp_spike_event.h"
#include "utils/time/nimcp_time.h"

//=============================================================================
// Test Constants
//=============================================================================

static const uint32_t DEFAULT_TRAIN_CAPACITY = 100;
static const uint32_t DEFAULT_QUEUE_CAPACITY = 1024;
static const uint32_t LARGE_CAPACITY = 100000;
static const uint64_t TEST_TIMESTAMP = 1000000;  // 1 second in microseconds
static const float TEST_AMPLITUDE = 1.0f;
static const uint32_t TEST_SOURCE_ID = 42;
static const uint32_t TEST_TARGET_ID = 84;
static const uint32_t TEST_SYNAPSE_ID = 7;

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * WHAT: Create test spike event
 * WHY:  Generate consistent test data
 */
static spike_event_t create_test_spike(uint64_t timestamp, uint32_t source,
                                        uint32_t target, float amplitude)
{
    spike_event_t event;
    event.timestamp = timestamp;
    event.source_id = source;
    event.target_id = target;
    event.synapse_id = TEST_SYNAPSE_ID;
    event.amplitude = amplitude;
    return event;
}

/**
 * WHAT: Compare two spike events
 * WHY:  Verify event integrity
 */
static bool spikes_equal(const spike_event_t& a, const spike_event_t& b)
{
    return a.timestamp == b.timestamp &&
           a.source_id == b.source_id &&
           a.target_id == b.target_id &&
           a.synapse_id == b.synapse_id &&
           a.amplitude == b.amplitude;
}

/**
 * WHAT: Get current time in microseconds
 * WHY:  For timestamp generation
 */
static uint64_t get_current_time_us()
{
    return nimcp_time_get_us();
}

//=============================================================================
// Spike Train Tests
//=============================================================================

/**
 * WHAT: Test spike train creation with valid capacity
 * WHY:  Verify basic allocation works
 */
TEST(SpikeTrainTest, CreateValid)
{
    spike_train_t* train = spike_train_create(DEFAULT_TRAIN_CAPACITY);
    ASSERT_NE(nullptr, train) << "Should create valid spike train";
    spike_train_destroy(train);
}

/**
 * WHAT: Test spike train creation with zero capacity
 * WHY:  Verify input validation
 */
TEST(SpikeTrainTest, CreateZeroCapacity)
{
    spike_train_t* train = spike_train_create(0);
    EXPECT_EQ(nullptr, train) << "Should reject zero capacity";
}

/**
 * WHAT: Test spike train creation with excessive capacity
 * WHY:  Verify bounds checking
 */
TEST(SpikeTrainTest, CreateExcessiveCapacity)
{
    spike_train_t* train = spike_train_create(10000000);
    EXPECT_EQ(nullptr, train) << "Should reject excessive capacity (>1M)";
}

/**
 * WHAT: Test spike train destruction with NULL
 * WHY:  Verify NULL-safety
 */
TEST(SpikeTrainTest, DestroyNull)
{
    spike_train_destroy(nullptr);
    // Should not crash
    SUCCEED();
}

/**
 * WHAT: Test adding single spike to train
 * WHY:  Verify basic spike storage
 */
TEST(SpikeTrainTest, AddSingleSpike)
{
    spike_train_t* train = spike_train_create(DEFAULT_TRAIN_CAPACITY);
    ASSERT_NE(nullptr, train);

    bool result = spike_train_add(train, TEST_TIMESTAMP, TEST_AMPLITUDE);
    EXPECT_TRUE(result) << "Should add spike successfully";

    uint64_t last_spike = spike_train_get_last_spike(train);
    EXPECT_EQ(TEST_TIMESTAMP, last_spike) << "Should record spike timestamp";

    spike_train_destroy(train);
}

/**
 * WHAT: Test adding spike with NULL train
 * WHY:  Verify NULL-safety
 */
TEST(SpikeTrainTest, AddSpikeNullTrain)
{
    bool result = spike_train_add(nullptr, TEST_TIMESTAMP, TEST_AMPLITUDE);
    EXPECT_FALSE(result) << "Should reject NULL train";
}

/**
 * WHAT: Test adding spike with zero timestamp
 * WHY:  Verify timestamp validation
 * FIX:  Timestamp 0 is valid (t=0) - Issue #SPIKE-003
 */
TEST(SpikeTrainTest, AddSpikeZeroTimestamp)
{
    spike_train_t* train = spike_train_create(DEFAULT_TRAIN_CAPACITY);
    ASSERT_NE(nullptr, train);

    // Timestamp 0 is valid (simulations often start at t=0)
    bool result = spike_train_add(train, 0, TEST_AMPLITUDE);
    EXPECT_TRUE(result) << "Timestamp 0 should be accepted";

    // Verify spike was added
    spike_event_t event;
    EXPECT_TRUE(spike_train_get_spike(train, 0, &event));
    EXPECT_EQ(0u, event.timestamp);

    spike_train_destroy(train);
}

/**
 * WHAT: Test adding multiple spikes
 * WHY:  Verify sequential spike storage
 */
TEST(SpikeTrainTest, AddMultipleSpikes)
{
    spike_train_t* train = spike_train_create(DEFAULT_TRAIN_CAPACITY);
    ASSERT_NE(nullptr, train);

    const int num_spikes = 10;
    for (int i = 0; i < num_spikes; i++) {
        uint64_t timestamp = TEST_TIMESTAMP + i * 1000;
        bool result = spike_train_add(train, timestamp, TEST_AMPLITUDE);
        EXPECT_TRUE(result) << "Spike " << i << " should be added";
    }

    uint64_t last_spike = spike_train_get_last_spike(train);
    EXPECT_EQ(TEST_TIMESTAMP + (num_spikes - 1) * 1000, last_spike);

    spike_train_destroy(train);
}

/**
 * WHAT: Test circular buffer overflow
 * WHY:  Verify oldest spikes are overwritten
 */
TEST(SpikeTrainTest, CircularBufferOverflow)
{
    const uint32_t capacity = 10;
    spike_train_t* train = spike_train_create(capacity);
    ASSERT_NE(nullptr, train);

    // Add more spikes than capacity
    for (uint32_t i = 0; i < capacity + 5; i++) {
        spike_train_add(train, TEST_TIMESTAMP + i, TEST_AMPLITUDE);
    }

    // Should wrap around, keeping latest spikes
    uint64_t last_spike = spike_train_get_last_spike(train);
    EXPECT_EQ(TEST_TIMESTAMP + capacity + 4, last_spike);

    spike_train_destroy(train);
}

/**
 * WHAT: Test retrieving spike by index
 * WHY:  Verify random access to spike history
 */
TEST(SpikeTrainTest, GetSpikeByIndex)
{
    spike_train_t* train = spike_train_create(DEFAULT_TRAIN_CAPACITY);
    ASSERT_NE(nullptr, train);

    // Add test spikes
    const int num_spikes = 5;
    for (int i = 0; i < num_spikes; i++) {
        spike_train_add(train, TEST_TIMESTAMP + i * 1000, TEST_AMPLITUDE + i);
    }

    // Retrieve specific spike
    spike_event_t event;
    memset(&event, 0, sizeof(event));
    bool result = spike_train_get_spike(train, 2, &event);
    EXPECT_TRUE(result) << "Should retrieve spike at index 2";
    EXPECT_EQ(TEST_TIMESTAMP + 2000, event.timestamp);
    EXPECT_FLOAT_EQ(TEST_AMPLITUDE + 2, event.amplitude);

    spike_train_destroy(train);
}

/**
 * WHAT: Test retrieving spike with NULL train
 * WHY:  Verify NULL-safety
 */
TEST(SpikeTrainTest, GetSpikeNullTrain)
{
    spike_event_t event;
    memset(&event, 0, sizeof(event));
    bool result = spike_train_get_spike(nullptr, 0, &event);
    EXPECT_FALSE(result) << "Should reject NULL train";
}

/**
 * WHAT: Test retrieving spike with NULL event
 * WHY:  Verify NULL-safety
 */
TEST(SpikeTrainTest, GetSpikeNullEvent)
{
    spike_train_t* train = spike_train_create(DEFAULT_TRAIN_CAPACITY);
    ASSERT_NE(nullptr, train);

    spike_train_add(train, TEST_TIMESTAMP, TEST_AMPLITUDE);
    bool result = spike_train_get_spike(train, 0, nullptr);
    EXPECT_FALSE(result) << "Should reject NULL event pointer";

    spike_train_destroy(train);
}

/**
 * WHAT: Test retrieving spike with out-of-bounds index
 * WHY:  Verify bounds checking
 */
TEST(SpikeTrainTest, GetSpikeOutOfBounds)
{
    spike_train_t* train = spike_train_create(DEFAULT_TRAIN_CAPACITY);
    ASSERT_NE(nullptr, train);

    spike_train_add(train, TEST_TIMESTAMP, TEST_AMPLITUDE);

    spike_event_t event;
    memset(&event, 0, sizeof(event));
    bool result = spike_train_get_spike(train, 100, &event);
    EXPECT_FALSE(result) << "Should reject out-of-bounds index";

    spike_train_destroy(train);
}

/**
 * WHAT: Test firing rate computation with empty train
 * WHY:  Verify edge case handling
 */
TEST(SpikeTrainTest, ComputeRateEmpty)
{
    spike_train_t* train = spike_train_create(DEFAULT_TRAIN_CAPACITY);
    ASSERT_NE(nullptr, train);

    float rate = spike_train_compute_rate(train, 1000000);  // 1 second window
    EXPECT_FLOAT_EQ(0.0f, rate) << "Empty train should have zero firing rate";

    spike_train_destroy(train);
}

/**
 * WHAT: Test firing rate computation with NULL train
 * WHY:  Verify NULL-safety
 */
TEST(SpikeTrainTest, ComputeRateNullTrain)
{
    float rate = spike_train_compute_rate(nullptr, 1000000);
    EXPECT_FLOAT_EQ(0.0f, rate) << "NULL train should return zero rate";
}

/**
 * WHAT: Test firing rate computation with zero window
 * WHY:  Verify window validation
 */
TEST(SpikeTrainTest, ComputeRateZeroWindow)
{
    spike_train_t* train = spike_train_create(DEFAULT_TRAIN_CAPACITY);
    ASSERT_NE(nullptr, train);

    spike_train_add(train, get_current_time_us(), TEST_AMPLITUDE);
    float rate = spike_train_compute_rate(train, 0);
    EXPECT_FLOAT_EQ(0.0f, rate) << "Zero window should return zero rate";

    spike_train_destroy(train);
}

/**
 * WHAT: Test clearing spike train
 * WHY:  Verify reset functionality
 */
TEST(SpikeTrainTest, ClearTrain)
{
    spike_train_t* train = spike_train_create(DEFAULT_TRAIN_CAPACITY);
    ASSERT_NE(nullptr, train);

    // Add spikes
    for (int i = 0; i < 5; i++) {
        spike_train_add(train, TEST_TIMESTAMP + i, TEST_AMPLITUDE);
    }

    spike_train_clear(train);

    uint64_t last_spike = spike_train_get_last_spike(train);
    EXPECT_EQ(0ULL, last_spike) << "Cleared train should have no last spike";

    spike_train_destroy(train);
}

/**
 * WHAT: Test clearing NULL train
 * WHY:  Verify NULL-safety
 */
TEST(SpikeTrainTest, ClearNullTrain)
{
    spike_train_clear(nullptr);
    // Should not crash
    SUCCEED();
}

/**
 * WHAT: Test getting last spike from NULL train
 * WHY:  Verify NULL-safety
 */
TEST(SpikeTrainTest, GetLastSpikeNull)
{
    uint64_t last_spike = spike_train_get_last_spike(nullptr);
    EXPECT_EQ(0ULL, last_spike) << "NULL train should return zero timestamp";
}

//=============================================================================
// Spike Queue Tests
//=============================================================================

/**
 * WHAT: Test spike queue creation with valid capacity
 * WHY:  Verify basic allocation
 */
TEST(SpikeQueueTest, CreateValid)
{
    spike_queue_t* queue = spike_queue_create(DEFAULT_QUEUE_CAPACITY, false);
    ASSERT_NE(nullptr, queue) << "Should create valid spike queue";
    spike_queue_destroy(queue);
}

/**
 * WHAT: Test spike queue creation with GPU enabled
 * WHY:  Verify GPU flag is handled
 */
TEST(SpikeQueueTest, CreateGPUEnabled)
{
    spike_queue_t* queue = spike_queue_create(DEFAULT_QUEUE_CAPACITY, true);
    ASSERT_NE(nullptr, queue) << "Should create GPU-enabled queue";
    // Note: GPU memory may not actually be allocated without CUDA
    spike_queue_destroy(queue);
}

/**
 * WHAT: Test spike queue creation with zero capacity
 * WHY:  Verify input validation
 */
TEST(SpikeQueueTest, CreateZeroCapacity)
{
    spike_queue_t* queue = spike_queue_create(0, false);
    EXPECT_EQ(nullptr, queue) << "Should reject zero capacity";
}

/**
 * WHAT: Test spike queue creation with excessive capacity
 * WHY:  Verify bounds checking
 */
TEST(SpikeQueueTest, CreateExcessiveCapacity)
{
    spike_queue_t* queue = spike_queue_create(20000000, false);
    EXPECT_EQ(nullptr, queue) << "Should reject excessive capacity (>10M)";
}

/**
 * WHAT: Test spike queue destruction with NULL
 * WHY:  Verify NULL-safety
 */
TEST(SpikeQueueTest, DestroyNull)
{
    spike_queue_destroy(nullptr);
    // Should not crash
    SUCCEED();
}

/**
 * WHAT: Test capacity rounding to power of 2
 * WHY:  Verify fast modulo optimization
 */
TEST(SpikeQueueTest, CapacityPowerOf2)
{
    // Create queue with non-power-of-2 capacity
    spike_queue_t* queue = spike_queue_create(100, false);
    ASSERT_NE(nullptr, queue);

    // Capacity should be rounded up to 128 (next power of 2)
    // We can verify this by filling the queue
    spike_event_t event = create_test_spike(TEST_TIMESTAMP, TEST_SOURCE_ID,
                                            TEST_TARGET_ID, TEST_AMPLITUDE);

    int count = 0;
    while (spike_queue_push(queue, &event)) {
        count++;
        if (count > 200) break;  // Safety limit
    }

    EXPECT_EQ(128, count) << "Capacity should be rounded to power of 2";

    spike_queue_destroy(queue);
}

/**
 * WHAT: Test pushing single event to queue
 * WHY:  Verify basic enqueue operation
 */
TEST(SpikeQueueTest, PushSingleEvent)
{
    spike_queue_t* queue = spike_queue_create(DEFAULT_QUEUE_CAPACITY, false);
    ASSERT_NE(nullptr, queue);

    spike_event_t event = create_test_spike(TEST_TIMESTAMP, TEST_SOURCE_ID,
                                            TEST_TARGET_ID, TEST_AMPLITUDE);
    bool result = spike_queue_push(queue, &event);
    EXPECT_TRUE(result) << "Should push event successfully";

    uint32_t size = spike_queue_size(queue);
    EXPECT_EQ(1U, size) << "Queue should contain one event";

    spike_queue_destroy(queue);
}

/**
 * WHAT: Test pushing event with NULL queue
 * WHY:  Verify NULL-safety
 */
TEST(SpikeQueueTest, PushNullQueue)
{
    spike_event_t event = create_test_spike(TEST_TIMESTAMP, TEST_SOURCE_ID,
                                            TEST_TARGET_ID, TEST_AMPLITUDE);
    bool result = spike_queue_push(nullptr, &event);
    EXPECT_FALSE(result) << "Should reject NULL queue";
}

/**
 * WHAT: Test pushing NULL event
 * WHY:  Verify NULL-safety
 */
TEST(SpikeQueueTest, PushNullEvent)
{
    spike_queue_t* queue = spike_queue_create(DEFAULT_QUEUE_CAPACITY, false);
    ASSERT_NE(nullptr, queue);

    bool result = spike_queue_push(queue, nullptr);
    EXPECT_FALSE(result) << "Should reject NULL event";

    spike_queue_destroy(queue);
}

/**
 * WHAT: Test popping event from queue
 * WHY:  Verify basic dequeue operation
 */
TEST(SpikeQueueTest, PopSingleEvent)
{
    spike_queue_t* queue = spike_queue_create(DEFAULT_QUEUE_CAPACITY, false);
    ASSERT_NE(nullptr, queue);

    spike_event_t push_event = create_test_spike(TEST_TIMESTAMP, TEST_SOURCE_ID,
                                                  TEST_TARGET_ID, TEST_AMPLITUDE);
    spike_queue_push(queue, &push_event);

    spike_event_t pop_event;
    bool result = spike_queue_pop(queue, &pop_event);
    EXPECT_TRUE(result) << "Should pop event successfully";
    EXPECT_TRUE(spikes_equal(push_event, pop_event)) << "Popped event should match pushed event";

    spike_queue_destroy(queue);
}

/**
 * WHAT: Test popping from empty queue
 * WHY:  Verify empty queue handling
 */
TEST(SpikeQueueTest, PopEmptyQueue)
{
    spike_queue_t* queue = spike_queue_create(DEFAULT_QUEUE_CAPACITY, false);
    ASSERT_NE(nullptr, queue);

    spike_event_t event;
    memset(&event, 0, sizeof(event));
    bool result = spike_queue_pop(queue, &event);
    EXPECT_FALSE(result) << "Should fail to pop from empty queue";

    spike_queue_destroy(queue);
}

/**
 * WHAT: Test popping with NULL queue
 * WHY:  Verify NULL-safety
 */
TEST(SpikeQueueTest, PopNullQueue)
{
    spike_event_t event;
    memset(&event, 0, sizeof(event));
    bool result = spike_queue_pop(nullptr, &event);
    EXPECT_FALSE(result) << "Should reject NULL queue";
}

/**
 * WHAT: Test popping with NULL event
 * WHY:  Verify NULL-safety
 */
TEST(SpikeQueueTest, PopNullEvent)
{
    spike_queue_t* queue = spike_queue_create(DEFAULT_QUEUE_CAPACITY, false);
    ASSERT_NE(nullptr, queue);

    spike_event_t push_event = create_test_spike(TEST_TIMESTAMP, TEST_SOURCE_ID,
                                                  TEST_TARGET_ID, TEST_AMPLITUDE);
    spike_queue_push(queue, &push_event);

    bool result = spike_queue_pop(queue, nullptr);
    EXPECT_FALSE(result) << "Should reject NULL event pointer";

    spike_queue_destroy(queue);
}

/**
 * WHAT: Test FIFO ordering
 * WHY:  Verify queue maintains order
 */
TEST(SpikeQueueTest, FIFOOrdering)
{
    spike_queue_t* queue = spike_queue_create(DEFAULT_QUEUE_CAPACITY, false);
    ASSERT_NE(nullptr, queue);

    const int num_events = 10;
    std::vector<spike_event_t> pushed_events;

    // Push events with different timestamps
    for (int i = 0; i < num_events; i++) {
        spike_event_t event = create_test_spike(TEST_TIMESTAMP + i, TEST_SOURCE_ID,
                                                TEST_TARGET_ID, TEST_AMPLITUDE);
        pushed_events.push_back(event);
        spike_queue_push(queue, &event);
    }

    // Pop events and verify order
    for (int i = 0; i < num_events; i++) {
        spike_event_t event;
        memset(&event, 0, sizeof(event));
        bool result = spike_queue_pop(queue, &event);
        EXPECT_TRUE(result) << "Event " << i << " should pop successfully";
        EXPECT_TRUE(spikes_equal(pushed_events[i], event)) << "Event order should be preserved";
    }

    spike_queue_destroy(queue);
}

/**
 * WHAT: Test queue full condition
 * WHY:  Verify capacity limits
 */
TEST(SpikeQueueTest, QueueFull)
{
    const uint32_t capacity = 16;  // Small capacity for testing
    spike_queue_t* queue = spike_queue_create(capacity, false);
    ASSERT_NE(nullptr, queue);

    spike_event_t event = create_test_spike(TEST_TIMESTAMP, TEST_SOURCE_ID,
                                            TEST_TARGET_ID, TEST_AMPLITUDE);

    // Fill queue to capacity
    for (uint32_t i = 0; i < capacity; i++) {
        bool result = spike_queue_push(queue, &event);
        EXPECT_TRUE(result) << "Event " << i << " should push successfully";
    }

    // Next push should fail
    bool result = spike_queue_push(queue, &event);
    EXPECT_FALSE(result) << "Should reject push when queue is full";

    spike_queue_destroy(queue);
}

/**
 * WHAT: Test queue size tracking
 * WHY:  Verify accurate count maintenance
 */
TEST(SpikeQueueTest, QueueSizeTracking)
{
    spike_queue_t* queue = spike_queue_create(DEFAULT_QUEUE_CAPACITY, false);
    ASSERT_NE(nullptr, queue);

    EXPECT_EQ(0U, spike_queue_size(queue)) << "New queue should be empty";
    EXPECT_TRUE(spike_queue_is_empty(queue)) << "New queue should report as empty";

    spike_event_t event = create_test_spike(TEST_TIMESTAMP, TEST_SOURCE_ID,
                                            TEST_TARGET_ID, TEST_AMPLITUDE);

    // Push events and check size
    const int num_events = 5;
    for (int i = 0; i < num_events; i++) {
        spike_queue_push(queue, &event);
        EXPECT_EQ(i + 1, spike_queue_size(queue)) << "Size should increment";
    }

    EXPECT_FALSE(spike_queue_is_empty(queue)) << "Queue should not be empty";

    // Pop events and check size
    for (int i = 0; i < num_events; i++) {
        spike_event_t pop_event;
        memset(&pop_event, 0, sizeof(pop_event));
        spike_queue_pop(queue, &pop_event);
        EXPECT_EQ(num_events - i - 1, spike_queue_size(queue)) << "Size should decrement";
    }

    EXPECT_TRUE(spike_queue_is_empty(queue)) << "Empty queue should report as empty";

    spike_queue_destroy(queue);
}

/**
 * WHAT: Test queue size with NULL queue
 * WHY:  Verify NULL-safety
 */
TEST(SpikeQueueTest, QueueSizeNull)
{
    uint32_t size = spike_queue_size(nullptr);
    EXPECT_EQ(0U, size) << "NULL queue should return size 0";
}

/**
 * WHAT: Test queue is_empty with NULL queue
 * WHY:  Verify NULL-safety
 */
TEST(SpikeQueueTest, QueueIsEmptyNull)
{
    bool empty = spike_queue_is_empty(nullptr);
    EXPECT_TRUE(empty) << "NULL queue should report as empty";
}

/**
 * WHAT: Test GPU sync with NULL queue
 * WHY:  Verify NULL-safety
 */
TEST(SpikeQueueTest, GPUSyncNullQueue)
{
    bool result = spike_queue_sync_gpu(nullptr, true);
    EXPECT_FALSE(result) << "Should reject NULL queue";
}

/**
 * WHAT: Test GPU sync without GPU enabled
 * WHY:  Verify GPU-disabled handling
 */
TEST(SpikeQueueTest, GPUSyncDisabled)
{
    spike_queue_t* queue = spike_queue_create(DEFAULT_QUEUE_CAPACITY, false);
    ASSERT_NE(nullptr, queue);

    bool result = spike_queue_sync_gpu(queue, true);
    EXPECT_FALSE(result) << "Should fail sync when GPU not enabled";

    spike_queue_destroy(queue);
}

/**
 * WHAT: Test GPU sync CPU to GPU direction
 * WHY:  Verify direction parameter
 */
TEST(SpikeQueueTest, GPUSyncCPUtoGPU)
{
    spike_queue_t* queue = spike_queue_create(DEFAULT_QUEUE_CAPACITY, true);
    ASSERT_NE(nullptr, queue);

    // Will fail without CUDA, but tests the code path
    spike_queue_sync_gpu(queue, true);

    spike_queue_destroy(queue);
}

/**
 * WHAT: Test GPU sync GPU to CPU direction
 * WHY:  Verify direction parameter
 */
TEST(SpikeQueueTest, GPUSyncGPUtoCPU)
{
    spike_queue_t* queue = spike_queue_create(DEFAULT_QUEUE_CAPACITY, true);
    ASSERT_NE(nullptr, queue);

    // Will fail without CUDA, but tests the code path
    spike_queue_sync_gpu(queue, false);

    spike_queue_destroy(queue);
}

//=============================================================================
// Thread Safety Tests
//=============================================================================

/**
 * WHAT: Test concurrent push operations
 * WHY:  Verify thread-safe enqueue
 */
TEST(SpikeQueueThreadTest, ConcurrentPush)
{
    spike_queue_t* queue = spike_queue_create(LARGE_CAPACITY, false);
    ASSERT_NE(nullptr, queue);

    const int num_threads = 4;
    const int events_per_thread = 1000;
    std::atomic<int> push_count{0};

    auto push_worker = [&](int thread_id) {
        for (int i = 0; i < events_per_thread; i++) {
            spike_event_t event = create_test_spike(TEST_TIMESTAMP + i,
                                                    thread_id, TEST_TARGET_ID,
                                                    TEST_AMPLITUDE);
            if (spike_queue_push(queue, &event)) {
                push_count.fetch_add(1, std::memory_order_relaxed);
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back(push_worker, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(num_threads * events_per_thread, push_count.load())
        << "All pushes should succeed";
    EXPECT_EQ(num_threads * events_per_thread, spike_queue_size(queue))
        << "Queue size should match push count";

    spike_queue_destroy(queue);
}

/**
 * WHAT: Test concurrent pop operations
 * WHY:  Verify thread-safe dequeue
 */
TEST(SpikeQueueThreadTest, ConcurrentPop)
{
    spike_queue_t* queue = spike_queue_create(LARGE_CAPACITY, false);
    ASSERT_NE(nullptr, queue);

    const int num_events = 4000;

    // Pre-fill queue
    for (int i = 0; i < num_events; i++) {
        spike_event_t event = create_test_spike(TEST_TIMESTAMP + i, TEST_SOURCE_ID,
                                                TEST_TARGET_ID, TEST_AMPLITUDE);
        spike_queue_push(queue, &event);
    }

    const int num_threads = 4;
    std::atomic<int> pop_count{0};

    auto pop_worker = [&]() {
        spike_event_t event;
        memset(&event, 0, sizeof(event));
        while (spike_queue_pop(queue, &event)) {
            pop_count.fetch_add(1, std::memory_order_relaxed);
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back(pop_worker);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(num_events, pop_count.load()) << "All events should be popped";
    EXPECT_TRUE(spike_queue_is_empty(queue)) << "Queue should be empty";

    spike_queue_destroy(queue);
}

/**
 * WHAT: Test concurrent push and pop operations
 * WHY:  Verify full producer-consumer pattern
 */
TEST(SpikeQueueThreadTest, ConcurrentPushPop)
{
    spike_queue_t* queue = spike_queue_create(LARGE_CAPACITY, false);
    ASSERT_NE(nullptr, queue);

    const int num_producers = 2;
    const int num_consumers = 2;
    const int events_per_producer = 5000;

    std::atomic<int> push_count{0};
    std::atomic<int> pop_count{0};
    std::atomic<bool> producers_done{false};

    auto producer = [&](int id) {
        for (int i = 0; i < events_per_producer; i++) {
            spike_event_t event = create_test_spike(TEST_TIMESTAMP + i, id,
                                                    TEST_TARGET_ID, TEST_AMPLITUDE);
            while (!spike_queue_push(queue, &event)) {
                std::this_thread::yield();
            }
            push_count.fetch_add(1, std::memory_order_relaxed);
        }
    };

    auto consumer = [&]() {
        spike_event_t event;
        memset(&event, 0, sizeof(event));
        while (!producers_done.load(std::memory_order_acquire) ||
               !spike_queue_is_empty(queue)) {
            if (spike_queue_pop(queue, &event)) {
                pop_count.fetch_add(1, std::memory_order_relaxed);
            } else {
                std::this_thread::yield();
            }
        }
    };

    std::vector<std::thread> threads;

    // Start producers
    for (int i = 0; i < num_producers; i++) {
        threads.emplace_back(producer, i);
    }

    // Start consumers
    for (int i = 0; i < num_consumers; i++) {
        threads.emplace_back(consumer);
    }

    // Wait for producers
    for (int i = 0; i < num_producers; i++) {
        threads[i].join();
    }
    producers_done.store(true, std::memory_order_release);

    // Wait for consumers
    for (int i = num_producers; i < num_producers + num_consumers; i++) {
        threads[i].join();
    }

    EXPECT_EQ(num_producers * events_per_producer, push_count.load())
        << "All events should be pushed";
    EXPECT_EQ(push_count.load(), pop_count.load())
        << "All pushed events should be popped";

    spike_queue_destroy(queue);
}

//=============================================================================
// Performance Tests
//=============================================================================

/**
 * WHAT: Test spike train add performance
 * WHY:  Establish baseline for spike recording
 */
TEST(SpikePerformanceTest, TrainAddPerformance)
{
    spike_train_t* train = spike_train_create(LARGE_CAPACITY);
    ASSERT_NE(nullptr, train);

    const int num_spikes = 10000;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_spikes; i++) {
        spike_train_add(train, TEST_TIMESTAMP + i, TEST_AMPLITUDE);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double ops_per_sec = (num_spikes * 1000000.0) / duration.count();
    EXPECT_GT(ops_per_sec, 100000) << "Should add >100K spikes/sec";

    std::cout << "Spike train add: " << ops_per_sec << " ops/sec" << std::endl;

    spike_train_destroy(train);
}

/**
 * WHAT: Test queue push/pop performance
 * WHY:  Establish baseline for message passing
 */
TEST(SpikePerformanceTest, QueuePushPopPerformance)
{
    spike_queue_t* queue = spike_queue_create(LARGE_CAPACITY, false);
    ASSERT_NE(nullptr, queue);

    const int num_events = 100000;
    spike_event_t event = create_test_spike(TEST_TIMESTAMP, TEST_SOURCE_ID,
                                            TEST_TARGET_ID, TEST_AMPLITUDE);

    auto start = std::chrono::high_resolution_clock::now();

    // Push events
    for (int i = 0; i < num_events; i++) {
        spike_queue_push(queue, &event);
    }

    // Pop events
    for (int i = 0; i < num_events; i++) {
        spike_event_t pop_event;
        memset(&pop_event, 0, sizeof(pop_event));
        spike_queue_pop(queue, &pop_event);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double ops_per_sec = (2 * num_events * 1000000.0) / duration.count();
    EXPECT_GT(ops_per_sec, 1000000) << "Should process >1M queue ops/sec";

    std::cout << "Queue push/pop: " << ops_per_sec << " ops/sec" << std::endl;

    spike_queue_destroy(queue);
}

/**
 * WHAT: Test large spike train capacity
 * WHY:  Verify scalability
 */
TEST(SpikePerformanceTest, LargeCapacityTrain)
{
    spike_train_t* train = spike_train_create(LARGE_CAPACITY);
    ASSERT_NE(nullptr, train);

    // Fill to capacity
    for (uint32_t i = 0; i < LARGE_CAPACITY; i++) {
        spike_train_add(train, TEST_TIMESTAMP + i, TEST_AMPLITUDE);
    }

    // Verify we can retrieve spikes
    spike_event_t event;
    memset(&event, 0, sizeof(event));
    bool result = spike_train_get_spike(train, LARGE_CAPACITY / 2, &event);
    EXPECT_TRUE(result) << "Should retrieve spike from large train";

    spike_train_destroy(train);
}

/**
 * WHAT: Test large queue capacity
 * WHY:  Verify scalability
 */
TEST(SpikePerformanceTest, LargeCapacityQueue)
{
    spike_queue_t* queue = spike_queue_create(LARGE_CAPACITY, false);
    ASSERT_NE(nullptr, queue);

    spike_event_t event = create_test_spike(TEST_TIMESTAMP, TEST_SOURCE_ID,
                                            TEST_TARGET_ID, TEST_AMPLITUDE);

    // Fill to near capacity
    for (uint32_t i = 0; i < LARGE_CAPACITY - 1; i++) {
        bool result = spike_queue_push(queue, &event);
        ASSERT_TRUE(result) << "Push should succeed for event " << i;
    }

    EXPECT_EQ(LARGE_CAPACITY - 1, spike_queue_size(queue))
        << "Large queue should track size correctly";

    spike_queue_destroy(queue);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
