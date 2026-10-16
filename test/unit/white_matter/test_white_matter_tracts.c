/**
 * @file test_white_matter_tracts.c
 * @brief Unit tests for white matter tract system
 *
 * WHAT: Test suite for white matter tract system API
 * WHY:  Verify correct behavior of lifecycle, conduction velocity,
 *       myelination modulation, signal routing, and statistics
 * HOW:  Unit tests using Check framework covering all WMT functions
 *
 * @author NIMCP Development Team
 * @date 2026-03-05
 */

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "core/brain/white_matter/nimcp_white_matter_tracts.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

static wmt_system_t* g_system = NULL;

static void setup(void)
{
    wmt_config_t config = wmt_default_config();
    g_system = wmt_create(&config);
    ck_assert_ptr_nonnull(g_system);
}

static void teardown(void)
{
    if (g_system) {
        wmt_destroy(g_system);
        g_system = NULL;
    }
}

/* ============================================================================
 * Default Config Tests
 * ============================================================================ */

START_TEST(test_default_config)
{
    wmt_config_t config = wmt_default_config();
    ck_assert_float_ge(config.base_myelination, 0.0f);
    ck_assert_float_le(config.base_myelination, 1.0f);
    ck_assert_float_ge(config.base_integrity, 0.0f);
    ck_assert_float_le(config.base_integrity, 1.0f);
    ck_assert_float_gt(config.min_conduction_velocity, 0.0f);
    ck_assert_float_gt(config.max_conduction_velocity, config.min_conduction_velocity);
}
END_TEST

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

START_TEST(test_create_with_defaults)
{
    wmt_system_t* sys = wmt_create(NULL);
    ck_assert_ptr_nonnull(sys);
    wmt_destroy(sys);
}
END_TEST

START_TEST(test_create_with_config)
{
    wmt_config_t config = wmt_default_config();
    config.base_myelination = 0.9f;
    config.enable_velocity_jitter = false;

    wmt_system_t* sys = wmt_create(&config);
    ck_assert_ptr_nonnull(sys);
    wmt_destroy(sys);
}
END_TEST

START_TEST(test_destroy_null)
{
    /* Should not crash */
    wmt_destroy(NULL);
}
END_TEST

/* ============================================================================
 * Update Tests
 * ============================================================================ */

START_TEST(test_update_positive_dt)
{
    int result = wmt_update(g_system, 0.01f);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_update_zero_dt)
{
    int result = wmt_update(g_system, 0.0f);
    /* Zero dt is rejected as invalid (dt must be positive) */
    ck_assert_int_ne(result, 0);
}
END_TEST

START_TEST(test_update_negative_dt)
{
    int result = wmt_update(g_system, -1.0f);
    ck_assert_int_eq(result, -1);
}
END_TEST

START_TEST(test_update_null_system)
{
    int result = wmt_update(NULL, 0.01f);
    ck_assert_int_eq(result, -1);
}
END_TEST

/* ============================================================================
 * Query API Tests
 * ============================================================================ */

START_TEST(test_get_tract_delay_all)
{
    for (int i = 0; i < WMT_COUNT; i++) {
        float delay = wmt_get_tract_delay(g_system, (white_matter_tract_t)i);
        ck_assert_float_gt(delay, 0.0f);
        ck_assert(!isnan(delay));
    }
}
END_TEST

START_TEST(test_get_tract_delay_null)
{
    float delay = wmt_get_tract_delay(NULL, WMT_CORPUS_CALLOSUM);
    ck_assert_float_eq(delay, -1.0f);
}
END_TEST

START_TEST(test_get_tract_delay_invalid)
{
    float delay = wmt_get_tract_delay(g_system, WMT_COUNT);
    ck_assert_float_eq(delay, -1.0f);
}
END_TEST

START_TEST(test_get_tract_integrity)
{
    float integrity = wmt_get_tract_integrity(g_system, WMT_CORPUS_CALLOSUM);
    ck_assert_float_ge(integrity, 0.0f);
    ck_assert_float_le(integrity, 1.0f);
}
END_TEST

START_TEST(test_get_tract_integrity_null)
{
    float integrity = wmt_get_tract_integrity(NULL, WMT_CORPUS_CALLOSUM);
    ck_assert_float_eq(integrity, -1.0f);
}
END_TEST

START_TEST(test_get_tract_myelination)
{
    float myelin = wmt_get_tract_myelination(g_system, WMT_ARCUATE_FASCICULUS);
    ck_assert_float_ge(myelin, 0.0f);
    ck_assert_float_le(myelin, 1.0f);
}
END_TEST

