/**
 * @file test_brain_immune_integration.c
 * @brief Unit tests for brain immune integration helper
 * @version 1.0.0
 * @date 2025-01-25
 *
 * Tests the nimcp_immune_integration_* helper functions that provide
 * a simple way to set up the full exception-immune-recovery pipeline.
 */

#include <check.h>
#include <stdlib.h>
#include <string.h>

#include "cognitive/immune/nimcp_brain_immune_integration.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/immune/nimcp_brain_immune_tick.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_immune.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

static nimcp_immune_integration_t* g_integration = NULL;

static void setup(void) {
    g_integration = NULL;
}

static void teardown(void) {
    if (g_integration) {
        nimcp_immune_integration_destroy(g_integration);
        g_integration = NULL;
    }
}

/* ============================================================================
 * Default Config Tests
 * ============================================================================ */

START_TEST(test_default_config) {
    nimcp_immune_integration_config_t config;
    nimcp_immune_integration_default_config(&config);

    /* Verify defaults */
    ck_assert(config.install_default_handlers == true);
    ck_assert(config.install_recovery_callbacks == true);
    ck_assert(config.create_immune_system == true);
    ck_assert(config.external_immune == NULL);
    ck_assert(config.init_tick_orchestrator == true);
    ck_assert(config.max_exceptions_per_tick == 10);
    ck_assert(config.max_health_msgs_per_tick == 20);
    ck_assert(config.auto_start == true);
    ck_assert(config.enable_logging == true);
}
END_TEST

START_TEST(test_default_config_null_safe) {
    /* Should not crash on NULL */
    nimcp_immune_integration_default_config(NULL);
}
END_TEST

/* ============================================================================
 * Create/Destroy Tests
 * ============================================================================ */

START_TEST(test_create_with_defaults) {
    /* Create with NULL config (uses defaults) */
    g_integration = nimcp_immune_integration_create(NULL);
    ck_assert_ptr_nonnull(g_integration);

    /* Should be running by default */
    ck_assert(nimcp_immune_integration_is_running(g_integration) == true);

    /* Should have an immune system */
    brain_immune_system_t* immune = nimcp_immune_integration_get_immune_system(g_integration);
    ck_assert_ptr_nonnull(immune);
}
END_TEST

START_TEST(test_create_with_custom_config) {
    nimcp_immune_integration_config_t config;
    nimcp_immune_integration_default_config(&config);

    config.auto_start = false;  /* Don't auto-start */
    config.enable_logging = false;
    config.max_exceptions_per_tick = 5;

    g_integration = nimcp_immune_integration_create(&config);
    ck_assert_ptr_nonnull(g_integration);

    /* Should NOT be running since auto_start is false */
    ck_assert(nimcp_immune_integration_is_running(g_integration) == false);
}
END_TEST

START_TEST(test_create_no_immune_system_fails) {
    nimcp_immune_integration_config_t config;
    nimcp_immune_integration_default_config(&config);

    config.create_immune_system = false;  /* Don't create */
    config.external_immune = NULL;        /* And no external */

    /* Should fail since no immune system provided */
    g_integration = nimcp_immune_integration_create(&config);
    ck_assert_ptr_null(g_integration);
}
END_TEST

START_TEST(test_destroy_null_safe) {
    /* Should not crash on NULL */
    nimcp_immune_integration_destroy(NULL);
}
END_TEST

/* ============================================================================
 * Start/Stop Tests
 * ============================================================================ */

START_TEST(test_start_stop) {
    nimcp_immune_integration_config_t config;
    nimcp_immune_integration_default_config(&config);
    config.auto_start = false;

    g_integration = nimcp_immune_integration_create(&config);
    ck_assert_ptr_nonnull(g_integration);
    ck_assert(nimcp_immune_integration_is_running(g_integration) == false);

    /* Start */
    int result = nimcp_immune_integration_start(g_integration);
    ck_assert_int_eq(result, 0);
    ck_assert(nimcp_immune_integration_is_running(g_integration) == true);

    /* Stop */
    result = nimcp_immune_integration_stop(g_integration);
    ck_assert_int_eq(result, 0);
    ck_assert(nimcp_immune_integration_is_running(g_integration) == false);
}
END_TEST

START_TEST(test_start_null_safe) {
    ck_assert_int_eq(nimcp_immune_integration_start(NULL), -1);
}
END_TEST

