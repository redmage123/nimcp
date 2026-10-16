/**
 * @file nimcp_financial_monte_carlo_gpu.cu
 * @brief GPU Monte Carlo Simulation for Financial Modeling
 *
 * WHAT: Parallel Monte Carlo path simulation on GPU
 * WHY:  100K+ paths in parallel for option pricing, risk metrics
 * HOW:  cuRAND for RNG, kernel parallelism over paths
 *
 * Implements:
 *   - Geometric Brownian Motion (GBM)
 *   - Heston stochastic volatility
 *   - Jump-diffusion models
 *   - Variance reduction (antithetic, control variates)
 *   - Path-dependent options (Asian, Lookback, Barrier)
 *   - Multi-asset correlated simulation
 *
 * RNG ARCHITECTURE:
 * =================
 * Uses local cuRAND state management for path simulation reproducibility.
 * For general statistics RNG, see: gpu/statistics/nimcp_statistics_gpu.h
 * For device-level RNG helpers, see: gpu/common/nimcp_device_utils.cuh
 *
 * This module also uses the central statistics reduction kernels for
 * mean/variance computation of path payoffs.
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#ifdef NIMCP_ENABLE_CUDA

#include "utils/memory/nimcp_memory.h"
#include <cuda_runtime.h>
#include <curand.h>
#include <curand_kernel.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <float.h>

#include "gpu/financial/nimcp_financial_gpu.h"
#include "gpu/financial/nimcp_financial_monte_carlo_gpu.h"
#include "gpu/common/nimcp_cuda_utils.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "gpu/statistics/nimcp_statistics_gpu.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/exception/nimcp_exception_macros.h"

// Comparison function for qsort (ascending order)
static int compare_float_asc(const void* a, const void* b) {
    float fa = *(const float*)a;
    float fb = *(const float*)b;
    return (fa > fb) - (fa < fb);
}

// Forward declaration of central statistics kernel for mean/variance reduction
// This kernel is defined in nimcp_statistics_kernels.cu and computes
// partial sums and partial sum-of-squares in a single pass
extern __global__ void kernel_reduce_sum_sq(
    const float* __restrict__ input,
    float* __restrict__ partial_sums,
    float* __restrict__ partial_sq_sums,
    uint32_t n);

//=============================================================================
// RNG Initialization Kernel (local static)
//=============================================================================

static __global__ void kernel_init_rng_states(
    curandState* states,
    uint64_t seed,
    uint32_t num_states)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_states) return;
    curand_init(seed, idx, 0, &states[idx]);
}

//=============================================================================
// Thread-Local Error Storage
//=============================================================================

static __thread char g_mc_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_mc_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_mc_error, sizeof(g_mc_error), fmt, args);
    va_end(args);
}

//=============================================================================
// Device Helper Functions
//=============================================================================

/**
 * @brief Standard normal CDF approximation (Abramowitz and Stegun)
 */
__device__ __forceinline__ float norm_cdf_device(float x) {
    const float a1 =  0.254829592f;
    const float a2 = -0.284496736f;
    const float a3 =  1.421413741f;
    const float a4 = -1.453152027f;
    const float a5 =  1.061405429f;
    const float p  =  0.3275911f;

    float sign = 1.0f;
    if (x < 0.0f) {
        sign = -1.0f;
        x = -x;
    }

    float t = 1.0f / (1.0f + p * x);
    float t2 = t * t;
    float t3 = t2 * t;
    float t4 = t3 * t;
    float t5 = t4 * t;
    float y = 1.0f - (a1*t + a2*t2 + a3*t3 + a4*t4 + a5*t5) * expf(-x*x/2.0f);

    return 0.5f * (1.0f + sign * y);
}

/**
 * @brief Black-Scholes price for control variate
 */
__device__ float black_scholes_device(
    float S, float K, float r, float sigma, float T, bool is_call)
{
    if (T <= 0.0f) {
        return is_call ? fmaxf(S - K, 0.0f) : fmaxf(K - S, 0.0f);
    }

    float sqrt_T = sqrtf(T);
    float d1 = (logf(S / K) + (r + 0.5f * sigma * sigma) * T) / (sigma * sqrt_T);
    float d2 = d1 - sigma * sqrt_T;

    if (is_call) {
        return S * norm_cdf_device(d1) - K * expf(-r * T) * norm_cdf_device(d2);
    } else {
        return K * expf(-r * T) * norm_cdf_device(-d2) - S * norm_cdf_device(-d1);
    }
}

//=============================================================================
// GBM Simulation Kernels
//=============================================================================

/**
 * @brief Simple GBM terminal value simulation
 */
__global__ void kernel_gbm_simulate(
    curandState* __restrict__ rng_states,
    float* __restrict__ terminal_values,
    float initial_value,
    float drift,
    float volatility,
    float dt,
    uint32_t num_steps,
    uint32_t num_paths)
{
    uint32_t path_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (path_idx >= num_paths) return;

    // Get RNG state
    curandState local_state = rng_states[path_idx % (gridDim.x * blockDim.x)];

    float S = initial_value;
    float sqrt_dt = sqrtf(dt);
    float drift_term = (drift - 0.5f * volatility * volatility) * dt;

    // Simulate path
    for (uint32_t step = 0; step < num_steps; step++) {
        float Z = curand_normal(&local_state);
        S *= expf(drift_term + volatility * sqrt_dt * Z);
    }

    terminal_values[path_idx] = S;

    // Save RNG state back
    rng_states[path_idx % (gridDim.x * blockDim.x)] = local_state;
}

/**
 * @brief GBM with antithetic variates
 */
__global__ void kernel_gbm_simulate_antithetic(
    curandState* __restrict__ rng_states,
    float* __restrict__ terminal_values,
    float initial_value,
    float drift,
    float volatility,
    float dt,
    uint32_t num_steps,
    uint32_t num_paths)
{
    // Each thread generates 2 paths (original and antithetic)
    uint32_t path_idx = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t half_paths = num_paths / 2;
    if (path_idx >= half_paths) return;

    curandState local_state = rng_states[path_idx];

    float S1 = initial_value;
    float S2 = initial_value;  // Antithetic
    float sqrt_dt = sqrtf(dt);
    float drift_term = (drift - 0.5f * volatility * volatility) * dt;

    for (uint32_t step = 0; step < num_steps; step++) {
        float Z = curand_normal(&local_state);
        S1 *= expf(drift_term + volatility * sqrt_dt * Z);
        S2 *= expf(drift_term + volatility * sqrt_dt * (-Z));  // Antithetic
    }

    terminal_values[path_idx] = S1;
    terminal_values[path_idx + half_paths] = S2;

    rng_states[path_idx] = local_state;
}

/**
 * @brief GBM with full path storage (for path-dependent options)
 */
__global__ void kernel_gbm_simulate_paths(
    curandState* __restrict__ rng_states,
    float* __restrict__ paths,      // [num_paths x (num_steps + 1)]
    float initial_value,
    float drift,
    float volatility,
    float dt,
    uint32_t num_steps,
    uint32_t num_paths)
{
    uint32_t path_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (path_idx >= num_paths) return;

    curandState local_state = rng_states[path_idx % (gridDim.x * blockDim.x)];

    float* path = &paths[path_idx * (num_steps + 1)];
    float S = initial_value;
    path[0] = S;

    float sqrt_dt = sqrtf(dt);
    float drift_term = (drift - 0.5f * volatility * volatility) * dt;

    for (uint32_t step = 0; step < num_steps; step++) {
        float Z = curand_normal(&local_state);
        S *= expf(drift_term + volatility * sqrt_dt * Z);
        path[step + 1] = S;
    }

    rng_states[path_idx % (gridDim.x * blockDim.x)] = local_state;
}

//=============================================================================
// Heston Model Kernels
//=============================================================================

/**
 * @brief Heston stochastic volatility simulation
 *
 * dS = r*S*dt + sqrt(V)*S*dW_S
 * dV = kappa*(theta - V)*dt + xi*sqrt(V)*dW_V
 * corr(dW_S, dW_V) = rho
 */
__global__ void kernel_heston_simulate(
    curandState* __restrict__ rng_states,
    float* __restrict__ terminal_values,
    float* __restrict__ terminal_vols,      // Optional: final variance
    float initial_value,
    float initial_variance,
    float risk_free_rate,
    float kappa,        // Mean reversion speed
    float theta,        // Long-run variance
    float xi,           // Vol of vol
    float rho,          // Correlation
    float dt,
    uint32_t num_steps,
    uint32_t num_paths)
{
    uint32_t path_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (path_idx >= num_paths) return;

    curandState local_state = rng_states[path_idx % (gridDim.x * blockDim.x)];

    float S = initial_value;
    float V = initial_variance;
    float sqrt_dt = sqrtf(dt);
    float sqrt_1_rho2 = sqrtf(1.0f - rho * rho);

    for (uint32_t step = 0; step < num_steps; step++) {
        // Generate correlated Brownians
        float Z1 = curand_normal(&local_state);
        float Z2 = curand_normal(&local_state);
        float W_S = Z1;
        float W_V = rho * Z1 + sqrt_1_rho2 * Z2;

        // Ensure V stays positive (truncation scheme)
        float sqrt_V = sqrtf(fmaxf(V, 0.0f));

        // Update variance (Euler)
        V += kappa * (theta - V) * dt + xi * sqrt_V * sqrt_dt * W_V;
        V = fmaxf(V, 0.0f);  // Truncate

        // Update price
        S *= expf((risk_free_rate - 0.5f * fmaxf(V, 0.0f)) * dt + sqrt_V * sqrt_dt * W_S);
    }

    terminal_values[path_idx] = S;
    if (terminal_vols) {
        terminal_vols[path_idx] = V;
    }

    rng_states[path_idx % (gridDim.x * blockDim.x)] = local_state;
}

//=============================================================================
// Jump-Diffusion Kernels (Merton Model)
//=============================================================================

/**
 * @brief Merton jump-diffusion simulation
 *
 * dS/S = (r - lambda*k)*dt + sigma*dW + (J-1)*dN
 * where N is Poisson process with intensity lambda
 * J is lognormal jump size
 */
