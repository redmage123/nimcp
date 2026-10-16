/**
 * @file nimcp_streaming_statistics_kernels.cu
 * @brief CUDA GPU Kernels for Streaming Statistics
 * @version 1.0.0
 * @date 2026-01-30
 *
 * WHAT: GPU-accelerated kernels for streaming statistical computation
 * WHY:  Enable massive parallelism for batch statistical updates
 * HOW:  CUDA parallel reduction, warp-level primitives, shared memory
 *
 * NEUROSCIENCE MOTIVATION:
 * Neural data streams (spike trains, LFP, fMRI) generate terabytes
 * of data requiring real-time statistical monitoring. GPU acceleration
 * enables millisecond-latency statistical updates on millions of data points.
 *
 * @author NIMCP Development Team
 */

#ifdef NIMCP_ENABLE_CUDA

#include <cuda_runtime.h>
#include <cuda_fp16.h>
#include <cooperative_groups.h>
#include <float.h>
#include <math.h>

#include "utils/statistics/nimcp_streaming_statistics.h"
#include "utils/logging/nimcp_logging.h"
#include "gpu/common/nimcp_cuda_utils.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "utils/exception/nimcp_exception_macros.h"

namespace cg = cooperative_groups;

#define LOG_MODULE "STREAM_STATS_GPU"

//=============================================================================
// Kernel Configuration
//=============================================================================

#define BLOCK_SIZE 256
#define WARP_SIZE 32
#define MAX_BLOCKS 1024

/* Calculate grid dimensions */
#define GRID_SIZE(n) (min(((n) + BLOCK_SIZE - 1) / BLOCK_SIZE, (unsigned)MAX_BLOCKS))

//=============================================================================
// Warp-Level Reduction Primitives
//=============================================================================

/**
 * @brief Warp-level parallel sum reduction
 *
 * Uses warp shuffle intrinsics for efficient intra-warp communication.
 * All threads in warp must participate.
 */
__device__ __forceinline__ float warp_reduce_sum(float val)
{
    for (int offset = WARP_SIZE / 2; offset > 0; offset /= 2) {
        val += __shfl_down_sync(0xffffffff, val, offset);
    }
    return val;
}

/**
 * @brief Warp-level parallel minimum reduction
 */
__device__ __forceinline__ float warp_reduce_min(float val)
{
    for (int offset = WARP_SIZE / 2; offset > 0; offset /= 2) {
        float other = __shfl_down_sync(0xffffffff, val, offset);
        val = fminf(val, other);
    }
    return val;
}

/**
 * @brief Warp-level parallel maximum reduction
 */
__device__ __forceinline__ float warp_reduce_max(float val)
{
    for (int offset = WARP_SIZE / 2; offset > 0; offset /= 2) {
        float other = __shfl_down_sync(0xffffffff, val, offset);
        val = fmaxf(val, other);
    }
    return val;
}

/**
 * @brief Warp-level parallel sum of squares reduction
 * Returns both sum and sum of squares
 */
__device__ __forceinline__ void warp_reduce_sum_sq(float& sum, float& sum_sq)
{
    for (int offset = WARP_SIZE / 2; offset > 0; offset /= 2) {
        sum += __shfl_down_sync(0xffffffff, sum, offset);
        sum_sq += __shfl_down_sync(0xffffffff, sum_sq, offset);
    }
}

//=============================================================================
// Block-Level Reduction with Shared Memory
//=============================================================================

/**
 * @brief Shared memory structure for statistics reduction
 */
struct __align__(16) BlockStats {
    float sum;
    float sum_sq;
    float min_val;
    float max_val;
    unsigned int count;
};

/**
 * @brief Block-level statistics reduction
 *
 * Computes partial sum, sum of squares, min, and max for a block.
 * Uses shared memory for inter-warp communication.
 */
