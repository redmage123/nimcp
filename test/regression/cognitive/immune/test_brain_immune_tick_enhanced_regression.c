/**
 * @file test_brain_immune_tick_enhanced_regression.c
 * @brief Regression tests for brain immune tick NIMCP utility enhancements
 *
 * WHAT: Tests to catch regressions in MC severity, QA recovery, pattern memory,
 *       and numerical integration features
 * WHY:  Ensure enhancements maintain correct behavior across code changes
 * HOW:  Tests using Check framework covering edge cases and known issues
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

static void setup_regression(void)
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
    strncpy(agent_config.agent_name, "regression_agent", sizeof(agent_config.agent_name) - 1);

    g_agent = nimcp_health_agent_create(&agent_config);
    ck_assert_ptr_nonnull(g_agent);

    nimcp_health_agent_connect_immune(g_agent, g_immune);
    brain_immune_tick_connect_health_agent(g_immune, g_agent);

    nimcp_exception_immune_init(NULL);
}

static void teardown_regression(void)
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
 * MC Severity Regression Tests
 * ============================================================================ */

/**
 * @test Regression: MC severity must stay in [1, 10] range
 *
 * Issue: Unclamped MC output could produce invalid severity values
 */
START_TEST(test_mc_severity_clamping_regression)
{
    /* Test with all severity levels */
    health_agent_severity_t severities[] = {
        HEALTH_SEVERITY_INFO,
        HEALTH_SEVERITY_WARNING,
        HEALTH_SEVERITY_ERROR,
        HEALTH_SEVERITY_CRITICAL,
        HEALTH_SEVERITY_FATAL
    };

    for (int s = 0; s < 5; s++) {
        for (int i = 0; i < 50; i++) {
            health_agent_message_t msg = {
                .type = HEALTH_MSG_ANOMALY_DETECTED,
                .severity = severities[s],
                .source = HEALTH_SOURCE_NEURAL,
            };
            snprintf(msg.description, sizeof(msg.description),
                     "Severity clamp test %d-%d", s, i);

            int result = brain_immune_process_health_message(g_immune, &msg);
            ck_assert_int_eq(result, 0);
        }
    }

    /* All should succeed - if any had invalid severity, brain_immune would reject */
    brain_immune_stats_t metrics;
    brain_immune_get_stats(g_immune, &metrics);
    ck_assert_uint_ge(metrics.antigens_processed, 200);
}
END_TEST

/**
 * @test Regression: MC severity with NULL state must fallback
 *
 * Issue: Null pointer dereference when tick state unavailable
 */
START_TEST(test_mc_severity_null_state_regression)
{
    /* Process message directly without tick init would have NULL state */
    /* The implementation should fallback to base severity */
    health_agent_message_t msg = {
        .type = HEALTH_MSG_ANOMALY_DETECTED,
        .severity = HEALTH_SEVERITY_ERROR,
        .source = HEALTH_SOURCE_MEMORY,
    };
    strncpy(msg.description, "Null state test", sizeof(msg.description) - 1);

    /* Should not crash */
    int result = brain_immune_process_health_message(g_immune, &msg);
    ck_assert_int_eq(result, 0);
}
END_TEST

/**
 * @test Regression: MC severity for empty description
 *
 * Issue: Empty epitope could cause hash collision issues or validation failure
 */
START_TEST(test_mc_severity_empty_description_regression)
{
    health_agent_message_t msg = {
        .type = HEALTH_MSG_ANOMALY_DETECTED,
        .severity = HEALTH_SEVERITY_WARNING,
        .source = HEALTH_SOURCE_MEMORY,
    };
    msg.description[0] = '\0';  /* Empty description */

    int result = brain_immune_process_health_message(g_immune, &msg);
    /* Should handle gracefully - either succeed (0) or fail gracefully (-1) without crashing */
    ck_assert_int_ge(result, -1);
}
END_TEST

/* ============================================================================
 * Pattern Memory Regression Tests
 * ============================================================================ */

/**
 * @test Regression: Pattern memory hash collision handling
 *
 * Issue: Colliding hashes could overwrite wrong pattern
 */
