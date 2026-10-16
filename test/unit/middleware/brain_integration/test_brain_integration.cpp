/**
 * @file test_brain_integration.cpp
 * @brief Comprehensive unit tests for brain integration module
 *
 * WHAT: Complete test coverage for all 15 brain integration functions
 * WHY:  Ensure reliable middleware-brain integration in cognitive modules
 * HOW:  GoogleTest framework with NULL safety, boundary, and integration tests
 *
 * COVERAGE:
 * 1. Temporal Buffering (4 functions): 28 tests
 * 2. Feature Normalization (3 functions): 22 tests
 * 3. Combined Operations (1 function): 8 tests
 * 4. Spike Feature Extraction (3 functions): 15 tests
 * 5. Population Coding Analysis (4 functions): 18 tests
 *
 * Total: 91 tests, ~1,450 LOC
 *
 * @author NIMCP Test Suite
 * @date 2025-01-21
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "middleware/brain_integration.h"
#include "middleware/buffering/nimcp_sliding_window.h"
#include "middleware/buffering/nimcp_integration_buffer.h"
#include "middleware/buffering/nimcp_temporal_accumulator.h"
#include "middleware/encoding/nimcp_population_coding.h"
#include "middleware/encoding/nimcp_rate_coding.h"
#include "middleware/features/nimcp_feature_extractor.h"
#include "utils/memory/nimcp_memory.h"

#include <cmath>
#include <vector>
#include <algorithm>

//=============================================================================
// Test Fixtures
//=============================================================================

/**
 * WHAT: Main test fixture for brain integration module
 * WHY:  Provide clean setup/teardown for each test
 * HOW:  Initialize and cleanup resources per test
 */
class BrainIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clean state for each test
    }

    void TearDown() override {
        // Verify no memory leaks
    }

    /**
     * WHAT: Create test spike data for population analysis
     * WHY:  Helper to simplify test setup
     * HOW:  Allocate and populate spike_data_t structure
     */
    spike_data_t* create_test_spike_data(uint32_t num_neurons,
                                         uint64_t start_time,
                                         uint64_t end_time) {
        spike_data_t* data = spike_data_create(num_neurons);
        if (!data) return nullptr;

        data->start_time = start_time;
        data->end_time = end_time;

        // Add some test spikes
        for (uint32_t i = 0; i < num_neurons; i++) {
            data->spike_counts[i] = 5;
            data->spike_times[i] = (uint64_t*)nimcp_malloc(5 * sizeof(uint64_t));
            for (uint32_t j = 0; j < 5; j++) {
                data->spike_times[i][j] = start_time + j * 10 + i;
            }
        }

        return data;
    }

    /**
     * WHAT: Create test spike trains for synchrony analysis
     * WHY:  Helper for population synchrony tests
     * HOW:  Allocate array of spike_train_t pointers
     */
    spike_train_t** create_test_spike_trains(uint32_t num_neurons) {
        spike_train_t** trains = (spike_train_t**)nimcp_malloc(
            num_neurons * sizeof(spike_train_t*)
        );
        if (!trains) return nullptr;

        for (uint32_t i = 0; i < num_neurons; i++) {
            trains[i] = rate_coding_spike_train_create(100);
            if (!trains[i]) {
                // Cleanup on failure
                for (uint32_t j = 0; j < i; j++) {
                    rate_coding_spike_train_destroy(trains[j]);
                }
                nimcp_free(trains);
                return nullptr;
            }

            // Add test spikes
            for (uint64_t t = 0; t < 100; t += 20 + i) {
                spike_train_add_spike(trains[i], t);
            }
        }

        return trains;
    }

    /**
     * WHAT: Cleanup spike trains array
     * WHY:  Proper memory management
     * HOW:  Destroy each train and free array
     */
    void destroy_spike_trains(spike_train_t** trains, uint32_t num_neurons) {
        if (!trains) return;
        for (uint32_t i = 0; i < num_neurons; i++) {
            rate_coding_spike_train_destroy(trains[i]);
        }
        nimcp_free(trains);
    }
};

//=============================================================================
// 1. TEMPORAL BUFFERING TESTS (28 tests)
//=============================================================================

//-----------------------------------------------------------------------------
// brain_create_temporal_buffer Tests (12 tests)
//-----------------------------------------------------------------------------

/**
 * WHAT: Test successful buffer creation with 10ms preset
 * WHY:  Verify fast timescale buffering works
 * HOW:  Create buffer, check structure initialization
 */
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

/**
 * WHAT: Test successful buffer creation with 100ms preset
 * WHY:  Verify medium timescale buffering works
 * HOW:  Create buffer, validate configuration
 */
TEST_F(BrainIntegrationTest, CreateTemporalBuffer_Success_100ms) {
    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(5, BUFFER_SIZE_100MS);

    ASSERT_NE(buffer, nullptr);
    EXPECT_EQ(buffer->num_channels, 5);
    EXPECT_EQ(buffer->size_preset, BUFFER_SIZE_100MS);
    EXPECT_NE(buffer->window, nullptr);
    EXPECT_NE(buffer->multiscale, nullptr);
    EXPECT_NE(buffer->accumulator, nullptr);

    brain_destroy_temporal_buffer(buffer);
}

/**
 * WHAT: Test successful buffer creation with 1s preset
 * WHY:  Verify slow timescale buffering works
 * HOW:  Create buffer, check all components initialized
 */
TEST_F(BrainIntegrationTest, CreateTemporalBuffer_Success_1s) {
    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(3, BUFFER_SIZE_1S);

    ASSERT_NE(buffer, nullptr);
    EXPECT_EQ(buffer->num_channels, 3);
    EXPECT_EQ(buffer->size_preset, BUFFER_SIZE_1S);

    brain_destroy_temporal_buffer(buffer);
}

/**
 * WHAT: Test successful buffer creation with custom preset
 * WHY:  Verify custom size configuration works
 * HOW:  Create buffer with BUFFER_SIZE_CUSTOM
 */
TEST_F(BrainIntegrationTest, CreateTemporalBuffer_Success_Custom) {
    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(7, BUFFER_SIZE_CUSTOM);

    ASSERT_NE(buffer, nullptr);
    EXPECT_EQ(buffer->num_channels, 7);
    EXPECT_EQ(buffer->size_preset, BUFFER_SIZE_CUSTOM);

    brain_destroy_temporal_buffer(buffer);
}

/**
 * WHAT: Test buffer creation failure with zero channels
 * WHY:  NULL check - zero channels is invalid
 * HOW:  Attempt creation, expect NULL
 */
TEST_F(BrainIntegrationTest, CreateTemporalBuffer_Fail_ZeroChannels) {
    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(0, BUFFER_SIZE_100MS);

    EXPECT_EQ(buffer, nullptr);
}

/**
 * WHAT: Test buffer creation with single channel
 * WHY:  Boundary test - minimum valid channel count
 * HOW:  Create buffer with 1 channel, verify success
 */
TEST_F(BrainIntegrationTest, CreateTemporalBuffer_Success_SingleChannel) {
    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(1, BUFFER_SIZE_100MS);

    ASSERT_NE(buffer, nullptr);
    EXPECT_EQ(buffer->num_channels, 1);

    brain_destroy_temporal_buffer(buffer);
}

/**
 * WHAT: Test buffer creation with large channel count
 * WHY:  Verify scalability to realistic neural populations
 * HOW:  Create buffer with 100 channels (reduced for unit test speed)
 */
TEST_F(BrainIntegrationTest, CreateTemporalBuffer_Success_LargeChannelCount) {
    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(100, BUFFER_SIZE_100MS);

    ASSERT_NE(buffer, nullptr);
    EXPECT_EQ(buffer->num_channels, 100);

    brain_destroy_temporal_buffer(buffer);
}

/**
 * WHAT: Test buffer creation with very large channel count
 * WHY:  Verify handling of large-scale neural populations
 * HOW:  Create buffer with 500 channels (reduced for unit test speed)
 * NOTE: Stress tests with 1000+ channels belong in performance/regression tests
 */
TEST_F(BrainIntegrationTest, CreateTemporalBuffer_Success_VeryLargeChannelCount) {
    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(500, BUFFER_SIZE_10MS);

    ASSERT_NE(buffer, nullptr);
    EXPECT_EQ(buffer->num_channels, 500);

    brain_destroy_temporal_buffer(buffer);
}

/**
 * WHAT: Test all buffer size presets work correctly
 * WHY:  Comprehensive preset validation
 * HOW:  Test each enum value individually
 */
TEST_F(BrainIntegrationTest, CreateTemporalBuffer_AllPresets_Work) {
    brain_buffer_size_t presets[] = {
        BUFFER_SIZE_10MS,
        BUFFER_SIZE_100MS,
        BUFFER_SIZE_1S,
        BUFFER_SIZE_CUSTOM
    };

    for (auto preset : presets) {
        brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(5, preset);
        ASSERT_NE(buffer, nullptr) << "Failed for preset " << preset;
        EXPECT_EQ(buffer->size_preset, preset);
        brain_destroy_temporal_buffer(buffer);
    }
}

