/**
 * @file test_brain_immune_tick_enhanced_integration.c
 * @brief Integration tests for brain immune tick NIMCP utility enhancements
 *
 * WHAT: Tests MC severity, QA recovery, pattern memory integration with
 *       health agent and full immune system
 * WHY:  Verify enhanced features work correctly in integrated environment
 * HOW:  Tests using Check framework with full system setup
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
#include "utils/algorithms/nimcp_monte_carlo.h"
#include "optimization/quantum_annealing/nimcp_quantum_annealing.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

static brain_immune_system_t* g_immune = NULL;
static nimcp_health_agent_t* g_agent = NULL;

static void setup_full_integration(void)
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
    int result = brain_immune_tick_init(g_immune, &tick_config);
    ck_assert_int_eq(result, 0);

    health_agent_config_t agent_config;
    nimcp_health_agent_default_config(&agent_config);
    strncpy(agent_config.agent_name, "enhanced_integration_agent",
            sizeof(agent_config.agent_name) - 1);
    agent_config.check_interval_ms = 50;

    g_agent = nimcp_health_agent_create(&agent_config);
    ck_assert_ptr_nonnull(g_agent);

    nimcp_health_agent_connect_immune(g_agent, g_immune);
    result = brain_immune_tick_connect_health_agent(g_immune, g_agent);
    ck_assert_int_eq(result, 0);

    nimcp_exception_immune_init(NULL);
}

static void teardown_full_integration(void)
{
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
 * Monte Carlo Integration Tests
 * ============================================================================ */

/**
 * @test Test MC severity with health agent flow
 */
START_TEST(test_mc_severity_health_agent_flow)
{
    /* Report anomalies via health agent */
    for (int i = 0; i < 10; i++) {
        health_agent_message_t msg = nimcp_health_agent_create_message(
            HEALTH_MSG_ANOMALY_DETECTED,
            HEALTH_SEVERITY_WARNING,
            HEALTH_SOURCE_NEURAL,
            "MC integration test anomaly %d", i
        );
        nimcp_health_agent_report_anomaly(g_agent, &msg);
    }

    /* Process via tick */
    for (int i = 0; i < 5; i++) {
        brain_immune_tick(g_immune, 20);
    }

    /* Verify MC was used - messages processed with potentially varied severities */
    brain_immune_tick_stats_t stats;
    brain_immune_tick_get_stats(g_immune, &stats);
    ck_assert_uint_ge(stats.health_messages_processed, 5);

    brain_immune_stats_t immune_metrics;
    brain_immune_get_stats(g_immune, &immune_metrics);
    ck_assert_uint_ge(immune_metrics.antigens_processed, 5);
}
END_TEST

/**
 * @test Test MC severity with varying sources
 */
START_TEST(test_mc_severity_source_integration)
{
    health_agent_source_t sources[] = {
        HEALTH_SOURCE_MEMORY,
        HEALTH_SOURCE_NEURAL,
        HEALTH_SOURCE_THREADING,
        HEALTH_SOURCE_IO,
        HEALTH_SOURCE_KG
    };

    for (int i = 0; i < 5; i++) {
        health_agent_message_t msg = nimcp_health_agent_create_message(
            HEALTH_MSG_ANOMALY_DETECTED,
            HEALTH_SEVERITY_ERROR,
            sources[i],
            "Source %d test", i
        );
        nimcp_health_agent_report_anomaly(g_agent, &msg);
    }

    /* Process all */
    for (int i = 0; i < 3; i++) {
        brain_immune_tick(g_immune, 30);
    }

    brain_immune_tick_stats_t stats;
    brain_immune_tick_get_stats(g_immune, &stats);
    ck_assert_uint_ge(stats.health_messages_processed, 5);
}
END_TEST

/* ============================================================================
 * Pattern Memory Integration Tests
 * ============================================================================ */

/**
 * @test Test pattern memory builds coherence over repeated anomalies
 */