__global__ void kernel_jump_diffusion_simulate(
    curandState* __restrict__ rng_states,
    float* __restrict__ terminal_values,
    float initial_value,
    float risk_free_rate,
    float volatility,
    float jump_intensity,   // lambda: expected jumps per year
    float jump_mean,        // Mean of log-jump size
    float jump_std,         // Std of log-jump size
    float dt,
    uint32_t num_steps,
    uint32_t num_paths)
{
    uint32_t path_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (path_idx >= num_paths) return;

    curandState local_state = rng_states[path_idx % (gridDim.x * blockDim.x)];

    float S = initial_value;
    float sqrt_dt = sqrtf(dt);

    // Expected jump compensation: k = E[J-1] = exp(mu + 0.5*sigma^2) - 1
    float k = expf(jump_mean + 0.5f * jump_std * jump_std) - 1.0f;
    float drift_term = (risk_free_rate - jump_intensity * k - 0.5f * volatility * volatility) * dt;

    for (uint32_t step = 0; step < num_steps; step++) {
        // Diffusion
        float Z = curand_normal(&local_state);
        S *= expf(drift_term + volatility * sqrt_dt * Z);

        // Poisson jump process
        float U = curand_uniform(&local_state);
        float lambda_dt = jump_intensity * dt;

        // Simple Poisson approximation for small dt: P(jump) ≈ lambda*dt
        if (U < lambda_dt) {
            // Jump occurred - draw lognormal jump size
            float Z_jump = curand_normal(&local_state);
            float J = expf(jump_mean + jump_std * Z_jump);
            S *= J;
        }
    }

    terminal_values[path_idx] = S;
    rng_states[path_idx % (gridDim.x * blockDim.x)] = local_state;
}

//=============================================================================
// Path-Dependent Option Kernels
//=============================================================================

/**
 * @brief Asian option pricing (arithmetic average)
 */
__global__ void kernel_asian_option_price(
    curandState* __restrict__ rng_states,
    float* __restrict__ payoffs,
    float initial_value,
    float strike,
    float drift,
    float volatility,
    float dt,
    uint32_t num_steps,
    uint32_t num_paths,
    bool is_call)
{
    uint32_t path_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (path_idx >= num_paths) return;

    curandState local_state = rng_states[path_idx % (gridDim.x * blockDim.x)];

    float S = initial_value;
    float sum_S = initial_value;
    float sqrt_dt = sqrtf(dt);
    float drift_term = (drift - 0.5f * volatility * volatility) * dt;

    for (uint32_t step = 0; step < num_steps; step++) {
        float Z = curand_normal(&local_state);
        S *= expf(drift_term + volatility * sqrt_dt * Z);
        sum_S += S;
    }

    // Arithmetic average
    float avg_S = sum_S / (float)(num_steps + 1);

    // Payoff
    if (is_call) {
        payoffs[path_idx] = fmaxf(avg_S - strike, 0.0f);
    } else {
        payoffs[path_idx] = fmaxf(strike - avg_S, 0.0f);
    }

    rng_states[path_idx % (gridDim.x * blockDim.x)] = local_state;
}

/**
 * @brief Lookback option pricing (floating strike)
 */
__global__ void kernel_lookback_option_price(
    curandState* __restrict__ rng_states,
    float* __restrict__ payoffs,
    float initial_value,
    float drift,
    float volatility,
    float dt,
    uint32_t num_steps,
    uint32_t num_paths,
    bool is_call)
{
    uint32_t path_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (path_idx >= num_paths) return;

    curandState local_state = rng_states[path_idx % (gridDim.x * blockDim.x)];

    float S = initial_value;
    float S_min = initial_value;
    float S_max = initial_value;
    float sqrt_dt = sqrtf(dt);
    float drift_term = (drift - 0.5f * volatility * volatility) * dt;

    for (uint32_t step = 0; step < num_steps; step++) {
        float Z = curand_normal(&local_state);
        S *= expf(drift_term + volatility * sqrt_dt * Z);
        S_min = fminf(S_min, S);
        S_max = fmaxf(S_max, S);
    }

    // Floating strike lookback payoff
    if (is_call) {
        payoffs[path_idx] = S - S_min;  // Call: S_T - min(S)
    } else {
        payoffs[path_idx] = S_max - S;  // Put: max(S) - S_T
    }

    rng_states[path_idx % (gridDim.x * blockDim.x)] = local_state;
}

/**
 * @brief Barrier option pricing (knock-out)
 */
__global__ void kernel_barrier_option_price(
    curandState* __restrict__ rng_states,
    float* __restrict__ payoffs,
    float initial_value,
    float strike,
    float barrier,
    float drift,
    float volatility,
    float dt,
    uint32_t num_steps,
    uint32_t num_paths,
    bool is_call,
    bool is_up,         // true = up-and-out, false = down-and-out
    float rebate)       // Rebate if knocked out
{
    uint32_t path_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (path_idx >= num_paths) return;

    curandState local_state = rng_states[path_idx % (gridDim.x * blockDim.x)];

    float S = initial_value;
    float sqrt_dt = sqrtf(dt);
    float drift_term = (drift - 0.5f * volatility * volatility) * dt;
    bool knocked_out = false;

    for (uint32_t step = 0; step < num_steps && !knocked_out; step++) {
        float Z = curand_normal(&local_state);
        S *= expf(drift_term + volatility * sqrt_dt * Z);

        if (is_up && S >= barrier) {
            knocked_out = true;
        } else if (!is_up && S <= barrier) {
            knocked_out = true;
        }
    }

    if (knocked_out) {
        payoffs[path_idx] = rebate;
    } else {
        if (is_call) {
            payoffs[path_idx] = fmaxf(S - strike, 0.0f);
        } else {
            payoffs[path_idx] = fmaxf(strike - S, 0.0f);
        }
    }

    rng_states[path_idx % (gridDim.x * blockDim.x)] = local_state;
}

//=============================================================================
// Multi-Asset Correlated Simulation
//=============================================================================

/**
 * @brief Multi-asset correlated GBM simulation
 *
 * @param cholesky_L  Lower triangular Cholesky factor [n_assets x n_assets]
 */
__global__ void kernel_multi_asset_gbm_simulate(
    curandState* __restrict__ rng_states,
    float* __restrict__ terminal_values,    // [num_paths x n_assets]
    const float* __restrict__ initial_values,  // [n_assets]
    const float* __restrict__ drifts,          // [n_assets]
    const float* __restrict__ volatilities,    // [n_assets]
    const float* __restrict__ cholesky_L,      // [n_assets x n_assets]
    float dt,
    uint32_t num_steps,
    uint32_t num_paths,
    uint32_t n_assets)
{
    extern __shared__ float s_data[];
    float* s_Z = s_data;                              // [n_assets]
    float* s_corr_Z = &s_data[n_assets];              // [n_assets]

    uint32_t path_idx = blockIdx.x;
    if (path_idx >= num_paths) return;

    // Each block handles one path, thread 0 does RNG to avoid state conflicts
    curandState local_state;
    if (threadIdx.x == 0) {
        local_state = rng_states[path_idx];
    }

    // Initialize prices in registers (for small n_assets) or use shared memory
    float sqrt_dt = sqrtf(dt);

    // Load initial values
    float* out = &terminal_values[path_idx * n_assets];
    for (uint32_t i = threadIdx.x; i < n_assets; i += blockDim.x) {
        out[i] = initial_values[i];
    }
    __syncthreads();

    // Simulate path
    for (uint32_t step = 0; step < num_steps; step++) {
        // Generate independent normals (thread 0 only to avoid RNG state conflicts)
        if (threadIdx.x == 0) {
            for (uint32_t i = 0; i < n_assets; i++) {
                s_Z[i] = curand_normal(&local_state);
            }
        }
        __syncthreads();

        // Apply Cholesky: corr_Z = L @ Z
        for (uint32_t i = threadIdx.x; i < n_assets; i += blockDim.x) {
            float sum = 0.0f;
            for (uint32_t j = 0; j <= i; j++) {
                sum += cholesky_L[i * n_assets + j] * s_Z[j];
            }
            s_corr_Z[i] = sum;
        }
        __syncthreads();

        // Update prices
        for (uint32_t i = threadIdx.x; i < n_assets; i += blockDim.x) {
            float drift_term = (drifts[i] - 0.5f * volatilities[i] * volatilities[i]) * dt;
            out[i] *= expf(drift_term + volatilities[i] * sqrt_dt * s_corr_Z[i]);
        }
        __syncthreads();
    }

    // Save RNG state (thread 0 only)
    if (threadIdx.x == 0) {
        rng_states[path_idx] = local_state;
    }
}

//=============================================================================
// Statistical Reduction Kernels
//=============================================================================
//
// NOTE: Mean/variance reduction has been migrated to the central GPU statistics
// module. The kernel_reduce_sum_sq() function from nimcp_statistics_kernels.cu
// provides identical functionality with numerically equivalent results.
//
// This migration reduces code duplication and ensures consistent statistical
// implementations across the codebase. The kernel computes partial sums and
// partial sum-of-squares in a single pass using parallel reduction.
//
// See: src/gpu/statistics/nimcp_statistics_kernels.cu:kernel_reduce_sum_sq()
//=============================================================================

//=============================================================================
// European Option Payoff Kernel
//=============================================================================

/**
 * @brief Compute European option payoff from terminal values
 */
__global__ void kernel_european_payoff(
    const float* __restrict__ terminal_values,
    float* __restrict__ payoffs,
    float strike,
    uint32_t num_paths,
    bool is_call)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_paths) return;

    float S = terminal_values[idx];
    if (is_call) {
        payoffs[idx] = fmaxf(S - strike, 0.0f);
    } else {
        payoffs[idx] = fmaxf(strike - S, 0.0f);
    }
}

//=============================================================================
// Cholesky Decomposition Kernel
//=============================================================================

/**
 * @brief GPU Cholesky decomposition (column-major)
 *
 * Computes L such that A = L * L^T for a positive-definite matrix A.
 * Uses a simple blocked algorithm suitable for small matrices (< 64x64).
 */
__global__ void kernel_cholesky_decomp(
    float* __restrict__ L,
    const float* __restrict__ A,
    uint32_t n)
{
    // Simple sequential Cholesky for small n (run with 1 thread)
    // For large matrices, use cuSOLVER instead
    if (threadIdx.x != 0 || blockIdx.x != 0) return;

    for (uint32_t j = 0; j < n; j++) {
        // Compute diagonal element
        float sum = A[j * n + j];
        for (uint32_t k = 0; k < j; k++) {
            sum -= L[j * n + k] * L[j * n + k];
        }
        if (sum <= 0.0f) {
            // Matrix not positive definite - add small regularization
            sum = 1e-6f;
        }
        L[j * n + j] = sqrtf(sum);

        // Compute off-diagonal elements in column j
        for (uint32_t i = j + 1; i < n; i++) {
            sum = A[i * n + j];
            for (uint32_t k = 0; k < j; k++) {
                sum -= L[i * n + k] * L[j * n + k];
            }
            L[i * n + j] = sum / L[j * n + j];
        }

        // Set upper triangle to zero
        for (uint32_t i = 0; i < j; i++) {
            L[i * n + j] = 0.0f;
        }
    }
}

//=============================================================================
// VaR/CVaR Computation Kernels (local to this file)
//=============================================================================
// Note: Sorting for VaR is now done on CPU using qsort for reliability.
// The GPU bitonic sort kernel was removed to avoid duplicate symbol errors.

/**
 * @brief Compute returns from terminal values
 */
