//=============================================================================
// test_min_max_normalizer.cpp - Comprehensive Min-Max Normalizer Tests
//=============================================================================

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "middleware/normalization/nimcp_min_max_normalizer.h"

/**
 * WHAT: Comprehensive test suite for min-max normalization
 * WHY:  Ensure correct scaling to [0,1] or custom ranges
 * HOW:  Unit tests for all 10 functions, edge cases, integration tests
 */

class MinMaxNormalizerTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}

    bool FloatEquals(float a, float b, float eps = 0.001f) {
        return std::fabs(a - b) < eps;
    }
};

//=============================================================================
// LIFECYCLE TESTS
//=============================================================================
// WHAT: Test normalizer creation and destruction
// WHY:  Verify resource management and parameter validation
// HOW:  Test all parameter combinations and edge cases

TEST_F(MinMaxNormalizerTest, Create_Success_Default01Range) {
    min_max_normalizer_t* norm = minmax_normalizer_create(1, 0.0f, 1.0f, false);
    ASSERT_NE(norm, nullptr);
    EXPECT_EQ(minmax_normalizer_num_channels(norm), 1);
    minmax_normalizer_destroy(norm);
}

TEST_F(MinMaxNormalizerTest, Create_Success_CustomRange) {
    min_max_normalizer_t* norm = minmax_normalizer_create(5, -1.0f, 1.0f, false);
    ASSERT_NE(norm, nullptr);
    EXPECT_EQ(minmax_normalizer_num_channels(norm), 5);
    minmax_normalizer_destroy(norm);
}

TEST_F(MinMaxNormalizerTest, Create_Success_WithPercentiles) {
    min_max_normalizer_t* norm = minmax_normalizer_create(3, 0.0f, 1.0f, true);
    ASSERT_NE(norm, nullptr);
    minmax_normalizer_destroy(norm);
}

TEST_F(MinMaxNormalizerTest, Create_Failure_ZeroChannels) {
    min_max_normalizer_t* norm = minmax_normalizer_create(0, 0.0f, 1.0f, false);
    EXPECT_EQ(norm, nullptr);
}

TEST_F(MinMaxNormalizerTest, Create_Failure_InvalidRange) {
    min_max_normalizer_t* norm = minmax_normalizer_create(1, 1.0f, 0.0f, false);
    EXPECT_EQ(norm, nullptr);

    norm = minmax_normalizer_create(1, 5.0f, 5.0f, false);
    EXPECT_EQ(norm, nullptr);
}

TEST_F(MinMaxNormalizerTest, Destroy_NullSafe) {
    minmax_normalizer_destroy(nullptr);
    // Should not crash
}

//=============================================================================
// FIT OPERATION TESTS
//=============================================================================
// WHAT: Test min/max tracking
// WHY:  Verify normalization range calculation
// HOW:  Fit known sequences, verify min/max/range

TEST_F(MinMaxNormalizerTest, Fit_Success_SingleValue) {
    min_max_normalizer_t* norm = minmax_normalizer_create(1, 0.0f, 1.0f, false);
    ASSERT_NE(norm, nullptr);

    bool result = minmax_normalizer_fit(norm, 0, 5.0f);
    EXPECT_TRUE(result);

    minmax_stats_t stats;
    minmax_normalizer_get_stats(norm, 0, &stats);
    EXPECT_FLOAT_EQ(stats.min_value, 5.0f);
    EXPECT_FLOAT_EQ(stats.max_value, 5.0f);

    minmax_normalizer_destroy(norm);
}

TEST_F(MinMaxNormalizerTest, Fit_Success_Range_Tracked) {
    min_max_normalizer_t* norm = minmax_normalizer_create(1, 0.0f, 1.0f, false);
    ASSERT_NE(norm, nullptr);

    minmax_normalizer_fit(norm, 0, 2.0f);
    minmax_normalizer_fit(norm, 0, 8.0f);
    minmax_normalizer_fit(norm, 0, 5.0f);

    minmax_stats_t stats;
    minmax_normalizer_get_stats(norm, 0, &stats);

    EXPECT_FLOAT_EQ(stats.min_value, 2.0f);
    EXPECT_FLOAT_EQ(stats.max_value, 8.0f);
    EXPECT_FLOAT_EQ(stats.range, 6.0f);
    EXPECT_EQ(stats.sample_count, 3);

    minmax_normalizer_destroy(norm);
}

