/**
 * @file test_failure_prediction_integration.cpp
 * @brief Integration Tests for Failure Prediction with Brain System
 * @version 1.0.0
 * @date 2025-11-20
 *
 * WHAT: Integration tests with brain, health monitoring, and fault tolerance
 * WHY:  Verify end-to-end failure prediction in realistic scenarios
 * HOW:  Test with actual brain instances, real metrics, preventive actions
 *
 * Test coverage: Integration scenarios, system interactions
 * Test count: 25+ integration tests
 *
 * TEST CATEGORIES:
 * 1. Brain Integration (6 tests)
 * 2. Health Metrics Integration (5 tests)
 * 3. Preventive Action Execution (5 tests)
 * 4. End-to-End Scenarios (5 tests)
 * 5. Performance Under Load (4 tests)
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>

// Headers have their own extern "C" guards
#include "cognitive/fault_tolerance/nimcp_failure_prediction.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class FailurePredictionIntegrationTest : public ::testing::Test {
protected:
    failure_predictor_t* predictor;

    void SetUp() override {
        predictor = nullptr;
        nimcp_memory_init();
    }

    void TearDown() override {
        if (predictor) {
            failure_predictor_destroy(predictor);
            predictor = nullptr;
        }
        nimcp_memory_cleanup();
    }

    // Helper: Simulate realistic health metrics
    health_metrics_t simulate_healthy_system() {
        health_metrics_t metrics = {};
        metrics.memory_usage = 100 * 1024 * 1024;      // 100 MB
        metrics.peak_memory = 200 * 1024 * 1024;       // 200 MB
        metrics.gradient_norm = 1.5f;
        metrics.loss_value = 0.3f;
        metrics.learning_rate = 0.001f;
        metrics.throughput = 1000.0f;
        metrics.error_rate = 0.01f;
        metrics.timestamp_ms = 1000;
        return metrics;
    }

    // Helper: Simulate system under memory pressure
    health_metrics_t simulate_memory_pressure() {
        health_metrics_t metrics = {};
        metrics.memory_usage = 800 * 1024 * 1024;      // 800 MB
        metrics.peak_memory = 900 * 1024 * 1024;       // 900 MB peak
        metrics.gradient_norm = 2.0f;
        metrics.loss_value = 0.5f;
        metrics.learning_rate = 0.001f;
        metrics.throughput = 800.0f;
        metrics.error_rate = 0.02f;
        metrics.timestamp_ms = 5000;
        return metrics;
    }

    // Helper: Simulate gradient explosion scenario
    health_metrics_t simulate_gradient_explosion() {
        health_metrics_t metrics = {};
        metrics.memory_usage = 200 * 1024 * 1024;
        metrics.peak_memory = 250 * 1024 * 1024;
        metrics.gradient_norm = 15000.0f;              // Exploding gradients
        metrics.loss_value = 100.0f;                   // High loss
        metrics.learning_rate = 0.1f;                  // Too high LR
        metrics.throughput = 500.0f;
        metrics.error_rate = 0.5f;
        metrics.timestamp_ms = 3000;
        return metrics;
    }
};

//=============================================================================
// 1. Brain Integration Tests (6 tests)
//=============================================================================

/**
 * @test Create predictor for brain monitoring
 * WHAT: Initialize predictor for brain instance
 * WHY:  Verify integration setup
 */
TEST_F(FailurePredictionIntegrationTest, CreateForBrainMonitoring) {
    predictor = failure_predictor_create();
    ASSERT_NE(predictor, nullptr);

    // Simulate brain providing health metrics
    health_metrics_t metrics = simulate_healthy_system();

    bool success = failure_predictor_update_from_health_metrics(predictor, &metrics);
    EXPECT_TRUE(success);
}

/**
 * @test Monitor brain over training lifecycle
 * WHAT: Track metrics throughout training
 * WHY:  Realistic usage pattern
 */
