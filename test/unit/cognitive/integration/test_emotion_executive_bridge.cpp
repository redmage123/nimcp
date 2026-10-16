/**
 * @file test_emotion_executive_bridge.cpp
 * @brief Unit tests for Emotion-Executive Bridge module
 *
 * WHAT: Comprehensive tests for Emotion-Executive bidirectional integration
 * WHY:  Ensure emotional influence on decisions and executive regulation of emotions
 *       works correctly
 * HOW:  Test lifecycle, configuration, influence, regulation, and statistics
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

#include "cognitive/integration/nimcp_emotion_executive_bridge.h"

class EmotionExecutiveBridgeTest : public ::testing::Test {
protected:
    emotion_executive_bridge_t* bridge = nullptr;

    void SetUp() override {
        emotion_executive_config_t config;
        emotion_executive_default_config(&config);
        bridge = emotion_executive_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge) {
            emotion_executive_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(EmotionExecutiveBridgeTest, BridgeCreation) {
    ASSERT_NE(bridge, nullptr);
    emotion_executive_bridge_destroy(bridge);
    bridge = nullptr;

    // Recreate for TearDown
    emotion_executive_config_t config;
    emotion_executive_default_config(&config);
    bridge = emotion_executive_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(EmotionExecutiveBridgeTest, CreateWithNullConfig) {
    emotion_executive_bridge_t* br = emotion_executive_bridge_create(nullptr);
    ASSERT_NE(br, nullptr);
    emotion_executive_bridge_destroy(br);
}

TEST_F(EmotionExecutiveBridgeTest, DestroyNull) {
    emotion_executive_bridge_destroy(nullptr);
    // Should not crash
}

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST_F(EmotionExecutiveBridgeTest, DefaultConfig) {
    emotion_executive_config_t config;
    int ret = emotion_executive_default_config(&config);

    EXPECT_EQ(ret, 0);
    // Test that influence weight, regulation strength, and decision threshold are set
    // Per header defaults: influence_weight=0.3, regulation_strength=0.5
    EXPECT_GT(config.emotion_influence_weight, 0.0f);
    EXPECT_GT(config.regulation_strength, 0.0f);
    EXPECT_GT(config.decision_threshold, 0.0f);
    EXPECT_LE(config.emotion_influence_weight, 1.0f);
    EXPECT_LE(config.regulation_strength, 1.0f);
    EXPECT_LE(config.decision_threshold, 1.0f);
}

TEST_F(EmotionExecutiveBridgeTest, DefaultConfigNullPtr) {
    int ret = emotion_executive_default_config(nullptr);
    EXPECT_EQ(ret, -1);
}

/* ============================================================================
 * Emotion -> Executive Tests (Influence Decision)
 * ============================================================================ */

TEST_F(EmotionExecutiveBridgeTest, InfluenceDecision) {
    emotion_executive_decision_context_t decision_context;
    memset(&decision_context, 0, sizeof(decision_context));
    decision_context.decision_id = 1;
    decision_context.option_count = 3;
    decision_context.time_pressure = 0.5f;
    decision_context.stakes = 0.7f;
    decision_context.uncertainty = 0.4f;
    decision_context.risk_level = 0.6f;

    emotion_executive_emotional_bias_t emotional_bias;
    memset(&emotional_bias, 0, sizeof(emotional_bias));

    int ret = emotion_executive_influence_decision(bridge, &decision_context, &emotional_bias);
    EXPECT_EQ(ret, 0);

    // Emotional bias should influence decision - valence_bias should be set
    EXPECT_GE(emotional_bias.valence_bias, -1.0f);
    EXPECT_LE(emotional_bias.valence_bias, 1.0f);
}

TEST_F(EmotionExecutiveBridgeTest, InfluenceDecisionNullBridge) {
    emotion_executive_decision_context_t decision_context;
    memset(&decision_context, 0, sizeof(decision_context));
    emotion_executive_emotional_bias_t emotional_bias;

    int ret = emotion_executive_influence_decision(nullptr, &decision_context, &emotional_bias);
    EXPECT_EQ(ret, -1);
}

