/**
 * @file test_brain_immune_tick_regression.c
 * @brief Regression tests for brain immune tick orchestrator
 *
 * WHAT: Regression tests ensuring existing behavior is not broken
 * WHY:  Verify brain_immune_update, exception immune presentation, and
 *       health agent message queue behavior remain unchanged
 * HOW:  Tests verifying known good behavior is preserved
 *
 * @author NIMCP Development Team
 * @date 2025-01-25
 */

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "cognitive/immune/nimcp_brain_immune_tick.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/fault_tolerance/nimcp_health_agent.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

static brain_immune_system_t* g_immune = NULL;

static void setup(void)
{
    nimcp_exception_system_init();

    brain_immune_config_t config;
    brain_immune_default_config(&config);
    g_immune = brain_immune_create(&config);
    ck_assert_ptr_nonnull(g_immune);
}

static void setup_with_tick(void)
{
    setup();
    int result = brain_immune_tick_init(g_immune, NULL);
    ck_assert_int_eq(result, 0);
}

static void teardown(void)
{
    if (g_immune) {
        brain_immune_tick_shutdown(g_immune);
        brain_immune_destroy(g_immune);
        g_immune = NULL;
    }
    nimcp_exception_system_shutdown();
}

/* ============================================================================
 * Regression Tests: brain_immune_update Still Works
 * ============================================================================ */

START_TEST(test_brain_immune_update_still_works)
{
    /* brain_immune_update should still work even with tick orchestrator */
    brain_immune_metrics_t metrics_before;
    brain_immune_get_metrics(g_immune, &metrics_before);

    /* Call update directly */
    int result = brain_immune_update(g_immune, 100);
    ck_assert_int_eq(result, 0);

    /* Verify update executed */
    brain_immune_metrics_t metrics_after;
    brain_immune_get_metrics(g_immune, &metrics_after);
    /* Update should not throw errors */
}
END_TEST

START_TEST(test_brain_immune_update_without_tick_init)
{
    /* brain_immune_update should work even without tick_init */
    brain_immune_config_t config;
    brain_immune_default_config(&config);
    brain_immune_system_t* immune = brain_immune_create(&config);
    ck_assert_ptr_nonnull(immune);

    /* Call update without tick init */
    int result = brain_immune_update(immune, 100);
    ck_assert_int_eq(result, 0);

    brain_immune_destroy(immune);
}
END_TEST

START_TEST(test_brain_immune_update_after_tick)
{
    /* Running update after tick should not cause issues */
    brain_immune_tick(g_immune, 50);
    int result = brain_immune_update(g_immune, 50);
    ck_assert_int_eq(result, 0);
}
END_TEST

/* ============================================================================
 * Regression Tests: Exception Immune Presentation Unchanged
 * ============================================================================ */

START_TEST(test_exception_present_to_immune_unchanged)
{
    /* Connect immune system */
    nimcp_exception_immune_init(g_immune);

    /* Create and present exception directly (old API) */
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_NULL_POINTER,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        "Regression test exception"
    );
    ck_assert_ptr_nonnull(ex);

    nimcp_immune_response_t response;
    int result = nimcp_exception_present_to_immune(ex, &response);
    ck_assert_int_eq(result, 0);
    ck_assert(ex->presented_to_immune);
    ck_assert_uint_ne(ex->antigen_id, 0);

    /* Verify immune system received it */
    brain_immune_metrics_t metrics;
    brain_immune_get_metrics(g_immune, &metrics);
    ck_assert_uint_ge(metrics.total_antigens_presented, 1);

    nimcp_exception_unref(ex);
    nimcp_exception_immune_shutdown();
}
END_TEST