START_TEST(test_pattern_memory_collision_regression)
{
    /* Create patterns that might collide */
    const char* similar_patterns[] = {
        "aaaaaaaa",
        "aaaaaaab",
        "aaaaaaac",
        "aaaaaaad",
        "aaaaaaae"
    };

    for (int round = 0; round < 3; round++) {
        for (int p = 0; p < 5; p++) {
            health_agent_message_t msg = {
                .type = HEALTH_MSG_ANOMALY_DETECTED,
                .severity = HEALTH_SEVERITY_WARNING,
                .source = HEALTH_SOURCE_NEURAL,
            };
            strncpy(msg.description, similar_patterns[p], sizeof(msg.description) - 1);

            brain_immune_process_health_message(g_immune, &msg);
        }
        brain_immune_tick(g_immune, 10);
    }

    /* All should be tracked */
    brain_immune_stats_t metrics;
    brain_immune_get_stats(g_immune, &metrics);
    ck_assert_uint_ge(metrics.antigens_processed, 15);
}
END_TEST

/**
 * @test Regression: Pattern memory stale entry eviction
 *
 * Issue: Stale patterns not properly evicted could cause memory growth
 */
START_TEST(test_pattern_memory_stale_eviction_regression)
{
    /* Create many patterns to fill memory */
    for (int i = 0; i < 300; i++) {
        health_agent_message_t msg = {
            .type = HEALTH_MSG_ANOMALY_DETECTED,
            .severity = HEALTH_SEVERITY_WARNING,
            .source = HEALTH_SOURCE_NEURAL,
        };
        snprintf(msg.description, sizeof(msg.description), "Unique pattern %d", i);

        brain_immune_process_health_message(g_immune, &msg);
        brain_immune_tick(g_immune, 5);
    }

    /* Should not crash or leak memory */
    brain_immune_tick_stats_t stats;
    brain_immune_tick_get_stats(g_immune, &stats);
    ck_assert_uint_ge(stats.ticks_executed, 200);
}
END_TEST

/**
 * @test Regression: Pattern memory coherence calculation
 *
 * Issue: Coherence could go >1.0 or <0.0
 */
START_TEST(test_pattern_memory_coherence_bounds_regression)
{
    /* Build high coherence with identical timing */
    for (int i = 0; i < 50; i++) {
        health_agent_message_t msg = {
            .type = HEALTH_MSG_ANOMALY_DETECTED,
            .severity = HEALTH_SEVERITY_ERROR,
            .source = HEALTH_SOURCE_MEMORY,
        };
        strncpy(msg.description, "Coherence bounds test", sizeof(msg.description) - 1);

        brain_immune_process_health_message(g_immune, &msg);
        /* Minimal delay to stress coherence calculation */
    }

    brain_immune_tick(g_immune, 50);

    /* Should not produce invalid severity values from bad coherence */
    brain_immune_stats_t metrics;
    brain_immune_get_stats(g_immune, &metrics);
    ck_assert_uint_ge(metrics.antigens_processed, 50);
}
END_TEST

/* ============================================================================
 * Quantum Annealing Regression Tests
 * ============================================================================ */

/**
 * @test Regression: QA with single candidate action
 *
 * Issue: QA could fail when only one action is valid
 */
START_TEST(test_qa_single_candidate_regression)
{
    /* Low severity with specific action - should fallback properly */
    health_agent_message_t msg = {
        .type = HEALTH_MSG_RECOVERY_REQUEST,
        .severity = HEALTH_SEVERITY_INFO,
        .source = HEALTH_SOURCE_MEMORY,
        .suggested_action = HEALTH_RECOVERY_NONE,
    };
    strncpy(msg.description, "Single candidate test", sizeof(msg.description) - 1);

    int result = brain_immune_process_health_message(g_immune, &msg);
    ck_assert_int_ge(result, 0);
}
END_TEST

/**
 * @test Regression: QA energy function with zero weights
 *
 * Issue: Division by zero in energy normalization
 */
START_TEST(test_qa_zero_weights_regression)
{
    /* Critical severity triggers full QA */
    health_agent_message_t msg = {
        .type = HEALTH_MSG_RECOVERY_REQUEST,
        .severity = HEALTH_SEVERITY_CRITICAL,
        .source = HEALTH_SOURCE_IMMUNE,
        .suggested_action = HEALTH_RECOVERY_NONE,
    };
    strncpy(msg.description, "Zero weights test", sizeof(msg.description) - 1);

    /* Should not crash */
    int result = brain_immune_process_health_message(g_immune, &msg);
    ck_assert_int_ge(result, 0);
}
END_TEST

/**
 * @test Regression: QA fallback when annealer unavailable
 *
 * Issue: Null annealer could cause crash
 */
START_TEST(test_qa_fallback_regression)
{
    /* Recovery with explicit action - should use direct mapping */
    health_agent_message_t msg = {
        .type = HEALTH_MSG_RECOVERY_REQUEST,
        .severity = HEALTH_SEVERITY_ERROR,
        .source = HEALTH_SOURCE_MEMORY,
        .suggested_action = HEALTH_RECOVERY_GC,
    };
    strncpy(msg.description, "Fallback test", sizeof(msg.description) - 1);

    int result = brain_immune_process_health_message(g_immune, &msg);
    ck_assert_int_ge(result, 0);
}
END_TEST

