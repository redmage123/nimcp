/**
 * @file test_vae_latent.c
 * @brief Unit tests for VAE latent space operations
 *
 * WHAT: Test suite for VAE latent API
 * WHY:  Verify correct behavior of sampling, reparameterization,
 *       interpolation, and variance monitoring
 * HOW:  Unit tests using Check framework covering all latent functions
 *
 * @author NIMCP Development Team
 * @date 2026-01-30
 */

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "cognitive/vae/nimcp_vae_latent.h"
#include "tensor/nimcp_tensor.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception.h"

/* ============================================================================
 * Test Constants
 * ============================================================================ */

#define TEST_LATENT_DIM     8
#define TEST_BATCH_SIZE     4
#define TEST_EPSILON        1e-4f
#define TEST_SAMPLES        1000

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

static void setup(void)
{
    nimcp_exception_system_init();
}

static void teardown(void)
{
    nimcp_exception_system_shutdown();
}

/* ============================================================================
 * Sampling Tests
 * ============================================================================ */

START_TEST(test_sample_basic)
{
    uint32_t dims[2] = {TEST_BATCH_SIZE, TEST_LATENT_DIM};
    nimcp_tensor_t* mu = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* log_var = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* z = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);

    /* mu = 0, log_var = 0 => variance = 1 */
    memset(mu->data, 0, TEST_BATCH_SIZE * TEST_LATENT_DIM * sizeof(float));
    memset(log_var->data, 0, TEST_BATCH_SIZE * TEST_LATENT_DIM * sizeof(float));

    int result = vae_latent_sample(mu, log_var, z);
    ck_assert_int_eq(result, 0);

    /* Check samples are valid */
    float* z_data = (float*)z->data;
    for (uint32_t i = 0; i < TEST_BATCH_SIZE * TEST_LATENT_DIM; i++) {
        ck_assert(!isnan(z_data[i]));
        ck_assert(!isinf(z_data[i]));
    }

    nimcp_tensor_destroy(mu);
    nimcp_tensor_destroy(log_var);
    nimcp_tensor_destroy(z);
}
END_TEST

START_TEST(test_sample_with_mean)
{
    uint32_t dims[2] = {1, TEST_LATENT_DIM};
    nimcp_tensor_t* mu = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* log_var = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* z = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);

    /* mu = 10, log_var = -20 (very small variance) */
    float* mu_data = (float*)mu->data;
    float* lv_data = (float*)log_var->data;
    for (uint32_t i = 0; i < TEST_LATENT_DIM; i++) {
        mu_data[i] = 10.0f;
        lv_data[i] = -20.0f;  /* exp(-20) ~ 0 */
    }

    int result = vae_latent_sample(mu, log_var, z);
    ck_assert_int_eq(result, 0);

    /* With near-zero variance, z should be very close to mu */
    float* z_data = (float*)z->data;
    for (uint32_t i = 0; i < TEST_LATENT_DIM; i++) {
        ck_assert_float_eq_tol(z_data[i], 10.0f, 0.01f);
    }

    nimcp_tensor_destroy(mu);
    nimcp_tensor_destroy(log_var);
    nimcp_tensor_destroy(z);
}
END_TEST

START_TEST(test_sample_null_tensors)
{
    uint32_t dims[2] = {1, TEST_LATENT_DIM};
    nimcp_tensor_t* mu = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* log_var = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* z = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);

    int result = vae_latent_sample(NULL, log_var, z);
    ck_assert_int_eq(result, -1);

    result = vae_latent_sample(mu, NULL, z);
    ck_assert_int_eq(result, -1);

    result = vae_latent_sample(mu, log_var, NULL);
    ck_assert_int_eq(result, -1);

    nimcp_tensor_destroy(mu);
    nimcp_tensor_destroy(log_var);
    nimcp_tensor_destroy(z);
}
END_TEST

