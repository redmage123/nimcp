/**
 * @file test_financial_orchestrator_fuzzy.c
 * @brief Unit tests for Financial Cognitive Orchestrator and Fuzzy Bridge
 *
 * WHAT: Test suite for the financial cognitive orchestrator (8-layer pipeline)
 *       and fuzzy bridge KG wiring / enhanced setters
 * WHY:  Verify correct behavior of orchestrated financial processing and
 *       fuzzy logic bridge subsystem integration
 * HOW:  Unit tests using Check framework covering lifecycle, pipeline stages,
 *       subsystem setters, stats, health monitoring, and fuzzy bridge operations
 *
 * @author NIMCP Development Team
 * @date 2026-01-29
 */

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "cognitive/parietal/nimcp_financial_cognitive_orchestrator.h"
#include "utils/fuzzy/nimcp_fuzzy_bridge.h"
#include "utils/fuzzy/nimcp_fuzzy_types.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

static financial_cognitive_orchestrator_handle_t* g_orch = NULL;
static fuzzy_bridge_t* g_fuzzy = NULL;

static void setup_orchestrator(void)
{
    fin_orchestrator_config_t config;
    financial_cognitive_orchestrator_default_config(&config);
    config.enable_immune_integration = false;  /* Disable for unit tests */
    config.enable_bbb_validation = false;
    config.enable_health_monitoring = false;
    g_orch = financial_cognitive_orchestrator_create(&config);
    ck_assert_ptr_nonnull(g_orch);
}

static void setup_fuzzy(void)
{
    fuzzy_bridge_config_t config = fuzzy_bridge_default_config();
    config.enable_immune_integration = false;
    config.enable_bbb_validation = false;
    g_fuzzy = fuzzy_bridge_create(&config);
    ck_assert_ptr_nonnull(g_fuzzy);
}

static void setup_both(void)
{
    setup_orchestrator();
    setup_fuzzy();
}

static void teardown_orchestrator(void)
{
    if (g_orch) {
        financial_cognitive_orchestrator_destroy(g_orch);
        g_orch = NULL;
    }
}

static void teardown_fuzzy(void)
{
    if (g_fuzzy) {
        fuzzy_bridge_destroy(g_fuzzy);
        g_fuzzy = NULL;
    }
}

static void teardown_both(void)
{
    teardown_orchestrator();
    teardown_fuzzy();
}

/* ============================================================================
 * Orchestrator Lifecycle Tests
 * ============================================================================ */

START_TEST(test_orchestrator_default_config)
{
    fin_orchestrator_config_t config;
    int result = financial_cognitive_orchestrator_default_config(&config);
    ck_assert_int_eq(result, 0);

    /* Verify defaults */
    ck_assert(config.enable_working_memory);
    ck_assert(config.enable_emotion_processing);
    ck_assert(config.enable_attention_filtering);
    ck_assert(config.enable_ethics_validation);
    ck_assert(config.enable_metacognition);
    ck_assert(config.enable_learning);
    ck_assert_float_gt(config.min_confidence_threshold, 0.0f);
    ck_assert_float_le(config.min_confidence_threshold, 1.0f);
}
END_TEST

START_TEST(test_orchestrator_default_config_null)
{
    int result = financial_cognitive_orchestrator_default_config(NULL);
    ck_assert_int_eq(result, FIN_ORCH_ERR_NULL);
}
END_TEST

START_TEST(test_orchestrator_create_destroy)
{
    fin_orchestrator_config_t config;
    financial_cognitive_orchestrator_default_config(&config);
    financial_cognitive_orchestrator_handle_t* orch =
        financial_cognitive_orchestrator_create(&config);
    ck_assert_ptr_nonnull(orch);

    fin_orchestrator_state_t state = financial_cognitive_orchestrator_get_state(orch);
    ck_assert_int_eq(state, FIN_ORCH_STATE_INITIALIZED);

    financial_cognitive_orchestrator_destroy(orch);
}
END_TEST

START_TEST(test_orchestrator_create_null_config)
{
    /* Should use defaults when config is NULL */
    financial_cognitive_orchestrator_handle_t* orch =
        financial_cognitive_orchestrator_create(NULL);
    ck_assert_ptr_nonnull(orch);

    financial_cognitive_orchestrator_destroy(orch);
}
END_TEST

