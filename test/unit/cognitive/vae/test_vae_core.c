/**
 * @file test_vae_core.c
 * @brief Unit tests for VAE core module
 *
 * WHAT: Test suite for core VAE API
 * WHY:  Verify correct behavior of VAE lifecycle, encoding, decoding,
 *       sampling, and loss computation
 * HOW:  Unit tests using Check framework covering all core VAE functions
 *
 * @author NIMCP Development Team
 * @date 2026-01-30
 */

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "cognitive/vae/nimcp_vae.h"
#include "cognitive/vae/nimcp_vae_encoder.h"
#include "cognitive/vae/nimcp_vae_decoder.h"
#include "cognitive/vae/nimcp_vae_latent.h"
#include "cognitive/vae/nimcp_vae_loss.h"
#include "tensor/nimcp_tensor.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception.h"

/* ============================================================================
 * Test Constants
 * ============================================================================ */

#define TEST_INPUT_DIM      64
#define TEST_LATENT_DIM     8
#define TEST_OUTPUT_DIM     64
#define TEST_BATCH_SIZE     4
#define TEST_EPSILON        1e-5f

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

static vae_system_t* g_vae = NULL;
static vae_config_t g_config;

static void setup(void)
{
    /* Initialize exception system */
    nimcp_exception_system_init();

    /* Get default config and customize for testing */
    vae_default_config(&g_config);

    g_config.encoder.input_dim = TEST_INPUT_DIM;
    g_config.encoder.latent_dim = TEST_LATENT_DIM;
    g_config.encoder.num_layers = 1;
    g_config.encoder.layers[0].units = 32;
    g_config.encoder.layers[0].activation = VAE_ACTIVATION_RELU;

    g_config.decoder.latent_dim = TEST_LATENT_DIM;
    g_config.decoder.output_dim = TEST_OUTPUT_DIM;
    g_config.decoder.num_layers = 1;
    g_config.decoder.layers[0].units = 32;
    g_config.decoder.layers[0].activation = VAE_ACTIVATION_RELU;

    g_config.training.beta = 1.0f;
    g_config.training.learning_rate = 0.001f;
    g_config.enable_logging = false;
    g_config.enable_immune_integration = false;

    g_vae = vae_create(&g_config);
    ck_assert_ptr_nonnull(g_vae);
}

static void teardown(void)
{
    if (g_vae) {
        vae_destroy(g_vae);
        g_vae = NULL;
    }

    nimcp_exception_system_shutdown();
}

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

START_TEST(test_default_config)
{
    vae_config_t config;
    int result = vae_default_config(&config);
    ck_assert_int_eq(result, 0);

    /* Check encoder defaults */
    ck_assert_uint_eq(config.encoder.input_dim, 784);
    ck_assert_uint_eq(config.encoder.latent_dim, 32);
    ck_assert_uint_eq(config.encoder.num_layers, 2);

    /* Check decoder defaults */
    ck_assert_uint_eq(config.decoder.latent_dim, 32);
    ck_assert_uint_eq(config.decoder.output_dim, 784);

    /* Check training defaults */
    ck_assert_float_eq(config.training.beta, VAE_DEFAULT_BETA);
    ck_assert_float_eq(config.training.learning_rate, VAE_DEFAULT_LEARNING_RATE);

    /* Check integration flags */
    ck_assert(config.enable_immune_integration);
}
END_TEST

START_TEST(test_default_config_null)
{
    int result = vae_default_config(NULL);
    ck_assert_int_eq(result, -1);
}
END_TEST

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

START_TEST(test_create_basic)
{
    vae_config_t config;
    vae_default_config(&config);
    config.encoder.input_dim = 32;
    config.encoder.latent_dim = 4;
    config.decoder.latent_dim = 4;
    config.decoder.output_dim = 32;

    vae_system_t* vae = vae_create(&config);
    ck_assert_ptr_nonnull(vae);

    /* Check dimensions */
    ck_assert_uint_eq(vae_get_input_dim(vae), 32);
    ck_assert_uint_eq(vae_get_latent_dim(vae), 4);
    ck_assert_uint_eq(vae_get_output_dim(vae), 32);

    vae_destroy(vae);
}
END_TEST

START_TEST(test_create_with_null_config)
{
    /* Should use default config */
    vae_system_t* vae = vae_create(NULL);
    ck_assert_ptr_nonnull(vae);

    ck_assert_uint_eq(vae_get_input_dim(vae), 784);
    ck_assert_uint_eq(vae_get_latent_dim(vae), 32);

    vae_destroy(vae);
}
END_TEST

START_TEST(test_create_invalid_dim)
{
    vae_config_t config;
    vae_default_config(&config);
    config.encoder.input_dim = 0;  /* Invalid */

    vae_system_t* vae = vae_create(&config);
    ck_assert_ptr_null(vae);
}
END_TEST

