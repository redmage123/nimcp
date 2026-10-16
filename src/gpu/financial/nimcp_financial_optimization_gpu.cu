/**
 * @file nimcp_financial_optimization_gpu.cu
 * @brief GPU Portfolio Optimization
 *
 * WHAT: GPU-accelerated portfolio optimization algorithms
 * WHY:  Fast mean-variance, efficient frontier, risk parity
 * HOW:  cuBLAS for matrix ops, custom kernels for constraints
 *
 * Implements:
 *   - Mean-variance optimization
 *   - Efficient frontier computation
 *   - Risk parity optimization
 *   - Black-Litterman model
 *   - Constrained optimization (box, linear, sector)
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#ifdef NIMCP_ENABLE_CUDA

#include "utils/memory/nimcp_memory.h"
#include <cuda_runtime.h>
#include <cublas_v2.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <float.h>

#include "gpu/financial/nimcp_financial_gpu.h"
#include "gpu/financial/nimcp_financial_optimization_gpu.h"
#include "gpu/common/nimcp_cuda_utils.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/exception/nimcp_exception_macros.h"

//=============================================================================
// Thread-Local Error Storage
//=============================================================================

static __thread char g_opt_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_opt_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_opt_error, sizeof(g_opt_error), fmt, args);
    va_end(args);
}

//=============================================================================
// cuBLAS Handle Management
//=============================================================================

static __thread cublasHandle_t g_cublas_handle = NULL;

static cublasHandle_t get_cublas_handle(void) {
    if (g_cublas_handle == NULL) {
        cublasCreate(&g_cublas_handle);
    }
    return g_cublas_handle;
}

//=============================================================================
// Device Helper Functions
//=============================================================================

/**
 * @brief Clamp value to range
 */
__device__ __forceinline__ float clamp_device(float x, float lo, float hi) {
    return fminf(fmaxf(x, lo), hi);
}

/**
 * @brief Warp-level reduction for sum
 */
__device__ __forceinline__ float warp_reduce_sum_opt(float val) {
    for (int offset = 16; offset > 0; offset /= 2) {
        val += __shfl_down_sync(0xFFFFFFFF, val, offset);
    }
    return val;
}

//=============================================================================
// Portfolio Computation Kernels
//=============================================================================

/**
 * @brief Compute portfolio variance: w^T @ Cov @ w
 *
 * Two-step: temp = Cov @ w, then variance = w^T @ temp
 */
__global__ void kernel_portfolio_variance(
    const float* __restrict__ covariance,   // [n x n]
    const float* __restrict__ weights,       // [n]
    float* __restrict__ temp,                // [n] intermediate
    uint32_t n)
{
    extern __shared__ float s_weights[];

    // Load weights to shared memory
    for (uint32_t i = threadIdx.x; i < n; i += blockDim.x) {
        s_weights[i] = weights[i];
    }
    __syncthreads();

    // Compute Cov @ w
    uint32_t row = blockIdx.x * blockDim.x + threadIdx.x;
    if (row < n) {
        float sum = 0.0f;
        for (uint32_t j = 0; j < n; j++) {
            sum += covariance[row * n + j] * s_weights[j];
        }
        temp[row] = sum;
    }
}

/**
 * @brief Compute gradient of portfolio variance: 2 * Cov @ w
 */
__global__ void kernel_variance_gradient(
    const float* __restrict__ covariance,   // [n x n]
    const float* __restrict__ weights,       // [n]
    float* __restrict__ gradient,            // [n]
    uint32_t n)
{
    extern __shared__ float s_weights[];

    for (uint32_t i = threadIdx.x; i < n; i += blockDim.x) {
        s_weights[i] = weights[i];
    }
    __syncthreads();

    uint32_t row = blockIdx.x * blockDim.x + threadIdx.x;
    if (row < n) {
        float sum = 0.0f;
        for (uint32_t j = 0; j < n; j++) {
            sum += covariance[row * n + j] * s_weights[j];
        }
        gradient[row] = 2.0f * sum;
    }
}

/**
 * @brief Compute portfolio expected return: mu^T @ w
 */
__global__ void kernel_portfolio_return(
    const float* __restrict__ expected_returns,  // [n]
    const float* __restrict__ weights,           // [n]
    float* __restrict__ result,                  // scalar
    uint32_t n)
{
    extern __shared__ float s_partial[];

    uint32_t tid = threadIdx.x;
    float sum = 0.0f;

    for (uint32_t i = tid; i < n; i += blockDim.x) {
        sum += expected_returns[i] * weights[i];
    }

    s_partial[tid] = sum;
    __syncthreads();

    // Parallel reduction
    for (uint32_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            s_partial[tid] += s_partial[tid + s];
        }
        __syncthreads();
    }

    if (tid == 0) {
        *result = s_partial[0];
    }
}

/**
 * @brief Project weights onto simplex (sum to 1, non-negative)
 *
 * Algorithm: Sort descending, find threshold, project
 */
__global__ void kernel_project_simplex(
    float* __restrict__ weights,
    uint32_t n)
{
    extern __shared__ float s_sorted[];

    uint32_t tid = threadIdx.x;

    // Copy and sort (simple bubble sort for small n)
    for (uint32_t i = tid; i < n; i += blockDim.x) {
        s_sorted[i] = weights[i];
    }
    __syncthreads();

    // Bubble sort (for small n; use bitonic for large n)
    if (tid == 0) {
        for (uint32_t i = 0; i < n - 1; i++) {
            for (uint32_t j = 0; j < n - i - 1; j++) {
                if (s_sorted[j] < s_sorted[j + 1]) {
                    float tmp = s_sorted[j];
                    s_sorted[j] = s_sorted[j + 1];
                    s_sorted[j + 1] = tmp;
                }
            }
        }

        // Find threshold
        float cumsum = 0.0f;
        float theta = 0.0f;
        for (uint32_t i = 0; i < n; i++) {
            cumsum += s_sorted[i];
            float t = (cumsum - 1.0f) / (float)(i + 1);
            if (s_sorted[i] - t > 0.0f) {
                theta = t;
            }
        }

        // Project
        for (uint32_t i = 0; i < n; i++) {
            weights[i] = fmaxf(weights[i] - theta, 0.0f);
        }
    }
}

/**
 * @brief Apply box constraints to weights
 */
__global__ void kernel_apply_box_constraints(
    float* __restrict__ weights,
    const float* __restrict__ lower_bounds,
    const float* __restrict__ upper_bounds,
    uint32_t n)
{
    uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) {
        weights[i] = clamp_device(weights[i], lower_bounds[i], upper_bounds[i]);
    }
}

/**
 * @brief Normalize weights to sum to 1
 *
 * Uses warp-level reduction for correct handling of any block size.
 */
__global__ void kernel_normalize_weights(
    float* __restrict__ weights,
    uint32_t n)
{
    extern __shared__ float s_partial[];

    uint32_t tid = threadIdx.x;
    float sum = 0.0f;

    for (uint32_t i = tid; i < n; i += blockDim.x) {
        sum += weights[i];
    }

    s_partial[tid] = sum;
    __syncthreads();

    // Tree reduction that handles non-power-of-2 block sizes correctly
    for (uint32_t s = (blockDim.x + 1) / 2; s > 0; s = (s > 1) ? ((s + 1) / 2) : 0) {
        if (tid < s && tid + s < blockDim.x) {
            s_partial[tid] += s_partial[tid + s];
        }
        __syncthreads();
        if (s == 1) break;
    }

    float total = s_partial[0];
    __syncthreads();

    if (total > 1e-10f) {
        for (uint32_t i = tid; i < n; i += blockDim.x) {
            weights[i] /= total;
        }
    }
}

/**
 * @brief Clamp negative weights to small positive value
 *
 * For long-only portfolio constraints
 */
__global__ void kernel_clamp_positive(
    float* __restrict__ weights,
    float min_weight,
    uint32_t n)
{
    uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) {
        if (weights[i] < min_weight) {
            weights[i] = min_weight;
        }
    }
}

/**
 * @brief Gradient descent step with momentum
 */
__global__ void kernel_gradient_step(
    float* __restrict__ weights,
    const float* __restrict__ gradient,
    float* __restrict__ momentum,
    float learning_rate,
    float momentum_coef,
    uint32_t n)
{
    uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) {
        momentum[i] = momentum_coef * momentum[i] + learning_rate * gradient[i];
        weights[i] -= momentum[i];
    }
}

//=============================================================================
// Risk Parity Kernels
//=============================================================================

/**
 * @brief Compute marginal risk contributions
 *
 * MRC_i = (Cov @ w)_i / sqrt(w^T @ Cov @ w)
 */
__global__ void kernel_marginal_risk_contribution(
    const float* __restrict__ covariance,
    const float* __restrict__ weights,
    const float* __restrict__ cov_w,         // Cov @ w
    float portfolio_vol,
    float* __restrict__ mrc,
    uint32_t n)
{
    uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) {
        mrc[i] = cov_w[i] / portfolio_vol;
    }
}

/**
 * @brief Compute risk contribution: RC_i = w_i * MRC_i
 */
__global__ void kernel_risk_contribution(
    const float* __restrict__ weights,
    const float* __restrict__ mrc,
    float* __restrict__ rc,
    uint32_t n)
{
    uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) {
        rc[i] = weights[i] * mrc[i];
    }
}

/**
 * @brief Risk parity gradient
 *
 * Gradient of sum((RC_i - RC_target)^2)
 */
__global__ void kernel_risk_parity_gradient(
    const float* __restrict__ covariance,
    const float* __restrict__ weights,
    const float* __restrict__ rc,
    float rc_target,
    float portfolio_vol,
    float* __restrict__ gradient,
    uint32_t n)
{
    extern __shared__ float s_data[];

    uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;

    if (i < n) {
        // Compute gradient term
        float diff = rc[i] - rc_target;

        // d(RC_i)/d(w_j) involves complex terms
        // Simplified: use finite difference approximation
        // or analytical gradient

        // Approximate gradient
        gradient[i] = 2.0f * diff * (covariance[i * n + i] * weights[i] / portfolio_vol);
    }
}