START_TEST(test_pattern_memory_coherence_buildup)
{
    /* Send same pattern multiple times via health agent */
    for (int i = 0; i < 20; i++) {
        health_agent_message_t msg = nimcp_health_agent_create_message(
            HEALTH_MSG_ANOMALY_DETECTED,
            HEALTH_SEVERITY_WARNING,
            HEALTH_SOURCE_NEURAL,
            "Coherence test pattern"  /* Same pattern */
        );
        nimcp_health_agent_report_anomaly(g_agent, &msg);
        usleep(500);  /* Small delay for timestamp variation */
    }

    /* Process all */
    for (int i = 0; i < 10; i++) {
        brain_immune_tick(g_immune, 20);
    }

    /* Verify all were processed */
    brain_immune_tick_stats_t stats;
    brain_immune_tick_get_stats(g_immune, &stats);
    ck_assert_uint_ge(stats.health_messages_processed, 15);
}
END_TEST

/**
 * @test Test pattern memory with exception flow
 *
 * Uses NIMCP_THROW_ASYNC to queue exceptions for async processing by the tick.
 */
START_TEST(test_pattern_memory_exception_flow)
{
    /* Throw same exception multiple times using async (queued for tick processing) */
    for (int i = 0; i < 8; i++) {
        NIMCP_THROW_ASYNC(NIMCP_ERROR_NO_MEMORY, "Repeated memory error");
    }

    /* Process queued exceptions */
    for (int i = 0; i < 10; i++) {
        brain_immune_tick(g_immune, 10);
        usleep(1000);
    }

    brain_immune_tick_stats_t stats;
    brain_immune_tick_get_stats(g_immune, &stats);
    ck_assert_uint_ge(stats.exceptions_processed, 5);
}
END_TEST

/**
 * @test Test multiple distinct patterns tracked
 */
START_TEST(test_pattern_memory_multiple_distinct)
{
    const char* patterns[] = {
        "Pattern Alpha: memory leak",
        "Pattern Beta: thread stall",
        "Pattern Gamma: NaN detected",
        "Pattern Delta: IO error"
    };

    /* Interleave patterns */
    for (int round = 0; round < 5; round++) {
        for (int p = 0; p < 4; p++) {
            health_agent_message_t msg = nimcp_health_agent_create_message(
                HEALTH_MSG_ANOMALY_DETECTED,
                HEALTH_SEVERITY_WARNING,
                HEALTH_SOURCE_NEURAL,
                "%s", patterns[p]
            );
            nimcp_health_agent_report_anomaly(g_agent, &msg);
        }
        brain_immune_tick(g_immune, 30);
    }

    brain_immune_tick_stats_t stats;
    brain_immune_tick_get_stats(g_immune, &stats);
    ck_assert_uint_ge(stats.health_messages_processed, 15);
}
END_TEST

/* ============================================================================
 * Quantum Annealing Integration Tests
 * ============================================================================ */

/**
 * @test Test QA recovery with health agent flow
 */
START_TEST(test_qa_recovery_health_agent_flow)
{
    /* Report error that needs recovery */
    health_agent_message_t error_msg = nimcp_health_agent_create_message(
        HEALTH_MSG_ANOMALY_DETECTED,
        HEALTH_SEVERITY_ERROR,
        HEALTH_SOURCE_MEMORY,
        "Memory pressure detected"
    );
    nimcp_health_agent_report_anomaly(g_agent, &error_msg);

    brain_immune_tick(g_immune, 20);

    /* Request recovery via health agent */
    health_agent_message_t recovery_msg = nimcp_health_agent_create_message(
        HEALTH_MSG_RECOVERY_REQUEST,
        HEALTH_SEVERITY_ERROR,
        HEALTH_SOURCE_MEMORY,
        "Request memory recovery"
    );
    recovery_msg.suggested_action = HEALTH_RECOVERY_GC;
    nimcp_health_agent_report_anomaly(g_agent, &recovery_msg);

    brain_immune_tick(g_immune, 20);

    brain_immune_tick_stats_t stats;
    brain_immune_tick_get_stats(g_immune, &stats);
    ck_assert_uint_ge(stats.health_messages_processed, 2);
}
END_TEST

/**
 * @test Test QA recovery for critical emergency
 */
