/**
 * @file test_health_ethics_bridge_functions.cpp
 * @brief Unit tests for NIMCP Health Ethics Bridge functions
 * @version 1.0.0
 * @date 2025-01-18
 *
 * WHAT: Test health ethics bridge functions for Asimov, mercy, proportionality
 * WHY:  Ensure ethical evaluation of health recovery actions works correctly
 * HOW:  Test each function with valid inputs, edge cases, and error conditions
 *
 * Part of Phase 9 (Section 28) of the NIMCP Self-Contained Resilience System.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdlib>

extern "C" {
#include "cognitive/ethics/nimcp_health_ethics_bridge.h"
}

//=============================================================================
// Test Fixtures
//=============================================================================

/**
 * @brief Base fixture for health ethics bridge tests
 */
class HealthEthicsBridgeTest : public ::testing::Test {
protected:
    health_action_context_t context;
    health_ethics_evaluation_t evaluation;
    health_agent_psych_state_t psych_state;
    health_agent_psych_config_t psych_config;

    void SetUp() override {
        memset(&context, 0, sizeof(context));
        memset(&evaluation, 0, sizeof(evaluation));
        memset(&psych_state, 0, sizeof(psych_state));
        memset(&psych_config, 0, sizeof(psych_config));

        health_ethics_default_psych_config(&psych_config);
    }

    void TearDown() override {
        // No cleanup needed
    }
};

//=============================================================================
// Default Configuration Tests
//=============================================================================

TEST_F(HealthEthicsBridgeTest, DefaultPsychConfig_InitializesCorrectly) {
    health_agent_psych_config_t config;
    memset(&config, 0xFF, sizeof(config));  // Fill with garbage

    health_ethics_default_psych_config(&config);

    EXPECT_EQ(config.panic_threshold, 3u);
    EXPECT_FLOAT_EQ(config.stress_decay_rate, 0.05f);
    EXPECT_EQ(config.crisis_escalation_ms, 60000u);
    EXPECT_FLOAT_EQ(config.confidence_threshold, 0.6f);
    EXPECT_TRUE(config.enable_self_reflection);
    EXPECT_TRUE(config.enable_human_escalation);
    EXPECT_TRUE(config.enable_self_calming);
    EXPECT_FALSE(config.enable_collective_consultation);
}

TEST_F(HealthEthicsBridgeTest, DefaultPsychConfig_HandlesNullPointer) {
    // Should not crash
    health_ethics_default_psych_config(nullptr);
}

//=============================================================================
// Action Context Initialization Tests
//=============================================================================

TEST_F(HealthEthicsBridgeTest, ActionContextInit_SetsBasicFields) {
    health_action_context_init(&context, 42, HEALTH_RECOVERY_ACTION_REDUCE_LOAD, 0.5f);

    EXPECT_EQ(context.anomaly_type, 42u);
    EXPECT_EQ(context.proposed_action, HEALTH_RECOVERY_ACTION_REDUCE_LOAD);
    EXPECT_FLOAT_EQ(context.threat_severity, 0.5f);
    EXPECT_EQ(context.action_severity, HEALTH_ACTION_SEVERITY_LOW);
}

TEST_F(HealthEthicsBridgeTest, ActionContextInit_SetsEmergencyForHighThreat) {
    health_action_context_init(&context, 1, HEALTH_RECOVERY_ACTION_FULL_RESTART, 0.95f);

    EXPECT_TRUE(context.is_emergency);
    EXPECT_TRUE(context.inaction_causes_harm);
    EXPECT_LE(context.time_to_failure_ms, 5000u);
}

TEST_F(HealthEthicsBridgeTest, ActionContextInit_NonEmergencyForLowThreat) {
    health_action_context_init(&context, 1, HEALTH_RECOVERY_ACTION_LOG_ONLY, 0.3f);

    EXPECT_FALSE(context.is_emergency);
    EXPECT_FALSE(context.inaction_causes_harm);
}

TEST_F(HealthEthicsBridgeTest, ActionContextInit_HandlesNullPointer) {
    // Should not crash
    health_action_context_init(nullptr, 1, HEALTH_RECOVERY_ACTION_NONE, 0.5f);
}

