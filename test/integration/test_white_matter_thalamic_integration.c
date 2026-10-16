/**
 * @file test_white_matter_thalamic_integration.c
 * @brief Integration tests for White Matter + Thalamus systems
 * @date 2026-03-05
 *
 * WHAT: Verifies white matter tract conduction delays, myelination effects
 *       on velocity, signal routing with attenuation, and integrity dynamics
 * WHY:  White matter tracts route signals between brain regions with
 *       myelination-dependent velocity; correct delay modeling is essential
 *       for timing-sensitive inter-region communication
 * HOW:  Uses Check framework; creates white matter system, modulates
 *       myelination, routes signals, and verifies delay/velocity/integrity
 */

#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "core/brain/white_matter/nimcp_white_matter_tracts.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

static wmt_system_t* g_wmt = NULL;

static void setup(void)
{
    wmt_config_t cfg = wmt_default_config();
    /* Disable velocity jitter for deterministic tests */
    cfg.enable_velocity_jitter = false;
    cfg.enable_integrity_decay = false;
    g_wmt = wmt_create(&cfg);
    ck_assert_ptr_nonnull(g_wmt);
}

static void teardown(void)
{
    if (g_wmt) {
        wmt_destroy(g_wmt);
        g_wmt = NULL;
    }
}

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

START_TEST(test_wmt_create_defaults)
{
    wmt_system_t* sys = wmt_create(NULL);
    ck_assert_ptr_nonnull(sys);
    wmt_destroy(sys);
}
END_TEST

START_TEST(test_wmt_destroy_null_safe)
{
    wmt_destroy(NULL);
}
END_TEST

/* ============================================================================
 * Tract Delay Tests
 * ============================================================================ */

START_TEST(test_corpus_callosum_delay_positive)
{
    float delay = wmt_get_tract_delay(g_wmt, WMT_CORPUS_CALLOSUM);
    /* Delay should be positive (tract has non-zero length) */
    ck_assert_float_gt(delay, 0.0f);
}
END_TEST

START_TEST(test_longer_tract_has_more_delay)
{
    float cc_delay = wmt_get_tract_delay(g_wmt, WMT_CORPUS_CALLOSUM);
    float cst_delay = wmt_get_tract_delay(g_wmt, WMT_CORTICOSPINAL);

    /* Corticospinal (~45cm) should have more delay than corpus callosum (~10cm)
       assuming similar myelination */
    ck_assert_float_gt(cst_delay, cc_delay);
}
END_TEST

START_TEST(test_all_tracts_have_valid_delays)
{
    for (int t = 0; t < WMT_COUNT; t++) {
        float delay = wmt_get_tract_delay(g_wmt, (white_matter_tract_t)t);
        ck_assert_float_gt(delay, 0.0f);
        /* Delays should be reasonable: 0.1ms to 100ms */
        ck_assert_float_lt(delay, 100.0f);
    }
}
END_TEST

/* ============================================================================
 * Myelination Effect on Velocity Tests
 * ============================================================================ */

START_TEST(test_myelination_affects_velocity)
{
    float vel_before = wmt_get_tract_velocity(g_wmt, WMT_ARCUATE_FASCICULUS);
    ck_assert_float_gt(vel_before, 0.0f);

    /* Reduce myelination */
    int rc = wmt_modulate_myelination(g_wmt, WMT_ARCUATE_FASCICULUS, -0.3f);
    ck_assert_int_eq(rc, 0);

    /* Update to recompute velocity */
    wmt_update(g_wmt, 0.01f);

    float vel_after = wmt_get_tract_velocity(g_wmt, WMT_ARCUATE_FASCICULUS);

    /* Demyelination should reduce conduction velocity */
    ck_assert_float_lt(vel_after, vel_before);
}
END_TEST

START_TEST(test_myelination_clamped)
{
    /* Try to push myelination beyond 1.0 */
    wmt_modulate_myelination(g_wmt, WMT_CINGULUM, +2.0f);
    float myelin = wmt_get_tract_myelination(g_wmt, WMT_CINGULUM);
    ck_assert_float_le(myelin, 1.0f);

    /* Try to push myelination below 0.0 */
    wmt_modulate_myelination(g_wmt, WMT_CINGULUM, -5.0f);
    myelin = wmt_get_tract_myelination(g_wmt, WMT_CINGULUM);
    ck_assert_float_ge(myelin, 0.0f);
}
END_TEST

/* ============================================================================
 * Integrity Tests
 * ============================================================================ */

START_TEST(test_integrity_initial_healthy)
{
    for (int t = 0; t < WMT_COUNT; t++) {
        float integrity = wmt_get_tract_integrity(g_wmt, (white_matter_tract_t)t);
        /* Default integrity should be 1.0 (healthy) */
        ck_assert_float_ge(integrity, 0.5f);
        ck_assert_float_le(integrity, 1.0f);
    }
}
END_TEST

START_TEST(test_integrity_modulation)
{
    float before = wmt_get_tract_integrity(g_wmt, WMT_UNCINATE_FASCICULUS);

    /* Damage integrity */
    int rc = wmt_modulate_integrity(g_wmt, WMT_UNCINATE_FASCICULUS, -0.3f);
    ck_assert_int_eq(rc, 0);

    float after = wmt_get_tract_integrity(g_wmt, WMT_UNCINATE_FASCICULUS);
    ck_assert_float_lt(after, before);
}
END_TEST

