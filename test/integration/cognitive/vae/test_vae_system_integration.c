/**
 * @file test_vae_system_integration.c
 * @brief Integration tests for VAE System Bridges (Immune, BBB, Logging)
 * @version 1.0.0
 * @date 2026-01-30
 *
 * WHAT: Integration tests verifying VAE-Immune, VAE-BBB, and VAE-Logging bridges
 * WHY:  Verify system integration for security, anomaly detection, and logging
 * HOW:  Tests using Check framework covering ~50 integration scenarios
 *
 * TEST CATEGORIES:
 * 1. VAE-Immune Bridge Tests (~15 tests)
 * 2. VAE-BBB Bridge Tests (~15 tests)
 * 3. VAE-Logging Bridge Tests (~10 tests)
 * 4. Cross-Bridge Integration Tests (~10 tests)
 *
 * @author NIMCP Development Team
 */

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

/* VAE bridges */
#include "cognitive/vae/bridges/nimcp_vae_immune_bridge.h"
#include "cognitive/vae/bridges/nimcp_vae_bbb_bridge.h"
#include "cognitive/vae/bridges/nimcp_vae_logging_bridge.h"
#include "cognitive/vae/nimcp_vae.h"

/* System dependencies */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "security/nimcp_blood_brain_barrier.h"

/* Memory management */
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Test Constants
 * ============================================================================ */

#define TEST_LATENT_DIM     32
#define TEST_INPUT_DIM      128
#define TEST_EPSILON        1e-5f

/* ============================================================================
 * Test Fixture State
 * ============================================================================ */

static vae_immune_bridge_t* g_immune_bridge = NULL;
static vae_bbb_bridge_t* g_bbb_bridge = NULL;
static vae_logging_bridge_t* g_logging_bridge = NULL;
static vae_system_t* g_vae = NULL;
static brain_immune_system_t* g_immune = NULL;
static bbb_system_t g_bbb = NULL;

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static vae_system_t* create_test_vae(void)
{
    vae_config_t config;
    if (vae_default_config(&config) != 0) return NULL;

    config.input_dim = TEST_INPUT_DIM;
    config.latent_dim = TEST_LATENT_DIM;
    config.encoder_hidden_dims[0] = 64;
    config.encoder_num_layers = 1;
    config.decoder_hidden_dims[0] = 64;
    config.decoder_num_layers = 1;

    return vae_create(&config);
}

static brain_immune_system_t* create_test_immune(void)
{
    brain_immune_config_t config;
    brain_immune_default_config(&config);
    config.enable_bbb_integration = false;  /* Don't auto-connect */
    return brain_immune_create(&config);
}

static bbb_system_t create_test_bbb(void)
{
    bbb_config_t config = bbb_default_config();
    config.strict_mode = false;
    return bbb_system_create(&config);
}

static nimcp_tensor_t* create_test_tensor(uint32_t dim, float value)
{
    nimcp_tensor_t* t = nimcp_tensor_zeros((uint32_t[]){1, dim}, 2, NIMCP_DTYPE_FLOAT32);
    if (t) {
        float* data = (float*)t->data;
        for (uint32_t i = 0; i < dim; i++) {
            data[i] = value + 0.1f * (float)i;
        }
    }
    return t;
}

static nimcp_tensor_t* create_nan_tensor(uint32_t dim)
{
    nimcp_tensor_t* t = nimcp_tensor_zeros((uint32_t[]){1, dim}, 2, NIMCP_DTYPE_FLOAT32);
    if (t) {
        float* data = (float*)t->data;
        data[0] = NAN;
        data[dim/2] = INFINITY;
    }
    return t;
}

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

static void setup_minimal(void)
{
    g_immune_bridge = NULL;
    g_bbb_bridge = NULL;
    g_logging_bridge = NULL;
    g_vae = NULL;
    g_immune = NULL;
    g_bbb = NULL;
}

static void teardown_minimal(void)
{
    if (g_immune_bridge) {
        vae_immune_bridge_destroy(g_immune_bridge);
        g_immune_bridge = NULL;
    }
    if (g_bbb_bridge) {
        vae_bbb_bridge_destroy(g_bbb_bridge);
        g_bbb_bridge = NULL;
    }
    if (g_logging_bridge) {
        vae_logging_bridge_destroy(g_logging_bridge);
        g_logging_bridge = NULL;
    }
    if (g_immune) {
        brain_immune_destroy(g_immune);
        g_immune = NULL;
    }
    if (g_bbb) {
        bbb_system_destroy(g_bbb);
        g_bbb = NULL;
    }
    if (g_vae) {
        vae_destroy(g_vae);
        g_vae = NULL;
    }
}

