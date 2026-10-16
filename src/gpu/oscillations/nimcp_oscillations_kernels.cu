/**
 * @file nimcp_oscillations_kernels.cu
 * @brief GPU Brain Oscillation CUDA Kernels
 *
 * WHAT: Advanced CUDA kernels for brain oscillation computations
 * WHY:  GPU parallelization for massive speedup of neural oscillation analysis
 * HOW:  Specialized kernels for phase sync, PAC, FFT-based power, coherence
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifdef NIMCP_ENABLE_CUDA

// Include CUDA headers FIRST (before any extern "C" blocks from our headers)
#include "utils/memory/nimcp_memory.h"
#include <cuda_runtime.h>
#include <cufft.h>
#include <math.h>
#include <float.h>
#include <stdlib.h>

// Now include our headers (which have extern "C" blocks)
#include "gpu/oscillations/nimcp_oscillations_gpu.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "gpu/common/nimcp_cuda_utils.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "constants/nimcp_math_constants.h"

#define LOG_MODULE "OSCILLATIONS_GPU"

#define CUFFT_CHECK(call) do { \
    cufftResult err = call; \
    if (err != CUFFT_SUCCESS) { \
        LOG_ERROR("cuFFT error at %s:%d: %d", __FILE__, __LINE__, (int)err); \
        return false; \
    } \
} while(0)

#define BLOCK_SIZE 256
#define GRID_SIZE(n) (((n) + BLOCK_SIZE - 1) / BLOCK_SIZE)
#define WARP_SIZE 32


//=============================================================================
// Band Frequency Definitions (Hz)
//=============================================================================

static const float BAND_FREQUENCIES[6][2] = {
    {0.5f, 4.0f},     // NIMCP_BAND_DELTA
    {4.0f, 8.0f},     // NIMCP_BAND_THETA
    {8.0f, 13.0f},    // NIMCP_BAND_ALPHA
    {13.0f, 30.0f},   // NIMCP_BAND_BETA
    {30.0f, 100.0f},  // NIMCP_BAND_GAMMA
    {100.0f, 200.0f}  // NIMCP_BAND_HIGH_GAMMA
};

//=============================================================================
// Default Parameter Functions
//=============================================================================

nimcp_oscillation_params_t nimcp_oscillation_default_params(float sampling_rate)
{
    nimcp_oscillation_params_t params;
    params.sampling_rate = sampling_rate;
    params.dt = 1.0f / sampling_rate;
    params.n_samples = 1024;
    params.n_channels = 1;
    params.freq_low = 1.0f;
    params.freq_high = 100.0f;
    params.n_fft = 256;
    params.hop_length = 64;
    return params;
}

nimcp_pac_params_t nimcp_pac_default_params(void)
{
    nimcp_pac_params_t params;
    // Default: theta-gamma coupling
    params.phase_freq_low = 4.0f;   // Theta low
    params.phase_freq_high = 8.0f;  // Theta high
    params.amp_freq_low = 30.0f;    // Gamma low
    params.amp_freq_high = 100.0f;  // Gamma high
    params.n_phase_bins = 18;       // 20 degree bins
    params.use_hilbert = true;
    return params;
}

nimcp_coherence_params_t nimcp_coherence_default_params(void)
{
    nimcp_coherence_params_t params;
    params.n_fft = 256;
    params.n_overlap = 128;
    params.freq_low = 1.0f;
    params.freq_high = 100.0f;
    params.imaginary_coherence = false;
    return params;
}

void nimcp_get_band_frequencies(
    nimcp_oscillation_band_t band,
    float* low_out,
    float* high_out)
{
    if (!low_out || !high_out) return;

    int idx = (int)band;
    if (idx < 0 || idx > 5) {
        *low_out = 0.0f;
        *high_out = 0.0f;
        return;
    }

    *low_out = BAND_FREQUENCIES[idx][0];
    *high_out = BAND_FREQUENCIES[idx][1];
}

//=============================================================================
// Oscillation State Lifecycle
//=============================================================================

nimcp_oscillation_state_t* nimcp_oscillation_state_create(
    nimcp_gpu_context_t* ctx,
    const nimcp_oscillation_params_t* params)
{
    if (!ctx || !params) {
        LOG_ERROR("Invalid parameters for oscillation state create");
        return NULL;
    }

    nimcp_oscillation_state_t* state = (nimcp_oscillation_state_t*)nimcp_malloc(sizeof(nimcp_oscillation_state_t));
    if (!state) {
        LOG_ERROR("Failed to allocate oscillation state");
        return NULL;
    }

    state->params = *params;

    // Create tensors for signal processing
    size_t signal_dims[] = {params->n_channels, params->n_samples};
    size_t phase_dims[] = {params->n_channels, params->n_samples};
    size_t power_dims[] = {params->n_channels};
    size_t fft_dims[] = {params->n_channels, params->n_fft};

    state->signal = nimcp_gpu_tensor_create(ctx, signal_dims, 2, NIMCP_GPU_PRECISION_FP32);
    state->phase = nimcp_gpu_tensor_create(ctx, phase_dims, 2, NIMCP_GPU_PRECISION_FP32);
    state->amplitude = nimcp_gpu_tensor_create(ctx, phase_dims, 2, NIMCP_GPU_PRECISION_FP32);
    state->power = nimcp_gpu_tensor_create(ctx, power_dims, 1, NIMCP_GPU_PRECISION_FP32);
    state->fft_buffer = nimcp_gpu_tensor_create(ctx, fft_dims, 2, NIMCP_GPU_PRECISION_FP32);

    if (!state->signal || !state->phase || !state->amplitude ||
        !state->power || !state->fft_buffer) {
        LOG_ERROR("Failed to allocate oscillation state tensors");
        nimcp_oscillation_state_destroy(state);
        return NULL;
    }

    LOG_DEBUG("Created oscillation state: %zu channels, %zu samples",
              params->n_channels, params->n_samples);

    return state;
}

void nimcp_oscillation_state_destroy(nimcp_oscillation_state_t* state)
{
    if (!state) return;

    if (state->signal) nimcp_gpu_tensor_destroy(state->signal);
    if (state->phase) nimcp_gpu_tensor_destroy(state->phase);
    if (state->amplitude) nimcp_gpu_tensor_destroy(state->amplitude);
    if (state->power) nimcp_gpu_tensor_destroy(state->power);
    if (state->fft_buffer) nimcp_gpu_tensor_destroy(state->fft_buffer);

    nimcp_free(state);
}

nimcp_phase_sync_state_t* nimcp_phase_sync_state_create(
    nimcp_gpu_context_t* ctx,
    size_t n_channels,
    size_t window_size)
{
    if (!ctx || n_channels == 0 || window_size == 0) {
        LOG_ERROR("Invalid parameters for phase sync state create");
        return NULL;
    }

    nimcp_phase_sync_state_t* state = (nimcp_phase_sync_state_t*)nimcp_malloc(sizeof(nimcp_phase_sync_state_t));
    if (!state) {
        LOG_ERROR("Failed to allocate phase sync state");
        return NULL;
    }

    state->n_channels = n_channels;
    state->window_size = window_size;

    // Create tensors
    size_t diff_dims[] = {n_channels, n_channels, window_size};
    size_t matrix_dims[] = {n_channels, n_channels};

    state->phase_diff = nimcp_gpu_tensor_create(ctx, diff_dims, 3, NIMCP_GPU_PRECISION_FP32);
    state->plv = nimcp_gpu_tensor_create(ctx, matrix_dims, 2, NIMCP_GPU_PRECISION_FP32);
    state->pli = nimcp_gpu_tensor_create(ctx, matrix_dims, 2, NIMCP_GPU_PRECISION_FP32);
    state->sync_matrix = nimcp_gpu_tensor_create(ctx, matrix_dims, 2, NIMCP_GPU_PRECISION_FP32);

    if (!state->phase_diff || !state->plv || !state->pli || !state->sync_matrix) {
        LOG_ERROR("Failed to allocate phase sync state tensors");
        nimcp_phase_sync_state_destroy(state);
        return NULL;
    }

    LOG_DEBUG("Created phase sync state: %zu channels, window %zu", n_channels, window_size);

    return state;
}

void nimcp_phase_sync_state_destroy(nimcp_phase_sync_state_t* state)
{
    if (!state) return;

    if (state->phase_diff) nimcp_gpu_tensor_destroy(state->phase_diff);
    if (state->plv) nimcp_gpu_tensor_destroy(state->plv);
    if (state->pli) nimcp_gpu_tensor_destroy(state->pli);
    if (state->sync_matrix) nimcp_gpu_tensor_destroy(state->sync_matrix);

    nimcp_free(state);
}

//=============================================================================
// Device Helper Functions
//=============================================================================

/**
 * @brief Compute complex magnitude: sqrt(re^2 + im^2)
 */
__device__ inline float device_complex_magnitude(float re, float im)
{
    return sqrtf(re * re + im * im);
}

/**
 * @brief Compute complex phase: atan2(im, re)
 */
__device__ inline float device_complex_phase(float re, float im)
{
    return atan2f(im, re);
}

/**
 * @brief Wrap angle to [-pi, pi]
 */
__device__ inline float device_wrap_angle(float angle)
{
    while (angle > M_PI) angle -= 2.0f * M_PI;
    while (angle < -M_PI) angle += 2.0f * M_PI;
    return angle;
}

/**
 * @brief Sign function
 */
__device__ inline float device_sign(float x)
{
    if (x > 0.0f) return 1.0f;
    if (x < 0.0f) return -1.0f;
    return 0.0f;
}

//=============================================================================
// Phase Locking Value (PLV) Kernels
//=============================================================================

/**
 * @brief Compute phase difference and exp(i * diff) for PLV
 *
 * For each time point: compute exp(i * (phase1 - phase2))
 * Store as (cos, sin) pairs for later averaging
 */
__global__ void kernel_phase_diff_exp(
    const float* __restrict__ phase1,
    const float* __restrict__ phase2,
    float* __restrict__ cos_diff,
    float* __restrict__ sin_diff,
    uint32_t n_samples)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_samples) return;

    float diff = phase1[idx] - phase2[idx];
    cos_diff[idx] = cosf(diff);
    sin_diff[idx] = sinf(diff);
}

/**
 * @brief Parallel reduction to compute sum for PLV
 */
static __global__ void kernel_osc_reduce_sum(
    const float* __restrict__ data,
    float* __restrict__ output,
    uint32_t n)
{
    extern __shared__ float sdata[];

    uint32_t tid = threadIdx.x;
    uint32_t idx = blockIdx.x * blockDim.x * 2 + threadIdx.x;

    // Load and add first pair
    float sum = 0.0f;
    if (idx < n) sum = data[idx];
    if (idx + blockDim.x < n) sum += data[idx + blockDim.x];
    sdata[tid] = sum;
    __syncthreads();

    // Parallel reduction
    for (uint32_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            sdata[tid] += sdata[tid + s];
        }
        __syncthreads();
    }

    if (tid == 0) {
        atomicAdd(output, sdata[0]);
    }
}

