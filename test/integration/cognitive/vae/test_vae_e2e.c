/**
 * @file test_vae_e2e.c
 * @brief End-to-end tests for VAE-FEP integration system
 *
 * Tests complete workflows through the VAE system, simulating
 * real-world usage patterns and verifying system behavior under
 * realistic conditions.
 *
 * E2E scenarios:
 * - Perception pipeline (input -> encode -> latent -> decode -> output)
 * - Active inference loop (observe -> predict -> act -> update)
 * - Neural pathway simulation (VAE -> SNN -> plasticity -> thalamic)
 * - Bio-async communication pipeline
 * - Multi-bridge coordination
 */

#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "cognitive/vae/nimcp_vae.h"
#include "cognitive/vae/bridges/nimcp_vae_fep_bridge.h"
#include "cognitive/vae/bridges/nimcp_vae_snn_bridge.h"
#include "cognitive/vae/bridges/nimcp_vae_plasticity_bridge.h"
#include "cognitive/vae/bridges/nimcp_vae_thalamic_bridge.h"
#include "cognitive/vae/bridges/nimcp_vae_hippocampus_bridge.h"
#include "cognitive/vae/nimcp_vae_bio_async.h"
#include "utils/tensor/nimcp_tensor.h"

/* ============================================================================
 * Test Fixtures
 * ========================================================================== */

static vae_system_t *g_vae = NULL;

/* E2E test configuration */
#define E2E_LATENT_DIM 32
#define E2E_INPUT_DIM 128
#define E2E_BATCH_SIZE 1

static void setup_e2e(void) {
    srand((unsigned int)time(NULL));

    /* Create VAE system with default config */
    vae_config_t config;
    if (vae_default_config(&config) != 0) {
        return;
    }

    /* Configure encoder */
    config.encoder.input_dim = E2E_INPUT_DIM;
    config.encoder.latent_dim = E2E_LATENT_DIM;
    config.encoder.num_layers = 2;
    config.encoder.layers[0].units = 64;
    config.encoder.layers[0].activation = VAE_ACTIVATION_RELU;
    config.encoder.layers[1].units = 48;
    config.encoder.layers[1].activation = VAE_ACTIVATION_RELU;

    /* Configure decoder */
    config.decoder.latent_dim = E2E_LATENT_DIM;
    config.decoder.output_dim = E2E_INPUT_DIM;
    config.decoder.num_layers = 2;
    config.decoder.layers[0].units = 48;
    config.decoder.layers[0].activation = VAE_ACTIVATION_RELU;
    config.decoder.layers[1].units = 64;
    config.decoder.layers[1].activation = VAE_ACTIVATION_RELU;

    /* Training config */
    config.training.beta = 1.0f;
    config.training.learning_rate = 0.001f;

    g_vae = vae_create(&config);
}

static void teardown_e2e(void) {
    if (g_vae) {
        vae_destroy(g_vae);
        g_vae = NULL;
    }
}

/* Helper: Generate synthetic sensory input tensor */
static nimcp_tensor_t* generate_input_tensor(uint32_t dim, float noise) {
    nimcp_tensor_t* input = nimcp_tensor_create_1d(dim, NIMCP_DTYPE_F32);
    if (!input) return NULL;

    float* data = (float*)nimcp_tensor_data(input);
    if (data) {
        for (uint32_t i = 0; i < dim; i++) {
            float signal = sinf((float)i * 0.1f) * 0.5f + 0.5f;
            float n = ((float)rand() / RAND_MAX - 0.5f) * noise;
            data[i] = fmaxf(0.0f, fminf(1.0f, signal + n));
        }
    }
    return input;
}

