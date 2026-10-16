/**
 * @file nimcp_neural_statistics_kernels.cu
 * @brief CUDA Kernels for Neural-Specific Statistics
 *
 * WHAT: GPU-accelerated implementations of neural statistics functions
 * WHY:  Massive parallelism for batch processing of spike trains and populations
 * HOW:  Custom CUDA kernels with optimized memory access patterns
 *
 * PERFORMANCE TARGETS:
 * - ISI batch (1000 trains, 10K spikes each): <10ms
 * - Cross-correlogram batch (10K pairs): <100ms
 * - Fisher information (1000 neurons): <5ms
 *
 * THREAD SAFETY:
 * - All kernels are thread-safe
 * - Uses atomic operations for histogram updates
 * - Stream-based execution for overlap with CPU
 *
 * @author NIMCP Development Team
 * @date 2026-01-30
 * @version 1.0.0
 */

#ifdef NIMCP_ENABLE_CUDA

// Include CUDA headers FIRST (before any extern "C" blocks)
#include "utils/memory/nimcp_memory.h"
#include <cuda_runtime.h>
#include <cuda_fp16.h>
#include <math.h>
#include <float.h>

// Now include our headers
#include "utils/statistics/nimcp_neural_statistics.h"
#include "utils/logging/nimcp_logging.h"
#include "gpu/common/nimcp_cuda_utils.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "NEURAL_STATS_GPU"

//=============================================================================
// Kernel Configuration
//=============================================================================

#define BLOCK_SIZE 256
#define WARP_SIZE 32
#define GRID_SIZE(n) (((n) + BLOCK_SIZE - 1) / BLOCK_SIZE)

// Maximum spikes per train for shared memory
#define MAX_SHARED_SPIKES 1024

// Maximum correlogram bins
#define MAX_CORRELOGRAM_BINS 512

//=============================================================================
// Device Helper Functions
//=============================================================================

/**
 * @brief Warp-level reduction for sum
 */
__device__ __forceinline__ float warp_reduce_sum(float val) {
    for (int offset = WARP_SIZE / 2; offset > 0; offset /= 2) {
        val += __shfl_down_sync(0xffffffff, val, offset);
    }
    return val;
}

/**
 * @brief Warp-level reduction for max
 */
__device__ __forceinline__ float warp_reduce_max(float val) {
    for (int offset = WARP_SIZE / 2; offset > 0; offset /= 2) {
        float other = __shfl_down_sync(0xffffffff, val, offset);
        val = fmaxf(val, other);
    }
    return val;
}

/**
 * @brief Block-level reduction using shared memory
 */
__attribute__((unused)) static __device__ float block_reduce_sum(float val, float* shared) {
    int lane = threadIdx.x % WARP_SIZE;
    int wid = threadIdx.x / WARP_SIZE;

    val = warp_reduce_sum(val);

    if (lane == 0) shared[wid] = val;
    __syncthreads();

    val = (threadIdx.x < blockDim.x / WARP_SIZE) ? shared[lane] : 0.0f;

    if (wid == 0) val = warp_reduce_sum(val);

    return val;
}

//=============================================================================
// ISI Statistics Kernels
//=============================================================================

/**
 * @brief Kernel to compute ISI from spike times for multiple trains
 *
 * Each block handles one spike train.
 * Computes ISIs and basic statistics (sum, sum_sq, min, max).
 */
__global__ void kernel_isi_compute(
    const float* __restrict__ spike_times,       // [total_spikes]
    const uint32_t* __restrict__ train_offsets,  // [n_trains + 1]
    const uint32_t* __restrict__ train_lengths,  // [n_trains]
    float* __restrict__ isi_out,                 // [total_spikes - n_trains]
    float* __restrict__ stats_out,               // [n_trains * 4] (sum, sum_sq, min, max)
    uint32_t n_trains)
{
    uint32_t train_idx = blockIdx.x;
    if (train_idx >= n_trains) return;

    uint32_t n_spikes = train_lengths[train_idx];
    if (n_spikes < 2) return;

    uint32_t spike_offset = train_offsets[train_idx];
    uint32_t isi_offset = train_offsets[train_idx] - train_idx;  // Adjust for ISI count

    __shared__ float shared_sum[BLOCK_SIZE / WARP_SIZE];
    __shared__ float shared_sum_sq[BLOCK_SIZE / WARP_SIZE];
    __shared__ float shared_min;
    __shared__ float shared_max;

    if (threadIdx.x == 0) {
        shared_min = FLT_MAX;
        shared_max = -FLT_MAX;
    }
    __syncthreads();

    float local_sum = 0.0f;
    float local_sum_sq = 0.0f;
    float local_min = FLT_MAX;
    float local_max = -FLT_MAX;

    // Each thread handles multiple ISIs
    for (uint32_t i = threadIdx.x; i < n_spikes - 1; i += blockDim.x) {
        float t1 = spike_times[spike_offset + i];
        float t2 = spike_times[spike_offset + i + 1];
        float isi = t2 - t1;

        isi_out[isi_offset + i] = isi;

        local_sum += isi;
        local_sum_sq += isi * isi;
        local_min = fminf(local_min, isi);
        local_max = fmaxf(local_max, isi);
    }

    // Reduce within block
    float sum = block_reduce_sum(local_sum, shared_sum);
    float sum_sq = block_reduce_sum(local_sum_sq, shared_sum_sq);

    // Min/max reduction
    atomicMin((int*)&shared_min, __float_as_int(local_min));
    atomicMax((int*)&shared_max, __float_as_int(local_max));
    __syncthreads();

    // Write results (thread 0 only)
    if (threadIdx.x == 0) {
        stats_out[train_idx * 4 + 0] = sum;
        stats_out[train_idx * 4 + 1] = sum_sq;
        stats_out[train_idx * 4 + 2] = shared_min;
        stats_out[train_idx * 4 + 3] = shared_max;
    }
}