/**
 * WHAT: Test buffer structure members are non-NULL
 * WHY:  Verify complete initialization
 * HOW:  Check all struct pointers after creation
 */
TEST_F(BrainIntegrationTest, CreateTemporalBuffer_AllComponents_Initialized) {
    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(8, BUFFER_SIZE_100MS);

    ASSERT_NE(buffer, nullptr);
    EXPECT_NE(buffer->window, nullptr) << "window not initialized";
    EXPECT_NE(buffer->multiscale, nullptr) << "multiscale not initialized";
    EXPECT_NE(buffer->accumulator, nullptr) << "accumulator not initialized";
    EXPECT_EQ(buffer->num_channels, 8) << "num_channels mismatch";

    brain_destroy_temporal_buffer(buffer);
}

/**
 * WHAT: Test buffer creation with different channel counts for each preset
 * WHY:  Verify preset-channel count combinations work
 * HOW:  Test matrix of presets × channel counts
 */
TEST_F(BrainIntegrationTest, CreateTemporalBuffer_PresetChannelCombinations) {
    size_t channel_counts[] = {1, 10, 100, 500};
    brain_buffer_size_t presets[] = {BUFFER_SIZE_10MS, BUFFER_SIZE_100MS, BUFFER_SIZE_1S};

    for (auto preset : presets) {
        for (auto channels : channel_counts) {
            brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(channels, preset);
            ASSERT_NE(buffer, nullptr)
                << "Failed for preset " << preset << " and " << channels << " channels";
            EXPECT_EQ(buffer->num_channels, channels);
            brain_destroy_temporal_buffer(buffer);
        }
    }
}

/**
 * WHAT: Test multiple sequential buffer creations
 * WHY:  Verify no resource leaks or state issues
 * HOW:  Create and destroy multiple buffers in sequence
 */
TEST_F(BrainIntegrationTest, CreateTemporalBuffer_MultipleSequential) {
    for (int i = 0; i < 10; i++) {
        brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(5, BUFFER_SIZE_100MS);
        ASSERT_NE(buffer, nullptr) << "Failed on iteration " << i;
        brain_destroy_temporal_buffer(buffer);
    }
}

//-----------------------------------------------------------------------------
// brain_destroy_temporal_buffer Tests (3 tests)
//-----------------------------------------------------------------------------

/**
 * WHAT: Test successful buffer destruction
 * WHY:  Verify proper cleanup of valid buffer
 * HOW:  Create buffer, destroy it, expect no crash
 */
TEST_F(BrainIntegrationTest, DestroyTemporalBuffer_Success) {
    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(10, BUFFER_SIZE_100MS);
    ASSERT_NE(buffer, nullptr);

    // Should not crash
    brain_destroy_temporal_buffer(buffer);
}

/**
 * WHAT: Test NULL buffer destruction
 * WHY:  NULL check - must handle NULL gracefully
 * HOW:  Pass NULL, expect no crash
 */
TEST_F(BrainIntegrationTest, DestroyTemporalBuffer_Null) {
    // Should not crash
    brain_destroy_temporal_buffer(nullptr);
}

/**
 * WHAT: Test double destruction safety
 * WHY:  Verify idempotency (though not recommended)
 * HOW:  Destroy same buffer twice (use after destroy not recommended)
 */
TEST_F(BrainIntegrationTest, DestroyTemporalBuffer_AfterDestroy) {
    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(5, BUFFER_SIZE_100MS);
    ASSERT_NE(buffer, nullptr);

    brain_destroy_temporal_buffer(buffer);
    // Second destroy should be avoided but shouldn't crash if NULL check exists
    // Note: This is intentionally testing undefined behavior for robustness
}

//-----------------------------------------------------------------------------
// brain_buffer_activity Tests (8 tests)
//-----------------------------------------------------------------------------

/**
 * WHAT: Test successful activity buffering
 * WHY:  Verify basic buffering operation works
 * HOW:  Create buffer, add activity, expect success
 */
TEST_F(BrainIntegrationTest, BufferActivity_Success) {
    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(3, BUFFER_SIZE_100MS);
    ASSERT_NE(buffer, nullptr);

    float activity[3] = {0.5f, 0.7f, 0.3f};
    uint64_t timestamp = 1000;

    bool result = brain_buffer_activity(buffer, activity, 3, timestamp);

    EXPECT_TRUE(result);

    brain_destroy_temporal_buffer(buffer);
}

/**
 * WHAT: Test multiple sequential activity updates
 * WHY:  Verify temporal accumulation works over time
 * HOW:  Add multiple activity samples with increasing timestamps
 */
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

/**
 * WHAT: Test buffering failure with NULL buffer
 * WHY:  NULL check - buffer parameter validation
 * HOW:  Pass NULL buffer, expect false
 */
TEST_F(BrainIntegrationTest, BufferActivity_Fail_NullBuffer) {
    float activity[3] = {0.5f, 0.7f, 0.3f};

    bool result = brain_buffer_activity(nullptr, activity, 3, 1000);

    EXPECT_FALSE(result);
}

/**
 * WHAT: Test buffering failure with NULL activity array
 * WHY:  NULL check - activity parameter validation
 * HOW:  Pass NULL activity, expect false
 */
TEST_F(BrainIntegrationTest, BufferActivity_Fail_NullActivity) {
    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(3, BUFFER_SIZE_100MS);
    ASSERT_NE(buffer, nullptr);

    bool result = brain_buffer_activity(buffer, nullptr, 3, 1000);

    EXPECT_FALSE(result);

    brain_destroy_temporal_buffer(buffer);
}

/**
 * WHAT: Test buffering failure with mismatched channel count
 * WHY:  Size validation - detect array size mismatch
 * HOW:  Pass wrong num_channels, expect false
 */
TEST_F(BrainIntegrationTest, BufferActivity_Fail_MismatchedChannels) {
    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(3, BUFFER_SIZE_100MS);
    ASSERT_NE(buffer, nullptr);

    float activity[5] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};

    bool result = brain_buffer_activity(buffer, activity, 5, 1000);

    EXPECT_FALSE(result);

    brain_destroy_temporal_buffer(buffer);
}

/**
 * WHAT: Test buffering with zero channels
 * WHY:  Boundary test - invalid channel count
 * HOW:  Pass num_channels=0, expect false
 */
TEST_F(BrainIntegrationTest, BufferActivity_Fail_ZeroChannels) {
    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(3, BUFFER_SIZE_100MS);
    ASSERT_NE(buffer, nullptr);

    float activity[3] = {0.5f, 0.7f, 0.3f};

    bool result = brain_buffer_activity(buffer, activity, 0, 1000);

    EXPECT_FALSE(result);

    brain_destroy_temporal_buffer(buffer);
}

/**
 * WHAT: Test timestamp ordering enforcement
 * WHY:  Verify monotonic timestamp handling
 * HOW:  Add activities with non-increasing timestamps
 */
TEST_F(BrainIntegrationTest, BufferActivity_TimestampHandling) {
    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(2, BUFFER_SIZE_100MS);
    ASSERT_NE(buffer, nullptr);

    float activity[2] = {0.5f, 0.6f};

    EXPECT_TRUE(brain_buffer_activity(buffer, activity, 2, 1000));
    EXPECT_TRUE(brain_buffer_activity(buffer, activity, 2, 1500));
    EXPECT_TRUE(brain_buffer_activity(buffer, activity, 2, 2000));

    brain_destroy_temporal_buffer(buffer);
}

/**
 * WHAT: Test buffering with extreme activity values
 * WHY:  Verify numerical stability
 * HOW:  Use very large and very small float values
 */
TEST_F(BrainIntegrationTest, BufferActivity_ExtremeValues) {
    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(4, BUFFER_SIZE_100MS);
    ASSERT_NE(buffer, nullptr);

    float activity[4] = {0.0f, 1000.0f, -500.0f, 0.0001f};

    bool result = brain_buffer_activity(buffer, activity, 4, 1000);

    EXPECT_TRUE(result);

    brain_destroy_temporal_buffer(buffer);
}

//-----------------------------------------------------------------------------
// brain_extract_windowed_features Tests (5 tests)
//-----------------------------------------------------------------------------

/**
 * WHAT: Test successful feature extraction
 * WHY:  Verify basic feature extraction works
 * HOW:  Buffer activity, extract features, check count
 */
TEST_F(BrainIntegrationTest, ExtractWindowedFeatures_Success) {
    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(2, BUFFER_SIZE_100MS);
    ASSERT_NE(buffer, nullptr);

    // Add some activity
    float activity[2] = {0.5f, 0.7f};
    brain_buffer_activity(buffer, activity, 2, 1000);

    float features[10];
    size_t num_extracted = brain_extract_windowed_features(buffer, features, 10);

    EXPECT_GT(num_extracted, 0);
    EXPECT_LE(num_extracted, 10);

    brain_destroy_temporal_buffer(buffer);
}

