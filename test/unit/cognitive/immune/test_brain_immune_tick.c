/**
 * @file test_brain_immune_tick.c
 * @brief Unit tests for brain immune tick orchestrator
 *
 * WHAT: Test suite for brain_immune_tick API
 * WHY:  Verify correct behavior of tick orchestration, health message processing,
 *       and integration between health agent and immune system
 * HOW:  Unit tests using Check framework covering all tick API functions
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
static nimcp_health_agent_t* g_agent = NULL;

static void setup(void)
{
    /* Initialize exception system */
    nimcp_exception_system_init();

    /* Create brain immune system */
    brain_immune_config_t immune_config;
    brain_immune_default_config(&immune_config);
    g_immune = brain_immune_create(&immune_config);
    ck_assert_ptr_nonnull(g_immune);

    /* Initialize tick orchestrator */
    int result = brain_immune_tick_init(g_immune, NULL);
    ck_assert_int_eq(result, 0);
}

static void setup_with_agent(void)
{
    setup();

    /* Create health agent */
    health_agent_config_t agent_config;
    nimcp_health_agent_default_config(&agent_config);
    strcpy(agent_config.agent_name, "test_agent");
    agent_config.check_interval_ms = 100;

    g_agent = nimcp_health_agent_create(&agent_config);
    ck_assert_ptr_nonnull(g_agent);

    /* Connect agent to tick orchestrator */
    int result = brain_immune_tick_connect_health_agent(g_immune, g_agent);
    ck_assert_int_eq(result, 0);
}

static void teardown(void)
{
    if (g_agent) {
        nimcp_health_agent_destroy(g_agent);
        g_agent = NULL;
    }

    if (g_immune) {
        brain_immune_tick_shutdown(g_immune);
        brain_immune_destroy(g_immune);
        g_immune = NULL;
    }

    nimcp_exception_system_shutdown();
}

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

START_TEST(test_default_config)
{
    brain_immune_tick_config_t config;
    brain_immune_tick_default_config(&config);

    ck_assert_uint_eq(config.max_exceptions_per_tick, BRAIN_IMMUNE_TICK_DEFAULT_MAX_EXCEPTIONS);
    ck_assert_uint_eq(config.max_health_msgs_per_tick, BRAIN_IMMUNE_TICK_DEFAULT_MAX_HEALTH_MSGS);
    ck_assert(config.enable_exception_processing);
    ck_assert(config.enable_health_agent_processing);
    ck_assert(config.enable_antigen_processing);
    ck_assert(config.enable_antibody_decay);
    ck_assert(config.enable_inflammation_updates);
    ck_assert(!config.enable_tick_logging);
}
END_TEST

START_TEST(test_default_config_null)
{
    /* Should handle NULL gracefully */
    brain_immune_tick_default_config(NULL);
    /* No crash means success */
}
END_TEST

/* ============================================================================
 * Initialization Tests
 * ============================================================================ */

START_TEST(test_init_basic)
{
    brain_immune_config_t immune_config;
    brain_immune_default_config(&immune_config);
    brain_immune_system_t* immune = brain_immune_create(&immune_config);
    ck_assert_ptr_nonnull(immune);

    int result = brain_immune_tick_init(immune, NULL);
    ck_assert_int_eq(result, 0);
    ck_assert(brain_immune_tick_is_initialized(immune));

    brain_immune_tick_shutdown(immune);
    brain_immune_destroy(immune);
}
END_TEST

START_TEST(test_init_with_config)
{
    brain_immune_config_t immune_config;
    brain_immune_default_config(&immune_config);
    brain_immune_system_t* immune = brain_immune_create(&immune_config);
    ck_assert_ptr_nonnull(immune);

    brain_immune_tick_config_t tick_config;
    brain_immune_tick_default_config(&tick_config);
    tick_config.max_exceptions_per_tick = 5;
    tick_config.max_health_msgs_per_tick = 10;
    tick_config.enable_tick_logging = true;

    int result = brain_immune_tick_init(immune, &tick_config);
    ck_assert_int_eq(result, 0);

    brain_immune_tick_config_t retrieved_config;
    brain_immune_tick_get_config(immune, &retrieved_config);
    ck_assert_uint_eq(retrieved_config.max_exceptions_per_tick, 5);
    ck_assert_uint_eq(retrieved_config.max_health_msgs_per_tick, 10);
    ck_assert(retrieved_config.enable_tick_logging);

    brain_immune_tick_shutdown(immune);
    brain_immune_destroy(immune);
}
END_TEST

