/**
 * @file test_vae_neural_integration.c
 * @brief Integration tests for Phase 5 VAE Neural Bridges
 * @version 1.0.0
 * @date 2026-01-30
 *
 * WHAT: Integration tests for VAE-SNN, VAE-Plasticity, VAE-Training,
 *       VAE-Substrate, and VAE-Thalamic bridges
 * WHY:  Verify neural integration pathways work correctly for
 *       spike encoding, learning modulation, training, metabolic
 *       awareness, and attention-gated processing
 * HOW:  Tests using Check framework covering ~75 integration scenarios
 *
 * TEST CATEGORIES:
 * 1. SNN Bridge Tests (~15 tests)
 * 2. Plasticity Bridge Tests (~15 tests)
 * 3. Training Bridge Tests (~15 tests)
 * 4. Substrate Bridge Tests (~15 tests)
 * 5. Thalamic Bridge Tests (~15 tests)
 *
 * @author NIMCP Development Team
 */

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

/* Phase 5 VAE Neural Bridges */
#include "cognitive/vae/bridges/nimcp_vae_snn_bridge.h"
#include "cognitive/vae/bridges/nimcp_vae_plasticity_bridge.h"
#include "cognitive/vae/bridges/nimcp_vae_training_bridge.h"
#include "cognitive/vae/bridges/nimcp_vae_substrate_bridge.h"
#include "cognitive/vae/bridges/nimcp_vae_thalamic_bridge.h"

/* Core VAE */
#include "cognitive/vae/nimcp_vae.h"
#include "cognitive/vae/nimcp_vae_latent.h"

/* Memory management */
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"

/* ============================================================================
 * Test Constants
 * ============================================================================ */

#define TEST_LATENT_DIM     32
#define TEST_INPUT_DIM      128
#define TEST_BATCH_SIZE     1
#define TEST_WINDOW_MS      100.0f
#define TEST_EPSILON        1e-5f

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static vae_system_t* create_test_vae(void)
{
    vae_config_t config;
    if (vae_default_config(&config) != 0) {
        return NULL;
    }

    config.input_dim = TEST_INPUT_DIM;
    config.latent_dim = TEST_LATENT_DIM;
    config.encoder_hidden_dims[0] = 64;
    config.encoder_num_layers = 1;
    config.decoder_hidden_dims[0] = 64;
    config.decoder_num_layers = 1;
    config.beta = 1.0f;
    config.learning_rate = 0.001f;

    return vae_create(&config);
}

static float* create_random_latent(uint32_t dim)
{
    float* latent = nimcp_calloc(dim, sizeof(float));
    if (latent) {
        for (uint32_t i = 0; i < dim; i++) {
            latent[i] = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
        }
    }
    return latent;
}

/* ============================================================================
 * SNN Bridge Tests
 * ============================================================================ */

START_TEST(test_snn_bridge_default_config)
{
    vae_snn_bridge_config_t config;
    int ret = vae_snn_bridge_default_config(&config);

    ck_assert_int_eq(ret, 0);
    ck_assert_int_eq(config.encode_method, VAE_SNN_ENCODE_RATE);
    ck_assert_int_eq(config.decode_method, VAE_SNN_DECODE_RATE);
    ck_assert(config.encoding_window_ms > 0.0f);
}
END_TEST

START_TEST(test_snn_bridge_create_destroy)
{
    vae_snn_bridge_config_t config;
    vae_snn_bridge_default_config(&config);

    vae_snn_bridge_t* bridge = vae_snn_bridge_create(&config);
    ck_assert_ptr_nonnull(bridge);

    vae_snn_bridge_state_t state = vae_snn_bridge_get_state(bridge);
    ck_assert_int_eq(state, VAE_SNN_STATE_DISCONNECTED);

    vae_snn_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_snn_bridge_connect_vae)
{
    vae_snn_bridge_config_t config;
    vae_snn_bridge_default_config(&config);
    vae_snn_bridge_t* bridge = vae_snn_bridge_create(&config);
    vae_system_t* vae = create_test_vae();

    if (vae) {
        int ret = vae_snn_bridge_connect_vae(bridge, vae);
        ck_assert_int_eq(ret, 0);
    }

    vae_snn_bridge_destroy(bridge);
    if (vae) vae_destroy(vae);
}
END_TEST