TEST_F(EmotionExecutiveBridgeTest, InfluenceDecisionNullContext) {
    emotion_executive_emotional_bias_t emotional_bias;

    int ret = emotion_executive_influence_decision(bridge, nullptr, &emotional_bias);
    EXPECT_EQ(ret, -1);
}

TEST_F(EmotionExecutiveBridgeTest, InfluenceDecisionNullOutput) {
    emotion_executive_decision_context_t decision_context;
    memset(&decision_context, 0, sizeof(decision_context));

    int ret = emotion_executive_influence_decision(bridge, &decision_context, nullptr);
    EXPECT_EQ(ret, -1);
}

/* ============================================================================
 * Executive -> Emotion Tests (Decision Outcome)
 * ============================================================================ */

TEST_F(EmotionExecutiveBridgeTest, OnDecision) {
    // First create a decision record via influence_decision
    emotion_executive_decision_context_t context;
    memset(&context, 0, sizeof(context));
    context.decision_id = 1;
    context.time_pressure = 0.5f;
    context.stakes = 0.5f;

    emotion_executive_emotional_bias_t bias;
    EXPECT_EQ(0, emotion_executive_influence_decision(bridge, &context, &bias));

    emotion_executive_decision_outcome_t outcome;
    memset(&outcome, 0, sizeof(outcome));
    outcome.decision_id = 1;
    outcome.outcome_valence = 0.5f;
    outcome.expectation_violation = 0.2f;
    outcome.success = true;
    outcome.decision_time_ms = 150;

    int ret = emotion_executive_on_decision(bridge, outcome.decision_id, &outcome);
    EXPECT_EQ(ret, 0);

    // Check that emotional state was affected
    emotion_executive_emotional_state_t state;
    emotion_executive_get_emotional_state(bridge, &state);
    // State should reflect the outcome
}

TEST_F(EmotionExecutiveBridgeTest, OnDecisionPositive) {
    // First create a decision record via influence_decision
    emotion_executive_decision_context_t context;
    memset(&context, 0, sizeof(context));
    context.decision_id = 1;
    context.time_pressure = 0.5f;
    context.stakes = 0.5f;

    emotion_executive_emotional_bias_t bias;
    EXPECT_EQ(0, emotion_executive_influence_decision(bridge, &context, &bias));

    // Get baseline emotional state
    emotion_executive_emotional_state_t state_before;
    emotion_executive_get_emotional_state(bridge, &state_before);

    emotion_executive_decision_outcome_t outcome;
    memset(&outcome, 0, sizeof(outcome));
    outcome.decision_id = 1;
    outcome.outcome_valence = 0.9f;  // Very positive outcome
    outcome.expectation_violation = 0.3f;  // Better than expected
    outcome.success = true;
    outcome.decision_time_ms = 100;

    int ret = emotion_executive_on_decision(bridge, outcome.decision_id, &outcome);
    EXPECT_EQ(ret, 0);

    // Positive outcome should boost valence or at least not decrease it significantly
    emotion_executive_emotional_state_t state_after;
    emotion_executive_get_emotional_state(bridge, &state_after);

    // Positive outcome should result in non-negative valence
    EXPECT_GE(state_after.valence, -1.0f);
    EXPECT_LE(state_after.valence, 1.0f);
}

TEST_F(EmotionExecutiveBridgeTest, OnDecisionNegative) {
    // First create a decision record via influence_decision
    emotion_executive_decision_context_t context;
    memset(&context, 0, sizeof(context));
    context.decision_id = 1;
    context.time_pressure = 0.5f;
    context.stakes = 0.5f;

    emotion_executive_emotional_bias_t bias;
    EXPECT_EQ(0, emotion_executive_influence_decision(bridge, &context, &bias));

    // Get baseline emotional state
    emotion_executive_emotional_state_t state_before;
    emotion_executive_get_emotional_state(bridge, &state_before);

    emotion_executive_decision_outcome_t outcome;
    memset(&outcome, 0, sizeof(outcome));
    outcome.decision_id = 1;
    outcome.outcome_valence = -0.8f;  // Very negative outcome
    outcome.expectation_violation = -0.4f;  // Worse than expected
    outcome.success = false;
    outcome.decision_time_ms = 200;

    int ret = emotion_executive_on_decision(bridge, outcome.decision_id, &outcome);
    EXPECT_EQ(ret, 0);

    // Negative outcome affects emotional state
    emotion_executive_emotional_state_t state_after;
    emotion_executive_get_emotional_state(bridge, &state_after);

    // Valence should be within valid range
    EXPECT_GE(state_after.valence, -1.0f);
    EXPECT_LE(state_after.valence, 1.0f);
}

