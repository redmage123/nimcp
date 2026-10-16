/**
 * @file test_financial_ethics_learning_bridges.c
 * @brief Unit tests for Financial Ethics and Learning bridges
 *
 * WHAT: Test suite for financial ethics, explanations, consolidation, STDP,
 *       and temporal credit assignment bridges.
 *
 * WHY:  Verify correct behavior of:
 *       - Ethics: Asimov laws, Golden Rule, harm assessment
 *       - Explanations: Decision explanations, audit trails
 *       - Consolidation: Pattern replay, winner strengthening, loser pruning
 *       - STDP: Signal-outcome timing correlations, weight updates
 *       - Temporal Credit: TD(lambda), eligibility traces, credit assignment
 *
 * HOW:  Unit tests using Check framework covering all bridge APIs
 *
 * NOTE: Multiple headers define fin_decision_type_t with different enumerators.
 *       We handle this by defining a guard macro after the first inclusion.
 *
 * @author NIMCP Development Team
 * @date 2026-01-29
 */

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "cognitive/parietal/nimcp_financial_ethics_bridge.h"
#include "cognitive/parietal/nimcp_financial_explanations_bridge.h"

/* Guard against conflicting fin_decision_type_t definitions */
#define fin_decision_type_t fin_temporal_decision_type_t

#include "cognitive/parietal/nimcp_financial_consolidation_bridge.h"
#include "cognitive/parietal/nimcp_financial_stdp_bridge.h"
#include "cognitive/parietal/nimcp_financial_temporal_credit_bridge.h"

/* Restore the name for temporal credit tests - use local typedef */
#undef fin_decision_type_t

/* Create a local alias for the temporal credit decision type */
typedef enum {
    LOCAL_DECISION_TYPE_BUY = 0,
    LOCAL_DECISION_TYPE_SELL,
    LOCAL_DECISION_TYPE_HOLD,
    LOCAL_DECISION_TYPE_EXIT_LONG,
    LOCAL_DECISION_TYPE_EXIT_SHORT,
    LOCAL_DECISION_TYPE_INCREASE_POSITION,
    LOCAL_DECISION_TYPE_DECREASE_POSITION,
    LOCAL_DECISION_TYPE_STOP_LOSS,
    LOCAL_DECISION_TYPE_TAKE_PROFIT,
    LOCAL_DECISION_TYPE_REBALANCE,
    LOCAL_DECISION_TYPE_HEDGE,
    LOCAL_DECISION_TYPE_CUSTOM,
    LOCAL_DECISION_TYPE_COUNT
} local_decision_type_t;

/* ============================================================================
 * Test Fixtures - Financial Ethics Bridge
 * ============================================================================ */

static financial_ethics_bridge_t* g_ethics_bridge = NULL;

static void setup_ethics(void)
{
    fin_ethics_config_t config;
    int rc = financial_ethics_bridge_default_config(&config);
    ck_assert_int_eq(rc, 0);

    g_ethics_bridge = financial_ethics_bridge_create(&config);
    ck_assert_ptr_nonnull(g_ethics_bridge);
}

static void teardown_ethics(void)
{
    if (g_ethics_bridge) {
        financial_ethics_bridge_destroy(g_ethics_bridge);
        g_ethics_bridge = NULL;
    }
}

/* ============================================================================
 * Test Fixtures - Financial Explanations Bridge
 * ============================================================================ */

static financial_explanations_bridge_t* g_explanations_bridge = NULL;

static void setup_explanations(void)
{
    fin_explanations_config_t config;
    int rc = financial_explanations_bridge_default_config(&config);
    ck_assert_int_eq(rc, 0);

    g_explanations_bridge = financial_explanations_bridge_create(&config);
    ck_assert_ptr_nonnull(g_explanations_bridge);
}

static void teardown_explanations(void)
{
    if (g_explanations_bridge) {
        financial_explanations_bridge_destroy(g_explanations_bridge);
        g_explanations_bridge = NULL;
    }
}

/* ============================================================================
 * Test Fixtures - Financial Consolidation Bridge
 * ============================================================================ */

static financial_consolidation_bridge_t* g_consolidation_bridge = NULL;

static void setup_consolidation(void)
{
    fin_consolidation_config_t config;
    int rc = financial_consolidation_bridge_default_config(&config);
    ck_assert_int_eq(rc, 0);

    g_consolidation_bridge = financial_consolidation_bridge_create(&config);
    ck_assert_ptr_nonnull(g_consolidation_bridge);
}

static void teardown_consolidation(void)
{
    if (g_consolidation_bridge) {
        financial_consolidation_bridge_destroy(g_consolidation_bridge);
        g_consolidation_bridge = NULL;
    }
}

/* ============================================================================
 * Test Fixtures - Financial STDP Bridge
 * ============================================================================ */

static financial_stdp_bridge_t* g_stdp_bridge = NULL;

static void setup_stdp(void)
{
    fin_stdp_config_t config;
    int rc = financial_stdp_bridge_default_config(&config);
    ck_assert_int_eq(rc, 0);

    g_stdp_bridge = financial_stdp_bridge_create(&config);
    ck_assert_ptr_nonnull(g_stdp_bridge);
}

static void teardown_stdp(void)
{
    if (g_stdp_bridge) {
        financial_stdp_bridge_destroy(g_stdp_bridge);
        g_stdp_bridge = NULL;
    }
}

/* ============================================================================
 * Test Fixtures - Financial Temporal Credit Bridge
 * ============================================================================ */

static financial_temporal_credit_bridge_t* g_temporal_credit_bridge = NULL;

static void setup_temporal_credit(void)
{
    fin_temporal_credit_config_t config;
    int rc = financial_temporal_credit_bridge_default_config(&config);
    ck_assert_int_eq(rc, 0);

    g_temporal_credit_bridge = financial_temporal_credit_bridge_create(&config);
    ck_assert_ptr_nonnull(g_temporal_credit_bridge);
}

static void teardown_temporal_credit(void)
{
    if (g_temporal_credit_bridge) {
        financial_temporal_credit_bridge_destroy(g_temporal_credit_bridge);
        g_temporal_credit_bridge = NULL;
    }
}

/* ============================================================================
 * Financial Ethics Bridge Tests - Lifecycle
 * ============================================================================ */

START_TEST(test_ethics_default_config)
{
    fin_ethics_config_t config;
    int rc = financial_ethics_bridge_default_config(&config);
    ck_assert_int_eq(rc, 0);

    /* Verify default thresholds are set */
    ck_assert(config.harm_deny_threshold > 0.0f);
    ck_assert(config.harm_escalate_threshold > 0.0f);
    ck_assert(config.harm_warn_threshold > 0.0f);

    /* Verify Asimov laws enabled by default */
    ck_assert(config.enforce_first_law);
    ck_assert(config.enforce_second_law);
    ck_assert(config.enforce_third_law);
}
END_TEST

START_TEST(test_ethics_default_config_null)
{
    int rc = financial_ethics_bridge_default_config(NULL);
    ck_assert_int_eq(rc, -1);
}
END_TEST

START_TEST(test_ethics_create_destroy)
{
    fin_ethics_config_t config;
    financial_ethics_bridge_default_config(&config);

    financial_ethics_bridge_t* bridge = financial_ethics_bridge_create(&config);
    ck_assert_ptr_nonnull(bridge);

    fin_ethics_bridge_state_t state = financial_ethics_bridge_get_bridge_state(bridge);
    ck_assert_int_eq(state, FIN_ETHICS_STATE_INITIALIZED);

    financial_ethics_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_ethics_create_null_config)
{
    /* Should use defaults when config is NULL */
    financial_ethics_bridge_t* bridge = financial_ethics_bridge_create(NULL);
    ck_assert_ptr_nonnull(bridge);
    financial_ethics_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_ethics_destroy_null)
{
    /* Should not crash */
    financial_ethics_bridge_destroy(NULL);
}
END_TEST

START_TEST(test_ethics_reset)
{
    int rc = financial_ethics_bridge_reset(g_ethics_bridge);
    ck_assert_int_eq(rc, 0);

    fin_ethics_bridge_state_t state = financial_ethics_bridge_get_bridge_state(g_ethics_bridge);
    ck_assert_int_eq(state, FIN_ETHICS_STATE_INITIALIZED);
}
END_TEST

START_TEST(test_ethics_reset_null)
{
    int rc = financial_ethics_bridge_reset(NULL);
    ck_assert_int_ne(rc, 0);
}
END_TEST

/* ============================================================================
 * Financial Ethics Bridge Tests - Subsystem Setters
 * ============================================================================ */

START_TEST(test_ethics_set_immune)
{
    int dummy_immune = 42;
    int rc = financial_ethics_bridge_set_immune(g_ethics_bridge, &dummy_immune);
    ck_assert_int_eq(rc, 0);

    /* Set to NULL should also work */
    rc = financial_ethics_bridge_set_immune(g_ethics_bridge, NULL);
    ck_assert_int_eq(rc, 0);
}
END_TEST