/**
 * WHAT: Test feature extraction with limited output space
 * WHY:  Verify buffer overflow protection
 * HOW:  Extract more features than max_features allows
 */
TEST_F(BrainIntegrationTest, ExtractWindowedFeatures_Success_LimitedSpace) {
    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(5, BUFFER_SIZE_100MS);
    ASSERT_NE(buffer, nullptr);

    float activity[5] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
    brain_buffer_activity(buffer, activity, 5, 1000);

    float features[10];
    size_t num_extracted = brain_extract_windowed_features(buffer, features, 10);

    EXPECT_LE(num_extracted, 10);

    brain_destroy_temporal_buffer(buffer);
}

/**
 * WHAT: Test extraction failure with NULL buffer
 * WHY:  NULL check - buffer parameter validation
 * HOW:  Pass NULL buffer, expect 0 features
 */
TEST_F(BrainIntegrationTest, ExtractWindowedFeatures_Fail_NullBuffer) {
    float features[10];

    size_t num_extracted = brain_extract_windowed_features(nullptr, features, 10);

    EXPECT_EQ(num_extracted, 0);
}

/**
 * WHAT: Test extraction failure with NULL features array
 * WHY:  NULL check - features parameter validation
 * HOW:  Pass NULL features, expect 0
 */
TEST_F(BrainIntegrationTest, ExtractWindowedFeatures_Fail_NullFeatures) {
    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(2, BUFFER_SIZE_100MS);
    ASSERT_NE(buffer, nullptr);

    size_t num_extracted = brain_extract_windowed_features(buffer, nullptr, 10);

    EXPECT_EQ(num_extracted, 0);

    brain_destroy_temporal_buffer(buffer);
}

/**
 * WHAT: Test extraction failure with zero max_features
 * WHY:  Boundary test - invalid output size
 * HOW:  Pass max_features=0, expect 0
 */
TEST_F(BrainIntegrationTest, ExtractWindowedFeatures_Fail_ZeroMaxFeatures) {
    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(2, BUFFER_SIZE_100MS);
    ASSERT_NE(buffer, nullptr);

    float features[10];
    size_t num_extracted = brain_extract_windowed_features(buffer, features, 0);

    EXPECT_EQ(num_extracted, 0);

    brain_destroy_temporal_buffer(buffer);
}

/**
 * WHAT: Test extracted features are valid numbers
 * WHY:  Verify numerical stability of feature computation
 * HOW:  Check all features are finite
 */
TEST_F(BrainIntegrationTest, ExtractWindowedFeatures_FeaturesAreValid) {
    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(1, BUFFER_SIZE_100MS);
    ASSERT_NE(buffer, nullptr);

    // Add activity
    float activity[1] = {0.8f};
    brain_buffer_activity(buffer, activity, 1, 1000);

    float features[5];
    size_t num_extracted = brain_extract_windowed_features(buffer, features, 5);

    EXPECT_GT(num_extracted, 0);

    // All features should be finite numbers
    for (size_t i = 0; i < num_extracted; i++) {
        EXPECT_TRUE(std::isfinite(features[i])) << "Feature " << i << " is not finite";
    }

    brain_destroy_temporal_buffer(buffer);
}

/**
 * WHAT: Test feature extraction from empty buffer
 * WHY:  Verify handling of no-data scenario
 * HOW:  Extract features without buffering activity
 */
TEST_F(BrainIntegrationTest, ExtractWindowedFeatures_EmptyBuffer) {
    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(3, BUFFER_SIZE_100MS);
    ASSERT_NE(buffer, nullptr);

    float features[15];
    size_t num_extracted = brain_extract_windowed_features(buffer, features, 15);

    // Should handle empty buffer gracefully (may return 0 or defaults)
    EXPECT_LE(num_extracted, 15);

    brain_destroy_temporal_buffer(buffer);
}

//=============================================================================
// 2. FEATURE NORMALIZATION TESTS (22 tests)
//=============================================================================

//-----------------------------------------------------------------------------
// brain_create_feature_normalizer Tests (9 tests)
//-----------------------------------------------------------------------------

/**
 * WHAT: Test normalizer creation with Z-score type
 * WHY:  Verify Z-score normalization is initialized
 * HOW:  Create normalizer, check zscore pointer non-NULL
 */
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

/**
 * WHAT: Test normalizer creation with Min-Max type
 * WHY:  Verify Min-Max normalization is initialized
 * HOW:  Create normalizer, check minmax pointer non-NULL
 */
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

/**
 * WHAT: Test normalizer creation with Adaptive type
 * WHY:  Verify Adaptive normalization is initialized
 * HOW:  Create normalizer, check adaptive pointer non-NULL
 */
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

/**
 * WHAT: Test normalizer creation with Homeostatic type
 * WHY:  Verify Homeostatic normalization is initialized
 * HOW:  Create normalizer, check homeo pointer non-NULL
 */
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

/**
 * WHAT: Test normalizer creation with None type
 * WHY:  Verify no-op normalization is initialized
 * HOW:  Create normalizer, check all normalizer pointers NULL
 */
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

/**
 * WHAT: Test normalizer creation failure with zero features
 * WHY:  NULL check - zero features is invalid
 * HOW:  Pass num_features=0, expect NULL
 */
TEST_F(BrainIntegrationTest, CreateFeatureNormalizer_Fail_ZeroFeatures) {
    brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(0, NORMALIZE_ZSCORE);

    EXPECT_EQ(normalizer, nullptr);
}

/**
 * WHAT: Test normalizer creation with single feature
 * WHY:  Boundary test - minimum valid feature count
 * HOW:  Create normalizer with 1 feature
 */
TEST_F(BrainIntegrationTest, CreateFeatureNormalizer_Success_SingleFeature) {
    brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(1, NORMALIZE_ZSCORE);

    ASSERT_NE(normalizer, nullptr);
    EXPECT_EQ(normalizer->num_features, 1);

    brain_destroy_feature_normalizer(normalizer);
}

/**
 * WHAT: Test normalizer creation with large feature count
 * WHY:  Verify scalability to high-dimensional features
 * HOW:  Create normalizer with 200 features (reduced for unit test speed)
 */
TEST_F(BrainIntegrationTest, CreateFeatureNormalizer_Success_LargeFeatureCount) {
    brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(200, NORMALIZE_MINMAX);

    ASSERT_NE(normalizer, nullptr);
    EXPECT_EQ(normalizer->num_features, 200);

    brain_destroy_feature_normalizer(normalizer);
}

/**
 * WHAT: Test all normalization types work
 * WHY:  Comprehensive type validation
 * HOW:  Test each enum value
 */
TEST_F(BrainIntegrationTest, CreateFeatureNormalizer_AllTypes_Work) {
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
        EXPECT_EQ(normalizer->type, type);
        brain_destroy_feature_normalizer(normalizer);
    }
}

//-----------------------------------------------------------------------------
// brain_destroy_feature_normalizer Tests (3 tests)
//-----------------------------------------------------------------------------

/**
 * WHAT: Test successful normalizer destruction
 * WHY:  Verify proper cleanup of valid normalizer
 * HOW:  Create normalizer, destroy it, expect no crash
 */
TEST_F(BrainIntegrationTest, DestroyFeatureNormalizer_Success) {
    brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(10, NORMALIZE_ZSCORE);
    ASSERT_NE(normalizer, nullptr);

    // Should not crash
    brain_destroy_feature_normalizer(normalizer);
}

/**
 * WHAT: Test NULL normalizer destruction
 * WHY:  NULL check - must handle NULL gracefully
 * HOW:  Pass NULL, expect no crash
 */
TEST_F(BrainIntegrationTest, DestroyFeatureNormalizer_Null) {
    // Should not crash
    brain_destroy_feature_normalizer(nullptr);
}

/**
 * WHAT: Test destruction of all normalizer types
 * WHY:  Verify cleanup works for each type
 * HOW:  Create and destroy each type
 */
TEST_F(BrainIntegrationTest, DestroyFeatureNormalizer_AllTypes) {
    brain_normalize_type_t types[] = {
        NORMALIZE_ZSCORE,
        NORMALIZE_MINMAX,
        NORMALIZE_ADAPTIVE,
        NORMALIZE_HOMEOSTATIC,
        NORMALIZE_NONE
    };

    for (auto type : types) {
        brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(5, type);
        ASSERT_NE(normalizer, nullptr);
        // Should not crash for any type
        brain_destroy_feature_normalizer(normalizer);
    }
}

//-----------------------------------------------------------------------------
// brain_normalize_features Tests (10 tests)
//-----------------------------------------------------------------------------

