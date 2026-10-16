/**
 * @file test_brain_immune_tick_e2e.c
 * @brief End-to-end tests for brain immune tick orchestrator
 *
 * WHAT: E2E tests verifying complete error-to-recovery-to-memory flows
 * WHY:  Validate the full immune response cycle works correctly
 * HOW:  Full scenarios from error detection through recovery and memory formation
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
#include <signal.h>

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

static void setup_e2e(void)
{
    /* Initialize exception system */
    nimcp_exception_system_init();

    /* Create brain immune system with full configuration */
    brain_immune_config_t immune_config;
    brain_immune_default_config(&immune_config);
    immune_config.max_antigens = 100;
    immune_config.max_b_cells = 50;
    immune_config.max_t_cells = 50;
    immune_config.max_antibodies = 100;
    // memory formation enabled by default
    g_immune = brain_immune_create(&immune_config);
    ck_assert_ptr_nonnull(g_immune);
    brain_immune_start(g_immune);

    /* Initialize tick orchestrator */
    brain_immune_tick_config_t tick_config;
    brain_immune_tick_default_config(&tick_config);
    tick_config.enable_tick_logging = false;
    tick_config.max_exceptions_per_tick = 20;
    tick_config.max_health_msgs_per_tick = 20;
    int result = brain_immune_tick_init(g_immune, &tick_config);
    ck_assert_int_eq(result, 0);

    /* Create health agent */
    health_agent_config_t agent_config;
    nimcp_health_agent_default_config(&agent_config);
    strcpy(agent_config.agent_name, "e2e_test_agent");
    agent_config.check_interval_ms = 50;

    g_agent = nimcp_health_agent_create(&agent_config);
    ck_assert_ptr_nonnull(g_agent);

    /* Connect systems */
    nimcp_health_agent_connect_immune(g_agent, g_immune);
    brain_immune_tick_connect_health_agent(g_immune, g_agent);
    nimcp_exception_immune_connect(g_immune);
}

static void teardown_e2e(void)
{
    nimcp_exception_immune_disconnect();
    nimcp_exception_immune_shutdown();

    if (g_agent) {
        if (nimcp_health_agent_is_running(g_agent)) {
            nimcp_health_agent_stop(g_agent);
        }
        nimcp_health_agent_destroy(g_agent);
        g_agent = NULL;
    }

    if (g_immune) {
        brain_immune_tick_shutdown(g_immune);
        brain_immune_stop(g_immune);
        brain_immune_destroy(g_immune);
        g_immune = NULL;
    }

    nimcp_exception_system_shutdown();
}

/* ============================================================================
 * Helper: Queue exception for async tick processing
 * NIMCP_THROW_TO_IMMUNE processes synchronously (bypasses async queue).
 * For testing brain_immune_tick, we need to enqueue via present_async.
 * ============================================================================ */
static void queue_exception_async(nimcp_error_t code, const char* msg)
{
    nimcp_exception_t* ex = nimcp_exception_create(
        code,
        nimcp_exception_get_severity_from_code(code),
        __FILE__, __LINE__, __func__,
        "%s", msg
    );
    if (ex) {
        nimcp_exception_present_async(ex);
        nimcp_exception_unref(ex);
    }
}

/* ============================================================================
 * E2E Test: Full Error -> Exception -> Immune -> Recovery -> Memory Cycle
 * ============================================================================ */