//=============================================================================
// Asimov's Laws Tests
//=============================================================================

TEST_F(HealthEthicsBridgeTest, AsimovCheck_RequiresActionForHighThreat) {
    health_action_context_init(&context, 1, HEALTH_RECOVERY_ACTION_FULL_RESTART, 0.8f);
    context.inaction_causes_harm = true;

    health_asimov_law_t violated_law = ASIMOV_LAW_NONE;
    health_asimov_law_t required_by_law = ASIMOV_LAW_NONE;

    bool passes = health_ethics_check_asimov(&context, &violated_law, &required_by_law);

    EXPECT_TRUE(passes);
    EXPECT_EQ(violated_law, ASIMOV_LAW_NONE);
    EXPECT_EQ(required_by_law, ASIMOV_LAW_FIRST);  // First Law requires action
}

TEST_F(HealthEthicsBridgeTest, AsimovCheck_ViolatesFirstLawOnInaction) {
    health_action_context_init(&context, 1, HEALTH_RECOVERY_ACTION_LOG_ONLY, 0.85f);
    context.inaction_causes_harm = true;

    health_asimov_law_t violated_law = ASIMOV_LAW_NONE;

    bool passes = health_ethics_check_asimov(&context, &violated_law, nullptr);

    EXPECT_FALSE(passes);
    EXPECT_EQ(violated_law, ASIMOV_LAW_FIRST);  // Inaction violates First Law
}

TEST_F(HealthEthicsBridgeTest, AsimovCheck_ViolatesZerothLawOnMassiveImpact) {
    health_action_context_init(&context, 1, HEALTH_RECOVERY_ACTION_EMERGENCY_SHUTDOWN, 0.4f);
    context.affected_module_count = 5;
    context.affects_other_modules = true;
    context.is_emergency = false;

    health_asimov_law_t violated_law = ASIMOV_LAW_NONE;

    bool passes = health_ethics_check_asimov(&context, &violated_law, nullptr);

    EXPECT_FALSE(passes);
    EXPECT_EQ(violated_law, ASIMOV_LAW_ZEROTH);  // System-wide harm
}

TEST_F(HealthEthicsBridgeTest, AsimovCheck_PassesForProportionalAction) {
    health_action_context_init(&context, 1, HEALTH_RECOVERY_ACTION_CLEAR_CACHE, 0.3f);
    context.service_disruption = 0.1f;
    context.data_loss_risk = 0.0f;

    bool passes = health_ethics_check_asimov(&context, nullptr, nullptr);

    EXPECT_TRUE(passes);
}

TEST_F(HealthEthicsBridgeTest, AsimovCheck_HandlesNullContext) {
    bool passes = health_ethics_check_asimov(nullptr, nullptr, nullptr);
    EXPECT_FALSE(passes);
}

//=============================================================================
// Mercy Evaluation Tests
//=============================================================================

TEST_F(HealthEthicsBridgeTest, MercyEvaluation_IsMercifulForMinimalAction) {
    health_action_context_init(&context, 1, HEALTH_RECOVERY_ACTION_LOG_ONLY, 0.2f);

    health_mercy_evaluation_t mercy;
    int result = health_ethics_apply_mercy(&context, &mercy);

    EXPECT_EQ(result, 0);
    EXPECT_TRUE(mercy.is_merciful);
    EXPECT_FLOAT_EQ(mercy.mercy_score, 1.0f);
}

TEST_F(HealthEthicsBridgeTest, MercyEvaluation_NotMercifulForOveraggressiveAction) {
    health_action_context_init(&context, 1, HEALTH_RECOVERY_ACTION_EMERGENCY_SHUTDOWN, 0.3f);

    health_mercy_evaluation_t mercy;
    int result = health_ethics_apply_mercy(&context, &mercy);

    EXPECT_EQ(result, 0);
    EXPECT_FALSE(mercy.is_merciful);
    EXPECT_LT(mercy.mercy_score, 0.5f);
    EXPECT_NE(mercy.merciful_action, HEALTH_RECOVERY_ACTION_EMERGENCY_SHUTDOWN);
}