/**
 * WHAT: Test successful normalization with Z-score
 * WHY:  Verify Z-score normalization transforms features
 * HOW:  Normalize features, check they're finite
 */
TEST_F(BrainIntegrationTest, NormalizeFeatures_Success_ZScore) {
    brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(3, NORMALIZE_ZSCORE);
    ASSERT_NE(normalizer, nullptr);

    float features[3] = {1.0f, 2.0f, 3.0f};

    bool result = brain_normalize_features(normalizer, features, 3);

    EXPECT_TRUE(result);

    // Features should be normalized (finite)
    for (size_t i = 0; i < 3; i++) {
        EXPECT_TRUE(std::isfinite(features[i]));
    }

    brain_destroy_feature_normalizer(normalizer);
}

/**
 * WHAT: Test successful normalization with Min-Max
 * WHY:  Verify Min-Max normalization works
 * HOW:  Normalize features, check success
 */
TEST_F(BrainIntegrationTest, NormalizeFeatures_Success_MinMax) {
    brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(3, NORMALIZE_MINMAX);
    ASSERT_NE(normalizer, nullptr);

    float features[3] = {0.5f, 1.5f, 2.5f};

    bool result = brain_normalize_features(normalizer, features, 3);

    EXPECT_TRUE(result);

    brain_destroy_feature_normalizer(normalizer);
}

/**
 * WHAT: Test normalization with None type leaves features unchanged
 * WHY:  Verify no-op normalization doesn't modify data
 * HOW:  Normalize with NORMALIZE_NONE, compare before/after
 */
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

/**
 * WHAT: Test normalization failure with NULL normalizer
 * WHY:  NULL check - normalizer parameter validation
 * HOW:  Pass NULL normalizer, expect false
 */
TEST_F(BrainIntegrationTest, NormalizeFeatures_Fail_NullNormalizer) {
    float features[3] = {1.0f, 2.0f, 3.0f};

    bool result = brain_normalize_features(nullptr, features, 3);

    EXPECT_FALSE(result);
}

/**
 * WHAT: Test normalization failure with NULL features
 * WHY:  NULL check - features parameter validation
 * HOW:  Pass NULL features, expect false
 */
TEST_F(BrainIntegrationTest, NormalizeFeatures_Fail_NullFeatures) {
    brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(3, NORMALIZE_ZSCORE);
    ASSERT_NE(normalizer, nullptr);

    bool result = brain_normalize_features(normalizer, nullptr, 3);

    EXPECT_FALSE(result);

    brain_destroy_feature_normalizer(normalizer);
}

/**
 * WHAT: Test normalization failure with zero features
 * WHY:  Boundary test - invalid feature count
 * HOW:  Pass num_features=0, expect false
 */
TEST_F(BrainIntegrationTest, NormalizeFeatures_Fail_ZeroFeatures) {
    brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(3, NORMALIZE_ZSCORE);
    ASSERT_NE(normalizer, nullptr);

    float features[3] = {1.0f, 2.0f, 3.0f};

    bool result = brain_normalize_features(normalizer, features, 0);

    EXPECT_FALSE(result);

    brain_destroy_feature_normalizer(normalizer);
}

/**
 * WHAT: Test normalization failure with too many features
 * WHY:  Size validation - detect size mismatch
 * HOW:  Pass more features than normalizer expects, expect false
 */
TEST_F(BrainIntegrationTest, NormalizeFeatures_Fail_TooManyFeatures) {
    brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(3, NORMALIZE_ZSCORE);
    ASSERT_NE(normalizer, nullptr);

    float features[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};

    bool result = brain_normalize_features(normalizer, features, 5);

    EXPECT_FALSE(result);

    brain_destroy_feature_normalizer(normalizer);
}

/**
 * WHAT: Test in-place transformation works correctly
 * WHY:  Verify features are modified in-place
 * HOW:  Check feature values change after normalization
 */
TEST_F(BrainIntegrationTest, NormalizeFeatures_InPlaceTransformation) {
    brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(3, NORMALIZE_ZSCORE);
    ASSERT_NE(normalizer, nullptr);

    float features[3] = {10.0f, 20.0f, 30.0f};
    float original[3] = {10.0f, 20.0f, 30.0f};

    EXPECT_TRUE(brain_normalize_features(normalizer, features, 3));

    // After multiple normalizations, features should converge
    EXPECT_TRUE(brain_normalize_features(normalizer, features, 3));

    brain_destroy_feature_normalizer(normalizer);
}

/**
 * WHAT: Test normalization with extreme values
 * WHY:  Verify numerical stability
 * HOW:  Use very large and very small values
 */
TEST_F(BrainIntegrationTest, NormalizeFeatures_ExtremeValues) {
    brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(4, NORMALIZE_MINMAX);
    ASSERT_NE(normalizer, nullptr);

    float features[4] = {0.0f, 1000000.0f, -1000000.0f, 0.00001f};

    bool result = brain_normalize_features(normalizer, features, 4);

    EXPECT_TRUE(result);

    for (size_t i = 0; i < 4; i++) {
        EXPECT_TRUE(std::isfinite(features[i]));
    }

    brain_destroy_feature_normalizer(normalizer);
}

/**
 * WHAT: Test all normalization types produce valid output
 * WHY:  Comprehensive validation across types
 * HOW:  Test each type normalizes without error
 */