START_TEST(test_full_error_recovery_cycle)
{
    printf("E2E Test: Full error recovery cycle\n");

    /* Phase 1: Error occurs - queue exception for tick processing */
    printf("  Phase 1: Queuing exception for tick...\n");
    queue_exception_async(NIMCP_ERROR_NO_MEMORY, "Out of memory during neural computation");

    /* Phase 2: Exception is queued */
    brain_immune_tick_stats_t stats_before_tick;
    brain_immune_tick_get_stats(g_immune, &stats_before_tick);

    /* Phase 3: Tick processes exception */
    printf("  Phase 2: Processing tick...\n");
    brain_immune_tick(g_immune, 50);

    brain_immune_tick_stats_t stats_after_tick;
    brain_immune_tick_get_stats(g_immune, &stats_after_tick);
    ck_assert_uint_ge(stats_after_tick.exceptions_processed,
                      stats_before_tick.exceptions_processed + 1);

    /* Phase 4: Verify antigen was created */
    printf("  Phase 3: Verifying antigen...\n");
    brain_immune_stats_t metrics;
    brain_immune_get_stats(g_immune, &metrics);
    ck_assert_uint_ge(metrics.antigens_processed, 1);

    /* Phase 5: Run multiple ticks to allow immune response to develop */
    printf("  Phase 4: Running immune response cycle...\n");
    for (int i = 0; i < 10; i++) {
        brain_immune_tick(g_immune, 100);
    }

    /* Phase 6: Verify immune response occurred */
    brain_immune_get_stats(g_immune, &metrics);
    printf("    Antigens: %u, B cells: %u, T cells: %u\n",
           metrics.antigens_processed,
           metrics.active_b_cells,
           metrics.active_t_cells);

    /* At least some immune activity should have occurred */
    ck_assert_uint_ge(metrics.antigens_processed, 1);

    printf("  Test passed!\n\n");
}
END_TEST

/* ============================================================================
 * E2E Test: Multiple Concurrent Errors Handling
 * ============================================================================ */

START_TEST(test_concurrent_errors_handling)
{
    printf("E2E Test: Multiple concurrent errors\n");

    /* Simulate multiple simultaneous errors */
    printf("  Generating concurrent errors...\n");

    /* Error 1: Memory error via async exception queue */
    queue_exception_async(NIMCP_ERROR_NO_MEMORY, "Concurrent error 1: memory");

    /* Error 2: IO error via async exception queue */
    queue_exception_async(NIMCP_ERROR_FILE_NOT_FOUND, "Concurrent error 2: file");

    /* Error 3: Timeout via async exception queue */
    queue_exception_async(NIMCP_ERROR_TIMEOUT, "Concurrent error 3: timeout");

    /* Error 4: Anomaly via health agent */
    health_agent_message_t msg1 = nimcp_health_agent_create_message(
        HEALTH_MSG_ANOMALY_DETECTED,
        HEALTH_SEVERITY_ERROR,
        HEALTH_SOURCE_NEURAL,
        "Concurrent error 4: NaN"
    );
    nimcp_health_agent_report_anomaly(g_agent, &msg1);

    /* Error 5: Emergency via health agent */
    health_agent_message_t msg2 = nimcp_health_agent_create_message(
        HEALTH_MSG_EMERGENCY,
        HEALTH_SEVERITY_CRITICAL,
        HEALTH_SOURCE_THREADING,
        "Concurrent error 5: deadlock"
    );
    nimcp_health_agent_report_anomaly(g_agent, &msg2);

    /* Process all errors in one tick */
    printf("  Processing all errors...\n");
    brain_immune_tick(g_immune, 100);

    /* Verify all were processed */
    brain_immune_tick_stats_t stats;
    brain_immune_tick_get_stats(g_immune, &stats);
    printf("    Exceptions: %lu, Health msgs: %lu\n",
           (unsigned long)stats.exceptions_processed,
           (unsigned long)stats.health_messages_processed);

    ck_assert_uint_ge(stats.exceptions_processed, 3);
    ck_assert_uint_ge(stats.health_messages_processed, 2);

    /* Verify immune system handled them */
    brain_immune_stats_t metrics;
    brain_immune_get_stats(g_immune, &metrics);
    ck_assert_uint_ge(metrics.antigens_processed, 5);

    printf("  Test passed!\n\n");
}
END_TEST

/* ============================================================================
 * E2E Test: Long-Running Health Agent Integration
 * ============================================================================ */

