/**
 * @file test_brain_immune_tick_integration.c
 * @brief Integration tests for brain immune tick orchestrator
 *
 * WHAT: Integration tests verifying end-to-end immune tick functionality
 * WHY:  Verify health agent + immune system + tick orchestrator work together
 * HOW:  Tests using Check framework covering integration scenarios
 *
 * @author NIMCP Development Team
 * @date 2025-01-25
 */

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

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

static void setup_full(void)
{
    /* Initialize exception system */
    nimcp_exception_system_init();

    /* Create brain immune system */
    brain_immune_config_t immune_config;
    brain_immune_default_config(&immune_config);
    g_immune = brain_immune_create(&immune_config);
    ck_assert_ptr_nonnull(g_immune);

    /* Initialize tick orchestrator */
    brain_immune_tick_config_t tick_config;
    brain_immune_tick_default_config(&tick_config);
    tick_config.enable_tick_logging = false;
    int result = brain_immune_tick_init(g_immune, &tick_config);
    ck_assert_int_eq(result, 0);

    /* Create health agent */
    health_agent_config_t agent_config;
    nimcp_health_agent_default_config(&agent_config);
    strcpy(agent_config.agent_name, "integration_test_agent");
    agent_config.check_interval_ms = 50;

    g_agent = nimcp_health_agent_create(&agent_config);
    ck_assert_ptr_nonnull(g_agent);

    /* Connect agent to immune system */
    nimcp_health_agent_connect_immune(g_agent, g_immune);

    /* Connect agent to tick orchestrator */
    result = brain_immune_tick_connect_health_agent(g_immune, g_agent);
    ck_assert_int_eq(result, 0);

    /* Connect exception system to immune system */
    nimcp_exception_immune_init(g_immune);
}

static void teardown_full(void)
{
    nimcp_exception_immune_shutdown();

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
 * Health Agent -> Tick -> Immune Integration Tests
 * ============================================================================ */

START_TEST(test_health_agent_to_immune_flow)
{
    /* Report an anomaly via health agent */
    health_agent_message_t msg = nimcp_health_agent_create_message(
        HEALTH_MSG_ANOMALY_DETECTED,
        HEALTH_SEVERITY_ERROR,
        HEALTH_SOURCE_MEMORY,
        "Integration test: memory anomaly"
    );
    int result = nimcp_health_agent_report_anomaly(g_agent, &msg);
    ck_assert_int_eq(result, 0);

    /* Verify message is queued */
    uint32_t queue_depth = nimcp_health_agent_get_queue_depth(g_agent);
    ck_assert_uint_ge(queue_depth, 1);

    /* Run tick to process the message */
    result = brain_immune_tick(g_immune, 50);
    ck_assert_int_eq(result, 0);

    /* Verify message was processed */
    brain_immune_tick_stats_t stats;
    brain_immune_tick_get_stats(g_immune, &stats);
    ck_assert_uint_ge(stats.health_messages_processed, 1);
    ck_assert_uint_ge(stats.health_antigens_created, 1);

    /* Verify immune system received the antigen */
    brain_immune_metrics_t immune_metrics;
    brain_immune_get_metrics(g_immune, &immune_metrics);
    ck_assert_uint_ge(immune_metrics.total_antigens_presented, 1);
}
END_TEST

START_TEST(test_exception_to_immune_flow)
{
    /* Throw an exception via NIMCP_THROW_TO_IMMUNE */
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Integration test: out of memory");

    /* Run tick to process async exceptions */
    int result = brain_immune_tick(g_immune, 50);
    ck_assert_int_eq(result, 0);

    /* Verify exception was processed */
    brain_immune_tick_stats_t stats;
    brain_immune_tick_get_stats(g_immune, &stats);
    ck_assert_uint_ge(stats.exceptions_processed, 1);

    /* Verify immune system received the antigen */
    brain_immune_metrics_t immune_metrics;
    brain_immune_get_metrics(g_immune, &immune_metrics);
    ck_assert_uint_ge(immune_metrics.total_antigens_presented, 1);
}
END_TEST

START_TEST(test_multiple_sources_integration)
{
    /* 1. Report anomaly via health agent */
    health_agent_message_t msg1 = nimcp_health_agent_create_message(
        HEALTH_MSG_ANOMALY_DETECTED,
        HEALTH_SEVERITY_WARNING,
        HEALTH_SOURCE_THREADING,
        "Thread contention detected"
    );
    nimcp_health_agent_report_anomaly(g_agent, &msg1);

    /* 2. Throw exception */
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_TIMEOUT, "Operation timed out");

    /* 3. Report another anomaly */
    health_agent_message_t msg2 = nimcp_health_agent_create_message(
        HEALTH_MSG_NAN_DETECTED,
        HEALTH_SEVERITY_ERROR,
        HEALTH_SOURCE_NEURAL,
        "NaN in layer 3"
    );
    msg2.data.nan.layer_id = 3;
    msg2.data.nan.neuron_id = 42;
    msg2.data.nan.nan_count = 5;
    nimcp_health_agent_report_anomaly(g_agent, &msg2);

    /* Run tick */
    brain_immune_tick(g_immune, 50);

    /* Verify all were processed */
    brain_immune_tick_stats_t stats;
    brain_immune_tick_get_stats(g_immune, &stats);
    ck_assert_uint_ge(stats.health_messages_processed, 2);
    ck_assert_uint_ge(stats.exceptions_processed, 1);
}
END_TEST