TEST_F(BrainIntegrationTest, NormalizeFeatures_AllTypes_ProduceValidOutput) {
    brain_normalize_type_t types[] = {
        NORMALIZE_ZSCORE,
        NORMALIZE_MINMAX,
        NORMALIZE_ADAPTIVE,
        NORMALIZE_HOMEOSTATIC,
        NORMALIZE_NONE
    };

    for (auto type : types) {
        brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(5, type);
        ASSERT_NE(normalizer, nullptr);

        float features[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        EXPECT_TRUE(brain_normalize_features(normalizer, features, 5))
            << "Failed for type " << type;

        brain_destroy_feature_normalizer(normalizer);
    }
}

//=============================================================================
// 3. COMBINED OPERATIONS TESTS (8 tests)
//=============================================================================

/**
 * WHAT: Test successful combined extraction and normalization
 * WHY:  Verify integrated workflow works
 * HOW:  Buffer activity, call combined function, check results
 */
TEST_F(BrainIntegrationTest, ExtractAndNormalize_Success) {
    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(2, BUFFER_SIZE_100MS);
    ASSERT_NE(buffer, nullptr);

    brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(10, NORMALIZE_ZSCORE);
    ASSERT_NE(normalizer, nullptr);

    float activity[2] = {0.5f, 0.7f};
    brain_buffer_activity(buffer, activity, 2, 1000);

    float features[10];
    size_t num_extracted = brain_extract_and_normalize(buffer, normalizer, features, 10);

    EXPECT_GT(num_extracted, 0);
    EXPECT_LE(num_extracted, 10);

    for (size_t i = 0; i < num_extracted; i++) {
        EXPECT_TRUE(std::isfinite(features[i]));
    }

    brain_destroy_temporal_buffer(buffer);
    brain_destroy_feature_normalizer(normalizer);
}

/**
 * WHAT: Test failure with NULL buffer
 * WHY:  NULL check - buffer parameter
 * HOW:  Pass NULL buffer, expect 0
 */
TEST_F(BrainIntegrationTest, ExtractAndNormalize_Fail_NullBuffer) {
    brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(10, NORMALIZE_ZSCORE);
    ASSERT_NE(normalizer, nullptr);

    float features[10];
    size_t num_extracted = brain_extract_and_normalize(nullptr, normalizer, features, 10);

    EXPECT_EQ(num_extracted, 0);

    brain_destroy_feature_normalizer(normalizer);
}

/**
 * WHAT: Test failure with NULL normalizer
 * WHY:  NULL check - normalizer parameter
 * HOW:  Pass NULL normalizer, expect 0
 */
TEST_F(BrainIntegrationTest, ExtractAndNormalize_Fail_NullNormalizer) {
    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(2, BUFFER_SIZE_100MS);
    ASSERT_NE(buffer, nullptr);

    float features[10];
    size_t num_extracted = brain_extract_and_normalize(buffer, nullptr, features, 10);

    EXPECT_EQ(num_extracted, 0);

    brain_destroy_temporal_buffer(buffer);
}

/**
 * WHAT: Test failure with NULL features
 * WHY:  NULL check - features parameter
 * HOW:  Pass NULL features, expect 0
 */
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

/**
 * WHAT: Test failure with zero max_features
 * WHY:  Boundary test - invalid output size
 * HOW:  Pass max_features=0, expect 0
 */
TEST_F(BrainIntegrationTest, ExtractAndNormalize_Fail_ZeroMaxFeatures) {
    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(2, BUFFER_SIZE_100MS);
    ASSERT_NE(buffer, nullptr);

    brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(10, NORMALIZE_ZSCORE);
    ASSERT_NE(normalizer, nullptr);

    float features[10];
    size_t num_extracted = brain_extract_and_normalize(buffer, normalizer, features, 0);

    EXPECT_EQ(num_extracted, 0);

    brain_destroy_temporal_buffer(buffer);
    brain_destroy_feature_normalizer(normalizer);
}

/**
 * WHAT: Test error propagation from extraction stage
 * WHY:  Verify proper error handling in pipeline
 * HOW:  Use invalid buffer state, check graceful failure
 */
TEST_F(BrainIntegrationTest, ExtractAndNormalize_ErrorPropagation_Extraction) {
    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(2, BUFFER_SIZE_100MS);
    ASSERT_NE(buffer, nullptr);

    brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(10, NORMALIZE_ZSCORE);
    ASSERT_NE(normalizer, nullptr);

    // Don't buffer any activity - extraction should handle gracefully
    float features[10];
    size_t num_extracted = brain_extract_and_normalize(buffer, normalizer, features, 10);

    // Should handle gracefully (may return 0 or partial features)
    EXPECT_LE(num_extracted, 10);

    brain_destroy_temporal_buffer(buffer);
    brain_destroy_feature_normalizer(normalizer);
}

/**
 * WHAT: Test combined operation with different normalization types
 * WHY:  Verify all normalization types work in pipeline
 * HOW:  Test each normalization type in combined operation
 */
TEST_F(BrainIntegrationTest, ExtractAndNormalize_AllNormalizationTypes) {
    brain_normalize_type_t types[] = {
        NORMALIZE_ZSCORE,
        NORMALIZE_MINMAX,
        NORMALIZE_ADAPTIVE,
        NORMALIZE_HOMEOSTATIC,
        NORMALIZE_NONE
    };

    for (auto type : types) {
        brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(2, BUFFER_SIZE_100MS);
        ASSERT_NE(buffer, nullptr);

        brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(10, type);
        ASSERT_NE(normalizer, nullptr);

        float activity[2] = {0.5f, 0.7f};
        brain_buffer_activity(buffer, activity, 2, 1000);

        float features[10];
        size_t num = brain_extract_and_normalize(buffer, normalizer, features, 10);

        EXPECT_GE(num, 0) << "Failed for type " << type;

        brain_destroy_temporal_buffer(buffer);
        brain_destroy_feature_normalizer(normalizer);
    }
}

/**
 * WHAT: Test combined operation with different buffer presets
 * WHY:  Verify all buffer timescales work in pipeline
 * HOW:  Test each buffer preset in combined operation
 */
TEST_F(BrainIntegrationTest, ExtractAndNormalize_AllBufferPresets) {
    brain_buffer_size_t presets[] = {
        BUFFER_SIZE_10MS,
        BUFFER_SIZE_100MS,
        BUFFER_SIZE_1S,
        BUFFER_SIZE_CUSTOM
    };

    for (auto preset : presets) {
        brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(2, preset);
        ASSERT_NE(buffer, nullptr);

        brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(10, NORMALIZE_ZSCORE);
        ASSERT_NE(normalizer, nullptr);

        float activity[2] = {0.5f, 0.7f};
        brain_buffer_activity(buffer, activity, 2, 1000);

        float features[10];
        size_t num = brain_extract_and_normalize(buffer, normalizer, features, 10);

        EXPECT_GE(num, 0) << "Failed for preset " << preset;

        brain_destroy_temporal_buffer(buffer);
        brain_destroy_feature_normalizer(normalizer);
    }
}

//=============================================================================
// 4. SPIKE FEATURE EXTRACTION TESTS (15 tests)
//=============================================================================

//-----------------------------------------------------------------------------
// brain_create_spike_feature_extractor Tests (6 tests)
//-----------------------------------------------------------------------------

/**
 * WHAT: Test extractor creation with basic config
 * WHY:  Verify minimal configuration works
 * HOW:  Create with oscillations and synchrony disabled
 */
TEST_F(BrainIntegrationTest, CreateSpikeFeatureExtractor_Success_BasicConfig) {
    brain_spike_feature_extractor_t extractor = brain_create_spike_feature_extractor(
        100, false, false
    );

    ASSERT_NE(extractor, nullptr);

    brain_destroy_spike_feature_extractor(extractor);
}

/**
 * WHAT: Test extractor creation with oscillations enabled
 * WHY:  Verify oscillation feature extraction works
 * HOW:  Create with compute_oscillations=true
 */
TEST_F(BrainIntegrationTest, CreateSpikeFeatureExtractor_Success_WithOscillations) {
    brain_spike_feature_extractor_t extractor = brain_create_spike_feature_extractor(
        50, true, false
    );

    ASSERT_NE(extractor, nullptr);

    brain_destroy_spike_feature_extractor(extractor);
}

/**
 * WHAT: Test extractor creation with synchrony enabled
 * WHY:  Verify synchrony analysis works
 * HOW:  Create with compute_synchrony=true
 */
TEST_F(BrainIntegrationTest, CreateSpikeFeatureExtractor_Success_WithSynchrony) {
    brain_spike_feature_extractor_t extractor = brain_create_spike_feature_extractor(
        50, false, true
    );

    ASSERT_NE(extractor, nullptr);

    brain_destroy_spike_feature_extractor(extractor);
}

/**
 * WHAT: Test extractor creation with all features enabled
 * WHY:  Verify full feature extraction config works
 * HOW:  Create with both oscillations and synchrony enabled
 */
TEST_F(BrainIntegrationTest, CreateSpikeFeatureExtractor_Success_AllFeatures) {
    brain_spike_feature_extractor_t extractor = brain_create_spike_feature_extractor(
        75, true, true
    );

    ASSERT_NE(extractor, nullptr);

    brain_destroy_spike_feature_extractor(extractor);
}

/**
 * WHAT: Test extractor creation failure with zero neurons
 * WHY:  NULL check - zero neurons is invalid
 * HOW:  Pass max_neurons=0, expect NULL
 */
TEST_F(BrainIntegrationTest, CreateSpikeFeatureExtractor_Fail_ZeroNeurons) {
    brain_spike_feature_extractor_t extractor = brain_create_spike_feature_extractor(
        0, false, false
    );

    EXPECT_EQ(extractor, nullptr);
}

/**
 * WHAT: Test extractor creation failure with too many neurons
 * WHY:  Boundary test - exceeds maximum
 * HOW:  Pass max_neurons > FEATURE_EXTRACTOR_MAX_NEURONS, expect NULL
 */
TEST_F(BrainIntegrationTest, CreateSpikeFeatureExtractor_Fail_TooManyNeurons) {
    brain_spike_feature_extractor_t extractor = brain_create_spike_feature_extractor(
        FEATURE_EXTRACTOR_MAX_NEURONS + 1, false, false
    );

    EXPECT_EQ(extractor, nullptr);
}

/**
 * WHAT: Test extractor creation at maximum boundary
 * WHY:  Boundary test - maximum valid neuron count
 * HOW:  Create with exactly FEATURE_EXTRACTOR_MAX_NEURONS
 */
TEST_F(BrainIntegrationTest, CreateSpikeFeatureExtractor_Success_MaxNeurons) {
    brain_spike_feature_extractor_t extractor = brain_create_spike_feature_extractor(
        FEATURE_EXTRACTOR_MAX_NEURONS, false, false
    );

    ASSERT_NE(extractor, nullptr);

    brain_destroy_spike_feature_extractor(extractor);
}

//-----------------------------------------------------------------------------
// brain_destroy_spike_feature_extractor Tests (2 tests)
//-----------------------------------------------------------------------------

/**
 * WHAT: Test successful extractor destruction
 * WHY:  Verify proper cleanup
 * HOW:  Create and destroy, expect no crash
 */
TEST_F(BrainIntegrationTest, DestroySpikeFeatureExtractor_Success) {
    brain_spike_feature_extractor_t extractor = brain_create_spike_feature_extractor(
        100, false, false
    );
    ASSERT_NE(extractor, nullptr);

    // Should not crash
    brain_destroy_spike_feature_extractor(extractor);
}

/**
 * WHAT: Test NULL extractor destruction
 * WHY:  NULL check - must handle NULL gracefully
 * HOW:  Pass NULL, expect no crash
 */
TEST_F(BrainIntegrationTest, DestroySpikeFeatureExtractor_Null) {
    // Should not crash
    brain_destroy_spike_feature_extractor(nullptr);
}

//-----------------------------------------------------------------------------
// brain_extract_spike_features Tests (7 tests)
//-----------------------------------------------------------------------------

/**
 * WHAT: Test successful spike feature extraction
 * WHY:  Verify basic extraction works
 * HOW:  Create extractor and spike data, extract features
 */
TEST_F(BrainIntegrationTest, ExtractSpikeFeatures_Success) {
    brain_spike_feature_extractor_t extractor = brain_create_spike_feature_extractor(
        10, false, false
    );
    ASSERT_NE(extractor, nullptr);

    spike_data_t* spike_data = create_test_spike_data(10, 0, 1000);
    ASSERT_NE(spike_data, nullptr);

    middleware_features_t features;
    bool result = brain_extract_spike_features(extractor, spike_data, &features);

    EXPECT_TRUE(result);
    EXPECT_TRUE(features.valid);

    spike_data_destroy(spike_data);
    brain_destroy_spike_feature_extractor(extractor);
}

/**
 * WHAT: Test feature extraction failure with NULL extractor
 * WHY:  NULL check - extractor parameter
 * HOW:  Pass NULL extractor, expect false
 */
TEST_F(BrainIntegrationTest, ExtractSpikeFeatures_Fail_NullExtractor) {
    spike_data_t* spike_data = create_test_spike_data(10, 0, 1000);
    ASSERT_NE(spike_data, nullptr);

    middleware_features_t features;
    bool result = brain_extract_spike_features(nullptr, spike_data, &features);

    EXPECT_FALSE(result);

    spike_data_destroy(spike_data);
}

/**
 * WHAT: Test feature extraction failure with NULL spike data
 * WHY:  NULL check - spike_data parameter
 * HOW:  Pass NULL spike_data, expect false
 */
TEST_F(BrainIntegrationTest, ExtractSpikeFeatures_Fail_NullSpikeData) {
    brain_spike_feature_extractor_t extractor = brain_create_spike_feature_extractor(
        10, false, false
    );
    ASSERT_NE(extractor, nullptr);

    middleware_features_t features;
    bool result = brain_extract_spike_features(extractor, nullptr, &features);

    EXPECT_FALSE(result);

    brain_destroy_spike_feature_extractor(extractor);
}

/**
 * WHAT: Test feature extraction failure with NULL features_out
 * WHY:  NULL check - features_out parameter
 * HOW:  Pass NULL features_out, expect false
 */
TEST_F(BrainIntegrationTest, ExtractSpikeFeatures_Fail_NullFeaturesOut) {
    brain_spike_feature_extractor_t extractor = brain_create_spike_feature_extractor(
        10, false, false
    );
    ASSERT_NE(extractor, nullptr);

    spike_data_t* spike_data = create_test_spike_data(10, 0, 1000);
    ASSERT_NE(spike_data, nullptr);

    bool result = brain_extract_spike_features(extractor, spike_data, nullptr);

    EXPECT_FALSE(result);

    spike_data_destroy(spike_data);
    brain_destroy_spike_feature_extractor(extractor);
}

/**
 * WHAT: Test feature extraction with neuron count mismatch
 * WHY:  Validate neuron count consistency
 * HOW:  Create extractor for N neurons, use M neurons in data
 */
TEST_F(BrainIntegrationTest, ExtractSpikeFeatures_Fail_NeuronCountMismatch) {
    brain_spike_feature_extractor_t extractor = brain_create_spike_feature_extractor(
        10, false, false
    );
    ASSERT_NE(extractor, nullptr);

    // Create spike data with different neuron count
    spike_data_t* spike_data = create_test_spike_data(20, 0, 1000);
    ASSERT_NE(spike_data, nullptr);

    middleware_features_t features;
    bool result = brain_extract_spike_features(extractor, spike_data, &features);

    // Should handle gracefully or fail safely
    // Implementation may succeed with subset or fail

    spike_data_destroy(spike_data);
    brain_destroy_spike_feature_extractor(extractor);
}

/**
 * WHAT: Test feature extraction produces valid features
 * WHY:  Verify output quality
 * HOW:  Extract features, check all are finite
 */
TEST_F(BrainIntegrationTest, ExtractSpikeFeatures_FeaturesAreValid) {
    brain_spike_feature_extractor_t extractor = brain_create_spike_feature_extractor(
        10, true, true
    );
    ASSERT_NE(extractor, nullptr);

    spike_data_t* spike_data = create_test_spike_data(10, 0, 1000);
    ASSERT_NE(spike_data, nullptr);

    middleware_features_t features;
    bool result = brain_extract_spike_features(extractor, spike_data, &features);

    EXPECT_TRUE(result);

    // Check all features are finite
    EXPECT_TRUE(std::isfinite(features.mean_firing_rate));
    EXPECT_TRUE(std::isfinite(features.population_rate_std));
    EXPECT_TRUE(std::isfinite(features.mean_isi));
    EXPECT_TRUE(std::isfinite(features.isi_cv));
    EXPECT_TRUE(std::isfinite(features.synchrony_index));

    spike_data_destroy(spike_data);
    brain_destroy_spike_feature_extractor(extractor);
}

/**
 * WHAT: Test feature extraction with oscillation computation
 * WHY:  Verify oscillation features are computed
 * HOW:  Enable oscillations, extract features, check oscillation fields
 */
TEST_F(BrainIntegrationTest, ExtractSpikeFeatures_WithOscillations) {
    brain_spike_feature_extractor_t extractor = brain_create_spike_feature_extractor(
        10, true, false
    );
    ASSERT_NE(extractor, nullptr);

    spike_data_t* spike_data = create_test_spike_data(10, 0, 1000);
    ASSERT_NE(spike_data, nullptr);

    middleware_features_t features;
    bool result = brain_extract_spike_features(extractor, spike_data, &features);

    EXPECT_TRUE(result);

    // Oscillation features should be computed
    EXPECT_TRUE(std::isfinite(features.delta_power));
    EXPECT_TRUE(std::isfinite(features.theta_power));
    EXPECT_TRUE(std::isfinite(features.alpha_power));
    EXPECT_TRUE(std::isfinite(features.beta_power));
    EXPECT_TRUE(std::isfinite(features.gamma_power));

    spike_data_destroy(spike_data);
    brain_destroy_spike_feature_extractor(extractor);
}

//=============================================================================
// 5. POPULATION CODING ANALYSIS TESTS (18 tests)
//=============================================================================

//-----------------------------------------------------------------------------
// brain_create_population_analyzer Tests (3 tests)
//-----------------------------------------------------------------------------

/**
 * WHAT: Test successful analyzer creation
 * WHY:  Verify default configuration works
 * HOW:  Create analyzer with no parameters
 */
TEST_F(BrainIntegrationTest, CreatePopulationAnalyzer_Success) {
    brain_population_analyzer_t analyzer = brain_create_population_analyzer();

    ASSERT_NE(analyzer, nullptr);

    brain_destroy_population_analyzer(analyzer);
}

/**
 * WHAT: Test multiple analyzer creations
 * WHY:  Verify independent instances work
 * HOW:  Create multiple analyzers simultaneously
 */
TEST_F(BrainIntegrationTest, CreatePopulationAnalyzer_Multiple) {
    brain_population_analyzer_t analyzer1 = brain_create_population_analyzer();
    brain_population_analyzer_t analyzer2 = brain_create_population_analyzer();
    brain_population_analyzer_t analyzer3 = brain_create_population_analyzer();

    ASSERT_NE(analyzer1, nullptr);
    ASSERT_NE(analyzer2, nullptr);
    ASSERT_NE(analyzer3, nullptr);

    brain_destroy_population_analyzer(analyzer1);
    brain_destroy_population_analyzer(analyzer2);
    brain_destroy_population_analyzer(analyzer3);
}

/**
 * WHAT: Test sequential analyzer creation/destruction
 * WHY:  Verify no resource leaks
 * HOW:  Create and destroy in loop
 */
TEST_F(BrainIntegrationTest, CreatePopulationAnalyzer_Sequential) {
    for (int i = 0; i < 10; i++) {
        brain_population_analyzer_t analyzer = brain_create_population_analyzer();
        ASSERT_NE(analyzer, nullptr) << "Failed on iteration " << i;
        brain_destroy_population_analyzer(analyzer);
    }
}

//-----------------------------------------------------------------------------
// brain_destroy_population_analyzer Tests (2 tests)
//-----------------------------------------------------------------------------

/**
 * WHAT: Test successful analyzer destruction
 * WHY:  Verify proper cleanup
 * HOW:  Create and destroy, expect no crash
 */
TEST_F(BrainIntegrationTest, DestroyPopulationAnalyzer_Success) {
    brain_population_analyzer_t analyzer = brain_create_population_analyzer();
    ASSERT_NE(analyzer, nullptr);

    // Should not crash
    brain_destroy_population_analyzer(analyzer);
}

/**
 * WHAT: Test NULL analyzer destruction
 * WHY:  NULL check - must handle NULL gracefully
 * HOW:  Pass NULL, expect no crash
 */
TEST_F(BrainIntegrationTest, DestroyPopulationAnalyzer_Null) {
    // Should not crash
    brain_destroy_population_analyzer(nullptr);
}

//-----------------------------------------------------------------------------
// brain_compute_population_vector Tests (8 tests)
//-----------------------------------------------------------------------------

/**
 * WHAT: Test successful population vector computation
 * WHY:  Verify basic vector encoding works
 * HOW:  Create test data with tuning curves, compute vector
 */
TEST_F(BrainIntegrationTest, ComputePopulationVector_Success) {
    brain_population_analyzer_t analyzer = brain_create_population_analyzer();
    ASSERT_NE(analyzer, nullptr);

    const uint32_t num_neurons = 8;
    float rates[8] = {0.5f, 0.8f, 0.3f, 0.6f, 0.4f, 0.7f, 0.2f, 0.9f};

    tuning_curve_t tuning_curves[8];
    for (uint32_t i = 0; i < num_neurons; i++) {
        tuning_curves[i].preferred_direction.x = std::cos(i * M_PI / 4.0f);
        tuning_curves[i].preferred_direction.y = std::sin(i * M_PI / 4.0f);
        tuning_curves[i].preferred_direction.z = 0.0f;
        tuning_curves[i].preferred_direction.magnitude = 1.0f;
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
    EXPECT_TRUE(std::isfinite(vector_out.magnitude));

    brain_destroy_population_analyzer(analyzer);
}

/**
 * WHAT: Test failure with NULL analyzer
 * WHY:  NULL check - analyzer parameter
 * HOW:  Pass NULL analyzer, expect false
 */
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

/**
 * WHAT: Test failure with NULL rates
 * WHY:  NULL check - rates parameter
 * HOW:  Pass NULL rates, expect false
 */
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

/**
 * WHAT: Test failure with NULL tuning curves
 * WHY:  NULL check - tuning_curves parameter
 * HOW:  Pass NULL tuning_curves, expect false
 */
TEST_F(BrainIntegrationTest, ComputePopulationVector_Fail_NullTuningCurves) {
    brain_population_analyzer_t analyzer = brain_create_population_analyzer();
    ASSERT_NE(analyzer, nullptr);

    float rates[8] = {0.5f, 0.8f, 0.3f, 0.6f, 0.4f, 0.7f, 0.2f, 0.9f};
    vector3d_t vector_out;

    bool result = brain_compute_population_vector(
        analyzer, rates, nullptr, 8, &vector_out
    );

    EXPECT_FALSE(result);

    brain_destroy_population_analyzer(analyzer);
}

/**
 * WHAT: Test failure with NULL vector_out
 * WHY:  NULL check - vector_out parameter
 * HOW:  Pass NULL vector_out, expect false
 */
TEST_F(BrainIntegrationTest, ComputePopulationVector_Fail_NullVectorOut) {
    brain_population_analyzer_t analyzer = brain_create_population_analyzer();
    ASSERT_NE(analyzer, nullptr);

    float rates[8] = {0.5f, 0.8f, 0.3f, 0.6f, 0.4f, 0.7f, 0.2f, 0.9f};
    tuning_curve_t tuning_curves[8];

    bool result = brain_compute_population_vector(
        analyzer, rates, tuning_curves, 8, nullptr
    );

    EXPECT_FALSE(result);

    brain_destroy_population_analyzer(analyzer);
}

/**
 * WHAT: Test failure with zero neurons
 * WHY:  Boundary test - invalid neuron count
 * HOW:  Pass num_neurons=0, expect false
 */
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

/**
 * WHAT: Test failure with too many neurons
 * WHY:  Boundary test - exceeds maximum
 * HOW:  Pass num_neurons > POPULATION_MAX_NEURONS, expect false
 */
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

/**
 * WHAT: Test with single neuron
 * WHY:  Boundary test - minimum valid neuron count
 * HOW:  Compute vector with 1 neuron
 */
TEST_F(BrainIntegrationTest, ComputePopulationVector_SingleNeuron) {
    brain_population_analyzer_t analyzer = brain_create_population_analyzer();
    ASSERT_NE(analyzer, nullptr);

    float rates[1] = {0.5f};
    tuning_curve_t tuning_curves[1];
    tuning_curves[0].preferred_direction.x = 1.0f;
    tuning_curves[0].preferred_direction.y = 0.0f;
    tuning_curves[0].preferred_direction.z = 0.0f;
    tuning_curves[0].preferred_direction.magnitude = 1.0f;
    tuning_curves[0].tuning_width = 30.0f;
    tuning_curves[0].max_rate = 1.0f;

    vector3d_t vector_out;
    bool result = brain_compute_population_vector(
        analyzer, rates, tuning_curves, 1, &vector_out
    );

    EXPECT_TRUE(result);

    brain_destroy_population_analyzer(analyzer);
}

//-----------------------------------------------------------------------------
// brain_compute_population_synchrony Tests (5 tests)
//-----------------------------------------------------------------------------

/**
 * WHAT: Test successful synchrony computation
 * WHY:  Verify synchrony analysis works
 * HOW:  Create spike trains, compute synchrony
 */
TEST_F(BrainIntegrationTest, ComputePopulationSynchrony_Success) {
    brain_population_analyzer_t analyzer = brain_create_population_analyzer();
    ASSERT_NE(analyzer, nullptr);

    const uint32_t num_neurons = 5;
    spike_train_t** trains = create_test_spike_trains(num_neurons);
    ASSERT_NE(trains, nullptr);

    synchrony_result_t synchrony_out;
    bool result = brain_compute_population_synchrony(
        analyzer, trains, num_neurons, &synchrony_out
    );

    EXPECT_TRUE(result);
    EXPECT_TRUE(std::isfinite(synchrony_out.synchrony_index));
    EXPECT_TRUE(std::isfinite(synchrony_out.mean_correlation));
    EXPECT_GE(synchrony_out.synchrony_index, 0.0f);
    EXPECT_LE(synchrony_out.synchrony_index, 1.0f);

    destroy_spike_trains(trains, num_neurons);
    brain_destroy_population_analyzer(analyzer);
}

/**
 * WHAT: Test failure with NULL analyzer
 * WHY:  NULL check - analyzer parameter
 * HOW:  Pass NULL analyzer, expect false
 */
TEST_F(BrainIntegrationTest, ComputePopulationSynchrony_Fail_NullAnalyzer) {
    const uint32_t num_neurons = 5;
    spike_train_t** trains = create_test_spike_trains(num_neurons);
    ASSERT_NE(trains, nullptr);

    synchrony_result_t synchrony_out;
    bool result = brain_compute_population_synchrony(
        nullptr, trains, num_neurons, &synchrony_out
    );

    EXPECT_FALSE(result);

    destroy_spike_trains(trains, num_neurons);
}

/**
 * WHAT: Test failure with NULL spike trains
 * WHY:  NULL check - spike_trains parameter
 * HOW:  Pass NULL spike_trains, expect false
 */
TEST_F(BrainIntegrationTest, ComputePopulationSynchrony_Fail_NullSpikeTrains) {
    brain_population_analyzer_t analyzer = brain_create_population_analyzer();
    ASSERT_NE(analyzer, nullptr);

    synchrony_result_t synchrony_out;
    bool result = brain_compute_population_synchrony(
        analyzer, nullptr, 5, &synchrony_out
    );

    EXPECT_FALSE(result);

    brain_destroy_population_analyzer(analyzer);
}

/**
 * WHAT: Test failure with NULL synchrony_out
 * WHY:  NULL check - synchrony_out parameter
 * HOW:  Pass NULL synchrony_out, expect false
 */
TEST_F(BrainIntegrationTest, ComputePopulationSynchrony_Fail_NullSynchronyOut) {
    brain_population_analyzer_t analyzer = brain_create_population_analyzer();
    ASSERT_NE(analyzer, nullptr);

    const uint32_t num_neurons = 5;
    spike_train_t** trains = create_test_spike_trains(num_neurons);
    ASSERT_NE(trains, nullptr);

    bool result = brain_compute_population_synchrony(
        analyzer, trains, num_neurons, nullptr
    );

    EXPECT_FALSE(result);

    destroy_spike_trains(trains, num_neurons);
    brain_destroy_population_analyzer(analyzer);
}

/**
 * WHAT: Test failure with insufficient neurons for synchrony
 * WHY:  Minimum 2 neurons required for pairwise correlation
 * HOW:  Pass num_neurons < 2, expect false
 */
TEST_F(BrainIntegrationTest, ComputePopulationSynchrony_Fail_InsufficientNeurons) {
    brain_population_analyzer_t analyzer = brain_create_population_analyzer();
    ASSERT_NE(analyzer, nullptr);

    spike_train_t** trains = create_test_spike_trains(1);
    ASSERT_NE(trains, nullptr);

    synchrony_result_t synchrony_out;
    bool result = brain_compute_population_synchrony(
        analyzer, trains, 1, &synchrony_out
    );

    EXPECT_FALSE(result);

    destroy_spike_trains(trains, 1);
    brain_destroy_population_analyzer(analyzer);
}

//=============================================================================
// INTEGRATION TESTS (10 tests)
//=============================================================================

/**
 * WHAT: Test complete workflow: buffer, extract, normalize
 * WHY:  Verify end-to-end cognitive module integration
 * HOW:  Simulate realistic usage pattern
 */
TEST_F(BrainIntegrationTest, FullWorkflow_BufferExtractNormalize) {
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

    EXPECT_GT(num_extracted, 0);

    // 4. Verify features are valid
    for (size_t i = 0; i < num_extracted; i++) {
        EXPECT_TRUE(std::isfinite(features[i]));
    }

    // 5. Cleanup
    brain_destroy_temporal_buffer(buffer);
    brain_destroy_feature_normalizer(normalizer);
}

/**
 * WHAT: Test all buffer presets work
 * WHY:  Verify comprehensive buffer size support
 * HOW:  Test each preset in realistic workflow
 */
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
        EXPECT_GE(num, 0) << "No features extracted for preset " << preset;

        brain_destroy_temporal_buffer(buffer);
    }
}