START_TEST(test_ethics_set_immune_null_bridge)
{
    int dummy_immune = 42;
    int rc = financial_ethics_bridge_set_immune(NULL, &dummy_immune);
    ck_assert_int_ne(rc, 0);
}
END_TEST

START_TEST(test_ethics_set_bbb)
{
    /* bbb_system_t is a pointer type, so passing NULL is valid */
    int rc = financial_ethics_bridge_set_bbb(g_ethics_bridge, NULL);
    ck_assert_int_eq(rc, 0);
}
END_TEST

START_TEST(test_ethics_set_health_agent)
{
    int dummy_agent = 99;
    int rc = financial_ethics_bridge_set_health_agent(g_ethics_bridge, &dummy_agent);
    ck_assert_int_eq(rc, 0);
}
END_TEST

START_TEST(test_ethics_set_kg_wiring)
{
    int dummy_kg = 77;
    int rc = financial_ethics_bridge_set_kg_wiring(g_ethics_bridge, &dummy_kg);
    ck_assert_int_eq(rc, 0);
}
END_TEST

START_TEST(test_ethics_set_logger)
{
    int dummy_logger = 88;
    int rc = financial_ethics_bridge_set_logger(g_ethics_bridge, &dummy_logger);
    ck_assert_int_eq(rc, 0);
}
END_TEST

/* ============================================================================
 * Financial Ethics Bridge Tests - Core Ethics API
 * ============================================================================ */

START_TEST(test_ethics_evaluate_action_buy)
{
    fin_ethics_action_t action = {
        .action_type = FIN_ACTION_BUY,
        .action_magnitude = 0.3f,
        .position_size = 10000.0f
    };
    strncpy(action.target_asset, "AAPL", FIN_ETHICS_ASSET_LEN - 1);
    strncpy(action.context, "Regular buy order", FIN_ETHICS_CONTEXT_LEN - 1);

    fin_ethics_result_t result;
    int rc = financial_ethics_bridge_evaluate(g_ethics_bridge, &action, &result);
    ck_assert_int_eq(rc, 0);

    /* Normal buy should be approved or at least not denied */
    ck_assert_int_ne(result.verdict, FIN_ETHICS_DENIED);
    ck_assert(result.harm_score >= 0.0f && result.harm_score <= 1.0f);
}
END_TEST

START_TEST(test_ethics_evaluate_action_spoofing)
{
    fin_ethics_action_t action = {
        .action_type = FIN_ACTION_SPOOFING,
        .action_magnitude = 0.8f,
        .position_size = 50000.0f
    };
    strncpy(action.target_asset, "SPY", FIN_ETHICS_ASSET_LEN - 1);
    strncpy(action.context, "Spoofing attempt", FIN_ETHICS_CONTEXT_LEN - 1);

    fin_ethics_result_t result;
    int rc = financial_ethics_bridge_evaluate(g_ethics_bridge, &action, &result);
    ck_assert_int_eq(rc, 0);

    /* Spoofing should be denied or escalated */
    ck_assert(result.verdict == FIN_ETHICS_DENIED || result.verdict == FIN_ETHICS_ESCALATE);
}
END_TEST

START_TEST(test_ethics_evaluate_null_params)
{
    fin_ethics_action_t action = {0};
    fin_ethics_result_t result;

    int rc = financial_ethics_bridge_evaluate(NULL, &action, &result);
    ck_assert_int_ne(rc, 0);

    rc = financial_ethics_bridge_evaluate(g_ethics_bridge, NULL, &result);
    ck_assert_int_ne(rc, 0);

    rc = financial_ethics_bridge_evaluate(g_ethics_bridge, &action, NULL);
    ck_assert_int_ne(rc, 0);
}
END_TEST

START_TEST(test_ethics_assess_harm)
{
    fin_ethics_action_t action = {
        .action_type = FIN_ACTION_SHORT,
        .action_magnitude = 0.6f,
        .position_size = 25000.0f
    };
    strncpy(action.target_asset, "GME", FIN_ETHICS_ASSET_LEN - 1);

    fin_harm_assessment_t assessment;
    int rc = financial_ethics_bridge_assess_harm(g_ethics_bridge, &action, &assessment);
    ck_assert_int_eq(rc, 0);

    /* Verify harm scores are within bounds */
    ck_assert(assessment.total_harm >= 0.0f && assessment.total_harm <= 1.0f);
    ck_assert(assessment.counterparty_harm >= 0.0f && assessment.counterparty_harm <= 1.0f);
    ck_assert(assessment.market_harm >= 0.0f && assessment.market_harm <= 1.0f);
    ck_assert(assessment.systemic_harm >= 0.0f && assessment.systemic_harm <= 1.0f);
}
END_TEST

START_TEST(test_ethics_assess_harm_null)
{
    fin_ethics_action_t action = {0};
    fin_harm_assessment_t assessment;

    int rc = financial_ethics_bridge_assess_harm(NULL, &action, &assessment);
    ck_assert_int_ne(rc, 0);
}
END_TEST

START_TEST(test_ethics_golden_rule)
{
    fin_ethics_action_t action = {
        .action_type = FIN_ACTION_MARKET_MAKE,
        .action_magnitude = 0.5f,
        .position_size = 15000.0f
    };
    strncpy(action.target_asset, "MSFT", FIN_ETHICS_ASSET_LEN - 1);

    fin_golden_rule_result_t result;
    int rc = financial_ethics_bridge_golden_rule(g_ethics_bridge, &action, &result);
    ck_assert_int_eq(rc, 0);

    /* Market making should pass Golden Rule (fair activity) */
    ck_assert(result.reciprocity_score >= 0.0f && result.reciprocity_score <= 1.0f);
    ck_assert(result.fairness_score >= 0.0f && result.fairness_score <= 1.0f);
}
END_TEST

START_TEST(test_ethics_golden_rule_wash_trade)
{
    fin_ethics_action_t action = {
        .action_type = FIN_ACTION_WASH_TRADE,
        .action_magnitude = 0.7f,
        .position_size = 30000.0f
    };
    strncpy(action.target_asset, "BTC", FIN_ETHICS_ASSET_LEN - 1);

    fin_golden_rule_result_t result;
    int rc = financial_ethics_bridge_golden_rule(g_ethics_bridge, &action, &result);
    ck_assert_int_eq(rc, 0);

    /* Wash trading should fail Golden Rule */
    ck_assert(!result.passes);
}
END_TEST

START_TEST(test_ethics_check_asimov)
{
    fin_ethics_action_t action = {
        .action_type = FIN_ACTION_BUY,
        .action_magnitude = 0.2f,
        .position_size = 5000.0f
    };
    strncpy(action.target_asset, "GOOG", FIN_ETHICS_ASSET_LEN - 1);

    fin_asimov_result_t result;
    int rc = financial_ethics_bridge_check_asimov(g_ethics_bridge, &action, &result);
    ck_assert_int_eq(rc, 0);

    /* Normal buy should be Asimov compliant */
    ck_assert(result.compliant);
    ck_assert_int_eq(result.primary_violation, FIN_ASIMOV_NONE);
}
END_TEST

START_TEST(test_ethics_check_asimov_stop_hunt)
{
    fin_ethics_action_t action = {
        .action_type = FIN_ACTION_STOP_HUNT,
        .action_magnitude = 0.9f,
        .position_size = 100000.0f
    };
    strncpy(action.target_asset, "ES", FIN_ETHICS_ASSET_LEN - 1);

    fin_asimov_result_t result;
    int rc = financial_ethics_bridge_check_asimov(g_ethics_bridge, &action, &result);
    ck_assert_int_eq(rc, 0);

    /* Stop hunting violates First Law (harm to participants) */
    ck_assert(!result.compliant);
    ck_assert_int_eq(result.primary_violation, FIN_ASIMOV_FIRST_LAW);
}
END_TEST

START_TEST(test_ethics_compute_empathy)
{
    fin_ethics_action_t action = {
        .action_type = FIN_ACTION_SELL,
        .action_magnitude = 0.4f,
        .position_size = 8000.0f
    };
    strncpy(action.target_asset, "AMZN", FIN_ETHICS_ASSET_LEN - 1);

    float empathy_score = -1.0f;
    int rc = financial_ethics_bridge_compute_empathy(g_ethics_bridge, &action, &empathy_score);
    ck_assert_int_eq(rc, 0);

    ck_assert(empathy_score >= 0.0f && empathy_score <= 1.0f);
}
END_TEST

/* ============================================================================
 * Financial Ethics Bridge Tests - Query API
 * ============================================================================ */

START_TEST(test_ethics_get_stats)
{
    fin_ethics_bridge_stats_t stats;
    int rc = financial_ethics_bridge_get_stats(g_ethics_bridge, &stats);
    ck_assert_int_eq(rc, 0);

    /* Initial stats should be zero */
    ck_assert_uint_eq(stats.evaluations, 0);
    ck_assert_uint_eq(stats.approvals, 0);
    ck_assert_uint_eq(stats.denials, 0);
}
END_TEST

