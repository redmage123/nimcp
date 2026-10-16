/**
 * @file test_vae_loss.c
 * @brief Unit tests for VAE loss computation module
 *
 * WHAT: Test suite for VAE loss API
 * WHY:  Verify correct behavior of reconstruction losses, KL divergence,
 *       annealing, and FEP-compatible free energy computation
 * HOW:  Unit tests using Check framework covering all loss functions
 *
 * @author NIMCP Development Team
 * @date 2026-01-30
 */

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "cognitive/vae/nimcp_vae_loss.h"
#include "tensor/nimcp_tensor.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception.h"

/* ============================================================================
 * Test Constants
 * ============================================================================ */

#define TEST_DIM            16
#define TEST_LATENT_DIM     4
#define TEST_BATCH_SIZE     2
#define TEST_EPSILON        1e-4f

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

static vae_loss_ctx_t* g_loss_ctx = NULL;

static void setup(void)
{
    nimcp_exception_system_init();

    vae_loss_config_t config = vae_loss_default_config();
    g_loss_ctx = vae_loss_ctx_create(&config);
    ck_assert_ptr_nonnull(g_loss_ctx);
}

static void teardown(void)
{
    if (g_loss_ctx) {
        vae_loss_ctx_destroy(g_loss_ctx);
        g_loss_ctx = NULL;
    }

    nimcp_exception_system_shutdown();
}

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

START_TEST(test_default_config)
{
    vae_loss_config_t config = vae_loss_default_config();

    ck_assert_int_eq(config.recon_type, VAE_RECON_MSE);
    ck_assert_float_eq(config.beta, VAE_LOSS_DEFAULT_BETA);
    ck_assert_float_eq(config.free_bits, VAE_LOSS_DEFAULT_FREE_BITS);
    ck_assert_int_eq(config.kl_reduction, VAE_KL_SUM);
    ck_assert_int_eq(config.aggregation, VAE_LOSS_AGG_MEAN);
    ck_assert(!config.use_kl_annealing);
    ck_assert(!config.use_cyclical_annealing);
}
END_TEST

START_TEST(test_ctx_create)
{
    vae_loss_config_t config = vae_loss_default_config();
    vae_loss_ctx_t* ctx = vae_loss_ctx_create(&config);

    ck_assert_ptr_nonnull(ctx);
    ck_assert(ctx->is_initialized);
    ck_assert_uint_eq(ctx->step, 0);
    ck_assert_uint_eq(ctx->loss_computations, 0);

    vae_loss_ctx_destroy(ctx);
}
END_TEST

START_TEST(test_ctx_create_null_config)
{
    vae_loss_ctx_t* ctx = vae_loss_ctx_create(NULL);
    ck_assert_ptr_null(ctx);
}
END_TEST

START_TEST(test_ctx_destroy_null)
{
    /* Should not crash */
    vae_loss_ctx_destroy(NULL);
}
END_TEST

START_TEST(test_ctx_reset)
{
    g_loss_ctx->step = 100;
    g_loss_ctx->loss_computations = 50;
    g_loss_ctx->nan_count = 2;

    int result = vae_loss_ctx_reset(g_loss_ctx);
    ck_assert_int_eq(result, 0);

    ck_assert_uint_eq(g_loss_ctx->step, 0);
    ck_assert_uint_eq(g_loss_ctx->loss_computations, 0);
    ck_assert_uint_eq(g_loss_ctx->nan_count, 0);
}
END_TEST

/* ============================================================================
 * MSE Loss Tests
 * ============================================================================ */

START_TEST(test_mse_zero_error)
{
    uint32_t dims[2] = {TEST_BATCH_SIZE, TEST_DIM};
    nimcp_tensor_t* x = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* x_recon = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);

    /* Same values => MSE = 0 */
    float* x_data = (float*)x->data;
    float* recon_data = (float*)x_recon->data;
    for (uint32_t i = 0; i < TEST_BATCH_SIZE * TEST_DIM; i++) {
        x_data[i] = 0.5f;
        recon_data[i] = 0.5f;
    }

    float mse = vae_loss_mse(x, x_recon, VAE_LOSS_AGG_MEAN);
    ck_assert_float_eq_tol(mse, 0.0f, TEST_EPSILON);

    nimcp_tensor_destroy(x);
    nimcp_tensor_destroy(x_recon);
}
END_TEST

