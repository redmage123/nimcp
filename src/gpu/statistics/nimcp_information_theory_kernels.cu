/**
 * @file nimcp_information_theory_kernels.cu
 * @brief CUDA Kernels for Advanced Information Theory Computations
 *
 * WHAT: GPU-accelerated implementations of computationally intensive
 *       information-theoretic measures including PID, Renyi entropy,
 *       quantum correlations, and complexity measures.
 *
 * WHY:  Many information-theoretic computations scale poorly with data size:
 *       - PID optimization: O(n^3) over probability space
 *       - Directed information: O(n * bins^history)
 *       - Phi computation: Exponential in system size
 *       GPU acceleration provides 10-100x speedup for large-scale analysis.
 *
 * HOW:  CUDA kernels with optimized memory access patterns, shared memory
 *       for probability distributions, warp-level reductions for entropy,
 *       and parallel optimization for PID.
 *
 * PERFORMANCE TARGETS:
 * - Entropy (1M samples, 1024 bins): <5ms
 * - PID bivariate (64x64x64 joint): <20ms
 * - Directed information (100K samples): <50ms
 * - Renyi measures (1M samples): <10ms
 *
 * @author NIMCP Development Team
 * @date 2026-01-30
 */

#ifdef NIMCP_ENABLE_CUDA

#include "utils/memory/nimcp_memory.h"
#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "gpu/common/nimcp_cuda_utils.h"
#include "gpu/common/nimcp_device_utils.cuh"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/exception/nimcp_exception_macros.h"

//=============================================================================
// Constants
//=============================================================================

#define INFO_GPU_BLOCK_SIZE 256
#define INFO_GPU_WARP_SIZE 32
#define INFO_GPU_MAX_BINS 1024
#define INFO_GPU_EPSILON 1e-12f

//=============================================================================
// Thread-Local Error Storage
//=============================================================================

static __thread char g_info_gpu_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_info_gpu_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_info_gpu_error, sizeof(g_info_gpu_error), fmt, args);
    va_end(args);
}

//=============================================================================
// Device Utility Functions
//=============================================================================

/**
 * @brief Safe log2 on device with zero handling
 */
__device__ __forceinline__ float safe_log2_device(float x) {
    return (x > INFO_GPU_EPSILON) ? log2f(x) : 0.0f;
}

/**
 * @brief Warp-level reduction for sum
 */
__device__ __forceinline__ float warp_reduce_sum(float val) {
    for (int offset = INFO_GPU_WARP_SIZE / 2; offset > 0; offset /= 2) {
        val += __shfl_down_sync(0xFFFFFFFF, val, offset);
    }
    return val;
}

/**
 * @brief Block-level reduction for sum
 */
static __device__ float block_reduce_sum(float val, float* shared) {
    int lane = threadIdx.x % INFO_GPU_WARP_SIZE;
    int wid = threadIdx.x / INFO_GPU_WARP_SIZE;

    val = warp_reduce_sum(val);

    if (lane == 0) {
        shared[wid] = val;
    }
    __syncthreads();

    val = (threadIdx.x < blockDim.x / INFO_GPU_WARP_SIZE) ? shared[lane] : 0.0f;

    if (wid == 0) {
        val = warp_reduce_sum(val);
    }

    return val;
}

/**
 * @brief Warp-level reduction for max
 */
__device__ __forceinline__ float warp_reduce_max(float val) {
    for (int offset = INFO_GPU_WARP_SIZE / 2; offset > 0; offset /= 2) {
        val = fmaxf(val, __shfl_down_sync(0xFFFFFFFF, val, offset));
    }
    return val;
}

//=============================================================================
// Entropy Kernels
//=============================================================================

/**
 * @brief Compute Shannon entropy of probability distribution
 *
 * Each block processes a portion of the distribution, results are reduced.
 */
__global__ void kernel_shannon_entropy(
    const float* __restrict__ probabilities,
    float* __restrict__ partial_entropy,
    uint32_t n)
{
    extern __shared__ float sdata[];

    uint32_t tid = threadIdx.x;
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t stride = blockDim.x * gridDim.x;

    float local_h = 0.0f;

    /* Each thread accumulates entropy for multiple elements */
    for (uint32_t i = idx; i < n; i += stride) {
        float p = probabilities[i];
        if (p > INFO_GPU_EPSILON) {
            local_h -= p * safe_log2_device(p);
        }
    }

    /* Block-level reduction */
    float h = block_reduce_sum(local_h, sdata);

    if (tid == 0) {
        partial_entropy[blockIdx.x] = h;
    }
}

/**
 * @brief Compute Renyi entropy of order alpha
 *
 * H_alpha = (1/(1-alpha)) * log(sum p_i^alpha)
 */