TEST_F(MinMaxNormalizerTest, Fit_Success_NegativeValues) {
    min_max_normalizer_t* norm = minmax_normalizer_create(1, 0.0f, 1.0f, false);
    ASSERT_NE(norm, nullptr);

    minmax_normalizer_fit(norm, 0, -10.0f);
    minmax_normalizer_fit(norm, 0, -5.0f);
    minmax_normalizer_fit(norm, 0, -1.0f);

    minmax_stats_t stats;
    minmax_normalizer_get_stats(norm, 0, &stats);

    EXPECT_FLOAT_EQ(stats.min_value, -10.0f);
    EXPECT_FLOAT_EQ(stats.max_value, -1.0f);

    minmax_normalizer_destroy(norm);
}

TEST_F(MinMaxNormalizerTest, Fit_Failure_NullNormalizer) {
    bool result = minmax_normalizer_fit(nullptr, 0, 5.0f);
    EXPECT_FALSE(result);
}

TEST_F(MinMaxNormalizerTest, Fit_Failure_InvalidChannel) {
    min_max_normalizer_t* norm = minmax_normalizer_create(2, 0.0f, 1.0f, false);
    ASSERT_NE(norm, nullptr);

    bool result = minmax_normalizer_fit(norm, 5, 5.0f);
    EXPECT_FALSE(result);

    minmax_normalizer_destroy(norm);
}

TEST_F(MinMaxNormalizerTest, Fit_Success_MultiChannel_Independent) {
    min_max_normalizer_t* norm = minmax_normalizer_create(3, 0.0f, 1.0f, false);
    ASSERT_NE(norm, nullptr);

    minmax_normalizer_fit(norm, 0, 0.0f);
    minmax_normalizer_fit(norm, 0, 10.0f);

    minmax_normalizer_fit(norm, 1, 100.0f);
    minmax_normalizer_fit(norm, 1, 200.0f);

    minmax_stats_t stats0, stats1;
    minmax_normalizer_get_stats(norm, 0, &stats0);
    minmax_normalizer_get_stats(norm, 1, &stats1);

    EXPECT_FLOAT_EQ(stats0.range, 10.0f);
    EXPECT_FLOAT_EQ(stats1.range, 100.0f);

    minmax_normalizer_destroy(norm);
}

//=============================================================================
// TRANSFORM TESTS
//=============================================================================
// WHAT: Test min-max scaling transformation
// WHY:  Verify formula: (x - min) / (max - min) * (target_max - target_min) + target_min
// HOW:  Test known transformations, verify [0,1] and custom ranges

TEST_F(MinMaxNormalizerTest, Transform_NoData_ReturnsTargetMin) {
    min_max_normalizer_t* norm = minmax_normalizer_create(1, 0.0f, 1.0f, false);
    ASSERT_NE(norm, nullptr);

    float result = minmax_normalizer_transform(norm, 0, 5.0f);
    EXPECT_FLOAT_EQ(result, 0.0f);  // Returns target_min

    minmax_normalizer_destroy(norm);
}

TEST_F(MinMaxNormalizerTest, Transform_Success_To01Range) {
    min_max_normalizer_t* norm = minmax_normalizer_create(1, 0.0f, 1.0f, false);
    ASSERT_NE(norm, nullptr);

    // Fit range [0, 10]
    minmax_normalizer_fit(norm, 0, 0.0f);
    minmax_normalizer_fit(norm, 0, 10.0f);

    // Transform values
    EXPECT_TRUE(FloatEquals(minmax_normalizer_transform(norm, 0, 0.0f), 0.0f));
    EXPECT_TRUE(FloatEquals(minmax_normalizer_transform(norm, 0, 5.0f), 0.5f));
    EXPECT_TRUE(FloatEquals(minmax_normalizer_transform(norm, 0, 10.0f), 1.0f));

    minmax_normalizer_destroy(norm);
}

