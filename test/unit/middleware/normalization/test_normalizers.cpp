//=============================================================================
// test_normalizers.cpp - Comprehensive Normalizer Integration Tests
//=============================================================================
/**
 * This test suite tests all 4 normalizers together to ensure:
 * - Consistent API across all normalizers
 * - Proper normalization properties
 * - Correct interaction between different normalization strategies
 */

#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include <cmath>
#include <algorithm>

// Headers have their own extern "C" guards
#include "middleware/normalization/nimcp_adaptive_normalizer.h"
#include "middleware/normalization/nimcp_homeostatic_normalizer.h"
#include "middleware/normalization/nimcp_min_max_normalizer.h"
#include "middleware/normalization/nimcp_zscore_normalizer.h"

//=============================================================================
// Test Fixture
//=============================================================================

class NormalizersTest : public ::testing::Test {
protected:
    adaptive_normalizer_t* adaptive;
    homeostatic_normalizer_t* homeostatic;
    min_max_normalizer_t* minmax;
    zscore_normalizer_t* zscore;

    static constexpr size_t NUM_CHANNELS = 10;
    static constexpr float EPSILON = 1e-6f;

    void SetUp() override {
        // Create all normalizers
        adaptive = adaptive_normalizer_create(NUM_CHANNELS, 0.01f, 0.1f);
        homeostatic = homeostatic_normalizer_create(NUM_CHANNELS, 0.5f, 10.0f);
        minmax = minmax_normalizer_create(NUM_CHANNELS, 0.0f, 1.0f, false);
        zscore = zscore_normalizer_create(NUM_CHANNELS, 0, 0.0f);

        ASSERT_NE(adaptive, nullptr);
        ASSERT_NE(homeostatic, nullptr);
        ASSERT_NE(minmax, nullptr);
        ASSERT_NE(zscore, nullptr);
    }

    void TearDown() override {
        adaptive_normalizer_destroy(adaptive);
        homeostatic_normalizer_destroy(homeostatic);
        minmax_normalizer_destroy(minmax);
        zscore_normalizer_destroy(zscore);
    }

    // Helper: Generate test data
    std::vector<float> generateTestData(size_t count, float mean, float std) {
        std::vector<float> data(count);
        for (size_t i = 0; i < count; i++) {
            data[i] = mean + std * sinf(i * 0.1f);
        }
        return data;
    }
};

//=============================================================================
// API Consistency Tests
//=============================================================================

TEST_F(NormalizersTest, AllHaveNumChannelsQuery) {
    EXPECT_EQ(adaptive_normalizer_num_channels(adaptive), NUM_CHANNELS);
    EXPECT_EQ(homeostatic_normalizer_num_channels(homeostatic), NUM_CHANNELS);
    EXPECT_EQ(minmax_normalizer_num_channels(minmax), NUM_CHANNELS);
    EXPECT_EQ(zscore_normalizer_num_channels(zscore), NUM_CHANNELS);
}

TEST_F(NormalizersTest, AllHaveResetChannel) {
    EXPECT_TRUE(adaptive_normalizer_reset_channel(adaptive, 0));
    EXPECT_TRUE(homeostatic_normalizer_reset_channel(homeostatic, 0));
    EXPECT_TRUE(minmax_normalizer_reset_channel(minmax, 0));
    EXPECT_TRUE(zscore_normalizer_reset_channel(zscore, 0));
}

TEST_F(NormalizersTest, AllHaveResetAll) {
    adaptive_normalizer_reset_all(adaptive);
    homeostatic_normalizer_reset_all(homeostatic);
    minmax_normalizer_reset_all(minmax);
    zscore_normalizer_reset_all(zscore);
    // Should not crash
}

//=============================================================================
// Basic Normalization Tests
//=============================================================================

TEST_F(NormalizersTest, AdaptiveNormalizationBasic) {
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};

    for (float val : data) {
        adaptive_normalizer_fit(adaptive, 0, val);
    }

    float normalized = adaptive_normalizer_transform(adaptive, 0, 3.0f);

    // Should be normalized
    EXPECT_GE(normalized, -5.0f);
    EXPECT_LE(normalized, 5.0f);
}

TEST_F(NormalizersTest, HomeostaticNormalizationBasic) {
    float activity = 0.8f;
    float dt = 0.1f;

    homeostatic_normalizer_update(homeostatic, 0, activity, dt);

    float scaling = homeostatic_normalizer_get_scaling(homeostatic, 0);
    EXPECT_GT(scaling, 0.0f);

    float normalized = homeostatic_normalizer_apply(homeostatic, 0, 1.0f);
    EXPECT_GT(normalized, 0.0f);
}

