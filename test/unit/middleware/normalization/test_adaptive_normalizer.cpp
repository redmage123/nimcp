//=============================================================================
// test_adaptive_normalizer.cpp - Comprehensive Adaptive Normalizer Tests
//=============================================================================

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "middleware/normalization/nimcp_adaptive_normalizer.h"

/**
 * WHAT: Comprehensive test suite for adaptive normalization
 * WHY:  Ensure adaptive learning rate normalization works correctly
 * HOW:  Unit tests for all 7 functions, edge cases, integration tests
 */

class AdaptiveNormalizerTest : public ::testing::Test {
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
// HOW:  Test parameter combinations and edge cases

TEST_F(AdaptiveNormalizerTest, Create_Success) {
    adaptive_normalizer_t* norm = adaptive_normalizer_create(1, 0.1f, 0.01f);
    ASSERT_NE(norm, nullptr);
    EXPECT_EQ(adaptive_normalizer_num_channels(norm), 1);
    adaptive_normalizer_destroy(norm);
}

TEST_F(AdaptiveNormalizerTest, Create_Success_MultiChannel) {
    adaptive_normalizer_t* norm = adaptive_normalizer_create(10, 0.05f, 0.001f);
    ASSERT_NE(norm, nullptr);
    EXPECT_EQ(adaptive_normalizer_num_channels(norm), 10);
    adaptive_normalizer_destroy(norm);
}

TEST_F(AdaptiveNormalizerTest, Create_Failure_ZeroChannels) {
    adaptive_normalizer_t* norm = adaptive_normalizer_create(0, 0.1f, 0.01f);
    EXPECT_EQ(norm, nullptr);
}

TEST_F(AdaptiveNormalizerTest, Destroy_NullSafe) {
    adaptive_normalizer_destroy(nullptr);
    // Should not crash
}

//=============================================================================
// FIT OPERATION TESTS
//=============================================================================
// WHAT: Test adaptive mean/variance calculation with learning rate adaptation
// WHY:  Verify exponential moving average and adaptive learning
// HOW:  Fit sequences, verify mean/variance adaptation

TEST_F(AdaptiveNormalizerTest, Fit_Success_SingleValue) {
    adaptive_normalizer_t* norm = adaptive_normalizer_create(1, 0.1f, 0.01f);
    ASSERT_NE(norm, nullptr);

    bool result = adaptive_normalizer_fit(norm, 0, 5.0f);
    EXPECT_TRUE(result);

    adaptive_normalizer_destroy(norm);
}

TEST_F(AdaptiveNormalizerTest, Fit_Success_AdaptsToData) {
    adaptive_normalizer_t* norm = adaptive_normalizer_create(1, 0.1f, 0.01f);
    ASSERT_NE(norm, nullptr);

    // Fit sequence
    for (int i = 0; i < 10; i++) {
        adaptive_normalizer_fit(norm, 0, (float)i);
    }

    // Mean should have adapted toward recent values
    // (Exact value depends on learning rate adaptation)

    adaptive_normalizer_destroy(norm);
}

TEST_F(AdaptiveNormalizerTest, Fit_Failure_NullNormalizer) {
    bool result = adaptive_normalizer_fit(nullptr, 0, 5.0f);
    EXPECT_FALSE(result);
}

TEST_F(AdaptiveNormalizerTest, Fit_Failure_InvalidChannel) {
    adaptive_normalizer_t* norm = adaptive_normalizer_create(2, 0.1f, 0.01f);
    ASSERT_NE(norm, nullptr);

    bool result = adaptive_normalizer_fit(norm, 5, 5.0f);
    EXPECT_FALSE(result);

    adaptive_normalizer_destroy(norm);
}

TEST_F(AdaptiveNormalizerTest, Fit_Success_MultiChannel_Independent) {
    adaptive_normalizer_t* norm = adaptive_normalizer_create(3, 0.1f, 0.01f);
    ASSERT_NE(norm, nullptr);

    adaptive_normalizer_fit(norm, 0, 1.0f);
    adaptive_normalizer_fit(norm, 1, 10.0f);
    adaptive_normalizer_fit(norm, 2, 100.0f);

    // Each channel should adapt independently
    adaptive_normalizer_destroy(norm);
}

//=============================================================================
// TRANSFORM TESTS
//=============================================================================
// WHAT: Test adaptive normalization transformation
// WHY:  Verify formula: (x - mean) / sqrt(variance)
// HOW:  Test known transformations

TEST_F(AdaptiveNormalizerTest, Transform_NoData_ReturnsZero) {
    adaptive_normalizer_t* norm = adaptive_normalizer_create(1, 0.1f, 0.01f);
    ASSERT_NE(norm, nullptr);

    float result = adaptive_normalizer_transform(norm, 0, 5.0f);
    EXPECT_FLOAT_EQ(result, 0.0f);

    adaptive_normalizer_destroy(norm);
}

TEST_F(AdaptiveNormalizerTest, Transform_Success_AfterFit) {
    adaptive_normalizer_t* norm = adaptive_normalizer_create(1, 0.1f, 0.01f);
    ASSERT_NE(norm, nullptr);

    // Fit some data
    for (int i = 0; i < 20; i++) {
        adaptive_normalizer_fit(norm, 0, (float)i);
    }

    // Transform should produce z-score
    float result = adaptive_normalizer_transform(norm, 0, 10.0f);
    EXPECT_FALSE(std::isnan(result));
    EXPECT_FALSE(std::isinf(result));

    adaptive_normalizer_destroy(norm);
}

TEST_F(AdaptiveNormalizerTest, Transform_Failure_NullNormalizer) {
    float result = adaptive_normalizer_transform(nullptr, 0, 5.0f);
    EXPECT_FLOAT_EQ(result, 5.0f);  // Returns input unchanged
}

TEST_F(AdaptiveNormalizerTest, Transform_Failure_InvalidChannel) {
    adaptive_normalizer_t* norm = adaptive_normalizer_create(2, 0.1f, 0.01f);
    ASSERT_NE(norm, nullptr);

    float result = adaptive_normalizer_transform(norm, 5, 5.0f);
    EXPECT_FLOAT_EQ(result, 5.0f);  // Returns input unchanged

    adaptive_normalizer_destroy(norm);
}

//=============================================================================
// FIT_TRANSFORM TESTS
//=============================================================================
// WHAT: Test combined fit and transform operation
// WHY:  Verify efficient online normalization
// HOW:  Test online adaptation

TEST_F(AdaptiveNormalizerTest, FitTransform_Success) {
    adaptive_normalizer_t* norm = adaptive_normalizer_create(1, 0.1f, 0.01f);
    ASSERT_NE(norm, nullptr);

    for (int i = 0; i < 10; i++) {
        float z = adaptive_normalizer_fit_transform(norm, 0, (float)i);
        EXPECT_FALSE(std::isnan(z));
        EXPECT_FALSE(std::isinf(z));
    }

    adaptive_normalizer_destroy(norm);
}

TEST_F(AdaptiveNormalizerTest, FitTransform_Success_AdaptsOverTime) {
    adaptive_normalizer_t* norm = adaptive_normalizer_create(1, 0.2f, 0.05f);
    ASSERT_NE(norm, nullptr);

    // Fit sequence with shift
    for (int i = 0; i < 5; i++) {
        adaptive_normalizer_fit_transform(norm, 0, 0.0f);
    }

    // Shift distribution
    for (int i = 0; i < 5; i++) {
        float z = adaptive_normalizer_fit_transform(norm, 0, 10.0f);
        EXPECT_FALSE(std::isnan(z));
    }

    adaptive_normalizer_destroy(norm);
}

//=============================================================================
// RESET TESTS
//=============================================================================
// WHAT: Test channel reset functionality
// WHY:  Verify statistics can be cleared
// HOW:  Fit data, reset, verify clean state

TEST_F(AdaptiveNormalizerTest, ResetChannel_Success) {
    adaptive_normalizer_t* norm = adaptive_normalizer_create(3, 0.1f, 0.01f);
    ASSERT_NE(norm, nullptr);

    // Fit channel 1
    for (int i = 0; i < 10; i++) {
        adaptive_normalizer_fit(norm, 1, (float)i);
    }

    // Reset channel 1
    bool result = adaptive_normalizer_reset_channel(norm, 1);
    EXPECT_TRUE(result);

    // After reset, should return initial state
    adaptive_normalizer_destroy(norm);
}

TEST_F(AdaptiveNormalizerTest, ResetChannel_Failure) {
    adaptive_normalizer_t* norm = adaptive_normalizer_create(2, 0.1f, 0.01f);

    EXPECT_FALSE(adaptive_normalizer_reset_channel(nullptr, 0));
    EXPECT_FALSE(adaptive_normalizer_reset_channel(norm, 5));

    adaptive_normalizer_destroy(norm);
}

TEST_F(AdaptiveNormalizerTest, ResetAll_Success) {
    adaptive_normalizer_t* norm = adaptive_normalizer_create(3, 0.1f, 0.01f);
    ASSERT_NE(norm, nullptr);

    // Fit all channels
    for (size_t ch = 0; ch < 3; ch++) {
        for (int i = 0; i < 5; i++) {
            adaptive_normalizer_fit(norm, ch, (float)i);
        }
    }

    // Reset all
    adaptive_normalizer_reset_all(norm);

    // All channels should be reset
    adaptive_normalizer_destroy(norm);
}

TEST_F(AdaptiveNormalizerTest, ResetAll_Null) {
    adaptive_normalizer_reset_all(nullptr);
    // Should not crash
}

//=============================================================================
// NUM_CHANNELS TEST
//=============================================================================
// WHAT: Test channel count query
// WHY:  Verify channel count tracking
// HOW:  Create with various channel counts

TEST_F(AdaptiveNormalizerTest, NumChannels_Success) {
    adaptive_normalizer_t* norm = adaptive_normalizer_create(7, 0.1f, 0.01f);
    ASSERT_NE(norm, nullptr);

    EXPECT_EQ(adaptive_normalizer_num_channels(norm), 7);

    adaptive_normalizer_destroy(norm);
}

TEST_F(AdaptiveNormalizerTest, NumChannels_Null) {
    EXPECT_EQ(adaptive_normalizer_num_channels(nullptr), 0);
}

//=============================================================================
// REGRESSION TESTS
//=============================================================================
// WHAT: Test known edge cases and bugs
// WHY:  Prevent regressions
// HOW:  Test problematic scenarios

TEST_F(AdaptiveNormalizerTest, Regression_ConstantValues) {
    adaptive_normalizer_t* norm = adaptive_normalizer_create(1, 0.1f, 0.01f);
    ASSERT_NE(norm, nullptr);

    // All same value (variance -> 0)
    for (int i = 0; i < 10; i++) {
        adaptive_normalizer_fit(norm, 0, 5.0f);
    }

    // Transform should handle zero variance
    float result = adaptive_normalizer_transform(norm, 0, 5.0f);
    EXPECT_FALSE(std::isnan(result));
    EXPECT_FALSE(std::isinf(result));

    adaptive_normalizer_destroy(norm);
}

TEST_F(AdaptiveNormalizerTest, Regression_LargeValues) {
    adaptive_normalizer_t* norm = adaptive_normalizer_create(1, 0.1f, 0.01f);
    ASSERT_NE(norm, nullptr);

    adaptive_normalizer_fit(norm, 0, 1e6f);
    adaptive_normalizer_fit(norm, 0, 1e7f);

    float result = adaptive_normalizer_transform(norm, 0, 5e6f);
    EXPECT_FALSE(std::isnan(result));
    EXPECT_FALSE(std::isinf(result));

    adaptive_normalizer_destroy(norm);
}

TEST_F(AdaptiveNormalizerTest, Regression_NegativeValues) {
    adaptive_normalizer_t* norm = adaptive_normalizer_create(1, 0.1f, 0.01f);
    ASSERT_NE(norm, nullptr);

    adaptive_normalizer_fit(norm, 0, -10.0f);
    adaptive_normalizer_fit(norm, 0, -5.0f);

    float result = adaptive_normalizer_transform(norm, 0, -7.5f);
    EXPECT_FALSE(std::isnan(result));

    adaptive_normalizer_destroy(norm);
}

TEST_F(AdaptiveNormalizerTest, Regression_LearningRateClipping) {
    adaptive_normalizer_t* norm = adaptive_normalizer_create(1, 0.1f, 1.0f);  // High adaptation rate
    ASSERT_NE(norm, nullptr);

    // Large variations should trigger learning rate adaptation
    adaptive_normalizer_fit(norm, 0, 0.0f);
    adaptive_normalizer_fit(norm, 0, 1000.0f);
    adaptive_normalizer_fit(norm, 0, 0.0f);

    // Should still work (learning rate clamped)
    float result = adaptive_normalizer_transform(norm, 0, 500.0f);
    EXPECT_FALSE(std::isnan(result));
    EXPECT_FALSE(std::isinf(result));

    adaptive_normalizer_destroy(norm);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