START_TEST(test_init_null_immune)
{
    int result = brain_immune_tick_init(NULL, NULL);
    ck_assert_int_eq(result, -1);
}
END_TEST

START_TEST(test_init_double_init)
{
    /* Second init should succeed (idempotent) */
    int result = brain_immune_tick_init(g_immune, NULL);
    ck_assert_int_eq(result, 0);
}
END_TEST

/* ============================================================================
 * Health Agent Connection Tests
 * ============================================================================ */

START_TEST(test_connect_health_agent)
{
    health_agent_config_t agent_config;
    nimcp_health_agent_default_config(&agent_config);
    nimcp_health_agent_t* agent = nimcp_health_agent_create(&agent_config);
    ck_assert_ptr_nonnull(agent);

    ck_assert(!brain_immune_tick_has_health_agent(g_immune));

    int result = brain_immune_tick_connect_health_agent(g_immune, agent);
    ck_assert_int_eq(result, 0);
    ck_assert(brain_immune_tick_has_health_agent(g_immune));

    /* Disconnect */
    result = brain_immune_tick_connect_health_agent(g_immune, NULL);
    ck_assert_int_eq(result, 0);
    ck_assert(!brain_immune_tick_has_health_agent(g_immune));

    nimcp_health_agent_destroy(agent);
}
END_TEST

/* ============================================================================
 * Tick Execution Tests
 * ============================================================================ */

START_TEST(test_tick_basic)
{
    int result = brain_immune_tick(g_immune, 50);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_tick_null_immune)
{
    int result = brain_immune_tick(NULL, 50);
    ck_assert_int_eq(result, -1);
}
END_TEST

START_TEST(test_tick_updates_stats)
{
    brain_immune_tick_stats_t stats_before;
    brain_immune_tick_get_stats(g_immune, &stats_before);

    brain_immune_tick(g_immune, 50);

    brain_immune_tick_stats_t stats_after;
    brain_immune_tick_get_stats(g_immune, &stats_after);

    ck_assert_uint_eq(stats_after.ticks_executed, stats_before.ticks_executed + 1);
}
END_TEST

START_TEST(test_tick_multiple)
{
    for (int i = 0; i < 10; i++) {
        int result = brain_immune_tick(g_immune, 10);
        ck_assert_int_eq(result, 0);
    }

    brain_immune_tick_stats_t stats;
    brain_immune_tick_get_stats(g_immune, &stats);
    ck_assert_uint_eq(stats.ticks_executed, 10);
}
END_TEST

/* ============================================================================
 * Reentry Guard Tests
 * ============================================================================ */

START_TEST(test_reentry_guard_basic)
{
    /* The guard should not be active outside of tick */
    ck_assert(!brain_immune_tick_in_progress());
}
END_TEST

/* Note: Testing actual reentry requires recursive call setup which is complex
 * We verify the guard mechanism exists and basic function works */

/* ============================================================================
 * Health Message Processing Tests
 * ============================================================================ */

START_TEST(test_process_health_message_anomaly)
{
    health_agent_message_t msg = nimcp_health_agent_create_message(
        HEALTH_MSG_ANOMALY_DETECTED,
        HEALTH_SEVERITY_WARNING,
        HEALTH_SOURCE_MEMORY,
        "Test anomaly"
    );

    int result = brain_immune_process_health_message(g_immune, &msg);
    ck_assert_int_eq(result, 0);

    /* Run tick to process */
    brain_immune_tick(g_immune, 50);

    brain_immune_tick_stats_t stats;
    brain_immune_tick_get_stats(g_immune, &stats);
    ck_assert_uint_ge(stats.health_antigens_created, 1);
}
END_TEST

START_TEST(test_process_health_message_cytokine)
{
    health_agent_message_t msg = nimcp_health_agent_create_message(
        HEALTH_MSG_CYTOKINE_SIGNAL,
        HEALTH_SEVERITY_INFO,
        HEALTH_SOURCE_IMMUNE,
        "Test cytokine"
    );

    int result = brain_immune_process_health_message(g_immune, &msg);
    ck_assert_int_eq(result, 0);

    brain_immune_tick_stats_t stats;
    brain_immune_tick_get_stats(g_immune, &stats);
    ck_assert_uint_ge(stats.health_cytokines_released, 1);
}
END_TEST

