/**
 * @file test_inferior_colliculus.c
 * @brief Unit tests for inferior colliculus (auditory midbrain)
 *
 * WHAT: Test suite for inferior colliculus API
 * WHY:  Verify correct behavior of lifecycle, audio processing, tonotopic
 *       response, sound localization, and statistics
 * HOW:  Unit tests using Check framework covering all IC functions
 *
 * @author NIMCP Development Team
 * @date 2026-03-05
 */

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "core/brain/subcortical/nimcp_inferior_colliculus.h"

/* ============================================================================
 * Test Constants
 * ============================================================================ */

#define TEST_NUM_SAMPLES    256
#define TEST_SAMPLE_RATE    16000

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

static inferior_colliculus_t* g_ic = NULL;

static void setup(void)
{
    ic_config_t config = ic_default_config();
    g_ic = ic_create(&config);
    ck_assert_ptr_nonnull(g_ic);
}

static void teardown(void)
{
    if (g_ic) {
        ic_destroy(g_ic);
        g_ic = NULL;
    }
}

/* ============================================================================
 * Default Config Tests
 * ============================================================================ */

START_TEST(test_default_config)
{
    ic_config_t config = ic_default_config();
    ck_assert_uint_eq(config.num_frequency_channels, IC_DEFAULT_NUM_CHANNELS);
    ck_assert_float_eq(config.min_freq_hz, IC_MIN_FREQ_HZ);
    ck_assert_float_eq(config.max_freq_hz, IC_MAX_FREQ_HZ);
    ck_assert_float_ge(config.itd_weight, 0.0f);
    ck_assert_float_ge(config.ild_weight, 0.0f);
}
END_TEST

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

START_TEST(test_create_with_defaults)
{
    inferior_colliculus_t* ic = ic_create(NULL);
    ck_assert_ptr_nonnull(ic);
    ic_destroy(ic);
}
END_TEST

START_TEST(test_create_with_config)
{
    ic_config_t config = ic_default_config();
    config.num_frequency_channels = 32;
    config.enable_cortical_modulation = true;

    inferior_colliculus_t* ic = ic_create(&config);
    ck_assert_ptr_nonnull(ic);
    ic_destroy(ic);
}
END_TEST

START_TEST(test_destroy_null)
{
    /* Should not crash */
    ic_destroy(NULL);
}
END_TEST

/* ============================================================================
 * Update Tests
 * ============================================================================ */

START_TEST(test_update_positive_dt)
{
    int result = ic_update(g_ic, 0.001f);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_update_zero_dt)
{
    int result = ic_update(g_ic, 0.0f);
    /* Zero dt is rejected as invalid (dt must be positive) */
    ck_assert_int_ne(result, 0);
}
END_TEST

START_TEST(test_update_negative_dt)
{
    int result = ic_update(g_ic, -0.001f);
    ck_assert_int_eq(result, -1);
}
END_TEST

START_TEST(test_update_null)
{
    int result = ic_update(NULL, 0.001f);
    ck_assert_int_eq(result, -1);
}
END_TEST

/* ============================================================================
 * Audio Processing Tests
 * ============================================================================ */

START_TEST(test_process_audio_silence)
{
    float left[TEST_NUM_SAMPLES];
    float right[TEST_NUM_SAMPLES];
    memset(left, 0, sizeof(left));
    memset(right, 0, sizeof(right));

    int result = ic_process_audio(g_ic, left, right, TEST_NUM_SAMPLES);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_process_audio_tone)
{
    float left[TEST_NUM_SAMPLES];
    float right[TEST_NUM_SAMPLES];

    /* Generate a 1000 Hz pure tone */
    for (uint32_t i = 0; i < TEST_NUM_SAMPLES; i++) {
        float t = (float)i / TEST_SAMPLE_RATE;
        left[i] = sinf(2.0f * (float)M_PI * 1000.0f * t) * 0.5f;
        right[i] = left[i]; /* Same in both ears = centered */
    }

    int result = ic_process_audio(g_ic, left, right, TEST_NUM_SAMPLES);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_process_audio_null_left)
{
    float right[TEST_NUM_SAMPLES];
    memset(right, 0, sizeof(right));
    int result = ic_process_audio(g_ic, NULL, right, TEST_NUM_SAMPLES);
    ck_assert_int_eq(result, -1);
}
END_TEST

START_TEST(test_process_audio_null_right)
{
    float left[TEST_NUM_SAMPLES];
    memset(left, 0, sizeof(left));
    int result = ic_process_audio(g_ic, left, NULL, TEST_NUM_SAMPLES);
    ck_assert_int_eq(result, -1);
}
END_TEST

START_TEST(test_process_audio_null_system)
{
    float left[TEST_NUM_SAMPLES], right[TEST_NUM_SAMPLES];
    memset(left, 0, sizeof(left));
    memset(right, 0, sizeof(right));
    int result = ic_process_audio(NULL, left, right, TEST_NUM_SAMPLES);
    ck_assert_int_eq(result, -1);
}
END_TEST