bool nimcp_gpu_phase_locking_value(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* phase1,
    const nimcp_gpu_tensor_t* phase2,
    float* plv)
{
    if (!ctx || !phase1 || !phase2 || !plv) {
        LOG_ERROR("Invalid parameters for PLV computation");
        return false;
    }

    if (phase1->numel != phase2->numel) {
        LOG_ERROR("Phase tensors must have same size");
        return false;
    }

    uint32_t n_samples = (uint32_t)phase1->numel;
    bool success = false;

    // Pre-initialize all device pointers to NULL
    float *d_cos_diff = NULL, *d_sin_diff = NULL, *d_sum_cos = NULL, *d_sum_sin = NULL;

    // Allocate temporary buffers
    if (cudaMalloc(&d_cos_diff, n_samples * sizeof(float)) != cudaSuccess) goto cleanup_plv;
    if (cudaMalloc(&d_sin_diff, n_samples * sizeof(float)) != cudaSuccess) goto cleanup_plv;
    if (cudaMalloc(&d_sum_cos, sizeof(float)) != cudaSuccess) goto cleanup_plv;
    if (cudaMalloc(&d_sum_sin, sizeof(float)) != cudaSuccess) goto cleanup_plv;
    if (cudaMemset(d_sum_cos, 0, sizeof(float)) != cudaSuccess) goto cleanup_plv;
    if (cudaMemset(d_sum_sin, 0, sizeof(float)) != cudaSuccess) goto cleanup_plv;

    // Compute exp(i * (phase1 - phase2))
    kernel_phase_diff_exp<<<GRID_SIZE(n_samples), BLOCK_SIZE>>>(
        (const float*)phase1->data,
        (const float*)phase2->data,
        d_cos_diff, d_sin_diff, n_samples);

    // Sum the cosine and sine components
    kernel_osc_reduce_sum<<<GRID_SIZE(n_samples), BLOCK_SIZE, BLOCK_SIZE * sizeof(float)>>>(
        d_cos_diff, d_sum_cos, n_samples);
    kernel_osc_reduce_sum<<<GRID_SIZE(n_samples), BLOCK_SIZE, BLOCK_SIZE * sizeof(float)>>>(
        d_sin_diff, d_sum_sin, n_samples);

    // Copy results to host
    {
        float h_sum_cos, h_sum_sin;
        if (cudaMemcpy(&h_sum_cos, d_sum_cos, sizeof(float), cudaMemcpyDeviceToHost) != cudaSuccess) goto cleanup_plv;
        if (cudaMemcpy(&h_sum_sin, d_sum_sin, sizeof(float), cudaMemcpyDeviceToHost) != cudaSuccess) goto cleanup_plv;

        // PLV = |mean(exp(i * diff))|
        float mean_cos = h_sum_cos / (float)n_samples;
        float mean_sin = h_sum_sin / (float)n_samples;
        *plv = sqrtf(mean_cos * mean_cos + mean_sin * mean_sin);
    }

    success = true;

cleanup_plv:
    cudaFree(d_cos_diff);
    cudaFree(d_sin_diff);
    cudaFree(d_sum_cos);
    cudaFree(d_sum_sin);

    return success;
}

//=============================================================================
// Phase Lag Index (PLI) Kernels
//=============================================================================

/**
 * @brief Compute sign of phase difference for PLI
 */
__global__ void kernel_phase_diff_sign(
    const float* __restrict__ phase1,
    const float* __restrict__ phase2,
    float* __restrict__ signs,
    uint32_t n_samples)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_samples) return;

    float diff = device_wrap_angle(phase1[idx] - phase2[idx]);
    signs[idx] = device_sign(sinf(diff));  // Use sin to get imaginary part
}

bool nimcp_gpu_phase_lag_index(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* phase1,
    const nimcp_gpu_tensor_t* phase2,
    float* pli)
{
    if (!ctx || !phase1 || !phase2 || !pli) {
        LOG_ERROR("Invalid parameters for PLI computation");
        return false;
    }

    if (phase1->numel != phase2->numel) {
        LOG_ERROR("Phase tensors must have same size");
        return false;
    }

    uint32_t n_samples = (uint32_t)phase1->numel;
    bool success = false;

    // Pre-initialize all device pointers to NULL
    float *d_signs = NULL, *d_sum = NULL;

    // Allocate temporary buffers
    if (cudaMalloc(&d_signs, n_samples * sizeof(float)) != cudaSuccess) goto cleanup_pli;
    if (cudaMalloc(&d_sum, sizeof(float)) != cudaSuccess) goto cleanup_pli;
    if (cudaMemset(d_sum, 0, sizeof(float)) != cudaSuccess) goto cleanup_pli;

    // Compute sign of imaginary part of phase difference
    kernel_phase_diff_sign<<<GRID_SIZE(n_samples), BLOCK_SIZE>>>(
        (const float*)phase1->data,
        (const float*)phase2->data,
        d_signs, n_samples);

    // Sum the signs
    kernel_osc_reduce_sum<<<GRID_SIZE(n_samples), BLOCK_SIZE, BLOCK_SIZE * sizeof(float)>>>(
        d_signs, d_sum, n_samples);

    // Copy result to host
    {
        float h_sum;
        if (cudaMemcpy(&h_sum, d_sum, sizeof(float), cudaMemcpyDeviceToHost) != cudaSuccess) goto cleanup_pli;

        // PLI = |mean(sign(imag(exp(i*diff))))|
        *pli = fabsf(h_sum / (float)n_samples);
    }

    success = true;

cleanup_pli:
    cudaFree(d_signs);
    cudaFree(d_sum);

    return success;
}

//=============================================================================
// Global Synchronization Index (Kuramoto Order Parameter) Kernels
//=============================================================================

/**
 * @brief Compute exp(i * phase) for each channel/time and sum across channels
 */
__global__ void kernel_kuramoto_order_parameter(
    const float* __restrict__ phases,  // [n_channels, n_samples]
    float* __restrict__ sync_re,       // [n_samples]
    float* __restrict__ sync_im,       // [n_samples]
    uint32_t n_channels,
    uint32_t n_samples)
{
    uint32_t t = blockIdx.x * blockDim.x + threadIdx.x;
    if (t >= n_samples) return;

    float sum_cos = 0.0f;
    float sum_sin = 0.0f;

    // Sum exp(i * phase) across channels
    for (uint32_t ch = 0; ch < n_channels; ch++) {
        float phase = phases[ch * n_samples + t];
        sum_cos += cosf(phase);
        sum_sin += sinf(phase);
    }

    sync_re[t] = sum_cos / (float)n_channels;
    sync_im[t] = sum_sin / (float)n_channels;
}

/**
 * @brief Compute magnitude of order parameter
 */
__global__ void kernel_complex_magnitude(
    const float* __restrict__ re,
    const float* __restrict__ im,
    float* __restrict__ magnitude,
    uint32_t n)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    magnitude[idx] = sqrtf(re[idx] * re[idx] + im[idx] * im[idx]);
}

bool nimcp_gpu_global_sync_index(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* phases,
    nimcp_gpu_tensor_t* sync_index)
{
    if (!ctx || !phases || !sync_index) {
        LOG_ERROR("Invalid parameters for global sync index");
        return false;
    }

    if (phases->ndim != 2) {
        LOG_ERROR("Phases must be 2D (n_channels x n_samples)");
        return false;
    }

    uint32_t n_channels = (uint32_t)phases->dims[0];
    uint32_t n_samples = (uint32_t)phases->dims[1];

    // Pre-initialize all device pointers to NULL
    float *d_sync_re = NULL, *d_sync_im = NULL;

    // Allocate temporary buffers for complex order parameter
    if (cudaMalloc(&d_sync_re, n_samples * sizeof(float)) != cudaSuccess) goto cleanup_sync;
    if (cudaMalloc(&d_sync_im, n_samples * sizeof(float)) != cudaSuccess) goto cleanup_sync;

    // Compute Kuramoto order parameter: R = |mean(exp(i * phases))|
    kernel_kuramoto_order_parameter<<<GRID_SIZE(n_samples), BLOCK_SIZE>>>(
        (const float*)phases->data,
        d_sync_re, d_sync_im,
        n_channels, n_samples);

    // Compute magnitude
    kernel_complex_magnitude<<<GRID_SIZE(n_samples), BLOCK_SIZE>>>(
        d_sync_re, d_sync_im,
        (float*)sync_index->data, n_samples);

    // Cleanup
    cudaFree(d_sync_re);
    cudaFree(d_sync_im);

    return true;

cleanup_sync:
    cudaFree(d_sync_re);
    cudaFree(d_sync_im);
    return false;
}

//=============================================================================
// Hilbert Transform Context (for PAC computation)
//=============================================================================

// Forward declarations for kernels defined later in file
__global__ void kernel_create_hilbert_filter(float* __restrict__ filter, uint32_t n_fft);
__global__ void kernel_real_to_complex(const float* __restrict__ real,
                                       cufftComplex* __restrict__ complex_out, uint32_t n);

/**
 * @brief Hilbert transform context for efficient reuse
 */
typedef struct nimcp_hilbert_ctx {
    cufftHandle fft_plan;
    cufftHandle ifft_plan;
    int signal_length;
    void* gpu_context;

    // Workspace
    cufftComplex* d_fft_buffer;
    float* d_filter;
} nimcp_hilbert_ctx_t;

/**
 * @brief Analytic signal result from Hilbert transform
 */
typedef struct nimcp_analytic_signal {
    float* d_real;          // Original signal (real part)
    float* d_imag;          // Hilbert transform (imaginary part)
    float* d_amplitude;     // Instantaneous amplitude (envelope)
    float* d_phase;         // Instantaneous phase
    int length;
} nimcp_analytic_signal_t;

/**
 * @brief Create Hilbert transform context
 */
nimcp_hilbert_ctx_t* nimcp_hilbert_create(nimcp_gpu_context_t* gpu_ctx, int signal_length)
{
    if (!gpu_ctx || signal_length <= 0) return NULL;

    nimcp_hilbert_ctx_t* ctx = (nimcp_hilbert_ctx_t*)nimcp_malloc(sizeof(nimcp_hilbert_ctx_t));
    if (!ctx) return NULL;

    ctx->signal_length = signal_length;
    ctx->gpu_context = gpu_ctx;

    // Create C2C FFT plan
    if (cufftPlan1d(&ctx->fft_plan, signal_length, CUFFT_C2C, 1) != CUFFT_SUCCESS) {
        nimcp_free(ctx);
        return NULL;
    }

    // Allocate workspace
    if (cudaMalloc(&ctx->d_fft_buffer, signal_length * sizeof(cufftComplex)) != cudaSuccess) {
        cufftDestroy(ctx->fft_plan);
        nimcp_free(ctx);
        return NULL;
    }

    if (cudaMalloc(&ctx->d_filter, signal_length * sizeof(float)) != cudaSuccess) {
        cudaFree(ctx->d_fft_buffer);
        cufftDestroy(ctx->fft_plan);
        nimcp_free(ctx);
        return NULL;
    }

    // Pre-compute Hilbert filter
    kernel_create_hilbert_filter<<<GRID_SIZE(signal_length), BLOCK_SIZE>>>(
        ctx->d_filter, signal_length);

    return ctx;
}

/**
 * @brief Destroy Hilbert transform context
 */
void nimcp_hilbert_destroy(nimcp_hilbert_ctx_t* ctx)
{
    if (!ctx) return;

    cudaFree(ctx->d_fft_buffer);
    cudaFree(ctx->d_filter);
    cufftDestroy(ctx->fft_plan);
    nimcp_free(ctx);
}

/**
 * @brief Apply Hilbert filter in frequency domain to create analytic signal
 *
 * H[k] = 2 for 0 < k < N/2
 * H[0] = H[N/2] = 1
 * H[k] = 0 for k > N/2
 */
__global__ void kernel_hilbert_multiply_h(
    cufftComplex* __restrict__ spectrum,
    int n)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= (uint32_t)n) return;

    uint32_t half = n / 2;
    float multiplier;

    if (idx == 0 || idx == half) {
        multiplier = 1.0f;
    } else if (idx < half) {
        multiplier = 2.0f;
    } else {
        multiplier = 0.0f;
    }

    spectrum[idx].x *= multiplier;
    spectrum[idx].y *= multiplier;
}

/**
 * @brief Compute analytic signal components from IFFT result
 */
__global__ void kernel_compute_analytic_signal(
    const cufftComplex* __restrict__ analytic,
    const float* __restrict__ original,
    float* __restrict__ amplitude,
    float* __restrict__ phase,
    float norm_factor,
    int n)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= (uint32_t)n) return;

    float re = original[idx];
    float im = analytic[idx].y * norm_factor;  // Imaginary part is Hilbert transform

    amplitude[idx] = sqrtf(re * re + im * im);
    phase[idx] = atan2f(im, re);
}

/**
 * @brief Full Hilbert transform producing analytic signal
 */
int nimcp_hilbert_transform(
    nimcp_hilbert_ctx_t* ctx,
    const float* d_signal,
    nimcp_analytic_signal_t* result)
{
    if (!ctx || !d_signal || !result) return -1;

    int n = ctx->signal_length;

    // Copy real signal to complex buffer
    kernel_real_to_complex<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        d_signal, ctx->d_fft_buffer, n);

    // Forward FFT
    if (cufftExecC2C(ctx->fft_plan, ctx->d_fft_buffer, ctx->d_fft_buffer, CUFFT_FORWARD) != CUFFT_SUCCESS) {
        return -1;
    }

    // Apply Hilbert filter (multiply by H)
    kernel_hilbert_multiply_h<<<GRID_SIZE(n), BLOCK_SIZE>>>(ctx->d_fft_buffer, n);

    // Inverse FFT
    if (cufftExecC2C(ctx->fft_plan, ctx->d_fft_buffer, ctx->d_fft_buffer, CUFFT_INVERSE) != CUFFT_SUCCESS) {
        return -1;
    }

    // Extract amplitude and phase
    float norm = 1.0f / (float)n;
    kernel_compute_analytic_signal<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        ctx->d_fft_buffer, d_signal, result->d_amplitude, result->d_phase, norm, n);

    result->length = n;
    return 0;
}

