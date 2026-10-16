/**
 * @file nimcp_financial_risk_gpu.cu
 * @brief GPU Risk Metrics Computation
 *
 * WHAT: GPU-accelerated risk metric calculations
 * WHY:  Fast VaR, CVaR, volatility for large portfolios
 * HOW:  GPU sorting, parallel reduction, rolling windows
 *
 * Implements:
 *   - Value at Risk (VaR): Historical, Parametric, Monte Carlo
 *   - Conditional VaR (CVaR/Expected Shortfall)
 *   - Volatility estimation: Simple, EWMA, Parkinson, Garman-Klass
 *   - Rolling risk metrics
 *   - Batch risk computation for multiple portfolios
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#ifdef NIMCP_ENABLE_CUDA

#include "utils/memory/nimcp_memory.h"
#include <cuda_runtime.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <float.h>
#include <vector>

#include "gpu/financial/nimcp_financial_gpu.h"
#include "gpu/financial/nimcp_financial_risk_gpu.h"
#include "gpu/common/nimcp_cuda_utils.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "gpu/statistics/nimcp_statistics_gpu.h"  // Central GPU statistics module
#include "constants/nimcp_buffer_constants.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "constants/nimcp_math_constants.h"

//=============================================================================
// Thread-Local Error Storage
//=============================================================================

static __thread char g_risk_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_risk_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_risk_error, sizeof(g_risk_error), fmt, args);
    va_end(args);
}

//=============================================================================
// Device Helper Functions
//=============================================================================

/**
 * @brief Standard normal CDF approximation
 */
__device__ __forceinline__ float norm_cdf_risk(float x) {
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
    float y = 1.0f - (((((a5*t + a4)*t) + a3)*t + a2)*t + a1)*t * expf(-x*x/2.0f);

    return 0.5f * (1.0f + sign * y);
}

/**
 * @brief Standard normal quantile (inverse CDF) approximation
 */
__device__ float norm_quantile_device(float p) {
    // Rational approximation for normal quantile
    if (p <= 0.0f) return -FLT_MAX;
    if (p >= 1.0f) return FLT_MAX;

    float t;
    if (p < 0.5f) {
        t = sqrtf(-2.0f * logf(p));
    } else {
        t = sqrtf(-2.0f * logf(1.0f - p));
    }

    // Coefficients for rational approximation
    const float c0 = 2.515517f;
    const float c1 = 0.802853f;
    const float c2 = 0.010328f;
    const float d1 = 1.432788f;
    const float d2 = 0.189269f;
    const float d3 = 0.001308f;

    float result = t - (c0 + c1*t + c2*t*t) / (1.0f + d1*t + d2*t*t + d3*t*t*t);

    return (p < 0.5f) ? -result : result;
}

//=============================================================================
// Bitonic Sort Kernels (for VaR percentile)
//=============================================================================

/**
 * @brief Bitonic sort compare-and-swap step
 */
__device__ __forceinline__ void bitonic_compare(
    float* arr, uint32_t i, uint32_t j, bool ascending)
{
    if ((arr[i] > arr[j]) == ascending) {
        float tmp = arr[i];
        arr[i] = arr[j];
        arr[j] = tmp;
    }
}

/**
 * @brief Bitonic sort step kernel
 */
__global__ void kernel_bitonic_sort_step(
    float* __restrict__ data,
    uint32_t j,
    uint32_t k,
    uint32_t n)
{
    uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t ixj = i ^ j;

    if (ixj > i && ixj < n && i < n) {
        // Ascending if (i & k) == 0
        bool ascending = ((i & k) == 0);
        if ((data[i] > data[ixj]) == ascending) {
            float tmp = data[i];
            data[i] = data[ixj];
            data[ixj] = tmp;
        }
    }
}

/**
 * @brief Full bitonic sort (for small arrays fitting in shared memory)
 */
__global__ void kernel_bitonic_sort_shared(
    float* __restrict__ data,
    uint32_t n)
{
    extern __shared__ float s_data[];

    uint32_t tid = threadIdx.x;
    uint32_t n_padded = blockDim.x * 2;

    // Load into shared memory (each thread loads 2 elements)
    if (tid < n) s_data[tid] = data[tid];
    else s_data[tid] = FLT_MAX;

    if (tid + blockDim.x < n) s_data[tid + blockDim.x] = data[tid + blockDim.x];
    else s_data[tid + blockDim.x] = FLT_MAX;

    __syncthreads();

    // Bitonic sort in shared memory
    // Each thread handles one comparison pair per stage
    for (uint32_t k = 2; k <= n_padded; k *= 2) {
        for (uint32_t j = k / 2; j > 0; j /= 2) {
            // Compute which element pair this thread compares
            // Thread tid compares elements (i, i^j) where i = tid with some offset
            uint32_t i = tid;
            // Map thread to comparison position
            // For j-stage, threads compare (0,j), (1,j+1), ..., but skipping already-compared pairs
            uint32_t segment = tid / j;
            uint32_t pos_in_segment = tid % j;
            i = segment * 2 * j + pos_in_segment;

            uint32_t ixj = i ^ j;

            if (i < n_padded && ixj < n_padded && i < ixj) {
                // Ascending if (i & k) == 0
                bool ascending = ((i & k) == 0);
                if ((s_data[i] > s_data[ixj]) == ascending) {
                    float tmp = s_data[i];
                    s_data[i] = s_data[ixj];
                    s_data[ixj] = tmp;
                }
            }
            __syncthreads();
        }
    }

    // Write back
    if (tid < n) data[tid] = s_data[tid];
    if (tid + blockDim.x < n) data[tid + blockDim.x] = s_data[tid + blockDim.x];
}

//=============================================================================
// Statistical Kernels
//=============================================================================
//
// NOTE ON STATISTICS KERNEL ARCHITECTURE:
// These kernels implement a two-pass mean/variance algorithm specifically
// designed for financial risk metrics. The central GPU statistics module
// (src/gpu/statistics/nimcp_statistics_kernels.cu) provides alternative
// implementations:
//
//   - kernel_mean_partial(): Multi-variable batch mean computation
//   - kernel_welford_stats(): Numerically stable single-pass mean/variance
//   - kernel_reduce_sum_sq(): Fused sum and sum-of-squares reduction
//
// The two-pass algorithm used here (mean first, then variance) was retained
// because:
// 1. Risk calculations often need the mean separately for VaR computation
// 2. The two-pass approach can be more numerically stable for certain
//    financial return distributions with extreme values
// 3. The kernel signatures are optimized for single-variable risk metrics
//
// For new development, consider using the central statistics module's
// nimcp_stats_gpu_variance_batch() for batch multi-variable operations.
//=============================================================================

/**
 * @brief Compute mean using two-pass algorithm (pass 1: sum reduction)
 *
 * @note Related: kernel_mean_partial() in nimcp_statistics_kernels.cu
 */
static __global__ void kernel_compute_mean(
    const float* __restrict__ data,
    float* __restrict__ partial_sums,
    uint32_t n)
{
    extern __shared__ float s_sum[];

    uint32_t tid = threadIdx.x;
    uint32_t i = blockIdx.x * blockDim.x * 2 + threadIdx.x;

    float sum = 0.0f;
    if (i < n) sum = data[i];
    if (i + blockDim.x < n) sum += data[i + blockDim.x];

    s_sum[tid] = sum;
    __syncthreads();

    for (uint32_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            s_sum[tid] += s_sum[tid + s];
        }
        __syncthreads();
    }

    if (tid == 0) {
        partial_sums[blockIdx.x] = s_sum[0];
    }
}

/**
 * @brief Compute variance given pre-computed mean (pass 2: squared deviation reduction)
 *
 * @note Related: kernel_welford_stats() provides single-pass alternative
 */
__global__ void kernel_compute_variance(
    const float* __restrict__ data,
    float mean,
    float* __restrict__ partial_sq_devs,
    uint32_t n)
{
    extern __shared__ float s_sq[];

    uint32_t tid = threadIdx.x;
    uint32_t i = blockIdx.x * blockDim.x * 2 + threadIdx.x;

    float sq_sum = 0.0f;
    if (i < n) {
        float diff = data[i] - mean;
        sq_sum = diff * diff;
    }
    if (i + blockDim.x < n) {
        float diff = data[i + blockDim.x] - mean;
        sq_sum += diff * diff;
    }

    s_sq[tid] = sq_sum;
    __syncthreads();

    for (uint32_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            s_sq[tid] += s_sq[tid + s];
        }
        __syncthreads();
    }

    if (tid == 0) {
        partial_sq_devs[blockIdx.x] = s_sq[0];
    }
}

/**
 * @brief Compute CVaR from sorted returns
 */
__global__ void kernel_compute_cvar(
    const float* __restrict__ sorted_returns,
    float* __restrict__ cvar,
    uint32_t n,
    float confidence)
{
    extern __shared__ float s_sum[];

    uint32_t tid = threadIdx.x;
    uint32_t cutoff = (uint32_t)((1.0f - confidence) * n);
    if (cutoff == 0) cutoff = 1;

    float sum = 0.0f;
    for (uint32_t i = tid; i < cutoff; i += blockDim.x) {
        sum += sorted_returns[i];
    }

    s_sum[tid] = sum;
    __syncthreads();

    for (uint32_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            s_sum[tid] += s_sum[tid + s];
        }
        __syncthreads();
    }

    if (tid == 0) {
        *cvar = s_sum[0] / (float)cutoff;
    }
}

//=============================================================================
// Volatility Estimation Kernels
//=============================================================================

/**
 * @brief Compute log returns from prices
 */
__global__ void kernel_compute_log_returns(
    const float* __restrict__ prices,
    float* __restrict__ returns,
    uint32_t n)
{
    uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n - 1) {
        returns[i] = logf(prices[i + 1] / prices[i]);
    }
}

/**
 * @brief EWMA volatility estimation
 *
 * sigma_t^2 = lambda * sigma_{t-1}^2 + (1 - lambda) * r_{t-1}^2
 */
__global__ void kernel_ewma_volatility(
    const float* __restrict__ returns,
    float* __restrict__ variance_out,
    float lambda,
    float initial_variance,
    uint32_t n)
{
    // Single thread processes sequentially (inherently serial)
    if (blockIdx.x == 0 && threadIdx.x == 0) {
        float variance = initial_variance;

        for (uint32_t i = 0; i < n; i++) {
            float r = returns[i];
            variance = lambda * variance + (1.0f - lambda) * r * r;
        }

        *variance_out = variance;
    }
}

/**
 * @brief Parkinson volatility estimator (using high-low range)
 *
 * sigma^2 = (1/4ln(2)) * (1/n) * sum(ln(H/L)^2)
 */
__global__ void kernel_parkinson_volatility(
    const float* __restrict__ high_prices,
    const float* __restrict__ low_prices,
    float* __restrict__ partial_sums,
    uint32_t n)
{
    extern __shared__ float s_sum[];

    uint32_t tid = threadIdx.x;
    uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;

    float sum = 0.0f;
    if (i < n && low_prices[i] > 0.0f) {
        float log_hl = logf(high_prices[i] / low_prices[i]);
        sum = log_hl * log_hl;
    }

    s_sum[tid] = sum;
    __syncthreads();

    for (uint32_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            s_sum[tid] += s_sum[tid + s];
        }
        __syncthreads();
    }

    if (tid == 0) {
        partial_sums[blockIdx.x] = s_sum[0];
    }
}

/**
 * @brief Garman-Klass volatility estimator
 *
 * Uses open, high, low, close prices for more efficient estimation
 */
__global__ void kernel_garman_klass_volatility(
    const float* __restrict__ open_prices,
    const float* __restrict__ high_prices,
    const float* __restrict__ low_prices,
    const float* __restrict__ close_prices,
    float* __restrict__ partial_sums,
    uint32_t n)
{
    extern __shared__ float s_sum[];

    uint32_t tid = threadIdx.x;
    uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;

    float sum = 0.0f;
    if (i < n && open_prices[i] > 0.0f && low_prices[i] > 0.0f) {
        float u = logf(high_prices[i] / open_prices[i]);
        float d = logf(low_prices[i] / open_prices[i]);
        float c = logf(close_prices[i] / open_prices[i]);

        // Garman-Klass formula
        sum = 0.5f * (u - d) * (u - d) - (2.0f * logf(2.0f) - 1.0f) * c * c;
    }

    s_sum[tid] = sum;
    __syncthreads();

    for (uint32_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            s_sum[tid] += s_sum[tid + s];
        }
        __syncthreads();
    }

    if (tid == 0) {
        partial_sums[blockIdx.x] = s_sum[0];
    }
}

/**
 * @brief Yang-Zhang volatility estimator
 */
__global__ void kernel_yang_zhang_components(
    const float* __restrict__ open_prices,
    const float* __restrict__ high_prices,
    const float* __restrict__ low_prices,
    const float* __restrict__ close_prices,
    float* __restrict__ partial_overnight,  // Close-to-open variance
    float* __restrict__ partial_open_close, // Open-to-close variance
    float* __restrict__ partial_rogers,     // Rogers-Satchell component
    uint32_t n)
{
    extern __shared__ float s_data[];
    float* s_overnight = s_data;
    float* s_oc = &s_data[blockDim.x];
    float* s_rs = &s_data[2 * blockDim.x];

    uint32_t tid = threadIdx.x;
    uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;

    float overnight = 0.0f;
    float oc = 0.0f;
    float rs = 0.0f;

    if (i > 0 && i < n) {
        // Overnight return
        float log_overnight = logf(open_prices[i] / close_prices[i - 1]);
        overnight = log_overnight * log_overnight;

        // Open to close
        float log_oc = logf(close_prices[i] / open_prices[i]);
        oc = log_oc * log_oc;

        // Rogers-Satchell
        float log_hi = logf(high_prices[i] / open_prices[i]);
        float log_lo = logf(low_prices[i] / open_prices[i]);
        float log_co = logf(close_prices[i] / open_prices[i]);
        rs = log_hi * (log_hi - log_co) + log_lo * (log_lo - log_co);
    }

    s_overnight[tid] = overnight;
    s_oc[tid] = oc;
    s_rs[tid] = rs;
    __syncthreads();

    for (uint32_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            s_overnight[tid] += s_overnight[tid + s];
            s_oc[tid] += s_oc[tid + s];
            s_rs[tid] += s_rs[tid + s];
        }
        __syncthreads();
    }

    if (tid == 0) {
        partial_overnight[blockIdx.x] = s_overnight[0];
        partial_open_close[blockIdx.x] = s_oc[0];
        partial_rogers[blockIdx.x] = s_rs[0];
    }
}