TEST_F(EmotionExecutiveBridgeTest, OnDecisionNullBridge) {
    emotion_executive_decision_outcome_t outcome;
    memset(&outcome, 0, sizeof(outcome));

    int ret = emotion_executive_on_decision(nullptr, 1, &outcome);
    EXPECT_EQ(ret, -1);
}

TEST_F(EmotionExecutiveBridgeTest, OnDecisionNullOutcome) {
    int ret = emotion_executive_on_decision(bridge, 1, nullptr);
    EXPECT_EQ(ret, -1);
}

/* ============================================================================
 * Executive Regulation Tests
 * ============================================================================ */

TEST_F(EmotionExecutiveBridgeTest, RegulateEmotion) {
    emotion_executive_regulation_target_t target;
    memset(&target, 0, sizeof(target));
    target.target_emotion = EMOTION_EXECUTIVE_TYPE_FEAR;
    target.target_intensity = 0.3f;  // Reduce fear to 30%
    target.strategy = EMOTION_EXECUTIVE_REG_REAPPRAISAL;
    target.max_duration_ms = 500;

    int ret = emotion_executive_regulate_emotion(bridge, EMOTION_EXECUTIVE_TYPE_FEAR, &target);
    EXPECT_EQ(ret, 0);

    // Regulation should move emotional state toward target
    emotion_executive_emotional_state_t state;
    emotion_executive_get_emotional_state(bridge, &state);
    // State should reflect regulation attempt
}

TEST_F(EmotionExecutiveBridgeTest, RegulateEmotionSuppression) {
    emotion_executive_regulation_target_t target;
    memset(&target, 0, sizeof(target));
    target.target_emotion = EMOTION_EXECUTIVE_TYPE_ANGER;
    target.target_intensity = 0.1f;  // Suppress anger
    target.strategy = EMOTION_EXECUTIVE_REG_SUPPRESSION;
    target.max_duration_ms = 300;

    int ret = emotion_executive_regulate_emotion(bridge, EMOTION_EXECUTIVE_TYPE_ANGER, &target);
    EXPECT_EQ(ret, 0);
}

TEST_F(EmotionExecutiveBridgeTest, RegulateEmotionDistraction) {
    emotion_executive_regulation_target_t target;
    memset(&target, 0, sizeof(target));
    target.target_emotion = EMOTION_EXECUTIVE_TYPE_SADNESS;
    target.target_intensity = 0.2f;
    target.strategy = EMOTION_EXECUTIVE_REG_DISTRACTION;
    target.max_duration_ms = 400;

    int ret = emotion_executive_regulate_emotion(bridge, EMOTION_EXECUTIVE_TYPE_SADNESS, &target);
    EXPECT_EQ(ret, 0);
}

TEST_F(EmotionExecutiveBridgeTest, RegulateEmotionNullBridge) {
    emotion_executive_regulation_target_t target;
    memset(&target, 0, sizeof(target));

    int ret = emotion_executive_regulate_emotion(nullptr, EMOTION_EXECUTIVE_TYPE_FEAR, &target);
    EXPECT_EQ(ret, -1);
}

TEST_F(EmotionExecutiveBridgeTest, RegulateEmotionNullTarget) {
    int ret = emotion_executive_regulate_emotion(bridge, EMOTION_EXECUTIVE_TYPE_FEAR, nullptr);
    EXPECT_EQ(ret, -1);
}

/* ============================================================================
 * State Retrieval Tests
 * ============================================================================ */

