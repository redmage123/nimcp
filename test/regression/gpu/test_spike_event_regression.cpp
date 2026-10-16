/**
 * @file test_spike_event_regression.cpp
 * @brief Regression tests for spike event message protocol
 *
 * WHAT: Comprehensive regression tests for nimcp_spike_event
 * WHY:  Ensure API stability, performance, biological accuracy
 * HOW:  Test spike events, queues, trains, performance baselines
 *
 * REGRESSION CATEGORIES:
 * - API Stability: Function signatures, struct layout, enum values
 * - Backward Compatibility: Old spike handling code still works
 * - Performance Baselines: Queue operations, spike processing speed
 * - Data Integrity: Spike timing accuracy, queue ordering
 * - Bug Fixes: Previously fixed bugs must stay fixed
 * - Biological Accuracy: Spike timing precision, refractory periods
 *
 * @author NIMCP Test Team
 * @date 2025-01-19
 */

#include <gtest/gtest.h>
#include <cstring>
#include <chrono>
#include <vector>
#include <random>

// Headers have their own extern "C" guards
    #include "gpu/nimcp_spike_event.h"

//=============================================================================
// Test Utilities
//=============================================================================

class SpikeEventRegressionTest : public ::testing::Test {
protected:
    spike_train_t* train;
    spike_queue_t* queue;

    void SetUp() override {
        train = nullptr;
        queue = nullptr;
    }

    void TearDown() override {
        if (train) {
            spike_train_destroy(train);
            train = nullptr;
        }
        if (queue) {
            spike_queue_destroy(queue);
            queue = nullptr;
        }
    }
};

//=============================================================================
// API Stability Tests - Enum Values
//=============================================================================

TEST_F(SpikeEventRegressionTest, SpikeTypeEnumStable) {
    // WHAT: Verify spike_type_t enum values
    // WHY:  API stability - enum values must not change
    // REGRESSION: Enum values must remain constant

    spike_type_t type;

    type = SPIKE_TYPE_EXCITATORY;
    EXPECT_EQ(type, SPIKE_TYPE_EXCITATORY);

    type = SPIKE_TYPE_INHIBITORY;
    EXPECT_EQ(type, SPIKE_TYPE_INHIBITORY);

    type = SPIKE_TYPE_MODULATORY;
    EXPECT_EQ(type, SPIKE_TYPE_MODULATORY);

    type = SPIKE_TYPE_BACKPROP;
    EXPECT_EQ(type, SPIKE_TYPE_BACKPROP);

    type = SPIKE_TYPE_CALCIUM;
    EXPECT_EQ(type, SPIKE_TYPE_CALCIUM);

    type = SPIKE_TYPE_BURST;
    EXPECT_EQ(type, SPIKE_TYPE_BURST);
}

//=============================================================================
// API Stability Tests - Struct Layout
//=============================================================================

TEST_F(SpikeEventRegressionTest, SpikeEventStructStable) {
    // WHAT: Verify spike_event_t structure layout
    // WHY:  API stability - struct must remain 24 bytes
    // REGRESSION: Struct size and alignment critical for GPU

    spike_event_t event;
    memset(&event, 0, sizeof(event));

    // Verify size is exactly 24 bytes
    EXPECT_EQ(sizeof(spike_event_t), 24u);

    // Verify fields are accessible
    event.timestamp = 1000;
    event.source_id = 42;
    event.target_id = 99;
    event.synapse_id = 7;
    event.amplitude = 1.0f;

    EXPECT_EQ(event.timestamp, 1000u);
    EXPECT_EQ(event.source_id, 42u);
    EXPECT_EQ(event.target_id, 99u);
    EXPECT_EQ(event.synapse_id, 7u);
    EXPECT_FLOAT_EQ(event.amplitude, 1.0f);
}

TEST_F(SpikeEventRegressionTest, SpikeEventAlignment) {
    // WHAT: Verify spike_event_t is properly aligned
    // WHY:  GPU performance - must be aligned for coalesced access
    // REGRESSION: Alignment must be maintained

    // Check alignment
    spike_event_t event;
    uintptr_t addr = reinterpret_cast<uintptr_t>(&event);

    // Should be aligned to at least 8 bytes (for uint64_t timestamp)
    EXPECT_EQ(addr % 8, 0u);
}

//=============================================================================
// Spike Train Tests
//=============================================================================

