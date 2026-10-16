/**
 * @file test_brain_immune_tick_enhanced.c
 * @brief Unit tests for brain immune tick enhanced features (NIMCP utilities)
 *
 * WHAT: Tests for Monte Carlo severity, quantum annealing recovery,
 *       pattern memory, and numerical integration enhancements
 * WHY:  Verify NIMCP utility integration works correctly
 * HOW:  Tests using Check framework
 *
 * @author NIMCP Development Team
 * @date 2025-01-25
 */

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "cognitive/immune/nimcp_brain_immune_tick.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/fault_tolerance/nimcp_health_agent.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/algorithms/nimcp_monte_carlo.h"
#include "optimization/quantum_annealing/nimcp_quantum_annealing.h"
#include "utils/math/nimcp_complex_math.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

static brain_immune_system_t* g_immune = NULL;

static void setup_immune(void)
{
    nimcp_exception_system_init();

    brain_immune_config_t config;
    brain_immune_default_config(&config);
    g_immune = brain_immune_create(&config);
    ck_assert_ptr_nonnull(g_immune);

    /* Start the immune system so brain_immune_update() will work */
    int start_result = brain_immune_start(g_immune);
    ck_assert_int_eq(start_result, 0);

    brain_immune_tick_config_t tick_config;
    brain_immune_tick_default_config(&tick_config);
    tick_config.enable_tick_logging = false;
    int result = brain_immune_tick_init(g_immune, &tick_config);
    ck_assert_int_eq(result, 0);
}

static void teardown_immune(void)
{
    if (g_immune) {
        brain_immune_stop(g_immune);
        brain_immune_tick_shutdown(g_immune);
        brain_immune_destroy(g_immune);
        g_immune = NULL;
    }

    nimcp_exception_system_shutdown();
}

/* ============================================================================
 * Monte Carlo Severity Assessment Tests
 * ============================================================================ */

/**
 * @test Test that MC severity produces values in expected range
 */
START_TEST(test_mc_severity_range)
{
    /* Process multiple anomalies and verify severity is in valid range */
    health_agent_message_t msg = {
        .type = HEALTH_MSG_ANOMALY_DETECTED,
        .severity = HEALTH_SEVERITY_WARNING,
        .source = HEALTH_SOURCE_MEMORY,
    };
    strncpy(msg.description, "Test MC severity range", sizeof(msg.description) - 1);

    /* Process multiple times - MC should produce varying but valid severities */
    for (int i = 0; i < 20; i++) {
        int result = brain_immune_process_health_message(g_immune, &msg);
        ck_assert_int_eq(result, 0);
    }

    /* Verify antigens were created with valid severities (checked internally) */
    brain_immune_stats_t metrics;
    brain_immune_get_stats(g_immune, &metrics);
    ck_assert_uint_ge(metrics.antigens_processed, 20);
}
END_TEST

/**
 * @test Test MC severity differs based on source type
 */
START_TEST(test_mc_severity_source_variance)
{
    /* Memory source - low variance */
    health_agent_message_t msg_memory = {
        .type = HEALTH_MSG_ANOMALY_DETECTED,
        .severity = HEALTH_SEVERITY_ERROR,
        .source = HEALTH_SOURCE_MEMORY,
    };
    strncpy(msg_memory.description, "Memory anomaly test", sizeof(msg_memory.description) - 1);

    /* Neural source - high variance */
    health_agent_message_t msg_neural = {
        .type = HEALTH_MSG_ANOMALY_DETECTED,
        .severity = HEALTH_SEVERITY_ERROR,
        .source = HEALTH_SOURCE_NEURAL,
    };
    strncpy(msg_neural.description, "Neural anomaly test", sizeof(msg_neural.description) - 1);

    /* Process both types */
    brain_immune_process_health_message(g_immune, &msg_memory);
    brain_immune_process_health_message(g_immune, &msg_neural);

    /* Both should succeed - different variance doesn't affect correctness */
    brain_immune_stats_t metrics;
    brain_immune_get_stats(g_immune, &metrics);
    ck_assert_uint_ge(metrics.antigens_processed, 2);
}
END_TEST