__global__ void kernel_renyi_entropy(
    const float* __restrict__ probabilities,
    float* __restrict__ partial_sum,
    uint32_t n,
    float alpha)
{
    extern __shared__ float sdata[];

    uint32_t tid = threadIdx.x;
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t stride = blockDim.x * gridDim.x;

    float local_sum = 0.0f;

    for (uint32_t i = idx; i < n; i += stride) {
        float p = probabilities[i];
        if (p > INFO_GPU_EPSILON) {
            local_sum += powf(p, alpha);
        }
    }

    float sum = block_reduce_sum(local_sum, sdata);

    if (tid == 0) {
        partial_sum[blockIdx.x] = sum;
    }
}

/**
 * @brief Compute Tsallis entropy
 *
 * S_q = (1 - sum p_i^q) / (q - 1)
 */
__global__ void kernel_tsallis_entropy(
    const float* __restrict__ probabilities,
    float* __restrict__ partial_sum,
    uint32_t n,
    float q)
{
    extern __shared__ float sdata[];

    uint32_t tid = threadIdx.x;
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t stride = blockDim.x * gridDim.x;

    float local_sum = 0.0f;

    for (uint32_t i = idx; i < n; i += stride) {
        float p = probabilities[i];
        if (p > INFO_GPU_EPSILON) {
            local_sum += powf(p, q);
        }
    }

    float sum = block_reduce_sum(local_sum, sdata);

    if (tid == 0) {
        partial_sum[blockIdx.x] = sum;
    }
}

//=============================================================================
// Mutual Information Kernels
//=============================================================================

/**
 * @brief Compute mutual information from joint probability
 *
 * I(X;Y) = sum_{x,y} P(x,y) * log(P(x,y) / (P(x)*P(y)))
 */
__global__ void kernel_mutual_information(
    const float* __restrict__ joint_prob,
    const float* __restrict__ marginal_x,
    const float* __restrict__ marginal_y,
    float* __restrict__ partial_mi,
    uint32_t n_x,
    uint32_t n_y)
{
    extern __shared__ float sdata[];

    uint32_t tid = threadIdx.x;
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t total = n_x * n_y;
    uint32_t stride = blockDim.x * gridDim.x;

    float local_mi = 0.0f;

    for (uint32_t k = idx; k < total; k += stride) {
        uint32_t i = k / n_y;
        uint32_t j = k % n_y;

        float pxy = joint_prob[k];
        float px = marginal_x[i];
        float py = marginal_y[j];

        if (pxy > INFO_GPU_EPSILON && px > INFO_GPU_EPSILON && py > INFO_GPU_EPSILON) {
            local_mi += pxy * safe_log2_device(pxy / (px * py));
        }
    }

    float mi = block_reduce_sum(local_mi, sdata);

    if (tid == 0) {
        partial_mi[blockIdx.x] = mi;
    }
}

/**
 * @brief Extract marginals from joint probability
 */
__global__ void kernel_extract_marginals(
    const float* __restrict__ joint_prob,
    float* __restrict__ marginal_x,
    float* __restrict__ marginal_y,
    uint32_t n_x,
    uint32_t n_y)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;

    /* Compute marginal X: sum over Y */
    if (idx < n_x) {
        float sum = 0.0f;
        for (uint32_t j = 0; j < n_y; j++) {
            sum += joint_prob[idx * n_y + j];
        }
        marginal_x[idx] = sum;
    }

    /* Compute marginal Y: sum over X */
    if (idx < n_y) {
        float sum = 0.0f;
        for (uint32_t i = 0; i < n_x; i++) {
            sum += joint_prob[i * n_y + idx];
        }
        marginal_y[idx] = sum;
    }
}

//=============================================================================
// KL Divergence Kernels
//=============================================================================

/**
 * @brief Compute KL divergence D_KL(P||Q)
 */
__global__ void kernel_kl_divergence(
    const float* __restrict__ p,
    const float* __restrict__ q,
    float* __restrict__ partial_kl,
    uint32_t n)
{
    extern __shared__ float sdata[];

    uint32_t tid = threadIdx.x;
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t stride = blockDim.x * gridDim.x;

    float local_kl = 0.0f;

    for (uint32_t i = idx; i < n; i += stride) {
        float pi = p[i];
        float qi = q[i];

        if (pi > INFO_GPU_EPSILON) {
            if (qi <= INFO_GPU_EPSILON) {
                /* Q has zero where P is non-zero: infinite divergence */
                local_kl = INFINITY;
                break;
            }
            local_kl += pi * safe_log2_device(pi / qi);
        }
    }

    float kl = block_reduce_sum(local_kl, sdata);

    if (tid == 0) {
        partial_kl[blockIdx.x] = kl;
    }
}

/**
 * @brief Compute Renyi divergence
 */
