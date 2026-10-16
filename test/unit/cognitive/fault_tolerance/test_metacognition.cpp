/**
 * @file test_metacognition.cpp
 * @brief Unit tests for Metacognition Self-Monitoring Module
 * @version 1.0.0
 * @date 2025-01-20
 *
 * WHAT: Comprehensive unit tests for metacognition self-monitoring
 * WHY: Ensure brain can monitor its own cognitive health and detect degradation
 * HOW: Test-driven development with 100% coverage of all public APIs
 *
 * Test Coverage:
 * - Creation and destruction
 * - Self-monitoring of cognitive health
 * - Performance baseline tracking
 * - Degradation detection
 * - Self-diagnosis
 * - Confidence calibration
 * - Error handling
 * - Edge cases
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "cognitive/fault_tolerance/nimcp_metacognition.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

//=============================================================================
// Test Fixture
//=============================================================================

class MetacognitionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize memory tracking
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
    }

    void TearDown() override {
        // Check for memory leaks
        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        EXPECT_EQ(stats.current_allocated, 0) << "Memory leak detected!";
    }
};

//=============================================================================
// Creation and Destruction Tests
//=============================================================================

/**
 * @test Metacognition Creation with Default Config
 *
 * WHAT: Verify metacognition system can be created with default configuration
 * WHY: Ensure proper initialization of all components
 * HOW: Create with defaults, verify structure, destroy
 */
TEST_F(MetacognitionTest, CreateWithDefaults) {
    // ACT: Create with default config
    metacognition_t* meta = metacognition_create(NULL);

    // ASSERT: Created successfully
    ASSERT_NE(meta, nullptr);

    // Verify initial state
    EXPECT_TRUE(metacognition_is_initialized(meta));
    EXPECT_GT(metacognition_get_self_confidence(meta), 0.0f);
    EXPECT_LT(metacognition_get_self_confidence(meta), 1.0f);

    // CLEANUP
    metacognition_destroy(meta);
}

/**
 * @test Metacognition Creation with Custom Config
 *
 * WHAT: Verify metacognition system accepts custom configuration
 * WHY: Allow users to customize monitoring parameters
 * HOW: Create with custom config, verify settings applied
 */
TEST_F(MetacognitionTest, CreateWithCustomConfig) {
    // ARRANGE: Custom config
    metacognition_config_t config = metacognition_default_config();
    config.baseline_window_size = 1000;
    config.degradation_threshold = 0.6f;
    config.confidence_learning_rate = 0.05f;

    // ACT: Create with custom config
    metacognition_t* meta = metacognition_create(&config);

    // ASSERT: Created with custom settings
    ASSERT_NE(meta, nullptr);
    EXPECT_TRUE(metacognition_is_initialized(meta));

    // CLEANUP
    metacognition_destroy(meta);
}

/**
 * @test Metacognition Destroy NULL Safety
 *
 * WHAT: Verify destroy handles NULL gracefully
 * WHY: Prevent crashes on double-free or accidental NULL
 * HOW: Call destroy with NULL, expect no crash
 */
TEST_F(MetacognitionTest, DestroyNullSafety) {
    // ACT & ASSERT: Should not crash
    EXPECT_NO_THROW(metacognition_destroy(NULL));
}

//=============================================================================
// Self-Monitoring Tests
//=============================================================================

/**
 * @test Monitor Cognitive Health - Normal Operation
 *
 * WHAT: Verify metacognition can monitor cognitive health metrics
 * WHY: Core functionality for self-awareness
 * HOW: Create mock brain state, monitor, verify metrics recorded
 */
TEST_F(MetacognitionTest, MonitorCognitiveHealthNormal) {
    // ARRANGE
    metacognition_t* meta = metacognition_create(NULL);
    ASSERT_NE(meta, nullptr);

    // Create mock cognitive state
    cognitive_state_t state = {0};
    state.reasoning_speed = 1.0f;          // Normal speed
    state.memory_recall_accuracy = 0.95f;  // High accuracy
    state.decision_quality = 0.90f;        // Good decisions
    state.learning_rate_actual = 0.8f;     // Learning well
    state.attention_focus = 0.85f;         // Focused

    // ACT: Monitor cognitive health
    bool result = metacognition_monitor_self(meta, &state);

    // ASSERT: Monitoring succeeded
    EXPECT_TRUE(result);
    EXPECT_FALSE(metacognition_is_degraded(meta, 0.7f));

    // CLEANUP
    metacognition_destroy(meta);
}