START_TEST(test_ethics_get_stats_after_eval)
{
    /* Perform an evaluation */
    fin_ethics_action_t action = {
        .action_type = FIN_ACTION_BUY,
        .action_magnitude = 0.3f,
        .position_size = 10000.0f
    };
    fin_ethics_result_t result;
    financial_ethics_bridge_evaluate(g_ethics_bridge, &action, &result);

    /* Check stats incremented */
    fin_ethics_bridge_stats_t stats;
    int rc = financial_ethics_bridge_get_stats(g_ethics_bridge, &stats);
    ck_assert_int_eq(rc, 0);
    ck_assert_uint_ge(stats.evaluations, 1);
}
END_TEST

START_TEST(test_ethics_reset_stats)
{
    /* Perform evaluation */
    fin_ethics_action_t action = { .action_type = FIN_ACTION_BUY };
    fin_ethics_result_t result;
    financial_ethics_bridge_evaluate(g_ethics_bridge, &action, &result);

    /* Reset */
    financial_ethics_bridge_reset_stats(g_ethics_bridge);

    fin_ethics_bridge_stats_t stats;
    financial_ethics_bridge_get_stats(g_ethics_bridge, &stats);
    ck_assert_uint_eq(stats.evaluations, 0);
}
END_TEST

START_TEST(test_ethics_version)
{
    const char* version = financial_ethics_bridge_version();
    ck_assert_ptr_nonnull(version);
    ck_assert_str_eq(version, FINANCIAL_ETHICS_BRIDGE_VERSION);
}
END_TEST

START_TEST(test_ethics_utility_names)
{
    /* Test verdict names */
    ck_assert_ptr_nonnull(fin_ethics_verdict_name(FIN_ETHICS_APPROVED));
    ck_assert_ptr_nonnull(fin_ethics_verdict_name(FIN_ETHICS_DENIED));
    ck_assert_ptr_nonnull(fin_ethics_verdict_name(FIN_ETHICS_ESCALATE));
    ck_assert_ptr_nonnull(fin_ethics_verdict_name(FIN_ETHICS_WARN));

    /* Test action names */
    ck_assert_ptr_nonnull(fin_ethics_action_name(FIN_ACTION_BUY));
    ck_assert_ptr_nonnull(fin_ethics_action_name(FIN_ACTION_SPOOFING));

    /* Test Asimov law names */
    ck_assert_ptr_nonnull(fin_ethics_asimov_name(FIN_ASIMOV_NONE));
    ck_assert_ptr_nonnull(fin_ethics_asimov_name(FIN_ASIMOV_FIRST_LAW));
}
END_TEST

/* ============================================================================
 * Financial Explanations Bridge Tests - Lifecycle
 * ============================================================================ */

START_TEST(test_expl_default_config)
{
    fin_explanations_config_t config;
    int rc = financial_explanations_bridge_default_config(&config);
    ck_assert_int_eq(rc, 0);

    ck_assert(config.include_confidence);
    ck_assert(config.max_reasoning_steps > 0);
}
END_TEST

START_TEST(test_expl_default_config_null)
{
    int rc = financial_explanations_bridge_default_config(NULL);
    ck_assert_int_eq(rc, -1);
}
END_TEST

START_TEST(test_expl_create_destroy)
{
    fin_explanations_config_t config;
    financial_explanations_bridge_default_config(&config);

    financial_explanations_bridge_t* bridge = financial_explanations_bridge_create(&config);
    ck_assert_ptr_nonnull(bridge);

    fin_explanations_bridge_state_t state = financial_explanations_bridge_get_state(bridge);
    ck_assert_int_eq(state, FIN_EXPL_STATE_INITIALIZED);

    financial_explanations_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_expl_destroy_null)
{
    financial_explanations_bridge_destroy(NULL);
}
END_TEST

START_TEST(test_expl_reset)
{
    int rc = financial_explanations_bridge_reset(g_explanations_bridge);
    ck_assert_int_eq(rc, 0);
}
END_TEST

/* ============================================================================
 * Financial Explanations Bridge Tests - Subsystem Setters
 * ============================================================================ */

START_TEST(test_expl_set_immune)
{
    int dummy = 1;
    int rc = financial_explanations_bridge_set_immune(g_explanations_bridge, &dummy);
    ck_assert_int_eq(rc, 0);
}
END_TEST

START_TEST(test_expl_set_bbb)
{
    int rc = financial_explanations_bridge_set_bbb(g_explanations_bridge, NULL);
    ck_assert_int_eq(rc, 0);
}
END_TEST

START_TEST(test_expl_set_health_agent)
{
    int dummy = 2;
    int rc = financial_explanations_bridge_set_health_agent(g_explanations_bridge, &dummy);
    ck_assert_int_eq(rc, 0);
}
END_TEST

/* ============================================================================
 * Financial Explanations Bridge Tests - Core API
 * ============================================================================ */

START_TEST(test_expl_explain_decision_buy)
{
    fin_decision_record_t decision = {
        .decision_type = FIN_DECISION_BUY,
        .magnitude = 0.4f
    };
    strncpy(decision.asset, "TSLA", FIN_EXPL_ASSET_LEN - 1);
    strncpy(decision.rationale, "Strong momentum", FIN_EXPL_RATIONALE_LEN - 1);

    fin_explanation_t explanation = {0};
    int rc = financial_explanations_bridge_explain_decision(
        g_explanations_bridge, &decision, FIN_EXPL_LEVEL_STANDARD, &explanation
    );
    ck_assert_int_eq(rc, 0);

    /* Should have summary populated */
    ck_assert(strlen(explanation.summary) > 0);
    ck_assert(explanation.confidence >= 0.0f && explanation.confidence <= 1.0f);

    /* Free resources */
    financial_explanations_bridge_free_explanation(&explanation);
}
END_TEST

START_TEST(test_expl_explain_decision_detailed)
{
    fin_decision_record_t decision = {
        .decision_type = FIN_DECISION_SELL,
        .magnitude = 0.7f
    };
    strncpy(decision.asset, "NVDA", FIN_EXPL_ASSET_LEN - 1);
    strncpy(decision.rationale, "Target reached", FIN_EXPL_RATIONALE_LEN - 1);

    fin_explanation_t explanation = {0};
    int rc = financial_explanations_bridge_explain_decision(
        g_explanations_bridge, &decision, FIN_EXPL_LEVEL_DETAILED, &explanation
    );
    ck_assert_int_eq(rc, 0);

    /* Detailed level should have reasoning steps */
    ck_assert(explanation.num_steps > 0);

    financial_explanations_bridge_free_explanation(&explanation);
}
END_TEST

START_TEST(test_expl_explain_decision_null)
{
    fin_decision_record_t decision = {0};
    fin_explanation_t explanation;

    int rc = financial_explanations_bridge_explain_decision(NULL, &decision, FIN_EXPL_LEVEL_BRIEF, &explanation);
    ck_assert_int_ne(rc, 0);

    rc = financial_explanations_bridge_explain_decision(g_explanations_bridge, NULL, FIN_EXPL_LEVEL_BRIEF, &explanation);
    ck_assert_int_ne(rc, 0);
}
END_TEST

START_TEST(test_expl_audit_trail)
{
    fin_decision_record_t decision = {
        .decision_type = FIN_DECISION_HEDGE,
        .magnitude = 0.5f
    };
    strncpy(decision.asset, "VIX", FIN_EXPL_ASSET_LEN - 1);

    fin_audit_entry_t entry;
    int rc = financial_explanations_bridge_audit_trail(
        g_explanations_bridge, &decision, NULL, FIN_AUDIT_DECISION, &entry
    );
    ck_assert_int_eq(rc, 0);

    ck_assert_uint_gt(entry.timestamp_ns, 0);
    ck_assert_int_eq(entry.audit_type, FIN_AUDIT_DECISION);
}
END_TEST

START_TEST(test_expl_audit_trail_with_explanation)
{
    fin_decision_record_t decision = {
        .decision_type = FIN_DECISION_STOP_LOSS,
        .magnitude = 1.0f
    };
    strncpy(decision.asset, "SPY", FIN_EXPL_ASSET_LEN - 1);

    /* Generate explanation first */
    fin_explanation_t explanation = {0};
    financial_explanations_bridge_explain_decision(
        g_explanations_bridge, &decision, FIN_EXPL_LEVEL_STANDARD, &explanation
    );

    /* Create audit trail with explanation */
    fin_audit_entry_t entry;
    int rc = financial_explanations_bridge_audit_trail(
        g_explanations_bridge, &decision, &explanation, FIN_AUDIT_RISK_CHECK, &entry
    );
    ck_assert_int_eq(rc, 0);

    ck_assert(strlen(entry.explanation_summary) > 0);

    financial_explanations_bridge_free_explanation(&explanation);
}
END_TEST

START_TEST(test_expl_free_explanation_null)
{
    /* Should not crash */
    financial_explanations_bridge_free_explanation(NULL);
}
END_TEST

/* ============================================================================
 * Financial Explanations Bridge Tests - Query API
 * ============================================================================ */

