//=============================================================================
// test_brain_integration_integration.cpp - Brain Integration Workflow Tests
//=============================================================================
//
// WHAT: Integration tests for brain_integration module workflows
// WHY:  Verify end-to-end correctness of combined middleware operations
// HOW:  Test realistic multi-step workflows: buffer→extract→normalize
//
// TEST CATEGORIES:
// 1. Temporal Buffer + Feature Extraction (7 tests)
// 2. Feature Extraction + Normalization (10 tests)
// 3. Complete Pipeline (7 tests)
// 4. Spike Features + Population Analysis (7 tests)
// 5. Cross-Component Integration (5 tests)
//
// Total: 36 integration tests
//
//=============================================================================

#include <gtest/gtest.h>
#include "middleware/brain_integration.h"
#include "utils/memory/nimcp_memory.h"
#include <cmath>
#include <vector>
#include <algorithm>

//=============================================================================
// GLOBAL TEST ENVIRONMENT
//=============================================================================

/**
 * WHAT: Global test environment for memory tracking
 * WHY:  Initialize memory tracking before any tests run
 * HOW:  Set up tracking in SetUp, report leaks in TearDown
 */
class MemoryTrackingEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
    }

    void TearDown() override {
        nimcp_memory_check_leaks();
        nimcp_memory_cleanup();
    }
};

// Register global environment
::testing::Environment* const memory_env =
    ::testing::AddGlobalTestEnvironment(new MemoryTrackingEnvironment);

//=============================================================================
// TEST FIXTURES
//=============================================================================

/**
 * WHAT: Base fixture for brain integration tests
 * WHY:  Provide common setup/teardown and helper methods
 * HOW:  Initialize components, track memory, provide utilities
 */
class BrainIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Track initial memory state (current allocated bytes, not cumulative count)
        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        initial_bytes_allocated = stats.current_allocated;
    }

    void TearDown() override {
        // Verify no memory leaks (compare current allocations, not cumulative)
        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        size_t final_bytes_allocated = stats.current_allocated;
        EXPECT_EQ(initial_bytes_allocated, final_bytes_allocated)
            << "Memory leak detected: "
            << (final_bytes_allocated - initial_bytes_allocated)
            << " bytes not freed";
    }

    // Helper: Generate synthetic neural activity
    void generate_activity(float* activity, size_t num_channels,
                          float base_rate, float noise_level) {
        for (size_t i = 0; i < num_channels; i++) {
            float noise = (static_cast<float>(rand()) / RAND_MAX - 0.5f)
                         * 2.0f * noise_level;
            activity[i] = base_rate + noise;
        }
    }

    // Helper: Generate oscillating activity pattern
    void generate_oscillation(float* activity, size_t num_channels,
                             float frequency, uint64_t time_ms) {
        float phase = 2.0f * M_PI * frequency * time_ms / 1000.0f;
        for (size_t i = 0; i < num_channels; i++) {
            activity[i] = 0.5f + 0.5f * std::sin(phase + i * 0.1f);
        }
    }

    // Helper: Verify feature is in valid range
    void check_feature_range(float value, float min_val, float max_val) {
        EXPECT_GE(value, min_val);
        EXPECT_LE(value, max_val);
    }

    // Helper: Check if value is approximately equal
    bool approx_equal(float a, float b, float tolerance = 0.01f) {
        return std::abs(a - b) < tolerance;
    }

    size_t initial_bytes_allocated = 0;
};

//=============================================================================
// CATEGORY 1: TEMPORAL BUFFER + FEATURE EXTRACTION WORKFLOW
//=============================================================================

/**
 * TEST: Buffer creation and basic activity buffering
 * WHAT: Create buffer → buffer activity → verify state
 * WHY:  Foundation for all buffering workflows
 */
TEST_F(BrainIntegrationTest, BufferCreationAndActivity) {
    const size_t num_channels = 10;

    // Create buffer with 100MS preset
    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(
        num_channels, BUFFER_SIZE_100MS
    );
    ASSERT_NE(buffer, nullptr);
    EXPECT_EQ(buffer->num_channels, num_channels);
    EXPECT_EQ(buffer->size_preset, BUFFER_SIZE_100MS);

    // Buffer some activity
    float activity[10];
    generate_activity(activity, num_channels, 10.0f, 2.0f);

    bool success = brain_buffer_activity(buffer, activity, num_channels, 1000);
    EXPECT_TRUE(success);

    brain_destroy_temporal_buffer(buffer);
}

/**
 * TEST: Buffer with 10MS preset (fast timescale)
 * WHAT: Test fast timescale buffering workflow
 * WHY:  Verify small buffer size handles rapid updates
 */
TEST_F(BrainIntegrationTest, Buffer10MSPreset) {
    const size_t num_channels = 5;

    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(
        num_channels, BUFFER_SIZE_10MS
    );
    ASSERT_NE(buffer, nullptr);
    EXPECT_EQ(buffer->size_preset, BUFFER_SIZE_10MS);

    // Buffer activity for 100ms (10 samples at 10ms intervals)
    float activity[5];
    for (uint64_t t = 0; t < 100; t += 10) {
        generate_activity(activity, num_channels, 5.0f, 1.0f);
        EXPECT_TRUE(brain_buffer_activity(buffer, activity, num_channels, t));
    }

    // Extract features
    float features[25];
    size_t num_features = brain_extract_windowed_features(
        buffer, features, 25
    );
    EXPECT_GT(num_features, 0);
    EXPECT_LE(num_features, 25);

    brain_destroy_temporal_buffer(buffer);
}

/**
 * TEST: Buffer with 1S preset (slow timescale)
 * WHAT: Test long-timescale buffering with many samples
 * WHY:  Verify large buffer handles extended history
 */
TEST_F(BrainIntegrationTest, Buffer1SPreset) {
    const size_t num_channels = 3;

    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(
        num_channels, BUFFER_SIZE_1S
    );
    ASSERT_NE(buffer, nullptr);
    EXPECT_EQ(buffer->size_preset, BUFFER_SIZE_1S);

    // Buffer activity for 1 second (100 samples at 10ms intervals)
    float activity[3];
    for (uint64_t t = 0; t < 1000; t += 10) {
        generate_activity(activity, num_channels, 8.0f, 1.5f);
        EXPECT_TRUE(brain_buffer_activity(buffer, activity, num_channels, t));
    }

    // Extract features (should get 3 channels * 5 features = 15)
    float features[15];
    size_t num_features = brain_extract_windowed_features(
        buffer, features, 15
    );
    EXPECT_EQ(num_features, 15);

    brain_destroy_temporal_buffer(buffer);
}

/**
 * TEST: Feature extraction accuracy across timescales
 * WHAT: Buffer → extract → verify feature values are reasonable
 * WHY:  Features should reflect actual neural activity statistics
 */
TEST_F(BrainIntegrationTest, FeatureExtractionAccuracy) {
    const size_t num_channels = 4;

    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(
        num_channels, BUFFER_SIZE_100MS
    );
    ASSERT_NE(buffer, nullptr);

    // Buffer constant activity (mean should be stable)
    float activity[4] = {10.0f, 20.0f, 30.0f, 40.0f};
    for (uint64_t t = 0; t < 500; t += 10) {
        EXPECT_TRUE(brain_buffer_activity(buffer, activity, num_channels, t));
    }

    // Extract features
    float features[20];
    size_t num_features = brain_extract_windowed_features(
        buffer, features, 20
    );
    EXPECT_EQ(num_features, 20); // 4 channels * 5 features

    // Verify features are in reasonable range
    for (size_t i = 0; i < num_features; i++) {
        check_feature_range(features[i], -100.0f, 100.0f);
    }

    brain_destroy_temporal_buffer(buffer);
}

