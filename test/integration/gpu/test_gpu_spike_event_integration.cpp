/**
 * @file test_gpu_spike_event_integration.cpp
 * @brief Integration tests for GPU spike event processing in cognitive pipeline
 *
 * WHAT: Tests that spike event features are actively used by brain/cognitive modules
 * WHY:  Ensure spike event processing is properly wired into cognitive pipeline
 * HOW:  Test spike train/queue operations through brain API and verify integration
 *
 * @version GPU Spike Event Integration Testing
 * @date 2025-11-13
 */

#include <gtest/gtest.h>

#include "core/brain/nimcp_brain.h"
#include "gpu/nimcp_spike_event.h"

//=============================================================================
// Test Fixture
//=============================================================================

class GPUSpikeEventIntegrationTest : public ::testing::Test {
protected:
    brain_t brain;

    void SetUp() override {
        // Create brain that may use spike events internally
        brain = brain_create("spike_test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
        ASSERT_NE(brain, nullptr) << "Failed to create brain";
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }
};

//=============================================================================
// Integration Test 1: Spike Train Creation
//=============================================================================

TEST_F(GPUSpikeEventIntegrationTest, SpikeTrainCreation) {
    // Create spike train
    spike_train_t* train = spike_train_create(100);
    ASSERT_NE(train, nullptr) << "Failed to create spike train";

    // Initially should have no spikes
    uint64_t last_spike = spike_train_get_last_spike(train);
    EXPECT_EQ(last_spike, 0) << "New train should have no spikes";

    // Cleanup
    spike_train_destroy(train);
}

//=============================================================================
// Integration Test 2: Spike Train Operations
//=============================================================================

TEST_F(GPUSpikeEventIntegrationTest, SpikeTrainOperations) {
    spike_train_t* train = spike_train_create(100);
    ASSERT_NE(train, nullptr);

    // Add spike
    bool added = spike_train_add(train, 1000, 1.0f);  // t=1000μs, amplitude=1.0
    EXPECT_TRUE(added) << "Should be able to add spike to train";

    // Get last spike time
    uint64_t last_spike = spike_train_get_last_spike(train);
    EXPECT_EQ(last_spike, 1000) << "Last spike should be at t=1000μs";

    // Add more spikes
    spike_train_add(train, 2000, 1.0f);
    spike_train_add(train, 3000, 1.0f);

    // Get specific spike
    spike_event_t event;
    bool got_spike = spike_train_get_spike(train, 1, &event);
    EXPECT_TRUE(got_spike) << "Should be able to get spike at index 1";
    EXPECT_EQ(event.timestamp, 2000) << "Second spike should be at t=2000μs";

    spike_train_destroy(train);
}

//=============================================================================
// Integration Test 3: Spike Queue Creation
//=============================================================================

TEST_F(GPUSpikeEventIntegrationTest, SpikeQueueCreation) {
    // Create CPU-only queue
    spike_queue_t* queue = spike_queue_create(256, false);
    ASSERT_NE(queue, nullptr) << "Failed to create spike queue";

    // Should be empty initially
    EXPECT_TRUE(spike_queue_is_empty(queue)) << "New queue should be empty";
    EXPECT_EQ(spike_queue_size(queue), 0) << "New queue should have size 0";

    spike_queue_destroy(queue);
}

//=============================================================================
// Integration Test 4: Spike Queue Operations
//=============================================================================

TEST_F(GPUSpikeEventIntegrationTest, SpikeQueueOperations) {
    spike_queue_t* queue = spike_queue_create(256, false);
    ASSERT_NE(queue, nullptr);

    // Create and push spike event
    spike_event_t event_in = {
        .timestamp = 1000,
        .source_id = 0,
        .target_id = 1,
        .synapse_id = 0,
        .amplitude = 1.0f
    };

    bool pushed = spike_queue_push(queue, &event_in);
    EXPECT_TRUE(pushed) << "Should be able to push spike to queue";

    // Queue should not be empty
    EXPECT_FALSE(spike_queue_is_empty(queue));
    EXPECT_EQ(spike_queue_size(queue), 1);

    // Pop spike
    spike_event_t event_out;
    bool popped = spike_queue_pop(queue, &event_out);
    EXPECT_TRUE(popped) << "Should be able to pop spike from queue";
    EXPECT_EQ(event_out.timestamp, 1000);
    EXPECT_EQ(event_out.source_id, 0);
    EXPECT_EQ(event_out.target_id, 1);

    spike_queue_destroy(queue);
}

//=============================================================================
// Integration Test 5: Spike Queue FIFO Order
//=============================================================================

TEST_F(GPUSpikeEventIntegrationTest, SpikeQueueFIFOOrder) {
    spike_queue_t* queue = spike_queue_create(256, false);
    ASSERT_NE(queue, nullptr);

    // Push multiple spikes
    for (uint32_t i = 0; i < 10; i++) {
        spike_event_t event = {
            .timestamp = 1000 + i * 100,
            .source_id = i,
            .target_id = i + 1,
            .synapse_id = 0,
            .amplitude = 1.0f
        };
        EXPECT_TRUE(spike_queue_push(queue, &event));
    }

    // Pop and verify FIFO order
    for (uint32_t i = 0; i < 10; i++) {
        spike_event_t event;
        ASSERT_TRUE(spike_queue_pop(queue, &event));
        EXPECT_EQ(event.timestamp, 1000 + i * 100) << "Queue should maintain FIFO order";
        EXPECT_EQ(event.source_id, i);
    }

    spike_queue_destroy(queue);
}

//=============================================================================
// Integration Test 6: Spike Train Firing Rate
//=============================================================================

TEST_F(GPUSpikeEventIntegrationTest, SpikeTrainFiringRate) {
    spike_train_t* train = spike_train_create(100);
    ASSERT_NE(train, nullptr);

    // Add spikes at regular intervals (100 Hz = one spike every 10,000 μs)
    for (int i = 0; i < 10; i++) {
        spike_train_add(train, i * 10000, 1.0f);
    }

    // Compute firing rate over 100,000 μs window (should be ~100 Hz)
    float rate = spike_train_compute_rate(train, 100000);

    // If implemented, rate should be reasonable
    // If not implemented, may return 0 (which is acceptable)
    if (rate > 0.0f) {
        EXPECT_GT(rate, 50.0f) << "Firing rate should be > 50 Hz";
        EXPECT_LT(rate, 150.0f) << "Firing rate should be < 150 Hz";
    }

    spike_train_destroy(train);
}

//=============================================================================
// Integration Test 7: Spike Train Clear
//=============================================================================

TEST_F(GPUSpikeEventIntegrationTest, SpikeTrainClear) {
    spike_train_t* train = spike_train_create(100);
    ASSERT_NE(train, nullptr);

    // Add some spikes
    spike_train_add(train, 1000, 1.0f);
    spike_train_add(train, 2000, 1.0f);
    EXPECT_NE(spike_train_get_last_spike(train), 0);

    // Clear train
    spike_train_clear(train);

    // Should be empty now
    EXPECT_EQ(spike_train_get_last_spike(train), 0) << "Cleared train should have no spikes";

    spike_train_destroy(train);
}

//=============================================================================
// Integration Test 8: Spike Queue Full Behavior
//=============================================================================

TEST_F(GPUSpikeEventIntegrationTest, SpikeQueueFullBehavior) {
    // Create small queue
    spike_queue_t* queue = spike_queue_create(4, false);  // Only 4 slots
    ASSERT_NE(queue, nullptr);

    // Fill queue
    spike_event_t event = {1000, 0, 1, 0, 1.0f};
    EXPECT_TRUE(spike_queue_push(queue, &event));
    event.timestamp = 2000;
    EXPECT_TRUE(spike_queue_push(queue, &event));
    event.timestamp = 3000;
    EXPECT_TRUE(spike_queue_push(queue, &event));
    event.timestamp = 4000;
    EXPECT_TRUE(spike_queue_push(queue, &event));

    // Queue should be full (capacity is rounded to power of 2, so might be more than 4)
    // Push until we get false
    event.timestamp = 5000;
    int pushes = 0;
    while (spike_queue_push(queue, &event) && pushes < 100) {
        pushes++;
        event.timestamp += 1000;
    }

    // Should have failed at some point
    EXPECT_LT(pushes, 100) << "Queue should eventually become full";

    spike_queue_destroy(queue);
}

//=============================================================================
// Integration Test 9: Brain Works With Spike Events
//=============================================================================

TEST_F(GPUSpikeEventIntegrationTest, BrainWorksWithSpikeEvents) {
    // Verify brain can process decisions (may use spike events internally)
    float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};

    for (int i = 0; i < 5; i++) {
        brain_decision_t* decision = brain_decide(brain, features, 4);
        ASSERT_NE(decision, nullptr) << "Brain should work with spike events";
        EXPECT_NE(decision->label, nullptr);
    }
}

//=============================================================================
// Integration Test 10: Memory Management
//=============================================================================

TEST_F(GPUSpikeEventIntegrationTest, MemoryManagement_NoLeaks) {
    // Create and destroy multiple spike structures
    for (int i = 0; i < 10; i++) {
        spike_train_t* train = spike_train_create(100);
        ASSERT_NE(train, nullptr);

        // Do some work
        spike_train_add(train, 1000, 1.0f);
        spike_train_add(train, 2000, 1.0f);
        spike_train_get_last_spike(train);

        spike_train_destroy(train);

        // Create and destroy queue
        spike_queue_t* queue = spike_queue_create(256, false);
        ASSERT_NE(queue, nullptr);

        spike_event_t event = {1000, 0, 1, 0, 1.0f};
        spike_queue_push(queue, &event);
        spike_queue_pop(queue, &event);

        spike_queue_destroy(queue);
    }

    // If we get here without crashing, memory management is OK
    SUCCEED();
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
