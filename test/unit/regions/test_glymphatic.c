/**
 * @file test_glymphatic.c
 * @brief Unit tests for glymphatic waste clearance system
 *
 * WHAT: Test suite for glymphatic system API
 * WHY:  Verify correct behavior of lifecycle, waste clearance dynamics,
 *       sleep state modulation, and query functions
 * HOW:  Unit tests using Check framework covering all core glymphatic functions
 *
 * @author NIMCP Development Team
 * @date 2026-03-05
 */

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "core/brain/regions/glymphatic/nimcp_glymphatic.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

static glymphatic_system_t* g_system = NULL;

static void setup(void)
{
    glymphatic_config_t config = glymphatic_default_config();
    g_system = glymphatic_create(&config);
    ck_assert_ptr_nonnull(g_system);
}

static void teardown(void)
{
    if (g_system) {
        glymphatic_destroy(g_system);
        g_system = NULL;
    }
}

/* ============================================================================
 * Default Config Tests
 * ============================================================================ */

START_TEST(test_default_config_values)
{
    glymphatic_config_t config = glymphatic_default_config();

    ck_assert_float_eq(config.base_clearance_rate, GLYM_DEFAULT_BASE_CLEARANCE);
    ck_assert_float_eq(config.waste_generation_rate, GLYM_DEFAULT_WASTE_GEN_RATE);
    ck_assert_float_eq(config.aqp4_expression, GLYM_DEFAULT_AQP4_EXPRESSION);
    ck_assert_float_eq(config.nrem_clearance_multiplier, GLYM_DEFAULT_NREM_MULTIPLIER);
    ck_assert_float_eq(config.rem_clearance_multiplier, GLYM_DEFAULT_REM_MULTIPLIER);
    ck_assert_float_eq(config.awake_clearance_multiplier, GLYM_DEFAULT_AWAKE_MULTIPLIER);
    ck_assert_float_eq(config.waste_alert_threshold, GLYM_DEFAULT_WASTE_ALERT);
}
END_TEST

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

START_TEST(test_create_with_defaults)
{
    glymphatic_system_t* sys = glymphatic_create(NULL);
    ck_assert_ptr_nonnull(sys);
    ck_assert_uint_eq(sys->magic, GLYM_MAGIC);
    glymphatic_destroy(sys);
}
END_TEST

START_TEST(test_create_with_config)
{
    glymphatic_config_t config = glymphatic_default_config();
    config.base_clearance_rate = 0.01f;
    config.aqp4_expression = 0.5f;

    glymphatic_system_t* sys = glymphatic_create(&config);
    ck_assert_ptr_nonnull(sys);
    ck_assert_uint_eq(sys->magic, GLYM_MAGIC);
    ck_assert_float_eq(sys->config.base_clearance_rate, 0.01f);
    ck_assert_float_eq(sys->config.aqp4_expression, 0.5f);
    glymphatic_destroy(sys);
}
END_TEST

START_TEST(test_destroy_null)
{
    /* Should not crash */
    glymphatic_destroy(NULL);
}
END_TEST

START_TEST(test_initial_state)
{
    ck_assert_int_eq(g_system->state, GLYM_INACTIVE);
    ck_assert_float_ge(g_system->waste_accumulation, 0.0f);
    ck_assert_float_le(g_system->waste_accumulation, 1.0f);
}
END_TEST

/* ============================================================================
 * Update Tests
 * ============================================================================ */

START_TEST(test_update_positive_dt)
{
    int result = glymphatic_update(g_system, 0.1f);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_update_zero_dt)
{
    int result = glymphatic_update(g_system, 0.0f);
    /* Zero dt is rejected as invalid (dt must be positive) */
    ck_assert_int_ne(result, 0);
}
END_TEST

START_TEST(test_update_negative_dt)
{
    int result = glymphatic_update(g_system, -1.0f);
    ck_assert_int_eq(result, -1);
}
END_TEST

START_TEST(test_update_null_system)
{
    int result = glymphatic_update(NULL, 0.1f);
    ck_assert_int_eq(result, -1);
}
END_TEST

START_TEST(test_waste_accumulates_while_awake)
{
    /* System starts awake (GLYM_SLEEP_AWAKE), waste should accumulate */
    float initial_waste = glymphatic_get_waste_level(g_system);
    for (int i = 0; i < 100; i++) {
        glymphatic_update(g_system, 0.1f);
    }
    float final_waste = glymphatic_get_waste_level(g_system);
    ck_assert_float_gt(final_waste, initial_waste);
}
END_TEST

/* ============================================================================
 * Sleep State Change Tests
 * ============================================================================ */

START_TEST(test_sleep_state_change_nrem)
{
    int result = glymphatic_on_sleep_state_change(g_system, GLYM_SLEEP_DEEP_NREM);
    ck_assert_int_eq(result, 0);
    ck_assert_uint_eq(g_system->current_sleep_state, GLYM_SLEEP_DEEP_NREM);
}
END_TEST

START_TEST(test_sleep_state_change_rem)
{
    int result = glymphatic_on_sleep_state_change(g_system, GLYM_SLEEP_REM);
    ck_assert_int_eq(result, 0);
    ck_assert_uint_eq(g_system->current_sleep_state, GLYM_SLEEP_REM);
}
END_TEST

START_TEST(test_sleep_state_change_null)
{
    int result = glymphatic_on_sleep_state_change(NULL, GLYM_SLEEP_DEEP_NREM);
    ck_assert_int_eq(result, -1);
}
END_TEST