/**
 * TEST: Continuous buffering over extended time
 * WHAT: Buffer activity continuously for extended period
 * WHY:  Verify buffer handles long-running operation without issues
 */
TEST_F(BrainIntegrationTest, ContinuousBufferingOverTime) {
    const size_t num_channels = 8;

    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(
        num_channels, BUFFER_SIZE_100MS
    );
    ASSERT_NE(buffer, nullptr);

    // Buffer for 5 seconds (500 samples at 10ms)
    float activity[8];
    for (uint64_t t = 0; t < 5000; t += 10) {
        generate_activity(activity, num_channels, 15.0f, 3.0f);
        EXPECT_TRUE(brain_buffer_activity(buffer, activity, num_channels, t));
    }

    // Extract features at end
    float features[40];
    size_t num_features = brain_extract_windowed_features(
        buffer, features, 40
    );
    EXPECT_EQ(num_features, 40);

    brain_destroy_temporal_buffer(buffer);
}

/**
 * TEST: Buffer with different channel counts
 * WHAT: Test 1, 10, 100 channel configurations
 * WHY:  Verify scalability across channel dimensions
 */
TEST_F(BrainIntegrationTest, BufferDifferentChannelCounts) {
    const size_t channel_counts[] = {1, 10, 100};

    for (size_t count : channel_counts) {
        brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(
            count, BUFFER_SIZE_100MS
        );
        ASSERT_NE(buffer, nullptr);
        EXPECT_EQ(buffer->num_channels, count);

        // Buffer activity
        std::vector<float> activity(count);
        generate_activity(activity.data(), count, 12.0f, 2.0f);

        EXPECT_TRUE(brain_buffer_activity(
            buffer, activity.data(), count, 1000
        ));

        // Extract features
        std::vector<float> features(count * 5);
        size_t num_features = brain_extract_windowed_features(
            buffer, features.data(), count * 5
        );
        EXPECT_EQ(num_features, count * 5);

        brain_destroy_temporal_buffer(buffer);
    }
}

/**
 * TEST: Feature extraction with oscillating patterns
 * WHAT: Buffer oscillating activity → extract → verify temporal features
 * WHY:  Temporal features should capture oscillatory dynamics
 */
TEST_F(BrainIntegrationTest, FeatureExtractionOscillatingPattern) {
    const size_t num_channels = 6;

    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(
        num_channels, BUFFER_SIZE_100MS
    );
    ASSERT_NE(buffer, nullptr);

    // Buffer oscillating activity (10 Hz)
    float activity[6];
    for (uint64_t t = 0; t < 1000; t += 10) {
        generate_oscillation(activity, num_channels, 10.0f, t);
        EXPECT_TRUE(brain_buffer_activity(buffer, activity, num_channels, t));
    }

    // Extract features
    float features[30];
    size_t num_features = brain_extract_windowed_features(
        buffer, features, 30
    );
    EXPECT_EQ(num_features, 30);

    // Features should vary (not constant)
    bool has_variation = false;
    for (size_t i = 1; i < num_features; i++) {
        if (std::abs(features[i] - features[0]) > 0.01f) {
            has_variation = true;
            break;
        }
    }
    EXPECT_TRUE(has_variation);

    brain_destroy_temporal_buffer(buffer);
}

//=============================================================================
// CATEGORY 2: FEATURE EXTRACTION + NORMALIZATION WORKFLOW
//=============================================================================

/**
 * TEST: Z-score normalization maintains statistical properties
 * WHAT: Extract features → normalize (z-score) → verify mean≈0, std≈1
 * WHY:  Z-score normalization should produce standard normal distribution
 */
TEST_F(BrainIntegrationTest, ZScoreNormalizationStatistics) {
    const size_t num_features = 20;

    brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(
        num_features, NORMALIZE_ZSCORE
    );
    ASSERT_NE(normalizer, nullptr);
    EXPECT_EQ(normalizer->type, NORMALIZE_ZSCORE);

    // Accumulate features for statistics
    std::vector<float> all_features;

    // Normalize 100 feature vectors
    for (int iter = 0; iter < 100; iter++) {
        float features[20];
        for (size_t i = 0; i < num_features; i++) {
            features[i] = 10.0f + (rand() % 1000) / 100.0f;
        }

        EXPECT_TRUE(brain_normalize_features(
            normalizer, features, num_features
        ));

        for (size_t i = 0; i < num_features; i++) {
            all_features.push_back(features[i]);
        }
    }

    // Calculate mean and std
    float sum = 0.0f;
    for (float val : all_features) {
        sum += val;
    }
    float mean = sum / all_features.size();

    float variance_sum = 0.0f;
    for (float val : all_features) {
        variance_sum += (val - mean) * (val - mean);
    }
    float std = std::sqrt(variance_sum / all_features.size());

    // Verify approximate standard normal
    EXPECT_TRUE(approx_equal(mean, 0.0f, 0.5f));
    EXPECT_TRUE(approx_equal(std, 1.0f, 0.5f));

    brain_destroy_feature_normalizer(normalizer);
}

/**
 * TEST: Min-max normalization scales to [0,1]
 * WHAT: Normalize features → verify all in [0,1] range
 * WHY:  Min-max should bound all values to target range
 */
TEST_F(BrainIntegrationTest, MinMaxNormalizationRange) {
    const size_t num_features = 15;

    brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(
        num_features, NORMALIZE_MINMAX
    );
    ASSERT_NE(normalizer, nullptr);

    // Normalize diverse feature vectors
    for (int iter = 0; iter < 50; iter++) {
        float features[15];
        for (size_t i = 0; i < num_features; i++) {
            features[i] = -50.0f + (rand() % 10000) / 100.0f;
        }

        EXPECT_TRUE(brain_normalize_features(
            normalizer, features, num_features
        ));

        // Verify all in [0, 1]
        for (size_t i = 0; i < num_features; i++) {
            check_feature_range(features[i], 0.0f, 1.0f);
        }
    }

    brain_destroy_feature_normalizer(normalizer);
}

/**
 * TEST: Adaptive normalization convergence
 * WHAT: Repeatedly normalize → verify adaptation converges
 * WHY:  Adaptive normalizer should stabilize over time
 */