START_TEST(test_exception_async_queue_unchanged)
{
    /* Connect immune system */
    nimcp_exception_immune_init(g_immune);

    /* Queue exception asynchronously (old API) */
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_TIMEOUT,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__,
        __LINE__,
        __func__,
        "Async exception"
    );
    ck_assert_ptr_nonnull(ex);

    int result = nimcp_exception_present_async(ex);
    ck_assert_int_eq(result, 0);

    /* Process pending (old API) */
    size_t processed = nimcp_exception_immune_process_pending(10);
    ck_assert_uint_ge(processed, 1);

    nimcp_exception_unref(ex);
    nimcp_exception_immune_shutdown();
}
END_TEST

START_TEST(test_throw_to_immune_macro_unchanged)
{
    /* Connect immune system */
    nimcp_exception_immune_init(g_immune);

    /* Use NIMCP_THROW_TO_IMMUNE macro */
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Macro test");

    /* Process via old API */
    size_t processed = nimcp_exception_immune_process_pending(10);
    ck_assert_uint_ge(processed, 1);

    nimcp_exception_immune_shutdown();
}
END_TEST

/* ============================================================================
 * Regression Tests: Health Agent Message Queue Unchanged
 * ============================================================================ */

START_TEST(test_health_agent_message_queue_unchanged)
{
    /* Create agent */
    health_agent_config_t config;
    nimcp_health_agent_default_config(&config);
    nimcp_health_agent_t* agent = nimcp_health_agent_create(&config);
    ck_assert_ptr_nonnull(agent);

    /* Queue messages (old API) */
    for (int i = 0; i < 5; i++) {
        health_agent_message_t msg = nimcp_health_agent_create_message(
            HEALTH_MSG_ANOMALY_DETECTED,
            HEALTH_SEVERITY_INFO,
            HEALTH_SOURCE_MEMORY,
            "Queue test %d", i
        );
        int result = nimcp_health_agent_report_anomaly(agent, &msg);
        ck_assert_int_eq(result, 0);
    }

    /* Verify queue depth */
    uint32_t pending = nimcp_health_agent_pending_messages(agent);
    ck_assert_uint_eq(pending, 5);

    /* New dequeue API should also work */
    uint32_t depth = nimcp_health_agent_get_queue_depth(agent);
    ck_assert_uint_eq(depth, 5);

    /* Dequeue one */
    health_agent_message_t dequeued;
    bool has_msg = nimcp_health_agent_dequeue_message(agent, &dequeued);
    ck_assert(has_msg);
    ck_assert_int_eq(dequeued.type, HEALTH_MSG_ANOMALY_DETECTED);

    /* Verify one less */
    depth = nimcp_health_agent_get_queue_depth(agent);
    ck_assert_uint_eq(depth, 4);

    nimcp_health_agent_destroy(agent);
}
END_TEST

START_TEST(test_health_agent_stats_unchanged)
{
    /* Create agent */
    health_agent_config_t config;
    nimcp_health_agent_default_config(&config);
    nimcp_health_agent_t* agent = nimcp_health_agent_create(&config);
    ck_assert_ptr_nonnull(agent);

    /* Report anomalies */
    for (int i = 0; i < 3; i++) {
        health_agent_message_t msg = nimcp_health_agent_create_message(
            HEALTH_MSG_ANOMALY_DETECTED,
            HEALTH_SEVERITY_WARNING,
            HEALTH_SOURCE_THREADING,
            "Stats test %d", i
        );
        nimcp_health_agent_report_anomaly(agent, &msg);
    }

    /* Check stats (old API) */
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    ck_assert_uint_eq(stats.anomalies_detected, 3);
    ck_assert_uint_eq(stats.messages_sent, 3);

    nimcp_health_agent_destroy(agent);
}
END_TEST

/* ============================================================================
 * Regression Tests: Antigen Presentation Unchanged
 * ============================================================================ */