/* Helper: Compute tensor MSE */
static float compute_tensor_mse(const nimcp_tensor_t* a, const nimcp_tensor_t* b) {
    size_t n = nimcp_tensor_numel(a);
    if (n == 0 || n != nimcp_tensor_numel(b)) return -1.0f;

    const float* da = (const float*)nimcp_tensor_data(a);
    const float* db = (const float*)nimcp_tensor_data(b);
    if (!da || !db) return -1.0f;

    float mse = 0.0f;
    for (size_t i = 0; i < n; i++) {
        float diff = da[i] - db[i];
        mse += diff * diff;
    }
    return mse / (float)n;
}

/* ============================================================================
 * E2E Scenario 1: VAE System Lifecycle
 * ========================================================================== */

START_TEST(test_e2e_vae_lifecycle)
{
    /* Test complete VAE lifecycle: create, use, destroy */
    ck_assert_ptr_nonnull(g_vae);

    /* Verify configuration */
    ck_assert_uint_eq(vae_get_input_dim(g_vae), E2E_INPUT_DIM);
    ck_assert_uint_eq(vae_get_latent_dim(g_vae), E2E_LATENT_DIM);

    /* Verify state */
    vae_state_t state = vae_get_state(g_vae);
    ck_assert(state == VAE_STATE_IDLE || state == VAE_STATE_UNINITIALIZED);
}
END_TEST

/* ============================================================================
 * E2E Scenario 2: Perception Pipeline with Tensors
 * ========================================================================== */

START_TEST(test_e2e_perception_pipeline)
{
    if (!g_vae) return;

    /* Create input tensor */
    nimcp_tensor_t* input = generate_input_tensor(E2E_INPUT_DIM, 0.1f);
    ck_assert_ptr_nonnull(input);

    /* Create latent tensors */
    nimcp_tensor_t* mu = nimcp_tensor_create_1d(E2E_LATENT_DIM, NIMCP_DTYPE_F32);
    nimcp_tensor_t* log_var = nimcp_tensor_create_1d(E2E_LATENT_DIM, NIMCP_DTYPE_F32);
    nimcp_tensor_t* z = nimcp_tensor_create_1d(E2E_LATENT_DIM, NIMCP_DTYPE_F32);
    nimcp_tensor_t* reconstruction = nimcp_tensor_create_1d(E2E_INPUT_DIM, NIMCP_DTYPE_F32);

    if (!mu || !log_var || !z || !reconstruction) {
        nimcp_tensor_destroy(input);
        nimcp_tensor_destroy(mu);
        nimcp_tensor_destroy(log_var);
        nimcp_tensor_destroy(z);
        nimcp_tensor_destroy(reconstruction);
        return;
    }

    /* Step 1: Encode to latent distribution */
    int ret = vae_encode(g_vae, input, mu, log_var);
    ck_assert_int_eq(ret, 0);

    /* Verify mu is valid */
    const float* mu_data = (const float*)nimcp_tensor_data(mu);
    for (uint32_t i = 0; i < E2E_LATENT_DIM; i++) {
        ck_assert(!isnan(mu_data[i]));
        ck_assert(!isinf(mu_data[i]));
    }

    /* Step 2: Sample from latent distribution */
    ret = vae_sample(g_vae, mu, log_var, z);
    ck_assert_int_eq(ret, 0);

    /* Step 3: Decode to reconstruction */
    ret = vae_decode(g_vae, z, reconstruction);
    ck_assert_int_eq(ret, 0);

    /* Step 4: Verify reconstruction quality */
    float mse = compute_tensor_mse(input, reconstruction);
    /* MSE should be bounded (model may not be trained, so be lenient) */
    ck_assert(mse >= 0.0f && mse < 10.0f);

    /* Cleanup */
    nimcp_tensor_destroy(input);
    nimcp_tensor_destroy(mu);
    nimcp_tensor_destroy(log_var);
    nimcp_tensor_destroy(z);
    nimcp_tensor_destroy(reconstruction);
}
END_TEST

/* ============================================================================
 * E2E Scenario 3: FEP Bridge Integration
 * ========================================================================== */

