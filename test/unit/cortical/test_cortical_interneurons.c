/**
 * @file test_cortical_interneurons.c
 * @brief Unit tests for cortical interneuron system
 *
 * WHAT: Test suite for cortical interneuron system API
 * WHY:  Verify correct behavior of lifecycle, E/I balance, gamma oscillations,
 *       VIP disinhibition, attention modulation, and statistics
 * HOW:  Unit tests using Check framework covering all cortical interneuron functions
 *
 * @author NIMCP Development Team
 * @date 2026-03-05
 */

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "core/cortical_columns/nimcp_cortical_interneurons.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

static cortical_interneuron_system_t* g_system = NULL;

static void setup(void)
{
    cint_config_t config;
    cint_default_config(&config);
    g_system = cint_create(&config);
    ck_assert_ptr_nonnull(g_system);
}

static void teardown(void)
{
    if (g_system) {
        cint_destroy(g_system);
        g_system = NULL;
    }
}

/* ============================================================================
 * Default Config Tests
 * ============================================================================ */

START_TEST(test_default_config)
{
    cint_config_t config;
    int result = cint_default_config(&config);
    ck_assert_int_eq(result, 0);

    ck_assert_uint_eq(config.num_pv_basket, CINT_DEFAULT_PV_BASKET);
    ck_assert_uint_eq(config.num_pv_chandelier, CINT_DEFAULT_PV_CHANDELIER);
    ck_assert_uint_eq(config.num_sst, CINT_DEFAULT_SST);
    ck_assert_uint_eq(config.num_vip, CINT_DEFAULT_VIP);
    ck_assert_uint_eq(config.num_ngf, CINT_DEFAULT_NGF);
    ck_assert_float_eq(config.target_ei_ratio, CINT_TARGET_EI_RATIO);
}
END_TEST

START_TEST(test_default_config_null)
{
    int result = cint_default_config(NULL);
    ck_assert_int_eq(result, -1);
}
END_TEST

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

START_TEST(test_create_with_defaults)
{
    cortical_interneuron_system_t* sys = cint_create(NULL);
    ck_assert_ptr_nonnull(sys);
    ck_assert_uint_eq(sys->magic, CINT_MAGIC);
    cint_destroy(sys);
}
END_TEST

START_TEST(test_create_with_config)
{
    cint_config_t config;
    cint_default_config(&config);
    config.num_pv_basket = 20;
    config.num_sst = 10;

    cortical_interneuron_system_t* sys = cint_create(&config);
    ck_assert_ptr_nonnull(sys);
    ck_assert_uint_eq(sys->magic, CINT_MAGIC);
    /* Total = 20 + 10(chand) + 10(sst) + 15(vip) + 10(ngf) = 65 */
    uint32_t expected = config.num_pv_basket + config.num_pv_chandelier +
                        config.num_sst + config.num_vip + config.num_ngf;
    ck_assert_uint_eq(sys->num_interneurons, expected);
    cint_destroy(sys);
}
END_TEST

START_TEST(test_destroy_null)
{
    /* Should not crash */
    cint_destroy(NULL);
}
END_TEST

START_TEST(test_initial_state)
{
    ck_assert_uint_eq(g_system->magic, CINT_MAGIC);
    ck_assert_ptr_nonnull(g_system->interneurons);
    ck_assert_uint_gt(g_system->num_interneurons, 0);
}
END_TEST

/* ============================================================================
 * Update Tests
 * ============================================================================ */

START_TEST(test_update_positive_dt)
{
    int result = cint_update(g_system, 0.001f);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_update_zero_dt)
{
    int result = cint_update(g_system, 0.0f);
    /* Zero dt is rejected as invalid (dt must be positive) */
    ck_assert_int_ne(result, 0);
}
END_TEST

START_TEST(test_update_negative_dt)
{
    int result = cint_update(g_system, -0.01f);
    ck_assert_int_eq(result, -1);
}
END_TEST

START_TEST(test_update_null_system)
{
    int result = cint_update(NULL, 0.001f);
    ck_assert_int_eq(result, -1);
}
END_TEST

START_TEST(test_update_multiple_steps)
{
    for (int i = 0; i < 100; i++) {
        int result = cint_update(g_system, 0.001f);
        ck_assert_int_eq(result, 0);
    }
}
END_TEST

/* ============================================================================
 * Query API Tests
 * ============================================================================ */

START_TEST(test_get_gamma_power)
{
    float gamma = cint_get_gamma_power(g_system);
    ck_assert_float_ge(gamma, 0.0f);
    ck_assert_float_le(gamma, 1.0f);
    ck_assert(!isnan(gamma));
}
END_TEST

START_TEST(test_get_gamma_power_null)
{
    float gamma = cint_get_gamma_power(NULL);
    ck_assert_float_eq(gamma, 0.0f);
}
END_TEST

START_TEST(test_get_ei_balance)
{
    float ei = cint_get_ei_balance(g_system);
    ck_assert(!isnan(ei));
    /* E/I balance should be positive */
    ck_assert_float_gt(ei, 0.0f);
}
END_TEST

START_TEST(test_get_ei_balance_null)
{
    float ei = cint_get_ei_balance(NULL);
    ck_assert_float_eq(ei, -1.0f);
}
END_TEST

START_TEST(test_get_disinhibition)
{
    float disinhib = cint_get_disinhibition(g_system);
    ck_assert_float_ge(disinhib, 0.0f);
    ck_assert_float_le(disinhib, 1.0f);
}
END_TEST

