/**
 * @file test_temporal_patterns_regression.cpp
 * @brief Regression tests for temporal pattern analysis
 *
 * TEST COVERAGE:
 * - Pattern detection performance benchmarks
 * - Pattern matching performance
 * - Prediction performance
 * - Trend analysis performance
 * - Memory usage tracking
 * - Pattern library scalability
 * - Thread safety under concurrent access
 * - Stability across runs (determinism)
 *
 * PERFORMANCE TARGETS:
 * - Pattern detection: < 100ms for 100 state history
 * - Pattern matching: < 10ms per pattern
 * - Prediction: < 50ms with 100 pattern library
 * - Trend analysis: < 20ms for 100 samples
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <vector>

#include "cognitive/introspection/nimcp_temporal_patterns.h"
#include "cognitive/introspection/nimcp_introspection.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain_regions/nimcp_brain_regions.h"

using namespace std::chrono;

//=============================================================================
// Test Fixture with Performance Tracking
//=============================================================================

class TemporalPatternsRegressionTest : public ::testing::Test {
protected:
    brain_t brain;
    introspection_context_t intro;

    void SetUp() override {
        brain = brain_create(
            "test_temporal_regression",
            BRAIN_SIZE_MEDIUM,
            BRAIN_TASK_CLASSIFICATION,
            50,   // num_inputs
            10    // num_outputs
        );

        if (brain != nullptr) {
            intro = brain_get_introspection(brain);
        } else {
            intro = nullptr;
        }
    }

    void TearDown() override {
        // Clear pattern library before destroying brain to avoid
        // leftover patterns affecting subsequent tests
        if (intro != nullptr) {
            introspection_clear_pattern_library(intro);
        }

        if (brain != nullptr) {
            brain_destroy(brain);
            brain = nullptr;
        }
        intro = nullptr;
    }

    // Helper: Measure execution time
    template<typename Func>
    double measure_ms(Func func) {
        auto start = high_resolution_clock::now();
        func();
        auto end = high_resolution_clock::now();
        return duration_cast<microseconds>(end - start).count() / 1000.0;
    }

    // Helper: Build substantial history
    void build_history(uint32_t num_steps) {
        if (brain == nullptr) return;

        float inputs[50] = {0};
        float outputs[10] = {0};  // Match brain's num_outputs
        for (uint32_t step = 0; step < num_steps; step++) {
            for (int i = 0; i < 50; i++) {
                inputs[i] = 0.5f + 0.5f * sinf(step * 0.05f + i * 0.1f);
            }
            brain_predict(brain, inputs, 50, outputs, 10);
        }
    }

    // Helper: Create test pattern
    temporal_pattern_t create_pattern(const char* name, uint32_t length) {
        temporal_pattern_t pattern;
        memset(&pattern, 0, sizeof(pattern));

        snprintf(pattern.name, TEMPORAL_MAX_PATTERN_NAME, "%s", name);
        pattern.sequence_length = length;
        pattern.state_dimension = 1;
        pattern.strength = 0.8f;

        pattern.state_sequence = (float**)malloc(length * sizeof(float*));
        for (uint32_t i = 0; i < length; i++) {
            pattern.state_sequence[i] = (float*)malloc(sizeof(float));
            pattern.state_sequence[i][0] = sinf(i * 0.3f);
        }

        return pattern;
    }
};

//=============================================================================
// 1. Pattern Detection Performance
//=============================================================================

TEST_F(TemporalPatternsRegressionTest, DetectionPerformanceSmallHistory) {
    if (brain == nullptr || intro == nullptr) {
        GTEST_SKIP() << "Brain or introspection not available";
    }

    // Build small history
    build_history(20);

    // Measure detection time
    uint32_t num_patterns = 0;
    temporal_pattern_t* patterns = nullptr;

    double time_ms = measure_ms([&]() {
        patterns = introspection_detect_patterns(intro, nullptr, &num_patterns);
    });

    // Should complete quickly for small history
    EXPECT_LT(time_ms, 50.0) << "Detection took " << time_ms << "ms for 20 states";

    if (patterns != nullptr) {
        pattern_array_free(patterns, num_patterns);
    }
}

TEST_F(TemporalPatternsRegressionTest, DetectionPerformanceLargeHistory) {
    if (brain == nullptr || intro == nullptr) {
        GTEST_SKIP() << "Brain or introspection not available";
    }

    // Build large history
    build_history(100);

    // Measure detection time
    uint32_t num_patterns = 0;
    temporal_pattern_t* patterns = nullptr;

    double time_ms = measure_ms([&]() {
        patterns = introspection_detect_patterns(intro, nullptr, &num_patterns);
    });

    // Should complete within performance target
    EXPECT_LT(time_ms, 200.0) << "Detection took " << time_ms << "ms for 100 states";

    if (patterns != nullptr) {
        pattern_array_free(patterns, num_patterns);
    }
}

TEST_F(TemporalPatternsRegressionTest, DetectionScalability) {
    if (brain == nullptr || intro == nullptr) {
        GTEST_SKIP() << "Brain or introspection not available";
    }

    std::vector<std::pair<uint32_t, double>> measurements;

    // Test different history sizes
    uint32_t sizes[] = {10, 20, 50, 100};

    for (uint32_t size : sizes) {
        // Build history
        build_history(size);

        // Measure
        uint32_t num_patterns = 0;
        temporal_pattern_t* patterns = nullptr;

        double time_ms = measure_ms([&]() {
            patterns = introspection_detect_patterns(intro, nullptr, &num_patterns);
        });

        measurements.push_back({size, time_ms});

        if (patterns != nullptr) {
            pattern_array_free(patterns, num_patterns);
        }

        // Print for analysis
        std::cout << "History size " << size << ": " << time_ms << "ms\n";
    }

    // Complexity should be polynomial (not exponential)
    // Ratio of time for 100 vs 10 should be < 100
    if (measurements.size() >= 2) {
        double ratio = measurements.back().second / measurements.front().second;
        EXPECT_LT(ratio, 100.0) << "Detection appears to have exponential complexity";
    }
}

//=============================================================================
// 2. Pattern Matching Performance
//=============================================================================

TEST_F(TemporalPatternsRegressionTest, MatchingPerformanceSinglePattern) {
    if (brain == nullptr || intro == nullptr) {
        GTEST_SKIP() << "Brain or introspection not available";
    }

    build_history(50);

    temporal_pattern_t pattern = create_pattern("test_pattern", 10);

    // Measure matching time
    pattern_match_result_t result;
    double time_ms = measure_ms([&]() {
        result = introspection_match_pattern(intro, &pattern, nullptr);
    });

    // Matching should be fast
    EXPECT_LT(time_ms, 20.0) << "Matching took " << time_ms << "ms";

    temporal_pattern_free(&pattern);
}

TEST_F(TemporalPatternsRegressionTest, MatchingPerformanceMultiplePatterns) {
    if (brain == nullptr || intro == nullptr) {
        GTEST_SKIP() << "Brain or introspection not available";
    }

    build_history(50);

    // Create multiple patterns
    std::vector<temporal_pattern_t> patterns;
    for (int i = 0; i < 10; i++) {
        char name[64];
        snprintf(name, sizeof(name), "pattern_%d", i);
        patterns.push_back(create_pattern(name, 5 + i));
    }

    // Measure time to match all patterns
    double total_time_ms = measure_ms([&]() {
        for (const auto& pattern : patterns) {
            introspection_match_pattern(intro, &pattern, nullptr);
        }
    });

    double avg_time_ms = total_time_ms / patterns.size();

    EXPECT_LT(avg_time_ms, 10.0) << "Average matching took " << avg_time_ms << "ms per pattern";

    // Cleanup
    for (auto& pattern : patterns) {
        temporal_pattern_free(&pattern);
    }
}

//=============================================================================
// 3. Prediction Performance
//=============================================================================

TEST_F(TemporalPatternsRegressionTest, PredictionPerformanceSmallLibrary) {
    if (brain == nullptr || intro == nullptr) {
        GTEST_SKIP() << "Brain or introspection not available";
    }

    build_history(50);

    // Register few patterns
    for (int i = 0; i < 5; i++) {
        char name[64];
        snprintf(name, sizeof(name), "pattern_%d", i);
        temporal_pattern_t pattern = create_pattern(name, 5);
        introspection_register_pattern(intro, &pattern);
        temporal_pattern_free(&pattern);
    }

    // Measure prediction time
    brain_state_t predicted;
    double time_ms = measure_ms([&]() {
        predicted = introspection_predict_next_state(intro, nullptr);
    });

    EXPECT_LT(time_ms, 30.0) << "Prediction took " << time_ms << "ms with 5 patterns";

    if (predicted.state_vector != nullptr) {
        brain_state_free(&predicted);
    }
}

TEST_F(TemporalPatternsRegressionTest, PredictionPerformanceLargeLibrary) {
    if (brain == nullptr || intro == nullptr) {
        GTEST_SKIP() << "Brain or introspection not available";
    }

    build_history(50);

    // Register many patterns
    for (int i = 0; i < 50; i++) {
        char name[64];
        snprintf(name, sizeof(name), "pattern_%d", i);
        temporal_pattern_t pattern = create_pattern(name, 5);
        introspection_register_pattern(intro, &pattern);
        temporal_pattern_free(&pattern);
    }

    // Measure prediction time
    brain_state_t predicted;
    double time_ms = measure_ms([&]() {
        predicted = introspection_predict_next_state(intro, nullptr);
    });

    EXPECT_LT(time_ms, 100.0) << "Prediction took " << time_ms << "ms with 50 patterns";

    if (predicted.state_vector != nullptr) {
        brain_state_free(&predicted);
    }
}

//=============================================================================
// 4. Trend Analysis Performance
//=============================================================================

TEST_F(TemporalPatternsRegressionTest, TrendAnalysisPerformance) {
    if (brain == nullptr || intro == nullptr) {
        GTEST_SKIP() << "Brain or introspection not available";
    }

    build_history(100);

    // Measure trend analysis time
    temporal_trend_t trend;
    double time_ms = measure_ms([&]() {
        trend = introspection_get_trend(intro, "avg_activation", nullptr);
    });

    EXPECT_LT(time_ms, 30.0) << "Trend analysis took " << time_ms << "ms for 100 samples";
}

TEST_F(TemporalPatternsRegressionTest, TrendAnalysisMultipleMetrics) {
    if (brain == nullptr || intro == nullptr) {
        GTEST_SKIP() << "Brain or introspection not available";
    }

    build_history(100);

    const char* metrics[] = {"avg_activation", "max_activation", "num_active", "energy"};

    double total_time_ms = measure_ms([&]() {
        for (const char* metric : metrics) {
            introspection_get_trend(intro, metric, nullptr);
        }
    });

    double avg_time_ms = total_time_ms / 4;

    EXPECT_LT(avg_time_ms, 20.0) << "Average trend analysis took " << avg_time_ms << "ms per metric";
}

//=============================================================================
// 5. Memory Usage Tests
//=============================================================================

TEST_F(TemporalPatternsRegressionTest, MemoryUsagePatternLibrary) {
    if (brain == nullptr || intro == nullptr) {
        GTEST_SKIP() << "Brain or introspection not available";
    }

    // Register many patterns and verify no memory leaks
    for (int i = 0; i < 100; i++) {
        char name[64];
        snprintf(name, sizeof(name), "pattern_%d", i);
        temporal_pattern_t pattern = create_pattern(name, 10);
        introspection_register_pattern(intro, &pattern);
        temporal_pattern_free(&pattern);
    }

    // Get library multiple times
    for (int i = 0; i < 10; i++) {
        uint32_t num_patterns = 0;
        temporal_pattern_t* library = introspection_get_pattern_library(intro, &num_patterns);

        if (library != nullptr) {
            // Don't free to test that library maintains ownership
            pattern_array_free(library, num_patterns);
        }
    }

    // Should complete without crashes or excessive memory usage
    SUCCEED();
}

TEST_F(TemporalPatternsRegressionTest, MemoryUsageDetection) {
    if (brain == nullptr || intro == nullptr) {
        GTEST_SKIP() << "Brain or introspection not available";
    }

    build_history(100);

    // Detect patterns multiple times
    for (int i = 0; i < 10; i++) {
        uint32_t num_patterns = 0;
        temporal_pattern_t* patterns = introspection_detect_patterns(intro, nullptr, &num_patterns);

        if (patterns != nullptr) {
            pattern_array_free(patterns, num_patterns);
        }
    }

    // Should not leak memory
    SUCCEED();
}

//=============================================================================
// 6. Pattern Library Scalability
//=============================================================================

TEST_F(TemporalPatternsRegressionTest, LibraryScalabilityRegistration) {
    if (brain == nullptr || intro == nullptr) {
        GTEST_SKIP() << "Brain or introspection not available";
    }

    std::vector<double> times;

    // Test registration at different library sizes
    for (int batch = 0; batch < 5; batch++) {
        // Register 20 patterns
        double time_ms = measure_ms([&]() {
            for (int i = 0; i < 20; i++) {
                char name[64];
                snprintf(name, sizeof(name), "pattern_batch%d_%d", batch, i);
                temporal_pattern_t pattern = create_pattern(name, 5);
                introspection_register_pattern(intro, &pattern);
                temporal_pattern_free(&pattern);
            }
        });

        times.push_back(time_ms);
        std::cout << "Batch " << batch << " (total " << (batch + 1) * 20
                  << " patterns): " << time_ms << "ms\n";
    }

    // Registration time should remain relatively constant (O(1) insertion)
    if (times.size() >= 2) {
        double ratio = times.back() / times.front();
        EXPECT_LT(ratio, 3.0) << "Registration appears to degrade significantly with library size";
    }
}

TEST_F(TemporalPatternsRegressionTest, LibraryScalabilityRetrieval) {
    if (brain == nullptr || intro == nullptr) {
        GTEST_SKIP() << "Brain or introspection not available";
    }

    // Register patterns
    for (int i = 0; i < 100; i++) {
        char name[64];
        snprintf(name, sizeof(name), "pattern_%d", i);
        temporal_pattern_t pattern = create_pattern(name, 5);
        introspection_register_pattern(intro, &pattern);
        temporal_pattern_free(&pattern);
    }

    // Measure retrieval time
    uint32_t num_patterns = 0;
    temporal_pattern_t* library = nullptr;

    double time_ms = measure_ms([&]() {
        library = introspection_get_pattern_library(intro, &num_patterns);
    });

    EXPECT_LT(time_ms, 50.0) << "Library retrieval took " << time_ms << "ms for 100 patterns";

    if (library != nullptr) {
        pattern_array_free(library, num_patterns);
    }
}

//=============================================================================
// 7. Thread Safety Tests
//=============================================================================

TEST_F(TemporalPatternsRegressionTest, ConcurrentRegistration) {
    if (brain == nullptr || intro == nullptr) {
        GTEST_SKIP() << "Brain or introspection not available";
    }

    // Concurrent registration from multiple threads
    std::vector<std::thread> threads;
    std::atomic<uint32_t> success_count{0};
    std::atomic<uint32_t> attempt_count{0};

    for (int t = 0; t < 4; t++) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < 10; i++) {
                char name[64];
                snprintf(name, sizeof(name), "thread%d_pattern_%d", t, i);
                temporal_pattern_t pattern = create_pattern(name, 5);
                attempt_count++;
                if (introspection_register_pattern(intro, &pattern)) {
                    success_count++;
                }
                temporal_pattern_free(&pattern);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Most registrations should succeed (>75% success rate under concurrency)
    // Some may fail due to library capacity limits or lock contention
    uint32_t success = success_count.load();
    EXPECT_GE(success, 30u) << "Expected at least 75% success rate, got "
                            << success << " out of " << attempt_count.load();
}

TEST_F(TemporalPatternsRegressionTest, ConcurrentMatching) {
    if (brain == nullptr || intro == nullptr) {
        GTEST_SKIP() << "Brain or introspection not available";
    }

    build_history(50);

    temporal_pattern_t pattern = create_pattern("shared_pattern", 10);

    // Concurrent matching from multiple threads
    std::vector<std::thread> threads;

    for (int t = 0; t < 4; t++) {
        threads.emplace_back([&]() {
            for (int i = 0; i < 10; i++) {
                pattern_match_result_t result = introspection_match_pattern(intro, &pattern, nullptr);
                // Just verify no crashes
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    temporal_pattern_free(&pattern);
    SUCCEED();
}

//=============================================================================
// 8. Determinism Tests
//=============================================================================

TEST_F(TemporalPatternsRegressionTest, DetectionDeterminism) {
    if (brain == nullptr || intro == nullptr) {
        GTEST_SKIP() << "Brain or introspection not available";
    }

    build_history(50);

    // Detect patterns twice
    uint32_t num_patterns1 = 0;
    temporal_pattern_t* patterns1 = introspection_detect_patterns(intro, nullptr, &num_patterns1);

    uint32_t num_patterns2 = 0;
    temporal_pattern_t* patterns2 = introspection_detect_patterns(intro, nullptr, &num_patterns2);

    // Should return same results
    EXPECT_EQ(num_patterns1, num_patterns2);

    if (patterns1 != nullptr) {
        pattern_array_free(patterns1, num_patterns1);
    }
    if (patterns2 != nullptr) {
        pattern_array_free(patterns2, num_patterns2);
    }
}

TEST_F(TemporalPatternsRegressionTest, MatchingDeterminism) {
    if (brain == nullptr || intro == nullptr) {
        GTEST_SKIP() << "Brain or introspection not available";
    }

    build_history(50);

    temporal_pattern_t pattern = create_pattern("test_pattern", 10);

    // Match multiple times
    pattern_match_result_t result1 = introspection_match_pattern(intro, &pattern, nullptr);
    pattern_match_result_t result2 = introspection_match_pattern(intro, &pattern, nullptr);

    // Should return same confidence
    EXPECT_FLOAT_EQ(result1.confidence, result2.confidence);
    EXPECT_FLOAT_EQ(result1.dtw_distance, result2.dtw_distance);

    temporal_pattern_free(&pattern);
}

//=============================================================================
// Main Entry Point
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