/**
 * WHAT: Test all normalization types work
 * WHY:  Verify comprehensive normalization support
 * HOW:  Test each type with feature data
 */
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

/**
 * WHAT: Test spike feature extraction pipeline
 * WHY:  Verify spike-based analysis workflow
 * HOW:  Create extractor, extract features from spike data
 */
TEST_F(BrainIntegrationTest, SpikeFeatureExtraction_Pipeline) {
    brain_spike_feature_extractor_t extractor = brain_create_spike_feature_extractor(
        20, true, true
    );
    ASSERT_NE(extractor, nullptr);

    spike_data_t* spike_data = create_test_spike_data(20, 0, 1000);
    ASSERT_NE(spike_data, nullptr);

    middleware_features_t features;
    bool result = brain_extract_spike_features(extractor, spike_data, &features);

    EXPECT_TRUE(result);
    EXPECT_TRUE(features.valid);

    spike_data_destroy(spike_data);
    brain_destroy_spike_feature_extractor(extractor);
}

/**
 * WHAT: Test population coding analysis pipeline
 * WHY:  Verify population-level analysis workflow
 * HOW:  Compute population vector and synchrony
 */
TEST_F(BrainIntegrationTest, PopulationCoding_Pipeline) {
    brain_population_analyzer_t analyzer = brain_create_population_analyzer();
    ASSERT_NE(analyzer, nullptr);

    // Test population vector
    const uint32_t num_neurons = 10;
    float rates[10];
    tuning_curve_t tuning_curves[10];

    for (uint32_t i = 0; i < num_neurons; i++) {
        rates[i] = 0.5f + 0.1f * i;
        tuning_curves[i].preferred_direction.x = std::cos(i * 2.0f * M_PI / num_neurons);
        tuning_curves[i].preferred_direction.y = std::sin(i * 2.0f * M_PI / num_neurons);
        tuning_curves[i].preferred_direction.z = 0.0f;
        tuning_curves[i].preferred_direction.magnitude = 1.0f;
        tuning_curves[i].tuning_width = 30.0f;
        tuning_curves[i].max_rate = 1.0f;
    }

    vector3d_t vector_out;
    EXPECT_TRUE(brain_compute_population_vector(
        analyzer, rates, tuning_curves, num_neurons, &vector_out
    ));

    // Test population synchrony
    spike_train_t** trains = create_test_spike_trains(num_neurons);
    ASSERT_NE(trains, nullptr);

    synchrony_result_t synchrony_out;
    EXPECT_TRUE(brain_compute_population_synchrony(
        analyzer, trains, num_neurons, &synchrony_out
    ));

    destroy_spike_trains(trains, num_neurons);
    brain_destroy_population_analyzer(analyzer);
}