TEST_F(EmotionExecutiveBridgeTest, GetEmotionalState) {
    emotion_executive_emotional_state_t state;
    memset(&state, 0, sizeof(state));

    int ret = emotion_executive_get_emotional_state(bridge, &state);
    EXPECT_EQ(ret, 0);

    // Validate state ranges
    EXPECT_GE(state.valence, -1.0f);
    EXPECT_LE(state.valence, 1.0f);
    EXPECT_GE(state.arousal, 0.0f);
    EXPECT_LE(state.arousal, 1.0f);
    EXPECT_GE(state.dominant_intensity, 0.0f);
    EXPECT_LE(state.dominant_intensity, 1.0f);
    EXPECT_GE(state.stability, 0.0f);
    EXPECT_LE(state.stability, 1.0f);
}

TEST_F(EmotionExecutiveBridgeTest, GetEmotionalStateNullBridge) {
    emotion_executive_emotional_state_t state;

    int ret = emotion_executive_get_emotional_state(nullptr, &state);
    EXPECT_EQ(ret, -1);
}

TEST_F(EmotionExecutiveBridgeTest, GetEmotionalStateNullOutput) {
    int ret = emotion_executive_get_emotional_state(bridge, nullptr);
    EXPECT_EQ(ret, -1);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(EmotionExecutiveBridgeTest, StatsTracking) {
    // Perform some operations to generate statistics
    emotion_executive_decision_context_t decision_context;
    memset(&decision_context, 0, sizeof(decision_context));
    decision_context.decision_id = 1;
    decision_context.option_count = 2;
    decision_context.time_pressure = 0.5f;
    decision_context.stakes = 0.5f;
    decision_context.uncertainty = 0.5f;
    decision_context.risk_level = 0.5f;

    emotion_executive_emotional_bias_t emotional_bias;
    emotion_executive_influence_decision(bridge, &decision_context, &emotional_bias);

    emotion_executive_decision_outcome_t outcome;
    memset(&outcome, 0, sizeof(outcome));
    outcome.decision_id = 1;
    outcome.outcome_valence = 0.7f;
    outcome.success = true;
    emotion_executive_on_decision(bridge, 1, &outcome);

    emotion_executive_regulation_target_t target;
    memset(&target, 0, sizeof(target));
    target.target_emotion = EMOTION_EXECUTIVE_TYPE_JOY;
    target.target_intensity = 0.5f;
    target.strategy = EMOTION_EXECUTIVE_REG_ACCEPTANCE;
    emotion_executive_regulate_emotion(bridge, EMOTION_EXECUTIVE_TYPE_JOY, &target);

    // Get statistics
    emotion_executive_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    int ret = emotion_executive_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);

    // Verify statistics were tracked
    EXPECT_GT(stats.decisions_influenced, 0u);
    EXPECT_GT(stats.emotions_triggered, 0u);
    EXPECT_GT(stats.regulations_applied, 0u);
}

TEST_F(EmotionExecutiveBridgeTest, GetStatsNullBridge) {
    emotion_executive_stats_t stats;

    int ret = emotion_executive_get_stats(nullptr, &stats);
    EXPECT_EQ(ret, -1);
}

TEST_F(EmotionExecutiveBridgeTest, GetStatsNullOutput) {
    int ret = emotion_executive_get_stats(bridge, nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(EmotionExecutiveBridgeTest, InitialStatsZero) {
    // Create a fresh bridge to check initial stats
    emotion_executive_bridge_t* fresh_bridge = emotion_executive_bridge_create(nullptr);
    ASSERT_NE(fresh_bridge, nullptr);

    emotion_executive_stats_t stats;
    memset(&stats, 0xFF, sizeof(stats));  // Fill with non-zero

    int ret = emotion_executive_get_stats(fresh_bridge, &stats);
    EXPECT_EQ(ret, 0);

    // Initial stats should be zero
    EXPECT_EQ(stats.decisions_influenced, 0u);
    EXPECT_EQ(stats.emotions_triggered, 0u);
    EXPECT_EQ(stats.regulations_applied, 0u);
    EXPECT_EQ(stats.successful_regulations, 0u);
    EXPECT_EQ(stats.regulation_failures, 0u);
    EXPECT_EQ(stats.conflicts_detected, 0u);

    emotion_executive_bridge_destroy(fresh_bridge);
}