/**
 * @test Monitor Cognitive Health - Reasoning Slowdown
 *
 * WHAT: Verify detection of reasoning speed degradation
 * WHY: Early warning of cognitive slowdown
 * HOW: Monitor with slow reasoning speed, verify degradation detected
 */
TEST_F(MetacognitionTest, MonitorCognitiveHealthSlowdown) {
    // ARRANGE
    metacognition_t* meta = metacognition_create(NULL);
    ASSERT_NE(meta, nullptr);

    // Establish baseline with normal performance
    cognitive_state_t baseline_state = {0};
    baseline_state.reasoning_speed = 1.0f;
    baseline_state.memory_recall_accuracy = 0.95f;
    baseline_state.decision_quality = 0.90f;
    baseline_state.learning_rate_actual = 0.8f;
    baseline_state.attention_focus = 0.85f;

    for (int i = 0; i < 10; i++) {
        metacognition_monitor_self(meta, &baseline_state);
    }

    // ACT: Monitor with degraded reasoning speed (50% slower)
    cognitive_state_t degraded_state = baseline_state;
    degraded_state.reasoning_speed = 0.5f;  // 50% slower than baseline

    metacognition_monitor_self(meta, &degraded_state);

    // ASSERT: Degradation detected
    EXPECT_TRUE(metacognition_is_degraded(meta, 0.7f));

    // CLEANUP
    metacognition_destroy(meta);
}

/**
 * @test Monitor Cognitive Health - Memory Issues
 *
 * WHAT: Verify detection of memory recall degradation
 * WHY: Identify potential memory corruption or failure
 * HOW: Monitor with low memory accuracy, verify detected
 */
TEST_F(MetacognitionTest, MonitorCognitiveHealthMemoryIssues) {
    // ARRANGE
    metacognition_t* meta = metacognition_create(NULL);
    ASSERT_NE(meta, nullptr);

    // Create state with poor memory recall
    cognitive_state_t state = {0};
    state.reasoning_speed = 1.0f;
    state.memory_recall_accuracy = 0.6f;   // Poor accuracy
    state.decision_quality = 0.90f;
    state.learning_rate_actual = 0.8f;
    state.attention_focus = 0.85f;

    // ACT: Monitor cognitive health
    metacognition_monitor_self(meta, &state);

    // ASSERT: Memory issues detected
    diagnosis_t* diagnosis = metacognition_self_diagnose(meta);
    ASSERT_NE(diagnosis, nullptr);
    EXPECT_TRUE(diagnosis->has_memory_issues);

    // CLEANUP
    diagnosis_destroy(diagnosis);
    metacognition_destroy(meta);
}

//=============================================================================
// Performance Baseline Tests
//=============================================================================

/**
 * @test Baseline Establishment
 *
 * WHAT: Verify baseline performance is established over time
 * WHY: Need baseline to detect degradation
 * HOW: Feed multiple samples, verify baseline computed
 */
TEST_F(MetacognitionTest, BaselineEstablishment) {
    // ARRANGE
    metacognition_t* meta = metacognition_create(NULL);
    ASSERT_NE(meta, nullptr);

    cognitive_state_t state = {0};
    state.reasoning_speed = 1.0f;
    state.memory_recall_accuracy = 0.95f;
    state.decision_quality = 0.90f;
    state.learning_rate_actual = 0.8f;
    state.attention_focus = 0.85f;

    // ACT: Feed samples to establish baseline
    for (int i = 0; i < 100; i++) {
        metacognition_monitor_self(meta, &state);
    }

    // ASSERT: Baseline established
    performance_baseline_t baseline;
    bool has_baseline = metacognition_get_baseline(meta, &baseline);
    EXPECT_TRUE(has_baseline);
    EXPECT_NEAR(baseline.reasoning_speed, 1.0f, 0.1f);
    EXPECT_NEAR(baseline.memory_recall_accuracy, 0.95f, 0.05f);

    // CLEANUP
    metacognition_destroy(meta);
}