START_TEST(test_sample_statistics)
{
    /* Sample many times and check statistics approach N(0,1) */
    uint32_t dims[2] = {TEST_SAMPLES, 1};
    nimcp_tensor_t* mu = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* log_var = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* z = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);

    /* mu = 0, log_var = 0 */
    memset(mu->data, 0, TEST_SAMPLES * sizeof(float));
    memset(log_var->data, 0, TEST_SAMPLES * sizeof(float));

    int result = vae_latent_sample(mu, log_var, z);
    ck_assert_int_eq(result, 0);

    /* Compute sample mean and variance */
    float* z_data = (float*)z->data;
    float sum = 0.0f, sum_sq = 0.0f;
    for (uint32_t i = 0; i < TEST_SAMPLES; i++) {
        sum += z_data[i];
        sum_sq += z_data[i] * z_data[i];
    }
    float sample_mean = sum / TEST_SAMPLES;
    float sample_var = sum_sq / TEST_SAMPLES - sample_mean * sample_mean;

    /* Should be close to N(0,1) */
    ck_assert_float_eq_tol(sample_mean, 0.0f, 0.1f);
    ck_assert_float_eq_tol(sample_var, 1.0f, 0.2f);

    nimcp_tensor_destroy(mu);
    nimcp_tensor_destroy(log_var);
    nimcp_tensor_destroy(z);
}
END_TEST

START_TEST(test_sample_prior)
{
    uint32_t num_samples = 100;
    uint32_t dims[2] = {num_samples, TEST_LATENT_DIM};
    nimcp_tensor_t* samples = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);

    int result = vae_latent_sample_prior(num_samples, TEST_LATENT_DIM, samples);
    ck_assert_int_eq(result, 0);

    /* Check all samples are valid */
    float* data = (float*)samples->data;
    for (uint32_t i = 0; i < num_samples * TEST_LATENT_DIM; i++) {
        ck_assert(!isnan(data[i]));
        ck_assert(!isinf(data[i]));
    }

    nimcp_tensor_destroy(samples);
}
END_TEST

START_TEST(test_sample_with_noise)
{
    uint32_t dims[2] = {1, TEST_LATENT_DIM};
    nimcp_tensor_t* mu = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* log_var = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* epsilon = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* z = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);

    /* mu = 2, log_var = 0 (sigma = 1), epsilon = 1 */
    float* mu_data = (float*)mu->data;
    float* lv_data = (float*)log_var->data;
    float* eps_data = (float*)epsilon->data;
    for (uint32_t i = 0; i < TEST_LATENT_DIM; i++) {
        mu_data[i] = 2.0f;
        lv_data[i] = 0.0f;  /* sigma = 1 */
        eps_data[i] = 1.0f;
    }

    int result = vae_latent_sample_with_noise(mu, log_var, epsilon, z);
    ck_assert_int_eq(result, 0);

    /* z = mu + sigma * epsilon = 2 + 1 * 1 = 3 */
    float* z_data = (float*)z->data;
    for (uint32_t i = 0; i < TEST_LATENT_DIM; i++) {
        ck_assert_float_eq_tol(z_data[i], 3.0f, TEST_EPSILON);
    }

    nimcp_tensor_destroy(mu);
    nimcp_tensor_destroy(log_var);
    nimcp_tensor_destroy(epsilon);
    nimcp_tensor_destroy(z);
}
END_TEST

/* ============================================================================
 * KL Divergence Tests
 * ============================================================================ */

START_TEST(test_kl_divergence_standard)
{
    uint32_t dims[2] = {1, TEST_LATENT_DIM};
    nimcp_tensor_t* mu = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* log_var = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);

    /* Standard normal => KL = 0 */
    memset(mu->data, 0, TEST_LATENT_DIM * sizeof(float));
    memset(log_var->data, 0, TEST_LATENT_DIM * sizeof(float));

    float kl = vae_latent_kl_divergence(mu, log_var, NULL);
    ck_assert_float_eq_tol(kl, 0.0f, TEST_EPSILON);

    nimcp_tensor_destroy(mu);
    nimcp_tensor_destroy(log_var);
}
END_TEST