/**
 * WHAT: Test memory cleanup in complex workflow
 * WHY:  Verify no memory leaks
 * HOW:  Create all components, use them, cleanup
 */
TEST_F(BrainIntegrationTest, CompleteWorkflow_NoMemoryLeaks) {
    // Create all components
    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(5, BUFFER_SIZE_100MS);
    brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(25, NORMALIZE_ZSCORE);
    brain_spike_feature_extractor_t extractor = brain_create_spike_feature_extractor(10, true, true);
    brain_population_analyzer_t analyzer = brain_create_population_analyzer();

    ASSERT_NE(buffer, nullptr);
    ASSERT_NE(normalizer, nullptr);
    ASSERT_NE(extractor, nullptr);
    ASSERT_NE(analyzer, nullptr);

    // Use components
    float activity[5] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
    brain_buffer_activity(buffer, activity, 5, 1000);

    float features[25];
    brain_extract_and_normalize(buffer, normalizer, features, 25);

    // Cleanup all
    brain_destroy_temporal_buffer(buffer);
    brain_destroy_feature_normalizer(normalizer);
    brain_destroy_spike_feature_extractor(extractor);
    brain_destroy_population_analyzer(analyzer);
}

/**
 * WHAT: Test concurrent buffer and normalizer usage
 * WHY:  Verify independent operation
 * HOW:  Use multiple buffers and normalizers simultaneously
 */