__global__ void kernel_renyi_divergence(
    const float* __restrict__ p,
    const float* __restrict__ q,
    float* __restrict__ partial_sum,
    uint32_t n,
    float alpha)
{
    extern __shared__ float sdata[];

    uint32_t tid = threadIdx.x;
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t stride = blockDim.x * gridDim.x;

    float local_sum = 0.0f;

    for (uint32_t i = idx; i < n; i += stride) {
        float pi = p[i];
        float qi = q[i];

        if (pi > INFO_GPU_EPSILON && qi > INFO_GPU_EPSILON) {
            local_sum += powf(pi, alpha) * powf(qi, 1.0f - alpha);
        }
    }

    float sum = block_reduce_sum(local_sum, sdata);

    if (tid == 0) {
        partial_sum[blockIdx.x] = sum;
    }
}

//=============================================================================
// PID (Partial Information Decomposition) Kernels
//=============================================================================

/**
 * @brief Compute PID marginals from 3D joint probability
 *
 * Joint P(S1, S2, T) -> P(S1, T), P(S2, T), P(T)
 */
__global__ void kernel_pid_marginals(
    const float* __restrict__ joint_s1s2t,
    float* __restrict__ p_s1_t,
    float* __restrict__ p_s2_t,
    float* __restrict__ p_t,
    uint32_t n_s1,
    uint32_t n_s2,
    uint32_t n_t)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;

    /* Compute P(S1, T) by summing over S2 */
    if (idx < n_s1 * n_t) {
        uint32_t i = idx / n_t;  /* S1 index */
        uint32_t k = idx % n_t;  /* T index */
        float sum = 0.0f;
        for (uint32_t j = 0; j < n_s2; j++) {
            sum += joint_s1s2t[(i * n_s2 + j) * n_t + k];
        }
        p_s1_t[idx] = sum;
    }

    /* Compute P(S2, T) by summing over S1 */
    if (idx < n_s2 * n_t) {
        uint32_t j = idx / n_t;  /* S2 index */
        uint32_t k = idx % n_t;  /* T index */
        float sum = 0.0f;
        for (uint32_t i = 0; i < n_s1; i++) {
            sum += joint_s1s2t[(i * n_s2 + j) * n_t + k];
        }
        p_s2_t[idx] = sum;
    }

    /* Compute P(T) by summing over S1 and S2 */
    if (idx < n_t) {
        float sum = 0.0f;
        for (uint32_t i = 0; i < n_s1; i++) {
            for (uint32_t j = 0; j < n_s2; j++) {
                sum += joint_s1s2t[(i * n_s2 + j) * n_t + idx];
            }
        }
        p_t[idx] = sum;
    }
}

/**
 * @brief Compute total mutual information I(S1,S2 ; T)
 *
 * I = H(S1,S2) + H(T) - H(S1,S2,T)
 */
__global__ void kernel_pid_total_mi(
    const float* __restrict__ joint_s1s2t,
    const float* __restrict__ p_s1s2,
    const float* __restrict__ p_t,
    float* __restrict__ partial_h_joint,
    float* __restrict__ partial_h_s1s2,
    float* __restrict__ partial_h_t,
    uint32_t n_s1,
    uint32_t n_s2,
    uint32_t n_t)
{
    extern __shared__ float sdata[];
    float* shared_h_joint = sdata;
    float* shared_h_s1s2 = &sdata[INFO_GPU_BLOCK_SIZE / INFO_GPU_WARP_SIZE];
    float* shared_h_t = &sdata[2 * INFO_GPU_BLOCK_SIZE / INFO_GPU_WARP_SIZE];

    uint32_t tid = threadIdx.x;
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;

    uint32_t total_joint = n_s1 * n_s2 * n_t;
    uint32_t total_s1s2 = n_s1 * n_s2;
    uint32_t stride = blockDim.x * gridDim.x;

    /* Entropy of joint P(S1,S2,T) */
    float local_h_joint = 0.0f;
    for (uint32_t i = idx; i < total_joint; i += stride) {
        float p = joint_s1s2t[i];
        if (p > INFO_GPU_EPSILON) {
            local_h_joint -= p * safe_log2_device(p);
        }
    }

    /* Entropy of P(S1,S2) */
    float local_h_s1s2 = 0.0f;
    for (uint32_t i = idx; i < total_s1s2; i += stride) {
        float p = p_s1s2[i];
        if (p > INFO_GPU_EPSILON) {
            local_h_s1s2 -= p * safe_log2_device(p);
        }
    }

    /* Entropy of P(T) */
    float local_h_t = 0.0f;
    for (uint32_t i = idx; i < n_t; i += stride) {
        float p = p_t[i];
        if (p > INFO_GPU_EPSILON) {
            local_h_t -= p * safe_log2_device(p);
        }
    }

    /* Block reductions */
    float h_joint = block_reduce_sum(local_h_joint, shared_h_joint);
    __syncthreads();
    float h_s1s2 = block_reduce_sum(local_h_s1s2, shared_h_s1s2);
    __syncthreads();
    float h_t = block_reduce_sum(local_h_t, shared_h_t);

    if (tid == 0) {
        partial_h_joint[blockIdx.x] = h_joint;
        partial_h_s1s2[blockIdx.x] = h_s1s2;
        partial_h_t[blockIdx.x] = h_t;
    }
}