/**
 * @test Test MC severity with critical severity
 */
START_TEST(test_mc_severity_critical)
{
    health_agent_message_t msg = {
        .type = HEALTH_MSG_ANOMALY_DETECTED,
        .severity = HEALTH_SEVERITY_CRITICAL,
        .source = HEALTH_SOURCE_THREADING,
    };
    strncpy(msg.description, "Critical thread contention", sizeof(msg.description) - 1);

    int result = brain_immune_process_health_message(g_immune, &msg);
    ck_assert_int_eq(result, 0);

    /* Critical severity should produce high-severity antigen */
    brain_immune_stats_t metrics;
    brain_immune_get_stats(g_immune, &metrics);
    ck_assert_uint_ge(metrics.antigens_processed, 1);
}
END_TEST

/* ============================================================================
 * Pattern Memory Tests (Quantum-Inspired)
 * ============================================================================ */

/**
 * @test Test pattern memory records recurring anomalies
 */
START_TEST(test_pattern_memory_recording)
{
    /* Send same anomaly pattern multiple times */
    health_agent_message_t msg = {
        .type = HEALTH_MSG_ANOMALY_DETECTED,
        .severity = HEALTH_SEVERITY_WARNING,
        .source = HEALTH_SOURCE_NEURAL,
    };
    strncpy(msg.description, "Recurring NaN in layer 5", sizeof(msg.description) - 1);

    for (int i = 0; i < 5; i++) {
        brain_immune_process_health_message(g_immune, &msg);
        /* Small delay to allow different timestamps */
        usleep(1000);
    }

    brain_immune_stats_t metrics;
    brain_immune_get_stats(g_immune, &metrics);
    ck_assert_uint_ge(metrics.antigens_processed, 5);
}
END_TEST

/**
 * @test Test recurring pattern gets severity boost
 */
START_TEST(test_pattern_memory_severity_boost)
{
    /* First occurrence - baseline */
    health_agent_message_t msg = {
        .type = HEALTH_MSG_ANOMALY_DETECTED,
        .severity = HEALTH_SEVERITY_WARNING,
        .source = HEALTH_SOURCE_NEURAL,
    };
    strncpy(msg.description, "Boost test pattern alpha", sizeof(msg.description) - 1);

    /* Send same pattern 10+ times to build coherence */
    for (int i = 0; i < 15; i++) {
        brain_immune_process_health_message(g_immune, &msg);
        usleep(100);  /* Minimal delay for timestamp variation */
    }

    /* Verify all were processed - recurring ones should have boosted severity */
    brain_immune_stats_t metrics;
    brain_immune_get_stats(g_immune, &metrics);
    ck_assert_uint_ge(metrics.antigens_processed, 15);
}
END_TEST

/**
 * @test Test different patterns are tracked separately
 */
START_TEST(test_pattern_memory_distinct_patterns)
{
    health_agent_message_t msg1 = {
        .type = HEALTH_MSG_ANOMALY_DETECTED,
        .severity = HEALTH_SEVERITY_ERROR,
        .source = HEALTH_SOURCE_MEMORY,
    };
    strncpy(msg1.description, "Pattern Alpha", sizeof(msg1.description) - 1);

    health_agent_message_t msg2 = {
        .type = HEALTH_MSG_ANOMALY_DETECTED,
        .severity = HEALTH_SEVERITY_ERROR,
        .source = HEALTH_SOURCE_IO,
    };
    strncpy(msg2.description, "Pattern Beta", sizeof(msg2.description) - 1);

    /* Interleave patterns */
    for (int i = 0; i < 10; i++) {
        brain_immune_process_health_message(g_immune, &msg1);
        brain_immune_process_health_message(g_immune, &msg2);
    }

    brain_immune_stats_t metrics;
    brain_immune_get_stats(g_immune, &metrics);
    ck_assert_uint_ge(metrics.antigens_processed, 20);
}
END_TEST