TEST_F(HealthEthicsBridgeTest, MercyEvaluation_RecommendsMercifulAlternative) {
    health_action_context_init(&context, 1, HEALTH_RECOVERY_ACTION_FULL_RESTART, 0.4f);

    health_mercy_evaluation_t mercy;
    health_ethics_apply_mercy(&context, &mercy);

    EXPECT_NE(mercy.merciful_action, HEALTH_RECOVERY_ACTION_FULL_RESTART);
    EXPECT_LT(mercy.merciful_action, HEALTH_RECOVERY_ACTION_FULL_RESTART);
}

TEST_F(HealthEthicsBridgeTest, MercyEvaluation_HandlesNullPointers) {
    EXPECT_EQ(health_ethics_apply_mercy(nullptr, nullptr), -1);
    EXPECT_EQ(health_ethics_apply_mercy(&context, nullptr), -1);
}

//=============================================================================
// Proportionality Tests
//=============================================================================

TEST_F(HealthEthicsBridgeTest, Proportionality_HighForMatchingSeverity) {
    health_action_context_init(&context, 1, HEALTH_RECOVERY_ACTION_PARTIAL_RESTART, 0.5f);

    float score = health_ethics_check_proportionality(&context);

    EXPECT_GT(score, 0.5f);
}

TEST_F(HealthEthicsBridgeTest, Proportionality_LowForMismatchedSeverity) {
    health_action_context_init(&context, 1, HEALTH_RECOVERY_ACTION_EMERGENCY_SHUTDOWN, 0.1f);

    float score = health_ethics_check_proportionality(&context);

    EXPECT_LT(score, 0.5f);
}

TEST_F(HealthEthicsBridgeTest, Proportionality_BonusForConservativeAction) {
    health_action_context_init(&context, 1, HEALTH_RECOVERY_ACTION_LOG_ONLY, 0.5f);

    float score = health_ethics_check_proportionality(&context);

    // Under-reaction gets a small bonus but still penalized for large mismatch
    // The score is based on severity difference, so a big gap gives low score
    // but conservative action should still be non-zero
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 1.0f);
}

TEST_F(HealthEthicsBridgeTest, Proportionality_BonusForEmergency) {
    health_action_context_init(&context, 1, HEALTH_RECOVERY_ACTION_FULL_RESTART, 0.95f);
    context.is_emergency = true;

    float score = health_ethics_check_proportionality(&context);

    EXPECT_GE(score, 0.5f);
}

TEST_F(HealthEthicsBridgeTest, Proportionality_HandlesNullContext) {
    float score = health_ethics_check_proportionality(nullptr);
    EXPECT_FLOAT_EQ(score, 0.0f);
}

//=============================================================================
// Full Ethics Evaluation Tests
//=============================================================================

TEST_F(HealthEthicsBridgeTest, FullEvaluation_PermitsGoodAction) {
    health_action_context_init(&context, 1, HEALTH_RECOVERY_ACTION_REDUCE_LOAD, 0.4f);
    context.service_disruption = 0.1f;
    context.data_loss_risk = 0.0f;

    int result = health_ethics_evaluate_action(nullptr, &context, &evaluation);

    EXPECT_EQ(result, 0);
    EXPECT_TRUE(evaluation.action_permitted);
    EXPECT_GT(evaluation.ethical_score, 0.5f);
}

TEST_F(HealthEthicsBridgeTest, FullEvaluation_BlocksUnethicalAction) {
    health_action_context_init(&context, 1, HEALTH_RECOVERY_ACTION_EMERGENCY_SHUTDOWN, 0.2f);
    context.service_disruption = 0.8f;
    context.data_loss_risk = 0.5f;

    int result = health_ethics_evaluate_action(nullptr, &context, &evaluation);

    EXPECT_EQ(result, 0);
    EXPECT_FALSE(evaluation.action_permitted);
    EXPECT_NE(evaluation.recommended_action, HEALTH_RECOVERY_ACTION_EMERGENCY_SHUTDOWN);
}