START_TEST(test_orchestrator_destroy_null)
{
    /* Should not crash */
    financial_cognitive_orchestrator_destroy(NULL);
}
END_TEST

START_TEST(test_orchestrator_reset)
{
    int result = financial_cognitive_orchestrator_reset(g_orch);
    ck_assert_int_eq(result, 0);

    fin_orchestrator_state_t state = financial_cognitive_orchestrator_get_state(g_orch);
    ck_assert_int_eq(state, FIN_ORCH_STATE_INITIALIZED);
}
END_TEST

START_TEST(test_orchestrator_reset_null)
{
    int result = financial_cognitive_orchestrator_reset(NULL);
    ck_assert_int_eq(result, FIN_ORCH_ERR_NULL);
}
END_TEST

/* ============================================================================
 * Orchestrator Module Registration Tests
 * ============================================================================ */

START_TEST(test_orchestrator_get_modules)
{
    financial_cognitive_orchestrator_t* modules =
        financial_cognitive_orchestrator_get_modules(g_orch);
    ck_assert_ptr_nonnull(modules);
}
END_TEST

START_TEST(test_orchestrator_get_modules_null)
{
    financial_cognitive_orchestrator_t* modules =
        financial_cognitive_orchestrator_get_modules(NULL);
    ck_assert_ptr_null(modules);
}
END_TEST

START_TEST(test_orchestrator_validate_modules_empty)
{
    /* Fresh orchestrator has no modules registered - validation should fail */
    int result = financial_cognitive_orchestrator_validate_modules(g_orch);
    ck_assert_int_ne(result, 0);  /* Should return error for missing modules */
}
END_TEST

/* ============================================================================
 * Orchestrator Subsystem Setter Tests
 * ============================================================================ */

START_TEST(test_orchestrator_set_immune)
{
    int dummy_immune = 42;
    int result = financial_cognitive_orchestrator_set_immune(g_orch, &dummy_immune);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_orchestrator_set_immune_null_orch)
{
    int dummy_immune = 42;
    int result = financial_cognitive_orchestrator_set_immune(NULL, &dummy_immune);
    ck_assert_int_eq(result, FIN_ORCH_ERR_NULL);
}
END_TEST

START_TEST(test_orchestrator_set_bbb)
{
    int dummy_bbb = 42;
    int result = financial_cognitive_orchestrator_set_bbb(g_orch, (bbb_system_t)&dummy_bbb);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_orchestrator_set_health_agent)
{
    int dummy_agent = 42;
    int result = financial_cognitive_orchestrator_set_health_agent(g_orch, &dummy_agent);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_orchestrator_set_kg_wiring)
{
    int dummy_kg = 42;
    int result = financial_cognitive_orchestrator_set_kg_wiring(g_orch, &dummy_kg);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_orchestrator_set_logger)
{
    int dummy_logger = 42;
    int result = financial_cognitive_orchestrator_set_logger(g_orch, &dummy_logger);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_orchestrator_set_security)
{
    int dummy_security = 42;
    int result = financial_cognitive_orchestrator_set_security(g_orch, &dummy_security);
    ck_assert_int_eq(result, 0);
}
END_TEST

/* ============================================================================
 * Orchestrator Statistics Tests
 * ============================================================================ */

START_TEST(test_orchestrator_get_stats)
{
    fin_orchestrator_stats_t stats;
    int result = financial_cognitive_orchestrator_get_stats(g_orch, &stats);
    ck_assert_int_eq(result, 0);

    /* Fresh orchestrator should have zero stats */
    ck_assert_uint_eq(stats.market_data_processed, 0);
    ck_assert_uint_eq(stats.decisions_made, 0);
    ck_assert_uint_eq(stats.learning_cycles, 0);
}
END_TEST

START_TEST(test_orchestrator_get_stats_null)
{
    fin_orchestrator_stats_t stats;
    int result = financial_cognitive_orchestrator_get_stats(NULL, &stats);
    ck_assert_int_eq(result, FIN_ORCH_ERR_NULL);

    result = financial_cognitive_orchestrator_get_stats(g_orch, NULL);
    ck_assert_int_eq(result, FIN_ORCH_ERR_NULL);
}
END_TEST