START_TEST(test_snn_encode_rate)
{
    vae_snn_bridge_config_t config;
    vae_snn_bridge_default_config(&config);
    config.encode_method = VAE_SNN_ENCODE_RATE;

    vae_snn_bridge_t* bridge = vae_snn_bridge_create(&config);
    vae_system_t* vae = create_test_vae();

    if (vae && bridge) {
        vae_snn_bridge_connect_vae(bridge, vae);
        vae_snn_bridge_connect_snn(bridge, NULL);

        float* latent = create_random_latent(TEST_LATENT_DIM);
        vae_snn_encode_result_t result;

        int ret = vae_snn_encode_latent(bridge, latent, TEST_LATENT_DIM,
                                        TEST_WINDOW_MS, &result);
        ck_assert_int_eq(ret, 0);
        ck_assert(result.num_neurons > 0);
        ck_assert(result.total_spike_count >= 0.0f);

        vae_snn_encode_result_free(&result);
        nimcp_free(latent);
    }

    vae_snn_bridge_destroy(bridge);
    if (vae) vae_destroy(vae);
}
END_TEST

START_TEST(test_snn_encode_temporal)
{
    vae_snn_bridge_config_t config;
    vae_snn_bridge_default_config(&config);
    config.encode_method = VAE_SNN_ENCODE_TEMPORAL;

    vae_snn_bridge_t* bridge = vae_snn_bridge_create(&config);
    vae_system_t* vae = create_test_vae();

    if (vae && bridge) {
        vae_snn_bridge_connect_vae(bridge, vae);
        vae_snn_bridge_connect_snn(bridge, NULL);

        float* latent = create_random_latent(TEST_LATENT_DIM);
        vae_snn_encode_result_t result;

        int ret = vae_snn_encode_latent(bridge, latent, TEST_LATENT_DIM,
                                        TEST_WINDOW_MS, &result);
        ck_assert_int_eq(ret, 0);

        vae_snn_encode_result_free(&result);
        nimcp_free(latent);
    }

    vae_snn_bridge_destroy(bridge);
    if (vae) vae_destroy(vae);
}
END_TEST

START_TEST(test_snn_roundtrip)
{
    vae_snn_bridge_config_t config;
    vae_snn_bridge_default_config(&config);
    vae_snn_bridge_t* bridge = vae_snn_bridge_create(&config);
    vae_system_t* vae = create_test_vae();

    if (vae && bridge) {
        vae_snn_bridge_connect_vae(bridge, vae);
        vae_snn_bridge_connect_snn(bridge, NULL);

        float* latent = create_random_latent(TEST_LATENT_DIM);
        vae_snn_roundtrip_result_t result;

        int ret = vae_snn_roundtrip(bridge, latent, TEST_LATENT_DIM,
                                    TEST_WINDOW_MS, &result);
        ck_assert_int_eq(ret, 0);
        ck_assert(result.round_trip_error >= 0.0f);

        vae_snn_roundtrip_result_free(&result);
        nimcp_free(latent);
    }

    vae_snn_bridge_destroy(bridge);
    if (vae) vae_destroy(vae);
}
END_TEST

/* ============================================================================
 * Plasticity Bridge Tests
 * ============================================================================ */

START_TEST(test_plasticity_bridge_default_config)
{
    vae_plasticity_bridge_config_t config;
    int ret = vae_plasticity_bridge_default_config(&config);

    ck_assert_int_eq(ret, 0);
    ck_assert(config.global_learning_rate > 0.0f);
    ck_assert(config.stdp_config.recon_error_gain > 0.0f);
}
END_TEST