//=============================================================================
// Additional Statistical Kernels
//=============================================================================

/**
 * @brief Compute skewness given pre-computed mean and std_dev
 */
__global__ void kernel_compute_skewness(
    const float* __restrict__ data,
    float mean,
    float std_dev,
    float* __restrict__ partial_cubed,
    uint32_t n)
{
    extern __shared__ float s_cubed[];

    uint32_t tid = threadIdx.x;
    uint32_t i = blockIdx.x * blockDim.x * 2 + threadIdx.x;

    float cubed_sum = 0.0f;
    if (i < n && std_dev > 1e-10f) {
        float z = (data[i] - mean) / std_dev;
        cubed_sum = z * z * z;
    }
    if (i + blockDim.x < n && std_dev > 1e-10f) {
        float z = (data[i + blockDim.x] - mean) / std_dev;
        cubed_sum += z * z * z;
    }

    s_cubed[tid] = cubed_sum;
    __syncthreads();

    for (uint32_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            s_cubed[tid] += s_cubed[tid + s];
        }
        __syncthreads();
    }

    if (tid == 0) {
        partial_cubed[blockIdx.x] = s_cubed[0];
    }
}

/**
 * @brief Compute kurtosis given pre-computed mean and std_dev
 */
__global__ void kernel_compute_kurtosis(
    const float* __restrict__ data,
    float mean,
    float std_dev,
    float* __restrict__ partial_fourth,
    uint32_t n)
{
    extern __shared__ float s_fourth[];

    uint32_t tid = threadIdx.x;
    uint32_t i = blockIdx.x * blockDim.x * 2 + threadIdx.x;

    float fourth_sum = 0.0f;
    if (i < n && std_dev > 1e-10f) {
        float z = (data[i] - mean) / std_dev;
        float z2 = z * z;
        fourth_sum = z2 * z2;
    }
    if (i + blockDim.x < n && std_dev > 1e-10f) {
        float z = (data[i + blockDim.x] - mean) / std_dev;
        float z2 = z * z;
        fourth_sum += z2 * z2;
    }

    s_fourth[tid] = fourth_sum;
    __syncthreads();

    for (uint32_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            s_fourth[tid] += s_fourth[tid + s];
        }
        __syncthreads();
    }

    if (tid == 0) {
        partial_fourth[blockIdx.x] = s_fourth[0];
    }
}

/**
 * @brief Compute drawdown series from cumulative returns
 */
__global__ void kernel_compute_drawdown_series(
    const float* __restrict__ prices,
    float* __restrict__ drawdown,
    float* __restrict__ peak,
    uint32_t n)
{
    // This kernel computes parallel drawdowns
    // Each thread handles its position
    uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;

    if (i < n) {
        // Find peak up to this point (O(n) per thread, but parallelized)
        // For better performance, use parallel scan
        float max_so_far = prices[0];
        for (uint32_t j = 1; j <= i; j++) {
            if (prices[j] > max_so_far) {
                max_so_far = prices[j];
            }
        }
        if (peak) peak[i] = max_so_far;
        drawdown[i] = (max_so_far > 1e-10f) ? (prices[i] - max_so_far) / max_so_far : 0.0f;  // Negative if below peak
    }
}

/**
 * @brief Parallel prefix max for drawdown (Hillis-Steele style)
 */
__global__ void kernel_parallel_prefix_max(
    float* __restrict__ data,
    uint32_t n,
    uint32_t offset)
{
    uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;

    if (i >= offset && i < n) {
        float left = data[i - offset];
        float curr = data[i];
        data[i] = fmaxf(left, curr);
    }
}

/**
 * @brief Find max drawdown using reduction
 */
__global__ void kernel_find_max_drawdown(
    const float* __restrict__ drawdown,
    float* __restrict__ partial_min,
    uint32_t* __restrict__ partial_idx,
    uint32_t n)
{
    extern __shared__ float s_data[];
    uint32_t* s_idx = (uint32_t*)&s_data[blockDim.x];

    uint32_t tid = threadIdx.x;
    uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;

    s_data[tid] = (i < n) ? drawdown[i] : 0.0f;
    s_idx[tid] = (i < n) ? i : 0;
    __syncthreads();

    for (uint32_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            if (s_data[tid + s] < s_data[tid]) {
                s_data[tid] = s_data[tid + s];
                s_idx[tid] = s_idx[tid + s];
            }
        }
        __syncthreads();
    }

    if (tid == 0) {
        partial_min[blockIdx.x] = s_data[0];
        partial_idx[blockIdx.x] = s_idx[0];
    }
}

/**
 * @brief Compute EWMA volatility series (with output at each step)
 */
__global__ void kernel_ewma_volatility_series(
    const float* __restrict__ returns,
    float* __restrict__ variance_series,
    float lambda,
    float initial_variance,
    uint32_t n)
{
    // Single thread processes sequentially
    if (blockIdx.x == 0 && threadIdx.x == 0) {
        float variance = initial_variance;
        variance_series[0] = sqrtf(variance);

        for (uint32_t i = 0; i < n; i++) {
            float r = returns[i];
            variance = lambda * variance + (1.0f - lambda) * r * r;
            if (i + 1 < n) {
                variance_series[i + 1] = sqrtf(variance);
            }
        }
    }
}

/**
 * @brief Compute covariance matrix elements
 */
__global__ void kernel_compute_covariance_element(
    const float* __restrict__ returns,  // [num_assets x num_returns]
    const float* __restrict__ means,    // [num_assets]
    float* __restrict__ covariance,     // [num_assets x num_assets]
    uint32_t num_assets,
    uint32_t num_returns)
{
    uint32_t i = blockIdx.x;
    uint32_t j = blockIdx.y;

    if (i >= num_assets || j >= num_assets) return;

    extern __shared__ float s_sum[];

    uint32_t tid = threadIdx.x;
    float sum = 0.0f;

    float mean_i = means[i];
    float mean_j = means[j];

    for (uint32_t t = tid; t < num_returns; t += blockDim.x) {
        float ri = returns[i * num_returns + t] - mean_i;
        float rj = returns[j * num_returns + t] - mean_j;
        sum += ri * rj;
    }

    s_sum[tid] = sum;
    __syncthreads();

    for (uint32_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            s_sum[tid] += s_sum[tid + s];
        }
        __syncthreads();
    }

    if (tid == 0) {
        covariance[i * num_assets + j] = s_sum[0] / (float)(num_returns - 1);
    }
}

/**
 * @brief Compute asset means for covariance
 */
__global__ void kernel_compute_asset_means(
    const float* __restrict__ returns,  // [num_assets x num_returns]
    float* __restrict__ means,          // [num_assets]
    uint32_t num_assets,
    uint32_t num_returns)
{
    uint32_t asset = blockIdx.x;
    if (asset >= num_assets) return;

    extern __shared__ float s_sum[];

    uint32_t tid = threadIdx.x;
    float sum = 0.0f;

    for (uint32_t t = tid; t < num_returns; t += blockDim.x) {
        sum += returns[asset * num_returns + t];
    }

    s_sum[tid] = sum;
    __syncthreads();

    for (uint32_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            s_sum[tid] += s_sum[tid + s];
        }
        __syncthreads();
    }

    if (tid == 0) {
        means[asset] = s_sum[0] / (float)num_returns;
    }
}

/**
 * @brief Batch risk computation kernel
 * Each block processes one portfolio's returns
 */
__global__ void kernel_batch_risk_mean_var(
    const float* __restrict__ returns,   // [num_portfolios x num_returns]
    float* __restrict__ means,           // [num_portfolios]
    float* __restrict__ variances,       // [num_portfolios]
    uint32_t num_portfolios,
    uint32_t num_returns)
{
    extern __shared__ float s_data[];
    float* s_sum = s_data;
    float* s_sq = &s_data[blockDim.x];

    uint32_t portfolio = blockIdx.x;
    if (portfolio >= num_portfolios) return;

    uint32_t tid = threadIdx.x;
    const float* port_returns = &returns[portfolio * num_returns];

    // First pass: compute sum
    float sum = 0.0f;
    for (uint32_t i = tid; i < num_returns; i += blockDim.x) {
        sum += port_returns[i];
    }
    s_sum[tid] = sum;
    __syncthreads();

    for (uint32_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            s_sum[tid] += s_sum[tid + s];
        }
        __syncthreads();
    }

    float mean = s_sum[0] / (float)num_returns;
    if (tid == 0) means[portfolio] = mean;
    __syncthreads();

    // Second pass: compute variance
    float sq_sum = 0.0f;
    for (uint32_t i = tid; i < num_returns; i += blockDim.x) {
        float diff = port_returns[i] - mean;
        sq_sum += diff * diff;
    }
    s_sq[tid] = sq_sum;
    __syncthreads();

    for (uint32_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            s_sq[tid] += s_sq[tid + s];
        }
        __syncthreads();
    }

    if (tid == 0) {
        variances[portfolio] = s_sq[0] / (float)(num_returns - 1);
    }
}

/**
 * @brief Rolling CVaR computation kernel (proper computation, not approximation)
 */
__global__ void kernel_rolling_cvar(
    const float* __restrict__ returns,
    float* __restrict__ rolling_cvar,
    uint32_t window_size,
    float confidence,
    uint32_t n)
{
    // Note: This is a proper CVaR computation using sorting within each window
    // For very large windows, this is O(n * window_size * log(window_size))
    // but parallelizes across windows

    extern __shared__ float s_window[];

    uint32_t out_idx = blockIdx.x;
    uint32_t window_start = out_idx;

    if (out_idx + window_size > n) return;

    uint32_t tid = threadIdx.x;

    // Load window data into shared memory
    if (tid < window_size) {
        s_window[tid] = returns[window_start + tid];
    }
    __syncthreads();

    // Simple insertion sort in shared memory (good for small windows)
    // Thread 0 does the sorting
    if (tid == 0 && window_size <= blockDim.x) {
        for (uint32_t i = 1; i < window_size; i++) {
            float key = s_window[i];
            int j = i - 1;
            while (j >= 0 && s_window[j] > key) {
                s_window[j + 1] = s_window[j];
                j--;
            }
            s_window[j + 1] = key;
        }

        // Compute CVaR: average of returns below VaR threshold
        uint32_t cutoff = (uint32_t)((1.0f - confidence) * window_size);
        if (cutoff == 0) cutoff = 1;

        float sum = 0.0f;
        for (uint32_t i = 0; i < cutoff; i++) {
            sum += s_window[i];
        }
        rolling_cvar[out_idx] = -sum / (float)cutoff;  // Positive CVaR
    }
}

//=============================================================================
// Rolling Risk Metrics Kernels
//=============================================================================

/**
 * @brief Rolling volatility computation
 */
__global__ void kernel_rolling_volatility(
    const float* __restrict__ returns,
    float* __restrict__ rolling_vol,
    uint32_t window_size,
    uint32_t n)
{
    uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;

    if (i >= window_size - 1 && i < n) {
        // Compute mean for window
        float sum = 0.0f;
        for (uint32_t j = 0; j < window_size; j++) {
            sum += returns[i - window_size + 1 + j];
        }
        float mean = sum / (float)window_size;

        // Compute variance
        float sq_sum = 0.0f;
        for (uint32_t j = 0; j < window_size; j++) {
            float diff = returns[i - window_size + 1 + j] - mean;
            sq_sum += diff * diff;
        }
        float var = sq_sum / (float)(window_size - 1);

        rolling_vol[i] = sqrtf(var);
    } else if (i < n) {
        rolling_vol[i] = 0.0f;  // Not enough data
    }
}

/**
 * @brief Rolling VaR computation
 */
__global__ void kernel_rolling_var(
    const float* __restrict__ returns,
    float* __restrict__ rolling_var,
    uint32_t window_size,
    float confidence,
    uint32_t n)
{
    // Note: This is a simplified version using parametric VaR
    // For historical VaR, we'd need sorting which is complex in rolling windows

    uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;

    if (i >= window_size - 1 && i < n) {
        // Compute mean and std for window
        float sum = 0.0f;
        for (uint32_t j = 0; j < window_size; j++) {
            sum += returns[i - window_size + 1 + j];
        }
        float mean = sum / (float)window_size;

        float sq_sum = 0.0f;
        for (uint32_t j = 0; j < window_size; j++) {
            float diff = returns[i - window_size + 1 + j] - mean;
            sq_sum += diff * diff;
        }
        float std = sqrtf(sq_sum / (float)(window_size - 1));

        // Parametric VaR
        float z = norm_quantile_device(1.0f - confidence);
        rolling_var[i] = -(mean + z * std);
    } else if (i < n) {
        rolling_var[i] = 0.0f;
    }
}

//=============================================================================
// Host API Implementation
//=============================================================================

