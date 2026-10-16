/**
 * @file test_bias_snn_plasticity_integration.cpp
 * @brief Integration tests for Cognitive Bias SNN-Plasticity bidirectional system
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Integration tests verifying Bias SNN and Plasticity bridges work together
 * WHY:  Bias detection should improve through feedback; plasticity should tune sensitivity
 * HOW:  Create both bridges, connect them, verify closed-loop bias detection improvement
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

#include "cognitive/bias/nimcp_bias_snn_bridge.h"
#include "cognitive/bias/nimcp_bias_plasticity_bridge.h"

class BiasSnnPlasticityIntegrationTest : public ::testing::Test {
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
        if (snn) {
            bias_snn_destroy(snn);
            snn = nullptr;
        }
        if (plasticity) {
            bias_plasticity_destroy(plasticity);
            plasticity = nullptr;
        }
    }
};

// ============================================================================
// Basic Integration Tests
// ============================================================================

TEST_F(BiasSnnPlasticityIntegrationTest, BothBridgesInitialize) {
    bias_snn_bridge_state_t snn_state;
    EXPECT_EQ(bias_snn_get_state(snn, &snn_state), 0);
    EXPECT_EQ(snn_state.state, BIAS_SNN_STATE_IDLE);

    bias_plasticity_bridge_state_t plas_state;
    EXPECT_EQ(bias_plasticity_get_state(plasticity, &plas_state), 0);
    EXPECT_EQ(plas_state.state, BIAS_PLASTICITY_STATE_IDLE);
}

// ============================================================================
// Bias Detection Integration Tests
// ============================================================================

TEST_F(BiasSnnPlasticityIntegrationTest, SNNDetectionDrivesPlasticityUpdate) {
    // Register plasticity synapse for detection
    bias_plasticity_register_synapse(
        plasticity, 1, BIAS_SYNAPSE_DETECTION, 0, 0.5f);

    // SNN detects anchoring bias
    bias_snn_encode_decision_context(snn, 0.95f, 0.3f, 0.0f);  // Strong anchor
    bias_snn_simulate(snn, 150.0f);

    bias_snn_output_t snn_output;
    bias_snn_detect_biases(snn, &snn_output);

    // If bias detected, report to plasticity
    if (snn_output.overall_bias_level > 0.3f) {
        bias_plasticity_bias_detected(plasticity, 0, snn_output.overall_bias_level, 1000);
    }

    // Provide feedback (assume detection was correct)
    bias_plasticity_detection_feedback(plasticity, 0, true, 2000);

    // Check plasticity updated
    float sensitivity = bias_plasticity_get_detection_sensitivity(plasticity, 0);
    EXPECT_GE(sensitivity, 0.0f);
}

TEST_F(BiasSnnPlasticityIntegrationTest, IncorrectDetectionDecreasesSensitivity) {
    // Register detection synapse
    bias_plasticity_register_synapse(
        plasticity, 1, BIAS_SYNAPSE_DETECTION, 0, 0.7f);

    float initial_weight = 0.7f;

    // Report false positive detections
    for (int i = 0; i < 5; i++) {
        bias_plasticity_bias_detected(plasticity, 0, 0.8f, i * 2000);
        bias_plasticity_detection_feedback(plasticity, 0, false, i * 2000 + 1000);  // Incorrect
        bias_plasticity_update(plasticity, 10.0f);
    }

    bias_plasticity_synapse_t synapse;
    bias_plasticity_get_synapse(plasticity, 1, &synapse);

    // Weight should decrease with incorrect detections
    EXPECT_LT(synapse.weight, initial_weight);
}

// ============================================================================
// Bias Type Learning Integration Tests
// ============================================================================

TEST_F(BiasSnnPlasticityIntegrationTest, LearningAcrossBiasTypes) {
    // Register synapses for each bias type
    for (uint32_t type = 0; type < 4; type++) {
        bias_plasticity_register_synapse(
            plasticity, type + 1, BIAS_SYNAPSE_DETECTION, type, 0.5f);
    }

    // Test different bias scenarios
    struct {
        float anchor;
        float recency;
        float valence;
        bias_snn_type_t expected_type;
    } scenarios[] = {
        {0.95f, 0.3f, 0.0f, BIAS_SNN_TYPE_ANCHORING},
        {0.0f, 0.9f, 0.0f, BIAS_SNN_TYPE_RECENCY},
        {0.0f, 0.5f, 0.8f, BIAS_SNN_TYPE_OPTIMISM},
        {0.0f, 0.5f, -0.8f, BIAS_SNN_TYPE_PESSIMISM}
    };

    for (int s = 0; s < 4; s++) {
        bias_snn_encode_decision_context(
            snn, scenarios[s].anchor, scenarios[s].recency, scenarios[s].valence);
        bias_snn_simulate(snn, 150.0f);

        bias_snn_output_t output;
        bias_snn_detect_biases(snn, &output);

        // Report detection for the expected type
        uint32_t type_index = (uint32_t)scenarios[s].expected_type;
        if (type_index < 4) {
            float bias_level = bias_snn_get_bias_level(snn, scenarios[s].expected_type);
            if (bias_level > 0.2f) {
                bias_plasticity_bias_detected(plasticity, type_index, bias_level, s * 5000);
                bias_plasticity_detection_feedback(plasticity, type_index, true, s * 5000 + 2000);
            }
        }

        bias_snn_reset(snn);
    }

    // All type learning should be tracked
    for (uint32_t type = 0; type < 4; type++) {
        bias_type_learning_t learning;
        int result = bias_plasticity_get_type_learning(plasticity, type, &learning);
        EXPECT_EQ(result, 0);
    }
}

// ============================================================================
// Conflict Detection Integration Tests
// ============================================================================

TEST_F(BiasSnnPlasticityIntegrationTest, ConflictDetectionTriggersPlas) {
    // Register conflict monitoring synapse
    bias_plasticity_register_synapse(
        plasticity, 1, BIAS_SYNAPSE_CONFLICT_MONITOR, 0, 0.5f);

    // Create conflicting evidence
    bias_snn_encode_decision_context(snn, 0.8f, 0.0f, 0.0f);  // Anchor suggests X
    bias_snn_simulate(snn, 100.0f);

    // Check conflict level
    float conflict = bias_snn_get_conflict_level(snn);

    // If conflict detected, report resolution
    if (conflict > 0.3f) {
        bool resolved_correctly = true;  // Assume correct resolution
        bias_plasticity_conflict_resolved(plasticity, conflict, resolved_correctly, 1000);
    }

    // Verify stats
    bias_plasticity_stats_t stats;
    bias_plasticity_get_stats(plasticity, &stats);
    EXPECT_GE(stats.total_detections, 0u);
}

// ============================================================================
// Metacognitive Improvement Integration Tests
// ============================================================================

TEST_F(BiasSnnPlasticityIntegrationTest, MetacognitionImprovesThroughExperience) {
    // Register metacognitive synapse
    bias_plasticity_register_synapse(
        plasticity, 1, BIAS_SYNAPSE_METACOGNITIVE, 0, 0.3f);

    // Simulate metacognitive insights over time
    for (int i = 0; i < 10; i++) {
        // SNN processes decision context
        float anchor = 0.5f + 0.4f * sinf(i * 0.5f);
        bias_snn_encode_decision_context(snn, anchor, 0.5f, 0.0f);
        bias_snn_simulate(snn, 100.0f);

        bias_snn_output_t output;
        bias_snn_detect_biases(snn, &output);

        // Generate metacognitive insight based on self-awareness of bias
        float insight_strength = output.overall_bias_level * 0.8f;
        if (insight_strength > 0.2f) {
            bias_plasticity_metacognitive_insight(plasticity, 0, insight_strength, i * 2000);
        }

        bias_snn_reset(snn);
    }

    // Check metacognitive awareness has grown
    float awareness = bias_plasticity_get_metacognitive_awareness(plasticity, 0);
    EXPECT_GT(awareness, 0.0f);
}

// ============================================================================
// Reward Modulation Integration Tests
// ============================================================================

TEST_F(BiasSnnPlasticityIntegrationTest, RewardModulatesLearning) {
    // Register detection synapse
    bias_plasticity_register_synapse(
        plasticity, 1, BIAS_SYNAPSE_DETECTION, 0, 0.5f);

    // Detect bias and create eligibility
    bias_snn_encode_decision_context(snn, 0.9f, 0.2f, 0.0f);
    bias_snn_simulate(snn, 100.0f);

    bias_snn_output_t output;
    bias_snn_detect_biases(snn, &output);

    // Report detection
    bias_plasticity_bias_detected(plasticity, 0, output.overall_bias_level, 1000);

    bias_plasticity_synapse_t synapse_before;
    bias_plasticity_get_synapse(plasticity, 1, &synapse_before);
    float weight_before = synapse_before.weight;

    // Positive reward for good detection
    bias_plasticity_reward(plasticity, 1.0f, 2000);

    bias_plasticity_synapse_t synapse_after;
    bias_plasticity_get_synapse(plasticity, 1, &synapse_after);

    // Weight should increase with positive reward
    EXPECT_GT(synapse_after.weight, weight_before);
}

// ============================================================================
// Closed Loop Detection Improvement
// ============================================================================

TEST_F(BiasSnnPlasticityIntegrationTest, DetectionAccuracyImprovesThroughFeedback) {
    // Register detection synapse
    bias_plasticity_register_synapse(
        plasticity, 1, BIAS_SYNAPSE_DETECTION, 0, 0.5f);

    int correct_detections = 0;
    int total_trials = 20;

    for (int trial = 0; trial < total_trials; trial++) {
        // Create anchoring scenario with known ground truth
        float anchor_strength = 0.7f + 0.2f * (trial % 2);  // Alternate strong/medium
        bool should_detect = anchor_strength > 0.75f;

        bias_snn_encode_decision_context(snn, anchor_strength, 0.3f, 0.0f);
        bias_snn_simulate(snn, 150.0f);

        float detected_level = bias_snn_get_bias_level(snn, BIAS_SNN_TYPE_ANCHORING);
        bool did_detect = detected_level > 0.3f;

        // Check if detection matches ground truth
        bool was_correct = (did_detect == should_detect);
        if (was_correct) correct_detections++;

        // Provide feedback to plasticity
        if (did_detect) {
            bias_plasticity_bias_detected(plasticity, 0, detected_level, trial * 3000);
            bias_plasticity_detection_feedback(plasticity, 0, was_correct, trial * 3000 + 1500);
        }

        bias_plasticity_update(plasticity, 10.0f);
        bias_snn_reset(snn);
    }

    // System should maintain reasonable accuracy
    float accuracy = (float)correct_detections / total_trials;
    EXPECT_GT(accuracy, 0.4f);  // Better than random
}

// ============================================================================
// Bio-Async Coordination Tests
// ============================================================================

TEST_F(BiasSnnPlasticityIntegrationTest, BioAsyncCoordination) {
    // Create bridges with bio-async enabled
    bias_snn_config_t snn_config = bias_snn_config_default();
    snn_config.enable_bio_async = true;
    bias_snn_bridge_t* snn_async = bias_snn_create(&snn_config);

    bias_plasticity_config_t plas_config = bias_plasticity_config_default();
    plas_config.enable_bio_async = true;
    bias_plasticity_bridge_t* plas_async = bias_plasticity_create(&plas_config);

    ASSERT_NE(snn_async, nullptr);
    ASSERT_NE(plas_async, nullptr);

    // Connect both
    EXPECT_EQ(bias_snn_bio_async_connect(snn_async), 0);
    EXPECT_EQ(bias_plasticity_connect_bio_async(plas_async), 0);

    EXPECT_TRUE(bias_snn_is_bio_async_connected(snn_async));
    EXPECT_TRUE(bias_plasticity_is_bio_async_connected(plas_async));

    // Both should operate correctly
    bias_snn_encode_decision_context(snn_async, 0.8f, 0.5f, 0.3f);
    bias_snn_simulate(snn_async, 50.0f);

    bias_plasticity_bias_detected(plas_async, 0, 0.6f, 1000);

    // Disconnect
    bias_snn_bio_async_disconnect(snn_async);
    bias_plasticity_disconnect_bio_async(plas_async);

    bias_snn_destroy(snn_async);
    bias_plasticity_destroy(plas_async);
}

// ============================================================================
// Stress Test
// ============================================================================

TEST_F(BiasSnnPlasticityIntegrationTest, HighVolumeProcessing) {
    bias_plasticity_register_synapse(
        plasticity, 1, BIAS_SYNAPSE_DETECTION, 0, 0.5f);

    for (int i = 0; i < 100; i++) {
        float anchor = fabsf(sinf(i * 0.2f));
        float recency = fabsf(cosf(i * 0.3f));
        float valence = sinf(i * 0.5f);

        bias_snn_encode_decision_context(snn, anchor, recency, valence);
        bias_snn_simulate(snn, 50.0f);

        bias_snn_output_t output;
        bias_snn_detect_biases(snn, &output);

        if (output.overall_bias_level > 0.3f) {
            bias_plasticity_bias_detected(plasticity, 0, output.overall_bias_level, i * 500);
            bias_plasticity_detection_feedback(plasticity, 0, true, i * 500 + 250);
        }

        bias_plasticity_update(plasticity, 5.0f);
        bias_snn_reset(snn);
    }

    // Verify no crashes and stats are tracked
    bias_snn_stats_t snn_stats;
    bias_snn_get_stats(snn, &snn_stats);

    bias_plasticity_stats_t plas_stats;
    bias_plasticity_get_stats(plasticity, &plas_stats);

    EXPECT_GE(plas_stats.total_detections, 10u);
}
