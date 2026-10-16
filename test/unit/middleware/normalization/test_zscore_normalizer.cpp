//=============================================================================
// test_zscore_normalizer.cpp - Comprehensive Z-Score Normalizer Tests
//=============================================================================

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

// Headers have their own extern "C" guards
#include "middleware/normalization/nimcp_zscore_normalizer.h"

/**
 * WHAT: Comprehensive test suite for z-score normalization
 * WHY:  Ensure statistical correctness, Welford's algorithm, windowing
 * HOW:  Unit tests for all 15 functions, integration tests, regression tests
 */

class ZScoreNormalizerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Test fixture setup
    }

    void TearDown() override {
        // Cleanup
    }

    // Helper: Check if two floats are approximately equal
    bool FloatEquals(float a, float b, float epsilon = 0.001f) {
        return std::fabs(a - b) < epsilon;
    }
};

//=============================================================================
// LIFECYCLE TESTS
//=============================================================================
// WHAT: Test normalizer creation and destruction
// WHY:  Verify resource management and parameter validation
// HOW:  Test all parameter combinations and edge cases

TEST_F(ZScoreNormalizerTest, Create_Success_SingleChannel) {
    zscore_normalizer_t* norm = zscore_normalizer_create(1, 0, 0.0f);
    ASSERT_NE(norm, nullptr);
    EXPECT_EQ(zscore_normalizer_num_channels(norm), 1);
    zscore_normalizer_destroy(norm);
}

TEST_F(ZScoreNormalizerTest, Create_Success_MultipleChannels) {
    zscore_normalizer_t* norm = zscore_normalizer_create(10, 0, 0.0f);
    ASSERT_NE(norm, nullptr);
    EXPECT_EQ(zscore_normalizer_num_channels(norm), 10);
    zscore_normalizer_destroy(norm);
}

TEST_F(ZScoreNormalizerTest, Create_Success_WithWindow) {
    zscore_normalizer_t* norm = zscore_normalizer_create(5, 100, 0.0f);
    ASSERT_NE(norm, nullptr);
    EXPECT_EQ(zscore_normalizer_num_channels(norm), 5);
    zscore_normalizer_destroy(norm);
}

TEST_F(ZScoreNormalizerTest, Create_Success_WithOutlierClip) {
    zscore_normalizer_t* norm = zscore_normalizer_create(3, 0, 3.0f);
    ASSERT_NE(norm, nullptr);
    zscore_normalizer_destroy(norm);
}

TEST_F(ZScoreNormalizerTest, Create_Failure_ZeroChannels) {
    zscore_normalizer_t* norm = zscore_normalizer_create(0, 0, 0.0f);
    EXPECT_EQ(norm, nullptr);
}

TEST_F(ZScoreNormalizerTest, Destroy_NullSafe) {
    zscore_normalizer_destroy(nullptr);
    // Should not crash
}

TEST_F(ZScoreNormalizerTest, Destroy_Success) {
    zscore_normalizer_t* norm = zscore_normalizer_create(5, 0, 0.0f);
    ASSERT_NE(norm, nullptr);
    zscore_normalizer_destroy(norm);
    // Should not crash or leak
}

//=============================================================================
// FIT OPERATION TESTS (Welford's Algorithm)
//=============================================================================
// WHAT: Test online mean/variance calculation
// WHY:  Verify Welford's algorithm correctness
// HOW:  Fit known sequences, verify statistics

TEST_F(ZScoreNormalizerTest, Fit_Success_SingleValue) {
    zscore_normalizer_t* norm = zscore_normalizer_create(1, 0, 0.0f);
    ASSERT_NE(norm, nullptr);

    bool result = zscore_normalizer_fit(norm, 0, 5.0f);
    EXPECT_TRUE(result);

    float mean = zscore_normalizer_mean(norm, 0);
    EXPECT_FLOAT_EQ(mean, 5.0f);

    zscore_normalizer_destroy(norm);
}

TEST_F(ZScoreNormalizerTest, Fit_Success_TwoValues_MeanCorrect) {
    zscore_normalizer_t* norm = zscore_normalizer_create(1, 0, 0.0f);
    ASSERT_NE(norm, nullptr);

    zscore_normalizer_fit(norm, 0, 2.0f);
    zscore_normalizer_fit(norm, 0, 8.0f);

    float mean = zscore_normalizer_mean(norm, 0);
    EXPECT_FLOAT_EQ(mean, 5.0f);  // (2 + 8) / 2 = 5

    zscore_normalizer_destroy(norm);
}