START_TEST(test_process_health_message_emergency)
{
    health_agent_message_t msg = nimcp_health_agent_create_message(
        HEALTH_MSG_EMERGENCY,
        HEALTH_SEVERITY_CRITICAL,
        HEALTH_SOURCE_HEARTBEAT,
        "Test emergency"
    );

    int result = brain_immune_process_health_message(g_immune, &msg);
    ck_assert_int_eq(result, 0);

    brain_immune_tick_stats_t stats;
    brain_immune_tick_get_stats(g_immune, &stats);
    ck_assert_uint_ge(stats.health_antigens_created, 1);
    ck_assert_uint_ge(stats.recovery_actions_triggered, 1);
}
END_TEST

START_TEST(test_process_health_message_null)
{
    int result = brain_immune_process_health_message(g_immune, NULL);
    ck_assert_int_eq(result, -1);

    result = brain_immune_process_health_message(NULL, NULL);
    ck_assert_int_eq(result, -1);
}
END_TEST

/* ============================================================================
 * Health Queue Processing Tests
 * ============================================================================ */

START_TEST(test_process_health_queue_no_agent)
{
    /* Without agent connected, should return 0 (no messages) */
    int processed = brain_immune_process_health_queue(g_immune, 10);
    ck_assert_int_eq(processed, 0);
}
END_TEST

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

START_TEST(test_stats_reset)
{
    /* Run some ticks */
    brain_immune_tick(g_immune, 50);
    brain_immune_tick(g_immune, 50);

    brain_immune_tick_stats_t stats_before;
    brain_immune_tick_get_stats(g_immune, &stats_before);
    ck_assert_uint_gt(stats_before.ticks_executed, 0);

    /* Reset */
    brain_immune_tick_reset_stats(g_immune);

    brain_immune_tick_stats_t stats_after;
    brain_immune_tick_get_stats(g_immune, &stats_after);
    ck_assert_uint_eq(stats_after.ticks_executed, 0);
    ck_assert_uint_eq(stats_after.exceptions_processed, 0);
    ck_assert_uint_eq(stats_after.health_messages_processed, 0);
}
END_TEST

START_TEST(test_stats_null)
{
    int result = brain_immune_tick_get_stats(g_immune, NULL);
    ck_assert_int_eq(result, -1);

    result = brain_immune_tick_get_stats(NULL, NULL);
    ck_assert_int_eq(result, -1);
}
END_TEST

/* ============================================================================
 * Configuration Update Tests
 * ============================================================================ */

START_TEST(test_config_update)
{
    brain_immune_tick_config_t config;
    brain_immune_tick_default_config(&config);
    config.max_exceptions_per_tick = 20;
    config.enable_tick_logging = true;

    int result = brain_immune_tick_set_config(g_immune, &config);
    ck_assert_int_eq(result, 0);

    brain_immune_tick_config_t retrieved;
    brain_immune_tick_get_config(g_immune, &retrieved);
    ck_assert_uint_eq(retrieved.max_exceptions_per_tick, 20);
    ck_assert(retrieved.enable_tick_logging);
}
END_TEST

START_TEST(test_config_update_null)
{
    int result = brain_immune_tick_set_config(g_immune, NULL);
    ck_assert_int_eq(result, -1);

    result = brain_immune_tick_set_config(NULL, NULL);
    ck_assert_int_eq(result, -1);
}
END_TEST

/* ============================================================================
 * Query Tests
 * ============================================================================ */

START_TEST(test_is_initialized)
{
    ck_assert(brain_immune_tick_is_initialized(g_immune));
    ck_assert(!brain_immune_tick_is_initialized(NULL));
}
END_TEST

START_TEST(test_has_health_agent)
{
    ck_assert(!brain_immune_tick_has_health_agent(g_immune));
    ck_assert(!brain_immune_tick_has_health_agent(NULL));
}
END_TEST

/* ============================================================================
 * Shutdown Tests
 * ============================================================================ */

START_TEST(test_shutdown_null)
{
    /* Should not crash */
    brain_immune_tick_shutdown(NULL);
}
END_TEST

START_TEST(test_shutdown_reinit)
{
    brain_immune_tick_shutdown(g_immune);
    ck_assert(!brain_immune_tick_is_initialized(g_immune));

    int result = brain_immune_tick_init(g_immune, NULL);
    ck_assert_int_eq(result, 0);
    ck_assert(brain_immune_tick_is_initialized(g_immune));
}
END_TEST