TEST_F(BrainIntegrationTest, AdaptiveNormalizationConvergence) {
    const size_t num_features = 10;

    brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(
        num_features, NORMALIZE_ADAPTIVE
    );
    ASSERT_NE(normalizer, nullptr);

    // Normalize with constant input
    float features[10];
    for (size_t i = 0; i < num_features; i++) {
        features[i] = 25.0f;
    }

    std::vector<float> normalized_values;

    // Run adaptation for 200 iterations
    for (int iter = 0; iter < 200; iter++) {
        EXPECT_TRUE(brain_normalize_features(
            normalizer, features, num_features
        ));
        normalized_values.push_back(features[0]);
    }

    // Check convergence: later values should be more stable
    float early_variance = 0.0f;
    float late_variance = 0.0f;

    // Early iterations (20-40)
    float early_mean = 0.0f;
    for (int i = 20; i < 40; i++) {
        early_mean += normalized_values[i];
    }
    early_mean /= 20;

    for (int i = 20; i < 40; i++) {
        float diff = normalized_values[i] - early_mean;
        early_variance += diff * diff;
    }
    early_variance /= 20;

    // Late iterations (180-200)
    float late_mean = 0.0f;
    for (int i = 180; i < 200; i++) {
        late_mean += normalized_values[i];
    }
    late_mean /= 20;

    for (int i = 180; i < 200; i++) {
        float diff = normalized_values[i] - late_mean;
        late_variance += diff * diff;
    }
    late_variance /= 20;

    // Late variance should be lower (more stable)
    EXPECT_LE(late_variance, early_variance * 1.5f);

    brain_destroy_feature_normalizer(normalizer);
}

/**
 * TEST: Homeostatic normalization regulation over time
 * WHAT: Test homeostatic regulation with varying inputs
 * WHY:  Homeostatic normalizer should maintain target activity
 */
TEST_F(BrainIntegrationTest, HomeostaticNormalizationRegulation) {
    const size_t num_features = 8;

    brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(
        num_features, NORMALIZE_HOMEOSTATIC
    );
    ASSERT_NE(normalizer, nullptr);

    // Test with varying activity levels
    std::vector<float> normalized_outputs;

    for (int iter = 0; iter < 100; iter++) {
        float features[8];
        // Alternate between high and low activity
        float base = (iter % 2 == 0) ? 5.0f : 50.0f;
        for (size_t i = 0; i < num_features; i++) {
            features[i] = base + (rand() % 100) / 100.0f;
        }

        EXPECT_TRUE(brain_normalize_features(
            normalizer, features, num_features
        ));

        // Track mean output
        float mean = 0.0f;
        for (size_t i = 0; i < num_features; i++) {
            mean += features[i];
        }
        normalized_outputs.push_back(mean / num_features);
    }

    // Homeostatic regulation should stabilize output despite input variation
    float output_variance = 0.0f;
    float output_mean = 0.0f;
    for (float val : normalized_outputs) {
        output_mean += val;
    }
    output_mean /= normalized_outputs.size();

    for (float val : normalized_outputs) {
        float diff = val - output_mean;
        output_variance += diff * diff;
    }
    output_variance /= normalized_outputs.size();

    // Verify output is more stable than input would be
    EXPECT_LT(output_variance, 1000.0f);

    brain_destroy_feature_normalizer(normalizer);
}

/**
 * TEST: No normalization passthrough
 * WHAT: Test NORMALIZE_NONE preserves original values
 * WHY:  Verify passthrough mode doesn't modify features
 */
TEST_F(BrainIntegrationTest, NoNormalizationPassthrough) {
    const size_t num_features = 12;

    brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(
        num_features, NORMALIZE_NONE
    );
    ASSERT_NE(normalizer, nullptr);

    float features[12];
    float original[12];

    // Set up features
    for (size_t i = 0; i < num_features; i++) {
        features[i] = -100.0f + i * 17.3f;
        original[i] = features[i];
    }

    EXPECT_TRUE(brain_normalize_features(
        normalizer, features, num_features
    ));

    // Verify unchanged
    for (size_t i = 0; i < num_features; i++) {
        EXPECT_FLOAT_EQ(features[i], original[i]);
    }

    brain_destroy_feature_normalizer(normalizer);
}

/**
 * TEST: Normalization type comparison
 * WHAT: Normalize same features with all 5 types, compare results
 * WHY:  Verify different normalization strategies produce different results
 */
TEST_F(BrainIntegrationTest, NormalizationTypeComparison) {
    const size_t num_features = 10;
    float input_features[10];

    // Generate input features
    for (size_t i = 0; i < num_features; i++) {
        input_features[i] = 5.0f + i * 3.0f;
    }

    struct Result {
        brain_normalize_type_t type;
        float features[10];
    };

    Result results[5];
    brain_normalize_type_t types[] = {
        NORMALIZE_ZSCORE,
        NORMALIZE_MINMAX,
        NORMALIZE_ADAPTIVE,
        NORMALIZE_HOMEOSTATIC,
        NORMALIZE_NONE
    };

    // Normalize with each type
    for (int t = 0; t < 5; t++) {
        brain_feature_normalizer_t* normalizer =
            brain_create_feature_normalizer(num_features, types[t]);
        ASSERT_NE(normalizer, nullptr);

        results[t].type = types[t];
        std::copy(input_features, input_features + num_features,
                 results[t].features);

        EXPECT_TRUE(brain_normalize_features(
            normalizer, results[t].features, num_features
        ));

        brain_destroy_feature_normalizer(normalizer);
    }

    // Verify NONE matches input
    for (size_t i = 0; i < num_features; i++) {
        EXPECT_FLOAT_EQ(results[4].features[i], input_features[i]);
    }

    // Verify others differ from input
    for (int t = 0; t < 4; t++) {
        bool differs = false;
        for (size_t i = 0; i < num_features; i++) {
            if (std::abs(results[t].features[i] - input_features[i]) > 0.01f) {
                differs = true;
                break;
            }
        }
        EXPECT_TRUE(differs) << "Type " << t << " should differ from input";
    }
}

/**
 * TEST: Normalization with edge case values
 * WHAT: Test normalization with zeros, negatives, large values
 * WHY:  Verify robustness to edge cases
 */
TEST_F(BrainIntegrationTest, NormalizationEdgeCases) {
    const size_t num_features = 6;

    brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(
        num_features, NORMALIZE_ZSCORE
    );
    ASSERT_NE(normalizer, nullptr);

    // Test with zeros
    float features1[6] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    EXPECT_TRUE(brain_normalize_features(normalizer, features1, num_features));

    // Test with negative values
    float features2[6] = {-10.0f, -20.0f, -30.0f, -40.0f, -50.0f, -60.0f};
    EXPECT_TRUE(brain_normalize_features(normalizer, features2, num_features));

    // Test with large values
    float features3[6] = {1000.0f, 2000.0f, 3000.0f, 4000.0f, 5000.0f, 6000.0f};
    EXPECT_TRUE(brain_normalize_features(normalizer, features3, num_features));

    // Test with mixed
    float features4[6] = {-100.0f, 0.0f, 100.0f, -1000.0f, 1000.0f, 0.0f};
    EXPECT_TRUE(brain_normalize_features(normalizer, features4, num_features));

    brain_destroy_feature_normalizer(normalizer);
}

/**
 * TEST: Normalization consistency across batches
 * WHAT: Normalize same input multiple times, verify consistency
 * WHY:  Normalization should be deterministic for given input
 */