TEST_F(ZScoreNormalizerTest, Fit_Success_MultipleValues) {
    zscore_normalizer_t* norm = zscore_normalizer_create(1, 0, 0.0f);
    ASSERT_NE(norm, nullptr);

    // Sequence: 1, 2, 3, 4, 5
    for (int i = 1; i <= 5; i++) {
        zscore_normalizer_fit(norm, 0, (float)i);
    }

    float mean = zscore_normalizer_mean(norm, 0);
    EXPECT_FLOAT_EQ(mean, 3.0f);  // (1+2+3+4+5)/5 = 3

    zscore_normalizer_destroy(norm);
}

TEST_F(ZScoreNormalizerTest, Fit_Success_Variance_KnownSequence) {
    zscore_normalizer_t* norm = zscore_normalizer_create(1, 0, 0.0f);
    ASSERT_NE(norm, nullptr);

    // Sequence: 2, 4, 4, 4, 5, 5, 7, 9
    // Mean = 5, Sample Variance (n-1) = 4.571, Sample Stddev = 2.138
    std::vector<float> values = {2.0f, 4.0f, 4.0f, 4.0f, 5.0f, 5.0f, 7.0f, 9.0f};
    for (float v : values) {
        zscore_normalizer_fit(norm, 0, v);
    }

    zscore_stats_t stats;
    zscore_normalizer_get_stats(norm, 0, &stats);

    EXPECT_FLOAT_EQ(stats.mean, 5.0f);
    EXPECT_TRUE(FloatEquals(stats.variance, 4.571f, 0.01f));
    EXPECT_TRUE(FloatEquals(stats.stddev, 2.138f, 0.01f));

    zscore_normalizer_destroy(norm);
}

TEST_F(ZScoreNormalizerTest, Fit_Success_MinMax_Tracked) {
    zscore_normalizer_t* norm = zscore_normalizer_create(1, 0, 0.0f);
    ASSERT_NE(norm, nullptr);

    zscore_normalizer_fit(norm, 0, 5.0f);
    zscore_normalizer_fit(norm, 0, 2.0f);
    zscore_normalizer_fit(norm, 0, 8.0f);
    zscore_normalizer_fit(norm, 0, 3.0f);

    zscore_stats_t stats;
    zscore_normalizer_get_stats(norm, 0, &stats);

    EXPECT_FLOAT_EQ(stats.min_value, 2.0f);
    EXPECT_FLOAT_EQ(stats.max_value, 8.0f);

    zscore_normalizer_destroy(norm);
}

TEST_F(ZScoreNormalizerTest, Fit_Failure_NullNormalizer) {
    bool result = zscore_normalizer_fit(nullptr, 0, 5.0f);
    EXPECT_FALSE(result);
}

TEST_F(ZScoreNormalizerTest, Fit_Failure_InvalidChannel) {
    zscore_normalizer_t* norm = zscore_normalizer_create(3, 0, 0.0f);
    ASSERT_NE(norm, nullptr);

    bool result = zscore_normalizer_fit(norm, 5, 5.0f);
    EXPECT_FALSE(result);

    zscore_normalizer_destroy(norm);
}

TEST_F(ZScoreNormalizerTest, Fit_Success_MultiChannel_Independent) {
    zscore_normalizer_t* norm = zscore_normalizer_create(3, 0, 0.0f);
    ASSERT_NE(norm, nullptr);

    zscore_normalizer_fit(norm, 0, 1.0f);
    zscore_normalizer_fit(norm, 1, 10.0f);
    zscore_normalizer_fit(norm, 2, 100.0f);

    EXPECT_FLOAT_EQ(zscore_normalizer_mean(norm, 0), 1.0f);
    EXPECT_FLOAT_EQ(zscore_normalizer_mean(norm, 1), 10.0f);
    EXPECT_FLOAT_EQ(zscore_normalizer_mean(norm, 2), 100.0f);

    zscore_normalizer_destroy(norm);
}

//=============================================================================
// TRANSFORM TESTS
//=============================================================================
// WHAT: Test z-score transformation
// WHY:  Verify normalization formula: z = (x - mean) / stddev
// HOW:  Test known transformations, verify properties

