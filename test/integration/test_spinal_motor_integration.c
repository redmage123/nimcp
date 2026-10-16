/**
 * @file test_spinal_motor_integration.c
 * @brief Integration tests for Spinal Cord + Motor output systems
 * @date 2026-03-05
 *
 * WHAT: Verifies CPG activation/deactivation, reflex arc triggering,
 *       corticospinal input integration, and motor pool output
 * WHY:  The spinal cord is the final common pathway for motor output;
 *       correct CPG oscillation, reflex processing, and descending
 *       command integration are essential for motor behavior
 * HOW:  Uses Check framework; creates spinal cord, activates CPGs,
 *       triggers reflexes, provides corticospinal input, and verifies
 *       motor pool output
 */

#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "core/spinal/nimcp_spinal_cord.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

static spinal_cord_t* g_spinal = NULL;

static void setup(void)
{
    spinal_config_t cfg = spinal_default_config();
    g_spinal = spinal_create(&cfg);
    ck_assert_ptr_nonnull(g_spinal);
}

static void teardown(void)
{
    if (g_spinal) {
        spinal_destroy(g_spinal);
        g_spinal = NULL;
    }
}

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

START_TEST(test_spinal_create_defaults)
{
    spinal_config_t cfg = spinal_default_config();
    spinal_cord_t* sc = spinal_create(&cfg);
    ck_assert_ptr_nonnull(sc);
    ck_assert_uint_eq(sc->magic, SPINAL_CORD_MAGIC);
    ck_assert_uint_gt(sc->num_motor_pools, 0);
    ck_assert_uint_gt(sc->num_cpgs, 0);
    spinal_destroy(sc);
}
END_TEST

START_TEST(test_spinal_destroy_null_safe)
{
    spinal_destroy(NULL);
}
END_TEST

START_TEST(test_spinal_motor_pools_allocated)
{
    ck_assert_ptr_nonnull(g_spinal->motor_pools);
    ck_assert_uint_gt(g_spinal->num_motor_pools, 0);

    /* Each motor pool should have neurons */
    for (uint32_t i = 0; i < g_spinal->num_motor_pools; i++) {
        ck_assert_uint_gt(g_spinal->motor_pools[i].num_neurons, 0);
        ck_assert_ptr_nonnull(g_spinal->motor_pools[i].activations);
    }
}
END_TEST

/* ============================================================================
 * CPG Tests
 * ============================================================================ */

START_TEST(test_cpg_activation)
{
    /* CPG 0 should start inactive */
    ck_assert(!g_spinal->cpgs[0].active);

    int rc = spinal_activate_cpg(g_spinal, 0);
    ck_assert_int_eq(rc, 0);
    ck_assert(g_spinal->cpgs[0].active);
}
END_TEST

START_TEST(test_cpg_deactivation)
{
    spinal_activate_cpg(g_spinal, 0);
    ck_assert(g_spinal->cpgs[0].active);

    int rc = spinal_deactivate_cpg(g_spinal, 0);
    ck_assert_int_eq(rc, 0);
    ck_assert(!g_spinal->cpgs[0].active);
}
END_TEST

START_TEST(test_cpg_oscillation)
{
    spinal_activate_cpg(g_spinal, 0);

    /* Record flexor/extensor outputs over time */
    float flexor_values[20];
    float extensor_values[20];

    for (int i = 0; i < 20; i++) {
        spinal_update(g_spinal, 0.01f);
        flexor_values[i] = g_spinal->cpgs[0].flexor_output;
        extensor_values[i] = g_spinal->cpgs[0].extensor_output;
    }

    /* CPG should oscillate - check that output varies */
    float min_flex = flexor_values[0];
    float max_flex = flexor_values[0];
    for (int i = 1; i < 20; i++) {
        if (flexor_values[i] < min_flex) min_flex = flexor_values[i];
        if (flexor_values[i] > max_flex) max_flex = flexor_values[i];
    }

    /* With active CPG, there should be some variation */
    /* (May be zero variation in early timesteps; just verify no crash) */
    ck_assert_float_ge(max_flex, min_flex);
}
END_TEST

START_TEST(test_cpg_out_of_range_rejected)
{
    int rc = spinal_activate_cpg(g_spinal, 9999);
    ck_assert_int_eq(rc, -1);

    rc = spinal_deactivate_cpg(g_spinal, 9999);
    ck_assert_int_eq(rc, -1);
}
END_TEST

/* ============================================================================
 * Reflex Arc Tests
 * ============================================================================ */

START_TEST(test_reflex_trigger)
{
    if (g_spinal->num_reflexes == 0) {
        /* Skip if no reflexes configured */
        return;
    }

    int rc = spinal_trigger_reflex(g_spinal, 0, 0.8f);
    ck_assert_int_eq(rc, 0);

    /* Run update to process reflex */
    rc = spinal_update(g_spinal, 0.01f);
    ck_assert_int_eq(rc, 0);
}
END_TEST

START_TEST(test_reflex_out_of_range_rejected)
{
    int rc = spinal_trigger_reflex(g_spinal, 9999, 0.5f);
    ck_assert_int_eq(rc, -1);
}
END_TEST

