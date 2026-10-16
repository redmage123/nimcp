/**
 * @file nimcp_timeseries_kernels.cu
 * @brief CUDA Kernels for GPU-Accelerated Time Series Analysis
 *
 * WHAT: GPU implementations of spectral analysis, autocorrelation, and filtering
 * WHY:  50-100x speedup for FFT-based operations on large time series
 * HOW:  cuFFT for transforms, custom kernels for windowing/reduction
 *
 * KERNEL ARCHITECTURE:
 *   - Spectral analysis: cuFFT + custom magnitude/power kernels
 *   - Autocorrelation: FFT-based O(n log n) via Wiener-Khinchin theorem
 *   - Welch PSD: Batched FFT with parallel segment processing
 *   - Smoothing: Parallel convolution with shared memory
 *
 * PERFORMANCE TARGETS:
 *   - FFT (n=1M): <2ms
 *   - Welch PSD (n=100K, 8 segments): <10ms
 *   - ACF (n=10K, max_lag=1K): <1ms
 *   - Batch coherence (10 pairs, n=10K): <20ms
 *
 * THREAD SAFETY:
 *   - All kernels are thread-safe
 *   - Uses stream-based execution for overlap
 *   - No global mutable state
 *
 * @author NIMCP Development Team
 * @date 2026-01-30
 * @version 1.0.0
 */

#ifdef NIMCP_ENABLE_CUDA

// Include CUDA headers FIRST (before any extern "C" blocks)
#include "utils/memory/nimcp_memory.h"
#include <cuda_runtime.h>
#include <cufft.h>
#include <math.h>
#include <float.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// Project headers
#include "utils/statistics/nimcp_timeseries.h"
#include "gpu/common/nimcp_cuda_utils.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "TIMESERIES_GPU"

//=============================================================================
// Kernel Configuration
//=============================================================================

#define BLOCK_SIZE 256
#define WARP_SIZE 32
#define GRID_SIZE(n) (((n) + BLOCK_SIZE - 1) / BLOCK_SIZE)

// Maximum segment size for shared memory operations
#define MAX_SHARED_SIZE 2048

// Maximum number of segments for Welch
#define MAX_WELCH_SEGMENTS 256

//=============================================================================
// cuFFT Error Checking with Recovery
//=============================================================================

#define CUFFT_CHECK(call) do { \
    cufftResult _result = (call); \
    if (_result != CUFFT_SUCCESS) { \
        nimcp_gpu_recovery_result_t _rec_result = {0}; \
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_LIBRARY, cudaErrorUnknown, &_rec_result)) { \
            _result = (call); \
        } \
        if (_result != CUFFT_SUCCESS) { \
            LOG_ERROR("cuFFT error at %s:%d: %d", __FILE__, __LINE__, _result); \
            NIMCP_THROW_GPU(NIMCP_ERROR_GPU, 0, 0, "cuFFT call failed"); \
            return false; \
        } \
    } \
} while(0)

#define CUFFT_CHECK_GOTO(call, label) do { \
    cufftResult _result = (call); \
    if (_result != CUFFT_SUCCESS) { \
        nimcp_gpu_recovery_result_t _rec_result = {0}; \
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_LIBRARY, cudaErrorUnknown, &_rec_result)) { \
            _result = (call); \
        } \
        if (_result != CUFFT_SUCCESS) { \
            LOG_ERROR("cuFFT error at %s:%d: %d", __FILE__, __LINE__, _result); \
            NIMCP_THROW_GPU(NIMCP_ERROR_GPU, 0, 0, "cuFFT call failed"); \
            goto label; \
        } \
    } \
} while(0)

//=============================================================================
// Thread-Local Error Storage
//=============================================================================

static __thread char g_ts_gpu_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_ts_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_ts_gpu_error, sizeof(g_ts_gpu_error), fmt, args);
    va_end(args);
}

//=============================================================================
// GPU Context Structure for Time Series
//=============================================================================

/**
 * @brief GPU workspace for time series operations
 */
typedef struct ts_gpu_workspace {
    cudaStream_t stream;           /**< CUDA stream */
    cufftHandle fft_plan;          /**< FFT plan (reusable) */
    size_t fft_plan_size;          /**< Size of current FFT plan */

    float* d_input;                /**< Device input buffer */
    cufftComplex* d_complex;       /**< Complex FFT buffer */
    float* d_output;               /**< Device output buffer */
    float* d_window;               /**< Window function buffer */
    float* d_temp;                 /**< Temporary buffer */

    size_t input_size;             /**< Allocated input size */
    size_t complex_size;           /**< Allocated complex size */
    size_t output_size;            /**< Allocated output size */

    bool initialized;              /**< Initialization flag */
} ts_gpu_workspace_t;

// Global workspace (thread-local for safety)
static __thread ts_gpu_workspace_t* g_ts_workspace = NULL;

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
 * @brief Block-level reduction for sum using shared memory
 */
__device__ float block_reduce_sum(float val, float* shared) {
    int lane = threadIdx.x % WARP_SIZE;
    int wid = threadIdx.x / WARP_SIZE;

    val = warp_reduce_sum(val);

    if (lane == 0) shared[wid] = val;
    __syncthreads();

    val = (threadIdx.x < blockDim.x / WARP_SIZE) ? shared[lane] : 0.0f;

    if (wid == 0) val = warp_reduce_sum(val);

    return val;
}

/**
 * @brief Warp-level reduction for maximum
 */
__device__ __forceinline__ float warp_reduce_max(float val) {
    for (int offset = WARP_SIZE / 2; offset > 0; offset /= 2) {
        val = fmaxf(val, __shfl_down_sync(0xffffffff, val, offset));
    }
    return val;
}

//=============================================================================
// Window Function Kernels
//=============================================================================

/**
 * @brief Kernel to generate Hann window
 */
__global__ void kernel_hann_window(float* window, uint32_t n) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float phase = 2.0f * M_PI * idx / (n - 1);
    window[idx] = 0.5f * (1.0f - cosf(phase));
}

/**
 * @brief Kernel to generate Hamming window
 */
__global__ void kernel_hamming_window(float* window, uint32_t n) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float phase = 2.0f * M_PI * idx / (n - 1);
    window[idx] = 0.54f - 0.46f * cosf(phase);
}

/**
 * @brief Kernel to generate Blackman window
 */
__global__ void kernel_blackman_window(float* window, uint32_t n) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float a0 = 0.42f;
    float a1 = 0.5f;
    float a2 = 0.08f;
    float phase = 2.0f * M_PI * idx / (n - 1);
    window[idx] = a0 - a1 * cosf(phase) + a2 * cosf(2.0f * phase);
}

/**
 * @brief Kernel to generate Blackman-Harris window
 */
__global__ void kernel_blackman_harris_window(float* window, uint32_t n) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float a0 = 0.35875f;
    float a1 = 0.48829f;
    float a2 = 0.14128f;
    float a3 = 0.01168f;
    float phase = 2.0f * M_PI * idx / (n - 1);
    window[idx] = a0 - a1 * cosf(phase) + a2 * cosf(2.0f * phase) - a3 * cosf(3.0f * phase);
}

/**
 * @brief Kernel to generate Kaiser window
 */