//=============================================================================
// Discretization Kernels
//=============================================================================

/**
 * @brief Find min and max of data array
 */
__global__ void kernel_find_minmax(
    const float* __restrict__ data,
    float* __restrict__ partial_min,
    float* __restrict__ partial_max,
    uint32_t n)
{
    extern __shared__ float sdata[];
    float* s_min = sdata;
    float* s_max = &sdata[blockDim.x];

    uint32_t tid = threadIdx.x;
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t stride = blockDim.x * gridDim.x;

    float local_min = INFINITY;
    float local_max = -INFINITY;

    for (uint32_t i = idx; i < n; i += stride) {
        float val = data[i];
        local_min = fminf(local_min, val);
        local_max = fmaxf(local_max, val);
    }

    s_min[tid] = local_min;
    s_max[tid] = local_max;
    __syncthreads();

    /* Tree reduction in shared memory */
    for (uint32_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            s_min[tid] = fminf(s_min[tid], s_min[tid + s]);
            s_max[tid] = fmaxf(s_max[tid], s_max[tid + s]);
        }
        __syncthreads();
    }

    if (tid == 0) {
        partial_min[blockIdx.x] = s_min[0];
        partial_max[blockIdx.x] = s_max[0];
    }
}

/**
 * @brief Discretize continuous data into bins (equal-width)
 */
__global__ void kernel_discretize(
    const float* __restrict__ data,
    uint32_t* __restrict__ bins,
    uint32_t n,
    float min_val,
    float bin_width,
    uint32_t n_bins)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t stride = blockDim.x * gridDim.x;

    for (uint32_t i = idx; i < n; i += stride) {
        float val = data[i];
        uint32_t bin = (uint32_t)((val - min_val) / bin_width);
        if (bin >= n_bins) bin = n_bins - 1;
        bins[i] = bin;
    }
}

/**
 * @brief Build histogram from discretized data
 */
__global__ void kernel_histogram(
    const uint32_t* __restrict__ bins,
    uint32_t* __restrict__ counts,
    uint32_t n,
    uint32_t n_bins)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t stride = blockDim.x * gridDim.x;

    for (uint32_t i = idx; i < n; i += stride) {
        uint32_t bin = bins[i];
        if (bin < n_bins) {
            atomicAdd(&counts[bin], 1);
        }
    }
}

/**
 * @brief Build 2D joint histogram
 */
__global__ void kernel_joint_histogram(
    const uint32_t* __restrict__ bins_x,
    const uint32_t* __restrict__ bins_y,
    uint32_t* __restrict__ joint_counts,
    uint32_t n,
    uint32_t n_bins_x,
    uint32_t n_bins_y)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t stride = blockDim.x * gridDim.x;

    for (uint32_t i = idx; i < n; i += stride) {
        uint32_t bx = bins_x[i];
        uint32_t by = bins_y[i];
        if (bx < n_bins_x && by < n_bins_y) {
            atomicAdd(&joint_counts[bx * n_bins_y + by], 1);
        }
    }
}

/**
 * @brief Normalize counts to probabilities
 */
__global__ void kernel_normalize_to_prob(
    const uint32_t* __restrict__ counts,
    float* __restrict__ probabilities,
    uint32_t n,
    uint32_t total_count)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t stride = blockDim.x * gridDim.x;

    float norm = (total_count > 0) ? 1.0f / total_count : 0.0f;

    for (uint32_t i = idx; i < n; i += stride) {
        probabilities[i] = counts[i] * norm;
    }
}

//=============================================================================
// Transfer Entropy / Directed Information Kernels
//=============================================================================

/**
 * @brief Build 3D joint histogram for transfer entropy
 *
 * Counts (Y_t, Y_{t-1}, X_{t-1}) tuples
 */
__global__ void kernel_te_joint_histogram(
    const uint32_t* __restrict__ bins_x,
    const uint32_t* __restrict__ bins_y,
    uint32_t* __restrict__ joint_counts,
    uint32_t n,
    uint32_t history,
    uint32_t n_bins)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t stride = blockDim.x * gridDim.x;

    uint32_t joint_size = n_bins * n_bins * n_bins;

    for (uint32_t t = history + idx; t < n; t += stride) {
        uint32_t yt = bins_y[t];
        uint32_t yt_1 = bins_y[t - 1];
        uint32_t xt_1 = bins_x[t - 1];

        if (yt < n_bins && yt_1 < n_bins && xt_1 < n_bins) {
            uint32_t joint_idx = (yt * n_bins + yt_1) * n_bins + xt_1;
            if (joint_idx < joint_size) {
                atomicAdd(&joint_counts[joint_idx], 1);
            }
        }
    }
}