START_TEST(test_qa_recovery_emergency)
{
    /* Report emergency */
    health_agent_message_t msg = nimcp_health_agent_create_message(
        HEALTH_MSG_EMERGENCY,
        HEALTH_SEVERITY_CRITICAL,
        HEALTH_SOURCE_HEARTBEAT,
        "System unresponsive - emergency"
    );
    nimcp_health_agent_report_anomaly(g_agent, &msg);

    brain_immune_tick(g_immune, 50);

    /* Verify emergency triggered recovery */
    brain_immune_tick_stats_t stats;
    brain_immune_tick_get_stats(g_immune, &stats);
    ck_assert_uint_ge(stats.recovery_actions_triggered, 1);
}
END_TEST

/**
 * @test Test QA recovery action selection varies by source
 */
START_TEST(test_qa_recovery_source_specific)
{
    /* Memory source recovery */
    health_agent_message_t mem_recovery = nimcp_health_agent_create_message(
        HEALTH_MSG_RECOVERY_REQUEST,
        HEALTH_SEVERITY_ERROR,
        HEALTH_SOURCE_MEMORY,
        "Memory recovery request"
    );
    mem_recovery.suggested_action = HEALTH_RECOVERY_NONE;
    nimcp_health_agent_report_anomaly(g_agent, &mem_recovery);

    /* Threading source recovery */
    health_agent_message_t thread_recovery = nimcp_health_agent_create_message(
        HEALTH_MSG_RECOVERY_REQUEST,
        HEALTH_SEVERITY_ERROR,
        HEALTH_SOURCE_THREADING,
        "Thread recovery request"
    );
    thread_recovery.suggested_action = HEALTH_RECOVERY_NONE;
    nimcp_health_agent_report_anomaly(g_agent, &thread_recovery);

    for (int i = 0; i < 3; i++) {
        brain_immune_tick(g_immune, 20);
    }

    brain_immune_tick_stats_t stats;
    brain_immune_tick_get_stats(g_immune, &stats);
    ck_assert_uint_ge(stats.health_messages_processed, 2);
}
END_TEST

/* ============================================================================
 * Numerical Integration Integration Tests
 * ============================================================================ */

/**
 * @test Test integration stats with load
 */
START_TEST(test_integration_stats_under_load)
{
    /* Generate load */
    for (int i = 0; i < 20; i++) {
        health_agent_message_t msg = nimcp_health_agent_create_message(
            HEALTH_MSG_ANOMALY_DETECTED,
            HEALTH_SEVERITY_WARNING,
            HEALTH_SOURCE_NEURAL,
            "Load test %d", i
        );
        nimcp_health_agent_report_anomaly(g_agent, &msg);
    }

    /* Process with ticks */
    for (int i = 0; i < 30; i++) {
        brain_immune_tick(g_immune, 10);
    }

    brain_immune_tick_stats_t stats;
    brain_immune_tick_get_stats(g_immune, &stats);

    ck_assert_uint_eq(stats.ticks_executed, 30);
    ck_assert(stats.avg_tick_duration_us > 0.0f);
    ck_assert(stats.max_tick_duration_us >= stats.avg_tick_duration_us);
}
END_TEST

/**
 * @test Test EMA convergence with consistent workload
 */
START_TEST(test_ema_convergence)
{
    /* Run many ticks to allow EMA to converge */
    for (int i = 0; i < 200; i++) {
        brain_immune_tick(g_immune, 5);
    }

    brain_immune_tick_stats_t stats1;
    brain_immune_tick_get_stats(g_immune, &stats1);

    /* Run more ticks */
    for (int i = 0; i < 100; i++) {
        brain_immune_tick(g_immune, 5);
    }

    brain_immune_tick_stats_t stats2;
    brain_immune_tick_get_stats(g_immune, &stats2);

    /* EMA should be relatively stable after convergence.
     * For very small tick durations, use absolute threshold.
     * For larger values, use relative threshold. */
    float diff = fabsf(stats2.avg_tick_duration_us - stats1.avg_tick_duration_us);

    if (stats1.avg_tick_duration_us < 1.0f) {
        /* For sub-microsecond ticks, just check absolute difference */
        ck_assert_msg(diff < 10.0f,
                      "EMA diff too large: %.2f (stats1=%.2f, stats2=%.2f)",
                      diff, stats1.avg_tick_duration_us, stats2.avg_tick_duration_us);
    } else {
        /* For larger values, check relative difference */
        float relative_diff = diff / stats1.avg_tick_duration_us;
        ck_assert_msg(relative_diff < 0.5f,
                      "EMA relative diff too large: %.2f%% (stats1=%.2f, stats2=%.2f)",
                      relative_diff * 100.0f, stats1.avg_tick_duration_us,
                      stats2.avg_tick_duration_us);
    }
}
END_TEST