START_TEST(test_antigen_presentation_unchanged)
{
    /* Present antigen directly (old API) */
    uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE] = {0};
    strcpy((char*)epitope, "test_epitope_pattern");

    uint32_t antigen_id = 0;
    int result = brain_immune_present_antigen(
        g_immune,
        ANTIGEN_SOURCE_ANOMALY,
        epitope,
        strlen((char*)epitope),
        5,  /* severity */
        0,  /* source_node */
        &antigen_id
    );
    ck_assert_int_eq(result, 0);
    ck_assert_uint_ne(antigen_id, 0);

    /* Verify metrics */
    brain_immune_metrics_t metrics;
    brain_immune_get_metrics(g_immune, &metrics);
    ck_assert_uint_ge(metrics.total_antigens_presented, 1);
}
END_TEST

START_TEST(test_cytokine_release_unchanged)
{
    /* Release cytokine directly (old API) */
    uint32_t cytokine_id = 0;
    int result = brain_immune_release_cytokine(
        g_immune,
        BRAIN_CYTOKINE_IL6,
        0,     /* source_cell */
        0.5f,  /* concentration */
        0,     /* target_region */
        &cytokine_id
    );
    ck_assert_int_eq(result, 0);

    /* Verify metrics */
    brain_immune_metrics_t metrics;
    brain_immune_get_metrics(g_immune, &metrics);
    ck_assert_uint_ge(metrics.cytokines_released, 1);
}
END_TEST

START_TEST(test_inflammation_unchanged)
{
    /* Present antigen first */
    uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE] = "test";
    uint32_t antigen_id = 0;
    brain_immune_present_antigen(g_immune, ANTIGEN_SOURCE_BFT, epitope, 4, 8, 0, &antigen_id);

    /* Initiate inflammation (old API) */
    uint32_t site_id = 0;
    int result = brain_immune_initiate_inflammation(g_immune, 0, antigen_id, &site_id);
    ck_assert_int_eq(result, 0);
    ck_assert_uint_ne(site_id, 0);

    /* Verify metrics */
    brain_immune_metrics_t metrics;
    brain_immune_get_metrics(g_immune, &metrics);
    ck_assert_uint_ge(metrics.inflammation_sites, 1);
}
END_TEST

/* ============================================================================
 * Regression Tests: B Cell / T Cell Activation Unchanged
 * ============================================================================ */

START_TEST(test_b_cell_activation_unchanged)
{
    /* Present antigen to trigger B cell activation */
    uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE] = "b_cell_test";
    uint32_t antigen_id = 0;
    brain_immune_present_antigen(g_immune, ANTIGEN_SOURCE_ANOMALY, epitope, 11, 6, 0, &antigen_id);

    /* Run update to process */
    brain_immune_update(g_immune, 100);

    /* Activate B cell directly (old API) */
    uint32_t b_cell_id = 0;
    int result = brain_immune_activate_b_cell(g_immune, antigen_id, &b_cell_id);
    ck_assert_int_eq(result, 0);

    /* Verify metrics */
    brain_immune_metrics_t metrics;
    brain_immune_get_metrics(g_immune, &metrics);
    ck_assert_uint_ge(metrics.b_cells_activated, 1);
}
END_TEST

START_TEST(test_t_cell_activation_unchanged)
{
    /* Present antigen to trigger T cell activation */
    uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE] = "t_cell_test";
    uint32_t antigen_id = 0;
    brain_immune_present_antigen(g_immune, ANTIGEN_SOURCE_BBB, epitope, 11, 7, 0, &antigen_id);

    /* Run update to process */
    brain_immune_update(g_immune, 100);

    /* Activate T cell directly (old API) */
    uint32_t t_cell_id = 0;
    int result = brain_immune_activate_t_cell(g_immune, antigen_id, T_CELL_HELPER, &t_cell_id);
    ck_assert_int_eq(result, 0);

    /* Verify metrics */
    brain_immune_metrics_t metrics;
    brain_immune_get_metrics(g_immune, &metrics);
    ck_assert_uint_ge(metrics.t_cells_activated, 1);
}
END_TEST

/* ============================================================================
 * Regression Tests: No Double Processing
 * ============================================================================ */

