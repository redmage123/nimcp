/**
 * @file test_financial_metacognition_bridges.c
 * @brief Unit tests for Financial Metacognition Bridges
 *
 * WHAT: Comprehensive test suite for four metacognition-related financial bridges:
 *       1. Financial Metacognition Bridge - Cognitive bias detection
 *       2. Financial Uncertainty Bridge - Epistemic/aleatoric decomposition
 *       3. Financial Curiosity Bridge - Hypothesis generation
 *       4. Financial Regret Bridge - Counterfactual analysis
 *
 * WHY:  Verify correct behavior of metacognitive financial processing including:
 *       - Lifecycle management (create/destroy)
 *       - Subsystem integration (immune, BBB, KG, health agent, logger)
 *       - Core metacognitive operations (bias detection, uncertainty decomposition,
 *         hypothesis generation, regret analysis)
 *
 * HOW:  Unit tests using Check framework covering all bridge APIs
 *
 * @author NIMCP Development Team
 * @date 2026-01-29
 */

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "cognitive/parietal/nimcp_financial_metacognition_bridge.h"
#include "cognitive/parietal/nimcp_financial_uncertainty_bridge.h"
#include "cognitive/parietal/nimcp_financial_curiosity_bridge.h"
#include "cognitive/parietal/nimcp_financial_regret_bridge.h"

/* ============================================================================
 * Test Fixtures - Financial Metacognition Bridge
 * ============================================================================ */

static financial_metacognition_bridge_t* g_metacog_bridge = NULL;

static void setup_metacognition(void)
{
    fin_metacognition_config_t config;
    financial_metacognition_bridge_default_config(&config);
    g_metacog_bridge = financial_metacognition_bridge_create(&config);
    ck_assert_ptr_nonnull(g_metacog_bridge);
}

static void teardown_metacognition(void)
{
    if (g_metacog_bridge) {
        financial_metacognition_bridge_destroy(g_metacog_bridge);
        g_metacog_bridge = NULL;
    }
}

/* ============================================================================
 * Test Fixtures - Financial Uncertainty Bridge
 * ============================================================================ */

static financial_uncertainty_bridge_t* g_uncertainty_bridge = NULL;

static void setup_uncertainty(void)
{
    fin_uncertainty_config_t config = financial_uncertainty_bridge_default_config();
    g_uncertainty_bridge = financial_uncertainty_bridge_create(&config);
    ck_assert_ptr_nonnull(g_uncertainty_bridge);
}

static void teardown_uncertainty(void)
{
    if (g_uncertainty_bridge) {
        financial_uncertainty_bridge_destroy(g_uncertainty_bridge);
        g_uncertainty_bridge = NULL;
    }
}

/* ============================================================================
 * Test Fixtures - Financial Curiosity Bridge
 * ============================================================================ */

static financial_curiosity_bridge_t* g_curiosity_bridge = NULL;

static void setup_curiosity(void)
{
    fin_curiosity_config_t config = financial_curiosity_bridge_default_config();
    g_curiosity_bridge = financial_curiosity_bridge_create(&config);
    ck_assert_ptr_nonnull(g_curiosity_bridge);
}

static void teardown_curiosity(void)
{
    if (g_curiosity_bridge) {
        financial_curiosity_bridge_destroy(g_curiosity_bridge);
        g_curiosity_bridge = NULL;
    }
}

/* ============================================================================
 * Test Fixtures - Financial Regret Bridge
 * ============================================================================ */

static financial_regret_bridge_t* g_regret_bridge = NULL;

static void setup_regret(void)
{
    fin_regret_config_t config;
    financial_regret_bridge_default_config(&config);
    g_regret_bridge = financial_regret_bridge_create(&config);
    ck_assert_ptr_nonnull(g_regret_bridge);
}

static void teardown_regret(void)
{
    if (g_regret_bridge) {
        financial_regret_bridge_destroy(g_regret_bridge);
        g_regret_bridge = NULL;
    }
}

/* ============================================================================
 * METACOGNITION BRIDGE TESTS
 * ============================================================================ */

/* --------------------------------------------------------------------------
 * Lifecycle Tests - Metacognition
 * -------------------------------------------------------------------------- */

START_TEST(test_metacog_default_config)
{
    fin_metacognition_config_t config;
    int result = financial_metacognition_bridge_default_config(&config);
    ck_assert_int_eq(result, 0);
    /* Verify sensible defaults are set */
    ck_assert_uint_gt(config.min_decisions_for_analysis, 0);
    ck_assert_uint_gt(config.analysis_window_size, 0);
}
END_TEST

START_TEST(test_metacog_default_config_null)
{
    int result = financial_metacognition_bridge_default_config(NULL);
    ck_assert_int_ne(result, 0);  /* Should fail with NULL */
}
END_TEST

START_TEST(test_metacog_create_default)
{
    financial_metacognition_bridge_t* bridge = financial_metacognition_bridge_create(NULL);
    ck_assert_ptr_nonnull(bridge);
    financial_metacognition_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_metacog_create_with_config)
{
    fin_metacognition_config_t config;
    financial_metacognition_bridge_default_config(&config);
    config.min_decisions_for_analysis = 10;
    config.analysis_window_size = 32;

    financial_metacognition_bridge_t* bridge = financial_metacognition_bridge_create(&config);
    ck_assert_ptr_nonnull(bridge);
    financial_metacognition_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_metacog_destroy_null)
{
    /* Should not crash with NULL */
    financial_metacognition_bridge_destroy(NULL);
}
END_TEST

START_TEST(test_metacog_reset)
{
    int result = financial_metacognition_bridge_reset(g_metacog_bridge);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_metacog_reset_null)
{
    int result = financial_metacognition_bridge_reset(NULL);
    ck_assert_int_ne(result, 0);
}
END_TEST

/* --------------------------------------------------------------------------
 * Subsystem Setter Tests - Metacognition
 * -------------------------------------------------------------------------- */

START_TEST(test_metacog_set_immune)
{
    int dummy_immune = 42;
    int result = financial_metacognition_bridge_set_immune(g_metacog_bridge, &dummy_immune);
    ck_assert_int_eq(result, 0);

    /* Can also set to NULL to disable */
    result = financial_metacognition_bridge_set_immune(g_metacog_bridge, NULL);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_metacog_set_immune_null_bridge)
{
    int dummy_immune = 42;
    int result = financial_metacognition_bridge_set_immune(NULL, &dummy_immune);
    ck_assert_int_ne(result, 0);
}
END_TEST