__global__ void kernel_kaiser_window(float* window, uint32_t n, float beta) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    // Kaiser window: w[n] = I_0(beta * sqrt(1 - ((n - N/2)/(N/2))^2)) / I_0(beta)
    // Using polynomial approximation for I_0
    float x = 2.0f * idx / (n - 1) - 1.0f;
    float arg = beta * sqrtf(1.0f - x * x);

    // I_0 approximation (good for |x| < 15)
    float sum = 1.0f;
    float term = 1.0f;
    for (int k = 1; k <= 12; k++) {
        term *= (arg / (2.0f * k)) * (arg / (2.0f * k));
        sum += term;
    }

    // Normalize by I_0(beta)
    float i0_beta = 1.0f;
    term = 1.0f;
    for (int k = 1; k <= 12; k++) {
        term *= (beta / (2.0f * k)) * (beta / (2.0f * k));
        i0_beta += term;
    }

    window[idx] = sum / i0_beta;
}

/**
 * @brief Kernel to generate Gaussian window
 */
__global__ void kernel_gaussian_window(float* window, uint32_t n, float sigma) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float x = (idx - (n - 1) / 2.0f) / (sigma * (n - 1) / 2.0f);
    window[idx] = expf(-0.5f * x * x);
}

/**
 * @brief Kernel to generate Tukey (tapered cosine) window
 */
__global__ void kernel_tukey_window(float* window, uint32_t n, float alpha) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float x = (float)idx / (n - 1);

    if (x < alpha / 2.0f) {
        window[idx] = 0.5f * (1.0f + cosf(M_PI * (2.0f * x / alpha - 1.0f)));
    } else if (x > 1.0f - alpha / 2.0f) {
        window[idx] = 0.5f * (1.0f + cosf(M_PI * (2.0f * x / alpha - 2.0f / alpha + 1.0f)));
    } else {
        window[idx] = 1.0f;
    }
}

//=============================================================================
// Signal Processing Kernels
//=============================================================================

/**
 * @brief Kernel to apply window function to signal
 */
__global__ void kernel_apply_window(
    const float* __restrict__ input,
    const float* __restrict__ window,
    float* __restrict__ output,
    uint32_t n)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    output[idx] = input[idx] * window[idx];
}

/**
 * @brief Kernel to apply window and convert real to complex
 */
__global__ void kernel_apply_window_r2c(
    const float* __restrict__ input,
    const float* __restrict__ window,
    cufftComplex* __restrict__ output,
    uint32_t n)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    output[idx].x = input[idx] * window[idx];
    output[idx].y = 0.0f;
}

/**
 * @brief Kernel to detrend signal (remove mean)
 */
__global__ void kernel_detrend_mean(
    float* data,
    float mean,
    uint32_t n)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    data[idx] -= mean;
}

/**
 * @brief Kernel to compute mean for detrending
 */
__global__ void kernel_compute_mean_partial(
    const float* __restrict__ data,
    float* __restrict__ partial_sums,
    uint32_t n)
{
    __shared__ float sdata[BLOCK_SIZE / WARP_SIZE];

    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;

    float local_sum = 0.0f;
    for (uint32_t i = idx; i < n; i += blockDim.x * gridDim.x) {
        local_sum += data[i];
    }

    float sum = block_reduce_sum(local_sum, sdata);

    if (threadIdx.x == 0) {
        partial_sums[blockIdx.x] = sum;
    }
}

/**
 * @brief Kernel to detrend signal (remove linear trend)
 */
__global__ void kernel_detrend_linear(
    float* data,
    float slope,
    float intercept,
    uint32_t n)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    data[idx] -= (slope * idx + intercept);
}

//=============================================================================
// FFT and Spectral Analysis Kernels
//=============================================================================

/**
 * @brief Kernel to compute power spectrum from complex FFT output
 */
__global__ void kernel_power_spectrum(
    const cufftComplex* __restrict__ fft_output,
    float* __restrict__ power,
    uint32_t n_freqs,
    float scale)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_freqs) return;

    float re = fft_output[idx].x;
    float im = fft_output[idx].y;
    power[idx] = scale * (re * re + im * im);
}

/**
 * @brief Kernel to compute magnitude spectrum from complex FFT
 */
__global__ void kernel_magnitude_spectrum(
    const cufftComplex* __restrict__ fft_output,
    float* __restrict__ magnitude,
    uint32_t n_freqs)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_freqs) return;

    float re = fft_output[idx].x;
    float im = fft_output[idx].y;
    magnitude[idx] = sqrtf(re * re + im * im);
}

/**
 * @brief Kernel to compute phase spectrum from complex FFT
 */
__global__ void kernel_phase_spectrum(
    const cufftComplex* __restrict__ fft_output,
    float* __restrict__ phase,
    uint32_t n_freqs)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_freqs) return;

    phase[idx] = atan2f(fft_output[idx].y, fft_output[idx].x);
}

/**
 * @brief Kernel to accumulate power spectrum (for Welch averaging)
 */
__global__ void kernel_accumulate_power(
    const cufftComplex* __restrict__ segment_fft,
    float* __restrict__ accumulated_power,
    uint32_t n_freqs,
    float scale)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_freqs) return;

    float re = segment_fft[idx].x;
    float im = segment_fft[idx].y;
    atomicAdd(&accumulated_power[idx], scale * (re * re + im * im));
}

/**
 * @brief Kernel to normalize accumulated power (Welch final step)
 */
__global__ void kernel_normalize_power(
    float* power,
    uint32_t n_freqs,
    float scale)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_freqs) return;

    power[idx] *= scale;
}

/**
 * @brief Kernel to compute frequency array
 */
__global__ void kernel_compute_frequencies(
    float* frequencies,
    uint32_t n_freqs,
    float fs,
    uint32_t nfft)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_freqs) return;

    frequencies[idx] = (float)idx * fs / nfft;
}

//=============================================================================
// Autocorrelation Kernels (FFT-based via Wiener-Khinchin)
//=============================================================================

/**
 * @brief Kernel to compute cross-correlation via multiplication in frequency domain
 */
__global__ void kernel_multiply_conj(
    const cufftComplex* __restrict__ fft_x,
    const cufftComplex* __restrict__ fft_y,
    cufftComplex* __restrict__ product,
    uint32_t n)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    // product = fft_x * conj(fft_y)
    float ax = fft_x[idx].x;
    float ay = fft_x[idx].y;
    float bx = fft_y[idx].x;
    float by = fft_y[idx].y;

    product[idx].x = ax * bx + ay * by;  // Real: Re(a)*Re(b) + Im(a)*Im(b)
    product[idx].y = ay * bx - ax * by;  // Imag: Im(a)*Re(b) - Re(a)*Im(b)
}

/**
 * @brief Kernel to compute autocorrelation (multiply FFT by its conjugate)
 */
__global__ void kernel_multiply_self_conj(
    cufftComplex* fft_data,
    uint32_t n)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float re = fft_data[idx].x;
    float im = fft_data[idx].y;

    // |FFT|^2 = FFT * conj(FFT) = re^2 + im^2 + 0i
    fft_data[idx].x = re * re + im * im;
    fft_data[idx].y = 0.0f;
}

/**
 * @brief Kernel to extract and normalize ACF from iFFT result
 */
__global__ void kernel_extract_acf(
    const cufftComplex* __restrict__ ifft_result,
    float* __restrict__ acf,
    uint32_t max_lag,
    float var0)  // Variance at lag 0 for normalization
{
    uint32_t lag = blockIdx.x * blockDim.x + threadIdx.x;
    if (lag > max_lag) return;

    // Normalize by variance at lag 0
    acf[lag] = ifft_result[lag].x / (var0 > 1e-10f ? var0 : 1e-10f);
}

/**
 * @brief Kernel to compute ACF confidence bounds
 */