START_TEST(test_kl_divergence_positive)
{
    uint32_t dims[2] = {TEST_BATCH_SIZE, TEST_LATENT_DIM};
    nimcp_tensor_t* mu = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* log_var = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);

    /* Random non-standard values */
    float* mu_data = (float*)mu->data;
    float* lv_data = (float*)log_var->data;
    for (uint32_t i = 0; i < TEST_BATCH_SIZE * TEST_LATENT_DIM; i++) {
        mu_data[i] = (float)rand() / RAND_MAX * 2.0f - 1.0f;
        lv_data[i] = (float)rand() / RAND_MAX * 2.0f - 1.0f;
    }

    float kl = vae_latent_kl_divergence(mu, log_var, NULL);
    ck_assert(!isnan(kl));
    ck_assert(kl >= -TEST_EPSILON);  /* KL >= 0 */

    nimcp_tensor_destroy(mu);
    nimcp_tensor_destroy(log_var);
}
END_TEST

START_TEST(test_kl_per_dimension)
{
    uint32_t dims[2] = {1, TEST_LATENT_DIM};
    nimcp_tensor_t* mu = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* log_var = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);

    float* mu_data = (float*)mu->data;
    float* lv_data = (float*)log_var->data;
    for (uint32_t i = 0; i < TEST_LATENT_DIM; i++) {
        mu_data[i] = (float)i;
        lv_data[i] = 0.0f;
    }

    float kl_per_dim[TEST_LATENT_DIM];
    float total_kl = vae_latent_kl_divergence(mu, log_var, kl_per_dim);

    /* Sum of per-dim KL should equal total */
    float sum = 0.0f;
    for (uint32_t i = 0; i < TEST_LATENT_DIM; i++) {
        sum += kl_per_dim[i];
    }
    ck_assert_float_eq_tol(sum, total_kl, TEST_EPSILON);

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

    float free_bits = 0.5f;
    float kl = vae_latent_kl_with_free_bits(mu, log_var, free_bits, NULL);

    /* With free_bits, KL should be at least free_bits * latent_dim */
    ck_assert_float_eq_tol(kl, free_bits * TEST_LATENT_DIM, TEST_EPSILON);

    nimcp_tensor_destroy(mu);
    nimcp_tensor_destroy(log_var);
}
END_TEST

/* ============================================================================
 * Precision Tests
 * ============================================================================ */

START_TEST(test_compute_precision)
{
    uint32_t dims[2] = {1, TEST_LATENT_DIM};
    nimcp_tensor_t* log_var = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* precision = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);

    /* log_var = 0 => variance = 1 => precision = 1 */
    memset(log_var->data, 0, TEST_LATENT_DIM * sizeof(float));

    int result = vae_latent_compute_precision(log_var, precision);
    ck_assert_int_eq(result, 0);

    float* prec_data = (float*)precision->data;
    for (uint32_t i = 0; i < TEST_LATENT_DIM; i++) {
        ck_assert_float_eq_tol(prec_data[i], 1.0f, TEST_EPSILON);
    }

    nimcp_tensor_destroy(log_var);
    nimcp_tensor_destroy(precision);
}
END_TEST

START_TEST(test_precision_varies_with_variance)
{
    uint32_t dims[2] = {1, 4};
    nimcp_tensor_t* log_var = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* precision = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);

    /* Different variances */
    float* lv_data = (float*)log_var->data;
    lv_data[0] = 0.0f;          /* var = 1, prec = 1 */
    lv_data[1] = logf(2.0f);    /* var = 2, prec = 0.5 */
    lv_data[2] = logf(0.5f);    /* var = 0.5, prec = 2 */
    lv_data[3] = logf(4.0f);    /* var = 4, prec = 0.25 */

    int result = vae_latent_compute_precision(log_var, precision);
    ck_assert_int_eq(result, 0);

    float* prec_data = (float*)precision->data;
    ck_assert_float_eq_tol(prec_data[0], 1.0f, TEST_EPSILON);
    ck_assert_float_eq_tol(prec_data[1], 0.5f, TEST_EPSILON);
    ck_assert_float_eq_tol(prec_data[2], 2.0f, TEST_EPSILON);
    ck_assert_float_eq_tol(prec_data[3], 0.25f, TEST_EPSILON);

    nimcp_tensor_destroy(log_var);
    nimcp_tensor_destroy(precision);
}
END_TEST