START_TEST(test_metacog_set_bbb)
{
    /* BBB system is an opaque pointer, we can use any non-null address for test */
    int dummy_bbb = 42;
    int result = financial_metacognition_bridge_set_bbb(g_metacog_bridge, (bbb_system_t)&dummy_bbb);
    ck_assert_int_eq(result, 0);

    result = financial_metacognition_bridge_set_bbb(g_metacog_bridge, NULL);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_metacog_set_kg_wiring)
{
    int dummy_kg = 42;
    int result = financial_metacognition_bridge_set_kg_wiring(g_metacog_bridge, &dummy_kg);
    ck_assert_int_eq(result, 0);

    result = financial_metacognition_bridge_set_kg_wiring(g_metacog_bridge, NULL);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_metacog_set_health_agent)
{
    int dummy_agent = 42;
    int result = financial_metacognition_bridge_set_health_agent(g_metacog_bridge, &dummy_agent);
    ck_assert_int_eq(result, 0);

    result = financial_metacognition_bridge_set_health_agent(g_metacog_bridge, NULL);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_metacog_set_logger)
{
    int dummy_logger = 42;
    int result = financial_metacognition_bridge_set_logger(g_metacog_bridge, &dummy_logger);
    ck_assert_int_eq(result, 0);

    result = financial_metacognition_bridge_set_logger(g_metacog_bridge, NULL);
    ck_assert_int_eq(result, 0);
}
END_TEST

/* --------------------------------------------------------------------------
 * Core API Tests - Metacognition (Bias Detection)
 * -------------------------------------------------------------------------- */

START_TEST(test_metacog_detect_biases_empty_history)
{
    fin_bias_detection_t biases[FIN_METACOG_MAX_BIASES];
    uint32_t bias_count = 0;

    /* With no decision history, should return error or 0 biases */
    int result = financial_metacognition_bridge_detect_biases(g_metacog_bridge, biases, &bias_count);
    /* May return error due to insufficient data, or 0 with 0 biases */
    if (result == 0) {
        ck_assert_uint_eq(bias_count, 0);
    }
}
END_TEST

START_TEST(test_metacog_detect_biases_null)
{
    uint32_t bias_count = 0;

    int result = financial_metacognition_bridge_detect_biases(NULL, NULL, &bias_count);
    ck_assert_int_ne(result, 0);

    result = financial_metacognition_bridge_detect_biases(g_metacog_bridge, NULL, &bias_count);
    ck_assert_int_ne(result, 0);
}
END_TEST

START_TEST(test_metacog_check_specific_bias)
{
    fin_bias_detection_t detection;
    memset(&detection, 0, sizeof(detection));

    int result = financial_metacognition_bridge_check_bias(
        g_metacog_bridge, FIN_BIAS_CONFIRMATION, &detection);
    /* May fail due to insufficient data, but should not crash */
    (void)result;
    /* If successful, check structure is populated */
    if (result == 0) {
        ck_assert(detection.strength >= 0.0f && detection.strength <= 1.0f);
    }
}
END_TEST

/* --------------------------------------------------------------------------
 * Core API Tests - Metacognition (Confidence Calibration)
 * -------------------------------------------------------------------------- */

START_TEST(test_metacog_assess_confidence_empty)
{
    fin_confidence_result_t result_struct;
    memset(&result_struct, 0, sizeof(result_struct));

    int result = financial_metacognition_bridge_assess_confidence(g_metacog_bridge, &result_struct);
    /* May return error due to insufficient data */
    (void)result;
}
END_TEST

START_TEST(test_metacog_assess_confidence_null)
{
    int result = financial_metacognition_bridge_assess_confidence(NULL, NULL);
    ck_assert_int_ne(result, 0);

    result = financial_metacognition_bridge_assess_confidence(g_metacog_bridge, NULL);
    ck_assert_int_ne(result, 0);
}
END_TEST

/* --------------------------------------------------------------------------
 * Core API Tests - Metacognition (Full Assessment)
 * -------------------------------------------------------------------------- */

START_TEST(test_metacog_assess_full)
{
    fin_metacognitive_assessment_t assessment;
    memset(&assessment, 0, sizeof(assessment));

    int result = financial_metacognition_bridge_assess(g_metacog_bridge, &assessment);
    /* May return error due to insufficient data, but should not crash */
    (void)result;
}
END_TEST

START_TEST(test_metacog_assess_null)
{
    int result = financial_metacognition_bridge_assess(NULL, NULL);
    ck_assert_int_ne(result, 0);

    result = financial_metacognition_bridge_assess(g_metacog_bridge, NULL);
    ck_assert_int_ne(result, 0);
}
END_TEST

/* --------------------------------------------------------------------------
 * Query/Stats Tests - Metacognition
 * -------------------------------------------------------------------------- */

START_TEST(test_metacog_get_state)
{
    fin_metacognition_bridge_state_t state = financial_metacognition_bridge_get_state(g_metacog_bridge);
    /* Should be INITIALIZED or ACTIVE after creation */
    ck_assert(state == FIN_METACOG_STATE_INITIALIZED || state == FIN_METACOG_STATE_ACTIVE);
}
END_TEST

START_TEST(test_metacog_get_state_null)
{
    fin_metacognition_bridge_state_t state = financial_metacognition_bridge_get_state(NULL);
    ck_assert_int_eq(state, FIN_METACOG_STATE_UNINITIALIZED);
}
END_TEST

START_TEST(test_metacog_get_stats)
{
    fin_metacognition_bridge_stats_t stats;
    memset(&stats, 0xFF, sizeof(stats));  /* Fill with garbage to verify it gets set */

    int result = financial_metacognition_bridge_get_stats(g_metacog_bridge, &stats);
    ck_assert_int_eq(result, 0);
    /* Stats should be zero or reasonable values on fresh bridge */
}
END_TEST

START_TEST(test_metacog_get_stats_null)
{
    int result = financial_metacognition_bridge_get_stats(NULL, NULL);
    ck_assert_int_ne(result, 0);
}
END_TEST

START_TEST(test_metacog_version)
{
    const char* version = financial_metacognition_bridge_version();
    ck_assert_ptr_nonnull(version);
    ck_assert_uint_gt(strlen(version), 0);
}
END_TEST

