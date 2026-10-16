#include <gtest/gtest.h>
#include <cstring>

#include "gpu/nimcp_spike_event.h"

//=============================================================================
// Spike Event Real Tests
//=============================================================================

class SpikeEventRealTest : public ::testing::Test {
protected:
    spike_train_t* train = nullptr;
    spike_queue_t* queue = nullptr;

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
// Spike Train Tests
//=============================================================================

TEST_F(SpikeEventRealTest, CreateSpikeTrain) {
    train = spike_train_create(100);
    ASSERT_NE(train, nullptr);
}

TEST_F(SpikeEventRealTest, CreateSpikeTrainZeroCapacity) {
    train = spike_train_create(0);
    EXPECT_EQ(train, nullptr);
}

TEST_F(SpikeEventRealTest, DestroySpikeTrainNull) {
    // Should not crash
    spike_train_destroy(nullptr);
    SUCCEED();
}

TEST_F(SpikeEventRealTest, AddSpike) {
    train = spike_train_create(100);
    ASSERT_NE(train, nullptr);

    bool result = spike_train_add(train, 1000, 1.0f);
    EXPECT_TRUE(result);
}

TEST_F(SpikeEventRealTest, AddMultipleSpikes) {
    train = spike_train_create(100);
    ASSERT_NE(train, nullptr);

    for (uint64_t i = 1; i <= 10; i++) {
        bool result = spike_train_add(train, i * 1000, 1.0f);
        EXPECT_TRUE(result);
    }
}

TEST_F(SpikeEventRealTest, AddSpikeNull) {
    bool result = spike_train_add(nullptr, 1000, 1.0f);
    EXPECT_FALSE(result);
}

TEST_F(SpikeEventRealTest, GetLastSpike) {
    train = spike_train_create(100);
    ASSERT_NE(train, nullptr);

    spike_train_add(train, 1000, 1.0f);
    spike_train_add(train, 2000, 1.0f);

    uint64_t last = spike_train_get_last_spike(train);
    EXPECT_EQ(last, 2000);
}

TEST_F(SpikeEventRealTest, GetLastSpikeEmpty) {
    train = spike_train_create(100);
    ASSERT_NE(train, nullptr);

    uint64_t last = spike_train_get_last_spike(train);
    EXPECT_EQ(last, 0);
}

TEST_F(SpikeEventRealTest, GetLastSpikeNull) {
    uint64_t last = spike_train_get_last_spike(nullptr);
    EXPECT_EQ(last, 0);
}

TEST_F(SpikeEventRealTest, GetSpike) {
    train = spike_train_create(100);
    ASSERT_NE(train, nullptr);

    spike_train_add(train, 1000, 1.0f);
    spike_train_add(train, 2000, 1.5f);

    spike_event_t event;
    bool result = spike_train_get_spike(train, 0, &event);
    EXPECT_TRUE(result);
    EXPECT_EQ(event.timestamp, 1000);
    EXPECT_FLOAT_EQ(event.amplitude, 1.0f);
}

TEST_F(SpikeEventRealTest, GetSpikeNull) {
    train = spike_train_create(100);
    ASSERT_NE(train, nullptr);

    spike_train_add(train, 1000, 1.0f);

    bool result = spike_train_get_spike(train, 0, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(SpikeEventRealTest, GetSpikeInvalidIndex) {
    train = spike_train_create(100);
    ASSERT_NE(train, nullptr);

    spike_train_add(train, 1000, 1.0f);

    spike_event_t event;
    bool result = spike_train_get_spike(train, 999, &event);
    EXPECT_FALSE(result);
}

TEST_F(SpikeEventRealTest, ComputeFiringRate) {
    train = spike_train_create(100);
    ASSERT_NE(train, nullptr);

    // Add spikes over 1 second window
    for (uint64_t i = 0; i < 10; i++) {
        spike_train_add(train, i * 100000, 1.0f);
    }

    float rate = spike_train_compute_rate(train, 1000000);
    EXPECT_GE(rate, 0.0f);
}

TEST_F(SpikeEventRealTest, ComputeFiringRateEmpty) {
    train = spike_train_create(100);
    ASSERT_NE(train, nullptr);

    float rate = spike_train_compute_rate(train, 1000000);
    EXPECT_FLOAT_EQ(rate, 0.0f);
}

TEST_F(SpikeEventRealTest, ComputeFiringRateNull) {
    float rate = spike_train_compute_rate(nullptr, 1000000);
    EXPECT_FLOAT_EQ(rate, 0.0f);
}

TEST_F(SpikeEventRealTest, ClearSpikeTrain) {
    train = spike_train_create(100);
    ASSERT_NE(train, nullptr);

    spike_train_add(train, 1000, 1.0f);
    spike_train_add(train, 2000, 1.0f);

    spike_train_clear(train);

    uint64_t last = spike_train_get_last_spike(train);
    EXPECT_EQ(last, 0);
}

TEST_F(SpikeEventRealTest, ClearSpikeTrainNull) {
    // Should not crash
    spike_train_clear(nullptr);
    SUCCEED();
}

//=============================================================================
// Spike Queue Tests
//=============================================================================

TEST_F(SpikeEventRealTest, CreateSpikeQueue) {
    queue = spike_queue_create(1024, false);
    ASSERT_NE(queue, nullptr);
}

TEST_F(SpikeEventRealTest, CreateSpikeQueueZeroCapacity) {
    queue = spike_queue_create(0, false);
    // May return nullptr or valid queue (implementation-defined)
    SUCCEED();
}

TEST_F(SpikeEventRealTest, DestroySpikeQueueNull) {
    // Should not crash
    spike_queue_destroy(nullptr);
    SUCCEED();
}

TEST_F(SpikeEventRealTest, PushSpikeEvent) {
    queue = spike_queue_create(1024, false);
    ASSERT_NE(queue, nullptr);

    spike_event_t event = {};
    event.timestamp = 1000;
    event.source_id = 0;
    event.target_id = 1;
    event.synapse_id = 0;
    event.amplitude = 1.0f;

    bool result = spike_queue_push(queue, &event);
    EXPECT_TRUE(result);
}

TEST_F(SpikeEventRealTest, PushMultipleSpikeEvents) {
    queue = spike_queue_create(1024, false);
    ASSERT_NE(queue, nullptr);

    for (uint32_t i = 0; i < 10; i++) {
        spike_event_t event = {};
        event.timestamp = i * 1000;
        event.source_id = i;
        event.target_id = i + 1;
        event.synapse_id = i;
        event.amplitude = 1.0f;

        bool result = spike_queue_push(queue, &event);
        EXPECT_TRUE(result);
    }
}

TEST_F(SpikeEventRealTest, PushSpikeEventNull) {
    queue = spike_queue_create(1024, false);
    ASSERT_NE(queue, nullptr);

    bool result = spike_queue_push(queue, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(SpikeEventRealTest, PopSpikeEvent) {
    queue = spike_queue_create(1024, false);
    ASSERT_NE(queue, nullptr);

    spike_event_t push_event = {};
    push_event.timestamp = 1000;
    push_event.source_id = 0;
    push_event.target_id = 1;
    push_event.amplitude = 1.0f;

    spike_queue_push(queue, &push_event);

    spike_event_t pop_event;
    bool result = spike_queue_pop(queue, &pop_event);
    EXPECT_TRUE(result);
    EXPECT_EQ(pop_event.timestamp, 1000);
    EXPECT_EQ(pop_event.source_id, 0);
    EXPECT_EQ(pop_event.target_id, 1);
}

TEST_F(SpikeEventRealTest, PopSpikeEventEmpty) {
    queue = spike_queue_create(1024, false);
    ASSERT_NE(queue, nullptr);

    spike_event_t event;
    bool result = spike_queue_pop(queue, &event);
    EXPECT_FALSE(result);
}

TEST_F(SpikeEventRealTest, PopSpikeEventNull) {
    queue = spike_queue_create(1024, false);
    ASSERT_NE(queue, nullptr);

    spike_event_t event = {};
    spike_queue_push(queue, &event);

    bool result = spike_queue_pop(queue, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(SpikeEventRealTest, GetQueueSize) {
    queue = spike_queue_create(1024, false);
    ASSERT_NE(queue, nullptr);

    spike_event_t event = {};
    spike_queue_push(queue, &event);
    spike_queue_push(queue, &event);

    uint32_t size = spike_queue_size(queue);
    EXPECT_EQ(size, 2);
}

TEST_F(SpikeEventRealTest, GetQueueSizeEmpty) {
    queue = spike_queue_create(1024, false);
    ASSERT_NE(queue, nullptr);

    uint32_t size = spike_queue_size(queue);
    EXPECT_EQ(size, 0);
}

TEST_F(SpikeEventRealTest, GetQueueSizeNull) {
    uint32_t size = spike_queue_size(nullptr);
    EXPECT_EQ(size, 0);
}

TEST_F(SpikeEventRealTest, IsQueueEmpty) {
    queue = spike_queue_create(1024, false);
    ASSERT_NE(queue, nullptr);

    bool empty = spike_queue_is_empty(queue);
    EXPECT_TRUE(empty);

    spike_event_t event = {};
    spike_queue_push(queue, &event);

    empty = spike_queue_is_empty(queue);
    EXPECT_FALSE(empty);
}

TEST_F(SpikeEventRealTest, IsQueueEmptyNull) {
    bool empty = spike_queue_is_empty(nullptr);
    EXPECT_TRUE(empty);
}

TEST_F(SpikeEventRealTest, SyncGPU) {
    queue = spike_queue_create(1024, false);
    ASSERT_NE(queue, nullptr);

    // May fail if not GPU-enabled, but shouldn't crash
    bool result = spike_queue_sync_gpu(queue, true);
    SUCCEED();
}

TEST_F(SpikeEventRealTest, SyncGPUNull) {
    bool result = spike_queue_sync_gpu(nullptr, true);
    EXPECT_FALSE(result);
}