TEST_F(MinMaxNormalizerTest, Transform_Success_ToCustomRange) {
    min_max_normalizer_t* norm = minmax_normalizer_create(1, -1.0f, 1.0f, false);
    ASSERT_NE(norm, nullptr);

    // Fit range [0, 100]
    minmax_normalizer_fit(norm, 0, 0.0f);
    minmax_normalizer_fit(norm, 0, 100.0f);

    // Transform to [-1, 1]
    EXPECT_TRUE(FloatEquals(minmax_normalizer_transform(norm, 0, 0.0f), -1.0f));
    EXPECT_TRUE(FloatEquals(minmax_normalizer_transform(norm, 0, 50.0f), 0.0f));
    EXPECT_TRUE(FloatEquals(minmax_normalizer_transform(norm, 0, 100.0f), 1.0f));

    minmax_normalizer_destroy(norm);
}

TEST_F(MinMaxNormalizerTest, Transform_Success_OutOfRange_Extrapolates) {
    min_max_normalizer_t* norm = minmax_normalizer_create(1, 0.0f, 1.0f, false);
    ASSERT_NE(norm, nullptr);

    minmax_normalizer_fit(norm, 0, 0.0f);
    minmax_normalizer_fit(norm, 0, 10.0f);

    // Values outside trained range
    float result_below = minmax_normalizer_transform(norm, 0, -5.0f);
    EXPECT_LT(result_below, 0.0f);  // Extrapolates below 0

    float result_above = minmax_normalizer_transform(norm, 0, 15.0f);
    EXPECT_GT(result_above, 1.0f);  // Extrapolates above 1

    minmax_normalizer_destroy(norm);
}

TEST_F(MinMaxNormalizerTest, Transform_Failure_NullNormalizer) {
    float result = minmax_normalizer_transform(nullptr, 0, 5.0f);
    EXPECT_FLOAT_EQ(result, 5.0f);  // Returns input unchanged
}

TEST_F(MinMaxNormalizerTest, Transform_Failure_InvalidChannel) {
    min_max_normalizer_t* norm = minmax_normalizer_create(2, 0.0f, 1.0f, false);
    ASSERT_NE(norm, nullptr);

    float result = minmax_normalizer_transform(norm, 5, 5.0f);
    EXPECT_FLOAT_EQ(result, 5.0f);  // Returns input unchanged

    minmax_normalizer_destroy(norm);
}

//=============================================================================
// FIT_TRANSFORM & INVERSE TESTS
//=============================================================================
// WHAT: Test combined operations
// WHY:  Verify efficient online normalization and reconstruction
// HOW:  Test fit_transform and inverse_transform

TEST_F(MinMaxNormalizerTest, FitTransform_Success) {
    min_max_normalizer_t* norm = minmax_normalizer_create(1, 0.0f, 1.0f, false);
    ASSERT_NE(norm, nullptr);

    float v1 = minmax_normalizer_fit_transform(norm, 0, 0.0f);
    float v2 = minmax_normalizer_fit_transform(norm, 0, 10.0f);
    float v3 = minmax_normalizer_fit_transform(norm, 0, 5.0f);

    // After fitting [0, 10], middle value should be ~0.5
    EXPECT_TRUE(FloatEquals(v3, 0.5f, 0.1f));

    minmax_normalizer_destroy(norm);
}

TEST_F(MinMaxNormalizerTest, InverseTransform_Success_RoundTrip) {
    min_max_normalizer_t* norm = minmax_normalizer_create(1, 0.0f, 1.0f, false);
    ASSERT_NE(norm, nullptr);

    // Fit range [0, 100]
    minmax_normalizer_fit(norm, 0, 0.0f);
    minmax_normalizer_fit(norm, 0, 100.0f);

    // Transform and inverse
    float original = 75.0f;
    float normalized = minmax_normalizer_transform(norm, 0, original);
    float reconstructed = minmax_normalizer_inverse_transform(norm, 0, normalized);

    EXPECT_TRUE(FloatEquals(original, reconstructed, 0.01f));

    minmax_normalizer_destroy(norm);
}