extern "C" {

/**
 * @brief GPU bitonic sort for arrays up to 2^20 elements
 */
// Helper to compute next power of 2
static uint32_t next_pow2(uint32_t v) {
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

static bool gpu_sort(float* d_data, uint32_t n, cudaStream_t stream) {
    // Bitonic sort requires power-of-2 array size
    uint32_t n_padded = next_pow2(n);
    if (n_padded < 2) n_padded = 2;

    // If n is not power of 2, we need to pad with FLT_MAX
    float* d_work = d_data;
    bool allocated_work = false;

    if (n_padded != n) {
        cudaError_t err = cudaMalloc(&d_work, n_padded * sizeof(float));
        if (err != cudaSuccess) {
            return false;
        }
        allocated_work = true;

        // Copy original data
        cudaMemcpy(d_work, d_data, n * sizeof(float), cudaMemcpyDeviceToDevice);

        // Fill padding with FLT_MAX to sort to end
        std::vector<float> padding(n_padded - n, FLT_MAX);
        cudaMemcpy(d_work + n, padding.data(), (n_padded - n) * sizeof(float), cudaMemcpyHostToDevice);
    }

    uint32_t block_size = 256;

    // Use shared memory sort for small arrays
    if (n_padded <= 512) {
        kernel_bitonic_sort_shared<<<1, n_padded / 2, n_padded * sizeof(float), stream>>>(
            d_work, n_padded);
    } else {
        // Full bitonic sort for larger arrays
        uint32_t grid_size = NIMCP_CUDA_GRID_SIZE(n_padded, block_size);

        for (uint32_t k = 2; k <= n_padded; k *= 2) {
            for (uint32_t j = k / 2; j > 0; j /= 2) {
                kernel_bitonic_sort_step<<<grid_size, block_size, 0, stream>>>(
                    d_work, j, k, n_padded);
            }
        }
    }

    // Copy sorted data back if we used a work buffer
    if (allocated_work) {
        cudaMemcpy(d_data, d_work, n * sizeof(float), cudaMemcpyDeviceToDevice);
        cudaFree(d_work);
    }

    return true;
}

bool fin_risk_gpu_compute(
    nimcp_gpu_context_t* ctx,
    const float* returns,
    const fin_risk_gpu_params_t* params,
    fin_risk_gpu_result_t* result)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !nimcp_gpu_context_is_valid(ctx)) {
        set_risk_error("Invalid GPU context");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid GPU context");
        return false;
    }
    if (!returns || !params || !result) {
        set_risk_error("Invalid parameters");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid parameters");
        return false;
    }
    if (params->num_returns == 0) {
        set_risk_error("Zero returns");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Zero returns");
        return false;
    }

    uint32_t n = params->num_returns;
    cudaStream_t stream = nimcp_gpu_get_compute_stream(ctx);
    uint32_t block_size = 256;
    uint32_t grid_size = NIMCP_CUDA_GRID_SIZE(n, block_size);
    uint32_t reduce_blocks = (n + block_size * 2 - 1) / (block_size * 2);

    // Create timing events
    cudaEvent_t start_event, end_event;
    cudaEventCreate(&start_event);
    cudaEventCreate(&end_event);
    cudaEventRecord(start_event, stream);

    // Allocate device memory
    float* d_returns = NULL;
    float* d_sorted = NULL;
    float* d_partial = NULL;

    cudaError_t err;

    err = cudaMalloc(&d_returns, n * sizeof(float));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t recovery_result = {0};
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &recovery_result)) {
            err = cudaMalloc(&d_returns, n * sizeof(float));
        }
        if (err != cudaSuccess) {
            cudaEventDestroy(start_event);
            cudaEventDestroy(end_event);
            set_risk_error("Failed to allocate returns");
            NIMCP_THROW_GPU(NIMCP_ERROR_GPU, 0, err, "Failed to allocate returns");
            return false;
        }
    }

    err = cudaMalloc(&d_sorted, n * sizeof(float));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t recovery_result = {0};
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &recovery_result)) {
            err = cudaMalloc(&d_sorted, n * sizeof(float));
        }
        if (err != cudaSuccess) {
            cudaEventDestroy(start_event);
            cudaEventDestroy(end_event);
            cudaFree(d_returns);
            set_risk_error("Failed to allocate sorted array");
            NIMCP_THROW_GPU(NIMCP_ERROR_GPU, 0, err, "Failed to allocate sorted array");
            return false;
        }
    }

    err = cudaMalloc(&d_partial, reduce_blocks * sizeof(float));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t recovery_result = {0};
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &recovery_result)) {
            err = cudaMalloc(&d_partial, reduce_blocks * sizeof(float));
        }
        if (err != cudaSuccess) {
            cudaEventDestroy(start_event);
            cudaEventDestroy(end_event);
            cudaFree(d_returns);
            cudaFree(d_sorted);
            set_risk_error("Failed to allocate partial sums");
            NIMCP_THROW_GPU(NIMCP_ERROR_GPU, 0, err, "Failed to allocate partial sums");
            return false;
        }
    }

    // Copy returns to device
    cudaMemcpyAsync(d_returns, returns, n * sizeof(float),
                    cudaMemcpyHostToDevice, stream);

    // Compute mean
    kernel_compute_mean<<<reduce_blocks, block_size, block_size * sizeof(float), stream>>>(
        d_returns, d_partial, n);

    float* h_partial = (float*)nimcp_malloc(reduce_blocks * sizeof(float));
    if (!h_partial) {
        cudaEventDestroy(start_event);
        cudaEventDestroy(end_event);
        cudaFree(d_returns);
        cudaFree(d_sorted);
        cudaFree(d_partial);
        set_risk_error("Failed to allocate h_partial");
        return false;
    }
    cudaMemcpy(h_partial, d_partial, reduce_blocks * sizeof(float), cudaMemcpyDeviceToHost);

    float sum = 0.0f;
    for (uint32_t i = 0; i < reduce_blocks; i++) {
        sum += h_partial[i];
    }
    float mean = sum / (float)n;

    // Compute variance
    kernel_compute_variance<<<reduce_blocks, block_size, block_size * sizeof(float), stream>>>(
        d_returns, mean, d_partial, n);

    cudaMemcpy(h_partial, d_partial, reduce_blocks * sizeof(float), cudaMemcpyDeviceToHost);

    float sq_sum = 0.0f;
    for (uint32_t i = 0; i < reduce_blocks; i++) {
        sq_sum += h_partial[i];
    }
    float variance = sq_sum / (float)(n - 1);
    float std_dev = sqrtf(variance);

    // Sort returns for VaR/CVaR
    cudaMemcpyAsync(d_sorted, d_returns, n * sizeof(float),
                    cudaMemcpyDeviceToDevice, stream);
    gpu_sort(d_sorted, n, stream);

    // Compute Historical VaR (percentile of sorted returns)
    uint32_t var_index = (uint32_t)((1.0f - params->confidence_level) * n);
    if (var_index >= n) var_index = n - 1;

    float h_var;
    cudaMemcpy(&h_var, &d_sorted[var_index], sizeof(float), cudaMemcpyDeviceToHost);

    // Compute CVaR (average of worst returns)
    float* d_cvar = NULL;
    if (cudaMalloc(&d_cvar, sizeof(float)) != cudaSuccess) {
        nimcp_free(h_partial);
        cudaEventDestroy(start_event);
        cudaEventDestroy(end_event);
        cudaFree(d_returns);
        cudaFree(d_sorted);
        cudaFree(d_partial);
        set_risk_error("Failed to allocate d_cvar");
        return false;
    }

    kernel_compute_cvar<<<1, block_size, block_size * sizeof(float), stream>>>(
        d_sorted, d_cvar, n, params->confidence_level);

    float h_cvar;
    cudaMemcpy(&h_cvar, d_cvar, sizeof(float), cudaMemcpyDeviceToHost);

    cudaStreamSynchronize(stream);

    // Fill result
    result->var = -h_var;  // VaR is typically reported as positive
    result->cvar = -h_cvar;
    result->volatility = std_dev;
    result->mean_return = mean;
    result->max_drawdown = 0.0f;  // TODO: Compute max drawdown

    // Compute 95% VaR and CVaR
    uint32_t var_95_idx = (uint32_t)(0.05f * n);
    if (var_95_idx >= n) var_95_idx = n - 1;
    float h_var_95;
    cudaMemcpy(&h_var_95, &d_sorted[var_95_idx], sizeof(float), cudaMemcpyDeviceToHost);
    result->var_95 = -h_var_95;

    // Compute 95% CVaR (average of worst 5%)
    float sum_cvar_95 = 0.0f;
    uint32_t num_cvar_95 = var_95_idx + 1;
    if (num_cvar_95 > 0) {
        std::vector<float> h_tail(num_cvar_95);
        cudaMemcpy(h_tail.data(), d_sorted, num_cvar_95 * sizeof(float), cudaMemcpyDeviceToHost);
        for (uint32_t i = 0; i < num_cvar_95; i++) {
            sum_cvar_95 += h_tail[i];
        }
        result->cvar_95 = -sum_cvar_95 / num_cvar_95;
    } else {
        result->cvar_95 = result->var_95;
    }

    // Compute 99% VaR and CVaR
    uint32_t var_99_idx = (uint32_t)(0.01f * n);
    if (var_99_idx >= n) var_99_idx = n - 1;
    float h_var_99;
    cudaMemcpy(&h_var_99, &d_sorted[var_99_idx], sizeof(float), cudaMemcpyDeviceToHost);
    result->var_99 = -h_var_99;

    // Compute 99% CVaR (average of worst 1%)
    float sum_cvar_99 = 0.0f;
    uint32_t num_cvar_99 = var_99_idx + 1;
    if (num_cvar_99 > 0) {
        std::vector<float> h_tail99(num_cvar_99);
        cudaMemcpy(h_tail99.data(), d_sorted, num_cvar_99 * sizeof(float), cudaMemcpyDeviceToHost);
        for (uint32_t i = 0; i < num_cvar_99; i++) {
            sum_cvar_99 += h_tail99[i];
        }
        result->cvar_99 = -sum_cvar_99 / num_cvar_99;
    } else {
        result->cvar_99 = result->var_99;
    }

    // Compute parametric VaR for comparison
    float z = -2.326f;  // 99% confidence
    if (params->confidence_level < 0.99f) {
        z = -1.645f;  // 95% confidence
    }
    result->var_parametric = -(mean + z * std_dev);

    // Also compute annualized volatility (assuming daily returns)
    result->volatility_daily = std_dev;
    result->volatility_annual = std_dev * sqrtf(252.0f);  // Assuming 252 trading days

    // Compute Sharpe ratio using risk_free_rate from params
    float daily_rf = params->risk_free_rate / 252.0f;
    float excess_mean = mean - daily_rf;
    result->sharpe_ratio = (std_dev > 1e-10f) ?
        (excess_mean / std_dev) * sqrtf(252.0f) : 0.0f;

    // Compute Sortino ratio (downside deviation)
    float* h_returns = (float*)nimcp_malloc(n * sizeof(float));
    if (!h_returns) {
        // Fall back to using std_dev for downside deviation
        result->downside_deviation = std_dev;
        result->sortino_ratio = (std_dev > 1e-10f) ?
            (excess_mean / std_dev) * sqrtf(252.0f) : 0.0f;
    } else {
        cudaMemcpy(h_returns, d_returns, n * sizeof(float), cudaMemcpyDeviceToHost);

        float downside_sum = 0.0f;
        uint32_t downside_count = 0;
        for (uint32_t i = 0; i < n; i++) {
            if (h_returns[i] < daily_rf) {
                float diff = h_returns[i] - daily_rf;
                downside_sum += diff * diff;
                downside_count++;
            }
        }
        float downside_dev = (downside_count > 0) ? sqrtf(downside_sum / downside_count) : std_dev;
        result->downside_deviation = downside_dev;
        result->sortino_ratio = (downside_dev > 1e-10f) ?
            (excess_mean / downside_dev) * sqrtf(252.0f) : 0.0f;

        nimcp_free(h_returns);
    }

    // Record timing
    cudaEventRecord(end_event, stream);
    cudaEventSynchronize(end_event);
    float elapsed_ms = 0.0f;
    cudaEventElapsedTime(&elapsed_ms, start_event, end_event);
    result->kernel_time_ms = elapsed_ms;
    cudaEventDestroy(start_event);
    cudaEventDestroy(end_event);

    // Cleanup
    nimcp_free(h_partial);
    cudaFree(d_returns);
    cudaFree(d_sorted);
    cudaFree(d_partial);
    cudaFree(d_cvar);

    return true;
}