TEST_F(ZScoreNormalizerTest, Transform_NoData_ReturnsZero) {
    zscore_normalizer_t* norm = zscore_normalizer_create(1, 0, 0.0f);
    ASSERT_NE(norm, nullptr);

    float z = zscore_normalizer_transform(norm, 0, 5.0f);
    EXPECT_FLOAT_EQ(z, 0.0f);

    zscore_normalizer_destroy(norm);
}

TEST_F(ZScoreNormalizerTest, Transform_Success_AtMean_ReturnsZero) {
    zscore_normalizer_t* norm = zscore_normalizer_create(1, 0, 0.0f);
    ASSERT_NE(norm, nullptr);

    // Fit mean = 5.0
    for (int i = 1; i <= 9; i++) {
        zscore_normalizer_fit(norm, 0, (float)i);
    }

    float z = zscore_normalizer_transform(norm, 0, 5.0f);
    EXPECT_TRUE(FloatEquals(z, 0.0f, 0.1f));

    zscore_normalizer_destroy(norm);
}

TEST_F(ZScoreNormalizerTest, Transform_Success_OneStddevAbove) {
    zscore_normalizer_t* norm = zscore_normalizer_create(1, 0, 0.0f);
    ASSERT_NE(norm, nullptr);

    // Mean = 0, Stddev = 1
    zscore_normalizer_fit(norm, 0, -1.0f);
    zscore_normalizer_fit(norm, 0, 0.0f);
    zscore_normalizer_fit(norm, 0, 1.0f);

    float z = zscore_normalizer_transform(norm, 0, 1.0f);
    EXPECT_TRUE(FloatEquals(z, 1.0f, 0.2f));

    zscore_normalizer_destroy(norm);
}

TEST_F(ZScoreNormalizerTest, Transform_Success_NegativeZScore) {
    zscore_normalizer_t* norm = zscore_normalizer_create(1, 0, 0.0f);
    ASSERT_NE(norm, nullptr);

    // Mean = 5.0
    for (int i = 1; i <= 9; i++) {
        zscore_normalizer_fit(norm, 0, (float)i);
    }

    float z = zscore_normalizer_transform(norm, 0, 1.0f);
    EXPECT_LT(z, 0.0f);  // Below mean should be negative

    zscore_normalizer_destroy(norm);
}

TEST_F(ZScoreNormalizerTest, Transform_WithClipping) {
    zscore_normalizer_t* norm = zscore_normalizer_create(1, 0, 3.0f);
    ASSERT_NE(norm, nullptr);

    // Mean = 0, Stddev ≈ 1
    for (int i = -2; i <= 2; i++) {
        zscore_normalizer_fit(norm, 0, (float)i);
    }

    // Value far above mean (should clip to +3.0)
    float z = zscore_normalizer_transform(norm, 0, 100.0f);
    EXPECT_FLOAT_EQ(z, 3.0f);

    // Value far below mean (should clip to -3.0)
    z = zscore_normalizer_transform(norm, 0, -100.0f);
    EXPECT_FLOAT_EQ(z, -3.0f);

    zscore_normalizer_destroy(norm);
}

TEST_F(ZScoreNormalizerTest, Transform_Failure_NullNormalizer) {
    float z = zscore_normalizer_transform(nullptr, 0, 5.0f);
    EXPECT_FLOAT_EQ(z, 5.0f);  // Returns input unchanged
}

TEST_F(ZScoreNormalizerTest, Transform_Failure_InvalidChannel) {
    zscore_normalizer_t* norm = zscore_normalizer_create(2, 0, 0.0f);
    ASSERT_NE(norm, nullptr);

    float z = zscore_normalizer_transform(norm, 5, 5.0f);
    EXPECT_FLOAT_EQ(z, 5.0f);  // Returns input unchanged

    zscore_normalizer_destroy(norm);
}

//=============================================================================
// FIT_TRANSFORM & INVERSE TESTS
//=============================================================================
// WHAT: Test combined operations
// WHY:  Verify efficient online normalization and reconstruction
// HOW:  Test fit_transform and inverse_transform

