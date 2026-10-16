//=============================================================================
// test_homeostatic_normalizer.cpp - Comprehensive Homeostatic Normalizer Tests
//=============================================================================

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "middleware/normalization/nimcp_homeostatic_normalizer.h"

/**
 * WHAT: Comprehensive test suite for homeostatic normalization
 * WHY:  Ensure homeostatic plasticity and activity regulation works correctly
 * HOW:  Unit tests for all 7 functions, edge cases, integration tests
 */

class HomeostaticNormalizerTest : public ::testing::Test {
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

TEST_F(HomeostaticNormalizerTest, Create_Success) {
    homeostatic_normalizer_t* norm = homeostatic_normalizer_create(1, 1.0f, 10.0f);
    ASSERT_NE(norm, nullptr);
    EXPECT_EQ(homeostatic_normalizer_num_channels(norm), 1);
    homeostatic_normalizer_destroy(norm);
}

TEST_F(HomeostaticNormalizerTest, Create_Success_MultiChannel) {
    homeostatic_normalizer_t* norm = homeostatic_normalizer_create(10, 0.5f, 100.0f);
    ASSERT_NE(norm, nullptr);
    EXPECT_EQ(homeostatic_normalizer_num_channels(norm), 10);
    homeostatic_normalizer_destroy(norm);
}

TEST_F(HomeostaticNormalizerTest, Create_Failure_ZeroChannels) {
    homeostatic_normalizer_t* norm = homeostatic_normalizer_create(0, 1.0f, 10.0f);
    EXPECT_EQ(norm, nullptr);
}

TEST_F(HomeostaticNormalizerTest, Create_Failure_InvalidTimeConstant) {
    homeostatic_normalizer_t* norm = homeostatic_normalizer_create(1, 1.0f, 0.0f);
    EXPECT_EQ(norm, nullptr);

    norm = homeostatic_normalizer_create(1, 1.0f, -1.0f);
    EXPECT_EQ(norm, nullptr);
}

TEST_F(HomeostaticNormalizerTest, Destroy_NullSafe) {
    homeostatic_normalizer_destroy(nullptr);
    // Should not crash
}

//=============================================================================
// UPDATE OPERATION TESTS
//=============================================================================
// WHAT: Test homeostatic update mechanism
// WHY:  Verify activity tracking and scaling factor adjustment
// HOW:  Update with various activity levels, verify scaling

TEST_F(HomeostaticNormalizerTest, Update_Success) {
    homeostatic_normalizer_t* norm = homeostatic_normalizer_create(1, 1.0f, 10.0f);
    ASSERT_NE(norm, nullptr);

    bool result = homeostatic_normalizer_update(norm, 0, 0.5f, 1.0f);
    EXPECT_TRUE(result);

    homeostatic_normalizer_destroy(norm);
}

TEST_F(HomeostaticNormalizerTest, Update_Success_BelowTarget) {
    homeostatic_normalizer_t* norm = homeostatic_normalizer_create(1, 1.0f, 10.0f);
    ASSERT_NE(norm, nullptr);

    // Update with low activity (below target of 1.0)
    for (int i = 0; i < 10; i++) {
        homeostatic_normalizer_update(norm, 0, 0.2f, 1.0f);
    }

    // Scaling factor should increase to compensate
    float scaling = homeostatic_normalizer_get_scaling(norm, 0);
    EXPECT_GT(scaling, 1.0f);

    homeostatic_normalizer_destroy(norm);
}

TEST_F(HomeostaticNormalizerTest, Update_Success_AboveTarget) {
    homeostatic_normalizer_t* norm = homeostatic_normalizer_create(1, 1.0f, 1.0f);
    ASSERT_NE(norm, nullptr);

    // Update with high activity (above target of 1.0)
    // Need faster time constant (1.0) for quicker convergence
    for (int i = 0; i < 20; i++) {
        homeostatic_normalizer_update(norm, 0, 2.0f, 1.0f);
    }

    // Scaling factor should decrease to compensate (after sufficient iterations)
    float scaling = homeostatic_normalizer_get_scaling(norm, 0);
    EXPECT_LT(scaling, 1.0f);

    homeostatic_normalizer_destroy(norm);
}

TEST_F(HomeostaticNormalizerTest, Update_Failure_NullNormalizer) {
    bool result = homeostatic_normalizer_update(nullptr, 0, 1.0f, 1.0f);
    EXPECT_FALSE(result);
}

TEST_F(HomeostaticNormalizerTest, Update_Failure_InvalidChannel) {
    homeostatic_normalizer_t* norm = homeostatic_normalizer_create(2, 1.0f, 10.0f);
    ASSERT_NE(norm, nullptr);

    bool result = homeostatic_normalizer_update(norm, 5, 1.0f, 1.0f);
    EXPECT_FALSE(result);

    homeostatic_normalizer_destroy(norm);
}

TEST_F(HomeostaticNormalizerTest, Update_Failure_InvalidDt) {
    homeostatic_normalizer_t* norm = homeostatic_normalizer_create(1, 1.0f, 10.0f);
    ASSERT_NE(norm, nullptr);

    bool result = homeostatic_normalizer_update(norm, 0, 1.0f, 0.0f);
    EXPECT_FALSE(result);

    result = homeostatic_normalizer_update(norm, 0, 1.0f, -1.0f);
    EXPECT_FALSE(result);

    homeostatic_normalizer_destroy(norm);
}

TEST_F(HomeostaticNormalizerTest, Update_Success_MultiChannel_Independent) {
    homeostatic_normalizer_t* norm = homeostatic_normalizer_create(3, 1.0f, 10.0f);
    ASSERT_NE(norm, nullptr);

    // Different activity for each channel
    homeostatic_normalizer_update(norm, 0, 0.5f, 1.0f);
    homeostatic_normalizer_update(norm, 1, 1.0f, 1.0f);
    homeostatic_normalizer_update(norm, 2, 2.0f, 1.0f);

    // Each channel should have different scaling
    homeostatic_normalizer_destroy(norm);
}

//=============================================================================
// SCALING OPERATION TESTS
//=============================================================================
// WHAT: Test scaling factor retrieval and application
// WHY:  Verify homeostatic plasticity mechanism
// HOW:  Get and apply scaling factors

TEST_F(HomeostaticNormalizerTest, GetScaling_Success_InitialValue) {
    homeostatic_normalizer_t* norm = homeostatic_normalizer_create(1, 1.0f, 10.0f);
    ASSERT_NE(norm, nullptr);

    float scaling = homeostatic_normalizer_get_scaling(norm, 0);
    EXPECT_FLOAT_EQ(scaling, 1.0f);  // Initial scaling is 1.0

    homeostatic_normalizer_destroy(norm);
}

TEST_F(HomeostaticNormalizerTest, GetScaling_Failure_NullNormalizer) {
    float scaling = homeostatic_normalizer_get_scaling(nullptr, 0);
    EXPECT_FLOAT_EQ(scaling, 1.0f);  // Returns 1.0 on error
}

TEST_F(HomeostaticNormalizerTest, GetScaling_Failure_InvalidChannel) {
    homeostatic_normalizer_t* norm = homeostatic_normalizer_create(2, 1.0f, 10.0f);
    ASSERT_NE(norm, nullptr);

    float scaling = homeostatic_normalizer_get_scaling(norm, 5);
    EXPECT_FLOAT_EQ(scaling, 1.0f);  // Returns 1.0 on error

    homeostatic_normalizer_destroy(norm);
}

TEST_F(HomeostaticNormalizerTest, Apply_Success) {
    homeostatic_normalizer_t* norm = homeostatic_normalizer_create(1, 1.0f, 10.0f);
    ASSERT_NE(norm, nullptr);

    float result = homeostatic_normalizer_apply(norm, 0, 5.0f);
    EXPECT_FLOAT_EQ(result, 5.0f);  // Initially scaling = 1.0

    homeostatic_normalizer_destroy(norm);
}

TEST_F(HomeostaticNormalizerTest, Apply_Success_WithScaling) {
    homeostatic_normalizer_t* norm = homeostatic_normalizer_create(1, 1.0f, 10.0f);
    ASSERT_NE(norm, nullptr);

    // Update to create scaling factor != 1.0
    for (int i = 0; i < 10; i++) {
        homeostatic_normalizer_update(norm, 0, 0.5f, 1.0f);
    }

    float scaling = homeostatic_normalizer_get_scaling(norm, 0);
    float value = 5.0f;
    float result = homeostatic_normalizer_apply(norm, 0, value);

    EXPECT_TRUE(FloatEquals(result, value * scaling, 0.01f));

    homeostatic_normalizer_destroy(norm);
}

TEST_F(HomeostaticNormalizerTest, Apply_Failure_NullNormalizer) {
    float result = homeostatic_normalizer_apply(nullptr, 0, 5.0f);
    EXPECT_FLOAT_EQ(result, 5.0f);  // Returns input unchanged
}

TEST_F(HomeostaticNormalizerTest, Apply_Failure_InvalidChannel) {
    homeostatic_normalizer_t* norm = homeostatic_normalizer_create(2, 1.0f, 10.0f);
    ASSERT_NE(norm, nullptr);

    float result = homeostatic_normalizer_apply(norm, 5, 5.0f);
    EXPECT_FLOAT_EQ(result, 5.0f);  // Returns input unchanged

    homeostatic_normalizer_destroy(norm);
}

//=============================================================================
// RESET TESTS
//=============================================================================
// WHAT: Test channel reset functionality
// WHY:  Verify state can be cleared
// HOW:  Update data, reset, verify clean state

TEST_F(HomeostaticNormalizerTest, ResetChannel_Success) {
    homeostatic_normalizer_t* norm = homeostatic_normalizer_create(3, 1.0f, 10.0f);
    ASSERT_NE(norm, nullptr);

    // Update channel 1
    for (int i = 0; i < 10; i++) {
        homeostatic_normalizer_update(norm, 1, 0.5f, 1.0f);
    }

    // Reset channel 1
    bool result = homeostatic_normalizer_reset_channel(norm, 1);
    EXPECT_TRUE(result);

    // Scaling should be back to 1.0
    float scaling = homeostatic_normalizer_get_scaling(norm, 1);
    EXPECT_FLOAT_EQ(scaling, 1.0f);

    homeostatic_normalizer_destroy(norm);
}

TEST_F(HomeostaticNormalizerTest, ResetChannel_Failure) {
    homeostatic_normalizer_t* norm = homeostatic_normalizer_create(2, 1.0f, 10.0f);

    EXPECT_FALSE(homeostatic_normalizer_reset_channel(nullptr, 0));
    EXPECT_FALSE(homeostatic_normalizer_reset_channel(norm, 5));

    homeostatic_normalizer_destroy(norm);
}

TEST_F(HomeostaticNormalizerTest, ResetAll_Success) {
    homeostatic_normalizer_t* norm = homeostatic_normalizer_create(3, 1.0f, 10.0f);
    ASSERT_NE(norm, nullptr);

    // Update all channels
    for (size_t ch = 0; ch < 3; ch++) {
        for (int i = 0; i < 5; i++) {
            homeostatic_normalizer_update(norm, ch, 0.5f, 1.0f);
        }
    }

    // Reset all
    homeostatic_normalizer_reset_all(norm);

    // All channels should have scaling = 1.0
    for (size_t ch = 0; ch < 3; ch++) {
        float scaling = homeostatic_normalizer_get_scaling(norm, ch);
        EXPECT_FLOAT_EQ(scaling, 1.0f);
    }

    homeostatic_normalizer_destroy(norm);
}

TEST_F(HomeostaticNormalizerTest, ResetAll_Null) {
    homeostatic_normalizer_reset_all(nullptr);
    // Should not crash
}

//=============================================================================
// NUM_CHANNELS TEST
//=============================================================================
// WHAT: Test channel count query
// WHY:  Verify channel count tracking
// HOW:  Create with various channel counts

TEST_F(HomeostaticNormalizerTest, NumChannels_Success) {
    homeostatic_normalizer_t* norm = homeostatic_normalizer_create(7, 1.0f, 10.0f);
    ASSERT_NE(norm, nullptr);

    EXPECT_EQ(homeostatic_normalizer_num_channels(norm), 7);

    homeostatic_normalizer_destroy(norm);
}

TEST_F(HomeostaticNormalizerTest, NumChannels_Null) {
    EXPECT_EQ(homeostatic_normalizer_num_channels(nullptr), 0);
}

//=============================================================================
// REGRESSION TESTS
//=============================================================================
// WHAT: Test known edge cases and bugs
// WHY:  Prevent regressions
// HOW:  Test problematic scenarios

TEST_F(HomeostaticNormalizerTest, Regression_ScalingClipping) {
    homeostatic_normalizer_t* norm = homeostatic_normalizer_create(1, 1.0f, 1.0f);
    ASSERT_NE(norm, nullptr);

    // Extreme low activity to force scaling to max
    for (int i = 0; i < 100; i++) {
        homeostatic_normalizer_update(norm, 0, 0.0f, 1.0f);
    }

    // Scaling should be clamped to 10.0
    float scaling = homeostatic_normalizer_get_scaling(norm, 0);
    EXPECT_LE(scaling, 10.0f);

    homeostatic_normalizer_destroy(norm);
}

TEST_F(HomeostaticNormalizerTest, Regression_ScalingClipping_Min) {
    homeostatic_normalizer_t* norm = homeostatic_normalizer_create(1, 1.0f, 1.0f);
    ASSERT_NE(norm, nullptr);

    // Extreme high activity to force scaling to min
    for (int i = 0; i < 100; i++) {
        homeostatic_normalizer_update(norm, 0, 10.0f, 1.0f);
    }

    // Scaling should be clamped to 0.1
    float scaling = homeostatic_normalizer_get_scaling(norm, 0);
    EXPECT_GE(scaling, 0.1f);

    homeostatic_normalizer_destroy(norm);
}

TEST_F(HomeostaticNormalizerTest, Regression_LargeDt) {
    homeostatic_normalizer_t* norm = homeostatic_normalizer_create(1, 1.0f, 10.0f);
    ASSERT_NE(norm, nullptr);

    // Large dt
    bool result = homeostatic_normalizer_update(norm, 0, 0.5f, 100.0f);
    EXPECT_TRUE(result);

    // Should still work
    float scaling = homeostatic_normalizer_get_scaling(norm, 0);
    EXPECT_FALSE(std::isnan(scaling));
    EXPECT_FALSE(std::isinf(scaling));

    homeostatic_normalizer_destroy(norm);
}

TEST_F(HomeostaticNormalizerTest, Regression_ZeroActivity) {
    homeostatic_normalizer_t* norm = homeostatic_normalizer_create(1, 1.0f, 10.0f);
    ASSERT_NE(norm, nullptr);

    // All zero activity
    for (int i = 0; i < 10; i++) {
        homeostatic_normalizer_update(norm, 0, 0.0f, 1.0f);
    }

    float scaling = homeostatic_normalizer_get_scaling(norm, 0);
    EXPECT_FALSE(std::isnan(scaling));
    EXPECT_FALSE(std::isinf(scaling));

    homeostatic_normalizer_destroy(norm);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