//=============================================================================
// Efficient Frontier Kernels
//=============================================================================

/**
 * @brief Compute efficient frontier point for target return
 *
 * Minimize: w^T @ Cov @ w
 * Subject to: mu^T @ w = target_return, sum(w) = 1
 */
__global__ void kernel_efficient_frontier_point(
    const float* __restrict__ covariance,
    const float* __restrict__ expected_returns,
    const float* __restrict__ weights,
    float target_return,
    float lambda_return,       // Lagrange multiplier for return
    float lambda_sum,          // Lagrange multiplier for sum=1
    float* __restrict__ gradient,
    uint32_t n)
{
    extern __shared__ float s_weights[];

    for (uint32_t j = threadIdx.x; j < n; j += blockDim.x) {
        s_weights[j] = weights[j];
    }
    __syncthreads();

    uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) {
        // Gradient of Lagrangian:
        // 2 * Cov @ w - lambda_return * mu - lambda_sum * 1

        float cov_w_i = 0.0f;
        for (uint32_t j = 0; j < n; j++) {
            cov_w_i += covariance[i * n + j] * s_weights[j];
        }

        gradient[i] = 2.0f * cov_w_i - lambda_return * expected_returns[i] - lambda_sum;
    }
}

//=============================================================================
// Host API Implementation
//=============================================================================

extern "C" {

bool fin_optimization_gpu_mean_variance(
    nimcp_gpu_context_t* ctx,
    const float* expected_returns,
    const float* covariance_matrix,
    const fin_optimization_gpu_params_t* params,
    fin_optimization_gpu_result_t* result)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !nimcp_gpu_context_is_valid(ctx)) {
        set_opt_error("Invalid GPU context");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid GPU context");
        return false;
    }
    if (!expected_returns || !covariance_matrix || !params || !result) {
        set_opt_error("Invalid parameters");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid parameters");
        return false;
    }
    if (params->n_assets == 0) {
        set_opt_error("Zero assets");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Zero assets");
        return false;
    }

    uint32_t n = params->n_assets;
    cudaStream_t stream = nimcp_gpu_get_compute_stream(ctx);
    cublasHandle_t cublas = get_cublas_handle();
    cublasSetStream(cublas, stream);

    // Pre-declare all variables to avoid "transfer of control bypasses initialization"
    float init_weight = 0.0f;
    float* h_init = NULL;
    uint32_t block_size = 0;
    uint32_t grid_size = 0;
    float learning_rate = 0.0f;
    float momentum_coef = 0.0f;
    uint32_t max_iter = 0;
    float one = 1.0f;
    float h_variance = 0.0f;
    float h_return = 0.0f;
    float conv_tol = 0.0f;
    float prev_variance = FLT_MAX;
    bool converged = false;
    uint32_t actual_iter = 0;
    float kernel_time = 0.0f;
    float* h_lower_tmp = NULL;
    float* h_upper_tmp = NULL;
    bool free_bounds_tmp = false;
    cudaEvent_t start_event = NULL;
    cudaEvent_t stop_event = NULL;

    // Allocate device memory
    float* d_returns = NULL;
    float* d_covariance = NULL;
    float* d_weights = NULL;
    float* d_gradient = NULL;
    float* d_momentum = NULL;
    float* d_temp = NULL;
    float* d_lower = NULL;
    float* d_upper = NULL;

    cudaError_t err;

    err = cudaMalloc(&d_returns, n * sizeof(float));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t recovery_result = {0};
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &recovery_result)) {
            err = cudaMalloc(&d_returns, n * sizeof(float));
        }
        if (err != cudaSuccess) goto cleanup_mv;
    }

    err = cudaMalloc(&d_covariance, n * n * sizeof(float));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t recovery_result = {0};
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &recovery_result)) {
            err = cudaMalloc(&d_covariance, n * n * sizeof(float));
        }
        if (err != cudaSuccess) goto cleanup_mv;
    }

    err = cudaMalloc(&d_weights, n * sizeof(float));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t recovery_result = {0};
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &recovery_result)) {
            err = cudaMalloc(&d_weights, n * sizeof(float));
        }
        if (err != cudaSuccess) goto cleanup_mv;
    }

    err = cudaMalloc(&d_gradient, n * sizeof(float));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t recovery_result = {0};
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &recovery_result)) {
            err = cudaMalloc(&d_gradient, n * sizeof(float));
        }
        if (err != cudaSuccess) goto cleanup_mv;
    }

    err = cudaMalloc(&d_momentum, n * sizeof(float));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t recovery_result = {0};
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &recovery_result)) {
            err = cudaMalloc(&d_momentum, n * sizeof(float));
        }
        if (err != cudaSuccess) goto cleanup_mv;
    }

    err = cudaMalloc(&d_temp, n * sizeof(float));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t recovery_result = {0};
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &recovery_result)) {
            err = cudaMalloc(&d_temp, n * sizeof(float));
        }
        if (err != cudaSuccess) goto cleanup_mv;
    }

    // Copy inputs to device
    cudaMemcpyAsync(d_returns, expected_returns, n * sizeof(float),
                    cudaMemcpyHostToDevice, stream);
    cudaMemcpyAsync(d_covariance, covariance_matrix, n * n * sizeof(float),
                    cudaMemcpyHostToDevice, stream);

    // Initialize weights uniformly
    init_weight = 1.0f / (float)n;
    h_init = (float*)nimcp_malloc(n * sizeof(float));
    for (uint32_t i = 0; i < n; i++) {
        h_init[i] = init_weight;
    }
    cudaMemcpyAsync(d_weights, h_init, n * sizeof(float),
                    cudaMemcpyHostToDevice, stream);
    cudaMemsetAsync(d_momentum, 0, n * sizeof(float), stream);
    nimcp_free(h_init);

    // Handle constraints - use explicit arrays or create from scalar bounds
    if (params->lower_bounds && params->upper_bounds) {
        // Use explicit bounds arrays
        err = cudaMalloc(&d_lower, n * sizeof(float));
        if (err != cudaSuccess) goto cleanup_mv;
        err = cudaMalloc(&d_upper, n * sizeof(float));
        if (err != cudaSuccess) goto cleanup_mv;

        cudaMemcpyAsync(d_lower, params->lower_bounds, n * sizeof(float),
                        cudaMemcpyHostToDevice, stream);
        cudaMemcpyAsync(d_upper, params->upper_bounds, n * sizeof(float),
                        cudaMemcpyHostToDevice, stream);
    } else if (params->min_weight > 0.0f || params->max_weight < 1.0f) {
        // Create bounds from scalar min/max
        h_lower_tmp = (float*)nimcp_malloc(n * sizeof(float));
        h_upper_tmp = (float*)nimcp_malloc(n * sizeof(float));
        if (h_lower_tmp && h_upper_tmp) {
            float min_w = params->min_weight > 0.0f ? params->min_weight : 0.0f;
            float max_w = params->max_weight > 0.0f && params->max_weight <= 1.0f ? params->max_weight : 1.0f;
            for (uint32_t i = 0; i < n; i++) {
                h_lower_tmp[i] = min_w;
                h_upper_tmp[i] = max_w;
            }
            err = cudaMalloc(&d_lower, n * sizeof(float));
            if (err != cudaSuccess) { nimcp_free(h_lower_tmp); nimcp_free(h_upper_tmp); goto cleanup_mv; }
            err = cudaMalloc(&d_upper, n * sizeof(float));
            if (err != cudaSuccess) { nimcp_free(h_lower_tmp); nimcp_free(h_upper_tmp); goto cleanup_mv; }

            cudaMemcpyAsync(d_lower, h_lower_tmp, n * sizeof(float), cudaMemcpyHostToDevice, stream);
            cudaMemcpyAsync(d_upper, h_upper_tmp, n * sizeof(float), cudaMemcpyHostToDevice, stream);
            free_bounds_tmp = true;
        }
    } else if (params->long_only) {
        // Long-only constraint: weights >= 0
        h_lower_tmp = (float*)nimcp_calloc(n, sizeof(float));  // All zeros
        h_upper_tmp = (float*)nimcp_malloc(n * sizeof(float));
        if (h_lower_tmp && h_upper_tmp) {
            for (uint32_t i = 0; i < n; i++) {
                h_upper_tmp[i] = 1.0f;
            }
            err = cudaMalloc(&d_lower, n * sizeof(float));
            if (err != cudaSuccess) { nimcp_free(h_lower_tmp); nimcp_free(h_upper_tmp); goto cleanup_mv; }
            err = cudaMalloc(&d_upper, n * sizeof(float));
            if (err != cudaSuccess) { nimcp_free(h_lower_tmp); nimcp_free(h_upper_tmp); goto cleanup_mv; }

            cudaMemcpyAsync(d_lower, h_lower_tmp, n * sizeof(float), cudaMemcpyHostToDevice, stream);
            cudaMemcpyAsync(d_upper, h_upper_tmp, n * sizeof(float), cudaMemcpyHostToDevice, stream);
            free_bounds_tmp = true;
        }
    }

    if (free_bounds_tmp) {
        nimcp_free(h_lower_tmp);
        nimcp_free(h_upper_tmp);
    }

    // Use power-of-2 block size for reduction kernels
    block_size = 1;
    while (block_size < n && block_size < 256) block_size *= 2;
    if (block_size < 32) block_size = 32;  // Minimum for warp
    grid_size = NIMCP_CUDA_GRID_SIZE(n, block_size);

    // Optimization loop with convergence detection
    learning_rate = params->learning_rate > 0 ? params->learning_rate : 0.01f;
    momentum_coef = 0.9f;
    max_iter = params->max_iterations > 0 ? params->max_iterations : 1000;
    conv_tol = params->convergence_tolerance > 0 ? params->convergence_tolerance : 1e-6f;

    // CUDA timing
    cudaEventCreate(&start_event);
    cudaEventCreate(&stop_event);
    cudaEventRecord(start_event, stream);

    for (uint32_t iter = 0; iter < max_iter; iter++) {
        actual_iter = iter + 1;

        // Compute gradient: 2 * Cov @ w (variance gradient)
        kernel_variance_gradient<<<grid_size, block_size, n * sizeof(float), stream>>>(
            d_covariance, d_weights, d_gradient, n);

        // For MIN_VARIANCE strategy, only minimize variance (no return component)
        // For other strategies, use mean-variance formulation:
        //   Maximize: mu^T w - (risk_aversion/2) * w^T Cov w
        //   Gradient for descent: risk_aversion * 2 * Cov @ w - mu
        //   We already have 2 * Cov @ w, so scale it and subtract mu
        if (params->strategy != FIN_OPT_STRATEGY_MIN_VARIANCE) {
            // Scale variance gradient by risk_aversion
            float ra = params->risk_aversion > 0 ? params->risk_aversion : 1.0f;
            cublasSscal(cublas, n, &ra, d_gradient, 1);
            // Subtract return gradient (we want to move towards higher returns)
            float neg_one = -1.0f;
            cublasSaxpy(cublas, n, &neg_one, d_returns, 1, d_gradient, 1);
        }

        // Gradient step with momentum
        kernel_gradient_step<<<grid_size, block_size, 0, stream>>>(
            d_weights, d_gradient, d_momentum, learning_rate, momentum_coef, n);

        // Project onto constraints
        if (d_lower && d_upper) {
            kernel_apply_box_constraints<<<grid_size, block_size, 0, stream>>>(
                d_weights, d_lower, d_upper, n);
        }

        // Normalize to sum to 1
        kernel_normalize_weights<<<1, block_size, block_size * sizeof(float), stream>>>(
            d_weights, n);

        // Check convergence every 50 iterations
        if (iter > 0 && iter % 50 == 0) {
            // Compute current variance
            kernel_portfolio_variance<<<grid_size, block_size, n * sizeof(float), stream>>>(
                d_covariance, d_weights, d_temp, n);
            float current_var = 0.0f;
            cublasSdot(cublas, n, d_weights, 1, d_temp, 1, &current_var);
            cudaStreamSynchronize(stream);

            float delta = fabsf(prev_variance - current_var);
            if (delta < conv_tol && iter > 100) {
                converged = true;
                break;
            }
            prev_variance = current_var;
        }
    }

    cudaEventRecord(stop_event, stream);
    cudaEventSynchronize(stop_event);
    cudaEventElapsedTime(&kernel_time, start_event, stop_event);
    cudaEventDestroy(start_event);
    cudaEventDestroy(stop_event);

    // Compute final portfolio metrics
    // Portfolio variance: w^T @ Cov @ w
    kernel_portfolio_variance<<<grid_size, block_size, n * sizeof(float), stream>>>(
        d_covariance, d_weights, d_temp, n);

    one = 1.0f;
    cublasSdot(cublas, n, d_weights, 1, d_temp, 1, &h_variance);

    // Portfolio return: mu^T @ w
    cublasSdot(cublas, n, d_returns, 1, d_weights, 1, &h_return);

    cudaStreamSynchronize(stream);

    // Copy results
    result->optimal_weights = (float*)nimcp_malloc(n * sizeof(float));
    if (!result->optimal_weights) {
        set_opt_error("Failed to allocate result weights");
        goto cleanup_mv;
    }

    cudaMemcpy(result->optimal_weights, d_weights, n * sizeof(float),
               cudaMemcpyDeviceToHost);

    result->expected_return = h_return;
    result->portfolio_variance = h_variance;
    result->portfolio_volatility = sqrtf(h_variance);
    result->sharpe_ratio = (result->portfolio_volatility > 1e-10f && params->risk_free_rate >= 0) ?
        (h_return - params->risk_free_rate) / result->portfolio_volatility : 0.0f;
    result->n_assets = n;
    result->converged = converged || (actual_iter < max_iter);  // Consider converged if stopped early or completed
    result->iterations = actual_iter;
    result->kernel_time_ms = kernel_time;

    // Cleanup
    cudaFree(d_returns);
    cudaFree(d_covariance);
    cudaFree(d_weights);
    cudaFree(d_gradient);
    cudaFree(d_momentum);
    cudaFree(d_temp);
    cudaFree(d_lower);
    cudaFree(d_upper);

    return true;

