/**
 * @file test_brain_immune_tick_enhanced_e2e.c
 * @brief End-to-end tests for brain immune tick NIMCP utility enhancements
 *
 * WHAT: Full system tests for MC severity, QA recovery, pattern memory,
 *       and numerical integration under realistic workloads
 * WHY:  Verify enhancements work correctly in production-like scenarios
 * HOW:  Tests using Check framework with realistic workloads and stress tests
 *
 * @author NIMCP Development Team
 * @date 2025-01-25
 */

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

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
    nimcp_exception_system_init();

    brain_immune_config_t immune_config;
    brain_immune_default_config(&immune_config);
    g_immune = brain_immune_create(&immune_config);
    ck_assert_ptr_nonnull(g_immune);

    /* Start the immune system so brain_immune_update() will work */
    int start_result = brain_immune_start(g_immune);
    ck_assert_int_eq(start_result, 0);

    brain_immune_tick_config_t tick_config;
    brain_immune_tick_default_config(&tick_config);
    tick_config.enable_tick_logging = false;
    tick_config.max_health_msgs_per_tick = 50;
    tick_config.max_exceptions_per_tick = 20;
    int result = brain_immune_tick_init(g_immune, &tick_config);
    ck_assert_int_eq(result, 0);

    health_agent_config_t agent_config;
    nimcp_health_agent_default_config(&agent_config);
    strncpy(agent_config.agent_name, "e2e_enhanced_agent", sizeof(agent_config.agent_name) - 1);
    agent_config.check_interval_ms = 25;

    g_agent = nimcp_health_agent_create(&agent_config);
    ck_assert_ptr_nonnull(g_agent);

    nimcp_health_agent_connect_immune(g_agent, g_immune);
    brain_immune_tick_connect_health_agent(g_immune, g_agent);

    nimcp_exception_immune_init(NULL);
}

static void teardown_e2e(void)
{
    if (g_agent) {
        nimcp_health_agent_stop(g_agent);
    }

    nimcp_exception_immune_shutdown();

    if (g_agent) {
        nimcp_health_agent_destroy(g_agent);
        g_agent = NULL;
    }

    if (g_immune) {
        brain_immune_stop(g_immune);
        brain_immune_tick_shutdown(g_immune);
        brain_immune_destroy(g_immune);
        g_immune = NULL;
    }

    nimcp_exception_system_shutdown();
}

/* ============================================================================
 * High-Volume Monte Carlo E2E Tests
 * ============================================================================ */

/**
 * @test E2E: High-volume anomaly processing with MC severity
 *
 * Simulates production workload with many anomalies.
 */
START_TEST(test_e2e_high_volume_mc_severity)
{
    /* Generate high volume of anomalies */
    for (int batch = 0; batch < 10; batch++) {
        for (int i = 0; i < 100; i++) {
            health_agent_severity_t sev = (i % 5 == 0) ? HEALTH_SEVERITY_ERROR
                                        : (i % 3 == 0) ? HEALTH_SEVERITY_WARNING
                                        : HEALTH_SEVERITY_INFO;

            health_agent_message_t msg = nimcp_health_agent_create_message(
                HEALTH_MSG_ANOMALY_DETECTED,
                sev,
                (health_agent_source_t)(i % 6),
                "E2E anomaly batch %d item %d", batch, i
            );
            nimcp_health_agent_report_anomaly(g_agent, &msg);
        }

        /* Process batch */
        for (int t = 0; t < 20; t++) {
            brain_immune_tick(g_immune, 10);
        }
    }

    brain_immune_tick_stats_t stats;
    brain_immune_tick_get_stats(g_immune, &stats);

    /* Should process majority of messages */
    ck_assert_uint_ge(stats.health_messages_processed, 500);
    ck_assert_uint_ge(stats.health_antigens_created, 500);

    /* MC should have varied severities - can't directly test but no crashes */
    ck_assert(!isnan(stats.avg_tick_duration_us));
}
END_TEST

/**
 * @test E2E: MC severity distribution over long run
 */
START_TEST(test_e2e_mc_severity_distribution)
{
    /* Run for extended period with consistent severity input */
    for (int i = 0; i < 500; i++) {
        health_agent_message_t msg = nimcp_health_agent_create_message(
            HEALTH_MSG_ANOMALY_DETECTED,
            HEALTH_SEVERITY_WARNING,  /* Consistent input */
            HEALTH_SOURCE_NEURAL,
            "Distribution test %d", i
        );
        nimcp_health_agent_report_anomaly(g_agent, &msg);

        if (i % 50 == 0) {
            brain_immune_tick(g_immune, 30);
        }
    }

    /* Final processing */
    for (int t = 0; t < 50; t++) {
        brain_immune_tick(g_immune, 10);
    }

    brain_immune_tick_stats_t stats;
    brain_immune_tick_get_stats(g_immune, &stats);

    /* All should be processed */
    ck_assert_uint_ge(stats.health_messages_processed, 400);
}
END_TEST