/**
 * @brief Compute transfer entropy from 3D joint probability
 *
 * TE(X->Y) = sum P(yt, yt-1, xt-1) * log(P(yt|yt-1,xt-1) / P(yt|yt-1))
 */
__global__ void kernel_transfer_entropy(
    const float* __restrict__ joint_yyx,       /* P(Y_t, Y_{t-1}, X_{t-1}) */
    const float* __restrict__ marginal_yx,      /* P(Y_{t-1}, X_{t-1}) */
    const float* __restrict__ marginal_yy,      /* P(Y_t, Y_{t-1}) */
    const float* __restrict__ marginal_y,       /* P(Y_{t-1}) */
    float* __restrict__ partial_te,
    uint32_t n_bins)
{
    extern __shared__ float sdata[];

    uint32_t tid = threadIdx.x;
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t total = n_bins * n_bins * n_bins;
    uint32_t stride = blockDim.x * gridDim.x;

    float local_te = 0.0f;

    for (uint32_t k = idx; k < total; k += stride) {
        uint32_t yt = k / (n_bins * n_bins);
        uint32_t ytp = (k / n_bins) % n_bins;
        uint32_t xtp = k % n_bins;

        float p_yyx = joint_yyx[k];

        if (p_yyx > INFO_GPU_EPSILON) {
            /* P(Y_t | Y_{t-1}, X_{t-1}) */
            float p_yx = marginal_yx[ytp * n_bins + xtp];
            float p_cond_full = (p_yx > INFO_GPU_EPSILON) ? p_yyx / p_yx : 0.0f;

            /* P(Y_t | Y_{t-1}) */
            float p_yy = marginal_yy[yt * n_bins + ytp];
            float p_y = marginal_y[ytp];
            float p_cond_base = (p_y > INFO_GPU_EPSILON) ? p_yy / p_y : 0.0f;

            if (p_cond_full > INFO_GPU_EPSILON && p_cond_base > INFO_GPU_EPSILON) {
                local_te += p_yyx * safe_log2_device(p_cond_full / p_cond_base);
            }
        }
    }

    float te = block_reduce_sum(local_te, sdata);

    if (tid == 0) {
        partial_te[blockIdx.x] = te;
    }
}

//=============================================================================
// Quantum Correlations Kernels (Simplified)
//=============================================================================

/**
 * @brief Compute Von Neumann entropy from eigenvalues
 *
 * S(rho) = -sum lambda_i * log(lambda_i)
 */
__global__ void kernel_von_neumann_entropy(
    const float* __restrict__ eigenvalues,
    float* __restrict__ partial_entropy,
    uint32_t dim)
{
    extern __shared__ float sdata[];

    uint32_t tid = threadIdx.x;
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t stride = blockDim.x * gridDim.x;

    float local_h = 0.0f;

    for (uint32_t i = idx; i < dim; i += stride) {
        float lambda = eigenvalues[i];
        if (lambda > INFO_GPU_EPSILON) {
            local_h -= lambda * safe_log2_device(lambda);
        }
    }

    float h = block_reduce_sum(local_h, sdata);

    if (tid == 0) {
        partial_entropy[blockIdx.x] = h;
    }
}

/**
 * @brief Compute partial trace of density matrix (trace over B)
 *
 * rho_A[i,j] = sum_k rho_AB[i*dim_b+k, j*dim_b+k]
 */
__global__ void kernel_partial_trace_b(
    const float* __restrict__ rho_ab,
    float* __restrict__ rho_a,
    uint32_t dim_a,
    uint32_t dim_b)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t total = dim_a * dim_a;

    if (idx < total) {
        uint32_t i = idx / dim_a;
        uint32_t j = idx % dim_a;

        uint32_t dim_ab = dim_a * dim_b;
        float sum = 0.0f;

        for (uint32_t k = 0; k < dim_b; k++) {
            uint32_t row = i * dim_b + k;
            uint32_t col = j * dim_b + k;
            sum += rho_ab[row * dim_ab + col];
        }

        rho_a[idx] = sum;
    }
}

//=============================================================================
// Phi (Integrated Information) Kernels
//=============================================================================

/**
 * @brief Power iteration step for stationary distribution
 *
 * p_new = TPM^T * p_old
 */
__global__ void kernel_power_iteration(
    const float* __restrict__ tpm,
    const float* __restrict__ p_old,
    float* __restrict__ p_new,
    uint32_t n_states)
{
    uint32_t j = blockIdx.x * blockDim.x + threadIdx.x;

    if (j < n_states) {
        float sum = 0.0f;
        for (uint32_t i = 0; i < n_states; i++) {
            sum += tpm[i * n_states + j] * p_old[i];
        }
        p_new[j] = sum;
    }
}

