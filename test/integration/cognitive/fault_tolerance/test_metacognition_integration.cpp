/**
 * @file test_metacognition_integration.cpp
 * @brief Integration tests for Metacognition with Brain Systems
 * @version 1.0.0
 * @date 2025-01-20
 *
 * WHAT: Integration tests verifying metacognition works with brain cognitive systems
 * WHY: Ensure metacognition integrates properly with working memory, executive functions, etc.
 * HOW: Test metacognition monitoring actual brain operations and providing feedback
 *
 * Integration Scenarios:
 * - Working memory monitoring
 * - Executive function monitoring
 * - Learning rate tracking
 * - Recovery system integration
 * - Multi-threaded monitoring
 * - Long-running cognitive tasks
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>

// Headers have their own extern "C" guards
#include "cognitive/fault_tolerance/nimcp_metacognition.h"
#include "utils/fault_tolerance/nimcp_fast_recovery.h"
#include "utils/fault_tolerance/nimcp_lockfree_metrics.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

//=============================================================================
// Test Fixture
//=============================================================================

class MetacognitionIntegrationTest : public ::testing::Test {
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

    // Helper: Simulate cognitive workload
    cognitive_state_t simulate_cognitive_task(float difficulty) {
        cognitive_state_t state = {0};

        // Reasoning speed inversely proportional to difficulty
        state.reasoning_speed = 1.0f / (1.0f + difficulty);

        // Memory accuracy decreases with difficulty
        state.memory_recall_accuracy = 0.95f - (difficulty * 0.1f);

        // Decision quality affected by difficulty
        state.decision_quality = 0.90f - (difficulty * 0.15f);

        // Learning rate
        state.learning_rate_actual = 0.8f - (difficulty * 0.1f);

        // Attention focus
        state.attention_focus = 0.85f - (difficulty * 0.1f);

        return state;
    }
};

//=============================================================================
// Working Memory Integration Tests
//=============================================================================

/**
 * @test Integration with Working Memory Monitoring
 *
 * WHAT: Verify metacognition monitors working memory load
 * WHY: Working memory overload affects cognitive performance
 * HOW: Simulate varying WM loads, verify detection
 */
TEST_F(MetacognitionIntegrationTest, WorkingMemoryMonitoring) {
    // ARRANGE
    metacognition_t* meta = metacognition_create(NULL);
    ASSERT_NE(meta, nullptr);

    // ACT: Simulate light working memory load
    for (int i = 0; i < 20; i++) {
        cognitive_state_t state = simulate_cognitive_task(0.2f);  // Easy
        metacognition_monitor_self(meta, &state);
    }

    // ASSERT: No degradation with light load
    EXPECT_FALSE(metacognition_is_degraded(meta, 0.7f));

    // ACT: Simulate heavy working memory load
    for (int i = 0; i < 20; i++) {
        cognitive_state_t state = simulate_cognitive_task(1.5f);  // Heavy
        metacognition_monitor_self(meta, &state);
    }

    // ASSERT: Degradation detected with heavy load
    EXPECT_TRUE(metacognition_is_degraded(meta, 0.7f));

    // CLEANUP
    metacognition_destroy(meta);
}

//=============================================================================
// Recovery System Integration Tests
//=============================================================================

/**
 * @test Integration with Fast Recovery System
 *
 * WHAT: Verify metacognition triggers recovery when degraded
 * WHY: Enable self-healing cognitive system
 * HOW: Monitor degradation, trigger recovery, verify restoration
 */