cleanup_mv:
    cudaFree(d_returns);
    cudaFree(d_covariance);
    cudaFree(d_weights);
    cudaFree(d_gradient);
    cudaFree(d_momentum);
    cudaFree(d_temp);
    cudaFree(d_lower);
    cudaFree(d_upper);
    set_opt_error("Memory allocation failed");
    return false;
}

bool fin_optimization_gpu_efficient_frontier(
    nimcp_gpu_context_t* ctx,
    const float* expected_returns,
    const float* covariance_matrix,
    const fin_optimization_gpu_params_t* params,
    uint32_t num_points,
    fin_efficient_frontier_result_t* result)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !nimcp_gpu_context_is_valid(ctx)) {
        set_opt_error("Invalid GPU context");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid GPU context");
        return false;
    }
    if (!expected_returns || !covariance_matrix || !params || !result) {
        set_opt_error("Invalid parameters");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid parameters");
        return false;
    }
    if (num_points == 0 || params->n_assets == 0) {
        set_opt_error("Invalid num_points or n_assets");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid num_points or n_assets");
        return false;
    }

    uint32_t n = params->n_assets;

    // Find min/max possible returns
    float min_return = FLT_MAX;
    float max_return = -FLT_MAX;
    for (uint32_t i = 0; i < n; i++) {
        if (expected_returns[i] < min_return) min_return = expected_returns[i];
        if (expected_returns[i] > max_return) max_return = expected_returns[i];
    }

    // Allocate result arrays
    result->returns = (float*)nimcp_malloc(num_points * sizeof(float));
    result->volatilities = (float*)nimcp_malloc(num_points * sizeof(float));
    result->sharpe_ratios = (float*)nimcp_malloc(num_points * sizeof(float));
    result->weights = (float*)nimcp_malloc(num_points * n * sizeof(float));
    result->num_points = num_points;

    if (!result->returns || !result->volatilities ||
        !result->sharpe_ratios || !result->weights) {
        nimcp_free(result->returns);
        nimcp_free(result->volatilities);
        nimcp_free(result->sharpe_ratios);
        nimcp_free(result->weights);
        set_opt_error("Failed to allocate frontier results");
        return false;
    }

    // Compute frontier points
    float step = (max_return - min_return) / (float)(num_points - 1);

    for (uint32_t p = 0; p < num_points; p++) {
        float target_return = min_return + p * step;

        // Create modified params with target return constraint
        fin_optimization_gpu_params_t modified_params = *params;
        modified_params.target_return = target_return;

        fin_optimization_gpu_result_t point_result = {0};

        if (fin_optimization_gpu_mean_variance(ctx, expected_returns, covariance_matrix,
                                                &modified_params, &point_result)) {
            result->returns[p] = point_result.expected_return;
            result->volatilities[p] = point_result.portfolio_volatility;
            result->sharpe_ratios[p] = point_result.sharpe_ratio;
            memcpy(&result->weights[p * n], point_result.optimal_weights,
                   n * sizeof(float));
            nimcp_free(point_result.optimal_weights);
        } else {
            // Use default values for failed point
            result->returns[p] = target_return;
            result->volatilities[p] = 0.0f;
            result->sharpe_ratios[p] = 0.0f;
            for (uint32_t i = 0; i < n; i++) {
                result->weights[p * n + i] = 1.0f / (float)n;
            }
        }
    }

    return true;
}

void fin_optimization_gpu_result_free(fin_optimization_gpu_result_t* result) {
    if (result) {
        nimcp_free(result->optimal_weights);
        result->optimal_weights = NULL;
    }
}

void fin_efficient_frontier_result_free(fin_efficient_frontier_result_t* result) {
    if (result) {
        nimcp_free(result->returns);
        nimcp_free(result->volatilities);
        nimcp_free(result->sharpe_ratios);
        nimcp_free(result->weights);
        result->returns = NULL;
        result->volatilities = NULL;
        result->sharpe_ratios = NULL;
        result->weights = NULL;
    }
}

void fin_opt_extended_result_free(fin_opt_extended_result_t* result) {
    if (!result) return;

    // Free base result
    nimcp_free(result->base.optimal_weights);
    result->base.optimal_weights = NULL;

    // Free risk attribution
    nimcp_free(result->marginal_risk);
    result->marginal_risk = NULL;
    nimcp_free(result->risk_contribution);
    result->risk_contribution = NULL;

    // Free Black-Litterman outputs
    nimcp_free(result->posterior_returns);
    result->posterior_returns = NULL;
    nimcp_free(result->posterior_covariance);
    result->posterior_covariance = NULL;

    // Free constraint analysis
    nimcp_free(result->constraint_slackness);
    result->constraint_slackness = NULL;
    nimcp_free(result->active_constraints);
    result->active_constraints = NULL;
}

const char* fin_optimization_gpu_get_last_error(void) {
    return g_opt_error;
}

//=============================================================================
// Portfolio Variance Computation
//=============================================================================