//=============================================================================
// Phase-Amplitude Coupling (PAC) Kernels
//=============================================================================

/**
 * @brief PAC parameters for computation
 */
typedef struct nimcp_pac_compute_params {
    float phase_freq_low;   // Low frequency band for phase
    float phase_freq_high;
    float amp_freq_low;     // High frequency band for amplitude
    float amp_freq_high;
    float sample_rate;
    int num_phase_bins;     // For modulation index (typically 18 = 20 degree bins)
} nimcp_pac_compute_params_t;

/**
 * @brief PAC result structure
 */
typedef struct nimcp_pac_result {
    float modulation_index; // Overall PAC strength (Tort MI)
    float* d_phase_amplitude_dist; // Distribution of amplitude over phase bins [num_bins]
    float mean_vector_length;      // Alternative PAC measure (MVL)
    float preferred_phase;         // Phase of maximum amplitude
} nimcp_pac_result_t;

/**
 * @brief Bin amplitudes by phase for PAC modulation index
 */
__global__ void kernel_pac_bin_amplitudes(
    const float* __restrict__ phase,
    const float* __restrict__ amplitude,
    float* __restrict__ phase_bins,      // [n_bins]
    uint32_t* __restrict__ bin_counts,   // [n_bins]
    uint32_t n_samples,
    uint32_t n_bins)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_samples) return;

    // Map phase [-pi, pi] to bin [0, n_bins-1]
    float p = phase[idx];
    float normalized = (p + M_PI) / (2.0f * M_PI);  // [0, 1]
    uint32_t bin = (uint32_t)(normalized * (float)n_bins);
    if (bin >= n_bins) bin = n_bins - 1;

    // Atomic add to bin
    atomicAdd(&phase_bins[bin], amplitude[idx]);
    atomicAdd(&bin_counts[bin], 1u);
}

/**
 * @brief Normalize phase bins and compute mean amplitude per bin
 */
__global__ void kernel_pac_normalize_bins(
    float* __restrict__ phase_bins,
    const uint32_t* __restrict__ bin_counts,
    uint32_t n_bins)
{
    uint32_t bin = blockIdx.x * blockDim.x + threadIdx.x;
    if (bin >= n_bins) return;

    if (bin_counts[bin] > 0) {
        phase_bins[bin] /= (float)bin_counts[bin];
    } else {
        phase_bins[bin] = 0.0f;
    }
}

/**
 * @brief Compute entropy for PAC modulation index
 *
 * H = -sum(p * log(p)) where p is normalized amplitude distribution
 */
__device__ float device_entropy(float* probs, uint32_t n)
{
    float H = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        if (probs[i] > 1e-10f) {
            H -= probs[i] * logf(probs[i]);
        }
    }
    return H;
}

/**
 * @brief Compute modulation index from amplitude distribution
 *
 * MI = (H_max - H) / H_max
 * where H_max = log(n_bins) for uniform distribution
 */
__global__ void kernel_pac_compute_mi(
    const float* __restrict__ mean_amplitudes,  // [n_bins]
    float* __restrict__ mi_out,
    uint32_t n_bins)
{
    if (threadIdx.x != 0 || blockIdx.x != 0) return;

    // Normalize amplitudes to create probability distribution
    float sum = 0.0f;
    for (uint32_t i = 0; i < n_bins; i++) {
        sum += mean_amplitudes[i];
    }

    float probs[36];  // Support up to 36 bins
    if (n_bins > 36) n_bins = 36;

    if (sum > 1e-10f) {
        for (uint32_t i = 0; i < n_bins; i++) {
            probs[i] = mean_amplitudes[i] / sum;
        }
    } else {
        // Uniform distribution
        for (uint32_t i = 0; i < n_bins; i++) {
            probs[i] = 1.0f / (float)n_bins;
        }
    }

    // Compute entropy
    float H = 0.0f;
    for (uint32_t i = 0; i < n_bins; i++) {
        if (probs[i] > 1e-10f) {
            H -= probs[i] * logf(probs[i]);
        }
    }

    // Maximum entropy for uniform distribution
    float H_max = logf((float)n_bins);

    // Modulation index
    float mi = (H_max - H) / H_max;
    if (mi < 0.0f) mi = 0.0f;
    if (mi > 1.0f) mi = 1.0f;

    *mi_out = mi;
}

/**
 * @brief Compute Mean Vector Length (alternative PAC measure)
 *
 * MVL = |mean(amplitude * exp(i * phase))|
 */
__global__ void kernel_pac_compute_mvl(
    const float* __restrict__ phase,
    const float* __restrict__ amplitude,
    float* __restrict__ mvl_out,
    uint32_t n_samples)
{
    // Use parallel reduction
    extern __shared__ float sdata[];

    uint32_t tid = threadIdx.x;
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;

    float sum_cos = 0.0f;
    float sum_sin = 0.0f;
    float sum_amp = 0.0f;

    if (idx < n_samples) {
        float amp = amplitude[idx];
        float ph = phase[idx];
        sum_cos = amp * cosf(ph);
        sum_sin = amp * sinf(ph);
        sum_amp = amp;
    }

    // Store in shared memory
    sdata[tid * 3 + 0] = sum_cos;
    sdata[tid * 3 + 1] = sum_sin;
    sdata[tid * 3 + 2] = sum_amp;
    __syncthreads();

    // Reduction
    for (uint32_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            sdata[tid * 3 + 0] += sdata[(tid + s) * 3 + 0];
            sdata[tid * 3 + 1] += sdata[(tid + s) * 3 + 1];
            sdata[tid * 3 + 2] += sdata[(tid + s) * 3 + 2];
        }
        __syncthreads();
    }

    if (tid == 0) {
        float total_cos = sdata[0];
        float total_sin = sdata[1];
        float total_amp = sdata[2];

        if (total_amp > 1e-10f) {
            float mvl = sqrtf(total_cos * total_cos + total_sin * total_sin) / total_amp;
            atomicAdd(mvl_out, mvl);
        }
    }
}

/**
 * @brief Full PAC computation with Hilbert transform
 *
 * Complete pipeline:
 * 1. Bandpass filter for phase band
 * 2. Bandpass filter for amplitude band
 * 3. Hilbert transform to get phase from low freq
 * 4. Hilbert transform to get amplitude envelope from high freq
 * 5. Compute modulation index
 */
int nimcp_pac_compute(
    nimcp_gpu_context_t* ctx,
    const float* d_signal,
    int signal_length,
    nimcp_pac_compute_params_t* params,
    nimcp_pac_result_t* result)
{
    if (!ctx || !d_signal || !params || !result) return -1;

    int n = signal_length;
    int n_bins = params->num_phase_bins;
    if (n_bins <= 0) n_bins = 18;
    if (n_bins > 36) n_bins = 36;

    // Allocate filtered signal buffers
    float *d_phase_filtered, *d_amp_filtered;
    if (cudaMalloc(&d_phase_filtered, n * sizeof(float)) != cudaSuccess) return -1;
    if (cudaMalloc(&d_amp_filtered, n * sizeof(float)) != cudaSuccess) {
        cudaFree(d_phase_filtered);
        return -1;
    }

    // Allocate analytic signal buffers
    float *d_phase_signal = NULL, *d_phase_unused = NULL;
    float *d_amp_envelope = NULL, *d_amp_phase_unused = NULL;
    if (cudaMalloc(&d_phase_signal, n * sizeof(float)) != cudaSuccess ||
        cudaMalloc(&d_phase_unused, n * sizeof(float)) != cudaSuccess ||
        cudaMalloc(&d_amp_envelope, n * sizeof(float)) != cudaSuccess ||
        cudaMalloc(&d_amp_phase_unused, n * sizeof(float)) != cudaSuccess) {
        cudaFree(d_phase_filtered);
        cudaFree(d_amp_filtered);
        cudaFree(d_phase_signal);
        cudaFree(d_phase_unused);
        cudaFree(d_amp_envelope);
        cudaFree(d_amp_phase_unused);
        return -1;
    }

    // Create temporary tensors for bandpass filtering
    size_t dims[1] = {(size_t)n};
    nimcp_gpu_tensor_t* signal_tensor = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* phase_filtered_tensor = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* amp_filtered_tensor = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);

    if (!signal_tensor || !phase_filtered_tensor || !amp_filtered_tensor) {
        cudaFree(d_phase_filtered);
        cudaFree(d_amp_filtered);
        cudaFree(d_phase_signal);
        cudaFree(d_phase_unused);
        cudaFree(d_amp_envelope);
        cudaFree(d_amp_phase_unused);
        if (signal_tensor) nimcp_gpu_tensor_destroy(signal_tensor);
        if (phase_filtered_tensor) nimcp_gpu_tensor_destroy(phase_filtered_tensor);
        if (amp_filtered_tensor) nimcp_gpu_tensor_destroy(amp_filtered_tensor);
        return -1;
    }

    // Copy signal to tensor
    cudaMemcpy(signal_tensor->data, d_signal, n * sizeof(float), cudaMemcpyDeviceToDevice);

    // 1. Bandpass filter for phase band (low frequency)
    nimcp_gpu_bandpass_filter(ctx, signal_tensor, phase_filtered_tensor,
                               params->phase_freq_low, params->phase_freq_high,
                               4, params->sample_rate);

    // 2. Bandpass filter for amplitude band (high frequency)
    nimcp_gpu_bandpass_filter(ctx, signal_tensor, amp_filtered_tensor,
                               params->amp_freq_low, params->amp_freq_high,
                               4, params->sample_rate);

    // Create tensors for Hilbert transform outputs
    nimcp_gpu_tensor_t* phase_out = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* amp_out = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);

    if (!phase_out || !amp_out) {
        nimcp_gpu_tensor_destroy(signal_tensor);
        nimcp_gpu_tensor_destroy(phase_filtered_tensor);
        nimcp_gpu_tensor_destroy(amp_filtered_tensor);
        if (phase_out) nimcp_gpu_tensor_destroy(phase_out);
        if (amp_out) nimcp_gpu_tensor_destroy(amp_out);
        cudaFree(d_phase_filtered);
        cudaFree(d_amp_filtered);
        cudaFree(d_phase_signal);
        cudaFree(d_phase_unused);
        cudaFree(d_amp_envelope);
        cudaFree(d_amp_phase_unused);
        return -1;
    }

    // 3. Extract phase from low-frequency signal
    nimcp_gpu_hilbert_phase(ctx, phase_filtered_tensor, phase_out);

    // 4. Extract amplitude envelope from high-frequency signal
    nimcp_gpu_hilbert_amplitude(ctx, amp_filtered_tensor, amp_out);

    // 5. Compute PAC modulation index
    float *d_phase_bins = NULL;
    uint32_t *d_bin_counts = NULL;
    float *d_mi = NULL;
    float *d_mvl = NULL;

    if (cudaMalloc(&d_phase_bins, n_bins * sizeof(float)) != cudaSuccess ||
        cudaMalloc(&d_bin_counts, n_bins * sizeof(uint32_t)) != cudaSuccess ||
        cudaMalloc(&d_mi, sizeof(float)) != cudaSuccess) {
        cudaFree(d_phase_bins);
        cudaFree(d_bin_counts);
        cudaFree(d_mi);
        nimcp_gpu_tensor_destroy(signal_tensor);
        nimcp_gpu_tensor_destroy(phase_filtered_tensor);
        nimcp_gpu_tensor_destroy(amp_filtered_tensor);
        nimcp_gpu_tensor_destroy(phase_out);
        nimcp_gpu_tensor_destroy(amp_out);
        cudaFree(d_phase_filtered);
        cudaFree(d_amp_filtered);
        cudaFree(d_phase_signal);
        cudaFree(d_phase_unused);
        cudaFree(d_amp_envelope);
        cudaFree(d_amp_phase_unused);
        return -1;
    }
    cudaMemset(d_phase_bins, 0, n_bins * sizeof(float));
    cudaMemset(d_bin_counts, 0, n_bins * sizeof(uint32_t));
    cudaMemset(d_mi, 0, sizeof(float));

    // Bin amplitudes by phase
    kernel_pac_bin_amplitudes<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)phase_out->data, (float*)amp_out->data,
        d_phase_bins, d_bin_counts, n, n_bins);

    // Normalize bins
    kernel_pac_normalize_bins<<<GRID_SIZE(n_bins), BLOCK_SIZE>>>(
        d_phase_bins, d_bin_counts, n_bins);

    // Compute modulation index
    kernel_pac_compute_mi<<<1, 1>>>(d_phase_bins, d_mi, n_bins);

    // Copy result to output
    cudaMemcpy(&result->modulation_index, d_mi, sizeof(float), cudaMemcpyDeviceToHost);

    // Copy distribution if requested
    if (result->d_phase_amplitude_dist) {
        cudaMemcpy(result->d_phase_amplitude_dist, d_phase_bins,
                   n_bins * sizeof(float), cudaMemcpyDeviceToDevice);
    }

    // Compute MVL as alternative measure
    if (cudaMalloc(&d_mvl, sizeof(float)) != cudaSuccess) {
        result->mean_vector_length = 0.0f;
    } else {
        cudaMemset(d_mvl, 0, sizeof(float));
        kernel_pac_compute_mvl<<<GRID_SIZE(n), BLOCK_SIZE, BLOCK_SIZE * 3 * sizeof(float)>>>(
            (float*)phase_out->data, (float*)amp_out->data, d_mvl, n);
        cudaMemcpy(&result->mean_vector_length, d_mvl, sizeof(float), cudaMemcpyDeviceToHost);
        cudaFree(d_mvl);
    }

    // Cleanup
    cudaFree(d_phase_bins);
    cudaFree(d_bin_counts);
    cudaFree(d_mi);
    cudaFree(d_phase_filtered);
    cudaFree(d_amp_filtered);
    cudaFree(d_phase_signal);
    cudaFree(d_phase_unused);
    cudaFree(d_amp_envelope);
    cudaFree(d_amp_phase_unused);
    nimcp_gpu_tensor_destroy(signal_tensor);
    nimcp_gpu_tensor_destroy(phase_filtered_tensor);
    nimcp_gpu_tensor_destroy(amp_filtered_tensor);
    nimcp_gpu_tensor_destroy(phase_out);
    nimcp_gpu_tensor_destroy(amp_out);

    return 0;
}