TEST_F(MetacognitionIntegrationTest, FastRecoveryIntegration) {
    // ARRANGE
    metacognition_t* meta = metacognition_create(NULL);
    ASSERT_NE(meta, nullptr);

    fast_recovery_config_t recovery_config = fast_recovery_default_config();
    fast_recovery_t* recovery = fast_recovery_create(&recovery_config);
    ASSERT_NE(recovery, nullptr);

    // Establish healthy baseline
    cognitive_state_t healthy_state = simulate_cognitive_task(0.1f);
    for (int i = 0; i < 50; i++) {
        metacognition_monitor_self(meta, &healthy_state);
    }

    // ACT: Induce degradation
    cognitive_state_t degraded_state = simulate_cognitive_task(2.0f);
    metacognition_monitor_self(meta, &degraded_state);

    // Trigger recovery if degraded
    if (metacognition_is_degraded(meta, 0.7f)) {
        diagnosis_t* diagnosis = metacognition_self_diagnose(meta);

        // Create recovery snapshot
        brain_snapshot_t snapshot = {0};
        snapshot.timestamp_us = 12345678;
        snapshot.health_score = 0.5f;

        bool recovered = fast_recovery_trigger(recovery, &snapshot);
        EXPECT_TRUE(recovered);

        diagnosis_destroy(diagnosis);
    }

    // ACT: Monitor post-recovery
    metacognition_monitor_self(meta, &healthy_state);

    // ASSERT: System recovering
    float confidence = metacognition_get_self_confidence(meta);
    EXPECT_GT(confidence, 0.3f);  // Confidence should be rebuilding

    // CLEANUP
    fast_recovery_destroy(recovery);
    metacognition_destroy(meta);
}

//=============================================================================
// Metrics Integration Tests
//=============================================================================

/**
 * @test Integration with Lock-Free Metrics
 *
 * WHAT: Verify metacognition records metrics to lock-free buffer
 * WHY: Enable high-performance monitoring
 * HOW: Record cognitive metrics, verify in metrics buffer
 */
TEST_F(MetacognitionIntegrationTest, LockFreeMetricsIntegration) {
    // ARRANGE
    metacognition_t* meta = metacognition_create(NULL);
    ASSERT_NE(meta, nullptr);

    lockfree_metrics_config_t metrics_config = lockfree_metrics_default_config();
    lockfree_metrics_t* metrics = lockfree_metrics_create(&metrics_config);
    ASSERT_NE(metrics, nullptr);

    // ACT: Monitor cognitive state and record metrics
    cognitive_state_t state = simulate_cognitive_task(0.5f);
    metacognition_monitor_self(meta, &state);

    // Record metacognition metrics
    metric_entry_t entry = {0};
    entry.timestamp_us = 12345678;
    entry.type = METRIC_TYPE_CUSTOM;
    entry.component_id = 1;  // Metacognition component
    entry.value = metacognition_get_self_confidence(meta);

    metric_result_t result = lockfree_metrics_record(metrics, &entry);
    EXPECT_EQ(result, METRIC_RESULT_SUCCESS);

    // ASSERT: Metrics recorded
    metrics_stats_t stats;
    lockfree_metrics_get_stats(metrics, &stats);
    EXPECT_GT(stats.total_recorded, 0);

    // CLEANUP
    lockfree_metrics_destroy(metrics);
    metacognition_destroy(meta);
}

//=============================================================================
// Multi-Threaded Integration Tests
//=============================================================================

/**
 * @test Multi-Threaded Monitoring
 *
 * WHAT: Verify metacognition handles concurrent monitoring
 * WHY: Brain processes are parallel
 * HOW: Multiple threads monitor simultaneously, verify thread-safety
 */
TEST_F(MetacognitionIntegrationTest, MultiThreadedMonitoring) {
    // ARRANGE
    metacognition_t* meta = metacognition_create(NULL);
    ASSERT_NE(meta, nullptr);

    const int num_threads = 4;
    const int iterations = 100;
    std::vector<std::thread> threads;

    // ACT: Multiple threads monitoring concurrently
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([meta, iterations, t]() {
            for (int i = 0; i < iterations; i++) {
                float difficulty = (t * 0.2f) + (i * 0.001f);
                cognitive_state_t state = {0};
                state.reasoning_speed = 1.0f / (1.0f + difficulty);
                state.memory_recall_accuracy = 0.95f - (difficulty * 0.1f);
                state.decision_quality = 0.90f;
                state.learning_rate_actual = 0.8f;
                state.attention_focus = 0.85f;

                metacognition_monitor_self(meta, &state);

                // Small delay to allow interleaving
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        });
    }

    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }

    // ASSERT: No crashes, system still functional
    EXPECT_TRUE(metacognition_is_initialized(meta));
    float confidence = metacognition_get_self_confidence(meta);
    EXPECT_GE(confidence, 0.0f);
    EXPECT_LE(confidence, 1.0f);

    // CLEANUP
    metacognition_destroy(meta);
}