START_TEST(test_get_tract_velocity)
{
    float vel = wmt_get_tract_velocity(g_system, WMT_CORTICOSPINAL);
    ck_assert_float_gt(vel, 0.0f);
    ck_assert(!isnan(vel));
}
END_TEST

START_TEST(test_get_tract_velocity_null)
{
    float vel = wmt_get_tract_velocity(NULL, WMT_CORTICOSPINAL);
    ck_assert_float_eq(vel, -1.0f);
}
END_TEST

START_TEST(test_get_tract_state)
{
    tract_state_t state;
    memset(&state, 0, sizeof(state));

    int result = wmt_get_tract_state(g_system, WMT_CINGULUM, &state);
    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(state.type, WMT_CINGULUM);
    ck_assert_float_gt(state.conduction_velocity_ms, 0.0f);
    ck_assert_float_ge(state.myelination_level, 0.0f);
    ck_assert_float_ge(state.integrity, 0.0f);
    ck_assert_float_gt(state.tract_length_m, 0.0f);
}
END_TEST

START_TEST(test_get_tract_state_null_system)
{
    tract_state_t state;
    int result = wmt_get_tract_state(NULL, WMT_CINGULUM, &state);
    ck_assert_int_eq(result, -1);
}
END_TEST

START_TEST(test_get_tract_state_null_output)
{
    int result = wmt_get_tract_state(g_system, WMT_CINGULUM, NULL);
    ck_assert_int_eq(result, -1);
}
END_TEST

START_TEST(test_get_stats)
{
    wmt_stats_t stats;
    int result = wmt_get_stats(g_system, &stats);
    ck_assert_int_eq(result, 0);
    ck_assert_float_ge(stats.mean_myelination, 0.0f);
    ck_assert_float_ge(stats.mean_integrity, 0.0f);
    ck_assert_float_gt(stats.mean_conduction_velocity, 0.0f);
}
END_TEST

START_TEST(test_get_stats_null)
{
    wmt_stats_t stats;
    int result = wmt_get_stats(NULL, &stats);
    ck_assert_int_eq(result, -1);
}
END_TEST

/* ============================================================================
 * Modulation Tests
 * ============================================================================ */

START_TEST(test_modulate_myelination_positive)
{
    float before = wmt_get_tract_myelination(g_system, WMT_CORPUS_CALLOSUM);
    int result = wmt_modulate_myelination(g_system, WMT_CORPUS_CALLOSUM, 0.1f);
    ck_assert_int_eq(result, 0);
    float after = wmt_get_tract_myelination(g_system, WMT_CORPUS_CALLOSUM);
    ck_assert_float_ge(after, before);
}
END_TEST

START_TEST(test_modulate_myelination_negative)
{
    float before = wmt_get_tract_myelination(g_system, WMT_CORPUS_CALLOSUM);
    int result = wmt_modulate_myelination(g_system, WMT_CORPUS_CALLOSUM, -0.1f);
    ck_assert_int_eq(result, 0);
    float after = wmt_get_tract_myelination(g_system, WMT_CORPUS_CALLOSUM);
    ck_assert_float_le(after, before);
}
END_TEST

START_TEST(test_modulate_myelination_null)
{
    int result = wmt_modulate_myelination(NULL, WMT_CORPUS_CALLOSUM, 0.1f);
    ck_assert_int_eq(result, -1);
}
END_TEST

START_TEST(test_modulate_integrity_positive)
{
    int result = wmt_modulate_integrity(g_system, WMT_OPTIC_RADIATION, 0.05f);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_modulate_integrity_null)
{
    int result = wmt_modulate_integrity(NULL, WMT_OPTIC_RADIATION, 0.05f);
    ck_assert_int_eq(result, -1);
}
END_TEST

/* ============================================================================
 * Signal Routing Tests
 * ============================================================================ */

START_TEST(test_route_signal_basic)
{
    float attenuated = 0.0f;
    float delay = 0.0f;
    int result = wmt_route_signal(g_system, WMT_CORPUS_CALLOSUM, 1.0f,
                                   &attenuated, &delay);
    ck_assert_int_eq(result, 0);
    ck_assert_float_ge(attenuated, 0.0f);
    ck_assert_float_le(attenuated, 1.0f);
    ck_assert_float_gt(delay, 0.0f);
}
END_TEST

START_TEST(test_route_signal_null_system)
{
    float attenuated = 0.0f, delay = 0.0f;
    int result = wmt_route_signal(NULL, WMT_CORPUS_CALLOSUM, 1.0f,
                                   &attenuated, &delay);
    ck_assert_int_eq(result, -1);
}
END_TEST