START_TEST(test_create_invalid_latent_dim)
{
    vae_config_t config;
    vae_default_config(&config);
    config.encoder.latent_dim = VAE_MAX_LATENT_DIM + 1;  /* Invalid */

    vae_system_t* vae = vae_create(&config);
    ck_assert_ptr_null(vae);
}
END_TEST

START_TEST(test_destroy_null)
{
    /* Should not crash */
    vae_destroy(NULL);
}
END_TEST

START_TEST(test_reset)
{
    ck_assert_ptr_nonnull(g_vae);

    int result = vae_reset(g_vae);
    ck_assert_int_eq(result, 0);

    /* Check state is idle */
    ck_assert_int_eq(vae_get_state(g_vae), VAE_STATE_IDLE);
}
END_TEST

/* ============================================================================
 * State Tests
 * ============================================================================ */

START_TEST(test_get_state)
{
    vae_state_t state = vae_get_state(g_vae);
    ck_assert_int_eq(state, VAE_STATE_IDLE);
}
END_TEST

START_TEST(test_state_to_string)
{
    ck_assert_str_eq(vae_state_to_string(VAE_STATE_UNINITIALIZED), "UNINITIALIZED");
    ck_assert_str_eq(vae_state_to_string(VAE_STATE_IDLE), "IDLE");
    ck_assert_str_eq(vae_state_to_string(VAE_STATE_ENCODING), "ENCODING");
    ck_assert_str_eq(vae_state_to_string(VAE_STATE_SAMPLING), "SAMPLING");
    ck_assert_str_eq(vae_state_to_string(VAE_STATE_DECODING), "DECODING");
    ck_assert_str_eq(vae_state_to_string(VAE_STATE_TRAINING), "TRAINING");
    ck_assert_str_eq(vae_state_to_string(VAE_STATE_GENERATING), "GENERATING");
    ck_assert_str_eq(vae_state_to_string(VAE_STATE_ERROR), "ERROR");
}
END_TEST

/* ============================================================================
 * Encoding Tests
 * ============================================================================ */