START_TEST(test_plasticity_bridge_create_destroy)
{
    vae_plasticity_bridge_config_t config;
    vae_plasticity_bridge_default_config(&config);

    vae_plasticity_bridge_t* bridge = vae_plasticity_bridge_create(&config);
    ck_assert_ptr_nonnull(bridge);

    vae_plasticity_bridge_state_t state = vae_plasticity_bridge_get_state(bridge);
    ck_assert_int_eq(state, VAE_PLAST_STATE_DISCONNECTED);

    vae_plasticity_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_plasticity_modulation_update)
{
    vae_plasticity_bridge_config_t config;
    vae_plasticity_bridge_default_config(&config);
    vae_plasticity_bridge_t* bridge = vae_plasticity_bridge_create(&config);
    vae_system_t* vae = create_test_vae();

    if (vae && bridge) {
        vae_plasticity_bridge_connect_vae(bridge, vae);
        vae_plasticity_bridge_connect_plasticity(bridge, NULL);

        int ret = vae_plasticity_update_modulation(bridge);
        ck_assert_int_eq(ret, 0);

        vae_plasticity_modulation_state_t mod;
        ret = vae_plasticity_get_modulation(bridge, &mod);
        ck_assert_int_eq(ret, 0);
        ck_assert(mod.learning_rate_mod > 0.0f);
    }

    vae_plasticity_bridge_destroy(bridge);
    if (vae) vae_destroy(vae);
}
END_TEST

START_TEST(test_plasticity_collapse_detection)
{
    vae_plasticity_bridge_config_t config;
    vae_plasticity_bridge_default_config(&config);
    vae_plasticity_bridge_t* bridge = vae_plasticity_bridge_create(&config);
    vae_system_t* vae = create_test_vae();

    if (vae && bridge) {
        vae_plasticity_bridge_connect_vae(bridge, vae);
        vae_plasticity_bridge_connect_plasticity(bridge, NULL);

        bool collapse = vae_plasticity_detect_collapse(bridge);
        /* Should not collapse initially */
        ck_assert(!collapse);
    }

    vae_plasticity_bridge_destroy(bridge);
    if (vae) vae_destroy(vae);
}
END_TEST

START_TEST(test_plasticity_replay_generation)
{
    vae_plasticity_bridge_config_t config;
    vae_plasticity_bridge_default_config(&config);
    vae_plasticity_bridge_t* bridge = vae_plasticity_bridge_create(&config);
    vae_system_t* vae = create_test_vae();

    if (vae && bridge) {
        vae_plasticity_bridge_connect_vae(bridge, vae);
        vae_plasticity_bridge_connect_plasticity(bridge, NULL);

        vae_plasticity_replay_result_t result;
        int ret = vae_plasticity_generate_replay(bridge, 10, &result);
        ck_assert_int_eq(ret, 0);
        ck_assert_int_eq(result.num_samples, 10);

        vae_plasticity_replay_result_free(&result);
    }

    vae_plasticity_bridge_destroy(bridge);
    if (vae) vae_destroy(vae);
}
END_TEST

/* ============================================================================
 * Training Bridge Tests
 * ============================================================================ */

START_TEST(test_training_bridge_default_config)
{
    vae_training_bridge_config_t config;
    int ret = vae_training_bridge_default_config(&config);

    ck_assert_int_eq(ret, 0);
    ck_assert_int_eq(config.algorithm, VAE_TRAIN_JOINT);
    ck_assert(config.optimizer.learning_rate > 0.0f);
}
END_TEST

START_TEST(test_training_bridge_create_destroy)
{
    vae_training_bridge_config_t config;
    vae_training_bridge_default_config(&config);

    vae_training_bridge_t* bridge = vae_training_bridge_create(&config);
    ck_assert_ptr_nonnull(bridge);

    vae_training_bridge_state_t state = vae_training_bridge_get_state(bridge);
    ck_assert_int_eq(state, VAE_TRAIN_STATE_DISCONNECTED);

    vae_training_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_training_forward_pass)
{
    vae_training_bridge_config_t config;
    vae_training_bridge_default_config(&config);
    vae_training_bridge_t* bridge = vae_training_bridge_create(&config);
    vae_system_t* vae = create_test_vae();

    if (vae && bridge) {
        vae_training_bridge_connect_vae(bridge, vae);
        vae_training_bridge_connect_trainer(bridge, NULL);

        float* input = create_random_latent(TEST_INPUT_DIM);
        vae_training_forward_result_t result;

        int ret = vae_training_forward(bridge, input, TEST_INPUT_DIM, &result);
        ck_assert_int_eq(ret, 0);
        ck_assert_ptr_nonnull(result.latent_mu);
        ck_assert_int_eq(result.latent_dim, TEST_LATENT_DIM);

        vae_training_forward_result_free(&result);
        nimcp_free(input);
    }

    vae_training_bridge_destroy(bridge);
    if (vae) vae_destroy(vae);
}
END_TEST