bool fin_risk_gpu_volatility(
    nimcp_gpu_context_t* ctx,
    const float* prices,
    uint32_t n,
    fin_vol_method_t method,
    float* volatility)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !nimcp_gpu_context_is_valid(ctx)) {
        set_risk_error("Invalid GPU context");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid GPU context");
        return false;
    }
    if (!prices || n < 2 || !volatility) {
        set_risk_error("Invalid parameters");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid parameters");
        return false;
    }

    cudaStream_t stream = nimcp_gpu_get_compute_stream(ctx);
    uint32_t block_size = 256;
    uint32_t grid_size = NIMCP_CUDA_GRID_SIZE(n - 1, block_size);
    uint32_t reduce_blocks = (n + block_size - 1) / block_size;

    float* d_prices = NULL;
    float* d_returns = NULL;
    float* d_partial = NULL;
    float result_var = 0.0f;

    cudaError_t err;

    err = cudaMalloc(&d_prices, n * sizeof(float));
    if (err != cudaSuccess) goto cleanup_vol;

    err = cudaMalloc(&d_returns, (n - 1) * sizeof(float));
    if (err != cudaSuccess) goto cleanup_vol;

    err = cudaMalloc(&d_partial, reduce_blocks * sizeof(float));
    if (err != cudaSuccess) goto cleanup_vol;

    cudaMemcpyAsync(d_prices, prices, n * sizeof(float),
                    cudaMemcpyHostToDevice, stream);

    // Compute log returns
    kernel_compute_log_returns<<<grid_size, block_size, 0, stream>>>(
        d_prices, d_returns, n);

    switch (method) {
        case FIN_VOL_SIMPLE: {
            // Simple historical volatility
            uint32_t ret_n = n - 1;
            uint32_t ret_reduce = (ret_n + block_size * 2 - 1) / (block_size * 2);

            kernel_compute_mean<<<ret_reduce, block_size, block_size * sizeof(float), stream>>>(
                d_returns, d_partial, ret_n);

            float* h_partial = (float*)nimcp_malloc(ret_reduce * sizeof(float));
            if (!h_partial) {
                set_risk_error("Failed to allocate h_partial");
                goto cleanup_vol;
            }
            cudaMemcpy(h_partial, d_partial, ret_reduce * sizeof(float), cudaMemcpyDeviceToHost);

            float sum = 0.0f;
            for (uint32_t i = 0; i < ret_reduce; i++) sum += h_partial[i];
            float mean = sum / (float)ret_n;

            kernel_compute_variance<<<ret_reduce, block_size, block_size * sizeof(float), stream>>>(
                d_returns, mean, d_partial, ret_n);

            cudaMemcpy(h_partial, d_partial, ret_reduce * sizeof(float), cudaMemcpyDeviceToHost);

            float sq_sum = 0.0f;
            for (uint32_t i = 0; i < ret_reduce; i++) sq_sum += h_partial[i];
            result_var = sq_sum / (float)(ret_n - 1);

            nimcp_free(h_partial);
            break;
        }

        case FIN_VOL_EWMA: {
            float* d_var_out = NULL;
            if (cudaMalloc(&d_var_out, sizeof(float)) != cudaSuccess) {
                set_risk_error("Failed to allocate d_var_out");
                goto cleanup_vol;
            }

            float lambda = 0.94f;  // RiskMetrics default
            float init_var = 0.0004f;  // Initial variance guess (2% daily vol)

            kernel_ewma_volatility<<<1, 1, 0, stream>>>(
                d_returns, d_var_out, lambda, init_var, n - 1);

            cudaMemcpy(&result_var, d_var_out, sizeof(float), cudaMemcpyDeviceToHost);
            cudaFree(d_var_out);
            break;
        }

        case FIN_VOL_PARKINSON:
        case FIN_VOL_GARMAN_KLASS:
        case FIN_VOL_YANG_ZHANG:
            // These require OHLC data, not just prices
            set_risk_error("Method requires OHLC data - use fin_risk_gpu_volatility_ohlc");
            goto cleanup_vol;

        default:
            set_risk_error("Unknown volatility method");
            goto cleanup_vol;
    }

    *volatility = sqrtf(result_var);

    cudaFree(d_prices);
    cudaFree(d_returns);
    cudaFree(d_partial);
    return true;

cleanup_vol:
    cudaFree(d_prices);
    cudaFree(d_returns);
    cudaFree(d_partial);
    return false;
}

bool fin_risk_gpu_volatility_ohlc(
    nimcp_gpu_context_t* ctx,
    const float* open_prices,
    const float* high_prices,
    const float* low_prices,
    const float* close_prices,
    uint32_t n,
    fin_vol_method_t method,
    float* volatility)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !nimcp_gpu_context_is_valid(ctx)) {
        set_risk_error("Invalid GPU context");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid GPU context");
        return false;
    }
    if (!open_prices || !high_prices || !low_prices || !close_prices || n < 2) {
        set_risk_error("Invalid OHLC data");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid OHLC data");
        return false;
    }

    cudaStream_t stream = nimcp_gpu_get_compute_stream(ctx);
    uint32_t block_size = 256;
    uint32_t grid_size = NIMCP_CUDA_GRID_SIZE(n, block_size);
    uint32_t reduce_blocks = (n + block_size - 1) / block_size;

    float* d_open = NULL;
    float* d_high = NULL;
    float* d_low = NULL;
    float* d_close = NULL;
    float* d_partial = NULL;
    float result_var = 0.0f;
    float* h_partial = NULL;

    cudaError_t err;

    err = cudaMalloc(&d_open, n * sizeof(float));
    if (err != cudaSuccess) goto cleanup_ohlc;
    err = cudaMalloc(&d_high, n * sizeof(float));
    if (err != cudaSuccess) goto cleanup_ohlc;
    err = cudaMalloc(&d_low, n * sizeof(float));
    if (err != cudaSuccess) goto cleanup_ohlc;
    err = cudaMalloc(&d_close, n * sizeof(float));
    if (err != cudaSuccess) goto cleanup_ohlc;
    err = cudaMalloc(&d_partial, reduce_blocks * sizeof(float));
    if (err != cudaSuccess) goto cleanup_ohlc;

    h_partial = (float*)nimcp_malloc(reduce_blocks * sizeof(float));
    if (!h_partial) goto cleanup_ohlc;

    cudaMemcpyAsync(d_open, open_prices, n * sizeof(float), cudaMemcpyHostToDevice, stream);
    cudaMemcpyAsync(d_high, high_prices, n * sizeof(float), cudaMemcpyHostToDevice, stream);
    cudaMemcpyAsync(d_low, low_prices, n * sizeof(float), cudaMemcpyHostToDevice, stream);
    cudaMemcpyAsync(d_close, close_prices, n * sizeof(float), cudaMemcpyHostToDevice, stream);

    switch (method) {
        case FIN_VOL_PARKINSON:
            kernel_parkinson_volatility<<<reduce_blocks, block_size,
                                          block_size * sizeof(float), stream>>>(
                d_high, d_low, d_partial, n);

            cudaMemcpy(h_partial, d_partial, reduce_blocks * sizeof(float), cudaMemcpyDeviceToHost);
            {
                float sum = 0.0f;
                for (uint32_t i = 0; i < reduce_blocks; i++) sum += h_partial[i];
                result_var = sum / ((float)n * 4.0f * logf(2.0f));
            }
            break;

        case FIN_VOL_GARMAN_KLASS:
            kernel_garman_klass_volatility<<<reduce_blocks, block_size,
                                             block_size * sizeof(float), stream>>>(
                d_open, d_high, d_low, d_close, d_partial, n);

            cudaMemcpy(h_partial, d_partial, reduce_blocks * sizeof(float), cudaMemcpyDeviceToHost);
            {
                float sum = 0.0f;
                for (uint32_t i = 0; i < reduce_blocks; i++) sum += h_partial[i];
                result_var = sum / (float)n;
            }
            break;

        case FIN_VOL_YANG_ZHANG: {
            float* d_overnight = NULL;
            float* d_oc = NULL;
            float* d_rs = NULL;

            if (cudaMalloc(&d_overnight, reduce_blocks * sizeof(float)) != cudaSuccess ||
                cudaMalloc(&d_oc, reduce_blocks * sizeof(float)) != cudaSuccess ||
                cudaMalloc(&d_rs, reduce_blocks * sizeof(float)) != cudaSuccess) {
                cudaFree(d_overnight);
                cudaFree(d_oc);
                cudaFree(d_rs);
                set_risk_error("Failed to allocate Yang-Zhang GPU buffers");
                nimcp_free(h_partial);
                goto cleanup_ohlc;
            }

            kernel_yang_zhang_components<<<reduce_blocks, block_size,
                                           3 * block_size * sizeof(float), stream>>>(
                d_open, d_high, d_low, d_close, d_overnight, d_oc, d_rs, n);

            float* h_overnight = (float*)nimcp_malloc(reduce_blocks * sizeof(float));
            float* h_oc = (float*)nimcp_malloc(reduce_blocks * sizeof(float));
            float* h_rs = (float*)nimcp_malloc(reduce_blocks * sizeof(float));

            if (!h_overnight || !h_oc || !h_rs) {
                nimcp_free(h_overnight);
                nimcp_free(h_oc);
                nimcp_free(h_rs);
                cudaFree(d_overnight);
                cudaFree(d_oc);
                cudaFree(d_rs);
                set_risk_error("Failed to allocate Yang-Zhang host buffers");
                nimcp_free(h_partial);
                goto cleanup_ohlc;
            }

            cudaMemcpy(h_overnight, d_overnight, reduce_blocks * sizeof(float), cudaMemcpyDeviceToHost);
            cudaMemcpy(h_oc, d_oc, reduce_blocks * sizeof(float), cudaMemcpyDeviceToHost);
            cudaMemcpy(h_rs, d_rs, reduce_blocks * sizeof(float), cudaMemcpyDeviceToHost);

            float sum_overnight = 0.0f, sum_oc = 0.0f, sum_rs = 0.0f;
            for (uint32_t i = 0; i < reduce_blocks; i++) {
                sum_overnight += h_overnight[i];
                sum_oc += h_oc[i];
                sum_rs += h_rs[i];
            }

            float k = 0.34f / (1.34f + (float)(n + 1) / (float)(n - 1));
            float var_overnight = sum_overnight / (float)(n - 1);
            float var_oc = sum_oc / (float)(n - 1);
            float var_rs = sum_rs / (float)(n - 1);

            result_var = var_overnight + k * var_oc + (1.0f - k) * var_rs;

            nimcp_free(h_overnight);
            nimcp_free(h_oc);
            nimcp_free(h_rs);
            cudaFree(d_overnight);
            cudaFree(d_oc);
            cudaFree(d_rs);
            break;
        }

        default:
            set_risk_error("Invalid method for OHLC volatility");
            nimcp_free(h_partial);
            goto cleanup_ohlc;
    }

    nimcp_free(h_partial);
    *volatility = sqrtf(result_var);

    cudaFree(d_open);
    cudaFree(d_high);
    cudaFree(d_low);
    cudaFree(d_close);
    cudaFree(d_partial);
    return true;

cleanup_ohlc:
    cudaFree(d_open);
    cudaFree(d_high);
    cudaFree(d_low);
    cudaFree(d_close);
    cudaFree(d_partial);
    return false;
}

// Internal helper for simple rolling calculations
static bool fin_risk_gpu_rolling_simple(
    nimcp_gpu_context_t* ctx,
    const float* returns,
    uint32_t n,
    uint32_t window_size,
    float confidence,
    float* rolling_var,
    float* rolling_vol)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !nimcp_gpu_context_is_valid(ctx)) {
        set_risk_error("Invalid GPU context");
        return false;
    }
    if (!returns || n == 0 || window_size == 0 || window_size > n) {
        set_risk_error("Invalid parameters");
        return false;
    }

    cudaStream_t stream = nimcp_gpu_get_compute_stream(ctx);
    uint32_t block_size = 256;
    uint32_t grid_size = NIMCP_CUDA_GRID_SIZE(n, block_size);

    float* d_returns = NULL;
    float* d_rolling_var = NULL;
    float* d_rolling_vol = NULL;

    cudaError_t err;

    err = cudaMalloc(&d_returns, n * sizeof(float));
    if (err != cudaSuccess) goto cleanup_rolling_simple;

    if (rolling_var) {
        err = cudaMalloc(&d_rolling_var, n * sizeof(float));
        if (err != cudaSuccess) goto cleanup_rolling_simple;
    }

    if (rolling_vol) {
        err = cudaMalloc(&d_rolling_vol, n * sizeof(float));
        if (err != cudaSuccess) goto cleanup_rolling_simple;
    }

    cudaMemcpyAsync(d_returns, returns, n * sizeof(float),
                    cudaMemcpyHostToDevice, stream);

    if (rolling_vol) {
        kernel_rolling_volatility<<<grid_size, block_size, 0, stream>>>(
            d_returns, d_rolling_vol, window_size, n);

        cudaMemcpy(rolling_vol, d_rolling_vol, n * sizeof(float),
                   cudaMemcpyDeviceToHost);
    }

    if (rolling_var) {
        kernel_rolling_var<<<grid_size, block_size, 0, stream>>>(
            d_returns, d_rolling_var, window_size, confidence, n);

        cudaMemcpy(rolling_var, d_rolling_var, n * sizeof(float),
                   cudaMemcpyDeviceToHost);
    }

    cudaStreamSynchronize(stream);

    cudaFree(d_returns);
    cudaFree(d_rolling_var);
    cudaFree(d_rolling_vol);
    return true;

cleanup_rolling_simple:
    cudaFree(d_returns);
    cudaFree(d_rolling_var);
    cudaFree(d_rolling_vol);
    set_risk_error("Memory allocation failed");
    return false;
}

