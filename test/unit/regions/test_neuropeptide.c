/**
 * @file test_neuropeptide.c
 * @brief Unit tests for neuropeptide system
 *
 * WHAT: Test suite for neuropeptide system API
 * WHY:  Verify correct behavior of lifecycle, kinetic simulation,
 *       stimulation, firing rate, and query functions for all 8 peptides
 * HOW:  Unit tests using Check framework covering all neuropeptide functions
 *
 * @author NIMCP Development Team
 * @date 2026-03-05
 */

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "core/brain/regions/neuropeptide/nimcp_neuropeptide.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

static neuropeptide_system_t* g_system = NULL;

static void setup(void)
{
    npt_config_t config = npt_default_config();
    g_system = npt_create(&config);
    ck_assert_ptr_nonnull(g_system);
}

static void teardown(void)
{
    if (g_system) {
        npt_destroy(g_system);
        g_system = NULL;
    }
}

/* ============================================================================
 * Default Config Tests
 * ============================================================================ */

START_TEST(test_default_config_values)
{
    npt_config_t config = npt_default_config();
    for (int i = 0; i < NPT_COUNT; i++) {
        ck_assert_float_gt(config.base_synthesis_rates[i], 0.0f);
        ck_assert_float_gt(config.degradation_rates[i], 0.0f);
        ck_assert_float_gt(config.release_thresholds[i], 0.0f);
        ck_assert_float_gt(config.kd_values[i], 0.0f);
        ck_assert_float_gt(config.gains[i], 0.0f);
    }
}
END_TEST

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

START_TEST(test_create_with_defaults)
{
    neuropeptide_system_t* sys = npt_create(NULL);
    ck_assert_ptr_nonnull(sys);
    ck_assert_uint_eq(sys->magic, NPT_MAGIC);
    ck_assert(sys->initialized);
    npt_destroy(sys);
}
END_TEST

START_TEST(test_create_with_config)
{
    npt_config_t config = npt_default_config();
    config.gains[NPT_OXYTOCIN] = 2.5f;

    neuropeptide_system_t* sys = npt_create(&config);
    ck_assert_ptr_nonnull(sys);
    ck_assert_uint_eq(sys->magic, NPT_MAGIC);
    npt_destroy(sys);
}
END_TEST

START_TEST(test_destroy_null)
{
    /* Should not crash */
    npt_destroy(NULL);
}
END_TEST

START_TEST(test_initial_state)
{
    ck_assert(g_system->initialized);
    ck_assert_uint_eq(g_system->update_count, 0);
    /* All peptide concentrations should start at 0 or near-baseline */
    for (int i = 0; i < NPT_COUNT; i++) {
        ck_assert_float_ge(g_system->peptides[i].concentration, 0.0f);
        ck_assert_float_le(g_system->peptides[i].concentration, NPT_MAX_CONCENTRATION);
    }
}
END_TEST

/* ============================================================================
 * Update Tests
 * ============================================================================ */

START_TEST(test_update_positive_dt)
{
    npt_error_t result = npt_update(g_system, 0.1f);
    ck_assert_int_eq(result, NPT_OK);
    ck_assert_uint_eq(g_system->update_count, 1);
}
END_TEST

START_TEST(test_update_zero_dt)
{
    npt_error_t result = npt_update(g_system, 0.0f);
    /* Zero dt is rejected as invalid (dt must be positive) */
    ck_assert_int_eq(result, NPT_ERR_INVALID_PARAM);
}
END_TEST

START_TEST(test_update_negative_dt)
{
    npt_error_t result = npt_update(g_system, -1.0f);
    ck_assert_int_eq(result, NPT_ERR_INVALID_PARAM);
}
END_TEST

START_TEST(test_update_null_system)
{
    npt_error_t result = npt_update(NULL, 0.1f);
    ck_assert_int_eq(result, NPT_ERR_NULL_PTR);
}
END_TEST

/* ============================================================================
 * Concentration Tests
 * ============================================================================ */

START_TEST(test_get_concentration_all_types)
{
    float conc = 0.0f;
    for (int i = 0; i < NPT_COUNT; i++) {
        npt_error_t result = npt_get_concentration(g_system, (neuropeptide_type_t)i, &conc);
        ck_assert_int_eq(result, NPT_OK);
        ck_assert_float_ge(conc, 0.0f);
        ck_assert_float_le(conc, NPT_MAX_CONCENTRATION);
    }
}
END_TEST

START_TEST(test_get_concentration_null_system)
{
    float conc = 0.0f;
    npt_error_t result = npt_get_concentration(NULL, NPT_OXYTOCIN, &conc);
    ck_assert_int_eq(result, NPT_ERR_NULL_PTR);
}
END_TEST