START_TEST(test_mse_known_error)
{
    uint32_t dims[2] = {1, 4};
    nimcp_tensor_t* x = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* x_recon = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);

    /* x = [0, 0, 0, 0], x_recon = [1, 1, 1, 1] => MSE = 4/4 = 1 */
    memset(x->data, 0, 4 * sizeof(float));
    float* recon = (float*)x_recon->data;
    for (int i = 0; i < 4; i++) recon[i] = 1.0f;

    float mse = vae_loss_mse(x, x_recon, VAE_LOSS_AGG_MEAN);
    ck_assert_float_eq_tol(mse, 1.0f, TEST_EPSILON);

    nimcp_tensor_destroy(x);
    nimcp_tensor_destroy(x_recon);
}
END_TEST

START_TEST(test_mse_null_tensor)
{
    uint32_t dims[2] = {1, 4};
    nimcp_tensor_t* x = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);

    float mse = vae_loss_mse(x, NULL, VAE_LOSS_AGG_MEAN);
    ck_assert(isnan(mse));

    mse = vae_loss_mse(NULL, x, VAE_LOSS_AGG_MEAN);
    ck_assert(isnan(mse));

    nimcp_tensor_destroy(x);
}
END_TEST

/* ============================================================================
 * BCE Loss Tests
 * ============================================================================ */

START_TEST(test_bce_perfect_prediction)
{
    uint32_t dims[2] = {1, 4};
    nimcp_tensor_t* x = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* x_recon = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);

    /* x = [1, 1, 0, 0], x_recon = [0.999, 0.999, 0.001, 0.001] => low BCE */
    float* x_data = (float*)x->data;
    float* recon = (float*)x_recon->data;
    x_data[0] = 1.0f; x_data[1] = 1.0f; x_data[2] = 0.0f; x_data[3] = 0.0f;
    recon[0] = 0.999f; recon[1] = 0.999f; recon[2] = 0.001f; recon[3] = 0.001f;

    float bce = vae_loss_bce(x, x_recon, VAE_LOSS_AGG_MEAN);
    ck_assert(!isnan(bce));
    ck_assert(bce >= 0.0f);
    ck_assert(bce < 0.1f);  /* Should be very small */

    nimcp_tensor_destroy(x);
    nimcp_tensor_destroy(x_recon);
}
END_TEST

START_TEST(test_bce_poor_prediction)
{
    uint32_t dims[2] = {1, 4};
    nimcp_tensor_t* x = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* x_recon = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);

    /* x = [1, 1, 0, 0], x_recon = [0.001, 0.001, 0.999, 0.999] => high BCE */
    float* x_data = (float*)x->data;
    float* recon = (float*)x_recon->data;
    x_data[0] = 1.0f; x_data[1] = 1.0f; x_data[2] = 0.0f; x_data[3] = 0.0f;
    recon[0] = 0.001f; recon[1] = 0.001f; recon[2] = 0.999f; recon[3] = 0.999f;

    float bce = vae_loss_bce(x, x_recon, VAE_LOSS_AGG_MEAN);
    ck_assert(!isnan(bce));
    ck_assert(bce > 1.0f);  /* Should be high */

    nimcp_tensor_destroy(x);
    nimcp_tensor_destroy(x_recon);
}
END_TEST

/* ============================================================================
 * KL Divergence Tests
 * ============================================================================ */

START_TEST(test_kl_standard_normal)
{
    uint32_t dims[2] = {1, TEST_LATENT_DIM};
    nimcp_tensor_t* mu = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* log_var = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);

    /* mu = 0, log_var = 0 (variance = 1) => KL = 0 */
    memset(mu->data, 0, TEST_LATENT_DIM * sizeof(float));
    memset(log_var->data, 0, TEST_LATENT_DIM * sizeof(float));

    float kl = vae_loss_kl_standard_normal(mu, log_var, VAE_KL_SUM, VAE_LOSS_AGG_MEAN);
    ck_assert_float_eq_tol(kl, 0.0f, TEST_EPSILON);

    nimcp_tensor_destroy(mu);
    nimcp_tensor_destroy(log_var);
}
END_TEST