START_TEST(test_get_disinhibition_null)
{
    float disinhib = cint_get_disinhibition(NULL);
    ck_assert_float_eq(disinhib, 0.0f);
}
END_TEST

START_TEST(test_get_prediction_error)
{
    float pe = cint_get_prediction_error(g_system);
    ck_assert_float_ge(pe, 0.0f);
    ck_assert_float_le(pe, 1.0f);
}
END_TEST

START_TEST(test_get_prediction_error_null)
{
    float pe = cint_get_prediction_error(NULL);
    ck_assert_float_eq(pe, 0.0f);
}
END_TEST

/* ============================================================================
 * Modulation Tests
 * ============================================================================ */

START_TEST(test_modulate_attention)
{
    int result = cint_modulate_attention(g_system, 0.8f);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_modulate_attention_null)
{
    int result = cint_modulate_attention(NULL, 0.5f);
    ck_assert_int_eq(result, -1);
}
END_TEST

START_TEST(test_modulate_attention_bounds)
{
    /* Zero attention */
    int result = cint_modulate_attention(g_system, 0.0f);
    ck_assert_int_eq(result, 0);

    /* Full attention */
    result = cint_modulate_attention(g_system, 1.0f);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_attention_increases_disinhibition)
{
    /* Update a few times with no attention */
    for (int i = 0; i < 10; i++) {
        cint_update(g_system, 0.001f);
    }
    float disinhib_before = cint_get_disinhibition(g_system);

    /* Apply high attention */
    cint_modulate_attention(g_system, 1.0f);
    for (int i = 0; i < 50; i++) {
        cint_update(g_system, 0.001f);
    }
    float disinhib_after = cint_get_disinhibition(g_system);

    /* Attention should increase VIP-mediated disinhibition */
    ck_assert_float_ge(disinhib_after, disinhib_before);
}
END_TEST

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

START_TEST(test_get_stats)
{
    cint_stats_t stats;
    int result = cint_get_stats(g_system, &stats);
    ck_assert_int_eq(result, 0);
    ck_assert_uint_eq(stats.total_updates, 0);
}
END_TEST

START_TEST(test_get_stats_null_system)
{
    cint_stats_t stats;
    int result = cint_get_stats(NULL, &stats);
    ck_assert_int_eq(result, -1);
}
END_TEST

START_TEST(test_get_stats_null_output)
{
    int result = cint_get_stats(g_system, NULL);
    ck_assert_int_eq(result, -1);
}
END_TEST

START_TEST(test_reset_stats)
{
    /* Do some updates */
    for (int i = 0; i < 10; i++) {
        cint_update(g_system, 0.001f);
    }
    cint_stats_t stats;
    cint_get_stats(g_system, &stats);
    ck_assert_uint_gt(stats.total_updates, 0);

    /* Reset */
    cint_reset_stats(g_system);
    cint_get_stats(g_system, &stats);
    ck_assert_uint_eq(stats.total_updates, 0);
}
END_TEST

/* ============================================================================
 * Test Suite Setup
 * ============================================================================ */

Suite* cortical_interneurons_suite(void)
{
    Suite* s = suite_create("Cortical Interneurons");

    /* Config tests */
    TCase* tc_config = tcase_create("Configuration");
    tcase_add_test(tc_config, test_default_config);
    tcase_add_test(tc_config, test_default_config_null);
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
    tcase_add_test(tc_update, test_update_multiple_steps);
    suite_add_tcase(s, tc_update);

    /* Query tests */
    TCase* tc_query = tcase_create("Query");
    tcase_add_checked_fixture(tc_query, setup, teardown);
    tcase_add_test(tc_query, test_get_gamma_power);
    tcase_add_test(tc_query, test_get_gamma_power_null);
    tcase_add_test(tc_query, test_get_ei_balance);
    tcase_add_test(tc_query, test_get_ei_balance_null);
    tcase_add_test(tc_query, test_get_disinhibition);
    tcase_add_test(tc_query, test_get_disinhibition_null);
    tcase_add_test(tc_query, test_get_prediction_error);
    tcase_add_test(tc_query, test_get_prediction_error_null);
    suite_add_tcase(s, tc_query);

    /* Modulation tests */
    TCase* tc_modulate = tcase_create("Modulation");
    tcase_add_checked_fixture(tc_modulate, setup, teardown);
    tcase_add_test(tc_modulate, test_modulate_attention);
    tcase_add_test(tc_modulate, test_modulate_attention_null);
    tcase_add_test(tc_modulate, test_modulate_attention_bounds);
    tcase_add_test(tc_modulate, test_attention_increases_disinhibition);
    suite_add_tcase(s, tc_modulate);

    /* Statistics tests */
    TCase* tc_stats = tcase_create("Statistics");
    tcase_add_checked_fixture(tc_stats, setup, teardown);
    tcase_add_test(tc_stats, test_get_stats);
    tcase_add_test(tc_stats, test_get_stats_null_system);
    tcase_add_test(tc_stats, test_get_stats_null_output);
    tcase_add_test(tc_stats, test_reset_stats);
    suite_add_tcase(s, tc_stats);

    return s;
}

int main(void)
{
    int number_failed;
    Suite* s = cortical_interneurons_suite();
    SRunner* sr = srunner_create(s);

    srunner_set_fork_status(sr, CK_NOFORK);
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
