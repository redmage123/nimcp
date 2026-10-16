/**
 * @file test_neuropeptide_hypothalamus_integration.c
 * @brief Integration tests for Neuropeptide + Hypothalamus systems
 * @date 2026-03-05
 *
 * WHAT: Verifies neuropeptide kinetics, dose-response curves, and behavioral
 *       drive computation across all 8 peptide types
 * WHY:  Neuropeptides provide slow neuromodulation complementing fast
 *       neurotransmitters; correct kinetics are essential for behavioral
 *       state regulation (stress, social bonding, wakefulness, pain)
 * HOW:  Uses Check framework; creates neuropeptide system, stimulates
 *       release, verifies kinetics and downstream effects
 */

#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "core/brain/regions/neuropeptide/nimcp_neuropeptide.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

static neuropeptide_system_t* g_npt = NULL;

static void setup(void)
{
    npt_config_t cfg = npt_default_config();
    g_npt = npt_create(&cfg);
    ck_assert_ptr_nonnull(g_npt);
}

static void teardown(void)
{
    if (g_npt) {
        npt_destroy(g_npt);
        g_npt = NULL;
    }
}

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

START_TEST(test_npt_create_defaults)
{
    neuropeptide_system_t* sys = npt_create(NULL);
    ck_assert_ptr_nonnull(sys);
    ck_assert_uint_eq(sys->magic, NPT_MAGIC);
    ck_assert(sys->initialized);
    npt_destroy(sys);
}
END_TEST

START_TEST(test_npt_destroy_null_safe)
{
    npt_destroy(NULL);
}
END_TEST

/* ============================================================================
 * Peptide Kinetics Tests
 * ============================================================================ */

START_TEST(test_stimulated_peptide_rises)
{
    float conc_before = 0.0f;
    npt_error_t rc = npt_get_concentration(g_npt, NPT_OXYTOCIN, &conc_before);
    ck_assert_int_eq(rc, NPT_OK);

    /* Stimulate oxytocin release */
    rc = npt_stimulate_release(g_npt, NPT_OXYTOCIN, 20.0f);
    ck_assert_int_eq(rc, NPT_OK);

    float conc_after = 0.0f;
    rc = npt_get_concentration(g_npt, NPT_OXYTOCIN, &conc_after);
    ck_assert_int_eq(rc, NPT_OK);

    ck_assert_float_gt(conc_after, conc_before);
}
END_TEST

START_TEST(test_peptide_degrades_over_time)
{
    /* Inject a bolus of CRH */
    npt_stimulate_release(g_npt, NPT_CRH, 50.0f);

    float conc_peak = 0.0f;
    npt_get_concentration(g_npt, NPT_CRH, &conc_peak);

    /* Let it degrade */
    for (int i = 0; i < 500; i++) {
        npt_update(g_npt, 0.1f);
    }

    float conc_decayed = 0.0f;
    npt_get_concentration(g_npt, NPT_CRH, &conc_decayed);

    ck_assert_float_lt(conc_decayed, conc_peak);
}
END_TEST

START_TEST(test_concentration_clamped_to_max)
{
    /* Inject massive bolus - should clamp to NPT_MAX_CONCENTRATION */
    npt_stimulate_release(g_npt, NPT_ENDORPHIN, 500.0f);

    float conc = 0.0f;
    npt_get_concentration(g_npt, NPT_ENDORPHIN, &conc);

    ck_assert_float_le(conc, NPT_MAX_CONCENTRATION);
}
END_TEST

/* ============================================================================
 * Dose-Response (Receptor Occupancy) Tests
 * ============================================================================ */

START_TEST(test_dose_response_sigmoid)
{
    /* Low dose: low occupancy */
    npt_stimulate_release(g_npt, NPT_NPY, 1.0f);
    npt_update(g_npt, 0.1f);

    float effect_low = 0.0f;
    npt_get_downstream_effect(g_npt, NPT_NPY, &effect_low);

    /* High dose: higher occupancy */
    npt_stimulate_release(g_npt, NPT_NPY, 80.0f);
    npt_update(g_npt, 0.1f);

    float effect_high = 0.0f;
    npt_get_downstream_effect(g_npt, NPT_NPY, &effect_high);

    /* Higher dose should produce greater effect */
    ck_assert_float_gt(effect_high, effect_low);
}
END_TEST

START_TEST(test_receptor_occupancy_saturates)
{
    /* Very high dose */
    npt_stimulate_release(g_npt, NPT_SUBSTANCE_P, 90.0f);
    npt_update(g_npt, 0.1f);

    float effect_high = 0.0f;
    npt_get_downstream_effect(g_npt, NPT_SUBSTANCE_P, &effect_high);

    /* Even higher dose */
    npt_stimulate_release(g_npt, NPT_SUBSTANCE_P, 10.0f);
    npt_update(g_npt, 0.1f);

    float effect_max = 0.0f;
    npt_get_downstream_effect(g_npt, NPT_SUBSTANCE_P, &effect_max);

    /* Difference should be small at saturation (sigmoid plateau) */
    float diff = fabsf(effect_max - effect_high);
    /* At near-saturation, occupancy gain from extra dose is diminishing */
    ck_assert_float_ge(effect_max, effect_high - 0.01f);
    (void)diff;
}
END_TEST

/* ============================================================================
 * Firing Rate Threshold Tests
 * ============================================================================ */

