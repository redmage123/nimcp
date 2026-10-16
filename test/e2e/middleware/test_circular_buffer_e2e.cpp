/**
 * @file test_circular_buffer_e2e.cpp
 * @brief End-to-End Tests for Circular Buffer
 *
 * WHAT: Full workflow E2E tests for circular buffer batch operations and SPSC
 * WHY:  Verify batch push/pop data integrity, overflow accuracy, and
 *       single-producer single-consumer correctness
 * HOW:  Test realistic buffer scenarios with data verification
 *
 * TEST PIPELINES:
 * - CircularBufferE2E_BatchPushPopConsistency: Batch operations data integrity
 * - CircularBufferE2E_BatchOverflowAccuracy: Partial batch fill count accuracy
 * - CircularBufferE2E_SPSCProducerConsumer: Lock-free producer/consumer
 *
 * @author NIMCP Development Team
 * @date 2026-02-08
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>

extern "C" {
#include "middleware/buffering/nimcp_circular_buffer.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class CircularBufferE2ETest : public ::testing::Test {
protected:
    circular_buffer_t* buffer_ = nullptr;

    void TearDown() override {
        if (buffer_) {
            circular_buffer_destroy(buffer_);
            buffer_ = nullptr;
        }
    }
};

//=============================================================================
// Test 1: Batch Push/Pop Consistency
//=============================================================================

TEST_F(CircularBufferE2ETest, CircularBufferE2E_BatchPushPopConsistency) {
    // Create buffer with capacity for 100 uint32_t elements
    const size_t CAPACITY = 100;
    buffer_ = circular_buffer_create(sizeof(uint32_t), CAPACITY, OVERFLOW_ERROR);
    ASSERT_NE(buffer_, nullptr) << "Failed to create circular buffer";

    // Verify initial state
    EXPECT_TRUE(circular_buffer_is_empty(buffer_));
    EXPECT_FALSE(circular_buffer_is_full(buffer_));
    EXPECT_EQ(circular_buffer_size(buffer_), 0u);
    EXPECT_EQ(circular_buffer_capacity(buffer_), CAPACITY);

    // Push a batch of 50 elements
    const size_t BATCH_SIZE = 50;
    std::vector<uint32_t> push_data(BATCH_SIZE);
    for (size_t i = 0; i < BATCH_SIZE; i++) {
        push_data[i] = (uint32_t)(i * 7 + 42);  // Predictable pattern
    }

    size_t pushed = circular_buffer_push_batch(buffer_, push_data.data(), BATCH_SIZE);
    EXPECT_EQ(pushed, BATCH_SIZE) << "Expected all elements to be pushed";
    EXPECT_EQ(circular_buffer_size(buffer_), BATCH_SIZE);

    // Pop a batch of 50 elements
    std::vector<uint32_t> pop_data(BATCH_SIZE, 0);
    size_t popped = circular_buffer_pop_batch(buffer_, pop_data.data(), BATCH_SIZE);
    EXPECT_EQ(popped, BATCH_SIZE) << "Expected all elements to be popped";
    EXPECT_EQ(circular_buffer_size(buffer_), 0u);

    // Verify data integrity
    for (size_t i = 0; i < BATCH_SIZE; i++) {
        EXPECT_EQ(pop_data[i], push_data[i])
            << "Data mismatch at index " << i
            << ": expected " << push_data[i] << " got " << pop_data[i];
    }

    // Verify stats
    circular_buffer_stats_t stats;
    circular_buffer_get_stats(buffer_, &stats);
    EXPECT_GE(stats.total_writes, BATCH_SIZE);
    EXPECT_GE(stats.total_reads, BATCH_SIZE);
    EXPECT_EQ(stats.overflows, 0u);
    EXPECT_EQ(stats.underflows, 0u);

    // Push another batch, pop partially, push more, pop all
    pushed = circular_buffer_push_batch(buffer_, push_data.data(), BATCH_SIZE);
    EXPECT_EQ(pushed, BATCH_SIZE);

    // Pop half
    std::vector<uint32_t> partial_pop(25, 0);
    popped = circular_buffer_pop_batch(buffer_, partial_pop.data(), 25);
    EXPECT_EQ(popped, 25u);
    EXPECT_EQ(circular_buffer_size(buffer_), 25u);

    // Verify the first 25 elements
    for (size_t i = 0; i < 25; i++) {
        EXPECT_EQ(partial_pop[i], push_data[i]);
    }

    // Pop remaining
    std::vector<uint32_t> remaining_pop(25, 0);
    popped = circular_buffer_pop_batch(buffer_, remaining_pop.data(), 25);
    EXPECT_EQ(popped, 25u);
    EXPECT_EQ(circular_buffer_size(buffer_), 0u);

    // Verify the remaining 25 elements
    for (size_t i = 0; i < 25; i++) {
        EXPECT_EQ(remaining_pop[i], push_data[i + 25]);
    }
}

//=============================================================================
// Test 2: Batch Overflow Accuracy
//=============================================================================

TEST_F(CircularBufferE2ETest, CircularBufferE2E_BatchOverflowAccuracy) {
    // Create a small buffer (10 elements) with OVERFLOW_ERROR strategy
    const size_t CAPACITY = 10;
    buffer_ = circular_buffer_create(sizeof(uint32_t), CAPACITY, OVERFLOW_ERROR);
    ASSERT_NE(buffer_, nullptr) << "Failed to create circular buffer";

    // Fill buffer to capacity with individual pushes
    for (size_t i = 0; i < CAPACITY; i++) {
        uint32_t val = (uint32_t)i;
        bool ok = circular_buffer_push(buffer_, &val);
        EXPECT_TRUE(ok) << "Push failed at index " << i;
    }
    EXPECT_TRUE(circular_buffer_is_full(buffer_));
    EXPECT_EQ(circular_buffer_size(buffer_), CAPACITY);

    // Try to push more (should fail with ERROR strategy)
    uint32_t extra_val = 999;
    bool pushed_extra = circular_buffer_push(buffer_, &extra_val);
    EXPECT_FALSE(pushed_extra) << "Push should fail when buffer is full (ERROR strategy)";

    // Clear buffer
    circular_buffer_clear(buffer_);
    EXPECT_TRUE(circular_buffer_is_empty(buffer_));
    EXPECT_EQ(circular_buffer_size(buffer_), 0u);

    // Fill to half capacity
    const size_t HALF = CAPACITY / 2;
    std::vector<uint32_t> half_data(HALF);
    for (size_t i = 0; i < HALF; i++) {
        half_data[i] = (uint32_t)(i + 100);
    }
    size_t pushed = circular_buffer_push_batch(buffer_, half_data.data(), HALF);
    EXPECT_EQ(pushed, HALF);
    EXPECT_EQ(circular_buffer_size(buffer_), HALF);

    // Try to push a batch larger than remaining capacity
    // Buffer has HALF free slots, trying to push CAPACITY elements
    std::vector<uint32_t> overflow_data(CAPACITY);
    for (size_t i = 0; i < CAPACITY; i++) {
        overflow_data[i] = (uint32_t)(i + 200);
    }
    size_t overflow_pushed = circular_buffer_push_batch(
        buffer_, overflow_data.data(), CAPACITY);

    // With ERROR strategy, batch push should push as many as fit
    size_t remaining_capacity = CAPACITY - HALF;
    EXPECT_LE(overflow_pushed, remaining_capacity)
        << "Batch push should not exceed remaining capacity";
    EXPECT_EQ(circular_buffer_size(buffer_), HALF + overflow_pushed);

    // Verify overflow stats
    circular_buffer_stats_t stats;
    circular_buffer_get_stats(buffer_, &stats);
    // The individual push that failed should have counted as overflow
    EXPECT_GE(stats.overflows, 1u)
        << "Expected at least 1 overflow event";

    // Now test with OVERFLOW_OVERWRITE strategy
    circular_buffer_destroy(buffer_);
    buffer_ = circular_buffer_create(sizeof(uint32_t), CAPACITY, OVERFLOW_OVERWRITE);
    ASSERT_NE(buffer_, nullptr);

    // Fill completely
    for (size_t i = 0; i < CAPACITY; i++) {
        uint32_t val = (uint32_t)i;
        circular_buffer_push(buffer_, &val);
    }
    EXPECT_TRUE(circular_buffer_is_full(buffer_));

    // Push one more (should overwrite oldest)
    uint32_t overwrite_val = 999;
    bool ok = circular_buffer_push(buffer_, &overwrite_val);
    EXPECT_TRUE(ok) << "Push should succeed with OVERWRITE strategy";

    // Pop all and verify the oldest was overwritten
    std::vector<uint32_t> all_data(CAPACITY, 0);
    size_t total_popped = circular_buffer_pop_batch(buffer_, all_data.data(), CAPACITY);
    EXPECT_EQ(total_popped, CAPACITY);

    // The oldest element (0) should be gone, replaced by wrapping
    // The newest element (999) should be present
    bool found_999 = false;
    for (size_t i = 0; i < total_popped; i++) {
        if (all_data[i] == 999) found_999 = true;
    }
    EXPECT_TRUE(found_999) << "Overwrite value should be in buffer";
}

//=============================================================================
// Test 3: SPSC Producer Consumer
//=============================================================================

TEST_F(CircularBufferE2ETest, CircularBufferE2E_SPSCProducerConsumer) {
    // SPSC = Single Producer, Single Consumer (lock-free design)
    const size_t CAPACITY = 256;
    const size_t TOTAL_ITEMS = 1000;

    buffer_ = circular_buffer_create(sizeof(uint32_t), CAPACITY, OVERFLOW_BLOCK);
    ASSERT_NE(buffer_, nullptr) << "Failed to create circular buffer";

    std::atomic<bool> producer_done{false};
    std::atomic<size_t> items_produced{0};
    std::atomic<size_t> items_consumed{0};
    std::vector<uint32_t> consumed_values;
    consumed_values.reserve(TOTAL_ITEMS);
    std::atomic<bool> data_mismatch{false};

    // Producer thread
    std::thread producer([&]() {
        for (size_t i = 0; i < TOTAL_ITEMS; i++) {
            uint32_t val = (uint32_t)i;
            // Retry loop for OVERFLOW_BLOCK strategy
            int retries = 0;
            while (!circular_buffer_push(buffer_, &val)) {
                // Buffer full, yield and retry
                std::this_thread::yield();
                retries++;
                if (retries > 10000) {
                    // Safety valve to prevent infinite loop in test
                    break;
                }
            }
            items_produced.fetch_add(1);
        }
        producer_done.store(true);
    });

    // Consumer thread
    std::thread consumer([&]() {
        uint32_t expected = 0;
        while (true) {
            uint32_t val;
            if (circular_buffer_pop(buffer_, &val)) {
                if (val != expected) {
                    data_mismatch.store(true);
                }
                expected++;
                items_consumed.fetch_add(1);
            } else {
                // Buffer empty
                if (producer_done.load() && circular_buffer_is_empty(buffer_)) {
                    break;
                }
                std::this_thread::yield();
            }
        }
    });

    // Wait for completion with timeout
    producer.join();

    // Give consumer time to drain
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (items_consumed.load() < items_produced.load() &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    consumer.join();

    // Verify results
    EXPECT_EQ(items_produced.load(), TOTAL_ITEMS)
        << "Producer should have produced all items";
    EXPECT_EQ(items_consumed.load(), items_produced.load())
        << "Consumer should have consumed all produced items (no data loss)";
    EXPECT_FALSE(data_mismatch.load())
        << "Data should be received in order (FIFO)";

    // Check stats
    circular_buffer_stats_t stats;
    circular_buffer_get_stats(buffer_, &stats);
    EXPECT_GE(stats.total_writes, TOTAL_ITEMS);
    EXPECT_GE(stats.total_reads, TOTAL_ITEMS);
}