/**
 * @brief Kernel to compute CV2 (local coefficient of variation)
 */
__global__ void kernel_isi_cv2(
    const float* __restrict__ spike_times,
    const uint32_t* __restrict__ train_offsets,
    const uint32_t* __restrict__ train_lengths,
    float* __restrict__ cv2_out,
    uint32_t n_trains)
{
    uint32_t train_idx = blockIdx.x;
    if (train_idx >= n_trains) return;

    uint32_t n_spikes = train_lengths[train_idx];
    if (n_spikes < 3) {
        if (threadIdx.x == 0) cv2_out[train_idx] = nanf("");
        return;
    }

    uint32_t spike_offset = train_offsets[train_idx];
    uint32_t n_pairs = n_spikes - 2;

    __shared__ float shared[BLOCK_SIZE / WARP_SIZE];

    float local_sum = 0.0f;

    for (uint32_t i = threadIdx.x; i < n_pairs; i += blockDim.x) {
        float isi1 = spike_times[spike_offset + i + 1] - spike_times[spike_offset + i];
        float isi2 = spike_times[spike_offset + i + 2] - spike_times[spike_offset + i + 1];
        float sum_isi = isi1 + isi2;

        if (sum_isi > 1e-10f) {
            local_sum += 2.0f * fabsf(isi2 - isi1) / sum_isi;
        }
    }

    float sum = block_reduce_sum(local_sum, shared);

    if (threadIdx.x == 0) {
        cv2_out[train_idx] = sum / n_pairs;
    }
}

//=============================================================================
// Cross-Correlogram Kernels
//=============================================================================

/**
 * @brief Kernel to compute cross-correlogram for spike train pairs
 *
 * Uses atomic additions to histogram bins.
 * Optimized for many short spike trains.
 */
__global__ void kernel_cross_correlogram(
    const float* __restrict__ spikes1,           // [total_spikes1]
    const float* __restrict__ spikes2,           // [total_spikes2]
    const uint32_t* __restrict__ offsets1,       // [n_pairs + 1]
    const uint32_t* __restrict__ offsets2,       // [n_pairs + 1]
    const uint32_t* __restrict__ lengths1,       // [n_pairs]
    const uint32_t* __restrict__ lengths2,       // [n_pairs]
    float* __restrict__ correlograms,            // [n_pairs * n_bins]
    float bin_width,
    float max_lag,
    uint32_t n_bins,
    uint32_t n_pairs)
{
    // Each block handles one pair
    uint32_t pair_idx = blockIdx.x;
    if (pair_idx >= n_pairs) return;

    uint32_t n1 = lengths1[pair_idx];
    uint32_t n2 = lengths2[pair_idx];
    if (n1 == 0 || n2 == 0) return;

    uint32_t off1 = offsets1[pair_idx];
    uint32_t off2 = offsets2[pair_idx];

    float* corr = correlograms + pair_idx * n_bins;

    // Initialize shared memory histogram
    __shared__ float shared_hist[MAX_CORRELOGRAM_BINS];

    for (uint32_t i = threadIdx.x; i < n_bins && i < MAX_CORRELOGRAM_BINS; i += blockDim.x) {
        shared_hist[i] = 0.0f;
    }
    __syncthreads();

    // Each thread handles multiple spike pairs
    // Parallelize over spikes in train1
    for (uint32_t i = threadIdx.x; i < n1; i += blockDim.x) {
        float t1 = spikes1[off1 + i];

        for (uint32_t j = 0; j < n2; j++) {
            float t2 = spikes2[off2 + j];
            float lag = t2 - t1;

            if (lag >= -max_lag && lag <= max_lag) {
                uint32_t bin = (uint32_t)((lag + max_lag) / bin_width);
                if (bin < n_bins && bin < MAX_CORRELOGRAM_BINS) {
                    atomicAdd(&shared_hist[bin], 1.0f);
                }
            }
        }
    }
    __syncthreads();

    // Write shared histogram to global memory
    for (uint32_t i = threadIdx.x; i < n_bins && i < MAX_CORRELOGRAM_BINS; i += blockDim.x) {
        corr[i] = shared_hist[i];
    }
}

/**
 * @brief Kernel to normalize correlograms
 */
__global__ void kernel_normalize_correlogram(
    float* __restrict__ correlograms,
    const uint32_t* __restrict__ lengths1,
    float bin_width,
    uint32_t n_bins,
    uint32_t n_pairs)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t pair_idx = idx / n_bins;
    uint32_t bin_idx = idx % n_bins;

    if (pair_idx >= n_pairs) return;

    float n1 = (float)lengths1[pair_idx];
    if (n1 > 0.0f) {
        float norm = 1.0f / (n1 * bin_width / 1000.0f);
        correlograms[pair_idx * n_bins + bin_idx] *= norm;
    }
}

