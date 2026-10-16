/**
 * @file test_metacognition_regression.cpp
 * @brief Regression tests for Metacognition Self-Monitoring Module
 * @version 1.0.0
 * @date 2025-01-20
 *
 * WHAT: Regression tests to prevent bugs from reappearing
 * WHY: Ensure fixes remain fixed and new features don't break existing behavior
 * HOW: Test historical bugs, edge cases, and performance regressions
 *
 * Regression Test Categories:
 * - Historical bug fixes
 * - Edge case handling
 * - Performance regressions
 * - Memory leak prevention
 * - Thread safety issues
 * - Numerical stability
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <cmath>

// Headers have their own extern "C" guards
#include "cognitive/fault_tolerance/nimcp_metacognition.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

//=============================================================================
// Test Fixture
//=============================================================================

class MetacognitionRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
    }

    void TearDown() override {
        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        EXPECT_EQ(stats.current_allocated, 0) << "Memory leak detected!";
    }
};

//=============================================================================
// Bug Fix Regression Tests
//=============================================================================

/**
 * @test Regression: Division by Zero in Baseline Calculation
 *
 * BUG: Division by zero when baseline window is empty
 * FIX: Check for zero samples before division
 * WHY: Crashed on first monitoring call
 * WHEN: 2025-01-20
 */
TEST_F(MetacognitionRegressionTest, BugFix_DivisionByZeroInBaseline) {
    // ARRANGE
    metacognition_t* meta = metacognition_create(NULL);
    ASSERT_NE(meta, nullptr);

    cognitive_state_t state = {0};
    state.reasoning_speed = 1.0f;
    state.memory_recall_accuracy = 0.95f;
    state.decision_quality = 0.90f;
    state.learning_rate_actual = 0.8f;
    state.attention_focus = 0.85f;

    // ACT: Monitor with empty baseline (first call)
    bool result = metacognition_monitor_self(meta, &state);

    // ASSERT: No crash, graceful handling
    EXPECT_TRUE(result);
    EXPECT_TRUE(metacognition_is_initialized(meta));

    // CLEANUP
    metacognition_destroy(meta);
}

/**
 * @test Regression: Confidence Overflow
 *
 * BUG: Confidence could exceed 1.0 after many successes
 * FIX: Clamp confidence to [0, 1] range
 * WHY: Invalid confidence values caused logic errors
 * WHEN: 2025-01-20
 */
TEST_F(MetacognitionRegressionTest, BugFix_ConfidenceOverflow) {
    // ARRANGE
    metacognition_t* meta = metacognition_create(NULL);
    ASSERT_NE(meta, nullptr);

    // ACT: Many successful calibrations
    float confidence = 0.9f;
    for (int i = 0; i < 1000; i++) {
        confidence = metacognition_calibrate_confidence(meta, confidence, true);
    }

    // ASSERT: Confidence clamped to valid range
    EXPECT_LE(confidence, 1.0f);
    EXPECT_GE(confidence, 0.0f);

    // CLEANUP
    metacognition_destroy(meta);
}

/**
 * @test Regression: Negative Confidence
 *
 * BUG: Confidence could go negative after many failures
 * FIX: Clamp confidence to [0, 1] range
 * WHY: Negative confidence is nonsensical
 * WHEN: 2025-01-20
 */
TEST_F(MetacognitionRegressionTest, BugFix_NegativeConfidence) {
    // ARRANGE
    metacognition_t* meta = metacognition_create(NULL);
    ASSERT_NE(meta, nullptr);

    // ACT: Many failed calibrations
    float confidence = 0.1f;
    for (int i = 0; i < 1000; i++) {
        confidence = metacognition_calibrate_confidence(meta, confidence, false);
    }

    // ASSERT: Confidence clamped to valid range
    EXPECT_GE(confidence, 0.0f);
    EXPECT_LE(confidence, 1.0f);

    // CLEANUP
    metacognition_destroy(meta);
}

/**
 * @test Regression: Memory Leak in Diagnosis
 *
 * BUG: diagnosis_t not properly freed in some code paths
 * FIX: Ensure diagnosis_destroy() always called
 * WHY: Leaked memory on every self-diagnosis
 * WHEN: 2025-01-20
 */