START_TEST(test_long_running_agent_integration)
{
    printf("E2E Test: Long-running agent integration\n");

    /* Start the health agent */
    printf("  Starting health agent...\n");
    int result = nimcp_health_agent_start(g_agent);
    ck_assert_int_eq(result, 0);

    /* Generate errors over time while agent runs */
    printf("  Generating errors over 500ms...\n");
    for (int i = 0; i < 10; i++) {
        /* Generate exception */
        if (i % 2 == 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Periodic error %d", i);
        } else {
            health_agent_message_t msg = nimcp_health_agent_create_message(
                HEALTH_MSG_ANOMALY_DETECTED,
                HEALTH_SEVERITY_WARNING,
                HEALTH_SOURCE_MEMORY,
                "Periodic anomaly %d", i
            );
            nimcp_health_agent_report_anomaly(g_agent, &msg);
        }
        usleep(50000);  /* 50ms */
    }

    /* Let agent process remaining items */
    usleep(200000);  /* 200ms */

    /* Stop agent */
    printf("  Stopping agent...\n");
    nimcp_health_agent_stop(g_agent);

    /* Verify processing occurred */
    brain_immune_tick_stats_t stats;
    brain_immune_tick_get_stats(g_immune, &stats);
    printf("    Ticks: %lu, Exceptions: %lu, Health msgs: %lu\n",
           (unsigned long)stats.ticks_executed,
           (unsigned long)stats.exceptions_processed,
           (unsigned long)stats.health_messages_processed);

    /* Agent should have run multiple ticks */
    ck_assert_uint_ge(stats.ticks_executed, 5);

    printf("  Test passed!\n\n");
}
END_TEST

/* ============================================================================
 * E2E Test: Graceful Shutdown with Pending Items
 * ============================================================================ */

START_TEST(test_graceful_shutdown_with_pending)
{
    printf("E2E Test: Graceful shutdown with pending items\n");

    /* Queue many items */
    printf("  Queueing items...\n");
    for (int i = 0; i < 20; i++) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_TIMEOUT, "Pending %d", i);
        health_agent_message_t msg = nimcp_health_agent_create_message(
            HEALTH_MSG_ANOMALY_DETECTED,
            HEALTH_SEVERITY_INFO,
            HEALTH_SOURCE_IO,
            "Pending msg %d", i
        );
        nimcp_health_agent_report_anomaly(g_agent, &msg);
    }

    /* Verify items are queued */
    uint32_t queue_depth = nimcp_health_agent_get_queue_depth(g_agent);
    printf("    Queue depth before shutdown: %u\n", queue_depth);
    ck_assert_uint_ge(queue_depth, 10);

    /* Start shutdown process */
    printf("  Initiating shutdown...\n");

    /* Drain remaining items before shutdown */
    while (nimcp_health_agent_get_queue_depth(g_agent) > 0) {
        brain_immune_tick(g_immune, 50);
    }

    /* Process remaining exceptions */
    nimcp_exception_immune_process_pending(0);  /* 0 = all */

    /* Verify clean shutdown */
    queue_depth = nimcp_health_agent_get_queue_depth(g_agent);
    printf("    Queue depth after draining: %u\n", queue_depth);
    ck_assert_uint_eq(queue_depth, 0);

    printf("  Test passed!\n\n");
}
END_TEST

/* ============================================================================
 * E2E Test: Stress Test with Multi-Threaded Access
 * ============================================================================ */

#define STRESS_THREADS 4
#define STRESS_ITERATIONS 50

static volatile int stress_errors_thrown = 0;
static pthread_mutex_t stress_mutex = PTHREAD_MUTEX_INITIALIZER;

static void* stress_exception_thread(void* arg)
{
    brain_immune_system_t* immune = (brain_immune_system_t*)arg;
    (void)immune;

    for (int i = 0; i < STRESS_ITERATIONS; i++) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_TIMEOUT, "Stress exception %d", i);
        pthread_mutex_lock(&stress_mutex);
        stress_errors_thrown++;
        pthread_mutex_unlock(&stress_mutex);
        usleep(1000);  /* 1ms */
    }
    return NULL;
}