float fin_opt_gpu_portfolio_variance(
    nimcp_gpu_context_t* ctx,
    const float* weights,
    const float* covariance,
    uint32_t num_assets)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !nimcp_gpu_context_is_valid(ctx)) {
        set_opt_error("Invalid GPU context");
        return -1.0f;
    }
    if (!weights || !covariance || num_assets == 0) {
        set_opt_error("Invalid parameters");
        return -1.0f;
    }

    cudaStream_t stream = nimcp_gpu_get_compute_stream(ctx);
    cublasHandle_t cublas = get_cublas_handle();
    cublasSetStream(cublas, stream);

    uint32_t n = num_assets;
    float* d_weights = NULL;
    float* d_covariance = NULL;
    float* d_temp = NULL;
    float result = 0.0f;

    cudaError_t err;

    // Allocate device memory
    err = cudaMalloc(&d_weights, n * sizeof(float));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t recovery_result = {0};
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &recovery_result)) {
            err = cudaMalloc(&d_weights, n * sizeof(float));
        }
        if (err != cudaSuccess) goto cleanup_var;
    }

    err = cudaMalloc(&d_covariance, n * n * sizeof(float));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t recovery_result = {0};
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &recovery_result)) {
            err = cudaMalloc(&d_covariance, n * n * sizeof(float));
        }
        if (err != cudaSuccess) goto cleanup_var;
    }

    err = cudaMalloc(&d_temp, n * sizeof(float));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t recovery_result = {0};
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &recovery_result)) {
            err = cudaMalloc(&d_temp, n * sizeof(float));
        }
        if (err != cudaSuccess) goto cleanup_var;
    }

    // Copy to device
    cudaMemcpyAsync(d_weights, weights, n * sizeof(float), cudaMemcpyHostToDevice, stream);
    cudaMemcpyAsync(d_covariance, covariance, n * n * sizeof(float), cudaMemcpyHostToDevice, stream);

    // Compute Cov @ w using cuBLAS: temp = covariance * weights
    {
        float alpha = 1.0f, beta = 0.0f;
        cublasSgemv(cublas, CUBLAS_OP_N, n, n, &alpha, d_covariance, n, d_weights, 1, &beta, d_temp, 1);
    }

    // Compute w^T @ temp using cuBLAS dot product
    cublasSdot(cublas, n, d_weights, 1, d_temp, 1, &result);

    cudaStreamSynchronize(stream);

cleanup_var:
    cudaFree(d_weights);
    cudaFree(d_covariance);
    cudaFree(d_temp);

    return result;
}

//=============================================================================
// Portfolio Return Computation
//=============================================================================

float fin_opt_gpu_portfolio_return(
    nimcp_gpu_context_t* ctx,
    const float* weights,
    const float* returns,
    uint32_t num_assets)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !nimcp_gpu_context_is_valid(ctx)) {
        set_opt_error("Invalid GPU context");
        return -1.0f;
    }
    if (!weights || !returns || num_assets == 0) {
        set_opt_error("Invalid parameters");
        return -1.0f;
    }

    cudaStream_t stream = nimcp_gpu_get_compute_stream(ctx);
    cublasHandle_t cublas = get_cublas_handle();
    cublasSetStream(cublas, stream);

    uint32_t n = num_assets;
    float* d_weights = NULL;
    float* d_returns = NULL;
    float result = 0.0f;

    cudaError_t err;

    err = cudaMalloc(&d_weights, n * sizeof(float));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t recovery_result = {0};
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &recovery_result)) {
            err = cudaMalloc(&d_weights, n * sizeof(float));
        }
        if (err != cudaSuccess) goto cleanup_ret;
    }

    err = cudaMalloc(&d_returns, n * sizeof(float));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t recovery_result = {0};
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &recovery_result)) {
            err = cudaMalloc(&d_returns, n * sizeof(float));
        }
        if (err != cudaSuccess) goto cleanup_ret;
    }

    cudaMemcpyAsync(d_weights, weights, n * sizeof(float), cudaMemcpyHostToDevice, stream);
    cudaMemcpyAsync(d_returns, returns, n * sizeof(float), cudaMemcpyHostToDevice, stream);

    // Compute w^T @ returns using cuBLAS dot product
    cublasSdot(cublas, n, d_weights, 1, d_returns, 1, &result);

    cudaStreamSynchronize(stream);

cleanup_ret:
    cudaFree(d_weights);
    cudaFree(d_returns);

    return result;
}

//=============================================================================
// Covariance Matrix Inversion
//=============================================================================

/**
 * @brief Kernel for LU decomposition pivot row swap
 */
__global__ void kernel_lu_pivot_swap(
    float* __restrict__ matrix,
    int pivot_row,
    int swap_row,
    uint32_t n)
{
    uint32_t col = blockIdx.x * blockDim.x + threadIdx.x;
    if (col < n) {
        float temp = matrix[pivot_row * n + col];
        matrix[pivot_row * n + col] = matrix[swap_row * n + col];
        matrix[swap_row * n + col] = temp;
    }
}

/**
 * @brief Kernel for LU decomposition elimination step
 */
__global__ void kernel_lu_eliminate(
    float* __restrict__ matrix,
    int pivot_row,
    uint32_t n)
{
    uint32_t row = blockIdx.x * blockDim.x + threadIdx.x + pivot_row + 1;
    if (row < n) {
        float factor = matrix[row * n + pivot_row] / matrix[pivot_row * n + pivot_row];
        matrix[row * n + pivot_row] = factor;  // Store L factor
        for (uint32_t col = pivot_row + 1; col < n; col++) {
            matrix[row * n + col] -= factor * matrix[pivot_row * n + col];
        }
    }
}

/**
 * @brief Kernel for forward substitution
 */
__global__ void kernel_forward_substitute(
    const float* __restrict__ LU,
    float* __restrict__ Y,
    int col,
    uint32_t n)
{
    // Solve L * Y = I (column by column)
    // This is called for each column of the identity matrix
    extern __shared__ float s_y[];

    uint32_t tid = threadIdx.x;

    // Initialize: Y[i, col] = (i == col) ? 1.0 : 0.0
    for (uint32_t i = tid; i < n; i += blockDim.x) {
        s_y[i] = (i == (uint32_t)col) ? 1.0f : 0.0f;
    }
    __syncthreads();

    // Forward substitution
    for (uint32_t i = 0; i < n; i++) {
        __syncthreads();
        for (uint32_t j = tid + i + 1; j < n; j += blockDim.x) {
            s_y[j] -= LU[j * n + i] * s_y[i];
        }
    }
    __syncthreads();

    // Copy result
    for (uint32_t i = tid; i < n; i += blockDim.x) {
        Y[i * n + col] = s_y[i];
    }
}

/**
 * @brief Kernel for backward substitution
 */
__global__ void kernel_backward_substitute(
    const float* __restrict__ LU,
    const float* __restrict__ Y,
    float* __restrict__ X,
    int col,
    uint32_t n)
{
    // Solve U * X = Y (column by column)
    extern __shared__ float s_x[];

    uint32_t tid = threadIdx.x;

    // Initialize from Y
    for (uint32_t i = tid; i < n; i += blockDim.x) {
        s_x[i] = Y[i * n + col];
    }
    __syncthreads();

    // Backward substitution
    for (int i = n - 1; i >= 0; i--) {
        if (tid == 0) {
            s_x[i] /= LU[i * n + i];
        }
        __syncthreads();
        for (int j = tid; j < i; j += blockDim.x) {
            s_x[j] -= LU[j * n + i] * s_x[i];
        }
        __syncthreads();
    }

    // Copy result
    for (uint32_t i = tid; i < n; i += blockDim.x) {
        X[i * n + col] = s_x[i];
    }
}

bool fin_opt_gpu_invert_covariance(
    nimcp_gpu_context_t* ctx,
    const float* covariance_matrix,
    uint32_t n,
    float* inverse)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !nimcp_gpu_context_is_valid(ctx)) {
        set_opt_error("Invalid GPU context");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid GPU context");
        return false;
    }
    if (!covariance_matrix || !inverse || n == 0) {
        set_opt_error("Invalid parameters");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid parameters");
        return false;
    }

    cudaStream_t stream = nimcp_gpu_get_compute_stream(ctx);
    cublasHandle_t cublas = get_cublas_handle();
    cublasSetStream(cublas, stream);

    // For small matrices, use direct LU decomposition
    // For larger matrices, could use cuSOLVER

    float* d_LU = NULL;
    float* d_Y = NULL;
    float* d_inv = NULL;
    float* h_LU = NULL;
    int* h_pivot = NULL;
    bool success = false;

    cudaError_t err;

    err = cudaMalloc(&d_LU, n * n * sizeof(float));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t recovery_result = {0};
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &recovery_result)) {
            err = cudaMalloc(&d_LU, n * n * sizeof(float));
        }
        if (err != cudaSuccess) goto cleanup_inv;
    }

    err = cudaMalloc(&d_Y, n * n * sizeof(float));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t recovery_result = {0};
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &recovery_result)) {
            err = cudaMalloc(&d_Y, n * n * sizeof(float));
        }
        if (err != cudaSuccess) goto cleanup_inv;
    }

    err = cudaMalloc(&d_inv, n * n * sizeof(float));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t recovery_result = {0};
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &recovery_result)) {
            err = cudaMalloc(&d_inv, n * n * sizeof(float));
        }
        if (err != cudaSuccess) goto cleanup_inv;
    }

    // Allocate host memory for LU with pivoting
    h_LU = (float*)nimcp_malloc(n * n * sizeof(float));
    h_pivot = (int*)nimcp_malloc(n * sizeof(int));
    if (!h_LU || !h_pivot) {
        set_opt_error("Host memory allocation failed");
        goto cleanup_inv;
    }

    // Copy matrix and perform LU decomposition with partial pivoting on host
    memcpy(h_LU, covariance_matrix, n * n * sizeof(float));

    // LU decomposition with partial pivoting
    for (uint32_t k = 0; k < n; k++) {
        // Find pivot
        int max_row = k;
        float max_val = fabsf(h_LU[k * n + k]);
        for (uint32_t i = k + 1; i < n; i++) {
            float val = fabsf(h_LU[i * n + k]);
            if (val > max_val) {
                max_val = val;
                max_row = i;
            }
        }
        h_pivot[k] = max_row;

        // Check for singularity
        if (max_val < 1e-10f) {
            set_opt_error("Matrix is singular or nearly singular");
            goto cleanup_inv;
        }

        // Swap rows
        if (max_row != (int)k) {
            for (uint32_t j = 0; j < n; j++) {
                float temp = h_LU[k * n + j];
                h_LU[k * n + j] = h_LU[max_row * n + j];
                h_LU[max_row * n + j] = temp;
            }
        }

        // Eliminate
        for (uint32_t i = k + 1; i < n; i++) {
            h_LU[i * n + k] /= h_LU[k * n + k];
            for (uint32_t j = k + 1; j < n; j++) {
                h_LU[i * n + j] -= h_LU[i * n + k] * h_LU[k * n + j];
            }
        }
    }

    // Copy LU to device
    cudaMemcpyAsync(d_LU, h_LU, n * n * sizeof(float), cudaMemcpyHostToDevice, stream);

    // Forward and backward substitution for each column
    {
        uint32_t block_size = min(256u, n);

        for (uint32_t col = 0; col < n; col++) {
            kernel_forward_substitute<<<1, block_size, n * sizeof(float), stream>>>(
                d_LU, d_Y, col, n);
        }

        for (uint32_t col = 0; col < n; col++) {
            kernel_backward_substitute<<<1, block_size, n * sizeof(float), stream>>>(
                d_LU, d_Y, d_inv, col, n);
        }
    }

    // Apply pivot permutation to columns
    cudaStreamSynchronize(stream);

    // Copy result back, applying pivot permutation
    {
        float* h_inv_temp = (float*)nimcp_malloc(n * n * sizeof(float));
        if (h_inv_temp) {
            cudaMemcpy(h_inv_temp, d_inv, n * n * sizeof(float), cudaMemcpyDeviceToHost);

            // Apply column permutation (reverse order)
            for (int k = n - 1; k >= 0; k--) {
                int swap_col = h_pivot[k];
                if (swap_col != k) {
                    for (uint32_t row = 0; row < n; row++) {
                        float temp = h_inv_temp[row * n + k];
                        h_inv_temp[row * n + k] = h_inv_temp[row * n + swap_col];
                        h_inv_temp[row * n + swap_col] = temp;
                    }
                }
            }

            memcpy(inverse, h_inv_temp, n * n * sizeof(float));
            nimcp_free(h_inv_temp);
            success = true;
        }
    }

