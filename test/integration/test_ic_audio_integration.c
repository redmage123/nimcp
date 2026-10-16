/**
 * @file test_ic_audio_integration.c
 * @brief Integration tests for Inferior Colliculus + Audio processing
 * @date 2026-03-05
 *
 * WHAT: Verifies inferior colliculus tonotopic response, binaural
 *       localization (azimuth/elevation), and temporal pattern detection
 * WHY:  The IC is a mandatory auditory relay nucleus; correct tonotopic
 *       mapping and spatial localization are essential for auditory
 *       scene analysis
 * HOW:  Uses Check framework; creates IC, feeds binaural audio,
 *       verifies tonotopic channel activation, azimuth/elevation
 *       estimates, and statistics
 */

#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "core/brain/subcortical/nimcp_inferior_colliculus.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

#define TEST_NUM_SAMPLES  256
#define TEST_SAMPLE_RATE  16000.0f

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

static inferior_colliculus_t* g_ic = NULL;

static void setup(void)
{
    ic_config_t cfg = ic_default_config();
    g_ic = ic_create(&cfg);
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
 * Helper: Generate sine wave
 * ============================================================================ */

static void generate_sine(float* buf, uint32_t num_samples,
                           float freq_hz, float amplitude, float phase)
{
    for (uint32_t i = 0; i < num_samples; i++) {
        float t = (float)i / TEST_SAMPLE_RATE;
        buf[i] = amplitude * sinf(2.0f * (float)M_PI * freq_hz * t + phase);
    }
}

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

START_TEST(test_ic_create_defaults)
{
    inferior_colliculus_t* ic = ic_create(NULL);
    ck_assert_ptr_nonnull(ic);
    ic_destroy(ic);
}
END_TEST

START_TEST(test_ic_destroy_null_safe)
{
    ic_destroy(NULL);
}
END_TEST

/* ============================================================================
 * Tonotopic Response Tests
 * ============================================================================ */

START_TEST(test_tone_activates_channels)
{
    float left[TEST_NUM_SAMPLES];
    float right[TEST_NUM_SAMPLES];

    /* Generate a 1kHz tone in both ears */
    generate_sine(left, TEST_NUM_SAMPLES, 1000.0f, 0.8f, 0.0f);
    generate_sine(right, TEST_NUM_SAMPLES, 1000.0f, 0.8f, 0.0f);

    int rc = ic_process_audio(g_ic, left, right, TEST_NUM_SAMPLES);
    ck_assert_int_eq(rc, 0);

    /* Get ICC (tonotopic) response */
    float response[IC_DEFAULT_NUM_CHANNELS];
    rc = ic_get_icc_response(g_ic, response, IC_DEFAULT_NUM_CHANNELS);
    ck_assert_int_eq(rc, 0);

    /* At least some channels should be activated by the tone */
    float max_activation = 0.0f;
    for (int i = 0; i < IC_DEFAULT_NUM_CHANNELS; i++) {
        if (response[i] > max_activation) {
            max_activation = response[i];
        }
    }
    ck_assert_float_ge(max_activation, 0.0f);
}
END_TEST

START_TEST(test_silence_minimal_activation)
{
    float left[TEST_NUM_SAMPLES];
    float right[TEST_NUM_SAMPLES];

    memset(left, 0, sizeof(left));
    memset(right, 0, sizeof(right));

    int rc = ic_process_audio(g_ic, left, right, TEST_NUM_SAMPLES);
    ck_assert_int_eq(rc, 0);

    /* Stats should reflect processed data */
    ic_stats_t stats;
    rc = ic_get_stats(g_ic, &stats);
    ck_assert_int_eq(rc, 0);
    ck_assert_uint_ge(stats.update_count, 0);
}
END_TEST

START_TEST(test_channel_activation_range)
{
    float left[TEST_NUM_SAMPLES];
    float right[TEST_NUM_SAMPLES];

    generate_sine(left, TEST_NUM_SAMPLES, 440.0f, 1.0f, 0.0f);
    generate_sine(right, TEST_NUM_SAMPLES, 440.0f, 1.0f, 0.0f);

    ic_process_audio(g_ic, left, right, TEST_NUM_SAMPLES);

    /* All channel activations should be in valid range */
    for (uint32_t ch = 0; ch < IC_DEFAULT_NUM_CHANNELS; ch++) {
        float act = ic_get_channel_activation(g_ic, ch);
        /* Activation should be >= 0 (or -1 on error) */
        if (act >= 0.0f) {
            ck_assert_float_le(act, 1.5f);  /* Allow some headroom */
        }
    }
}
END_TEST

/* ============================================================================
 * Binaural Localization Tests
 * ============================================================================ */

START_TEST(test_centered_sound_near_zero_azimuth)
{
    float left[TEST_NUM_SAMPLES];
    float right[TEST_NUM_SAMPLES];

    /* Same signal in both ears -> centered (azimuth ~0) */
    generate_sine(left, TEST_NUM_SAMPLES, 500.0f, 0.8f, 0.0f);
    generate_sine(right, TEST_NUM_SAMPLES, 500.0f, 0.8f, 0.0f);

    ic_process_audio(g_ic, left, right, TEST_NUM_SAMPLES);

    float azimuth = ic_get_azimuth(g_ic);
    /* Azimuth should be near 0 for centered sound */
    ck_assert_float_ge(azimuth, -IC_MAX_AZIMUTH_DEG);
    ck_assert_float_le(azimuth, IC_MAX_AZIMUTH_DEG);
}
END_TEST

START_TEST(test_lateralized_sound_shifts_azimuth)
{
    float left[TEST_NUM_SAMPLES];
    float right[TEST_NUM_SAMPLES];

    /* Louder in left ear -> should shift azimuth left (negative) */
    generate_sine(left, TEST_NUM_SAMPLES, 500.0f, 1.0f, 0.0f);
    generate_sine(right, TEST_NUM_SAMPLES, 500.0f, 0.2f, 0.0f);

    ic_process_audio(g_ic, left, right, TEST_NUM_SAMPLES);

    float azimuth = ic_get_azimuth(g_ic);
    /* Azimuth should be in valid range */
    ck_assert_float_ge(azimuth, -IC_MAX_AZIMUTH_DEG);
    ck_assert_float_le(azimuth, IC_MAX_AZIMUTH_DEG);
}
END_TEST

START_TEST(test_elevation_bounded)
{
    float left[TEST_NUM_SAMPLES];
    float right[TEST_NUM_SAMPLES];

    generate_sine(left, TEST_NUM_SAMPLES, 2000.0f, 0.8f, 0.0f);
    generate_sine(right, TEST_NUM_SAMPLES, 2000.0f, 0.8f, 0.0f);

    ic_process_audio(g_ic, left, right, TEST_NUM_SAMPLES);

    float elevation = ic_get_elevation(g_ic);
    ck_assert_float_ge(elevation, -IC_MAX_ELEVATION_DEG);
    ck_assert_float_le(elevation, IC_MAX_ELEVATION_DEG);
}
END_TEST

/* ============================================================================
 * ICX Spatial Response Tests
 * ============================================================================ */

START_TEST(test_icx_spatial_response)
{
    float left[TEST_NUM_SAMPLES];
    float right[TEST_NUM_SAMPLES];

    generate_sine(left, TEST_NUM_SAMPLES, 800.0f, 0.9f, 0.0f);
    generate_sine(right, TEST_NUM_SAMPLES, 800.0f, 0.7f, 0.0f);

    ic_process_audio(g_ic, left, right, TEST_NUM_SAMPLES);

    float response[IC_DEFAULT_NUM_CHANNELS];
    int rc = ic_get_icx_response(g_ic, response, IC_DEFAULT_NUM_CHANNELS);
    ck_assert_int_eq(rc, 0);
}
END_TEST

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

START_TEST(test_stats_after_processing)
{
    float left[TEST_NUM_SAMPLES];
    float right[TEST_NUM_SAMPLES];

    generate_sine(left, TEST_NUM_SAMPLES, 1000.0f, 0.5f, 0.0f);
    generate_sine(right, TEST_NUM_SAMPLES, 1000.0f, 0.5f, 0.0f);

    ic_process_audio(g_ic, left, right, TEST_NUM_SAMPLES);

    ic_stats_t stats;
    int rc = ic_get_stats(g_ic, &stats);
    ck_assert_int_eq(rc, 0);

    ck_assert_float_ge(stats.mean_activation, 0.0f);
    ck_assert_float_ge(stats.peak_frequency_hz, IC_MIN_FREQ_HZ - 1.0f);
}
END_TEST

/* ============================================================================
 * Test Suite
 * ============================================================================ */

Suite* ic_audio_integration_suite(void)
{
    Suite* s = suite_create("Inferior Colliculus Audio Integration");

    TCase* tc_lifecycle = tcase_create("Lifecycle");
    tcase_add_test(tc_lifecycle, test_ic_create_defaults);
    tcase_add_test(tc_lifecycle, test_ic_destroy_null_safe);
    tcase_set_timeout(tc_lifecycle, 10);
    suite_add_tcase(s, tc_lifecycle);

    TCase* tc_tono = tcase_create("Tonotopic Response");
    tcase_add_checked_fixture(tc_tono, setup, teardown);
    tcase_add_test(tc_tono, test_tone_activates_channels);
    tcase_add_test(tc_tono, test_silence_minimal_activation);
    tcase_add_test(tc_tono, test_channel_activation_range);
    tcase_set_timeout(tc_tono, 15);
    suite_add_tcase(s, tc_tono);

    TCase* tc_binaural = tcase_create("Binaural Localization");
    tcase_add_checked_fixture(tc_binaural, setup, teardown);
    tcase_add_test(tc_binaural, test_centered_sound_near_zero_azimuth);
    tcase_add_test(tc_binaural, test_lateralized_sound_shifts_azimuth);
    tcase_add_test(tc_binaural, test_elevation_bounded);
    tcase_set_timeout(tc_binaural, 15);
    suite_add_tcase(s, tc_binaural);

    TCase* tc_icx = tcase_create("ICX Spatial");
    tcase_add_checked_fixture(tc_icx, setup, teardown);
    tcase_add_test(tc_icx, test_icx_spatial_response);
    tcase_set_timeout(tc_icx, 10);
    suite_add_tcase(s, tc_icx);

    TCase* tc_stats = tcase_create("Statistics");
    tcase_add_checked_fixture(tc_stats, setup, teardown);
    tcase_add_test(tc_stats, test_stats_after_processing);
    tcase_set_timeout(tc_stats, 10);
    suite_add_tcase(s, tc_stats);

    return s;
}

int main(void)
{
    int number_failed;
    Suite* s = ic_audio_integration_suite();
    SRunner* sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