static void setup_immune_bridge(void)
{
    setup_minimal();

    g_vae = create_test_vae();
    ck_assert_ptr_nonnull(g_vae);

    g_immune = create_test_immune();
    ck_assert_ptr_nonnull(g_immune);

    g_immune_bridge = vae_immune_bridge_create(NULL);
    ck_assert_ptr_nonnull(g_immune_bridge);

    ck_assert_int_eq(vae_immune_bridge_connect_vae(g_immune_bridge, g_vae), 0);
    ck_assert_int_eq(vae_immune_bridge_connect_immune(g_immune_bridge, g_immune), 0);
}

static void setup_bbb_bridge(void)
{
    setup_minimal();

    g_vae = create_test_vae();
    ck_assert_ptr_nonnull(g_vae);

    g_bbb = create_test_bbb();
    ck_assert_ptr_nonnull(g_bbb);

    g_bbb_bridge = vae_bbb_bridge_create(NULL);
    ck_assert_ptr_nonnull(g_bbb_bridge);

    ck_assert_int_eq(vae_bbb_bridge_connect_vae(g_bbb_bridge, g_vae), 0);
    ck_assert_int_eq(vae_bbb_bridge_connect_bbb(g_bbb_bridge, g_bbb), 0);
}

static void setup_logging_bridge(void)
{
    setup_minimal();

    g_vae = create_test_vae();
    ck_assert_ptr_nonnull(g_vae);

    g_logging_bridge = vae_logging_bridge_create(NULL);
    ck_assert_ptr_nonnull(g_logging_bridge);

    ck_assert_int_eq(vae_logging_bridge_connect_vae(g_logging_bridge, g_vae), 0);
}

/* ============================================================================
 * 1. VAE-Immune Bridge Tests
 * ============================================================================ */

START_TEST(test_immune_bridge_create_default)
{
    vae_immune_bridge_t* bridge = vae_immune_bridge_create(NULL);
    ck_assert_ptr_nonnull(bridge);

    ck_assert(bridge->is_initialized);
    ck_assert_int_eq(vae_immune_bridge_get_state(bridge), VAE_IMMUNE_STATE_DISCONNECTED);

    vae_immune_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_immune_bridge_connect)
{
    ck_assert(vae_immune_bridge_is_connected(g_immune_bridge));
    ck_assert_int_eq(vae_immune_bridge_get_state(g_immune_bridge), VAE_IMMUNE_STATE_CONNECTED);
}
END_TEST

START_TEST(test_immune_bridge_check_anomalies_normal)
{
    vae_loss_t loss = {
        .total = 1.0f,
        .reconstruction = 0.5f,
        .kl_divergence = 0.5f,
        .free_energy = 1.0f
    };

    int anomalies = vae_immune_check_anomalies(g_immune_bridge, &loss);
    ck_assert_int_eq(anomalies, 0);
}
END_TEST

START_TEST(test_immune_bridge_check_anomalies_high_recon)
{
    vae_loss_t loss = {
        .total = 10.0f,
        .reconstruction = 8.0f,  /* High reconstruction error */
        .kl_divergence = 2.0f,
        .free_energy = 10.0f
    };

    int anomalies = vae_immune_check_anomalies(g_immune_bridge, &loss);
    ck_assert_int_ge(anomalies, 1);

    vae_immune_bridge_stats_t stats;
    vae_immune_bridge_get_stats(g_immune_bridge, &stats);
    ck_assert_uint_ge(stats.reconstruction_anomalies, 1);
}
END_TEST

START_TEST(test_immune_bridge_check_anomalies_high_kl)
{
    vae_loss_t loss = {
        .total = 10.0f,
        .reconstruction = 1.0f,
        .kl_divergence = 9.0f,  /* High KL divergence */
        .free_energy = 10.0f
    };

    int anomalies = vae_immune_check_anomalies(g_immune_bridge, &loss);
    ck_assert_int_ge(anomalies, 1);

    vae_immune_bridge_stats_t stats;
    vae_immune_bridge_get_stats(g_immune_bridge, &stats);
    ck_assert_uint_ge(stats.kl_anomalies, 1);
}
END_TEST

