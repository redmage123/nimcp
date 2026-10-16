/**
 * @file test_bias_snn_plasticity_regression.cpp
 * @brief Regression tests for Cognitive Bias SNN-Plasticity bridges
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Regression tests to ensure bias detection maintains performance
 * WHY:  Prevent regressions in bias type detection, learning rates, accuracy
 * HOW:  Test known baseline scenarios, boundary conditions, performance thresholds
 */

#include <gtest/gtest.h>
#include <cmath>
#include <chrono>

#include "cognitive/bias/nimcp_bias_snn_bridge.h"
#include "cognitive/bias/nimcp_bias_plasticity_bridge.h"

class BiasRegressionTest : public ::testing::Test {
protected:
    bias_snn_bridge_t* snn = nullptr;
    bias_plasticity_bridge_t* plasticity = nullptr;

    void SetUp() override {
        bias_snn_config_t snn_config = bias_snn_config_default();
        snn = bias_snn_create(&snn_config);
        ASSERT_NE(snn, nullptr);

        bias_plasticity_config_t plas_config = bias_plasticity_config_default();
        plasticity = bias_plasticity_create(&plas_config);
        ASSERT_NE(plasticity, nullptr);
    }

    void TearDown() override {
        if (snn) bias_snn_destroy(snn);
        if (plasticity) bias_plasticity_destroy(plasticity);
    }
};

// ============================================================================
// Bias Detection Baseline Tests
// ============================================================================

TEST_F(BiasRegressionTest, AnchoringBiasDetectionBaseline) {
    // Strong anchor should be detected
    bias_snn_encode_decision_context(snn, 0.95f, 0.3f, 0.0f);
    bias_snn_simulate(snn, 150.0f);

    float anchoring = bias_snn_get_bias_level(snn, BIAS_SNN_TYPE_ANCHORING);

    // BASELINE: Strong anchor (0.95) should yield anchoring > 0.3
    EXPECT_GT(anchoring, 0.3f);
}

TEST_F(BiasRegressionTest, RecencyBiasDetectionBaseline) {
    // High recency weight should be detected
    bias_snn_encode_decision_context(snn, 0.0f, 0.9f, 0.0f);
    bias_snn_simulate(snn, 150.0f);

    float recency = bias_snn_get_bias_level(snn, BIAS_SNN_TYPE_RECENCY);

    // BASELINE: Strong recency (0.9) should yield recency > 0.2
    EXPECT_GT(recency, 0.2f);
}

TEST_F(BiasRegressionTest, OptimismBiasDetectionBaseline) {
    // Positive valence should activate optimism
    bias_snn_encode_decision_context(snn, 0.0f, 0.5f, 0.85f);
    bias_snn_simulate(snn, 150.0f);

    float optimism = bias_snn_get_bias_level(snn, BIAS_SNN_TYPE_OPTIMISM);

    // BASELINE: High positive valence should yield optimism > 0.2
    EXPECT_GT(optimism, 0.2f);
}

TEST_F(BiasRegressionTest, PessimismBiasDetectionBaseline) {
    // Negative valence should activate pessimism
    bias_snn_encode_decision_context(snn, 0.0f, 0.5f, -0.85f);
    bias_snn_simulate(snn, 150.0f);

    float pessimism = bias_snn_get_bias_level(snn, BIAS_SNN_TYPE_PESSIMISM);

    // BASELINE: High negative valence should yield pessimism > 0.2
    EXPECT_GT(pessimism, 0.2f);
}

// ============================================================================
// Plasticity Learning Baseline Tests
// ============================================================================

TEST_F(BiasRegressionTest, DetectionLearningBaseline) {
    bias_plasticity_register_synapse(
        plasticity, 1, BIAS_SYNAPSE_DETECTION, 0, 0.5f);

    // 10 correct detections should improve sensitivity
    for (int i = 0; i < 10; i++) {
        bias_plasticity_bias_detected(plasticity, 0, 0.8f, i * 2000);
        bias_plasticity_detection_feedback(plasticity, 0, true, i * 2000 + 1000);
        bias_plasticity_update(plasticity, 10.0f);
    }

    float sensitivity = bias_plasticity_get_detection_sensitivity(plasticity, 0);

    // BASELINE: 10 correct detections should yield sensitivity > 0.6
    EXPECT_GT(sensitivity, 0.6f);
}

TEST_F(BiasRegressionTest, MetacognitionGrowthBaseline) {
    bias_plasticity_register_synapse(
        plasticity, 1, BIAS_SYNAPSE_METACOGNITIVE, 0, 0.3f);

    // Multiple insights should grow metacognitive awareness
    for (int i = 0; i < 10; i++) {
        bias_plasticity_metacognitive_insight(plasticity, 0, 0.6f, i * 1000);
        bias_plasticity_update(plasticity, 10.0f);
    }

    float awareness = bias_plasticity_get_metacognitive_awareness(plasticity, 0);

    // BASELINE: 10 insights should yield awareness > 0.3
    EXPECT_GT(awareness, 0.3f);
}

