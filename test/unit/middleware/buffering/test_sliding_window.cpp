//=============================================================================
// test_sliding_window.cpp - Comprehensive Sliding Window Tests
//=============================================================================

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
#include "middleware/buffering/nimcp_sliding_window.h"
#include <cmath>

class SlidingWindowTest : public ::testing::Test {
protected:
    sliding_window_t* window = nullptr;

    void SetUp() override {
        window = sliding_window_create(100, 0);
        ASSERT_NE(window, nullptr);
    }

    void TearDown() override {
        sliding_window_destroy(window);
    }
};

//=============================================================================
// LIFECYCLE TESTS
//=============================================================================

TEST_F(SlidingWindowTest, CreateDestroy) {
    EXPECT_NE(window, nullptr);
    EXPECT_EQ(sliding_window_size(window), 100);
    EXPECT_EQ(sliding_window_count(window), 0);
}

TEST_F(SlidingWindowTest, CreateWithNullReturnsNull) {
    sliding_window_t* null_window = sliding_window_create(0, 0);
    EXPECT_EQ(null_window, nullptr);
}

TEST_F(SlidingWindowTest, CreateWithOverlap) {
    sliding_window_t* overlap_window = sliding_window_create(100, 50);
    ASSERT_NE(overlap_window, nullptr);
    EXPECT_EQ(sliding_window_overlap(overlap_window), 50);
    EXPECT_EQ(sliding_window_stride(overlap_window), 50);
    sliding_window_destroy(overlap_window);
}

TEST_F(SlidingWindowTest, CreateWithInvalidOverlap) {
    sliding_window_t* invalid = sliding_window_create(100, 100);
    EXPECT_EQ(invalid, nullptr);
}

TEST_F(SlidingWindowTest, DestroyNullSafe) {
    sliding_window_destroy(nullptr);  // Should not crash
}

//=============================================================================
// DATA OPERATIONS TESTS
//=============================================================================

TEST_F(SlidingWindowTest, AddSingleValue) {
    EXPECT_TRUE(sliding_window_add(window, 42.0f));
    EXPECT_EQ(sliding_window_count(window), 1);
    EXPECT_FLOAT_EQ(sliding_window_mean(window), 42.0f);
}

TEST_F(SlidingWindowTest, AddMultipleValues) {
    for (int i = 0; i < 50; i++) {
        EXPECT_TRUE(sliding_window_add(window, static_cast<float>(i)));
    }
    EXPECT_EQ(sliding_window_count(window), 50);
}

TEST_F(SlidingWindowTest, AddUntilFull) {
    for (int i = 0; i < 100; i++) {
        sliding_window_add(window, static_cast<float>(i));
    }
    EXPECT_TRUE(sliding_window_is_full(window));
    EXPECT_EQ(sliding_window_count(window), 100);
}

TEST_F(SlidingWindowTest, AddBeyondCapacity) {
    for (int i = 0; i < 150; i++) {
        sliding_window_add(window, static_cast<float>(i));
    }
    EXPECT_TRUE(sliding_window_is_full(window));
    EXPECT_EQ(sliding_window_count(window), 100);
}

TEST_F(SlidingWindowTest, AddNullWindow) {
    EXPECT_FALSE(sliding_window_add(nullptr, 42.0f));
}