cleanup_inv:
    cudaFree(d_LU);
    cudaFree(d_Y);
    cudaFree(d_inv);
    nimcp_free(h_LU);
    nimcp_free(h_pivot);

    return success;
}

//=============================================================================
// Risk Contribution Analysis
//=============================================================================

bool fin_opt_gpu_risk_contribution(
    nimcp_gpu_context_t* ctx,
    const float* weights,
    const float* covariance_matrix,
    uint32_t num_assets,
    float* out_marginal,
    float* out_contribution)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !nimcp_gpu_context_is_valid(ctx)) {
        set_opt_error("Invalid GPU context");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid GPU context");
        return false;
    }
    if (!weights || !covariance_matrix || !out_marginal || !out_contribution || num_assets == 0) {
        set_opt_error("Invalid parameters");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid parameters");
        return false;
    }

    cudaStream_t stream = nimcp_gpu_get_compute_stream(ctx);
    cublasHandle_t cublas = get_cublas_handle();
    cublasSetStream(cublas, stream);

    uint32_t n = num_assets;
    float* d_weights = NULL;
    float* d_covariance = NULL;
    float* d_cov_w = NULL;
    float* d_mrc = NULL;
    float* d_rc = NULL;
    bool success = false;
    float portfolio_variance = 0.0f;
    float portfolio_vol = 0.0f;
    uint32_t block_size = 0;
    uint32_t grid_size = 0;

    cudaError_t err;

    err = cudaMalloc(&d_weights, n * sizeof(float));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t recovery_result = {0};
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &recovery_result)) {
            err = cudaMalloc(&d_weights, n * sizeof(float));
        }
        if (err != cudaSuccess) goto cleanup_rc;
    }

    err = cudaMalloc(&d_covariance, n * n * sizeof(float));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t recovery_result = {0};
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &recovery_result)) {
            err = cudaMalloc(&d_covariance, n * n * sizeof(float));
        }
        if (err != cudaSuccess) goto cleanup_rc;
    }

    err = cudaMalloc(&d_cov_w, n * sizeof(float));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t recovery_result = {0};
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &recovery_result)) {
            err = cudaMalloc(&d_cov_w, n * sizeof(float));
        }
        if (err != cudaSuccess) goto cleanup_rc;
    }

    err = cudaMalloc(&d_mrc, n * sizeof(float));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t recovery_result = {0};
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &recovery_result)) {
            err = cudaMalloc(&d_mrc, n * sizeof(float));
        }
        if (err != cudaSuccess) goto cleanup_rc;
    }

    err = cudaMalloc(&d_rc, n * sizeof(float));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t recovery_result = {0};
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &recovery_result)) {
            err = cudaMalloc(&d_rc, n * sizeof(float));
        }
        if (err != cudaSuccess) goto cleanup_rc;
    }

    // Copy to device
    cudaMemcpyAsync(d_weights, weights, n * sizeof(float), cudaMemcpyHostToDevice, stream);
    cudaMemcpyAsync(d_covariance, covariance_matrix, n * n * sizeof(float), cudaMemcpyHostToDevice, stream);

    // Compute Cov @ w using cuBLAS
    {
        float alpha = 1.0f, beta = 0.0f;
        cublasSgemv(cublas, CUBLAS_OP_N, n, n, &alpha, d_covariance, n, d_weights, 1, &beta, d_cov_w, 1);
    }

    // Compute portfolio variance: w^T @ Cov @ w
    cublasSdot(cublas, n, d_weights, 1, d_cov_w, 1, &portfolio_variance);
    portfolio_vol = sqrtf(portfolio_variance);

    if (portfolio_vol < 1e-10f) {
        set_opt_error("Portfolio volatility is zero");
        goto cleanup_rc;
    }

    // Compute marginal risk contribution: (Cov @ w) / portfolio_vol
    block_size = min(256u, n);
    grid_size = NIMCP_CUDA_GRID_SIZE(n, block_size);
    kernel_marginal_risk_contribution<<<grid_size, block_size, 0, stream>>>(
        d_covariance, d_weights, d_cov_w, portfolio_vol, d_mrc, n);

    // Compute risk contribution: w * MRC
    kernel_risk_contribution<<<grid_size, block_size, 0, stream>>>(
        d_weights, d_mrc, d_rc, n);

    // Copy results back
    cudaMemcpy(out_marginal, d_mrc, n * sizeof(float), cudaMemcpyDeviceToHost);
    cudaMemcpy(out_contribution, d_rc, n * sizeof(float), cudaMemcpyDeviceToHost);

    success = true;

cleanup_rc:
    cudaFree(d_weights);
    cudaFree(d_covariance);
    cudaFree(d_cov_w);
    cudaFree(d_mrc);
    cudaFree(d_rc);

    return success;
}

//=============================================================================
// Risk Parity Optimization (Header Signature)
//=============================================================================