START_TEST(test_exception_not_double_processed)
{
    /* Connect immune system */
    nimcp_exception_immune_init(g_immune);
    brain_immune_tick_init(g_immune, NULL);

    /* Create exception */
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_INVALID_PARAM,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__,
        __LINE__,
        __func__,
        "Double process test"
    );

    /* Present directly */
    nimcp_exception_present_to_immune(ex, NULL);
    ck_assert(ex->presented_to_immune);

    brain_immune_metrics_t metrics_after_first;
    brain_immune_get_metrics(g_immune, &metrics_after_first);

    /* Try to present again */
    nimcp_exception_present_to_immune(ex, NULL);

    brain_immune_metrics_t metrics_after_second;
    brain_immune_get_metrics(g_immune, &metrics_after_second);

    /* Should not have double-counted */
    ck_assert_uint_eq(metrics_after_first.total_antigens_presented,
                      metrics_after_second.total_antigens_presented);

    nimcp_exception_unref(ex);
    brain_immune_tick_shutdown(g_immune);
    nimcp_exception_immune_shutdown();
}
END_TEST

/* ============================================================================
 * Test Suite Creation
 * ============================================================================ */

Suite* brain_immune_tick_regression_suite(void)
{
    Suite* s = suite_create("Brain Immune Tick Regression");

    /* brain_immune_update regression */
    TCase* tc_update = tcase_create("brain_immune_update");
    tcase_add_checked_fixture(tc_update, setup_with_tick, teardown);
    tcase_add_test(tc_update, test_brain_immune_update_still_works);
    tcase_add_test(tc_update, test_brain_immune_update_without_tick_init);
    tcase_add_test(tc_update, test_brain_immune_update_after_tick);
    suite_add_tcase(s, tc_update);

    /* Exception immune regression */
    TCase* tc_exception = tcase_create("Exception Immune");
    tcase_add_checked_fixture(tc_exception, setup_with_tick, teardown);
    tcase_add_test(tc_exception, test_exception_present_to_immune_unchanged);
    tcase_add_test(tc_exception, test_exception_async_queue_unchanged);
    tcase_add_test(tc_exception, test_throw_to_immune_macro_unchanged);
    suite_add_tcase(s, tc_exception);

    /* Health agent regression */
    TCase* tc_health = tcase_create("Health Agent");
    tcase_add_checked_fixture(tc_health, setup, teardown);
    tcase_add_test(tc_health, test_health_agent_message_queue_unchanged);
    tcase_add_test(tc_health, test_health_agent_stats_unchanged);
    suite_add_tcase(s, tc_health);

    /* Antigen/Cytokine/Inflammation regression */
    TCase* tc_immune_api = tcase_create("Immune API");
    tcase_add_checked_fixture(tc_immune_api, setup_with_tick, teardown);
    tcase_add_test(tc_immune_api, test_antigen_presentation_unchanged);
    tcase_add_test(tc_immune_api, test_cytokine_release_unchanged);
    tcase_add_test(tc_immune_api, test_inflammation_unchanged);
    suite_add_tcase(s, tc_immune_api);

    /* Cell activation regression */
    TCase* tc_cells = tcase_create("Cell Activation");
    tcase_add_checked_fixture(tc_cells, setup_with_tick, teardown);
    tcase_add_test(tc_cells, test_b_cell_activation_unchanged);
    tcase_add_test(tc_cells, test_t_cell_activation_unchanged);
    suite_add_tcase(s, tc_cells);

    /* Double processing prevention */
    TCase* tc_double = tcase_create("Double Processing");
    tcase_add_checked_fixture(tc_double, setup, teardown);
    tcase_add_test(tc_double, test_exception_not_double_processed);
    suite_add_tcase(s, tc_double);

    return s;
}

int main(void)
{
    int number_failed;
    Suite* s = brain_immune_tick_regression_suite();
    SRunner* sr = srunner_create(s);

    /* Run all tests */
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