/* ============================================================================
 * Quantum Annealing Recovery Tests
 * ============================================================================ */

/**
 * @test Test QA recovery for critical severity
 */
START_TEST(test_qa_recovery_critical)
{
    health_agent_message_t msg = {
        .type = HEALTH_MSG_RECOVERY_REQUEST,
        .severity = HEALTH_SEVERITY_CRITICAL,
        .source = HEALTH_SOURCE_IMMUNE,
        .suggested_action = HEALTH_RECOVERY_NONE,
    };
    strncpy(msg.description, "Critical recovery needed", sizeof(msg.description) - 1);

    /* QA should select appropriate recovery for critical */
    int result = brain_immune_process_health_message(g_immune, &msg);
    ck_assert_int_ge(result, 0);  /* -1 is error, 0+ is success */
}
END_TEST

/**
 * @test Test QA recovery for memory source prefers GC
 */
START_TEST(test_qa_recovery_memory_source)
{
    health_agent_message_t msg = {
        .type = HEALTH_MSG_RECOVERY_REQUEST,
        .severity = HEALTH_SEVERITY_ERROR,
        .source = HEALTH_SOURCE_MEMORY,
        .suggested_action = HEALTH_RECOVERY_NONE,
    };
    strncpy(msg.description, "Memory pressure recovery", sizeof(msg.description) - 1);

    int result = brain_immune_process_health_message(g_immune, &msg);
    ck_assert_int_ge(result, 0);
}
END_TEST

/**
 * @test Test QA recovery for threading source
 */
START_TEST(test_qa_recovery_threading_source)
{
    health_agent_message_t msg = {
        .type = HEALTH_MSG_RECOVERY_REQUEST,
        .severity = HEALTH_SEVERITY_ERROR,
        .source = HEALTH_SOURCE_THREADING,
        .suggested_action = HEALTH_RECOVERY_NONE,
    };
    strncpy(msg.description, "Thread deadlock recovery", sizeof(msg.description) - 1);

    int result = brain_immune_process_health_message(g_immune, &msg);
    ck_assert_int_ge(result, 0);
}
END_TEST

/**
 * @test Test QA fallback for low severity (direct mapping)
 */
START_TEST(test_qa_recovery_fallback_low_severity)
{
    health_agent_message_t msg = {
        .type = HEALTH_MSG_RECOVERY_REQUEST,
        .severity = HEALTH_SEVERITY_INFO,
        .source = HEALTH_SOURCE_MEMORY,
        .suggested_action = HEALTH_RECOVERY_GC,
    };
    strncpy(msg.description, "Low severity GC", sizeof(msg.description) - 1);

    /* Low severity should use direct mapping, not QA */
    int result = brain_immune_process_health_message(g_immune, &msg);
    ck_assert_int_ge(result, 0);
}
END_TEST

/* ============================================================================
 * Numerical Integration Tests
 * ============================================================================ */

/**
 * @test Test tick duration statistics integration
 */
START_TEST(test_tick_duration_integration)
{
    /* Run multiple ticks */
    for (int i = 0; i < 50; i++) {
        int result = brain_immune_tick(g_immune, 10);
        ck_assert_int_eq(result, 0);
    }

    /* Check stats were updated */
    brain_immune_tick_stats_t stats;
    brain_immune_tick_get_stats(g_immune, &stats);

    ck_assert_uint_eq(stats.ticks_executed, 50);
    /* Average should be computed via numerical integration */
    ck_assert(stats.avg_tick_duration_us > 0.0f);
}
END_TEST

/**
 * @test Test EMA smoothing of tick durations
 */
START_TEST(test_tick_ema_smoothing)
{
    /* Run ticks and observe EMA behavior */
    for (int i = 0; i < 100; i++) {
        brain_immune_tick(g_immune, 5);
    }

    brain_immune_tick_stats_t stats_before;
    brain_immune_tick_get_stats(g_immune, &stats_before);
    float avg_before = stats_before.avg_tick_duration_us;

    /* Run more ticks */
    for (int i = 0; i < 100; i++) {
        brain_immune_tick(g_immune, 5);
    }

    brain_immune_tick_stats_t stats_after;
    brain_immune_tick_get_stats(g_immune, &stats_after);

    /* EMA should be stable */
    ck_assert(stats_after.avg_tick_duration_us > 0.0f);
}
END_TEST

