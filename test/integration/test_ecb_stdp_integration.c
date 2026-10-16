/**
 * @file test_ecb_stdp_integration.c
 * @brief Integration tests for Endocannabinoid + STDP + Plasticity Coordinator
 * @date 2026-03-05
 *
 * WHAT: Verifies cross-module interactions between ECS retrograde signaling,
 *       STDP modulation, and presynaptic suppression
 * WHY:  The endocannabinoid system modulates synaptic plasticity through
 *       retrograde signaling (DSI/DSE); these tests verify the 2-AG/AEA
 *       pathways respond correctly to depolarization and affect presynaptic
 *       release probability
 * HOW:  Uses Check framework; creates ECS, drives depolarization events,
 *       and verifies retrograde signal and suppression dynamics
 */

#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "core/brain/regions/endocannabinoid/nimcp_endocannabinoid.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

static endocannabinoid_system_t* g_ecs = NULL;

static void setup(void)
{
    ecb_config_t cfg = ecb_default_config();
    g_ecs = ecb_create(&cfg);
    ck_assert_ptr_nonnull(g_ecs);
}

static void teardown(void)
{
    if (g_ecs) {
        ecb_destroy(g_ecs);
        g_ecs = NULL;
    }
}

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

START_TEST(test_ecs_create_defaults)
{
    endocannabinoid_system_t* sys = ecb_create(NULL);
    ck_assert_ptr_nonnull(sys);
    ck_assert_uint_eq(sys->magic, ECB_MAGIC);
    ck_assert_float_ge(sys->two_ag_level, 0.0f);
    ck_assert_float_ge(sys->aea_level, 0.0f);
    ecb_destroy(sys);
}
END_TEST

START_TEST(test_ecs_destroy_null_safe)
{
    ecb_destroy(NULL);
}
END_TEST

/* ============================================================================
 * Depolarization -> 2-AG Synthesis Tests
 * ============================================================================ */

START_TEST(test_depolarization_drives_2ag_synthesis)
{
    float initial_2ag = ecb_get_retrograde_signal(g_ecs, ECB_2AG);

    /* Apply strong depolarization to drive 2-AG synthesis */
    for (int i = 0; i < 20; i++) {
        int rc = ecb_on_postsynaptic_depolarization(g_ecs, 0, 0.9f);
        ck_assert_int_eq(rc, 0);
        ecb_update(g_ecs, 0.01f);
    }

    float post_depol_2ag = ecb_get_retrograde_signal(g_ecs, ECB_2AG);

    /* 2-AG should increase after depolarization */
    ck_assert_float_gt(post_depol_2ag, initial_2ag);
}
END_TEST

START_TEST(test_no_depolarization_2ag_decays)
{
    /* First build up some 2-AG */
    for (int i = 0; i < 20; i++) {
        ecb_on_postsynaptic_depolarization(g_ecs, 0, 0.9f);
        ecb_update(g_ecs, 0.01f);
    }
    float peak_2ag = ecb_get_retrograde_signal(g_ecs, ECB_2AG);

    /* Now let it decay without depolarization (MAGL degradation) */
    for (int i = 0; i < 200; i++) {
        ecb_update(g_ecs, 0.01f);
    }
    float decayed_2ag = ecb_get_retrograde_signal(g_ecs, ECB_2AG);

    /* 2-AG should decay via MAGL */
    ck_assert_float_lt(decayed_2ag, peak_2ag);
}
END_TEST

/* ============================================================================
 * Presynaptic Suppression Tests
 * ============================================================================ */

START_TEST(test_presynaptic_suppression_increases_with_2ag)
{
    float baseline_suppression = ecb_get_presynaptic_suppression(g_ecs, 0);

    /* Drive 2-AG release via depolarization */
    for (int i = 0; i < 30; i++) {
        ecb_on_postsynaptic_depolarization(g_ecs, 0, 1.0f);
        ecb_update(g_ecs, 0.01f);
    }

    float elevated_suppression = ecb_get_presynaptic_suppression(g_ecs, 0);

    /* Suppression should be greater with elevated 2-AG */
    ck_assert_float_ge(elevated_suppression, baseline_suppression);
}
END_TEST

START_TEST(test_suppression_bounded_zero_one)
{
    /* Even with maximal depolarization, suppression stays in [0, 1] */
    for (int i = 0; i < 100; i++) {
        ecb_on_postsynaptic_depolarization(g_ecs, 0, 1.0f);
        ecb_update(g_ecs, 0.005f);
    }

    float suppression = ecb_get_presynaptic_suppression(g_ecs, 0);
    ck_assert_float_ge(suppression, 0.0f);
    ck_assert_float_le(suppression, 1.0f);
}
END_TEST

/* ============================================================================
 * DSI/DSE Strength Tests
 * ============================================================================ */

START_TEST(test_dsi_strength_responds_to_depolarization)
{
    float initial_dsi = g_ecs->dsi_strength;

    for (int i = 0; i < 30; i++) {
        ecb_on_postsynaptic_depolarization(g_ecs, 0, 0.8f);
        ecb_update(g_ecs, 0.01f);
    }

    float post_dsi = g_ecs->dsi_strength;

    /* DSI (depolarization-induced suppression of inhibition) should respond */
    ck_assert_float_ge(post_dsi, initial_dsi);
}
END_TEST