//=============================================================================
// Long-Running Cognitive Task Tests
//=============================================================================

/**
 * @test Long-Running Task Monitoring
 *
 * WHAT: Verify metacognition tracks performance over extended operation
 * WHY: Detect gradual degradation or fatigue
 * HOW: Simulate long task with gradual degradation, verify detection
 */
TEST_F(MetacognitionIntegrationTest, LongRunningTaskMonitoring) {
    // ARRANGE
    metacognition_t* meta = metacognition_create(NULL);
    ASSERT_NE(meta, nullptr);

    // ACT: Simulate 1000 iterations of cognitive work with gradual degradation
    for (int i = 0; i < 1000; i++) {
        // Gradual degradation over time (simulating cognitive fatigue)
        float fatigue_factor = i / 1000.0f;  // 0.0 to 1.0
        float difficulty = 0.2f + (fatigue_factor * 0.8f);  // 0.2 to 1.0

        cognitive_state_t state = simulate_cognitive_task(difficulty);
        metacognition_monitor_self(meta, &state);
    }

    // ASSERT: Degradation detected
    EXPECT_TRUE(metacognition_is_degraded(meta, 0.7f));

    diagnosis_t* diagnosis = metacognition_self_diagnose(meta);
    ASSERT_NE(diagnosis, nullptr);
    EXPECT_GT(diagnosis->severity, 0.5f);  // Significant degradation

    // CLEANUP
    diagnosis_destroy(diagnosis);
    metacognition_destroy(meta);
}

//=============================================================================
// Baseline Adaptation Tests
//=============================================================================

/**
 * @test Baseline Adaptation to Improved Performance
 *
 * WHAT: Verify baseline adapts to sustained improvement
 * WHY: Avoid false positives after learning/optimization
 * HOW: Simulate learning, verify baseline updates
 */
TEST_F(MetacognitionIntegrationTest, BaselineAdaptationToImprovement) {
    // ARRANGE
    metacognition_t* meta = metacognition_create(NULL);
    ASSERT_NE(meta, nullptr);

    // Establish initial baseline (moderate performance)
    for (int i = 0; i < 100; i++) {
        cognitive_state_t state = simulate_cognitive_task(0.5f);
        metacognition_monitor_self(meta, &state);
    }

    performance_baseline_t initial_baseline;
    metacognition_get_baseline(meta, &initial_baseline);

    // ACT: Simulate learning/improvement over time
    for (int i = 0; i < 200; i++) {
        // Gradually improving performance
        float improvement = (i / 200.0f) * 0.3f;  // Up to 30% improvement
        float difficulty = 0.5f - improvement;
        cognitive_state_t state = simulate_cognitive_task(difficulty);
        metacognition_monitor_self(meta, &state);
    }

    // ASSERT: Baseline adapted to improvement
    performance_baseline_t new_baseline;
    metacognition_get_baseline(meta, &new_baseline);

    EXPECT_GT(new_baseline.reasoning_speed, initial_baseline.reasoning_speed);
    EXPECT_GT(new_baseline.decision_quality, initial_baseline.decision_quality);

    // CLEANUP
    metacognition_destroy(meta);
}

//=============================================================================
// Diagnosis Accuracy Tests
//=============================================================================

/**
 * @test Diagnosis Accuracy with Real Cognitive Patterns
 *
 * WHAT: Verify diagnosis matches actual degradation pattern
 * WHY: Ensure actionable diagnosis
 * HOW: Induce specific degradations, verify correct diagnosis
 */