//=============================================================================
// Fisher Information Kernels
//=============================================================================

/**
 * @brief Kernel to compute tuning curve derivatives
 */
__global__ void kernel_tuning_derivative(
    const float* __restrict__ tuning_curves,     // [n_neurons * n_stimuli]
    const float* __restrict__ stimulus_values,   // [n_stimuli]
    float* __restrict__ derivatives,             // [n_neurons * (n_stimuli-1)]
    uint32_t n_neurons,
    uint32_t n_stimuli)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t neuron = idx / (n_stimuli - 1);
    uint32_t stim = idx % (n_stimuli - 1);

    if (neuron >= n_neurons) return;

    float ds = stimulus_values[stim + 1] - stimulus_values[stim];
    if (fabsf(ds) > 1e-10f) {
        float f1 = tuning_curves[neuron * n_stimuli + stim];
        float f2 = tuning_curves[neuron * n_stimuli + stim + 1];
        derivatives[neuron * (n_stimuli - 1) + stim] = (f2 - f1) / ds;
    } else {
        derivatives[neuron * (n_stimuli - 1) + stim] = 0.0f;
    }
}

/**
 * @brief Kernel to compute Fisher information (independent Poisson neurons)
 *
 * I(s) = sum_i (df_i/ds)^2 / f_i(s)
 */
__global__ void kernel_fisher_information(
    const float* __restrict__ tuning_curves,     // [n_neurons * n_stimuli]
    const float* __restrict__ derivatives,       // [n_neurons * (n_stimuli-1)]
    float* __restrict__ fisher_per_stim,         // [n_stimuli - 1]
    uint32_t n_neurons,
    uint32_t n_stimuli)
{
    uint32_t stim = blockIdx.x;
    if (stim >= n_stimuli - 1) return;

    __shared__ float shared[BLOCK_SIZE / WARP_SIZE];

    float local_fisher = 0.0f;

    for (uint32_t n = threadIdx.x; n < n_neurons; n += blockDim.x) {
        // Average tuning curve value at this stimulus
        float f = (tuning_curves[n * n_stimuli + stim] +
                   tuning_curves[n * n_stimuli + stim + 1]) / 2.0f;
        float df = derivatives[n * (n_stimuli - 1) + stim];

        if (f > 1e-10f) {
            local_fisher += (df * df) / f;
        }
    }

    float fisher = block_reduce_sum(local_fisher, shared);

    if (threadIdx.x == 0) {
        fisher_per_stim[stim] = fisher;
    }
}

/**
 * @brief Kernel for population vector decoding (circular variables)
 */
__global__ void kernel_population_vector_circular(
    const float* __restrict__ spike_counts,      // [n_neurons * n_time_points]
    const float* __restrict__ preferred,         // [n_neurons]
    float* __restrict__ decoded,                 // [n_time_points]
    float* __restrict__ confidence,              // [n_time_points]
    uint32_t n_neurons,
    uint32_t n_time_points)
{
    uint32_t t = blockIdx.x;
    if (t >= n_time_points) return;

    __shared__ float shared_x[BLOCK_SIZE / WARP_SIZE];
    __shared__ float shared_y[BLOCK_SIZE / WARP_SIZE];
    __shared__ float shared_w[BLOCK_SIZE / WARP_SIZE];

    float local_x = 0.0f, local_y = 0.0f, local_w = 0.0f;

    for (uint32_t n = threadIdx.x; n < n_neurons; n += blockDim.x) {
        float weight = spike_counts[n * n_time_points + t];
        float angle = preferred[n];

        local_x += weight * cosf(angle);
        local_y += weight * sinf(angle);
        local_w += weight;
    }

    float sum_x = block_reduce_sum(local_x, shared_x);
    float sum_y = block_reduce_sum(local_y, shared_y);
    float total_w = block_reduce_sum(local_w, shared_w);

    if (threadIdx.x == 0) {
        decoded[t] = atan2f(sum_y, sum_x);

        if (total_w > 1e-10f) {
            float r = sqrtf(sum_x * sum_x + sum_y * sum_y) / total_w;
            confidence[t] = r;
        } else {
            confidence[t] = 0.0f;
        }
    }
}

//=============================================================================
// Spike-Triggered Average Kernels
//=============================================================================

/**
 * @brief Kernel to compute spike-triggered average
 */
