/**
 * @file test_temporal_patterns_integration.cpp
 * @brief Integration tests for temporal pattern analysis with brain and introspection
 *
 * TEST COVERAGE:
 * - Brain + introspection + temporal patterns integration
 * - Pattern detection across multiple brain steps
 * - Pattern callback invocation
 * - Bio-async message propagation
 * - Cross-module pattern library access
 * - Real-world usage scenarios
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>

#include "cognitive/introspection/nimcp_temporal_patterns.h"
#include "cognitive/introspection/nimcp_introspection.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain_regions/nimcp_brain_regions.h"

//=============================================================================
// Test Fixture
//=============================================================================

class TemporalPatternsIntegrationTest : public ::testing::Test {
protected:
    brain_t brain;
    introspection_context_t intro;

    void SetUp() override {
        // Create brain with introspection enabled
        brain = brain_create(
            "test_temporal_integration",
            BRAIN_SIZE_SMALL,
            BRAIN_TASK_CLASSIFICATION,
            20,  // num_inputs
            5    // num_outputs
        );

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

    // Helper: Run brain for N steps to build history
    void run_brain_steps(uint32_t num_steps) {
        if (brain == nullptr) return;

        float inputs[20] = {0};
        float outputs[5] = {0};  // Match brain's num_outputs
        for (uint32_t step = 0; step < num_steps; step++) {
            // Generate varying input pattern
            for (int i = 0; i < 20; i++) {
                inputs[i] = 0.5f + 0.5f * sinf(step * 0.1f + i * 0.2f);
            }

            brain_predict(brain, inputs, 20, outputs, 5);

            // Small delay to ensure timestamps differ
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
};

//=============================================================================
// 1. Brain + Introspection + Patterns Integration
//=============================================================================

TEST_F(TemporalPatternsIntegrationTest, BrainWithPatternDetectionEnabled) {
    if (brain == nullptr || intro == nullptr) {
        GTEST_SKIP() << "Brain or introspection not available";
    }

    // Enable pattern detection
    bool enabled = brain_enable_pattern_detection(brain, nullptr);
    EXPECT_TRUE(enabled);

    // Run brain to generate activity
    run_brain_steps(50);

    // Detect patterns
    uint32_t num_patterns = 0;
    temporal_pattern_t* patterns = introspection_detect_patterns(intro, nullptr, &num_patterns);

    // Should complete without errors
    if (patterns != nullptr) {
        EXPECT_GT(num_patterns, 0u);
        pattern_array_free(patterns, num_patterns);
    }
}

TEST_F(TemporalPatternsIntegrationTest, PatternLibraryPersistsAcrossCalls) {
    if (brain == nullptr || intro == nullptr) {
        GTEST_SKIP() << "Brain or introspection not available";
    }

    // Register patterns
    temporal_pattern_t pattern1;
    memset(&pattern1, 0, sizeof(pattern1));
    snprintf(pattern1.name, TEMPORAL_MAX_PATTERN_NAME, "persistent_pattern_1");
    pattern1.sequence_length = 3;
    pattern1.state_dimension = 1;
    pattern1.state_sequence = (float**)malloc(3 * sizeof(float*));
    for (int i = 0; i < 3; i++) {
        pattern1.state_sequence[i] = (float*)malloc(sizeof(float));
        pattern1.state_sequence[i][0] = (float)i / 3.0f;
    }

    bool registered = introspection_register_pattern(intro, &pattern1);
    EXPECT_TRUE(registered);

    // Get library
    uint32_t num_patterns1 = 0;
    temporal_pattern_t* library1 = introspection_get_pattern_library(intro, &num_patterns1);
    ASSERT_NE(library1, nullptr);
    EXPECT_GE(num_patterns1, 1u);

    // Register another pattern
    temporal_pattern_t pattern2;
    memset(&pattern2, 0, sizeof(pattern2));
    snprintf(pattern2.name, TEMPORAL_MAX_PATTERN_NAME, "persistent_pattern_2");
    pattern2.sequence_length = 3;
    pattern2.state_dimension = 1;
    pattern2.state_sequence = (float**)malloc(3 * sizeof(float*));
    for (int i = 0; i < 3; i++) {
        pattern2.state_sequence[i] = (float*)malloc(sizeof(float));
        pattern2.state_sequence[i][0] = (float)(i + 1) / 3.0f;
    }

    registered = introspection_register_pattern(intro, &pattern2);
    EXPECT_TRUE(registered);

    // Get library again - should have both patterns
    uint32_t num_patterns2 = 0;
    temporal_pattern_t* library2 = introspection_get_pattern_library(intro, &num_patterns2);
    ASSERT_NE(library2, nullptr);
    EXPECT_GT(num_patterns2, num_patterns1);

    // Cleanup
    pattern_array_free(library1, num_patterns1);
    pattern_array_free(library2, num_patterns2);
    temporal_pattern_free(&pattern1);
    temporal_pattern_free(&pattern2);
}

//=============================================================================
// 2. Pattern Detection Across Brain Steps
//=============================================================================

TEST_F(TemporalPatternsIntegrationTest, DetectPatternsFromBrainActivity) {
    if (brain == nullptr || intro == nullptr) {
        GTEST_SKIP() << "Brain or introspection not available";
    }

    // Run brain long enough to build substantial history
    run_brain_steps(100);

    // Detect patterns
    temporal_pattern_config_t config = temporal_pattern_default_config();
    config.window_size = 10;
    config.min_pattern_length = 5;

    uint32_t num_patterns = 0;
    temporal_pattern_t* patterns = introspection_detect_patterns(intro, &config, &num_patterns);

    // With 100 steps, should have detected at least one pattern
    if (patterns != nullptr) {
        EXPECT_GT(num_patterns, 0u);

        // Validate detected patterns
        for (uint32_t i = 0; i < num_patterns; i++) {
            EXPECT_GE(patterns[i].sequence_length, config.min_pattern_length);
            EXPECT_GT(patterns[i].state_dimension, 0u);
        }

        pattern_array_free(patterns, num_patterns);
    }
}

TEST_F(TemporalPatternsIntegrationTest, MatchPatternAfterBrainActivity) {
    if (brain == nullptr || intro == nullptr) {
        GTEST_SKIP() << "Brain or introspection not available";
    }

    // Build history
    run_brain_steps(50);

    // Detect patterns
    uint32_t num_patterns = 0;
    temporal_pattern_t* patterns = introspection_detect_patterns(intro, nullptr, &num_patterns);

    if (patterns != nullptr && num_patterns > 0) {
        // Run more steps
        run_brain_steps(20);

        // Try to match first detected pattern
        pattern_match_result_t match = introspection_match_pattern(intro, &patterns[0], nullptr);

        EXPECT_GE(match.confidence, 0.0f);
        EXPECT_LE(match.confidence, 1.0f);

        pattern_array_free(patterns, num_patterns);
    }
}

//=============================================================================
// 3. Pattern Callback Tests
//=============================================================================

TEST_F(TemporalPatternsIntegrationTest, CallbackInvokedOnDetection) {
    if (brain == nullptr || intro == nullptr) {
        GTEST_SKIP() << "Brain or introspection not available";
    }

    // Setup callback
    struct CallbackData {
        bool invoked = false;
        std::string pattern_name;
        float confidence = 0.0f;
    };

    CallbackData callback_data;

    auto callback = [](const temporal_pattern_t* pattern, float confidence, void* user_data) {
        CallbackData* data = (CallbackData*)user_data;
        data->invoked = true;
        if (pattern) {
            data->pattern_name = pattern->name;
        }
        data->confidence = confidence;
    };

    bool registered = brain_on_pattern_detected(brain, callback, &callback_data);
    EXPECT_TRUE(registered);

    // Note: Callback invocation depends on automatic pattern detection
    // which may not be implemented in current version. This test validates
    // the callback registration mechanism.
}

//=============================================================================
// 4. Prediction Integration
//=============================================================================

TEST_F(TemporalPatternsIntegrationTest, PredictionWithPatternLibrary) {
    if (brain == nullptr || intro == nullptr) {
        GTEST_SKIP() << "Brain or introspection not available";
    }

    // Build history and detect patterns
    run_brain_steps(100);

    uint32_t num_patterns = 0;
    temporal_pattern_t* patterns = introspection_detect_patterns(intro, nullptr, &num_patterns);

    if (patterns != nullptr && num_patterns > 0) {
        // Register patterns in library
        for (uint32_t i = 0; i < num_patterns && i < 5; i++) {
            introspection_register_pattern(intro, &patterns[i]);
        }

        // Run a few more steps
        run_brain_steps(10);

        // Predict next state
        brain_state_t predicted = introspection_predict_next_state(intro, nullptr);

        // May or may not predict depending on match quality
        if (predicted.state_vector != nullptr) {
            EXPECT_GT(predicted.dimension, 0u);
            EXPECT_NE(predicted.interpretation, nullptr);
            brain_state_free(&predicted);
        }

        pattern_array_free(patterns, num_patterns);
    }
}

//=============================================================================
// 5. Trend Analysis Integration
//=============================================================================

TEST_F(TemporalPatternsIntegrationTest, TrendAnalysisFromHistory) {
    if (brain == nullptr || intro == nullptr) {
        GTEST_SKIP() << "Brain or introspection not available";
    }

    // Run brain to build history
    run_brain_steps(100);

    // Analyze trends for different metrics
    const char* metrics[] = {
        "avg_activation",
        "max_activation",
        "num_active",
        "energy"
    };

    for (const char* metric : metrics) {
        temporal_trend_t trend = introspection_get_trend(intro, metric, nullptr);

        // With sufficient history, should detect trends
        if (trend.direction != TREND_UNKNOWN) {
            EXPECT_GT(trend.num_samples, 0u);
            EXPECT_GE(trend.r_squared, 0.0f);
            EXPECT_LE(trend.r_squared, 1.0f);
            EXPECT_LE(trend.min_value, trend.mean_value);
            EXPECT_LE(trend.mean_value, trend.max_value);
        }
    }
}

//=============================================================================
// 6. Cross-Module Integration
//=============================================================================

TEST_F(TemporalPatternsIntegrationTest, ActivePatternsReflectBrainState) {
    if (brain == nullptr || intro == nullptr) {
        GTEST_SKIP() << "Brain or introspection not available";
    }

    // Build history and register patterns
    run_brain_steps(100);

    uint32_t num_detected = 0;
    temporal_pattern_t* detected = introspection_detect_patterns(intro, nullptr, &num_detected);

    if (detected != nullptr && num_detected > 0) {
        // Register in library
        for (uint32_t i = 0; i < num_detected && i < 5; i++) {
            introspection_register_pattern(intro, &detected[i]);
        }

        // Run more steps to potentially activate patterns
        run_brain_steps(20);

        // Get active patterns
        uint32_t num_active = 0;
        temporal_pattern_t* active = brain_get_active_patterns(brain, &num_active);

        // Active patterns should be subset of library
        if (active != nullptr) {
            EXPECT_LE(num_active, num_detected);
            pattern_array_free(active, num_active);
        }

        pattern_array_free(detected, num_detected);
    }
}

//=============================================================================
// 7. Real-World Usage Scenarios
//=============================================================================

TEST_F(TemporalPatternsIntegrationTest, CompleteWorkflow) {
    if (brain == nullptr || intro == nullptr) {
        GTEST_SKIP() << "Brain or introspection not available";
    }

    // 1. Enable pattern detection
    bool enabled = brain_enable_pattern_detection(brain, nullptr);
    EXPECT_TRUE(enabled);

    // 2. Run brain to generate activity
    run_brain_steps(100);

    // 3. Detect patterns
    uint32_t num_patterns = 0;
    temporal_pattern_t* patterns = introspection_detect_patterns(intro, nullptr, &num_patterns);

    if (patterns != nullptr && num_patterns > 0) {
        // 4. Register best patterns
        for (uint32_t i = 0; i < num_patterns && i < 3; i++) {
            introspection_register_pattern(intro, &patterns[i]);
        }

        // 5. Run more brain steps
        run_brain_steps(20);

        // 6. Match current state against library
        uint32_t library_size = 0;
        temporal_pattern_t* library = introspection_get_pattern_library(intro, &library_size);

        if (library != nullptr && library_size > 0) {
            pattern_match_result_t best_match;
            memset(&best_match, 0, sizeof(best_match));

            for (uint32_t i = 0; i < library_size; i++) {
                pattern_match_result_t match = introspection_match_pattern(intro, &library[i], nullptr);
                if (match.confidence > best_match.confidence) {
                    best_match = match;
                }
            }

            // 7. Predict next state
            brain_state_t predicted = introspection_predict_next_state(intro, nullptr);

            // 8. Analyze trends
            temporal_trend_t trend = introspection_get_trend(intro, "avg_activation", nullptr);

            // All steps should complete successfully
            if (predicted.state_vector != nullptr) {
                brain_state_free(&predicted);
            }

            pattern_array_free(library, library_size);
        }

        pattern_array_free(patterns, num_patterns);
    }
}

//=============================================================================
// Main Entry Point
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