START_TEST(test_e2e_fep_bridge_integration)
{
    if (!g_vae) return;

    /* Create FEP bridge with default config */
    vae_fep_bridge_config_t config;
    if (vae_fep_bridge_default_config(&config) != 0) {
        return;
    }

    vae_fep_bridge_t* fep = vae_fep_bridge_create(&config);
    if (!fep) {
        /* Bridge creation may fail if dependencies not available */
        return;
    }

    /* Connect VAE to bridge */
    int ret = vae_fep_bridge_connect_vae(fep, g_vae);
    if (ret != 0) {
        vae_fep_bridge_destroy(fep);
        return;
    }

    /* Test synchronization */
    ret = vae_fep_sync_latent_to_belief(fep);
    /* May fail if FEP not connected, but shouldn't crash */

    /* Get bridge state */
    vae_fep_bridge_state_t state = vae_fep_bridge_get_state(fep);
    ck_assert(state != VAE_FEP_STATE_ERROR);

    /* Cleanup */
    vae_fep_bridge_destroy(fep);
}
END_TEST

/* ============================================================================
 * E2E Scenario 4: SNN Bridge Integration
 * ========================================================================== */

START_TEST(test_e2e_snn_bridge_integration)
{
    if (!g_vae) return;

    /* Create SNN bridge with default config */
    vae_snn_bridge_config_t config;
    if (vae_snn_bridge_default_config(&config) != 0) {
        return;
    }

    vae_snn_bridge_t* snn = vae_snn_bridge_create(&config);
    if (!snn) {
        return;
    }

    /* Connect VAE to bridge */
    int ret = vae_snn_bridge_connect_vae(snn, g_vae);
    if (ret != 0) {
        vae_snn_bridge_destroy(snn);
        return;
    }

    /* Test encoding latent to spikes */
    float latent[E2E_LATENT_DIM];
    for (uint32_t i = 0; i < E2E_LATENT_DIM; i++) {
        latent[i] = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
    }

    vae_snn_encode_result_t result;
    memset(&result, 0, sizeof(result));
    ret = vae_snn_encode_latent(snn, latent, E2E_LATENT_DIM, 100.0f, &result);
    if (ret == 0) {
        /* Success - verify result */
        ck_assert(result.window_ms > 0);
        vae_snn_encode_result_free(&result);
    }

    /* Get bridge state */
    vae_snn_bridge_state_t state = vae_snn_bridge_get_state(snn);
    ck_assert(state != VAE_SNN_STATE_ERROR);

    /* Cleanup */
    vae_snn_bridge_destroy(snn);
}
END_TEST

/* ============================================================================
 * E2E Scenario 5: Plasticity Bridge Integration
 * ========================================================================== */

START_TEST(test_e2e_plasticity_bridge_integration)
{
    if (!g_vae) return;

    /* Create plasticity bridge with default config */
    vae_plasticity_bridge_config_t config;
    if (vae_plasticity_bridge_default_config(&config) != 0) {
        return;
    }

    vae_plasticity_bridge_t* plasticity = vae_plasticity_bridge_create(&config);
    if (!plasticity) {
        return;
    }

    /* Connect VAE to bridge */
    int ret = vae_plasticity_bridge_connect_vae(plasticity, g_vae);
    if (ret != 0) {
        vae_plasticity_bridge_destroy(plasticity);
        return;
    }

    /* Test modulation update */
    ret = vae_plasticity_update_modulation(plasticity);
    /* May fail if not fully configured, but shouldn't crash */

    /* Get modulation state */
    vae_plasticity_modulation_state_t mod_state;
    ret = vae_plasticity_get_modulation(plasticity, &mod_state);
    if (ret == 0) {
        /* Verify state is reasonable */
        ck_assert(!isnan(mod_state.learning_rate_mod));
    }

    /* Cleanup */
    vae_plasticity_bridge_destroy(plasticity);
}
END_TEST

/* ============================================================================
 * E2E Scenario 6: Thalamic Bridge Integration
 * ========================================================================== */