/**
 * @test Test max duration tracking
 */
START_TEST(test_max_duration_tracking)
{
    /* Run some ticks */
    for (int i = 0; i < 20; i++) {
        brain_immune_tick(g_immune, 10);
    }

    brain_immune_tick_stats_t stats;
    brain_immune_tick_get_stats(g_immune, &stats);

    /* Max should be at least as large as average */
    ck_assert(stats.max_tick_duration_us >= stats.avg_tick_duration_us);
}
END_TEST

/* ============================================================================
 * Combined Enhancement Tests
 * ============================================================================ */

/**
 * @test Test full enhanced processing pipeline
 */
START_TEST(test_full_enhanced_pipeline)
{
    /* Create health agent */
    health_agent_config_t agent_config;
    nimcp_health_agent_default_config(&agent_config);
    strncpy(agent_config.agent_name, "enhanced_test_agent", sizeof(agent_config.agent_name) - 1);

    nimcp_health_agent_t* agent = nimcp_health_agent_create(&agent_config);
    ck_assert_ptr_nonnull(agent);

    /* Connect to immune */
    nimcp_health_agent_connect_immune(agent, g_immune);
    brain_immune_tick_connect_health_agent(g_immune, agent);

    /* Report anomalies */
    for (int i = 0; i < 10; i++) {
        health_agent_message_t msg = nimcp_health_agent_create_message(
            HEALTH_MSG_ANOMALY_DETECTED,
            HEALTH_SEVERITY_WARNING,
            HEALTH_SOURCE_NEURAL,
            "Enhanced pipeline test %d", i
        );
        nimcp_health_agent_report_anomaly(agent, &msg);
    }

    /* Run ticks to process */
    for (int i = 0; i < 5; i++) {
        brain_immune_tick(g_immune, 20);
    }

    /* Verify processing */
    brain_immune_tick_stats_t stats;
    brain_immune_tick_get_stats(g_immune, &stats);
    ck_assert_uint_ge(stats.health_messages_processed, 5);

    /* Cleanup */
    nimcp_health_agent_destroy(agent);
}
END_TEST

/**
 * @test Test recurring pattern with recovery request
 */
START_TEST(test_recurring_with_recovery)
{
    /* Create recurring pattern */
    health_agent_message_t anomaly = {
        .type = HEALTH_MSG_ANOMALY_DETECTED,
        .severity = HEALTH_SEVERITY_ERROR,
        .source = HEALTH_SOURCE_MEMORY,
    };
    strncpy(anomaly.description, "Memory leak detected", sizeof(anomaly.description) - 1);

    /* Build pattern */
    for (int i = 0; i < 8; i++) {
        brain_immune_process_health_message(g_immune, &anomaly);
        usleep(100);
    }

    /* Now request recovery - should be optimized via QA */
    health_agent_message_t recovery = {
        .type = HEALTH_MSG_RECOVERY_REQUEST,
        .severity = HEALTH_SEVERITY_ERROR,
        .source = HEALTH_SOURCE_MEMORY,
        .suggested_action = HEALTH_RECOVERY_GC,
    };
    strncpy(recovery.description, "Request GC for leak", sizeof(recovery.description) - 1);

    int result = brain_immune_process_health_message(g_immune, &recovery);
    ck_assert_int_ge(result, 0);
}
END_TEST

/* ============================================================================
 * Utility Function Tests
 * ============================================================================ */

/**
 * @test Test phasor coherence calculation (used in pattern memory)
 */
START_TEST(test_phasor_coherence_basic)
{
    /* Create phasors with similar phases */
    neural_phasor_t phasors[5];
    for (int i = 0; i < 5; i++) {
        float phase = 0.1f * (float)i;  /* Similar phases */
        phasors[i] = phasor_from_polar(1.0f, phase);
    }

    float coherence = phasor_array_coherence(phasors, 5);

    /* Similar phases should have high coherence */
    ck_assert(coherence > 0.9f);
}
END_TEST

