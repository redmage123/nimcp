/**
 * @file test_health_emotion_bridge_functions.cpp
 * @brief Unit tests for NIMCP Health Emotion Bridge functions
 * @version 1.0.0
 * @date 2025-01-18
 *
 * WHAT: Test health emotion bridge functions for threshold adjustment, event reporting
 * WHY:  Ensure emotion-aware health monitoring works correctly
 * HOW:  Test each function with valid inputs, edge cases, and error conditions
 *
 * Part of Phase 9 (Section 28) of the NIMCP Self-Contained Resilience System.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdlib>
#include <cmath>

extern "C" {
#include "cognitive/emotion/nimcp_health_emotion_bridge.h"
}

//=============================================================================
// Test Fixtures
//=============================================================================

/**
 * @brief Base fixture for health emotion bridge tests
 */
class HealthEmotionBridgeTest : public ::testing::Test {
protected:
    emotion_adjusted_thresholds_t thresholds;
    threshold_adjustment_factors_t factors;
    health_emotion_state_t emotion_state;
    health_event_emotion_mapping_t mapping;
    immune_emotion_health_state_t unified_state;

    void SetUp() override {
        memset(&thresholds, 0, sizeof(thresholds));
        memset(&factors, 0, sizeof(factors));
        memset(&emotion_state, 0, sizeof(emotion_state));
        memset(&mapping, 0, sizeof(mapping));
        memset(&unified_state, 0, sizeof(unified_state));

        health_emotion_default_factors(&factors);
    }

    void TearDown() override {
        // No cleanup needed
    }
};

//=============================================================================
// Default Factors Tests
//=============================================================================

TEST_F(HealthEmotionBridgeTest, DefaultFactors_InitializesCorrectly) {
    threshold_adjustment_factors_t f;
    memset(&f, 0xFF, sizeof(f));  // Fill with garbage

    health_emotion_default_factors(&f);

    EXPECT_FLOAT_EQ(f.high_stress_modifier, 0.8f);
    EXPECT_FLOAT_EQ(f.negative_valence_modifier, 1.2f);
    EXPECT_FLOAT_EQ(f.positive_valence_modifier, 0.9f);
    EXPECT_FLOAT_EQ(f.instability_modifier, 1.3f);
    EXPECT_FLOAT_EQ(f.inflammation_modifier, 1.25f);
}

TEST_F(HealthEmotionBridgeTest, DefaultFactors_HandlesNullPointer) {
    // Should not crash
    health_emotion_default_factors(nullptr);
}

//=============================================================================
// Threshold Computation Tests
//=============================================================================

TEST_F(HealthEmotionBridgeTest, ComputeThresholds_SetsDefaults) {
    int result = health_emotion_compute_thresholds(nullptr, nullptr, nullptr, &thresholds);

    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(thresholds.memory_warning_threshold, 0.75f);
    EXPECT_FLOAT_EQ(thresholds.memory_critical_threshold, 0.90f);
    EXPECT_FLOAT_EQ(thresholds.cpu_warning_threshold, 0.80f);
    EXPECT_FLOAT_EQ(thresholds.cpu_critical_threshold, 0.95f);
    EXPECT_FLOAT_EQ(thresholds.anomaly_sensitivity, 1.0f);
    EXPECT_FLOAT_EQ(thresholds.recovery_aggressiveness, 0.5f);
}