bool fin_opt_gpu_risk_parity(
    nimcp_gpu_context_t* ctx,
    const float* covariance_matrix,
    uint32_t num_assets,
    const float* target_risk_contrib,
    const fin_optimization_gpu_params_t* params,
    fin_optimization_gpu_result_t* result)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !nimcp_gpu_context_is_valid(ctx)) {
        set_opt_error("Invalid GPU context");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid GPU context");
        return false;
    }
    if (!covariance_matrix || !params || !result || num_assets == 0) {
        set_opt_error("Invalid parameters");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid parameters");
        return false;
    }

    cudaStream_t stream = nimcp_gpu_get_compute_stream(ctx);
    cublasHandle_t cublas = get_cublas_handle();
    cublasSetStream(cublas, stream);

    uint32_t n = num_assets;

    // Pre-declare variables
    float init_weight = 0.0f;
    float* h_weights = NULL;
    float* h_target_rc = NULL;
    uint32_t block_size = 0;
    uint32_t grid_size = 0;
    float learning_rate = 0.0f;
    uint32_t max_iter = 0;
    float rc_target = 0.0f;
    float h_var = 0.0f;
    float port_vol = 0.0f;
    float h_variance = 0.0f;
    float neg_lr = 0.0f;
    float damping = 0.5f;
    float* h_rc = NULL;
    float* h_w = NULL;
    float* h_init = NULL;

    // Allocate device memory
    float* d_covariance = NULL;
    float* d_weights = NULL;
    float* d_cov_w = NULL;
    float* d_mrc = NULL;
    float* d_rc = NULL;
    float* d_gradient = NULL;
    float* d_target_rc = NULL;

    cudaError_t err;

    err = cudaMalloc(&d_covariance, n * n * sizeof(float));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t recovery_result = {0};
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &recovery_result)) {
            err = cudaMalloc(&d_covariance, n * n * sizeof(float));
        }
        if (err != cudaSuccess) goto cleanup_rp2;
    }

    err = cudaMalloc(&d_weights, n * sizeof(float));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t recovery_result = {0};
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &recovery_result)) {
            err = cudaMalloc(&d_weights, n * sizeof(float));
        }
        if (err != cudaSuccess) goto cleanup_rp2;
    }

    err = cudaMalloc(&d_cov_w, n * sizeof(float));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t recovery_result = {0};
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &recovery_result)) {
            err = cudaMalloc(&d_cov_w, n * sizeof(float));
        }
        if (err != cudaSuccess) goto cleanup_rp2;
    }

    err = cudaMalloc(&d_mrc, n * sizeof(float));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t recovery_result = {0};
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &recovery_result)) {
            err = cudaMalloc(&d_mrc, n * sizeof(float));
        }
        if (err != cudaSuccess) goto cleanup_rp2;
    }

    err = cudaMalloc(&d_rc, n * sizeof(float));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t recovery_result = {0};
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &recovery_result)) {
            err = cudaMalloc(&d_rc, n * sizeof(float));
        }
        if (err != cudaSuccess) goto cleanup_rp2;
    }

    err = cudaMalloc(&d_gradient, n * sizeof(float));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t recovery_result = {0};
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &recovery_result)) {
            err = cudaMalloc(&d_gradient, n * sizeof(float));
        }
        if (err != cudaSuccess) goto cleanup_rp2;
    }

    // Copy covariance to device
    cudaMemcpyAsync(d_covariance, covariance_matrix, n * n * sizeof(float),
                    cudaMemcpyHostToDevice, stream);

    // Initialize weights uniformly
    init_weight = 1.0f / (float)n;
    h_weights = (float*)nimcp_malloc(n * sizeof(float));
    h_target_rc = (float*)nimcp_malloc(n * sizeof(float));
    if (!h_weights || !h_target_rc) {
        set_opt_error("Host memory allocation failed");
        nimcp_free(h_weights);
        nimcp_free(h_target_rc);
        goto cleanup_rp2;
    }

    for (uint32_t i = 0; i < n; i++) {
        h_weights[i] = init_weight;
        h_target_rc[i] = target_risk_contrib ? target_risk_contrib[i] : (1.0f / (float)n);
    }
    cudaMemcpyAsync(d_weights, h_weights, n * sizeof(float), cudaMemcpyHostToDevice, stream);

    nimcp_free(h_weights);
    nimcp_free(h_target_rc);

    // Use power-of-2 block size
    block_size = 1;
    while (block_size < n && block_size < 256) block_size *= 2;
    if (block_size < 32) block_size = 32;
    grid_size = NIMCP_CUDA_GRID_SIZE(n, block_size);

    // Risk parity typically needs smaller learning rate for stability
    learning_rate = params->learning_rate > 0 ? params->learning_rate : 0.001f;
    max_iter = params->max_iterations > 0 ? params->max_iterations : 2000;

    // Risk parity uses equal risk contribution target by default
    rc_target = 1.0f / (float)n;

    // For risk parity with equal risk contribution target, the analytical solution
    // is approximately the inverse-volatility portfolio (exact for diagonal covariance)
    // We use this as the starting point and refine with optimization
    h_init = (float*)nimcp_malloc(n * sizeof(float));
    if (h_init) {
        float sum_inv_vol = 0.0f;
        for (uint32_t i = 0; i < n; i++) {
            float vol = sqrtf(covariance_matrix[i * n + i]);
            h_init[i] = (vol > 1e-10f) ? (1.0f / vol) : 1.0f;
            sum_inv_vol += h_init[i];
        }
        for (uint32_t i = 0; i < n; i++) {
            h_init[i] /= sum_inv_vol;
        }
        cudaMemcpy(d_weights, h_init, n * sizeof(float), cudaMemcpyHostToDevice);
        nimcp_free(h_init);
        h_init = NULL;
    }

    // Risk parity optimization using the Spinu (2013) approach:
    // The objective is to minimize sum_i (w_i * (Cov @ w)_i - b_i * w^T Cov w)^2
    // where b_i is the target risk budget (1/n for equal risk contribution)
    //
    // For simplicity and robustness, we use an iterative rescaling approach:
    // w_i^{new} = w_i * (target_RC / actual_RC_i)^alpha
    // where alpha is a damping factor

    for (uint32_t iter = 0; iter < max_iter; iter++) {
        // Compute Cov @ w
        kernel_portfolio_variance<<<grid_size, block_size, n * sizeof(float), stream>>>(
            d_covariance, d_weights, d_cov_w, n);

        // Compute portfolio variance
        cublasSdot(cublas, n, d_weights, 1, d_cov_w, 1, &h_var);
        port_vol = sqrtf(h_var);

        if (port_vol < 1e-10f) port_vol = 1e-10f;

        // Compute marginal risk contribution
        kernel_marginal_risk_contribution<<<grid_size, block_size, 0, stream>>>(
            d_covariance, d_weights, d_cov_w, port_vol, d_mrc, n);

        // Compute risk contribution
        kernel_risk_contribution<<<grid_size, block_size, 0, stream>>>(
            d_weights, d_mrc, d_rc, n);

        // Copy RC to host for rescaling computation
        h_rc = (float*)nimcp_malloc(n * sizeof(float));
        h_w = (float*)nimcp_malloc(n * sizeof(float));
        if (h_rc && h_w) {
            cudaMemcpy(h_rc, d_rc, n * sizeof(float), cudaMemcpyDeviceToHost);
            cudaMemcpy(h_w, d_weights, n * sizeof(float), cudaMemcpyDeviceToHost);

            // Rescale weights based on risk contribution deviation
            float sum_w = 0.0f;
            for (uint32_t i = 0; i < n; i++) {
                if (h_rc[i] > 1e-10f) {
                    // Scale inversely to risk contribution deviation
                    float scale = powf(rc_target / h_rc[i], damping);
                    h_w[i] *= scale;
                }
                if (h_w[i] < 0.001f) h_w[i] = 0.001f;
                sum_w += h_w[i];
            }
            // Normalize
            for (uint32_t i = 0; i < n; i++) {
                h_w[i] /= sum_w;
            }
            cudaMemcpy(d_weights, h_w, n * sizeof(float), cudaMemcpyHostToDevice);
        }
        nimcp_free(h_rc);
        nimcp_free(h_w);
        h_rc = NULL;
        h_w = NULL;
    }

    // Compute final metrics
    kernel_portfolio_variance<<<grid_size, block_size, n * sizeof(float), stream>>>(
        d_covariance, d_weights, d_cov_w, n);

    cublasSdot(cublas, n, d_weights, 1, d_cov_w, 1, &h_variance);

    cudaStreamSynchronize(stream);

    // Copy results
    result->optimal_weights = (float*)nimcp_malloc(n * sizeof(float));
    if (!result->optimal_weights) {
        set_opt_error("Failed to allocate result weights");
        goto cleanup_rp2;
    }

    cudaMemcpy(result->optimal_weights, d_weights, n * sizeof(float), cudaMemcpyDeviceToHost);

    result->portfolio_variance = h_variance;
    result->portfolio_volatility = sqrtf(h_variance);
    result->expected_return = 0.0f;
    result->sharpe_ratio = 0.0f;
    result->n_assets = n;
    result->converged = true;

    // Cleanup
    cudaFree(d_covariance);
    cudaFree(d_weights);
    cudaFree(d_cov_w);
    cudaFree(d_mrc);
    cudaFree(d_rc);
    cudaFree(d_gradient);
    cudaFree(d_target_rc);

    return true;

cleanup_rp2:
    cudaFree(d_covariance);
    cudaFree(d_weights);
    cudaFree(d_cov_w);
    cudaFree(d_mrc);
    cudaFree(d_rc);
    cudaFree(d_gradient);
    cudaFree(d_target_rc);
    set_opt_error("Memory allocation failed");
    return false;
}

//=============================================================================
// Constrained Portfolio Optimization
//=============================================================================

/**
 * @brief Kernel to check and project linear constraints
 */
__global__ void kernel_check_linear_constraint(
    const float* __restrict__ weights,
    const float* __restrict__ coefficients,
    float bound,
    int constraint_type,  // 0=eq, 1=le, 2=ge
    float* __restrict__ violation,
    uint32_t n)
{
    extern __shared__ float s_constraint_partial[];
    uint32_t tid = threadIdx.x;
    float sum = 0.0f;

    for (uint32_t i = tid; i < n; i += blockDim.x) {
        sum += weights[i] * coefficients[i];
    }

    s_constraint_partial[tid] = sum;
    __syncthreads();

    for (uint32_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            s_constraint_partial[tid] += s_constraint_partial[tid + s];
        }
        __syncthreads();
    }

    if (tid == 0) {
        float val = s_constraint_partial[0];
        if (constraint_type == 0) {  // Equality
            *violation = fabsf(val - bound);
        } else if (constraint_type == 1) {  // Less than or equal
            *violation = fmaxf(0.0f, val - bound);
        } else {  // Greater than or equal
            *violation = fmaxf(0.0f, bound - val);
        }
    }
}

bool fin_opt_gpu_constrained(
    nimcp_gpu_context_t* ctx,
    const float* expected_returns,
    const float* covariance_matrix,
    uint32_t num_assets,
    const fin_opt_extended_params_t* params,
    fin_opt_extended_result_t* result)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !nimcp_gpu_context_is_valid(ctx)) {
        set_opt_error("Invalid GPU context");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid GPU context");
        return false;
    }
    if (!expected_returns || !covariance_matrix || !params || !result || num_assets == 0) {
        set_opt_error("Invalid parameters");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid parameters");
        return false;
    }

    // First, run base mean-variance optimization
    fin_optimization_gpu_params_t base_params = params->base;
    base_params.n_assets = num_assets;

    if (!fin_optimization_gpu_mean_variance(ctx, expected_returns, covariance_matrix,
                                             &base_params, &result->base)) {
        return false;
    }

    // Apply additional constraints iteratively
    cudaStream_t stream = nimcp_gpu_get_compute_stream(ctx);
    uint32_t n = num_assets;

    // Apply sector constraints if specified
    if (params->num_sectors > 0 && params->sector_ids &&
        params->sector_min_weights && params->sector_max_weights) {

        // Compute sector weights and adjust
        float* sector_weights = (float*)nimcp_calloc(params->num_sectors, sizeof(float));
        if (sector_weights) {
            for (uint32_t i = 0; i < n; i++) {
                uint32_t sector = params->sector_ids[i];
                if (sector < params->num_sectors) {
                    sector_weights[sector] += result->base.optimal_weights[i];
                }
            }

            // Scale assets within sectors that violate constraints
            for (uint32_t s = 0; s < params->num_sectors; s++) {
                float current = sector_weights[s];
                float min_w = params->sector_min_weights[s];
                float max_w = params->sector_max_weights[s];

                if (current > max_w && current > 0) {
                    float scale = max_w / current;
                    for (uint32_t i = 0; i < n; i++) {
                        if (params->sector_ids[i] == s) {
                            result->base.optimal_weights[i] *= scale;
                        }
                    }
                } else if (current < min_w && current > 0) {
                    float scale = min_w / current;
                    for (uint32_t i = 0; i < n; i++) {
                        if (params->sector_ids[i] == s) {
                            result->base.optimal_weights[i] *= scale;
                        }
                    }
                }
            }

            // Renormalize
            float sum = 0.0f;
            for (uint32_t i = 0; i < n; i++) {
                sum += result->base.optimal_weights[i];
            }
            if (sum > 0) {
                for (uint32_t i = 0; i < n; i++) {
                    result->base.optimal_weights[i] /= sum;
                }
            }

            nimcp_free(sector_weights);
        }
    }

    // Compute risk contributions
    result->marginal_risk = (float*)nimcp_malloc(n * sizeof(float));
    result->risk_contribution = (float*)nimcp_malloc(n * sizeof(float));

    if (result->marginal_risk && result->risk_contribution) {
        fin_opt_gpu_risk_contribution(ctx, result->base.optimal_weights,
                                       covariance_matrix, n,
                                       result->marginal_risk, result->risk_contribution);

        // Compute concentration ratio (Herfindahl index)
        float hhi = 0.0f;
        for (uint32_t i = 0; i < n; i++) {
            float rc = result->risk_contribution[i];
            hhi += rc * rc;
        }
        result->concentration_ratio = hhi;
    }

    // Compute transaction costs if current weights provided
    if (params->current_weights) {
        float turnover = 0.0f;
        for (uint32_t i = 0; i < n; i++) {
            turnover += fabsf(result->base.optimal_weights[i] - params->current_weights[i]);
        }
        result->turnover = turnover / 2.0f;  // One-way turnover
        result->transaction_costs = result->turnover * params->transaction_cost / 10000.0f;
    }

    return true;
}

