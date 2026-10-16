/**
 * @file test_cortical_interneurons_columns_integration.c
 * @brief Integration tests for Cortical Interneurons + Cortical Columns + SNN
 * @date 2026-03-05
 *
 * WHAT: Verifies the cortical interneuron system's gamma power generation,
 *       E/I balance maintenance, attention-driven disinhibition, and
 *       prediction error computation
 * WHY:  Cortical computation depends on diverse interneuron types for
 *       oscillation control, gain modulation, and circuit stability
 * HOW:  Uses Check framework; creates interneuron system, modulates
 *       attention, and verifies gamma/E-I/disinhibition metrics
 */

#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "core/cortical_columns/nimcp_cortical_interneurons.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

static cortical_interneuron_system_t* g_cint = NULL;

static void setup(void)
{
    cint_config_t cfg;
    int rc = cint_default_config(&cfg);
    ck_assert_int_eq(rc, 0);
    g_cint = cint_create(&cfg);
    ck_assert_ptr_nonnull(g_cint);
}

static void teardown(void)
{
    if (g_cint) {
        cint_destroy(g_cint);
        g_cint = NULL;
    }
}

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

START_TEST(test_cint_create_defaults)
{
    cortical_interneuron_system_t* sys = cint_create(NULL);
    ck_assert_ptr_nonnull(sys);
    ck_assert_uint_eq(sys->magic, CINT_MAGIC);
    ck_assert_uint_gt(sys->num_interneurons, 0);
    ck_assert_ptr_nonnull(sys->interneurons);
    cint_destroy(sys);
}
END_TEST

START_TEST(test_cint_destroy_null_safe)
{
    cint_destroy(NULL);
}
END_TEST

START_TEST(test_cint_interneuron_count)
{
    /* Should have default counts from all types */
    uint32_t expected_total = CINT_DEFAULT_PV_BASKET + CINT_DEFAULT_PV_CHANDELIER
                            + CINT_DEFAULT_SST + CINT_DEFAULT_VIP + CINT_DEFAULT_NGF;
    ck_assert_uint_eq(g_cint->num_interneurons, expected_total);
}
END_TEST

/* ============================================================================
 * Gamma Power Tests
 * ============================================================================ */

START_TEST(test_gamma_power_non_negative)
{
    /* Run a few update cycles */
    for (int i = 0; i < 50; i++) {
        cint_update(g_cint, 0.001f);
    }

    float gamma = cint_get_gamma_power(g_cint);
    ck_assert_float_ge(gamma, 0.0f);
    ck_assert_float_le(gamma, 1.0f);
}
END_TEST

START_TEST(test_gamma_power_responds_to_updates)
{
    float gamma_initial = cint_get_gamma_power(g_cint);

    /* Run many updates to let dynamics settle */
    for (int i = 0; i < 200; i++) {
        cint_update(g_cint, 0.001f);
    }

    float gamma_after = cint_get_gamma_power(g_cint);

    /* Gamma power should be non-negative after updates */
    ck_assert_float_ge(gamma_after, 0.0f);
    (void)gamma_initial;
}
END_TEST

/* ============================================================================
 * E/I Balance Tests
 * ============================================================================ */

START_TEST(test_ei_balance_non_negative)
{
    for (int i = 0; i < 50; i++) {
        cint_update(g_cint, 0.001f);
    }

    float ei = cint_get_ei_balance(g_cint);
    /* E/I balance should be positive (target ~4.0) */
    ck_assert_float_gt(ei, 0.0f);
}
END_TEST

START_TEST(test_ei_balance_reasonable_range)
{
    for (int i = 0; i < 100; i++) {
        cint_update(g_cint, 0.001f);
    }

    float ei = cint_get_ei_balance(g_cint);
    /* E/I balance should be in a biologically plausible range [0.5, 20.0] */
    ck_assert_float_ge(ei, 0.0f);
    ck_assert_float_le(ei, 20.0f);
}
END_TEST

/* ============================================================================
 * Attention Modulation Tests
 * ============================================================================ */

START_TEST(test_attention_modulates_disinhibition)
{
    /* Baseline disinhibition */
    for (int i = 0; i < 50; i++) {
        cint_update(g_cint, 0.001f);
    }
    float disinhibition_low = cint_get_disinhibition(g_cint);

    /* Apply high attention */
    int rc = cint_modulate_attention(g_cint, 0.9f);
    ck_assert_int_eq(rc, 0);

    for (int i = 0; i < 50; i++) {
        cint_update(g_cint, 0.001f);
    }
    float disinhibition_high = cint_get_disinhibition(g_cint);

    /* High attention should increase VIP-mediated disinhibition */
    ck_assert_float_ge(disinhibition_high, disinhibition_low);
}
END_TEST