bool fin_risk_gpu_rolling(
    nimcp_gpu_context_t* ctx,
    const float* returns,
    uint32_t num_returns,
    uint32_t window,
    const fin_risk_gpu_params_t* params,
    fin_risk_rolling_result_t* result)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !nimcp_gpu_context_is_valid(ctx)) {
        set_risk_error("Invalid GPU context");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid GPU context");
        return false;
    }
    if (!returns || num_returns == 0 || window == 0 || window > num_returns) {
        set_risk_error("Invalid parameters");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid parameters");
        return false;
    }
    if (!params || !result) {
        set_risk_error("Null parameters or result");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Null parameters or result");
        return false;
    }

    uint32_t num_points = num_returns - window + 1;
    result->num_points = num_points;

    // Allocate output arrays if needed
    if (params->compute_var && !result->var_series) {
        result->var_series = (float*)nimcp_malloc(num_points * sizeof(float));
        if (!result->var_series) {
            set_risk_error("Failed to allocate var_series");
            return false;
        }
    }
    if (params->compute_cvar && !result->cvar_series) {
        result->cvar_series = (float*)nimcp_malloc(num_points * sizeof(float));
        if (!result->cvar_series) {
            set_risk_error("Failed to allocate cvar_series");
            return false;
        }
    }
    if (params->compute_volatility && !result->vol_series) {
        result->vol_series = (float*)nimcp_malloc(num_points * sizeof(float));
        if (!result->vol_series) {
            set_risk_error("Failed to allocate vol_series");
            return false;
        }
    }

    // Use internal helper for volatility
    if (params->compute_volatility && result->vol_series) {
        fin_risk_gpu_rolling_simple(ctx, returns, num_returns, window,
            params->confidence_95, NULL, result->vol_series);
    }

    // Use internal helper for VaR
    if (params->compute_var && result->var_series) {
        fin_risk_gpu_rolling_simple(ctx, returns, num_returns, window,
            params->confidence_95, result->var_series, NULL);
    }

    // CVaR computation - properly compute using sorted windows
    if (params->compute_cvar && result->cvar_series) {
        cudaStream_t stream = nimcp_gpu_get_compute_stream(ctx);

        float* d_returns_cvar = NULL;
        float* d_cvar_out = NULL;

        cudaError_t err = cudaMalloc(&d_returns_cvar, num_returns * sizeof(float));
        if (err != cudaSuccess) {
            // Fall back to VaR approximation
            if (result->var_series) {
                memcpy(result->cvar_series, result->var_series, num_points * sizeof(float));
            }
        } else {
            err = cudaMalloc(&d_cvar_out, num_points * sizeof(float));
            if (err != cudaSuccess) {
                cudaFree(d_returns_cvar);
                if (result->var_series) {
                    memcpy(result->cvar_series, result->var_series, num_points * sizeof(float));
                }
            } else {
                cudaMemcpyAsync(d_returns_cvar, returns, num_returns * sizeof(float),
                                cudaMemcpyHostToDevice, stream);

                // Launch rolling CVaR kernel
                // Each block processes one window; use shared memory for sorting
                uint32_t cvar_block_size = (window <= 256) ? 256 : 512;
                size_t shared_size = window * sizeof(float);

                kernel_rolling_cvar<<<num_points, cvar_block_size, shared_size, stream>>>(
                    d_returns_cvar, d_cvar_out, window, params->confidence_95, num_returns);

                cudaMemcpy(result->cvar_series, d_cvar_out, num_points * sizeof(float),
                           cudaMemcpyDeviceToHost);

                cudaFree(d_returns_cvar);
                cudaFree(d_cvar_out);
            }
        }
    }

    return true;
}

//=============================================================================
// Extended Risk API Implementation
//=============================================================================

/**
 * @brief Helper to compute mean, variance, skewness, kurtosis on GPU
 */
static bool compute_moments_gpu(
    nimcp_gpu_context_t* ctx,
    const float* d_returns,
    uint32_t n,
    float* out_mean,
    float* out_variance,
    float* out_skewness,
    float* out_kurtosis)
{
    cudaStream_t stream = nimcp_gpu_get_compute_stream(ctx);
    uint32_t block_size = 256;
    uint32_t reduce_blocks = (n + block_size * 2 - 1) / (block_size * 2);

    float* d_partial = NULL;
    float* h_partial = NULL;

    cudaError_t err = cudaMalloc(&d_partial, reduce_blocks * sizeof(float));
    if (err != cudaSuccess) return false;

    h_partial = (float*)nimcp_malloc(reduce_blocks * sizeof(float));
    if (!h_partial) {
        cudaFree(d_partial);
        return false;
    }

    // Compute mean
    kernel_compute_mean<<<reduce_blocks, block_size, block_size * sizeof(float), stream>>>(
        d_returns, d_partial, n);
    cudaMemcpy(h_partial, d_partial, reduce_blocks * sizeof(float), cudaMemcpyDeviceToHost);

    float sum = 0.0f;
    for (uint32_t i = 0; i < reduce_blocks; i++) sum += h_partial[i];
    float mean = sum / (float)n;
    *out_mean = mean;

    // Compute variance
    kernel_compute_variance<<<reduce_blocks, block_size, block_size * sizeof(float), stream>>>(
        d_returns, mean, d_partial, n);
    cudaMemcpy(h_partial, d_partial, reduce_blocks * sizeof(float), cudaMemcpyDeviceToHost);

    float sq_sum = 0.0f;
    for (uint32_t i = 0; i < reduce_blocks; i++) sq_sum += h_partial[i];
    float variance = sq_sum / (float)(n - 1);
    float std_dev = sqrtf(variance);
    *out_variance = variance;

    // Compute skewness
    if (out_skewness && std_dev > 1e-10f) {
        kernel_compute_skewness<<<reduce_blocks, block_size, block_size * sizeof(float), stream>>>(
            d_returns, mean, std_dev, d_partial, n);
        cudaMemcpy(h_partial, d_partial, reduce_blocks * sizeof(float), cudaMemcpyDeviceToHost);

        float cubed_sum = 0.0f;
        for (uint32_t i = 0; i < reduce_blocks; i++) cubed_sum += h_partial[i];
        *out_skewness = cubed_sum / (float)n;
    } else if (out_skewness) {
        *out_skewness = 0.0f;
    }

    // Compute kurtosis
    if (out_kurtosis && std_dev > 1e-10f) {
        kernel_compute_kurtosis<<<reduce_blocks, block_size, block_size * sizeof(float), stream>>>(
            d_returns, mean, std_dev, d_partial, n);
        cudaMemcpy(h_partial, d_partial, reduce_blocks * sizeof(float), cudaMemcpyDeviceToHost);

        float fourth_sum = 0.0f;
        for (uint32_t i = 0; i < reduce_blocks; i++) fourth_sum += h_partial[i];
        *out_kurtosis = fourth_sum / (float)n - 3.0f;  // Excess kurtosis
    } else if (out_kurtosis) {
        *out_kurtosis = 0.0f;
    }

    nimcp_free(h_partial);
    cudaFree(d_partial);
    return true;
}

bool fin_risk_gpu_extended(
    nimcp_gpu_context_t* ctx,
    const float* returns,
    uint32_t num_returns,
    const fin_risk_extended_params_t* params,
    fin_risk_extended_result_t* result)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !nimcp_gpu_context_is_valid(ctx)) {
        set_risk_error("Invalid GPU context");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid GPU context");
        return false;
    }
    if (!returns || !params || !result || num_returns < 2) {
        set_risk_error("Invalid parameters");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid parameters");
        return false;
    }

    memset(result, 0, sizeof(*result));

    cudaStream_t stream = nimcp_gpu_get_compute_stream(ctx);
    uint32_t n = num_returns;
    uint32_t block_size = 256;

    // Allocate device memory
    float* d_returns = NULL;
    float* d_sorted = NULL;

    cudaError_t err;
    err = cudaMalloc(&d_returns, n * sizeof(float));
    if (err != cudaSuccess) {
        set_risk_error("Failed to allocate device memory");
        return false;
    }

    err = cudaMalloc(&d_sorted, n * sizeof(float));
    if (err != cudaSuccess) {
        cudaFree(d_returns);
        set_risk_error("Failed to allocate sorted array");
        return false;
    }

    cudaMemcpyAsync(d_returns, returns, n * sizeof(float), cudaMemcpyHostToDevice, stream);

    // Compute moments
    float mean, variance, skewness, kurtosis;
    if (!compute_moments_gpu(ctx, d_returns, n, &mean, &variance, &skewness, &kurtosis)) {
        cudaFree(d_returns);
        cudaFree(d_sorted);
        return false;
    }

    float std_dev = sqrtf(variance);
    result->base.mean_return = mean;
    result->base.volatility = std_dev;

    // Sort for VaR/CVaR
    cudaMemcpyAsync(d_sorted, d_returns, n * sizeof(float), cudaMemcpyDeviceToDevice, stream);
    gpu_sort(d_sorted, n, stream);

    // Historical VaR
    uint32_t var_idx_95 = (uint32_t)((1.0f - 0.95f) * n);
    uint32_t var_idx_99 = (uint32_t)((1.0f - 0.99f) * n);
    if (var_idx_95 >= n) var_idx_95 = n - 1;
    if (var_idx_99 >= n) var_idx_99 = n - 1;

    float h_var_95, h_var_99;
    cudaMemcpy(&h_var_95, &d_sorted[var_idx_95], sizeof(float), cudaMemcpyDeviceToHost);
    cudaMemcpy(&h_var_99, &d_sorted[var_idx_99], sizeof(float), cudaMemcpyDeviceToHost);

    result->var_historical = -h_var_95;
    result->base.var_95 = -h_var_95;
    result->base.var_99 = -h_var_99;

    // Parametric VaR (assumes normal distribution)
    float z_95 = -1.645f;
    float z_99 = -2.326f;
    result->var_parametric = -(mean + z_95 * std_dev);
    result->base.var_parametric = result->var_parametric;

    // Cornish-Fisher VaR (adjusts for skewness and kurtosis)
    float z_cf_95 = z_95 + (skewness / 6.0f) * (z_95 * z_95 - 1.0f)
                   + (kurtosis / 24.0f) * (z_95 * z_95 * z_95 - 3.0f * z_95)
                   - (skewness * skewness / 36.0f) * (2.0f * z_95 * z_95 * z_95 - 5.0f * z_95);
    result->var_cornish_fisher = -(mean + z_cf_95 * std_dev);

    // CVaR computation
    float* d_cvar = NULL;
    if (cudaMalloc(&d_cvar, sizeof(float)) != cudaSuccess) {
        result->base.cvar_95 = result->base.var_95;
        result->base.cvar_99 = result->base.var_99;
    } else {
        kernel_compute_cvar<<<1, block_size, block_size * sizeof(float), stream>>>(
            d_sorted, d_cvar, n, 0.95f);
        float h_cvar_95;
        cudaMemcpy(&h_cvar_95, d_cvar, sizeof(float), cudaMemcpyDeviceToHost);
        result->base.cvar_95 = -h_cvar_95;

        kernel_compute_cvar<<<1, block_size, block_size * sizeof(float), stream>>>(
            d_sorted, d_cvar, n, 0.99f);
        float h_cvar_99;
        cudaMemcpy(&h_cvar_99, d_cvar, sizeof(float), cudaMemcpyDeviceToHost);
        result->base.cvar_99 = -h_cvar_99;

        cudaFree(d_cvar);
    }

    // EWMA volatility
    if (params->vol_method == FIN_VOL_EWMA) {
        float* d_var_out = NULL;
        if (cudaMalloc(&d_var_out, sizeof(float)) != cudaSuccess) {
            result->vol_ewma = std_dev;
        } else {
            float init_var = variance;  // Use sample variance as initial
            kernel_ewma_volatility<<<1, 1, 0, stream>>>(
                d_returns, d_var_out, params->ewma_lambda, init_var, n);

            float h_ewma_var;
            cudaMemcpy(&h_ewma_var, d_var_out, sizeof(float), cudaMemcpyDeviceToHost);
            result->vol_ewma = sqrtf(h_ewma_var);

            cudaFree(d_var_out);
        }
    } else {
        result->vol_ewma = std_dev;
    }

    result->vol_realized = std_dev;

    // Compute Sharpe ratio
    float risk_free_daily = params->base.risk_free_rate / params->base.annualization_factor;
    float excess_return = mean - risk_free_daily;
    result->base.sharpe_ratio = (std_dev > 1e-10f) ? excess_return / std_dev : 0.0f;

    // Compute Sortino ratio (using downside deviation)
    float* h_returns = (float*)nimcp_malloc(n * sizeof(float));
    if (!h_returns) {
        // Fall back to using std_dev for downside deviation
        result->base.downside_deviation = std_dev;
        result->base.sortino_ratio = (std_dev > 1e-10f) ? excess_return / std_dev : 0.0f;
    } else {
        cudaMemcpy(h_returns, d_returns, n * sizeof(float), cudaMemcpyDeviceToHost);

        float downside_sum = 0.0f;
        uint32_t downside_count = 0;
        for (uint32_t i = 0; i < n; i++) {
            if (h_returns[i] < risk_free_daily) {
                float diff = h_returns[i] - risk_free_daily;
                downside_sum += diff * diff;
                downside_count++;
            }
        }
        float downside_dev = (downside_count > 0) ? sqrtf(downside_sum / downside_count) : std_dev;
        result->base.downside_deviation = downside_dev;
        result->base.sortino_ratio = (downside_dev > 1e-10f) ? excess_return / downside_dev : 0.0f;

        nimcp_free(h_returns);
    }

    cudaFree(d_returns);
    cudaFree(d_sorted);

    return true;
}