START_TEST(test_orchestrator_reset_stats)
{
    fin_orchestrator_stats_t stats;

    /* Reset stats */
    financial_cognitive_orchestrator_reset_stats(g_orch);

    financial_cognitive_orchestrator_get_stats(g_orch, &stats);
    ck_assert_uint_eq(stats.market_data_processed, 0);
    ck_assert_uint_eq(stats.decisions_made, 0);
}
END_TEST

START_TEST(test_orchestrator_reset_stats_null)
{
    /* Should not crash */
    financial_cognitive_orchestrator_reset_stats(NULL);
}
END_TEST

/* ============================================================================
 * Orchestrator Utility Tests
 * ============================================================================ */

START_TEST(test_orchestrator_state_name)
{
    const char* name = fin_orchestrator_state_name(FIN_ORCH_STATE_INITIALIZED);
    ck_assert_ptr_nonnull(name);
    ck_assert_str_ne(name, "");

    name = fin_orchestrator_state_name(FIN_ORCH_STATE_PROCESSING);
    ck_assert_ptr_nonnull(name);

    name = fin_orchestrator_state_name(FIN_ORCH_STATE_ERROR);
    ck_assert_ptr_nonnull(name);
}
END_TEST

START_TEST(test_orchestrator_decision_name)
{
    const char* name = fin_orchestrator_decision_name(FIN_DECISION_BUY);
    ck_assert_ptr_nonnull(name);
    ck_assert_str_ne(name, "");

    name = fin_orchestrator_decision_name(FIN_DECISION_SELL);
    ck_assert_ptr_nonnull(name);

    name = fin_orchestrator_decision_name(FIN_DECISION_HOLD);
    ck_assert_ptr_nonnull(name);
}
END_TEST

START_TEST(test_orchestrator_stage_name)
{
    const char* name = fin_orchestrator_stage_name(FIN_PIPELINE_PERCEPTION);
    ck_assert_ptr_nonnull(name);
    ck_assert_str_ne(name, "");

    name = fin_orchestrator_stage_name(FIN_PIPELINE_DECISION);
    ck_assert_ptr_nonnull(name);

    name = fin_orchestrator_stage_name(FIN_PIPELINE_LEARNING);
    ck_assert_ptr_nonnull(name);
}
END_TEST

START_TEST(test_orchestrator_version)
{
    const char* version = financial_cognitive_orchestrator_version();
    ck_assert_ptr_nonnull(version);
    ck_assert_str_ne(version, "");
}
END_TEST

START_TEST(test_orchestrator_get_last_error)
{
    const char* err = financial_cognitive_orchestrator_get_last_error();
    ck_assert_ptr_nonnull(err);  /* May be empty string, but not NULL */
}
END_TEST

/* ============================================================================
 * Fuzzy Bridge Lifecycle Tests
 * ============================================================================ */

START_TEST(test_fuzzy_bridge_default_config)
{
    fuzzy_bridge_config_t config = fuzzy_bridge_default_config();

    ck_assert(config.enable_snn_integration);
    ck_assert(config.enable_stdp_integration);
    ck_assert(config.enable_plasticity_integration);
    ck_assert(config.enable_lnn_integration);
    ck_assert(config.enable_training_integration);
    ck_assert_float_eq(config.spike_rate_min, 0.0f);
    ck_assert_float_gt(config.spike_rate_max, 0.0f);
}
END_TEST

START_TEST(test_fuzzy_bridge_create_destroy)
{
    fuzzy_bridge_config_t config = fuzzy_bridge_default_config();
    fuzzy_bridge_t* bridge = fuzzy_bridge_create(&config);
    ck_assert_ptr_nonnull(bridge);

    fuzzy_bridge_state_t state = fuzzy_bridge_get_state(bridge);
    ck_assert_int_eq(state, FUZZY_BRIDGE_STATE_ACTIVE);

    fuzzy_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_fuzzy_bridge_create_null_config)
{
    /* Should use defaults when config is NULL */
    fuzzy_bridge_t* bridge = fuzzy_bridge_create(NULL);
    ck_assert_ptr_nonnull(bridge);

    fuzzy_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_fuzzy_bridge_destroy_null)
{
    /* Should not crash */
    fuzzy_bridge_destroy(NULL);
}
END_TEST