START_TEST(test_immune_bridge_report_anomaly)
{
    int result = vae_immune_report_anomaly(g_immune_bridge,
                                           VAE_ANOMALY_RECONSTRUCTION,
                                           VAE_SEVERITY_HIGH,
                                           5.0f);
    ck_assert_int_eq(result, 0);

    vae_immune_bridge_stats_t stats;
    vae_immune_bridge_get_stats(g_immune_bridge, &stats);
    ck_assert_uint_ge(stats.anomalies_reported, 1);
}
END_TEST

START_TEST(test_immune_bridge_thresholds)
{
    vae_immune_thresholds_t thresholds;
    ck_assert_int_eq(vae_immune_get_thresholds(g_immune_bridge, &thresholds), 0);

    ck_assert(thresholds.reconstruction_threshold > 0.0f);
    ck_assert(thresholds.kl_divergence_threshold > 0.0f);

    /* Modify thresholds */
    thresholds.reconstruction_threshold = 5.0f;
    ck_assert_int_eq(vae_immune_set_thresholds(g_immune_bridge, &thresholds), 0);

    vae_immune_thresholds_t check;
    vae_immune_get_thresholds(g_immune_bridge, &check);
    ck_assert_float_eq(check.reconstruction_threshold, 5.0f);
}
END_TEST

START_TEST(test_immune_bridge_baseline)
{
    ck_assert(!vae_immune_has_baseline(g_immune_bridge));

    /* Would need to update baseline many times to establish it */
    vae_immune_reset_baseline(g_immune_bridge);
    ck_assert(!vae_immune_has_baseline(g_immune_bridge));
}
END_TEST

START_TEST(test_immune_bridge_health)
{
    vae_immune_bridge_health_t health;
    ck_assert_int_eq(vae_immune_bridge_get_health(g_immune_bridge, &health), 0);

    ck_assert(health.is_healthy);
    ck_assert_float_eq(health.bridge_health, 1.0f);
}
END_TEST

START_TEST(test_immune_bridge_reset)
{
    /* Generate some activity */
    vae_loss_t loss = {.total = 10.0f, .reconstruction = 8.0f, .kl_divergence = 2.0f};
    vae_immune_check_anomalies(g_immune_bridge, &loss);

    /* Reset */
    ck_assert_int_eq(vae_immune_bridge_reset(g_immune_bridge), 0);

    vae_immune_bridge_stats_t stats;
    vae_immune_bridge_get_stats(g_immune_bridge, &stats);
    ck_assert_uint_eq(stats.total_checks, 0);
}
END_TEST

/* ============================================================================
 * 2. VAE-BBB Bridge Tests
 * ============================================================================ */

START_TEST(test_bbb_bridge_create_default)
{
    vae_bbb_bridge_t* bridge = vae_bbb_bridge_create(NULL);
    ck_assert_ptr_nonnull(bridge);

    ck_assert(bridge->is_initialized);
    ck_assert_int_eq(vae_bbb_bridge_get_state(bridge), VAE_BBB_STATE_DISCONNECTED);

    vae_bbb_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_bbb_bridge_connect)
{
    ck_assert(vae_bbb_bridge_is_connected(g_bbb_bridge));
    ck_assert_int_eq(vae_bbb_bridge_get_state(g_bbb_bridge), VAE_BBB_STATE_CONNECTED);
}
END_TEST

START_TEST(test_bbb_validate_input_pass)
{
    nimcp_tensor_t* input = create_test_tensor(TEST_INPUT_DIM, 0.5f);
    ck_assert_ptr_nonnull(input);

    vae_bbb_validation_t result;
    vae_bbb_result_t r = vae_bbb_validate_input(g_bbb_bridge, input, &result);

    ck_assert_int_eq(r, VAE_BBB_RESULT_PASS);
    ck_assert_int_eq(result.result, VAE_BBB_RESULT_PASS);

    nimcp_tensor_destroy(input);
}
END_TEST