__global__ void kernel_acf_confidence(
    float* confidence_upper,
    float* confidence_lower,
    uint32_t max_lag,
    uint32_t n,
    float z_value)  // e.g., 1.96 for 95% CI
{
    uint32_t lag = blockIdx.x * blockDim.x + threadIdx.x;
    if (lag > max_lag) return;

    // Bartlett formula: SE = 1/sqrt(n) for white noise
    // For lag k > 0, adjust for sample size
    float se = 1.0f / sqrtf((float)n);

    confidence_upper[lag] = z_value * se;
    confidence_lower[lag] = -z_value * se;
}

//=============================================================================
// Cross-Spectral and Coherence Kernels
//=============================================================================

/**
 * @brief Kernel to compute cross-spectral density
 */
__global__ void kernel_cross_spectrum(
    const cufftComplex* __restrict__ fft_x,
    const cufftComplex* __restrict__ fft_y,
    float* __restrict__ csd_real,
    float* __restrict__ csd_imag,
    uint32_t n_freqs,
    float scale)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_freqs) return;

    // CSD = X * conj(Y)
    float ax = fft_x[idx].x;
    float ay = fft_x[idx].y;
    float bx = fft_y[idx].x;
    float by = fft_y[idx].y;

    csd_real[idx] = scale * (ax * bx + ay * by);
    csd_imag[idx] = scale * (ay * bx - ax * by);
}

/**
 * @brief Kernel to accumulate cross-spectral density (for Welch averaging)
 */
__global__ void kernel_accumulate_csd(
    const cufftComplex* __restrict__ fft_x,
    const cufftComplex* __restrict__ fft_y,
    float* __restrict__ csd_real_sum,
    float* __restrict__ csd_imag_sum,
    uint32_t n_freqs,
    float scale)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_freqs) return;

    float ax = fft_x[idx].x;
    float ay = fft_x[idx].y;
    float bx = fft_y[idx].x;
    float by = fft_y[idx].y;

    atomicAdd(&csd_real_sum[idx], scale * (ax * bx + ay * by));
    atomicAdd(&csd_imag_sum[idx], scale * (ay * bx - ax * by));
}

/**
 * @brief Kernel to compute coherence from accumulated spectra
 *
 * Coherence = |Sxy|^2 / (Sxx * Syy)
 */
__global__ void kernel_compute_coherence(
    const float* __restrict__ psd_x,        // Power spectral density of x
    const float* __restrict__ psd_y,        // Power spectral density of y
    const float* __restrict__ csd_real,     // Real part of cross-spectrum
    const float* __restrict__ csd_imag,     // Imaginary part of cross-spectrum
    float* __restrict__ coherence,          // Magnitude squared coherence
    float* __restrict__ phase,              // Phase spectrum
    uint32_t n_freqs)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_freqs) return;

    float sxx = psd_x[idx];
    float syy = psd_y[idx];
    float cxy_re = csd_real[idx];
    float cxy_im = csd_imag[idx];

    // Magnitude squared coherence
    float cxy_mag2 = cxy_re * cxy_re + cxy_im * cxy_im;
    float denom = sxx * syy;

    if (denom > 1e-20f) {
        coherence[idx] = cxy_mag2 / denom;
    } else {
        coherence[idx] = 0.0f;
    }

    // Phase
    phase[idx] = atan2f(cxy_im, cxy_re);
}

//=============================================================================
// Smoothing and Filtering Kernels
//=============================================================================

/**
 * @brief Kernel for simple moving average
 */
__global__ void kernel_moving_average(
    const float* __restrict__ input,
    float* __restrict__ output,
    uint32_t n,
    uint32_t window_size)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    int half_window = window_size / 2;
    int start = max(0, (int)idx - half_window);
    int end = min((int)n - 1, (int)idx + half_window);
    int count = end - start + 1;

    float sum = 0.0f;
    for (int i = start; i <= end; i++) {
        sum += input[i];
    }

    output[idx] = sum / count;
}

/**
 * @brief Kernel for exponential smoothing
 */
__global__ void kernel_exponential_smooth_single(
    const float* __restrict__ input,
    float* __restrict__ output,
    float alpha,
    uint32_t n)
{
    // This is a sequential operation - only one thread
    if (threadIdx.x != 0 || blockIdx.x != 0) return;

    output[0] = input[0];
    for (uint32_t i = 1; i < n; i++) {
        output[i] = alpha * input[i] + (1.0f - alpha) * output[i - 1];
    }
}

/**
 * @brief Kernel for parallel prefix exponential smoothing (work-efficient)
 *
 * Uses scan-based approach for parallelism
 */
__global__ void kernel_exponential_smooth_parallel(
    const float* __restrict__ input,
    float* __restrict__ output,
    float alpha,
    uint32_t n)
{
    extern __shared__ float sdata[];

    uint32_t tid = threadIdx.x;
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;

    // Load data
    sdata[tid] = (idx < n) ? input[idx] : 0.0f;
    __syncthreads();

    // Sequential within block for correctness
    // (Full parallel scan would require multi-pass algorithm)
    if (tid == 0) {
        for (uint32_t i = 1; i < blockDim.x && (blockIdx.x * blockDim.x + i) < n; i++) {
            sdata[i] = alpha * sdata[i] + (1.0f - alpha) * sdata[i - 1];
        }
    }
    __syncthreads();

    if (idx < n) {
        output[idx] = sdata[tid];
    }
}

/**
 * @brief Kernel for Savitzky-Golay filter (convolution with precomputed coefficients)
 */
__global__ void kernel_savitzky_golay(
    const float* __restrict__ input,
    const float* __restrict__ coefficients,
    float* __restrict__ output,
    uint32_t n,
    uint32_t window_size)
{
    extern __shared__ float shared_coef[];

    // Load coefficients to shared memory
    if (threadIdx.x < window_size) {
        shared_coef[threadIdx.x] = coefficients[threadIdx.x];
    }
    __syncthreads();

    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    int half_window = window_size / 2;

    // Boundary handling: mirror padding
    float sum = 0.0f;
    for (int j = 0; j < (int)window_size; j++) {
        int data_idx = (int)idx + j - half_window;

        // Mirror at boundaries
        if (data_idx < 0) {
            data_idx = -data_idx;
        } else if (data_idx >= (int)n) {
            data_idx = 2 * (int)n - data_idx - 2;
        }

        sum += input[data_idx] * shared_coef[j];
    }

    output[idx] = sum;
}

//=============================================================================
// Change Point Detection Kernels
//=============================================================================

/**
 * @brief Kernel to compute CUSUM statistic
 */
__global__ void kernel_cusum(
    const float* __restrict__ input,
    float* __restrict__ cusum_pos,
    float* __restrict__ cusum_neg,
    float mean,
    float k,  // Slack value
    uint32_t n)
{
    // Sequential operation
    if (threadIdx.x != 0 || blockIdx.x != 0) return;

    cusum_pos[0] = 0.0f;
    cusum_neg[0] = 0.0f;

    for (uint32_t i = 1; i < n; i++) {
        float diff = input[i] - mean;

        // Upper CUSUM
        cusum_pos[i] = fmaxf(0.0f, cusum_pos[i-1] + diff - k);

        // Lower CUSUM
        cusum_neg[i] = fminf(0.0f, cusum_neg[i-1] + diff + k);
    }
}

/**
 * @brief Kernel to detect change points from CUSUM
 */
__global__ void kernel_cusum_detect(
    const float* __restrict__ cusum_pos,
    const float* __restrict__ cusum_neg,
    uint32_t* __restrict__ change_points,
    uint32_t* __restrict__ n_changes,
    float threshold,
    uint32_t n,
    uint32_t max_changes)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    // Check if this is a change point
    if (cusum_pos[idx] > threshold || fabsf(cusum_neg[idx]) > threshold) {
        uint32_t slot = atomicAdd(n_changes, 1);
        if (slot < max_changes) {
            change_points[slot] = idx;
        }
    }
}