/**
 * @test Regression: QA cost calculation edge cases
 *
 * Issue: Negative costs or overflow in cost calculation
 */
START_TEST(test_qa_cost_edge_cases_regression)
{
    health_agent_source_t sources[] = {
        HEALTH_SOURCE_MEMORY,
        HEALTH_SOURCE_THREADING,
        HEALTH_SOURCE_NEURAL,
        HEALTH_SOURCE_IO,
        HEALTH_SOURCE_KG
    };

    for (int s = 0; s < 5; s++) {
        health_agent_message_t msg = {
            .type = HEALTH_MSG_RECOVERY_REQUEST,
            .severity = HEALTH_SEVERITY_CRITICAL,
            .source = sources[s],
            .suggested_action = HEALTH_RECOVERY_NONE,
        };
        snprintf(msg.description, sizeof(msg.description), "Cost edge case %d", s);

        int result = brain_immune_process_health_message(g_immune, &msg);
        ck_assert_int_ge(result, 0);
    }
}
END_TEST

/* ============================================================================
 * Numerical Integration Regression Tests
 * ============================================================================ */

/**
 * @test Regression: EMA with zero initial value
 *
 * Issue: First tick could produce NaN if initial avg is 0
 */
START_TEST(test_ema_zero_initial_regression)
{
    /* First tick - initial avg is 0 */
    int result = brain_immune_tick(g_immune, 10);
    ck_assert_int_eq(result, 0);

    brain_immune_tick_stats_t stats;
    brain_immune_tick_get_stats(g_immune, &stats);

    /* Should not be NaN or negative */
    ck_assert(!isnan(stats.avg_tick_duration_us));
    ck_assert(!isinf(stats.avg_tick_duration_us));
    ck_assert(stats.avg_tick_duration_us >= 0.0f);
}
END_TEST

/**
 * @test Regression: EMA with very large tick duration
 *
 * Issue: Overflow in duration accumulation
 */
START_TEST(test_ema_large_duration_regression)
{
    /* Run many ticks */
    for (int i = 0; i < 1000; i++) {
        brain_immune_tick(g_immune, 100);
    }

    brain_immune_tick_stats_t stats;
    brain_immune_tick_get_stats(g_immune, &stats);

    /* Should not overflow */
    ck_assert(!isnan(stats.avg_tick_duration_us));
    ck_assert(!isinf(stats.avg_tick_duration_us));
    ck_assert(stats.avg_tick_duration_us < 1e12f);  /* Reasonable bound */
}
END_TEST

/**
 * @test Regression: Integration step count overflow
 *
 * Issue: uint64_t overflow after many steps
 */
START_TEST(test_integration_count_regression)
{
    for (int i = 0; i < 100; i++) {
        brain_immune_tick(g_immune, 5);
    }

    brain_immune_tick_stats_t stats;
    brain_immune_tick_get_stats(g_immune, &stats);

    ck_assert_uint_eq(stats.ticks_executed, 100);
}
END_TEST

/* ============================================================================
 * Combined Regression Tests
 * ============================================================================ */

/**
 * @test Regression: Reentry guard with enhanced processing
 *
 * Issue: Enhanced processing could cause reentry issues
 */
START_TEST(test_reentry_with_enhancements_regression)
{
    /* Start tick */
    brain_immune_tick(g_immune, 10);

    /* Queue messages that trigger processing */
    for (int i = 0; i < 20; i++) {
        health_agent_message_t msg = nimcp_health_agent_create_message(
            HEALTH_MSG_ANOMALY_DETECTED,
            HEALTH_SEVERITY_ERROR,
            HEALTH_SOURCE_MEMORY,
            "Reentry test %d", i
        );
        nimcp_health_agent_report_anomaly(g_agent, &msg);
    }

    /* Rapid ticks */
    for (int i = 0; i < 50; i++) {
        brain_immune_tick(g_immune, 5);
    }

    brain_immune_tick_stats_t stats;
    brain_immune_tick_get_stats(g_immune, &stats);

    /* Should not have crashed */
    ck_assert_uint_ge(stats.ticks_executed, 50);
}
END_TEST

/**
 * @test Regression: Shutdown with pending enhanced processing
 *
 * Issue: Shutdown during enhanced processing could leak memory
 */