START_TEST(test_nrem_boosts_clearance)
{
    /* Accumulate some waste */
    for (int i = 0; i < 50; i++) {
        glymphatic_update(g_system, 0.1f);
    }
    float waste_before_sleep = glymphatic_get_waste_level(g_system);

    /* Switch to NREM sleep and run clearance */
    glymphatic_on_sleep_state_change(g_system, GLYM_SLEEP_DEEP_NREM);
    for (int i = 0; i < 200; i++) {
        glymphatic_update(g_system, 0.1f);
    }
    float waste_after_sleep = glymphatic_get_waste_level(g_system);
    ck_assert_float_lt(waste_after_sleep, waste_before_sleep);
}
END_TEST

/* ============================================================================
 * Flush Tests
 * ============================================================================ */

START_TEST(test_flush_basic)
{
    int result = glymphatic_flush(g_system);
    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(g_system->state, GLYM_FLUSHING);
}
END_TEST

START_TEST(test_flush_null)
{
    int result = glymphatic_flush(NULL);
    ck_assert_int_eq(result, -1);
}
END_TEST

/* ============================================================================
 * Query API Tests
 * ============================================================================ */

START_TEST(test_get_clearance_rate)
{
    float rate = glymphatic_get_clearance_rate(g_system);
    ck_assert_float_ge(rate, 0.0f);
    ck_assert(!isnan(rate));
}
END_TEST

START_TEST(test_get_clearance_rate_null)
{
    float rate = glymphatic_get_clearance_rate(NULL);
    ck_assert_float_eq(rate, -1.0f);
}
END_TEST

START_TEST(test_get_waste_level)
{
    float level = glymphatic_get_waste_level(g_system);
    ck_assert_float_ge(level, 0.0f);
    ck_assert_float_le(level, 1.0f);
}
END_TEST

START_TEST(test_get_waste_level_null)
{
    float level = glymphatic_get_waste_level(NULL);
    ck_assert_float_eq(level, -1.0f);
}
END_TEST

START_TEST(test_get_state)
{
    glymphatic_state_t state = glymphatic_get_state(g_system);
    ck_assert_int_eq(state, GLYM_INACTIVE);
}
END_TEST

START_TEST(test_get_state_null)
{
    glymphatic_state_t state = glymphatic_get_state(NULL);
    ck_assert_int_eq(state, GLYM_INACTIVE);
}
END_TEST

START_TEST(test_get_csf_flow)
{
    float flow = glymphatic_get_csf_flow(g_system);
    ck_assert_float_ge(flow, 0.0f);
    ck_assert(!isnan(flow));
}
END_TEST

START_TEST(test_get_beta_amyloid)
{
    float level = glymphatic_get_beta_amyloid(g_system);
    ck_assert_float_ge(level, 0.0f);
    ck_assert(!isnan(level));
}
END_TEST

START_TEST(test_get_tau_level)
{
    float level = glymphatic_get_tau_level(g_system);
    ck_assert_float_ge(level, 0.0f);
    ck_assert(!isnan(level));
}
END_TEST

START_TEST(test_get_interstitial_volume)
{
    float vol = glymphatic_get_interstitial_volume(g_system);
    ck_assert_float_gt(vol, 0.0f);
    ck_assert(!isnan(vol));
}
END_TEST

START_TEST(test_get_interstitial_volume_null)
{
    float vol = glymphatic_get_interstitial_volume(NULL);
    ck_assert_float_eq(vol, 0.0f);
}
END_TEST

/* ============================================================================
 * Test Suite Setup
 * ============================================================================ */

Suite* glymphatic_suite(void)
{
    Suite* s = suite_create("Glymphatic");

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
    tcase_add_test(tc_update, test_waste_accumulates_while_awake);
    suite_add_tcase(s, tc_update);

    /* Sleep state tests */
    TCase* tc_sleep = tcase_create("Sleep State");
    tcase_add_checked_fixture(tc_sleep, setup, teardown);
    tcase_add_test(tc_sleep, test_sleep_state_change_nrem);
    tcase_add_test(tc_sleep, test_sleep_state_change_rem);
    tcase_add_test(tc_sleep, test_sleep_state_change_null);
    tcase_add_test(tc_sleep, test_nrem_boosts_clearance);
    suite_add_tcase(s, tc_sleep);

    /* Flush tests */
    TCase* tc_flush = tcase_create("Flush");
    tcase_add_checked_fixture(tc_flush, setup, teardown);
    tcase_add_test(tc_flush, test_flush_basic);
    tcase_add_test(tc_flush, test_flush_null);
    suite_add_tcase(s, tc_flush);

    /* Query API tests */
    TCase* tc_query = tcase_create("Query");
    tcase_add_checked_fixture(tc_query, setup, teardown);
    tcase_add_test(tc_query, test_get_clearance_rate);
    tcase_add_test(tc_query, test_get_clearance_rate_null);
    tcase_add_test(tc_query, test_get_waste_level);
    tcase_add_test(tc_query, test_get_waste_level_null);
    tcase_add_test(tc_query, test_get_state);
    tcase_add_test(tc_query, test_get_state_null);
    tcase_add_test(tc_query, test_get_csf_flow);
    tcase_add_test(tc_query, test_get_beta_amyloid);
    tcase_add_test(tc_query, test_get_tau_level);
    tcase_add_test(tc_query, test_get_interstitial_volume);
    tcase_add_test(tc_query, test_get_interstitial_volume_null);
    suite_add_tcase(s, tc_query);

    return s;
}

int main(void)
{
    int number_failed;
    Suite* s = glymphatic_suite();
    SRunner* sr = srunner_create(s);

    srunner_set_fork_status(sr, CK_NOFORK);
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