START_TEST(test_avg_precision)
{
    uint32_t dims[2] = {1, 4};
    nimcp_tensor_t* log_var = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);

    /* All same variance */
    float* lv_data = (float*)log_var->data;
    for (int i = 0; i < 4; i++) {
        lv_data[i] = 0.0f;  /* var = 1, prec = 1 */
    }

    float avg = vae_latent_avg_precision(log_var);
    ck_assert_float_eq_tol(avg, 1.0f, TEST_EPSILON);

    nimcp_tensor_destroy(log_var);
}
END_TEST

/* ============================================================================
 * Variance Monitoring Tests
 * ============================================================================ */

START_TEST(test_check_collapse)
{
    uint32_t dims[2] = {1, TEST_LATENT_DIM};
    nimcp_tensor_t* mu = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* log_var = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);

    /* Standard normal => not collapsed */
    memset(mu->data, 0, TEST_LATENT_DIM * sizeof(float));
    memset(log_var->data, 0, TEST_LATENT_DIM * sizeof(float));

    uint32_t num_collapsed;
    bool collapsed = vae_latent_check_collapse(mu, log_var, VAE_LATENT_COLLAPSE_THRESHOLD, &num_collapsed);
    ck_assert(!collapsed);
    ck_assert_uint_eq(num_collapsed, 0);

    nimcp_tensor_destroy(mu);
    nimcp_tensor_destroy(log_var);
}
END_TEST

START_TEST(test_check_collapse_detected)
{
    uint32_t dims[2] = {1, 4};
    nimcp_tensor_t* mu = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* log_var = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);

    /* Set some dimensions to standard normal (collapsed) */
    float* mu_data = (float*)mu->data;
    float* lv_data = (float*)log_var->data;
    mu_data[0] = 0.0f; lv_data[0] = 0.0f;   /* Collapsed */
    mu_data[1] = 0.0f; lv_data[1] = 0.0f;   /* Collapsed */
    mu_data[2] = 2.0f; lv_data[2] = -2.0f;  /* Active */
    mu_data[3] = 3.0f; lv_data[3] = -1.0f;  /* Active */

    uint32_t num_collapsed;
    bool collapsed = vae_latent_check_collapse(mu, log_var, VAE_LATENT_COLLAPSE_THRESHOLD, &num_collapsed);

    /* Dimensions with KL ~ 0 are collapsed */
    ck_assert(collapsed);
    ck_assert(num_collapsed >= 2);

    nimcp_tensor_destroy(mu);
    nimcp_tensor_destroy(log_var);
}
END_TEST

START_TEST(test_check_explosion)
{
    uint32_t dims[2] = {1, TEST_LATENT_DIM};
    nimcp_tensor_t* log_var = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);

    /* Normal variance */
    memset(log_var->data, 0, TEST_LATENT_DIM * sizeof(float));

    uint32_t num_exploded;
    bool exploded = vae_latent_check_explosion(log_var, VAE_LATENT_EXPLODE_THRESHOLD, &num_exploded);
    ck_assert(!exploded);
    ck_assert_uint_eq(num_exploded, 0);

    nimcp_tensor_destroy(log_var);
}
END_TEST

START_TEST(test_check_explosion_detected)
{
    uint32_t dims[2] = {1, 4};
    nimcp_tensor_t* log_var = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);

    /* Set some dimensions to exploded variance */
    float* lv_data = (float*)log_var->data;
    lv_data[0] = 0.0f;                          /* Normal */
    lv_data[1] = logf(VAE_LATENT_EXPLODE_THRESHOLD + 10);  /* Exploded */
    lv_data[2] = 0.0f;                          /* Normal */
    lv_data[3] = logf(VAE_LATENT_EXPLODE_THRESHOLD + 20);  /* Exploded */

    uint32_t num_exploded;
    bool exploded = vae_latent_check_explosion(log_var, VAE_LATENT_EXPLODE_THRESHOLD, &num_exploded);
    ck_assert(exploded);
    ck_assert_uint_eq(num_exploded, 2);

    nimcp_tensor_destroy(log_var);
}
END_TEST