TEST_F(ZScoreNormalizerTest, FitTransform_Success) {
    zscore_normalizer_t* norm = zscore_normalizer_create(1, 0, 0.0f);
    ASSERT_NE(norm, nullptr);

    float z1 = zscore_normalizer_fit_transform(norm, 0, 5.0f);
    float z2 = zscore_normalizer_fit_transform(norm, 0, 7.0f);
    float z3 = zscore_normalizer_fit_transform(norm, 0, 3.0f);

    // After 3 samples, mean = 5.0
    float mean = zscore_normalizer_mean(norm, 0);
    EXPECT_FLOAT_EQ(mean, 5.0f);

    zscore_normalizer_destroy(norm);
}

TEST_F(ZScoreNormalizerTest, InverseTransform_Success_RoundTrip) {
    zscore_normalizer_t* norm = zscore_normalizer_create(1, 0, 0.0f);
    ASSERT_NE(norm, nullptr);

    // Fit some data
    for (int i = 0; i < 10; i++) {
        zscore_normalizer_fit(norm, 0, (float)i);
    }

    // Transform and inverse transform
    float original = 5.5f;
    float z = zscore_normalizer_transform(norm, 0, original);
    float reconstructed = zscore_normalizer_inverse_transform(norm, 0, z);

    EXPECT_TRUE(FloatEquals(original, reconstructed, 0.01f));

    zscore_normalizer_destroy(norm);
}

TEST_F(ZScoreNormalizerTest, InverseTransform_Failure_NullNormalizer) {
    float result = zscore_normalizer_inverse_transform(nullptr, 0, 2.0f);
    EXPECT_FLOAT_EQ(result, 2.0f);  // Returns input unchanged
}

TEST_F(ZScoreNormalizerTest, InverseTransform_Failure_InvalidChannel) {
    zscore_normalizer_t* norm = zscore_normalizer_create(2, 0, 0.0f);
    ASSERT_NE(norm, nullptr);

    float result = zscore_normalizer_inverse_transform(norm, 5, 2.0f);
    EXPECT_FLOAT_EQ(result, 2.0f);  // Returns input unchanged

    zscore_normalizer_destroy(norm);
}

//=============================================================================
// BATCH OPERATION TESTS
//=============================================================================
// WHAT: Test batch fit and transform operations
// WHY:  Verify efficient batch processing
// HOW:  Test arrays of values