START_TEST(test_get_concentration_null_output)
{
    npt_error_t result = npt_get_concentration(g_system, NPT_OXYTOCIN, NULL);
    ck_assert_int_eq(result, NPT_ERR_NULL_PTR);
}
END_TEST

START_TEST(test_get_concentration_invalid_type)
{
    float conc = 0.0f;
    npt_error_t result = npt_get_concentration(g_system, NPT_COUNT, &conc);
    ck_assert_int_eq(result, NPT_ERR_INVALID_TYPE);
}
END_TEST

/* ============================================================================
 * Downstream Effect Tests
 * ============================================================================ */

START_TEST(test_get_downstream_effect)
{
    float effect = 0.0f;
    npt_error_t result = npt_get_downstream_effect(g_system, NPT_ENDORPHIN, &effect);
    ck_assert_int_eq(result, NPT_OK);
    ck_assert(!isnan(effect));
}
END_TEST

START_TEST(test_get_downstream_effect_null)
{
    float effect = 0.0f;
    npt_error_t result = npt_get_downstream_effect(NULL, NPT_ENDORPHIN, &effect);
    ck_assert_int_eq(result, NPT_ERR_NULL_PTR);
}
END_TEST

/* ============================================================================
 * Stimulation Tests
 * ============================================================================ */

START_TEST(test_stimulate_release)
{
    float conc_before = 0.0f;
    npt_get_concentration(g_system, NPT_OXYTOCIN, &conc_before);

    npt_error_t result = npt_stimulate_release(g_system, NPT_OXYTOCIN, 10.0f);
    ck_assert_int_eq(result, NPT_OK);

    float conc_after = 0.0f;
    npt_get_concentration(g_system, NPT_OXYTOCIN, &conc_after);
    ck_assert_float_gt(conc_after, conc_before);
}
END_TEST

START_TEST(test_stimulate_release_null)
{
    npt_error_t result = npt_stimulate_release(NULL, NPT_OXYTOCIN, 5.0f);
    ck_assert_int_eq(result, NPT_ERR_NULL_PTR);
}
END_TEST

START_TEST(test_stimulate_release_invalid_type)
{
    npt_error_t result = npt_stimulate_release(g_system, NPT_COUNT, 5.0f);
    ck_assert_int_eq(result, NPT_ERR_INVALID_TYPE);
}
END_TEST

/* ============================================================================
 * Firing Rate Tests
 * ============================================================================ */

START_TEST(test_set_firing_rate)
{
    npt_error_t result = npt_set_firing_rate(g_system, NPT_OREXIN, 30.0f);
    ck_assert_int_eq(result, NPT_OK);
}
END_TEST

START_TEST(test_set_firing_rate_null)
{
    npt_error_t result = npt_set_firing_rate(NULL, NPT_OREXIN, 30.0f);
    ck_assert_int_eq(result, NPT_ERR_NULL_PTR);
}
END_TEST

START_TEST(test_firing_above_threshold_increases_concentration)
{
    /* Set firing rate above release threshold for orexin */
    npt_set_firing_rate(g_system, NPT_OREXIN, 100.0f);

    float conc_before = 0.0f;
    npt_get_concentration(g_system, NPT_OREXIN, &conc_before);

    /* Update several times */
    for (int i = 0; i < 100; i++) {
        npt_update(g_system, 0.1f);
    }

    float conc_after = 0.0f;
    npt_get_concentration(g_system, NPT_OREXIN, &conc_after);
    ck_assert_float_gt(conc_after, conc_before);
}
END_TEST

/* ============================================================================
 * Query Tests
 * ============================================================================ */

START_TEST(test_get_all_states)
{
    neuropeptide_state_t states[NPT_COUNT];
    memset(states, 0, sizeof(states));

    npt_error_t result = npt_get_all_states(g_system, states);
    ck_assert_int_eq(result, NPT_OK);

    for (int i = 0; i < NPT_COUNT; i++) {
        ck_assert_float_ge(states[i].concentration, 0.0f);
    }
}
END_TEST

START_TEST(test_get_all_states_null)
{
    neuropeptide_state_t states[NPT_COUNT];
    npt_error_t result = npt_get_all_states(NULL, states);
    ck_assert_int_eq(result, NPT_ERR_NULL_PTR);
}
END_TEST