START_TEST(test_variance_stats)
{
    uint32_t dims[2] = {1, 4};
    nimcp_tensor_t* log_var = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);

    /* Different variances */
    float* lv_data = (float*)log_var->data;
    lv_data[0] = logf(0.5f);   /* var = 0.5 */
    lv_data[1] = logf(1.0f);   /* var = 1.0 */
    lv_data[2] = logf(2.0f);   /* var = 2.0 */
    lv_data[3] = logf(4.0f);   /* var = 4.0 */

    float min_var, max_var, avg_var;
    int result = vae_latent_variance_stats(log_var, &min_var, &max_var, &avg_var);
    ck_assert_int_eq(result, 0);

    ck_assert_float_eq_tol(min_var, 0.5f, TEST_EPSILON);
    ck_assert_float_eq_tol(max_var, 4.0f, TEST_EPSILON);
    ck_assert_float_eq_tol(avg_var, (0.5f + 1.0f + 2.0f + 4.0f) / 4.0f, TEST_EPSILON);

    nimcp_tensor_destroy(log_var);
}
END_TEST

START_TEST(test_count_active)
{
    uint32_t dims[2] = {1, 4};
    nimcp_tensor_t* mu = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* log_var = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);

    /* Set up so 2 dimensions are active (high KL) */
    float* mu_data = (float*)mu->data;
    float* lv_data = (float*)log_var->data;
    mu_data[0] = 0.0f; lv_data[0] = 0.0f;   /* KL = 0 (inactive) */
    mu_data[1] = 3.0f; lv_data[1] = -1.0f;  /* High KL (active) */
    mu_data[2] = 0.0f; lv_data[2] = 0.0f;   /* KL = 0 (inactive) */
    mu_data[3] = 2.0f; lv_data[3] = -2.0f;  /* High KL (active) */

    float threshold = 0.1f;
    uint32_t active = vae_latent_count_active(mu, log_var, threshold);
    ck_assert_uint_eq(active, 2);

    nimcp_tensor_destroy(mu);
    nimcp_tensor_destroy(log_var);
}
END_TEST

/* ============================================================================
 * Interpolation Tests
 * ============================================================================ */

START_TEST(test_lerp)
{
    uint32_t dims[1] = {TEST_LATENT_DIM};
    nimcp_tensor_t* z1 = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* z2 = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* z_interp = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_FLOAT32);

    /* z1 = 0, z2 = 10 */
    float* z1_data = (float*)z1->data;
    float* z2_data = (float*)z2->data;
    for (uint32_t i = 0; i < TEST_LATENT_DIM; i++) {
        z1_data[i] = 0.0f;
        z2_data[i] = 10.0f;
    }

    /* alpha = 0 => z_interp = z1 */
    int result = vae_latent_lerp(z1, z2, 0.0f, z_interp);
    ck_assert_int_eq(result, 0);
    float* interp_data = (float*)z_interp->data;
    for (uint32_t i = 0; i < TEST_LATENT_DIM; i++) {
        ck_assert_float_eq_tol(interp_data[i], 0.0f, TEST_EPSILON);
    }

    /* alpha = 1 => z_interp = z2 */
    result = vae_latent_lerp(z1, z2, 1.0f, z_interp);
    ck_assert_int_eq(result, 0);
    for (uint32_t i = 0; i < TEST_LATENT_DIM; i++) {
        ck_assert_float_eq_tol(interp_data[i], 10.0f, TEST_EPSILON);
    }

    /* alpha = 0.5 => z_interp = 5 */
    result = vae_latent_lerp(z1, z2, 0.5f, z_interp);
    ck_assert_int_eq(result, 0);
    for (uint32_t i = 0; i < TEST_LATENT_DIM; i++) {
        ck_assert_float_eq_tol(interp_data[i], 5.0f, TEST_EPSILON);
    }

    nimcp_tensor_destroy(z1);
    nimcp_tensor_destroy(z2);
    nimcp_tensor_destroy(z_interp);
}
END_TEST