TEST_F(SlidingWindowTest, AddBatch) {
    float values[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    size_t added = sliding_window_add_batch(window, values, 10);
    EXPECT_EQ(added, 10);
    EXPECT_EQ(sliding_window_count(window), 10);
}

TEST_F(SlidingWindowTest, AddBatchNull) {
    EXPECT_EQ(sliding_window_add_batch(nullptr, nullptr, 10), 0);
    EXPECT_EQ(sliding_window_add_batch(window, nullptr, 10), 0);
}

TEST_F(SlidingWindowTest, AddBatchLarge) {
    float values[200];
    for (int i = 0; i < 200; i++) values[i] = static_cast<float>(i);

    size_t added = sliding_window_add_batch(window, values, 200);
    EXPECT_EQ(added, 200);
    EXPECT_TRUE(sliding_window_is_full(window));
}

//=============================================================================
// STATISTICS TESTS
//=============================================================================

TEST_F(SlidingWindowTest, MeanOfSingleValue) {
    sliding_window_add(window, 100.0f);
    EXPECT_FLOAT_EQ(sliding_window_mean(window), 100.0f);
}

TEST_F(SlidingWindowTest, MeanOfMultipleValues) {
    for (int i = 0; i < 10; i++) {
        sliding_window_add(window, static_cast<float>(i));
    }
    EXPECT_NEAR(sliding_window_mean(window), 4.5f, 0.1f);
}

TEST_F(SlidingWindowTest, VarianceOfConstant) {
    for (int i = 0; i < 50; i++) {
        sliding_window_add(window, 42.0f);
    }
    EXPECT_NEAR(sliding_window_variance(window), 0.0f, 0.001f);
}

TEST_F(SlidingWindowTest, VarianceOfSequence) {
    for (int i = 0; i < 100; i++) {
        sliding_window_add(window, static_cast<float>(i));
    }
    EXPECT_GT(sliding_window_variance(window), 0.0f);
}

TEST_F(SlidingWindowTest, StddevComputation) {
    for (int i = 0; i < 100; i++) {
        sliding_window_add(window, static_cast<float>(i));
    }
    float variance = sliding_window_variance(window);
    float stddev = sliding_window_stddev(window);
    EXPECT_NEAR(stddev, sqrtf(variance), 0.001f);
}

TEST_F(SlidingWindowTest, MinMaxTracking) {
    sliding_window_add(window, 10.0f);
    sliding_window_add(window, 5.0f);
    sliding_window_add(window, 15.0f);
    sliding_window_add(window, 3.0f);
    sliding_window_add(window, 20.0f);

    EXPECT_FLOAT_EQ(sliding_window_min(window), 3.0f);
    EXPECT_FLOAT_EQ(sliding_window_max(window), 20.0f);
}

TEST_F(SlidingWindowTest, RangeComputation) {
    sliding_window_add(window, 10.0f);
    sliding_window_add(window, 5.0f);
    sliding_window_add(window, 15.0f);

    float range = sliding_window_range(window);
    EXPECT_FLOAT_EQ(range, 10.0f);  // 15 - 5
}

TEST_F(SlidingWindowTest, GetStats) {
    for (int i = 0; i < 10; i++) {
        sliding_window_add(window, static_cast<float>(i * 10));
    }

    window_stats_t stats;
    EXPECT_TRUE(sliding_window_get_stats(window, &stats));

    EXPECT_EQ(stats.count, 10);
    EXPECT_NEAR(stats.mean, 45.0f, 0.1f);
    EXPECT_GT(stats.variance, 0.0f);
    EXPECT_FLOAT_EQ(stats.min, 0.0f);
    EXPECT_FLOAT_EQ(stats.max, 90.0f);
}

TEST_F(SlidingWindowTest, GetStatsNull) {
    EXPECT_FALSE(sliding_window_get_stats(nullptr, nullptr));
    window_stats_t stats;
    EXPECT_FALSE(sliding_window_get_stats(nullptr, &stats));
}

//=============================================================================
// SAMPLE RETRIEVAL TESTS
//=============================================================================

TEST_F(SlidingWindowTest, GetSamples) {
    for (int i = 0; i < 10; i++) {
        sliding_window_add(window, static_cast<float>(i));
    }

    float samples[10];
    size_t retrieved = sliding_window_get_samples(window, samples, 10);

    EXPECT_EQ(retrieved, 10);
    for (int i = 0; i < 10; i++) {
        EXPECT_FLOAT_EQ(samples[i], static_cast<float>(i));
    }
}

TEST_F(SlidingWindowTest, GetSamplesPartial) {
    for (int i = 0; i < 50; i++) {
        sliding_window_add(window, static_cast<float>(i));
    }

    float samples[25];
    size_t retrieved = sliding_window_get_samples(window, samples, 25);

    EXPECT_EQ(retrieved, 25);
}

TEST_F(SlidingWindowTest, GetSamplesNull) {
    EXPECT_EQ(sliding_window_get_samples(nullptr, nullptr, 10), 0);
}

//=============================================================================
// QUERY TESTS
//=============================================================================

TEST_F(SlidingWindowTest, SizeQuery) {
    EXPECT_EQ(sliding_window_size(window), 100);
}

TEST_F(SlidingWindowTest, CountQuery) {
    EXPECT_EQ(sliding_window_count(window), 0);
    sliding_window_add(window, 1.0f);
    EXPECT_EQ(sliding_window_count(window), 1);
}

TEST_F(SlidingWindowTest, IsFullEmpty) {
    EXPECT_FALSE(sliding_window_is_full(window));
    for (int i = 0; i < 100; i++) {
        sliding_window_add(window, 1.0f);
    }
    EXPECT_TRUE(sliding_window_is_full(window));
}

TEST_F(SlidingWindowTest, OverlapQuery) {
    sliding_window_t* win50 = sliding_window_create(100, 50);
    EXPECT_EQ(sliding_window_overlap(win50), 50);
    sliding_window_destroy(win50);
}

TEST_F(SlidingWindowTest, StrideComputation) {
    sliding_window_t* win75 = sliding_window_create(100, 75);
    EXPECT_EQ(sliding_window_stride(win75), 25);
    sliding_window_destroy(win75);
}

TEST_F(SlidingWindowTest, QueriesOnNull) {
    EXPECT_EQ(sliding_window_size(nullptr), 0);
    EXPECT_EQ(sliding_window_count(nullptr), 0);
    EXPECT_FALSE(sliding_window_is_full(nullptr));
    EXPECT_EQ(sliding_window_overlap(nullptr), 0);
    EXPECT_EQ(sliding_window_stride(nullptr), 0);
}

//=============================================================================
// MANAGEMENT TESTS
//=============================================================================

TEST_F(SlidingWindowTest, Clear) {
    for (int i = 0; i < 50; i++) {
        sliding_window_add(window, static_cast<float>(i));
    }
    EXPECT_EQ(sliding_window_count(window), 50);

    sliding_window_clear(window);
    EXPECT_EQ(sliding_window_count(window), 0);
}

TEST_F(SlidingWindowTest, ClearNull) {
    sliding_window_clear(nullptr);  // Should not crash
}

TEST_F(SlidingWindowTest, ResetStats) {
    for (int i = 0; i < 100; i++) {
        sliding_window_add(window, static_cast<float>(i));
    }

    sliding_window_reset_stats(window);

    // Stats should be recalculated from existing samples
    window_stats_t stats;
    EXPECT_TRUE(sliding_window_get_stats(window, &stats));
    EXPECT_GT(stats.variance, 0.0f);
}

TEST_F(SlidingWindowTest, ResetStatsNull) {
    sliding_window_reset_stats(nullptr);  // Should not crash
}

//=============================================================================
// EDGE CASE TESTS
//=============================================================================

TEST_F(SlidingWindowTest, SinusoidalSignal) {
    for (int i = 0; i < 100; i++) {
        float value = sinf(i * 0.1f);
        sliding_window_add(window, value);
    }

    EXPECT_TRUE(sliding_window_is_full(window));
    EXPECT_NEAR(sliding_window_mean(window), 0.0f, 0.5f);  // Sin mean near 0
}

TEST_F(SlidingWindowTest, ExtremeValues) {
    sliding_window_add(window, 1e6f);
    sliding_window_add(window, -1e6f);
    sliding_window_add(window, 0.0f);

    EXPECT_FLOAT_EQ(sliding_window_min(window), -1e6f);
    EXPECT_FLOAT_EQ(sliding_window_max(window), 1e6f);
}

TEST_F(SlidingWindowTest, NegativeValues) {
    for (int i = 0; i < 10; i++) {
        sliding_window_add(window, static_cast<float>(-i));
    }

    EXPECT_LT(sliding_window_mean(window), 0.0f);
}

TEST_F(SlidingWindowTest, ZeroValues) {
    for (int i = 0; i < 50; i++) {
        sliding_window_add(window, 0.0f);
    }

    EXPECT_FLOAT_EQ(sliding_window_mean(window), 0.0f);
    EXPECT_FLOAT_EQ(sliding_window_variance(window), 0.0f);
    EXPECT_FLOAT_EQ(sliding_window_min(window), 0.0f);
    EXPECT_FLOAT_EQ(sliding_window_max(window), 0.0f);
}

TEST_F(SlidingWindowTest, SmallWindow) {
    sliding_window_t* small = sliding_window_create(5, 0);
    ASSERT_NE(small, nullptr);

    for (int i = 0; i < 10; i++) {
        sliding_window_add(small, static_cast<float>(i));
    }

    EXPECT_TRUE(sliding_window_is_full(small));
    EXPECT_EQ(sliding_window_count(small), 5);

    // Should contain last 5 values: 5, 6, 7, 8, 9
    EXPECT_NEAR(sliding_window_mean(small), 7.0f, 0.1f);

    sliding_window_destroy(small);
}

TEST_F(SlidingWindowTest, WrapAround) {
    sliding_window_t* wrap = sliding_window_create(10, 0);

    for (int i = 0; i < 25; i++) {
        sliding_window_add(wrap, static_cast<float>(i));
    }

    EXPECT_TRUE(sliding_window_is_full(wrap));

    // Should contain values 15-24
    float samples[10];
    sliding_window_get_samples(wrap, samples, 10);

    EXPECT_FLOAT_EQ(samples[0], 15.0f);
    EXPECT_FLOAT_EQ(samples[9], 24.0f);

    sliding_window_destroy(wrap);
}

//=============================================================================
// STRESS TESTS
//=============================================================================

TEST_F(SlidingWindowTest, LargeNumberOfUpdates) {
    const int iterations = 10000;
    for (int i = 0; i < iterations; i++) {
        sliding_window_add(window, sinf(i * 0.01f));
    }

    EXPECT_TRUE(sliding_window_is_full(window));
    EXPECT_EQ(sliding_window_count(window), 100);
}

TEST_F(SlidingWindowTest, BatchVsIndividual) {
    float values[100];
    for (int i = 0; i < 100; i++) values[i] = static_cast<float>(i);

    // Window 1: batch add
    sliding_window_t* win1 = sliding_window_create(100, 0);
    sliding_window_add_batch(win1, values, 100);

    // Window 2: individual adds
    sliding_window_t* win2 = sliding_window_create(100, 0);
    for (int i = 0; i < 100; i++) {
        sliding_window_add(win2, values[i]);
    }

    // Should produce identical results
    EXPECT_NEAR(sliding_window_mean(win1), sliding_window_mean(win2), 0.001f);
    EXPECT_NEAR(sliding_window_variance(win1), sliding_window_variance(win2), 0.001f);

    sliding_window_destroy(win1);
    sliding_window_destroy(win2);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