START_TEST(test_fuzzy_bridge_get_state_null)
{
    fuzzy_bridge_state_t state = fuzzy_bridge_get_state(NULL);
    ck_assert_int_eq(state, FUZZY_BRIDGE_STATE_ERROR);
}
END_TEST

/* ============================================================================
 * Fuzzy Bridge Subsystem Setter Tests
 * ============================================================================ */

START_TEST(test_fuzzy_bridge_set_immune)
{
    int dummy = 42;
    int result = fuzzy_bridge_set_immune(g_fuzzy, &dummy);
    ck_assert_int_eq(result, FUZZY_BRIDGE_ERR_OK);
}
END_TEST

START_TEST(test_fuzzy_bridge_set_immune_null)
{
    int dummy = 42;
    int result = fuzzy_bridge_set_immune(NULL, &dummy);
    ck_assert_int_eq(result, FUZZY_BRIDGE_ERR_NULL);
}
END_TEST

START_TEST(test_fuzzy_bridge_set_bbb)
{
    int dummy = 42;
    int result = fuzzy_bridge_set_bbb(g_fuzzy, &dummy);
    ck_assert_int_eq(result, FUZZY_BRIDGE_ERR_OK);
}
END_TEST

START_TEST(test_fuzzy_bridge_set_health_agent)
{
    int dummy = 42;
    int result = fuzzy_bridge_set_health_agent(g_fuzzy, &dummy);
    ck_assert_int_eq(result, FUZZY_BRIDGE_ERR_OK);
}
END_TEST

START_TEST(test_fuzzy_bridge_set_kg_wiring)
{
    int dummy = 42;
    int result = fuzzy_bridge_set_kg_wiring(g_fuzzy, (kg_wiring_t*)&dummy);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_fuzzy_bridge_set_kg_wiring_null)
{
    int result = fuzzy_bridge_set_kg_wiring(NULL, NULL);
    ck_assert_int_eq(result, -1);
}
END_TEST

START_TEST(test_fuzzy_bridge_set_logger)
{
    int dummy = 42;
    int result = fuzzy_bridge_set_logger(g_fuzzy, &dummy);
    ck_assert_int_eq(result, FUZZY_BRIDGE_ERR_OK);
}
END_TEST

START_TEST(test_fuzzy_bridge_set_security)
{
    int dummy = 42;
    int result = fuzzy_bridge_set_security(g_fuzzy, &dummy);
    ck_assert_int_eq(result, FUZZY_BRIDGE_ERR_OK);
}
END_TEST

START_TEST(test_fuzzy_bridge_set_snn)
{
    int dummy = 42;
    int result = fuzzy_bridge_set_snn(g_fuzzy, &dummy);
    ck_assert_int_eq(result, FUZZY_BRIDGE_ERR_OK);
}
END_TEST

START_TEST(test_fuzzy_bridge_set_stdp)
{
    int dummy = 42;
    int result = fuzzy_bridge_set_stdp(g_fuzzy, &dummy);
    ck_assert_int_eq(result, FUZZY_BRIDGE_ERR_OK);
}
END_TEST

START_TEST(test_fuzzy_bridge_set_lgss)
{
    int dummy = 42;
    int result = fuzzy_bridge_set_lgss(g_fuzzy, &dummy);
    ck_assert_int_eq(result, FUZZY_BRIDGE_ERR_OK);
}
END_TEST

START_TEST(test_fuzzy_bridge_set_lgss_null)
{
    int dummy = 42;
    int result = fuzzy_bridge_set_lgss(NULL, &dummy);
    ck_assert_int_eq(result, FUZZY_BRIDGE_ERR_NULL);
}
END_TEST

/* ============================================================================
 * Fuzzy Bridge KG Wiring Factory Test
 * ============================================================================ */

START_TEST(test_fuzzy_bridge_create_kg_wiring)
{
    /* Factory function for KG wiring - currently returns placeholder */
    kg_module_wiring_t* wiring = fuzzy_bridge_create_kg_wiring();
    /* May be NULL in placeholder implementation - just verify no crash */
    (void)wiring;
}
END_TEST