__device__ void block_reduce_stats(
    float val,
    BlockStats* shared,
    unsigned int tid,
    unsigned int block_size)
{
    /* First, compute warp-level reductions */
    int lane = tid % WARP_SIZE;
    int warp_id = tid / WARP_SIZE;
    int num_warps = (block_size + WARP_SIZE - 1) / WARP_SIZE;

    float sum = val;
    float sum_sq = val * val;
    float min_v = val;
    float max_v = val;

    /* Warp-level reduction */
    sum = warp_reduce_sum(sum);
    sum_sq = warp_reduce_sum(sum_sq);
    min_v = warp_reduce_min(min_v);
    max_v = warp_reduce_max(max_v);

    /* Store warp results to shared memory */
    if (lane == 0) {
        shared[warp_id].sum = sum;
        shared[warp_id].sum_sq = sum_sq;
        shared[warp_id].min_val = min_v;
        shared[warp_id].max_val = max_v;
        shared[warp_id].count = WARP_SIZE;
    }

    __syncthreads();

    /* Final reduction by first warp */
    if (warp_id == 0 && lane < num_warps) {
        sum = shared[lane].sum;
        sum_sq = shared[lane].sum_sq;
        min_v = shared[lane].min_val;
        max_v = shared[lane].max_val;

        /* Reduce across warps */
        sum = warp_reduce_sum(sum);
        sum_sq = warp_reduce_sum(sum_sq);
        min_v = warp_reduce_min(min_v);
        max_v = warp_reduce_max(max_v);

        if (lane == 0) {
            shared[0].sum = sum;
            shared[0].sum_sq = sum_sq;
            shared[0].min_val = min_v;
            shared[0].max_val = max_v;
            shared[0].count = block_size;
        }
    }
}

//=============================================================================
// Statistics Kernels
//=============================================================================

/**
 * @brief Kernel for computing batch statistics (mean, variance, min, max)
 *
 * Two-pass algorithm:
 * Pass 1: Compute partial reductions per block
 * Pass 2: Combine block results (done on CPU or separate kernel)
 *
 * @param values Input array (device memory)
 * @param count Number of elements
 * @param partial_sums Output: partial sums per block
 * @param partial_sum_sqs Output: partial sum of squares per block
 * @param partial_mins Output: partial minimums per block
 * @param partial_maxs Output: partial maximums per block
 * @param partial_counts Output: count per block
 */
__global__ void kernel_stream_stats_reduce(
    const float* __restrict__ values,
    unsigned int count,
    float* partial_sums,
    float* partial_sum_sqs,
    float* partial_mins,
    float* partial_maxs,
    unsigned int* partial_counts)
{
    __shared__ BlockStats shared[BLOCK_SIZE / WARP_SIZE];

    unsigned int tid = threadIdx.x;
    unsigned int global_id = blockIdx.x * blockDim.x + threadIdx.x;
    unsigned int grid_stride = blockDim.x * gridDim.x;

    /* Initialize accumulators */
    float sum = 0.0f;
    float sum_sq = 0.0f;
    float min_v = FLT_MAX;
    float max_v = -FLT_MAX;
    unsigned int local_count = 0;

    /* Grid-stride loop for handling large arrays */
    for (unsigned int i = global_id; i < count; i += grid_stride) {
        float val = values[i];
        sum += val;
        sum_sq += val * val;
        min_v = fminf(min_v, val);
        max_v = fmaxf(max_v, val);
        local_count++;
    }

    /* Warp-level reduction */
    int lane = tid % WARP_SIZE;
    int warp_id = tid / WARP_SIZE;
    int num_warps = BLOCK_SIZE / WARP_SIZE;

    sum = warp_reduce_sum(sum);
    sum_sq = warp_reduce_sum(sum_sq);
    min_v = warp_reduce_min(min_v);
    max_v = warp_reduce_max(max_v);
    local_count = (unsigned int)warp_reduce_sum((float)local_count);

    if (lane == 0) {
        shared[warp_id].sum = sum;
        shared[warp_id].sum_sq = sum_sq;
        shared[warp_id].min_val = min_v;
        shared[warp_id].max_val = max_v;
        shared[warp_id].count = local_count;
    }

    __syncthreads();

    /* Block-level reduction by first warp */
    if (warp_id == 0) {
        sum = (lane < num_warps) ? shared[lane].sum : 0.0f;
        sum_sq = (lane < num_warps) ? shared[lane].sum_sq : 0.0f;
        min_v = (lane < num_warps) ? shared[lane].min_val : FLT_MAX;
        max_v = (lane < num_warps) ? shared[lane].max_val : -FLT_MAX;
        local_count = (lane < num_warps) ? shared[lane].count : 0;

        sum = warp_reduce_sum(sum);
        sum_sq = warp_reduce_sum(sum_sq);
        min_v = warp_reduce_min(min_v);
        max_v = warp_reduce_max(max_v);
        local_count = (unsigned int)warp_reduce_sum((float)local_count);

        if (lane == 0) {
            partial_sums[blockIdx.x] = sum;
            partial_sum_sqs[blockIdx.x] = sum_sq;
            partial_mins[blockIdx.x] = min_v;
            partial_maxs[blockIdx.x] = max_v;
            partial_counts[blockIdx.x] = local_count;
        }
    }
}

/**
 * @brief Final reduction kernel to combine block results
 */