/**
 * @brief Kernel to compute segment statistics for binary segmentation
 */
__global__ void kernel_segment_stats(
    const float* __restrict__ data,
    float* __restrict__ segment_sums,
    float* __restrict__ segment_sq_sums,
    uint32_t* __restrict__ segment_counts,
    const uint32_t* __restrict__ segment_starts,
    const uint32_t* __restrict__ segment_ends,
    uint32_t n_segments)
{
    __shared__ float sdata_sum[BLOCK_SIZE / WARP_SIZE];
    __shared__ float sdata_sq[BLOCK_SIZE / WARP_SIZE];

    uint32_t seg_idx = blockIdx.x;
    if (seg_idx >= n_segments) return;

    uint32_t start = segment_starts[seg_idx];
    uint32_t end = segment_ends[seg_idx];
    uint32_t count = end - start;

    float local_sum = 0.0f;
    float local_sq_sum = 0.0f;

    for (uint32_t i = start + threadIdx.x; i < end; i += blockDim.x) {
        float val = data[i];
        local_sum += val;
        local_sq_sum += val * val;
    }

    float sum = block_reduce_sum(local_sum, sdata_sum);
    float sq_sum = block_reduce_sum(local_sq_sum, sdata_sq);

    if (threadIdx.x == 0) {
        segment_sums[seg_idx] = sum;
        segment_sq_sums[seg_idx] = sq_sum;
        segment_counts[seg_idx] = count;
    }
}

//=============================================================================
// Workspace Management
//=============================================================================

/**
 * @brief Initialize GPU workspace for time series operations
 */
extern "C" bool nimcp_ts_gpu_init(size_t max_size) {
    /* Initialize recovery system if needed */
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (g_ts_workspace && g_ts_workspace->initialized) {
        return true;  // Already initialized
    }

    g_ts_workspace = (ts_gpu_workspace_t*)nimcp_calloc(1, sizeof(ts_gpu_workspace_t));
    if (!g_ts_workspace) {
        set_ts_error("Failed to allocate workspace");
        return false;
    }

    // Create CUDA stream
    cudaError_t err = cudaStreamCreate(&g_ts_workspace->stream);
    if (err != cudaSuccess) {
        set_ts_error("Failed to create CUDA stream: %s", cudaGetErrorString(err));
        nimcp_free(g_ts_workspace);
        g_ts_workspace = NULL;
        return false;
    }

    // Allocate device buffers
    size_t complex_size = (max_size / 2 + 1) * sizeof(cufftComplex);

    err = cudaMalloc(&g_ts_workspace->d_input, max_size * sizeof(float));
    if (err != cudaSuccess) goto cleanup;

    err = cudaMalloc(&g_ts_workspace->d_complex, complex_size);
    if (err != cudaSuccess) goto cleanup;

    err = cudaMalloc(&g_ts_workspace->d_output, max_size * sizeof(float));
    if (err != cudaSuccess) goto cleanup;

    err = cudaMalloc(&g_ts_workspace->d_window, max_size * sizeof(float));
    if (err != cudaSuccess) goto cleanup;

    err = cudaMalloc(&g_ts_workspace->d_temp, max_size * sizeof(float));
    if (err != cudaSuccess) goto cleanup;

    g_ts_workspace->input_size = max_size;
    g_ts_workspace->complex_size = max_size / 2 + 1;
    g_ts_workspace->output_size = max_size;
    g_ts_workspace->fft_plan_size = 0;
    g_ts_workspace->initialized = true;

    LOG_DEBUG("Time series GPU workspace initialized (max_size=%zu)", max_size);
    return true;

cleanup:
    set_ts_error("Failed to allocate device memory: %s", cudaGetErrorString(err));
    if (g_ts_workspace->d_input) cudaFree(g_ts_workspace->d_input);
    if (g_ts_workspace->d_complex) cudaFree(g_ts_workspace->d_complex);
    if (g_ts_workspace->d_output) cudaFree(g_ts_workspace->d_output);
    if (g_ts_workspace->d_window) cudaFree(g_ts_workspace->d_window);
    if (g_ts_workspace->d_temp) cudaFree(g_ts_workspace->d_temp);
    cudaStreamDestroy(g_ts_workspace->stream);
    nimcp_free(g_ts_workspace);
    g_ts_workspace = NULL;
    return false;
}

/**
 * @brief Shutdown GPU workspace
 */
extern "C" void nimcp_ts_gpu_shutdown(void) {
    if (!g_ts_workspace) return;

    if (g_ts_workspace->fft_plan_size > 0) {
        cufftDestroy(g_ts_workspace->fft_plan);
    }

    if (g_ts_workspace->d_input) cudaFree(g_ts_workspace->d_input);
    if (g_ts_workspace->d_complex) cudaFree(g_ts_workspace->d_complex);
    if (g_ts_workspace->d_output) cudaFree(g_ts_workspace->d_output);
    if (g_ts_workspace->d_window) cudaFree(g_ts_workspace->d_window);
    if (g_ts_workspace->d_temp) cudaFree(g_ts_workspace->d_temp);

    cudaStreamDestroy(g_ts_workspace->stream);

    nimcp_free(g_ts_workspace);
    g_ts_workspace = NULL;

    LOG_DEBUG("Time series GPU workspace shutdown");
}

/**
 * @brief Check if GPU is available for time series
 */
extern "C" bool nimcp_ts_gpu_available(void) {
    int device_count = 0;
    cudaError_t err = cudaGetDeviceCount(&device_count);
    return (err == cudaSuccess && device_count > 0);
}

//=============================================================================
// GPU Window Generation Functions
//=============================================================================

/**
 * @brief Generate window function on GPU
 */
extern "C" bool nimcp_ts_gpu_generate_window(
    float* d_window,
    uint32_t n,
    nimcp_ts_window_t window_type,
    float param)
{
    dim3 block(BLOCK_SIZE);
    dim3 grid(GRID_SIZE(n));

    switch (window_type) {
        case NIMCP_TS_WINDOW_RECTANGULAR:
            // Fill with 1.0
            cudaMemset(d_window, 0, n * sizeof(float));
            // Actually need to set to 1.0f
            {
                float one = 1.0f;
                cudaMemcpy(d_window, &one, sizeof(float), cudaMemcpyHostToDevice);
                // Use cuBLAS or custom kernel to set all values
                // For now, use a simple kernel approach implicitly via Hamming = 1.0 case
                kernel_hann_window<<<grid, block>>>(d_window, n);
                // Override with 1.0
                cudaMemset(d_window, 0, n * sizeof(float));
                float* h_ones = (float*)nimcp_malloc(n * sizeof(float));
                for (uint32_t i = 0; i < n; i++) h_ones[i] = 1.0f;
                cudaMemcpy(d_window, h_ones, n * sizeof(float), cudaMemcpyHostToDevice);
                nimcp_free(h_ones);
            }
            break;

        case NIMCP_TS_WINDOW_HANN:
            kernel_hann_window<<<grid, block>>>(d_window, n);
            break;

        case NIMCP_TS_WINDOW_HAMMING:
            kernel_hamming_window<<<grid, block>>>(d_window, n);
            break;

        case NIMCP_TS_WINDOW_BLACKMAN:
            kernel_blackman_window<<<grid, block>>>(d_window, n);
            break;

        case NIMCP_TS_WINDOW_BLACKMAN_HARRIS:
            kernel_blackman_harris_window<<<grid, block>>>(d_window, n);
            break;

        case NIMCP_TS_WINDOW_KAISER:
            kernel_kaiser_window<<<grid, block>>>(d_window, n, param);
            break;

        case NIMCP_TS_WINDOW_GAUSSIAN:
            kernel_gaussian_window<<<grid, block>>>(d_window, n, param);
            break;

        case NIMCP_TS_WINDOW_TUKEY:
            kernel_tukey_window<<<grid, block>>>(d_window, n, param);
            break;

        default:
            LOG_WARN("Unknown window type %d, using Hann", window_type);
            kernel_hann_window<<<grid, block>>>(d_window, n);
            break;
    }

    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
        set_ts_error("Window generation failed: %s", cudaGetErrorString(err));
        return false;
    }

    return true;
}