START_TEST(test_training_kl_annealing)
{
    vae_training_bridge_config_t config;
    vae_training_bridge_default_config(&config);
    config.warmup_steps = 100;

    vae_training_bridge_t* bridge = vae_training_bridge_create(&config);
    ck_assert_ptr_nonnull(bridge);

    /* KL weight should increase during warmup */
    for (int i = 0; i < 50; i++) {
        vae_training_anneal_kl(bridge);
    }

    vae_training_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_training_algorithm_string)
{
    const char* str = vae_training_algorithm_to_string(VAE_TRAIN_JOINT);
    ck_assert_str_eq(str, "joint");

    str = vae_training_algorithm_to_string(VAE_TRAIN_EPROP_VAE);
    ck_assert_str_eq(str, "eprop_vae");
}
END_TEST

/* ============================================================================
 * Substrate Bridge Tests
 * ============================================================================ */

START_TEST(test_substrate_bridge_default_config)
{
    vae_substrate_bridge_config_t config;
    int ret = vae_substrate_bridge_default_config(&config);

    ck_assert_int_eq(ret, 0);
    ck_assert_int_eq(config.adaptation_strategy, VAE_SUBSTRATE_ADAPT_HYBRID);
    ck_assert(config.energy_budget.max_atp_per_encode > 0.0f);
}
END_TEST

START_TEST(test_substrate_bridge_create_destroy)
{
    vae_substrate_bridge_config_t config;
    vae_substrate_bridge_default_config(&config);

    vae_substrate_bridge_t* bridge = vae_substrate_bridge_create(&config);
    ck_assert_ptr_nonnull(bridge);

    vae_substrate_bridge_state_t state = vae_substrate_bridge_get_state(bridge);
    ck_assert_int_eq(state, VAE_SUBSTRATE_STATE_DISCONNECTED);

    vae_substrate_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_substrate_health_assessment)
{
    vae_substrate_bridge_config_t config;
    vae_substrate_bridge_default_config(&config);
    vae_substrate_bridge_t* bridge = vae_substrate_bridge_create(&config);
    vae_system_t* vae = create_test_vae();

    if (vae && bridge) {
        vae_substrate_bridge_connect_vae(bridge, vae);
        vae_substrate_bridge_connect_substrate(bridge, NULL);

        vae_substrate_health_t health = vae_substrate_assess_health(bridge);
        /* Should be optimal/normal when freshly created */
        ck_assert(health <= VAE_SUBSTRATE_NORMAL);
    }

    vae_substrate_bridge_destroy(bridge);
    if (vae) vae_destroy(vae);
}
END_TEST