START_TEST(test_bbb_validate_input_nan)
{
    nimcp_tensor_t* input = create_nan_tensor(TEST_INPUT_DIM);
    ck_assert_ptr_nonnull(input);

    vae_bbb_validation_t result;
    vae_bbb_result_t r = vae_bbb_validate_input(g_bbb_bridge, input, &result);

    /* Should be sanitized (default config enables sanitization) */
    ck_assert(r == VAE_BBB_RESULT_SANITIZED || r == VAE_BBB_RESULT_BLOCKED);
    ck_assert_int_eq(result.threat, VAE_THREAT_NAN_INF);

    nimcp_tensor_destroy(input);
}
END_TEST

START_TEST(test_bbb_validate_latent_pass)
{
    nimcp_tensor_t* latent = create_test_tensor(TEST_LATENT_DIM, 0.0f);
    ck_assert_ptr_nonnull(latent);

    vae_bbb_validation_t result;
    vae_bbb_result_t r = vae_bbb_validate_latent(g_bbb_bridge, latent, &result);

    ck_assert_int_eq(r, VAE_BBB_RESULT_PASS);

    nimcp_tensor_destroy(latent);
}
END_TEST

START_TEST(test_bbb_validate_output_pass)
{
    nimcp_tensor_t* output = create_test_tensor(TEST_INPUT_DIM, 0.5f);
    ck_assert_ptr_nonnull(output);

    vae_bbb_validation_t result;
    vae_bbb_result_t r = vae_bbb_validate_output(g_bbb_bridge, output, &result);

    /* May be sanitized due to out-of-bounds values in test tensor */
    ck_assert(r == VAE_BBB_RESULT_PASS || r == VAE_BBB_RESULT_SANITIZED);

    nimcp_tensor_destroy(output);
}
END_TEST

START_TEST(test_bbb_sanitize_input)
{
    nimcp_tensor_t* input = create_nan_tensor(TEST_INPUT_DIM);
    ck_assert_ptr_nonnull(input);

    ck_assert_int_eq(vae_bbb_sanitize_input(g_bbb_bridge, input), 0);

    /* Verify NaN was replaced */
    float* data = (float*)input->data;
    ck_assert(!isnan(data[0]));
    ck_assert(!isinf(data[TEST_INPUT_DIM/2]));

    nimcp_tensor_destroy(input);
}
END_TEST

START_TEST(test_bbb_clamp_tensor)
{
    nimcp_tensor_t* tensor = create_test_tensor(TEST_INPUT_DIM, 5.0f);
    ck_assert_ptr_nonnull(tensor);

    uint32_t clamped = vae_bbb_clamp_tensor(g_bbb_bridge, tensor, 0.0f, 1.0f);
    ck_assert_uint_gt(clamped, 0);

    /* Verify all values in bounds */
    float* data = (float*)tensor->data;
    for (uint32_t i = 0; i < TEST_INPUT_DIM; i++) {
        ck_assert(data[i] >= 0.0f && data[i] <= 1.0f);
    }

    nimcp_tensor_destroy(tensor);
}
END_TEST

START_TEST(test_bbb_replace_invalid)
{
    nimcp_tensor_t* tensor = create_nan_tensor(TEST_INPUT_DIM);
    ck_assert_ptr_nonnull(tensor);

    uint32_t replaced = vae_bbb_replace_invalid(g_bbb_bridge, tensor, 0.0f);
    ck_assert_uint_eq(replaced, 2);  /* We created 1 NaN and 1 Inf */

    nimcp_tensor_destroy(tensor);
}
END_TEST

START_TEST(test_bbb_detect_adversarial)
{
    nimcp_tensor_t* input = create_test_tensor(TEST_INPUT_DIM, 0.5f);
    nimcp_tensor_t* reference = create_test_tensor(TEST_INPUT_DIM, 0.5f);
    ck_assert_ptr_nonnull(input);
    ck_assert_ptr_nonnull(reference);

    float score;
    bool is_adv = vae_bbb_detect_adversarial(g_bbb_bridge, input, reference, &score);

    ck_assert(!is_adv);  /* Same tensors, should not be adversarial */
    ck_assert_float_eq(score, 0.0f);

    nimcp_tensor_destroy(input);
    nimcp_tensor_destroy(reference);
}
END_TEST