START_TEST(test_slerp)
{
    uint32_t dims[1] = {2};
    nimcp_tensor_t* z1 = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* z2 = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* z_interp = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_FLOAT32);

    /* z1 = [1, 0], z2 = [0, 1] (unit vectors) */
    float* z1_data = (float*)z1->data;
    float* z2_data = (float*)z2->data;
    z1_data[0] = 1.0f; z1_data[1] = 0.0f;
    z2_data[0] = 0.0f; z2_data[1] = 1.0f;

    /* alpha = 0.5 => should be on great circle */
    int result = vae_latent_slerp(z1, z2, 0.5f, z_interp);
    ck_assert_int_eq(result, 0);

    /* Result should be roughly [0.707, 0.707] */
    float* interp_data = (float*)z_interp->data;
    float norm = sqrtf(interp_data[0] * interp_data[0] + interp_data[1] * interp_data[1]);
    ck_assert_float_eq_tol(norm, 1.0f, 0.1f);

    nimcp_tensor_destroy(z1);
    nimcp_tensor_destroy(z2);
    nimcp_tensor_destroy(z_interp);
}
END_TEST

START_TEST(test_interpolate_path)
{
    uint32_t dims[1] = {TEST_LATENT_DIM};
    nimcp_tensor_t* z1 = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* z2 = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_FLOAT32);

    uint32_t num_steps = 5;
    uint32_t path_dims[2] = {num_steps, TEST_LATENT_DIM};
    nimcp_tensor_t* path = nimcp_tensor_create(path_dims, 2, NIMCP_DTYPE_FLOAT32);

    /* z1 = 0, z2 = 10 */
    float* z1_data = (float*)z1->data;
    float* z2_data = (float*)z2->data;
    for (uint32_t i = 0; i < TEST_LATENT_DIM; i++) {
        z1_data[i] = 0.0f;
        z2_data[i] = 10.0f;
    }

    int result = vae_latent_interpolate_path(z1, z2, num_steps, false, path);
    ck_assert_int_eq(result, 0);

    /* Check path values */
    float* path_data = (float*)path->data;
    for (uint32_t step = 0; step < num_steps; step++) {
        float expected = 10.0f * step / (num_steps - 1);
        for (uint32_t d = 0; d < TEST_LATENT_DIM; d++) {
            ck_assert_float_eq_tol(path_data[step * TEST_LATENT_DIM + d], expected, TEST_EPSILON);
        }
    }

    nimcp_tensor_destroy(z1);
    nimcp_tensor_destroy(z2);
    nimcp_tensor_destroy(path);
}
END_TEST

/* ============================================================================
 * Latent State Management Tests
 * ============================================================================ */

START_TEST(test_state_init)
{
    vae_latent_state_t state;
    int result = vae_latent_state_init(&state, TEST_LATENT_DIM);
    ck_assert_int_eq(result, 0);

    ck_assert_ptr_nonnull(state.mu);
    ck_assert_ptr_nonnull(state.log_var);
    ck_assert_ptr_nonnull(state.z);
    ck_assert_ptr_nonnull(state.precision);
    ck_assert_uint_eq(state.latent_dim, TEST_LATENT_DIM);
    ck_assert(!state.is_valid);

    vae_latent_state_free(&state);
}
END_TEST

START_TEST(test_state_init_null)
{
    int result = vae_latent_state_init(NULL, TEST_LATENT_DIM);
    ck_assert_int_eq(result, -1);
}
END_TEST

START_TEST(test_state_free)
{
    vae_latent_state_t state;
    vae_latent_state_init(&state, TEST_LATENT_DIM);

    /* Should not crash */
    vae_latent_state_free(&state);

    /* Double free should be safe */
    vae_latent_state_free(&state);
}
END_TEST