static __global__ void kernel_compute_returns(
    const float* __restrict__ terminal_values,
    float* __restrict__ returns,
    float initial_value,
    uint32_t n)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    returns[idx] = (terminal_values[idx] - initial_value) / initial_value;
}

/**
 * @brief Compute CVaR from sorted returns
 * @note Static to avoid conflict with same kernel in nimcp_financial_risk_gpu.cu
 */
static __global__ void kernel_mc_compute_cvar(
    const float* __restrict__ sorted_returns,
    float* __restrict__ partial_sums,
    uint32_t tail_size,
    uint32_t n)
{
    extern __shared__ float s_data[];

    uint32_t tid = threadIdx.x;
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;

    // Load tail values (worst returns are at the beginning after ascending sort)
    s_data[tid] = (idx < tail_size) ? sorted_returns[idx] : 0.0f;
    __syncthreads();

    // Parallel reduction for sum
    for (uint32_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            s_data[tid] += s_data[tid + s];
        }
        __syncthreads();
    }

    if (tid == 0) {
        partial_sums[blockIdx.x] = s_data[0];
    }
}

//=============================================================================
// Portfolio Simulation Kernels
//=============================================================================

/**
 * @brief Portfolio simulation with correlated returns
 */
__global__ void kernel_portfolio_simulate(
    curandState* __restrict__ rng_states,
    const float* __restrict__ weights,
    const float* __restrict__ expected_returns,
    const float* __restrict__ cholesky_L,
    float* __restrict__ portfolio_values,
    uint32_t num_assets,
    float dt,
    uint32_t num_steps,
    uint32_t num_paths)
{
    extern __shared__ float s_data[];
    float* s_Z = s_data;                           // [num_assets]
    float* s_corr_Z = &s_data[num_assets];         // [num_assets]
    float* s_weights = &s_data[2 * num_assets];    // [num_assets]

    uint32_t path_idx = blockIdx.x;
    if (path_idx >= num_paths) return;

    // Load weights into shared memory
    for (uint32_t i = threadIdx.x; i < num_assets; i += blockDim.x) {
        s_weights[i] = weights[i];
    }
    __syncthreads();

    curandState local_state = rng_states[path_idx % (gridDim.x * blockDim.x)];

    // Start with portfolio value = 1.0
    float portfolio_value = 1.0f;
    float sqrt_dt = sqrtf(dt);

    for (uint32_t step = 0; step < num_steps; step++) {
        // Generate independent normals
        for (uint32_t i = threadIdx.x; i < num_assets; i += blockDim.x) {
            s_Z[i] = curand_normal(&local_state);
        }
        __syncthreads();

        // Apply Cholesky: corr_Z = L @ Z
        for (uint32_t i = threadIdx.x; i < num_assets; i += blockDim.x) {
            float sum = 0.0f;
            for (uint32_t j = 0; j <= i; j++) {
                sum += cholesky_L[i * num_assets + j] * s_Z[j];
            }
            s_corr_Z[i] = sum;
        }
        __syncthreads();

        // Compute weighted portfolio return for this step
        float portfolio_return = 0.0f;
        for (uint32_t i = threadIdx.x; i < num_assets; i += blockDim.x) {
            float asset_return = expected_returns[i] * dt + s_corr_Z[i] * sqrt_dt;
            portfolio_return += s_weights[i] * asset_return;
        }

        // Reduce portfolio return across threads (for correctness with num_assets > blockDim)
        s_Z[threadIdx.x] = portfolio_return;
        __syncthreads();

        if (threadIdx.x == 0) {
            float total_return = 0.0f;
            for (uint32_t i = 0; i < min(blockDim.x, num_assets); i++) {
                total_return += s_Z[i];
            }
            portfolio_value *= expf(total_return);
        }
        __syncthreads();
    }

    // Store final portfolio value (only thread 0)
    if (threadIdx.x == 0) {
        portfolio_values[path_idx] = portfolio_value;
        rng_states[path_idx] = local_state;
    }
}

//=============================================================================
// Host API Implementation
//=============================================================================

extern "C" {

bool fin_monte_carlo_gpu_simulate(
    nimcp_gpu_context_t* ctx,
    fin_gpu_rng_t* rng,
    const fin_monte_carlo_gpu_params_t* params,
    fin_monte_carlo_gpu_result_t* result)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !nimcp_gpu_context_is_valid(ctx)) {
        set_mc_error("Invalid GPU context");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid GPU context");
        return false;
    }
    if (!rng) {
        set_mc_error("Invalid RNG");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid RNG");
        return false;
    }
    if (!params || !result) {
        set_mc_error("Invalid parameters or result");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid parameters or result");
        return false;
    }
    if (params->num_paths == 0 || params->num_steps == 0) {
        set_mc_error("Zero paths or steps");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Zero paths or steps");
        return false;
    }

    cudaStream_t stream = nimcp_gpu_get_compute_stream(ctx);
    uint32_t block_size = 256;
    uint32_t grid_size = NIMCP_CUDA_GRID_SIZE(params->num_paths, block_size);

    // Allocate device memory for terminal values with recovery
    float* d_terminal_values = NULL;
    cudaError_t err = cudaMalloc(&d_terminal_values, params->num_paths * sizeof(float));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t recovery_result = {0};
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &recovery_result)) {
            err = cudaMalloc(&d_terminal_values, params->num_paths * sizeof(float));
        }
        if (err != cudaSuccess) {
            set_mc_error("Failed to allocate terminal values: %s", cudaGetErrorString(err));
            NIMCP_THROW_GPU(NIMCP_ERROR_GPU, 0, err, "Failed to allocate terminal values: %s", cudaGetErrorString(err));
            return false;
        }
    }

    // Get RNG states
    curandState* d_rng_states = NULL;
    // Note: In real implementation, get from rng->d_states
    err = cudaMalloc(&d_rng_states, params->num_paths * sizeof(curandState));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t recovery_result = {0};
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &recovery_result)) {
            err = cudaMalloc(&d_rng_states, params->num_paths * sizeof(curandState));
        }
        if (err != cudaSuccess) {
            cudaFree(d_terminal_values);
            set_mc_error("Failed to allocate RNG states: %s", cudaGetErrorString(err));
            NIMCP_THROW_GPU(NIMCP_ERROR_GPU, 0, err, "Failed to allocate RNG states: %s", cudaGetErrorString(err));
            return false;
        }
    }

    // Initialize RNG states
    kernel_init_rng_states<<<grid_size, block_size, 0, stream>>>(
        d_rng_states, params->seed != 0 ? params->seed : (uint64_t)time(NULL),
        params->num_paths);

    float dt = params->time_horizon / (float)params->num_steps;

    // Run appropriate simulation kernel
    if (params->use_antithetic && params->num_paths % 2 == 0) {
        uint32_t half_grid = NIMCP_CUDA_GRID_SIZE(params->num_paths / 2, block_size);
        kernel_gbm_simulate_antithetic<<<half_grid, block_size, 0, stream>>>(
            d_rng_states, d_terminal_values,
            params->initial_value, params->drift, params->volatility,
            dt, params->num_steps, params->num_paths);
    } else {
        kernel_gbm_simulate<<<grid_size, block_size, 0, stream>>>(
            d_rng_states, d_terminal_values,
            params->initial_value, params->drift, params->volatility,
            dt, params->num_steps, params->num_paths);
    }

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    // Compute full statistics including VaR/CVaR
    {
        uint32_t num_paths = params->num_paths;
        float n_paths = (float)num_paths;
        float initial = params->initial_value;
        float discount = expf(-params->drift * params->time_horizon);

        // Copy terminal values to host for comprehensive statistics
        float* h_terminal = (float*)nimcp_malloc(num_paths * sizeof(float));
        float* h_returns = (float*)nimcp_malloc(num_paths * sizeof(float));
        if (!h_terminal || !h_returns) {
            nimcp_free(h_terminal);
            nimcp_free(h_returns);
            set_mc_error("Host allocation failed for statistics");
            goto cleanup;
        }

        NIMCP_CUDA_RECOVER(cudaMemcpy(h_terminal, d_terminal_values, num_paths * sizeof(float),
                                      cudaMemcpyDeviceToHost), GPU_ERROR_CUDA_RUNTIME);

        // Compute statistics on CPU for reliability
        float sum_terminal = 0.0f;
        float sum_sq_terminal = 0.0f;
        float sum_returns = 0.0f;
        float sum_sq_returns = 0.0f;
        float min_term = FLT_MAX;
        float max_term = -FLT_MAX;
        uint32_t loss_count = 0;

        for (uint32_t i = 0; i < num_paths; i++) {
            float term = h_terminal[i];
            float ret = (term - initial) / initial;
            h_returns[i] = ret;

            sum_terminal += term;
            sum_sq_terminal += term * term;
            sum_returns += ret;
            sum_sq_returns += ret * ret;

            if (term < min_term) min_term = term;
            if (term > max_term) max_term = term;
            if (ret < 0.0f) loss_count++;
        }

        // Terminal value statistics
        float mean_terminal = sum_terminal / n_paths;
        float variance_terminal = (sum_sq_terminal / n_paths) - (mean_terminal * mean_terminal);

        // Return statistics
        float mean_return = sum_returns / n_paths;
        float variance_return = (sum_sq_returns / n_paths) - (mean_return * mean_return);
        float std_return = sqrtf(variance_return > 0.0f ? variance_return : 0.0f);

        // Populate basic result fields - no discounting for simulation statistics
        result->mean_value = mean_terminal;  // E[S_T]
        result->std_error = sqrtf(variance_terminal / n_paths);
        result->variance = variance_terminal;
        result->num_paths = num_paths;
        result->paths_completed = num_paths;
        result->mean_return = mean_return;
        result->std_return = std_return;
        result->min_terminal = min_term;
        result->max_terminal = max_term;
        result->probability_of_loss = (float)loss_count / n_paths;

        // Sort returns for VaR/CVaR using qsort (ascending order)
        qsort(h_returns, num_paths, sizeof(float), compare_float_asc);

        // VaR: percentile of sorted returns (worst returns are at the beginning)
        // At 95% confidence, VaR is the 5th percentile
        uint32_t var_95_idx = (uint32_t)(0.05f * n_paths);
        uint32_t var_99_idx = (uint32_t)(0.01f * n_paths);
        if (var_95_idx >= num_paths) var_95_idx = num_paths - 1;
        if (var_99_idx >= num_paths) var_99_idx = num_paths - 1;
        if (var_95_idx == 0) var_95_idx = 1;
        if (var_99_idx == 0) var_99_idx = 1;

        // VaR: negative of the percentile return (expressed as positive loss)
        result->var_95 = -h_returns[var_95_idx];
        result->var_99 = -h_returns[var_99_idx];

        // CVaR: negative of the average of returns below VaR threshold (expected shortfall)
        float sum_cvar_95 = 0.0f;
        float sum_cvar_99 = 0.0f;
        for (uint32_t i = 0; i < var_95_idx; i++) {
            sum_cvar_95 += h_returns[i];
        }
        for (uint32_t i = 0; i < var_99_idx; i++) {
            sum_cvar_99 += h_returns[i];
        }

        result->cvar_95 = var_95_idx > 0 ? -sum_cvar_95 / (float)var_95_idx : result->var_95;
        result->cvar_99 = var_99_idx > 0 ? -sum_cvar_99 / (float)var_99_idx : result->var_99;
        result->expected_shortfall = result->cvar_95;

        // Median terminal value (from sorted returns, reconstruct)
        uint32_t median_idx = num_paths / 2;
        result->median_terminal = initial * (1.0f + h_returns[median_idx]);

        // Compute skewness and kurtosis
        if (std_return > 1e-10f) {
            float m3 = 0.0f, m4 = 0.0f;
            for (uint32_t i = 0; i < num_paths; i++) {
                float diff = (h_returns[i] - mean_return) / std_return;
                float diff2 = diff * diff;
                m3 += diff2 * diff;
                m4 += diff2 * diff2;
            }
            result->skewness = m3 / n_paths;
            result->kurtosis = (m4 / n_paths) - 3.0f;  // Excess kurtosis
        } else {
            result->skewness = 0.0f;
            result->kurtosis = 0.0f;
        }

        // Cleanup
        nimcp_free(h_terminal);
        nimcp_free(h_returns);
    }

    // Handle path storage if requested
    if (params->store_paths) {
        // Allocate path data on device
        size_t path_size = (size_t)params->num_paths * (params->num_steps + 1) * sizeof(float);
        float* d_path_data = NULL;
        err = cudaMalloc(&d_path_data, path_size);
        if (err == cudaSuccess) {
            // Reinitialize RNG states for path simulation
            kernel_init_rng_states<<<grid_size, block_size, 0, stream>>>(
                d_rng_states, params->seed != 0 ? params->seed : (uint64_t)time(NULL) + 1,
                params->num_paths);

            // Run path-storing kernel
            kernel_gbm_simulate_paths<<<grid_size, block_size, 0, stream>>>(
                d_rng_states, d_path_data,
                params->initial_value, params->drift, params->volatility,
                dt, params->num_steps, params->num_paths);

            result->path_data = d_path_data;  // Keep on device
        } else {
            result->path_data = NULL;  // Allocation failed, paths not stored
        }
    } else {
        result->path_data = NULL;
    }

    // Cleanup main allocations
    cudaFree(d_terminal_values);
    cudaFree(d_rng_states);

    return true;