START_TEST(test_expl_get_stats)
{
    fin_explanations_bridge_stats_t stats;
    int rc = financial_explanations_bridge_get_stats(g_explanations_bridge, &stats);
    ck_assert_int_eq(rc, 0);
    ck_assert_uint_eq(stats.explanations_generated, 0);
}
END_TEST

START_TEST(test_expl_get_audit_count)
{
    uint64_t count = financial_explanations_bridge_get_audit_count(g_explanations_bridge);
    ck_assert_uint_eq(count, 0);

    /* Add an audit entry */
    fin_decision_record_t decision = { .decision_type = FIN_DECISION_BUY };
    fin_audit_entry_t entry;
    financial_explanations_bridge_audit_trail(g_explanations_bridge, &decision, NULL, FIN_AUDIT_DECISION, &entry);

    count = financial_explanations_bridge_get_audit_count(g_explanations_bridge);
    ck_assert_uint_ge(count, 1);
}
END_TEST

START_TEST(test_expl_version)
{
    const char* version = financial_explanations_bridge_version();
    ck_assert_ptr_nonnull(version);
    ck_assert_str_eq(version, FINANCIAL_EXPLANATIONS_BRIDGE_VERSION);
}
END_TEST

START_TEST(test_expl_utility_names)
{
    ck_assert_ptr_nonnull(fin_expl_decision_name(FIN_DECISION_BUY));
    ck_assert_ptr_nonnull(fin_expl_level_name(FIN_EXPL_LEVEL_DETAILED));
    ck_assert_ptr_nonnull(fin_expl_audit_name(FIN_AUDIT_DECISION));
    ck_assert_ptr_nonnull(fin_expl_state_name(FIN_EXPL_STATE_ACTIVE));
}
END_TEST

/* ============================================================================
 * Financial Consolidation Bridge Tests - Lifecycle
 * ============================================================================ */

START_TEST(test_consol_default_config)
{
    fin_consolidation_config_t config;
    int rc = financial_consolidation_bridge_default_config(&config);
    ck_assert_int_eq(rc, 0);

    ck_assert_uint_gt(config.replay_batch_size, 0);
    ck_assert(config.strengthen_rate > 0.0f);
    ck_assert(config.prune_threshold > 0.0f);
}
END_TEST

START_TEST(test_consol_default_config_null)
{
    int rc = financial_consolidation_bridge_default_config(NULL);
    ck_assert_int_ne(rc, 0);
}
END_TEST

START_TEST(test_consol_create_destroy)
{
    fin_consolidation_config_t config;
    financial_consolidation_bridge_default_config(&config);

    financial_consolidation_bridge_t* bridge = financial_consolidation_bridge_create(&config);
    ck_assert_ptr_nonnull(bridge);

    fin_consolidation_op_state_t state = financial_consolidation_bridge_get_op_state(bridge);
    ck_assert_int_eq(state, FIN_CONSOLIDATION_OP_STATE_INITIALIZED);

    financial_consolidation_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_consol_destroy_null)
{
    financial_consolidation_bridge_destroy(NULL);
}
END_TEST

START_TEST(test_consol_reset)
{
    int rc = financial_consolidation_bridge_reset(g_consolidation_bridge);
    ck_assert_int_eq(rc, 0);
}
END_TEST

/* ============================================================================
 * Financial Consolidation Bridge Tests - Trade History
 * ============================================================================ */

START_TEST(test_consol_add_trade)
{
    fin_trade_record_t trade = {
        .price = 150.0f,
        .quantity = 100.0f,
        .direction = FIN_TRADE_DIRECTION_LONG,
        .outcome = 500.0f,
        .timestamp_ms = 1000000ULL
    };

    int rc = financial_consolidation_bridge_add_trade(g_consolidation_bridge, &trade);
    ck_assert_int_eq(rc, 0);

    uint32_t count = financial_consolidation_bridge_get_trade_count(g_consolidation_bridge);
    ck_assert_uint_eq(count, 1);
}
END_TEST

START_TEST(test_consol_add_trade_null)
{
    int rc = financial_consolidation_bridge_add_trade(g_consolidation_bridge, NULL);
    ck_assert_int_ne(rc, 0);

    fin_trade_record_t trade = {0};
    rc = financial_consolidation_bridge_add_trade(NULL, &trade);
    ck_assert_int_ne(rc, 0);
}
END_TEST

START_TEST(test_consol_add_multiple_trades)
{
    for (int i = 0; i < 10; i++) {
        fin_trade_record_t trade = {
            .price = 100.0f + i * 5.0f,
            .quantity = 50.0f,
            .direction = (i % 2 == 0) ? FIN_TRADE_DIRECTION_LONG : FIN_TRADE_DIRECTION_SHORT,
            .outcome = (i % 3 == 0) ? 200.0f : -100.0f,
            .timestamp_ms = 1000000ULL + i * 60000ULL
        };
        int rc = financial_consolidation_bridge_add_trade(g_consolidation_bridge, &trade);
        ck_assert_int_eq(rc, 0);
    }

    uint32_t count = financial_consolidation_bridge_get_trade_count(g_consolidation_bridge);
    ck_assert_uint_eq(count, 10);
}
END_TEST

START_TEST(test_consol_clear_history)
{
    /* Add some trades */
    fin_trade_record_t trade = { .price = 100.0f, .direction = FIN_TRADE_DIRECTION_LONG };
    financial_consolidation_bridge_add_trade(g_consolidation_bridge, &trade);
    financial_consolidation_bridge_add_trade(g_consolidation_bridge, &trade);

    int rc = financial_consolidation_bridge_clear_history(g_consolidation_bridge);
    ck_assert_int_eq(rc, 0);

    uint32_t count = financial_consolidation_bridge_get_trade_count(g_consolidation_bridge);
    ck_assert_uint_eq(count, 0);
}
END_TEST

/* ============================================================================
 * Financial Consolidation Bridge Tests - Pattern Management
 * ============================================================================ */

START_TEST(test_consol_register_pattern)
{
    int pattern_id = financial_consolidation_bridge_register_pattern(
        g_consolidation_bridge, FIN_PATTERN_TYPE_MOMENTUM, 0.5f
    );
    ck_assert_int_ge(pattern_id, 0);

    uint32_t count = financial_consolidation_bridge_get_pattern_count(g_consolidation_bridge);
    ck_assert_uint_eq(count, 1);
}
END_TEST

START_TEST(test_consol_register_multiple_patterns)
{
    int id1 = financial_consolidation_bridge_register_pattern(
        g_consolidation_bridge, FIN_PATTERN_TYPE_MOMENTUM, 0.5f
    );
    int id2 = financial_consolidation_bridge_register_pattern(
        g_consolidation_bridge, FIN_PATTERN_TYPE_REVERSAL, 0.6f
    );
    int id3 = financial_consolidation_bridge_register_pattern(
        g_consolidation_bridge, FIN_PATTERN_TYPE_BREAKOUT, 0.7f
    );

    ck_assert_int_ge(id1, 0);
    ck_assert_int_ge(id2, 0);
    ck_assert_int_ge(id3, 0);
    ck_assert_int_ne(id1, id2);
    ck_assert_int_ne(id2, id3);

    uint32_t count = financial_consolidation_bridge_get_pattern_count(g_consolidation_bridge);
    ck_assert_uint_eq(count, 3);
}
END_TEST

START_TEST(test_consol_get_pattern)
{
    int pattern_id = financial_consolidation_bridge_register_pattern(
        g_consolidation_bridge, FIN_PATTERN_TYPE_VOLUME, 0.8f
    );

    fin_pattern_entry_t pattern;
    int rc = financial_consolidation_bridge_get_pattern(g_consolidation_bridge, pattern_id, &pattern);
    ck_assert_int_eq(rc, 0);

    ck_assert_int_eq(pattern.type, FIN_PATTERN_TYPE_VOLUME);
    ck_assert(fabsf(pattern.strength - 0.8f) < 0.001f);
}
END_TEST

/* ============================================================================
 * Financial Consolidation Bridge Tests - Consolidation Operations
 * ============================================================================ */

START_TEST(test_consol_replay_patterns)
{
    /* Register patterns and add trades */
    int pattern_id = financial_consolidation_bridge_register_pattern(
        g_consolidation_bridge, FIN_PATTERN_TYPE_MOMENTUM, 0.5f
    );

    fin_trade_record_t trade = {
        .price = 100.0f,
        .quantity = 50.0f,
        .direction = FIN_TRADE_DIRECTION_LONG,
        .outcome = 500.0f,
        .timestamp_ms = 1000000ULL
    };
    financial_consolidation_bridge_add_trade(g_consolidation_bridge, &trade);
    financial_consolidation_bridge_associate_pattern(g_consolidation_bridge, 0, pattern_id, 500.0f);

    fin_consolidation_result_t result;
    financial_consolidation_result_init(&result);

    int rc = financial_consolidation_bridge_replay(g_consolidation_bridge, &result);
    ck_assert_int_eq(rc, 0);

    financial_consolidation_result_free(&result);
}
END_TEST