TEST_F(MinMaxNormalizerTest, InverseTransform_Success_CustomRange) {
    min_max_normalizer_t* norm = minmax_normalizer_create(1, -1.0f, 1.0f, false);
    ASSERT_NE(norm, nullptr);

    minmax_normalizer_fit(norm, 0, 0.0f);
    minmax_normalizer_fit(norm, 0, 100.0f);

    float normalized = 0.0f;  // Middle of [-1, 1]
    float original = minmax_normalizer_inverse_transform(norm, 0, normalized);
    EXPECT_TRUE(FloatEquals(original, 50.0f, 0.01f));  // Middle of [0, 100]

    minmax_normalizer_destroy(norm);
}

TEST_F(MinMaxNormalizerTest, InverseTransform_Failure_NullNormalizer) {
    float result = minmax_normalizer_inverse_transform(nullptr, 0, 0.5f);
    EXPECT_FLOAT_EQ(result, 0.5f);
}

TEST_F(MinMaxNormalizerTest, InverseTransform_Failure_InvalidChannel) {
    min_max_normalizer_t* norm = minmax_normalizer_create(2, 0.0f, 1.0f, false);
    ASSERT_NE(norm, nullptr);

    float result = minmax_normalizer_inverse_transform(norm, 5, 0.5f);
    EXPECT_FLOAT_EQ(result, 0.5f);

    minmax_normalizer_destroy(norm);
}

//=============================================================================
// STATISTICS QUERY TESTS
//=============================================================================
// WHAT: Test statistics retrieval functions
// WHY:  Verify stats tracking and query operations
// HOW:  Test get_stats, num_channels

TEST_F(MinMaxNormalizerTest, GetStats_Success) {
    min_max_normalizer_t* norm = minmax_normalizer_create(1, 0.0f, 1.0f, false);
    ASSERT_NE(norm, nullptr);

    minmax_normalizer_fit(norm, 0, 1.0f);
    minmax_normalizer_fit(norm, 0, 5.0f);
    minmax_normalizer_fit(norm, 0, 9.0f);

    minmax_stats_t stats;
    bool result = minmax_normalizer_get_stats(norm, 0, &stats);

    EXPECT_TRUE(result);
    EXPECT_FLOAT_EQ(stats.min_value, 1.0f);
    EXPECT_FLOAT_EQ(stats.max_value, 9.0f);
    EXPECT_FLOAT_EQ(stats.range, 8.0f);
    EXPECT_EQ(stats.sample_count, 3);

    minmax_normalizer_destroy(norm);
}

TEST_F(MinMaxNormalizerTest, GetStats_Failure_NullParams) {
    min_max_normalizer_t* norm = minmax_normalizer_create(1, 0.0f, 1.0f, false);
    minmax_stats_t stats;

    EXPECT_FALSE(minmax_normalizer_get_stats(nullptr, 0, &stats));
    EXPECT_FALSE(minmax_normalizer_get_stats(norm, 0, nullptr));
    EXPECT_FALSE(minmax_normalizer_get_stats(norm, 5, &stats));

    minmax_normalizer_destroy(norm);
}

TEST_F(MinMaxNormalizerTest, NumChannels_Success) {
    min_max_normalizer_t* norm = minmax_normalizer_create(7, 0.0f, 1.0f, false);
    ASSERT_NE(norm, nullptr);

    EXPECT_EQ(minmax_normalizer_num_channels(norm), 7);

    minmax_normalizer_destroy(norm);
}

TEST_F(MinMaxNormalizerTest, NumChannels_Null) {
    EXPECT_EQ(minmax_normalizer_num_channels(nullptr), 0);
}

//=============================================================================
// RESET TESTS
//=============================================================================
// WHAT: Test channel reset functionality
// WHY:  Verify statistics can be cleared
// HOW:  Fit data, reset, verify clean state