//=============================================================================
// GPU Periodogram Implementation
//=============================================================================

/**
 * @brief Compute periodogram on GPU
 */
extern "C" bool nimcp_ts_gpu_periodogram(
    const float* h_input,
    float* h_power,
    float* h_frequencies,
    uint32_t n,
    float fs,
    uint32_t nfft,
    nimcp_ts_window_t window,
    nimcp_ts_detrend_t detrend)
{
    if (!g_ts_workspace || !g_ts_workspace->initialized) {
        if (!nimcp_ts_gpu_init(nfft * 2)) {
            return false;
        }
    }

    // Ensure workspace is large enough
    if (nfft > g_ts_workspace->input_size) {
        set_ts_error("FFT size %u exceeds workspace size %zu", nfft, g_ts_workspace->input_size);
        return false;
    }

    cudaStream_t stream = g_ts_workspace->stream;
    bool success = false;
    cufftHandle plan;
    bool plan_created = false;
    uint32_t n_freqs = 0;
    float scale = 0.0f;
    dim3 block(BLOCK_SIZE);
    dim3 grid(GRID_SIZE(n));

    // Copy input to device
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(g_ts_workspace->d_input, h_input,
                                      n * sizeof(float), cudaMemcpyHostToDevice, stream), GPU_ERROR_CUDA_RUNTIME);

    // Zero-pad if nfft > n
    if (nfft > n) {
        NIMCP_CUDA_RECOVER(cudaMemsetAsync(g_ts_workspace->d_input + n, 0,
                                          (nfft - n) * sizeof(float), stream), GPU_ERROR_CUDA_RUNTIME);
    }

    // Generate window
    if (!nimcp_ts_gpu_generate_window(g_ts_workspace->d_window, n, window, 3.0f)) {
        goto cleanup;
    }

    // Apply window (and zero-pad window too)
    kernel_apply_window<<<grid, block, 0, stream>>>(
        g_ts_workspace->d_input, g_ts_workspace->d_window,
        g_ts_workspace->d_temp, n);

    // Handle detrending
    if (detrend == NIMCP_TS_DETREND_MEAN) {
        // Compute mean and subtract
        float* d_partials;
        uint32_t n_blocks = (n + BLOCK_SIZE - 1) / BLOCK_SIZE;
        NIMCP_CUDA_RECOVER(cudaMalloc(&d_partials, n_blocks * sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);

        kernel_compute_mean_partial<<<n_blocks, BLOCK_SIZE, 0, stream>>>(
            g_ts_workspace->d_temp, d_partials, n);

        // Sum partials on host (small operation)
        float* h_partials = (float*)nimcp_malloc(n_blocks * sizeof(float));
        NIMCP_CUDA_RECOVER(cudaMemcpy(h_partials, d_partials, n_blocks * sizeof(float),
                                     cudaMemcpyDeviceToHost), GPU_ERROR_CUDA_RUNTIME);

        float mean = 0.0f;
        for (uint32_t i = 0; i < n_blocks; i++) {
            mean += h_partials[i];
        }
        mean /= n;
        nimcp_free(h_partials);
        cudaFree(d_partials);

        // Subtract mean
        kernel_detrend_mean<<<grid, block, 0, stream>>>(
            g_ts_workspace->d_temp, mean, n);
    }

    // Create R2C FFT plan
    CUFFT_CHECK_GOTO(cufftPlan1d(&plan, nfft, CUFFT_R2C, 1), cleanup);
    plan_created = true;
    CUFFT_CHECK_GOTO(cufftSetStream(plan, stream), cleanup);

    // Execute FFT
    CUFFT_CHECK_GOTO(cufftExecR2C(plan, g_ts_workspace->d_temp,
                                   g_ts_workspace->d_complex), cleanup);

    // Compute power spectrum
    n_freqs = nfft / 2 + 1;
    scale = 1.0f / (fs * n);  // Power scaling

    grid = dim3(GRID_SIZE(n_freqs));
    kernel_power_spectrum<<<grid, block, 0, stream>>>(
        g_ts_workspace->d_complex, g_ts_workspace->d_output,
        n_freqs, scale);

    // Compute frequencies
    kernel_compute_frequencies<<<grid, block, 0, stream>>>(
        g_ts_workspace->d_temp, n_freqs, fs, nfft);

    // Copy results back
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(h_power, g_ts_workspace->d_output,
                                      n_freqs * sizeof(float), cudaMemcpyDeviceToHost, stream), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(h_frequencies, g_ts_workspace->d_temp,
                                      n_freqs * sizeof(float), cudaMemcpyDeviceToHost, stream), GPU_ERROR_CUDA_RUNTIME);

    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(stream), GPU_ERROR_CUDA_RUNTIME);

    success = true;

cleanup:
    if (plan_created) {
        cufftDestroy(plan);
    }

    return success;
}

//=============================================================================
// GPU Welch PSD Implementation
//=============================================================================

/**
 * @brief Compute Welch PSD on GPU
 */