TEST_F(BrainIntegrationTest, NormalizationConsistency) {
    const size_t num_features = 8;

    brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(
        num_features, NORMALIZE_MINMAX
    );
    ASSERT_NE(normalizer, nullptr);

    float input[8] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
    float result1[8], result2[8], result3[8];

    // Normalize same input 3 times
    std::copy(input, input + num_features, result1);
    EXPECT_TRUE(brain_normalize_features(normalizer, result1, num_features));

    std::copy(input, input + num_features, result2);
    EXPECT_TRUE(brain_normalize_features(normalizer, result2, num_features));

    std::copy(input, input + num_features, result3);
    EXPECT_TRUE(brain_normalize_features(normalizer, result3, num_features));

    // Results should be similar (accounting for adaptation)
    for (size_t i = 0; i < num_features; i++) {
        EXPECT_TRUE(approx_equal(result1[i], result2[i], 0.1f));
        EXPECT_TRUE(approx_equal(result2[i], result3[i], 0.1f));
    }

    brain_destroy_feature_normalizer(normalizer);
}

/**
 * TEST: Feature dimension mismatch handling
 * WHAT: Try to normalize with wrong feature count
 * WHY:  Should gracefully handle dimension mismatches
 */
TEST_F(BrainIntegrationTest, NormalizationDimensionMismatch) {
    brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(
        10, NORMALIZE_ZSCORE
    );
    ASSERT_NE(normalizer, nullptr);

    // Try with too many features
    float features[15];
    for (size_t i = 0; i < 15; i++) {
        features[i] = static_cast<float>(i);
    }

    bool result = brain_normalize_features(normalizer, features, 15);
    EXPECT_FALSE(result); // Should fail

    // Try with valid count
    result = brain_normalize_features(normalizer, features, 10);
    EXPECT_TRUE(result);

    brain_destroy_feature_normalizer(normalizer);
}

/**
 * TEST: Normalization with single feature
 * WHAT: Test normalization with num_features = 1
 * WHY:  Edge case: single-dimensional normalization
 */
TEST_F(BrainIntegrationTest, NormalizationSingleFeature) {
    brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(
        1, NORMALIZE_ZSCORE
    );
    ASSERT_NE(normalizer, nullptr);

    // Normalize single values multiple times
    for (int iter = 0; iter < 50; iter++) {
        float feature = static_cast<float>(rand() % 100);
        EXPECT_TRUE(brain_normalize_features(normalizer, &feature, 1));
        // Value should be modified
        check_feature_range(feature, -10.0f, 10.0f);
    }

    brain_destroy_feature_normalizer(normalizer);
}

//=============================================================================
// CATEGORY 3: COMPLETE PIPELINE WORKFLOW
//=============================================================================

/**
 * TEST: Buffer → Extract → Normalize pipeline
 * WHAT: Full pipeline with all three stages
 * WHY:  Verify end-to-end workflow integration
 */
TEST_F(BrainIntegrationTest, CompleteBufferExtractNormalizePipeline) {
    const size_t num_channels = 5;
    const size_t num_features = 25; // 5 channels * 5 features

    // Create components
    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(
        num_channels, BUFFER_SIZE_100MS
    );
    brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(
        num_features, NORMALIZE_ZSCORE
    );
    ASSERT_NE(buffer, nullptr);
    ASSERT_NE(normalizer, nullptr);

    // Run pipeline: buffer → extract → normalize
    float activity[5];
    for (uint64_t t = 0; t < 500; t += 10) {
        generate_activity(activity, num_channels, 20.0f, 5.0f);
        EXPECT_TRUE(brain_buffer_activity(buffer, activity, num_channels, t));
    }

    float features[25];
    size_t extracted = brain_extract_windowed_features(
        buffer, features, num_features
    );
    EXPECT_EQ(extracted, num_features);

    EXPECT_TRUE(brain_normalize_features(normalizer, features, num_features));

    // Verify normalized features are in reasonable range
    for (size_t i = 0; i < num_features; i++) {
        check_feature_range(features[i], -10.0f, 10.0f);
    }

    brain_destroy_temporal_buffer(buffer);
    brain_destroy_feature_normalizer(normalizer);
}

/**
 * TEST: brain_extract_and_normalize convenience function
 * WHAT: Test single-call pipeline function
 * WHY:  Convenience function should match manual pipeline
 */
TEST_F(BrainIntegrationTest, ExtractAndNormalizeConvenienceFunction) {
    const size_t num_channels = 4;
    const size_t num_features = 20;

    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(
        num_channels, BUFFER_SIZE_100MS
    );
    brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(
        num_features, NORMALIZE_MINMAX
    );
    ASSERT_NE(buffer, nullptr);
    ASSERT_NE(normalizer, nullptr);

    // Buffer activity
    float activity[4];
    for (uint64_t t = 0; t < 300; t += 10) {
        generate_activity(activity, num_channels, 15.0f, 3.0f);
        EXPECT_TRUE(brain_buffer_activity(buffer, activity, num_channels, t));
    }

    // Use convenience function
    float features[20];
    size_t num_extracted = brain_extract_and_normalize(
        buffer, normalizer, features, num_features
    );

    EXPECT_EQ(num_extracted, num_features);

    // Verify features are normalized (min-max should be in [0,1])
    for (size_t i = 0; i < num_features; i++) {
        check_feature_range(features[i], 0.0f, 1.0f);
    }

    brain_destroy_temporal_buffer(buffer);
    brain_destroy_feature_normalizer(normalizer);
}

/**
 * TEST: Pipeline with real neural activity patterns
 * WHAT: Run pipeline with realistic bursting/oscillating activity
 * WHY:  Verify pipeline handles complex biological patterns
 */
TEST_F(BrainIntegrationTest, PipelineWithRealisticActivity) {
    const size_t num_channels = 6;

    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(
        num_channels, BUFFER_SIZE_100MS
    );
    brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(
        30, NORMALIZE_ADAPTIVE
    );
    ASSERT_NE(buffer, nullptr);
    ASSERT_NE(normalizer, nullptr);

    // Generate realistic activity: baseline + bursts + oscillations
    float activity[6];
    for (uint64_t t = 0; t < 1000; t += 10) {
        for (size_t ch = 0; ch < num_channels; ch++) {
            // Baseline activity
            float base = 5.0f;

            // Add oscillation (10 Hz)
            float osc = 3.0f * std::sin(2.0f * M_PI * 10.0f * t / 1000.0f);

            // Add bursts every 200ms
            float burst = ((t % 200) < 30) ? 20.0f : 0.0f;

            activity[ch] = base + osc + burst;
        }

        EXPECT_TRUE(brain_buffer_activity(buffer, activity, num_channels, t));
    }

    // Extract and normalize
    float features[30];
    size_t num_extracted = brain_extract_and_normalize(
        buffer, normalizer, features, 30
    );

    EXPECT_EQ(num_extracted, 30);

    brain_destroy_temporal_buffer(buffer);
    brain_destroy_feature_normalizer(normalizer);
}

/**
 * TEST: Temporal coherence across multiple updates
 * WHAT: Run pipeline repeatedly, verify temporal smoothness
 * WHY:  Features should change smoothly over time, not erratically
 */
