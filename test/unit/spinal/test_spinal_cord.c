/**
 * @file test_spinal_cord.c
 * @brief Unit tests for spinal cord motor output system
 *
 * WHAT: Test suite for spinal cord API
 * WHY:  Verify correct behavior of lifecycle, CPG control, reflex arcs,
 *       corticospinal input, and motor output
 * HOW:  Unit tests using Check framework covering all spinal cord functions
 *
 * @author NIMCP Development Team
 * @date 2026-03-05
 */

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "core/spinal/nimcp_spinal_cord.h"

/* ============================================================================
 * Test Constants
 * ============================================================================ */

#define TEST_NUM_POOLS      4
#define TEST_NEURONS_PER    8
#define TEST_NUM_CPGS       2
#define TEST_NUM_REFLEXES   2

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

static spinal_cord_t* g_spinal = NULL;

static void setup(void)
{
    spinal_config_t config = spinal_default_config();
    config.num_motor_pools = TEST_NUM_POOLS;
    config.neurons_per_pool = TEST_NEURONS_PER;
    config.num_cpgs = TEST_NUM_CPGS;
    config.num_reflexes = TEST_NUM_REFLEXES;
    g_spinal = spinal_create(&config);
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
 * Default Config Tests
 * ============================================================================ */

START_TEST(test_default_config)
{
    spinal_config_t config = spinal_default_config();
    ck_assert_uint_gt(config.num_motor_pools, 0);
    ck_assert_uint_le(config.num_motor_pools, SPINAL_MAX_MOTOR_POOLS);
    ck_assert_uint_gt(config.neurons_per_pool, 0);
    ck_assert_uint_gt(config.num_cpgs, 0);
    ck_assert_uint_le(config.num_cpgs, SPINAL_MAX_CPGS);
    ck_assert_uint_gt(config.num_reflexes, 0);
    ck_assert_uint_le(config.num_reflexes, SPINAL_MAX_REFLEXES);
    ck_assert_float_gt(config.default_cpg_frequency, 0.0f);
    ck_assert_float_ge(config.default_reflex_gain, 0.0f);
    ck_assert_float_le(config.default_reflex_gain, 1.0f);
}
END_TEST

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

START_TEST(test_create_with_defaults)
{
    spinal_config_t config = spinal_default_config();
    spinal_cord_t* sc = spinal_create(&config);
    ck_assert_ptr_nonnull(sc);
    ck_assert_uint_eq(sc->magic, SPINAL_CORD_MAGIC);
    spinal_destroy(sc);
}
END_TEST

START_TEST(test_create_null_config)
{
    /* Depending on implementation, NULL may use defaults or fail */
    spinal_cord_t* sc = spinal_create(NULL);
    /* Either outcome is acceptable */
    if (sc) {
        ck_assert_uint_eq(sc->magic, SPINAL_CORD_MAGIC);
        spinal_destroy(sc);
    }
}
END_TEST

START_TEST(test_destroy_null)
{
    /* Should not crash */
    spinal_destroy(NULL);
}
END_TEST

START_TEST(test_initial_state)
{
    ck_assert_uint_eq(g_spinal->magic, SPINAL_CORD_MAGIC);
    ck_assert_uint_eq(g_spinal->num_motor_pools, TEST_NUM_POOLS);
    ck_assert_ptr_nonnull(g_spinal->motor_pools);
    ck_assert_uint_eq(g_spinal->num_cpgs, TEST_NUM_CPGS);
    ck_assert_ptr_nonnull(g_spinal->cpgs);
    ck_assert_uint_eq(g_spinal->num_reflexes, TEST_NUM_REFLEXES);
    ck_assert_ptr_nonnull(g_spinal->reflexes);
}
END_TEST

START_TEST(test_motor_pool_allocation)
{
    for (uint32_t i = 0; i < g_spinal->num_motor_pools; i++) {
        ck_assert_uint_eq(g_spinal->motor_pools[i].num_neurons, TEST_NEURONS_PER);
        ck_assert_ptr_nonnull(g_spinal->motor_pools[i].activations);
        ck_assert_ptr_nonnull(g_spinal->motor_pools[i].target_forces);
    }
}
END_TEST

/* ============================================================================
 * Update Tests
 * ============================================================================ */

START_TEST(test_update_positive_dt)
{
    int result = spinal_update(g_spinal, 0.001f);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_update_zero_dt)
{
    int result = spinal_update(g_spinal, 0.0f);
    /* Zero dt is rejected as invalid (dt must be positive) */
    ck_assert_int_ne(result, 0);
}
END_TEST

START_TEST(test_update_negative_dt)
{
    int result = spinal_update(g_spinal, -0.001f);
    ck_assert_int_eq(result, -1);
}
END_TEST

START_TEST(test_update_null_system)
{
    int result = spinal_update(NULL, 0.001f);
    ck_assert_int_eq(result, -1);
}
END_TEST

START_TEST(test_update_multiple_steps)
{
    for (int i = 0; i < 100; i++) {
        int result = spinal_update(g_spinal, 0.001f);
        ck_assert_int_eq(result, 0);
    }
}
END_TEST

/* ============================================================================
 * CPG Tests
 * ============================================================================ */

START_TEST(test_activate_cpg)
{
    int result = spinal_activate_cpg(g_spinal, 0);
    ck_assert_int_eq(result, 0);
    ck_assert(g_spinal->cpgs[0].active);
}
END_TEST

START_TEST(test_activate_cpg_invalid_id)
{
    int result = spinal_activate_cpg(g_spinal, TEST_NUM_CPGS);
    ck_assert_int_eq(result, -1);
}
END_TEST

START_TEST(test_activate_cpg_null)
{
    int result = spinal_activate_cpg(NULL, 0);
    ck_assert_int_eq(result, -1);
}
END_TEST

START_TEST(test_deactivate_cpg)
{
    spinal_activate_cpg(g_spinal, 0);
    ck_assert(g_spinal->cpgs[0].active);

    int result = spinal_deactivate_cpg(g_spinal, 0);
    ck_assert_int_eq(result, 0);
    ck_assert(!g_spinal->cpgs[0].active);
}
END_TEST

START_TEST(test_deactivate_cpg_invalid_id)
{
    int result = spinal_deactivate_cpg(g_spinal, TEST_NUM_CPGS);
    ck_assert_int_eq(result, -1);
}
END_TEST

START_TEST(test_deactivate_cpg_null)
{
    int result = spinal_deactivate_cpg(NULL, 0);
    ck_assert_int_eq(result, -1);
}
END_TEST

START_TEST(test_cpg_produces_oscillation)
{
    spinal_activate_cpg(g_spinal, 0);

    /* Run several update steps.
     * CPG flexor_output = amplitude * max(sin(phase), 0), clamped to [0,1].
     * With default amplitude=0.5, output ranges [0, 0.5].
     * Detect crossings around 0.25 (midpoint of range). */
    float prev_flexor = g_spinal->cpgs[0].flexor_output;
    int sign_changes = 0;
    for (int i = 0; i < 200; i++) {
        spinal_update(g_spinal, 0.005f);
        float cur_flexor = g_spinal->cpgs[0].flexor_output;
        if ((cur_flexor - 0.25f) * (prev_flexor - 0.25f) < 0.0f) {
            sign_changes++;
        }
        prev_flexor = cur_flexor;
    }
    /* An oscillating CPG should have multiple crossings around the midpoint */
    ck_assert_int_gt(sign_changes, 0);
}
END_TEST

/* ============================================================================
 * Corticospinal Input Tests
 * ============================================================================ */

START_TEST(test_set_corticospinal_input)
{
    float input[TEST_NUM_POOLS];
    for (uint32_t i = 0; i < TEST_NUM_POOLS; i++) {
        input[i] = 0.5f;
    }
    int result = spinal_set_corticospinal_input(g_spinal, input, TEST_NUM_POOLS);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_set_corticospinal_input_null_system)
{
    float input[TEST_NUM_POOLS];
    memset(input, 0, sizeof(input));
    int result = spinal_set_corticospinal_input(NULL, input, TEST_NUM_POOLS);
    ck_assert_int_eq(result, -1);
}
END_TEST

START_TEST(test_set_corticospinal_input_null_input)
{
    int result = spinal_set_corticospinal_input(g_spinal, NULL, TEST_NUM_POOLS);
    ck_assert_int_eq(result, -1);
}
END_TEST

START_TEST(test_set_corticospinal_input_wrong_size)
{
    float input[2] = {0.5f, 0.5f};
    int result = spinal_set_corticospinal_input(g_spinal, input, 2);
    /* Size mismatch should fail (2 != TEST_NUM_POOLS) */
    ck_assert_int_eq(result, -1);
}
END_TEST

/* ============================================================================
 * Motor Output Tests
 * ============================================================================ */

START_TEST(test_get_motor_output)
{
    float output[TEST_NEURONS_PER];
    int result = spinal_get_motor_output(g_spinal, 0, output, TEST_NEURONS_PER);
    ck_assert_int_eq(result, 0);
    for (uint32_t i = 0; i < TEST_NEURONS_PER; i++) {
        ck_assert(!isnan(output[i]));
    }
}
END_TEST

START_TEST(test_get_motor_output_null_system)
{
    float output[TEST_NEURONS_PER];
    int result = spinal_get_motor_output(NULL, 0, output, TEST_NEURONS_PER);
    ck_assert_int_eq(result, -1);
}
END_TEST

START_TEST(test_get_motor_output_null_output)
{
    int result = spinal_get_motor_output(g_spinal, 0, NULL, TEST_NEURONS_PER);
    ck_assert_int_eq(result, -1);
}
END_TEST

START_TEST(test_get_motor_output_invalid_pool)
{
    float output[TEST_NEURONS_PER];
    int result = spinal_get_motor_output(g_spinal, TEST_NUM_POOLS, output, TEST_NEURONS_PER);
    ck_assert_int_eq(result, -1);
}
END_TEST

/* ============================================================================
 * Reflex Tests
 * ============================================================================ */

START_TEST(test_trigger_reflex)
{
    int result = spinal_trigger_reflex(g_spinal, 0, 0.7f);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_trigger_reflex_null)
{
    int result = spinal_trigger_reflex(NULL, 0, 0.5f);
    ck_assert_int_eq(result, -1);
}
END_TEST

START_TEST(test_trigger_reflex_invalid_id)
{
    int result = spinal_trigger_reflex(g_spinal, TEST_NUM_REFLEXES, 0.5f);
    ck_assert_int_eq(result, -1);
}
END_TEST

START_TEST(test_trigger_reflex_zero_stimulus)
{
    int result = spinal_trigger_reflex(g_spinal, 0, 0.0f);
    ck_assert_int_eq(result, 0);
}
END_TEST

/* ============================================================================
 * Test Suite Setup
 * ============================================================================ */

Suite* spinal_cord_suite(void)
{
    Suite* s = suite_create("Spinal Cord");

    /* Config tests */
    TCase* tc_config = tcase_create("Configuration");
    tcase_add_test(tc_config, test_default_config);
    suite_add_tcase(s, tc_config);

    /* Lifecycle tests */
    TCase* tc_lifecycle = tcase_create("Lifecycle");
    tcase_add_test(tc_lifecycle, test_create_with_defaults);
    tcase_add_test(tc_lifecycle, test_create_null_config);
    tcase_add_test(tc_lifecycle, test_destroy_null);
    tcase_add_checked_fixture(tc_lifecycle, setup, teardown);
    tcase_add_test(tc_lifecycle, test_initial_state);
    tcase_add_test(tc_lifecycle, test_motor_pool_allocation);
    suite_add_tcase(s, tc_lifecycle);

    /* Update tests */
    TCase* tc_update = tcase_create("Update");
    tcase_add_checked_fixture(tc_update, setup, teardown);
    tcase_add_test(tc_update, test_update_positive_dt);
    tcase_add_test(tc_update, test_update_zero_dt);
    tcase_add_test(tc_update, test_update_negative_dt);
    tcase_add_test(tc_update, test_update_null_system);
    tcase_add_test(tc_update, test_update_multiple_steps);
    suite_add_tcase(s, tc_update);

    /* CPG tests */
    TCase* tc_cpg = tcase_create("CPG");
    tcase_add_checked_fixture(tc_cpg, setup, teardown);
    tcase_add_test(tc_cpg, test_activate_cpg);
    tcase_add_test(tc_cpg, test_activate_cpg_invalid_id);
    tcase_add_test(tc_cpg, test_activate_cpg_null);
    tcase_add_test(tc_cpg, test_deactivate_cpg);
    tcase_add_test(tc_cpg, test_deactivate_cpg_invalid_id);
    tcase_add_test(tc_cpg, test_deactivate_cpg_null);
    tcase_add_test(tc_cpg, test_cpg_produces_oscillation);
    suite_add_tcase(s, tc_cpg);

    /* Corticospinal input tests */
    TCase* tc_csi = tcase_create("Corticospinal Input");
    tcase_add_checked_fixture(tc_csi, setup, teardown);
    tcase_add_test(tc_csi, test_set_corticospinal_input);
    tcase_add_test(tc_csi, test_set_corticospinal_input_null_system);
    tcase_add_test(tc_csi, test_set_corticospinal_input_null_input);
    tcase_add_test(tc_csi, test_set_corticospinal_input_wrong_size);
    suite_add_tcase(s, tc_csi);

    /* Motor output tests */
    TCase* tc_motor = tcase_create("Motor Output");
    tcase_add_checked_fixture(tc_motor, setup, teardown);
    tcase_add_test(tc_motor, test_get_motor_output);
    tcase_add_test(tc_motor, test_get_motor_output_null_system);
    tcase_add_test(tc_motor, test_get_motor_output_null_output);
    tcase_add_test(tc_motor, test_get_motor_output_invalid_pool);
    suite_add_tcase(s, tc_motor);

    /* Reflex tests */
    TCase* tc_reflex = tcase_create("Reflex");
    tcase_add_checked_fixture(tc_reflex, setup, teardown);
    tcase_add_test(tc_reflex, test_trigger_reflex);
    tcase_add_test(tc_reflex, test_trigger_reflex_null);
    tcase_add_test(tc_reflex, test_trigger_reflex_invalid_id);
    tcase_add_test(tc_reflex, test_trigger_reflex_zero_stimulus);
    suite_add_tcase(s, tc_reflex);

    return s;
}

int main(void)
{
    int number_failed;
    Suite* s = spinal_cord_suite();
    SRunner* sr = srunner_create(s);

    srunner_set_fork_status(sr, CK_NOFORK);
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