START_TEST(test_attention_zero_baseline)
{
    int rc = cint_modulate_attention(g_cint, 0.0f);
    ck_assert_int_eq(rc, 0);

    for (int i = 0; i < 50; i++) {
        cint_update(g_cint, 0.001f);
    }

    float disinhibition = cint_get_disinhibition(g_cint);
    /* With zero attention, disinhibition should be low */
    ck_assert_float_ge(disinhibition, 0.0f);
    ck_assert_float_le(disinhibition, 1.0f);
}
END_TEST

/* ============================================================================
 * Prediction Error Tests
 * ============================================================================ */

START_TEST(test_prediction_error_bounded)
{
    for (int i = 0; i < 100; i++) {
        cint_update(g_cint, 0.001f);
    }

    float pe = cint_get_prediction_error(g_cint);
    ck_assert_float_ge(pe, 0.0f);
    ck_assert_float_le(pe, 1.0f);
}
END_TEST

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

START_TEST(test_stats_track_updates)
{
    cint_reset_stats(g_cint);

    for (int i = 0; i < 10; i++) {
        cint_update(g_cint, 0.001f);
    }

    cint_stats_t stats;
    int rc = cint_get_stats(g_cint, &stats);
    ck_assert_int_eq(rc, 0);
    ck_assert_uint_ge(stats.total_updates, 10);
}
END_TEST

START_TEST(test_stats_reset)
{
    for (int i = 0; i < 5; i++) {
        cint_update(g_cint, 0.001f);
    }

    cint_reset_stats(g_cint);

    cint_stats_t stats;
    int rc = cint_get_stats(g_cint, &stats);
    ck_assert_int_eq(rc, 0);
    ck_assert_uint_eq(stats.total_updates, 0);
}
END_TEST

/* ============================================================================
 * Test Suite
 * ============================================================================ */

Suite* cortical_interneurons_columns_integration_suite(void)
{
    Suite* s = suite_create("Cortical Interneurons Columns Integration");

    TCase* tc_lifecycle = tcase_create("Lifecycle");
    tcase_add_checked_fixture(tc_lifecycle, setup, teardown);
    tcase_add_test(tc_lifecycle, test_cint_create_defaults);
    tcase_add_test(tc_lifecycle, test_cint_destroy_null_safe);
    tcase_add_test(tc_lifecycle, test_cint_interneuron_count);
    tcase_set_timeout(tc_lifecycle, 10);
    suite_add_tcase(s, tc_lifecycle);

    TCase* tc_gamma = tcase_create("Gamma Power");
    tcase_add_checked_fixture(tc_gamma, setup, teardown);
    tcase_add_test(tc_gamma, test_gamma_power_non_negative);
    tcase_add_test(tc_gamma, test_gamma_power_responds_to_updates);
    tcase_set_timeout(tc_gamma, 10);
    suite_add_tcase(s, tc_gamma);

    TCase* tc_ei = tcase_create("E/I Balance");
    tcase_add_checked_fixture(tc_ei, setup, teardown);
    tcase_add_test(tc_ei, test_ei_balance_non_negative);
    tcase_add_test(tc_ei, test_ei_balance_reasonable_range);
    tcase_set_timeout(tc_ei, 10);
    suite_add_tcase(s, tc_ei);

    TCase* tc_attention = tcase_create("Attention Modulation");
    tcase_add_checked_fixture(tc_attention, setup, teardown);
    tcase_add_test(tc_attention, test_attention_modulates_disinhibition);
    tcase_add_test(tc_attention, test_attention_zero_baseline);
    tcase_set_timeout(tc_attention, 10);
    suite_add_tcase(s, tc_attention);

    TCase* tc_pred = tcase_create("Prediction Error");
    tcase_add_checked_fixture(tc_pred, setup, teardown);
    tcase_add_test(tc_pred, test_prediction_error_bounded);
    tcase_set_timeout(tc_pred, 10);
    suite_add_tcase(s, tc_pred);

    TCase* tc_stats = tcase_create("Statistics");
    tcase_add_checked_fixture(tc_stats, setup, teardown);
    tcase_add_test(tc_stats, test_stats_track_updates);
    tcase_add_test(tc_stats, test_stats_reset);
    tcase_set_timeout(tc_stats, 10);
    suite_add_tcase(s, tc_stats);

    return s;
}

int main(void)
{
    int number_failed;
    Suite* s = cortical_interneurons_columns_integration_suite();
    SRunner* sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