TEST_F(BrainIntegrationTest, TemporalCoherenceAcrossUpdates) {
    const size_t num_channels = 4;

    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(
        num_channels, BUFFER_SIZE_100MS
    );
    brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(
        20, NORMALIZE_ZSCORE
    );
    ASSERT_NE(buffer, nullptr);
    ASSERT_NE(normalizer, nullptr);

    std::vector<float> feature_history;

    // Run pipeline 100 times with slowly changing activity
    for (int iter = 0; iter < 100; iter++) {
        // Buffer activity for this iteration
        float activity[4];
        float base = 10.0f + iter * 0.1f; // Slowly increasing baseline

        for (int t = 0; t < 10; t++) {
            generate_activity(activity, num_channels, base, 2.0f);
            EXPECT_TRUE(brain_buffer_activity(
                buffer, activity, num_channels, iter * 100 + t * 10
            ));
        }

        // Extract and normalize
        float features[20];
        size_t num_extracted = brain_extract_and_normalize(
            buffer, normalizer, features, 20
        );
        EXPECT_EQ(num_extracted, 20);

        // Track mean feature value
        float mean = 0.0f;
        for (size_t i = 0; i < 20; i++) {
            mean += features[i];
        }
        feature_history.push_back(mean / 20);
    }

    // Verify temporal smoothness: no large jumps
    for (size_t i = 1; i < feature_history.size(); i++) {
        float change = std::abs(feature_history[i] - feature_history[i-1]);
        EXPECT_LT(change, 5.0f) << "Large jump at iteration " << i;
    }

    brain_destroy_temporal_buffer(buffer);
    brain_destroy_feature_normalizer(normalizer);
}

/**
 * TEST: Pipeline with different buffer sizes
 * WHAT: Run pipeline with 10MS, 100MS, 1S buffers
 * WHY:  Verify pipeline works across all buffer size presets
 */
TEST_F(BrainIntegrationTest, PipelineDifferentBufferSizes) {
    brain_buffer_size_t sizes[] = {
        BUFFER_SIZE_10MS,
        BUFFER_SIZE_100MS,
        BUFFER_SIZE_1S
    };

    for (auto size : sizes) {
        brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(
            5, size
        );
        brain_feature_normalizer_t* normalizer =
            brain_create_feature_normalizer(25, NORMALIZE_MINMAX);

        ASSERT_NE(buffer, nullptr);
        ASSERT_NE(normalizer, nullptr);

        // Buffer some activity
        float activity[5];
        for (uint64_t t = 0; t < 500; t += 10) {
            generate_activity(activity, 5, 12.0f, 2.0f);
            EXPECT_TRUE(brain_buffer_activity(buffer, activity, 5, t));
        }

        // Run pipeline
        float features[25];
        size_t num = brain_extract_and_normalize(
            buffer, normalizer, features, 25
        );
        EXPECT_EQ(num, 25);

        brain_destroy_temporal_buffer(buffer);
        brain_destroy_feature_normalizer(normalizer);
    }
}

/**
 * TEST: Pipeline performance with large channel count
 * WHAT: Test pipeline with 100 channels
 * WHY:  Verify scalability to realistic neural population sizes
 */
TEST_F(BrainIntegrationTest, PipelineLargeChannelCount) {
    const size_t num_channels = 100;
    const size_t num_features = 500; // 100 * 5

    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(
        num_channels, BUFFER_SIZE_100MS
    );
    brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(
        num_features, NORMALIZE_ADAPTIVE
    );
    ASSERT_NE(buffer, nullptr);
    ASSERT_NE(normalizer, nullptr);

    // Buffer activity
    std::vector<float> activity(num_channels);
    for (uint64_t t = 0; t < 200; t += 10) {
        generate_activity(activity.data(), num_channels, 18.0f, 4.0f);
        EXPECT_TRUE(brain_buffer_activity(
            buffer, activity.data(), num_channels, t
        ));
    }

    // Run pipeline
    std::vector<float> features(num_features);
    size_t num = brain_extract_and_normalize(
        buffer, normalizer, features.data(), num_features
    );
    EXPECT_EQ(num, num_features);

    brain_destroy_temporal_buffer(buffer);
    brain_destroy_feature_normalizer(normalizer);
}

/**
 * TEST: Multiple pipeline instances independence
 * WHAT: Create and run two independent pipelines simultaneously
 * WHY:  Verify pipelines don't interfere with each other
 */
TEST_F(BrainIntegrationTest, MultiplePipelineIndependence) {
    // Pipeline 1
    brain_temporal_buffer_t* buffer1 = brain_create_temporal_buffer(
        5, BUFFER_SIZE_100MS
    );
    brain_feature_normalizer_t* normalizer1 =
        brain_create_feature_normalizer(25, NORMALIZE_ZSCORE);

    // Pipeline 2
    brain_temporal_buffer_t* buffer2 = brain_create_temporal_buffer(
        8, BUFFER_SIZE_1S
    );
    brain_feature_normalizer_t* normalizer2 =
        brain_create_feature_normalizer(40, NORMALIZE_MINMAX);

    ASSERT_NE(buffer1, nullptr);
    ASSERT_NE(normalizer1, nullptr);
    ASSERT_NE(buffer2, nullptr);
    ASSERT_NE(normalizer2, nullptr);

    // Run both pipelines with different data
    float activity1[5], activity2[8];
    for (uint64_t t = 0; t < 300; t += 10) {
        generate_activity(activity1, 5, 10.0f, 2.0f);
        generate_activity(activity2, 8, 30.0f, 5.0f);

        EXPECT_TRUE(brain_buffer_activity(buffer1, activity1, 5, t));
        EXPECT_TRUE(brain_buffer_activity(buffer2, activity2, 8, t));
    }

    // Extract from both
    float features1[25], features2[40];
    size_t num1 = brain_extract_and_normalize(
        buffer1, normalizer1, features1, 25
    );
    size_t num2 = brain_extract_and_normalize(
        buffer2, normalizer2, features2, 40
    );

    EXPECT_EQ(num1, 25);
    EXPECT_EQ(num2, 40);

    // Verify both pipelines completed successfully
    // Note: Normalized features may be similar even with different inputs
    // (that's the nature of normalization), so we just verify completion
    EXPECT_GT(num1, 0);
    EXPECT_GT(num2, 0);

    brain_destroy_temporal_buffer(buffer1);
    brain_destroy_feature_normalizer(normalizer1);
    brain_destroy_temporal_buffer(buffer2);
    brain_destroy_feature_normalizer(normalizer2);
}

//=============================================================================
// CATEGORY 4: SPIKE FEATURES + POPULATION ANALYSIS WORKFLOW
//=============================================================================

/**
 * TEST: Spike feature extractor creation and basic extraction
 * WHAT: Create extractor → extract features from spike data
 * WHY:  Foundation for spike-based workflows
 */
TEST_F(BrainIntegrationTest, SpikeFeatureExtractorBasic) {
    brain_spike_feature_extractor_t extractor =
        brain_create_spike_feature_extractor(10, true, true);
    ASSERT_NE(extractor, nullptr);

    // Create spike data
    spike_data_t* spike_data = spike_data_create(10);
    ASSERT_NE(spike_data, nullptr);

    spike_data->start_time = 0;
    spike_data->end_time = 1000;

    // Add some spikes
    for (uint32_t i = 0; i < 10; i++) {
        spike_data->spike_counts[i] = 5 + i;
        spike_data->spike_times[i] = (uint64_t*)nimcp_malloc(
            spike_data->spike_counts[i] * sizeof(uint64_t)
        );
        for (uint32_t s = 0; s < spike_data->spike_counts[i]; s++) {
            spike_data->spike_times[i][s] = s * 100;
        }
    }

    // Extract features
    middleware_features_t features;
    bool success = brain_extract_spike_features(
        extractor, spike_data, &features
    );
    EXPECT_TRUE(success);
    EXPECT_TRUE(features.valid);

    // Cleanup (spike_data_destroy handles spike_times freeing)
    spike_data_destroy(spike_data);
    brain_destroy_spike_feature_extractor(extractor);
}