/**
 * @test Baseline Update Over Time
 *
 * WHAT: Verify baseline adapts to sustained performance changes
 * WHY: Adapt to gradual improvements or long-term changes
 * HOW: Change performance, verify baseline updates
 */
TEST_F(MetacognitionTest, BaselineUpdateOverTime) {
    // ARRANGE
    metacognition_t* meta = metacognition_create(NULL);
    ASSERT_NE(meta, nullptr);

    // Establish initial baseline
    cognitive_state_t state = {0};
    state.reasoning_speed = 1.0f;
    state.memory_recall_accuracy = 0.95f;
    state.decision_quality = 0.90f;
    state.learning_rate_actual = 0.8f;
    state.attention_focus = 0.85f;

    for (int i = 0; i < 100; i++) {
        metacognition_monitor_self(meta, &state);
    }

    // ACT: Sustained improvement
    state.reasoning_speed = 1.5f;  // 50% faster
    for (int i = 0; i < 200; i++) {
        metacognition_monitor_self(meta, &state);
    }

    // ASSERT: Baseline adapted
    performance_baseline_t baseline;
    metacognition_get_baseline(meta, &baseline);
    EXPECT_GT(baseline.reasoning_speed, 1.2f);  // Baseline increased

    // CLEANUP
    metacognition_destroy(meta);
}

//=============================================================================
// Degradation Detection Tests
//=============================================================================

/**
 * @test Degradation Detection - Threshold-Based
 *
 * WHAT: Verify degradation detection with configurable threshold
 * WHY: Allow tuning of sensitivity
 * HOW: Test different thresholds, verify detection behavior
 */
TEST_F(MetacognitionTest, DegradationDetectionThreshold) {
    // ARRANGE
    metacognition_t* meta = metacognition_create(NULL);
    ASSERT_NE(meta, nullptr);

    // Establish baseline
    cognitive_state_t state = {0};
    state.reasoning_speed = 1.0f;
    state.memory_recall_accuracy = 0.95f;
    state.decision_quality = 0.90f;
    state.learning_rate_actual = 0.8f;
    state.attention_focus = 0.85f;

    for (int i = 0; i < 50; i++) {
        metacognition_monitor_self(meta, &state);
    }

    // ACT: Degrade performance by 20%
    state.reasoning_speed = 0.8f;
    state.memory_recall_accuracy = 0.75f;
    metacognition_monitor_self(meta, &state);

    // ASSERT: Detected at 0.7 threshold, not at 0.5 threshold
    EXPECT_TRUE(metacognition_is_degraded(meta, 0.7f));   // 70% threshold
    EXPECT_FALSE(metacognition_is_degraded(meta, 0.5f));  // 50% threshold

    // CLEANUP
    metacognition_destroy(meta);
}

/**
 * @test Degradation Detection - Multiple Metrics
 *
 * WHAT: Verify degradation considers all cognitive metrics
 * WHY: Comprehensive health assessment
 * HOW: Degrade different metrics, verify all detected
 */
TEST_F(MetacognitionTest, DegradationDetectionMultipleMetrics) {
    // ARRANGE
    metacognition_t* meta = metacognition_create(NULL);
    ASSERT_NE(meta, nullptr);

    // Establish baseline
    cognitive_state_t state = {0};
    state.reasoning_speed = 1.0f;
    state.memory_recall_accuracy = 0.95f;
    state.decision_quality = 0.90f;
    state.learning_rate_actual = 0.8f;
    state.attention_focus = 0.85f;

    for (int i = 0; i < 50; i++) {
        metacognition_monitor_self(meta, &state);
    }

    // ACT: Degrade different metrics separately

    // Test 1: Reasoning speed
    state.reasoning_speed = 0.6f;
    metacognition_monitor_self(meta, &state);
    EXPECT_TRUE(metacognition_is_degraded(meta, 0.7f));

    // Reset
    state.reasoning_speed = 1.0f;

    // Test 2: Memory recall
    state.memory_recall_accuracy = 0.65f;
    metacognition_monitor_self(meta, &state);
    EXPECT_TRUE(metacognition_is_degraded(meta, 0.7f));

    // Reset
    state.memory_recall_accuracy = 0.95f;

    // Test 3: Decision quality
    state.decision_quality = 0.60f;
    metacognition_monitor_self(meta, &state);
    EXPECT_TRUE(metacognition_is_degraded(meta, 0.7f));

    // CLEANUP
    metacognition_destroy(meta);
}