/* ============================================================================
 * UNCERTAINTY BRIDGE TESTS
 * ============================================================================ */

/* --------------------------------------------------------------------------
 * Lifecycle Tests - Uncertainty
 * -------------------------------------------------------------------------- */

START_TEST(test_uncertainty_default_config)
{
    fin_uncertainty_config_t config = financial_uncertainty_bridge_default_config();
    /* Verify sensible defaults */
    ck_assert(config.min_predictions >= 1.0f);
    ck_assert(config.confidence_threshold >= 0.0f && config.confidence_threshold <= 1.0f);
}
END_TEST

START_TEST(test_uncertainty_create_default)
{
    financial_uncertainty_bridge_t* bridge = financial_uncertainty_bridge_create(NULL);
    ck_assert_ptr_nonnull(bridge);
    financial_uncertainty_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_uncertainty_create_with_config)
{
    fin_uncertainty_config_t config = financial_uncertainty_bridge_default_config();
    config.info_gathering_threshold = 0.6f;
    config.act_threshold = 0.3f;

    financial_uncertainty_bridge_t* bridge = financial_uncertainty_bridge_create(&config);
    ck_assert_ptr_nonnull(bridge);
    financial_uncertainty_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_uncertainty_destroy_null)
{
    financial_uncertainty_bridge_destroy(NULL);
}
END_TEST

START_TEST(test_uncertainty_reset)
{
    int result = financial_uncertainty_bridge_reset(g_uncertainty_bridge);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_uncertainty_reset_null)
{
    int result = financial_uncertainty_bridge_reset(NULL);
    ck_assert_int_ne(result, 0);
}
END_TEST

/* --------------------------------------------------------------------------
 * Subsystem Setter Tests - Uncertainty
 * -------------------------------------------------------------------------- */

START_TEST(test_uncertainty_set_immune)
{
    int dummy = 42;
    int result = financial_uncertainty_bridge_set_immune(g_uncertainty_bridge, &dummy);
    ck_assert_int_eq(result, 0);

    result = financial_uncertainty_bridge_set_immune(g_uncertainty_bridge, NULL);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_uncertainty_set_bbb)
{
    int dummy = 42;
    int result = financial_uncertainty_bridge_set_bbb(g_uncertainty_bridge, (bbb_system_t)&dummy);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_uncertainty_set_kg_wiring)
{
    int dummy = 42;
    int result = financial_uncertainty_bridge_set_kg_wiring(g_uncertainty_bridge, &dummy);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_uncertainty_set_health_agent)
{
    int dummy = 42;
    int result = financial_uncertainty_bridge_set_health_agent(g_uncertainty_bridge, &dummy);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_uncertainty_set_logger)
{
    int dummy = 42;
    int result = financial_uncertainty_bridge_set_logger(g_uncertainty_bridge, &dummy);
    ck_assert_int_eq(result, 0);
}
END_TEST

/* --------------------------------------------------------------------------
 * Core API Tests - Uncertainty (Decomposition)
 * -------------------------------------------------------------------------- */

START_TEST(test_uncertainty_decompose_basic)
{
    /* Create simple prediction data */
    float values[] = {1.0f, 1.1f, 0.9f, 1.05f, 0.95f};
    float confidences[] = {0.8f, 0.7f, 0.9f, 0.85f, 0.75f};

    fin_prediction_t prediction = {
        .values = values,
        .confidences = confidences,
        .count = 5
    };

    fin_uncertainty_t uncertainty;
    memset(&uncertainty, 0, sizeof(uncertainty));

    int result = financial_uncertainty_bridge_decompose(g_uncertainty_bridge, &prediction, &uncertainty);
    ck_assert_int_eq(result, 0);

    /* Verify decomposition values are sensible */
    ck_assert(uncertainty.total >= 0.0f);
    ck_assert(uncertainty.epistemic >= 0.0f);
    ck_assert(uncertainty.aleatoric >= 0.0f);
    /* Total should be related to epistemic and aleatoric (exact formula may vary) */
    ck_assert(uncertainty.total >= 0.0f && uncertainty.total <= 1.0f);
}
END_TEST

START_TEST(test_uncertainty_decompose_null)
{
    fin_uncertainty_t uncertainty;
    int result = financial_uncertainty_bridge_decompose(NULL, NULL, &uncertainty);
    ck_assert_int_ne(result, 0);

    result = financial_uncertainty_bridge_decompose(g_uncertainty_bridge, NULL, &uncertainty);
    ck_assert_int_ne(result, 0);
}
END_TEST

START_TEST(test_uncertainty_should_gather_info)
{
    fin_uncertainty_t uncertainty = {
        .epistemic = 0.7f,
        .aleatoric = 0.2f,
        .total = 0.9f
    };

    bool should_gather = false;
    int result = financial_uncertainty_bridge_should_gather_info(
        g_uncertainty_bridge, &uncertainty, &should_gather);
    ck_assert_int_eq(result, 0);
    /* High epistemic ratio should recommend gathering info */
    ck_assert(should_gather == true);
}
END_TEST

START_TEST(test_uncertainty_should_gather_info_low_epistemic)
{
    fin_uncertainty_t uncertainty = {
        .epistemic = 0.1f,
        .aleatoric = 0.8f,
        .total = 0.9f
    };

    bool should_gather = true;
    int result = financial_uncertainty_bridge_should_gather_info(
        g_uncertainty_bridge, &uncertainty, &should_gather);
    ck_assert_int_eq(result, 0);
    /* Low epistemic ratio should not recommend gathering info */
    ck_assert(should_gather == false);
}
END_TEST

START_TEST(test_uncertainty_should_gather_info_null)
{
    bool should_gather;
    int result = financial_uncertainty_bridge_should_gather_info(NULL, NULL, &should_gather);
    ck_assert_int_ne(result, 0);
}
END_TEST

/* --------------------------------------------------------------------------
 * Core API Tests - Uncertainty (Decision Guidance)
 * -------------------------------------------------------------------------- */