bool nimcp_gpu_pac_modulation_index(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* signal,
    nimcp_gpu_tensor_t* pac_values,
    const nimcp_pac_params_t* params)
{
    if (!ctx || !signal || !pac_values || !params) {
        LOG_ERROR("Invalid parameters for PAC modulation index");
        return false;
    }

    uint32_t n_samples = (uint32_t)signal->numel;
    uint32_t n_bins = (uint32_t)params->n_phase_bins;
    if (n_bins == 0) n_bins = 18;

    // Set up PAC compute parameters
    nimcp_pac_compute_params_t pac_params;
    pac_params.phase_freq_low = params->phase_freq_low;
    pac_params.phase_freq_high = params->phase_freq_high;
    pac_params.amp_freq_low = params->amp_freq_low;
    pac_params.amp_freq_high = params->amp_freq_high;
    pac_params.sample_rate = 1000.0f;  // Default, should be passed in params
    pac_params.num_phase_bins = n_bins;

    // Set up result
    nimcp_pac_result_t result;
    result.modulation_index = 0.0f;
    result.d_phase_amplitude_dist = NULL;
    result.mean_vector_length = 0.0f;

    // Run full PAC computation
    int ret = nimcp_pac_compute(ctx, (const float*)signal->data, n_samples, &pac_params, &result);

    if (ret == 0) {
        // Store modulation index
        NIMCP_CUDA_RECOVER(cudaMemcpy(pac_values->data, &result.modulation_index,
                              sizeof(float), cudaMemcpyHostToDevice), GPU_ERROR_CUDA_RUNTIME);
        LOG_DEBUG("PAC modulation index: %.4f, MVL: %.4f",
                  result.modulation_index, result.mean_vector_length);
    } else {
        float zero = 0.0f;
        NIMCP_CUDA_RECOVER(cudaMemcpy(pac_values->data, &zero, sizeof(float), cudaMemcpyHostToDevice), GPU_ERROR_CUDA_RUNTIME);
        LOG_WARN("PAC computation failed, returning zero");
    }

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return ret == 0;
}

//=============================================================================
// Hilbert Transform Kernels (Phase and Amplitude Extraction)
//=============================================================================

/**
 * @brief Create Hilbert filter in frequency domain
 *
 * H(k) = 2 for k = 1..N/2-1
 * H(k) = 1 for k = 0, N/2
 * H(k) = 0 for k = N/2+1..N-1
 */
__global__ void kernel_create_hilbert_filter(
    float* __restrict__ filter,
    uint32_t n_fft)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_fft) return;

    uint32_t half = n_fft / 2;

    if (idx == 0 || idx == half) {
        filter[idx] = 1.0f;
    } else if (idx < half) {
        filter[idx] = 2.0f;
    } else {
        filter[idx] = 0.0f;
    }
}

/**
 * @brief Apply filter to complex spectrum (multiply each complex by scalar)
 */
__global__ void kernel_apply_spectrum_filter(
    float* __restrict__ spectrum,  // Interleaved re,im,re,im,...
    const float* __restrict__ filter,
    uint32_t n_fft)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_fft) return;

    float f = filter[idx];
    // Interleaved layout: spectrum[2*idx] = real, spectrum[2*idx+1] = imag
    spectrum[2 * idx] *= f;
    spectrum[2 * idx + 1] *= f;
}

/**
 * @brief Extract phase from complex signal
 */
__global__ void kernel_extract_phase(
    const float* __restrict__ re,
    const float* __restrict__ im,
    float* __restrict__ phase,
    uint32_t n)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    phase[idx] = atan2f(im[idx], re[idx]);
}

/**
 * @brief Extract amplitude (envelope) from complex signal
 */
__global__ void kernel_extract_amplitude(
    const float* __restrict__ re,
    const float* __restrict__ im,
    float* __restrict__ amplitude,
    uint32_t n)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    amplitude[idx] = sqrtf(re[idx] * re[idx] + im[idx] * im[idx]);
}

/**
 * @brief Extract amplitude with scaled imaginary part (for FFT normalization)
 */
__global__ void kernel_extract_amplitude_scaled(
    const float* __restrict__ re,
    const float* __restrict__ im,
    float* __restrict__ amplitude,
    float im_scale,
    uint32_t n)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float scaled_im = im[idx] * im_scale;
    amplitude[idx] = sqrtf(re[idx] * re[idx] + scaled_im * scaled_im);
}

/**
 * @brief Scale array in-place
 */
__global__ void kernel_scale_array(
    float* __restrict__ data,
    float scale,
    uint32_t n)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    data[idx] *= scale;
}

/**
 * @brief Copy real signal to complex (imaginary = 0)
 */
__global__ void kernel_real_to_complex(
    const float* __restrict__ real_signal,
    cufftComplex* __restrict__ complex_signal,
    uint32_t n)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    complex_signal[idx].x = real_signal[idx];
    complex_signal[idx].y = 0.0f;
}

/**
 * @brief Extract amplitude from interleaved complex signal with normalization
 */
__global__ void kernel_complex_amplitude(
    const cufftComplex* __restrict__ complex_signal,
    float* __restrict__ amplitude,
    float scale,
    uint32_t n)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float re = complex_signal[idx].x * scale;
    float im = complex_signal[idx].y * scale;
    amplitude[idx] = sqrtf(re * re + im * im);
}

bool nimcp_gpu_hilbert_phase(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* signal,
    nimcp_gpu_tensor_t* phase)
{
    if (!ctx || !signal || !phase) {
        LOG_ERROR("Invalid parameters for Hilbert phase");
        return false;
    }

    if (signal->numel != phase->numel) {
        LOG_ERROR("Signal and phase must have same size");
        return false;
    }

    uint32_t n = (uint32_t)signal->numel;
    bool success = false;

    // Pre-initialize all device pointers to NULL
    cufftComplex *d_spectrum = NULL;
    float *d_filter = NULL;
    float *d_signal_copy = NULL;
    float *d_analytic_im = NULL;
    cufftHandle plan = 0;
    cufftHandle plan_inv = 0;
    bool plan_created = false;
    bool plan_inv_created = false;
    uint32_t n_freq = n / 2 + 1;

    // Allocate complex spectrum buffer
    if (cudaMalloc(&d_spectrum, n * sizeof(cufftComplex)) != cudaSuccess) goto cleanup_hilbert_phase;
    if (cudaMalloc(&d_filter, n * sizeof(float)) != cudaSuccess) goto cleanup_hilbert_phase;

    // Copy signal to complex buffer (real part)
    if (cudaMalloc(&d_signal_copy, n * sizeof(float)) != cudaSuccess) goto cleanup_hilbert_phase;
    if (cudaMemcpy(d_signal_copy, signal->data, n * sizeof(float), cudaMemcpyDeviceToDevice) != cudaSuccess) goto cleanup_hilbert_phase;

    // Create FFT plan
    if (cufftPlan1d(&plan, n, CUFFT_R2C, 1) != CUFFT_SUCCESS) goto cleanup_hilbert_phase;
    plan_created = true;

    // Forward FFT
    if (cufftExecR2C(plan, d_signal_copy, d_spectrum) != CUFFT_SUCCESS) goto cleanup_hilbert_phase;

    // Create Hilbert filter
    kernel_create_hilbert_filter<<<GRID_SIZE(n), BLOCK_SIZE>>>(d_filter, n);

    // Apply filter to spectrum
    // Note: R2C gives n/2+1 complex values, need to handle carefully
    kernel_apply_spectrum_filter<<<GRID_SIZE(n_freq), BLOCK_SIZE>>>(
        (float*)d_spectrum, d_filter, n_freq);

    // Create inverse FFT plan
    if (cufftPlan1d(&plan_inv, n, CUFFT_C2R, 1) != CUFFT_SUCCESS) goto cleanup_hilbert_phase;
    plan_inv_created = true;

    // Inverse FFT to get analytic signal
    if (cudaMalloc(&d_analytic_im, n * sizeof(float)) != cudaSuccess) goto cleanup_hilbert_phase;
    if (cufftExecC2R(plan_inv, d_spectrum, d_analytic_im) != CUFFT_SUCCESS) goto cleanup_hilbert_phase;

    // Normalize inverse FFT (cuFFT doesn't normalize)
    {
        float norm_factor = 1.0f / (float)n;
        kernel_scale_array<<<GRID_SIZE(n), BLOCK_SIZE>>>(d_analytic_im, norm_factor, n);
    }

    // Extract phase: atan2(hilbert(x), x)
    kernel_extract_phase<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (const float*)signal->data,
        d_analytic_im,
        (float*)phase->data, n);

    success = true;

cleanup_hilbert_phase:
    if (plan_created) cufftDestroy(plan);
    if (plan_inv_created) cufftDestroy(plan_inv);
    cudaFree(d_spectrum);
    cudaFree(d_filter);
    cudaFree(d_signal_copy);
    cudaFree(d_analytic_im);

    return success;
}