/* ============================================================================
 * Pattern Memory E2E Tests
 * ============================================================================ */

/**
 * @test E2E: Recurring anomaly detection in production scenario
 *
 * Simulates real pattern where same error occurs repeatedly.
 */
START_TEST(test_e2e_recurring_anomaly_detection)
{
    /* Simulate recurring memory leak pattern */
    const char* leak_pattern = "Memory leak in module X: 1024 bytes lost";

    for (int cycle = 0; cycle < 20; cycle++) {
        /* Recurring pattern */
        for (int repeat = 0; repeat < 5; repeat++) {
            health_agent_message_t msg = nimcp_health_agent_create_message(
                HEALTH_MSG_ANOMALY_DETECTED,
                HEALTH_SEVERITY_WARNING,
                HEALTH_SOURCE_MEMORY,
                "%s", leak_pattern
            );
            nimcp_health_agent_report_anomaly(g_agent, &msg);
        }

        /* Some noise */
        health_agent_message_t noise = nimcp_health_agent_create_message(
            HEALTH_MSG_ANOMALY_DETECTED,
            HEALTH_SEVERITY_INFO,
            HEALTH_SOURCE_IO,
            "Random noise %d", cycle
        );
        nimcp_health_agent_report_anomaly(g_agent, &noise);

        brain_immune_tick(g_immune, 30);
        usleep(10000);  /* Simulate real timing */
    }

    brain_immune_tick_stats_t stats;
    brain_immune_tick_get_stats(g_immune, &stats);

    /* Pattern should be recognized and potentially boosted */
    ck_assert_uint_ge(stats.health_messages_processed, 80);
}
END_TEST

/**
 * @test E2E: Multiple concurrent patterns
 */
START_TEST(test_e2e_concurrent_patterns)
{
    const char* patterns[] = {
        "NaN detected in forward pass layer 3",
        "Thread deadlock between worker 1 and 2",
        "Cache miss rate exceeded threshold",
        "Network latency spike detected"
    };

    /* Interleave patterns */
    for (int cycle = 0; cycle < 30; cycle++) {
        for (int p = 0; p < 4; p++) {
            health_agent_message_t msg = nimcp_health_agent_create_message(
                HEALTH_MSG_ANOMALY_DETECTED,
                (p == 1) ? HEALTH_SEVERITY_CRITICAL : HEALTH_SEVERITY_WARNING,
                (health_agent_source_t)(p % 4),
                "%s", patterns[p]
            );
            nimcp_health_agent_report_anomaly(g_agent, &msg);
        }
        brain_immune_tick(g_immune, 20);
    }

    brain_immune_tick_stats_t stats;
    brain_immune_tick_get_stats(g_immune, &stats);

    ck_assert_uint_ge(stats.health_messages_processed, 100);
}
END_TEST

/* ============================================================================
 * Quantum Annealing E2E Tests
 * ============================================================================ */

/**
 * @test E2E: Recovery optimization under crisis
 *
 * Simulates system crisis requiring optimal recovery selection.
 */
START_TEST(test_e2e_qa_crisis_recovery)
{
    /* Build up crisis */
    for (int i = 0; i < 20; i++) {
        health_agent_message_t msg = nimcp_health_agent_create_message(
            HEALTH_MSG_ANOMALY_DETECTED,
            (i % 3 == 0) ? HEALTH_SEVERITY_CRITICAL : HEALTH_SEVERITY_ERROR,
            HEALTH_SOURCE_MEMORY,
            "Crisis buildup %d", i
        );
        nimcp_health_agent_report_anomaly(g_agent, &msg);
    }

    brain_immune_tick(g_immune, 50);

    /* Request recovery - QA should optimize */
    health_agent_message_t recovery = nimcp_health_agent_create_message(
        HEALTH_MSG_RECOVERY_REQUEST,
        HEALTH_SEVERITY_CRITICAL,
        HEALTH_SOURCE_MEMORY,
        "Request optimal crisis recovery"
    );
    recovery.suggested_action = HEALTH_RECOVERY_NONE;  /* Let QA decide */
    nimcp_health_agent_report_anomaly(g_agent, &recovery);

    brain_immune_tick(g_immune, 100);

    brain_immune_tick_stats_t stats;
    brain_immune_tick_get_stats(g_immune, &stats);

    /* Recovery should be triggered */
    ck_assert_uint_ge(stats.recovery_actions_triggered, 1);
}
END_TEST