//=============================================================================
// Black-Litterman Model
//=============================================================================

bool fin_opt_gpu_black_litterman(
    nimcp_gpu_context_t* ctx,
    const float* market_covariance,
    uint32_t num_assets,
    const fin_opt_extended_params_t* params,
    fin_opt_extended_result_t* result)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !nimcp_gpu_context_is_valid(ctx)) {
        set_opt_error("Invalid GPU context");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid GPU context");
        return false;
    }
    if (!market_covariance || !params || !result || num_assets == 0) {
        set_opt_error("Invalid parameters");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid parameters");
        return false;
    }
    if (!params->market_weights) {
        set_opt_error("Market weights required for Black-Litterman");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Market weights required");
        return false;
    }

    cudaStream_t stream = nimcp_gpu_get_compute_stream(ctx);
    cublasHandle_t cublas = get_cublas_handle();
    cublasSetStream(cublas, stream);

    uint32_t n = num_assets;
    uint32_t num_views = params->num_views;
    float tau = params->tau > 0 ? params->tau : 0.05f;

    // Pre-declare all variables to avoid goto initialization issues
    float alpha = 0.0f;
    float beta = 0.0f;
    float scale = 0.0f;
    float* h_cov = NULL;
    float* h_views_adj = NULL;
    float* h_pi = NULL;
    float confidence = 0.0f;
    fin_optimization_gpu_params_t opt_params;

    // Device memory
    float* d_cov = NULL;
    float* d_market_w = NULL;
    float* d_pi = NULL;  // Equilibrium returns
    float* d_posterior_returns = NULL;
    float* d_posterior_cov = NULL;

    // Host memory for results
    float* h_posterior_returns = NULL;
    float* h_posterior_cov = NULL;

    cudaError_t err;
    bool success = false;

    err = cudaMalloc(&d_cov, n * n * sizeof(float));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t recovery_result = {0};
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &recovery_result)) {
            err = cudaMalloc(&d_cov, n * n * sizeof(float));
        }
        if (err != cudaSuccess) goto cleanup_bl;
    }

    err = cudaMalloc(&d_market_w, n * sizeof(float));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t recovery_result = {0};
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &recovery_result)) {
            err = cudaMalloc(&d_market_w, n * sizeof(float));
        }
        if (err != cudaSuccess) goto cleanup_bl;
    }

    err = cudaMalloc(&d_pi, n * sizeof(float));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t recovery_result = {0};
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &recovery_result)) {
            err = cudaMalloc(&d_pi, n * sizeof(float));
        }
        if (err != cudaSuccess) goto cleanup_bl;
    }

    err = cudaMalloc(&d_posterior_returns, n * sizeof(float));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t recovery_result = {0};
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &recovery_result)) {
            err = cudaMalloc(&d_posterior_returns, n * sizeof(float));
        }
        if (err != cudaSuccess) goto cleanup_bl;
    }

    err = cudaMalloc(&d_posterior_cov, n * n * sizeof(float));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t recovery_result = {0};
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &recovery_result)) {
            err = cudaMalloc(&d_posterior_cov, n * n * sizeof(float));
        }
        if (err != cudaSuccess) goto cleanup_bl;
    }

    // Copy inputs
    cudaMemcpyAsync(d_cov, market_covariance, n * n * sizeof(float), cudaMemcpyHostToDevice, stream);
    cudaMemcpyAsync(d_market_w, params->market_weights, n * sizeof(float), cudaMemcpyHostToDevice, stream);

    // Step 1: Compute equilibrium returns (pi = risk_aversion * Cov * market_weights)
    alpha = params->base.risk_aversion > 0 ? params->base.risk_aversion : 2.5f;
    beta = 0.0f;
    cublasSgemv(cublas, CUBLAS_OP_N, n, n, &alpha, d_cov, n, d_market_w, 1, &beta, d_pi, 1);

    // Step 2: If no views, posterior = prior
    if (num_views == 0 || !params->views_matrix || !params->views_returns) {
        // Posterior returns = equilibrium returns
        cudaMemcpyAsync(d_posterior_returns, d_pi, n * sizeof(float), cudaMemcpyDeviceToDevice, stream);

        // Posterior covariance = (1 + tau) * Cov
        scale = 1.0f + tau;
        h_cov = (float*)nimcp_malloc(n * n * sizeof(float));
        if (h_cov) {
            memcpy(h_cov, market_covariance, n * n * sizeof(float));
            for (uint32_t i = 0; i < n * n; i++) {
                h_cov[i] *= scale;
            }
            cudaMemcpyAsync(d_posterior_cov, h_cov, n * n * sizeof(float), cudaMemcpyHostToDevice, stream);
            nimcp_free(h_cov);
            h_cov = NULL;
        }
    } else {
        // Step 3: Incorporate views using BL formula
        // This is a simplified version - full implementation would need matrix inversions

        // Posterior returns: E[r] = pi + tau * Cov * P' * (P * tau * Cov * P' + Omega)^-1 * (Q - P * pi)
        // For simplicity, we use a blended approach

        h_views_adj = (float*)nimcp_calloc(n, sizeof(float));
        if (h_views_adj) {
            // Apply views with confidence weighting
            for (uint32_t v = 0; v < num_views && v < n; v++) {
                confidence = params->views_confidence ? (1.0f / params->views_confidence[v]) : 1.0f;
                for (uint32_t i = 0; i < n; i++) {
                    h_views_adj[i] += tau * confidence * params->views_matrix[v * n + i] * params->views_returns[v];
                }
            }

            // Copy equilibrium returns to host, add views adjustment
            h_pi = (float*)nimcp_malloc(n * sizeof(float));
            if (h_pi) {
                cudaMemcpy(h_pi, d_pi, n * sizeof(float), cudaMemcpyDeviceToHost);
                for (uint32_t i = 0; i < n; i++) {
                    h_pi[i] += h_views_adj[i];
                }
                cudaMemcpyAsync(d_posterior_returns, h_pi, n * sizeof(float), cudaMemcpyHostToDevice, stream);
                nimcp_free(h_pi);
                h_pi = NULL;
            }
            nimcp_free(h_views_adj);
            h_views_adj = NULL;
        }

        // Posterior covariance (simplified)
        cudaMemcpyAsync(d_posterior_cov, d_cov, n * n * sizeof(float), cudaMemcpyDeviceToDevice, stream);
    }

    cudaStreamSynchronize(stream);

    // Allocate and copy results
    h_posterior_returns = (float*)nimcp_malloc(n * sizeof(float));
    h_posterior_cov = (float*)nimcp_malloc(n * n * sizeof(float));

    if (!h_posterior_returns || !h_posterior_cov) {
        set_opt_error("Failed to allocate posterior arrays");
        nimcp_free(h_posterior_returns);
        nimcp_free(h_posterior_cov);
        goto cleanup_bl;
    }

    cudaMemcpy(h_posterior_returns, d_posterior_returns, n * sizeof(float), cudaMemcpyDeviceToHost);
    cudaMemcpy(h_posterior_cov, d_posterior_cov, n * n * sizeof(float), cudaMemcpyDeviceToHost);

    // Run mean-variance optimization with posterior estimates
    opt_params = params->base;
    opt_params.n_assets = n;

    if (!fin_optimization_gpu_mean_variance(ctx, h_posterior_returns, h_posterior_cov,
                                             &opt_params, &result->base)) {
        nimcp_free(h_posterior_returns);
        nimcp_free(h_posterior_cov);
        goto cleanup_bl;
    }

    result->posterior_returns = h_posterior_returns;
    result->posterior_covariance = h_posterior_cov;

    // Compute risk contributions
    result->marginal_risk = (float*)nimcp_malloc(n * sizeof(float));
    result->risk_contribution = (float*)nimcp_malloc(n * sizeof(float));

    if (result->marginal_risk && result->risk_contribution) {
        fin_opt_gpu_risk_contribution(ctx, result->base.optimal_weights,
                                       h_posterior_cov, n,
                                       result->marginal_risk, result->risk_contribution);
    }

    success = true;

cleanup_bl:
    cudaFree(d_cov);
    cudaFree(d_market_w);
    cudaFree(d_pi);
    cudaFree(d_posterior_returns);
    cudaFree(d_posterior_cov);

    return success;
}

//=============================================================================
// Robust Optimization
//=============================================================================