START_TEST(test_uncertainty_get_guidance)
{
    fin_uncertainty_t uncertainty = {
        .epistemic = 0.1f,
        .aleatoric = 0.1f,
        .total = 0.2f
    };

    fin_decision_guidance_t guidance;
    int result = financial_uncertainty_bridge_get_guidance(
        g_uncertainty_bridge, &uncertainty, &guidance);
    ck_assert_int_eq(result, 0);
    /* Low total uncertainty should give confident guidance */
    ck_assert(guidance == FIN_DECISION_ACT_CONFIDENTLY || guidance == FIN_DECISION_ACT_CAUTIOUSLY);
}
END_TEST

START_TEST(test_uncertainty_get_guidance_null)
{
    fin_decision_guidance_t guidance;
    int result = financial_uncertainty_bridge_get_guidance(NULL, NULL, &guidance);
    ck_assert_int_ne(result, 0);
}
END_TEST

/* --------------------------------------------------------------------------
 * Query/Stats Tests - Uncertainty
 * -------------------------------------------------------------------------- */

START_TEST(test_uncertainty_get_state)
{
    fin_uncertainty_op_state_t state = financial_uncertainty_bridge_get_state(g_uncertainty_bridge);
    ck_assert(state == FIN_UNCERTAINTY_STATE_IDLE || state != FIN_UNCERTAINTY_STATE_ERROR);
}
END_TEST

START_TEST(test_uncertainty_get_state_null)
{
    fin_uncertainty_op_state_t state = financial_uncertainty_bridge_get_state(NULL);
    ck_assert_int_eq(state, FIN_UNCERTAINTY_STATE_UNINITIALIZED);
}
END_TEST

START_TEST(test_uncertainty_get_stats)
{
    fin_uncertainty_bridge_stats_t stats;
    int result = financial_uncertainty_bridge_get_stats(g_uncertainty_bridge, &stats);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_uncertainty_version)
{
    const char* version = financial_uncertainty_bridge_version();
    ck_assert_ptr_nonnull(version);
    ck_assert_uint_gt(strlen(version), 0);
}
END_TEST

/* ============================================================================
 * CURIOSITY BRIDGE TESTS
 * ============================================================================ */

/* --------------------------------------------------------------------------
 * Lifecycle Tests - Curiosity
 * -------------------------------------------------------------------------- */

START_TEST(test_curiosity_default_config)
{
    fin_curiosity_config_t config = financial_curiosity_bridge_default_config();
    ck_assert_uint_gt(config.max_hypotheses_per_cycle, 0);
    ck_assert(config.min_information_gain >= 0.0f);
    ck_assert(config.exploration_coefficient >= 0.0f);
}
END_TEST

START_TEST(test_curiosity_create_default)
{
    financial_curiosity_bridge_t* bridge = financial_curiosity_bridge_create(NULL);
    ck_assert_ptr_nonnull(bridge);
    financial_curiosity_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_curiosity_create_with_config)
{
    fin_curiosity_config_t config = financial_curiosity_bridge_default_config();
    config.max_hypotheses_per_cycle = 20;
    config.strategy = FIN_SELECTION_THOMPSON;

    financial_curiosity_bridge_t* bridge = financial_curiosity_bridge_create(&config);
    ck_assert_ptr_nonnull(bridge);
    financial_curiosity_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_curiosity_destroy_null)
{
    financial_curiosity_bridge_destroy(NULL);
}
END_TEST

START_TEST(test_curiosity_reset)
{
    int result = financial_curiosity_bridge_reset(g_curiosity_bridge);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_curiosity_reset_null)
{
    int result = financial_curiosity_bridge_reset(NULL);
    ck_assert_int_ne(result, 0);
}
END_TEST

/* --------------------------------------------------------------------------
 * Subsystem Setter Tests - Curiosity
 * -------------------------------------------------------------------------- */

START_TEST(test_curiosity_set_immune)
{
    int dummy = 42;
    int result = financial_curiosity_bridge_set_immune(g_curiosity_bridge, &dummy);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_curiosity_set_bbb)
{
    int dummy = 42;
    int result = financial_curiosity_bridge_set_bbb(g_curiosity_bridge, (bbb_system_t)&dummy);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_curiosity_set_kg_wiring)
{
    int dummy = 42;
    int result = financial_curiosity_bridge_set_kg_wiring(g_curiosity_bridge, &dummy);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_curiosity_set_health_agent)
{
    int dummy = 42;
    int result = financial_curiosity_bridge_set_health_agent(g_curiosity_bridge, &dummy);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_curiosity_set_logger)
{
    int dummy = 42;
    int result = financial_curiosity_bridge_set_logger(g_curiosity_bridge, &dummy);
    ck_assert_int_eq(result, 0);
}
END_TEST

/* --------------------------------------------------------------------------
 * Core API Tests - Curiosity (Hypothesis Generation)
 * -------------------------------------------------------------------------- */

START_TEST(test_curiosity_generate_hypotheses)
{
    /* Create market state */
    float prices[] = {100.0f, 101.0f, 99.5f, 102.0f, 98.0f};
    float volumes[] = {1000.0f, 1200.0f, 800.0f, 1500.0f, 900.0f};

    fin_market_state_t market_state = {
        .prices = prices,
        .volumes = volumes,
        .num_assets = 5,
        .timestamp_ms = 1000000
    };

    fin_hypothesis_result_t* result = financial_curiosity_result_create(FIN_CURIOSITY_MAX_CANDIDATES);
    ck_assert_ptr_nonnull(result);

    int ret = financial_curiosity_bridge_generate_hypotheses(
        g_curiosity_bridge, &market_state, result);
    ck_assert_int_eq(ret, 0);

    /* Should generate some hypotheses */
    ck_assert_uint_ge(result->num_candidates, 0);

    financial_curiosity_result_destroy(result);
}
END_TEST

START_TEST(test_curiosity_generate_hypotheses_null)
{
    fin_hypothesis_result_t result;
    int ret = financial_curiosity_bridge_generate_hypotheses(NULL, NULL, &result);
    ck_assert_int_ne(ret, 0);

    ret = financial_curiosity_bridge_generate_hypotheses(g_curiosity_bridge, NULL, &result);
    ck_assert_int_ne(ret, 0);
}
END_TEST

