/**
 * @file test_endocannabinoid.c
 * @brief Unit tests for endocannabinoid system (ECS)
 *
 * WHAT: Test suite for endocannabinoid system API
 * WHY:  Verify correct behavior of lifecycle, synaptic modulation,
 *       retrograde signaling, and pain modulation
 * HOW:  Unit tests using Check framework covering all ECS functions
 *
 * @author NIMCP Development Team
 * @date 2026-03-05
 */

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "core/brain/regions/endocannabinoid/nimcp_endocannabinoid.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

static endocannabinoid_system_t* g_system = NULL;

static void setup(void)
{
    ecb_config_t config = ecb_default_config();
    g_system = ecb_create(&config);
    ck_assert_ptr_nonnull(g_system);
}

static void teardown(void)
{
    if (g_system) {
        ecb_destroy(g_system);
        g_system = NULL;
    }
}

/* ============================================================================
 * Default Config Tests
 * ============================================================================ */

START_TEST(test_default_config_values)
{
    ecb_config_t config = ecb_default_config();
    ck_assert_float_ge(config.base_two_ag, 0.0f);
    ck_assert_float_le(config.base_two_ag, 1.0f);
    ck_assert_float_ge(config.base_aea, 0.0f);
    ck_assert_float_le(config.base_aea, 1.0f);
    ck_assert_float_gt(config.magl_rate, 0.0f);
    ck_assert_float_gt(config.faah_rate, 0.0f);
    ck_assert_float_gt(config.cb1_gain, 0.0f);
    ck_assert_float_gt(config.cb2_gain, 0.0f);
}
END_TEST

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

START_TEST(test_create_with_defaults)
{
    endocannabinoid_system_t* sys = ecb_create(NULL);
    ck_assert_ptr_nonnull(sys);
    ck_assert_uint_eq(sys->magic, ECB_MAGIC);
    ecb_destroy(sys);
}
END_TEST

START_TEST(test_create_with_config)
{
    ecb_config_t config = ecb_default_config();
    config.cb1_gain = 2.0f;
    config.cb2_gain = 0.5f;

    endocannabinoid_system_t* sys = ecb_create(&config);
    ck_assert_ptr_nonnull(sys);
    ck_assert_uint_eq(sys->magic, ECB_MAGIC);
    ck_assert_float_eq(sys->config.cb1_gain, 2.0f);
    ck_assert_float_eq(sys->config.cb2_gain, 0.5f);
    ecb_destroy(sys);
}
END_TEST

START_TEST(test_destroy_null)
{
    /* Should not crash */
    ecb_destroy(NULL);
}
END_TEST

START_TEST(test_initial_state)
{
    ck_assert_float_ge(g_system->two_ag_level, 0.0f);
    ck_assert_float_ge(g_system->aea_level, 0.0f);
    ck_assert_float_ge(g_system->dsi_strength, 0.0f);
    ck_assert_float_ge(g_system->dse_strength, 0.0f);
}
END_TEST

/* ============================================================================
 * Update Tests
 * ============================================================================ */

START_TEST(test_update_positive_dt)
{
    int result = ecb_update(g_system, 0.01f);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_update_zero_dt)
{
    int result = ecb_update(g_system, 0.0f);
    /* Zero dt is rejected as invalid (dt must be positive) */
    ck_assert_int_ne(result, 0);
}
END_TEST

START_TEST(test_update_negative_dt)
{
    int result = ecb_update(g_system, -1.0f);
    ck_assert_int_eq(result, -1);
}
END_TEST

START_TEST(test_update_null_system)
{
    int result = ecb_update(NULL, 0.01f);
    ck_assert_int_eq(result, -1);
}
END_TEST

/* ============================================================================
 * Synaptic Modulation Tests
 * ============================================================================ */

START_TEST(test_postsynaptic_depolarization)
{
    int result = ecb_on_postsynaptic_depolarization(g_system, 0, 0.8f);
    ck_assert_int_eq(result, 0);
    /* Depolarization should increase the accumulator */
    ck_assert_float_gt(g_system->depolarization_accumulator, 0.0f);
}
END_TEST

START_TEST(test_postsynaptic_depolarization_null)
{
    int result = ecb_on_postsynaptic_depolarization(NULL, 0, 0.5f);
    ck_assert_int_eq(result, -1);
}
END_TEST

START_TEST(test_presynaptic_suppression)
{
    float suppression = ecb_get_presynaptic_suppression(g_system, 0);
    ck_assert_float_ge(suppression, 0.0f);
    ck_assert_float_le(suppression, 1.0f);
}
END_TEST

