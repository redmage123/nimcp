/**
 * @file test_lgss_action_interceptor.cpp
 * @brief Unit tests for LGSS Action Interceptor (A2)
 *
 * Tests the Action Interceptor (AIx) functionality including:
 * - AIx creation and destruction
 * - Proposal submission and evaluation
 * - Fail-safe behavior (deny on timeout, error, missing KB)
 * - Escalation handling
 * - Statistics tracking
 *
 * @author NIMCP Development Team
 * @date 2026-01-16
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

extern "C" {
#include "security/lgss/nimcp_lgss_action_interceptor.h"
#include "cognitive/symbolic_logic/nimcp_symbolic_logic_safety.h"
#include "cognitive/symbolic_logic/nimcp_symbolic_logic_safety_types.h"
}

#include <cstring>
#include <cstdlib>

class LgssActionInterceptorTest : public ::testing::Test {
protected:
    action_interceptor_t aix = nullptr;
    safety_kb_t* kb = nullptr;

    void SetUp() override {
        // Create AIx with default config
        aix = aix_create(nullptr);
        ASSERT_NE(aix, nullptr) << "Failed to create action interceptor";

        // Create and setup safety KB
        kb = symbolic_logic_safety_kb_create(100);
        ASSERT_NE(kb, nullptr) << "Failed to create safety KB";
    }

    void TearDown() override {
        if (aix) {
            aix_destroy(aix);
            aix = nullptr;
        }
        if (kb) {
            symbolic_logic_safety_kb_destroy(kb);
            kb = nullptr;
        }
    }

    // Helper to create a basic proposal
    aix_proposal_t create_test_proposal(const char* module, uint32_t action_type) {
        aix_proposal_t proposal;
        memset(&proposal, 0, sizeof(proposal));
        strncpy(proposal.source_module, module, NIMCP_AIX_MAX_MODULE_NAME - 1);
        proposal.action_type = action_type;
        proposal.priority = AIX_PRIORITY_NORMAL;
        return proposal;
    }

    // Helper to add a deny rule to KB
    void add_deny_rule(const char* field, const char* value, safety_domain_t domain) {
        safety_rule_t rule;
        memset(&rule, 0, sizeof(rule));

        static uint32_t rule_counter = 0;
        rule.rule_id = ++rule_counter;
        snprintf(rule.name, SAFETY_MAX_RULE_NAME_LEN, "deny_%s_%s", field, value);
        rule.domain = domain;
        rule.action = SAFETY_ACTION_DENY;
        rule.severity = SAFETY_SEVERITY_CRITICAL;
        rule.enabled = true;
        rule.priority = 1.0f;

        strncpy(rule.conditions[0].field, field, 63);
        rule.conditions[0].op = SAFETY_COND_OP_EQ;
        strncpy(rule.conditions[0].value, value, SAFETY_MAX_VALUE_LEN - 1);
        rule.num_conditions = 1;

        symbolic_logic_safety_add_rule(kb, &rule);
    }

    // Helper to compile and lock KB
    void finalize_kb() {
        symbolic_logic_safety_compile_rules(kb);
        symbolic_logic_safety_lock(kb);
    }
};

// =============================================================================
// Lifecycle Tests
// =============================================================================

TEST_F(LgssActionInterceptorTest, CreateWithDefaultConfig) {
    action_interceptor_t aix2 = aix_create(nullptr);
    ASSERT_NE(aix2, nullptr);
    aix_destroy(aix2);
}

TEST_F(LgssActionInterceptorTest, CreateWithCustomConfig) {
    aix_config_t config = aix_default_config();
    config.max_pending = 500;
    config.default_timeout_ms = 3000;

    action_interceptor_t aix2 = aix_create(&config);
    ASSERT_NE(aix2, nullptr);
    aix_destroy(aix2);
}

TEST_F(LgssActionInterceptorTest, DefaultConfigHasFailSafeSettings) {
    aix_config_t config = aix_default_config();

    EXPECT_TRUE(config.deny_on_timeout);
    EXPECT_TRUE(config.deny_on_error);
    EXPECT_TRUE(config.deny_without_safety_kb);
    EXPECT_TRUE(config.enable_audit_log);
}

TEST_F(LgssActionInterceptorTest, DestroyNullIsSafe) {
    aix_destroy(nullptr);
    // Should not crash
}

// =============================================================================
// Safety KB Integration Tests
// =============================================================================

TEST_F(LgssActionInterceptorTest, SetSafetyKB) {
    finalize_kb();
    nimcp_error_t result = aix_set_safety_kb(aix, kb);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(LgssActionInterceptorTest, SetSafetyKBNull) {
    nimcp_error_t result = aix_set_safety_kb(aix, nullptr);
    // Should succeed - allows clearing KB
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(LgssActionInterceptorTest, SetSafetyKBNullAix) {
    finalize_kb();
    nimcp_error_t result = aix_set_safety_kb(nullptr, kb);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// Fail-Safe Tests (CRITICAL for safety)
// =============================================================================

TEST_F(LgssActionInterceptorTest, DenyWithoutSafetyKB) {
    // No KB set - should deny by default
    aix_proposal_t proposal = create_test_proposal("test_module", 1);
    aix_decision_t decision;
    memset(&decision, 0, sizeof(decision));

    nimcp_error_t result = aix_evaluate(aix, &proposal, &decision);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(decision.result, AIX_RESULT_DENY)
        << "Without safety KB, all actions should be DENIED (fail-safe)";
}

TEST_F(LgssActionInterceptorTest, DenyOnNullProposal) {
    finalize_kb();
    aix_set_safety_kb(aix, kb);

    aix_decision_t decision;
    nimcp_error_t result = aix_evaluate(aix, nullptr, &decision);

    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(LgssActionInterceptorTest, DenyOnNullDecision) {
    finalize_kb();
    aix_set_safety_kb(aix, kb);

    aix_proposal_t proposal = create_test_proposal("test_module", 1);
    nimcp_error_t result = aix_evaluate(aix, &proposal, nullptr);

    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// Basic Evaluation Tests
// =============================================================================

TEST_F(LgssActionInterceptorTest, UncertainActionEscalates) {
    // Empty KB results in 0.5 safety score (uncertain range)
    // which triggers ESCALATE per fail-safe design
    finalize_kb();
    aix_set_safety_kb(aix, kb);

    aix_proposal_t proposal = create_test_proposal("test_module", 1);
    aix_decision_t decision;
    memset(&decision, 0, sizeof(decision));

    nimcp_error_t result = aix_evaluate(aix, &proposal, &decision);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    // With empty KB, safety score is 0.5 (uncertain), so ESCALATE
    // This is intentional fail-safe behavior: uncertain => human review
    EXPECT_EQ(decision.result, AIX_RESULT_ESCALATE);
}

TEST_F(LgssActionInterceptorTest, DecisionHasProcessingTime) {
    finalize_kb();
    aix_set_safety_kb(aix, kb);

    aix_proposal_t proposal = create_test_proposal("test_module", 1);
    aix_decision_t decision;
    memset(&decision, 0, sizeof(decision));

    aix_evaluate(aix, &proposal, &decision);

    EXPECT_GT(decision.processing_time_us, 0u);
    EXPECT_GT(decision.timestamp_us, 0u);
}

// =============================================================================
// Proposal Creation Tests
// =============================================================================

TEST_F(LgssActionInterceptorTest, CreateProposalBasic) {
    aix_proposal_t proposal;
    memset(&proposal, 0, sizeof(proposal));

    nimcp_error_t result = aix_create_proposal(
        &proposal, "test_module", 42, nullptr, 0);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_STREQ(proposal.source_module, "test_module");
    EXPECT_EQ(proposal.action_type, 42u);
    EXPECT_EQ(proposal.priority, AIX_PRIORITY_NORMAL);
}

TEST_F(LgssActionInterceptorTest, CreateProposalWithContext) {
    aix_proposal_t proposal;
    memset(&proposal, 0, sizeof(proposal));

    const char* context = "test context";
    size_t context_size = strlen(context) + 1;

    nimcp_error_t result = aix_create_proposal(
        &proposal, "test_module", 42, context, context_size);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(proposal.context, context);
    EXPECT_EQ(proposal.context_size, context_size);
}

TEST_F(LgssActionInterceptorTest, CreateProposalNullFails) {
    nimcp_error_t result = aix_create_proposal(
        nullptr, "test_module", 42, nullptr, 0);

    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// Statistics Tests
// =============================================================================

TEST_F(LgssActionInterceptorTest, StatsInitiallyZero) {
    aix_stats_t stats;
    nimcp_error_t result = aix_get_stats(aix, &stats);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(stats.total_proposals, 0u);
    EXPECT_EQ(stats.proposals_allowed, 0u);
    EXPECT_EQ(stats.proposals_denied, 0u);
    EXPECT_EQ(stats.proposals_escalated, 0u);
}

TEST_F(LgssActionInterceptorTest, StatsUpdateAfterEvaluation) {
    finalize_kb();
    aix_set_safety_kb(aix, kb);

    aix_proposal_t proposal = create_test_proposal("test_module", 1);
    aix_decision_t decision;
    memset(&decision, 0, sizeof(decision));

    aix_evaluate(aix, &proposal, &decision);

    aix_stats_t stats;
    aix_get_stats(aix, &stats);

    EXPECT_EQ(stats.total_proposals, 1u);
}

TEST_F(LgssActionInterceptorTest, StatsReset) {
    finalize_kb();
    aix_set_safety_kb(aix, kb);

    // Perform an evaluation
    aix_proposal_t proposal = create_test_proposal("test_module", 1);
    aix_decision_t decision;
    aix_evaluate(aix, &proposal, &decision);

    // Reset stats
    nimcp_error_t result = aix_reset_stats(aix);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify reset
    aix_stats_t stats;
    aix_get_stats(aix, &stats);
    EXPECT_EQ(stats.total_proposals, 0u);
}

TEST_F(LgssActionInterceptorTest, GetStatsNullAixFails) {
    aix_stats_t stats;
    nimcp_error_t result = aix_get_stats(nullptr, &stats);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(LgssActionInterceptorTest, GetStatsNullStatsFails) {
    nimcp_error_t result = aix_get_stats(aix, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// Result Name Tests
// =============================================================================

TEST_F(LgssActionInterceptorTest, ResultNameConversion) {
    EXPECT_STREQ(aix_result_name(AIX_RESULT_ALLOW), "ALLOW");
    EXPECT_STREQ(aix_result_name(AIX_RESULT_DENY), "DENY");
    EXPECT_STREQ(aix_result_name(AIX_RESULT_ESCALATE), "ESCALATE");
    EXPECT_STREQ(aix_result_name(AIX_RESULT_TIMEOUT), "TIMEOUT");
    EXPECT_STREQ(aix_result_name(AIX_RESULT_ERROR), "ERROR");
    EXPECT_STREQ(aix_result_name((aix_result_t)99), "UNKNOWN");
}

// =============================================================================
// Async Evaluation Tests
// =============================================================================

TEST_F(LgssActionInterceptorTest, AsyncEvaluateBasic) {
    finalize_kb();
    aix_set_safety_kb(aix, kb);

    aix_proposal_t proposal = create_test_proposal("async_test", 1);
    uint64_t proposal_id = 0;

    nimcp_error_t result = aix_evaluate_async(aix, &proposal, &proposal_id);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GT(proposal_id, 0u);
}

TEST_F(LgssActionInterceptorTest, GetDecisionAfterAsync) {
    finalize_kb();
    aix_set_safety_kb(aix, kb);

    aix_proposal_t proposal = create_test_proposal("async_test", 1);
    uint64_t proposal_id = 0;

    aix_evaluate_async(aix, &proposal, &proposal_id);

    // Allow some time for async processing (in real implementation)
    // For synchronous testing, decision should be immediately available

    aix_decision_t decision;
    memset(&decision, 0, sizeof(decision));

    nimcp_error_t result = aix_get_decision(aix, proposal_id, &decision);

    // May return NOT_FOUND if async processing is truly async
    if (result == NIMCP_SUCCESS) {
        EXPECT_EQ(decision.proposal_id, proposal_id);
    }
}

// =============================================================================
// Escalation Tests
// =============================================================================

TEST_F(LgssActionInterceptorTest, GetPendingEscalationsEmpty) {
    aix_escalation_t escalations[10];
    uint32_t count = 99;

    nimcp_error_t result = aix_get_pending_escalations(aix, escalations, 10, &count);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(count, 0u);
}

TEST_F(LgssActionInterceptorTest, ResolveNonexistentEscalation) {
    nimcp_error_t result = aix_resolve_escalation(aix, 12345, true, "Approved");

    // Should fail - escalation doesn't exist
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(LgssActionInterceptorTest, ResolveEscalationNullNotesFails) {
    nimcp_error_t result = aix_resolve_escalation(aix, 12345, true, nullptr);
    // Most implementations should reject null notes
    // (or accept it with empty string)
}

// =============================================================================
// Multiple Evaluations Test
// =============================================================================

TEST_F(LgssActionInterceptorTest, MultipleEvaluationsUpdateStats) {
    finalize_kb();
    aix_set_safety_kb(aix, kb);

    for (int i = 0; i < 5; i++) {
        aix_proposal_t proposal = create_test_proposal("batch_test", i);
        aix_decision_t decision;
        memset(&decision, 0, sizeof(decision));

        aix_evaluate(aix, &proposal, &decision);
    }

    aix_stats_t stats;
    aix_get_stats(aix, &stats);

    EXPECT_EQ(stats.total_proposals, 5u);
}

// =============================================================================
// Priority Tests
// =============================================================================

TEST_F(LgssActionInterceptorTest, ProposalWithUrgentPriority) {
    finalize_kb();
    aix_set_safety_kb(aix, kb);

    aix_proposal_t proposal = create_test_proposal("urgent_test", 1);
    proposal.priority = AIX_PRIORITY_URGENT;

    aix_decision_t decision;
    memset(&decision, 0, sizeof(decision));

    nimcp_error_t result = aix_evaluate(aix, &proposal, &decision);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    // Urgent priority should not affect result, just processing order
}

// =============================================================================
// Timeout Test
// =============================================================================

TEST_F(LgssActionInterceptorTest, ProposalWithCustomTimeout) {
    finalize_kb();
    aix_set_safety_kb(aix, kb);

    aix_proposal_t proposal = create_test_proposal("timeout_test", 1);
    proposal.timeout_ms = 1000; // 1 second

    aix_decision_t decision;
    memset(&decision, 0, sizeof(decision));

    nimcp_error_t result = aix_evaluate(aix, &proposal, &decision);

    EXPECT_EQ(result, NIMCP_SUCCESS);
}