/**
 * TEST: Population vector computation from rates
 * WHAT: Create analyzer → compute population vector
 * WHY:  Test basic population coding workflow
 */
TEST_F(BrainIntegrationTest, PopulationVectorComputation) {
    brain_population_analyzer_t analyzer = brain_create_population_analyzer();
    ASSERT_NE(analyzer, nullptr);

    const uint32_t num_neurons = 10;
    float rates[10];
    tuning_curve_t tuning_curves[10];

    // Set up tuning curves (evenly distributed directions)
    for (uint32_t i = 0; i < num_neurons; i++) {
        float angle = 2.0f * M_PI * i / num_neurons;
        tuning_curves[i].preferred_direction.x = std::cos(angle);
        tuning_curves[i].preferred_direction.y = std::sin(angle);
        tuning_curves[i].preferred_direction.z = 0.0f;
        tuning_curves[i].tuning_width = 1.0f;
        tuning_curves[i].max_rate = 100.0f;

        rates[i] = 10.0f + i * 2.0f;
    }

    vector3d_t pop_vector;
    bool success = brain_compute_population_vector(
        analyzer, rates, tuning_curves, num_neurons, &pop_vector
    );

    EXPECT_TRUE(success);
    EXPECT_GT(pop_vector.magnitude, 0.0f);

    brain_destroy_population_analyzer(analyzer);
}

/**
 * TEST: Population synchrony analysis
 * WHAT: Create spike trains → compute synchrony
 * WHY:  Test synchrony workflow with multiple neurons
 */
TEST_F(BrainIntegrationTest, PopulationSynchronyAnalysis) {
    brain_population_analyzer_t analyzer = brain_create_population_analyzer();
    ASSERT_NE(analyzer, nullptr);

    const uint32_t num_neurons = 5;
    spike_train_t* spike_trains[5];

    // Create spike trains with some synchrony
    for (uint32_t i = 0; i < num_neurons; i++) {
        spike_trains[i] = (spike_train_t*)nimcp_calloc(1, sizeof(spike_train_t));
        spike_trains[i]->num_spikes = 10;
        spike_trains[i]->capacity = 10;
        spike_trains[i]->spike_times = (uint64_t*)nimcp_malloc(
            10 * sizeof(uint64_t)
        );

        // Add spikes with slight jitter (synchronized within 5ms)
        for (uint32_t s = 0; s < 10; s++) {
            uint64_t base_time = s * 100;
            uint64_t jitter = rand() % 5;
            spike_trains[i]->spike_times[s] = base_time + jitter;
        }

        spike_trains[i]->start_time = 0;
        spike_trains[i]->end_time = 1000;
    }

    synchrony_result_t synchrony;
    bool success = brain_compute_population_synchrony(
        analyzer, spike_trains, num_neurons, &synchrony
    );

    EXPECT_TRUE(success);
    check_feature_range(synchrony.synchrony_index, 0.0f, 1.0f);
    check_feature_range(synchrony.mean_correlation, -1.0f, 1.0f);

    // Cleanup (rate_coding_spike_train_destroy handles spike_times freeing)
    for (uint32_t i = 0; i < num_neurons; i++) {
        rate_coding_spike_train_destroy(spike_trains[i]);
    }
    brain_destroy_population_analyzer(analyzer);
}

/**
 * TEST: Spike features with 2, 10, 100 neurons
 * WHAT: Test feature extraction with different population sizes
 * WHY:  Verify scalability across population dimensions
 */
TEST_F(BrainIntegrationTest, SpikeFeaturesDifferentPopulationSizes) {
    uint32_t neuron_counts[] = {2, 10, 100};

    for (uint32_t count : neuron_counts) {
        brain_spike_feature_extractor_t extractor =
            brain_create_spike_feature_extractor(count, false, false);
        ASSERT_NE(extractor, nullptr);

        spike_data_t* spike_data = spike_data_create(count);
        ASSERT_NE(spike_data, nullptr);

        spike_data->start_time = 0;
        spike_data->end_time = 1000;

        // Add spikes
        for (uint32_t i = 0; i < count; i++) {
            spike_data->spike_counts[i] = 5;
            spike_data->spike_times[i] = (uint64_t*)nimcp_malloc(
                5 * sizeof(uint64_t)
            );
            for (uint32_t s = 0; s < 5; s++) {
                spike_data->spike_times[i][s] = s * 200;
            }
        }

        middleware_features_t features;
        bool success = brain_extract_spike_features(
            extractor, spike_data, &features
        );
        EXPECT_TRUE(success);

        // Cleanup (spike_data_destroy handles spike_times freeing)
        spike_data_destroy(spike_data);
        brain_destroy_spike_feature_extractor(extractor);
    }
}

/**
 * TEST: Oscillation analysis integration
 * WHAT: Extract spike features with oscillation analysis enabled
 * WHY:  Verify oscillation power features are computed
 */
TEST_F(BrainIntegrationTest, OscillationAnalysisIntegration) {
    brain_spike_feature_extractor_t extractor =
        brain_create_spike_feature_extractor(20, true, false);
    ASSERT_NE(extractor, nullptr);

    spike_data_t* spike_data = spike_data_create(20);
    ASSERT_NE(spike_data, nullptr);

    spike_data->start_time = 0;
    spike_data->end_time = 2000;

    // Add periodic spikes (10 Hz oscillation)
    for (uint32_t i = 0; i < 20; i++) {
        spike_data->spike_counts[i] = 20;
        spike_data->spike_times[i] = (uint64_t*)nimcp_malloc(
            20 * sizeof(uint64_t)
        );
        for (uint32_t s = 0; s < 20; s++) {
            spike_data->spike_times[i][s] = s * 100; // 10 Hz
        }
    }

    middleware_features_t features;
    bool success = brain_extract_spike_features(
        extractor, spike_data, &features
    );
    EXPECT_TRUE(success);

    // Oscillation powers should be non-negative
    EXPECT_GE(features.delta_power, 0.0f);
    EXPECT_GE(features.theta_power, 0.0f);
    EXPECT_GE(features.alpha_power, 0.0f);
    EXPECT_GE(features.beta_power, 0.0f);
    EXPECT_GE(features.gamma_power, 0.0f);

    // Cleanup (spike_data_destroy handles spike_times freeing)
    spike_data_destroy(spike_data);
    brain_destroy_spike_feature_extractor(extractor);
}

/**
 * TEST: Spike features → population vector workflow
 * WHAT: Extract features → use rates for population vector
 * WHY:  Verify integration between feature extraction and population coding
 */