cleanup:
    cudaFree(d_terminal_values);
    cudaFree(d_rng_states);
    return false;
}

bool fin_monte_carlo_gpu_option_price(
    nimcp_gpu_context_t* ctx,
    fin_gpu_rng_t* rng,
    const fin_mc_option_params_t* params,
    fin_mc_option_result_t* result)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !nimcp_gpu_context_is_valid(ctx)) {
        set_mc_error("Invalid GPU context");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid GPU context");
        return false;
    }
    if (!rng || !params || !result) {
        set_mc_error("Invalid parameters");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid parameters");
        return false;
    }

    cudaStream_t stream = nimcp_gpu_get_compute_stream(ctx);
    uint32_t block_size = 256;
    uint32_t grid_size = NIMCP_CUDA_GRID_SIZE(params->base.num_paths, block_size);

    float dt = params->base.time_horizon / (float)params->base.num_steps;
    bool is_call = (params->option_type == FIN_OPT_CALL);

    // Allocate payoffs with recovery
    float* d_payoffs = NULL;
    cudaError_t err = cudaMalloc(&d_payoffs, params->base.num_paths * sizeof(float));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t recovery_result = {0};
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &recovery_result)) {
            err = cudaMalloc(&d_payoffs, params->base.num_paths * sizeof(float));
        }
        if (err != cudaSuccess) {
            set_mc_error("Failed to allocate payoffs: %s", cudaGetErrorString(err));
            NIMCP_THROW_GPU(NIMCP_ERROR_GPU, 0, err, "Failed to allocate payoffs: %s", cudaGetErrorString(err));
            return false;
        }
    }

    // Allocate and initialize RNG states with recovery
    curandState* d_rng_states = NULL;
    err = cudaMalloc(&d_rng_states, params->base.num_paths * sizeof(curandState));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t recovery_result = {0};
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &recovery_result)) {
            err = cudaMalloc(&d_rng_states, params->base.num_paths * sizeof(curandState));
        }
        if (err != cudaSuccess) {
            cudaFree(d_payoffs);
            set_mc_error("Failed to allocate RNG states: %s", cudaGetErrorString(err));
            NIMCP_THROW_GPU(NIMCP_ERROR_GPU, 0, err, "Failed to allocate RNG states: %s", cudaGetErrorString(err));
            return false;
        }
    }

    kernel_init_rng_states<<<grid_size, block_size, 0, stream>>>(
        d_rng_states, params->base.seed != 0 ? params->base.seed : (uint64_t)time(NULL),
        params->base.num_paths);

    // Select kernel based on option style
    switch (params->option_style) {
        case FIN_MC_OPT_ASIAN:
            kernel_asian_option_price<<<grid_size, block_size, 0, stream>>>(
                d_rng_states, d_payoffs,
                params->base.initial_value, params->strike,
                params->base.drift, params->base.volatility,
                dt, params->base.num_steps, params->base.num_paths,
                is_call);
            break;

        case FIN_MC_OPT_LOOKBACK:
            kernel_lookback_option_price<<<grid_size, block_size, 0, stream>>>(
                d_rng_states, d_payoffs,
                params->base.initial_value,
                params->base.drift, params->base.volatility,
                dt, params->base.num_steps, params->base.num_paths,
                is_call);
            break;

        case FIN_MC_OPT_BARRIER:
            kernel_barrier_option_price<<<grid_size, block_size, 0, stream>>>(
                d_rng_states, d_payoffs,
                params->base.initial_value, params->strike, params->barrier,
                params->base.drift, params->base.volatility,
                dt, params->base.num_steps, params->base.num_paths,
                is_call, params->is_up_barrier, params->rebate);
            break;

        default:
            // Standard European - simulate terminal values, then compute payoff
            {
                float* d_terminal_eur = NULL;
                cudaMalloc(&d_terminal_eur, params->base.num_paths * sizeof(float));
                kernel_gbm_simulate<<<grid_size, block_size, 0, stream>>>(
                    d_rng_states, d_terminal_eur,
                    params->base.initial_value,
                    params->base.drift, params->base.volatility,
                    dt, params->base.num_steps, params->base.num_paths);
                kernel_european_payoff<<<grid_size, block_size, 0, stream>>>(
                    d_terminal_eur, d_payoffs, params->strike,
                    params->base.num_paths, is_call);
                cudaFree(d_terminal_eur);
            }
            break;
    }

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    // Compute mean payoff using reduction
    uint32_t reduce_blocks = (params->base.num_paths + block_size * 2 - 1) / (block_size * 2);
    float* d_partial_sums = NULL;
    float* d_partial_sq_sums = NULL;

    err = cudaMalloc(&d_partial_sums, reduce_blocks * sizeof(float));
    if (err != cudaSuccess) {
        cudaFree(d_payoffs);
        cudaFree(d_rng_states);
        return false;
    }
    err = cudaMalloc(&d_partial_sq_sums, reduce_blocks * sizeof(float));
    if (err != cudaSuccess) {
        cudaFree(d_partial_sums);
        cudaFree(d_payoffs);
        cudaFree(d_rng_states);
        return false;
    }

    kernel_reduce_sum_sq<<<reduce_blocks, block_size,
                                  2 * block_size * sizeof(float), stream>>>(
        d_payoffs, d_partial_sums, d_partial_sq_sums, params->base.num_paths);

    // Copy and finalize
    float* h_partial_sums = (float*)nimcp_malloc(reduce_blocks * sizeof(float));
    float* h_partial_sq_sums = (float*)nimcp_malloc(reduce_blocks * sizeof(float));

    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(h_partial_sums, d_partial_sums,
                                      reduce_blocks * sizeof(float),
                                      cudaMemcpyDeviceToHost, stream), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(h_partial_sq_sums, d_partial_sq_sums,
                                      reduce_blocks * sizeof(float),
                                      cudaMemcpyDeviceToHost, stream), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(stream), GPU_ERROR_CUDA_RUNTIME);

    float total_sum = 0.0f;
    float total_sq_sum = 0.0f;
    for (uint32_t i = 0; i < reduce_blocks; i++) {
        total_sum += h_partial_sums[i];
        total_sq_sum += h_partial_sq_sums[i];
    }

    float n = (float)params->base.num_paths;
    float mean_payoff = total_sum / n;
    float variance = (total_sq_sum / n) - (mean_payoff * mean_payoff);

    // Discount to present value
    float discount = expf(-params->base.drift * params->base.time_horizon);

    result->price = mean_payoff * discount;
    result->std_error = sqrtf(variance / n) * discount;
    result->delta = 0.0f;  // TODO: Compute Greeks with finite difference
    result->gamma = 0.0f;

    // Cleanup
    nimcp_free(h_partial_sums);
    nimcp_free(h_partial_sq_sums);
    cudaFree(d_partial_sums);
    cudaFree(d_partial_sq_sums);
    cudaFree(d_payoffs);
    cudaFree(d_rng_states);

    return true;
}