extern "C" bool nimcp_ts_gpu_welch_psd(
    const float* h_input,
    float* h_power,
    float* h_frequencies,
    uint32_t n,
    float fs,
    uint32_t segment_length,
    float overlap,
    uint32_t nfft,
    nimcp_ts_window_t window)
{
    if (!g_ts_workspace || !g_ts_workspace->initialized) {
        if (!nimcp_ts_gpu_init(nfft * 2)) {
            return false;
        }
    }

    // Compute number of segments
    uint32_t hop = (uint32_t)(segment_length * (1.0f - overlap));
    if (hop < 1) hop = 1;
    uint32_t n_segments = (n - segment_length) / hop + 1;

    if (n_segments < 1) {
        set_ts_error("Signal too short for Welch with given segment length");
        return false;
    }

    cudaStream_t stream = g_ts_workspace->stream;
    bool success = false;
    cufftHandle plan;
    bool plan_created = false;
    float window_energy = 0.0f;
    dim3 block(BLOCK_SIZE);

    float* d_segment = NULL;
    float* d_accumulated_power = NULL;
    cufftComplex* d_segment_fft = NULL;

    uint32_t n_freqs = nfft / 2 + 1;

    // Allocate segment buffers
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_segment, nfft * sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_accumulated_power, n_freqs * sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_segment_fft, n_freqs * sizeof(cufftComplex)), GPU_ERROR_OUT_OF_MEMORY);

    // Initialize accumulated power to zero
    NIMCP_CUDA_RECOVER(cudaMemset(d_accumulated_power, 0, n_freqs * sizeof(float)), GPU_ERROR_CUDA_RUNTIME);

    // Generate window once
    NIMCP_CUDA_RECOVER(cudaMalloc(&g_ts_workspace->d_window, segment_length * sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);
    if (!nimcp_ts_gpu_generate_window(g_ts_workspace->d_window, segment_length, window, 3.0f)) {
        goto cleanup;
    }

    // Compute window energy for normalization
    {
        float* h_window = (float*)nimcp_malloc(segment_length * sizeof(float));
        NIMCP_CUDA_RECOVER(cudaMemcpy(h_window, g_ts_workspace->d_window,
                                     segment_length * sizeof(float), cudaMemcpyDeviceToHost), GPU_ERROR_CUDA_RUNTIME);
        for (uint32_t i = 0; i < segment_length; i++) {
            window_energy += h_window[i] * h_window[i];
        }
        nimcp_free(h_window);
    }

    // Create FFT plan
    CUFFT_CHECK_GOTO(cufftPlan1d(&plan, nfft, CUFFT_R2C, 1), cleanup);
    plan_created = true;
    CUFFT_CHECK_GOTO(cufftSetStream(plan, stream), cleanup);

    // Process each segment
    for (uint32_t seg = 0; seg < n_segments; seg++) {
        uint32_t offset = seg * hop;

        // Copy segment to device
        NIMCP_CUDA_RECOVER(cudaMemcpyAsync(d_segment, h_input + offset,
                                          segment_length * sizeof(float),
                                          cudaMemcpyHostToDevice, stream), GPU_ERROR_CUDA_RUNTIME);

        // Zero-pad
        if (nfft > segment_length) {
            NIMCP_CUDA_RECOVER(cudaMemsetAsync(d_segment + segment_length, 0,
                                              (nfft - segment_length) * sizeof(float), stream), GPU_ERROR_CUDA_RUNTIME);
        }

        // Apply window
        dim3 grid(GRID_SIZE(segment_length));
        kernel_apply_window<<<grid, block, 0, stream>>>(
            d_segment, g_ts_workspace->d_window, d_segment, segment_length);

        // FFT
        CUFFT_CHECK_GOTO(cufftExecR2C(plan, d_segment, d_segment_fft), cleanup);

        // Accumulate power
        float scale = 1.0f;  // Will normalize later
        grid = dim3(GRID_SIZE(n_freqs));
        kernel_accumulate_power<<<grid, block, 0, stream>>>(
            d_segment_fft, d_accumulated_power, n_freqs, scale);
    }

    // Normalize by number of segments and window energy
    {
        float norm_scale = 1.0f / (n_segments * fs * window_energy);
        dim3 grid(GRID_SIZE(n_freqs));
        kernel_normalize_power<<<grid, block, 0, stream>>>(
            d_accumulated_power, n_freqs, norm_scale);
    }

    // Compute frequencies
    {
        dim3 grid(GRID_SIZE(n_freqs));
        kernel_compute_frequencies<<<grid, block, 0, stream>>>(
            g_ts_workspace->d_temp, n_freqs, fs, nfft);
    }

    // Copy results
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(h_power, d_accumulated_power,
                                      n_freqs * sizeof(float), cudaMemcpyDeviceToHost, stream), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(h_frequencies, g_ts_workspace->d_temp,
                                      n_freqs * sizeof(float), cudaMemcpyDeviceToHost, stream), GPU_ERROR_CUDA_RUNTIME);

    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(stream), GPU_ERROR_CUDA_RUNTIME);

    success = true;

cleanup:
    if (plan_created) cufftDestroy(plan);
    if (d_segment) cudaFree(d_segment);
    if (d_accumulated_power) cudaFree(d_accumulated_power);
    if (d_segment_fft) cudaFree(d_segment_fft);

    return success;
}

//=============================================================================
// GPU Autocorrelation Implementation (FFT-based)
//=============================================================================

/**
 * @brief Compute autocorrelation function on GPU using Wiener-Khinchin theorem
 */
extern "C" bool nimcp_ts_gpu_acf(
    const float* h_input,
    float* h_acf,
    uint32_t n,
    uint32_t max_lag)
{
    if (!g_ts_workspace || !g_ts_workspace->initialized) {
        // FFT size should be at least 2*n for circular convolution avoidance
        size_t fft_size = 1;
        while (fft_size < 2 * n) fft_size *= 2;
        if (!nimcp_ts_gpu_init(fft_size)) {
            return false;
        }
    }

    // Compute FFT size (next power of 2 >= 2n for proper convolution)
    uint32_t nfft = 1;
    while (nfft < 2 * n) nfft *= 2;

    if (nfft > g_ts_workspace->input_size) {
        set_ts_error("FFT size %u exceeds workspace size %zu", nfft, g_ts_workspace->input_size);
        return false;
    }

    cudaStream_t stream = g_ts_workspace->stream;
    bool success = false;
    cufftHandle plan_forward, plan_inverse;
    bool forward_plan_created = false, inverse_plan_created = false;
    uint32_t n_complex = 0;
    float var0 = 0.0f;
    dim3 block(BLOCK_SIZE);
    dim3 grid(GRID_SIZE(n));

    cufftComplex* d_fft = NULL;

    // Allocate FFT buffer
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_fft, nfft * sizeof(cufftComplex)), GPU_ERROR_OUT_OF_MEMORY);

    // Copy input and zero-pad
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(g_ts_workspace->d_input, h_input,
                                      n * sizeof(float), cudaMemcpyHostToDevice, stream), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaMemsetAsync(g_ts_workspace->d_input + n, 0,
                                      (nfft - n) * sizeof(float), stream), GPU_ERROR_CUDA_RUNTIME);

    // Compute mean and subtract (for unbiased ACF)
    float h_mean = 0.0f;
    for (uint32_t i = 0; i < n; i++) h_mean += h_input[i];
    h_mean /= n;
    kernel_detrend_mean<<<grid, block, 0, stream>>>(g_ts_workspace->d_input, h_mean, n);

    // Create forward C2C plan
    CUFFT_CHECK_GOTO(cufftPlan1d(&plan_forward, nfft, CUFFT_R2C, 1), cleanup);
    forward_plan_created = true;
    CUFFT_CHECK_GOTO(cufftSetStream(plan_forward, stream), cleanup);

    // Forward FFT
    CUFFT_CHECK_GOTO(cufftExecR2C(plan_forward, g_ts_workspace->d_input, d_fft), cleanup);

    // Compute |FFT|^2 (power spectrum)
    n_complex = nfft / 2 + 1;
    grid = dim3(GRID_SIZE(n_complex));
    kernel_multiply_self_conj<<<grid, block, 0, stream>>>(d_fft, n_complex);

    // Create inverse C2R plan
    CUFFT_CHECK_GOTO(cufftPlan1d(&plan_inverse, nfft, CUFFT_C2R, 1), cleanup);
    inverse_plan_created = true;
    CUFFT_CHECK_GOTO(cufftSetStream(plan_inverse, stream), cleanup);

    // Inverse FFT gives autocorrelation
    CUFFT_CHECK_GOTO(cufftExecC2R(plan_inverse, d_fft, g_ts_workspace->d_output), cleanup);

    // Copy result (only up to max_lag)
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(h_acf, g_ts_workspace->d_output,
                                      (max_lag + 1) * sizeof(float), cudaMemcpyDeviceToHost, stream), GPU_ERROR_CUDA_RUNTIME);

    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(stream), GPU_ERROR_CUDA_RUNTIME);

    // Normalize: divide by variance at lag 0 and FFT normalization
    var0 = h_acf[0] / nfft;
    if (var0 > 1e-10f) {
        for (uint32_t i = 0; i <= max_lag; i++) {
            h_acf[i] = h_acf[i] / (nfft * var0);
        }
    }

    success = true;