//=============================================================================
// Self-Diagnosis Tests
//=============================================================================

/**
 * @test Self-Diagnosis - Cognitive Slowdown
 *
 * WHAT: Verify self-diagnosis identifies reasoning slowdown
 * WHY: Provide actionable diagnosis
 * HOW: Degrade reasoning, verify diagnosis
 */
TEST_F(MetacognitionTest, SelfDiagnosisCognitiveSlowdown) {
    // ARRANGE
    metacognition_t* meta = metacognition_create(NULL);
    ASSERT_NE(meta, nullptr);

    // Establish baseline
    cognitive_state_t state = {0};
    state.reasoning_speed = 1.0f;
    state.memory_recall_accuracy = 0.95f;
    state.decision_quality = 0.90f;
    state.learning_rate_actual = 0.8f;
    state.attention_focus = 0.85f;

    for (int i = 0; i < 50; i++) {
        metacognition_monitor_self(meta, &state);
    }

    // ACT: Induce slowdown
    state.reasoning_speed = 0.5f;
    metacognition_monitor_self(meta, &state);

    diagnosis_t* diagnosis = metacognition_self_diagnose(meta);

    // ASSERT: Diagnosed as cognitive slowdown
    ASSERT_NE(diagnosis, nullptr);
    EXPECT_EQ(diagnosis->primary_issue, DIAGNOSIS_COGNITIVE_SLOWDOWN);
    EXPECT_GT(diagnosis->severity, 0.5f);

    // CLEANUP
    diagnosis_destroy(diagnosis);
    metacognition_destroy(meta);
}

/**
 * @test Self-Diagnosis - Memory Corruption
 *
 * WHAT: Verify self-diagnosis identifies memory issues
 * WHY: Detect data corruption early
 * HOW: Degrade memory accuracy, verify diagnosis
 */
TEST_F(MetacognitionTest, SelfDiagnosisMemoryCorruption) {
    // ARRANGE
    metacognition_t* meta = metacognition_create(NULL);
    ASSERT_NE(meta, nullptr);

    // Create state with memory issues
    cognitive_state_t state = {0};
    state.reasoning_speed = 1.0f;
    state.memory_recall_accuracy = 0.4f;  // Very poor
    state.decision_quality = 0.90f;
    state.learning_rate_actual = 0.8f;
    state.attention_focus = 0.85f;

    // ACT: Monitor and diagnose
    metacognition_monitor_self(meta, &state);
    diagnosis_t* diagnosis = metacognition_self_diagnose(meta);

    // ASSERT: Diagnosed as memory corruption
    ASSERT_NE(diagnosis, nullptr);
    EXPECT_EQ(diagnosis->primary_issue, DIAGNOSIS_MEMORY_CORRUPTION);
    EXPECT_TRUE(diagnosis->has_memory_issues);

    // CLEANUP
    diagnosis_destroy(diagnosis);
    metacognition_destroy(meta);
}

/**
 * @test Self-Diagnosis - Healthy State
 *
 * WHAT: Verify self-diagnosis reports healthy state
 * WHY: Confirm no false positives
 * HOW: Monitor healthy state, verify no issues
 */
TEST_F(MetacognitionTest, SelfDiagnosisHealthy) {
    // ARRANGE
    metacognition_t* meta = metacognition_create(NULL);
    ASSERT_NE(meta, nullptr);

    cognitive_state_t state = {0};
    state.reasoning_speed = 1.0f;
    state.memory_recall_accuracy = 0.95f;
    state.decision_quality = 0.90f;
    state.learning_rate_actual = 0.8f;
    state.attention_focus = 0.85f;

    // ACT: Monitor healthy state
    for (int i = 0; i < 20; i++) {
        metacognition_monitor_self(meta, &state);
    }

    diagnosis_t* diagnosis = metacognition_self_diagnose(meta);

    // ASSERT: Healthy diagnosis
    ASSERT_NE(diagnosis, nullptr);
    EXPECT_EQ(diagnosis->primary_issue, DIAGNOSIS_HEALTHY);
    EXPECT_LT(diagnosis->severity, 0.3f);

    // CLEANUP
    diagnosis_destroy(diagnosis);
    metacognition_destroy(meta);
}