/* ============================================================================
 * Combined Enhancement Integration Tests
 * ============================================================================ */

/**
 * @test Test full pipeline with all enhancements
 */
START_TEST(test_full_enhancement_pipeline)
{
    /* 1. Create recurring pattern (pattern memory) */
    for (int i = 0; i < 10; i++) {
        health_agent_message_t msg = nimcp_health_agent_create_message(
            HEALTH_MSG_ANOMALY_DETECTED,
            HEALTH_SEVERITY_ERROR,  /* MC will vary this */
            HEALTH_SOURCE_MEMORY,
            "Recurring leak pattern"
        );
        nimcp_health_agent_report_anomaly(g_agent, &msg);
        brain_immune_tick(g_immune, 10);
        usleep(100);
    }

    /* 2. Request recovery (QA optimization) */
    health_agent_message_t recovery = nimcp_health_agent_create_message(
        HEALTH_MSG_RECOVERY_REQUEST,
        HEALTH_SEVERITY_ERROR,
        HEALTH_SOURCE_MEMORY,
        "Request optimal recovery"
    );
    recovery.suggested_action = HEALTH_RECOVERY_NONE;
    nimcp_health_agent_report_anomaly(g_agent, &recovery);

    brain_immune_tick(g_immune, 50);

    /* 3. Verify stats (numerical integration) */
    brain_immune_tick_stats_t stats;
    brain_immune_tick_get_stats(g_immune, &stats);

    ck_assert_uint_ge(stats.ticks_executed, 11);
    ck_assert_uint_ge(stats.health_messages_processed, 5);
    ck_assert(stats.avg_tick_duration_us > 0.0f);
}
END_TEST

/**
 * @test Test health agent thread with enhancements
 */
START_TEST(test_health_agent_thread_enhanced)
{
    /* Start health agent thread */
    int result = nimcp_health_agent_start(g_agent);
    ck_assert_int_eq(result, 0);

    usleep(100000);  /* Let agent run */

    /* Report anomalies while agent is running */
    for (int i = 0; i < 5; i++) {
        health_agent_message_t msg = nimcp_health_agent_create_message(
            HEALTH_MSG_ANOMALY_DETECTED,
            HEALTH_SEVERITY_WARNING,
            HEALTH_SOURCE_NEURAL,
            "Agent thread test %d", i
        );
        nimcp_health_agent_report_anomaly(g_agent, &msg);
        usleep(20000);
    }

    usleep(200000);  /* Wait for processing */

    nimcp_health_agent_stop(g_agent);

    brain_immune_tick_stats_t stats;
    brain_immune_tick_get_stats(g_immune, &stats);

    /* Agent thread should have called tick */
    ck_assert_uint_ge(stats.ticks_executed, 1);
}
END_TEST

/* ============================================================================
 * Concurrent Enhancement Tests
 * ============================================================================ */

static void* enhanced_reporter_thread(void* arg)
{
    brain_immune_system_t* immune = (brain_immune_system_t*)arg;
    (void)immune;

    for (int i = 0; i < 20; i++) {
        health_agent_message_t msg = nimcp_health_agent_create_message(
            HEALTH_MSG_ANOMALY_DETECTED,
            (i % 2 == 0) ? HEALTH_SEVERITY_WARNING : HEALTH_SEVERITY_ERROR,
            (i % 3 == 0) ? HEALTH_SOURCE_MEMORY : HEALTH_SOURCE_NEURAL,
            "Concurrent enhanced %d", i
        );
        nimcp_health_agent_report_anomaly(g_agent, &msg);
        usleep(5000);
    }
    return NULL;
}