START_TEST(test_presynaptic_suppression_after_depolarization)
{
    /* Drive 2-AG synthesis via depolarization */
    for (int i = 0; i < 50; i++) {
        ecb_on_postsynaptic_depolarization(g_system, 0, 1.0f);
        ecb_update(g_system, 0.01f);
    }
    float suppression = ecb_get_presynaptic_suppression(g_system, 0);
    ck_assert_float_ge(suppression, 0.0f);
    ck_assert_float_le(suppression, 1.0f);
}
END_TEST

/* ============================================================================
 * Retrograde Signal Tests
 * ============================================================================ */

START_TEST(test_retrograde_signal_2ag)
{
    float signal = ecb_get_retrograde_signal(g_system, ECB_2AG);
    ck_assert_float_ge(signal, 0.0f);
    ck_assert_float_le(signal, 1.0f);
}
END_TEST

START_TEST(test_retrograde_signal_aea)
{
    float signal = ecb_get_retrograde_signal(g_system, ECB_AEA);
    ck_assert_float_ge(signal, 0.0f);
    ck_assert_float_le(signal, 1.0f);
}
END_TEST

START_TEST(test_retrograde_signal_invalid_type)
{
    float signal = ecb_get_retrograde_signal(g_system, ECB_TYPE_COUNT);
    ck_assert_float_eq(signal, -1.0f);
}
END_TEST

START_TEST(test_retrograde_signal_null)
{
    float signal = ecb_get_retrograde_signal(NULL, ECB_2AG);
    ck_assert_float_eq(signal, -1.0f);
}
END_TEST

/* ============================================================================
 * Pain Modulation Tests
 * ============================================================================ */

START_TEST(test_modulate_pain_basic)
{
    float modulated = 0.0f;
    int result = ecb_modulate_pain(g_system, 0.8f, &modulated);
    ck_assert_int_eq(result, 0);
    ck_assert_float_ge(modulated, 0.0f);
    ck_assert_float_le(modulated, 1.0f);
    /* ECS should attenuate pain, modulated <= original */
    ck_assert_float_le(modulated, 0.8f + 0.01f);
}
END_TEST

START_TEST(test_modulate_pain_null_system)
{
    float modulated = 0.0f;
    int result = ecb_modulate_pain(NULL, 0.5f, &modulated);
    ck_assert_int_eq(result, -1);
}
END_TEST

START_TEST(test_modulate_pain_null_output)
{
    int result = ecb_modulate_pain(g_system, 0.5f, NULL);
    ck_assert_int_eq(result, -1);
}
END_TEST

START_TEST(test_modulate_pain_zero_signal)
{
    float modulated = -1.0f;
    int result = ecb_modulate_pain(g_system, 0.0f, &modulated);
    ck_assert_int_eq(result, 0);
    ck_assert_float_eq_tol(modulated, 0.0f, 1e-5f);
}
END_TEST

/* ============================================================================
 * Test Suite Setup
 * ============================================================================ */

Suite* endocannabinoid_suite(void)
{
    Suite* s = suite_create("Endocannabinoid");

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

    /* Synaptic modulation tests */
    TCase* tc_synapse = tcase_create("Synaptic Modulation");
    tcase_add_checked_fixture(tc_synapse, setup, teardown);
    tcase_add_test(tc_synapse, test_postsynaptic_depolarization);
    tcase_add_test(tc_synapse, test_postsynaptic_depolarization_null);
    tcase_add_test(tc_synapse, test_presynaptic_suppression);
    tcase_add_test(tc_synapse, test_presynaptic_suppression_after_depolarization);
    suite_add_tcase(s, tc_synapse);

    /* Retrograde signal tests */
    TCase* tc_retro = tcase_create("Retrograde Signal");
    tcase_add_checked_fixture(tc_retro, setup, teardown);
    tcase_add_test(tc_retro, test_retrograde_signal_2ag);
    tcase_add_test(tc_retro, test_retrograde_signal_aea);
    tcase_add_test(tc_retro, test_retrograde_signal_invalid_type);
    tcase_add_test(tc_retro, test_retrograde_signal_null);
    suite_add_tcase(s, tc_retro);

    /* Pain modulation tests */
    TCase* tc_pain = tcase_create("Pain Modulation");
    tcase_add_checked_fixture(tc_pain, setup, teardown);
    tcase_add_test(tc_pain, test_modulate_pain_basic);
    tcase_add_test(tc_pain, test_modulate_pain_null_system);
    tcase_add_test(tc_pain, test_modulate_pain_null_output);
    tcase_add_test(tc_pain, test_modulate_pain_zero_signal);
    suite_add_tcase(s, tc_pain);

    return s;
}

int main(void)
{
    int number_failed;
    Suite* s = endocannabinoid_suite();
    SRunner* sr = srunner_create(s);

    srunner_set_fork_status(sr, CK_NOFORK);
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