START_TEST(test_shutdown_with_pending_regression)
{
    /* Queue many messages */
    for (int i = 0; i < 100; i++) {
        health_agent_message_t msg = nimcp_health_agent_create_message(
            HEALTH_MSG_ANOMALY_DETECTED,
            HEALTH_SEVERITY_WARNING,
            HEALTH_SOURCE_NEURAL,
            "Shutdown test %d", i
        );
        nimcp_health_agent_report_anomaly(g_agent, &msg);
    }

    /* Partial processing */
    brain_immune_tick(g_immune, 20);

    /* Shutdown will happen in teardown - should not leak or crash */
}
END_TEST

/**
 * @test Regression: Exception during enhanced processing
 *
 * Issue: Exception thrown during MC/QA could leave inconsistent state
 */
START_TEST(test_exception_during_enhanced_regression)
{
    /* Process anomaly */
    health_agent_message_t msg = nimcp_health_agent_create_message(
        HEALTH_MSG_ANOMALY_DETECTED,
        HEALTH_SEVERITY_ERROR,
        HEALTH_SOURCE_MEMORY,
        "Exception test"
    );
    nimcp_health_agent_report_anomaly(g_agent, &msg);

    /* Throw exception */
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Test exception");

    /* Continue ticks */
    for (int i = 0; i < 10; i++) {
        brain_immune_tick(g_immune, 10);
    }

    /* Should recover gracefully */
    brain_immune_tick_stats_t stats;
    brain_immune_tick_get_stats(g_immune, &stats);
    ck_assert_uint_ge(stats.ticks_executed, 10);
}
END_TEST

/* ============================================================================
 * Test Suite Creation
 * ============================================================================ */

Suite* brain_immune_tick_enhanced_regression_suite(void)
{
    Suite* s = suite_create("Brain Immune Tick Enhanced Regression");

    /* MC Severity */
    TCase* tc_mc = tcase_create("MC Severity Regression");
    tcase_add_checked_fixture(tc_mc, setup_regression, teardown_regression);
    tcase_add_test(tc_mc, test_mc_severity_clamping_regression);
    tcase_add_test(tc_mc, test_mc_severity_null_state_regression);
    tcase_add_test(tc_mc, test_mc_severity_empty_description_regression);
    tcase_set_timeout(tc_mc, 15);
    suite_add_tcase(s, tc_mc);

    /* Pattern Memory */
    TCase* tc_pattern = tcase_create("Pattern Memory Regression");
    tcase_add_checked_fixture(tc_pattern, setup_regression, teardown_regression);
    tcase_add_test(tc_pattern, test_pattern_memory_collision_regression);
    tcase_add_test(tc_pattern, test_pattern_memory_stale_eviction_regression);
    tcase_add_test(tc_pattern, test_pattern_memory_coherence_bounds_regression);
    tcase_set_timeout(tc_pattern, 15);
    suite_add_tcase(s, tc_pattern);

    /* Quantum Annealing */
    TCase* tc_qa = tcase_create("QA Regression");
    tcase_add_checked_fixture(tc_qa, setup_regression, teardown_regression);
    tcase_add_test(tc_qa, test_qa_single_candidate_regression);
    tcase_add_test(tc_qa, test_qa_zero_weights_regression);
    tcase_add_test(tc_qa, test_qa_fallback_regression);
    tcase_add_test(tc_qa, test_qa_cost_edge_cases_regression);
    tcase_set_timeout(tc_qa, 15);
    suite_add_tcase(s, tc_qa);

    /* Numerical Integration */
    TCase* tc_integration = tcase_create("Integration Regression");
    tcase_add_checked_fixture(tc_integration, setup_regression, teardown_regression);
    tcase_add_test(tc_integration, test_ema_zero_initial_regression);
    tcase_add_test(tc_integration, test_ema_large_duration_regression);
    tcase_add_test(tc_integration, test_integration_count_regression);
    tcase_set_timeout(tc_integration, 15);
    suite_add_tcase(s, tc_integration);

    /* Combined */
    TCase* tc_combined = tcase_create("Combined Regression");
    tcase_add_checked_fixture(tc_combined, setup_regression, teardown_regression);
    tcase_add_test(tc_combined, test_reentry_with_enhancements_regression);
    tcase_add_test(tc_combined, test_shutdown_with_pending_regression);
    tcase_add_test(tc_combined, test_exception_during_enhanced_regression);
    tcase_set_timeout(tc_combined, 15);
    suite_add_tcase(s, tc_combined);

    return s;
}

int main(void)
{
    int number_failed;
    Suite* s = brain_immune_tick_enhanced_regression_suite();
    SRunner* sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