bool fin_risk_gpu_rolling_extended(
    nimcp_gpu_context_t* ctx,
    const float* returns,
    uint32_t num_returns,
    uint32_t window,
    const fin_risk_gpu_params_t* params,
    fin_risk_rolling_result_t* result)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !nimcp_gpu_context_is_valid(ctx)) {
        set_risk_error("Invalid GPU context");
        return false;
    }
    if (!returns || num_returns == 0 || window == 0 || window > num_returns) {
        set_risk_error("Invalid parameters");
        return false;
    }

    uint32_t num_points = num_returns - window + 1;
    result->num_points = num_points;

    // Allocate output arrays
    result->var_series = (float*)nimcp_calloc(num_points, sizeof(float));
    result->cvar_series = (float*)nimcp_calloc(num_points, sizeof(float));
    result->vol_series = (float*)nimcp_calloc(num_points, sizeof(float));
    result->drawdown_series = (float*)nimcp_calloc(num_points, sizeof(float));

    if (!result->var_series || !result->cvar_series ||
        !result->vol_series || !result->drawdown_series) {
        fin_risk_rolling_result_free(result);
        set_risk_error("Memory allocation failed");
        return false;
    }

    cudaStream_t stream = nimcp_gpu_get_compute_stream(ctx);
    uint32_t block_size = 256;
    uint32_t grid_size = NIMCP_CUDA_GRID_SIZE(num_returns, block_size);

    float* d_returns = NULL;
    float* d_rolling_var = NULL;
    float* d_rolling_vol = NULL;
    float* d_rolling_cvar = NULL;

    cudaError_t err = cudaMalloc(&d_returns, num_returns * sizeof(float));
    if (err != cudaSuccess) goto cleanup_rolling_ext;

    err = cudaMalloc(&d_rolling_var, num_points * sizeof(float));
    if (err != cudaSuccess) goto cleanup_rolling_ext;

    err = cudaMalloc(&d_rolling_vol, num_points * sizeof(float));
    if (err != cudaSuccess) goto cleanup_rolling_ext;

    err = cudaMalloc(&d_rolling_cvar, num_points * sizeof(float));
    if (err != cudaSuccess) goto cleanup_rolling_ext;

    cudaMemcpyAsync(d_returns, returns, num_returns * sizeof(float),
                    cudaMemcpyHostToDevice, stream);

    // Rolling volatility
    kernel_rolling_volatility<<<grid_size, block_size, 0, stream>>>(
        d_returns, d_rolling_vol, window, num_returns);

    // Rolling VaR (parametric)
    kernel_rolling_var<<<grid_size, block_size, 0, stream>>>(
        d_returns, d_rolling_var, window, params->confidence_95, num_returns);

    // Rolling CVaR (proper computation)
    {
        uint32_t cvar_block_size = (window <= 256) ? 256 : 512;
        size_t shared_size = window * sizeof(float);
        kernel_rolling_cvar<<<num_points, cvar_block_size, shared_size, stream>>>(
            d_returns, d_rolling_cvar, window, params->confidence_95, num_returns);
    }

    cudaMemcpy(result->vol_series, d_rolling_vol, num_points * sizeof(float),
               cudaMemcpyDeviceToHost);
    cudaMemcpy(result->var_series, d_rolling_var, num_points * sizeof(float),
               cudaMemcpyDeviceToHost);
    cudaMemcpy(result->cvar_series, d_rolling_cvar, num_points * sizeof(float),
               cudaMemcpyDeviceToHost);

    // Compute rolling drawdown on CPU (requires cumulative price)
    // This is a simplified version using returns
    {
        float cum_return = 1.0f;
        float peak = 1.0f;
        for (uint32_t i = 0; i < num_points; i++) {
            // Use mean return of window as proxy
            float sum = 0.0f;
            for (uint32_t j = 0; j < window && (i + j) < num_returns; j++) {
                sum += returns[i + j];
            }
            cum_return *= (1.0f + sum / window);
            if (cum_return > peak) peak = cum_return;
            result->drawdown_series[i] = (cum_return - peak) / peak;
        }
    }

    cudaFree(d_returns);
    cudaFree(d_rolling_var);
    cudaFree(d_rolling_vol);
    cudaFree(d_rolling_cvar);
    return true;

cleanup_rolling_ext:
    cudaFree(d_returns);
    cudaFree(d_rolling_var);
    cudaFree(d_rolling_vol);
    cudaFree(d_rolling_cvar);
    fin_risk_rolling_result_free(result);
    set_risk_error("Memory allocation failed");
    return false;
}

float fin_risk_gpu_var_parametric(
    nimcp_gpu_context_t* ctx,
    const float* returns,
    uint32_t num_returns,
    float confidence)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !nimcp_gpu_context_is_valid(ctx) || !returns || num_returns < 2) {
        set_risk_error("Invalid parameters");
        return 0.0f;
    }

    cudaStream_t stream = nimcp_gpu_get_compute_stream(ctx);
    uint32_t block_size = 256;
    uint32_t reduce_blocks = (num_returns + block_size * 2 - 1) / (block_size * 2);

    float* d_returns = NULL;
    float* d_partial = NULL;
    float* h_partial = NULL;

    cudaError_t err = cudaMalloc(&d_returns, num_returns * sizeof(float));
    if (err != cudaSuccess) return 0.0f;

    err = cudaMalloc(&d_partial, reduce_blocks * sizeof(float));
    if (err != cudaSuccess) {
        cudaFree(d_returns);
        return 0.0f;
    }

    h_partial = (float*)nimcp_malloc(reduce_blocks * sizeof(float));
    if (!h_partial) {
        cudaFree(d_returns);
        cudaFree(d_partial);
        return 0.0f;
    }

    cudaMemcpyAsync(d_returns, returns, num_returns * sizeof(float),
                    cudaMemcpyHostToDevice, stream);

    // Compute mean
    kernel_compute_mean<<<reduce_blocks, block_size, block_size * sizeof(float), stream>>>(
        d_returns, d_partial, num_returns);
    cudaMemcpy(h_partial, d_partial, reduce_blocks * sizeof(float), cudaMemcpyDeviceToHost);

    float sum = 0.0f;
    for (uint32_t i = 0; i < reduce_blocks; i++) sum += h_partial[i];
    float mean = sum / (float)num_returns;

    // Compute variance
    kernel_compute_variance<<<reduce_blocks, block_size, block_size * sizeof(float), stream>>>(
        d_returns, mean, d_partial, num_returns);
    cudaMemcpy(h_partial, d_partial, reduce_blocks * sizeof(float), cudaMemcpyDeviceToHost);

    float sq_sum = 0.0f;
    for (uint32_t i = 0; i < reduce_blocks; i++) sq_sum += h_partial[i];
    float std_dev = sqrtf(sq_sum / (float)(num_returns - 1));

    nimcp_free(h_partial);
    cudaFree(d_returns);
    cudaFree(d_partial);

    // Normal quantile for given confidence
    // Using approximation for common values
    float z;
    if (confidence >= 0.99f) {
        z = -2.326f;
    } else if (confidence >= 0.975f) {
        z = -1.96f;
    } else if (confidence >= 0.95f) {
        z = -1.645f;
    } else if (confidence >= 0.90f) {
        z = -1.282f;
    } else {
        // General approximation
        float p = 1.0f - confidence;
        float t = sqrtf(-2.0f * logf(p));
        z = -(t - (2.515517f + 0.802853f * t + 0.010328f * t * t) /
              (1.0f + 1.432788f * t + 0.189269f * t * t + 0.001308f * t * t * t));
    }

    return -(mean + z * std_dev);
}

float fin_risk_gpu_var_cornish_fisher(
    nimcp_gpu_context_t* ctx,
    const float* returns,
    uint32_t num_returns,
    float confidence)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !nimcp_gpu_context_is_valid(ctx) || !returns || num_returns < 4) {
        set_risk_error("Invalid parameters (need at least 4 returns for Cornish-Fisher)");
        return 0.0f;
    }

    // Allocate and copy returns to device
    float* d_returns = NULL;
    cudaError_t err = cudaMalloc(&d_returns, num_returns * sizeof(float));
    if (err != cudaSuccess) return 0.0f;

    cudaStream_t stream = nimcp_gpu_get_compute_stream(ctx);
    cudaMemcpyAsync(d_returns, returns, num_returns * sizeof(float),
                    cudaMemcpyHostToDevice, stream);

    // Compute all moments
    float mean, variance, skewness, kurtosis;
    if (!compute_moments_gpu(ctx, d_returns, num_returns, &mean, &variance, &skewness, &kurtosis)) {
        cudaFree(d_returns);
        return 0.0f;
    }

    cudaFree(d_returns);

    float std_dev = sqrtf(variance);
    if (std_dev < 1e-10f) return -mean;

    // Normal quantile
    float z;
    if (confidence >= 0.99f) {
        z = -2.326f;
    } else if (confidence >= 0.95f) {
        z = -1.645f;
    } else {
        float p = 1.0f - confidence;
        float t = sqrtf(-2.0f * logf(p));
        z = -(t - (2.515517f + 0.802853f * t + 0.010328f * t * t) /
              (1.0f + 1.432788f * t + 0.189269f * t * t + 0.001308f * t * t * t));
    }

    // Cornish-Fisher expansion
    float z_cf = z + (skewness / 6.0f) * (z * z - 1.0f)
               + (kurtosis / 24.0f) * (z * z * z - 3.0f * z)
               - (skewness * skewness / 36.0f) * (2.0f * z * z * z - 5.0f * z);

    return -(mean + z_cf * std_dev);
}

float fin_risk_gpu_max_drawdown(
    nimcp_gpu_context_t* ctx,
    const float* prices,
    uint32_t num_prices,
    uint32_t* out_start,
    uint32_t* out_end)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !nimcp_gpu_context_is_valid(ctx) || !prices || num_prices < 2) {
        set_risk_error("Invalid parameters");
        return 0.0f;
    }

    cudaStream_t stream = nimcp_gpu_get_compute_stream(ctx);
    uint32_t block_size = 256;
    uint32_t grid_size = NIMCP_CUDA_GRID_SIZE(num_prices, block_size);

    float* d_prices = NULL;
    float* d_drawdown = NULL;
    float* d_peak = NULL;

    cudaError_t err = cudaMalloc(&d_prices, num_prices * sizeof(float));
    if (err != cudaSuccess) return 0.0f;

    err = cudaMalloc(&d_drawdown, num_prices * sizeof(float));
    if (err != cudaSuccess) {
        cudaFree(d_prices);
        return 0.0f;
    }

    err = cudaMalloc(&d_peak, num_prices * sizeof(float));
    if (err != cudaSuccess) {
        cudaFree(d_prices);
        cudaFree(d_drawdown);
        return 0.0f;
    }

    cudaMemcpyAsync(d_prices, prices, num_prices * sizeof(float),
                    cudaMemcpyHostToDevice, stream);

    // Compute prefix max (peak prices) using parallel scan
    cudaMemcpy(d_peak, d_prices, num_prices * sizeof(float), cudaMemcpyDeviceToDevice);
    for (uint32_t offset = 1; offset < num_prices; offset *= 2) {
        kernel_parallel_prefix_max<<<grid_size, block_size, 0, stream>>>(
            d_peak, num_prices, offset);
        cudaDeviceSynchronize();
    }

    // Compute drawdowns: (price - peak) / peak
    kernel_compute_drawdown_series<<<grid_size, block_size, 0, stream>>>(
        d_prices, d_drawdown, d_peak, num_prices);

    // Find minimum drawdown (most negative = max drawdown)
    uint32_t reduce_blocks = (num_prices + block_size - 1) / block_size;
    float* d_partial_min = NULL;
    uint32_t* d_partial_idx = NULL;
    if (cudaMalloc(&d_partial_min, reduce_blocks * sizeof(float)) != cudaSuccess ||
        cudaMalloc(&d_partial_idx, reduce_blocks * sizeof(uint32_t)) != cudaSuccess) {
        cudaFree(d_partial_min);
        cudaFree(d_partial_idx);
        cudaFree(d_prices);
        cudaFree(d_drawdown);
        cudaFree(d_peak);
        if (out_start) *out_start = 0;
        if (out_end) *out_end = 0;
        return 0.0f;
    }

    kernel_find_max_drawdown<<<reduce_blocks, block_size,
        block_size * (sizeof(float) + sizeof(uint32_t)), stream>>>(
        d_drawdown, d_partial_min, d_partial_idx, num_prices);

    float* h_partial_min = (float*)nimcp_malloc(reduce_blocks * sizeof(float));
    uint32_t* h_partial_idx = (uint32_t*)nimcp_malloc(reduce_blocks * sizeof(uint32_t));

    if (!h_partial_min || !h_partial_idx) {
        nimcp_free(h_partial_min);
        nimcp_free(h_partial_idx);
        cudaFree(d_prices);
        cudaFree(d_drawdown);
        cudaFree(d_peak);
        cudaFree(d_partial_min);
        cudaFree(d_partial_idx);
        if (out_start) *out_start = 0;
        if (out_end) *out_end = 0;
        return 0.0f;
    }

    cudaMemcpy(h_partial_min, d_partial_min, reduce_blocks * sizeof(float), cudaMemcpyDeviceToHost);
    cudaMemcpy(h_partial_idx, d_partial_idx, reduce_blocks * sizeof(uint32_t), cudaMemcpyDeviceToHost);

    float max_dd = 0.0f;
    uint32_t max_dd_end = 0;
    for (uint32_t i = 0; i < reduce_blocks; i++) {
        if (h_partial_min[i] < max_dd) {
            max_dd = h_partial_min[i];
            max_dd_end = h_partial_idx[i];
        }
    }

    // Find start of drawdown (last peak before max_dd_end)
    float* h_peak = (float*)nimcp_malloc(num_prices * sizeof(float));
    if (!h_peak) {
        if (out_start) *out_start = 0;
        if (out_end) *out_end = max_dd_end;
    } else {
        cudaMemcpy(h_peak, d_peak, num_prices * sizeof(float), cudaMemcpyDeviceToHost);

        uint32_t max_dd_start = 0;
        for (uint32_t i = max_dd_end; i > 0; i--) {
            if (h_peak[i] > h_peak[i - 1]) {
                max_dd_start = i;
                break;
            }
        }

        if (out_start) *out_start = max_dd_start;
        if (out_end) *out_end = max_dd_end;
        nimcp_free(h_peak);
    }

    nimcp_free(h_partial_min);
    nimcp_free(h_partial_idx);
    cudaFree(d_prices);
    cudaFree(d_drawdown);
    cudaFree(d_peak);
    cudaFree(d_partial_min);
    cudaFree(d_partial_idx);

    return -max_dd;  // Returns positive value (e.g., 0.20 for 20% drawdown)
}