__global__ void kernel_stream_stats_final(
    const float* partial_sums,
    const float* partial_sum_sqs,
    const float* partial_mins,
    const float* partial_maxs,
    const unsigned int* partial_counts,
    unsigned int num_blocks,
    float* out_mean,
    float* out_variance,
    float* out_min,
    float* out_max)
{
    __shared__ float s_sum[WARP_SIZE];
    __shared__ float s_sum_sq[WARP_SIZE];
    __shared__ float s_min[WARP_SIZE];
    __shared__ float s_max[WARP_SIZE];
    __shared__ unsigned int s_count[WARP_SIZE];

    int tid = threadIdx.x;

    float sum = 0.0f;
    float sum_sq = 0.0f;
    float min_v = FLT_MAX;
    float max_v = -FLT_MAX;
    unsigned int count = 0;

    /* Load and reduce partial results */
    for (int i = tid; i < num_blocks; i += blockDim.x) {
        sum += partial_sums[i];
        sum_sq += partial_sum_sqs[i];
        min_v = fminf(min_v, partial_mins[i]);
        max_v = fmaxf(max_v, partial_maxs[i]);
        count += partial_counts[i];
    }

    /* Warp reduction */
    sum = warp_reduce_sum(sum);
    sum_sq = warp_reduce_sum(sum_sq);
    min_v = warp_reduce_min(min_v);
    max_v = warp_reduce_max(max_v);
    count = (unsigned int)warp_reduce_sum((float)count);

    /* Store warp results */
    if (tid % WARP_SIZE == 0) {
        int warp_idx = tid / WARP_SIZE;
        s_sum[warp_idx] = sum;
        s_sum_sq[warp_idx] = sum_sq;
        s_min[warp_idx] = min_v;
        s_max[warp_idx] = max_v;
        s_count[warp_idx] = count;
    }

    __syncthreads();

    /* Final reduction by thread 0 */
    if (tid == 0) {
        int num_warps = (blockDim.x + WARP_SIZE - 1) / WARP_SIZE;
        sum = 0.0f;
        sum_sq = 0.0f;
        min_v = FLT_MAX;
        max_v = -FLT_MAX;
        count = 0;

        for (int i = 0; i < num_warps; i++) {
            sum += s_sum[i];
            sum_sq += s_sum_sq[i];
            min_v = fminf(min_v, s_min[i]);
            max_v = fmaxf(max_v, s_max[i]);
            count += s_count[i];
        }

        float mean = sum / count;
        float variance = (sum_sq / count) - (mean * mean);
        /* Bessel's correction for sample variance */
        variance = variance * count / (count - 1);

        if (out_mean) *out_mean = mean;
        if (out_variance) *out_variance = variance;
        if (out_min) *out_min = min_v;
        if (out_max) *out_max = max_v;
    }
}

//=============================================================================
// Covariance Kernels
//=============================================================================

/**
 * @brief Kernel for computing batch covariance
 *
 * Computes partial sums needed for covariance:
 * - sum_x, sum_y
 * - sum_xy (product sum)
 * - count
 */