/* ============================================================================
 * Fuzzy Bridge Statistics Tests
 * ============================================================================ */

START_TEST(test_fuzzy_bridge_get_stats)
{
    fuzzy_bridge_stats_t stats;
    int result = fuzzy_bridge_get_stats(g_fuzzy, &stats);
    ck_assert_int_eq(result, 0);

    /* Fresh bridge should have zero stats */
    ck_assert_uint_eq(stats.spike_conversions, 0);
    ck_assert_uint_eq(stats.stdp_modulations, 0);
    ck_assert_uint_eq(stats.lnn_classifications, 0);
}
END_TEST

START_TEST(test_fuzzy_bridge_get_stats_null)
{
    fuzzy_bridge_stats_t stats;
    int result = fuzzy_bridge_get_stats(NULL, &stats);
    ck_assert_int_ne(result, 0);

    result = fuzzy_bridge_get_stats(g_fuzzy, NULL);
    ck_assert_int_ne(result, 0);
}
END_TEST

START_TEST(test_fuzzy_bridge_reset_stats)
{
    fuzzy_bridge_stats_t stats;

    /* Reset stats */
    fuzzy_bridge_reset_stats(g_fuzzy);

    fuzzy_bridge_get_stats(g_fuzzy, &stats);
    ck_assert_uint_eq(stats.spike_conversions, 0);
    ck_assert_uint_eq(stats.kg_messages_sent, 0);
}
END_TEST

START_TEST(test_fuzzy_bridge_reset_stats_null)
{
    /* Should not crash */
    fuzzy_bridge_reset_stats(NULL);
}
END_TEST

/* ============================================================================
 * Fuzzy Bridge Health and Modulation Tests
 * ============================================================================ */

START_TEST(test_fuzzy_bridge_set_inflammation)
{
    int result = fuzzy_bridge_set_inflammation(g_fuzzy, 0.5f);
    ck_assert_int_eq(result, 0);

    /* Test boundary values */
    result = fuzzy_bridge_set_inflammation(g_fuzzy, 0.0f);
    ck_assert_int_eq(result, 0);

    result = fuzzy_bridge_set_inflammation(g_fuzzy, 1.0f);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_fuzzy_bridge_set_fatigue)
{
    int result = fuzzy_bridge_set_fatigue(g_fuzzy, 0.5f);
    ck_assert_int_eq(result, 0);

    /* Test boundary values */
    result = fuzzy_bridge_set_fatigue(g_fuzzy, 0.0f);
    ck_assert_int_eq(result, 0);

    result = fuzzy_bridge_set_fatigue(g_fuzzy, 1.0f);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_fuzzy_bridge_heartbeat)
{
    int result = fuzzy_bridge_heartbeat(g_fuzzy, "test_operation", 0.5f);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_fuzzy_bridge_check_health)
{
    int result = fuzzy_bridge_check_health(g_fuzzy);
    ck_assert_int_eq(result, 0);  /* Active bridge should be healthy */
}
END_TEST

START_TEST(test_fuzzy_bridge_check_health_null)
{
    int result = fuzzy_bridge_check_health(NULL);
    ck_assert_int_ne(result, 0);  /* NULL bridge is not healthy */
}
END_TEST

START_TEST(test_fuzzy_bridge_get_last_error)
{
    const char* err = fuzzy_bridge_get_last_error();
    ck_assert_ptr_nonnull(err);  /* May be empty, but not NULL */
}
END_TEST

/* ============================================================================
 * Integration: Orchestrator with Fuzzy Bridge
 * ============================================================================ */

START_TEST(test_orchestrator_fuzzy_integration)
{
    /* Get modules and set fuzzy bridge */
    financial_cognitive_orchestrator_t* modules =
        financial_cognitive_orchestrator_get_modules(g_orch);
    ck_assert_ptr_nonnull(modules);

    modules->fuzzy = g_fuzzy;

    /* Verify fuzzy bridge is set */
    ck_assert_ptr_eq(modules->fuzzy, g_fuzzy);
}
END_TEST

/* ============================================================================
 * Test Suite Creation
 * ============================================================================ */