TEST_F(NormalizersTest, MinMaxNormalizationBasic) {
    std::vector<float> data = {0.0f, 5.0f, 10.0f, 15.0f, 20.0f};

    for (float val : data) {
        minmax_normalizer_fit(minmax, 0, val);
    }

    float normalized = minmax_normalizer_transform(minmax, 0, 10.0f);

    // Should be normalized to [0, 1]
    EXPECT_GE(normalized, 0.0f);
    EXPECT_LE(normalized, 1.0f);
    EXPECT_NEAR(normalized, 0.5f, 0.1f); // 10 is middle of [0, 20]
}

TEST_F(NormalizersTest, ZScoreNormalizationBasic) {
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};

    for (float val : data) {
        zscore_normalizer_fit(zscore, 0, val);
    }

    float normalized = zscore_normalizer_transform(zscore, 0, 3.0f);

    // Mean is 3.0, so z-score should be near 0
    EXPECT_NEAR(normalized, 0.0f, 1.0f);
}

//=============================================================================
// Normalization Properties Tests
//=============================================================================

TEST_F(NormalizersTest, MinMaxRange) {
    std::vector<float> data = generateTestData(100, 50.0f, 10.0f);

    for (float val : data) {
        minmax_normalizer_fit(minmax, 0, val);
    }

    // Test multiple values
    for (int i = 0; i < 50; i++) {
        float normalized = minmax_normalizer_transform(minmax, 0, data[i]);
        EXPECT_GE(normalized, 0.0f) << "Value " << i << " out of range";
        EXPECT_LE(normalized, 1.0f) << "Value " << i << " out of range";
    }
}

TEST_F(NormalizersTest, ZScorePropertiesStandardNormal) {
    std::vector<float> data = generateTestData(1000, 0.0f, 1.0f);

    for (float val : data) {
        zscore_normalizer_fit(zscore, 0, val);
    }

    // Transform and check mean ~0, std ~1
    std::vector<float> normalized;
    for (float val : data) {
        normalized.push_back(zscore_normalizer_transform(zscore, 0, val));
    }

    float sum = 0.0f;
    for (float val : normalized) {
        sum += val;
    }
    float mean = sum / normalized.size();

    EXPECT_NEAR(mean, 0.0f, 0.2f) << "Normalized mean should be ~0";
}

TEST_F(NormalizersTest, AdaptiveFitTransform) {
    float value = 5.0f;

    float result = adaptive_normalizer_fit_transform(adaptive, 0, value);

    // Should return a valid normalized value
    EXPECT_TRUE(std::isfinite(result));
}

TEST_F(NormalizersTest, MinMaxFitTransform) {
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};

    for (float val : data) {
        float result = minmax_normalizer_fit_transform(minmax, 0, val);
        EXPECT_TRUE(std::isfinite(result));
    }
}

TEST_F(NormalizersTest, ZScoreFitTransform) {
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};

    for (float val : data) {
        float result = zscore_normalizer_fit_transform(zscore, 0, val);
        EXPECT_TRUE(std::isfinite(result));
    }
}

//=============================================================================
// Inverse Transform Tests
//=============================================================================

TEST_F(NormalizersTest, MinMaxInverseTransform) {
    std::vector<float> data = {0.0f, 5.0f, 10.0f, 15.0f, 20.0f};

    for (float val : data) {
        minmax_normalizer_fit(minmax, 0, val);
    }

    float original = 10.0f;
    float normalized = minmax_normalizer_transform(minmax, 0, original);
    float reconstructed = minmax_normalizer_inverse_transform(minmax, 0, normalized);

    EXPECT_NEAR(reconstructed, original, 0.1f);
}

TEST_F(NormalizersTest, ZScoreInverseTransform) {
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};

    for (float val : data) {
        zscore_normalizer_fit(zscore, 0, val);
    }

    float original = 3.0f;
    float normalized = zscore_normalizer_transform(zscore, 0, original);
    float reconstructed = zscore_normalizer_inverse_transform(zscore, 0, normalized);

    EXPECT_NEAR(reconstructed, original, 0.01f);
}

//=============================================================================
// Multi-Channel Tests
//=============================================================================