__global__ void kernel_stream_cov_reduce(
    const float* __restrict__ x,
    const float* __restrict__ y,
    unsigned int count,
    float* partial_sum_x,
    float* partial_sum_y,
    float* partial_sum_xy,
    float* partial_sum_x2,
    float* partial_sum_y2,
    unsigned int* partial_counts)
{
    unsigned int tid = threadIdx.x;
    unsigned int global_id = blockIdx.x * blockDim.x + threadIdx.x;
    unsigned int grid_stride = blockDim.x * gridDim.x;

    float sum_x = 0.0f;
    float sum_y = 0.0f;
    float sum_xy = 0.0f;
    float sum_x2 = 0.0f;
    float sum_y2 = 0.0f;
    unsigned int local_count = 0;

    for (unsigned int i = global_id; i < count; i += grid_stride) {
        float xi = x[i];
        float yi = y[i];
        sum_x += xi;
        sum_y += yi;
        sum_xy += xi * yi;
        sum_x2 += xi * xi;
        sum_y2 += yi * yi;
        local_count++;
    }

    /* Shared memory for warp reduction results */
    __shared__ float s_sum_x[BLOCK_SIZE / WARP_SIZE];
    __shared__ float s_sum_y[BLOCK_SIZE / WARP_SIZE];
    __shared__ float s_sum_xy[BLOCK_SIZE / WARP_SIZE];
    __shared__ float s_sum_x2[BLOCK_SIZE / WARP_SIZE];
    __shared__ float s_sum_y2[BLOCK_SIZE / WARP_SIZE];
    __shared__ unsigned int s_count[BLOCK_SIZE / WARP_SIZE];

    int lane = tid % WARP_SIZE;
    int warp_id = tid / WARP_SIZE;
    int num_warps = BLOCK_SIZE / WARP_SIZE;

    /* Warp reduction */
    sum_x = warp_reduce_sum(sum_x);
    sum_y = warp_reduce_sum(sum_y);
    sum_xy = warp_reduce_sum(sum_xy);
    sum_x2 = warp_reduce_sum(sum_x2);
    sum_y2 = warp_reduce_sum(sum_y2);
    local_count = (unsigned int)warp_reduce_sum((float)local_count);

    if (lane == 0) {
        s_sum_x[warp_id] = sum_x;
        s_sum_y[warp_id] = sum_y;
        s_sum_xy[warp_id] = sum_xy;
        s_sum_x2[warp_id] = sum_x2;
        s_sum_y2[warp_id] = sum_y2;
        s_count[warp_id] = local_count;
    }

    __syncthreads();

    /* Block reduction by first warp */
    if (warp_id == 0) {
        sum_x = (lane < num_warps) ? s_sum_x[lane] : 0.0f;
        sum_y = (lane < num_warps) ? s_sum_y[lane] : 0.0f;
        sum_xy = (lane < num_warps) ? s_sum_xy[lane] : 0.0f;
        sum_x2 = (lane < num_warps) ? s_sum_x2[lane] : 0.0f;
        sum_y2 = (lane < num_warps) ? s_sum_y2[lane] : 0.0f;
        local_count = (lane < num_warps) ? s_count[lane] : 0;

        sum_x = warp_reduce_sum(sum_x);
        sum_y = warp_reduce_sum(sum_y);
        sum_xy = warp_reduce_sum(sum_xy);
        sum_x2 = warp_reduce_sum(sum_x2);
        sum_y2 = warp_reduce_sum(sum_y2);
        local_count = (unsigned int)warp_reduce_sum((float)local_count);

        if (lane == 0) {
            partial_sum_x[blockIdx.x] = sum_x;
            partial_sum_y[blockIdx.x] = sum_y;
            partial_sum_xy[blockIdx.x] = sum_xy;
            partial_sum_x2[blockIdx.x] = sum_x2;
            partial_sum_y2[blockIdx.x] = sum_y2;
            partial_counts[blockIdx.x] = local_count;
        }
    }
}

/**
 * @brief Final covariance reduction kernel
 */
__global__ void kernel_stream_cov_final(
    const float* partial_sum_x,
    const float* partial_sum_y,
    const float* partial_sum_xy,
    const float* partial_sum_x2,
    const float* partial_sum_y2,
    const unsigned int* partial_counts,
    unsigned int num_blocks,
    float* out_covariance,
    float* out_correlation)
{
    int tid = threadIdx.x;

    float sum_x = 0.0f;
    float sum_y = 0.0f;
    float sum_xy = 0.0f;
    float sum_x2 = 0.0f;
    float sum_y2 = 0.0f;
    unsigned int count = 0;

    for (int i = tid; i < num_blocks; i += blockDim.x) {
        sum_x += partial_sum_x[i];
        sum_y += partial_sum_y[i];
        sum_xy += partial_sum_xy[i];
        sum_x2 += partial_sum_x2[i];
        sum_y2 += partial_sum_y2[i];
        count += partial_counts[i];
    }

    /* Warp reduction */
    sum_x = warp_reduce_sum(sum_x);
    sum_y = warp_reduce_sum(sum_y);
    sum_xy = warp_reduce_sum(sum_xy);
    sum_x2 = warp_reduce_sum(sum_x2);
    sum_y2 = warp_reduce_sum(sum_y2);
    count = (unsigned int)warp_reduce_sum((float)count);

    /* Shared memory for final reduction */
    __shared__ float s_values[6][WARP_SIZE];

    if (tid % WARP_SIZE == 0) {
        int warp_idx = tid / WARP_SIZE;
        s_values[0][warp_idx] = sum_x;
        s_values[1][warp_idx] = sum_y;
        s_values[2][warp_idx] = sum_xy;
        s_values[3][warp_idx] = sum_x2;
        s_values[4][warp_idx] = sum_y2;
        s_values[5][warp_idx] = (float)count;
    }

    __syncthreads();

    if (tid == 0) {
        int num_warps = (blockDim.x + WARP_SIZE - 1) / WARP_SIZE;
        sum_x = 0.0f;
        sum_y = 0.0f;
        sum_xy = 0.0f;
        sum_x2 = 0.0f;
        sum_y2 = 0.0f;
        count = 0;

        for (int i = 0; i < num_warps; i++) {
            sum_x += s_values[0][i];
            sum_y += s_values[1][i];
            sum_xy += s_values[2][i];
            sum_x2 += s_values[3][i];
            sum_y2 += s_values[4][i];
            count += (unsigned int)s_values[5][i];
        }

        float n = (float)count;
        float mean_x = sum_x / n;
        float mean_y = sum_y / n;

        /* Covariance: E[XY] - E[X]E[Y] */
        float covariance = (sum_xy / n) - (mean_x * mean_y);
        /* Sample covariance with Bessel's correction */
        covariance = covariance * n / (n - 1.0f);

        if (out_covariance) *out_covariance = covariance;

        if (out_correlation) {
            /* Correlation: Cov(X,Y) / (std(X) * std(Y)) */
            float var_x = (sum_x2 / n) - (mean_x * mean_x);
            float var_y = (sum_y2 / n) - (mean_y * mean_y);
            float std_x = sqrtf(var_x);
            float std_y = sqrtf(var_y);
            *out_correlation = covariance / (std_x * std_y);
        }
    }
}