TEST_F(ZScoreNormalizerTest, FitBatch_Success) {
    zscore_normalizer_t* norm = zscore_normalizer_create(1, 0, 0.0f);
    ASSERT_NE(norm, nullptr);

    float values[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    size_t count = zscore_normalizer_fit_batch(norm, 0, values, 5);

    EXPECT_EQ(count, 5);

    float mean = zscore_normalizer_mean(norm, 0);
    EXPECT_FLOAT_EQ(mean, 3.0f);

    zscore_normalizer_destroy(norm);
}

TEST_F(ZScoreNormalizerTest, FitBatch_Failure_NullParams) {
    zscore_normalizer_t* norm = zscore_normalizer_create(1, 0, 0.0f);
    float values[] = {1.0f, 2.0f};

    EXPECT_EQ(zscore_normalizer_fit_batch(nullptr, 0, values, 2), 0);
    EXPECT_EQ(zscore_normalizer_fit_batch(norm, 0, nullptr, 5), 0);
    EXPECT_EQ(zscore_normalizer_fit_batch(norm, 0, values, 0), 0);

    zscore_normalizer_destroy(norm);
}

TEST_F(ZScoreNormalizerTest, TransformBatch_Success) {
    zscore_normalizer_t* norm = zscore_normalizer_create(1, 0, 0.0f);
    ASSERT_NE(norm, nullptr);

    // Fit data: mean = 3.0
    for (int i = 1; i <= 5; i++) {
        zscore_normalizer_fit(norm, 0, (float)i);
    }

    float inputs[] = {1.0f, 3.0f, 5.0f};
    float outputs[3];

    size_t count = zscore_normalizer_transform_batch(norm, 0, inputs, outputs, 3);
    EXPECT_EQ(count, 3);

    // Mean value should transform to ~0.0
    EXPECT_TRUE(FloatEquals(outputs[1], 0.0f, 0.2f));

    zscore_normalizer_destroy(norm);
}

TEST_F(ZScoreNormalizerTest, TransformBatch_Failure_NullParams) {
    zscore_normalizer_t* norm = zscore_normalizer_create(1, 0, 0.0f);
    float values[] = {1.0f, 2.0f};
    float outputs[2];

    EXPECT_EQ(zscore_normalizer_transform_batch(nullptr, 0, values, outputs, 2), 0);
    EXPECT_EQ(zscore_normalizer_transform_batch(norm, 0, nullptr, outputs, 2), 0);
    EXPECT_EQ(zscore_normalizer_transform_batch(norm, 0, values, nullptr, 2), 0);
    EXPECT_EQ(zscore_normalizer_transform_batch(norm, 0, values, outputs, 0), 0);

    zscore_normalizer_destroy(norm);
}

//=============================================================================
// STATISTICS QUERY TESTS
//=============================================================================
// WHAT: Test statistics retrieval functions
// WHY:  Verify stats tracking and query operations
// HOW:  Test get_stats, mean, stddev, num_channels

TEST_F(ZScoreNormalizerTest, GetStats_Success) {
    zscore_normalizer_t* norm = zscore_normalizer_create(1, 0, 0.0f);
    ASSERT_NE(norm, nullptr);

    for (int i = 1; i <= 5; i++) {
        zscore_normalizer_fit(norm, 0, (float)i);
    }

    zscore_stats_t stats;
    bool result = zscore_normalizer_get_stats(norm, 0, &stats);

    EXPECT_TRUE(result);
    EXPECT_FLOAT_EQ(stats.mean, 3.0f);
    EXPECT_GT(stats.variance, 0.0f);
    EXPECT_GT(stats.stddev, 0.0f);
    EXPECT_EQ(stats.sample_count, 5);
    EXPECT_FLOAT_EQ(stats.min_value, 1.0f);
    EXPECT_FLOAT_EQ(stats.max_value, 5.0f);

    zscore_normalizer_destroy(norm);
}

TEST_F(ZScoreNormalizerTest, GetStats_Failure_NullParams) {
    zscore_normalizer_t* norm = zscore_normalizer_create(1, 0, 0.0f);
    zscore_stats_t stats;

    EXPECT_FALSE(zscore_normalizer_get_stats(nullptr, 0, &stats));
    EXPECT_FALSE(zscore_normalizer_get_stats(norm, 0, nullptr));
    EXPECT_FALSE(zscore_normalizer_get_stats(norm, 5, &stats));

    zscore_normalizer_destroy(norm);
}

TEST_F(ZScoreNormalizerTest, Mean_Success) {
    zscore_normalizer_t* norm = zscore_normalizer_create(1, 0, 0.0f);
    ASSERT_NE(norm, nullptr);

    zscore_normalizer_fit(norm, 0, 10.0f);
    zscore_normalizer_fit(norm, 0, 20.0f);

    float mean = zscore_normalizer_mean(norm, 0);
    EXPECT_FLOAT_EQ(mean, 15.0f);

    zscore_normalizer_destroy(norm);
}

TEST_F(ZScoreNormalizerTest, Mean_Failure) {
    EXPECT_FLOAT_EQ(zscore_normalizer_mean(nullptr, 0), 0.0f);

    zscore_normalizer_t* norm = zscore_normalizer_create(2, 0, 0.0f);
    EXPECT_FLOAT_EQ(zscore_normalizer_mean(norm, 5), 0.0f);
    zscore_normalizer_destroy(norm);
}

TEST_F(ZScoreNormalizerTest, Stddev_Success) {
    zscore_normalizer_t* norm = zscore_normalizer_create(1, 0, 0.0f);
    ASSERT_NE(norm, nullptr);

    // Sample Variance = 4.571, Sample Stddev = 2.138
    std::vector<float> values = {2.0f, 4.0f, 4.0f, 4.0f, 5.0f, 5.0f, 7.0f, 9.0f};
    for (float v : values) {
        zscore_normalizer_fit(norm, 0, v);
    }

    float stddev = zscore_normalizer_stddev(norm, 0);
    EXPECT_TRUE(FloatEquals(stddev, 2.138f, 0.01f));

    zscore_normalizer_destroy(norm);
}

TEST_F(ZScoreNormalizerTest, Stddev_Failure) {
    EXPECT_FLOAT_EQ(zscore_normalizer_stddev(nullptr, 0), 1.0f);

    zscore_normalizer_t* norm = zscore_normalizer_create(2, 0, 0.0f);
    EXPECT_FLOAT_EQ(zscore_normalizer_stddev(norm, 5), 1.0f);
    zscore_normalizer_destroy(norm);
}

TEST_F(ZScoreNormalizerTest, NumChannels_Success) {
    zscore_normalizer_t* norm = zscore_normalizer_create(7, 0, 0.0f);
    ASSERT_NE(norm, nullptr);

    EXPECT_EQ(zscore_normalizer_num_channels(norm), 7);

    zscore_normalizer_destroy(norm);
}

TEST_F(ZScoreNormalizerTest, NumChannels_Null) {
    EXPECT_EQ(zscore_normalizer_num_channels(nullptr), 0);
}

//=============================================================================
// RESET TESTS
//=============================================================================
// WHAT: Test channel reset functionality
// WHY:  Verify statistics can be cleared
// HOW:  Fit data, reset, verify clean state

TEST_F(ZScoreNormalizerTest, ResetChannel_Success) {
    zscore_normalizer_t* norm = zscore_normalizer_create(3, 0, 0.0f);
    ASSERT_NE(norm, nullptr);

    // Fit channel 1
    for (int i = 1; i <= 10; i++) {
        zscore_normalizer_fit(norm, 1, (float)i);
    }

    EXPECT_FLOAT_EQ(zscore_normalizer_mean(norm, 1), 5.5f);

    // Reset channel 1
    bool result = zscore_normalizer_reset_channel(norm, 1);
    EXPECT_TRUE(result);

    // Verify reset
    EXPECT_FLOAT_EQ(zscore_normalizer_mean(norm, 1), 0.0f);
    EXPECT_FLOAT_EQ(zscore_normalizer_stddev(norm, 1), 1.0f);

    zscore_stats_t stats;
    zscore_normalizer_get_stats(norm, 1, &stats);
    EXPECT_EQ(stats.sample_count, 0);

    zscore_normalizer_destroy(norm);
}

TEST_F(ZScoreNormalizerTest, ResetChannel_Failure) {
    zscore_normalizer_t* norm = zscore_normalizer_create(2, 0, 0.0f);

    EXPECT_FALSE(zscore_normalizer_reset_channel(nullptr, 0));
    EXPECT_FALSE(zscore_normalizer_reset_channel(norm, 5));

    zscore_normalizer_destroy(norm);
}

TEST_F(ZScoreNormalizerTest, ResetAll_Success) {
    zscore_normalizer_t* norm = zscore_normalizer_create(3, 0, 0.0f);
    ASSERT_NE(norm, nullptr);

    // Fit all channels
    for (size_t ch = 0; ch < 3; ch++) {
        for (int i = 1; i <= 5; i++) {
            zscore_normalizer_fit(norm, ch, (float)i);
        }
    }

    // Reset all
    zscore_normalizer_reset_all(norm);

    // Verify all channels reset
    for (size_t ch = 0; ch < 3; ch++) {
        EXPECT_FLOAT_EQ(zscore_normalizer_mean(norm, ch), 0.0f);
        zscore_stats_t stats;
        zscore_normalizer_get_stats(norm, ch, &stats);
        EXPECT_EQ(stats.sample_count, 0);
    }

    zscore_normalizer_destroy(norm);
}

TEST_F(ZScoreNormalizerTest, ResetAll_Null) {
    zscore_normalizer_reset_all(nullptr);
    // Should not crash
}

//=============================================================================
// WINDOWED STATISTICS TESTS
//=============================================================================
// WHAT: Test windowed statistics tracking
// WHY:  Verify sliding window correctly tracks recent statistics
// HOW:  Fit data exceeding window size, verify old data dropped

TEST_F(ZScoreNormalizerTest, Windowed_TracksRecentData) {
    // Window size = 5
    zscore_normalizer_t* norm = zscore_normalizer_create(1, 5, 0.0f);
    ASSERT_NE(norm, nullptr);

    // Fit 10 values (window holds last 5)
    for (int i = 1; i <= 10; i++) {
        zscore_normalizer_fit(norm, 0, (float)i);
    }

    // Mean should be of last 5: (6+7+8+9+10)/5 = 8.0
    float mean = zscore_normalizer_mean(norm, 0);
    EXPECT_FLOAT_EQ(mean, 8.0f);

    zscore_stats_t stats;
    zscore_normalizer_get_stats(norm, 0, &stats);
    EXPECT_EQ(stats.sample_count, 5);

    zscore_normalizer_destroy(norm);
}

TEST_F(ZScoreNormalizerTest, Windowed_MinMaxCorrect) {
    zscore_normalizer_t* norm = zscore_normalizer_create(1, 3, 0.0f);
    ASSERT_NE(norm, nullptr);

    // Initial values
    zscore_normalizer_fit(norm, 0, 1.0f);
    zscore_normalizer_fit(norm, 0, 5.0f);
    zscore_normalizer_fit(norm, 0, 3.0f);

    zscore_stats_t stats;
    zscore_normalizer_get_stats(norm, 0, &stats);
    EXPECT_FLOAT_EQ(stats.min_value, 1.0f);
    EXPECT_FLOAT_EQ(stats.max_value, 5.0f);

    // Push out the min value (1.0)
    zscore_normalizer_fit(norm, 0, 4.0f);  // Window now: 5, 3, 4

    zscore_normalizer_get_stats(norm, 0, &stats);
    EXPECT_FLOAT_EQ(stats.min_value, 3.0f);
    EXPECT_FLOAT_EQ(stats.max_value, 5.0f);

    zscore_normalizer_destroy(norm);
}

//=============================================================================
// REGRESSION TESTS
//=============================================================================
// WHAT: Test known edge cases and bugs
// WHY:  Prevent regressions
// HOW:  Test problematic scenarios

TEST_F(ZScoreNormalizerTest, Regression_ZeroDivision) {
    zscore_normalizer_t* norm = zscore_normalizer_create(1, 0, 0.0f);
    ASSERT_NE(norm, nullptr);

    zscore_normalizer_fit(norm, 0, 5.0f);

    // Stddev should be minimum value to prevent division by zero
    float stddev = zscore_normalizer_stddev(norm, 0);
    EXPECT_GT(stddev, 0.0f);

    // Transform should not crash
    float z = zscore_normalizer_transform(norm, 0, 5.0f);
    EXPECT_FALSE(std::isnan(z));
    EXPECT_FALSE(std::isinf(z));

    zscore_normalizer_destroy(norm);
}

TEST_F(ZScoreNormalizerTest, Regression_ConstantValues) {
    zscore_normalizer_t* norm = zscore_normalizer_create(1, 0, 0.0f);
    ASSERT_NE(norm, nullptr);

    // All same value
    for (int i = 0; i < 10; i++) {
        zscore_normalizer_fit(norm, 0, 5.0f);
    }

    // Variance should be 0, stddev should be minimum
    float stddev = zscore_normalizer_stddev(norm, 0);
    EXPECT_GT(stddev, 0.0f);

    // Transform should work
    float z = zscore_normalizer_transform(norm, 0, 5.0f);
    EXPECT_FALSE(std::isnan(z));

    zscore_normalizer_destroy(norm);
}

TEST_F(ZScoreNormalizerTest, Regression_NegativeValues) {
    zscore_normalizer_t* norm = zscore_normalizer_create(1, 0, 0.0f);
    ASSERT_NE(norm, nullptr);

    zscore_normalizer_fit(norm, 0, -10.0f);
    zscore_normalizer_fit(norm, 0, -5.0f);
    zscore_normalizer_fit(norm, 0, -1.0f);

    float mean = zscore_normalizer_mean(norm, 0);
    EXPECT_LT(mean, 0.0f);

    float z = zscore_normalizer_transform(norm, 0, -5.0f);
    EXPECT_FALSE(std::isnan(z));

    zscore_normalizer_destroy(norm);
}

TEST_F(ZScoreNormalizerTest, Regression_LargeValues) {
    zscore_normalizer_t* norm = zscore_normalizer_create(1, 0, 0.0f);
    ASSERT_NE(norm, nullptr);

    zscore_normalizer_fit(norm, 0, 1e6f);
    zscore_normalizer_fit(norm, 0, 1e7f);
    zscore_normalizer_fit(norm, 0, 1e8f);

    float mean = zscore_normalizer_mean(norm, 0);
    EXPECT_FALSE(std::isnan(mean));
    EXPECT_FALSE(std::isinf(mean));

    zscore_normalizer_destroy(norm);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