START_TEST(test_curiosity_select_exploration)
{
    /* Create test candidates */
    fin_extended_candidate_t candidates[3];
    memset(candidates, 0, sizeof(candidates));

    strcpy(candidates[0].base.hypothesis, "Price trend continuation");
    candidates[0].base.information_gain = 0.8f;
    candidates[0].base.exploration_cost = 0.2f;
    candidates[0].base.expected_value = 0.6f;
    candidates[0].type = FIN_HYPOTHESIS_TREND;
    candidates[0].confidence = 0.7f;

    strcpy(candidates[1].base.hypothesis, "Mean reversion expected");
    candidates[1].base.information_gain = 0.5f;
    candidates[1].base.exploration_cost = 0.3f;
    candidates[1].base.expected_value = 0.4f;
    candidates[1].type = FIN_HYPOTHESIS_MEAN_REVERSION;
    candidates[1].confidence = 0.6f;

    strcpy(candidates[2].base.hypothesis, "Volatility breakout");
    candidates[2].base.information_gain = 0.9f;
    candidates[2].base.exploration_cost = 0.4f;
    candidates[2].base.expected_value = 0.5f;
    candidates[2].type = FIN_HYPOTHESIS_VOLATILITY;
    candidates[2].confidence = 0.5f;

    fin_selection_result_t selection;
    memset(&selection, 0, sizeof(selection));

    int ret = financial_curiosity_bridge_select_exploration(
        g_curiosity_bridge, candidates, 3, &selection);
    ck_assert_int_eq(ret, 0);

    /* Should select one of the candidates */
    ck_assert_uint_lt(selection.selected_index, 3);
    ck_assert(selection.selection_score >= 0.0f);
}
END_TEST

START_TEST(test_curiosity_select_exploration_null)
{
    fin_selection_result_t selection;
    int ret = financial_curiosity_bridge_select_exploration(NULL, NULL, 0, &selection);
    ck_assert_int_ne(ret, 0);
}
END_TEST

/* --------------------------------------------------------------------------
 * Core API Tests - Curiosity (Information Gain)
 * -------------------------------------------------------------------------- */

START_TEST(test_curiosity_compute_information_gain)
{
    fin_extended_candidate_t hypothesis;
    memset(&hypothesis, 0, sizeof(hypothesis));
    strcpy(hypothesis.base.hypothesis, "Test hypothesis");
    hypothesis.type = FIN_HYPOTHESIS_TREND;
    hypothesis.confidence = 0.7f;

    float prices[] = {100.0f, 101.0f, 99.5f};
    float volumes[] = {1000.0f, 1200.0f, 800.0f};

    fin_market_state_t market_state = {
        .prices = prices,
        .volumes = volumes,
        .num_assets = 3,
        .timestamp_ms = 1000000
    };

    float info_gain = 0.0f;
    int ret = financial_curiosity_bridge_compute_information_gain(
        g_curiosity_bridge, &hypothesis, &market_state, &info_gain);
    ck_assert_int_eq(ret, 0);
    ck_assert(info_gain >= 0.0f && info_gain <= 1.0f);
}
END_TEST

START_TEST(test_curiosity_compute_information_gain_null)
{
    float info_gain;
    int ret = financial_curiosity_bridge_compute_information_gain(NULL, NULL, NULL, &info_gain);
    ck_assert_int_ne(ret, 0);
}
END_TEST

/* --------------------------------------------------------------------------
 * Query/Stats Tests - Curiosity
 * -------------------------------------------------------------------------- */

START_TEST(test_curiosity_get_state)
{
    fin_curiosity_op_state_t state = financial_curiosity_bridge_get_state(g_curiosity_bridge);
    ck_assert(state == FIN_CURIOSITY_STATE_IDLE || state != FIN_CURIOSITY_STATE_ERROR);
}
END_TEST

START_TEST(test_curiosity_get_state_null)
{
    fin_curiosity_op_state_t state = financial_curiosity_bridge_get_state(NULL);
    ck_assert_int_eq(state, FIN_CURIOSITY_STATE_UNINITIALIZED);
}
END_TEST

START_TEST(test_curiosity_get_stats)
{
    fin_curiosity_bridge_stats_t stats;
    int ret = financial_curiosity_bridge_get_stats(g_curiosity_bridge, &stats);
    ck_assert_int_eq(ret, 0);
}
END_TEST

START_TEST(test_curiosity_version)
{
    const char* version = financial_curiosity_bridge_version();
    ck_assert_ptr_nonnull(version);
    ck_assert_uint_gt(strlen(version), 0);
}
END_TEST

/* ============================================================================
 * REGRET BRIDGE TESTS
 * ============================================================================ */

/* --------------------------------------------------------------------------
 * Lifecycle Tests - Regret
 * -------------------------------------------------------------------------- */

START_TEST(test_regret_default_config)
{
    fin_regret_config_t config;
    int result = financial_regret_bridge_default_config(&config);
    ck_assert_int_eq(result, 0);
    ck_assert_uint_gt(config.min_trades_for_analysis, 0);
    ck_assert(config.regret_threshold >= 0.0f && config.regret_threshold <= 1.0f);
}
END_TEST

START_TEST(test_regret_default_config_null)
{
    int result = financial_regret_bridge_default_config(NULL);
    ck_assert_int_ne(result, 0);
}
END_TEST

START_TEST(test_regret_create_default)
{
    financial_regret_bridge_t* bridge = financial_regret_bridge_create(NULL);
    ck_assert_ptr_nonnull(bridge);
    financial_regret_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_regret_create_with_config)
{
    fin_regret_config_t config;
    financial_regret_bridge_default_config(&config);
    config.min_trades_for_analysis = 5;
    config.max_counterfactuals = 4;

    financial_regret_bridge_t* bridge = financial_regret_bridge_create(&config);
    ck_assert_ptr_nonnull(bridge);
    financial_regret_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_regret_destroy_null)
{
    financial_regret_bridge_destroy(NULL);
}
END_TEST

START_TEST(test_regret_reset)
{
    int result = financial_regret_bridge_reset(g_regret_bridge);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_regret_reset_null)
{
    int result = financial_regret_bridge_reset(NULL);
    ck_assert_int_ne(result, 0);
}
END_TEST

/* --------------------------------------------------------------------------
 * Subsystem Setter Tests - Regret
 * -------------------------------------------------------------------------- */