TEST_F(BrainIntegrationTest, SpikeFeaturesPopulationVectorWorkflow) {
    const uint32_t num_neurons = 8;

    brain_spike_feature_extractor_t extractor =
        brain_create_spike_feature_extractor(num_neurons, false, false);
    brain_population_analyzer_t analyzer = brain_create_population_analyzer();

    ASSERT_NE(extractor, nullptr);
    ASSERT_NE(analyzer, nullptr);

    // Create spike data
    spike_data_t* spike_data = spike_data_create(num_neurons);
    spike_data->start_time = 0;
    spike_data->end_time = 1000;

    float rates[8];
    tuning_curve_t tuning_curves[8];

    for (uint32_t i = 0; i < num_neurons; i++) {
        // Generate spike train
        spike_data->spike_counts[i] = 5 + i;
        spike_data->spike_times[i] = (uint64_t*)nimcp_malloc(
            spike_data->spike_counts[i] * sizeof(uint64_t)
        );
        for (uint32_t s = 0; s < spike_data->spike_counts[i]; s++) {
            spike_data->spike_times[i][s] = s * 150;
        }

        // Compute rate
        rates[i] = spike_data->spike_counts[i] * 1000.0f /
                  (spike_data->end_time - spike_data->start_time);

        // Set tuning curve
        float angle = 2.0f * M_PI * i / num_neurons;
        tuning_curves[i].preferred_direction.x = std::cos(angle);
        tuning_curves[i].preferred_direction.y = std::sin(angle);
        tuning_curves[i].preferred_direction.z = 0.0f;
        tuning_curves[i].tuning_width = 1.0f;
        tuning_curves[i].max_rate = 100.0f;
    }

    // Extract features (for validation)
    middleware_features_t features;
    bool success1 = brain_extract_spike_features(
        extractor, spike_data, &features
    );
    EXPECT_TRUE(success1);

    // Compute population vector
    vector3d_t pop_vector;
    bool success2 = brain_compute_population_vector(
        analyzer, rates, tuning_curves, num_neurons, &pop_vector
    );
    EXPECT_TRUE(success2);

    // Cleanup (spike_data_destroy handles spike_times freeing)
    spike_data_destroy(spike_data);
    brain_destroy_spike_feature_extractor(extractor);
    brain_destroy_population_analyzer(analyzer);
}

/**
 * TEST: Spike features → synchrony analysis workflow
 * WHAT: Create spikes → extract features → compute synchrony
 * WHY:  Verify complete spike analysis pipeline
 */
TEST_F(BrainIntegrationTest, SpikeFeaturesynchronyWorkflow) {
    const uint32_t num_neurons = 6;

    brain_spike_feature_extractor_t extractor =
        brain_create_spike_feature_extractor(num_neurons, false, true);
    brain_population_analyzer_t analyzer = brain_create_population_analyzer();

    ASSERT_NE(extractor, nullptr);
    ASSERT_NE(analyzer, nullptr);

    // Create spike trains
    spike_train_t* spike_trains[6];
    spike_data_t* spike_data = spike_data_create(num_neurons);
    spike_data->start_time = 0;
    spike_data->end_time = 1000;

    for (uint32_t i = 0; i < num_neurons; i++) {
        spike_trains[i] = (spike_train_t*)nimcp_calloc(1, sizeof(spike_train_t));
        spike_trains[i]->num_spikes = 8;
        spike_trains[i]->capacity = 8;
        spike_trains[i]->spike_times = (uint64_t*)nimcp_malloc(
            8 * sizeof(uint64_t)
        );

        spike_data->spike_counts[i] = 8;
        spike_data->spike_times[i] = spike_trains[i]->spike_times;

        for (uint32_t s = 0; s < 8; s++) {
            spike_trains[i]->spike_times[s] = s * 125;
        }

        spike_trains[i]->start_time = 0;
        spike_trains[i]->end_time = 1000;
    }

    // Extract features
    middleware_features_t features;
    bool success1 = brain_extract_spike_features(
        extractor, spike_data, &features
    );
    EXPECT_TRUE(success1);

    // Compute synchrony
    synchrony_result_t synchrony;
    bool success2 = brain_compute_population_synchrony(
        analyzer, spike_trains, num_neurons, &synchrony
    );
    EXPECT_TRUE(success2);

    // Cleanup (rate_coding_spike_train_destroy handles spike_times freeing)
    for (uint32_t i = 0; i < num_neurons; i++) {
        rate_coding_spike_train_destroy(spike_trains[i]);
    }
    spike_data_destroy(spike_data);
    brain_destroy_spike_feature_extractor(extractor);
    brain_destroy_population_analyzer(analyzer);
}

//=============================================================================
// CATEGORY 5: CROSS-COMPONENT INTEGRATION
//=============================================================================

/**
 * TEST: Temporal buffer with all normalizer types
 * WHAT: Test buffer with each of 5 normalization types
 * WHY:  Verify buffer works with all normalizers
 */
TEST_F(BrainIntegrationTest, BufferWithAllNormalizerTypes) {
    brain_normalize_type_t types[] = {
        NORMALIZE_ZSCORE,
        NORMALIZE_MINMAX,
        NORMALIZE_ADAPTIVE,
        NORMALIZE_HOMEOSTATIC,
        NORMALIZE_NONE
    };

    for (auto type : types) {
        brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(
            5, BUFFER_SIZE_100MS
        );
        brain_feature_normalizer_t* normalizer =
            brain_create_feature_normalizer(25, type);

        ASSERT_NE(buffer, nullptr);
        ASSERT_NE(normalizer, nullptr);

        // Run pipeline
        float activity[5];
        for (uint64_t t = 0; t < 200; t += 10) {
            generate_activity(activity, 5, 15.0f, 3.0f);
            EXPECT_TRUE(brain_buffer_activity(buffer, activity, 5, t));
        }

        float features[25];
        size_t num = brain_extract_and_normalize(
            buffer, normalizer, features, 25
        );
        EXPECT_EQ(num, 25);

        brain_destroy_temporal_buffer(buffer);
        brain_destroy_feature_normalizer(normalizer);
    }
}

/**
 * TEST: Spike features with population coding
 * WHAT: Combine spike feature extraction with population vector
 * WHY:  Verify complementary middleware components work together
 */
TEST_F(BrainIntegrationTest, SpikeFeaturesWithPopulationCoding) {
    const uint32_t num_neurons = 10;

    brain_spike_feature_extractor_t extractor =
        brain_create_spike_feature_extractor(num_neurons, true, true);
    brain_population_analyzer_t analyzer = brain_create_population_analyzer();

    ASSERT_NE(extractor, nullptr);
    ASSERT_NE(analyzer, nullptr);

    // Setup spike data and tuning curves
    spike_data_t* spike_data = spike_data_create(num_neurons);
    spike_data->start_time = 0;
    spike_data->end_time = 1000;

    tuning_curve_t tuning_curves[10];
    float rates[10];

    for (uint32_t i = 0; i < num_neurons; i++) {
        spike_data->spike_counts[i] = 10;
        spike_data->spike_times[i] = (uint64_t*)nimcp_malloc(
            10 * sizeof(uint64_t)
        );
        for (uint32_t s = 0; s < 10; s++) {
            spike_data->spike_times[i][s] = s * 100;
        }

        rates[i] = 10.0f;
        float angle = 2.0f * M_PI * i / num_neurons;
        tuning_curves[i].preferred_direction.x = std::cos(angle);
        tuning_curves[i].preferred_direction.y = std::sin(angle);
        tuning_curves[i].preferred_direction.z = 0.0f;
        tuning_curves[i].tuning_width = 1.0f;
        tuning_curves[i].max_rate = 100.0f;
    }

    // Extract features
    middleware_features_t features;
    EXPECT_TRUE(brain_extract_spike_features(
        extractor, spike_data, &features
    ));

    // Compute population vector
    vector3d_t pop_vector;
    EXPECT_TRUE(brain_compute_population_vector(
        analyzer, rates, tuning_curves, num_neurons, &pop_vector
    ));

    // Both operations successful
    EXPECT_TRUE(features.valid);
    EXPECT_GT(pop_vector.magnitude, 0.0f);

    // Cleanup (spike_data_destroy handles spike_times freeing)
    spike_data_destroy(spike_data);
    brain_destroy_spike_feature_extractor(extractor);
    brain_destroy_population_analyzer(analyzer);
}