/* ============================================================================
 * Recovery Action Integration Tests
 * ============================================================================ */

START_TEST(test_recovery_request_integration)
{
    /* Request a recovery action via health agent */
    health_agent_message_t msg = nimcp_health_agent_create_message(
        HEALTH_MSG_RECOVERY_REQUEST,
        HEALTH_SEVERITY_WARNING,
        HEALTH_SOURCE_IMMUNE,
        "Request GC"
    );
    msg.suggested_action = HEALTH_RECOVERY_GC;
    nimcp_health_agent_report_anomaly(g_agent, &msg);

    /* Run tick */
    brain_immune_tick(g_immune, 50);

    /* Verify recovery was triggered */
    brain_immune_tick_stats_t stats;
    brain_immune_tick_get_stats(g_immune, &stats);
    ck_assert_uint_ge(stats.recovery_actions_triggered, 1);
}
END_TEST

START_TEST(test_emergency_response_integration)
{
    /* Report emergency */
    health_agent_message_t msg = nimcp_health_agent_create_message(
        HEALTH_MSG_EMERGENCY,
        HEALTH_SEVERITY_CRITICAL,
        HEALTH_SOURCE_HEARTBEAT,
        "System unresponsive"
    );
    nimcp_health_agent_report_anomaly(g_agent, &msg);

    /* Run tick */
    brain_immune_tick(g_immune, 50);

    /* Verify emergency was handled */
    brain_immune_tick_stats_t stats;
    brain_immune_tick_get_stats(g_immune, &stats);
    ck_assert_uint_ge(stats.health_antigens_created, 1);
    ck_assert_uint_ge(stats.recovery_actions_triggered, 1);

    /* Verify inflammation was initiated */
    brain_immune_metrics_t immune_metrics;
    brain_immune_get_metrics(g_immune, &immune_metrics);
    /* Emergency should cause inflammation */
}
END_TEST

/* ============================================================================
 * Agent Thread Integration Tests
 * ============================================================================ */

START_TEST(test_agent_thread_tick_integration)
{
    /* Start the health agent */
    int result = nimcp_health_agent_start(g_agent);
    ck_assert_int_eq(result, 0);

    /* Wait for agent to run some ticks */
    usleep(200000);  /* 200ms */

    /* Report anomaly while agent is running */
    health_agent_message_t msg = nimcp_health_agent_create_message(
        HEALTH_MSG_ANOMALY_DETECTED,
        HEALTH_SEVERITY_WARNING,
        HEALTH_SOURCE_IO,
        "IO latency spike"
    );
    nimcp_health_agent_report_anomaly(g_agent, &msg);

    /* Wait for processing */
    usleep(100000);  /* 100ms */

    /* Stop agent */
    nimcp_health_agent_stop(g_agent);

    /* Verify tick was called */
    brain_immune_tick_stats_t stats;
    brain_immune_tick_get_stats(g_immune, &stats);
    ck_assert_uint_ge(stats.ticks_executed, 1);
}
END_TEST

/* ============================================================================
 * Memory Formation Integration Tests
 * ============================================================================ */

START_TEST(test_immune_memory_formation)
{
    /* Present same anomaly pattern multiple times */
    for (int i = 0; i < 3; i++) {
        health_agent_message_t msg = nimcp_health_agent_create_message(
            HEALTH_MSG_ANOMALY_DETECTED,
            HEALTH_SEVERITY_WARNING,
            HEALTH_SOURCE_NEURAL,
            "Pattern: NaN in forward pass"
        );
        nimcp_health_agent_report_anomaly(g_agent, &msg);
        brain_immune_tick(g_immune, 50);
        usleep(10000);  /* 10ms between anomalies */
    }

    /* Verify multiple antigens were presented */
    brain_immune_metrics_t metrics;
    brain_immune_get_metrics(g_immune, &metrics);
    ck_assert_uint_ge(metrics.total_antigens_presented, 3);

    /* Run immune update to trigger memory formation */
    brain_immune_tick(g_immune, 100);

    /* Check for B cell activation (memory pathway) */
    ck_assert_uint_ge(metrics.b_cells_activated + metrics.t_cells_activated, 1);
}
END_TEST

/* ============================================================================
 * Concurrent Access Integration Tests
 * ============================================================================ */

static void* reporter_thread(void* arg)
{
    brain_immune_system_t* immune = (brain_immune_system_t*)arg;
    (void)immune;

    for (int i = 0; i < 10; i++) {
        health_agent_message_t msg = nimcp_health_agent_create_message(
            HEALTH_MSG_ANOMALY_DETECTED,
            HEALTH_SEVERITY_INFO,
            HEALTH_SOURCE_MEMORY,
            "Concurrent anomaly %d", i
        );
        nimcp_health_agent_report_anomaly(g_agent, &msg);
        usleep(5000);  /* 5ms */
    }
    return NULL;
}