START_TEST(test_route_signal_null_outputs)
{
    int result = wmt_route_signal(g_system, WMT_CORPUS_CALLOSUM, 1.0f, NULL, NULL);
    ck_assert_int_eq(result, -1);
}
END_TEST

/* ============================================================================
 * Utility Tests
 * ============================================================================ */

START_TEST(test_tract_name)
{
    ck_assert_str_ne(wmt_tract_name(WMT_CORPUS_CALLOSUM), "UNKNOWN");
    ck_assert_str_ne(wmt_tract_name(WMT_ARCUATE_FASCICULUS), "UNKNOWN");
    ck_assert_str_ne(wmt_tract_name(WMT_CORTICOSPINAL), "UNKNOWN");
    ck_assert_str_ne(wmt_tract_name(WMT_OPTIC_RADIATION), "UNKNOWN");
}
END_TEST

START_TEST(test_tract_name_invalid)
{
    const char* name = wmt_tract_name(WMT_COUNT);
    ck_assert_str_eq(name, "UNKNOWN");
}
END_TEST

/* ============================================================================
 * Test Suite Setup
 * ============================================================================ */

Suite* white_matter_suite(void)
{
    Suite* s = suite_create("White Matter Tracts");

    /* Config tests */
    TCase* tc_config = tcase_create("Configuration");
    tcase_add_test(tc_config, test_default_config);
    suite_add_tcase(s, tc_config);

    /* Lifecycle tests */
    TCase* tc_lifecycle = tcase_create("Lifecycle");
    tcase_add_test(tc_lifecycle, test_create_with_defaults);
    tcase_add_test(tc_lifecycle, test_create_with_config);
    tcase_add_test(tc_lifecycle, test_destroy_null);
    suite_add_tcase(s, tc_lifecycle);

    /* Update tests */
    TCase* tc_update = tcase_create("Update");
    tcase_add_checked_fixture(tc_update, setup, teardown);
    tcase_add_test(tc_update, test_update_positive_dt);
    tcase_add_test(tc_update, test_update_zero_dt);
    tcase_add_test(tc_update, test_update_negative_dt);
    tcase_add_test(tc_update, test_update_null_system);
    suite_add_tcase(s, tc_update);

    /* Query tests */
    TCase* tc_query = tcase_create("Query");
    tcase_add_checked_fixture(tc_query, setup, teardown);
    tcase_add_test(tc_query, test_get_tract_delay_all);
    tcase_add_test(tc_query, test_get_tract_delay_null);
    tcase_add_test(tc_query, test_get_tract_delay_invalid);
    tcase_add_test(tc_query, test_get_tract_integrity);
    tcase_add_test(tc_query, test_get_tract_integrity_null);
    tcase_add_test(tc_query, test_get_tract_myelination);
    tcase_add_test(tc_query, test_get_tract_velocity);
    tcase_add_test(tc_query, test_get_tract_velocity_null);
    tcase_add_test(tc_query, test_get_tract_state);
    tcase_add_test(tc_query, test_get_tract_state_null_system);
    tcase_add_test(tc_query, test_get_tract_state_null_output);
    tcase_add_test(tc_query, test_get_stats);
    tcase_add_test(tc_query, test_get_stats_null);
    suite_add_tcase(s, tc_query);

    /* Modulation tests */
    TCase* tc_modulate = tcase_create("Modulation");
    tcase_add_checked_fixture(tc_modulate, setup, teardown);
    tcase_add_test(tc_modulate, test_modulate_myelination_positive);
    tcase_add_test(tc_modulate, test_modulate_myelination_negative);
    tcase_add_test(tc_modulate, test_modulate_myelination_null);
    tcase_add_test(tc_modulate, test_modulate_integrity_positive);
    tcase_add_test(tc_modulate, test_modulate_integrity_null);
    suite_add_tcase(s, tc_modulate);

    /* Signal routing tests */
    TCase* tc_route = tcase_create("Signal Routing");
    tcase_add_checked_fixture(tc_route, setup, teardown);
    tcase_add_test(tc_route, test_route_signal_basic);
    tcase_add_test(tc_route, test_route_signal_null_system);
    tcase_add_test(tc_route, test_route_signal_null_outputs);
    suite_add_tcase(s, tc_route);

    /* Utility tests */
    TCase* tc_util = tcase_create("Utility");
    tcase_add_test(tc_util, test_tract_name);
    tcase_add_test(tc_util, test_tract_name_invalid);
    suite_add_tcase(s, tc_util);

    return s;
}

int main(void)
{
    int number_failed;
    Suite* s = white_matter_suite();
    SRunner* sr = srunner_create(s);

    srunner_set_fork_status(sr, CK_NOFORK);
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