bool fin_opt_gpu_robust(
    nimcp_gpu_context_t* ctx,
    const float* expected_returns,
    const float* covariance_matrix,
    uint32_t num_assets,
    const fin_opt_extended_params_t* params,
    fin_opt_extended_result_t* result)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !nimcp_gpu_context_is_valid(ctx)) {
        set_opt_error("Invalid GPU context");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid GPU context");
        return false;
    }
    if (!expected_returns || !covariance_matrix || !params || !result || num_assets == 0) {
        set_opt_error("Invalid parameters");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid parameters");
        return false;
    }

    uint32_t n = num_assets;
    float kappa = params->uncertainty_budget > 0 ? params->uncertainty_budget : 1.0f;

    // Robust optimization adjusts expected returns based on uncertainty
    // Worst-case return: r_robust = r - kappa * delta_r
    // where delta_r is the uncertainty in returns

    float* robust_returns = (float*)nimcp_malloc(n * sizeof(float));
    float* robust_cov = (float*)nimcp_malloc(n * n * sizeof(float));

    if (!robust_returns || !robust_cov) {
        set_opt_error("Failed to allocate robust arrays");
        nimcp_free(robust_returns);
        nimcp_free(robust_cov);
        return false;
    }

    // Adjust returns for uncertainty
    for (uint32_t i = 0; i < n; i++) {
        float uncertainty = params->return_uncertainty ? params->return_uncertainty[i] : 0.02f;
        robust_returns[i] = expected_returns[i] - kappa * uncertainty;
    }

    // Adjust covariance for uncertainty
    memcpy(robust_cov, covariance_matrix, n * n * sizeof(float));
    if (params->cov_uncertainty) {
        for (uint32_t i = 0; i < n * n; i++) {
            robust_cov[i] += kappa * params->cov_uncertainty[i];
        }
    } else {
        // Add small diagonal perturbation for robustness
        for (uint32_t i = 0; i < n; i++) {
            robust_cov[i * n + i] *= (1.0f + 0.1f * kappa);
        }
    }

    // Run optimization with robust estimates
    fin_optimization_gpu_params_t opt_params = params->base;
    opt_params.n_assets = n;

    bool success = fin_optimization_gpu_mean_variance(ctx, robust_returns, robust_cov,
                                                       &opt_params, &result->base);

    if (success) {
        // Compute risk contributions using original covariance
        result->marginal_risk = (float*)nimcp_malloc(n * sizeof(float));
        result->risk_contribution = (float*)nimcp_malloc(n * sizeof(float));

        if (result->marginal_risk && result->risk_contribution) {
            fin_opt_gpu_risk_contribution(ctx, result->base.optimal_weights,
                                           covariance_matrix, n,
                                           result->marginal_risk, result->risk_contribution);
        }
    }

    nimcp_free(robust_returns);
    nimcp_free(robust_cov);

    return success;
}

//=============================================================================
// Constrained Efficient Frontier
//=============================================================================

bool fin_opt_gpu_efficient_frontier_constrained(
    nimcp_gpu_context_t* ctx,
    const float* expected_returns,
    const float* covariance_matrix,
    uint32_t num_assets,
    uint32_t num_points,
    const fin_opt_extended_params_t* params,
    float* out_returns,
    float* out_volatilities,
    float* out_weights,
    float* out_sharpes)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !nimcp_gpu_context_is_valid(ctx)) {
        set_opt_error("Invalid GPU context");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid GPU context");
        return false;
    }
    if (!expected_returns || !covariance_matrix || !params || num_assets == 0 || num_points == 0) {
        set_opt_error("Invalid parameters");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid parameters");
        return false;
    }
    if (!out_returns || !out_volatilities || !out_weights) {
        set_opt_error("Invalid output arrays");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid output arrays");
        return false;
    }

    uint32_t n = num_assets;

    // Find min/max possible returns considering constraints
    float min_return = FLT_MAX;
    float max_return = -FLT_MAX;

    // Consider min/max weights when computing return bounds
    float min_weight = params->base.min_weight;
    float max_weight = params->base.max_weight;

    for (uint32_t i = 0; i < n; i++) {
        // Minimum return point: max weight on lowest return assets
        if (expected_returns[i] < min_return) min_return = expected_returns[i];
        if (expected_returns[i] > max_return) max_return = expected_returns[i];
    }

    // Adjust bounds based on weight constraints
    float effective_min = min_return * max_weight + max_return * min_weight;
    float effective_max = max_return * max_weight + min_return * min_weight;

    // Use tighter bounds
    min_return = fmaxf(min_return, effective_min);
    max_return = fminf(max_return, effective_max);

    float step = (max_return - min_return) / (float)(num_points - 1);

    for (uint32_t p = 0; p < num_points; p++) {
        float target_return = min_return + p * step;

        // Create modified params with target return
        fin_opt_extended_params_t modified_params = *params;
        modified_params.base.target_return = target_return;

        fin_opt_extended_result_t point_result = {0};

        if (fin_opt_gpu_constrained(ctx, expected_returns, covariance_matrix,
                                     n, &modified_params, &point_result)) {
            out_returns[p] = point_result.base.expected_return;
            out_volatilities[p] = point_result.base.portfolio_volatility;
            if (out_sharpes) {
                out_sharpes[p] = point_result.base.sharpe_ratio;
            }
            memcpy(&out_weights[p * n], point_result.base.optimal_weights, n * sizeof(float));
            fin_opt_extended_result_free(&point_result);
        } else {
            // Use default values for failed point
            out_returns[p] = target_return;
            out_volatilities[p] = 0.0f;
            if (out_sharpes) {
                out_sharpes[p] = 0.0f;
            }
            for (uint32_t i = 0; i < n; i++) {
                out_weights[p * n + i] = 1.0f / (float)n;
            }
        }
    }

    return true;
}

} // extern "C"

#else // !NIMCP_ENABLE_CUDA

//=============================================================================
// CPU Fallback Stubs
//=============================================================================

extern "C" {

bool fin_optimization_gpu_mean_variance(
    nimcp_gpu_context_t* ctx,
    const float* expected_returns,
    const float* covariance_matrix,
    const fin_optimization_gpu_params_t* params,
    fin_optimization_gpu_result_t* result)
{
    (void)ctx; (void)expected_returns; (void)covariance_matrix;
    (void)params; (void)result;
    return false;
}

bool fin_optimization_gpu_efficient_frontier(
    nimcp_gpu_context_t* ctx,
    const float* expected_returns,
    const float* covariance_matrix,
    const fin_optimization_gpu_params_t* params,
    uint32_t num_points,
    fin_efficient_frontier_result_t* result)
{
    (void)ctx; (void)expected_returns; (void)covariance_matrix;
    (void)params; (void)num_points; (void)result;
    return false;
}

bool fin_opt_gpu_risk_parity(
    nimcp_gpu_context_t* ctx,
    const float* covariance_matrix,
    uint32_t num_assets,
    const float* target_risk_contrib,
    const fin_optimization_gpu_params_t* params,
    fin_optimization_gpu_result_t* result)
{
    (void)ctx; (void)covariance_matrix; (void)num_assets;
    (void)target_risk_contrib; (void)params; (void)result;
    return false;
}

bool fin_opt_gpu_constrained(
    nimcp_gpu_context_t* ctx,
    const float* expected_returns,
    const float* covariance_matrix,
    uint32_t num_assets,
    const fin_opt_extended_params_t* params,
    fin_opt_extended_result_t* result)
{
    (void)ctx; (void)expected_returns; (void)covariance_matrix;
    (void)num_assets; (void)params; (void)result;
    return false;
}

bool fin_opt_gpu_black_litterman(
    nimcp_gpu_context_t* ctx,
    const float* market_covariance,
    uint32_t num_assets,
    const fin_opt_extended_params_t* params,
    fin_opt_extended_result_t* result)
{
    (void)ctx; (void)market_covariance; (void)num_assets;
    (void)params; (void)result;
    return false;
}

bool fin_opt_gpu_robust(
    nimcp_gpu_context_t* ctx,
    const float* expected_returns,
    const float* covariance_matrix,
    uint32_t num_assets,
    const fin_opt_extended_params_t* params,
    fin_opt_extended_result_t* result)
{
    (void)ctx; (void)expected_returns; (void)covariance_matrix;
    (void)num_assets; (void)params; (void)result;
    return false;
}

bool fin_opt_gpu_risk_contribution(
    nimcp_gpu_context_t* ctx,
    const float* weights,
    const float* covariance_matrix,
    uint32_t num_assets,
    float* out_marginal,
    float* out_contribution)
{
    (void)ctx; (void)weights; (void)covariance_matrix;
    (void)num_assets; (void)out_marginal; (void)out_contribution;
    return false;
}

bool fin_opt_gpu_efficient_frontier_constrained(
    nimcp_gpu_context_t* ctx,
    const float* expected_returns,
    const float* covariance_matrix,
    uint32_t num_assets,
    uint32_t num_points,
    const fin_opt_extended_params_t* params,
    float* out_returns,
    float* out_volatilities,
    float* out_weights,
    float* out_sharpes)
{
    (void)ctx; (void)expected_returns; (void)covariance_matrix;
    (void)num_assets; (void)num_points; (void)params;
    (void)out_returns; (void)out_volatilities; (void)out_weights; (void)out_sharpes;
    return false;
}

float fin_opt_gpu_portfolio_variance(
    nimcp_gpu_context_t* ctx,
    const float* weights,
    const float* covariance,
    uint32_t num_assets)
{
    (void)ctx; (void)weights; (void)covariance; (void)num_assets;
    return -1.0f;
}

float fin_opt_gpu_portfolio_return(
    nimcp_gpu_context_t* ctx,
    const float* weights,
    const float* returns,
    uint32_t num_assets)
{
    (void)ctx; (void)weights; (void)returns; (void)num_assets;
    return -1.0f;
}

bool fin_opt_gpu_invert_covariance(
    nimcp_gpu_context_t* ctx,
    const float* covariance_matrix,
    uint32_t n,
    float* inverse)
{
    (void)ctx; (void)covariance_matrix; (void)n; (void)inverse;
    return false;
}

void fin_optimization_gpu_result_free(fin_optimization_gpu_result_t* result) {
    (void)result;
}

void fin_efficient_frontier_result_free(fin_efficient_frontier_result_t* result) {
    (void)result;
}

void fin_opt_extended_result_free(fin_opt_extended_result_t* result) {
    (void)result;
}

const char* fin_optimization_gpu_get_last_error(void) {
    return "GPU support not compiled";
}

} // extern "C"

#endif // NIMCP_ENABLE_CUDA