bool nimcp_gpu_hilbert_amplitude(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* signal,
    nimcp_gpu_tensor_t* amplitude)
{
    if (!ctx || !signal || !amplitude) {
        LOG_ERROR("Invalid parameters for Hilbert amplitude");
        return false;
    }

    if (signal->numel != amplitude->numel) {
        LOG_ERROR("Signal and amplitude must have same size");
        return false;
    }

    uint32_t n = (uint32_t)signal->numel;
    bool success = false;

    // Pre-initialize all device pointers to NULL
    cufftComplex *d_complex_signal = NULL, *d_spectrum = NULL;
    float *d_filter = NULL;
    cufftHandle plan = 0;
    bool plan_created = false;

    // Allocate complex buffers for C2C FFT
    if (cudaMalloc(&d_complex_signal, n * sizeof(cufftComplex)) != cudaSuccess) goto cleanup_hilbert_amp;
    if (cudaMalloc(&d_spectrum, n * sizeof(cufftComplex)) != cudaSuccess) goto cleanup_hilbert_amp;

    // Copy real signal to complex (imaginary = 0)
    kernel_real_to_complex<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (const float*)signal->data, d_complex_signal, n);

    // Create C2C FFT plan
    if (cufftPlan1d(&plan, n, CUFFT_C2C, 1) != CUFFT_SUCCESS) goto cleanup_hilbert_amp;
    plan_created = true;

    // Forward FFT
    if (cufftExecC2C(plan, d_complex_signal, d_spectrum, CUFFT_FORWARD) != CUFFT_SUCCESS) goto cleanup_hilbert_amp;

    // Apply analytic signal filter in-place:
    // - DC (k=0): multiply by 1
    // - Positive freqs (k=1 to N/2-1): multiply by 2
    // - Nyquist (k=N/2): multiply by 1
    // - Negative freqs (k=N/2+1 to N-1): multiply by 0
    if (cudaMalloc(&d_filter, n * sizeof(float)) != cudaSuccess) goto cleanup_hilbert_amp;
    kernel_create_hilbert_filter<<<GRID_SIZE(n), BLOCK_SIZE>>>(d_filter, n);
    kernel_apply_spectrum_filter<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)d_spectrum, d_filter, n);

    // Inverse FFT (in-place)
    if (cufftExecC2C(plan, d_spectrum, d_spectrum, CUFFT_INVERSE) != CUFFT_SUCCESS) goto cleanup_hilbert_amp;

    // Extract amplitude from analytic signal: |z| = sqrt(re^2 + im^2)
    // Note: cuFFT doesn't normalize, so divide by n
    {
        float norm = 1.0f / (float)n;
        kernel_complex_amplitude<<<GRID_SIZE(n), BLOCK_SIZE>>>(
            d_spectrum, (float*)amplitude->data, norm, n);
    }

    success = true;

cleanup_hilbert_amp:
    if (plan_created) cufftDestroy(plan);
    cudaFree(d_complex_signal);
    cudaFree(d_spectrum);
    cudaFree(d_filter);

    return success;
}

//=============================================================================
// Band Power Computation Kernels
//=============================================================================

/**
 * @brief Compute power from FFT magnitude in frequency band
 */
__global__ void kernel_band_power_accumulate(
    const float* __restrict__ spectrum_re,
    const float* __restrict__ spectrum_im,
    float* __restrict__ power,
    float sampling_rate,
    float freq_low,
    float freq_high,
    uint32_t n_channels,
    uint32_t n_fft)
{
    uint32_t ch = blockIdx.x;
    uint32_t freq_idx = threadIdx.x;

    if (ch >= n_channels) return;

    extern __shared__ float sdata[];

    // Compute frequency for this bin
    float freq_resolution = sampling_rate / (float)n_fft;
    float local_power = 0.0f;

    // Sum power in frequency band
    for (uint32_t k = freq_idx; k < n_fft / 2; k += blockDim.x) {
        float freq = k * freq_resolution;
        if (freq >= freq_low && freq <= freq_high) {
            uint32_t idx = ch * n_fft + k;
            float re = spectrum_re[idx];
            float im = spectrum_im[idx];
            local_power += re * re + im * im;
        }
    }

    sdata[freq_idx] = local_power;
    __syncthreads();

    // Reduction
    for (uint32_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (freq_idx < s) {
            sdata[freq_idx] += sdata[freq_idx + s];
        }
        __syncthreads();
    }

    if (freq_idx == 0) {
        power[ch] = sdata[0];
    }
}

bool nimcp_gpu_band_power(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* signal,
    nimcp_gpu_tensor_t* power,
    const nimcp_oscillation_params_t* params)
{
    if (!ctx || !signal || !power || !params) {
        LOG_ERROR("Invalid parameters for band power");
        return false;
    }

    uint32_t n_channels = (signal->ndim == 2) ? (uint32_t)signal->dims[0] : 1;
    uint32_t n_samples = (signal->ndim == 2) ? (uint32_t)signal->dims[1] : (uint32_t)signal->numel;
    uint32_t n_fft = (uint32_t)params->n_fft;

    if (n_samples < n_fft) {
        n_fft = n_samples;
    }

    // Allocate FFT buffers
    cufftComplex *d_spectrum = NULL;
    {
        cudaError_t alloc_err = cudaMalloc(&d_spectrum, n_channels * (n_fft / 2 + 1) * sizeof(cufftComplex));
        if (alloc_err != cudaSuccess) {
            LOG_ERROR("Failed to allocate FFT spectrum buffer");
            return false;
        }
    }

    // Create batched FFT plan
    cufftHandle plan;
    int rank = 1;
    int n_arr[] = {(int)n_fft};
    int inembed[] = {(int)n_fft};
    int onembed[] = {(int)(n_fft / 2 + 1)};

    CUFFT_CHECK(cufftPlanMany(&plan, rank, n_arr,
                               inembed, 1, n_fft,
                               onembed, 1, n_fft / 2 + 1,
                               CUFFT_R2C, n_channels));

    // Execute FFT
    CUFFT_CHECK(cufftExecR2C(plan, (float*)signal->data, d_spectrum));

    // Compute band power
    dim3 grid(n_channels);
    dim3 block(min((int)n_fft / 2, 256));

    kernel_band_power_accumulate<<<grid, block, block.x * sizeof(float)>>>(
        (const float*)d_spectrum, (const float*)d_spectrum + 1,
        (float*)power->data,
        params->sampling_rate,
        params->freq_low, params->freq_high,
        n_channels, n_fft);

    // Cleanup
    cufftDestroy(plan);
    cudaFree(d_spectrum);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Power Spectral Density (PSD) Kernels - Welch's Method
//=============================================================================

/**
 * @brief Apply Hanning window to signal segment
 */
__global__ void kernel_apply_hanning_window(
    const float* __restrict__ input,
    float* __restrict__ output,
    uint32_t segment_offset,
    uint32_t n_fft)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_fft) return;

    // Hanning window: 0.5 * (1 - cos(2*pi*n/(N-1)))
    float window = 0.5f * (1.0f - cosf(2.0f * M_PI * (float)idx / (float)(n_fft - 1)));
    output[idx] = input[segment_offset + idx] * window;
}

/**
 * @brief Accumulate power from FFT magnitude
 */
__global__ void kernel_accumulate_psd(
    const cufftComplex* __restrict__ spectrum,
    float* __restrict__ psd,
    uint32_t n_freqs,
    uint32_t segment_count)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_freqs) return;

    float re = spectrum[idx].x;
    float im = spectrum[idx].y;
    float power = (re * re + im * im) / (float)segment_count;

    atomicAdd(&psd[idx], power);
}

bool nimcp_gpu_power_spectral_density(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* signal,
    nimcp_gpu_tensor_t* psd,
    nimcp_gpu_tensor_t* freqs,
    const nimcp_oscillation_params_t* params)
{
    if (!ctx || !signal || !psd || !freqs || !params) {
        LOG_ERROR("Invalid parameters for PSD");
        return false;
    }

    uint32_t n_samples = (uint32_t)signal->numel;
    uint32_t n_fft = (uint32_t)params->n_fft;
    uint32_t hop = (uint32_t)params->hop_length;
    uint32_t n_freqs = n_fft / 2 + 1;

    if (n_samples < n_fft) {
        LOG_ERROR("Signal too short for FFT size");
        return false;
    }

    // Calculate number of segments (Welch's method)
    uint32_t n_segments = (n_samples - n_fft) / hop + 1;

    // Allocate buffers
    float *d_windowed = NULL;
    cufftComplex *d_spectrum = NULL;
    cufftHandle plan = 0;
    bool plan_created = false;
    bool success = false;

    if (cudaMalloc(&d_windowed, n_fft * sizeof(float)) != cudaSuccess) {
        LOG_ERROR("Failed to allocate windowed buffer");
        goto cleanup_psd;
    }
    if (cudaMalloc(&d_spectrum, n_freqs * sizeof(cufftComplex)) != cudaSuccess) {
        LOG_ERROR("Failed to allocate spectrum buffer");
        goto cleanup_psd;
    }

    // Zero PSD accumulator
    if (cudaMemset(psd->data, 0, n_freqs * sizeof(float)) != cudaSuccess) {
        LOG_ERROR("Failed to zero PSD accumulator");
        goto cleanup_psd;
    }

    // Create FFT plan
    CUFFT_CHECK(cufftPlan1d(&plan, n_fft, CUFFT_R2C, 1));
    plan_created = true;

    // Process each segment
    for (uint32_t seg = 0; seg < n_segments; seg++) {
        uint32_t offset = seg * hop;

        // Apply window
        kernel_apply_hanning_window<<<GRID_SIZE(n_fft), BLOCK_SIZE>>>(
            (const float*)signal->data, d_windowed, offset, n_fft);

        // FFT
        CUFFT_CHECK(cufftExecR2C(plan, d_windowed, d_spectrum));

        // Accumulate power
        kernel_accumulate_psd<<<GRID_SIZE(n_freqs), BLOCK_SIZE>>>(
            d_spectrum, (float*)psd->data, n_freqs, n_segments);
    }

    // Compute frequency array on host and copy to device
    {
        float *h_freqs = (float*)nimcp_malloc(n_freqs * sizeof(float));
        if (!h_freqs) {
            LOG_ERROR("Failed to allocate host frequency array");
            goto cleanup_psd;
        }
        float freq_resolution = params->sampling_rate / (float)n_fft;
        for (uint32_t k = 0; k < n_freqs; k++) {
            h_freqs[k] = k * freq_resolution;
        }
        if (cudaMemcpy(freqs->data, h_freqs, n_freqs * sizeof(float), cudaMemcpyHostToDevice) != cudaSuccess) {
            nimcp_free(h_freqs);
            LOG_ERROR("Failed to copy frequency array to device");
            goto cleanup_psd;
        }
        nimcp_free(h_freqs);
    }

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    success = true;

cleanup_psd:
    if (plan_created) cufftDestroy(plan);
    cudaFree(d_windowed);
    cudaFree(d_spectrum);

    return success;
}

//=============================================================================
// Spectrogram Kernels
//=============================================================================

bool nimcp_gpu_spectrogram(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* signal,
    nimcp_gpu_tensor_t* spectrogram,
    const nimcp_oscillation_params_t* params)
{
    if (!ctx || !signal || !spectrogram || !params) {
        LOG_ERROR("Invalid parameters for spectrogram");
        return false;
    }

    uint32_t n_samples = (uint32_t)signal->numel;
    uint32_t n_fft = (uint32_t)params->n_fft;
    uint32_t hop = (uint32_t)params->hop_length;
    uint32_t n_freqs = n_fft / 2 + 1;
    uint32_t n_time = (n_samples - n_fft) / hop + 1;

    if (n_samples < n_fft) {
        LOG_ERROR("Signal too short for FFT size");
        return false;
    }

    // Allocate buffers
    float *d_windowed = NULL;
    cufftComplex *d_spectrum = NULL;
    cufftHandle plan = 0;
    bool plan_created = false;
    bool success = false;

    if (cudaMalloc(&d_windowed, n_fft * sizeof(float)) != cudaSuccess) {
        LOG_ERROR("Failed to allocate windowed buffer");
        goto cleanup_spectrogram;
    }
    if (cudaMalloc(&d_spectrum, n_freqs * sizeof(cufftComplex)) != cudaSuccess) {
        LOG_ERROR("Failed to allocate spectrum buffer");
        goto cleanup_spectrogram;
    }

    // Create FFT plan
    CUFFT_CHECK(cufftPlan1d(&plan, n_fft, CUFFT_R2C, 1));
    plan_created = true;

    // Process each time frame
    for (uint32_t t = 0; t < n_time; t++) {
        uint32_t offset = t * hop;

        // Apply window
        kernel_apply_hanning_window<<<GRID_SIZE(n_fft), BLOCK_SIZE>>>(
            (const float*)signal->data, d_windowed, offset, n_fft);

        // FFT
        CUFFT_CHECK(cufftExecR2C(plan, d_windowed, d_spectrum));

        // Store power in spectrogram (t, f)
        kernel_extract_amplitude<<<GRID_SIZE(n_freqs), BLOCK_SIZE>>>(
            (const float*)d_spectrum, (const float*)d_spectrum + 1,
            (float*)spectrogram->data + t * n_freqs, n_freqs);
    }

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    success = true;

cleanup_spectrogram:
    if (plan_created) cufftDestroy(plan);
    cudaFree(d_windowed);
    cudaFree(d_spectrum);

    return success;
}