START_TEST(test_stop_null_safe) {
    ck_assert_int_eq(nimcp_immune_integration_stop(NULL), -1);
}
END_TEST

/* ============================================================================
 * Tick Tests
 * ============================================================================ */

START_TEST(test_tick) {
    g_integration = nimcp_immune_integration_create(NULL);
    ck_assert_ptr_nonnull(g_integration);

    /* Execute some ticks */
    for (int i = 0; i < 10; i++) {
        int result = nimcp_immune_integration_tick(g_integration, 50);
        ck_assert_int_eq(result, 0);
    }

    /* Check stats */
    nimcp_immune_integration_stats_t stats;
    int result = nimcp_immune_integration_get_stats(g_integration, &stats);
    ck_assert_int_eq(result, 0);
    ck_assert(stats.ticks_executed >= 10);
    /* uptime_ms can be 0 if test runs very fast, just verify it's non-negative */
    ck_assert(stats.uptime_ms >= 0);
}
END_TEST

START_TEST(test_tick_not_running) {
    nimcp_immune_integration_config_t config;
    nimcp_immune_integration_default_config(&config);
    config.auto_start = false;

    g_integration = nimcp_immune_integration_create(&config);
    ck_assert_ptr_nonnull(g_integration);

    /* Tick should fail when not running */
    int result = nimcp_immune_integration_tick(g_integration, 50);
    ck_assert_int_eq(result, -1);
}
END_TEST

START_TEST(test_tick_null_safe) {
    ck_assert_int_eq(nimcp_immune_integration_tick(NULL, 50), -1);
}
END_TEST

/* ============================================================================
 * Query Tests
 * ============================================================================ */

START_TEST(test_get_immune_system) {
    g_integration = nimcp_immune_integration_create(NULL);
    ck_assert_ptr_nonnull(g_integration);

    brain_immune_system_t* immune = nimcp_immune_integration_get_immune_system(g_integration);
    ck_assert_ptr_nonnull(immune);
}
END_TEST

START_TEST(test_get_immune_system_null_safe) {
    ck_assert_ptr_null(nimcp_immune_integration_get_immune_system(NULL));
}
END_TEST

START_TEST(test_is_running_null_safe) {
    ck_assert(nimcp_immune_integration_is_running(NULL) == false);
}
END_TEST

/* ============================================================================
 * Stats Tests
 * ============================================================================ */

START_TEST(test_get_stats) {
    g_integration = nimcp_immune_integration_create(NULL);
    ck_assert_ptr_nonnull(g_integration);

    nimcp_immune_integration_stats_t stats;
    int result = nimcp_immune_integration_get_stats(g_integration, &stats);
    ck_assert_int_eq(result, 0);
    ck_assert(stats.uptime_ms >= 0);
}
END_TEST

START_TEST(test_get_stats_null_safe) {
    nimcp_immune_integration_stats_t stats;
    ck_assert_int_eq(nimcp_immune_integration_get_stats(NULL, &stats), -1);

    g_integration = nimcp_immune_integration_create(NULL);
    ck_assert_int_eq(nimcp_immune_integration_get_stats(g_integration, NULL), -1);
}
END_TEST

START_TEST(test_reset_stats) {
    g_integration = nimcp_immune_integration_create(NULL);
    ck_assert_ptr_nonnull(g_integration);

    /* Execute some ticks */
    for (int i = 0; i < 5; i++) {
        nimcp_immune_integration_tick(g_integration, 50);
    }

    /* Reset stats */
    nimcp_immune_integration_reset_stats(g_integration);

    /* Check stats are reset */
    nimcp_immune_integration_stats_t stats;
    nimcp_immune_integration_get_stats(g_integration, &stats);
    ck_assert(stats.ticks_executed == 0);
}
END_TEST

START_TEST(test_reset_stats_null_safe) {
    /* Should not crash on NULL */
    nimcp_immune_integration_reset_stats(NULL);
}
END_TEST

/* ============================================================================
 * Diagnostic Tests
 * ============================================================================ */

START_TEST(test_diagnose_healthy) {
    g_integration = nimcp_immune_integration_create(NULL);
    ck_assert_ptr_nonnull(g_integration);

    char buffer[1024];
    int issues = nimcp_immune_integration_diagnose(g_integration, buffer, sizeof(buffer));
    ck_assert_int_eq(issues, 0);
    ck_assert(strstr(buffer, "OK") != NULL);
}
END_TEST