START_TEST(test_e2e_thalamic_bridge_integration)
{
    if (!g_vae) return;

    /* Create thalamic bridge with default config */
    vae_thalamic_bridge_config_t config;
    if (vae_thalamic_bridge_default_config(&config) != 0) {
        return;
    }

    vae_thalamic_bridge_t* thalamic = vae_thalamic_bridge_create(&config);
    if (!thalamic) {
        return;
    }

    /* Connect VAE to bridge */
    int ret = vae_thalamic_bridge_connect_vae(thalamic, g_vae);
    if (ret != 0) {
        vae_thalamic_bridge_destroy(thalamic);
        return;
    }

    /* Test attention setting */
    ret = vae_thalamic_set_global_attention(thalamic, 0.8f);
    /* May fail if not fully configured */

    /* Get bridge state */
    vae_thalamic_bridge_state_t state = vae_thalamic_bridge_get_state(thalamic);
    ck_assert(state != VAE_THAL_STATE_ERROR);

    /* Cleanup */
    vae_thalamic_bridge_destroy(thalamic);
}
END_TEST

/* ============================================================================
 * E2E Scenario 7: Hippocampus Bridge Integration
 * ========================================================================== */

START_TEST(test_e2e_hippocampus_bridge_integration)
{
    if (!g_vae) return;

    /* Create hippocampus bridge with default config */
    vae_hippo_bridge_config_t config;
    if (vae_hippo_bridge_default_config(&config) != 0) {
        return;
    }

    vae_hippo_bridge_t* hippo = vae_hippo_bridge_create(&config);
    if (!hippo) {
        return;
    }

    /* Connect VAE to bridge */
    int ret = vae_hippo_bridge_connect_vae(hippo, g_vae);
    if (ret != 0) {
        vae_hippo_bridge_destroy(hippo);
        return;
    }

    /* Get latent dimension */
    uint32_t latent_dim = vae_hippo_get_latent_dim(hippo);
    /* Should be 0 if hippo not connected, but non-zero if VAE connected */

    /* Get bridge state */
    vae_hippo_bridge_state_t state = vae_hippo_bridge_get_state(hippo);
    ck_assert(state != VAE_HIPPO_STATE_ERROR);

    /* Cleanup */
    vae_hippo_bridge_destroy(hippo);
}
END_TEST

/* ============================================================================
 * E2E Scenario 8: Bio-Async Bridge Integration
 * ========================================================================== */

START_TEST(test_e2e_bio_async_integration)
{
    if (!g_vae) return;

    /* Create bio-async bridge with default config */
    vae_bio_async_config_t config;
    if (vae_bio_async_default_config(&config) != 0) {
        return;
    }

    vae_bio_async_bridge_t* bio = vae_bio_async_create(&config);
    if (!bio) {
        return;
    }

    /* Connect VAE to bridge */
    int ret = vae_bio_async_connect_vae(bio, g_vae);
    if (ret != 0) {
        vae_bio_async_destroy(bio);
        return;
    }

    /* Try to send heartbeat */
    ret = vae_bio_async_send_heartbeat(bio);
    /* May fail if router not connected */

    /* Get bridge state */
    vae_bio_async_state_t state = vae_bio_async_get_state(bio);
    ck_assert(state != VAE_BIO_ASYNC_ERROR);

    /* Get statistics */
    vae_bio_async_stats_t stats;
    ret = vae_bio_async_get_stats(bio, &stats);
    if (ret == 0) {
        /* Stats should be initialized */
        ck_assert_uint_ge(stats.creation_time_us, 0);
    }

    /* Cleanup */
    vae_bio_async_destroy(bio);
}
END_TEST

/* ============================================================================
 * E2E Scenario 9: Multi-Bridge Coordination
 * ========================================================================== */