__global__ void kernel_spike_triggered_average(
    const float* __restrict__ signal,            // [n_samples]
    const float* __restrict__ spike_indices,     // [n_spikes] - pre-computed indices
    float* __restrict__ sta_sum,                 // [n_sta_samples]
    float* __restrict__ sta_sum_sq,              // [n_sta_samples]
    uint32_t* __restrict__ count,                // [1]
    uint32_t n_spikes,
    uint32_t n_samples,
    uint32_t samples_before,
    uint32_t samples_after)
{
    uint32_t n_sta = samples_before + samples_after + 1;

    __shared__ float shared[BLOCK_SIZE / WARP_SIZE];

    // Each block handles one time point in STA
    uint32_t sta_idx = blockIdx.x;
    if (sta_idx >= n_sta) return;

    float local_sum = 0.0f;
    float local_sum_sq = 0.0f;
    uint32_t local_count = 0;

    for (uint32_t s = threadIdx.x; s < n_spikes; s += blockDim.x) {
        int32_t spike_sample = (int32_t)spike_indices[s];
        int32_t sig_idx = spike_sample - (int32_t)samples_before + (int32_t)sta_idx;

        if (sig_idx >= 0 && sig_idx < (int32_t)n_samples) {
            float val = signal[sig_idx];
            local_sum += val;
            local_sum_sq += val * val;
            if (sta_idx == 0) local_count++;  // Count only once
        }
    }

    float sum = block_reduce_sum(local_sum, shared);
    float sum_sq = block_reduce_sum(local_sum_sq, shared);

    if (threadIdx.x == 0) {
        atomicAdd(&sta_sum[sta_idx], sum);
        atomicAdd(&sta_sum_sq[sta_idx], sum_sq);

        if (sta_idx == 0) {
            atomicAdd(count, local_count);
        }
    }
}

//=============================================================================
// Burst Detection Kernel
//=============================================================================

/**
 * @brief Kernel to identify bursts in spike trains
 */
__global__ void kernel_burst_detection(
    const float* __restrict__ spike_times,
    const uint32_t* __restrict__ train_offsets,
    const uint32_t* __restrict__ train_lengths,
    uint8_t* __restrict__ is_burst_spike,        // [total_spikes]
    uint32_t* __restrict__ burst_counts,         // [n_trains]
    float max_isi_within,
    uint32_t min_spikes_per_burst,
    uint32_t n_trains)
{
    uint32_t train_idx = blockIdx.x;
    if (train_idx >= n_trains) return;

    uint32_t n_spikes = train_lengths[train_idx];
    if (n_spikes < min_spikes_per_burst) {
        if (threadIdx.x == 0) burst_counts[train_idx] = 0;
        return;
    }

    uint32_t offset = train_offsets[train_idx];

    // Only thread 0 does the sequential burst detection
    if (threadIdx.x == 0) {
        uint32_t n_bursts = 0;
        bool in_burst = false;
        uint32_t burst_start = 0;
        uint32_t spikes_in_burst = 1;

        for (uint32_t i = 1; i < n_spikes; i++) {
            float isi = spike_times[offset + i] - spike_times[offset + i - 1];

            if (isi <= max_isi_within) {
                if (!in_burst) {
                    in_burst = true;
                    burst_start = i - 1;
                    spikes_in_burst = 2;
                } else {
                    spikes_in_burst++;
                }
            } else {
                if (in_burst && spikes_in_burst >= min_spikes_per_burst) {
                    // Mark spikes as burst spikes
                    for (uint32_t j = burst_start; j < i; j++) {
                        is_burst_spike[offset + j] = 1;
                    }
                    n_bursts++;
                }
                in_burst = false;
                spikes_in_burst = 1;
            }
        }

        // Handle last burst
        if (in_burst && spikes_in_burst >= min_spikes_per_burst) {
            for (uint32_t j = burst_start; j < n_spikes; j++) {
                is_burst_spike[offset + j] = 1;
            }
            n_bursts++;
        }

        burst_counts[train_idx] = n_bursts;
    }
}

//=============================================================================
// Fano Factor Kernel
//=============================================================================

/**
 * @brief Kernel to count spikes in windows for Fano factor
 */
__global__ void kernel_fano_spike_counts(
    const float* __restrict__ spike_times,
    const uint32_t* __restrict__ train_offsets,
    const uint32_t* __restrict__ train_lengths,
    const float* __restrict__ start_times,       // [n_trains]
    uint32_t* __restrict__ window_counts,        // [n_trains * n_windows]
    float window_size,
    uint32_t n_windows,
    uint32_t n_trains)
{
    uint32_t train_idx = blockIdx.x;
    if (train_idx >= n_trains) return;

    uint32_t n_spikes = train_lengths[train_idx];
    uint32_t offset = train_offsets[train_idx];
    float start = start_times[train_idx];

    uint32_t* counts = window_counts + train_idx * n_windows;

    // Each thread handles a window
    for (uint32_t w = threadIdx.x; w < n_windows; w += blockDim.x) {
        float w_start = start + w * window_size;
        float w_end = w_start + window_size;
        uint32_t count = 0;

        for (uint32_t i = 0; i < n_spikes; i++) {
            float t = spike_times[offset + i];
            if (t >= w_start && t < w_end) {
                count++;
            }
        }

        counts[w] = count;
    }
}

/**
 * @brief Kernel to compute Fano factor from window counts
 */
__global__ void kernel_fano_factor_compute(
    const uint32_t* __restrict__ window_counts,  // [n_trains * n_windows]
    float* __restrict__ fano_factors,            // [n_trains]
    uint32_t n_windows,
    uint32_t n_trains)
{
    uint32_t train_idx = blockIdx.x;
    if (train_idx >= n_trains) return;

    const uint32_t* counts = window_counts + train_idx * n_windows;

    __shared__ float shared_sum[BLOCK_SIZE / WARP_SIZE];
    __shared__ float shared_sum_sq[BLOCK_SIZE / WARP_SIZE];

    float local_sum = 0.0f;
    float local_sum_sq = 0.0f;

    for (uint32_t w = threadIdx.x; w < n_windows; w += blockDim.x) {
        float c = (float)counts[w];
        local_sum += c;
        local_sum_sq += c * c;
    }

    float sum = block_reduce_sum(local_sum, shared_sum);
    float sum_sq = block_reduce_sum(local_sum_sq, shared_sum_sq);

    if (threadIdx.x == 0) {
        float mean = sum / n_windows;
        float variance = (sum_sq / n_windows) - (mean * mean);

        if (mean > 1e-10f) {
            fano_factors[train_idx] = variance / mean;
        } else {
            fano_factors[train_idx] = nanf("");
        }
    }
}