//=============================================================================
// Welford Update Kernel
//=============================================================================

/**
 * @brief Structure for parallel Welford accumulator
 */
struct WelfordState {
    double mean;
    double m2;
    unsigned long long n;
};

/**
 * @brief Kernel for parallel Welford mean/variance update
 *
 * Each thread maintains a local Welford state, then states are merged.
 * Uses Chan et al. parallel combination formula for correctness.
 */
__global__ void kernel_welford_update(
    const float* __restrict__ values,
    unsigned int count,
    WelfordState* block_states)
{
    unsigned int tid = threadIdx.x;
    unsigned int global_id = blockIdx.x * blockDim.x + threadIdx.x;
    unsigned int grid_stride = blockDim.x * gridDim.x;

    /* Local Welford state */
    double mean = 0.0;
    double m2 = 0.0;
    unsigned long long n = 0;

    /* Process elements with grid stride */
    for (unsigned int i = global_id; i < count; i += grid_stride) {
        double x = (double)values[i];
        n++;
        double delta = x - mean;
        mean += delta / (double)n;
        double delta2 = x - mean;
        m2 += delta * delta2;
    }

    /* Store to shared memory for reduction */
    __shared__ double s_mean[BLOCK_SIZE];
    __shared__ double s_m2[BLOCK_SIZE];
    __shared__ unsigned long long s_n[BLOCK_SIZE];

    s_mean[tid] = mean;
    s_m2[tid] = m2;
    s_n[tid] = n;

    __syncthreads();

    /* Parallel reduction using Chan's formula */
    for (int stride = blockDim.x / 2; stride > 0; stride >>= 1) {
        if (tid < stride) {
            unsigned long long na = s_n[tid];
            unsigned long long nb = s_n[tid + stride];
            if (nb > 0) {
                if (na == 0) {
                    s_mean[tid] = s_mean[tid + stride];
                    s_m2[tid] = s_m2[tid + stride];
                    s_n[tid] = nb;
                } else {
                    unsigned long long n_total = na + nb;
                    double delta = s_mean[tid + stride] - s_mean[tid];
                    double new_mean = (na * s_mean[tid] + nb * s_mean[tid + stride]) / n_total;
                    double new_m2 = s_m2[tid] + s_m2[tid + stride] +
                                   delta * delta * na * nb / n_total;
                    s_mean[tid] = new_mean;
                    s_m2[tid] = new_m2;
                    s_n[tid] = n_total;
                }
            }
        }
        __syncthreads();
    }

    /* Store block result */
    if (tid == 0) {
        block_states[blockIdx.x].mean = s_mean[0];
        block_states[blockIdx.x].m2 = s_m2[0];
        block_states[blockIdx.x].n = s_n[0];
    }
}

//=============================================================================
// HyperLogLog GPU Kernels
//=============================================================================

/**
 * @brief Hash function for HLL (MurmurHash3 finalizer)
 */
__device__ __forceinline__ uint64_t hll_hash(uint64_t key)
{
    key ^= key >> 33;
    key *= 0xff51afd7ed558ccdULL;
    key ^= key >> 33;
    key *= 0xc4ceb9fe1a85ec53ULL;
    key ^= key >> 33;
    return key;
}

/**
 * @brief Count leading zeros for 64-bit integer
 */
__device__ __forceinline__ int clz64_device(uint64_t x)
{
    return (x == 0) ? 64 : __clzll(x);
}

/**
 * @brief Kernel for batch HLL insertion
 *
 * Each thread processes multiple items, updating buckets atomically.
 */