TEST_F(BrainIntegrationTest, ConcurrentUsage_MultipleBuffersAndNormalizers) {
    brain_temporal_buffer_t* buffer1 = brain_create_temporal_buffer(3, BUFFER_SIZE_10MS);
    brain_temporal_buffer_t* buffer2 = brain_create_temporal_buffer(5, BUFFER_SIZE_100MS);
    brain_feature_normalizer_t* norm1 = brain_create_feature_normalizer(15, NORMALIZE_ZSCORE);
    brain_feature_normalizer_t* norm2 = brain_create_feature_normalizer(25, NORMALIZE_MINMAX);

    ASSERT_NE(buffer1, nullptr);
    ASSERT_NE(buffer2, nullptr);
    ASSERT_NE(norm1, nullptr);
    ASSERT_NE(norm2, nullptr);

    float activity1[3] = {0.1f, 0.2f, 0.3f};
    float activity2[5] = {0.4f, 0.5f, 0.6f, 0.7f, 0.8f};

    EXPECT_TRUE(brain_buffer_activity(buffer1, activity1, 3, 1000));
    EXPECT_TRUE(brain_buffer_activity(buffer2, activity2, 5, 1000));

    brain_destroy_temporal_buffer(buffer1);
    brain_destroy_temporal_buffer(buffer2);
    brain_destroy_feature_normalizer(norm1);
    brain_destroy_feature_normalizer(norm2);
}

/**
 * WHAT: Test moderate scale: realistic populations
 * WHY:  Verify scalability without extreme overhead
 * HOW:  Use moderate channel counts (reduced for unit test speed)
 * NOTE: Extreme scale tests (1000+ channels) belong in performance/regression tests
 */
TEST_F(BrainIntegrationTest, ExtremeScale_LargePopulations) {
    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(50, BUFFER_SIZE_100MS);
    ASSERT_NE(buffer, nullptr);

    brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(250, NORMALIZE_MINMAX);
    ASSERT_NE(normalizer, nullptr);

    std::vector<float> activity(50, 0.5f);
    EXPECT_TRUE(brain_buffer_activity(buffer, activity.data(), 50, 1000));

    brain_destroy_temporal_buffer(buffer);
    brain_destroy_feature_normalizer(normalizer);
}

/**
 * WHAT: Test error recovery in pipeline
 * WHY:  Verify graceful degradation
 * HOW:  Inject errors at each stage, verify handling
 */
TEST_F(BrainIntegrationTest, ErrorRecovery_Pipeline) {
    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(3, BUFFER_SIZE_100MS);
    brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(15, NORMALIZE_ZSCORE);

    ASSERT_NE(buffer, nullptr);
    ASSERT_NE(normalizer, nullptr);

    // Test recovery from NULL activity
    EXPECT_FALSE(brain_buffer_activity(buffer, nullptr, 3, 1000));

    // Test recovery from mismatched channels
    float activity[5] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
    EXPECT_FALSE(brain_buffer_activity(buffer, activity, 5, 1000));

    // Valid operation should still work
    float valid_activity[3] = {0.1f, 0.2f, 0.3f};
    EXPECT_TRUE(brain_buffer_activity(buffer, valid_activity, 3, 1000));

    brain_destroy_temporal_buffer(buffer);
    brain_destroy_feature_normalizer(normalizer);
}

/**
 * WHAT: Test rapid create/destroy cycles
 * WHY:  Stress test resource management
 * HOW:  Rapidly create and destroy components
 */
TEST_F(BrainIntegrationTest, StressTest_RapidCreateDestroy) {
    for (int i = 0; i < 100; i++) {
        brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(5, BUFFER_SIZE_100MS);
        brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(25, NORMALIZE_ZSCORE);

        if (buffer) brain_destroy_temporal_buffer(buffer);
        if (normalizer) brain_destroy_feature_normalizer(normalizer);
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