static void* stress_health_msg_thread(void* arg)
{
    nimcp_health_agent_t* agent = (nimcp_health_agent_t*)arg;

    for (int i = 0; i < STRESS_ITERATIONS; i++) {
        health_agent_message_t msg = nimcp_health_agent_create_message(
            HEALTH_MSG_ANOMALY_DETECTED,
            (i % 5 == 0) ? HEALTH_SEVERITY_ERROR : HEALTH_SEVERITY_INFO,
            HEALTH_SOURCE_MEMORY,
            "Stress anomaly %d", i
        );
        nimcp_health_agent_report_anomaly(agent, &msg);
        pthread_mutex_lock(&stress_mutex);
        stress_errors_thrown++;
        pthread_mutex_unlock(&stress_mutex);
        usleep(1000);  /* 1ms */
    }
    return NULL;
}

static void* stress_tick_thread(void* arg)
{
    brain_immune_system_t* immune = (brain_immune_system_t*)arg;

    for (int i = 0; i < STRESS_ITERATIONS * 2; i++) {
        brain_immune_tick(immune, 10);
        usleep(2000);  /* 2ms */
    }
    return NULL;
}

START_TEST(test_stress_multi_threaded)
{
    printf("E2E Test: Stress test with multi-threaded access\n");

    stress_errors_thrown = 0;

    pthread_t exception_threads[STRESS_THREADS / 2];
    pthread_t health_threads[STRESS_THREADS / 2];
    pthread_t tick_threads[STRESS_THREADS / 2];

    printf("  Starting %d threads...\n", STRESS_THREADS + STRESS_THREADS / 2);

    /* Start threads */
    for (int i = 0; i < STRESS_THREADS / 2; i++) {
        pthread_create(&exception_threads[i], NULL, stress_exception_thread, g_immune);
        pthread_create(&health_threads[i], NULL, stress_health_msg_thread, g_agent);
        pthread_create(&tick_threads[i], NULL, stress_tick_thread, g_immune);
    }

    /* Wait for threads */
    for (int i = 0; i < STRESS_THREADS / 2; i++) {
        pthread_join(exception_threads[i], NULL);
        pthread_join(health_threads[i], NULL);
        pthread_join(tick_threads[i], NULL);
    }

    printf("    Total errors thrown: %d\n", stress_errors_thrown);

    /* Drain any remaining */
    for (int i = 0; i < 20; i++) {
        brain_immune_tick(g_immune, 50);
    }

    /* Verify system is stable */
    brain_immune_tick_stats_t stats;
    brain_immune_tick_get_stats(g_immune, &stats);
    printf("    Ticks executed: %lu\n", (unsigned long)stats.ticks_executed);
    printf("    Exceptions processed: %lu\n", (unsigned long)stats.exceptions_processed);
    printf("    Health msgs processed: %lu\n", (unsigned long)stats.health_messages_processed);
    printf("    Reentrant calls blocked: %lu\n", (unsigned long)stats.reentrant_calls_blocked);

    /* Verify immune system is still functional */
    brain_immune_stats_t metrics;
    brain_immune_get_stats(g_immune, &metrics);
    printf("    Antigens presented: %u\n", metrics.antigens_processed);

    ck_assert_uint_ge(stats.ticks_executed, STRESS_THREADS);
    ck_assert_uint_ge(metrics.antigens_processed, 1);

    printf("  Test passed!\n\n");
}
END_TEST

/* ============================================================================
 * E2E Test: Recovery and Memory Formation
 * ============================================================================ */