START_TEST(test_process_audio_zero_samples)
{
    float left[1] = {0.0f};
    float right[1] = {0.0f};
    int result = ic_process_audio(g_ic, left, right, 0);
    ck_assert_int_eq(result, -1);
}
END_TEST

/* ============================================================================
 * Query API Tests
 * ============================================================================ */

START_TEST(test_get_azimuth)
{
    float az = ic_get_azimuth(g_ic);
    ck_assert_float_ge(az, -IC_MAX_AZIMUTH_DEG);
    ck_assert_float_le(az, IC_MAX_AZIMUTH_DEG);
}
END_TEST

START_TEST(test_get_azimuth_null)
{
    float az = ic_get_azimuth(NULL);
    ck_assert_float_eq(az, 0.0f);
}
END_TEST

START_TEST(test_get_elevation)
{
    float el = ic_get_elevation(g_ic);
    ck_assert_float_ge(el, -IC_MAX_ELEVATION_DEG);
    ck_assert_float_le(el, IC_MAX_ELEVATION_DEG);
}
END_TEST

START_TEST(test_get_elevation_null)
{
    float el = ic_get_elevation(NULL);
    ck_assert_float_eq(el, 0.0f);
}
END_TEST

START_TEST(test_get_channel_activation)
{
    float act = ic_get_channel_activation(g_ic, 0);
    ck_assert_float_ge(act, 0.0f);
    ck_assert(!isnan(act));
}
END_TEST

START_TEST(test_get_channel_activation_invalid)
{
    /* Channel out of range */
    float act = ic_get_channel_activation(g_ic, 9999);
    ck_assert_float_eq(act, -1.0f);
}
END_TEST

START_TEST(test_get_icc_response)
{
    float response[IC_DEFAULT_NUM_CHANNELS];
    int result = ic_get_icc_response(g_ic, response, IC_DEFAULT_NUM_CHANNELS);
    ck_assert_int_eq(result, 0);
    for (uint32_t i = 0; i < IC_DEFAULT_NUM_CHANNELS; i++) {
        ck_assert(!isnan(response[i]));
    }
}
END_TEST

START_TEST(test_get_icc_response_null)
{
    int result = ic_get_icc_response(g_ic, NULL, IC_DEFAULT_NUM_CHANNELS);
    ck_assert_int_eq(result, -1);
}
END_TEST

START_TEST(test_get_icx_response)
{
    float response[IC_DEFAULT_NUM_CHANNELS];
    int result = ic_get_icx_response(g_ic, response, IC_DEFAULT_NUM_CHANNELS);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_get_stats)
{
    ic_stats_t stats;
    int result = ic_get_stats(g_ic, &stats);
    ck_assert_int_eq(result, 0);
}
END_TEST

START_TEST(test_get_stats_null_system)
{
    ic_stats_t stats;
    int result = ic_get_stats(NULL, &stats);
    ck_assert_int_eq(result, -1);
}
END_TEST

START_TEST(test_get_stats_null_output)
{
    int result = ic_get_stats(g_ic, NULL);
    ck_assert_int_eq(result, -1);
}
END_TEST

/* ============================================================================
 * Test Suite Setup
 * ============================================================================ */

Suite* inferior_colliculus_suite(void)
{
    Suite* s = suite_create("Inferior Colliculus");

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
    tcase_add_test(tc_update, test_update_null);
    suite_add_tcase(s, tc_update);

    /* Audio processing tests */
    TCase* tc_audio = tcase_create("Audio Processing");
    tcase_add_checked_fixture(tc_audio, setup, teardown);
    tcase_add_test(tc_audio, test_process_audio_silence);
    tcase_add_test(tc_audio, test_process_audio_tone);
    tcase_add_test(tc_audio, test_process_audio_null_left);
    tcase_add_test(tc_audio, test_process_audio_null_right);
    tcase_add_test(tc_audio, test_process_audio_null_system);
    tcase_add_test(tc_audio, test_process_audio_zero_samples);
    suite_add_tcase(s, tc_audio);

    /* Query tests */
    TCase* tc_query = tcase_create("Query");
    tcase_add_checked_fixture(tc_query, setup, teardown);
    tcase_add_test(tc_query, test_get_azimuth);
    tcase_add_test(tc_query, test_get_azimuth_null);
    tcase_add_test(tc_query, test_get_elevation);
    tcase_add_test(tc_query, test_get_elevation_null);
    tcase_add_test(tc_query, test_get_channel_activation);
    tcase_add_test(tc_query, test_get_channel_activation_invalid);
    tcase_add_test(tc_query, test_get_icc_response);
    tcase_add_test(tc_query, test_get_icc_response_null);
    tcase_add_test(tc_query, test_get_icx_response);
    tcase_add_test(tc_query, test_get_stats);
    tcase_add_test(tc_query, test_get_stats_null_system);
    tcase_add_test(tc_query, test_get_stats_null_output);
    suite_add_tcase(s, tc_query);

    return s;
}

int main(void)
{
    int number_failed;
    Suite* s = inferior_colliculus_suite();
    SRunner* sr = srunner_create(s);

    srunner_set_fork_status(sr, CK_NOFORK);
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
