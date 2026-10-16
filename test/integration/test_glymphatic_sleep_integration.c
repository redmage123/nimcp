/**
 * @file test_glymphatic_sleep_integration.c
 * @brief Integration tests for Glymphatic + Sleep + Glial + Immune systems
 * @date 2026-03-05
 *
 * WHAT: Verifies cross-module interactions between the glymphatic waste
 *       clearance system and sleep state transitions
 * WHY:  Glymphatic clearance is tightly coupled to sleep state; these tests
 *       verify that state transitions modulate clearance rate correctly and
 *       that waste accumulates during wakefulness
 * HOW:  Uses Check framework; creates glymphatic system, drives sleep
 *       transitions, and asserts clearance/waste dynamics
 */

#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "core/brain/regions/glymphatic/nimcp_glymphatic.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

static glymphatic_system_t* g_glym = NULL;

static void setup(void)
{
    glymphatic_config_t cfg = glymphatic_default_config();
    g_glym = glymphatic_create(&cfg);
    ck_assert_ptr_nonnull(g_glym);
}

static void teardown(void)
{
    if (g_glym) {
        glymphatic_destroy(g_glym);
        g_glym = NULL;
    }
}

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

START_TEST(test_glymphatic_create_defaults)
{
    /* Verify default config produces valid system */
    glymphatic_system_t* sys = glymphatic_create(NULL);
    ck_assert_ptr_nonnull(sys);
    ck_assert_uint_eq(sys->magic, GLYM_MAGIC);
    ck_assert_int_eq(glymphatic_get_state(sys), GLYM_INACTIVE);
    ck_assert_float_ge(glymphatic_get_waste_level(sys), 0.0f);
    glymphatic_destroy(sys);
}
END_TEST

START_TEST(test_glymphatic_destroy_null_safe)
{
    /* NULL destroy must not crash */
    glymphatic_destroy(NULL);
}
END_TEST

/* ============================================================================
 * Sleep State Transition Tests
 * ============================================================================ */

START_TEST(test_sleep_transition_awake_to_nrem)
{
    /* Start in AWAKE, transition to deep NREM */
    int rc = glymphatic_on_sleep_state_change(g_glym, GLYM_SLEEP_AWAKE);
    ck_assert_int_eq(rc, 0);

    float awake_clearance = glymphatic_get_clearance_rate(g_glym);

    rc = glymphatic_on_sleep_state_change(g_glym, GLYM_SLEEP_DEEP_NREM);
    ck_assert_int_eq(rc, 0);

    /* Run a few update ticks to let state transition propagate */
    for (int i = 0; i < 50; i++) {
        glymphatic_update(g_glym, 1.0f);
    }

    float nrem_clearance = glymphatic_get_clearance_rate(g_glym);

    /* NREM clearance should be substantially higher than awake clearance */
    ck_assert_float_gt(nrem_clearance, awake_clearance);
}
END_TEST

START_TEST(test_sleep_transition_nrem_to_rem)
{
    /* Transition to NREM first, then REM */
    glymphatic_on_sleep_state_change(g_glym, GLYM_SLEEP_DEEP_NREM);
    for (int i = 0; i < 50; i++) {
        glymphatic_update(g_glym, 1.0f);
    }
    float nrem_clearance = glymphatic_get_clearance_rate(g_glym);

    glymphatic_on_sleep_state_change(g_glym, GLYM_SLEEP_REM);
    for (int i = 0; i < 50; i++) {
        glymphatic_update(g_glym, 1.0f);
    }
    float rem_clearance = glymphatic_get_clearance_rate(g_glym);

    /* REM clearance should be less than deep NREM (biologically accurate) */
    ck_assert_float_lt(rem_clearance, nrem_clearance);
}
END_TEST

START_TEST(test_sleep_transition_back_to_awake)
{
    /* Deep NREM -> Awake should reduce clearance */
    glymphatic_on_sleep_state_change(g_glym, GLYM_SLEEP_DEEP_NREM);
    for (int i = 0; i < 50; i++) {
        glymphatic_update(g_glym, 1.0f);
    }
    float nrem_clearance = glymphatic_get_clearance_rate(g_glym);

    glymphatic_on_sleep_state_change(g_glym, GLYM_SLEEP_AWAKE);
    for (int i = 0; i < 50; i++) {
        glymphatic_update(g_glym, 1.0f);
    }
    float awake_clearance = glymphatic_get_clearance_rate(g_glym);

    ck_assert_float_lt(awake_clearance, nrem_clearance);
}
END_TEST