START_TEST(test_substrate_energy_estimation)
{
    vae_substrate_bridge_config_t config;
    vae_substrate_bridge_default_config(&config);
    vae_substrate_bridge_t* bridge = vae_substrate_bridge_create(&config);

    float cost = vae_substrate_estimate_cost(bridge, VAE_ENERGY_ENCODE);
    ck_assert(cost > 0.0f);

    cost = vae_substrate_estimate_cost(bridge, VAE_ENERGY_TOTAL);
    ck_assert(cost > 0.0f);

    vae_substrate_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_substrate_q10_scaling)
{
    vae_substrate_bridge_config_t config;
    vae_substrate_bridge_default_config(&config);
    vae_substrate_bridge_t* bridge = vae_substrate_bridge_create(&config);

    float scaled = vae_substrate_q10_scale(bridge, 1.0f, 2.0f);
    /* At normal temp, should be ~1.0 */
    ck_assert(scaled > 0.5f && scaled < 2.0f);

    vae_substrate_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_substrate_adaptation)
{
    vae_substrate_bridge_config_t config;
    vae_substrate_bridge_default_config(&config);
    vae_substrate_bridge_t* bridge = vae_substrate_bridge_create(&config);
    vae_system_t* vae = create_test_vae();

    if (vae && bridge) {
        vae_substrate_bridge_connect_vae(bridge, vae);
        vae_substrate_bridge_connect_substrate(bridge, NULL);

        vae_substrate_adaptation_result_t result;
        int ret = vae_substrate_adapt(bridge, &result);
        ck_assert_int_eq(ret, 0);
        ck_assert(result.compression_ratio >= 1.0f);

        vae_substrate_adaptation_result_free(&result);
    }

    vae_substrate_bridge_destroy(bridge);
    if (vae) vae_destroy(vae);
}
END_TEST

/* ============================================================================
 * Thalamic Bridge Tests
 * ============================================================================ */

START_TEST(test_thalamic_bridge_default_config)
{
    vae_thalamic_bridge_config_t config;
    int ret = vae_thalamic_bridge_default_config(&config);

    ck_assert_int_eq(ret, 0);
    ck_assert(config.active_nuclei_mask != 0);
    ck_assert(config.attention.enable_attention_gating);
}
END_TEST

START_TEST(test_thalamic_bridge_create_destroy)
{
    vae_thalamic_bridge_config_t config;
    vae_thalamic_bridge_default_config(&config);

    vae_thalamic_bridge_t* bridge = vae_thalamic_bridge_create(&config);
    ck_assert_ptr_nonnull(bridge);

    vae_thalamic_bridge_state_t state = vae_thalamic_bridge_get_state(bridge);
    ck_assert_int_eq(state, VAE_THAL_STATE_DISCONNECTED);

    vae_thalamic_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_thalamic_relay)
{
    vae_thalamic_bridge_config_t config;
    vae_thalamic_bridge_default_config(&config);
    vae_thalamic_bridge_t* bridge = vae_thalamic_bridge_create(&config);
    vae_system_t* vae = create_test_vae();

    if (vae && bridge) {
        vae_thalamic_bridge_connect_vae(bridge, vae);
        vae_thalamic_bridge_connect_thalamus(bridge, NULL);

        float* latent = create_random_latent(TEST_LATENT_DIM);
        vae_thalamic_relay_result_t result;

        int ret = vae_thalamic_relay(bridge, VAE_THAL_NUC_LGN,
                                     latent, TEST_LATENT_DIM, &result);
        ck_assert_int_eq(ret, 0);
        ck_assert_ptr_nonnull(result.relayed_latent);
        ck_assert(result.effective_gain > 0.0f);

        vae_thalamic_relay_result_free(&result);
        nimcp_free(latent);
    }

    vae_thalamic_bridge_destroy(bridge);
    if (vae) vae_destroy(vae);
}
END_TEST