TEST_F(HealthEmotionBridgeTest, ComputeThresholds_HandlesNullOutput) {
    int result = health_emotion_compute_thresholds(nullptr, nullptr, nullptr, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(HealthEmotionBridgeTest, ComputeThresholds_ClampsToReasonableRange) {
    // Even with extreme factors, thresholds should be clamped
    threshold_adjustment_factors_t extreme;
    extreme.high_stress_modifier = 0.1f;  // Would make thresholds too low
    extreme.negative_valence_modifier = 10.0f;  // Would make sensitivity too high
    extreme.positive_valence_modifier = 0.1f;
    extreme.instability_modifier = 10.0f;
    extreme.inflammation_modifier = 10.0f;

    health_emotion_compute_thresholds(nullptr, nullptr, &extreme, &thresholds);

    // Verify clamping
    EXPECT_GE(thresholds.memory_warning_threshold, 0.5f);
    EXPECT_LE(thresholds.memory_warning_threshold, 0.9f);
    EXPECT_GE(thresholds.anomaly_sensitivity, 0.5f);
    EXPECT_LE(thresholds.anomaly_sensitivity, 2.0f);
    EXPECT_GE(thresholds.recovery_aggressiveness, 0.2f);
    EXPECT_LE(thresholds.recovery_aggressiveness, 1.0f);
}

//=============================================================================
// Emotional State Query Tests
//=============================================================================

TEST_F(HealthEmotionBridgeTest, GetState_ReturnsDefaultsWithoutEmotionSystem) {
    int result = health_emotion_get_state(nullptr, &emotion_state);

    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(emotion_state.valence, 0.0f);
    EXPECT_GT(emotion_state.arousal, 0.0f);
    EXPECT_GT(emotion_state.stability, 0.0f);
    EXPECT_TRUE(emotion_state.is_calm);
}

TEST_F(HealthEmotionBridgeTest, GetState_HandlesNullOutput) {
    int result = health_emotion_get_state(nullptr, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(HealthEmotionBridgeTest, GetState_ComputesStressIndex) {
    health_emotion_get_state(nullptr, &emotion_state);

    // Stress index should be computed from arousal and valence
    EXPECT_GE(emotion_state.stress_index, 0.0f);
    EXPECT_LE(emotion_state.stress_index, 1.0f);
}

//=============================================================================
// Event Mapping Tests
//=============================================================================

TEST_F(HealthEmotionBridgeTest, GetEventMapping_MinorAnomaly) {
    health_emotion_get_event_mapping(HEALTH_EMOTION_EVENT_MINOR_ANOMALY, 1.0f, &mapping);

    EXPECT_LT(mapping.valence_delta, 0.0f);  // Negative
    EXPECT_GT(mapping.arousal_delta, 0.0f);  // Increases
    EXPECT_GT(mapping.duration_ms, 0u);
    EXPECT_FALSE(mapping.triggers_fear);
}

TEST_F(HealthEmotionBridgeTest, GetEventMapping_CriticalAnomaly) {
    health_emotion_get_event_mapping(HEALTH_EMOTION_EVENT_CRITICAL_ANOMALY, 1.0f, &mapping);

    EXPECT_LT(mapping.valence_delta, -0.2f);  // Very negative
    EXPECT_GT(mapping.arousal_delta, 0.3f);   // High arousal
    EXPECT_TRUE(mapping.triggers_fear);
    EXPECT_TRUE(mapping.triggers_stress);
}

TEST_F(HealthEmotionBridgeTest, GetEventMapping_RecoverySuccess) {
    health_emotion_get_event_mapping(HEALTH_EMOTION_EVENT_RECOVERY_SUCCESS, 1.0f, &mapping);

    EXPECT_GT(mapping.valence_delta, 0.0f);   // Positive
    EXPECT_LT(mapping.arousal_delta, 0.0f);   // Decreases (calming)
    EXPECT_TRUE(mapping.triggers_relief);
    EXPECT_FALSE(mapping.triggers_stress);
}

TEST_F(HealthEmotionBridgeTest, GetEventMapping_ScalesBySeverity) {
    health_event_emotion_mapping_t low_severity, high_severity;

    health_emotion_get_event_mapping(HEALTH_EMOTION_EVENT_MODERATE_ANOMALY, 0.2f, &low_severity);
    health_emotion_get_event_mapping(HEALTH_EMOTION_EVENT_MODERATE_ANOMALY, 1.0f, &high_severity);

    EXPECT_LT(fabs(low_severity.valence_delta), fabs(high_severity.valence_delta));
    EXPECT_LT(low_severity.arousal_delta, high_severity.arousal_delta);
}

TEST_F(HealthEmotionBridgeTest, GetEventMapping_HandlesNullOutput) {
    // Should not crash
    health_emotion_get_event_mapping(HEALTH_EMOTION_EVENT_MINOR_ANOMALY, 1.0f, nullptr);
}

TEST_F(HealthEmotionBridgeTest, GetEventMapping_HandlesInvalidEventType) {
    health_emotion_get_event_mapping((health_emotion_event_type_t)999, 1.0f, &mapping);

    // Should default to minor anomaly
    EXPECT_LE(fabs(mapping.valence_delta), 0.1f);
}

//=============================================================================
// Event Reporting Tests
//=============================================================================

TEST_F(HealthEmotionBridgeTest, ReportEvent_ReturnsErrorWithoutEmotionSystem) {
    int result = health_emotion_report_event(nullptr, nullptr,
                                             HEALTH_EMOTION_EVENT_MINOR_ANOMALY, 0.5f);
    EXPECT_EQ(result, -1);
}

TEST_F(HealthEmotionBridgeTest, ReportEvent_HandlesInvalidEventType) {
    int result = health_emotion_report_event(nullptr, nullptr,
                                             (health_emotion_event_type_t)999, 0.5f);
    EXPECT_EQ(result, -1);
}

//=============================================================================
// Action Permission Tests
//=============================================================================

TEST_F(HealthEmotionBridgeTest, PermitsAction_AllowsLowSeverityWithoutEmotionSystem) {
    bool permits = health_emotion_permits_action(nullptr, HEALTH_RECOVERY_LOG_ONLY);
    EXPECT_TRUE(permits);
}

TEST_F(HealthEmotionBridgeTest, PermitsAction_AllowsHighSeverityWithoutEmotionSystem) {
    bool permits = health_emotion_permits_action(nullptr, HEALTH_RECOVERY_EMERGENCY_SHUTDOWN);
    EXPECT_TRUE(permits);
}

//=============================================================================
// Action Adjustment Tests
//=============================================================================

TEST_F(HealthEmotionBridgeTest, AdjustRecovery_NoChangeWithoutEmotionSystem) {
    health_recovery_action_t adjusted;
    int result = health_emotion_adjust_recovery(nullptr, HEALTH_RECOVERY_FULL_RESTART, &adjusted);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(adjusted, HEALTH_RECOVERY_FULL_RESTART);
}

TEST_F(HealthEmotionBridgeTest, AdjustRecovery_HandlesNullOutput) {
    int result = health_emotion_adjust_recovery(nullptr, HEALTH_RECOVERY_FULL_RESTART, nullptr);
    EXPECT_EQ(result, -1);
}

//=============================================================================
// Shadow Pattern Detection Tests
//=============================================================================

TEST_F(HealthEmotionBridgeTest, DetectShadowPatterns_HandlesNullPointers) {
    shadow_detection_result_t patterns[4];
    uint32_t num_detected;

    EXPECT_EQ(health_agent_detect_shadow_patterns(nullptr, nullptr, 4, &num_detected), -1);
    EXPECT_EQ(health_agent_detect_shadow_patterns(nullptr, patterns, 0, &num_detected), -1);
    EXPECT_EQ(health_agent_detect_shadow_patterns(nullptr, patterns, 4, nullptr), -1);
}

TEST_F(HealthEmotionBridgeTest, DetectShadowPatterns_ReturnsEmptyWithoutAgent) {
    shadow_detection_result_t patterns[4];
    uint32_t num_detected = 999;

    int result = health_agent_detect_shadow_patterns(nullptr, patterns, 4, &num_detected);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(num_detected, 0u);
}

TEST_F(HealthEmotionBridgeTest, GetShadowIntervention_CorrectForHypervigilance) {
    shadow_intervention_type_t intervention =
        health_shadow_get_intervention(HEALTH_SHADOW_HYPERVIGILANCE);
    EXPECT_EQ(intervention, SHADOW_INTERVENTION_RAISE_THRESHOLD);
}

TEST_F(HealthEmotionBridgeTest, GetShadowIntervention_CorrectForDenial) {
    shadow_intervention_type_t intervention =
        health_shadow_get_intervention(HEALTH_SHADOW_DENIAL);
    EXPECT_EQ(intervention, SHADOW_INTERVENTION_LOWER_THRESHOLD);
}

TEST_F(HealthEmotionBridgeTest, GetShadowIntervention_CorrectForDecisionParalysis) {
    shadow_intervention_type_t intervention =
        health_shadow_get_intervention(HEALTH_SHADOW_DECISION_PARALYSIS);
    EXPECT_EQ(intervention, SHADOW_INTERVENTION_FORCE_DEFAULT);
}

TEST_F(HealthEmotionBridgeTest, InterveneShadow_HandlesNullAgent) {
    int result = health_agent_intervene_shadow(nullptr, HEALTH_SHADOW_HYPERVIGILANCE);
    EXPECT_EQ(result, -1);
}

TEST_F(HealthEmotionBridgeTest, InterveneShadow_HandlesNonePattern) {
    int result = health_agent_intervene_shadow(nullptr, HEALTH_SHADOW_NONE);
    // Returns 0 for NONE pattern (no intervention needed)
    // But also -1 because agent is null - check the precedence
    EXPECT_EQ(result, -1);  // Null check comes first
}

//=============================================================================
// Unified State Tests
//=============================================================================

TEST_F(HealthEmotionBridgeTest, UpdateUnifiedState_SetsDefaults) {
    int result = health_emotion_update_unified_state(nullptr, nullptr, nullptr, &unified_state);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(unified_state.inflammation_level, INFLAMMATION_NONE);
    EXPECT_FLOAT_EQ(unified_state.immune_health_score, 1.0f);
    EXPECT_EQ(unified_state.active_threats, 0u);
    EXPECT_FLOAT_EQ(unified_state.overall_health_score, 1.0f);
}

TEST_F(HealthEmotionBridgeTest, UpdateUnifiedState_HandlesNullOutput) {
    int result = health_emotion_update_unified_state(nullptr, nullptr, nullptr, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(HealthEmotionBridgeTest, UpdateUnifiedState_ComputesDerivedMetrics) {
    health_emotion_update_unified_state(nullptr, nullptr, nullptr, &unified_state);

    EXPECT_GE(unified_state.combined_stress_index, 0.0f);
    EXPECT_LE(unified_state.combined_stress_index, 1.0f);
    EXPECT_GE(unified_state.system_resilience, 0.0f);
    EXPECT_GE(unified_state.recovery_capacity, 0.1f);
}

//=============================================================================
// Combined Stress Tests
//=============================================================================

TEST_F(HealthEmotionBridgeTest, ComputeCombinedStress_LowForHealthyState) {
    unified_state.inflammation_level = INFLAMMATION_NONE;
    unified_state.arousal = 0.2f;
    unified_state.active_anomalies = 0;
    unified_state.agent_stress = 0.1f;
    unified_state.valence = 0.5f;

    float stress = health_emotion_compute_combined_stress(&unified_state);

    EXPECT_LT(stress, 0.3f);
}

TEST_F(HealthEmotionBridgeTest, ComputeCombinedStress_HighForStressedState) {
    unified_state.inflammation_level = INFLAMMATION_HIGH;
    unified_state.arousal = 0.9f;
    unified_state.active_anomalies = 8;
    unified_state.agent_stress = 0.8f;
    unified_state.valence = -0.7f;

    float stress = health_emotion_compute_combined_stress(&unified_state);

    EXPECT_GT(stress, 0.7f);
}

TEST_F(HealthEmotionBridgeTest, ComputeCombinedStress_ClampedToValidRange) {
    // Extreme values
    unified_state.inflammation_level = INFLAMMATION_SEVERE;
    unified_state.arousal = 1.0f;
    unified_state.active_anomalies = 100;
    unified_state.agent_stress = 1.0f;
    unified_state.valence = -1.0f;

    float stress = health_emotion_compute_combined_stress(&unified_state);

    EXPECT_GE(stress, 0.0f);
    EXPECT_LE(stress, 1.0f);
}

TEST_F(HealthEmotionBridgeTest, ComputeCombinedStress_HandlesNullState) {
    float stress = health_emotion_compute_combined_stress(nullptr);
    EXPECT_FLOAT_EQ(stress, 0.5f);  // Returns default
}

//=============================================================================
// Holistic Recommendation Tests
//=============================================================================

TEST_F(HealthEmotionBridgeTest, HolisticRecommendation_NoChangeForHealthyState) {
    unified_state.combined_stress_index = 0.2f;
    unified_state.system_resilience = 0.8f;
    unified_state.recovery_capacity = 0.9f;
    unified_state.inflammation_level = INFLAMMATION_NONE;
    unified_state.agent_confidence = 0.9f;

    health_recovery_action_t adjusted;
    int result = health_emotion_get_holistic_recommendation(
        &unified_state, HEALTH_RECOVERY_FULL_RESTART, &adjusted);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(adjusted, HEALTH_RECOVERY_FULL_RESTART);
}

TEST_F(HealthEmotionBridgeTest, HolisticRecommendation_DowngradesOnHighStress) {
    unified_state.combined_stress_index = 0.85f;
    unified_state.system_resilience = 0.5f;
    unified_state.recovery_capacity = 0.5f;

    health_recovery_action_t adjusted;
    health_emotion_get_holistic_recommendation(
        &unified_state, HEALTH_RECOVERY_FULL_RESTART, &adjusted);

    EXPECT_LT(adjusted, HEALTH_RECOVERY_FULL_RESTART);
}

TEST_F(HealthEmotionBridgeTest, HolisticRecommendation_DowngradesOnLowResilience) {
    unified_state.combined_stress_index = 0.5f;
    unified_state.system_resilience = 0.2f;
    unified_state.recovery_capacity = 0.5f;

    health_recovery_action_t adjusted;
    health_emotion_get_holistic_recommendation(
        &unified_state, HEALTH_RECOVERY_EMERGENCY_SHUTDOWN, &adjusted);

    EXPECT_LT(adjusted, HEALTH_RECOVERY_EMERGENCY_SHUTDOWN);
}

TEST_F(HealthEmotionBridgeTest, HolisticRecommendation_DowngradesOnLowConfidence) {
    unified_state.combined_stress_index = 0.3f;
    unified_state.system_resilience = 0.7f;
    unified_state.recovery_capacity = 0.7f;
    unified_state.agent_confidence = 0.2f;

    health_recovery_action_t adjusted;
    health_emotion_get_holistic_recommendation(
        &unified_state, HEALTH_RECOVERY_QUARANTINE, &adjusted);

    EXPECT_LE(adjusted, HEALTH_RECOVERY_CLEAR_CACHE);
}

TEST_F(HealthEmotionBridgeTest, HolisticRecommendation_HandlesNullOutput) {
    int result = health_emotion_get_holistic_recommendation(
        &unified_state, HEALTH_RECOVERY_FULL_RESTART, nullptr);
    EXPECT_EQ(result, -1);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(HealthEmotionBridgeTest, GetStats_HandlesNullPointers) {
    health_emotion_stats_t stats;
    EXPECT_EQ(health_emotion_get_stats(nullptr, nullptr), -1);
    EXPECT_EQ(health_emotion_get_stats(nullptr, &stats), 0);  // Returns defaults
}

TEST_F(HealthEmotionBridgeTest, ResetStats_HandlesNullAgent) {
    // Should not crash
    health_emotion_reset_stats(nullptr);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(HealthEmotionBridgeTest, EventName_ReturnsReadableStrings) {
    EXPECT_STREQ(health_emotion_event_name(HEALTH_EMOTION_EVENT_MINOR_ANOMALY), "Minor Anomaly");
    EXPECT_STREQ(health_emotion_event_name(HEALTH_EMOTION_EVENT_CRITICAL_ANOMALY), "Critical Anomaly");
    EXPECT_STREQ(health_emotion_event_name(HEALTH_EMOTION_EVENT_RECOVERY_SUCCESS), "Recovery Success");
    EXPECT_STREQ(health_emotion_event_name(HEALTH_EMOTION_EVENT_SYSTEM_STABLE), "System Stable");
}

TEST_F(HealthEmotionBridgeTest, EventName_HandlesInvalidType) {
    const char* name = health_emotion_event_name((health_emotion_event_type_t)999);
    EXPECT_STREQ(name, "Unknown");
}

TEST_F(HealthEmotionBridgeTest, RecoveryActionName_ReturnsReadableStrings) {
    EXPECT_STREQ(health_emotion_recovery_action_name(HEALTH_RECOVERY_NONE), "None");
    EXPECT_STREQ(health_emotion_recovery_action_name(HEALTH_RECOVERY_QUARANTINE), "Quarantine");
    EXPECT_STREQ(health_emotion_recovery_action_name(HEALTH_RECOVERY_EMERGENCY_SHUTDOWN), "Emergency Shutdown");
}

TEST_F(HealthEmotionBridgeTest, RecoveryActionName_HandlesInvalidAction) {
    const char* name = health_emotion_recovery_action_name((health_recovery_action_t)999);
    EXPECT_STREQ(name, "Unknown");
}

TEST_F(HealthEmotionBridgeTest, ShadowPatternName_ReturnsReadableStrings) {
    EXPECT_STREQ(health_shadow_pattern_name(HEALTH_SHADOW_NONE), "None");
    EXPECT_STREQ(health_shadow_pattern_name(HEALTH_SHADOW_HYPERVIGILANCE), "Hypervigilance");
    EXPECT_STREQ(health_shadow_pattern_name(HEALTH_SHADOW_DENIAL), "Denial");
    EXPECT_STREQ(health_shadow_pattern_name(HEALTH_SHADOW_DECISION_PARALYSIS), "Decision Paralysis");
}

TEST_F(HealthEmotionBridgeTest, ShadowPatternName_HandlesInvalidPattern) {
    const char* name = health_shadow_pattern_name((health_shadow_pattern_t)999);
    EXPECT_STREQ(name, "Unknown");
}

TEST_F(HealthEmotionBridgeTest, InflammationLevelName_ReturnsReadableStrings) {
    EXPECT_STREQ(health_inflammation_level_name(INFLAMMATION_NONE), "None");
    EXPECT_STREQ(health_inflammation_level_name(INFLAMMATION_LOW), "Low");
    EXPECT_STREQ(health_inflammation_level_name(INFLAMMATION_HIGH), "High");
    EXPECT_STREQ(health_inflammation_level_name(INFLAMMATION_SEVERE), "Severe");
}

TEST_F(HealthEmotionBridgeTest, InflammationLevelName_HandlesInvalidLevel) {
    const char* name = health_inflammation_level_name((brain_inflammation_level_t)999);
    EXPECT_STREQ(name, "Unknown");
}