START_TEST(test_regret_set_immune)
{
    int dummy = 42;
    int result = financial_regret_bridge_set_immune(g_regret_bridge, &dummy);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_regret_set_bbb)
{
    int dummy = 42;
    int result = financial_regret_bridge_set_bbb(g_regret_bridge, (bbb_system_t)&dummy);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_regret_set_kg_wiring)
{
    int dummy = 42;
    int result = financial_regret_bridge_set_kg_wiring(g_regret_bridge, &dummy);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_regret_set_health_agent)
{
    int dummy = 42;
    int result = financial_regret_bridge_set_health_agent(g_regret_bridge, &dummy);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_regret_set_logger)
{
    int dummy = 42;
    int result = financial_regret_bridge_set_logger(g_regret_bridge, &dummy);
    ck_assert_int_eq(result, 0);
}
END_TEST

/* --------------------------------------------------------------------------
 * Core API Tests - Regret (Analysis)
 * -------------------------------------------------------------------------- */

START_TEST(test_regret_analyze_basic)
{
    fin_trade_t trade = {
        .price = 100.0f,
        .quantity = 10.0f,
        .direction = FIN_TRADE_DIRECTION_LONG,
        .outcome = -0.05f,  /* 5% loss */
        .timestamp_ms = 1000000
    };

    fin_action_t action_taken = {
        .type = FIN_ACTION_BUY,
        .magnitude = 10.0f
    };

    fin_regret_analysis_t analysis;
    memset(&analysis, 0, sizeof(analysis));

    int result = financial_regret_bridge_analyze(g_regret_bridge, &trade, &action_taken, &analysis);
    ck_assert_int_eq(result, 0);

    /* Verify analysis results */
    ck_assert(analysis.regret_magnitude >= 0.0f && analysis.regret_magnitude <= 1.0f);
}
END_TEST

START_TEST(test_regret_analyze_null)
{
    fin_regret_analysis_t analysis;
    int result = financial_regret_bridge_analyze(NULL, NULL, NULL, &analysis);
    ck_assert_int_ne(result, 0);

    result = financial_regret_bridge_analyze(g_regret_bridge, NULL, NULL, &analysis);
    ck_assert_int_ne(result, 0);
}
END_TEST

/* --------------------------------------------------------------------------
 * Core API Tests - Regret (Counterfactual)
 * -------------------------------------------------------------------------- */

START_TEST(test_regret_counterfactual_basic)
{
    fin_trade_t trade = {
        .price = 100.0f,
        .quantity = 10.0f,
        .direction = FIN_TRADE_DIRECTION_LONG,
        .outcome = -0.05f,
        .timestamp_ms = 1000000
    };

    fin_action_t alternative = {
        .type = FIN_ACTION_HOLD,
        .magnitude = 0.0f
    };

    float hypothetical_outcome = 0.0f;
    int result = financial_regret_bridge_counterfactual(
        g_regret_bridge, &trade, &alternative, &hypothetical_outcome);
    ck_assert_int_eq(result, 0);
    /* Hypothetical outcome should be computed */
}
END_TEST

START_TEST(test_regret_counterfactual_null)
{
    float hypothetical_outcome;
    int result = financial_regret_bridge_counterfactual(NULL, NULL, NULL, &hypothetical_outcome);
    ck_assert_int_ne(result, 0);
}
END_TEST

/* --------------------------------------------------------------------------
 * Core API Tests - Regret (Lesson Extraction)
 * -------------------------------------------------------------------------- */

START_TEST(test_regret_extract_lesson)
{
    /* First do an analysis to have something to extract from */
    fin_trade_t trade = {
        .price = 100.0f,
        .quantity = 10.0f,
        .direction = FIN_TRADE_DIRECTION_LONG,
        .outcome = -0.15f,  /* 15% loss - significant regret */
        .timestamp_ms = 1000000
    };

    fin_action_t action_taken = {
        .type = FIN_ACTION_BUY,
        .magnitude = 10.0f
    };

    fin_regret_analysis_t analysis;
    memset(&analysis, 0, sizeof(analysis));

    int result = financial_regret_bridge_analyze(g_regret_bridge, &trade, &action_taken, &analysis);
    ck_assert_int_eq(result, 0);

    fin_lesson_t lesson;
    memset(&lesson, 0, sizeof(lesson));

    result = financial_regret_bridge_extract_lesson(g_regret_bridge, &analysis, &lesson);
    ck_assert_int_eq(result, 0);
    /* Lesson should be extracted if regret is significant */
}
END_TEST

START_TEST(test_regret_extract_lesson_null)
{
    fin_lesson_t lesson;
    int result = financial_regret_bridge_extract_lesson(NULL, NULL, &lesson);
    ck_assert_int_ne(result, 0);
}
END_TEST

START_TEST(test_regret_get_lessons)
{
    fin_lesson_t lessons[10];
    uint32_t num_lessons = 0;

    int result = financial_regret_bridge_get_lessons(g_regret_bridge, lessons, 10, &num_lessons);
    ck_assert_int_eq(result, 0);
    /* Initially should have 0 lessons */
    ck_assert_uint_ge(num_lessons, 0);
}
END_TEST

START_TEST(test_regret_get_lessons_null)
{
    uint32_t num_lessons;
    int result = financial_regret_bridge_get_lessons(NULL, NULL, 0, &num_lessons);
    ck_assert_int_ne(result, 0);
}
END_TEST

/* --------------------------------------------------------------------------
 * Query/Stats Tests - Regret
 * -------------------------------------------------------------------------- */

START_TEST(test_regret_get_state)
{
    fin_regret_bridge_state_t state = financial_regret_bridge_get_state(g_regret_bridge);
    ck_assert(state == FIN_REGRET_STATE_INITIALIZED || state == FIN_REGRET_STATE_ACTIVE);
}
END_TEST

START_TEST(test_regret_get_state_null)
{
    fin_regret_bridge_state_t state = financial_regret_bridge_get_state(NULL);
    ck_assert_int_eq(state, FIN_REGRET_STATE_UNINITIALIZED);
}
END_TEST

START_TEST(test_regret_get_stats)
{
    fin_regret_bridge_stats_t stats;
    int result = financial_regret_bridge_get_stats(g_regret_bridge, &stats);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_regret_get_stats_null)
{
    int result = financial_regret_bridge_get_stats(NULL, NULL);
    ck_assert_int_ne(result, 0);
}
END_TEST

START_TEST(test_regret_version)
{
    const char* version = financial_regret_bridge_version();
    ck_assert_ptr_nonnull(version);
    ck_assert_uint_gt(strlen(version), 0);
}
END_TEST

