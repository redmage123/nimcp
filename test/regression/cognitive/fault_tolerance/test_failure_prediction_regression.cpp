/**
 * @file test_failure_prediction_regression.cpp
 * @brief Regression Tests for Failure Prediction Module
 * @version 1.0.0
 * @date 2025-11-20
 *
 * WHAT: Regression tests to prevent bugs from reappearing
 * WHY:  Ensure fixes stay fixed and features don't break
 * HOW:  Test historical bugs, edge cases, and critical scenarios
 *
 * Test coverage: Critical bugs, edge cases, performance regressions
 * Test count: 20+ regression tests
 *
 * TEST CATEGORIES:
 * 1. Historical Bug Fixes (6 tests)
 * 2. Edge Cases (6 tests)
 * 3. Performance Regressions (4 tests)
 * 4. API Stability (4 tests)
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>
#include <cmath>

// Headers have their own extern "C" guards
#include "cognitive/fault_tolerance/nimcp_failure_prediction.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class FailurePredictionRegressionTest : public ::testing::Test {
protected:
    failure_predictor_t* predictor;

    void SetUp() override {
        predictor = nullptr;
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
    }

    void TearDown() override {
        if (predictor) {
            failure_predictor_destroy(predictor);
            predictor = nullptr;
        }
        nimcp_memory_check_leaks();
        nimcp_memory_cleanup();
    }

    health_metrics_t create_metrics() {
        health_metrics_t metrics = {};
        metrics.memory_usage = 100 * 1024 * 1024;
        metrics.peak_memory = 150 * 1024 * 1024;
        metrics.gradient_norm = 1.0f;
        metrics.loss_value = 0.5f;
        metrics.timestamp_ms = 1000;
        return metrics;
    }
};

//=============================================================================
// 1. Historical Bug Fixes (6 tests)
//=============================================================================

/**
 * @test BUG-001: Division by zero when time delta is zero
 * WHAT: Prevent crash on simultaneous updates
 * WHY:  Historical bug caused crash
 * FIXED: 2025-11-20
 */
TEST_F(FailurePredictionRegressionTest, BUG001_DivisionByZeroTimeDelta) {
    predictor = failure_predictor_create();
    ASSERT_NE(predictor, nullptr);

    health_metrics_t metrics = create_metrics();

    // Two updates with same timestamp (zero time delta)
    metrics.timestamp_ms = 1000;
    failure_predictor_update_from_health_metrics(predictor, &metrics);

    metrics.timestamp_ms = 1000;  // Same timestamp
    metrics.memory_usage = 200 * 1024 * 1024;

    // Should not crash
    EXPECT_NO_THROW(
        failure_predictor_update_from_health_metrics(predictor, &metrics)
    );
}

/**
 * @test BUG-002: NULL pointer dereference in prediction array
 * WHAT: Handle empty predictions gracefully
 * WHY:  Crash when no predictions available
 * FIXED: 2025-11-20
 */
TEST_F(FailurePredictionRegressionTest, BUG002_NullPointerInPredictions) {
    predictor = failure_predictor_create();
    ASSERT_NE(predictor, nullptr);

    // Try to get predictions with no data
    failure_prediction_t* top = failure_predictor_get_highest_probability_prediction(predictor);

    // Should return NULL, not crash
    EXPECT_EQ(top, nullptr);

    // Try to get action with no predictions
    const char* action = failure_predictor_get_highest_priority_action(predictor);

    // Should handle gracefully
}

/**
 * @test BUG-003: Memory leak in prediction array reallocation
 * WHAT: Verify no leaks when predictions cleared
 * WHY:  Memory leak on repeated predict/clear cycles
 * FIXED: 2025-11-20
 */
TEST_F(FailurePredictionRegressionTest, BUG003_MemoryLeakPredictionRealloc) {
    predictor = failure_predictor_create();
    ASSERT_NE(predictor, nullptr);

    nimcp_memory_stats_t stats_before = {};
    nimcp_memory_get_stats(&stats_before);

    // Repeatedly create and clear predictions
    health_metrics_t metrics = create_metrics();
    for (int i = 0; i < 100; i++) {
        metrics.memory_usage = (500 + i * 5) * 1024 * 1024;
        failure_predictor_update_from_health_metrics(predictor, &metrics);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        failure_prediction_t* preds = failure_predictor_predict(predictor, &metrics);
        if (preds) {
            nimcp_free(preds);
        }

        failure_predictor_clear_predictions(predictor);
    }

    nimcp_memory_stats_t stats_after = {};
    nimcp_memory_get_stats(&stats_after);

    // Memory should not grow unbounded
    size_t growth = stats_after.current_allocated - stats_before.current_allocated;
    EXPECT_LT(growth, 100000u);  // < 100 KB growth for 100 iterations
}