START_TEST(test_type_name)
{
    ck_assert_str_eq(npt_type_name(NPT_OXYTOCIN), "Oxytocin");
    ck_assert_str_eq(npt_type_name(NPT_VASOPRESSIN), "Vasopressin");
    ck_assert_str_eq(npt_type_name(NPT_NPY), "NPY");
    ck_assert_str_eq(npt_type_name(NPT_SUBSTANCE_P), "Substance P");
    ck_assert_str_eq(npt_type_name(NPT_OREXIN), "Orexin");
    ck_assert_str_eq(npt_type_name(NPT_CRH), "CRH");
    ck_assert_str_eq(npt_type_name(NPT_ENDORPHIN), "Endorphin");
    ck_assert_str_eq(npt_type_name(NPT_CCK), "CCK");
}
END_TEST

START_TEST(test_type_name_invalid)
{
    const char* name = npt_type_name(NPT_COUNT);
    ck_assert_str_eq(name, "UNKNOWN");
}
END_TEST

START_TEST(test_error_string)
{
    const char* s = npt_error_string(NPT_OK);
    ck_assert_ptr_nonnull(s);
    s = npt_error_string(NPT_ERR_NULL_PTR);
    ck_assert_ptr_nonnull(s);
}
END_TEST

/* ============================================================================
 * Test Suite Setup
 * ============================================================================ */

Suite* neuropeptide_suite(void)
{
    Suite* s = suite_create("Neuropeptide");

    /* Config tests */
    TCase* tc_config = tcase_create("Configuration");
    tcase_add_test(tc_config, test_default_config_values);
    suite_add_tcase(s, tc_config);

    /* Lifecycle tests */
    TCase* tc_lifecycle = tcase_create("Lifecycle");
    tcase_add_test(tc_lifecycle, test_create_with_defaults);
    tcase_add_test(tc_lifecycle, test_create_with_config);
    tcase_add_test(tc_lifecycle, test_destroy_null);
    tcase_add_checked_fixture(tc_lifecycle, setup, teardown);
    tcase_add_test(tc_lifecycle, test_initial_state);
    suite_add_tcase(s, tc_lifecycle);

    /* Update tests */
    TCase* tc_update = tcase_create("Update");
    tcase_add_checked_fixture(tc_update, setup, teardown);
    tcase_add_test(tc_update, test_update_positive_dt);
    tcase_add_test(tc_update, test_update_zero_dt);
    tcase_add_test(tc_update, test_update_negative_dt);
    tcase_add_test(tc_update, test_update_null_system);
    suite_add_tcase(s, tc_update);

    /* Concentration tests */
    TCase* tc_conc = tcase_create("Concentration");
    tcase_add_checked_fixture(tc_conc, setup, teardown);
    tcase_add_test(tc_conc, test_get_concentration_all_types);
    tcase_add_test(tc_conc, test_get_concentration_null_system);
    tcase_add_test(tc_conc, test_get_concentration_null_output);
    tcase_add_test(tc_conc, test_get_concentration_invalid_type);
    suite_add_tcase(s, tc_conc);

    /* Downstream effect tests */
    TCase* tc_effect = tcase_create("Downstream Effect");
    tcase_add_checked_fixture(tc_effect, setup, teardown);
    tcase_add_test(tc_effect, test_get_downstream_effect);
    tcase_add_test(tc_effect, test_get_downstream_effect_null);
    suite_add_tcase(s, tc_effect);

    /* Stimulation tests */
    TCase* tc_stim = tcase_create("Stimulation");
    tcase_add_checked_fixture(tc_stim, setup, teardown);
    tcase_add_test(tc_stim, test_stimulate_release);
    tcase_add_test(tc_stim, test_stimulate_release_null);
    tcase_add_test(tc_stim, test_stimulate_release_invalid_type);
    suite_add_tcase(s, tc_stim);

    /* Firing rate tests */
    TCase* tc_fire = tcase_create("Firing Rate");
    tcase_add_checked_fixture(tc_fire, setup, teardown);
    tcase_add_test(tc_fire, test_set_firing_rate);
    tcase_add_test(tc_fire, test_set_firing_rate_null);
    tcase_add_test(tc_fire, test_firing_above_threshold_increases_concentration);
    suite_add_tcase(s, tc_fire);

    /* Query tests */
    TCase* tc_query = tcase_create("Query");
    tcase_add_checked_fixture(tc_query, setup, teardown);
    tcase_add_test(tc_query, test_get_all_states);
    tcase_add_test(tc_query, test_get_all_states_null);
    tcase_add_test(tc_query, test_type_name);
    tcase_add_test(tc_query, test_type_name_invalid);
    tcase_add_test(tc_query, test_error_string);
    suite_add_tcase(s, tc_query);

    return s;
}

int main(void)
{
    int number_failed;
    Suite* s = neuropeptide_suite();
    SRunner* sr = srunner_create(s);

    srunner_set_fork_status(sr, CK_NOFORK);
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