/* ============================================================================
 * Signal Routing Tests
 * ============================================================================ */

START_TEST(test_signal_routing_attenuation)
{
    float amplitude = 1.0f;
    float attenuated = 0.0f;
    float delay_ms = 0.0f;

    int rc = wmt_route_signal(g_wmt, WMT_OPTIC_RADIATION, amplitude,
                               &attenuated, &delay_ms);
    ck_assert_int_eq(rc, 0);

    /* Attenuated signal should be <= original amplitude */
    ck_assert_float_le(attenuated, amplitude);
    ck_assert_float_ge(attenuated, 0.0f);

    /* Delay should be positive */
    ck_assert_float_gt(delay_ms, 0.0f);
}
END_TEST

START_TEST(test_damaged_tract_attenuates_more)
{
    float amp_in = 1.0f;
    float amp_healthy = 0.0f;
    float delay_healthy = 0.0f;

    wmt_route_signal(g_wmt, WMT_IFOF, amp_in, &amp_healthy, &delay_healthy);

    /* Damage the tract */
    wmt_modulate_integrity(g_wmt, WMT_IFOF, -0.5f);

    float amp_damaged = 0.0f;
    float delay_damaged = 0.0f;
    wmt_route_signal(g_wmt, WMT_IFOF, amp_in, &amp_damaged, &delay_damaged);

    /* Damaged tract should attenuate more */
    ck_assert_float_le(amp_damaged, amp_healthy);
}
END_TEST

/* ============================================================================
 * Statistics and Utility Tests
 * ============================================================================ */

START_TEST(test_stats_valid)
{
    wmt_stats_t stats;
    int rc = wmt_get_stats(g_wmt, &stats);
    ck_assert_int_eq(rc, 0);

    ck_assert_float_ge(stats.mean_myelination, 0.0f);
    ck_assert_float_le(stats.mean_myelination, 1.0f);
    ck_assert_float_ge(stats.mean_integrity, 0.0f);
    ck_assert_float_le(stats.mean_integrity, 1.0f);
    ck_assert_float_gt(stats.mean_conduction_velocity, 0.0f);
}
END_TEST

START_TEST(test_tract_names)
{
    for (int t = 0; t < WMT_COUNT; t++) {
        const char* name = wmt_tract_name((white_matter_tract_t)t);
        ck_assert_ptr_nonnull(name);
    }

    /* Invalid tract should return "UNKNOWN" */
    const char* unknown = wmt_tract_name((white_matter_tract_t)99);
    ck_assert_ptr_nonnull(unknown);
}
END_TEST

/* ============================================================================
 * Test Suite
 * ============================================================================ */

Suite* white_matter_thalamic_integration_suite(void)
{
    Suite* s = suite_create("White Matter Thalamic Integration");

    TCase* tc_lifecycle = tcase_create("Lifecycle");
    tcase_add_test(tc_lifecycle, test_wmt_create_defaults);
    tcase_add_test(tc_lifecycle, test_wmt_destroy_null_safe);
    tcase_set_timeout(tc_lifecycle, 10);
    suite_add_tcase(s, tc_lifecycle);

    TCase* tc_delay = tcase_create("Tract Delays");
    tcase_add_checked_fixture(tc_delay, setup, teardown);
    tcase_add_test(tc_delay, test_corpus_callosum_delay_positive);
    tcase_add_test(tc_delay, test_longer_tract_has_more_delay);
    tcase_add_test(tc_delay, test_all_tracts_have_valid_delays);
    tcase_set_timeout(tc_delay, 10);
    suite_add_tcase(s, tc_delay);

    TCase* tc_myelin = tcase_create("Myelination");
    tcase_add_checked_fixture(tc_myelin, setup, teardown);
    tcase_add_test(tc_myelin, test_myelination_affects_velocity);
    tcase_add_test(tc_myelin, test_myelination_clamped);
    tcase_set_timeout(tc_myelin, 10);
    suite_add_tcase(s, tc_myelin);

    TCase* tc_integrity = tcase_create("Integrity");
    tcase_add_checked_fixture(tc_integrity, setup, teardown);
    tcase_add_test(tc_integrity, test_integrity_initial_healthy);
    tcase_add_test(tc_integrity, test_integrity_modulation);
    tcase_set_timeout(tc_integrity, 10);
    suite_add_tcase(s, tc_integrity);

    TCase* tc_routing = tcase_create("Signal Routing");
    tcase_add_checked_fixture(tc_routing, setup, teardown);
    tcase_add_test(tc_routing, test_signal_routing_attenuation);
    tcase_add_test(tc_routing, test_damaged_tract_attenuates_more);
    tcase_set_timeout(tc_routing, 10);
    suite_add_tcase(s, tc_routing);

    TCase* tc_stats = tcase_create("Statistics");
    tcase_add_checked_fixture(tc_stats, setup, teardown);
    tcase_add_test(tc_stats, test_stats_valid);
    tcase_add_test(tc_stats, test_tract_names);
    tcase_set_timeout(tc_stats, 10);
    suite_add_tcase(s, tc_stats);

    return s;
}

int main(void)
{
    int number_failed;
    Suite* s = white_matter_thalamic_integration_suite();
    SRunner* sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