__global__ void kernel_hll_add_batch(
    const uint64_t* __restrict__ items,
    unsigned int count,
    uint8_t* buckets,
    unsigned int precision)
{
    unsigned int global_id = blockIdx.x * blockDim.x + threadIdx.x;
    unsigned int grid_stride = blockDim.x * gridDim.x;

    unsigned int bucket_mask = (1U << precision) - 1;

    for (unsigned int i = global_id; i < count; i += grid_stride) {
        uint64_t hash = hll_hash(items[i]);

        /* Extract bucket index */
        unsigned int idx = (hash >> (64 - precision)) & bucket_mask;

        /* Count leading zeros in remaining bits */
        uint64_t remaining = hash << precision | (1ULL << (precision - 1));
        int rho = clz64_device(remaining) + 1;

        /* Atomic max update */
        atomicMax((int*)&buckets[idx], rho);
    }
}

/**
 * @brief Kernel for HLL cardinality estimation
 *
 * Uses harmonic mean formula with bias correction.
 */
__global__ void kernel_hll_count(
    const uint8_t* __restrict__ buckets,
    unsigned int n_buckets,
    double* out_estimate)
{
    __shared__ double s_sum[BLOCK_SIZE];
    __shared__ unsigned int s_zeros[BLOCK_SIZE];

    unsigned int tid = threadIdx.x;
    unsigned int global_id = blockIdx.x * blockDim.x + threadIdx.x;
    unsigned int grid_stride = blockDim.x * gridDim.x;

    double local_sum = 0.0;
    unsigned int local_zeros = 0;

    for (unsigned int i = global_id; i < n_buckets; i += grid_stride) {
        uint8_t val = buckets[i];
        local_sum += 1.0 / (double)(1ULL << val);
        if (val == 0) local_zeros++;
    }

    s_sum[tid] = local_sum;
    s_zeros[tid] = local_zeros;

    __syncthreads();

    /* Reduction */
    for (int stride = blockDim.x / 2; stride > 0; stride >>= 1) {
        if (tid < stride) {
            s_sum[tid] += s_sum[tid + stride];
            s_zeros[tid] += s_zeros[tid + stride];
        }
        __syncthreads();
    }

    if (tid == 0 && blockIdx.x == 0) {
        double sum = s_sum[0];
        unsigned int zeros = s_zeros[0];

        /* Alpha correction factor */
        double alpha;
        if (n_buckets >= 128) {
            alpha = 0.7213 / (1.0 + 1.079 / n_buckets);
        } else if (n_buckets >= 64) {
            alpha = 0.709;
        } else if (n_buckets >= 32) {
            alpha = 0.697;
        } else {
            alpha = 0.673;
        }

        double estimate = alpha * n_buckets * n_buckets / sum;

        /* Linear counting for small cardinalities */
        if (estimate <= 2.5 * n_buckets && zeros > 0) {
            estimate = n_buckets * log((double)n_buckets / zeros);
        }

        *out_estimate = estimate;
    }
}

//=============================================================================
// Count-Min Sketch GPU Kernels
//=============================================================================

/**
 * @brief Hash function for CMS
 */
__device__ __forceinline__ unsigned int cms_hash_device(
    uint64_t item,
    uint64_t seed,
    unsigned int width)
{
    uint64_t hash = item * seed;
    hash ^= hash >> 33;
    hash *= 0xff51afd7ed558ccdULL;
    hash ^= hash >> 33;
    return (unsigned int)(hash % width);
}

/**
 * @brief Kernel for batch CMS update
 */
__global__ void kernel_cms_update_batch(
    const uint64_t* __restrict__ items,
    const int32_t* __restrict__ counts,
    unsigned int count,
    uint32_t* counters,
    const uint64_t* hash_seeds,
    unsigned int width,
    unsigned int depth)
{
    unsigned int global_id = blockIdx.x * blockDim.x + threadIdx.x;
    unsigned int grid_stride = blockDim.x * gridDim.x;

    for (unsigned int i = global_id; i < count; i += grid_stride) {
        uint64_t item = items[i];
        int32_t c = counts[i];

        for (unsigned int d = 0; d < depth; d++) {
            unsigned int idx = cms_hash_device(item, hash_seeds[d], width);
            atomicAdd(&counters[d * width + idx], c);
        }
    }
}

/**
 * @brief Kernel for batch CMS query
 */