/**
 * @test E2E: Multiple recovery requests with different sources
 */
START_TEST(test_e2e_qa_multi_source_recovery)
{
    struct {
        health_agent_source_t source;
        const char* description;
    } recoveries[] = {
        { HEALTH_SOURCE_MEMORY, "Memory pressure recovery" },
        { HEALTH_SOURCE_THREADING, "Thread contention recovery" },
        { HEALTH_SOURCE_NEURAL, "Neural network recovery" },
        { HEALTH_SOURCE_IO, "IO bottleneck recovery" }
    };

    for (int i = 0; i < 4; i++) {
        /* Create context */
        health_agent_message_t anomaly = nimcp_health_agent_create_message(
            HEALTH_MSG_ANOMALY_DETECTED,
            HEALTH_SEVERITY_ERROR,
            recoveries[i].source,
            "%s context", recoveries[i].description
        );
        nimcp_health_agent_report_anomaly(g_agent, &anomaly);

        brain_immune_tick(g_immune, 20);

        /* Request recovery */
        health_agent_message_t recovery = nimcp_health_agent_create_message(
            HEALTH_MSG_RECOVERY_REQUEST,
            HEALTH_SEVERITY_ERROR,
            recoveries[i].source,
            "%s", recoveries[i].description
        );
        recovery.suggested_action = HEALTH_RECOVERY_NONE;
        nimcp_health_agent_report_anomaly(g_agent, &recovery);

        brain_immune_tick(g_immune, 30);
    }

    brain_immune_tick_stats_t stats;
    brain_immune_tick_get_stats(g_immune, &stats);

    ck_assert_uint_ge(stats.health_messages_processed, 8);
}
END_TEST

/* ============================================================================
 * Numerical Integration E2E Tests
 * ============================================================================ */

/**
 * @test E2E: Statistics stability under varying load
 */
START_TEST(test_e2e_stats_stability_varying_load)
{
    /* Phase 1: Light load */
    for (int i = 0; i < 100; i++) {
        brain_immune_tick(g_immune, 5);
    }

    brain_immune_tick_stats_t stats_light;
    brain_immune_tick_get_stats(g_immune, &stats_light);

    /* Phase 2: Heavy load */
    for (int i = 0; i < 200; i++) {
        health_agent_message_t msg = nimcp_health_agent_create_message(
            HEALTH_MSG_ANOMALY_DETECTED,
            HEALTH_SEVERITY_WARNING,
            HEALTH_SOURCE_NEURAL,
            "Heavy load %d", i
        );
        nimcp_health_agent_report_anomaly(g_agent, &msg);
        brain_immune_tick(g_immune, 10);
    }

    brain_immune_tick_stats_t stats_heavy;
    brain_immune_tick_get_stats(g_immune, &stats_heavy);

    /* Phase 3: Return to light */
    for (int i = 0; i < 100; i++) {
        brain_immune_tick(g_immune, 5);
    }

    brain_immune_tick_stats_t stats_final;
    brain_immune_tick_get_stats(g_immune, &stats_final);

    /* EMA should track changes */
    ck_assert(stats_final.avg_tick_duration_us > 0.0f);
    ck_assert(!isnan(stats_final.avg_tick_duration_us));
}
END_TEST

/**
 * @test E2E: Long-running statistics accumulation
 */
START_TEST(test_e2e_long_running_stats)
{
    /* Simulate extended operation */
    for (int hour = 0; hour < 5; hour++) {  /* Simulated hours */
        for (int minute = 0; minute < 60; minute++) {  /* Simulated minutes */
            /* Background ticks */
            for (int sec = 0; sec < 10; sec++) {
                brain_immune_tick(g_immune, 10);
            }

            /* Occasional anomaly */
            if (minute % 10 == 0) {
                health_agent_message_t msg = nimcp_health_agent_create_message(
                    HEALTH_MSG_ANOMALY_DETECTED,
                    HEALTH_SEVERITY_WARNING,
                    HEALTH_SOURCE_NEURAL,
                    "Periodic check hour %d minute %d", hour, minute
                );
                nimcp_health_agent_report_anomaly(g_agent, &msg);
            }
        }
    }

    brain_immune_tick_stats_t stats;
    brain_immune_tick_get_stats(g_immune, &stats);

    /* Should have many ticks without overflow */
    ck_assert_uint_ge(stats.ticks_executed, 2000);
    ck_assert(!isnan(stats.avg_tick_duration_us));
    ck_assert(!isinf(stats.avg_tick_duration_us));
}
END_TEST

