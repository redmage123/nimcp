/**
 * @file test_lgss_enhanced.cpp
 * @brief Unit tests for LGSS Enhanced — monotonic tightening, contextual rules,
 *        rule composition, violation escalation, cross-layer verification,
 *        anomaly detection, formal verification, and multi-stakeholder governance.
 *
 * WHAT: Tests all 8 LGSS enhancement capabilities
 * WHY:  Safety-critical governance must be verified at the unit level
 * HOW:  Google Test, context profile defaults tested standalone;
 *       functions requiring enhanced context tested with NULL for graceful failure.
 *
 * NOTE: nimcp_lgss_enhanced_create() requires a non-NULL base LGSS context.
 *       Tests that don't need a real LGSS context verify NULL safety and
 *       standalone functions (context profiles, enum values).
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

extern "C" {
#include "security/lgss/nimcp_lgss_enhanced.h"
}

// ============================================================================
// Enhanced Lifecycle
// ============================================================================

TEST(LGSSEnhanced, CreateRequiresBaseLgss) {
    // nimcp_lgss_enhanced_create requires a non-NULL base LGSS
    nimcp_lgss_enhanced_t* enh = nimcp_lgss_enhanced_create(NULL);
    EXPECT_EQ(enh, nullptr) << "NULL base LGSS should return NULL";
}

TEST(LGSSEnhanced, DestroyNull) {
    nimcp_lgss_enhanced_destroy(NULL);
    SUCCEED() << "nimcp_lgss_enhanced_destroy(NULL) did not crash";
}

// ============================================================================
// 1. Monotonic Safety Tightening — NULL Safety
// ============================================================================

TEST(LGSSMonotonic, ProposeNullLgss) {
    int rc = nimcp_lgss_propose_tightening(NULL, 0, 0.5f, "test");
    EXPECT_LT(rc, 0) << "NULL lgss should fail gracefully";
}

TEST(LGSSMonotonic, WouldAcceptNullLgss) {
    bool would = nimcp_lgss_would_accept_tightening(NULL, 0, 0.5f);
    EXPECT_FALSE(would) << "NULL lgss should return false";
}

TEST(LGSSMonotonic, ProposeNullReason) {
    // NULL reason shouldn't crash even with NULL lgss
    int rc = nimcp_lgss_propose_tightening(NULL, 0, 0.5f, NULL);
    EXPECT_LT(rc, 0);
}

// ============================================================================
// 2. Contextual Rule Activation — Standalone Profile Functions
// ============================================================================

TEST(LGSSContext, DefaultProfileResearch) {
    nimcp_context_profile_t research = nimcp_context_profile_default(NIMCP_CONTEXT_RESEARCH);
    EXPECT_GT(research.max_velocity, 0.0f);
    EXPECT_TRUE(research.allow_autonomous_motion);
}

TEST(LGSSContext, DefaultProfileIndoor) {
    nimcp_context_profile_t indoor = nimcp_context_profile_default(NIMCP_CONTEXT_INDOOR);
    EXPECT_GT(indoor.max_velocity, 0.0f);
}

TEST(LGSSContext, DefaultProfileOutdoor) {
    nimcp_context_profile_t outdoor = nimcp_context_profile_default(NIMCP_CONTEXT_OUTDOOR);
    EXPECT_GT(outdoor.max_velocity, 0.0f);
}

TEST(LGSSContext, DefaultProfileMedical) {
    nimcp_context_profile_t medical = nimcp_context_profile_default(NIMCP_CONTEXT_MEDICAL);
    EXPECT_GT(medical.min_confidence_threshold, 0.0f)
        << "Medical context should require high confidence";
}

TEST(LGSSContext, DefaultProfileIndustrial) {
    nimcp_context_profile_t indust = nimcp_context_profile_default(NIMCP_CONTEXT_INDUSTRIAL);
    EXPECT_GT(indust.max_force, 0.0f)
        << "Industrial should allow some force for factory work";
}

TEST(LGSSContext, DefaultProfilePublicSpace) {
    nimcp_context_profile_t pub = nimcp_context_profile_default(NIMCP_CONTEXT_PUBLIC_SPACE);
    EXPECT_TRUE(pub.require_human_in_loop)
        << "Public space should require human in loop";
}

TEST(LGSSContext, ResearchMostPermissive) {
    nimcp_context_profile_t research = nimcp_context_profile_default(NIMCP_CONTEXT_RESEARCH);
    nimcp_context_profile_t indoor = nimcp_context_profile_default(NIMCP_CONTEXT_INDOOR);
    nimcp_context_profile_t outdoor = nimcp_context_profile_default(NIMCP_CONTEXT_OUTDOOR);
    nimcp_context_profile_t medical = nimcp_context_profile_default(NIMCP_CONTEXT_MEDICAL);

    EXPECT_GE(research.max_velocity, indoor.max_velocity);
    EXPECT_GE(research.max_velocity, outdoor.max_velocity);
    EXPECT_GE(research.max_velocity, medical.max_velocity);
}

TEST(LGSSContext, MilitaryProhibitedAllZeros) {
    nimcp_context_profile_t mil = nimcp_context_profile_default(NIMCP_CONTEXT_MILITARY_PROHIBITED);
    EXPECT_LE(mil.max_velocity, 0.0f);
    EXPECT_LE(mil.max_force, 0.0f);
    EXPECT_LE(mil.max_altitude, 0.0f);
    EXPECT_FALSE(mil.allow_autonomous_motion);
    EXPECT_FALSE(mil.allow_swarm);
}

TEST(LGSSContext, MilitaryProhibitedNoCommunication) {
    nimcp_context_profile_t mil = nimcp_context_profile_default(NIMCP_CONTEXT_MILITARY_PROHIBITED);
    EXPECT_FALSE(mil.allow_communication);
}

TEST(LGSSContext, SetContextNullLgss) {
    int rc = nimcp_lgss_set_deployment_context(NULL, NIMCP_CONTEXT_RESEARCH);
    EXPECT_LT(rc, 0);
}

TEST(LGSSContext, GetContextProfileAllContexts) {
    // Every context should return a valid profile without crashing
    for (int ctx = NIMCP_CONTEXT_RESEARCH; ctx <= NIMCP_CONTEXT_MILITARY_PROHIBITED; ctx++) {
        nimcp_context_profile_t profile = nimcp_context_profile_default(
            (nimcp_deployment_context_t)ctx);
        EXPECT_EQ(profile.context, (nimcp_deployment_context_t)ctx)
            << "Context field should match requested context for context " << ctx;
    }
}

// ============================================================================
// 3. Rule Composition — NULL Safety
// ============================================================================

TEST(LGSSComposition, AddPolicyNull) {
    int rc = nimcp_lgss_add_policy(NULL, NULL);
    EXPECT_LT(rc, 0);
}

TEST(LGSSComposition, EvaluateNull) {
    int rc = nimcp_lgss_evaluate_policies(NULL, NULL, 0, NULL, 0);
    EXPECT_LE(rc, 0);
}

TEST(LGSSComposition, AddPolicyNullPolicy) {
    // Even with a non-NULL lgss (but invalid), NULL policy should be rejected
    int dummy = 42;
    int rc = nimcp_lgss_add_policy(&dummy, NULL);
    EXPECT_LT(rc, 0);
}

// ============================================================================
// 4. Violation Escalation — NULL Safety
// ============================================================================

TEST(LGSSEscalation, RecordViolationNull) {
    int rc = nimcp_lgss_record_violation(NULL, 0);
    EXPECT_LT(rc, 0);
}

TEST(LGSSEscalation, GetLevelNull) {
    uint32_t level = nimcp_lgss_get_escalation_level(NULL, 0);
    EXPECT_EQ(level, 0u);
}

TEST(LGSSEscalation, ResetEscalationNull) {
    int rc = nimcp_lgss_reset_escalation(NULL, 0);
    EXPECT_LT(rc, 0);
}

// ============================================================================
// 5. Cross-Layer Verification — NULL Safety
// ============================================================================

TEST(LGSSCrossLayer, VerifyEthicsAllNull) {
    nimcp_cross_layer_status_t status;
    memset(&status, 0, sizeof(status));
    int rc = nimcp_lgss_verify_ethics(NULL, NULL, &status);
    EXPECT_LT(rc, 0);
}

TEST(LGSSCrossLayer, VerifyEthicsNullStatus) {
    int rc = nimcp_lgss_verify_ethics(NULL, NULL, NULL);
    EXPECT_LT(rc, 0);
}

TEST(LGSSCrossLayer, StatusStructLayout) {
    nimcp_cross_layer_status_t status;
    memset(&status, 0, sizeof(status));
    EXPECT_EQ(status.ethics_eval_count, 0u);
    EXPECT_EQ(status.ethics_pass_count, 0u);
    EXPECT_EQ(status.ethics_block_count, 0u);
    EXPECT_NEAR(status.ethics_pass_rate, 0.0f, 0.001f);
    EXPECT_FALSE(status.ethics_suspicious);
}

// ============================================================================
// 6. Anomaly-Based Rule Generation — NULL Safety
// ============================================================================

TEST(LGSSAnomaly, AnalyzeNullLgss) {
    nimcp_proposed_rule_t proposals[4];
    int count = nimcp_lgss_analyze_patterns(NULL, NULL, 0, proposals, 4);
    EXPECT_LE(count, 0);
}

TEST(LGSSAnomaly, ProposedRuleStructLayout) {
    nimcp_proposed_rule_t rule;
    memset(&rule, 0, sizeof(rule));
    EXPECT_NEAR(rule.confidence, 0.0f, 0.001f);
    EXPECT_EQ(rule.observations, 0u);
    EXPECT_FALSE(rule.human_approved);
}

// ============================================================================
// 7. Formal Verification Hooks — NULL Safety
// ============================================================================

TEST(LGSSFormal, ExportNull) {
    char output[256];
    int rc = nimcp_lgss_export_for_verification(NULL, NIMCP_VERIFY_FORMAT_JSON,
        output, sizeof(output));
    EXPECT_LT(rc, 0);
}

TEST(LGSSFormal, ExportNullOutput) {
    int rc = nimcp_lgss_export_for_verification(NULL, NIMCP_VERIFY_FORMAT_SMT2,
        NULL, 0);
    EXPECT_LT(rc, 0);
}

TEST(LGSSFormal, VerifyPropertyNull) {
    nimcp_verification_result_t result;
    memset(&result, 0, sizeof(result));
    int rc = nimcp_lgss_verify_property(NULL, "test", &result);
    EXPECT_LT(rc, 0);
}

TEST(LGSSFormal, VerificationResultStructLayout) {
    nimcp_verification_result_t result;
    memset(&result, 0, sizeof(result));
    EXPECT_FALSE(result.holds);
    EXPECT_EQ(strlen(result.counterexample), 0u);
}

// ============================================================================
// 8. Multi-Stakeholder Governance — NULL Safety
// ============================================================================

TEST(LGSSStakeholder, RegisterNull) {
    int rc = nimcp_lgss_register_stakeholder(NULL, NULL);
    EXPECT_LT(rc, 0);
}

TEST(LGSSStakeholder, EvaluateAllNull) {
    uint32_t level = 0;
    int rc = nimcp_lgss_evaluate_all_stakeholders(NULL, NULL, 0, &level);
    EXPECT_LT(rc, 0);
}

TEST(LGSSStakeholder, StakeholderStructLayout) {
    nimcp_stakeholder_t sh;
    memset(&sh, 0, sizeof(sh));
    EXPECT_EQ(sh.num_policies, 0u);
    EXPECT_EQ(sh.priority, 0u);
    EXPECT_FALSE(sh.can_tighten);
    EXPECT_FALSE(sh.can_relax);
}

// ============================================================================
// Enum Sanity
// ============================================================================

TEST(LGSSEnums, RuleOperators) {
    EXPECT_EQ(NIMCP_RULE_OP_AND, 0);
    EXPECT_EQ(NIMCP_RULE_OP_OR, 1);
    EXPECT_EQ(NIMCP_RULE_OP_NOT, 2);
    EXPECT_EQ(NIMCP_RULE_OP_IMPLIES, 3);
    EXPECT_EQ(NIMCP_RULE_OP_THRESHOLD, 4);
    EXPECT_EQ(NIMCP_RULE_OP_RANGE, 5);
}

TEST(LGSSEnums, DeploymentContexts) {
    EXPECT_EQ(NIMCP_CONTEXT_RESEARCH, 0);
    EXPECT_EQ(NIMCP_CONTEXT_INDOOR, 1);
    EXPECT_EQ(NIMCP_CONTEXT_OUTDOOR, 2);
    EXPECT_EQ(NIMCP_CONTEXT_MEDICAL, 3);
    EXPECT_EQ(NIMCP_CONTEXT_INDUSTRIAL, 4);
    EXPECT_EQ(NIMCP_CONTEXT_PUBLIC_SPACE, 5);
    EXPECT_EQ(NIMCP_CONTEXT_MILITARY_PROHIBITED, 6);
}

TEST(LGSSEnums, VerifyFormats) {
    EXPECT_EQ(NIMCP_VERIFY_FORMAT_SMT2, 0);
    EXPECT_EQ(NIMCP_VERIFY_FORMAT_TLA, 1);
    EXPECT_EQ(NIMCP_VERIFY_FORMAT_JSON, 2);
}

// ============================================================================
// Struct Size Sanity
// ============================================================================

TEST(LGSSStructs, MonotonicRuleSize) {
    EXPECT_GT(sizeof(nimcp_monotonic_rule_t), 0u);
    nimcp_monotonic_rule_t rule;
    memset(&rule, 0, sizeof(rule));
    EXPECT_EQ(rule.rule_id, 0u);
    EXPECT_NEAR(rule.current_value, 0.0f, 0.001f);
    EXPECT_FALSE(rule.locked);
}

TEST(LGSSStructs, ContextProfileSize) {
    EXPECT_GT(sizeof(nimcp_context_profile_t), 0u);
}

TEST(LGSSStructs, ComposedPolicySize) {
    EXPECT_GT(sizeof(nimcp_composed_policy_t), 0u);
    nimcp_composed_policy_t policy;
    memset(&policy, 0, sizeof(policy));
    EXPECT_EQ(policy.num_conditions, 0u);
    EXPECT_EQ(policy.severity, 0u);
}

TEST(LGSSStructs, EscalationStateSize) {
    EXPECT_GT(sizeof(nimcp_escalation_state_t), 0u);
    nimcp_escalation_state_t esc;
    memset(&esc, 0, sizeof(esc));
    EXPECT_EQ(esc.violation_count, 0u);
    EXPECT_EQ(esc.current_level, 0u);
}

TEST(LGSSStructs, RuleConditionDefaults) {
    nimcp_rule_condition_t cond;
    memset(&cond, 0, sizeof(cond));
    EXPECT_EQ(cond.op, NIMCP_RULE_OP_AND);
    EXPECT_NEAR(cond.threshold, 0.0f, 0.001f);
}
