/**
 * @file nimcp_statistics_gpu.c
 * @brief CPU Fallback Implementation for GPU Statistics Module
 *
 * WHAT: CPU fallback stubs when CUDA is not available
 * WHY:  Allow code to compile and link without CUDA
 * HOW:  Stub functions that return appropriate error codes
 *
 * When CUDA is enabled, the actual implementations are in
 * nimcp_statistics_kernels.cu which overrides these stubs.
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include "gpu/statistics/nimcp_statistics_gpu.h"
#include "constants/nimcp_buffer_constants.h"
#include <stdlib.h>
#include <string.h>
#include "utils/exception/nimcp_exception_macros.h"

#ifndef NIMCP_ENABLE_CUDA

//=============================================================================
// Thread-Local Error Storage
//=============================================================================

static __thread char g_stats_gpu_error[NIMCP_ERROR_BUFFER_SIZE] = "GPU statistics not available (CUDA disabled)";

//=============================================================================
// RNG Lifecycle Stubs
//=============================================================================

stats_gpu_rng_t* stats_gpu_rng_create(
    nimcp_gpu_context_t* ctx,
    uint32_t n,
    uint64_t seed)
{
    (void)ctx; (void)n; (void)seed;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stats_gpu_rng_create: operation failed");
    return NULL;
}

void stats_gpu_rng_destroy(stats_gpu_rng_t* rng)
{
    (void)rng;
}

bool stats_gpu_rng_reseed(stats_gpu_rng_t* rng, uint64_t seed)
{
    (void)rng; (void)seed;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "stats_gpu_rng_reseed: operation failed");
    return false;
}

//=============================================================================
// Workspace Management Stubs
//=============================================================================

stats_gpu_workspace_t* stats_gpu_workspace_create(
    nimcp_gpu_context_t* ctx,
    uint32_t max_samples,
    uint32_t max_vars)
{
    (void)ctx; (void)max_samples; (void)max_vars;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stats_gpu_workspace_create: operation failed");
    return NULL;
}

void stats_gpu_workspace_destroy(stats_gpu_workspace_t* workspace)
{
    (void)workspace;
}

//=============================================================================
// Descriptive Statistics Stubs
//=============================================================================

bool nimcp_stats_gpu_mean_batch(
    nimcp_gpu_context_t* ctx,
    const float* data,
    float* means_out,
    const stats_gpu_descriptive_params_t* params)
{
    (void)ctx; (void)data; (void)means_out; (void)params;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_stats_gpu_mean_batch: operation failed");
    return false;
}

bool nimcp_stats_gpu_variance_batch(
    nimcp_gpu_context_t* ctx,
    const float* data,
    float* means_out,
    float* variances_out,
    const stats_gpu_descriptive_params_t* params)
{
    (void)ctx; (void)data; (void)means_out; (void)variances_out; (void)params;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_stats_gpu_variance_batch: operation failed");
    return false;
}

bool nimcp_stats_gpu_covariance_matrix(
    nimcp_gpu_context_t* ctx,
    const float* data,
    float* cov_out,
    const stats_gpu_covariance_params_t* params)
{
    (void)ctx; (void)data; (void)cov_out; (void)params;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_stats_gpu_covariance_matrix: operation failed");
    return false;
}

bool nimcp_stats_gpu_correlation_matrix(
    nimcp_gpu_context_t* ctx,
    const float* data,
    float* corr_out,
    const stats_gpu_covariance_params_t* params)
{
    (void)ctx; (void)data; (void)corr_out; (void)params;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_stats_gpu_correlation_matrix: operation failed");
    return false;
}

bool nimcp_stats_gpu_quantiles_batch(
    nimcp_gpu_context_t* ctx,
    const float* data,
    float* quantiles_out,
    const stats_gpu_quantile_params_t* params)
{
    (void)ctx; (void)data; (void)quantiles_out; (void)params;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_stats_gpu_quantiles_batch: operation failed");
    return false;
}

//=============================================================================
// Bootstrap Methods Stubs
//=============================================================================

bool nimcp_stats_gpu_bootstrap(
    nimcp_gpu_context_t* ctx,
    stats_gpu_rng_t* rng,
    const float* data,
    stats_gpu_bootstrap_result_t* result,
    const stats_gpu_bootstrap_params_t* params)
{
    (void)ctx; (void)rng; (void)data; (void)result; (void)params;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_stats_gpu_bootstrap: operation failed");
    return false;
}

bool nimcp_stats_gpu_bootstrap_ci(
    nimcp_gpu_context_t* ctx,
    stats_gpu_rng_t* rng,
    const float* data,
    uint32_t num_samples,
    stats_gpu_ci_result_t* ci_out,
    const stats_gpu_ci_params_t* params)
{
    (void)ctx; (void)rng; (void)data; (void)num_samples; (void)ci_out; (void)params;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_stats_gpu_bootstrap_ci: operation failed");
    return false;
}

void nimcp_stats_gpu_bootstrap_result_free(
    nimcp_gpu_context_t* ctx,
    stats_gpu_bootstrap_result_t* result)
{
    (void)ctx; (void)result;
}

//=============================================================================
// Distribution Operations Stubs
//=============================================================================

bool nimcp_stats_gpu_sample_normal(
    nimcp_gpu_context_t* ctx,
    stats_gpu_rng_t* rng,
    float mean,
    float std,
    float* samples,
    uint32_t n)
{
    (void)ctx; (void)rng; (void)mean; (void)std; (void)samples; (void)n;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_stats_gpu_sample_normal: operation failed");
    return false;
}

bool nimcp_stats_gpu_sample_uniform(
    nimcp_gpu_context_t* ctx,
    stats_gpu_rng_t* rng,
    float min_val,
    float max_val,
    float* samples,
    uint32_t n)
{
    (void)ctx; (void)rng; (void)min_val; (void)max_val; (void)samples; (void)n;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_stats_gpu_sample_uniform: operation failed");
    return false;
}

bool nimcp_stats_gpu_sample_distribution(
    nimcp_gpu_context_t* ctx,
    stats_gpu_rng_t* rng,
    float* samples,
    const stats_gpu_sample_params_t* params)
{
    (void)ctx; (void)rng; (void)samples; (void)params;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_stats_gpu_sample_distribution: operation failed");
    return false;
}

bool nimcp_stats_gpu_pdf_batch(
    nimcp_gpu_context_t* ctx,
    const float* points,
    float* pdf_out,
    const stats_gpu_density_params_t* params)
{
    (void)ctx; (void)points; (void)pdf_out; (void)params;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_stats_gpu_pdf_batch: operation failed");
    return false;
}

bool nimcp_stats_gpu_cdf_batch(
    nimcp_gpu_context_t* ctx,
    const float* points,
    float* cdf_out,
    const stats_gpu_density_params_t* params)
{
    (void)ctx; (void)points; (void)cdf_out; (void)params;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_stats_gpu_cdf_batch: operation failed");
    return false;
}

//=============================================================================
// Information Theory Stubs
//=============================================================================

bool nimcp_stats_gpu_entropy(
    nimcp_gpu_context_t* ctx,
    const float* data,
    float* entropy,
    const stats_gpu_entropy_params_t* params)
{
    (void)ctx; (void)data; (void)entropy; (void)params;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_stats_gpu_entropy: operation failed");
    return false;
}

bool nimcp_stats_gpu_mutual_information(
    nimcp_gpu_context_t* ctx,
    const float* data_x,
    const float* data_y,
    float* mi,
    const stats_gpu_entropy_params_t* params)
{
    (void)ctx; (void)data_x; (void)data_y; (void)mi; (void)params;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_stats_gpu_mutual_information: operation failed");
    return false;
}

bool nimcp_stats_gpu_kl_divergence(
    nimcp_gpu_context_t* ctx,
    const float* p,
    const float* q,
    uint32_t n,
    float* kl_divergence,
    float base)
{
    (void)ctx; (void)p; (void)q; (void)n; (void)kl_divergence; (void)base;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_stats_gpu_kl_divergence: operation failed");
    return false;
}

bool nimcp_stats_gpu_js_divergence(
    nimcp_gpu_context_t* ctx,
    const float* p,
    const float* q,
    uint32_t n,
    float* js_div,
    float base)
{
    (void)ctx; (void)p; (void)q; (void)n; (void)js_div; (void)base;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_stats_gpu_js_divergence: operation failed");
    return false;
}

//=============================================================================
// Matrix Operations Stubs
//=============================================================================

bool nimcp_stats_gpu_eigendecomposition(
    nimcp_gpu_context_t* ctx,
    const float* matrix,
    stats_gpu_eigen_result_t* result,
    const stats_gpu_eigen_params_t* params)
{
    (void)ctx; (void)matrix; (void)result; (void)params;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_stats_gpu_eigendecomposition: operation failed");
    return false;
}

bool nimcp_stats_gpu_svd(
    nimcp_gpu_context_t* ctx,
    const float* matrix,
    stats_gpu_svd_result_t* result,
    const stats_gpu_svd_params_t* params)
{
    (void)ctx; (void)matrix; (void)result; (void)params;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_stats_gpu_svd: operation failed");
    return false;
}

bool nimcp_stats_gpu_matrix_inverse(
    nimcp_gpu_context_t* ctx,
    const float* matrix,
    float* inverse,
    uint32_t n)
{
    (void)ctx; (void)matrix; (void)inverse; (void)n;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_stats_gpu_matrix_inverse: operation failed");
    return false;
}

bool nimcp_stats_gpu_matrix_pinverse(
    nimcp_gpu_context_t* ctx,
    const float* matrix,
    float* pinv,
    uint32_t m,
    uint32_t n,
    float tolerance)
{
    (void)ctx; (void)matrix; (void)pinv; (void)m; (void)n; (void)tolerance;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_stats_gpu_matrix_pinverse: operation failed");
    return false;
}

bool nimcp_stats_gpu_solve_linear(
    nimcp_gpu_context_t* ctx,
    const float* A,
    const float* b,
    float* x,
    uint32_t n)
{
    (void)ctx; (void)A; (void)b; (void)x; (void)n;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_stats_gpu_solve_linear: operation failed");
    return false;
}

bool nimcp_stats_gpu_cholesky(
    nimcp_gpu_context_t* ctx,
    const float* matrix,
    float* L,
    uint32_t n)
{
    (void)ctx; (void)matrix; (void)L; (void)n;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_stats_gpu_cholesky: operation failed");
    return false;
}

//=============================================================================
// Result Cleanup Stubs
//=============================================================================

void nimcp_stats_gpu_eigen_result_free(
    nimcp_gpu_context_t* ctx,
    stats_gpu_eigen_result_t* result)
{
    (void)ctx; (void)result;
}

void nimcp_stats_gpu_svd_result_free(
    nimcp_gpu_context_t* ctx,
    stats_gpu_svd_result_t* result)
{
    (void)ctx; (void)result;
}

void nimcp_stats_gpu_descriptive_result_free(
    nimcp_gpu_context_t* ctx,
    stats_gpu_descriptive_result_t* result)
{
    (void)ctx; (void)result;
}

//=============================================================================
// Statistics and Utilities Stubs
//=============================================================================

static stats_gpu_stats_t g_stats_gpu_stats = {0};

int nimcp_stats_gpu_get_stats(stats_gpu_stats_t* stats)
{
    if (!stats) return STATS_GPU_ERR_NULL_OUTPUT;
    memset(stats, 0, sizeof(stats_gpu_stats_t));
    return STATS_GPU_ERR_OK;
}

void nimcp_stats_gpu_reset_stats(void)
{
    memset(&g_stats_gpu_stats, 0, sizeof(g_stats_gpu_stats));
}

const char* nimcp_stats_gpu_get_last_error(void)
{
    return g_stats_gpu_error;
}

bool nimcp_stats_gpu_is_available(void)
{
    return false;
}

uint32_t nimcp_stats_gpu_recommended_samples(nimcp_gpu_context_t* ctx)
{
    (void)ctx;
    return 0;
}

uint32_t nimcp_stats_gpu_max_matrix_dim(nimcp_gpu_context_t* ctx)
{
    (void)ctx;
    return 0;
}

#endif /* !NIMCP_ENABLE_CUDA */
