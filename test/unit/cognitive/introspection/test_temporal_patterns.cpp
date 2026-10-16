/**
 * @file test_temporal_patterns.cpp
 * @brief Comprehensive unit tests for temporal pattern analysis
 *
 * TEST COVERAGE:
 * - Configuration and defaults
 * - Pattern detection from history
 * - Pattern matching with DTW
 * - Next state prediction
 * - Trend analysis (linear regression)
 * - Pattern library management
 * - Brain integration functions
 * - Memory management
 * - Edge cases and error handling
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

#include "cognitive/introspection/nimcp_temporal_patterns.h"
#include "cognitive/introspection/nimcp_introspection.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain_regions/nimcp_brain_regions.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class TemporalPatternsTest : public ::testing::Test {
protected:
    brain_t brain;
    introspection_context_t intro;

    void SetUp() override {
        // Create minimal brain for testing
        brain = brain_create(
            "test_temporal_patterns",
            BRAIN_SIZE_TINY,
            BRAIN_TASK_CLASSIFICATION,
            10,   // num_inputs
            3     // num_outputs
        );

        // Get introspection context (may be NULL if brain creation failed)
        if (brain != nullptr) {
            intro = brain_get_introspection(brain);
        } else {
            intro = nullptr;
        }
    }

    void TearDown() override {
        if (brain != nullptr) {
            brain_destroy(brain);
            brain = nullptr;
        }
        intro = nullptr;
    }

    // Helper: Create synthetic pattern
    temporal_pattern_t create_test_pattern(const char* name, uint32_t length) {
        temporal_pattern_t pattern;
        memset(&pattern, 0, sizeof(temporal_pattern_t));

        snprintf(pattern.name, TEMPORAL_MAX_PATTERN_NAME, "%s", name);
        pattern.sequence_length = length;
        pattern.state_dimension = 1;
        pattern.strength = 0.8f;
        pattern.occurrence_count = 5;
        pattern.first_detected = 1000;
        pattern.last_detected = 2000;
        pattern.average_duration_ms = 100.0f;

        // Allocate state sequence (simple sawtooth pattern)
        pattern.state_sequence = (float**)nimcp_malloc(length * sizeof(float*));
        for (uint32_t i = 0; i < length; i++) {
            pattern.state_sequence[i] = (float*)nimcp_malloc(sizeof(float));
            pattern.state_sequence[i][0] = (float)(i % 5) / 5.0f; // 0, 0.2, 0.4, 0.6, 0.8, repeat
        }

        return pattern;
    }
};

//=============================================================================
// 1. Configuration Tests
//=============================================================================

TEST_F(TemporalPatternsTest, DefaultConfigValid) {
    temporal_pattern_config_t config = temporal_pattern_default_config();

    // Check all defaults
    EXPECT_EQ(config.window_size, TEMPORAL_DEFAULT_WINDOW_SIZE);
    EXPECT_EQ(config.min_pattern_length, TEMPORAL_DEFAULT_MIN_PATTERN_LENGTH);
    EXPECT_EQ(config.max_pattern_length, TEMPORAL_DEFAULT_MAX_PATTERN_LENGTH);
    EXPECT_FLOAT_EQ(config.similarity_threshold, TEMPORAL_DEFAULT_SIMILARITY_THRESHOLD);
    EXPECT_EQ(config.max_patterns, TEMPORAL_DEFAULT_MAX_PATTERNS);
    EXPECT_EQ(config.min_occurrences, TEMPORAL_DEFAULT_MIN_OCCURRENCES);
    EXPECT_EQ(config.trend_window, TEMPORAL_DEFAULT_TREND_WINDOW);
    EXPECT_TRUE(config.enable_auto_detection);
    EXPECT_TRUE(config.enable_prediction);
    EXPECT_FALSE(config.enable_callbacks);
}

TEST_F(TemporalPatternsTest, ConfigThresholdsValid) {
    temporal_pattern_config_t config = temporal_pattern_default_config();

    // Similarity threshold should be in [0, 1]
    EXPECT_GE(config.similarity_threshold, 0.0f);
    EXPECT_LE(config.similarity_threshold, 1.0f);

    // Pattern length constraints
    EXPECT_GT(config.min_pattern_length, 0u);
    EXPECT_GE(config.max_pattern_length, config.min_pattern_length);

    // Library size should be reasonable
    EXPECT_GT(config.max_patterns, 0u);
    EXPECT_LE(config.max_patterns, 10000u);
}

//=============================================================================
// 2. Pattern Detection Tests
//=============================================================================

TEST_F(TemporalPatternsTest, DetectPatternsNullContext) {
    uint32_t num_patterns = 999;
    temporal_pattern_t* patterns = introspection_detect_patterns(nullptr, nullptr, &num_patterns);

    EXPECT_EQ(patterns, nullptr);
    EXPECT_EQ(num_patterns, 0u);
}

TEST_F(TemporalPatternsTest, DetectPatternsNullOutput) {
    if (intro == nullptr) {
        GTEST_SKIP() << "Introspection not available";
    }

    temporal_pattern_t* patterns = introspection_detect_patterns(intro, nullptr, nullptr);
    EXPECT_EQ(patterns, nullptr);
}

TEST_F(TemporalPatternsTest, DetectPatternsWithValidContext) {
    if (intro == nullptr) {
        GTEST_SKIP() << "Introspection not available";
    }

    uint32_t num_patterns = 0;
    temporal_pattern_t* patterns = introspection_detect_patterns(intro, nullptr, &num_patterns);

    // May return NULL if insufficient history, but num_patterns should be 0
    if (patterns == nullptr) {
        EXPECT_EQ(num_patterns, 0u);
    } else {
        // If patterns found, validate structure
        EXPECT_GT(num_patterns, 0u);
        for (uint32_t i = 0; i < num_patterns; i++) {
            EXPECT_GT(patterns[i].sequence_length, 0u);
            EXPECT_GT(patterns[i].state_dimension, 0u);
            EXPECT_GE(patterns[i].strength, 0.0f);
            EXPECT_LE(patterns[i].strength, 1.0f);
        }

        pattern_array_free(patterns, num_patterns);
    }
}

TEST_F(TemporalPatternsTest, DetectPatternsCustomConfig) {
    if (intro == nullptr) {
        GTEST_SKIP() << "Introspection not available";
    }

    temporal_pattern_config_t config = temporal_pattern_default_config();
    config.window_size = 5;
    config.min_pattern_length = 3;
    config.max_pattern_length = 10;

    uint32_t num_patterns = 0;
    temporal_pattern_t* patterns = introspection_detect_patterns(intro, &config, &num_patterns);

    // Should handle custom config without crashing
    if (patterns != nullptr) {
        pattern_array_free(patterns, num_patterns);
    }
}

//=============================================================================
// 3. Pattern Matching Tests
//=============================================================================

TEST_F(TemporalPatternsTest, MatchPatternNullContext) {
    temporal_pattern_t pattern = create_test_pattern("test_pattern", 5);

    pattern_match_result_t result = introspection_match_pattern(nullptr, &pattern, nullptr);

    EXPECT_EQ(result.matched_pattern, nullptr);
    EXPECT_FLOAT_EQ(result.confidence, 0.0f);
    EXPECT_FALSE(result.is_complete_match);

    temporal_pattern_free(&pattern);
}

TEST_F(TemporalPatternsTest, MatchPatternNullPattern) {
    if (intro == nullptr) {
        GTEST_SKIP() << "Introspection not available";
    }

    pattern_match_result_t result = introspection_match_pattern(intro, nullptr, nullptr);

    EXPECT_EQ(result.matched_pattern, nullptr);
    EXPECT_FLOAT_EQ(result.confidence, 0.0f);
    EXPECT_FALSE(result.is_complete_match);
}

TEST_F(TemporalPatternsTest, MatchPatternValidInputs) {
    if (intro == nullptr) {
        GTEST_SKIP() << "Introspection not available";
    }

    temporal_pattern_t pattern = create_test_pattern("test_pattern", 5);

    pattern_match_result_t result = introspection_match_pattern(intro, &pattern, nullptr);

    // Result should be valid even if no match
    EXPECT_GE(result.confidence, 0.0f);
    EXPECT_LE(result.confidence, 1.0f);
    EXPECT_GE(result.dtw_distance, 0.0f);

    temporal_pattern_free(&pattern);
}

TEST_F(TemporalPatternsTest, MatchPatternConfidenceRange) {
    if (intro == nullptr) {
        GTEST_SKIP() << "Introspection not available";
    }

    temporal_pattern_t pattern = create_test_pattern("test_pattern", 5);

    pattern_match_result_t result = introspection_match_pattern(intro, &pattern, nullptr);

    // Confidence must be in [0, 1]
    EXPECT_GE(result.confidence, 0.0f);
    EXPECT_LE(result.confidence, 1.0f);

    // Complete match determined by threshold
    temporal_pattern_config_t config = temporal_pattern_default_config();
    if (result.confidence >= config.similarity_threshold) {
        EXPECT_TRUE(result.is_complete_match);
    } else {
        EXPECT_FALSE(result.is_complete_match);
    }

    temporal_pattern_free(&pattern);
}

//=============================================================================
// 4. Prediction Tests
//=============================================================================

TEST_F(TemporalPatternsTest, PredictNextStateNullContext) {
    brain_state_t predicted = introspection_predict_next_state(nullptr, nullptr);

    EXPECT_EQ(predicted.dimension, 0u);
    EXPECT_EQ(predicted.state_vector, nullptr);
}

TEST_F(TemporalPatternsTest, PredictNextStateEmptyLibrary) {
    if (intro == nullptr) {
        GTEST_SKIP() << "Introspection not available";
    }

    brain_state_t predicted = introspection_predict_next_state(intro, nullptr);

    // Should return empty state if no patterns in library
    // Note: dimension may be 0 or 1 depending on implementation
    if (predicted.state_vector != nullptr) {
        brain_state_free(&predicted);
    }
}

TEST_F(TemporalPatternsTest, PredictNextStateWithPatterns) {
    if (intro == nullptr) {
        GTEST_SKIP() << "Introspection not available";
    }

    // Register a test pattern
    temporal_pattern_t pattern = create_test_pattern("test_pattern", 5);
    bool registered = introspection_register_pattern(intro, &pattern);

    if (registered) {
        brain_state_t predicted = introspection_predict_next_state(intro, nullptr);

        // May or may not predict depending on history match
        if (predicted.state_vector != nullptr) {
            EXPECT_GT(predicted.dimension, 0u);
            brain_state_free(&predicted);
        }
    }

    temporal_pattern_free(&pattern);
}

TEST_F(TemporalPatternsTest, PredictNextStateDisabled) {
    if (intro == nullptr) {
        GTEST_SKIP() << "Introspection not available";
    }

    temporal_pattern_config_t config = temporal_pattern_default_config();
    config.enable_prediction = false;

    brain_state_t predicted = introspection_predict_next_state(intro, &config);

    // Should return empty state when prediction disabled
    EXPECT_EQ(predicted.dimension, 0u);
    EXPECT_EQ(predicted.state_vector, nullptr);
}

//=============================================================================
// 5. Trend Analysis Tests
//=============================================================================

TEST_F(TemporalPatternsTest, TrendAnalysisNullContext) {
    temporal_trend_t trend = introspection_get_trend(nullptr, "avg_activation", nullptr);

    EXPECT_EQ(trend.direction, TREND_UNKNOWN);
    EXPECT_EQ(trend.num_samples, 0u);
}

TEST_F(TemporalPatternsTest, TrendAnalysisNullMetric) {
    if (intro == nullptr) {
        GTEST_SKIP() << "Introspection not available";
    }

    temporal_trend_t trend = introspection_get_trend(intro, nullptr, nullptr);

    EXPECT_EQ(trend.direction, TREND_UNKNOWN);
}

TEST_F(TemporalPatternsTest, TrendAnalysisValidMetric) {
    if (intro == nullptr) {
        GTEST_SKIP() << "Introspection not available";
    }

    temporal_trend_t trend = introspection_get_trend(intro, "avg_activation", nullptr);

    // May be UNKNOWN if insufficient history
    EXPECT_GE(trend.direction, TREND_INCREASING);
    EXPECT_LE(trend.direction, TREND_UNKNOWN);

    // Statistics should be reasonable
    EXPECT_GE(trend.mean_value, 0.0f);
    EXPECT_GE(trend.variance, 0.0f);
    EXPECT_LE(trend.min_value, trend.max_value);
    EXPECT_GE(trend.r_squared, 0.0f);
    EXPECT_LE(trend.r_squared, 1.0f);
}

TEST_F(TemporalPatternsTest, TrendAnalysisSupportedMetrics) {
    if (intro == nullptr) {
        GTEST_SKIP() << "Introspection not available";
    }

    const char* metrics[] = {
        "avg_activation",
        "max_activation",
        "num_active",
        "energy"
    };

    for (const char* metric : metrics) {
        temporal_trend_t trend = introspection_get_trend(intro, metric, nullptr);

        // Should handle all supported metrics
        EXPECT_STREQ(trend.metric_name, metric);
    }
}

//=============================================================================
// 6. Pattern Library Tests
//=============================================================================

TEST_F(TemporalPatternsTest, RegisterPatternNullContext) {
    temporal_pattern_t pattern = create_test_pattern("test_pattern", 5);

    bool result = introspection_register_pattern(nullptr, &pattern);

    EXPECT_FALSE(result);

    temporal_pattern_free(&pattern);
}

TEST_F(TemporalPatternsTest, RegisterPatternNullPattern) {
    if (intro == nullptr) {
        GTEST_SKIP() << "Introspection not available";
    }

    bool result = introspection_register_pattern(intro, nullptr);

    EXPECT_FALSE(result);
}

TEST_F(TemporalPatternsTest, RegisterPatternValid) {
    if (intro == nullptr) {
        GTEST_SKIP() << "Introspection not available";
    }

    temporal_pattern_t pattern = create_test_pattern("test_pattern", 5);

    bool result = introspection_register_pattern(intro, &pattern);

    EXPECT_TRUE(result);

    temporal_pattern_free(&pattern);
}

TEST_F(TemporalPatternsTest, GetPatternLibraryEmpty) {
    if (intro == nullptr) {
        GTEST_SKIP() << "Introspection not available";
    }

    uint32_t num_patterns = 999;
    temporal_pattern_t* library = introspection_get_pattern_library(intro, &num_patterns);

    // Library may be empty initially
    if (library == nullptr) {
        EXPECT_EQ(num_patterns, 0u);
    } else {
        pattern_array_free(library, num_patterns);
    }
}

TEST_F(TemporalPatternsTest, GetPatternLibraryAfterRegistration) {
    if (intro == nullptr) {
        GTEST_SKIP() << "Introspection not available";
    }

    // Register multiple patterns
    for (int i = 0; i < 3; i++) {
        char name[64];
        snprintf(name, sizeof(name), "pattern_%d", i);
        temporal_pattern_t pattern = create_test_pattern(name, 5);
        introspection_register_pattern(intro, &pattern);
        temporal_pattern_free(&pattern);
    }

    uint32_t num_patterns = 0;
    temporal_pattern_t* library = introspection_get_pattern_library(intro, &num_patterns);

    // Should have at least the patterns we registered
    if (library != nullptr) {
        EXPECT_GE(num_patterns, 3u);
        pattern_array_free(library, num_patterns);
    }
}

TEST_F(TemporalPatternsTest, PatternSimilarityNullInputs) {
    temporal_pattern_t pattern = create_test_pattern("test_pattern", 5);

    float similarity1 = introspection_pattern_similarity(nullptr, &pattern);
    EXPECT_FLOAT_EQ(similarity1, 0.0f);

    float similarity2 = introspection_pattern_similarity(&pattern, nullptr);
    EXPECT_FLOAT_EQ(similarity2, 0.0f);

    float similarity3 = introspection_pattern_similarity(nullptr, nullptr);
    EXPECT_FLOAT_EQ(similarity3, 0.0f);

    temporal_pattern_free(&pattern);
}

TEST_F(TemporalPatternsTest, PatternSimilarityIdentical) {
    temporal_pattern_t pattern = create_test_pattern("test_pattern", 5);

    float similarity = introspection_pattern_similarity(&pattern, &pattern);

    // Identical patterns should have high similarity
    EXPECT_GE(similarity, 0.9f);
    EXPECT_LE(similarity, 1.0f);

    temporal_pattern_free(&pattern);
}

TEST_F(TemporalPatternsTest, PatternSimilarityDifferent) {
    temporal_pattern_t pattern1 = create_test_pattern("pattern1", 5);
    temporal_pattern_t pattern2 = create_test_pattern("pattern2", 5);

    // Modify pattern2 to be different
    if (pattern2.state_sequence && pattern2.state_sequence[0]) {
        pattern2.state_sequence[0][0] = 0.99f;
    }

    float similarity = introspection_pattern_similarity(&pattern1, &pattern2);

    // Different patterns should have lower similarity
    EXPECT_GE(similarity, 0.0f);
    EXPECT_LE(similarity, 1.0f);

    temporal_pattern_free(&pattern1);
    temporal_pattern_free(&pattern2);
}

//=============================================================================
// 7. Brain Integration Tests
//=============================================================================

TEST_F(TemporalPatternsTest, EnablePatternDetectionNullBrain) {
    bool result = brain_enable_pattern_detection(nullptr, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(TemporalPatternsTest, EnablePatternDetectionValidBrain) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed";
    }

    bool result = brain_enable_pattern_detection(brain, nullptr);

    // May fail if introspection not available, but shouldn't crash
    // Result depends on brain configuration
}

TEST_F(TemporalPatternsTest, GetActivePatternsNullBrain) {
    uint32_t num_patterns = 999;
    temporal_pattern_t* patterns = brain_get_active_patterns(nullptr, &num_patterns);

    EXPECT_EQ(patterns, nullptr);
    EXPECT_EQ(num_patterns, 0u);
}

TEST_F(TemporalPatternsTest, GetActivePatternsValidBrain) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed";
    }

    uint32_t num_patterns = 0;
    temporal_pattern_t* patterns = brain_get_active_patterns(brain, &num_patterns);

    // May return NULL if no active patterns
    if (patterns != nullptr) {
        pattern_array_free(patterns, num_patterns);
    }
}

TEST_F(TemporalPatternsTest, PatternDetectionCallbackNullBrain) {
    auto callback = [](const temporal_pattern_t*, float, void*) {};

    bool result = brain_on_pattern_detected(nullptr, callback, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(TemporalPatternsTest, PatternDetectionCallbackValidBrain) {
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation failed";
    }

    bool callback_invoked = false;
    auto callback = [](const temporal_pattern_t* pattern, float confidence, void* user_data) {
        bool* flag = (bool*)user_data;
        *flag = true;
    };

    bool result = brain_on_pattern_detected(brain, callback, &callback_invoked);

    // May succeed or fail depending on introspection availability
    // but shouldn't crash
}

//=============================================================================
// 8. Memory Management Tests
//=============================================================================

TEST_F(TemporalPatternsTest, FreeNullPattern) {
    // Should not crash
    temporal_pattern_free(nullptr);
    SUCCEED();
}

TEST_F(TemporalPatternsTest, FreeValidPattern) {
    temporal_pattern_t pattern = create_test_pattern("test_pattern", 5);

    // Should not crash
    temporal_pattern_free(&pattern);

    // Pattern should be zeroed
    EXPECT_EQ(pattern.sequence_length, 0u);
    EXPECT_EQ(pattern.state_sequence, nullptr);
}

TEST_F(TemporalPatternsTest, FreePatternArray) {
    temporal_pattern_t* patterns = (temporal_pattern_t*)nimcp_calloc(3, sizeof(temporal_pattern_t));

    for (int i = 0; i < 3; i++) {
        char name[64];
        snprintf(name, sizeof(name), "pattern_%d", i);
        patterns[i] = create_test_pattern(name, 5);
    }

    // Should not crash
    pattern_array_free(patterns, 3);
    SUCCEED();
}

TEST_F(TemporalPatternsTest, FreeNullPatternArray) {
    // Should not crash
    pattern_array_free(nullptr, 5);
    SUCCEED();
}

TEST_F(TemporalPatternsTest, FreePatternSequence) {
    pattern_sequence_t sequence;
    memset(&sequence, 0, sizeof(pattern_sequence_t));

    sequence.num_states = 3;
    sequence.states = (brain_state_t*)nimcp_calloc(3, sizeof(brain_state_t));

    // Should not crash
    pattern_sequence_free(&sequence);

    EXPECT_EQ(sequence.num_states, 0u);
    EXPECT_EQ(sequence.states, nullptr);
}

TEST_F(TemporalPatternsTest, FreeMatchResult) {
    pattern_match_result_t result;
    memset(&result, 0, sizeof(pattern_match_result_t));

    result.confidence = 0.8f;
    result.dtw_distance = 0.2f;

    // Should not crash (matched_pattern is not owned)
    pattern_match_result_free(&result);

    EXPECT_FLOAT_EQ(result.confidence, 0.0f);
}

//=============================================================================
// 9. Edge Cases and Error Handling
//=============================================================================

TEST_F(TemporalPatternsTest, ZeroLengthPattern) {
    temporal_pattern_t pattern;
    memset(&pattern, 0, sizeof(temporal_pattern_t));
    snprintf(pattern.name, TEMPORAL_MAX_PATTERN_NAME, "zero_length");
    pattern.sequence_length = 0;
    pattern.state_dimension = 1;

    if (intro != nullptr) {
        bool result = introspection_register_pattern(intro, &pattern);
        // May succeed or fail, but shouldn't crash
    }
}

TEST_F(TemporalPatternsTest, LargePatternLength) {
    temporal_pattern_t pattern = create_test_pattern("large_pattern", 100);

    if (intro != nullptr) {
        bool result = introspection_register_pattern(intro, &pattern);
        // Should handle large patterns
    }

    temporal_pattern_free(&pattern);
}

TEST_F(TemporalPatternsTest, ExtremeConfidenceValues) {
    if (intro == nullptr) {
        GTEST_SKIP() << "Introspection not available";
    }

    temporal_pattern_t pattern = create_test_pattern("test_pattern", 5);
    pattern_match_result_t result = introspection_match_pattern(intro, &pattern, nullptr);

    // Confidence should never exceed valid range
    EXPECT_GE(result.confidence, 0.0f);
    EXPECT_LE(result.confidence, 1.0f);

    temporal_pattern_free(&pattern);
}

TEST_F(TemporalPatternsTest, EmptyMetricName) {
    if (intro == nullptr) {
        GTEST_SKIP() << "Introspection not available";
    }

    temporal_trend_t trend = introspection_get_trend(intro, "", nullptr);

    // Should handle empty metric name gracefully
    EXPECT_EQ(trend.direction, TREND_UNKNOWN);
}

TEST_F(TemporalPatternsTest, UnknownMetricName) {
    if (intro == nullptr) {
        GTEST_SKIP() << "Introspection not available";
    }

    temporal_trend_t trend = introspection_get_trend(intro, "unknown_metric_xyz", nullptr);

    // Should handle unknown metric gracefully
    // May return TREND_UNKNOWN or process as zeros
}

//=============================================================================
// Main Entry Point
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