/* ============================================================================
 * Test Suite Creation
 * ============================================================================ */

Suite* brain_immune_tick_suite(void)
{
    Suite* s = suite_create("Brain Immune Tick");

    /* Configuration tests */
    TCase* tc_config = tcase_create("Configuration");
    tcase_add_checked_fixture(tc_config, setup, teardown);
    tcase_add_test(tc_config, test_default_config);
    tcase_add_test(tc_config, test_default_config_null);
    suite_add_tcase(s, tc_config);

    /* Initialization tests */
    TCase* tc_init = tcase_create("Initialization");
    tcase_add_test(tc_init, test_init_basic);
    tcase_add_test(tc_init, test_init_with_config);
    tcase_add_test(tc_init, test_init_null_immune);
    tcase_add_checked_fixture(tc_init, setup, teardown);
    tcase_add_test(tc_init, test_init_double_init);
    suite_add_tcase(s, tc_init);

    /* Connection tests */
    TCase* tc_connect = tcase_create("Health Agent Connection");
    tcase_add_checked_fixture(tc_connect, setup, teardown);
    tcase_add_test(tc_connect, test_connect_health_agent);
    suite_add_tcase(s, tc_connect);

    /* Tick execution tests */
    TCase* tc_tick = tcase_create("Tick Execution");
    tcase_add_checked_fixture(tc_tick, setup, teardown);
    tcase_add_test(tc_tick, test_tick_basic);
    tcase_add_test(tc_tick, test_tick_null_immune);
    tcase_add_test(tc_tick, test_tick_updates_stats);
    tcase_add_test(tc_tick, test_tick_multiple);
    suite_add_tcase(s, tc_tick);

    /* Reentry guard tests */
    TCase* tc_reentry = tcase_create("Reentry Guard");
    tcase_add_checked_fixture(tc_reentry, setup, teardown);
    tcase_add_test(tc_reentry, test_reentry_guard_basic);
    suite_add_tcase(s, tc_reentry);

    /* Health message processing tests */
    TCase* tc_health_msg = tcase_create("Health Message Processing");
    tcase_add_checked_fixture(tc_health_msg, setup, teardown);
    tcase_add_test(tc_health_msg, test_process_health_message_anomaly);
    tcase_add_test(tc_health_msg, test_process_health_message_cytokine);
    tcase_add_test(tc_health_msg, test_process_health_message_emergency);
    tcase_add_test(tc_health_msg, test_process_health_message_null);
    suite_add_tcase(s, tc_health_msg);

    /* Health queue processing tests */
    TCase* tc_health_queue = tcase_create("Health Queue Processing");
    tcase_add_checked_fixture(tc_health_queue, setup, teardown);
    tcase_add_test(tc_health_queue, test_process_health_queue_no_agent);
    suite_add_tcase(s, tc_health_queue);

    /* Statistics tests */
    TCase* tc_stats = tcase_create("Statistics");
    tcase_add_checked_fixture(tc_stats, setup, teardown);
    tcase_add_test(tc_stats, test_stats_reset);
    tcase_add_test(tc_stats, test_stats_null);
    suite_add_tcase(s, tc_stats);

    /* Configuration update tests */
    TCase* tc_config_update = tcase_create("Configuration Update");
    tcase_add_checked_fixture(tc_config_update, setup, teardown);
    tcase_add_test(tc_config_update, test_config_update);
    tcase_add_test(tc_config_update, test_config_update_null);
    suite_add_tcase(s, tc_config_update);

    /* Query tests */
    TCase* tc_query = tcase_create("Query");
    tcase_add_checked_fixture(tc_query, setup, teardown);
    tcase_add_test(tc_query, test_is_initialized);
    tcase_add_test(tc_query, test_has_health_agent);
    suite_add_tcase(s, tc_query);

    /* Shutdown tests */
    TCase* tc_shutdown = tcase_create("Shutdown");
    tcase_add_checked_fixture(tc_shutdown, setup, teardown);
    tcase_add_test(tc_shutdown, test_shutdown_null);
    tcase_add_test(tc_shutdown, test_shutdown_reinit);
    suite_add_tcase(s, tc_shutdown);

    return s;
}

int main(void)
{
    int number_failed;
    Suite* s = brain_immune_tick_suite();
    SRunner* sr = srunner_create(s);

    /* Run all tests */
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