cleanup:
    if (forward_plan_created) cufftDestroy(plan_forward);
    if (inverse_plan_created) cufftDestroy(plan_inverse);
    if (d_fft) cudaFree(d_fft);

    return success;
}

//=============================================================================
// GPU Coherence Implementation
//=============================================================================

/**
 * @brief Compute coherence between two signals on GPU
 */
extern "C" bool nimcp_ts_gpu_coherence(
    const float* h_x,
    const float* h_y,
    float* h_coherence,
    float* h_phase,
    float* h_frequencies,
    uint32_t n,
    float fs,
    uint32_t segment_length,
    float overlap,
    uint32_t nfft,
    nimcp_ts_window_t window)
{
    if (!g_ts_workspace || !g_ts_workspace->initialized) {
        if (!nimcp_ts_gpu_init(nfft * 2)) {
            return false;
        }
    }

    // Compute number of segments
    uint32_t hop = (uint32_t)(segment_length * (1.0f - overlap));
    if (hop < 1) hop = 1;
    uint32_t n_segments = (n - segment_length) / hop + 1;

    if (n_segments < 2) {
        set_ts_error("Need at least 2 segments for coherence estimation");
        return false;
    }

    cudaStream_t stream = g_ts_workspace->stream;
    bool success = false;
    cufftHandle plan;
    bool plan_created = false;
    dim3 block(BLOCK_SIZE);

    float* d_segment_x = NULL;
    float* d_segment_y = NULL;
    cufftComplex* d_fft_x = NULL;
    cufftComplex* d_fft_y = NULL;
    float* d_psd_x = NULL;
    float* d_psd_y = NULL;
    float* d_csd_real = NULL;
    float* d_csd_imag = NULL;
    float* d_window = NULL;

    uint32_t n_freqs = nfft / 2 + 1;

    // Allocate buffers
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_segment_x, nfft * sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_segment_y, nfft * sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_fft_x, n_freqs * sizeof(cufftComplex)), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_fft_y, n_freqs * sizeof(cufftComplex)), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_psd_x, n_freqs * sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_psd_y, n_freqs * sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_csd_real, n_freqs * sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_csd_imag, n_freqs * sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_window, segment_length * sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);

    // Initialize accumulators to zero
    NIMCP_CUDA_RECOVER(cudaMemset(d_psd_x, 0, n_freqs * sizeof(float)), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaMemset(d_psd_y, 0, n_freqs * sizeof(float)), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaMemset(d_csd_real, 0, n_freqs * sizeof(float)), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaMemset(d_csd_imag, 0, n_freqs * sizeof(float)), GPU_ERROR_CUDA_RUNTIME);

    // Generate window
    if (!nimcp_ts_gpu_generate_window(d_window, segment_length, window, 3.0f)) {
        goto cleanup;
    }

    // Create FFT plan
    CUFFT_CHECK_GOTO(cufftPlan1d(&plan, nfft, CUFFT_R2C, 1), cleanup);
    plan_created = true;
    CUFFT_CHECK_GOTO(cufftSetStream(plan, stream), cleanup);

    // Process each segment
    for (uint32_t seg = 0; seg < n_segments; seg++) {
        uint32_t offset = seg * hop;

        // Copy segments
        NIMCP_CUDA_RECOVER(cudaMemcpyAsync(d_segment_x, h_x + offset,
                                          segment_length * sizeof(float),
                                          cudaMemcpyHostToDevice, stream), GPU_ERROR_CUDA_RUNTIME);
        NIMCP_CUDA_RECOVER(cudaMemcpyAsync(d_segment_y, h_y + offset,
                                          segment_length * sizeof(float),
                                          cudaMemcpyHostToDevice, stream), GPU_ERROR_CUDA_RUNTIME);

        // Zero-pad
        if (nfft > segment_length) {
            NIMCP_CUDA_RECOVER(cudaMemsetAsync(d_segment_x + segment_length, 0,
                                              (nfft - segment_length) * sizeof(float), stream), GPU_ERROR_CUDA_RUNTIME);
            NIMCP_CUDA_RECOVER(cudaMemsetAsync(d_segment_y + segment_length, 0,
                                              (nfft - segment_length) * sizeof(float), stream), GPU_ERROR_CUDA_RUNTIME);
        }

        // Apply window
        dim3 grid(GRID_SIZE(segment_length));
        kernel_apply_window<<<grid, block, 0, stream>>>(d_segment_x, d_window, d_segment_x, segment_length);
        kernel_apply_window<<<grid, block, 0, stream>>>(d_segment_y, d_window, d_segment_y, segment_length);

        // FFT both signals
        CUFFT_CHECK_GOTO(cufftExecR2C(plan, d_segment_x, d_fft_x), cleanup);
        CUFFT_CHECK_GOTO(cufftExecR2C(plan, d_segment_y, d_fft_y), cleanup);

        // Accumulate PSDs and CSD
        float scale = 1.0f;
        grid = dim3(GRID_SIZE(n_freqs));
        kernel_accumulate_power<<<grid, block, 0, stream>>>(d_fft_x, d_psd_x, n_freqs, scale);
        kernel_accumulate_power<<<grid, block, 0, stream>>>(d_fft_y, d_psd_y, n_freqs, scale);
        kernel_accumulate_csd<<<grid, block, 0, stream>>>(d_fft_x, d_fft_y, d_csd_real, d_csd_imag, n_freqs, scale);
    }

    // Compute coherence and phase
    {
        dim3 grid(GRID_SIZE(n_freqs));
        kernel_compute_coherence<<<grid, block, 0, stream>>>(
            d_psd_x, d_psd_y, d_csd_real, d_csd_imag,
            g_ts_workspace->d_output, g_ts_workspace->d_temp, n_freqs);
    }

    // Compute frequencies
    {
        dim3 grid(GRID_SIZE(n_freqs));
        kernel_compute_frequencies<<<grid, block, 0, stream>>>(
            d_csd_real, n_freqs, fs, nfft);  // Reuse buffer for frequencies
    }

    // Copy results
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(h_coherence, g_ts_workspace->d_output,
                                      n_freqs * sizeof(float), cudaMemcpyDeviceToHost, stream), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(h_phase, g_ts_workspace->d_temp,
                                      n_freqs * sizeof(float), cudaMemcpyDeviceToHost, stream), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(h_frequencies, d_csd_real,
                                      n_freqs * sizeof(float), cudaMemcpyDeviceToHost, stream), GPU_ERROR_CUDA_RUNTIME);

    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(stream), GPU_ERROR_CUDA_RUNTIME);

    success = true;

cleanup:
    if (plan_created) cufftDestroy(plan);
    if (d_segment_x) cudaFree(d_segment_x);
    if (d_segment_y) cudaFree(d_segment_y);
    if (d_fft_x) cudaFree(d_fft_x);
    if (d_fft_y) cudaFree(d_fft_y);
    if (d_psd_x) cudaFree(d_psd_x);
    if (d_psd_y) cudaFree(d_psd_y);
    if (d_csd_real) cudaFree(d_csd_real);
    if (d_csd_imag) cudaFree(d_csd_imag);
    if (d_window) cudaFree(d_window);

    return success;
}

//=============================================================================
// GPU Smoothing Functions
//=============================================================================

/**
 * @brief Apply moving average filter on GPU
 */