//=============================================================================
// Host-Side Batch Functions
//=============================================================================

/**
 * @brief GPU implementation of batch ISI distribution
 */
extern "C" neural_stats_result_t nimcp_neural_isi_distribution_batch_gpu_impl(
    const neural_spike_train_t* spike_trains,
    uint32_t n_trains,
    neural_isi_distribution_t* results)
{
    /* Initialize recovery system if needed */
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!spike_trains || !results || n_trains == 0) {
        return NEURAL_STATS_ERROR_NULL;
    }

    // Count total spikes and prepare offsets
    uint32_t* h_offsets = (uint32_t*)nimcp_malloc((n_trains + 1) * sizeof(uint32_t));
    uint32_t* h_lengths = (uint32_t*)nimcp_malloc(n_trains * sizeof(uint32_t));
    if (!h_offsets || !h_lengths) {
        nimcp_free(h_offsets);
        nimcp_free(h_lengths);
        return NEURAL_STATS_ERROR_MEMORY;
    }

    h_offsets[0] = 0;
    uint32_t total_spikes = 0;

    for (uint32_t i = 0; i < n_trains; i++) {
        h_lengths[i] = spike_trains[i].n_spikes;
        total_spikes += spike_trains[i].n_spikes;
        h_offsets[i + 1] = total_spikes;
    }

    if (total_spikes == 0) {
        nimcp_free(h_offsets);
        nimcp_free(h_lengths);
        return NEURAL_STATS_OK;
    }

    // Flatten spike times
    float* h_spikes = (float*)nimcp_malloc(total_spikes * sizeof(float));
    if (!h_spikes) {
        nimcp_free(h_offsets);
        nimcp_free(h_lengths);
        return NEURAL_STATS_ERROR_MEMORY;
    }

    for (uint32_t i = 0; i < n_trains; i++) {
        if (spike_trains[i].n_spikes > 0 && spike_trains[i].spike_times) {
            memcpy(h_spikes + h_offsets[i],
                   spike_trains[i].spike_times,
                   spike_trains[i].n_spikes * sizeof(float));
        }
    }

    // Allocate device memory
    float* d_spikes = NULL;
    uint32_t* d_offsets = NULL;
    uint32_t* d_lengths = NULL;
    float* d_isis = NULL;
    float* d_stats = NULL;
    float* d_cv2 = NULL;

    // Pre-declare host arrays before goto statements to avoid initialization bypass
    float* h_stats = NULL;
    float* h_cv2 = NULL;

    uint32_t total_isis = total_spikes - n_trains;

    cudaError_t err;
    nimcp_gpu_recovery_result_t rec_result = {0};

    err = cudaMalloc(&d_spikes, total_spikes * sizeof(float));
    if (err != cudaSuccess) {
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &rec_result)) {
            err = cudaMalloc(&d_spikes, total_spikes * sizeof(float));
        }
        if (err != cudaSuccess) goto cleanup;
    }

    err = cudaMalloc(&d_offsets, (n_trains + 1) * sizeof(uint32_t));
    if (err != cudaSuccess) {
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &rec_result)) {
            err = cudaMalloc(&d_offsets, (n_trains + 1) * sizeof(uint32_t));
        }
        if (err != cudaSuccess) goto cleanup;
    }

    err = cudaMalloc(&d_lengths, n_trains * sizeof(uint32_t));
    if (err != cudaSuccess) {
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &rec_result)) {
            err = cudaMalloc(&d_lengths, n_trains * sizeof(uint32_t));
        }
        if (err != cudaSuccess) goto cleanup;
    }

    err = cudaMalloc(&d_isis, total_isis * sizeof(float));
    if (err != cudaSuccess) {
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &rec_result)) {
            err = cudaMalloc(&d_isis, total_isis * sizeof(float));
        }
        if (err != cudaSuccess) goto cleanup;
    }

    err = cudaMalloc(&d_stats, n_trains * 4 * sizeof(float));
    if (err != cudaSuccess) {
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &rec_result)) {
            err = cudaMalloc(&d_stats, n_trains * 4 * sizeof(float));
        }
        if (err != cudaSuccess) goto cleanup;
    }

    err = cudaMalloc(&d_cv2, n_trains * sizeof(float));
    if (err != cudaSuccess) {
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &rec_result)) {
            err = cudaMalloc(&d_cv2, n_trains * sizeof(float));
        }
        if (err != cudaSuccess) goto cleanup;
    }

    // Copy to device
    err = cudaMemcpy(d_spikes, h_spikes, total_spikes * sizeof(float), cudaMemcpyHostToDevice);
    if (err != cudaSuccess) {
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_CUDA_RUNTIME, err, &rec_result)) {
            err = cudaMemcpy(d_spikes, h_spikes, total_spikes * sizeof(float), cudaMemcpyHostToDevice);
        }
        if (err != cudaSuccess) goto cleanup;
    }
    err = cudaMemcpy(d_offsets, h_offsets, (n_trains + 1) * sizeof(uint32_t), cudaMemcpyHostToDevice);
    if (err != cudaSuccess) {
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_CUDA_RUNTIME, err, &rec_result)) {
            err = cudaMemcpy(d_offsets, h_offsets, (n_trains + 1) * sizeof(uint32_t), cudaMemcpyHostToDevice);
        }
        if (err != cudaSuccess) goto cleanup;
    }
    err = cudaMemcpy(d_lengths, h_lengths, n_trains * sizeof(uint32_t), cudaMemcpyHostToDevice);
    if (err != cudaSuccess) {
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_CUDA_RUNTIME, err, &rec_result)) {
            err = cudaMemcpy(d_lengths, h_lengths, n_trains * sizeof(uint32_t), cudaMemcpyHostToDevice);
        }
        if (err != cudaSuccess) goto cleanup;
    }

    // Launch kernels
    kernel_isi_compute<<<n_trains, BLOCK_SIZE>>>(
        d_spikes, d_offsets, d_lengths, d_isis, d_stats, n_trains);

    kernel_isi_cv2<<<n_trains, BLOCK_SIZE>>>(
        d_spikes, d_offsets, d_lengths, d_cv2, n_trains);

    cudaDeviceSynchronize();

    // Copy results back
    h_stats = (float*)nimcp_malloc(n_trains * 4 * sizeof(float));
    h_cv2 = (float*)nimcp_malloc(n_trains * sizeof(float));

    if (h_stats && h_cv2) {
        cudaMemcpy(h_stats, d_stats, n_trains * 4 * sizeof(float), cudaMemcpyDeviceToHost);
        cudaMemcpy(h_cv2, d_cv2, n_trains * sizeof(float), cudaMemcpyDeviceToHost);

        // Populate results
        for (uint32_t i = 0; i < n_trains; i++) {
            uint32_t n_intervals = h_lengths[i] > 0 ? h_lengths[i] - 1 : 0;
            results[i].n_intervals = n_intervals;

            if (n_intervals > 0) {
                float sum = h_stats[i * 4 + 0];
                float sum_sq = h_stats[i * 4 + 1];

                results[i].mean = sum / n_intervals;
                results[i].min_isi = h_stats[i * 4 + 2];
                results[i].max_isi = h_stats[i * 4 + 3];

                float variance = (sum_sq / n_intervals) -
                    (results[i].mean * results[i].mean);
                results[i].variance = variance * n_intervals / (n_intervals - 1);
                results[i].std_dev = sqrtf(results[i].variance);

                if (results[i].mean > 1e-10f) {
                    results[i].cv = results[i].std_dev / results[i].mean;
                } else {
                    results[i].cv = NAN;
                }

                results[i].cv2 = h_cv2[i];
                results[i].intervals = NULL;  // Not copying individual ISIs
            }
        }
    }

    nimcp_free(h_stats);
    nimcp_free(h_cv2);