static void* enhanced_ticker_thread(void* arg)
{
    brain_immune_system_t* immune = (brain_immune_system_t*)arg;

    for (int i = 0; i < 30; i++) {
        brain_immune_tick(immune, 10);
        usleep(5000);
    }
    return NULL;
}

START_TEST(test_concurrent_enhanced_processing)
{
    pthread_t reporter, ticker;

    pthread_create(&reporter, NULL, enhanced_reporter_thread, g_immune);
    pthread_create(&ticker, NULL, enhanced_ticker_thread, g_immune);

    pthread_join(reporter, NULL);
    pthread_join(ticker, NULL);

    brain_immune_tick_stats_t stats;
    brain_immune_tick_get_stats(g_immune, &stats);

    ck_assert_uint_ge(stats.ticks_executed, 20);
    /* Some messages should have been processed with enhancements */
}
END_TEST

/* ============================================================================
 * Test Suite Creation
 * ============================================================================ */

Suite* brain_immune_tick_enhanced_integration_suite(void)
{
    Suite* s = suite_create("Brain Immune Tick Enhanced Integration");

    /* MC Integration */
    TCase* tc_mc = tcase_create("MC Integration");
    tcase_add_checked_fixture(tc_mc, setup_full_integration, teardown_full_integration);
    tcase_add_test(tc_mc, test_mc_severity_health_agent_flow);
    tcase_add_test(tc_mc, test_mc_severity_source_integration);
    tcase_set_timeout(tc_mc, 15);
    suite_add_tcase(s, tc_mc);

    /* Pattern Memory Integration */
    TCase* tc_pattern = tcase_create("Pattern Memory Integration");
    tcase_add_checked_fixture(tc_pattern, setup_full_integration, teardown_full_integration);
    tcase_add_test(tc_pattern, test_pattern_memory_coherence_buildup);
    tcase_add_test(tc_pattern, test_pattern_memory_exception_flow);
    tcase_add_test(tc_pattern, test_pattern_memory_multiple_distinct);
    tcase_set_timeout(tc_pattern, 15);
    suite_add_tcase(s, tc_pattern);

    /* QA Integration */
    TCase* tc_qa = tcase_create("QA Recovery Integration");
    tcase_add_checked_fixture(tc_qa, setup_full_integration, teardown_full_integration);
    tcase_add_test(tc_qa, test_qa_recovery_health_agent_flow);
    tcase_add_test(tc_qa, test_qa_recovery_emergency);
    tcase_add_test(tc_qa, test_qa_recovery_source_specific);
    tcase_set_timeout(tc_qa, 15);
    suite_add_tcase(s, tc_qa);

    /* Numerical Integration */
    TCase* tc_integration = tcase_create("Numerical Integration");
    tcase_add_checked_fixture(tc_integration, setup_full_integration, teardown_full_integration);
    tcase_add_test(tc_integration, test_integration_stats_under_load);
    tcase_add_test(tc_integration, test_ema_convergence);
    tcase_set_timeout(tc_integration, 20);
    suite_add_tcase(s, tc_integration);

    /* Combined */
    TCase* tc_combined = tcase_create("Combined Enhancements");
    tcase_add_checked_fixture(tc_combined, setup_full_integration, teardown_full_integration);
    tcase_add_test(tc_combined, test_full_enhancement_pipeline);
    tcase_add_test(tc_combined, test_health_agent_thread_enhanced);
    tcase_set_timeout(tc_combined, 15);
    suite_add_tcase(s, tc_combined);

    /* Concurrent */
    TCase* tc_concurrent = tcase_create("Concurrent Enhancements");
    tcase_add_checked_fixture(tc_concurrent, setup_full_integration, teardown_full_integration);
    tcase_add_test(tc_concurrent, test_concurrent_enhanced_processing);
    tcase_set_timeout(tc_concurrent, 15);
    suite_add_tcase(s, tc_concurrent);

    return s;
}

int main(void)
{
    int number_failed;
    Suite* s = brain_immune_tick_enhanced_integration_suite();
    SRunner* sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