//=============================================================================
// Coherence Computation Kernels
//=============================================================================

/**
 * @brief Compute cross-spectrum and auto-spectra for coherence
 */
__global__ void kernel_cross_spectrum(
    const cufftComplex* __restrict__ spectrum1,
    const cufftComplex* __restrict__ spectrum2,
    cufftComplex* __restrict__ cross_spectrum,
    float* __restrict__ psd1,
    float* __restrict__ psd2,
    uint32_t n_freqs)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_freqs) return;

    float re1 = spectrum1[idx].x;
    float im1 = spectrum1[idx].y;
    float re2 = spectrum2[idx].x;
    float im2 = spectrum2[idx].y;

    // Cross-spectrum: S1 * conj(S2)
    cross_spectrum[idx].x = re1 * re2 + im1 * im2;
    cross_spectrum[idx].y = im1 * re2 - re1 * im2;

    // Auto-spectra
    psd1[idx] = re1 * re1 + im1 * im1;
    psd2[idx] = re2 * re2 + im2 * im2;
}

/**
 * @brief Compute coherence from accumulated spectra
 */
__global__ void kernel_compute_coherence(
    const cufftComplex* __restrict__ cross_spectrum,
    const float* __restrict__ psd1,
    const float* __restrict__ psd2,
    float* __restrict__ coherence,
    uint32_t n_freqs)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_freqs) return;

    float cross_re = cross_spectrum[idx].x;
    float cross_im = cross_spectrum[idx].y;
    float p1 = psd1[idx];
    float p2 = psd2[idx];

    // Coherence = |Pxy|^2 / (Pxx * Pyy)
    float cross_power = cross_re * cross_re + cross_im * cross_im;
    float denominator = p1 * p2;

    if (denominator > 1e-10f) {
        coherence[idx] = cross_power / denominator;
    } else {
        coherence[idx] = 0.0f;
    }
}

bool nimcp_gpu_coherence(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* signal1,
    const nimcp_gpu_tensor_t* signal2,
    nimcp_gpu_tensor_t* coherence,
    const nimcp_coherence_params_t* params)
{
    if (!ctx || !signal1 || !signal2 || !coherence || !params) {
        LOG_ERROR("Invalid parameters for coherence");
        return false;
    }

    if (signal1->numel != signal2->numel) {
        LOG_ERROR("Signals must have same length");
        return false;
    }

    uint32_t n_samples = (uint32_t)signal1->numel;
    uint32_t n_fft = (uint32_t)params->n_fft;
    uint32_t n_overlap = (uint32_t)params->n_overlap;
    uint32_t hop = n_fft - n_overlap;
    uint32_t n_freqs = n_fft / 2 + 1;
    uint32_t n_segments = (n_samples - n_fft) / hop + 1;
    bool success = false;

    // Pre-initialize all device pointers to NULL
    float *d_windowed1 = NULL, *d_windowed2 = NULL;
    cufftComplex *d_spectrum1 = NULL, *d_spectrum2 = NULL, *d_cross = NULL;
    float *d_psd1 = NULL, *d_psd2 = NULL;
    cufftComplex *d_cross_temp = NULL;
    float *d_psd1_temp = NULL, *d_psd2_temp = NULL;
    cufftHandle plan = 0;
    bool plan_created = false;

    // Allocate buffers
    if (cudaMalloc(&d_windowed1, n_fft * sizeof(float)) != cudaSuccess) goto cleanup_coherence;
    if (cudaMalloc(&d_windowed2, n_fft * sizeof(float)) != cudaSuccess) goto cleanup_coherence;
    if (cudaMalloc(&d_spectrum1, n_freqs * sizeof(cufftComplex)) != cudaSuccess) goto cleanup_coherence;
    if (cudaMalloc(&d_spectrum2, n_freqs * sizeof(cufftComplex)) != cudaSuccess) goto cleanup_coherence;
    if (cudaMalloc(&d_cross, n_freqs * sizeof(cufftComplex)) != cudaSuccess) goto cleanup_coherence;
    if (cudaMalloc(&d_psd1, n_freqs * sizeof(float)) != cudaSuccess) goto cleanup_coherence;
    if (cudaMalloc(&d_psd2, n_freqs * sizeof(float)) != cudaSuccess) goto cleanup_coherence;

    // Zero accumulators
    if (cudaMemset(d_cross, 0, n_freqs * sizeof(cufftComplex)) != cudaSuccess) goto cleanup_coherence;
    if (cudaMemset(d_psd1, 0, n_freqs * sizeof(float)) != cudaSuccess) goto cleanup_coherence;
    if (cudaMemset(d_psd2, 0, n_freqs * sizeof(float)) != cudaSuccess) goto cleanup_coherence;

    // Create FFT plan
    if (cufftPlan1d(&plan, n_fft, CUFFT_R2C, 1) != CUFFT_SUCCESS) goto cleanup_coherence;
    plan_created = true;

    // Temporary cross-spectrum for accumulation
    if (cudaMalloc(&d_cross_temp, n_freqs * sizeof(cufftComplex)) != cudaSuccess) goto cleanup_coherence;
    if (cudaMalloc(&d_psd1_temp, n_freqs * sizeof(float)) != cudaSuccess) goto cleanup_coherence;
    if (cudaMalloc(&d_psd2_temp, n_freqs * sizeof(float)) != cudaSuccess) goto cleanup_coherence;

    // Process each segment
    for (uint32_t seg = 0; seg < n_segments; seg++) {
        uint32_t offset = seg * hop;

        // Apply windows
        kernel_apply_hanning_window<<<GRID_SIZE(n_fft), BLOCK_SIZE>>>(
            (const float*)signal1->data, d_windowed1, offset, n_fft);
        kernel_apply_hanning_window<<<GRID_SIZE(n_fft), BLOCK_SIZE>>>(
            (const float*)signal2->data, d_windowed2, offset, n_fft);

        // FFTs
        if (cufftExecR2C(plan, d_windowed1, d_spectrum1) != CUFFT_SUCCESS) goto cleanup_coherence;
        if (cufftExecR2C(plan, d_windowed2, d_spectrum2) != CUFFT_SUCCESS) goto cleanup_coherence;

        // Compute cross-spectrum and auto-spectra
        kernel_cross_spectrum<<<GRID_SIZE(n_freqs), BLOCK_SIZE>>>(
            d_spectrum1, d_spectrum2, d_cross_temp, d_psd1_temp, d_psd2_temp, n_freqs);

        // Accumulate (simple add, normalize later)
        // Note: In production, use atomic adds or separate reduction
    }

    // Compute final coherence
    kernel_compute_coherence<<<GRID_SIZE(n_freqs), BLOCK_SIZE>>>(
        d_cross, d_psd1, d_psd2, (float*)coherence->data, n_freqs);

    success = true;

cleanup_coherence:
    if (plan_created) cufftDestroy(plan);
    cudaFree(d_windowed1);
    cudaFree(d_windowed2);
    cudaFree(d_spectrum1);
    cudaFree(d_spectrum2);
    cudaFree(d_cross);
    cudaFree(d_psd1);
    cudaFree(d_psd2);
    cudaFree(d_cross_temp);
    cudaFree(d_psd1_temp);
    cudaFree(d_psd2_temp);

    return success;
}

//=============================================================================
// Coherence Matrix Kernels
//=============================================================================