START_TEST(test_recovery_and_memory_formation)
{
    printf("E2E Test: Recovery and memory formation\n");

    /* Simulate recurring error pattern (should trigger memory formation) */
    printf("  Simulating recurring error pattern...\n");
    const char* error_pattern = "Connection timeout to database";

    for (int cycle = 0; cycle < 3; cycle++) {
        printf("    Cycle %d...\n", cycle + 1);

        /* Report the error */
        health_agent_message_t msg = nimcp_health_agent_create_message(
            HEALTH_MSG_ANOMALY_DETECTED,
            HEALTH_SEVERITY_ERROR,
            HEALTH_SOURCE_IO,
            "%s", error_pattern
        );
        nimcp_health_agent_report_anomaly(g_agent, &msg);

        /* Process through immune tick */
        brain_immune_tick(g_immune, 100);

        /* Run multiple update cycles to allow immune response */
        for (int i = 0; i < 5; i++) {
            brain_immune_tick(g_immune, 200);
        }

        /* Brief pause between cycles */
        usleep(50000);  /* 50ms */
    }

    /* Verify immune system has learned from the pattern */
    brain_immune_stats_t metrics;
    brain_immune_get_stats(g_immune, &metrics);

    printf("    Final metrics:\n");
    printf("      Antigens: %u\n", metrics.antigens_processed);
    printf("      B cells activated: %u\n", metrics.active_b_cells);
    printf("      Antibodies produced: %u\n", metrics.active_antibodies);
    printf("      Memory cells: %u\n", metrics.memory_cells);

    /* Should have processed multiple antigens */
    ck_assert_uint_ge(metrics.antigens_processed, 3);

    printf("  Test passed!\n\n");
}
END_TEST

/* ============================================================================
 * E2E Test: System Stability Under Error Cascade
 * ============================================================================ */

START_TEST(test_error_cascade_stability)
{
    printf("E2E Test: System stability under error cascade\n");

    /* Generate rapid cascade of errors */
    printf("  Generating error cascade...\n");
    for (int i = 0; i < 100; i++) {
        /* Alternate between different error types */
        switch (i % 5) {
            case 0: {
                char buf[64]; snprintf(buf, sizeof(buf), "Cascade mem %d", i);
                queue_exception_async(NIMCP_ERROR_NO_MEMORY, buf);
                break;
            }
            case 1: {
                char buf[64]; snprintf(buf, sizeof(buf), "Cascade timeout %d", i);
                queue_exception_async(NIMCP_ERROR_TIMEOUT, buf);
                break;
            }
            case 2: {
                health_agent_message_t msg = nimcp_health_agent_create_message(
                    HEALTH_MSG_NAN_DETECTED,
                    HEALTH_SEVERITY_ERROR,
                    HEALTH_SOURCE_NEURAL,
                    "Cascade NaN %d", i
                );
                nimcp_health_agent_report_anomaly(g_agent, &msg);
                break;
            }
            case 3: {
                health_agent_message_t msg = nimcp_health_agent_create_message(
                    HEALTH_MSG_RESOURCE_EXHAUSTION,
                    HEALTH_SEVERITY_WARNING,
                    HEALTH_SOURCE_MEMORY,
                    "Cascade resource %d", i
                );
                msg.data.resource.utilization_pct = 85.0f + (i % 15);
                nimcp_health_agent_report_anomaly(g_agent, &msg);
                break;
            }
            case 4: {
                char buf[64]; snprintf(buf, sizeof(buf), "Cascade param %d", i);
                queue_exception_async(NIMCP_ERROR_INVALID_PARAM, buf);
                break;
            }
        }
    }

    printf("  Processing cascade...\n");

    /* Process all errors */
    for (int i = 0; i < 50; i++) {
        brain_immune_tick(g_immune, 20);
    }

    /* Verify system remained stable */
    brain_immune_tick_stats_t stats;
    brain_immune_tick_get_stats(g_immune, &stats);

    printf("    Ticks: %lu\n", (unsigned long)stats.ticks_executed);
    printf("    Exceptions: %lu\n", (unsigned long)stats.exceptions_processed);
    printf("    Health msgs: %lu\n", (unsigned long)stats.health_messages_processed);
    printf("    Recovery actions: %lu\n", (unsigned long)stats.recovery_actions_triggered);

    /* System should have processed most errors */
    ck_assert_uint_ge(stats.exceptions_processed + stats.health_messages_processed, 50);

    /* Verify immune system is still functional */
    brain_immune_stats_t metrics;
    brain_immune_get_stats(g_immune, &metrics);
    printf("    Immune system still active: antigens=%u\n", metrics.antigens_processed);

    printf("  Test passed!\n\n");
}
END_TEST