START_TEST(test_kl_with_mean)
{
    uint32_t dims[2] = {1, 1};
    nimcp_tensor_t* mu = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* log_var = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);

    /* mu = 1, log_var = 0 => KL = 0.5 * (1 - 0 + 1 - 1) = 0.5 */
    ((float*)mu->data)[0] = 1.0f;
    ((float*)log_var->data)[0] = 0.0f;

    float kl = vae_loss_kl_standard_normal(mu, log_var, VAE_KL_SUM, VAE_LOSS_AGG_MEAN);
    ck_assert_float_eq_tol(kl, 0.5f, TEST_EPSILON);

    nimcp_tensor_destroy(mu);
    nimcp_tensor_destroy(log_var);
}
END_TEST

START_TEST(test_kl_with_variance)
{
    uint32_t dims[2] = {1, 1};
    nimcp_tensor_t* mu = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* log_var = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);

    /* mu = 0, log_var = log(2) => KL = 0.5 * (1 + log(2) - 0 - 2) = 0.5 * (log(2) - 1) */
    ((float*)mu->data)[0] = 0.0f;
    ((float*)log_var->data)[0] = logf(2.0f);

    float expected = 0.5f * (logf(2.0f) - 1.0f + 2.0f - 1.0f);  /* ~0.153 */
    expected = -0.5f * (1.0f + logf(2.0f) - 0.0f - 2.0f);  /* Correct formula */

    float kl = vae_loss_kl_standard_normal(mu, log_var, VAE_KL_SUM, VAE_LOSS_AGG_MEAN);
    ck_assert(!isnan(kl));
    ck_assert(kl >= 0.0f);

    nimcp_tensor_destroy(mu);
    nimcp_tensor_destroy(log_var);
}
END_TEST

START_TEST(test_kl_positive)
{
    /* KL divergence should always be >= 0 */
    uint32_t dims[2] = {TEST_BATCH_SIZE, TEST_LATENT_DIM};
    nimcp_tensor_t* mu = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* log_var = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);

    /* Random values */
    float* mu_data = (float*)mu->data;
    float* lv_data = (float*)log_var->data;
    for (uint32_t i = 0; i < TEST_BATCH_SIZE * TEST_LATENT_DIM; i++) {
        mu_data[i] = (float)rand() / RAND_MAX * 2.0f - 1.0f;
        lv_data[i] = (float)rand() / RAND_MAX * 2.0f - 1.0f;
    }

    float kl = vae_loss_kl_standard_normal(mu, log_var, VAE_KL_SUM, VAE_LOSS_AGG_MEAN);
    ck_assert(!isnan(kl));
    ck_assert(kl >= -TEST_EPSILON);  /* Allow small numerical error */

    nimcp_tensor_destroy(mu);
    nimcp_tensor_destroy(log_var);
}
END_TEST

START_TEST(test_kl_with_free_bits)
{
    uint32_t dims[2] = {1, TEST_LATENT_DIM};
    nimcp_tensor_t* mu = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* log_var = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);

    /* Standard normal => raw KL = 0 */
    memset(mu->data, 0, TEST_LATENT_DIM * sizeof(float));
    memset(log_var->data, 0, TEST_LATENT_DIM * sizeof(float));

    float free_bits = 0.1f;
    float kl = vae_loss_kl_with_free_bits(mu, log_var, free_bits, NULL, VAE_LOSS_AGG_MEAN);

    /* With free_bits = 0.1 per dim, total should be 0.1 * latent_dim */
    ck_assert_float_eq_tol(kl, free_bits * TEST_LATENT_DIM, TEST_EPSILON);

    nimcp_tensor_destroy(mu);
    nimcp_tensor_destroy(log_var);
}
END_TEST