TEST_F(NormalizersTest, AllSupportMultipleChannels) {
    for (size_t ch = 0; ch < NUM_CHANNELS; ch++) {
        adaptive_normalizer_fit(adaptive, ch, ch * 1.0f);
        homeostatic_normalizer_update(homeostatic, ch, 0.5f, 0.1f);
        minmax_normalizer_fit(minmax, ch, ch * 10.0f);
        zscore_normalizer_fit(zscore, ch, ch * 5.0f);
    }

    // All channels should work independently
    for (size_t ch = 0; ch < NUM_CHANNELS; ch++) {
        EXPECT_TRUE(std::isfinite(adaptive_normalizer_transform(adaptive, ch, 1.0f)));
        EXPECT_TRUE(std::isfinite(homeostatic_normalizer_apply(homeostatic, ch, 1.0f)));
        EXPECT_TRUE(std::isfinite(minmax_normalizer_transform(minmax, ch, 1.0f)));
        EXPECT_TRUE(std::isfinite(zscore_normalizer_transform(zscore, ch, 1.0f)));
    }
}

TEST_F(NormalizersTest, ChannelIndependence) {
    // Fit channel 0 with data
    for (int i = 0; i < 10; i++) {
        minmax_normalizer_fit(minmax, 0, i * 1.0f);
        zscore_normalizer_fit(zscore, 0, i * 1.0f);
    }

    // Fit channel 1 with different data
    for (int i = 0; i < 10; i++) {
        minmax_normalizer_fit(minmax, 1, i * 10.0f);
        zscore_normalizer_fit(zscore, 1, i * 10.0f);
    }

    // Transformations should differ
    float norm0_mm = minmax_normalizer_transform(minmax, 0, 5.0f);
    float norm1_mm = minmax_normalizer_transform(minmax, 1, 5.0f);

    float norm0_z = zscore_normalizer_transform(zscore, 0, 5.0f);
    float norm1_z = zscore_normalizer_transform(zscore, 1, 5.0f);

    EXPECT_NE(norm0_mm, norm1_mm);
    EXPECT_NE(norm0_z, norm1_z);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(NormalizersTest, MinMaxStats) {
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};

    for (float val : data) {
        minmax_normalizer_fit(minmax, 0, val);
    }

    minmax_stats_t stats;
    bool success = minmax_normalizer_get_stats(minmax, 0, &stats);

    EXPECT_TRUE(success);
    EXPECT_FLOAT_EQ(stats.min_value, 1.0f);
    EXPECT_FLOAT_EQ(stats.max_value, 5.0f);
    EXPECT_FLOAT_EQ(stats.range, 4.0f);
    EXPECT_EQ(stats.sample_count, 5);
}

TEST_F(NormalizersTest, ZScoreStats) {
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};

    for (float val : data) {
        zscore_normalizer_fit(zscore, 0, val);
    }

    zscore_stats_t stats;
    bool success = zscore_normalizer_get_stats(zscore, 0, &stats);

    EXPECT_TRUE(success);
    EXPECT_NEAR(stats.mean, 3.0f, 0.01f);
    EXPECT_GT(stats.variance, 0.0f);
    EXPECT_GT(stats.stddev, 0.0f);
    EXPECT_EQ(stats.sample_count, 5);
}

TEST_F(NormalizersTest, ZScoreMeanAndStddevGetters) {
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};

    for (float val : data) {
        zscore_normalizer_fit(zscore, 0, val);
    }

    float mean = zscore_normalizer_mean(zscore, 0);
    float stddev = zscore_normalizer_stddev(zscore, 0);

    EXPECT_NEAR(mean, 3.0f, 0.01f);
    EXPECT_GT(stddev, 0.0f);
}

//=============================================================================
// Batch Operations Tests
//=============================================================================

TEST_F(NormalizersTest, ZScoreBatchFit) {
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};

    size_t fitted = zscore_normalizer_fit_batch(
        zscore, 0, data.data(), data.size());

    EXPECT_EQ(fitted, data.size());

    float mean = zscore_normalizer_mean(zscore, 0);
    EXPECT_NEAR(mean, 3.0f, 0.01f);
}

TEST_F(NormalizersTest, ZScoreBatchTransform) {
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};

    for (float val : data) {
        zscore_normalizer_fit(zscore, 0, val);
    }

    std::vector<float> outputs(data.size());
    size_t transformed = zscore_normalizer_transform_batch(
        zscore, 0, data.data(), outputs.data(), data.size());

    EXPECT_EQ(transformed, data.size());

    // Check all outputs are valid
    for (float val : outputs) {
        EXPECT_TRUE(std::isfinite(val));
    }
}