extern "C" bool nimcp_ts_gpu_moving_average(
    const float* h_input,
    float* h_output,
    uint32_t n,
    uint32_t window_size)
{
    if (!g_ts_workspace || !g_ts_workspace->initialized) {
        if (!nimcp_ts_gpu_init(n)) {
            return false;
        }
    }

    cudaStream_t stream = g_ts_workspace->stream;

    // Copy input
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(g_ts_workspace->d_input, h_input,
                                      n * sizeof(float), cudaMemcpyHostToDevice, stream), GPU_ERROR_CUDA_RUNTIME);

    // Apply moving average
    dim3 block(BLOCK_SIZE);
    dim3 grid(GRID_SIZE(n));
    kernel_moving_average<<<grid, block, 0, stream>>>(
        g_ts_workspace->d_input, g_ts_workspace->d_output, n, window_size);

    // Copy result
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(h_output, g_ts_workspace->d_output,
                                      n * sizeof(float), cudaMemcpyDeviceToHost, stream), GPU_ERROR_CUDA_RUNTIME);

    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(stream), GPU_ERROR_CUDA_RUNTIME);

    return true;
}

/**
 * @brief Apply exponential smoothing on GPU
 */
extern "C" bool nimcp_ts_gpu_exponential_smooth(
    const float* h_input,
    float* h_output,
    uint32_t n,
    float alpha)
{
    if (!g_ts_workspace || !g_ts_workspace->initialized) {
        if (!nimcp_ts_gpu_init(n)) {
            return false;
        }
    }

    cudaStream_t stream = g_ts_workspace->stream;

    // Copy input
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(g_ts_workspace->d_input, h_input,
                                      n * sizeof(float), cudaMemcpyHostToDevice, stream), GPU_ERROR_CUDA_RUNTIME);

    // Apply exponential smoothing (single thread - sequential algorithm)
    kernel_exponential_smooth_single<<<1, 1, 0, stream>>>(
        g_ts_workspace->d_input, g_ts_workspace->d_output, alpha, n);

    // Copy result
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(h_output, g_ts_workspace->d_output,
                                      n * sizeof(float), cudaMemcpyDeviceToHost, stream), GPU_ERROR_CUDA_RUNTIME);

    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(stream), GPU_ERROR_CUDA_RUNTIME);

    return true;
}

//=============================================================================
// GPU CUSUM Change Point Detection
//=============================================================================

/**
 * @brief Compute CUSUM for change point detection on GPU
 */
extern "C" bool nimcp_ts_gpu_cusum(
    const float* h_input,
    uint32_t* h_change_points,
    uint32_t* n_changes,
    uint32_t n,
    float mean,
    float k,
    float threshold,
    uint32_t max_changes)
{
    if (!g_ts_workspace || !g_ts_workspace->initialized) {
        if (!nimcp_ts_gpu_init(n * 2)) {
            return false;
        }
    }

    cudaStream_t stream = g_ts_workspace->stream;

    float* d_cusum_pos = NULL;
    float* d_cusum_neg = NULL;
    uint32_t* d_change_points = NULL;
    uint32_t* d_n_changes = NULL;

    bool success = false;

    NIMCP_CUDA_RECOVER(cudaMalloc(&d_cusum_pos, n * sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_cusum_neg, n * sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_change_points, max_changes * sizeof(uint32_t)), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_n_changes, sizeof(uint32_t)), GPU_ERROR_OUT_OF_MEMORY);

    // Initialize count
    NIMCP_CUDA_RECOVER(cudaMemset(d_n_changes, 0, sizeof(uint32_t)), GPU_ERROR_CUDA_RUNTIME);

    // Copy input
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(g_ts_workspace->d_input, h_input,
                                      n * sizeof(float), cudaMemcpyHostToDevice, stream), GPU_ERROR_CUDA_RUNTIME);

    // Compute CUSUM (sequential)
    kernel_cusum<<<1, 1, 0, stream>>>(
        g_ts_workspace->d_input, d_cusum_pos, d_cusum_neg, mean, k, n);

    // Detect change points
    dim3 block(BLOCK_SIZE);
    dim3 grid(GRID_SIZE(n));
    kernel_cusum_detect<<<grid, block, 0, stream>>>(
        d_cusum_pos, d_cusum_neg, d_change_points, d_n_changes,
        threshold, n, max_changes);

    // Copy results
    NIMCP_CUDA_RECOVER(cudaMemcpy(n_changes, d_n_changes, sizeof(uint32_t), cudaMemcpyDeviceToHost), GPU_ERROR_CUDA_RUNTIME);

    if (*n_changes > 0) {
        uint32_t copy_count = (*n_changes > max_changes) ? max_changes : *n_changes;
        NIMCP_CUDA_RECOVER(cudaMemcpy(h_change_points, d_change_points,
                                     copy_count * sizeof(uint32_t), cudaMemcpyDeviceToHost), GPU_ERROR_CUDA_RUNTIME);
    }

    success = true;

    cudaFree(d_cusum_pos);
    cudaFree(d_cusum_neg);
    cudaFree(d_change_points);
    cudaFree(d_n_changes);

    return success;
}

//=============================================================================
// Get Last Error
//=============================================================================

/**
 * @brief Get last error message
 */
extern "C" const char* nimcp_ts_gpu_get_error(void) {
    return g_ts_gpu_error;
}

#else // !NIMCP_ENABLE_CUDA

//=============================================================================
// CPU Stubs
//=============================================================================

#include "utils/statistics/nimcp_timeseries.h"
#include "utils/logging/nimcp_logging.h"

#define LOG_MODULE "TIMESERIES_GPU"

extern "C" bool nimcp_ts_gpu_init(size_t max_size) {
    LOG_WARN("CUDA not available - time series GPU operations disabled");
    return false;
}

extern "C" void nimcp_ts_gpu_shutdown(void) {}

extern "C" bool nimcp_ts_gpu_available(void) {
    return false;
}

extern "C" bool nimcp_ts_gpu_periodogram(
    const float* h_input,
    float* h_power,
    float* h_frequencies,
    uint32_t n,
    float fs,
    uint32_t nfft,
    nimcp_ts_window_t window,
    nimcp_ts_detrend_t detrend)
{
    return false;
}

extern "C" bool nimcp_ts_gpu_welch_psd(
    const float* h_input,
    float* h_power,
    float* h_frequencies,
    uint32_t n,
    float fs,
    uint32_t segment_length,
    float overlap,
    uint32_t nfft,
    nimcp_ts_window_t window)
{
    return false;
}

extern "C" bool nimcp_ts_gpu_acf(
    const float* h_input,
    float* h_acf,
    uint32_t n,
    uint32_t max_lag)
{
    return false;
}

extern "C" bool nimcp_ts_gpu_coherence(
    const float* h_x,
    const float* h_y,
    float* h_coherence,
    float* h_phase,
    float* h_frequencies,
    uint32_t n,
    float fs,
    uint32_t segment_length,
    float overlap,
    uint32_t nfft,
    nimcp_ts_window_t window)
{
    return false;
}

extern "C" bool nimcp_ts_gpu_moving_average(
    const float* h_input,
    float* h_output,
    uint32_t n,
    uint32_t window_size)
{
    return false;
}

extern "C" bool nimcp_ts_gpu_exponential_smooth(
    const float* h_input,
    float* h_output,
    uint32_t n,
    float alpha)
{
    return false;
}

extern "C" bool nimcp_ts_gpu_cusum(
    const float* h_input,
    uint32_t* h_change_points,
    uint32_t* n_changes,
    uint32_t n,
    float mean,
    float k,
    float threshold,
    uint32_t max_changes)
{
    return false;
}

extern "C" const char* nimcp_ts_gpu_get_error(void) {
    return "CUDA not available";
}

#endif // NIMCP_ENABLE_CUDA