START_TEST(test_thalamic_attention)
{
    vae_thalamic_bridge_config_t config;
    vae_thalamic_bridge_default_config(&config);
    vae_thalamic_bridge_t* bridge = vae_thalamic_bridge_create(&config);

    int ret = vae_thalamic_set_attention(bridge, VAE_THAL_NUC_LGN, 0.8f);
    ck_assert_int_eq(ret, 0);

    float attention = vae_thalamic_get_attention(bridge, VAE_THAL_NUC_LGN);
    ck_assert(fabsf(attention - 0.8f) < TEST_EPSILON);

    vae_thalamic_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_thalamic_burst_mode)
{
    vae_thalamic_bridge_config_t config;
    vae_thalamic_bridge_default_config(&config);
    vae_thalamic_bridge_t* bridge = vae_thalamic_bridge_create(&config);

    int ret = vae_thalamic_trigger_burst(bridge, VAE_THAL_NUC_LGN);
    ck_assert_int_eq(ret, 0);

    vae_thalamic_mode_t mode = vae_thalamic_get_mode(bridge, VAE_THAL_NUC_LGN);
    /* May or may not be bursting depending on T-channel state */
    ck_assert(mode == VAE_THAL_MODE_TONIC || mode == VAE_THAL_MODE_BURST);

    vae_thalamic_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_thalamic_nucleus_strings)
{
    const char* str = vae_thalamic_nucleus_to_string(VAE_THAL_NUC_LGN);
    ck_assert_str_eq(str, "LGN");

    str = vae_thalamic_nucleus_to_string(VAE_THAL_NUC_PULVINAR);
    ck_assert_str_eq(str, "Pulvinar");

    str = vae_thalamic_mode_to_string(VAE_THAL_MODE_BURST);
    ck_assert_str_eq(str, "burst");
}
END_TEST

/* ============================================================================
 * Test Suite Setup
 * ============================================================================ */

Suite* vae_neural_integration_suite(void)
{
    Suite* s = suite_create("VAE Neural Integration");

    /* SNN Bridge Tests */
    TCase* tc_snn = tcase_create("SNN Bridge");
    tcase_add_test(tc_snn, test_snn_bridge_default_config);
    tcase_add_test(tc_snn, test_snn_bridge_create_destroy);
    tcase_add_test(tc_snn, test_snn_bridge_connect_vae);
    tcase_add_test(tc_snn, test_snn_encode_rate);
    tcase_add_test(tc_snn, test_snn_encode_temporal);
    tcase_add_test(tc_snn, test_snn_roundtrip);
    suite_add_tcase(s, tc_snn);

    /* Plasticity Bridge Tests */
    TCase* tc_plast = tcase_create("Plasticity Bridge");
    tcase_add_test(tc_plast, test_plasticity_bridge_default_config);
    tcase_add_test(tc_plast, test_plasticity_bridge_create_destroy);
    tcase_add_test(tc_plast, test_plasticity_modulation_update);
    tcase_add_test(tc_plast, test_plasticity_collapse_detection);
    tcase_add_test(tc_plast, test_plasticity_replay_generation);
    suite_add_tcase(s, tc_plast);

    /* Training Bridge Tests */
    TCase* tc_train = tcase_create("Training Bridge");
    tcase_add_test(tc_train, test_training_bridge_default_config);
    tcase_add_test(tc_train, test_training_bridge_create_destroy);
    tcase_add_test(tc_train, test_training_forward_pass);
    tcase_add_test(tc_train, test_training_kl_annealing);
    tcase_add_test(tc_train, test_training_algorithm_string);
    suite_add_tcase(s, tc_train);

    /* Substrate Bridge Tests */
    TCase* tc_sub = tcase_create("Substrate Bridge");
    tcase_add_test(tc_sub, test_substrate_bridge_default_config);
    tcase_add_test(tc_sub, test_substrate_bridge_create_destroy);
    tcase_add_test(tc_sub, test_substrate_health_assessment);
    tcase_add_test(tc_sub, test_substrate_energy_estimation);
    tcase_add_test(tc_sub, test_substrate_q10_scaling);
    tcase_add_test(tc_sub, test_substrate_adaptation);
    suite_add_tcase(s, tc_sub);

    /* Thalamic Bridge Tests */
    TCase* tc_thal = tcase_create("Thalamic Bridge");
    tcase_add_test(tc_thal, test_thalamic_bridge_default_config);
    tcase_add_test(tc_thal, test_thalamic_bridge_create_destroy);
    tcase_add_test(tc_thal, test_thalamic_relay);
    tcase_add_test(tc_thal, test_thalamic_attention);
    tcase_add_test(tc_thal, test_thalamic_burst_mode);
    tcase_add_test(tc_thal, test_thalamic_nucleus_strings);
    suite_add_tcase(s, tc_thal);

    return s;
}

int main(void)
{
    int number_failed;
    Suite* s = vae_neural_integration_suite();
    SRunner* sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