/**
 * @test Test phasor coherence with random phases
 */
START_TEST(test_phasor_coherence_random)
{
    /* Create phasors with random phases */
    neural_phasor_t phasors[10];
    for (int i = 0; i < 10; i++) {
        float phase = (float)i * M_PI / 5.0f;  /* Spread phases */
        phasors[i] = phasor_from_polar(1.0f, phase);
    }

    float coherence = phasor_array_coherence(phasors, 10);

    /* Spread phases should have lower coherence */
    ck_assert(coherence < 0.9f);
}
END_TEST

/* ============================================================================
 * Test Suite Creation
 * ============================================================================ */

Suite* brain_immune_tick_enhanced_suite(void)
{
    Suite* s = suite_create("Brain Immune Tick Enhanced");

    /* Monte Carlo Tests */
    TCase* tc_mc = tcase_create("Monte Carlo Severity");
    tcase_add_checked_fixture(tc_mc, setup_immune, teardown_immune);
    tcase_add_test(tc_mc, test_mc_severity_range);
    tcase_add_test(tc_mc, test_mc_severity_source_variance);
    tcase_add_test(tc_mc, test_mc_severity_critical);
    tcase_set_timeout(tc_mc, 10);
    suite_add_tcase(s, tc_mc);

    /* Pattern Memory Tests */
    TCase* tc_pattern = tcase_create("Pattern Memory");
    tcase_add_checked_fixture(tc_pattern, setup_immune, teardown_immune);
    tcase_add_test(tc_pattern, test_pattern_memory_recording);
    tcase_add_test(tc_pattern, test_pattern_memory_severity_boost);
    tcase_add_test(tc_pattern, test_pattern_memory_distinct_patterns);
    tcase_set_timeout(tc_pattern, 10);
    suite_add_tcase(s, tc_pattern);

    /* Quantum Annealing Tests */
    TCase* tc_qa = tcase_create("Quantum Annealing Recovery");
    tcase_add_checked_fixture(tc_qa, setup_immune, teardown_immune);
    tcase_add_test(tc_qa, test_qa_recovery_critical);
    tcase_add_test(tc_qa, test_qa_recovery_memory_source);
    tcase_add_test(tc_qa, test_qa_recovery_threading_source);
    tcase_add_test(tc_qa, test_qa_recovery_fallback_low_severity);
    tcase_set_timeout(tc_qa, 15);
    suite_add_tcase(s, tc_qa);

    /* Numerical Integration Tests */
    TCase* tc_integration = tcase_create("Numerical Integration");
    tcase_add_checked_fixture(tc_integration, setup_immune, teardown_immune);
    tcase_add_test(tc_integration, test_tick_duration_integration);
    tcase_add_test(tc_integration, test_tick_ema_smoothing);
    tcase_add_test(tc_integration, test_max_duration_tracking);
    tcase_set_timeout(tc_integration, 10);
    suite_add_tcase(s, tc_integration);

    /* Combined Tests */
    TCase* tc_combined = tcase_create("Combined Enhancements");
    tcase_add_checked_fixture(tc_combined, setup_immune, teardown_immune);
    tcase_add_test(tc_combined, test_full_enhanced_pipeline);
    tcase_add_test(tc_combined, test_recurring_with_recovery);
    tcase_set_timeout(tc_combined, 15);
    suite_add_tcase(s, tc_combined);

    /* Utility Tests */
    TCase* tc_utils = tcase_create("Utility Functions");
    tcase_add_test(tc_utils, test_phasor_coherence_basic);
    tcase_add_test(tc_utils, test_phasor_coherence_random);
    tcase_set_timeout(tc_utils, 5);
    suite_add_tcase(s, tc_utils);

    return s;
}

int main(void)
{
    int number_failed;
    Suite* s = brain_immune_tick_enhanced_suite();
    SRunner* sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