__global__ void kernel_cms_query_batch(
    const uint64_t* __restrict__ items,
    unsigned int count,
    const uint32_t* counters,
    const uint64_t* hash_seeds,
    unsigned int width,
    unsigned int depth,
    uint64_t* results)
{
    unsigned int global_id = blockIdx.x * blockDim.x + threadIdx.x;
    unsigned int grid_stride = blockDim.x * gridDim.x;

    for (unsigned int i = global_id; i < count; i += grid_stride) {
        uint64_t item = items[i];
        uint64_t min_count = UINT64_MAX;

        for (unsigned int d = 0; d < depth; d++) {
            unsigned int idx = cms_hash_device(item, hash_seeds[d], width);
            uint64_t c = counters[d * width + idx];
            if (c < min_count) min_count = c;
        }

        results[i] = min_count;
    }
}

//=============================================================================
// Host-Callable Functions
//=============================================================================

extern "C" {

/**
 * @brief GPU batch statistics computation
 */
nimcp_stream_stats_result_t nimcp_stream_stats_gpu_batch_impl(
    const float* d_values,
    uint32_t count,
    float* d_mean,
    float* d_variance,
    float* d_min,
    float* d_max)
{
    /* Initialize recovery system if needed */
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!d_values || count == 0) {
        return NIMCP_STREAM_ERROR_NULL;
    }

    unsigned int num_blocks = GRID_SIZE(count);

    /* Allocate temporary arrays for partial results */
    float *d_partial_sums, *d_partial_sum_sqs, *d_partial_mins, *d_partial_maxs;
    unsigned int* d_partial_counts;

    cudaError_t err;
    nimcp_gpu_recovery_result_t rec_result = {0};

    err = cudaMalloc(&d_partial_sums, num_blocks * sizeof(float));
    if (err != cudaSuccess) {
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &rec_result)) {
            err = cudaMalloc(&d_partial_sums, num_blocks * sizeof(float));
        }
        if (err != cudaSuccess) return NIMCP_STREAM_ERROR_GPU;
    }

    err = cudaMalloc(&d_partial_sum_sqs, num_blocks * sizeof(float));
    if (err != cudaSuccess) {
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &rec_result)) {
            err = cudaMalloc(&d_partial_sum_sqs, num_blocks * sizeof(float));
        }
        if (err != cudaSuccess) { cudaFree(d_partial_sums); return NIMCP_STREAM_ERROR_GPU; }
    }

    err = cudaMalloc(&d_partial_mins, num_blocks * sizeof(float));
    if (err != cudaSuccess) {
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &rec_result)) {
            err = cudaMalloc(&d_partial_mins, num_blocks * sizeof(float));
        }
        if (err != cudaSuccess) { cudaFree(d_partial_sums); cudaFree(d_partial_sum_sqs); return NIMCP_STREAM_ERROR_GPU; }
    }

    err = cudaMalloc(&d_partial_maxs, num_blocks * sizeof(float));
    if (err != cudaSuccess) {
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &rec_result)) {
            err = cudaMalloc(&d_partial_maxs, num_blocks * sizeof(float));
        }
        if (err != cudaSuccess) { cudaFree(d_partial_sums); cudaFree(d_partial_sum_sqs); cudaFree(d_partial_mins); return NIMCP_STREAM_ERROR_GPU; }
    }

    err = cudaMalloc(&d_partial_counts, num_blocks * sizeof(unsigned int));
    if (err != cudaSuccess) {
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &rec_result)) {
            err = cudaMalloc(&d_partial_counts, num_blocks * sizeof(unsigned int));
        }
        if (err != cudaSuccess) { cudaFree(d_partial_sums); cudaFree(d_partial_sum_sqs); cudaFree(d_partial_mins); cudaFree(d_partial_maxs); return NIMCP_STREAM_ERROR_GPU; }
    }

    /* Launch reduction kernel */
    kernel_stream_stats_reduce<<<num_blocks, BLOCK_SIZE>>>(
        d_values, count,
        d_partial_sums, d_partial_sum_sqs, d_partial_mins, d_partial_maxs, d_partial_counts
    );

    /* Launch final reduction kernel */
    kernel_stream_stats_final<<<1, BLOCK_SIZE>>>(
        d_partial_sums, d_partial_sum_sqs, d_partial_mins, d_partial_maxs, d_partial_counts,
        num_blocks,
        d_mean, d_variance, d_min, d_max
    );

    err = cudaGetLastError();

    /* Cleanup */
    cudaFree(d_partial_sums);
    cudaFree(d_partial_sum_sqs);
    cudaFree(d_partial_mins);
    cudaFree(d_partial_maxs);
    cudaFree(d_partial_counts);

    return (err == cudaSuccess) ? NIMCP_STREAM_OK : NIMCP_STREAM_ERROR_GPU;
}

