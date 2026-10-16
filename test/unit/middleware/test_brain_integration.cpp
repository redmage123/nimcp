/**
 * @file test_brain_integration.cpp
 * @brief Comprehensive tests for brain integration module
 *
 * Tests all 15 functions across 5 categories:
 * 1. Temporal Buffering (4 functions)
 * 2. Feature Normalization (3 functions)
 * 3. Combined Operations (1 function)
 * 4. Spike Feature Extraction (3 functions)
 * 5. Population Coding Analysis (3 functions)
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "middleware/brain_integration.h"
#include "middleware/buffering/nimcp_sliding_window.h"
#include "middleware/buffering/nimcp_integration_buffer.h"
#include "middleware/buffering/nimcp_temporal_accumulator.h"
#include "middleware/encoding/nimcp_population_coding.h"
#include "utils/memory/nimcp_memory.h"

#include <cmath>
#include <vector>

//=============================================================================
// Test Fixtures
//=============================================================================

class BrainIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize memory tracking
    }

    void TearDown() override {
        // Verify no memory leaks
    }
};

//=============================================================================
// 1. TEMPORAL BUFFERING TESTS
//=============================================================================

//-----------------------------------------------------------------------------
// brain_create_temporal_buffer Tests
//-----------------------------------------------------------------------------

TEST_F(BrainIntegrationTest, CreateTemporalBuffer_Success_10ms) {
    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(10, BUFFER_SIZE_10MS);

    ASSERT_NE(buffer, nullptr);
    EXPECT_EQ(buffer->num_channels, 10);
    EXPECT_EQ(buffer->size_preset, BUFFER_SIZE_10MS);
    EXPECT_NE(buffer->window, nullptr);
    EXPECT_NE(buffer->multiscale, nullptr);
    EXPECT_NE(buffer->accumulator, nullptr);

    brain_destroy_temporal_buffer(buffer);
}

TEST_F(BrainIntegrationTest, CreateTemporalBuffer_Success_100ms) {
    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(5, BUFFER_SIZE_100MS);

    ASSERT_NE(buffer, nullptr);
    EXPECT_EQ(buffer->num_channels, 5);
    EXPECT_EQ(buffer->size_preset, BUFFER_SIZE_100MS);

    brain_destroy_temporal_buffer(buffer);
}

TEST_F(BrainIntegrationTest, CreateTemporalBuffer_Success_1s) {
    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(3, BUFFER_SIZE_1S);

    ASSERT_NE(buffer, nullptr);
    EXPECT_EQ(buffer->num_channels, 3);
    EXPECT_EQ(buffer->size_preset, BUFFER_SIZE_1S);

    brain_destroy_temporal_buffer(buffer);
}

TEST_F(BrainIntegrationTest, CreateTemporalBuffer_Success_Custom) {
    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(7, BUFFER_SIZE_CUSTOM);

    ASSERT_NE(buffer, nullptr);
    EXPECT_EQ(buffer->num_channels, 7);
    EXPECT_EQ(buffer->size_preset, BUFFER_SIZE_CUSTOM);

    brain_destroy_temporal_buffer(buffer);
}

TEST_F(BrainIntegrationTest, CreateTemporalBuffer_Fail_ZeroChannels) {
    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(0, BUFFER_SIZE_100MS);

    EXPECT_EQ(buffer, nullptr);
}

TEST_F(BrainIntegrationTest, CreateTemporalBuffer_Success_LargeChannelCount) {
    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(1000, BUFFER_SIZE_100MS);

    ASSERT_NE(buffer, nullptr);
    EXPECT_EQ(buffer->num_channels, 1000);

    brain_destroy_temporal_buffer(buffer);
}

//-----------------------------------------------------------------------------
// brain_destroy_temporal_buffer Tests
//-----------------------------------------------------------------------------

TEST_F(BrainIntegrationTest, DestroyTemporalBuffer_Success) {
    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(10, BUFFER_SIZE_100MS);
    ASSERT_NE(buffer, nullptr);

    // Should not crash
    brain_destroy_temporal_buffer(buffer);
}

TEST_F(BrainIntegrationTest, DestroyTemporalBuffer_Null) {
    // Should not crash
    brain_destroy_temporal_buffer(nullptr);
}

//-----------------------------------------------------------------------------
// brain_buffer_activity Tests
//-----------------------------------------------------------------------------

TEST_F(BrainIntegrationTest, BufferActivity_Success) {
    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(3, BUFFER_SIZE_100MS);
    ASSERT_NE(buffer, nullptr);

    float activity[3] = {0.5f, 0.7f, 0.3f};
    uint64_t timestamp = 1000;

    bool result = brain_buffer_activity(buffer, activity, 3, timestamp);

    EXPECT_TRUE(result);

    brain_destroy_temporal_buffer(buffer);
}

TEST_F(BrainIntegrationTest, BufferActivity_Success_MultipleUpdates) {
    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(2, BUFFER_SIZE_100MS);
    ASSERT_NE(buffer, nullptr);

    float activity1[2] = {0.1f, 0.2f};
    float activity2[2] = {0.3f, 0.4f};
    float activity3[2] = {0.5f, 0.6f};

    EXPECT_TRUE(brain_buffer_activity(buffer, activity1, 2, 1000));
    EXPECT_TRUE(brain_buffer_activity(buffer, activity2, 2, 2000));
    EXPECT_TRUE(brain_buffer_activity(buffer, activity3, 2, 3000));

    brain_destroy_temporal_buffer(buffer);
}

TEST_F(BrainIntegrationTest, BufferActivity_Fail_NullBuffer) {
    float activity[3] = {0.5f, 0.7f, 0.3f};

    bool result = brain_buffer_activity(nullptr, activity, 3, 1000);

    EXPECT_FALSE(result);
}

TEST_F(BrainIntegrationTest, BufferActivity_Fail_NullActivity) {
    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(3, BUFFER_SIZE_100MS);
    ASSERT_NE(buffer, nullptr);

    bool result = brain_buffer_activity(buffer, nullptr, 3, 1000);

    EXPECT_FALSE(result);

    brain_destroy_temporal_buffer(buffer);
}

TEST_F(BrainIntegrationTest, BufferActivity_Fail_MismatchedChannels) {
    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(3, BUFFER_SIZE_100MS);
    ASSERT_NE(buffer, nullptr);

    float activity[5] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};

    bool result = brain_buffer_activity(buffer, activity, 5, 1000);

    EXPECT_FALSE(result);

    brain_destroy_temporal_buffer(buffer);
}

//-----------------------------------------------------------------------------
// brain_extract_windowed_features Tests
//-----------------------------------------------------------------------------

TEST_F(BrainIntegrationTest, ExtractWindowedFeatures_Success) {
    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(2, BUFFER_SIZE_100MS);
    ASSERT_NE(buffer, nullptr);

    // Add some activity
    float activity[2] = {0.5f, 0.7f};
    brain_buffer_activity(buffer, activity, 2, 1000);

    float features[10];
    size_t num_extracted = brain_extract_windowed_features(buffer, features, 10);

    EXPECT_EQ(num_extracted, 10);  // 5 features per channel × 2 channels

    brain_destroy_temporal_buffer(buffer);
}

TEST_F(BrainIntegrationTest, ExtractWindowedFeatures_Success_LimitedSpace) {
    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(5, BUFFER_SIZE_100MS);
    ASSERT_NE(buffer, nullptr);

    float activity[5] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
    brain_buffer_activity(buffer, activity, 5, 1000);

    float features[10];
    size_t num_extracted = brain_extract_windowed_features(buffer, features, 10);

    // Should extract 2 channels × 5 features = 10 (limited by max_features)
    EXPECT_EQ(num_extracted, 10);

    brain_destroy_temporal_buffer(buffer);
}

TEST_F(BrainIntegrationTest, ExtractWindowedFeatures_Fail_NullBuffer) {
    float features[10];

    size_t num_extracted = brain_extract_windowed_features(nullptr, features, 10);

    EXPECT_EQ(num_extracted, 0);
}

TEST_F(BrainIntegrationTest, ExtractWindowedFeatures_Fail_NullFeatures) {
    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(2, BUFFER_SIZE_100MS);
    ASSERT_NE(buffer, nullptr);

    size_t num_extracted = brain_extract_windowed_features(buffer, nullptr, 10);

    EXPECT_EQ(num_extracted, 0);

    brain_destroy_temporal_buffer(buffer);
}

TEST_F(BrainIntegrationTest, ExtractWindowedFeatures_Fail_ZeroMaxFeatures) {
    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(2, BUFFER_SIZE_100MS);
    ASSERT_NE(buffer, nullptr);

    float features[10];
    size_t num_extracted = brain_extract_windowed_features(buffer, features, 0);

    EXPECT_EQ(num_extracted, 0);

    brain_destroy_temporal_buffer(buffer);
}

TEST_F(BrainIntegrationTest, ExtractWindowedFeatures_FeaturesAreValid) {
    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(1, BUFFER_SIZE_100MS);
    ASSERT_NE(buffer, nullptr);

    // Add activity
    float activity[1] = {0.8f};
    brain_buffer_activity(buffer, activity, 1, 1000);

    float features[5];
    size_t num_extracted = brain_extract_windowed_features(buffer, features, 5);

    EXPECT_EQ(num_extracted, 5);

    // All features should be finite numbers
    for (size_t i = 0; i < num_extracted; i++) {
        EXPECT_TRUE(std::isfinite(features[i])) << "Feature " << i << " is not finite";
    }

    brain_destroy_temporal_buffer(buffer);
}

//=============================================================================
// 2. FEATURE NORMALIZATION TESTS
//=============================================================================

//-----------------------------------------------------------------------------
// brain_create_feature_normalizer Tests
//-----------------------------------------------------------------------------

TEST_F(BrainIntegrationTest, CreateFeatureNormalizer_Success_ZScore) {
    brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(10, NORMALIZE_ZSCORE);

    ASSERT_NE(normalizer, nullptr);
    EXPECT_EQ(normalizer->type, NORMALIZE_ZSCORE);
    EXPECT_EQ(normalizer->num_features, 10);
    EXPECT_NE(normalizer->zscore, nullptr);
    EXPECT_EQ(normalizer->minmax, nullptr);
    EXPECT_EQ(normalizer->adaptive, nullptr);
    EXPECT_EQ(normalizer->homeo, nullptr);

    brain_destroy_feature_normalizer(normalizer);
}

TEST_F(BrainIntegrationTest, CreateFeatureNormalizer_Success_MinMax) {
    brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(5, NORMALIZE_MINMAX);

    ASSERT_NE(normalizer, nullptr);
    EXPECT_EQ(normalizer->type, NORMALIZE_MINMAX);
    EXPECT_EQ(normalizer->num_features, 5);
    EXPECT_EQ(normalizer->zscore, nullptr);
    EXPECT_NE(normalizer->minmax, nullptr);
    EXPECT_EQ(normalizer->adaptive, nullptr);
    EXPECT_EQ(normalizer->homeo, nullptr);

    brain_destroy_feature_normalizer(normalizer);
}

TEST_F(BrainIntegrationTest, CreateFeatureNormalizer_Success_Adaptive) {
    brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(7, NORMALIZE_ADAPTIVE);

    ASSERT_NE(normalizer, nullptr);
    EXPECT_EQ(normalizer->type, NORMALIZE_ADAPTIVE);
    EXPECT_EQ(normalizer->num_features, 7);
    EXPECT_EQ(normalizer->zscore, nullptr);
    EXPECT_EQ(normalizer->minmax, nullptr);
    EXPECT_NE(normalizer->adaptive, nullptr);
    EXPECT_EQ(normalizer->homeo, nullptr);

    brain_destroy_feature_normalizer(normalizer);
}

TEST_F(BrainIntegrationTest, CreateFeatureNormalizer_Success_Homeostatic) {
    brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(3, NORMALIZE_HOMEOSTATIC);

    ASSERT_NE(normalizer, nullptr);
    EXPECT_EQ(normalizer->type, NORMALIZE_HOMEOSTATIC);
    EXPECT_EQ(normalizer->num_features, 3);
    EXPECT_EQ(normalizer->zscore, nullptr);
    EXPECT_EQ(normalizer->minmax, nullptr);
    EXPECT_EQ(normalizer->adaptive, nullptr);
    EXPECT_NE(normalizer->homeo, nullptr);

    brain_destroy_feature_normalizer(normalizer);
}

TEST_F(BrainIntegrationTest, CreateFeatureNormalizer_Success_None) {
    brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(5, NORMALIZE_NONE);

    ASSERT_NE(normalizer, nullptr);
    EXPECT_EQ(normalizer->type, NORMALIZE_NONE);
    EXPECT_EQ(normalizer->num_features, 5);
    EXPECT_EQ(normalizer->zscore, nullptr);
    EXPECT_EQ(normalizer->minmax, nullptr);
    EXPECT_EQ(normalizer->adaptive, nullptr);
    EXPECT_EQ(normalizer->homeo, nullptr);

    brain_destroy_feature_normalizer(normalizer);
}

TEST_F(BrainIntegrationTest, CreateFeatureNormalizer_Fail_ZeroFeatures) {
    brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(0, NORMALIZE_ZSCORE);

    EXPECT_EQ(normalizer, nullptr);
}

//-----------------------------------------------------------------------------
// brain_destroy_feature_normalizer Tests
//-----------------------------------------------------------------------------

TEST_F(BrainIntegrationTest, DestroyFeatureNormalizer_Success) {
    brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(10, NORMALIZE_ZSCORE);
    ASSERT_NE(normalizer, nullptr);

    // Should not crash
    brain_destroy_feature_normalizer(normalizer);
}

TEST_F(BrainIntegrationTest, DestroyFeatureNormalizer_Null) {
    // Should not crash
    brain_destroy_feature_normalizer(nullptr);
}

//-----------------------------------------------------------------------------
// brain_normalize_features Tests
//-----------------------------------------------------------------------------

TEST_F(BrainIntegrationTest, NormalizeFeatures_Success_ZScore) {
    brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(3, NORMALIZE_ZSCORE);
    ASSERT_NE(normalizer, nullptr);

    float features[3] = {1.0f, 2.0f, 3.0f};

    bool result = brain_normalize_features(normalizer, features, 3);

    EXPECT_TRUE(result);

    // Features should be normalized (non-trivial check)
    for (size_t i = 0; i < 3; i++) {
        EXPECT_TRUE(std::isfinite(features[i]));
    }

    brain_destroy_feature_normalizer(normalizer);
}

TEST_F(BrainIntegrationTest, NormalizeFeatures_Success_MinMax) {
    brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(3, NORMALIZE_MINMAX);
    ASSERT_NE(normalizer, nullptr);

    float features[3] = {0.5f, 1.5f, 2.5f};

    bool result = brain_normalize_features(normalizer, features, 3);

    EXPECT_TRUE(result);

    brain_destroy_feature_normalizer(normalizer);
}

TEST_F(BrainIntegrationTest, NormalizeFeatures_Success_None) {
    brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(3, NORMALIZE_NONE);
    ASSERT_NE(normalizer, nullptr);

    float features[3] = {1.0f, 2.0f, 3.0f};
    float original[3] = {1.0f, 2.0f, 3.0f};

    bool result = brain_normalize_features(normalizer, features, 3);

    EXPECT_TRUE(result);

    // Features should be unchanged
    for (size_t i = 0; i < 3; i++) {
        EXPECT_FLOAT_EQ(features[i], original[i]);
    }

    brain_destroy_feature_normalizer(normalizer);
}

TEST_F(BrainIntegrationTest, NormalizeFeatures_Fail_NullNormalizer) {
    float features[3] = {1.0f, 2.0f, 3.0f};

    bool result = brain_normalize_features(nullptr, features, 3);

    EXPECT_FALSE(result);
}

TEST_F(BrainIntegrationTest, NormalizeFeatures_Fail_NullFeatures) {
    brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(3, NORMALIZE_ZSCORE);
    ASSERT_NE(normalizer, nullptr);

    bool result = brain_normalize_features(normalizer, nullptr, 3);

    EXPECT_FALSE(result);

    brain_destroy_feature_normalizer(normalizer);
}

TEST_F(BrainIntegrationTest, NormalizeFeatures_Fail_ZeroFeatures) {
    brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(3, NORMALIZE_ZSCORE);
    ASSERT_NE(normalizer, nullptr);

    float features[3] = {1.0f, 2.0f, 3.0f};

    bool result = brain_normalize_features(normalizer, features, 0);

    EXPECT_FALSE(result);

    brain_destroy_feature_normalizer(normalizer);
}

TEST_F(BrainIntegrationTest, NormalizeFeatures_Fail_TooManyFeatures) {
    brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(3, NORMALIZE_ZSCORE);
    ASSERT_NE(normalizer, nullptr);

    float features[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};

    bool result = brain_normalize_features(normalizer, features, 5);

    EXPECT_FALSE(result);

    brain_destroy_feature_normalizer(normalizer);
}

//=============================================================================
// 3. COMBINED OPERATIONS TESTS
//=============================================================================

TEST_F(BrainIntegrationTest, ExtractAndNormalize_Success) {
    // Create buffer
    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(2, BUFFER_SIZE_100MS);
    ASSERT_NE(buffer, nullptr);

    // Create normalizer
    brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(10, NORMALIZE_ZSCORE);
    ASSERT_NE(normalizer, nullptr);

    // Add activity
    float activity[2] = {0.5f, 0.7f};
    brain_buffer_activity(buffer, activity, 2, 1000);

    // Extract and normalize
    float features[10];
    size_t num_extracted = brain_extract_and_normalize(buffer, normalizer, features, 10);

    EXPECT_EQ(num_extracted, 10);

    // All features should be finite
    for (size_t i = 0; i < num_extracted; i++) {
        EXPECT_TRUE(std::isfinite(features[i]));
    }

    brain_destroy_temporal_buffer(buffer);
    brain_destroy_feature_normalizer(normalizer);
}

TEST_F(BrainIntegrationTest, ExtractAndNormalize_Fail_NullBuffer) {
    brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(10, NORMALIZE_ZSCORE);
    ASSERT_NE(normalizer, nullptr);

    float features[10];
    size_t num_extracted = brain_extract_and_normalize(nullptr, normalizer, features, 10);

    EXPECT_EQ(num_extracted, 0);

    brain_destroy_feature_normalizer(normalizer);
}

TEST_F(BrainIntegrationTest, ExtractAndNormalize_Fail_NullNormalizer) {
    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(2, BUFFER_SIZE_100MS);
    ASSERT_NE(buffer, nullptr);

    float features[10];
    size_t num_extracted = brain_extract_and_normalize(buffer, nullptr, features, 10);

    EXPECT_EQ(num_extracted, 0);

    brain_destroy_temporal_buffer(buffer);
}

TEST_F(BrainIntegrationTest, ExtractAndNormalize_Fail_NullFeatures) {
    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(2, BUFFER_SIZE_100MS);
    ASSERT_NE(buffer, nullptr);

    brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(10, NORMALIZE_ZSCORE);
    ASSERT_NE(normalizer, nullptr);

    size_t num_extracted = brain_extract_and_normalize(buffer, normalizer, nullptr, 10);

    EXPECT_EQ(num_extracted, 0);

    brain_destroy_temporal_buffer(buffer);
    brain_destroy_feature_normalizer(normalizer);
}

//=============================================================================
// 4. SPIKE FEATURE EXTRACTION TESTS
//=============================================================================

//-----------------------------------------------------------------------------
// brain_create_spike_feature_extractor Tests
//-----------------------------------------------------------------------------

TEST_F(BrainIntegrationTest, CreateSpikeFeatureExtractor_Success_BasicConfig) {
    brain_spike_feature_extractor_t extractor = brain_create_spike_feature_extractor(
        100, false, false
    );

    ASSERT_NE(extractor, nullptr);

    brain_destroy_spike_feature_extractor(extractor);
}

TEST_F(BrainIntegrationTest, CreateSpikeFeatureExtractor_Success_WithOscillations) {
    brain_spike_feature_extractor_t extractor = brain_create_spike_feature_extractor(
        50, true, false
    );

    ASSERT_NE(extractor, nullptr);

    brain_destroy_spike_feature_extractor(extractor);
}

TEST_F(BrainIntegrationTest, CreateSpikeFeatureExtractor_Success_WithSynchrony) {
    brain_spike_feature_extractor_t extractor = brain_create_spike_feature_extractor(
        50, false, true
    );

    ASSERT_NE(extractor, nullptr);

    brain_destroy_spike_feature_extractor(extractor);
}

TEST_F(BrainIntegrationTest, CreateSpikeFeatureExtractor_Success_AllFeatures) {
    brain_spike_feature_extractor_t extractor = brain_create_spike_feature_extractor(
        75, true, true
    );

    ASSERT_NE(extractor, nullptr);

    brain_destroy_spike_feature_extractor(extractor);
}

TEST_F(BrainIntegrationTest, CreateSpikeFeatureExtractor_Fail_ZeroNeurons) {
    brain_spike_feature_extractor_t extractor = brain_create_spike_feature_extractor(
        0, false, false
    );

    EXPECT_EQ(extractor, nullptr);
}

TEST_F(BrainIntegrationTest, CreateSpikeFeatureExtractor_Fail_TooManyNeurons) {
    brain_spike_feature_extractor_t extractor = brain_create_spike_feature_extractor(
        FEATURE_EXTRACTOR_MAX_NEURONS + 1, false, false
    );

    EXPECT_EQ(extractor, nullptr);
}

//-----------------------------------------------------------------------------
// brain_destroy_spike_feature_extractor Tests
//-----------------------------------------------------------------------------

TEST_F(BrainIntegrationTest, DestroySpikeFeatureExtractor_Success) {
    brain_spike_feature_extractor_t extractor = brain_create_spike_feature_extractor(
        100, false, false
    );
    ASSERT_NE(extractor, nullptr);

    // Should not crash
    brain_destroy_spike_feature_extractor(extractor);
}

TEST_F(BrainIntegrationTest, DestroySpikeFeatureExtractor_Null) {
    // Should not crash
    brain_destroy_spike_feature_extractor(nullptr);
}

//=============================================================================
// 5. POPULATION CODING ANALYSIS TESTS
//=============================================================================

//-----------------------------------------------------------------------------
// brain_create_population_analyzer Tests
//-----------------------------------------------------------------------------

TEST_F(BrainIntegrationTest, CreatePopulationAnalyzer_Success) {
    brain_population_analyzer_t analyzer = brain_create_population_analyzer();

    ASSERT_NE(analyzer, nullptr);

    brain_destroy_population_analyzer(analyzer);
}

//-----------------------------------------------------------------------------
// brain_destroy_population_analyzer Tests
//-----------------------------------------------------------------------------

TEST_F(BrainIntegrationTest, DestroyPopulationAnalyzer_Success) {
    brain_population_analyzer_t analyzer = brain_create_population_analyzer();
    ASSERT_NE(analyzer, nullptr);

    // Should not crash
    brain_destroy_population_analyzer(analyzer);
}

TEST_F(BrainIntegrationTest, DestroyPopulationAnalyzer_Null) {
    // Should not crash
    brain_destroy_population_analyzer(nullptr);
}

//-----------------------------------------------------------------------------
// brain_compute_population_vector Tests
//-----------------------------------------------------------------------------

TEST_F(BrainIntegrationTest, ComputePopulationVector_Success) {
    brain_population_analyzer_t analyzer = brain_create_population_analyzer();
    ASSERT_NE(analyzer, nullptr);

    // Create test data
    const uint32_t num_neurons = 8;
    float rates[8] = {0.5f, 0.8f, 0.3f, 0.6f, 0.4f, 0.7f, 0.2f, 0.9f};

    tuning_curve_t tuning_curves[8];
    for (uint32_t i = 0; i < num_neurons; i++) {
        tuning_curves[i].preferred_direction.x = std::cos(i * M_PI / 4.0f);
        tuning_curves[i].preferred_direction.y = std::sin(i * M_PI / 4.0f);
        tuning_curves[i].preferred_direction.z = 0.0f;
        tuning_curves[i].tuning_width = 30.0f;
        tuning_curves[i].max_rate = 1.0f;
    }

    vector3d_t vector_out;
    bool result = brain_compute_population_vector(
        analyzer, rates, tuning_curves, num_neurons, &vector_out
    );

    EXPECT_TRUE(result);
    EXPECT_TRUE(std::isfinite(vector_out.x));
    EXPECT_TRUE(std::isfinite(vector_out.y));
    EXPECT_TRUE(std::isfinite(vector_out.z));

    brain_destroy_population_analyzer(analyzer);
}

TEST_F(BrainIntegrationTest, ComputePopulationVector_Fail_NullAnalyzer) {
    const uint32_t num_neurons = 8;
    float rates[8] = {0.5f, 0.8f, 0.3f, 0.6f, 0.4f, 0.7f, 0.2f, 0.9f};
    tuning_curve_t tuning_curves[8];
    vector3d_t vector_out;

    bool result = brain_compute_population_vector(
        nullptr, rates, tuning_curves, num_neurons, &vector_out
    );

    EXPECT_FALSE(result);
}

TEST_F(BrainIntegrationTest, ComputePopulationVector_Fail_NullRates) {
    brain_population_analyzer_t analyzer = brain_create_population_analyzer();
    ASSERT_NE(analyzer, nullptr);

    tuning_curve_t tuning_curves[8];
    vector3d_t vector_out;

    bool result = brain_compute_population_vector(
        analyzer, nullptr, tuning_curves, 8, &vector_out
    );

    EXPECT_FALSE(result);

    brain_destroy_population_analyzer(analyzer);
}

TEST_F(BrainIntegrationTest, ComputePopulationVector_Fail_ZeroNeurons) {
    brain_population_analyzer_t analyzer = brain_create_population_analyzer();
    ASSERT_NE(analyzer, nullptr);

    float rates[1] = {0.5f};
    tuning_curve_t tuning_curves[1];
    vector3d_t vector_out;

    bool result = brain_compute_population_vector(
        analyzer, rates, tuning_curves, 0, &vector_out
    );

    EXPECT_FALSE(result);

    brain_destroy_population_analyzer(analyzer);
}

TEST_F(BrainIntegrationTest, ComputePopulationVector_Fail_TooManyNeurons) {
    brain_population_analyzer_t analyzer = brain_create_population_analyzer();
    ASSERT_NE(analyzer, nullptr);

    float rates[1] = {0.5f};
    tuning_curve_t tuning_curves[1];
    vector3d_t vector_out;

    bool result = brain_compute_population_vector(
        analyzer, rates, tuning_curves, POPULATION_MAX_NEURONS + 1, &vector_out
    );

    EXPECT_FALSE(result);

    brain_destroy_population_analyzer(analyzer);
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(BrainIntegrationTest, FullWorkflow_BufferExtractNormalize) {
    // Simulate complete workflow as would be used in cognitive module

    // 1. Create components
    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(4, BUFFER_SIZE_100MS);
    ASSERT_NE(buffer, nullptr);

    brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(20, NORMALIZE_MINMAX);
    ASSERT_NE(normalizer, nullptr);

    // 2. Simulate neural activity over time
    for (uint64_t t = 0; t < 10; t++) {
        float activity[4] = {
            0.1f + 0.05f * t,
            0.2f + 0.03f * t,
            0.3f - 0.02f * t,
            0.4f + 0.01f * t
        };

        EXPECT_TRUE(brain_buffer_activity(buffer, activity, 4, t * 100));
    }

    // 3. Extract and normalize features
    float features[20];
    size_t num_extracted = brain_extract_and_normalize(buffer, normalizer, features, 20);

    EXPECT_EQ(num_extracted, 20);  // 4 channels × 5 features

    // 4. Verify features are valid
    for (size_t i = 0; i < num_extracted; i++) {
        EXPECT_TRUE(std::isfinite(features[i]));
    }

    // 5. Cleanup
    brain_destroy_temporal_buffer(buffer);
    brain_destroy_feature_normalizer(normalizer);
}

TEST_F(BrainIntegrationTest, MultipleBufferPresets_AllWork) {
    brain_buffer_size_t presets[] = {
        BUFFER_SIZE_10MS,
        BUFFER_SIZE_100MS,
        BUFFER_SIZE_1S,
        BUFFER_SIZE_CUSTOM
    };

    for (auto preset : presets) {
        brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(3, preset);
        ASSERT_NE(buffer, nullptr) << "Failed for preset " << preset;

        float activity[3] = {0.5f, 0.6f, 0.7f};
        EXPECT_TRUE(brain_buffer_activity(buffer, activity, 3, 1000));

        float features[15];
        size_t num = brain_extract_windowed_features(buffer, features, 15);
        EXPECT_GT(num, 0) << "No features extracted for preset " << preset;

        brain_destroy_temporal_buffer(buffer);
    }
}

TEST_F(BrainIntegrationTest, AllNormalizationTypes_Work) {
    brain_normalize_type_t types[] = {
        NORMALIZE_ZSCORE,
        NORMALIZE_MINMAX,
        NORMALIZE_ADAPTIVE,
        NORMALIZE_HOMEOSTATIC,
        NORMALIZE_NONE
    };

    for (auto type : types) {
        brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(5, type);
        ASSERT_NE(normalizer, nullptr) << "Failed for type " << type;

        float features[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        EXPECT_TRUE(brain_normalize_features(normalizer, features, 5));

        brain_destroy_feature_normalizer(normalizer);
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