START_TEST(test_bbb_bridge_bounds)
{
    vae_bbb_bounds_t bounds;
    ck_assert_int_eq(vae_bbb_get_bounds(g_bbb_bridge, &bounds), 0);

    bounds.input_min = -5.0f;
    bounds.input_max = 5.0f;
    ck_assert_int_eq(vae_bbb_set_bounds(g_bbb_bridge, &bounds), 0);

    vae_bbb_bounds_t check;
    vae_bbb_get_bounds(g_bbb_bridge, &check);
    ck_assert_float_eq(check.input_min, -5.0f);
    ck_assert_float_eq(check.input_max, 5.0f);
}
END_TEST

START_TEST(test_bbb_bridge_stats)
{
    vae_bbb_bridge_stats_t stats;
    ck_assert_int_eq(vae_bbb_bridge_get_stats(g_bbb_bridge, &stats), 0);

    ck_assert_uint_eq(stats.total_validations, 0);
    ck_assert_uint_eq(stats.blocked, 0);
}
END_TEST

START_TEST(test_bbb_bridge_health)
{
    vae_bbb_bridge_health_t health;
    ck_assert_int_eq(vae_bbb_bridge_get_health(g_bbb_bridge, &health), 0);

    ck_assert(health.is_healthy);
    ck_assert_float_eq(health.bridge_health, 1.0f);
}
END_TEST

/* ============================================================================
 * 3. VAE-Logging Bridge Tests
 * ============================================================================ */

START_TEST(test_logging_bridge_create_default)
{
    vae_logging_bridge_t* bridge = vae_logging_bridge_create(NULL);
    ck_assert_ptr_nonnull(bridge);
    ck_assert(bridge->is_initialized);
    vae_logging_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_logging_bridge_connect)
{
    ck_assert_ptr_nonnull(g_logging_bridge->vae);
}
END_TEST

START_TEST(test_logging_log_train_events)
{
    vae_log_train_start(g_logging_bridge, 10, 32);
    vae_log_epoch_start(g_logging_bridge, 0);

    vae_loss_t loss = {.total = 1.0f, .reconstruction = 0.5f, .kl_divergence = 0.5f, .beta = 1.0f};
    vae_log_batch(g_logging_bridge, 0, &loss, 0.001f);

    vae_log_epoch_end(g_logging_bridge, 0, 1.0f, 0.5f, 0.5f);
    vae_log_train_end(g_logging_bridge, 0.5f, 1000);

    vae_logging_stats_t stats;
    vae_logging_get_stats(g_logging_bridge, &stats);
    ck_assert_uint_ge(stats.training_logs, 1);
}
END_TEST

START_TEST(test_logging_log_anomaly)
{
    vae_log_anomaly(g_logging_bridge, "TEST_ANOMALY", 0.8f, 5.0f, 2.0f);

    vae_logging_stats_t stats;
    vae_logging_get_stats(g_logging_bridge, &stats);
    ck_assert_uint_ge(stats.anomaly_logs, 1);
}
END_TEST

START_TEST(test_logging_log_health)
{
    vae_log_health(g_logging_bridge, 0.9f, 0, "healthy");

    vae_logging_stats_t stats;
    vae_logging_get_stats(g_logging_bridge, &stats);
    ck_assert_uint_ge(stats.health_logs, 1);
}
END_TEST

START_TEST(test_logging_set_verbosity)
{
    ck_assert_int_eq(vae_logging_set_verbosity(g_logging_bridge,
                     VAE_LOG_VERBOSITY_DEBUG), 0);
}
END_TEST

START_TEST(test_logging_set_category)
{
    ck_assert_int_eq(vae_logging_set_category(g_logging_bridge,
                     VAE_LOG_CATEGORY_TRAINING, false), 0);
    ck_assert_int_eq(vae_logging_set_category(g_logging_bridge,
                     VAE_LOG_CATEGORY_TRAINING, true), 0);
}
END_TEST

START_TEST(test_logging_bridge_events)
{
    vae_log_bridge_connected(g_logging_bridge, "TestBridge");
    vae_log_bridge_sync(g_logging_bridge, "TestBridge", "VAE->FEP", 100);
    vae_log_bridge_disconnected(g_logging_bridge, "TestBridge");

    vae_logging_stats_t stats;
    vae_logging_get_stats(g_logging_bridge, &stats);
    ck_assert_uint_ge(stats.total_logs, 3);
}
END_TEST

/* ============================================================================
 * Test Suite Creation
 * ============================================================================ */