START_TEST(test_consol_strengthen_winners)
{
    /* Create winning pattern */
    int pattern_id = financial_consolidation_bridge_register_pattern(
        g_consolidation_bridge, FIN_PATTERN_TYPE_BREAKOUT, 0.3f
    );

    /* Add winning trades */
    for (int i = 0; i < 5; i++) {
        fin_trade_record_t trade = {
            .price = 100.0f,
            .outcome = 1000.0f,  /* Large profit */
            .timestamp_ms = 1000000ULL + i * 1000
        };
        financial_consolidation_bridge_add_trade(g_consolidation_bridge, &trade);
        financial_consolidation_bridge_associate_pattern(g_consolidation_bridge, i, pattern_id, 1000.0f);
    }

    fin_consolidation_result_t result;
    financial_consolidation_result_init(&result);

    int rc = financial_consolidation_bridge_strengthen_winners(g_consolidation_bridge, &result);
    ck_assert_int_eq(rc, 0);

    /* Pattern should be stronger now */
    fin_pattern_entry_t pattern;
    financial_consolidation_bridge_get_pattern(g_consolidation_bridge, pattern_id, &pattern);
    ck_assert(pattern.strength > 0.3f);  /* Should have increased */

    financial_consolidation_result_free(&result);
}
END_TEST

START_TEST(test_consol_prune_losers)
{
    /* Create losing pattern */
    int pattern_id = financial_consolidation_bridge_register_pattern(
        g_consolidation_bridge, FIN_PATTERN_TYPE_SENTIMENT, 0.3f
    );

    /* Add losing trades */
    for (int i = 0; i < 5; i++) {
        fin_trade_record_t trade = {
            .price = 100.0f,
            .outcome = -500.0f,  /* Loss */
            .timestamp_ms = 1000000ULL + i * 1000
        };
        financial_consolidation_bridge_add_trade(g_consolidation_bridge, &trade);
        financial_consolidation_bridge_associate_pattern(g_consolidation_bridge, i, pattern_id, -500.0f);
    }

    fin_consolidation_result_t result;
    financial_consolidation_result_init(&result);

    int rc = financial_consolidation_bridge_prune_losers(g_consolidation_bridge, &result);
    ck_assert_int_eq(rc, 0);

    /* Pattern should be weaker or pruned */
    fin_pattern_entry_t pattern;
    financial_consolidation_bridge_get_pattern(g_consolidation_bridge, pattern_id, &pattern);
    ck_assert(pattern.strength <= 0.3f);  /* Should have decreased */

    financial_consolidation_result_free(&result);
}
END_TEST

START_TEST(test_consol_consolidate_full)
{
    /* Register patterns */
    financial_consolidation_bridge_register_pattern(g_consolidation_bridge, FIN_PATTERN_TYPE_MOMENTUM, 0.5f);
    financial_consolidation_bridge_register_pattern(g_consolidation_bridge, FIN_PATTERN_TYPE_REVERSAL, 0.5f);

    fin_consolidation_result_t result;
    financial_consolidation_result_init(&result);

    int rc = financial_consolidation_bridge_consolidate(
        g_consolidation_bridge, FIN_CONSOLIDATION_MODE_FULL, &result
    );
    ck_assert_int_eq(rc, 0);

    financial_consolidation_result_free(&result);
}
END_TEST

START_TEST(test_consol_version)
{
    const char* version = financial_consolidation_bridge_version();
    ck_assert_ptr_nonnull(version);
    ck_assert_str_eq(version, FINANCIAL_CONSOLIDATION_BRIDGE_VERSION);
}
END_TEST

/* ============================================================================
 * Financial STDP Bridge Tests - Lifecycle
 * ============================================================================ */

START_TEST(test_stdp_default_config)
{
    fin_stdp_config_t config;
    int rc = financial_stdp_bridge_default_config(&config);
    ck_assert_int_eq(rc, 0);

    ck_assert(config.tau_plus_ms > 0.0f);
    ck_assert(config.tau_minus_ms > 0.0f);
    ck_assert(config.a_plus > 0.0f);
    ck_assert(config.a_minus > 0.0f);
    ck_assert(config.learning_rate > 0.0f);
}
END_TEST

START_TEST(test_stdp_default_config_null)
{
    int rc = financial_stdp_bridge_default_config(NULL);
    ck_assert_int_ne(rc, 0);
}
END_TEST

START_TEST(test_stdp_create_destroy)
{
    fin_stdp_config_t config;
    financial_stdp_bridge_default_config(&config);

    financial_stdp_bridge_t* bridge = financial_stdp_bridge_create(&config);
    ck_assert_ptr_nonnull(bridge);

    fin_stdp_op_state_t state = financial_stdp_bridge_get_op_state(bridge);
    ck_assert_int_eq(state, FIN_STDP_OP_STATE_INITIALIZED);

    financial_stdp_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_stdp_destroy_null)
{
    financial_stdp_bridge_destroy(NULL);
}
END_TEST

START_TEST(test_stdp_reset)
{
    int rc = financial_stdp_bridge_reset(g_stdp_bridge);
    ck_assert_int_eq(rc, 0);
}
END_TEST

START_TEST(test_stdp_reset_weights)
{
    int rc = financial_stdp_bridge_reset_weights(g_stdp_bridge);
    ck_assert_int_eq(rc, 0);
}
END_TEST

/* ============================================================================
 * Financial STDP Bridge Tests - Signal Recording
 * ============================================================================ */

START_TEST(test_stdp_record_signal)
{
    fin_signal_t signal = {
        .signal_type = 1,
        .strength = 0.8f,
        .timestamp_ms = 1000000ULL
    };

    int rc = financial_stdp_bridge_record_signal(g_stdp_bridge, &signal);
    ck_assert_int_eq(rc, 0);
}
END_TEST

START_TEST(test_stdp_record_signal_null)
{
    int rc = financial_stdp_bridge_record_signal(g_stdp_bridge, NULL);
    ck_assert_int_ne(rc, 0);

    fin_signal_t signal = {0};
    rc = financial_stdp_bridge_record_signal(NULL, &signal);
    ck_assert_int_ne(rc, 0);
}
END_TEST

START_TEST(test_stdp_record_multiple_signals)
{
    for (int i = 0; i < 10; i++) {
        fin_signal_t signal = {
            .signal_type = i % 5,
            .strength = 0.5f + i * 0.05f,
            .timestamp_ms = 1000000ULL + i * 100
        };
        int rc = financial_stdp_bridge_record_signal(g_stdp_bridge, &signal);
        ck_assert_int_eq(rc, 0);
    }
}
END_TEST

START_TEST(test_stdp_clear_signals)
{
    /* Record signals */
    fin_signal_t signal = { .signal_type = 1, .strength = 0.5f };
    financial_stdp_bridge_record_signal(g_stdp_bridge, &signal);

    int rc = financial_stdp_bridge_clear_signals(g_stdp_bridge);
    ck_assert_int_eq(rc, 0);
}
END_TEST

/* ============================================================================
 * Financial STDP Bridge Tests - Learning
 * ============================================================================ */

START_TEST(test_stdp_learn_correlation)
{
    fin_signal_t signal = {
        .signal_type = 1,
        .strength = 0.9f,
        .timestamp_ms = 1000000ULL
    };

    fin_outcome_t outcome = {
        .outcome = 1.0f,  /* Positive outcome */
        .timestamp_ms = 1000050ULL  /* 50ms after signal */
    };

    int rc = financial_stdp_bridge_learn_correlation(g_stdp_bridge, &signal, &outcome);
    ck_assert_int_eq(rc, 0);
}
END_TEST

START_TEST(test_stdp_learn_correlation_null)
{
    fin_signal_t signal = {0};
    fin_outcome_t outcome = {0};

    int rc = financial_stdp_bridge_learn_correlation(NULL, &signal, &outcome);
    ck_assert_int_ne(rc, 0);

    rc = financial_stdp_bridge_learn_correlation(g_stdp_bridge, NULL, &outcome);
    ck_assert_int_ne(rc, 0);

    rc = financial_stdp_bridge_learn_correlation(g_stdp_bridge, &signal, NULL);
    ck_assert_int_ne(rc, 0);
}
END_TEST

START_TEST(test_stdp_update_from_trade)
{
    /* Record multiple signals first */
    for (int i = 0; i < 5; i++) {
        fin_signal_t signal = {
            .signal_type = i,
            .strength = 0.7f,
            .timestamp_ms = 1000000ULL + i * 10
        };
        financial_stdp_bridge_record_signal(g_stdp_bridge, &signal);
    }

    /* Now observe outcome */
    fin_outcome_t outcome = {
        .outcome = 0.5f,  /* Moderate positive */
        .timestamp_ms = 1000100ULL
    };

    int updated = financial_stdp_bridge_update_from_trade(g_stdp_bridge, &outcome);
    ck_assert_int_ge(updated, 0);
}
END_TEST