/**
 * @brief Normalize probability distribution
 */
__global__ void kernel_normalize_prob(
    float* __restrict__ prob,
    uint32_t n,
    float* __restrict__ sum_out)
{
    extern __shared__ float sdata[];

    uint32_t tid = threadIdx.x;
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t stride = blockDim.x * gridDim.x;

    float local_sum = 0.0f;
    for (uint32_t i = idx; i < n; i += stride) {
        local_sum += prob[i];
    }

    float sum = block_reduce_sum(local_sum, sdata);

    if (tid == 0 && blockIdx.x == 0) {
        *sum_out = sum;
    }

    __syncthreads();

    /* Second pass to normalize */
    float norm = (sum > INFO_GPU_EPSILON) ? 1.0f / sum : 0.0f;
    for (uint32_t i = idx; i < n; i += stride) {
        prob[i] *= norm;
    }
}

/**
 * @brief Compute effective information from TPM
 *
 * EI = H(output | uniform input) - H(output | state input)
 */
__global__ void kernel_effective_information(
    const float* __restrict__ tpm,
    const float* __restrict__ stationary,
    float* __restrict__ partial_ei,
    uint32_t n_states)
{
    extern __shared__ float sdata[];

    uint32_t tid = threadIdx.x;
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t stride = blockDim.x * gridDim.x;

    float local_ei = 0.0f;

    /* Compute H(X_t | X_{t-1}) = sum_i P(X_{t-1}=i) * H(X_t | X_{t-1}=i) */
    for (uint32_t i = idx; i < n_states; i += stride) {
        float p_i = stationary[i];
        if (p_i > INFO_GPU_EPSILON) {
            /* Entropy of row i of TPM */
            float h_row = 0.0f;
            for (uint32_t j = 0; j < n_states; j++) {
                float p_ij = tpm[i * n_states + j];
                if (p_ij > INFO_GPU_EPSILON) {
                    h_row -= p_ij * safe_log2_device(p_ij);
                }
            }
            local_ei += p_i * h_row;
        }
    }

    float ei = block_reduce_sum(local_ei, sdata);

    if (tid == 0) {
        partial_ei[blockIdx.x] = ei;
    }
}

//=============================================================================
// Host API
//=============================================================================

extern "C" {

/**
 * @brief GPU-accelerated Shannon entropy computation
 */
int nimcp_info_gpu_shannon_entropy(
    const float* d_probabilities,
    uint32_t n,
    float* result,
    cudaStream_t stream)
{
    /* Initialize GPU recovery if not already done */
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!d_probabilities || !result) {
        set_info_gpu_error("NULL argument");
        NIMCP_THROW_GPU(NIMCP_ERROR_NULL_POINTER, 0, 0,
            "NULL argument in nimcp_info_gpu_shannon_entropy");
        return -1;
    }

    uint32_t n_blocks = (n + INFO_GPU_BLOCK_SIZE - 1) / INFO_GPU_BLOCK_SIZE;
    n_blocks = (n_blocks > 256) ? 256 : n_blocks;

    float* d_partial;
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_partial, n_blocks * sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);

    size_t shared_size = INFO_GPU_BLOCK_SIZE / INFO_GPU_WARP_SIZE * sizeof(float);

    kernel_shannon_entropy<<<n_blocks, INFO_GPU_BLOCK_SIZE, shared_size, stream>>>(
        d_probabilities, d_partial, n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    /* Final reduction on host */
    float* h_partial = (float*)nimcp_malloc(n_blocks * sizeof(float));
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(h_partial, d_partial, n_blocks * sizeof(float),
                                      cudaMemcpyDeviceToHost, stream), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(stream), GPU_ERROR_CUDA_RUNTIME);

    float h = 0.0f;
    for (uint32_t i = 0; i < n_blocks; i++) {
        h += h_partial[i];
    }

    *result = h;

    nimcp_free(h_partial);
    cudaFree(d_partial);

    return 0;
}

/**
 * @brief GPU-accelerated Renyi entropy computation
 */