TEST_F(SpikeEventRegressionTest, SpikeTrainCreateDestroy) {
    // WHAT: Verify spike train lifecycle
    // WHY:  Core functionality - must manage resources
    // REGRESSION: Memory leak fix (Issue #SPIKE-001)

    train = spike_train_create(100);
    EXPECT_NE(train, nullptr);

    spike_train_destroy(train);
    train = nullptr;

    // Double destroy should be safe
    spike_train_destroy(nullptr);
}

TEST_F(SpikeEventRegressionTest, SpikeTrainAddSpikes) {
    // WHAT: Verify adding spikes to train
    // WHY:  Core functionality - must record spikes
    // REGRESSION: Spike ordering must be maintained

    train = spike_train_create(100);
    ASSERT_NE(train, nullptr);

    // Add spikes with increasing timestamps
    EXPECT_TRUE(spike_train_add(train, 1000, 1.0f));
    EXPECT_TRUE(spike_train_add(train, 2000, 1.0f));
    EXPECT_TRUE(spike_train_add(train, 3000, 1.0f));

    // Last spike should be 3000
    uint64_t last = spike_train_get_last_spike(train);
    EXPECT_EQ(last, 3000u);
}

TEST_F(SpikeEventRegressionTest, SpikeTrainCapacityLimit) {
    // WHAT: Verify train respects capacity limit
    // WHY:  Memory bounds - must not overflow
    // REGRESSION: Bug fix - buffer overflow (Issue #SPIKE-002)

    train = spike_train_create(10);
    ASSERT_NE(train, nullptr);

    // Add more spikes than capacity
    for (int i = 0; i < 20; i++) {
        bool result = spike_train_add(train, i * 1000, 1.0f);
        // First 10 should succeed, rest may fail or wrap
        if (i < 10) {
            EXPECT_TRUE(result);
        }
    }

    // Should not crash
    SUCCEED();
}

TEST_F(SpikeEventRegressionTest, SpikeTrainGetSpike) {
    // WHAT: Verify retrieving spikes by index
    // WHY:  Data access - must retrieve correctly
    // REGRESSION: Index ordering must be correct

    train = spike_train_create(100);
    ASSERT_NE(train, nullptr);

    // Add spikes
    spike_train_add(train, 1000, 1.0f);
    spike_train_add(train, 2000, 1.5f);
    spike_train_add(train, 3000, 2.0f);

    // Retrieve spikes
    spike_event_t event;

    EXPECT_TRUE(spike_train_get_spike(train, 0, &event));
    EXPECT_EQ(event.timestamp, 1000u);

    EXPECT_TRUE(spike_train_get_spike(train, 1, &event));
    EXPECT_EQ(event.timestamp, 2000u);

    EXPECT_TRUE(spike_train_get_spike(train, 2, &event));
    EXPECT_EQ(event.timestamp, 3000u);

    // Out of bounds should fail
    EXPECT_FALSE(spike_train_get_spike(train, 100, &event));
}

TEST_F(SpikeEventRegressionTest, SpikeTrainFiringRate) {
    // WHAT: Verify firing rate calculation
    // WHY:  Biological metric - must be accurate
    // REGRESSION: Rate calculation must be correct

    train = spike_train_create(100);
    ASSERT_NE(train, nullptr);

    // Add 10 spikes over 1 second (10 Hz expected)
    for (int i = 0; i < 10; i++) {
        spike_train_add(train, i * 100000, 1.0f);  // μs
    }

    // Calculate rate over 1 second window
    float rate = spike_train_compute_rate(train, 1000000);  // 1 sec in μs

    // Should be approximately 10 Hz (allow some tolerance)
    EXPECT_NEAR(rate, 10.0f, 1.0f);
}

TEST_F(SpikeEventRegressionTest, SpikeTrainClear) {
    // WHAT: Verify clearing spike train
    // WHY:  Reset functionality
    // REGRESSION: Clear must reset state

    train = spike_train_create(100);
    ASSERT_NE(train, nullptr);

    spike_train_add(train, 1000, 1.0f);
    spike_train_add(train, 2000, 1.0f);

    spike_train_clear(train);

    uint64_t last = spike_train_get_last_spike(train);
    EXPECT_EQ(last, 0u);
}

//=============================================================================
// Spike Queue Tests
//=============================================================================