/* ============================================================================
 * Concurrent Stress E2E Tests
 * ============================================================================ */

static volatile int g_stress_running = 1;

static void* stress_reporter_thread(void* arg)
{
    brain_immune_system_t* immune = (brain_immune_system_t*)arg;
    (void)immune;

    uint32_t count = 0;
    while (g_stress_running) {
        health_agent_message_t msg = nimcp_health_agent_create_message(
            HEALTH_MSG_ANOMALY_DETECTED,
            (count % 4 == 0) ? HEALTH_SEVERITY_ERROR : HEALTH_SEVERITY_WARNING,
            (health_agent_source_t)(count % 6),
            "Stress test %u", count
        );
        nimcp_health_agent_report_anomaly(g_agent, &msg);
        count++;
        usleep(1000);  /* 1ms between reports */
    }
    return NULL;
}

static void* stress_ticker_thread(void* arg)
{
    brain_immune_system_t* immune = (brain_immune_system_t*)arg;

    while (g_stress_running) {
        brain_immune_tick(immune, 5);
        usleep(2000);  /* 2ms between ticks */
    }
    return NULL;
}

static void* stress_recovery_thread(void* arg)
{
    brain_immune_system_t* immune = (brain_immune_system_t*)arg;
    (void)immune;

    uint32_t count = 0;
    while (g_stress_running) {
        health_agent_message_t msg = nimcp_health_agent_create_message(
            HEALTH_MSG_RECOVERY_REQUEST,
            HEALTH_SEVERITY_ERROR,
            (health_agent_source_t)(count % 4),
            "Stress recovery %u", count
        );
        msg.suggested_action = HEALTH_RECOVERY_NONE;
        nimcp_health_agent_report_anomaly(g_agent, &msg);
        count++;
        usleep(50000);  /* 50ms between recovery requests */
    }
    return NULL;
}

START_TEST(test_e2e_concurrent_stress)
{
    pthread_t reporter, ticker, recovery;

    g_stress_running = 1;

    pthread_create(&reporter, NULL, stress_reporter_thread, g_immune);
    pthread_create(&ticker, NULL, stress_ticker_thread, g_immune);
    pthread_create(&recovery, NULL, stress_recovery_thread, g_immune);

    /* Run for 2 seconds */
    usleep(2000000);

    g_stress_running = 0;

    pthread_join(reporter, NULL);
    pthread_join(ticker, NULL);
    pthread_join(recovery, NULL);

    brain_immune_tick_stats_t stats;
    brain_immune_tick_get_stats(g_immune, &stats);

    /* Should have processed significant load without crash */
    ck_assert_uint_ge(stats.ticks_executed, 500);
    ck_assert_uint_ge(stats.health_messages_processed, 100);
    ck_assert(!isnan(stats.avg_tick_duration_us));
}
END_TEST

/**
 * @test E2E: Agent thread with enhanced processing
 */
START_TEST(test_e2e_agent_thread_stress)
{
    /* Start health agent thread */
    int result = nimcp_health_agent_start(g_agent);
    ck_assert_int_eq(result, 0);

    /* Generate load while agent runs */
    for (int i = 0; i < 50; i++) {
        health_agent_message_t msg = nimcp_health_agent_create_message(
            HEALTH_MSG_ANOMALY_DETECTED,
            HEALTH_SEVERITY_WARNING,
            HEALTH_SOURCE_NEURAL,
            "Agent thread stress %d", i
        );
        nimcp_health_agent_report_anomaly(g_agent, &msg);
        usleep(20000);  /* 20ms between */
    }

    usleep(500000);  /* Let agent process */

    nimcp_health_agent_stop(g_agent);

    brain_immune_tick_stats_t stats;
    brain_immune_tick_get_stats(g_immune, &stats);

    /* Agent should have run ticks */
    ck_assert_uint_ge(stats.ticks_executed, 10);
}
END_TEST

/* ============================================================================
 * Full System E2E Tests
 * ============================================================================ */

/**
 * @test E2E: Full production simulation
 *
 * Simulates realistic production workload over extended period.
 */