//=============================================================================
// Confidence Calibration Tests
//=============================================================================

/**
 * @test Confidence Calibration - Success Increases Confidence
 *
 * WHAT: Verify confidence increases after successful predictions
 * WHY: Self-calibrating confidence based on outcomes
 * HOW: Calibrate with successes, verify confidence increases
 */
TEST_F(MetacognitionTest, ConfidenceCalibrationSuccess) {
    // ARRANGE
    metacognition_t* meta = metacognition_create(NULL);
    ASSERT_NE(meta, nullptr);

    float initial_confidence = 0.5f;

    // ACT: Series of successful predictions
    float confidence = initial_confidence;
    for (int i = 0; i < 10; i++) {
        confidence = metacognition_calibrate_confidence(meta, confidence, true);
    }

    // ASSERT: Confidence increased
    EXPECT_GT(confidence, initial_confidence);
    EXPECT_LE(confidence, 1.0f);

    // CLEANUP
    metacognition_destroy(meta);
}

/**
 * @test Confidence Calibration - Failure Decreases Confidence
 *
 * WHAT: Verify confidence decreases after failed predictions
 * WHY: Avoid overconfidence
 * HOW: Calibrate with failures, verify confidence decreases
 */
TEST_F(MetacognitionTest, ConfidenceCalibrationFailure) {
    // ARRANGE
    metacognition_t* meta = metacognition_create(NULL);
    ASSERT_NE(meta, nullptr);

    float initial_confidence = 0.8f;

    // ACT: Series of failed predictions
    float confidence = initial_confidence;
    for (int i = 0; i < 10; i++) {
        confidence = metacognition_calibrate_confidence(meta, confidence, false);
    }

    // ASSERT: Confidence decreased
    EXPECT_LT(confidence, initial_confidence);
    EXPECT_GE(confidence, 0.0f);

    // CLEANUP
    metacognition_destroy(meta);
}

/**
 * @test Confidence Calibration - Boundary Conditions
 *
 * WHAT: Verify confidence stays within [0, 1] bounds
 * WHY: Prevent invalid confidence values
 * HOW: Test extreme cases, verify bounds respected
 */
TEST_F(MetacognitionTest, ConfidenceCalibrationBounds) {
    // ARRANGE
    metacognition_t* meta = metacognition_create(NULL);
    ASSERT_NE(meta, nullptr);

    // ACT & ASSERT: Test upper bound
    float confidence = 0.99f;
    for (int i = 0; i < 20; i++) {
        confidence = metacognition_calibrate_confidence(meta, confidence, true);
    }
    EXPECT_LE(confidence, 1.0f);
    EXPECT_GE(confidence, 0.0f);

    // ACT & ASSERT: Test lower bound
    confidence = 0.01f;
    for (int i = 0; i < 20; i++) {
        confidence = metacognition_calibrate_confidence(meta, confidence, false);
    }
    EXPECT_GE(confidence, 0.0f);
    EXPECT_LE(confidence, 1.0f);

    // CLEANUP
    metacognition_destroy(meta);
}

//=============================================================================
// Uncertainty Tracking Tests
//=============================================================================

/**
 * @test Uncertainty Measurement
 *
 * WHAT: Verify uncertainty is tracked and reported
 * WHY: Know when to ask for help
 * HOW: Monitor varying states, measure uncertainty
 */
TEST_F(MetacognitionTest, UncertaintyMeasurement) {
    // ARRANGE
    metacognition_t* meta = metacognition_create(NULL);
    ASSERT_NE(meta, nullptr);

    // ACT: Highly variable performance = high uncertainty
    cognitive_state_t state = {0};
    for (int i = 0; i < 20; i++) {
        state.reasoning_speed = (i % 2 == 0) ? 1.0f : 0.5f;  // Alternating
        state.memory_recall_accuracy = 0.95f;
        state.decision_quality = 0.90f;
        state.learning_rate_actual = 0.8f;
        state.attention_focus = 0.85f;
        metacognition_monitor_self(meta, &state);
    }

    float uncertainty = metacognition_get_uncertainty(meta);

    // ASSERT: High uncertainty due to variability
    EXPECT_GT(uncertainty, 0.3f);

    // CLEANUP
    metacognition_destroy(meta);
}