// ============================================================================
// Boundary Condition Tests
// ============================================================================

TEST_F(BiasRegressionTest, ZeroContextBoundary) {
    bias_snn_encode_decision_context(snn, 0.0f, 0.0f, 0.0f);
    bias_snn_simulate(snn, 100.0f);

    bias_snn_output_t output;
    int result = bias_snn_detect_biases(snn, &output);

    EXPECT_EQ(result, 0);
    EXPECT_GE(output.overall_bias_level, 0.0f);
    EXPECT_LE(output.overall_bias_level, 1.0f);
}

TEST_F(BiasRegressionTest, MaxContextBoundary) {
    bias_snn_encode_decision_context(snn, 1.0f, 1.0f, 1.0f);
    bias_snn_simulate(snn, 100.0f);

    bias_snn_output_t output;
    int result = bias_snn_detect_biases(snn, &output);

    EXPECT_EQ(result, 0);
    // High across all dimensions should still produce valid output
    EXPECT_GE(output.overall_bias_level, 0.0f);
}

TEST_F(BiasRegressionTest, NegativeValenceBoundary) {
    bias_snn_encode_decision_context(snn, 0.5f, 0.5f, -1.0f);
    bias_snn_simulate(snn, 100.0f);

    int result = bias_snn_get_bias_level(snn, BIAS_SNN_TYPE_PESSIMISM);
    // Should not crash with negative valence
    EXPECT_GE(result, 0.0f);
}

// ============================================================================
// Performance Regression Tests
// ============================================================================

TEST_F(BiasRegressionTest, SNNSimulationPerformance) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; i++) {
        bias_snn_encode_decision_context(snn, 0.8f, 0.5f, 0.3f);
        bias_snn_simulate(snn, 100.0f);
        bias_snn_reset(snn);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // BASELINE: 100 iterations should complete in < 5000ms
    EXPECT_LT(duration, 5000);
}

TEST_F(BiasRegressionTest, PlasticityUpdatePerformance) {
    bias_plasticity_register_synapse(
        plasticity, 1, BIAS_SYNAPSE_DETECTION, 0, 0.5f);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; i++) {
        bias_plasticity_bias_detected(plasticity, 0, 0.7f, i * 100);
        bias_plasticity_detection_feedback(plasticity, 0, i % 2 == 0, i * 100 + 50);
        bias_plasticity_update(plasticity, 1.0f);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // BASELINE: 1000 updates should complete in < 1000ms
    EXPECT_LT(duration, 1000);
}

// ============================================================================
// Consistency Tests
// ============================================================================

TEST_F(BiasRegressionTest, BiasTypeConsistency) {
    // Verify bias type names are consistent
    EXPECT_STREQ(bias_snn_type_name(BIAS_SNN_TYPE_CONFIRMATION), "confirmation");
    EXPECT_STREQ(bias_snn_type_name(BIAS_SNN_TYPE_AVAILABILITY), "availability");
    EXPECT_STREQ(bias_snn_type_name(BIAS_SNN_TYPE_ANCHORING), "anchoring");
    EXPECT_STREQ(bias_snn_type_name(BIAS_SNN_TYPE_RECENCY), "recency");
}

TEST_F(BiasRegressionTest, DeterministicDetection) {
    float level1, level2;

    bias_snn_encode_decision_context(snn, 0.9f, 0.3f, 0.0f);
    bias_snn_simulate(snn, 100.0f);
    level1 = bias_snn_get_bias_level(snn, BIAS_SNN_TYPE_ANCHORING);

    bias_snn_reset(snn);

    bias_snn_encode_decision_context(snn, 0.9f, 0.3f, 0.0f);
    bias_snn_simulate(snn, 100.0f);
    level2 = bias_snn_get_bias_level(snn, BIAS_SNN_TYPE_ANCHORING);

    // Should be approximately equal
    EXPECT_NEAR(level1, level2, 0.15f);
}

// ============================================================================
// Known Issue Regression Tests
// ============================================================================

TEST_F(BiasRegressionTest, NoBiasTypeLeakage) {
    // Detecting one bias type should not affect others unexpectedly
    bias_snn_encode_decision_context(snn, 0.95f, 0.0f, 0.0f);  // Pure anchoring
    bias_snn_simulate(snn, 150.0f);

    float anchoring = bias_snn_get_bias_level(snn, BIAS_SNN_TYPE_ANCHORING);
    float recency = bias_snn_get_bias_level(snn, BIAS_SNN_TYPE_RECENCY);

    // Anchoring should dominate, recency should be low
    EXPECT_GT(anchoring, recency);
}

TEST_F(BiasRegressionTest, ResetClearsAllState) {
    bias_snn_encode_decision_context(snn, 0.9f, 0.9f, 0.9f);
    bias_snn_simulate(snn, 100.0f);

    bias_snn_reset(snn);

    bias_snn_bridge_state_t state;
    bias_snn_get_state(snn, &state);

    EXPECT_EQ(state.state, BIAS_SNN_STATE_IDLE);
}