START_TEST(test_diagnose_null) {
    char buffer[1024];
    int issues = nimcp_immune_integration_diagnose(NULL, buffer, sizeof(buffer));
    ck_assert_int_eq(issues, 1);
    ck_assert(strstr(buffer, "NULL") != NULL);
}
END_TEST

START_TEST(test_diagnose_not_running) {
    nimcp_immune_integration_config_t config;
    nimcp_immune_integration_default_config(&config);
    config.auto_start = false;

    g_integration = nimcp_immune_integration_create(&config);
    ck_assert_ptr_nonnull(g_integration);

    char buffer[1024];
    int issues = nimcp_immune_integration_diagnose(g_integration, buffer, sizeof(buffer));
    ck_assert(issues > 0);
    ck_assert(strstr(buffer, "not running") != NULL);
}
END_TEST

START_TEST(test_log_state_null_safe) {
    /* Should not crash on NULL */
    nimcp_immune_integration_log_state(NULL);
}
END_TEST

/* ============================================================================
 * Test Suite Setup
 * ============================================================================ */

Suite* brain_immune_integration_suite(void) {
    Suite* s = suite_create("BrainImmuneIntegration");

    /* Default Config tests */
    TCase* tc_config = tcase_create("DefaultConfig");
    tcase_add_checked_fixture(tc_config, setup, teardown);
    tcase_add_test(tc_config, test_default_config);
    tcase_add_test(tc_config, test_default_config_null_safe);
    suite_add_tcase(s, tc_config);

    /* Create/Destroy tests */
    TCase* tc_lifecycle = tcase_create("Lifecycle");
    tcase_add_checked_fixture(tc_lifecycle, setup, teardown);
    tcase_add_test(tc_lifecycle, test_create_with_defaults);
    tcase_add_test(tc_lifecycle, test_create_with_custom_config);
    tcase_add_test(tc_lifecycle, test_create_no_immune_system_fails);
    tcase_add_test(tc_lifecycle, test_destroy_null_safe);
    suite_add_tcase(s, tc_lifecycle);

    /* Start/Stop tests */
    TCase* tc_startstop = tcase_create("StartStop");
    tcase_add_checked_fixture(tc_startstop, setup, teardown);
    tcase_add_test(tc_startstop, test_start_stop);
    tcase_add_test(tc_startstop, test_start_null_safe);
    tcase_add_test(tc_startstop, test_stop_null_safe);
    suite_add_tcase(s, tc_startstop);

    /* Tick tests */
    TCase* tc_tick = tcase_create("Tick");
    tcase_add_checked_fixture(tc_tick, setup, teardown);
    tcase_add_test(tc_tick, test_tick);
    tcase_add_test(tc_tick, test_tick_not_running);
    tcase_add_test(tc_tick, test_tick_null_safe);
    suite_add_tcase(s, tc_tick);

    /* Query tests */
    TCase* tc_query = tcase_create("Query");
    tcase_add_checked_fixture(tc_query, setup, teardown);
    tcase_add_test(tc_query, test_get_immune_system);
    tcase_add_test(tc_query, test_get_immune_system_null_safe);
    tcase_add_test(tc_query, test_is_running_null_safe);
    suite_add_tcase(s, tc_query);

    /* Stats tests */
    TCase* tc_stats = tcase_create("Stats");
    tcase_add_checked_fixture(tc_stats, setup, teardown);
    tcase_add_test(tc_stats, test_get_stats);
    tcase_add_test(tc_stats, test_get_stats_null_safe);
    tcase_add_test(tc_stats, test_reset_stats);
    tcase_add_test(tc_stats, test_reset_stats_null_safe);
    suite_add_tcase(s, tc_stats);

    /* Diagnostic tests */
    TCase* tc_diag = tcase_create("Diagnostics");
    tcase_add_checked_fixture(tc_diag, setup, teardown);
    tcase_add_test(tc_diag, test_diagnose_healthy);
    tcase_add_test(tc_diag, test_diagnose_null);
    tcase_add_test(tc_diag, test_diagnose_not_running);
    tcase_add_test(tc_diag, test_log_state_null_safe);
    suite_add_tcase(s, tc_diag);

    return s;
}

int main(void) {
    int failed;
    Suite* s = brain_immune_integration_suite();
    SRunner* sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