START_TEST(test_stdp_batch_learn)
{
    fin_signal_t signals[5] = {
        { .signal_type = 0, .strength = 0.6f, .timestamp_ms = 100 },
        { .signal_type = 1, .strength = 0.7f, .timestamp_ms = 200 },
        { .signal_type = 2, .strength = 0.8f, .timestamp_ms = 300 },
        { .signal_type = 3, .strength = 0.9f, .timestamp_ms = 400 },
        { .signal_type = 4, .strength = 0.5f, .timestamp_ms = 500 }
    };

    fin_outcome_t outcomes[5] = {
        { .outcome = 0.5f, .timestamp_ms = 150 },
        { .outcome = -0.2f, .timestamp_ms = 250 },
        { .outcome = 0.8f, .timestamp_ms = 350 },
        { .outcome = -0.5f, .timestamp_ms = 450 },
        { .outcome = 0.3f, .timestamp_ms = 550 }
    };

    int rc = financial_stdp_bridge_batch_learn(g_stdp_bridge, signals, outcomes, 5);
    ck_assert_int_eq(rc, 0);
}
END_TEST

/* ============================================================================
 * Financial STDP Bridge Tests - Weight Access
 * ============================================================================ */

START_TEST(test_stdp_get_weight)
{
    /* Learn a correlation first */
    fin_signal_t signal = { .signal_type = 5, .strength = 0.8f, .timestamp_ms = 1000 };
    fin_outcome_t outcome = { .outcome = 1.0f, .timestamp_ms = 1050 };
    financial_stdp_bridge_learn_correlation(g_stdp_bridge, &signal, &outcome);

    float weight = 0.0f;
    int rc = financial_stdp_bridge_get_weight(g_stdp_bridge, 5, &weight);
    ck_assert_int_eq(rc, 0);

    /* Weight should be positive after LTP */
    ck_assert(weight >= 0.0f);
}
END_TEST

START_TEST(test_stdp_get_correlation)
{
    /* Learn a correlation */
    fin_signal_t signal = { .signal_type = 7, .strength = 0.9f, .timestamp_ms = 2000 };
    fin_outcome_t outcome = { .outcome = 0.8f, .timestamp_ms = 2030 };
    financial_stdp_bridge_learn_correlation(g_stdp_bridge, &signal, &outcome);

    fin_stdp_correlation_t correlation;
    int rc = financial_stdp_bridge_get_correlation(g_stdp_bridge, 7, &correlation);
    ck_assert_int_eq(rc, 0);

    ck_assert_int_eq(correlation.signal_type, 7);
    ck_assert_uint_ge(correlation.update_count, 1);
}
END_TEST

START_TEST(test_stdp_get_stats)
{
    fin_stdp_bridge_stats_t stats;
    int rc = financial_stdp_bridge_get_stats(g_stdp_bridge, &stats);
    ck_assert_int_eq(rc, 0);
}
END_TEST

START_TEST(test_stdp_version)
{
    const char* version = financial_stdp_bridge_version();
    ck_assert_ptr_nonnull(version);
    ck_assert_str_eq(version, FINANCIAL_STDP_BRIDGE_VERSION);
}
END_TEST

/* ============================================================================
 * Financial Temporal Credit Bridge Tests - Lifecycle
 * ============================================================================ */

START_TEST(test_tcredit_default_config)
{
    fin_temporal_credit_config_t config;
    int rc = financial_temporal_credit_bridge_default_config(&config);
    ck_assert_int_eq(rc, 0);

    ck_assert(config.lambda > 0.0f && config.lambda <= 1.0f);
    ck_assert(config.gamma > 0.0f && config.gamma <= 1.0f);
    ck_assert(config.learning_rate > 0.0f);
}
END_TEST

START_TEST(test_tcredit_default_config_null)
{
    int rc = financial_temporal_credit_bridge_default_config(NULL);
    ck_assert_int_ne(rc, 0);
}
END_TEST

START_TEST(test_tcredit_create_destroy)
{
    fin_temporal_credit_config_t config;
    financial_temporal_credit_bridge_default_config(&config);

    financial_temporal_credit_bridge_t* bridge = financial_temporal_credit_bridge_create(&config);
    ck_assert_ptr_nonnull(bridge);

    fin_temporal_credit_op_state_t state = financial_temporal_credit_bridge_get_op_state(bridge);
    ck_assert_int_eq(state, FIN_TEMPORAL_CREDIT_OP_STATE_INITIALIZED);

    financial_temporal_credit_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_tcredit_destroy_null)
{
    financial_temporal_credit_bridge_destroy(NULL);
}
END_TEST

START_TEST(test_tcredit_reset)
{
    int rc = financial_temporal_credit_bridge_reset(g_temporal_credit_bridge);
    ck_assert_int_eq(rc, 0);
}
END_TEST

/* ============================================================================
 * Financial Temporal Credit Bridge Tests - Decision History
 * ============================================================================ */

START_TEST(test_tcredit_record_decision)
{
    fin_decision_t decision = {
        .decision_type = LOCAL_DECISION_TYPE_BUY,  /* Use local enum to avoid conflict */
        .magnitude = 10000.0f,
        .timestamp_ms = 1000000ULL,
        .eligibility = 1.0f
    };

    int rc = financial_temporal_credit_bridge_record_decision(g_temporal_credit_bridge, &decision);
    ck_assert_int_eq(rc, 0);

    uint32_t count = financial_temporal_credit_bridge_get_decision_count(g_temporal_credit_bridge);
    ck_assert_uint_eq(count, 1);
}
END_TEST

START_TEST(test_tcredit_record_decision_null)
{
    int rc = financial_temporal_credit_bridge_record_decision(g_temporal_credit_bridge, NULL);
    ck_assert_int_ne(rc, 0);

    fin_decision_t decision = {0};
    rc = financial_temporal_credit_bridge_record_decision(NULL, &decision);
    ck_assert_int_ne(rc, 0);
}
END_TEST

START_TEST(test_tcredit_record_multiple_decisions)
{
    for (int i = 0; i < 10; i++) {
        fin_decision_t decision = {
            .decision_type = i % 4,  /* Use raw integers to avoid enum conflicts */
            .magnitude = 5000.0f + i * 1000.0f,
            .timestamp_ms = 1000000ULL + i * 60000ULL,
            .eligibility = 1.0f
        };
        int rc = financial_temporal_credit_bridge_record_decision(g_temporal_credit_bridge, &decision);
        ck_assert_int_eq(rc, 0);
    }

    uint32_t count = financial_temporal_credit_bridge_get_decision_count(g_temporal_credit_bridge);
    ck_assert_uint_eq(count, 10);
}
END_TEST

START_TEST(test_tcredit_clear_history)
{
    /* Add decisions */
    fin_decision_t decision = { .decision_type = LOCAL_DECISION_TYPE_BUY, .timestamp_ms = 1000 };
    financial_temporal_credit_bridge_record_decision(g_temporal_credit_bridge, &decision);
    financial_temporal_credit_bridge_record_decision(g_temporal_credit_bridge, &decision);

    int rc = financial_temporal_credit_bridge_clear_history(g_temporal_credit_bridge);
    ck_assert_int_eq(rc, 0);

    uint32_t count = financial_temporal_credit_bridge_get_decision_count(g_temporal_credit_bridge);
    ck_assert_uint_eq(count, 0);
}
END_TEST

/* ============================================================================
 * Financial Temporal Credit Bridge Tests - Credit Assignment
 * ============================================================================ */

START_TEST(test_tcredit_assign)
{
    /* Record some decisions */
    for (int i = 0; i < 5; i++) {
        fin_decision_t decision = {
            .decision_type = LOCAL_DECISION_TYPE_BUY,
            .magnitude = 1000.0f,
            .timestamp_ms = 1000000ULL + i * 1000,
            .eligibility = 1.0f
        };
        financial_temporal_credit_bridge_record_decision(g_temporal_credit_bridge, &decision);
    }

    /* Assign credit based on outcome */
    fin_credit_assignment_result_t result;
    financial_temporal_credit_result_init(&result);

    int rc = financial_temporal_credit_bridge_assign(
        g_temporal_credit_bridge, 500.0f, 1010000ULL, &result
    );
    ck_assert_int_eq(rc, 0);

    ck_assert(result.outcome_value > 0.0f);

    financial_temporal_credit_result_free(&result);
}
END_TEST

START_TEST(test_tcredit_assign_null)
{
    fin_credit_assignment_result_t result;

    int rc = financial_temporal_credit_bridge_assign(NULL, 100.0f, 1000, &result);
    ck_assert_int_ne(rc, 0);

    rc = financial_temporal_credit_bridge_assign(g_temporal_credit_bridge, 100.0f, 1000, NULL);
    ck_assert_int_ne(rc, 0);
}
END_TEST