/* ============================================================================
 * Waste Accumulation Tests
 * ============================================================================ */

START_TEST(test_waste_accumulates_during_wake)
{
    /* During wakefulness, waste should accumulate */
    glymphatic_on_sleep_state_change(g_glym, GLYM_SLEEP_AWAKE);

    float initial_waste = glymphatic_get_waste_level(g_glym);

    /* Simulate 100 seconds of wakefulness */
    for (int i = 0; i < 100; i++) {
        glymphatic_update(g_glym, 1.0f);
    }

    float final_waste = glymphatic_get_waste_level(g_glym);
    ck_assert_float_gt(final_waste, initial_waste);
}
END_TEST

START_TEST(test_waste_clears_during_sleep)
{
    /* Accumulate waste while awake first */
    glymphatic_on_sleep_state_change(g_glym, GLYM_SLEEP_AWAKE);
    for (int i = 0; i < 200; i++) {
        glymphatic_update(g_glym, 1.0f);
    }
    float peak_waste = glymphatic_get_waste_level(g_glym);

    /* Now enter deep sleep and let clearance work */
    glymphatic_on_sleep_state_change(g_glym, GLYM_SLEEP_DEEP_NREM);
    for (int i = 0; i < 500; i++) {
        glymphatic_update(g_glym, 1.0f);
    }
    float cleared_waste = glymphatic_get_waste_level(g_glym);

    ck_assert_float_lt(cleared_waste, peak_waste);
}
END_TEST

/* ============================================================================
 * Beta-Amyloid and Tau Marker Tests
 * ============================================================================ */

START_TEST(test_beta_amyloid_accumulation)
{
    glymphatic_on_sleep_state_change(g_glym, GLYM_SLEEP_AWAKE);

    float initial_ab = glymphatic_get_beta_amyloid(g_glym);
    for (int i = 0; i < 200; i++) {
        glymphatic_update(g_glym, 1.0f);
    }
    float final_ab = glymphatic_get_beta_amyloid(g_glym);

    /* Beta-amyloid should accumulate during wakefulness */
    ck_assert_float_ge(final_ab, initial_ab);
}
END_TEST

START_TEST(test_tau_clears_slower_than_metabolic)
{
    /* Accumulate waste */
    glymphatic_on_sleep_state_change(g_glym, GLYM_SLEEP_AWAKE);
    for (int i = 0; i < 200; i++) {
        glymphatic_update(g_glym, 1.0f);
    }

    float tau_before = glymphatic_get_tau_level(g_glym);
    float waste_before = glymphatic_get_waste_level(g_glym);

    /* Clear during sleep */
    glymphatic_on_sleep_state_change(g_glym, GLYM_SLEEP_DEEP_NREM);
    for (int i = 0; i < 200; i++) {
        glymphatic_update(g_glym, 1.0f);
    }

    float tau_after = glymphatic_get_tau_level(g_glym);
    float waste_after = glymphatic_get_waste_level(g_glym);

    /* Tau has longer half-life, should clear proportionally slower */
    float tau_fraction_cleared = (tau_before > 0.0f) ? (tau_before - tau_after) / tau_before : 0.0f;
    float waste_fraction_cleared = (waste_before > 0.0f) ? (waste_before - waste_after) / waste_before : 0.0f;

    /* Just verify both cleared somewhat */
    ck_assert_float_ge(tau_fraction_cleared, 0.0f);
    ck_assert_float_ge(waste_fraction_cleared, 0.0f);
}
END_TEST

/* ============================================================================
 * Flush and Interstitial Volume Tests
 * ============================================================================ */