START_TEST(test_e2e_production_simulation)
{
    /* Simulate 5 "hours" of operation (compressed) */
    for (int hour = 0; hour < 5; hour++) {
        /* Morning ramp up */
        for (int i = 0; i < 20; i++) {
            health_agent_message_t msg = nimcp_health_agent_create_message(
                HEALTH_MSG_ANOMALY_DETECTED,
                HEALTH_SEVERITY_INFO,
                HEALTH_SOURCE_NEURAL,
                "Warmup hour %d", hour
            );
            nimcp_health_agent_report_anomaly(g_agent, &msg);
            brain_immune_tick(g_immune, 10);
        }

        /* Peak load period */
        for (int i = 0; i < 50; i++) {
            health_agent_message_t msg = nimcp_health_agent_create_message(
                HEALTH_MSG_ANOMALY_DETECTED,
                (i % 10 == 0) ? HEALTH_SEVERITY_ERROR : HEALTH_SEVERITY_WARNING,
                (health_agent_source_t)(i % 5),
                "Peak load hour %d item %d", hour, i
            );
            nimcp_health_agent_report_anomaly(g_agent, &msg);
            brain_immune_tick(g_immune, 10);
        }

        /* Occasional recovery */
        if (hour % 2 == 0) {
            health_agent_message_t recovery = nimcp_health_agent_create_message(
                HEALTH_MSG_RECOVERY_REQUEST,
                HEALTH_SEVERITY_WARNING,
                HEALTH_SOURCE_MEMORY,
                "Scheduled GC hour %d", hour
            );
            recovery.suggested_action = HEALTH_RECOVERY_GC;
            nimcp_health_agent_report_anomaly(g_agent, &recovery);
            brain_immune_tick(g_immune, 50);
        }

        /* Quiet period */
        for (int i = 0; i < 10; i++) {
            brain_immune_tick(g_immune, 10);
        }
    }

    brain_immune_tick_stats_t stats;
    brain_immune_tick_get_stats(g_immune, &stats);

    ck_assert_uint_ge(stats.ticks_executed, 300);
    ck_assert_uint_ge(stats.health_messages_processed, 200);
    ck_assert(!isnan(stats.avg_tick_duration_us));
}
END_TEST

/* ============================================================================
 * Test Suite Creation
 * ============================================================================ */

Suite* brain_immune_tick_enhanced_e2e_suite(void)
{
    Suite* s = suite_create("Brain Immune Tick Enhanced E2E");

    /* MC E2E */
    TCase* tc_mc = tcase_create("MC E2E");
    tcase_add_checked_fixture(tc_mc, setup_e2e, teardown_e2e);
    tcase_add_test(tc_mc, test_e2e_high_volume_mc_severity);
    tcase_add_test(tc_mc, test_e2e_mc_severity_distribution);
    tcase_set_timeout(tc_mc, 30);
    suite_add_tcase(s, tc_mc);

    /* Pattern Memory E2E */
    TCase* tc_pattern = tcase_create("Pattern Memory E2E");
    tcase_add_checked_fixture(tc_pattern, setup_e2e, teardown_e2e);
    tcase_add_test(tc_pattern, test_e2e_recurring_anomaly_detection);
    tcase_add_test(tc_pattern, test_e2e_concurrent_patterns);
    tcase_set_timeout(tc_pattern, 30);
    suite_add_tcase(s, tc_pattern);

    /* QA E2E */
    TCase* tc_qa = tcase_create("QA E2E");
    tcase_add_checked_fixture(tc_qa, setup_e2e, teardown_e2e);
    tcase_add_test(tc_qa, test_e2e_qa_crisis_recovery);
    tcase_add_test(tc_qa, test_e2e_qa_multi_source_recovery);
    tcase_set_timeout(tc_qa, 30);
    suite_add_tcase(s, tc_qa);

    /* Integration E2E */
    TCase* tc_integration = tcase_create("Integration E2E");
    tcase_add_checked_fixture(tc_integration, setup_e2e, teardown_e2e);
    tcase_add_test(tc_integration, test_e2e_stats_stability_varying_load);
    tcase_add_test(tc_integration, test_e2e_long_running_stats);
    tcase_set_timeout(tc_integration, 60);
    suite_add_tcase(s, tc_integration);

    /* Stress E2E */
    TCase* tc_stress = tcase_create("Concurrent Stress E2E");
    tcase_add_checked_fixture(tc_stress, setup_e2e, teardown_e2e);
    tcase_add_test(tc_stress, test_e2e_concurrent_stress);
    tcase_add_test(tc_stress, test_e2e_agent_thread_stress);
    tcase_set_timeout(tc_stress, 30);
    suite_add_tcase(s, tc_stress);

    /* Full System E2E */
    TCase* tc_full = tcase_create("Full System E2E");
    tcase_add_checked_fixture(tc_full, setup_e2e, teardown_e2e);
    tcase_add_test(tc_full, test_e2e_production_simulation);
    tcase_set_timeout(tc_full, 60);
    suite_add_tcase(s, tc_full);

    return s;
}

int main(void)
{
    int number_failed;
    Suite* s = brain_immune_tick_enhanced_e2e_suite();
    SRunner* sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