START_TEST(test_tcredit_eligibility_trace)
{
    /* Record decisions */
    for (int i = 0; i < 3; i++) {
        fin_decision_t decision = {
            .decision_type = LOCAL_DECISION_TYPE_HOLD,
            .magnitude = 0.0f,
            .timestamp_ms = 1000000ULL + i * 1000,
            .eligibility = 1.0f
        };
        financial_temporal_credit_bridge_record_decision(g_temporal_credit_bridge, &decision);
    }

    fin_eligibility_result_t result;
    financial_temporal_eligibility_result_init(&result);

    int rc = financial_temporal_credit_bridge_eligibility_trace(
        g_temporal_credit_bridge, 1005000ULL, &result
    );
    ck_assert_int_eq(rc, 0);

    ck_assert_uint_eq(result.num_traces, 3);

    financial_temporal_eligibility_result_free(&result);
}
END_TEST

START_TEST(test_tcredit_decay_traces)
{
    /* Record a decision */
    fin_decision_t decision = {
        .decision_type = LOCAL_DECISION_TYPE_BUY,
        .magnitude = 1000.0f,
        .timestamp_ms = 1000000ULL,
        .eligibility = 1.0f
    };
    financial_temporal_credit_bridge_record_decision(g_temporal_credit_bridge, &decision);

    /* Decay traces */
    int rc = financial_temporal_credit_bridge_decay_traces(g_temporal_credit_bridge, 1000);
    ck_assert_int_eq(rc, 0);

    /* Check eligibility has decayed */
    float elig = financial_temporal_credit_bridge_get_eligibility(g_temporal_credit_bridge, 0);
    ck_assert(elig < 1.0f);
}
END_TEST

START_TEST(test_tcredit_boost_eligibility)
{
    /* Record a decision */
    fin_decision_t decision = {
        .decision_type = LOCAL_DECISION_TYPE_SELL,
        .magnitude = 2000.0f,
        .timestamp_ms = 2000000ULL,
        .eligibility = 0.5f
    };
    financial_temporal_credit_bridge_record_decision(g_temporal_credit_bridge, &decision);

    /* Boost eligibility */
    int rc = financial_temporal_credit_bridge_boost_eligibility(g_temporal_credit_bridge, 0, 0.3f);
    ck_assert_int_eq(rc, 0);
}
END_TEST

/* ============================================================================
 * Financial Temporal Credit Bridge Tests - Configuration Updates
 * ============================================================================ */

START_TEST(test_tcredit_set_lambda)
{
    int rc = financial_temporal_credit_bridge_set_lambda(g_temporal_credit_bridge, 0.95f);
    ck_assert_int_eq(rc, 0);

    float lambda = financial_temporal_credit_bridge_get_lambda(g_temporal_credit_bridge);
    ck_assert(fabsf(lambda - 0.95f) < 0.001f);
}
END_TEST

START_TEST(test_tcredit_set_gamma)
{
    int rc = financial_temporal_credit_bridge_set_gamma(g_temporal_credit_bridge, 0.98f);
    ck_assert_int_eq(rc, 0);

    float gamma = financial_temporal_credit_bridge_get_gamma(g_temporal_credit_bridge);
    ck_assert(fabsf(gamma - 0.98f) < 0.001f);
}
END_TEST

START_TEST(test_tcredit_set_learning_rate)
{
    int rc = financial_temporal_credit_bridge_set_learning_rate(g_temporal_credit_bridge, 0.05f);
    ck_assert_int_eq(rc, 0);
}
END_TEST

START_TEST(test_tcredit_set_method)
{
    int rc = financial_temporal_credit_bridge_set_method(
        g_temporal_credit_bridge, FIN_CREDIT_METHOD_MONTE_CARLO
    );
    ck_assert_int_eq(rc, 0);
}
END_TEST

START_TEST(test_tcredit_get_stats)
{
    fin_temporal_credit_bridge_stats_t stats;
    int rc = financial_temporal_credit_bridge_get_stats(g_temporal_credit_bridge, &stats);
    ck_assert_int_eq(rc, 0);
}
END_TEST

START_TEST(test_tcredit_version)
{
    const char* version = financial_temporal_credit_bridge_version();
    ck_assert_ptr_nonnull(version);
    ck_assert_str_eq(version, FINANCIAL_TEMPORAL_CREDIT_BRIDGE_VERSION);
}
END_TEST

START_TEST(test_tcredit_utility_names)
{
    ck_assert_ptr_nonnull(fin_temporal_credit_op_state_name(FIN_TEMPORAL_CREDIT_OP_STATE_ACTIVE));
    ck_assert_ptr_nonnull(fin_temporal_credit_decision_type_name((fin_temporal_decision_type_t)LOCAL_DECISION_TYPE_BUY));
    ck_assert_ptr_nonnull(fin_temporal_credit_method_name(FIN_CREDIT_METHOD_TD_LAMBDA));
    ck_assert_ptr_nonnull(fin_temporal_credit_trace_replace_name(FIN_TRACE_REPLACE_ACCUMULATING));
}
END_TEST

/* ============================================================================
 * Test Suite Creation
 * ============================================================================ */

Suite* financial_ethics_suite(void)
{
    Suite* s = suite_create("Financial Ethics Bridge");

    /* Lifecycle tests */
    TCase* tc_lifecycle = tcase_create("Lifecycle");
    tcase_add_test(tc_lifecycle, test_ethics_default_config);
    tcase_add_test(tc_lifecycle, test_ethics_default_config_null);
    tcase_add_test(tc_lifecycle, test_ethics_create_destroy);
    tcase_add_test(tc_lifecycle, test_ethics_create_null_config);
    tcase_add_test(tc_lifecycle, test_ethics_destroy_null);
    tcase_add_checked_fixture(tc_lifecycle, setup_ethics, teardown_ethics);
    tcase_add_test(tc_lifecycle, test_ethics_reset);
    tcase_add_test(tc_lifecycle, test_ethics_reset_null);
    suite_add_tcase(s, tc_lifecycle);

    /* Subsystem setter tests */
    TCase* tc_setters = tcase_create("Subsystem Setters");
    tcase_add_checked_fixture(tc_setters, setup_ethics, teardown_ethics);
    tcase_add_test(tc_setters, test_ethics_set_immune);
    tcase_add_test(tc_setters, test_ethics_set_immune_null_bridge);
    tcase_add_test(tc_setters, test_ethics_set_bbb);
    tcase_add_test(tc_setters, test_ethics_set_health_agent);
    tcase_add_test(tc_setters, test_ethics_set_kg_wiring);
    tcase_add_test(tc_setters, test_ethics_set_logger);
    suite_add_tcase(s, tc_setters);

    /* Core ethics API tests */
    TCase* tc_ethics_api = tcase_create("Core Ethics API");
    tcase_add_checked_fixture(tc_ethics_api, setup_ethics, teardown_ethics);
    tcase_add_test(tc_ethics_api, test_ethics_evaluate_action_buy);
    tcase_add_test(tc_ethics_api, test_ethics_evaluate_action_spoofing);
    tcase_add_test(tc_ethics_api, test_ethics_evaluate_null_params);
    tcase_add_test(tc_ethics_api, test_ethics_assess_harm);
    tcase_add_test(tc_ethics_api, test_ethics_assess_harm_null);
    tcase_add_test(tc_ethics_api, test_ethics_golden_rule);
    tcase_add_test(tc_ethics_api, test_ethics_golden_rule_wash_trade);
    tcase_add_test(tc_ethics_api, test_ethics_check_asimov);
    tcase_add_test(tc_ethics_api, test_ethics_check_asimov_stop_hunt);
    tcase_add_test(tc_ethics_api, test_ethics_compute_empathy);
    suite_add_tcase(s, tc_ethics_api);

    /* Query API tests */
    TCase* tc_query = tcase_create("Query API");
    tcase_add_checked_fixture(tc_query, setup_ethics, teardown_ethics);
    tcase_add_test(tc_query, test_ethics_get_stats);
    tcase_add_test(tc_query, test_ethics_get_stats_after_eval);
    tcase_add_test(tc_query, test_ethics_reset_stats);
    tcase_add_test(tc_query, test_ethics_version);
    tcase_add_test(tc_query, test_ethics_utility_names);
    suite_add_tcase(s, tc_query);

    return s;
}