bool fin_monte_carlo_gpu_heston(
    nimcp_gpu_context_t* ctx,
    fin_gpu_rng_t* rng,
    const fin_heston_params_t* params,
    fin_monte_carlo_gpu_result_t* result)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !nimcp_gpu_context_is_valid(ctx)) {
        set_mc_error("Invalid GPU context");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid GPU context");
        return false;
    }
    if (!rng || !params || !result) {
        set_mc_error("Invalid parameters");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid parameters");
        return false;
    }

    cudaStream_t stream = nimcp_gpu_get_compute_stream(ctx);
    uint32_t block_size = 256;
    uint32_t num_paths = params->base.num_paths;
    uint32_t grid_size = NIMCP_CUDA_GRID_SIZE(num_paths, block_size);

    float dt = params->base.time_horizon / (float)params->base.num_steps;

    // Allocate terminal values with recovery
    float* d_terminal_values = NULL;
    cudaError_t err = cudaMalloc(&d_terminal_values, num_paths * sizeof(float));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t recovery_result = {0};
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &recovery_result)) {
            err = cudaMalloc(&d_terminal_values, num_paths * sizeof(float));
        }
        if (err != cudaSuccess) {
            set_mc_error("Failed to allocate terminal values");
            NIMCP_THROW_GPU(NIMCP_ERROR_GPU, 0, err, "Failed to allocate terminal values");
            return false;
        }
    }

    // Allocate RNG states with recovery
    curandState* d_rng_states = NULL;
    err = cudaMalloc(&d_rng_states, num_paths * sizeof(curandState));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t recovery_result = {0};
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &recovery_result)) {
            err = cudaMalloc(&d_rng_states, num_paths * sizeof(curandState));
        }
        if (err != cudaSuccess) {
            cudaFree(d_terminal_values);
            set_mc_error("Failed to allocate RNG states");
            NIMCP_THROW_GPU(NIMCP_ERROR_GPU, 0, err, "Failed to allocate RNG states");
            return false;
        }
    }

    kernel_init_rng_states<<<grid_size, block_size, 0, stream>>>(
        d_rng_states, params->base.seed != 0 ? params->base.seed : (uint64_t)time(NULL),
        num_paths);

    // Run Heston simulation
    kernel_heston_simulate<<<grid_size, block_size, 0, stream>>>(
        d_rng_states, d_terminal_values, NULL,
        params->base.initial_value, params->initial_variance,
        params->base.drift, params->kappa, params->theta,
        params->xi, params->rho, dt,
        params->base.num_steps, num_paths);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    // Compute statistics
    uint32_t reduce_blocks = (num_paths + block_size * 2 - 1) / (block_size * 2);
    float* d_partial_sums = NULL;
    float* d_partial_sq_sums = NULL;

    cudaMalloc(&d_partial_sums, reduce_blocks * sizeof(float));
    cudaMalloc(&d_partial_sq_sums, reduce_blocks * sizeof(float));

    kernel_reduce_sum_sq<<<reduce_blocks, block_size,
                                  2 * block_size * sizeof(float), stream>>>(
        d_terminal_values, d_partial_sums, d_partial_sq_sums, num_paths);

    float* h_partial_sums = (float*)nimcp_malloc(reduce_blocks * sizeof(float));
    float* h_partial_sq_sums = (float*)nimcp_malloc(reduce_blocks * sizeof(float));

    cudaMemcpy(h_partial_sums, d_partial_sums, reduce_blocks * sizeof(float),
               cudaMemcpyDeviceToHost);
    cudaMemcpy(h_partial_sq_sums, d_partial_sq_sums, reduce_blocks * sizeof(float),
               cudaMemcpyDeviceToHost);

    float total_sum = 0.0f;
    float total_sq_sum = 0.0f;
    for (uint32_t i = 0; i < reduce_blocks; i++) {
        total_sum += h_partial_sums[i];
        total_sq_sum += h_partial_sq_sums[i];
    }

    float n = (float)num_paths;
    float mean = total_sum / n;
    float variance = (total_sq_sum / n) - (mean * mean);
    float discount = expf(-params->base.drift * params->base.time_horizon);

    result->mean_value = mean * discount;
    result->std_error = sqrtf(variance / n) * discount;
    result->variance = variance * discount * discount;
    result->num_paths = num_paths;
    result->paths_completed = num_paths;

    // Compute min/max terminal values
    float* h_terminal = (float*)nimcp_malloc(num_paths * sizeof(float));
    if (h_terminal) {
        cudaMemcpy(h_terminal, d_terminal_values, num_paths * sizeof(float), cudaMemcpyDeviceToHost);
        float min_t = FLT_MAX, max_t = -FLT_MAX;
        for (uint32_t i = 0; i < num_paths; i++) {
            if (h_terminal[i] < min_t) min_t = h_terminal[i];
            if (h_terminal[i] > max_t) max_t = h_terminal[i];
        }
        result->min_terminal = min_t;
        result->max_terminal = max_t;
        nimcp_free(h_terminal);
    }

    // Cleanup
    nimcp_free(h_partial_sums);
    nimcp_free(h_partial_sq_sums);
    cudaFree(d_partial_sums);
    cudaFree(d_partial_sq_sums);
    cudaFree(d_terminal_values);
    cudaFree(d_rng_states);

    return true;
}

bool fin_monte_carlo_gpu_multi_asset(
    nimcp_gpu_context_t* ctx,
    fin_gpu_rng_t* rng,
    const fin_multi_asset_params_t* params,
    float* terminal_values)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !nimcp_gpu_context_is_valid(ctx)) {
        set_mc_error("Invalid GPU context");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid GPU context");
        return false;
    }
    if (!rng || !params || !terminal_values) {
        set_mc_error("Invalid parameters");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid parameters");
        return false;
    }

    cudaStream_t stream = nimcp_gpu_get_compute_stream(ctx);
    uint32_t num_paths = params->base.num_paths;
    uint32_t n_assets = params->n_assets;
    uint32_t block_size = min(256u, n_assets);

    float dt = params->base.time_horizon / (float)params->base.num_steps;
    size_t shared_size = 2 * n_assets * sizeof(float);

    // Allocate device memory
    float* d_terminal = NULL;
    float* d_initial = NULL;
    float* d_drifts = NULL;
    float* d_vols = NULL;
    float* d_cholesky = NULL;
    curandState* d_rng_states = NULL;

    cudaError_t err;

    err = cudaMalloc(&d_terminal, num_paths * n_assets * sizeof(float));
    if (err != cudaSuccess) goto cleanup_mc_multi;

    err = cudaMalloc(&d_initial, n_assets * sizeof(float));
    if (err != cudaSuccess) goto cleanup_mc_multi;

    err = cudaMalloc(&d_drifts, n_assets * sizeof(float));
    if (err != cudaSuccess) goto cleanup_mc_multi;

    err = cudaMalloc(&d_vols, n_assets * sizeof(float));
    if (err != cudaSuccess) goto cleanup_mc_multi;

    err = cudaMalloc(&d_cholesky, n_assets * n_assets * sizeof(float));
    if (err != cudaSuccess) goto cleanup_mc_multi;

    err = cudaMalloc(&d_rng_states, num_paths * sizeof(curandState));
    if (err != cudaSuccess) goto cleanup_mc_multi;

    // Copy parameters to device
    cudaMemcpyAsync(d_initial, params->initial_values, n_assets * sizeof(float),
                    cudaMemcpyHostToDevice, stream);
    cudaMemcpyAsync(d_drifts, params->drifts, n_assets * sizeof(float),
                    cudaMemcpyHostToDevice, stream);
    cudaMemcpyAsync(d_vols, params->volatilities, n_assets * sizeof(float),
                    cudaMemcpyHostToDevice, stream);
    cudaMemcpyAsync(d_cholesky, params->cholesky_L, n_assets * n_assets * sizeof(float),
                    cudaMemcpyHostToDevice, stream);

    // Initialize RNG
    kernel_init_rng_states<<<NIMCP_CUDA_GRID_SIZE(num_paths, 256), 256, 0, stream>>>(
        d_rng_states, params->base.seed != 0 ? params->base.seed : (uint64_t)time(NULL),
        num_paths);

    // Run simulation (one block per path)
    kernel_multi_asset_gbm_simulate<<<num_paths, block_size, shared_size, stream>>>(
        d_rng_states, d_terminal, d_initial, d_drifts, d_vols, d_cholesky,
        dt, params->base.num_steps, num_paths, n_assets);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    // Copy results back
    cudaMemcpy(terminal_values, d_terminal, num_paths * n_assets * sizeof(float),
               cudaMemcpyDeviceToHost);

    cudaFree(d_terminal);
    cudaFree(d_initial);
    cudaFree(d_drifts);
    cudaFree(d_vols);
    cudaFree(d_cholesky);
    cudaFree(d_rng_states);

    return true;

cleanup_mc_multi:
    cudaFree(d_terminal);
    cudaFree(d_initial);
    cudaFree(d_drifts);
    cudaFree(d_vols);
    cudaFree(d_cholesky);
    cudaFree(d_rng_states);
    set_mc_error("Memory allocation failed");
    return false;
}

const char* fin_monte_carlo_gpu_get_last_error(void) {
    return g_mc_error;
}

//=============================================================================
// Heston Extended Wrapper (fin_mc_gpu_heston)
//=============================================================================

bool fin_mc_gpu_heston(
    nimcp_gpu_context_t* ctx,
    fin_gpu_rng_t* rng,
    const fin_mc_extended_params_t* params,
    fin_mc_extended_result_t* result)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !nimcp_gpu_context_is_valid(ctx)) {
        set_mc_error("Invalid GPU context");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid GPU context");
        return false;
    }
    if (!rng || !params || !result) {
        set_mc_error("Invalid parameters");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid parameters");
        return false;
    }

    cudaStream_t stream = nimcp_gpu_get_compute_stream(ctx);
    uint32_t block_size = 256;
    uint32_t num_paths = params->base.num_paths;
    uint32_t grid_size = NIMCP_CUDA_GRID_SIZE(num_paths, block_size);
    float dt = params->base.horizon_years / (float)params->base.num_steps;

    float* d_terminal_values = NULL;
    float* d_terminal_vols = NULL;
    curandState* d_rng_states = NULL;
    cudaError_t err;

    err = cudaMalloc(&d_terminal_values, num_paths * sizeof(float));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t recovery_result = {0};
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &recovery_result)) {
            err = cudaMalloc(&d_terminal_values, num_paths * sizeof(float));
        }
        if (err != cudaSuccess) {
            set_mc_error("Failed to allocate terminal values");
            return false;
        }
    }

    err = cudaMalloc(&d_terminal_vols, num_paths * sizeof(float));
    if (err != cudaSuccess) { cudaFree(d_terminal_values); return false; }

    err = cudaMalloc(&d_rng_states, num_paths * sizeof(curandState));
    if (err != cudaSuccess) { cudaFree(d_terminal_values); cudaFree(d_terminal_vols); return false; }

    kernel_init_rng_states<<<grid_size, block_size, 0, stream>>>(
        d_rng_states, params->base.seed != 0 ? params->base.seed : (uint64_t)time(NULL), num_paths);

    kernel_heston_simulate<<<grid_size, block_size, 0, stream>>>(
        d_rng_states, d_terminal_values, d_terminal_vols,
        params->base.initial_value, params->base.initial_variance,
        params->base.risk_free_rate, params->base.mean_reversion,
        params->base.long_term_variance, params->base.vol_of_vol,
        params->base.correlation, dt, params->base.num_steps, num_paths);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    float* h_terminal = (float*)nimcp_malloc(num_paths * sizeof(float));
    float* h_returns = (float*)nimcp_malloc(num_paths * sizeof(float));
    if (!h_terminal || !h_returns) {
        nimcp_free(h_terminal); nimcp_free(h_returns);
        cudaFree(d_terminal_values); cudaFree(d_terminal_vols); cudaFree(d_rng_states);
        return false;
    }

    cudaMemcpy(h_terminal, d_terminal_values, num_paths * sizeof(float), cudaMemcpyDeviceToHost);

    float sum = 0.0f, sum_sq = 0.0f;
    float initial = params->base.initial_value;
    for (uint32_t i = 0; i < num_paths; i++) {
        sum += h_terminal[i];
        sum_sq += h_terminal[i] * h_terminal[i];
        h_returns[i] = (h_terminal[i] - initial) / initial;
    }

    float n = (float)num_paths;
    float mean = sum / n;
    float variance = (sum_sq / n) - (mean * mean);
    float discount = expf(-params->base.risk_free_rate * params->base.horizon_years);

    result->base.mean_value = mean * discount;
    result->base.std_error = sqrtf(variance / n) * discount;
    result->base.variance = variance * discount * discount;
    result->base.num_paths = num_paths;
    result->base.paths_completed = num_paths;

    qsort(h_returns, num_paths, sizeof(float), compare_float_asc);

    uint32_t var_95_idx = (uint32_t)(0.05f * n); if (var_95_idx == 0) var_95_idx = 1;
    uint32_t var_99_idx = (uint32_t)(0.01f * n); if (var_99_idx == 0) var_99_idx = 1;
    result->base.var_95 = h_returns[var_95_idx];
    result->base.var_99 = h_returns[var_99_idx];

    float cvar_sum = 0.0f;
    for (uint32_t i = 0; i < var_95_idx; i++) cvar_sum += h_returns[i];
    result->base.cvar_95 = var_95_idx > 0 ? cvar_sum / var_95_idx : result->base.var_95;
    cvar_sum = 0.0f;
    for (uint32_t i = 0; i < var_99_idx; i++) cvar_sum += h_returns[i];
    result->base.cvar_99 = var_99_idx > 0 ? cvar_sum / var_99_idx : result->base.var_99;

    float mean_return = 0.0f;
    for (uint32_t i = 0; i < num_paths; i++) mean_return += h_returns[i];
    result->base.mean_return = mean_return / n;

    nimcp_free(h_terminal); nimcp_free(h_returns);
    cudaFree(d_terminal_values); cudaFree(d_terminal_vols); cudaFree(d_rng_states);
    return true;
}