/**
 * @brief GPU batch covariance computation
 */
nimcp_stream_stats_result_t nimcp_stream_cov_gpu_batch_impl(
    const float* d_x,
    const float* d_y,
    uint32_t count,
    float* d_covariance)
{
    /* Initialize recovery system if needed */
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!d_x || !d_y || !d_covariance || count == 0) {
        return NIMCP_STREAM_ERROR_NULL;
    }

    unsigned int num_blocks = GRID_SIZE(count);

    /* Allocate temporaries */
    float *d_partial_sum_x, *d_partial_sum_y, *d_partial_sum_xy;
    float *d_partial_sum_x2, *d_partial_sum_y2;
    unsigned int* d_partial_counts;

    cudaError_t err;
    nimcp_gpu_recovery_result_t rec_result = {0};
    size_t alloc_size = num_blocks * sizeof(float);

    err = cudaMalloc(&d_partial_sum_x, alloc_size);
    if (err != cudaSuccess) {
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &rec_result)) {
            err = cudaMalloc(&d_partial_sum_x, alloc_size);
        }
        if (err != cudaSuccess) return NIMCP_STREAM_ERROR_GPU;
    }
    err = cudaMalloc(&d_partial_sum_y, alloc_size);
    if (err != cudaSuccess) {
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &rec_result)) {
            err = cudaMalloc(&d_partial_sum_y, alloc_size);
        }
        if (err != cudaSuccess) { cudaFree(d_partial_sum_x); return NIMCP_STREAM_ERROR_GPU; }
    }
    err = cudaMalloc(&d_partial_sum_xy, alloc_size);
    if (err != cudaSuccess) {
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &rec_result)) {
            err = cudaMalloc(&d_partial_sum_xy, alloc_size);
        }
        if (err != cudaSuccess) { cudaFree(d_partial_sum_x); cudaFree(d_partial_sum_y); return NIMCP_STREAM_ERROR_GPU; }
    }
    err = cudaMalloc(&d_partial_sum_x2, alloc_size);
    if (err != cudaSuccess) {
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &rec_result)) {
            err = cudaMalloc(&d_partial_sum_x2, alloc_size);
        }
        if (err != cudaSuccess) { cudaFree(d_partial_sum_x); cudaFree(d_partial_sum_y); cudaFree(d_partial_sum_xy); return NIMCP_STREAM_ERROR_GPU; }
    }
    err = cudaMalloc(&d_partial_sum_y2, alloc_size);
    if (err != cudaSuccess) {
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &rec_result)) {
            err = cudaMalloc(&d_partial_sum_y2, alloc_size);
        }
        if (err != cudaSuccess) { cudaFree(d_partial_sum_x); cudaFree(d_partial_sum_y); cudaFree(d_partial_sum_xy); cudaFree(d_partial_sum_x2); return NIMCP_STREAM_ERROR_GPU; }
    }
    err = cudaMalloc(&d_partial_counts, num_blocks * sizeof(unsigned int));
    if (err != cudaSuccess) {
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &rec_result)) {
            err = cudaMalloc(&d_partial_counts, num_blocks * sizeof(unsigned int));
        }
        if (err != cudaSuccess) { cudaFree(d_partial_sum_x); cudaFree(d_partial_sum_y); cudaFree(d_partial_sum_xy); cudaFree(d_partial_sum_x2); cudaFree(d_partial_sum_y2); return NIMCP_STREAM_ERROR_GPU; }
    }

    /* Launch kernels */
    kernel_stream_cov_reduce<<<num_blocks, BLOCK_SIZE>>>(
        d_x, d_y, count,
        d_partial_sum_x, d_partial_sum_y, d_partial_sum_xy,
        d_partial_sum_x2, d_partial_sum_y2, d_partial_counts
    );

    kernel_stream_cov_final<<<1, BLOCK_SIZE>>>(
        d_partial_sum_x, d_partial_sum_y, d_partial_sum_xy,
        d_partial_sum_x2, d_partial_sum_y2, d_partial_counts,
        num_blocks,
        d_covariance, NULL
    );

    err = cudaGetLastError();

    /* Cleanup */
    cudaFree(d_partial_sum_x);
    cudaFree(d_partial_sum_y);
    cudaFree(d_partial_sum_xy);
    cudaFree(d_partial_sum_x2);
    cudaFree(d_partial_sum_y2);
    cudaFree(d_partial_counts);

    return (err == cudaSuccess) ? NIMCP_STREAM_OK : NIMCP_STREAM_ERROR_GPU;
}

} // extern "C"

#endif // NIMCP_ENABLE_CUDA