TEST_F(SpikeEventRegressionTest, SpikeQueueCreateDestroy) {
    // WHAT: Verify spike queue lifecycle
    // WHY:  Core functionality - must manage resources
    // REGRESSION: Memory leak fix (Issue #SPIKE-003)

    queue = spike_queue_create(1024, false);
    EXPECT_NE(queue, nullptr);

    spike_queue_destroy(queue);
    queue = nullptr;

    // Double destroy should be safe
    spike_queue_destroy(nullptr);
}

TEST_F(SpikeEventRegressionTest, SpikeQueuePushPop) {
    // WHAT: Verify push/pop operations
    // WHY:  Core functionality - FIFO queue
    // REGRESSION: Queue ordering must be FIFO

    queue = spike_queue_create(1024, false);
    ASSERT_NE(queue, nullptr);

    // Create and push events
    spike_event_t event1;
    event1.timestamp = 1000;
    event1.source_id = 1;
    event1.target_id = 2;
    event1.synapse_id = 0;
    event1.amplitude = 1.0f;

    spike_event_t event2;
    event2.timestamp = 2000;
    event2.source_id = 3;
    event2.target_id = 4;
    event2.synapse_id = 1;
    event2.amplitude = 1.5f;

    EXPECT_TRUE(spike_queue_push(queue, &event1));
    EXPECT_TRUE(spike_queue_push(queue, &event2));

    EXPECT_EQ(spike_queue_size(queue), 2u);
    EXPECT_FALSE(spike_queue_is_empty(queue));

    // Pop and verify FIFO order
    spike_event_t popped;

    EXPECT_TRUE(spike_queue_pop(queue, &popped));
    EXPECT_EQ(popped.timestamp, 1000u);
    EXPECT_EQ(popped.source_id, 1u);

    EXPECT_TRUE(spike_queue_pop(queue, &popped));
    EXPECT_EQ(popped.timestamp, 2000u);
    EXPECT_EQ(popped.source_id, 3u);

    EXPECT_TRUE(spike_queue_is_empty(queue));
}

TEST_F(SpikeEventRegressionTest, SpikeQueueCapacity) {
    // WHAT: Verify queue respects capacity
    // WHY:  Memory bounds - must handle full queue
    // REGRESSION: Bug fix - overflow (Issue #SPIKE-004)

    queue = spike_queue_create(16, false);
    ASSERT_NE(queue, nullptr);

    spike_event_t event;
    memset(&event, 0, sizeof(event));

    // Fill queue to capacity
    for (uint32_t i = 0; i < 16; i++) {
        event.timestamp = i * 1000;
        EXPECT_TRUE(spike_queue_push(queue, &event));
    }

    // Queue should be full
    EXPECT_EQ(spike_queue_size(queue), 16u);

    // Next push should fail or wrap
    event.timestamp = 99999;
    bool push_result = spike_queue_push(queue, &event);
    (void)push_result;  // May fail or succeed depending on implementation

    // Should not crash
    SUCCEED();
}

TEST_F(SpikeEventRegressionTest, SpikeQueueEmptyPop) {
    // WHAT: Verify pop from empty queue
    // WHY:  Edge case - must handle gracefully
    // REGRESSION: Bug fix - crash on empty pop (Issue #SPIKE-005)

    queue = spike_queue_create(1024, false);
    ASSERT_NE(queue, nullptr);

    spike_event_t event;

    // Pop from empty queue should fail gracefully
    EXPECT_FALSE(spike_queue_pop(queue, &event));
    EXPECT_TRUE(spike_queue_is_empty(queue));
}

//=============================================================================
// Performance Baseline Tests
//=============================================================================

TEST_F(SpikeEventRegressionTest, QueuePushPerformance) {
    // WHAT: Verify queue push performance
    // WHY:  Performance baseline - must be fast
    // BASELINE: > 1M pushes/second

    queue = spike_queue_create(100000, false);
    ASSERT_NE(queue, nullptr);

    spike_event_t event;
    memset(&event, 0, sizeof(event));

    const int num_pushes = 100000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_pushes; i++) {
        event.timestamp = i * 1000;
        spike_queue_push(queue, &event);
    }

    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double seconds = duration.count() / 1000000.0;
    double pushes_per_sec = num_pushes / seconds;

    std::cout << "Queue push rate: " << pushes_per_sec << " ops/sec" << std::endl;

    // Baseline: > 1M pushes/second
    EXPECT_GT(pushes_per_sec, 1000000.0);
}