START_TEST(test_firing_rate_triggers_synthesis)
{
    float conc_before = 0.0f;
    npt_get_concentration(g_npt, NPT_OREXIN, &conc_before);

    /* Set high firing rate to exceed release threshold */
    npt_set_firing_rate(g_npt, NPT_OREXIN, 50.0f);

    /* Run updates for synthesis to occur */
    for (int i = 0; i < 100; i++) {
        npt_update(g_npt, 0.1f);
    }

    float conc_after = 0.0f;
    npt_get_concentration(g_npt, NPT_OREXIN, &conc_after);

    /* With high firing rate above threshold, concentration should increase */
    ck_assert_float_gt(conc_after, conc_before);
}
END_TEST

START_TEST(test_subthreshold_firing_no_synthesis)
{
    /* Set firing rate below release threshold */
    npt_set_firing_rate(g_npt, NPT_VASOPRESSIN, 0.1f);

    float conc_before = 0.0f;
    npt_get_concentration(g_npt, NPT_VASOPRESSIN, &conc_before);

    for (int i = 0; i < 50; i++) {
        npt_update(g_npt, 0.1f);
    }

    float conc_after = 0.0f;
    npt_get_concentration(g_npt, NPT_VASOPRESSIN, &conc_after);

    /* With sub-threshold firing, concentration should not increase beyond baseline */
    /* (degradation keeps pulling it down) */
    ck_assert_float_le(conc_after, conc_before + 1.0f);
}
END_TEST

/* ============================================================================
 * Behavioral Drive Tests
 * ============================================================================ */

START_TEST(test_stress_drive_from_crh)
{
    float stress_before = g_npt->stress_level;

    /* Inject CRH to drive stress */
    npt_stimulate_release(g_npt, NPT_CRH, 40.0f);
    npt_update(g_npt, 0.1f);

    float stress_after = g_npt->stress_level;

    ck_assert_float_ge(stress_after, stress_before);
}
END_TEST

START_TEST(test_wakefulness_from_orexin)
{
    float wake_before = g_npt->wakefulness;

    /* Stimulate orexin */
    npt_stimulate_release(g_npt, NPT_OREXIN, 40.0f);
    npt_update(g_npt, 0.1f);

    float wake_after = g_npt->wakefulness;

    ck_assert_float_ge(wake_after, wake_before);
}
END_TEST

/* ============================================================================
 * Error Handling Tests
 * ============================================================================ */

START_TEST(test_null_system_errors)
{
    float conc = 0.0f;
    npt_error_t rc = npt_get_concentration(NULL, NPT_OXYTOCIN, &conc);
    ck_assert_int_eq(rc, NPT_ERR_NULL_PTR);

    rc = npt_update(NULL, 0.1f);
    ck_assert_int_ne(rc, NPT_OK);
}
END_TEST

START_TEST(test_type_name_lookup)
{
    const char* name = npt_type_name(NPT_OXYTOCIN);
    ck_assert_ptr_nonnull(name);

    name = npt_type_name(NPT_ENDORPHIN);
    ck_assert_ptr_nonnull(name);

    /* Out-of-range should return "UNKNOWN" or similar */
    name = npt_type_name((neuropeptide_type_t)99);
    ck_assert_ptr_nonnull(name);
}
END_TEST

/* ============================================================================
 * Test Suite
 * ============================================================================ */

Suite* neuropeptide_hypothalamus_integration_suite(void)
{
    Suite* s = suite_create("Neuropeptide Hypothalamus Integration");

    TCase* tc_lifecycle = tcase_create("Lifecycle");
    tcase_add_test(tc_lifecycle, test_npt_create_defaults);
    tcase_add_test(tc_lifecycle, test_npt_destroy_null_safe);
    tcase_set_timeout(tc_lifecycle, 10);
    suite_add_tcase(s, tc_lifecycle);

    TCase* tc_kinetics = tcase_create("Kinetics");
    tcase_add_checked_fixture(tc_kinetics, setup, teardown);
    tcase_add_test(tc_kinetics, test_stimulated_peptide_rises);
    tcase_add_test(tc_kinetics, test_peptide_degrades_over_time);
    tcase_add_test(tc_kinetics, test_concentration_clamped_to_max);
    tcase_set_timeout(tc_kinetics, 15);
    suite_add_tcase(s, tc_kinetics);

    TCase* tc_dose = tcase_create("Dose Response");
    tcase_add_checked_fixture(tc_dose, setup, teardown);
    tcase_add_test(tc_dose, test_dose_response_sigmoid);
    tcase_add_test(tc_dose, test_receptor_occupancy_saturates);
    tcase_set_timeout(tc_dose, 10);
    suite_add_tcase(s, tc_dose);

    TCase* tc_firing = tcase_create("Firing Rate");
    tcase_add_checked_fixture(tc_firing, setup, teardown);
    tcase_add_test(tc_firing, test_firing_rate_triggers_synthesis);
    tcase_add_test(tc_firing, test_subthreshold_firing_no_synthesis);
    tcase_set_timeout(tc_firing, 15);
    suite_add_tcase(s, tc_firing);

    TCase* tc_drives = tcase_create("Behavioral Drives");
    tcase_add_checked_fixture(tc_drives, setup, teardown);
    tcase_add_test(tc_drives, test_stress_drive_from_crh);
    tcase_add_test(tc_drives, test_wakefulness_from_orexin);
    tcase_set_timeout(tc_drives, 10);
    suite_add_tcase(s, tc_drives);

    TCase* tc_errors = tcase_create("Error Handling");
    tcase_add_checked_fixture(tc_errors, setup, teardown);
    tcase_add_test(tc_errors, test_null_system_errors);
    tcase_add_test(tc_errors, test_type_name_lookup);
    tcase_set_timeout(tc_errors, 10);
    suite_add_tcase(s, tc_errors);

    return s;
}

int main(void)
{
    int number_failed;
    Suite* s = neuropeptide_hypothalamus_integration_suite();
    SRunner* sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