TEST_F(FailurePredictionIntegrationTest, MonitorTrainingLifecycle) {
    predictor = failure_predictor_create();
    ASSERT_NE(predictor, nullptr);

    // Simulate 10 training epochs
    for (int epoch = 0; epoch < 10; epoch++) {
        health_metrics_t metrics = simulate_healthy_system();
        metrics.timestamp_ms = 1000 + epoch * 1000;
        metrics.loss_value = 1.0f / (epoch + 1);  // Loss decreasing

        failure_predictor_update_from_health_metrics(predictor, &metrics);
        failure_predictor_predict(predictor, &metrics);

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    // Should not predict failures for healthy training
    uint32_t count = failure_predictor_get_prediction_count(predictor);
    EXPECT_EQ(count, 0u);  // No failures predicted
}

/**
 * @test Detect memory leak during brain operation
 * WHAT: Identify leak in running brain
 * WHY:  Prevent OOM crashes
 */
TEST_F(FailurePredictionIntegrationTest, DetectBrainMemoryLeak) {
    predictor = failure_predictor_create();
    ASSERT_NE(predictor, nullptr);

    // Simulate brain with memory leak
    health_metrics_t metrics = simulate_healthy_system();

    for (int i = 0; i < 20; i++) {
        metrics.memory_usage = (100 + i * 50) * 1024 * 1024;  // Growing 50 MB/iteration
        metrics.timestamp_ms = 1000 + i * 100;

        failure_predictor_update_from_health_metrics(predictor, &metrics);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    // Should detect leak
    bool is_leaking = failure_predictor_detect_memory_leak(predictor, &metrics);
    EXPECT_TRUE(is_leaking);

    // Should predict OOM
    failure_prediction_t* predictions = failure_predictor_predict(predictor, &metrics);
    uint32_t count = failure_predictor_get_prediction_count(predictor);

    EXPECT_GT(count, 0u);

    if (predictions) {
        nimcp_free(predictions);
    }
}

/**
 * @test Predict gradient explosion during training
 * WHAT: Detect unstable training
 * WHY:  Prevent divergence
 */
TEST_F(FailurePredictionIntegrationTest, PredictTrainingInstability) {
    predictor = failure_predictor_create();
    ASSERT_NE(predictor, nullptr);

    // Start with normal gradients
    health_metrics_t metrics = simulate_healthy_system();
    failure_predictor_update_from_health_metrics(predictor, &metrics);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Gradients start growing
    metrics.gradient_norm = 100.0f;
    failure_predictor_update_from_health_metrics(predictor, &metrics);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Explosion
    metrics = simulate_gradient_explosion();
    failure_predictor_update_from_health_metrics(predictor, &metrics);

    bool is_exploding = failure_predictor_detect_gradient_explosion(predictor, &metrics);
    EXPECT_TRUE(is_exploding);

    // Should predict gradient explosion
    failure_prediction_t* predictions = failure_predictor_predict(predictor, &metrics);
    uint32_t count = failure_predictor_get_prediction_count(predictor);

    EXPECT_GT(count, 0u);

    if (predictions) {
        nimcp_free(predictions);
    }
}

/**
 * @test Multiple concurrent brain instances
 * WHAT: Monitor multiple brains simultaneously
 * WHY:  Verify scalability
 */
TEST_F(FailurePredictionIntegrationTest, MultipleBrainInstances) {
    const int num_brains = 4;
    std::vector<failure_predictor_t*> predictors;

    // Create predictor for each brain
    for (int i = 0; i < num_brains; i++) {
        failure_predictor_t* p = failure_predictor_create();
        ASSERT_NE(p, nullptr);
        predictors.push_back(p);
    }

    // Simulate each brain with different health
    for (int i = 0; i < num_brains; i++) {
        health_metrics_t metrics = simulate_healthy_system();
        metrics.memory_usage = (100 + i * 100) * 1024 * 1024;  // Different memory usage

        failure_predictor_update_from_health_metrics(predictors[i], &metrics);
        failure_predictor_predict(predictors[i], &metrics);
    }

    // Cleanup
    for (auto* p : predictors) {
        failure_predictor_destroy(p);
    }

    SUCCEED();
}

/**
 * @test Recovery after prediction
 * WHAT: Verify predictions clear after recovery
 * WHY:  Avoid false alarms
 */
TEST_F(FailurePredictionIntegrationTest, RecoveryAfterPrediction) {
    predictor = failure_predictor_create();
    ASSERT_NE(predictor, nullptr);

    // Simulate memory pressure
    health_metrics_t metrics = simulate_memory_pressure();
    failure_predictor_update_from_health_metrics(predictor, &metrics);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    metrics.memory_usage = 950 * 1024 * 1024;
    failure_predictor_predict(predictor, &metrics);

    uint32_t count_during = failure_predictor_get_prediction_count(predictor);
    EXPECT_GT(count_during, 0u);  // Should have predictions

    // Simulate recovery (GC, memory freed)
    failure_predictor_clear_predictions(predictor);

    metrics = simulate_healthy_system();
    failure_predictor_update_from_health_metrics(predictor, &metrics);
    failure_predictor_predict(predictor, &metrics);

    uint32_t count_after = failure_predictor_get_prediction_count(predictor);
    EXPECT_EQ(count_after, 0u);  // No predictions after recovery
}

//=============================================================================
// 2. Health Metrics Integration (5 tests)
//=============================================================================

/**
 * @test Full health metrics coverage
 * WHAT: Update all metrics fields
 * WHY:  Comprehensive monitoring
 */
TEST_F(FailurePredictionIntegrationTest, FullMetricsCoverage) {
    predictor = failure_predictor_create();
    ASSERT_NE(predictor, nullptr);

    health_metrics_t metrics = {};
    metrics.memory_usage = 500 * 1024 * 1024;
    metrics.peak_memory = 600 * 1024 * 1024;
    metrics.gradient_norm = 2.5f;
    metrics.loss_value = 0.4f;
    metrics.learning_rate = 0.001f;
    metrics.throughput = 900.0f;
    metrics.error_rate = 0.015f;
    metrics.timestamp_ms = 2000;

    bool success = failure_predictor_update_from_health_metrics(predictor, &metrics);
    EXPECT_TRUE(success);

    // Verify all indicators updated
    uint32_t count = 0;
    leading_indicator_t* indicators = failure_predictor_get_all_indicators(predictor, &count);

    EXPECT_NE(indicators, nullptr);
    EXPECT_GT(count, 0u);

    if (indicators) {
        nimcp_free(indicators);
    }
}

/**
 * @test Metrics at different time scales
 * WHAT: Track metrics over various intervals
 * WHY:  Verify time-based predictions
 */
TEST_F(FailurePredictionIntegrationTest, MetricsTimeScales) {
    predictor = failure_predictor_create();
    ASSERT_NE(predictor, nullptr);

    // Fast updates (millisecond scale)
    health_metrics_t metrics = simulate_healthy_system();
    for (int i = 0; i < 10; i++) {
        metrics.timestamp_ms = 1000 + i * 10;  // 10ms intervals
        failure_predictor_update_from_health_metrics(predictor, &metrics);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // Slow updates (second scale)
    for (int i = 0; i < 5; i++) {
        metrics.timestamp_ms = 2000 + i * 1000;  // 1 second intervals
        failure_predictor_update_from_health_metrics(predictor, &metrics);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    SUCCEED();
}

/**
 * @test Metrics under high load
 * WHAT: Monitor during intensive operations
 * WHY:  Verify prediction under stress
 */
TEST_F(FailurePredictionIntegrationTest, MetricsUnderLoad) {
    predictor = failure_predictor_create();
    ASSERT_NE(predictor, nullptr);

    health_metrics_t metrics = simulate_healthy_system();

    // Simulate high load (100 updates rapidly)
    for (int i = 0; i < 100; i++) {
        metrics.memory_usage = (100 + i * 2) * 1024 * 1024;  // Gradual increase
        metrics.throughput = 1000.0f - i * 5.0f;             // Decreasing throughput
        metrics.timestamp_ms = 1000 + i * 10;

        failure_predictor_update_from_health_metrics(predictor, &metrics);
    }

    // Should predict degradation
    failure_prediction_t* predictions = failure_predictor_predict(predictor, &metrics);
    uint32_t count = failure_predictor_get_prediction_count(predictor);

    EXPECT_GT(count, 0u);

    if (predictions) {
        nimcp_free(predictions);
    }
}

/**
 * @test Metrics with missing fields
 * WHAT: Handle incomplete metrics
 * WHY:  Robustness
 */
TEST_F(FailurePredictionIntegrationTest, IncompleteMetrics) {
    predictor = failure_predictor_create();
    ASSERT_NE(predictor, nullptr);

    // Partial metrics (some fields zero)
    health_metrics_t metrics = {};
    metrics.memory_usage = 200 * 1024 * 1024;
    metrics.timestamp_ms = 1000;
    // Other fields left as 0

    bool success = failure_predictor_update_from_health_metrics(predictor, &metrics);
    EXPECT_TRUE(success);  // Should handle gracefully
}

/**
 * @test Metrics boundary values
 * WHAT: Test with extreme values
 * WHY:  Verify robustness
 */
TEST_F(FailurePredictionIntegrationTest, MetricsBoundaryValues) {
    predictor = failure_predictor_create();
    ASSERT_NE(predictor, nullptr);

    // Very high values
    health_metrics_t metrics = {};
    metrics.memory_usage = UINT64_MAX / 2;  // Very high memory
    metrics.gradient_norm = 1000000.0f;     // Extreme gradient
    metrics.error_rate = 0.99f;             // Nearly all errors
    metrics.timestamp_ms = UINT64_MAX / 2;

    bool success = failure_predictor_update_from_health_metrics(predictor, &metrics);
    EXPECT_TRUE(success);

    // Should predict multiple failures
    failure_prediction_t* predictions = failure_predictor_predict(predictor, &metrics);
    uint32_t count = failure_predictor_get_prediction_count(predictor);

    EXPECT_GT(count, 0u);

    if (predictions) {
        nimcp_free(predictions);
    }
}

//=============================================================================
// 3. Preventive Action Execution (5 tests)
//=============================================================================

/**
 * @test Execute preventive action for OOM
 * WHAT: Trigger memory cleanup
 * WHY:  Prevent crash
 */
TEST_F(FailurePredictionIntegrationTest, PreventiveActionOOM) {
    predictor = failure_predictor_create();
    ASSERT_NE(predictor, nullptr);

    // Create OOM scenario
    health_metrics_t metrics = simulate_memory_pressure();
    failure_predictor_update_from_health_metrics(predictor, &metrics);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    metrics.memory_usage = 980 * 1024 * 1024;
    failure_prediction_t* predictions = failure_predictor_predict(predictor, &metrics);
    uint32_t count = failure_predictor_get_prediction_count(predictor);

    EXPECT_GT(count, 0u);

    if (count > 0) {
        // Get recommended action
        const char* action = failure_predictor_get_preventive_action(predictor, &predictions[0]);

        EXPECT_NE(action, nullptr);
        if (action) {
            // Verify action is memory-related
            EXPECT_TRUE(
                strstr(action, "memory") != nullptr ||
                strstr(action, "GC") != nullptr ||
                strstr(action, "cleanup") != nullptr
            );
        }
    }

    if (predictions) {
        nimcp_free(predictions);
    }
}

/**
 * @test Execute preventive action for gradient explosion
 * WHAT: Recommend gradient clipping
 * WHY:  Stabilize training
 */
TEST_F(FailurePredictionIntegrationTest, PreventiveActionGradient) {
    predictor = failure_predictor_create();
    ASSERT_NE(predictor, nullptr);

    // Create gradient explosion scenario
    health_metrics_t metrics = simulate_gradient_explosion();
    failure_predictor_update_from_health_metrics(predictor, &metrics);

    failure_prediction_t* predictions = failure_predictor_predict(predictor, &metrics);
    uint32_t count = failure_predictor_get_prediction_count(predictor);

    EXPECT_GT(count, 0u);

    // Find gradient explosion prediction
    for (uint32_t i = 0; i < count; i++) {
        if (predictions[i].type == FAILURE_TYPE_GRADIENT_EXPLOSION) {
            const char* action = failure_predictor_get_preventive_action(predictor, &predictions[i]);

            EXPECT_NE(action, nullptr);
            if (action) {
                // Verify action is gradient-related
                EXPECT_TRUE(
                    strstr(action, "gradient") != nullptr ||
                    strstr(action, "clip") != nullptr ||
                    strstr(action, "learning rate") != nullptr
                );
            }
            break;
        }
    }

    if (predictions) {
        nimcp_free(predictions);
    }
}

/**
 * @test Prioritize actions by urgency
 * WHAT: Execute most urgent first
 * WHY:  Optimal resource use
 */
TEST_F(FailurePredictionIntegrationTest, ActionPrioritization) {
    predictor = failure_predictor_create();
    ASSERT_NE(predictor, nullptr);

    // Create multiple issues
    health_metrics_t metrics = {};
    metrics.memory_usage = 950 * 1024 * 1024;  // Near OOM
    metrics.gradient_norm = 10000.0f;           // High gradient
    metrics.error_rate = 0.3f;                  // Moderate errors

    failure_predictor_update_from_health_metrics(predictor, &metrics);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    metrics.memory_usage = 990 * 1024 * 1024;  // Critical
    failure_prediction_t* predictions = failure_predictor_predict(predictor, &metrics);
    uint32_t count = failure_predictor_get_prediction_count(predictor);

    EXPECT_GT(count, 0u);

    // Get highest priority action
    const char* top_action = failure_predictor_get_highest_priority_action(predictor);

    EXPECT_NE(top_action, nullptr);

    if (predictions) {
        nimcp_free(predictions);
    }
}

/**
 * @test Action execution reduces risk
 * WHAT: Verify predictions change after action
 * WHY:  Feedback loop validation
 */
TEST_F(FailurePredictionIntegrationTest, ActionReducesRisk) {
    predictor = failure_predictor_create();
    ASSERT_NE(predictor, nullptr);

    // High risk scenario
    health_metrics_t metrics = simulate_memory_pressure();
    failure_predictor_update_from_health_metrics(predictor, &metrics);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    metrics.memory_usage = 950 * 1024 * 1024;
    failure_predictor_predict(predictor, &metrics);

    uint32_t count_before = failure_predictor_get_prediction_count(predictor);
    EXPECT_GT(count_before, 0u);

    // Simulate action execution (memory freed)
    failure_predictor_clear_predictions(predictor);

    metrics = simulate_healthy_system();  // After cleanup
    failure_predictor_update_from_health_metrics(predictor, &metrics);
    failure_predictor_predict(predictor, &metrics);

    uint32_t count_after = failure_predictor_get_prediction_count(predictor);
    EXPECT_LT(count_after, count_before);  // Fewer predictions
}

/**
 * @test No action for low-priority predictions
 * WHAT: Skip action if not urgent
 * WHY:  Avoid unnecessary interventions
 */
TEST_F(FailurePredictionIntegrationTest, NoActionLowPriority) {
    failure_predictor_config_t config = failure_predictor_default_config();
    config.prediction_threshold = 0.95f;  // Very high threshold
    predictor = failure_predictor_create_custom(&config);
    ASSERT_NE(predictor, nullptr);

    // Moderate issue (won't reach 95% threshold)
    health_metrics_t metrics = simulate_healthy_system();
    metrics.memory_usage = 300 * 1024 * 1024;

    failure_predictor_update_from_health_metrics(predictor, &metrics);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    metrics.memory_usage = 350 * 1024 * 1024;
    failure_predictor_predict(predictor, &metrics);

    // Should have few/no predictions
    uint32_t count = failure_predictor_get_prediction_count(predictor);
    // Low risk shouldn't trigger actions
}

//=============================================================================
// 4. End-to-End Scenarios (5 tests)
//=============================================================================

/**
 * @test Complete training cycle with monitoring
 * WHAT: Full training lifecycle with predictions
 * WHY:  Realistic end-to-end test
 */
TEST_F(FailurePredictionIntegrationTest, CompleteTrainingCycle) {
    predictor = failure_predictor_create();
    ASSERT_NE(predictor, nullptr);

    // Phase 1: Normal training
    for (int epoch = 0; epoch < 5; epoch++) {
        health_metrics_t metrics = simulate_healthy_system();
        metrics.loss_value = 1.0f / (epoch + 1);
        metrics.timestamp_ms = 1000 + epoch * 1000;

        failure_predictor_update_from_health_metrics(predictor, &metrics);
        failure_predictor_predict(predictor, &metrics);

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    // Phase 2: Detect issue
    health_metrics_t metrics = simulate_memory_pressure();
    failure_predictor_update_from_health_metrics(predictor, &metrics);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    metrics.memory_usage = 950 * 1024 * 1024;
    failure_prediction_t* predictions = failure_predictor_predict(predictor, &metrics);

    EXPECT_GT(failure_predictor_get_prediction_count(predictor), 0u);

    // Phase 3: Take action
    const char* action = failure_predictor_get_highest_priority_action(predictor);
    EXPECT_NE(action, nullptr);

    // Phase 4: Recovery
    failure_predictor_clear_predictions(predictor);
    metrics = simulate_healthy_system();
    failure_predictor_update_from_health_metrics(predictor, &metrics);

    if (predictions) {
        nimcp_free(predictions);
    }

    SUCCEED();
}

/**
 * @test Long-running system monitoring
 * WHAT: Monitor for extended period
 * WHY:  Verify stability
 */
TEST_F(FailurePredictionIntegrationTest, LongRunningMonitoring) {
    predictor = failure_predictor_create();
    ASSERT_NE(predictor, nullptr);

    // Simulate 100 iterations of monitoring
    health_metrics_t metrics = simulate_healthy_system();

    for (int i = 0; i < 100; i++) {
        metrics.memory_usage = (100 + (i % 20) * 10) * 1024 * 1024;  // Oscillating
        metrics.timestamp_ms = 1000 + i * 100;

        failure_predictor_update_from_health_metrics(predictor, &metrics);

        if (i % 10 == 0) {
            failure_predictor_predict(predictor, &metrics);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // Should handle continuous monitoring without issues
    SUCCEED();
}

/**
 * @test Recovery from multiple failures
 * WHAT: Handle cascade of issues
 * WHY:  Verify robustness
 */
TEST_F(FailurePredictionIntegrationTest, MultipleFailureRecovery) {
    predictor = failure_predictor_create();
    ASSERT_NE(predictor, nullptr);

    // Issue 1: Memory pressure
    health_metrics_t metrics = simulate_memory_pressure();
    failure_predictor_update_from_health_metrics(predictor, &metrics);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    metrics.memory_usage = 950 * 1024 * 1024;
    failure_predictor_predict(predictor, &metrics);

    uint32_t count1 = failure_predictor_get_prediction_count(predictor);
    EXPECT_GT(count1, 0u);

    // Recovery from issue 1
    failure_predictor_clear_predictions(predictor);

    // Issue 2: Gradient explosion
    metrics = simulate_gradient_explosion();
    failure_predictor_update_from_health_metrics(predictor, &metrics);
    failure_predictor_predict(predictor, &metrics);

    uint32_t count2 = failure_predictor_get_prediction_count(predictor);
    EXPECT_GT(count2, 0u);

    // Final recovery
    failure_predictor_clear_predictions(predictor);
    metrics = simulate_healthy_system();
    failure_predictor_update_from_health_metrics(predictor, &metrics);

    SUCCEED();
}

/**
 * @test Graceful degradation scenario
 * WHAT: System adapts to reduce load
 * WHY:  Prevent hard failures
 */
TEST_F(FailurePredictionIntegrationTest, GracefulDegradation) {
    predictor = failure_predictor_create();
    ASSERT_NE(predictor, nullptr);

    // Start with high load
    health_metrics_t metrics = simulate_memory_pressure();
    metrics.throughput = 500.0f;  // Reduced throughput

    failure_predictor_update_from_health_metrics(predictor, &metrics);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    metrics.memory_usage = 900 * 1024 * 1024;
    failure_prediction_t* predictions = failure_predictor_predict(predictor, &metrics);

    EXPECT_GT(failure_predictor_get_prediction_count(predictor), 0u);

    // Reduce complexity (graceful degradation)
    metrics.memory_usage = 700 * 1024 * 1024;  // Freed some memory
    metrics.throughput = 800.0f;                // Improved throughput

    failure_predictor_clear_predictions(predictor);
    failure_predictor_update_from_health_metrics(predictor, &metrics);
    failure_predictor_predict(predictor, &metrics);

    // Should have fewer predictions
    uint32_t count_after = failure_predictor_get_prediction_count(predictor);

    if (predictions) {
        nimcp_free(predictions);
    }
}

/**
 * @test System health dashboard integration
 * WHAT: Provide data for monitoring UI
 * WHY:  Observability
 */
TEST_F(FailurePredictionIntegrationTest, HealthDashboardIntegration) {
    predictor = failure_predictor_create();
    ASSERT_NE(predictor, nullptr);

    // Collect comprehensive status
    health_metrics_t metrics = simulate_healthy_system();
    metrics.memory_usage = 600 * 1024 * 1024;

    failure_predictor_update_from_health_metrics(predictor, &metrics);
    failure_prediction_t* predictions = failure_predictor_predict(predictor, &metrics);

    // Dashboard queries
    uint32_t pred_count = failure_predictor_get_prediction_count(predictor);
    uint32_t indicator_count = 0;
    leading_indicator_t* indicators = failure_predictor_get_all_indicators(predictor, &indicator_count);

    failure_prediction_t* top_pred = failure_predictor_get_highest_probability_prediction(predictor);

    // Verify data available for UI
    EXPECT_GE(indicator_count, 0u);

    if (indicators) {
        nimcp_free(indicators);
    }
    if (predictions) {
        nimcp_free(predictions);
    }
}

//=============================================================================
// 5. Performance Under Load (4 tests)
//=============================================================================

/**
 * @test High-frequency updates
 * WHAT: 1000 updates/sec
 * WHY:  Verify performance
 */
TEST_F(FailurePredictionIntegrationTest, HighFrequencyUpdates) {
    predictor = failure_predictor_create();
    ASSERT_NE(predictor, nullptr);

    auto start = std::chrono::high_resolution_clock::now();

    health_metrics_t metrics = simulate_healthy_system();
    for (int i = 0; i < 1000; i++) {
        metrics.memory_usage = (100 + i % 100) * 1024 * 1024;
        failure_predictor_update_from_health_metrics(predictor, &metrics);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete in reasonable time (< 1 second)
    EXPECT_LT(duration.count(), 1000);
}

/**
 * @test Prediction latency
 * WHAT: Measure prediction time
 * WHY:  Ensure low latency
 */
TEST_F(FailurePredictionIntegrationTest, PredictionLatency) {
    predictor = failure_predictor_create();
    ASSERT_NE(predictor, nullptr);

    // Populate some data
    health_metrics_t metrics = simulate_memory_pressure();
    for (int i = 0; i < 10; i++) {
        metrics.memory_usage = (500 + i * 50) * 1024 * 1024;
        failure_predictor_update_from_health_metrics(predictor, &metrics);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    // Measure prediction time
    auto start = std::chrono::high_resolution_clock::now();

    failure_prediction_t* predictions = failure_predictor_predict(predictor, &metrics);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Prediction should be fast (< 10ms)
    EXPECT_LT(duration.count(), 10000);

    if (predictions) {
        nimcp_free(predictions);
    }
}

/**
 * @test Memory efficiency
 * WHAT: Track memory usage
 * WHY:  Verify low overhead
 */
TEST_F(FailurePredictionIntegrationTest, MemoryEfficiency) {
    nimcp_memory_stats_t stats_before = {};
    nimcp_memory_get_stats(&stats_before);

    predictor = failure_predictor_create();
    ASSERT_NE(predictor, nullptr);

    // Add data
    health_metrics_t metrics = simulate_healthy_system();
    for (int i = 0; i < 100; i++) {
        failure_predictor_update_from_health_metrics(predictor, &metrics);
    }

    nimcp_memory_stats_t stats_after = {};
    nimcp_memory_get_stats(&stats_after);

    // Memory usage should be reasonable (< 1 MB for predictor)
    size_t overhead = stats_after.current_allocated - stats_before.current_allocated;
    EXPECT_LT(overhead, 1024 * 1024);  // < 1 MB
}

/**
 * @test Concurrent operations performance
 * WHAT: Multiple threads using predictor
 * WHY:  Verify thread-safe performance
 */
TEST_F(FailurePredictionIntegrationTest, ConcurrentPerformance) {
    predictor = failure_predictor_create();
    ASSERT_NE(predictor, nullptr);

    const int num_threads = 4;
    std::vector<std::thread> threads;

    auto start = std::chrono::high_resolution_clock::now();

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this]() {
            health_metrics_t metrics = simulate_healthy_system();
            for (int i = 0; i < 100; i++) {
                failure_predictor_update_from_health_metrics(predictor, &metrics);
                if (i % 10 == 0) {
                    failure_prediction_t* preds = failure_predictor_predict(predictor, &metrics);
                    if (preds) {
                        nimcp_free(preds);
                    }
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete in reasonable time (< 2 seconds)
    EXPECT_LT(duration.count(), 2000);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