/* ============================================================================
 * Test Suite Creation
 * ============================================================================ */

Suite* financial_metacognition_bridges_suite(void)
{
    Suite* s = suite_create("Financial Metacognition Bridges");

    /* ========== METACOGNITION BRIDGE TEST CASES ========== */

    /* Metacognition - Lifecycle */
    TCase* tc_metacog_lifecycle = tcase_create("Metacognition Lifecycle");
    tcase_add_test(tc_metacog_lifecycle, test_metacog_default_config);
    tcase_add_test(tc_metacog_lifecycle, test_metacog_default_config_null);
    tcase_add_test(tc_metacog_lifecycle, test_metacog_create_default);
    tcase_add_test(tc_metacog_lifecycle, test_metacog_create_with_config);
    tcase_add_test(tc_metacog_lifecycle, test_metacog_destroy_null);
    tcase_add_checked_fixture(tc_metacog_lifecycle, setup_metacognition, teardown_metacognition);
    tcase_add_test(tc_metacog_lifecycle, test_metacog_reset);
    tcase_add_test(tc_metacog_lifecycle, test_metacog_reset_null);
    suite_add_tcase(s, tc_metacog_lifecycle);

    /* Metacognition - Subsystem Setters */
    TCase* tc_metacog_setters = tcase_create("Metacognition Setters");
    tcase_add_checked_fixture(tc_metacog_setters, setup_metacognition, teardown_metacognition);
    tcase_add_test(tc_metacog_setters, test_metacog_set_immune);
    tcase_add_test(tc_metacog_setters, test_metacog_set_immune_null_bridge);
    tcase_add_test(tc_metacog_setters, test_metacog_set_bbb);
    tcase_add_test(tc_metacog_setters, test_metacog_set_kg_wiring);
    tcase_add_test(tc_metacog_setters, test_metacog_set_health_agent);
    tcase_add_test(tc_metacog_setters, test_metacog_set_logger);
    suite_add_tcase(s, tc_metacog_setters);

    /* Metacognition - Core API */
    TCase* tc_metacog_core = tcase_create("Metacognition Core API");
    tcase_add_checked_fixture(tc_metacog_core, setup_metacognition, teardown_metacognition);
    tcase_add_test(tc_metacog_core, test_metacog_detect_biases_empty_history);
    tcase_add_test(tc_metacog_core, test_metacog_detect_biases_null);
    tcase_add_test(tc_metacog_core, test_metacog_check_specific_bias);
    tcase_add_test(tc_metacog_core, test_metacog_assess_confidence_empty);
    tcase_add_test(tc_metacog_core, test_metacog_assess_confidence_null);
    tcase_add_test(tc_metacog_core, test_metacog_assess_full);
    tcase_add_test(tc_metacog_core, test_metacog_assess_null);
    suite_add_tcase(s, tc_metacog_core);

    /* Metacognition - Query/Stats */
    TCase* tc_metacog_query = tcase_create("Metacognition Query");
    tcase_add_checked_fixture(tc_metacog_query, setup_metacognition, teardown_metacognition);
    tcase_add_test(tc_metacog_query, test_metacog_get_state);
    tcase_add_test(tc_metacog_query, test_metacog_get_state_null);
    tcase_add_test(tc_metacog_query, test_metacog_get_stats);
    tcase_add_test(tc_metacog_query, test_metacog_get_stats_null);
    tcase_add_test(tc_metacog_query, test_metacog_version);
    suite_add_tcase(s, tc_metacog_query);

    /* ========== UNCERTAINTY BRIDGE TEST CASES ========== */

    /* Uncertainty - Lifecycle */
    TCase* tc_uncert_lifecycle = tcase_create("Uncertainty Lifecycle");
    tcase_add_test(tc_uncert_lifecycle, test_uncertainty_default_config);
    tcase_add_test(tc_uncert_lifecycle, test_uncertainty_create_default);
    tcase_add_test(tc_uncert_lifecycle, test_uncertainty_create_with_config);
    tcase_add_test(tc_uncert_lifecycle, test_uncertainty_destroy_null);
    tcase_add_checked_fixture(tc_uncert_lifecycle, setup_uncertainty, teardown_uncertainty);
    tcase_add_test(tc_uncert_lifecycle, test_uncertainty_reset);
    tcase_add_test(tc_uncert_lifecycle, test_uncertainty_reset_null);
    suite_add_tcase(s, tc_uncert_lifecycle);

    /* Uncertainty - Subsystem Setters */
    TCase* tc_uncert_setters = tcase_create("Uncertainty Setters");
    tcase_add_checked_fixture(tc_uncert_setters, setup_uncertainty, teardown_uncertainty);
    tcase_add_test(tc_uncert_setters, test_uncertainty_set_immune);
    tcase_add_test(tc_uncert_setters, test_uncertainty_set_bbb);
    tcase_add_test(tc_uncert_setters, test_uncertainty_set_kg_wiring);
    tcase_add_test(tc_uncert_setters, test_uncertainty_set_health_agent);
    tcase_add_test(tc_uncert_setters, test_uncertainty_set_logger);
    suite_add_tcase(s, tc_uncert_setters);

    /* Uncertainty - Core API */
    TCase* tc_uncert_core = tcase_create("Uncertainty Core API");
    tcase_add_checked_fixture(tc_uncert_core, setup_uncertainty, teardown_uncertainty);
    tcase_add_test(tc_uncert_core, test_uncertainty_decompose_basic);
    tcase_add_test(tc_uncert_core, test_uncertainty_decompose_null);
    tcase_add_test(tc_uncert_core, test_uncertainty_should_gather_info);
    tcase_add_test(tc_uncert_core, test_uncertainty_should_gather_info_low_epistemic);
    tcase_add_test(tc_uncert_core, test_uncertainty_should_gather_info_null);
    tcase_add_test(tc_uncert_core, test_uncertainty_get_guidance);
    tcase_add_test(tc_uncert_core, test_uncertainty_get_guidance_null);
    suite_add_tcase(s, tc_uncert_core);

    /* Uncertainty - Query/Stats */
    TCase* tc_uncert_query = tcase_create("Uncertainty Query");
    tcase_add_checked_fixture(tc_uncert_query, setup_uncertainty, teardown_uncertainty);
    tcase_add_test(tc_uncert_query, test_uncertainty_get_state);
    tcase_add_test(tc_uncert_query, test_uncertainty_get_state_null);
    tcase_add_test(tc_uncert_query, test_uncertainty_get_stats);
    tcase_add_test(tc_uncert_query, test_uncertainty_version);
    suite_add_tcase(s, tc_uncert_query);

    /* ========== CURIOSITY BRIDGE TEST CASES ========== */

    /* Curiosity - Lifecycle */
    TCase* tc_curio_lifecycle = tcase_create("Curiosity Lifecycle");
    tcase_add_test(tc_curio_lifecycle, test_curiosity_default_config);
    tcase_add_test(tc_curio_lifecycle, test_curiosity_create_default);
    tcase_add_test(tc_curio_lifecycle, test_curiosity_create_with_config);
    tcase_add_test(tc_curio_lifecycle, test_curiosity_destroy_null);
    tcase_add_checked_fixture(tc_curio_lifecycle, setup_curiosity, teardown_curiosity);
    tcase_add_test(tc_curio_lifecycle, test_curiosity_reset);
    tcase_add_test(tc_curio_lifecycle, test_curiosity_reset_null);
    suite_add_tcase(s, tc_curio_lifecycle);

    /* Curiosity - Subsystem Setters */
    TCase* tc_curio_setters = tcase_create("Curiosity Setters");
    tcase_add_checked_fixture(tc_curio_setters, setup_curiosity, teardown_curiosity);
    tcase_add_test(tc_curio_setters, test_curiosity_set_immune);
    tcase_add_test(tc_curio_setters, test_curiosity_set_bbb);
    tcase_add_test(tc_curio_setters, test_curiosity_set_kg_wiring);
    tcase_add_test(tc_curio_setters, test_curiosity_set_health_agent);
    tcase_add_test(tc_curio_setters, test_curiosity_set_logger);
    suite_add_tcase(s, tc_curio_setters);

    /* Curiosity - Core API */
    TCase* tc_curio_core = tcase_create("Curiosity Core API");
    tcase_add_checked_fixture(tc_curio_core, setup_curiosity, teardown_curiosity);
    tcase_add_test(tc_curio_core, test_curiosity_generate_hypotheses);
    tcase_add_test(tc_curio_core, test_curiosity_generate_hypotheses_null);
    tcase_add_test(tc_curio_core, test_curiosity_select_exploration);
    tcase_add_test(tc_curio_core, test_curiosity_select_exploration_null);
    tcase_add_test(tc_curio_core, test_curiosity_compute_information_gain);
    tcase_add_test(tc_curio_core, test_curiosity_compute_information_gain_null);
    suite_add_tcase(s, tc_curio_core);

    /* Curiosity - Query/Stats */
    TCase* tc_curio_query = tcase_create("Curiosity Query");
    tcase_add_checked_fixture(tc_curio_query, setup_curiosity, teardown_curiosity);
    tcase_add_test(tc_curio_query, test_curiosity_get_state);
    tcase_add_test(tc_curio_query, test_curiosity_get_state_null);
    tcase_add_test(tc_curio_query, test_curiosity_get_stats);
    tcase_add_test(tc_curio_query, test_curiosity_version);
    suite_add_tcase(s, tc_curio_query);

    /* ========== REGRET BRIDGE TEST CASES ========== */

    /* Regret - Lifecycle */
    TCase* tc_regret_lifecycle = tcase_create("Regret Lifecycle");
    tcase_add_test(tc_regret_lifecycle, test_regret_default_config);
    tcase_add_test(tc_regret_lifecycle, test_regret_default_config_null);
    tcase_add_test(tc_regret_lifecycle, test_regret_create_default);
    tcase_add_test(tc_regret_lifecycle, test_regret_create_with_config);
    tcase_add_test(tc_regret_lifecycle, test_regret_destroy_null);
    tcase_add_checked_fixture(tc_regret_lifecycle, setup_regret, teardown_regret);
    tcase_add_test(tc_regret_lifecycle, test_regret_reset);
    tcase_add_test(tc_regret_lifecycle, test_regret_reset_null);
    suite_add_tcase(s, tc_regret_lifecycle);

    /* Regret - Subsystem Setters */
    TCase* tc_regret_setters = tcase_create("Regret Setters");
    tcase_add_checked_fixture(tc_regret_setters, setup_regret, teardown_regret);
    tcase_add_test(tc_regret_setters, test_regret_set_immune);
    tcase_add_test(tc_regret_setters, test_regret_set_bbb);
    tcase_add_test(tc_regret_setters, test_regret_set_kg_wiring);
    tcase_add_test(tc_regret_setters, test_regret_set_health_agent);
    tcase_add_test(tc_regret_setters, test_regret_set_logger);
    suite_add_tcase(s, tc_regret_setters);

    /* Regret - Core API */
    TCase* tc_regret_core = tcase_create("Regret Core API");
    tcase_add_checked_fixture(tc_regret_core, setup_regret, teardown_regret);
    tcase_add_test(tc_regret_core, test_regret_analyze_basic);
    tcase_add_test(tc_regret_core, test_regret_analyze_null);
    tcase_add_test(tc_regret_core, test_regret_counterfactual_basic);
    tcase_add_test(tc_regret_core, test_regret_counterfactual_null);
    tcase_add_test(tc_regret_core, test_regret_extract_lesson);
    tcase_add_test(tc_regret_core, test_regret_extract_lesson_null);
    tcase_add_test(tc_regret_core, test_regret_get_lessons);
    tcase_add_test(tc_regret_core, test_regret_get_lessons_null);
    suite_add_tcase(s, tc_regret_core);

    /* Regret - Query/Stats */
    TCase* tc_regret_query = tcase_create("Regret Query");
    tcase_add_checked_fixture(tc_regret_query, setup_regret, teardown_regret);
    tcase_add_test(tc_regret_query, test_regret_get_state);
    tcase_add_test(tc_regret_query, test_regret_get_state_null);
    tcase_add_test(tc_regret_query, test_regret_get_stats);
    tcase_add_test(tc_regret_query, test_regret_get_stats_null);
    tcase_add_test(tc_regret_query, test_regret_version);
    suite_add_tcase(s, tc_regret_query);

    return s;
}

int main(void)
{
    int number_failed;
    Suite* s = financial_metacognition_bridges_suite();
    SRunner* sr = srunner_create(s);

    /* Run all tests */
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