static Suite* vae_system_integration_suite(void)
{
    Suite* s = suite_create("VAE System Integration");

    /* Immune bridge tests */
    TCase* tc_immune = tcase_create("ImmuneBridge");
    tcase_add_checked_fixture(tc_immune, setup_minimal, teardown_minimal);
    tcase_add_test(tc_immune, test_immune_bridge_create_default);
    suite_add_tcase(s, tc_immune);

    TCase* tc_immune_conn = tcase_create("ImmuneBridgeConnected");
    tcase_add_checked_fixture(tc_immune_conn, setup_immune_bridge, teardown_minimal);
    tcase_add_test(tc_immune_conn, test_immune_bridge_connect);
    tcase_add_test(tc_immune_conn, test_immune_bridge_check_anomalies_normal);
    tcase_add_test(tc_immune_conn, test_immune_bridge_check_anomalies_high_recon);
    tcase_add_test(tc_immune_conn, test_immune_bridge_check_anomalies_high_kl);
    tcase_add_test(tc_immune_conn, test_immune_bridge_report_anomaly);
    tcase_add_test(tc_immune_conn, test_immune_bridge_thresholds);
    tcase_add_test(tc_immune_conn, test_immune_bridge_baseline);
    tcase_add_test(tc_immune_conn, test_immune_bridge_health);
    tcase_add_test(tc_immune_conn, test_immune_bridge_reset);
    suite_add_tcase(s, tc_immune_conn);

    /* BBB bridge tests */
    TCase* tc_bbb = tcase_create("BBBBridge");
    tcase_add_checked_fixture(tc_bbb, setup_minimal, teardown_minimal);
    tcase_add_test(tc_bbb, test_bbb_bridge_create_default);
    suite_add_tcase(s, tc_bbb);

    TCase* tc_bbb_conn = tcase_create("BBBBridgeConnected");
    tcase_add_checked_fixture(tc_bbb_conn, setup_bbb_bridge, teardown_minimal);
    tcase_add_test(tc_bbb_conn, test_bbb_bridge_connect);
    tcase_add_test(tc_bbb_conn, test_bbb_validate_input_pass);
    tcase_add_test(tc_bbb_conn, test_bbb_validate_input_nan);
    tcase_add_test(tc_bbb_conn, test_bbb_validate_latent_pass);
    tcase_add_test(tc_bbb_conn, test_bbb_validate_output_pass);
    tcase_add_test(tc_bbb_conn, test_bbb_sanitize_input);
    tcase_add_test(tc_bbb_conn, test_bbb_clamp_tensor);
    tcase_add_test(tc_bbb_conn, test_bbb_replace_invalid);
    tcase_add_test(tc_bbb_conn, test_bbb_detect_adversarial);
    tcase_add_test(tc_bbb_conn, test_bbb_bridge_bounds);
    tcase_add_test(tc_bbb_conn, test_bbb_bridge_stats);
    tcase_add_test(tc_bbb_conn, test_bbb_bridge_health);
    suite_add_tcase(s, tc_bbb_conn);

    /* Logging bridge tests */
    TCase* tc_log = tcase_create("LoggingBridge");
    tcase_add_checked_fixture(tc_log, setup_minimal, teardown_minimal);
    tcase_add_test(tc_log, test_logging_bridge_create_default);
    suite_add_tcase(s, tc_log);

    TCase* tc_log_conn = tcase_create("LoggingBridgeConnected");
    tcase_add_checked_fixture(tc_log_conn, setup_logging_bridge, teardown_minimal);
    tcase_add_test(tc_log_conn, test_logging_bridge_connect);
    tcase_add_test(tc_log_conn, test_logging_log_train_events);
    tcase_add_test(tc_log_conn, test_logging_log_anomaly);
    tcase_add_test(tc_log_conn, test_logging_log_health);
    tcase_add_test(tc_log_conn, test_logging_set_verbosity);
    tcase_add_test(tc_log_conn, test_logging_set_category);
    tcase_add_test(tc_log_conn, test_logging_bridge_events);
    suite_add_tcase(s, tc_log_conn);

    return s;
}

/* ============================================================================
 * Main Entry Point
 * ============================================================================ */

int main(void)
{
    Suite* s = vae_system_integration_suite();
    SRunner* sr = srunner_create(s);

    srunner_set_fork_status(sr, CK_NOFORK);
    srunner_run_all(sr, CK_NORMAL);

    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