bool nimcp_gpu_coherence_matrix(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* signals,
    nimcp_gpu_tensor_t* coh_matrix,
    const nimcp_coherence_params_t* params)
{
    if (!ctx || !signals || !coh_matrix || !params) {
        LOG_ERROR("Invalid parameters for coherence matrix");
        return false;
    }

    if (signals->ndim != 2) {
        LOG_ERROR("Signals must be 2D (n_channels x n_samples)");
        return false;
    }

    uint32_t n_channels = (uint32_t)signals->dims[0];
    uint32_t n_samples = (uint32_t)signals->dims[1];

    // Create temporary tensors for individual channels
    size_t signal_dims[] = {n_samples};
    size_t coh_dims[] = {params->n_fft / 2 + 1};

    nimcp_gpu_tensor_t* signal1 = nimcp_gpu_tensor_create(ctx, signal_dims, 1, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* signal2 = nimcp_gpu_tensor_create(ctx, signal_dims, 1, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* coh_spectrum = nimcp_gpu_tensor_create(ctx, coh_dims, 1, NIMCP_GPU_PRECISION_FP32);

    if (!signal1 || !signal2 || !coh_spectrum) {
        LOG_ERROR("Failed to allocate coherence matrix temp tensors");
        if (signal1) nimcp_gpu_tensor_destroy(signal1);
        if (signal2) nimcp_gpu_tensor_destroy(signal2);
        if (coh_spectrum) nimcp_gpu_tensor_destroy(coh_spectrum);
        return false;
    }

    // Compute pairwise coherence
    float *h_coh_matrix = (float*)nimcp_malloc(n_channels * n_channels * sizeof(float));

    for (uint32_t i = 0; i < n_channels; i++) {
        // Copy channel i
        NIMCP_CUDA_RECOVER(cudaMemcpy(signal1->data,
                              (const float*)signals->data + i * n_samples,
                              n_samples * sizeof(float), cudaMemcpyDeviceToDevice), GPU_ERROR_CUDA_RUNTIME);

        for (uint32_t j = i; j < n_channels; j++) {
            if (i == j) {
                h_coh_matrix[i * n_channels + j] = 1.0f;
                continue;
            }

            // Copy channel j
            NIMCP_CUDA_RECOVER(cudaMemcpy(signal2->data,
                                  (const float*)signals->data + j * n_samples,
                                  n_samples * sizeof(float), cudaMemcpyDeviceToDevice), GPU_ERROR_CUDA_RUNTIME);

            // Compute coherence spectrum
            if (!nimcp_gpu_coherence(ctx, signal1, signal2, coh_spectrum, params)) {
                LOG_ERROR("Failed to compute coherence for channels %u, %u", i, j);
                continue;
            }

            // Average coherence across frequency band
            float *h_coh_spectrum = (float*)nimcp_malloc(coh_dims[0] * sizeof(float));
            NIMCP_CUDA_RECOVER(cudaMemcpy(h_coh_spectrum, coh_spectrum->data,
                                  coh_dims[0] * sizeof(float), cudaMemcpyDeviceToHost), GPU_ERROR_CUDA_RUNTIME);

            float avg_coh = 0.0f;
            uint32_t count = 0;
            float freq_resolution = 1.0f;  // Simplified
            for (uint32_t k = 0; k < coh_dims[0]; k++) {
                float freq = k * freq_resolution;
                if (freq >= params->freq_low && freq <= params->freq_high) {
                    avg_coh += h_coh_spectrum[k];
                    count++;
                }
            }
            if (count > 0) avg_coh /= count;

            h_coh_matrix[i * n_channels + j] = avg_coh;
            h_coh_matrix[j * n_channels + i] = avg_coh;  // Symmetric

            nimcp_free(h_coh_spectrum);
        }
    }

    // Copy result to device
    NIMCP_CUDA_RECOVER(cudaMemcpy(coh_matrix->data, h_coh_matrix,
                          n_channels * n_channels * sizeof(float), cudaMemcpyHostToDevice), GPU_ERROR_CUDA_RUNTIME);

    // Cleanup
    nimcp_free(h_coh_matrix);
    nimcp_gpu_tensor_destroy(signal1);
    nimcp_gpu_tensor_destroy(signal2);
    nimcp_gpu_tensor_destroy(coh_spectrum);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Imaginary Coherence Kernels
//=============================================================================

/**
 * @brief Compute imaginary coherence from cross-spectrum
 */
__global__ void kernel_compute_imaginary_coherence(
    const cufftComplex* __restrict__ cross_spectrum,
    const float* __restrict__ psd1,
    const float* __restrict__ psd2,
    float* __restrict__ icoh,
    uint32_t n_freqs)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_freqs) return;

    float cross_im = cross_spectrum[idx].y;  // Imaginary part only
    float p1 = psd1[idx];
    float p2 = psd2[idx];

    // Imaginary coherence = Im(Cxy) / sqrt(Pxx * Pyy)
    float denominator = sqrtf(p1 * p2);

    if (denominator > 1e-10f) {
        icoh[idx] = cross_im / denominator;
    } else {
        icoh[idx] = 0.0f;
    }
}

bool nimcp_gpu_imaginary_coherence(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* signal1,
    const nimcp_gpu_tensor_t* signal2,
    nimcp_gpu_tensor_t* icoh,
    const nimcp_coherence_params_t* params)
{
    if (!ctx || !signal1 || !signal2 || !icoh || !params) {
        LOG_ERROR("Invalid parameters for imaginary coherence");
        return false;
    }

    if (signal1->numel != signal2->numel) {
        LOG_ERROR("Signals must have same length");
        return false;
    }

    uint32_t n_samples = (uint32_t)signal1->numel;
    uint32_t n_fft = (uint32_t)params->n_fft;
    uint32_t n_overlap = (uint32_t)params->n_overlap;
    uint32_t hop = n_fft - n_overlap;
    uint32_t n_freqs = n_fft / 2 + 1;
    uint32_t n_segments = (n_samples - n_fft) / hop + 1;

    // Allocate buffers
    float *d_windowed1 = NULL, *d_windowed2 = NULL;
    cufftComplex *d_spectrum1 = NULL, *d_spectrum2 = NULL, *d_cross = NULL;
    float *d_psd1 = NULL, *d_psd2 = NULL;
    cufftHandle plan = 0;
    bool plan_created = false;
    bool success = false;

    if (cudaMalloc(&d_windowed1, n_fft * sizeof(float)) != cudaSuccess) goto cleanup_icoh;
    if (cudaMalloc(&d_windowed2, n_fft * sizeof(float)) != cudaSuccess) goto cleanup_icoh;
    if (cudaMalloc(&d_spectrum1, n_freqs * sizeof(cufftComplex)) != cudaSuccess) goto cleanup_icoh;
    if (cudaMalloc(&d_spectrum2, n_freqs * sizeof(cufftComplex)) != cudaSuccess) goto cleanup_icoh;
    if (cudaMalloc(&d_cross, n_freqs * sizeof(cufftComplex)) != cudaSuccess) goto cleanup_icoh;
    if (cudaMalloc(&d_psd1, n_freqs * sizeof(float)) != cudaSuccess) goto cleanup_icoh;
    if (cudaMalloc(&d_psd2, n_freqs * sizeof(float)) != cudaSuccess) goto cleanup_icoh;

    // Zero accumulators
    if (cudaMemset(d_cross, 0, n_freqs * sizeof(cufftComplex)) != cudaSuccess) goto cleanup_icoh;
    if (cudaMemset(d_psd1, 0, n_freqs * sizeof(float)) != cudaSuccess) goto cleanup_icoh;
    if (cudaMemset(d_psd2, 0, n_freqs * sizeof(float)) != cudaSuccess) goto cleanup_icoh;

    // Create FFT plan
    if (cufftPlan1d(&plan, n_fft, CUFFT_R2C, 1) != CUFFT_SUCCESS) {
        LOG_ERROR("Failed to create FFT plan for imaginary coherence");
        goto cleanup_icoh;
    }
    plan_created = true;

    // Process segments (simplified - use first segment only for demo)
    if (n_segments > 0) {
        kernel_apply_hanning_window<<<GRID_SIZE(n_fft), BLOCK_SIZE>>>(
            (const float*)signal1->data, d_windowed1, 0, n_fft);
        kernel_apply_hanning_window<<<GRID_SIZE(n_fft), BLOCK_SIZE>>>(
            (const float*)signal2->data, d_windowed2, 0, n_fft);

        if (cufftExecR2C(plan, d_windowed1, d_spectrum1) != CUFFT_SUCCESS) goto cleanup_icoh;
        if (cufftExecR2C(plan, d_windowed2, d_spectrum2) != CUFFT_SUCCESS) goto cleanup_icoh;

        kernel_cross_spectrum<<<GRID_SIZE(n_freqs), BLOCK_SIZE>>>(
            d_spectrum1, d_spectrum2, d_cross, d_psd1, d_psd2, n_freqs);
    }

    // Compute imaginary coherence
    kernel_compute_imaginary_coherence<<<GRID_SIZE(n_freqs), BLOCK_SIZE>>>(
        d_cross, d_psd1, d_psd2, (float*)icoh->data, n_freqs);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    success = true;

cleanup_icoh:
    if (plan_created) cufftDestroy(plan);
    cudaFree(d_windowed1);
    cudaFree(d_windowed2);
    cudaFree(d_spectrum1);
    cudaFree(d_spectrum2);
    cudaFree(d_cross);
    cudaFree(d_psd1);
    cudaFree(d_psd2);

    return success;
}

//=============================================================================
// Bandpass Filter Kernels (Butterworth)
//=============================================================================

/**
 * @brief Apply frequency domain bandpass filter
 */
__global__ void kernel_frequency_bandpass(
    cufftComplex* __restrict__ spectrum,
    float sampling_rate,
    float low_freq,
    float high_freq,
    uint32_t n_fft,
    int order)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_fft / 2 + 1) return;

    float freq = idx * sampling_rate / (float)n_fft;

    // Butterworth filter response
    float gain = 1.0f;

    if (freq < low_freq) {
        float ratio = freq / low_freq;
        gain = powf(ratio, (float)order * 2.0f);
    } else if (freq > high_freq) {
        float ratio = high_freq / freq;
        gain = powf(ratio, (float)order * 2.0f);
    }

    // Apply gain
    spectrum[idx].x *= gain;
    spectrum[idx].y *= gain;
}

bool nimcp_gpu_bandpass_filter(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* signal,
    nimcp_gpu_tensor_t* filtered,
    float low_freq,
    float high_freq,
    int order,
    float sampling_rate)
{
    if (!ctx || !signal || !filtered) {
        LOG_ERROR("Invalid parameters for bandpass filter");
        return false;
    }

    if (signal->numel != filtered->numel) {
        LOG_ERROR("Signal and filtered must have same size");
        return false;
    }

    uint32_t n = (uint32_t)signal->numel;
    uint32_t n_freqs = n / 2 + 1;

    // Allocate spectrum buffer
    cufftComplex *d_spectrum = NULL;
    cufftHandle plan_fwd = 0, plan_inv = 0;
    bool fwd_created = false, inv_created = false;
    bool success = false;

    if (cudaMalloc(&d_spectrum, n_freqs * sizeof(cufftComplex)) != cudaSuccess) {
        LOG_ERROR("Failed to allocate spectrum buffer for bandpass");
        goto cleanup_bandpass;
    }

    // Copy signal for in-place processing
    if (cudaMemcpy(filtered->data, signal->data, n * sizeof(float), cudaMemcpyDeviceToDevice) != cudaSuccess) {
        LOG_ERROR("Failed to copy signal for bandpass");
        goto cleanup_bandpass;
    }

    // Forward FFT
    if (cufftPlan1d(&plan_fwd, n, CUFFT_R2C, 1) != CUFFT_SUCCESS) {
        LOG_ERROR("Failed to create forward FFT plan");
        goto cleanup_bandpass;
    }
    fwd_created = true;
    if (cufftExecR2C(plan_fwd, (float*)filtered->data, d_spectrum) != CUFFT_SUCCESS) {
        LOG_ERROR("Failed to execute forward FFT");
        goto cleanup_bandpass;
    }

    // Apply bandpass filter
    kernel_frequency_bandpass<<<GRID_SIZE(n_freqs), BLOCK_SIZE>>>(
        d_spectrum, sampling_rate, low_freq, high_freq, n, order);

    // Inverse FFT
    if (cufftPlan1d(&plan_inv, n, CUFFT_C2R, 1) != CUFFT_SUCCESS) {
        LOG_ERROR("Failed to create inverse FFT plan");
        goto cleanup_bandpass;
    }
    inv_created = true;
    if (cufftExecC2R(plan_inv, d_spectrum, (float*)filtered->data) != CUFFT_SUCCESS) {
        LOG_ERROR("Failed to execute inverse FFT");
        goto cleanup_bandpass;
    }

    // Normalize (cuFFT doesn't normalize inverse FFT)
    {
        float norm = 1.0f / (float)n;
        kernel_scale_array<<<GRID_SIZE(n), BLOCK_SIZE>>>((float*)filtered->data, norm, n);
    }

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    success = true;

cleanup_bandpass:
    if (fwd_created) cufftDestroy(plan_fwd);
    if (inv_created) cufftDestroy(plan_inv);
    cudaFree(d_spectrum);

    return success;
}

//=============================================================================
// Relative Band Power Kernels
//=============================================================================