//=============================================================================
// Jump-Diffusion (fin_mc_gpu_jump_diffusion)
//=============================================================================

bool fin_mc_gpu_jump_diffusion(
    nimcp_gpu_context_t* ctx,
    fin_gpu_rng_t* rng,
    const fin_mc_extended_params_t* params,
    fin_mc_extended_result_t* result)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !nimcp_gpu_context_is_valid(ctx)) {
        set_mc_error("Invalid GPU context");
        return false;
    }
    if (!rng || !params || !result) {
        set_mc_error("Invalid parameters");
        return false;
    }

    cudaStream_t stream = nimcp_gpu_get_compute_stream(ctx);
    uint32_t block_size = 256;
    uint32_t num_paths = params->base.num_paths;
    uint32_t grid_size = NIMCP_CUDA_GRID_SIZE(num_paths, block_size);
    float dt = params->base.horizon_years / (float)params->base.num_steps;

    float* d_terminal_values = NULL;
    curandState* d_rng_states = NULL;
    cudaError_t err;

    err = cudaMalloc(&d_terminal_values, num_paths * sizeof(float));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t recovery_result = {0};
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &recovery_result)) {
            err = cudaMalloc(&d_terminal_values, num_paths * sizeof(float));
        }
        if (err != cudaSuccess) { set_mc_error("Failed to allocate terminal values"); return false; }
    }

    err = cudaMalloc(&d_rng_states, num_paths * sizeof(curandState));
    if (err != cudaSuccess) { cudaFree(d_terminal_values); return false; }

    kernel_init_rng_states<<<grid_size, block_size, 0, stream>>>(
        d_rng_states, params->base.seed != 0 ? params->base.seed : (uint64_t)time(NULL), num_paths);

    kernel_jump_diffusion_simulate<<<grid_size, block_size, 0, stream>>>(
        d_rng_states, d_terminal_values, params->base.initial_value,
        params->base.risk_free_rate, params->base.volatility,
        params->base.jump_intensity, params->base.jump_mean, params->base.jump_vol,
        dt, params->base.num_steps, num_paths);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    float* h_terminal = (float*)nimcp_malloc(num_paths * sizeof(float));
    float* h_returns = (float*)nimcp_malloc(num_paths * sizeof(float));
    if (!h_terminal || !h_returns) {
        nimcp_free(h_terminal); nimcp_free(h_returns);
        cudaFree(d_terminal_values); cudaFree(d_rng_states);
        return false;
    }

    cudaMemcpy(h_terminal, d_terminal_values, num_paths * sizeof(float), cudaMemcpyDeviceToHost);

    float sum = 0.0f, sum_sq = 0.0f;
    float initial = params->base.initial_value;
    for (uint32_t i = 0; i < num_paths; i++) {
        sum += h_terminal[i];
        sum_sq += h_terminal[i] * h_terminal[i];
        h_returns[i] = (h_terminal[i] - initial) / initial;
    }

    float n = (float)num_paths;
    float mean = sum / n;
    float variance = (sum_sq / n) - (mean * mean);
    float discount = expf(-params->base.risk_free_rate * params->base.horizon_years);

    result->base.mean_value = mean * discount;
    result->base.std_error = sqrtf(variance / n) * discount;
    result->base.variance = variance * discount * discount;
    result->base.num_paths = num_paths;
    result->base.paths_completed = num_paths;

    qsort(h_returns, num_paths, sizeof(float), compare_float_asc);

    uint32_t var_95_idx = (uint32_t)(0.05f * n); if (var_95_idx == 0) var_95_idx = 1;
    uint32_t var_99_idx = (uint32_t)(0.01f * n); if (var_99_idx == 0) var_99_idx = 1;
    result->base.var_95 = h_returns[var_95_idx];
    result->base.var_99 = h_returns[var_99_idx];

    float cvar_sum = 0.0f;
    for (uint32_t i = 0; i < var_95_idx; i++) cvar_sum += h_returns[i];
    result->base.cvar_95 = var_95_idx > 0 ? cvar_sum / var_95_idx : result->base.var_95;
    cvar_sum = 0.0f;
    for (uint32_t i = 0; i < var_99_idx; i++) cvar_sum += h_returns[i];
    result->base.cvar_99 = var_99_idx > 0 ? cvar_sum / var_99_idx : result->base.var_99;

    float mean_return = 0.0f;
    for (uint32_t i = 0; i < num_paths; i++) mean_return += h_returns[i];
    result->base.mean_return = mean_return / n;

    nimcp_free(h_terminal); nimcp_free(h_returns);
    cudaFree(d_terminal_values); cudaFree(d_rng_states);
    return true;
}

//=============================================================================
// Path-Dependent Option Pricing (fin_mc_gpu_path_dependent_option)
//=============================================================================

bool fin_mc_gpu_path_dependent_option(
    nimcp_gpu_context_t* ctx,
    fin_gpu_rng_t* rng,
    float spot,
    float strike,
    const fin_mc_extended_params_t* params,
    bool is_call,
    fin_mc_extended_result_t* result)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !nimcp_gpu_context_is_valid(ctx) || !rng || !params || !result) {
        set_mc_error("Invalid parameters");
        return false;
    }

    cudaStream_t stream = nimcp_gpu_get_compute_stream(ctx);
    uint32_t block_size = 256;
    uint32_t num_paths = params->base.num_paths;
    uint32_t grid_size = NIMCP_CUDA_GRID_SIZE(num_paths, block_size);
    float dt = params->base.horizon_years / (float)params->base.num_steps;

    float* d_payoffs = NULL;
    curandState* d_rng_states = NULL;
    cudaError_t err;

    err = cudaMalloc(&d_payoffs, num_paths * sizeof(float));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t recovery_result = {0};
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &recovery_result)) {
            err = cudaMalloc(&d_payoffs, num_paths * sizeof(float));
        }
        if (err != cudaSuccess) return false;
    }

    err = cudaMalloc(&d_rng_states, num_paths * sizeof(curandState));
    if (err != cudaSuccess) { cudaFree(d_payoffs); return false; }

    kernel_init_rng_states<<<grid_size, block_size, 0, stream>>>(
        d_rng_states, params->base.seed != 0 ? params->base.seed : (uint64_t)time(NULL), num_paths);

    switch (params->option_type) {
        case FIN_MC_OPTION_ASIAN_ARITHMETIC:
        case FIN_MC_OPTION_ASIAN_GEOMETRIC:
            kernel_asian_option_price<<<grid_size, block_size, 0, stream>>>(
                d_rng_states, d_payoffs, spot, strike,
                params->base.risk_free_rate, params->base.volatility,
                dt, params->base.num_steps, num_paths, is_call);
            break;
        case FIN_MC_OPTION_LOOKBACK_FIXED:
        case FIN_MC_OPTION_LOOKBACK_FLOATING:
            kernel_lookback_option_price<<<grid_size, block_size, 0, stream>>>(
                d_rng_states, d_payoffs, spot,
                params->base.risk_free_rate, params->base.volatility,
                dt, params->base.num_steps, num_paths, is_call);
            break;
        case FIN_MC_OPTION_BARRIER_UP_IN:
        case FIN_MC_OPTION_BARRIER_UP_OUT:
        case FIN_MC_OPTION_BARRIER_DOWN_IN:
        case FIN_MC_OPTION_BARRIER_DOWN_OUT: {
            bool is_up = (params->option_type == FIN_MC_OPTION_BARRIER_UP_IN ||
                          params->option_type == FIN_MC_OPTION_BARRIER_UP_OUT);
            kernel_barrier_option_price<<<grid_size, block_size, 0, stream>>>(
                d_rng_states, d_payoffs, spot, strike, params->barrier,
                params->base.risk_free_rate, params->base.volatility,
                dt, params->base.num_steps, num_paths, is_call, is_up, 0.0f);
            break;
        }
        default: {
            float* d_terminal = NULL;
            cudaMalloc(&d_terminal, num_paths * sizeof(float));
            kernel_gbm_simulate<<<grid_size, block_size, 0, stream>>>(
                d_rng_states, d_terminal, spot, params->base.risk_free_rate,
                params->base.volatility, dt, params->base.num_steps, num_paths);
            kernel_european_payoff<<<grid_size, block_size, 0, stream>>>(
                d_terminal, d_payoffs, strike, num_paths, is_call);
            cudaFree(d_terminal);
            break;
        }
    }

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    float* h_payoffs = (float*)nimcp_malloc(num_paths * sizeof(float));
    if (!h_payoffs) { cudaFree(d_payoffs); cudaFree(d_rng_states); return false; }

    cudaMemcpy(h_payoffs, d_payoffs, num_paths * sizeof(float), cudaMemcpyDeviceToHost);

    float sum = 0.0f, sum_sq = 0.0f;
    for (uint32_t i = 0; i < num_paths; i++) {
        sum += h_payoffs[i];
        sum_sq += h_payoffs[i] * h_payoffs[i];
    }

    float n = (float)num_paths;
    float mean_payoff = sum / n;
    float variance = (sum_sq / n) - (mean_payoff * mean_payoff);
    float discount = expf(-params->base.risk_free_rate * params->base.horizon_years);

    result->option_price = mean_payoff * discount;
    result->option_std_error = sqrtf(variance / n) * discount;
    result->base.mean_value = result->option_price;
    result->base.std_error = result->option_std_error;
    result->base.num_paths = num_paths;
    result->base.paths_completed = num_paths;

    if (params->compute_delta) {
        float bump = params->bump_size * spot;
        float* d_payoffs_up = NULL;
        float* d_terminal_up = NULL;
        cudaMalloc(&d_payoffs_up, num_paths * sizeof(float));
        cudaMalloc(&d_terminal_up, num_paths * sizeof(float));

        kernel_init_rng_states<<<grid_size, block_size, 0, stream>>>(
            d_rng_states, params->base.seed != 0 ? params->base.seed : (uint64_t)time(NULL), num_paths);

        kernel_gbm_simulate<<<grid_size, block_size, 0, stream>>>(
            d_rng_states, d_terminal_up, spot + bump, params->base.risk_free_rate,
            params->base.volatility, dt, params->base.num_steps, num_paths);
        kernel_european_payoff<<<grid_size, block_size, 0, stream>>>(
            d_terminal_up, d_payoffs_up, strike, num_paths, is_call);

        float* h_payoffs_up = (float*)nimcp_malloc(num_paths * sizeof(float));
        cudaMemcpy(h_payoffs_up, d_payoffs_up, num_paths * sizeof(float), cudaMemcpyDeviceToHost);

        float up_sum = 0.0f;
        for (uint32_t i = 0; i < num_paths; i++) up_sum += h_payoffs_up[i];
        float price_up = (up_sum / n) * discount;
        result->delta = (price_up - result->option_price) / bump;

        nimcp_free(h_payoffs_up);
        cudaFree(d_terminal_up);
        cudaFree(d_payoffs_up);
    }

    nimcp_free(h_payoffs);
    cudaFree(d_payoffs);
    cudaFree(d_rng_states);
    return true;
}