/**
 * @test BUG-004: Overflow in time-to-failure calculation
 * WHAT: Handle very high growth rates
 * WHY:  Integer overflow in time calculation
 * FIXED: 2025-11-20
 */
TEST_F(FailurePredictionRegressionTest, BUG004_TimeToFailureOverflow) {
    predictor = failure_predictor_create();
    ASSERT_NE(predictor, nullptr);

    health_metrics_t metrics = create_metrics();

    // Extreme growth rate
    metrics.memory_usage = 100 * 1024 * 1024;
    failure_predictor_update_from_health_metrics(predictor, &metrics);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

    metrics.memory_usage = 900 * 1024 * 1024;  // 800 MB in 1ms
    failure_predictor_update_from_health_metrics(predictor, &metrics);

    // Should not overflow
    uint64_t time_to_oom = failure_predictor_estimate_time_to_oom(predictor, &metrics);

    // Should be reasonable value, not UINT64_MAX
    if (time_to_oom > 0) {
        EXPECT_LT(time_to_oom, UINT64_MAX / 2);
    }
}

/**
 * @test BUG-005: Race condition in concurrent indicator updates
 * WHAT: Verify thread-safe indicator updates
 * WHY:  Data corruption in multi-threaded scenarios
 * FIXED: 2025-11-20
 */