TEST_F(MinMaxNormalizerTest, ResetChannel_Success) {
    min_max_normalizer_t* norm = minmax_normalizer_create(3, 0.0f, 1.0f, false);
    ASSERT_NE(norm, nullptr);

    // Fit channel 1
    minmax_normalizer_fit(norm, 1, 0.0f);
    minmax_normalizer_fit(norm, 1, 100.0f);

    minmax_stats_t stats;
    minmax_normalizer_get_stats(norm, 1, &stats);
    EXPECT_EQ(stats.sample_count, 2);

    // Reset channel 1
    bool result = minmax_normalizer_reset_channel(norm, 1);
    EXPECT_TRUE(result);

    // Verify reset
    minmax_normalizer_get_stats(norm, 1, &stats);
    EXPECT_EQ(stats.sample_count, 0);

    minmax_normalizer_destroy(norm);
}

TEST_F(MinMaxNormalizerTest, ResetChannel_Failure) {
    min_max_normalizer_t* norm = minmax_normalizer_create(2, 0.0f, 1.0f, false);

    EXPECT_FALSE(minmax_normalizer_reset_channel(nullptr, 0));
    EXPECT_FALSE(minmax_normalizer_reset_channel(norm, 5));

    minmax_normalizer_destroy(norm);
}

TEST_F(MinMaxNormalizerTest, ResetAll_Success) {
    min_max_normalizer_t* norm = minmax_normalizer_create(3, 0.0f, 1.0f, false);
    ASSERT_NE(norm, nullptr);

    // Fit all channels
    for (size_t ch = 0; ch < 3; ch++) {
        minmax_normalizer_fit(norm, ch, 0.0f);
        minmax_normalizer_fit(norm, ch, 100.0f);
    }

    // Reset all
    minmax_normalizer_reset_all(norm);

    // Verify all channels reset
    for (size_t ch = 0; ch < 3; ch++) {
        minmax_stats_t stats;
        minmax_normalizer_get_stats(norm, ch, &stats);
        EXPECT_EQ(stats.sample_count, 0);
    }

    minmax_normalizer_destroy(norm);
}

TEST_F(MinMaxNormalizerTest, ResetAll_Null) {
    minmax_normalizer_reset_all(nullptr);
    // Should not crash
}

//=============================================================================
// REGRESSION TESTS
//=============================================================================
// WHAT: Test known edge cases and bugs
// WHY:  Prevent regressions
// HOW:  Test problematic scenarios

TEST_F(MinMaxNormalizerTest, Regression_ZeroRange_Constant) {
    min_max_normalizer_t* norm = minmax_normalizer_create(1, 0.0f, 1.0f, false);
    ASSERT_NE(norm, nullptr);

    // All same value (zero range)
    for (int i = 0; i < 10; i++) {
        minmax_normalizer_fit(norm, 0, 5.0f);
    }

    // Range should be clamped to minimum
    minmax_stats_t stats;
    minmax_normalizer_get_stats(norm, 0, &stats);
    EXPECT_GE(stats.range, 1.0f);  // Clamped to 1.0

    // Transform should not crash
    float result = minmax_normalizer_transform(norm, 0, 5.0f);
    EXPECT_FALSE(std::isnan(result));
    EXPECT_FALSE(std::isinf(result));

    minmax_normalizer_destroy(norm);
}

TEST_F(MinMaxNormalizerTest, Regression_LargeValues) {
    min_max_normalizer_t* norm = minmax_normalizer_create(1, 0.0f, 1.0f, false);
    ASSERT_NE(norm, nullptr);

    minmax_normalizer_fit(norm, 0, 1e6f);
    minmax_normalizer_fit(norm, 0, 1e7f);

    float result = minmax_normalizer_transform(norm, 0, 5e6f);
    EXPECT_FALSE(std::isnan(result));
    EXPECT_FALSE(std::isinf(result));

    minmax_normalizer_destroy(norm);
}

TEST_F(MinMaxNormalizerTest, Regression_NegativeRange) {
    min_max_normalizer_t* norm = minmax_normalizer_create(1, 0.0f, 1.0f, false);
    ASSERT_NE(norm, nullptr);

    minmax_normalizer_fit(norm, 0, -100.0f);
    minmax_normalizer_fit(norm, 0, -10.0f);

    float result = minmax_normalizer_transform(norm, 0, -50.0f);
    EXPECT_GE(result, 0.0f);
    EXPECT_LE(result, 1.0f);

    minmax_normalizer_destroy(norm);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