int nimcp_info_gpu_renyi_entropy(
    const float* d_probabilities,
    uint32_t n,
    float alpha,
    float* result,
    cudaStream_t stream)
{
    /* Initialize GPU recovery if not already done */
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!d_probabilities || !result) {
        set_info_gpu_error("NULL argument");
        NIMCP_THROW_GPU(NIMCP_ERROR_NULL_POINTER, 0, 0,
            "NULL argument in nimcp_info_gpu_renyi_entropy");
        return -1;
    }

    if (fabsf(alpha - 1.0f) < 1e-6f) {
        /* Use Shannon entropy for alpha close to 1 */
        return nimcp_info_gpu_shannon_entropy(d_probabilities, n, result, stream);
    }

    uint32_t n_blocks = (n + INFO_GPU_BLOCK_SIZE - 1) / INFO_GPU_BLOCK_SIZE;
    n_blocks = (n_blocks > 256) ? 256 : n_blocks;

    float* d_partial;
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_partial, n_blocks * sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);

    size_t shared_size = INFO_GPU_BLOCK_SIZE / INFO_GPU_WARP_SIZE * sizeof(float);

    kernel_renyi_entropy<<<n_blocks, INFO_GPU_BLOCK_SIZE, shared_size, stream>>>(
        d_probabilities, d_partial, n, alpha);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    float* h_partial = (float*)nimcp_malloc(n_blocks * sizeof(float));
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(h_partial, d_partial, n_blocks * sizeof(float),
                                      cudaMemcpyDeviceToHost, stream), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(stream), GPU_ERROR_CUDA_RUNTIME);

    float sum = 0.0f;
    for (uint32_t i = 0; i < n_blocks; i++) {
        sum += h_partial[i];
    }

    /* H_alpha = (1/(1-alpha)) * log(sum) */
    *result = (sum > INFO_GPU_EPSILON) ? log2f(sum) / (1.0f - alpha) : 0.0f;

    nimcp_free(h_partial);
    cudaFree(d_partial);

    return 0;
}

/**
 * @brief GPU-accelerated mutual information computation
 */
int nimcp_info_gpu_mutual_information(
    const float* d_joint_prob,
    uint32_t n_x,
    uint32_t n_y,
    float* result,
    cudaStream_t stream)
{
    if (!d_joint_prob || !result) {
        set_info_gpu_error("NULL argument");
        return -1;
    }

    /* Allocate marginals */
    float* d_marginal_x;
    float* d_marginal_y;
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_marginal_x, n_x * sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_marginal_y, n_y * sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);

    uint32_t max_dim = (n_x > n_y) ? n_x : n_y;
    uint32_t n_blocks_marginal = (max_dim + INFO_GPU_BLOCK_SIZE - 1) / INFO_GPU_BLOCK_SIZE;

    kernel_extract_marginals<<<n_blocks_marginal, INFO_GPU_BLOCK_SIZE, 0, stream>>>(
        d_joint_prob, d_marginal_x, d_marginal_y, n_x, n_y);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    /* Compute MI */
    uint32_t total = n_x * n_y;
    uint32_t n_blocks = (total + INFO_GPU_BLOCK_SIZE - 1) / INFO_GPU_BLOCK_SIZE;
    n_blocks = (n_blocks > 256) ? 256 : n_blocks;

    float* d_partial;
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_partial, n_blocks * sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);

    size_t shared_size = INFO_GPU_BLOCK_SIZE / INFO_GPU_WARP_SIZE * sizeof(float);

    kernel_mutual_information<<<n_blocks, INFO_GPU_BLOCK_SIZE, shared_size, stream>>>(
        d_joint_prob, d_marginal_x, d_marginal_y, d_partial, n_x, n_y);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    float* h_partial = (float*)nimcp_malloc(n_blocks * sizeof(float));
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(h_partial, d_partial, n_blocks * sizeof(float),
                                      cudaMemcpyDeviceToHost, stream), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(stream), GPU_ERROR_CUDA_RUNTIME);

    float mi = 0.0f;
    for (uint32_t i = 0; i < n_blocks; i++) {
        mi += h_partial[i];
    }

    *result = mi;

    nimcp_free(h_partial);
    cudaFree(d_partial);
    cudaFree(d_marginal_x);
    cudaFree(d_marginal_y);

    return 0;
}

/**
 * @brief GPU-accelerated discretization
 */