TEST_F(MetacognitionRegressionTest, BugFix_DiagnosisMemoryLeak) {
    // ARRANGE
    metacognition_t* meta = metacognition_create(NULL);
    ASSERT_NE(meta, nullptr);

    cognitive_state_t state = {0};
    state.reasoning_speed = 0.5f;
    state.memory_recall_accuracy = 0.6f;
    state.decision_quality = 0.5f;
    state.learning_rate_actual = 0.5f;
    state.attention_focus = 0.5f;

    metacognition_monitor_self(meta, &state);

    nimcp_memory_stats_t stats_before;
    nimcp_memory_get_stats(&stats_before);

    // ACT: Multiple diagnoses with proper cleanup
    for (int i = 0; i < 100; i++) {
        diagnosis_t* diagnosis = metacognition_self_diagnose(meta);
        if (diagnosis) {
            diagnosis_destroy(diagnosis);
        }
    }

    nimcp_memory_stats_t stats_after;
    nimcp_memory_get_stats(&stats_after);

    // ASSERT: No memory leak (accounting for metacognition itself)
    EXPECT_EQ(stats_before.current_allocated, stats_after.current_allocated);

    // CLEANUP
    metacognition_destroy(meta);
}

/**
 * @test Regression: Race Condition in Multi-Threaded Monitoring
 *
 * BUG: Data race when updating baseline from multiple threads
 * FIX: Add proper synchronization
 * WHY: Caused crashes in multi-threaded applications
 * WHEN: 2025-01-20
 */
TEST_F(MetacognitionRegressionTest, BugFix_RaceConditionMultiThreaded) {
    // ARRANGE
    metacognition_t* meta = metacognition_create(NULL);
    ASSERT_NE(meta, nullptr);

    const int num_threads = 8;
    const int iterations = 500;
    std::vector<std::thread> threads;

    // ACT: Concurrent monitoring from multiple threads
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([meta, iterations]() {
            for (int i = 0; i < iterations; i++) {
                cognitive_state_t state = {0};
                state.reasoning_speed = 0.8f + (rand() % 100) / 500.0f;
                state.memory_recall_accuracy = 0.90f + (rand() % 100) / 1000.0f;
                state.decision_quality = 0.85f + (rand() % 100) / 1000.0f;
                state.learning_rate_actual = 0.75f + (rand() % 100) / 1000.0f;
                state.attention_focus = 0.80f + (rand() % 100) / 1000.0f;

                metacognition_monitor_self(meta, &state);
            }
        });
    }

    // Wait for completion
    for (auto& thread : threads) {
        thread.join();
    }

    // ASSERT: No crash, data still valid
    EXPECT_TRUE(metacognition_is_initialized(meta));
    float confidence = metacognition_get_self_confidence(meta);
    EXPECT_GE(confidence, 0.0f);
    EXPECT_LE(confidence, 1.0f);

    // CLEANUP
    metacognition_destroy(meta);
}

//=============================================================================
// Edge Case Regression Tests
//=============================================================================

/**
 * @test Regression: All Metrics Zero
 *
 * WHAT: Handle case where all cognitive metrics are zero
 * WHY: Should not crash or produce invalid output
 * HOW: Monitor with all-zero state, verify graceful handling
 */
TEST_F(MetacognitionRegressionTest, EdgeCase_AllMetricsZero) {
    // ARRANGE
    metacognition_t* meta = metacognition_create(NULL);
    ASSERT_NE(meta, nullptr);

    cognitive_state_t state = {0};  // All zeros

    // ACT: Monitor zero state
    bool result = metacognition_monitor_self(meta, &state);

    // ASSERT: Handled gracefully
    EXPECT_TRUE(result);
    EXPECT_TRUE(metacognition_is_initialized(meta));

    // CLEANUP
    metacognition_destroy(meta);
}

/**
 * @test Regression: All Metrics Maximum
 *
 * WHAT: Handle case where all cognitive metrics are at maximum
 * WHY: Edge case for perfect performance
 * HOW: Monitor with all-max state, verify no issues
 */
TEST_F(MetacognitionRegressionTest, EdgeCase_AllMetricsMaximum) {
    // ARRANGE
    metacognition_t* meta = metacognition_create(NULL);
    ASSERT_NE(meta, nullptr);

    cognitive_state_t state = {0};
    state.reasoning_speed = 10.0f;      // Very fast
    state.memory_recall_accuracy = 1.0f;
    state.decision_quality = 1.0f;
    state.learning_rate_actual = 1.0f;
    state.attention_focus = 1.0f;

    // ACT: Monitor maximum performance
    bool result = metacognition_monitor_self(meta, &state);

    // ASSERT: Handled gracefully
    EXPECT_TRUE(result);
    EXPECT_FALSE(metacognition_is_degraded(meta, 0.7f));

    // CLEANUP
    metacognition_destroy(meta);
}