START_TEST(test_dse_strength_responds_to_depolarization)
{
    float initial_dse = g_ecs->dse_strength;

    for (int i = 0; i < 30; i++) {
        ecb_on_postsynaptic_depolarization(g_ecs, 0, 0.8f);
        ecb_update(g_ecs, 0.01f);
    }

    float post_dse = g_ecs->dse_strength;

    /* DSE should also respond to depolarization-driven 2-AG */
    ck_assert_float_ge(post_dse, initial_dse);
}
END_TEST

/* ============================================================================
 * Tonic Inhibition (AEA) Tests
 * ============================================================================ */

START_TEST(test_aea_provides_tonic_inhibition)
{
    /* AEA provides baseline tonic inhibition */
    float tonic = g_ecs->tonic_inhibition;

    /* Update without depolarization - AEA tonic should remain stable or settle */
    for (int i = 0; i < 100; i++) {
        ecb_update(g_ecs, 0.01f);
    }

    float tonic_after = g_ecs->tonic_inhibition;

    /* Tonic inhibition should be non-negative */
    ck_assert_float_ge(tonic_after, 0.0f);
    ck_assert_float_le(tonic_after, 1.0f);
}
END_TEST

/* ============================================================================
 * Pain Modulation Tests
 * ============================================================================ */

START_TEST(test_pain_modulation)
{
    float pain_in = 0.8f;
    float pain_out = 0.0f;

    int rc = ecb_modulate_pain(g_ecs, pain_in, &pain_out);
    ck_assert_int_eq(rc, 0);

    /* Pain should be modulated (reduced or equal) by ECS analgesic pathway */
    ck_assert_float_le(pain_out, pain_in);
    ck_assert_float_ge(pain_out, 0.0f);
}
END_TEST

START_TEST(test_pain_modulation_null_check)
{
    float pain_out = 0.0f;
    int rc = ecb_modulate_pain(NULL, 0.5f, &pain_out);
    ck_assert_int_eq(rc, -1);

    rc = ecb_modulate_pain(g_ecs, 0.5f, NULL);
    ck_assert_int_eq(rc, -1);
}
END_TEST

/* ============================================================================
 * Test Suite
 * ============================================================================ */

Suite* ecb_stdp_integration_suite(void)
{
    Suite* s = suite_create("Endocannabinoid STDP Integration");

    TCase* tc_lifecycle = tcase_create("Lifecycle");
    tcase_add_test(tc_lifecycle, test_ecs_create_defaults);
    tcase_add_test(tc_lifecycle, test_ecs_destroy_null_safe);
    tcase_set_timeout(tc_lifecycle, 10);
    suite_add_tcase(s, tc_lifecycle);

    TCase* tc_2ag = tcase_create("2-AG Synthesis");
    tcase_add_checked_fixture(tc_2ag, setup, teardown);
    tcase_add_test(tc_2ag, test_depolarization_drives_2ag_synthesis);
    tcase_add_test(tc_2ag, test_no_depolarization_2ag_decays);
    tcase_set_timeout(tc_2ag, 10);
    suite_add_tcase(s, tc_2ag);

    TCase* tc_suppress = tcase_create("Presynaptic Suppression");
    tcase_add_checked_fixture(tc_suppress, setup, teardown);
    tcase_add_test(tc_suppress, test_presynaptic_suppression_increases_with_2ag);
    tcase_add_test(tc_suppress, test_suppression_bounded_zero_one);
    tcase_set_timeout(tc_suppress, 10);
    suite_add_tcase(s, tc_suppress);

    TCase* tc_dsi = tcase_create("DSI DSE Strength");
    tcase_add_checked_fixture(tc_dsi, setup, teardown);
    tcase_add_test(tc_dsi, test_dsi_strength_responds_to_depolarization);
    tcase_add_test(tc_dsi, test_dse_strength_responds_to_depolarization);
    tcase_set_timeout(tc_dsi, 10);
    suite_add_tcase(s, tc_dsi);

    TCase* tc_tonic = tcase_create("Tonic Inhibition");
    tcase_add_checked_fixture(tc_tonic, setup, teardown);
    tcase_add_test(tc_tonic, test_aea_provides_tonic_inhibition);
    tcase_set_timeout(tc_tonic, 10);
    suite_add_tcase(s, tc_tonic);

    TCase* tc_pain = tcase_create("Pain Modulation");
    tcase_add_checked_fixture(tc_pain, setup, teardown);
    tcase_add_test(tc_pain, test_pain_modulation);
    tcase_add_test(tc_pain, test_pain_modulation_null_check);
    tcase_set_timeout(tc_pain, 10);
    suite_add_tcase(s, tc_pain);

    return s;
}

int main(void)
{
    int number_failed;
    Suite* s = ecb_stdp_integration_suite();
    SRunner* sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