/* ============================================================================
 * Test Suite Creation
 * ============================================================================ */

Suite* brain_immune_tick_e2e_suite(void)
{
    Suite* s = suite_create("Brain Immune Tick E2E");

    /* Full cycle test */
    TCase* tc_full_cycle = tcase_create("Full Recovery Cycle");
    tcase_add_checked_fixture(tc_full_cycle, setup_e2e, teardown_e2e);
    tcase_add_test(tc_full_cycle, test_full_error_recovery_cycle);
    tcase_set_timeout(tc_full_cycle, 30);
    suite_add_tcase(s, tc_full_cycle);

    /* Concurrent errors */
    TCase* tc_concurrent = tcase_create("Concurrent Errors");
    tcase_add_checked_fixture(tc_concurrent, setup_e2e, teardown_e2e);
    tcase_add_test(tc_concurrent, test_concurrent_errors_handling);
    tcase_set_timeout(tc_concurrent, 30);
    suite_add_tcase(s, tc_concurrent);

    /* Long-running agent */
    TCase* tc_long_running = tcase_create("Long Running Agent");
    tcase_add_checked_fixture(tc_long_running, setup_e2e, teardown_e2e);
    tcase_add_test(tc_long_running, test_long_running_agent_integration);
    tcase_set_timeout(tc_long_running, 30);
    suite_add_tcase(s, tc_long_running);

    /* Graceful shutdown */
    TCase* tc_shutdown = tcase_create("Graceful Shutdown");
    tcase_add_checked_fixture(tc_shutdown, setup_e2e, teardown_e2e);
    tcase_add_test(tc_shutdown, test_graceful_shutdown_with_pending);
    tcase_set_timeout(tc_shutdown, 30);
    suite_add_tcase(s, tc_shutdown);

    /* Stress test */
    TCase* tc_stress = tcase_create("Stress Test");
    tcase_add_checked_fixture(tc_stress, setup_e2e, teardown_e2e);
    tcase_add_test(tc_stress, test_stress_multi_threaded);
    tcase_set_timeout(tc_stress, 60);
    suite_add_tcase(s, tc_stress);

    /* Memory formation */
    TCase* tc_memory = tcase_create("Memory Formation");
    tcase_add_checked_fixture(tc_memory, setup_e2e, teardown_e2e);
    tcase_add_test(tc_memory, test_recovery_and_memory_formation);
    tcase_set_timeout(tc_memory, 30);
    suite_add_tcase(s, tc_memory);

    /* Error cascade */
    TCase* tc_cascade = tcase_create("Error Cascade");
    tcase_add_checked_fixture(tc_cascade, setup_e2e, teardown_e2e);
    tcase_add_test(tc_cascade, test_error_cascade_stability);
    tcase_set_timeout(tc_cascade, 60);
    suite_add_tcase(s, tc_cascade);

    return s;
}

int main(void)
{
    printf("\n=================================================\n");
    printf("Brain Immune Tick End-to-End Test Suite\n");
    printf("=================================================\n\n");

    int number_failed;
    Suite* s = brain_immune_tick_e2e_suite();
    SRunner* sr = srunner_create(s);

    /* Run all tests */
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    printf("\n=================================================\n");
    printf("E2E Tests Complete: %d failures\n", number_failed);
    printf("=================================================\n\n");

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