//=============================================================================
// Correlated Multi-Asset Simulation (fin_mc_gpu_correlated_assets)
//=============================================================================

bool fin_mc_gpu_correlated_assets(
    nimcp_gpu_context_t* ctx,
    fin_gpu_rng_t* rng,
    const float* initial_values,
    const float* drifts,
    const float* volatilities,
    const float* correlation,
    uint32_t num_assets,
    const fin_monte_carlo_gpu_params_t* params,
    float* terminal_values)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !nimcp_gpu_context_is_valid(ctx) || !rng || !initial_values ||
        !drifts || !volatilities || !correlation || !params || !terminal_values) {
        set_mc_error("Invalid parameters");
        return false;
    }
    if (num_assets == 0 || num_assets > FIN_GPU_MAX_ASSETS) {
        set_mc_error("Invalid number of assets: %u", num_assets);
        return false;
    }

    cudaStream_t stream = nimcp_gpu_get_compute_stream(ctx);
    uint32_t num_paths = params->num_paths;
    uint32_t block_size = min(256u, num_assets);
    float dt = params->time_horizon / (float)params->num_steps;

    // Compute Cholesky decomposition on CPU
    float* h_cholesky = (float*)nimcp_calloc(num_assets * num_assets, sizeof(float));
    if (!h_cholesky) return false;

    for (uint32_t j = 0; j < num_assets; j++) {
        float sum = correlation[j * num_assets + j];
        for (uint32_t k = 0; k < j; k++) sum -= h_cholesky[j * num_assets + k] * h_cholesky[j * num_assets + k];
        if (sum <= 0.0f) sum = 1e-6f;
        h_cholesky[j * num_assets + j] = sqrtf(sum);
        for (uint32_t i = j + 1; i < num_assets; i++) {
            sum = correlation[i * num_assets + j];
            for (uint32_t k = 0; k < j; k++) sum -= h_cholesky[i * num_assets + k] * h_cholesky[j * num_assets + k];
            h_cholesky[i * num_assets + j] = sum / h_cholesky[j * num_assets + j];
        }
    }

    float* d_terminal = NULL; float* d_initial = NULL; float* d_drifts = NULL;
    float* d_vols = NULL; float* d_cholesky = NULL; curandState* d_rng_states = NULL;
    cudaError_t err;

    err = cudaMalloc(&d_terminal, num_paths * num_assets * sizeof(float)); if (err != cudaSuccess) goto cleanup_corr;
    err = cudaMalloc(&d_initial, num_assets * sizeof(float)); if (err != cudaSuccess) goto cleanup_corr;
    err = cudaMalloc(&d_drifts, num_assets * sizeof(float)); if (err != cudaSuccess) goto cleanup_corr;
    err = cudaMalloc(&d_vols, num_assets * sizeof(float)); if (err != cudaSuccess) goto cleanup_corr;
    err = cudaMalloc(&d_cholesky, num_assets * num_assets * sizeof(float)); if (err != cudaSuccess) goto cleanup_corr;
    err = cudaMalloc(&d_rng_states, num_paths * sizeof(curandState)); if (err != cudaSuccess) goto cleanup_corr;

    cudaMemcpyAsync(d_initial, initial_values, num_assets * sizeof(float), cudaMemcpyHostToDevice, stream);
    cudaMemcpyAsync(d_drifts, drifts, num_assets * sizeof(float), cudaMemcpyHostToDevice, stream);
    cudaMemcpyAsync(d_vols, volatilities, num_assets * sizeof(float), cudaMemcpyHostToDevice, stream);
    cudaMemcpyAsync(d_cholesky, h_cholesky, num_assets * num_assets * sizeof(float), cudaMemcpyHostToDevice, stream);

    kernel_init_rng_states<<<NIMCP_CUDA_GRID_SIZE(num_paths, 256), 256, 0, stream>>>(
        d_rng_states, params->seed != 0 ? params->seed : (uint64_t)time(NULL), num_paths);

    kernel_multi_asset_gbm_simulate<<<num_paths, block_size, 2 * num_assets * sizeof(float), stream>>>(
        d_rng_states, d_terminal, d_initial, d_drifts, d_vols, d_cholesky,
        dt, params->num_steps, num_paths, num_assets);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    cudaMemcpy(terminal_values, d_terminal, num_paths * num_assets * sizeof(float), cudaMemcpyDeviceToHost);

    cudaFree(d_terminal); cudaFree(d_initial); cudaFree(d_drifts);
    cudaFree(d_vols); cudaFree(d_cholesky); cudaFree(d_rng_states);
    nimcp_free(h_cholesky);
    return true;

cleanup_corr:
    cudaFree(d_terminal); cudaFree(d_initial); cudaFree(d_drifts);
    cudaFree(d_vols); cudaFree(d_cholesky); cudaFree(d_rng_states);
    nimcp_free(h_cholesky);
    return false;
}

//=============================================================================
// GPU Cholesky Decomposition (fin_mc_gpu_cholesky)
//=============================================================================

bool fin_mc_gpu_cholesky(
    nimcp_gpu_context_t* ctx,
    const float* correlation,
    uint32_t n,
    float* cholesky)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !nimcp_gpu_context_is_valid(ctx) || !correlation || !cholesky || n == 0) {
        set_mc_error("Invalid parameters");
        return false;
    }

    cudaStream_t stream = nimcp_gpu_get_compute_stream(ctx);
    float* d_A = NULL; float* d_L = NULL;

    cudaError_t err = cudaMalloc(&d_A, n * n * sizeof(float));
    if (err != cudaSuccess) return false;

    err = cudaMalloc(&d_L, n * n * sizeof(float));
    if (err != cudaSuccess) { cudaFree(d_A); return false; }

    cudaMemcpyAsync(d_A, correlation, n * n * sizeof(float), cudaMemcpyHostToDevice, stream);
    cudaMemsetAsync(d_L, 0, n * n * sizeof(float), stream);

    kernel_cholesky_decomp<<<1, 1, 0, stream>>>(d_L, d_A, n);
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    cudaMemcpy(cholesky, d_L, n * n * sizeof(float), cudaMemcpyDeviceToHost);
    cudaFree(d_A); cudaFree(d_L);
    return true;
}

//=============================================================================
// Free Extended Result (fin_mc_extended_result_free)
//=============================================================================

void fin_mc_extended_result_free(
    nimcp_gpu_context_t* ctx,
    fin_mc_extended_result_t* result)
{
    (void)ctx;
    if (!result) return;
    if (result->base.terminal_values) { cudaFree(result->base.terminal_values); result->base.terminal_values = NULL; }
    if (result->base.path_data) { cudaFree(result->base.path_data); result->base.path_data = NULL; }
    memset(result, 0, sizeof(fin_mc_extended_result_t));
}

//=============================================================================
// Portfolio Monte Carlo (fin_monte_carlo_gpu_portfolio)
//=============================================================================

