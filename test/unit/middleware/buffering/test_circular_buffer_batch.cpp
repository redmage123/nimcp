/**
 * @file test_circular_buffer_batch.cpp
 * @brief Unit tests for circular buffer batch operations
 *
 * Validates that push_batch and pop_batch correctly report
 * the number of elements actually written/read, especially
 * after the P1-10 fix for inflated written counter.
 */

#include <gtest/gtest.h>
#include "middleware/buffering/nimcp_circular_buffer.h"

// ============================================================================
// Test Fixture
// ============================================================================

class CircularBufferBatchTest : public ::testing::Test {
protected:
    void TearDown() override {
        // Buffers are destroyed in individual tests or via helpers
    }
};

// ============================================================================
// Push Batch Tests
// ============================================================================

/**
 * PushBatch_CountMatchesActualPushes
 *
 * Push a batch of 10 items into a buffer with capacity 10.
 * Verify the returned count equals the actual number of items in the buffer.
 */
TEST_F(CircularBufferBatchTest, PushBatch_CountMatchesActualPushes) {
    circular_buffer_t* buffer = circular_buffer_create(sizeof(float), 10, OVERFLOW_ERROR);
    ASSERT_NE(buffer, nullptr);

    float values[10];
    for (int i = 0; i < 10; i++) {
        values[i] = (float)(i + 1);
    }

    size_t written = circular_buffer_push_batch(buffer, values, 10);

    // All 10 should succeed since capacity is 10
    EXPECT_EQ(written, 10u);
    EXPECT_EQ(circular_buffer_size(buffer), 10u);

    // Verify the actual data matches
    float out_values[10];
    size_t popped = circular_buffer_pop_batch(buffer, out_values, 10);
    EXPECT_EQ(popped, 10u);
    for (int i = 0; i < 10; i++) {
        EXPECT_FLOAT_EQ(out_values[i], (float)(i + 1));
    }

    circular_buffer_destroy(buffer);
}

/**
 * PushBatch_OverflowErrorReportsCorrectCount
 *
 * Fill a buffer, then attempt to push a batch.
 * With OVERFLOW_ERROR strategy, verify the count reflects only
 * the successful pushes (not inflated by failed attempts).
 */
TEST_F(CircularBufferBatchTest, PushBatch_OverflowErrorReportsCorrectCount) {
    // Create buffer with capacity 5, OVERFLOW_ERROR strategy
    circular_buffer_t* buffer = circular_buffer_create(sizeof(float), 5, OVERFLOW_ERROR);
    ASSERT_NE(buffer, nullptr);

    // Fill the buffer completely
    for (int i = 0; i < 5; i++) {
        float val = (float)i;
        EXPECT_TRUE(circular_buffer_push(buffer, &val));
    }
    EXPECT_TRUE(circular_buffer_is_full(buffer));

    // Now try to push a batch of 5 more - all should fail
    float more_values[5] = {10.0f, 20.0f, 30.0f, 40.0f, 50.0f};
    size_t written = circular_buffer_push_batch(buffer, more_values, 5);

    // With OVERFLOW_ERROR, the first push fails and breaks the loop
    // So written should be 0
    EXPECT_EQ(written, 0u);
    EXPECT_EQ(circular_buffer_size(buffer), 5u);  // Original 5 still there

    circular_buffer_destroy(buffer);
}

/**
 * PushBatch_EmptyBatch
 *
 * Push 0 items. Should return 0 immediately.
 */
TEST_F(CircularBufferBatchTest, PushBatch_EmptyBatch) {
    circular_buffer_t* buffer = circular_buffer_create(sizeof(float), 10, OVERFLOW_ERROR);
    ASSERT_NE(buffer, nullptr);

    float values[1] = {1.0f};
    size_t written = circular_buffer_push_batch(buffer, values, 0);

    EXPECT_EQ(written, 0u);
    EXPECT_EQ(circular_buffer_size(buffer), 0u);

    circular_buffer_destroy(buffer);
}

// ============================================================================
// Pop Batch Tests
// ============================================================================

/**
 * PopBatch_CountMatchesActualPops
 *
 * Pop batch from a partially filled buffer.
 * Request more items than available and verify count reflects actual pops.
 */
TEST_F(CircularBufferBatchTest, PopBatch_CountMatchesActualPops) {
    circular_buffer_t* buffer = circular_buffer_create(sizeof(float), 10, OVERFLOW_ERROR);
    ASSERT_NE(buffer, nullptr);

    // Push only 3 items
    float in_values[3] = {1.0f, 2.0f, 3.0f};
    size_t pushed = circular_buffer_push_batch(buffer, in_values, 3);
    EXPECT_EQ(pushed, 3u);

    // Try to pop 10 items - should only get 3
    float out_values[10] = {0};
    size_t popped = circular_buffer_pop_batch(buffer, out_values, 10);

    EXPECT_EQ(popped, 3u);
    EXPECT_FLOAT_EQ(out_values[0], 1.0f);
    EXPECT_FLOAT_EQ(out_values[1], 2.0f);
    EXPECT_FLOAT_EQ(out_values[2], 3.0f);

    EXPECT_TRUE(circular_buffer_is_empty(buffer));

    circular_buffer_destroy(buffer);
}

/**
 * PopBatch_EmptyBuffer
 *
 * Pop from an empty buffer. Should return 0.
 */
TEST_F(CircularBufferBatchTest, PopBatch_EmptyBuffer) {
    circular_buffer_t* buffer = circular_buffer_create(sizeof(float), 10, OVERFLOW_ERROR);
    ASSERT_NE(buffer, nullptr);

    float out_values[5] = {0};
    size_t popped = circular_buffer_pop_batch(buffer, out_values, 5);

    EXPECT_EQ(popped, 0u);
    EXPECT_TRUE(circular_buffer_is_empty(buffer));

    circular_buffer_destroy(buffer);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