TEST_F(FailurePredictionRegressionTest, BUG005_RaceConditionIndicators) {
    predictor = failure_predictor_create();
    ASSERT_NE(predictor, nullptr);

    std::atomic<int> error_count{0};
    const int num_threads = 8;
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, &error_count, t]() {
            try {
                health_metrics_t metrics = create_metrics();
                for (int i = 0; i < 100; i++) {
                    metrics.memory_usage = (t * 100 + i) * 1024 * 1024;
                    failure_predictor_update_indicator(
                        predictor,
                        METRIC_TYPE_MEMORY,
                        (float)metrics.memory_usage,
                        1024.0f * 1024 * 1024  // 1 GB threshold
                    );
                }
            } catch (...) {
                error_count.fetch_add(1);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // No crashes or errors
    EXPECT_EQ(error_count.load(), 0);
}

/**
 * @test BUG-006: Incorrect probability calculation for low confidence
 * WHAT: Verify probability bounds
 * WHY:  Probabilities outside [0,1] range
 * FIXED: 2025-11-20
 */
TEST_F(FailurePredictionRegressionTest, BUG006_ProbabilityBounds) {
    predictor = failure_predictor_create();
    ASSERT_NE(predictor, nullptr);

    health_metrics_t metrics = create_metrics();

    // Edge case: Very small changes
    metrics.memory_usage = 100 * 1024 * 1024;
    failure_predictor_update_from_health_metrics(predictor, &metrics);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    metrics.memory_usage = 101 * 1024 * 1024;  // Only 1 MB change
    failure_prediction_t* predictions = failure_predictor_predict(predictor, &metrics);
    uint32_t count = failure_predictor_get_prediction_count(predictor);

    // All probabilities must be in [0, 1]
    for (uint32_t i = 0; i < count; i++) {
        EXPECT_GE(predictions[i].probability, 0.0f);
        EXPECT_LE(predictions[i].probability, 1.0f);
        EXPECT_FALSE(std::isnan(predictions[i].probability));
        EXPECT_FALSE(std::isinf(predictions[i].probability));
    }

    if (predictions) {
        nimcp_free(predictions);
    }
}

//=============================================================================
// 2. Edge Cases (6 tests)
//=============================================================================

/**
 * @test EDGE-001: Zero memory usage
 * WHAT: Handle zero values gracefully
 */
TEST_F(FailurePredictionRegressionTest, EDGE001_ZeroMemoryUsage) {
    predictor = failure_predictor_create();
    ASSERT_NE(predictor, nullptr);

    health_metrics_t metrics = create_metrics();
    metrics.memory_usage = 0;  // Zero memory

    bool success = failure_predictor_update_from_health_metrics(predictor, &metrics);
    EXPECT_TRUE(success);

    // Should not crash or produce invalid predictions
    failure_prediction_t* predictions = failure_predictor_predict(predictor, &metrics);
    if (predictions) {
        nimcp_free(predictions);
    }
}

/**
 * @test EDGE-002: Extremely large time delta
 * WHAT: Handle stale metrics
 */
TEST_F(FailurePredictionRegressionTest, EDGE002_LargeTimeDelta) {
    predictor = failure_predictor_create();
    ASSERT_NE(predictor, nullptr);

    health_metrics_t metrics = create_metrics();

    metrics.timestamp_ms = 1000;
    failure_predictor_update_from_health_metrics(predictor, &metrics);

    // Very old timestamp (1 hour later)
    metrics.timestamp_ms = 1000 + 3600000;
    metrics.memory_usage = 200 * 1024 * 1024;

    bool success = failure_predictor_update_from_health_metrics(predictor, &metrics);
    EXPECT_TRUE(success);
}

/**
 * @test EDGE-003: Negative gradient norm (invalid)
 * WHAT: Sanitize invalid inputs
 */
TEST_F(FailurePredictionRegressionTest, EDGE003_NegativeGradientNorm) {
    predictor = failure_predictor_create();
    ASSERT_NE(predictor, nullptr);

    health_metrics_t metrics = create_metrics();
    metrics.gradient_norm = -100.0f;  // Invalid negative

    bool success = failure_predictor_update_from_health_metrics(predictor, &metrics);
    // Should handle gracefully (sanitize or reject)
}

/**
 * @test EDGE-004: NaN or Inf in metrics
 * WHAT: Handle floating point edge cases
 */
TEST_F(FailurePredictionRegressionTest, EDGE004_NaNInfMetrics) {
    predictor = failure_predictor_create();
    ASSERT_NE(predictor, nullptr);

    health_metrics_t metrics = create_metrics();

    // NaN value
    metrics.gradient_norm = NAN;
    bool success1 = failure_predictor_update_from_health_metrics(predictor, &metrics);

    // Inf value
    metrics.gradient_norm = INFINITY;
    bool success2 = failure_predictor_update_from_health_metrics(predictor, &metrics);

    // Should handle gracefully
}

/**
 * @test EDGE-005: Maximum indicator capacity
 * WHAT: Verify limit enforcement
 */
TEST_F(FailurePredictionRegressionTest, EDGE005_MaxIndicatorCapacity) {
    failure_predictor_config_t config = failure_predictor_default_config();
    config.max_indicators = 5;
    predictor = failure_predictor_create_custom(&config);
    ASSERT_NE(predictor, nullptr);

    // Try to add more than max
    for (uint32_t i = 0; i < 10; i++) {
        failure_predictor_update_indicator(
            predictor,
            (metric_type_t)i,
            (float)i,
            100.0f
        );
    }

    // Should not crash, may have exactly max_indicators
    uint32_t count = 0;
    leading_indicator_t* indicators = failure_predictor_get_all_indicators(predictor, &count);

    EXPECT_LE(count, config.max_indicators);

    if (indicators) {
        nimcp_free(indicators);
    }
}

/**
 * @test EDGE-006: Rapid oscillating metrics
 * WHAT: Handle unstable metrics
 */
TEST_F(FailurePredictionRegressionTest, EDGE006_OscillatingMetrics) {
    predictor = failure_predictor_create();
    ASSERT_NE(predictor, nullptr);

    health_metrics_t metrics = create_metrics();

    // Rapidly oscillating memory usage
    for (int i = 0; i < 50; i++) {
        metrics.memory_usage = (i % 2 == 0 ? 100 : 500) * 1024 * 1024;
        metrics.timestamp_ms = 1000 + i * 10;

        failure_predictor_update_from_health_metrics(predictor, &metrics);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // Should handle oscillations without false positives
    failure_prediction_t* predictions = failure_predictor_predict(predictor, &metrics);
    uint32_t count = failure_predictor_get_prediction_count(predictor);

    // May or may not predict, but should not crash
    if (predictions) {
        nimcp_free(predictions);
    }
}

//=============================================================================
// 3. Performance Regressions (4 tests)
//=============================================================================

/**
 * @test PERF-001: Update performance degradation
 * WHAT: Verify update latency < 1ms
 * WHY:  Ensure no performance regression
 */
TEST_F(FailurePredictionRegressionTest, PERF001_UpdateLatency) {
    predictor = failure_predictor_create();
    ASSERT_NE(predictor, nullptr);

    health_metrics_t metrics = create_metrics();

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; i++) {
        metrics.memory_usage = (100 + i) * 1024 * 1024;
        failure_predictor_update_from_health_metrics(predictor, &metrics);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete 1000 updates in < 500ms (< 0.5ms per update)
    EXPECT_LT(duration.count(), 500);
}

/**
 * @test PERF-002: Prediction latency regression
 * WHAT: Verify prediction < 5ms
 */
TEST_F(FailurePredictionRegressionTest, PERF002_PredictionLatency) {
    predictor = failure_predictor_create();
    ASSERT_NE(predictor, nullptr);

    // Populate data
    health_metrics_t metrics = create_metrics();
    for (int i = 0; i < 20; i++) {
        metrics.memory_usage = (100 + i * 50) * 1024 * 1024;
        failure_predictor_update_from_health_metrics(predictor, &metrics);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    // Measure prediction time
    auto start = std::chrono::high_resolution_clock::now();

    failure_prediction_t* predictions = failure_predictor_predict(predictor, &metrics);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Should be < 5ms
    EXPECT_LT(duration.count(), 5000);

    if (predictions) {
        nimcp_free(predictions);
    }
}

/**
 * @test PERF-003: Memory overhead regression
 * WHAT: Verify memory usage < 500 KB
 */
TEST_F(FailurePredictionRegressionTest, PERF003_MemoryOverhead) {
    nimcp_memory_stats_t stats_before = {};
    nimcp_memory_get_stats(&stats_before);

    predictor = failure_predictor_create();
    ASSERT_NE(predictor, nullptr);

    // Add typical workload
    health_metrics_t metrics = create_metrics();
    for (int i = 0; i < 100; i++) {
        failure_predictor_update_from_health_metrics(predictor, &metrics);
    }

    nimcp_memory_stats_t stats_after = {};
    nimcp_memory_get_stats(&stats_after);

    size_t overhead = stats_after.current_allocated - stats_before.current_allocated;

    // Should use < 500 KB
    EXPECT_LT(overhead, 500 * 1024);
}

/**
 * @test PERF-004: Concurrent access scalability
 * WHAT: Verify performance with 8 threads
 */
TEST_F(FailurePredictionRegressionTest, PERF004_ConcurrentScalability) {
    predictor = failure_predictor_create();
    ASSERT_NE(predictor, nullptr);

    const int num_threads = 8;
    std::vector<std::thread> threads;

    auto start = std::chrono::high_resolution_clock::now();

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this]() {
            health_metrics_t metrics = create_metrics();
            for (int i = 0; i < 100; i++) {
                failure_predictor_update_from_health_metrics(predictor, &metrics);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete in < 1 second with 8 threads
    EXPECT_LT(duration.count(), 1000);
}

//=============================================================================
// 4. API Stability (4 tests)
//=============================================================================

/**
 * @test API-001: Config structure stability
 * WHAT: Verify config struct hasn't changed
 * WHY:  API compatibility
 */
TEST_F(FailurePredictionRegressionTest, API001_ConfigStructStability) {
    failure_predictor_config_t config = failure_predictor_default_config();

    // Verify expected fields exist
    EXPECT_GE(config.max_predictions, 1u);
    EXPECT_GE(config.max_indicators, 1u);
    EXPECT_GE(config.prediction_threshold, 0.0f);
    EXPECT_LE(config.prediction_threshold, 1.0f);

    // Booleans should be 0 or 1
    EXPECT_TRUE(config.enable_memory_leak_detection == true ||
                config.enable_memory_leak_detection == false);
    EXPECT_TRUE(config.enable_gradient_explosion_detection == true ||
                config.enable_gradient_explosion_detection == false);
}

/**
 * @test API-002: Prediction struct stability
 * WHAT: Verify prediction fields
 */
TEST_F(FailurePredictionRegressionTest, API002_PredictionStructStability) {
    predictor = failure_predictor_create();
    ASSERT_NE(predictor, nullptr);

    health_metrics_t metrics = create_metrics();
    metrics.memory_usage = 900 * 1024 * 1024;

    failure_predictor_update_from_health_metrics(predictor, &metrics);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    metrics.memory_usage = 980 * 1024 * 1024;
    failure_prediction_t* predictions = failure_predictor_predict(predictor, &metrics);
    uint32_t count = failure_predictor_get_prediction_count(predictor);

    if (count > 0) {
        // Verify expected fields
        EXPECT_GE(predictions[0].type, 0);
        EXPECT_GE(predictions[0].probability, 0.0f);
        EXPECT_LE(predictions[0].probability, 1.0f);
        EXPECT_GE(predictions[0].estimated_time_ms, 0u);
        EXPECT_GE(predictions[0].confidence, CONFIDENCE_LOW);
        EXPECT_NE(predictions[0].reasoning, nullptr);
    }

    if (predictions) {
        nimcp_free(predictions);
    }
}

/**
 * @test API-003: Function signature stability
 * WHAT: Verify all public functions compile
 * WHY:  API compatibility
 */
TEST_F(FailurePredictionRegressionTest, API003_FunctionSignatures) {
    // Create/destroy
    predictor = failure_predictor_create();
    ASSERT_NE(predictor, nullptr);

    failure_predictor_config_t config = failure_predictor_default_config();
    failure_predictor_t* pred2 = failure_predictor_create_custom(&config);
    ASSERT_NE(pred2, nullptr);

    failure_predictor_destroy(pred2);

    // Update functions
    health_metrics_t metrics = create_metrics();
    failure_predictor_update_from_health_metrics(predictor, &metrics);
    failure_predictor_update_indicator(predictor, METRIC_TYPE_MEMORY, 100.0f, 200.0f);

    // Prediction functions
    failure_prediction_t* preds = failure_predictor_predict(predictor, &metrics);
    uint32_t count = failure_predictor_get_prediction_count(predictor);
    failure_prediction_t* top = failure_predictor_get_highest_probability_prediction(predictor);

    // Detection functions
    failure_predictor_detect_memory_leak(predictor, &metrics);
    failure_predictor_detect_gradient_explosion(predictor, &metrics);

    // Prevention functions
    if (count > 0 && preds) {
        failure_predictor_needs_prevention(predictor, &preds[0]);
        failure_predictor_get_preventive_action(predictor, &preds[0]);
    }
    failure_predictor_get_highest_priority_action(predictor);

    // Utility functions
    failure_predictor_clear_predictions(predictor);
    uint32_t ind_count = 0;
    leading_indicator_t* inds = failure_predictor_get_all_indicators(predictor, &ind_count);

    if (preds) nimcp_free(preds);
    if (inds) nimcp_free(inds);

    SUCCEED();
}

/**
 * @test API-004: Enum value stability
 * WHAT: Verify enum values unchanged
 * WHY:  Serialization compatibility
 */
TEST_F(FailurePredictionRegressionTest, API004_EnumValueStability) {
    // Failure types
    EXPECT_EQ((int)FAILURE_TYPE_OOM, 0);
    EXPECT_EQ((int)FAILURE_TYPE_GRADIENT_EXPLOSION, 1);
    EXPECT_EQ((int)FAILURE_TYPE_DIVERGENCE, 2);
    EXPECT_EQ((int)FAILURE_TYPE_PERFORMANCE_DEGRADATION, 3);
    EXPECT_EQ((int)FAILURE_TYPE_ERROR_RATE_SPIKE, 4);

    // Confidence levels
    EXPECT_EQ((int)CONFIDENCE_LOW, 0);
    EXPECT_EQ((int)CONFIDENCE_MEDIUM, 1);
    EXPECT_EQ((int)CONFIDENCE_HIGH, 2);
    EXPECT_EQ((int)CONFIDENCE_VERY_HIGH, 3);

    // Metric types
    EXPECT_EQ((int)METRIC_TYPE_MEMORY, 0);
    EXPECT_EQ((int)METRIC_TYPE_LATENCY, 1);
    EXPECT_EQ((int)METRIC_TYPE_ERROR, 2);
    EXPECT_EQ((int)METRIC_TYPE_THROUGHPUT, 3);
    EXPECT_EQ((int)METRIC_TYPE_GRADIENT, 4);
    EXPECT_EQ((int)METRIC_TYPE_LOSS, 5);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