/**
 * @test Regression: Extreme Variability
 *
 * WHAT: Handle extreme performance variability
 * WHY: Real systems can have wild fluctuations
 * HOW: Alternate between min and max, verify stability
 */
TEST_F(MetacognitionRegressionTest, EdgeCase_ExtremeVariability) {
    // ARRANGE
    metacognition_t* meta = metacognition_create(NULL);
    ASSERT_NE(meta, nullptr);

    // ACT: Extreme variability
    for (int i = 0; i < 100; i++) {
        cognitive_state_t state = {0};

        if (i % 2 == 0) {
            // Maximum performance
            state.reasoning_speed = 5.0f;
            state.memory_recall_accuracy = 1.0f;
            state.decision_quality = 1.0f;
            state.learning_rate_actual = 1.0f;
            state.attention_focus = 1.0f;
        } else {
            // Minimum performance
            state.reasoning_speed = 0.1f;
            state.memory_recall_accuracy = 0.3f;
            state.decision_quality = 0.3f;
            state.learning_rate_actual = 0.2f;
            state.attention_focus = 0.3f;
        }

        metacognition_monitor_self(meta, &state);
    }

    // ASSERT: High uncertainty but no crash
    EXPECT_TRUE(metacognition_is_initialized(meta));
    float uncertainty = metacognition_get_uncertainty(meta);
    EXPECT_GT(uncertainty, 0.5f);  // Should be very uncertain

    // CLEANUP
    metacognition_destroy(meta);
}

/**
 * @test Regression: Rapid Confidence Oscillation
 *
 * WHAT: Handle rapid success/failure oscillation
 * WHY: Could cause numerical instability
 * HOW: Alternate success/failure rapidly, verify stability
 */
TEST_F(MetacognitionRegressionTest, EdgeCase_RapidConfidenceOscillation) {
    // ARRANGE
    metacognition_t* meta = metacognition_create(NULL);
    ASSERT_NE(meta, nullptr);

    // ACT: Rapid oscillation
    float confidence = 0.5f;
    for (int i = 0; i < 10000; i++) {
        bool success = (i % 2 == 0);
        confidence = metacognition_calibrate_confidence(meta, confidence, success);

        // ASSERT: Always valid
        EXPECT_GE(confidence, 0.0f);
        EXPECT_LE(confidence, 1.0f);
        EXPECT_FALSE(std::isnan(confidence));
        EXPECT_FALSE(std::isinf(confidence));
    }

    // CLEANUP
    metacognition_destroy(meta);
}

//=============================================================================
// Performance Regression Tests
//=============================================================================

/**
 * @test Regression: Monitoring Performance
 *
 * WHAT: Ensure monitoring remains fast (<100μs)
 * WHY: Performance regression would impact real-time systems
 * HOW: Benchmark monitoring calls, verify latency
 */
TEST_F(MetacognitionRegressionTest, Performance_MonitoringLatency) {
    // ARRANGE
    metacognition_t* meta = metacognition_create(NULL);
    ASSERT_NE(meta, nullptr);

    cognitive_state_t state = {0};
    state.reasoning_speed = 1.0f;
    state.memory_recall_accuracy = 0.95f;
    state.decision_quality = 0.90f;
    state.learning_rate_actual = 0.8f;
    state.attention_focus = 0.85f;

    // Warmup
    for (int i = 0; i < 100; i++) {
        metacognition_monitor_self(meta, &state);
    }

    // ACT: Benchmark
    const int iterations = 10000;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; i++) {
        metacognition_monitor_self(meta, &state);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(
        end - start
    ).count();

    double avg_latency_us = duration_us / static_cast<double>(iterations);

    // ASSERT: Average latency < 100μs
    EXPECT_LT(avg_latency_us, 100.0) << "Monitoring latency regression detected!";

    // CLEANUP
    metacognition_destroy(meta);
}

/**
 * @test Regression: Diagnosis Performance
 *
 * WHAT: Ensure self-diagnosis remains fast (<1ms)
 * WHY: Used in critical paths
 * HOW: Benchmark diagnosis calls
 */