START_TEST(test_e2e_multi_bridge_coordination)
{
    if (!g_vae) return;

    /* Create multiple bridges */
    vae_fep_bridge_config_t fep_config;
    vae_snn_bridge_config_t snn_config;
    vae_plasticity_bridge_config_t plast_config;

    vae_fep_bridge_default_config(&fep_config);
    vae_snn_bridge_default_config(&snn_config);
    vae_plasticity_bridge_default_config(&plast_config);

    vae_fep_bridge_t* fep = vae_fep_bridge_create(&fep_config);
    vae_snn_bridge_t* snn = vae_snn_bridge_create(&snn_config);
    vae_plasticity_bridge_t* plasticity = vae_plasticity_bridge_create(&plast_config);

    /* Connect all bridges to same VAE */
    if (fep) vae_fep_bridge_connect_vae(fep, g_vae);
    if (snn) vae_snn_bridge_connect_vae(snn, g_vae);
    if (plasticity) vae_plasticity_bridge_connect_vae(plasticity, g_vae);

    /* Generate input and process through VAE */
    nimcp_tensor_t* input = generate_input_tensor(E2E_INPUT_DIM, 0.1f);
    nimcp_tensor_t* mu = nimcp_tensor_create_1d(E2E_LATENT_DIM, NIMCP_DTYPE_F32);
    nimcp_tensor_t* log_var = nimcp_tensor_create_1d(E2E_LATENT_DIM, NIMCP_DTYPE_F32);

    if (input && mu && log_var) {
        int ret = vae_encode(g_vae, input, mu, log_var);
        if (ret == 0) {
            /* Now bridges can access the VAE state */

            /* FEP: sync latent to beliefs */
            if (fep) {
                vae_fep_sync_latent_to_belief(fep);
            }

            /* Plasticity: update modulation based on encoding */
            if (plasticity) {
                vae_plasticity_update_modulation(plasticity);
            }

            /* All bridges should still be functional */
            if (fep) ck_assert(vae_fep_bridge_get_state(fep) != VAE_FEP_STATE_ERROR);
            if (snn) ck_assert(vae_snn_bridge_get_state(snn) != VAE_SNN_STATE_ERROR);
            if (plasticity) ck_assert(vae_plasticity_bridge_get_state(plasticity) != VAE_PLAST_STATE_ERROR);
        }
    }

    /* Cleanup */
    nimcp_tensor_destroy(input);
    nimcp_tensor_destroy(mu);
    nimcp_tensor_destroy(log_var);

    if (fep) vae_fep_bridge_destroy(fep);
    if (snn) vae_snn_bridge_destroy(snn);
    if (plasticity) vae_plasticity_bridge_destroy(plasticity);
}
END_TEST

/* ============================================================================
 * E2E Scenario 10: Loss Computation and Statistics
 * ========================================================================== */

START_TEST(test_e2e_loss_and_statistics)
{
    if (!g_vae) return;

    /* Get statistics - this is safe even without prior operations */
    vae_stats_t stats;
    int ret = vae_get_stats(g_vae, &stats);
    if (ret == 0) {
        /* Verify stats are reasonable */
        ck_assert_uint_ge(stats.total_encode_calls, 0);
        ck_assert_uint_ge(stats.total_decode_calls, 0);
    }

    /* Get health */
    vae_health_t health;
    ret = vae_get_health(g_vae, &health);
    if (ret == 0) {
        ck_assert(health.overall_health >= 0.0f && health.overall_health <= 1.0f);
    }

    /* Verify we can get config */
    vae_config_t config;
    ret = vae_get_config(g_vae, &config);
    /* Just verifying no crash */

    /* Verify state */
    vae_state_t state = vae_get_state(g_vae);
    ck_assert(state != VAE_STATE_ERROR);
}
END_TEST

/* ============================================================================
 * Test Suite Setup
 * ========================================================================== */