bool fin_monte_carlo_gpu_portfolio(
    nimcp_gpu_context_t* ctx,
    fin_gpu_rng_t* rng,
    const float* weights,
    const float* expected_returns,
    const float* covariance,
    uint32_t num_assets,
    const fin_monte_carlo_gpu_params_t* params,
    fin_monte_carlo_gpu_result_t* result)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !nimcp_gpu_context_is_valid(ctx) || !rng || !weights ||
        !expected_returns || !covariance || !params || !result) {
        set_mc_error("Invalid parameters");
        return false;
    }
    if (num_assets == 0 || num_assets > FIN_GPU_MAX_ASSETS) {
        set_mc_error("Invalid number of assets");
        return false;
    }

    cudaStream_t stream = nimcp_gpu_get_compute_stream(ctx);
    uint32_t num_paths = params->num_paths;
    uint32_t block_size = min(256u, num_assets);
    float dt = params->time_horizon / (float)params->num_steps;

    // Extract volatilities and compute correlation/Cholesky
    float* h_volatilities = (float*)nimcp_malloc(num_assets * sizeof(float));
    float* h_correlation = (float*)nimcp_malloc(num_assets * num_assets * sizeof(float));
    float* h_cholesky = (float*)nimcp_calloc(num_assets * num_assets, sizeof(float));
    if (!h_volatilities || !h_correlation || !h_cholesky) {
        nimcp_free(h_volatilities); nimcp_free(h_correlation); nimcp_free(h_cholesky);
        return false;
    }

    for (uint32_t i = 0; i < num_assets; i++)
        h_volatilities[i] = sqrtf(covariance[i * num_assets + i]);

    for (uint32_t i = 0; i < num_assets; i++) {
        for (uint32_t j = 0; j < num_assets; j++) {
            float denom = h_volatilities[i] * h_volatilities[j];
            h_correlation[i * num_assets + j] = (denom > 0.0f) ? covariance[i * num_assets + j] / denom : 0.0f;
        }
    }

    for (uint32_t j = 0; j < num_assets; j++) {
        float sum = h_correlation[j * num_assets + j];
        for (uint32_t k = 0; k < j; k++) sum -= h_cholesky[j * num_assets + k] * h_cholesky[j * num_assets + k];
        if (sum <= 0.0f) sum = 1e-6f;
        h_cholesky[j * num_assets + j] = sqrtf(sum);
        for (uint32_t i = j + 1; i < num_assets; i++) {
            sum = h_correlation[i * num_assets + j];
            for (uint32_t k = 0; k < j; k++) sum -= h_cholesky[i * num_assets + k] * h_cholesky[j * num_assets + k];
            h_cholesky[i * num_assets + j] = sum / h_cholesky[j * num_assets + j];
        }
    }

    for (uint32_t i = 0; i < num_assets; i++)
        for (uint32_t j = 0; j <= i; j++)
            h_cholesky[i * num_assets + j] *= h_volatilities[i];

    float* d_portfolio_values = NULL; float* d_weights = NULL;
    float* d_returns = NULL; float* d_cholesky = NULL;
    curandState* d_rng_states = NULL;
    cudaError_t err;

    err = cudaMalloc(&d_portfolio_values, num_paths * sizeof(float)); if (err != cudaSuccess) goto cleanup_port;
    err = cudaMalloc(&d_weights, num_assets * sizeof(float)); if (err != cudaSuccess) goto cleanup_port;
    err = cudaMalloc(&d_returns, num_assets * sizeof(float)); if (err != cudaSuccess) goto cleanup_port;
    err = cudaMalloc(&d_cholesky, num_assets * num_assets * sizeof(float)); if (err != cudaSuccess) goto cleanup_port;
    err = cudaMalloc(&d_rng_states, num_paths * sizeof(curandState)); if (err != cudaSuccess) goto cleanup_port;

    cudaMemcpyAsync(d_weights, weights, num_assets * sizeof(float), cudaMemcpyHostToDevice, stream);
    cudaMemcpyAsync(d_returns, expected_returns, num_assets * sizeof(float), cudaMemcpyHostToDevice, stream);
    cudaMemcpyAsync(d_cholesky, h_cholesky, num_assets * num_assets * sizeof(float), cudaMemcpyHostToDevice, stream);

    kernel_init_rng_states<<<NIMCP_CUDA_GRID_SIZE(num_paths, 256), 256, 0, stream>>>(
        d_rng_states, params->seed != 0 ? params->seed : (uint64_t)time(NULL), num_paths);

    kernel_portfolio_simulate<<<num_paths, block_size, 3 * num_assets * sizeof(float), stream>>>(
        d_rng_states, d_weights, d_returns, d_cholesky, d_portfolio_values,
        num_assets, dt, params->num_steps, num_paths);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    {
        float* h_portfolio_values = (float*)nimcp_malloc(num_paths * sizeof(float));
        float* h_returns_arr = (float*)nimcp_malloc(num_paths * sizeof(float));
        if (!h_portfolio_values || !h_returns_arr) {
            nimcp_free(h_portfolio_values); nimcp_free(h_returns_arr);
            goto cleanup_port;
        }

        cudaMemcpy(h_portfolio_values, d_portfolio_values, num_paths * sizeof(float), cudaMemcpyDeviceToHost);

        float sum = 0.0f, sum_sq = 0.0f;
        for (uint32_t i = 0; i < num_paths; i++) {
            float val = h_portfolio_values[i];
            sum += val; sum_sq += val * val;
            h_returns_arr[i] = val - 1.0f;
        }

        float n = (float)num_paths;
        result->mean_value = sum / n;
        result->variance = (sum_sq / n) - (result->mean_value * result->mean_value);
        result->std_error = sqrtf(result->variance / n);
        result->num_paths = num_paths;
        result->paths_completed = num_paths;

        qsort(h_returns_arr, num_paths, sizeof(float), compare_float_asc);

        uint32_t var_95_idx = (uint32_t)(0.05f * n); if (var_95_idx == 0) var_95_idx = 1;
        uint32_t var_99_idx = (uint32_t)(0.01f * n); if (var_99_idx == 0) var_99_idx = 1;
        result->var_95 = h_returns_arr[var_95_idx];
        result->var_99 = h_returns_arr[var_99_idx];

        float cvar_sum = 0.0f;
        for (uint32_t i = 0; i < var_95_idx; i++) cvar_sum += h_returns_arr[i];
        result->cvar_95 = var_95_idx > 0 ? cvar_sum / var_95_idx : result->var_95;
        cvar_sum = 0.0f;
        for (uint32_t i = 0; i < var_99_idx; i++) cvar_sum += h_returns_arr[i];
        result->cvar_99 = var_99_idx > 0 ? cvar_sum / var_99_idx : result->var_99;

        float mean_return = 0.0f;
        for (uint32_t i = 0; i < num_paths; i++) mean_return += h_returns_arr[i];
        result->mean_return = mean_return / n;

        nimcp_free(h_portfolio_values);
        nimcp_free(h_returns_arr);
    }

    cudaFree(d_portfolio_values); cudaFree(d_weights); cudaFree(d_returns);
    cudaFree(d_cholesky); cudaFree(d_rng_states);
    nimcp_free(h_volatilities); nimcp_free(h_correlation); nimcp_free(h_cholesky);
    return true;

cleanup_port:
    cudaFree(d_portfolio_values); cudaFree(d_weights); cudaFree(d_returns);
    cudaFree(d_cholesky); cudaFree(d_rng_states);
    nimcp_free(h_volatilities); nimcp_free(h_correlation); nimcp_free(h_cholesky);
    return false;
}

//=============================================================================
// Free Monte Carlo Result (fin_monte_carlo_gpu_result_free)
//=============================================================================

void fin_monte_carlo_gpu_result_free(
    nimcp_gpu_context_t* ctx,
    fin_monte_carlo_gpu_result_t* result)
{
    (void)ctx;
    if (!result) return;
    if (result->terminal_values) { cudaFree(result->terminal_values); result->terminal_values = NULL; }
    if (result->path_data) { cudaFree(result->path_data); result->path_data = NULL; }
    result->mean_return = 0.0f; result->mean_value = 0.0f;
    result->std_return = 0.0f; result->std_error = 0.0f;
    result->variance = 0.0f; result->var_95 = 0.0f; result->var_99 = 0.0f;
    result->cvar_95 = 0.0f; result->cvar_99 = 0.0f;
    result->num_paths = 0; result->paths_completed = 0;
}

} // extern "C"

#else // !NIMCP_ENABLE_CUDA

//=============================================================================
// CPU Fallback Stubs
//=============================================================================

extern "C" {

bool fin_monte_carlo_gpu_simulate(
    nimcp_gpu_context_t* ctx,
    fin_gpu_rng_t* rng,
    const fin_monte_carlo_gpu_params_t* params,
    fin_monte_carlo_gpu_result_t* result)
{
    (void)ctx; (void)rng; (void)params; (void)result;
    return false;
}

bool fin_monte_carlo_gpu_option_price(
    nimcp_gpu_context_t* ctx,
    fin_gpu_rng_t* rng,
    const fin_mc_option_params_t* params,
    fin_mc_option_result_t* result)
{
    (void)ctx; (void)rng; (void)params; (void)result;
    return false;
}

bool fin_monte_carlo_gpu_heston(
    nimcp_gpu_context_t* ctx,
    fin_gpu_rng_t* rng,
    const fin_heston_params_t* params,
    fin_monte_carlo_gpu_result_t* result)
{
    (void)ctx; (void)rng; (void)params; (void)result;
    return false;
}

bool fin_monte_carlo_gpu_multi_asset(
    nimcp_gpu_context_t* ctx,
    fin_gpu_rng_t* rng,
    const fin_multi_asset_params_t* params,
    float* terminal_values)
{
    (void)ctx; (void)rng; (void)params; (void)terminal_values;
    return false;
}

bool fin_mc_gpu_heston(
    nimcp_gpu_context_t* ctx,
    fin_gpu_rng_t* rng,
    const fin_mc_extended_params_t* params,
    fin_mc_extended_result_t* result)
{
    (void)ctx; (void)rng; (void)params; (void)result;
    return false;
}

bool fin_mc_gpu_jump_diffusion(
    nimcp_gpu_context_t* ctx,
    fin_gpu_rng_t* rng,
    const fin_mc_extended_params_t* params,
    fin_mc_extended_result_t* result)
{
    (void)ctx; (void)rng; (void)params; (void)result;
    return false;
}

bool fin_mc_gpu_path_dependent_option(
    nimcp_gpu_context_t* ctx,
    fin_gpu_rng_t* rng,
    float spot,
    float strike,
    const fin_mc_extended_params_t* params,
    bool is_call,
    fin_mc_extended_result_t* result)
{
    (void)ctx; (void)rng; (void)spot; (void)strike;
    (void)params; (void)is_call; (void)result;
    return false;
}

bool fin_mc_gpu_correlated_assets(
    nimcp_gpu_context_t* ctx,
    fin_gpu_rng_t* rng,
    const float* initial_values,
    const float* drifts,
    const float* volatilities,
    const float* correlation,
    uint32_t num_assets,
    const fin_monte_carlo_gpu_params_t* params,
    float* terminal_values)
{
    (void)ctx; (void)rng; (void)initial_values; (void)drifts;
    (void)volatilities; (void)correlation; (void)num_assets;
    (void)params; (void)terminal_values;
    return false;
}

bool fin_mc_gpu_cholesky(
    nimcp_gpu_context_t* ctx,
    const float* correlation,
    uint32_t n,
    float* cholesky)
{
    (void)ctx; (void)correlation; (void)n; (void)cholesky;
    return false;
}

void fin_mc_extended_result_free(
    nimcp_gpu_context_t* ctx,
    fin_mc_extended_result_t* result)
{
    (void)ctx; (void)result;
}

bool fin_monte_carlo_gpu_portfolio(
    nimcp_gpu_context_t* ctx,
    fin_gpu_rng_t* rng,
    const float* weights,
    const float* expected_returns,
    const float* covariance,
    uint32_t num_assets,
    const fin_monte_carlo_gpu_params_t* params,
    fin_monte_carlo_gpu_result_t* result)
{
    (void)ctx; (void)rng; (void)weights; (void)expected_returns;
    (void)covariance; (void)num_assets; (void)params; (void)result;
    return false;
}

void fin_monte_carlo_gpu_result_free(
    nimcp_gpu_context_t* ctx,
    fin_monte_carlo_gpu_result_t* result)
{
    (void)ctx; (void)result;
}

const char* fin_monte_carlo_gpu_get_last_error(void) {
    return "GPU support not compiled";
}

} // extern "C"

#endif // NIMCP_ENABLE_CUDA