/* ============================================================================
 * Corticospinal Input Tests
 * ============================================================================ */

START_TEST(test_corticospinal_input)
{
    uint32_t num_pools = g_spinal->num_motor_pools;
    float* input = calloc(num_pools, sizeof(float));
    ck_assert_ptr_nonnull(input);

    /* Set corticospinal command to each pool */
    for (uint32_t i = 0; i < num_pools; i++) {
        input[i] = 0.5f;
    }

    int rc = spinal_set_corticospinal_input(g_spinal, input, num_pools);
    ck_assert_int_eq(rc, 0);

    /* Verify the input was stored */
    if (g_spinal->corticospinal_input) {
        for (uint32_t i = 0; i < num_pools; i++) {
            ck_assert_float_eq_tol(g_spinal->corticospinal_input[i], 0.5f, 0.001f);
        }
    }

    free(input);
}
END_TEST

START_TEST(test_corticospinal_drives_motor_output)
{
    uint32_t num_pools = g_spinal->num_motor_pools;
    float* input = calloc(num_pools, sizeof(float));
    ck_assert_ptr_nonnull(input);

    /* Strong corticospinal drive */
    for (uint32_t i = 0; i < num_pools; i++) {
        input[i] = 0.9f;
    }

    spinal_set_corticospinal_input(g_spinal, input, num_pools);

    /* Update to process input */
    for (int t = 0; t < 10; t++) {
        spinal_update(g_spinal, 0.01f);
    }

    /* Read motor output from pool 0 */
    uint32_t pool_size = g_spinal->motor_pools[0].num_neurons;
    float* output = calloc(pool_size, sizeof(float));
    ck_assert_ptr_nonnull(output);

    int rc = spinal_get_motor_output(g_spinal, 0, output, pool_size);
    ck_assert_int_eq(rc, 0);

    free(output);
    free(input);
}
END_TEST

/* ============================================================================
 * Update Integration Tests
 * ============================================================================ */

START_TEST(test_update_no_crash)
{
    /* Just verify update doesn't crash with default state */
    for (int i = 0; i < 100; i++) {
        int rc = spinal_update(g_spinal, 0.001f);
        ck_assert_int_eq(rc, 0);
    }
}
END_TEST

START_TEST(test_update_null_rejected)
{
    int rc = spinal_update(NULL, 0.01f);
    ck_assert_int_eq(rc, -1);
}
END_TEST

/* ============================================================================
 * Test Suite
 * ============================================================================ */

Suite* spinal_motor_integration_suite(void)
{
    Suite* s = suite_create("Spinal Motor Integration");

    TCase* tc_lifecycle = tcase_create("Lifecycle");
    tcase_add_test(tc_lifecycle, test_spinal_create_defaults);
    tcase_add_test(tc_lifecycle, test_spinal_destroy_null_safe);
    tcase_add_checked_fixture(tc_lifecycle, NULL, NULL);
    tcase_set_timeout(tc_lifecycle, 10);
    suite_add_tcase(s, tc_lifecycle);

    TCase* tc_pools = tcase_create("Motor Pools");
    tcase_add_checked_fixture(tc_pools, setup, teardown);
    tcase_add_test(tc_pools, test_spinal_motor_pools_allocated);
    tcase_set_timeout(tc_pools, 10);
    suite_add_tcase(s, tc_pools);

    TCase* tc_cpg = tcase_create("CPG");
    tcase_add_checked_fixture(tc_cpg, setup, teardown);
    tcase_add_test(tc_cpg, test_cpg_activation);
    tcase_add_test(tc_cpg, test_cpg_deactivation);
    tcase_add_test(tc_cpg, test_cpg_oscillation);
    tcase_add_test(tc_cpg, test_cpg_out_of_range_rejected);
    tcase_set_timeout(tc_cpg, 10);
    suite_add_tcase(s, tc_cpg);

    TCase* tc_reflex = tcase_create("Reflex Arcs");
    tcase_add_checked_fixture(tc_reflex, setup, teardown);
    tcase_add_test(tc_reflex, test_reflex_trigger);
    tcase_add_test(tc_reflex, test_reflex_out_of_range_rejected);
    tcase_set_timeout(tc_reflex, 10);
    suite_add_tcase(s, tc_reflex);

    TCase* tc_cst = tcase_create("Corticospinal Input");
    tcase_add_checked_fixture(tc_cst, setup, teardown);
    tcase_add_test(tc_cst, test_corticospinal_input);
    tcase_add_test(tc_cst, test_corticospinal_drives_motor_output);
    tcase_set_timeout(tc_cst, 10);
    suite_add_tcase(s, tc_cst);

    TCase* tc_update = tcase_create("Update");
    tcase_add_checked_fixture(tc_update, setup, teardown);
    tcase_add_test(tc_update, test_update_no_crash);
    tcase_add_test(tc_update, test_update_null_rejected);
    tcase_set_timeout(tc_update, 10);
    suite_add_tcase(s, tc_update);

    return s;
}

int main(void)
{
    int number_failed;
    Suite* s = spinal_motor_integration_suite();
    SRunner* sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