TEST_F(MetacognitionIntegrationTest, DiagnosisAccuracyRealPatterns) {
    // Test 1: Memory-specific degradation
    {
        metacognition_t* meta = metacognition_create(NULL);
        ASSERT_NE(meta, nullptr);

        cognitive_state_t state = {0};
        state.reasoning_speed = 1.0f;
        state.memory_recall_accuracy = 0.3f;  // Very poor
        state.decision_quality = 0.90f;
        state.learning_rate_actual = 0.8f;
        state.attention_focus = 0.85f;

        metacognition_monitor_self(meta, &state);
        diagnosis_t* diagnosis = metacognition_self_diagnose(meta);

        ASSERT_NE(diagnosis, nullptr);
        EXPECT_TRUE(diagnosis->has_memory_issues);

        diagnosis_destroy(diagnosis);
        metacognition_destroy(meta);
    }

    // Test 2: Attention deficit
    {
        metacognition_t* meta = metacognition_create(NULL);
        ASSERT_NE(meta, nullptr);

        cognitive_state_t state = {0};
        state.reasoning_speed = 1.0f;
        state.memory_recall_accuracy = 0.95f;
        state.decision_quality = 0.90f;
        state.learning_rate_actual = 0.8f;
        state.attention_focus = 0.3f;  // Very poor

        metacognition_monitor_self(meta, &state);
        diagnosis_t* diagnosis = metacognition_self_diagnose(meta);

        ASSERT_NE(diagnosis, nullptr);
        EXPECT_TRUE(diagnosis->has_attention_issues);

        diagnosis_destroy(diagnosis);
        metacognition_destroy(meta);
    }
}

//=============================================================================
// Stress Tests
//=============================================================================

/**
 * @test High-Frequency Monitoring Stress Test
 *
 * WHAT: Verify system handles very frequent monitoring calls
 * WHY: Real-time cognitive monitoring needs high throughput
 * HOW: Monitor at high frequency, verify no degradation
 */
TEST_F(MetacognitionIntegrationTest, HighFrequencyMonitoringStress) {
    // ARRANGE
    metacognition_t* meta = metacognition_create(NULL);
    ASSERT_NE(meta, nullptr);

    const int num_iterations = 10000;
    auto start_time = std::chrono::high_resolution_clock::now();

    // ACT: High-frequency monitoring
    for (int i = 0; i < num_iterations; i++) {
        cognitive_state_t state = simulate_cognitive_task(0.5f);
        bool result = metacognition_monitor_self(meta, &state);
        EXPECT_TRUE(result);
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time
    ).count();

    // ASSERT: Performance acceptable (< 1ms per operation on average)
    double avg_time_ms = duration / static_cast<double>(num_iterations);
    EXPECT_LT(avg_time_ms, 1.0);  // < 1ms average

    // System still functional
    EXPECT_TRUE(metacognition_is_initialized(meta));

    // CLEANUP
    metacognition_destroy(meta);
}

//=============================================================================
// End-to-End Scenario Tests
//=============================================================================

/**
 * @test End-to-End: Detection, Diagnosis, Recovery
 *
 * WHAT: Full workflow from degradation detection to recovery
 * WHY: Verify complete self-monitoring loop
 * HOW: Induce degradation, detect, diagnose, trigger recovery, verify
 */
TEST_F(MetacognitionIntegrationTest, EndToEndDetectionDiagnosisRecovery) {
    // ARRANGE
    metacognition_t* meta = metacognition_create(NULL);
    ASSERT_NE(meta, nullptr);

    // Phase 1: Establish healthy baseline
    for (int i = 0; i < 50; i++) {
        cognitive_state_t state = simulate_cognitive_task(0.2f);
        metacognition_monitor_self(meta, &state);
    }
    EXPECT_FALSE(metacognition_is_degraded(meta, 0.7f));

    // Phase 2: Induce degradation
    for (int i = 0; i < 20; i++) {
        cognitive_state_t state = simulate_cognitive_task(2.0f);  // Severe
        metacognition_monitor_self(meta, &state);
    }

    // Phase 3: Detect degradation
    EXPECT_TRUE(metacognition_is_degraded(meta, 0.7f));

    // Phase 4: Self-diagnose
    diagnosis_t* diagnosis = metacognition_self_diagnose(meta);
    ASSERT_NE(diagnosis, nullptr);
    EXPECT_GT(diagnosis->severity, 0.5f);

    // Phase 5: Recovery action (simulated - reduce workload)
    for (int i = 0; i < 30; i++) {
        cognitive_state_t state = simulate_cognitive_task(0.1f);  // Easy
        metacognition_monitor_self(meta, &state);
    }

    // Phase 6: Verify recovery
    EXPECT_FALSE(metacognition_is_degraded(meta, 0.7f));
    float confidence = metacognition_get_self_confidence(meta);
    EXPECT_GT(confidence, 0.5f);

    // CLEANUP
    diagnosis_destroy(diagnosis);
    metacognition_destroy(meta);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