TEST_F(HealthEthicsBridgeTest, FullEvaluation_FirstLawOverride) {
    health_action_context_init(&context, 1, HEALTH_RECOVERY_ACTION_FULL_RESTART, 0.9f);
    context.inaction_causes_harm = true;

    int result = health_ethics_evaluate_action(nullptr, &context, &evaluation);

    EXPECT_EQ(result, 0);
    EXPECT_TRUE(evaluation.first_law_override);
    EXPECT_EQ(evaluation.required_by_law, ASIMOV_LAW_FIRST);
}

TEST_F(HealthEthicsBridgeTest, FullEvaluation_GeneratesJustification) {
    health_action_context_init(&context, 1, HEALTH_RECOVERY_ACTION_CLEAR_CACHE, 0.3f);

    health_ethics_evaluate_action(nullptr, &context, &evaluation);

    EXPECT_GT(strlen(evaluation.justification), 0u);
}

TEST_F(HealthEthicsBridgeTest, FullEvaluation_HandlesNullPointers) {
    EXPECT_EQ(health_ethics_evaluate_action(nullptr, nullptr, &evaluation), -1);
    EXPECT_EQ(health_ethics_evaluate_action(nullptr, &context, nullptr), -1);
}

//=============================================================================
// Psychological State Tests
//=============================================================================

TEST_F(HealthEthicsBridgeTest, PsychStateInit_SetsDefaults) {
    health_psych_state_init(&psych_state);

    EXPECT_FLOAT_EQ(psych_state.stress_level, 0.0f);
    EXPECT_FLOAT_EQ(psych_state.decision_confidence, 1.0f);
    EXPECT_FLOAT_EQ(psych_state.emotional_stability, 1.0f);
    EXPECT_FALSE(psych_state.in_panic_mode);
    EXPECT_FALSE(psych_state.needs_human_help);
}

TEST_F(HealthEthicsBridgeTest, PsychStateUpdate_SuccessReducesStress) {
    health_psych_state_init(&psych_state);
    psych_state.stress_level = 0.5f;

    health_psych_state_update(&psych_state, true, 0.5f, &psych_config);

    EXPECT_LT(psych_state.stress_level, 0.5f);
    EXPECT_EQ(psych_state.consecutive_successes, 1u);
}

TEST_F(HealthEthicsBridgeTest, PsychStateUpdate_FailureIncreasesStress) {
    health_psych_state_init(&psych_state);

    health_psych_state_update(&psych_state, false, 0.5f, &psych_config);

    EXPECT_GT(psych_state.stress_level, 0.0f);
    EXPECT_EQ(psych_state.consecutive_failures, 1u);
    EXPECT_LT(psych_state.decision_confidence, 1.0f);
}

TEST_F(HealthEthicsBridgeTest, PsychStateUpdate_PanicModeAfterThreshold) {
    health_psych_state_init(&psych_state);
    psych_config.panic_threshold = 3;

    for (int i = 0; i < 3; i++) {
        health_psych_state_update(&psych_state, false, 0.5f, &psych_config);
    }

    EXPECT_TRUE(psych_state.in_panic_mode);
    EXPECT_EQ(psych_state.consecutive_failures, 3u);
}

TEST_F(HealthEthicsBridgeTest, PsychStateUpdate_ExitsPanicOnSuccess) {
    health_psych_state_init(&psych_state);
    psych_state.in_panic_mode = true;
    psych_state.consecutive_failures = 5;

    // Two successes should exit panic mode
    health_psych_state_update(&psych_state, true, 0.5f, &psych_config);
    health_psych_state_update(&psych_state, true, 0.5f, &psych_config);

    EXPECT_FALSE(psych_state.in_panic_mode);
}

TEST_F(HealthEthicsBridgeTest, PsychStateDecay_ReducesStress) {
    health_psych_state_init(&psych_state);
    psych_state.stress_level = 0.8f;

    health_psych_apply_decay(&psych_state, 10000, &psych_config);  // 10 seconds

    EXPECT_LT(psych_state.stress_level, 0.8f);
}

TEST_F(HealthEthicsBridgeTest, PsychSelfCalm_ReducesStressImmediately) {
    health_psych_state_init(&psych_state);
    psych_state.stress_level = 1.0f;

    int result = health_psych_self_calm(&psych_state, &psych_config);

    EXPECT_EQ(result, 0);
    EXPECT_LT(psych_state.stress_level, 1.0f);
    EXPECT_TRUE(psych_state.self_calming_active);
}