float fin_risk_gpu_ewma_volatility(
    nimcp_gpu_context_t* ctx,
    const float* returns,
    uint32_t num_returns,
    float lambda,
    float* out_vol)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !nimcp_gpu_context_is_valid(ctx) || !returns || num_returns < 2) {
        set_risk_error("Invalid parameters");
        return 0.0f;
    }

    if (lambda <= 0.0f || lambda >= 1.0f) {
        lambda = 0.94f;  // RiskMetrics default
    }

    cudaStream_t stream = nimcp_gpu_get_compute_stream(ctx);

    float* d_returns = NULL;
    float* d_vol_series = NULL;
    float* d_var_final = NULL;

    cudaError_t err = cudaMalloc(&d_returns, num_returns * sizeof(float));
    if (err != cudaSuccess) return 0.0f;

    if (out_vol) {
        err = cudaMalloc(&d_vol_series, num_returns * sizeof(float));
        if (err != cudaSuccess) {
            cudaFree(d_returns);
            return 0.0f;
        }
    }

    err = cudaMalloc(&d_var_final, sizeof(float));
    if (err != cudaSuccess) {
        cudaFree(d_returns);
        cudaFree(d_vol_series);
        return 0.0f;
    }

    cudaMemcpyAsync(d_returns, returns, num_returns * sizeof(float),
                    cudaMemcpyHostToDevice, stream);

    // Initial variance estimate from first few returns
    float init_var = 0.0f;
    uint32_t init_n = (num_returns > 10) ? 10 : num_returns;
    for (uint32_t i = 0; i < init_n; i++) {
        init_var += returns[i] * returns[i];
    }
    init_var /= init_n;

    if (out_vol) {
        // Compute full volatility series
        kernel_ewma_volatility_series<<<1, 1, 0, stream>>>(
            d_returns, d_vol_series, lambda, init_var, num_returns);
        cudaMemcpy(out_vol, d_vol_series, num_returns * sizeof(float), cudaMemcpyDeviceToHost);
    }

    // Compute final variance
    kernel_ewma_volatility<<<1, 1, 0, stream>>>(
        d_returns, d_var_final, lambda, init_var, num_returns);

    float final_var;
    cudaMemcpy(&final_var, d_var_final, sizeof(float), cudaMemcpyDeviceToHost);

    cudaFree(d_returns);
    cudaFree(d_vol_series);
    cudaFree(d_var_final);

    return sqrtf(final_var);
}

float fin_risk_gpu_portfolio_var_delta_normal(
    nimcp_gpu_context_t* ctx,
    const float* weights,
    const float* covariance,
    uint32_t num_assets,
    float confidence,
    uint32_t horizon_days)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !nimcp_gpu_context_is_valid(ctx) || !weights || !covariance || num_assets == 0) {
        set_risk_error("Invalid parameters");
        return 0.0f;
    }

    // Compute portfolio variance: w' * Sigma * w
    // First compute Sigma * w, then w' * (Sigma * w)
    float portfolio_variance = 0.0f;

    // For small portfolios, compute on CPU (simpler)
    // For large portfolios, could use cuBLAS
    if (num_assets <= 256) {
        float* sigma_w = (float*)nimcp_malloc(num_assets * sizeof(float));
        if (!sigma_w) return 0.0f;

        // Sigma * w
        for (uint32_t i = 0; i < num_assets; i++) {
            sigma_w[i] = 0.0f;
            for (uint32_t j = 0; j < num_assets; j++) {
                sigma_w[i] += covariance[i * num_assets + j] * weights[j];
            }
        }

        // w' * (Sigma * w)
        for (uint32_t i = 0; i < num_assets; i++) {
            portfolio_variance += weights[i] * sigma_w[i];
        }

        nimcp_free(sigma_w);
    } else {
        // TODO: Use cuBLAS for large portfolios
        set_risk_error("Large portfolios (>256 assets) require cuBLAS - not yet implemented");
        return 0.0f;
    }

    float portfolio_std = sqrtf(portfolio_variance);

    // Scale for horizon
    portfolio_std *= sqrtf((float)horizon_days);

    // Normal quantile
    float z;
    if (confidence >= 0.99f) {
        z = 2.326f;
    } else if (confidence >= 0.95f) {
        z = 1.645f;
    } else {
        float p = 1.0f - confidence;
        float t = sqrtf(-2.0f * logf(p));
        z = t - (2.515517f + 0.802853f * t + 0.010328f * t * t) /
            (1.0f + 1.432788f * t + 0.189269f * t * t + 0.001308f * t * t * t);
    }

    return z * portfolio_std;
}

bool fin_risk_gpu_batch(
    nimcp_gpu_context_t* ctx,
    const float* returns,
    uint32_t num_portfolios,
    uint32_t num_returns,
    const fin_risk_gpu_params_t* params,
    fin_risk_gpu_result_t* results)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !nimcp_gpu_context_is_valid(ctx)) {
        set_risk_error("Invalid GPU context");
        return false;
    }
    if (!returns || !params || !results || num_portfolios == 0 || num_returns < 2) {
        set_risk_error("Invalid parameters");
        return false;
    }

    cudaStream_t stream = nimcp_gpu_get_compute_stream(ctx);
    uint32_t block_size = 256;

    // Allocate device memory
    float* d_returns = NULL;
    float* d_means = NULL;
    float* d_variances = NULL;

    size_t total_size = (size_t)num_portfolios * num_returns * sizeof(float);
    cudaError_t err = cudaMalloc(&d_returns, total_size);
    if (err != cudaSuccess) {
        set_risk_error("Failed to allocate returns");
        return false;
    }

    err = cudaMalloc(&d_means, num_portfolios * sizeof(float));
    if (err != cudaSuccess) {
        cudaFree(d_returns);
        set_risk_error("Failed to allocate means");
        return false;
    }

    err = cudaMalloc(&d_variances, num_portfolios * sizeof(float));
    if (err != cudaSuccess) {
        cudaFree(d_returns);
        cudaFree(d_means);
        set_risk_error("Failed to allocate variances");
        return false;
    }

    cudaMemcpyAsync(d_returns, returns, total_size, cudaMemcpyHostToDevice, stream);

    // Launch batch mean/variance kernel
    kernel_batch_risk_mean_var<<<num_portfolios, block_size,
        2 * block_size * sizeof(float), stream>>>(
        d_returns, d_means, d_variances, num_portfolios, num_returns);

    // Copy results back
    float* h_means = (float*)nimcp_malloc(num_portfolios * sizeof(float));
    float* h_variances = (float*)nimcp_malloc(num_portfolios * sizeof(float));

    if (!h_means || !h_variances) {
        nimcp_free(h_means);
        nimcp_free(h_variances);
        cudaFree(d_returns);
        cudaFree(d_means);
        cudaFree(d_variances);
        set_risk_error("Failed to allocate host buffers");
        return false;
    }

    cudaMemcpy(h_means, d_means, num_portfolios * sizeof(float), cudaMemcpyDeviceToHost);
    cudaMemcpy(h_variances, d_variances, num_portfolios * sizeof(float), cudaMemcpyDeviceToHost);

    // Compute VaR/CVaR for each portfolio
    float z_95 = -1.645f;
    float z_99 = -2.326f;
    float risk_free_daily = params->risk_free_rate / params->annualization_factor;

    for (uint32_t p = 0; p < num_portfolios; p++) {
        float mean = h_means[p];
        float std_dev = sqrtf(h_variances[p]);

        results[p].mean_return = mean;
        results[p].volatility = std_dev;
        results[p].volatility_daily = std_dev;
        results[p].volatility_annual = std_dev * sqrtf((float)params->annualization_factor);

        // Parametric VaR
        results[p].var_95 = -(mean + z_95 * std_dev);
        results[p].var_99 = -(mean + z_99 * std_dev);
        results[p].var_parametric = results[p].var_95;

        // Approximate CVaR (for parametric: E[X | X < VaR])
        // For normal distribution: CVaR = mean + std * phi(z) / (1 - confidence)
        float phi_95 = expf(-z_95 * z_95 / 2.0f) / sqrtf(NIMCP_TWO_PI_F);
        float phi_99 = expf(-z_99 * z_99 / 2.0f) / sqrtf(NIMCP_TWO_PI_F);
        results[p].cvar_95 = -(mean - std_dev * phi_95 / 0.05f);
        results[p].cvar_99 = -(mean - std_dev * phi_99 / 0.01f);

        // Sharpe ratio
        float excess = mean - risk_free_daily;
        results[p].sharpe_ratio = (std_dev > 1e-10f) ? excess / std_dev : 0.0f;
    }

    nimcp_free(h_means);
    nimcp_free(h_variances);
    cudaFree(d_returns);
    cudaFree(d_means);
    cudaFree(d_variances);

    return true;
}

bool fin_risk_gpu_correlation_matrix(
    nimcp_gpu_context_t* ctx,
    const float* returns,
    uint32_t num_assets,
    uint32_t num_returns,
    float* correlation)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !nimcp_gpu_context_is_valid(ctx)) {
        set_risk_error("Invalid GPU context");
        return false;
    }
    if (!returns || !correlation || num_assets == 0 || num_returns < 2) {
        set_risk_error("Invalid parameters");
        return false;
    }

    // First compute covariance matrix
    float* covariance = (float*)nimcp_malloc(num_assets * num_assets * sizeof(float));
    if (!covariance) {
        set_risk_error("Memory allocation failed");
        return false;
    }

    if (!fin_risk_gpu_covariance_matrix(ctx, returns, num_assets, num_returns, covariance)) {
        nimcp_free(covariance);
        return false;
    }

    // Convert covariance to correlation: corr[i,j] = cov[i,j] / (std[i] * std[j])
    float* std_devs = (float*)nimcp_malloc(num_assets * sizeof(float));
    if (!std_devs) {
        nimcp_free(covariance);
        set_risk_error("Memory allocation failed");
        return false;
    }

    // Extract standard deviations from diagonal
    for (uint32_t i = 0; i < num_assets; i++) {
        std_devs[i] = sqrtf(covariance[i * num_assets + i]);
    }

    // Compute correlation and clamp to [-1, 1] to handle floating point precision
    for (uint32_t i = 0; i < num_assets; i++) {
        for (uint32_t j = 0; j < num_assets; j++) {
            float denom = std_devs[i] * std_devs[j];
            float corr;
            if (denom > 1e-10f) {
                corr = covariance[i * num_assets + j] / denom;
            } else {
                corr = (i == j) ? 1.0f : 0.0f;
            }
            // Clamp to valid correlation range [-1, 1]
            if (corr > 1.0f) corr = 1.0f;
            if (corr < -1.0f) corr = -1.0f;
            correlation[i * num_assets + j] = corr;
        }
    }

    nimcp_free(covariance);
    nimcp_free(std_devs);
    return true;
}

bool fin_risk_gpu_covariance_matrix(
    nimcp_gpu_context_t* ctx,
    const float* returns,
    uint32_t num_assets,
    uint32_t num_returns,
    float* covariance)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !nimcp_gpu_context_is_valid(ctx)) {
        set_risk_error("Invalid GPU context");
        return false;
    }
    if (!returns || !covariance || num_assets == 0 || num_returns < 2) {
        set_risk_error("Invalid parameters");
        return false;
    }

    cudaStream_t stream = nimcp_gpu_get_compute_stream(ctx);
    uint32_t block_size = 256;

    // Allocate device memory
    float* d_returns = NULL;
    float* d_means = NULL;
    float* d_covariance = NULL;

    size_t returns_size = (size_t)num_assets * num_returns * sizeof(float);
    size_t cov_size = (size_t)num_assets * num_assets * sizeof(float);

    cudaError_t err = cudaMalloc(&d_returns, returns_size);
    if (err != cudaSuccess) {
        set_risk_error("Failed to allocate returns");
        return false;
    }

    err = cudaMalloc(&d_means, num_assets * sizeof(float));
    if (err != cudaSuccess) {
        cudaFree(d_returns);
        set_risk_error("Failed to allocate means");
        return false;
    }

    err = cudaMalloc(&d_covariance, cov_size);
    if (err != cudaSuccess) {
        cudaFree(d_returns);
        cudaFree(d_means);
        set_risk_error("Failed to allocate covariance");
        return false;
    }

    cudaMemcpyAsync(d_returns, returns, returns_size, cudaMemcpyHostToDevice, stream);

    // Compute means for each asset
    kernel_compute_asset_means<<<num_assets, block_size, block_size * sizeof(float), stream>>>(
        d_returns, d_means, num_assets, num_returns);

    // Compute covariance elements
    dim3 grid(num_assets, num_assets);
    kernel_compute_covariance_element<<<grid, block_size, block_size * sizeof(float), stream>>>(
        d_returns, d_means, d_covariance, num_assets, num_returns);

    cudaMemcpy(covariance, d_covariance, cov_size, cudaMemcpyDeviceToHost);

    cudaFree(d_returns);
    cudaFree(d_means);
    cudaFree(d_covariance);

    return true;
}