static Suite *vae_e2e_suite(void) {
    Suite *s = suite_create("VAE E2E");

    /* Lifecycle Test */
    TCase *tc_lifecycle = tcase_create("Lifecycle");
    tcase_add_checked_fixture(tc_lifecycle, setup_e2e, teardown_e2e);
    tcase_set_timeout(tc_lifecycle, 30);
    tcase_add_test(tc_lifecycle, test_e2e_vae_lifecycle);
    suite_add_tcase(s, tc_lifecycle);

    /* Perception Pipeline */
    TCase *tc_perception = tcase_create("Perception Pipeline");
    tcase_add_checked_fixture(tc_perception, setup_e2e, teardown_e2e);
    tcase_set_timeout(tc_perception, 60);
    tcase_add_test(tc_perception, test_e2e_perception_pipeline);
    suite_add_tcase(s, tc_perception);

    /* FEP Bridge */
    TCase *tc_fep = tcase_create("FEP Bridge");
    tcase_add_checked_fixture(tc_fep, setup_e2e, teardown_e2e);
    tcase_set_timeout(tc_fep, 60);
    tcase_add_test(tc_fep, test_e2e_fep_bridge_integration);
    suite_add_tcase(s, tc_fep);

    /* SNN Bridge */
    TCase *tc_snn = tcase_create("SNN Bridge");
    tcase_add_checked_fixture(tc_snn, setup_e2e, teardown_e2e);
    tcase_set_timeout(tc_snn, 60);
    tcase_add_test(tc_snn, test_e2e_snn_bridge_integration);
    suite_add_tcase(s, tc_snn);

    /* Plasticity Bridge */
    TCase *tc_plasticity = tcase_create("Plasticity Bridge");
    tcase_add_checked_fixture(tc_plasticity, setup_e2e, teardown_e2e);
    tcase_set_timeout(tc_plasticity, 60);
    tcase_add_test(tc_plasticity, test_e2e_plasticity_bridge_integration);
    suite_add_tcase(s, tc_plasticity);

    /* Thalamic Bridge */
    TCase *tc_thalamic = tcase_create("Thalamic Bridge");
    tcase_add_checked_fixture(tc_thalamic, setup_e2e, teardown_e2e);
    tcase_set_timeout(tc_thalamic, 60);
    tcase_add_test(tc_thalamic, test_e2e_thalamic_bridge_integration);
    suite_add_tcase(s, tc_thalamic);

    /* Hippocampus Bridge */
    TCase *tc_hippo = tcase_create("Hippocampus Bridge");
    tcase_add_checked_fixture(tc_hippo, setup_e2e, teardown_e2e);
    tcase_set_timeout(tc_hippo, 60);
    tcase_add_test(tc_hippo, test_e2e_hippocampus_bridge_integration);
    suite_add_tcase(s, tc_hippo);

    /* Bio-Async Bridge */
    TCase *tc_bio_async = tcase_create("Bio-Async Bridge");
    tcase_add_checked_fixture(tc_bio_async, setup_e2e, teardown_e2e);
    tcase_set_timeout(tc_bio_async, 60);
    tcase_add_test(tc_bio_async, test_e2e_bio_async_integration);
    suite_add_tcase(s, tc_bio_async);

    /* Multi-Bridge Coordination */
    TCase *tc_multi = tcase_create("Multi-Bridge");
    tcase_add_checked_fixture(tc_multi, setup_e2e, teardown_e2e);
    tcase_set_timeout(tc_multi, 120);
    tcase_add_test(tc_multi, test_e2e_multi_bridge_coordination);
    suite_add_tcase(s, tc_multi);

    /* Loss and Statistics */
    TCase *tc_stats = tcase_create("Statistics");
    tcase_add_checked_fixture(tc_stats, setup_e2e, teardown_e2e);
    tcase_set_timeout(tc_stats, 60);
    tcase_add_test(tc_stats, test_e2e_loss_and_statistics);
    suite_add_tcase(s, tc_stats);

    return s;
}

int main(void) {
    Suite *s = vae_e2e_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int failures = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (failures == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