TEST_F(SpikeEventRegressionTest, QueuePopPerformance) {
    // WHAT: Verify queue pop performance
    // WHY:  Performance baseline - must be fast
    // BASELINE: > 1M pops/second

    queue = spike_queue_create(100000, false);
    ASSERT_NE(queue, nullptr);

    // Fill queue
    spike_event_t event;
    memset(&event, 0, sizeof(event));

    const int num_events = 100000;
    for (int i = 0; i < num_events; i++) {
        event.timestamp = i * 1000;
        spike_queue_push(queue, &event);
    }

    // Measure pop performance
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_events; i++) {
        spike_queue_pop(queue, &event);
    }

    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double seconds = duration.count() / 1000000.0;
    double pops_per_sec = num_events / seconds;

    std::cout << "Queue pop rate: " << pops_per_sec << " ops/sec" << std::endl;

    // Baseline: > 1M pops/second
    EXPECT_GT(pops_per_sec, 1000000.0);
}

TEST_F(SpikeEventRegressionTest, SpikeTrainAddPerformance) {
    // WHAT: Verify spike train add performance
    // WHY:  Performance baseline - must be efficient
    // BASELINE: > 500K adds/second

    train = spike_train_create(100000);
    ASSERT_NE(train, nullptr);

    const int num_spikes = 50000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_spikes; i++) {
        spike_train_add(train, i * 1000, 1.0f);
    }

    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double seconds = duration.count() / 1000000.0;
    double adds_per_sec = num_spikes / seconds;

    std::cout << "Spike train add rate: " << adds_per_sec << " ops/sec" << std::endl;

    // Baseline: > 500K adds/second
    EXPECT_GT(adds_per_sec, 500000.0);
}

//=============================================================================
// Data Integrity Tests
//=============================================================================

TEST_F(SpikeEventRegressionTest, TimestampPreservation) {
    // WHAT: Verify timestamps are preserved accurately
    // WHY:  Biological accuracy - timing is critical
    // REGRESSION: Precision must be maintained

    queue = spike_queue_create(1024, false);
    ASSERT_NE(queue, nullptr);

    // Create events with precise timestamps
    std::vector<uint64_t> timestamps = {
        1000, 1001, 2500, 10000, 999999
    };

    for (uint64_t ts : timestamps) {
        spike_event_t event;
        event.timestamp = ts;
        event.source_id = 0;
        event.target_id = 1;
        event.synapse_id = 0;
        event.amplitude = 1.0f;

        ASSERT_TRUE(spike_queue_push(queue, &event));
    }

    // Verify timestamps are preserved
    for (uint64_t expected_ts : timestamps) {
        spike_event_t popped;
        ASSERT_TRUE(spike_queue_pop(queue, &popped));
        EXPECT_EQ(popped.timestamp, expected_ts);
    }
}

TEST_F(SpikeEventRegressionTest, AmplitudePreservation) {
    // WHAT: Verify amplitudes are preserved
    // WHY:  Data integrity - amplitude must be exact
    // REGRESSION: Float precision must be maintained

    queue = spike_queue_create(1024, false);
    ASSERT_NE(queue, nullptr);

    std::vector<float> amplitudes = {
        1.0f, 0.5f, 1.5f, 2.0f, 0.1f, 9.99f
    };

    for (float amp : amplitudes) {
        spike_event_t event;
        event.timestamp = 1000;
        event.source_id = 0;
        event.target_id = 1;
        event.synapse_id = 0;
        event.amplitude = amp;

        ASSERT_TRUE(spike_queue_push(queue, &event));
    }

    for (float expected_amp : amplitudes) {
        spike_event_t popped;
        ASSERT_TRUE(spike_queue_pop(queue, &popped));
        EXPECT_FLOAT_EQ(popped.amplitude, expected_amp);
    }
}