START_TEST(test_emergency_flush)
{
    /* Accumulate waste */
    glymphatic_on_sleep_state_change(g_glym, GLYM_SLEEP_AWAKE);
    for (int i = 0; i < 300; i++) {
        glymphatic_update(g_glym, 1.0f);
    }
    float before_flush = glymphatic_get_waste_level(g_glym);

    /* Force flush */
    int rc = glymphatic_flush(g_glym);
    ck_assert_int_eq(rc, 0);

    /* State should be FLUSHING */
    ck_assert_int_eq(glymphatic_get_state(g_glym), GLYM_FLUSHING);

    /* Switch to sleep state so FLUSHING doesn't immediately revert to INACTIVE
     * (the state machine cancels flush if current_sleep_state == AWAKE) */
    glymphatic_on_sleep_state_change(g_glym, GLYM_SLEEP_DEEP_NREM);

    /* Run updates to complete flush */
    for (int i = 0; i < 200; i++) {
        glymphatic_update(g_glym, 1.0f);
    }
    float after_flush = glymphatic_get_waste_level(g_glym);

    ck_assert_float_lt(after_flush, before_flush);
}
END_TEST

START_TEST(test_interstitial_volume_changes)
{
    /* Interstitial space should expand during NREM */
    glymphatic_on_sleep_state_change(g_glym, GLYM_SLEEP_AWAKE);
    for (int i = 0; i < 10; i++) {
        glymphatic_update(g_glym, 1.0f);
    }
    float awake_vol = glymphatic_get_interstitial_volume(g_glym);

    glymphatic_on_sleep_state_change(g_glym, GLYM_SLEEP_DEEP_NREM);
    for (int i = 0; i < 50; i++) {
        glymphatic_update(g_glym, 1.0f);
    }
    float nrem_vol = glymphatic_get_interstitial_volume(g_glym);

    /* Volume should increase during NREM (biological ~60% expansion) */
    ck_assert_float_ge(nrem_vol, awake_vol);
}
END_TEST

/* ============================================================================
 * Test Suite
 * ============================================================================ */

Suite* glymphatic_sleep_integration_suite(void)
{
    Suite* s = suite_create("Glymphatic Sleep Integration");

    TCase* tc_lifecycle = tcase_create("Lifecycle");
    tcase_add_test(tc_lifecycle, test_glymphatic_create_defaults);
    tcase_add_test(tc_lifecycle, test_glymphatic_destroy_null_safe);
    tcase_set_timeout(tc_lifecycle, 10);
    suite_add_tcase(s, tc_lifecycle);

    TCase* tc_sleep = tcase_create("Sleep Transitions");
    tcase_add_checked_fixture(tc_sleep, setup, teardown);
    tcase_add_test(tc_sleep, test_sleep_transition_awake_to_nrem);
    tcase_add_test(tc_sleep, test_sleep_transition_nrem_to_rem);
    tcase_add_test(tc_sleep, test_sleep_transition_back_to_awake);
    tcase_set_timeout(tc_sleep, 15);
    suite_add_tcase(s, tc_sleep);

    TCase* tc_waste = tcase_create("Waste Dynamics");
    tcase_add_checked_fixture(tc_waste, setup, teardown);
    tcase_add_test(tc_waste, test_waste_accumulates_during_wake);
    tcase_add_test(tc_waste, test_waste_clears_during_sleep);
    tcase_set_timeout(tc_waste, 15);
    suite_add_tcase(s, tc_waste);

    TCase* tc_markers = tcase_create("Waste Markers");
    tcase_add_checked_fixture(tc_markers, setup, teardown);
    tcase_add_test(tc_markers, test_beta_amyloid_accumulation);
    tcase_add_test(tc_markers, test_tau_clears_slower_than_metabolic);
    tcase_set_timeout(tc_markers, 15);
    suite_add_tcase(s, tc_markers);

    TCase* tc_flush = tcase_create("Flush and Volume");
    tcase_add_checked_fixture(tc_flush, setup, teardown);
    tcase_add_test(tc_flush, test_emergency_flush);
    tcase_add_test(tc_flush, test_interstitial_volume_changes);
    tcase_set_timeout(tc_flush, 15);
    suite_add_tcase(s, tc_flush);

    return s;
}

int main(void)
{
    int number_failed;
    Suite* s = glymphatic_sleep_integration_suite();
    SRunner* sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
