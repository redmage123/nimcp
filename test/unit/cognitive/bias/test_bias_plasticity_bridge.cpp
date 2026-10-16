/**
 * @file test_bias_plasticity_bridge.cpp
 * @brief Unit tests for Cognitive Bias-Plasticity Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

#include "cognitive/bias/nimcp_bias_plasticity_bridge.h"

class BiasPlasticityBridgeTest : public ::testing::Test {
protected:
    bias_plasticity_bridge_t* bridge = nullptr;

    void SetUp() override {
        bias_plasticity_config_t config = bias_plasticity_config_default();
        bridge = bias_plasticity_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            bias_plasticity_destroy(bridge);
            bridge = nullptr;
        }
    }
};

// ============================================================================
// Lifecycle Tests
// ============================================================================

TEST_F(BiasPlasticityBridgeTest, CreateWithNullConfig) {
    bias_plasticity_bridge_t* b = bias_plasticity_create(nullptr);
    EXPECT_EQ(b, nullptr);
}

TEST_F(BiasPlasticityBridgeTest, DefaultConfigValid) {
    bias_plasticity_config_t config = bias_plasticity_config_default();
    EXPECT_GT(config.stdp_ltp_window_ms, 0.0f);
    EXPECT_GT(config.stdp_a_plus, 0.0f);
    EXPECT_GT(config.stdp_a_minus, 0.0f);
    EXPECT_TRUE(config.enable_detection_learning);
}

TEST_F(BiasPlasticityBridgeTest, ResetSucceeds) {
    int result = bias_plasticity_reset(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(BiasPlasticityBridgeTest, ResetNullBridge) {
    int result = bias_plasticity_reset(nullptr);
    EXPECT_EQ(result, -1);
}

// ============================================================================
// Synapse Management Tests
// ============================================================================

TEST_F(BiasPlasticityBridgeTest, RegisterSynapse) {
    int result = bias_plasticity_register_synapse(
        bridge, 1, BIAS_SYNAPSE_DETECTION, 0, 0.5f);
    EXPECT_EQ(result, 0);
}

TEST_F(BiasPlasticityBridgeTest, RegisterSynapseDuplicate) {
    bias_plasticity_register_synapse(bridge, 1, BIAS_SYNAPSE_DETECTION, 0, 0.5f);
    int result = bias_plasticity_register_synapse(
        bridge, 1, BIAS_SYNAPSE_CONFLICT_MONITOR, 1, 0.6f);
    EXPECT_EQ(result, -1);
}

TEST_F(BiasPlasticityBridgeTest, UnregisterSynapse) {
    bias_plasticity_register_synapse(bridge, 1, BIAS_SYNAPSE_DETECTION, 0, 0.5f);
    int result = bias_plasticity_unregister_synapse(bridge, 1);
    EXPECT_EQ(result, 0);
}

TEST_F(BiasPlasticityBridgeTest, GetSynapse) {
    bias_plasticity_register_synapse(bridge, 1, BIAS_SYNAPSE_DETECTION, 0, 0.5f);

    bias_plasticity_synapse_t synapse;
    int result = bias_plasticity_get_synapse(bridge, 1, &synapse);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(synapse.synapse_id, 1u);
    EXPECT_EQ(synapse.type, BIAS_SYNAPSE_DETECTION);
}

// ============================================================================
// Event Recording Tests
// ============================================================================

TEST_F(BiasPlasticityBridgeTest, BiasDetected) {
    bias_plasticity_register_synapse(bridge, 1, BIAS_SYNAPSE_DETECTION, 0, 0.5f);

    int result = bias_plasticity_bias_detected(bridge, 0, 0.8f, 1000);
    EXPECT_EQ(result, 0);
}

TEST_F(BiasPlasticityBridgeTest, DetectionFeedbackCorrect) {
    bias_plasticity_register_synapse(bridge, 1, BIAS_SYNAPSE_DETECTION, 0, 0.5f);
    bias_plasticity_bias_detected(bridge, 0, 0.8f, 1000);

    int result = bias_plasticity_detection_feedback(bridge, 0, true, 2000);
    EXPECT_EQ(result, 0);
}

TEST_F(BiasPlasticityBridgeTest, DetectionFeedbackIncorrect) {
    bias_plasticity_register_synapse(bridge, 1, BIAS_SYNAPSE_DETECTION, 0, 0.5f);
    bias_plasticity_bias_detected(bridge, 0, 0.8f, 1000);

    int result = bias_plasticity_detection_feedback(bridge, 0, false, 2000);
    EXPECT_EQ(result, 0);
}

TEST_F(BiasPlasticityBridgeTest, ConflictResolved) {
    bias_plasticity_register_synapse(bridge, 1, BIAS_SYNAPSE_CONFLICT_MONITOR, 0, 0.5f);

    int result = bias_plasticity_conflict_resolved(bridge, 0.7f, true, 1000);
    EXPECT_EQ(result, 0);
}

TEST_F(BiasPlasticityBridgeTest, MetacognitiveInsight) {
    bias_plasticity_register_synapse(bridge, 1, BIAS_SYNAPSE_METACOGNITIVE, 0, 0.5f);

    int result = bias_plasticity_metacognitive_insight(bridge, 0, 0.6f, 1000);
    EXPECT_EQ(result, 0);
}

TEST_F(BiasPlasticityBridgeTest, Reward) {
    bias_plasticity_register_synapse(bridge, 1, BIAS_SYNAPSE_DETECTION, 0, 0.5f);
    bias_plasticity_bias_detected(bridge, 0, 0.8f, 1000);  // Create eligibility

    int result = bias_plasticity_reward(bridge, 1.0f, 2000);
    EXPECT_EQ(result, 0);
}

// ============================================================================
// Update Tests
// ============================================================================

TEST_F(BiasPlasticityBridgeTest, UpdateBasic) {
    int result = bias_plasticity_update(bridge, 10.0f);
    EXPECT_EQ(result, 0);
}

TEST_F(BiasPlasticityBridgeTest, UpdateDecaysEligibility) {
    bias_plasticity_register_synapse(bridge, 1, BIAS_SYNAPSE_DETECTION, 0, 0.5f);
    bias_plasticity_bias_detected(bridge, 0, 0.8f, 1000);

    bias_plasticity_synapse_t synapse_before;
    bias_plasticity_get_synapse(bridge, 1, &synapse_before);
    float elig_before = synapse_before.eligibility_trace;

    bias_plasticity_update(bridge, 100.0f);

    bias_plasticity_synapse_t synapse_after;
    bias_plasticity_get_synapse(bridge, 1, &synapse_after);

    EXPECT_LT(synapse_after.eligibility_trace, elig_before);
}

TEST_F(BiasPlasticityBridgeTest, Consolidate) {
    bias_plasticity_register_synapse(bridge, 1, BIAS_SYNAPSE_DETECTION, 0, 0.5f);

    // Create history of correct detections
    for (int i = 0; i < 15; i++) {
        bias_plasticity_bias_detected(bridge, 0, 0.8f, i * 1000);
        bias_plasticity_detection_feedback(bridge, 0, true, i * 1000 + 500);
    }

    int result = bias_plasticity_consolidate(bridge);
    EXPECT_EQ(result, 0);
}

// ============================================================================
// Query Tests
// ============================================================================

TEST_F(BiasPlasticityBridgeTest, GetDetectionSensitivity) {
    bias_plasticity_bias_detected(bridge, 0, 0.8f, 1000);
    bias_plasticity_detection_feedback(bridge, 0, true, 2000);

    float sensitivity = bias_plasticity_get_detection_sensitivity(bridge, 0);
    EXPECT_GE(sensitivity, 0.0f);
    EXPECT_LE(sensitivity, 1.0f);
}

TEST_F(BiasPlasticityBridgeTest, GetCorrectionEfficiency) {
    bias_plasticity_bias_detected(bridge, 0, 0.8f, 1000);
    bias_plasticity_detection_feedback(bridge, 0, true, 2000);

    float efficiency = bias_plasticity_get_correction_efficiency(bridge, 0);
    EXPECT_GE(efficiency, 0.0f);
    EXPECT_LE(efficiency, 1.0f);
}

TEST_F(BiasPlasticityBridgeTest, GetMetacognitiveAwareness) {
    bias_plasticity_metacognitive_insight(bridge, 0, 0.6f, 1000);

    float awareness = bias_plasticity_get_metacognitive_awareness(bridge, 0);
    EXPECT_GE(awareness, 0.0f);
}

TEST_F(BiasPlasticityBridgeTest, GetTypeLearning) {
    bias_plasticity_bias_detected(bridge, 0, 0.8f, 1000);
    bias_plasticity_detection_feedback(bridge, 0, true, 2000);
    bias_plasticity_detection_feedback(bridge, 0, false, 3000);

    bias_type_learning_t learning;
    int result = bias_plasticity_get_type_learning(bridge, 0, &learning);
    EXPECT_EQ(result, 0);
    EXPECT_GE(learning.total_encounters, 1u);
}

// ============================================================================
// State and Stats Tests
// ============================================================================

TEST_F(BiasPlasticityBridgeTest, GetState) {
    bias_plasticity_bridge_state_t state;
    int result = bias_plasticity_get_state(bridge, &state);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(state.state, BIAS_PLASTICITY_STATE_IDLE);
}

TEST_F(BiasPlasticityBridgeTest, GetStats) {
    bias_plasticity_stats_t stats;
    int result = bias_plasticity_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
}

TEST_F(BiasPlasticityBridgeTest, ResetStats) {
    bias_plasticity_bias_detected(bridge, 0, 0.8f, 1000);

    bias_plasticity_reset_stats(bridge);

    bias_plasticity_stats_t stats;
    bias_plasticity_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_detections, 0u);
}

// ============================================================================
// Callback Tests
// ============================================================================

static bool weight_callback_called = false;
static void test_weight_callback(uint32_t, uint32_t, float, float, bias_learn_event_t, void*) {
    weight_callback_called = true;
}

TEST_F(BiasPlasticityBridgeTest, SetWeightCallback) {
    weight_callback_called = false;
    int result = bias_plasticity_set_weight_callback(bridge, test_weight_callback, nullptr);
    EXPECT_EQ(result, 0);

    // Trigger callback
    bias_plasticity_register_synapse(bridge, 1, BIAS_SYNAPSE_DETECTION, 0, 0.5f);
    bias_plasticity_bias_detected(bridge, 0, 0.8f, 1000);
    bias_plasticity_detection_feedback(bridge, 0, true, 2000);

    EXPECT_TRUE(weight_callback_called);
}

static bool metacog_callback_called = false;
static void test_metacog_callback(uint32_t, float, float, void*) {
    metacog_callback_called = true;
}

TEST_F(BiasPlasticityBridgeTest, SetMetacognitiveCallback) {
    metacog_callback_called = false;
    int result = bias_plasticity_set_metacognitive_callback(bridge, test_metacog_callback, nullptr);
    EXPECT_EQ(result, 0);

    // Trigger callback
    bias_plasticity_metacognitive_insight(bridge, 0, 0.6f, 1000);

    EXPECT_TRUE(metacog_callback_called);
}

// ============================================================================
// Bio-Async Tests
// ============================================================================

TEST_F(BiasPlasticityBridgeTest, BioAsyncNotConnectedByDefault) {
    bool connected = bias_plasticity_is_bio_async_connected(bridge);
    EXPECT_FALSE(connected);
}

TEST_F(BiasPlasticityBridgeTest, BioAsyncConnectWithEnable) {
    bias_plasticity_config_t config = bias_plasticity_config_default();
    config.enable_bio_async = true;
    bias_plasticity_bridge_t* b = bias_plasticity_create(&config);
    ASSERT_NE(b, nullptr);

    int result = bias_plasticity_connect_bio_async(b);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(bias_plasticity_is_bio_async_connected(b));

    bias_plasticity_disconnect_bio_async(b);
    EXPECT_FALSE(bias_plasticity_is_bio_async_connected(b));

    bias_plasticity_destroy(b);
}

// ============================================================================
// Learning Scenarios
// ============================================================================

TEST_F(BiasPlasticityBridgeTest, DetectionImprovesWithFeedback) {
    bias_plasticity_register_synapse(bridge, 1, BIAS_SYNAPSE_DETECTION, 0, 0.5f);

    // Multiple correct detections
    for (int i = 0; i < 10; i++) {
        bias_plasticity_bias_detected(bridge, 0, 0.8f, i * 2000);
        bias_plasticity_detection_feedback(bridge, 0, true, i * 2000 + 1000);
        bias_plasticity_update(bridge, 10.0f);
    }

    float sensitivity = bias_plasticity_get_detection_sensitivity(bridge, 0);
    EXPECT_GT(sensitivity, 0.7f);
}

TEST_F(BiasPlasticityBridgeTest, MetacognitionGrowsWithInsights) {
    bias_plasticity_register_synapse(bridge, 1, BIAS_SYNAPSE_METACOGNITIVE, 0, 0.5f);

    // Multiple insights
    for (int i = 0; i < 5; i++) {
        bias_plasticity_metacognitive_insight(bridge, 0, 0.5f, i * 1000);
    }

    float awareness = bias_plasticity_get_metacognitive_awareness(bridge, 0);
    EXPECT_GT(awareness, 0.0f);
}

TEST_F(BiasPlasticityBridgeTest, RewardModulatedLearning) {
    bias_plasticity_register_synapse(bridge, 1, BIAS_SYNAPSE_DETECTION, 0, 0.5f);

    // Create eligibility and reward
    bias_plasticity_bias_detected(bridge, 0, 0.9f, 1000);
    bias_plasticity_reward(bridge, 1.0f, 2000);

    bias_plasticity_synapse_t synapse;
    bias_plasticity_get_synapse(bridge, 1, &synapse);

    EXPECT_GT(synapse.weight, 0.5f);  // Weight should increase with positive reward
}