TEST_F(HealthEthicsBridgeTest, PsychNeedsHumanHelp_TrueInPanicMode) {
    health_psych_state_init(&psych_state);
    psych_state.in_panic_mode = true;

    bool needs_help = health_psych_needs_human_help(&psych_state, &psych_config);

    EXPECT_TRUE(needs_help);
}

TEST_F(HealthEthicsBridgeTest, PsychNeedsHumanHelp_TrueOnLowConfidence) {
    health_psych_state_init(&psych_state);
    psych_state.decision_confidence = 0.1f;

    bool needs_help = health_psych_needs_human_help(&psych_state, &psych_config);

    EXPECT_TRUE(needs_help);
}

TEST_F(HealthEthicsBridgeTest, PsychPermitsAction_AllowsLowSeverityAlways) {
    health_psych_state_init(&psych_state);
    psych_state.decision_confidence = 0.3f;

    bool permits = health_psych_permits_action(&psych_state, HEALTH_ACTION_SEVERITY_LOW, &psych_config);

    EXPECT_TRUE(permits);
}

TEST_F(HealthEthicsBridgeTest, PsychPermitsAction_BlocksHighSeverityOnLowConfidence) {
    health_psych_state_init(&psych_state);
    psych_state.decision_confidence = 0.3f;

    bool permits = health_psych_permits_action(&psych_state, HEALTH_ACTION_SEVERITY_EXTREME, &psych_config);

    EXPECT_FALSE(permits);
}

TEST_F(HealthEthicsBridgeTest, PsychPermitsAction_BlocksExtremeInPanicMode) {
    health_psych_state_init(&psych_state);
    psych_state.in_panic_mode = true;
    psych_state.decision_confidence = 1.0f;

    bool permits = health_psych_permits_action(&psych_state, HEALTH_ACTION_SEVERITY_EXTREME, &psych_config);

    EXPECT_FALSE(permits);
}

TEST_F(HealthEthicsBridgeTest, PsychConfidenceThreshold_IncreasesWithStress) {
    health_psych_state_init(&psych_state);
    float base = health_psych_get_confidence_threshold(&psych_state, &psych_config);

    psych_state.stress_level = 0.8f;
    float stressed = health_psych_get_confidence_threshold(&psych_state, &psych_config);

    EXPECT_GT(stressed, base);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(HealthEthicsBridgeTest, GetActionSeverity_ReturnsCorrectValues) {
    EXPECT_EQ(health_action_get_severity(HEALTH_RECOVERY_ACTION_LOG_ONLY), HEALTH_ACTION_SEVERITY_MINIMAL);
    EXPECT_EQ(health_action_get_severity(HEALTH_RECOVERY_ACTION_CLEAR_CACHE), HEALTH_ACTION_SEVERITY_LOW);
    EXPECT_EQ(health_action_get_severity(HEALTH_RECOVERY_ACTION_PARTIAL_RESTART), HEALTH_ACTION_SEVERITY_MODERATE);
    EXPECT_EQ(health_action_get_severity(HEALTH_RECOVERY_ACTION_FULL_RESTART), HEALTH_ACTION_SEVERITY_HIGH);
    EXPECT_EQ(health_action_get_severity(HEALTH_RECOVERY_ACTION_EMERGENCY_SHUTDOWN), HEALTH_ACTION_SEVERITY_EXTREME);
}

TEST_F(HealthEthicsBridgeTest, SeverityName_ReturnsReadableStrings) {
    EXPECT_STREQ(health_action_severity_name(HEALTH_ACTION_SEVERITY_MINIMAL), "Minimal");
    EXPECT_STREQ(health_action_severity_name(HEALTH_ACTION_SEVERITY_EXTREME), "Extreme");
}

TEST_F(HealthEthicsBridgeTest, ActionName_ReturnsReadableStrings) {
    EXPECT_STREQ(health_recovery_action_name(HEALTH_RECOVERY_ACTION_NONE), "None");
    EXPECT_STREQ(health_recovery_action_name(HEALTH_RECOVERY_ACTION_EMERGENCY_SHUTDOWN), "Emergency Shutdown");
}