START_TEST(test_state_update)
{
    vae_latent_state_t state;
    vae_latent_state_init(&state, TEST_LATENT_DIM);

    uint32_t dims[2] = {1, TEST_LATENT_DIM};
    nimcp_tensor_t* mu = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* log_var = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);
    nimcp_tensor_t* z = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_FLOAT32);

    /* Fill tensors */
    float* mu_data = (float*)mu->data;
    float* lv_data = (float*)log_var->data;
    float* z_data = (float*)z->data;
    for (uint32_t i = 0; i < TEST_LATENT_DIM; i++) {
        mu_data[i] = (float)i;
        lv_data[i] = -(float)i * 0.1f;
        z_data[i] = (float)i * 0.5f;
    }

    int result = vae_latent_state_update(&state, mu, log_var, z);
    ck_assert_int_eq(result, 0);
    ck_assert(state.is_valid);

    /* Check values were copied */
    for (uint32_t i = 0; i < TEST_LATENT_DIM; i++) {
        ck_assert_float_eq_tol(state.mu[i], (float)i, TEST_EPSILON);
        ck_assert_float_eq_tol(state.log_var[i], -(float)i * 0.1f, TEST_EPSILON);
        ck_assert_float_eq_tol(state.z[i], (float)i * 0.5f, TEST_EPSILON);
    }

    nimcp_tensor_destroy(mu);
    nimcp_tensor_destroy(log_var);
    nimcp_tensor_destroy(z);
    vae_latent_state_free(&state);
}
END_TEST

/* ============================================================================
 * Test Suite Setup
 * ============================================================================ */

Suite* vae_latent_suite(void)
{
    Suite* s = suite_create("VAE Latent");

    /* Sampling tests */
    TCase* tc_sample = tcase_create("Sampling");
    tcase_add_checked_fixture(tc_sample, setup, teardown);
    tcase_add_test(tc_sample, test_sample_basic);
    tcase_add_test(tc_sample, test_sample_with_mean);
    tcase_add_test(tc_sample, test_sample_null_tensors);
    tcase_add_test(tc_sample, test_sample_statistics);
    tcase_add_test(tc_sample, test_sample_prior);
    tcase_add_test(tc_sample, test_sample_with_noise);
    suite_add_tcase(s, tc_sample);

    /* KL tests */
    TCase* tc_kl = tcase_create("KL Divergence");
    tcase_add_checked_fixture(tc_kl, setup, teardown);
    tcase_add_test(tc_kl, test_kl_divergence_standard);
    tcase_add_test(tc_kl, test_kl_divergence_positive);
    tcase_add_test(tc_kl, test_kl_per_dimension);
    tcase_add_test(tc_kl, test_kl_with_free_bits);
    suite_add_tcase(s, tc_kl);

    /* Precision tests */
    TCase* tc_prec = tcase_create("Precision");
    tcase_add_checked_fixture(tc_prec, setup, teardown);
    tcase_add_test(tc_prec, test_compute_precision);
    tcase_add_test(tc_prec, test_precision_varies_with_variance);
    tcase_add_test(tc_prec, test_avg_precision);
    suite_add_tcase(s, tc_prec);

    /* Variance monitoring tests */
    TCase* tc_var = tcase_create("Variance Monitoring");
    tcase_add_checked_fixture(tc_var, setup, teardown);
    tcase_add_test(tc_var, test_check_collapse);
    tcase_add_test(tc_var, test_check_collapse_detected);
    tcase_add_test(tc_var, test_check_explosion);
    tcase_add_test(tc_var, test_check_explosion_detected);
    tcase_add_test(tc_var, test_variance_stats);
    tcase_add_test(tc_var, test_count_active);
    suite_add_tcase(s, tc_var);

    /* Interpolation tests */
    TCase* tc_interp = tcase_create("Interpolation");
    tcase_add_checked_fixture(tc_interp, setup, teardown);
    tcase_add_test(tc_interp, test_lerp);
    tcase_add_test(tc_interp, test_slerp);
    tcase_add_test(tc_interp, test_interpolate_path);
    suite_add_tcase(s, tc_interp);

    /* State management tests */
    TCase* tc_state = tcase_create("State Management");
    tcase_add_checked_fixture(tc_state, setup, teardown);
    tcase_add_test(tc_state, test_state_init);
    tcase_add_test(tc_state, test_state_init_null);
    tcase_add_test(tc_state, test_state_free);
    tcase_add_test(tc_state, test_state_update);
    suite_add_tcase(s, tc_state);

    return s;
}

int main(void)
{
    int number_failed;
    Suite* s = vae_latent_suite();
    SRunner* sr = srunner_create(s);

    srunner_set_fork_status(sr, CK_NOFORK);
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