/**
 * @test High Uncertainty Detection
 *
 * WHAT: Verify detection of high uncertainty states
 * WHY: Trigger help requests when needed
 * HOW: Create uncertain state, verify detection
 */
TEST_F(MetacognitionTest, HighUncertaintyDetection) {
    // ARRANGE
    metacognition_t* meta = metacognition_create(NULL);
    ASSERT_NE(meta, nullptr);

    // ACT: Create highly uncertain state
    cognitive_state_t state = {0};
    for (int i = 0; i < 30; i++) {
        state.reasoning_speed = 0.3f + (rand() % 100) / 100.0f;  // Random
        state.memory_recall_accuracy = 0.95f;
        state.decision_quality = 0.90f;
        state.learning_rate_actual = 0.8f;
        state.attention_focus = 0.85f;
        metacognition_monitor_self(meta, &state);
    }

    // ASSERT: High uncertainty detected
    EXPECT_TRUE(metacognition_has_high_uncertainty(meta, 0.5f));

    // CLEANUP
    metacognition_destroy(meta);
}

//=============================================================================
// Error Handling Tests
//=============================================================================

/**
 * @test NULL Parameter Handling
 *
 * WHAT: Verify all functions handle NULL parameters gracefully
 * WHY: Prevent crashes from invalid input
 * HOW: Call all functions with NULL, expect false/0 returns
 */
TEST_F(MetacognitionTest, NullParameterHandling) {
    // ACT & ASSERT: All functions should handle NULL gracefully
    EXPECT_FALSE(metacognition_is_initialized(NULL));
    EXPECT_EQ(metacognition_get_self_confidence(NULL), 0.0f);
    EXPECT_EQ(metacognition_get_uncertainty(NULL), 0.0f);
    EXPECT_FALSE(metacognition_is_degraded(NULL, 0.7f));
    EXPECT_EQ(metacognition_self_diagnose(NULL), nullptr);
    EXPECT_EQ(metacognition_calibrate_confidence(NULL, 0.5f, true), 0.5f);
    EXPECT_FALSE(metacognition_monitor_self(NULL, nullptr));
}

/**
 * @test Invalid Threshold Handling
 *
 * WHAT: Verify degradation detection handles invalid thresholds
 * WHY: Prevent logic errors from bad thresholds
 * HOW: Test with out-of-range thresholds
 */
TEST_F(MetacognitionTest, InvalidThresholdHandling) {
    // ARRANGE
    metacognition_t* meta = metacognition_create(NULL);
    ASSERT_NE(meta, nullptr);

    // ACT & ASSERT: Invalid thresholds should be clamped or rejected
    EXPECT_FALSE(metacognition_is_degraded(meta, -0.5f));  // Negative
    EXPECT_FALSE(metacognition_is_degraded(meta, 1.5f));   // > 1.0

    // CLEANUP
    metacognition_destroy(meta);
}

//=============================================================================
// Integration Helper Tests
//=============================================================================

/**
 * @test Get Cognitive Health Snapshot
 *
 * WHAT: Verify ability to retrieve current health snapshot
 * WHY: Allow external monitoring
 * HOW: Monitor state, retrieve snapshot, verify data
 */
TEST_F(MetacognitionTest, GetCognitiveHealthSnapshot) {
    // ARRANGE
    metacognition_t* meta = metacognition_create(NULL);
    ASSERT_NE(meta, nullptr);

    cognitive_state_t state = {0};
    state.reasoning_speed = 1.2f;
    state.memory_recall_accuracy = 0.93f;
    state.decision_quality = 0.88f;
    state.learning_rate_actual = 0.75f;
    state.attention_focus = 0.82f;

    metacognition_monitor_self(meta, &state);

    // ACT: Get snapshot
    cognitive_health_t health;
    bool result = metacognition_get_current_health(meta, &health);

    // ASSERT: Snapshot retrieved
    EXPECT_TRUE(result);
    EXPECT_NEAR(health.reasoning_speed, 1.2f, 0.01f);
    EXPECT_NEAR(health.memory_recall_accuracy, 0.93f, 0.01f);

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