bool fin_risk_gpu_sort(
    nimcp_gpu_context_t* ctx,
    float* data,
    uint32_t n,
    bool ascending)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !nimcp_gpu_context_is_valid(ctx)) {
        set_risk_error("Invalid GPU context");
        return false;
    }
    if (!data || n == 0) {
        set_risk_error("Invalid parameters");
        return false;
    }

    cudaStream_t stream = nimcp_gpu_get_compute_stream(ctx);

    // Check if data is already on device using cudaPointerGetAttributes
    cudaPointerAttributes attr;
    cudaError_t ptr_err = cudaPointerGetAttributes(&attr, data);

    bool is_device_ptr = (ptr_err == cudaSuccess &&
                          (attr.type == cudaMemoryTypeDevice || attr.type == cudaMemoryTypeManaged));

    // Reset any error from cudaPointerGetAttributes (it may fail for host pointers on older CUDA)
    cudaGetLastError();

    float* d_work = NULL;
    bool allocated_work = false;

    if (is_device_ptr) {
        // Data is already on device - use directly or copy to work buffer
        // We need a separate buffer for sorting in case input needs preservation
        cudaError_t err = cudaMalloc(&d_work, n * sizeof(float));
        if (err != cudaSuccess) {
            set_risk_error("Failed to allocate device memory for sort");
            return false;
        }
        allocated_work = true;
        cudaMemcpy(d_work, data, n * sizeof(float), cudaMemcpyDeviceToDevice);
    } else {
        // Data is on host - allocate device memory and copy
        cudaError_t err = cudaMalloc(&d_work, n * sizeof(float));
        if (err != cudaSuccess) {
            set_risk_error("Failed to allocate device memory for sort");
            return false;
        }
        allocated_work = true;
        err = cudaMemcpy(d_work, data, n * sizeof(float), cudaMemcpyHostToDevice);
        if (err != cudaSuccess) {
            cudaFree(d_work);
            set_risk_error("Failed to copy data to device for sort");
            return false;
        }
    }

    // Sort on device
    bool result = gpu_sort(d_work, n, stream);

    cudaStreamSynchronize(stream);

    if (result) {
        if (is_device_ptr) {
            // Copy sorted data back to device pointer
            cudaMemcpy(data, d_work, n * sizeof(float), cudaMemcpyDeviceToDevice);

            // If descending, reverse on device (use a simple kernel or copy to host)
            if (!ascending) {
                // For simplicity, do reverse on host via temp buffer
                std::vector<float> h_temp(n);
                cudaMemcpy(h_temp.data(), d_work, n * sizeof(float), cudaMemcpyDeviceToHost);
                for (uint32_t i = 0; i < n / 2; i++) {
                    std::swap(h_temp[i], h_temp[n - 1 - i]);
                }
                cudaMemcpy(data, h_temp.data(), n * sizeof(float), cudaMemcpyHostToDevice);
            }
        } else {
            // Copy sorted data back to host
            cudaError_t err = cudaMemcpy(data, d_work, n * sizeof(float), cudaMemcpyDeviceToHost);
            if (err != cudaSuccess) {
                if (allocated_work) cudaFree(d_work);
                set_risk_error("Failed to copy sorted data back to host");
                return false;
            }

            // If descending, reverse the array on host
            if (!ascending) {
                for (uint32_t i = 0; i < n / 2; i++) {
                    float tmp = data[i];
                    data[i] = data[n - 1 - i];
                    data[n - 1 - i] = tmp;
                }
            }
        }
    }

    if (allocated_work) cudaFree(d_work);
    return result;
}

float fin_risk_gpu_percentile(
    nimcp_gpu_context_t* ctx,
    const float* sorted,
    uint32_t n,
    float percentile)
{
    if (!ctx || !nimcp_gpu_context_is_valid(ctx) || !sorted || n == 0) {
        return 0.0f;
    }

    if (percentile < 0.0f) percentile = 0.0f;
    if (percentile > 100.0f) percentile = 100.0f;

    // Convert percentile to index
    float idx_f = (percentile / 100.0f) * (n - 1);
    uint32_t idx_lo = (uint32_t)idx_f;
    uint32_t idx_hi = idx_lo + 1;
    if (idx_hi >= n) idx_hi = n - 1;

    float frac = idx_f - (float)idx_lo;

    // Read values from device
    float val_lo, val_hi;
    cudaMemcpy(&val_lo, &sorted[idx_lo], sizeof(float), cudaMemcpyDeviceToHost);
    cudaMemcpy(&val_hi, &sorted[idx_hi], sizeof(float), cudaMemcpyDeviceToHost);

    // Linear interpolation
    return val_lo + frac * (val_hi - val_lo);
}

float fin_risk_gpu_var(
    nimcp_gpu_context_t* ctx,
    const float* returns,
    uint32_t num_returns,
    float confidence)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !nimcp_gpu_context_is_valid(ctx) || !returns || num_returns < 2) {
        return 0.0f;
    }

    cudaStream_t stream = nimcp_gpu_get_compute_stream(ctx);

    // Allocate and copy
    float* d_sorted = NULL;
    cudaError_t err = cudaMalloc(&d_sorted, num_returns * sizeof(float));
    if (err != cudaSuccess) return 0.0f;

    cudaMemcpyAsync(d_sorted, returns, num_returns * sizeof(float),
                    cudaMemcpyHostToDevice, stream);

    // Sort
    gpu_sort(d_sorted, num_returns, stream);

    // Get VaR percentile
    uint32_t var_idx = (uint32_t)((1.0f - confidence) * num_returns);
    if (var_idx >= num_returns) var_idx = num_returns - 1;

    float h_var;
    cudaMemcpy(&h_var, &d_sorted[var_idx], sizeof(float), cudaMemcpyDeviceToHost);

    cudaFree(d_sorted);

    return -h_var;  // Return as positive loss
}

float fin_risk_gpu_cvar(
    nimcp_gpu_context_t* ctx,
    const float* returns,
    uint32_t num_returns,
    float confidence)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !nimcp_gpu_context_is_valid(ctx) || !returns || num_returns < 2) {
        return 0.0f;
    }

    cudaStream_t stream = nimcp_gpu_get_compute_stream(ctx);
    uint32_t block_size = 256;

    // Allocate and copy
    float* d_sorted = NULL;
    float* d_cvar = NULL;

    cudaError_t err = cudaMalloc(&d_sorted, num_returns * sizeof(float));
    if (err != cudaSuccess) return 0.0f;

    err = cudaMalloc(&d_cvar, sizeof(float));
    if (err != cudaSuccess) {
        cudaFree(d_sorted);
        return 0.0f;
    }

    cudaMemcpyAsync(d_sorted, returns, num_returns * sizeof(float),
                    cudaMemcpyHostToDevice, stream);

    // Sort
    gpu_sort(d_sorted, num_returns, stream);

    // Compute CVaR
    kernel_compute_cvar<<<1, block_size, block_size * sizeof(float), stream>>>(
        d_sorted, d_cvar, num_returns, confidence);

    float h_cvar;
    cudaMemcpy(&h_cvar, d_cvar, sizeof(float), cudaMemcpyDeviceToHost);

    cudaFree(d_sorted);
    cudaFree(d_cvar);

    return -h_cvar;  // Return as positive expected shortfall
}

void fin_risk_rolling_result_free(fin_risk_rolling_result_t* result)
{
    if (!result) return;

    nimcp_free(result->var_series);
    nimcp_free(result->cvar_series);
    nimcp_free(result->vol_series);
    nimcp_free(result->drawdown_series);

    result->var_series = NULL;
    result->cvar_series = NULL;
    result->vol_series = NULL;
    result->drawdown_series = NULL;
    result->num_points = 0;
}

void fin_risk_extended_result_free(fin_risk_extended_result_t* result)
{
    // Extended result doesn't have dynamic allocations currently
    // But clear it anyway for safety
    if (result) {
        memset(result, 0, sizeof(*result));
    }
}

const char* fin_risk_gpu_get_last_error(void) {
    return g_risk_error;
}

} // extern "C"

#else // !NIMCP_ENABLE_CUDA

//=============================================================================
// CPU Fallback Stubs
//=============================================================================

extern "C" {

bool fin_risk_gpu_compute(
    nimcp_gpu_context_t* ctx,
    const float* returns,
    const fin_risk_gpu_params_t* params,
    fin_risk_gpu_result_t* result)
{
    (void)ctx; (void)returns; (void)params; (void)result;
    return false;
}

bool fin_risk_gpu_volatility(
    nimcp_gpu_context_t* ctx,
    const float* prices,
    uint32_t n,
    fin_vol_method_t method,
    float* volatility)
{
    (void)ctx; (void)prices; (void)n; (void)method; (void)volatility;
    return false;
}

bool fin_risk_gpu_volatility_ohlc(
    nimcp_gpu_context_t* ctx,
    const float* open_prices,
    const float* high_prices,
    const float* low_prices,
    const float* close_prices,
    uint32_t n,
    fin_vol_method_t method,
    float* volatility)
{
    (void)ctx; (void)open_prices; (void)high_prices; (void)low_prices;
    (void)close_prices; (void)n; (void)method; (void)volatility;
    return false;
}

bool fin_risk_gpu_rolling(
    nimcp_gpu_context_t* ctx,
    const float* returns,
    uint32_t num_returns,
    uint32_t window,
    const fin_risk_gpu_params_t* params,
    fin_risk_rolling_result_t* result)
{
    (void)ctx; (void)returns; (void)num_returns; (void)window;
    (void)params; (void)result;
    return false;
}

bool fin_risk_gpu_extended(
    nimcp_gpu_context_t* ctx,
    const float* returns,
    uint32_t num_returns,
    const fin_risk_extended_params_t* params,
    fin_risk_extended_result_t* result)
{
    (void)ctx; (void)returns; (void)num_returns; (void)params; (void)result;
    return false;
}

bool fin_risk_gpu_rolling_extended(
    nimcp_gpu_context_t* ctx,
    const float* returns,
    uint32_t num_returns,
    uint32_t window,
    const fin_risk_gpu_params_t* params,
    fin_risk_rolling_result_t* result)
{
    (void)ctx; (void)returns; (void)num_returns; (void)window;
    (void)params; (void)result;
    return false;
}

float fin_risk_gpu_var_parametric(
    nimcp_gpu_context_t* ctx,
    const float* returns,
    uint32_t num_returns,
    float confidence)
{
    (void)ctx; (void)returns; (void)num_returns; (void)confidence;
    return 0.0f;
}

float fin_risk_gpu_var_cornish_fisher(
    nimcp_gpu_context_t* ctx,
    const float* returns,
    uint32_t num_returns,
    float confidence)
{
    (void)ctx; (void)returns; (void)num_returns; (void)confidence;
    return 0.0f;
}

float fin_risk_gpu_max_drawdown(
    nimcp_gpu_context_t* ctx,
    const float* prices,
    uint32_t num_prices,
    uint32_t* out_start,
    uint32_t* out_end)
{
    (void)ctx; (void)prices; (void)num_prices;
    (void)out_start; (void)out_end;
    return 0.0f;
}

float fin_risk_gpu_ewma_volatility(
    nimcp_gpu_context_t* ctx,
    const float* returns,
    uint32_t num_returns,
    float lambda,
    float* out_vol)
{
    (void)ctx; (void)returns; (void)num_returns; (void)lambda; (void)out_vol;
    return 0.0f;
}

float fin_risk_gpu_portfolio_var_delta_normal(
    nimcp_gpu_context_t* ctx,
    const float* weights,
    const float* covariance,
    uint32_t num_assets,
    float confidence,
    uint32_t horizon_days)
{
    (void)ctx; (void)weights; (void)covariance; (void)num_assets;
    (void)confidence; (void)horizon_days;
    return 0.0f;
}

bool fin_risk_gpu_batch(
    nimcp_gpu_context_t* ctx,
    const float* returns,
    uint32_t num_portfolios,
    uint32_t num_returns,
    const fin_risk_gpu_params_t* params,
    fin_risk_gpu_result_t* results)
{
    (void)ctx; (void)returns; (void)num_portfolios; (void)num_returns;
    (void)params; (void)results;
    return false;
}

bool fin_risk_gpu_correlation_matrix(
    nimcp_gpu_context_t* ctx,
    const float* returns,
    uint32_t num_assets,
    uint32_t num_returns,
    float* correlation)
{
    (void)ctx; (void)returns; (void)num_assets; (void)num_returns;
    (void)correlation;
    return false;
}

bool fin_risk_gpu_covariance_matrix(
    nimcp_gpu_context_t* ctx,
    const float* returns,
    uint32_t num_assets,
    uint32_t num_returns,
    float* covariance)
{
    (void)ctx; (void)returns; (void)num_assets; (void)num_returns;
    (void)covariance;
    return false;
}

bool fin_risk_gpu_sort(
    nimcp_gpu_context_t* ctx,
    float* data,
    uint32_t n,
    bool ascending)
{
    (void)ctx; (void)data; (void)n; (void)ascending;
    return false;
}

float fin_risk_gpu_percentile(
    nimcp_gpu_context_t* ctx,
    const float* sorted,
    uint32_t n,
    float percentile)
{
    (void)ctx; (void)sorted; (void)n; (void)percentile;
    return 0.0f;
}

float fin_risk_gpu_var(
    nimcp_gpu_context_t* ctx,
    const float* returns,
    uint32_t num_returns,
    float confidence)
{
    (void)ctx; (void)returns; (void)num_returns; (void)confidence;
    return 0.0f;
}

float fin_risk_gpu_cvar(
    nimcp_gpu_context_t* ctx,
    const float* returns,
    uint32_t num_returns,
    float confidence)
{
    (void)ctx; (void)returns; (void)num_returns; (void)confidence;
    return 0.0f;
}

void fin_risk_rolling_result_free(fin_risk_rolling_result_t* result)
{
    if (!result) return;
    nimcp_free(result->var_series);
    nimcp_free(result->cvar_series);
    nimcp_free(result->vol_series);
    nimcp_free(result->drawdown_series);
    result->var_series = NULL;
    result->cvar_series = NULL;
    result->vol_series = NULL;
    result->drawdown_series = NULL;
    result->num_points = 0;
}

void fin_risk_extended_result_free(fin_risk_extended_result_t* result)
{
    if (result) {
        memset(result, 0, sizeof(*result));
    }
}

const char* fin_risk_gpu_get_last_error(void) {
    return "GPU support not compiled";
}

} // extern "C"

#endif // NIMCP_ENABLE_CUDA