static void* ticker_thread(void* arg)
{
    brain_immune_system_t* immune = (brain_immune_system_t*)arg;

    for (int i = 0; i < 20; i++) {
        brain_immune_tick(immune, 10);
        usleep(5000);  /* 5ms */
    }
    return NULL;
}

START_TEST(test_concurrent_reporting_and_ticking)
{
    pthread_t reporter, ticker;

    pthread_create(&reporter, NULL, reporter_thread, g_immune);
    pthread_create(&ticker, NULL, ticker_thread, g_immune);

    pthread_join(reporter, NULL);
    pthread_join(ticker, NULL);

    /* Verify no crashes and some messages were processed */
    brain_immune_tick_stats_t stats;
    brain_immune_tick_get_stats(g_immune, &stats);
    ck_assert_uint_ge(stats.ticks_executed, 10);
    /* Some messages should have been processed */
}
END_TEST

/* ============================================================================
 * Configuration Integration Tests
 * ============================================================================ */

START_TEST(test_limited_processing_per_tick)
{
    /* Configure to process only 2 messages per tick */
    brain_immune_tick_config_t config;
    brain_immune_tick_default_config(&config);
    config.max_health_msgs_per_tick = 2;
    brain_immune_tick_set_config(g_immune, &config);

    /* Queue 5 messages */
    for (int i = 0; i < 5; i++) {
        health_agent_message_t msg = nimcp_health_agent_create_message(
            HEALTH_MSG_ANOMALY_DETECTED,
            HEALTH_SEVERITY_INFO,
            HEALTH_SOURCE_MEMORY,
            "Batch message %d", i
        );
        nimcp_health_agent_report_anomaly(g_agent, &msg);
    }

    /* Run one tick - should process at most 2 */
    brain_immune_tick(g_immune, 50);

    /* Should still have messages in queue */
    uint32_t queue_depth = nimcp_health_agent_get_queue_depth(g_agent);
    ck_assert_uint_ge(queue_depth, 2);  /* At least 3 remaining */

    /* Run more ticks to drain */
    brain_immune_tick(g_immune, 50);
    brain_immune_tick(g_immune, 50);

    /* Now queue should be empty */
    queue_depth = nimcp_health_agent_get_queue_depth(g_agent);
    ck_assert_uint_eq(queue_depth, 0);
}
END_TEST

/* ============================================================================
 * Test Suite Creation
 * ============================================================================ */

Suite* brain_immune_tick_integration_suite(void)
{
    Suite* s = suite_create("Brain Immune Tick Integration");

    /* Health agent to immune flow */
    TCase* tc_flow = tcase_create("Health Agent Flow");
    tcase_add_checked_fixture(tc_flow, setup_full, teardown_full);
    tcase_add_test(tc_flow, test_health_agent_to_immune_flow);
    tcase_add_test(tc_flow, test_exception_to_immune_flow);
    tcase_add_test(tc_flow, test_multiple_sources_integration);
    tcase_set_timeout(tc_flow, 10);
    suite_add_tcase(s, tc_flow);

    /* Recovery actions */
    TCase* tc_recovery = tcase_create("Recovery Actions");
    tcase_add_checked_fixture(tc_recovery, setup_full, teardown_full);
    tcase_add_test(tc_recovery, test_recovery_request_integration);
    tcase_add_test(tc_recovery, test_emergency_response_integration);
    tcase_set_timeout(tc_recovery, 10);
    suite_add_tcase(s, tc_recovery);

    /* Agent thread integration */
    TCase* tc_thread = tcase_create("Agent Thread");
    tcase_add_checked_fixture(tc_thread, setup_full, teardown_full);
    tcase_add_test(tc_thread, test_agent_thread_tick_integration);
    tcase_set_timeout(tc_thread, 5);
    suite_add_tcase(s, tc_thread);

    /* Memory formation */
    TCase* tc_memory = tcase_create("Memory Formation");
    tcase_add_checked_fixture(tc_memory, setup_full, teardown_full);
    tcase_add_test(tc_memory, test_immune_memory_formation);
    tcase_set_timeout(tc_memory, 10);
    suite_add_tcase(s, tc_memory);

    /* Concurrent access */
    TCase* tc_concurrent = tcase_create("Concurrent Access");
    tcase_add_checked_fixture(tc_concurrent, setup_full, teardown_full);
    tcase_add_test(tc_concurrent, test_concurrent_reporting_and_ticking);
    tcase_set_timeout(tc_concurrent, 10);
    suite_add_tcase(s, tc_concurrent);

    /* Configuration */
    TCase* tc_config = tcase_create("Configuration");
    tcase_add_checked_fixture(tc_config, setup_full, teardown_full);
    tcase_add_test(tc_config, test_limited_processing_per_tick);
    tcase_set_timeout(tc_config, 10);
    suite_add_tcase(s, tc_config);

    return s;
}

int main(void)
{
    int number_failed;
    Suite* s = brain_immune_tick_integration_suite();
    SRunner* sr = srunner_create(s);

    /* Run all tests */
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