int nimcp_info_gpu_discretize(
    const float* d_data,
    uint32_t n,
    uint32_t n_bins,
    uint32_t* d_bins,
    float* min_val_out,
    float* max_val_out,
    cudaStream_t stream)
{
    if (!d_data || !d_bins) {
        set_info_gpu_error("NULL argument");
        return -1;
    }

    /* Find min/max */
    uint32_t n_blocks = (n + INFO_GPU_BLOCK_SIZE - 1) / INFO_GPU_BLOCK_SIZE;
    n_blocks = (n_blocks > 256) ? 256 : n_blocks;

    float* d_partial_min;
    float* d_partial_max;
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_partial_min, n_blocks * sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_partial_max, n_blocks * sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);

    size_t shared_size = 2 * INFO_GPU_BLOCK_SIZE * sizeof(float);

    kernel_find_minmax<<<n_blocks, INFO_GPU_BLOCK_SIZE, shared_size, stream>>>(
        d_data, d_partial_min, d_partial_max, n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    /* Final reduction on host */
    float* h_partial_min = (float*)nimcp_malloc(n_blocks * sizeof(float));
    float* h_partial_max = (float*)nimcp_malloc(n_blocks * sizeof(float));
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(h_partial_min, d_partial_min, n_blocks * sizeof(float),
                                      cudaMemcpyDeviceToHost, stream), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(h_partial_max, d_partial_max, n_blocks * sizeof(float),
                                      cudaMemcpyDeviceToHost, stream), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(stream), GPU_ERROR_CUDA_RUNTIME);

    float min_val = h_partial_min[0];
    float max_val = h_partial_max[0];
    for (uint32_t i = 1; i < n_blocks; i++) {
        min_val = fminf(min_val, h_partial_min[i]);
        max_val = fmaxf(max_val, h_partial_max[i]);
    }

    nimcp_free(h_partial_min);
    nimcp_free(h_partial_max);
    cudaFree(d_partial_min);
    cudaFree(d_partial_max);

    /* Compute bin width and discretize */
    float range = max_val - min_val;
    float bin_width = (range > INFO_GPU_EPSILON) ? range / n_bins : 1.0f;

    kernel_discretize<<<n_blocks, INFO_GPU_BLOCK_SIZE, 0, stream>>>(
        d_data, d_bins, n, min_val, bin_width, n_bins);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    if (min_val_out) *min_val_out = min_val;
    if (max_val_out) *max_val_out = max_val;

    return 0;
}

/**
 * @brief GPU-accelerated KL divergence
 */
int nimcp_info_gpu_kl_divergence(
    const float* d_p,
    const float* d_q,
    uint32_t n,
    float* result,
    cudaStream_t stream)
{
    if (!d_p || !d_q || !result) {
        set_info_gpu_error("NULL argument");
        return -1;
    }

    uint32_t n_blocks = (n + INFO_GPU_BLOCK_SIZE - 1) / INFO_GPU_BLOCK_SIZE;
    n_blocks = (n_blocks > 256) ? 256 : n_blocks;

    float* d_partial;
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_partial, n_blocks * sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);

    size_t shared_size = INFO_GPU_BLOCK_SIZE / INFO_GPU_WARP_SIZE * sizeof(float);

    kernel_kl_divergence<<<n_blocks, INFO_GPU_BLOCK_SIZE, shared_size, stream>>>(
        d_p, d_q, d_partial, n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    float* h_partial = (float*)nimcp_malloc(n_blocks * sizeof(float));
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(h_partial, d_partial, n_blocks * sizeof(float),
                                      cudaMemcpyDeviceToHost, stream), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(stream), GPU_ERROR_CUDA_RUNTIME);

    float kl = 0.0f;
    for (uint32_t i = 0; i < n_blocks; i++) {
        kl += h_partial[i];
        if (isinf(h_partial[i])) {
            kl = INFINITY;
            break;
        }
    }

    *result = kl;

    nimcp_free(h_partial);
    cudaFree(d_partial);

    return 0;
}

/**
 * @brief Get last GPU error message
 */
const char* nimcp_info_gpu_get_error(void) {
    return g_info_gpu_error;
}

} /* extern "C" */

#else /* !NIMCP_ENABLE_CUDA */

/* Stub implementations when CUDA is not available */
extern "C" {

int nimcp_info_gpu_shannon_entropy(
    const float* d_probabilities,
    uint32_t n,
    float* result,
    void* stream)
{
    (void)d_probabilities; (void)n; (void)result; (void)stream;
    return -1;
}

int nimcp_info_gpu_renyi_entropy(
    const float* d_probabilities,
    uint32_t n,
    float alpha,
    float* result,
    void* stream)
{
    (void)d_probabilities; (void)n; (void)alpha; (void)result; (void)stream;
    return -1;
}

int nimcp_info_gpu_mutual_information(
    const float* d_joint_prob,
    uint32_t n_x,
    uint32_t n_y,
    float* result,
    void* stream)
{
    (void)d_joint_prob; (void)n_x; (void)n_y; (void)result; (void)stream;
    return -1;
}

int nimcp_info_gpu_discretize(
    const float* d_data,
    uint32_t n,
    uint32_t n_bins,
    uint32_t* d_bins,
    float* min_val_out,
    float* max_val_out,
    void* stream)
{
    (void)d_data; (void)n; (void)n_bins; (void)d_bins;
    (void)min_val_out; (void)max_val_out; (void)stream;
    return -1;
}

int nimcp_info_gpu_kl_divergence(
    const float* d_p,
    const float* d_q,
    uint32_t n,
    float* result,
    void* stream)
{
    (void)d_p; (void)d_q; (void)n; (void)result; (void)stream;
    return -1;
}

const char* nimcp_info_gpu_get_error(void) {
    return "CUDA not enabled";
}

} /* extern "C" */

#endif /* NIMCP_ENABLE_CUDA */