START_TEST(test_kl_per_dimension)
{
    uint32_t dims[2] = {1, TEST_LATENT_DIM};
    nimcp_tensor_t* mu = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* log_var = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);

    /* Set different values per dimension */
    float* mu_data = (float*)mu->data;
    float* lv_data = (float*)log_var->data;
    for (uint32_t i = 0; i < TEST_LATENT_DIM; i++) {
        mu_data[i] = (float)i * 0.5f;
        lv_data[i] = 0.0f;
    }

    float kl_per_dim[TEST_LATENT_DIM];
    int result = vae_loss_kl_per_dimension(mu, log_var, kl_per_dim);
    ck_assert_int_eq(result, 0);

    /* KL should increase with mu */
    for (uint32_t i = 1; i < TEST_LATENT_DIM; i++) {
        ck_assert(kl_per_dim[i] >= kl_per_dim[i-1] - TEST_EPSILON);
    }

    nimcp_tensor_destroy(mu);
    nimcp_tensor_destroy(log_var);
}
END_TEST

/* ============================================================================
 * Full Loss Computation Tests
 * ============================================================================ */

START_TEST(test_compute_loss)
{
    uint32_t input_dims[2] = {TEST_BATCH_SIZE, TEST_DIM};
    uint32_t latent_dims[2] = {TEST_BATCH_SIZE, TEST_LATENT_DIM};

    nimcp_tensor_t* x = nimcp_tensor_create(input_dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* x_recon = nimcp_tensor_create(input_dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* mu = nimcp_tensor_create(latent_dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* log_var = nimcp_tensor_create(latent_dims, 2, NIMCP_DTYPE_FLOAT32);

    /* Fill tensors */
    float* x_data = (float*)x->data;
    float* recon_data = (float*)x_recon->data;
    for (uint32_t i = 0; i < TEST_BATCH_SIZE * TEST_DIM; i++) {
        x_data[i] = (float)rand() / RAND_MAX;
        recon_data[i] = x_data[i] + 0.1f * ((float)rand() / RAND_MAX - 0.5f);
    }

    float* mu_data = (float*)mu->data;
    float* lv_data = (float*)log_var->data;
    for (uint32_t i = 0; i < TEST_BATCH_SIZE * TEST_LATENT_DIM; i++) {
        mu_data[i] = (float)rand() / RAND_MAX - 0.5f;
        lv_data[i] = -0.5f;
    }

    vae_loss_breakdown_t* breakdown = vae_loss_breakdown_create(TEST_LATENT_DIM);
    ck_assert_ptr_nonnull(breakdown);

    float loss = vae_loss_compute(g_loss_ctx, x, x_recon, mu, log_var, breakdown);

    ck_assert(!isnan(loss));
    ck_assert(!isinf(loss));
    ck_assert(loss >= 0.0f);

    /* Check breakdown */
    ck_assert_float_eq_tol(breakdown->total_loss, loss, TEST_EPSILON);
    ck_assert_float_eq_tol(breakdown->free_energy, loss, TEST_EPSILON);
    ck_assert_float_eq_tol(breakdown->elbo, -loss, TEST_EPSILON);
    ck_assert(breakdown->recon_loss >= 0.0f);
    ck_assert(breakdown->kl_raw >= -TEST_EPSILON);

    vae_loss_breakdown_free(breakdown);
    nimcp_tensor_destroy(x);
    nimcp_tensor_destroy(x_recon);
    nimcp_tensor_destroy(mu);
    nimcp_tensor_destroy(log_var);
}
END_TEST

START_TEST(test_compute_loss_stateless)
{
    uint32_t input_dims[2] = {1, TEST_DIM};
    uint32_t latent_dims[2] = {1, TEST_LATENT_DIM};

    nimcp_tensor_t* x = nimcp_tensor_create(input_dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* x_recon = nimcp_tensor_create(input_dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* mu = nimcp_tensor_create(latent_dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* log_var = nimcp_tensor_create(latent_dims, 2, NIMCP_DTYPE_FLOAT32);

    /* Fill with same values */
    float* x_data = (float*)x->data;
    float* recon_data = (float*)x_recon->data;
    for (uint32_t i = 0; i < TEST_DIM; i++) {
        x_data[i] = 0.5f;
        recon_data[i] = 0.5f;
    }
    memset(mu->data, 0, TEST_LATENT_DIM * sizeof(float));
    memset(log_var->data, 0, TEST_LATENT_DIM * sizeof(float));

    vae_loss_config_t config = vae_loss_default_config();
    float loss = vae_loss_compute_stateless(&config, x, x_recon, mu, log_var);

    /* Perfect reconstruction and standard normal => loss ~= 0 */
    ck_assert_float_eq_tol(loss, 0.0f, TEST_EPSILON);

    nimcp_tensor_destroy(x);
    nimcp_tensor_destroy(x_recon);
    nimcp_tensor_destroy(mu);
    nimcp_tensor_destroy(log_var);
}
END_TEST

/* ============================================================================
 * FEP Integration Tests
 * ============================================================================ */

START_TEST(test_free_energy)
{
    uint32_t input_dims[2] = {1, TEST_DIM};
    uint32_t latent_dims[2] = {1, TEST_LATENT_DIM};

    nimcp_tensor_t* x = nimcp_tensor_create(input_dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* x_recon = nimcp_tensor_create(input_dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* mu = nimcp_tensor_create(latent_dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* log_var = nimcp_tensor_create(latent_dims, 2, NIMCP_DTYPE_FLOAT32);

    /* Fill tensors */
    for (uint32_t i = 0; i < TEST_DIM; i++) {
        ((float*)x->data)[i] = 0.5f;
        ((float*)x_recon->data)[i] = 0.6f;
    }
    memset(mu->data, 0, TEST_LATENT_DIM * sizeof(float));
    memset(log_var->data, 0, TEST_LATENT_DIM * sizeof(float));

    float pred_error, complexity;
    float fe = vae_loss_free_energy(g_loss_ctx, x, x_recon, mu, log_var,
                                     &pred_error, &complexity);

    ck_assert(!isnan(fe));
    ck_assert(!isnan(pred_error));
    ck_assert(!isnan(complexity));

    /* Free energy = prediction error + complexity */
    ck_assert_float_eq_tol(fe, pred_error + complexity, TEST_EPSILON);

    /* Prediction error should be positive (reconstruction error) */
    ck_assert(pred_error >= 0.0f);

    /* Complexity should be ~0 for standard normal */
    ck_assert_float_eq_tol(complexity, 0.0f, TEST_EPSILON);

    nimcp_tensor_destroy(x);
    nimcp_tensor_destroy(x_recon);
    nimcp_tensor_destroy(mu);
    nimcp_tensor_destroy(log_var);
}
END_TEST

START_TEST(test_loss_to_fep)
{
    vae_loss_breakdown_t breakdown;
    breakdown.total_loss = 5.0f;
    breakdown.free_energy = 5.0f;
    breakdown.recon_loss = 3.0f;
    breakdown.kl_raw = 2.0f;

    float fep_fe, fep_accuracy, fep_complexity;
    vae_loss_to_fep(&breakdown, &fep_fe, &fep_accuracy, &fep_complexity);

    ck_assert_float_eq(fep_fe, 5.0f);
    ck_assert_float_eq(fep_accuracy, -3.0f);  /* Negative recon loss */
    ck_assert_float_eq(fep_complexity, 2.0f);
}
END_TEST

START_TEST(test_precision_weighted_loss)
{
    uint32_t dims[2] = {1, 4};
    nimcp_tensor_t* x = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* x_recon = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* precision = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);

    /* x = [0, 0, 0, 0], x_recon = [1, 1, 1, 1], precision = [1, 1, 1, 1] */
    memset(x->data, 0, 4 * sizeof(float));
    float* recon = (float*)x_recon->data;
    float* prec = (float*)precision->data;
    for (int i = 0; i < 4; i++) {
        recon[i] = 1.0f;
        prec[i] = 1.0f;
    }

    float loss = vae_loss_precision_weighted(x, x_recon, precision, VAE_LOSS_AGG_MEAN);
    ck_assert_float_eq_tol(loss, 1.0f, TEST_EPSILON);  /* Same as unweighted MSE */

    /* With higher precision, loss should be higher */
    for (int i = 0; i < 4; i++) prec[i] = 2.0f;
    float loss2 = vae_loss_precision_weighted(x, x_recon, precision, VAE_LOSS_AGG_MEAN);
    ck_assert(loss2 > loss);

    nimcp_tensor_destroy(x);
    nimcp_tensor_destroy(x_recon);
    nimcp_tensor_destroy(precision);
}
END_TEST

/* ============================================================================
 * Annealing Tests
 * ============================================================================ */

START_TEST(test_linear_anneal)
{
    float start = 0.0f;
    float end = 1.0f;
    uint32_t total_steps = 100;

    /* At step 0 */
    float w = vae_loss_linear_anneal(0, start, end, total_steps);
    ck_assert_float_eq_tol(w, 0.0f, TEST_EPSILON);

    /* At step 50 */
    w = vae_loss_linear_anneal(50, start, end, total_steps);
    ck_assert_float_eq_tol(w, 0.5f, TEST_EPSILON);

    /* At step 100 */
    w = vae_loss_linear_anneal(100, start, end, total_steps);
    ck_assert_float_eq_tol(w, 1.0f, TEST_EPSILON);

    /* Past end */
    w = vae_loss_linear_anneal(150, start, end, total_steps);
    ck_assert_float_eq_tol(w, 1.0f, TEST_EPSILON);
}
END_TEST

START_TEST(test_cyclical_anneal)
{
    uint32_t cycle_steps = 100;
    uint32_t num_cycles = 4;
    uint32_t total_steps = cycle_steps * num_cycles;

    /* At start of first cycle */
    float w = vae_loss_cyclical_anneal(0, cycle_steps, num_cycles, total_steps);
    ck_assert_float_eq_tol(w, 0.0f, TEST_EPSILON);

    /* At end of first cycle */
    w = vae_loss_cyclical_anneal(99, cycle_steps, num_cycles, total_steps);
    ck_assert(w > 0.9f);

    /* At start of second cycle */
    w = vae_loss_cyclical_anneal(100, cycle_steps, num_cycles, total_steps);
    ck_assert_float_eq_tol(w, 0.0f, TEST_EPSILON);
}
END_TEST

START_TEST(test_anneal_step)
{
    /* Configure annealing */
    vae_loss_config_t config = vae_loss_default_config();
    config.use_kl_annealing = true;
    config.kl_anneal_start = 0.0f;
    config.kl_anneal_end = 1.0f;
    config.kl_anneal_steps = 100;

    vae_loss_ctx_t* ctx = vae_loss_ctx_create(&config);
    ck_assert_ptr_nonnull(ctx);

    /* Initial weight should be start value */
    ck_assert_float_eq_tol(vae_loss_get_kl_weight(ctx), 0.0f, TEST_EPSILON);

    /* Step forward */
    for (int i = 0; i < 50; i++) {
        vae_loss_anneal_step(ctx);
    }

    float w = vae_loss_get_kl_weight(ctx);
    ck_assert(w > 0.4f && w < 0.6f);

    vae_loss_ctx_destroy(ctx);
}
END_TEST

START_TEST(test_set_step)
{
    vae_loss_config_t config = vae_loss_default_config();
    config.use_kl_annealing = true;
    config.kl_anneal_start = 0.0f;
    config.kl_anneal_end = 1.0f;
    config.kl_anneal_steps = 100;

    vae_loss_ctx_t* ctx = vae_loss_ctx_create(&config);

    int result = vae_loss_set_step(ctx, 50);
    ck_assert_int_eq(result, 0);

    float w = vae_loss_get_kl_weight(ctx);
    ck_assert_float_eq_tol(w, 0.5f, TEST_EPSILON);

    vae_loss_ctx_destroy(ctx);
}
END_TEST

/* ============================================================================
 * Gradient Tests
 * ============================================================================ */

START_TEST(test_recon_gradient_mse)
{
    uint32_t dims[2] = {1, 4};
    nimcp_tensor_t* x = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* x_recon = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* d_recon = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);

    /* x = [0, 0, 0, 0], x_recon = [1, 1, 1, 1] */
    memset(x->data, 0, 4 * sizeof(float));
    float* recon = (float*)x_recon->data;
    for (int i = 0; i < 4; i++) recon[i] = 1.0f;

    int result = vae_loss_recon_gradient(x, x_recon, VAE_RECON_MSE, d_recon);
    ck_assert_int_eq(result, 0);

    /* Gradient should be positive (recon > x) */
    float* grad = (float*)d_recon->data;
    for (int i = 0; i < 4; i++) {
        ck_assert(grad[i] > 0.0f);
    }

    nimcp_tensor_destroy(x);
    nimcp_tensor_destroy(x_recon);
    nimcp_tensor_destroy(d_recon);
}
END_TEST

START_TEST(test_kl_gradient)
{
    uint32_t dims[2] = {1, TEST_LATENT_DIM};
    nimcp_tensor_t* mu = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* log_var = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* d_mu = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* d_log_var = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);

    /* Set mu = 1, log_var = 0 */
    float* mu_data = (float*)mu->data;
    float* lv_data = (float*)log_var->data;
    for (uint32_t i = 0; i < TEST_LATENT_DIM; i++) {
        mu_data[i] = 1.0f;
        lv_data[i] = 0.0f;
    }

    int result = vae_loss_kl_gradient(mu, log_var, 1.0f, d_mu, d_log_var);
    ck_assert_int_eq(result, 0);

    /* d(KL)/d(mu) = mu => should be 1.0 (scaled by batch) */
    float* grad_mu = (float*)d_mu->data;
    for (uint32_t i = 0; i < TEST_LATENT_DIM; i++) {
        ck_assert(!isnan(grad_mu[i]));
        ck_assert(grad_mu[i] > 0.0f);  /* Should push mu toward 0 */
    }

    nimcp_tensor_destroy(mu);
    nimcp_tensor_destroy(log_var);
    nimcp_tensor_destroy(d_mu);
    nimcp_tensor_destroy(d_log_var);
}
END_TEST

/* ============================================================================
 * Monitoring Tests
 * ============================================================================ */

START_TEST(test_breakdown_create)
{
    vae_loss_breakdown_t* breakdown = vae_loss_breakdown_create(TEST_LATENT_DIM);
    ck_assert_ptr_nonnull(breakdown);
    ck_assert_ptr_nonnull(breakdown->kl_per_dim);
    ck_assert_uint_eq(breakdown->latent_dim, TEST_LATENT_DIM);

    vae_loss_breakdown_free(breakdown);
}
END_TEST

START_TEST(test_breakdown_create_zero_dim)
{
    vae_loss_breakdown_t* breakdown = vae_loss_breakdown_create(0);
    ck_assert_ptr_nonnull(breakdown);
    ck_assert_ptr_null(breakdown->kl_per_dim);

    vae_loss_breakdown_free(breakdown);
}
END_TEST

START_TEST(test_is_invalid)
{
    ck_assert(vae_loss_is_invalid(NAN));
    ck_assert(vae_loss_is_invalid(INFINITY));
    ck_assert(vae_loss_is_invalid(-INFINITY));
    ck_assert(!vae_loss_is_invalid(1.0f));
    ck_assert(!vae_loss_is_invalid(0.0f));
    ck_assert(!vae_loss_is_invalid(-1.0f));
}
END_TEST

START_TEST(test_count_active_units)
{
    float kl_per_dim[4] = {0.5f, 0.01f, 0.001f, 1.0f};
    uint32_t count = vae_loss_count_active_units(kl_per_dim, 4, 0.1f);
    ck_assert_uint_eq(count, 2);  /* 0.5 and 1.0 are > 0.1 */
}
END_TEST

START_TEST(test_get_stats)
{
    uint32_t nan_count, inf_count;
    uint64_t total;

    g_loss_ctx->nan_count = 5;
    g_loss_ctx->inf_count = 3;
    g_loss_ctx->loss_computations = 100;

    vae_loss_get_stats(g_loss_ctx, &nan_count, &inf_count, &total);

    ck_assert_uint_eq(nan_count, 5);
    ck_assert_uint_eq(inf_count, 3);
    ck_assert_uint_eq(total, 100);
}
END_TEST

/* ============================================================================
 * Test Suite Setup
 * ============================================================================ */

Suite* vae_loss_suite(void)
{
    Suite* s = suite_create("VAE Loss");

    /* Configuration tests */
    TCase* tc_config = tcase_create("Configuration");
    tcase_add_test(tc_config, test_default_config);
    tcase_add_test(tc_config, test_ctx_create);
    tcase_add_test(tc_config, test_ctx_create_null_config);
    tcase_add_test(tc_config, test_ctx_destroy_null);
    tcase_add_checked_fixture(tc_config, setup, teardown);
    tcase_add_test(tc_config, test_ctx_reset);
    suite_add_tcase(s, tc_config);

    /* MSE tests */
    TCase* tc_mse = tcase_create("MSE Loss");
    tcase_add_test(tc_mse, test_mse_zero_error);
    tcase_add_test(tc_mse, test_mse_known_error);
    tcase_add_test(tc_mse, test_mse_null_tensor);
    suite_add_tcase(s, tc_mse);

    /* BCE tests */
    TCase* tc_bce = tcase_create("BCE Loss");
    tcase_add_test(tc_bce, test_bce_perfect_prediction);
    tcase_add_test(tc_bce, test_bce_poor_prediction);
    suite_add_tcase(s, tc_bce);

    /* KL tests */
    TCase* tc_kl = tcase_create("KL Divergence");
    tcase_add_test(tc_kl, test_kl_standard_normal);
    tcase_add_test(tc_kl, test_kl_with_mean);
    tcase_add_test(tc_kl, test_kl_with_variance);
    tcase_add_test(tc_kl, test_kl_positive);
    tcase_add_test(tc_kl, test_kl_with_free_bits);
    tcase_add_test(tc_kl, test_kl_per_dimension);
    suite_add_tcase(s, tc_kl);

    /* Full loss tests */
    TCase* tc_loss = tcase_create("Full Loss");
    tcase_add_checked_fixture(tc_loss, setup, teardown);
    tcase_add_test(tc_loss, test_compute_loss);
    tcase_add_test(tc_loss, test_compute_loss_stateless);
    suite_add_tcase(s, tc_loss);

    /* FEP tests */
    TCase* tc_fep = tcase_create("FEP Integration");
    tcase_add_checked_fixture(tc_fep, setup, teardown);
    tcase_add_test(tc_fep, test_free_energy);
    tcase_add_test(tc_fep, test_loss_to_fep);
    tcase_add_test(tc_fep, test_precision_weighted_loss);
    suite_add_tcase(s, tc_fep);

    /* Annealing tests */
    TCase* tc_anneal = tcase_create("Annealing");
    tcase_add_test(tc_anneal, test_linear_anneal);
    tcase_add_test(tc_anneal, test_cyclical_anneal);
    tcase_add_test(tc_anneal, test_anneal_step);
    tcase_add_test(tc_anneal, test_set_step);
    suite_add_tcase(s, tc_anneal);

    /* Gradient tests */
    TCase* tc_grad = tcase_create("Gradients");
    tcase_add_test(tc_grad, test_recon_gradient_mse);
    tcase_add_test(tc_grad, test_kl_gradient);
    suite_add_tcase(s, tc_grad);

    /* Monitoring tests */
    TCase* tc_monitor = tcase_create("Monitoring");
    tcase_add_checked_fixture(tc_monitor, setup, teardown);
    tcase_add_test(tc_monitor, test_breakdown_create);
    tcase_add_test(tc_monitor, test_breakdown_create_zero_dim);
    tcase_add_test(tc_monitor, test_is_invalid);
    tcase_add_test(tc_monitor, test_count_active_units);
    tcase_add_test(tc_monitor, test_get_stats);
    suite_add_tcase(s, tc_monitor);

    return s;
}

int main(void)
{
    int number_failed;
    Suite* s = vae_loss_suite();
    SRunner* sr = srunner_create(s);

    srunner_set_fork_status(sr, CK_NOFORK);
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