TEST_F(MetacognitionRegressionTest, Performance_DiagnosisLatency) {
    // ARRANGE
    metacognition_t* meta = metacognition_create(NULL);
    ASSERT_NE(meta, nullptr);

    cognitive_state_t state = {0};
    state.reasoning_speed = 0.5f;
    state.memory_recall_accuracy = 0.7f;
    state.decision_quality = 0.6f;
    state.learning_rate_actual = 0.5f;
    state.attention_focus = 0.6f;

    metacognition_monitor_self(meta, &state);

    // ACT: Benchmark diagnosis
    const int iterations = 1000;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; i++) {
        diagnosis_t* diagnosis = metacognition_self_diagnose(meta);
        diagnosis_destroy(diagnosis);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(
        end - start
    ).count();

    double avg_latency_us = duration_us / static_cast<double>(iterations);

    // ASSERT: Average latency < 1000μs (1ms)
    EXPECT_LT(avg_latency_us, 1000.0) << "Diagnosis latency regression detected!";

    // CLEANUP
    metacognition_destroy(meta);
}

/**
 * @test Regression: Memory Footprint
 *
 * WHAT: Ensure memory footprint hasn't increased
 * WHY: Memory regression impacts scalability
 * HOW: Measure allocation size
 */
TEST_F(MetacognitionRegressionTest, Performance_MemoryFootprint) {
    // ARRANGE
    nimcp_memory_stats_t stats_before;
    nimcp_memory_get_stats(&stats_before);

    // ACT: Create metacognition system
    metacognition_t* meta = metacognition_create(NULL);
    ASSERT_NE(meta, nullptr);

    nimcp_memory_stats_t stats_after;
    nimcp_memory_get_stats(&stats_after);

    size_t footprint = stats_after.current_allocated - stats_before.current_allocated;

    // ASSERT: Footprint reasonable (< 100KB)
    EXPECT_LT(footprint, 100 * 1024) << "Memory footprint regression detected!";

    // CLEANUP
    metacognition_destroy(meta);
}

//=============================================================================
// Numerical Stability Tests
//=============================================================================

/**
 * @test Regression: Floating Point Precision
 *
 * WHAT: Ensure calculations maintain precision
 * WHY: Accumulated errors could cause drift
 * HOW: Repeated calculations, verify stability
 */
TEST_F(MetacognitionRegressionTest, NumericalStability_FloatingPointPrecision) {
    // ARRANGE
    metacognition_t* meta = metacognition_create(NULL);
    ASSERT_NE(meta, nullptr);

    cognitive_state_t state = {0};
    state.reasoning_speed = 0.123456789f;
    state.memory_recall_accuracy = 0.987654321f;
    state.decision_quality = 0.876543219f;
    state.learning_rate_actual = 0.765432198f;
    state.attention_focus = 0.654321987f;

    // ACT: Many iterations with precise values
    for (int i = 0; i < 10000; i++) {
        metacognition_monitor_self(meta, &state);
    }

    // ASSERT: Values remain stable (no NaN, Inf, or extreme drift)
    float confidence = metacognition_get_self_confidence(meta);
    EXPECT_FALSE(std::isnan(confidence));
    EXPECT_FALSE(std::isinf(confidence));
    EXPECT_GE(confidence, 0.0f);
    EXPECT_LE(confidence, 1.0f);

    // CLEANUP
    metacognition_destroy(meta);
}

/**
 * @test Regression: Baseline Stability
 *
 * WHAT: Ensure baseline doesn't drift with constant input
 * WHY: Baseline drift would cause false degradation alerts
 * HOW: Feed constant input, verify baseline stability
 */
TEST_F(MetacognitionRegressionTest, NumericalStability_BaselineStability) {
    // ARRANGE
    metacognition_t* meta = metacognition_create(NULL);
    ASSERT_NE(meta, nullptr);

    cognitive_state_t state = {0};
    state.reasoning_speed = 1.0f;
    state.memory_recall_accuracy = 0.95f;
    state.decision_quality = 0.90f;
    state.learning_rate_actual = 0.8f;
    state.attention_focus = 0.85f;

    // ACT: Feed constant input
    for (int i = 0; i < 1000; i++) {
        metacognition_monitor_self(meta, &state);
    }

    performance_baseline_t baseline1;
    metacognition_get_baseline(meta, &baseline1);

    // Continue feeding same input
    for (int i = 0; i < 1000; i++) {
        metacognition_monitor_self(meta, &state);
    }

    performance_baseline_t baseline2;
    metacognition_get_baseline(meta, &baseline2);

    // ASSERT: Baseline stable (minimal drift)
    EXPECT_NEAR(baseline1.reasoning_speed, baseline2.reasoning_speed, 0.01f);
    EXPECT_NEAR(baseline1.memory_recall_accuracy, baseline2.memory_recall_accuracy, 0.01f);

    // CLEANUP
    metacognition_destroy(meta);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