Suite* financial_orchestrator_fuzzy_suite(void)
{
    Suite* s = suite_create("Financial Orchestrator and Fuzzy Bridge");

    /* Orchestrator Lifecycle tests */
    TCase* tc_orch_lifecycle = tcase_create("Orchestrator Lifecycle");
    tcase_add_test(tc_orch_lifecycle, test_orchestrator_default_config);
    tcase_add_test(tc_orch_lifecycle, test_orchestrator_default_config_null);
    tcase_add_test(tc_orch_lifecycle, test_orchestrator_create_destroy);
    tcase_add_test(tc_orch_lifecycle, test_orchestrator_create_null_config);
    tcase_add_test(tc_orch_lifecycle, test_orchestrator_destroy_null);
    tcase_add_checked_fixture(tc_orch_lifecycle, setup_orchestrator, teardown_orchestrator);
    tcase_add_test(tc_orch_lifecycle, test_orchestrator_reset);
    tcase_add_test(tc_orch_lifecycle, test_orchestrator_reset_null);
    suite_add_tcase(s, tc_orch_lifecycle);

    /* Orchestrator Module Registration tests */
    TCase* tc_orch_modules = tcase_create("Orchestrator Modules");
    tcase_add_checked_fixture(tc_orch_modules, setup_orchestrator, teardown_orchestrator);
    tcase_add_test(tc_orch_modules, test_orchestrator_get_modules);
    tcase_add_test(tc_orch_modules, test_orchestrator_get_modules_null);
    tcase_add_test(tc_orch_modules, test_orchestrator_validate_modules_empty);
    suite_add_tcase(s, tc_orch_modules);

    /* Orchestrator Subsystem Setter tests */
    TCase* tc_orch_setters = tcase_create("Orchestrator Setters");
    tcase_add_checked_fixture(tc_orch_setters, setup_orchestrator, teardown_orchestrator);
    tcase_add_test(tc_orch_setters, test_orchestrator_set_immune);
    tcase_add_test(tc_orch_setters, test_orchestrator_set_immune_null_orch);
    tcase_add_test(tc_orch_setters, test_orchestrator_set_bbb);
    tcase_add_test(tc_orch_setters, test_orchestrator_set_health_agent);
    tcase_add_test(tc_orch_setters, test_orchestrator_set_kg_wiring);
    tcase_add_test(tc_orch_setters, test_orchestrator_set_logger);
    tcase_add_test(tc_orch_setters, test_orchestrator_set_security);
    suite_add_tcase(s, tc_orch_setters);

    /* Orchestrator Statistics tests */
    TCase* tc_orch_stats = tcase_create("Orchestrator Statistics");
    tcase_add_checked_fixture(tc_orch_stats, setup_orchestrator, teardown_orchestrator);
    tcase_add_test(tc_orch_stats, test_orchestrator_get_stats);
    tcase_add_test(tc_orch_stats, test_orchestrator_get_stats_null);
    tcase_add_test(tc_orch_stats, test_orchestrator_reset_stats);
    tcase_add_test(tc_orch_stats, test_orchestrator_reset_stats_null);
    suite_add_tcase(s, tc_orch_stats);

    /* Orchestrator Utility tests */
    TCase* tc_orch_util = tcase_create("Orchestrator Utilities");
    tcase_add_test(tc_orch_util, test_orchestrator_state_name);
    tcase_add_test(tc_orch_util, test_orchestrator_decision_name);
    tcase_add_test(tc_orch_util, test_orchestrator_stage_name);
    tcase_add_test(tc_orch_util, test_orchestrator_version);
    tcase_add_test(tc_orch_util, test_orchestrator_get_last_error);
    suite_add_tcase(s, tc_orch_util);

    /* Fuzzy Bridge Lifecycle tests */
    TCase* tc_fuzzy_lifecycle = tcase_create("Fuzzy Bridge Lifecycle");
    tcase_add_test(tc_fuzzy_lifecycle, test_fuzzy_bridge_default_config);
    tcase_add_test(tc_fuzzy_lifecycle, test_fuzzy_bridge_create_destroy);
    tcase_add_test(tc_fuzzy_lifecycle, test_fuzzy_bridge_create_null_config);
    tcase_add_test(tc_fuzzy_lifecycle, test_fuzzy_bridge_destroy_null);
    tcase_add_test(tc_fuzzy_lifecycle, test_fuzzy_bridge_get_state_null);
    suite_add_tcase(s, tc_fuzzy_lifecycle);

    /* Fuzzy Bridge Subsystem Setter tests */
    TCase* tc_fuzzy_setters = tcase_create("Fuzzy Bridge Setters");
    tcase_add_checked_fixture(tc_fuzzy_setters, setup_fuzzy, teardown_fuzzy);
    tcase_add_test(tc_fuzzy_setters, test_fuzzy_bridge_set_immune);
    tcase_add_test(tc_fuzzy_setters, test_fuzzy_bridge_set_immune_null);
    tcase_add_test(tc_fuzzy_setters, test_fuzzy_bridge_set_bbb);
    tcase_add_test(tc_fuzzy_setters, test_fuzzy_bridge_set_health_agent);
    tcase_add_test(tc_fuzzy_setters, test_fuzzy_bridge_set_kg_wiring);
    tcase_add_test(tc_fuzzy_setters, test_fuzzy_bridge_set_kg_wiring_null);
    tcase_add_test(tc_fuzzy_setters, test_fuzzy_bridge_set_logger);
    tcase_add_test(tc_fuzzy_setters, test_fuzzy_bridge_set_security);
    tcase_add_test(tc_fuzzy_setters, test_fuzzy_bridge_set_snn);
    tcase_add_test(tc_fuzzy_setters, test_fuzzy_bridge_set_stdp);
    tcase_add_test(tc_fuzzy_setters, test_fuzzy_bridge_set_lgss);
    tcase_add_test(tc_fuzzy_setters, test_fuzzy_bridge_set_lgss_null);
    suite_add_tcase(s, tc_fuzzy_setters);

    /* Fuzzy Bridge KG Wiring tests */
    TCase* tc_fuzzy_kg = tcase_create("Fuzzy Bridge KG Wiring");
    tcase_add_test(tc_fuzzy_kg, test_fuzzy_bridge_create_kg_wiring);
    suite_add_tcase(s, tc_fuzzy_kg);

    /* Fuzzy Bridge Statistics tests */
    TCase* tc_fuzzy_stats = tcase_create("Fuzzy Bridge Statistics");
    tcase_add_checked_fixture(tc_fuzzy_stats, setup_fuzzy, teardown_fuzzy);
    tcase_add_test(tc_fuzzy_stats, test_fuzzy_bridge_get_stats);
    tcase_add_test(tc_fuzzy_stats, test_fuzzy_bridge_get_stats_null);
    tcase_add_test(tc_fuzzy_stats, test_fuzzy_bridge_reset_stats);
    tcase_add_test(tc_fuzzy_stats, test_fuzzy_bridge_reset_stats_null);
    suite_add_tcase(s, tc_fuzzy_stats);

    /* Fuzzy Bridge Health and Modulation tests */
    TCase* tc_fuzzy_health = tcase_create("Fuzzy Bridge Health");
    tcase_add_checked_fixture(tc_fuzzy_health, setup_fuzzy, teardown_fuzzy);
    tcase_add_test(tc_fuzzy_health, test_fuzzy_bridge_set_inflammation);
    tcase_add_test(tc_fuzzy_health, test_fuzzy_bridge_set_fatigue);
    tcase_add_test(tc_fuzzy_health, test_fuzzy_bridge_heartbeat);
    tcase_add_test(tc_fuzzy_health, test_fuzzy_bridge_check_health);
    tcase_add_test(tc_fuzzy_health, test_fuzzy_bridge_check_health_null);
    tcase_add_test(tc_fuzzy_health, test_fuzzy_bridge_get_last_error);
    suite_add_tcase(s, tc_fuzzy_health);

    /* Integration tests */
    TCase* tc_integration = tcase_create("Integration");
    tcase_add_checked_fixture(tc_integration, setup_both, teardown_both);
    tcase_add_test(tc_integration, test_orchestrator_fuzzy_integration);
    suite_add_tcase(s, tc_integration);

    return s;
}

int main(void)
{
    int number_failed;
    Suite* s = financial_orchestrator_fuzzy_suite();
    SRunner* sr = srunner_create(s);

    /* Run all tests */
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