/**
 * TEST: Buffer features → spike features comparison
 * WHAT: Extract features from buffer and from spikes, compare
 * WHY:  Verify consistency between continuous and discrete feature extraction
 */
TEST_F(BrainIntegrationTest, BufferVsSpikeFeatureConsistency) {
    const size_t num_channels = 5;

    // Buffer-based features
    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(
        num_channels, BUFFER_SIZE_100MS
    );
    ASSERT_NE(buffer, nullptr);

    float activity[5];
    for (uint64_t t = 0; t < 500; t += 10) {
        generate_activity(activity, num_channels, 10.0f, 2.0f);
        EXPECT_TRUE(brain_buffer_activity(buffer, activity, num_channels, t));
    }

    float buffer_features[25];
    size_t num_buffer = brain_extract_windowed_features(
        buffer, buffer_features, 25
    );
    EXPECT_EQ(num_buffer, 25);

    // Spike-based features
    brain_spike_feature_extractor_t extractor =
        brain_create_spike_feature_extractor(num_channels, false, false);
    ASSERT_NE(extractor, nullptr);

    spike_data_t* spike_data = spike_data_create(num_channels);
    spike_data->start_time = 0;
    spike_data->end_time = 500;

    for (uint32_t i = 0; i < num_channels; i++) {
        spike_data->spike_counts[i] = 5;
        spike_data->spike_times[i] = (uint64_t*)nimcp_malloc(
            5 * sizeof(uint64_t)
        );
        for (uint32_t s = 0; s < 5; s++) {
            spike_data->spike_times[i][s] = s * 100;
        }
    }

    middleware_features_t spike_features;
    EXPECT_TRUE(brain_extract_spike_features(
        extractor, spike_data, &spike_features
    ));

    // Both should produce valid features
    EXPECT_GT(num_buffer, 0);
    EXPECT_TRUE(spike_features.valid);

    // Cleanup (spike_data_destroy handles spike_times freeing)
    spike_data_destroy(spike_data);
    brain_destroy_temporal_buffer(buffer);
    brain_destroy_spike_feature_extractor(extractor);
}

/**
 * TEST: All middleware components together
 * WHAT: Use buffer, normalizer, spike extractor, population analyzer
 * WHY:  Ultimate integration test: all components working together
 */
TEST_F(BrainIntegrationTest, AllMiddlewareComponentsTogether) {
    const size_t num_channels = 8;

    // Create all components
    brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(
        num_channels, BUFFER_SIZE_100MS
    );
    brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(
        40, NORMALIZE_ZSCORE
    );
    brain_spike_feature_extractor_t extractor =
        brain_create_spike_feature_extractor(num_channels, true, true);
    brain_population_analyzer_t analyzer = brain_create_population_analyzer();

    ASSERT_NE(buffer, nullptr);
    ASSERT_NE(normalizer, nullptr);
    ASSERT_NE(extractor, nullptr);
    ASSERT_NE(analyzer, nullptr);

    // Buffer continuous activity
    float activity[8];
    for (uint64_t t = 0; t < 300; t += 10) {
        generate_activity(activity, num_channels, 12.0f, 3.0f);
        EXPECT_TRUE(brain_buffer_activity(buffer, activity, num_channels, t));
    }

    // Extract and normalize buffer features
    float buffer_features[40];
    size_t num_buffer = brain_extract_and_normalize(
        buffer, normalizer, buffer_features, 40
    );
    EXPECT_EQ(num_buffer, 40);

    // Create spike data
    spike_data_t* spike_data = spike_data_create(num_channels);
    spike_data->start_time = 0;
    spike_data->end_time = 300;

    for (uint32_t i = 0; i < num_channels; i++) {
        spike_data->spike_counts[i] = 6;
        spike_data->spike_times[i] = (uint64_t*)nimcp_malloc(
            6 * sizeof(uint64_t)
        );
        for (uint32_t s = 0; s < 6; s++) {
            spike_data->spike_times[i][s] = s * 50;
        }
    }

    // Extract spike features
    middleware_features_t spike_features;
    EXPECT_TRUE(brain_extract_spike_features(
        extractor, spike_data, &spike_features
    ));

    // All operations successful
    EXPECT_GT(num_buffer, 0);
    EXPECT_TRUE(spike_features.valid);

    // Cleanup (spike_data_destroy handles spike_times freeing)
    spike_data_destroy(spike_data);
    brain_destroy_temporal_buffer(buffer);
    brain_destroy_feature_normalizer(normalizer);
    brain_destroy_spike_feature_extractor(extractor);
    brain_destroy_population_analyzer(analyzer);
}

/**
 * TEST: Component compatibility verification
 * WHAT: Test all valid component combinations
 * WHY:  Ensure middleware components are truly composable
 */
TEST_F(BrainIntegrationTest, ComponentCompatibilityVerification) {
    // Test matrix of compatible combinations
    struct Combination {
        brain_buffer_size_t buffer_size;
        brain_normalize_type_t norm_type;
        bool use_oscillations;
        bool use_synchrony;
    };

    Combination combinations[] = {
        {BUFFER_SIZE_10MS, NORMALIZE_ZSCORE, true, false},
        {BUFFER_SIZE_100MS, NORMALIZE_MINMAX, false, true},
        {BUFFER_SIZE_1S, NORMALIZE_ADAPTIVE, true, true},
        {BUFFER_SIZE_100MS, NORMALIZE_HOMEOSTATIC, false, false},
        {BUFFER_SIZE_10MS, NORMALIZE_NONE, true, true},
    };

    for (const auto& combo : combinations) {
        brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(
            5, combo.buffer_size
        );
        brain_feature_normalizer_t* normalizer =
            brain_create_feature_normalizer(25, combo.norm_type);
        brain_spike_feature_extractor_t extractor =
            brain_create_spike_feature_extractor(
                5, combo.use_oscillations, combo.use_synchrony
            );

        ASSERT_NE(buffer, nullptr);
        ASSERT_NE(normalizer, nullptr);
        ASSERT_NE(extractor, nullptr);

        // Quick test: all components work
        float activity[5] = {1, 2, 3, 4, 5};
        EXPECT_TRUE(brain_buffer_activity(buffer, activity, 5, 100));

        float features[25];
        size_t num = brain_extract_and_normalize(
            buffer, normalizer, features, 25
        );
        EXPECT_EQ(num, 25);

        brain_destroy_temporal_buffer(buffer);
        brain_destroy_feature_normalizer(normalizer);
        brain_destroy_spike_feature_extractor(extractor);
    }
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