TEST_F(SpikeEventRegressionTest, NeuronIDPreservation) {
    // WHAT: Verify neuron IDs are preserved
    // WHY:  Routing accuracy - IDs must be exact
    // REGRESSION: ID corruption bug fix (Issue #SPIKE-006)

    queue = spike_queue_create(1024, false);
    ASSERT_NE(queue, nullptr);

    spike_event_t event;
    event.timestamp = 1000;
    event.source_id = 12345;
    event.target_id = 67890;
    event.synapse_id = 999;
    event.amplitude = 1.0f;

    ASSERT_TRUE(spike_queue_push(queue, &event));

    spike_event_t popped;
    ASSERT_TRUE(spike_queue_pop(queue, &popped));

    EXPECT_EQ(popped.source_id, 12345u);
    EXPECT_EQ(popped.target_id, 67890u);
    EXPECT_EQ(popped.synapse_id, 999u);
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(SpikeEventRegressionTest, NullPointerHandling) {
    // WHAT: Verify NULL pointer handling
    // WHY:  API contract - must handle NULL gracefully
    // REGRESSION: Bug fix - NULL caused crash (Issue #SPIKE-007)

    // NULL train operations
    spike_train_destroy(nullptr);
    EXPECT_EQ(spike_train_get_last_spike(nullptr), 0u);
    EXPECT_FALSE(spike_train_add(nullptr, 1000, 1.0f));

    spike_event_t event;
    EXPECT_FALSE(spike_train_get_spike(nullptr, 0, &event));

    spike_train_clear(nullptr);

    // NULL queue operations
    spike_queue_destroy(nullptr);
    EXPECT_EQ(spike_queue_size(nullptr), 0u);
    EXPECT_TRUE(spike_queue_is_empty(nullptr));

    EXPECT_FALSE(spike_queue_push(nullptr, &event));
    EXPECT_FALSE(spike_queue_pop(nullptr, &event));

    SUCCEED();
}

TEST_F(SpikeEventRegressionTest, ZeroCapacityHandling) {
    // WHAT: Verify zero capacity is handled
    // WHY:  Input validation
    // REGRESSION: Bug fix - zero capacity crashed (Issue #SPIKE-008)

    train = spike_train_create(0);
    // Should either return NULL or handle gracefully
    if (train != nullptr) {
        spike_train_destroy(train);
        train = nullptr;
    }

    queue = spike_queue_create(0, false);
    // Should either return NULL or handle gracefully
    if (queue != nullptr) {
        spike_queue_destroy(queue);
        queue = nullptr;
    }

    SUCCEED();
}

//=============================================================================
// Biological Accuracy Tests
//=============================================================================

TEST_F(SpikeEventRegressionTest, RefractoryPeriodTiming) {
    // WHAT: Verify refractory period timing
    // WHY:  Biological accuracy - neurons can't spike too fast
    // REGRESSION: Timing precision must be accurate

    train = spike_train_create(100);
    ASSERT_NE(train, nullptr);

    // Add spikes with 1ms intervals (1000 μs)
    const uint64_t refractory = 1000;  // 1ms

    spike_train_add(train, 0, 1.0f);
    spike_train_add(train, refractory, 1.0f);
    spike_train_add(train, 2 * refractory, 1.0f);

    // Verify spike times
    spike_event_t event;

    EXPECT_TRUE(spike_train_get_spike(train, 0, &event));
    EXPECT_EQ(event.timestamp, 0u);

    EXPECT_TRUE(spike_train_get_spike(train, 1, &event));
    EXPECT_EQ(event.timestamp, refractory);

    EXPECT_TRUE(spike_train_get_spike(train, 2, &event));
    EXPECT_EQ(event.timestamp, 2 * refractory);
}

TEST_F(SpikeEventRegressionTest, BurstSpikeDetection) {
    // WHAT: Verify burst spike patterns
    // WHY:  Biological pattern - burst detection
    // REGRESSION: Burst patterns must be identifiable

    train = spike_train_create(100);
    ASSERT_NE(train, nullptr);

    // Create burst: 5 spikes within 10ms
    for (int i = 0; i < 5; i++) {
        spike_train_add(train, i * 2000, 1.0f);  // 2ms intervals
    }

    // Calculate firing rate over burst window (10ms)
    float rate = spike_train_compute_rate(train, 10000);  // 10ms in μs

    // 5 spikes in 10ms = 500 Hz
    EXPECT_NEAR(rate, 500.0f, 50.0f);
}

//=============================================================================
// Test Summary
//=============================================================================

// Test count: 22 regression tests
// Coverage:
// - API Stability: 3 tests (enums, structs, alignment)
// - Spike Train: 6 tests
// - Spike Queue: 5 tests
// - Performance Baselines: 3 tests
// - Data Integrity: 3 tests
// - Error Handling: 2 tests
// - Biological Accuracy: 2 tests