Suite* financial_explanations_suite(void)
{
    Suite* s = suite_create("Financial Explanations Bridge");

    /* Lifecycle tests */
    TCase* tc_lifecycle = tcase_create("Lifecycle");
    tcase_add_test(tc_lifecycle, test_expl_default_config);
    tcase_add_test(tc_lifecycle, test_expl_default_config_null);
    tcase_add_test(tc_lifecycle, test_expl_create_destroy);
    tcase_add_test(tc_lifecycle, test_expl_destroy_null);
    tcase_add_checked_fixture(tc_lifecycle, setup_explanations, teardown_explanations);
    tcase_add_test(tc_lifecycle, test_expl_reset);
    suite_add_tcase(s, tc_lifecycle);

    /* Subsystem setter tests */
    TCase* tc_setters = tcase_create("Subsystem Setters");
    tcase_add_checked_fixture(tc_setters, setup_explanations, teardown_explanations);
    tcase_add_test(tc_setters, test_expl_set_immune);
    tcase_add_test(tc_setters, test_expl_set_bbb);
    tcase_add_test(tc_setters, test_expl_set_health_agent);
    suite_add_tcase(s, tc_setters);

    /* Core API tests */
    TCase* tc_core = tcase_create("Core API");
    tcase_add_checked_fixture(tc_core, setup_explanations, teardown_explanations);
    tcase_add_test(tc_core, test_expl_explain_decision_buy);
    tcase_add_test(tc_core, test_expl_explain_decision_detailed);
    tcase_add_test(tc_core, test_expl_explain_decision_null);
    tcase_add_test(tc_core, test_expl_audit_trail);
    tcase_add_test(tc_core, test_expl_audit_trail_with_explanation);
    tcase_add_test(tc_core, test_expl_free_explanation_null);
    suite_add_tcase(s, tc_core);

    /* Query API tests */
    TCase* tc_query = tcase_create("Query API");
    tcase_add_checked_fixture(tc_query, setup_explanations, teardown_explanations);
    tcase_add_test(tc_query, test_expl_get_stats);
    tcase_add_test(tc_query, test_expl_get_audit_count);
    tcase_add_test(tc_query, test_expl_version);
    tcase_add_test(tc_query, test_expl_utility_names);
    suite_add_tcase(s, tc_query);

    return s;
}

Suite* financial_consolidation_suite(void)
{
    Suite* s = suite_create("Financial Consolidation Bridge");

    /* Lifecycle tests */
    TCase* tc_lifecycle = tcase_create("Lifecycle");
    tcase_add_test(tc_lifecycle, test_consol_default_config);
    tcase_add_test(tc_lifecycle, test_consol_default_config_null);
    tcase_add_test(tc_lifecycle, test_consol_create_destroy);
    tcase_add_test(tc_lifecycle, test_consol_destroy_null);
    tcase_add_checked_fixture(tc_lifecycle, setup_consolidation, teardown_consolidation);
    tcase_add_test(tc_lifecycle, test_consol_reset);
    suite_add_tcase(s, tc_lifecycle);

    /* Trade history tests */
    TCase* tc_history = tcase_create("Trade History");
    tcase_add_checked_fixture(tc_history, setup_consolidation, teardown_consolidation);
    tcase_add_test(tc_history, test_consol_add_trade);
    tcase_add_test(tc_history, test_consol_add_trade_null);
    tcase_add_test(tc_history, test_consol_add_multiple_trades);
    tcase_add_test(tc_history, test_consol_clear_history);
    suite_add_tcase(s, tc_history);

    /* Pattern management tests */
    TCase* tc_patterns = tcase_create("Pattern Management");
    tcase_add_checked_fixture(tc_patterns, setup_consolidation, teardown_consolidation);
    tcase_add_test(tc_patterns, test_consol_register_pattern);
    tcase_add_test(tc_patterns, test_consol_register_multiple_patterns);
    tcase_add_test(tc_patterns, test_consol_get_pattern);
    suite_add_tcase(s, tc_patterns);

    /* Consolidation operations tests */
    TCase* tc_consolidation = tcase_create("Consolidation Operations");
    tcase_add_checked_fixture(tc_consolidation, setup_consolidation, teardown_consolidation);
    tcase_add_test(tc_consolidation, test_consol_replay_patterns);
    tcase_add_test(tc_consolidation, test_consol_strengthen_winners);
    tcase_add_test(tc_consolidation, test_consol_prune_losers);
    tcase_add_test(tc_consolidation, test_consol_consolidate_full);
    tcase_add_test(tc_consolidation, test_consol_version);
    suite_add_tcase(s, tc_consolidation);

    return s;
}

Suite* financial_stdp_suite(void)
{
    Suite* s = suite_create("Financial STDP Bridge");

    /* Lifecycle tests */
    TCase* tc_lifecycle = tcase_create("Lifecycle");
    tcase_add_test(tc_lifecycle, test_stdp_default_config);
    tcase_add_test(tc_lifecycle, test_stdp_default_config_null);
    tcase_add_test(tc_lifecycle, test_stdp_create_destroy);
    tcase_add_test(tc_lifecycle, test_stdp_destroy_null);
    tcase_add_checked_fixture(tc_lifecycle, setup_stdp, teardown_stdp);
    tcase_add_test(tc_lifecycle, test_stdp_reset);
    tcase_add_test(tc_lifecycle, test_stdp_reset_weights);
    suite_add_tcase(s, tc_lifecycle);

    /* Signal recording tests */
    TCase* tc_signals = tcase_create("Signal Recording");
    tcase_add_checked_fixture(tc_signals, setup_stdp, teardown_stdp);
    tcase_add_test(tc_signals, test_stdp_record_signal);
    tcase_add_test(tc_signals, test_stdp_record_signal_null);
    tcase_add_test(tc_signals, test_stdp_record_multiple_signals);
    tcase_add_test(tc_signals, test_stdp_clear_signals);
    suite_add_tcase(s, tc_signals);

    /* Learning tests */
    TCase* tc_learning = tcase_create("Learning");
    tcase_add_checked_fixture(tc_learning, setup_stdp, teardown_stdp);
    tcase_add_test(tc_learning, test_stdp_learn_correlation);
    tcase_add_test(tc_learning, test_stdp_learn_correlation_null);
    tcase_add_test(tc_learning, test_stdp_update_from_trade);
    tcase_add_test(tc_learning, test_stdp_batch_learn);
    suite_add_tcase(s, tc_learning);

    /* Weight access tests */
    TCase* tc_weights = tcase_create("Weight Access");
    tcase_add_checked_fixture(tc_weights, setup_stdp, teardown_stdp);
    tcase_add_test(tc_weights, test_stdp_get_weight);
    tcase_add_test(tc_weights, test_stdp_get_correlation);
    tcase_add_test(tc_weights, test_stdp_get_stats);
    tcase_add_test(tc_weights, test_stdp_version);
    suite_add_tcase(s, tc_weights);

    return s;
}

Suite* financial_temporal_credit_suite(void)
{
    Suite* s = suite_create("Financial Temporal Credit Bridge");

    /* Lifecycle tests */
    TCase* tc_lifecycle = tcase_create("Lifecycle");
    tcase_add_test(tc_lifecycle, test_tcredit_default_config);
    tcase_add_test(tc_lifecycle, test_tcredit_default_config_null);
    tcase_add_test(tc_lifecycle, test_tcredit_create_destroy);
    tcase_add_test(tc_lifecycle, test_tcredit_destroy_null);
    tcase_add_checked_fixture(tc_lifecycle, setup_temporal_credit, teardown_temporal_credit);
    tcase_add_test(tc_lifecycle, test_tcredit_reset);
    suite_add_tcase(s, tc_lifecycle);

    /* Decision history tests */
    TCase* tc_history = tcase_create("Decision History");
    tcase_add_checked_fixture(tc_history, setup_temporal_credit, teardown_temporal_credit);
    tcase_add_test(tc_history, test_tcredit_record_decision);
    tcase_add_test(tc_history, test_tcredit_record_decision_null);
    tcase_add_test(tc_history, test_tcredit_record_multiple_decisions);
    tcase_add_test(tc_history, test_tcredit_clear_history);
    suite_add_tcase(s, tc_history);

    /* Credit assignment tests */
    TCase* tc_credit = tcase_create("Credit Assignment");
    tcase_add_checked_fixture(tc_credit, setup_temporal_credit, teardown_temporal_credit);
    tcase_add_test(tc_credit, test_tcredit_assign);
    tcase_add_test(tc_credit, test_tcredit_assign_null);
    tcase_add_test(tc_credit, test_tcredit_eligibility_trace);
    tcase_add_test(tc_credit, test_tcredit_decay_traces);
    tcase_add_test(tc_credit, test_tcredit_boost_eligibility);
    suite_add_tcase(s, tc_credit);

    /* Configuration tests */
    TCase* tc_config = tcase_create("Configuration");
    tcase_add_checked_fixture(tc_config, setup_temporal_credit, teardown_temporal_credit);
    tcase_add_test(tc_config, test_tcredit_set_lambda);
    tcase_add_test(tc_config, test_tcredit_set_gamma);
    tcase_add_test(tc_config, test_tcredit_set_learning_rate);
    tcase_add_test(tc_config, test_tcredit_set_method);
    tcase_add_test(tc_config, test_tcredit_get_stats);
    tcase_add_test(tc_config, test_tcredit_version);
    tcase_add_test(tc_config, test_tcredit_utility_names);
    suite_add_tcase(s, tc_config);

    return s;
}

int main(void)
{
    int number_failed = 0;
    SRunner* sr;

    /* Create runner with all suites */
    sr = srunner_create(financial_ethics_suite());
    srunner_add_suite(sr, financial_explanations_suite());
    srunner_add_suite(sr, financial_consolidation_suite());
    srunner_add_suite(sr, financial_stdp_suite());
    srunner_add_suite(sr, financial_temporal_credit_suite());

    /* Run all tests */
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