//=============================================================================
// Reset Tests
//=============================================================================

TEST_F(NormalizersTest, ResetChannelClearsState) {
    // Fit with data
    for (int i = 0; i < 10; i++) {
        zscore_normalizer_fit(zscore, 0, i * 1.0f);
    }

    zscore_stats_t stats_before;
    zscore_normalizer_get_stats(zscore, 0, &stats_before);
    EXPECT_GT(stats_before.sample_count, 0);

    // Reset
    zscore_normalizer_reset_channel(zscore, 0);

    zscore_stats_t stats_after;
    zscore_normalizer_get_stats(zscore, 0, &stats_after);
    EXPECT_EQ(stats_after.sample_count, 0);
}

TEST_F(NormalizersTest, ResetAllClearsAllChannels) {
    // Fit multiple channels
    for (size_t ch = 0; ch < NUM_CHANNELS; ch++) {
        for (int i = 0; i < 10; i++) {
            zscore_normalizer_fit(zscore, ch, i * 1.0f);
        }
    }

    // Reset all
    zscore_normalizer_reset_all(zscore);

    // Check all channels reset
    for (size_t ch = 0; ch < NUM_CHANNELS; ch++) {
        zscore_stats_t stats;
        zscore_normalizer_get_stats(zscore, ch, &stats);
        EXPECT_EQ(stats.sample_count, 0) << "Channel " << ch << " not reset";
    }
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(NormalizersTest, NullNormalizerHandling) {
    EXPECT_EQ(adaptive_normalizer_num_channels(nullptr), 0);
    EXPECT_EQ(homeostatic_normalizer_num_channels(nullptr), 0);
    EXPECT_EQ(minmax_normalizer_num_channels(nullptr), 0);
    EXPECT_EQ(zscore_normalizer_num_channels(nullptr), 0);
}

TEST_F(NormalizersTest, InvalidChannelIndex) {
    size_t invalid_ch = 9999;

    EXPECT_FALSE(adaptive_normalizer_reset_channel(adaptive, invalid_ch));
    EXPECT_FALSE(homeostatic_normalizer_reset_channel(homeostatic, invalid_ch));
    EXPECT_FALSE(minmax_normalizer_reset_channel(minmax, invalid_ch));
    EXPECT_FALSE(zscore_normalizer_reset_channel(zscore, invalid_ch));
}

TEST_F(NormalizersTest, SingleSample) {
    minmax_normalizer_fit(minmax, 0, 5.0f);
    zscore_normalizer_fit(zscore, 0, 5.0f);

    // Should handle gracefully
    float mm_result = minmax_normalizer_transform(minmax, 0, 5.0f);
    float z_result = zscore_normalizer_transform(zscore, 0, 5.0f);

    EXPECT_TRUE(std::isfinite(mm_result));
    EXPECT_TRUE(std::isfinite(z_result));
}

TEST_F(NormalizersTest, ZeroVariance) {
    // All same values
    for (int i = 0; i < 10; i++) {
        zscore_normalizer_fit(zscore, 0, 5.0f);
    }

    float result = zscore_normalizer_transform(zscore, 0, 5.0f);

    // Should handle zero variance gracefully
    EXPECT_TRUE(std::isfinite(result));
}

//=============================================================================
// Comparative Tests
//=============================================================================

TEST_F(NormalizersTest, AllNormalizesSameData) {
    std::vector<float> data = generateTestData(100, 10.0f, 2.0f);

    // Fit all normalizers with same data
    for (float val : data) {
        adaptive_normalizer_fit(adaptive, 0, val);
        minmax_normalizer_fit(minmax, 0, val);
        zscore_normalizer_fit(zscore, 0, val);
    }

    // Transform middle value
    float test_val = 10.0f;

    float adaptive_norm = adaptive_normalizer_transform(adaptive, 0, test_val);
    float minmax_norm = minmax_normalizer_transform(minmax, 0, test_val);
    float zscore_norm = zscore_normalizer_transform(zscore, 0, test_val);

    // All should produce finite results
    EXPECT_TRUE(std::isfinite(adaptive_norm));
    EXPECT_TRUE(std::isfinite(minmax_norm));
    EXPECT_TRUE(std::isfinite(zscore_norm));

    // MinMax should be in [0, 1]
    EXPECT_GE(minmax_norm, 0.0f);
    EXPECT_LE(minmax_norm, 1.0f);

    // ZScore of mean should be near 0
    EXPECT_NEAR(zscore_norm, 0.0f, 1.0f);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