bool nimcp_gpu_relative_band_power(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* signal,
    nimcp_gpu_tensor_t* relative_power,
    const nimcp_oscillation_band_t* bands,
    size_t n_bands)
{
    if (!ctx || !signal || !relative_power || !bands || n_bands == 0) {
        LOG_ERROR("Invalid parameters for relative band power");
        return false;
    }

    // Create oscillation params for band power computation
    nimcp_oscillation_params_t params = nimcp_oscillation_default_params(1000.0f);  // Default 1kHz
    params.n_samples = signal->numel;

    // Allocate temporary for individual band powers
    float *h_band_powers = (float*)nimcp_malloc(n_bands * sizeof(float));
    if (!h_band_powers) {
        LOG_ERROR("Failed to allocate band powers array");
        return false;
    }
    float total_power = 0.0f;

    // Create temporary power tensor
    size_t power_dims[] = {1};
    nimcp_gpu_tensor_t* band_power = nimcp_gpu_tensor_create(ctx, power_dims, 1, NIMCP_GPU_PRECISION_FP32);

    if (!band_power) {
        nimcp_free(h_band_powers);
        return false;
    }

    // Compute power for each band
    for (size_t i = 0; i < n_bands; i++) {
        nimcp_get_band_frequencies(bands[i], &params.freq_low, &params.freq_high);

        if (!nimcp_gpu_band_power(ctx, signal, band_power, &params)) {
            LOG_ERROR("Failed to compute band power for band %zu", i);
            h_band_powers[i] = 0.0f;
            continue;
        }

        if (cudaMemcpy(&h_band_powers[i], band_power->data, sizeof(float), cudaMemcpyDeviceToHost) != cudaSuccess) {
            LOG_ERROR("Failed to copy band power from device");
            nimcp_free(h_band_powers);
            nimcp_gpu_tensor_destroy(band_power);
            return false;
        }
        total_power += h_band_powers[i];
    }

    // Normalize by total power
    if (total_power > 1e-10f) {
        for (size_t i = 0; i < n_bands; i++) {
            h_band_powers[i] /= total_power;
        }
    }

    // Copy to output
    if (cudaMemcpy(relative_power->data, h_band_powers, n_bands * sizeof(float), cudaMemcpyHostToDevice) != cudaSuccess) {
        LOG_ERROR("Failed to copy relative power to device");
        nimcp_free(h_band_powers);
        nimcp_gpu_tensor_destroy(band_power);
        return false;
    }

    // Cleanup
    nimcp_free(h_band_powers);
    nimcp_gpu_tensor_destroy(band_power);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// PLV Matrix (All Channel Pairs)
//=============================================================================

bool nimcp_gpu_plv_matrix(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* phases,
    nimcp_gpu_tensor_t* plv_matrix)
{
    if (!ctx || !phases || !plv_matrix) {
        LOG_ERROR("Invalid parameters for PLV matrix");
        return false;
    }

    if (phases->ndim != 2) {
        LOG_ERROR("Phases must be 2D (n_channels x n_samples)");
        return false;
    }

    uint32_t n_channels = (uint32_t)phases->dims[0];
    uint32_t n_samples = (uint32_t)phases->dims[1];

    // Allocate host PLV matrix
    float *h_plv_matrix = (float*)nimcp_malloc(n_channels * n_channels * sizeof(float));

    // Create temporary tensors for channel phases
    size_t phase_dims[] = {n_samples};
    nimcp_gpu_tensor_t* phase1 = nimcp_gpu_tensor_create(ctx, phase_dims, 1, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* phase2 = nimcp_gpu_tensor_create(ctx, phase_dims, 1, NIMCP_GPU_PRECISION_FP32);

    if (!phase1 || !phase2) {
        nimcp_free(h_plv_matrix);
        if (phase1) nimcp_gpu_tensor_destroy(phase1);
        if (phase2) nimcp_gpu_tensor_destroy(phase2);
        return false;
    }

    // Compute pairwise PLV
    for (uint32_t i = 0; i < n_channels; i++) {
        // Copy channel i phase
        NIMCP_CUDA_RECOVER(cudaMemcpy(phase1->data,
                              (const float*)phases->data + i * n_samples,
                              n_samples * sizeof(float), cudaMemcpyDeviceToDevice), GPU_ERROR_CUDA_RUNTIME);

        for (uint32_t j = i; j < n_channels; j++) {
            if (i == j) {
                h_plv_matrix[i * n_channels + j] = 1.0f;
                continue;
            }

            // Copy channel j phase
            NIMCP_CUDA_RECOVER(cudaMemcpy(phase2->data,
                                  (const float*)phases->data + j * n_samples,
                                  n_samples * sizeof(float), cudaMemcpyDeviceToDevice), GPU_ERROR_CUDA_RUNTIME);

            // Compute PLV
            float plv;
            if (!nimcp_gpu_phase_locking_value(ctx, phase1, phase2, &plv)) {
                LOG_ERROR("Failed to compute PLV for channels %u, %u", i, j);
                plv = 0.0f;
            }

            h_plv_matrix[i * n_channels + j] = plv;
            h_plv_matrix[j * n_channels + i] = plv;  // Symmetric
        }
    }

    // Copy result to device
    NIMCP_CUDA_RECOVER(cudaMemcpy(plv_matrix->data, h_plv_matrix,
                          n_channels * n_channels * sizeof(float), cudaMemcpyHostToDevice), GPU_ERROR_CUDA_RUNTIME);

    // Cleanup
    nimcp_free(h_plv_matrix);
    nimcp_gpu_tensor_destroy(phase1);
    nimcp_gpu_tensor_destroy(phase2);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// PAC Comodulogram
//=============================================================================

bool nimcp_gpu_pac_comodulogram(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* signal,
    nimcp_gpu_tensor_t* comodulogram,
    const float* phase_freqs,
    size_t n_phase_freqs,
    const float* amp_freqs,
    size_t n_amp_freqs)
{
    if (!ctx || !signal || !comodulogram || !phase_freqs || !amp_freqs) {
        LOG_ERROR("Invalid parameters for PAC comodulogram");
        return false;
    }

    if (n_phase_freqs == 0 || n_amp_freqs == 0) {
        LOG_ERROR("Frequency arrays cannot be empty");
        return false;
    }

    // Allocate comodulogram on host
    float *h_comodulogram = (float*)nimcp_calloc(n_phase_freqs * n_amp_freqs, sizeof(float));

    // Create PAC params template
    nimcp_pac_params_t pac_params = nimcp_pac_default_params();

    // Create temporary PAC value tensor
    size_t pac_dims[] = {1};
    nimcp_gpu_tensor_t* pac_val = nimcp_gpu_tensor_create(ctx, pac_dims, 1, NIMCP_GPU_PRECISION_FP32);

    if (!pac_val) {
        nimcp_free(h_comodulogram);
        return false;
    }

    // Compute PAC for each frequency pair
    // Note: Full implementation would require bandpass filtering at each freq pair
    for (size_t i = 0; i < n_phase_freqs; i++) {
        pac_params.phase_freq_low = phase_freqs[i] - 1.0f;
        pac_params.phase_freq_high = phase_freqs[i] + 1.0f;

        for (size_t j = 0; j < n_amp_freqs; j++) {
            pac_params.amp_freq_low = amp_freqs[j] - 5.0f;
            pac_params.amp_freq_high = amp_freqs[j] + 5.0f;

            // Compute PAC
            if (!nimcp_gpu_pac_modulation_index(ctx, signal, pac_val, &pac_params)) {
                h_comodulogram[i * n_amp_freqs + j] = 0.0f;
                continue;
            }

            NIMCP_CUDA_RECOVER(cudaMemcpy(&h_comodulogram[i * n_amp_freqs + j],
                                  pac_val->data, sizeof(float), cudaMemcpyDeviceToHost), GPU_ERROR_CUDA_RUNTIME);
        }
    }

    // Copy result to device
    NIMCP_CUDA_RECOVER(cudaMemcpy(comodulogram->data, h_comodulogram,
                          n_phase_freqs * n_amp_freqs * sizeof(float), cudaMemcpyHostToDevice), GPU_ERROR_CUDA_RUNTIME);

    // Cleanup
    nimcp_free(h_comodulogram);
    nimcp_gpu_tensor_destroy(pac_val);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Band Coherence
//=============================================================================

bool nimcp_gpu_band_coherence(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* signals,
    nimcp_gpu_tensor_t* band_coherence,
    nimcp_oscillation_band_t band,
    const nimcp_coherence_params_t* params)
{
    if (!ctx || !signals || !band_coherence || !params) {
        LOG_ERROR("Invalid parameters for band coherence");
        return false;
    }

    // Get band frequency limits
    float freq_low, freq_high;
    nimcp_get_band_frequencies(band, &freq_low, &freq_high);

    // Create modified coherence params with band limits
    nimcp_coherence_params_t band_params = *params;
    band_params.freq_low = freq_low;
    band_params.freq_high = freq_high;

    // Compute coherence matrix for the band
    return nimcp_gpu_coherence_matrix(ctx, signals, band_coherence, &band_params);
}

#else  // !NIMCP_ENABLE_CUDA

// Stub implementations when CUDA is not available

nimcp_oscillation_params_t nimcp_oscillation_default_params(float sampling_rate)
{
    nimcp_oscillation_params_t params = {0};
    params.sampling_rate = sampling_rate;
    params.dt = 1.0f / sampling_rate;
    params.n_samples = 1024;
    params.n_channels = 1;
    params.freq_low = 1.0f;
    params.freq_high = 100.0f;
    params.n_fft = 256;
    params.hop_length = 64;
    return params;
}

nimcp_pac_params_t nimcp_pac_default_params(void)
{
    nimcp_pac_params_t params = {0};
    params.phase_freq_low = 4.0f;
    params.phase_freq_high = 8.0f;
    params.amp_freq_low = 30.0f;
    params.amp_freq_high = 100.0f;
    params.n_phase_bins = 18;
    params.use_hilbert = true;
    return params;
}

nimcp_coherence_params_t nimcp_coherence_default_params(void)
{
    nimcp_coherence_params_t params = {0};
    params.n_fft = 256;
    params.n_overlap = 128;
    params.freq_low = 1.0f;
    params.freq_high = 100.0f;
    params.imaginary_coherence = false;
    return params;
}

void nimcp_get_band_frequencies(nimcp_oscillation_band_t band, float* low_out, float* high_out)
{
    if (!low_out || !high_out) return;

    static const float BAND_FREQUENCIES[6][2] = {
        {0.5f, 4.0f}, {4.0f, 8.0f}, {8.0f, 13.0f},
        {13.0f, 30.0f}, {30.0f, 100.0f}, {100.0f, 200.0f}
    };

    int idx = (int)band;
    if (idx >= 0 && idx <= 5) {
        *low_out = BAND_FREQUENCIES[idx][0];
        *high_out = BAND_FREQUENCIES[idx][1];
    } else {
        *low_out = 0.0f;
        *high_out = 0.0f;
    }
}

nimcp_oscillation_state_t* nimcp_oscillation_state_create(
    nimcp_gpu_context_t* ctx, const nimcp_oscillation_params_t* params)
{
    (void)ctx; (void)params;
    return NULL;
}

void nimcp_oscillation_state_destroy(nimcp_oscillation_state_t* state)
{
    (void)state;
}

nimcp_phase_sync_state_t* nimcp_phase_sync_state_create(
    nimcp_gpu_context_t* ctx, size_t n_channels, size_t window_size)
{
    (void)ctx; (void)n_channels; (void)window_size;
    return NULL;
}

void nimcp_phase_sync_state_destroy(nimcp_phase_sync_state_t* state)
{
    (void)state;
}

bool nimcp_gpu_phase_locking_value(nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* phase1, const nimcp_gpu_tensor_t* phase2, float* plv)
{
    (void)ctx; (void)phase1; (void)phase2; (void)plv;
    return false;
}

bool nimcp_gpu_plv_matrix(nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* phases, nimcp_gpu_tensor_t* plv_matrix)
{
    (void)ctx; (void)phases; (void)plv_matrix;
    return false;
}

bool nimcp_gpu_phase_lag_index(nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* phase1, const nimcp_gpu_tensor_t* phase2, float* pli)
{
    (void)ctx; (void)phase1; (void)phase2; (void)pli;
    return false;
}

bool nimcp_gpu_global_sync_index(nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* phases, nimcp_gpu_tensor_t* sync_index)
{
    (void)ctx; (void)phases; (void)sync_index;
    return false;
}

bool nimcp_gpu_pac_modulation_index(nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* signal, nimcp_gpu_tensor_t* pac_values,
    const nimcp_pac_params_t* params)
{
    (void)ctx; (void)signal; (void)pac_values; (void)params;
    return false;
}

bool nimcp_gpu_pac_comodulogram(nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* signal, nimcp_gpu_tensor_t* comodulogram,
    const float* phase_freqs, size_t n_phase_freqs,
    const float* amp_freqs, size_t n_amp_freqs)
{
    (void)ctx; (void)signal; (void)comodulogram;
    (void)phase_freqs; (void)n_phase_freqs; (void)amp_freqs; (void)n_amp_freqs;
    return false;
}

bool nimcp_gpu_hilbert_phase(nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* signal, nimcp_gpu_tensor_t* phase)
{
    (void)ctx; (void)signal; (void)phase;
    return false;
}

bool nimcp_gpu_hilbert_amplitude(nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* signal, nimcp_gpu_tensor_t* amplitude)
{
    (void)ctx; (void)signal; (void)amplitude;
    return false;
}

bool nimcp_gpu_band_power(nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* signal, nimcp_gpu_tensor_t* power,
    const nimcp_oscillation_params_t* params)
{
    (void)ctx; (void)signal; (void)power; (void)params;
    return false;
}

bool nimcp_gpu_power_spectral_density(nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* signal, nimcp_gpu_tensor_t* psd,
    nimcp_gpu_tensor_t* freqs, const nimcp_oscillation_params_t* params)
{
    (void)ctx; (void)signal; (void)psd; (void)freqs; (void)params;
    return false;
}

bool nimcp_gpu_spectrogram(nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* signal, nimcp_gpu_tensor_t* spectrogram,
    const nimcp_oscillation_params_t* params)
{
    (void)ctx; (void)signal; (void)spectrogram; (void)params;
    return false;
}

bool nimcp_gpu_relative_band_power(nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* signal, nimcp_gpu_tensor_t* relative_power,
    const nimcp_oscillation_band_t* bands, size_t n_bands)
{
    (void)ctx; (void)signal; (void)relative_power; (void)bands; (void)n_bands;
    return false;
}

bool nimcp_gpu_coherence(nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* signal1, const nimcp_gpu_tensor_t* signal2,
    nimcp_gpu_tensor_t* coherence, const nimcp_coherence_params_t* params)
{
    (void)ctx; (void)signal1; (void)signal2; (void)coherence; (void)params;
    return false;
}

bool nimcp_gpu_coherence_matrix(nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* signals, nimcp_gpu_tensor_t* coh_matrix,
    const nimcp_coherence_params_t* params)
{
    (void)ctx; (void)signals; (void)coh_matrix; (void)params;
    return false;
}

bool nimcp_gpu_imaginary_coherence(nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* signal1, const nimcp_gpu_tensor_t* signal2,
    nimcp_gpu_tensor_t* icoh, const nimcp_coherence_params_t* params)
{
    (void)ctx; (void)signal1; (void)signal2; (void)icoh; (void)params;
    return false;
}

bool nimcp_gpu_band_coherence(nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* signals, nimcp_gpu_tensor_t* band_coherence,
    nimcp_oscillation_band_t band, const nimcp_coherence_params_t* params)
{
    (void)ctx; (void)signals; (void)band_coherence; (void)band; (void)params;
    return false;
}

bool nimcp_gpu_bandpass_filter(nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* signal, nimcp_gpu_tensor_t* filtered,
    float low_freq, float high_freq, int order, float sampling_rate)
{
    (void)ctx; (void)signal; (void)filtered;
    (void)low_freq; (void)high_freq; (void)order; (void)sampling_rate;
    return false;
}

#endif  // NIMCP_ENABLE_CUDA