cleanup:
    cudaFree(d_spikes);
    cudaFree(d_offsets);
    cudaFree(d_lengths);
    cudaFree(d_isis);
    cudaFree(d_stats);
    cudaFree(d_cv2);
    nimcp_free(h_spikes);
    nimcp_free(h_offsets);
    nimcp_free(h_lengths);

    return (err == cudaSuccess) ? NEURAL_STATS_OK : NEURAL_STATS_ERROR_GPU;
}

/**
 * @brief GPU implementation of batch cross-correlogram
 */
extern "C" neural_stats_result_t nimcp_neural_cross_correlogram_batch_gpu_impl(
    const neural_spike_train_t* trains1,
    const neural_spike_train_t* trains2,
    uint32_t n_pairs,
    float bin_width,
    float max_lag,
    neural_cross_correlogram_t* results)
{
    /* Initialize recovery system if needed */
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!trains1 || !trains2 || !results || n_pairs == 0) {
        return NEURAL_STATS_ERROR_NULL;
    }

    uint32_t n_bins = (uint32_t)(2.0f * max_lag / bin_width) + 1;
    if (n_bins > MAX_CORRELOGRAM_BINS) {
        n_bins = MAX_CORRELOGRAM_BINS;
    }

    // Count total spikes
    uint32_t total_spikes1 = 0, total_spikes2 = 0;
    for (uint32_t i = 0; i < n_pairs; i++) {
        total_spikes1 += trains1[i].n_spikes;
        total_spikes2 += trains2[i].n_spikes;
    }

    if (total_spikes1 == 0 || total_spikes2 == 0) {
        return NEURAL_STATS_OK;
    }

    // Prepare data (similar structure to ISI batch)
    uint32_t* h_offsets1 = (uint32_t*)nimcp_malloc((n_pairs + 1) * sizeof(uint32_t));
    uint32_t* h_offsets2 = (uint32_t*)nimcp_malloc((n_pairs + 1) * sizeof(uint32_t));
    uint32_t* h_lengths1 = (uint32_t*)nimcp_malloc(n_pairs * sizeof(uint32_t));
    uint32_t* h_lengths2 = (uint32_t*)nimcp_malloc(n_pairs * sizeof(uint32_t));
    float* h_spikes1 = (float*)nimcp_malloc(total_spikes1 * sizeof(float));
    float* h_spikes2 = (float*)nimcp_malloc(total_spikes2 * sizeof(float));

    if (!h_offsets1 || !h_offsets2 || !h_lengths1 || !h_lengths2 ||
        !h_spikes1 || !h_spikes2) {
        nimcp_free(h_offsets1); nimcp_free(h_offsets2);
        nimcp_free(h_lengths1); nimcp_free(h_lengths2);
        nimcp_free(h_spikes1); nimcp_free(h_spikes2);
        return NEURAL_STATS_ERROR_MEMORY;
    }

    h_offsets1[0] = h_offsets2[0] = 0;
    for (uint32_t i = 0; i < n_pairs; i++) {
        h_lengths1[i] = trains1[i].n_spikes;
        h_lengths2[i] = trains2[i].n_spikes;
        h_offsets1[i + 1] = h_offsets1[i] + h_lengths1[i];
        h_offsets2[i + 1] = h_offsets2[i] + h_lengths2[i];

        if (trains1[i].spike_times) {
            memcpy(h_spikes1 + h_offsets1[i], trains1[i].spike_times,
                   trains1[i].n_spikes * sizeof(float));
        }
        if (trains2[i].spike_times) {
            memcpy(h_spikes2 + h_offsets2[i], trains2[i].spike_times,
                   trains2[i].n_spikes * sizeof(float));
        }
    }

    // Allocate device memory
    float *d_spikes1 = NULL, *d_spikes2 = NULL;
    uint32_t *d_offsets1 = NULL, *d_offsets2 = NULL;
    uint32_t *d_lengths1 = NULL, *d_lengths2 = NULL;
    float *d_correlograms = NULL;

    // Pre-declare host array before goto statements to avoid initialization bypass
    float* h_correlograms = NULL;
    cudaError_t err = cudaSuccess;

    err = cudaMalloc(&d_spikes1, total_spikes1 * sizeof(float));
    if (err != cudaSuccess) goto cleanup_cc;

    err = cudaMalloc(&d_spikes2, total_spikes2 * sizeof(float));
    if (err != cudaSuccess) goto cleanup_cc;

    err = cudaMalloc(&d_offsets1, (n_pairs + 1) * sizeof(uint32_t));
    if (err != cudaSuccess) goto cleanup_cc;

    err = cudaMalloc(&d_offsets2, (n_pairs + 1) * sizeof(uint32_t));
    if (err != cudaSuccess) goto cleanup_cc;

    err = cudaMalloc(&d_lengths1, n_pairs * sizeof(uint32_t));
    if (err != cudaSuccess) goto cleanup_cc;

    err = cudaMalloc(&d_lengths2, n_pairs * sizeof(uint32_t));
    if (err != cudaSuccess) goto cleanup_cc;

    err = cudaMalloc(&d_correlograms, n_pairs * n_bins * sizeof(float));
    if (err != cudaSuccess) goto cleanup_cc;

    // Copy to device
    cudaMemcpy(d_spikes1, h_spikes1, total_spikes1 * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(d_spikes2, h_spikes2, total_spikes2 * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(d_offsets1, h_offsets1, (n_pairs + 1) * sizeof(uint32_t), cudaMemcpyHostToDevice);
    cudaMemcpy(d_offsets2, h_offsets2, (n_pairs + 1) * sizeof(uint32_t), cudaMemcpyHostToDevice);
    cudaMemcpy(d_lengths1, h_lengths1, n_pairs * sizeof(uint32_t), cudaMemcpyHostToDevice);
    cudaMemcpy(d_lengths2, h_lengths2, n_pairs * sizeof(uint32_t), cudaMemcpyHostToDevice);
    cudaMemset(d_correlograms, 0, n_pairs * n_bins * sizeof(float));

    // Launch kernels
    kernel_cross_correlogram<<<n_pairs, BLOCK_SIZE>>>(
        d_spikes1, d_spikes2, d_offsets1, d_offsets2,
        d_lengths1, d_lengths2, d_correlograms,
        bin_width, max_lag, n_bins, n_pairs);

    kernel_normalize_correlogram<<<GRID_SIZE(n_pairs * n_bins), BLOCK_SIZE>>>(
        d_correlograms, d_lengths1, bin_width, n_bins, n_pairs);

    cudaDeviceSynchronize();

    // Copy results back
    h_correlograms = (float*)nimcp_malloc(n_pairs * n_bins * sizeof(float));
    if (h_correlograms) {
        cudaMemcpy(h_correlograms, d_correlograms,
                   n_pairs * n_bins * sizeof(float), cudaMemcpyDeviceToHost);

        for (uint32_t i = 0; i < n_pairs; i++) {
            results[i].n_bins = n_bins;
            results[i].bin_width = bin_width;
            results[i].max_lag = max_lag;

            results[i].correlogram = (float*)nimcp_malloc(n_bins * sizeof(float));
            results[i].lags = (float*)nimcp_malloc(n_bins * sizeof(float));

            if (results[i].correlogram && results[i].lags) {
                memcpy(results[i].correlogram, h_correlograms + i * n_bins,
                       n_bins * sizeof(float));

                for (uint32_t j = 0; j < n_bins; j++) {
                    results[i].lags[j] = -max_lag + j * bin_width;
                }

                // Find peak
                results[i].peak_correlation = -FLT_MAX;
                for (uint32_t j = 0; j < n_bins; j++) {
                    if (results[i].correlogram[j] > results[i].peak_correlation) {
                        results[i].peak_correlation = results[i].correlogram[j];
                        results[i].peak_lag = results[i].lags[j];
                    }
                }
            }
        }

        nimcp_free(h_correlograms);
    }

cleanup_cc:
    cudaFree(d_spikes1);
    cudaFree(d_spikes2);
    cudaFree(d_offsets1);
    cudaFree(d_offsets2);
    cudaFree(d_lengths1);
    cudaFree(d_lengths2);
    cudaFree(d_correlograms);
    nimcp_free(h_offsets1); nimcp_free(h_offsets2);
    nimcp_free(h_lengths1); nimcp_free(h_lengths2);
    nimcp_free(h_spikes1); nimcp_free(h_spikes2);

    return (err == cudaSuccess) ? NEURAL_STATS_OK : NEURAL_STATS_ERROR_GPU;
}

/**
 * @brief GPU implementation of batch Fisher information
 */
extern "C" neural_stats_result_t nimcp_neural_fisher_information_batch_gpu_impl(
    const float* tuning_curves,
    const float* stimulus_values,
    uint32_t n_populations,
    uint32_t n_neurons,
    uint32_t n_stimuli,
    neural_fisher_info_t* results)
{
    /* Initialize recovery system if needed */
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!tuning_curves || !stimulus_values || !results) {
        return NEURAL_STATS_ERROR_NULL;
    }

    if (n_populations == 0 || n_neurons == 0 || n_stimuli < 2) {
        return NEURAL_STATS_ERROR_SIZE;
    }

    size_t pop_size = n_neurons * n_stimuli;
    size_t deriv_size = n_neurons * (n_stimuli - 1);

    // Allocate device memory
    float *d_tuning = NULL, *d_stimulus = NULL;
    float *d_derivatives = NULL, *d_fisher = NULL;

    // Pre-declare host array before goto statements to avoid initialization bypass
    float* h_fisher = NULL;
    cudaError_t err = cudaSuccess;

    err = cudaMalloc(&d_tuning, n_populations * pop_size * sizeof(float));
    if (err != cudaSuccess) goto cleanup_fi;

    err = cudaMalloc(&d_stimulus, n_stimuli * sizeof(float));
    if (err != cudaSuccess) goto cleanup_fi;

    err = cudaMalloc(&d_derivatives, n_populations * deriv_size * sizeof(float));
    if (err != cudaSuccess) goto cleanup_fi;

    err = cudaMalloc(&d_fisher, n_populations * (n_stimuli - 1) * sizeof(float));
    if (err != cudaSuccess) goto cleanup_fi;

    // Copy to device
    cudaMemcpy(d_tuning, tuning_curves,
               n_populations * pop_size * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(d_stimulus, stimulus_values,
               n_stimuli * sizeof(float), cudaMemcpyHostToDevice);

    // Process each population
    for (uint32_t p = 0; p < n_populations; p++) {
        float* pop_tuning = d_tuning + p * pop_size;
        float* pop_deriv = d_derivatives + p * deriv_size;
        float* pop_fisher = d_fisher + p * (n_stimuli - 1);

        // Compute derivatives
        kernel_tuning_derivative<<<GRID_SIZE(deriv_size), BLOCK_SIZE>>>(
            pop_tuning, d_stimulus, pop_deriv, n_neurons, n_stimuli);

        // Compute Fisher information
        kernel_fisher_information<<<n_stimuli - 1, BLOCK_SIZE>>>(
            pop_tuning, pop_deriv, pop_fisher, n_neurons, n_stimuli);
    }

    cudaDeviceSynchronize();

    // Copy results back
    h_fisher = (float*)nimcp_malloc(n_populations * (n_stimuli - 1) * sizeof(float));
    if (h_fisher) {
        cudaMemcpy(h_fisher, d_fisher,
                   n_populations * (n_stimuli - 1) * sizeof(float),
                   cudaMemcpyDeviceToHost);

        for (uint32_t p = 0; p < n_populations; p++) {
            float total = 0.0f;
            for (uint32_t s = 0; s < n_stimuli - 1; s++) {
                total += h_fisher[p * (n_stimuli - 1) + s];
            }

            results[p].fisher_info = total / (n_stimuli - 1);
            results[p].total_info = total;
            results[p].n_params = 1;

            results[p].cramer_rao_bounds = (float*)nimcp_malloc(sizeof(float));
            if (results[p].cramer_rao_bounds) {
                results[p].cramer_rao_bounds[0] =
                    (results[p].fisher_info > 1e-10f) ?
                    1.0f / results[p].fisher_info : FLT_MAX;
            }
        }

        nimcp_free(h_fisher);
    }

cleanup_fi:
    cudaFree(d_tuning);
    cudaFree(d_stimulus);
    cudaFree(d_derivatives);
    cudaFree(d_fisher);

    return (err == cudaSuccess) ? NEURAL_STATS_OK : NEURAL_STATS_ERROR_GPU;
}

#endif // NIMCP_ENABLE_CUDA