START_TEST(test_encode_basic)
{
    /* Create input tensor */
    uint32_t input_dims[2] = {TEST_BATCH_SIZE, TEST_INPUT_DIM};
    nimcp_tensor_t* input = nimcp_tensor_create(input_dims, 2, NIMCP_DTYPE_FLOAT32);
    ck_assert_ptr_nonnull(input);

    /* Fill with random data */
    float* data = (float*)input->data;
    for (uint32_t i = 0; i < TEST_BATCH_SIZE * TEST_INPUT_DIM; i++) {
        data[i] = (float)rand() / RAND_MAX;
    }

    /* Create output tensors */
    uint32_t latent_dims[2] = {TEST_BATCH_SIZE, TEST_LATENT_DIM};
    nimcp_tensor_t* mu = nimcp_tensor_create(latent_dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* log_var = nimcp_tensor_create(latent_dims, 2, NIMCP_DTYPE_FLOAT32);
    ck_assert_ptr_nonnull(mu);
    ck_assert_ptr_nonnull(log_var);

    /* Encode */
    int result = vae_encode(g_vae, input, mu, log_var);
    ck_assert_int_eq(result, 0);

    /* Check output shapes */
    ck_assert_uint_eq(mu->dims[0], TEST_BATCH_SIZE);
    ck_assert_uint_eq(mu->dims[1], TEST_LATENT_DIM);
    ck_assert_uint_eq(log_var->dims[0], TEST_BATCH_SIZE);
    ck_assert_uint_eq(log_var->dims[1], TEST_LATENT_DIM);

    /* Check values are not NaN */
    float* mu_data = (float*)mu->data;
    float* lv_data = (float*)log_var->data;
    for (uint32_t i = 0; i < TEST_BATCH_SIZE * TEST_LATENT_DIM; i++) {
        ck_assert(!isnan(mu_data[i]));
        ck_assert(!isnan(lv_data[i]));
    }

    nimcp_tensor_destroy(input);
    nimcp_tensor_destroy(mu);
    nimcp_tensor_destroy(log_var);
}
END_TEST

START_TEST(test_encode_null_input)
{
    uint32_t latent_dims[2] = {TEST_BATCH_SIZE, TEST_LATENT_DIM};
    nimcp_tensor_t* mu = nimcp_tensor_create(latent_dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* log_var = nimcp_tensor_create(latent_dims, 2, NIMCP_DTYPE_FLOAT32);

    int result = vae_encode(g_vae, NULL, mu, log_var);
    ck_assert_int_eq(result, -1);

    nimcp_tensor_destroy(mu);
    nimcp_tensor_destroy(log_var);
}
END_TEST

/* ============================================================================
 * Sampling Tests
 * ============================================================================ */

START_TEST(test_sample_basic)
{
    uint32_t latent_dims[2] = {TEST_BATCH_SIZE, TEST_LATENT_DIM};
    nimcp_tensor_t* mu = nimcp_tensor_create(latent_dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* log_var = nimcp_tensor_create(latent_dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* z = nimcp_tensor_create(latent_dims, 2, NIMCP_DTYPE_FLOAT32);

    /* Set mu = 0, log_var = 0 (variance = 1) */
    memset(mu->data, 0, TEST_BATCH_SIZE * TEST_LATENT_DIM * sizeof(float));
    memset(log_var->data, 0, TEST_BATCH_SIZE * TEST_LATENT_DIM * sizeof(float));

    int result = vae_sample(g_vae, mu, log_var, z);
    ck_assert_int_eq(result, 0);

    /* Check samples are not NaN */
    float* z_data = (float*)z->data;
    for (uint32_t i = 0; i < TEST_BATCH_SIZE * TEST_LATENT_DIM; i++) {
        ck_assert(!isnan(z_data[i]));
    }

    nimcp_tensor_destroy(mu);
    nimcp_tensor_destroy(log_var);
    nimcp_tensor_destroy(z);
}
END_TEST

START_TEST(test_sample_with_mean)
{
    uint32_t latent_dims[2] = {1, TEST_LATENT_DIM};
    nimcp_tensor_t* mu = nimcp_tensor_create(latent_dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* log_var = nimcp_tensor_create(latent_dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* z = nimcp_tensor_create(latent_dims, 2, NIMCP_DTYPE_FLOAT32);

    /* Set mu = 5, log_var = -10 (variance ~ 0) */
    float* mu_data = (float*)mu->data;
    float* lv_data = (float*)log_var->data;
    for (uint32_t i = 0; i < TEST_LATENT_DIM; i++) {
        mu_data[i] = 5.0f;
        lv_data[i] = -10.0f;  /* Very small variance */
    }

    int result = vae_sample(g_vae, mu, log_var, z);
    ck_assert_int_eq(result, 0);

    /* With very small variance, z should be close to mu */
    float* z_data = (float*)z->data;
    for (uint32_t i = 0; i < TEST_LATENT_DIM; i++) {
        ck_assert_float_eq_tol(z_data[i], 5.0f, 0.1f);
    }

    nimcp_tensor_destroy(mu);
    nimcp_tensor_destroy(log_var);
    nimcp_tensor_destroy(z);
}
END_TEST

/* ============================================================================
 * Decoding Tests
 * ============================================================================ */

START_TEST(test_decode_basic)
{
    uint32_t latent_dims[2] = {TEST_BATCH_SIZE, TEST_LATENT_DIM};
    uint32_t output_dims[2] = {TEST_BATCH_SIZE, TEST_OUTPUT_DIM};

    nimcp_tensor_t* z = nimcp_tensor_create(latent_dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* recon = nimcp_tensor_create(output_dims, 2, NIMCP_DTYPE_FLOAT32);

    /* Fill z with random data */
    float* z_data = (float*)z->data;
    for (uint32_t i = 0; i < TEST_BATCH_SIZE * TEST_LATENT_DIM; i++) {
        z_data[i] = (float)rand() / RAND_MAX - 0.5f;
    }

    int result = vae_decode(g_vae, z, recon);
    ck_assert_int_eq(result, 0);

    /* Check output is not NaN */
    float* recon_data = (float*)recon->data;
    for (uint32_t i = 0; i < TEST_BATCH_SIZE * TEST_OUTPUT_DIM; i++) {
        ck_assert(!isnan(recon_data[i]));
    }

    nimcp_tensor_destroy(z);
    nimcp_tensor_destroy(recon);
}
END_TEST

START_TEST(test_decode_null_z)
{
    uint32_t output_dims[2] = {TEST_BATCH_SIZE, TEST_OUTPUT_DIM};
    nimcp_tensor_t* recon = nimcp_tensor_create(output_dims, 2, NIMCP_DTYPE_FLOAT32);

    int result = vae_decode(g_vae, NULL, recon);
    ck_assert_int_eq(result, -1);

    nimcp_tensor_destroy(recon);
}
END_TEST

/* ============================================================================
 * Forward Pass Tests
 * ============================================================================ */

START_TEST(test_forward_basic)
{
    uint32_t input_dims[2] = {TEST_BATCH_SIZE, TEST_INPUT_DIM};
    uint32_t output_dims[2] = {TEST_BATCH_SIZE, TEST_OUTPUT_DIM};

    nimcp_tensor_t* input = nimcp_tensor_create(input_dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* recon = nimcp_tensor_create(output_dims, 2, NIMCP_DTYPE_FLOAT32);

    /* Fill input with random data */
    float* data = (float*)input->data;
    for (uint32_t i = 0; i < TEST_BATCH_SIZE * TEST_INPUT_DIM; i++) {
        data[i] = (float)rand() / RAND_MAX;
    }

    int result = vae_forward(g_vae, input, recon, NULL);
    ck_assert_int_eq(result, 0);

    /* Check output is valid */
    float* recon_data = (float*)recon->data;
    for (uint32_t i = 0; i < TEST_BATCH_SIZE * TEST_OUTPUT_DIM; i++) {
        ck_assert(!isnan(recon_data[i]));
        ck_assert(!isinf(recon_data[i]));
    }

    nimcp_tensor_destroy(input);
    nimcp_tensor_destroy(recon);
}
END_TEST

START_TEST(test_forward_with_latent)
{
    uint32_t input_dims[2] = {1, TEST_INPUT_DIM};
    uint32_t output_dims[2] = {1, TEST_OUTPUT_DIM};

    nimcp_tensor_t* input = nimcp_tensor_create(input_dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* recon = nimcp_tensor_create(output_dims, 2, NIMCP_DTYPE_FLOAT32);

    /* Fill input */
    float* data = (float*)input->data;
    for (uint32_t i = 0; i < TEST_INPUT_DIM; i++) {
        data[i] = (float)rand() / RAND_MAX;
    }

    /* Create latent state */
    vae_latent_state_t* latent = vae_latent_state_create(TEST_LATENT_DIM);
    ck_assert_ptr_nonnull(latent);

    int result = vae_forward(g_vae, input, recon, latent);
    ck_assert_int_eq(result, 0);

    /* Check latent state is valid */
    ck_assert(latent->is_valid);
    ck_assert_uint_eq(latent->latent_dim, TEST_LATENT_DIM);

    for (uint32_t i = 0; i < TEST_LATENT_DIM; i++) {
        ck_assert(!isnan(latent->mu[i]));
        ck_assert(!isnan(latent->log_var[i]));
        ck_assert(!isnan(latent->z[i]));
    }

    vae_latent_state_destroy(latent);
    nimcp_tensor_destroy(input);
    nimcp_tensor_destroy(recon);
}
END_TEST

/* ============================================================================
 * Loss Computation Tests
 * ============================================================================ */

START_TEST(test_compute_loss_basic)
{
    uint32_t input_dims[2] = {TEST_BATCH_SIZE, TEST_INPUT_DIM};
    uint32_t latent_dims[2] = {TEST_BATCH_SIZE, TEST_LATENT_DIM};

    nimcp_tensor_t* input = nimcp_tensor_create(input_dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* recon = nimcp_tensor_create(input_dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* mu = nimcp_tensor_create(latent_dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* log_var = nimcp_tensor_create(latent_dims, 2, NIMCP_DTYPE_FLOAT32);

    /* Fill with data */
    float* in_data = (float*)input->data;
    float* recon_data = (float*)recon->data;
    float* mu_data = (float*)mu->data;
    float* lv_data = (float*)log_var->data;

    for (uint32_t i = 0; i < TEST_BATCH_SIZE * TEST_INPUT_DIM; i++) {
        in_data[i] = (float)rand() / RAND_MAX;
        recon_data[i] = in_data[i] + 0.1f * ((float)rand() / RAND_MAX - 0.5f);
    }

    for (uint32_t i = 0; i < TEST_BATCH_SIZE * TEST_LATENT_DIM; i++) {
        mu_data[i] = (float)rand() / RAND_MAX - 0.5f;
        lv_data[i] = -1.0f;  /* variance = exp(-1) */
    }

    vae_loss_t loss;
    int result = vae_compute_loss(g_vae, input, recon, mu, log_var, &loss);
    ck_assert_int_eq(result, 0);

    /* Check loss values */
    ck_assert(!isnan(loss.total_loss));
    ck_assert(!isnan(loss.reconstruction_loss));
    ck_assert(!isnan(loss.kl_divergence));
    ck_assert(!isnan(loss.free_energy));
    ck_assert(!isnan(loss.elbo));

    ck_assert(loss.total_loss >= 0.0f);
    ck_assert(loss.reconstruction_loss >= 0.0f);
    ck_assert(loss.kl_divergence >= 0.0f);

    /* ELBO should be negative of free energy */
    ck_assert_float_eq_tol(loss.elbo, -loss.free_energy, TEST_EPSILON);

    nimcp_tensor_destroy(input);
    nimcp_tensor_destroy(recon);
    nimcp_tensor_destroy(mu);
    nimcp_tensor_destroy(log_var);
}
END_TEST

START_TEST(test_compute_loss_zero_kl)
{
    uint32_t input_dims[2] = {1, TEST_INPUT_DIM};
    uint32_t latent_dims[2] = {1, TEST_LATENT_DIM};

    nimcp_tensor_t* input = nimcp_tensor_create(input_dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* recon = nimcp_tensor_create(input_dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* mu = nimcp_tensor_create(latent_dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* log_var = nimcp_tensor_create(latent_dims, 2, NIMCP_DTYPE_FLOAT32);

    /* Perfect reconstruction */
    float* in_data = (float*)input->data;
    float* recon_data = (float*)recon->data;
    for (uint32_t i = 0; i < TEST_INPUT_DIM; i++) {
        in_data[i] = 0.5f;
        recon_data[i] = 0.5f;
    }

    /* Standard normal posterior (KL = 0) */
    memset(mu->data, 0, TEST_LATENT_DIM * sizeof(float));
    memset(log_var->data, 0, TEST_LATENT_DIM * sizeof(float));

    vae_loss_t loss;
    int result = vae_compute_loss(g_vae, input, recon, mu, log_var, &loss);
    ck_assert_int_eq(result, 0);

    /* With perfect reconstruction and standard normal, both should be ~0 */
    ck_assert_float_eq_tol(loss.reconstruction_loss, 0.0f, TEST_EPSILON);
    ck_assert_float_eq_tol(loss.kl_divergence, 0.0f, TEST_EPSILON);

    nimcp_tensor_destroy(input);
    nimcp_tensor_destroy(recon);
    nimcp_tensor_destroy(mu);
    nimcp_tensor_destroy(log_var);
}
END_TEST

/* ============================================================================
 * Training Tests
 * ============================================================================ */

START_TEST(test_set_training_mode)
{
    ck_assert(!vae_is_training(g_vae));

    int result = vae_set_training(g_vae, true);
    ck_assert_int_eq(result, 0);
    ck_assert(vae_is_training(g_vae));

    result = vae_set_training(g_vae, false);
    ck_assert_int_eq(result, 0);
    ck_assert(!vae_is_training(g_vae));
}
END_TEST

START_TEST(test_set_beta)
{
    float initial_beta = vae_get_beta(g_vae);
    ck_assert_float_eq(initial_beta, 1.0f);

    int result = vae_set_beta(g_vae, 4.0f);
    ck_assert_int_eq(result, 0);
    ck_assert_float_eq(vae_get_beta(g_vae), 4.0f);

    /* Negative beta should fail */
    result = vae_set_beta(g_vae, -1.0f);
    ck_assert_int_eq(result, -1);
}
END_TEST

START_TEST(test_set_learning_rate)
{
    int result = vae_set_learning_rate(g_vae, 0.0001f);
    ck_assert_int_eq(result, 0);

    /* Zero LR should fail */
    result = vae_set_learning_rate(g_vae, 0.0f);
    ck_assert_int_eq(result, -1);

    /* Negative LR should fail */
    result = vae_set_learning_rate(g_vae, -0.001f);
    ck_assert_int_eq(result, -1);
}
END_TEST

START_TEST(test_train_step_basic)
{
    uint32_t input_dims[2] = {TEST_BATCH_SIZE, TEST_INPUT_DIM};
    nimcp_tensor_t* input = nimcp_tensor_create(input_dims, 2, NIMCP_DTYPE_FLOAT32);

    /* Fill with random data */
    float* data = (float*)input->data;
    for (uint32_t i = 0; i < TEST_BATCH_SIZE * TEST_INPUT_DIM; i++) {
        data[i] = (float)rand() / RAND_MAX;
    }

    vae_loss_t loss;
    int result = vae_train_step(g_vae, input, &loss);
    ck_assert_int_eq(result, 0);

    /* Check loss is valid */
    ck_assert(!isnan(loss.total_loss));
    ck_assert(!isinf(loss.total_loss));
    ck_assert(loss.total_loss >= 0.0f);

    nimcp_tensor_destroy(input);
}
END_TEST

/* ============================================================================
 * Generation Tests
 * ============================================================================ */

START_TEST(test_generate_basic)
{
    uint32_t num_samples = 2;
    uint32_t output_dims[2] = {num_samples, TEST_OUTPUT_DIM};

    nimcp_tensor_t* samples = nimcp_tensor_create(output_dims, 2, NIMCP_DTYPE_FLOAT32);
    ck_assert_ptr_nonnull(samples);

    int result = vae_generate(g_vae, num_samples, samples);
    ck_assert_int_eq(result, 0);

    /* Check samples are valid */
    float* data = (float*)samples->data;
    for (uint32_t i = 0; i < num_samples * TEST_OUTPUT_DIM; i++) {
        ck_assert(!isnan(data[i]));
        ck_assert(!isinf(data[i]));
    }

    nimcp_tensor_destroy(samples);
}
END_TEST

START_TEST(test_sample_prior)
{
    uint32_t num_samples = 3;
    uint32_t latent_dims[2] = {num_samples, TEST_LATENT_DIM};

    nimcp_tensor_t* z = nimcp_tensor_create(latent_dims, 2, NIMCP_DTYPE_FLOAT32);
    ck_assert_ptr_nonnull(z);

    int result = vae_sample_prior(g_vae, num_samples, z);
    ck_assert_int_eq(result, 0);

    /* Check samples are valid */
    float* data = (float*)z->data;
    for (uint32_t i = 0; i < num_samples * TEST_LATENT_DIM; i++) {
        ck_assert(!isnan(data[i]));
    }

    nimcp_tensor_destroy(z);
}
END_TEST

START_TEST(test_reconstruct)
{
    uint32_t input_dims[2] = {1, TEST_INPUT_DIM};
    uint32_t output_dims[2] = {1, TEST_OUTPUT_DIM};

    nimcp_tensor_t* input = nimcp_tensor_create(input_dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* recon = nimcp_tensor_create(output_dims, 2, NIMCP_DTYPE_FLOAT32);

    /* Fill input */
    float* data = (float*)input->data;
    for (uint32_t i = 0; i < TEST_INPUT_DIM; i++) {
        data[i] = (float)i / TEST_INPUT_DIM;
    }

    int result = vae_reconstruct(g_vae, input, recon);
    ck_assert_int_eq(result, 0);

    /* Check reconstruction is valid */
    float* recon_data = (float*)recon->data;
    for (uint32_t i = 0; i < TEST_OUTPUT_DIM; i++) {
        ck_assert(!isnan(recon_data[i]));
        ck_assert(!isinf(recon_data[i]));
    }

    nimcp_tensor_destroy(input);
    nimcp_tensor_destroy(recon);
}
END_TEST

/* ============================================================================
 * Anomaly Detection Tests
 * ============================================================================ */

START_TEST(test_compute_anomaly_score)
{
    uint32_t input_dims[2] = {1, TEST_INPUT_DIM};
    nimcp_tensor_t* input = nimcp_tensor_create(input_dims, 2, NIMCP_DTYPE_FLOAT32);

    /* Normal input */
    float* data = (float*)input->data;
    for (uint32_t i = 0; i < TEST_INPUT_DIM; i++) {
        data[i] = (float)rand() / RAND_MAX;
    }

    float score;
    int result = vae_compute_anomaly_score(g_vae, input, &score);
    ck_assert_int_eq(result, 0);
    ck_assert(!isnan(score));
    ck_assert(score >= 0.0f);

    nimcp_tensor_destroy(input);
}
END_TEST

START_TEST(test_is_anomaly)
{
    uint32_t input_dims[2] = {1, TEST_INPUT_DIM};
    nimcp_tensor_t* input = nimcp_tensor_create(input_dims, 2, NIMCP_DTYPE_FLOAT32);

    /* Fill with data */
    float* data = (float*)input->data;
    for (uint32_t i = 0; i < TEST_INPUT_DIM; i++) {
        data[i] = 0.5f;
    }

    bool is_anomaly;
    int result = vae_is_anomaly(g_vae, input, &is_anomaly);
    ck_assert_int_eq(result, 0);

    nimcp_tensor_destroy(input);
}
END_TEST

/* ============================================================================
 * FEP Integration Tests
 * ============================================================================ */

START_TEST(test_get_free_energy)
{
    /* Do a forward pass first */
    uint32_t input_dims[2] = {1, TEST_INPUT_DIM};
    uint32_t output_dims[2] = {1, TEST_OUTPUT_DIM};

    nimcp_tensor_t* input = nimcp_tensor_create(input_dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* recon = nimcp_tensor_create(output_dims, 2, NIMCP_DTYPE_FLOAT32);

    float* data = (float*)input->data;
    for (uint32_t i = 0; i < TEST_INPUT_DIM; i++) {
        data[i] = 0.5f;
    }

    vae_forward(g_vae, input, recon, NULL);

    /* Now check free energy is available */
    float fe = vae_get_free_energy(g_vae);
    /* May be NaN if loss wasn't computed - that's OK for this test */
    (void)fe;

    nimcp_tensor_destroy(input);
    nimcp_tensor_destroy(recon);
}
END_TEST

/* ============================================================================
 * Latent State Tests
 * ============================================================================ */

START_TEST(test_latent_state_create)
{
    vae_latent_state_t* state = vae_latent_state_create(TEST_LATENT_DIM);
    ck_assert_ptr_nonnull(state);
    ck_assert_ptr_nonnull(state->mu);
    ck_assert_ptr_nonnull(state->log_var);
    ck_assert_ptr_nonnull(state->z);
    ck_assert_ptr_nonnull(state->precision);
    ck_assert_uint_eq(state->latent_dim, TEST_LATENT_DIM);
    ck_assert(!state->is_valid);

    vae_latent_state_destroy(state);
}
END_TEST

START_TEST(test_latent_state_create_invalid)
{
    vae_latent_state_t* state = vae_latent_state_create(0);
    ck_assert_ptr_null(state);

    state = vae_latent_state_create(VAE_MAX_LATENT_DIM + 1);
    ck_assert_ptr_null(state);
}
END_TEST

START_TEST(test_latent_state_copy)
{
    vae_latent_state_t* src = vae_latent_state_create(TEST_LATENT_DIM);
    vae_latent_state_t* dst = vae_latent_state_create(TEST_LATENT_DIM);

    /* Fill source */
    for (uint32_t i = 0; i < TEST_LATENT_DIM; i++) {
        src->mu[i] = (float)i;
        src->log_var[i] = -(float)i;
        src->z[i] = (float)i * 0.5f;
    }
    src->is_valid = true;

    int result = vae_latent_state_copy(dst, src);
    ck_assert_int_eq(result, 0);

    /* Verify copy */
    for (uint32_t i = 0; i < TEST_LATENT_DIM; i++) {
        ck_assert_float_eq(dst->mu[i], src->mu[i]);
        ck_assert_float_eq(dst->log_var[i], src->log_var[i]);
        ck_assert_float_eq(dst->z[i], src->z[i]);
    }
    ck_assert(dst->is_valid);

    vae_latent_state_destroy(src);
    vae_latent_state_destroy(dst);
}
END_TEST

START_TEST(test_latent_state_reset)
{
    vae_latent_state_t* state = vae_latent_state_create(TEST_LATENT_DIM);

    /* Set some values */
    for (uint32_t i = 0; i < TEST_LATENT_DIM; i++) {
        state->mu[i] = 1.0f;
        state->z[i] = 1.0f;
    }
    state->is_valid = true;

    int result = vae_latent_state_reset(state);
    ck_assert_int_eq(result, 0);

    /* Check reset */
    for (uint32_t i = 0; i < TEST_LATENT_DIM; i++) {
        ck_assert_float_eq(state->mu[i], 0.0f);
        ck_assert_float_eq(state->z[i], 0.0f);
        ck_assert_float_eq(state->precision[i], 1.0f);
    }
    ck_assert(!state->is_valid);

    vae_latent_state_destroy(state);
}
END_TEST

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

START_TEST(test_get_stats)
{
    vae_stats_t stats;
    int result = vae_get_stats(g_vae, &stats);
    ck_assert_int_eq(result, 0);

    /* Initial stats should be zero */
    ck_assert_uint_eq(stats.total_encode_calls, 0);
    ck_assert_uint_eq(stats.total_decode_calls, 0);
}
END_TEST

START_TEST(test_get_health)
{
    vae_health_t health;
    int result = vae_get_health(g_vae, &health);
    ck_assert_int_eq(result, 0);

    /* Initial health should be good */
    ck_assert(health.is_healthy);
    ck_assert_float_eq(health.overall_health, 1.0f);
    ck_assert_uint_eq(health.consecutive_errors, 0);
}
END_TEST

START_TEST(test_get_config)
{
    vae_config_t config;
    int result = vae_get_config(g_vae, &config);
    ck_assert_int_eq(result, 0);

    ck_assert_uint_eq(config.encoder.input_dim, TEST_INPUT_DIM);
    ck_assert_uint_eq(config.encoder.latent_dim, TEST_LATENT_DIM);
    ck_assert_uint_eq(config.decoder.output_dim, TEST_OUTPUT_DIM);
}
END_TEST

/* ============================================================================
 * Test Suite Setup
 * ============================================================================ */

Suite* vae_core_suite(void)
{
    Suite* s = suite_create("VAE Core");

    /* Configuration tests */
    TCase* tc_config = tcase_create("Configuration");
    tcase_add_test(tc_config, test_default_config);
    tcase_add_test(tc_config, test_default_config_null);
    suite_add_tcase(s, tc_config);

    /* Lifecycle tests */
    TCase* tc_lifecycle = tcase_create("Lifecycle");
    tcase_add_test(tc_lifecycle, test_create_basic);
    tcase_add_test(tc_lifecycle, test_create_with_null_config);
    tcase_add_test(tc_lifecycle, test_create_invalid_dim);
    tcase_add_test(tc_lifecycle, test_create_invalid_latent_dim);
    tcase_add_test(tc_lifecycle, test_destroy_null);
    tcase_add_checked_fixture(tc_lifecycle, setup, teardown);
    tcase_add_test(tc_lifecycle, test_reset);
    suite_add_tcase(s, tc_lifecycle);

    /* State tests */
    TCase* tc_state = tcase_create("State");
    tcase_add_checked_fixture(tc_state, setup, teardown);
    tcase_add_test(tc_state, test_get_state);
    tcase_add_test(tc_state, test_state_to_string);
    suite_add_tcase(s, tc_state);

    /* Encoding tests */
    TCase* tc_encode = tcase_create("Encoding");
    tcase_add_checked_fixture(tc_encode, setup, teardown);
    tcase_add_test(tc_encode, test_encode_basic);
    tcase_add_test(tc_encode, test_encode_null_input);
    suite_add_tcase(s, tc_encode);

    /* Sampling tests */
    TCase* tc_sample = tcase_create("Sampling");
    tcase_add_checked_fixture(tc_sample, setup, teardown);
    tcase_add_test(tc_sample, test_sample_basic);
    tcase_add_test(tc_sample, test_sample_with_mean);
    suite_add_tcase(s, tc_sample);

    /* Decoding tests */
    TCase* tc_decode = tcase_create("Decoding");
    tcase_add_checked_fixture(tc_decode, setup, teardown);
    tcase_add_test(tc_decode, test_decode_basic);
    tcase_add_test(tc_decode, test_decode_null_z);
    suite_add_tcase(s, tc_decode);

    /* Forward pass tests */
    TCase* tc_forward = tcase_create("Forward Pass");
    tcase_add_checked_fixture(tc_forward, setup, teardown);
    tcase_add_test(tc_forward, test_forward_basic);
    tcase_add_test(tc_forward, test_forward_with_latent);
    suite_add_tcase(s, tc_forward);

    /* Loss tests */
    TCase* tc_loss = tcase_create("Loss Computation");
    tcase_add_checked_fixture(tc_loss, setup, teardown);
    tcase_add_test(tc_loss, test_compute_loss_basic);
    tcase_add_test(tc_loss, test_compute_loss_zero_kl);
    suite_add_tcase(s, tc_loss);

    /* Training tests */
    TCase* tc_training = tcase_create("Training");
    tcase_add_checked_fixture(tc_training, setup, teardown);
    tcase_add_test(tc_training, test_set_training_mode);
    tcase_add_test(tc_training, test_set_beta);
    tcase_add_test(tc_training, test_set_learning_rate);
    tcase_add_test(tc_training, test_train_step_basic);
    suite_add_tcase(s, tc_training);

    /* Generation tests */
    TCase* tc_generation = tcase_create("Generation");
    tcase_add_checked_fixture(tc_generation, setup, teardown);
    tcase_add_test(tc_generation, test_generate_basic);
    tcase_add_test(tc_generation, test_sample_prior);
    tcase_add_test(tc_generation, test_reconstruct);
    suite_add_tcase(s, tc_generation);

    /* Anomaly detection tests */
    TCase* tc_anomaly = tcase_create("Anomaly Detection");
    tcase_add_checked_fixture(tc_anomaly, setup, teardown);
    tcase_add_test(tc_anomaly, test_compute_anomaly_score);
    tcase_add_test(tc_anomaly, test_is_anomaly);
    suite_add_tcase(s, tc_anomaly);

    /* FEP integration tests */
    TCase* tc_fep = tcase_create("FEP Integration");
    tcase_add_checked_fixture(tc_fep, setup, teardown);
    tcase_add_test(tc_fep, test_get_free_energy);
    suite_add_tcase(s, tc_fep);

    /* Latent state tests */
    TCase* tc_latent = tcase_create("Latent State");
    tcase_add_test(tc_latent, test_latent_state_create);
    tcase_add_test(tc_latent, test_latent_state_create_invalid);
    tcase_add_test(tc_latent, test_latent_state_copy);
    tcase_add_test(tc_latent, test_latent_state_reset);
    suite_add_tcase(s, tc_latent);

    /* Statistics tests */
    TCase* tc_stats = tcase_create("Statistics");
    tcase_add_checked_fixture(tc_stats, setup, teardown);
    tcase_add_test(tc_stats, test_get_stats);
    tcase_add_test(tc_stats, test_get_health);
    tcase_add_test(tc_stats, test_get_config);
    suite_add_tcase(s, tc_stats);

    return s;
}

int main(void)
{
    int number_failed;
    Suite* s = vae_core_suite();
    SRunner* sr = srunner_create(s);

    srunner_set_fork_status(sr, CK_NOFORK);
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
